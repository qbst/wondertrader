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
	uint32_t getSectionCount() const{ return (uint32_t)m_tradingTimes.size(); }

	/**
	 * 计算偏移以后的日期
	 * 主要用于各种日期比较，如夜盘的偏移日期都是下一日
	 * @param uDate 日期，格式为YYYYMMDD，0表示当前日期
	 * @param uTime 时间，格式为HHMM，0表示当前时间
	 * @return 偏移后的日期
	 */
	uint32_t getOffsetDate(uint32_t uDate = 0, uint32_t uTime = 0)
	{
		if(uDate == 0)
		{
			TimeUtils::getDateTime(uDate, uTime);
			uTime /= 100000;
		}

		int32_t curMinute = (uTime / 100) * 60 + uTime % 100;
		curMinute += m_uOffsetMins;

		if (curMinute >= 1440)
			return TimeUtils::getNextDate(uDate);

		if (curMinute < 0)
			return TimeUtils::getNextDate(uDate, -1);

		return uDate;
	}

	/**
	 * 将时间转换成分钟数
	 * 从开盘时间开始计算，返回当前时间对应的交易分钟数
	 * @param uTime 当前时间，格式如0910表示9:10
	 * @param autoAdjust 是否自动调整，如果开启，非交易时间内的行情会自动对齐到下一个交易时间
	 * @return 交易分钟数，非交易时间返回INVALID_UINT32
	 */
	uint32_t timeToMinutes(uint32_t uTime, bool autoAdjust = false)
	{
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		if(isInAuctionTime(uTime))
			return 0;

		uint32_t offTime = offsetTime(uTime, true);

		uint32_t offset = 0;
		bool bFound = false;
		auto it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			if (section.first <= offTime && offTime <= section.second)
			{
				int32_t hour = offTime / 100 - section.first / 100;
				int32_t minute = offTime % 100 - section.first % 100;
				offset += hour*60 + minute;
				bFound = true;
				break;
			}
			else if(offTime > section.second)	//大于上边界
			{
				int32_t hour = section.second/100 - section.first/100;
				int32_t minute = section.second%100 - section.first%100;
				offset += hour*60 + minute;
			} 
			else //小于下边界
			{
				if(autoAdjust)
				{
					bFound = true;
				}
				break;
			}
		}

		//没找到就返回0
		if(!bFound)
			return INVALID_UINT32;

		return offset;
	}

	uint32_t minuteToTime(uint32_t uMinutes, bool bHeadFirst = false)
	{
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		uint32_t offset = uMinutes;
		TradingTimes::iterator it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			uint32_t startMin = section.first/100*60 + section.first%100;
			uint32_t stopMin = section.second/100*60 + section.second%100;

			if(!bHeadFirst)
			{
				if (startMin + offset >= stopMin)
				{
					offset -= (stopMin - startMin);
					if (offset == 0)
					{
						return originalTime(stopMin / 60 * 100 + stopMin % 60);
					}
				}
				else
				{
					//干好位于该区间
					uint32_t desMin = startMin + offset;
					if (desMin >= 1440)
						desMin -= 1440;

					return originalTime(desMin / 60 * 100 + desMin % 60);
				}
			}
			else
			{
				if (startMin + offset < stopMin)
				{
					//干好位于该区间
					uint32_t desMin = startMin + offset;
					if (desMin >= 1440)
						desMin -= 1440;

					return originalTime(desMin / 60 * 100 + desMin % 60);
				}
				else
				{
					offset -= (stopMin - startMin);
				}
			}
		}

		return getCloseTime();
	}

	uint32_t timeToSeconds(uint32_t uTime)
	{
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		//如果是集合竞价的价格,则认为是0秒价格
		if(isInAuctionTime(uTime/100))
			return 0;

		uint32_t sec = uTime%100;
		uint32_t h = uTime/10000;
		uint32_t m = uTime%10000/100;
		uint32_t offMin = offsetTime(h*100 + m, true);
		h = offMin/100;
		m = offMin%100;
		uint32_t seconds = h*60*60 + m*60 + sec;

		uint32_t offset = 0;
		bool bFound = false;
		TradingTimes::iterator it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			uint32_t startSecs = (section.first/100*60 + section.first%100)*60;
			uint32_t stopSecs = (section.second/100*60 + section.second%100)*60;
			//uint32_t s = section.first;
			//uint32_t e = section.second;
			//uint32_t hour = (e/100 - s/100);
			//uint32_t minute = (e%100 - s%100);
			if(startSecs <= seconds && seconds <= stopSecs)
			{
				offset += seconds-startSecs;
				if(seconds == stopSecs)
					offset--;
				bFound = true;
				break;
			}
			else
			{
				offset += stopSecs - startSecs;
			}
		}

		//没找到就返回0
		if(!bFound)
			return INVALID_UINT32;

		return offset;
	}

	uint32_t secondsToTime(uint32_t seconds)
	{
		if(m_tradingTimes.empty())
			return INVALID_UINT32;

		uint32_t offset = seconds;
		TradingTimes::iterator it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			uint32_t startSecs = (section.first/100*60 + section.first%100)*60;
			uint32_t stopSecs = (section.second/100*60 + section.second%100)*60;

			if(startSecs + offset >= stopSecs)
			{
				offset -= (stopSecs-startSecs);
				if(offset == 0)
				{
					uint32_t desMin = stopSecs/60;
					return originalTime((desMin/60*100 + desMin%60))*100 + stopSecs%60;
				}
			}
			else
			{
				//干好位于该区间
				uint32_t desSecs = startSecs+offset;
				if(desSecs >= 86400)
					desSecs -= 86400;

				uint32_t desMin = desSecs/60;
				return originalTime((desMin/60*100 + desMin%60))*100 + desSecs%60;
			}
		}

		return INVALID_UINT32;
	}

	inline uint32_t getOpenTime(bool bOffseted = false) const
	{
		if(m_tradingTimes.empty())
			return 0;

		return bOffseted ? m_tradingTimes[0].first : m_tradingTimes[0].first_raw;
	}

	inline uint32_t getAuctionStartTime(bool bOffseted = false) const
	{
		if (m_auctionTimes.empty())
			return -1;

		return bOffseted?m_auctionTimes[0].first: m_auctionTimes[0].first_raw;
	}

	inline uint32_t getCloseTime(bool bOffseted = false) const
	{
		if(m_tradingTimes.empty())
			return 0;

		uint32_t ret = bOffseted ? m_tradingTimes[m_tradingTimes.size() - 1].second : m_tradingTimes[m_tradingTimes.size() - 1].second_raw;

		// By Wesley @ 2021.12.25
		// 如果收盘时间是0点，无法跟开盘时间进行比较，所以这里要做一个修正
		if (ret == 0 && bOffseted)
			ret = 2400;

		return ret;
	}

	inline uint32_t getTradingSeconds()
	{
		uint32_t count = 0;
		TradingTimes::iterator it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			uint32_t s = section.first;
			uint32_t e = section.second;

			uint32_t hour = (e/100 - s/100);
			uint32_t minute = (e%100 - s%100);
			count += hour*60+minute;
		}

		//By Welsey @ 2021.12.25
		//这种只能是全天候交易时段
		if (count == 0) count = 1440;
		return count*60;
	}

	/*
	 *	获取交易的分钟数
	 */
	inline uint32_t getTradingMins()
	{
		uint32_t count = 0;
		TradingTimes::iterator it = m_tradingTimes.begin();
		for (; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			uint32_t s = section.first;
			uint32_t e = section.second;

			uint32_t hour = (e / 100 - s / 100);
			uint32_t minute = (e % 100 - s % 100);
			count += hour * 60 + minute;
		}
		//By Welsey @ 2021.12.25
		//这种只能是全天候交易时段
		if (count == 0) count = 1440;
		return count;
	}

	/*
	 *	获取小节分钟数列表
	 */
	inline const std::vector<uint32_t>& getSecMinList()
	{
		static std::vector<uint32_t> minutes;
		if(minutes.empty())
		{
			uint32_t total = 0;
			TradingTimes::iterator it = m_tradingTimes.begin();
			for (; it != m_tradingTimes.end(); it++)
			{
				TradingSection &section = *it;
				uint32_t s = section.first;
				uint32_t e = section.second;

				uint32_t hour = (e / 100 - s / 100);
				uint32_t minute = (e % 100 - s % 100);

				total += hour * 60 + minute;
				minutes.emplace_back(total);
			}
			
			if (minutes.empty())
				minutes.emplace_back(1440);
		}
		
		return minutes;
	}

	/*
	 *	是否处于交易时间
	 *	@uTime		时间，格式为hhmm
	 *	@bStrict	是否严格检查，如果是严格检查
	 *				则在每一交易时段最后一分钟，如1500，不属于交易时间
	 */
	bool	isInTradingTime(uint32_t uTime, bool bStrict = false)
	{
		uint32_t count = timeToMinutes(uTime);
		if(count == INVALID_UINT32)
			return false;

		if (bStrict && isLastOfSection(uTime))
			return false;

		return true;
	}

	inline bool	isLastOfSection(uint32_t uTime)
	{
		//uint32_t offTime = offsetTime(uTime, false);
		TradingTimes::iterator it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			if(section.second_raw == uTime)
				return true;
		}

		return false;
	}

	inline bool	isFirstOfSection(uint32_t uTime)
	{
		//uint32_t offTime = offsetTime(uTime, true);
		TradingTimes::iterator it = m_tradingTimes.begin();
		for(; it != m_tradingTimes.end(); it++)
		{
			TradingSection &section = *it;
			if(section.first_raw == uTime)
				return true;
		}

		return false;
	}

	inline bool	isInAuctionTime(uint32_t uTime)
	{
		uint32_t offTime = offsetTime(uTime, true);
		
		for(const TradingSection& aucSec : m_auctionTimes)
		{
			if (aucSec.first == 0 && aucSec.second == 0)
				continue;

			if (aucSec.first <= offTime && offTime < aucSec.second)
				return true;
		}
		

		return false;
	}

	/*
	 *	计算偏移时间
	 *	@uTime		原始时间
	 *	@bAlignLeft	是否向左对齐，这个主要针对0点结束的情况
	 *				如果向左对齐，则0点就做0点算
	 *				如果向右对齐，则0点就做24点算
	 */
	inline uint32_t	offsetTime(uint32_t uTime, bool bAlignLeft) const
	{
		if (m_uOffsetMins == 0)
			return uTime;

		int32_t curMinute = (uTime/100)*60 + uTime%100;
		curMinute += m_uOffsetMins;
		if(bAlignLeft)
		{
			if (curMinute >= 1440)
				curMinute -= 1440;
			else if (curMinute < 0)
				curMinute += 1440;
		}
		else
		{
			if (curMinute > 1440)
				curMinute -= 1440;
			else if (curMinute <= 0)
				curMinute += 1440;
		}
		
		return (curMinute/60)*100 + curMinute%60;
	}

	inline uint32_t	originalTime(uint32_t uTime) const
	{
		if (m_uOffsetMins == 0)
			return uTime;

		int32_t curMinute = (uTime/100)*60 + uTime%100;
		curMinute -= m_uOffsetMins;
		if(curMinute >= 1440)
			curMinute -= 1440;
		else if(curMinute < 0)
			curMinute += 1440;

		return (curMinute/60)*100 + curMinute%60;
	}
};

NS_WTP_END