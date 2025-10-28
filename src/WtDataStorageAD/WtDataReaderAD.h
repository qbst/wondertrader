/*!
 * \file WtDataReaderAD.h
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块实时数据读取器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataReaderAD类，这是WonderTrader框架中的高级实时数据读取器。
 * 该类基于LMDB数据库和内存映射缓存技术，为策略引擎提供高性能的实时和历史
 * 数据访问服务，支持Tick和K线数据的智能缓存和增量更新。
 * 
 * 核心设计理念：
 * 
 * 1. 混合存储架构（Hybrid Storage Architecture）：
 *    - LMDB持久化存储：历史数据的高性能访问
 *    - 内存映射缓存：实时数据的快速读写
 *    - 智能缓存策略：热点数据的内存优化
 * 
 * 2. 增量更新机制（Incremental Update Mechanism）：
 *    - 实时数据增量加载
 *    - 缓存与数据库的智能同步
 *    - 最小化数据传输和处理开销
 * 
 * 3. 多级缓存设计（Multi-Level Cache Design）：
 *    - L1缓存：内存中的循环缓冲区
 *    - L2缓存：内存映射的实时缓存文件
 *    - L3存储：LMDB持久化数据库
 * 
 * 数据流转架构：
 * 
 *   策略请求
 *       ↓
 *   WtDataReaderAD
 *       ↓
 *   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
 *   │  内存缓存       │    │  映射文件缓存   │    │  LMDB数据库     │
 *   │  (最新数据)     │ ←→ │  (实时数据)     │ ←→ │  (历史数据)     │
 *   │  CircularBuffer │    │  RTCache        │    │  Persistent     │
 *   └─────────────────┘    └─────────────────┘    └─────────────────┘
 * 
 * 缓存管理策略：
 * 
 * 1. 数据分层（Data Tiering）：
 *    - 最新数据：保存在内存循环缓冲区中
 *    - 当日数据：保存在内存映射缓存文件中
 *    - 历史数据：保存在LMDB数据库中
 * 
 * 2. 智能加载（Smart Loading）：
 *    - 按需加载历史数据到缓存
 *    - 增量更新实时数据
 *    - 缓存容量动态调整
 * 
 * 3. 时间同步（Time Synchronization）：
 *    - onMinuteEnd事件驱动更新
 *    - 实时缓存与数据库同步
 *    - 跨日数据的平滑切换
 * 
 * 性能优化特点：
 * - 零拷贝数据访问
 * - 智能预取和缓存
 * - 高效的时间范围查询
 * - 多线程安全的并发访问
 */

#pragma once                                    // 防止头文件重复包含
#include <string>                               // 引入字符串类
#include <stdint.h>                             // 引入标准整数类型
#include <boost/circular_buffer.hpp>            // 引入Boost循环缓冲区

#include "DataDefineAD.h"                       // 引入数据结构定义

#include "../WTSUtils/WtLMDB.hpp"               // 引入LMDB数据库封装
#include "../Includes/FasterDefs.h"             // 引入高性能数据结构
#include "../Includes/IDataReader.h"            // 引入数据读取器接口

#include "../Share/StdUtils.hpp"                // 引入标准工具类
#include "../Share/BoostMappingFile.hpp"        // 引入内存映射文件类

NS_WTP_BEGIN                                    // 开始WonderTrader命名空间

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;  ///< 内存映射文件智能指针类型

