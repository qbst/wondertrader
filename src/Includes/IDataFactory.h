/*!
 * \file IDataFactory.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据拼接工厂接口定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中数据拼接工厂的核心接口，负责各种市场数据的拼接、更新和转换。
 * 主要功能包括：
 * 1. 利用Tick数据更新K线数据，支持不同周期的K线生成
 * 2. 利用基础周期K线数据更新其他周期K线数据
 * 3. 从基础周期K线数据提取非基础周期的K线数据
 * 4. 从Tick数据提取秒周期的K线数据
 * 5. 支持K线数据的合并操作
 * 
 * 该类主要用于WonderTrader框架中的数据处理系统，为策略引擎和回测系统提供标准化的数据接口。
 * 通过抽象工厂模式，支持多种数据源和不同格式的数据处理。
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含固定大小整数类型
#include "../Includes/WTSTypes.h"  // 包含WTS类型定义

//USING_NS_WTP;  // 注释掉的命名空间使用声明

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSKlineData;  // 前向声明：WTS K线数据类
class WTSHisTrendData;  // 前向声明：WTS历史趋势数据类
class WTSTickData;  // 前向声明：WTS Tick数据类
class WTSSessionInfo;  // 前向声明：WTS交易时间模板信息类
class WTSKlineSlice;  // 前向声明：WTS K线切片类
class WTSContractInfo;  // 前向声明：WTS合约信息类
struct WTSBarStruct;  // 前向声明：WTS K线结构体
struct WTSTickStruct;  // 前向声明：WTS Tick结构体
class WTSTickSlice;  // 前向声明：WTS Tick切片类

/**
 * @class IDataFactory
 * @brief 数据拼接工厂接口类
 * 
 * 该类定义了WonderTrader框架中数据拼接工厂的核心接口，主要用于各种数据的拼接、更新和转换。
 * 通过抽象接口设计，支持多种数据源和不同格式的数据处理，为策略引擎和回测系统提供标准化的数据接口。
 * 
 * 主要特性：
 * - 支持Tick数据到K线数据的转换和更新
 * - 支持不同周期K线数据之间的转换
 * - 支持K线数据的合并和提取操作
 * - 提供灵活的数据处理配置选项
 * - 统一的接口设计，支持多种数据源
 */
class IDataFactory
{
public:
	/**
	 * @brief 利用Tick数据更新K线数据
	 * @param klineData K线数据对象指针
	 * @param tick Tick数据对象指针
	 * @param sInfo 交易时间模板信息对象指针
	 * @param bAlignSec 是否对齐秒级时间戳，默认为false
	 * @return WTSBarStruct* 返回更新后的K线结构体指针
	 * 
	 * 该函数利用最新的Tick数据更新指定周期的K线数据。
	 * 支持秒级时间戳对齐，确保K线数据的准确性。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSBarStruct*	updateKlineData(WTSKlineData* klineData, WTSTickData* tick, WTSSessionInfo* sInfo, bool bAlignSec = false)						= 0;  // 纯虚函数：利用Tick数据更新K线数据

	/**
	 * @brief 利用基础周期K线数据更新K线数据
	 * @param klineData K线数据对象指针
	 * @param newBasicBar 基础周期K线数据结构体指针
	 * @param sInfo 交易时间模板信息对象指针
	 * @param bAlignSec 是否对齐秒级时间戳，默认为false
	 * @return WTSBarStruct* 返回更新后的K线结构体指针
	 * 
	 * 该函数利用基础周期的K线数据更新其他周期的K线数据。
	 * 支持秒级时间戳对齐，确保数据的时间一致性。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSBarStruct*	updateKlineData(WTSKlineData* klineData, WTSBarStruct* newBasicBar, WTSSessionInfo* sInfo, bool bAlignSec = false)				= 0;  // 纯虚函数：利用基础周期K线数据更新K线数据

	/**
	 * @brief 从基础周期K线数据提取非基础周期的K线数据
	 * @param baseKline 基础周期K线切片指针
	 * @param period 基础周期，如m1/m5/day
	 * @param times 周期倍数
	 * @param sInfo 交易时间模板信息对象指针
	 * @param bIncludeOpen 是否包含未闭合的K线，默认为true
	 * @param bSectionSplit 是否按交易时段分割，默认为false
	 * @return WTSKlineData* 返回提取的K线数据对象指针
	 * 
	 * 该函数从基础周期K线数据中提取指定周期倍数的K线数据。
	 * 支持未闭合K线的包含选项和交易时段分割功能。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineData*	extractKlineData(WTSKlineSlice* baseKline, WTSKlinePeriod period, uint32_t times, WTSSessionInfo* sInfo, bool bIncludeOpen = true, bool bSectionSplit = false) = 0;  // 纯虚函数：从基础周期K线数据提取非基础周期的K线数据

	/**
	 * @brief 从Tick数据提取秒周期的K线数据
	 * @param ayTicks Tick数据切片指针
	 * @param seconds 目标周期（秒数）
	 * @param sInfo 交易时间模板信息对象指针
	 * @param bUnixTime Tick时间戳是否是Unix时间格式，默认为false
	 * @param bSectionSplit 是否按交易时段分割，默认为false
	 * @return WTSKlineData* 返回提取的K线数据对象指针
	 * 
	 * 该函数从Tick数据中提取指定秒周期的K线数据。
	 * 支持Unix时间格式和交易时段分割功能。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineData*	extractKlineData(WTSTickSlice* ayTicks, uint32_t seconds, WTSSessionInfo* sInfo, bool bUnixTime = false, bool bSectionSplit = false) = 0;  // 纯虚函数：从Tick数据提取秒周期的K线数据

	/**
	 * @brief 合并K线数据
	 * @param klineData 目标K线数据对象指针
	 * @param newKline 待合并的K线数据对象指针
	 * @return bool 合并成功返回true，失败返回false
	 * 
	 * 该函数将新的K线数据合并到目标K线数据中。
	 * 用于数据的增量更新和历史数据的补充。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool			mergeKlineData(WTSKlineData* klineData, WTSKlineData* newKline)											= 0;  // 纯虚函数：合并K线数据
};

NS_WTP_END  // 结束WonderTrader命名空间
