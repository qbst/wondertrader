/*!
 * \file WTSSessionInfo.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt交易时间模板对象定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统的交易时间管理类，用于处理不同市场的交易时段、集合竞价时间等时间相关逻辑。
 * 
 * 主要功能：
 * 1. 交易时段管理：支持多段交易时间，如上午、下午、夜盘等
 * 2. 集合竞价时间：管理开盘前和收盘前的集合竞价时段
 * 3. 时间偏移处理：支持夜盘等跨日交易的时间偏移计算
 * 4. 时间转换功能：在交易时间和绝对时间之间进行转换
 * 5. 交易状态判断：判断当前时间是否处于交易时段或集合竞价时段
 * 
 * 设计特点：
 * - 灵活的时间段配置：支持任意数量的交易时段
 * - 智能的时间偏移：自动处理跨日交易的时间计算
 * - 高效的时间查询：提供快速的时间段判断和转换
 * - 支持多市场：可以为不同市场配置不同的交易时间模板
 * - 便于策略使用：为交易策略提供准确的时间判断依据
 */
#pragma once
#include <vector>  // 向量容器，用于存储交易时段信息

#include "WTSObject.hpp"           // WonderTrader基础对象类，提供引用计数功能
#include "../Share/TimeUtils.hpp"   // 时间工具类，提供日期时间处理功能

NS_WTP_BEGIN  // 开始WonderTrader命名空间

static const char* DEFAULT_SESSIONID = "TRADING";  // 默认交易时段标识符，用于标准交易时段

/**
 * 交易时段信息类
 * 
 * 功能概述：
 * 管理不同市场的交易时间配置，支持多时段交易、集合竞价、夜盘等复杂交易时间安排。
 * 提供时间转换、状态判断、偏移计算等核心功能，是交易系统时间管理的基础设施。
 * 
 * 主要特性：
 * - 多时段支持：支持上午、下午、夜盘等多个交易时段
 * - 时间偏移：自动处理跨日交易的时间偏移计算
 * - 集合竞价：支持开盘前、收盘前的集合竞价时段管理
 * - 时间转换：提供交易时间与分钟数、秒数之间的相互转换
 * - 状态判断：判断指定时间是否处于交易或集合竞价状态
 * 
 * 应用场景：
 * - 策略系统的时间判断和控制
 * - 行情数据的时间校验和处理
 * - 交易指令的时间窗口控制
 * - 风控系统的时间维度监控
 * 
 * 时间格式说明：
 * - 时间格式：HHMM（如0930表示9:30，1500表示15:00）
 * - 日期格式：YYYYMMDD（如20230315表示2023年3月15日）
 * - 支持24小时制，夜盘可能跨越0点
 */
class WTSSessionInfo : public WTSObject
{
public:
	/**
	 * 交易时段结构体
	 * 
	 * 功能说明：
	 * 定义单个交易时段的时间范围，同时保存原始时间和偏移后时间。
	 * 原始时间用于显示和配置，偏移时间用于内部计算和比较。
	 * 
	 * 设计考虑：
	 * - 支持夜盘等跨日交易场景的时间偏移
	 * - 便于时间范围的判断和计算
	 * - 保持配置的直观性和计算的准确性
	 */
	typedef struct _TradingSection
	{
		uint32_t	first_raw;	    // 原始开始时间：用户配置的开始时间（如900表示9:00）
		uint32_t	first;		    // 偏移后开始时间：经过夜盘偏移计算后的时间，用于内部比较

		uint32_t	second_raw;	    // 原始结束时间：用户配置的结束时间（如1500表示15:00）
		uint32_t	second;		    // 偏移后结束时间：经过夜盘偏移计算后的时间，用于内部比较

		/**
		 * 构造函数：初始化交易时段
		 * 
		 * @param stime 偏移后的开始时间
		 * @param etime 偏移后的结束时间
		 * @param stime_raw 原始开始时间
		 * @param etime_raw 原始结束时间
		 */
		_TradingSection(uint32_t stime, uint32_t etime, uint32_t stime_raw, uint32_t etime_raw)
			: first(stime), second(etime), first_raw(stime_raw), second_raw(etime_raw)
		{
		}
	} TradingSection;  // 交易时段类型别名

	typedef std::vector<TradingSection>		TradingTimes;  // 交易时段向量类型，支持多个交易时段

protected:
	TradingTimes	m_tradingTimes;  // 交易时段列表，存储所有正常交易时段（如上午、下午、夜盘等）
	
	/*
	 * 集合竞价时段管理
	 * 
	 * 设计说明 (By Wesley @ 2023.05.17)：
	 * - 集合竞价时间支持多段配置，以适应不同市场的复杂需求
	 * - 当前系统中很多地方仍只使用第一个集合竞价时间进行状态判断
	 * - 白盘集合竞价通常在开盘前一分钟撮合，状态机会相应前移
	 * - 现有逻辑基本无需修改，保持了向后兼容性
	 */
	TradingTimes	m_auctionTimes;  // 集合竞价时段列表，支持多个集合竞价时段
	
	int32_t			m_uOffsetMins;   // 时间偏移分钟数，用于处理夜盘等跨日交易场景

	std::string		m_strID;         // 交易时段标识符，唯一标识该交易时段配置
	std::string		m_strName;       // 交易时段名称，用于显示和日志记录

protected:
	/**
	 * 保护构造函数
	 * 防止直接实例化，必须通过静态工厂方法创建
	 * 
	 * @param offset 时间偏移分钟数，正数表示向后偏移，负数表示向前偏移
	 */
	WTSSessionInfo(int32_t offset)
	{
		m_uOffsetMins = offset;  // 设置时间偏移量
	}
	
	/**
	 * 虚析构函数
	 * 支持多态销毁，确保派生类对象能够正确清理资源
	 */
	virtual ~WTSSessionInfo(){}

public:
	/**
	 * 获取交易时段标识符
	 * @return 交易时段ID字符串
	 */
	const char* id() const{ return m_strID.c_str(); }
	
	/**
	 * 获取交易时段名称
	 * @return 交易时段名称字符串
	 */
	const char* name() const{ return m_strName.c_str(); }

	/**
	 * 静态工厂方法：创建交易时段信息对象
	 * 
	 * @param sid 交易时段标识符，用于唯一标识该时段配置
	 * @param name 交易时段名称，用于显示和日志
	 * @param offset 时间偏移分钟数，默认为0（无偏移）
	 * @return 新创建的交易时段信息对象指针
	 * 
	 * 使用示例：
	 * WTSSessionInfo* session = WTSSessionInfo::create("SHFE_RB", "上期螺纹钢", -480);
	 */
	static WTSSessionInfo* create(const char* sid, const char* name, int32_t offset = 0)
	{
		WTSSessionInfo* pRet = new WTSSessionInfo(offset);  // 创建新实例
		pRet->m_strID = sid;    // 设置标识符
		pRet->m_strName = name; // 设置名称
		return pRet;
	}

public:
	/**
	 * 获取时间偏移分钟数
	 * @return 偏移分钟数，正数表示向后偏移，负数表示向前偏移
	 */
	int32_t	getOffsetMins() const{return m_uOffsetMins;}

