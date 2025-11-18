/*!
 * \file WtExecuter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 本地执行器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtLocalExecuter类的所有方法，提供了执行单元管理和交易执行功能。
 * 主要实现包括：
 * 1. 执行单元管理：根据合约代码创建和管理执行单元，支持按品种配置不同的执行策略
 * 2. 仓位管理：处理目标仓位设置，支持倍数缩放、合约组合匹配、自动清零等功能
 * 3. 交易接口：实现ExecuteContext接口，提供买入、卖出、撤单等交易操作
 * 4. 数据查询：从数据管理器和交易适配器查询仓位、订单、行情等数据
 * 5. 交易回报：处理交易回报（成交、订单、持仓、资金等），转发给执行单元
 * 6. 自动清理：支持自动清理上一期主力合约的头寸
 * 7. 线程池：支持使用线程池并发处理执行单元回调，提高性能
 * 
 * 实现细节：
 * - 使用自旋锁保护执行单元映射表的并发访问
 * - 支持线程池异步处理，避免阻塞主线程
 * - 支持合约组合配置，实现组合仓位的自动匹配和拆分
 * - 支持严格同步模式，确保通道持仓与目标仓位一致
 * - 使用高精度小数运算处理仓位计算
 */
#include "WtLocalExecuter.h"  // 包含本地执行器头文件
#include "TraderAdapter.h"  // 包含交易适配器头文件
#include "WtEngine.h"  // 包含引擎头文件

#include "../Share/CodeHelper.hpp"  // 包含合约代码解析辅助工具
#include "../Includes/IDataManager.h"  // 包含数据管理器接口头文件
#include "../Includes/WTSVariant.hpp"  // 包含变体类型定义
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件
#include "../Share/decimal.h"  // 包含高精度小数运算工具

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

USING_NS_WTP;  // 使用WonderTrader命名空间


/**
 * @brief 构造函数
 * @param factory 执行器工厂指针
 * @param name 执行器名称
 * @param dataMgr 数据管理器指针
 * 
 * 初始化本地执行器，设置工厂、名称和数据管理器，初始化配置参数为默认值。
 */
WtLocalExecuter::WtLocalExecuter(WtExecuterFactory* factory, const char* name, IDataManager* dataMgr)
	: IExecCommand(name)  // 调用基类构造函数，设置执行命令名称
	, _factory(factory)  // 初始化执行器工厂指针
	, _data_mgr(dataMgr)  // 初始化数据管理器指针
	, _channel_ready(false)  // 初始化通道就绪标志为false
	, _scale(1.0)  // 初始化仓位倍数为1.0
	, _auto_clear(true)  // 初始化自动清理标志为true
	, _trader(NULL)  // 初始化交易适配器指针为NULL
{
}


/**
 * @brief 析构函数
 * 
 * 清理资源，如果线程池已创建，等待所有任务完成。
 */
WtLocalExecuter::~WtLocalExecuter()
{
	if (_pool)  // 如果线程池指针有效
		_pool->wait();  // 等待线程池中所有任务完成
}

/**
 * @brief 设置交易适配器
 * @param adapter 交易适配器指针
 * 
 * 设置交易适配器，并读取其就绪状态，更新通道就绪标志。
 */
void WtLocalExecuter::setTrader(TraderAdapter* adapter)
{
	_trader = adapter;  // 保存交易适配器指针
	//设置的时候读取一下trader的状态
	if(_trader)  // 如果交易适配器指针有效
		_channel_ready = _trader->isReady();  // 读取适配器的就绪状态并更新通道就绪标志
}

/**
 * @brief 初始化执行器
 * @param params 初始化参数配置对象
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 从配置对象中读取执行器参数并初始化，包括：
 * - scale: 仓位倍数
 * - strict_sync: 是否严格同步
 * - poolsize: 线程池大小
 * - clear: 自动清理配置（active、includes、excludes）
 * - groups: 合约组合配置
 * 
 * 参数 params 的一个完整JSON格式的例子：
 * {
 *   "scale": 1.0,
 *   "strict_sync": false,
 *   "poolsize": 10,
 *   "clear": {
 *     "active": true,
 *     "includes": ["CFFEX.IF"],
 *     "excludes": ["CFFEX.IF"]
 *   },
 *   "groups": {
 *     "group1": {
 *       "CFFEX.IF": 1.0,
 *       "CFFEX.IC": 1.0
 *     }
 *   }
 * }
 */
