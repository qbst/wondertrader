/*!
 * \file WtStraDtSel.h
 * \project	WonderTrader
 * 
 * \brief DualThrust选股策略类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中DualThrust选股策略类的接口声明。
 * DualThrust选股策略是基于DualThrust双突破算法的多标的选股策略。
 * 
 * 主要功能包括：
 * 1. 实现SelStrategy基类接口，提供选股策略的基本功能
 * 2. 实现DualThrust选股逻辑，支持多个标的的同时选股和仓位管理
 * 3. 支持策略参数配置，包括K1、K2系数、回看天数等
 * 4. 支持股票和期货两种交易模式，股票不支持做空
 * 5. 提供策略生命周期回调，包括初始化、定时调度等
 * 6. 支持多标的的Tick和K线数据处理
 * 
 * 策略原理：
 * DualThrust选股策略通过计算每个标的过去N天的价格波动范围来确定交易信号：
 * 1. 计算过去N天（排除最后1根K线）的最高价HH和最低价LL
 * 2. 计算过去N天（排除最后1根K线）的收盘价最高值HC和最低值LC
 * 3. 计算价格波动范围：Range = max(HH-LC, HC-LL)
 * 4. 计算上轨：上轨 = 开盘价 + K1 * Range
 * 5. 计算下轨：下轨 = 开盘价 - K2 * Range
 * 6. 根据当前价格与上下轨的关系生成交易信号
 * 7. 对多个标的分别执行选股逻辑，独立管理每个标的的仓位
 * 
 * SEL策略特点：
 * - 适用于多标的、计算量大的策略
 * - 采用时间驱动模式，定时触发重算并调整多标的的目标仓位
 * - 可以同时管理多个标的的持仓，实现组合投资
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/SelStrategyDefs.h"  // 包含SEL策略定义头文件，提供SelStrategy基类和ISelStraCtx接口

#include <unordered_set>  // 包含无序集合容器，用于存储多个合约代码

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class WtStraDtSel
 * @brief DualThrust选股策略类
 * 
 * 该类实现了DualThrust选股策略，继承自SelStrategy基类。
 * DualThrust选股策略是基于DualThrust双突破算法的多标的选股策略。
 * 
 * 策略特点：
 * - 多标的支持：可以同时管理多个标的的选股和仓位
 * - 趋势跟踪：通过突破上下轨捕捉趋势
 * - 参数可调：支持自定义K1、K2系数和回看天数
 * - 多周期支持：支持不同周期的K线数据
 * - 股票适配：支持股票和期货两种模式，股票不支持做空
 * 
 * 主要功能：
 * - 策略初始化：从配置文件加载策略参数和标的列表
 * - 多标的选股：对每个标的分别执行DualThrust算法
 * - 仓位管理：独立管理每个标的的持仓
 * - 交易信号生成：根据价格突破情况生成买卖信号
 */
class WtStraDtSel : public SelStrategy  // 继承自SEL策略基类
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
	 * 
	 * 初始化DualThrust选股策略对象，设置策略ID。
	 * 策略ID用于在系统中唯一标识该策略实例，通常由策略工厂在创建策略时传入。
	 */
	WtStraDtSel(const char* id);  // 构造函数声明

	/**
	 * @brief 析构函数
	 * 
	 * 析构函数在对象销毁时自动调用，释放策略对象占用的资源。
	 */
	~WtStraDtSel();  // 析构函数声明

