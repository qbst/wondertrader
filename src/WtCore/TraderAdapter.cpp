/*!
 * \file TraderAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易适配器实现文件
 * 
 * 文件设计逻辑和作用总结：
 * ========================
 * 本文件实现了 TraderAdapter 类的所有功能，是交易系统的核心实现。
 * 
 * 主要实现内容：
 * 1. 交易适配器初始化：从配置文件加载交易模块，初始化交易API
 * 2. 交易操作实现：实现买入、卖出、开仓、平仓、撤单等交易操作
 * 3. 持仓管理：维护和更新持仓数据，处理持仓查询响应
 * 4. 订单管理：跟踪订单状态变化，处理订单推送和查询响应
 * 5. 成交处理：处理成交推送，更新持仓和统计数据
 * 6. 风控检查：实现下单频率限制、撤单频率限制、自成交检测等风控逻辑
 * 7. 数据持久化：保存交易日志、订单日志和实时数据到文件
 * 8. 交易回调处理：实现ITraderSpi接口的所有回调方法
 * 
 * 关键设计点：
 * - 使用本地订单ID管理订单，通过订单标签模式区分不同通道的订单
 * - 维护未完成订单数量，用于跟踪待成交的订单
 * - 实现智能开平仓逻辑，根据持仓情况和策略规则自动选择开仓或平仓
 * - 支持多品种的风控策略配置，可以针对不同品种设置不同的风控参数
 */
#include "EventNotifier.h"						// 事件通知器头文件
#include "WtLocalExecuter.h"					// 本地执行器头文件
#include "TraderAdapter.h"						// 交易适配器头文件
#include "ActionPolicyMgr.h"					// 动作策略管理器头文件
#include "WtHelper.h"							// WonderTrader辅助函数头文件
#include "ITrdNotifySink.h"					// 交易通知接收器接口头文件
#include "../Includes/RiskMonDefs.h"			// 风控监控定义头文件

#include <atomic>								// 原子操作头文件，用于线程安全的计数器

#include "../WTSTools/WTSLogger.h"				// 日志工具头文件

#include "../Includes/WTSError.hpp"			// 错误信息类头文件
#include "../Includes/WTSVariant.hpp"			// 变体类型头文件
#include "../Includes/WTSTradeDef.hpp"			// 交易定义头文件
#include "../Includes/WTSRiskDef.hpp"			// 风控定义头文件
#include "../Includes/WTSSessionInfo.hpp"		// 交易时段信息头文件
#include "../Includes/WTSContractInfo.hpp"		// 合约信息头文件
#include "../Includes/IBaseDataMgr.h"			// 基础数据管理器接口头文件
#include "../Includes/WTSVersion.h"				// 版本信息头文件

#include "../Share/decimal.h"					// 高精度小数运算头文件
#include "../Share/TimeUtils.hpp"				// 时间工具函数头文件
#include "../Share/CodeHelper.hpp"				// 代码辅助函数头文件

#include <exception>								// 异常处理头文件
#include <rapidjson/document.h>					// RapidJSON文档类头文件
#include <rapidjson/prettywriter.h>				// RapidJSON格式化写入器头文件

namespace rj = rapidjson;						// 使用rapidjson命名空间别名

/**
 * @brief 生成本地订单ID
 * @return 本地订单ID
 * 
 * 使用原子操作生成唯一的本地订单ID。
 * 首次调用时，基于当前时间戳初始化ID，后续通过原子递增生成唯一ID。
 * 订单ID的计算方式：基于当前时间距离年初的秒数，乘以50作为初始值。
 */
uint32_t makeLocalOrderID()
{
	static std::atomic<uint32_t> _auto_order_id{ 0 };		// 静态原子变量，用于生成唯一订单ID
	if (_auto_order_id == 0)								// 首次调用时初始化
	{
		uint32_t curYear = TimeUtils::getCurDate() / 10000 * 10000 + 101;	// 计算当前年份的第一天（YYYY0101格式）
		_auto_order_id = (uint32_t)((TimeUtils::getLocalTimeNow() - TimeUtils::makeTime(curYear, 0)) / 1000 * 50);	// 计算距离年初的秒数，乘以50作为初始ID
	}

	return _auto_order_id.fetch_add(1);						// 原子递增并返回新ID
}

/**
 * @brief 格式化交易动作字符串
 * @param dType 交易方向（多头/空头）
 * @param oType 开平仓类型（开仓/平仓/平今）
 * @return 动作字符串（OL=开多, CL=平多, CNL=平今多, OS=开空, CS=平空, CNS=平今空）
 */
inline const char* formatAction(WTSDirectionType dType, WTSOffsetType oType)
{
	if(dType == WDT_LONG)									// 如果是多头方向
	{
		if (oType == WOT_OPEN)								// 开仓
			return "OL";										// 返回"开多"
		else if (oType == WOT_CLOSE)						// 平仓
			return "CL";										// 返回"平多"
		else													// 平今
			return "CNL";										// 返回"平今多"
	}
	else														// 如果是空头方向
	{
		if (oType == WOT_OPEN)								// 开仓
			return "OS";										// 返回"开空"
		else if (oType == WOT_CLOSE)						// 平仓
			return "CS";										// 返回"平空"
		else													// 平今
			return "CNS";										// 返回"平今空"
	}
}

/**
 * @brief TraderAdapter构造函数
 * @param caster 事件通知器指针，用于发送交易事件通知，可为NULL
 * 
 * 初始化交易适配器的所有成员变量为默认值。
 */
TraderAdapter::TraderAdapter(EventNotifier* caster /* = NULL */)
	: _id("")							// 初始化交易通道ID为空字符串
	, _cfg(NULL)						// 初始化配置参数为NULL
	, _state(AS_NOTLOGIN)				// 初始化状态为未登录
	, _trader_api(NULL)					// 初始化交易API指针为NULL
	, _orders(NULL)						// 初始化订单映射表为NULL
	, _stat_map(NULL)					// 初始化统计数据映射表为NULL
	, _risk_mon_enabled(false)			// 初始化风控监控为禁用状态
	, _save_data(false)					// 初始化数据保存为禁用状态
	, _notifier(caster)					// 初始化事件通知器指针
	, _ignore_sefmatch(false)			// 初始化自成交检查为启用状态
{
}


/**
 * @brief TraderAdapter析构函数
 * 
 * 释放资源，清理统计数据映射表。
 */
TraderAdapter::~TraderAdapter()
{
	if (_stat_map)						// 如果统计数据映射表存在
		_stat_map->release();			// 释放统计数据映射表资源
}

/**
 * @brief 初始化交易适配器（从配置文件加载）
 * @param id 交易通道标识符
 * @param params 配置参数，包含交易模块路径、登录信息等
 * @param bdMgr 基础数据管理器，用于获取合约和商品信息
 * @param policyMgr 动作策略管理器，用于管理开平仓策略
 * @return 初始化是否成功
 * 
 * 初始化流程：
 * 1. 保存管理器和配置参数
 * 2. 配置订单标签模式
 * 3. 解析自成交忽略标志和数据保存标志
 * 4. 解析风控参数配置
 * 5. 加载交易模块动态库
 * 6. 创建交易API实例
 * 7. 初始化交易API
 */
bool TraderAdapter::init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr)
{
	if (params == NULL)					// 如果配置参数为空，则初始化失败
		return false;

	_policy_mgr = policyMgr;				// 保存动作策略管理器指针
	_bd_mgr = bdMgr;						// 保存基础数据管理器指针
	_id = id;								// 保存交易通道标识符

	_order_pattern = fmt::format("otp.{}", id);	// 生成订单标签模式，格式为"otp.{通道ID}"

	if (_cfg != NULL)						// 如果配置已存在，说明已经初始化过，返回失败
		return false;

	_cfg = params;							// 保存配置参数
	_cfg->retain();							// 增加配置参数的引用计数

	_ignore_sefmatch = _cfg->getBoolean("ignore_selfmatch");	// 读取是否忽略自成交标志
	_save_data = _cfg->getBoolean("savedata");					// 读取是否保存数据标志
	if (_save_data)							// 如果需要保存数据
		initSaveData();						// 初始化数据保存功能

	// 这里解析流量风控参数
	WTSVariant* cfgRisk = params->get("riskmon");		// 获取风控配置节点
	// cfgRisk 的JSON形式例子：
	/*
	{
		"active": true,
		"policy": {
			"default": {
				"cancel_total_limits": 100,
				"cancel_times_boundary": 10,
				"cancel_stat_timespan": 60,
				"order_total_limits": 100,
				"order_times_boundary": 10,
				"order_stat_timespan": 60
			}
			"SHFE.cu": {
				"cancel_total_limits": 100,
				"cancel_times_boundary": 10,
				"cancel_stat_timespan": 60,
				"order_total_limits": 100,
				"order_times_boundary": 10,
				"order_stat_timespan": 60
			}
		}
	}
	*/
	if (cfgRisk)							// 如果存在风控配置
	{
		if (cfgRisk->getBoolean("active"))	// 如果风控监控已激活
		{
			_risk_mon_enabled = true;		// 启用风控监控

			WTSVariant* cfgPolicy = cfgRisk->get("policy");	// 获取风控策略配置
			auto keys = cfgPolicy->memberNames();				// 获取所有策略键名（品种代码）
			for (auto it = keys.begin(); it != keys.end(); it++)	// 遍历所有策略
			{
				const char* product = (*it).c_str();			// 获取品种代码
				WTSVariant*	vProdItem = cfgPolicy->get(product);	// 获取该品种的风控参数配置
				RiskParams& rParam = _risk_params_map[product];	// 获取或创建该品种的风控参数对象
				rParam._cancel_total_limits = vProdItem->getUInt32("cancel_total_limits");		// 读取撤单总限额
				rParam._cancel_times_boundary = vProdItem->getUInt32("cancel_times_boundary");	// 读取撤单频率边界
				rParam._cancel_stat_timespan = vProdItem->getUInt32("cancel_stat_timespan");		// 读取撤单统计时间窗口

				rParam._order_total_limits = vProdItem->getUInt32("order_total_limits");			// 读取下单总限额
				rParam._order_times_boundary = vProdItem->getUInt32("order_times_boundary");		// 读取下单频率边界
				rParam._order_stat_timespan = vProdItem->getUInt32("order_stat_timespan");			// 读取下单统计时间窗口

				WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Risk control rule {} of trading channel loaded", _id.c_str(), product);
			}

			auto it = _risk_params_map.find("default");		// 查找默认风控参数
			if (it == _risk_params_map.end())					// 如果不存在默认参数
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] Some instruments may not be monitored due to no default risk control rule of trading channel", _id.c_str());
			}
		}
		else													// 如果风控监控未激活
		{
			WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] Risk control rule of trading channel not activated", _id.c_str());
		}
	}
	else														// 如果不存在风控配置
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] No risk control rule setup of trading channel", _id.c_str());
	}

	if (params->getString("module").empty())	// 如果交易模块名称为空，则初始化失败
		return false;

	std::string module = DLLHelper::wrap_module(params->getCString("module"), "lib");	// 包装模块名称，添加"lib"前缀

	// 先看工作目录下是否有交易模块
	std::string dllpath = WtHelper::getModulePath(module.c_str(), "traders", true);		// 获取交易模块路径（优先从工作目录查找）
	// 如果没有，则再看模块目录，即dll同目录下
	if (!StdFile::exists(dllpath.c_str()))						// 如果工作目录下不存在
		dllpath = WtHelper::getModulePath(module.c_str(), "traders", false);		// 从模块目录查找
	DllHandle hInst = DLLHelper::load_library(dllpath.c_str());	// 加载动态库
	if (hInst == NULL)											// 如果加载失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Loading trading module {} failed", _id.c_str(), dllpath.c_str());
		return false;
	}
	else														// 如果加载成功
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Trader module {} loaded", _id.c_str(), dllpath.c_str());
	}

	FuncCreateTrader pFunCreateTrader = (FuncCreateTrader)DLLHelper::get_symbol(hInst, "createTrader");	// 获取创建交易API的函数指针
	if (NULL == pFunCreateTrader)								// 如果函数不存在
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_FATAL, "[{}] Entrance function createTrader not found", _id.c_str());
		return false;
	}

	_trader_api = pFunCreateTrader();							// 调用创建函数，创建交易API实例
	if (NULL == _trader_api)									// 如果创建失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_FATAL, "[{}] Creating trading api failed", _id.c_str());
		return false;
	}

	_remover = (FuncDeleteTrader)DLLHelper::get_symbol(hInst, "deleteTrader");	// 获取删除交易API的函数指针

	if (!_trader_api->init(params))							// 初始化交易API
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Entrance function deleteTrader not found", id);
		return false;
	}

	return true;												// 初始化成功
}

/**
 * @brief 初始化交易适配器（使用外部API）
 * @param id 交易通道标识符
 * @param api 外部提供的交易API接口
 * @param bdMgr 基础数据管理器
 * @param policyMgr 动作策略管理器
 * @return 初始化是否成功
 * 
 * 使用外部提供的交易API初始化适配器，主要用于测试或特殊场景。
 * 与init()的区别是不需要加载动态库，直接使用外部API。
 */
bool TraderAdapter::initExt(const char* id, ITraderApi* api, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr)
{
	if (api == NULL)							// 如果API为空，则初始化失败
		return false;

	_policy_mgr = policyMgr;					// 保存动作策略管理器指针
	_bd_mgr = bdMgr;							// 保存基础数据管理器指针
	_id = id;									// 保存交易通道标识符

	_order_pattern = fmt::format("otp.{}", id);	// 生成订单标签模式

	if (_cfg != NULL)							// 如果配置已存在，说明已经初始化过，返回失败
		return false;

	_save_data = true;							// 强制启用数据保存功能
	if (_save_data)								// 如果需要保存数据
		initSaveData();							// 初始化数据保存功能

	_trader_api = api;							// 保存外部交易API指针
	if (!_trader_api->init(NULL))				// 初始化交易API（传入NULL表示使用默认配置）
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Trader initializing failed", id);
		return false;
	}

	return true;								// 初始化成功
}

/**
 * @brief 初始化数据保存功能
 * 
 * 创建交易日志和订单日志文件，准备保存交易数据。包括：成交日志、订单日志、实时数据文件。
 * 基础路径/traders/{交易通道ID}/trades.csv
 * 基础路径/traders/{交易通道ID}/orders.csv
 * 基础路径/traders/{交易通道ID}/rtdata.json
 */
