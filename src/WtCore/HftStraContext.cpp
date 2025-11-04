/*!
 * \file HftStraContext.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 高频交易策略上下文实现文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件实现了高频交易策略上下文类HftStraContext的所有功能。
 * 该类作为策略实例和基础上下文之间的桥梁，负责将各种回调转发给策略实例。
 * 
 * 主要实现功能：
 * 1. 策略生命周期管理：初始化、交易日开始/结束的回调转发。
 * 2. 市场数据回调转发：Tick、委托队列、委托明细、逐笔成交、K线数据的回调转发。
 * 3. 交易通知回调转发：成交、订单、通道状态、委托、持仓变化的回调转发。
 * 4. 动态盈亏更新：在Tick数据回调时先更新动态盈亏。
 * 5. 订阅过滤：只转发已订阅合约的Tick数据给策略。
 * 6. 代码映射：将标准代码转换为内部代码后传递给策略和基类。
 * 7. 订单标签传递：在交易相关回调中传递订单的用户标签给策略。
 */
#include "HftStraContext.h"  // 包含高频策略上下文头文件
#include "../Includes/HftStrategyDefs.h"  // 包含高频策略定义头文件


/**
 * @brief 构造函数实现
 * @param engine 高频交易引擎指针
 * @param name 策略名称
 * @param bAgent 是否启用数据托管模式
 * @param slippage 滑点点数（用于回测）
 *
 * 初始化高频交易策略上下文对象。
 * 调用基类构造函数初始化基础上下文，并将策略实例指针初始化为NULL。
 */
HftStraContext::HftStraContext(WtHftEngine* engine, const char* name, bool bAgent, int32_t slippage)
	: HftStraBaseCtx(engine, name, bAgent, slippage)  // 调用基类构造函数初始化基础上下文
	, _strategy(NULL)  // 初始化策略实例指针为NULL
{
}


/**
 * @brief 析构函数实现
 *
 * 清理策略上下文对象。
 * 由于策略实例由外部管理，这里不需要显式删除。
 */
HftStraContext::~HftStraContext()
{
}

/**
 * @brief 策略初始化回调实现
 *
 * 当策略初始化时被调用。
 * 先调用基类的on_init方法完成基础上下文的初始化（如初始化输出文件、加载用户数据等），
 * 然后如果策略实例存在，则调用策略的on_init方法，将上下文指针传递给策略。
 */
void HftStraContext::on_init()
{
	HftStraBaseCtx::on_init();  // 调用基类的初始化方法，完成基础上下文的初始化

	if (_strategy)  // 如果策略实例存在
		_strategy->on_init(this);  // 调用策略的初始化方法，传递上下文指针
}

/**
 * @brief 交易日开始回调实现
 * @param uTDate 交易日期
 *
 * 当交易日开始时被调用。
 * 先调用基类的on_session_begin方法，然后如果策略实例存在，则调用策略的on_session_begin方法。
 */
void HftStraContext::on_session_begin(uint32_t uTDate)
{
	HftStraBaseCtx::on_session_begin(uTDate);  // 调用基类的交易日开始方法

	if (_strategy)  // 如果策略实例存在
		_strategy->on_session_begin(this, uTDate);  // 调用策略的交易日开始方法，传递上下文指针和交易日期
}

/**
 * @brief 交易日结束回调实现
 * @param uTDate 交易日期
 *
 * 当交易日结束时被调用。
 * 先调用策略的on_session_end方法（如果策略实例存在），让策略先处理交易日结束逻辑，
 * 然后调用基类的on_session_end方法，完成基础上下文的结算工作（如记录资金日志等）。
 */
void HftStraContext::on_session_end(uint32_t uTDate)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_session_end(this, uTDate);  // 调用策略的交易日结束方法，传递上下文指针和交易日期

	HftStraBaseCtx::on_session_end(uTDate);  // 调用基类的交易日结束方法，完成基础上下文的结算工作
}

/**
 * @brief Tick数据回调实现
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针
 *
 * 当收到新的Tick数据时被调用。
 * 1. 先调用update_dyn_profit更新该合约的持仓动态盈亏。
 * 2. 检查策略是否订阅了该合约的Tick数据（通过_tick_subs集合判断）。
 * 3. 如果已订阅且策略实例存在，则调用策略的on_tick方法。
 * 4. 最后调用基类的on_tick方法，完成基础上下文的处理（如保存用户数据等）。
 */
