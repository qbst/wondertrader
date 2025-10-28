/*!
 * \file WtRdmDtReaderAD.h
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块随机数据读取器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtRdmDtReaderAD类，这是WonderTrader框架中的高级随机数据读取器。
 * 该类基于LMDB数据库实现，为数据分析和研究工具提供灵活的历史数据随机访问
 * 服务，支持按时间范围、按数量、按日期等多种方式查询Tick和K线数据。
 * 
 * 核心设计理念：
 * 
 * 1. 随机访问优化（Random Access Optimization）：
 *    - 支持任意时间范围的数据查询
 *    - 支持按数量的反向查询
 *    - 支持按日期的批量查询
 * 
 * 2. 智能缓存策略（Smart Caching Strategy）：
 *    - 增量加载历史数据
 *    - 时间范围扩展缓存
 *    - 减少重复数据库访问
 * 
 * 3. 多维度查询接口（Multi-dimensional Query Interface）：
 *    - 时间范围查询：readXXXByRange()
 *    - 数量限制查询：readXXXByCount()
 *    - 日期批量查询：readXXXByDate()
 * 
 * 查询模式对比：
 * 
 * 1. 按时间范围查询（Range Query）：
 *    - 适用场景：分析特定时间段的数据
 *    - 查询方式：指定开始和结束时间
 *    - 返回结果：时间范围内的所有数据
 * 
 * 2. 按数量查询（Count Query）：
 *    - 适用场景：获取最新的N条数据
 *    - 查询方式：指定数据条数和结束时间
 *    - 返回结果：从结束时间往前的N条数据
 * 
 * 3. 按日期查询（Date Query）：
 *    - 适用场景：获取某个交易日的全部数据
 *    - 查询方式：指定交易日期
 *    - 返回结果：该日期的所有数据
 * 
 * 缓存管理策略：
 * 
 * 1. 增量扩展（Incremental Expansion）：
 *    - 检测查询范围是否超出缓存
 *    - 自动扩展缓存到新的时间范围
 *    - 保持已有数据，只加载新数据
 * 
 * 2. 双向扩展（Bidirectional Expansion）：
 *    - 支持向前扩展（更早的数据）
 *    - 支持向后扩展（更新的数据）
 *    - 智能判断扩展方向和范围
 * 
 * 使用场景：
 * - 量化研究的历史数据分析
 * - 策略回测的数据准备
 * - 数据质量检查和验证
 * - 自定义时间范围的数据导出
 */

#pragma once                                    // 防止头文件重复包含
#include <string>                               // 引入字符串类
#include <stdint.h>                             // 引入标准整数类型
#include <boost/circular_buffer.hpp>            // 引入Boost循环缓冲区

#include "DataDefineAD.h"                       // 引入数据结构定义

#include "../WTSUtils/WtLMDB.hpp"               // 引入LMDB数据库封装
#include "../Includes/FasterDefs.h"             // 引入高性能数据结构
#include "../Includes/IRdmDtReader.h"           // 引入随机数据读取器接口

#include "../Share/StdUtils.hpp"                // 引入标准工具类
#include "../Share/BoostMappingFile.hpp"        // 引入内存映射文件类

NS_WTP_BEGIN                                    // 开始WonderTrader命名空间

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;  ///< 内存映射文件智能指针类型

/**
 * @class WtRdmDtReaderAD
 * @brief WonderTrader高级随机数据读取器
 * 
 * 基于LMDB数据库的高性能随机数据读取器，为数据分析工具提供灵活的
 * 历史数据访问服务，支持多种查询模式和智能缓存管理。
 * 
 * 主要特性：
 * 
 * 1. 多维度查询支持：
 *    - 时间范围查询
 *    - 数量限制查询
 *    - 日期批量查询
 * 
 * 2. 智能缓存管理：
 *    - 增量数据加载
 *    - 双向缓存扩展
 *    - 时间范围优化
 * 
 * 3. 高性能数据访问：
 *    - LMDB零拷贝读取
 *    - 内存缓存加速
 *    - 批量数据处理
 * 
 * 典型使用场景：
 * @code
 *   // 1. 创建并初始化读取器
 *   WtRdmDtReaderAD* reader = new WtRdmDtReaderAD();
 *   reader->init(config, sink);
 *   
 *   // 2. 按时间范围查询Tick数据
 *   WTSTickSlice* ticks = reader->readTickSliceByRange("SHFE.rb2305", 
 *                                                      20230508090000000, 
 *                                                      20230508150000000);
 *   
 *   // 3. 按数量查询K线数据
 *   WTSKlineSlice* bars = reader->readKlineSliceByCount("SHFE.rb2305", 
 *                                                       KP_Minute1, 100);
 *   
 *   // 4. 按日期查询全天数据
 *   WTSTickSlice* dayTicks = reader->readTickSliceByDate("SHFE.rb2305", 20230508);
 * @endcode
 */
class WtRdmDtReaderAD : public IRdmDtReader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建WtRdmDtReaderAD实例，初始化基本成员变量。
	 */
	WtRdmDtReaderAD();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，关闭数据库连接。
	 */
	virtual ~WtRdmDtReaderAD();

