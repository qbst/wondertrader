/*!
 * \file ExpHftContext.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展HFT策略上下文类实现文件
 * 
 * 本文件实现了ExpHftContext类的所有方法，通过WtRtRunner将策略事件转发给外部语言实现的回调函数。
 * 每个事件处理方法都会先调用基类的对应方法（如果需要），然后通过WtRtRunner通知外部语言。
 */
#include "ExpHftContext.h"  // ExpHftContext类定义
#include "WtRtRunner.h"  // 运行时运行器
#include "../Share/StrUtil.hpp"  // 字符串工具函数

extern WtRtRunner& getRunner();  // 获取WtRtRunner单例对象

/**
 * @brief K线闭合事件处理
 * 
 * 当订阅的K线周期完成并生成新的K线时调用，先构建完整的周期字符串，然后通知外部语言
 * 
 * @param code 合约代码
 * @param period K线周期（如"m1"等）
 * @param times 周期倍数（如"m5"中的5）
 * @param newBar 新生成的K线数据结构指针
 */
void ExpHftContext::on_bar(const char* code, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (newBar == NULL)  // 如果K线数据为空，直接返回
		return;

	thread_local static char realPeriod[8] = { 0 };  // 线程局部静态变量，用于存储完整周期字符串
	fmtutil::format_to(realPeriod, "{}{}", period, times);  // 格式化周期字符串（如"m1"+"5"->"m15"）

	getRunner().ctx_on_bar(_context_id, code, realPeriod, newBar, ET_HFT);  // 通知外部语言K线闭合

	HftStraBaseCtx::on_bar(code, period, times, newBar);  // 调用基类方法，处理K线闭合的内部逻辑
}

/**
 * @brief 交易通道断开事件处理
 * 
 * 当交易通道断开连接时调用，先通知外部语言，然后调用基类方法
 */
void ExpHftContext::on_channel_lost()
{
	getRunner().hft_on_channel_lost(_context_id, _trader->id());  // 通知外部语言交易通道断开

	HftStraBaseCtx::on_channel_lost();  // 调用基类方法，处理通道断开的内部逻辑
}

/**
 * @brief 交易通道就绪事件处理
 * 
 * 当交易通道连接成功并准备就绪时调用，先通知外部语言，然后调用基类方法
 */
void ExpHftContext::on_channel_ready()
{
	getRunner().hft_on_channel_ready(_context_id, _trader->id());  // 通知外部语言交易通道就绪

	HftStraBaseCtx::on_channel_ready();  // 调用基类方法，处理通道就绪的内部逻辑
}

/**
 * @brief 委托回报事件处理
 * 
 * 当委托单提交后收到回报时调用，先通知外部语言，然后调用基类方法
 * 
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 返回消息
 */
void ExpHftContext::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	getRunner().hft_on_entrust(_context_id, localid, stdCode, bSuccess, message, getOrderTag(localid));  // 通知外部语言委托回报

	HftStraBaseCtx::on_entrust(localid, stdCode, bSuccess, message);  // 调用基类方法，处理委托回报的内部逻辑
}

/**
 * @brief 策略初始化事件处理
 * 
 * 当策略上下文初始化完成时调用，先调用基类的初始化方法，然后通知外部语言策略已初始化
 */
void ExpHftContext::on_init()
{
	HftStraBaseCtx::on_init();  // 调用基类的初始化方法，完成基础初始化工作

	//向外部回调
	getRunner().ctx_on_init(_context_id, ET_HFT);  // 通知外部语言策略已初始化
}

/**
 * @brief 交易日开始事件处理
 * 
 * 当新的交易日开始时调用，先调用基类方法，然后通知外部语言交易日开始
 * 
 * @param uTDate 交易日（格式：YYYYMMDD）
 */
void ExpHftContext::on_session_begin(uint32_t uTDate)
{
	HftStraBaseCtx::on_session_begin(uTDate);  // 调用基类方法，处理交易日开始的内部逻辑

	//向外部回调
	getRunner().ctx_on_session_event(_context_id, uTDate, true, ET_HFT);  // 通知外部语言交易日开始
}

/**
 * @brief 交易日结束事件处理
 * 
 * 当交易日结束时调用，先通知外部语言，然后调用基类方法
 * 
 * @param uTDate 交易日（格式：YYYYMMDD）
 */
