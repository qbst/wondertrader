/*!
 * \file HisDataReplayer.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 历史数据回放器头文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了HisDataReplayer类，用于历史数据的回放和模拟。
 *
 * 主要功能：
 * 1. 历史数据加载：从文件或外部加载器加载K线、Tick、订单队列、订单明细、逐笔成交等数据
 * 2. 数据缓存管理：维护各种数据类型的缓存，支持按需加载和自动清理
 * 3. 回放模式：支持按K线、按Tick、按定时任务三种回放模式
 * 4. 数据订阅：支持策略订阅各种数据类型
 * 5. 数据查询：提供K线切片、Tick切片等数据查询接口
 * 6. 复权处理：支持股票复权数据的处理
 * 7. 手续费计算：提供手续费计算功能
 *
 * 设计特点：
 * - 采用游标机制跟踪数据回放进度
 * - 支持多种数据源（二进制文件、CSV文件、外部加载器）
 * - 智能缓存管理，自动清理未使用的数据
 * - 支持期货主力合约数据的整合
 * - 支持股票复权数据的处理
 */
#pragma once
#include <string>                                                      // 字符串类型
#include <set>                                                         // 集合容器
#include "HisDataMgr.h"                                                // 历史数据管理器
#include "../WtDataStorage/DataDefine.h"                             // 数据定义

#include "../Includes/FasterDefs.h"                                  // 快速定义
#include "../Includes/WTSMarcos.h"                                    // WonderTrader宏定义
#include "../Includes/WTSTypes.h"                                     // WonderTrader类型定义

#include "../WTSTools/WTSHotMgr.h"                                   // 主力合约管理器
#include "../WTSTools/WTSBaseDataMgr.h"                              // 基础数据管理器

NS_WTP_BEGIN                                                          // WonderTrader命名空间开始
class WTSTickData;                                                    // Tick数据前向声明
class WTSVariant;                                                     // 变体类型前向声明
class WTSKlineSlice;                                                  // K线切片前向声明
class WTSTickSlice;                                                   // Tick切片前向声明
class WTSOrdDtlSlice;                                                 // 订单明细切片前向声明
class WTSOrdQueSlice;                                                 // 订单队列切片前向声明
class WTSTransSlice;                                                  // 逐笔成交切片前向声明
class WTSSessionInfo;                                                 // 交易时段信息前向声明
class WTSCommodityInfo;                                               // 合约信息前向声明

class WTSOrdDtlData;                                                  // 订单明细数据前向声明
class WTSOrdQueData;                                                  // 订单队列数据前向声明
class WTSTransData;                                                   // 逐笔成交数据前向声明

class EventNotifier;                                                  // 事件通知器前向声明
NS_WTP_END                                                            // WonderTrader命名空间结束

USING_NS_WTP;                                                         // 使用WonderTrader命名空间

/**
 * @brief 数据接收器接口
 * 
 * 用于接收历史数据回放器推送的各种市场数据
 */
class IDataSink
{
public:
	/**
	 * @brief 处理Tick数据回调
	 * 
	 * @param stdCode 合约代码
	 * @param curTick 当前Tick数据
	 * @param pxType 价格类型
	 */
	virtual void	handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType) = 0;  // 处理Tick数据（纯虚函数）

	/**
	 * @brief 处理订单队列数据回调
	 * 
	 * @param stdCode 合约代码
	 * @param curOrdQue 当前订单队列数据
	 */
	virtual void	handle_order_queue(const char* stdCode, WTSOrdQueData* curOrdQue) {};  // 处理订单队列数据（默认空实现）

	/**
	 * @brief 处理订单明细数据回调
	 * 
	 * @param stdCode 合约代码
	 * @param curOrdDtl 当前订单明细数据
	 */
	virtual void	handle_order_detail(const char* stdCode, WTSOrdDtlData* curOrdDtl) {};  // 处理订单明细数据（默认空实现）

	/**
	 * @brief 处理逐笔成交数据回调
	 * 
	 * @param stdCode 合约代码
	 * @param curTrans 当前逐笔成交数据
	 */
	virtual void	handle_transaction(const char* stdCode, WTSTransData* curTrans) {};  // 处理逐笔成交数据（默认空实现）

	/**
	 * @brief 处理K线收盘回调
	 * 
	 * @param stdCode 合约代码
	 * @param period 周期
	 * @param times 倍数
	 * @param newBar 新的K线数据
	 */
	virtual void	handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) = 0;  // 处理K线收盘（纯虚函数）

	/**
	 * @brief 处理定时调度回调
	 * 
	 * @param uDate 日期
	 * @param uTime 时间
	 */
	virtual void	handle_schedule(uint32_t uDate, uint32_t uTime) = 0;  // 处理定时调度（纯虚函数）

	/**
	 * @brief 处理初始化回调
	 */
	virtual void	handle_init() = 0;                                 // 处理初始化（纯虚函数）

	/**
	 * @brief 处理交易时段开始回调
	 * 
	 * @param curTDate 当前交易日期
	 */
	virtual void	handle_session_begin(uint32_t curTDate) = 0;       // 处理交易时段开始（纯虚函数）

	/**
	 * @brief 处理交易时段结束回调
	 * 
	 * @param curTDate 当前交易日期
	 */
	virtual void	handle_session_end(uint32_t curTDate) = 0;         // 处理交易时段结束（纯虚函数）

	/**
	 * @brief 处理回放完成回调
	 */
	virtual void	handle_replay_done() {}                            // 处理回放完成（默认空实现）

	/**
	 * @brief 处理小节结束回调
	 * 
	 * @param curTDate 当前交易日期
	 * @param curTime 当前时间
	 */
	virtual void	handle_section_end(uint32_t curTDate, uint32_t curTime) {}  // 处理小节结束（默认空实现）
};

