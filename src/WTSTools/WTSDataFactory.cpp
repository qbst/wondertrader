/*!
 * \file WTSDataFactory.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据工厂类的实现文件
 * 
 * 本文件实现了WTSDataFactory.h中定义的数据工厂类，是WonderTrader框架中
 * 最核心的数据处理模块之一，负责处理各种时间周期的K线数据转换和更新操作。
 * 
 * 设计逻辑和主要作用：
 * ===============
 * 
 * 1. **核心设计理念**：
 *    - 作为"数据转换引擎"，提供Tick数据到各种K线数据的实时转换
 *    - 实现多层级的数据聚合，支持从秒级到日级的各种时间周期
 *    - 提供高效的实时数据处理能力，满足高频交易的性能要求
 *    - 确保数据的时间一致性和完整性，处理复杂的交易时间逻辑
 * 
 * 2. **在WonderTrader框架中的核心作用**：
 *    - **实时K线生成**：将实时Tick数据转换为各种周期的K线数据
 *    - **历史数据处理**：从基础周期K线提取目标周期的历史数据
 *    - **数据聚合服务**：为策略引擎提供所需周期的K线数据
 *    - **时间对齐处理**：处理交易时段边界对齐和跨时段数据
 *    - **数据完整性保证**：确保K线数据的OHLCV完整性和时间连续性
 * 
 * 3. **数据处理架构**：
 *    - **输入层**：Tick数据、基础周期K线数据
 *    - **处理层**：时间计算、数据聚合、周期转换
 *    - **输出层**：目标周期K线数据、更新状态信息
 *    - **辅助层**：交易时段处理、时间对齐、数据验证
 * 
 * 4. **核心算法特点**：
 *    - **时间窗口算法**：基于交易时段的动态时间窗口计算
 *    - **增量更新机制**：支持实时数据的增量更新，避免全量重算
 *    - **多周期支持**：统一的算法框架支持任意时间周期的转换
 *    - **边界对齐处理**：精确处理交易时段边界的K线对齐
 *    - **异常数据过滤**：自动过滤非交易时间的异常数据
 * 
 * 5. **支持的数据转换类型**：
 *    - **Tick -> 秒级K线**：直接从Tick数据生成秒级K线
 *    - **Tick -> 分钟K线**：从Tick数据生成1分钟、5分钟等K线
 *    - **Tick -> 日线**：从Tick数据直接聚合日线数据
 *    - **基础K线 -> 目标K线**：从小周期K线生成大周期K线
 *    - **K线数据合并**：多个K线数据源的合并和拼接
 * 
 * 6. **时间处理复杂性**：
 *    - **夜盘处理**：正确处理跨自然日的夜盘交易时间
 *    - **时段切换**：处理交易时段之间的切换和边界
 *    - **节假日处理**：考虑节假日对交易时间的影响
 *    - **时区转换**：支持不同交易所的时区差异处理
 *    - **分钟对齐**：支持按交易时段分钟边界对齐K线
 * 
 * 7. **性能优化策略**：
 *    - **就地更新**：尽可能在原有数据结构上进行更新，减少内存分配
 *    - **缓存机制**：缓存时间计算结果，避免重复计算
 *    - **批量处理**：支持批量数据的高效处理
 *    - **内存管理**：使用引用计数和智能指针确保内存安全
 * 
 * 8. **主要使用场景**：
 *    - 实时行情系统：将实时Tick转换为各种K线供策略使用
 *    - 历史数据服务：从基础数据生成各种周期的历史K线
 *    - 策略回测引擎：为回测提供准确的历史K线数据
 *    - 数据分析工具：为量化分析提供标准化的K线数据
 *    - 风控系统：提供实时的价格和成交量数据
 */
// 数据工厂相关头文件包含
#include "WTSDataFactory.h"                    // 数据工厂类定义
#include "../Includes/WTSDataDef.hpp"          // K线数据结构定义（WTSBarStruct、WTSKlineData等）
#include "../Includes/WTSContractInfo.hpp"     // 合约信息数据结构定义
#include "../Includes/WTSSessionInfo.hpp"      // 交易时段信息数据结构定义
#include "../Share/TimeUtils.hpp"              // 时间处理工具类，提供各种时间计算功能

// 使用标准命名空间，简化代码编写
using namespace std;


/**
 * @brief 基于Tick数据更新K线数据（主入口函数）
 * @param klineData 目标K线数据对象，要更新的K线数据容器
 * @param tick 新的Tick数据，包含最新的价格和成交信息
 * @param sInfo 交易时段信息，用于时间验证和计算
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSBarStruct* 返回新生成或更新的K线结构指针，无更新返回NULL
 * 
 * 该函数是数据工厂的核心入口函数，负责将实时Tick数据转换为各种周期的K线数据。
 * 这是实时行情系统中最重要的数据处理函数，直接影响策略的数据质量。
 * 
 * 处理流程：
 * 1. **输入验证**：检查所有输入参数的有效性
 * 2. **合约匹配**：确保Tick数据和K线数据属于同一合约
 * 3. **时间验证**：验证Tick时间是否在有效交易时段内
 * 4. **周期分发**：根据K线周期类型分发到相应的处理函数
 * 
 * 支持的K线周期：
 * - KP_Tick：秒级K线（实际上是处理Tick聚合）
 * - KP_Minute1：1分钟K线
 * - KP_Minute5：5分钟K线
 * - KP_DAY：日线数据
 * 
 * 关键设计特点：
 * - 统一的接口设计，支持多种K线周期
 * - 严格的数据验证，确保数据质量
 * - 交易时间验证，过滤非交易时间的异常数据
 * - 模块化设计，每种周期有专门的处理函数
 */
