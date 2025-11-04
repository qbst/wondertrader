/*!
 * \file WtCtaEngine.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略引擎头文件
 * 
 * 文件设计逻辑和作用总结：
 * ========================
 * WtCtaEngine 是CTA（Commodity Trading Advisor）策略引擎类，负责管理CTA策略的运行和执行。
 * 
 * 主要功能：
 * 1. 策略管理：管理多个CTA策略上下文（CtaContext），每个策略上下文对应一个策略实例
 * 2. 行情处理：接收和处理实时行情数据，包括Tick数据和K线数据
 * 3. 策略调度：定时调用策略的on_schedule方法，触发策略逻辑执行
 * 4. 持仓管理：收集策略的目标仓位，并通过执行器管理器分配给执行器执行
 * 5. 执行器管理：管理执行器（Executer），负责将策略的目标仓位转换为实际交易
 * 6. 数据复权：支持股票的前复权、后复权处理
 * 7. 事件通知：提供图表标记、指标、成交等事件通知功能
 * 
 * 设计特点：
 * - 继承自WtEngine，复用引擎的基础功能（行情订阅、数据管理等）
 * - 实现了IExecuterStub接口，为执行器提供数据访问接口
 * - 支持线程池并发处理策略逻辑，提高性能
 * - 支持策略路由，可以将不同策略的信号路由到不同的执行器
 */
#pragma once
#include "../Includes/ICtaStraCtx.h"			// CTA策略上下文接口头文件
#include "../Share/threadpool.hpp"				// 线程池头文件
#include "WtExecMgr.h"							// 执行器管理器头文件
#include "WtEngine.h"							// 引擎基类头文件

NS_WTP_BEGIN									// WonderTrader命名空间开始

class WTSVariant;								// 变体类型，用于配置参数传递
typedef std::shared_ptr<ICtaStraCtx> CtaContextPtr;	// CTA策略上下文智能指针类型

class WtCtaRtTicker;							// CTA实时时钟类（前向声明）

/**
 * @class WtCtaEngine
 * @brief CTA策略引擎类
 * 
 * WtCtaEngine 是CTA策略引擎，负责管理CTA策略的运行和执行。
 * 继承自WtEngine（提供基础引擎功能）和IExecuterStub（为执行器提供数据访问接口）。
 * 
 * 主要职责：
 * 1. 管理策略上下文：创建和管理多个CTA策略实例
 * 2. 处理行情数据：接收实时行情，并分发给订阅的策略
 * 3. 策略调度：定时调用策略的调度方法，触发策略逻辑
 * 4. 仓位管理：收集策略的目标仓位，分配给执行器执行
 * 5. 执行器管理：管理执行器，负责实际的交易执行
 */
class WtCtaEngine : public WtEngine, public IExecuterStub
{
public:
	/**
	 * @brief 构造函数
	 * 初始化引擎，创建实时时钟对象
	 */
	WtCtaEngine();
	
	/**
	 * @brief 析构函数
	 * 释放资源，清理配置对象和实时时钟对象
	 */
	virtual ~WtCtaEngine();

public:
	//////////////////////////////////////////////////////////////////////////
	// WtEngine接口实现
	// 以下方法实现WtEngine接口，处理引擎的基础功能
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 处理推送的行情数据
	 * @param newTick 新的Tick数据
	 * 
	 * 接收外部推送的行情数据，转发给实时时钟处理。
	 */
	virtual void handle_push_quote(WTSTickData* newTick) override;

	/**
	 * @brief 处理Tick数据回调
	 * @param stdCode 标准合约代码
	 * @param curTick 当前Tick数据
	 * 
	 * 接收Tick数据，分发给订阅该合约的策略，并转发给执行器管理器。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* curTick) override;

	/**
	 * @brief 处理K线数据回调
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据
	 * 
	 * 接收K线数据，分发给订阅该K线的策略。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 初始化回调
	 * 
	 * 引擎初始化时调用，收集所有策略的初始仓位，并提交给执行器执行。
	 */
	virtual void on_init() override;
	
	/**
	 * @brief 交易时段开始回调
	 * 
	 * 交易日开始时调用，通知所有策略交易时段开始。
	 */
	virtual void on_session_begin() override;
	
	/**
	 * @brief 交易时段结束回调
	 * 
	 * 交易日结束时调用，通知所有策略交易时段结束。
	 */
	virtual void on_session_end() override;

	/**
	 * @brief 启动引擎
	 * 
	 * 启动实时时钟，开始运行策略引擎。
	 */
	virtual void run() override;