/**
 * @brief 历史数据加载器的K线数据回调函数类型定义
 * 
 * @param obj 回传用的，原样返回即可
 * @param firstBar K线数据数组首地址
 * @param count K线条数
 */
typedef void(*FuncReadBars)(void* obj, WTSBarStruct* firstBar, uint32_t count);  // K线数据回调函数类型

/**
 * @brief 加载复权因子回调函数类型定义
 * 
 * @param obj 回传用的，原样返回即可
 * @param stdCode 合约代码
 * @param dates 日期数组
 * @param factors 复权因子数组
 * @param count 数量
 */
typedef void(*FuncReadFactors)(void* obj, const char* stdCode, uint32_t* dates, double* factors, uint32_t count);  // 复权因子回调函数类型

/**
 * @brief 加载Tick数据回调函数类型定义
 * 
 * @param obj 回传用的，原样返回即可
 * @param firstItem Tick数据数组首地址
 * @param count 条数
 */
typedef void(*FuncReadTicks)(void* obj, WTSTickStruct* firstItem, uint32_t count);  // Tick数据回调函数类型

/**
 * @brief 加载委托明细数据回调函数类型定义
 * 
 * @param obj 回传用的，原样返回即可
 * @param firstItem 委托明细数据数组首地址
 * @param count 条数
 */
typedef void(*FuncReadOrdDtl)(void* obj, WTSOrdDtlStruct* firstItem, uint32_t count);  // 委托明细数据回调函数类型

/**
 * @brief 加载委托队列数据回调函数类型定义
 * 
 * @param obj 回传用的，原样返回即可
 * @param firstItem 委托队列数据数组首地址
 * @param count 条数
 */
typedef void(*FuncReadOrdQue)(void* obj, WTSOrdQueStruct* firstItem, uint32_t count);  // 委托队列数据回调函数类型

/**
 * @brief 加载逐笔成交数据回调函数类型定义
 * 
 * @param obj 回传用的，原样返回即可
 * @param firstItem 逐笔成交数据数组首地址
 * @param count 条数
 */
typedef void(*FuncReadTrans)(void* obj, WTSTransStruct* firstItem, uint32_t count);  // 逐笔成交数据回调函数类型

/**
 * @brief 回测数据加载器接口
 * 
 * 用于从外部数据源加载历史数据
 */
class IBtDataLoader
{
public:
	/**
	 * @brief 加载最终历史K线数据
	 * 
	 * 和loadRawHisBars的区别在于，loadFinalHisBars系统认为是最终所需数据，不再进行加工，
	 * 例如复权数据、主力合约数据等。loadRawHisBars是加载未加工的原始数据的接口。
	 *
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param cb 回调函数
	 * @return 是否加载成功
	 */
	virtual bool loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) = 0;  // 加载最终历史K线数据

	/**
	 * @brief 加载原始历史K线数据
	 *
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param cb 回调函数
	 * @return 是否加载成功
	 */
	virtual bool loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) = 0;  // 加载原始历史K线数据

	/**
	 * @brief 加载全部除权因子
	 * 
	 * @param obj 回传用的，原样返回即可
	 * @param cb 回调函数
	 * @return 是否加载成功
	 */
	virtual bool loadAllAdjFactors(void* obj, FuncReadFactors cb) = 0;  // 加载全部除权因子

	/**
	 * @brief 根据合约加载除权因子
	 *
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param cb 回调函数
	 * @return 是否加载成功
	 */
	virtual bool loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb) = 0;  // 根据合约加载除权因子

	/**
	 * @brief 加载历史Tick数据
	 * 
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @param cb 回调函数
	 * @return 是否加载成功
	 */
	virtual bool loadRawHisTicks(void* obj, const char* stdCode, uint32_t uDate, FuncReadTicks cb) = 0;  // 加载历史Tick数据

	/**
	 * @brief 是否自动转储为dsb格式
	 * 
	 * @return 是否自动转储
	 */
	virtual bool isAutoTrans() { return true; }                      // 是否自动转储为dsb（默认返回true）
};

/**
 * @brief 历史数据回放器类
 * 
 * 负责历史数据的加载、缓存和回放，支持多种回放模式
 */
class HisDataReplayer
{

private:
	/**
	 * @brief HFT数据列表模板类
	 * 
	 * 用于存储和管理高频交易数据（Tick、订单队列、订单明细、逐笔成交等）
	 * 
	 * @tparam T 数据类型（WTSTickStruct、WTSOrdDtlStruct等）
	 */
	template <typename T>
	class HftDataList
	{
	public:
		std::string		_code;                                          // 合约代码
		uint32_t		_date;                                          // 日期
		/*
		 * By Wesley @ 2022.03.21
		 * 游标，用于标记下一条数据的位置，或者说已经回放过的条数
		 * 未初始化时，游标为UINT_MAX，一旦初始化，游标必然是大于0的
		 */
		std::size_t		_cursor;                                        // 游标（已回放的数据条数）
		std::size_t		_count;                                         // 总数据条数

