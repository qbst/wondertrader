/*!
 * \file WtStraDualThrust.h
 * \project	WonderTrader
 * 
 * \brief DualThrust双突破策略类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中DualThrust双突破策略类的接口声明。
 * DualThrust是一种经典的量化交易策略，通过计算价格波动范围来确定交易信号。
 * 
 * 主要功能包括：
 * 1. 实现CTA策略基类接口，提供策略的基本功能
 * 2. 实现DualThrust策略逻辑，包括上下轨计算和交易信号生成
 * 3. 支持策略参数配置，包括K1、K2系数、回看天数等
 * 4. 处理主力合约换月，自动将持仓转移到新主力合约
 * 5. 支持股票和期货两种交易模式，股票不支持做空
 * 6. 提供策略生命周期回调，包括初始化、交易日开始、定时调度等
 * 
 * 策略原理：
 * DualThrust策略通过计算过去N天的最高价、最低价和收盘价，确定上下突破轨道。
 * 当价格突破上轨时做多，突破下轨时做空（期货）或平多（股票）。
 * 策略公式：
 * - Range = max(HH-LC, HC-LL)
 * - 上轨 = 开盘价 + K1 * Range
 * - 下轨 = 开盘价 - K2 * Range
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/CtaStrategyDefs.h"  // 包含CTA策略定义头文件，提供CtaStrategy基类和ICtaStraCtx接口

/**
 * @class WtStraDualThrust
 * @brief DualThrust双突破策略类
 * 
 * 该类实现了DualThrust双突破交易策略，继承自CtaStrategy基类。
 * DualThrust是一种基于价格突破的交易策略，通过计算价格波动范围来确定交易信号。
 * 
 * 策略特点：
 * - 趋势跟踪：通过突破上下轨捕捉趋势
 * - 参数可调：支持自定义K1、K2系数和回看天数
 * - 多周期支持：支持不同周期的K线数据
 * - 自动换月：支持期货主力合约自动换月
 * - 股票适配：支持股票和期货两种模式，股票不支持做空
 * 
 * 主要功能：
 * - 策略初始化：从配置文件加载策略参数
 * - 上下轨计算：根据历史K线数据计算突破轨道
 * - 交易信号生成：根据价格突破情况生成买卖信号
 * - 持仓管理：管理多空持仓，支持自动换月
 * - 图表支持：在图表上显示上下轨和交易标记
 */
class WtStraDualThrust : public CtaStrategy  // 继承自CTA策略基类
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
	 * 
	 * 初始化DualThrust策略对象，设置策略ID。
	 * 策略ID用于在系统中唯一标识该策略实例，通常由策略工厂在创建策略时传入。
	 */
	WtStraDualThrust(const char* id);  // 构造函数声明

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 在对象销毁时自动调用，释放策略对象占用的资源。
	 */
	virtual ~WtStraDualThrust();  // 虚析构函数声明，支持多态销毁

