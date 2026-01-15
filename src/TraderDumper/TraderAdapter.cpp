/*!
 * \file TraderAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief TraderAdapter类的实现文件，实现交易适配器的所有功能
 * 
 * 本文件实现了TraderAdapter类和TraderAdapterMgr类的所有方法。
 * 主要功能包括：
 * - 动态加载交易模块DLL/so
 * - 创建和管理交易接口实例
 * - 实现ITraderSpi接口，处理交易数据回调
 * - 将交易数据转换为标准格式并传递给Dumper
 * - 管理多个交易适配器的生命周期
 * 
 * 数据流程：
 * 交易接口回调 -> TraderAdapter处理 -> Dumper转发 -> 外部回调函数
 */
#include "TraderAdapter.h"          // 包含类定义头文件
#include "WtHelper.h"                // 包含辅助工具类，用于获取模块目录
#include "Dumper.h"                  // 包含Dumper类，用于转发数据

#include <atomic>                    // 原子操作支持

#include "../WTSTools/WTSLogger.h"   // WonderTrader日志工具

#include "../Share/CodeHelper.hpp"   // 代码辅助工具
#include "../Includes/WTSError.hpp"  // 错误信息类
#include "../Includes/WTSVariant.hpp"  // 变体类型，用于配置参数
#include "../Includes/WTSTradeDef.hpp"  // 交易相关定义
#include "../Includes/WTSRiskDef.hpp"   // 风控相关定义
#include "../Share/StrUtil.hpp"      // 字符串工具
#include "../Share/StdUtils.hpp"     // 标准工具函数
#include "../Share/TimeUtils.hpp"    // 时间工具函数
#include "../Share/decimal.h"        // 高精度小数类型
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息

#include "../Includes/WTSContractInfo.hpp"  // 合约信息类
#include "../Includes/IBaseDataMgr.h"        // 基础数据管理器接口
#include "../Share/DLLHelper.hpp"           // 动态库加载辅助工具
#include "../Share/StdUtils.hpp"             // 标准工具函数（重复包含，但无害）


extern Dumper& getDumper();         // 外部函数声明：获取全局Dumper单例
extern std::string getBaseFolder();  // 外部函数声明：获取基础文件夹路径


/**
 * @brief TraderAdapter构造函数实现
 * @param mgr 交易适配器管理器指针
 * 
 * 使用初始化列表初始化所有成员变量：
 * - _id初始化为空字符串
 * - _cfg初始化为NULL
 * - _trader_api初始化为NULL
 * - _mgr设置为传入的管理器指针
 * - _done初始化为false（未完成状态）
 */
TraderAdapter::TraderAdapter(TraderAdapterMgr* mgr)
	: _id("")                        // 初始化通道ID为空字符串
	, _cfg(NULL)                     // 初始化配置指针为NULL
	, _trader_api(NULL)              // 初始化交易接口指针为NULL
	, _mgr(mgr)                      // 初始化管理器指针为传入的值
	, _done(false)                   // 初始化完成标志为false（未完成）
{
}


/**
 * @brief TraderAdapter析构函数实现
 * 
 * 析构函数为空，实际的资源释放应在release()方法中完成。
 * 这样可以确保资源释放的顺序和时机可控。
 */
TraderAdapter::~TraderAdapter()
{
}

/**
 * @brief 初始化交易适配器的实现
 * @param id 交易通道ID
 * @param params 配置参数
 * @param bdMgr 基础数据管理器
 * @return 返回初始化是否成功
 * 
 * 初始化流程：
 * 1. 参数验证：检查params和_cfg是否有效
 * 2. 保存参数：保存通道ID、配置参数、基础数据管理器
 * 3. 加载交易模块：根据配置中的模块名，动态加载DLL/so
 * 4. 创建交易接口：调用模块的createTrader函数创建接口实例
 * 5. 初始化交易接口：调用接口的init方法，传入配置参数
 */
