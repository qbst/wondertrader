/*!
 * \file WtUftStraDemo.h
 * \project	WonderTrader
 * 
 * \brief SimpleUft简单极速交易策略示例类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中SimpleUft简单极速交易策略示例类的接口声明。
 * 这是一个UFT策略的示例实现，展示了如何开发一个基于Tick数据的极速交易策略。
 * 
 * 主要功能包括：
 * 1. 实现UftStrategy基类接口，提供策略的基本功能
 * 2. 实现基于理论价格计算的交易信号生成逻辑
 * 3. 支持订单管理和超时撤销机制
 * 4. 处理交易通道状态变化，确保交易安全
 * 5. 支持参数动态更新和监控
 * 6. 提供完整的交易回报处理，包括成交、持仓、订单状态等
 * 
 * 策略原理：
 * SimpleUft策略通过计算理论价格（基于买卖盘口的加权平均）与最新价的比较来生成交易信号：
 * - 理论价格 = (买一价*卖一量 + 卖一价*买一量) / (买一量 + 卖一量)
 * - 当理论价格 > 最新价时，产生正向信号（做多）
 * - 当理论价格 < 最新价时，产生反向信号（做空或平多）
 * - 订单超时未成交会自动撤销
 * 
 * UFT策略特点：
 * - 专为极速交易设计，系统延迟在200纳秒以内
 * - 支持参数动态更新，无需重启策略
 * - 使用SpinMutex自旋锁，提高并发性能
 * - 完全在C++中实现，不依赖应用层接口
 * 
 * 该策略主要用于演示UFT策略的开发模式，可以作为开发其他极速策略的参考模板。
 */
#pragma once  // 防止头文件重复包含
#include <unordered_set>  // 包含无序集合容器，用于存储订单ID集合
#include <memory>  // 包含智能指针支持
#include <thread>  // 包含线程支持（虽然当前未使用，但保留以备将来扩展）
#include <mutex>  // 包含互斥量（虽然当前未使用，但保留以备将来扩展）

#include "../Includes/UftStrategyDefs.h"  // 包含UFT策略定义头文件，提供UftStrategy基类和IUftStraCtx接口
#include "../Share/SpinMutex.hpp"  // 包含自旋锁互斥量，用于高性能的订单集合保护

/**
 * @class WtUftStraDemo
 * @brief SimpleUft简单极速交易策略示例类
 * 
 * 该类实现了SimpleUft简单极速交易策略，继承自UftStrategy基类。
 * 这是一个基于Tick数据的极速交易策略示例，展示了UFT策略的基本开发模式。
 * 
 * 策略特点：
 * - 基于理论价格计算：通过买卖盘口数据计算理论价格，与最新价比较生成信号
 * - 订单管理：维护订单ID集合，支持订单状态跟踪和超时撤销
 * - 交易通道监控：监控交易通道状态，确保交易安全
 * - 频率控制：支持设置交易频率限制，避免过度交易
 * - 超时撤销：订单超时未成交自动撤销，避免订单滞留
 * - 参数动态更新：支持参数动态更新，无需重启策略
 * 
 * 主要功能：
 * - 策略初始化：从配置文件加载策略参数
 * - 交易信号生成：根据理论价格与最新价的比较生成交易信号
 * - 订单管理：跟踪订单状态，处理订单回报
 * - 交易回报处理：处理成交、持仓、订单状态等回报
 * - 通道状态处理：处理交易通道就绪和丢失事件
 * - 参数更新处理：处理参数动态更新事件
 */
class WtUftStraDemo : public UftStrategy  // 继承自UFT策略基类
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
	 * 
	 * 初始化SimpleUft策略对象，设置策略ID。
	 * 策略ID用于在系统中唯一标识该策略实例，通常由策略工厂在创建策略时传入。
	 */
	WtUftStraDemo(const char* id);  // 构造函数声明

	/**
	 * @brief 析构函数
	 * 
	 * 析构函数在对象销毁时自动调用，释放策略对象占用的资源。
	 * 主要释放_last_tick指针指向的Tick数据对象。
	 */
	~WtUftStraDemo();  // 析构函数声明

private:
	/**
	 * @brief 检查订单状态
	 * 
	 * 该函数检查当前未完成的订单，如果订单超时未成交则自动撤销。
	 * 订单超时时间由_secs参数控制，从_last_entry_time开始计算。
	 * 
	 * @note 该函数是私有函数，仅在策略内部调用
	 */
	void	check_orders();  // 检查订单状态函数声明