public:
	/**
	 * @brief 获取策略名称
	 * @return const char* 返回策略的名称字符串
	 * 
	 * 该函数返回策略的名称，用于标识策略类型。
	 * 返回值为"DualThrustSelection"，表示这是DualThrust选股策略。
	 * 
	 * @note 该函数重写了SelStrategy基类的纯虚函数
	 */
	virtual const char* getName() override;  // 重写基类函数：获取策略名称

	/**
	 * @brief 获取所属策略工厂名称
	 * @return const char* 返回策略所属的工厂名称字符串
	 * 
	 * 该函数返回策略所属的策略工厂名称，用于标识策略的来源工厂。
	 * 返回值为"WtSelStraFact"，表示该策略由WtSelStraFact工厂创建。
	 * 
	 * @note 该函数重写了SelStrategy基类的纯虚函数
	 */
	virtual const char* getFactName() override;  // 重写基类函数：获取工厂名称

	/**
	 * @brief 策略初始化
	 * @param cfg 策略配置参数，包含策略运行所需的所有参数
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数从配置参数中加载策略运行所需的参数，包括：
	 * - days: 回看天数，用于计算价格波动范围（必需参数）
	 * - k1: 上轨系数，用于计算上突破轨道（必需参数）
	 * - k2: 下轨系数，用于计算下突破轨道（必需参数）
	 * - period: K线周期，如"m1"、"m5"、"d1"等（必需参数）
	 * - count: K线条数，用于获取历史K线数据（必需参数）
	 * - codes: 合约代码列表，逗号分隔的多个合约代码（必需参数）
	 * - stock: 是否为股票，true表示股票模式（不支持做空），false表示期货模式（可选参数，默认为false）
	 * 
	 * @note 该函数重写了SelStrategy基类的虚函数
	 * @note 如果配置参数为空或缺少必要参数，函数返回false
	 */
	virtual bool init(WTSVariant* cfg) override;  // 重写基类函数：策略初始化

	/**
	 * @brief 策略初始化完成回调
	 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
	 * 
	 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
	 * 主要功能包括：
	 * 1. 订阅所有标的的Tick数据，用于接收实时行情
	 * 
	 * @note 该函数重写了SelStrategy基类的虚函数
	 */
	virtual void on_init(ISelStraCtx* ctx) override;  // 重写基类函数：初始化完成回调

	/**
	 * @brief 策略调度执行入口
	 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
	 * @param uDate 当前日期，格式为YYYYMMDD
	 * @param uTime 当前时间，格式为HHMMSS
	 * 
	 * 该函数是策略的主体逻辑执行入口，在策略调度时被调用。
	 * 主要功能包括：
	 * 1. 遍历所有标的，对每个标的分别执行选股逻辑
	 * 2. 检查标的是否在交易时间内
	 * 3. 获取历史K线数据，计算价格波动范围
	 * 4. 计算上下突破轨道（上轨和下轨）
	 * 5. 根据当前价格与上下轨的关系生成交易信号
	 * 6. 执行交易操作（开多、开空、平多、平空）
	 * 
	 * 交易逻辑：
	 * - 无持仓时：价格突破上轨做多，突破下轨做空（仅期货）
	 * - 持多仓时：价格跌破下轨平多
	 * - 持空仓时：价格突破上轨平空（仅期货）
	 * 
	 * @note 该函数重写了SelStrategy基类的虚函数
	 */
	virtual void on_schedule(ISelStraCtx* ctx, uint32_t uDate, uint32_t uTime) override;  // 重写基类函数：策略调度执行

	/**
	 * @brief Tick数据处理回调
	 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
	 * @param stdCode 标准合约代码，触发Tick数据的合约
	 * @param newTick 新的Tick数据，包含最新的价格和成交量信息
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * DualThrust选股策略主要基于K线数据，因此Tick数据处理为空实现。
	 * 如果将来需要基于Tick数据做更精细的交易决策，可以在此函数中实现。
	 * 
	 * @note 该函数重写了SelStrategy基类的虚函数
	 */
	virtual void on_tick(ISelStraCtx* ctx, const char* stdCode, WTSTickData* newTick) override;  // 重写基类函数：Tick数据处理回调

	/**
	 * @brief K线闭合回调
	 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
	 * @param stdCode 标准合约代码，触发K线数据的合约
	 * @param period K线周期，如"m1"、"m5"等
	 * @param newBar 新的K线数据
	 * 
	 * 该函数在K线闭合时被调用，用于处理K线数据。
	 * DualThrust选股策略主要基于定时调度执行，因此K线数据处理为空实现。
	 * 
	 * @note 该函数重写了SelStrategy基类的虚函数
	 */
	virtual void on_bar(ISelStraCtx* ctx, const char* stdCode, const char* period, WTSBarStruct* newBar) override;  // 重写基类函数：K线闭合回调

private:
	// 策略指标参数
	double		_k1;  // 上轨系数，用于计算上突破轨道，通常取值范围为0.5-1.5
	double		_k2;  // 下轨系数，用于计算下突破轨道，通常取值范围为0.5-1.5
	uint32_t	_days;  // 回看天数，用于计算价格波动范围，通常取值为5-20

	// 数据周期相关参数
	std::string _period;  // K线周期，如"m1"（1分钟）、"m5"（5分钟）、"d1"（日线）等
	uint32_t	_count;  // K线条数，用于获取历史K线数据，通常取值为_days的2-3倍

	bool		_isstk;  // 是否为股票标志，true表示股票模式（不支持做空），false表示期货模式（支持做空）

	// 合约代码相关参数
	std::unordered_set<std::string> _codes;  // 合约代码集合，存储策略要选股的所有标的代码，使用无序集合提高查找效率
};