	/**
	 * 添加交易时段
	 * 将新的交易时段添加到交易时段列表中，自动计算偏移时间
	 * 
	 * @param sTime 开始时间，格式HHMM（如0930表示9:30）
	 * @param eTime 结束时间，格式HHMM（如1130表示11:30）
	 * 
	 * 使用示例：
	 * session->addTradingSection(930, 1130);   // 添加上午时段9:30-11:30
	 * session->addTradingSection(1300, 1500);  // 添加下午时段13:00-15:00
	 */
	void addTradingSection(uint32_t sTime, uint32_t eTime)
	{
		m_tradingTimes.emplace_back(TradingSection(
			offsetTime(sTime, true),   // 计算偏移后的开始时间
			offsetTime(eTime, false),  // 计算偏移后的结束时间
			sTime, eTime               // 保存原始时间
		));
	}

	/**
	 * 设置集合竞价时间
	 * 设置第一个（主要的）集合竞价时段，如果已存在则更新
	 * 
	 * @param sTime 集合竞价开始时间，格式HHMM
	 * @param eTime 集合竞价结束时间，格式HHMM
	 * 
	 * 使用示例：
	 * session->setAuctionTime(925, 930);  // 设置集合竞价9:25-9:30
	 */
	void setAuctionTime(uint32_t sTime, uint32_t eTime)
	{
		if (m_auctionTimes.empty())  // 如果集合竞价列表为空，添加新的
		{
			m_auctionTimes.emplace_back(TradingSection(
				offsetTime(sTime, true), offsetTime(eTime, false), sTime, eTime));
		}
		else  // 如果已存在，更新第一个集合竞价时段
		{
			m_auctionTimes[0].first_raw = sTime;
			m_auctionTimes[0].second_raw = eTime;
			m_auctionTimes[0].first = offsetTime(sTime, true);
			m_auctionTimes[0].second = offsetTime(eTime, false);
		}
	}

	/**
	 * 添加集合竞价时段
	 * 在现有集合竞价列表中添加新的时段，支持多个集合竞价时间
	 * 
	 * @param sTime 集合竞价开始时间，格式HHMM
	 * @param eTime 集合竞价结束时间，格式HHMM
	 */
	void addAuctionTime(uint32_t sTime, uint32_t eTime)
	{
		m_auctionTimes.emplace_back(TradingSection(
			offsetTime(sTime, true), offsetTime(eTime, false), sTime, eTime));
	}

	/**
	 * 设置时间偏移分钟数
	 * 用于处理夜盘等跨日交易的时间偏移
	 * 
	 * @param offset 偏移分钟数，正数向后偏移，负数向前偏移
	 */
	void setOffsetMins(int32_t offset){m_uOffsetMins = offset;}

	/**
	 * 获取交易时段列表的常量引用
	 * @return 交易时段列表的常量引用，不可修改
	 */
	const TradingTimes&		getTradingSections() const{ return m_tradingTimes; }
	
	/**
	 * 获取集合竞价时段列表的常量引用
	 * @return 集合竞价时段列表的常量引用，不可修改
	 */
	const TradingTimes&		getAuctionSections() const{ return m_auctionTimes; }

	//需要导出到脚本的函数
public:
	/**
	 * 获取交易时段数量
	 * 
	 * 功能说明：
	 * 返回当前配置的交易时段总数，用于脚本接口和外部查询。
	 * 
	 * @return 交易时段数量，如果未配置任何时段则返回0
	 */
	uint32_t getSectionCount() const{ return (uint32_t)m_tradingTimes.size(); }

	/**
	 * 计算偏移以后的日期
	 * 
	 * 功能说明：
	 * 根据时间偏移量计算偏移后的日期，主要用于夜盘等跨日交易场景的日期比较。
	 * 当时间偏移导致日期发生变化时，返回相应的偏移日期。
	 * 
	 * 计算逻辑：
	 * 1. 如果未提供日期和时间，则获取当前系统时间
	 * 2. 将时间转换为分钟数并加上偏移量
	 * 3. 如果偏移后超过一天，则返回下一天
	 * 4. 如果偏移后为负数，则返回前一天
	 * 5. 否则返回原日期
	 * 
	 * 使用示例：
	 * uint32_t nextDay = session->getOffsetDate(20230315, 2300);  // 夜盘23:00可能偏移到次日
	 * uint32_t prevDay = session->getOffsetDate(20230315, 100);   // 早盘1:00可能偏移到前日
	 * 
	 * @param uDate 日期，格式为YYYYMMDD，0表示当前日期
	 * @param uTime 时间，格式为HHMM，0表示当前时间
	 * @return 偏移后的日期，格式为YYYYMMDD
	 */
	uint32_t getOffsetDate(uint32_t uDate = 0, uint32_t uTime = 0)
	{
		// 如果未提供日期，则获取当前系统日期和时间
		if(uDate == 0)
		{
			// 获取当前日期和时间，uTime格式为HHMMSSSSS
			TimeUtils::getDateTime(uDate, uTime);
			// 将时间转换为HHMM格式，去掉秒数部分
			uTime /= 100000;
		}

		// 将时间转换为分钟数：小时*60 + 分钟
		// 例如：1430 -> 14*60 + 30 = 870分钟
		int32_t curMinute = (uTime / 100) * 60 + uTime % 100;
		// 加上时间偏移量，正数向后偏移，负数向前偏移
		curMinute += m_uOffsetMins;

		// 如果偏移后的分钟数超过一天（1440分钟），则返回下一天
		if (curMinute >= 1440)
			return TimeUtils::getNextDate(uDate);

		// 如果偏移后的分钟数为负数，则返回前一天
		if (curMinute < 0)
			return TimeUtils::getNextDate(uDate, -1);

		// 偏移后仍在当天范围内，返回原日期
		return uDate;
	}