bool TraderAdapter::init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr)
{
	if (params == NULL)             // 如果配置参数为空，初始化失败
		return false;

	_bd_mgr = bdMgr;                 // 保存基础数据管理器指针
	_id = id;                        // 保存交易通道ID字符串

	if (_cfg != NULL)                // 如果配置已存在，说明已经初始化过，不允许重复初始化
		return false;

	_cfg = params;                   // 保存配置参数指针
	_cfg->retain();                  // 增加配置参数的引用计数，防止被释放


	if (params->getString("module").empty())  // 如果配置中没有模块名，初始化失败
		return false;


	//先看工作目录下是否有交易模块
	std::string module = DLLHelper::wrap_module(params->getCString("module"), "lib");;  // 将模块名包装为DLL/so文件名（如TraderCTP -> libTraderCTP.so或TraderCTP.dll）

	if (!StdFile::exists(module.c_str()))    // 如果工作目录下不存在该模块文件
	{
		module = WtHelper::get_module_dir();  // 获取模块目录路径
		module += "traders/";                 // 拼接traders子目录
		module += DLLHelper::wrap_module(params->getCString("module"), "lib");  // 拼接模块文件名
	}

	DllHandle hInst = DLLHelper::load_library(module.c_str());  // 动态加载交易模块DLL/so文件
	if (hInst == NULL)              // 如果加载失败
	{
		WTSLogger::error("[{}]交易模块{}加载失败", _id.c_str(), module.c_str());  // 记录错误日志
		return false;
	}

	FuncCreateTrader pFunCreateTrader = (FuncCreateTrader)DLLHelper::get_symbol(hInst, "createTrader");  // 从DLL/so中获取createTrader函数地址
	if (NULL == pFunCreateTrader)   // 如果获取函数地址失败
	{
		WTSLogger::error("[{}]交易接口创建函数读取失败", _id.c_str());  // 记录错误日志
		return false;
	}

	_trader_api = pFunCreateTrader();  // 调用createTrader函数创建交易接口实例
	if (NULL == _trader_api)         // 如果创建失败
	{
		WTSLogger::error("[{}]交易接口创建失败", _id.c_str());  // 记录错误日志
		return false;
	}

	_remover = (FuncDeleteTrader)DLLHelper::get_symbol(hInst, "deleteTrader");  // 获取deleteTrader函数地址，用于后续释放接口
	
	//这里要强制把quick改成true，不查全部成交和订单
	params->append("quick", true);   // 强制设置quick参数为true，表示快速模式，不查询全部历史成交和订单
	if (!_trader_api->init(params))  // 调用交易接口的init方法初始化接口
	{
		WTSLogger::error("[{}]交易接口启动失败: 交易接口初始化失败", id);  // 记录错误日志
		return false;
	}

	WTSLogger::info("[{}]交易接口初始化成功", id);  // 记录成功日志
	return true;                     // 返回成功
}

/**
 * @brief 启动交易适配器的实现
 * @return 返回启动是否成功
 * 
 * 启动流程：
 * 1. 检查交易接口是否已创建
 * 2. 注册回调接口（this），使交易接口可以回调本类的方法
 * 3. 连接交易服务器，连接成功后会触发handleEvent回调
 */
bool TraderAdapter::run()
{
	if (_trader_api == NULL)        // 如果交易接口未创建，启动失败
		return false;

	_trader_api->registerSpi(this);  // 注册回调接口，this指针指向本TraderAdapter实例

	_trader_api->connect();         // 连接交易服务器，异步操作，连接结果通过handleEvent回调通知
	return true;                     // 返回成功
}

/**
 * @brief 释放资源的实现
 * 
 * 释放流程：
 * 1. 注销回调接口（设置为NULL）
 * 2. 调用交易接口的release方法释放资源
 */
void TraderAdapter::release()
{
	if (_trader_api)                 // 如果交易接口存在
	{
		_trader_api->registerSpi(NULL);  // 注销回调接口，防止回调已释放的对象
		_trader_api->release();          // 释放交易接口资源
	}
}

