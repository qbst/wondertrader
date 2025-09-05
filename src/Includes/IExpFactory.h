/*!
 * \file IExpFactory.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 指标工厂接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中指标工厂的核心接口，负责创建和计算各种技术指标。
 * 主要功能包括：
 * 1. 定义指标工厂接口，支持K线数据的技术指标计算
 * 2. 支持趋势数据的技术指标计算
 * 3. 提供统一的指标计算接口，支持参数化配置
 * 4. 支持多种指标类型的扩展和插件化开发
 * 
 * 该类主要用于WonderTrader框架中的技术指标系统，为策略引擎和图表系统提供指标计算服务。
 * 通过抽象工厂模式，支持多种指标算法和不同数据源的指标计算。
 */
#pragma once  // 防止头文件重复包含
#include "WTSMarcos.h"  // 包含WonderTrader宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSKlineData;  // 前向声明：WTS K线数据类
class WTSExpressParams;  // 前向声明：WTS指标参数类
class WTSExpressData;  // 前向声明：WTS指标数据类
class WTSHisTrendData;  // 前向声明：WTS历史趋势数据类

/**
 * @class IExpFactory
 * @brief 指标工厂接口类
 * 
 * 该类定义了WonderTrader框架中指标工厂的核心接口，负责创建和计算各种技术指标。
 * 支持基于K线数据和趋势数据的指标计算，提供统一的指标计算接口。
 * 
 * 主要特性：
 * - 支持基于K线数据的技术指标计算
 * - 支持基于趋势数据的技术指标计算
 * - 提供参数化的指标计算接口
 * - 支持多种指标类型的扩展
 * - 统一的接口设计，支持插件化开发
 */
class IExpFactory
{
public:
	/**
	 * @brief 计算基于K线数据的指标
	 * @param expName 指标名称
	 * @param klineData K线数据指针
	 * @param params 指标参数指针
	 * @return WTSExpressData* 返回计算后的指标数据指针
	 * 
	 * 该函数根据指定的指标名称和参数，基于K线数据计算技术指标。
	 * 支持各种技术指标如MA、MACD、RSI、KDJ等的计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSExpressData *calcKlineExpress(const char* expName, WTSKlineData* klineData, WTSExpressParams* params) = 0;  // 纯虚函数：计算基于K线数据的指标

	/**
	 * @brief 计算基于趋势数据的指标
	 * @param expName 指标名称
	 * @param trendData 趋势数据指针
	 * @param params 指标参数指针
	 * @return WTSExpressData* 返回计算后的指标数据指针
	 * 
	 * 该函数根据指定的指标名称和参数，基于趋势数据计算技术指标。
	 * 支持基于趋势数据的指标计算，如趋势强度、趋势持续性等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSExpressData *calcTrendExpress(const char* expName, WTSHisTrendData* trendData, WTSExpressParams* params) = 0;  // 纯虚函数：计算基于趋势数据的指标
};

NS_WTP_END  // 结束WonderTrader命名空间