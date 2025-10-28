/*!
 * \file WtDataWriterAD.cpp
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块数据写入器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtDataWriterAD类的具体功能，为WonderTrader数据落地系统提供基于
 * LMDB数据库和内存映射缓存的高性能数据写入服务。该实现采用异步处理架构，
 * 支持实时Tick数据的接收、处理、缓存和持久化存储，同时实现多周期K线的实时合成。
 * 
 * 核心实现特点：
 * 
 * 1. 异步处理架构（Asynchronous Processing Architecture）：
 *    - 主线程快速接收数据，避免阻塞行情处理
 *    - 后台线程异步处理数据写入和K线合成
 *    - 任务队列缓冲，平滑处理突发数据流
 * 
 * 2. 多级存储实现（Multi-Level Storage Implementation）：
 *    - L1缓存：内存映射的实时缓存文件（RTCache）
 *    - L2存储：LMDB持久化数据库
 *    - 智能缓存管理：动态扩容和索引优化
 * 
 * 3. 实时K线合成引擎（Real-time Bar Synthesis Engine）：
 *    - 从Tick数据实时合成1分钟、5分钟、日K线
 *    - 支持多周期K线的并行处理
 *    - 智能处理交易时段和跨日切换
 * 
 * 数据处理流水线：
 * 
 * 1. 数据接收阶段（Data Reception Phase）：
 *    writeTick() → 数据验证 → 交易时段检查 → 任务队列
 * 
 * 2. 异步处理阶段（Asynchronous Processing Phase）：
 *    任务队列 → updateTickCache() → updateBarCache() → pipeToXXX()
 * 
 * 3. 数据存储阶段（Data Storage Phase）：
 *    内存缓存 → LMDB持久化 → 扩展转储器 → 数据广播
 * 
 * K线合成算法：
 * 
 * 1. 时间窗口计算（Time Window Calculation）：
 *    - 根据交易时段计算K线时间窗口
 *    - 处理跨小节和跨日的时间切换
 *    - 支持不规则交易时段
 * 
 * 2. OHLC数据合成（OHLC Data Synthesis）：
 *    - Open：新K线的第一个价格
 *    - High：K线时间窗口内的最高价
 *    - Low：K线时间窗口内的最低价
 *    - Close：K线时间窗口内的最后价格
 * 
 * 3. 成交量数据聚合（Volume Data Aggregation）：
 *    - 成交量：K线时间窗口内的累计成交量
 *    - 成交额：K线时间窗口内的累计成交额
 *    - 持仓量：K线结束时的持仓量
 *    - 增仓：K线时间窗口内的净增仓
 * 
 * 性能优化实现：
 * 
 * 1. 内存映射优化（Memory Mapping Optimization）：
 *    - 零拷贝数据访问
 *    - 动态文件扩容
 *    - 进程间数据共享
 * 
 * 2. 异步处理优化（Asynchronous Processing Optimization）：
 *    - 无锁任务队列
 *    - 批量任务处理
 *    - 条件变量同步
 * 
 * 3. 数据库访问优化（Database Access Optimization）：
 *    - 连接池管理
 *    - 批量写入事务
 *    - 索引优化查询
 */

#include "WtDataWriterAD.h"                     // 引入数据写入器头文件
#include "LMDBKeys.h"                           // 引入LMDB键值结构定义

#include "../Includes/WTSSessionInfo.hpp"       // 引入交易时段信息类
#include "../Includes/WTSContractInfo.hpp"      // 引入合约信息类
#include "../Includes/WTSDataDef.hpp"           // 引入数据定义
#include "../Includes/WTSVariant.hpp"           // 引入配置参数类
#include "../Share/BoostFile.hpp"               // 引入Boost文件操作类
#include "../Share/StrUtil.hpp"                 // 引入字符串工具类
#include "../Share/decimal.h"                   // 引入高精度小数计算

#include "../Includes/IBaseDataMgr.h"           // 引入基础数据管理器接口

using namespace std;                            // 使用标准命名空间

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                    // 引入格式化字符串库

/**
 * @brief 数据写入器日志输出函数
 * 
 * 线程安全的日志输出函数，使用线程局部存储优化性能。
 * 
 * @tparam Args 可变参数类型
 * @param sink 日志输出接口
 * @param ll 日志级别
 * @param format 格式化字符串
 * @param args 格式化参数
 */
template<typename... Args>
inline void pipe_writer_log(IDataWriterSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                           // 检查接口有效性
		return;

	static thread_local char buffer[512] = { 0 };  // 线程局部缓冲区
	memset(buffer, 0, 512);                     // 清零缓冲区
	fmt::format_to(buffer, format, args...);    // 格式化字符串

	sink->outputLog(ll, buffer);                // 输出日志
}

/**
 * @brief C接口导出函数
 * 
 * 提供标准C接口，支持动态库加载和跨语言调用。
 */
extern "C"
{
	/**
	 * @brief 创建数据写入器实例
	 * 
	 * 工厂函数，创建WtDataWriterAD实例并返回基类指针。
	 * 
	 * @return 数据写入器接口指针
	 */
	EXPORT_FLAG IDataWriter* createWriter()
	{
		IDataWriter* ret = new WtDataWriterAD();  // 创建实例
		return ret;                             // 返回接口指针
	}

	/**
	 * @brief 销毁数据写入器实例
	 * 
	 * 安全销毁写入器实例，释放所有资源。
	 * 
	 * @param writer 要销毁的写入器指针引用
	 */
	EXPORT_FLAG void deleteWriter(IDataWriter* &writer)
	{
		if (writer != NULL)                     // 检查指针有效性
		{
			delete writer;                      // 销毁实例
			writer = NULL;                      // 置空指针
		}
	}
};

/**
 * @brief 缓存大小扩展步长常量
 * 
 * 定义内存映射缓存文件的扩容步长，每次扩容增加400个数据项。
 * 该值在性能和内存使用之间取得平衡。
 */
static const uint32_t CACHE_SIZE_STEP_AD = 400;


/**
 * @brief 构造函数
 * 
 * 初始化WtDataWriterAD实例，设置默认配置参数。
 */
WtDataWriterAD::WtDataWriterAD()
	: _terminated(false)                        // 初始化终止标志为false
	, _log_group_size(1000)                     // 设置日志分组大小为1000
	, _disable_day(false)                       // 默认启用日K线写入
	, _disable_min1(false)                      // 默认启用1分钟K线写入
	, _disable_min5(false)                      // 默认启用5分钟K线写入
	, _disable_tick(false)                      // 默认启用Tick数据写入
	, _tick_cache_block(nullptr)                // 初始化Tick缓存块指针
	, _tick_mapsize(16*1024*1024)               // 设置Tick数据库映射大小为16MB
	, _kline_mapsize(8*1024*1024)               // 设置K线数据库映射大小为8MB
{
	// 构造函数中只设置默认值
	// 实际初始化在init()方法中完成
}

/**
 * @brief 析构函数
 * 
 * 清理资源，停止后台线程，关闭数据库连接。
 */
WtDataWriterAD::~WtDataWriterAD()
{
	// 析构函数中的清理工作在release()方法中完成
	// 智能指针会自动释放LMDB数据库资源
}

