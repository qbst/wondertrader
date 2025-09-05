/*!
 * \file CtaStrategyDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中CTA（Commodity Trading Advisor）策略的核心接口和类结构。
 * 主要功能包括：
 * 1. 定义CTA策略基类，提供策略生命周期管理接口
 * 2. 定义策略工厂接口，支持策略的动态创建和管理
 * 3. 提供策略回调机制，处理市场数据、交易信号等事件
 * 4. 支持策略的初始化和配置管理
 * 5. 定义策略工厂的创建和删除函数指针类型
 * 
 * 该类主要用于WonderTrader框架中的CTA策略开发，为量化交易策略提供统一的接口规范。
 * 通过抽象基类设计，支持多种策略类型的扩展和插件化开发。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串支持
#include <stdint.h>  // 包含固定大小整数类型

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：WTS变体类型类
class ICtaStraCtx;  // 前向声明：CTA策略上下文接口
class ICtaTickStraCtx;  // 前向声明：CTA Tick策略上下文接口
class WTSTickData;  // 前向声明：WTS Tick数据结构类
struct WTSBarStruct;  // 前向声明：WTS K线结构体
NS_WTP_END  // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class CtaStrategy
 * @brief CTA策略基类
 * 
 * 该类定义了CTA策略的基本接口和生命周期管理方法。
 * 所有具体的CTA策略都应该继承此类并实现相应的虚函数。
 * 提供了从策略初始化到交易执行的完整生命周期管理。
 * 
 * 主要特性：
 * - 支持策略的初始化和配置
 * - 提供交易日开始/结束的回调
 * - 支持Tick数据和K线数据的处理
 * - 支持条件单触发处理
 * - 提供策略调度和完成回调
 */
class CtaStrategy
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符
	 * 
	 * 初始化CTA策略对象，设置策略ID。
	 * 策略ID用于在系统中唯一标识该策略实例。
	 */
	CtaStrategy(const char* id) :_id(id){}  // 初始化策略ID

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~CtaStrategy(){}  // 虚析构函数，支持继承

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

	/**
	 * @brief 初始化完成回调
	 * @param ctx 策略上下文对象
	 * 
	 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_init(ICtaStraCtx* ctx){}  // 虚函数：初始化完成回调

	/**
	 * @brief 交易日开始回调
	 * @param ctx 策略上下文对象
	 * @param uTDate 交易日日期
	 * 
	 * 该函数在每个交易日开始时被调用，用于执行交易日开始时的准备工作。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_session_begin(ICtaStraCtx* ctx, uint32_t uTDate) {}  // 虚函数：交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * @param ctx 策略上下文对象
	 * @param uTDate 交易日日期
	 * 
	 * 该函数在每个交易日结束时被调用，用于执行交易日结束时的清理工作。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_session_end(ICtaStraCtx* ctx, uint32_t uTDate) {}  // 虚函数：交易日结束回调

	/**
	 * @brief 策略调度执行入口
	 * @param ctx 策略上下文对象
	 * @param uDate 执行日期
	 * @param uTime 执行时间
	 * 
	 * 该函数是策略的主体逻辑执行入口，在策略调度时被调用。
	 * 默认实现为空，子类必须重写此函数实现具体的策略逻辑。
	 */
	virtual void on_schedule(ICtaStraCtx* ctx, uint32_t uDate, uint32_t uTime){}  // 虚函数：策略调度执行入口

	/**
	 * @brief 策略调度执行完成回调
	 * @param ctx 策略上下文对象
	 * @param uDate 执行日期
	 * @param uTime 执行时间
	 * 
	 * 该函数在策略调度执行完成后被调用，用于执行完成后的处理工作。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_schedule_done(ICtaStraCtx* ctx, uint32_t uDate, uint32_t uTime) {}  // 虚函数：策略调度执行完成回调

	/**
	 * @brief Tick数据处理回调
	 * @param ctx 策略上下文对象
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * 默认实现为空，子类可以重写此函数实现Tick数据处理逻辑。
	 */
	virtual void on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick){}  // 虚函数：Tick数据处理回调

	/**
	 * @brief K线闭合回调
	 * @param ctx 策略上下文对象
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据
	 * 
	 * 该函数在K线闭合时被调用，用于处理K线数据。
	 * 默认实现为空，子类可以重写此函数实现K线数据处理逻辑。
	 */
	virtual void on_bar(ICtaStraCtx* ctx, const char* stdCode, const char* period, WTSBarStruct* newBar){}  // 虚函数：K线闭合回调

	/**
	 * @brief 条件单触发回调
	 * @param ctx 策略上下文对象
	 * @param stdCode 标准合约代码
	 * @param target 目标价格
	 * @param price 触发价格
	 * @param usertag 用户标签
	 * 
	 * 该函数在条件单触发时被调用，用于处理条件单触发事件。
	 * 默认实现为空，子类可以重写此函数实现条件单触发处理逻辑。
	 */
	virtual void on_condition_triggered(ICtaStraCtx* ctx, const char* stdCode, double target, double price, const char* usertag) {}  // 虚函数：条件单触发回调

