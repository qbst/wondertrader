/*!
 * \file WtDataManager.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据管理器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataManager（数据管理器）类，是WonderTrader数据伺服器模块中用于
 * 管理历史数据读取、K线数据生成、实时K线更新等核心数据管理功能的类。该类实现了
 * IRdmDtReaderSink接口，作为数据读取器的回调接收者，负责协调数据读取、缓存管理和
 * 实时数据处理。
 * 
 * 核心设计理念：
 * 
 * 1. 数据读取接口（Data Reading Interface）：
 *    - 实现IRdmDtReaderSink接口，接收数据读取器的回调
 *    - 提供统一的数据查询接口（按日期、按范围、按数量）
 *    - 支持多种数据类型的查询（K线、Tick、Level-2数据）
 * 
 * 2. 数据缓存机制（Data Caching Mechanism）：
 *    - K线数据缓存（_bars_cache）：缓存已查询的K线数据
 *    - 实时K线映射（_rt_bars）：管理实时生成的K线数据
 *    - 智能缓存策略，减少重复查询
 * 
 * 3. 实时K线生成（Real-time Bar Generation）：
 *    - 从Tick数据实时生成K线
 *    - 支持多种K线周期（分钟、秒级等）
 *    - 订阅管理，只生成订阅的K线
 * 
 * 4. 交易时段处理（Trading Session Handling）：
 *    - 支持按交易时段对齐K线数据
 *    - 获取交易时段信息用于K线生成
 *    - 处理不同交易所的交易时段差异
 * 
 * 主要功能模块：
 * 
 * 1. 数据查询功能：
 *    - K线数据查询（按日期、范围、数量）
 *    - Tick数据查询（按日期、范围、数量）
 *    - Level-2数据查询（逐笔委托、委托队列、逐笔成交）
 *    - 秒级K线查询
 * 
 * 2. 实时数据处理：
 *    - 实时K线订阅和更新
 *    - Tick数据驱动的K线生成
 *    - 缓存管理和清理
 * 
 * 3. 数据格式化：
 *    - 时间格式转换
 *    - 复权因子处理
 *    - 数据切片封装
 * 
 * 使用场景：
 * - 历史数据查询和分析
 * - 实时行情数据处理
 * - K线数据的生成和维护
 * - 数据缓存的优化管理
 * 
 * 技术特点：
 * - 高效的缓存策略
 * - 线程安全的数据访问
 * - 灵活的数据查询接口
 * - 支持多种数据源
 * 
 * 注意事项：
 * - 缓存的数据需要手动释放
 * - 实时K线需要先订阅才能更新
 * - 数据查询结果需要调用release()释放
 * - 线程安全考虑使用互斥锁保护
 */
#pragma once                                                                     // 防止头文件重复包含
#include <vector>                                                                // 包含向量容器
#include <stdint.h>                                                              // 包含标准整数类型定义

#include "../Includes/IDataManager.h"                                           // 包含数据管理器接口定义
#include "../Includes/IRdmDtReader.h"                                            // 包含随机数据读取器接口定义
#include "../Includes/FasterDefs.h"                                              // 包含快速数据结构定义
#include "../Includes/WTSCollection.hpp"                                         // 包含集合容器定义
#include "../Share/StdUtils.hpp"                                                 // 包含标准工具类

class WtDtRunner;                                                                // 数据服务运行器类前向声明

NS_WTP_BEGIN                                                                    // WonderTrader命名空间开始
class WTSVariant;                                                                // 配置变体类前向声明
class WTSTickData;                                                               // Tick数据类前向声明
class WTSKlineSlice;                                                             // K线数据切片类前向声明
class WTSKlineData;                                                              // K线数据类前向声明
class WTSTickSlice;                                                              // Tick数据切片类前向声明
class IBaseDataMgr;                                                              // 基础数据管理器接口前向声明
class IHotMgr;                                                                   // 主力合约管理器接口前向声明
class WTSSessionInfo;                                                            // 交易时段信息类前向声明
struct WTSBarStruct;                                                             // K线数据结构前向声明
class WTSCommodityInfo;                                                          // 品种信息类前向声明

/**
 * @class WtDataManager
 * @brief 数据管理器类
 * 
 * 负责管理历史数据读取、K线数据生成、实时K线更新等核心数据管理功能。
 * 实现了IRdmDtReaderSink接口，作为数据读取器的回调接收者。
 */
