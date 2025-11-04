/*!
 * \file TraderAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易适配器实现文件
 *
 * 本文件实现了TraderAdapter类的所有功能，包括：
 * 1. 初始化：加载交易模块，初始化交易接口和风险控制参数
 * 2. 连接管理：连接交易服务器，处理登录流程
 * 3. 下单操作：实现开多、开空、平多、平空等下单接口
 * 4. 撤单操作：实现撤单和全部撤单功能
 * 5. 持仓管理：维护持仓信息，区分昨仓和今仓，多空双向持仓
 * 6. 订单管理：维护订单列表，跟踪订单状态和未完成数量
 * 7. 策略转换：根据ActionPolicyMgr的策略规则，将策略信号转换为实际订单
 * 8. 风险控制：实现下单频率限制和撤单频率限制
 * 9. 回调处理：实现ITraderSpi接口的所有回调函数
 * 10. 统计管理：维护交易统计信息，包括开仓、平仓、撤单等统计数据
 */
#include "TraderAdapter.h"  // 交易适配器头文件
#include "WtHelper.h"  // UFT辅助工具
#include "ITrdNotifySink.h"  // 交易通知接收器接口
#include "ActionPolicyMgr.h"  // 动作策略管理器

#include "../Includes/WTSError.hpp"  // 错误定义
#include "../Includes/WTSVariant.hpp"  // 变体类型
#include "../Includes/WTSTradeDef.hpp"  // 交易定义
#include "../Includes/WTSSessionInfo.hpp"  // 会话信息
#include "../Includes/WTSContractInfo.hpp"  // 合约信息
#include "../Includes/IBaseDataMgr.h"  // 基础数据管理器接口
#include "../Includes/WTSRiskDef.hpp"  // 风险定义

#include <atomic>  // 原子操作

#include "../WTSTools/WTSLogger.h"  // 日志工具
#include "../Share/TimeUtils.hpp"  // 时间工具
#include "../Share/decimal.h"  // 小数工具
#include "../Share/DLLHelper.hpp"  // 动态库加载工具
#include "../Share/StrUtil.hpp"  // 字符串工具

#include <exception>  // 异常处理
#include <rapidjson/document.h>  // RapidJSON文档
#include <rapidjson/prettywriter.h>  // RapidJSON格式化写入器

namespace rj = rapidjson;  // RapidJSON命名空间别名
using namespace std;  // 标准命名空间

/**
 * @brief 生成本地订单ID
 * @return 本地订单ID
 * 
 * 使用原子操作生成本地订单ID，确保ID的唯一性。
 * 首次调用时，基于当前时间初始化ID生成器。
 */
inline uint32_t makeLocalOrderID()
{
	static std::atomic<uint32_t> _auto_order_id{ 0 };  // 静态原子变量，用于生成订单ID
	if (_auto_order_id == 0)  // 如果还未初始化
	{
		uint32_t curYear = TimeUtils::getCurDate() / 10000 * 10000 + 101;  // 计算当前年份的起始时间（1月1日）
		_auto_order_id = (uint32_t)((TimeUtils::getLocalTimeNow() - TimeUtils::makeTime(curYear, 0)) / 1000 * 50);  // 基于当前时间初始化ID
	}

	return _auto_order_id.fetch_add(1);  // 原子递增并返回
}

/**
 * @brief 格式化交易动作字符串
 * @param dType 方向类型
 * @param oType 开平类型
 * @return 动作字符串（"OL"开多、"CL"平多、"CNL"撤多、"OS"开空、"CS"平空、"CNS"撤空）
 * 
 * 根据方向类型和开平类型，返回对应的动作字符串。
 */
inline const char* formatAction(WTSDirectionType dType, WTSOffsetType oType)
{
	if(dType == WDT_LONG)  // 如果是多仓
	{
		if (oType == WOT_OPEN)  // 如果是开仓
			return "OL";  // 返回"开多"
		else if (oType == WOT_CLOSE)  // 如果是平仓
			return "CL";  // 返回"平多"
		else  // 如果是撤单
			return "CNL";  // 返回"撤多"
	}
	else  // 如果是空仓
	{
		if (oType == WOT_OPEN)  // 如果是开仓
			return "OS";  // 返回"开空"
		else if (oType == WOT_CLOSE)  // 如果是平仓
			return "CS";  // 返回"平空"
		else  // 如果是撤单
			return "CNS";  // 返回"撤空"
	}
}

/**
 * @brief 构造函数
 * 
 * 初始化交易适配器实例，设置默认值。
 */
TraderAdapter::TraderAdapter()
	: _id("")  // 初始化适配器ID为空字符串
	, _cfg(NULL)  // 初始化配置参数为空指针
	, _state(AS_NOTLOGIN)  // 初始化状态为未登录
	, _trader_api(NULL)  // 初始化交易接口为空指针
	, _orders(NULL)  // 初始化订单列表为空指针
	, _risk_mon_enabled(false)  // 初始化风险监控为禁用
	, _stat_map(NULL)  // 初始化统计映射表为空指针
{
}


/**
 * @brief 析构函数
 * 
 * 清理交易适配器占用的资源。
 */
TraderAdapter::~TraderAdapter()
{
	if (_stat_map)  // 如果统计映射表存在
		_stat_map->release();  // 释放统计映射表
}

/**
 * @brief 使用外部交易接口初始化适配器
 * @param id 适配器ID
 * @param api 交易接口指针
 * @param bdMgr 基础数据管理器
 * @param policyMgr 动作策略管理器
 * @return 初始化成功返回true，失败返回false
 * 
 * 直接使用外部提供的交易接口初始化适配器，无需加载动态库。
 */
bool TraderAdapter::initExt(const char* id, ITraderApi* api, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr)
{
	_policy_mgr = policyMgr;  // 保存动作策略管理器指针
	_bd_mgr = bdMgr;  // 保存基础数据管理器指针
	_id = id;  // 保存适配器ID

	_order_pattern = StrUtil::printf("wtp.%s", id);  // 生成订单用户标签模式："wtp.{id}"

	api->init(NULL);  // 初始化交易接口
	_trader_api = api;  // 保存交易接口指针
	return true;  // 返回成功
}

/**
 * @brief 初始化交易适配器
 * @param id 适配器ID
 * @param params 配置参数
 * @param bdMgr 基础数据管理器
 * @param policyMgr 动作策略管理器
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置参数加载交易模块，初始化交易接口。
 * 解析风险控制参数，加载交易模块动态库。
 */
bool TraderAdapter::init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr)
{
	if (params == NULL)  // 如果配置参数为空
		return false;  // 返回失败

	_policy_mgr = policyMgr;  // 保存动作策略管理器指针
	_bd_mgr = bdMgr;  // 保存基础数据管理器指针
	_id = id;  // 保存适配器ID

	_order_pattern = StrUtil::printf("wtp.%s", id);  // 生成订单用户标签模式："wtp.{id}"

	if (_cfg != NULL)  // 如果已经初始化过
		return false;  // 返回失败

	_cfg = params;  // 保存配置参数
	_cfg->retain();  // 增加引用计数

	//这里解析流量风控参数
	WTSVariant* cfgRisk = params->get("riskmon");  // 获取风险控制配置节点
	if (cfgRisk)  // 如果存在风险控制配置
	{
		if (cfgRisk->getBoolean("active"))  // 如果风险控制已激活
		{
			_risk_mon_enabled = true;  // 启用风险监控

			WTSVariant* cfgPolicy = cfgRisk->get("policy");  // 获取策略配置节点
			auto keys = cfgPolicy->memberNames();  // 获取所有策略键名
			for (auto it = keys.begin(); it != keys.end(); it++)  // 遍历所有策略
			{
				const char* product = (*it).c_str();  // 获取品种代码
				WTSVariant*	vProdItem = cfgPolicy->get(product);  // 获取品种配置项
				RiskParams& rParam = _risk_params_map[product];  // 获取或创建风险参数
				rParam._cancel_total_limits = vProdItem->getUInt32("cancel_total_limits");  // 设置撤单总限额
				rParam._cancel_times_boundary = vProdItem->getUInt32("cancel_times_boundary");  // 设置撤单频率边界值
				rParam._cancel_stat_timespan = vProdItem->getUInt32("cancel_stat_timespan");  // 设置撤单统计时间跨度

				rParam._order_total_limits = vProdItem->getUInt32("order_total_limits");  // 设置下单总限额
				rParam._order_times_boundary = vProdItem->getUInt32("order_times_boundary");  // 设置下单频率边界值
				rParam._order_stat_timespan = vProdItem->getUInt32("order_stat_timespan");  // 设置下单统计时间跨度

				WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Risk control rule {} of trading channel loaded", _id.c_str(), product);  // 记录信息日志
			}

			auto it = _risk_params_map.find("default");  // 查找默认风险参数
			if (it == _risk_params_map.end())  // 如果未找到默认参数
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] Some instruments may not be monitored due to no default risk control rule of trading channel", _id.c_str());  // 记录警告日志
			}
		}
		else  // 如果风险控制未激活
		{
			WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] Risk control rule of trading channel not activated", _id.c_str());  // 记录警告日志
		}
	}
	else  // 如果不存在风险控制配置
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] No risk control rule setup of trading channel", _id.c_str());  // 记录警告日志
	}

	if (params->getString("module").empty())  // 如果模块名称为空
		return false;  // 返回失败

	std::string module = DLLHelper::wrap_module(params->getCString("module"), "lib");;  // 包装模块名称

	//先看工作目录下是否有交易模块
	std::string dllpath = WtHelper::getModulePath(module.c_str(), "traders", true);  // 获取工作目录下的模块路径
	//如果没有,则再看模块目录,即dll同目录下
	if (!StdFile::exists(dllpath.c_str()))  // 如果文件不存在
		dllpath = WtHelper::getModulePath(module.c_str(), "traders", false);  // 获取模块目录下的路径
	DllHandle hInst = DLLHelper::load_library(dllpath.c_str());  // 加载动态库
	if (hInst == NULL)  // 如果加载失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Loading trading module %s failed", _id.c_str(), dllpath.c_str());  // 记录错误日志
		return false;  // 返回失败
	}
	else  // 如果加载成功
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Trader module {} loaded", _id.c_str(), dllpath.c_str());  // 记录信息日志
	}

	FuncCreateTrader pFunCreateTrader = (FuncCreateTrader)DLLHelper::get_symbol(hInst, "createTrader");  // 获取创建交易接口函数
	if (NULL == pFunCreateTrader)  // 如果函数不存在
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_FATAL, "[{}] Entrance function createTrader not found", _id.c_str());  // 记录致命错误日志
		return false;  // 返回失败
	}

	_trader_api = pFunCreateTrader();  // 创建交易接口实例
	if (NULL == _trader_api)  // 如果创建失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_FATAL, "[{}] Creating trading api failed", _id.c_str());  // 记录致命错误日志
		return false;  // 返回失败
	}

	_remover = (FuncDeleteTrader)DLLHelper::get_symbol(hInst, "deleteTrader");  // 获取删除交易接口函数

	if (!_trader_api->init(params))  // 如果初始化交易接口失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Entrance function deleteTrader not found", id);  // 记录错误日志
		return false;  // 返回失败
	}

	return true;  // 返回成功
}

/**
 * @brief 启动交易适配器
 * @return 启动成功返回true，失败返回false
 * 
 * 注册回调接口，连接交易服务器，开始登录流程。
 */
bool TraderAdapter::run()
{
	if (_trader_api == NULL)  // 如果交易接口不存在
		return false;  // 返回失败

	if (_stat_map == NULL)  // 如果统计映射表不存在
		_stat_map = TradeStatMap::create();  // 创建统计映射表

	_trader_api->registerSpi(this);  // 注册回调接口

	_trader_api->connect();  // 连接交易服务器
	_state = AS_LOGINING;  // 设置状态为正在登录
	return true;  // 返回成功
}

/**
 * @brief 释放交易适配器资源
 * 
 * 断开连接，释放交易接口资源。
 */
void TraderAdapter::release()
{
	if (_trader_api)  // 如果交易接口存在
	{
		_trader_api->registerSpi(NULL);  // 注销回调接口
		_trader_api->release();  // 释放交易接口资源
	}
}