void TraderAdapter::initSaveData()
{
	/*std::string folder = WtHelper::getOutputDir();
	folder += _name;
	folder += "//";*/
	std::stringstream ss;
	ss << WtHelper::getBaseDir() << "traders/" << _id << "//";	// 构建日志文件目录路径
	std::string folder = ss.str();
	BoostFile::create_directories(folder.c_str());					// 创建日志目录（如果不存在）
	std::string filename = folder + "trades.csv";					// 成交日志文件名
	_trades_log.reset(new BoostFile());							// 创建成交日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());		// 检查文件是否已存在
		_trades_log->create_or_open_file(filename.c_str());			// 创建或打开成交日志文件
		if (isNewFile)												// 如果是新文件
		{
			_trades_log->write_file("localid,date,time,code,action,volume,price,tradeid,orderid\n");	// 写入CSV表头
		}
		else														// 如果是已存在的文件
		{
			_trades_log->seek_to_end();							// 定位到文件末尾，追加模式
		}
	}

	filename = folder + "orders.csv";								// 订单日志文件名
	_orders_log.reset(new BoostFile());							// 创建订单日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());		// 检查文件是否已存在
		_orders_log->create_or_open_file(filename.c_str());			// 创建或打开订单日志文件
		if (isNewFile)												// 如果是新文件
		{
			_orders_log->write_file("localid,date,inserttime,code,action,volume,traded,price,orderid,canceled,remark\n");	// 写入CSV表头
		}
		else														// 如果是已存在的文件
		{
			_orders_log->seek_to_end();							// 定位到文件末尾，追加模式
		}
	}

	_rt_data_file = folder + "rtdata.json";						// 实时数据文件路径（JSON格式，包含持仓和资金信息）
}

/**
 * @brief 记录成交日志
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param trdInfo 成交信息对象
 * 
 * 将成交信息写入CSV格式的成交日志文件。
 * 日志格式：本地订单ID, 成交日期, 成交时间, 合约代码, 动作类型, 成交数量, 成交价格, 成交单号, 订单号
 */
void TraderAdapter::logTrade(uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo)
{
	if (_trades_log == NULL || trdInfo == NULL)					// 如果日志文件不存在或成交信息为空，则直接返回
		return;

	_trades_log->write_file(fmt::format("{},{},{},{},{},{},{},{},{}\n",	// 格式化成交信息并写入日志文件
		localid, trdInfo->getTradeDate(), trdInfo->getTradeTime(), stdCode,	// 本地订单ID, 成交日期, 成交时间, 合约代码
		formatAction(trdInfo->getDirection(), trdInfo->getOffsetType()),		// 动作类型（开多/平多/开空/平空等）
		trdInfo->getVolume(), trdInfo->getPrice(), trdInfo->getTradeID(), trdInfo->getRefOrder()));	// 成交数量, 成交价格, 成交单号, 关联订单号
}

/**
 * @brief 记录订单日志
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param ordInfo 订单信息对象
 * 
 * 将订单信息写入CSV格式的订单日志文件。
 * 日志格式：本地订单ID, 订单日期, 订单时间, 合约代码, 动作类型, 委托数量, 已成交数量, 委托价格, 订单号, 是否撤销, 状态消息
 */
void TraderAdapter::logOrder(uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo)
{
	if (_orders_log == NULL || ordInfo == NULL)					// 如果日志文件不存在或订单信息为空，则直接返回
		return;

	_orders_log->write_file(fmt::format("{},{},{},{},{},{},{},{},{},{},{}\n",	// 格式化订单信息并写入日志文件
		localid, ordInfo->getOrderDate(), ordInfo->getOrderTime(), stdCode,		// 本地订单ID, 订单日期, 订单时间, 合约代码
		formatAction(ordInfo->getDirection(), ordInfo->getOffsetType()),			// 动作类型（开多/平多/开空/平空等）
		ordInfo->getVolume(), ordInfo->getVolTraded(), ordInfo->getPrice(), 		// 委托数量, 已成交数量, 委托价格
		ordInfo->getOrderID(), ordInfo->getOrderState()==WOS_Canceled?"TRUE":"FALSE", ordInfo->getStateMsg()));	// 订单号, 是否撤销, 状态消息
}

/**
 * @brief 保存实时数据到文件
 * @param ayFunds 资金信息数组，可为NULL
 * 
 * 将持仓数据和资金数据保存到JSON格式的实时数据文件中。
 * JSON格式的例子：
 * ```json
 * {
 *   "positions": [
 *     {
 *       "code": "IF2512",
 *       "long": {
 *         "newvol": 100,
 *         "newavail": 100,
 *         "prevol": 0,
 *         "preavail": 0
 *       },
 *       "short": {
 *         "newvol": 0,
 *         "newavail": 0,
 *         "prevol": 0,
 *         "preavail": 0
 *       }
 *     }
 *   ],
 *   "funds": {
 *     "CNY": {
 *       "prebalance": 100000,
 *       "balance": 100000,
 *       "closeprofit": 0,
 *       "dynprofit": 0,
 *       "margin": 0,
 *       "fee": 0,
 *       "available": 100000,
 *       "deposit": 0,
 *       "withdraw": 0
 *     }
 *   }
 * }
 * 文件路径：traders/{交易通道ID}/rtdata.json
 */
void TraderAdapter::saveData(WTSArray* ayFunds /* = NULL */)
{
	rj::Document root(rj::kObjectType);							// 创建JSON根对象
	rj::Document::AllocatorType &allocator = root.GetAllocator();	// 获取JSON分配器

	{// 持仓数据保存：将持仓映射表转换为JSON数组格式
		rj::Value jPos(rj::kArrayType);						// 创建持仓数组
		/*
		//多仓数据
		double	l_newvol;
		double	l_newavail;
		double	l_prevol;
		double	l_preavail;

		//空仓数据
		double	s_newvol;
		double	s_newavail;
		double	s_prevol;
		double	s_preavail;
		 */

		for (auto it = _positions.begin(); it != _positions.end(); it++)	// 遍历所有持仓
		{
			const char* stdCode = it->first.c_str();				// 获取标准合约代码
			const PosItem& pInfo = it->second;						// 获取持仓项数据

			rj::Value pItem(rj::kObjectType);						// 创建持仓项JSON对象
			pItem.AddMember("code", rj::Value(stdCode, allocator), allocator);	// 添加合约代码字段

			rj::Value longItem(rj::kObjectType);					// 创建多头持仓JSON对象
			longItem.AddMember("newvol", pInfo.l_newvol, allocator);			// 添加多头今仓数量
			longItem.AddMember("newavail", pInfo.l_newavail, allocator);		// 添加多头今仓可用
			longItem.AddMember("prevol", pInfo.l_prevol, allocator);			// 添加多头昨仓数量
			longItem.AddMember("preavail", pInfo.l_preavail, allocator);		// 添加多头昨仓可用
			pItem.AddMember("long", longItem, allocator);			// 将多头持仓对象添加到持仓项

			rj::Value shortItem(rj::kObjectType);					// 创建空头持仓JSON对象
			shortItem.AddMember("newvol", pInfo.s_newvol, allocator);			// 添加空头今仓数量
			shortItem.AddMember("newavail", pInfo.s_newavail, allocator);		// 添加空头今仓可用
			shortItem.AddMember("prevol", pInfo.s_prevol, allocator);			// 添加空头昨仓数量
			shortItem.AddMember("preavail", pInfo.s_preavail, allocator);		// 添加空头昨仓可用
			pItem.AddMember("short", shortItem, allocator);			// 将空头持仓对象添加到持仓项

			jPos.PushBack(pItem, allocator);						// 将持仓项添加到持仓数组
		}

		root.AddMember("positions", jPos, allocator);				// 将持仓数组添加到根对象
	}

	{// 资金保存：将资金信息数组转换为JSON对象格式
		rj::Value jFunds(rj::kObjectType);						// 创建资金JSON对象

		if(ayFunds && ayFunds->size() > 0)						// 如果资金信息数组存在且不为空
		{
			for(auto it = ayFunds->begin(); it != ayFunds->end(); it++)	// 遍历所有资金账户
			{
				WTSAccountInfo* fundInfo = (WTSAccountInfo*)(*it);		// 获取资金账户信息
				rj::Value fItem(rj::kObjectType);					// 创建资金项JSON对象
				fItem.AddMember("prebalance", fundInfo->getPreBalance(), allocator);		// 添加上日余额
				fItem.AddMember("balance", fundInfo->getBalance(), allocator);				// 添加当前余额
				fItem.AddMember("closeprofit", fundInfo->getCloseProfit(), allocator);		// 添加平仓盈亏
				fItem.AddMember("dynprofit", fundInfo->getDynProfit(), allocator);			// 添加浮动盈亏
				fItem.AddMember("margin", fundInfo->getMargin(), allocator);					// 添加占用保证金
				fItem.AddMember("fee", fundInfo->getCommission(), allocator);				// 添加手续费
				fItem.AddMember("available", fundInfo->getAvailable(), allocator);			// 添加可用资金

				fItem.AddMember("deposit", fundInfo->getDeposit(), allocator);				// 添加入金
				fItem.AddMember("withdraw", fundInfo->getWithdraw(), allocator);			// 添加出金

				jFunds.AddMember(rj::Value(fundInfo->getCurrency(), allocator), fItem, allocator);	// 以货币类型为key，将资金项添加到资金对象
			}
		}

		root.AddMember("funds", jFunds, allocator);				// 将资金对象添加到根对象
	}

	{// 将JSON对象写入文件
		BoostFile bf;											// 创建文件对象
		if (bf.create_new_file(_rt_data_file.c_str()))			// 创建新文件（如果文件已存在则覆盖）
		{
			rj::StringBuffer sb;									// 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);		// 创建格式化写入器
			root.Accept(writer);									// 将JSON对象写入缓冲区
			bf.write_file(sb.GetString());						// 将格式化后的JSON字符串写入文件
			bf.close_file();										// 关闭文件
		}
	}
}

/**
 * @brief 获取指定合约的风控参数
 * @param stdCode 标准合约代码
 * @return 风控参数指针，如果该合约没有配置则返回默认参数
 * 
 * 首先查找该合约所属品种的风控参数，如果找不到则返回默认风控参数。
 */
const TraderAdapter::RiskParams* TraderAdapter::getRiskParams(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);	// 解析标准合约代码，提取品种信息
	std::string pid = cInfo.stdCommID();										// 获取标准品种ID（格式：交易所.品种）
	auto it = _risk_params_map.find(pid);										// 查找该品种的风控参数
	if (it != _risk_params_map.end())											// 如果找到该品种的风控参数
		return &it->second;														// 返回该品种的风控参数

	it = _risk_params_map.find("default");										// 查找默认风控参数
	return &it->second;															// 返回默认风控参数（如果default不存在，这里会返回空指针，实际使用中应该检查）
}

/**
 * @brief 启动交易适配器
 * @return 启动是否成功
 * 
 * 启动流程：
 * 1. 创建统计数据映射表
 * 2. 注册回调接口
 * 3. 连接交易服务器
 * 4. 设置状态为正在登录
 */
bool TraderAdapter::run()
{
	if (_trader_api == NULL)						// 如果交易API不存在，则启动失败
		return false;

	if (_stat_map == NULL)							// 如果统计数据映射表不存在
		_stat_map = TradeStatMap::create();			// 创建统计数据映射表

	_trader_api->registerSpi(this);					// 注册回调接口，接收交易服务器的回调通知

	_trader_api->connect();							// 连接交易服务器
	_state = AS_LOGINING;							// 设置状态为正在登录
	return true;									// 启动成功
}

/**
 * @brief 释放资源
 * 
 * 断开交易连接，注销回调接口，释放交易API资源。
 */
void TraderAdapter::release()
{
	if (_trader_api)							// 如果交易API存在
	{
		_trader_api->registerSpi(NULL);			// 注销回调接口
		_trader_api->release();					// 释放交易API资源
	}
}

/**
 * @brief 获取持仓数量
 * @param stdCode 标准合约代码
 * @param bValidOnly 是否只返回可用持仓（true=可用持仓，false=总持仓）
 * @param flag 持仓标志（1=多头，2=空头，3=净持仓）
 * @return 持仓数量（正数表示多头，负数表示空头）
 * 
 * 根据持仓标志计算持仓数量：
 * - flag & 1: 包含多头持仓
 * - flag & 2: 包含空头持仓（从结果中减去）
 * - flag = 3: 返回净持仓（多头-空头）
 */
double TraderAdapter::getPosition(const char* stdCode, bool bValidOnly, int32_t flag /* = 3 */)
{
	auto it = _positions.find(stdCode);			// 查找该合约的持仓
	if (it == _positions.end())					// 如果不存在，返回0
		return 0;

	double ret = 0;								// 初始化返回值为0
	const PosItem& pItem = it->second;			// 获取持仓项
	if(flag & 1)								// 如果包含多头持仓
	{
		if(bValidOnly)							// 如果只要可用持仓
			ret += (pItem.l_newavail + pItem.l_preavail);	// 累加多头可用持仓（今仓可用+昨仓可用）
		else									// 如果要总持仓
			ret += (pItem.l_newvol + pItem.l_prevol);		// 累加多头总持仓（今仓+昨仓）
	}

	if (flag & 2)								// 如果包含空头持仓
	{
		if (bValidOnly)							// 如果只要可用持仓
			ret -= (pItem.s_newavail + pItem.s_preavail);	// 减去空头可用持仓（今仓可用+昨仓可用）
		else									// 如果要总持仓
			ret -= pItem.s_newvol + pItem.s_prevol;			// 减去空头总持仓（今仓+昨仓）
	}
	return ret;									// 返回计算结果
}

/**
 * @brief 枚举所有持仓
 * @param cb 回调函数，对每个有持仓的合约调用该回调
 * 
 * 遍历所有持仓，对有持仓的合约调用回调函数。
 * 回调函数参数：标准代码, 是否多头, 昨仓数量, 昨仓可用, 今仓数量, 今仓可用
 */
void TraderAdapter::enumPosition(FuncEnumChnlPosCallBack cb)
{
	for(auto& v : _positions)										// 遍历所有持仓
	{
		const char* stdCode = v.first.c_str();						// 获取标准合约代码
		const PosItem& pItem = v.second;							// 获取持仓项
		if(decimal::gt(pItem.l_prevol + pItem.l_newvol, 0))			// 如果有多头持仓
			cb(stdCode, true, pItem.l_prevol, pItem.l_preavail, pItem.l_newvol, pItem.l_newavail);	// 回调多头持仓信息
		if (decimal::gt(pItem.s_prevol + pItem.s_newvol, 0))		// 如果有空头持仓
			cb(stdCode, false, pItem.s_prevol, pItem.s_preavail, pItem.s_newvol, pItem.s_newavail);	// 回调空头持仓信息
	}
}

/**
 * @brief 获取订单列表
 * @param stdCode 标准合约代码，空字符串表示获取所有订单
 * @return 订单映射表指针，失败返回NULL
 * 
 * 获取指定合约的订单列表，如果传入空字符串则返回所有订单。
 * 使用自旋锁保证线程安全。
 */
OrderMap* TraderAdapter::getOrders(const char* stdCode)
{
	if (_orders == NULL)						// 如果订单映射表不存在，返回NULL
		return NULL;

	bool isAll = strlen(stdCode) == 0;			// 判断是否获取所有订单（空字符串表示所有）

	SpinLock lock(_mtx_orders);					// 加锁保护订单映射表
	OrderMap* ret = OrderMap::create();			// 创建新的订单映射表用于返回
	for (auto it = _orders->begin(); it != _orders->end(); it++)	// 遍历所有订单
	{
		uint32_t localid = it->first;			// 获取本地订单ID
		WTSOrderInfo* ordInfo = (WTSOrderInfo*)it->second;	// 获取订单信息

		if (isAll || strcmp(ordInfo->getCode(), stdCode) == 0)	// 如果是获取所有订单，或者合约代码匹配
			ret->add(localid, ordInfo);			// 将订单添加到返回映射表中
	}
	return ret;									// 返回订单映射表
}

