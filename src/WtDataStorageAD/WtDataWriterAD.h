/*!
 * \file WtDataWriterAD.h
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块数据写入器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataWriterAD类，这是WonderTrader框架中的高级数据写入器。
 * 该类基于LMDB数据库和内存映射缓存技术，为数据落地系统提供高性能的实时
 * 数据写入服务，支持Tick数据的接收、处理、缓存和持久化存储。
 * 
 * 核心设计理念：
 * 
 * 1. 异步写入架构（Asynchronous Writing Architecture）：
 *    - 主线程快速接收数据，避免阻塞行情处理
 *    - 后台线程异步处理数据写入和K线合成
 *    - 任务队列缓冲，平滑处理突发数据流
 * 
 * 2. 多级存储设计（Multi-Level Storage Design）：
 *    - L1缓存：内存映射的实时缓存文件
 *    - L2存储：LMDB持久化数据库
 *    - 实时缓存提供快速访问，LMDB提供可靠存储
 * 
 * 3. 实时K线合成（Real-time Bar Synthesis）：
 *    - 从Tick数据实时合成1分钟、5分钟、日K线
 *    - 支持多周期K线的并行处理
 *    - 智能处理交易时段和跨日切换
 * 
 * 数据处理流程：
 * 
 *   行情数据输入
 *       ↓
 *   writeTick() [主线程]
 *       ↓
 *   任务队列 [异步缓冲]
 *       ↓
 *   后台处理线程
 *       ↓
 *   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
 *   │  Tick缓存更新   │ → │  K线实时合成    │ → │  数据持久化     │
 *   │  updateTickCache│    │  updateBarCache │    │  pipeToXXX      │
 *   └─────────────────┘    └─────────────────┘    └─────────────────┘
 *           ↓                       ↓                       ↓
 *   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
 *   │  内存映射缓存   │    │  实时K线缓存    │    │  LMDB数据库     │
 *   │  RTTickCache    │    │  RTBarCache     │    │  Persistent     │
 *   └─────────────────┘    └─────────────────┘    └─────────────────┘
 * 
 * 性能优化特点：
 * 
 * 1. 零拷贝技术（Zero-Copy Technology）：
 *    - 内存映射文件直接访问
 *    - 避免不必要的数据拷贝
 *    - 提高数据处理效率
 * 
 * 2. 批量处理（Batch Processing）：
 *    - 任务队列批量处理
 *    - 减少系统调用开销
 *    - 提高整体吞吐量
 * 
 * 3. 智能缓存管理（Smart Cache Management）：
 *    - 动态缓存扩容
 *    - 索引优化查找
 *    - 内存使用优化
 */

#pragma once                                    // 防止头文件重复包含
#include "DataDefineAD.h"                       // 引入数据结构定义

#include "../WTSUtils/WtLMDB.hpp"               // 引入LMDB数据库封装

#include "../Includes/FasterDefs.h"             // 引入高性能数据结构
#include "../Includes/IDataWriter.h"            // 引入数据写入器接口
#include "../Share/StdUtils.hpp"                // 引入标准工具类
#include "../Share/BoostMappingFile.hpp"        // 引入内存映射文件类

#include <queue>                                // 引入队列容器

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;  ///< 内存映射文件智能指针类型

NS_WTP_BEGIN                                    // 开始WonderTrader命名空间
class WTSContractInfo;                          // 前向声明合约信息类
NS_WTP_END                                      // 结束WonderTrader命名空间

USING_NS_WTP;                                   // 使用WonderTrader命名空间

/**
 * @class WtDataWriterAD
 * @brief WonderTrader高级数据写入器
 * 
 * 基于LMDB数据库和内存映射缓存的高性能数据写入器，为数据落地系统提供
 * 实时数据接收、处理、缓存和持久化存储服务。
 * 
 * 主要特性：
 * 
 * 1. 异步处理架构：
 *    - 主线程快速接收数据
 *    - 后台线程异步处理写入
 *    - 任务队列平滑数据流
 * 
 * 2. 多级存储系统：
 *    - 内存映射实时缓存
 *    - LMDB持久化存储
 *    - 智能缓存管理
 * 
 * 3. 实时K线合成：
 *    - 多周期K线并行处理
 *    - 交易时段智能处理
 *    - 跨日数据平滑切换
 * 
 * 典型使用场景：
 * @code
 *   // 1. 创建并初始化写入器
 *   WtDataWriterAD* writer = new WtDataWriterAD();
 *   writer->init(config, sink);
 *   
 *   // 2. 写入Tick数据
 *   writer->writeTick(tickData, 0);
 *   
 *   // 3. 获取当前Tick
 *   WTSTickData* curTick = writer->getCurTick("rb2305", "SHFE");
 *   
 *   // 4. 释放资源
 *   writer->release();
 * @endcode
 */