bool WtLocalExecuter::init(WTSVariant* params)
{
	if (params == NULL)  // 如果配置对象为空
		return false;  // 返回失败

	_config = params;  // 保存配置对象指针
	_config->retain();  // 增加配置对象引用计数

	_scale = params->getDouble("scale");  // 读取仓位倍数配置，默认为1.0
	_strict_sync  = params->getBoolean("strict_sync");  // 读取严格同步配置，默认为false

	uint32_t poolsize = params->getUInt32("poolsize");  // 读取线程池大小配置
	if(poolsize > 0)  // 如果线程池大小大于0
	{
		_pool.reset(new boost::threadpool::pool(poolsize));  // 创建指定大小的线程池
	}

	/*
	 *	By Wesley @ 2021.12.14
	 *	从配置文件中读取自动清理的策略
	 *	active: 是否启用
	 *	includes: 包含列表，格式如CFFEX.IF
	 *	excludes: 排除列表，格式如CFFEX.IF
	 */
	WTSVariant* cfgClear = params->get("clear");  // 获取自动清理配置节点
	if(cfgClear)  // 如果配置节点存在
	{
		_auto_clear = cfgClear->getBoolean("active");  // 读取是否启用自动清理
		WTSVariant* cfgItem = cfgClear->get("includes");  // 获取包含列表配置节点
		if(cfgItem)  // 如果配置节点存在
		{
			if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个品种）
				_clear_includes.insert(cfgItem->asCString());  // 将字符串添加到包含列表
			else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个品种）
			{
				for(uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组元素
					_clear_includes.insert(cfgItem->get(i)->asCString());  // 将每个元素添加到包含列表
			}
		}

		cfgItem = cfgClear->get("excludes");  // 获取排除列表配置节点
		if (cfgItem)  // 如果配置节点存在
		{
			if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个品种）
				_clear_excludes.insert(cfgItem->asCString());  // 将字符串添加到排除列表
			else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个品种）
			{
				for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组元素
					_clear_excludes.insert(cfgItem->get(i)->asCString());  // 将每个元素添加到排除列表
			}
		}
	}

	WTSVariant* cfgGroups = params->get("groups");  // 获取合约组合配置节点
	if (cfgGroups)  // 如果配置节点存在
	{
		auto names = cfgGroups->memberNames();  // 获取所有组合名称
		for(const std::string& gpname : names)  // 遍历每个组合名称
		{
			CodeGroupPtr& gpInfo = _groups[gpname];  // 在组合映射表中获取或创建组合信息引用
			if (gpInfo == NULL)  // 如果组合信息不存在
			{
				gpInfo.reset(new CodeGroup);  // 创建新的组合信息对象
				wt_strcpy(gpInfo->_name, gpname.c_str(), gpname.size());  // 复制组合名称
			}

			WTSVariant* cfgGrp = cfgGroups->get(gpname.c_str());  // 获取该组合的配置节点
			auto codes = cfgGrp->memberNames();  // 获取该组合中的所有合约代码
			for(const std::string& code : codes)  // 遍历每个合约代码
			{
				gpInfo->_items[code] = cfgGrp->getDouble(code.c_str());  // 将合约代码和权重存入组合项映射表
				_code_to_groups[code] = gpInfo;  // 建立合约代码到组合的反向映射
			}
		}
	}

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Local executer inited, scale: {}, auto_clear: {}, strict_sync: {}, thread poolsize: {}, code_groups: {}",  // 记录初始化日志
		_scale, _auto_clear, _strict_sync, poolsize, _groups.size());  // 日志参数：倍数、自动清理、严格同步、线程池大小、组合数量

	return true;  // 返回成功
}

/**
 * @brief 获取执行单元
 * @param stdCode 标准合约代码
 * @param bAutoCreate 是否自动创建，默认为true
 * @return ExecuteUnitPtr 返回执行单元智能指针，不存在且不自动创建则返回空指针
 * 
 * 根据合约代码获取或创建执行单元。流程：
 * 1. 解析合约代码，提取品种代码
 * 2. 从配置中查找该品种的执行策略（如果没有则使用默认策略）
 * 3. 如果执行单元已存在，直接返回
 * 4. 如果不存在且bAutoCreate为true，则创建新的执行单元并初始化
 * 5. 如果通道已就绪，通知执行单元通道就绪
 */