/**
 * @brief 枚举持仓并通知接收器
 * @param stdCode 标准合约代码，空字符串表示枚举所有合约
 * @return 总持仓数量
 * 
 * 遍历持仓，通过回调函数通知所有接收器。
 * 使用回调方式，避免接口设计过于复杂。
 */
double TraderAdapter::enumPosition(const char* stdCode /* = "" */)
{
	/*
	 *	By Wesley @ 2022.03.19
	 *	这里改成回调的方式
	 *	不然接口会设计得很复杂
	 */
	double ret = 0;  // 总持仓数量
	bool bAll = (strlen(stdCode) == 0);  // 是否枚举所有合约
	for (auto it = _positions.begin(); it != _positions.end(); it++)  // 遍历所有持仓
	{
		if (!bAll && strcmp(it->first.c_str(), stdCode) != 0)  // 如果指定了合约代码且不匹配
			continue;  // 跳过

		const PosItem& pItem = it->second;  // 获取持仓项
		for (auto sink : _sinks)  // 遍历所有通知接收器
		{
			sink->on_position(stdCode, true, pItem.l_prevol, pItem.l_preavail, pItem.l_newvol, pItem.l_newavail, _trading_day);  // 通知多仓持仓
			sink->on_position(stdCode, false, pItem.s_prevol, pItem.s_preavail, pItem.s_newvol, pItem.s_newavail, _trading_day);  // 通知空仓持仓
		}
		ret += pItem.total_pos(true) + pItem.total_pos(false);  // 累加总持仓数量
	}

	return ret;  // 返回总持仓数量
}

/**
 * @brief 获取持仓数量
 * @param stdCode 标准合约代码
 * @param bValidOnly 是否只返回可用持仓，true表示只返回可用持仓，false表示返回全部持仓
 * @param flag 持仓标志：1-多仓，2-空仓，3-全部（默认）
 * @return 持仓数量，多仓为正，空仓为负
 */
double TraderAdapter::getPosition(const char* stdCode, bool bValidOnly, int32_t flag /* = 3 */)
{
	auto it = _positions.find(stdCode);  // 查找持仓项
	if (it == _positions.end())  // 如果未找到
		return 0;  // 返回0

	double ret = 0;  // 持仓数量
	const PosItem& pItem = it->second;  // 获取持仓项
	if(flag & 1)  // 如果包含多仓标志
	{
		if(bValidOnly)  // 如果只返回可用持仓
			ret += (pItem.l_newavail + pItem.l_preavail);  // 累加多可用持仓
		else  // 如果返回全部持仓
			ret += (pItem.l_newvol + pItem.l_prevol);  // 累加多总持仓
	}

	if (flag & 2)  // 如果包含空仓标志
	{
		if (bValidOnly)  // 如果只返回可用持仓
			ret -= (pItem.s_newavail + pItem.s_preavail);  // 减去空可用持仓
		else  // 如果返回全部持仓
			ret -= pItem.s_newvol + pItem.s_prevol;  // 减去空总持仓
	}
	return ret;  // 返回持仓数量
}

/**
 * @brief 获取订单列表
 * @param stdCode 标准合约代码，空字符串表示获取所有订单
 * @return 订单映射表指针，调用者需要释放
 */
OrderMap* TraderAdapter::getOrders(const char* stdCode)
{
	if (_orders == NULL)  // 如果订单列表为空
		return NULL;  // 返回空指针

	bool isAll = strlen(stdCode) == 0;  // 是否获取所有订单

	SpinLock lock(_mtx_orders);  // 获取订单列表锁
	OrderMap* ret = OrderMap::create();  // 创建订单映射表
	for (auto it = _orders->begin(); it != _orders->end(); it++)  // 遍历所有订单
	{
		uint32_t localid = it->first;  // 获取本地订单ID
		WTSOrderInfo* ordInfo = (WTSOrderInfo*)it->second;  // 获取订单信息

		if (isAll || strcmp(ordInfo->getCode(), stdCode) == 0)  // 如果获取所有订单或合约代码匹配
			ret->add(localid, ordInfo);  // 添加到结果映射表
	}
	return ret;  // 返回订单映射表
}

/**
 * @brief 更新未完成数量
 * @param stdCode 合约代码
 * @param qty 数量变化（正数表示增加，负数表示减少）
 * 
 * 内部方法，更新合约的未完成订单数量。
 */
void TraderAdapter::updateUndone(const char* stdCode, double qty)
{
	double& undone = _undone_qty[stdCode];  // 获取未完成数量引用
	double oldQty = undone;  // 保存旧数量（未使用）
	undone += qty;  // 更新未完成数量
}

/**
 * @brief 执行委托下单
 * @param entrust 委托单指针
 * @return 本地订单ID，失败返回UINT_MAX
 * 
 * 内部方法，处理委托单的下单逻辑，生成本地订单ID。
 */
uint32_t TraderAdapter::doEntrust(WTSEntrust* entrust)
{
	_trader_api->makeEntrustID(entrust->getEntrustID(), 64);  // 生成委托单ID

	const char* stdCode = entrust->getCode();  // 获取标准合约代码
	std::size_t pos = StrUtil::findFirst(entrust->getCode(), '.');  // 查找交易所和合约代码的分隔符位置
	entrust->setExchange(stdCode, pos);  // 设置交易所代码
	entrust->setCode(stdCode + pos + 1);  // 设置合约代码（去掉交易所前缀）
	//if(entrust->getContractInfo() == NULL)
	//{
	//	WTSContractInfo* cInfo = _bd_mgr->getContract(entrust->getCode(), entrust->getExchg());
	//	entrust->setContractInfo(cInfo);
	//}

	uint32_t localid = makeLocalOrderID();  // 生成本地订单ID
	char* usertag = entrust->getUserTag();  // 获取用户标签缓冲区
	wt_strcpy(usertag, _order_pattern.c_str(), _order_pattern.size());  // 复制订单模式前缀
	usertag[_order_pattern.size()] = '.';  // 设置分隔符
	fmtutil::format_to(usertag + _order_pattern.size() + 1, "{}", localid);  // 格式化本地订单ID
	
	int32_t ret = _trader_api->orderInsert(entrust);  // 调用交易接口下单
	if(ret < 0)  // 如果下单失败
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] Order placing failed: {}", _id, ret);  // 记录错误日志
		return UINT_MAX;  // 返回失败标志
	}
	//else if(_risk_mon_enabled)
	//{
	//	int64_t now = TimeUtils::getLocalTimeNow();
	//	_order_time_cache[entrust->getCode()].emplace_back(now);
	//}
	return localid;  // 返回本地订单ID
}

/**
 * @brief 获取合约信息
 * @param stdCode 标准合约代码
 * @return 合约信息指针
 * 
 * 内部方法，从标准合约代码获取合约信息。
 */
WTSContractInfo* TraderAdapter::getContract(const char* stdCode)
{
	char buf[64] = { 0 };  // 创建缓冲区
	wt_strcpy(buf, stdCode);  // 复制标准合约代码
	auto idx = StrUtil::findFirst(buf, '.');  // 查找交易所和合约代码的分隔符位置
	buf[idx] = '\0';  // 设置分隔符为字符串结束符
	return _bd_mgr->getContract(buf + idx + 1, buf);  // 从基础数据管理器获取合约信息
}

/**
 * @brief 执行撤单
 * @param ordInfo 订单信息指针
 * @return 撤单成功返回true，失败返回false
 * 
 * 内部方法，处理订单的撤单逻辑。
 */
bool TraderAdapter::doCancel(WTSOrderInfo* ordInfo)
{
	if (ordInfo == NULL || !ordInfo->isAlive())  // 如果订单信息为空或订单已结束
		return false;  // 返回失败

	WTSContractInfo* cInfo = ordInfo->getContractInfo();  // 获取合约信息
	if(cInfo == NULL)  // 如果合约信息为空
		cInfo = _bd_mgr->getContract(ordInfo->getCode(), ordInfo->getExchg());  // 从基础数据管理器获取合约信息

	//撤单频率检查
	//if (_risk_mon_enabled && !checkCancelLimits(ordInfo->getCode()))  // 如果启用风险监控且撤单频率超限
	//	return false;  // 返回失败

	WTSEntrustAction* action = WTSEntrustAction::create(ordInfo->getCode(), cInfo->getExchg());  // 创建撤单动作
	action->setEntrustID(ordInfo->getEntrustID());  // 设置委托单ID
	action->setOrderID(ordInfo->getOrderID());  // 设置订单ID
	int ret = _trader_api->orderAction(action);  // 调用交易接口撤单
	bool isSent = (ret >= 0);  // 判断是否发送成功
	action->release();  // 释放撤单动作
	return isSent;  // 返回是否发送成功
}

/**
 * @brief 撤单
 * @param localid 本地订单ID
 * @return 撤单成功返回true，失败返回false
 */
bool TraderAdapter::cancel(uint32_t localid)
{
	if (_orders == NULL || _orders->size() == 0)  // 如果订单列表为空
		return false;  // 返回失败

	WTSOrderInfo* ordInfo = NULL;  // 订单信息指针
	{
		SpinLock lock(_mtx_orders);  // 获取订单列表锁
		ordInfo = (WTSOrderInfo*)_orders->grab(localid);  // 获取订单信息
		if (ordInfo == NULL)  // 如果订单不存在
			return false;  // 返回失败
	}
	
	bool bRet = doCancel(ordInfo);  // 执行撤单

	//if(_risk_mon_enabled)  // 如果启用风险监控
	//	_cancel_time_cache[ordInfo->getCode()].emplace_back(TimeUtils::getLocalTimeNow());  // 记录撤单时间

	ordInfo->release();  // 释放订单信息

	return bRet;  // 返回撤单结果
}

/**
 * @brief 全部撤单
 * @param stdCode 标准合约代码，空字符串表示撤所有订单
 * @return 订单ID列表
 */
OrderIDs TraderAdapter::cancelAll(const char* stdCode)
{
	OrderIDs ret;  // 订单ID列表

	double actQty = 0;  // 实际数量（未使用）
	bool isAll = strlen(stdCode) == 0;  // 是否撤所有订单
	if (_orders != NULL && _orders->size() > 0)  // 如果订单列表存在且不为空
	{
		for (auto it = _orders->begin(); it != _orders->end(); it++)  // 遍历所有订单
		{
			WTSOrderInfo* orderInfo = (WTSOrderInfo*)it->second;  // 获取订单信息
			if(!orderInfo->isAlive())  // 如果订单已结束
				continue;  // 跳过

			WTSContractInfo* cInfo = orderInfo->getContractInfo();  // 获取合约信息
			if (isAll || strcmp(stdCode, cInfo->getFullCode()) == 0)  // 如果撤所有订单或合约代码匹配
			{
				if(doCancel(orderInfo))  // 如果撤单成功
				{
					ret.emplace_back(it->first);  // 添加订单ID到列表
					//if (_risk_mon_enabled)  // 如果启用风险监控
					//	_cancel_time_cache[cInfo->getCode()].emplace_back(TimeUtils::getLocalTimeNow());  // 记录撤单时间
				}
			}
		}
	}

	return ret;  // 返回订单ID列表
}

/**
 * @brief 获取交易统计信息数量
 * @param stdCode 标准合约代码
 * @return 统计信息数量
 */
uint32_t TraderAdapter::getInfos(const char* stdCode)
{
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);  // 获取交易统计信息
	if (statInfo == NULL)  // 如果统计信息不存在
		return 0;  // 返回0

	return statInfo->infos();  // 返回统计信息数量
}

/**
 * @brief 买入操作（根据策略规则转换为实际订单）
 * @param stdCode 标准合约代码
 * @param price 价格，0表示市价单
 * @param qty 数量
 * @param flag 下单标志：0-normal，1-fak，2-fok
 * @param bForceClose 是否强制平仓
 * @param cInfo 合约信息指针，可为NULL
 * @return 订单ID列表
 * 
 * 根据动作策略规则，将买入信号转换为实际的开多或平空订单。
 * 策略规则包括：开仓、平今、平昨、平仓等。
 * 根据品种的平仓模式（是否区分平昨平今）和持仓情况，选择合适的策略规则执行。
 */