class WtDataWriterAD : public IDataWriter
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建WtDataWriterAD实例，初始化基本成员变量。
	 */
	WtDataWriterAD();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，停止后台线程，关闭数据库连接。
	 */
	~WtDataWriterAD();	

private:
	/**
	 * @brief 调整实时缓存块大小
	 * 
	 * 模板函数，用于动态调整内存映射文件的大小以容纳更多数据项。
	 * 
	 * @tparam HeaderType 缓存块头部类型
	 * @tparam T 数据项类型
	 * @param mfPtr 内存映射文件智能指针引用
	 * @param nCount 新的容量大小
	 * @return 调整后的内存地址，失败返回NULL
	 */
	template<typename HeaderType, typename T>
	void* resizeRTBlock(BoostMFPtr& mfPtr, uint32_t nCount);

public:
	/**
	 * @brief 初始化数据写入器
	 * 
	 * 根据配置参数初始化数据写入器，设置存储路径、缓存参数等。
	 * 
	 * @param params 配置参数对象，包含：
	 *               - path: 数据存储根目录
	 *               - groupsize: 日志分组大小
	 *               - disabletick: 是否禁用Tick写入
	 *               - disablemin1: 是否禁用1分钟K线
	 *               - disablemin5: 是否禁用5分钟K线
	 *               - disableday: 是否禁用日K线
	 * @param sink 回调接口，提供基础数据管理器等服务
	 * @return 初始化成功返回true，失败返回false
	 */
	virtual bool init(WTSVariant* params, IDataWriterSink* sink) override;
	
	/**
	 * @brief 释放资源
	 * 
	 * 停止后台处理线程，关闭所有数据库连接，清理内存资源。
	 */
	virtual void release() override;

	/**
	 * @brief 写入Tick数据
	 * 
	 * 接收Tick数据并异步处理，包括缓存更新、K线合成和数据持久化。
	 * 
	 * @param curTick Tick数据对象
	 * @param procFlag 处理标志：
	 *                 - 0: 直接使用原始数据
	 *                 - 1: 计算增量数据（成交量、成交额等）
	 *                 - 2: 自动累加模式
	 * @return 处理成功返回true，失败返回false
	 */
	virtual bool writeTick(WTSTickData* curTick, uint32_t procFlag) override;

	/**
	 * @brief 获取当前Tick数据
	 * 
	 * 从缓存中获取指定合约的最新Tick数据。
	 * 
	 * @param code 合约代码
	 * @param exchg 交易所代码（可选）
	 * @return Tick数据对象，失败返回NULL
	 */
	virtual WTSTickData* getCurTick(const char* code, const char* exchg = "") override;