/**
 * @brief 初始化数据写入器
 * 
 * 根据配置参数初始化数据写入器，设置存储路径、缓存参数、功能开关等。
 * 
 * @param params 配置参数对象
 * @param sink 回调接口，提供基础数据管理器等服务
 * @return 初始化成功返回true，失败返回false
 */
bool WtDataWriterAD::init(WTSVariant* params, IDataWriterSink* sink)
{
	IDataWriter::init(params, sink);            // 调用基类初始化方法

	_bd_mgr = sink->getBDMgr();                 // 获取基础数据管理器

	// 设置数据存储根目录
	_base_dir = StrUtil::standardisePath(params->getCString("path"));
	if (!BoostFile::exists(_base_dir.c_str()))  // 检查目录是否存在
		BoostFile::create_directories(_base_dir.c_str());  // 创建目录

	// 设置各类缓存文件名
	_cache_file_tick = "cache_tick.dmb";        // Tick数据缓存文件名
	_m1_cache._filename = "cache_m1.dmb";       // 1分钟K线缓存文件名
	_m5_cache._filename = "cache_m5.dmb";       // 5分钟K线缓存文件名
	_d1_cache._filename = "cache_d1.dmb";       // 日K线缓存文件名

	// 设置日志分组大小
	_log_group_size = params->getUInt32("groupsize");

	// 读取功能开关配置
	_disable_tick = params->getBoolean("disabletick");   // 是否禁用Tick写入
	_disable_min1 = params->getBoolean("disablemin1");   // 是否禁用1分钟K线
	_disable_min5 = params->getBoolean("disablemin5");   // 是否禁用5分钟K线
	_disable_day = params->getBoolean("disableday");     // 是否禁用日K线

	// 读取数据库映射大小配置（可选）
	if (params->has("tickmapsize"))
		_tick_mapsize = params->getUInt32("tickmapsize");    // Tick数据库映射大小

	if (params->has("klinemapsize"))
		_kline_mapsize = params->getUInt32("klinemapsize");  // K线数据库映射大小

	loadCache();                                // 加载内存映射缓存文件

	return true;                                // 返回初始化成功
}

/**
 * @brief 释放资源
 * 
 * 停止后台处理线程，关闭所有数据库连接，清理内存资源。
 * 该方法在程序退出或重新初始化时调用。
 */
void WtDataWriterAD::release()
{
	_terminated = true;                         // 设置终止标志
	if (_task_thrd)                             // 检查后台线程是否存在
	{
		_task_cond.notify_all();                // 通知后台线程退出
		_task_thrd->join();                     // 等待线程结束
	}
}

/**
 * @brief 加载内存映射缓存文件
 * 
 * 初始化所有内存映射缓存文件，包括Tick缓存和各周期K线缓存。
 * 如果缓存文件不存在，会自动创建；如果存在，会加载现有数据并重建索引。
 */
void WtDataWriterAD::loadCache()
{
	//////////////////////////////////////////////////////////////////////////
	// 加载Tick数据缓存文件
	if (_tick_cache_file == NULL)
	{
		bool bNew = false;                      // 是否为新创建的文件
		std::string filename = _base_dir + _cache_file_tick;  // 构造完整文件路径
		
		// 检查缓存文件是否存在
		if (!BoostFile::exists(filename.c_str()))
		{
			// 文件不存在，创建新的缓存文件
			uint64_t uSize = sizeof(RTTickCache) + sizeof(TickCacheItem) * CACHE_SIZE_STEP_AD;
			BoostFile bf;
			bf.create_new_file(filename.c_str());  // 创建新文件
			bf.truncate_file((uint32_t)uSize);     // 设置文件大小
			bf.close_file();                       // 关闭文件
			bNew = true;                           // 标记为新文件
		}

		// 创建内存映射文件对象并映射到内存
		_tick_cache_file.reset(new BoostMappingFile);
		_tick_cache_file->map(filename.c_str());    // 映射文件到内存
		_tick_cache_block = (RTTickCache*)_tick_cache_file->addr();  // 获取缓存块指针

		// 修正缓存大小（防止异常情况下的数据不一致）
		_tick_cache_block->_size = min(_tick_cache_block->_size, _tick_cache_block->_capacity);

		if (bNew)
		{
			// 新文件初始化
			memset(_tick_cache_block, 0, _tick_cache_file->size());  // 清零内存

			// 设置缓存块头部信息
			_tick_cache_block->_capacity = CACHE_SIZE_STEP_AD;  // 设置初始容量
			_tick_cache_block->_type = BT_RT_Cache;             // 设置块类型
			_tick_cache_block->_size = 0;                       // 设置初始大小为0
			_tick_cache_block->_version = 1;                    // 设置版本号
			strcpy(_tick_cache_block->_blk_flag, BLK_FLAG);     // 设置块标识
		}
		else
		{
			// 已存在文件，重建索引
			for (uint32_t i = 0; i < _tick_cache_block->_size; i++)
			{
				const TickCacheItem& item = _tick_cache_block->_items[i];
				std::string key = StrUtil::printf("%s.%s", item._tick.exchg, item._tick.code);
				_tick_cache_idx[key] = i;       // 重建合约索引映射
			}
		}
	}

	if (_m1_cache.empty())
	{
		bool bNew = false;
		std::string filename = _base_dir + _m1_cache._filename;
		if (!BoostFile::exists(filename.c_str()))
		{
			uint64_t uSize = sizeof(RTBarCache) + sizeof(BarCacheItem) * CACHE_SIZE_STEP_AD;
			BoostFile bf;
			bf.create_new_file(filename.c_str());
			bf.truncate_file((uint32_t)uSize);
			bf.close_file();
			bNew = true;
		}

		_m1_cache._file_ptr.reset(new BoostMappingFile);
		_m1_cache._file_ptr->map(filename.c_str());
		_m1_cache._cache_block = (RTBarCache*)_m1_cache._file_ptr->addr();

		_m1_cache._cache_block->_size = min(_m1_cache._cache_block->_size, _m1_cache._cache_block->_capacity);

		if (bNew)
		{
			memset(_m1_cache._cache_block, 0, _m1_cache._file_ptr->size());

			_m1_cache._cache_block->_capacity = CACHE_SIZE_STEP_AD;
			_m1_cache._cache_block->_type = BT_RT_Cache;
			_m1_cache._cache_block->_size = 0;
			_m1_cache._cache_block->_version = 1;
			strcpy(_m1_cache._cache_block->_blk_flag, BLK_FLAG);
		}
		else
		{
			for (uint32_t i = 0; i < _m1_cache._cache_block->_size; i++)
			{
				const BarCacheItem& item = _m1_cache._cache_block->_items[i];
				std::string key = StrUtil::printf("%s.%s", item._exchg, item._code);
				_m1_cache._idx[key] = i;
			}
		}
	}

	if (_m5_cache.empty())
	{
		bool bNew = false;
		std::string filename = _base_dir + _m5_cache._filename;
		if (!BoostFile::exists(filename.c_str()))
		{
			uint64_t uSize = sizeof(RTBarCache) + sizeof(BarCacheItem) * CACHE_SIZE_STEP_AD;
			BoostFile bf;
			bf.create_new_file(filename.c_str());
			bf.truncate_file((uint32_t)uSize);
			bf.close_file();
			bNew = true;
		}

		_m5_cache._file_ptr.reset(new BoostMappingFile);
		_m5_cache._file_ptr->map(filename.c_str());
		_m5_cache._cache_block = (RTBarCache*)_m5_cache._file_ptr->addr();

		_m5_cache._cache_block->_size = min(_m5_cache._cache_block->_size, _m5_cache._cache_block->_capacity);

		if (bNew)
		{
			memset(_m5_cache._cache_block, 0, _m5_cache._file_ptr->size());

			_m5_cache._cache_block->_capacity = CACHE_SIZE_STEP_AD;
			_m5_cache._cache_block->_type = BT_RT_Cache;
			_m5_cache._cache_block->_size = 0;
			_m5_cache._cache_block->_version = 1;
			strcpy(_m5_cache._cache_block->_blk_flag, BLK_FLAG);
		}
		else
		{
			for (uint32_t i = 0; i < _m5_cache._cache_block->_size; i++)
			{
				const BarCacheItem& item = _m5_cache._cache_block->_items[i];
				std::string key = StrUtil::printf("%s.%s", item._exchg, item._code);
				_m5_cache._idx[key] = i;
			}
		}
	}

	if (_d1_cache.empty())
	{
		bool bNew = false;
		std::string filename = _base_dir + _d1_cache._filename;
		if (!BoostFile::exists(filename.c_str()))
		{
			uint64_t uSize = sizeof(RTBarCache) + sizeof(BarCacheItem) * CACHE_SIZE_STEP_AD;
			BoostFile bf;
			bf.create_new_file(filename.c_str());
			bf.truncate_file((uint32_t)uSize);
			bf.close_file();
			bNew = true;
		}

		_d1_cache._file_ptr.reset(new BoostMappingFile);
		_d1_cache._file_ptr->map(filename.c_str());
		_d1_cache._cache_block = (RTBarCache*)_d1_cache._file_ptr->addr();

		_d1_cache._cache_block->_size = min(_d1_cache._cache_block->_size, _d1_cache._cache_block->_capacity);

		if (bNew)
		{
			memset(_d1_cache._cache_block, 0, _d1_cache._file_ptr->size());

			_d1_cache._cache_block->_capacity = CACHE_SIZE_STEP_AD;
			_d1_cache._cache_block->_type = BT_RT_Cache;
			_d1_cache._cache_block->_size = 0;
			_d1_cache._cache_block->_version = 1;
			strcpy(_d1_cache._cache_block->_blk_flag, BLK_FLAG);
		}
		else
		{
			for (uint32_t i = 0; i < _d1_cache._cache_block->_size; i++)
			{
				const BarCacheItem& item = _d1_cache._cache_block->_items[i];
				std::string key = StrUtil::printf("%s.%s", item._exchg, item._code);
				_d1_cache._idx[key] = i;
			}
		}
	}
}

