/*!
 * \file WtDataWriter.h
 * \project WonderTrader
 * 
 * \brief WonderTrader数据写入器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataWriter类，是WonderTrader框架中用于数据写入的核心组件。
 * 该类实现了IDataWriter接口，提供了将各种数据写入WonderTrader数据存储格式的功能，
 * 支持实时数据和历史数据的写入，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 核心设计理念：
 * 
 * 1. 高效数据写入（Efficient Data Writing）：
 *    - 使用内存映射文件技术提高写入性能
 *    - 支持异步数据处理和写入
 *    - 提供数据压缩和优化存储
 * 
 * 2. 实时数据处理（Real-time Data Processing）：
 *    - 支持实时数据的快速写入
 *    - 提供数据缓存和批量写入
 *    - 支持高频数据的处理
 * 
 * 3. 数据完整性保证（Data Integrity Assurance）：
 *    - 提供数据完整性校验
 *    - 支持数据恢复和错误处理
 *    - 确保数据写入的原子性
 * 
 * 主要功能：
 * 
 * 1. 实时数据写入：
 *    - 支持实时K线数据写入
 *    - 支持实时Tick数据写入
 *    - 支持实时逐笔数据写入
 * 
 * 2. 历史数据写入：
 *    - 支持历史数据转储
 *    - 支持数据压缩和归档
 *    - 支持数据格式转换
 * 
 * 3. 数据处理功能：
 *    - 支持数据过滤和清洗
 *    - 支持数据格式转换
 *    - 支持数据统计和分析
 * 
 * 技术特点：
 * - 使用内存映射文件技术提高写入性能
 * - 支持异步数据处理和写入
 * - 提供数据压缩和优化存储
 * - 支持高频数据的处理
 * 
 * 使用场景：
 * - 实时行情数据存储
 * - 历史数据归档
 * - 数据备份和恢复
 * - 数据格式转换和迁移
 * 
 * 注意事项：
 * - 需要正确配置数据存储路径
 * - 大数据量写入时需要注意磁盘空间
 * - 实时数据写入需要考虑数据更新频率
 * - 需要确保数据写入的原子性
 */

#pragma once                                                    // 防止头文件重复包含
#include "DataDefine.h"                                         // 数据存储格式定义

#include "../Includes/FasterDefs.h"                            // 性能优化定义
#include "../Includes/IDataWriter.h"                           // 数据写入器接口
#include "../Share/StdUtils.hpp"                               // 标准工具函数
#include "../Share/BoostMappingFile.hpp"                       // Boost内存映射文件
#include "../Share/SpinMutex.hpp"                              // 自旋锁

#include <queue>                                               // 队列容器
#include <map>                                                 // 映射容器

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;         // Boost内存映射文件智能指针

NS_WTP_BEGIN                                                   // WonderTrader命名空间开始
class WTSObject;                                               // WonderTrader对象基类
class WTSContractInfo;                                         // 合约信息类
NS_WTP_END                                                     // WonderTrader命名空间结束

USING_NS_WTP;                                                  // 使用WonderTrader命名空间

/*!
 * \class WtDataWriter
 * \brief WonderTrader数据写入器
 * 
 * 该类实现了IDataWriter接口，提供了将各种数据写入WonderTrader数据存储格式的功能。
 * 支持实时数据和历史数据的写入，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 主要功能：
 * 1. 实时数据写入（K线、Tick、逐笔数据等）
 * 2. 历史数据写入（历史K线、历史Tick、历史逐笔数据等）
 * 3. 数据处理功能（过滤、清洗、转换等）
 * 4. 数据压缩和优化存储
 * 5. 异步数据处理和写入
 * 
 * 技术特点：
 * - 使用内存映射文件技术提高写入性能
 * - 支持异步数据处理和写入
 * - 提供数据压缩和优化存储
 * - 支持高频数据的处理
 * 
 * 使用场景：
 * - 实时行情数据存储
 * - 历史数据归档
 * - 数据备份和恢复
 * - 数据格式转换和迁移
 */