ExecuteUnitPtr WtLocalExecuter::getUnit(const char* stdCode, bool bAutoCreate /* = true */)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 解析标准合约代码，提取品种信息
	std::string commID = codeInfo.stdCommID();  // 获取标准品种代码（如"CFFEX.IF"）

	WTSVariant* policy = _config->get("policy");  // 获取策略配置节点
	std::string des = commID;  // 默认使用品种代码作为策略键
	if (!policy->has(commID.c_str()))  // 如果配置中没有该品种的策略
		des = "default";  // 使用默认策略

	SpinLock lock(_mtx_units);  // 获取自旋锁，保护执行单元映射表的并发访问

	auto it = _unit_map.find(stdCode);  // 在执行单元映射表中查找合约代码
	if(it != _unit_map.end())  // 如果找到执行单元
	{
		return it->second;  // 返回执行单元智能指针
	}

	if (bAutoCreate)  // 如果允许自动创建
	{
		WTSVariant* cfg = policy->get(des.c_str());  // 获取该策略的配置节点

		const char* name = cfg->getCString("name");  // 获取执行单元名称（格式："工厂名.单元名"）
		ExecuteUnitPtr unit = _factory->createExeUnit(name);  // 使用工厂创建执行单元
		if (unit != NULL)  // 如果创建成功
		{
			_unit_map[stdCode] = unit;  // 将执行单元存入映射表
			unit->self()->init(this, stdCode, cfg);  // 初始化执行单元，传入执行上下文、合约代码和配置

			//如果通道已经就绪，则直接通知执行单元
			if (_channel_ready)  // 如果交易通道已就绪
				unit->self()->on_channel_ready();  // 通知执行单元通道就绪
		}
		return unit;  // 返回执行单元智能指针
	}
	else  // 如果不允许自动创建
	{
		return ExecuteUnitPtr();  // 返回空指针
	}
}


//////////////////////////////////////////////////////////////////////////
//ExecuteContext接口实现
#pragma region Context回调接口
/**
 * @brief 获取Tick数据切片
 * @param stdCode 标准合约代码
 * @param count 数据条数
 * @param etime 截止时间，0表示当前时间
 * @return WTSTickSlice* 返回Tick数据切片指针
 * 
 * 从数据管理器获取指定合约的Tick历史数据切片。
 */
WTSTickSlice* WtLocalExecuter::getTicks(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_data_mgr == NULL)  // 如果数据管理器指针无效
		return NULL;  // 返回空指针

	return _data_mgr->get_tick_slice(stdCode, count);  // 调用数据管理器获取Tick数据切片
}

/**
 * @brief 获取最近一笔Tick数据
 * @param stdCode 标准合约代码
 * @return WTSTickData* 返回最近一笔Tick数据指针
 * 
 * 从数据管理器获取指定合约的最近一笔Tick数据。
 */
WTSTickData* WtLocalExecuter::grabLastTick(const char* stdCode)
{
	if (_data_mgr == NULL)  // 如果数据管理器指针无效
		return NULL;  // 返回空指针

	return _data_mgr->grab_last_tick(stdCode);  // 调用数据管理器获取最近一笔Tick数据
}

/**
 * @brief 获取仓位信息
 * @param stdCode 标准合约代码
 * @param validOnly 是否只读取可用持仓，默认为true
 * @param flag 操作标记：1-多仓，2-空仓，3-多空轧平，默认为3
 * @return double 返回轧平后的仓位：多仓>0，空仓<0
 * 
 * 从交易适配器获取指定合约的仓位信息。
 */
double WtLocalExecuter::getPosition(const char* stdCode, bool validOnly /* = true */, int32_t flag /* = 3 */)
{
	if (NULL == _trader)  // 如果交易适配器指针无效
		return 0.0;  // 返回0

	return _trader->getPosition(stdCode, validOnly, flag);  // 调用交易适配器获取仓位信息
}

/**
 * @brief 获取未完成数量
 * @param stdCode 标准合约代码
 * @return double 返回未完成数量
 * 
 * 从交易适配器获取指定合约的未完成订单数量。
 */
double WtLocalExecuter::getUndoneQty(const char* stdCode)
{
	if (NULL == _trader)  // 如果交易适配器指针无效
		return 0.0;  // 返回0

	return _trader->getUndoneQty(stdCode);  // 调用交易适配器获取未完成数量
}

/**
 * @brief 获取订单映射表
 * @param stdCode 标准合约代码
 * @return OrderMap* 返回订单映射表指针
 * 
 * 从交易适配器获取指定合约的订单映射表。
 */
OrderMap* WtLocalExecuter::getOrders(const char* stdCode)
{
	if (NULL == _trader)  // 如果交易适配器指针无效
		return NULL;  // 返回空指针

	return _trader->getOrders(stdCode);  // 调用交易适配器获取订单映射表
}

/**
 * @brief 买入操作
 * @param stdCode 标准合约代码
 * @param price 价格
 * @param qty 数量
 * @param bForceClose 是否强制平仓，默认为false
 * @return OrderIDs 返回订单ID列表
 * 
 * 通过交易适配器提交买入订单。如果通道未就绪，返回空列表。
 */
