/*!
 * \file WtExecuter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 差量执行器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件实现了WtDiffExecuter类的所有方法，提供差量执行功能。
 * 
 * 实现要点：
 * 1. 构造函数和析构函数：初始化成员变量，清理资源
 * 2. 初始化：加载配置参数，创建线程池，恢复历史仓位数据
 * 3. 数据持久化：保存和加载目标仓位、差量仓位到JSON文件
 * 4. 执行单元管理：获取或创建执行单元，根据品种配置执行策略
 * 5. ExecuteContext接口实现：为执行单元提供数据访问接口
 * 6. 交易回报处理：处理成交、订单、委托等回报，更新差量仓位
 * 7. 仓位管理：设置目标仓位，处理仓位变动，计算差量
 */
#include "WtDiffExecuter.h"  // 包含差量执行器头文件
#include "TraderAdapter.h"  // 包含交易适配器头文件
#include "WtEngine.h"  // 包含引擎头文件
#include "WtHelper.h"  // 包含辅助工具头文件

#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具（合约代码解析等）
#include "../Includes/IDataManager.h"  // 包含数据管理器接口头文件
#include "../Includes/WTSVariant.hpp"  // 包含变体类型头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件
#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件
#include "../Share/decimal.h"  // 包含小数精度工具头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志工具头文件

#include <rapidjson/document.h>  // 包含JSON解析库（文档类）
#include <rapidjson/prettywriter.h>  // 包含JSON解析库（格式化写入器）
namespace rj = rapidjson;  // 使用rapidjson命名空间别名

USING_NS_WTP;  // 使用WonderTrader命名空间


/**
 * @brief 构造函数
 * @param factory 执行器工厂指针，用于创建执行单元
 * @param name 执行器名称字符串
 * @param dataMgr 数据管理器指针，用于获取行情数据
 * @param bdMgr 基础数据管理器指针，用于获取合约信息
 * 
 * 初始化差量执行器，设置工厂指针、名称和数据管理器。
 * 初始化所有成员变量：交易通道就绪标志为false，放大倍数为1.0，交易适配器为NULL。
 */
WtDiffExecuter::WtDiffExecuter(WtExecuterFactory* factory, const char* name, IDataManager* dataMgr, IBaseDataMgr* bdMgr)
	: IExecCommand(name)  // 调用父类构造函数，设置执行器名称
	, _factory(factory)  // 设置执行器工厂指针
	, _data_mgr(dataMgr)  // 设置数据管理器指针
	, _channel_ready(false)  // 初始化交易通道就绪标志为false
	, _scale(1.0)  // 初始化放大倍数为1.0（不放大）
	, _trader(NULL)  // 初始化交易适配器指针为NULL
	, _bd_mgr(bdMgr)  // 设置基础数据管理器指针
{
}


/**
 * @brief 析构函数
 * 
 * 清理资源，等待线程池中所有任务完成。
 */
WtDiffExecuter::~WtDiffExecuter()
{
	if (_pool)  // 如果线程池存在
		_pool->wait();  // 等待线程池中所有任务完成（阻塞直到所有任务完成）
}

/**
 * @brief 设置交易适配器
 * @param adapter 交易适配器指针，用于执行交易
 * 
 * 保存交易适配器指针，读取交易适配器的就绪状态。
 */
void WtDiffExecuter::setTrader(TraderAdapter* adapter)
{
	_trader = adapter;  // 保存交易适配器指针
	//设置的时候读取一下trader的状态
	if(_trader)  // 如果交易适配器指针有效
		_channel_ready = _trader->isReady();  // 读取交易适配器的就绪状态
}

/**
 * @brief 初始化执行器
 * @param params 初始化参数，包含放大倍数、线程池大小、执行策略等配置
 * @return bool 初始化成功返回true，否则返回false
 * 
 * 解析初始化参数，设置放大倍数和线程池大小。
 * 如果配置了线程池大小，创建线程池。
 * 加载历史仓位数据（目标仓位和差量仓位）。
 */
bool WtDiffExecuter::init(WTSVariant* params)
{
	if (params == NULL)  // 如果参数为NULL
		return false;  // 返回false

	_config = params;  // 保存配置参数指针
	_config->retain();  // 增加配置参数的引用计数（防止被释放）

	_scale = params->getDouble("scale");  // 从配置参数中获取放大倍数（键名为"scale"）

	uint32_t poolsize = params->getUInt32("poolsize");  // 从配置参数中获取线程池大小（键名为"poolsize"）
	if(poolsize > 0)  // 如果线程池大小大于0
	{
		_pool.reset(new boost::threadpool::pool(poolsize));  // 创建线程池，大小为poolsize
	}

	load_data();  // 加载历史仓位数据（目标仓位和差量仓位）

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Diff executer inited, scale: {}, thread poolsize: {}", _name, _scale, poolsize);  // 记录日志：执行器初始化完成

	return true;  // 初始化成功，返回true
}