/**
 * @class WtDataReaderAD
 * @brief WonderTrader高级实时数据读取器
 * 
 * 基于LMDB数据库和内存映射缓存的高性能实时数据读取器，为策略引擎提供
 * Tick和K线数据的智能缓存和高效访问服务。
 * 
 * 主要特性：
 * 
 * 1. 混合存储架构：
 *    - LMDB数据库：持久化历史数据存储
 *    - 内存映射缓存：实时数据快速访问
 *    - 循环缓冲区：最新数据的内存缓存
 * 
 * 2. 智能缓存管理：
 *    - 按需加载历史数据
 *    - 增量更新实时数据
 *    - 自适应缓存容量调整
 * 
 * 3. 高性能数据访问：
 *    - 零拷贝数据读取
 *    - 时间范围查询优化
 *    - 多线程并发安全
 * 
 * 典型使用场景：
 * @code
 *   // 1. 创建并初始化读取器
 *   WtDataReaderAD* reader = new WtDataReaderAD();
 *   reader->init(config, sink, loader);
 *   
 *   // 2. 读取最新的100个Tick数据
 *   WTSTickSlice* ticks = reader->readTickSlice("SHFE.rb2305", 100);
 *   
 *   // 3. 读取最新的50根1分钟K线
 *   WTSKlineSlice* bars = reader->readKlineSlice("SHFE.rb2305", KP_Minute1, 50);
 *   
 *   // 4. 分钟结束时更新缓存
 *   reader->onMinuteEnd(20230508, 1030);
 * @endcode
 */
class WtDataReaderAD : public IDataReader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建WtDataReaderAD实例，初始化基本成员变量。
	 */
	WtDataReaderAD();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，关闭数据库连接和内存映射文件。
	 */
	virtual ~WtDataReaderAD();

public:
	/**
	 * @brief 初始化数据读取器
	 * 
	 * 根据配置参数初始化数据读取器，设置存储路径、缓存参数等。
	 * 
	 * @param cfg 配置参数对象，包含：
	 *            - path: 数据存储根目录
	 * @param sink 回调接口，提供基础数据管理器和热点管理器
	 * @param loader 历史数据加载器（可选）
	 */
	virtual void init(WTSVariant* cfg, IDataReaderSink* sink, IHisDataLoader* loader = NULL) override;

	/**
	 * @brief 分钟结束事件处理
	 * 
	 * 在每分钟结束时被调用，用于更新缓存数据，同步实时缓存与数据库。
	 * 
	 * @param uDate 当前日期（YYYYMMDD格式）
	 * @param uTime 当前时间（HHmm格式）
	 * @param endTDate 结束交易日（可选，用于日线处理）
	 * 
	 * 处理流程：
	 * 1. 检查缓存中的K线数据
	 * 2. 从LMDB更新最新的K线
	 * 3. 从实时缓存补充未完成的K线
	 * 4. 触发策略的onBar事件
	 */
	virtual void onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate = 0) override;

	/**
	 * @brief 读取Tick数据切片
	 * 
	 * 读取指定合约的最新Tick数据，支持智能缓存和增量加载。
	 * 
	 * @param stdCode 标准合约代码（如"SHFE.rb.HOT"）
	 * @param count 需要读取的Tick数量
	 * @param etime 结束时间（0表示当前时间）
	 * @return Tick数据切片对象，失败返回NULL
	 * 
	 * 数据获取策略：
	 * 1. 检查内存缓存是否满足需求
	 * 2. 从LMDB增量加载历史数据
	 * 3. 返回指定数量的最新数据
	 */
	virtual WTSTickSlice*	readTickSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;

	/**
	 * @brief 读取K线数据切片
	 * 
	 * 读取指定合约和周期的最新K线数据，支持多级缓存和实时更新。
	 * 
	 * @param stdCode 标准合约代码（如"SHFE.rb.HOT"）
	 * @param period K线周期（KP_Minute1、KP_Minute5、KP_DAY等）
	 * @param count 需要读取的K线数量
	 * @param etime 结束时间（0表示当前时间）
	 * @return K线数据切片对象，失败返回NULL
	 * 
	 * 数据合成策略：
	 * 1. 从缓存中获取历史K线
	 * 2. 从LMDB更新最新完整K线
	 * 3. 从实时缓存获取未完成K线
	 * 4. 合成完整的K线序列
	 */
	virtual WTSKlineSlice*	readKlineSlice(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime = 0) override;