OrderIDs TraderAdapter::buy(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo /* = NULL */)
{
	OrderIDs ret;  // 订单ID列表
	if (qty == 0)  // 如果数量为0
		return ret;  // 返回空列表

	//if(_risk_mon_enabled && !checkOrderLimits(stdCode))  // 如果启用风险监控且下单频率超限
	//{
	//	WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "{} is forbidden to trade", stdCode);  // 记录警告日志
	//	return ret;  // 返回空列表
	//}

	if (cInfo == NULL) cInfo = getContract(stdCode);  // 如果合约信息为空，则获取合约信息
	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取品种信息

	WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG, "[{}] Buying {} of quantity {}", _id.c_str(), stdCode, qty);  // 记录调试日志

	const PosItem& pItem = _positions[stdCode];  // 获取持仓项
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);  // 获取交易统计信息
	if (statInfo == NULL)  // 如果统计信息不存在
	{
		statInfo = WTSTradeStateInfo::create(stdCode);  // 创建统计信息
		_stat_map->add(stdCode, statInfo, false);  // 添加到统计映射表
	}
	TradeStatInfo& statItem = statInfo->statInfo();  // 获取统计信息引用
	const ActionRuleGroup& ruleGP = _policy_mgr->getActionRules(commInfo->getFullPid());  // 获取动作策略规则组

	double left = qty;  // 剩余数量

	double unitQty = (price == 0.0) ? cInfo->getMaxMktVol() : cInfo->getMaxLmtVol();  // 获取单笔最大委托数量（市价单或限价单）
	if (decimal::eq(unitQty, 0))  // 如果单笔最大委托数量为0
		unitQty = DBL_MAX;  // 设置为最大值

	for (auto it = ruleGP.begin(); it != ruleGP.end(); it++)  // 遍历所有策略规则
	{
		const ActionRule& curRule = (*it);  // 获取当前规则
		if (curRule._atype == AT_Open && !bForceClose)  // 如果是开仓规则且不强制平仓
		{
			//先检查是否已经到了限额
			//买入开仓, 即开多仓
			double maxQty = left;  // 最大数量初始化为剩余数量

			if (curRule._limit_l != 0)  // 如果多仓限额不为0
			{
				if (statItem.l_openvol >= curRule._limit_l)  // 如果今日多仓开仓量已达到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} long position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit_l);  // 记录警告日志
					continue;  // 跳过该规则
				}
				else  // 如果未达到限额
				{
					maxQty = min(maxQty, curRule._limit_l - statItem.l_openvol);  // 计算最大可开仓数量
				}
			}

			if (curRule._limit != 0)  // 如果总限额不为0
			{
				if (statItem.l_openvol + statItem.s_openvol >= curRule._limit)  // 如果今日总开仓量已达到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit);  // 记录警告日志
					continue;  // 跳过该规则
				}
				else  // 如果未达到限额
				{
					maxQty = min(maxQty, curRule._limit - statItem.l_openvol - statItem.s_openvol);  // 计算最大可开仓数量
				}
			}

			//这里还要考虑单笔最大委托数量
			double leftQty = maxQty;  // 剩余数量
			for (;;)  // 循环拆分订单
			{
				double curQty = min(leftQty, unitQty);  // 当前订单数量
				uint32_t localid = openLong(stdCode, price, curQty, flag);  // 开多单
				ret.emplace_back(localid);  // 添加订单ID到列表

				leftQty -= curQty;  // 减少剩余数量

				if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
					break;  // 跳出循环
			}

			left -= maxQty;  // 减少剩余数量

			WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
				"[{}] Signal of buying {} of quantity {} triggered: Opening long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
		}
		else if (curRule._atype == AT_CloseToday)  // 如果是平今规则
		{
			double maxQty = 0;  // 最大数量
			//如果要区分平昨平今的品种, 则只读取可平今仓即可
			//如果不区分平昨平今的品种, 则读取全部可平, 因为读取可平今仓也没意义
			if (commInfo->getCoverMode() == CM_CoverToday)  // 如果品种区分平昨平今
				maxQty = min(left, pItem.s_newavail);	//先看看可平今仓
			else  // 如果品种不区分平昨平今
				maxQty = min(left, pItem.avail_pos(false));  // 读取全部可平空仓


			//如果要检查净今仓，但是昨仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.s_prevol, 0.0))  // 如果检查净今仓且昨仓不为0
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing new short position of {} skipped because of non-zero pre short position", _id.c_str(), stdCode);  // 记录警告日志
				continue;  // 跳过该规则
			}

			//这里还要考虑单笔最大委托数量
			//if (maxQty > 0)
			if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
			{
				double leftQty = maxQty;  // 剩余数量
				for (;;)  // 循环拆分订单
				{
					double curQty = min(leftQty, unitQty);  // 当前订单数量
					uint32_t localid = closeShort(stdCode, price, curQty, (commInfo->getCoverMode() == CM_CoverToday), flag);//如果不支持平今, 则直接下平仓标记即可
					ret.emplace_back(localid);  // 添加订单ID到列表

					leftQty -= curQty;  // 减少剩余数量

					//if (leftQty == 0)
					if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
						break;  // 跳出循环
				}
				left -= maxQty;  // 减少剩余数量

				if (commInfo->getCoverMode() == CM_CoverToday)  // 如果品种区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing new short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
				else  // 如果品种不区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
			}

		}
		else if (curRule._atype == AT_CloseYestoday)  // 如果是平昨规则
		{
			//平昨比较简单, 因为不需要区分标记
			double maxQty = min(left, pItem.s_preavail);  // 计算最大可平昨仓数量

			//如果要检查净昨仓，但是今仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.s_newvol, 0.0))  // 如果检查净昨仓且今仓不为0
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing pre short position of {} skipped because of non-zero new short position", _id.c_str(), stdCode);  // 记录警告日志
				continue;  // 跳过该规则
			}

			//这里还要考虑单笔最大委托数量
			//if (maxQty > 0)
			if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
			{
				double leftQty = maxQty;  // 剩余数量
				for (;;)  // 循环拆分订单
				{
					double curQty = min(leftQty, unitQty);  // 当前订单数量
					uint32_t localid = closeShort(stdCode, price, curQty, false, flag);//如果不支持平今, 则直接下平仓标记即可
					ret.emplace_back(localid);  // 添加订单ID到列表

					leftQty -= curQty;  // 减少剩余数量

					//if (leftQty == 0)
					if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
						break;  // 跳出循环
				}

				left -= maxQty;  // 减少剩余数量

				if (commInfo->getCoverMode() == CM_CoverToday)  // 如果品种区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing old short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
				else  // 如果品种不区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
			}
		}
		else if (curRule._atype == AT_Close)  // 如果是平仓规则
		{
			//如果只是平仓, 则分情况处理
			//如果区分平昨平今, 则要先平昨再平今
			//如果不区分平昨平今, 则统一平仓
			if (commInfo->getCoverMode() != CM_CoverToday)  // 如果品种不区分平昨平今
			{
				double maxQty = min(pItem.avail_pos(false), left);  // 计算最大可平仓数量

				//if (maxQty > 0)
				if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
				{
					double leftQty = maxQty;  // 剩余数量
					for (;;)  // 循环拆分订单
					{
						double curQty = min(leftQty, unitQty);  // 当前订单数量
						uint32_t localid = closeShort(stdCode, price, curQty, false, flag);  // 平空单
						ret.emplace_back(localid);  // 添加订单ID到列表

						leftQty -= curQty;  // 减少剩余数量

						//if (leftQty == 0)
						if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
							break;  // 跳出循环
					}
					left -= maxQty;  // 减少剩余数量

					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
			}
			else  // 如果品种区分平昨平今
			{
				//if (pItem.s_preavail > 0)
				if (decimal::gt(pItem.s_preavail, 0))  // 如果可平昨仓大于0
				{
					//先将可平昨仓平仓
					double maxQty = min(pItem.s_preavail, qty);  // 计算最大可平昨仓数量
					double leftQty = maxQty;  // 剩余数量
					for (;;)  // 循环拆分订单
					{
						double curQty = min(leftQty, unitQty);  // 当前订单数量
						uint32_t localid = closeShort(stdCode, price, curQty, false, flag);  // 平昨空单
						ret.emplace_back(localid);  // 添加订单ID到列表

						leftQty -= curQty;  // 减少剩余数量

						//if (leftQty == 0)
						if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
							break;  // 跳出循环
					}
					left -= maxQty;  // 减少剩余数量

					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing old short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}

				//if (left > 0 && pItem.s_newavail > 0)
				if (decimal::gt(left, 0) && decimal::gt(pItem.s_newavail, 0))  // 如果还有剩余数量且可平今仓大于0
				{
					//再将可平今仓平仓
					//TODO: 这里还有一个控制, 就是强制锁今仓的话, 这段逻辑就跳过去了
					double maxQty = min(pItem.s_newavail, left);  // 计算最大可平今仓数量
					double leftQty = maxQty;  // 剩余数量
					for (;;)  // 循环拆分订单
					{
						double curQty = min(leftQty, unitQty);  // 当前订单数量
						uint32_t localid = closeShort(stdCode, price, curQty, true, flag);  // 平今空单
						ret.emplace_back(localid);  // 添加订单ID到列表

						leftQty -= curQty;  // 减少剩余数量

						//if (leftQty == 0)
						if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
							break;  // 跳出循环
					}
					left -= maxQty;  // 减少剩余数量

					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of buying {} of quantity {} triggered: Closing new short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
			}
		}

		if (left == 0)  // 如果剩余数量为0
			break;  // 跳出循环
	}

	if (left > 0)  // 如果还有剩余数量未处理
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,
			"[{}] Signal of buying {} of quantity {} left quantity of {} not triggered", _id.c_str(), stdCode, qty, left);  // 记录错误日志
	}

	return ret;  // 返回订单ID列表
}

/**
 * @brief 卖出操作（根据策略规则转换为实际订单）
 * @param stdCode 标准合约代码
 * @param price 价格，0表示市价单
 * @param qty 数量
 * @param flag 下单标志：0-normal，1-fak，2-fok
 * @param bForceClose 是否强制平仓
 * @param cInfo 合约信息指针，可为NULL
 * @return 订单ID列表
 * 
 * 根据动作策略规则，将卖出信号转换为实际的开空或平多订单。
 * 策略规则包括：开仓、平今、平昨、平仓等。
 * 根据品种的平仓模式（是否区分平昨平今）和持仓情况，选择合适的策略规则执行。
 */
