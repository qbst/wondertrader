/*!
 * \file WtHftEngine.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易引擎头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的高频交易（HFT）引擎类，继承自WtEngine基类。
 * 主要功能包括：
 * 1. 策略管理：管理多个HFT策略上下文，支持策略的初始化、运行和生命周期管理
 * 2. 行情处理：处理实时行情数据（Tick、委托队列、委托明细、成交明细），并分发给订阅的策略
 * 3. Level2数据订阅：支持策略订阅委托队列、委托明细、成交明细等Level2市场数据
 * 4. 时间管理：通过WtHftRtTicker管理实时行情时间，触发分钟线闭合和交易日切换
 * 5. 数据查询：提供Level2历史数据查询接口（委托队列切片、委托明细切片、成交明细切片）
 * 6. 复权处理：支持前复权（QFQ）和后复权（HFQ）的行情数据处理
 * 
 * 设计模式：
 * - 继承模式：继承自WtEngine，复用基础引擎功能
 * - 观察者模式：策略订阅行情数据，引擎负责分发
 * - 工厂模式：通过工厂创建策略上下文
 * 
 * 使用场景：
 * 该引擎主要用于高频交易场景，需要处理实时行情数据、Level2数据和极速交易执行。
 * 适用于对延迟要求极高的交易策略，如做市、套利、高频趋势跟踪等。
 */
#pragma once  // 防止头文件重复包含
#include "WtEngine.h"  // 包含基础引擎头文件
#include "WtLocalExecuter.h"  // 包含本地执行器头文件

#include "../Includes/IHftStraCtx.h"  // 包含HFT策略上下文接口头文件

NS_WTP_BEGIN  // 开始WonderTrader命名空间

class WTSVariant;  // 前向声明：变体类型类，用于配置参数
class WtHftRtTicker;  // 前向声明：HFT实时行情时钟类

typedef std::shared_ptr<IHftStraCtx> HftContextPtr;  // HFT策略上下文智能指针类型别名

/**
 * @class WtHftEngine
 * @brief 高频交易引擎类
 * 
 * 该类是WonderTrader框架中的高频交易引擎实现，继承自WtEngine基类。
 * 负责管理HFT策略的运行环境，处理实时行情数据，并支持Level2数据订阅和查询。
 * 
 * 主要特性：
 * - 支持多个HFT策略同时运行
 * - 实时处理Tick行情、委托队列、委托明细、成交明细
 * - 支持前复权和后复权数据处理
 * - 自动管理策略生命周期（初始化、运行、结束）
 * - 提供Level2历史数据查询接口
 */
class WtHftEngine :	public WtEngine  // 继承自WtEngine基类
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化HFT引擎，设置配置对象和时间时钟指针为NULL。
	 */
	WtHftEngine();  // 构造函数
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理资源，停止时间时钟并释放配置对象。
	 */
	virtual ~WtHftEngine();  // 虚析构函数

public:
	//////////////////////////////////////////////////////////////////////////
	//WtEngine 接口实现
	/**
	 * @brief 初始化引擎
	 * @param cfg 配置对象指针
	 * @param bdMgr 基础数据管理器指针
	 * @param dataMgr 数据管理器指针
	 * @param hotMgr 热点合约管理器指针
	 * @param notifier 事件通知器指针
	 * 
	 * 初始化HFT引擎，调用基类初始化方法，并保存配置对象。
	 */
	virtual void init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier) override;  // 初始化引擎

	/**
	 * @brief 运行引擎
	 * 
	 * 启动HFT引擎，初始化所有策略上下文，创建并启动实时行情时钟。
	 * 启动前会生成策略和通道标记文件。
	 */
	virtual void run() override;  // 运行引擎

	/**
	 * @brief 处理推送的行情数据
	 * @param newTick 新的Tick数据指针
	 * 
	 * 接收外部推送的实时行情数据，转发给实时行情时钟处理。
	 */
	virtual void handle_push_quote(WTSTickData* newTick) override;  // 处理推送的行情数据

	/**
	 * @brief 处理推送的委托明细数据
	 * @param curOrdDtl 委托明细数据指针
	 * 
	 * 接收外部推送的委托明细数据，分发给订阅了该合约的策略。
	 * Level2数据不进行复权处理。
	 */
	virtual void handle_push_order_detail(WTSOrdDtlData* curOrdDtl) override;  // 处理推送的委托明细数据

	/**
	 * @brief 处理推送的委托队列数据
	 * @param curOrdQue 委托队列数据指针
	 * 
	 * 接收外部推送的委托队列数据，分发给订阅了该合约的策略。
	 * Level2数据不进行复权处理。
	 */
	virtual void handle_push_order_queue(WTSOrdQueData* curOrdQue) override;  // 处理推送的委托队列数据

	/**
	 * @brief 处理推送的成交明细数据
	 * @param curTrans 成交明细数据指针
	 * 
	 * 接收外部推送的成交明细数据，分发给订阅了该合约的策略。
	 * Level2数据不进行复权处理。
	 */
	virtual void handle_push_transaction(WTSTransData* curTrans) override;  // 处理推送的成交明细数据

	/**
	 * @brief Tick行情回调
	 * @param stdCode 标准合约代码
	 * @param curTick 当前Tick数据指针
	 * 
	 * 处理Tick行情数据，根据订阅标记进行复权处理，然后分发给订阅的策略。
	 * 支持无复权、前复权和后复权三种模式。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* curTick) override;  // Tick行情回调

	/**
	 * @brief K线数据回调
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 处理K线数据闭合事件，分发给订阅了该K线的策略。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;  // K线数据回调

	/**
	 * @brief 交易日开始回调
	 * 
	 * 处理交易日开始事件，通知所有策略并设置引擎就绪标志。
	 */
	virtual void on_session_begin() override;  // 交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * 
	 * 处理交易日结束事件，通知所有策略并记录日志。
	 */
	virtual void on_session_end() override;  // 交易日结束回调