OrderIDs WtLocalExecuter::buy(const char* stdCode, double price, double qty, bool bForceClose/* = false*/)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return OrderIDs();  // 返回空列表

	return _trader->buy(stdCode, price, qty, 0, bForceClose);  // 调用交易适配器提交买入订单（本地订单ID设为0）
}

/**
 * @brief 卖出操作
 * @param stdCode 标准合约代码
 * @param price 价格
 * @param qty 数量
 * @param bForceClose 是否强制平仓，默认为false
 * @return OrderIDs 返回订单ID列表
 * 
 * 通过交易适配器提交卖出订单。如果通道未就绪，返回空列表。
 */
OrderIDs WtLocalExecuter::sell(const char* stdCode, double price, double qty, bool bForceClose/* = false*/)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return OrderIDs();  // 返回空列表

	return _trader->sell(stdCode, price, qty, 0, bForceClose);  // 调用交易适配器提交卖出订单（本地订单ID设为0）
}

/**
 * @brief 撤单操作（按订单ID）
 * @param localid 本地订单ID
 * @return bool 撤单成功返回true，失败返回false
 * 
 * 通过交易适配器撤销指定订单。如果通道未就绪，返回false。
 */
bool WtLocalExecuter::cancel(uint32_t localid)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return false;  // 返回失败

	return _trader->cancel(localid);  // 调用交易适配器撤销订单
}

/**
 * @brief 撤单操作（按合约代码和方向）
 * @param stdCode 标准合约代码
 * @param isBuy 是否买入方向
 * @param qty 撤单数量
 * @return OrderIDs 返回被撤销的订单ID列表
 * 
 * 通过交易适配器撤销指定合约和方向的订单。如果通道未就绪，返回空列表。
 */
OrderIDs WtLocalExecuter::cancel(const char* stdCode, bool isBuy, double qty)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return OrderIDs();  // 返回空列表

	return _trader->cancel(stdCode, isBuy, qty);  // 调用交易适配器撤销订单
}

/**
 * @brief 写入日志
 * @param message 日志消息
 * 
 * 将日志消息写入日志系统，消息前会自动添加执行器名称前缀。
 */
void WtLocalExecuter::writeLog(const char* message)
{
	static thread_local char szBuf[2048] = { 0 };  // 线程局部静态缓冲区，用于格式化日志消息
	fmtutil::format_to(szBuf, "[{}]", _name.c_str());  // 格式化执行器名称前缀
	strcat(szBuf, message);  // 拼接日志消息
	WTSLogger::log_dyn_raw("executer", _name.c_str(), LL_INFO, szBuf);  // 记录动态日志
}

/**
 * @brief 获取商品信息
 * @param stdCode 标准合约代码
 * @return WTSCommodityInfo* 返回商品信息指针
 * 
 * 通过执行器存根获取指定合约的商品信息。
 */
WTSCommodityInfo* WtLocalExecuter::getCommodityInfo(const char* stdCode)
{
	return _stub->get_comm_info(stdCode);  // 调用执行器存根获取商品信息
}

/**
 * @brief 获取交易会话信息
 * @param stdCode 标准合约代码
 * @return WTSSessionInfo* 返回交易会话信息指针
 * 
 * 通过执行器存根获取指定合约的交易会话信息。
 */
WTSSessionInfo* WtLocalExecuter::getSessionInfo(const char* stdCode)
{
	return _stub->get_sess_info(stdCode);  // 调用执行器存根获取交易会话信息
}

/**
 * @brief 获取当前时间
 * @return uint64_t 返回当前时间戳（纳秒级）
 * 
 * 通过执行器存根获取当前实时时间戳。
 */
uint64_t WtLocalExecuter::getCurTime()
{
	return _stub->get_real_time();  // 调用执行器存根获取实时时间戳
	//return TimeUtils::makeTime(_stub->get_date(), _stub->get_raw_time() * 100000 + _stub->get_secs());  // 备用实现：根据日期和时间构建时间戳（已注释）
}

#pragma endregion Context回调接口
//ExecuteContext
//////////////////////////////////////////////////////////////////////////


#pragma region 外部接口
/**
 * @brief 合约仓位变动通知
 * @param stdCode 标准合约代码
 * @param diffPos 仓位变动数量（正数表示增加，负数表示减少）
 * 
 * 处理合约仓位变动通知，更新目标仓位，并根据倍数缩放后转发给执行单元。
 * 如果是增量头寸变动，会记录日志。
 */