/**
 * @brief 执行委托下单
 * @param entrust 委托单对象，包含合约、价格、数量等信息
 * @return 本地订单ID，失败返回UINT_MAX
 * 
 * 执行下单操作：
 * 1. 生成委托单号
 * 2. 获取合约信息
 * 3. 生成本地订单ID
 * 4. 设置订单标签（用于标识订单来源）
 * 5. 调用交易API下单
 * 6. 记录下单时间到缓存（用于风控）
 */
uint32_t TraderAdapter::doEntrust(WTSEntrust* entrust)
{
	_trader_api->makeEntrustID(entrust->getEntrustID(), 64);	// 生成委托单号（64字节）

	WTSContractInfo* cInfo = entrust->getContractInfo();		// 获取委托单中的合约信息
	if (cInfo == NULL) cInfo = getContract(entrust->getCode());	// 如果合约信息为空，则根据代码获取

	entrust->setCode(cInfo->getCode());							// 设置合约代码（统一格式）
	entrust->setExchange(cInfo->getExchg());					// 设置交易所代码

	uint32_t localid = makeLocalOrderID();						// 生成本地订单ID
	char* usertag = entrust->getUserTag();						// 获取用户标签缓冲区
	wt_strcpy(usertag, _order_pattern.c_str(), _order_pattern.size());	// 复制订单标签模式（如"otp.channel1"）
	usertag[_order_pattern.size()] =  '.';					// 添加分隔符
	fmtutil::format_to(usertag + _order_pattern.size() + 1, "{}", localid);	// 追加本地订单ID，完整格式："otp.channel1.12345"
	
	int32_t ret = _trader_api->orderInsert(entrust);			// 调用交易API下单
	if(ret < 0)													// 如果下单失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Order placing failed: {}", _id.c_str(), ret);
		return UINT_MAX;											// 返回最大值表示失败
	}
	else														// 如果下单成功
	{
		int64_t now = TimeUtils::getLocalTimeNow();			// 获取当前时间戳
		_order_time_cache[entrust->getCode()].emplace_back(now);	// 记录下单时间到缓存（用于频率风控）
	}
	return localid;												// 返回本地订单ID
}

/**
 * @brief 更新未完成订单数量
 * @param stdCode 标准合约代码
 * @param qty 数量变化（正数表示增加，负数表示减少）
 * @param bOuput 是否输出日志
 * 
 * 更新指定合约的未完成订单数量。
 * 未完成数量用于跟踪待成交的订单，买入订单增加数量，卖出订单减少数量。
 */
void TraderAdapter::updateUndone(const char* stdCode, double qty, bool bOuput /* = false */)
{
	double& undone = _undone_qty[stdCode];						// 获取该合约的未完成数量引用
	double oldQty = undone;									// 保存旧值
	undone += qty;												// 更新未完成数量

	if (bOuput)													// 如果需要输出日志
		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] {} qty of undone order updated, {} -> {}", _id.c_str(), stdCode, oldQty, undone);
}

/**
 * @brief 根据标准代码获取合约信息
 * @param stdCode 标准合约代码
 * @return 合约信息指针，失败返回NULL
 */
WTSContractInfo* TraderAdapter::getContract(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);	// 解析标准合约代码，提取合约代码和交易所
	return _bd_mgr->getContract(cInfo._code, cInfo._exchg);					// 从基础数据管理器获取合约信息
}

/**
 * @brief 根据标准代码获取商品信息
 * @param stdCode 标准合约代码
 * @return 商品信息指针，失败返回NULL
 */
WTSCommodityInfo* TraderAdapter::getCommodify(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);	// 解析标准合约代码，提取交易所和品种
	return _bd_mgr->getCommodity(cInfo._exchg, cInfo._product);				// 从基础数据管理器获取商品信息
}

/**
 * @brief 检查撤单限制
 * @param stdCode 标准合约代码
 * @return true表示允许撤单，false表示超过限制
 * 
 * 检查指定合约的撤单频率限制：
 * 1. 检查是否在排除列表中
 * 2. 检查撤单总限额
 * 3. 检查撤单频率限制（在时间窗口内的撤单次数）
 * 如果超过限制，将该合约加入排除列表。
 */
bool TraderAdapter::checkCancelLimits(const char* stdCode)
{
	if (!_risk_mon_enabled)						// 如果风控监控未启用，则允许撤单
		return true;

	if (_exclude_codes.find(stdCode) != _exclude_codes.end())	// 如果该合约在排除列表中，则禁止撤单
		return false;

	const RiskParams* riskPara = getRiskParams(stdCode);		// 获取该合约的风控参数
	if (riskPara == NULL)										// 如果风控参数不存在，则允许撤单
		return true;

	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);	// 获取该合约的交易统计信息
	if (statInfo && riskPara->_cancel_total_limits != 0 && statInfo->total_cancels() >= riskPara->_cancel_total_limits )	// 如果撤单总次数超过限额
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} cancel {} times totaly, beyond boundary {} times, adding to excluding list",
			_id.c_str(), stdCode, statInfo->total_cancels(), riskPara->_cancel_total_limits);
		_exclude_codes.insert(stdCode);							// 将该合约加入排除列表
		return false;												// 禁止撤单
	}

	// 撤单频率检查：检查在时间窗口内的撤单次数
	auto it = _cancel_time_cache.find(stdCode);					// 查找该合约的撤单时间缓存
	if (it != _cancel_time_cache.end())							// 如果存在撤单时间缓存
	{
		TimeCacheList& cache = (TimeCacheList&)it->second;		// 获取撤单时间列表
		uint32_t cnt = cache.size();								// 获取撤单时间列表长度
		if (cnt >= riskPara->_cancel_times_boundary)				// 如果撤单次数达到频率边界值
		{
			uint64_t eTime = cache[cnt - 1];						// 获取最后一次撤单时间
			uint64_t sTime = eTime - riskPara->_cancel_stat_timespan * 1000;	// 计算时间窗口起始时间（毫秒转微秒）
			auto tit = std::lower_bound(cache.begin(), cache.end(), sTime);	// 二分查找时间窗口内的第一个撤单时间
			auto sIdx = tit - cache.begin();						// 计算起始索引
			auto times = cnt - sIdx - 1;							// 计算时间窗口内的撤单次数
			if (times > riskPara->_cancel_times_boundary)			// 如果时间窗口内的撤单次数超过频率边界值
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} cancel {} times within {} seconds, beyond boundary {} times, adding to excluding list",
					_id.c_str(), stdCode, times, riskPara->_cancel_stat_timespan, riskPara->_cancel_times_boundary);
				_exclude_codes.insert(stdCode);					// 将该合约加入排除列表
				return false;										// 禁止撤单
			}

			// 这里必须要清理一下，没有特别好的办法
			// 不然随着时间推移，vector长度会越来越长
			if(tit != cache.begin())								// 如果时间窗口起始位置不在列表开头
			{
				cache.erase(cache.begin(), tit);					// 删除时间窗口之前的历史数据，避免内存无限增长
			}
		}
	}

	return true;													// 允许撤单
}

/**
 * @brief 检查合约是否允许交易
 * @param stdCode 标准合约代码
 * @return true表示允许交易，false表示被风控禁止
 * 
 * 检查指定合约是否在风控排除列表中。
 */
bool TraderAdapter::isTradeEnabled(const char* stdCode) const
{
	if (!_risk_mon_enabled)						// 如果风控监控未启用，则允许交易
		return true;

	if (_exclude_codes.find(stdCode) != _exclude_codes.end())	// 如果该合约在排除列表中，则禁止交易
		return false;

	return true;								// 允许交易
}

/**
 * @brief 检查下单限制
 * @param stdCode 标准合约代码
 * @return true表示允许下单，false表示超过限制
 * 
 * 检查指定合约的下单频率限制：
 * 1. 检查是否在排除列表中
 * 2. 检查下单总限额
 * 3. 检查下单频率限制（在时间窗口内的下单次数）
 * 如果超过限制，将该合约加入排除列表。
 */
bool TraderAdapter::checkOrderLimits(const char* stdCode)
{
	if (!_risk_mon_enabled)						// 如果风控监控未启用，则允许下单
		return true;

	if (_exclude_codes.find(stdCode) != _exclude_codes.end())	// 如果该合约在排除列表中，则禁止下单
		return false;

	const RiskParams* riskPara = getRiskParams(stdCode);		// 获取该合约的风控参数
	if (riskPara == NULL)										// 如果风控参数不存在，则允许下单
		return true;

	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);	// 获取该合约的交易统计信息
	if (statInfo && riskPara->_order_total_limits != 0 && statInfo->total_orders() >= riskPara->_order_total_limits)	// 如果下单总次数超过限额
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} entrust {} times totally, beyond boundary {} times, adding to excluding list",
			_id.c_str(), stdCode, statInfo->total_orders(), riskPara->_order_total_limits);
		_exclude_codes.insert(stdCode);							// 将该合约加入排除列表
		return false;												// 禁止下单
	}

	// 下单频率检查：检查在时间窗口内的下单次数
	auto it = _order_time_cache.find(stdCode);					// 查找该合约的下单时间缓存
	if (it != _order_time_cache.end())							// 如果存在下单时间缓存
	{
		TimeCacheList& cache = (TimeCacheList&)it->second;		// 获取下单时间列表
		uint32_t cnt = cache.size();								// 获取下单时间列表长度
		if (cnt >= riskPara->_order_times_boundary)				// 如果下单次数达到频率边界值
		{
			uint64_t eTime = cache[cnt - 1];						// 获取最后一次下单时间
			uint64_t sTime = eTime - riskPara->_order_stat_timespan * 1000;	// 计算时间窗口起始时间（毫秒转微秒）
			auto tit = std::lower_bound(cache.begin(), cache.end(), sTime);	// 二分查找时间窗口内的第一个下单时间
			auto sIdx = tit - cache.begin();						// 计算起始索引
			auto times = cnt - sIdx - 1;							// 计算时间窗口内的下单次数
			if (times > riskPara->_order_times_boundary)			// 如果时间窗口内的下单次数超过频率边界值
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} entrust {} times within {} seconds, beyond boundary {} times, adding to excluding list",
					_id.c_str(), stdCode, times, riskPara->_order_stat_timespan, riskPara->_order_times_boundary);
				_exclude_codes.insert(stdCode);					// 将该合约加入排除列表
				return false;										// 禁止下单
			}

			// 这里必须要清理一下，没有特别好的办法
			// 不然随着时间推移，vector长度会越来越长
			if (tit != cache.begin())								// 如果时间窗口起始位置不在列表开头
			{
				cache.erase(cache.begin(), tit);					// 删除时间窗口之前的历史数据，避免内存无限增长
			}
		}
	}

	return true;													// 允许下单
}

/**
 * @brief 买入操作（智能开平）
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param flag 订单标志（用于扩展订单属性）
 * @param bForceClose 是否强制平仓（true=优先平仓，false=优先开仓）
 * @param cInfo 合约信息，可为NULL（会自动获取）
 * @return 订单ID列表
 * 
 * 根据持仓情况和策略规则，自动判断是开多还是平空。
 * 处理流程：
 * 1. 检查自成交限制
 * 2. 检查交易时段
 * 3. 获取动作策略规则
 * 4. 根据规则处理开仓、平今、平昨等操作
 * 5. 考虑单笔最大委托数量，自动拆单
 */