/**
 * @brief 从文件加载数据
 * 
 * 从JSON文件加载目标仓位和差量仓位，恢复系统状态。
 * 如果文件不存在或格式错误，则跳过加载。
 * 
 * 实现逻辑：
 * 1. 构建文件路径（执行器数据目录 + 执行器名称 + .json）
 * 2. 检查文件是否存在，如果不存在则返回
 * 3. 读取文件内容并解析JSON
 * 4. 如果JSON解析失败，则返回
 * 5. 加载目标仓位数据（targets数组）
 * 6. 加载差量仓位数据（diffs数组）
 * 7. 验证合约代码有效性，无效的合约代码会被跳过
 */
void WtDiffExecuter::load_data()
{
	//读取执行器的理论部位，以及待执行的差量
	std::string filename = WtHelper::getExecDataDir();  // 获取执行器数据目录路径
	filename += _name + ".json";  // 拼接文件路径（执行器名称 + .json）

	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回，不进行加载
	}

	std::string content;  // 文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;  // 直接返回，不进行解析

	rj::Document root;  // 创建JSON文档对象
	root.Parse(content.c_str());  // 解析JSON内容

	if (root.HasParseError())  // 如果JSON解析失败
		return;  // 直接返回，不进行加载

	if(root.HasMember("targets"))  // 如果JSON中包含"targets"键（目标仓位数组）
	{
		const rj::Value& jTargets = root["targets"];  // 获取目标仓位数组
		for (const rj::Value& jItem : jTargets.GetArray())  // 遍历目标仓位数组
		{
			const char* stdCode = jItem["code"].GetString();  // 获取合约代码字符串
			CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 解析标准合约代码
			WTSContractInfo* ct = _bd_mgr->getContract(cInfo._code, cInfo._exchg);  // 从基础数据管理器获取合约信息
			if (ct == NULL)  // 如果合约信息无效
			{
				WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Ticker {} is not valid", _name, stdCode);  // 记录日志：合约代码无效
				continue;  // 跳过当前合约，继续处理下一个
			}

			double pos = jItem["target"].GetDouble();  // 获取目标仓位数量
			_target_pos[stdCode] = pos;  // 更新目标仓位映射表
		}
	}

	if (root.HasMember("diffs"))  // 如果JSON中包含"diffs"键（差量仓位数组）
	{
		const rj::Value& jDiffs = root["diffs"];  // 获取差量仓位数组
		for (const rj::Value& jItem : jDiffs.GetArray())  // 遍历差量仓位数组
		{
			const char* stdCode = jItem["code"].GetString();  // 获取合约代码字符串
			CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 解析标准合约代码
			WTSContractInfo* ct = _bd_mgr->getContract(cInfo._code, cInfo._exchg);  // 从基础数据管理器获取合约信息
			if (ct == NULL)  // 如果合约信息无效
			{
				WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Ticker {} is not valid", _name, stdCode);  // 记录日志：合约代码无效
				continue;  // 跳过当前合约，继续处理下一个
			}

			double pos = jItem["diff"].GetDouble();  // 获取差量仓位数量
			_diff_pos[stdCode] = pos;  // 更新差量仓位映射表
		}
	}
}

/**
 * @brief 保存数据到文件
 * 
 * 将目标仓位和差量仓位保存到JSON文件，用于系统重启后恢复。
 * 文件保存在执行器数据目录下，文件名为执行器名称.json。
 * 
 * 实现逻辑：
 * 1. 构建文件路径（执行器数据目录 + 执行器名称 + .json）
 * 2. 创建JSON文档对象和分配器
 * 3. 遍历目标仓位映射表，构建目标仓位数组
 * 4. 遍历差量仓位映射表，构建差量仓位数组
 * 5. 将JSON文档格式化为字符串并写入文件
 */
void WtDiffExecuter::save_data()
{
	std::string filename = WtHelper::getExecDataDir();  // 获取执行器数据目录路径
	filename += _name + ".json";  // 拼接文件路径（执行器名称 + .json）

	rj::Document root(rj::kObjectType);  // 创建JSON文档对象（对象类型）
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用（用于内存管理）

	{//目标持仓数据保存
		rj::Value jTarget(rj::kArrayType);  // 创建目标仓位数组（JSON数组类型）

		for (auto& v : _target_pos)  // 遍历目标仓位映射表
		{
			rj::Value jItem(rj::kObjectType);  // 创建单个合约的JSON对象
			jItem.AddMember("code", rj::Value(v.first.c_str(), allocator), allocator);  // 添加合约代码字段
			jItem.AddMember("target", v.second, allocator);  // 添加目标仓位字段

			jTarget.PushBack(jItem, allocator);  // 将合约对象添加到数组中
		}

		root.AddMember("targets", jTarget, allocator);  // 将目标仓位数组添加到JSON根对象
	}

	{//差量持仓数据保存
		rj::Value jDiff(rj::kArrayType);  // 创建差量仓位数组（JSON数组类型）

		for (auto& v : _diff_pos)  // 遍历差量仓位映射表
		{
			rj::Value jItem(rj::kObjectType);  // 创建单个合约的JSON对象
			jItem.AddMember("code", rj::Value(v.first.c_str(), allocator), allocator);  // 添加合约代码字段
			jItem.AddMember("diff", v.second, allocator);  // 添加差量仓位字段

			jDiff.PushBack(jItem, allocator);  // 将合约对象添加到数组中
		}

		root.AddMember("diffs", jDiff, allocator);  // 将差量仓位数组添加到JSON根对象
	}

	{  // 文件写入部分
		std::string filename = WtHelper::getExecDataDir();  // 重新获取执行器数据目录路径（确保路径正确）
		filename += _name + ".json";  // 拼接文件路径（执行器名称 + .json）

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 创建新文件（如果文件已存在则覆盖）
		{
			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器（生成格式化的JSON字符串）
			root.Accept(writer);  // 将JSON文档写入缓冲区（格式化输出）
			bf.write_file(sb.GetString());  // 将JSON字符串写入文件
			bf.close_file();  // 关闭文件
		}
	}
}