class WtDataManager : public IRdmDtReaderSink                                 // 继承IRdmDtReaderSink接口
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化数据管理器对象，设置所有成员变量为初始值。
	 */
	WtDataManager();

	/**
	 * @brief 析构函数
	 * 
	 * 清理数据管理器资源，释放所有缓存的数据。
	 */
	~WtDataManager();

private:
	/**
	 * @brief 初始化数据存储模块
	 * @param cfg 配置信息（包含数据存储模块配置）
	 * @return 是否初始化成功
	 * 
	 * 根据配置信息加载数据存储模块（IRdmDtReader），用于读取历史数据。
	 */
	bool	initStore(WTSVariant* cfg);

	/**
	 * @brief 获取交易时段信息
	 * @param sid 合约代码或品种ID
	 * @param isCode 是否为合约代码（true=合约代码，false=品种ID）
	 * @return 交易时段信息指针（如果不存在则返回NULL）
	 * 
	 * 获取指定合约或品种的交易时段信息，用于K线生成和数据对齐。
	 */
	WTSSessionInfo* get_session_info(const char* sid, bool isCode = false);

//////////////////////////////////////////////////////////////////////////
//IRdmDtReaderSink接口实现
//////////////////////////////////////////////////////////////////////////
public:
	/**
	 * @brief 获取基础数据管理接口指针
	 * @return 基础数据管理器指针
	 * 
	 * 返回基础数据管理器，用于获取合约信息、品种信息等基础数据。
	 */
	virtual IBaseDataMgr*	get_basedata_mgr() override { return _bd_mgr; }

	/**
	 * @brief 获取主力切换规则管理接口指针
	 * @return 主力合约管理器指针
	 * 
	 * 返回主力合约管理器，用于处理主力合约切换逻辑。
	 */
	virtual IHotMgr*		get_hot_mgr() override { return _hot_mgr; }

	/**
	 * @brief 输出数据读取模块的日志
	 * @param ll 日志级别
	 * @param message 日志消息
	 * 
	 * 将数据读取模块的日志转发到WonderTrader的日志系统。
	 */
	virtual void			reader_log(WTSLogLevel ll, const char* message) override;

public:
	/**
	 * @brief 初始化数据管理器
	 * @param cfg 配置信息
	 * @param runner 数据服务运行器指针
	 * @return 是否初始化成功
	 * 
	 * 初始化数据管理器，设置基础数据管理器、主力合约管理器等。
	 * 配置信息应包含数据存储模块的配置。
	 */
	bool	init(WTSVariant* cfg, WtDtRunner* runner);

	// ===== Level-2数据查询接口 =====

	/**
	 * @brief 查询委托队列数据切片
	 * @param stdCode 标准化合约代码
	 * @param stime 开始时间（格式：yyyymmddHHMMSS）
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
	 * @return 委托队列数据切片指针（需要调用release()释放）
	 */
	WTSOrdQueSlice* get_order_queue_slice(const char* stdCode, uint64_t stime, uint64_t etime = 0);

	/**
	 * @brief 查询逐笔委托数据切片
	 * @param stdCode 标准化合约代码
	 * @param stime 开始时间（格式：yyyymmddHHMMSS）
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
	 * @return 逐笔委托数据切片指针（需要调用release()释放）
	 */
	WTSOrdDtlSlice* get_order_detail_slice(const char* stdCode, uint64_t stime, uint64_t etime = 0);

	/**
	 * @brief 查询逐笔成交数据切片
	 * @param stdCode 标准化合约代码
	 * @param stime 开始时间（格式：yyyymmddHHMMSS）
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
	 * @return 逐笔成交数据切片指针（需要调用release()释放）
	 */
	WTSTransSlice* get_transaction_slice(const char* stdCode, uint64_t stime, uint64_t etime = 0);

	// ===== K线数据查询接口 =====

	/**
	 * @brief 按日期查询Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
	 * @return Tick数据切片指针（需要调用release()释放）
	 */
	WTSTickSlice* get_tick_slice_by_date(const char* stdCode, uint32_t uDate = 0);

	/**
	 * @brief 按日期查询秒级K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param secs 秒数（如：60表示60秒K线）
	 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
	 * @return K线数据切片指针（需要调用release()释放）
	 * 
	 * 从Tick数据生成指定秒数的K线数据。
	 */
	WTSKlineSlice* get_skline_slice_by_date(const char* stdCode, uint32_t secs, uint32_t uDate = 0);

	/**
	 * @brief 按日期查询K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
	 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
	 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice* get_kline_slice_by_date(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t uDate = 0);

	/**
	 * @brief 按时间范围查询Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param stime 开始时间（格式：yyyymmddHHMMSS）
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
	 * @return Tick数据切片指针（需要调用release()释放）
	 */
	WTSTickSlice* get_tick_slices_by_range(const char* stdCode, uint64_t stime, uint64_t etime = 0);

	/**
	 * @brief 按时间范围查询K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
	 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
	 * @param stime 开始时间（格式：yyyymmddHHMMSS）
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice* get_kline_slice_by_range(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint64_t stime, uint64_t etime = 0);

	/**
	 * @brief 按数量查询Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 查询的数据条数
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
	 * @return Tick数据切片指针（需要调用release()释放）
	 */
	WTSTickSlice* get_tick_slice_by_count(const char* stdCode, uint32_t count, uint64_t etime = 0);

	/**
	 * @brief 按数量查询K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
	 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
	 * @param count 查询的数据条数
	 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice* get_kline_slice_by_count(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime = 0);

	/**
	 * @brief 获取复权因子
	 * @param stdCode 标准化合约代码
	 * @param commInfo 品种信息指针（可选，如果为NULL则自动获取）
	 * @return 复权因子（1.0表示不复权）
	 * 
	 * 获取股票的复权因子，用于K线数据的复权处理。
	 * 期货合约通常返回1.0（不复权）。
	 */
	double	get_exright_factor(const char* stdCode, WTSCommodityInfo* commInfo = NULL);

	// ===== 实时K线管理接口 =====

	/**
	 * @brief 订阅实时K线数据
	 * @param stdCode 标准化合约代码
	 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
	 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
	 * 
	 * 订阅指定合约和周期的实时K线数据。
	 * 订阅后，当有新Tick数据时，会自动更新对应的K线。
	 */
	void	subscribe_bar(const char* stdCode, WTSKlinePeriod period, uint32_t times);

	/**
	 * @brief 清除所有订阅的K线
	 * 
	 * 清除所有已订阅的实时K线数据。
	 */
	void	clear_subbed_bars();

	/**
	 * @brief 更新K线数据
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 根据新的Tick数据更新订阅的K线。
	 * 只有当订阅了对应合约和周期的K线时才会更新。
	 */
	void	update_bars(const char* stdCode, WTSTickData* newTick);

	/**
	 * @brief 清理数据缓存
	 * 
	 * 清理所有已缓存的数据，释放内存。
	 * 包括K线数据缓存和实时K线映射。
	 */
	void	clear_cache();