void WtLocalExecuter::on_position_changed(const char* stdCode, double diffPos)
{
	ExecuteUnitPtr unit = getUnit(stdCode);  // 获取或创建执行单元（自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回

	double oldVol = _target_pos[stdCode];  // 获取旧的目标仓位
	double newVol = oldVol + diffPos;  // 计算新的目标仓位
	_target_pos[stdCode] = newVol;  // 更新目标仓位映射表

	double traderTarget = round(newVol * _scale);  // 根据倍数缩放目标仓位并四舍五入

	if(!decimal::eq(diffPos, 0))  // 如果仓位变动不为0
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Target position of {} changed: {} -> {} : {} with scale:{}", stdCode, oldVol, newVol, traderTarget, _scale);  // 记录仓位变动日志
	}

	if (_trader && !_trader->checkOrderLimits(stdCode))  // 如果交易适配器存在且合约被禁用（超过委托限制）
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "{} is disabled", stdCode);  // 记录禁用日志
		return;  // 直接返回，不执行仓位设置
	}

	unit->self()->set_position(stdCode, traderTarget);  // 调用执行单元设置目标仓位
}

/**
 * @brief 设置目标仓位
 * @param targets 目标仓位映射表，键为合约代码，值为目标持仓数量
 * 
 * 设置各合约的目标仓位，执行以下操作：
 * 1. 进行合约组合匹配，将组合仓位转换为单个合约仓位
 * 2. 遍历目标仓位，根据配置的倍数缩放后转发给执行单元
 * 3. 对于不在新目标中的合约，自动设置为0
 * 4. 如果开启严格同步，检查通道持仓并平掉不在管理中的仓位
 */
void WtLocalExecuter::set_position(const wt_hashmap<std::string, double>& targets)
{
	/*
	 *	先要把目标头寸进行组合匹配
	 */
	auto real_targets = targets;  // 复制目标仓位映射表
	for(auto& v : _groups)  // 遍历所有合约组合
	{
		const CodeGroupPtr& gpInfo = v.second;  // 获取组合信息常量引用
		bool bHit = false;  // 组合匹配标志
		double gpQty = DBL_MAX;  // 组合单位数量（初始化为最大值）
		for(auto& vi : gpInfo->_items)  // 遍历组合中的所有合约
		{
			double unit = vi.second;  // 获取该合约在组合中的权重
			auto it = real_targets.find(vi.first);  // 在目标仓位中查找该合约
			if (it == real_targets.end())  // 如果未找到
			{
				bHit = false;  // 标记为未匹配
				break;  // 跳出循环
			}
			else  // 如果找到
			{
				bHit = true;  // 标记为匹配
				//计算最小的组合单位数量
				gpQty = std::min(gpQty, decimal::mod(it->second, unit));  // 计算该合约能匹配的组合单位数量（取最小值）
			}
		}

		if(bHit && decimal::gt(gpQty, 0))  // 如果组合匹配成功且组合单位数量大于0
		{
			real_targets[gpInfo->_name] = gpQty;  // 将组合名称和单位数量添加到目标仓位
			for (auto& vi : gpInfo->_items)  // 遍历组合中的所有合约
			{
				double unit = vi.second;  // 获取该合约在组合中的权重
				real_targets[vi.first] -= gpQty * unit;  // 从该合约的目标仓位中减去组合仓位
			}
		}
	}


	for (auto it = targets.begin(); it != targets.end(); it++)  // 遍历原始目标仓位映射表
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码
		double newVol = it->second;  // 获取新目标仓位
		ExecuteUnitPtr unit = getUnit(stdCode);  // 获取或创建执行单元（自动创建）
		if (unit == NULL)  // 如果执行单元不存在
			continue;  // 跳过该合约

		double oldVol = _target_pos[stdCode];  // 获取旧目标仓位
		_target_pos[stdCode] = newVol;  // 更新目标仓位映射表
		// 账户的理论持仓要经过修正
		double traderTarget = round(newVol * _scale);  // 根据倍数缩放目标仓位并四舍五入

		if(!decimal::eq(oldVol, newVol))  // 如果目标仓位发生变化
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Target position of {} changed: {} -> {} : {} with scale{}", stdCode, oldVol, newVol, traderTarget, _scale);  // 记录仓位变动日志
		}

		if (_trader && !_trader->checkOrderLimits(stdCode))  // 如果交易适配器存在且合约被禁用（超过委托限制）
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_WARN, "{} is disabled due to entrust limit control ", stdCode);  // 记录警告日志
			continue;  // 跳过该合约
		}

		if (_pool)  // 如果线程池已创建
		{
			std::string code = stdCode;  // 创建代码字符串副本（Lambda需要值捕获）
			_pool->schedule([unit, code, traderTarget](){  // 在线程池中异步执行
				unit->self()->set_position(code.c_str(), traderTarget);  // 调用执行单元设置目标仓位
			});
		}
		else  // 如果线程池未创建
		{
			unit->self()->set_position(stdCode, traderTarget);  // 直接调用执行单元设置目标仓位
		}
	}

	//在原来的目标头寸中，但是不在新的目标头寸中，则需要自动设置为0
	for (auto it = _target_pos.begin(); it != _target_pos.end(); it++)  // 遍历当前目标仓位映射表
	{
		const char* code = it->first.c_str();  // 获取合约代码
		double& pos = (double&)it->second;  // 获取目标仓位引用
		auto tit = targets.find(code);  // 在新的目标仓位中查找该合约
		if(tit != targets.end())  // 如果找到（该合约仍在目标中）
			continue;  // 跳过该合约

		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "{} is not in target, set to 0 automatically", code);  // 记录自动清零日志

		ExecuteUnitPtr unit = getUnit(code);  // 获取执行单元（自动创建）
		if (unit == NULL)  // 如果执行单元不存在
			continue;  // 跳过该合约

		//unit->self()->set_position(code, 0);
		if (_pool)  // 如果线程池已创建
		{
			_pool->schedule([unit, code](){  // 在线程池中异步执行
				unit->self()->set_position(code, 0);  // 调用执行单元设置目标仓位为0
			});
		}
		else  // 如果线程池未创建
		{
			unit->self()->set_position(code, 0);  // 直接调用执行单元设置目标仓位为0
		}

		pos = 0;  // 更新目标仓位映射表为0
	}

	//如果开启了严格同步，则需要检查通道持仓
	//如果通道持仓不在管理中，则直接平掉
	if(_strict_sync)  // 如果开启严格同步模式
	{
		for(const std::string& stdCode : _channel_holds)  // 遍历通道持仓集合
		{
			auto it = _target_pos.find(stdCode.c_str());  // 在目标仓位映射表中查找该合约
			if(it != _target_pos.end())  // 如果找到（该合约在管理中）
				continue;  // 跳过该合约

			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "{} is not in management, set to 0 due to strict sync mode", stdCode.c_str());  // 记录严格同步日志

			ExecuteUnitPtr unit = getUnit(stdCode.c_str());  // 获取执行单元（自动创建）
			if (unit == NULL)  // 如果执行单元不存在
				continue;  // 跳过该合约

			if (_pool)  // 如果线程池已创建
			{
				std::string code = stdCode.c_str();  // 创建代码字符串副本
				_pool->schedule([unit, code]() {  // 在线程池中异步执行
					unit->self()->set_position(code.c_str(), 0);  // 调用执行单元设置目标仓位为0
				});
			}
			else  // 如果线程池未创建
			{
				unit->self()->set_position(stdCode.c_str(), 0);  // 直接调用执行单元设置目标仓位为0
			}
		}
	}
}

