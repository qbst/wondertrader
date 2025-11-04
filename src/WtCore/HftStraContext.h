/*!
 * \file HftStraContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易策略上下文头文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件定义了高频交易策略上下文类HftStraContext，它继承自HftStraBaseCtx（高频策略基础上下文）。
 * HftStraContext作为策略实例（HftStrategy）和基础上下文（HftStraBaseCtx）之间的桥梁，
 * 负责将各种市场数据回调和交易通知回调转发给具体的策略实例。
 * 
 * 主要功能：
 * 1. 策略实例管理：持有并管理HftStrategy实例的指针。
 * 2. 回调转发：将所有IHftStraCtx和ITrdNotifySink接口的回调转发给策略实例。
 * 3. 动态盈亏更新：在收到Tick数据时，先更新动态盈亏，再转发给策略。
 * 4. 订阅过滤：对于Tick数据回调，只转发给已订阅该合约的策略。
 * 5. 代码映射：在处理交易相关回调时，将标准代码转换为内部代码。
 * 6. 订单标签传递：在成交通知和订单通知中，传递订单的用户标签给策略。
 */
#pragma once  // 防止头文件重复包含
#include "HftStraBaseCtx.h"  // 包含高频策略基础上下文头文件


USING_NS_WTP;  // 使用WonderTrader命名空间

class HftStrategy;  // 前向声明：高频交易策略类

/**
 * @class HftStraContext
 * @brief 高频交易策略上下文类
 *
 * 该类继承自HftStraBaseCtx，作为策略实例和基础上下文之间的桥梁。
 * 它持有HftStrategy实例的指针，并将所有市场数据回调和交易通知回调转发给策略实例。
 * 在转发回调之前，会先执行基础上下文的一些处理（如更新动态盈亏、代码映射等）。
 */
class HftStraContext : public HftStraBaseCtx  // 继承高频策略基础上下文
{
public:
	/**
	 * @brief 构造函数
	 * @param engine 高频交易引擎指针
	 * @param name 策略名称
	 * @param bAgent 是否启用数据托管模式
	 * @param slippage 滑点点数（用于回测）
	 *
	 * 初始化高频交易策略上下文对象，调用基类构造函数。
	 */
	HftStraContext(WtHftEngine* engine, const char* name, bool bAgent, int32_t slippage);  // 构造函数
	/**
	 * @brief 虚析构函数
	 *
	 * 虚析构函数确保继承类能够正确析构，释放资源。
	 */
	virtual ~HftStraContext();  // 析构函数

	/**
	 * @brief 设置策略实例
	 * @param stra 策略实例指针
	 *
	 * 设置当前上下文关联的策略实例。
	 */
	void set_strategy(HftStrategy* stra){ _strategy = stra; }  // 设置策略实例指针
	/**
	 * @brief 获取策略实例
	 * @return HftStrategy* 返回策略实例指针
	 *
	 * 获取当前上下文关联的策略实例。
	 * 注意：函数名拼写错误（get_stragety应为get_strategy），但为保持兼容性保留原函数名。
	 */
	HftStrategy* get_stragety() { return _strategy; }  // 获取策略实例指针

public:
	/**
	 * @brief 策略初始化回调
	 *
	 * 当策略初始化时被调用。
	 * 先调用基类的on_init方法，然后如果策略实例存在，则调用策略的on_init方法。
	 */
	virtual void on_init() override;  // 策略初始化回调

	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日期
	 *
	 * 当交易日开始时被调用。
	 * 先调用基类的on_session_begin方法，然后如果策略实例存在，则调用策略的on_session_begin方法。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;  // 交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日期
	 *
	 * 当交易日结束时被调用。
	 * 先调用策略的on_session_end方法（如果策略实例存在），然后调用基类的on_session_end方法。
	 */
	virtual void on_session_end(uint32_t uTDate) override;  // 交易日结束回调

