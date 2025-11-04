/*!
 * \file CtaStraContext.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略上下文实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了CTA策略上下文类的核心功能，主要是将引擎的各种回调事件转发给策略对象。
 * 主要实现逻辑：
 * 1. 构造函数和析构函数：初始化上下文对象，调用基类构造函数
 * 2. 生命周期回调转发：将引擎的生命周期事件（初始化、交易日开始/结束）转发给策略对象
 * 3. 数据更新回调转发：将市场数据更新事件（Tick更新、K线闭合）转发给策略对象
 * 4. 策略计算回调转发：将定时调度事件转发给策略对象执行策略逻辑
 * 5. 条件单触发回调转发：将条件单触发事件转发给策略对象
 * 
 * 该类作为策略对象和引擎之间的桥梁，负责将引擎的各种事件转发给策略对象处理。
 * 通过转发机制，实现了策略对象与引擎的解耦，策略对象只需要关注业务逻辑。
 */
#include "CtaStraContext.h"  // 包含CTA策略上下文头文件
#include "WtCtaEngine.h"      // 包含CTA引擎头文件
#include "../Includes/CtaStrategyDefs.h"  // 包含CTA策略定义

#include <exception>  // 包含异常处理支持

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息定义

/**
 * @brief 构造函数实现
 * @param engine CTA引擎指针
 * @param name 策略上下文名称
 * @param slippage 滑点设置
 * 
 * 初始化CTA策略上下文对象，调用基类构造函数完成基础初始化。
 * 此时策略对象指针还未设置，需要在后续通过set_strategy设置。
 */
CtaStraContext::CtaStraContext(WtCtaEngine* engine, const char* name, int32_t slippage)
	: CtaStraBaseCtx(engine, name, slippage)  // 调用基类构造函数，初始化基础上下文
{
}


/**
 * @brief 析构函数实现
 * 
 * 清理CTA策略上下文对象，释放相关资源。
 * 由于策略对象指针由外部管理，这里不需要释放策略对象。
 */
CtaStraContext::~CtaStraContext()
{
}

//////////////////////////////////////////////////////////////////////////
//回调函数实现（将引擎事件转发给策略对象）
//////////////////////////////////////////////////////////////////////////

/**
 * @brief K线闭合回调实现
 * @param code 合约代码
 * @param period K线周期
 * @param newBar 闭合的K线数据指针
 * 
 * 当订阅的K线闭合时，将事件转发给策略对象的on_bar方法处理。
 */
void CtaStraContext::on_bar_close(const char* code, const char* period, WTSBarStruct* newBar)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_bar(this, code, period, newBar);  // 调用策略对象的on_bar方法处理K线闭合事件
}

/**
 * @brief 策略初始化回调实现
 * 
 * 策略初始化时调用，执行顺序：
 * 1. 先调用基类的on_init完成基础初始化（如初始化输出文件、加载历史数据等）
 * 2. 然后调用策略对象的on_init进行策略特定的初始化
 * 3. 最后导出图表配置信息
 */
void CtaStraContext::on_init()
{
	CtaStraBaseCtx::on_init();  // 调用基类的on_init，完成基础初始化（初始化输出文件、加载历史数据等）

	if (_strategy)  // 如果策略对象存在
		_strategy->on_init(this);  // 调用策略对象的on_init，进行策略特定的初始化

	dump_chart_info();  // 导出图表配置信息到JSON文件
}

/**
 * @brief 交易日开始回调实现
 * @param uTDate 交易日日期，格式为YYYYMMDD
 * 
 * 每个交易日开始时调用，执行顺序：
 * 1. 先调用基类的on_session_begin完成基础处理（如解冻持仓等）
 * 2. 然后调用策略对象的on_session_begin进行策略特定的处理
 */
void CtaStraContext::on_session_begin(uint32_t uTDate)
{
	CtaStraBaseCtx::on_session_begin(uTDate);  // 调用基类的on_session_begin，完成基础处理（如解冻持仓等）

	if (_strategy)  // 如果策略对象存在
		_strategy->on_session_begin(this, uTDate);  // 调用策略对象的on_session_begin，进行策略特定的处理
}

/**
 * @brief 交易日结束回调实现
 * @param uTDate 交易日日期，格式为YYYYMMDD
 * 
 * 每个交易日结束时调用，执行顺序：
 * 1. 先调用策略对象的on_session_end进行策略特定的处理
 * 2. 然后调用基类的on_session_end完成基础处理（如保存数据、记录结算信息等）
 * 
 * 注意：这里先调用策略对象的回调，再调用基类的回调，确保策略可以在数据保存前进行最后的处理。
 */
void CtaStraContext::on_session_end(uint32_t uTDate)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_session_end(this, uTDate);  // 调用策略对象的on_session_end，进行策略特定的处理

	CtaStraBaseCtx::on_session_end(uTDate);  // 调用基类的on_session_end，完成基础处理（保存数据、记录结算信息等）
}

/**
 * @brief Tick数据更新回调实现
 * @param code 合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 当收到订阅合约的Tick数据更新时调用。
 * 只有订阅的合约才会触发此回调，然后转发给策略对象的on_tick处理。
 */
void CtaStraContext::on_tick_updated(const char* code, WTSTickData* newTick)
{
	auto it = _tick_subs.find(code);  // 在Tick订阅集合中查找该合约
	if (it == _tick_subs.end())       // 如果未订阅该合约
		return;                       // 直接返回，不处理

	if (_strategy)  // 如果策略对象存在
		_strategy->on_tick(this, code, newTick);  // 调用策略对象的on_tick方法处理Tick更新事件
}

/**
 * @brief 策略计算回调实现
 * @param curDate 当前日期，格式为YYYYMMDD
 * @param curTime 当前时间，格式为HHMMSS
 * 
 * 定时调度时调用，将事件转发给策略对象的on_schedule执行策略逻辑。
 * 这是策略执行的核心入口，策略的主要交易逻辑在这里实现。
 */
void CtaStraContext::on_calculate(uint32_t curDate, uint32_t curTime)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_schedule(this, curDate, curTime);  // 调用策略对象的on_schedule方法执行策略逻辑
}

/**
 * @brief 条件单触发回调实现
 * @param stdCode 合约代码
 * @param target 目标仓位
 * @param price 触发价格
 * @param usertag 用户标签
 * 
 * 当条件单触发时调用，将事件转发给策略对象的on_condition_triggered处理。
 * 策略可以在回调中进行额外的处理，如记录日志、更新状态等。
 */
void CtaStraContext::on_condition_triggered(const char* stdCode, double target, double price, const char* usertag)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_condition_triggered(this, stdCode, target, price, usertag);  // 调用策略对象的on_condition_triggered方法处理条件单触发事件
}

