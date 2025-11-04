/*!
 * \file SelStraContext.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 选股策略上下文实现文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件实现选股策略上下文类SelStraContext的所有功能。
 * 
 * 主要实现功能：
 * 1. 构造函数和析构函数的实现。
 * 2. 各种回调函数的实现，将基础上下文的回调转发给策略实例。
 * 3. Tick数据过滤，只转发已订阅的合约数据。
 */
#include "SelStraContext.h"  // 包含选股策略上下文头文件
#include "../Includes/SelStrategyDefs.h"  // 包含选股策略定义头文件


/**
 * @brief 构造函数实现
 * @param engine 选股引擎指针
 * @param name 策略名称
 * @param slippage 滑点设置（回测时使用）
 *
 * 调用基类构造函数初始化基础上下文，并将策略指针初始化为NULL。
 */
SelStraContext::SelStraContext(WtSelEngine* engine, const char* name, int32_t slippage)
	: SelStraBaseCtx(engine, name, slippage)  // 调用基类构造函数
	, _strategy(NULL)  // 初始化策略指针为NULL
{
}


/**
 * @brief 析构函数实现
 *
 * 清理解股策略上下文对象。
 */
SelStraContext::~SelStraContext()
{
}

/**
 * @brief 初始化回调实现
 *
 * 策略初始化时被调用，先调用基类的on_init完成基础初始化，
 * 然后如果策略实例存在，则调用策略实例的on_init方法。
 */
void SelStraContext::on_init()
{
	SelStraBaseCtx::on_init();  // 调用基类的初始化方法，完成基础上下文的初始化

	if (_strategy)  // 如果策略实例存在
		_strategy->on_init(this);  // 调用策略实例的初始化方法，传入当前上下文指针
}

/**
 * @brief 交易日开始回调实现
 * @param uTDate 交易日日期
 *
 * 每个交易日开始时被调用，先调用基类的on_session_begin处理基础逻辑，
 * 然后如果策略实例存在，则调用策略实例的on_session_begin方法。
 */
void SelStraContext::on_session_begin(uint32_t uTDate)
{
	SelStraBaseCtx::on_session_begin(uTDate);  // 调用基类的交易日开始方法，处理冻结持仓解冻等

	if (_strategy)  // 如果策略实例存在
		_strategy->on_session_begin(this, uTDate);  // 调用策略实例的交易日开始方法，传入当前上下文指针和交易日日期
}

/**
 * @brief 交易日结束回调实现
 * @param uTDate 交易日日期
 *
 * 每个交易日结束时被调用，先调用策略实例的on_session_end（如果存在），
 * 然后调用基类的on_session_end保存数据、记录日志等。
 */
void SelStraContext::on_session_end(uint32_t uTDate)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_session_end(this, uTDate);  // 先调用策略实例的交易日结束方法，传入当前上下文指针和交易日日期

	SelStraBaseCtx::on_session_end(uTDate);  // 调用基类的交易日结束方法，保存数据、记录日志等
}

/**
 * @brief K线收盘回调实现
 * @param stdCode 标准合约代码
 * @param period 周期
 * @param newBar 新的K线数据
 *
 * 当K线收盘时被调用，如果策略实例存在，则转发给策略实例的on_bar方法。
 */
void SelStraContext::on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_bar(this, stdCode, period, newBar);  // 调用策略实例的K线回调方法，传入当前上下文指针、合约代码、周期和K线数据
}

/**
 * @brief Tick更新回调实现
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据
 *
 * 当Tick数据更新时被调用，先检查该合约是否在订阅列表中，
 * 如果是，则转发给策略实例的on_tick方法。
 */
void SelStraContext::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	auto it = _tick_subs.find(stdCode);  // 在Tick订阅列表中查找该合约代码
	if (it == _tick_subs.end())  // 如果未找到（未订阅）
		return;  // 直接返回，不转发给策略

	if (_strategy)  // 如果策略实例存在
		_strategy->on_tick(this, stdCode, newTick);  // 调用策略实例的Tick回调方法，传入当前上下文指针、合约代码和Tick数据
}

/**
 * @brief 策略定时调度回调实现
 * @param curDate 当前日期
 * @param curTime 当前时间
 *
 * 定时调度时被调用，如果策略实例存在，则转发给策略实例的on_schedule方法。
 */
void SelStraContext::on_strategy_schedule(uint32_t curDate, uint32_t curTime)
{
	if (_strategy)  // 如果策略实例存在
		_strategy->on_schedule(this, curDate, curTime);  // 调用策略实例的定时调度方法，传入当前上下文指针、当前日期和当前时间
}
