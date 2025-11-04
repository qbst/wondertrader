/*!
 * \file WtExecuter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 套利执行器实现文件
 *
 * 文件设计逻辑和作用总结：
 * ========================
 * 本文件实现了 WtArbiExecuter 类的所有功能，是套利交易执行器的核心实现。
 *
 * 主要实现内容：
 * 1. 执行单元管理：为每个合约创建和管理执行单元，执行单元负责具体的交易执行逻辑
 * 2. 目标仓位处理：接收策略的目标仓位信号，并将目标仓位分配给对应的执行单元
 * 3. 合约组合处理：支持合约组合交易，可以将多个合约组合成一个交易单元
 * 4. 自动清理功能：支持自动清理上一期主力合约的头寸，避免换月风险
 * 5. 严格同步模式：在严格同步模式下，自动清理不在管理范围内的持仓
 * 6. 线程池支持：支持使用线程池并发处理多个执行单元的操作
 * 7. 交易回调处理：接收交易通道的状态变化、成交、订单等回调通知，并分发给执行单元
 *
 * 关键设计点：
 * - 使用工厂模式创建执行单元，支持不同类型的执行策略
 * - 支持仓位放大倍数（scale），可以将策略仓位放大后执行
 * - 实现了交易通知接收器接口，接收交易通道的状态变化
 * - 实现了执行上下文接口，为执行单元提供数据访问接口
 */
#include "WtArbiExecuter.h"
#include "TraderAdapter.h"
#include "WtEngine.h"

#include "../Share/CodeHelper.hpp"
#include "../Includes/IDataManager.h"
#include "../Includes/WTSVariant.hpp"
#include "../Includes/IHotMgr.h"
#include "../Share/decimal.h"

#include "../WTSTools/WTSLogger.h"

USING_NS_WTP;


/**
 * @brief 构造函数
 * @param factory 执行器工厂指针，用于创建执行单元
 * @param name 执行器名称
 * @param dataMgr 数据管理器指针，用于获取行情数据
 * 
 * 初始化套利执行器，设置默认参数：
 * - 通道就绪状态为false
 * - 仓位放大倍数为1.0
 * - 自动清理功能启用
 * - 交易适配器为空
 */
WtArbiExecuter::WtArbiExecuter(WtExecuterFactory* factory, const char* name, IDataManager* dataMgr)
	: IExecCommand(name)											// 调用基类构造函数，设置执行器名称
	, _factory(factory)											// 保存执行器工厂指针
	, _data_mgr(dataMgr)											// 保存数据管理器指针
	, _channel_ready(false)										// 初始化通道就绪状态为false
	, _scale(1.0)													// 初始化仓位放大倍数为1.0
	, _auto_clear(true)											// 初始化自动清理功能为启用
	, _trader(NULL)												// 初始化交易适配器为空
{
}


/**
 * @brief 析构函数
 * 
 * 等待线程池中的所有任务完成后再销毁对象。
 */
WtArbiExecuter::~WtArbiExecuter()
{
	if (_pool)														// 如果线程池存在
		_pool->wait();												// 等待所有任务完成
}

/**
 * @brief 设置交易适配器
 * @param adapter 交易适配器指针
 * 
 * 设置交易适配器，并读取交易通道的就绪状态。
 */
void WtArbiExecuter::setTrader(TraderAdapter* adapter)
{
	_trader = adapter;												// 保存交易适配器指针
	// 设置的时候读取一下trader的状态
	if(_trader)														// 如果交易适配器存在
		_channel_ready = _trader->isReady();						// 读取交易通道的就绪状态
}

/**
 * @brief 初始化执行器
 * @param params 配置参数对象
 * @return 初始化是否成功
 * 
 * 从配置参数中读取执行器的配置信息：
 * 1. 仓位放大倍数（scale）
 * 2. 严格同步模式（strict_sync）
 * 3. 线程池大小（poolsize）
 * 4. 自动清理配置（clear）：包含列表、排除列表
 * 5. 合约组合配置（groups）
 */