class WtDataWriter : public IDataWriter
{
public:
	WtDataWriter();                                             // 构造函数
	~WtDataWriter();                                            // 析构函数	

private:
	/*!
	 * \brief 调整实时数据块大小
	 * \param mfPtr 内存映射文件指针
	 * \param nCount 新的数据项数量
	 * \return 调整后的数据块指针
	 * 
	 * 该模板函数用于动态调整实时数据块的大小，支持不同类型的数据块。
	 * 当数据块空间不足时，会自动扩展数据块大小以容纳更多数据。
	 */
	template<typename HeaderType, typename T>
	void* resizeRTBlock(BoostMFPtr& mfPtr, uint32_t nCount);

	/*!
	 * \brief 数据处理循环
	 * 
	 * 该函数是数据处理的主循环，负责从任务队列中取出任务并处理。
	 * 支持异步数据处理，提高系统整体性能。
	 */
	void  proc_loop();

	/*!
	 * \brief 数据检查循环
	 * 
	 * 该函数负责定期检查数据完整性和一致性，确保数据写入的正确性。
	 * 包括数据块状态检查、文件完整性验证等。
	 */
	void  check_loop();

	/*!
	 * \brief 将K线数据转储到文件
	 * \param ct 合约信息指针
	 * \return 转储的数据条数
	 * 
	 * 该函数将内存中的K线数据转储到磁盘文件，用于历史数据归档。
	 */
	uint32_t  dump_bars_to_file(WTSContractInfo* ct);

	/*!
	 * \brief 通过转储器转储K线数据
	 * \param ct 合约信息指针
	 * \return 转储的数据条数
	 * 
	 * 该函数通过外部转储器将K线数据转储，支持多种转储格式。
	 */
	uint32_t  dump_bars_via_dumper(WTSContractInfo* ct);

private:
	/*!
	 * \brief 转储日K线数据
	 * \param ct 合约信息指针
	 * \param newBar 新的K线数据
	 * \return 是否转储成功
	 * 
	 * 该函数负责将日K线数据转储到历史数据文件中。
	 */
	bool	dump_day_data(WTSContractInfo* ct, WTSBarStruct* newBar);

	/*!
	 * \brief 处理数据块数据
	 * \param tag 数据标签
	 * \param content 数据内容
	 * \param isBar 是否为K线数据
	 * \param bKeepHead 是否保留头部信息
	 * \return 处理是否成功
	 * 
	 * 该函数负责处理各种类型的数据块，包括数据压缩、格式转换等。
	 */
	bool	proc_block_data(const char* tag, std::string& content, bool isBar, bool bKeepHead = true);

	/*!
	 * \brief 处理Tick数据
	 * \param curTick 当前Tick数据
	 * \param procFlag 处理标志
	 * 
	 * 该函数负责处理Tick数据，包括数据验证、格式转换、存储等。
	 */
	void	procTick(WTSTickData* curTick, uint32_t procFlag);
	
	/*!
	 * \brief 处理委托队列数据
	 * \param curOrdQue 当前委托队列数据
	 * 
	 * 该函数负责处理委托队列数据，包括数据验证、格式转换、存储等。
	 */
	void	procQueue(WTSOrdQueData* curOrdQue);
	
	/*!
	 * \brief 处理委托详情数据
	 * \param curOrdDetail 当前委托详情数据
	 * 
	 * 该函数负责处理委托详情数据，包括数据验证、格式转换、存储等。
	 */
	void	procOrder(WTSOrdDtlData* curOrdDetail);
	
	/*!
	 * \brief 处理逐笔成交数据
	 * \param curTrans 当前逐笔成交数据
	 * 
	 * 该函数负责处理逐笔成交数据，包括数据验证、格式转换、存储等。
	 */
	void	procTrans(WTSTransData* curTrans);

public:
	/*!
	 * \brief 初始化数据写入器
	 * \param params 配置参数
	 * \param sink 数据写入回调接口
	 * \return 是否初始化成功
	 * 
	 * 该函数负责初始化数据写入器，包括配置参数解析、资源分配、线程启动等。
	 * 是数据写入器正常工作的前提条件。
	 */
	virtual bool init(WTSVariant* params, IDataWriterSink* sink) override;
	
