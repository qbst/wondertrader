/*!
 * \file ITraderApi.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易通道对接接口定义文件
 * 
 * 本文件定义了WonderTrader框架中交易通道对接的核心接口。
 * 交易通道是策略与交易所之间的桥梁，负责订单的下达、撤单、查询等操作。
 * 该接口支持多种交易品种（期货、期权、股票）和多种交易接口（CTP、XTP、OES等）。
 * 
 * 主要功能：
 * 1. 交易连接管理：连接/断开交易服务器、登录/注销账户
 * 2. 订单管理：下单、撤单、订单查询、成交查询
 * 3. 账户管理：资金查询、持仓查询、结算单查询
 * 4. 多品种支持：期货、期权、股票的统一接口
 * 5. 事件通知：通过回调接口向策略推送交易相关事件
 */
#pragma once
#include <set>
#include <map>
#include <stdint.h>
#include <functional>
#include "WTSTypes.h"

NS_WTP_BEGIN
class WTSVariant;        // 配置参数变体类
class WTSEntrust;        // 委托单结构
class WTSOrderInfo;      // 订单信息结构
class WTSTradeInfo;      // 成交信息结构
class WTSEntrustAction;  // 委托操作结构
class WTSAccountInfo;    // 账户信息结构
class WTSPositionItem;   // 持仓项结构
class WTSContractInfo;   // 合约信息结构
class WTSError;          // 错误信息结构
class WTSTickData;       // Tick数据结构
class WTSNotify;         // 通知信息结构
class WTSArray;          // 数组结构
class IBaseDataMgr;      // 基础数据管理器接口

typedef std::function<void()>	CommonExecuter;    // 通用执行器函数类型

#pragma region "Stock Trading API definations"
/**
 * @brief 股票交易接口回调类
 * 
 * 该类定义了股票交易特有的回调接口，主要提供融资融券等股票特有功能。
 * 目前为预留接口，为将来的股票交易功能扩展做准备。
 * 
 * Added By Wesley @ 2020/05/06
 */
class IStkTraderSpi
{

};

/**
 * @brief 股票交易接口类
 * 
 * 该类定义了股票交易的基本接口，主要提供融资融券等股票特有接口。
 * 目前为预留接口，先把接口的相互框架搭建起来，为将来的股票交易功能扩展做准备。
 * 
 * Added By Wesley @ 2020/05/06
 */
class IStkTraderApi
{
};
#pragma endregion

#pragma region "Option Trading API definations"
/**
 * @brief 期权交易接口回调类
 * 
 * 该类定义了期权交易的回调接口，包括委托回报、订单推送、成交推送等。
 * 为期权交易提供完整的回调机制。
 * 
 * Added By Wesley @ 2020/05/06
 */
class IOptTraderSpi
{
public:
	/**
	 * @brief 期权委托回报回调
	 * @param entrust 委托单信息
	 * @param err 错误信息，成功时为NULL
	 */
	virtual void onRspEntrustOpt(WTSEntrust* entrust, WTSError *err) {}
	
	/**
	 * @brief 期权订单查询回报回调
	 * @param ayOrders 订单信息数组
	 */
	virtual void onRspOrdersOpt(const WTSArray* ayOrders) {}
	
	/**
	 * @brief 期权订单推送回调
	 * @param orderInfo 订单信息
	 */
	virtual void onPushOrderOpt(WTSOrderInfo* orderInfo) {}
};

/**
 * @brief 期权交易接口类
 * 
 * 该类定义了期权交易的基本接口，主要提供报价、行权等期权特有功能。
 * 目前为预留接口，先把接口的相互框架搭建起来，为将来的期权交易功能扩展做准备。
 * 
 * Added By Wesley @ 2020/05/06
 */
class IOptTraderApi
{
public:
	/**
	 * @brief 期权下单接口
	 * @param eutrust 委托单的具体数据结构
	 * @return 下单结果，成功返回0，失败返回负值
	 */
	virtual int orderInsertOpt(WTSEntrust* eutrust) { return -1; }

