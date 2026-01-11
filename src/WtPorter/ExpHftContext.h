/*!
 * \file ExpHftContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展HFT策略上下文类定义文件
 * 
 * 本文件定义了ExpHftContext类，用于实现扩展的HFT策略上下文。
 * ExpHftContext继承自HftStraBaseCtx，是外部语言（如Python）实现HFT策略的桥梁。
 * 
 * 设计逻辑：
 * - ExpHftContext作为适配器，将HFT策略的各种事件（初始化、交易日开始/结束、
 *   Tick更新、K线闭合、订单回报、成交回报、委托回报、持仓变化、订单队列、
 *   订单明细、逐笔成交、通道事件等）转发给外部语言实现的回调函数
 * - 通过WtRtRunner的回调机制，将策略事件通知给外部语言
 * - 外部语言通过C接口调用策略的查询和交易方法
 */
#pragma once
#include "../WtCore/HftStraBaseCtx.h"  // HFT策略基础上下文类

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 扩展HFT策略上下文类
 * 
 * 扩展的HFT策略上下文类，用于外部语言实现的HFT策略与WonderTrader框架的桥接
 */
class ExpHftContext : public HftStraBaseCtx
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param engine HFT引擎指针
	 * @param name 策略名称
	 * @param bAgent true表示使用代理模式（通过执行器下单），false表示直接下单
	 * @param slippage 滑点设置（单位：最小变动价位）
	 */
	ExpHftContext(WtHftEngine* engine, const char* name, bool bAgent, int32_t slippage):HftStraBaseCtx(engine, name, bAgent, slippage){}
	
	/**
	 * @brief 析构函数
	 */
	virtual ~ExpHftContext(){}

public:
	/**
	 * @brief K线闭合事件
	 * 
	 * 当订阅的K线周期完成并生成新的K线时调用
	 * 
	 * @param code 合约代码
	 * @param period K线周期（如"m1"等）
	 * @param times 周期倍数（如"m5"中的5）
	 * @param newBar 新生成的K线数据结构指针
	 */
	virtual void on_bar(const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 交易通道断开事件
	 * 
	 * 当交易通道断开连接时调用
	 */
	virtual void on_channel_lost() override;

	/**
	 * @brief 交易通道就绪事件
	 * 
	 * 当交易通道连接成功并准备就绪时调用
	 */
	virtual void on_channel_ready() override;

	/**
	 * @brief 委托回报事件
	 * 
	 * 当委托单提交后收到回报时调用（成功或失败）
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功
	 * @param message 返回消息（如果失败，包含错误信息）
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;

	/**
	 * @brief 策略初始化事件
	 * 
	 * 当策略上下文初始化完成时调用
	 */
	virtual void on_init() override;

	/**
	 * @brief 交易日开始事件
	 * 
	 * 当新的交易日开始时调用
	 * 
	 * @param uTDate 交易日（格式：YYYYMMDD）
	 */
	virtual void on_session_begin(uint32_t uTDate) override;

	/**
	 * @brief 交易日结束事件
	 * 
	 * 当交易日结束时调用
	 * 
	 * @param uTDate 交易日（格式：YYYYMMDD）
	 */
	virtual void on_session_end(uint32_t uTDate) override;

	/**
	 * @brief 订单状态变化事件
	 * 
	 * 当订单状态发生变化时调用（如订单提交、部分成交、全部成交、撤单等）
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy true表示买入，false表示卖出
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余未成交数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤单
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled) override;

	/**
	 * @brief Tick更新事件
	 * 
	 * 当订阅的合约有新的Tick数据时调用
	 * 
	 * @param code 合约代码
	 * @param newTick 新的Tick数据指针
	 */
	virtual void on_tick(const char* code, WTSTickData* newTick) override;

	/**
	 * @brief 订单队列更新事件
	 * 
	 * 当订阅的合约有新的订单队列数据时调用（Level2行情）
	 * 
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 新的订单队列数据指针
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) override;

	/**
	 * @brief 订单明细更新事件
	 * 
	 * 当订阅的合约有新的订单明细数据时调用（Level2行情）
	 * 
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 新的订单明细数据指针
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;

	/**
	 * @brief 逐笔成交更新事件
	 * 
	 * 当订阅的合约有新的逐笔成交数据时调用（Level2行情）
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTrans 新的逐笔成交数据指针
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) override;

	/**
	 * @brief 成交回报事件
	 * 
	 * 当订单有成交回报时调用
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy true表示买入，false表示卖出
	 * @param vol 成交数量
	 * @param price 成交价格
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;

	/**
	 * @brief 持仓变化事件
	 * 
	 * 当持仓发生变化时调用
	 * 
	 * @param stdCode 标准合约代码
	 * @param isLong true表示多头持仓，false表示空头持仓
	 * @param prevol 变化前持仓数量
	 * @param preavail 变化前可用持仓数量
	 * @param newvol 变化后持仓数量
	 * @param newavail 变化后可用持仓数量
	 * @param tradingday 交易日（格式：YYYYMMDD）
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;
};