#pragma region "ITraderSpi接口"
/**
 * @brief 处理交易事件的实现
 * @param e 交易事件类型
 * @param ec 事件代码
 * 
 * 处理交易接口的事件通知：
 * - WTE_Connect: 连接事件，连接成功则自动登录，失败则标记为完成
 * - WTE_Close: 连接关闭事件，记录日志
 */
void TraderAdapter::handleEvent(WTSTraderEvent e, int32_t ec)
{
	if(e == WTE_Connect)            // 如果是连接事件
	{
		if(ec == 0)                  // 如果连接成功（错误码为0）
		{
			_trader_api->login(_cfg->getCString("user"), _cfg->getCString("pass"), _cfg->getCString("product"));  // 自动登录，传入用户名、密码、产品信息
		}
		else                         // 如果连接失败
		{
			WTSLogger::error("[{}]交易账号连接失败: {}", _id.c_str(), ec);  // 记录错误日志
			_mgr->decAlive();         // 通知管理器减少活跃计数
			_done = true;             // 标记为完成状态（失败也算完成）
		}
	}
	else if(e == WTE_Close)         // 如果是连接关闭事件
	{
		WTSLogger::error("[{}]交易账号连接已断开: {}", _id.c_str(), ec);  // 记录错误日志
	}
}

/**
 * @brief 登录结果回调的实现
 * @param bSucc 登录是否成功
 * @param msg 登录结果消息
 * @param tradingdate 交易日
 * 
 * 登录成功后会：
 * - 保存交易日
 * - 开始查询持仓（然后依次查询账户、成交、订单）
 * 
 * 登录失败会：
 * - 标记适配器为完成状态
 * - 通知管理器减少活跃计数
 */
void TraderAdapter::onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate)
{
	if(!bSucc)                      // 如果登录失败
	{
		WTSLogger::error("[{}]交易账号登录失败: {}", _id.c_str(), msg);  // 记录错误日志
		_mgr->decAlive();           // 通知管理器减少活跃计数
		_done = true;                // 标记为完成状态
	}
	else                            // 如果登录成功
	{
		_date = tradingdate;         // 保存交易日
		WTSLogger::info("[{}]交易账号登录成功, 当前交易日:{}", _id.c_str(), tradingdate);  // 记录成功日志

		_trader_api->queryPositions();	//查持仓，查询结果会通过onRspPosition回调返回
	}
}

/**
 * @brief 登出回调的实现
 * 
 * 交易接口登出时的通知，当前实现为空，不做任何处理。
 */
void TraderAdapter::onLogout()
{
	
}

/**
 * @brief 查询账户资金的实现
 * 
 * 主动查询账户资金信息，查询结果会通过onRspAccount回调返回。
 * 只有在交易日已确定（_date不为0）时才会执行查询。
 */
void TraderAdapter::queryFund()
{
	if (_date == 0)                 // 如果交易日未确定，不执行查询
		return;

	_trader_api->queryAccount();    // 调用交易接口查询账户资金
}

/**
 * @brief 查询持仓的实现
 * 
 * 主动查询持仓信息，查询结果会通过onRspPosition回调返回。
 * 只有在交易日已确定（_date不为0）时才会执行查询。
 */
void TraderAdapter::queryPosition()
{
	if (_date == 0)                 // 如果交易日未确定，不执行查询
		return;

	_trader_api->queryPositions();  // 调用交易接口查询持仓
}

/**
 * @brief 账户资金查询结果回调的实现
 * @param ayAccounts 账户信息数组
 * 
 * 处理账户资金查询结果：
 * - 遍历所有账户信息
 * - 提取账户数据（余额、盈亏、保证金等）
 * - 通过Dumper传递给外部回调函数
 * - 查询完成后，继续查询成交明细
 */