/**
 * @brief 获取执行单元
 * @param stdCode 标准合约代码字符串
 * @param bAutoCreate 是否自动创建执行单元，默认为true
 * @return ExecuteUnitPtr 返回执行单元智能指针，如果不存在且不自动创建则返回空指针
 * 
 * 根据合约代码获取执行单元，如果不存在且bAutoCreate为true，则创建新的执行单元。
 * 创建时会根据合约品种查找对应的执行策略配置，如果未配置则使用默认策略。
 * 如果交易通道已就绪，会立即通知执行单元。
 * 
 * 实现逻辑：
 * 1. 解析标准合约代码，提取品种ID
 * 2. 从配置中获取执行策略配置
 * 3. 如果品种未配置策略，使用默认策略
 * 4. 查找执行单元映射表，如果存在则直接返回
 * 5. 如果不存在且bAutoCreate为true，创建新的执行单元
 * 6. 初始化执行单元，如果通道已就绪则通知执行单元
 */
ExecuteUnitPtr WtDiffExecuter::getUnit(const char* stdCode, bool bAutoCreate /* = true */)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 解析标准合约代码，提取合约代码和交易所信息
	std::string commID = codeInfo.stdCommID();  // 获取标准品种ID（例如：SHFE.rb）

	WTSVariant* policy = _config->get("policy");  // 从配置中获取执行策略配置对象
	std::string des = commID;  // 默认使用品种ID作为策略名称
	if (!policy->has(commID.c_str()))  // 如果配置中不存在该品种的策略
		des = "default";  // 使用默认策略名称

	//SpinLock lock(_mtx_units);  // 自旋锁保护（已注释，可能不需要线程安全保护）

	auto it = _unit_map.find(stdCode);  // 在执行单元映射表中查找合约代码
	if(it != _unit_map.end())  // 如果找到执行单元
	{
		return it->second;  // 直接返回找到的执行单元
	}

	if (bAutoCreate)  // 如果允许自动创建执行单元
	{
		WTSVariant* cfg = policy->get(des.c_str());  // 从策略配置中获取对应策略的配置对象

		const char* name = cfg->getCString("name");  // 从配置中获取执行单元名称（例如："TWAP"、"VWAP"等）
		ExecuteUnitPtr unit = _factory->createDiffExeUnit(name);  // 使用工厂创建差量执行单元
		if (unit != NULL)  // 如果创建成功
		{
			_unit_map[stdCode] = unit;  // 将执行单元添加到映射表中
			unit->self()->init(this, stdCode, cfg);  // 初始化执行单元（传入执行上下文、合约代码和配置）

			//如果通道已经就绪，则直接通知执行单元
			if (_channel_ready)  // 如果交易通道已就绪
				unit->self()->on_channel_ready();  // 通知执行单元通道已就绪
		}
		else  // 如果创建失败
		{
			WTSLogger::error("Creating ExecUnit {} failed", name);  // 记录错误日志：创建执行单元失败
		}
		return unit;  // 返回执行单元（可能为NULL）
	}
	else  // 如果不允许自动创建
	{
		return ExecuteUnitPtr();  // 返回空指针
	}
}


//////////////////////////////////////////////////////////////////////////
// ExecuteContext接口实现
// 以下方法实现ExecuteContext接口，为执行单元提供数据访问接口
//////////////////////////////////////////////////////////////////////////
#pragma region Context回调接口
/**
 * @brief 获取Tick数据切片
 * @param stdCode 标准合约代码字符串
 * @param count 获取的Tick数量
 * @param etime 结束时间戳，默认为0（使用最新时间）
 * @return WTSTickSlice* 返回Tick数据切片指针，如果数据管理器无效返回NULL
 * 
 * 从数据管理器获取指定数量的Tick数据。
 */
WTSTickSlice* WtDiffExecuter::getTicks(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_data_mgr == NULL)  // 如果数据管理器无效
		return NULL;  // 返回NULL

	return _data_mgr->get_tick_slice(stdCode, count);  // 从数据管理器获取Tick数据切片
}

/**
 * @brief 获取最后一个Tick数据
 * @param stdCode 标准合约代码字符串
 * @return WTSTickData* 返回最后一个Tick数据指针，如果数据管理器无效返回NULL
 * 
 * 从数据管理器获取最新的Tick数据。
 */
WTSTickData* WtDiffExecuter::grabLastTick(const char* stdCode)
{
	if (_data_mgr == NULL)  // 如果数据管理器无效
		return NULL;  // 返回NULL

	return _data_mgr->grab_last_tick(stdCode);  // 从数据管理器获取最后一个Tick数据
}

