/*!
 * \file UftStrategyDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT极速策略引擎核心定义文件
 * 
 * 本文件定义了WonderTrader框架中UFT（Ultra Fast Trading）极速策略引擎的核心接口和类。
 * UFT引擎是专门针对超高频或超低延时策略设计的，系统延迟在200纳秒以内。
 * 该引擎完全从WtCore项目剥离，不向应用层提供接口，全部在C++中进行开发实现。
 * 
 * 主要包含：
 * 1. UftStrategy类：UFT策略基类，定义了策略的基本接口和生命周期回调
 * 2. IUftStrategyFact类：UFT策略工厂接口，负责策略的创建、管理和销毁
 * 3. 相关函数指针类型定义：用于动态加载策略工厂
 * 
 * UFT策略特点：
 * - 支持Tick、委托队列、委托明细、逐笔成交等实时数据
 * - 提供完整的交易生命周期回调
 * - 支持参数动态更新和监控
 * - 专为极速交易场景优化
 */
#pragma once
#include <string>
#include <stdint.h>

#include "../Includes/WTSMarcos.h"

NS_WTP_BEGIN
class WTSVariant;        // 配置参数变体类
class IUftStraCtx;       // UFT策略上下文接口
class WTSTickData;       // Tick数据结构
class WTSOrdDtlData;     // 委托明细数据结构
class WTSOrdQueData;     // 委托队列数据结构
class WTSTransData;      // 逐笔成交数据结构
struct WTSBarStruct;     // K线数据结构
NS_WTP_END

USING_NS_WTP;

/**
 * @brief UFT极速策略基类
 * 
 * 该类定义了UFT极速策略引擎中所有策略必须实现的基本接口。
 * UFT策略适用于超高频交易场景，支持多种实时数据类型的回调，
 * 包括Tick、委托队列、委托明细、逐笔成交等，为策略提供最全面的市场信息。
 */