OrderIDs TraderAdapter::buy(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo /* = NULL */)
{
	OrderIDs ret;													// 订单ID列表
	if (qty == 0)													// 如果数量为0，直接返回空列表
		return ret;

	if (isSelfMatched(stdCode))									// 如果该合约发生过自成交，禁止交易
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,
			"[{0}] Instructions on {1} are forbidden: {1} is in self matches", _id.c_str(), stdCode);
		return ret;
	}

	if(cInfo == NULL) cInfo = getContract(stdCode);				// 如果合约信息为空，则自动获取
	WTSCommodityInfo* commInfo = cInfo->getCommInfo();			// 获取商品信息
	WTSSessionInfo* sInfo = commInfo->getSessionInfo();			// 获取交易时段信息

	if (!sInfo->isInTradingTime(WtHelper::getTime(), true))		// 如果不在交易时段，禁止交易
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,
			"[{}] Buying {} of quantity {}, {:04d} not in trading time", _id.c_str(), stdCode, qty, WtHelper::getTime());
		return ret;
	}


	WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Buying {} of quantity {}", _id.c_str(), stdCode, qty);	// 记录买入日志

	updateUndone(stdCode, qty, true);								// 更新未完成订单数量（买入增加）

	const PosItem& pItem = _positions[stdCode];					// 获取当前持仓
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);	// 获取交易统计信息
	if (statInfo == NULL)											// 如果统计信息不存在，则创建
	{
		statInfo = WTSTradeStateInfo::create(stdCode);
		_stat_map->add(stdCode, statInfo, false);
	}
	TradeStatInfo& statItem = statInfo->statInfo();				// 获取统计项

	const ActionRuleGroup& ruleGP = _policy_mgr->getActionRules(commInfo->getFullPid());	// 获取动作策略规则组

	double left = qty;												// 剩余待处理数量

	double unitQty = (price == 0.0) ? cInfo->getMaxMktVol() : cInfo->getMaxLmtVol();	// 获取单笔最大委托数量（市价或限价）
	if (decimal::eq(unitQty, 0))									// 如果没有限制，则使用最大值
		unitQty = DBL_MAX;

	for (auto it = ruleGP.begin(); it != ruleGP.end(); it++)		// 遍历动作策略规则
	{
		const ActionRule& curRule = (*it);							// 获取当前规则
		if(curRule._atype == AT_Open && !bForceClose)				// 如果是开仓规则且不强制平仓
		{
			// 先检查是否已经到了限额
			// 买入开仓，即开多仓
			double maxQty = left;									// 最大可开仓数量

			if (curRule._limit_l != 0)								// 如果设置了多头开仓限额
			{
				if (statItem.l_openvol >= curRule._limit_l)		// 如果今日多头开仓已到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} long position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit_l);
					continue;										// 跳过该规则
				}
				else
				{
					maxQty = min(maxQty, curRule._limit_l - statItem.l_openvol);	// 计算剩余可开仓数量
				}
			}

			if (curRule._limit != 0)								// 如果设置了总开仓限额
			{
				if (statItem.l_openvol + statItem.s_openvol >= curRule._limit)	// 如果今日总开仓已到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit);
					continue;										// 跳过该规则
				}
				else
				{
					maxQty = min(maxQty, curRule._limit - statItem.l_openvol - statItem.s_openvol);	// 计算剩余可开仓数量
				}
			}

			// 这里还要考虑单笔最大委托数量
			double leftQty = maxQty;								// 剩余待下单数量
			for (;;)												// 循环拆单，直到全部下单完成
			{
				double curQty = min(leftQty, unitQty);				// 当前单笔数量（不超过单笔最大限制）
				uint32_t localid = openLong(stdCode, price, curQty, flag, cInfo);	// 开多仓
				ret.emplace_back(localid);							// 添加到订单ID列表

				leftQty -= curQty;									// 减少剩余数量

				if (decimal::eq(leftQty, 0))						// 如果剩余数量为0，退出循环
					break;
			}			

			left -= maxQty;											// 减少总剩余数量

			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
				"[{}] Signal of buying {} of quantity {} triggered: Opening long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
		}
		else if(curRule._atype == AT_CloseToday)					// 如果是平今规则
		{
			double maxQty = 0;
			// 如果要区分平昨平今的品种，则只读取可平今仓即可
			// 如果不区分平昨平今的品种，则读取全部可平，因为读取可平今仓也没意义
			if (commInfo->getCoverMode() == CM_CoverToday)			// 如果支持平今
				maxQty = min(left, pItem.s_newavail);				// 先看看可平今仓
			else													// 如果不支持平今
				maxQty = min(left, pItem.avail_pos(false));		// 读取全部可平空仓
			
			
			// 如果要检查净今仓，但是昨仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.s_prevol, 0.0))	// 如果要求净今仓且昨仓不为0
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing new short position of {} skipped because of non-zero pre short position", _id.c_str(), stdCode);
				continue;											// 跳过该规则
			}

			// 这里还要考虑单笔最大委托数量
			if (decimal::gt(maxQty, 0))								// 如果有可平数量
			{
				double leftQty = maxQty;							// 剩余待平仓数量
				for (;;)											// 循环拆单
				{
					double curQty = min(leftQty, unitQty);			// 当前单笔数量
					uint32_t localid = closeShort(stdCode, price, curQty, (commInfo->getCoverMode() == CM_CoverToday), flag, cInfo);	// 平空仓（如果不支持平今，则直接下平仓标记）
					ret.emplace_back(localid);						// 添加到订单ID列表

					leftQty -= curQty;								// 减少剩余数量

					if (decimal::eq(leftQty, 0))					// 如果剩余数量为0，退出循环
						break;
				}
				left -= maxQty;										// 减少总剩余数量

				if (commInfo->getCoverMode() == CM_CoverToday)		// 如果支持平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
						"[{}] Signal of buying {} of quantity {} triggered: Closing new short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
				else												// 如果不支持平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
						"[{}] Signal of buying {} of quantity {} triggered: Closing short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
			}
			
		}
		else if (curRule._atype == AT_CloseYestoday)
		{
			//平昨比较简单, 因为不需要区分标记
			double maxQty = min(left, pItem.s_preavail);

			//如果要检查净昨仓，但是今仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.s_newvol, 0.0))
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing pre short position of {} skipped because of non-zero new short position", _id.c_str(), stdCode);
				continue;
			}

			//这里还要考虑单笔最大委托数量
			//if (maxQty > 0)
			if (decimal::gt(maxQty, 0))
			{
				double leftQty = maxQty;
				for (;;)
				{
					double curQty = min(leftQty, unitQty);
					uint32_t localid = closeShort(stdCode, price, curQty, false, flag, cInfo);//如果不支持平今, 则直接下平仓标记即可
					ret.emplace_back(localid);

					leftQty -= curQty;

					//if (leftQty == 0)
					if (decimal::eq(leftQty, 0))
						break;
				}

				left -= maxQty;

				if (commInfo->getCoverMode() == CM_CoverToday)
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
						"[{}] Signal of buying {} of quantity {} triggered: Closing old short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
				else
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of buying {} of quantity {} triggered: Closing short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
			}
		}
		else if (curRule._atype == AT_Close)
		{
			//如果只是平仓, 则分情况处理
			//如果区分平昨平今, 则要先平昨再平今
			//如果不区分平昨平今, 则统一平仓
			if (commInfo->getCoverMode() != CM_CoverToday)
			{
				double maxQty = min(pItem.avail_pos(false), left);
				
				//if (maxQty > 0)
				if (decimal::gt(maxQty, 0))
				{
					double leftQty = maxQty;
					for (;;)
					{
						double curQty = min(leftQty, unitQty);
						uint32_t localid = closeShort(stdCode, price, curQty, false, flag, cInfo);
						ret.emplace_back(localid);

						leftQty -= curQty;

						//if (leftQty == 0)
						if (decimal::eq(leftQty, 0))
							break;
					}
					left -= maxQty;

					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of buying {} of quantity {} triggered: Closing short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
			}
			else
			{
				//if (pItem.s_preavail > 0)
				if (decimal::gt(pItem.s_preavail, 0))
				{
					//先将可平昨仓平仓
					double maxQty = min(pItem.s_preavail, qty);
					double leftQty = maxQty;
					for (;;)
					{
						double curQty = min(leftQty, unitQty);
						uint32_t localid = closeShort(stdCode, price, curQty, false, flag, cInfo);
						ret.emplace_back(localid);

						leftQty -= curQty;

						//if (leftQty == 0)
						if (decimal::eq(leftQty, 0))
							break;
					}
					left -= maxQty;

					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of buying {} of quantity {} triggered: Closing old short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}

				//if (left > 0 && pItem.s_newavail > 0)
				if (decimal::gt(left, 0) && decimal::gt(pItem.s_newavail, 0))
				{
					//再将可平今仓平仓
					//TODO: 这里还有一个控制, 就是强制锁今仓的话, 这段逻辑就跳过去了
					double maxQty = min(pItem.s_newavail, left);
					double leftQty = maxQty;
					for (;;)
					{
						double curQty = min(leftQty, unitQty);
						uint32_t localid = closeShort(stdCode, price, curQty, true, flag, cInfo);
						ret.emplace_back(localid);

						leftQty -= curQty;

						//if (leftQty == 0)
						if (decimal::eq(leftQty, 0))
							break;
					}
					left -= maxQty;

					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of buying {} of quantity {} triggered: Closing new short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
			}
		}

		if(left == 0)
			break;
	}

	if(left > 0)
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, 
			"[{}] Signal of buying {} of quantity {} left quantity of {} not triggered", _id.c_str(), stdCode, qty, left);
	}

	return ret;
}

/**
 * @brief 卖出操作（智能开平）
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param flag 订单标志（用于扩展订单属性）
 * @param bForceClose 是否强制平仓（true=优先平仓，false=优先开仓）
 * @param cInfo 合约信息，可为NULL（会自动获取）
 * @return 订单ID列表
 * 
 * 根据持仓情况和策略规则，自动判断是开空还是平多。
 * 处理流程与buy()类似，但方向相反：
 * 1. 检查自成交限制
 * 2. 检查交易时段
 * 3. 获取动作策略规则
 * 4. 根据规则处理开仓、平今、平昨等操作
 * 5. 考虑单笔最大委托数量，自动拆单
 */
OrderIDs TraderAdapter::sell(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo /* = NULL */)
{
	OrderIDs ret;													// 订单ID列表
	if (qty == 0)													// 如果数量为0，直接返回空列表
		return ret;

	if(isSelfMatched(stdCode))									// 如果该合约发生过自成交，禁止交易
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,
			"[{0}] Instructions on {1} are forbidden: {1} is in self matches", _id.c_str(), stdCode);
		return ret;
	}

	if (cInfo == NULL) cInfo = getContract(stdCode);				// 如果合约信息为空，则自动获取
	WTSCommodityInfo* commInfo = cInfo->getCommInfo();			// 获取商品信息
	WTSSessionInfo* sInfo = commInfo->getSessionInfo();			// 获取交易时段信息

	if(!sInfo->isInTradingTime(WtHelper::getTime(), true))		// 如果不在交易时段，禁止交易
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, 
			"[{}] Selling {} of quantity {}, {:04d} not in trading time", _id.c_str(), stdCode, qty, WtHelper::getTime());
		return ret;
	}

	WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Selling {} of quantity {}", _id.c_str(), stdCode, qty);	// 记录卖出日志

	updateUndone(stdCode, -qty, true);								// 更新未完成订单数量（卖出减少）

	const PosItem& pItem = _positions[stdCode];					// 获取当前持仓
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);	// 获取交易统计信息
	if (statInfo == NULL)											// 如果统计信息不存在，则创建
	{
		statInfo = WTSTradeStateInfo::create(stdCode);
		_stat_map->add(stdCode, statInfo, false);
	}
	TradeStatInfo& statItem = statInfo->statInfo();				// 获取统计项

	const ActionRuleGroup& ruleGP = _policy_mgr->getActionRules(commInfo->getFullPid());	// 获取动作策略规则组

	double left = qty;												// 剩余待处理数量

	double unitQty = (price == 0.0) ? cInfo->getMaxMktVol() : cInfo->getMaxLmtVol();	// 获取单笔最大委托数量（市价或限价）
	if (decimal::eq(unitQty, 0))									// 如果没有限制，则使用最大值
		unitQty = DBL_MAX;

	for (auto it = ruleGP.begin(); it != ruleGP.end(); it++)		// 遍历动作策略规则
	{
		const ActionRule& curRule = (*it);							// 获取当前规则
		if (curRule._atype == AT_Open && !bForceClose)				// 如果是开仓规则且不强制平仓
		{
			// 先检查是否已经到了限额
			// 卖出开仓，即开空仓
			double maxQty = left;									// 最大可开仓数量

			if (curRule._limit_s != 0)								// 如果设置了空头开仓限额
			{
				if (statItem.s_openvol >= curRule._limit_s)		// 如果今日空头开仓已到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} short position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit_l);
					continue;										// 跳过该规则
				}
				else
				{
					maxQty = min(maxQty, curRule._limit_s - statItem.s_openvol);	// 计算剩余可开仓数量
				}
			}

			if (curRule._limit != 0)								// 如果设置了总开仓限额
			{
				if (statItem.l_openvol + statItem.s_openvol >= curRule._limit)	// 如果今日总开仓已到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit);
					continue;										// 跳过该规则
				}
				else
				{
					maxQty = min(maxQty, curRule._limit - statItem.l_openvol - statItem.s_openvol);	// 计算剩余可开仓数量
				}
			}

			// 这里还要考虑单笔最大委托数量
			double leftQty = maxQty;								// 剩余待下单数量
			for (;;)												// 循环拆单，直到全部下单完成
			{
				double curQty = min(leftQty, unitQty);				// 当前单笔数量（不超过单笔最大限制）
				uint32_t localid = openShort(stdCode, price, curQty, flag, cInfo);	// 开空仓
				ret.emplace_back(localid);							// 添加到订单ID列表

				leftQty -= curQty;									// 减少剩余数量

				if (decimal::eq(leftQty, 0))						// 如果剩余数量为0，退出循环
					break;
			}

			left -= maxQty;											// 减少总剩余数量

			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
				"[{}] Signal of selling {} of quantity {} triggered: Opening short of quantity {}", _id.c_str(), stdCode, qty, maxQty);
		}
		else if (curRule._atype == AT_CloseToday)					// 如果是平今规则
		{
			double maxQty = 0;
			// 如果要区分平昨平今的品种，则只读取可平今仓即可
			// 如果不区分平昨平今的品种，则读取全部可平，因为读取可平今仓也没意义
			if (commInfo->getCoverMode() == CM_CoverToday)			// 如果支持平今
				maxQty = min(left, pItem.l_newavail);				// 先看看可平今仓
			else													// 如果不支持平今
				maxQty = min(left, pItem.avail_pos(true));			// 读取全部可平多仓

			// 如果要检查净今仓，但是昨仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.l_prevol, 0.0))	// 如果要求净今仓且昨仓不为0
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing new long position of {} skipped because of non-zero pre long position", _id.c_str(), stdCode);
				continue;											// 跳过该规则
			}

			// 这里还要考虑单笔最大委托数量
			if(decimal::gt(maxQty, 0))								// 如果有可平数量
			{
				double leftQty = maxQty;							// 剩余待平仓数量
				for (;;)											// 循环拆单
				{
					double curQty = min(leftQty, unitQty);			// 当前单笔数量
					uint32_t localid = closeLong(stdCode, price, curQty, (commInfo->getCoverMode() == CM_CoverToday), flag, cInfo);	// 平多仓（如果不支持平今，则直接下平仓标记）
					ret.emplace_back(localid);						// 添加到订单ID列表

					leftQty -= curQty;								// 减少剩余数量

					if (decimal::eq(leftQty, 0))					// 如果剩余数量为0，退出循环
						break;
				}
				left -= maxQty;										// 减少总剩余数量

				if (commInfo->getCoverMode() == CM_CoverToday)		// 如果支持平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of selling {} of quantity {} triggered: Closing new long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
				else												// 如果不支持平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of selling {} of quantity {} triggered: Closing long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
			}

		}
		else if (curRule._atype == AT_CloseYestoday)
		{
			//平昨比较简单, 因为不需要区分标记
			double maxQty = min(left, pItem.l_preavail);
			
			//如果要检查净昨仓，但是今仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.l_newvol, 0.0))
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing pre long position of {} skipped because of non-zero new long position", _id.c_str(), stdCode);
				continue;
			}

			//这里还要考虑单笔最大委托数量
			if(decimal::gt(maxQty, 0))
			{
				double leftQty = maxQty;
				for (;;)
				{
					double curQty = min(leftQty, unitQty);
					uint32_t localid = closeLong(stdCode, price, curQty, false, flag, cInfo);//如果不支持平今, 则直接下平仓标记即可
					ret.emplace_back(localid);

					leftQty -= curQty;

					if (decimal::eq(leftQty, 0))
						break;
				}
				left -= maxQty;

				if (commInfo->getCoverMode() == CM_CoverToday)
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of selling {} of quantity {} triggered: Closing old long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
				else
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of selling {} of quantity {} triggered: Closing long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
			}
		}
		else if (curRule._atype == AT_Close)
		{
			//如果只是平仓, 则分情况处理
			//如果区分平昨平今, 则要先平昨再平今
			//如果不区分平昨平今, 则统一平仓
			if (commInfo->getCoverMode() != CM_CoverToday)
			{
				double maxQty = min(pItem.avail_pos(true), left);	//不区分平昨平今, 则读取全部可平量
				if(decimal::gt(maxQty, 0))
				{
					double leftQty = maxQty;
					for (;;)
					{
						double curQty = min(leftQty, unitQty);
						uint32_t localid = closeLong(stdCode, price, curQty, false, flag, cInfo);
						ret.emplace_back(localid);

						leftQty -= curQty;

						if (decimal::eq(leftQty, 0))
							break;
					}
					left -= maxQty;

					WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
						"[{}] Signal of selling {} of quantity {} triggered: Closing long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
				}
				
			}
			else
			{
				if (decimal::gt(left, 0) && decimal::gt(pItem.l_preavail, 0))
				{
					//先将可平昨仓平仓
					double maxQty = min(pItem.l_preavail, qty);
					if(decimal::gt(maxQty, 0))
					{
						double leftQty = maxQty;
						for (;;)
						{
							double curQty = min(leftQty, unitQty);
							uint32_t localid = closeLong(stdCode, price, curQty, false, flag, cInfo);
							ret.emplace_back(localid);

							leftQty -= curQty;

							if (decimal::eq(leftQty, 0))
								break;
						}
						left -= maxQty;

						WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
							"[{}] Signal of selling {} of quantity {} triggered: Closing old long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
					}
					
				}

				if (decimal::gt(left, 0) && decimal::gt(pItem.l_newavail, 0))
				{
					//再将可平今仓平仓
					//TODO: 这里还有一个控制, 就是强制锁今仓的话, 这段逻辑就跳过去了
					double maxQty = min(pItem.l_newavail, left);
					if(decimal::gt(maxQty, 0))
					{
						double leftQty = maxQty;
						for (;;)
						{
							double curQty = min(leftQty, unitQty);
							uint32_t localid = closeLong(stdCode, price, curQty, true, flag, cInfo);
							ret.emplace_back(localid);

							leftQty -= curQty;

							if (decimal::eq(leftQty, 0))
								break;
						}
						left -= maxQty;

						WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
							"[{}] Signal of selling {} of quantity {} triggered: Closing new long of quantity {}", _id.c_str(), stdCode, qty, maxQty);
					}
					
				}
			}
		}

		if (left == 0)
			break;
	}

	if (left > 0)
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, 
			"[{}] Signal of buying {} of quantity {} left quantity of {} not triggered", _id.c_str(), stdCode, qty, left);
	}

	return ret;
}