	/**
	 * @brief 期权订单操作接口
	 * @param action 操作的具体数据结构
	 * @return 操作结果，成功返回0，失败返回负值
	 */
	virtual int orderActionOpt(WTSEntrustAction* action) { return -1; }

	/**
	 * @brief 查询期权订单
	 * @param bType 业务类型
	 * @return 查询结果，成功返回0，失败返回负值
	 */
	virtual int	queryOrdersOpt(WTSBusinessType bType) { return -1; }
};
#pragma endregion


/**
 * @brief 交易接口回调类
 * 
 * 该类定义了交易接口的所有回调函数，包括连接事件、委托回报、成交推送等。
 * 策略通过实现该接口来接收交易相关的各种事件通知。
 */
class ITraderSpi
{
public:
	/**
	 * @brief 获取基础数据管理器
	 * @return 基础数据管理器对象指针
	 */
	virtual IBaseDataMgr*	getBaseDataMgr() = 0;

	/**
	 * @brief 处理交易接口的日志
	 * @param ll 日志级别
	 * @param message 日志消息
	 */
	virtual void handleTraderLog(WTSLogLevel ll, const char* message){}

	/**
	 * @brief 获取股票交易接口Spi
	 * @return 股票交易接口Spi对象指针
	 */
	virtual IStkTraderSpi* getStkSpi(){ return NULL; }

	/**
	 * @brief 获取期权交易接口Spi
	 * @return 期权交易接口Spi对象指针
	 */
	virtual IOptTraderSpi* getOptSpi(){ return NULL; }

public:
	/**
	 * @brief 处理交易接口事件
	 * @param e 事件类型
	 * @param ec 事件代码
	 */
	virtual void handleEvent(WTSTraderEvent e, int32_t ec) = 0;

	/**
	 * @brief 登录回报回调
	 * @param bSucc 登录是否成功
	 * @param msg 登录结果消息
	 * @param tradingdate 交易日期
	 */
	virtual void onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate) = 0;

	/**
	 * @brief 注销回报回调
	 */
	virtual void onLogout(){}

	/**
	 * @brief 委托回报回调
	 * @param entrust 委托单信息
	 * @param err 错误信息，成功时为NULL
	 */
	virtual void onRspEntrust(WTSEntrust* entrust, WTSError *err){}

	/**
	 * @brief 资金查询回报回调
	 * @param ayAccounts 账户信息数组
	 */
	virtual void onRspAccount(WTSArray* ayAccounts) {}

	/**
	 * @brief 持仓查询回报回调
	 * @param ayPositions 持仓信息数组
	 */
	virtual void onRspPosition(const WTSArray* ayPositions){}

	/**
	 * @brief 订单查询回报回调
	 * @param ayOrders 订单信息数组
	 */
	virtual void onRspOrders(const WTSArray* ayOrders){}

	/**
	 * @brief 成交查询回报回调
	 * @param ayTrades 成交信息数组
	 */
	virtual void onRspTrades(const WTSArray* ayTrades){}

	/**
	 * @brief 结算单查询回报回调
	 * @param uDate 结算日期
	 * @param content 结算单内容
	 */
	virtual void onRspSettlementInfo(uint32_t uDate, const char* content){}

	/**
	 * @brief 订单回报推送回调
	 * @param orderInfo 订单信息
	 */
	virtual void onPushOrder(WTSOrderInfo* orderInfo){}

	/**
	 * @brief 成交回报推送回调
	 * @param tradeRecord 成交记录
	 */
	virtual void onPushTrade(WTSTradeInfo* tradeRecord){}

	/**
	 * @brief 交易接口错误回报回调
	 * @param err 错误信息
	 * @param pData 相关数据指针
	 */
	virtual void onTraderError(WTSError* err, void* pData = NULL){}

	/**
	 * @brief 合约状态推送回调
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param state 交易状态
	 */
	virtual void onPushInstrumentStatus(const char* exchg, const char* code, WTSTradeStatus state) {}
};