/**
 * @brief 实时行情回调
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 将实时行情数据转发给对应的执行单元。如果配置了线程池，则异步处理。
 */
void WtLocalExecuter::on_tick(const char* stdCode, WTSTickData* newTick)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回

	//unit->self()->on_tick(newTick);
	if (_pool)  // 如果线程池已创建
	{
		newTick->retain();  // 增加Tick数据引用计数（防止异步处理时数据被释放）
		_pool->schedule([unit, newTick](){  // 在线程池中异步执行
			unit->self()->on_tick(newTick);  // 调用执行单元的行情回调方法
			newTick->release();  // 释放Tick数据引用计数
		});
	}
	else  // 如果线程池未创建
	{
		unit->self()->on_tick(newTick);  // 直接调用执行单元的行情回调方法
	}
}

/**
 * @brief 成交回报
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否买入
 * @param vol 成交数量
 * @param price 成交价格
 * 
 * 将成交回报转发给对应的执行单元。如果配置了线程池，则异步处理。
 */
void WtLocalExecuter::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回

	//unit->self()->on_trade(stdCode, isBuy, vol, price);
	if (_pool)  // 如果线程池已创建
	{
		std::string code = stdCode;  // 创建代码字符串副本（Lambda需要值捕获）
		_pool->schedule([localid, unit, code, isBuy, vol, price](){  // 在线程池中异步执行
			unit->self()->on_trade(localid, code.c_str(), isBuy, vol, price);  // 调用执行单元的成交回调方法
		});
	}
	else  // 如果线程池未创建
	{
		unit->self()->on_trade(localid, stdCode, isBuy, vol, price);  // 直接调用执行单元的成交回调方法
	}
}

