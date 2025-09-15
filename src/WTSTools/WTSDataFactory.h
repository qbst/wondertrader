/*!
 * \file WTSDataFactory.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据工厂类 - K线数据处理和转换核心模块
 * 
 * 本文件定义了WonderTrader框架中的数据工厂类，负责处理各种时间周期的K线数据转换、
 * 更新和提取操作。这是框架中处理历史数据和实时数据转换的核心组件。
 * 
 * 主要功能：
 * - 基于Tick数据实时更新各周期K线（1分钟、5分钟、日线等）
 * - 基于基础周期K线数据生成更大周期的K线数据
 * - 从基础周期K线提取目标周期的K线数据
 * - 从Tick数据直接提取秒级K线数据
 * - K线数据的合并和拼接操作
 * - 支持按交易时段对齐的K线生成
 * 
 * 设计特点：
 * - 实现IDataFactory接口，提供统一的数据处理接口
 * - 支持多种时间周期的K线数据处理
 * - 考虑交易时段特性，支持夜盘等复杂交易时间
 * - 高效的数据转换算法，适合实时和历史数据处理
 * - 支持数据完整性检查和闭合状态管理
 */
#pragma once
#include "../Includes/IDataFactory.h"  // 数据工厂接口定义

USING_NS_WTP;

/**
 * @class WTSDataFactory
 * @brief WonderTrader数据工厂实现类
 * 
 * WTSDataFactory是IDataFactory接口的具体实现，提供了完整的K线数据处理功能。
 * 该类负责处理从最底层的Tick数据到各种时间周期K线数据的转换和更新操作。
 * 
 * 核心能力：
 * - 实时K线更新：基于新的Tick数据更新现有K线
 * - 周期转换：从基础周期生成更大周期的K线数据
 * - 数据提取：从历史数据中提取指定周期的K线
 * - 数据合并：将多个K线数据源进行合并
 * - 时段对齐：支持按交易时段边界对齐K线
 * 
 * 支持的数据类型：
 * - Tick级别数据（最小时间单位）
 * - 秒级K线数据
 * - 分钟级K线数据（1分钟、5分钟等）
 * - 日线数据
 * 
 * 时间处理特性：
 * - 考虑交易所交易时段规则
 * - 支持夜盘等跨自然日的交易时段
 * - 处理节假日和交易日历
 * - 支持不同交易所的时区处理
 */
class WTSDataFactory : public IDataFactory
{
public:
	/**
	 * @name 公共接口方法 - IDataFactory接口实现
	 * @brief 实现数据工厂的核心数据处理接口
	 * @{
	 */

	/**
	 * @brief 基于Tick数据更新K线数据
	 * @param klineData 目标K线数据对象
	 * @param tick 新的Tick数据
	 * @param sInfo 交易时间模板信息
	 * @param bAlignSec 是否按交易时段对齐，默认false
	 * @return WTSBarStruct* 返回新生成或更新的K线结构，如果没有更新返回NULL
	 * 
	 * 使用新的Tick数据来更新现有的K线数据。根据Tick的时间戳和价格信息，
	 * 决定是更新当前K线还是创建新的K线。支持各种周期的K线更新。
	 * 
	 * 处理逻辑：
	 * - 检查Tick时间是否在交易时段内
	 * - 根据K线周期类型选择相应的更新策略
	 * - 支持1分钟、5分钟、日线和秒级K线的更新
	 * - 可选择按交易时段边界对齐K线时间
	 */
	virtual WTSBarStruct*	updateKlineData(WTSKlineData* klineData, WTSTickData* tick, WTSSessionInfo* sInfo, bool bAlignSec = false);

	/**
	 * @brief 基于基础周期K线数据更新目标K线
	 * @param klineData 目标K线数据对象
	 * @param newBasicBar 新的基础周期K线数据
	 * @param sInfo 交易时间模板信息
	 * @param bAlignSec 是否按交易时段对齐，默认false
	 * @return WTSBarStruct* 返回新生成或更新的K线结构，如果没有更新返回NULL
	 * 
	 * 使用基础周期的K线数据来更新更大周期的K线数据。例如，使用1分钟K线
	 * 来更新5分钟K线，或使用5分钟K线来更新小时K线。
	 * 
	 * 适用场景：
	 * - 从1分钟K线生成多分钟K线
	 * - 从5分钟K线生成更大周期K线
	 * - 实时K线数据的周期转换
	 */
	virtual WTSBarStruct*	updateKlineData(WTSKlineData* klineData, WTSBarStruct* newBasicBar, WTSSessionInfo* sInfo, bool bAlignSec = false);