void TraderAdapter::onRspAccount(WTSArray* ayAccounts)
{
	if(ayAccounts && ayAccounts->size() > 0)  // 如果账户数组存在且不为空
	{
		for (std::size_t idx = 0; idx < ayAccounts->size(); idx++)  // 遍历所有账户信息
		{
			WTSAccountInfo* accInfo = (WTSAccountInfo*)ayAccounts->at(idx);  // 获取第idx个账户信息对象

			// 通过Dumper转发账户数据到外部回调函数
			// 参数说明：通道ID、交易日、货币、上日余额、当前余额、动态权益、平仓盈亏、浮动盈亏、手续费、保证金、入金、出金、是否最后一条
			getDumper().on_account(_id.c_str(), _date, accInfo->getCurrency(), accInfo->getPreBalance(), accInfo->getBalance(), accInfo->getBalance() + accInfo->getDynProfit(),
				accInfo->getCloseProfit(), accInfo->getDynProfit(), accInfo->getCommission(), accInfo->getMargin(), accInfo->getDeposit(), accInfo->getWithdraw(), idx == ayAccounts->size()-1);
		}
	}

	WTSLogger::info("[{}]资金数据已更新", _id.c_str());  // 记录日志

	if(!_done)                      // 如果还未完成（未完成所有数据查询）
		_trader_api->queryTrades();  // 继续查询成交明细，查询结果会通过onRspTrades回调返回
}

/**
 * @brief 成交推送回调的实现
 * @param tradeRecord 成交记录
 * 
 * 处理实时成交推送：
 * - 提取成交数据
 * - 通过Dumper传递给外部回调函数
 * - 与查询结果不同，推送的isLast始终为true
 */
void TraderAdapter::onPushTrade(WTSTradeInfo* tInfo)
{
	WTSContractInfo* cInfo = _bd_mgr->getContract(tInfo->getCode(), tInfo->getExchg());  // 从基础数据管理器获取合约信息
	if (cInfo == NULL)              // 如果合约信息不存在，忽略该成交
		return;

	// 通过Dumper转发成交数据到外部回调函数
	// 参数说明：通道ID、交易所、合约代码、交易日、成交编号、订单号、方向、开平、数量、价格、金额、订单类型、成交类型、成交时间、是否最后一条（推送始终为true）
	getDumper().on_trade(_id.c_str(), cInfo->getExchg(), cInfo->getCode(), _date, tInfo->getTradeID(), tInfo->getRefOrder(),
		(uint32_t)tInfo->getDirection(), (uint32_t)tInfo->getOffsetType(), tInfo->getVolume(), tInfo->getPrice(), tInfo->getAmount(),
		(uint32_t)tInfo->getOrderType(), (uint32_t)tInfo->getTradeType(), tInfo->getTradeTime(), true);
}

/**
 * @brief 成交查询结果回调的实现
 * @param ayTrades 成交信息数组
 * 
 * 处理成交查询结果：
 * - 遍历所有成交记录
 * - 提取成交数据（价格、数量、方向等）
 * - 通过Dumper传递给外部回调函数
 * - 查询完成后，继续查询订单明细
 */
void TraderAdapter::onRspTrades(const WTSArray* ayTrades)
{
	if (ayTrades && ayTrades->size() > 0)  // 如果成交数组存在且不为空
	{
		for (std::size_t idx = 0; idx < ayTrades->size(); idx++)  // 遍历所有成交记录
		{
			WTSTradeInfo* pItem = (WTSTradeInfo*)((WTSArray*)ayTrades)->at(idx);  // 获取第idx个成交信息对象
			WTSContractInfo* cInfo = _bd_mgr->getContract(pItem->getCode(), pItem->getExchg());  // 从基础数据管理器获取合约信息
			if (cInfo == NULL)      // 如果合约信息不存在，跳过该成交
				continue;

			// 通过Dumper转发成交数据到外部回调函数
			// 参数说明：通道ID、交易所、合约代码、交易日、成交编号、订单号、方向、开平、数量、价格、金额、订单类型、成交类型、成交时间、是否最后一条
			getDumper().on_trade(_id.c_str(), cInfo->getExchg(), cInfo->getCode(), _date, pItem->getTradeID(), pItem->getRefOrder(),
				(uint32_t)pItem->getDirection(), (uint32_t)pItem->getOffsetType(), pItem->getVolume(), pItem->getPrice(), pItem->getAmount(),
				(uint32_t)pItem->getOrderType(), (uint32_t)pItem->getTradeType(), pItem->getTradeTime(), idx == ayTrades->size()-1);
		}
	}

	WTSLogger::info("[{}]成交明细已更新", _id.c_str());  // 记录日志

	_trader_api->queryOrders();     // 继续查询订单明细，查询结果会通过onRspOrders回调返回
}