	/*!
	 * \brief 释放数据写入器资源
	 * 
	 * 该函数负责释放数据写入器占用的所有资源，包括内存、文件句柄、线程等。
	 * 确保程序正常退出时资源得到正确释放。
	 */
	virtual void release() override;

	/*!
	 * \brief 写入Tick数据
	 * \param curTick 当前Tick数据
	 * \param procFlag 处理标志
	 * \return 是否写入成功
	 * 
	 * 该函数负责将Tick数据写入到存储系统中，支持实时数据写入和历史数据归档。
	 * 包括数据验证、格式转换、存储等步骤。
	 */
	virtual bool writeTick(WTSTickData* curTick, uint32_t procFlag) override;

	/*!
	 * \brief 写入委托队列数据
	 * \param curOrdQue 当前委托队列数据
	 * \return 是否写入成功
	 * 
	 * 该函数负责将委托队列数据写入到存储系统中，用于记录市场深度信息。
	 */
	virtual bool writeOrderQueue(WTSOrdQueData* curOrdQue) override;

	/*!
	 * \brief 写入委托详情数据
	 * \param curOrdDetail 当前委托详情数据
	 * \return 是否写入成功
	 * 
	 * 该函数负责将委托详情数据写入到存储系统中，用于记录委托的详细信息。
	 */
	virtual bool writeOrderDetail(WTSOrdDtlData* curOrdDetail) override;

	/*!
	 * \brief 写入逐笔成交数据
	 * \param curTrans 当前逐笔成交数据
	 * \return 是否写入成功
	 * 
	 * 该函数负责将逐笔成交数据写入到存储系统中，用于记录每笔交易的详细信息。
	 */
	virtual bool writeTransaction(WTSTransData* curTrans) override;

	/*!
	 * \brief 转储历史数据
	 * \param sid 合约标识
	 * 
	 * 该函数负责将实时数据转储为历史数据，用于数据归档和备份。
	 */
	virtual void transHisData(const char* sid) override;
	
	/*!
	 * \brief 检查交易时段是否已处理
	 * \param sid 合约标识
	 * \return 是否已处理
	 * 
	 * 该函数用于检查指定合约的交易时段是否已经处理完成，避免重复处理。
	 */
	virtual bool isSessionProceeded(const char* sid) override;

	/*!
	 * \brief 获取当前Tick数据
	 * \param code 合约代码
	 * \param exchg 交易所代码
	 * \return 当前Tick数据指针
	 * 
	 * 该函数用于获取指定合约的当前最新Tick数据，用于实时行情查询。
	 */
	virtual WTSTickData* getCurTick(const char* code, const char* exchg = "") override;

private:
	IBaseDataMgr*		_bd_mgr;                                // 基础数据管理器指针

	/*!
	 * \struct _KBlockPair
	 * \brief K线数据块对结构
	 * 
	 * 该结构用于管理K线数据块，包含数据块指针、内存映射文件、互斥锁和最后更新时间。
	 * 用于实时K线数据的写入和管理。
	 */
	typedef struct _KBlockPair
	{
		RTKlineBlock*	_block;                                 // 实时K线数据块指针
		BoostMFPtr		_file;                                  // 内存映射文件指针
		SpinMutex		_mutex;                                 // 自旋锁，用于线程同步
		uint64_t		_lasttime;                              // 最后更新时间戳

		_KBlockPair()
		{
			_block = NULL;                                      // 初始化数据块指针为空
			_file = NULL;                                       // 初始化文件指针为空
			_lasttime = 0;                                      // 初始化时间为0
		}

	} KBlockPair;
	typedef wt_hashmap<std::string, KBlockPair*>	KBlockFilesMap;  // K线数据块文件映射表