bool WtArbiExecuter::init(WTSVariant* params)
{
	if (params == NULL)												// 如果配置参数为空，返回失败
		return false;

	_config = params;												// 保存配置参数
	_config->retain();												// 增加引用计数

	_scale = params->getDouble("scale");							// 读取仓位放大倍数
	_strict_sync  = params->getBoolean("strict_sync");				// 读取严格同步模式标志

	uint32_t poolsize = params->getUInt32("poolsize");				// 读取线程池大小
	if(poolsize > 0)												// 如果线程池大小大于0
	{
		_pool.reset(new boost::threadpool::pool(poolsize));		// 创建线程池
	}

	/*
	 *	By Wesley @ 2021.12.14
	 *	从配置文件中读取自动清理的策略
	 *	active: 是否启用
	 *	includes: 包含列表，格式如CFFEX.IF
	 *	excludes: 排除列表，格式如CFFEX.IF
	 */
	WTSVariant* cfgClear = params->get("clear");					// 获取自动清理配置
	if(cfgClear)													// 如果配置存在
	{
		_auto_clear = cfgClear->getBoolean("active");				// 读取是否启用自动清理
		WTSVariant* cfgItem = cfgClear->get("includes");			// 获取包含列表配置
		if(cfgItem)													// 如果包含列表配置存在
		{
			if (cfgItem->type() == WTSVariant::VT_String)			// 如果是字符串类型（单个品种）
				_clear_includes.insert(cfgItem->asCString());		// 添加到包含列表
			else if (cfgItem->type() == WTSVariant::VT_Array)		// 如果是数组类型（多个品种）
			{
				for(uint32_t i = 0; i < cfgItem->size(); i++)		// 遍历数组
					_clear_includes.insert(cfgItem->get(i)->asCString());	// 添加到包含列表
			}
		}

		cfgItem = cfgClear->get("excludes");						// 获取排除列表配置
		if (cfgItem)												// 如果排除列表配置存在
		{
			if (cfgItem->type() == WTSVariant::VT_String)			// 如果是字符串类型（单个品种）
				_clear_excludes.insert(cfgItem->asCString());		// 添加到排除列表
			else if (cfgItem->type() == WTSVariant::VT_Array)		// 如果是数组类型（多个品种）
			{
				for (uint32_t i = 0; i < cfgItem->size(); i++)		// 遍历数组
					_clear_excludes.insert(cfgItem->get(i)->asCString());	// 添加到排除列表
			}
		}
	}

	WTSVariant* cfgGroups = params->get("groups");					// 获取合约组合配置
	if (cfgGroups)													// 如果配置存在
	{
		auto names = cfgGroups->memberNames();						// 获取所有组合名称
		for(const std::string& gpname : names)						// 遍历所有组合
		{
			CodeGroupPtr& gpInfo = _groups[gpname];					// 获取或创建组合对象
			if (gpInfo == NULL)										// 如果组合不存在
			{
				gpInfo.reset(new CodeGroup);							// 创建新的组合对象
				wt_strcpy(gpInfo->_name, gpname.c_str(), gpname.size());	// 设置组合名称
			}

			WTSVariant* cfgGrp = cfgGroups->get(gpname.c_str());	// 获取组合的详细配置
			auto codes = cfgGrp->memberNames();						// 获取组合中的所有合约代码
			for(const std::string& code : codes)					// 遍历所有合约
			{
				gpInfo->_items[code] = cfgGrp->getDouble(code.c_str());	// 保存合约在组合中的比例
				_code_to_groups[code] = gpInfo;						// 建立合约到组合的映射
			}
		}
	}

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Local executer inited, scale: {}, auto_clear: {}, strict_sync: {}, thread poolsize: {}, code_groups: {}",
		_scale, _auto_clear, _strict_sync, poolsize, _groups.size());	// 记录初始化日志

	return true;													// 初始化成功
}

/**
 * @brief 获取或创建执行单元
 * @param stdCode 标准合约代码
 * @param bAutoCreate 是否自动创建（true=不存在时自动创建，false=不存在时返回空）
 * @return 执行单元指针
 * 
 * 根据合约代码获取执行单元，如果不存在且允许自动创建，则创建新的执行单元。
 * 执行单元的类型由策略配置决定。
 */
ExecuteUnitPtr WtArbiExecuter::getUnit(const char* stdCode, bool bAutoCreate /* = true */)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, NULL);	// 解析标准合约代码，提取品种信息
	std::string commID = codeInfo.stdCommID();									// 获取标准品种ID（格式：交易所.品种）

	WTSVariant* policy = _config->get("policy");									// 获取策略配置
	std::string des = commID;														// 默认使用品种ID作为策略名称
	if (!policy->has(commID.c_str()))												// 如果该品种没有配置策略
		des = "default";															// 使用默认策略

	SpinLock lock(_mtx_units);														// 加锁保护执行单元映射表

	auto it = _unit_map.find(stdCode);												// 查找执行单元
	if(it != _unit_map.end())														// 如果执行单元已存在
	{
		return it->second;															// 直接返回
	}

	if (bAutoCreate)																// 如果允许自动创建
	{
		WTSVariant* cfg = policy->get(des.c_str());									// 获取策略配置

		const char* name = cfg->getCString("name");									// 获取执行单元类型名称
		ExecuteUnitPtr unit = _factory->createExeUnit(name);						// 使用工厂创建执行单元
		if (unit != NULL)															// 如果创建成功
		{
			_unit_map[stdCode] = unit;												// 保存到映射表
			unit->self()->init(this, stdCode, cfg);									// 初始化执行单元

			// 如果通道已经就绪，则直接通知执行单元
			if (_channel_ready)														// 如果交易通道已就绪
				unit->self()->on_channel_ready();									// 通知执行单元通道就绪
		}
		return unit;																// 返回执行单元
	}
	else																			// 如果不允许自动创建
	{
		return ExecuteUnitPtr();													// 返回空指针
	}
}


