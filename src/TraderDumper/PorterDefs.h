/*!
 * \file PorterDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易数据转储模块的回调函数类型定义
 * 
 * 本文件定义了TraderDumper模块对外提供的所有回调函数类型。
 * 这些回调函数用于将交易数据（账户、订单、成交、持仓）传递给外部调用者。
 * 所有回调函数都使用PORTER_FLAG导出标志，确保跨DLL/so调用的兼容性。
 * 
 * 设计说明：
 * - 使用函数指针类型定义，支持C语言接口
 * - 所有回调函数都包含isLast参数，用于标识是否为最后一条数据
 * - 使用WtUInt32/WtUInt64等平台无关的整数类型，确保跨平台兼容性
 */
#pragma once

#include <stdint.h>                // 标准整数类型定义
#include "../Includes/WTSTypes.h"  // WonderTrader类型定义，包含WtUInt32、WtUInt64等类型

/*
*	回调函数定义
* 
* 以下定义了四种交易数据的回调函数类型：
* 1. FuncOnAccount - 账户资金信息回调
* 2. FuncOnOrder - 订单信息回调
* 3. FuncOnTrade - 成交信息回调
* 4. FuncOnPosition - 持仓信息回调
*/

/**
 * @typedef FuncOnAccount
 * @brief 账户资金信息回调函数类型
 * 
 * 当交易接口返回账户资金信息时，会调用此回调函数。
 * 回调函数会将账户的所有资金相关数据传递给外部调用者。
 * 
 * @param channelid 交易通道ID（字符串），标识是哪个交易账号
 * @param curTDate 当前交易日（32位无符号整数），格式为YYYYMMDD
 * @param currency 货币类型（字符串），如"CNY"、"USD"等
 * @param prebalance 上日余额（双精度浮点数），交易日开始时的账户余额
 * @param balance 当前余额（双精度浮点数），账户当前可用余额
 * @param dynbalance 动态权益（双精度浮点数），包含浮动盈亏后的总权益
 * @param closeprofit 平仓盈亏（双精度浮点数），已实现盈亏
 * @param dynprofit 浮动盈亏（双精度浮点数），持仓的未实现盈亏
 * @param fee 手续费（双精度浮点数），累计产生的手续费
 * @param margin 占用保证金（双精度浮点数），当前持仓占用的保证金
 * @param deposit 入金（双精度浮点数），累计入金金额
 * @param withdraw 出金（双精度浮点数），累计出金金额
 * @param isLast 是否为最后一条数据（布尔值），true表示这是最后一条账户数据
 */
typedef void(PORTER_FLAG *FuncOnAccount)(const char* channelid, WtUInt32 curTDate, const char* currency, double prebalance, double balance, 
	double dynbalance, double closeprofit, double dynprofit, double fee, double margin, double deposit, double withdraw, bool isLast);

/**
 * @typedef FuncOnOrder
 * @brief 订单信息回调函数类型
 * 
 * 当交易接口返回订单信息时，会调用此回调函数。
 * 包括订单查询结果和订单状态推送。
 * 
 * @param channelid 交易通道ID（字符串），标识是哪个交易账号
 * @param exchg 交易所代码（字符串），如"SHFE"、"CFFEX"等
 * @param code 合约代码（字符串），如"rb2305"、"IF2305"等
 * @param curTDate 当前交易日（32位无符号整数），格式为YYYYMMDD
 * @param orderid 订单号（字符串），交易所或券商分配的订单编号
 * @param direct 买卖方向（32位无符号整数），0=买入/做多，1=卖出/做空
 * @param offset 开平标志（32位无符号整数），0=开仓，1=平仓，2=平今，3=平昨
 * @param volume 委托数量（双精度浮点数），订单的总委托数量
 * @param leftover 剩余数量（双精度浮点数），尚未成交的数量
 * @param traded 已成交数量（双精度浮点数），已经成交的数量
 * @param price 委托价格（双精度浮点数），订单的委托价格
 * @param ordertype 订单类型（32位无符号整数），0=限价，1=市价等
 * @param pricetype 价格类型（32位无符号整数），限价、市价等价格类型
 * @param ordertime 委托时间（64位无符号整数），订单委托的时间戳（毫秒）
 * @param state 订单状态（32位无符号整数），0=已报，1=部成，2=全成，3=部撤，4=全撤，5=已拒等
 * @param statemsg 状态信息（字符串），订单状态的文字描述
 * @param isLast 是否为最后一条数据（布尔值），true表示这是最后一条订单数据
 */
