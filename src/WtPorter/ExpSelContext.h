/*!
 * \file ExpSelContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展SEL策略上下文类定义文件
 * 
 * 本文件定义了ExpSelContext类，用于实现扩展的SEL策略上下文。
 * ExpSelContext继承自SelStraBaseCtx，是外部语言（如Python）实现SEL策略的桥梁。
 * 
 * 设计逻辑：
 * - ExpSelContext作为适配器，将SEL策略的各种事件（初始化、交易日开始/结束、
 *   策略调度、Tick更新、K线闭合等）转发给外部语言实现的回调函数
 * - 通过WtRtRunner的回调机制，将策略事件通知给外部语言
 * - 外部语言通过C接口调用策略的查询和交易方法
 */
#pragma once
#include "../WtCore/SelStraBaseCtx.h"  // SEL策略基础上下文类

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 扩展SEL策略上下文类
 * 
 * 扩展的SEL策略上下文类，用于外部语言实现的SEL策略与WonderTrader框架的桥接
 */
class ExpSelContext : public SelStraBaseCtx
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param env SEL引擎指针
	 * @param name 策略名称
	 * @param slippage 滑点设置（单位：最小变动价位）
	 */
	ExpSelContext(WtSelEngine* env, const char* name, int32_t slippage);
	
	/**
	 * @brief 析构函数
	 */
	virtual ~ExpSelContext();

public:
	/**
	 * @brief 策略初始化事件
	 * 
	 * 当策略上下文初始化完成时调用，通知外部语言策略已准备好
	 */
	virtual void on_init() override;

	/**
	 * @brief 交易日开始事件
	 * 
	 * 当新的交易日开始时调用，通知外部语言交易日开始
	 * 
	 * @param uDate 交易日（格式：YYYYMMDD）
	 */
	virtual void on_session_begin(uint32_t uDate) override;

	/**
	 * @brief 交易日结束事件
	 * 
	 * 当交易日结束时调用，通知外部语言交易日结束
	 * 
	 * @param uDate 交易日（格式：YYYYMMDD）
	 */
	virtual void on_session_end(uint32_t uDate) override;

	/**
	 * @brief 策略调度事件
	 * 
	 * 当策略到达调度时间时调用，通知外部语言执行策略计算
	 * 
	 * @param curDate 当前日期（格式：YYYYMMDD）
	 * @param curTime 当前时间（格式：HHMMSS）
	 */
	virtual void on_strategy_schedule(uint32_t curDate, uint32_t curTime) override;

	/**
	 * @brief K线闭合事件
	 * 
	 * 当订阅的K线周期完成并生成新的K线时调用，通知外部语言K线闭合
	 * 
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param newBar 新生成的K线数据结构指针
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;

	/**
	 * @brief Tick更新事件
	 * 
	 * 当订阅的合约有新的Tick数据时调用，通知外部语言Tick更新
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;

};