private:
	std::string		_base_dir;				///< 数据存储根目录路径
	IBaseDataMgr*	_base_data_mgr;			///< 基础数据管理器指针
	IHotMgr*		_hot_mgr;				///< 热点合约管理器指针

	//////////////////////////////////////////////////////////////////////////
	// 实时缓存管理（Real-time Cache Management）
	
	/**
	 * @struct RTBarCacheWrapper
	 * @brief 实时K线缓存包装器
	 * 
	 * 封装内存映射文件的K线缓存，提供线程安全的访问接口。
	 * 每个K线周期（1分钟、5分钟、日线）都有独立的缓存实例。
	 */
	typedef struct _RTBarCacheWrapper
	{
		StdUniqueMutex	_mtx;					///< 互斥锁，保证线程安全访问
		std::string		_filename;				///< 缓存文件名
		wt_hashmap<std::string, uint32_t> _idx;	///< 合约索引映射表（key: "交易所.合约"）
		BoostMFPtr		_file_ptr;				///< 内存映射文件指针
		RTBarCache*		_cache_block;			///< 缓存数据块指针
		uint32_t		_last_size;				///< 上次检查时的缓存大小

		/**
		 * @brief 构造函数
		 * 
		 * 初始化缓存包装器，设置默认值。
		 */
		_RTBarCacheWrapper() :_cache_block(NULL), _file_ptr(NULL), _last_size(0){}

		/**
		 * @brief 检查缓存是否为空
		 * 
		 * @return 缓存为空返回true，否则返回false
		 */
		inline bool empty() const { return _cache_block == NULL; }
	} RTBarCacheWrapper;

	RTBarCacheWrapper _m1_cache;				///< 1分钟K线实时缓存
	RTBarCacheWrapper _m5_cache;				///< 5分钟K线实时缓存
	RTBarCacheWrapper _d1_cache;				///< 日K线实时缓存

	/**
	 * @struct BarsList
	 * @brief K线数据列表结构
	 * 
	 * 存储特定合约和周期的K线数据，使用循环缓冲区提供高效的数据访问。
	 * 支持增量更新和缓存状态管理。
	 */
	typedef struct _BarsList
	{
		std::string		_exchg;					///< 交易所代码
		std::string		_code;					///< 合约代码
		WTSKlinePeriod	_period;				///< K线周期
		bool			_last_from_cache;		///< 最后一条K线是否来自实时缓存
		uint64_t		_last_req_time;			///< 最后请求时间戳

		boost::circular_buffer<WTSBarStruct>	_bars;	///< K线数据循环缓冲区

		/**
		 * @brief 构造函数
		 * 
		 * 初始化K线列表，设置默认状态。
		 */
		_BarsList():_last_from_cache(false),_last_req_time(0){}
	} BarsList;

	/**
	 * @struct TicksList
	 * @brief Tick数据列表结构
	 * 
	 * 存储特定合约的Tick数据，使用循环缓冲区提供高效的数据访问。
	 * 支持增量加载和时间戳管理。
	 */
	typedef struct _TicksList
	{
		std::string		_exchg;					///< 交易所代码
		std::string		_code;					///< 合约代码
		uint64_t		_last_req_time;			///< 最后请求时间戳

		boost::circular_buffer<WTSTickStruct>	_ticks;	///< Tick数据循环缓冲区

		/**
		 * @brief 构造函数
		 * 
		 * 初始化Tick列表，设置默认时间戳。
		 */
		_TicksList():_last_req_time(0){}
	} TicksList;

	typedef wt_hashmap<std::string, BarsList> BarsCache;	///< K线缓存映射表类型
	BarsCache	_bars_cache;				///< K线数据缓存（key: "合约#周期"格式）

	typedef wt_hashmap<std::string, TicksList> TicksCache;	///< Tick缓存映射表类型
	TicksCache	_ticks_cache;				///< Tick数据缓存（key: "交易所.合约"格式）

	uint64_t	_last_time;					///< 最后处理的时间戳（用于onMinuteEnd去重）

