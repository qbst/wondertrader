/*!
 * \file TimeUtils.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 时间处理的封装
 * 
 * 该文件提供了全面的时间处理工具类，主要包括：
 * 1. 高精度时间获取功能（毫秒级精度）
 * 2. 时间格式化和转换功能
 * 3. 日期时间计算和操作功能
 * 4. 时区偏移计算功能
 * 5. 时间戳生成和解析功能
 * 6. 高性能计时器功能
 * 
 * 设计逻辑：
 * - 通过条件编译实现Windows和Linux/Unix的跨平台兼容
 * - Windows平台使用系统共享数据结构获取高精度时间
 * - Linux/Unix平台使用clock_gettime获取高精度时间
 * - 提供多种时间格式和表示方式，满足不同需求
 * - 使用thread_local优化多线程环境下的性能
 * - 支持毫秒级精度的时间操作
 * 
 * 主要作用：
 * - 为WonderTrader框架提供高精度的时间管理能力
 * - 支持高频交易等对时间精度要求极高的场景
 * - 提供统一的时间接口，简化时间相关代码编写
 * - 支持跨平台的时间操作兼容性
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含固定大小整数类型
#include <sys/timeb.h>  // 包含时间相关结构体
#ifdef _MSC_VER  // Microsoft Visual C++编译器
#include <time.h>  // Windows平台的时间函数
#else  // 非Windows平台
#include <sys/time.h>  // Unix/Linux平台的时间函数
#endif
#include <string>  // 包含字符串支持
#include <string.h>  // 包含C风格字符串函数
#include<chrono>  // 包含现代C++时间库
#include <thread>  // 包含线程支持
#include <cmath>  // 包含数学函数

#ifdef _MSC_VER  // Windows平台特定代码
#define CTIME_BUF_SIZE 64  // 定义时间缓冲区大小

#define WIN32_LEAN_AND_MEAN  // 减少Windows头文件包含

#include <windows.h>  // Windows API头文件

/**
 * @struct _KSYSTEM_TIME
 * @brief Windows系统时间结构体
 * 
 * 该结构体用于存储Windows系统的高精度时间信息。
 * 包含低32位和高32位时间值。
 */
typedef struct _KSYSTEM_TIME
{
	ULONG LowPart;    // 时间低32位部分
	LONG High1Time;   // 时间高32位部分1
	LONG High2Time;   // 时间高32位部分2（用于验证一致性）
} KSYSTEM_TIME, *PKSYSTEM_TIME;  // 结构体类型定义和指针类型

/**
 * @struct KUSER_SHARED_DATA
 * @brief Windows用户共享数据结构体
 * 
 * 该结构体包含系统共享的时间信息，用于获取高精度系统时间。
 * 所有字段都是易变的，确保多线程环境下的正确性。
 */
struct KUSER_SHARED_DATA
{
	ULONG TickCountLowDeprecated;  // 已废弃的滴答计数低32位
	ULONG TickCountMultiplier;     // 滴答计数乘数
	volatile KSYSTEM_TIME InterruptTime;  // 中断时间（易变）
	volatile KSYSTEM_TIME SystemTime;     // 系统时间（易变）
	volatile KSYSTEM_TIME TimeZoneBias;   // 时区偏移（易变）
};

#define KI_USER_SHARED_DATA   0x7FFE0000  // 用户共享数据的内存地址
#define SharedUserData   ((KUSER_SHARED_DATA * const)KI_USER_SHARED_DATA)  // 用户共享数据指针

#define TICKSPERSEC        10000000L  // 每秒的滴答数（100纳秒单位）
#endif

/**
 * @class TimeUtils
 * @brief 时间工具类
 * 
 * 该类提供了全面的时间处理功能，包括高精度时间获取、时间格式化、
 * 日期计算、时区处理等。支持跨平台使用，提供毫秒级精度。
 */
class TimeUtils 
{
	
public:
	/**
	 * @brief 获取本地时间（旧版本实现）
	 * @return int64_t 返回毫秒级时间戳
	 * 
	 * 该函数使用ftime获取本地时间，精度为毫秒。
	 * 使用thread_local静态变量优化多线程性能。
	 * 
	 * 注意：这是旧版本实现，建议使用getLocalTimeNow()。
	 */
	static inline int64_t getLocalTimeNowOld(void)
	{
		thread_local static timeb now;  // 线程本地静态时间结构体
		ftime(&now);  // 获取当前时间
		return now.time * 1000 + now.millitm;  // 转换为毫秒级时间戳
	}

