/*!
 * \file ExpCtaMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief ExpCtaMocker CTA策略扩展模拟器类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了ExpCtaMocker类，这是CTA策略模拟器的扩展实现类，继承自CtaMocker基类。
 * ExpCtaMocker的主要作用是将回测引擎内部的事件（如初始化、交易日开始/结束、Tick更新、
 * K线闭合、策略计算等）转发给外部语言（如Python、C#等）注册的回调函数。
 * 
 * 设计目标：
 * 1. 作为回测引擎与外部语言之间的桥梁，实现事件转发机制
 * 2. 重写CtaMocker的虚函数，在调用基类实现后，额外调用外部回调函数
 * 3. 支持多种策略事件的通知，包括初始化、交易日事件、Tick更新、K线闭合、策略计算等
 * 4. 通过WtBtRunner单例对象统一管理回调函数的调用
 * 
 * 核心功能：
 * - 策略初始化事件转发：当策略初始化完成时，通知外部语言
 * - 交易日事件转发：当交易日开始或结束时，通知外部语言
 * - Tick数据更新转发：当订阅的合约有新的Tick数据时，通知外部语言
 * - K线闭合事件转发：当订阅的K线周期完成并生成新K线时，通知外部语言
 * - 策略计算事件转发：当策略需要执行计算逻辑时，通知外部语言
 * - 条件单触发事件转发：当条件单被触发时，通知外部语言
 * - 回测结束事件转发：当回测完成时，通知外部语言
 * 
 * 架构特点：
 * - 采用装饰器模式，在基类功能基础上增加事件转发功能
 * - 通过WtBtRunner统一管理所有回调函数，避免直接依赖外部语言接口
 * - 使用override关键字确保正确重写基类虚函数
 */
#pragma once
#include "../WtBtCore/CtaMocker.h"  // CTA策略模拟器基类定义

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief ExpCtaMocker CTA策略扩展模拟器类
 * 
 * 继承自CtaMocker基类，重写所有事件回调函数，在调用基类实现后，
 * 额外调用WtBtRunner中注册的外部回调函数，实现事件转发功能。
 * 
 * 该类是回测引擎与外部语言之间的桥梁，负责将回测过程中的各种事件
 * 转发给外部语言注册的回调函数，使得外部语言可以监听和响应回测事件。
 */
class ExpCtaMocker : public CtaMocker
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建CTA策略扩展模拟器实例，初始化回测环境
	 * 
	 * @param replayer 历史数据回放器指针，用于回放历史数据驱动策略执行
	 * @param name 策略名称，用于标识策略实例
	 * @param slippage 滑点设置（单位：最小变动价位），0表示不设置滑点（默认0）
	 * @param persistData 是否持久化策略数据，true表示持久化（程序重启后可恢复），false表示不持久化（默认true）
	 * @param notifier 事件通知器指针，用于发送事件通知，NULL表示不使用事件通知器（默认NULL）
	 * @param isRatioSlp 是否使用比例滑点，true表示比例滑点（滑点=价格*比例），false表示固定滑点（默认false）
	 */
	ExpCtaMocker(HisDataReplayer* replayer, const char* name, int32_t slippage = 0, bool persistData = true, EventNotifier* notifier = NULL, bool isRatioSlp = false);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存
	 */
	virtual ~ExpCtaMocker();

public:
	/**
	 * @brief 策略初始化事件处理函数
	 * 
	 * 当策略初始化完成时调用此函数，先调用基类的初始化逻辑，
	 * 然后通知外部语言策略已初始化完成。
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_init() override;

	/**
	 * @brief 交易日开始事件处理函数
	 * 
	 * 当新的交易日开始时调用此函数，先调用基类的交易日开始逻辑，
	 * 然后通知外部语言交易日已开始。
	 * 
	 * @param uCurDate 当前交易日（格式：YYYYMMDD）
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_session_begin(uint32_t uCurDate) override;

	/**
	 * @brief 交易日结束事件处理函数
	 * 
	 * 当交易日结束时调用此函数，先通知外部语言交易日已结束，
	 * 然后调用基类的交易日结束逻辑。
	 * 
	 * 注意：这里先通知外部语言，再调用基类，是为了确保外部语言
	 * 可以在基类清理资源之前进行必要的处理。
	 * 
	 * @param uCurDate 当前交易日（格式：YYYYMMDD）
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_session_end(uint32_t uCurDate) override;

	/**
	 * @brief Tick数据更新事件处理函数
	 * 
	 * 当订阅的合约有新的Tick数据时调用此函数，先检查是否订阅了该合约，
	 * 如果已订阅则调用基类的Tick更新逻辑，然后通知外部语言有新的Tick数据。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;

	/**
	 * @brief K线闭合事件处理函数
	 * 
	 * 当订阅的K线周期完成并生成新的K线时调用此函数，先调用基类的K线闭合逻辑，
	 * 然后通知外部语言有新的K线闭合事件。
	 * 
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param newBar 新生成的K线数据指针
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;

	/**
	 * @brief 策略计算事件处理函数
	 * 
	 * 当策略需要执行计算逻辑时调用此函数（通常在K线闭合或定时触发时），
	 * 先调用基类的计算逻辑，然后通知外部语言策略需要计算。
	 * 
	 * @param curDate 当前日期（格式：YYYYMMDD）
	 * @param curTime 当前时间（格式：HHMMSS）
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_calculate(uint32_t curDate, uint32_t curTime) override;

	/**
	 * @brief 策略计算完成事件处理函数
	 * 
	 * 当策略计算逻辑执行完毕后调用此函数，先调用基类的计算完成逻辑，
	 * 然后通知外部语言策略计算已完成，最后触发引擎调度事件。
	 * 
	 * @param curDate 当前日期（格式：YYYYMMDD）
	 * @param curTime 当前时间（格式：HHMMSS）
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_calculate_done(uint32_t curDate, uint32_t curTime) override;

	/**
	 * @brief 回测结束事件处理函数
	 * 
	 * 当回测完成时调用此函数，通知外部语言回测已结束。
	 * 
	 * 注意：此函数不调用基类实现，因为基类可能没有此函数或实现不同。
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_bactest_end() override;

	/**
	 * @brief 条件单触发事件处理函数
	 * 
	 * 当策略设置的条件单被触发时调用此函数，通知外部语言条件单已触发。
	 * 
	 * @param stdCode 标准合约代码
	 * @param target 目标持仓数量
	 * @param price 触发价格
	 * @param usertag 用户标签（用于标识该条件单）
	 * 
	 * 重写自CtaMocker基类
	 */
	virtual void on_condition_triggered(const char* stdCode, double target, double price, const char* usertag) override;
};