	/**
	 * 将交易时间转换为从开盘开始的累计交易分钟数
	 * 
	 * 功能说明：
	 * 将指定的交易时间（HHMM格式）转换为从当日第一个交易时段开始计算的累计交易分钟数。
	 * 该函数是交易系统时间计算的核心，用于将绝对时间映射到交易时间轴上的相对位置。
	 * 
	 * 计算逻辑：
	 * 1. 首先检查是否处于集合竞价时间，如果是则返回0（开盘前状态）
	 * 2. 将输入时间进行偏移处理（处理夜盘等跨日交易场景）
	 * 3. 遍历所有交易时段，计算当前时间在交易时间轴上的累计分钟数
	 * 4. 支持多时段交易，自动累加前面时段的交易分钟数
	 * 
	 * 使用示例：
	 * WTSSessionInfo* session = WTSSessionInfo::create("SHFE_RB", "上期螺纹钢", -480);
	 * session->addTradingSection(900, 1015);   // 上午时段 9:00-10:15
	 * session->addTradingSection(1030, 1130); // 上午时段 10:30-11:30
	 * session->addTradingSection(1330, 1500); // 下午时段 13:30-15:00
	 * 
	 * uint32_t minutes = session->timeToMinutes(930);  // 9:30 -> 30分钟（第一个时段内）
	 * uint32_t minutes2 = session->timeToMinutes(1100); // 11:00 -> 105分钟（前两个时段累计）
	 * uint32_t minutes3 = session->timeToMinutes(1400); // 14:00 -> 165分钟（所有时段累计）
	 * 
	 * @param uTime 输入时间，格式为HHMM（如930表示9:30，1500表示15:00）
	 * @param autoAdjust 自动调整标志，当为true时，非交易时间内的输入会自动对齐到下一个交易时间
	 * @return 从开盘开始的累计交易分钟数，如果输入时间不在任何交易时段内且autoAdjust为false，则返回INVALID_UINT32
	 */
	uint32_t timeToMinutes(uint32_t uTime, bool autoAdjust = false)
	{
		// 检查交易时段列表是否为空，如果为空则无法进行时间转换
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		// 检查输入时间是否处于集合竞价时段，如果是则返回0分钟（表示开盘前状态）
		// 集合竞价时间通常为开盘前几分钟，此时市场处于集合竞价状态，不算正式交易时间
		if(isInAuctionTime(uTime))
			return 0;

		// 对输入时间进行偏移处理，将原始时间转换为偏移后的时间
		// 主要用于处理夜盘等跨日交易场景，偏移量由m_uOffsetMins决定
		// 例如：夜盘21:00实际对应次日凌晨1:00，需要偏移-480分钟
		uint32_t offTime = offsetTime(uTime, true);

		// 初始化累计分钟数偏移量，用于累加前面所有交易时段的分钟数
		uint32_t offset = 0;
		// 标记是否找到匹配的交易时段
		bool bFound = false;
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		auto it = m_tradingTimes.begin();
		// 遍历所有交易时段，计算当前时间在交易时间轴上的位置
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用，包含开始时间和结束时间
			TradingSection &section = *it;
			// 检查偏移后的时间是否在当前交易时段范围内（包含边界）
			if (section.first <= offTime && offTime <= section.second)
			{
				// 计算当前时间在当前交易时段内的小时偏移量
				// 例如：10:30在9:00-11:30时段内，小时偏移为1小时
				int32_t hour = offTime / 100 - section.first / 100;
				// 计算当前时间在当前交易时段内的分钟偏移量
				// 例如：10:30在9:00-11:30时段内，分钟偏移为30分钟
				int32_t minute = offTime % 100 - section.first % 100;
				// 将小时和分钟转换为总分钟数，并累加到总偏移量中
				// 例如：1小时30分钟 = 90分钟
				offset += hour*60 + minute;
				// 标记已找到匹配的交易时段
				bFound = true;
				// 跳出循环，不再检查后续时段
				break;
			}
			else if(offTime > section.second)	// 当前时间大于当前时段的结束时间（超过上边界）
			{
				// 计算当前交易时段的持续小时数
				// 例如：9:00-11:30时段持续2.5小时
				int32_t hour = section.second/100 - section.first/100;
				// 计算当前交易时段的持续分钟数
				// 例如：9:00-11:30时段持续30分钟
				int32_t minute = section.second%100 - section.first%100;
				// 将当前时段的持续分钟数累加到总偏移量中
				// 这样后续时段就能正确计算累计时间
				offset += hour*60 + minute;
			} 
			else // 当前时间小于当前时段的开始时间（低于下边界）
			{
				// 如果启用了自动调整功能
				if(autoAdjust)
				{
					// 标记为已找到，这样会返回当前累计的偏移量
					// 实现将非交易时间对齐到下一个交易时间的效果
					bFound = true;
				}
				// 跳出循环，不再检查后续时段
				break;
			}
		}

		// 如果没有找到匹配的交易时段且未启用自动调整，返回无效值
		// 这表示输入时间不在任何交易时段内
		if(!bFound)
			return INVALID_UINT32;

