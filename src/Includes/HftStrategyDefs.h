/*!
 * \file HftStrategyDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief HFT高频策略定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中HFT（High Frequency Trading）高频策略的核心接口和类结构。
 * 主要功能包括：
 * 1. 定义HFT策略基类，提供高频交易策略的生命周期管理接口
 * 2. 定义策略工厂接口，支持策略的动态创建和管理
 * 3. 提供高频交易特有的回调机制，处理委托队列、委托明细、逐笔成交等数据
 * 4. 支持策略的初始化和配置管理
 * 5. 定义策略工厂的创建和删除函数指针类型
 * 
 * 该类主要用于WonderTrader框架中的高频交易策略开发，为量化交易策略提供统一的接口规范。
 * 通过抽象基类设计，支持多种高频策略类型的扩展和插件化开发。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串支持
#include <stdint.h>  // 包含固定大小整数类型

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：WTS变体类型类
class IHftStraCtx;  // 前向声明：HFT策略上下文接口
class WTSTickData;  // 前向声明：WTS Tick数据结构类
class WTSOrdDtlData;  // 前向声明：WTS委托明细数据结构类
class WTSOrdQueData;  // 前向声明：WTS委托队列数据结构类
class WTSTransData;  // 前向声明：WTS逐笔成交数据结构类
struct WTSBarStruct;  // 前向声明：WTS K线结构体
NS_WTP_END  // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class HftStrategy
 * @brief HFT高频策略基类
 * 
 * 该类定义了HFT高频策略的基本接口和生命周期管理方法。
 * 所有具体的HFT策略都应该继承此类并实现相应的虚函数。
 * 提供了从策略初始化到高频交易执行的完整生命周期管理。
 * 
 * 主要特性：
 * - 支持策略的初始化和配置
 * - 提供交易日开始/结束的回调
 * - 支持高频交易特有的数据回调（委托队列、委托明细、逐笔成交）
 * - 支持Tick数据和K线数据的处理
 * - 提供完整的交易回报处理
 * - 支持交易通道状态监控
 */
