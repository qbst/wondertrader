/*!
 * \file SelStraContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 选股策略上下文头文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件定义了选股策略上下文类SelStraContext。
 * 该类继承自SelStraBaseCtx，作为选股策略实例与基础上下文之间的桥梁。
 * 
 * 主要功能：
 * 1. 策略回调转发：将基础上下文的各种回调转发给具体的策略实例。
 * 2. 策略生命周期管理：管理策略的初始化、交易日开始/结束等回调。
 * 3. 数据过滤：对Tick数据进行过滤，只转发已订阅的合约数据。
 * 4. 策略调度：处理定时调度回调，转发给策略实例。
 */
#pragma once  // 防止头文件重复包含
#include "SelStraBaseCtx.h"  // 包含选股策略基础上下文头文件

USING_NS_WTP;  // 使用WonderTrader命名空间

class SelStrategy;  // 前向声明：选股策略类

/**
 * @class SelStraContext
 * @brief 选股策略上下文类
 *
 * 该类继承自SelStraBaseCtx，作为选股策略实例与基础上下文之间的桥梁。
 * 负责将基础上下文的各种回调（初始化、交易日开始/结束、K线收盘、Tick更新、定时调度等）
 * 转发给具体的策略实例。
 * 
 * 特点：
 * 1. 在基础上下文的基础上，增加了策略实例的关联。
 * 2. 对Tick数据进行过滤，只转发已订阅的合约数据。
 * 3. 所有回调都会先调用基础上下文的对应方法，然后再转发给策略实例。
 */
class SelStraContext : public SelStraBaseCtx  // 继承选股策略基础上下文
{
public:
	/**
	 * @brief 构造函数
	 * @param engine 选股引擎指针
	 * @param name 策略名称
	 * @param slippage 滑点设置（回测时使用）
	 *
	 * 初始化选股策略上下文对象，调用基类构造函数并初始化策略指针为NULL。
	 */
	SelStraContext(WtSelEngine* engine, const char* name, int32_t slippage);  // 构造函数
	/**
	 * @brief 析构函数
	 *
	 * 清理解股策略上下文对象。
	 */
	virtual ~SelStraContext();  // 析构函数

	/**
	 * @brief 设置策略实例
	 * @param stra 策略实例指针
	 *
	 * 设置关联的策略实例指针。
	 */
	void set_strategy(SelStrategy* stra){ _strategy = stra; }  // 设置策略实例
	/**
	 * @brief 获取策略实例
	 * @return SelStrategy* 返回策略实例指针
	 *
	 * 获取关联的策略实例指针。
	 */
	SelStrategy* get_stragety() { return _strategy; }  // 获取策略实例（注意：函数名拼写为get_stragety，应为get_strategy）

public:
	/**
	 * @brief 初始化回调
	 *
	 * 策略初始化时被调用，先调用基类的on_init，然后调用策略实例的on_init。
	 */
	virtual void on_init() override;  // 初始化回调

	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期
	 *
	 * 每个交易日开始时被调用，先调用基类的on_session_begin，然后调用策略实例的on_session_begin。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;  // 交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期
	 *
	 * 每个交易日结束时被调用，先调用策略实例的on_session_end，然后调用基类的on_session_end。
	 */
	virtual void on_session_end(uint32_t uTDate) override;  // 交易日结束回调

	/**
	 * @brief K线收盘回调
	 * @param stdCode 标准合约代码
	 * @param period 周期
	 * @param newBar 新的K线数据
	 *
	 * 当K线收盘时被调用，转发给策略实例的on_bar方法。
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;  // K线收盘回调

	/**
	 * @brief Tick更新回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据
	 *
	 * 当Tick数据更新时被调用，只转发已订阅的合约数据给策略实例的on_tick方法。
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;  // Tick更新回调

	/**
	 * @brief 策略定时调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 *
	 * 定时调度时被调用，转发给策略实例的on_schedule方法。
	 */
	virtual void on_strategy_schedule(uint32_t curDate, uint32_t curTime) override;  // 策略定时调度回调

private:
	SelStrategy* _strategy;  // 策略实例指针
};