	/*!
	 * \struct _TickBlockPair
	 * \brief Tick数据块对结构
	 * 
	 * 该结构用于管理Tick数据块，包含数据块指针、内存映射文件、互斥锁、最后更新时间和文件流。
	 * 用于实时Tick数据的写入和管理。
	 */
	typedef struct _TickBlockPair
	{
		RTTickBlock*	_block;                                 // 实时Tick数据块指针
		BoostMFPtr		_file;                                  // 内存映射文件指针
		SpinMutex		_mutex;                                 // 自旋锁，用于线程同步
		uint64_t		_lasttime;                              // 最后更新时间戳

		std::shared_ptr< std::ofstream>	_fstream;              // 文件流指针，用于数据写入

		_TickBlockPair()
		{
			_block = NULL;                                      // 初始化数据块指针为空
			_file = NULL;                                       // 初始化文件指针为空
			_fstream = NULL;                                    // 初始化文件流指针为空
			_lasttime = 0;                                      // 初始化时间为0
		}
	} TickBlockPair;
	typedef wt_hashmap<std::string, TickBlockPair*>	TickBlockFilesMap;  // Tick数据块文件映射表

	/*!
	 * \struct _TransBlockPair
	 * \brief 逐笔成交数据块对结构
	 * 
	 * 该结构用于管理逐笔成交数据块，包含数据块指针、内存映射文件、互斥锁和最后更新时间。
	 * 用于实时逐笔成交数据的写入和管理。
	 */
	typedef struct _TransBlockPair
	{
		RTTransBlock*	_block;                                 // 实时逐笔成交数据块指针
		BoostMFPtr		_file;                                  // 内存映射文件指针
		SpinMutex		_mutex;                                 // 自旋锁，用于线程同步
		uint64_t		_lasttime;                              // 最后更新时间戳

		_TransBlockPair()
		{
			_block = NULL;                                      // 初始化数据块指针为空
			_file = NULL;                                       // 初始化文件指针为空
			_lasttime = 0;                                      // 初始化时间为0
		}
	} TransBlockPair;
	typedef wt_hashmap<std::string, TransBlockPair*>	TransBlockFilesMap;  // 逐笔成交数据块文件映射表

	/*!
	 * \struct _OdeDtlBlockPair
	 * \brief 委托详情数据块对结构
	 * 
	 * 该结构用于管理委托详情数据块，包含数据块指针、内存映射文件、互斥锁和最后更新时间。
	 * 用于实时委托详情数据的写入和管理。
	 */
	typedef struct _OdeDtlBlockPair
	{
		RTOrdDtlBlock*	_block;                                 // 实时委托详情数据块指针
		BoostMFPtr		_file;                                  // 内存映射文件指针
		SpinMutex		_mutex;                                 // 自旋锁，用于线程同步
		uint64_t		_lasttime;                              // 最后更新时间戳

		_OdeDtlBlockPair()
		{
			_block = NULL;                                      // 初始化数据块指针为空
			_file = NULL;                                       // 初始化文件指针为空
			_lasttime = 0;                                      // 初始化时间为0
		}
	} OrdDtlBlockPair;
	typedef wt_hashmap<std::string, OrdDtlBlockPair*>	OrdDtlBlockFilesMap;  // 委托详情数据块文件映射表

	/*!
	 * \struct _OdeQueBlockPair
	 * \brief 委托队列数据块对结构
	 * 
	 * 该结构用于管理委托队列数据块，包含数据块指针、内存映射文件、互斥锁和最后更新时间。
	 * 用于实时委托队列数据的写入和管理。
	 */
	typedef struct _OdeQueBlockPair
	{
		RTOrdQueBlock*	_block;                                 // 实时委托队列数据块指针
		BoostMFPtr		_file;                                  // 内存映射文件指针
		SpinMutex		_mutex;                                 // 自旋锁，用于线程同步
		uint64_t		_lasttime;                              // 最后更新时间戳

		_OdeQueBlockPair()
		{
			_block = NULL;                                      // 初始化数据块指针为空
			_file = NULL;                                       // 初始化文件指针为空
			_lasttime = 0;                                      // 初始化时间为0
		}
	} OrdQueBlockPair;
	typedef wt_hashmap<std::string, OrdQueBlockPair*>	OrdQueBlockFilesMap;  // 委托队列数据块文件映射表
	