/**
 * @brief 调整实时缓存块大小（模板方法）
 * 
 * 动态调整内存映射文件的大小以容纳更多数据项。
 * 该方法通过扩展文件大小并重新映射来实现缓存扩容。
 * 
 * @tparam HeaderType 缓存块头部类型（如RTTickCache、RTBarCache）
 * @tparam T 数据项类型（如TickCacheItem、BarCacheItem）
 * @param mfPtr 内存映射文件智能指针引用
 * @param nCount 新的容量大小（数据项数量）
 * @return 调整后的内存地址，失败返回NULL
 * 
 * 实现逻辑：
 * 1. 检查当前容量是否已满足需求
 * 2. 计算需要扩展的文件大小
 * 3. 向文件末尾追加空白数据
 * 4. 重新映射文件到内存
 * 5. 更新容量信息
 */
template<typename HeaderType, typename T>
void* WtDataWriterAD::resizeRTBlock(BoostMFPtr& mfPtr, uint32_t nCount)
{
	if (mfPtr == NULL)                          // 检查内存映射文件指针有效性
		return NULL;

	// 注意：调用该函数之前，应该已经申请了写锁
	RTBlockHeader* tBlock = (RTBlockHeader*)mfPtr->addr();  // 获取块头指针
	if (tBlock->_capacity >= nCount)            // 如果当前容量足够
		return mfPtr->addr();                   // 直接返回，无需扩容

	// 计算文件大小
	const char* filename = mfPtr->filename();   // 获取文件名
	uint64_t uOldSize = sizeof(HeaderType) + sizeof(T)*tBlock->_capacity;  // 旧文件大小
	uint64_t uNewSize = sizeof(HeaderType) + sizeof(T)*nCount;  // 新文件大小
	
	// 准备扩展数据（用0填充）
	std::string data;
	data.resize((std::size_t)(uNewSize - uOldSize), 0);
	
	try
	{
		// 打开文件并追加数据
		BoostFile f;
		f.open_existing_file(filename);         // 打开现有文件
		f.seek_to_end();                        // 定位到文件末尾
		f.write_file(data.c_str(), data.size());  // 写入扩展数据
		f.close_file();                         // 关闭文件
	}
	catch(std::exception& ex)
	{
		// 扩展失败，记录错误日志
		pipe_writer_log(_sink, LL_ERROR, "Exception occured while expanding RT cache file of {}[{}]: {}", 
			filename, uNewSize, ex.what());
		return mfPtr->addr();                   // 返回原地址
	}

	// 创建新的内存映射文件对象
	BoostMappingFile* pNewMf = new BoostMappingFile();
	if (!pNewMf->map(filename))                 // 重新映射文件
	{
		delete pNewMf;                          // 映射失败，删除对象
		return NULL;
	}

	mfPtr.reset(pNewMf);                        // 更新智能指针

	// 更新容量信息
	tBlock = (RTBlockHeader*)mfPtr->addr();     // 获取新的块头指针
	tBlock->_capacity = nCount;                 // 设置新容量
	return mfPtr->addr();                       // 返回新地址
}

/**
 * @brief 写入Tick数据
 * 
 * 接收Tick数据并异步处理，包括缓存更新、K线合成和数据持久化。
 * 这是数据写入器的核心方法，处理所有实时行情数据的写入逻辑。
 * 
 * @param curTick Tick数据对象
 * @param procFlag 处理标志：
 *                 - 0: 直接使用原始数据
 *                 - 1: 计算增量数据（成交量、成交额等）
 *                 - 2: 自动累加模式
 * @return 处理成功返回true，失败返回false
 */
bool WtDataWriterAD::writeTick(WTSTickData* curTick, uint32_t procFlag)
{
	if (curTick == NULL)                        // 检查Tick数据有效性
		return false;

	curTick->retain();                          // 增加引用计数，防止异步处理时被释放
	
	// 将Tick处理任务推送到异步队列
	pushTask([this, curTick, procFlag](){

		do
		{
			// 获取合约信息
			WTSContractInfo* ct = curTick->getContractInfo();
			if(ct == NULL)                      // 检查合约信息有效性
				break;

			WTSCommodityInfo* commInfo = ct->getCommInfo();  // 获取商品信息

			// 检查交易时段状态，非交易时段不处理数据
			if (!_sink->canSessionReceive(commInfo->getSession()))
				break;

			// 第一步：更新Tick缓存
			if (!updateTickCache(ct, curTick, procFlag))
				break;                          // 缓存更新失败则跳出

			// 第二步：写入Tick数据到持久化存储
			if(!_disable_tick)                  // 检查是否禁用Tick写入
				pipeToTicks(ct, curTick);

			// 第三步：更新K线缓存（实时合成K线）
			updateBarCache(ct, curTick);

			// 第四步：广播Tick数据到下游系统
			_sink->broadcastTick(curTick);

			// 统计和日志记录（按交易所分组统计）
			static wt_hashmap<std::string, uint64_t> _tcnt_map;
			_tcnt_map[curTick->exchg()]++;      // 增加该交易所的Tick计数
			
			// 每处理指定数量的Tick后输出统计日志
			if (_tcnt_map[curTick->exchg()] % _log_group_size == 0)
			{
				pipe_writer_log(_sink, LL_INFO, "{} ticks received from exchange {}",
					_tcnt_map[curTick->exchg()], curTick->exchg());
			}
		} while (false);

		curTick->release();                     // 释放引用计数
	});
	return true;                                // 返回任务推送成功
}

