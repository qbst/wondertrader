/*!
* \file SelStrategyDefs.h
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief SEL策略引擎核心定义文件
* 
* 本文件定义了WonderTrader框架中SEL（Selection）策略引擎的核心接口和类。
* SEL引擎是异步策略引擎，适用于标的较多、计算逻辑耗时较长的策略，如多因子选股策略、截面多空策略等。
* 该引擎采用时间驱动模式，通过向引擎注册重算时间调度，定时触发重算并调整多标的的目标仓位。
* 
* 主要包含：
* 1. SelStrategy类：SEL策略基类，定义了策略的基本接口和生命周期回调
* 2. ISelStrategyFact类：策略工厂接口，负责策略的创建、管理和销毁
* 3. 相关函数指针类型定义：用于动态加载策略工厂
*/
#pragma once
#include <string>
#include <stdint.h>

#include "../Includes/WTSMarcos.h"

NS_WTP_BEGIN
class WTSVariant;        // 配置参数变体类
class ISelStraCtx;       // SEL策略上下文接口
class WTSTickData;       // Tick数据结构
struct WTSBarStruct;     // K线数据结构
NS_WTP_END

USING_NS_WTP;

/**
 * @brief SEL策略基类
 * 
 * 该类定义了SEL策略引擎中所有策略必须实现的基本接口。
 * SEL策略适用于多标的、计算量大的策略，如选股策略、多因子策略等。
 * 策略通过时间驱动的方式执行，可以设置不同的重算周期（日内、每日、每周、每月等）。
 */
class SelStrategy
{
public:
	/**
	 * @brief 构造函数
	 * @param id 策略唯一标识符
	 */
	SelStrategy(const char* id) :_id(id){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~SelStrategy(){}

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

	/**
	 * @brief 策略初始化完成回调
	 * @param ctx 策略上下文对象
	 */
	virtual void on_init(ISelStraCtx* ctx){}

	/**
	 * @brief 交易日开始回调
	 * @param ctx 策略上下文对象
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_begin(ISelStraCtx* ctx, uint32_t uTDate) {}

	/**
	 * @brief 交易日结束回调
	 * @param ctx 策略上下文对象
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_end(ISelStraCtx* ctx, uint32_t uTDate) {}

	/**
	 * @brief 策略主体逻辑执行入口
	 * @param ctx 策略上下文对象
	 * @param uDate 当前日期
	 * @param uTime 当前时间
	 */
	virtual void on_schedule(ISelStraCtx* ctx, uint32_t uDate, uint32_t uTime){}

	/**
	 * @brief Tick数据回调
	 * @param ctx 策略上下文对象
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据
	 */
	virtual void on_tick(ISelStraCtx* ctx, const char* stdCode, WTSTickData* newTick){}

	/**
	 * @brief K线闭合回调
	 * @param ctx 策略上下文对象
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar(ISelStraCtx* ctx, const char* stdCode, const char* period, WTSBarStruct* newBar){}

protected:
	std::string _id;    // 策略唯一标识符
};

//////////////////////////////////////////////////////////////////////////
//策略工厂接口

/**
 * @brief 策略枚举回调函数类型
 * @param factName 工厂名称
 * @param straName 策略名称
 * @param isLast 是否为最后一个策略
 */
typedef void(*FuncEnumSelStrategyCallback)(const char* factName, const char* straName, bool isLast);

/**
 * @brief SEL策略工厂接口
 * 
 * 该接口定义了SEL策略工厂的基本功能，包括策略的创建、管理和销毁。
 * 通过工厂模式实现策略的动态加载和管理。
 */
class ISelStrategyFact
{
public:
	/**
	 * @brief 默认构造函数
	 */
	ISelStrategyFact(){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~ISelStrategyFact(){}

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
	virtual void enumStrategy(FuncEnumSelStrategyCallback cb) = 0;

	/**
	 * @brief 根据名称创建策略实例
	 * @param name 策略名称
	 * @param id 策略ID
	 * @return 策略实例指针
	 */
	virtual SelStrategy* createStrategy(const char* name, const char* id) = 0;

	/**
	 * @brief 删除策略实例
	 * @param stra 要删除的策略实例
	 * @return 删除是否成功
	 */
	virtual bool deleteStrategy(SelStrategy* stra) = 0;
};

//创建工厂的函数指针类型
typedef ISelStrategyFact* (*FuncCreateSelStraFact)();
//删除工厂的函数指针类型
typedef void(*FuncDeleteSelStraFact)(ISelStrategyFact* &fact);