public:
	/**
	 * @brief 获取委托队列历史数据切片
	 * @param sid 策略ID（未使用）
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
	 * 
	 * 查询指定合约的委托队列历史数据，返回指定条数的数据切片。
	 */
	WTSOrdQueSlice* get_order_queue_slice(uint32_t sid, const char* stdCode, uint32_t count);  // 获取委托队列历史数据切片

	/**
	 * @brief 获取委托明细历史数据切片
	 * @param sid 策略ID（未使用）
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针
	 * 
	 * 查询指定合约的委托明细历史数据，返回指定条数的数据切片。
	 */
	WTSOrdDtlSlice* get_order_detail_slice(uint32_t sid, const char* stdCode, uint32_t count);  // 获取委托明细历史数据切片

	/**
	 * @brief 获取成交明细历史数据切片
	 * @param sid 策略ID（未使用）
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSTransSlice* 返回成交明细数据切片指针
	 * 
	 * 查询指定合约的成交明细历史数据，返回指定条数的数据切片。
	 */
	WTSTransSlice* get_transaction_slice(uint32_t sid, const char* stdCode, uint32_t count);  // 获取成交明细历史数据切片

public:
	/**
	 * @brief 分钟结束回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间（分钟）
	 * 
	 * 处理分钟线闭合事件，由实时行情时钟调用。
	 * 目前HFT策略不再调用on_schedule方法，此函数保留为空实现。
	 */
	void on_minute_end(uint32_t curDate, uint32_t curTime);  // 分钟结束回调

	/**
	 * @brief 添加策略上下文
	 * @param ctx 策略上下文智能指针
	 * 
	 * 将策略上下文添加到引擎的管理列表中，使用策略ID作为键。
	 */
	void addContext(HftContextPtr ctx);  // 添加策略上下文

	/**
	 * @brief 获取策略上下文
	 * @param id 策略ID
	 * @return HftContextPtr 返回策略上下文智能指针，不存在返回空指针
	 * 
	 * 根据策略ID查找并返回对应的策略上下文。
	 */
	HftContextPtr	getContext(uint32_t id);  // 获取策略上下文

	/**
	 * @brief 订阅委托队列数据
	 * @param sid 策略ID
	 * @param stdCode 标准合约代码
	 * 
	 * 为指定策略订阅指定合约的委托队列数据。
	 * 如果代码包含复权后缀，会自动去除后缀进行订阅。
	 */
	void sub_order_queue(uint32_t sid, const char* stdCode);  // 订阅委托队列数据

	/**
	 * @brief 订阅委托明细数据
	 * @param sid 策略ID
	 * @param stdCode 标准合约代码
	 * 
	 * 为指定策略订阅指定合约的委托明细数据。
	 * 如果代码包含复权后缀，会自动去除后缀进行订阅。
	 */
	void sub_order_detail(uint32_t sid, const char* stdCode);  // 订阅委托明细数据

	/**
	 * @brief 订阅成交明细数据
	 * @param sid 策略ID
	 * @param stdCode 标准合约代码
	 * 
	 * 为指定策略订阅指定合约的成交明细数据。
	 * 如果代码包含复权后缀，会自动去除后缀进行订阅。
	 */
	void sub_transaction(uint32_t sid, const char* stdCode);  // 订阅成交明细数据

private:
	typedef wt_hashmap<uint32_t, HftContextPtr> ContextMap;  // 策略上下文映射表类型别名，键为策略ID，值为策略上下文指针
	ContextMap		_ctx_map;  // 策略上下文映射表，存储所有注册的HFT策略上下文

	WtHftRtTicker*	_tm_ticker;  // 实时行情时钟指针，用于管理实时行情时间和触发分钟线闭合
	WTSVariant*		_cfg;  // 配置对象指针，存储引擎配置信息


	StraSubMap		_ordque_sub_map;	// 委托队列订阅表，键为合约代码，值为订阅该合约的策略ID列表
	StraSubMap		_orddtl_sub_map;	// 委托明细订阅表，键为合约代码，值为订阅该合约的策略ID列表
	StraSubMap		_trans_sub_map;		// 成交明细订阅表，键为合约代码，值为订阅该合约的策略ID列表
};

NS_WTP_END  // 结束WonderTrader命名空间