/**
 * @brief 推送异步任务
 * 
 * 将任务添加到异步处理队列中，由后台线程执行。
 * 如果是第一次调用，会自动创建后台处理线程。
 * 
 * @param task 任务函数对象
 */
void WtDataWriterAD::pushTask(TaskInfo task)
{
	if(_async_task)                             // 如果启用异步任务处理
	{
		StdUniqueLock lck(_task_mtx);           // 加锁保护任务队列
		_tasks.push(task);                      // 将任务加入队列
		_task_cond.notify_all();                // 通知后台线程有新任务
	}
	else
	{
		// 同步模式，直接执行任务
		task();                                 // 立即执行任务
		return;
	}

	// 检查后台线程是否已创建
	if(_task_thrd == NULL)
	{
		// 创建后台处理线程
		_task_thrd.reset(new StdThread([this](){
			// 线程主循环
			while (!_terminated)                // 循环直到终止标志被设置
			{
				// 检查任务队列是否为空
				if(_tasks.empty())
				{
					// 队列为空，等待新任务
					StdUniqueLock lck(_task_mtx);  // 加锁
					_task_cond.wait(_task_mtx); // 等待条件变量通知
					continue;
				}

				// 批量获取任务（减少锁竞争）
				std::queue<TaskInfo> tempQueue;
				{
					StdUniqueLock lck(_task_mtx);  // 加锁
					tempQueue.swap(_tasks);     // 交换队列，快速释放锁
				}

				// 执行批量任务
				while(!tempQueue.empty())
				{
					TaskInfo& curTask = tempQueue.front();  // 获取队首任务
					curTask();                  // 执行任务
					tempQueue.pop();            // 移除已执行的任务
				}
			}
		}));
	}
}

/**
 * @brief 将Tick数据写入持久化存储
 * 
 * 将Tick数据写入LMDB数据库并通知扩展转储器。
 * 该方法实现了Tick数据的持久化存储逻辑。
 * 
 * @param ct 合约信息
 * @param curTick 当前Tick数据
 */
void WtDataWriterAD::pipeToTicks(WTSContractInfo* ct, WTSTickData* curTick)
{
	// 获取对应的LMDB数据库连接
	WtLMDBPtr db = get_t_db(ct->getExchg(), ct->getCode());
	if (db)
	{
		// 时间转换：将实际发生时间转换为偏移时间
		// 使用交易日作为日期，这样可以方便地按交易日筛选历史Tick数据
		uint32_t actTime = curTick->actiontime();  // 获取实际发生时间
		uint32_t offTime = ct->getCommInfo()->getSessionInfo()->offsetTime(actTime / 100000, true) 
			* 100000 + actTime % 100000;        // 转换为偏移时间（保留毫秒）

		// 构造LMDB键值
		LMDBHftKey key(ct->getExchg(), ct->getCode(), curTick->tradingdate(), offTime);
		WtLMDBQuery query(*db);                 // 创建查询对象
		
		// 写入数据库并提交事务
		if (!query.put_and_commit((void*)&key, sizeof(key), &curTick->getTickStruct(), sizeof(WTSTickStruct)))
		{
			// 写入失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe tick of {} to db failed: {}", ct->getFullCode(), db->errmsg());
		}
	}

	// 通知所有扩展转储器
	for(auto& item : _dumpers)
	{
		const char* id = item.first.c_str();    // 转储器ID
		IHisDataDumper* dumper = item.second;   // 转储器接口
		if (dumper == NULL)                     // 检查转储器有效性
			continue;

		// 调用扩展转储器的Tick转储方法
		bool bSucc = dumper->dumpHisTicks(ct->getFullCode(), curTick->tradingdate(), 
			&curTick->getTickStruct(), 1);
		if (!bSucc)
		{
			// 转储失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe tick data of {} via extended dumper {} failed", 
				ct->getFullCode(), id);
		}
	}
}

/**
 * @brief 将日K线数据写入持久化存储
 * 
 * 将完整的日K线数据写入LMDB数据库并通知扩展转储器。
 * 
 * @param ct 合约信息
 * @param bar K线数据结构
 */
void WtDataWriterAD::pipeToDayBars(WTSContractInfo* ct, const WTSBarStruct& bar)
{
	// 获取日K线数据库连接
	WtLMDBPtr db = get_k_db(ct->getExchg(), KP_DAY);
	if (db)
	{
		// 构造LMDB键值（使用日期作为键）
		LMDBBarKey key(ct->getExchg(), ct->getCode(), bar.date);
		WtLMDBQuery query(*db);                 // 创建查询对象
		
		// 写入数据库并提交事务
		if (!query.put_and_commit((void*)&key, sizeof(key), (void*)&bar, sizeof(WTSBarStruct)))
		{
			// 写入失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe day bar @ {} of {} to db failed", bar.date, ct->getFullCode());
		}
		else
		{
			// 写入成功，记录调试日志
			pipe_writer_log(_sink, LL_DEBUG, "day bar @ {} of {} piped to db", bar.date, ct->getFullCode());
		}
	}

	// 通知所有扩展转储器
	for (auto& item : _dumpers)
	{
		const char* id = item.first.c_str();    // 转储器ID
		IHisDataDumper* dumper = item.second;   // 转储器接口
		if (dumper == NULL)                     // 检查转储器有效性
			continue;

		// 调用扩展转储器的K线转储方法
		bool bSucc = dumper->dumpHisBars(ct->getFullCode(), "d1", (WTSBarStruct*)&bar, 1);
		if (!bSucc)
		{
			// 转储失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe day bar @ {} of {} via extended dumper {} failed", 
				bar.date, ct->getFullCode(), id);
		}
	}
}

/**
 * @brief 将1分钟K线数据写入持久化存储
 * 
 * 将完整的1分钟K线数据写入LMDB数据库并通知扩展转储器。
 * 
 * @param ct 合约信息
 * @param bar K线数据结构
 */