class HftStrategy
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符
	 * 
	 * 初始化HFT策略对象，设置策略ID。
	 * 策略ID用于在系统中唯一标识该策略实例。
	 */
	HftStrategy(const char* id) :_id(id){}  // 初始化策略ID

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~HftStrategy(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 获取执行单元名称
	 * @return const char* 返回策略的执行单元名称
	 * 
	 * 该函数返回策略的执行单元名称，用于标识策略的执行环境。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getName() = 0;  // 纯虚函数：获取执行单元名称

	/**
	 * @brief 获取所属执行器工厂名称
	 * @return const char* 返回策略所属的执行器工厂名称
	 * 
	 * 该函数返回策略所属的执行器工厂名称，用于策略的管理和分类。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getFactName() = 0;  // 纯虚函数：获取工厂名称

	/**
	 * @brief 策略初始化
	 * @param cfg 策略配置参数
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数用于策略的初始化，接收配置参数进行策略设置。
	 * 默认实现返回true，子类可以重写此函数进行自定义初始化。
	 */
	virtual bool init(WTSVariant* cfg){ return true; }  // 虚函数：策略初始化，默认返回true

	/**
	 * @brief 获取策略ID
	 * @return const char* 返回策略的唯一标识符
	 * 
	 * 该函数返回策略的唯一标识符，用于在系统中识别策略实例。
	 */
	virtual const char* id() const { return _id.c_str(); }  // 返回策略ID

	//回调函数

	/**
	 * @brief 策略初始化完成回调
	 * @param ctx HFT策略上下文对象
	 * 
	 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_init(IHftStraCtx* ctx) = 0;  // 纯虚函数：策略初始化完成回调

	/**
	 * @brief 交易日开始回调
	 * @param ctx HFT策略上下文对象
	 * @param uTDate 交易日日期
	 * 
	 * 该函数在每个交易日开始时被调用，用于执行交易日开始时的准备工作。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_session_begin(IHftStraCtx* ctx, uint32_t uTDate) {}  // 虚函数：交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * @param ctx HFT策略上下文对象
	 * @param uTDate 交易日日期
	 * 
	 * 该函数在每个交易日结束时被调用，用于执行交易日结束时的清理工作。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_session_end(IHftStraCtx* ctx, uint32_t uTDate) {}  // 虚函数：交易日结束回调

	/**
	 * @brief Tick数据回调
	 * @param ctx HFT策略上下文对象
	 * @param code 合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * 默认实现为空，子类可以重写此函数实现Tick数据处理逻辑。
	 */
	virtual void on_tick(IHftStraCtx* ctx, const char* code, WTSTickData* newTick) {}  // 虚函数：Tick数据回调

	/**
	 * @brief 委托队列回调
	 * @param ctx HFT策略上下文对象
	 * @param code 合约代码
	 * @param newOrdQue 新的委托队列数据
	 * 
	 * 该函数在接收到新的委托队列数据时被调用，用于分析市场深度和委托分布。
	 * 默认实现为空，子类可以重写此函数实现委托队列分析逻辑。
	 */
	virtual void on_order_queue(IHftStraCtx* ctx, const char* code, WTSOrdQueData* newOrdQue) {}  // 虚函数：委托队列回调

	/**
	 * @brief 委托明细回调
	 * @param ctx HFT策略上下文对象
	 * @param code 合约代码
	 * @param newOrdDtl 新的委托明细数据
	 * 
	 * 该函数在接收到新的委托明细数据时被调用，用于分析委托的详细信息。
	 * 默认实现为空，子类可以重写此函数实现委托明细分析逻辑。
	 */
	virtual void on_order_detail (IHftStraCtx* ctx, const char* code, WTSOrdDtlData* newOrdDtl) {}  // 虚函数：委托明细回调

	/**
	 * @brief 逐笔成交回调
	 * @param ctx HFT策略上下文对象
	 * @param code 合约代码
	 * @param newTrans 新的逐笔成交数据
	 * 
	 * 该函数在接收到新的逐笔成交数据时被调用，用于分析成交的详细信息。
	 * 默认实现为空，子类可以重写此函数实现逐笔成交分析逻辑。
	 */
	virtual void on_transaction(IHftStraCtx* ctx, const char* code, WTSTransData* newTrans) {}  // 虚函数：逐笔成交回调

	/**
	 * @brief K线闭合回调
	 * @param ctx HFT策略上下文对象
	 * @param code 合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据
	 * 
	 * 该函数在K线闭合时被调用，用于处理K线数据。
	 * 默认实现为空，子类可以重写此函数实现K线数据处理逻辑。
	 */
	virtual void on_bar(IHftStraCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) {}  // 虚函数：K线闭合回调

	/**
	 * @brief 成交回报回调
	 * @param ctx HFT策略上下文对象
	 * @param localid 本地订单号
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入，true为买入，false为卖出
	 * @param vol 成交数量
	 * @param price 成交价格
	 * @param userTag 用户标签
	 * 
	 * 该函数在订单成交时被调用，用于处理成交回报信息。
	 * 默认实现为空，子类可以重写此函数实现成交回报处理逻辑。
	 */
	virtual void on_trade(IHftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag) {}  // 虚函数：成交回报回调

	/**
	 * @brief 持仓回报回调
	 * @param ctx HFT策略上下文对象
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为多头持仓，true为多头，false为空头
	 * @param prevol 之前持仓数量
	 * @param preavail 之前可用持仓数量
	 * @param newvol 当前持仓数量
	 * @param newavail 当前可用持仓数量
	 * 
	 * 该函数在持仓发生变化时被调用，用于处理持仓回报信息。
	 * 只有在刚启动的时候，交易接口就绪之前会触发该回调。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_position(IHftStraCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) {}  // 虚函数：持仓回报回调

	/**
	 * @brief 订单回报回调
	 * @param ctx HFT策略上下文对象
	 * @param localid 本地订单号
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入，true为买入，false为卖出
	 * @param totalQty 总委托数量
	 * @param leftQty 剩余委托数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销
	 * @param userTag 用户标签
	 * 
	 * 该函数在订单状态变化时被调用，用于处理订单回报信息。
	 * 默认实现为空，子类可以重写此函数实现订单回报处理逻辑。
	 */
	virtual void on_order(IHftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag) {}  // 虚函数：订单回报回调

	/**
	 * @brief 交易通道就绪回调
	 * @param ctx HFT策略上下文对象
	 * 
	 * 该函数在交易通道初始化完成并准备就绪时被调用。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_channel_ready(IHftStraCtx* ctx) {}  // 虚函数：交易通道就绪回调

	/**
	 * @brief 交易通道断开回调
	 * @param ctx HFT策略上下文对象
	 * 
	 * 该函数在交易通道连接断开时被调用，用于处理连接异常情况。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_channel_lost(IHftStraCtx* ctx) {}  // 虚函数：交易通道断开回调

	/**
	 * @brief 委托回报回调
	 * @param localid 本地订单号
	 * @param bSuccess 委托是否成功
	 * @param message 委托结果消息
	 * @param userTag 用户标签
	 * 
	 * 该函数在委托指令发送后返回结果时被调用，用于处理委托回报信息。
	 * 默认实现为空，子类可以重写此函数实现委托回报处理逻辑。
	 */
	virtual void on_entrust(uint32_t localid, bool bSuccess, const char* message, const char* userTag) {}  // 虚函数：委托回报回调

protected:
	std::string _id;  // 策略唯一标识符，存储策略的ID字符串
};