//////////////////////////////////////////////////////////////////////////
//ExecuteContext
#pragma region Context回调接口

/**
 * @brief 获取tick数据切片
 * @param stdCode 标准合约代码
 * @param count 获取的tick数量
 * @param etime 结束时间戳，当前未使用
 * @return tick数据切片指针，如果数据管理器不存在则返回NULL
 * 
 * 实现ExecuteContext接口，为执行单元提供历史tick数据访问。
 */
WTSTickSlice* WtArbiExecuter::getTicks(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_data_mgr == NULL)											// 如果数据管理器不存在，返回NULL
		return NULL;

	return _data_mgr->get_tick_slice(stdCode, count);				// 从数据管理器获取tick数据切片
}

/**
 * @brief 获取最新tick数据
 * @param stdCode 标准合约代码
 * @return 最新tick数据指针，如果数据管理器不存在则返回NULL
 * 
 * 实现ExecuteContext接口，为执行单元提供最新tick数据访问。
 * 注意：返回的tick数据需要调用者负责释放。
 */
WTSTickData* WtArbiExecuter::grabLastTick(const char* stdCode)
{
	if (_data_mgr == NULL)											// 如果数据管理器不存在，返回NULL
		return NULL;

	return _data_mgr->grab_last_tick(stdCode);						// 从数据管理器获取最新tick数据
}

/**
 * @brief 获取持仓数量
 * @param stdCode 标准合约代码
 * @param validOnly 是否只返回可用持仓（true=只返回可用持仓，false=返回全部持仓）
 * @param flag 持仓标志位（1=多头，2=空头，3=全部）
 * @return 持仓数量，如果交易适配器不存在则返回0.0
 * 
 * 实现ExecuteContext接口，为执行单元提供持仓查询功能。
 */
double WtArbiExecuter::getPosition(const char* stdCode, bool validOnly /* = true */, int32_t flag /* = 3 */)
{
	if (NULL == _trader)												// 如果交易适配器不存在，返回0.0
		return 0.0;

	return _trader->getPosition(stdCode, validOnly, flag);				// 从交易适配器获取持仓数量
}

/**
 * @brief 获取未完成订单数量
 * @param stdCode 标准合约代码
 * @return 未完成订单数量（正数表示买入未完成，负数表示卖出未完成），如果交易适配器不存在则返回0.0
 * 
 * 实现ExecuteContext接口，为执行单元提供未完成订单查询功能。
 */
double WtArbiExecuter::getUndoneQty(const char* stdCode)
{
	if (NULL == _trader)												// 如果交易适配器不存在，返回0.0
		return 0.0;

	return _trader->getUndoneQty(stdCode);								// 从交易适配器获取未完成订单数量
}

/**
 * @brief 获取订单映射表
 * @param stdCode 标准合约代码，空字符串表示获取所有订单
 * @return 订单映射表指针，如果交易适配器不存在则返回NULL
 * 
 * 实现ExecuteContext接口，为执行单元提供订单查询功能。
 * 注意：返回的订单映射表需要调用者负责释放。
 */
OrderMap* WtArbiExecuter::getOrders(const char* stdCode)
{
	if (NULL == _trader)												// 如果交易适配器不存在，返回NULL
		return NULL;

	return _trader->getOrders(stdCode);									// 从交易适配器获取订单映射表
}

/**
 * @brief 买入下单
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param bForceClose 是否强制平仓（true=强制平仓，false=正常开平仓）
 * @return 订单ID列表，如果交易通道未就绪则返回空列表
 * 
 * 实现ExecuteContext接口，为执行单元提供买入下单功能。
 * 如果交易通道未就绪，则直接返回空列表，不执行下单操作。
 */
OrderIDs WtArbiExecuter::buy(const char* stdCode, double price, double qty, bool bForceClose/* = false*/)
{
	if (!_channel_ready)												// 如果交易通道未就绪，返回空列表
		return OrderIDs();

	return _trader->buy(stdCode, price, qty, 0, bForceClose);			// 调用交易适配器买入下单（订单标志为0）
}

/**
 * @brief 卖出下单
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param bForceClose 是否强制平仓（true=强制平仓，false=正常开平仓）
 * @return 订单ID列表，如果交易通道未就绪则返回空列表
 * 
 * 实现ExecuteContext接口，为执行单元提供卖出下单功能。
 * 如果交易通道未就绪，则直接返回空列表，不执行下单操作。
 */
OrderIDs WtArbiExecuter::sell(const char* stdCode, double price, double qty, bool bForceClose/* = false*/)
{
	if (!_channel_ready)												// 如果交易通道未就绪，返回空列表
		return OrderIDs();

	return _trader->sell(stdCode, price, qty, 0, bForceClose);			// 调用交易适配器卖出下单（订单标志为0）
}