		std::vector<T> _items;                                         // 数据项列表

		/**
		 * @brief 构造函数
		 */
		HftDataList() :_cursor(UINT_MAX), _count(0), _date(0){}      // 初始化游标为UINT_MAX，表示未初始化
	};

	/**
	 * @brief Tick缓存类型定义
	 */
	typedef wt_hashmap<std::string, HftDataList<WTSTickStruct>>		TickCache;  // Tick缓存（合约代码 -> Tick数据列表）

	/**
	 * @brief 订单明细缓存类型定义
	 */
	typedef wt_hashmap<std::string, HftDataList<WTSOrdDtlStruct>>	OrdDtlCache;  // 订单明细缓存（合约代码 -> 订单明细数据列表）

	/**
	 * @brief 订单队列缓存类型定义
	 */
	typedef wt_hashmap<std::string, HftDataList<WTSOrdQueStruct>>	OrdQueCache;  // 订单队列缓存（合约代码 -> 订单队列数据列表）

	/**
	 * @brief 逐笔成交缓存类型定义
	 */
	typedef wt_hashmap<std::string, HftDataList<WTSTransStruct>>	TransCache;  // 逐笔成交缓存（合约代码 -> 逐笔成交数据列表）


	/**
	 * @brief K线列表结构体
	 */
	typedef struct _BarsList
	{
		std::string		_code;                                          // 合约代码
		WTSKlinePeriod	_period;                                        // K线周期
		/*
		 * By Wesley @ 2022.03.21
		 * 游标，用于标记下一条数据的位置，或者说已经回放过的条数
		 * 未初始化时，游标为UINT_MAX，一旦初始化，游标必然是大于0的
		 */
		uint32_t		_cursor;                                        // 游标（已回放的K线条数）
		uint32_t		_count;                                         // 总K线条数
		uint32_t		_times;                                         // 倍数（如m5表示5分钟）

		std::vector<WTSBarStruct>	_bars;                              // K线数据列表
		double			_factor;                                        // 最后一条复权因子

		uint32_t		_untouch_days;                                  // 未用到的天数（用于缓存清理）

		/**
		 * @brief 标记为已使用
		 * 
		 * 将未使用天数重置为0，表示该K线列表最近被使用
		 */
		inline void mark()
		{
			_untouch_days = 0;                                         // 重置未使用天数
		}

		/**
		 * @brief 获取K线列表占用的内存大小
		 * 
		 * @return 内存大小（字节）
		 */
		inline std::size_t size()
		{
			return sizeof(WTSBarStruct)*_bars.size();                 // 返回K线数据占用的内存大小
		}

		/**
		 * @brief 构造函数
		 */
		_BarsList() :_cursor(UINT_MAX), _count(0), _times(1), _factor(1), _untouch_days(0){}  // 初始化所有成员变量
	} BarsList;                                                       // K线列表结构体

	/*
	 *	By Wesley @ 2022.03.13
	 *	这里把缓存改成智能指针
	 *	因为有用户发现如果在oncalc的时候获取未在oninit中订阅的K线的时候
	 *	因为使用BarList的引用，当K线缓存的map重新插入新的K线以后
	 *	引用的地方失效了，会引用到错误地址
	 *	我怀疑这里有可能是重新拷贝了一下数据
	 *	这里改成智能指针就能避免这个问题，因为不管map自己的内存如何组织
	 *	智能指针指向的地址都是不会变的
	 */
	/**
	 * @brief K线列表智能指针类型定义
	 */
	typedef std::shared_ptr<BarsList> BarsListPtr;                   // K线列表智能指针

	/**
	 * @brief K线缓存类型定义
	 */
	typedef wt_hashmap<std::string, BarsListPtr>	BarsCache;       // K线缓存（合约代码周期 -> K线列表指针）

	/**
	 * @brief 任务周期类型枚举
	 */
	typedef enum tagTaskPeriodType
	{
		TPT_None,		                                                // 不重复
		TPT_Minute = 4,	                                                // 分钟线周期
		TPT_Daily = 8,	                                                // 每个交易日
		TPT_Weekly,		                                                // 每周，遇到节假日的话要顺延
		TPT_Monthly,	                                                // 每月，遇到节假日顺延
		TPT_Yearly		                                                // 每年，遇到节假日顺延
	}TaskPeriodType;                                                  // 任务周期类型

	/**
	 * @brief 定时任务信息结构体
	 */
	typedef struct _TaskInfo
	{
		uint32_t	_id;                                                // 任务ID
		char		_name[16];		                                    // 任务名
		char		_trdtpl[16];	                                    // 交易日模板
		char		_session[16];	                                    // 交易时间模板
		uint32_t	_day;		                                        // 日期，根据周期变化：每日为0，每周为0~6（对应周日到周六），每月为1~31，每年为0101~1231
		uint32_t	_time;		                                        // 时间，精确到分钟
		bool		_strict_time;	                                    // 是否是严格时间：严格时间即只有时间相等才会执行，不是严格时间，则大于等于触发时间都会执行

		uint64_t	_last_exe_time;	                                    // 上次执行时间，主要为了防止重复执行

		TaskPeriodType	_period;                                        // 任务周期
	} TaskInfo;                                                       // 任务信息结构体