/**
 * @brief 执行撤单操作
 * @param ordInfo 订单信息对象
 * @return true表示撤单请求已发送，false表示失败
 * 
 * 执行撤单操作：
 * 1. 检查订单是否有效
 * 2. 检查撤单频率限制
 * 3. 调用交易API撤单
 */
bool TraderAdapter::doCancel(WTSOrderInfo* ordInfo)
{
	if (ordInfo == NULL || !ordInfo->isAlive())					// 如果订单不存在或已结束，则不能撤单
		return false;

	WTSContractInfo* cInfo = ordInfo->getContractInfo();		// 获取合约信息
	WTSCommodityInfo* commInfo = cInfo->getCommInfo();			// 获取商品信息
	std::string stdCode;
	if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
		stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
	else if (CodeHelper::isMonthlyCode(cInfo->getCode()))		// 如果是分月合约
		stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
	else														// 普通合约
		stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码
	// 撤单频率检查
	if (!checkCancelLimits(stdCode.c_str()))					// 如果超过撤单频率限制，则禁止撤单
		return false;

	WTSEntrustAction* action = WTSEntrustAction::create(ordInfo->getCode(), cInfo->getExchg());	// 创建撤单动作对象
	action->setEntrustID(ordInfo->getEntrustID());				// 设置委托单号
	action->setOrderID(ordInfo->getOrderID());					// 设置订单号
	int ret = _trader_api->orderAction(action);					// 调用交易API撤单
	bool isSent = (ret >= 0);									// 判断是否发送成功（返回值>=0表示成功）
	action->release();											// 释放动作对象
	return isSent;												// 返回是否发送成功
}

/**
 * @brief 根据本地订单ID撤单
 * @param localid 本地订单ID
 * @return true表示撤单请求已发送，false表示失败
 * 
 * 根据本地订单ID查找订单并执行撤单操作。
 */
bool TraderAdapter::cancel(uint32_t localid)
{
	if (_orders == NULL || _orders->size() == 0)				// 如果订单映射表不存在或为空，返回失败
		return false;

	WTSOrderInfo* ordInfo = NULL;
	{
		SpinLock lock(_mtx_orders);								// 加锁保护订单映射表
		ordInfo = (WTSOrderInfo*)_orders->grab(localid);		// 从订单映射表中获取订单（grab会增加引用计数）
		if (ordInfo == NULL)									// 如果订单不存在，返回失败
			return false;
	}
	
	bool bRet = doCancel(ordInfo);								// 执行撤单操作

	_cancel_time_cache[ordInfo->getCode()].emplace_back(TimeUtils::getLocalTimeNow());	// 记录撤单时间到缓存（用于频率风控）

	ordInfo->release();											// 释放订单对象（减少引用计数）

	return bRet;													// 返回撤单结果
}

/**
 * @brief 批量撤单（按合约和方向）
 * @param stdCode 标准合约代码，空字符串表示撤所有合约的订单
 * @param isBuy 是否买入方向（true=撤买单，false=撤卖单）
 * @param qty 撤单数量，0表示撤所有符合条件的订单
 * @return 已撤单的本地订单ID列表
 * 
 * 根据合约代码和方向批量撤单：
 * 1. 遍历所有订单
 * 2. 筛选符合条件的订单（合约匹配、方向匹配、订单有效）
 * 3. 执行撤单操作
 * 4. 如果指定了数量，达到数量后停止
 */
OrderIDs TraderAdapter::cancel(const char* stdCode, bool isBuy, double qty /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);	// 解析标准合约代码

	OrderIDs ret;													// 已撤单的订单ID列表

	double actQty = 0;												// 已撤单的数量
	bool isAll = strlen(stdCode) == 0;								// 是否撤所有合约的订单（空字符串表示所有）
	if (_orders != NULL && _orders->size() > 0)					// 如果订单映射表存在且不为空
	{
		for (auto it = _orders->begin(); it != _orders->end(); it++)	// 遍历所有订单
		{
			WTSOrderInfo* orderInfo = (WTSOrderInfo*)it->second;	// 获取订单信息
			if(!orderInfo->isAlive())								// 如果订单已结束，跳过
				continue;

			// 判断订单方向：买单=(多头且开仓)或(空头且平仓)，卖单=(空头且开仓)或(多头且平仓)
			bool bBuy = (orderInfo->getDirection() == WDT_LONG && orderInfo->getOffsetType() == WOT_OPEN) || (orderInfo->getDirection() == WDT_SHORT && orderInfo->getOffsetType() != WOT_OPEN);
			if(bBuy != isBuy)										// 如果方向不匹配，跳过
				continue;

			if (isAll || strcmp(orderInfo->getCode(), cInfo._code) == 0)	// 如果是撤所有合约，或者合约代码匹配
			{
				if(doCancel(orderInfo))								// 执行撤单操作
				{
					actQty += orderInfo->getVolLeft();				// 累加已撤单数量（剩余未成交数量）
					ret.emplace_back(it->first);						// 添加到结果列表
					//_cancel_time_cache[orderInfo->getCode()].emplace_back(TimeUtils::getLocalTimeNow());
				}
			}

			if (!decimal::eq(qty, 0) && decimal::ge(actQty, qty))	// 如果指定了撤单数量且已达到，退出循环
				break;
		}
	}

	return ret;														// 返回已撤单的订单ID列表
}

/**
 * @brief 开多仓
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param flag 订单标志（用于扩展订单属性）
 * @param cInfo 合约信息，可为NULL（会自动获取）
 * @return 本地订单ID，失败返回UINT_MAX
 * 
 * 创建开多仓委托单并提交。
 */
uint32_t TraderAdapter::openLong(const char* stdCode, double price, double qty, int flag, WTSContractInfo* cInfo /* = NULL */)
{
	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);	// 创建委托单对象
	entrust->setContractInfo(cInfo == NULL ? getContract(stdCode) : cInfo);	// 设置合约信息
	if(decimal::eq(price, 0.0))									// 如果价格为0，表示市价
		entrust->setPriceType(WPT_ANYPRICE);						// 设置为市价单
	else															// 否则为限价
		entrust->setPriceType(WPT_LIMITPRICE);					// 设置为限价单
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));		// 设置订单标志

	entrust->setDirection(WDT_LONG);								// 设置方向为多头
	entrust->setOffsetType(WOT_OPEN);								// 设置开平仓标记为开仓

	uint32_t ret = doEntrust(entrust);								// 执行委托下单
	entrust->release();												// 释放委托单对象
	return ret;														// 返回本地订单ID
}

/**
 * @brief 开空仓
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param flag 订单标志（用于扩展订单属性）
 * @param cInfo 合约信息，可为NULL（会自动获取）
 * @return 本地订单ID，失败返回UINT_MAX
 * 
 * 创建开空仓委托单并提交。
 */
uint32_t TraderAdapter::openShort(const char* stdCode, double price, double qty, int flag, WTSContractInfo* cInfo /* = NULL */)
{
	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);	// 创建委托单对象
	entrust->setContractInfo(cInfo == NULL ? getContract(stdCode) : cInfo);	// 设置合约信息
	if (decimal::eq(price, 0.0))									// 如果价格为0，表示市价
		entrust->setPriceType(WPT_ANYPRICE);						// 设置为市价单
	else															// 否则为限价
		entrust->setPriceType(WPT_LIMITPRICE);					// 设置为限价单
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));		// 设置订单标志

	entrust->setDirection(WDT_SHORT);								// 设置方向为空头
	entrust->setOffsetType(WOT_OPEN);								// 设置开平仓标记为开仓

	uint32_t ret = doEntrust(entrust);								// 执行委托下单
	entrust->release();												// 释放委托单对象
	return ret;														// 返回本地订单ID
}

/**
 * @brief 平多仓
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param isToday 是否平今仓（true=平今仓，false=平昨仓）
 * @param flag 订单标志（用于扩展订单属性）
 * @param cInfo 合约信息，可为NULL（会自动获取）
 * @return 本地订单ID，失败返回UINT_MAX
 * 
 * 创建平多仓委托单并提交。
 */
uint32_t TraderAdapter::closeLong(const char* stdCode, double price, double qty, bool isToday, int flag, WTSContractInfo* cInfo /* = NULL */)
{
	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);	// 创建委托单对象
	entrust->setContractInfo(cInfo == NULL ? getContract(stdCode) : cInfo);	// 设置合约信息
	if (decimal::eq(price, 0.0))									// 如果价格为0，表示市价
		entrust->setPriceType(WPT_ANYPRICE);						// 设置为市价单
	else															// 否则为限价
		entrust->setPriceType(WPT_LIMITPRICE);					// 设置为限价单
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));		// 设置订单标志

	entrust->setDirection(WDT_LONG);								// 设置方向为多头
	entrust->setOffsetType(isToday ? WOT_CLOSETODAY : WOT_CLOSE);	// 设置开平仓标记（平今或平昨）

	uint32_t ret = doEntrust(entrust);								// 执行委托下单
	entrust->release();												// 释放委托单对象
	return ret;														// 返回本地订单ID
}

/**
 * @brief 平空仓
 * @param stdCode 标准合约代码
 * @param price 委托价格，0表示市价
 * @param qty 委托数量
 * @param isToday 是否平今仓（true=平今仓，false=平昨仓）
 * @param flag 订单标志（用于扩展订单属性）
 * @param cInfo 合约信息，可为NULL（会自动获取）
 * @return 本地订单ID，失败返回UINT_MAX
 * 
 * 创建平空仓委托单并提交。
 */
uint32_t TraderAdapter::closeShort(const char* stdCode, double price, double qty, bool isToday, int flag, WTSContractInfo* cInfo /* = NULL */)
{
	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);	// 创建委托单对象
	entrust->setContractInfo(cInfo == NULL ? getContract(stdCode) : cInfo);	// 设置合约信息
	if (decimal::eq(price, 0.0))									// 如果价格为0，表示市价
		entrust->setPriceType(WPT_ANYPRICE);						// 设置为市价单
	else															// 否则为限价
		entrust->setPriceType(WPT_LIMITPRICE);					// 设置为限价单
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));		// 设置订单标志

	entrust->setDirection(WDT_SHORT);								// 设置方向为空头
	entrust->setOffsetType(isToday ? WOT_CLOSETODAY : WOT_CLOSE);	// 设置开平仓标记（平今或平昨）

	uint32_t ret = doEntrust(entrust);								// 执行委托下单
	entrust->release();												// 释放委托单对象
	return ret;														// 返回本地订单ID
}


#pragma region "ITraderSpi接口"
/**
 * @brief 处理交易事件
 * @param e 交易事件类型
 * @param ec 错误代码
 * 
 * 处理交易通道的连接和断开事件：
 * - WTE_Connect: 连接成功时自动登录，连接失败时记录错误
 * - WTE_Close: 连接断开时通知所有监听器
 */
void TraderAdapter::handleEvent(WTSTraderEvent e, int32_t ec)
{
	if(e == WTE_Connect)											// 如果是连接事件
	{
		if(ec == 0)													// 如果连接成功
		{
			_trader_api->login(_cfg->getCString("user"), _cfg->getCString("pass"), WT_PRODUCT);	// 自动登录
		}
		else														// 如果连接失败
		{
			WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Trading channel connecting failed: {}", _id.c_str(), ec);
		}
	}
	else if(e == WTE_Close)											// 如果是断开事件
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Trading channel disconnected: {}", _id.c_str(), ec);
		for (auto sink : _sinks)									// 通知所有监听器
			sink->on_channel_lost();								// 调用通道丢失回调
	}
}

/**
 * @brief 登录结果回调
 * @param bSucc 是否登录成功
 * @param msg 登录消息（成功或失败原因）
 * @param tradingdate 交易日期
 * 
 * 处理登录结果：
 * - 登录成功：设置状态为已登录，保存交易日期，查询持仓
 * - 登录失败：设置状态为登录失败，记录错误日志，发送通知
 */
void TraderAdapter::onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate)
{
	if(!bSucc)														// 如果登录失败
	{
		_state = AS_LOGINFAILED;									// 设置状态为登录失败
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Trader login failed: {}", _id.c_str(), msg);

		if (_notifier)												// 如果事件通知器存在
			_notifier->notify(id(), fmt::format("login failed: {}", msg).c_str());	// 发送登录失败通知
	}
	else															// 如果登录成功
	{
		_state = AS_LOGINED;										// 设置状态为已登录
		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,"[{}] Trader login succeed, trading date: {}", _id.c_str(), tradingdate);
		_trading_day = tradingdate;									// 保存交易日期
		_trader_api->queryPositions();								// 查询持仓
	}
}

/**
 * @brief 登出回调
 * 
 * 处理登出事件（当前为空实现）。
 */
void TraderAdapter::onLogout()
{
	
}

/**
 * @brief 委托响应回调
 * @param entrust 委托单对象
 * @param err 错误信息，成功时为NULL
 * 
 * 处理委托下单的响应：
 * - 如果下单失败：更新未完成订单数量，通知监听器，发送错误通知
 * - 如果下单成功：不做处理（由订单推送回调处理）
 * 
 * 注意：实盘中发现错误单有时候会推送两次，所以这里加了一个检查未完成单的逻辑，
 * 如果未完成订单为0，则说明这一次是重复通知，则不再处理了。
 */