/**
 * @brief 订单回报
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否买入
 * @param totalQty 订单总数量
 * @param leftQty 剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤销，默认为false
 * 
 * 将订单回报转发给对应的执行单元。如果配置了线程池，则异步处理。
 */
void WtLocalExecuter::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled /* = false */)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回

	//unit->self()->on_order(localid, stdCode, isBuy, leftQty, price, isCanceled);
	if (_pool)  // 如果线程池已创建
	{
		std::string code = stdCode;  // 创建代码字符串副本（Lambda需要值捕获）
		_pool->schedule([localid, unit, code, isBuy, leftQty, price, isCanceled](){  // 在线程池中异步执行
			unit->self()->on_order(localid, code.c_str(), isBuy, leftQty, price, isCanceled);  // 调用执行单元的订单回调方法
		});
	}
	else  // 如果线程池未创建
	{
		unit->self()->on_order(localid, stdCode, isBuy, leftQty, price, isCanceled);  // 直接调用执行单元的订单回调方法
	}
}

/**
 * @brief 委托回报
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 消息
 * 
 * 将委托回报转发给对应的执行单元。如果配置了线程池，则异步处理。
 */
void WtLocalExecuter::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回

	//unit->self()->on_entrust(localid, stdCode, bSuccess, message);
	if (_pool)  // 如果线程池已创建
	{
		std::string code = stdCode;  // 创建代码字符串副本（Lambda需要值捕获）
		std::string msg = message;  // 创建消息字符串副本（Lambda需要值捕获）
		_pool->schedule([unit, localid, code, bSuccess, msg](){  // 在线程池中异步执行
			unit->self()->on_entrust(localid, code.c_str(), bSuccess, msg.c_str());  // 调用执行单元的委托回调方法
		});
	}
	else  // 如果线程池未创建
	{
		unit->self()->on_entrust(localid, stdCode, bSuccess, message);  // 直接调用执行单元的委托回调方法
	}
}

/**
 * @brief 交易通道就绪回调
 * 
 * 处理交易通道就绪事件，设置就绪标志，并通知所有执行单元。
 */
void WtLocalExecuter::on_channel_ready()
{
	_channel_ready = true;  // 设置通道就绪标志为true
	SpinLock lock(_mtx_units);  // 获取自旋锁，保护执行单元映射表的并发访问
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)  // 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;  // 获取执行单元引用
		if (unitPtr)  // 如果执行单元指针有效
		{
			//unitPtr->self()->on_channel_ready();
			if (_pool)  // 如果线程池已创建
			{
				_pool->schedule([unitPtr](){  // 在线程池中异步执行
					unitPtr->self()->on_channel_ready();  // 调用执行单元的通道就绪回调方法
				});
			}
			else  // 如果线程池未创建
			{
				unitPtr->self()->on_channel_ready();  // 直接调用执行单元的通道就绪回调方法
			}
		}
	}
}

/**
 * @brief 交易通道丢失回调
 * 
 * 处理交易通道丢失事件，清除就绪标志，并通知所有执行单元。
 */
void WtLocalExecuter::on_channel_lost()
{
	_channel_ready = false;  // 设置通道就绪标志为false
	SpinLock lock(_mtx_units);  // 获取自旋锁，保护执行单元映射表的并发访问
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)  // 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;  // 获取执行单元引用
		if (unitPtr)  // 如果执行单元指针有效
		{
			if (_pool)  // 如果线程池已创建
			{
				_pool->schedule([unitPtr](){  // 在线程池中异步执行
					unitPtr->self()->on_channel_lost();  // 调用执行单元的通道丢失回调方法
				});
			}
			else  // 如果线程池未创建
			{
				unitPtr->self()->on_channel_lost();  // 直接调用执行单元的通道丢失回调方法
			}
		}
	}
}

/**
 * @brief 资金回报
 * @param currency 币种
 * @param prebalance 之前余额
 * @param balance 当前余额
 * @param dynbalance 动态余额
 * @param avaliable 可用资金
 * @param closeprofit 平仓盈亏
 * @param dynprofit 浮动盈亏
 * @param margin 保证金
 * @param fee 手续费
 * @param deposit 入金
 * @param withdraw 出金
 * 
 * 将资金回报转发给所有执行单元。如果配置了线程池，则异步处理。
 */
