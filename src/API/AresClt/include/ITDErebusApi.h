/**
 * @file ITDErebusApi.h
 * @brief TDErebus交易系统API接口定义文件
 * 
 * 本文件定义了TDErebus交易系统的核心API接口，主要包含三个核心接口类：
 * 1. ITDErebusApp - 应用程序控制接口，管理交易应用的生命周期
 * 2. ITDErebusSpi - 交易事件回调接口，处理各种交易响应和推送
 * 3. ITDErebusApi - 主交易API接口，提供登录、查询、下单等核心功能
 * 
 * 设计逻辑：
 * - 采用异步回调模式，所有请求都通过Spi接口异步返回结果
 * - 分离应用控制和交易操作，提高系统模块化程度
 * - 使用标准化的请求/响应数据结构，确保数据格式统一
 * - 支持批量查询和实时推送，满足不同业务场景需求
 */

#pragma once                                    // 防止头文件重复包含的预处理指令
#include "XTTradeStruct.h"                      // 包含XT交易相关的数据结构定义

/**
 * @class ITDErebusApp
 * @brief TDErebus应用程序控制接口类
 * 
 * 该接口负责管理TDErebus交易应用的生命周期，包括启动和停止操作。
 * 通过此接口可以控制整个交易应用的运行状态，是系统的入口点。
 */
class ITDErebusApp
{
public:
	/**
	 * @brief 启动交易应用工作
	 * @param param 启动参数字符串，包含配置信息或连接参数
	 * @return 0表示启动成功，非0表示启动失败
	 * 
	 * 初始化交易系统，建立与服务器的连接，开始接收交易数据
	 */
	virtual		int		BeginWork(const char* param) = 0;

	/**
	 * @brief 结束交易应用工作
	 * 
	 * 断开与服务器的连接，清理资源，优雅地关闭交易应用
	 * 程序退出前应调用此方法确保资源正确释放
	 */
	virtual		void	EndWork() = 0;
};

/**
 * @class ITDErebusSpi
 * @brief TDErebus交易事件回调接口类
 * 
 * 该接口定义了所有交易相关事件的回调方法，客户端需要实现此接口
 * 来处理连接状态变化、交易响应、实时推送等各种事件。
 * 所有方法都有默认空实现，客户端只需重写关心的方法。
 */
class ITDErebusSpi
{
public:
	/**
	 * @brief 前置连接成功回调
	 * 
	 * 当与交易服务器建立连接成功时触发此回调
	 * 连接成功后可以进行登录等后续操作
	 */
	virtual		void	OnFrontConnected(){}

	/**
	 * @brief 前置连接断开回调
	 * 
	 * 当与交易服务器连接断开时触发此回调
	 * 可在此处理断线重连逻辑或清理相关状态
	 */
	virtual		void	OnFrontDisconnected(){}

