/*!
 * \file ExpCtaMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief ExpCtaMocker CTA策略扩展模拟器类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了ExpCtaMocker类的所有成员函数，该类继承自CtaMocker基类，
 * 主要功能是将回测引擎内部的事件转发给外部语言注册的回调函数。
 * 
 * 实现要点：
 * 1. 所有事件处理函数都遵循"先调用基类，再通知外部"的模式（除了on_session_end）
 * 2. 通过getRunner()获取WtBtRunner单例对象，统一管理回调函数调用
 * 3. 使用_context_id标识策略实例，确保回调函数能正确识别策略
 * 4. 在on_tick_updated中检查订阅状态，只处理已订阅的合约
 * 
 * 事件转发流程：
 * - 策略初始化：on_init() -> ctx_on_init() -> on_initialize_event()
 * - 交易日事件：on_session_begin/end() -> ctx_on_session_event() -> on_session_event()
 * - Tick更新：on_tick_updated() -> ctx_on_tick()
 * - K线闭合：on_bar_close() -> ctx_on_bar()
 * - 策略计算：on_calculate() -> ctx_on_calc()
 * - 计算完成：on_calculate_done() -> ctx_on_calc_done() -> on_schedule_event()
 * - 条件单触发：on_condition_triggered() -> ctx_on_cond_triggered()
 * - 回测结束：on_bactest_end() -> on_backtest_end()
 */
#include "ExpCtaMocker.h"  // ExpCtaMocker类定义
#include "WtBtRunner.h"  // WtBtRunner类定义

/**
 * @brief 获取WtBtRunner单例对象的外部声明
 * 
 * 声明getRunner()函数，用于获取WtBtRunner单例对象引用
 * 该函数在WtBtPorter.cpp中定义
 */
extern WtBtRunner& getRunner();

/**
 * @brief ExpCtaMocker构造函数实现
 * 
 * 创建CTA策略扩展模拟器实例，调用基类CtaMocker的构造函数初始化回测环境
 * 
 * @param replayer 历史数据回放器指针，用于回放历史数据驱动策略执行
 * @param name 策略名称，用于标识策略实例
 * @param slippage 滑点设置（单位：最小变动价位），0表示不设置滑点
 * @param persistData 是否持久化策略数据，true表示持久化（程序重启后可恢复），false表示不持久化
 * @param notifier 事件通知器指针，用于发送事件通知，NULL表示不使用事件通知器
 * @param isRatioSlp 是否使用比例滑点，true表示比例滑点（滑点=价格*比例），false表示固定滑点
 */
ExpCtaMocker::ExpCtaMocker(HisDataReplayer* replayer, const char* name, int32_t slippage /* = 0 */, bool persistData /* = true */, EventNotifier* notifier /* = NULL */, bool isRatioSlp /* = false */)
	: CtaMocker(replayer, name, slippage, persistData, notifier, isRatioSlp)  // 调用基类构造函数，初始化回测环境
{
	// 构造函数体为空，所有初始化工作都在基类构造函数中完成
}

/**
 * @brief ExpCtaMocker析构函数实现
 * 
 * 清理资源，释放占用的内存
 * 基类CtaMocker的析构函数会自动清理基类资源
 */
ExpCtaMocker::~ExpCtaMocker()
{
	// 析构函数体为空，所有清理工作都在基类析构函数中完成
}

/**
 * @brief 策略初始化事件处理函数实现
 * 
 * 当策略初始化完成时调用此函数，执行以下操作：
 * 1. 调用基类CtaMocker::on_init()，执行基类的初始化逻辑
 * 2. 通过WtBtRunner通知外部语言策略已初始化完成（ctx_on_init）
 * 3. 触发引擎初始化事件（on_initialize_event），通知外部语言引擎已初始化
 */
void ExpCtaMocker::on_init()
{
	CtaMocker::on_init();  // 调用基类的初始化逻辑，完成策略的基础初始化工作

	// 向外部回调：通知外部语言策略已初始化完成
	// _context_id是策略上下文ID，用于标识策略实例；ET_CTA表示CTA引擎类型
	getRunner().ctx_on_init(_context_id, ET_CTA);

	// 触发引擎初始化事件，通知外部语言回测引擎已初始化完成
	getRunner().on_initialize_event();
}