/**
 * @brief 订单查询结果回调的实现
 * @param ayOrders 订单信息数组
 * 
 * 处理订单查询结果：
 * - 遍历所有订单记录
 * - 提取订单数据（状态、数量、价格等）
 * - 通过Dumper传递给外部回调函数
 * - 查询完成后，标记适配器为完成状态，通知管理器
 */
void TraderAdapter::onRspOrders(const WTSArray* ayOrders)
{
	if (ayOrders && ayOrders->size() > 0)  // 如果订单数组存在且不为空
	{
		for (std::size_t idx = 0; idx < ayOrders->size(); idx++)  // 遍历所有订单记录
		{
			WTSOrderInfo* pItem = (WTSOrderInfo*)((WTSArray*)ayOrders)->at(idx);  // 获取第idx个订单信息对象
			WTSContractInfo* cInfo = _bd_mgr->getContract(pItem->getCode(), pItem->getExchg());  // 从基础数据管理器获取合约信息
			if (cInfo == NULL)      // 如果合约信息不存在，跳过该订单
				continue;

			// 通过Dumper转发订单数据到外部回调函数
			// 参数说明：通道ID、交易所、合约代码、交易日、订单号、方向、开平、委托数量、剩余数量、已成交数量、价格、订单类型、价格类型、委托时间、订单状态、状态消息、是否最后一条
			getDumper().on_order(_id.c_str(), cInfo->getExchg(), cInfo->getCode(), _date, pItem->getOrderID(), pItem->getDirection(),
				pItem->getOffsetType(), pItem->getVolume(), pItem->getVolLeft(), pItem->getVolTraded(), pItem->getPrice(),
				pItem->getOrderType(), pItem->getPriceType(), pItem->getOrderTime(), pItem->getOrderState(), pItem->getStateMsg(), idx == ayOrders->size()-1);
		}
	}

	WTSLogger::info("[{}]订单明细已更新", _id.c_str());  // 记录日志
	_mgr->decAlive();               // 通知管理器减少活跃计数（所有数据查询完成）
	_done = true;                    // 标记为完成状态
}

/**
 * @brief 订单推送回调的实现
 * @param oInfo 订单信息
 * 
 * 处理实时订单推送：
 * - 如果订单已结束（非活跃状态），刷新账户和持仓
 * - 提取订单数据
 * - 通过Dumper传递给外部回调函数
 */
void TraderAdapter::onPushOrder(WTSOrderInfo* oInfo)
{
	WTSContractInfo* cInfo = _bd_mgr->getContract(oInfo->getCode(), oInfo->getExchg());  // 从基础数据管理器获取合约信息
	if (cInfo == NULL)              // 如果合约信息不存在，忽略该订单
		return;

	//如果订单回报中，订单状态是已结束，则刷新资金和持仓
	if (!oInfo->isAlive())           // 如果订单已结束（已成交、已撤销、已拒绝等）
	{
		_trader_api->queryAccount();  // 刷新账户资金
		_trader_api->queryPositions();  // 刷新持仓信息
	}

	// 通过Dumper转发订单数据到外部回调函数
	// 参数说明：通道ID、交易所、合约代码、交易日、订单号、方向、开平、委托数量、剩余数量、已成交数量、价格、订单类型、价格类型、委托时间、订单状态、状态消息、是否最后一条（推送始终为true）
	getDumper().on_order(_id.c_str(), cInfo->getExchg(), cInfo->getCode(), _date, oInfo->getOrderID(), oInfo->getDirection(),
		oInfo->getOffsetType(), oInfo->getVolume(), oInfo->getVolLeft(), oInfo->getVolTraded(), oInfo->getPrice(),
		oInfo->getOrderType(), oInfo->getPriceType(), oInfo->getOrderTime(), oInfo->getOrderState(), oInfo->getStateMsg(), true);
}

