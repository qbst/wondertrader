/*!
 * \file RiskMonDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 风险监控模块定义文件
 * 
 * 本文件定义了WonderTrader框架中风险监控模块的核心接口和类。
 * 风险监控模块主要负责组合盘的资金风控、通道流量风控、账户资金风控等。
 * 目前风控模块暂时不考虑根据行情风控，只实现基本的资金风控要求。
 * 行情风控通过更高层的择时策略来实现，不在风控模块里处理。
 * 风控模块只处理高效的风控策略。
 * 
 * 主要功能：
 * 1. 组合盘资金风控：针对组合盘的虚拟资金进行风控
 * 2. 通道流量风控：控制总撤单笔数、短时间内下单次数和撤单次数等
 * 3. 账户资金风控：控制账户资金的回撤等
 * 4. 紧急人工介入：提供紧急的人工介入入口
 * 5. 离合器机制：依托于信号和执行分离的机制，直接断开信号执行
 */
#pragma once
#include "../Includes/WTSMarcos.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

NS_WTP_BEGIN
class WTSVariant;        // 配置参数变体类
class WTSPortFundInfo;   // 组合资金信息类

/**
 * @brief 组合上下文接口
 * 
 * 该接口定义了组合风控模块运行所需的上下文环境。
 * 通过该接口，风控模块可以获取组合资金数据、设置数量倍数、
 * 检查交易状态、记录日志等。
 */
class WtPortContext
{
public:
	/**
	 * @brief 获取组合资金数据
	 * @return 组合资金信息对象指针
	 */
	virtual WTSPortFundInfo*	getFundInfo() = 0;

	/**
	 * @brief 设置数量倍数
	 * @param scale 数量倍率，一般小于等于1，用于控制整体仓位比例
	 * 
	 * 该方法用于设置组合盘的数量倍数，通过调整倍数来控制整体仓位比例。
	 * 当市场风险较大时，可以降低倍数来减少仓位；当市场机会较好时，可以适当提高倍数。
	 */
	virtual void	setVolScale(double scale) = 0;

	/**
	 * @brief 检查是否处于交易状态
	 * @return 处于交易状态为true，否则为false
	 * 
	 * 该方法用于检查组合盘当前是否处于交易状态。
	 * 风控模块可以根据交易状态来决定是否执行风控逻辑。
	 */
	virtual bool	isInTrading() = 0;

	/**
	 * @brief 写风控日志
	 * @param message 日志消息
	 * 
	 * 该方法用于记录风控模块的运行日志，便于后续分析和监控。
	 */
	virtual void	writeRiskLog(const char* message) = 0;

	/**
	 * @brief 获取当前日期
	 * @return 当前日期
	 */
	virtual uint32_t	getCurDate() = 0;

	/**
	 * @brief 获取当前时间
	 * @return 当前时间
	 */
	virtual uint32_t	getCurTime() = 0;

	/**
	 * @brief 获取当前交易日
	 * @return 当前交易日
	 */
	virtual uint32_t	getTradingDate() = 0;

	/**
	 * @brief 将时间转换为分钟数（日内有效）
	 * @param uTime 时间值
	 * @return 对应的分钟数
	 * 
	 * 该方法用于将时间值转换为分钟数，主要用于日内风控的时间计算。
	 */
	virtual uint32_t	transTimeToMin(uint32_t uTime) = 0;
};

/**
 * @brief 组合风控模块基类
 * 
 * 该类定义了组合风控模块的基本接口和功能。
 * 风控模块负责监控组合盘的风险状况，当风险超过预设阈值时，
 * 可以采取相应的风控措施，如降低仓位、停止交易等。
 */
class WtRiskMonitor
{
public:
	/**
	 * @brief 构造函数
	 */
	WtRiskMonitor():_ctx(NULL){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~WtRiskMonitor(){}

public:
	/**
	 * @brief 获取风控模块名称
	 * @return 风控模块名称字符串
	 */
	virtual const char* getName() = 0;

	/**
	 * @brief 获取所属执行器工厂名称
	 * @return 执行器工厂名称字符串
	 */
	virtual const char* getFactName() = 0;

	/**
	 * @brief 初始化风控模块
	 * @param ctx 执行单元运行环境
	 * @param cfg 配置参数对象
	 * 
	 * 该方法用于初始化风控模块，设置运行环境和配置参数。
	 * 风控模块在初始化时会获取组合上下文对象，用于后续的风险监控。
	 */
	virtual void init(WtPortContext* ctx, WTSVariant* cfg){ _ctx = ctx; }

	/**
	 * @brief 启动风控模块
	 * 
	 * 该方法用于启动风控模块，开始执行风险监控逻辑。
	 * 启动后，风控模块会持续监控组合盘的风险状况。
	 */
	virtual void run(){}

	/**
	 * @brief 停止风控模块
	 * 
	 * 该方法用于停止风控模块，停止执行风险监控逻辑。
	 * 停止后，风控模块不再进行风险监控。
	 */
	virtual void stop(){}

protected:
	WtPortContext*	_ctx;    // 组合上下文对象指针
};


//////////////////////////////////////////////////////////////////////////
//风控模块工厂接口

/**
 * @brief 风控模块枚举回调函数类型
 * @param factName 工厂名称
 * @param unitName 风控模块名称
 * @param isLast 是否为最后一个风控模块
 */
typedef void(*FuncEnumRiskMonCallback)(const char* factName, const char* unitName, bool isLast);

/**
 * @brief 风控模块工厂接口
 * 
 * 该接口定义了风控模块工厂的基本功能，包括风控模块的创建、管理和销毁。
 * 通过工厂模式实现风控模块的动态加载和管理。
 */
class IRiskMonitorFact
{
public:
	/**
	 * @brief 默认构造函数
	 */
	IRiskMonitorFact(){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~IRiskMonitorFact(){}

public:
	/**
	 * @brief 获取工厂名称
	 * @return 工厂名称字符串
	 */
	virtual const char* getName() = 0;

	/**
	 * @brief 枚举所有风控模块
	 * @param cb 枚举回调函数
	 */
	virtual void enumRiskMonitors(FuncEnumRiskMonCallback cb) = 0;

	/**
	 * @brief 根据名称创建风控模块
	 * @param name 风控模块名称
	 * @return 风控模块对象指针
	 */
	virtual WtRiskMonitor* createRiskMonotor(const char* name) = 0;

	/**
	 * @brief 删除风控模块
	 * @param unit 要删除的风控模块对象
	 * @return 删除是否成功
	 */
	virtual bool deleteRiskMonotor(WtRiskMonitor* unit) = 0;
};

//创建风控模块工厂的函数指针类型
typedef IRiskMonitorFact* (*FuncCreateRiskMonFact)();
//删除风控模块工厂的函数指针类型
typedef void(*FuncDeleteRiskMonFact)(IRiskMonitorFact* &fact);

NS_WTP_END