void HftStraContext::on_tick(const char* stdCode, WTSTickData* newTick)
{
	update_dyn_profit(stdCode, newTick);  // 更新该合约的持仓动态盈亏

	auto it = _tick_subs.find(stdCode);  // 在Tick订阅集合中查找该合约
	if (it != _tick_subs.end())  // 如果策略已订阅该合约的Tick数据
	{
		if (_strategy)  // 如果策略实例存在
			_strategy->on_tick(this, stdCode, newTick);  // 调用策略的Tick数据回调方法，传递上下文指针、合约代码和Tick数据
	}

	HftStraBaseCtx::on_tick(stdCode, newTick);  // 调用基类的Tick数据回调方法，完成基础上下文的处理
}

/**
 * @brief 委托队列数据回调实现
 * @param stdCode 标准合约代码
 * @param newOrdQue 新的委托队列数据指针
 *
 * 当收到新的委托队列数据时被调用。
 * 先调用策略的on_order_queue方法（如果策略实例存在），然后调用基类的on_order_queue方法。
 */
void HftStraContext::on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_order_queue(this, stdCode, newOrdQue);  // 调用策略的委托队列数据回调方法，传递上下文指针、合约代码和委托队列数据

	HftStraBaseCtx::on_order_queue(stdCode, newOrdQue);  // 调用基类的委托队列数据回调方法，完成基础上下文的处理
}

/**
 * @brief 委托明细数据回调实现
 * @param stdCode 标准合约代码
 * @param newOrdDtl 新的委托明细数据指针
 *
 * 当收到新的委托明细数据时被调用。
 * 先调用策略的on_order_detail方法（如果策略实例存在），然后调用基类的on_order_detail方法。
 */
void HftStraContext::on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_order_detail(this, stdCode, newOrdDtl);  // 调用策略的委托明细数据回调方法，传递上下文指针、合约代码和委托明细数据

	HftStraBaseCtx::on_order_detail(stdCode, newOrdDtl);  // 调用基类的委托明细数据回调方法，完成基础上下文的处理
}

/**
 * @brief 逐笔成交数据回调实现
 * @param stdCode 标准合约代码
 * @param newTrans 新的逐笔成交数据指针
 *
 * 当收到新的逐笔成交数据时被调用。
 * 先调用策略的on_transaction方法（如果策略实例存在），然后调用基类的on_transaction方法。
 */
void HftStraContext::on_transaction(const char* stdCode, WTSTransData* newTrans)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_transaction(this, stdCode, newTrans);  // 调用策略的逐笔成交数据回调方法，传递上下文指针、合约代码和逐笔成交数据

	HftStraBaseCtx::on_transaction(stdCode, newTrans);  // 调用基类的逐笔成交数据回调方法，完成基础上下文的处理
}

/**
 * @brief K线数据回调实现
 * @param code 标准合约代码
 * @param period 周期（如"m1"、"m5"等）
 * @param times 周期倍数
 * @param newBar 新的K线数据指针
 *
 * 当收到新的K线数据时被调用。
 * 先调用策略的on_bar方法（如果策略实例存在），然后调用基类的on_bar方法。
 */
void HftStraContext::on_bar(const char* code, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_bar(this, code, period, times, newBar);  // 调用策略的K线数据回调方法，传递上下文指针、合约代码、周期、倍数和K线数据

	HftStraBaseCtx::on_bar(code, period, times, newBar);  // 调用基类的K线数据回调方法，完成基础上下文的处理
}

/**
 * @brief 成交通知回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入方向
 * @param vol 成交数量
 * @param price 成交价格
 *
 * 当订单成交时被调用。
 * 1. 将标准代码转换为内部代码（通过get_inner_code方法，处理代码映射）。
 * 2. 如果策略实例存在，调用策略的on_trade方法，并传递订单标签（通过getOrderTag方法获取）。
 * 3. 调用基类的on_trade方法，完成基础上下文的处理（如更新持仓、记录日志等）。
 */