/**
 * @brief 根据本地订单ID撤单
 * @param localid 本地订单ID
 * @return 撤单是否成功（true=成功，false=失败），如果交易通道未就绪则返回false
 * 
 * 实现ExecuteContext接口，为执行单元提供撤单功能。
 * 如果交易通道未就绪，则直接返回false，不执行撤单操作。
 */
bool WtArbiExecuter::cancel(uint32_t localid)
{
	if (!_channel_ready)												// 如果交易通道未就绪，返回false
		return false;

	return _trader->cancel(localid);										// 调用交易适配器撤单
}

/**
 * @brief 根据合约代码和方向撤单
 * @param stdCode 标准合约代码
 * @param isBuy 是否买入方向（true=撤买入单，false=撤卖出单）
 * @param qty 撤单数量，0表示撤该方向的所有订单
 * @return 已撤单的订单ID列表，如果交易通道未就绪则返回空列表
 * 
 * 实现ExecuteContext接口，为执行单元提供批量撤单功能。
 * 如果交易通道未就绪，则直接返回空列表，不执行撤单操作。
 */
OrderIDs WtArbiExecuter::cancel(const char* stdCode, bool isBuy, double qty)
{
	if (!_channel_ready)												// 如果交易通道未就绪，返回空列表
		return OrderIDs();

	return _trader->cancel(stdCode, isBuy, qty);							// 调用交易适配器批量撤单
}

/**
 * @brief 写日志
 * @param message 日志消息
 * 
 * 实现ExecuteContext接口，为执行单元提供日志记录功能。
 * 日志格式：[执行器名称] + 消息内容。
 */
void WtArbiExecuter::writeLog(const char* message)
{
	static thread_local char szBuf[2048] = { 0 };						// 线程局部静态缓冲区
	fmtutil::format_to(szBuf, "[{}]", _name.c_str());					// 格式化执行器名称前缀
	strcat(szBuf, message);												// 追加日志消息
	WTSLogger::log_dyn_raw("executer", _name.c_str(), LL_INFO, szBuf);	// 记录日志
}

/**
 * @brief 获取商品信息
 * @param stdCode 标准合约代码
 * @return 商品信息指针，如果stub不存在则返回NULL
 * 
 * 实现ExecuteContext接口，为执行单元提供商品信息查询功能。
 */
WTSCommodityInfo* WtArbiExecuter::getCommodityInfo(const char* stdCode)
{
	return _stub->get_comm_info(stdCode);									// 从stub获取商品信息
}

/**
 * @brief 获取交易时段信息
 * @param stdCode 标准合约代码
 * @return 交易时段信息指针，如果stub不存在则返回NULL
 * 
 * 实现ExecuteContext接口，为执行单元提供交易时段信息查询功能。
 */
WTSSessionInfo* WtArbiExecuter::getSessionInfo(const char* stdCode)
{
	return _stub->get_sess_info(stdCode);									// 从stub获取交易时段信息
}

/**
 * @brief 获取当前时间戳
 * @return 当前时间戳（微秒精度）
 * 
 * 实现ExecuteContext接口，为执行单元提供当前时间查询功能。
 */
uint64_t WtArbiExecuter::getCurTime()
{
	return _stub->get_real_time();											// 从stub获取当前时间戳
	//return TimeUtils::makeTime(_stub->get_date(), _stub->get_raw_time() * 100000 + _stub->get_secs());	// 备用实现方式（已注释）
}

#pragma endregion Context回调接口
//ExecuteContext
//////////////////////////////////////////////////////////////////////////


#pragma region 外部接口

/**
 * @brief 持仓变化通知
 * @param stdCode 标准合约代码
 * @param diffPos 持仓变化量（正数表示增加，负数表示减少）
 * 
 * 处理策略持仓变化通知：
 * 1. 更新目标持仓（原有持仓 + 变化量）
 * 2. 应用仓位放大倍数，计算交易通道的目标持仓
 * 3. 检查交易限制（如果合约被禁用，则不执行）
 * 4. 通知执行单元更新目标持仓
 */
void WtArbiExecuter::on_position_changed(const char* stdCode, double diffPos)
{
	ExecuteUnitPtr unit = getUnit(stdCode);									// 获取或创建执行单元
	if (unit == NULL)														// 如果执行单元不存在，直接返回
		return;

	double oldVol = _target_pos[stdCode];									// 获取原有目标持仓
	double newVol = oldVol + diffPos;										// 计算新的目标持仓
	_target_pos[stdCode] = newVol;											// 更新目标持仓

	double traderTarget = round(newVol * _scale);							// 应用仓位放大倍数，计算交易通道的目标持仓

	if(!decimal::eq(diffPos, 0))											// 如果持仓有变化
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Target position of {} changed: {} -> {} : {} with scale:{}", stdCode, oldVol, newVol, traderTarget, _scale);	// 记录持仓变化日志
	}

	if (_trader && !_trader->checkOrderLimits(stdCode))						// 如果交易适配器存在但合约被禁用
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "{} is disabled", stdCode);	// 记录禁用日志
		return;																// 直接返回，不执行
	}

	unit->self()->set_position(stdCode, traderTarget);						// 通知执行单元更新目标持仓
}