/**
 * @brief 交易日开始事件处理函数实现
 * 
 * 当新的交易日开始时调用此函数，执行以下操作：
 * 1. 调用基类CtaMocker::on_session_begin()，执行基类的交易日开始逻辑
 * 2. 通过WtBtRunner通知外部语言策略的交易日已开始（ctx_on_session_event）
 * 3. 触发引擎交易日开始事件（on_session_event），通知外部语言引擎交易日已开始
 * 
 * @param uCurDate 当前交易日（格式：YYYYMMDD，如20230330表示2023年3月30日）
 */
void ExpCtaMocker::on_session_begin(uint32_t uCurDate)
{
	CtaMocker::on_session_begin(uCurDate);  // 调用基类的交易日开始逻辑，完成策略的交易日初始化工作

	// 通知外部语言策略的交易日已开始
	// 参数：策略上下文ID、当前交易日、true表示交易日开始、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_session_event(_context_id, uCurDate, true, ET_CTA);
	
	// 触发引擎交易日开始事件，通知外部语言回测引擎的交易日已开始
	getRunner().on_session_event(uCurDate, true);
}

/**
 * @brief 交易日结束事件处理函数实现
 * 
 * 当交易日结束时调用此函数，执行以下操作：
 * 1. 先通知外部语言策略的交易日已结束（ctx_on_session_event）
 * 2. 触发引擎交易日结束事件（on_session_event），通知外部语言引擎交易日已结束
 * 3. 最后调用基类CtaMocker::on_session_end()，执行基类的交易日结束逻辑
 * 
 * 注意：这里先通知外部语言，再调用基类，是为了确保外部语言可以在基类清理资源之前进行必要的处理。
 * 
 * @param uCurDate 当前交易日（格式：YYYYMMDD）
 */
void ExpCtaMocker::on_session_end(uint32_t uCurDate)
{
	// 先通知外部语言策略的交易日已结束
	// 参数：策略上下文ID、当前交易日、false表示交易日结束、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_session_event(_context_id, uCurDate, false, ET_CTA);
	
	// 触发引擎交易日结束事件，通知外部语言回测引擎的交易日已结束
	getRunner().on_session_event(uCurDate, false);

	// 最后调用基类的交易日结束逻辑，完成策略的交易日清理工作
	CtaMocker::on_session_end(uCurDate);
}

/**
 * @brief Tick数据更新事件处理函数实现
 * 
 * 当订阅的合约有新的Tick数据时调用此函数，执行以下操作：
 * 1. 检查是否订阅了该合约（通过_tick_subs查找），如果未订阅则直接返回
 * 2. 调用基类CtaMocker::on_tick_updated()，执行基类的Tick更新逻辑
 * 3. 通过WtBtRunner通知外部语言有新的Tick数据（ctx_on_tick）
 * 
 * @param stdCode 标准合约代码（如"SHFE.rb2305"）
 * @param newTick 新的Tick数据指针，包含最新价格、成交量、持仓量等信息
 */
void ExpCtaMocker::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	// 检查是否订阅了该合约的Tick数据
	// _tick_subs是基类CtaMocker的成员变量，存储已订阅的合约集合
	auto it = _tick_subs.find(stdCode);
	if (it == _tick_subs.end())  // 如果未订阅该合约，直接返回，不处理
		return;

	// 调用基类的Tick更新逻辑，更新策略内部状态
	CtaMocker::on_tick_updated(stdCode, newTick);
	
	// 通知外部语言有新的Tick数据
	// 参数：策略上下文ID、标准合约代码、新的Tick数据指针、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_tick(_context_id, stdCode, newTick, ET_CTA);
}

