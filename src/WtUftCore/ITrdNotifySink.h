/*!
 * \file ITrdNotifySink.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易通知接口头文件
 *
 * 本文件定义了ITrdNotifySink接口，用于接收交易事件通知。
 *
 * 设计逻辑：
 * 1. 接口设计：采用纯虚函数接口，定义交易事件回调的标准接口
 * 2. 事件类型：支持成交、订单、持仓更新、通道状态等多种事件类型
 * 3. 回调机制：通过虚函数实现多态，允许不同的实现类处理交易事件
 * 4. 通道管理：支持交易通道就绪和丢失的事件通知
 * 5. 持仓更新：支持详细的持仓信息更新，包括昨仓、今仓、可用数量等
 *
 * 主要功能：
 * - 成交回报：接收成交事件通知
 * - 订单回报：接收订单状态变化通知
 * - 持仓更新：接收持仓变化通知
 * - 通道管理：接收交易通道状态变化通知
 */
#pragma once
#include <stdint.h>  // 标准整数类型定义
#include "../Includes/WTSMarcos.h"  // WonderTrader宏定义

NS_WTP_BEGIN  // WonderTrader命名空间开始

/**
 * @class ITrdNotifySink
 * @brief 交易通知接口类
 * 
 * 定义交易事件通知的标准接口，用于接收交易接口的各种事件回调。
 * 交易接口实现类通过实现此接口来接收交易事件通知。
 */
class ITrdNotifySink
{
public:
	/**
	 * @brief 成交回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多（true-做多，false-做空）
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 当订单成交时调用，通知成交信息。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double vol, double price) = 0;

	/**
	 * @brief 订单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多（true-做多，false-做空）
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param totalQty 总委托数量
	 * @param leftQty 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销，默认为false
	 * 
	 * 当订单状态发生变化时调用，通知订单的最新状态。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled = false) = 0;

	/**
	 * @brief 持仓更新回调
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多（true-做多，false-做空）
	 * @param prevol 昨仓数量
	 * @param preavail 昨仓可用数量
	 * @param newvol 今仓数量
	 * @param newavail 今仓可用数量
	 * @param tradingday 交易日
	 * 
	 * 当持仓发生变化时调用，通知最新的持仓信息。
	 * 默认实现为空，子类可以重写此方法处理持仓更新。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) {}

	/**
	 * @brief 交易通道就绪回调
	 * @param tradingday 交易日
	 * 
	 * 当交易通道连接成功并准备就绪时调用，表示可以开始交易。
	 */
	virtual void on_channel_ready(uint32_t tradingday) = 0;

	/**
	 * @brief 交易通道丢失回调
	 * 
	 * 当交易通道断开连接时调用，表示交易通道已不可用。
	 */
	virtual void on_channel_lost() = 0;

	/**
	 * @brief 下单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息内容
	 * 
	 * 当下单操作完成时调用，通知下单结果。
	 * 默认实现为空，子类可以重写此方法处理下单回报。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message){}
};

NS_WTP_END  // WonderTrader命名空间结束