	/**
	 * @brief 任务信息智能指针类型定义
	 */
	typedef std::shared_ptr<TaskInfo> TaskInfoPtr;                    // 任务信息智能指针



public:
	/**
	 * @brief 构造函数
	 */
	HisDataReplayer();                                                // 构造函数

	/**
	 * @brief 析构函数
	 */
	~HisDataReplayer();                                               // 析构函数

private:
	/**
	 * @brief 从自定义数据文件缓存历史K线数据
	 * 
	 * @param key 缓存键（合约代码周期）
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param bForBars 是否用于K线回放（默认true）
	 * @return 是否缓存成功
	 */
	bool		cacheRawBarsFromBin(const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bForBars = true);  // 从二进制文件缓存K线数据

	/**
	 * @brief 从CSV文件缓存历史K线数据
	 * 
	 * @param key 缓存键（合约代码周期）
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param bSubbed 是否已订阅（默认true）
	 * @return 是否缓存成功
	 */
	bool		cacheRawBarsFromCSV(const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed = true);  // 从CSV文件缓存K线数据

	/**
	 * @brief 从自定义数据文件缓存历史Tick数据
	 * 
	 * @param key 缓存键（合约代码日期）
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否缓存成功
	 */
	bool		cacheRawTicksFromBin(const std::string& key, const char* stdCode, uint32_t uDate);  // 从二进制文件缓存Tick数据

	/**
	 * @brief 从自定义数据文件缓存历史委托明细数据
	 * 
	 * @param key 缓存键（合约代码日期）
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否缓存成功
	 */
	bool		cacheRawOrdDtlFromBin(const std::string& key, const char* stdCode, uint32_t uDate);  // 从二进制文件缓存委托明细数据

	/**
	 * @brief 从自定义数据文件缓存历史订单队列数据
	 * 
	 * @param key 缓存键（合约代码日期）
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否缓存成功
	 */
	bool		cacheRawOrdQueFromBin(const std::string& key, const char* stdCode, uint32_t uDate);  // 从二进制文件缓存订单队列数据

	/**
	 * @brief 从自定义数据文件缓存历史逐笔成交数据
	 * 
	 * @param key 缓存键（合约代码日期）
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否缓存成功
	 */
	bool		cacheRawTransFromBin(const std::string& key, const char* stdCode, uint32_t uDate);  // 从二进制文件缓存逐笔成交数据

	/**
	 * @brief 从CSV文件缓存历史Tick数据
	 * 
	 * @param key 缓存键（合约代码日期）
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否缓存成功
	 */
	bool		cacheRawTicksFromCSV(const std::string& key, const char* stdCode, uint32_t uDate);  // 从CSV文件缓存Tick数据

	/**
	 * @brief 从外部加载器缓存最终历史K线数据
	 * 
	 * @param key 缓存键（合约代码周期）
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param bSubbed 是否已订阅（默认true）
	 * @return 是否缓存成功
	 */
	bool		cacheFinalBarsFromLoader(const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed = true);  // 从外部加载器缓存最终K线数据

	/**
	 * @brief 从外部加载器缓存历史Tick数据
	 * 
	 * @param key 缓存键（合约代码日期）
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否缓存成功
	 */
	bool		cacheRawTicksFromLoader(const std::string& key, const char* stdCode, uint32_t uDate);  // 从外部加载器缓存Tick数据

	/**
	 * @brief 缓存整合的期货合约历史K线（针对.HOT//2ND等主力合约）
	 * 
	 * @param codeInfo 合约信息指针
	 * @param key 缓存键（合约代码周期）
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param bSubbed 是否已订阅（默认true）
	 * @return 是否缓存成功
	 */
	bool		cacheIntegratedFutBarsFromBin(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed = true);  // 缓存整合的期货主力合约K线数据

	/**
	 * @brief 缓存复权股票K线数据
	 * 
	 * @param codeInfo 合约信息指针
	 * @param key 缓存键（合约代码周期）
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param bSubbed 是否已订阅（默认true）
	 * @return 是否缓存成功
	 */
	bool		cacheAdjustedStkBarsFromBin(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed = true);  // 缓存复权股票K线数据