typedef void(PORTER_FLAG *FuncOnOrder)(const char* channelid, const char* exchg, const char* code, WtUInt32 curTDate, 
	const char* orderid, WtUInt32 direct, WtUInt32 offset, double volume, double leftover, double traded, double price, WtUInt32 ordertype, 
	WtUInt32 pricetype, WtUInt64 ordertime, WtUInt32 state, const char* statemsg, bool isLast);

/**
 * @typedef FuncOnTrade
 * @brief 成交信息回调函数类型
 * 
 * 当交易接口返回成交信息时，会调用此回调函数。
 * 包括成交查询结果和成交推送。
 * 
 * @param channelid 交易通道ID（字符串），标识是哪个交易账号
 * @param exchg 交易所代码（字符串），如"SHFE"、"CFFEX"等
 * @param code 合约代码（字符串），如"rb2305"、"IF2305"等
 * @param curTDate 当前交易日（32位无符号整数），格式为YYYYMMDD
 * @param tradeid 成交编号（字符串），交易所分配的成交编号
 * @param orderid 订单号（字符串），该成交对应的订单编号
 * @param direct 买卖方向（32位无符号整数），0=买入/做多，1=卖出/做空
 * @param offset 开平标志（32位无符号整数），0=开仓，1=平仓，2=平今，3=平昨
 * @param volume 成交数量（双精度浮点数），本次成交的数量
 * @param price 成交价格（双精度浮点数），本次成交的价格
 * @param amount 成交金额（双精度浮点数），成交数量×成交价格
 * @param ordertype 订单类型（32位无符号整数），0=限价，1=市价等
 * @param tradetype 成交类型（32位无符号整数），普通成交、撤单成交等
 * @param tradetime 成交时间（64位无符号整数），成交发生的时间戳（毫秒）
 * @param isLast 是否为最后一条数据（布尔值），true表示这是最后一条成交数据
 */
typedef void(PORTER_FLAG *FuncOnTrade)(const char* channelid, const char* exchg, const char* code, WtUInt32 curTDate,  
	const char* tradeid, const char* orderid, WtUInt32 direct, WtUInt32 offset, double volume, double price, 
	double amount, WtUInt32 ordertype, WtUInt32 tradetype, WtUInt64 tradetime, bool isLast);

/**
 * @typedef FuncOnPosition
 * @brief 持仓信息回调函数类型
 * 
 * 当交易接口返回持仓信息时，会调用此回调函数。
 * 包括持仓查询结果和持仓变化推送。
 * 
 * @param channelid 交易通道ID（字符串），标识是哪个交易账号
 * @param exchg 交易所代码（字符串），如"SHFE"、"CFFEX"等
 * @param code 合约代码（字符串），如"rb2305"、"IF2305"等
 * @param curTDate 当前交易日（32位无符号整数），格式为YYYYMMDD
 * @param direct 持仓方向（32位无符号整数），0=多头持仓，1=空头持仓
 * @param volume 持仓数量（双精度浮点数），当前持仓的总数量
 * @param cost 持仓成本（双精度浮点数），持仓的总成本金额
 * @param margin 占用保证金（双精度浮点数），该持仓占用的保证金
 * @param avgpx 持仓均价（双精度浮点数），持仓的平均开仓价格
 * @param dynprofit 浮动盈亏（双精度浮点数），该持仓的未实现盈亏
 * @param volscale 数量乘数（32位无符号整数），合约的数量乘数（如1手=10吨，则volscale=10）
 * @param isLast 是否为最后一条数据（布尔值），true表示这是最后一条持仓数据
 */
typedef void(PORTER_FLAG *FuncOnPosition)(const char* channelid, const char* exchg, const char* code, WtUInt32 curTDate, WtUInt32 direct,
	double volume, double cost, double margin, double avgpx, double dynprofit, WtUInt32 volscale, bool isLast);