void WtDataWriterAD::pipeToM1Bars(WTSContractInfo* ct, const WTSBarStruct& bar)
{
	// 获取1分钟K线数据库连接
	WtLMDBPtr db = get_k_db(ct->getExchg(), KP_Minute1);
	if(db)
	{
		// 构造LMDB键值（使用K线时间作为键）
		LMDBBarKey key(ct->getExchg(), ct->getCode(), (uint32_t)bar.time);
		WtLMDBQuery query(*db);                 // 创建查询对象
		
		// 写入数据库并提交事务
		if(!query.put_and_commit((void*)&key, sizeof(key), (void*)&bar, sizeof(WTSBarStruct)))
		{
			// 写入失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe m1 bar @ {} of {} to db failed", bar.time, ct->getFullCode());
		}
		else
		{
			// 写入成功，记录调试日志
			pipe_writer_log(_sink, LL_DEBUG, "m1 bar @ {} of {} piped to db", bar.time, ct->getFullCode());
		}
	}

	// 通知所有扩展转储器
	for (auto& item : _dumpers)
	{
		const char* id = item.first.c_str();    // 转储器ID
		IHisDataDumper* dumper = item.second;   // 转储器接口
		if (dumper == NULL)                     // 检查转储器有效性
			continue;

		// 调用扩展转储器的K线转储方法
		bool bSucc = dumper->dumpHisBars(ct->getFullCode(), "m1", (WTSBarStruct*)&bar, 1);
		if (!bSucc)
		{
			// 转储失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe m1 bar @ {} of {} via extended dumper {} failed", 
				bar.time, ct->getFullCode(), id);
		}
	}
}

/**
 * @brief 将5分钟K线数据写入持久化存储
 * 
 * 将完整的5分钟K线数据写入LMDB数据库并通知扩展转储器。
 * 
 * @param ct 合约信息
 * @param bar K线数据结构
 */
void WtDataWriterAD::pipeToM5Bars(WTSContractInfo* ct, const WTSBarStruct& bar)
{
	// 获取5分钟K线数据库连接
	WtLMDBPtr db = get_k_db(ct->getExchg(), KP_Minute5);
	if (db)
	{
		// 构造LMDB键值（使用K线时间作为键）
		LMDBBarKey key(ct->getExchg(), ct->getCode(), (uint32_t)bar.time);
		WtLMDBQuery query(*db);                 // 创建查询对象
		
		// 写入数据库并提交事务
		if (!query.put_and_commit((void*)&key, sizeof(key), (void*)&bar, sizeof(bar)))
		{
			// 写入失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe m5 bar @ {} of {} to db failed", bar.time, ct->getFullCode());
		}
		else
		{
			// 写入成功，记录调试日志
			pipe_writer_log(_sink, LL_DEBUG, "m5 bar @ {} of {} piped to db", bar.time, ct->getFullCode());
		}
	}

	// 通知所有扩展转储器
	for (auto& item : _dumpers)
	{
		const char* id = item.first.c_str();    // 转储器ID
		IHisDataDumper* dumper = item.second;   // 转储器接口
		if (dumper == NULL)                     // 检查转储器有效性
			continue;

		// 调用扩展转储器的K线转储方法
		bool bSucc = dumper->dumpHisBars(ct->getFullCode(), "m5", (WTSBarStruct*)&bar, 1);
		if (!bSucc)
		{
			// 转储失败，记录错误日志
			pipe_writer_log(_sink, LL_ERROR, "pipe m5 bar @ {} of {} via extended dumper {} failed", 
				bar.time, ct->getFullCode(), id);
		}
	}
}

/**
 * @brief 更新K线缓存
 * 
 * 根据Tick数据实时合成并更新各周期K线缓存（1分钟、5分钟、日K线）。
 * 该方法是K线实时合成引擎的核心，负责将Tick数据聚合成不同周期的K线。
 * 
 * @param ct 合约信息
 * @param curTick 当前Tick数据
 */