	/**
	 * @brief 获取本地时间，精确到毫秒
	 * @return int64_t 返回毫秒级时间戳
	 * 
	 * 该函数提供高精度的本地时间获取功能：
	 * - Windows平台：使用系统共享数据结构获取高精度时间
	 * - Linux/Unix平台：使用clock_gettime获取高精度时间
	 * 
	 * 注意：Linux平台下clock_gettime比ftime性能提升约10%。
	 */
	static inline int64_t getLocalTimeNow(void)
	{
#ifdef _MSC_VER  // Windows平台
		LARGE_INTEGER SystemTime;  // 声明大整数结构体
		do  // 循环直到获取一致的时间值
		{
			SystemTime.HighPart = SharedUserData->SystemTime.High1Time;  // 获取高32位时间1
			SystemTime.LowPart = SharedUserData->SystemTime.LowPart;     // 获取低32位时间
		} while (SystemTime.HighPart != SharedUserData->SystemTime.High2Time);  // 验证高32位一致性

		uint64_t t = SystemTime.QuadPart;  // 获取64位时间值
		t = t - 11644473600L * TICKSPERSEC;  // 转换为Unix时间戳（从1601年1月1日开始）
		return t / 10000;  // 转换为毫秒级时间戳
#else  // Linux/Unix平台
		//timeb now;
		//ftime(&now);
		//return now.time * 1000 + now.millitm;
		/*
		 *	clock_gettime比ftime会提升约10%的性能
		 */
		thread_local static struct timespec now;  // 线程本地静态时间规格结构体
		clock_gettime(CLOCK_REALTIME, &now);  // 获取实时时钟时间
		return now.tv_sec * 1000 + now.tv_nsec / 1000000;  // 转换为毫秒级时间戳
#endif
	}

	/**
	 * @brief 获取格式化的本地时间字符串
	 * @param bIncludeMilliSec 是否包含毫秒，默认为true
	 * @return std::string 返回格式化的时间字符串
	 * 
	 * 该函数返回格式化的时间字符串，格式为HH:MM:SS或HH:MM:SS,mmm。
	 * 支持选择是否包含毫秒部分。
	 */
	static inline std::string getLocalTime(bool bIncludeMilliSec = true)
	{
		uint64_t ltime = getLocalTimeNow();  // 获取当前毫秒级时间戳
		time_t now = ltime / 1000;  // 转换为秒级时间戳
		uint32_t millitm = ltime % 1000;  // 提取毫秒部分
		tm * tNow = localtime(&now);  // 转换为本地时间结构体

		char str[64] = {0};  // 初始化时间字符串缓冲区
		if(bIncludeMilliSec)  // 如果需要包含毫秒
			sprintf(str, "%02d:%02d:%02d,%03d", tNow->tm_hour, tNow->tm_min, tNow->tm_sec, millitm);  // 格式化包含毫秒的时间
		else  // 如果不包含毫秒
			sprintf(str, "%02d:%02d:%02d", tNow->tm_hour, tNow->tm_min, tNow->tm_sec);  // 格式化不包含毫秒的时间
		return str;  // 返回格式化的时间字符串
	}

	/**
	 * @brief 获取YYYYMMDDhhmmss格式的时间戳
	 * @return uint64_t 返回格式化的时间戳
	 * 
	 * 该函数返回YYYYMMDDhhmmss格式的时间戳，精确到秒。
	 * 格式：前8位为日期（YYYYMMDD），后6位为时间（hhmmss）。
	 */
	static inline uint64_t getYYYYMMDDhhmmss()
	{
		uint64_t ltime = getLocalTimeNow();  // 获取当前毫秒级时间戳
		time_t now = ltime / 1000;  // 转换为秒级时间戳

		tm * tNow = localtime(&now);  // 转换为本地时间结构体

		uint64_t date = (tNow->tm_year + 1900) * 10000 + (tNow->tm_mon + 1) * 100 + tNow->tm_mday;  // 计算日期部分

		uint64_t time = tNow->tm_hour * 10000 + tNow->tm_min * 100 + tNow->tm_sec;  // 计算时间部分
		return date * 1000000 + time;  // 组合日期和时间
	}