		// 返回计算得到的累计交易分钟数
		// 这个值表示从当日第一个交易时段开始到当前时间的累计交易分钟数
		return offset;
	}

	/**
	 * 将累计交易分钟数转换为具体的交易时间
	 * 
	 * 功能说明：
	 * 将指定的累计交易分钟数转换为对应的具体交易时间（HHMM格式）。
	 * 该函数是timeToMinutes函数的逆操作，用于将交易时间轴上的相对位置映射回绝对时间。
	 * 支持多时段交易，能够准确计算跨时段的分钟数偏移。
	 * 
	 * 计算逻辑：
	 * 1. 检查交易时段列表是否为空，如果为空则无法进行时间转换
	 * 2. 遍历所有交易时段，将累计分钟数分配到各个时段中
	 * 3. 根据bHeadFirst参数决定处理顺序：false为从前往后，true为从后往前
	 * 4. 如果分钟数在当前时段内，则计算该时段内的具体时间
	 * 5. 如果分钟数超过当前时段，则减去当前时段的分钟数，继续处理下一时段
	 * 6. 将计算结果转换为HHMM格式并应用时间偏移
	 * 
	 * 使用示例：
	 * WTSSessionInfo* session = WTSSessionInfo::create("SHFE_RB", "上期螺纹钢", -480);
	 * session->addTradingSection(900, 1015);   // 上午时段 9:00-10:15
	 * session->addTradingSection(1030, 1130); // 上午时段 10:30-11:30
	 * session->addTradingSection(1330, 1500); // 下午时段 13:30-15:00
	 * 
	 * uint32_t time1 = session->minuteToTime(30);    // 30分钟 -> 930（9:30）
	 * uint32_t time2 = session->minuteToTime(105);   // 105分钟 -> 1100（11:00）
	 * uint32_t time3 = session->minuteToTime(165);   // 165分钟 -> 1400（14:00）
	 * 
	 * @param uMinutes 累计交易分钟数，从当日第一个交易时段开始计算的分钟数
	 * @param bHeadFirst 处理顺序标志，false表示从前往后处理，true表示从后往前处理
	 * @return 对应的交易时间，格式为HHMM（如930表示9:30），如果输入无效则返回INVALID_UINT32
	 */
	uint32_t minuteToTime(uint32_t uMinutes, bool bHeadFirst = false)
	{
		// 检查交易时段列表是否为空，如果为空则无法进行时间转换
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		// 初始化剩余分钟数偏移量，用于在遍历交易时段时逐步减少
		// 开始时等于输入的累计分钟数，随着处理每个时段会相应减少
		uint32_t offset = uMinutes;
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，将累计分钟数分配到各个时段中
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用，包含开始时间和结束时间
			TradingSection &section = *it;
			// 将当前交易时段的开始时间转换为从当日0点开始的分钟数
			// 例如：9:00 = 9*60 + 0 = 540分钟
			uint32_t startMin = section.first/100*60 + section.first%100;
			// 将当前交易时段的结束时间转换为从当日0点开始的分钟数
			// 例如：10:15 = 10*60 + 15 = 615分钟
			uint32_t stopMin = section.second/100*60 + section.second%100;

			// 根据处理顺序标志决定处理逻辑
			if(!bHeadFirst)  // 从前往后处理（默认方式）
			{
				// 检查剩余分钟数是否大于等于当前时段的持续分钟数
				// 如果是，说明目标时间不在当前时段内，需要继续处理下一时段
				if (startMin + offset >= stopMin)
				{
					// 从剩余分钟数中减去当前时段的持续分钟数
					// 这样剩余分钟数就表示在后续时段中的偏移量
					offset -= (stopMin - startMin);
					// 如果剩余分钟数正好为0，说明目标时间正好是当前时段的结束时间
					if (offset == 0)
					{
						// 将当前时段的结束分钟数转换为HHMM格式，应用时间偏移并返回结果
						// 例如：615分钟 = 10小时15分钟 = 1015
						return originalTime(stopMin / 60 * 100 + stopMin % 60);
					}
				}
				else
				{
					// 目标时间位于当前时段内，计算具体的分钟数位置
					// 例如：如果当前时段是9:00-10:15，剩余分钟数是30分钟
					// 则目标时间为9:00 + 30分钟 = 9:30
					uint32_t desMin = startMin + offset;
					// 处理跨日情况：如果计算出的分钟数超过一天（1440分钟），则减去一天
					// 这主要处理夜盘等跨日交易场景
					if (desMin >= 1440)
						desMin -= 1440;

					// 将目标分钟数转换为HHMM格式，应用时间偏移并返回结果
					// 例如：570分钟 = 9小时30分钟 = 930
					return originalTime(desMin / 60 * 100 + desMin % 60);
				}
			}
			else  // 从后往前处理（特殊模式）
			{
				// 检查剩余分钟数是否小于当前时段的持续分钟数
				// 如果是，说明目标时间在当前时段内
				if (startMin + offset < stopMin)
				{
					// 目标时间位于当前时段内，计算具体的分钟数位置
					uint32_t desMin = startMin + offset;
					// 处理跨日情况：如果计算出的分钟数超过一天，则减去一天
					if (desMin >= 1440)
						desMin -= 1440;

					// 将目标分钟数转换为HHMM格式，应用时间偏移并返回结果
					return originalTime(desMin / 60 * 100 + desMin % 60);
				}
				else
				{
					// 目标时间不在当前时段内，从剩余分钟数中减去当前时段的持续分钟数
					// 继续处理下一个时段
					offset -= (stopMin - startMin);
				}
			}
		}

		// 如果遍历完所有交易时段后仍有剩余分钟数，返回收盘时间
		// 这通常表示输入分钟数超出了所有交易时段的总长度
		return getCloseTime();
	}

	/**
	 * 将交易时间转换为从开盘开始的累计交易秒数
	 * 
	 * 功能说明：
	 * 将指定的交易时间（HHMMSS格式）转换为从当日第一个交易时段开始计算的累计交易秒数。
	 * 该函数是交易系统精确时间计算的核心，用于将绝对时间映射到交易时间轴上的精确位置。
	 * 相比timeToMinutes函数，此函数提供秒级精度的时间计算。
	 * 
	 * 计算逻辑：
	 * 1. 首先检查是否处于集合竞价时间，如果是则返回0秒（开盘前状态）
	 * 2. 解析输入时间的小时、分钟、秒数，并进行偏移处理
	 * 3. 将时间转换为从当日0点开始的秒数
	 * 4. 遍历所有交易时段，计算当前时间在交易时间轴上的累计秒数
	 * 5. 支持多时段交易，自动累加前面时段的交易秒数
	 * 
	 * 应用场景：
	 * - 高频交易：需要秒级精度的时间计算和策略控制
	 * - 精确数据存储：将行情数据按交易秒数进行精确索引
	 * - 实时风控：基于精确交易时间进行毫秒级风险控制
	 * - 回测系统：将历史时间转换为回测时间轴上的精确位置
	 * - 延迟分析：计算交易指令的执行延迟和响应时间
	 * 
	 * 使用示例：
	 * WTSSessionInfo* session = WTSSessionInfo::create("SHFE_RB", "上期螺纹钢", -480);
	 * session->addTradingSection(900, 1015);   // 上午时段 9:00-10:15
	 * session->addTradingSection(1030, 1130); // 上午时段 10:30-11:30
	 * session->addTradingSection(1330, 1500); // 下午时段 13:30-15:00
	 * 
	 * uint32_t seconds = session->timeToSeconds(93000);  // 9:30:00 -> 1800秒（第一个时段内）
	 * uint32_t seconds2 = session->timeToSeconds(110000); // 11:00:00 -> 6300秒（前两个时段累计）
	 * uint32_t seconds3 = session->timeToSeconds(140000); // 14:00:00 -> 9900秒（所有时段累计）
	 * 
	 * @param uTime 输入时间，格式为HHMMSS（如93000表示9:30:00，150000表示15:00:00）
	 * @return 从开盘开始的累计交易秒数，如果输入时间不在任何交易时段内，则返回INVALID_UINT32
	 */
	uint32_t timeToSeconds(uint32_t uTime)
	{
		// 检查交易时段列表是否为空，如果为空则无法进行时间转换
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		// 检查输入时间是否处于集合竞价时段，如果是则返回0秒（表示开盘前状态）
		// 集合竞价时间通常为开盘前几分钟，此时市场处于集合竞价状态，不算正式交易时间
		// 注意：这里使用uTime/100是因为uTime是HHMMSS格式，需要提取HHMM部分进行判断
		if(isInAuctionTime(uTime/100))
			return 0;

		// 从输入时间中提取秒数部分（HHMMSS格式的最后两位）
		// 例如：93030中的30秒
		uint32_t sec = uTime%100;
		// 从输入时间中提取小时部分（HHMMSS格式的前两位）
		// 例如：93030中的9小时
		uint32_t h = uTime/10000;
		// 从输入时间中提取分钟部分（HHMMSS格式的中间两位）
		// 例如：93030中的30分钟
		uint32_t m = uTime%10000/100;
		// 将小时和分钟组合成HHMM格式，并进行偏移处理
		// 主要用于处理夜盘等跨日交易场景，偏移量由m_uOffsetMins决定
		uint32_t offMin = offsetTime(h*100 + m, true);
		// 从偏移后的时间中重新提取小时部分
		h = offMin/100;
		// 从偏移后的时间中重新提取分钟部分
		m = offMin%100;
		// 将偏移后的小时、分钟、秒数转换为从当日0点开始的秒数
		// 公式：总秒数 = 小时*3600 + 分钟*60 + 秒数
		// 例如：9:30:30 = 9*3600 + 30*60 + 30 = 34230秒
		uint32_t seconds = h*60*60 + m*60 + sec;

		// 初始化累计秒数偏移量，用于累加前面所有交易时段的秒数
		uint32_t offset = 0;
		// 标记是否找到匹配的交易时段
		bool bFound = false;
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，计算当前时间在交易时间轴上的位置
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用，包含开始时间和结束时间
			TradingSection &section = *it;
			// 将当前交易时段的开始时间转换为从当日0点开始的秒数
			// 例如：9:00:00 = 9*3600 + 0*60 + 0 = 32400秒
			uint32_t startSecs = (section.first/100*60 + section.first%100)*60;
			// 将当前交易时段的结束时间转换为从当日0点开始的秒数
			// 例如：11:30:00 = 11*3600 + 30*60 + 0 = 41400秒
			uint32_t stopSecs = (section.second/100*60 + section.second%100)*60;
			// 检查当前时间是否在当前交易时段范围内（包含边界）
			if(startSecs <= seconds && seconds <= stopSecs)
			{
				// 计算当前时间在当前交易时段内的秒数偏移量
				// 例如：10:30:30在9:00:00-11:30:00时段内，偏移为(10*3600+30*60+30)-(9*3600+0*60+0)=5430秒
				offset += seconds-startSecs;
				// 特殊处理：如果当前时间正好等于交易时段的结束时间，则减去1秒
				// 这是因为交易时段的结束时间通常不包含在交易时间内
				// 例如：11:30:00是11:30时段的结束时间，实际交易时间只到11:29:59
				if(seconds == stopSecs)
					offset--;
				// 标记已找到匹配的交易时段
				bFound = true;
				// 跳出循环，不再检查后续时段
				break;
			}
			else
			{
				// 当前时间不在当前时段内，将当前时段的持续秒数累加到总偏移量中
				// 这样后续时段就能正确计算累计时间
				// 例如：9:00:00-11:30:00时段持续9000秒
				offset += stopSecs - startSecs;
			}
		}

		// 如果没有找到匹配的交易时段，返回无效值
		// 这表示输入时间不在任何交易时段内
		if(!bFound)
			return INVALID_UINT32;

		// 返回计算得到的累计交易秒数
		// 这个值表示从当日第一个交易时段开始到当前时间的累计交易秒数
		return offset;
	}

	/**
	 * 将累计交易秒数转换为具体的交易时间
	 * 
	 * 功能说明：
	 * 将指定的累计交易秒数转换为对应的具体交易时间（HHMMSS格式）。
	 * 该函数是timeToSeconds函数的逆操作，用于将交易时间轴上的相对位置映射回绝对时间。
	 * 支持多时段交易，能够准确计算跨时段的秒数偏移。
	 * 
	 * 计算逻辑：
	 * 1. 检查交易时段列表是否为空，如果为空则无法进行时间转换
	 * 2. 遍历所有交易时段，将累计秒数分配到各个时段中
	 * 3. 如果秒数在当前时段内，则计算该时段内的具体时间
	 * 4. 如果秒数超过当前时段，则减去当前时段的秒数，继续处理下一时段
	 * 5. 将计算结果转换为HHMMSS格式并应用时间偏移
	 * 
	 * 应用场景：
	 * - 回测系统：将回测时间轴上的位置转换为具体的历史时间
	 * - 数据查询：根据交易秒数索引查询对应的具体时间点
	 * - 策略执行：将策略计算的时间偏移转换为实际交易时间
	 * - 时间同步：在不同时间表示之间进行精确转换
	 * - 延迟计算：计算交易指令从发出到执行的时间差
	 * 
	 * 使用示例：
	 * WTSSessionInfo* session = WTSSessionInfo::create("SHFE_RB", "上期螺纹钢", -480);
	 * session->addTradingSection(900, 1015);   // 上午时段 9:00-10:15
	 * session->addTradingSection(1030, 1130); // 上午时段 10:30-11:30
	 * session->addTradingSection(1330, 1500); // 下午时段 13:30-15:00
	 * 
	 * uint32_t time1 = session->secondsToTime(1800);   // 1800秒 -> 93000（9:30:00）
	 * uint32_t time2 = session->secondsToTime(6300);   // 6300秒 -> 110000（11:00:00）
	 * uint32_t time3 = session->secondsToTime(9900);   // 9900秒 -> 140000（14:00:00）
	 * 
	 * @param seconds 累计交易秒数，从当日第一个交易时段开始计算的秒数
	 * @return 对应的交易时间，格式为HHMMSS（如93000表示9:30:00），如果输入无效则返回INVALID_UINT32
	 */
	uint32_t secondsToTime(uint32_t seconds)
	{
		// 检查交易时段列表是否为空，如果为空则无法进行时间转换
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		// 初始化剩余秒数偏移量，用于在遍历交易时段时逐步减少
		// 开始时等于输入的累计秒数，随着处理每个时段会相应减少
		uint32_t offset = seconds;
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，将累计秒数分配到各个时段中
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用，包含开始时间和结束时间
			TradingSection &section = *it;
			// 将当前交易时段的开始时间转换为从当日0点开始的秒数
			// 例如：9:00:00 = 9*3600 + 0*60 + 0 = 32400秒
			uint32_t startSecs = (section.first/100*60 + section.first%100)*60;
			// 将当前交易时段的结束时间转换为从当日0点开始的秒数
			// 例如：10:15:00 = 10*3600 + 15*60 + 0 = 36900秒
			uint32_t stopSecs = (section.second/100*60 + section.second%100)*60;

			// 检查剩余秒数是否大于等于当前时段的持续秒数
			// 如果是，说明目标时间不在当前时段内，需要继续处理下一时段
			if(startSecs + offset >= stopSecs)
			{
				// 从剩余秒数中减去当前时段的持续秒数
				// 这样剩余秒数就表示在后续时段中的偏移量
				offset -= (stopSecs-startSecs);
				// 如果剩余秒数正好为0，说明目标时间正好是当前时段的结束时间
				if(offset == 0)
				{
					// 将当前时段的结束秒数转换为分钟数
					// 例如：36900秒 = 615分钟
					uint32_t desMin = stopSecs/60;
					// 将分钟数转换为HHMM格式，应用时间偏移，然后转换为HHMMSS格式
					// 例如：615分钟 = 10小时15分钟 = 1015，应用偏移后可能变为其他时间
					// 最后加上秒数部分：1015*100 + 0 = 101500
					return originalTime((desMin/60*100 + desMin%60))*100 + stopSecs%60;
				}
			}
			else
			{
				// 目标时间位于当前时段内，计算具体的秒数位置
				// 例如：如果当前时段是9:00:00-10:15:00，剩余秒数是1800秒
				// 则目标时间为9:00:00 + 1800秒 = 9:30:00
				uint32_t desSecs = startSecs+offset;
				// 处理跨日情况：如果计算出的秒数超过一天（86400秒），则减去一天
				// 这主要处理夜盘等跨日交易场景
				if(desSecs >= 86400)
					desSecs -= 86400;

				// 将目标秒数转换为分钟数
				// 例如：34200秒 = 570分钟
				uint32_t desMin = desSecs/60;
				// 将分钟数转换为HHMM格式，应用时间偏移，然后转换为HHMMSS格式
				// 例如：570分钟 = 9小时30分钟 = 930，应用偏移后可能变为其他时间
				// 最后加上秒数部分：930*100 + 0 = 93000
				return originalTime((desMin/60*100 + desMin%60))*100 + desSecs%60;
			}
		}

		// 如果遍历完所有交易时段后仍有剩余秒数，说明输入秒数超出了所有交易时段的总长度
		// 返回无效值表示输入参数错误
		return INVALID_UINT32;
	}

	/**
	 * 获取开盘时间
	 * 
	 * 功能说明：
	 * 返回当日第一个交易时段的开始时间，即开盘时间。
	 * 支持返回原始时间或偏移后时间，用于不同场景的时间比较。
	 * 
	 * @param bOffseted 是否返回偏移后的时间，true返回偏移后时间，false返回原始时间
	 * @return 开盘时间，格式为HHMM（如930表示9:30），如果未配置交易时段则返回0
	 */
	inline uint32_t getOpenTime(bool bOffseted = false) const
	{
		// 检查交易时段列表是否为空，如果为空则返回0
		if(m_tradingTimes.empty())
			return 0;

		// 根据bOffseted参数决定返回原始时间还是偏移后时间
		// 偏移后时间用于内部计算，原始时间用于显示和配置
		return bOffseted ? m_tradingTimes[0].first : m_tradingTimes[0].first_raw;
	}

	/**
	 * 获取集合竞价开始时间
	 * 
	 * 功能说明：
	 * 返回第一个集合竞价时段的开始时间，即集合竞价开始时间。
	 * 支持返回原始时间或偏移后时间，用于不同场景的时间比较。
	 * 
	 * @param bOffseted 是否返回偏移后的时间，true返回偏移后时间，false返回原始时间
	 * @return 集合竞价开始时间，格式为HHMM（如925表示9:25），如果未配置集合竞价时段则返回-1
	 */
	inline uint32_t getAuctionStartTime(bool bOffseted = false) const
	{
		// 检查集合竞价时段列表是否为空，如果为空则返回-1表示无效
		if (m_auctionTimes.empty())
			return -1;

		// 根据bOffseted参数决定返回原始时间还是偏移后时间
		// 偏移后时间用于内部计算，原始时间用于显示和配置
		return bOffseted?m_auctionTimes[0].first: m_auctionTimes[0].first_raw;
	}

	/**
	 * 获取收盘时间
	 * 
	 * 功能说明：
	 * 返回当日最后一个交易时段的结束时间，即收盘时间。
	 * 支持返回原始时间或偏移后时间，用于不同场景的时间比较。
	 * 特殊处理0点收盘的情况，将其转换为2400以便与开盘时间进行比较。
	 * 
	 * @param bOffseted 是否返回偏移后的时间，true返回偏移后时间，false返回原始时间
	 * @return 收盘时间，格式为HHMM（如1500表示15:00），如果未配置交易时段则返回0
	 */
	inline uint32_t getCloseTime(bool bOffseted = false) const
	{
		// 检查交易时段列表是否为空，如果为空则返回0
		if(m_tradingTimes.empty())
			return 0;

		// 获取最后一个交易时段的结束时间（原始时间或偏移后时间）
		uint32_t ret = bOffseted ? m_tradingTimes[m_tradingTimes.size() - 1].second : m_tradingTimes[m_tradingTimes.size() - 1].second_raw;

		// By Wesley @ 2021.12.25
		// 如果收盘时间是0点，无法跟开盘时间进行比较，所以这里要做一个修正
		// 将0点转换为2400点，这样就能正确进行时间比较
		if (ret == 0 && bOffseted)
			ret = 2400;

		return ret;
	}

	/**
	 * 获取总交易秒数
	 * 
	 * 功能说明：
	 * 计算所有交易时段的总交易秒数，用于统计和计算交易时间长度。
	 * 支持多时段交易，自动累加所有时段的交易秒数。
	 * 特殊处理全天候交易时段（24小时交易）。
	 * 
	 * @return 总交易秒数，如果为全天候交易则返回86400秒（24小时）
	 */
	inline uint32_t getTradingSeconds()
	{
		// 初始化总分钟数计数器
		uint32_t count = 0;
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，累加每个时段的交易分钟数
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用
			TradingSection &section = *it;
			// 获取当前时段的开始时间（偏移后时间）
			uint32_t s = section.first;
			// 获取当前时段的结束时间（偏移后时间）
			uint32_t e = section.second;

			// 计算当前时段的小时数
			uint32_t hour = (e/100 - s/100);
			// 计算当前时段的分钟数
			uint32_t minute = (e%100 - s%100);
			// 将小时和分钟转换为总分钟数，并累加到总计数中
			count += hour*60+minute;
		}

		//By Welsey @ 2021.12.25
		//这种只能是全天候交易时段
		// 如果总分钟数为0，说明是全天候交易（24小时），设置为1440分钟
		if (count == 0) count = 1440;
		// 将总分钟数转换为总秒数并返回
		return count*60;
	}

	/**
	 * 获取总交易分钟数
	 * 
	 * 功能说明：
	 * 计算所有交易时段的总交易分钟数，用于统计和计算交易时间长度。
	 * 支持多时段交易，自动累加所有时段的交易分钟数。
	 * 特殊处理全天候交易时段（24小时交易）。
	 * 
	 * @return 总交易分钟数，如果为全天候交易则返回1440分钟（24小时）
	 */
	inline uint32_t getTradingMins()
	{
		// 初始化总分钟数计数器
		uint32_t count = 0;
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，累加每个时段的交易分钟数
		for (; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用
			TradingSection &section = *it;
			// 获取当前时段的开始时间（偏移后时间）
			uint32_t s = section.first;
			// 获取当前时段的结束时间（偏移后时间）
			uint32_t e = section.second;

			// 计算当前时段的小时数
			uint32_t hour = (e / 100 - s / 100);
			// 计算当前时段的分钟数
			uint32_t minute = (e % 100 - s % 100);
			// 将小时和分钟转换为总分钟数，并累加到总计数中
			count += hour * 60 + minute;
		}
		//By Welsey @ 2021.12.25
		//这种只能是全天候交易时段
		// 如果总分钟数为0，说明是全天候交易（24小时），设置为1440分钟
		if (count == 0) count = 1440;
		// 返回总交易分钟数
		return count;
	}

	/**
	 * 获取小节分钟数列表
	 * 
	 * 功能说明：
	 * 返回每个交易时段的累计分钟数列表，用于快速查询各时段的累计时间。
	 * 列表中的每个元素表示从第一个时段开始到当前时段结束的累计分钟数。
	 * 使用静态变量缓存结果，避免重复计算。
	 * 
	 * 计算逻辑：
	 * 1. 遍历所有交易时段，计算每个时段的持续分钟数
	 * 2. 累加前面所有时段的分钟数，得到当前时段的累计分钟数
	 * 3. 将累计分钟数添加到结果列表中
	 * 4. 如果没有配置任何时段，则返回全天候交易（1440分钟）
	 * 
	 * 使用示例：
	 * const auto& minList = session->getSecMinList();
	 * // minList[0] = 75分钟（第一个时段：9:00-10:15）
	 * // minList[1] = 135分钟（前两个时段累计：9:00-11:30）
	 * // minList[2] = 345分钟（所有时段累计：9:00-15:00）
	 * 
	 * @return 小节分钟数列表的常量引用，每个元素为累计分钟数
	 */
	inline const std::vector<uint32_t>& getSecMinList()
	{
		// 使用静态变量缓存计算结果，避免重复计算
		static std::vector<uint32_t> minutes;
		// 如果列表为空，说明还未计算过，需要重新计算
		if(minutes.empty())
		{
			// 初始化累计分钟数计数器
			uint32_t total = 0;
			// 获取交易时段列表的迭代器，准备遍历所有交易时段
			TradingTimes::iterator it = m_tradingTimes.begin();
			// 遍历所有交易时段，计算每个时段的累计分钟数
			for (; it != m_tradingTimes.end(); it++)
			{
				// 获取当前交易时段的引用
				TradingSection &section = *it;
				// 获取当前时段的开始时间（偏移后时间）
				uint32_t s = section.first;
				// 获取当前时段的结束时间（偏移后时间）
				uint32_t e = section.second;

				// 计算当前时段的小时数
				uint32_t hour = (e / 100 - s / 100);
				// 计算当前时段的分钟数
				uint32_t minute = (e % 100 - s % 100);

				// 累加当前时段的分钟数到总计数中
				total += hour * 60 + minute;
				// 将当前累计分钟数添加到结果列表中
				minutes.emplace_back(total);
			}
			
			// 如果没有配置任何交易时段，则设置为全天候交易（1440分钟）
			if (minutes.empty())
				minutes.emplace_back(1440);
		}
		
		// 返回缓存的分钟数列表
		return minutes;
	}

	/**
	 * 判断是否处于交易时间
	 * 
	 * 功能说明：
	 * 判断指定时间是否处于交易时段内，支持严格模式和普通模式。
	 * 严格模式下，交易时段的最后一分钟不算作交易时间。
	 * 
	 * 判断逻辑：
	 * 1. 调用timeToMinutes函数将时间转换为交易分钟数
	 * 2. 如果转换失败（返回INVALID_UINT32），则不在交易时间内
	 * 3. 如果启用严格模式且是时段最后一分钟，则不算交易时间
	 * 4. 否则认为在交易时间内
	 * 
	 * 使用示例：
	 * bool inTrading = session->isInTradingTime(930);   // 9:30是否在交易时间内
	 * bool strict = session->isInTradingTime(1500, true); // 15:00在严格模式下不算交易时间
	 * 
	 * @param uTime 输入时间，格式为HHMM（如930表示9:30）
	 * @param bStrict 是否严格检查，true表示严格模式，false表示普通模式
	 * @return true表示在交易时间内，false表示不在交易时间内
	 */
	bool	isInTradingTime(uint32_t uTime, bool bStrict = false)
	{
		// 将输入时间转换为交易分钟数，用于判断是否在交易时段内
		uint32_t count = timeToMinutes(uTime);
		// 如果转换失败，说明不在任何交易时段内
		if(count == INVALID_UINT32)
			return false;

		// 如果启用严格模式且是时段最后一分钟，则不算交易时间
		// 例如：15:00是15:00-15:00时段的结束时间，严格模式下不算交易时间
		if (bStrict && isLastOfSection(uTime))
			return false;

		// 其他情况都认为在交易时间内
		return true;
	}

	/**
	 * 判断是否为交易时段最后一分钟
	 * 
	 * 功能说明：
	 * 判断指定时间是否为某个交易时段的结束时间（最后一分钟）。
	 * 用于严格模式下的交易时间判断，时段最后一分钟通常不算作交易时间。
	 * 
	 * 判断逻辑：
	 * 遍历所有交易时段，检查输入时间是否等于任何时段的结束时间。
	 * 使用原始时间进行比较，确保判断的准确性。
	 * 
	 * 使用示例：
	 * bool isLast = session->isLastOfSection(1500);  // 15:00是否为某个时段的结束时间
	 * 
	 * @param uTime 输入时间，格式为HHMM（如1500表示15:00）
	 * @return true表示是某个时段的最后一分钟，false表示不是
	 */
	inline bool	isLastOfSection(uint32_t uTime)
	{
		//uint32_t offTime = offsetTime(uTime, false);
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，检查输入时间是否为任何时段的结束时间
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用
			TradingSection &section = *it;
			// 检查输入时间是否等于当前时段的原始结束时间
			if(section.second_raw == uTime)
				return true;
		}

		// 遍历完所有时段都没有找到匹配的结束时间，返回false
		return false;
	}

	/**
	 * 判断是否为交易时段第一分钟
	 * 
	 * 功能说明：
	 * 判断指定时间是否为某个交易时段的开始时间（第一分钟）。
	 * 用于识别交易时段的开盘时间点，通常用于开盘信号检测。
	 * 
	 * 判断逻辑：
	 * 遍历所有交易时段，检查输入时间是否等于任何时段的开始时间。
	 * 使用原始时间进行比较，确保判断的准确性。
	 * 
	 * 使用示例：
	 * bool isFirst = session->isFirstOfSection(930);   // 9:30是否为某个时段的开始时间
	 * bool isOpen = session->isFirstOfSection(900);    // 9:00是否为开盘时间
	 * 
	 * @param uTime 输入时间，格式为HHMM（如930表示9:30）
	 * @return true表示是某个时段的第一分钟，false表示不是
	 */
	inline bool	isFirstOfSection(uint32_t uTime)
	{
		//uint32_t offTime = offsetTime(uTime, true);
		// 获取交易时段列表的迭代器，准备遍历所有交易时段
		TradingTimes::iterator it = m_tradingTimes.begin();
		// 遍历所有交易时段，检查输入时间是否为任何时段的开始时间
		for(; it != m_tradingTimes.end(); it++)
		{
			// 获取当前交易时段的引用
			TradingSection &section = *it;
			// 检查输入时间是否等于当前时段的原始开始时间
			if(section.first_raw == uTime)
				return true;
		}

		// 遍历完所有时段都没有找到匹配的开始时间，返回false
		return false;
	}

	/**
	 * 判断是否处于集合竞价时间
	 * 
	 * 功能说明：
	 * 判断指定时间是否处于集合竞价时段内，支持多个集合竞价时段。
	 * 集合竞价时间通常为开盘前几分钟，此时市场处于集合竞价状态。
	 * 
	 * 判断逻辑：
	 * 1. 将输入时间进行偏移处理，处理夜盘等跨日交易场景
	 * 2. 遍历所有集合竞价时段，检查偏移后时间是否在任意时段内
	 * 3. 跳过无效的集合竞价时段（开始和结束时间都为0）
	 * 4. 使用左闭右开区间判断（包含开始时间，不包含结束时间）
	 * 
	 * 使用示例：
	 * bool inAuction = session->isInAuctionTime(925);  // 9:25是否在集合竞价时间内
	 * bool preOpen = session->isInAuctionTime(930);    // 9:30是否在集合竞价时间内
	 * 
	 * @param uTime 输入时间，格式为HHMM（如925表示9:25）
	 * @return true表示在集合竞价时间内，false表示不在集合竞价时间内
	 */
	inline bool	isInAuctionTime(uint32_t uTime)
	{
		// 对输入时间进行偏移处理，将原始时间转换为偏移后的时间
		// 主要用于处理夜盘等跨日交易场景，偏移量由m_uOffsetMins决定
		uint32_t offTime = offsetTime(uTime, true);
		
		// 遍历所有集合竞价时段，检查偏移后时间是否在任意时段内
		for(const TradingSection& aucSec : m_auctionTimes)
		{
			// 跳过无效的集合竞价时段（开始和结束时间都为0表示未配置）
			if (aucSec.first == 0 && aucSec.second == 0)
				continue;

			// 检查偏移后时间是否在当前集合竞价时段内（左闭右开区间）
			// 例如：集合竞价时段9:25-9:30，9:25包含在内，9:30不包含在内
			if (aucSec.first <= offTime && offTime < aucSec.second)
				return true;
		}
		

		// 遍历完所有集合竞价时段都没有找到匹配的时段，返回false
		return false;
	}

	/**
	 * 计算偏移时间
	 * 
	 * 功能说明：
	 * 根据时间偏移量计算偏移后的时间，主要用于夜盘等跨日交易场景。
	 * 支持左对齐和右对齐两种模式，处理0点边界的不同情况。
	 * 
	 * 计算逻辑：
	 * 1. 如果偏移量为0，直接返回原时间
	 * 2. 将时间转换为分钟数并加上偏移量
	 * 3. 根据对齐模式处理跨日情况：
	 *    - 左对齐：0点按0点处理，24点按0点处理
	 *    - 右对齐：0点按24点处理，24点按24点处理
	 * 4. 将处理后的分钟数转换回HHMM格式
	 * 
	 * 对齐模式说明：
	 * - 左对齐：适用于开始时间偏移，0点结束的时段按0点计算
	 * - 右对齐：适用于结束时间偏移，0点结束的时段按24点计算
	 * 
	 * 使用示例：
	 * uint32_t offsetTime = session->offsetTime(2100, true);   // 21:00左对齐偏移
	 * uint32_t nextDay = session->offsetTime(2300, true);     // 23:00可能偏移到次日
	 * 
	 * @param uTime 原始时间，格式为HHMM（如2100表示21:00）
	 * @param bAlignLeft 是否向左对齐，true表示左对齐，false表示右对齐
	 * @return 偏移后的时间，格式为HHMM
	 */
	inline uint32_t	offsetTime(uint32_t uTime, bool bAlignLeft) const
	{
		// 如果偏移量为0，直接返回原时间，无需计算
		if (m_uOffsetMins == 0)
			return uTime;

		// 将时间转换为分钟数：小时*60 + 分钟
		// 例如：2100 -> 21*60 + 0 = 1260分钟
		int32_t curMinute = (uTime/100)*60 + uTime%100;
		// 加上时间偏移量，正数向后偏移，负数向前偏移
		curMinute += m_uOffsetMins;
		
		// 根据对齐模式处理跨日情况
		if(bAlignLeft)  // 左对齐模式
		{
			// 如果偏移后超过一天（1440分钟），则减去一天
			if (curMinute >= 1440)
				curMinute -= 1440;
			// 如果偏移后为负数，则加上一天
			else if (curMinute < 0)
				curMinute += 1440;
		}
		else  // 右对齐模式
		{
			// 如果偏移后大于一天（1440分钟），则减去一天
			if (curMinute > 1440)
				curMinute -= 1440;
			// 如果偏移后小于等于0，则加上一天
			else if (curMinute <= 0)
				curMinute += 1440;
		}
		
		// 将处理后的分钟数转换回HHMM格式
		// 例如：570分钟 -> 9小时30分钟 = 930
		return (curMinute/60)*100 + curMinute%60;
	}

	/**
	 * 计算原始时间
	 * 
	 * 功能说明：
	 * 将偏移后的时间还原为原始时间，是offsetTime函数的逆操作。
	 * 主要用于将内部计算使用的偏移时间转换回用户配置的原始时间。
	 * 
	 * 计算逻辑：
	 * 1. 如果偏移量为0，直接返回原时间
	 * 2. 将时间转换为分钟数并减去偏移量
	 * 3. 处理跨日情况：超过一天则减去一天，为负数则加上一天
	 * 4. 将处理后的分钟数转换回HHMM格式
	 * 
	 * 应用场景：
	 * - 时间显示：将内部偏移时间转换为用户可见的原始时间
	 * - 数据输出：将计算结果转换为原始时间格式
	 * - 时间比较：与用户配置的时间进行比较
	 * 
	 * 使用示例：
	 * uint32_t original = session->originalTime(930);   // 将偏移后的9:30还原为原始时间
	 * uint32_t display = session->originalTime(1500);   // 将偏移后的15:00还原为原始时间
	 * 
	 * @param uTime 偏移后的时间，格式为HHMM（如930表示9:30）
	 * @return 原始时间，格式为HHMM
	 */
	inline uint32_t	originalTime(uint32_t uTime) const
	{
		// 如果偏移量为0，直接返回原时间，无需计算
		if (m_uOffsetMins == 0)
			return uTime;

		// 将时间转换为分钟数：小时*60 + 分钟
		// 例如：930 -> 9*60 + 30 = 570分钟
		int32_t curMinute = (uTime/100)*60 + uTime%100;
		// 减去时间偏移量，还原为原始时间
		curMinute -= m_uOffsetMins;
		
		// 处理跨日情况：如果还原后的分钟数超过一天（1440分钟），则减去一天
		if(curMinute >= 1440)
			curMinute -= 1440;
		// 如果还原后的分钟数为负数，则加上一天
		else if(curMinute < 0)
			curMinute += 1440;

		// 将处理后的分钟数转换回HHMM格式
		// 例如：570分钟 -> 9小时30分钟 = 930
		return (curMinute/60)*100 + curMinute%60;
	}
};

NS_WTP_END