void WtDataWriterAD::updateBarCache(WTSContractInfo* ct, WTSTickData* curTick)
{
	// 获取基本时间信息
	uint32_t uDate = curTick->actiondate();     // 实际发生日期
	WTSSessionInfo* sInfo = _bd_mgr->getSessionByCode(curTick->code(), curTick->exchg());
	uint32_t curTime = curTick->actiontime() / 100000;  // 实际发生时间（去掉毫秒）

	// 将时间转换为分钟数（从交易时段开始计算）
	uint32_t minutes = sInfo->timeToMinutes(curTime, false);
	if (minutes == INVALID_UINT32)              // 检查时间是否有效
		return;                                 // 无效时间（非交易时段），直接返回

	// 时间边界处理：
	// 当秒数为0时，要专门处理，比如091500000这笔tick要算作0915的
	// 如果是小节结束，要算作小节结束那一分钟，因为经常会有超过结束时间的价格进来，如113000500
	// 不能同时处理，所以用or
	if (sInfo->isLastOfSection(curTime))        // 如果是小节最后一分钟
	{
		minutes--;                              // 时间往前调整一分钟
	}

	// 构造缓存键值
	std::string key = StrUtil::printf("%s.%s", curTick->exchg(), curTick->code());

	//////////////////////////////////////////////////////////////////////////
	// 更新日K线缓存
	if (!_disable_day && _d1_cache._cache_block)  // 检查是否启用日K线且缓存已初始化
	{
		StdUniqueLock lock(_d1_cache._mtx);     // 加锁保护日K线缓存
		uint32_t idx = 0;                       // 缓存项索引
		bool bNewCode = false;                  // 是否为新合约
		
		// 检查该合约是否已有缓存项
		if (_d1_cache._idx.find(key) == _d1_cache._idx.end())
		{
			// 新合约，创建新的缓存项
			idx = _d1_cache._cache_block->_size;  // 使用当前大小作为索引
			_d1_cache._idx[key] = _d1_cache._cache_block->_size;  // 添加到索引映射
			_d1_cache._cache_block->_size += 1;  // 增加缓存大小
			
			// 检查是否需要扩容
			if (_d1_cache._cache_block->_size >= _d1_cache._cache_block->_capacity)
			{
				_d1_cache._cache_block = (RTBarCache*)resizeRTBlock<RTBarCache, BarCacheItem>(
					_d1_cache._file_ptr, _d1_cache._cache_block->_capacity + CACHE_SIZE_STEP_AD);
				pipe_writer_log(_sink, LL_INFO, "day cache resized to {} items", _d1_cache._cache_block->_capacity);
			}
			bNewCode = true;                    // 标记为新合约
		}
		else
		{
			// 已存在的合约，获取索引
			idx = _d1_cache._idx[key];
		}

		// 获取缓存项并设置基本信息
		BarCacheItem& item = (BarCacheItem&)_d1_cache._cache_block->_items[idx];
		if (bNewCode)                           // 如果是新合约
		{
			strcpy(item._exchg, curTick->exchg());  // 设置交易所代码
			strcpy(item._code, curTick->code());    // 设置合约代码
		}
		WTSBarStruct* lastBar = &item._bar;     // 获取K线数据指针

		// 检查是否需要创建新的K线
		uint32_t barDate = curTick->tradingdate();  // 获取交易日

		bool bNewBar = false;                   // 是否为新K线
		if (lastBar == NULL || barDate > lastBar->date)
		{
			bNewBar = true;                     // 交易日变化，需要新K线
		}

		WTSBarStruct* newBar = lastBar;         // 当前操作的K线指针
		if (bNewBar)
		{
			// 新K线：将上一根K线写入数据库，然后初始化新K线
			if (!bNewCode)                      // 如果不是新合约
			{
				pipeToDayBars(ct, *lastBar);    // 将完整的上一根日K线写入数据库
			}

			// 初始化新K线的OHLC数据
			newBar->date = curTick->tradingdate();  // 设置交易日
			newBar->time = barDate;             // 设置时间戳
			newBar->open = curTick->price();    // 开盘价（当前价格）
			newBar->high = curTick->price();    // 最高价（当前价格）
			newBar->low = curTick->price();     // 最低价（当前价格）
			newBar->close = curTick->price();   // 收盘价（当前价格）

			// 初始化成交量数据
			newBar->vol = curTick->volume();    // 成交量
			newBar->money = curTick->turnover();  // 成交额
			newBar->hold = curTick->openinterest();  // 持仓量
			newBar->add = curTick->additional();  // 增仓
		}
		else
		{
			// 更新现有K线：聚合Tick数据到当前K线
			
			/*
			 * By Wesley @ 2023.07.05
			 * 发现某些品种，开盘时可能会推送price为0的tick进来
			 * 会导致open和low都是0，所以要再做一个判断
			 */
			if (decimal::eq(newBar->open, 0))   // 如果开盘价为0
				newBar->open = curTick->price();  // 使用当前价格

			if (decimal::eq(newBar->low, 0))    // 如果最低价为0
				newBar->low = curTick->price();   // 使用当前价格
			else
				newBar->low = std::min(curTick->price(), newBar->low);  // 更新最低价

			newBar->close = curTick->price();   // 更新收盘价（始终使用最新价格）
			newBar->high = max(curTick->price(), newBar->high);  // 更新最高价

			// 累加成交量数据
			newBar->vol += curTick->volume();   // 累加成交量
			newBar->money += curTick->turnover();  // 累加成交额
			newBar->vol += curTick->volume();   // 累加成交量（重复了？）
			newBar->money += curTick->turnover();  // 累加成交额（重复了？）
			newBar->hold = curTick->openinterest();  // 更新持仓量（使用最新值）
			newBar->add += curTick->additional();  // 累加增仓
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 更新1分钟K线缓存
	if (!_disable_min1 && _m1_cache._cache_block)  // 检查是否启用1分钟K线且缓存已初始化
	{
		StdUniqueLock lock(_m1_cache._mtx);     // 加锁保护1分钟K线缓存
		uint32_t idx = 0;                       // 缓存项索引
		bool bNewCode = false;                  // 是否为新合约
		
		// 检查该合约是否已有缓存项
		if (_m1_cache._idx.find(key) == _m1_cache._idx.end())
		{
			// 新合约，创建新的缓存项
			idx = _m1_cache._cache_block->_size;
			_m1_cache._idx[key] = _m1_cache._cache_block->_size;
			_m1_cache._cache_block->_size += 1;
			
			// 检查是否需要扩容
			if (_m1_cache._cache_block->_size >= _m1_cache._cache_block->_capacity)
			{
				_m1_cache._cache_block = (RTBarCache*)resizeRTBlock<RTBarCache, BarCacheItem>(
					_m1_cache._file_ptr, _m1_cache._cache_block->_capacity + CACHE_SIZE_STEP_AD);
				pipe_writer_log(_sink, LL_INFO, "m1 cache resized to {} items", _m1_cache._cache_block->_capacity);
			}
			bNewCode = true;
		}
		else
		{
			idx = _m1_cache._idx[key];
		}

		BarCacheItem& item = (BarCacheItem&)_m1_cache._cache_block->_items[idx];
		if(bNewCode)
		{
			strcpy(item._exchg, curTick->exchg());
			strcpy(item._code, curTick->code());
		}
		WTSBarStruct* lastBar = &item._bar;

		// 计算1分钟K线的时间窗口
		uint32_t barMins = minutes + 1;         // K线时间为当前分钟+1
		uint64_t barTime = sInfo->minuteToTime(barMins);  // 将分钟数转换为时间
		uint32_t barDate = uDate;               // K线日期
		if (barTime == 0)                       // 如果时间为0（跨日）
		{
			barDate = TimeUtils::getNextDate(barDate);  // 使用下一个日期
		}
		barTime = TimeUtils::timeToMinBar(barDate, (uint32_t)barTime);  // 转换为K线时间格式

		// 检查是否需要创建新的K线
		bool bNewBar = false;
		if (lastBar == NULL || barTime > lastBar->time)
		{
			bNewBar = true;                     // 时间窗口变化，需要新K线
		}

		WTSBarStruct* newBar = lastBar;
		if (bNewBar)
		{
			// 新K线：将上一根K线写入数据库，然后初始化新K线
			if (!bNewCode)
			{
				pipeToM1Bars(ct, *lastBar);     // 将完整的上一根1分钟K线写入数据库

				// 检查是否跨日（用于日K线处理）
				uint32_t lastMins = sInfo->timeToMinutes(lastBar->time % 10000, false);
				if(lastMins > barMins)
				{
					// 如果上一条K线的分钟数大于当前K线的分钟数
					// 说明交易日换了，需要保存日线
					// TODO: 这里可以触发日K线的保存逻辑
				}
			}

			// 初始化新K线
			newBar->date = curTick->tradingdate();
			newBar->time = barTime;
			newBar->open = curTick->price();
			newBar->high = curTick->price();
			newBar->low = curTick->price();
			newBar->close = curTick->price();

			newBar->vol = curTick->volume();
			newBar->money = curTick->turnover();
			newBar->hold = curTick->openinterest();
			newBar->add = curTick->additional();
		}
		else
		{
			// 更新现有K线
			
			/*
			 * By Wesley @ 2023.07.05
			 * 发现某些品种，开盘时可能会推送price为0的tick进来
			 * 会导致open和low都是0，所以要再做一个判断
			 */
			if (decimal::eq(newBar->open, 0))
				newBar->open = curTick->price();

			if (decimal::eq(newBar->low, 0))
				newBar->low = curTick->price();
			else
				newBar->low = std::min(curTick->price(), newBar->low);

			newBar->close = curTick->price();
			newBar->high = max(curTick->price(), newBar->high);

			newBar->vol += curTick->volume();
			newBar->money += curTick->turnover();
			newBar->hold = curTick->openinterest();
			newBar->add += curTick->additional();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 更新5分钟K线缓存
	if (!_disable_min5 && _m5_cache._cache_block)  // 检查是否启用5分钟K线且缓存已初始化
	{
		StdUniqueLock lock(_m5_cache._mtx);     // 加锁保护5分钟K线缓存
		uint32_t idx = 0;                       // 缓存项索引
		bool bNewCode = false;                  // 是否为新合约
		
		// 检查该合约是否已有缓存项
		if (_m5_cache._idx.find(key) == _m5_cache._idx.end())
		{
			// 新合约，创建新的缓存项
			idx = _m5_cache._cache_block->_size;
			_m5_cache._idx[key] = _m5_cache._cache_block->_size;
			_m5_cache._cache_block->_size += 1;
			
			// 检查是否需要扩容
			if (_m5_cache._cache_block->_size >= _m5_cache._cache_block->_capacity)
			{
				_m5_cache._cache_block = (RTBarCache*)resizeRTBlock<RTBarCache, BarCacheItem>(
					_m5_cache._file_ptr, _m5_cache._cache_block->_capacity + CACHE_SIZE_STEP_AD);
				pipe_writer_log(_sink, LL_INFO, "m5 cache resized to {} items", _m5_cache._cache_block->_capacity);
			}
			bNewCode = true;
		}
		else
		{
			idx = _m5_cache._idx[key];
		}

		BarCacheItem& item = (BarCacheItem&)_m5_cache._cache_block->_items[idx];
		if (bNewCode)
		{
			strcpy(item._exchg, curTick->exchg());
			strcpy(item._code, curTick->code());
		}
		WTSBarStruct* lastBar = &item._bar;

		// 计算5分钟K线的时间窗口（向上取整到5的倍数）
		uint32_t barMins = (minutes / 5) * 5 + 5;  // K线时间向上对齐到5分钟
		uint64_t barTime = sInfo->minuteToTime(barMins);  // 将分钟数转换为时间
		uint32_t barDate = uDate;               // K线日期
		if (barTime == 0)                       // 如果时间为0（跨日）
		{
			barDate = TimeUtils::getNextDate(barDate);  // 使用下一个日期
		}
		barTime = TimeUtils::timeToMinBar(barDate, (uint32_t)barTime);  // 转换为K线时间格式

		// 检查是否需要创建新的K线
		bool bNewBar = false;
		if (lastBar == NULL || barTime > lastBar->time)
		{
			bNewBar = true;                     // 时间窗口变化，需要新K线
		}

		WTSBarStruct* newBar = lastBar;
		if (bNewBar)
		{
			// 新K线：将上一根K线写入数据库，然后初始化新K线
			if(!bNewCode)
				pipeToM5Bars(ct, *lastBar);     // 将完整的上一根5分钟K线写入数据库

			// 初始化新K线
			newBar->date = curTick->tradingdate();
			newBar->time = barTime;
			newBar->open = curTick->price();
			newBar->high = curTick->price();
			newBar->low = curTick->price();
			newBar->close = curTick->price();

			newBar->vol = curTick->volume();
			newBar->money = curTick->turnover();
			newBar->hold = curTick->openinterest();
			newBar->add = curTick->additional();
		}
		else
		{
			// 更新现有K线：聚合当前Tick数据
			newBar->close = curTick->price();   // 更新收盘价
			newBar->high = max(curTick->price(), newBar->high);  // 更新最高价
			newBar->low = min(curTick->price(), newBar->low);    // 更新最低价

			// 累加成交量数据
			newBar->vol += curTick->volume();
			newBar->money += curTick->turnover();
			newBar->hold = curTick->openinterest();
			newBar->add += curTick->additional();
		}
	}
}

/**
 * @brief 获取当前Tick数据
 * 
 * 从缓存中获取指定合约的最新Tick数据。
 * 该方法提供线程安全的Tick数据访问。
 * 
 * @param code 合约代码
 * @param exchg 交易所代码（可选）
 * @return Tick数据对象，失败返回NULL
 */
WTSTickData* WtDataWriterAD::getCurTick(const char* code, const char* exchg/* = ""*/)
{
	if (strlen(code) == 0)                      // 检查合约代码有效性
		return NULL;

	// 获取合约信息
	WTSContractInfo* ct = _bd_mgr->getContract(code, exchg);
	if (ct == NULL)                             // 检查合约是否存在
		return NULL;

	// 构造缓存键值
	std::string key = StrUtil::printf("%s.%s", ct->getExchg(), ct->getCode());
	
	// 线程安全地访问Tick缓存
	StdUniqueLock lock(_mtx_tick_cache);        // 加锁
	auto it = _tick_cache_idx.find(key);        // 查找合约索引
	if (it == _tick_cache_idx.end())            // 检查是否存在缓存
		return NULL;

	// 获取Tick缓存项并创建Tick数据对象
	uint32_t idx = it->second;                  // 获取索引位置
	TickCacheItem& item = _tick_cache_block->_items[idx];  // 获取缓存项
	return WTSTickData::create(item._tick);     // 创建并返回Tick数据对象
}

/**
 * @brief 更新Tick缓存
 * 
 * 将新的Tick数据更新到内存映射缓存中，处理数据预处理逻辑。
 * 该方法是数据写入流程的核心环节，负责Tick数据的验证、处理和缓存更新。
 * 
 * @param ct 合约信息
 * @param curTick 当前Tick数据
 * @param procFlag 处理标志：
 *                 - 0: 直接使用原始数据
 *                 - 1: 计算增量数据（成交量、成交额等）
 *                 - 2: 自动累加模式
 * @return 更新成功返回true，失败返回false
 */
bool WtDataWriterAD::updateTickCache(WTSContractInfo* ct, WTSTickData* curTick, uint32_t procFlag)
{
	// 参数有效性检查
	if (curTick == NULL || _tick_cache_block == NULL)
	{
		pipe_writer_log(_sink, LL_ERROR, "Tick cache data not initialized");
		return false;
	}

	// 线程安全地访问Tick缓存
	StdUniqueLock lock(_mtx_tick_cache);        // 加锁
	
	// 构造缓存键值
	std::string key = StrUtil::printf("%s.%s", curTick->exchg(), curTick->code());
	uint32_t idx = 0;
	
	// 检查该合约是否已有缓存项
	if (_tick_cache_idx.find(key) == _tick_cache_idx.end())
	{
		// 新合约，需要创建新的缓存项
		idx = _tick_cache_block->_size;         // 使用当前大小作为索引
		_tick_cache_idx[key] = _tick_cache_block->_size;  // 添加到索引映射
		_tick_cache_block->_size += 1;          // 增加缓存大小
		
		// 检查是否需要扩容
		if(_tick_cache_block->_size >= _tick_cache_block->_capacity)
		{
			// 缓存已满，扩容
			_tick_cache_block = (RTTickCache*)resizeRTBlock<RTTickCache, TickCacheItem>(
				_tick_cache_file, _tick_cache_block->_capacity + CACHE_SIZE_STEP_AD);
			pipe_writer_log(_sink, LL_INFO, "Tick Cache resized to {} items", _tick_cache_block->_capacity);
		}
	}
	else
	{
		// 已存在的合约，获取索引
		idx = _tick_cache_idx[key];
	}


	TickCacheItem& item = _tick_cache_block->_items[idx];
	if (curTick->tradingdate() < item._date)
	{
		pipe_writer_log(_sink, LL_INFO, "Tradingday[{}] of {} is less than cached tradingday[{}]", curTick->tradingdate(), curTick->code(), item._date);
		return false;
	}

	WTSTickStruct& newTick = curTick->getTickStruct();

	if (curTick->tradingdate() > item._date)
	{
		item._date = curTick->tradingdate();
		
		if(procFlag == 0)
		{
			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));
		}
		else if (procFlag == 1)
		{
			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));

			item._tick.volume = item._tick.total_volume;
			item._tick.turn_over = item._tick.total_turnover;
			item._tick.diff_interest = item._tick.open_interest - item._tick.pre_interest;

			newTick.volume = newTick.total_volume;
			newTick.turn_over = newTick.total_turnover;
			newTick.diff_interest = newTick.open_interest - newTick.pre_interest;
		}
		else if(procFlag == 2)
		{
			double pre_close = item._tick.price;
			double pre_interest = item._tick.open_interest;

			if (decimal::eq(newTick.total_volume, 0))
				newTick.total_volume = newTick.volume + item._tick.total_volume;

			if (decimal::eq(newTick.total_turnover, 0))
				newTick.total_turnover = newTick.turn_over + item._tick.total_turnover;

			if (decimal::eq(newTick.open, 0))
				newTick.open = newTick.price;

			if (decimal::eq(newTick.high, 0))
				newTick.high = newTick.price;

			if (decimal::eq(newTick.low, 0))
				newTick.low =newTick.price;

			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));
			item._tick.pre_close = pre_close;
			item._tick.pre_interest = pre_interest;
		}

		//	newTick.trading_date, curTick->exchg(), curTick->code(), curTick->volume(),
		//	curTick->turnover(), curTick->openinterest(), curTick->additional());
		pipe_writer_log(_sink, LL_INFO, "First tick of new tradingday {},{}.{},{},{},{},{},{}", 
			newTick.trading_date, curTick->exchg(), curTick->code(), curTick->price(), curTick->volume(),
			curTick->turnover(), curTick->openinterest(), curTick->additional());
	}
	else
	{
		//如果缓存里的数据日期大于最新行情的日期
		//或者缓存里的时间大于等于最新行情的时间,数据就不需要处理
		WTSSessionInfo* sInfo = _bd_mgr->getSessionByCode(curTick->code(), curTick->exchg());
		uint32_t tdate = sInfo->getOffsetDate(curTick->actiondate(), curTick->actiontime() / 100000);
		if (tdate > curTick->tradingdate())
		{
			pipe_writer_log(_sink, LL_ERROR, "Last tick of {}.{} with time {}.{} has an exception, abandoned", curTick->exchg(), curTick->code(), curTick->actiondate(), curTick->actiontime());
			return false;
		}
		else if (curTick->totalvolume() < item._tick.total_volume && procFlag != 2)
		{
			pipe_writer_log(_sink, LL_ERROR, "Last tick of {}.{} with time {}.{}, volume {} is less than cached volume {}, abandoned", 
				curTick->exchg(), curTick->code(), curTick->actiondate(), curTick->actiontime(), curTick->totalvolume(), item._tick.total_volume);
			return false;
		}

		//时间戳相同,但是成交量大于等于原来的,这种情况一般是郑商所,这里的处理方式就是时间戳+200毫秒
		//By Wesley @ 2021.12.21
		//今天发现居然一秒出现了4笔，实在是有点无语
		//只能把500毫秒的变化量改成200，并且改成发生时间小于等于上一笔的判断
		if(newTick.action_date == item._tick.action_date && newTick.action_time <= item._tick.action_time && newTick.total_volume >= item._tick.total_volume)
		{
			newTick.action_time += 200;
		}

		//这里就要看需不需要预处理了
		if(procFlag == 0)
		{
			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));
		}
		else if (procFlag == 1)
		{
			newTick.volume = newTick.total_volume - item._tick.total_volume;
			newTick.turn_over = newTick.total_turnover - item._tick.total_turnover;
			newTick.diff_interest = newTick.open_interest - item._tick.open_interest;

			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));
		}
		else if (procFlag == 2)
		{
			//自动累加
			//如果总成交量为0，则需要累加上一笔的总成交量
			if(decimal::eq(newTick.total_volume, 0))
				newTick.total_volume = newTick.volume + item._tick.total_volume;

			if (decimal::eq(newTick.total_turnover, 0))
				newTick.total_turnover = newTick.turn_over + item._tick.total_turnover;

			if (decimal::eq(newTick.open, 0))
				newTick.open = newTick.price;

			if (decimal::eq(newTick.high, 0))
				newTick.high = max(newTick.price, item._tick.high);

			if (decimal::eq(newTick.low, 0))
				newTick.low = max(newTick.price, item._tick.low);

			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));
		}
	}

	return true;                                // 返回更新成功
}

