/*!
 * \file ExpSelMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展SEL策略模拟器类实现文件
 * 
 * 本文件实现了ExpSelMocker类的所有方法，通过WtBtRunner将SEL策略事件转发给外部语言实现的回调函数。
 * 外部语言通过注册的回调函数接收策略事件通知，并通过C接口调用策略的查询和交易方法。
 */
#include "ExpSelMocker.h"  // ExpSelMocker类定义
#include "WtBtRunner.h"  // 回测运行器

extern WtBtRunner& getRunner();  // 获取WtBtRunner单例对象

/**
 * @brief ExpSelMocker构造函数
 * 
 * 调用基类构造函数，初始化SEL策略模拟器
 * 
 * @param replayer 历史数据回放器指针
 * @param name 策略名称
 * @param slippage 滑点设置（单位：最小变动价位）
 * @param isRatioSlp 是否使用比例滑点
 */
ExpSelMocker::ExpSelMocker(HisDataReplayer* replayer, const char* name, int32_t slippage /* = 0 */, bool isRatioSlp/* = false*/)
	: SelMocker(replayer, name, slippage, isRatioSlp)  // 调用基类构造函数
{
}

/**
 * @brief ExpSelMocker析构函数
 * 
 * 清理ExpSelMocker对象的资源
 */
ExpSelMocker::~ExpSelMocker()
{
}

/**
 * @brief 策略初始化事件处理
 * 
 * 调用基类的初始化方法，然后通知外部语言策略已初始化
 */
void ExpSelMocker::on_init()
{
	SelMocker::on_init();  // 调用基类的初始化方法

	//向外部回调
	getRunner().ctx_on_init(_context_id, ET_SEL);  // 通知外部语言策略已初始化

	getRunner().on_initialize_event();  // 通知外部语言引擎初始化事件
}

/**
 * @brief 交易日开始事件处理
 * 
 * 调用基类的交易日开始方法，然后通知外部语言交易日开始
 * 
 * @param uDate 当前交易日（格式：YYYYMMDD）
 */
void ExpSelMocker::on_session_begin(uint32_t uDate)
{
	SelMocker::on_session_begin(uDate);  // 调用基类的交易日开始方法

	getRunner().ctx_on_session_event(_context_id, uDate, true, ET_SEL);  // 通知外部语言交易日开始
	getRunner().on_session_event(uDate, true);  // 通知外部语言引擎交易日开始事件
}

/**
 * @brief 交易日结束事件处理
 * 
 * 调用基类的交易日结束方法，然后通知外部语言交易日结束
 * 
 * @param uDate 当前交易日（格式：YYYYMMDD）
 */
void ExpSelMocker::on_session_end(uint32_t uDate)
{
	SelMocker::on_session_end(uDate);  // 调用基类的交易日结束方法

	getRunner().ctx_on_session_event(_context_id, uDate, false, ET_SEL);  // 通知外部语言交易日结束
	getRunner().on_session_event(uDate, false);  // 通知外部语言引擎交易日结束事件
}

/**
 * @brief Tick更新事件处理
 * 
 * 检查合约是否已订阅，如果已订阅则通知外部语言Tick更新
 * 
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针
 */
void ExpSelMocker::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	auto it = _tick_subs.find(stdCode);  // 查找合约是否已订阅
	if (it == _tick_subs.end())  // 如果未订阅
		return;  // 直接返回

	//向外部回调
	getRunner().ctx_on_tick(_context_id, stdCode, newTick, ET_SEL);  // 通知外部语言Tick更新
}

/**
 * @brief K线闭合事件处理
 * 
 * 调用基类的K线闭合方法，然后通知外部语言K线闭合
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param newBar 新生成的K线数据结构指针
 */
void ExpSelMocker::on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar)
{
	SelMocker::on_bar_close(stdCode, period, newBar);  // 调用基类的K线闭合方法
	//要向外部回调
	getRunner().ctx_on_bar(_context_id, stdCode, period, newBar, ET_SEL);  // 通知外部语言K线闭合
}

/**
 * @brief 策略调度事件处理
 * 
 * 调用基类的策略调度方法，然后通知外部语言执行策略计算，并触发调度事件
 * 
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 */
void ExpSelMocker::on_strategy_schedule(uint32_t curDate, uint32_t curTime)
{
	SelMocker::on_strategy_schedule(curDate, curTime);  // 调用基类的策略调度方法

	//向外部回调
	getRunner().ctx_on_calc(_context_id, curDate, curTime, ET_SEL);  // 通知外部语言执行策略计算

	getRunner().on_schedule_event(curDate, curTime);  // 通知外部语言调度事件
}

/**
 * @brief 回测结束事件处理
 * 
 * 通知外部语言回测已结束
 */
void ExpSelMocker::on_bactest_end()
{
	getRunner().on_backtest_end();  // 通知外部语言回测已结束
}