/**
 * @brief 获取持仓数量
 * @param stdCode 标准合约代码字符串
 * @param validOnly 是否只返回有效持仓，默认为true
 * @param flag 持仓标志（1=今仓，2=昨仓，3=全部），默认为3
 * @return double 返回持仓数量，如果交易适配器无效返回0.0
 * 
 * 从交易适配器获取指定合约的持仓数量。
 */
double WtDiffExecuter::getPosition(const char* stdCode, bool validOnly /* = true */, int32_t flag /* = 3 */)
{
	if (NULL == _trader)  // 如果交易适配器无效
		return 0.0;  // 返回0.0

	return _trader->getPosition(stdCode, validOnly, flag);  // 从交易适配器获取持仓数量
}

/**
 * @brief 获取未完成数量
 * @param stdCode 标准合约代码字符串
 * @return double 返回未完成数量，如果交易适配器无效返回0.0
 * 
 * 从交易适配器获取指定合约的未完成订单数量。
 */
double WtDiffExecuter::getUndoneQty(const char* stdCode)
{
	if (NULL == _trader)  // 如果交易适配器无效
		return 0.0;  // 返回0.0

	return _trader->getUndoneQty(stdCode);  // 从交易适配器获取未完成数量
}

/**
 * @brief 获取订单映射
 * @param stdCode 标准合约代码字符串
 * @return OrderMap* 返回订单映射指针，如果交易适配器无效返回NULL
 * 
 * 从交易适配器获取指定合约的所有订单。
 */
OrderMap* WtDiffExecuter::getOrders(const char* stdCode)
{
	if (NULL == _trader)  // 如果交易适配器无效
		return NULL;  // 返回NULL

	return _trader->getOrders(stdCode);  // 从交易适配器获取订单映射
}

/**
 * @brief 买入
 * @param stdCode 标准合约代码字符串
 * @param price 买入价格
 * @param qty 买入数量
 * @param bForceClose 是否强制平仓，默认为false
 * @return OrderIDs 返回订单ID列表
 * 
 * 通过交易适配器提交买入订单。
 * 如果交易通道未就绪，返回空列表。
 */
OrderIDs WtDiffExecuter::buy(const char* stdCode, double price, double qty, bool bForceClose/* = false*/)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return OrderIDs();  // 返回空订单ID列表

	return _trader->buy(stdCode, price, qty, 0, bForceClose);  // 通过交易适配器提交买入订单（第三个参数为0表示使用默认订单类型）
}

/**
 * @brief 卖出
 * @param stdCode 标准合约代码字符串
 * @param price 卖出价格
 * @param qty 卖出数量
 * @param bForceClose 是否强制平仓，默认为false
 * @return OrderIDs 返回订单ID列表
 * 
 * 通过交易适配器提交卖出订单。
 * 如果交易通道未就绪，返回空列表。
 */
OrderIDs WtDiffExecuter::sell(const char* stdCode, double price, double qty, bool bForceClose/* = false*/)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return OrderIDs();  // 返回空订单ID列表

	return _trader->sell(stdCode, price, qty, 0, bForceClose);  // 通过交易适配器提交卖出订单（第三个参数为0表示使用默认订单类型）
}

/**
 * @brief 撤单（按订单ID）
 * @param localid 本地订单ID
 * @return bool 撤单成功返回true，否则返回false
 * 
 * 通过交易适配器撤销指定订单。
 * 如果交易通道未就绪，返回false。
 */
bool WtDiffExecuter::cancel(uint32_t localid)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return false;  // 返回false

	return _trader->cancel(localid);  // 通过交易适配器撤销指定订单
}

/**
 * @brief 撤单（按合约代码和方向）
 * @param stdCode 标准合约代码字符串
 * @param isBuy 是否为买入方向
 * @param qty 撤销数量
 * @return OrderIDs 返回撤销的订单ID列表
 * 
 * 通过交易适配器撤销指定合约和方向的订单。
 * 如果交易通道未就绪，返回空列表。
 */
OrderIDs WtDiffExecuter::cancel(const char* stdCode, bool isBuy, double qty)
{
	if (!_channel_ready)  // 如果交易通道未就绪
		return OrderIDs();  // 返回空订单ID列表

	return _trader->cancel(stdCode, isBuy, qty);  // 通过交易适配器撤销指定合约和方向的订单
}

/**
 * @brief 写入日志
 * @param message 日志消息字符串
 * 
 * 记录执行器日志，日志包含执行器名称前缀。
 * 使用线程本地存储的缓冲区，避免多线程竞争。
 */
void WtDiffExecuter::writeLog(const char* message)
{
	static thread_local char szBuf[2048] = { 0 };  // 线程本地静态缓冲区（每个线程独立，避免多线程竞争）
	fmtutil::format_to(szBuf, "[{}] {}", _name.c_str(), message);  // 格式化日志消息（添加执行器名称前缀）
	WTSLogger::log_dyn_raw("executer", _name.c_str(), LL_INFO, szBuf);  // 记录动态日志（使用原始字符串，避免重复格式化）
}

/**
 * @brief 获取商品信息
 * @param stdCode 标准合约代码字符串
 * @return WTSCommodityInfo* 返回商品信息指针
 * 
 * 从执行器存根获取商品信息。
 */