/**
 * @brief 批量设置目标持仓
 * @param targets 目标持仓映射表（合约代码 -> 目标持仓）
 * 
 * 处理策略的批量目标持仓设置：
 * 1. 合约组合匹配：将组合中的合约目标持仓转换为组合目标持仓
 * 2. 更新各合约的目标持仓并通知执行单元
 * 3. 清理不在新目标持仓中的合约（设置为0）
 * 4. 严格同步模式：清理不在管理范围内的通道持仓
 * 
 * 如果配置了线程池，则使用线程池并发处理多个执行单元的操作。
 */
void WtArbiExecuter::set_position(const wt_hashmap<std::string, double>& targets)
{
	/*
	 *	先要把目标头寸进行组合匹配
	 */
	auto real_targets = targets;											// 复制目标持仓映射表
	for(auto& v : _groups)													// 遍历所有合约组合
	{
		const CodeGroupPtr& gpInfo = v.second;								// 获取组合信息
		bool bHit = false;													// 是否匹配到组合
		double gpQty = DBL_MAX;												// 组合数量（初始化为最大值）
		for(auto& vi : gpInfo->_items)										// 遍历组合中的所有合约
		{
			double unit = vi.second;										// 获取合约在组合中的比例
			auto it = real_targets.find(vi.first);							// 查找合约是否在目标持仓中
			if (it == real_targets.end())									// 如果合约不在目标持仓中
			{
				bHit = false;												// 标记为不匹配
				break;														// 退出循环
			}
			else															// 如果合约在目标持仓中
			{
				bHit = true;												// 标记为匹配
				// 计算最小的组合单位数量
				gpQty = std::min(gpQty, decimal::mod(it->second, unit));	// 计算合约目标持仓除以比例的余数，取最小值
			}
		}

		if(bHit && decimal::gt(gpQty, 0))									// 如果匹配到组合且组合数量大于0
		{
			real_targets[gpInfo->_name] = gpQty;							// 设置组合的目标持仓
			for (auto& vi : gpInfo->_items)									// 遍历组合中的所有合约
			{
				double unit = vi.second;									// 获取合约在组合中的比例
				real_targets[vi.first] -= gpQty * unit;						// 从合约目标持仓中减去组合持仓（减去组合的组成部分）
			}
		}
	}


	for (auto it = targets.begin(); it != targets.end(); it++)				// 遍历所有目标持仓
	{
		const char* stdCode = it->first.c_str();							// 获取合约代码
		double newVol = it->second;										// 获取新的目标持仓
		ExecuteUnitPtr unit = getUnit(stdCode);								// 获取或创建执行单元
		if (unit == NULL)													// 如果执行单元不存在，跳过
			continue;

		double oldVol = _target_pos[stdCode];								// 获取原有目标持仓
		_target_pos[stdCode] = newVol;										// 更新目标持仓
		// 账户的理论持仓要经过修正
		double traderTarget = round(newVol * _scale);						// 应用仓位放大倍数，计算交易通道的目标持仓

		if(!decimal::eq(oldVol, newVol))									// 如果目标持仓有变化
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Target position of {} changed: {} -> {} : {} with scale{}", stdCode, oldVol, newVol, traderTarget, _scale);	// 记录持仓变化日志
		}

		if (_trader && !_trader->checkOrderLimits(stdCode))				// 如果交易适配器存在但合约被禁用
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_WARN, "{} is disabled due to entrust limit control ", stdCode);	// 记录警告日志
			continue;														// 跳过该合约
		}

		if (_pool)															// 如果配置了线程池
		{
			std::string code = stdCode;									// 复制合约代码（用于lambda捕获）
			_pool->schedule([unit, code, traderTarget](){					// 在线程池中调度任务
				unit->self()->set_position(code.c_str(), traderTarget);	// 通知执行单元更新目标持仓
			});
		}
		else																// 如果没有配置线程池
		{
			unit->self()->set_position(stdCode, traderTarget);				// 直接通知执行单元更新目标持仓
		}
	}

	// 在原来的目标头寸中，但是不在新的目标头寸中，则需要自动设置为0
	for (auto it = _target_pos.begin(); it != _target_pos.end(); it++)		// 遍历原有的目标持仓
	{
		const char* code = it->first.c_str();								// 获取合约代码
		double& pos = (double&)it->second;									// 获取目标持仓引用
		auto tit = targets.find(code);										// 查找合约是否在新的目标持仓中
		if(tit != targets.end())											// 如果合约在新的目标持仓中，跳过
			continue;

		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "{} is not in target, set to 0 automatically", code);	// 记录日志：合约不在目标持仓中，自动设置为0

		ExecuteUnitPtr unit = getUnit(code);								// 获取执行单元
		if (unit == NULL)													// 如果执行单元不存在，跳过
			continue;

		//unit->self()->set_position(code, 0);								// 原代码：直接设置（已注释）
		if (_pool)															// 如果配置了线程池
		{
			_pool->schedule([unit, code](){									// 在线程池中调度任务
				unit->self()->set_position(code, 0);						// 通知执行单元将目标持仓设置为0
			});
		}
		else																// 如果没有配置线程池
		{
			unit->self()->set_position(code, 0);							// 直接通知执行单元将目标持仓设置为0
		}

		pos = 0;															// 更新目标持仓为0
	}

	// 如果开启了严格同步，则需要检查通道持仓
	// 如果通道持仓不在管理中，则直接平掉
	if(_strict_sync)														// 如果开启了严格同步模式
	{
		for(const std::string& stdCode : _channel_holds)					// 遍历通道持仓集合
		{
			auto it = _target_pos.find(stdCode.c_str());					// 查找合约是否在目标持仓中
			if(it != _target_pos.end())									// 如果合约在目标持仓中，跳过
				continue;

			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "{} is not in management, set to 0 due to strict sync mode", stdCode.c_str());	// 记录日志：合约不在管理中，由于严格同步模式设置为0

			ExecuteUnitPtr unit = getUnit(stdCode.c_str());					// 获取执行单元
			if (unit == NULL)												// 如果执行单元不存在，跳过
				continue;

			if (_pool)														// 如果配置了线程池
			{
				std::string code = stdCode.c_str();						// 复制合约代码（用于lambda捕获）
				_pool->schedule([unit, code]() {							// 在线程池中调度任务
					unit->self()->set_position(code.c_str(), 0);			// 通知执行单元将目标持仓设置为0
				});
			}
			else															// 如果没有配置线程池
			{
				unit->self()->set_position(stdCode.c_str(), 0);				// 直接通知执行单元将目标持仓设置为0
			}
		}
	}
}