	/**
	 * @brief 处理分钟线结束
	 * 
	 * @param uDate 日期
	 * @param uTime 时间
	 * @param endTDate 结束交易日期（默认0）
	 * @param tickSimulated 是否模拟Tick（默认true）
	 */
	void		onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate = 0, bool tickSimulated = true);  // 处理分钟线结束

	/**
	 * @brief 加载手续费配置
	 * 
	 * @param filename 手续费配置文件路径
	 */
	void		loadFees(const char* filename);                        // 加载手续费配置

	/**
	 * @brief 回放HFT数据
	 * 
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @return 是否回放成功
	 */
	bool		replayHftDatas(uint64_t stime, uint64_t etime);       // 回放HFT数据

	/**
	 * @brief 按天回放HFT数据
	 * 
	 * @param curTDate 当前交易日期
	 * @return 最后处理的时间戳
	 */
	uint64_t	replayHftDatasByDay(uint32_t curTDate);                // 按天回放HFT数据

	/**
	 * @brief 使用未订阅的K线模拟Tick数据
	 * 
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @param endTDate 结束交易日期（默认0）
	 * @param pxType 价格类型（默认0）
	 */
	void		simTickWithUnsubBars(uint64_t stime, uint64_t etime, uint32_t endTDate = 0, int pxType = 0);  // 使用未订阅K线模拟Tick

	/**
	 * @brief 模拟Tick数据
	 * 
	 * @param uDate 日期
	 * @param uTime 时间
	 * @param endTDate 结束交易日期（默认0）
	 * @param pxType 价格类型（默认0）
	 */
	void		simTicks(uint32_t uDate, uint32_t uTime, uint32_t endTDate = 0, int pxType = 0);  // 模拟Tick数据

	/**
	 * @brief 检查是否已缓存Tick数据
	 * 
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否已缓存
	 */
	inline bool		checkTicks(const char* stdCode, uint32_t uDate);  // 检查Tick数据缓存

	/**
	 * @brief 检查是否已缓存订单明细数据
	 * 
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否已缓存
	 */
	inline bool		checkOrderDetails(const char* stdCode, uint32_t uDate);  // 检查订单明细数据缓存

	/**
	 * @brief 检查是否已缓存订单队列数据
	 * 
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否已缓存
	 */
	inline bool		checkOrderQueues(const char* stdCode, uint32_t uDate);  // 检查订单队列数据缓存

	/**
	 * @brief 检查是否已缓存逐笔成交数据
	 * 
	 * @param stdCode 合约代码
	 * @param uDate 日期
	 * @return 是否已缓存
	 */
	inline bool		checkTransactions(const char* stdCode, uint32_t uDate);  // 检查逐笔成交数据缓存

	/**
	 * @brief 检查未订阅的K线缓存
	 */
	void		checkUnbars();                                         // 检查未订阅K线缓存

	/**
	 * @brief 从文件加载股票复权因子
	 * 
	 * @param adjfile 复权因子文件路径
	 * @return 是否加载成功
	 */
	bool		loadStkAdjFactorsFromFile(const char* adjfile);       // 从文件加载股票复权因子

	/**
	 * @brief 从外部加载器加载股票复权因子
	 * 
	 * @return 是否加载成功
	 */
	bool		loadStkAdjFactorsFromLoader();                        // 从外部加载器加载股票复权因子

	/**
	 * @brief 检查所有Tick数据是否已缓存
	 * 
	 * @param uDate 日期
	 * @return 是否全部已缓存
	 */
	bool		checkAllTicks(uint32_t uDate);                         // 检查所有Tick数据缓存

	/**
	 * @brief 获取下一个Tick时间
	 * 
	 * @param curTDate 当前交易日期
	 * @param stime 开始时间（默认UINT64_MAX）
	 * @return 下一个Tick时间戳
	 */
	inline	uint64_t	getNextTickTime(uint32_t curTDate, uint64_t stime = UINT64_MAX);  // 获取下一个Tick时间

	/**
	 * @brief 获取下一个订单队列时间
	 * 
	 * @param curTDate 当前交易日期
	 * @param stime 开始时间（默认UINT64_MAX）
	 * @return 下一个订单队列时间戳
	 */
	inline	uint64_t	getNextOrdQueTime(uint32_t curTDate, uint64_t stime = UINT64_MAX);  // 获取下一个订单队列时间

	/**
	 * @brief 获取下一个订单明细时间
	 * 
	 * @param curTDate 当前交易日期
	 * @param stime 开始时间（默认UINT64_MAX）
	 * @return 下一个订单明细时间戳
	 */
	inline	uint64_t	getNextOrdDtlTime(uint32_t curTDate, uint64_t stime = UINT64_MAX);  // 获取下一个订单明细时间

	/**
	 * @brief 获取下一个逐笔成交时间
	 * 
	 * @param curTDate 当前交易日期
	 * @param stime 开始时间（默认UINT64_MAX）
	 * @return 下一个逐笔成交时间戳
	 */
	inline	uint64_t	getNextTransTime(uint32_t curTDate, uint64_t stime = UINT64_MAX);  // 获取下一个逐笔成交时间

	/**
	 * @brief 重置回放器状态
	 */
	void		reset();                                               // 重置回放器状态


	/**
	 * @brief 导出回测状态到文件
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param times 倍数
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @param progress 进度（0-1）
	 * @param elapse 已用时间（毫秒）
	 */
	void		dump_btstate(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint64_t stime, uint64_t etime, double progress, int64_t elapse);  // 导出回测状态

	/**
	 * @brief 通知回测状态
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param times 倍数
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @param progress 进度（0-1）
	 */
	void		notify_state(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint64_t stime, uint64_t etime, double progress);  // 通知回测状态

	/**
	 * @brief 定位K线索引
	 * 
	 * @param key 缓存键
	 * @param curTime 当前时间
	 * @param bUpperBound 是否使用上界（默认false）
	 * @return K线索引
	 */
	uint32_t	locate_barindex(const std::string& key, uint64_t curTime, bool bUpperBound = false);  // 定位K线索引

	/**
	 * @brief 按照K线进行回测
	 *
	 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
	 */
	void	run_by_bars(bool bNeedDump = false);                      // 按K线回测

	/**
	 * @brief 按照定时任务进行回测
	 *
	 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
	 */
	void	run_by_tasks(bool bNeedDump = false);                     // 按定时任务回测

	/**
	 * @brief 按照Tick进行回测
	 *
	 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
	 */
	void	run_by_ticks(bool bNeedDump = false);                     // 按Tick回测

	/**
	 * @brief 检查缓存天数并清理过期缓存
	 */
	void	check_cache_days();                                       // 检查缓存天数