	/**
	 * @brief Tick数据回调
	 * @param code 标准合约代码
	 * @param newTick 新的Tick数据指针
	 *
	 * 当收到新的Tick数据时被调用。
	 * 1. 先更新动态盈亏。
	 * 2. 如果策略已订阅该合约的Tick数据，则调用策略的on_tick方法。
	 * 3. 最后调用基类的on_tick方法。
	 */
	virtual void on_tick(const char* code, WTSTickData* newTick) override;  // Tick数据回调

	/**
	 * @brief 委托队列数据回调
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 新的委托队列数据指针
	 *
	 * 当收到新的委托队列数据时被调用。
	 * 先调用策略的on_order_queue方法（如果策略实例存在），然后调用基类的on_order_queue方法。
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) override;  // 委托队列数据回调

	/**
	 * @brief 委托明细数据回调
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 新的委托明细数据指针
	 *
	 * 当收到新的委托明细数据时被调用。
	 * 先调用策略的on_order_detail方法（如果策略实例存在），然后调用基类的on_order_detail方法。
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;  // 委托明细数据回调

	/**
	 * @brief 逐笔成交数据回调
	 * @param stdCode 标准合约代码
	 * @param newTrans 新的逐笔成交数据指针
	 *
	 * 当收到新的逐笔成交数据时被调用。
	 * 先调用策略的on_transaction方法（如果策略实例存在），然后调用基类的on_transaction方法。
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) override;  // 逐笔成交数据回调

	/**
	 * @brief K线数据回调
	 * @param code 标准合约代码
	 * @param period 周期（如"m1"、"m5"等）
	 * @param times 周期倍数
	 * @param newBar 新的K线数据指针
	 *
	 * 当收到新的K线数据时被调用。
	 * 先调用策略的on_bar方法（如果策略实例存在），然后调用基类的on_bar方法。
	 */
	virtual void on_bar(const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) override;  // K线数据回调

	/**
	 * @brief 成交通知回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向
	 * @param vol 成交数量
	 * @param price 成交价格
	 *
	 * 当订单成交时被调用。
	 * 1. 将标准代码转换为内部代码。
	 * 2. 如果策略实例存在，调用策略的on_trade方法，并传递订单标签。
	 * 3. 调用基类的on_trade方法。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;  // 成交通知回调

	/**
	 * @brief 订单状态通知回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销，默认false
	 *
	 * 当订单状态发生变化时被调用。
	 * 1. 将标准代码转换为内部代码。
	 * 2. 如果策略实例存在，调用策略的on_order方法，并传递订单标签。
	 * 3. 调用基类的on_order方法。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled = false) override;  // 订单状态通知回调

	/**
	 * @brief 交易通道就绪通知回调
	 *
	 * 当交易通道就绪时被调用。
	 * 先调用策略的on_channel_ready方法（如果策略实例存在），然后调用基类的on_channel_ready方法。
	 */
	virtual void on_channel_ready() override;  // 交易通道就绪通知回调

	/**
	 * @brief 交易通道丢失通知回调
	 *
	 * 当交易通道丢失时被调用。
	 * 先调用策略的on_channel_lost方法（如果策略实例存在），然后调用基类的on_channel_lost方法。
	 */
	virtual void on_channel_lost() override;  // 交易通道丢失通知回调

	/**
	 * @brief 委托通知回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息内容
	 *
	 * 当委托结果返回时被调用。
	 * 1. 如果策略实例存在，调用策略的on_entrust方法，并传递订单标签。
	 * 2. 调用基类的on_entrust方法，并将标准代码转换为内部代码。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;  // 委托通知回调

	/**
	 * @brief 持仓变化通知回调
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为做多方向
	 * @param prevol 变化前持仓数量
	 * @param preavail 变化前可用持仓数量
	 * @param newvol 变化后持仓数量
	 * @param newavail 变化后可用持仓数量
	 * @param tradingday 交易日
	 *
	 * 当持仓发生变化时被调用。
	 * 如果策略实例存在，则调用策略的on_position方法。
	 * 注意：此方法不调用基类的on_position方法。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;  // 持仓变化通知回调


private:
	HftStrategy*		_strategy;  // 策略实例指针
};

