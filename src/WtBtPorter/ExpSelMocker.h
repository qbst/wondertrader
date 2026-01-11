/*!
 * \file ExpSelMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief ExpSelMocker SEL策略扩展模拟器类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了ExpSelMocker类，这是SEL（Selection）选股策略模拟器的扩展实现类，
 * 继承自SelMocker基类。ExpSelMocker的主要作用是将回测引擎内部的事件（如初始化、
 * 交易日开始/结束、Tick更新、K线闭合、策略调度等）转发给外部语言注册的回调函数。
 * 
 * 设计目标：
 * 1. 作为回测引擎与外部语言之间的桥梁，实现事件转发机制
 * 2. 重写SelMocker的虚函数，在调用基类实现后，额外调用外部回调函数
 * 3. 支持选股策略特有的事件通知，包括策略调度事件（定时执行选股逻辑）
 * 4. 通过WtBtRunner单例对象统一管理回调函数的调用
 * 
 * 核心功能：
 * - 策略初始化事件转发：当策略初始化完成时，通知外部语言
 * - 交易日事件转发：当交易日开始或结束时，通知外部语言
 * - Tick数据更新转发：当订阅的合约有新的Tick数据时，通知外部语言
 * - K线闭合事件转发：当订阅的K线周期完成并生成新K线时，通知外部语言
 * - 策略调度事件转发：当策略需要执行选股逻辑时（定时触发），通知外部语言
 * - 回测结束事件转发：当回测完成时，通知外部语言
 * 
 * 架构特点：
 * - 采用装饰器模式，在基类功能基础上增加事件转发功能
 * - 通过WtBtRunner统一管理所有回调函数，避免直接依赖外部语言接口
 * - 使用override关键字确保正确重写基类虚函数
 * - 支持选股策略特有的调度机制（按周期执行选股逻辑）
 */
#pragma once
#include "../WtBtCore/SelMocker.h"  // SEL策略模拟器基类定义

/**
 * @brief ExpSelMocker SEL策略扩展模拟器类
 * 
 * 继承自SelMocker基类，重写所有事件回调函数，在调用基类实现后，
 * 额外调用WtBtRunner中注册的外部回调函数，实现事件转发功能。
 * 
 * 该类是回测引擎与外部语言之间的桥梁，负责将回测过程中的各种事件
 * 转发给外部语言注册的回调函数，使得外部语言可以监听和响应回测事件。
 * 
 * SEL策略相比CTA策略，主要用于选股场景，支持按周期执行选股逻辑。
 */
class ExpSelMocker : public SelMocker
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建SEL策略扩展模拟器实例，初始化回测环境
	 * 
	 * @param replayer 历史数据回放器指针，用于回放历史数据驱动策略执行
	 * @param name 策略名称，用于标识策略实例
	 * @param slippage 滑点设置（单位：最小变动价位），0表示不设置滑点（默认0）
	 * @param isRatioSlp 是否使用比例滑点，true表示比例滑点（滑点=价格*比例），false表示固定滑点（默认false）
	 */
	ExpSelMocker(HisDataReplayer* replayer, const char* name, int32_t slippage = 0, bool isRatioSlp = false);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存
	 */
	virtual ~ExpSelMocker();

public:
	/**
	 * @brief 策略初始化事件处理函数
	 * 
	 * 当策略初始化完成时调用此函数，先调用基类的初始化逻辑，
	 * 然后通知外部语言策略已初始化完成。
	 * 
	 * 重写自SelMocker基类
	 */
	virtual void on_init() override;

	/**
	 * @brief 交易日开始事件处理函数
	 * 
	 * 当新的交易日开始时调用此函数，先调用基类的交易日开始逻辑，
	 * 然后通知外部语言交易日已开始。
	 * 
	 * @param uDate 当前交易日（格式：YYYYMMDD）
	 * 
	 * 重写自SelMocker基类
	 */
	virtual void on_session_begin(uint32_t uDate) override;

	/**
	 * @brief 交易日结束事件处理函数
	 * 
	 * 当交易日结束时调用此函数，先调用基类的交易日结束逻辑，
	 * 然后通知外部语言交易日已结束。
	 * 
	 * @param uDate 当前交易日（格式：YYYYMMDD）
	 * 
	 * 重写自SelMocker基类
	 */
	virtual void on_session_end(uint32_t uDate) override;

	/**
	 * @brief 回测结束事件处理函数
	 * 
	 * 当回测完成时调用此函数，通知外部语言回测已结束。
	 * 
	 * 重写自SelMocker基类
	 */
	virtual void on_bactest_end() override;

	/**
	 * @brief Tick数据更新事件处理函数
	 * 
	 * 当订阅的合约有新的Tick数据时调用此函数，先检查是否订阅了该合约，
	 * 如果已订阅则通知外部语言有新的Tick数据。
	 * 
	 * 注意：此函数不调用基类实现，直接转发给外部语言。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 重写自SelMocker基类
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
	 * 重写自SelMocker基类
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;

	/**
	 * @brief 策略调度事件处理函数
	 * 
	 * 当策略需要执行选股逻辑时调用此函数（按设定的周期定时触发），
	 * 先调用基类的策略调度逻辑，然后通知外部语言策略需要执行选股逻辑，
	 * 最后触发引擎调度事件。
	 * 
	 * 这是SEL策略特有的调度机制，用于按周期执行选股逻辑。
	 * 
	 * @param curDate 当前日期（格式：YYYYMMDD）
	 * @param curTime 当前时间（格式：HHMMSS）
	 * 
	 * 重写自SelMocker基类
	 */
	virtual void on_strategy_schedule(uint32_t curDate, uint32_t curTime) override;
};