public:
	/**
	 * @brief 初始化历史数据回放器
	 * 
	 * @param cfg 配置信息
	 * @param notifier 事件通知器（可选，默认NULL）
	 * @param dataLoader 数据加载器（可选，默认NULL）
	 * @return 是否初始化成功
	 */
	bool init(WTSVariant* cfg, EventNotifier* notifier = NULL, IBtDataLoader* dataLoader = NULL);  // 初始化回放器

	/**
	 * @brief 准备回放（加载数据缓存等）
	 * 
	 * @return 是否准备成功
	 */
	bool prepare();                                                   // 准备回放

	/**
	 * @brief 运行回测
	 *
	 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
	 */
	void run(bool bNeedDump = false);                                 // 运行回测
	
	/**
	 * @brief 停止回测
	 */
	void stop();                                                      // 停止回测

	/**
	 * @brief 清空所有缓存
	 */
	void clear_cache();                                               // 清空缓存

	/**
	 * @brief 设置回放时间范围
	 * 
	 * @param stime 开始时间
	 * @param etime 结束时间
	 */
	inline void set_time_range(uint64_t stime, uint64_t etime)       // 设置时间范围
	{
		_begin_time = stime;                                          // 设置开始时间
		_end_time = etime;                                            // 设置结束时间
	}

	/**
	 * @brief 启用/禁用Tick回放
	 * 
	 * @param bEnabled 是否启用（默认true）
	 */
	inline void enable_tick(bool bEnabled = true)                    // 启用/禁用Tick回放
	{
		_tick_enabled = bEnabled;                                     // 设置Tick回放标志
	}

	/**
	 * @brief 注册数据接收器
	 * 
	 * @param listener 数据接收器指针
	 * @param sinkName 接收器名称
	 */
	inline void register_sink(IDataSink* listener, const char* sinkName)   // 注册数据接收器
	{
		_listener = listener;                                         // 设置数据接收器
		_stra_name = sinkName;                                        // 设置接收器名称
	}

	/**
	 * @brief 注册定时任务
	 * 
	 * @param taskid 任务ID
	 * @param date 日期，根据周期变化：每日为0，每周为0~6（对应周日到周六），每月为1~31，每年为0101~1231
	 * @param time 时间，精确到分钟
	 * @param period 时间周期，可以是分钟、天、周、月、年
	 * @param trdtpl 交易日模板（默认"CHINA"）
	 * @param session 交易时间模板（默认"TRADING"）
	 */
	void register_task(uint32_t taskid, uint32_t date, uint32_t time, const char* period, const char* trdtpl = "CHINA", const char* session = "TRADING");  // 注册定时任务

	/**
	 * @brief 获取K线切片
	 * 
	 * @param stdCode 合约代码
	 * @param period 周期（如"m1"、"d1"）
	 * @param count 数量
	 * @param times 倍数（默认1）
	 * @param isMain 是否主周期（默认false）
	 * @return K线切片指针
	 */
	WTSKlineSlice* get_kline_slice(const char* stdCode, const char* period, uint32_t count, uint32_t times = 1, bool isMain = false);  // 获取K线切片

	/**
	 * @brief 获取Tick切片
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @param etime 结束时间（默认0，表示不限制）
	 * @return Tick切片指针
	 */
	WTSTickSlice* get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime = 0);  // 获取Tick切片

	/**
	 * @brief 获取订单明细切片
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @param etime 结束时间（默认0，表示不限制）
	 * @return 订单明细切片指针
	 */
	WTSOrdDtlSlice* get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime = 0);  // 获取订单明细切片

	/**
	 * @brief 获取订单队列切片
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @param etime 结束时间（默认0，表示不限制）
	 * @return 订单队列切片指针
	 */
	WTSOrdQueSlice* get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime = 0);  // 获取订单队列切片

	/**
	 * @brief 获取逐笔成交切片
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @param etime 结束时间（默认0，表示不限制）
	 * @return 逐笔成交切片指针
	 */
	WTSTransSlice* get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime = 0);  // 获取逐笔成交切片

	/**
	 * @brief 获取最新Tick数据
	 * 
	 * @param stdCode 合约代码
	 * @return 最新Tick数据指针
	 */
	WTSTickData* get_last_tick(const char* stdCode);                 // 获取最新Tick数据

	/**
	 * @brief 获取当前日期
	 * 
	 * @return 当前日期（YYYYMMDD格式）
	 */
	uint32_t get_date() const{ return _cur_date; }                    // 获取当前日期

	/**
	 * @brief 获取当前分钟时间
	 * 
	 * @return 当前分钟时间（HHMM格式）
	 */
	uint32_t get_min_time() const{ return _cur_time; }                // 获取当前分钟时间

	/**
	 * @brief 获取当前原始时间
	 * 
	 * @return 当前原始时间（HHMMSS格式）
	 */
	uint32_t get_raw_time() const{ return _cur_time; }                // 获取当前原始时间

	/**
	 * @brief 获取当前秒数
	 * 
	 * @return 当前秒数（0-86399）
	 */
	uint32_t get_secs() const{ return _cur_secs; }                    // 获取当前秒数

	/**
	 * @brief 获取当前交易日期
	 * 
	 * @return 当前交易日期（YYYYMMDD格式）
	 */
	uint32_t get_trading_date() const{ return _cur_tdate; }          // 获取当前交易日期

	/**
	 * @brief 计算手续费
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param offset 开平仓标志（0-开仓，1-平昨，2-平今）
	 * @return 手续费金额
	 */
	double calc_fee(const char* stdCode, double price, double qty, uint32_t offset);  // 计算手续费

	/**
	 * @brief 获取交易时段信息
	 * 
	 * @param sid 时段ID或合约代码
	 * @param isCode 是否合约代码（默认false）
	 * @return 交易时段信息指针
	 */
	WTSSessionInfo*		get_session_info(const char* sid, bool isCode = false);  // 获取交易时段信息

	/**
	 * @brief 获取合约信息
	 * 
	 * @param stdCode 合约代码
	 * @return 合约信息指针
	 */
	WTSCommodityInfo*	get_commodity_info(const char* stdCode);       // 获取合约信息

	/**
	 * @brief 获取当前价格
	 * 
	 * @param stdCode 合约代码
	 * @return 当前价格
	 */
	double get_cur_price(const char* stdCode);                       // 获取当前价格

	/**
	 * @brief 获取日价格（开盘价、最高价、最低价、收盘价）
	 * 
	 * @param stdCode 合约代码
	 * @param flag 价格类型标志（0-收盘价，1-开盘价，2-最高价，3-最低价）
	 * @return 日价格
	 */
	double get_day_price(const char* stdCode, int flag = 0);          // 获取日价格

	/**
	 * @brief 获取原始合约代码
	 * 
	 * @param stdCode 标准化合约代码
	 * @return 原始合约代码
	 */
	std::string get_rawcode(const char* stdCode);                     // 获取原始合约代码

	/**
	 * @brief 订阅Tick数据
	 * 
	 * @param sid 上下文ID
	 * @param stdCode 合约代码
	 */
	void sub_tick(uint32_t sid, const char* stdCode);                // 订阅Tick数据

	/**
	 * @brief 订阅订单队列数据
	 * 
	 * @param sid 上下文ID
	 * @param stdCode 合约代码
	 */
	void sub_order_queue(uint32_t sid, const char* stdCode);         // 订阅订单队列数据

	/**
	 * @brief 订阅订单明细数据
	 * 
	 * @param sid 上下文ID
	 * @param stdCode 合约代码
	 */
	void sub_order_detail(uint32_t sid, const char* stdCode);        // 订阅订单明细数据

	/**
	 * @brief 订阅逐笔成交数据
	 * 
	 * @param sid 上下文ID
	 * @param stdCode 合约代码
	 */
	void sub_transaction(uint32_t sid, const char* stdCode);         // 订阅逐笔成交数据

	/**
	 * @brief 检查是否启用了Tick回放
	 * 
	 * @return 是否启用Tick回放
	 */
	inline bool	is_tick_enabled() const{ return _tick_enabled; }     // 检查是否启用Tick回放

	/**
	 * @brief 检查是否模拟Tick数据
	 * 
	 * @return 是否模拟Tick数据
	 */
	inline bool	is_tick_simulated() const { return _tick_simulated; }  // 检查是否模拟Tick数据

	/**
	 * @brief 更新价格映射
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 */
	inline void update_price(const char* stdCode, double price)      // 更新价格映射
	{
		_price_map[stdCode] = price;                                  // 更新价格映射表
	}

	/**
	 * @brief 获取主力合约管理器
	 * 
	 * @return 主力合约管理器指针
	 */
	inline IHotMgr*	get_hot_mgr() { return &_hot_mgr; }             // 获取主力合约管理器

