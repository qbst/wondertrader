/*!
 * \file ExpCtaContext.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展CTA策略上下文类实现文件
 * 
 * 本文件实现了ExpCtaContext类的所有方法，通过WtRtRunner将策略事件转发给外部语言实现的回调函数。
 * 每个事件处理方法都会先调用基类的对应方法，然后通过WtRtRunner通知外部语言。
 */
#include "ExpCtaContext.h"  // ExpCtaContext类定义
#include "WtRtRunner.h"  // 运行时运行器

#include "../Share/StrUtil.hpp"  // 字符串工具函数

extern WtRtRunner& getRunner();  // 获取WtRtRunner单例对象

/**
 * @brief 构造函数
 * 
 * 调用基类构造函数初始化CTA策略上下文
 * 
 * @param env CTA引擎指针
 * @param name 策略名称
 * @param slippage 滑点设置
 */
ExpCtaContext::ExpCtaContext(WtCtaEngine* env, const char* name, int32_t slippage)
	: CtaStraBaseCtx(env, name, slippage)  // 调用基类构造函数
{
}

/**
 * @brief 析构函数
 */
ExpCtaContext::~ExpCtaContext()
{
}

/**
 * @brief 策略初始化事件处理
 * 
 * 当策略上下文初始化完成时调用，先调用基类的初始化方法，然后通知外部语言策略已初始化
 */
void ExpCtaContext::on_init()
{
	CtaStraBaseCtx::on_init();  // 调用基类的初始化方法，完成基础初始化工作

	//向外部回调
	getRunner().ctx_on_init(_context_id, ET_CTA);  // 通知外部语言策略已初始化

	dump_chart_info();  // 输出图表信息（用于调试）
}

/**
 * @brief 交易日开始事件处理
 * 
 * 当新的交易日开始时调用，先调用基类方法，然后通知外部语言交易日开始
 * 
 * @param uDate 交易日（格式：YYYYMMDD）
 */
void ExpCtaContext::on_session_begin(uint32_t uDate)
{
	CtaStraBaseCtx::on_session_begin(uDate);  // 调用基类方法，处理交易日开始的内部逻辑

	//向外部回调
	getRunner().ctx_on_session_event(_context_id, uDate, true, ET_CTA);  // 通知外部语言交易日开始
}

/**
 * @brief 交易日结束事件处理
 * 
 * 当交易日结束时调用，先通知外部语言，然后调用基类方法
 * 
 * @param uDate 交易日（格式：YYYYMMDD）
 */
void ExpCtaContext::on_session_end(uint32_t uDate)
{
	//向外部回调
	getRunner().ctx_on_session_event(_context_id, uDate, false, ET_CTA);  // 通知外部语言交易日结束

	CtaStraBaseCtx::on_session_end(uDate);  // 调用基类方法，处理交易日结束的内部逻辑
}

/**
 * @brief Tick更新事件处理
 * 
 * 当订阅的合约有新的Tick数据时调用，检查是否订阅了该合约，如果是则通知外部语言
 * 
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针
 */
void ExpCtaContext::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	auto it = _tick_subs.find(stdCode);  // 查找是否订阅了该合约
	if (it == _tick_subs.end())  // 如果未订阅，直接返回
		return;

	getRunner().ctx_on_tick(_context_id, stdCode, newTick, ET_CTA);  // 通知外部语言Tick更新
}

/**
 * @brief K线闭合事件处理
 * 
 * 当订阅的K线周期完成并生成新的K线时调用，通知外部语言K线闭合
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param newBar 新生成的K线数据结构指针
 */
void ExpCtaContext::on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar)
{
	//要向外部回调
	getRunner().ctx_on_bar(_context_id, stdCode, period, newBar, ET_CTA);  // 通知外部语言K线闭合
}

/**
 * @brief 策略计算事件处理
 * 
 * 当引擎执行定时计算时调用，通知外部语言执行策略计算
 * 
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 */
void ExpCtaContext::on_calculate(uint32_t curDate, uint32_t curTime)
{
	getRunner().ctx_on_calc(_context_id, curDate, curTime, ET_CTA);  // 通知外部语言执行策略计算
}

/**
 * @brief 条件单触发事件处理
 * 
 * 当设置的条件单被触发时调用，通知外部语言条件单已触发
 * 
 * @param stdCode 标准合约代码
 * @param target 目标价格
 * @param price 触发价格
 * @param usertag 用户标签
 */
void ExpCtaContext::on_condition_triggered(const char* stdCode, double target, double price, const char* usertag)
{
	getRunner().ctx_on_cond_triggered(_context_id, stdCode, target, price, usertag, ET_CTA);  // 通知外部语言条件单已触发
}