/**
 * @brief Tick数据推送回调
 * @param stdCode 标准合约代码
 * @param newTick 最新tick数据
 * 
 * 处理tick数据推送：
 * 1. 获取对应的执行单元
 * 2. 如果配置了线程池，在线程池中异步处理tick数据
 * 3. 否则直接同步处理tick数据
 * 
 * 注意：如果使用线程池，tick数据会被retain，处理完成后release。
 */
void WtArbiExecuter::on_tick(const char* stdCode, WTSTickData* newTick)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);							// 获取执行单元（不自动创建）
	if (unit == NULL)														// 如果执行单元不存在，直接返回
		return;

	//unit->self()->on_tick(newTick);										// 原代码：直接处理（已注释）
	if (_pool)																// 如果配置了线程池
	{
		newTick->retain();													// 增加tick数据引用计数（防止在线程池中使用时被释放）
		_pool->schedule([unit, newTick](){									// 在线程池中调度任务
			unit->self()->on_tick(newTick);									// 通知执行单元处理tick数据
			newTick->release();												// 减少tick数据引用计数（处理完成后释放）
		});
	}
	else																	// 如果没有配置线程池
	{
		unit->self()->on_tick(newTick);										// 直接通知执行单元处理tick数据
	}
}

/**
 * @brief 成交通知回调
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否买入（true=买入，false=卖出）
 * @param vol 成交数量
 * @param price 成交价格
 * 
 * 处理成交通知：
 * 1. 获取对应的执行单元
 * 2. 如果配置了线程池，在线程池中异步处理成交
 * 3. 否则直接同步处理成交
 */
void WtArbiExecuter::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);							// 获取执行单元（不自动创建）
	if (unit == NULL)														// 如果执行单元不存在，直接返回
		return;

	//unit->self()->on_trade(stdCode, isBuy, vol, price);					// 原代码：直接处理（已注释）
	if (_pool)																// 如果配置了线程池
	{
		std::string code = stdCode;											// 复制合约代码（用于lambda捕获）
		_pool->schedule([localid, unit, code, isBuy, vol, price](){			// 在线程池中调度任务
			unit->self()->on_trade(localid, code.c_str(), isBuy, vol, price);	// 通知执行单元处理成交
		});
	}
	else																	// 如果没有配置线程池
	{
		unit->self()->on_trade(localid, stdCode, isBuy, vol, price);		// 直接通知执行单元处理成交
	}
}

/**
 * @brief 订单状态变化回调
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否买入（true=买入，false=卖出）
 * @param totalQty 委托总数量
 * @param leftQty 剩余未成交数量
 * @param price 委托价格
 * @param isCanceled 是否已撤销（true=已撤销，false=未撤销）
 * 
 * 处理订单状态变化通知：
 * 1. 获取对应的执行单元
 * 2. 如果配置了线程池，在线程池中异步处理订单状态变化
 * 3. 否则直接同步处理订单状态变化
 */