private:
	IRdmDtReader*			_reader;                                             // 随机数据读取器指针（用于读取历史数据）
	FuncDeleteRdmDtReader	_remover;                                             // 数据读取器删除函数指针（用于动态库创建的读取器）

	IBaseDataMgr*	_bd_mgr;                                                 // 基础数据管理器指针（用于获取合约信息等）
	IHotMgr*		_hot_mgr;                                                 // 主力合约管理器指针（用于处理主力合约切换）
	WtDtRunner*		_runner;                                                  // 数据服务运行器指针（用于触发回调等）
	bool			_align_by_section;                                         // 是否按交易时段对齐K线数据

	//K线缓存结构定义
	/**
	 * @struct _BarCache
	 * @brief K线缓存结构
	 * 
	 * 用于缓存已查询的K线数据，避免重复查询。
	 */
	typedef struct _BarCache
	{
		WTSKlineData*	_bars;                                                  // 缓存的K线数据指针
		uint64_t		_last_bartime;                                           // 最后一条K线的时间戳
		WTSKlinePeriod	_period;                                                 // K线周期
		uint32_t		_times;                                                  // 周期倍数

		/**
		 * @brief 构造函数
		 * 
		 * 初始化缓存结构，所有成员变量设置为默认值。
		 */
		_BarCache():_last_bartime(0),_period(KP_DAY),_times(1),_bars(NULL){}  // 初始化：最后K线时间为0，周期为日线，倍数为1，数据指针为NULL
	} BarCache;
	typedef wt_hashmap<std::string, BarCache>	BarCacheMap;                    // K线缓存映射表类型定义（键=合约代码+日期+周期）
	BarCacheMap	_bars_cache;                                                  // K线数据缓存映射表：存储已查询的K线数据

	typedef WTSHashMap<std::string>	RtBarMap;                                   // 实时K线映射表类型定义（键=合约代码+周期）
	RtBarMap*		_rt_bars;                                                   // 实时K线映射表指针：存储实时生成的K线数据
	StdUniqueMutex	_mtx_rtbars;                                                 // 实时K线映射表互斥锁：保护多线程访问
};

NS_WTP_END                                                                      // WonderTrader命名空间结束