	/**
	 * @brief 从基础周期K线数据提取目标周期K线
	 * @param baseKline 基础周期K线数据切片
	 * @param period 目标K线周期类型（KP_Minute1/KP_Minute5/KP_DAY等）
	 * @param times 周期倍数（如5分钟K线的times为5）
	 * @param sInfo 交易时间模板信息
	 * @param bIncludeOpen 是否包含未闭合的K线，默认true
	 * @param bAlignSec 是否按交易时段对齐，默认false
	 * @return WTSKlineData* 返回提取后的K线数据，失败返回NULL
	 * 
	 * 从已有的基础周期K线数据中提取出指定周期的K线数据。这是历史数据
	 * 处理中的核心功能，用于生成各种周期的K线数据。
	 * 
	 * 功能特点：
	 * - 支持多种目标周期的提取
	 * - 可控制是否包含最后一根未完成的K线
	 * - 支持按交易时段边界对齐
	 * - 自动处理跨交易时段的时间计算
	 */
	virtual WTSKlineData*	extractKlineData(WTSKlineSlice* baseKline, WTSKlinePeriod period, uint32_t times, WTSSessionInfo* sInfo, bool bIncludeOpen = true, bool bAlignSec = false);

	/**
	 * @brief 从Tick数据直接提取秒级K线数据
	 * @param ayTicks Tick数据切片
	 * @param seconds 目标秒级周期（如30表示30秒K线）
	 * @param sInfo 交易时间模板信息
	 * @param bUnixTime Tick时间戳是否为Unix时间格式，默认false
	 * @param bAlignSec 是否按交易时段对齐，默认false
	 * @return WTSKlineData* 返回生成的秒级K线数据，失败返回NULL
	 * 
	 * 直接从Tick数据生成秒级K线，适用于需要高频数据分析的场景。
	 * 支持各种秒级周期，如1秒、5秒、30秒等。
	 * 
	 * 应用场景：
	 * - 高频交易策略的数据准备
	 * - 精确的价格波动分析
	 * - 实时风控系统的数据支持
	 */
	virtual WTSKlineData*	extractKlineData(WTSTickSlice* ayTicks, uint32_t seconds, WTSSessionInfo* sInfo, bool bUnixTime = false, bool bAlignSec = false);

	/**
	 * @brief 合并多个K线数据
	 * @param klineData 目标K线数据（合并结果存储在这里）
	 * @param newKline 待合并的K线数据
	 * @return bool 合并成功返回true，失败返回false
	 * 
	 * 将两个K线数据进行合并，通常用于将新的历史数据与现有数据进行拼接。
	 * 合并时会自动处理时间序列的排序和去重。
	 * 
	 * 合并规则：
	 * - 检查K线数据的兼容性（合约代码、周期等）
	 * - 按时间顺序合并数据
	 * - 自动处理时间重叠的情况
	 * - 保持合并后数据的完整性和一致性
	 */
	virtual bool			mergeKlineData(WTSKlineData* klineData, WTSKlineData* newKline);
	
	/** @} */

protected:
	/**
	 * @name Tick数据更新K线的内部实现方法
	 * @brief 针对不同周期K线的Tick数据更新实现
	 * @{
	 */
	