WTSBarStruct* WTSDataFactory::updateKlineData(WTSKlineData* klineData, WTSTickData* tick, WTSSessionInfo* sInfo, bool bAlignSec/* = false*/)
{
	// **第一层验证：基础参数有效性检查**
	if(klineData == NULL || tick == NULL)
		return NULL;  // K线数据或Tick数据为空，无法处理

	// **第二层验证：合约代码一致性检查**
	// 确保Tick数据和K线数据属于同一个合约，避免数据混乱
	if(strcmp(klineData->code(), tick->code()) != 0)
		return NULL;  // 合约代码不匹配，返回NULL

	// **第三层验证：交易时段信息有效性检查**
	if(sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// **第四层验证：交易时间有效性检查**
	// 检查Tick的时间戳是否在有效的交易时段内
	// actiontime格式为HHMMSSMMM，除以100000得到HHMM格式
	if (!sInfo->isInTradingTime(tick->actiontime() / 100000))
		return NULL;  // Tick时间不在交易时段内，忽略该数据

	// **核心处理逻辑：根据K线周期类型分发处理**
	WTSKlinePeriod period = klineData->period();
	switch( period )
	{
	case KP_Tick:
		// 处理秒级K线（Tick级别的聚合）
		return updateSecData(sInfo, klineData, tick);
		break;
	case KP_Minute1:
		// 处理1分钟K线更新
		return updateMin1Data(sInfo, klineData, tick, bAlignSec);
	case KP_Minute5:
		// 处理5分钟K线更新
		return updateMin5Data(sInfo, klineData, tick, bAlignSec);
	case KP_DAY:
		// 处理日线数据更新
		return updateDayData(sInfo, klineData, tick);
	default:
		// 不支持的K线周期类型
		return NULL;
	}
}

/**
 * @brief 基于基础K线数据更新目标K线（重载函数）
 * @param klineData 目标K线数据对象，要更新的K线数据容器
 * @param newBasicBar 新的基础周期K线数据，已经聚合好的K线结构
 * @param sInfo 交易时段信息，用于时间计算和验证
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSBarStruct* 返回新生成或更新的K线结构指针，无更新返回NULL
 * 
 * 该函数是updateKlineData的重载版本，用于基于已有的基础周期K线数据来更新
 * 更大周期的K线数据。这种方式比直接从Tick数据聚合更高效。
 * 
 * 典型应用场景：
 * - 从1分钟K线生成5分钟K线
 * - 从5分钟K线生成15分钟、30分钟K线
 * - 实时数据流中的多级K线同步更新
 * 
 * 与Tick版本的区别：
 * - 输入数据已经是聚合后的K线结构，包含完整的OHLCV信息
 * - 不需要进行Tick级别的价格聚合，只需要进行时间窗口聚合
 * - 处理效率更高，适合多级K线的批量更新
 * 
 * 支持的周期转换：
 * - 1分钟 -> N分钟（N > 1）
 * - 5分钟 -> N×5分钟
 * 
 * 注意：该重载版本不支持日线和秒级K线的更新，
 * 这些周期需要使用Tick版本的updateKlineData函数。
 */
WTSBarStruct* WTSDataFactory::updateKlineData(WTSKlineData* klineData, WTSBarStruct* newBasicBar, WTSSessionInfo* sInfo, bool bAlignSec/* = false*/)
{
	// **参数有效性验证**
	if (klineData == NULL || newBasicBar == NULL)
		return NULL;  // 必要参数为空，无法处理

	// **交易时段信息验证**
	if (sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// **根据目标K线周期分发处理**
	WTSKlinePeriod period = klineData->period();
	switch (period)
	{
	case KP_Minute1:
		// 使用基础K线数据更新1分钟K线
		return updateMin1Data(sInfo, klineData, newBasicBar, bAlignSec);
	case KP_Minute5:
		// 使用基础K线数据更新5分钟K线
		return updateMin5Data(sInfo, klineData, newBasicBar, bAlignSec);
	default:
		// 该重载版本不支持其他周期类型
		// 日线和秒级K线需要使用Tick版本的updateKlineData
		return NULL;
	}
}

/**
 * @brief 使用基础K线数据更新1分钟K线（重载版本）
 * @param sInfo 交易时段信息，用于时间计算和交易时段验证
 * @param klineData 目标1分钟K线数据对象
 * @param newBasicBar 新的基础K线数据（通常是更小周期的K线）
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSBarStruct* 返回新生成或更新的K线结构指针，无更新返回NULL
 * 
 * 该函数是1分钟K线更新的核心实现，支持从更小周期的K线数据（如秒级K线）
 * 聚合生成1分钟或多分钟K线数据。这是多级K线数据处理的基础函数。
 * 
 * 核心算法特点：
 * 1. **时间窗口对齐**：
 *    - 支持按交易时段边界对齐K线时间
 *    - 处理跨交易时段的时间计算
 *    - 自动处理夜盘等复杂时间场景
 * 
 * 2. **多周期支持**：
 *    - times=1：标准1分钟K线
 *    - times>1：多分钟K线（如5分钟、15分钟等）
 * 
 * 3. **数据聚合逻辑**：
 *    - 开盘价：使用时间窗口内第一个K线的开盘价
 *    - 最高价：时间窗口内所有K线的最高价
 *    - 最低价：时间窗口内所有K线的最低价
 *    - 收盘价：使用最新K线的收盘价
 *    - 成交量：累加时间窗口内所有K线的成交量
 *    - 成交额：累加时间窗口内所有K线的成交额
 * 
 * 4. **交易时段对齐算法**：
 *    - 获取交易时段的分钟边界列表
 *    - 使用二分查找定位当前时间所在的交易时段
 *    - 计算基于时段边界的对齐时间
 *    - 确保K线时间戳符合交易规则
 */
WTSBarStruct* WTSDataFactory::updateMin1Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSBarStruct* newBasicBar, bool bAlignSec/* = false*/)
{
	// **交易时段信息验证**
	if (sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// 获取交易时段的分钟边界列表，用于时间对齐计算
	// 这个列表包含了每个交易时段结束时的累计分钟数
	auto secMins = sInfo->getSecMinList();

	// **特殊情况：标准1分钟K线处理**
	// 当times=1时，表示标准1分钟K线，直接追加数据即可
	if(klineData->times() == 1)
	{
		klineData->appendBar(*newBasicBar);  // 直接追加新的K线数据
		klineData->setClosed(true);          // 标记K线为已闭合状态
		return klineData->at(-1);            // 返回最后一根K线
	}

	// **多分钟K线处理：计算时间步长**
	// steplen表示目标K线的分钟数（如5分钟K线的steplen为5）
	uint32_t steplen = klineData->times();

	// 获取当前基础K线的数据引用，避免重复解引用
	const WTSBarStruct& curBar = *newBasicBar;

	// **时间信息提取和转换**
	uint32_t uTradingDate = curBar.date;                    // 交易日期
	uint32_t uDate = TimeUtils::minBarToDate(curBar.time); // 从K线时间戳提取日期
	if (uDate == 19900000)  // 特殊值检查，19900000表示无效日期
		uDate = uTradingDate;  // 使用交易日期作为默认值
	uint32_t uTime = TimeUtils::minBarToTime(curBar.time);  // 从K线时间戳提取时间
	uint32_t uMinute = sInfo->timeToMinutes(uTime);         // 将时间转换为交易时段内的分钟偏移
	uint32_t uBarMin = 0;  // 计算得出的目标K线分钟偏移

	// **核心算法：按交易时段对齐的时间计算**
	// 这是处理复杂交易时间的关键逻辑，支持按交易时段边界对齐K线
	if (bAlignSec)
	{
		// **交易时段对齐模式**：
		// 1. 使用二分查找定位当前分钟在哪个交易时段
		// 2. 计算基于时段边界的对齐时间
		// 3. 确保K线时间戳符合交易规则
		
		// 使用二分查找在交易时段分钟列表中定位当前分钟位置
		auto it = std::lower_bound(secMins.begin(), secMins.end(), uMinute);
		auto secIdx = it - secMins.begin();  // 获取时段索引
		
		if (secIdx == 0)
		{
			// **第一个交易时段内**：
			// 直接基于交易开始时间进行对齐计算
			uMinute -= 1;  // 减1是为了处理边界情况
			uBarMin = (uMinute / steplen)*steplen + steplen;
			
			// 确保不超过当前时段的结束时间
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
		else
		{
			// **非第一个交易时段内**：
			// 基于上一个时段的结束时间进行对齐计算
			uMinute -= secMins[secIdx - 1];  // 减去上一时段的累计分钟数，得到当前时段内的相对分钟
			uBarMin = secMins[secIdx - 1] + (uMinute / steplen)*steplen + steplen;
			
			// 确保不超过当前时段的结束时间
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
	}
	else
	{
		// **标准对齐模式**：
		// 不考虑交易时段边界，按标准时间窗口对齐
		uMinute -= 1;  // 减1是为了处理边界情况
		uBarMin = (uMinute / steplen)*steplen + steplen;
	}

	// **时间戳计算和跨日处理**
	// 将分钟偏移转换回实际时间
	uint64_t uBarTime = sInfo->minuteToTime(uBarMin);
	
	// 检查是否发生跨日情况（如夜盘跨到第二天）
	if (uBarTime < uTime)
		uDate = TimeUtils::getNextDate(uDate, 1);  // 日期推进一天
	
	// 构建最终的K线时间戳（日期+时间的组合格式）
	uBarTime = TimeUtils::timeToMinBar(uDate, (uint32_t)uBarTime);

	// **获取当前K线数据中的最后一根K线**
	WTSBarStruct* lastBar = NULL;
	if (klineData->size() > 0)
	{
		lastBar = klineData->at(klineData->size() - 1);  // 获取最后一根K线
	}

	// **判断是否需要创建新的K线**
	bool bNewBar = false;
	if (lastBar == NULL || lastBar->date != uDate || lastBar->time != uBarTime)
	{
		// **创建新K线的条件**：
		// 1. 还没有任何K线数据（lastBar == NULL）
		// 2. 日期不匹配（跨日了）
		// 3. 时间不匹配（进入新的时间窗口）
		
		lastBar = new WTSBarStruct();  // 创建新的K线结构
		bNewBar = true;

		// 复制基础K线的所有数据到新K线
		memcpy(lastBar, &curBar, sizeof(WTSBarStruct));
		lastBar->date = uDate;      // 设置计算得出的日期
		lastBar->time = uBarTime;   // 设置计算得出的时间戳
	}
	else
	{
		// **更新现有K线**：
		// 当前基础K线属于同一个时间窗口，需要聚合到现有K线中
		bNewBar = false;

		// 更新OHLC数据（开高低收）
		lastBar->high = max(lastBar->high, curBar.high);    // 更新最高价
		lastBar->low = min(lastBar->low, curBar.low);       // 更新最低价
		lastBar->close = curBar.close;                      // 更新收盘价（使用最新价格）
		lastBar->settle = curBar.settle;                    // 更新结算价

		// 累加成交量和成交额
		lastBar->vol += curBar.vol;      // 累加成交量
		lastBar->money += curBar.money;  // 累加成交额
		lastBar->add += curBar.add;      // 累加增仓量
		lastBar->hold = curBar.hold;     // 更新持仓量（使用最新值）
	}

	// **K线闭合状态判断**
	// 根据K线时间戳和当前数据时间的关系判断K线是否已闭合
	if(lastBar->time > curBar.time)
	{
		// K线时间戳大于当前数据时间，说明K线还未闭合
		// 这种情况通常发生在时间窗口还未结束时
		klineData->setClosed(false);
	}
	else
	{
		// K线时间戳小于等于当前数据时间，说明K线已闭合
		// 这表示当前时间窗口已经结束，K线数据完整
		klineData->setClosed(true);
	}

	// **处理新创建的K线**
	if (bNewBar)
	{
		// 将新创建的K线添加到K线数据容器中
		klineData->appendBar(*lastBar);
		
		// 释放临时创建的K线结构内存
		// 因为appendBar会复制数据，所以可以安全删除临时对象
		delete lastBar;

		// 返回新添加的K线（最后一根K线）
		return klineData->at(-1);
	}

	// 如果是更新现有K线，返回NULL表示没有新增K线
	// 调用者可以通过检查返回值来判断是否有新的K线生成
	return NULL;
}

/**
 * @brief 使用Tick数据更新1分钟K线
 * @param sInfo 交易时段信息，用于时间计算和交易时段验证
 * @param klineData 目标1分钟K线数据对象
 * @param tick 新的Tick数据，包含价格、成交量等信息
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSBarStruct* 返回新生成的K线结构指针，无新增返回NULL
 * 
 * 该函数是从Tick数据生成1分钟K线的核心实现，处理实时Tick数据的聚合。
 * 这是实时行情系统中最基础也是最重要的数据处理函数。
 * 
 * 核心处理逻辑：
 * 1. **时间有效性验证**：检查Tick时间是否在有效交易时段内
 * 2. **时间窗口计算**：确定Tick属于哪个分钟K线窗口
 * 3. **数据聚合**：将Tick数据聚合到对应的K线结构中
 * 4. **边界处理**：处理交易时段边界和跨日情况
 * 
 * 特殊处理场景：
 * - 非交易时间的Tick数据：更新最后一根K线但不创建新K线
 * - 交易时段结束时间：特殊的边界时间处理
 * - 按时段对齐：确保K线时间戳符合交易时段规则
 */
WTSBarStruct* WTSDataFactory::updateMin1Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick, bool bAlignSec /* = false */)
{
	// 获取K线的时间步长（分钟数）
	uint32_t steplen = klineData->times();

	// 获取交易时段的分钟边界列表
	auto secMins = sInfo->getSecMinList();

	// **从Tick数据中提取时间信息**
	uint32_t uDate = tick->actiondate();              // Tick的日期
	uint32_t uTime = tick->actiontime() / 100000;     // Tick的时间（HHMM格式）
	uint32_t uMinute = sInfo->timeToMinutes(uTime);   // 转换为交易时段内的分钟偏移
	
	// **处理非交易时间的Tick数据**
	if(uMinute == INVALID_UINT32)
	{
		// Tick时间不在有效交易时段内，但如果有成交量，仍需更新最后一根K线
		if(tick->volume() != 0)
		{
			// 获取最后一根K线并更新其价格和成交信息
			WTSBarStruct *bar = klineData->at(klineData->size()-1);
			bar->close = tick->price();                        // 更新收盘价
			bar->high = max(bar->high,tick->price());          // 更新最高价
			bar->low = min(bar->low,tick->price());            // 更新最低价
			bar->vol += tick->volume();                        // 累加成交量
			bar->money += tick->turnover();                    // 累加成交额
			bar->hold = tick->openinterest();                  // 更新持仓量
			bar->add += tick->additional();                    // 累加增仓量
		}

		// 非交易时间不创建新K线，返回NULL
		return NULL;
	}

	// **处理交易时段结束时间的特殊情况**
	// 如果当前时间是交易时段的最后时刻，需要特殊处理
	if (sInfo->isLastOfSection(uTime))
	{
		uMinute--;  // 将时间调整到时段内，避免边界问题
	}

	uint32_t uBarMin = 0;

	/*
	 *	By Wesley @ 2023.05.31
	 *	这里是按小节对齐的核心逻辑
	 *	1、先增加一个基础分钟数，如果不按小节对齐，就固定为0
	 *	2、如果按小节对齐，则判断当前分钟处于哪个小节，然后以上个小节结束的分钟数做基础分钟数
	 *	3、然后根据基础分钟数的差量计算新的对齐分钟数
	 *	4、最终得到bar的时间戳
	 */
	if (bAlignSec)
	{
		// **交易时段对齐模式**：
		// 使用二分查找在交易时段分钟列表中定位当前分钟位置
		auto it = std::lower_bound(secMins.begin(), secMins.end(), uMinute);
		auto secIdx = it - secMins.begin();  // 获取时段索引
		
		if (secIdx == 0)
		{
			// **第一个交易时段内**：直接基于交易开始时间进行对齐计算
			uBarMin = (uMinute / steplen)*steplen + steplen;
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
		else
		{
			// **非第一个交易时段内**：基于上一个时段的结束时间进行对齐计算
			uMinute -= secMins[secIdx - 1];  // 减去上一时段的累计分钟数
			uBarMin = secMins[secIdx - 1] + (uMinute / steplen)*steplen + steplen;
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];  // 确保不超过当前时段的结束时间
		}
	}
	else
	{
		// **标准对齐模式**：按标准时间窗口对齐
		uBarMin = (uMinute / steplen)*steplen + steplen;
	}

	// **时间戳计算和跨日处理**
	uint32_t uOnlyMin = sInfo->minuteToTime(uBarMin);  // 将分钟偏移转换回时间格式
	if(uOnlyMin == 0)
	{
		// 如果计算出的时间为0（如00:00），说明跨到了下一天
		uDate = TimeUtils::getNextDate(uDate);
	}
	// 构建最终的K线时间戳
	uint64_t uBarTime = TimeUtils::timeToMinBar(uDate, uOnlyMin);

	// **获取最后一根K线的时间信息**
	uint64_t lastTime = klineData->time(-1);    // 最后一根K线的时间戳
	uint32_t lastDate = klineData->date(-1);    // 最后一根K线的日期
	
	// **判断是否需要创建新的K线**
	if (lastTime == INVALID_UINT32 || uBarTime > lastTime || tick->tradingdate() > lastDate)
	{
		// **创建新K线的条件**：
		// 1. 还没有任何K线数据
		// 2. 新的时间戳大于最后一根K线的时间戳
		// 3. Tick的交易日期大于最后一根K线的日期
		
		// 创建新的K线结构
		WTSBarStruct *day = new WTSBarStruct;
		day->date = tick->tradingdate();   // 设置交易日期
		day->time = uBarTime;              // 设置计算得出的时间戳
		
		// 初始化OHLC数据
		day->open = tick->price();         // 开盘价
		day->high = tick->price();         // 最高价
		day->low = tick->price();          // 最低价
		day->close = tick->price();        // 收盘价
		
		// 初始化成交和持仓数据
		day->vol = tick->volume();         // 成交量
		day->money = tick->turnover();     // 成交额
		day->hold = tick->openinterest();  // 持仓量
		day->add = tick->additional();     // 增仓量

		// 将新K线添加到数据容器中
		klineData->appendBar(*day);
		delete day;  // 释放临时创建的结构

		// 返回新添加的K线
		return klineData->at(-1);
	}
	else if (lastTime != INVALID_UINT32 && uBarTime < lastTime)
	{
		// **时间倒退的异常情况**：
		// 这种情况主要是为了防止日期反复出现或时间错乱
		// 通常发生在数据源时间不准确或系统时间调整时
		return NULL;  // 忽略时间倒退的数据
	}
	else
	{
		// **更新现有K线**：
		// 当前Tick属于同一个时间窗口，更新现有K线的数据
		
		WTSBarStruct *bar = klineData->at(klineData->size()-1);
		
		// 更新价格数据
		bar->close = tick->price();                    // 更新收盘价
		bar->high = max(bar->high,tick->price());      // 更新最高价
		bar->low = min(bar->low,tick->price());        // 更新最低价
		
		// 累加成交和持仓数据
		bar->vol += tick->volume();                    // 累加成交量
		bar->money += tick->turnover();                // 累加成交额
		bar->hold = tick->openinterest();              // 更新持仓量
		bar->add += tick->additional();                // 累加增仓量

		// 更新现有K线，不返回新结构
		return NULL;
	}
}

/**
 * @brief 使用基础K线数据更新5分钟K线
 * @param sInfo 交易时段信息，用于时间计算和交易时段验证
 * @param klineData 目标5分钟K线数据对象
 * @param newBasicBar 新的基础K线数据（通常是1分钟K线）
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSBarStruct* 返回新生成或更新的K线结构指针，无更新返回NULL
 * 
 * 该函数用于将基础周期K线（通常是1分钟K线）聚合为5分钟或5分钟倍数的K线数据。
 * 这是多级K线数据处理中的重要环节，支持从分钟级到更大时间周期的转换。
 * 
 * 算法特点：
 * 1. **5分钟基础步长**：以5分钟为基础单位进行时间窗口计算
 * 2. **倍数支持**：支持5分钟的整数倍（如15分钟、30分钟、60分钟等）
 * 3. **时段对齐**：支持按交易时段边界对齐K线时间
 * 4. **数据聚合**：正确聚合OHLCV数据
 * 
 * 与1分钟K线更新的区别：
 * - 时间步长计算：steplen = 5 × times（而不是直接使用times）
 * - 适用场景：主要用于中等周期的K线生成
 * - 数据精度：相比1分钟K线，数据密度较低但计算效率更高
 * 
 * 支持的周期：
 * - times=1：标准5分钟K线
 * - times=3：15分钟K线
 * - times=6：30分钟K线
 * - times=12：60分钟K线
 */
WTSBarStruct* WTSDataFactory::updateMin5Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSBarStruct* newBasicBar, bool bAlignSec/* = false*/)
{
	// **交易时段信息验证**
	if (sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// 获取交易时段的分钟边界列表
	auto secMins = sInfo->getSecMinList();

	// **特殊情况：标准5分钟K线处理**
	if (klineData->times() == 1)
	{
		// 当times=1时，表示标准5分钟K线，直接追加数据即可
		klineData->appendBar(*newBasicBar);
		return klineData->at(-1);
	}

	// **计算5分钟基础的时间步长**
	// 注意：这里是5分钟的倍数，所以steplen = 5 × times
	// 例如：times=3时，steplen=15，表示15分钟K线
	uint32_t steplen = 5 * klineData->times();

	// 获取当前基础K线的数据引用
	const WTSBarStruct& curBar = *newBasicBar;

	// **时间信息提取和转换**
	uint32_t uTradingDate = curBar.date;                    // 交易日期
	uint32_t uDate = TimeUtils::minBarToDate(curBar.time); // 从K线时间戳提取日期
	if (uDate == 19900000)  // 无效日期检查
		uDate = uTradingDate;
	uint32_t uTime = TimeUtils::minBarToTime(curBar.time);  // 从K线时间戳提取时间
	uint32_t uMinute = sInfo->timeToMinutes(uTime);         // 转换为交易时段内的分钟偏移
	uint32_t uBarMin = 0;  // 计算得出的目标K线分钟偏移

	// **核心算法：按交易时段对齐的时间计算**
	// 这是处理复杂交易时间的关键逻辑，支持按交易时段边界对齐K线
	// 算法步骤：
	// 1. 先确定基础分钟数（如果不按小节对齐，就固定为0）
	// 2. 如果按小节对齐，则判断当前分钟处于哪个小节，然后以上个小节结束的分钟数做基础分钟数
	// 3. 然后根据基础分钟数的差量计算新的对齐分钟数
	// 4. 最终得到K线的时间戳
	if (bAlignSec)
	{
		// **交易时段对齐模式**
		auto it = std::lower_bound(secMins.begin(), secMins.end(), uMinute);
		auto secIdx = it - secMins.begin();
		if (secIdx == 0)
		{
			// **第一个交易时段内**
			uMinute -= 5;  // 5分钟K线的基础偏移
			uBarMin = (uMinute / steplen)*steplen + steplen;
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
		else
		{
			// **非第一个交易时段内**
			uMinute -= secMins[secIdx - 1];
			uBarMin = secMins[secIdx - 1] + (uMinute / steplen)*steplen + steplen;
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
	}
	else
	{
		// **标准对齐模式**
		uMinute -= 5;  // 5分钟K线的标准偏移
		uBarMin = (uMinute / steplen)*steplen + steplen;
	}

	// **时间戳计算和跨日处理**
	uint64_t uBarTime = sInfo->minuteToTime(uBarMin);
	if (uBarTime < uTime)
		uDate = TimeUtils::getNextDate(uDate, 1);  // 处理跨日情况
	uBarTime = TimeUtils::timeToMinBar(uDate, (uint32_t)uBarTime);

	// **获取当前K线数据中的最后一根K线**
	WTSBarStruct* lastBar = NULL;
	if (klineData->size() > 0)
	{
		lastBar = klineData->at(klineData->size() - 1);
	}

	// **判断是否需要创建新的K线**
	bool bNewBar = false;
	if (lastBar == NULL || lastBar->date != uDate || lastBar->time != uBarTime)
	{
		// **创建新K线的条件**：日期或时间不匹配
		lastBar = new WTSBarStruct();
		bNewBar = true;

		// 复制基础K线数据到新K线
		memcpy(lastBar, &curBar, sizeof(WTSBarStruct));
		lastBar->date = uTradingDate;  // 设置交易日期
		lastBar->time = uBarTime;      // 设置计算得出的时间戳
	}
	else
	{
		// **更新现有K线**
		bNewBar = false;

		// 更新OHLC数据
		lastBar->high = max(lastBar->high, curBar.high);    // 更新最高价
		lastBar->low = min(lastBar->low, curBar.low);       // 更新最低价
		lastBar->close = curBar.close;                      // 更新收盘价
		lastBar->settle = curBar.settle;                    // 更新结算价

		// 累加成交量和成交额
		lastBar->vol += curBar.vol;      // 累加成交量
		lastBar->money += curBar.money;  // 累加成交额
		lastBar->add += curBar.add;      // 累加增仓量
		lastBar->hold = curBar.hold;     // 更新持仓量
	}

	// **K线闭合状态判断**
	if (lastBar->time > curBar.time)
	{
		// K线时间戳大于当前数据时间，K线未闭合
		klineData->setClosed(false);
	}
	else
	{
		// K线时间戳小于等于当前数据时间，K线已闭合
		klineData->setClosed(true);
	}

	// **处理新创建的K线**
	if (bNewBar)
	{
		// 将新创建的K线添加到数据容器中
		klineData->appendBar(*lastBar);
		delete lastBar;  // 释放临时结构

		return klineData->at(-1);
	}

	return NULL;
}

/**
 * @brief 使用Tick数据更新5分钟K线
 * @param sInfo 交易时段信息，用于时间计算和交易时段验证
 * @param klineData 目标5分钟K线数据对象
 * @param tick 新的Tick数据，包含价格、成交量等信息
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSBarStruct* 返回新生成的K线结构指针，无新增返回NULL
 * 
 * 该函数是从Tick数据生成5分钟K线的核心实现，处理实时Tick数据的聚合。
 * 与1分钟K线类似，但使用5分钟作为基础时间单位。
 * 
 * 核心特点：
 * 1. **5分钟基础窗口**：以5分钟为基础时间窗口
 * 2. **倍数支持**：支持5分钟的整数倍周期
 * 3. **时段对齐**：支持按交易时段边界对齐
 * 4. **高效聚合**：相比1分钟K线，数据密度较低但处理效率更高
 */
WTSBarStruct* WTSDataFactory::updateMin5Data(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick, bool bAlignSec /* = false */)
{
	// 获取交易时段的分钟边界列表
	auto secMins = sInfo->getSecMinList();

	// **计算5分钟基础的时间步长**
	uint32_t steplen = 5*klineData->times();

	// **从Tick数据中提取时间信息**
	uint32_t uDate = tick->actiondate();              // Tick的日期
	uint32_t uTime = tick->actiontime()/100000;       // Tick的时间（HHMM格式）
	uint32_t uMinute = sInfo->timeToMinutes(uTime);   // 转换为交易时段内的分钟偏移
	
	// **处理交易时段结束时间的特殊情况**
	if (sInfo->isLastOfSection(uTime))
	{
		uMinute--;  // 调整到时段内，避免边界问题
	}

	uint32_t uBarMin = 0;  // 计算得出的目标K线分钟偏移

	// **核心算法：按交易时段对齐的时间计算**
	// 这是处理复杂交易时间的关键逻辑，支持按交易时段边界对齐K线
	if (bAlignSec)
	{
		// **交易时段对齐模式**
		auto it = std::lower_bound(secMins.begin(), secMins.end(), uMinute);
		auto secIdx = it - secMins.begin();
		if (secIdx == 0)
		{
			// **第一个交易时段内**
			uBarMin = (uMinute / steplen)*steplen + steplen;
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
		else
		{
			// **非第一个交易时段内**
			uMinute -= secMins[secIdx - 1];
			uBarMin = secMins[secIdx - 1] + (uMinute / steplen)*steplen + steplen;
			if (uBarMin > secMins[secIdx])
				uBarMin = secMins[secIdx];
		}
	}
	else
	{
		// **标准对齐模式**
		uBarMin = (uMinute / steplen)*steplen + steplen;
	}

	// **时间戳计算和跨日处理**
	uint32_t uOnlyMin = sInfo->minuteToTime(uBarMin);
	if (uOnlyMin == 0)
	{
		// 如果计算出的时间为0，说明跨到了下一天
		uDate = TimeUtils::getNextDate(uDate);
	}
	// 构建最终的K线时间戳
	uint64_t uBarTime = TimeUtils::timeToMinBar(uDate, uOnlyMin);

	// **获取最后一根K线的时间戳**
	uint64_t lastTime = klineData->time(klineData->size()-1);
	
	// **判断是否需要创建新的K线**
	if(lastTime == INVALID_UINT32 || uBarTime != lastTime)
	{
		// **创建新K线的条件**：没有K线数据或时间戳不匹配
		
		// 创建新的K线结构
		WTSBarStruct *day = new WTSBarStruct;
		day->date = tick->tradingdate();   // 设置交易日期
		day->time = uBarTime;              // 设置计算得出的时间戳
		
		// 初始化OHLC数据
		day->open = tick->price();         // 开盘价
		day->high = tick->price();         // 最高价
		day->low = tick->price();          // 最低价
		day->close = tick->price();        // 收盘价
		
		// 初始化成交和持仓数据
		day->vol = tick->volume();         // 成交量
		day->money = tick->turnover();     // 成交额
		day->hold = tick->openinterest();  // 持仓量
		day->add = tick->additional();     // 增仓量

		// 将新K线添加到数据容器中
		klineData->appendBar(*day);
		delete day;  // 释放临时结构

		// 返回新添加的K线
		return klineData->at(-1);
	}
	else
	{
		// **更新现有K线**：当前Tick属于同一个时间窗口
		
		WTSBarStruct *bar = klineData->at(klineData->size()-1);
		
		// 更新价格数据
		bar->close = tick->price();                    // 更新收盘价
		bar->high = max(bar->high,tick->price());      // 更新最高价
		bar->low = min(bar->low,tick->price());        // 更新最低价
		
		// 累加成交和持仓数据
		bar->vol += tick->volume();                    // 累加成交量
		bar->money += tick->turnover();                // 累加成交额
		bar->hold = tick->openinterest();              // 更新持仓量
		bar->add = tick->additional();                 // 累加增仓量

		// 更新现有K线，不返回新结构
		return NULL;
	}
}

/**
 * @brief 使用Tick数据更新日线数据
 * @param sInfo 交易时段信息（日线更新中主要用于验证）
 * @param klineData 目标日线数据对象
 * @param tick 新的Tick数据，包含价格、成交量等信息
 * @return WTSBarStruct* 返回新创建的日线结构指针，更新现有日线返回NULL
 * 
 * 该函数用于将Tick数据聚合为日线数据，是日线数据生成的核心函数。
 * 日线数据的聚合逻辑相对简单，主要基于交易日期进行分组。
 * 
 * 日线聚合逻辑：
 * 1. **日期比较**：比较Tick的交易日期与最后一根日线的日期
 * 2. **新日线创建**：如果日期不同，创建新的日线数据
 * 3. **现有日线更新**：如果日期相同，更新现有日线的OHLCV数据
 * 
 * 日线数据特点：
 * - 时间字段为0（日线不需要具体时间）
 * - 开盘价：当日第一个Tick的价格
 * - 最高价：当日所有Tick的最高价格
 * - 最低价：当日所有Tick的最低价格
 * - 收盘价：当日最后一个Tick的价格
 * - 成交量：当日所有Tick成交量的累加
 * - 成交额：当日所有Tick成交额的累加
 * 
 * 与分钟K线的区别：
 * - 日线基于交易日期分组，不考虑具体时间
 * - 不需要复杂的时间窗口计算
 * - 不需要处理交易时段边界对齐
 * - 数据更新频率较低，性能要求不高
 */
WTSBarStruct* WTSDataFactory::updateDayData(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick)
{
	// 获取Tick数据的交易日期
	uint32_t curDate = tick->tradingdate();
	
	// 获取最后一根日线的日期（如果存在）
	uint32_t lastDate = klineData->date(klineData->size()-1);

	// **判断是否需要创建新的日线**
	if(lastDate == INVALID_UINT32 || curDate != lastDate)
	{
		// **创建新日线的条件**：
		// 1. 还没有任何日线数据（lastDate == INVALID_UINT32）
		// 2. 交易日期发生变化（curDate != lastDate）
		
		// 创建新的日线结构
		WTSBarStruct *day = new WTSBarStruct;
		day->date = curDate;           // 设置交易日期
		day->time = 0;                 // 日线时间字段为0
		
		// 初始化OHLC数据（开高低收都使用当前Tick价格）
		day->open = tick->price();     // 开盘价
		day->high = tick->price();     // 最高价
		day->low = tick->price();      // 最低价
		day->close = tick->price();    // 收盘价
		
		// 初始化成交和持仓数据
		day->vol = tick->volume();     // 成交量
		day->money = tick->turnover(); // 成交额
		day->hold = tick->openinterest(); // 持仓量
		day->add = tick->additional(); // 增仓量

		// 返回新创建的日线结构，调用者需要将其添加到K线数据中
		return day;
	}
	else
	{
		// **更新现有日线**：
		// 当前Tick属于同一个交易日，需要更新现有日线的数据
		
		// 获取最后一根日线
		WTSBarStruct *bar = klineData->at(klineData->size()-1);
		
		// 更新价格数据
		bar->close = tick->price();                    // 更新收盘价（使用最新价格）
		bar->high = max(bar->high,tick->price());      // 更新最高价
		bar->low = min(bar->low,tick->price());        // 更新最低价
		
		// 累加成交和持仓数据
		bar->vol += tick->volume();                    // 累加成交量
		bar->money += tick->turnover();                // 累加成交额
		bar->hold = tick->openinterest();              // 更新持仓量（使用最新值）
		bar->add += tick->additional();                // 累加增仓量

		// 更新现有日线，不需要返回新结构
		return NULL;
	}
}

/**
 * @brief 使用Tick数据更新秒级K线数据
 * @param sInfo 交易时段信息，用于秒级时间计算
 * @param klineData 目标秒级K线数据对象
 * @param tick 新的Tick数据，包含价格、成交量等信息
 * @return WTSBarStruct* 返回新创建的秒级K线结构指针，更新现有K线返回NULL
 * 
 * 该函数用于将Tick数据聚合为秒级K线数据，是高频数据处理的核心函数。
 * 秒级K线主要用于高频交易策略和精细的价格分析。
 * 
 * 秒级K线特点：
 * 1. **高精度时间**：支持秒级的时间精度
 * 2. **快速更新**：需要处理高频的Tick数据流
 * 3. **时间窗口**：基于秒级时间窗口进行数据聚合
 * 4. **Unix时间支持**：可选择使用Unix时间戳格式
 * 
 * 时间计算逻辑：
 * 1. 将Tick时间转换为交易时段内的秒偏移
 * 2. 计算目标秒级K线的时间窗口
 * 3. 处理跨日情况和时间格式转换
 * 4. 生成标准化的K线时间戳
 * 
 * 聚合算法：
 * - 开盘价：时间窗口内第一个Tick的价格
 * - 最高价：时间窗口内所有Tick的最高价格
 * - 最低价：时间窗口内所有Tick的最低价格
 * - 收盘价：时间窗口内最后一个Tick的价格
 * - 成交量：累加时间窗口内所有Tick的成交量
 */
WTSBarStruct* WTSDataFactory::updateSecData(WTSSessionInfo* sInfo, WTSKlineData* klineData, WTSTickData* tick)
{
	// 获取目标秒级K线的时间窗口大小（秒数）
	uint32_t seconds = klineData->times();
	
	// 将Tick时间转换为交易时段内的秒偏移
	// actiontime格式为HHMMSSMMM，除以1000得到HHMMSS格式
	uint32_t curSeconds = sInfo->timeToSeconds(tick->actiontime()/1000);
	
	// 计算目标K线的秒偏移（时间窗口对齐）
	// 算法：(当前秒数/窗口大小)*窗口大小 + 窗口大小
	uint32_t barSeconds = (curSeconds/seconds)*seconds + seconds;
	
	// 将秒偏移转换回时间格式
	uint32_t barTime = sInfo->secondsToTime(barSeconds);

	// **处理Unix时间格式**
	if(klineData->isUnixTime())
	{
		uint32_t uDate = tick->actiondate();
		
		// 检查是否发生跨日情况
		if (barTime < tick->actiontime() / 1000)
			uDate = TimeUtils::getNextDate(uDate);  // 日期推进一天
			
		// 转换为Unix时间戳格式
		barTime = (uint32_t)(TimeUtils::makeTime(uDate, barTime * 1000) / 1000);
	}	

	// **判断是否需要创建新的秒级K线**
	uint64_t lastTime = klineData->time(klineData->size()-1);
	if(lastTime == INVALID_UINT32 || lastTime != barTime)
	{
		// **创建新秒级K线的条件**：
		// 1. 还没有任何K线数据（lastTime == INVALID_UINT32）
		// 2. 时间戳不匹配（进入新的时间窗口）
		
		// 创建新的秒级K线结构
		WTSBarStruct *day = new WTSBarStruct;
		day->date = tick->tradingdate();   // 设置交易日期
		day->time = barTime;               // 设置计算得出的时间戳
		
		// 初始化OHLC数据（开高低收都使用当前Tick价格）
		day->open = tick->price();         // 开盘价
		day->high = tick->price();         // 最高价
		day->low = tick->price();          // 最低价
		day->close = tick->price();        // 收盘价
		
		// 初始化成交和持仓数据
		day->vol = tick->volume();         // 成交量
		day->money = tick->turnover();     // 成交额
		day->hold = tick->openinterest();  // 持仓量
		day->add = tick->additional();     // 增仓量

		// 返回新创建的秒级K线结构
		return day;
	}
	else
	{
		// **更新现有秒级K线**：
		// 当前Tick属于同一个时间窗口，需要更新现有K线的数据
		
		// 获取最后一根秒级K线
		WTSBarStruct *bar = klineData->at(klineData->size()-1);
		
		// 更新价格数据
		bar->close = tick->price();                    // 更新收盘价（使用最新价格）
		bar->high = max(bar->high,tick->price());      // 更新最高价
		bar->low = min(bar->low,tick->price());        // 更新最低价
		
		// 累加成交和持仓数据
		bar->vol += tick->volume();                    // 累加成交量
		bar->money += tick->turnover();                // 累加成交额
		bar->hold = tick->openinterest();              // 更新持仓量（使用最新值）
		bar->add += tick->additional();                // 累加增仓量

		// 更新现有K线，不需要返回新结构
		return NULL;
	}
}

/**
 * @brief 获取前N分钟的时间
 * @param curMinute 当前时间（HHMM格式）
 * @param period 向前推进的分钟数，默认为1
 * @return uint32_t 前N分钟的时间（HHMM格式）
 * 
 * 该函数是一个时间计算的辅助工具，用于计算指定时间之前N分钟的时间。
 * 主要用于K线时间窗口的边界计算和时间对齐处理。
 * 
 * 算法逻辑：
 * 1. 将HHMM格式的时间分解为小时和分钟
 * 2. 处理分钟借位的情况（如从00:00向前推进）
 * 3. 处理小时借位的情况（如从00:XX向前推进）
 * 4. 返回计算结果的HHMM格式时间
 * 
 * 边界处理：
 * - 分钟为0时：向前一小时借位
 * - 小时为0时：回到24小时制的前一天
 * 
 * 应用场景：
 * - K线时间窗口计算
 * - 时间边界对齐
 * - 历史时间点查找
 */
uint32_t WTSDataFactory::getPrevMinute(uint32_t curMinute, int period /* = 1 */)
{
    // 1. 将 HHMM 格式转换为从午夜0点开始的总分钟数
    int32_t totalMinutes = (curMinute / 100) * 60 + (curMinute % 100);

    // 2. 执行分钟数的减法
    totalMinutes -= period;

    // 3. 处理跨天的情况（结果为负数）
    // C++的取模运算对负数结果依赖于具体实现，所以用循环更安全
    while (totalMinutes < 0)
    {
        totalMinutes += 1440; // 1440 = 24 * 60，加上一天的分钟数
    }

    // 确保结果在一天之内
    totalMinutes %= 1440;

    // 4. 将总分钟数转换回 HHMM 格式
    uint32_t newHour = totalMinutes / 60;
    uint32_t newMinute = totalMinutes % 60;

    return newHour * 100 + newMinute;
}

/**
 * @brief 从基础周期K线数据提取目标周期K线（主入口函数）
 * @param baseKline 基础周期K线数据切片，作为数据源
 * @param period 目标K线周期类型（KP_Minute1/KP_Minute5/KP_DAY等）
 * @param times 周期倍数（如5分钟K线的times为5）
 * @param sInfo 交易时段信息，用于时间计算和验证
 * @param bIncludeOpen 是否包含未闭合的K线，默认为true
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSKlineData* 返回提取后的K线数据对象，失败返回NULL
 * 
 * 该函数是数据提取的核心入口函数，负责从基础周期的K线数据中提取出指定周期的K线数据。
 * 这是历史数据处理和回测系统中的关键功能，用于生成各种周期的K线数据。
 * 
 * 核心功能：
 * 1. **周期转换**：将小周期K线聚合为大周期K线
 * 2. **时间对齐**：支持按交易时段边界对齐K线时间
 * 3. **数据完整性**：确保提取的K线数据完整且连续
 * 4. **开放控制**：可选择是否包含最后一根未完成的K线
 * 
 * 支持的周期转换：
 * - 基础周期 -> 日线（任意基础周期都可以转换为日线）
 * - 基础周期 -> 1分钟的倍数（如5分钟、15分钟、30分钟等）
 * - 基础周期 -> 5分钟的倍数（如15分钟、30分钟、60分钟等）
 * 
 * 应用场景：
 * - 历史数据服务：为策略提供各种周期的历史K线
 * - 回测引擎：生成回测所需的K线数据
 * - 数据分析：为量化分析提供标准化的K线数据
 * - 图表显示：为交易界面提供不同周期的K线图
 */
WTSKlineData* WTSDataFactory::extractKlineData(WTSKlineSlice* baseKline, WTSKlinePeriod period, uint32_t times, WTSSessionInfo* sInfo, 
		bool bIncludeOpen /* = true */, bool bAlignSec /* = false */)
{
	// **输入数据有效性验证**
	if(baseKline == NULL || baseKline->size() == 0)
		return NULL;  // 基础K线数据为空或无数据，无法提取

	// **周期参数验证**
	// times=1或Tick周期不需要转换，直接返回NULL
	if(times <= 1 || period == KP_Tick)
	{
		return NULL;  // 不需要转换的情况
	}

	// **根据目标周期类型分发处理**
	if(period == KP_DAY)
	{
		// 提取日线数据，日线提取不需要交易时段信息
		return extractDayData(baseKline, times, bIncludeOpen);
	}
	else if(period == KP_Minute1)
	{
		// 提取1分钟或多分钟K线数据
		return extractMin1Data(baseKline, times, sInfo, bIncludeOpen, bAlignSec);
	}
	else if(period == KP_Minute5)
	{
		// 提取5分钟或5分钟倍数的K线数据
		return extractMin5Data(baseKline, times, sInfo, bIncludeOpen, bAlignSec);
	}
	
	// 不支持的周期类型
	return NULL;
}

/**
 * @brief 从基础K线数据提取1分钟K线数据
 * @param baseKline 基础周期K线数据切片（通常是更小周期的K线）
 * @param times 目标K线的分钟倍数（如5表示5分钟K线）
 * @param sInfo 交易时段信息，用于时间计算和验证
 * @param bIncludeOpen 是否包含未闭合的K线，默认为true
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSKlineData* 返回提取后的1分钟K线数据，失败返回NULL
 * 
 * 该函数用于从基础周期K线数据中提取1分钟或多分钟K线数据，是历史数据处理的核心函数。
 * 主要用于将更小周期的K线（如秒级K线）聚合为分钟级K线。
 * 
 * 核心功能：
 * 1. **批量数据处理**：一次性处理整个K线数据切片
 * 2. **时间窗口聚合**：按指定的分钟倍数进行时间窗口聚合
 * 3. **交易时段对齐**：支持按交易时段边界对齐K线时间
 * 4. **数据完整性控制**：可选择是否包含最后一根未完成的K线
 * 
 * 算法特点：
 * - 遍历所有基础K线数据
 * - 为每根基础K线计算目标时间窗口
 * - 将同一时间窗口的K线数据进行聚合
 * - 生成符合目标周期的完整K线数据
 * 
 * 与实时更新的区别：
 * - extractMin1Data：批量处理历史数据，一次性生成完整的K线序列
 * - updateMin1Data：实时处理单个数据，增量更新现有K线
 */
WTSKlineData* WTSDataFactory::extractMin1Data(WTSKlineSlice* baseKline, uint32_t times, WTSSessionInfo* sInfo, bool bIncludeOpen /* = true */, bool bAlignSec /* = false */)
{
	// **交易时段信息验证**
	if(sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// **计算时间步长**
	// steplen表示目标K线的分钟数
	uint32_t steplen = times;

	// **获取交易时段对齐信息**
	// 用于支持按交易时段边界对齐的重采样方式
	// 一般逻辑：每个小节开始重新计算条数，然后在小节结束时强制对齐
	auto secMins = sInfo->getSecMinList();

	// **创建结果K线数据对象**
	WTSKlineData* ret = WTSKlineData::create(baseKline->code(), 0);
	ret->setPeriod(KP_Minute1, times);  // 设置为1分钟基础的times倍周期

	// **遍历所有基础K线数据进行聚合**
	for (auto i = 0; i < baseKline->size(); i++)  // 遍历基础K线切片中的每一根K线
	{
		// 获取当前基础K线数据：从基础K线切片中获取第i个K线数据
		const WTSBarStruct& curBar = *baseKline->at(i);

		// **时间信息提取和转换**
		uint32_t uTradingDate = curBar.date;                    // 交易日期：从基础K线数据中获取交易日期
		uint32_t uDate = TimeUtils::minBarToDate(curBar.time); // 从K线时间戳提取日期：将时间戳转换为YYYYMMDD格式
		if(uDate == 19900000)  // 无效日期检查：19900000表示无效日期，使用交易日期作为替代
			uDate = uTradingDate;
		uint32_t uTime = TimeUtils::minBarToTime(curBar.time);  // 从K线时间戳提取时间：将时间戳转换为HHMM格式
		uint32_t uMinute = sInfo->timeToMinutes(uTime);         // 转换为交易时段内的分钟偏移：将绝对时间转换为从开盘开始的分钟数
		uint32_t uBarMin = 0;  // 计算得出的目标K线分钟偏移：用于确定当前基础K线属于哪个目标K线

		// **核心算法：按交易时段对齐的时间计算**
		// 这是处理复杂交易时间的关键逻辑，支持按交易时段边界对齐K线
		if(bAlignSec)
		{
			// **交易时段对齐模式**
			auto it = std::lower_bound(secMins.begin(), secMins.end(), uMinute);  // 使用二分查找定位当前分钟数所在的交易时段
			auto secIdx = it - secMins.begin();  // 计算当前分钟数对应的交易时段索引
			if(secIdx == 0)
			{
				// **第一个交易时段内**
				uMinute -= 1;  // 减1处理边界情况：1分钟K线的基础偏移
				uBarMin = (uMinute / steplen)*steplen + steplen;  // 计算目标K线的分钟偏移：按步长对齐并加上一个步长
				if (uBarMin > secMins[secIdx])
					uBarMin = secMins[secIdx];  // 确保不超过时段边界：如果计算结果超过当前时段，则限制在时段边界
			}
			else
			{
				// **非第一个交易时段内**
				uMinute -= secMins[secIdx - 1];  // 减去前一个时段的累计分钟数，得到在当前时段内的相对分钟数
				uBarMin = secMins[secIdx - 1] + (uMinute / steplen)*steplen + steplen;  // 计算目标K线的绝对分钟偏移
				if (uBarMin > secMins[secIdx])
					uBarMin = secMins[secIdx];  // 确保不超过当前时段边界
			}
		}
		else
		{
			// **普通模式：不按交易时段对齐**
			uMinute -= 1;  // 减去1分钟基础偏移，对齐到1分钟边界
			uBarMin = (uMinute / steplen)*steplen + steplen;  // 计算目标K线的分钟偏移：按步长对齐并加上一个步长
		}

		uint64_t uBarTime = sInfo->minuteToTime(uBarMin);  // 将目标K线的分钟偏移转换为绝对时间（HHMM格式）
		if (uBarTime < uTime)  // 如果目标K线时间早于当前基础K线时间，说明跨日了
			uDate = TimeUtils::getNextDate(uDate, 1);  // 将日期推进到下一天
		uBarTime = TimeUtils::timeToMinBar(uDate, (uint32_t)uBarTime);  // 将日期和时间组合成完整的时间戳

		WTSBarStruct* lastBar = NULL;  // 指向结果K线数据中最后一条K线的指针
		if(ret->size() > 0)  // 如果结果K线数据不为空
		{
			lastBar = ret->at(ret->size()-1);  // 获取最后一条K线数据
		}

		bool bNewBar = false;  // 标记是否需要创建新的K线
		if(lastBar == NULL || lastBar->time != uBarTime)  // 如果没有最后一条K线或时间不匹配
		{
			//只要日期和时间都不符,则认为已经是一条新的bar了
			lastBar = new WTSBarStruct();  // 创建新的K线结构
			bNewBar = true;  // 标记为新K线

			memcpy(lastBar, &curBar, sizeof(WTSBarStruct));  // 复制基础K线数据到新K线
			lastBar->date = uDate;  // 设置交易日期
			lastBar->time = uBarTime;  // 设置目标K线时间
		}
		else
		{
			// **更新现有K线数据**
			bNewBar = false;  // 标记为更新现有K线

			lastBar->high = max(lastBar->high, curBar.high);  // 更新最高价：取当前最高价和基础K线最高价的较大值
			lastBar->low = min(lastBar->low, curBar.low);  // 更新最低价：取当前最低价和基础K线最低价的较小值
			lastBar->close = curBar.close;  // 更新收盘价：使用基础K线的收盘价
			lastBar->settle = curBar.settle;  // 更新结算价：使用基础K线的结算价

			lastBar->vol += curBar.vol;  // 累加成交量：将基础K线成交量加到目标K线
			lastBar->money += curBar.money;  // 累加成交金额：将基础K线成交金额加到目标K线
			lastBar->add += curBar.add;  // 累加持仓变化：将基础K线持仓变化加到目标K线
			lastBar->hold = curBar.hold;  // 更新持仓量：使用基础K线的持仓量
		}

		if(bNewBar)  // 如果是新K线
		{
			ret->appendBar(*lastBar);  // 将新K线添加到结果数据中
			delete lastBar;  // 删除临时K线对象，避免内存泄漏
		}
	}

	//检查最后一条数据：处理未闭合的K线
	{
		WTSBarStruct* lastRawBar = baseKline->at(-1);  // 获取基础K线数据的最后一条
		WTSBarStruct* lastDesBar = ret->at(-1);  // 获取目标K线数据的最后一条
		//如果目标K线的最后一条数据的日期或者时间大于原始K线最后一条的日期或时间
		if ( lastDesBar->date > lastRawBar->date || lastDesBar->time > lastRawBar->time)
		{
			if (!bIncludeOpen)  // 如果不包含未闭合的K线
				ret->getDataRef().resize(ret->size() - 1);  // 删除最后一条未闭合的K线
			else  // 如果包含未闭合的K线
				ret->setClosed(false);  // 标记最后一条K线为未闭合状态
		}
	}
	

	return ret;  // 返回生成的K线数据对象
}

/**
 * @brief 从基础K线数据提取5分钟K线数据
 * @param baseKline 基础周期K线数据切片（通常是1分钟K线）
 * @param times 目标K线的5分钟倍数（如3表示15分钟K线）
 * @param sInfo 交易时段信息，用于时间计算和验证
 * @param bIncludeOpen 是否包含未闭合的K线，默认为true
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSKlineData* 返回提取后的5分钟K线数据，失败返回NULL
 * 
 * 该函数用于从基础周期K线数据中提取5分钟或5分钟倍数的K线数据，是历史数据处理的核心函数。
 * 主要用于将更小周期的K线（如1分钟K线）聚合为5分钟基础的更大周期K线。
 * 
 * 核心功能：
 * 1. **5分钟基础聚合**：以5分钟为基础单位进行时间窗口聚合
 * 2. **倍数周期支持**：支持5分钟的整数倍（15分钟、30分钟、60分钟等）
 * 3. **交易时段对齐**：支持按交易时段边界对齐K线时间
 * 4. **批量数据处理**：一次性处理整个K线数据切片
 * 
 * 算法特点：
 * - 时间步长：steplen = 5 × times（5分钟的倍数）
 * - 遍历所有基础K线数据
 * - 为每根基础K线计算目标5分钟窗口
 * - 将同一时间窗口的K线数据进行聚合
 * - 生成符合目标周期的完整K线数据
 * 
 * 与1分钟K线提取的区别：
 * - extractMin5Data：以5分钟为基础单位，适用于中等周期
 * - extractMin1Data：以1分钟为基础单位，适用于短周期
 * 
 * 支持的周期转换：
 * - times=1：标准5分钟K线
 * - times=3：15分钟K线（5×3）
 * - times=6：30分钟K线（5×6）
 * - times=12：60分钟K线（5×12）
 */
WTSKlineData* WTSDataFactory::extractMin5Data(WTSKlineSlice* baseKline, uint32_t times, WTSSessionInfo* sInfo, bool bIncludeOpen /* = true */, bool bAlignSec /* = false */)
{
	// **交易时段信息验证**
	if(sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// **计算5分钟基础的时间步长**
	// steplen表示目标K线的分钟数（5分钟的倍数）
	uint32_t steplen = 5*times;
	
	// **获取交易时段对齐信息**
	// 用于支持按交易时段边界对齐的重采样方式
	// 一般逻辑：每个小节开始重新计算条数，然后在小节结束时强制对齐
	auto secMins = sInfo->getSecMinList();

	// **创建结果K线数据对象**
	WTSKlineData* ret = WTSKlineData::create(baseKline->code(), 0);  // 创建新的K线数据对象，使用基础K线的合约代码
	ret->setPeriod(KP_Minute5, times);  // 设置为5分钟基础的times倍周期

	// **遍历所有基础K线数据进行聚合**
	for (auto i = 0; i < baseKline->size(); i++)  // 遍历基础K线切片中的每一根K线
	{
		// 获取当前基础K线数据：从基础K线切片中获取第i个K线数据
		const WTSBarStruct& curBar = *baseKline->at(i);

		// **时间信息提取和转换**
		uint32_t uTradingDate = curBar.date;                    // 交易日期：从基础K线数据中获取交易日期
		uint32_t uDate = TimeUtils::minBarToDate(curBar.time); // 从K线时间戳提取日期：将时间戳转换为YYYYMMDD格式
		if(uDate == 19900000)  // 无效日期检查：19900000表示无效日期，使用交易日期作为替代
			uDate = uTradingDate;
		uint32_t uTime = TimeUtils::minBarToTime(curBar.time);  // 从K线时间戳提取时间：将时间戳转换为HHMM格式
		uint32_t uMinute = sInfo->timeToMinutes(uTime);         // 转换为交易时段内的分钟偏移：将绝对时间转换为从开盘开始的分钟数
		uint32_t uBarMin = 0;  // 计算得出的目标K线分钟偏移：用于确定当前基础K线属于哪个目标K线

		// **核心算法：按交易时段对齐的时间计算**
		// 这是处理复杂交易时间的关键逻辑，支持按交易时段边界对齐K线
		if (bAlignSec)
		{
			// **交易时段对齐模式**
			auto it = std::lower_bound(secMins.begin(), secMins.end(), uMinute);  // 使用二分查找定位当前分钟数所在的交易时段
			auto secIdx = it - secMins.begin();  // 计算当前分钟数对应的交易时段索引
			if (secIdx == 0)
			{
				// **第一个交易时段内**
				uMinute -= 5;  // 5分钟K线的基础偏移：减去5分钟以对齐到5分钟边界
				uBarMin = (uMinute / steplen)*steplen + steplen;  // 计算目标K线的分钟偏移：按步长对齐并加上一个步长
				if (uBarMin > secMins[secIdx])
					uBarMin = secMins[secIdx];  // 确保不超过时段边界：如果计算结果超过当前时段，则限制在时段边界
			}
			else
			{
				// **非第一个交易时段内**
				uMinute -= secMins[secIdx - 1];  // 减去前一个时段的累计分钟数，得到在当前时段内的相对分钟数
				uBarMin = secMins[secIdx - 1] + (uMinute / steplen)*steplen + steplen;  // 计算目标K线的绝对分钟偏移
				if (uBarMin > secMins[secIdx])
					uBarMin = secMins[secIdx];  // 确保不超过当前时段边界
			}
		}
		else
		{
			// **普通模式：不按交易时段对齐**
			uMinute -= 5;  // 减去5分钟基础偏移，对齐到5分钟边界
			uBarMin = (uMinute / steplen)*steplen + steplen;  // 计算目标K线的分钟偏移：按步长对齐并加上一个步长
		}

		uint64_t uBarTime = sInfo->minuteToTime(uBarMin);  // 将目标K线的分钟偏移转换为绝对时间（HHMM格式）
		if (uBarTime < uTime)  // 如果目标K线时间早于当前基础K线时间，说明跨日了
			uDate = TimeUtils::getNextDate(uDate, 1);  // 将日期推进到下一天
		uBarTime = TimeUtils::timeToMinBar(uDate, (uint32_t)uBarTime);  // 将日期和时间组合成完整的时间戳

		WTSBarStruct* lastBar = NULL;  // 指向结果K线数据中最后一条K线的指针
		if(ret->size() > 0)  // 如果结果K线数据不为空
		{
			lastBar = ret->at(ret->size()-1);  // 获取最后一条K线数据
		}

		bool bNewBar = false;  // 标记是否需要创建新的K线
		if(lastBar == NULL || lastBar->time != uBarTime)  // 如果没有最后一条K线或时间不匹配
		{
			//只要日期和时间都不符,则认为已经是一条新的bar了
			lastBar = new WTSBarStruct();  // 创建新的K线结构
			bNewBar = true;  // 标记为新K线

			memcpy(lastBar, &curBar, sizeof(WTSBarStruct));  // 复制基础K线数据到新K线
			lastBar->date = uTradingDate;  // 设置交易日期
			lastBar->time = uBarTime;  // 设置目标K线时间
		}
		else
		{
			// **更新现有K线数据**
			bNewBar = false;  // 标记为更新现有K线

			lastBar->high = max(lastBar->high, curBar.high);  // 更新最高价：取当前最高价和基础K线最高价的较大值
			lastBar->low = min(lastBar->low, curBar.low);  // 更新最低价：取当前最低价和基础K线最低价的较小值
			lastBar->close = curBar.close;  // 更新收盘价：使用基础K线的收盘价
			lastBar->settle = curBar.settle;  // 更新结算价：使用基础K线的结算价

			lastBar->vol += curBar.vol;  // 累加成交量：将基础K线成交量加到目标K线
			lastBar->money += curBar.money;  // 累加成交金额：将基础K线成交金额加到目标K线
			lastBar->add += curBar.add;  // 累加持仓变化：将基础K线持仓变化加到目标K线
			lastBar->hold = curBar.hold;  // 更新持仓量：使用基础K线的持仓量
		}

		if(bNewBar)  // 如果是新K线
		{
			ret->appendBar(*lastBar);  // 将新K线添加到结果数据中
			delete lastBar;  // 删除临时K线对象，避免内存泄漏
		}
	}

	//检查最后一条数据：处理未闭合的K线
	{
		WTSBarStruct* lastRawBar = baseKline->at(-1);  // 获取基础K线数据的最后一条
		WTSBarStruct* lastDesBar = ret->at(-1);  // 获取目标K线数据的最后一条
		//如果目标K线的最后一条数据的日期或者时间大于原始K线最后一条的日期或时间
		if (lastDesBar->date > lastRawBar->date || lastDesBar->time > lastRawBar->time)
		{
			if (!bIncludeOpen)  // 如果不包含未闭合的K线
				ret->getDataRef().resize(ret->size() - 1);  // 删除最后一条未闭合的K线
			else  // 如果包含未闭合的K线
				ret->setClosed(false);  // 标记最后一条K线为未闭合状态
		}
	}

	return ret;  // 返回生成的K线数据对象
}

/**
 * @brief 从基础K线数据提取日线数据
 * @param baseKline 基础周期K线数据切片（通常是分钟级K线）
 * @param times 目标日线的天数倍数（如5表示5日线，周线等）
 * @param bIncludeOpen 是否包含未闭合的K线，默认为true
 * @return WTSKlineData* 返回提取后的日线数据，失败返回NULL
 * 
 * 该函数用于从基础周期K线数据中提取日线或多日线数据，是历史数据处理中的重要函数。
 * 主要用于将更小周期的K线（如分钟K线）聚合为日线级别的更大周期K线。
 * 
 * 核心功能：
 * 1. **日线基础聚合**：以日为基础单位进行时间窗口聚合
 * 2. **多日周期支持**：支持多日线（如5日线、10日线、周线、月线等）
 * 3. **简化时间处理**：日线不需要复杂的交易时段对齐
 * 4. **批量数据处理**：一次性处理整个K线数据切片
 * 
 * 算法特点：
 * - **按计数聚合**：使用计数器方式进行聚合，每times根K线聚合为1根
 * - **时间戳简化**：日线的时间戳设置为0（只关注日期）
 * - **数据累积**：正确处理OHLCV数据的聚合规则
 * - **无交易时段限制**：日线级别不涉及复杂的交易时段计算
 * 
 * 与分钟级K线提取的区别：
 * - extractDayData：按计数聚合，不涉及复杂时间计算
 * - extractMin1Data/extractMin5Data：按时间窗口聚合，需要交易时段对齐
 * 
 * 支持的周期转换：
 * - times=1：标准日线
 * - times=5：5日线
 * - times=7：周线（7日线）
 * - times=30：月线（30日线）
 * 
 * 数据聚合规则：
 * - 开盘价：使用第一根K线的开盘价
 * - 最高价：所有K线最高价的最大值
 * - 最低价：所有K线最低价的最小值
 * - 收盘价：使用最后一根K线的收盘价
 * - 成交量：所有K线成交量的累加
 * - 成交额：所有K线成交额的累加
 */
WTSKlineData* WTSDataFactory::extractDayData(WTSKlineSlice* baseKline, uint32_t times, bool bIncludeOpen /* = true */)
{
	// **计算聚合步长**
	// steplen表示多少根基础K线聚合为1根日线
	uint32_t steplen = times;

	// **创建结果K线数据对象**
	WTSKlineData* ret = WTSKlineData::create(baseKline->code(), 0);
	ret->setPeriod(KP_DAY, times);  // 设置为日线基础的times倍周期

	// **初始化计数器**
	uint32_t count = 0;
	
	// **遍历所有基础K线数据进行聚合**
	for (auto i = 0; i < baseKline->size(); i++, count++)
	{
		// 获取当前基础K线数据
		const WTSBarStruct& curBar = *baseKline->at(i);

		// **提取日期信息**
		uint32_t uDate = curBar.date;  // 日线主要关注日期

		// **获取当前结果中的最后一根K线**
		WTSBarStruct* lastBar = NULL;
		if(ret->size() > 0)
		{
			lastBar = ret->at(ret->size()-1);
		}

		// **判断是否需要创建新的日线**
		bool bNewBar = false;
		if(lastBar == NULL || count == steplen)
		{
			// **创建新日线的条件**：
			// 1. 还没有任何日线数据
			// 2. 已经累积了steplen根基础K线
			
			// 创建新的日线结构
			lastBar = new WTSBarStruct();
			bNewBar = true;

			// 复制基础K线数据到新日线
			memcpy(lastBar, &curBar, sizeof(WTSBarStruct));
			lastBar->date = uDate;  // 设置日期
			lastBar->time = 0;      // 日线时间戳设置为0
			count = 0;              // 重置计数器
		}
		else
		{
			// **更新现有日线**
			bNewBar = false;

			// 更新OHLC数据
			lastBar->high = max(lastBar->high, curBar.high);    // 更新最高价
			lastBar->low = min(lastBar->low, curBar.low);       // 更新最低价
			lastBar->close = curBar.close;                      // 更新收盘价
			lastBar->settle = curBar.settle;                    // 更新结算价

			// 累加成交量和成交额
			lastBar->vol += curBar.vol;      // 累加成交量
			lastBar->money += curBar.money;  // 累加成交额
			lastBar->add = curBar.add;       // 更新增仓量
			lastBar->hold = curBar.hold;     // 更新持仓量
		}

		// **处理新创建的日线**
		if(bNewBar)
		{
			// 将新创建的日线添加到结果容器中
			ret->appendBar(*lastBar);
			delete lastBar;  // 释放临时结构
		}
	}

	return ret;
}

/**
 * @brief 从Tick数据切片提取秒级K线数据
 * @param ayTicks Tick数据切片，包含原始的逐笔成交数据
 * @param seconds 目标K线的秒数（如60表示1分钟K线）
 * @param sInfo 交易时段信息，用于时间计算和验证
 * @param bUnixTime 是否使用Unix时间戳格式，默认为false
 * @param bAlignSec 是否按交易时段对齐，默认为false
 * @return WTSKlineData* 返回提取后的秒级K线数据，失败返回NULL
 * 
 * 该函数是从最原始的Tick数据生成K线数据的核心实现，是整个数据处理链的起点。
 * 它将逐笔成交的Tick数据按照指定的时间窗口聚合为标准的OHLCV K线数据。
 * 
 * 核心功能：
 * 1. **Tick到K线转换**：将离散的Tick数据聚合为连续的K线数据
 * 2. **秒级时间窗口**：支持任意秒数的时间窗口（1秒、5秒、60秒等）
 * 3. **时间戳格式支持**：支持标准时间戳和Unix时间戳两种格式
 * 4. **交易时段感知**：基于交易时段信息进行准确的时间计算
 * 
 * 算法特点：
 * - **时间窗口计算**：barSeconds = (curSeconds/seconds)*seconds + seconds
 * - **跨日处理**：正确处理夜盘等跨自然日的交易时间
 * - **时间戳生成**：支持两种时间戳格式的生成
 * - **数据聚合**：按照K线标准规则聚合OHLCV数据
 * 
 * 与其他extract函数的区别：
 * - extractKlineData(Tick版本)：从原始Tick数据生成K线，是数据链的起点
 * - extractKlineData(K线版本)：从已有K线生成更大周期K线，是数据链的中间环节
 * - extractMin1Data/extractMin5Data：专门针对分钟级的优化实现
 * 
 * 时间戳格式：
 * - bUnixTime=false：标准格式 YYYYMMDD * 1000000 + HHMMSS
 * - bUnixTime=true：Unix时间戳格式（秒数）
 * 
 * 应用场景：
 * - 实时数据处理：将实时Tick数据转换为K线供策略使用
 * - 历史数据重建：从历史Tick数据重建各种周期的K线
 * - 数据验证：验证K线数据的准确性和完整性
 * - 自定义周期：生成非标准周期的K线数据
 */
WTSKlineData* WTSDataFactory::extractKlineData(WTSTickSlice* ayTicks, uint32_t seconds, 
	WTSSessionInfo* sInfo, bool bUnixTime /* = false */, bool bAlignSec /* = false */)
{
	// **输入数据有效性验证**
	if(ayTicks == NULL || ayTicks->size() == 0)
		return NULL;  // Tick数据为空或无数据，无法提取
	
	// 获取第一个Tick数据，用于初始化结果对象
	const WTSTickStruct& firstTick = *(ayTicks->at(0));

	// **交易时段信息验证**
	if(sInfo == NULL)
		return NULL;  // 交易时段信息为空，无法进行时间计算

	// **创建结果K线数据对象**
	WTSKlineData* ret = WTSKlineData::create(firstTick.code,0);
	ret->setPeriod(KP_Tick, seconds);  // 设置为Tick基础的seconds秒周期
	ret->setUnixTime(bUnixTime);       // 设置时间戳格式

	// **遍历所有Tick数据进行聚合**
	for (uint32_t i = 0; i < ayTicks->size(); i++)
	{
		// **获取当前结果中的最后一根K线**
		WTSBarStruct* lastBar = NULL;
		if(ret->size() > 0)
		{
			lastBar = ret->at(ret->size()-1);
		}

		// **处理当前Tick数据**
		const WTSTickStruct* curTick = ayTicks->at(i);
		uint32_t uDate = curTick->trading_date;  // 交易日期
		
		// **计算目标K线的时间窗口**
		uint32_t curSeconds = sInfo->timeToSeconds(curTick->action_time/1000);  // 当前秒数
		uint32_t barSeconds = (curSeconds/seconds)*seconds + seconds;           // 目标K线秒数
		uint64_t barTime = sInfo->secondsToTime(barSeconds);                    // 转换为时间格式

		// **处理跨日情况**
		// 如果计算出来的K线时间戳小于tick数据的时间戳，说明跨日了
		uint32_t actDt = curTick->action_date;
		if (barTime < curTick->action_time / 1000)
		{
			actDt = TimeUtils::getNextDate(actDt);  // 日期推进到下一天
		}

		// **生成最终的时间戳**
		if(bUnixTime)
		{
			// Unix时间戳格式：转换为标准的Unix秒数
			barTime = (uint64_t)TimeUtils::makeTime(actDt, (long)(barTime * 1000)) / 1000;
		}
		else
		{
			// 标准时间戳格式：YYYYMMDD * 1000000 + HHMMSS
			barTime = (uint64_t)actDt * 1000000 + barTime;
		}

		// **判断是否需要创建新的K线**
		bool bNewBar = false;
		if (lastBar == NULL || uDate != lastBar->date || barTime != lastBar->time)
		{
			// **创建新K线的条件**：
			// 1. 还没有任何K线数据
			// 2. 交易日期不匹配
			// 3. 时间戳不匹配
			
			// 创建新的K线结构
			lastBar = new WTSBarStruct();
			bNewBar = true;

			lastBar->date = uDate;
			lastBar->time = barTime;

			lastBar->open = curTick->price;
			lastBar->high = curTick->price;
			lastBar->low = curTick->price;
			lastBar->close = curTick->price;
			lastBar->vol = curTick->volume;
			lastBar->money = curTick->turn_over;
			lastBar->hold = curTick->open_interest;
			lastBar->add = curTick->diff_interest;
		}
		else
		{
			lastBar->close = curTick->price;
			lastBar->high = max(lastBar->high,curTick->price);
			lastBar->low = min(lastBar->low,curTick->price);
			lastBar->vol += curTick->volume;
			lastBar->money += curTick->turn_over;
			lastBar->hold = curTick->open_interest;
			lastBar->add += curTick->diff_interest;
		}

		if(bNewBar)
		{
			ret->appendBar(*lastBar);
			delete lastBar;
		}
	}

	return ret;
}

/**
 * @brief 合并两个K线数据
 * @param klineData 目标K线数据对象（合并结果存储在这里）
 * @param newKline 待合并的K线数据对象
 * @return bool 合并成功返回true，失败返回false
 * 
 * 该函数用于将两个K线数据进行合并，通常用于将新的历史数据与现有数据进行拼接。
 * 这是数据管理系统中的重要功能，确保K线数据的完整性和连续性。
 * 
 * 合并策略：
 * 1. **兼容性检查**：
 *    - 验证两个K线数据的合约代码是否相同
 *    - 验证K线周期和倍数是否匹配
 *    - 确保数据格式的一致性
 * 
 * 2. **时间范围分析**：
 *    - 分析现有数据的时间范围
 *    - 确定新数据的插入位置
 *    - 处理时间重叠的情况
 * 
 * 3. **数据分类插入**：
 *    - 头部数据：时间早于现有数据的K线
 *    - 尾部数据：时间晚于现有数据的K线
 *    - 重叠数据：时间重叠的K线需要特殊处理
 * 
 * 4. **优化处理**：
 *    - 空数据情况：直接交换数据，避免复制
 *    - 批量插入：使用高效的容器操作
 *    - 内存管理：合理使用临时容器
 * 
 * 应用场景：
 * - 历史数据加载：将新下载的历史数据与缓存数据合并
 * - 数据修复：修复缺失或错误的K线数据
 * - 数据更新：定期更新K线数据库
 * - 多源数据整合：整合来自不同数据源的K线数据
 */
bool WTSDataFactory::mergeKlineData(WTSKlineData* klineData, WTSKlineData* newKline)
{
	// **参数有效性验证**
	if (klineData == NULL || newKline == NULL)
		return false;  // 任一参数为空，无法合并

	// **合约代码一致性验证**
	// 确保两个K线数据属于同一个合约
	if (strcmp(klineData->code(), newKline->code()) != 0)
		return false;  // 合约代码不匹配，无法合并

	// **K线周期兼容性验证**
	// 确保两个K线数据的周期类型和倍数完全相同
	if (!(klineData->period() == newKline->period() && klineData->times() == newKline->times()))
		return false;  // 周期不匹配，无法合并

	// **获取K线数据的内部容器引用**
	WTSKlineData::WTSBarList& bars = klineData->getDataRef();      // 目标K线数据容器
	WTSKlineData::WTSBarList& newBars = newKline->getDataRef();    // 待合并K线数据容器
	
	// **优化处理：目标数据为空的情况**
	if(bars.empty())
	{
		// 如果目标数据为空，直接交换数据容器，避免逐个复制
		bars.swap(newBars);    // 高效的数据交换操作
		newBars.clear();       // 清空源数据容器
		return true;           // 合并成功
	}
	else
	{
		// **分析现有数据的时间范围**
		uint64_t sTime,eTime;  // 开始时间和结束时间
		
		// 根据K线周期类型选择时间字段
		if(klineData->period() == KP_DAY)
		{
			// 日线数据使用date字段作为时间戳
			sTime = bars[0].date;                    // 第一根K线的日期
			eTime = bars[bars.size() - 1].date;      // 最后一根K线的日期
		}
		else
		{
			// 其他周期使用time字段作为时间戳
			sTime = bars[0].time;                    // 第一根K线的时间戳
			eTime = bars[bars.size() - 1].time;      // 最后一根K线的时间戳
		}

		// **创建临时容器用于分类存储**
		WTSKlineData::WTSBarList tempHead, tempTail;  // 头部和尾部数据的临时容器
		
		// **遍历待合并的K线数据，进行分类**
		uint32_t count = newKline->size();
		for (uint32_t i = 0; i < count; i++)
		{
			WTSBarStruct& curBar = newBars[i];

			// 获取当前K线的时间戳
			uint64_t curTime;
			if (klineData->period() == KP_DAY)
				curTime = curBar.date;   // 日线使用日期作为时间戳
			else
				curTime = curBar.time;   // 其他周期使用时间戳

			// **数据分类逻辑**：根据时间关系将新数据分类
			if(curTime < sTime)
			{
				// 时间早于现有数据，归类为头部数据
				tempHead.emplace_back(curBar);
			}
			else if(curTime > eTime)
			{
				// 时间晚于现有数据，归类为尾部数据
				tempTail.emplace_back(curBar);
			}
			// 注意：时间重叠的数据（sTime <= curTime <= eTime）被忽略
			// 这避免了重复数据的问题，保持数据的唯一性
		}

		// **执行数据插入操作**
		// 将分类好的数据插入到目标容器的相应位置
		bars.insert(bars.begin(), tempHead.begin(), tempHead.end());  // 在开头插入头部数据
		bars.insert(bars.end(), tempTail.begin(), tempTail.end());    // 在末尾插入尾部数据
	}
	
	// 合并操作成功完成
	return true;
}