    /**
     * @brief 读取当前日期和时间
     * @param date 输出参数，当前日期，格式如20220309
     * @param time 输出参数，当前时间，精确到毫秒，格式如103029500
     * 
     * 该函数将当前日期和时间分别存储到指定的参数中。
     * 日期格式为YYYYMMDD，时间格式为HHMMSSmmm（包含毫秒）。
     */
	static inline void getDateTime(uint32_t &date, uint32_t &time)
	{
		uint64_t ltime = getLocalTimeNow();  // 获取当前毫秒级时间戳
		time_t now = ltime / 1000;  // 转换为秒级时间戳
		uint32_t millitm = ltime % 1000;  // 提取毫秒部分

		tm * tNow = localtime(&now);  // 转换为本地时间结构体

		date = (tNow->tm_year+1900)*10000 + (tNow->tm_mon+1)*100 + tNow->tm_mday;  // 计算日期部分
		
		time = tNow->tm_hour*10000 + tNow->tm_min*100 + tNow->tm_sec;  // 计算时间部分（不含毫秒）
		time *= 1000;  // 转换为毫秒级时间
		time += millitm;  // 添加毫秒部分
	}

	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期，格式为YYYYMMDD
	 * 
	 * 该函数返回当前日期，格式为8位数字：YYYYMMDD。
	 */
	static inline uint32_t getCurDate()
	{
		uint64_t ltime = getLocalTimeNow();  // 获取当前毫秒级时间戳
		time_t now = ltime / 1000;  // 转换为秒级时间戳
		tm * tNow = localtime(&now);  // 转换为本地时间结构体
		uint32_t date = (tNow->tm_year+1900)*10000 + (tNow->tm_mon+1)*100 + tNow->tm_mday;  // 计算日期部分

		return date;  // 返回当前日期
	}

	/**
	 * @brief 获取指定日期的星期几
	 * @param uDate 日期，格式为YYYYMMDD，0表示当前日期
	 * @return uint32_t 返回星期几（0=星期日，1=星期一，...，6=星期六）
	 * 
	 * 该函数计算指定日期是星期几。
	 * 如果未指定日期，则使用当前日期。
	 */
	static inline uint32_t getWeekDay(uint32_t uDate = 0)
	{
		time_t ts = 0;  // 初始化时间戳
		if(uDate == 0)  // 如果未指定日期
		{
			ts = getLocalTimeNow()/1000;  // 使用当前时间
		}
		else  // 如果指定了日期
		{
			tm t;	 // 声明时间结构体
			memset(&t,0,sizeof(tm));  // 清零时间结构体
			t.tm_year = uDate/10000 - 1900;  // 设置年份（从1900年开始）
			t.tm_mon = (uDate%10000)/100 - 1;  // 设置月份（从0开始）
			t.tm_mday = uDate % 100;  // 设置日期
			ts = mktime(&t);  // 转换为时间戳
		}

		tm * tNow = localtime(&ts);  // 转换为本地时间结构体
	
		return tNow->tm_wday;  // 返回星期几
	}

	/**
	 * @brief 获取当前时间（不含毫秒）
	 * @return uint32_t 返回当前时间，格式为HHMMSS
	 * 
	 * 该函数返回当前时间，格式为6位数字：HHMMSS。
	 * 不包含毫秒部分。
	 */
	static inline uint32_t getCurMin()
	{
		uint64_t ltime = getLocalTimeNow();  // 获取当前毫秒级时间戳
		time_t now = ltime / 1000;  // 转换为秒级时间戳
		tm * tNow = localtime(&now);  // 转换为本地时间结构体
		uint32_t time = tNow->tm_hour*10000 + tNow->tm_min*100 + tNow->tm_sec;  // 计算时间部分

		return time;  // 返回当前时间
	}

	/**
	 * @brief 获取时区偏移量
	 * @return int32_t 返回时区偏移量（小时）
	 * 
	 * 该函数计算当前时区相对于UTC的偏移量。
	 * 使用静态变量缓存结果，避免重复计算。
	 * 正值表示东时区，负值表示西时区。
	 */
	static inline int32_t getTZOffset()
	{
		static int32_t offset = 99;  // 静态偏移量变量，99表示未初始化
		if(offset == 99)  // 如果未初始化
		{
			time_t now = time(NULL);  // 获取当前时间
			tm tm_ltm = *localtime(&now);  // 获取本地时间结构体
			tm tm_gtm = *gmtime(&now);     // 获取UTC时间结构体

			time_t _gt = mktime(&tm_gtm);  // 将UTC时间转换为时间戳
			tm _gtm2 = *localtime(&_gt);   // 将时间戳转换为本地时间

			offset = (uint32_t)(((now - _gt) + (_gtm2.tm_isdst ? 3600 : 0)) / 60);  // 计算时区偏移（分钟）
			offset /= 60;  // 转换为小时
		}

		return offset;  // 返回时区偏移量
	}