protected:
	std::string _id;  // 策略唯一标识符，存储策略的ID字符串
};

//////////////////////////////////////////////////////////////////////////
//策略工厂接口
/**
 * @typedef FuncEnumStrategyCallback
 * @brief 策略枚举回调函数类型
 * 
 * 该类型定义了策略枚举时的回调函数签名。
 * 用于在枚举策略时通知调用者每个策略的信息。
 * 
 * @param factName 工厂名称
 * @param straName 策略名称
 * @param isLast 是否为最后一个策略
 */
typedef void(*FuncEnumStrategyCallback)(const char* factName, const char* straName, bool isLast);  // 策略枚举回调函数类型定义

/**
 * @class ICtaStrategyFact
 * @brief CTA策略工厂接口
 * 
 * 该类定义了CTA策略工厂的基本接口，负责策略的创建、删除和管理。
 * 通过工厂模式实现策略的动态加载和管理，支持插件化开发。
 * 
 * 主要特性：
 * - 支持策略的枚举和查询
 * - 提供策略的创建和删除功能
 * - 支持策略工厂的命名和管理
 * - 实现策略的生命周期管理
 */
class ICtaStrategyFact
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化策略工厂对象，不执行任何特殊操作。
	 */
	ICtaStrategyFact(){}  // 默认构造函数

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~ICtaStrategyFact(){}  // 虚析构函数，支持继承

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
	 * 该函数枚举工厂中所有可用的策略，通过回调函数通知调用者。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void enumStrategy(FuncEnumStrategyCallback cb) = 0;  // 纯虚函数：枚举策略

	/**
	 * @brief 根据名称创建K线级别策略
	 * @param name 策略名称
	 * @param id 策略ID
	 * @return CtaStrategy* 返回创建的策略对象指针
	 * 
	 * 该函数根据策略名称创建对应的策略对象实例。
	 * 纯虚函数，子类必须实现。
	 */
	virtual CtaStrategy* createStrategy(const char* name, const char* id) = 0;  // 纯虚函数：创建策略


	/**
	 * @brief 删除策略
	 * @param stra 要删除的策略对象指针
	 * @return bool 删除成功返回true，失败返回false
	 * 
	 * 该函数删除指定的策略对象，释放相关资源。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool deleteStrategy(CtaStrategy* stra) = 0;  // 纯虚函数：删除策略
};

/**
 * @typedef FuncCreateStraFact
 * @brief 创建策略工厂函数指针类型
 * 
 * 该类型定义了创建策略工厂的函数指针签名。
 * 用于动态加载策略工厂插件。
 */
typedef ICtaStrategyFact* (*FuncCreateStraFact)();  // 创建策略工厂函数指针类型定义

/**
 * @typedef FuncDeleteStraFact
 * @brief 删除策略工厂函数指针类型
 * 
 * 该类型定义了删除策略工厂的函数指针签名。
 * 用于动态卸载策略工厂插件。
 */
typedef void(*FuncDeleteStraFact)(ICtaStrategyFact* &fact);  // 删除策略工厂函数指针类型定义