OrderIDs TraderAdapter::sell(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo /* = NULL */)
{
	OrderIDs ret;  // 订单ID列表
	if (qty == 0)  // 如果数量为0
		return ret;  // 返回空列表

	//if (_risk_mon_enabled && !checkOrderLimits(stdCode))  // 如果启用风险监控且下单频率超限
	//{
	//	WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "{} is forbidden to trade", stdCode);  // 记录警告日志
	//	return ret;  // 返回空列表
	//}

	if (cInfo == NULL) cInfo = getContract(stdCode);  // 如果合约信息为空，则获取合约信息
	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取品种信息

	const PosItem& pItem = _positions[stdCode];  // 获取持仓项
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);  // 获取交易统计信息
	if (statInfo == NULL)  // 如果统计信息不存在
	{
		statInfo = WTSTradeStateInfo::create(stdCode);  // 创建统计信息
		_stat_map->add(stdCode, statInfo, false);  // 添加到统计映射表
	}
	TradeStatInfo& statItem = statInfo->statInfo();  // 获取统计信息引用

	const ActionRuleGroup& ruleGP = _policy_mgr->getActionRules(commInfo->getFullPid());  // 获取动作策略规则组

	double left = qty;  // 剩余数量

	double unitQty = (price == 0.0) ? cInfo->getMaxMktVol() : cInfo->getMaxLmtVol();  // 获取单笔最大委托数量（市价单或限价单）
	if (decimal::eq(unitQty, 0))  // 如果单笔最大委托数量为0
		unitQty = DBL_MAX;  // 设置为最大值

	for (auto it = ruleGP.begin(); it != ruleGP.end(); it++)  // 遍历所有策略规则
	{
		const ActionRule& curRule = (*it);  // 获取当前规则
		if (curRule._atype == AT_Open && !bForceClose)  // 如果是开仓规则且不强制平仓
		{
			//先检查是否已经到了限额
			//卖出开仓, 即开空仓
			double maxQty = left;  // 最大数量初始化为剩余数量

			if (curRule._limit_s != 0)  // 如果空仓限额不为0
			{
				if (statItem.s_openvol >= curRule._limit_s)  // 如果今日空仓开仓量已达到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} short position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit_l);  // 记录警告日志
					continue;  // 跳过该规则
				}
				else  // 如果未达到限额
				{
					maxQty = min(maxQty, curRule._limit_s - statItem.s_openvol);  // 计算最大可开仓数量
				}
			}

			if (curRule._limit != 0)  // 如果总限额不为0
			{
				if (statItem.l_openvol + statItem.s_openvol >= curRule._limit)  // 如果今日总开仓量已达到限额
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "[{}] {} position opened today is up to limit {}", _id.c_str(), stdCode, curRule._limit);  // 记录警告日志
					continue;  // 跳过该规则
				}
				else  // 如果未达到限额
				{
					maxQty = min(maxQty, curRule._limit - statItem.l_openvol - statItem.s_openvol);  // 计算最大可开仓数量
				}
			}

			//这里还要考虑单笔最大委托数量
			double leftQty = maxQty;  // 剩余数量
			for (;;)  // 循环拆分订单
			{
				double curQty = min(leftQty, unitQty);  // 当前订单数量
				uint32_t localid = openShort(stdCode, price, curQty, flag);  // 开空单
				ret.emplace_back(localid);  // 添加订单ID到列表

				leftQty -= curQty;  // 减少剩余数量

				if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
					break;  // 跳出循环
			}

			left -= maxQty;  // 减少剩余数量

			WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
				"[{}] Signal of selling {} of quantity {} triggered: Opening short of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
		}
		else if (curRule._atype == AT_CloseToday)  // 如果是平今规则
		{
			double maxQty = 0;  // 最大数量
			//如果要区分平昨平今的品种, 则只读取可平今仓即可
			//如果不区分平昨平今的品种, 则读取全部可平, 因为读取可平今仓也没意义
			if (commInfo->getCoverMode() == CM_CoverToday)  // 如果品种区分平昨平今
				maxQty = min(left, pItem.l_newavail);	//先看看可平今仓
			else  // 如果品种不区分平昨平今
				maxQty = min(left, pItem.avail_pos(true));  // 读取全部可平多仓

			//如果要检查净今仓，但是昨仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.l_prevol, 0.0))  // 如果检查净今仓且昨仓不为0
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing new long position of {} skipped because of non-zero pre long position", _id.c_str(), stdCode);  // 记录警告日志
				continue;  // 跳过该规则
			}

			//这里还要考虑单笔最大委托数量
			if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
			{
				double leftQty = maxQty;  // 剩余数量
				for (;;)  // 循环拆分订单
				{
					double curQty = min(leftQty, unitQty);  // 当前订单数量
					uint32_t localid = closeLong(stdCode, price, curQty, (commInfo->getCoverMode() == CM_CoverToday), flag);//如果不支持平今, 则直接下平仓标记即可
					ret.emplace_back(localid);  // 添加订单ID到列表

					leftQty -= curQty;  // 减少剩余数量

					if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
						break;  // 跳出循环
				}
				left -= maxQty;  // 减少剩余数量

				if (commInfo->getCoverMode() == CM_CoverToday)  // 如果品种区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of selling {} of quantity {} triggered: Closing new long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
				else  // 如果品种不区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of selling {} of quantity {} triggered: Closing long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
			}

		}
		else if (curRule._atype == AT_CloseYestoday)  // 如果是平昨规则
		{
			//平昨比较简单, 因为不需要区分标记
			double maxQty = min(left, pItem.l_preavail);  // 计算最大可平昨仓数量

			//如果要检查净昨仓，但是今仓不为0，则跳过该条规则
			if (!bForceClose && curRule._pure && !decimal::eq(pItem.l_newvol, 0.0))  // 如果检查净昨仓且今仓不为0
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN,
					"[{}] Closing pre long position of {} skipped because of non-zero new long position", _id.c_str(), stdCode);  // 记录警告日志
				continue;  // 跳过该规则
			}

			//这里还要考虑单笔最大委托数量
			if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
			{
				double leftQty = maxQty;  // 剩余数量
				for (;;)  // 循环拆分订单
				{
					double curQty = min(leftQty, unitQty);  // 当前订单数量
					uint32_t localid = closeLong(stdCode, price, curQty, false, flag);//如果不支持平今, 则直接下平仓标记即可
					ret.emplace_back(localid);  // 添加订单ID到列表

					leftQty -= curQty;  // 减少剩余数量

					if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
						break;  // 跳出循环
				}
				left -= maxQty;  // 减少剩余数量

				if (commInfo->getCoverMode() == CM_CoverToday)  // 如果品种区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of selling {} of quantity {} triggered: Closing old long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
				else  // 如果品种不区分平昨平今
				{
					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of selling {} of quantity {} triggered: Closing long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}
			}
		}
		else if (curRule._atype == AT_Close)  // 如果是平仓规则
		{
			//如果只是平仓, 则分情况处理
			//如果区分平昨平今, 则要先平昨再平今
			//如果不区分平昨平今, 则统一平仓
			if (commInfo->getCoverMode() != CM_CoverToday)  // 如果品种不区分平昨平今
			{
				double maxQty = min(pItem.avail_pos(true), left);	//不区分平昨平今, 则读取全部可平量
				if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
				{
					double leftQty = maxQty;  // 剩余数量
					for (;;)  // 循环拆分订单
					{
						double curQty = min(leftQty, unitQty);  // 当前订单数量
						uint32_t localid = closeLong(stdCode, price, curQty, false, flag);  // 平多单
						ret.emplace_back(localid);  // 添加订单ID到列表

						leftQty -= curQty;  // 减少剩余数量

						if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
							break;  // 跳出循环
					}
					left -= maxQty;  // 减少剩余数量

					WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
						"[{}] Signal of selling {} of quantity {} triggered: Closing long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
				}

			}
			else  // 如果品种区分平昨平今
			{
				if (decimal::gt(left, 0) && decimal::gt(pItem.l_preavail, 0))  // 如果还有剩余数量且可平昨仓大于0
				{
					//先将可平昨仓平仓
					double maxQty = min(pItem.l_preavail, qty);  // 计算最大可平昨仓数量
					if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
					{
						double leftQty = maxQty;  // 剩余数量
						for (;;)  // 循环拆分订单
						{
							double curQty = min(leftQty, unitQty);  // 当前订单数量
							uint32_t localid = closeLong(stdCode, price, curQty, false, flag);  // 平昨多单
							ret.emplace_back(localid);  // 添加订单ID到列表

							leftQty -= curQty;  // 减少剩余数量

							if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
								break;  // 跳出循环
						}
						left -= maxQty;  // 减少剩余数量

						WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
							"[{}] Signal of selling {} of quantity {} triggered: Closing old long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
					}

				}

				if (decimal::gt(left, 0) && decimal::gt(pItem.l_newavail, 0))  // 如果还有剩余数量且可平今仓大于0
				{
					//再将可平今仓平仓
					//TODO: 这里还有一个控制, 就是强制锁今仓的话, 这段逻辑就跳过去了
					double maxQty = min(pItem.l_newavail, left);  // 计算最大可平今仓数量
					if (decimal::gt(maxQty, 0))  // 如果最大数量大于0
					{
						double leftQty = maxQty;  // 剩余数量
						for (;;)  // 循环拆分订单
						{
							double curQty = min(leftQty, unitQty);  // 当前订单数量
							uint32_t localid = closeLong(stdCode, price, curQty, true, flag);  // 平今多单
							ret.emplace_back(localid);  // 添加订单ID到列表

							leftQty -= curQty;  // 减少剩余数量

							if (decimal::eq(leftQty, 0))  // 如果剩余数量为0
								break;  // 跳出循环
						}
						left -= maxQty;  // 减少剩余数量

						WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
							"[{}] Signal of selling {} of quantity {} triggered: Closing new long of quantity {}", _id.c_str(), stdCode, qty, maxQty);  // 记录调试日志
					}

				}
			}
		}

		if (left == 0)  // 如果剩余数量为0
			break;  // 跳出循环
	}

	if (left > 0)  // 如果还有剩余数量未处理
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,
			"[{}] Signal of buying {} of quantity {} left quantity of {} not triggered", _id.c_str(), stdCode, qty, left);  // 记录错误日志
	}

	return ret;  // 返回订单ID列表
}


/**
 * @brief 开多单
 * @param stdCode 标准合约代码
 * @param price 价格，0表示市价单
 * @param qty 数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 本地订单ID，失败返回0
 */
uint32_t TraderAdapter::openLong(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	//if (_risk_mon_enabled && !checkOrderLimits(stdCode))  // 如果启用风险监控且下单频率超限
	//{
	//	WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "{} is forbidden to trade", stdCode);  // 记录警告日志
	//	return 0;  // 返回失败
	//}

	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);  // 创建委托单
	if(price == 0.0)  // 如果价格为0
	{
		entrust->setPriceType(WPT_ANYPRICE);  // 设置价格类型为市价单
	}
	else  // 如果价格不为0
	{
		entrust->setPriceType(WPT_LIMITPRICE);  // 设置价格类型为限价单
	}
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));  // 设置下单标志

	entrust->setDirection(WDT_LONG);  // 设置方向为多仓
	entrust->setOffsetType(WOT_OPEN);  // 设置开平类型为开仓

	updateUndone(stdCode, qty);  // 更新未完成数量

	uint32_t ret = doEntrust(entrust);  // 执行委托下单
	entrust->release();  // 释放委托单
	return ret;  // 返回本地订单ID
}

/**
 * @brief 开空单
 * @param stdCode 标准合约代码
 * @param price 价格，0表示市价单
 * @param qty 数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 本地订单ID，失败返回0
 */
uint32_t TraderAdapter::openShort(const char* stdCode, double price, double qty, int flag/* = 0*/)
{
	//if (_risk_mon_enabled && !checkOrderLimits(stdCode))  // 如果启用风险监控且下单频率超限
	//{
	//	WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "{} is forbidden to trade", stdCode);  // 记录警告日志
	//	return 0;  // 返回失败
	//}

	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);  // 创建委托单
	if (price == 0.0)  // 如果价格为0
	{
		entrust->setPriceType(WPT_ANYPRICE);  // 设置价格类型为市价单
	}
	else  // 如果价格不为0
	{
		entrust->setPriceType(WPT_LIMITPRICE);  // 设置价格类型为限价单
	}
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));  // 设置下单标志

	entrust->setDirection(WDT_SHORT);  // 设置方向为空仓
	entrust->setOffsetType(WOT_OPEN);  // 设置开平类型为开仓

	updateUndone(stdCode, qty);  // 更新未完成数量

	uint32_t ret = doEntrust(entrust);  // 执行委托下单
	entrust->release();  // 释放委托单
	return ret;  // 返回本地订单ID
}

/**
 * @brief 平多单
 * @param stdCode 标准合约代码
 * @param price 价格，0表示市价单
 * @param qty 数量
 * @param isToday 是否平今仓，默认false
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 本地订单ID，失败返回0
 */
uint32_t TraderAdapter::closeLong(const char* stdCode, double price, double qty, bool isToday /* = false */, int flag/* = 0*/)
{
	//if (_risk_mon_enabled && !checkOrderLimits(stdCode))  // 如果启用风险监控且下单频率超限
	//{
	//	WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "{} is forbidden to trade", stdCode);  // 记录警告日志
	//	return 0;  // 返回失败
	//}

	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);  // 创建委托单
	if (price == 0.0)  // 如果价格为0
	{
		entrust->setPriceType(WPT_ANYPRICE);  // 设置价格类型为市价单
	}
	else  // 如果价格不为0
	{
		entrust->setPriceType(WPT_LIMITPRICE);  // 设置价格类型为限价单
	}
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));  // 设置下单标志

	entrust->setDirection(WDT_LONG);  // 设置方向为多仓
	entrust->setOffsetType(isToday ? WOT_CLOSETODAY : WOT_CLOSE);  // 设置开平类型（平今或平仓）

	updateUndone(stdCode, qty);  // 更新未完成数量

	uint32_t ret = doEntrust(entrust);  // 执行委托下单
	entrust->release();  // 释放委托单
	return ret;  // 返回本地订单ID
}