	/**
	 * @brief 生成带毫秒的时间戳
	 * @param lDate 日期，格式为yyyymmdd
	 * @param lTimeWithMs 带毫秒的时间，格式为HHMMSSsss
	 * @param isToUTC 是否转换为UTC时间，默认为false
	 * @return int64_t 返回毫秒级时间戳
	 * 
	 * 该函数根据指定的日期和时间生成毫秒级时间戳。
	 * 支持转换为UTC时间，自动处理时区偏移。
	 */
	static inline int64_t makeTime(long lDate, long lTimeWithMs, bool isToUTC = false)
	{
		tm t;	 // 声明时间结构体
		memset(&t,0,sizeof(tm));  // 清零时间结构体
		t.tm_year = lDate/10000 - 1900;  // 设置年份（从1900年开始）
		t.tm_mon = (lDate%10000)/100 - 1;  // 设置月份（从0开始）
		t.tm_mday = lDate % 100;  // 设置日期
		t.tm_hour = lTimeWithMs/10000000;  // 设置小时
		t.tm_min = (lTimeWithMs%10000000)/100000;  // 设置分钟
		t.tm_sec = (lTimeWithMs%100000)/1000;  // 设置秒
		int millisec = lTimeWithMs%1000;  // 提取毫秒部分
		//t.tm_isdst 	
		time_t ts = mktime(&t);  // 转换为时间戳
		if (ts == -1) return 0;  // 如果转换失败，返回0
		//如果要转成UTC时间，则需要根据时区进行转换
		if (isToUTC)  // 如果需要转换为UTC时间
			ts -= getTZOffset() * 3600;  // 减去时区偏移
		return ts * 1000+ millisec;  // 返回毫秒级时间戳
	}

	/**
	 * @brief 将时间戳转换为字符串
	 * @param mytime 毫秒级时间戳
	 * @return std::string 返回格式化的时间字符串
	 * 
	 * 该函数将毫秒级时间戳转换为可读的时间字符串。
	 * 格式：YYYYMMDDHHMMSS或YYYYMMDDHHMMSS.mmm
	 * 支持跨平台使用。
	 */
	static std::string timeToString(int64_t mytime)
	{
		if (mytime == 0) return "";  // 如果时间戳为0，返回空字符串
		int64_t sec = mytime/1000;  // 提取秒级部分
		int msec = (int) (mytime - sec * 1000);  // 提取毫秒部分
		if (msec < 0) return "";  // 如果毫秒为负，返回空字符串
		time_t tt =  sec;  // 转换为time_t类型
		struct tm t;  // 声明时间结构体
#ifdef _WIN32  // Windows平台
		localtime_s(&t, &tt);  // 使用安全版本的localtime
#else  // Unix/Linux平台
		localtime_r(&tt, &t);  // 使用线程安全版本的localtime
#endif
		char tm_buf[64] = {'\0'};  // 初始化时间字符串缓冲区
		if (msec > 0) //是否有毫秒
		   sprintf(tm_buf,"%4d%02d%02d%02d%02d%02d.%03d",t.tm_year+1900, t.tm_mon+1, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec, msec);  // 格式化包含毫秒的时间
		else 
		   sprintf(tm_buf,"%4d%02d%02d%02d%02d%02d",t.tm_year+1900, t.tm_mon+1, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec);  // 格式化不包含毫秒的时间
		return tm_buf;  // 返回格式化的时间字符串
	};