/**
 * @brief 交易接口管理类
 * 
 * 该类定义了交易接口的核心功能，包括连接管理、订单管理、查询功能等。
 * 策略通过该接口与交易所进行交互，执行各种交易操作。
 */
class ITraderApi
{
public:
	/**
	 * @brief 虚析构函数
	 */
	virtual ~ITraderApi(){}

	/**
	 * @brief 获取股票交易接口
	 * @return 股票交易接口对象指针
	 */
	virtual IStkTraderApi* getStkTrader() { return NULL; }
	
	/**
	 * @brief 获取期权交易接口
	 * @return 期权交易接口对象指针
	 */
	virtual IOptTraderApi* getOptTrader() { return NULL; }

public:
	/**
	 * @brief 初始化交易接口
	 * @param params 配置参数对象
	 * @return 初始化是否成功
	 */
	virtual bool init(WTSVariant *params) { return false; }

	/**
	 * @brief 释放交易接口资源
	 */
	virtual void release(){}

	/**
	 * @brief 注册回调接口
	 * @param listener 回调接口对象指针
	 */
	virtual void registerSpi(ITraderSpi *listener) {}


	//////////////////////////////////////////////////////////////////////////
	//业务逻辑接口

	/**
	 * @brief 连接交易服务器
	 */
	virtual void connect() {}

	/**
	 * @brief 断开与交易服务器的连接
	 */
	virtual void disconnect() {}

	/**
	 * @brief 检查是否已连接
	 * @return 已连接返回true，否则返回false
	 */
	virtual bool isConnected() { return false; }

	/**
	 * @brief 生成委托单号
	 * @param buffer 缓冲区指针
	 * @param length 缓冲区长度
	 * @return 生成是否成功
	 */
	virtual bool makeEntrustID(char* buffer, int length){ return false; }

	/**
	 * @brief 登录交易账户
	 * @param user 用户名
	 * @param pass 密码
	 * @param productInfo 产品信息
	 * @return 登录结果，成功返回0，失败返回负值
	 */
	virtual int login(const char* user, const char* pass, const char* productInfo) { return -1; }

	/**
	 * @brief 注销交易账户
	 * @return 注销结果，成功返回0，失败返回负值
	 */
	virtual int logout() { return -1; }

	/**
	 * @brief 下单接口
	 * @param eutrust 委托单的具体数据结构
	 * @return 下单结果，成功返回0，失败返回负值
	 */
	virtual int orderInsert(WTSEntrust* eutrust) { return -1; }

	/**
	 * @brief 订单操作接口
	 * @param action 操作的具体数据结构
	 * @return 操作结果，成功返回0，失败返回负值
	 */
	virtual int orderAction(WTSEntrustAction* action) { return -1; }

	/**
	 * @brief 查询账户信息
	 * @return 查询结果，成功返回0，失败返回负值
	 */
	virtual int queryAccount() { return -1; }

	/**
	 * @brief 查询持仓信息
	 * @return 查询结果，成功返回0，失败返回负值
	 */
	virtual int queryPositions() { return -1; }

	/**
	 * @brief 查询所有订单
	 * @return 查询结果，成功返回0，失败返回负值
	 */
	virtual int queryOrders() { return -1; }

	/**
	 * @brief 查询成交明细
	 * @return 查询结果，成功返回0，失败返回负值
	 */
	virtual int	queryTrades() { return -1; }

	/**
	 * @brief 查询结算单
	 * @param uDate 结算日期
	 * @return 查询结果，成功返回0，失败返回负值
	 */
	virtual int querySettlement(uint32_t uDate){ return 0; }

};

NS_WTP_END

//获取IDataMgr的函数指针类型
typedef wtp::ITraderApi* (*FuncCreateTrader)();        // 创建交易接口的函数指针类型
typedef void(*FuncDeleteTrader)(wtp::ITraderApi* &trader);    // 删除交易接口的函数指针类型