private:
	IBaseDataMgr*		_bd_mgr;			///< 基础数据管理器指针

	//////////////////////////////////////////////////////////////////////////
	// Tick数据缓存管理（Tick Data Cache Management）
	
	StdUniqueMutex	_mtx_tick_cache;		///< Tick缓存互斥锁
	std::string		_cache_file_tick;		///< Tick缓存文件名
	wt_hashmap<std::string, uint32_t> _tick_cache_idx;	///< Tick缓存索引映射表
	BoostMFPtr		_tick_cache_file;		///< Tick缓存内存映射文件指针
	RTTickCache*	_tick_cache_block;		///< Tick缓存数据块指针

	//////////////////////////////////////////////////////////////////////////
	// K线缓存管理（Bar Cache Management）
	
	/**
	 * @struct RTBarCacheWrapper
	 * @brief 实时K线缓存包装器
	 * 
	 * 封装内存映射文件的K线缓存，提供线程安全的访问接口。
	 */
	typedef struct _RTBarCacheWrapper
	{
		StdUniqueMutex	_mtx;				///< 互斥锁，保证线程安全
		std::string		_filename;			///< 缓存文件名
		wt_hashmap<std::string, uint32_t> _idx;	///< 合约索引映射表
		BoostMFPtr		_file_ptr;			///< 内存映射文件指针
		RTBarCache*		_cache_block;		///< 缓存数据块指针

		/**
		 * @brief 构造函数
		 * 
		 * 初始化缓存包装器。
		 */
		_RTBarCacheWrapper():_cache_block(NULL),_file_ptr(NULL){}

		/**
		 * @brief 检查缓存是否为空
		 * 
		 * @return 缓存为空返回true，否则返回false
		 */
		inline bool empty() const { return _cache_block == NULL; }
	} RTBarCacheWrapper;

	RTBarCacheWrapper _m1_cache;			///< 1分钟K线缓存
	RTBarCacheWrapper _m5_cache;			///< 5分钟K线缓存
	RTBarCacheWrapper _d1_cache;			///< 日K线缓存

	//////////////////////////////////////////////////////////////////////////
	// 异步任务处理（Asynchronous Task Processing）
	
	typedef std::function<void()> TaskInfo;	///< 任务信息类型定义
	std::queue<TaskInfo>	_tasks;			///< 任务队列
	StdThreadPtr			_task_thrd;		///< 后台处理线程
	StdUniqueMutex			_task_mtx;		///< 任务队列互斥锁
	StdCondVariable			_task_cond;		///< 任务队列条件变量
	bool					_async_task;	///< 是否启用异步任务处理

	//////////////////////////////////////////////////////////////////////////
	// 配置参数（Configuration Parameters）
	
	std::string		_base_dir;				///< 数据存储根目录
	uint32_t		_log_group_size;		///< 日志分组大小

	bool			_terminated;			///< 是否已终止标志

	bool			_disable_tick;			///< 是否禁用Tick数据写入
	bool			_disable_min1;			///< 是否禁用1分钟K线写入
	bool			_disable_min5;			///< 是否禁用5分钟K线写入
	bool			_disable_day;			///< 是否禁用日K线写入

	uint32_t		_tick_mapsize;			///< Tick数据库映射大小
	uint32_t		_kline_mapsize;			///< K线数据库映射大小

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

private:
	//////////////////////////////////////////////////////////////////////////
	// 私有辅助方法（Private Helper Methods）
	
	/**
	 * @brief 加载缓存文件
	 * 
	 * 初始化所有内存映射缓存文件，包括Tick缓存和各周期K线缓存。
	 */
	void loadCache();

	/**
	 * @brief 更新Tick缓存
	 * 
	 * 将新的Tick数据更新到内存映射缓存中，处理数据预处理逻辑。
	 * 
	 * @param ct 合约信息
	 * @param curTick 当前Tick数据
	 * @param procFlag 处理标志
	 * @return 更新成功返回true，失败返回false
	 */
	bool updateTickCache(WTSContractInfo* ct, WTSTickData* curTick, uint32_t procFlag);

	/**
	 * @brief 更新K线缓存
	 * 
	 * 根据Tick数据实时合成并更新各周期K线缓存。
	 * 
	 * @param ct 合约信息
	 * @param curTick 当前Tick数据
	 */
	void updateBarCache(WTSContractInfo* ct, WTSTickData* curTick);

	/**
	 * @brief 将Tick数据写入持久化存储
	 * 
	 * 将Tick数据写入LMDB数据库并通知扩展转储器。
	 * 
	 * @param ct 合约信息
	 * @param curTick 当前Tick数据
	 */
	void pipeToTicks(WTSContractInfo* ct, WTSTickData* curTick);

	/**
	 * @brief 将日K线数据写入持久化存储
	 * 
	 * 将完整的日K线数据写入LMDB数据库。
	 * 
	 * @param ct 合约信息
	 * @param bar K线数据结构
	 */
	void pipeToDayBars(WTSContractInfo* ct, const WTSBarStruct& bar);

	/**
	 * @brief 将1分钟K线数据写入持久化存储
	 * 
	 * 将完整的1分钟K线数据写入LMDB数据库。
	 * 
	 * @param ct 合约信息
	 * @param bar K线数据结构
	 */
	void pipeToM1Bars(WTSContractInfo* ct, const WTSBarStruct& bar);

	/**
	 * @brief 将5分钟K线数据写入持久化存储
	 * 
	 * 将完整的5分钟K线数据写入LMDB数据库。
	 * 
	 * @param ct 合约信息
	 * @param bar K线数据结构
	 */
	void pipeToM5Bars(WTSContractInfo* ct, const WTSBarStruct& bar);

	/**
	 * @brief 推送异步任务
	 * 
	 * 将任务添加到异步处理队列中，由后台线程执行。
	 * 
	 * @param task 任务函数对象
	 */
	void pushTask(TaskInfo task);
};