/**
 * @brief 平空单
 * @param stdCode 标准合约代码
 * @param price 价格，0表示市价单
 * @param qty 数量
 * @param isToday 是否平今仓，默认false
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 本地订单ID，失败返回0
 */
uint32_t TraderAdapter::closeShort(const char* stdCode, double price, double qty, bool isToday /* = false */, int flag/* = 0*/)
{
	//if (_risk_mon_enabled && !checkOrderLimits(stdCode))  // 如果启用风险监控且下单频率超限
	//{
	//	WTSLogger::log_dyn("trader", _id.c_str(), LL_WARN, "{} is forbidden to trade", stdCode);  // 记录警告日志
	//	return 0;  // 返回失败
	//}

	WTSEntrust* entrust = WTSEntrust::create(stdCode, qty, price);  // 创建委托单
	if (price == 0.0)  // 如果价格为0
	{
		entrust->setPriceType(WPT_ANYPRICE);  // 设置价格类型为市价单
	}
	else  // 如果价格不为0
	{
		entrust->setPriceType(WPT_LIMITPRICE);  // 设置价格类型为限价单
	}
	entrust->setOrderFlag((WTSOrderFlag)(WOF_NOR + flag));  // 设置下单标志

	entrust->setDirection(WDT_SHORT);  // 设置方向为空仓
	entrust->setOffsetType(isToday ? WOT_CLOSETODAY : WOT_CLOSE);  // 设置开平类型（平今或平仓）

	updateUndone(stdCode, qty);  // 更新未完成数量

	uint32_t ret = doEntrust(entrust);  // 执行委托下单
	entrust->release();  // 释放委托单
	return ret;  // 返回本地订单ID
}


#pragma region "ITraderSpi接口"
/**
 * @brief 处理交易事件
 * @param e 交易事件类型
 * @param ec 事件代码
 * 
 * 实现ITraderSpi接口，处理交易接口的事件回调。
 * 处理连接成功、连接失败、连接断开等事件。
 */
void TraderAdapter::handleEvent(WTSTraderEvent e, int32_t ec)
{
	if(e == WTE_Connect)  // 如果是连接事件
	{
		if(ec == 0)  // 如果连接成功
		{
			_trader_api->login(_cfg->getCString("user"), _cfg->getCString("pass"), _cfg->getCString("product"));  // 登录交易接口
		}
		else  // 如果连接失败
		{
			WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Trading channel connecting failed: {}", _id.c_str(), ec);  // 记录错误日志
		}
	}
	else if(e == WTE_Close)  // 如果是断开连接事件
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Trading channel disconnected: {}", _id.c_str(), ec);  // 记录错误日志
		for (auto sink : _sinks)  // 遍历所有通知接收器
			sink->on_channel_lost();  // 通知通道丢失
	}
}

/**
 * @brief 登录结果回调
 * @param bSucc 登录是否成功
 * @param msg 消息
 * @param tradingdate 交易日
 * 
 * 实现ITraderSpi接口，处理登录结果回调。
 * 登录成功后自动查询持仓、订单、成交。
 */
void TraderAdapter::onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate)
{
	if(!bSucc)  // 如果登录失败
	{
		_state = AS_LOGINFAILED;  // 设置状态为登录失败
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Trader login failed: {}", _id.c_str(), msg);  // 记录错误日志
	}
	else  // 如果登录成功
	{
		_state = AS_LOGINED;  // 设置状态为已登录
		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,"[{}] Trader login succeed, trading date: {}", _id.c_str(), tradingdate);  // 记录信息日志
		_trading_day = tradingdate;  // 保存交易日
		_trader_api->queryPositions();	//查持仓
	}
}

/**
 * @brief 登出回调
 * 
 * 实现ITraderSpi接口，处理登出回调。
 */
void TraderAdapter::onLogout()
{
	
}

/**
 * @brief 委托回报回调
 * @param entrust 委托单指针
 * @param err 错误信息指针
 * 
 * 实现ITraderSpi接口，处理委托回报。
 * 如果委托失败，更新未完成数量并通知接收器。
 */
void TraderAdapter::onRspEntrust(WTSEntrust* entrust, WTSError *err)
{
	if (err && err->getErrorCode() != WEC_NONE)  // 如果存在错误
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,err->getMessage());  // 记录错误日志
		WTSContractInfo* cInfo = _bd_mgr->getContract(entrust->getCode(), entrust->getExchg());  // 获取合约信息
		if (cInfo == NULL)  // 如果合约信息为空
			return;  // 返回

		std::string stdCode = cInfo->getFullCode();  // 获取标准合约代码

		bool isLong = (entrust->getDirection() == WDT_LONG);  // 是否多仓
		bool isToday = (entrust->getOffsetType() == WOT_CLOSETODAY);  // 是否平今
		bool isOpen = (entrust->getOffsetType() == WOT_OPEN);  // 是否开仓
		double qty = entrust->getVolume();  // 获取数量

		std::string action;  // 动作字符串
		if (isOpen)  // 如果是开仓
			action = "open ";  // 设置为"open "
		else if (isToday)  // 如果是平今
			action = "closetoday ";  // 设置为"closetoday "
		else  // 如果是平仓
			action = "close ";  // 设置为"close "
		action += isLong ? "long" : "short";  // 添加方向

		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, 
			"[{}] Order placing failed: {}, instrument: {}, action: {}, qty: {}", _id.c_str(), err->getMessage(), entrust->getCode(), action.c_str(), qty);  // 记录错误日志

		//如果下单失败, 要更新未完成数量
		//实盘中发现错误单有时候会推送两次
		//所以这里加一个检查未完成单的逻辑
		//如果有错单，正常情况下未完成单一定不为0
		//如果未完成订单为0，则说明这一次是重复通知，则不再处理了
		double oldQty = _undone_qty[stdCode];  // 获取旧未完成数量
		if (decimal::eq(oldQty, 0))  // 如果未完成数量为0
			return;  // 返回（避免重复处理）

		updateUndone(stdCode.c_str(), -qty);  // 更新未完成数量（减少）

		if (strlen(entrust->getUserTag()) > 0)  // 如果用户标签不为空
		{
			char* userTag = (char*)entrust->getUserTag();  // 获取用户标签
			userTag += _order_pattern.size() + 1;  // 跳过订单模式前缀
			uint32_t localid = strtoul(userTag, NULL, 10);  // 解析本地订单ID

			for(auto sink : _sinks)  // 遍历所有通知接收器
				sink->on_entrust(localid, stdCode.c_str(), false, err->getMessage());  // 通知委托失败
		}
	}
}

/**
 * @brief 账户查询回调
 * @param ayAccounts 账户数组
 * 
 * 实现ITraderSpi接口，处理账户查询回调。
 * 账户查询完成后，适配器进入全部就绪状态。
 */
void TraderAdapter::onRspAccount(WTSArray* ayAccounts)
{
	if(_state == AS_TRADES_QRYED)  // 如果状态为成交已查询
	{
		_state = AS_ALLREADY;  // 设置状态为全部就绪

		WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] Trading channel ready", _id.c_str());  // 记录信息日志
		for (auto sink : _sinks)  // 遍历所有通知接收器
			sink->on_channel_ready(_trading_day);  // 通知通道就绪
	}
}

/**
 * @brief 持仓查询回调
 * @param ayPositions 持仓数组
 * 
 * 实现ITraderSpi接口，处理持仓查询回调。
 * 更新持仓信息并通知接收器，然后查询订单。
 */
void TraderAdapter::onRspPosition(const WTSArray* ayPositions)
{
	if (ayPositions && ayPositions->size() > 0)  // 如果持仓数组存在且不为空
	{
		for (auto it = ayPositions->begin(); it != ayPositions->end(); it++)  // 遍历所有持仓
		{
			WTSPositionItem* pItem = (WTSPositionItem*)(*it);  // 获取持仓项
			WTSContractInfo* cInfo = _bd_mgr->getContract(pItem->getCode(), pItem->getExchg());  // 获取合约信息
			if (cInfo == NULL)  // 如果合约信息为空
				continue;  // 跳过

			std::string stdCode = cInfo->getFullCode();  // 获取标准合约代码

			PosItem& pos = _positions[stdCode];  // 获取持仓项
			if (pItem->getDirection() == WDT_LONG)  // 如果是多仓
			{
				pos.l_newavail = pItem->getAvailNewPos();  // 设置多今仓可平数量
				pos.l_newvol = pItem->getNewPosition();  // 设置多今仓数量
				pos.l_preavail = pItem->getAvailPrePos();  // 设置多昨仓可平数量
				pos.l_prevol = pItem->getPrePosition();  // 设置多昨仓数量
			}
			else  // 如果是空仓
			{
				pos.s_newavail = pItem->getAvailNewPos();  // 设置空今仓可平数量
				pos.s_newvol = pItem->getNewPosition();  // 设置空今仓数量
				pos.s_preavail = pItem->getAvailPrePos();  // 设置空昨仓可平数量
				pos.s_prevol = pItem->getPrePosition();  // 设置空昨仓数量
			}
		}

		for (auto it = _positions.begin(); it != _positions.end(); it++)  // 遍历所有持仓
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			const PosItem& pItem = it->second;  // 获取持仓项
			printPosition(stdCode, pItem);  // 打印持仓信息
			for (auto sink : _sinks)  // 遍历所有通知接收器
			{
				sink->on_position(stdCode, true, pItem.l_prevol, pItem.l_preavail, pItem.l_newvol, pItem.l_newavail, _trading_day);  // 通知多仓持仓
				sink->on_position(stdCode, false, pItem.s_prevol, pItem.s_preavail, pItem.s_newvol, pItem.s_newavail, _trading_day);  // 通知空仓持仓
			}
		}
	}

	WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,"[{}] Position data updated", _id.c_str());  // 记录信息日志

	if (_state == AS_LOGINED)  // 如果状态为已登录
	{
		_state = AS_POSITION_QRYED;  // 设置状态为仓位已查询

		_trader_api->queryOrders();  // 查询订单
	}
}

/**
 * @brief 订单查询回调
 * @param ayOrders 订单数组
 * 
 * 实现ITraderSpi接口，处理订单查询回调。
 * 更新订单列表和未完成数量，然后查询成交。
 */