void TraderAdapter::onRspEntrust(WTSEntrust* entrust, WTSError *err)
{
	if (err && err->getErrorCode() != WEC_NONE)					// 如果下单失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, err->getMessage());	// 记录错误日志
		WTSContractInfo* cInfo = entrust->getContractInfo();		// 获取合约信息
		WTSCommodityInfo* commInfo = cInfo->getCommInfo();			// 获取商品信息
		std::string stdCode;
		if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
			stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
		else if (CodeHelper::isMonthlyCode(cInfo->getCode()))		// 如果是分月合约
			stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
		else														// 普通合约
			stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码

		bool isLong = (entrust->getDirection() == WDT_LONG);		// 判断是否多头
		bool isToday = (entrust->getOffsetType() == WOT_CLOSETODAY);	// 判断是否平今
		bool isOpen = (entrust->getOffsetType() == WOT_OPEN);		// 判断是否开仓
		double qty = entrust->getVolume();							// 获取委托数量

		std::string action;											// 构造动作描述字符串
		if (isOpen)													// 如果是开仓
			action = "open ";
		else if (isToday)											// 如果是平今
			action = "closetoday ";
		else														// 如果是平昨
			action = "close ";
		action += isLong ? "long" : "short";						// 添加方向

		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, 
			"[{}] Order placing failed: {}, instrument: {}, action: {}, qty: {}", _id.c_str(), err->getMessage(), entrust->getCode(), action.c_str(), qty);	// 记录详细错误日志

		// 如果下单失败，要更新未完成数量
		// 实盘中发现错误单有时候会推送两次
		// 所以这里加一个检查未完成单的逻辑
		// 如果有错单，正常情况下未完成单一定不为0
		// 如果未完成订单为0，则说明这一次是重复通知，则不再处理了
		double oldQty = _undone_qty[stdCode];						// 获取旧的未完成数量
		if (decimal::eq(oldQty, 0))									// 如果未完成数量为0，说明是重复通知
			return;													// 直接返回，不再处理

		bool isBuy = (isLong&&isOpen) || (!isLong && !isOpen);		// 判断是否买入（买入=多头开仓或空头平仓）
		updateUndone(stdCode.c_str(), qty * (isBuy ? -1 : 1), true);	// 更新未完成数量（买入减少，卖出增加）

		if (strlen(entrust->getUserTag()) > 0)						// 如果用户标签不为空（说明是本系统的订单）
		{
			char* userTag = (char*)entrust->getUserTag();			// 获取用户标签
			userTag += _order_pattern.size() + 1;					// 跳过订单标签模式，定位到本地订单ID
			uint32_t localid = strtoul(userTag, NULL, 10);			// 解析本地订单ID

			for(auto sink : _sinks)									// 通知所有监听器
				sink->on_entrust(localid, stdCode.c_str(), false, err->getMessage());	// 调用委托失败回调

			if (_notifier)											// 如果事件通知器存在
				_notifier->notify(id(), fmt::format(" Order placing failed: {}", err->getMessage()).c_str());	// 发送通知
		}
		else														// 如果是外部订单
		{
			WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
				"[{}] Outter Order placing failed: {}, instrument: {}, action: {}, qty: {}", _id.c_str(), err->getMessage(), entrust->getCode(), action.c_str(), qty);	// 记录警告日志
		}
	}
}

/**
 * @brief 资金查询响应回调
 * @param ayAccounts 资金账户信息数组
 * 
 * 处理资金查询响应：
 * 1. 保存资金数据到文件（如果启用了数据保存）
 * 2. 通知所有监听器资金变化
 * 3. 如果所有查询完成，设置状态为就绪，通知监听器通道就绪
 */
void TraderAdapter::onRspAccount(WTSArray* ayAccounts)
{
	if (_save_data)												// 如果启用了数据保存
	{
		saveData(ayAccounts);										// 保存资金数据到文件
	}

	if(ayAccounts)													// 如果资金信息数组存在
	{
		// 通知所有监听接口
		for (auto sink : _sinks)									// 遍历所有监听器
		{
			for (uint32_t idx = 0; idx < ayAccounts->size(); idx++)	// 遍历所有资金账户
			{
				WTSAccountInfo* fundInfo = (WTSAccountInfo*)ayAccounts->at(idx);	// 获取资金账户信息
				sink->on_account(fundInfo->getCurrency(), fundInfo->getPreBalance(), fundInfo->getBalance(), fundInfo->getBalance() + fundInfo->getDynProfit(), fundInfo->getAvailable(),
					fundInfo->getCloseProfit(), fundInfo->getDynProfit(), fundInfo->getMargin(), fundInfo->getCommission(), fundInfo->getDeposit(), fundInfo->getWithdraw());	// 调用资金回调
			}
		}
	}

	if(_state == AS_TRADES_QRYED)									// 如果成交查询已完成（说明所有查询都完成了）
	{
		_state = AS_ALLREADY;										// 设置状态为全部就绪

		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Trading channel ready", _id.c_str());	// 记录就绪日志
		for (auto& sink : _sinks)									// 通知所有监听器
			sink->on_channel_ready();								// 调用通道就绪回调
	}
}

/**
 * @brief 持仓查询响应回调
 * @param ayPositions 持仓信息数组
 * 
 * 处理持仓查询响应：
 * 1. 更新内部持仓数据
 * 2. 打印持仓信息
 * 3. 通知所有监听器持仓变化
 * 4. 如果登录完成，设置状态为持仓查询完成，查询订单
 */
void TraderAdapter::onRspPosition(const WTSArray* ayPositions)
{
	if (ayPositions && ayPositions->size() > 0)					// 如果持仓信息数组存在且不为空
	{
		for (auto it = ayPositions->begin(); it != ayPositions->end(); it++)	// 遍历所有持仓
		{
			WTSPositionItem* pItem = (WTSPositionItem*)(*it);		// 获取持仓项
			WTSContractInfo* cInfo = pItem->getContractInfo();		// 获取合约信息
			if (cInfo == NULL)										// 如果合约信息为空，跳过
				continue;

			WTSCommodityInfo* commInfo = cInfo->getCommInfo();		// 获取商品信息
			std::string stdCode;
			if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
				stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
			else if (CodeHelper::isMonthlyCode(cInfo->getCode()))	// 如果是分月合约
				stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
			else													// 普通合约
				stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码
			PosItem& pos = _positions[stdCode];						// 获取或创建持仓项
			if (pItem->getDirection() == WDT_LONG)					// 如果是多头持仓
			{
				pos.l_newavail = pItem->getAvailNewPos();			// 更新多头今仓可用
				pos.l_newvol = pItem->getNewPosition();				// 更新多头今仓数量
				pos.l_preavail = pItem->getAvailPrePos();			// 更新多头昨仓可用
				pos.l_prevol = pItem->getPrePosition();				// 更新多头昨仓数量
			}
			else													// 如果是空头持仓
			{
				pos.s_newavail = pItem->getAvailNewPos();			// 更新空头今仓可用
				pos.s_newvol = pItem->getNewPosition();				// 更新空头今仓数量
				pos.s_preavail = pItem->getAvailPrePos();			// 更新空头昨仓可用
				pos.s_prevol = pItem->getPrePosition();				// 更新空头昨仓数量
			}
		}

		for (auto it = _positions.begin(); it != _positions.end(); it++)	// 遍历所有持仓
		{
			const char* stdCode = it->first.c_str();				// 获取标准合约代码
			const PosItem& pItem = it->second;						// 获取持仓项
			printPosition(stdCode, pItem);							// 打印持仓信息
			for (auto sink : _sinks)								// 通知所有监听器
			{
				sink->on_position(stdCode, true, pItem.l_prevol, pItem.l_preavail, pItem.l_newvol, pItem.l_newavail, _trading_day);	// 通知多头持仓
				sink->on_position(stdCode, false, pItem.s_prevol, pItem.s_preavail, pItem.s_newvol, pItem.s_newavail, _trading_day);	// 通知空头持仓
			}
		}
	}

	WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,"[{}] Position data updated", _id.c_str());	// 记录持仓更新日志

	if (_state == AS_LOGINED)										// 如果状态为已登录
	{
		_state = AS_POSITION_QRYED;									// 设置状态为持仓查询完成

		_trader_api->queryOrders();									// 查询订单
	}
}

/**
 * @brief 订单查询响应回调
 * @param ayOrders 订单信息数组
 * 
 * 处理订单查询响应：
 * 1. 初始化订单映射表（如果不存在）
 * 2. 清空未完成订单数量映射表（重新计算）
 * 3. 遍历所有订单，进行以下处理：
 *    a. 解析订单信息（合约代码、买卖方向等）
 *    b. 转换为标准合约代码（根据合约类型选择不同的转换方式）
 *    c. 更新订单ID集合（用于标记已处理过的订单）
 *    d. 更新交易统计数据：
 *       - 买入/卖出订单次数和数量
 *       - 错单次数和数量（区分买入和卖出）
 *       - 撤单次数和数量（区分普通撤单和自动撤单，区分买入和卖出）
 *    e. 如果是本系统的订单（通过用户标签匹配）：
 *       - 解析本地订单ID
 *       - 添加到订单映射表（使用自旋锁保证线程安全）
 *       - 更新未完成订单数量（买入增加，卖出减少）
 * 4. 打印所有合约的未完成订单数量日志
 * 5. 如果持仓查询已完成，设置状态为订单查询完成，并查询成交
 * 
 * 注意：
 * - 只有活跃订单（isAlive()返回true）才会被添加到订单映射表
 * - 只有本系统的订单（用户标签匹配订单模式）才会更新未完成订单数量
 * - 未完成订单数量：买入订单为正数，卖出订单为负数
 * - 错单和撤单的统计会区分普通订单（WOF_NOR）和自动撤单（如风控撤单）
 */
void TraderAdapter::onRspOrders(const WTSArray* ayOrders)
{
	if (ayOrders)													// 如果订单信息数组存在
	{
		if (_orders == NULL)										// 如果订单映射表不存在
			_orders = OrderMap::create();							// 创建订单映射表

		_undone_qty.clear();										// 清空未完成订单数量映射表（重新计算）

		for (auto it = ayOrders->begin(); it != ayOrders->end(); it++)	// 遍历所有订单
		{
			WTSOrderInfo* orderInfo = (WTSOrderInfo*)(*it);		// 获取订单信息
			if (orderInfo == NULL)									// 如果订单信息为空，跳过
				continue;

			WTSContractInfo* cInfo = orderInfo->getContractInfo();	// 获取合约信息
			if (cInfo == NULL)										// 如果合约信息为空，跳过
				continue;

			// 判断是否买入：买入=多头开仓或空头平仓
			bool isBuy = (orderInfo->getDirection() == WDT_LONG && orderInfo->getOffsetType() == WOT_OPEN) || (orderInfo->getDirection() == WDT_SHORT && orderInfo->getOffsetType() != WOT_OPEN);

			WTSCommodityInfo* commInfo = cInfo->getCommInfo();		// 获取商品信息
			std::string stdCode;
			if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
				stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
			else if (CodeHelper::isMonthlyCode(cInfo->getCode()))	// 如果是分月合约
				stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
			else													// 普通合约
				stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码

			_orderids.insert(orderInfo->getOrderID());				// 将订单号添加到订单ID集合（用于标记已处理过的订单）

			WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());	// 获取交易统计信息
			if (statInfo == NULL)									// 如果统计信息不存在，则创建
			{
				statInfo = WTSTradeStateInfo::create(stdCode.c_str());
				_stat_map->add(stdCode, statInfo, false);
			}
			TradeStatInfo& statItem = statInfo->statInfo();			// 获取统计项
			if (isBuy)												// 如果是买入订单
			{
				statItem.b_orders++;								// 增加买入订单次数
				statItem.b_ordqty += orderInfo->getVolume();		// 增加买入订单数量

				if (orderInfo->isError())							// 如果是错单（错单要和撤单区分开）
				{
					statItem.b_wrongs++;							// 增加买入错单次数
					statItem.b_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加买入错单数量（未成交部分）
				}
				else if (orderInfo->getOrderState() == WOS_Canceled)	// 如果是已撤销订单
				{			
					if (orderInfo->getOrderFlag() == WOF_NOR)		// 如果是普通订单标志
					{
						statItem.b_cancels++;						// 增加买入普通撤单次数
						statItem.b_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加买入普通撤单数量（未成交部分）
					}
					else											// 如果是自动撤单（如风险控制撤单）
					{
						statItem.b_auto_cancels++;					// 增加买入自动撤单次数
						statItem.b_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加买入自动撤单数量（未成交部分）
					}
				}
				
			}
			else													// 如果是卖出订单
			{
				statItem.s_orders++;								// 增加卖出订单次数
				statItem.s_ordqty += orderInfo->getVolume();		// 增加卖出订单数量

				if (orderInfo->isError())							// 如果是错单（错单要和撤单区分开）
				{
					statItem.s_wrongs++;							// 增加卖出错单次数
					statItem.s_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加卖出错单数量（未成交部分）
				}
				else if (orderInfo->getOrderState() == WOS_Canceled)	// 如果是已撤销订单
				{
					if (orderInfo->getOrderFlag() == WOF_NOR)		// 如果是普通订单标志
					{
						statItem.s_cancels++;						// 增加卖出普通撤单次数
						statItem.s_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加卖出普通撤单数量（未成交部分）
					}
					else											// 如果是自动撤单（如风险控制撤单）
					{
						statItem.s_auto_cancels++;					// 增加卖出自动撤单次数
						statItem.s_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加卖出自动撤单数量（未成交部分）
					}
				}
			}			

			if (!orderInfo->isAlive())								// 如果订单已结束（非活跃），跳过后续处理
				continue;

			if (!StrUtil::startsWith(orderInfo->getUserTag(), _order_pattern.c_str(), true))	// 如果用户标签不匹配订单模式（不是本系统的订单）
				continue;											// 跳过后续处理

			char* userTag = (char*)orderInfo->getUserTag();			// 获取用户标签
			userTag += _order_pattern.size() + 1;					// 跳过订单标签模式，定位到本地订单ID
			uint32_t localid = strtoul(userTag, NULL, 10);			// 解析本地订单ID

			{
				SpinLock lock(_mtx_orders);							// 加锁保护订单映射表
				_orders->add(localid, orderInfo);					// 将订单添加到订单映射表（使用本地订单ID作为key）
			}

			double& curQty = _undone_qty[stdCode];					// 获取该合约的未完成订单数量引用
			curQty += orderInfo->getVolLeft()*(isBuy ? 1 : -1);		// 更新未完成订单数量（买入增加，卖出减少）
		}

		for (auto it = _undone_qty.begin(); it != _undone_qty.end(); it++)	// 遍历所有未完成订单数量
		{
			const char* stdCode = it->first.c_str();				// 获取标准合约代码
			const double& curQty = _undone_qty[stdCode];			// 获取未完成订单数量

			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
				"[{}]{} undone quantity {}", _id.c_str(), stdCode, curQty);	// 记录未完成订单数量日志
		}
	}

	if (_state == AS_POSITION_QRYED)								// 如果持仓查询已完成
	{
		_state = AS_ORDERS_QRYED;									// 设置状态为订单查询完成

		_trader_api->queryTrades();									// 查询成交（继续查询流程）
	}
}

/**
 * @brief 打印持仓信息
 * @param code 合约代码
 * @param pItem 持仓项数据
 * 
 * 将持仓信息输出到日志，格式：多头昨仓[昨仓可用]|今仓[今仓可用]，空头昨仓[昨仓可用]|今仓[今仓可用]
 */
void TraderAdapter::printPosition(const char* code, const PosItem& pItem)
{
	WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] {} position updated, long:{}[{}]|{}[{}], short:{}[{}]|{}[{}]",
		_id.c_str(), code, pItem.l_prevol, pItem.l_preavail, pItem.l_newvol, pItem.l_newavail, 
		pItem.s_prevol, pItem.s_preavail, pItem.s_newvol, pItem.s_newavail);
}