	/**
	 * @brief 获取指定日期后的日期
	 * @param curDate 当前日期，格式为YYYYMMDD
	 * @param days 天数偏移，默认为1
	 * @return uint32_t 返回计算后的日期，格式为YYYYMMDD
	 * 
	 * 该函数计算指定日期后若干天的日期。
	 * 自动处理月份和年份的进位。
	 */
	static uint32_t getNextDate(uint32_t curDate, int days = 1)
	{
		tm t;	 // 声明时间结构体
		memset(&t,0,sizeof(tm));  // 清零时间结构体
		t.tm_year = curDate/10000 - 1900;  // 设置年份（从1900年开始）
		t.tm_mon = (curDate%10000)/100 - 1;  // 设置月份（从0开始）
		t.tm_mday = curDate % 100;  // 设置日期
		//t.tm_isdst 	
		time_t ts = mktime(&t);  // 转换为时间戳
		ts += days*86400;  // 添加指定的天数（86400秒/天）

		tm* newT = localtime(&ts);  // 转换为本地时间结构体
		return (newT->tm_year+1900)*10000 + (newT->tm_mon+1)*100 + newT->tm_mday;  // 返回新日期
	}

	/**
	 * @brief 获取指定时间后的时间
	 * @param curTime 当前时间，格式为HHMM
	 * @param mins 分钟偏移，默认为1
	 * @return uint32_t 返回计算后的时间，格式为HHMM
	 * 
	 * 该函数计算指定时间后若干分钟的时间。
	 * 自动处理跨日和负时间的情况。
	 */
	static uint32_t getNextMinute(int32_t curTime, int32_t mins = 1)
	{
		int32_t curHour = curTime / 100;  // 提取当前小时
		int32_t curMin = curTime % 100;   // 提取当前分钟
		int32_t totalMins = curHour * 60 + curMin;  // 转换为总分钟数
		totalMins += mins;  // 添加指定的分钟数

		if (totalMins >= 1440)  // 如果超过一天（1440分钟）
			totalMins -= 1440;  // 减去一天
		else if (totalMins < 0)  // 如果为负数
			totalMins += 1440;  // 加上一天

		int32_t ret = (totalMins / 60) * 100 + totalMins % 60;  // 转换回HHMM格式
		return (uint32_t)ret;  // 返回新时间
	}

    /**
     * @brief 获取指定月份后的月份
     * @param curMonth 当前月份，格式为YYYYMM
     * @param months 月份偏移，默认为1
     * @return uint32_t 返回计算后的月份，格式为YYYYMM
     * 
     * 该函数计算指定月份后若干个月的月份。
     * 自动处理年份进位和月份范围。
     */
    static uint32_t getNextMonth(uint32_t curMonth, int months = 1)
    {
        int32_t uYear = curMonth / 100;  // 提取年份
        int32_t uMonth = curMonth % 100; // [1, 12] 提取月份
     
		int32_t uAddYear = months / 12;  // 计算需要增加的年数
        int32_t uAddMon = months % 12;   // 计算需要增加的月数
        if (uAddMon < 0) uAddMon += 12;  // math modulus: [0, 11] 处理负数月份
     
        uYear += uAddYear;  // 增加年份
        uMonth += uAddMon;  // 增加月份
        if (uMonth > 12) {  // 如果月份超过12
            ++uYear;  // 年份加1
            uMonth -= 12;  // 月份减12
        }
        return (uint32_t) (uYear*100 + uMonth);  // 返回新月份
    }

	/**
	 * @brief 将日期和时间转换为分钟级时间条
	 * @param uDate 日期，格式为YYYYMMDD
	 * @param uTime 时间，格式为HHMM
	 * @return uint64_t 返回分钟级时间条
	 * 
	 * 该函数将日期和时间组合成分钟级时间条。
	 * 格式：前8位为日期偏移（从1990年开始），后4位为时间。
	 */
	static inline uint64_t timeToMinBar(uint32_t uDate, uint32_t uTime)
	{
		return (uint64_t)((uDate-19900000)*10000) + uTime;  // 计算分钟级时间条
	}

	/**
	 * @brief 从分钟级时间条提取日期
	 * @param minTime 分钟级时间条
	 * @return uint32_t 返回日期，格式为YYYYMMDD
	 * 
	 * 该函数从分钟级时间条中提取日期部分。
	 */
	static inline uint32_t minBarToDate(uint64_t minTime)
	{
		return (uint32_t)(minTime/10000 + 19900000);  // 提取日期部分
	}

	/**
	 * @brief 从分钟级时间条提取时间
	 * @param minTime 分钟级时间条
	 * @return uint32_t 返回时间，格式为HHMM
	 * 
	 * 该函数从分钟级时间条中提取时间部分。
	 */
	static inline uint32_t minBarToTime(uint64_t minTime)
	{
		return (uint32_t)(minTime%10000);  // 提取时间部分
	}