void TraderAdapter::onRspOrders(const WTSArray* ayOrders)
{
	if (ayOrders)  // 如果订单数组存在
	{
		if (_orders == NULL)  // 如果订单列表为空
			_orders = OrderMap::create();  // 创建订单列表

		_undone_qty.clear();  // 清空未完成数量

		for (auto it = ayOrders->begin(); it != ayOrders->end(); it++)  // 遍历所有订单
		{
			WTSOrderInfo* orderInfo = (WTSOrderInfo*)(*it);  // 获取订单信息
			if (orderInfo == NULL)  // 如果订单信息为空
				continue;  // 跳过

			WTSContractInfo* cInfo = _bd_mgr->getContract(orderInfo->getCode(), orderInfo->getExchg());  // 获取合约信息
			if (cInfo == NULL)  // 如果合约信息为空
				continue;  // 跳过

			bool isBuy = (orderInfo->getDirection() == WDT_LONG && orderInfo->getOffsetType() == WOT_OPEN) || (orderInfo->getDirection() == WDT_SHORT && orderInfo->getOffsetType() != WOT_OPEN);  // 判断是否买入

			std::string stdCode = cInfo->getFullCode();  // 获取标准合约代码

			_orderids.insert(orderInfo->getOrderID());		// 插入订单号到集合

			//更新统计信息
			WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());  // 获取交易统计信息
			if (statInfo == NULL)  // 如果统计信息不存在
			{
				statInfo = WTSTradeStateInfo::create(stdCode.c_str());  // 创建统计信息
				_stat_map->add(stdCode, statInfo, false);  // 添加到统计映射表
			}
			TradeStatInfo& statItem = statInfo->statInfo();  // 获取统计信息引用
			statItem._infos++;	//无论什么状态，挂单信息量+1
			if (isBuy)  // 如果是买入
			{
				statItem.b_orders++;  // 买入订单数+1
				statItem.b_ordqty += orderInfo->getVolume();  // 买入订单数量累加

				if (orderInfo->isError())  // 如果是错误订单
				{
					statItem.b_wrongs++;  // 买入错误订单数+1
					statItem.b_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 买入错误订单数量累加
				}
				else if (orderInfo->getOrderState() == WOS_Canceled)  // 如果订单已撤销
				{
					if (orderInfo->getOrderFlag() == WOF_NOR)  // 如果是普通订单
					{
						statItem.b_cancels++;  // 买入撤销订单数+1
						statItem.b_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 买入撤销订单数量累加
					}
					else  // 如果是自动撤销订单
					{
						statItem.b_auto_cancels++;  // 买入自动撤销订单数+1
						statItem.b_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 买入自动撤销订单数量累加
					}

					//撤单信息量+1
					statItem._infos++;  // 撤单信息量+1
				}

			}
			else  // 如果是卖出
			{
				statItem.s_orders++;  // 卖出订单数+1
				statItem.s_ordqty += orderInfo->getVolume();  // 卖出订单数量累加

				if (orderInfo->isError())  // 如果是错误订单
				{
					statItem.s_wrongs++;  // 卖出错误订单数+1
					statItem.s_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 卖出错误订单数量累加
				}
				else if (orderInfo->getOrderState() == WOS_Canceled)  // 如果订单已撤销
				{
					if (orderInfo->getOrderFlag() == WOF_NOR)  // 如果是普通订单
					{
						statItem.s_cancels++;  // 卖出撤销订单数+1
						statItem.s_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 卖出撤销订单数量累加
					}
					else  // 如果是自动撤销订单
					{
						statItem.s_auto_cancels++;  // 卖出自动撤销订单数+1
						statItem.s_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 卖出自动撤销订单数量累加
					}

					//撤单信息量+1
					statItem._infos++;  // 撤单信息量+1
				}
			}

			if (!orderInfo->isAlive())  // 如果订单已结束
				continue;  // 跳过

			if (!StrUtil::startsWith(orderInfo->getUserTag(), _order_pattern.c_str(), true))  // 如果不是WT的订单
				continue;;  // 跳过

			char* userTag = (char*)orderInfo->getUserTag();  // 获取用户标签
			userTag += _order_pattern.size() + 1;  // 跳过订单模式前缀
			uint32_t localid = strtoul(userTag, NULL, 10);  // 解析本地订单ID

			{
				SpinLock lock(_mtx_orders);  // 获取订单列表锁
				_orders->add(localid, orderInfo);  // 添加订单到列表
			}

			double& curQty = _undone_qty[stdCode];  // 获取未完成数量引用
			curQty += orderInfo->getVolLeft();  // 累加未完成数量
		}

		for (auto it = _undone_qty.begin(); it != _undone_qty.end(); it++)  // 遍历所有未完成数量
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			const double& curQty = _undone_qty[stdCode];  // 获取未完成数量

			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
				"[{}]{} undone quantity {}", _id.c_str(), stdCode, curQty);  // 记录信息日志
		}
	}

	if (_state == AS_POSITION_QRYED)  // 如果状态为仓位已查询
	{
		_state = AS_ORDERS_QRYED;  // 设置状态为订单已查询

		_trader_api->queryTrades();  // 查询成交
	}
}

/**
 * @brief 打印持仓信息
 * @param code 合约代码
 * @param pItem 持仓项引用
 * 
 * 内部方法，打印持仓信息到日志。
 */
void TraderAdapter::printPosition(const char* code, const PosItem& pItem)
{
	WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG, "[{}] {} position updated, long:{}[{}]|{}[{}], short:{}[{}]|{}[{}]",
		_id.c_str(), code, pItem.l_prevol, pItem.l_preavail, pItem.l_newvol, pItem.l_newavail, 
		pItem.s_prevol, pItem.s_preavail, pItem.s_newvol, pItem.s_newavail);  // 记录调试日志
}

/**
 * @brief 成交查询回调
 * @param ayTrades 成交数组
 * 
 * 实现ITraderSpi接口，处理成交查询回调。
 * 更新交易统计信息，然后查询账户。
 */
void TraderAdapter::onRspTrades(const WTSArray* ayTrades)
{
	if (ayTrades)  // 如果成交数组存在
	{
		for (auto it = ayTrades->begin(); it != ayTrades->end(); it++)  // 遍历所有成交
		{
			WTSTradeInfo* tInfo = (WTSTradeInfo*)(*it);  // 获取成交信息

			WTSContractInfo* cInfo = tInfo->getContractInfo();  // 获取合约信息
			if (cInfo == NULL)  // 如果合约信息为空
				continue;  // 跳过

			const char* stdCode = cInfo->getFullCode();  // 获取标准合约代码

			WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);  // 获取交易统计信息
			if (statInfo == NULL)  // 如果统计信息不存在
			{
				statInfo = WTSTradeStateInfo::create(stdCode);  // 创建统计信息
				_stat_map->add(stdCode, statInfo, false);  // 添加到统计映射表
			}
			TradeStatInfo& statItem = statInfo->statInfo();  // 获取统计信息引用

			bool isLong = (tInfo->getDirection() == WDT_LONG);  // 是否多仓
			bool isOpen = (tInfo->getOffsetType() == WOT_OPEN);  // 是否开仓
			bool isCloseT = (tInfo->getOffsetType() == WOT_CLOSETODAY);  // 是否平今
			double qty = tInfo->getVolume();  // 获取数量

			if (isLong)  // 如果是多仓
			{
				if (isOpen)  // 如果是开仓
					statItem.l_openvol += qty;  // 多仓开仓数量累加
				else if (isCloseT)  // 如果是平今
					statItem.l_closetvol += qty;  // 多仓平今数量累加
				else  // 如果是平仓
					statItem.l_closevol += qty;  // 多仓平仓数量累加
			}
			else  // 如果是空仓
			{
				if (isOpen)  // 如果是开仓
					statItem.s_openvol += qty;  // 空仓开仓数量累加
				else if (isCloseT)  // 如果是平今
					statItem.s_closetvol += qty;  // 空仓平今数量累加
				else  // 如果是平仓
					statItem.s_closevol += qty;  // 空仓平仓数量累加
			}
		}

		for (auto it = _stat_map->begin(); it != _stat_map->end(); it++)  // 遍历所有统计信息
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			WTSTradeStateInfo* pItem = (WTSTradeStateInfo*)it->second;  // 获取交易统计信息
			WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO,
				"[{}] {} action stats updated, long opened: {}, long closed: {}, new long closed: {}, short opened: {}, short closed: {}, new short closed: {}",
				_id.c_str(), stdCode, pItem->open_volume_long(), pItem->close_volume_long(), pItem->closet_volume_long(),
				pItem->open_volume_short(), pItem->close_volume_short(), pItem->closet_volume_short());  // 记录信息日志
		}
	}

	if (_state == AS_ORDERS_QRYED)  // 如果状态为订单已查询
	{
		_state = AS_TRADES_QRYED;  // 设置状态为成交已查询

		_trader_api->queryAccount();  // 查询账户
	}
}

/**
 * @brief 将订单状态转换为字符串名称
 * @param woState 订单状态
 * @return 订单状态的字符串表示
 * 
 * 工具函数，将订单状态枚举值转换为可读的字符串名称，用于日志输出。
 */
inline const char* stateToName(WTSOrderState woState)
{
	if (woState == WOS_AllTraded)  // 如果订单状态为全部成交
		return "AllTrd";  // 返回"全部成交"
	else if (woState == WOS_PartTraded_NotQueuing || woState == WOS_PartTraded_Queuing)  // 如果订单状态为部分成交
		return "PrtTrd";  // 返回"部分成交"
	else if (woState == WOS_NotTraded_NotQueuing || woState == WOS_NotTraded_Queuing)  // 如果订单状态为未成交
		return "UnTrd";  // 返回"未成交"
	else if (woState == WOS_Canceled)  // 如果订单状态为已撤销
		return "Cncld";  // 返回"已撤销"
	else if (woState == WOS_Submitting)  // 如果订单状态为提交中
		return "Smtting";  // 返回"提交中"
	else if (woState == WOS_Nottouched)  // 如果订单状态为未触发
		return "UnSmt";  // 返回"未触发"
	else  // 如果订单状态为其他
		return "Error";  // 返回"错误"
}

/**
 * @brief 订单推送回调
 * @param orderInfo 订单信息指针
 * 
 * 实现ITraderSpi接口，处理订单推送。
 * 更新订单状态、持仓可平数量，并通知接收器。
 * 处理撤单时的统计数据更新和可平数量恢复。
 */