void WtArbiExecuter::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled /* = false */)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);							// 获取执行单元（不自动创建）
	if (unit == NULL)														// 如果执行单元不存在，直接返回
		return;

	//unit->self()->on_order(localid, stdCode, isBuy, leftQty, price, isCanceled);	// 原代码：直接处理（已注释）
	if (_pool)																// 如果配置了线程池
	{
		std::string code = stdCode;											// 复制合约代码（用于lambda捕获）
		_pool->schedule([localid, unit, code, isBuy, leftQty, price, isCanceled](){	// 在线程池中调度任务
			unit->self()->on_order(localid, code.c_str(), isBuy, leftQty, price, isCanceled);	// 通知执行单元处理订单状态变化
		});
	}
	else																	// 如果没有配置线程池
	{
		unit->self()->on_order(localid, stdCode, isBuy, leftQty, price, isCanceled);	// 直接通知执行单元处理订单状态变化
	}
}

/**
 * @brief 委托响应回调
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 委托是否成功（true=成功，false=失败）
 * @param message 响应消息
 * 
 * 处理委托下单的响应通知：
 * 1. 获取对应的执行单元
 * 2. 如果配置了线程池，在线程池中异步处理委托响应
 * 3. 否则直接同步处理委托响应
 */
void WtArbiExecuter::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);							// 获取执行单元（不自动创建）
	if (unit == NULL)														// 如果执行单元不存在，直接返回
		return;

	//unit->self()->on_entrust(localid, stdCode, bSuccess, message);		// 原代码：直接处理（已注释）
	if (_pool)																// 如果配置了线程池
	{
		std::string code = stdCode;											// 复制合约代码（用于lambda捕获）
		std::string msg = message;											// 复制响应消息（用于lambda捕获）
		_pool->schedule([unit, localid, code, bSuccess, msg](){				// 在线程池中调度任务
			unit->self()->on_entrust(localid, code.c_str(), bSuccess, msg.c_str());	// 通知执行单元处理委托响应
		});
	}
	else																	// 如果没有配置线程池
	{
		unit->self()->on_entrust(localid, stdCode, bSuccess, message);		// 直接通知执行单元处理委托响应
	}
}

/**
 * @brief 交易通道就绪回调
 * 
 * 处理交易通道就绪通知：
 * 1. 设置通道就绪状态为true
 * 2. 遍历所有执行单元，通知它们通道已就绪
 * 3. 如果配置了线程池，在线程池中异步通知
 * 4. 否则直接同步通知
 */
void WtArbiExecuter::on_channel_ready()
{
	_channel_ready = true;													// 设置通道就绪状态为true
	SpinLock lock(_mtx_units);												// 加锁保护执行单元映射表
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)			// 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;				// 获取执行单元指针
		if (unitPtr)														// 如果执行单元存在
		{
			//unitPtr->self()->on_channel_ready();							// 原代码：直接通知（已注释）
			if (_pool)														// 如果配置了线程池
			{
				_pool->schedule([unitPtr](){									// 在线程池中调度任务
					unitPtr->self()->on_channel_ready();						// 通知执行单元通道已就绪
				});
			}
			else															// 如果没有配置线程池
			{
				unitPtr->self()->on_channel_ready();						// 直接通知执行单元通道已就绪
			}
		}
	}
}

/**
 * @brief 交易通道丢失回调
 * 
 * 处理交易通道丢失通知：
 * 1. 设置通道就绪状态为false
 * 2. 遍历所有执行单元，通知它们通道已丢失
 * 3. 如果配置了线程池，在线程池中异步通知
 * 4. 否则直接同步通知
 */
void WtArbiExecuter::on_channel_lost()
{
	_channel_ready = false;													// 设置通道就绪状态为false
	SpinLock lock(_mtx_units);												// 加锁保护执行单元映射表
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)			// 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;				// 获取执行单元指针
		if (unitPtr)														// 如果执行单元存在
		{
			if (_pool)														// 如果配置了线程池
			{
				_pool->schedule([unitPtr](){									// 在线程池中调度任务
					unitPtr->self()->on_channel_lost();						// 通知执行单元通道已丢失
				});
			}
			else															// 如果没有配置线程池
			{
				unitPtr->self()->on_channel_lost();							// 直接通知执行单元通道已丢失
			}
		}
	}
}

/**
 * @brief 资金账户变化回调
 * @param currency 币种
 * @param prebalance 上日余额
 * @param balance 当前余额
 * @param dynbalance 动态权益
 * @param avaliable 可用资金
 * @param closeprofit 平仓盈亏
 * @param dynprofit 浮动盈亏
 * @param margin 保证金占用
 * @param fee 手续费
 * @param deposit 入金
 * @param withdraw 出金
 * 
 * 处理资金账户变化通知：
 * 1. 遍历所有执行单元，通知它们资金账户变化
 * 2. 如果配置了线程池，在线程池中异步通知
 * 3. 否则直接同步通知
 */