	/**
	 * @brief 初始化引擎
	 * @param cfg 配置参数
	 * @param bdMgr 基础数据管理器
	 * @param dataMgr 数据管理器
	 * @param hotMgr 主力合约管理器
	 * @param notifier 事件通知器
	 * 
	 * 初始化引擎，设置数据管理器和事件通知器。
	 */
	virtual void init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier) override;

	/**
	 * @brief 检查是否在交易时段
	 * @return true表示在交易时段，false表示不在交易时段
	 */
	virtual bool isInTrading() override;
	
	/**
	 * @brief 将时间转换为分钟数
	 * @param uTime 时间（HHMM格式）
	 * @return 从0点开始的分钟数
	 */
	virtual uint32_t transTimeToMin(uint32_t uTime) override;

	///////////////////////////////////////////////////////////////////////////
	// IExecuterStub接口实现
	// 以下方法实现IExecuterStub接口，为执行器提供数据访问接口
	///////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 获取真实时间
	 * @return 当前时间戳（微秒）
	 */
	virtual uint64_t get_real_time() override;
	
	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return 商品信息指针，失败返回NULL
	 */
	virtual WTSCommodityInfo* get_comm_info(const char* stdCode) override;
	
	/**
	 * @brief 获取交易时段信息
	 * @param stdCode 标准合约代码
	 * @return 交易时段信息指针，失败返回NULL
	 */
	virtual WTSSessionInfo* get_sess_info(const char* stdCode) override;
	
	/**
	 * @brief 获取主力合约管理器
	 * @return 主力合约管理器指针
	 */
	virtual IHotMgr* get_hot_mon() { return _hot_mgr; }
	
	/**
	 * @brief 获取交易日
	 * @return 交易日（YYYYMMDD格式）
	 */
	virtual uint32_t get_trading_day() { return _cur_tdate; }


public:
	/**
	 * @brief 定时调度回调
	 * @param curDate 当前日期（YYYYMMDD格式）
	 * @param curTime 当前时间（HHMM格式）
	 * 
	 * 定时调用策略的on_schedule方法，收集策略的目标仓位，并提交给执行器执行。
	 */
	void on_schedule(uint32_t curDate, uint32_t curTime);	

	/**
	 * @brief 处理策略持仓变化
	 * @param straName 策略名称
	 * @param stdCode 标准合约代码
	 * @param diffPos 仓位变化量（正数表示增加，负数表示减少）
	 * 
	 * 当策略的持仓发生变化时调用，更新组合持仓，并通知执行器管理器。
	 */
	void handle_pos_change(const char* straName, const char* stdCode, double diffPos);

	/**
	 * @brief 添加策略上下文
	 * @param ctx 策略上下文智能指针
	 * 
	 * 添加一个策略上下文到引擎中，引擎会管理该策略的生命周期。
	 */
	void addContext(CtaContextPtr ctx);
	
	/**
	 * @brief 获取策略上下文
	 * @param id 策略ID
	 * @return 策略上下文智能指针，如果不存在则返回空指针
	 */
	CtaContextPtr	getContext(uint32_t id);

	/**
	 * @brief 添加执行器
	 * @param executer 执行器智能指针
	 * 
	 * 添加一个执行器到执行器管理器，并设置执行器的数据访问接口。
	 */
	inline void addExecuter(ExecCmdPtr executer)
	{
		_exec_mgr.add_executer(executer);		// 添加执行器到管理器
		executer->setStub(this);					// 设置执行器的数据访问接口
	}

	/**
	 * @brief 加载路由规则
	 * @param cfg 路由规则配置
	 * @return 加载是否成功
	 * 
	 * 加载策略到执行器的路由规则，用于将不同策略的信号路由到不同的执行器。
	 */
	inline bool loadRouterRules(WTSVariant* cfg)
	{
		return _exec_mgr.load_router_rules(cfg);	// 加载路由规则到执行器管理器
	}

public:
	/**
	 * @brief 通知图表标记
	 * @param time 时间戳
	 * @param straId 策略ID
	 * @param price 价格
	 * @param icon 图标类型
	 * @param tag 标记标签
	 * 
	 * 发送图表标记事件通知，用于在图表上显示标记。
	 */
	void notify_chart_marker(uint64_t time, const char* straId, double price, const char* icon, const char* tag);
	
	/**
	 * @brief 通知图表指标
	 * @param time 时间戳
	 * @param straId 策略ID
	 * @param idxName 指标名称
	 * @param lineName 线名称
	 * @param val 指标值
	 * 
	 * 发送图表指标事件通知，用于在图表上显示策略指标。
	 */
	void notify_chart_index(uint64_t time, const char* straId, const char* idxName, const char* lineName, double val);
	
	/**
	 * @brief 通知成交事件
	 * @param straId 策略ID
	 * @param stdCode 标准合约代码
	 * @param isLong 是否多头
	 * @param isOpen 是否开仓
	 * @param curTime 当前时间
	 * @param price 成交价格
	 * @param userTag 用户标签
	 * 
	 * 发送成交事件通知，用于记录和显示策略的成交信息。
	 */
	void notify_trade(const char* straId, const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, const char* userTag);

private:
	typedef wt_hashmap<uint32_t, CtaContextPtr> ContextMap;	// 策略上下文映射表类型，key为策略ID，value为策略上下文指针
	ContextMap		_ctx_map;						// 策略上下文映射表，管理所有策略实例

	WtCtaRtTicker*	_tm_ticker;						// 实时时钟指针，用于处理实时行情和时间调度

	WtExecuterMgr	_exec_mgr;						// 执行器管理器，管理所有执行器实例

	WTSVariant*		_cfg;							// 配置参数对象，保存引擎的配置信息

	typedef std::shared_ptr<boost::threadpool::pool> ThreadPoolPtr;
	ThreadPoolPtr		_pool;						// 线程池指针，用于并发处理策略逻辑
};

NS_WTP_END										// WonderTrader命名空间结束