	KBlockFilesMap	_rt_min1_blocks;                            // 实时1分钟K线数据块映射表
	KBlockFilesMap	_rt_min5_blocks;                            // 实时5分钟K线数据块映射表

	TickBlockFilesMap	_rt_ticks_blocks;                      // 实时Tick数据块映射表
	TransBlockFilesMap	_rt_trans_blocks;                      // 实时逐笔成交数据块映射表
	OrdDtlBlockFilesMap _rt_orddtl_blocks;                     // 实时委托详情数据块映射表
	OrdQueBlockFilesMap _rt_ordque_blocks;                     // 实时委托队列数据块映射表

	SpinMutex		_lck_tick_cache;                          // Tick缓存锁
	wt_hashmap<std::string, uint32_t> _tick_cache_idx;        // Tick缓存索引映射表
	BoostMFPtr		_tick_cache_file;                          // Tick缓存文件指针
	RTTickCache*	_tick_cache_block;                         // Tick缓存数据块指针

	//typedef std::function<void()> TaskInfo;
	/*!
	 * \struct _TaskInfo
	 * \brief 任务信息结构
	 * 
	 * 该结构用于管理异步任务信息，包含任务对象、类型、标志等。
	 * 使用64字节对齐优化缓存性能，提高任务处理效率。
	 */
	typedef struct alignas(64) _TaskInfo
	{
		WTSObject*	_obj;                                       // 任务对象指针
		uint64_t	_type;                                      // 任务类型
		uint32_t	_flag;                                      // 任务标志

		/*!
		 * \brief 构造函数
		 * \param data 任务对象
		 * \param dtype 任务类型
		 * \param flag 任务标志
		 */
		_TaskInfo(WTSObject* data, uint64_t dtype, uint32_t flag = 0);

		/*!
		 * \brief 拷贝构造函数
		 * \param rhs 源任务信息
		 */
		_TaskInfo(const _TaskInfo& rhs);

		/*!
		 * \brief 析构函数
		 */
		~_TaskInfo();

	} TaskInfo;
	std::queue<TaskInfo>	_tasks;                             // 任务队列
	StdThreadPtr			_task_thrd;                         // 任务处理线程指针
	StdUniqueMutex			_task_mtx;                         // 任务队列互斥锁
	StdCondVariable			_task_cond;                        // 任务队列条件变量

	std::string		_base_dir;                                 // 基础数据目录路径
	std::string		_cache_file;                               // 缓存文件路径
	uint32_t		_log_group_size;                           // 日志组大小
	bool			_async_proc;                               // 是否启用异步处理

	StdCondVariable	_proc_cond;                                // 处理条件变量
	StdUniqueMutex	_proc_mtx;                                 // 处理互斥锁
	std::queue<std::string> _proc_que;                          // 处理队列
	StdThreadPtr	_proc_thrd;                                // 处理线程指针
	StdThreadPtr	_proc_chk;                                 // 检查线程指针
	bool			_terminated;                               // 是否已终止

	bool			_save_tick_log;                            // 是否保存Tick日志
	bool			_skip_notrade_tick;                        // 是否跳过无交易Tick
	bool			_skip_notrade_bar;                         // 是否跳过无交易K线
	bool			_disable_his;                              // 是否禁用历史数据

	bool			_disable_tick;                             // 是否禁用Tick数据
	bool			_disable_min1;                             // 是否禁用1分钟K线
	bool			_disable_min5;                             // 是否禁用5分钟K线
	bool			_disable_day;                              // 是否禁用日K线

	bool			_disable_trans;                            // 是否禁用逐笔成交数据
	bool			_disable_ordque;                           // 是否禁用委托队列数据
	bool			_disable_orddtl;                           // 是否禁用委托详情数据