/**
 * @brief 成交查询响应回调
 * @param ayTrades 成交信息数组
 * 
 * 处理成交查询响应：
 * 1. 更新交易统计数据（开仓量、平仓量、平今量等）
 * 2. 检查自成交
 * 3. 打印交易统计信息
 * 4. 如果订单查询完成，设置状态为成交查询完成，查询资金
 */
void TraderAdapter::onRspTrades(const WTSArray* ayTrades)
{
	if (ayTrades)													// 如果成交信息数组存在
	{
		for (auto it = ayTrades->begin(); it != ayTrades->end(); it++)	// 遍历所有成交
		{
			WTSTradeInfo* tInfo = (WTSTradeInfo*)(*it);				// 获取成交信息

			WTSContractInfo* cInfo = tInfo->getContractInfo();		// 获取合约信息
			if (cInfo == NULL)										// 如果合约信息为空，跳过
				continue;

			WTSCommodityInfo* commInfo = cInfo->getCommInfo();		// 获取商品信息
			std::string stdCode;
			if (commInfo->getCategoty() == CC_Future)				// 如果是期货
				stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
			else if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
				stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
			else if (commInfo->getCategoty() == CC_Stock)			// 如果是股票
				stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码
			else													// 其他类型
				stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), commInfo->getProduct());	// 转换为标准代码

			WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());	// 获取交易统计信息
			if(statInfo == NULL)									// 如果统计信息不存在，则创建
			{
				statInfo = WTSTradeStateInfo::create(stdCode.c_str());
				_stat_map->add(stdCode, statInfo, false);
			}
			TradeStatInfo& statItem = statInfo->statInfo();			// 获取统计项

			bool isLong = (tInfo->getDirection() == WDT_LONG);		// 判断是否多头
			bool isOpen = (tInfo->getOffsetType() == WOT_OPEN);		// 判断是否开仓
			bool isCloseT = (tInfo->getOffsetType() == WOT_CLOSETODAY);	// 判断是否平今
			double qty = tInfo->getVolume();							// 获取成交数量

			if (isLong)												// 如果是多头成交
			{
				if (isOpen)											// 如果是开仓
					statItem.l_openvol += qty;						// 增加多头开仓量
				else if (isCloseT)									// 如果是平今
					statItem.l_closetvol += qty;						// 增加多头平今量
				else												// 如果是平昨
					statItem.l_closevol += qty;						// 增加多头平昨量
			}
			else													// 如果是空头成交
			{
				if (isOpen)											// 如果是开仓
					statItem.s_openvol += qty;						// 增加空头开仓量
				else if (isCloseT)									// 如果是平今
					statItem.s_closetvol += qty;						// 增加空头平今量
				else												// 如果是平昨
					statItem.s_closevol += qty;						// 增加空头平昨量
			}

			checkSelfMatch(stdCode.c_str(), tInfo);					// 检查自成交
		}

		for (auto it = _stat_map->begin(); it != _stat_map->end(); it++)	// 遍历所有交易统计信息
		{
			const char* stdCode = it->first.c_str();				// 获取标准合约代码
			WTSTradeStateInfo* pItem = (WTSTradeStateInfo*)it->second;	// 获取交易统计信息
			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
				"[{}] {} action stats updated, long opened: {}, long closed: {}, new long closed: {}, short opened: {}, short closed: {}, new short closed: {}",
				_id.c_str(), stdCode, pItem->open_volume_long(), pItem->close_volume_long(), pItem->closet_volume_long(),
				pItem->open_volume_short(), pItem->close_volume_short(), pItem->closet_volume_short());	// 打印交易统计信息
		}
	}

	if (_state == AS_ORDERS_QRYED)									// 如果订单查询已完成
	{
		_state = AS_TRADES_QRYED;									// 设置状态为成交查询完成

		_trader_api->queryAccount();									// 查询资金
	}
}

/**
 * @brief 检查自成交
 * @param stdCode 标准合约代码
 * @param tInfo 成交信息对象
 * @return true表示检测到自成交，false表示未检测到自成交
 * 
 * 通过成交单号和关联订单号检测自成交：
 * 1. 如果成交单号已存在，检查关联订单号是否相同
 * 2. 如果关联订单号不同，说明是自成交（同一个成交单号对应不同的订单号）
 * 3. 如果检测到自成交，将该合约加入自成交列表，禁止后续交易
 * 4. 如果关联订单号相同，说明是重复推送，忽略
 */
bool TraderAdapter::checkSelfMatch(const char* stdCode, WTSTradeInfo* tInfo)
{
	if (tInfo == NULL)												// 如果成交信息为空，返回false
		return false;

	const char* tid = tInfo->getTradeID();							// 获取成交单号
	const char* refid = tInfo->getRefOrder();						// 获取关联订单号

	auto it = _trade_refs.find(tid);								// 查找成交单号是否已存在
	if (it != _trade_refs.end())									// 如果成交单号已存在
	{
		/*
		 *	By Wesley @ 2022.03.07
		 *	如果成交单号已经存在，则检查关联订单号是否相同
		 */
		const std::string& oid = it->second;						// 获取已保存的关联订单号
		if (oid.compare(refid) != 0)								// 如果关联订单号不同
		{
            // 同一个成交单号对应不同的订单号 = 自成交！
            // 说明：同一个成交单T001，第一次推送时关联订单是O001（买单）
            //       第二次推送时关联订单是O002（卖单）
            //       说明O001和O002都是本账户的订单，发生了自成交
			WTSLogger::log_dyn("trader", _id.c_str(), LL_FATAL, 
				"[{0}] Self matching detected on {1}!!! Instructions on {1} will be forbidden!!!", _id.c_str(), stdCode);	// 记录严重错误日志
			_self_matches.insert(stdCode);							// 将该合约加入自成交列表

			return true;											// 返回true表示检测到自成交
		}
		else														// 如果关联订单号相同
		{
			// 关联订单一样，说明是重复推送，不用管了
		}
	}
	else															// 如果成交单号不存在
	{
		_trade_refs[tid] = refid;									// 保存成交单号和关联订单号的映射
	}

	return false;													// 返回false表示未检测到自成交
}

/**
 * @brief 将订单状态转换为字符串名称
 * @param woState 订单状态枚举值
 * @return 订单状态的字符串表示
 * 
 * 用于日志输出和调试，将订单状态枚举值转换为简短易读的字符串：
 * - WOS_AllTraded -> "AllTrd" (全部成交)
 * - WOS_PartTraded_NotQueuing/WOS_PartTraded_Queuing -> "PrtTrd" (部分成交)
 * - WOS_NotTraded_Queuing -> "UnTrd" (未成交排队中)
 * - WOS_Canceled/WOS_NotTraded_NotQueuing -> "Cncld" (已撤销)
 * - WOS_Submitting -> "Smtting" (提交中)
 * - WOS_Nottouched -> "UnSmt" (未提交)
 * - 其他 -> "Error" (错误状态)
 */
inline const char* stateToName(WTSOrderState woState)
{
	if (woState == WOS_AllTraded)									// 如果订单状态为全部成交
		return "AllTrd";												// 返回"AllTrd"
	else if (woState == WOS_PartTraded_NotQueuing || woState == WOS_PartTraded_Queuing)	// 如果订单状态为部分成交
		return "PrtTrd";												// 返回"PrtTrd"
	else if (woState == WOS_NotTraded_Queuing)						// 如果订单状态为未成交排队中
		return "UnTrd";													// 返回"UnTrd"
	else if (woState == WOS_Canceled || woState == WOS_NotTraded_NotQueuing)	// 如果订单状态为已撤销或未成交未排队
		return "Cncld";													// 返回"Cncld"
	else if (woState == WOS_Submitting)								// 如果订单状态为提交中
		return "Smtting";												// 返回"Smtting"
	else if (woState == WOS_Nottouched)								// 如果订单状态为未提交
		return "UnSmt";													// 返回"UnSmt"
	else																// 如果订单状态为其他未知状态
		return "Error";													// 返回"Error"
}

/**
 * @brief 订单推送回调
 * @param orderInfo 订单信息对象
 * 
 * 处理订单状态的实时推送：
 * 1. 解析订单信息（合约代码、买卖方向等）
 * 2. 更新撤单统计数据（区分错单和普通撤单）
 * 3. 处理首次推送订单时的可平仓量更新（平仓单会冻结可平仓量）
 * 4. 处理撤单时的可平仓量恢复（平仓单撤单后需恢复可平仓量）
 * 5. 更新内部订单映射（如果是本系统发出的订单）
 * 6. 更新未完成订单数量（如果是本系统的撤单）
 * 7. 通知所有监听器订单状态变化
 * 8. 记录订单日志（如果启用了数据保存）
 * 9. 发送订单通知
 */
void TraderAdapter::onPushOrder(WTSOrderInfo* orderInfo)
{
	if (orderInfo == NULL)													// 如果订单信息为空，直接返回
		return;

	WTSContractInfo* cInfo = orderInfo->getContractInfo();					// 获取合约信息
	if (cInfo == NULL)														// 如果合约信息为空，直接返回
		return;

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();						// 获取商品信息
	std::string stdCode;													// 标准合约代码
	if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
		stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
	else if (CodeHelper::isMonthlyCode(cInfo->getCode()))					// 如果是分月合约
		stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
	else																	// 其他类型合约
		stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码

	// 判断是否买入：买入=多头开仓或空头平仓
	bool isBuy = (orderInfo->getDirection() == WDT_LONG && orderInfo->getOffsetType() == WOT_OPEN) || (orderInfo->getDirection() == WDT_SHORT && orderInfo->getOffsetType() != WOT_OPEN);
	
	// 如果订单状态为已撤销，需要更新统计数据
	if (orderInfo->getOrderState() == WOS_Canceled)
	{
		WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());	// 获取交易统计信息
		if (statInfo == NULL)												// 如果统计信息不存在，则创建
		{
			statInfo = WTSTradeStateInfo::create(stdCode.c_str());
			_stat_map->add(stdCode, statInfo, false);
		}
		TradeStatInfo& statItem = statInfo->statInfo();						// 获取统计项
		if(isBuy)															// 如果是买入订单
		{
			if (orderInfo->isError())										// 如果是错单（错单要和撤单区分开）
			{
				statItem.b_wrongs++;										// 增加买入错单次数
				statItem.b_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加买入错单数量（未成交部分）
			}
			else															// 如果是正常撤单
			{
				// 只有普通订单的撤单才计入统计
				if (orderInfo->getOrderFlag() == WOF_NOR)					// 如果是普通订单标志
				{
					statItem.b_cancels++;									// 增加买入撤单次数
					statItem.b_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加买入撤单数量（未成交部分）
				}
				else														// 如果是自动撤单（如风险控制撤单）
				{
					statItem.b_auto_cancels++;								// 增加买入自动撤单次数
					statItem.b_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加买入自动撤单数量（未成交部分）
				}
			}
		}
		else																// 如果是卖出订单
		{
			if (orderInfo->isError())										// 如果是错单（错单要和撤单区分开）
			{
				statItem.s_wrongs++;										// 增加卖出错单次数
				statItem.s_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加卖出错单数量（未成交部分）
			}
			else															// 如果是正常撤单
			{
				// 只有普通订单的撤单才计入统计
				if (orderInfo->getOrderFlag() == WOF_NOR)					// 如果是普通订单标志
				{
					statItem.s_cancels++;									// 增加卖出撤单次数
					statItem.s_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加卖出撤单数量（未成交部分）
				}
				else														// 如果是自动撤单（如风险控制撤单）
				{
					statItem.s_auto_cancels++;								// 增加卖出自动撤单次数
					statItem.s_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();	// 增加卖出自动撤单数量（未成交部分）
				}
			}
		}
	}

	WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG, "[{}] Order notified, instrument: {}, usertag: {}, state: {}", _id.c_str(), stdCode.c_str(), orderInfo->getUserTag(), stateToName(orderInfo->getOrderState()));	// 记录订单推送日志

	// 先检查该订单是不是第一次推送过来
	// 如果是第一次推送过来，则要根据开平更新可平仓量
	if (strlen(orderInfo->getOrderID()) > 0)								// 如果订单号不为空
	{
		auto it = _orderids.find(orderInfo->getOrderID());					// 查找订单号是否已存在
		if (it == _orderids.end())											// 如果是第一次推送
		{
			// 先把订单号缓存起来，防止重复处理
			_orderids.insert(orderInfo->getOrderID());						// 将订单号添加到缓存集合

			WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());	// 获取交易统计信息
			if (statInfo == NULL)											// 如果统计信息不存在，则创建
			{
				statInfo = WTSTradeStateInfo::create(stdCode.c_str());
				_stat_map->add(stdCode, statInfo, false);
			}
			TradeStatInfo& statItem = statInfo->statInfo();					// 获取统计项
			if (isBuy)														// 如果是买入订单
			{
				statItem.b_orders++;										// 增加买入订单次数
				statItem.b_ordqty += orderInfo->getVolume();					// 增加买入订单数量
			}
			else															// 如果是卖出订单
			{
				statItem.s_orders++;										// 增加卖出订单次数
				statItem.s_ordqty += orderInfo->getVolume();					// 增加卖出订单数量
			}

			// 只有平仓需要更新可平仓量（开仓不影响可平仓量）
			if (orderInfo->getOffsetType() != WOT_OPEN)						// 如果是平仓订单
			{
				bool isLong = (orderInfo->getDirection() == WDT_LONG);		// 判断是否多头
				bool isToday = (orderInfo->getOffsetType() == WOT_CLOSETODAY);	// 判断是否平今
				double qty = orderInfo->getVolume();							// 获取订单数量

				PosItem& pItem = _positions[stdCode];						// 获取或创建持仓项
				if (isLong)													// 如果是平多仓
				{
					if (isToday)											// 如果是平今
					{
						// 如果是平今，则只需要更新可平今仓
						pItem.l_newavail -= min(pItem.l_newavail, qty);		// 减少多头今仓可用（冻结可平仓量）
					}
					else													// 如果是平昨
					{
						double left = qty;									// 剩余待平数量

						// 如果是平昨，则先更新可平昨仓，还有剩余，再更新可平今仓
						// 如果品种区分平昨平今，也按照这个流程，因为平昨的总数量不可能超出昨仓
						double maxQty = min(pItem.l_preavail, qty);			// 计算可平昨仓数量
						pItem.l_preavail -= maxQty;							// 减少多头昨仓可用（冻结可平仓量）
						pItem.l_preavail = max(pItem.l_preavail, 0.0);		// 确保不为负数
						left -= maxQty;										// 减少剩余数量

						if (left > 0)										// 如果还有剩余待平数量
						{
							pItem.l_newavail -= min(pItem.l_newavail, left);	// 减少多头今仓可用（用今仓补充）
							pItem.l_newavail = max(pItem.l_newavail, 0.0);	// 确保不为负数
						}
					}
				}
				else														// 如果是平空仓
				{
					if (isToday)											// 如果是平今
					{
						// 如果是平今，则只需要更新可平今仓
						pItem.s_newavail -= min(pItem.s_newavail, qty);		// 减少空头今仓可用（冻结可平仓量）
					}
					else													// 如果是平昨
					{
						double left = qty;									// 剩余待平数量

						// 如果是平昨，则先更新可平昨仓，还有剩余，再更新可平今仓
						double maxQty = min(pItem.s_preavail, qty);			// 计算可平昨仓数量
						pItem.s_preavail -= maxQty;							// 减少空头昨仓可用（冻结可平仓量）
						pItem.s_preavail = max(pItem.s_preavail, 0.0);		// 确保不为负数
						left -= maxQty;										// 减少剩余数量

						if (left > 0)										// 如果还有剩余待平数量
						{
							pItem.s_newavail -= min(pItem.s_newavail, left);	// 减少空头今仓可用（用今仓补充）
							pItem.s_newavail = max(pItem.s_newavail, 0.0);	// 确保不为负数
						}
					}
				}
				printPosition(stdCode.c_str(), pItem);						// 打印持仓信息
			}
		}
		else if (orderInfo->getOrderState() == WOS_Canceled && orderInfo->getOffsetType() != WOT_OPEN)	// 如果订单不是第一次推送，且撤销了，且是平仓单
		{
			// 如果订单不是第一次推送，且撤销了，则要更新可平仓量（恢复可平仓量）
			bool isLong = (orderInfo->getDirection() == WDT_LONG);			// 判断是否多头
			bool isToday = (orderInfo->getOffsetType() == WOT_CLOSETODAY);	// 判断是否平今
			double qty = orderInfo->getVolume() - orderInfo->getVolTraded();	// 获取未成交数量（撤单数量）

			PosItem& pItem = _positions[stdCode];							// 获取持仓项
			if (isLong)														// 如果是平多仓
			{
				if (isToday)												// 如果是平今
				{
					// 如果是平今，则只需要更新可平今仓
					pItem.l_newavail += qty;									// 恢复多头今仓可用（撤单后恢复可平仓量）
				}
				else														// 如果是平昨
				{
					pItem.l_preavail += qty;									// 恢复多头昨仓可用
					if (pItem.l_preavail > pItem.l_prevol)					// 如果可平昨仓超过昨仓总量（不应该发生，但做保护）
					{
						pItem.l_newavail += (pItem.l_preavail - pItem.l_prevol);	// 超出部分转为今仓可用
						pItem.l_preavail = pItem.l_prevol;					// 限制为昨仓总量
					}
				}
			}
			else															// 如果是平空仓
			{
				if (isToday)												// 如果是平今
				{
					// 如果是平今，则只需要更新可平今仓
					pItem.s_newavail += qty;									// 恢复空头今仓可用（撤单后恢复可平仓量）
				}
				else														// 如果是平昨
				{
					pItem.s_preavail += qty;									// 恢复空头昨仓可用
					if (pItem.s_preavail > pItem.s_prevol)					// 如果可平昨仓超过昨仓总量（不应该发生，但做保护）
					{
						pItem.s_newavail += (pItem.s_preavail - pItem.s_prevol);	// 超出部分转为今仓可用
						pItem.s_preavail = pItem.s_prevol;					// 限制为昨仓总量
					}
				}
			}
			printPosition(stdCode.c_str(), pItem);							// 打印持仓信息
		}
	}

	uint32_t localid = 0;													// 本地订单ID，初始化为0

	// 先看看是不是本系统发出的订单
	if (StrUtil::startsWith(orderInfo->getUserTag(), _order_pattern.c_str(), true))	// 如果用户标签匹配订单模式
	{
		char* userTag = (char*)orderInfo->getUserTag();						// 获取用户标签
		userTag += _order_pattern.size() + 1;								// 跳过订单标签模式，定位到本地订单ID
		localid = strtoul(userTag, NULL, 10);								// 解析本地订单ID

		// 如果订单撤销，并且是本系统的订单，则要先更新未完成数量
		if (orderInfo->getOrderState() == WOS_Canceled)						// 如果订单状态为已撤销
		{
			// 撤单的时候，要更新未完成订单数量
			bool isLong = (orderInfo->getDirection() == WDT_LONG);			// 判断是否多头
			bool isOpen = (orderInfo->getOffsetType() == WOT_OPEN);			// 判断是否开仓
			bool isToday = (orderInfo->getOffsetType() == WOT_CLOSETODAY);	// 判断是否平今
			double qty = orderInfo->getVolume() - orderInfo->getVolTraded();	// 获取未成交数量（撤单数量）

			// 判断是否买入：买入=多头开仓或空头平仓
			bool isBuy = (isLong&&isOpen) || (!isLong && !isOpen);

			updateUndone(stdCode.c_str(), qty*(isBuy ? -1 : 1), true);		// 更新未完成订单数量（买入减少，卖出增加）

			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Order {} of {} canceled:{}, action: {}, leftqty: {}",
				_id.c_str(), orderInfo->getUserTag(), stdCode.c_str(), orderInfo->getStateMsg(),
				formatAction(orderInfo->getDirection(), orderInfo->getOffsetType()), qty);	// 记录撤单日志
		}
	}

	// 如果是本系统发出的订单则需要更新内部数据
	if(localid != 0)														// 如果是本系统的订单
	{
		{
			SpinLock lock(_mtx_orders);										// 加锁保护订单映射
			if (!orderInfo->isAlive() && _orders)							// 如果订单已结束（非活跃）且订单映射存在
			{
				_orders->remove(localid);									// 从订单映射中移除
			}
			else															// 如果订单仍然活跃或订单映射不存在
			{
				if (_orders == NULL)										// 如果订单映射不存在
					_orders = OrderMap::create();							// 创建订单映射

				_orders->add(localid, orderInfo);							// 添加到订单映射（更新或新增）
			}
		}

		// 通知所有监听接口
		for (auto sink : _sinks)											// 遍历所有监听器
			sink->on_order(localid, stdCode.c_str(), isBuy, orderInfo->getVolume(), orderInfo->getVolLeft(), orderInfo->getPrice(), orderInfo->getOrderState() == WOS_Canceled);	// 调用订单回调
	}

	// 不管是不是内部订单，订单结束了，都要写到日志里
	if (_save_data && !orderInfo->isAlive())								// 如果启用了数据保存且订单已结束
	{
		logOrder(localid, stdCode.c_str(), orderInfo);						// 记录订单日志
	}

	if (_notifier)															// 如果事件通知器存在
		_notifier->notify(id(), localid, stdCode.c_str(), orderInfo);		// 发送订单通知
}