void WtArbiExecuter::on_account(const char* currency, double prebalance, double balance, double dynbalance, 
	double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw)
{
	SpinLock lock(_mtx_units);												// 加锁保护执行单元映射表
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)			// 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;				// 获取执行单元指针
		if (unitPtr)														// 如果执行单元存在
		{
			if (_pool)														// 如果配置了线程池
			{
				std::string strCur = currency;								// 复制币种（用于lambda捕获）
				_pool->schedule([unitPtr, strCur, prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw]() {	// 在线程池中调度任务
					unitPtr->self()->on_account(strCur.c_str(), prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw);	// 通知执行单元资金账户变化
				});
			}
			else															// 如果没有配置线程池
			{
				unitPtr->self()->on_account(currency, prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw);	// 直接通知执行单元资金账户变化
			}
		}
	}
}

/**
 * @brief 持仓变化回调
 * @param stdCode 标准合约代码
 * @param isLong 是否多头（true=多头，false=空头）
 * @param prevol 上日持仓
 * @param preavail 上日可用持仓
 * @param newvol 今日持仓
 * @param newavail 今日可用持仓
 * @param tradingday 交易日
 * 
 * 处理持仓变化通知：
 * 1. 将合约添加到通道持仓集合中
 * 2. 如果启用了自动清理功能，检查是否需要清理上一期主力合约
 * 3. 自动清理逻辑：
 *    - 只处理分月期货合约
 *    - 检查是否为上一期主力合约
 *    - 检查是否在排除列表中
 *    - 检查是否在包含列表中（如果包含列表不为空）
 *    - 如果满足条件，通知执行单元清空所有持仓
 */
void WtArbiExecuter::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	_channel_holds.insert(stdCode);											// 将合约添加到通道持仓集合中

	/*
	 *	By Wesley @ 2021.12.14
	 *	先检查自动清理过期主力合约的标记是否为true
	 *	如果不为true，则直接退出该逻辑
	 */
	if (!_auto_clear)														// 如果未启用自动清理功能，直接返回
		return;

	// 如果不是分月期货合约，直接退出
	if (!CodeHelper::isStdMonthlyFutCode(stdCode))							// 如果不是分月期货合约，直接返回
		return;

	IHotMgr* hotMgr = _stub->get_hot_mon();									// 获取主力合约管理器
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);	// 解析标准合约代码
	// 获取上一期的主力合约
	std::string prevCode = hotMgr->getPrevRawCode(cInfo._exchg, cInfo._product, tradingday);	// 获取上一期主力合约代码

	// 如果当前合约不是上一期的主力合约，则直接退出
	if (prevCode != cInfo._code)											// 如果当前合约不是上一期主力合约，直接返回
		return;

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Prev hot contract of {}.{} on {} is {}", cInfo._exchg, cInfo._product, tradingday, prevCode);	// 记录日志：上一期主力合约

	thread_local static char fullPid[64] = { 0 };							// 线程局部静态缓冲区（用于格式化品种ID）
	fmtutil::format_to(fullPid, "{}.{}", cInfo._exchg, cInfo._product);	// 格式化品种ID（格式：交易所.品种）

	// 先检查排除列表
	// 如果在排除列表中，则直接退出
	auto it = _clear_excludes.find(fullPid);								// 查找品种是否在排除列表中
	if(it != _clear_excludes.end())										// 如果在排除列表中
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Position of {}, as prev hot contract, won't be cleared for it's in exclude list", stdCode);	// 记录日志：在排除列表中，不清理
		return;																// 直接返回，不清理
	}

	// 如果包含列表不为空，再检查是否在包含列表中
	// 如果为空，则全部清理，不再进入该逻辑
	if(!_clear_includes.empty())											// 如果包含列表不为空
	{
		it = _clear_includes.find(fullPid);									// 查找品种是否在包含列表中
		if (it == _clear_includes.end())									// 如果不在包含列表中
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Position of {}, as prev hot contract, won't be cleared for it's not in include list", stdCode);	// 记录日志：不在包含列表中，不清理
			return;															// 直接返回，不清理
		}
	}

	// 最后再进行自动清理
	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "Position of {}, as prev hot contract, will be cleared", stdCode);	// 记录日志：将清理上一期主力合约持仓
	ExecuteUnitPtr unit = getUnit(stdCode);									// 获取或创建执行单元
	if (unit)																// 如果执行单元存在
	{
		if (_pool)															// 如果配置了线程池
		{
			std::string code = stdCode;										// 复制合约代码（用于lambda捕获）
			_pool->schedule([unit, code](){									// 在线程池中调度任务
				unit->self()->clear_all_position(code.c_str());				// 通知执行单元清空所有持仓
			});
		}
		else																// 如果没有配置线程池
		{
			unit->self()->clear_all_position(stdCode);						// 直接通知执行单元清空所有持仓
		}
	}
}

#pragma endregion 外部接口