void WtLocalExecuter::on_account(const char* currency, double prebalance, double balance, double dynbalance, 
	double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw)
{
	SpinLock lock(_mtx_units);  // 获取自旋锁，保护执行单元映射表的并发访问
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)  // 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;  // 获取执行单元引用
		if (unitPtr)  // 如果执行单元指针有效
		{
			if (_pool)  // 如果线程池已创建
			{
				std::string strCur = currency;  // 创建币种字符串副本（Lambda需要值捕获）
				_pool->schedule([unitPtr, strCur, prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw]() {  // 在线程池中异步执行
					unitPtr->self()->on_account(strCur.c_str(), prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw);  // 调用执行单元的资金回调方法
				});
			}
			else  // 如果线程池未创建
			{
				unitPtr->self()->on_account(currency, prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw);  // 直接调用执行单元的资金回调方法
			}
		}
	}
}

/**
 * @brief 持仓回报
 * @param stdCode 标准合约代码
 * @param isLong 是否多头
 * @param prevol 之前持仓量
 * @param preavail 之前可用持仓量
 * @param newvol 新持仓量
 * @param newavail 新可用持仓量
 * @param tradingday 交易日
 * 
 * 处理持仓回报，记录通道持仓。如果开启了自动清理功能，检查并清理上一期主力合约的头寸。
 * 清理逻辑：
 * 1. 检查是否为分月期货合约
 * 2. 获取上一期的主力合约
 * 3. 如果当前合约是上一期的主力合约，检查是否在排除列表中
 * 4. 如果包含列表不为空，检查是否在包含列表中
 * 5. 如果满足条件，调用执行单元清理所有持仓
 */
void WtLocalExecuter::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	_channel_holds.insert(stdCode);  // 将合约代码添加到通道持仓集合

	/*
	 *	By Wesley @ 2021.12.14
	 *	先检查自动清理过期主力合约的标记是否为true
	 *	如果不为true，则直接退出该逻辑
	 */
	if (!_auto_clear)  // 如果自动清理功能未启用
		return;  // 直接返回

	//如果不是分月期货合约，直接退出
	if (!CodeHelper::isStdMonthlyFutCode(stdCode))  // 如果不是标准分月期货合约
		return;  // 直接返回

	IHotMgr* hotMgr = _stub->get_hot_mon();  // 获取热点合约管理器
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 解析合约代码，提取品种信息
	//获取上一期的主力合约
	std::string prevCode = hotMgr->getPrevRawCode(cInfo._exchg, cInfo._product, tradingday);  // 获取上一期的主力合约代码

	//如果当前合约不是上一期的主力合约，则直接退出
	if (prevCode != cInfo._code)  // 如果当前合约不是上一期的主力合约
		return;  // 直接返回

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Prev hot contract of {}.{} on {} is {}", cInfo._exchg, cInfo._product, tradingday, prevCode);  // 记录日志：上一期主力合约

	thread_local static char fullPid[64] = { 0 };  // 线程局部静态缓冲区，用于格式化品种ID
	fmtutil::format_to(fullPid, "{}.{}", cInfo._exchg, cInfo._product);  // 格式化品种ID（如"CFFEX.IF"）

	//先检查排除列表
	//如果在排除列表中，则直接退出
	auto it = _clear_excludes.find(fullPid);  // 在排除列表中查找品种ID
	if(it != _clear_excludes.end())  // 如果找到（在排除列表中）
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Position of {}, as prev hot contract, won't be cleared for it's in exclude list", stdCode);  // 记录日志：不清理（在排除列表中）
		return;  // 直接返回
	}

	//如果包含列表不为空，再检查是否在包含列表中
	//如果为空，则全部清理，不再进入该逻辑
	if(!_clear_includes.empty())  // 如果包含列表不为空
	{
		it = _clear_includes.find(fullPid);  // 在包含列表中查找品种ID
		if (it == _clear_includes.end())  // 如果未找到（不在包含列表中）
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Position of {}, as prev hot contract, won't be cleared for it's not in include list", stdCode);  // 记录日志：不清理（不在包含列表中）
			return;  // 直接返回
		}
	}

	//最后再进行自动清理
	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Position of {}, as prev hot contract, will be cleared", stdCode);  // 记录日志：将清理持仓
	ExecuteUnitPtr unit = getUnit(stdCode);  // 获取执行单元（自动创建）
	if (unit)  // 如果执行单元存在
	{
		if (_pool)  // 如果线程池已创建
		{
			std::string code = stdCode;  // 创建代码字符串副本（Lambda需要值捕获）
			_pool->schedule([unit, code](){  // 在线程池中异步执行
				unit->self()->clear_all_position(code.c_str());  // 调用执行单元清理所有持仓
			});
		}
		else  // 如果线程池未创建
		{
			unit->self()->clear_all_position(stdCode);  // 直接调用执行单元清理所有持仓
		}
	}
}

#pragma endregion 外部接口