/**
 * @brief 成交推送回调
 * @param tradeRecord 成交信息对象
 * 
 * 处理成交的实时推送：
 * 1. 更新持仓数据（根据成交方向和平仓类型）
 * 2. 更新交易统计数据（开仓量、平仓量等）
 * 3. 更新未完成订单数量（如果是本系统的订单）
 * 4. 通知所有监听器成交变化
 * 5. 记录成交日志（如果启用了数据保存）
 * 6. 检查自成交
 * 7. 发送成交通知
 * 8. 查询资金账户（成交后刷新资金）
 */
void TraderAdapter::onPushTrade(WTSTradeInfo* tradeRecord)
{
	WTSContractInfo* cInfo = tradeRecord->getContractInfo();		// 获取合约信息
	if (cInfo == NULL)												// 如果合约信息为空，直接返回
		return;

	bool isLong = (tradeRecord->getDirection() == WDT_LONG);		// 判断是否多头
	bool isOpen = (tradeRecord->getOffsetType() == WOT_OPEN);		// 判断是否开仓
	// 判断是否买入：买入=多头开仓或空头平仓
	bool isBuy = (tradeRecord->getDirection() == WDT_LONG && tradeRecord->getOffsetType() == WOT_OPEN) || (tradeRecord->getDirection() == WDT_SHORT && tradeRecord->getOffsetType() != WOT_OPEN);

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();				// 获取商品信息
	std::string stdCode;
	if (commInfo->getCategoty() == CC_Future)						// 如果是期货
		stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
	else if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)	// 如果是期货期权或现货期权
		stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());		// 转换为标准代码
	else if (commInfo->getCategoty() == CC_Stock)					// 如果是股票
		stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());	// 转换为标准代码
	else															// 其他类型
		stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), commInfo->getProduct());	// 转换为标准代码

	WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
		"[{}] Trade notified, instrument: {}, usertag: {}, trdqty: {}, trdprice: {}", 
			_id.c_str(), stdCode.c_str(), tradeRecord->getUserTag(), tradeRecord->getVolume(), tradeRecord->getPrice());	// 记录成交日志

	PosItem& pItem = _positions[stdCode];							// 获取或创建持仓项
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());	// 获取交易统计信息
	if (statInfo == NULL)											// 如果统计信息不存在，则创建
	{
		statInfo = WTSTradeStateInfo::create(stdCode.c_str());
		_stat_map->add(stdCode, statInfo, false);
	}

	TradeStatInfo& statItem = statInfo->statInfo();					// 获取统计项
	double vol = tradeRecord->getVolume();							// 获取成交数量
	if(isLong)														// 如果是多头成交
	{
		if (isOpen)													// 如果是开仓
		{
			pItem.l_newvol += vol;									// 增加多头今仓数量

			if(!commInfo->isT1())									// 如果不是T1（当日可卖），则更新可用持仓
				pItem.l_newavail += vol;								// 增加多头今仓可用

			statItem.l_openvol += vol;								// 增加多头开仓量统计
		}
		else if (tradeRecord->getOffsetType() == WOT_CLOSETODAY)	// 如果是平今
		{
			pItem.l_newvol -= vol;									// 减少多头今仓数量

			statItem.l_closevol += vol;								// 增加多头平仓量统计（平今也算平仓）
		}
		else														// 如果是平昨
		{
			double left = vol;										// 剩余待平数量
			double maxVol = min(left, pItem.l_prevol);				// 计算可平昨仓数量
			pItem.l_prevol -= maxVol;								// 减少多头昨仓数量
			left -= maxVol;											// 减少剩余数量
			pItem.l_newvol -= left;									// 减少多头今仓数量（平昨不足时，用今仓补充）

			statItem.l_closevol += vol;								// 增加多头平仓量统计
		}
	}
	else															// 如果是空头成交
	{
		if (isOpen)													// 如果是开仓
		{
			pItem.s_newvol += vol;									// 增加空头今仓数量
			if (!commInfo->isT1())									// 如果不是T1（当日可卖），则更新可用持仓
				pItem.s_newavail += vol;								// 增加空头今仓可用

			statItem.s_openvol += vol;								// 增加空头开仓量统计
		}
		else if (tradeRecord->getOffsetType() == WOT_CLOSETODAY)	// 如果是平今
		{
			pItem.s_newvol -= vol;									// 减少空头今仓数量

			statItem.s_closevol += vol;								// 增加空头平仓量统计（平今也算平仓）
		}
		else														// 如果是平昨
		{
			double left = vol;										// 剩余待平数量
			double maxVol = min(left, pItem.s_prevol);				// 计算可平昨仓数量
			pItem.s_prevol -= maxVol;								// 减少空头昨仓数量
			left -= maxVol;											// 减少剩余数量
			pItem.s_newvol -= left;									// 减少空头今仓数量（平昨不足时，用今仓补充）

			statItem.s_closevol += vol;								// 增加空头平仓量统计
		}
	}

	printPosition(stdCode.c_str(), pItem);							// 打印持仓信息

	// 如果是自己的订单，则更新未完成单
	uint32_t localid = 0;											// 本地订单ID，初始化为0
	if (StrUtil::startsWith(tradeRecord->getUserTag(), _order_pattern.c_str(), true))	// 如果是本系统的订单（用户标签匹配）
	{
		char* userTag = (char*)tradeRecord->getUserTag();			// 获取用户标签
		userTag += _order_pattern.size() + 1;						// 跳过订单标签模式，定位到本地订单ID
		localid = strtoul(userTag, NULL, 10);						// 解析本地订单ID

		updateUndone(stdCode.c_str(), vol*(isBuy ? -1 : 1), true);	// 更新未完成订单数量（买入减少，卖出增加）
	}

	for (auto sink : _sinks)										// 通知所有监听器
		sink->on_trade(localid, stdCode.c_str(), isBuy, vol, tradeRecord->getPrice());	// 调用成交回调

	if (_save_data)													// 如果启用了数据保存
	{
		logTrade(localid, stdCode.c_str(), tradeRecord);			// 记录成交日志
	}

	checkSelfMatch(stdCode.c_str(), tradeRecord);					// 检查自成交

	if (_notifier)													// 如果事件通知器存在
		_notifier->notify(id(), localid, stdCode.c_str(), tradeRecord);	// 发送成交通知

	_trader_api->queryAccount();									// 查询资金账户（成交后刷新资金）
}

/**
 * @brief 交易错误回调
 * @param err 错误信息对象
 * @param pData 附加数据，可为NULL
 * 
 * 处理交易通道的错误通知：
 * 1. 记录错误日志
 * 2. 发送错误通知给事件通知器
 */
void TraderAdapter::onTraderError(WTSError* err, void* pData /* = NULL */)
{
	if(err)															// 如果错误信息存在
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Error of trading channel occured: {}", _id.c_str(), err->getMessage());	// 记录错误日志

	if (_notifier)													// 如果事件通知器存在
		_notifier->notify(id(), fmt::format("Trading channel error: {}", err->getMessage()).c_str());	// 发送错误通知
}

/**
 * @brief 获取基础数据管理器
 * @return 基础数据管理器指针
 */
IBaseDataMgr* TraderAdapter::getBaseDataMgr()
{
	return _bd_mgr;													// 返回基础数据管理器指针
}

/**
 * @brief 处理交易日志
 * @param ll 日志级别
 * @param message 日志消息
 * 
 * 将交易通道的日志转发到系统日志。
 */
void TraderAdapter::handleTraderLog(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_dyn_raw("trader", _id.c_str(), ll, message);	// 记录交易通道日志
}

/**
 * @brief 查询资金账户
 * 
 * 如果交易通道已就绪，则查询资金账户信息。
 */
void TraderAdapter::queryFund()
{
	if (_state != AS_ALLREADY)										// 如果交易通道未就绪，直接返回
		return;

	_trader_api->queryAccount();									// 查询资金账户
}

#pragma endregion "ITraderSpi接口"


//////////////////////////////////////////////////////////////////////////
//CTPWrapperMgr
bool TraderAdapterMgr::addAdapter(const char* tname, TraderAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(tname) == 0)
		return false;

	auto it = _adapters.find(tname);
	if(it != _adapters.end())
	{
		WTSLogger::error("Same name of trading channels: {}", tname);
		return false;
	}

	_adapters[tname] = adapter;

	return true;
}

TraderAdapterPtr TraderAdapterMgr::getAdapter(const char* tname)
{
	auto it = _adapters.find(tname);
	if (it != _adapters.end())
	{
		return it->second;
	}

	return TraderAdapterPtr();
}

void TraderAdapterMgr::run()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		it->second->run();
	}

	WTSLogger::info("{} trading channels started", _adapters.size());
}

void TraderAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		it->second->release();
	}

	_adapters.clear();
}

void TraderAdapterMgr::refresh_funds()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		it->second->queryFund();
	}
}