/**
 * @brief 持仓查询结果回调的实现
 * @param ayPositions 持仓信息数组
 * 
 * 处理持仓查询结果：
 * - 遍历所有持仓信息
 * - 提取持仓数据（数量、成本、盈亏等）
 * - 通过Dumper传递给外部回调函数
 * - 查询完成后，继续查询账户资金
 */
void TraderAdapter::onRspPosition(const WTSArray* ayPositions)
{
	if (ayPositions && ayPositions->size() > 0)  // 如果持仓数组存在且不为空
	{
		for (std::size_t idx = 0; idx < ((WTSArray*)ayPositions)->size(); idx++)  // 遍历所有持仓信息
		{
			WTSPositionItem* pItem = (WTSPositionItem*)(((WTSArray*)ayPositions)->at(idx));  // 获取第idx个持仓信息对象
			WTSContractInfo* cInfo = _bd_mgr->getContract(pItem->getCode());  // 从基础数据管理器获取合约信息（只需要合约代码）
			if (cInfo == NULL)      // 如果合约信息不存在，跳过该持仓
				continue;
			WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取品种信息，用于获取数量乘数

			// 通过Dumper转发持仓数据到外部回调函数
			// 参数说明：通道ID、交易所、合约代码、交易日、方向（0=多头，1=空头）、持仓数量、持仓成本、占用保证金、持仓均价、浮动盈亏、数量乘数、是否最后一条
			getDumper().on_position(_id.c_str(), cInfo->getExchg(), cInfo->getCode(), _date, (pItem->getDirection() == WDT_LONG ? 0 : 1),
				pItem->getTotalPosition(), pItem->getPositionCost(), pItem->getMargin(), pItem->getAvgPrice(),
				pItem->getDynProfit(), commInfo->getVolScale(), idx == ayPositions->size() - 1);
		}
	}

	WTSLogger::info("[{}]持仓数据已更新", _id.c_str());  // 记录日志

	if (!_done)                      // 如果还未完成（未完成所有数据查询）
		_trader_api->queryAccount();  // 继续查询账户资金，查询结果会通过onRspAccount回调返回
}

/**
 * @brief 交易错误回调的实现
 * @param err 错误信息
 * @param pData 附加数据指针
 * 
 * 处理交易接口的错误通知，记录错误日志。
 */
void TraderAdapter::onTraderError(WTSError* err, void* pData /* = NULL */)
{
	if(err)                          // 如果错误信息存在
		WTSLogger::error("[{}]交易通道出现错误: {}", _id.c_str(), err->getMessage());  // 记录错误日志
}

/**
 * @brief 获取基础数据管理器的实现
 * @return 返回基础数据管理器指针
 * 
 * 返回初始化时传入的基础数据管理器，供交易接口使用。
 */
IBaseDataMgr* TraderAdapter::getBaseDataMgr()
{
	return _bd_mgr;                  // 返回基础数据管理器指针
}

/**
 * @brief 处理交易日志的实现
 * @param ll 日志级别
 * @param message 日志消息
 * 
 * 将交易接口的日志转发到WonderTrader日志系统。
 */
void TraderAdapter::handleTraderLog(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_raw(ll, message);  // 直接转发日志到WonderTrader日志系统
}

#pragma endregion "ITraderSpi接口"