class UftStrategy
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符
	 */
	UftStrategy(const char* id) :_id(id){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~UftStrategy(){}

public:
	/**
	 * @brief 获取策略名称
	 * @return 策略名称字符串
	 */
	virtual const char* getName() = 0;

	/**
	 * @brief 获取所属执行器工厂名称
	 * @return 执行器工厂名称字符串
	 */
	virtual const char* getFactName() = 0;

	/**
	 * @brief 策略初始化
	 * @param cfg 配置参数对象
	 * @return 初始化是否成功
	 */
	virtual bool init(WTSVariant* cfg){ return true; }

	/**
	 * @brief 获取策略ID
	 * @return 策略ID字符串
	 */
	virtual const char* id() const { return _id.c_str(); }

	// 回调函数接口
	/**
	 * @brief 策略初始化完成回调
	 * @param ctx UFT策略上下文对象
	 * 
	 * 生命周期中只会回调一次，用于策略的初始化工作。
	 * 策略可以在这里进行数据准备、参数设置等初始化操作。
	 */
	virtual void on_init(IUftStraCtx* ctx) = 0;

	/**
	 * @brief 交易日开始回调
	 * @param ctx UFT策略上下文对象
	 * @param uTDate 交易日日期
	 * 
	 * 实盘时因为每天重启，所以会在on_init后调用一次。
	 * 回测时，生命周期中会调用多次。
	 * 如果有什么数据需要每天初始化，可以放到这个回调中处理，实盘就和回测保持一致了。
	 */
	virtual void on_session_begin(IUftStraCtx* ctx, uint32_t uTDate) {}

	/**
	 * @brief 交易日结束回调
	 * @param ctx UFT策略上下文对象
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_end(IUftStraCtx* ctx, uint32_t uTDate) {}

	/**
	 * @brief Tick数据推送回调
	 * @param ctx UFT策略上下文对象
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据
	 */
	virtual void on_tick(IUftStraCtx* ctx, const char* stdCode, WTSTickData* newTick) {}

	/**
	 * @brief 委托队列数据推送回调
	 * @param ctx UFT策略上下文对象
	 * @param stdCode 标准化合约代码
	 * @param newOrdQue 新的委托队列数据
	 */
	virtual void on_order_queue(IUftStraCtx* ctx, const char* stdCode, WTSOrdQueData* newOrdQue) {}

	/**
	 * @brief 逐笔委托数据推送回调
	 * @param ctx UFT策略上下文对象
	 * @param stdCode 标准化合约代码
	 * @param newOrdDtl 新的逐笔委托数据
	 */
	virtual void on_order_detail (IUftStraCtx* ctx, const char* stdCode, WTSOrdDtlData* newOrdDtl) {}

	/**
	 * @brief 逐笔成交数据推送回调
	 * @param ctx UFT策略上下文对象
	 * @param stdCode 标准化合约代码
	 * @param newTrans 新的逐笔成交数据
	 */
	virtual void on_transaction(IUftStraCtx* ctx, const char* stdCode, WTSTransData* newTrans) {}

	/**
	 * @brief K线闭合事件回调
	 * @param ctx UFT策略上下文对象
	 * @param stdCode 合约代码，格式如SHFE.rb2205
	 * @param period K线周期
	 * @param times 重采样倍数
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar(IUftStraCtx* ctx, const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) {}

	/**
	 * @brief 成交回报回调
	 * @param ctx UFT策略上下文对象
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码，格式如SHFE.rb2205
	 * @param isLong 是否做多
	 * @param offset 开平，0-开仓，1-平仓，2-平今
	 * @param vol 成交量
	 * @param price 成交价格
	 */
	virtual void on_trade(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double vol, double price) {}

	/**
	 * @brief 持仓同步回报回调
	 * @param ctx UFT策略上下文对象
	 * @param stdCode 合约代码，格式如SHFE.rb2205
	 * @param isLong 是否做多
	 * @param prevol 昨仓
	 * @param preavail 可用昨仓
	 * @param newvol 今仓
	 * @param newavail 可用今仓
	 * 
	 * 交易通道连接成功时，如果查到持仓，会推送一次。
	 * 如果没有查到，则不会推送，所以这个事件接口不适合放任何状态相关的东西。
	 */
	virtual void on_position(IUftStraCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) {}

	/**
	 * @brief 订单状态回调
	 * @param ctx UFT策略上下文对象
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码，格式如SHFE.rb2205
	 * @param isLong 是否做多
	 * @param offset 开平，0-开仓，1-平仓，2-平今
	 * @param totalQty 下单总数
	 * @param leftQty 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销
	 */
	virtual void on_order(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled) {}

	/**
	 * @brief 交易通道就绪事件回调
	 * @param ctx UFT策略上下文对象
	 */
	virtual void on_channel_ready(IUftStraCtx* ctx) {}

	/**
	 * @brief 交易通道断线事件回调
	 * @param ctx UFT策略上下文对象
	 */
	virtual void on_channel_lost(IUftStraCtx* ctx) {}

	/**
	 * @brief 下单回报回调
	 * @param localid 本地单号
	 * @param bSuccess 是否成功
	 * @param message 返回消息
	 * 
	 * 有些接口只有错单才会有回报，所以不能使用该接口作为下单是否成功的回报。
	 */
	virtual void on_entrust(uint32_t localid, bool bSuccess, const char* message) {}

	/**
	 * @brief 参数更新回调
	 * 
	 * 当策略参数发生变化时，会触发此回调。
	 * 策略可以在这里处理参数更新的逻辑。
	 */
	virtual void on_params_updated() {}

protected:
	std::string _id;    // 策略唯一标识符
};

//////////////////////////////////////////////////////////////////////////
//策略工厂接口

/**
 * @brief UFT策略枚举回调函数类型
 * @param factName 工厂名称
 * @param straName 策略名称
 * @param isLast 是否为最后一个策略
 */
typedef void(*FuncEnumUftStrategyCallback)(const char* factName, const char* straName, bool isLast);

/**
 * @brief UFT策略工厂接口
 * 
 * 该接口定义了UFT策略工厂的基本功能，包括策略的创建、管理和销毁。
 * 通过工厂模式实现UFT策略的动态加载和管理。
 */
class IUftStrategyFact
{
public:
	/**
	 * @brief 默认构造函数
	 */
	IUftStrategyFact(){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~IUftStrategyFact(){}

public:
	/**
	 * @brief 获取工厂名称
	 * @return 工厂名称字符串
	 */
	virtual const char* getName() = 0;

	/**
	 * @brief 枚举所有策略
	 * @param cb 枚举回调函数
	 */
	virtual void enumStrategy(FuncEnumUftStrategyCallback cb) = 0;

	/**
	 * @brief 根据名称创建策略实例
	 * @param name 策略名称
	 * @param id 策略ID
	 * @return 策略实例指针
	 */
	virtual UftStrategy* createStrategy(const char* name, const char* id) = 0;

	/**
	 * @brief 删除策略实例
	 * @param stra 要删除的策略实例
	 * @return 删除是否成功
	 */
	virtual bool deleteStrategy(UftStrategy* stra) = 0;
};

//创建策略工厂的函数指针类型
typedef IUftStrategyFact* (*FuncCreateUftStraFact)();
//删除策略工厂的函数指针类型
typedef void(*FuncDeleteUftStraFact)(IUftStrategyFact* &fact);