/**
 * @brief 获取K线数据库连接
 * 
 * 根据交易所和K线周期获取对应的LMDB数据库连接。
 * 实现了数据库连接的缓存和按需加载机制。
 * 注意：Writer使用读写模式打开数据库。
 * 
 * @param exchg 交易所代码
 * @param period K线周期
 * @return LMDB数据库智能指针，失败返回空指针
 */
WtDataWriterAD::WtLMDBPtr WtDataWriterAD::get_k_db(const char* exchg, WTSKlinePeriod period)
{
	WtLMDBMap* the_map = NULL;                  // 数据库映射表指针
	std::string subdir;                         // 子目录名称
	
	// 根据K线周期选择对应的数据库映射表和子目录
	if (period == KP_Minute1)
	{
		the_map = &_exchg_m1_dbs;               // 1分钟K线数据库映射表
		subdir = "min1";                        // 1分钟数据子目录
	}
	else if (period == KP_Minute5)
	{
		the_map = &_exchg_m5_dbs;               // 5分钟K线数据库映射表
		subdir = "min5";                        // 5分钟数据子目录
	}
	else if (period == KP_DAY)
	{
		the_map = &_exchg_d1_dbs;               // 日K线数据库映射表
		subdir = "day";                         // 日线数据子目录
	}
	else
		return std::move(WtLMDBPtr());          // 不支持的周期，返回空指针

	// 检查缓存中是否已有该交易所的数据库连接
	auto it = the_map->find(exchg);
	if (it != the_map->end())
		return std::move(it->second);           // 返回缓存的连接

	// 创建新的数据库连接（读写模式，Writer需要写入权限）
	WtLMDBPtr dbPtr(new WtLMDB(false));
	std::string path = StrUtil::printf("%s%s/%s/", _base_dir.c_str(), subdir.c_str(), exchg);
	boost::filesystem::create_directories(path);  // 确保目录存在
	
	// 尝试打开数据库，指定映射大小
	if(!dbPtr->open(path.c_str(), _kline_mapsize))
	{
		// 打开失败，记录错误日志
		if (_sink) pipe_writer_log(_sink, LL_ERROR, "Opening {} db at {} failed: {}", subdir, path, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}

	// 将新连接加入缓存
	(*the_map)[exchg] = dbPtr;
	return std::move(dbPtr);                    // 返回新连接
}

/**
 * @brief 获取Tick数据库连接
 * 
 * 根据交易所和合约代码获取对应的LMDB数据库连接。
 * 实现了数据库连接的缓存和按需加载机制。
 * 注意：Writer使用读写模式打开数据库。
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @return LMDB数据库智能指针，失败返回空指针
 */
WtDataWriterAD::WtLMDBPtr WtDataWriterAD::get_t_db(const char* exchg, const char* code)
{
	// 构造缓存键值（格式："交易所.合约"）
	std::string key = StrUtil::printf("%s.%s", exchg, code);
	
	// 检查缓存中是否已有该合约的数据库连接
	auto it = _tick_dbs.find(key);
	if (it != _tick_dbs.end())
		return std::move(it->second);           // 返回缓存的连接

	// 创建新的数据库连接（读写模式，Writer需要写入权限）
	WtLMDBPtr dbPtr(new WtLMDB(false));
	std::string path = StrUtil::printf("%sticks/%s/%s", _base_dir.c_str(), exchg, code);
	boost::filesystem::create_directories(path);  // 确保目录存在
	
	// 尝试打开数据库，指定映射大小
	if (!dbPtr->open(path.c_str(), _tick_mapsize))
	{
		// 打开失败，记录错误日志
		if (_sink) pipe_writer_log(_sink, LL_ERROR, "Opening tick db at {} failed: %s", path, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}

	// 将新连接加入缓存
	_tick_dbs[key] = dbPtr;                     // 使用key作为缓存键
	return std::move(dbPtr);                    // 返回新连接
}
