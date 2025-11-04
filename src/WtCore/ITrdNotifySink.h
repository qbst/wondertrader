/*!
 * \file ITrdNotifySink.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易通知接收器接口头文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件定义了交易通知接收器接口ITrdNotifySink，用于接收交易相关的各种通知和回调。
 * 该接口定义了交易过程中的关键事件回调，包括成交回报、订单回报、持仓更新、
 * 交易通道状态变化、委托回报和资金变化等。
 * 
 * 设计模式：
 * 使用了观察者模式（Observer Pattern），ITrdNotifySink作为观察者接口，
 * 交易适配器或交易引擎作为被观察者，当交易事件发生时，会通知所有注册的观察者。
 * 
 * 主要功能：
 * 1. 成交回报：当订单成交时通知接收器。
 * 2. 订单回报：当订单状态发生变化时通知接收器。
 * 3. 持仓更新：当持仓发生变化时通知接收器。
 * 4. 通道状态：当交易通道就绪或丢失时通知接收器。
 * 5. 委托回报：当下单结果返回时通知接收器。
 * 6. 资金回调：当账户资金发生变化时通知接收器。
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含标准整数类型定义
#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * @class ITrdNotifySink
 * @brief 交易通知接收器接口类
 *
 * 该接口定义了交易过程中各种事件的通知回调方法。
 * 任何需要接收交易通知的类都可以实现此接口，并注册到交易适配器或交易引擎中。
 * 当交易事件发生时，交易适配器或交易引擎会调用相应的回调方法通知接收器。
 */
class ITrdNotifySink
{
public:
	/**
	 * @brief 成交回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向（true表示买入，false表示卖出）
	 * @param vol 成交数量
	 * @param price 成交价格
	 *
	 * 当订单成交时被调用，通知接收器成交信息。
	 * 这是一个纯虚函数，子类必须实现此方法。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) = 0;  // 成交回报回调

	/**
	 * @brief 订单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向（true表示买入，false表示卖出）
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余数量（未成交数量）
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销，默认false
	 *
	 * 当订单状态发生变化时被调用，通知接收器订单状态信息。
	 * 这是一个纯虚函数，子类必须实现此方法。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled = false) = 0;  // 订单回报回调

	/**
	 * @brief 持仓更新回调
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为做多方向（true表示做多，false表示做空）
	 * @param prevol 变化前持仓数量
	 * @param preavail 变化前可用持仓数量
	 * @param newvol 变化后持仓数量
	 * @param newavail 变化后可用持仓数量
	 * @param tradingday 交易日
	 *
	 * 当持仓发生变化时被调用，通知接收器持仓变化信息。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) {}  // 持仓更新回调

	/**
	 * @brief 交易通道就绪回调
	 *
	 * 当交易通道就绪时被调用，通知接收器可以开始交易。
	 * 这是一个纯虚函数，子类必须实现此方法。
	 */
	virtual void on_channel_ready() = 0;  // 交易通道就绪回调

	/**
	 * @brief 交易通道丢失回调
	 *
	 * 当交易通道丢失时被调用，通知接收器交易通道已断开。
	 * 这是一个纯虚函数，子类必须实现此方法。
	 */
	virtual void on_channel_lost() = 0;  // 交易通道丢失回调

	/**
	 * @brief 下单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功（true表示下单成功，false表示下单失败）
	 * @param message 消息内容（成功或失败的原因）
	 *
	 * 当下单结果返回时被调用，通知接收器下单是否成功。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message){}  // 下单回报回调

	/**
	 * @brief 资金回调
	 * @param currency 货币类型
	 * @param prebalance 变化前账户余额
	 * @param balance 变化后账户余额
	 * @param dynbalance 动态余额（账户余额 + 浮动盈亏）
	 * @param avaliable 可用资金
	 * @param closeprofit 平仓盈亏
	 * @param dynprofit 浮动盈亏
	 * @param margin 保证金占用
	 * @param fee 手续费
	 * @param deposit 入金
	 * @param withdraw 出金
	 *
	 * 当账户资金发生变化时被调用，通知接收器资金变化信息。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void on_account(const char* currency, double prebalance, double balance, double dynbalance, double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw){}  // 资金回调
};

NS_WTP_END  // 结束WonderTrader命名空间