private:
	//////////////////////////////////////////////////////////////////////////
	// 私有辅助方法（Private Helper Methods）
	
	/**
	 * @brief 从存储中缓存K线数据
	 * 
	 * 从LMDB数据库中加载指定数量的历史K线数据到内存缓存中。
	 * 
	 * @param key 缓存键值
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param count 需要加载的K线数量
	 * @return 加载成功返回true，失败返回false
	 */
	bool	cacheBarsFromStorage(const std::string& key, const char* stdCode, WTSKlinePeriod period, uint32_t count);

	/**
	 * @brief 从LMDB更新缓存数据
	 * 
	 * 从LMDB数据库中增量更新K线缓存，获取最新的完整K线数据。
	 * 
	 * @param barsList K线列表引用
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param period K线周期
	 * @param lastBarTime 最后K线时间（输入输出参数）
	 */
	void	update_cache_from_lmdb(BarsList& barsList, const char* exchg, const char* code, WTSKlinePeriod period, uint32_t& lastBarTime);

	/**
	 * @brief 读取K线数据到缓冲区
	 * 
	 * 从LMDB数据库中读取指定合约的所有K线数据到字符串缓冲区。
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param period K线周期
	 * @return 包含K线数据的字符串缓冲区
	 */
	std::string	read_bars_to_buffer(const char* exchg, const char* code, WTSKlinePeriod period);

	/**
	 * @brief 获取实时缓存中的K线数据
	 * 
	 * 从内存映射的实时缓存中获取指定合约的最新K线数据。
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param period K线周期
	 * @return K线数据指针，失败返回NULL
	 */
	WTSBarStruct* get_rt_cache_bar(const char* exchg, const char* code, WTSKlinePeriod period);

private:
	//////////////////////////////////////////////////////////////////////////
	// LMDB数据库管理（LMDB Database Management）
	/*
	 * LMDB数据库组织结构：
	 * 
	 * K线数据：按交易所和周期分组
	 * - 路径格式：{_base_dir}/{period}/{exchg}/
	 * - 示例：./data/min1/SHFE/、./data/min5/DCE/、./data/day/CZCE/
	 * 
	 * Tick数据：按交易所和合约分组
	 * - 路径格式：{_base_dir}/ticks/{exchg}/{code}/
	 * - 示例：./data/ticks/SHFE/rb2305/、./data/ticks/DCE/i2305/
	 */
	
	typedef std::shared_ptr<WtLMDB> WtLMDBPtr;				///< LMDB数据库智能指针类型
	typedef wt_hashmap<std::string, WtLMDBPtr> WtLMDBMap;	///< 数据库映射表类型

	WtLMDBMap	_exchg_m1_dbs;				///< 1分钟K线数据库映射表（key: 交易所代码）
	WtLMDBMap	_exchg_m5_dbs;				///< 5分钟K线数据库映射表（key: 交易所代码）
	WtLMDBMap	_exchg_d1_dbs;				///< 日K线数据库映射表（key: 交易所代码）

	WtLMDBMap	_tick_dbs;					///< Tick数据库映射表（key: "交易所.合约"格式）

	/**
	 * @brief 获取K线数据库连接
	 * 
	 * 根据交易所和K线周期获取对应的LMDB数据库连接。
	 * 如果数据库尚未打开，会自动创建连接并加入缓存。
	 * 
	 * @param exchg 交易所代码
	 * @param period K线周期
	 * @return LMDB数据库智能指针，失败返回空指针
	 */
	WtLMDBPtr	get_k_db(const char* exchg, WTSKlinePeriod period);

	/**
	 * @brief 获取Tick数据库连接
	 * 
	 * 根据交易所和合约代码获取对应的LMDB数据库连接。
	 * 如果数据库尚未打开，会自动创建连接并加入缓存。
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @return LMDB数据库智能指针，失败返回空指针
	 */
	WtLMDBPtr	get_t_db(const char* exchg, const char* code);
};

NS_WTP_END                                      // 结束WonderTrader命名空间