WTSCommodityInfo* WtDiffExecuter::getCommodityInfo(const char* stdCode)
{
	return _stub->get_comm_info(stdCode);  // 从执行器存根获取商品信息
}

/**
 * @brief 获取交易会话信息
 * @param stdCode 标准合约代码字符串
 * @return WTSSessionInfo* 返回交易会话信息指针
 * 
 * 从执行器存根获取交易会话信息。
 */
WTSSessionInfo* WtDiffExecuter::getSessionInfo(const char* stdCode)
{
	return _stub->get_sess_info(stdCode);  // 从执行器存根获取交易会话信息
}

/**
 * @brief 获取当前时间
 * @return uint64_t 返回当前时间戳（纳秒级）
 * 
 * 从执行器存根获取当前时间。
 */
uint64_t WtDiffExecuter::getCurTime()
{
	return _stub->get_real_time();  // 从执行器存根获取实时时间戳（纳秒级）
	//return TimeUtils::makeTime(_stub->get_date(), _stub->get_raw_time() * 100000 + _stub->get_secs());  // 已废弃的时间构造方法（已注释）
}

#pragma endregion Context回调接口
// ExecuteContext接口实现
//////////////////////////////////////////////////////////////////////////


#pragma region 外部接口
/**
 * @brief 合约仓位变动通知
 * @param stdCode 标准合约代码字符串
 * @param diffPos 仓位变动数量（正数表示增加，负数表示减少）
 * 
 * 当合约仓位发生变化时被调用，更新目标仓位和差量仓位。
 * 如果差量为0，则直接返回。
 * 更新后会传递给执行单元执行。
 * 
 * 实现逻辑：
 * 1. 获取或创建执行单元
 * 2. 检查差量是否为0，如果为0则返回
 * 3. 应用放大倍数，更新目标仓位和差量仓位
 * 4. 检查交易限制，如果被限制则返回
 * 5. 将差量传递给执行单元执行（可选使用线程池）
 */
void WtDiffExecuter::on_position_changed(const char* stdCode, double diffPos)
{
	ExecuteUnitPtr unit = getUnit(stdCode, true);  // 获取或创建执行单元（自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回

	//如果差量为0，则直接返回
	if (decimal::eq(diffPos, 0))  // 如果差量等于0（使用小数精度比较）
		return;  // 直接返回，不进行更新

	diffPos = round(diffPos*_scale);  // 应用放大倍数并四舍五入（放大倍数乘以差量后取整）

	double oldVol = _target_pos[stdCode];  // 获取旧的目标仓位（如果不存在则为0）
	double& targetPos = _target_pos[stdCode];  // 获取目标仓位引用
	targetPos += diffPos;  // 更新目标仓位（累加差量）

	/*
	 *	By Sunseeeeeker @ 2023.01.10
	 *	更新差量
	*/
	double& thisDiff = _diff_pos[stdCode];  // 获取差量仓位引用
	double prevDiff = thisDiff;  // 保存旧的差量仓位
	thisDiff += diffPos;  // 更新差量仓位（累加差量）

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Target position of {} changed additonally: {} -> {}, diff postion changed: {} -> {}", _name, stdCode, oldVol, targetPos, prevDiff, thisDiff);  // 记录日志：目标仓位和差量仓位更新

	if (_trader && !_trader->checkOrderLimits(stdCode))  // 如果交易适配器存在且合约被限制（委托限制控制）
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] {} is disabled", _name, stdCode);  // 记录日志：合约被禁用
		return;  // 直接返回，不执行交易
	}

	//TODO 差量执行还要再看一下
	if (_pool)  // 如果线程池存在
	{
		std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
		_pool->schedule([unit, code, thisDiff]() {  // 将任务提交到线程池
			unit->self()->set_position(code.c_str(), thisDiff);  // 在后台线程中设置差量仓位
		});
	}
	else  // 如果线程池不存在
	{
		unit->self()->set_position(stdCode, thisDiff);  // 直接在当前线程中设置差量仓位
	}
}

/**
 * @brief 设置目标仓位
 * @param targets 目标仓位映射表，键为合约代码，值为目标持仓数量
 * 
 * 接收目标仓位映射表，计算每个合约的差量，传递给执行单元执行。
 * 如果目标仓位为0（不在映射表中），会自动将仓位设置为0。
 * 更新后会保存数据到文件。
 * 
 * 实现逻辑：
 * 1. 遍历目标仓位映射表，计算每个合约的差量
 * 2. 将差量传递给执行单元执行
 * 3. 检查原目标仓位中不在新映射表中的合约，将其设置为0
 */