void ExpHftContext::on_session_end(uint32_t uTDate)
{
	//向外部回调
	getRunner().ctx_on_session_event(_context_id, uTDate, false, ET_HFT);  // 通知外部语言交易日结束

	HftStraBaseCtx::on_session_end(uTDate);  // 调用基类方法，处理交易日结束的内部逻辑
}

/**
 * @brief 订单状态变化事件处理
 * 
 * 当订单状态发生变化时调用，先通知外部语言，然后调用基类方法
 * 
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy true表示买入，false表示卖出
 * @param totalQty 订单总数量
 * @param leftQty 剩余未成交数量
 * @param price 订单价格
 * @param isCanceled 是否已撤单
 */
void ExpHftContext::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled)
{
	getRunner().hft_on_order(_context_id, localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, getOrderTag(localid));  // 通知外部语言订单状态变化

	HftStraBaseCtx::on_order(localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled);  // 调用基类方法，处理订单状态变化的内部逻辑
}

/**
 * @brief 持仓变化事件处理
 * 
 * 当持仓发生变化时调用，通知外部语言持仓变化
 * 
 * @param stdCode 标准合约代码
 * @param isLong true表示多头持仓，false表示空头持仓
 * @param prevol 变化前持仓数量
 * @param preavail 变化前可用持仓数量
 * @param newvol 变化后持仓数量
 * @param newavail 变化后可用持仓数量
 * @param tradingday 交易日（格式：YYYYMMDD）
 */
void ExpHftContext::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	getRunner().hft_on_position(_context_id, stdCode, isLong, prevol, preavail, newvol, newavail);  // 通知外部语言持仓变化
}

/**
 * @brief Tick更新事件处理
 * 
 * 当订阅的合约有新的Tick数据时调用，先更新动态盈亏，然后检查是否订阅了该合约，
 * 如果是则通知外部语言，最后调用基类方法
 * 
 * @param code 合约代码
 * @param newTick 新的Tick数据指针
 */
void ExpHftContext::on_tick(const char* code, WTSTickData* newTick)
{
	update_dyn_profit(code, newTick);  // 更新动态盈亏（基于最新价格计算持仓盈亏）

	auto it = _tick_subs.find(code);  // 查找是否订阅了该合约
	if (it != _tick_subs.end())  // 如果已订阅，通知外部语言
	{
		getRunner().ctx_on_tick(_context_id, code, newTick, ET_HFT);  // 通知外部语言Tick更新
	}

	HftStraBaseCtx::on_tick(code, newTick);  // 调用基类方法，处理Tick更新的内部逻辑
}

/**
 * @brief 订单队列更新事件处理
 * 
 * 当订阅的合约有新的订单队列数据时调用，通知外部语言订单队列更新
 * 
 * @param stdCode 标准合约代码
 * @param newOrdQue 新的订单队列数据指针
 */
void ExpHftContext::on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	getRunner().hft_on_order_queue(_context_id, stdCode, newOrdQue);  // 通知外部语言订单队列更新
}

/**
 * @brief 订单明细更新事件处理
 * 
 * 当订阅的合约有新的订单明细数据时调用，通知外部语言订单明细更新
 * 
 * @param stdCode 标准合约代码
 * @param newOrdDtl 新的订单明细数据指针
 */
void ExpHftContext::on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	getRunner().hft_on_order_detail(_context_id, stdCode, newOrdDtl);  // 通知外部语言订单明细更新
}

/**
 * @brief 逐笔成交更新事件处理
 * 
 * 当订阅的合约有新的逐笔成交数据时调用，通知外部语言逐笔成交更新
 * 
 * @param stdCode 标准合约代码
 * @param newTrans 新的逐笔成交数据指针
 */
void ExpHftContext::on_transaction(const char* stdCode, WTSTransData* newTrans)
{
	getRunner().hft_on_transaction(_context_id, stdCode, newTrans);  // 通知外部语言逐笔成交更新
}

/**
 * @brief 成交回报事件处理
 * 
 * 当订单有成交回报时调用，先通知外部语言，然后调用基类方法
 * 
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy true表示买入，false表示卖出
 * @param vol 成交数量
 * @param price 成交价格
 */
void ExpHftContext::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	getRunner().hft_on_trade(_context_id, localid, stdCode, isBuy, vol, price, getOrderTag(localid));  // 通知外部语言成交回报

	HftStraBaseCtx::on_trade(localid, stdCode, isBuy, vol, price);  // 调用基类方法，处理成交回报的内部逻辑
}