//////////////////////////////////////////////////////////////////////////
//策略工厂接口
/**
 * @typedef FuncEnumHftStrategyCallback
 * @brief HFT策略枚举回调函数类型
 * 
 * 该类型定义了HFT策略枚举时的回调函数签名。
 * 用于在枚举HFT策略时通知调用者每个策略的信息。
 * 
 * @param factName 工厂名称
 * @param straName 策略名称
 * @param isLast 是否为最后一个策略
 */
typedef void(*FuncEnumHftStrategyCallback)(const char* factName, const char* straName, bool isLast);  // HFT策略枚举回调函数类型定义

/**
 * @class IHftStrategyFact
 * @brief HFT策略工厂接口
 * 
 * 该类定义了HFT策略工厂的基本接口，负责策略的创建、删除和管理。
 * 通过工厂模式实现策略的动态加载和管理，支持插件化开发。
 * 
 * 主要特性：
 * - 支持策略的枚举和查询
 * - 提供策略的创建和删除功能
 * - 支持策略工厂的命名和管理
 * - 实现策略的生命周期管理
 */
class IHftStrategyFact
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化HFT策略工厂对象，不执行任何特殊操作。
	 */
	IHftStrategyFact(){}  // 默认构造函数

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IHftStrategyFact(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 获取工厂名称
	 * @return const char* 返回策略工厂的名称
	 * 
	 * 该函数返回策略工厂的名称，用于标识和管理不同的策略工厂。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getName() = 0;  // 纯虚函数：获取工厂名称

	/**
	 * @brief 枚举策略
	 * @param cb 枚举回调函数
	 * 
	 * 该函数枚举工厂中所有可用的HFT策略，通过回调函数通知调用者。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void enumStrategy(FuncEnumHftStrategyCallback cb) = 0;  // 纯虚函数：枚举策略

	/**
	 * @brief 根据名称创建HFT策略
	 * @param name 策略名称
	 * @param id 策略ID
	 * @return HftStrategy* 返回创建的策略对象指针
	 * 
	 * 该函数根据策略名称创建对应的HFT策略对象实例。
	 * 纯虚函数，子类必须实现。
	 */
	virtual HftStrategy* createStrategy(const char* name, const char* id) = 0;  // 纯虚函数：根据名称创建策略

	/**
	 * @brief 删除策略
	 * @param stra 要删除的策略对象指针
	 * @return bool 删除成功返回true，失败返回false
	 * 
	 * 该函数删除指定的HFT策略对象，释放相关资源。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool deleteStrategy(HftStrategy* stra) = 0;  // 纯虚函数：删除策略
};

/**
 * @typedef FuncCreateHftStraFact
 * @brief 创建HFT策略工厂函数指针类型
 * 
 * 该类型定义了创建HFT策略工厂的函数指针签名。
 * 用于动态加载HFT策略工厂插件。
 */
typedef IHftStrategyFact* (*FuncCreateHftStraFact)();  // 创建HFT策略工厂函数指针类型定义

/**
 * @typedef FuncDeleteHftStraFact
 * @brief 删除HFT策略工厂函数指针类型
 * 
 * 该类型定义了删除HFT策略工厂的函数指针签名。
 * 用于动态卸载HFT策略工厂插件。
 */
typedef void(*FuncDeleteHftStraFact)(IHftStrategyFact* &fact);  // 删除HFT策略工厂函数指针类型定义