void TraderAdapter::onPushOrder(WTSOrderInfo* orderInfo)
{
	if (orderInfo == NULL)  // 如果订单信息为空
		return;  // 直接返回


	WTSContractInfo* cInfo = _bd_mgr->getContract(orderInfo->getCode(), orderInfo->getExchg());  // 获取合约信息
	if (cInfo == NULL)  // 如果合约信息为空
		return;  // 直接返回

	std::string stdCode = cInfo->getFullCode();  // 获取标准合约代码

	bool isBuy = (orderInfo->getDirection() == WDT_LONG && orderInfo->getOffsetType() == WOT_OPEN) || (orderInfo->getDirection() == WDT_SHORT && orderInfo->getOffsetType() != WOT_OPEN);  // 判断是否买入（开多或平空）

	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());  // 获取交易统计信息
	if (statInfo == NULL)  // 如果统计信息不存在
	{
		statInfo = WTSTradeStateInfo::create(stdCode.c_str());  // 创建统计信息
		_stat_map->add(stdCode, statInfo, false);  // 添加到统计映射表
	}
	TradeStatInfo& statItem = statInfo->statInfo();  // 获取统计信息引用

	//撤销的话, 要更新统计数据
	if (orderInfo->getOrderState() == WOS_Canceled)  // 如果订单状态为已撤销
	{
		statItem._infos++;	//撤单成功信息量+1
		if (isBuy)  // 如果是买入订单
		{
			if (orderInfo->isError())//错单要和撤单区分开
			{
				statItem.b_wrongs++;  // 买入错单数量+1
				statItem.b_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 买入错单数量累加
			}
			else  // 如果不是错单
			{
				//只有普通订单的撤单才计入统计
				if(orderInfo->getOrderFlag() == WOF_NOR)  // 如果是普通订单
				{
					statItem.b_cancels++;  // 买入撤单数量+1
					statItem.b_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 买入撤单数量累加
				}
				else  // 如果是自动撤单（FAK/FOK）
				{
					statItem.b_auto_cancels++;  // 买入自动撤单数量+1
					statItem.b_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 买入自动撤单数量累加
				}
			}
		}
		else  // 如果是卖出订单
		{
			if (orderInfo->isError())//错单要和撤单区分开
			{
				statItem.s_wrongs++;  // 卖出错单数量+1
				statItem.s_wrongqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 卖出错单数量累加
			}
			else  // 如果不是错单
			{
				if (orderInfo->getOrderFlag() == WOF_NOR)  // 如果是普通订单
				{
					statItem.s_cancels++;  // 卖出撤单数量+1
					statItem.s_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 卖出撤单数量累加
				}
				else  // 如果是自动撤单（FAK/FOK）
				{
					statItem.s_auto_cancels++;  // 卖出自动撤单数量+1
					statItem.s_auto_canclqty += orderInfo->getVolume() - orderInfo->getVolTraded();  // 卖出自动撤单数量累加
				}
			}
		}
	}


	WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,"[{}] Order notified, instrument: {}, usertag: {}, state: {}", _id.c_str(), stdCode.c_str(), orderInfo->getUserTag(), stateToName(orderInfo->getOrderState()));  // 记录调试日志

	//如果订单撤销, 并且是wt的订单, 则要先更新未完成数量
	if (orderInfo->getOrderState() == WOS_Canceled && StrUtil::startsWith(orderInfo->getUserTag(), _order_pattern.c_str(), true))  // 如果订单已撤销且是本适配器发出的订单
	{
		//撤单的时候, 要更新未完成
		bool isLong = (orderInfo->getDirection() == WDT_LONG);  // 是否多仓
		bool isOpen = (orderInfo->getOffsetType() == WOT_OPEN);  // 是否开仓
		bool isToday = (orderInfo->getOffsetType() == WOT_CLOSETODAY);  // 是否平今
		double qty = orderInfo->getVolume() - orderInfo->getVolTraded();  // 计算撤销数量

		bool isBuy = (isLong&&isOpen) || (!isLong&&!isOpen);  // 判断是否买入
		//double oldQty = _undone_qty[stdCode];
		//double newQty = oldQty - qty*(isBuy ? 1 : -1);
		//_undone_qty[stdCode] = newQty;
		//WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, "[{}] {} qty of undone order updated, {} -> {}", _id.c_str(), stdCode.c_str(), oldQty, newQty);
		updateUndone(stdCode.c_str(), -qty);  // 更新未完成数量（减少）

		WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG, "[{}] Order {} of {} canceled:{}, action: {}, leftqty: {}",
			_id.c_str(), orderInfo->getUserTag(), stdCode.c_str(), orderInfo->getStateMsg(),
			formatAction(orderInfo->getDirection(), orderInfo->getOffsetType()), qty);  // 记录调试日志
	}

	//先检查该订单是不是第一次推送过来
	//如果是第一次推送过来, 则要根据开平更新可平
	if (strlen(orderInfo->getOrderID()) > 0)  // 如果订单ID不为空
	{
		auto it = _orderids.find(orderInfo->getOrderID());  // 查找订单ID
		if (it == _orderids.end())  // 如果是第一次推送（订单ID不存在）
		{
			//先把订单号缓存起来, 防止重复处理
			_orderids.insert(orderInfo->getOrderID());  // 将订单ID添加到集合
			statItem._infos++;	//下单成功信息量+1

			//只有平仓需要更新可平
			if (orderInfo->getOffsetType() != WOT_OPEN)  // 如果是平仓订单
			{
				//const char* code = stdCode.c_str();
				bool isLong = (orderInfo->getDirection() == WDT_LONG);  // 是否多仓
				bool isToday = (orderInfo->getOffsetType() == WOT_CLOSETODAY);  // 是否平今
				double qty = orderInfo->getVolume();  // 获取订单数量

				PosItem& pItem = _positions[stdCode];  // 获取持仓项
				if (isLong)	//平多
				{
					if (isToday)  // 如果是平今
					{
						pItem.l_newavail -= min(pItem.l_newavail, qty);	//如果是平今, 则只需要更新可平今仓
					}
					else  // 如果是平仓（不分平昨平今）
					{
						double left = qty;  // 剩余数量

						//如果是平仓, 则先更新可平昨仓, 还有剩余, 再更新可平今仓
						//如果品种区分平昨平今, 也按照这个流程, 因为平昨的总数量不可能超出昨仓
						double maxQty = min(pItem.l_preavail, qty);  // 计算最大可平昨仓数量
						pItem.l_preavail -= maxQty;  // 减少可平昨仓
						left -= maxQty;  // 减少剩余数量

						if (left > 0)  // 如果还有剩余
							pItem.l_newavail -= min(pItem.l_newavail, left);  // 减少可平今仓
					}
				}
				else //平空
				{
					if (isToday)  // 如果是平今
					{
						pItem.s_newavail -= min(pItem.s_newavail, qty);  // 减少可平今空仓
					}
					else  // 如果是平仓（不分平昨平今）
					{
						double left = qty;  // 剩余数量

						double maxQty = min(pItem.s_preavail, qty);  // 计算最大可平昨空仓数量
						pItem.s_preavail -= maxQty;  // 减少可平昨空仓
						left -= maxQty;  // 减少剩余数量

						if (left > 0)  // 如果还有剩余
							pItem.s_newavail -= min(pItem.s_newavail, left);  // 减少可平今空仓
					}
				}
				printPosition(stdCode.c_str(), pItem);  // 打印持仓信息
			}
		}
		else if (orderInfo->getOrderState() == WOS_Canceled && orderInfo->getOffsetType() != WOT_OPEN)  // 如果订单不是第一次推送且已撤销且是平仓订单
		{
			//如果订单不是第一次推送, 且撤销了, 则要更新可平量
			//const char* code = orderInfo->getCode();
			bool isLong = (orderInfo->getDirection() == WDT_LONG);  // 是否多仓
			bool isToday = (orderInfo->getOffsetType() == WOT_CLOSETODAY);  // 是否平今
			double qty = orderInfo->getVolume() - orderInfo->getVolTraded();  // 计算撤销数量

			PosItem& pItem = _positions[stdCode];  // 获取持仓项
			if (isLong)	//平多
			{
				if (isToday)  // 如果是平今
				{
					pItem.l_newavail += qty;	//如果是平今, 则只需要更新可平今仓（恢复可平数量）
				}
				else  // 如果是平仓（不分平昨平今）
				{
					pItem.l_preavail += qty;  // 恢复可平昨仓
					if (pItem.l_preavail > pItem.l_prevol)  // 如果可平昨仓超过昨仓总量
					{
						pItem.l_newavail += (pItem.l_preavail - pItem.l_prevol);  // 将超出的部分加到可平今仓
						pItem.l_preavail = pItem.l_prevol;  // 将可平昨仓设为昨仓总量
					}
				}
			}
			else //平空
			{
				if (isToday)  // 如果是平今
				{
					pItem.s_newavail += qty;  // 恢复可平今空仓
				}
				else  // 如果是平仓（不分平昨平今）
				{
					pItem.s_preavail += qty;  // 恢复可平昨空仓
					if (pItem.s_preavail > pItem.s_prevol)  // 如果可平昨空仓超过昨空仓总量
					{
						pItem.s_newavail += (pItem.s_preavail - pItem.s_prevol);  // 将超出的部分加到可平今空仓
						pItem.s_preavail = pItem.s_prevol;  // 将可平昨空仓设为昨空仓总量
					}
				}
			}
			printPosition(stdCode.c_str(), pItem);  // 打印持仓信息
		}
	}

	uint32_t localid = 0;  // 本地订单ID

	//先看看是不是wt发出去的单子
	if (StrUtil::startsWith(orderInfo->getUserTag(), _order_pattern.c_str(), true))  // 如果用户标签以订单模式开头
	{
		char* userTag = (char*)orderInfo->getUserTag();  // 获取用户标签
		userTag += _order_pattern.size() + 1;  // 跳过订单模式前缀
		localid = strtoul(userTag, NULL, 10);  // 解析本地订单ID
	}

	//如果是wt发出去的单子则需要更新内部数据
	if(localid != 0)  // 如果本地订单ID不为0
	{
		{
			SpinLock lock(_mtx_orders);  // 获取订单列表锁
			if (!orderInfo->isAlive() && _orders)  // 如果订单已结束且订单列表存在
			{
				_orders->remove(localid);  // 从订单列表中移除
			}
			else  // 如果订单还未结束
			{
				if (_orders == NULL)  // 如果订单列表不存在
					_orders = OrderMap::create();  // 创建订单列表

				_orders->add(localid, orderInfo);  // 添加或更新订单
			}
		}
		

		uint32_t offset;  // 开平标志
		if (orderInfo->getOffsetType() == WOT_OPEN)  // 如果是开仓
			offset = 0;  // 开平标志为0
		else if (orderInfo->getOffsetType() == WOT_CLOSE)  // 如果是平仓
			offset = 1;  // 开平标志为1
		else  // 如果是平今
			offset = 2;  // 开平标志为2

		//通知所有监听接口
		for (auto sink : _sinks)  // 遍历所有通知接收器
			sink->on_order(localid, stdCode.c_str(), orderInfo->getDirection()==WDT_LONG, offset, 
				orderInfo->getVolume(), orderInfo->getVolLeft(), orderInfo->getPrice(), orderInfo->getOrderState() == WOS_Canceled);  // 通知订单变化
	}
}

/**
 * @brief 成交推送回调
 * @param tradeRecord 成交记录指针
 * 
 * 实现ITraderSpi接口，处理成交推送。
 * 更新持仓、未完成数量，并通知接收器。
 * 根据成交方向（多空）和开平类型（开仓/平今/平仓）更新持仓明细。
 */
void TraderAdapter::onPushTrade(WTSTradeInfo* tradeRecord)
{
	WTSContractInfo* cInfo = tradeRecord->getContractInfo();  // 获取合约信息
	cInfo = _bd_mgr->getContract(tradeRecord->getCode(), tradeRecord->getExchg());  // 从基础数据管理器获取合约信息（确保获取到）
	if (cInfo == NULL)  // 如果合约信息为空
		return;  // 直接返回

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取品种信息

	bool isLong = (tradeRecord->getDirection() == WDT_LONG);  // 是否多仓
	bool isOpen = (tradeRecord->getOffsetType() == WOT_OPEN);  // 是否开仓
	bool isBuy = (tradeRecord->getDirection() == WDT_LONG && tradeRecord->getOffsetType() == WOT_OPEN) || (tradeRecord->getDirection() == WDT_SHORT && tradeRecord->getOffsetType() != WOT_OPEN);  // 判断是否买入（开多或平空）

	std::string stdCode = cInfo->getFullCode();  // 获取标准合约代码

	WTSLogger::log_dyn("trader", _id.c_str(), LL_DEBUG,
		"[{}] Trade notified, instrument: {}, usertag: {}, trdqty: {}, trdprice: {}", 
			_id.c_str(), stdCode.c_str(), tradeRecord->getUserTag(), tradeRecord->getVolume(), tradeRecord->getPrice());  // 记录调试日志

	//如果是自己的订单，则更新未完成单
	uint32_t localid = 0;  // 本地订单ID
	if (StrUtil::startsWith(tradeRecord->getUserTag(), _order_pattern.c_str(), true))  // 如果用户标签以订单模式开头
	{
		char* userTag = (char*)tradeRecord->getUserTag();  // 获取用户标签
		userTag += _order_pattern.size() + 1;  // 跳过订单模式前缀
		localid = strtoul(userTag, NULL, 10);  // 解析本地订单ID

		//double oldQty = _undone_qty[stdCode];
		//double newQty = oldQty - tradeRecord->getVolume()*(isBuy ? 1 : -1);
		//_undone_qty[stdCode] = newQty;
		//WTSLogger::log_dyn("trader", _id.c_str(), LL_INFO, 
		//	"[{}] {} qty of undone orders updated, {} -> {}", _id.c_str(), stdCode.c_str(), oldQty, newQty);
		updateUndone(stdCode.c_str(), -tradeRecord->getVolume());  // 更新未完成数量（减少成交数量）
	}

	PosItem& pItem = _positions[stdCode];  // 获取持仓项
	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode.c_str());  // 获取交易统计信息
	if (statInfo == NULL)  // 如果统计信息不存在
	{
		statInfo = WTSTradeStateInfo::create(stdCode.c_str());  // 创建统计信息
		_stat_map->add(stdCode, statInfo, false);  // 添加到统计映射表
	}

	double vol = tradeRecord->getVolume();  // 获取成交数量
	if(isLong)  // 如果是多仓
	{
		if (isOpen)  // 如果是开仓
		{
			pItem.l_newvol += vol;  // 多今仓数量增加

			if(!commInfo->isT1())	//如果不是T1，则更新可用持仓
				pItem.l_newavail += vol;  // 多今仓可平数量增加
		}
		else if (tradeRecord->getOffsetType() == WOT_CLOSETODAY)  // 如果是平今
		{
			pItem.l_newvol -= vol;  // 多今仓数量减少
		}
		else  // 如果是平仓（不分平昨平今）
		{
			double left = vol;  // 剩余数量
			double maxVol = min(left, pItem.l_prevol);  // 计算最大可平昨仓数量
			pItem.l_prevol -= maxVol;  // 多昨仓数量减少
			left -= maxVol;  // 减少剩余数量
			pItem.l_newvol -= left;  // 多今仓数量减少剩余部分
		}
	}
	else  // 如果是空仓
	{
		if (isOpen)  // 如果是开仓
		{
			pItem.s_newvol += vol;  // 空今仓数量增加
			if (!commInfo->isT1())	//如果不是T1，则更新可用持仓
				pItem.s_newavail += vol;  // 空今仓可平数量增加
		}
		else if (tradeRecord->getOffsetType() == WOT_CLOSETODAY)  // 如果是平今
		{
			pItem.s_newvol -= vol;  // 空今仓数量减少
		}
		else  // 如果是平仓（不分平昨平今）
		{
			double left = vol;  // 剩余数量
			double maxVol = min(left, pItem.s_prevol);  // 计算最大可平昨空仓数量
			pItem.s_prevol -= maxVol;  // 空昨仓数量减少
			left -= maxVol;  // 减少剩余数量
			pItem.s_newvol -= left;  // 空今仓数量减少剩余部分
		}
	}

	printPosition(stdCode.c_str(), pItem);  // 打印持仓信息


	uint32_t offset;  // 开平标志
	if (tradeRecord->getOffsetType() == WOT_OPEN)  // 如果是开仓
		offset = 0;  // 开平标志为0
	else if (tradeRecord->getOffsetType() == WOT_CLOSE)  // 如果是平仓
		offset = 1;  // 开平标志为1
	else  // 如果是平今
		offset = 2;  // 开平标志为2
	for (auto sink : _sinks)  // 遍历所有通知接收器
		sink->on_trade(localid, stdCode.c_str(), isLong, offset, vol, tradeRecord->getPrice());  // 通知成交变化

	_trader_api->queryAccount();  // 查询账户（更新账户信息）
}