//////////////////////////////////////////////////////////////////////////
//TraderAdapterMgr - 交易适配器管理器实现
/**
 * @brief 添加适配器到管理器的实现
 * @param tname 交易通道ID
 * @param adapter 适配器智能指针引用
 * @return 返回添加是否成功
 * 
 * 添加适配器到映射表，如果ID已存在则添加失败。
 */
bool TraderAdapterMgr::addAdapter(const char* tname, TraderAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(tname) == 0)  // 如果适配器为空或通道ID为空，添加失败
		return false;

	auto it = _adapters.find(tname);  // 在映射表中查找该通道ID
	if(it != _adapters.end())         // 如果已存在
	{
		WTSLogger::error("交易通道名称相同: {}", tname);  // 记录错误日志
		return false;
	}

	_adapters[tname] = adapter;      // 将适配器添加到映射表

	return true;                      // 返回成功
}

/**
 * @brief 根据通道ID获取适配器的实现
 * @param tname 交易通道ID
 * @return 返回适配器智能指针
 * 
 * 在适配器映射表中查找指定ID的适配器。
 */
TraderAdapterPtr TraderAdapterMgr::getAdapter(const char* tname)
{
	auto it = _adapters.find(tname);  // 在映射表中查找该通道ID
	if (it != _adapters.end())        // 如果找到
	{
		return it->second;            // 返回适配器智能指针
	}

	return TraderAdapterPtr();        // 如果未找到，返回空智能指针
}

/**
 * @brief 启动所有适配器的实现
 * 
 * 启动流程：
 * - 设置活跃计数为适配器总数
 * - 遍历所有适配器，调用run方法启动
 * - 记录启动日志
 */
void TraderAdapterMgr::run()
{
	_live_cnt = _adapters.size();     // 设置活跃计数为适配器总数
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		it->second->run();            // 调用适配器的run方法启动
	}

	WTSLogger::info("{}个交易通道已启动", _adapters.size());  // 记录启动日志
}

/**
 * @brief 释放所有适配器资源的实现
 * 
 * 遍历所有适配器，调用其release方法释放资源，然后清空适配器映射表。
 */
void TraderAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		it->second->release();        // 调用适配器的release方法释放资源
	}

	_adapters.clear();                // 清空适配器映射表
}

/**
 * @brief 减少活跃适配器计数的实现
 * 
 * 当一个适配器完成数据查询后，调用此方法减少活跃计数。
 * 使用互斥锁保护计数器的原子操作。
 * 当剩余活跃数较少时，会记录日志。
 */
void TraderAdapterMgr::decAlive()
{
	_mutex.lock();                    // 加锁保护共享数据
	auto left = _live_cnt.fetch_sub(1);  // 原子操作：减少活跃计数并返回旧值
	_mutex.unlock();                  // 解锁
	if (left > 0)                     // 如果旧值大于0
		left--;                       // 计算新的剩余数（因为已经减1了）
	if(left <= 2 && left > 0)         // 如果剩余活跃数小于等于2且大于0
	{
		for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
		{
			TraderAdapterPtr trader = it->second;  // 获取适配器指针
			if (!trader->isDone())    // 如果适配器还未完成
				WTSLogger::info("{} is still undone", trader->id());  // 记录日志，显示哪些适配器还在工作
		}
	}

	WTSLogger::info("{}/{}", left, _adapters.size());  // 记录日志：剩余活跃数/总数
}

/**
 * @brief 刷新所有已完成的适配器的实现
 * 
 * 遍历所有适配器，对于已完成的适配器，重新查询持仓和资金。
 * 用于定时刷新功能，保持数据最新。
 */
void TraderAdapterMgr::refresh()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		TraderAdapterPtr trader = it->second;  // 获取适配器指针
		if (!trader->isDone())        // 如果适配器还未完成，跳过（只刷新已完成的）
			continue;
		trader->queryPosition();      // 重新查询持仓
		trader->queryFund();          // 重新查询资金
	}
}