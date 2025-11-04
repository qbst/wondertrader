/*!
 * \file CtaStraContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略上下文头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中CTA策略的具体上下文类，继承自CtaStraBaseCtx。
 * 主要功能包括：
 * 1. 作为策略和基础上下文之间的桥梁，将引擎的回调转发给策略对象
 * 2. 实现策略的生命周期回调：初始化、交易日开始/结束、Tick更新、K线闭合、策略计算等
 * 3. 管理策略对象的指针，提供策略对象的设置和获取接口
 * 4. 在策略回调中增加额外的处理逻辑（如导出图表信息等）
 * 
 * 该类是CTA策略运行时的实际上下文对象，负责将引擎的各种事件转发给策略对象处理。
 * 通过继承CtaStraBaseCtx，获得了完整的策略运行环境和交易接口。
 */
#pragma once
#include "CtaStraBaseCtx.h"  // 包含CTA策略基础上下文头文件

#include "../Includes/WTSDataDef.hpp"  // 包含WTS数据结构定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WtCtaEngine;  // 前向声明：CTA引擎类
NS_WTP_END  // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

class CtaStrategy;  // 前向声明：CTA策略类

/**
 * @class CtaStraContext
 * @brief CTA策略上下文类
 * 
 * 该类是CTA策略运行时的具体上下文实现，继承自CtaStraBaseCtx。
 * 主要作用是作为策略对象和基础上下文之间的桥梁，将引擎的各种回调事件转发给策略对象。
 * 
 * 主要特性：
 * - 管理策略对象指针，提供策略对象的设置和获取接口
 * - 实现策略的生命周期回调，将事件转发给策略对象
 * - 在策略初始化时导出图表配置信息
 * - 在策略回调前后可以添加额外的处理逻辑
 */
class CtaStraContext : public CtaStraBaseCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param engine CTA引擎指针，用于访问引擎提供的功能
	 * @param name 策略上下文名称，用于标识该策略实例
	 * @param slippage 滑点设置，单位为最小价格变动单位
	 * 
	 * 初始化CTA策略上下文对象，调用基类构造函数。
	 */
	CtaStraContext(WtCtaEngine* engine, const char* name, int32_t slippage);
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理CTA策略上下文对象，释放相关资源。
	 */
	virtual ~CtaStraContext();

	/**
	 * @brief 设置策略对象
	 * @param stra 策略对象指针
	 * 
	 * 设置该上下文关联的策略对象，用于后续的事件转发。
	 */
	void set_strategy(CtaStrategy* stra){ _strategy = stra; }
	
	/**
	 * @brief 获取策略对象
	 * @return CtaStrategy* 返回策略对象指针
	 * 
	 * 获取该上下文关联的策略对象指针。
	 */
	CtaStrategy* get_stragety() { return _strategy; }

public:
	//////////////////////////////////////////////////////////////////////////
	//回调函数（继承自ICtaStraCtx接口，将事件转发给策略对象）
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 策略初始化回调
	 * 
	 * 策略初始化时调用，先调用基类的on_init完成基础初始化，
	 * 然后调用策略对象的on_init进行策略特定的初始化，
	 * 最后导出图表配置信息。
	 */
	virtual void on_init() override;
	
	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期，格式为YYYYMMDD
	 * 
	 * 每个交易日开始时调用，先调用基类的on_session_begin完成基础处理，
	 * 然后调用策略对象的on_session_begin进行策略特定的处理。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;
	
	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期，格式为YYYYMMDD
	 * 
	 * 每个交易日结束时调用，先调用策略对象的on_session_end进行策略特定的处理，
	 * 然后调用基类的on_session_end完成基础处理（如保存数据）。
	 */
	virtual void on_session_end(uint32_t uTDate) override;
	
	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当收到订阅合约的Tick数据更新时调用。
	 * 只有订阅的合约才会触发此回调，然后转发给策略对象的on_tick处理。
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;
	
	/**
	 * @brief K线闭合回调
	 * @param stdCode 合约代码
	 * @param period K线周期，如"m1"、"d1"等
	 * @param newBar 闭合的K线数据指针
	 * 
	 * 当订阅的K线闭合时调用，转发给策略对象的on_bar处理。
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 策略计算回调
	 * @param curDate 当前日期，格式为YYYYMMDD
	 * @param curTime 当前时间，格式为HHMMSS
	 * 
	 * 定时调度时调用，转发给策略对象的on_schedule执行策略逻辑。
	 */
	virtual void on_calculate(uint32_t curDate, uint32_t curTime) override;
	
	/**
	 * @brief 条件单触发回调
	 * @param stdCode 合约代码
	 * @param target 目标仓位
	 * @param price 触发价格
	 * @param usertag 用户标签
	 * 
	 * 当条件单触发时调用，转发给策略对象的on_condition_triggered处理。
	 */
	virtual void on_condition_triggered(const char* stdCode, double target, double price, const char* usertag) override;

private:
	CtaStrategy*		_strategy;  // 策略对象指针：指向关联的CTA策略对象，用于事件转发
};