/**
 * @brief 交易错误回调
 * @param err 错误信息指针
 * @param pData 附加数据指针
 * 
 * 实现ITraderSpi接口，处理交易错误。
 */
void TraderAdapter::onTraderError(WTSError* err, void* pData /* = NULL */)
{
	if(err)  // 如果错误信息存在
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR,"[{}] Error of trading channel occured: {}", _id.c_str(), err->getMessage());  // 记录错误日志
}

/**
 * @brief 获取基础数据管理器
 * @return 基础数据管理器指针
 * 
 * 实现ITraderSpi接口，返回基础数据管理器。
 */
IBaseDataMgr* TraderAdapter::getBaseDataMgr()
{
	return _bd_mgr;  // 返回基础数据管理器指针
}

/**
 * @brief 处理交易日志
 * @param ll 日志级别
 * @param message 日志消息
 * 
 * 实现ITraderSpi接口，处理交易接口的日志输出。
 */
void TraderAdapter::handleTraderLog(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_dyn_raw("trader", _id.c_str(), ll, message);  // 记录原始日志
}

/**
 * @brief 检查撤单限制
 * @param stdCode 标准合约代码
 * @return 允许撤单返回true，禁止撤单返回false
 * 
 * 检查合约的撤单频率是否超过限制。
 * 检查撤单总限额和撤单频率限制，如果超过限制则将该合约加入排除列表。
 */
bool TraderAdapter::checkCancelLimits(const char* stdCode)
{
	if (_exclude_codes.find(stdCode) != _exclude_codes.end())  // 如果合约在排除列表中
		return false;  // 返回禁止撤单

	const RiskParams* riskPara = getRiskParams(stdCode);  // 获取风险控制参数
	if (riskPara == NULL)  // 如果风险参数不存在
		return true;  // 返回允许撤单

	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);  // 获取交易统计信息
	if (statInfo && riskPara->_cancel_total_limits != 0 && statInfo->total_cancels() >= riskPara->_cancel_total_limits)  // 如果撤单总限额不为0且总撤单次数已达到限额
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} cancel {} times totaly, beyond boundary {} times, adding to excluding list",
			_id.c_str(), stdCode, statInfo->total_cancels(), riskPara->_cancel_total_limits);  // 记录错误日志
		_exclude_codes.insert(stdCode);  // 将合约加入排除列表
		return false;  // 返回禁止撤单
	}

	//撤单频率检查
	auto it = _cancel_time_cache.find(stdCode);  // 查找撤单时间缓存
	if (it != _cancel_time_cache.end())  // 如果找到撤单时间缓存
	{
		TimeCacheList& cache = (TimeCacheList&)it->second;  // 获取时间缓存列表引用
		uint32_t cnt = cache.size();  // 获取缓存数量
		if (cnt >= riskPara->_cancel_times_boundary)  // 如果缓存数量达到边界值
		{
			uint64_t eTime = cache[cnt - 1];  // 获取最后一个时间戳
			uint64_t sTime = eTime - riskPara->_cancel_stat_timespan * 1000;  // 计算统计起始时间
			auto tit = std::lower_bound(cache.begin(), cache.end(), sTime);  // 查找起始时间位置
			auto sIdx = tit - cache.begin();  // 计算起始索引
			auto times = cnt - sIdx - 1;  // 计算时间窗口内的撤单次数
			if (times > riskPara->_cancel_times_boundary)  // 如果撤单次数超过边界值
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} cancel {} times within {} seconds, beyond boundary {} times, adding to excluding list",
					_id.c_str(), stdCode, times, riskPara->_cancel_stat_timespan, riskPara->_cancel_times_boundary);  // 记录错误日志
				_exclude_codes.insert(stdCode);  // 将合约加入排除列表
				return false;  // 返回禁止撤单
			}

			//这里必须要清理一下, 没有特别好的办法
			//不然随着时间推移, vector长度会越来越长
			if (tit != cache.begin())  // 如果起始位置不是开头
			{
				cache.erase(cache.begin(), tit);  // 删除过期的时间戳
			}
		}
	}

	return true;  // 返回允许撤单
}

/**
 * @brief 检查合约是否允许交易
 * @param stdCode 标准合约代码
 * @return 允许交易返回true，禁止交易返回false
 * 
 * 检查合约是否在风控排除列表中。
 * 如果未启用风险监控，则始终允许交易。
 */
bool TraderAdapter::isTradeEnabled(const char* stdCode) const
{
	if (!_risk_mon_enabled)  // 如果未启用风险监控
		return true;  // 返回允许交易

	if (_exclude_codes.find(stdCode) != _exclude_codes.end())  // 如果合约在排除列表中
		return false;  // 返回禁止交易

	return true;  // 返回允许交易
}

/**
 * @brief 检查下单限制
 * @param stdCode 标准合约代码
 * @return 允许下单返回true，禁止下单返回false
 * 
 * 检查合约的下单频率是否超过限制。
 * 检查下单总限额和下单频率限制，如果超过限制则将该合约加入排除列表。
 */
bool TraderAdapter::checkOrderLimits(const char* stdCode)
{
	if (_exclude_codes.find(stdCode) != _exclude_codes.end())  // 如果合约在排除列表中
		return false;  // 返回禁止下单

	const RiskParams* riskPara = getRiskParams(stdCode);  // 获取风险控制参数
	if (riskPara == NULL)  // 如果风险参数不存在
		return true;  // 返回允许下单

	WTSTradeStateInfo* statInfo = (WTSTradeStateInfo*)_stat_map->get(stdCode);  // 获取交易统计信息
	if (statInfo && riskPara->_order_total_limits != 0 && statInfo->total_orders() >= riskPara->_order_total_limits)  // 如果下单总限额不为0且总下单次数已达到限额
	{
		WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} entrust {} times totally, beyond boundary {} times, adding to excluding list",
			_id.c_str(), stdCode, statInfo->total_orders(), riskPara->_order_total_limits);  // 记录错误日志
		_exclude_codes.insert(stdCode);  // 将合约加入排除列表
		return false;  // 返回禁止下单
	}

	//下单频率检查
	auto it = _order_time_cache.find(stdCode);  // 查找下单时间缓存
	if (it != _order_time_cache.end())  // 如果找到下单时间缓存
	{
		TimeCacheList& cache = (TimeCacheList&)it->second;  // 获取时间缓存列表引用
		uint32_t cnt = cache.size();  // 获取缓存数量
		if (cnt >= riskPara->_order_times_boundary)  // 如果缓存数量达到边界值
		{
			uint64_t eTime = cache[cnt - 1];  // 获取最后一个时间戳
			uint64_t sTime = eTime - riskPara->_order_stat_timespan * 1000;  // 计算统计起始时间（毫秒）
			auto tit = std::lower_bound(cache.begin(), cache.end(), sTime);  // 查找起始时间位置
			auto sIdx = tit - cache.begin();  // 计算起始索引
			auto times = cnt - sIdx - 1;  // 计算时间窗口内的下单次数（减1是因为最后一次是当前时间）
			if (times > riskPara->_order_times_boundary)  // 如果下单次数超过边界值
			{
				WTSLogger::log_dyn("trader", _id.c_str(), LL_ERROR, "[{}] {} entrust {} times within {} seconds, beyond boundary {} times, adding to excluding list",
					_id.c_str(), stdCode, times, riskPara->_order_stat_timespan, riskPara->_order_times_boundary);  // 记录错误日志
				_exclude_codes.insert(stdCode);  // 将合约加入排除列表
				return false;  // 返回禁止下单
			}

			//这里必须要清理一下, 没有特别好的办法
			//不然随着时间推移, vector长度会越来越长
			if (tit != cache.begin())  // 如果起始位置不是开头
			{
				cache.erase(cache.begin(), tit);  // 删除过期的时间戳，避免内存无限增长
			}
		}
	}

	return true;  // 返回允许下单
}

/**
 * @brief 获取风险控制参数
 * @param stdCode 标准合约代码
 * @return 风险控制参数指针，如果未找到则返回默认参数（如果未找到默认参数则可能返回空指针）
 * 
 * 根据合约代码获取对应的风险控制参数。
 * 首先从标准合约代码中提取品种代码（如"SHFE.cu"中的"cu"），然后查找对应的风险参数。
 * 如果未找到品种对应的风险参数，则查找默认风险参数。
 * 注意：如果未找到默认风险参数，返回的指针可能无效，调用者需要检查。
 */
const TraderAdapter::RiskParams* TraderAdapter::getRiskParams(const char* stdCode)
{
	auto idx = StrUtil::findFirst(stdCode, '.');  // 查找交易所和合约代码的分隔符位置（如"SHFE.cu2312"中的'.'）
	auto eIdx = idx + 1;  // 品种代码结束位置（从分隔符后开始）
	while (isalpha(stdCode[eIdx]))  // 遍历字母字符，提取品种代码（如"cu"）
		eIdx++;  // 递增结束位置


	auto it = _risk_params_map.find(std::string(stdCode + idx + 1, eIdx - idx - 1));  // 查找品种代码对应的风险参数（如"cu"对应的风险参数）
	if (it != _risk_params_map.end())  // 如果找到
		return &it->second;  // 返回风险参数指针

	it = _risk_params_map.find("default");  // 查找默认风险参数
	if (it != _risk_params_map.end())  // 如果找到默认参数
		return &it->second;  // 返回默认风险参数指针
	return NULL;  // 如果未找到默认参数，返回空指针
}

#pragma endregion "ITraderSpi接口"


//////////////////////////////////////////////////////////////////////////
//TraderAdapterMgr
/**
 * @brief 添加交易适配器
 * @param tname 交易通道名称
 * @param adapter 交易适配器智能指针
 * @return 添加成功返回true，失败返回false
 * 
 * 将交易适配器添加到管理器中，如果名称已存在则添加失败。
 */
bool TraderAdapterMgr::addAdapter(const char* tname, TraderAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(tname) == 0)  // 如果适配器为空或名称为空
		return false;  // 返回失败

	auto it = _adapters.find(tname);  // 查找适配器
	if(it != _adapters.end())  // 如果已存在同名适配器
	{
		WTSLogger::error("Same name of trading channels: {}", tname);  // 记录错误日志
		return false;  // 返回失败
	}

	_adapters[tname] = adapter;  // 添加适配器到映射表

	return true;  // 返回成功
}

/**
 * @brief 获取指定名称的适配器
 * @param tname 交易通道名称
 * @return 交易适配器智能指针，如果不存在则返回空指针
 */
TraderAdapterPtr TraderAdapterMgr::getAdapter(const char* tname)
{
	auto it = _adapters.find(tname);  // 查找适配器
	if (it != _adapters.end())  // 如果找到
	{
		return it->second;  // 返回适配器智能指针
	}

	return TraderAdapterPtr();  // 返回空指针
}

/**
 * @brief 启动所有适配器
 * 
 * 启动所有交易适配器，开始连接和登录流程。
 */
void TraderAdapterMgr::run()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		it->second->run();  // 启动适配器
	}

	WTSLogger::info("{} trading channels started", _adapters.size());  // 记录信息日志
}

/**
 * @brief 释放所有适配器
 * 
 * 释放所有交易适配器占用的资源。
 */
void TraderAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		it->second->release();  // 释放适配器资源
	}

	_adapters.clear();  // 清空适配器映射表
}