void WtDiffExecuter::set_position(const wt_hashmap<std::string, double>& targets)
{
	for (auto it = targets.begin(); it != targets.end(); it++)  // 遍历目标仓位映射表
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码字符串
		double newVol = it->second;  // 获取新的目标仓位数量
		ExecuteUnitPtr unit = getUnit(stdCode);  // 获取执行单元（不自动创建，如果不存在则跳过）
		if (unit == NULL)  // 如果执行单元不存在
			continue;  // 跳过当前合约，继续处理下一个

		newVol = round(newVol*_scale);  // 应用放大倍数并四舍五入（放大倍数乘以目标仓位后取整）
		double oldVol = _target_pos[stdCode];  // 获取旧的目标仓位（如果不存在则为0）
		_target_pos[stdCode] = newVol;  // 更新目标仓位映射表
		if (decimal::eq(oldVol, newVol))  // 如果目标仓位未发生变化（使用小数精度比较）
			continue;  // 跳过当前合约，继续处理下一个

		//差量更新
		double& thisDiff = _diff_pos[stdCode];  // 获取差量仓位引用
		double prevDiff = thisDiff;  // 保存旧的差量仓位
		thisDiff += (newVol - oldVol);  // 更新差量仓位（累加目标仓位变化量）

		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Target position of {} changed: {} -> {}, diff postion changed: {} -> {}", _name, stdCode, oldVol, newVol, prevDiff, thisDiff);  // 记录日志：目标仓位和差量仓位更新

		if (_trader && !_trader->checkOrderLimits(stdCode))  // 如果交易适配器存在且合约被限制（委托限制控制）
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_WARN, "[{}] {} is disabled due to entrust limit control ", _name, stdCode);  // 记录警告日志：合约被禁用
			continue;  // 跳过当前合约，继续处理下一个
		}

		//TODO 差量执行还要再看一下
		if (_pool)  // 如果线程池存在
		{
			std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
			_pool->schedule([unit, code, thisDiff](){  // 将任务提交到线程池
				unit->self()->set_position(code.c_str(), thisDiff);  // 在后台线程中设置差量仓位
			});
		}
		else  // 如果线程池不存在
		{
			unit->self()->set_position(stdCode, thisDiff);  // 直接在当前线程中设置差量仓位
		}
	}

	//在原来的目标头寸中，但是不在新的目标头寸中，则需要自动设置为0
	for (auto it = _target_pos.begin(); it != _target_pos.end(); it++)  // 遍历原目标仓位映射表
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码字符串
		double& pos = (double&)it->second;  // 获取目标仓位引用（强制转换为非const引用）
		auto tit = targets.find(stdCode);  // 在新的目标仓位映射表中查找合约代码
		if(tit != targets.end())  // 如果在新映射表中找到
			continue;  // 跳过当前合约，继续处理下一个（已在上面处理过）

		WTSContractInfo* cInfo = _bd_mgr->getContract(stdCode);  // 从基础数据管理器获取合约信息
		if(cInfo == NULL)  // 如果合约信息无效
			continue;  // 跳过当前合约，继续处理下一个

		if(pos != 0)  // 如果目标仓位不为0（需要清零）
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] {} is not in target, set to 0 automatically", _name, stdCode);  // 记录日志：合约不在目标仓位中，自动设置为0

			ExecuteUnitPtr unit = getUnit(stdCode);  // 获取执行单元（不自动创建，如果不存在则跳过）
			if (unit == NULL)  // 如果执行单元不存在
				continue;  // 跳过当前合约，继续处理下一个

			//更新差量
			double& thisDiff = _diff_pos[stdCode];  // 获取差量仓位引用
			double prevDiff = thisDiff;  // 保存旧的差量仓位

			//WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[DiffExecuter][set_position][{}] {} is not in target, thisDiff: {}, prevDiff: {}, pos: {}, new thisDiff: {}", _name, stdCode, thisDiff, prevDiff, pos, thisDiff + pos);  // 已注释的调试日志

			thisDiff -= -pos;  // 更新差量仓位（减去负的目标仓位，相当于加上目标仓位，因为要清零）
			pos = 0;  // 将目标仓位设置为0

			if (_pool)  // 如果线程池存在
			{
				std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
				_pool->schedule([unit, code, thisDiff]() {  // 将任务提交到线程池
					unit->self()->set_position(code.c_str(), thisDiff);  // 在后台线程中设置差量仓位
				});
			}
			else  // 如果线程池不存在
			{
				unit->self()->set_position(stdCode, thisDiff);  // 直接在当前线程中设置差量仓位
			}
		}
	}

	save_data();  // 保存数据到文件（目标仓位和差量仓位）
}

/**
 * @brief 实时行情回调
 * @param stdCode 标准合约代码字符串
 * @param newTick 新的Tick数据指针
 * 
 * 当收到实时行情数据时被调用，传递给执行单元处理。
 * 如果执行单元不存在，则忽略。
 * 
 * 实现逻辑：
 * 1. 获取执行单元（不自动创建）
 * 2. 如果执行单元存在，将Tick数据传递给执行单元（可选使用线程池）
 * 3. 如果使用线程池，需要增加Tick数据的引用计数，使用后释放
 */
void WtDiffExecuter::on_tick(const char* stdCode, WTSTickData* newTick)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回，忽略行情数据

	//unit->self()->on_tick(newTick);  // 已注释的直接调用方式
	if (_pool)  // 如果线程池存在
	{
		newTick->retain();  // 增加Tick数据的引用计数（防止在后台线程中使用时被释放）
		_pool->schedule([unit, newTick](){  // 将任务提交到线程池
			unit->self()->on_tick(newTick);  // 在后台线程中处理Tick数据
			newTick->release();  // 释放Tick数据（减少引用计数）
		});
	}
	else  // 如果线程池不存在
	{
		unit->self()->on_tick(newTick);  // 直接在当前线程中处理Tick数据
	}
}

