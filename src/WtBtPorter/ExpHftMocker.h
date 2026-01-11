/*!
 * \file ExpHftMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief ExpHftMocker HFT策略扩展模拟器类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了ExpHftMocker类，这是HFT（High Frequency Trading）高频交易策略模拟器的扩展实现类，
 * 继承自HftMocker基类。ExpHftMocker的主要作用是将回测引擎内部的事件（如初始化、交易日开始/结束、
 * Tick更新、K线闭合、订单状态变化、成交回报、委托回报、Level2行情等）转发给外部语言注册的回调函数。
 * 
 * 设计目标：
 * 1. 作为回测引擎与外部语言之间的桥梁，实现事件转发机制
 * 2. 重写HftMocker的虚函数，在调用基类实现后，额外调用外部回调函数
 * 3. 支持HFT策略特有的事件通知，包括订单队列、订单明细、逐笔成交等Level2行情
 * 4. 通过WtBtRunner单例对象统一管理回调函数的调用
 * 
 * 核心功能：
 * - 策略初始化事件转发：当策略初始化完成时，通知外部语言
 * - 交易日事件转发：当交易日开始或结束时，通知外部语言
 * - Tick数据更新转发：当订阅的合约有新的Tick数据时，通知外部语言
 * - K线闭合事件转发：当订阅的K线周期完成并生成新K线时，通知外部语言
 * - 交易通道事件转发：当交易通道连接成功并准备就绪时，通知外部语言
 * - 订单状态变化转发：当订单状态发生变化时，通知外部语言
 * - 成交回报转发：当订单有成交回报时，通知外部语言
 * - 委托回报转发：当委托单提交后收到回报时，通知外部语言
 * - Level2行情转发：当有订单队列、订单明细、逐笔成交数据时，通知外部语言
 * - 回测结束事件转发：当回测完成时，通知外部语言
 * 
 * 架构特点：
 * - 采用装饰器模式，在基类功能基础上增加事件转发功能
 * - 通过WtBtRunner统一管理所有回调函数，避免直接依赖外部语言接口
 * - 使用override关键字确保正确重写基类虚函数
 * - 支持高频交易特有的Level2行情数据（订单队列、订单明细、逐笔成交）
 */
#pragma once
#include "../WtBtCore/HftMocker.h"  // HFT策略模拟器基类定义

/**
 * @brief ExpHftMocker HFT策略扩展模拟器类
 * 
 * 继承自HftMocker基类，重写所有事件回调函数，在调用基类实现后，
 * 额外调用WtBtRunner中注册的外部回调函数，实现事件转发功能。
 * 
 * 该类是回测引擎与外部语言之间的桥梁，负责将回测过程中的各种事件
 * 转发给外部语言注册的回调函数，使得外部语言可以监听和响应回测事件。
 * 
 * HFT策略相比CTA策略，增加了对Level2行情的支持，包括订单队列、订单明细、逐笔成交等。
 */
class ExpHftMocker : public HftMocker
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建HFT策略扩展模拟器实例，初始化回测环境
	 * 
	 * @param replayer 历史数据回放器指针，用于回放历史数据驱动策略执行
	 * @param name 策略名称，用于标识策略实例
	 */
	ExpHftMocker(HisDataReplayer* replayer, const char* name);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存
	 */
	virtual ~ExpHftMocker(){}

	/**
	 * @brief K线闭合事件处理函数
	 * 
	 * 当订阅的K线周期完成并生成新的K线时调用此函数，先调用基类的K线闭合逻辑，
	 * 然后通知外部语言有新的K线闭合事件。
	 * 
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param times K线倍数（如period为"m"时，times为5表示5分钟K线）
	 * @param newBar 新生成的K线数据指针
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 交易通道就绪事件处理函数
	 * 
	 * 当HFT策略的交易通道连接成功并准备就绪时调用此函数，先调用基类的通道就绪逻辑，
	 * 然后通知外部语言交易通道已就绪。
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_channel_ready() override;

	/**
	 * @brief 委托回报事件处理函数
	 * 
	 * 当HFT策略的委托单提交后收到回报时调用此函数，先调用基类的委托回报逻辑，
	 * 然后通知外部语言委托回报信息。
	 * 
	 * @param localid 本地订单ID（下单时返回的订单ID）
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功，true表示委托成功，false表示委托失败
	 * @param message 返回消息（如果失败，包含失败原因）
	 * @param userTag 用户标签（用于标识该笔交易）
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag) override;

	/**
	 * @brief 策略初始化事件处理函数
	 * 
	 * 当策略初始化完成时调用此函数，先调用基类的初始化逻辑，
	 * 然后通知外部语言策略已初始化完成。
	 * 
	 * 重写自HftMocker基类
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
	 * 重写自HftMocker基类
	 */
	virtual void on_session_begin(uint32_t uDate) override;

	/**
	 * @brief 交易日结束事件处理函数
	 * 
	 * 当交易日结束时调用此函数，先通知外部语言交易日已结束，
	 * 然后调用基类的交易日结束逻辑。
	 * 
	 * @param uDate 当前交易日（格式：YYYYMMDD）
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_session_end(uint32_t uDate) override;

	/**
	 * @brief 回测结束事件处理函数
	 * 
	 * 当回测完成时调用此函数，通知外部语言回测已结束。
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_bactest_end() override;

	/**
	 * @brief 订单状态变化事件处理函数
	 * 
	 * 当HFT策略的订单状态发生变化时调用此函数，先调用基类的订单状态变化逻辑，
	 * 然后通知外部语言订单状态变化信息。
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy true表示买入订单，false表示卖出订单
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余未成交数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤单，true表示已撤单，false表示未撤单
	 * @param userTag 用户标签
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag) override;

	/**
	 * @brief Tick数据更新事件处理函数
	 * 
	 * 当订阅的合约有新的Tick数据时调用此函数，先检查是否订阅了该合约，
	 * 如果已订阅则调用基类的Tick更新逻辑，然后通知外部语言有新的Tick数据。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;

	/**
	 * @brief 订单队列更新事件处理函数
	 * 
	 * 当订阅的合约有新的订单队列数据（Level2行情）时调用此函数，
	 * 通知外部语言有新的订单队列数据。
	 * 
	 * 注意：此函数不调用基类实现，直接转发给外部语言。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 新的订单队列数据指针，包含买卖盘口信息
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue) override;

	/**
	 * @brief 订单明细更新事件处理函数
	 * 
	 * 当订阅的合约有新的订单明细数据（Level2行情）时调用此函数，
	 * 通知外部语言有新的订单明细数据。
	 * 
	 * 注意：此函数不调用基类实现，直接转发给外部语言。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 新的订单明细数据指针，包含订单簿信息
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;

	/**
	 * @brief 逐笔成交更新事件处理函数
	 * 
	 * 当订阅的合约有新的逐笔成交数据（Level2行情）时调用此函数，
	 * 通知外部语言有新的逐笔成交数据。
	 * 
	 * 注意：此函数不调用基类实现，直接转发给外部语言。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTrans 新的逐笔成交数据指针，包含每笔成交的详细信息
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_trans_updated(const char* stdCode, WTSTransData* newTrans) override;

	/**
	 * @brief 成交回报事件处理函数
	 * 
	 * 当HFT策略的订单有成交回报时调用此函数，先调用基类的成交回报逻辑，
	 * 然后通知外部语言成交回报信息。
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy true表示买入成交，false表示卖出成交
	 * @param vol 成交数量
	 * @param price 成交价格
	 * @param userTag 用户标签
	 * 
	 * 重写自HftMocker基类
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag) override;

};