	/**
	 * @brief 使用Tick数据更新1分钟K线
	 * @param sInfo 交易时间模板信息
	 * @param klineData 目标1分钟K线数据
	 * @param tick 新的Tick数据
	 * @param bAlignSec 是否按交易时段对齐
	 * @return WTSBarStruct* 返回新生成的K线结构或NULL
	 */
	WTSBarStruct* updateMin1Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick, bool bAlignSec = false);
	
	/**
	 * @brief 使用Tick数据更新5分钟K线
	 * @param sInfo 交易时间模板信息
	 * @param klineData 目标5分钟K线数据
	 * @param tick 新的Tick数据
	 * @param bAlignSec 是否按交易时段对齐
	 * @return WTSBarStruct* 返回新生成的K线结构或NULL
	 */
	WTSBarStruct* updateMin5Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick, bool bAlignSec = false);
	
	/**
	 * @brief 使用Tick数据更新日线数据
	 * @param sInfo 交易时间模板信息
	 * @param klineData 目标日线数据
	 * @param tick 新的Tick数据
	 * @return WTSBarStruct* 返回新生成的K线结构或NULL
	 */
	WTSBarStruct* updateDayData(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick);
	
	/**
	 * @brief 使用Tick数据更新秒级K线数据
	 * @param sInfo 交易时间模板信息
	 * @param klineData 目标秒级K线数据
	 * @param tick 新的Tick数据
	 * @return WTSBarStruct* 返回新生成的K线结构或NULL
	 */
	WTSBarStruct* updateSecData(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick);
	
	/** @} */

	/**
	 * @name 基础K线更新目标K线的内部实现方法
	 * @brief 使用基础周期K线数据更新更大周期K线的实现
	 * @{
	 */
	
	/**
	 * @brief 使用基础K线数据更新1分钟K线（多倍周期）
	 * @param sInfo 交易时间模板信息
	 * @param klineData 目标多分钟K线数据
	 * @param newBasicBar 新的1分钟基础K线数据
	 * @param bAlignSec 是否按交易时段对齐
	 * @return WTSBarStruct* 返回新生成的K线结构或NULL
	 */
	WTSBarStruct* updateMin1Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSBarStruct* newBasicBar, bool bAlignSec = false);
	
	/**
	 * @brief 使用5分钟K线数据更新更大周期的K线
	 * @param sInfo 交易时间模板信息
	 * @param klineData 目标K线数据
	 * @param newBasicBar 新的5分钟基础K线数据
	 * @param bAlignSec 是否按交易时段对齐
	 * @return WTSBarStruct* 返回新生成的K线结构或NULL
	 */
	WTSBarStruct* updateMin5Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSBarStruct* newBasicBar, bool bAlignSec = false);
	
	/** @} */

	/**
	 * @name K线数据提取的内部实现方法
	 * @brief 从基础K线数据提取目标周期K线的具体实现
	 * @{
	 */
	
	/**
	 * @brief 从基础K线提取多倍1分钟周期的K线数据
	 * @param baseKline 基础周期K线数据切片
	 * @param times 周期倍数
	 * @param sInfo 交易时间模板信息
	 * @param bIncludeOpen 是否包含未闭合的K线
	 * @param bAlignSec 是否按交易时段对齐
	 * @return WTSKlineData* 提取的K线数据
	 */
	WTSKlineData* extractMin1Data(WTSKlineSlice* baseKline, uint32_t times, WTSSessionInfo* sInfo, bool bIncludeOpen = true, bool bAlignSec = false);
	
	/**
	 * @brief 从5分钟K线提取更大周期的K线数据
	 * @param baseKline 基础周期K线数据切片
	 * @param times 周期倍数
	 * @param sInfo 交易时间模板信息
	 * @param bIncludeOpen 是否包含未闭合的K线
	 * @param bAlignSec 是否按交易时段对齐
	 * @return WTSKlineData* 提取的K线数据
	 */
	WTSKlineData* extractMin5Data(WTSKlineSlice* baseKline, uint32_t times, WTSSessionInfo* sInfo, bool bIncludeOpen = true, bool bAlignSec = false);
	
	/**
	 * @brief 从日线数据提取多日周期的K线数据
	 * @param baseKline 基础日线数据切片
	 * @param times 周期倍数（如5表示5日K线）
	 * @param bIncludeOpen 是否包含未闭合的K线
	 * @return WTSKlineData* 提取的K线数据
	 */
	WTSKlineData* extractDayData(WTSKlineSlice* baseKline, uint32_t times, bool bIncludeOpen = true);
	
	/** @} */

	/**
	 * @name 辅助工具方法
	 * @brief 数据处理中使用的辅助函数
	 * @{
	 */
	
	/**
	 * @brief 获取指定分钟的前一个分钟时间
	 * @param curMinute 当前分钟时间（格式：HHMM）
	 * @param period 间隔周期，默认为1分钟
	 * @return uint32_t 前一个分钟的时间
	 * 
	 * 处理跨小时、跨天的分钟时间计算，用于K线时间戳的计算。
	 */
	static uint32_t getPrevMinute(uint32_t curMinute, int period = 1);
	
	/** @} */
};