	/**
	 * @brief 判断指定日期是否为周末
	 * @param uDate 日期，格式为YYYYMMDD
	 * @return bool 是周末返回true，否则返回false
	 * 
	 * 该函数判断指定日期是否为周末（星期六或星期日）。
	 */
	static inline bool isWeekends(uint32_t uDate)
	{
		tm t;	 // 声明时间结构体
		memset(&t,0,sizeof(tm));  // 清零时间结构体
		t.tm_year = uDate/1/10000 - 1900;  // 设置年份（从1900年开始）
		t.tm_mon = (uDate/1%10000)/100 - 1;  // 设置月份（从0开始）
		t.tm_mday = uDate/1 % 100;  // 设置日期

		time_t tt = mktime(&t);  // 转换为时间戳
		tm* tmt = localtime(&tt);  // 转换为本地时间结构体
		if(tmt == NULL)  // 如果转换失败
			return true;  // 返回true（保守处理）
	
		if(tmt->tm_wday == 0 || tmt->tm_wday==6)  // 如果是星期日(0)或星期六(6)
			return true;  // 返回true

		return false;  // 返回false
	}

public:
	/**
	 * @class Time32
	 * @brief 32位时间类
	 * 
	 * 该类提供了32位时间表示，支持毫秒级精度。
	 * 提供多种时间格式化和转换功能。
	 */
	class Time32
	{
	public:
		/**
		 * @brief 默认构造函数
		 * 
		 * 初始化Time32对象，毫秒部分设为0。
		 */
		Time32():_msec(0){}  // 初始化毫秒为0

		/**
		 * @brief 构造函数，从time_t和毫秒构造
		 * @param _time 秒级时间戳
		 * @param msecs 毫秒部分，默认为0
		 * 
		 * 该构造函数从time_t时间戳和毫秒构造Time32对象。
		 */
		Time32(time_t _time, uint32_t msecs = 0)
		{
#ifdef _WIN32  // Windows平台
			localtime_s(&t, &_time);  // 使用安全版本的localtime
#else  // Unix/Linux平台
			localtime_r(&_time, &t);  // 使用线程安全版本的localtime
#endif
			_msec = msecs;  // 设置毫秒部分
		}

		/**
		 * @brief 构造函数，从毫秒级时间戳构造
		 * @param _time 毫秒级时间戳
		 * 
		 * 该构造函数从毫秒级时间戳构造Time32对象。
		 * 自动分离秒级部分和毫秒部分。
		 */
		Time32(uint64_t _time)
		{
			time_t _t = _time/1000;  // 提取秒级部分
			_msec = (uint32_t)_time%1000;  // 提取毫秒部分
#ifdef _WIN32  // Windows平台
			localtime_s(&t, &_t);  // 使用安全版本的localtime
#else  // Unix/Linux平台
			localtime_r(&_t, &t);  // 使用线程安全版本的localtime
#endif
		}

		/**
		 * @brief 从毫秒级时间戳设置时间
		 * @param _time 毫秒级时间戳
		 * 
		 * 该函数从毫秒级时间戳设置Time32对象的时间。
		 */
		void from_local_time(uint64_t _time)
		{
			time_t _t = _time/1000;  // 提取秒级部分
			_msec = (uint32_t)(_time%1000);  // 提取毫秒部分
#ifdef _WIN32  // Windows平台
			localtime_s(&t, &_t);  // 使用安全版本的localtime
#else  // Unix/Linux平台
			localtime_r(&_t, &t);  // 使用线程安全版本的localtime
#endif
		}

		/**
		 * @brief 获取日期部分
		 * @return uint32_t 返回日期，格式为YYYYMMDD
		 * 
		 * 该函数返回Time32对象表示的日期。
		 */
		uint32_t date()
		{
			return (t.tm_year + 1900)*10000 + (t.tm_mon + 1)*100 + t.tm_mday;  // 计算日期部分
		}

		/**
		 * @brief 获取时间部分
		 * @return uint32_t 返回时间，格式为HHMMSS
		 * 
		 * 该函数返回Time32对象表示的时间（不含毫秒）。
		 */
		uint32_t time()
		{
			return t.tm_hour*10000 + t.tm_min*100 + t.tm_sec;  // 计算时间部分
		}