void HftStraContext::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	const char* innerCode = get_inner_code(stdCode);  // 将标准代码转换为内部代码（处理代码映射）
	if (_strategy)  // 如果策略实例存在
		_strategy->on_trade(this, localid, innerCode, isBuy, vol, price, getOrderTag(localid));  // 调用策略的成交通知回调方法，传递上下文指针、订单ID、内部代码、方向、数量、价格和订单标签

	HftStraBaseCtx::on_trade(localid, innerCode, isBuy, vol, price);  // 调用基类的成交通知回调方法，完成基础上下文的处理（使用内部代码）
}

/**
 * @brief 订单状态通知回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入方向
 * @param totalQty 订单总数量
 * @param leftQty 剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤销，默认false
 *
 * 当订单状态发生变化时被调用。
 * 1. 将标准代码转换为内部代码（通过get_inner_code方法，处理代码映射）。
 * 2. 如果策略实例存在，调用策略的on_order方法，并传递订单标签（通过getOrderTag方法获取）。
 * 3. 调用基类的on_order方法，完成基础上下文的处理（如保存用户数据等）。
 */
void HftStraContext::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled /* = false */)
{
	const char* innerCode = get_inner_code(stdCode);  // 将标准代码转换为内部代码（处理代码映射）
	if (_strategy)  // 如果策略实例存在
		_strategy->on_order(this, localid, innerCode, isBuy, totalQty, leftQty, price, isCanceled, getOrderTag(localid));  // 调用策略的订单状态通知回调方法，传递上下文指针、订单ID、内部代码、方向、总数量、剩余数量、价格、撤销标志和订单标签

	HftStraBaseCtx::on_order(localid, innerCode, isBuy, totalQty, leftQty, price, isCanceled);  // 调用基类的订单状态通知回调方法，完成基础上下文的处理（使用内部代码）
}

/**
 * @brief 持仓变化通知回调实现
 * @param stdCode 标准合约代码
 * @param isLong 是否为做多方向
 * @param prevol 变化前持仓数量
 * @param preavail 变化前可用持仓数量
 * @param newvol 变化后持仓数量
 * @param newavail 变化后可用持仓数量
 * @param tradingday 交易日
 *
 * 当持仓发生变化时被调用。
 * 如果策略实例存在，则调用策略的on_position方法。
 * 注意：此方法不调用基类的on_position方法，因为持仓变化由策略自己处理。
 */
void HftStraContext::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_position(this, stdCode, isLong, prevol, preavail, newvol, newavail);  // 调用策略的持仓变化通知回调方法，传递上下文指针、合约代码、方向、变化前后持仓和可用持仓
}

/**
 * @brief 交易通道就绪通知回调实现
 *
 * 当交易通道就绪时被调用。
 * 先调用策略的on_channel_ready方法（如果策略实例存在），然后调用基类的on_channel_ready方法。
 */
void HftStraContext::on_channel_ready()
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_channel_ready(this);  // 调用策略的交易通道就绪通知回调方法，传递上下文指针

	HftStraBaseCtx::on_channel_ready();  // 调用基类的交易通道就绪通知回调方法，完成基础上下文的处理
}

/**
 * @brief 交易通道丢失通知回调实现
 *
 * 当交易通道丢失时被调用。
 * 先调用策略的on_channel_lost方法（如果策略实例存在），然后调用基类的on_channel_lost方法。
 */
void HftStraContext::on_channel_lost()
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_channel_lost(this);  // 调用策略的交易通道丢失通知回调方法，传递上下文指针

	HftStraBaseCtx::on_channel_lost();  // 调用基类的交易通道丢失通知回调方法，完成基础上下文的处理
}

/**
 * @brief 委托通知回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 消息内容
 *
 * 当委托结果返回时被调用。
 * 1. 如果策略实例存在，调用策略的on_entrust方法，并传递订单标签（通过getOrderTag方法获取）。
 * 2. 调用基类的on_entrust方法，并将标准代码转换为内部代码。
 */
void HftStraContext::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_entrust(localid, bSuccess, message, getOrderTag(localid));  // 调用策略的委托通知回调方法，传递订单ID、成功标志、消息内容和订单标签

	HftStraBaseCtx::on_entrust(localid, get_inner_code(stdCode), bSuccess, message);  // 调用基类的委托通知回调方法，完成基础上下文的处理（使用内部代码）
}