public:
	/**
	 * @brief 获取所属策略工厂名称
	 * @return const char* 返回策略所属的工厂名称字符串
	 * 
	 * 该函数返回策略所属的策略工厂名称，用于标识策略的来源工厂。
	 * 返回值为"WtCtaStraFact"，表示该策略由WtCtaStraFact工厂创建。
	 * 
	 * @note 该函数重写了CtaStrategy基类的纯虚函数
	 */
	virtual const char* getFactName() override;  // 重写基类函数：获取工厂名称

	/**
	 * @brief 获取策略名称
	 * @return const char* 返回策略的名称字符串
	 * 
	 * 该函数返回策略的名称，用于标识策略类型。
	 * 返回值为"DualThrust"，表示这是DualThrust双突破策略。
	 * 
	 * @note 该函数重写了CtaStrategy基类的纯虚函数
	 */
	virtual const char* getName() override;  // 重写基类函数：获取策略名称

	/**
	 * @brief 策略初始化
	 * @param cfg 策略配置参数，包含策略运行所需的所有参数
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数从配置参数中加载策略运行所需的参数，包括：
	 * - days: 回看天数，用于计算价格波动范围
	 * - k1: 上轨系数，用于计算上突破轨道
	 * - k2: 下轨系数，用于计算下突破轨道
	 * - period: K线周期，如"m1"、"m5"、"d1"等
	 * - count: K线条数，用于获取历史K线数据
	 * - code: 合约代码，策略交易的合约
	 * - stock: 是否为股票，true表示股票模式（不支持做空），false表示期货模式
	 * 
	 * @note 该函数重写了CtaStrategy基类的虚函数
	 * @note 如果配置参数为空或缺少必要参数，函数返回false
	 */
	virtual bool init(WTSVariant* cfg) override;  // 重写基类函数：策略初始化

	/**
	 * @brief 策略调度执行入口
	 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
	 * @param curDate 当前日期，格式为YYYYMMDD
	 * @param curTime 当前时间，格式为HHMMSS
	 * 
	 * 该函数是策略的主体逻辑执行入口，在策略调度时被调用。
	 * 主要功能包括：
	 * 1. 获取历史K线数据，计算价格波动范围
	 * 2. 计算上下突破轨道（上轨和下轨）
	 * 3. 根据当前价格与上下轨的关系生成交易信号
	 * 4. 执行交易操作（开多、开空、平多、平空）
	 * 5. 在图表上显示上下轨和交易标记
	 * 
	 * 交易逻辑：
	 * - 无持仓时：价格突破上轨做多，突破下轨做空（仅期货）
	 * - 持多仓时：价格跌破下轨平多
	 * - 持空仓时：价格突破上轨平空（仅期货）
	 * 
	 * @note 该函数重写了CtaStrategy基类的虚函数
	 */
	virtual void on_schedule(ICtaStraCtx* ctx, uint32_t curDate, uint32_t curTime) override;  // 重写基类函数：策略调度执行

	/**
	 * @brief 策略初始化完成回调
	 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
	 * 
	 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
	 * 主要功能包括：
	 * 1. 订阅合约的Tick数据
	 * 2. 预加载历史K线数据，确保数据可用
	 * 3. 注册策略图表，用于显示K线和指标
	 * 4. 注册指标和指标线，用于显示上下轨
	 * 
	 * @note 该函数重写了CtaStrategy基类的虚函数
	 */
	virtual void on_init(ICtaStraCtx* ctx) override;  // 重写基类函数：初始化完成回调

	/**
	 * @brief Tick数据处理回调
	 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
	 * @param stdCode 标准合约代码，触发Tick数据的合约
	 * @param newTick 新的Tick数据，包含最新的价格和成交量信息
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * DualThrust策略主要基于K线数据，因此Tick数据处理为空实现。
	 * 如果将来需要基于Tick数据做更精细的交易决策，可以在此函数中实现。
	 * 
	 * @note 该函数重写了CtaStrategy基类的虚函数
	 */
	virtual void on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick) override;  // 重写基类函数：Tick数据处理回调

	/**
	 * @brief 交易日开始回调
	 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
	 * @param uTDate 交易日日期，格式为YYYYMMDD
	 * 
	 * 该函数在每个交易日开始时被调用，用于执行交易日开始时的准备工作。
	 * 主要功能包括：
	 * 1. 检查主力合约是否换月
	 * 2. 如果主力合约换月，自动将持仓从旧主力转移到新主力
	 * 3. 更新当前主力合约代码
	 * 
	 * 换月逻辑：
	 * - 获取当前交易日的主力合约代码
	 * - 如果主力合约发生变化，且旧主力有持仓
	 * - 将旧主力的持仓平仓，在新主力上开相同方向的持仓
	 * 
	 * @note 该函数重写了CtaStrategy基类的虚函数
	 */
	virtual void on_session_begin(ICtaStraCtx* ctx, uint32_t uTDate) override;  // 重写基类函数：交易日开始回调

private:
	// 策略指标参数
	double		_k1;  // 上轨系数，用于计算上突破轨道，通常取值范围为0.5-1.5
	double		_k2;  // 下轨系数，用于计算下突破轨道，通常取值范围为0.5-1.5
	uint32_t	_days;  // 回看天数，用于计算价格波动范围，通常取值为5-20

	std::string _moncode;  // 当前主力合约代码，用于期货合约的自动换月处理

	// 数据周期相关参数
	std::string _period;  // K线周期，如"m1"（1分钟）、"m5"（5分钟）、"d1"（日线）等
	uint32_t	_count;  // K线条数，用于获取历史K线数据，通常取值为_days的2-3倍

	// 合约相关参数
	std::string _code;  // 合约代码，策略交易的合约代码（可能是连续合约代码）

	bool		_isstk;  // 是否为股票标志，true表示股票模式（不支持做空），false表示期货模式（支持做空）
};