	/*
	 *	by Wesley @ 2023.05.04
	 *	分钟线价格模式，0-常规模式，1-将买卖价也记录下来，这个设计时只针对期权这种不活跃的品种
	 */
	uint32_t		_min_price_mode;                           // 分钟线价格模式
	
	std::map<std::string, uint32_t> _proc_date;                // 处理日期映射表

private:
	/*!
	 * \brief 加载缓存数据
	 * 
	 * 该函数负责从磁盘加载缓存数据到内存中，用于系统启动时的数据恢复。
	 */
	void loadCache();

	/*!
	 * \brief 更新缓存数据
	 * \param ct 合约信息
	 * \param curTick 当前Tick数据
	 * \param procFlag 处理标志
	 * \return 是否更新成功
	 * 
	 * 该函数负责更新Tick缓存数据，包括数据验证、格式转换、存储等。
	 */
	bool updateCache(WTSContractInfo* ct, WTSTickData* curTick, uint32_t procFlag);

	/*!
	 * \brief 将数据传递给Tick处理
	 * \param ct 合约信息
	 * \param curTick 当前Tick数据
	 * 
	 * 该函数负责将Tick数据传递给Tick处理模块，用于实时数据处理。
	 */
	void pipeToTicks(WTSContractInfo* ct, WTSTickData* curTick);

	/*!
	 * \brief 将数据传递给K线处理
	 * \param ct 合约信息
	 * \param curTick 当前Tick数据
	 * 
	 * 该函数负责将Tick数据传递给K线处理模块，用于K线数据生成。
	 */
	void pipeToKlines(WTSContractInfo* ct, WTSTickData* curTick);

	/*!
	 * \brief 获取K线数据块
	 * \param ct 合约信息
	 * \param period K线周期
	 * \param bAutoCreate 是否自动创建
	 * \return K线数据块指针
	 * 
	 * 该函数负责获取或创建指定周期的K线数据块，用于K线数据存储。
	 */
	KBlockPair* getKlineBlock(WTSContractInfo* ct, WTSKlinePeriod period, bool bAutoCreate = true);

	/*!
	 * \brief 获取Tick数据块
	 * \param ct 合约信息
	 * \param curDate 当前日期
	 * \param bAutoCreate 是否自动创建
	 * \return Tick数据块指针
	 * 
	 * 该函数负责获取或创建指定日期的Tick数据块，用于Tick数据存储。
	 */
	TickBlockPair* getTickBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);
	
	/*!
	 * \brief 获取逐笔成交数据块
	 * \param ct 合约信息
	 * \param curDate 当前日期
	 * \param bAutoCreate 是否自动创建
	 * \return 逐笔成交数据块指针
	 * 
	 * 该函数负责获取或创建指定日期的逐笔成交数据块，用于逐笔成交数据存储。
	 */
	TransBlockPair* getTransBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);
	
	/*!
	 * \brief 获取委托详情数据块
	 * \param ct 合约信息
	 * \param curDate 当前日期
	 * \param bAutoCreate 是否自动创建
	 * \return 委托详情数据块指针
	 * 
	 * 该函数负责获取或创建指定日期的委托详情数据块，用于委托详情数据存储。
	 */
	OrdDtlBlockPair* getOrdDtlBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);
	
	/*!
	 * \brief 获取委托队列数据块
	 * \param ct 合约信息
	 * \param curDate 当前日期
	 * \param bAutoCreate 是否自动创建
	 * \return 委托队列数据块指针
	 * 
	 * 该函数负责获取或创建指定日期的委托队列数据块，用于委托队列数据存储。
	 */
	OrdQueBlockPair* getOrdQueBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);

	/*!
	 * \brief 释放数据块
	 * \param block 数据块指针
	 * 
	 * 该模板函数负责释放指定类型的数据块，确保内存得到正确释放。
	 */
	template<typename T>
	void	releaseBlock(T* block);

	/*!
	 * \brief 推送任务到队列
	 * \param task 任务信息
	 * 
	 * 该函数负责将任务推送到任务队列中，用于异步任务处理。
	 */
	void pushTask(const TaskInfo& task);
};