public:
	/**
	 * @brief 获取策略名称
	 * @return const char* 返回策略的名称字符串
	 * 
	 * 该函数返回策略的名称，用于标识策略类型。
	 * 返回值为"UftDemoStrategy"，表示这是UFT策略示例。
	 * 
	 * @note 该函数重写了UftStrategy基类的纯虚函数
	 */
	virtual const char* getName() override;  // 重写基类函数：获取策略名称

	/**
	 * @brief 获取所属策略工厂名称
	 * @return const char* 返回策略所属的工厂名称字符串
	 * 
	 * 该函数返回策略所属的策略工厂名称，用于标识策略的来源工厂。
	 * 返回值为"WtUftStraFact"，表示该策略由WtUftStraFact工厂创建。
	 * 
	 * @note 该函数重写了UftStrategy基类的纯虚函数
	 */
	virtual const char* getFactName() override;  // 重写基类函数：获取工厂名称

	/**
	 * @brief 策略初始化
	 * @param cfg 策略配置参数，包含策略运行所需的所有参数
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数从配置参数中加载策略运行所需的参数，包括：
	 * - code: 合约代码，策略交易的合约（必需参数）
	 * - second: 订单超时时间（秒），超过此时间未成交的订单会被撤销（必需参数）
	 * - freq: 交易频率限制（毫秒），两次交易之间的最小时间间隔（必需参数）
	 * - offset: 价格偏移跳数，下单价格相对于最新价的偏移（必需参数）
	 * - lots: 下单数量，每次交易的手数（必需参数）
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual bool init(WTSVariant* cfg) override;  // 重写基类函数：策略初始化

	/**
	 * @brief 策略初始化完成回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * 
	 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
	 * 主要功能包括：
	 * 1. 注册参数监控，支持参数动态更新
	 * 2. 预加载历史K线数据，确保数据可用
	 * 3. 订阅合约的Tick数据，用于接收实时行情
	 * 4. 保存策略上下文指针，供后续使用
	 * 
	 * @note 该函数重写了UftStrategy基类的纯虚函数
	 */
	virtual void on_init(IUftStraCtx* ctx) override;  // 重写基类函数：初始化完成回调

	/**
	 * @brief Tick数据处理回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * @param code 标准合约代码，触发Tick数据的合约
	 * @param newTick 新的Tick数据，包含最新的价格和成交量信息
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * 主要逻辑：
	 * 1. 检查Tick数据是否属于策略关注的合约
	 * 2. 如果有未完成的订单，先检查订单状态
	 * 3. 如果交易通道就绪且没有未完成订单，执行策略计算
	 * 4. 根据理论价格与最新价的比较生成交易信号
	 * 5. 执行交易操作
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_tick(IUftStraCtx* ctx, const char* code, WTSTickData* newTick) override;  // 重写基类函数：Tick数据处理回调

	/**
	 * @brief K线闭合回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * @param code 标准合约代码，触发K线数据的合约
	 * @param period K线周期，如"m1"、"m5"等
	 * @param times K线倍数
	 * @param newBar 新的K线数据
	 * 
	 * 该函数在K线闭合时被调用，用于处理K线数据。
	 * SimpleUft策略主要基于Tick数据，因此K线数据处理为空实现。
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_bar(IUftStraCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) override;  // 重写基类函数：K线闭合回调

	/**
	 * @brief 成交回报回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * @param localid 本地订单ID，用于标识订单
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为做多，true表示做多，false表示做空
	 * @param offset 开平标志，0-开仓，1-平仓，2-平今
	 * @param qty 成交数量
	 * @param price 成交价格
	 * 
	 * 该函数在订单成交时被调用，用于处理成交回报。
	 * SimpleUft策略当前实现为空，可以根据需要扩展处理逻辑。
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_trade(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double qty, double price) override;  // 重写基类函数：成交回报回调

	/**
	 * @brief 持仓回报回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为多头，true表示多头，false表示空头
	 * @param prevol 变化前的持仓量（昨仓）
	 * @param preavail 变化前的可用持仓量（可用昨仓）
	 * @param newvol 变化后的持仓量（今仓）
	 * @param newavail 变化后的可用持仓量（可用今仓）
	 * 
	 * 该函数在持仓发生变化时被调用，用于处理持仓回报。
	 * 主要功能：
	 * 1. 记录昨仓信息，用于后续使用
	 * 2. 记录日志，输出持仓变化信息
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_position(IUftStraCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) override;  // 重写基类函数：持仓回报回调

	/**
	 * @brief 订单回报回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * @param localid 本地订单ID，用于标识订单
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为做多，true表示做多，false表示做空
	 * @param offset 开平标志，0-开仓，1-平仓，2-平今
	 * @param totalQty 订单总数量
	 * @param leftQty 订单剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销，true表示已撤销，false表示未撤销
	 * 
	 * 该函数在订单状态发生变化时被调用，用于处理订单回报。
	 * 主要功能：
	 * 1. 检查订单是否属于策略管理的订单
	 * 2. 如果订单已撤销或全部成交，从订单集合中移除
	 * 3. 更新撤销订单计数器
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_order(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled) override;  // 重写基类函数：订单回报回调

	/**
	 * @brief 交易通道就绪回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * 
	 * 该函数在交易通道就绪时被调用，表示可以开始交易。
	 * 主要功能：
	 * 1. 检查是否有不在管理中的未完成订单
	 * 2. 如果有，撤销这些订单并加入管理
	 * 3. 设置通道就绪标志，允许策略执行交易
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_channel_ready(IUftStraCtx* ctx) override;  // 重写基类函数：交易通道就绪回调

	/**
	 * @brief 交易通道丢失回调
	 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
	 * 
	 * 该函数在交易通道丢失时被调用，表示无法进行交易。
	 * 主要功能：
	 * 1. 设置通道丢失标志，禁止策略执行交易
	 * 2. 等待通道恢复后再继续交易
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_channel_lost(IUftStraCtx* ctx) override;  // 重写基类函数：交易通道丢失回调

	/**
	 * @brief 委托回报回调
	 * @param localid 本地订单ID，用于标识订单
	 * @param bSuccess 委托是否成功，true表示成功，false表示失败
	 * @param message 委托结果消息
	 * 
	 * 该函数在委托回报时被调用，用于处理委托结果。
	 * 主要功能：
	 * 1. 如果委托失败，从订单集合中移除订单ID
	 * 2. 避免管理失败的订单
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_entrust(uint32_t localid, bool bSuccess, const char* message) override;  // 重写基类函数：委托回报回调

	/**
	 * @brief 参数更新回调
	 * 
	 * 该函数在策略参数发生变化时被调用，用于处理参数更新。
	 * 主要功能：
	 * 1. 从上下文读取更新后的参数值
	 * 2. 更新策略内部的参数变量
	 * 3. 记录日志，输出参数更新信息
	 * 
	 * UFT策略支持参数动态更新，无需重启策略即可生效。
	 * 
	 * @note 该函数重写了UftStrategy基类的虚函数
	 */
	virtual void on_params_updated() override;  // 重写基类函数：参数更新回调