		/**
		 * @brief 获取带毫秒的时间
		 * @return uint32_t 返回时间，格式为HHMMSSmmm
		 * 
		 * 该函数返回Time32对象表示的完整时间（包含毫秒）。
		 */
		uint32_t time_ms()
		{
			return t.tm_hour*10000000 + t.tm_min*100000 + t.tm_sec*1000 + _msec;  // 计算带毫秒的时间
		}

		/**
		 * @brief 格式化时间字符串
		 * @param sfmt 格式字符串，默认为"%Y.%m.%d %H:%M:%S"
		 * @param hasMilliSec 是否包含毫秒，默认为false
		 * @return const char* 返回格式化的时间字符串
		 * 
		 * 该函数将Time32对象格式化为字符串。
		 * 支持自定义格式，可选择是否包含毫秒。
		 */
		const char* fmt(const char* sfmt = "%Y.%m.%d %H:%M:%S", bool hasMilliSec = false) const
		{
			static char buff[1024];  // 静态字符串缓冲区
			uint32_t length = (uint32_t)strftime(buff, 1023, sfmt, &t);  // 格式化时间字符串
			if (hasMilliSec)  // 如果需要包含毫秒
				sprintf(buff + length, ",%03u", _msec);  // 在末尾添加毫秒部分
			return buff;  // 返回格式化的字符串
		}

	protected:
		struct tm t;  // 时间结构体，存储年月日时分秒信息
		uint32_t _msec;  // 毫秒部分
	};

	/**
	 * @class Ticker
	 * @brief 高性能计时器类
	 * 
	 * 该类提供了高精度的计时功能，使用现代C++的chrono库实现。
	 * 支持秒、毫秒、微秒、纳秒级别的计时。
	 */
	class Ticker
	{
	public:
		/**
		 * @brief 默认构造函数
		 * 
		 * 构造Ticker对象并记录当前时间点。
		 */
		Ticker()
		{
			_tick = std::chrono::high_resolution_clock::now();  // 记录当前高精度时间点
		}

		/**
		 * @brief 重置计时器
		 * 
		 * 重置计时器，重新记录当前时间点。
		 */
		void reset()
		{
			_tick = std::chrono::high_resolution_clock::now();  // 重新记录当前高精度时间点
		}

		/**
		 * @brief 获取经过的秒数
		 * @return int64_t 返回从计时器启动到现在经过的秒数
		 * 
		 * 该函数计算从计时器启动到现在经过的秒数。
		 */
		inline int64_t seconds() const 
		{
			auto now = std::chrono::high_resolution_clock::now();  // 获取当前时间点
			auto td = now - _tick;  // 计算时间差
			return std::chrono::duration_cast<std::chrono::seconds>(td).count();  // 转换为秒数
		}

		/**
		 * @brief 获取经过的毫秒数
		 * @return int64_t 返回从计时器启动到现在经过的毫秒数
		 * 
		 * 该函数计算从计时器启动到现在经过的毫秒数。
		 */
		inline int64_t milli_seconds() const
		{
			auto now = std::chrono::high_resolution_clock::now();  // 获取当前时间点
			auto td = now - _tick;  // 计算时间差
			return std::chrono::duration_cast<std::chrono::milliseconds>(td).count();  // 转换为毫秒数
		}

		/**
		 * @brief 获取经过的微秒数
		 * @return int64_t 返回从计时器启动到现在经过的微秒数
		 * 
		 * 该函数计算从计时器启动到现在经过的微秒数。
		 */
		inline int64_t micro_seconds() const
		{
			auto now = std::chrono::high_resolution_clock::now();  // 获取当前时间点
			auto td = now - _tick;  // 计算时间差
			return std::chrono::duration_cast<std::chrono::microseconds>(td).count();  // 转换为微秒数
		}

		/**
		 * @brief 获取经过的纳秒数
		 * @return int64_t 返回从计时器启动到现在经过的纳秒数
		 * 
		 * 该函数计算从计时器启动到现在经过的纳秒数。
		 */
		inline int64_t nano_seconds() const
		{
			auto now = std::chrono::high_resolution_clock::now();  // 获取当前时间点
			auto td = now - _tick;  // 计算时间差
			return std::chrono::duration_cast<std::chrono::nanoseconds>(td).count();  // 转换为纳秒数
		}

	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> _tick;  // 高精度时钟时间点
	};
};