private:
	IDataSink*		_listener;                                         // 数据接收器指针
	IBtDataLoader*	_bt_loader;                                        // 回测数据加载器指针
	std::string		_stra_name;                                        // 策略名称

	TickCache		_ticks_cache;	                                    // Tick缓存
	OrdDtlCache		_orddtl_cache;	                                    // 订单明细缓存
	OrdQueCache		_ordque_cache;	                                    // 订单队列缓存
	TransCache		_trans_cache;	                                    // 逐笔成交缓存

	BarsCache		_bars_cache;	                                    // K线缓存
	BarsCache		_unbars_cache;	                                    // 未订阅的K线缓存
	wt_hashset<std::string> _codes_in_subbed;                          // 已订阅的合约代码集合
	wt_hashset<std::string> _codes_in_unsubbed;                        // 未订阅的合约代码集合

	TaskInfoPtr		_task;                                             // 定时任务信息指针

	std::string		_main_key;                                          // 主键（主合约代码周期）
	std::string		_min_period;	                                    // 最小K线周期，这个主要用于未订阅品种的信号处理上
	std::string		_main_period;	                                    // 主周期
	bool			_tick_enabled;	                                    // 是否开启了Tick回测
	bool			_tick_simulated;	                                // 是否需要模拟Tick数据
	bool			_align_by_section;	                                // 重采样分钟线是否按小节对齐
	
	/*
	 *	By Wesley @ 2023.05.05
	 *	如果K线没有成交量，则不模拟tick
	 *	默认为false，主要是针对涨跌停的行情，也适用于不活跃的合约
	 */
	bool			_nosim_if_notrade;                                  // 如果没有成交量则不模拟Tick
	std::map<std::string, WTSTickStruct>	_day_cache;	                // 每日Tick缓存，当Tick回放未开放时，会用到该缓存
	std::map<std::string, std::string>		_ticker_keys;                // Ticker键映射表

	//By Wesley @ 2022.06.01
	//这个主要是针对不订阅而直接指定合约下单的场景
	wt_hashset<std::string>		_unsubbed_in_need;	                    // 未订阅但需要的K线集合

	//By Wesley @ 2022.08.15
	//复权标记，采用位运算表示，1|2|4,1表示成交量复权，2表示成交额复权，4表示总持复权，其他待定
	uint32_t		_adjust_flag;                                       // 复权标记（位运算）

	uint32_t		_cur_date;                                          // 当前日期（YYYYMMDD）
	uint32_t		_cur_time;                                          // 当前时间（HHMM）
	uint32_t		_cur_secs;                                          // 当前秒数（0-86399）
	uint32_t		_cur_tdate;                                         // 当前交易日期（YYYYMMDD）
	uint32_t		_closed_tdate;                                      // 已关闭的交易日期
	uint32_t		_opened_tdate;                                      // 已打开的交易日期

	WTSBaseDataMgr	_bd_mgr;                                            // 基础数据管理器
	WTSHotMgr		_hot_mgr;                                           // 主力合约管理器

	std::string		_base_dir;                                          // 基础数据目录
	std::string		_mode;                                              // 回放模式（bars/tasks/ticks）
	uint64_t		_begin_time;                                        // 开始时间（时间戳）
	uint64_t		_end_time;                                          // 结束时间（时间戳）

	//缓存自动清理天数
	uint32_t		_cache_clear_days;                                  // 缓存自动清理天数

	bool			_running;                                           // 是否正在运行
	bool			_terminated;                                        // 是否已终止
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 手续费项结构体
	 */
	typedef struct _FeeItem
	{
		double	_open;                                                // 开仓手续费
		double	_close;                                               // 平仓手续费
		double	_close_today;                                         // 平今手续费
		bool	_by_volume;                                           // 是否按手数收费（true-按手数，false-按金额）

		/**
		 * @brief 构造函数
		 */
		_FeeItem()
		{
			memset(this, 0, sizeof(_FeeItem));                       // 清零初始化
		}
	} FeeItem;                                                         // 手续费项结构体

	/**
	 * @brief 手续费映射表类型定义
	 */
	typedef wt_hashmap<std::string, FeeItem>	FeeMap;                 // 手续费映射表（合约代码 -> 手续费项）
	FeeMap		_fee_map;                                             // 手续费映射表

	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 价格映射表类型定义
	 */
	typedef wt_hashmap<std::string, double> PriceMap;                  // 价格映射表（合约代码 -> 价格）
	PriceMap		_price_map;                                         // 价格映射表

	//////////////////////////////////////////////////////////////////////////
	//
	//By Wesley @ 2022.02.07
	//tick数据订阅项，first是contextid，second是订阅选项，0-原始订阅，1-前复权，2-后复权
	/**
	 * @brief 订阅选项类型定义
	 * 
	 * first是上下文ID，second是订阅选项（0-原始订阅，1-前复权，2-后复权）
	 */
	typedef std::pair<uint32_t, uint32_t> SubOpt;                     // 订阅选项（上下文ID，订阅选项）

	/**
	 * @brief 订阅列表类型定义
	 */
	typedef wt_hashmap<uint32_t, SubOpt> SubList;                      // 订阅列表（上下文ID -> 订阅选项）

	/**
	 * @brief 策略订阅映射表类型定义
	 */
	typedef wt_hashmap<std::string, SubList>	StraSubMap;              // 策略订阅映射表（合约代码 -> 订阅列表）
	StraSubMap		_tick_sub_map;		                                // Tick数据订阅表
	StraSubMap		_ordque_sub_map;	                                // 订单队列数据订阅表
	StraSubMap		_orddtl_sub_map;	                                // 订单明细数据订阅表
	StraSubMap		_trans_sub_map;		                            // 逐笔成交数据订阅表

	/**
	 * @brief 复权因子结构体
	 */
	//除权因子
	typedef struct _AdjFactor
	{
		uint32_t	_date;                                              // 日期
		double		_factor;                                            // 复权因子
	} AdjFactor;                                                      // 复权因子结构体

	/**
	 * @brief 复权因子列表类型定义
	 */
	typedef std::vector<AdjFactor> AdjFactorList;                     // 复权因子列表

	/**
	 * @brief 复权因子映射表类型定义
	 */
	typedef wt_hashmap<std::string, AdjFactorList>	AdjFactorMap;      // 复权因子映射表（合约代码 -> 复权因子列表）
	AdjFactorMap	_adj_factors;                                      // 复权因子映射表

	/**
	 * @brief 获取复权因子列表（私有辅助函数）
	 * 
	 * @param code 合约代码
	 * @param exchg 交易所代码
	 * @param pid 产品ID
	 * @return 复权因子列表的引用
	 */
	const AdjFactorList& getAdjFactors(const char* code, const char* exchg, const char* pid);  // 获取复权因子列表

	EventNotifier*	_notifier;                                         // 事件通知器指针

	HisDataMgr		_his_dt_mgr;                                       // 历史数据管理器
};