private:
	WTSTickData*	_last_tick;  // 最后接收到的Tick数据指针，用于保存Tick数据（当前未使用，保留以备将来扩展）
	IUftStraCtx*	_ctx;  // UFT策略上下文对象指针，用于访问数据接口和交易接口
	std::string		_code;  // 合约代码，策略交易的合约代码
	uint32_t		_secs;  // 订单超时时间（秒），超过此时间未成交的订单会被撤销
	uint32_t		_freq;  // 交易频率限制（毫秒），两次交易之间的最小时间间隔，用于控制交易频率
	int32_t			_offset;  // 价格偏移跳数，下单价格相对于最新价的偏移跳数（正数表示向上偏移，负数表示向下偏移）
	double			_lots;  // 下单数量，每次交易的手数
	double			_prev;  // 昨仓数量，用于记录昨日的持仓量

	typedef std::unordered_set<uint32_t> IDSet;  // 定义订单ID集合类型，用于存储策略管理的订单ID
	IDSet			_orders;  // 订单ID集合，存储当前策略管理的所有订单ID
	SpinMutex		_mtx_ords;  // 订单集合自旋锁互斥量，用于多线程环境下保护订单集合的访问（使用自旋锁提高性能）

	uint64_t		_last_entry_time;  // 最后一次交易时间（微秒时间戳），用于计算交易频率限制

	bool			_channel_ready;  // 交易通道就绪标志，true表示通道就绪可以交易，false表示通道未就绪不能交易
	uint32_t		_last_calc_time;  // 最后一次计算时间（分钟），用于控制计算频率
	uint32_t		_cancel_cnt;  // 撤销订单计数器，记录撤销的订单数量（用于统计和调试）
};