	/**
	 * @brief 用户登录响应回调
	 * @param data 登录响应数据指针，包含登录结果信息
	 * @param error 错误信息指针，登录失败时包含具体错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理用户登录请求的响应结果
	 */
	virtual		void	OnRspUserLogin(tagXTRspUserLoginField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 查询资金账户响应回调
	 * @param data 账户数据指针，包含资金余额、可用资金等信息
	 * @param error 错误信息指针，查询失败时包含错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理资金账户查询请求的响应结果
	 */
	virtual		void	OnRspQryTradingAccount(tagXTRspAccountField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 查询投资者持仓响应回调
	 * @param data 持仓数据指针，包含持仓数量、成本价等信息
	 * @param error 错误信息指针，查询失败时包含错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理持仓查询请求的响应结果，可能返回多条持仓记录
	 */
	virtual		void	OnRspQryInvestorPosition(tagXTRspPositionField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 查询委托单响应回调
	 * @param data 委托单数据指针，包含委托价格、数量、状态等信息
	 * @param error 错误信息指针，查询失败时包含错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理委托单查询请求的响应结果，可能返回多条委托记录
	 */
	virtual		void	OnRspQryOrder(tagXTOrderField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 查询成交记录响应回调
	 * @param data 成交数据指针，包含成交价格、数量、时间等信息
	 * @param error 错误信息指针，查询失败时包含错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理成交查询请求的响应结果，可能返回多条成交记录
	 */
	virtual		void	OnRspQryTrade(tagXTTradeField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 报单录入响应回调
	 * @param data 报单录入请求数据指针，回传原始请求信息
	 * @param error 错误信息指针，报单失败时包含错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理报单录入请求的响应结果
	 */
	virtual		void	OnRspOrderInsert(tagXTReqOrderInsertField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 报单录入错误回调
	 * @param data 报单录入请求数据指针，回传导致错误的原始请求
	 * @param error 错误信息指针，包含具体的错误描述
	 * 
	 * 当报单录入发生错误时触发此回调，用于处理下单失败的情况
	 */
	virtual		void	OnErrRtnOrderInsert(tagXTReqOrderInsertField* data, tagXTRspInfoField* error){}

	/**
	 * @brief 报单操作响应回调
	 * @param data 报单操作请求数据指针，回传原始撤单请求信息
	 * @param error 错误信息指针，操作失败时包含错误描述
	 * @param id 请求标识符，用于匹配请求和响应
	 * @param last 是否为最后一条响应数据的标志
	 * 
	 * 处理报单操作（如撤单）请求的响应结果
	 */
	virtual		void	OnRspOrderAction(tagXTReqOrderCancelField* data, tagXTRspInfoField* error, int id, bool last){}

	/**
	 * @brief 报单操作错误回调
	 * @param data 报单操作请求数据指针，回传导致错误的原始请求
	 * @param error 错误信息指针，包含具体的错误描述
	 * 
	 * 当报单操作发生错误时触发此回调，用于处理撤单失败等情况
	 */
	virtual		void	OnErrRtnOrderAction(tagXTReqOrderCancelField* data, tagXTRspInfoField* error){}

	/**
	 * @brief 报单状态变化推送回调
	 * @param data 报单数据指针，包含最新的报单状态信息
	 * 
	 * 当报单状态发生变化时（如部分成交、全部成交、已撤单等）实时推送
	 * 用于实时监控报单状态变化
	 */
	virtual		void	OnRtnOrder(tagXTOrderField* data){}

	/**
	 * @brief 成交回报推送回调
	 * @param data 成交数据指针，包含成交的详细信息
	 * 
	 * 当有新的成交发生时实时推送成交信息
	 * 用于实时监控成交情况和更新持仓
	 */
	virtual		void	OnRtnTrade(tagXTTradeField* data){}
protected:
private:
};

/**
 * @class ITDErebusApi
 * @brief TDErebus主交易API接口类
 * 
 * 该接口是TDErebus交易系统的主要操作接口，提供了完整的交易功能：
 * 1. 用户认证（登录/登出）
 * 2. 信息查询（账户、持仓、委托、成交）
 * 3. 交易操作（下单、撤单）
 * 
 * 所有操作都是异步的，结果通过ITDErebusSpi接口回调返回
 */
class ITDErebusApi
{
public:
	/**
	 * @brief 注册事件回调接口
	 * @param 指向实现了ITDErebusSpi接口的对象指针
	 * 
	 * 注册后，所有的交易事件都将通过此Spi接口进行回调通知
	 * 必须在其他操作之前调用
	 */
	virtual		void	RegisterSpi(ITDErebusSpi*) = 0;

	/**
	 * @brief 用户登录请求
	 * @param req 登录请求数据指针，包含用户名、密码等认证信息
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 发送用户登录请求，结果通过OnRspUserLogin回调返回
	 * 登录成功后才能进行其他交易操作
	 */
	virtual		int		ReqLogon(tagXTReqUserLoginField* req) = 0;

	/**
	 * @brief 用户登出请求
	 * 
	 * 发送用户登出请求，断开交易会话
	 * 登出后需要重新登录才能进行交易操作
	 */
	virtual		void	ReqLogout() = 0;

	/**
	 * @brief 查询资金账户请求
	 * @param req 账户查询请求数据指针，包含查询条件
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 查询账户资金信息，结果通过OnRspQryTradingAccount回调返回
	 */
	virtual		int		ReqQryAccount(tagXTReqQryAccountField* req) = 0;

	/**
	 * @brief 查询持仓请求
	 * @param req 持仓查询请求数据指针，包含查询条件
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 查询投资者持仓信息，结果通过OnRspQryInvestorPosition回调返回
	 * 可能返回多条持仓记录
	 */
	virtual		int		ReqQryPosition(tagXTReqQryPositionField* req) = 0;

	/**
	 * @brief 查询委托单请求
	 * @param req 委托查询请求数据指针，包含查询条件和时间范围
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 查询委托单信息，结果通过OnRspQryOrder回调返回
	 * 可根据时间范围和状态过滤委托单
	 */
	virtual		int		ReqQryOrder(tagXTReqQryOrderField* req) = 0;

	/**
	 * @brief 查询成交记录请求
	 * @param req 成交查询请求数据指针，包含查询条件和时间范围
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 查询成交记录信息，结果通过OnRspQryTrade回调返回
	 * 可根据时间范围过滤成交记录
	 */
	virtual		int		ReqQryTrade(tagXTReqQryTradeField* req) = 0;

	/**
	 * @brief 报单录入请求（下单）
	 * @param req 报单录入请求数据指针，包含合约、价格、数量等下单信息
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 发送下单请求，结果通过OnRspOrderInsert回调返回
	 * 报单状态变化会通过OnRtnOrder推送
	 */
	virtual		int		ReqOrderInsert(tagXTReqOrderInsertField* req) = 0;

	/**
	 * @brief 报单撤销请求（撤单）
	 * @param req 撤单请求数据指针，包含要撤销的报单标识信息
	 * @return 0表示请求发送成功，非0表示请求发送失败
	 * 
	 * 发送撤单请求，结果通过OnRspOrderAction回调返回
	 * 撤单结果会通过OnRtnOrder推送报单状态变化
	 */
	virtual		int		ReqOrderCancel(tagXTReqOrderCancelField* req) = 0;
protected:

private:
};