/**
 * @brief 成交回报
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码字符串
 * @param isBuy 是否为买入方向
 * @param vol 成交数量
 * @param price 成交价格
 * 
 * 当收到成交回报时被调用，更新差量仓位并传递给执行单元。
 * 如果本地订单ID为0，则忽略。
 * 
 * 实现逻辑：
 * 1. 获取执行单元（不自动创建）
 * 2. 如果本地订单ID为0，则返回
 * 3. 更新差量仓位（成交数量乘以方向：买入为正，卖出为负）
 * 4. 保存数据到文件
 * 5. 将成交回报传递给执行单元（可选使用线程池）
 */
void WtDiffExecuter::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回，忽略成交回报

	if (localid == 0)  // 如果本地订单ID为0（不是本执行器发出的订单）
		return;  // 直接返回，不更新差量仓位

	//如果localid不为0，则更新差量
	double& curDiff = _diff_pos[stdCode];  // 获取差量仓位引用
	double prevDiff = curDiff;  // 保存旧的差量仓位
	curDiff -= vol * (isBuy ? 1 : -1);  // 更新差量仓位（买入为正，卖出为负：买入减少差量，卖出增加差量）

	WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Diff of {} updated by trade: {} -> {}", _name, stdCode, prevDiff, curDiff);  // 记录日志：差量仓位更新
	save_data();  // 保存数据到文件（更新后的差量仓位）

	if (_pool)  // 如果线程池存在
	{
		std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
		_pool->schedule([localid, unit, code, isBuy, vol, price]() {  // 将任务提交到线程池
			unit->self()->on_trade(localid, code.c_str(), isBuy, vol, price);  // 在后台线程中处理成交回报
		});
	}
	else  // 如果线程池不存在
	{
		unit->self()->on_trade(localid, stdCode, isBuy, vol, price);  // 直接在当前线程中处理成交回报
		}
}

/**
 * @brief 订单回报
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码字符串
 * @param isBuy 是否为买入方向
 * @param totalQty 订单总数量
 * @param leftQty 剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤销，默认为false
 * 
 * 当收到订单回报时被调用，传递给执行单元处理。
 * 如果执行单元不存在，则忽略。
 * 
 * 实现逻辑：
 * 1. 获取执行单元（不自动创建）
 * 2. 如果执行单元存在，将订单回报传递给执行单元（可选使用线程池）
 * 注意：这里传递的是leftQty而不是totalQty，因为执行单元更关心剩余数量
 */
void WtDiffExecuter::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled /* = false */)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回，忽略订单回报

	if (_pool)  // 如果线程池存在
	{
		std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
		_pool->schedule([localid, unit, code, isBuy, leftQty, price, isCanceled](){  // 将任务提交到线程池
			unit->self()->on_order(localid, code.c_str(), isBuy, leftQty, price, isCanceled);  // 在后台线程中处理订单回报（传递剩余数量）
		});
	}
	else  // 如果线程池不存在
	{
		unit->self()->on_order(localid, stdCode, isBuy, leftQty, price, isCanceled);  // 直接在当前线程中处理订单回报（传递剩余数量）
	}
}

/**
 * @brief 委托回报
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码字符串
 * @param bSuccess 是否成功
 * @param message 消息字符串
 * 
 * 当收到委托回报时被调用，传递给执行单元处理。
 * 如果执行单元不存在，则忽略。
 * 
 * 实现逻辑：
 * 1. 获取执行单元（不自动创建）
 * 2. 如果执行单元存在，将委托回报传递给执行单元（可选使用线程池）
 * 3. 如果使用线程池，需要保存消息字符串（Lambda表达式需要值捕获）
 */
void WtDiffExecuter::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	ExecuteUnitPtr unit = getUnit(stdCode, false);  // 获取执行单元（不自动创建）
	if (unit == NULL)  // 如果执行单元不存在
		return;  // 直接返回，忽略委托回报

	if (_pool)  // 如果线程池存在
	{
		std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
		std::string msg = message;  // 保存消息字符串（Lambda表达式需要值捕获，因为message可能是临时字符串）
		_pool->schedule([unit, localid, code, bSuccess, msg](){  // 将任务提交到线程池
			unit->self()->on_entrust(localid, code.c_str(), bSuccess, msg.c_str());  // 在后台线程中处理委托回报
		});
	}
	else  // 如果线程池不存在
	{
		unit->self()->on_entrust(localid, stdCode, bSuccess, message);  // 直接在当前线程中处理委托回报
	}
}

/**
 * @brief 交易通道就绪
 * 
 * 当交易通道就绪时被调用，通知所有执行单元，并恢复所有差量仓位。
 * 
 * 实现逻辑：
 * 1. 设置通道就绪标志为true
 * 2. 通知所有执行单元通道就绪
 * 3. 恢复所有差量仓位（重新设置每个合约的差量）
 */