//////////////////////////////////////////////////////////////////////////
// IRdmDtReader接口实现
public:
	/**
	 * @brief 初始化随机数据读取器
	 * 
	 * 根据配置参数初始化数据读取器，设置存储路径等。
	 * 
	 * @param cfg 配置参数对象，包含：
	 *            - path: 数据存储根目录
	 * @param sink 回调接口，提供基础数据管理器和热点管理器
	 */
	virtual void init(WTSVariant* cfg, IRdmDtReaderSink* sink);

	/**
	 * @brief 按范围读取委托明细数据（未实现）
	 * 
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @return 始终返回NULL（功能未实现）
	 */
	virtual WTSOrdDtlSlice*	readOrdDtlSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override { return NULL; }
	
	/**
	 * @brief 按范围读取委托队列数据（未实现）
	 * 
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @return 始终返回NULL（功能未实现）
	 */
	virtual WTSOrdQueSlice*	readOrdQueSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override { return NULL; }
	
	/**
	 * @brief 按范围读取逐笔成交数据（未实现）
	 * 
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @return 始终返回NULL（功能未实现）
	 */
	virtual WTSTransSlice*	readTransSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override { return NULL; }

	/**
	 * @brief 按时间范围读取Tick数据
	 * 
	 * 读取指定时间范围内的Tick数据，支持智能缓存和增量加载。
	 * 
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间（格式：YYYYMMDDHHMMSSsss）
	 * @param etime 结束时间（格式：YYYYMMDDHHMMSSsss）
	 * @return Tick数据切片对象，失败返回NULL
	 */
	virtual WTSTickSlice*	readTickSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override;
	
	/**
	 * @brief 按时间范围读取K线数据
	 * 
	 * 读取指定时间范围内的K线数据，支持智能缓存和增量加载。
	 * 
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param stime 开始时间
	 * @param etime 结束时间
	 * @return K线数据切片对象，失败返回NULL
	 */
	virtual WTSKlineSlice*	readKlineSliceByRange(const char* stdCode, WTSKlinePeriod period, uint64_t stime, uint64_t etime = 0) override;

	/**
	 * @brief 按数量读取Tick数据
	 * 
	 * 从指定结束时间往前读取指定数量的Tick数据。
	 * 
	 * @param stdCode 标准合约代码
	 * @param count 需要读取的数据条数
	 * @param etime 结束时间（0表示当前时间）
	 * @return Tick数据切片对象，失败返回NULL
	 */
	virtual WTSTickSlice*	readTickSliceByCount(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 按数量读取K线数据
	 * 
	 * 从指定结束时间往前读取指定数量的K线数据。
	 * 
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param count 需要读取的数据条数
	 * @param etime 结束时间（0表示当前时间）
	 * @return K线数据切片对象，失败返回NULL
	 */
	virtual WTSKlineSlice*	readKlineSliceByCount(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime = 0) override;

	/**
	 * @brief 按日期读取Tick数据
	 * 
	 * 读取指定交易日的全部Tick数据。
	 * 
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期（格式：YYYYMMDD）
	 * @return Tick数据切片对象，失败返回NULL
	 */
	virtual WTSTickSlice*	readTickSliceByDate(const char* stdCode, uint32_t uDate /* = 0 */) override;

private:
	std::string		_base_dir;				///< 数据存储根目录路径
	IBaseDataMgr*	_base_data_mgr;			///< 基础数据管理器指针
	IHotMgr*		_hot_mgr;				///< 热点合约管理器指针

	/**
	 * @struct BarsList
	 * @brief K线数据列表结构
	 * 
	 * 存储特定合约和周期的K线数据，支持时间范围管理和增量加载。
	 */
	typedef struct _BarsList
	{
		std::string		_exchg;				///< 交易所代码
		std::string		_code;				///< 合约代码
		WTSKlinePeriod	_period;			///< K线周期
		uint64_t		_last_bar_time;		///< 最后一条K线时间

		std::vector<WTSBarStruct>	_bars;	///< K线数据向量

		/**
		 * @brief 构造函数
		 * 
		 * 初始化K线列表。
		 */
		_BarsList() :_last_bar_time(0){}
	} BarsList;

	/**
	 * @struct TicksList
	 * @brief Tick数据列表结构
	 * 
	 * 存储特定合约的Tick数据，支持时间范围管理和双向扩展。
	 */
	typedef struct _TicksList
	{
		std::string		_exchg;				///< 交易所代码
		std::string		_code;				///< 合约代码
		uint64_t		_first_tick_time;	///< 第一条Tick时间
		uint64_t		_last_tick_time;	///< 最后一条Tick时间

		std::vector<WTSTickStruct>	_ticks;	///< Tick数据向量

		/**
		 * @brief 构造函数
		 * 
		 * 初始化Tick列表。
		 */
		_TicksList() :_last_tick_time(0), _first_tick_time(UINT64_MAX){}
	} TicksList;

	typedef wt_hashmap<std::string, BarsList> BarsCache;	///< K线缓存映射表类型
	BarsCache	_bars_cache;				///< K线数据缓存

	typedef wt_hashmap<std::string, TicksList> TicksCache;	///< Tick缓存映射表类型
	TicksCache	_ticks_cache;				///< Tick数据缓存

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

	WtLMDBMap	_exchg_m1_dbs;				///< 1分钟K线数据库映射表
	WtLMDBMap	_exchg_m5_dbs;				///< 5分钟K线数据库映射表
	WtLMDBMap	_exchg_d1_dbs;				///< 日K线数据库映射表

	WtLMDBMap	_tick_dbs;					///< Tick数据库映射表（key: "交易所.合约"）

	/**
	 * @brief 获取K线数据库连接
	 * 
	 * @param exchg 交易所代码
	 * @param period K线周期
	 * @return LMDB数据库智能指针
	 */
	WtLMDBPtr	get_k_db(const char* exchg, WTSKlinePeriod period);

	/**
	 * @brief 获取Tick数据库连接
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @return LMDB数据库智能指针
	 */
	WtLMDBPtr	get_t_db(const char* exchg, const char* code);
};

NS_WTP_END                                      // 结束WonderTrader命名空间