/**
 * @brief K线闭合事件处理函数实现
 * 
 * 当订阅的K线周期完成并生成新的K线时调用此函数，执行以下操作：
 * 1. 调用基类CtaMocker::on_bar_close()，执行基类的K线闭合逻辑
 * 2. 通过WtBtRunner通知外部语言有新的K线闭合事件（ctx_on_bar）
 * 
 * @param code 标准合约代码（如"SHFE.rb2305"）
 * @param period K线周期（如"m1"表示1分钟K线，"d1"表示日线）
 * @param newBar 新生成的K线数据指针，包含开盘价、最高价、最低价、收盘价、成交量等信息
 */
void ExpCtaMocker::on_bar_close(const char* code, const char* period, WTSBarStruct* newBar)
{
	// 调用基类的K线闭合逻辑，更新策略内部状态，触发策略计算
	CtaMocker::on_bar_close(code, period, newBar);

	// 要向外部回调：通知外部语言有新的K线闭合事件
	// 参数：策略上下文ID、标准合约代码、K线周期、新生成的K线数据指针、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_bar(_context_id, code, period, newBar, ET_CTA);
}

/**
 * @brief 策略计算事件处理函数实现
 * 
 * 当策略需要执行计算逻辑时调用此函数（通常在K线闭合或定时触发时），执行以下操作：
 * 1. 调用基类CtaMocker::on_calculate()，执行基类的计算逻辑
 * 2. 通过WtBtRunner通知外部语言策略需要计算（ctx_on_calc）
 * 
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS，如143000表示14:30:00）
 */
void ExpCtaMocker::on_calculate(uint32_t curDate, uint32_t curTime)
{
	// 调用基类的计算逻辑，执行策略的计算函数
	CtaMocker::on_calculate(curDate, curTime);
	
	// 通知外部语言策略需要计算
	// 参数：策略上下文ID、当前日期、当前时间、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_calc(_context_id, curDate, curTime, ET_CTA);
}

/**
 * @brief 策略计算完成事件处理函数实现
 * 
 * 当策略计算逻辑执行完毕后调用此函数，执行以下操作：
 * 1. 调用基类CtaMocker::on_calculate_done()，执行基类的计算完成逻辑
 * 2. 通过WtBtRunner通知外部语言策略计算已完成（ctx_on_calc_done）
 * 3. 触发引擎调度事件（on_schedule_event），通知外部语言引擎已完成一次调度
 * 
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 */
void ExpCtaMocker::on_calculate_done(uint32_t curDate, uint32_t curTime)
{
	// 调用基类的计算完成逻辑，完成策略计算后的清理工作
	CtaMocker::on_calculate_done(curDate, curTime);
	
	// 通知外部语言策略计算已完成
	// 参数：策略上下文ID、当前日期、当前时间、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_calc_done(_context_id, curDate, curTime, ET_CTA);

	// 触发引擎调度事件，通知外部语言回测引擎已完成一次调度
	getRunner().on_schedule_event(curDate, curTime);
}

/**
 * @brief 回测结束事件处理函数实现
 * 
 * 当回测完成时调用此函数，通过WtBtRunner通知外部语言回测已结束。
 * 
 * 注意：此函数不调用基类实现，因为基类可能没有此函数或实现不同。
 */
void ExpCtaMocker::on_bactest_end()
{
	// 触发回测结束事件，通知外部语言回测已完成
	getRunner().on_backtest_end();
}

/**
 * @brief 条件单触发事件处理函数实现
 * 
 * 当策略设置的条件单被触发时调用此函数，通过WtBtRunner通知外部语言条件单已触发。
 * 
 * @param stdCode 标准合约代码
 * @param target 目标持仓数量（正数表示多头，负数表示空头）
 * @param price 触发价格（条件单触发时的价格）
 * @param usertag 用户标签（用于标识该条件单，由策略在设置条件单时指定）
 */
void ExpCtaMocker::on_condition_triggered(const char* stdCode, double target, double price, const char* usertag)
{
	// 通知外部语言条件单已触发
	// 参数：策略上下文ID、标准合约代码、目标持仓数量、触发价格、用户标签、ET_CTA表示CTA引擎类型
	getRunner().ctx_on_cond_triggered(_context_id, stdCode, target, price, usertag, ET_CTA);
}