void WtDiffExecuter::on_channel_ready()
{
	_channel_ready = true;  // 设置通道就绪标志为true
	//SpinLock lock(_mtx_units);  // 自旋锁保护（已注释，可能不需要线程安全保护）
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)  // 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;  // 获取执行单元引用（强制转换为非const引用）
		if (unitPtr)  // 如果执行单元存在
		{
			if (_pool)  // 如果线程池存在
			{
				_pool->schedule([unitPtr](){  // 将任务提交到线程池
					unitPtr->self()->on_channel_ready();  // 在后台线程中通知执行单元通道就绪
				});
			}
			else  // 如果线程池不存在
			{
				unitPtr->self()->on_channel_ready();  // 直接在当前线程中通知执行单元通道就绪
			}
		}
	}

	for(auto& v : _diff_pos)  // 遍历所有差量仓位
	{
		const char* stdCode = v.first.c_str();  // 获取合约代码字符串
		ExecuteUnitPtr unit = getUnit(stdCode);  // 获取执行单元（不自动创建，如果不存在则跳过）
		if (unit == NULL)  // 如果执行单元不存在
			continue;  // 跳过当前合约，继续处理下一个
		double thisDiff = _diff_pos[stdCode];  // 获取差量仓位

		if (_pool)  // 如果线程池存在
		{
			std::string code = stdCode;  // 保存合约代码字符串（Lambda表达式需要值捕获）
			_pool->schedule([unit, code, thisDiff]() {  // 将任务提交到线程池
				unit->self()->set_position(code.c_str(), thisDiff);  // 在后台线程中恢复差量仓位
			});
		}
		else  // 如果线程池不存在
		{
			unit->self()->set_position(stdCode, thisDiff);  // 直接在当前线程中恢复差量仓位
		}

		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}] Diff of {} recovered to {}", _name, stdCode, thisDiff);  // 记录日志：差量仓位已恢复
	}
}

/**
 * @brief 交易通道丢失
 * 
 * 当交易通道丢失时被调用，通知所有执行单元。
 * 
 * 实现逻辑：
 * 1. 设置通道就绪标志为false
 * 2. 通知所有执行单元通道丢失
 */
void WtDiffExecuter::on_channel_lost()
{
	_channel_ready = false;  // 设置通道就绪标志为false
	//SpinLock lock(_mtx_units);  // 自旋锁保护（已注释，可能不需要线程安全保护）
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)  // 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;  // 获取执行单元引用（强制转换为非const引用）
		if (unitPtr)  // 如果执行单元存在
		{
			if (_pool)  // 如果线程池存在
			{
				_pool->schedule([unitPtr](){  // 将任务提交到线程池
					unitPtr->self()->on_channel_lost();  // 在后台线程中通知执行单元通道丢失
				});
			}
			else  // 如果线程池不存在
			{
				unitPtr->self()->on_channel_lost();  // 直接在当前线程中通知执行单元通道丢失
			}
		}
	}
}

/**
 * @brief 资金回报
 * @param currency 货币类型字符串
 * @param prebalance 上一次资金余额
 * @param balance 资金余额
 * @param dynbalance 动态资金余额
 * @param avaliable 可用资金
 * @param closeprofit 平仓盈亏
 * @param dynprofit 浮动盈亏
 * @param margin 占用保证金
 * @param fee 手续费
 * @param deposit 入金
 * @param withdraw 出金
 * 
 * 当收到资金回报时被调用，传递给所有执行单元处理。
 * 
 * 实现逻辑：
 * 1. 遍历所有执行单元
 * 2. 将资金回报传递给每个执行单元（可选使用线程池）
 * 3. 如果使用线程池，需要保存货币类型字符串（Lambda表达式需要值捕获）
 */
void WtDiffExecuter::on_account(const char* currency, double prebalance, double balance, double dynbalance,
	double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw)
{
	//SpinLock lock(_mtx_units);  // 自旋锁保护（已注释，可能不需要线程安全保护）
	for (auto it = _unit_map.begin(); it != _unit_map.end(); it++)  // 遍历所有执行单元
	{
		ExecuteUnitPtr& unitPtr = (ExecuteUnitPtr&)it->second;  // 获取执行单元引用（强制转换为非const引用）
		if (unitPtr)  // 如果执行单元存在
		{
			if (_pool)  // 如果线程池存在
			{
				std::string strCur = currency;  // 保存货币类型字符串（Lambda表达式需要值捕获，因为currency可能是临时字符串）
				_pool->schedule([unitPtr, strCur, prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw]() {  // 将任务提交到线程池
					unitPtr->self()->on_account(strCur.c_str(), prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw);  // 在后台线程中处理资金回报
				});
			}
			else  // 如果线程池不存在
			{
				unitPtr->self()->on_account(currency, prebalance, balance, dynbalance, avaliable, closeprofit, dynprofit, margin, fee, deposit, withdraw);  // 直接在当前线程中处理资金回报
			}
		}
	}
}

/**
 * @brief 持仓回报
 * @param stdCode 标准合约代码字符串
 * @param isLong 是否为多头方向
 * @param prevol 上一次持仓数量
 * @param preavail 上一次可用持仓数量
 * @param newvol 新持仓数量
 * @param newavail 新可用持仓数量
 * @param tradingday 交易日（格式：YYYYMMDD）
 * 
 * 当收到持仓回报时被调用，目前未实现具体逻辑。
 * 持仓回报通常由交易适配器直接管理，执行器不需要特别处理。
 */
void WtDiffExecuter::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	// 持仓回报暂时不需要处理
	// 持仓信息由交易适配器直接管理，执行器内部通过getPosition()接口获取
}

#pragma endregion 外部接口