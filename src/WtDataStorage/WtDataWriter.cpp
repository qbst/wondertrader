/*!
 * \file WtDataWriter.cpp
 * \project WonderTrader
 * 
 * \brief WonderTrader数据写入器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtDataWriter类的所有功能，是WonderTrader框架中用于数据写入的核心组件。
 * 该文件提供了将各种数据写入WonderTrader数据存储格式的功能，支持实时数据和历史数据的写入，
 * 包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型，主要用于实时行情数据存储和历史数据归档。
 * 
 * 核心实现机制：
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
 * 主要功能模块：
 * 
 * 1. 实时数据写入：
 *    - 实时K线数据写入
 *    - 实时Tick数据写入
 *    - 实时逐笔数据写入
 * 
 * 2. 历史数据写入：
 *    - 历史数据转储
 *    - 数据压缩和归档
 *    - 数据格式转换
 * 
 * 3. 数据处理功能：
 *    - 数据过滤和清洗
 *    - 数据格式转换
 *    - 数据统计和分析
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

#include "WtDataWriter.h"                                         // 数据写入器头文件

#include "../Includes/WTSSessionInfo.hpp"                        // 交易时段信息类
#include "../Includes/WTSContractInfo.hpp"                       // 合约信息类
#include "../Includes/WTSDataDef.hpp"                             // 数据定义
#include "../Includes/WTSVariant.hpp"                             // 变体数据类型
#include "../Share/BoostFile.hpp"                                 // Boost文件操作
#include "../Share/StrUtil.hpp"                                   // 字符串工具函数
#include "../Share/IniHelper.hpp"                                 // INI文件辅助工具
#include "../Share/decimal.h"                                      // 十进制数处理
#include "../Share/TimeUtils.hpp"                                 // 时间工具函数

#include "../Includes/IBaseDataMgr.h"                             // 基础数据管理器接口
#include "../WTSUtils/WTSCmpHelper.hpp"                          // 数据压缩辅助工具

#include <set>                                                    // 集合容器
#include <algorithm>                                              // 算法库

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                                      // 格式化库

/*!
 * \brief 数据写入器日志记录模板函数
 * \tparam Args 可变参数类型
 * \param sink 日志回调接口
 * \param ll 日志级别
 * \param format 格式化字符串
 * \param args 格式化参数
 * 
 * 该函数用于数据写入器的日志记录，支持格式化字符串和可变参数。
 * 使用fmtutil::format进行字符串格式化，提供高效的日志输出。
 */
/*!
 * \brief 数据写入器日志记录模板函数
 * \tparam Args 可变参数类型
 * \param sink 日志回调接口
 * \param ll 日志级别
 * \param format 格式化字符串
 * \param args 格式化参数
 * 
 * 该函数用于数据写入器的日志记录，支持格式化字符串和可变参数。
 * 使用fmtutil::format进行字符串格式化，提供高效的日志输出。
 */
template<typename... Args>
inline void pipe_writer_log(IDataWriterSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                                              // 如果日志回调接口为空
		return;                                                    // 直接返回

	const char* buffer = fmtutil::format(format, args...);         // 格式化字符串

	sink->outputLog(ll, buffer);                                  // 调用日志回调接口输出日志
}

/*!
 * \brief 处理数据块数据的外部函数声明
 * \param content 数据内容
 * \param isBar 是否为K线数据
 * \param bKeepHead 是否保留头部信息
 * \return 处理是否成功
 * 
 * 该函数负责处理各种类型的数据块，包括数据压缩、格式转换等。
 */
extern bool proc_block_data(std::string& content, bool isBar, bool bKeepHead = true);

extern "C"
{
	/*!
	 * \brief 创建数据写入器实例
	 * \return 数据写入器接口指针
	 * 
	 * 该函数用于创建WtDataWriter实例，通过C接口导出供外部调用。
	 * 返回的指针需要调用deleteWriter函数释放内存。
	 */
	EXPORT_FLAG IDataWriter* createWriter()
	{
		IDataWriter* ret = new WtDataWriter();                     // 创建WtDataWriter实例
		return ret;                                                // 返回数据写入器接口指针
	}

	/*!
	 * \brief 删除数据写入器实例
	 * \param writer 数据写入器指针引用
	 * 
	 * 该函数用于释放数据写入器实例，确保内存得到正确释放。
	 */
	EXPORT_FLAG void deleteWriter(IDataWriter* &writer)
	{
		if (writer != NULL)                                        // 如果写入器指针不为空
		{
			delete writer;                                         // 删除写入器实例
			writer = NULL;                                          // 将指针设置为空
		}
	}
};

static const uint32_t CACHE_SIZE_STEP = 200;                     // 缓存大小步长
static const uint32_t HFT_SIZE_STEP = 2500;                      // 高频交易大小步长

const char CMD_CLEAR_CACHE[] = "CMD_CLEAR_CACHE";                 // 清除缓存命令
const char MARKER_FILE[] = "marker.ini";                         // 标记文件名

/*!
 * \brief 任务信息构造函数
 * \param data 任务对象
 * \param dtype 任务类型
 * \param flag 任务标志
 * 
 * 该构造函数用于创建任务信息对象，包含任务对象、类型和标志。
 * 会自动增加对象的引用计数。
 */
WtDataWriter::_TaskInfo::_TaskInfo(WTSObject* data, uint64_t dtype, uint32_t flag/* = 0*/)
	: _type(dtype), _flag(flag)                                   // 初始化任务类型和标志
{
	_obj = data;                                                  // 设置任务对象
	_obj->retain();                                               // 增加对象引用计数
}

/*!
 * \brief 任务信息拷贝构造函数
 * \param rhs 源任务信息
 * 
 * 该拷贝构造函数用于创建任务信息的副本，会自动增加对象的引用计数。
 */
WtDataWriter::_TaskInfo::_TaskInfo(const _TaskInfo& rhs)
	: _type(rhs._type), _flag(rhs._flag)                          // 拷贝任务类型和标志
{
	_obj = rhs._obj;                                              // 拷贝任务对象
	_obj->retain();                                               // 增加对象引用计数
}

/*!
 * \brief 任务信息析构函数
 * 
 * 该析构函数用于释放任务信息对象，会自动减少对象的引用计数。
 */
WtDataWriter::_TaskInfo::~_TaskInfo() 
{ 
	_obj->release();                                              // 减少对象引用计数
}


/*!
 * \brief WtDataWriter构造函数
 * 
 * 该构造函数用于初始化数据写入器的所有成员变量，设置默认值。
 * 包括各种功能开关、配置参数等。
 */
WtDataWriter::WtDataWriter()
	: _terminated(false)                                          // 初始化终止标志为false
	, _save_tick_log(false)                                       // 初始化Tick日志保存为false
	, _log_group_size(1000)                                        // 初始化日志组大小为1000
	, _disable_day(false)                                         // 初始化日K线禁用为false
	, _disable_min1(false)                                        // 初始化1分钟K线禁用为false
	, _disable_min5(false)                                        // 初始化5分钟K线禁用为false
	, _disable_orddtl(false)                                      // 初始化委托详情禁用为false
	, _disable_ordque(false)                                      // 初始化委托队列禁用为false
	, _disable_trans(false)                                       // 初始化逐笔成交禁用为false
	, _disable_tick(false)                                        // 初始化Tick数据禁用为false
	, _disable_his(false)                                         // 初始化历史数据禁用为false
	, _skip_notrade_tick(false)                                   // 初始化跳过无交易Tick为false
	, _skip_notrade_bar(false)                                    // 初始化跳过无交易K线为false
{
}

/*!
 * \brief WtDataWriter析构函数
 * 
 * 该析构函数用于释放数据写入器占用的所有资源，包括内存、文件句柄、线程等。
 * 确保程序正常退出时资源得到正确释放。
 */
WtDataWriter::~WtDataWriter()
{
}

/*!
 * \brief 检查交易时段是否已处理
 * \param sid 合约标识
 * \return 是否已处理
 * 
 * 该函数用于检查指定合约的交易时段是否已经处理完成，避免重复处理。
 * 通过比较处理日期和当前日期来判断。
 */
bool WtDataWriter::isSessionProceeded(const char* sid)
{
	auto it = _proc_date.find(sid);                               // 查找合约的处理日期
	if (it == _proc_date.end())                                   // 如果未找到处理记录
		return false;                                             // 返回未处理

	return (it->second >= TimeUtils::getCurDate());                // 比较处理日期和当前日期
}

/*!
 * \brief 初始化数据写入器
 * \param params 配置参数
 * \param sink 数据写入回调接口
 * \return 是否初始化成功
 * 
 * 该函数负责初始化数据写入器，包括配置参数解析、资源分配、线程启动等。
 * 是数据写入器正常工作的前提条件。
 */
bool WtDataWriter::init(WTSVariant* params, IDataWriterSink* sink)
{
	IDataWriter::init(params, sink);                               // 调用基类初始化

	_bd_mgr = sink->getBDMgr();                                   // 获取基础数据管理器
	_save_tick_log = params->getBoolean("savelog");               // 获取Tick日志保存配置

	_base_dir = StrUtil::standardisePath(params->getCString("path")); // 获取基础目录路径
	if (!BoostFile::exists(_base_dir.c_str()))                    // 如果目录不存在
		BoostFile::create_directories(_base_dir.c_str());          // 创建目录
	_cache_file = params->getCString("cache");                    // 获取缓存文件配置
	if (_cache_file.empty())                                      // 如果缓存文件名为空
		_cache_file = "cache.dmb";                                 // 设置默认缓存文件名

	_async_proc = params->getBoolean("async");                     // 获取异步处理配置
	_log_group_size = params->getUInt32("groupsize");             // 获取日志组大小配置

	// 没有成交的tick在有些数据源中不会用于更新bar,这里做一下细分
	// 即便没有成交的tick，但仍然会产生一个bar，价格延续前一个bar，参考快期，万德
	_skip_notrade_tick = params->getBoolean("skip_notrade_tick"); // 获取跳过无交易Tick配置
	// 如果一个bar内没有一个有成交的tick，则不会有这个bar，参考掘金,MC
	_skip_notrade_bar = params->getBoolean("skip_notrade_bar");   // 获取跳过无交易K线配置

	//禁用历史数据
	_disable_his = params->getBoolean("disablehis");               // 获取禁用历史数据配置

	_disable_tick = params->getBoolean("disabletick");             // 获取禁用Tick数据配置
	_disable_min1 = params->getBoolean("disablemin1");             // 获取禁用1分钟K线配置
	_disable_min5 = params->getBoolean("disablemin5");             // 获取禁用5分钟K线配置
	_disable_day = params->getBoolean("disableday");               // 获取禁用日K线配置

	_disable_trans = params->getBoolean("disabletrans");            // 获取禁用逐笔成交配置
	_disable_ordque = params->getBoolean("disableordque");         // 获取禁用委托队列配置
	_disable_orddtl = params->getBoolean("disableorddtl");         // 获取禁用委托详情配置

	_min_price_mode = params->getUInt32("minbar_price_mode");       // 获取分钟线价格模式配置

	{
		std::string filename = _base_dir + MARKER_FILE;            // 构建标记文件路径
		IniHelper iniHelper;                                       // 创建INI文件辅助工具
		iniHelper.load(filename.c_str());                          // 加载标记文件
		StringVector ayKeys, ayVals;                               // 创建键值对向量
		iniHelper.readSecKeyValArray("markers", ayKeys, ayVals);    // 读取标记数据
		for (uint32_t idx = 0; idx < ayKeys.size(); idx++)        // 遍历键值对
		{
			_proc_date[ayKeys[idx].c_str()] = strtoul(ayVals[idx].c_str(), 0, 10); // 解析处理日期
		}
	}

	loadCache();                                                   // 加载缓存数据

	_proc_chk.reset(new StdThread(boost::bind(&WtDataWriter::check_loop, this))); // 启动检查线程

	pipe_writer_log(sink, LL_INFO, "WtDataWriter initialized, root dir: {}, save_csv_tick: {}, async_mode: {}, log_group_size: {}, disable_history: {}, "
		"disable_tick: {}, disable_min1: {}, disable_min5: {}, disable_day: {}, disable_trans: {}, disable_ordque: {}, disable_orders: {}, min_price_mode: {}", 
		_base_dir, _save_tick_log, _async_proc, _log_group_size, _disable_his, _disable_tick, 
		_disable_min1, _disable_min5, _disable_day, _disable_trans, _disable_ordque, _disable_orddtl, _min_price_mode); // 输出初始化日志
	return true;                                                   // 返回初始化成功
}

/*!
 * \brief 释放数据写入器资源
 * 
 * 该函数负责释放数据写入器占用的所有资源，包括内存、文件句柄、线程等。
 * 确保程序正常退出时资源得到正确释放。
 */
void WtDataWriter::release()
{
	_terminated = true;                                            // 设置终止标志
	if (_proc_thrd)                                                // 如果处理线程存在
	{
		_proc_cond.notify_all();                                   // 通知所有等待的线程
		_proc_thrd->join();                                        // 等待处理线程结束
	}

	for(auto& v : _rt_ticks_blocks)                                // 遍历Tick数据块
	{
		delete v.second;                                           // 删除Tick数据块
	}

	for (auto& v : _rt_trans_blocks)                              // 遍历逐笔成交数据块
	{
		delete v.second;                                           // 删除逐笔成交数据块
	}

	for (auto& v : _rt_orddtl_blocks)                              // 遍历委托详情数据块
	{
		delete v.second;                                           // 删除委托详情数据块
	}

	for (auto& v : _rt_ordque_blocks)                             // 遍历委托队列数据块
	{
		delete v.second;                                            // 删除委托队列数据块
	}

	for (auto& v : _rt_min1_blocks)                                // 遍历1分钟K线数据块
	{
		delete v.second;
	}

	for (auto& v : _rt_min5_blocks)
	{
		delete v.second;
	}
}

/*
void DataManager::preloadRtCaches(const char* exchg)
{
	if (!_preload_enable || _preloaded)
		return;

	pipe_writer_log(_sink, LL_INFO, "开始预加载实时数据缓存文件...");
	TimeUtils::Ticker ticker;
	uint32_t cnt = 0;
	uint32_t codecnt = 0;
	WTSArray* ayCts = _bd_mgr->getContracts(exchg);
	if (ayCts != NULL && ayCts->size() > 0)
	{
		for (auto it = ayCts->begin(); it != ayCts->end(); it++)
		{
			WTSContractInfo* ct = (WTSContractInfo*)(*it);
			if (ct == NULL)
				continue;
			WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(ct);
			if(commInfo == NULL)
				continue;

			bool isStk = (commInfo->getCategoty() == CC_Stock);
			codecnt++;
			
			releaseBlock(getTickBlock(ct->getCode(), TimeUtils::getCurDate(), true));
			releaseBlock(getKlineBlock(ct->getCode(), KP_Minute1, true));
			releaseBlock(getKlineBlock(ct->getCode(), KP_Minute5, true));
			cnt += 3;
			if (isStk && strcmp(commInfo->getProduct(), "STK") == 0)
			{
				releaseBlock(getOrdQueBlock(ct->getCode(), TimeUtils::getCurDate(), true));
				releaseBlock(getTransBlock(ct->getCode(), TimeUtils::getCurDate(), true));
				cnt += 2;
				if (strcmp(ct->getExchg(), "SZSE") == 0)
				{
					releaseBlock(getOrdDtlBlock(ct->getCode(), TimeUtils::getCurDate(), true));
					cnt++;
				}
			}
		}
	}

	if (ayCts != NULL)
		ayCts->release();
	pipe_writer_log(_sink, LL_INFO, "预加载%个品种的实时数据缓存文件{}个,耗时{}微秒", codecnt, cnt, WTSLogger::fmtInt64(ticker.micro_seconds()));
	_preloaded = true;
}
*/

/*!
 * \brief 加载缓存数据
 * 
 * 该函数负责从磁盘加载缓存数据到内存中，用于系统启动时的数据恢复。
 * 如果缓存文件不存在，会创建新的缓存文件。
 */
void WtDataWriter::loadCache()
{
	if (_tick_cache_file != NULL)                                 // 如果缓存文件已加载
		return;                                                    // 直接返回

	uint32_t TOTAL_CODES = _bd_mgr->getContractSize("", TimeUtils::getCurDate()); // 获取合约总数

	bool bNew = false;                                             // 是否为新文件标志
	std::string filename = _base_dir + _cache_file;                // 构建缓存文件路径
	if (!BoostFile::exists(filename.c_str()))                     // 如果缓存文件不存在
	{
		uint64_t uSize = sizeof(RTTickCache) + sizeof(TickCacheItem) * TOTAL_CODES; // 计算文件大小
		BoostFile bf;                                              // 创建文件对象
		bf.create_new_file(filename.c_str());                      // 创建新文件
		bf.truncate_file((uint32_t)uSize);                         // 设置文件大小
		bf.close_file();                                           // 关闭文件
		bNew = true;                                               // 设置为新文件标志
	}

	_tick_cache_file.reset(new BoostMappingFile);                 // 创建内存映射文件对象
	_tick_cache_file->map(filename.c_str());                      // 映射文件到内存
	_tick_cache_block = (RTTickCache*)_tick_cache_file->addr();   // 获取缓存块指针
	_tick_cache_block->_size = min(_tick_cache_block->_size, _tick_cache_block->_capacity); // 设置缓存大小

	if(bNew)                                                       // 如果是新文件
	{
		memset(_tick_cache_block, 0, _tick_cache_file->size());    // 清空缓存块

		_tick_cache_block->_capacity = TOTAL_CODES;                // 设置缓存容量
		_tick_cache_block->_type = BT_RT_Cache;                    // 设置块类型
		_tick_cache_block->_size = 0;                              // 设置块大小
		_tick_cache_block->_version = 1;                           // 设置版本号
		strcpy(_tick_cache_block->_blk_flag, BLK_FLAG);            // 设置块标志
	}
	else                                                           // 如果是已存在的文件
	{
		for (uint32_t i = 0; i < _tick_cache_block->_size; i++)    // 遍历缓存项
		{
			const TickCacheItem& item = _tick_cache_block->_ticks[i];
			std::string key = fmt::format("{}.{}", item._tick.exchg, item._tick.code);
			_tick_cache_idx[key] = i;
		}
	}
}

template<typename HeaderType, typename T>
void* WtDataWriter::resizeRTBlock(BoostMFPtr& mfPtr, uint32_t nCount)
{
	if (mfPtr == NULL)
		return NULL;

	//调用该函数之前,应该已经申请了写锁了
	RTBlockHeader* tBlock = (RTBlockHeader*)mfPtr->addr();
	if (tBlock->_capacity >= nCount)
		return mfPtr->addr();

	std::string filename = mfPtr->filename();
	uint64_t uOldSize = sizeof(HeaderType) + sizeof(T)*tBlock->_capacity;
	uint64_t uNewSize = sizeof(HeaderType) + sizeof(T)*nCount;
	std::string data;
	data.resize((std::size_t)(uNewSize - uOldSize), 0);
	try
	{
		BoostFile f;
		f.open_existing_file(filename.c_str());
		f.seek_to_end();
		f.write_file(data.c_str(), data.size());
		f.close_file();
	}
	catch(std::exception& ex)
	{
		pipe_writer_log(_sink, LL_ERROR, "Exception occured while expanding RT cache file {} to {}: {}", filename, uNewSize, ex.what());
		return NULL;
	}


	mfPtr.reset();
	BoostMappingFile* pNewMf = new BoostMappingFile();
	try
	{
		if (!pNewMf->map(filename.c_str()))
		{
			delete pNewMf;
			return NULL;
		}
	}
	catch (std::exception& ex)
	{
		pipe_writer_log(_sink, LL_ERROR, "Exception occured while mapping RT cache file {}: {}", filename, ex.what());
		return NULL;
	}	

	mfPtr.reset(pNewMf);

	tBlock = (RTBlockHeader*)mfPtr->addr();
	tBlock->_capacity = nCount;
	return mfPtr->addr();
}

/*!
 * \brief 写入Tick数据
 * \param curTick 当前Tick数据
 * \param procFlag 处理标志
 * \return 是否写入成功
 * 
 * 该函数负责将Tick数据写入到存储系统中，支持实时数据写入和历史数据归档。
 * 包括数据验证、格式转换、存储等步骤。
 */
bool WtDataWriter::writeTick(WTSTickData* curTick, uint32_t procFlag)
{
	if (curTick == NULL)                                          // 如果Tick数据为空
		return false;                                              // 返回失败

	if (_async_proc)                                              // 如果启用异步处理
		pushTask(TaskInfo(curTick, 0, procFlag));                // 推送任务到队列
	else                                                          // 如果同步处理
		procTick(curTick, procFlag);                              // 直接处理Tick数据

	return true;                                                  // 返回成功
}

/*!
 * \brief 处理Tick数据
 * \param curTick 当前Tick数据
 * \param procFlag 处理标志
 * 
 * 该函数负责处理Tick数据，包括数据验证、格式转换、存储等。
 * 支持缓存更新、K线生成、数据广播等功能。
 */
void WtDataWriter::procTick(WTSTickData* curTick, uint32_t procFlag)
{
	do
	{
		WTSContractInfo* ct = curTick->getContractInfo();         // 获取合约信息
		if (ct == NULL)                                           // 如果合约信息为空
			break;                                                // 跳出循环

		WTSCommodityInfo* commInfo = ct->getCommInfo();           // 获取商品信息

		//再根据状态过滤
		if (!_sink->canSessionReceive(commInfo->getSession()))    // 如果会话不能接收数据
			break;                                                // 跳出循环

		//先更新缓存
		if (!updateCache(ct, curTick, procFlag))                   // 如果缓存更新失败
			break;                                                // 跳出循环

		//写到tick缓存
		if (!_disable_tick)                                       // 如果未禁用Tick数据
			pipeToTicks(ct, curTick);                             // 将数据传递给Tick处理

		//写到K线缓存
		pipeToKlines(ct, curTick);                                // 将数据传递给K线处理

		_sink->broadcastTick(curTick);                            // 广播Tick数据

		static wt_hashmap<std::string, uint64_t> _tcnt_map;       // 静态计数器映射表
		uint64_t& cnt = _tcnt_map[curTick->exchg()];             // 获取交易所计数器
		cnt++;                                                    // 增加计数
		if (cnt % _log_group_size == 0)                           // 如果达到日志组大小
		{
			pipe_writer_log(_sink, LL_INFO, "{} ticks received from exchange {}", cnt, curTick->exchg()); // 输出日志
		}
	} while (false);                                              // 只执行一次
}

/*!
 * \brief 写入委托队列数据
 * \param curOrdQue 当前委托队列数据
 * \return 是否写入成功
 * 
 * 该函数负责将委托队列数据写入到存储系统中，用于记录市场深度信息。
 */
bool WtDataWriter::writeOrderQueue(WTSOrdQueData* curOrdQue)
{
	if (curOrdQue == NULL || _disable_ordque)                      // 如果委托队列数据为空或已禁用
		return false;                                              // 返回失败

	if (_async_proc)                                               // 如果启用异步处理
		pushTask(TaskInfo(curOrdQue, 1));                         // 推送任务到队列
	else                                                           // 如果同步处理
		procQueue(curOrdQue);                                     // 直接处理委托队列数据

	return true;                                                   // 返回成功
}

/*!
 * \brief 处理委托队列数据
 * \param curOrdQue 当前委托队列数据
 * 
 * 该函数负责处理委托队列数据，包括数据验证、格式转换、存储等。
 */
void WtDataWriter::procQueue(WTSOrdQueData* curOrdQue)
{
	do
	{
		WTSContractInfo* ct = curOrdQue->getContractInfo();        // 获取合约信息
		WTSCommodityInfo* commInfo = ct->getCommInfo();           // 获取商品信息

		//再根据状态过滤
		if (!_sink->canSessionReceive(commInfo->getSession()))    // 如果会话不能接收数据
			break;                                                // 跳出循环

		OrdQueBlockPair* pBlockPair = getOrdQueBlock(ct, curOrdQue->tradingdate());
		if (pBlockPair == NULL)
			break;

		SpinLock lock(pBlockPair->_mutex);

		//先检查容量够不够,不够要扩
		RTOrdQueBlock* blk = pBlockPair->_block;
		if (blk->_size >= blk->_capacity)
		{
			pBlockPair->_file->sync();
			pBlockPair->_block = (RTOrdQueBlock*)resizeRTBlock<RTDayBlockHeader, WTSOrdQueStruct>(pBlockPair->_file, blk->_capacity * 2);
			blk = pBlockPair->_block;
		}

		memcpy(&blk->_queues[blk->_size], &curOrdQue->getOrdQueStruct(), sizeof(WTSOrdQueStruct));
		blk->_size += 1;

		_sink->broadcastOrdQue(curOrdQue);

		static wt_hashmap<std::string, uint64_t> _tcnt_map;
		uint64_t& cnt = _tcnt_map[curOrdQue->exchg()];
		cnt++;
		if (cnt % _log_group_size == 0)
		{
			pipe_writer_log(_sink, LL_INFO, "{} queues received from exchange {}", cnt, curOrdQue->exchg());
		}
	} while (false);
}

bool WtDataWriter::writeOrderDetail(WTSOrdDtlData* curOrdDtl)
{
	if (curOrdDtl == NULL || _disable_orddtl)
		return false;

	if (_async_proc)
		pushTask(TaskInfo(curOrdDtl, 2));
	else
		procOrder(curOrdDtl);

	return true;
}

void WtDataWriter::procOrder(WTSOrdDtlData* curOrdDtl)
{
	do
	{
		WTSContractInfo* ct = curOrdDtl->getContractInfo();
		WTSCommodityInfo* commInfo = ct->getCommInfo();

		//再根据状态过滤
		if (!_sink->canSessionReceive(commInfo->getSession()))
			break;

		OrdDtlBlockPair* pBlockPair = getOrdDtlBlock(ct, curOrdDtl->tradingdate());
		if (pBlockPair == NULL)
			break;

		SpinLock lock(pBlockPair->_mutex);

		//先检查容量够不够,不够要扩
		RTOrdDtlBlock* blk = pBlockPair->_block;
		if (blk->_size >= blk->_capacity)
		{
			pBlockPair->_file->sync();
			pBlockPair->_block = (RTOrdDtlBlock*)resizeRTBlock<RTDayBlockHeader, WTSOrdDtlStruct>(pBlockPair->_file, blk->_capacity * 2);
			blk = pBlockPair->_block;
		}

		memcpy(&blk->_details[blk->_size], &curOrdDtl->getOrdDtlStruct(), sizeof(WTSOrdDtlStruct));
		blk->_size += 1;

		_sink->broadcastOrdDtl(curOrdDtl);

		static wt_hashmap<std::string, uint64_t> _tcnt_map;
		uint64_t& cnt = _tcnt_map[curOrdDtl->exchg()];
		cnt++;
		if (cnt % _log_group_size == 0)
		{
			pipe_writer_log(_sink, LL_INFO, "{} orders received from exchange {}", cnt, curOrdDtl->exchg());
		}
	} while (false);
}

/*!
 * \brief 写入逐笔成交数据
 * \param curTrans 当前逐笔成交数据
 * \return 是否写入成功
 * 
 * 该函数负责将逐笔成交数据写入到存储系统中，用于记录每笔交易的详细信息。
 */
bool WtDataWriter::writeTransaction(WTSTransData* curTrans)
{
	if (curTrans == NULL || _disable_orddtl)                      // 如果逐笔成交数据为空或已禁用
		return false;                                              // 返回失败

	if (_async_proc)                                               // 如果启用异步处理
		pushTask(TaskInfo(curTrans, 3));                         // 推送任务到队列
	else                                                           // 如果同步处理
		procTrans(curTrans);                                      // 直接处理逐笔成交数据

	return true;                                                   // 返回成功
}

/*!
 * \brief 处理逐笔成交数据
 * \param curTrans 当前逐笔成交数据
 * 
 * 该函数负责处理逐笔成交数据，包括数据验证、格式转换、存储等。
 */
void WtDataWriter::procTrans(WTSTransData* curTrans)
{
	do
	{
		WTSContractInfo* ct = curTrans->getContractInfo();         // 获取合约信息
		WTSCommodityInfo* commInfo = ct->getCommInfo();           // 获取商品信息

		//再根据状态过滤
		if (!_sink->canSessionReceive(commInfo->getSession()))    // 如果会话不能接收数据
			break;                                                // 跳出循环

		TransBlockPair* pBlockPair = getTransBlock(ct, curTrans->tradingdate()); // 获取逐笔成交数据块
		if (pBlockPair == NULL)                                   // 如果数据块为空
			break;                                                // 跳出循环

		SpinLock lock(pBlockPair->_mutex);                        // 获取数据块锁

		//先检查容量够不够,不够要扩
		RTTransBlock* blk = pBlockPair->_block;                   // 获取数据块指针
		if (blk->_size >= blk->_capacity)                         // 如果数据块已满
		{
			pBlockPair->_file->sync();                            // 同步文件
			pBlockPair->_block = (RTTransBlock*)resizeRTBlock<RTDayBlockHeader, WTSTransStruct>(pBlockPair->_file, blk->_capacity * 2); // 扩展数据块
			blk = pBlockPair->_block;                             // 更新数据块指针
		}

		memcpy(&blk->_trans[blk->_size], &curTrans->getTransStruct(), sizeof(WTSTransStruct)); // 复制数据到数据块
		blk->_size += 1;                                          // 增加数据块大小

		_sink->broadcastTrans(curTrans);                          // 广播逐笔成交数据

		static wt_hashmap<std::string, uint64_t> _tcnt_map;       // 静态计数器映射表
		uint64_t& cnt = _tcnt_map[curTrans->exchg()];            // 获取交易所计数器
		cnt++;                                                    // 增加计数
		if (cnt % _log_group_size == 0)                           // 如果达到日志组大小
		{
			pipe_writer_log(_sink, LL_INFO, "{} transactions received from exchange {}", cnt, curTrans->exchg()); // 输出日志
		}
	} while (false);                                              // 只执行一次
}

/*!
 * \brief 推送任务到队列
 * \param task 任务信息
 * 
 * 该函数负责将任务推送到任务队列中，用于异步任务处理。
 * 如果任务处理线程未启动，会自动启动。
 */
void WtDataWriter::pushTask(const TaskInfo& task)
{
	if (!_async_proc)                                             // 如果未启用异步处理
		return;                                                    // 直接返回

	StdUniqueLock lck(_task_mtx);                                 // 获取任务队列锁
	_tasks.emplace(task);                                         // 将任务添加到队列
	_task_cond.notify_all();                                      // 通知所有等待的线程

	if (_task_thrd == NULL)                                       // 如果任务处理线程未启动
	{
		_task_thrd.reset(new StdThread([this]() {                 // 创建任务处理线程
			while (!_terminated)                                  // 当未终止时循环
			{
				if (_tasks.empty())                              // 如果任务队列为空
				{
					StdUniqueLock lck(_task_mtx);                 // 获取任务队列锁
					_task_cond.wait(_task_mtx);                   // 等待任务通知
					continue;                                      // 继续循环
				}

				std::queue<TaskInfo> tempQueue;                   // 创建临时任务队列
				{
					StdUniqueLock lck(_task_mtx);                 // 获取任务队列锁
					tempQueue.swap(_tasks);                       // 交换任务队列
				}

				while (!tempQueue.empty())                       // 当临时队列不为空时循环
				{
					TaskInfo& curTask = tempQueue.front();        // 获取当前任务
					switch (curTask._type)                       // 根据任务类型处理
					{
					case 0: procTick((WTSTickData*)curTask._obj, curTask._flag); break; // 处理Tick数据
					case 1: procQueue((WTSOrdQueData*)curTask._obj); break;              // 处理委托队列数据
					case 2: procOrder((WTSOrdDtlData*)curTask._obj); break;              // 处理委托详情数据
					case 3: procTrans((WTSTransData*)curTask._obj); break;               // 处理逐笔成交数据
					default:
						break;                                                          // 默认情况
					}
					tempQueue.pop();                                                   // 移除已处理的任务
				}
			}
		}));
	}
}

/*!
 * \brief 将数据传递给Tick处理
 * \param ct 合约信息
 * \param curTick 当前Tick数据
 * 
 * 该函数负责将Tick数据传递给Tick处理模块，用于实时数据处理。
 * 包括数据块管理、容量检查、数据写入和日志记录等功能。
 */
void WtDataWriter::pipeToTicks(WTSContractInfo* ct, WTSTickData* curTick)
{
	TickBlockPair* pBlockPair = getTickBlock(ct, curTick->tradingdate()); // 获取Tick数据块
	if (pBlockPair == NULL)                                   // 如果数据块为空
		return;                                                // 直接返回

	SpinLock lock(pBlockPair->_mutex);                        // 获取数据块锁

	//先检查容量够不够,不够要扩
	RTTickBlock* blk = pBlockPair->_block;                    // 获取数据块指针
	if(blk && blk->_size >= blk->_capacity)                   // 如果数据块已满
	{
		pBlockPair->_file->sync();                            // 同步文件
		pBlockPair->_block = (RTTickBlock*)resizeRTBlock<RTDayBlockHeader, WTSTickStruct>(pBlockPair->_file, blk->_capacity * 2); // 扩展数据块
		blk = pBlockPair->_block;                             // 更新数据块指针
		if(blk) pipe_writer_log(_sink, LL_DEBUG, "RT tick block of {} resized to {}", ct->getFullCode(), blk->_capacity); // 输出调试日志
	}
	
	if (blk == NULL)                                          // 如果数据块为空
	{
		pipe_writer_log(_sink, LL_DEBUG, "RT tick block of {} is not valid", ct->getFullCode()); // 输出调试日志
		return;                                                // 直接返回
	}

	memcpy(&blk->_ticks[blk->_size], &curTick->getTickStruct(), sizeof(WTSTickStruct)); // 复制数据到数据块
	blk->_size += 1;                                          // 增加数据块大小

	if(_save_tick_log && pBlockPair->_fstream)                // 如果需要保存Tick日志且文件流存在
	{
		*(pBlockPair->_fstream) << curTick->code() << ","     // 写入合约代码
			<< curTick->tradingdate() << ","                  // 写入交易日期
			<< curTick->actiondate() << ","                   // 写入动作日期
			<< curTick->actiontime() << ","                   // 写入动作时间
			<< TimeUtils::getLocalTime(false) << ","          // 写入本地时间
			<< curTick->price() << ","                        // 写入价格
			<< curTick->totalvolume() << ","                  // 写入总成交量
			<< curTick->openinterest() << ","                 // 写入持仓量
			<< (uint64_t)curTick->totalturnover() << ","      // 写入总成交额
			<< curTick->volume() << ","                       // 写入成交量
			<< curTick->additional() << ","                   // 写入附加信息
			<< (uint64_t)curTick->turnover() << std::endl;    // 写入成交额
	}
}

/*!
 * \brief 获取委托队列数据块
 * \param ct 合约信息
 * \param curDate 当前日期
 * \param bAutoCreate 是否自动创建
 * \return 委托队列数据块指针
 * 
 * 该函数负责获取或创建指定日期的委托队列数据块，用于委托队列数据存储。
 * 支持自动创建文件、内存映射、数据恢复等功能。
 */
WtDataWriter::OrdQueBlockPair* WtDataWriter::getOrdQueBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate /* = true */)
{
	if (ct == NULL)                                             // 如果合约信息为空
		return NULL;                                            // 返回空指针

	OrdQueBlockPair* pBlock = NULL;                             // 数据块对指针
	const char* key = ct->getFullCode();                         // 获取合约完整代码
	pBlock = _rt_ordque_blocks[key];                             // 从映射表中获取数据块
	if(pBlock == NULL)                                          // 如果数据块不存在
	{
		pBlock = new OrdQueBlockPair();                         // 创建新的数据块对
		_rt_ordque_blocks[key] = pBlock;                        // 添加到映射表
	}

	if (pBlock->_block == NULL)                                 // 如果数据块为空
	{
		std::string path = fmt::format("{}rt/queue/{}/", _base_dir.c_str(), ct->getExchg()); // 构建文件路径
		if (bAutoCreate)                                        // 如果允许自动创建
			BoostFile::create_directories(path.c_str());        // 创建目录
		path += ct->getCode();                                   // 添加合约代码
		path += ".dmb";                                          // 添加文件扩展名

		bool isNew = false;                                      // 是否为新文件标志
		if (!BoostFile::exists(path.c_str()))                   // 如果文件不存在
		{
			if (!bAutoCreate)                                    // 如果不允许自动创建
				return NULL;                                     // 返回空指针

			pipe_writer_log(_sink, LL_INFO, "Data file {} not exists, initializing...", path.c_str()); // 输出日志

			uint64_t uSize = sizeof(RTDayBlockHeader) + sizeof(WTSOrdQueStruct) * HFT_SIZE_STEP; // 计算文件大小

			BoostFile bf;                                        // 创建文件对象
			bf.create_new_file(path.c_str());                    // 创建新文件
			bf.truncate_file((uint32_t)uSize);                   // 设置文件大小
			bf.close_file();                                     // 关闭文件

			isNew = true;                                        // 设置为新文件标志
		}

		pBlock->_file.reset(new BoostMappingFile);              // 创建内存映射文件对象
		if (!pBlock->_file->map(path.c_str()))                  // 如果映射文件失败
		{
			pipe_writer_log(_sink, LL_INFO, "Mapping file {} failed", path.c_str()); // 输出日志
			pBlock->_file.reset();                              // 重置文件指针
			return NULL;                                         // 返回空指针
		}
		pBlock->_block = (RTOrdQueBlock*)pBlock->_file->addr(); // 获取数据块指针

		if (!isNew &&  pBlock->_block->_date != curDate)        // 如果不是新文件且日期不匹配
		{
			pipe_writer_log(_sink, LL_INFO, "date[{}] of orderqueue cache block[{}] is different from current date[{}], reinitializing...", pBlock->_block->_date, path.c_str(), curDate); // 输出日志
			pBlock->_block->_size = 0;                          // 重置数据块大小
			pBlock->_block->_date = curDate;                    // 设置当前日期

			memset(&pBlock->_block->_queues, 0, sizeof(WTSOrdQueStruct)*pBlock->_block->_capacity); // 清空队列数据
		}

		if (isNew)                                               // 如果是新文件
		{
			pBlock->_block->_capacity = HFT_SIZE_STEP;           // 设置数据块容量
			pBlock->_block->_size = 0;                          // 设置数据块大小
			pBlock->_block->_version = BLOCK_VERSION_RAW_V2;     // 设置版本号
			pBlock->_block->_type = BT_RT_OrdQueue;              // 设置块类型
			pBlock->_block->_date = curDate;                     // 设置日期
			strcpy(pBlock->_block->_blk_flag, BLK_FLAG);         // 设置块标志
		}
		else                                                     // 如果是已存在的文件
		{
			//检查缓存文件是否有问题,要自动恢复
			do
			{
				uint64_t uSize = sizeof(RTDayBlockHeader) + sizeof(WTSOrdQueStruct) * pBlock->_block->_capacity; // 计算期望文件大小
				uint64_t oldSize = pBlock->_file->size();       // 获取实际文件大小
				if (oldSize != uSize)                           // 如果大小不匹配
				{
					uint32_t oldCnt = (uint32_t)((oldSize - sizeof(RTDayBlockHeader)) / sizeof(WTSOrdQueStruct)); // 计算实际容量
					//文件大小不匹配,一般是因为capacity改了,但是实际没扩容
					//这是做一次扩容即可
					pBlock->_block->_capacity = oldCnt;         // 设置实际容量
					pBlock->_block->_size = oldCnt;             // 设置实际大小

					pipe_writer_log(_sink, LL_WARN, "Oderqueue cache file of {} on date {} repaired", ct->getCode(), curDate); // 输出警告日志
				}

			} while (false);                                    // 只执行一次

		}
	}

	pBlock->_lasttime = TimeUtils::getLocalTimeNow() / 1000;     // 更新最后时间
	return pBlock;                                              // 返回数据块对指针
}

/*!
 * \brief 获取委托详情数据块
 * \param ct 合约信息
 * \param curDate 当前日期
 * \param bAutoCreate 是否自动创建
 * \return 委托详情数据块指针
 * 
 * 该函数负责获取或创建指定日期的委托详情数据块，用于委托详情数据存储。
 * 支持自动创建文件、内存映射、数据恢复等功能。
 */
WtDataWriter::OrdDtlBlockPair* WtDataWriter::getOrdDtlBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate /* = true */)
{
	if (ct == NULL)                                             // 如果合约信息为空
		return NULL;                                            // 返回空指针

	OrdDtlBlockPair* pBlock = NULL;                             // 数据块对指针
	const char* key = ct->getFullCode();                         // 获取合约完整代码
	pBlock = _rt_orddtl_blocks[key];                             // 从映射表中获取数据块
	if (pBlock == NULL)                                          // 如果数据块不存在
	{
		pBlock = new OrdDtlBlockPair();                         // 创建新的数据块对
		_rt_orddtl_blocks[key] = pBlock;                        // 添加到映射表
	}

	if (pBlock->_block == NULL)                                 // 如果数据块为空
	{
		std::string path = fmt::format("{}rt/orders/{}/", _base_dir.c_str(), ct->getExchg()); // 构建文件路径
		if(bAutoCreate)                                         // 如果允许自动创建
			BoostFile::create_directories(path.c_str());         // 创建目录
		path += ct->getCode();                                   // 添加合约代码
		path += ".dmb";                                          // 添加文件扩展名

		bool isNew = false;                                      // 是否为新文件标志
		if (!BoostFile::exists(path.c_str()))                   // 如果文件不存在
		{
			if (!bAutoCreate)                                    // 如果不允许自动创建
				return NULL;                                     // 返回空指针

			pipe_writer_log(_sink, LL_INFO, "Data file {} not exists, initializing...", path.c_str()); // 输出日志

			uint64_t uSize = sizeof(RTDayBlockHeader) + sizeof(WTSOrdDtlStruct) * HFT_SIZE_STEP; // 计算文件大小

			BoostFile bf;                                        // 创建文件对象
			bf.create_new_file(path.c_str());                    // 创建新文件
			bf.truncate_file((uint32_t)uSize);                   // 设置文件大小
			bf.close_file();                                     // 关闭文件

			isNew = true;                                        // 设置为新文件标志
		}

		pBlock->_file.reset(new BoostMappingFile);              // 创建内存映射文件对象
		if (!pBlock->_file->map(path.c_str()))                  // 如果映射文件失败
		{
			pipe_writer_log(_sink, LL_INFO, "Mapping file {} failed", path.c_str()); // 输出日志
			pBlock->_file.reset();                              // 重置文件指针
			return NULL;                                         // 返回空指针
		}
		pBlock->_block = (RTOrdDtlBlock*)pBlock->_file->addr(); // 获取数据块指针

		if (!isNew &&  pBlock->_block->_date != curDate)        // 如果不是新文件且日期不匹配
		{
			pipe_writer_log(_sink, LL_INFO, "date[{}] of orderdetail cache block[{}] is different from current date[{}], reinitializing...", pBlock->_block->_date, path.c_str(), curDate); // 输出日志
			pBlock->_block->_size = 0;                          // 重置数据块大小
			pBlock->_block->_date = curDate;                    // 设置当前日期

			memset(&pBlock->_block->_details, 0, sizeof(WTSOrdDtlStruct)*pBlock->_block->_capacity); // 清空详情数据
		}

		if (isNew)                                               // 如果是新文件
		{
			pBlock->_block->_capacity = HFT_SIZE_STEP;           // 设置数据块容量
			pBlock->_block->_size = 0;                          // 设置数据块大小
			pBlock->_block->_version = BLOCK_VERSION_RAW_V2;     // 设置版本号
			pBlock->_block->_type = BT_RT_OrdDetail;             // 设置块类型
			pBlock->_block->_date = curDate;                     // 设置日期
			strcpy(pBlock->_block->_blk_flag, BLK_FLAG);         // 设置块标志
		}
		else                                                     // 如果是已存在的文件
		{
			//检查缓存文件是否有问题,要自动恢复
			for (;;)                                             // 无限循环
			{
				uint64_t uSize = sizeof(RTDayBlockHeader) + sizeof(WTSOrdDtlStruct) * pBlock->_block->_capacity; // 计算期望文件大小
				uint64_t oldSize = pBlock->_file->size();       // 获取实际文件大小
				if (oldSize != uSize)                           // 如果大小不匹配
				{
					uint32_t oldCnt = (uint32_t)((oldSize - sizeof(RTDayBlockHeader)) / sizeof(WTSOrdDtlStruct)); // 计算实际容量
					//文件大小不匹配,一般是因为capacity改了,但是实际没扩容
					//这是做一次扩容即可
					pBlock->_block->_capacity = oldCnt;         // 设置实际容量
					pBlock->_block->_size = oldCnt;             // 设置实际大小

					pipe_writer_log(_sink, LL_WARN, "Orderdetail cache file of {} on date {} repaired", ct->getCode(), curDate); // 输出警告日志
				}

				break;                                           // 跳出循环
			}

		}
	}

	pBlock->_lasttime = TimeUtils::getLocalTimeNow() / 1000;     // 更新最后时间
	return pBlock;                                              // 返回数据块对指针
}

/*!
 * \brief 获取逐笔成交数据块
 * \param ct 合约信息
 * \param curDate 当前日期
 * \param bAutoCreate 是否自动创建
 * \return 逐笔成交数据块指针
 * 
 * 该函数负责获取或创建指定日期的逐笔成交数据块，用于逐笔成交数据存储。
 * 支持自动创建文件、内存映射、数据恢复等功能。
 */
WtDataWriter::TransBlockPair* WtDataWriter::getTransBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate /* = true */)
{
	if (ct == NULL)                                             // 如果合约信息为空
		return NULL;                                            // 返回空指针

	TransBlockPair* pBlock = NULL;                              // 数据块对指针
	const char* key = ct->getFullCode();                         // 获取合约完整代码
	pBlock = _rt_trans_blocks[key];                              // 从映射表中获取数据块
	if (pBlock == NULL)                                          // 如果数据块不存在
	{
		pBlock = new TransBlockPair();                          // 创建新的数据块对
		_rt_trans_blocks[key] = pBlock;                         // 添加到映射表
	}

	if (pBlock->_block == NULL)                                 // 如果数据块为空
	{
		std::string path = fmt::format("{}rt/trans/{}/", _base_dir.c_str(), ct->getExchg()); // 构建文件路径
		if (bAutoCreate)                                        // 如果允许自动创建
			BoostFile::create_directories(path.c_str());        // 创建目录
		path += ct->getCode();                                   // 添加合约代码
		path += ".dmb";                                          // 添加文件扩展名

		bool isNew = false;                                      // 是否为新文件标志
		if (!BoostFile::exists(path.c_str()))                   // 如果文件不存在
		{
			if (!bAutoCreate)                                    // 如果不允许自动创建
				return NULL;                                     // 返回空指针

			pipe_writer_log(_sink, LL_INFO, "Data file {} not exists, initializing...", path.c_str()); // 输出日志

			uint64_t uSize = sizeof(RTDayBlockHeader) + sizeof(WTSTransStruct) * HFT_SIZE_STEP; // 计算文件大小

			BoostFile bf;                                        // 创建文件对象
			bf.create_new_file(path.c_str());                    // 创建新文件
			bf.truncate_file((uint32_t)uSize);                   // 设置文件大小
			bf.close_file();                                     // 关闭文件

			isNew = true;                                        // 设置为新文件标志
		}

		pBlock->_file.reset(new BoostMappingFile);              // 创建内存映射文件对象
		if (!pBlock->_file->map(path.c_str()))                  // 如果映射文件失败
		{
			pipe_writer_log(_sink, LL_INFO, "Mapping file {} failed", path.c_str()); // 输出日志
			pBlock->_file.reset();                              // 重置文件指针
			return NULL;                                         // 返回空指针
		}
		pBlock->_block = (RTTransBlock*)pBlock->_file->addr();  // 获取数据块指针

		if (!isNew &&  pBlock->_block->_date != curDate)        // 如果不是新文件且日期不匹配
		{
			pipe_writer_log(_sink, LL_INFO, "date[{}] of transaction cache block[{}] is different from current date[{}], reinitializing...", pBlock->_block->_date, path.c_str(), curDate); // 输出日志
			pBlock->_block->_size = 0;                          // 重置数据块大小
			pBlock->_block->_date = curDate;                    // 设置当前日期

			memset(&pBlock->_block->_trans, 0, sizeof(WTSTransStruct)*pBlock->_block->_capacity); // 清空成交数据
		}

		if (isNew)                                               // 如果是新文件
		{
			pBlock->_block->_capacity = HFT_SIZE_STEP;           // 设置数据块容量
			pBlock->_block->_size = 0;                          // 设置数据块大小
			pBlock->_block->_version = BLOCK_VERSION_RAW_V2;     // 设置版本号
			pBlock->_block->_type = BT_RT_Trnsctn;               // 设置块类型
			pBlock->_block->_date = curDate;                     // 设置日期
			strcpy(pBlock->_block->_blk_flag, BLK_FLAG);         // 设置块标志
		}
		else                                                     // 如果是已存在的文件
		{
			//检查缓存文件是否有问题,要自动恢复
			for (;;)                                             // 无限循环
			{
				uint64_t uSize = sizeof(RTDayBlockHeader) + sizeof(WTSTransStruct) * pBlock->_block->_capacity; // 计算期望文件大小
				uint64_t oldSize = pBlock->_file->size();       // 获取实际文件大小
				if (oldSize != uSize)                           // 如果大小不匹配
				{
					uint32_t oldCnt = (uint32_t)((oldSize - sizeof(RTDayBlockHeader)) / sizeof(WTSTransStruct)); // 计算实际容量
					//文件大小不匹配,一般是因为capacity改了,但是实际没扩容
					//这是做一次扩容即可
					pBlock->_block->_capacity = oldCnt;         // 设置实际容量
					pBlock->_block->_size = oldCnt;             // 设置实际大小

					pipe_writer_log(_sink, LL_WARN, "Transaction cache file of {} on date {} repaired", ct->getCode(), curDate); // 输出警告日志
				}

				break;                                           // 跳出循环
			}

		}
	}

	pBlock->_lasttime = TimeUtils::getLocalTimeNow() / 1000;     // 更新最后时间
	return pBlock;                                              // 返回数据块对指针
}

/*!
 * \brief 获取Tick数据块
 * \param ct 合约信息
 * \param curDate 当前日期
 * \param bAutoCreate 是否自动创建
 * \return Tick数据块指针
 * 
 * 该函数负责获取或创建指定日期的Tick数据块，用于Tick数据存储。
 * 支持自动创建文件、内存映射、数据恢复、CSV日志记录等功能。
 */
WtDataWriter::TickBlockPair* WtDataWriter::getTickBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate /* = true */)
{
	if (ct == NULL)                                             // 如果合约信息为空
		return NULL;                                            // 返回空指针

	TickBlockPair* pBlock = NULL;                               // 数据块对指针
	const char* key = ct->getFullCode();                         // 获取合约完整代码
	pBlock = _rt_ticks_blocks[key];                              // 从映射表中获取数据块
	if (pBlock == NULL)                                          // 如果数据块不存在
	{
		pBlock = new TickBlockPair();                           // 创建新的数据块对
		_rt_ticks_blocks[key] = pBlock;                         // 添加到映射表
	}

	if(pBlock->_block == NULL)                                   // 如果数据块为空
	{
		std::string path = fmt::format("{}rt/ticks/{}/", _base_dir.c_str(), ct->getExchg()); // 构建文件路径
		if (bAutoCreate)                                         // 如果允许自动创建
			BoostFile::create_directories(path.c_str());        // 创建目录

		if(_save_tick_log)                                       // 如果需要保存Tick日志
		{
			std::stringstream fname;                            // 创建文件名流
			fname << path << ct->getCode() << "." << curDate << ".csv"; // 构建CSV文件名
			pBlock->_fstream.reset(new std::ofstream());         // 创建文件流对象
			pBlock->_fstream->open(fname.str().c_str(), std::ios_base::app); // 以追加模式打开文件
		}

		path += ct->getCode();                                   // 添加合约代码
		path += ".dmb";                                          // 添加文件扩展名

		bool isNew = false;                                      // 是否为新文件标志
		if (!BoostFile::exists(path.c_str()))                   // 如果文件不存在
		{
			if (!bAutoCreate)                                    // 如果不允许自动创建
				return NULL;                                     // 返回空指针

			pipe_writer_log(_sink, LL_INFO, "Data file {} not exists, initializing...", path.c_str()); // 输出日志
			
			uint64_t uSize = sizeof(RTTickBlock) + sizeof(WTSTickStruct) * HFT_SIZE_STEP; // 计算文件大小
			BoostFile bf;                                        // 创建文件对象
			bf.create_new_file(path.c_str());                    // 创建新文件
			bf.truncate_file((uint32_t)uSize);                   // 设置文件大小
			bf.close_file();                                     // 关闭文件

			isNew = true;                                        // 设置为新文件标志
		}

		pBlock->_file.reset(new BoostMappingFile);              // 创建内存映射文件对象
		if(!pBlock->_file->map(path.c_str()))                   // 如果映射文件失败
		{
			pipe_writer_log(_sink, LL_ERROR, "Mapping file {} failed", path.c_str()); // 输出错误日志
			pBlock->_file.reset();                              // 重置文件指针
			return NULL;                                         // 返回空指针
		}
		pBlock->_block = (RTTickBlock*)pBlock->_file->addr();   // 获取数据块指针

		if (!isNew &&  pBlock->_block->_date != curDate)        // 如果不是新文件且日期不匹配
		{
			pipe_writer_log(_sink, LL_INFO, "date[{}] of tick cache block[{}] is different from current date[{}], reinitializing...", pBlock->_block->_date, path.c_str(), curDate); // 输出日志
			pBlock->_block->_size = 0;                          // 重置数据块大小
			pBlock->_block->_date = curDate;                     // 设置当前日期

			memset(&pBlock->_block->_ticks, 0, sizeof(WTSTickStruct)*pBlock->_block->_capacity); // 清空Tick数据
		}

		if(isNew)                                                // 如果是新文件
		{
			pBlock->_block->_capacity = HFT_SIZE_STEP;           // 设置数据块容量
			pBlock->_block->_size = 0;                          // 设置数据块大小
			pBlock->_block->_version = BLOCK_VERSION_RAW_V2;     // 设置版本号
			pBlock->_block->_type = BT_RT_Ticks;                 // 设置块类型
			pBlock->_block->_date = curDate;                     // 设置日期
			strcpy(pBlock->_block->_blk_flag, BLK_FLAG);         // 设置块标志
		}
		else                                                     // 如果是已存在的文件
		{
			//检查缓存文件是否有问题,要自动恢复
			do
			{
				uint64_t uSize = sizeof(RTTickBlock) + sizeof(WTSTickStruct) * pBlock->_block->_capacity; // 计算期望文件大小
				uint64_t realSz = pBlock->_file->size();         // 获取实际文件大小
				if (realSz != uSize)                             // 如果大小不匹配
				{
					uint32_t realCap = (uint32_t)((realSz - sizeof(RTTickBlock)) / sizeof(WTSTickStruct)); // 计算实际容量
					uint32_t markedCap = pBlock->_block->_capacity; // 获取标记容量
					pipe_writer_log(_sink, LL_WARN, "Tick cache file of {} on {} repaired, real capiacity:{}, marked capacity:{}",
						ct->getCode(), curDate, realCap, markedCap); // 输出警告日志

					//文件大小不匹配,一般是因为capacity改了,但是实际没扩容
					//这是做一次扩容即可
					pBlock->_block->_capacity = realCap;         // 设置实际容量
					pBlock->_block->_size = min(realCap,markedCap); // 设置实际大小
				}
				
			} while (false);                                     // 只执行一次
			
		}
	}

	pBlock->_lasttime = TimeUtils::getLocalTimeNow() / 1000;     // 更新最后时间
	return pBlock;                                              // 返回数据块对指针
}

/*!
 * \brief 将数据传递给K线处理
 * \param ct 合约信息
 * \param curTick 当前Tick数据
 * 
 * 该函数负责将Tick数据传递给K线处理模块，用于K线数据生成。
 * 支持1分钟、5分钟K线的生成，包括数据验证、时间处理、K线更新等功能。
 */
void WtDataWriter::pipeToKlines(WTSContractInfo* ct, WTSTickData* curTick)
{
	bool tickNoTrade = decimal::eq(curTick->turnover(),0);      // 检查是否为无成交Tick

	// 如果未成交的bar也要跳过，那么就不需要处理所有未成交的tick了，否则即便没有成交也要放进来闭合bar
	if (_skip_notrade_bar && tickNoTrade)                       // 如果跳过无交易K线且当前Tick无成交
	{
		return;                                                  // 直接返回
	}

	uint32_t uDate = curTick->actiondate();                     // 获取动作日期
	WTSSessionInfo* sInfo = ct->getCommInfo()->getSessionInfo(); // 获取交易时段信息
	uint32_t curTime = curTick->actiontime() / 100000;          // 获取当前时间（去掉毫秒）

	uint32_t minutes = sInfo->timeToMinutes(curTime, false);     // 将时间转换为分钟数
	if (minutes == INVALID_UINT32)                              // 如果时间无效
	{
		pipe_writer_log(_sink, LL_WARN, "[pipeToKlines] [{}.{}] {}.{} is invalid timestamp, skip this tick", curTick->exchg(), curTick->code(), curTick->actiondate(), curTick->actiontime(), curTime); // 输出警告日志
		return;                                                  // 直接返回
	}

	//当秒数为0,要专门处理,比如091500000,这笔tick要算作0915的
	//如果是小节结束,要算作小节结束那一分钟,因为经常会有超过结束时间的价格进来,如113000500
	//不能同时处理,所以用or	
	if (sInfo->isLastOfSection(curTime))                        // 如果是小节结束时间
	{
		minutes--;                                              // 分钟数减1
	}

	//更新1分钟线
	if (!_disable_min1)                                         // 如果未禁用1分钟K线
	{
		KBlockPair* pBlockPair = getKlineBlock(ct, KP_Minute1); // 获取1分钟K线数据块
		if (pBlockPair && pBlockPair->_block)                   // 如果数据块存在
		{
			SpinLock lock(pBlockPair->_mutex);                  // 获取数据块锁
			RTKlineBlock* blk = pBlockPair->_block;            // 获取数据块指针
			if (blk->_size == blk->_capacity)                   // 如果数据块已满
			{
				pBlockPair->_file->sync();                      // 同步文件
				pBlockPair->_block = (RTKlineBlock*)resizeRTBlock<RTKlineBlock, WTSBarStruct>(pBlockPair->_file, blk->_capacity * 2); // 扩展数据块
				blk = pBlockPair->_block;                       // 更新数据块指针
			}

			WTSBarStruct* lastBar = NULL;                       // 最后K线指针
			if (blk->_size > 0)                                 // 如果数据块不为空
			{
				lastBar = &blk->_bars[blk->_size - 1];          // 获取最后一条K线
			}

			//拼接1分钟线
			uint32_t barMins = minutes + 1;                      // 计算K线分钟数
			uint64_t barTime = sInfo->minuteToTime(barMins);     // 将分钟数转换为时间
			uint32_t barDate = uDate;                            // 设置K线日期
			if (barTime == 0)                                   // 如果时间为0（跨日）
			{
				barDate = TimeUtils::getNextDate(barDate);      // 获取下一交易日
			}
			barTime = TimeUtils::timeToMinBar(barDate, (uint32_t)barTime); // 转换为分钟K线时间

			bool bNew = false;                                   // 是否为新K线标志
			if (lastBar == NULL || barTime > lastBar->time)     // 如果没有最后K线或时间大于最后K线时间
			{
				bNew = true;                                     // 设置为新K线
			}

			WTSBarStruct* newBar = NULL;                        // 新K线指针
			if (bNew)                                            // 如果是新K线
			{
				newBar = &blk->_bars[blk->_size];                // 获取新K线位置
				blk->_size += 1;                                 // 增加数据块大小

				newBar->date = curTick->tradingdate();           // 设置K线日期
				newBar->time = barTime;                          // 设置K线时间
				newBar->open = curTick->price();                  // 设置开盘价
				newBar->high = curTick->price();                  // 设置最高价
				newBar->low = curTick->price();                   // 设置最低价
				newBar->close = curTick->price();                 // 设置收盘价

				newBar->vol = curTick->volume();                  // 设置成交量
				newBar->money = curTick->turnover();              // 设置成交额
				
				/*
				 *	如果分钟线价格走势为1，则将tick的挂单价格记录下来
				 */
				if(_min_price_mode == 1)                         // 如果分钟线价格模式为1
				{
					newBar->bid = curTick->bidprice(0);           // 记录买一价
					newBar->ask = curTick->askprice(0);           // 记录卖一价
				}
				else                                             // 否则
				{
					newBar->hold = curTick->openinterest();       // 记录持仓量
					newBar->add = curTick->additional();         // 记录附加信息
				}
			}
			else if (! (_skip_notrade_tick && tickNoTrade))     // 如果不是新K线且不跳过无交易Tick
			{
				newBar = &blk->_bars[blk->_size - 1];           // 获取当前K线

				/*
				 *	By Wesley @ 2023.07.05
				 *	发现某些品种，开盘时可能会推送price为0的tick进来
				 *	会导致open和low都是0，所以要再做一个判断
				 */
				if (decimal::eq(newBar->open, 0))                // 如果开盘价为0
					newBar->open = curTick->price();              // 设置开盘价

				if (decimal::eq(newBar->low, 0))                // 如果最低价为0
					newBar->low = curTick->price();              // 设置最低价
				else                                             // 否则
					newBar->low = std::min(curTick->price(), newBar->low); // 更新最低价

				newBar->close = curTick->price();                // 更新收盘价
				newBar->high = std::max(curTick->price(), newBar->high); // 更新最高价

				newBar->vol += curTick->volume();                // 累加成交量
				newBar->money += curTick->turnover();            // 累加成交额

				/*
				 *	如果分钟线价格走势为1，则将tick的挂单价格记录下来
				 */
				if (_min_price_mode == 1)                        // 如果分钟线价格模式为1
				{
					newBar->bid = curTick->bidprice(0);           // 更新买一价
					newBar->ask = curTick->askprice(0);           // 更新卖一价
				}
				else                                             // 否则
				{
					newBar->hold = curTick->openinterest();      // 更新持仓量
					newBar->add += curTick->additional();        // 累加附加信息
				}
			}
		}
	}

	//更新5分钟线
	if (!_disable_min5)                                         // 如果未禁用5分钟K线
	{
		KBlockPair* pBlockPair = getKlineBlock(ct, KP_Minute5); // 获取5分钟K线数据块
		if (pBlockPair && pBlockPair->_block)                   // 如果数据块存在
		{
			SpinLock lock(pBlockPair->_mutex);                  // 获取数据块锁
			RTKlineBlock* blk = pBlockPair->_block;            // 获取数据块指针
			if (blk->_size == blk->_capacity)                   // 如果数据块已满
			{
				pBlockPair->_file->sync();                      // 同步文件
				pBlockPair->_block = (RTKlineBlock*)resizeRTBlock<RTKlineBlock, WTSBarStruct>(pBlockPair->_file, blk->_capacity * 2); // 扩展数据块
				blk = pBlockPair->_block;                       // 更新数据块指针
			}

			WTSBarStruct* lastBar = NULL;                       // 最后K线指针
			if (blk->_size > 0)                                 // 如果数据块不为空
			{
				lastBar = &blk->_bars[blk->_size - 1];          // 获取最后一条K线
			}

			uint32_t barMins = (minutes / 5) * 5 + 5;          // 计算5分钟K线分钟数
			uint64_t barTime = sInfo->minuteToTime(barMins);     // 将分钟数转换为时间
			uint32_t barDate = uDate;                            // 设置K线日期
			if (barTime == 0)                                   // 如果时间为0（跨日）
			{
				barDate = TimeUtils::getNextDate(barDate);      // 获取下一交易日
			}
			barTime = TimeUtils::timeToMinBar(barDate, (uint32_t)barTime); // 转换为分钟K线时间

			bool bNew = false;                                   // 是否为新K线标志
			if (lastBar == NULL || barTime > lastBar->time)     // 如果没有最后K线或时间大于最后K线时间
			{
				bNew = true;                                     // 设置为新K线
			}

			WTSBarStruct* newBar = NULL;                        // 新K线指针
			if (bNew)                                            // 如果是新K线
			{
				newBar = &blk->_bars[blk->_size];                // 获取新K线位置
				blk->_size += 1;                                 // 增加数据块大小

				newBar->date = curTick->tradingdate();           // 设置K线日期
				newBar->time = barTime;                          // 设置K线时间
				newBar->open = curTick->price();                  // 设置开盘价
				newBar->high = curTick->price();                  // 设置最高价
				newBar->low = curTick->price();                   // 设置最低价
				newBar->close = curTick->price();                 // 设置收盘价

				newBar->vol = curTick->volume();                  // 设置成交量
				newBar->money = curTick->turnover();              // 设置成交额

				/*
				 *	如果分钟线价格走势为1，则将tick的挂单价格记录下来
				 */
				if (_min_price_mode == 1)                        // 如果分钟线价格模式为1
				{
					newBar->bid = curTick->bidprice(0);           // 记录买一价
					newBar->ask = curTick->askprice(0);           // 记录卖一价
				}
				else                                             // 否则
				{
					newBar->hold = curTick->openinterest();      // 记录持仓量
					newBar->add = curTick->additional();         // 记录附加信息
				}
			}
			else if (! (_skip_notrade_tick && tickNoTrade))     // 如果不是新K线且不跳过无交易Tick
			{
				newBar = &blk->_bars[blk->_size - 1];           // 获取当前K线

				/*
				 *	By Wesley @ 2023.07.05
				 *	发现某些品种，开盘时可能会推送price为0的tick进来
				 *	会导致open和low都是0，所以要再做一个判断
				 */
				if (decimal::eq(newBar->open, 0))                // 如果开盘价为0
					newBar->open = curTick->price();              // 设置开盘价

				if (decimal::eq(newBar->low, 0))                // 如果最低价为0
					newBar->low = curTick->price();              // 设置最低价
				else                                             // 否则
					newBar->low = std::min(curTick->price(), newBar->low); // 更新最低价

				newBar->close = curTick->price();                // 更新收盘价
				newBar->high = max(curTick->price(), newBar->high); // 更新最高价

				newBar->vol += curTick->volume();                // 累加成交量
				newBar->money += curTick->turnover();            // 累加成交额

				/*
				 *	如果分钟线价格走势为1，则将tick的挂单价格记录下来
				 */
				if (_min_price_mode == 1)                        // 如果分钟线价格模式为1
				{
					newBar->bid = curTick->bidprice(0);           // 更新买一价
					newBar->ask = curTick->askprice(0);           // 更新卖一价
				}
				else                                             // 否则
				{
					newBar->hold = curTick->openinterest();      // 更新持仓量
					newBar->add += curTick->additional();        // 累加附加信息
				}
			}
		}
	}
}

/*!
 * \brief 释放数据块
 * \tparam T 数据块类型
 * \param block 数据块指针
 * 
 * 该模板函数负责释放指定类型的数据块，包括重置数据块指针、文件指针和最后时间。
 * 用于数据块的清理和资源释放。
 */
template<typename T>
void WtDataWriter::releaseBlock(T* block)
{
	if (block == NULL || block->_file == NULL)                 // 如果数据块为空或文件指针为空
		return;                                                // 直接返回

	SpinLock lock(block->_mutex);                              // 获取数据块锁
	block->_block = NULL;                                      // 重置数据块指针
	block->_file.reset();                                      // 重置文件指针
	block->_lasttime = 0;                                      // 重置最后时间
}

/*!
 * \brief 获取K线数据块
 * \param ct 合约信息
 * \param period K线周期
 * \param bAutoCreate 是否自动创建
 * \return K线数据块指针
 * 
 * 该函数负责获取或创建指定周期的K线数据块，用于K线数据存储。
 * 支持1分钟、5分钟K线，包括自动创建文件、内存映射等功能。
 */
WtDataWriter::KBlockPair* WtDataWriter::getKlineBlock(WTSContractInfo* ct, WTSKlinePeriod period, bool bAutoCreate /* = true */)
{
	if (ct == NULL)                                             // 如果合约信息为空
		return NULL;                                            // 返回空指针

	KBlockPair* pBlock = NULL;                                  // 数据块对指针
	const char* key = ct->getFullCode();                         // 获取合约完整代码

	//读取交易的分钟数
	uint32_t totalMins = ct->getCommInfo()->getSessionInfo()->getTradingMins(); // 获取交易分钟数

	KBlockFilesMap* cache_map = NULL;                            // 缓存映射表指针
	std::string subdir = "";                                     // 子目录名
	BlockType bType;                                             // 块类型
	switch(period)                                               // 根据K线周期设置参数
	{
	case KP_Minute1:                                             // 1分钟K线
		cache_map = &_rt_min1_blocks;                           // 设置1分钟K线缓存映射表
		subdir = "min1";                                         // 设置子目录
		bType = BT_RT_Minute1;                                   // 设置块类型
		break;
	case KP_Minute5:                                             // 5分钟K线
		cache_map = &_rt_min5_blocks;                           // 设置5分钟K线缓存映射表
		subdir = "min5";                                         // 设置子目录
		bType = BT_RT_Minute5;                                   // 设置块类型
		totalMins /= 5;	                                        // 如果是5分钟线，要除以5
		break;
	default: break;                                              // 默认情况
	}

	if (cache_map == NULL)                                       // 如果缓存映射表为空
		return NULL;                                             // 返回空指针

	pBlock = (*cache_map)[key];                                  // 从映射表中获取数据块
	if (pBlock == NULL)                                          // 如果数据块不存在
	{
		pBlock = new KBlockPair();                              // 创建新的数据块对
		(*cache_map)[key] = pBlock;                             // 添加到映射表
	}

	if (pBlock->_block == NULL)                                 // 如果数据块为空
	{
		thread_local static char path[256] = { 0 };            // 线程局部静态路径缓冲区
		char * s = fmt::format_to(path, "{}rt/{}/{}/", _base_dir, subdir, ct->getExchg()); // 格式化路径
		s[0] = '\0';                                            // 重置字符串
		if (bAutoCreate)                                         // 如果允许自动创建
			BoostFile::create_directories(path);                // 创建目录

		wt_strcpy(s, ct->getCode());                            // 复制合约代码
		s += strlen(ct->getCode());                              // 移动指针
		wt_strcpy(s, ".dmb");                                   // 添加文件扩展名
		s += 4;                                                 // 移动指针
		s[0] = '\0';                                            // 重置字符串

		bool isNew = false;                                      // 是否为新文件标志
		if (!BoostFile::exists(path))                           // 如果文件不存在
		{
			if (!bAutoCreate)                                    // 如果不允许自动创建
				return NULL;                                     // 返回空指针

			pipe_writer_log(_sink, LL_INFO, "Data file {} not exists, initializing...", path); // 输出日志

			uint64_t uSize = sizeof(RTKlineBlock) + sizeof(WTSBarStruct) * totalMins;	//预分配按照K线条数分配
			BoostFile bf;                                        // 创建文件对象
			bf.create_new_file(path);                            // 创建新文件
			bf.truncate_file((uint32_t)uSize);                   // 设置文件大小
			bf.close_file();                                     // 关闭文件

			isNew = true;                                        // 设置为新文件标志
		}

		pBlock->_file.reset(new BoostMappingFile);              // 创建内存映射文件对象
		if(pBlock->_file->map(path))                             // 如果映射文件成功
		{
			pBlock->_block = (RTKlineBlock*)pBlock->_file->addr(); // 获取数据块指针
		}
		else                                                     // 如果映射文件失败
		{
			pipe_writer_log(_sink, LL_ERROR, "Mapping file {} failed", path); // 输出错误日志
			pBlock->_file.reset();                              // 重置文件指针
			return NULL;                                         // 返回空指针
		}

		//if(pBlock->_block->_date != uDate)
		//{
		//	pBlock->_block->_size = 0;
		//	pBlock->_block->_date = uDate;

		//	memset(&pBlock->_block->_bars, 0, sizeof(WTSBarStruct)*pBlock->_block->_capacity);
		//}

		if (isNew)                                               // 如果是新文件
		{
			//memset(pBlock->_block, 0, pBlock->_file->size());
			pBlock->_block->_capacity = totalMins;               // 设置数据块容量
			pBlock->_block->_size = 0;                          // 设置数据块大小
			pBlock->_block->_version = BLOCK_VERSION_RAW_V2;     // 设置版本号
			pBlock->_block->_type = bType;                       // 设置块类型
			pBlock->_block->_date = TimeUtils::getCurDate();     // 设置当前日期
			strcpy(pBlock->_block->_blk_flag, BLK_FLAG);         // 设置块标志
		}
	}

	pBlock->_lasttime = TimeUtils::getLocalTimeNow() / 1000;     // 更新最后时间
	return pBlock;                                              // 返回数据块对指针
}

/*!
 * \brief 获取当前Tick数据
 * \param code 合约代码
 * \param exchg 交易所代码
 * \return 当前Tick数据指针
 * 
 * 该函数用于获取指定合约的当前最新Tick数据，用于实时行情查询。
 * 从缓存中查找并返回最新的Tick数据。
 */
WTSTickData* WtDataWriter::getCurTick(const char* code, const char* exchg/* = ""*/)
{
	if (strlen(code) == 0)                                       // 如果合约代码为空
		return NULL;                                            // 返回空指针

	WTSContractInfo* ct = _bd_mgr->getContract(code, exchg);     // 获取合约信息
	if (ct == NULL)                                             // 如果合约信息为空
		return NULL;                                            // 返回空指针

	const char* key = ct->getFullCode();                         // 获取合约完整代码
	SpinLock lock(_lck_tick_cache);                              // 获取Tick缓存锁
	auto it = _tick_cache_idx.find(key);                         // 查找缓存索引
	if (it == _tick_cache_idx.end())                            // 如果未找到缓存项
		return NULL;                                            // 返回空指针

	uint32_t idx = it->second;                                   // 获取缓存索引
	TickCacheItem& item = _tick_cache_block->_ticks[idx];       // 获取缓存项
	return WTSTickData::create(item._tick);                      // 创建并返回Tick数据
}

/*!
 * \brief 更新Tick缓存
 * \param ct 合约信息
 * \param curTick 当前Tick数据
 * \param procFlag 处理标志
 * \return 是否更新成功
 * 
 * 该函数负责更新Tick缓存，包括数据验证、缓存扩容、数据更新等功能。
 * 支持新合约的缓存创建和现有合约的缓存更新。
 */
bool WtDataWriter::updateCache(WTSContractInfo* ct, WTSTickData* curTick, uint32_t procFlag)
{
	if (curTick == NULL || _tick_cache_block == NULL)            // 如果Tick数据为空或缓存块为空
	{
		pipe_writer_log(_sink, LL_ERROR, "Tick cache data not initialized"); // 输出错误日志
		return false;                                            // 返回失败
	}

	SpinLock lock(_lck_tick_cache);                              // 获取Tick缓存锁
	const char* key = ct->getFullCode();                         // 获取合约完整代码
	uint32_t idx = 0;                                            // 缓存索引
	auto it = _tick_cache_idx.find(key);                         // 查找缓存索引
	if (it == _tick_cache_idx.end())                            // 如果未找到缓存项
	{
		idx = _tick_cache_block->_size;                          // 设置索引为当前大小
		_tick_cache_idx[key] = _tick_cache_block->_size;         // 添加缓存索引
		_tick_cache_block->_size += 1;                           // 增加缓存大小
		if(_tick_cache_block->_size >= _tick_cache_block->_capacity) // 如果缓存已满
		{
			_tick_cache_block = (RTTickCache*)resizeRTBlock<RTTickCache, TickCacheItem>(_tick_cache_file, _tick_cache_block->_capacity + CACHE_SIZE_STEP); // 扩展缓存
			pipe_writer_log(_sink, LL_INFO, "Tick Cache resized to {} items", _tick_cache_block->_capacity); // 输出日志
		}
	}
	else                                                         // 如果找到缓存项
	{
		idx = it->second;                                         // 获取缓存索引
	}


	TickCacheItem& item = _tick_cache_block->_ticks[idx];        // 获取缓存项
	if (curTick->tradingdate() < item._date)                     // 如果当前交易日小于缓存交易日
	{
		pipe_writer_log(_sink, LL_INFO, "Tradingday[{}] of {} is less than cached tradingday[{}]", curTick->tradingdate(), curTick->code(), item._date); // 输出日志
		return false;                                            // 返回失败
	}

	WTSTickStruct& newTick = curTick->getTickStruct();           // 获取Tick结构

	if (curTick->tradingdate() > item._date)                     // 如果当前交易日大于缓存交易日
	{
		//新数据交易日大于老数据,则认为是新一天的数据
		item._date = curTick->tradingdate();                     // 更新缓存交易日
		memcpy(&item._tick, &newTick, sizeof(WTSTickStruct));    // 复制Tick数据
		if (procFlag==1)                                         // 如果处理标志为1
		{
			item._tick.volume = item._tick.total_volume;         // 设置成交量
			item._tick.turn_over = item._tick.total_turnover;   // 设置成交额
			item._tick.diff_interest = item._tick.open_interest - item._tick.pre_interest; // 设置持仓变化

			newTick.volume = newTick.total_volume;                // 设置成交量
			newTick.turn_over = newTick.total_turnover;          // 设置成交额
			newTick.diff_interest = newTick.open_interest - newTick.pre_interest; // 设置持仓变化
		}

		//	newTick.trading_date, curTick->exchg(), curTick->code(), curTick->volume(),
		//	curTick->turnover(), curTick->openinterest(), curTick->additional());
		pipe_writer_log(_sink, LL_INFO, "First tick of new tradingday {},{}.{},{},{},{},{},{}", 
			newTick.trading_date, curTick->exchg(), curTick->code(), curTick->price(), curTick->volume(),
			curTick->turnover(), curTick->openinterest(), curTick->additional()); // 输出新交易日首个Tick日志
	}
	else                                                         // 如果是同一交易日
	{
		//如果缓存里的数据日期大于最新行情的日期
		//或者缓存里的时间大于等于最新行情的时间,数据就不需要处理
		WTSSessionInfo* sInfo = ct->getCommInfo()->getSessionInfo(); // 获取交易时段信息
		uint32_t tdate = sInfo->getOffsetDate(curTick->actiondate(), curTick->actiontime() / 100000); // 计算偏移日期
		if (tdate > curTick->tradingdate())                      // 如果偏移日期大于交易日期
		{
			pipe_writer_log(_sink, LL_WARN, "Last tick of {}.{} with time {}.{} has an exception, abandoned", curTick->exchg(), curTick->code(), curTick->actiondate(), curTick->actiontime()); // 输出警告日志
			return false;                                        // 返回失败
		}
		else if (curTick->totalvolume() < item._tick.total_volume) // 如果总成交量小于缓存成交量
		{
			pipe_writer_log(_sink, LL_WARN, "Last tick of {}.{} with time {}.{}, volume {} is less than cached volume {}, abandoned",
				curTick->exchg(), curTick->code(), curTick->actiondate(), curTick->actiontime(), curTick->totalvolume(), item._tick.total_volume); // 输出警告日志
			return false;                                        // 返回失败
		}

		//时间戳相同,但是成交量大于等于原来的,这种情况一般是郑商所,这里的处理方式就是时间戳+200毫秒
		//By Wesley @ 2021.12.21
		//今天发现居然一秒出现了4笔，实在是有点无语
		//只能把500毫秒的变化量改成200，并且改成发生时间小于等于上一笔的判断
		if(newTick.action_date == item._tick.action_date && newTick.action_time <= item._tick.action_time && newTick.total_volume >= item._tick.total_volume) // 如果时间戳相同且成交量大于等于缓存
		{
			newTick.action_time += 200;                          // 时间戳增加200毫秒
		}

		//这里就要看需不需要预处理了
		if(procFlag == 0)                                         // 如果不需要预处理
		{
			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct)); // 直接复制Tick数据
		}
		else                                                     // 如果需要预处理
		{
			newTick.volume = newTick.total_volume - item._tick.total_volume; // 计算增量成交量
			newTick.turn_over = newTick.total_turnover - item._tick.total_turnover; // 计算增量成交额
			newTick.diff_interest = newTick.open_interest - item._tick.open_interest; // 计算持仓变化

			memcpy(&item._tick, &newTick, sizeof(WTSTickStruct)); // 复制处理后的Tick数据
		}

		//pipe_writer_log(_sink, LL_DEBUG, "Tick cache data updated {}[{}.{}]", newTick.code, newTick.action_date, newTick.action_time);
	}

	return true;                                                 // 返回成功
}

/*!
 * \brief 转储历史数据
 * \param sid 会话标识
 * 
 * 该函数负责将实时数据转储为历史数据，用于数据归档和备份。
 * 支持按会话转储和清除缓存功能。
 */
void WtDataWriter::transHisData(const char* sid)
{
	StdUniqueLock lock(_proc_mtx);                               // 获取处理锁
	if (strcmp(sid, CMD_CLEAR_CACHE) != 0)                      // 如果不是清除缓存命令
	{
		CodeSet* pCommSet = _sink->getSessionComms(sid);         // 获取会话商品集合
		if (pCommSet == NULL)                                    // 如果商品集合为空
			return;                                              // 直接返回

		for (auto it = pCommSet->begin(); it != pCommSet->end(); it++) // 遍历商品集合
		{
			const char* key = (*it).c_str();                     // 获取商品键

			const StringVector& ay = StrUtil::split(key, "."); // 分割商品键
			const char* exchg = ay[0].c_str();                   // 获取交易所
			const char* pid = ay[1].c_str();                     // 获取商品ID

			WTSCommodityInfo* pCommInfo = _bd_mgr->getCommodity(exchg, pid); // 获取商品信息
			if (pCommInfo == NULL)                               // 如果商品信息为空
				continue;                                        // 继续下一个

			const CodeSet& codes = pCommInfo->getCodes();        // 获取合约代码集合
			for (auto code : codes)                              // 遍历合约代码
			{
				WTSContractInfo* ct = _bd_mgr->getContract(code.c_str(), exchg); // 获取合约信息
				if(ct)                                           // 如果合约信息存在
					_proc_que.push(ct->getFullCode());           // 添加到处理队列
			}
		}

		_proc_que.push(fmtutil::format("MARK.{}", sid));         // 添加标记到处理队列
	}
	else                                                         // 如果是清除缓存命令
	{
		_proc_que.push(sid);                                     // 添加命令到处理队列
	}

	if (_proc_thrd == NULL)                                      // 如果处理线程未启动
	{
		_proc_thrd.reset(new StdThread(boost::bind(&WtDataWriter::proc_loop, this))); // 启动处理线程
	}
	else                                                         // 如果处理线程已启动
	{
		_proc_cond.notify_all();                                 // 通知处理线程
	}
}

/*!
 * \brief 检查循环
 * 
 * 该函数负责定期检查各种数据块的过期情况，自动释放过期的数据块。
 * 包括Tick、逐笔成交、委托详情、委托队列、1分钟K线、5分钟K线等数据块。
 */
void WtDataWriter::check_loop()
{
	uint32_t expire_secs = 600;                                  // 过期时间（秒）
	while(!_terminated)                                          // 当未终止时循环
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));  // 休眠10秒
		/*
		 *	By Wesley @ 2022.04.18
		 *	如果收盘作业线程已经启动，则直接退出检查线程
		 */
		if(_proc_thrd != NULL)                                   // 如果处理线程已启动
			break;                                               // 跳出循环

		uint64_t now = TimeUtils::getLocalTimeNow() / 1000;     // 获取当前时间
		for (auto it = _rt_ticks_blocks.begin(); it != _rt_ticks_blocks.end(); it++) // 遍历Tick数据块
		{
			const char* key = it->first.c_str();                 // 获取合约键
			TickBlockPair* tBlk = (TickBlockPair*)it->second;   // 获取数据块对
			if (tBlk->_lasttime != 0 && (now - tBlk->_lasttime > expire_secs)) // 如果数据块过期
			{
				pipe_writer_log(_sink, LL_INFO, "tick cache of {} mapping expired, automatically closed", key); // 输出日志
				releaseBlock<TickBlockPair>(tBlk);              // 释放数据块
			}
		}

		for (auto it = _rt_trans_blocks.begin(); it != _rt_trans_blocks.end(); it++) // 遍历逐笔成交数据块
		{
			const char* key = it->first.c_str();                 // 获取合约键
			TransBlockPair* tBlk = (TransBlockPair*)it->second; // 获取数据块对
			if (tBlk->_lasttime != 0 && (now - tBlk->_lasttime > expire_secs)) // 如果数据块过期
			{
				pipe_writer_log(_sink, LL_INFO, "trans cache o {} mapping expired, automatically closed", key); // 输出日志
				releaseBlock<TransBlockPair>(tBlk);              // 释放数据块
			}
		}

		for (auto it = _rt_orddtl_blocks.begin(); it != _rt_orddtl_blocks.end(); it++) // 遍历委托详情数据块
		{
			const char* key = it->first.c_str();                 // 获取合约键
			OrdDtlBlockPair* tBlk = (OrdDtlBlockPair*)it->second; // 获取数据块对
			if (tBlk->_lasttime != 0 && (now - tBlk->_lasttime > expire_secs)) // 如果数据块过期
			{
				pipe_writer_log(_sink, LL_INFO, "order cache of {} mapping expired, automatically closed", key); // 输出日志
				releaseBlock<OrdDtlBlockPair>(tBlk);             // 释放数据块
			}
		}

		for (auto& v : _rt_ordque_blocks)                        // 遍历委托队列数据块
		{
			const char* key = v.first.c_str();                   // 获取合约键
			OrdQueBlockPair* tBlk = (OrdQueBlockPair*)v.second; // 获取数据块对
			if (tBlk->_lasttime != 0 && (now - tBlk->_lasttime > expire_secs)) // 如果数据块过期
			{
				pipe_writer_log(_sink, LL_INFO, "queue cache of {} mapping expired, automatically closed", key); // 输出日志
				releaseBlock<OrdQueBlockPair>(tBlk);             // 释放数据块
			}
		}

		for (auto it = _rt_min1_blocks.begin(); it != _rt_min1_blocks.end(); it++) // 遍历1分钟K线数据块
		{
			const char* key = it->first.c_str();                 // 获取合约键
			KBlockPair* kBlk = (KBlockPair*)it->second;         // 获取数据块对
			if (kBlk->_lasttime != 0 && (now - kBlk->_lasttime > expire_secs)) // 如果数据块过期
			{
				pipe_writer_log(_sink, LL_INFO, "min1 cache of {} mapping expired, automatically closed", key); // 输出日志
				releaseBlock<KBlockPair>(kBlk);                 // 释放数据块
			}
		}

		for (auto it = _rt_min5_blocks.begin(); it != _rt_min5_blocks.end(); it++) // 遍历5分钟K线数据块
		{
			const char* key = it->first.c_str();                 // 获取合约键
			KBlockPair* kBlk = (KBlockPair*)it->second;         // 获取数据块对
			if (kBlk->_lasttime != 0 && (now - kBlk->_lasttime > expire_secs)) // 如果数据块过期
			{
				pipe_writer_log(_sink, LL_INFO, "min5 cache of {} mapping expired, automatically closed", key); // 输出日志
				releaseBlock<KBlockPair>(kBlk);                 // 释放数据块
			}
		}
	}
}

/*!
 * \brief 通过转储器转储K线数据
 * \param ct 合约信息
 * \return 转储的K线数量
 * 
 * 该函数负责通过外部转储器将实时K线数据转储为历史数据。
 * 包括日线、1分钟线、5分钟线的转储功能。
 */
uint32_t WtDataWriter::dump_bars_via_dumper(WTSContractInfo* ct)
{
	if (ct == NULL || _dumpers.empty())                         // 如果合约信息为空或转储器为空
		return 0;                                               // 返回0

	std::string key = ct->getFullCode();                        // 获取合约完整代码

	uint32_t count = 0;                                         // 转储计数器

	//从缓存中读取最新tick,更新到历史日线
	auto it = _tick_cache_idx.find(key);                        // 查找Tick缓存索引
	if (it != _tick_cache_idx.end())                            // 如果找到缓存项
	{
		uint32_t idx = it->second;                              // 获取缓存索引

		const TickCacheItem& tci = _tick_cache_block->_ticks[idx]; // 获取缓存项
		const WTSTickStruct& ts = tci._tick;                    // 获取Tick结构

		WTSBarStruct bsDay;                                      // 日线K线结构
		bsDay.open = ts.open;                                   // 设置开盘价
		bsDay.high = ts.high;                                   // 设置最高价
		bsDay.low = ts.low;                                     // 设置最低价
		bsDay.close = ts.price;                                 // 设置收盘价
		bsDay.settle = ts.settle_price;                        // 设置结算价
		bsDay.vol = ts.total_volume;                            // 设置成交量
		bsDay.money = ts.total_turnover;                        // 设置成交额
		bsDay.hold = ts.open_interest;                          // 设置持仓量
		bsDay.add = ts.diff_interest;                           // 设置持仓变化

		for(auto& item : _dumpers)                              // 遍历转储器
		{
			const char* id = item.first.c_str();                // 获取转储器ID
			IHisDataDumper* dumper = item.second;               // 获取转储器指针
			if(dumper == NULL)                                  // 如果转储器为空
				continue;                                       // 继续下一个

			bool bSucc = dumper->dumpHisBars(key.c_str(), "d1", &bsDay, 1); // 转储日线数据
			if (!bSucc)                                         // 如果转储失败
			{
				pipe_writer_log(_sink, LL_ERROR, "Closing Task of day bar of {} failed via extended dumper {}", ct->getFullCode(), id); // 输出错误日志
			}
		}

		count++;                                                 // 增加计数器

	}

	//转移实时1分钟线
	KBlockPair* kBlkPair = getKlineBlock(ct, KP_Minute1, false); // 获取1分钟K线数据块
	if (kBlkPair != NULL && kBlkPair->_block->_size > 0)        // 如果数据块存在且不为空
	{
		uint32_t size = kBlkPair->_block->_size;                // 获取数据块大小
		pipe_writer_log(_sink, LL_INFO, "Transfering min1 bars of {}...", ct->getFullCode()); // 输出日志
		SpinLock lock(kBlkPair->_mutex);                       // 获取数据块锁

		for (auto& item : _dumpers)                             // 遍历转储器
		{
			const char* id = item.first.c_str();                // 获取转储器ID
			IHisDataDumper* dumper = item.second;               // 获取转储器指针
			if (dumper == NULL)                                 // 如果转储器为空
				continue;                                       // 继续下一个

			bool bSucc = dumper->dumpHisBars(key.c_str(), "m1", kBlkPair->_block->_bars, size); // 转储1分钟K线数据
			if (!bSucc)                                          // 如果转储失败
			{
				pipe_writer_log(_sink, LL_ERROR, "Closing Task of m1 bar of {} failed via extended dumper {}", ct->getFullCode(), id); // 输出错误日志
			}
		}

		count++;                                                 // 增加计数器
		kBlkPair->_block->_size = 0;                            // 重置数据块大小
	}

	if (kBlkPair)                                                // 如果数据块存在
		releaseBlock(kBlkPair);                                 // 释放数据块

	//第四步,转移实时5分钟线
	kBlkPair = getKlineBlock(ct, KP_Minute5, false);           // 获取5分钟K线数据块
	if (kBlkPair != NULL && kBlkPair->_block->_size > 0)        // 如果数据块存在且不为空
	{
		uint32_t size = kBlkPair->_block->_size;                // 获取数据块大小
		pipe_writer_log(_sink, LL_INFO, "Transfering min5 bars of {}...", ct->getFullCode()); // 输出日志
		SpinLock lock(kBlkPair->_mutex);                        // 获取数据块锁

		for (auto& item : _dumpers)                             // 遍历转储器
		{
			const char* id = item.first.c_str();                // 获取转储器ID
			IHisDataDumper* dumper = item.second;               // 获取转储器指针
			if (dumper == NULL)                                 // 如果转储器为空
				continue;                                       // 继续下一个

			bool bSucc = dumper->dumpHisBars(key.c_str(), "m5", kBlkPair->_block->_bars, size); // 转储5分钟K线数据
			if (!bSucc)                                          // 如果转储失败
			{
				pipe_writer_log(_sink, LL_ERROR, "Closing Task of m5 bar of {} failed via extended dumper {}", ct->getFullCode(), id); // 输出错误日志
			}
		}

		count++;                                                 // 增加计数器
		kBlkPair->_block->_size = 0;
	}

	if (kBlkPair)
		releaseBlock(kBlkPair);

	return count;
}

/*!
 * \brief 处理数据块数据
 * \param tag 数据标签
 * \param content 数据内容
 * \param isBar 是否为K线数据
 * \param bKeepHead 是否保留头部
 * \return 是否处理成功
 * 
 * 该函数负责处理数据块数据，包括数据解压、版本转换等功能。
 * 支持压缩数据解压和老版本数据结构转换。
 */
bool WtDataWriter::proc_block_data(const char* tag, std::string& content, bool isBar, bool bKeepHead /* = true */)
{
	BlockHeader* header = (BlockHeader*)content.data();          // 获取数据块头部

	bool bCmped = header->is_compressed();                       // 检查是否压缩
	bool bOldVer = header->is_old_version();                     // 检查是否老版本

	//如果既没有压缩，也不是老版本结构体，则直接返回
	if (!bCmped && !bOldVer)                                    // 如果既未压缩也不是老版本
	{
		if (!bKeepHead)                                          // 如果不保留头部
			content.erase(0, BLOCK_HEADER_SIZE);                 // 删除头部
		return true;                                             // 返回成功
	}

	std::string buffer;                                          // 数据缓冲区
	if (bCmped)                                                  // 如果数据已压缩
	{
		BlockHeaderV2* blkV2 = (BlockHeaderV2*)content.c_str(); // 获取V2头部

		if (content.size() != (sizeof(BlockHeaderV2) + blkV2->_size)) // 如果数据大小不匹配
		{
			return false;                                        // 返回失败
		}

		//将文件头后面的数据进行解压
		buffer = WTSCmpHelper::uncompress_data(content.data() + BLOCK_HEADERV2_SIZE, (std::size_t)blkV2->_size); // 解压数据
	}
	else                                                         // 如果数据未压缩
	{
		if (!bOldVer)                                            // 如果不是老版本
		{
			//如果不是老版本，直接返回
			if (!bKeepHead)                                      // 如果不保留头部
				content.erase(0, BLOCK_HEADER_SIZE);             // 删除头部
			return true;                                         // 返回成功
		}
		else                                                     // 如果是老版本
		{
			buffer.append(content.data() + BLOCK_HEADER_SIZE, content.size() - BLOCK_HEADER_SIZE); // 复制数据部分
		}
	}

	if (bOldVer)                                                 // 如果是老版本
	{
		if (isBar)                                               // 如果是K线数据
		{
			std::string bufV2;                                   // V2缓冲区
			uint32_t barcnt = buffer.size() / sizeof(WTSBarStructOld); // 计算K线数量
			bufV2.resize(barcnt * sizeof(WTSBarStruct));        // 调整缓冲区大小
			WTSBarStruct* newBar = (WTSBarStruct*)bufV2.data();  // 获取新K线指针
			WTSBarStructOld* oldBar = (WTSBarStructOld*)buffer.data(); // 获取老K线指针
			for (uint32_t idx = 0; idx < barcnt; idx++)         // 遍历K线数据
			{
				newBar[idx] = oldBar[idx];                       // 转换K线数据
			}
			buffer.swap(bufV2);                                  // 交换缓冲区

			pipe_writer_log(_sink, LL_INFO, "{} bars of {} transferd to new version...", barcnt, tag); // 输出日志
		}
		else                                                     // 如果是Tick数据
		{
			uint32_t tick_cnt = buffer.size() / sizeof(WTSTickStructOld); // 计算Tick数量
			std::string bufv2;                                   // V2缓冲区
			bufv2.resize(sizeof(WTSTickStruct)*tick_cnt);        // 调整缓冲区大小
			WTSTickStruct* newTick = (WTSTickStruct*)bufv2.data(); // 获取新Tick指针
			WTSTickStructOld* oldTick = (WTSTickStructOld*)buffer.data(); // 获取老Tick指针
			for (uint32_t i = 0; i < tick_cnt; i++)              // 遍历Tick数据
			{
				newTick[i] = oldTick[i];                         // 转换Tick数据
			}
			buffer.swap(bufv2);                                  // 交换缓冲区
			pipe_writer_log(_sink, LL_INFO, "{} ticks of {} transferd to new version...", tick_cnt, tag); // 输出日志
		}
	}

	if (bKeepHead)                                               // 如果保留头部
	{
		content.resize(BLOCK_HEADER_SIZE);                       // 调整内容大小
		content.append(buffer);                                  // 追加数据
		header = (BlockHeader*)content.data();                   // 获取头部指针
		header->_version = BLOCK_VERSION_RAW_V2;                 // 设置版本号
	}
	else                                                         // 如果不保留头部
	{
		content.swap(buffer);                                    // 交换内容
	}

	return true;                                                 // 返回成功
}

/*!
 * \brief 转储日线数据
 * \param ct 合约信息
 * \param newBar 新的日线K线数据
 * \return 是否转储成功
 * 
 * 该函数负责将日线K线数据转储到历史数据文件中。
 * 支持数据压缩、重复数据检查和版本管理。
 */
bool WtDataWriter::dump_day_data(WTSContractInfo* ct, WTSBarStruct* newBar)
{
	std::stringstream ss;                                        // 字符串流
	ss << _base_dir << "his/day/" << ct->getExchg() << "/";     // 构建路径
	std::string path = ss.str();                                 // 获取路径字符串
	BoostFile::create_directories(ss.str().c_str());             // 创建目录
	std::string filename = fmtutil::format("{}{}.dsb", path, ct->getCode()); // 构建文件名

	bool bNew = false;                                           // 是否为新文件标志
	if (!BoostFile::exists(filename.c_str()))                     // 如果文件不存在
		bNew = true;                                             // 设置为新文件

	BoostFile f;                                                 // 文件对象
	if (f.create_or_open_file(filename.c_str()))                 // 如果成功打开文件
	{
		bool bNeedWrite = true;                                  // 是否需要写入标志
		if (bNew)                                                // 如果是新文件
		{
			BlockHeader header;                                   // 数据块头部
			strcpy(header._blk_flag, BLK_FLAG);                   // 设置块标志
			header._type = BT_HIS_Day;                           // 设置块类型
			header._version = BLOCK_VERSION_RAW_V2;              // 设置版本号

			f.write_file(&header, sizeof(header));                // 写入头部

			f.write_file(newBar, sizeof(WTSBarStruct));          // 写入K线数据
		}
		else                                                     // 如果是已存在的文件
		{
			//日线必须要检查一下
			std::string content;                                  // 文件内容
			BoostFile::read_file_contents(filename.c_str(), content); // 读取文件内容
			HisKlineBlock* kBlock = (HisKlineBlock*)content.data(); // 获取K线数据块
			//如果老的文件已经是压缩版本,或者最终数据大小大于100条,则进行压缩
			bool bCompressed = kBlock->is_compressed();          // 检查是否已压缩

			//先统一解压出来
			proc_block_data(filename.c_str(), content, true, false); // 处理数据块数据
			
			uint32_t barcnt = content.size() / sizeof(WTSBarStruct); // 计算K线数量
			//开始比较K线时间标签,主要为了防止数据重复写
			if (barcnt != 0)                                     // 如果K线数量不为0
			{
				WTSBarStruct& oldBS = ((WTSBarStruct*)content.data())[barcnt - 1]; // 获取最后一条K线

				if (oldBS.date == newBar->date && memcmp(&oldBS, newBar, sizeof(WTSBarStruct)) != 0) // 如果日期相同且数据不同
				{
					//日期相同且数据不同,则用最新的替换最后一条
					oldBS = *newBar;                             // 替换最后一条K线
				}
				else if (oldBS.date < newBar->date)	//老的K线日期小于新的,则直接追加到后面
				{
					content.append((char*)newBar, sizeof(WTSBarStruct)); // 追加新K线
					barcnt++;                                     // 增加K线数量
				}
			}

			//如果老的文件已经是压缩版本,或者最终数据大小大于100条,则进行压缩
			bool bNeedCompress = bCompressed || (barcnt > 100);  // 判断是否需要压缩
			if (bNeedCompress)                                   // 如果需要压缩
			{
				std::string cmpData = WTSCmpHelper::compress_data(content.data(), content.size()); // 压缩数据
				BlockHeaderV2 header;                            // V2头部
				strcpy(header._blk_flag, BLK_FLAG);               // 设置块标志
				header._type = BT_HIS_Day;                       // 设置块类型
				header._version = BLOCK_VERSION_CMP_V2;          // 设置版本号
				header._size = cmpData.size();                   // 设置数据大小

				f.truncate_file(0);                              // 清空文件
				f.seek_to_begin();                               // 定位到文件开始
				f.write_file(&header, sizeof(header));           // 写入头部

				f.write_file(cmpData.data(), cmpData.size());    // 写入压缩数据
			}
			else                                                 // 如果不需要压缩
			{
				BlockHeader header;                               // 数据块头部
				strcpy(header._blk_flag, BLK_FLAG);               // 设置块标志
				header._type = BT_HIS_Day;                       // 设置块类型
				header._version = BLOCK_VERSION_RAW_V2;          // 设置版本号

				f.truncate_file(0);                              // 清空文件
				f.seek_to_begin();                               // 定位到文件开始
				f.write_file(&header, sizeof(header));           // 写入头部
				f.write_file(content.data(), content.size());    // 写入数据
			}
		}

		f.close_file();                                          // 关闭文件

		return true;                                             // 返回成功
	}
	else                                                         // 如果打开文件失败
	{
		pipe_writer_log(_sink, LL_ERROR, "ClosingTask of day bar failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
		return false;                                            // 返回失败
	}
}

/*!
 * \brief 转储K线数据到文件
 * \param ct 合约信息
 * \return 转储的K线数量
 * 
 * 该函数负责将实时K线数据转储到历史数据文件中。
 * 包括日线、1分钟线、5分钟线的转储功能。
 */
uint32_t WtDataWriter::dump_bars_to_file(WTSContractInfo* ct)
{
	if (ct == NULL)                                             // 如果合约信息为空
		return 0;                                               // 返回0

	std::string key = fmtutil::format("{}.{}", ct->getExchg(), ct->getCode()); // 构建合约键

	uint32_t count = 0;                                         // 转储计数器

	//从缓存中读取最新tick,更新到历史日线
	if (!_disable_day)                                          // 如果未禁用日线
	{
		auto it = _tick_cache_idx.find(key);                    // 查找Tick缓存索引
		if (it != _tick_cache_idx.end())                        // 如果找到缓存项
		{
			uint32_t idx = it->second;                          // 获取缓存索引

			const TickCacheItem& tci = _tick_cache_block->_ticks[idx]; // 获取缓存项
			const WTSTickStruct& ts = tci._tick;                // 获取Tick结构

			WTSBarStruct bs;                                     // 日线K线结构
			bs.date = ts.trading_date;                          // 设置交易日期
			bs.time = 0;                                         // 设置时间为0
			bs.open = ts.open;                                  // 设置开盘价
			bs.close = ts.price;                                // 设置收盘价
			bs.high = ts.high;                                  // 设置最高价
			bs.low = ts.low;                                    // 设置最低价
			bs.settle = ts.settle_price;                       // 设置结算价
			bs.vol = ts.total_volume;                           // 设置成交量
			bs.hold = ts.open_interest;                         // 设置持仓量
			bs.money = ts.total_turnover;                       // 设置成交额
			bs.add = ts.open_interest - ts.pre_interest;        // 设置持仓变化

			dump_day_data(ct, &bs);                             // 转储日线数据
		}
	}

	//转移实时1分钟线
	if (!_disable_min1)                                          // 如果未禁用1分钟K线
	{
		KBlockPair* kBlkPair = getKlineBlock(ct, KP_Minute1, false); // 获取1分钟K线数据块
		if (kBlkPair != NULL && kBlkPair->_block->_size > 0)    // 如果数据块存在且不为空
		{
			uint32_t size = kBlkPair->_block->_size;            // 获取数据块大小
			pipe_writer_log(_sink, LL_INFO, "Transfering min1 bars of {}...", ct->getFullCode()); // 输出日志
			SpinLock lock(kBlkPair->_mutex);                    // 获取数据块锁

			std::stringstream ss;                               // 字符串流
			ss << _base_dir << "his/min1/" << ct->getExchg() << "/"; // 构建路径
			BoostFile::create_directories(ss.str().c_str());    // 创建目录
			std::string path = ss.str();                        // 获取路径字符串
			BoostFile::create_directories(ss.str().c_str());    // 再次创建目录
			std::string filename = fmtutil::format("{}{}.dsb", path, ct->getCode()); // 构建文件名

			bool bNew = false;                                   // 是否为新文件标志
			if (!BoostFile::exists(filename.c_str()))            // 如果文件不存在
				bNew = true;                                     // 设置为新文件

			pipe_writer_log(_sink, LL_INFO, "Openning data storage faile: {}", filename.c_str()); // 输出日志

			BoostFile f;                                         // 文件对象
			if (f.create_or_open_file(filename.c_str()))        // 如果成功打开文件
			{
				std::string buffer;                              // 数据缓冲区
				bool bOldVer = false;                            // 是否老版本标志
				if (!bNew)                                       // 如果不是新文件
				{
					std::string content;                          // 文件内容
					BoostFile::read_file_contents(filename.c_str(), content); // 读取文件内容
					proc_block_data(filename.c_str(), content, true, false); // 处理数据块数据
					buffer.swap(content);                        // 交换缓冲区
				}

				//追加新的数据
				buffer.append((const char*)kBlkPair->_block->_bars, sizeof(WTSBarStruct)*size); // 追加新数据

				std::string cmpData = WTSCmpHelper::compress_data(buffer.data(), buffer.size()); // 压缩数据

				f.truncate_file(0);                              // 清空文件
				f.seek_to_begin(0);                              // 定位到文件开始

				BlockHeaderV2 header;                            // V2头部
				strcpy(header._blk_flag, BLK_FLAG);               // 设置块标志
				header._type = BT_HIS_Minute1;                   // 设置块类型
				header._version = BLOCK_VERSION_CMP_V2;          // 设置版本号
				header._size = cmpData.size();                   // 设置数据大小
				f.write_file(&header, sizeof(header));           // 写入头部
				f.write_file(cmpData);                           // 写入压缩数据
				count += size;                                   // 增加计数器

				//最后将缓存清空
				//memset(kBlkPair->_block->_bars, 0, sizeof(WTSBarStruct)*kBlkPair->_block->_size);
				kBlkPair->_block->_size = 0;                    // 重置数据块大小
			}
			else                                                 // 如果打开文件失败
			{
				pipe_writer_log(_sink, LL_ERROR, "ClosingTask of min1 bar failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
			}
		}

		if (kBlkPair)                                            // 如果数据块存在
			releaseBlock(kBlkPair);                             // 释放数据块
	}

	//第四步,转移实时5分钟线
	if (!_disable_min5)                                          // 如果未禁用5分钟K线
	{
		KBlockPair* kBlkPair = getKlineBlock(ct, KP_Minute5, false); // 获取5分钟K线数据块
		if (kBlkPair != NULL && kBlkPair->_block->_size > 0)    // 如果数据块存在且不为空
		{
			uint32_t size = kBlkPair->_block->_size;            // 获取数据块大小
			pipe_writer_log(_sink, LL_INFO, "Transfering min5 bar of {}...", ct->getFullCode()); // 输出日志
			SpinLock lock(kBlkPair->_mutex);                    // 获取数据块锁

			std::stringstream ss;                               // 字符串流
			ss << _base_dir << "his/min5/" << ct->getExchg() << "/"; // 构建路径
			BoostFile::create_directories(ss.str().c_str());    // 创建目录
			std::string path = ss.str();                        // 获取路径字符串
			BoostFile::create_directories(ss.str().c_str());    // 再次创建目录
			std::string filename = fmtutil::format("{}{}.dsb", path.c_str(), ct->getCode()); // 构建文件名

			bool bNew = false;                                   // 是否为新文件标志
			if (!BoostFile::exists(filename.c_str()))            // 如果文件不存在
				bNew = true;                                     // 设置为新文件

			pipe_writer_log(_sink, LL_INFO, "Openning data storage file: {}", filename.c_str()); // 输出日志

			BoostFile f;                                         // 文件对象
			if (f.create_or_open_file(filename.c_str()))        // 如果成功打开文件
			{
				std::string buffer;                              // 数据缓冲区
				bool bOldVer = false;                            // 是否老版本标志
				if (!bNew)                                       // 如果不是新文件
				{
					std::string content;                          // 文件内容
					BoostFile::read_file_contents(filename.c_str(), content); // 读取文件内容
					proc_block_data(filename.c_str(), content, true, false); // 处理数据块数据
					buffer.swap(content);                        // 交换缓冲区
				}

				buffer.append((const char*)kBlkPair->_block->_bars, sizeof(WTSBarStruct)*size); // 追加新数据

				std::string cmpData = WTSCmpHelper::compress_data(buffer.data(), buffer.size()); // 压缩数据

				f.truncate_file(0);                              // 清空文件
				f.seek_to_begin(0);                              // 定位到文件开始

				BlockHeaderV2 header;                            // V2头部
				strcpy(header._blk_flag, BLK_FLAG);               // 设置块标志
				header._type = BT_HIS_Minute5;                   // 设置块类型
				header._version = BLOCK_VERSION_CMP_V2;          // 设置版本号
				header._size = cmpData.size();                   // 设置数据大小
				f.write_file(&header, sizeof(header));           // 写入头部
				f.write_file(cmpData);                           // 写入压缩数据
				count += size;                                   // 增加计数器

				//最后将缓存清空
				kBlkPair->_block->_size = 0;                    // 重置数据块大小
			}
			else                                                 // 如果打开文件失败
			{
				pipe_writer_log(_sink, LL_ERROR, "ClosingTask of min5 bar failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
			}
		}

		if (kBlkPair)                                            // 如果数据块存在
			releaseBlock(kBlkPair);                             // 释放数据块
	}

	return count;                                                // 返回转储数量
}

/*!
 * \brief 处理循环
 * 
 * 该函数负责后台处理循环，包括数据转储、缓存清理等功能。
 * 在独立线程中运行，处理历史数据转储任务。
 */
void WtDataWriter::proc_loop()
{
	while (!_terminated)                                         // 当未终止时循环
	{
		if(_proc_que.empty())                                   // 如果处理队列为空
		{
			StdUniqueLock lock(_proc_mtx);                      // 获取处理锁
			_proc_cond.wait(_proc_mtx);                         // 等待处理通知
			continue;                                            // 继续循环
		}

		std::string fullcode;                                     // 完整代码
		try                                                      // 尝试获取任务
		{
			StdUniqueLock lock(_proc_mtx);                      // 获取处理锁
			fullcode = _proc_que.front().c_str();               // 获取队列前端任务
			_proc_que.pop();                                    // 移除队列前端任务
		}
		catch(std::exception& e)                                 // 捕获异常
		{
			pipe_writer_log(_sink, LL_ERROR, e.what());         // 输出错误日志
			continue;                                            // 继续循环
		}

		if (fullcode.compare(CMD_CLEAR_CACHE) == 0)            // 如果是清除缓存命令
		{
			//清理缓存
			SpinLock lock(_lck_tick_cache);                     // 获取Tick缓存锁

			std::set<std::string> setCodes;                     // 有效代码集合
			std::stringstream ss_snapshot;                      // 快照字符串流
			ss_snapshot << "date,exchg,code,open,high,low,close,settle,volume,turnover,openinterest,upperlimit,lowerlimit,preclose,presettle,preinterest" << std::endl << std::fixed; // 设置CSV头部
			for (auto it = _tick_cache_idx.begin(); it != _tick_cache_idx.end(); it++) // 遍历Tick缓存索引
			{
				const char* key = it->first.c_str();            // 获取合约键
				const StringVector& ay = StrUtil::split(key, "."); // 分割合约键
				WTSContractInfo* ct = _bd_mgr->getContract(ay[1].c_str(), ay[0].c_str()); // 获取合约信息
				if (ct != NULL)                                 // 如果合约信息存在
				{
					setCodes.insert(key);                        // 添加到有效代码集合

					uint32_t idx = it->second;                  // 获取缓存索引

					const TickCacheItem& tci = _tick_cache_block->_ticks[idx]; // 获取缓存项
					const WTSTickStruct& ts = tci._tick;        // 获取Tick结构
					ss_snapshot << ts.trading_date << ","       // 输出交易日期
						<< ts.exchg << ","                       // 输出交易所
						<< ts.code << ","                        // 输出合约代码
						<< ts.open << ","                        // 输出开盘价
						<< ts.high << ","                        // 输出最高价
						<< ts.low << ","                         // 输出最低价
						<< ts.price << ","                       // 输出最新价
						<< ts.settle_price << ","                // 输出结算价
						<< ts.total_volume << ","                // 输出总成交量
						<< ts.total_turnover << ","              // 输出总成交额
						<< ts.open_interest << ","               // 输出持仓量
						<< ts.upper_limit << ","                 // 输出涨停价
						<< ts.lower_limit << ","                 // 输出跌停价
						<< ts.pre_close << ","                   // 输出昨收价
						<< ts.pre_settle << ","                   // 输出昨结算价
						<< ts.pre_interest << std::endl;         // 输出昨持仓量
				}
				else                                             // 如果合约信息不存在
				{
					pipe_writer_log(_sink, LL_WARN, "{}[{}] expired, cache will be cleared", ay[1].c_str(), ay[0].c_str()); // 输出警告日志

					//删除已经过期代码的实时tick文件
					std::string path = fmtutil::format("{}rt/ticks/{}/{}.dmb", _base_dir, ay[0], ay[1]); // 构建文件路径
					BoostFile::delete_file(path.c_str());       // 删除文件
				}
			}

			//如果两组代码个数不同,说明有代码过期了,被排除了
			if(setCodes.size() != _tick_cache_idx.size())      // 如果有效代码数量与缓存索引数量不同
			{
				uint32_t diff = _tick_cache_idx.size() - setCodes.size(); // 计算过期代码数量

				uint32_t scale = setCodes.size() / CACHE_SIZE_STEP; // 计算缓存规模
				if (setCodes.size() % CACHE_SIZE_STEP != 0)     // 如果不能整除
					scale++;                                     // 增加规模

				uint32_t size = sizeof(RTTickCache) + sizeof(TickCacheItem)*scale*CACHE_SIZE_STEP; // 计算新缓存大小
				std::string buffer;                              // 新缓存缓冲区
				buffer.resize(size, 0);                         // 调整缓冲区大小

				RTTickCache* newCache = (RTTickCache*)buffer.data(); // 获取新缓存指针
				newCache->_capacity = scale*CACHE_SIZE_STEP;    // 设置缓存容量
				newCache->_type = BT_RT_Cache;                  // 设置块类型
				newCache->_size = setCodes.size();              // 设置缓存大小
				newCache->_version = BLOCK_VERSION_RAW_V2;      // 设置版本号
				strcpy(newCache->_blk_flag, BLK_FLAG);          // 设置块标志

				wt_hashmap<std::string, uint32_t> newIdxMap;    // 新索引映射表

				uint32_t newIdx = 0;                            // 新索引
				for (const std::string& key : setCodes)          // 遍历有效代码
				{
					uint32_t oldIdx = _tick_cache_idx[key];     // 获取旧索引
					newIdxMap[key] = newIdx;                    // 设置新索引映射

					memcpy(&newCache->_ticks[newIdx], &_tick_cache_block->_ticks[oldIdx], sizeof(TickCacheItem)); // 复制缓存项

					newIdx++;                                   // 增加新索引
				}

				//索引替换
				_tick_cache_idx = newIdxMap;                    // 替换索引映射表
				_tick_cache_file->close();                      // 关闭缓存文件
				_tick_cache_block = NULL;                       // 重置缓存块指针

				std::string filename = _base_dir + _cache_file;  // 构建缓存文件名
				BoostFile f;                                    // 文件对象
				if (f.create_new_file(filename.c_str()))        // 如果成功创建新文件
				{
					f.write_file(buffer.data(), buffer.size()); // 写入新缓存数据
					f.close_file();                             // 关闭文件
				}

				_tick_cache_file->map(filename.c_str());        // 映射新缓存文件
				_tick_cache_block = (RTTickCache*)_tick_cache_file->addr(); // 获取新缓存块指针
				
				pipe_writer_log(_sink, LL_INFO, "{} expired cache cleared totally", diff); // 输出日志
			}

			//将当日的日线快照落地到一个快照文件
			{
				std::stringstream ss;                             // 字符串流
				ss << _base_dir << "his/snapshot/";              // 构建快照路径
				BoostFile::create_directories(ss.str().c_str()); // 创建快照目录
				ss << TimeUtils::getCurDate() << ".csv";         // 构建快照文件名
				std::string path = ss.str();                     // 获取快照文件路径

				const std::string& content = ss_snapshot.str();   // 获取快照内容
				BoostFile f;                                     // 文件对象
				f.create_new_file(path.c_str());                 // 创建快照文件
				f.write_file(content.data());                    // 写入快照内容
				f.close_file();                                  // 关闭文件
			}

			int try_count = 0;                                   // 重试计数器
			do                                                  // 循环清理实时缓存文件
			{
				if(try_count >= 5)                              // 如果重试次数超过5次
				{
					pipe_writer_log(_sink, LL_ERROR, "Too many trys to clear rt cache files，skip"); // 输出错误日志
					break;                                      // 跳出循环
				}

				try_count++;                                     // 增加重试计数
				try                                             // 尝试清理文件
				{
					std::string path = fmtutil::format("{}rt/min1/", _base_dir); // 构建1分钟K线路径
					boost::filesystem::remove_all(boost::filesystem::path(path)); // 删除1分钟K线目录
					path = fmtutil::format("{}rt/min5/", _base_dir); // 构建5分钟K线路径
					boost::filesystem::remove_all(boost::filesystem::path(path)); // 删除5分钟K线目录
					path = fmtutil::format("{}rt/ticks/", _base_dir); // 构建Tick数据路径
					boost::filesystem::remove_all(boost::filesystem::path(path)); // 删除Tick数据目录
					path = fmtutil::format("{}rt/orders/", _base_dir); // 构建委托数据路径
					boost::filesystem::remove_all(boost::filesystem::path(path)); // 删除委托数据目录
					path = fmtutil::format("{}rt/queue/", _base_dir); // 构建队列数据路径
					boost::filesystem::remove_all(boost::filesystem::path(path)); // 删除队列数据目录
					path = fmtutil::format("{}rt/trans/", _base_dir); // 构建成交数据路径
					boost::filesystem::remove_all(boost::filesystem::path(path)); // 删除成交数据目录
					break;                                      // 跳出循环
				}
				catch (...)                                     // 捕获所有异常
				{
					pipe_writer_log(_sink, LL_ERROR, "Error occured while clearing rt cache files，retry in 300s"); // 输出错误日志
					std::this_thread::sleep_for(std::chrono::seconds(300)); // 休眠300秒
					continue;                                    // 继续循环
				}
			} while (true);                                     // 无限循环

			continue;                                            // 继续下一次循环
		}
		else if (StrUtil::startsWith(fullcode.c_str(), "MARK.", false)) // 如果指令以MARK.开头
		{
			//如果指令以MARK.开头,说明是标记指令,要写一条标记
			std::string filename = _base_dir + MARKER_FILE;     // 构建标记文件路径
			std::string sid = fullcode.substr(5);               // 获取会话ID
			uint32_t curDate = TimeUtils::getCurDate();         // 获取当前日期
			IniHelper iniHelper;                                // INI文件助手
			iniHelper.load(filename.c_str());                   // 加载标记文件
			iniHelper.writeInt("markers", sid.c_str(), curDate); // 写入标记日期
			iniHelper.save();                                    // 保存文件
			pipe_writer_log(_sink, LL_INFO, "ClosingTask mark of Trading session [{}] updated: {}", sid.c_str(), curDate); // 输出日志
		}

		auto pos = fullcode.find(".");                          // 查找分隔符位置
		std::string exchg = fullcode.substr(0, pos);            // 获取交易所代码
		std::string code = fullcode.substr(pos + 1);            // 获取合约代码
		WTSContractInfo* ct = _bd_mgr->getContract(code.c_str(), exchg.c_str()); // 获取合约信息
		if (ct == NULL)                                         // 如果合约信息为空
			continue;                                           // 继续下一次循环

		//如果历史数据被禁用，则不再进行收盘作业
		if (!_disable_his)                                      // 如果未禁用历史数据
		{
			uint32_t count = 0;                                 // 转储计数器

			uint32_t uDate = _sink->getTradingDate(ct->getFullCode()); // 获取交易日期
			//转移实时tick数据
			if (!_disable_tick)                                 // 如果未禁用Tick数据
			{
				TickBlockPair *tBlkPair = getTickBlock(ct, uDate, false); // 获取Tick数据块
				if (tBlkPair != NULL)                                 // 如果数据块存在
				{
					if (tBlkPair->_fstream)                          // 如果文件流存在
						tBlkPair->_fstream.reset();                  // 重置文件流

					if (tBlkPair->_block->_size > 0)                 // 如果数据块不为空
					{
						pipe_writer_log(_sink, LL_INFO, "Transfering tick data of {}...", fullcode.c_str()); // 输出日志
						SpinLock lock(tBlkPair->_mutex);             // 获取数据块锁

						for (auto& item : _dumpers)                   // 遍历转储器
						{
							const char* id = item.first.c_str();    // 获取转储器ID
							IHisDataDumper* dumper = item.second;    // 获取转储器指针
							bool bSucc = dumper->dumpHisTicks(fullcode.c_str(), tBlkPair->_block->_date, tBlkPair->_block->_ticks, tBlkPair->_block->_size); // 转储Tick数据
							if (!bSucc)                              // 如果转储失败
							{
								pipe_writer_log(_sink, LL_ERROR, "ClosingTask of tick of {} on {} via extended dumper {} failed", fullcode.c_str(), tBlkPair->_block->_date, id); // 输出错误日志
							}
						}

						{//////////////////////////////////////////////////////////////////////////
							//dump tick data to dsb file
							std::stringstream ss;                   // 字符串流
							ss << _base_dir << "his/ticks/" << ct->getExchg() << "/" << tBlkPair->_block->_date << "/"; // 构建Tick数据路径
							std::string path = ss.str();            // 获取路径字符串
							pipe_writer_log(_sink, LL_INFO, path.c_str()); // 输出路径日志
							BoostFile::create_directories(ss.str().c_str()); // 创建目录
							std::string filename = fmtutil::format("{}{}.dsb", path, code); // 构建文件名

							bool bNew = false;                      // 是否为新文件标志
							if (!BoostFile::exists(filename.c_str())) // 如果文件不存在
								bNew = true;                         // 设置为新文件

							pipe_writer_log(_sink, LL_INFO, "Openning data storage file: {}", filename.c_str()); // 输出日志
							BoostFile f;                            // 文件对象
							if (f.create_new_file(filename.c_str())) // 如果成功创建新文件
							{
								//先压缩数据
								std::string cmp_data = WTSCmpHelper::compress_data(tBlkPair->_block->_ticks, sizeof(WTSTickStruct)*tBlkPair->_block->_size); // 压缩Tick数据

								BlockHeaderV2 header;               // V2头部
								strcpy(header._blk_flag, BLK_FLAG);  // 设置块标志
								header._type = BT_HIS_Ticks;        // 设置块类型
								header._version = BLOCK_VERSION_CMP_V2; // 设置版本号
								header._size = cmp_data.size();      // 设置数据大小
								f.write_file(&header, sizeof(header)); // 写入头部

								f.write_file(cmp_data.c_str(), cmp_data.size()); // 写入压缩数据
								f.close_file();                     // 关闭文件

								count += tBlkPair->_block->_size;   // 增加计数器

								//最后将缓存清空
								//memset(tBlkPair->_block->_ticks, 0, sizeof(WTSTickStruct)*tBlkPair->_block->_size);
								tBlkPair->_block->_size = 0;        // 重置数据块大小
							}
							else                                     // 如果创建文件失败
							{
								pipe_writer_log(_sink, LL_ERROR, "ClosingTask of tick failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
							}
						}
					}
				}

				if (tBlkPair)                                    // 如果Tick数据块存在
					releaseBlock<TickBlockPair>(tBlkPair);       // 释放Tick数据块
			}

			//转移实时trans数据
			if (!_disable_trans)                                // 如果未禁用成交数据
			{
				TransBlockPair *tBlkPair = getTransBlock(ct, uDate, false); // 获取成交数据块
				if (tBlkPair != NULL && tBlkPair->_block->_size > 0) // 如果数据块存在且不为空
				{
					pipe_writer_log(_sink, LL_INFO, "Transfering transaction data of {}...", fullcode.c_str()); // 输出日志
					SpinLock lock(tBlkPair->_mutex);             // 获取数据块锁

					for (auto& item : _dumpers)                   // 遍历转储器
					{
						const char* id = item.first.c_str();    // 获取转储器ID
						IHisDataDumper* dumper = item.second;    // 获取转储器指针
						bool bSucc = dumper->dumpHisTrans(fullcode.c_str(), tBlkPair->_block->_date, tBlkPair->_block->_trans, tBlkPair->_block->_size); // 转储成交数据
						if (!bSucc)                              // 如果转储失败
						{
							pipe_writer_log(_sink, LL_ERROR, "ClosingTask of transaction of {} on {} via extended dumper {} failed", fullcode.c_str(), tBlkPair->_block->_date, id); // 输出错误日志
						}
					}

					{
						std::stringstream ss;                   // 字符串流
						ss << _base_dir << "his/trans/" << ct->getExchg() << "/" << tBlkPair->_block->_date << "/"; // 构建成交数据路径
						std::string path = ss.str();            // 获取路径字符串
						pipe_writer_log(_sink, LL_INFO, path.c_str()); // 输出路径日志
						BoostFile::create_directories(ss.str().c_str()); // 创建目录
						std::string filename = fmtutil::format("{}{}.dsb", path, code); // 构建文件名

						bool bNew = false;                      // 是否为新文件标志
						if (!BoostFile::exists(filename.c_str())) // 如果文件不存在
							bNew = true;                         // 设置为新文件

						pipe_writer_log(_sink, LL_INFO, "Openning data storage file: {}", filename.c_str()); // 输出日志
						BoostFile f;                            // 文件对象
						if (f.create_new_file(filename.c_str())) // 如果成功创建新文件
						{
							//先压缩数据
							std::string cmp_data = WTSCmpHelper::compress_data(tBlkPair->_block->_trans, sizeof(WTSTransStruct)*tBlkPair->_block->_size); // 压缩成交数据

							BlockHeaderV2 header;               // V2头部
							strcpy(header._blk_flag, BLK_FLAG);  // 设置块标志
							header._type = BT_HIS_Trnsctn;      // 设置块类型
							header._version = BLOCK_VERSION_CMP_V2; // 设置版本号
							header._size = cmp_data.size();      // 设置数据大小
							f.write_file(&header, sizeof(header)); // 写入头部

							f.write_file(cmp_data.c_str(), cmp_data.size()); // 写入压缩数据
							f.close_file();                     // 关闭文件

							count += tBlkPair->_block->_size;   // 增加计数器

							//最后将缓存清空
							//memset(tBlkPair->_block->_ticks, 0, sizeof(WTSTickStruct)*tBlkPair->_block->_size);
							tBlkPair->_block->_size = 0;        // 重置数据块大小
						}
						else                                     // 如果创建文件失败
						{
							pipe_writer_log(_sink, LL_ERROR, "ClosingTask of transaction failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
						}
					}
				}

				if (tBlkPair)                                    // 如果成交数据块存在
					releaseBlock<TransBlockPair>(tBlkPair);     // 释放成交数据块
			}

			//转移实时order数据
			if (!_disable_orddtl)                               // 如果未禁用委托详情数据
			{
				OrdDtlBlockPair *tBlkPair = getOrdDtlBlock(ct, uDate, false); // 获取委托详情数据块
				if (tBlkPair != NULL && tBlkPair->_block->_size > 0) // 如果数据块存在且不为空
				{
					pipe_writer_log(_sink, LL_INFO, "Transfering order detail data of {}...", fullcode.c_str()); // 输出日志
					SpinLock lock(tBlkPair->_mutex);             // 获取数据块锁

					for (auto& item : _dumpers)                   // 遍历转储器
					{
						const char* id = item.first.c_str();    // 获取转储器ID
						IHisDataDumper* dumper = item.second;    // 获取转储器指针
						bool bSucc = dumper->dumpHisOrdDtl(fullcode.c_str(), tBlkPair->_block->_date, tBlkPair->_block->_details, tBlkPair->_block->_size); // 转储委托详情数据
						if (!bSucc)                              // 如果转储失败
						{
							pipe_writer_log(_sink, LL_ERROR, "ClosingTask of order details of {} on {} via extended dumper {} failed", fullcode.c_str(), tBlkPair->_block->_date, id); // 输出错误日志
						}
					}

					{
						std::stringstream ss;                   // 字符串流
						ss << _base_dir << "his/orders/" << ct->getExchg() << "/" << tBlkPair->_block->_date << "/"; // 构建委托详情数据路径
						std::string path = ss.str();            // 获取路径字符串
						pipe_writer_log(_sink, LL_INFO, path.c_str()); // 输出路径日志
						BoostFile::create_directories(ss.str().c_str()); // 创建目录
						std::string filename = fmtutil::format("{}{}.dsb", path, code); // 构建文件名

						bool bNew = false;                      // 是否为新文件标志
						if (!BoostFile::exists(filename.c_str())) // 如果文件不存在
							bNew = true;                         // 设置为新文件

						pipe_writer_log(_sink, LL_INFO, "Openning data storage file: {}", filename.c_str()); // 输出日志
						BoostFile f;                            // 文件对象
						if (f.create_new_file(filename.c_str())) // 如果成功创建新文件
						{
							//先压缩数据
							std::string cmp_data = WTSCmpHelper::compress_data(tBlkPair->_block->_details, sizeof(WTSOrdDtlStruct)*tBlkPair->_block->_size); // 压缩委托详情数据

							BlockHeaderV2 header;               // V2头部
							strcpy(header._blk_flag, BLK_FLAG);  // 设置块标志
							header._type = BT_HIS_OrdDetail;    // 设置块类型
							header._version = BLOCK_VERSION_CMP_V2; // 设置版本号
							header._size = cmp_data.size();      // 设置数据大小
							f.write_file(&header, sizeof(header)); // 写入头部

							f.write_file(cmp_data.c_str(), cmp_data.size()); // 写入压缩数据
							f.close_file();                     // 关闭文件

							count += tBlkPair->_block->_size;   // 增加计数器

							//最后将缓存清空
							//memset(tBlkPair->_block->_ticks, 0, sizeof(WTSTickStruct)*tBlkPair->_block->_size);
							tBlkPair->_block->_size = 0;        // 重置数据块大小
						}
						else                                     // 如果创建文件失败
						{
							pipe_writer_log(_sink, LL_ERROR, "ClosingTask of order detail failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
						}
					}
				}

				if (tBlkPair)                                    // 如果委托详情数据块存在
					releaseBlock<OrdDtlBlockPair>(tBlkPair);     // 释放委托详情数据块
			}

			//转移实时queue数据
			if (!_disable_ordque)                               // 如果未禁用委托队列数据
			{
				OrdQueBlockPair *tBlkPair = getOrdQueBlock(ct, uDate, false); // 获取委托队列数据块
				if (tBlkPair != NULL && tBlkPair->_block->_size > 0) // 如果数据块存在且不为空
				{
					pipe_writer_log(_sink, LL_INFO, "Transfering order queue data of {}...", fullcode.c_str()); // 输出日志
					SpinLock lock(tBlkPair->_mutex);             // 获取数据块锁

					for (auto& item : _dumpers)                   // 遍历转储器
					{
						const char* id = item.first.c_str();    // 获取转储器ID
						IHisDataDumper* dumper = item.second;    // 获取转储器指针
						bool bSucc = dumper->dumpHisOrdQue(fullcode.c_str(), tBlkPair->_block->_date, tBlkPair->_block->_queues, tBlkPair->_block->_size); // 转储委托队列数据
						if (!bSucc)                              // 如果转储失败
						{
							pipe_writer_log(_sink, LL_ERROR, "ClosingTask of order queues of {} on {} via extended dumper {} failed", fullcode.c_str(), tBlkPair->_block->_date, id); // 输出错误日志
						}
					}

					{
						std::stringstream ss;                   // 字符串流
						ss << _base_dir << "his/queue/" << ct->getExchg() << "/" << tBlkPair->_block->_date << "/"; // 构建委托队列数据路径
						std::string path = ss.str();            // 获取路径字符串
						pipe_writer_log(_sink, LL_INFO, path.c_str()); // 输出路径日志
						BoostFile::create_directories(ss.str().c_str()); // 创建目录
						std::string filename = fmtutil::format("{}{}.dsb", path, code); // 构建文件名

						bool bNew = false;                      // 是否为新文件标志
						if (!BoostFile::exists(filename.c_str())) // 如果文件不存在
							bNew = true;                         // 设置为新文件

						pipe_writer_log(_sink, LL_INFO, "Openning data storage file: {}", filename.c_str()); // 输出日志
						BoostFile f;                            // 文件对象
						if (f.create_new_file(filename.c_str())) // 如果成功创建新文件
						{
							//先压缩数据
							std::string cmp_data = WTSCmpHelper::compress_data(tBlkPair->_block->_queues, sizeof(WTSOrdQueStruct)*tBlkPair->_block->_size); // 压缩委托队列数据

							BlockHeaderV2 header;               // V2头部
							strcpy(header._blk_flag, BLK_FLAG);  // 设置块标志
							header._type = BT_HIS_OrdQueue;     // 设置块类型
							header._version = BLOCK_VERSION_CMP_V2; // 设置版本号
							header._size = cmp_data.size();      // 设置数据大小
							f.write_file(&header, sizeof(header)); // 写入头部

							f.write_file(cmp_data.c_str(), cmp_data.size()); // 写入压缩数据
							f.close_file();                     // 关闭文件

							count += tBlkPair->_block->_size;   // 增加计数器

							//最后将缓存清空
							//memset(tBlkPair->_block->_ticks, 0, sizeof(WTSTickStruct)*tBlkPair->_block->_size);
							tBlkPair->_block->_size = 0;        // 重置数据块大小
						}
						else                                     // 如果创建文件失败
						{
							pipe_writer_log(_sink, LL_ERROR, "ClosingTask of order queue failed: openning history data file {} failed", filename.c_str()); // 输出错误日志
						}
					}
				}

				if (tBlkPair)                                    // 如果委托队列数据块存在
					releaseBlock<OrdQueBlockPair>(tBlkPair);     // 释放委托队列数据块
			}

			//转移历史K线
			dump_bars_via_dumper(ct);                           // 通过转储器转储K线数据

			count += dump_bars_to_file(ct);                     // 转储K线数据到文件

			pipe_writer_log(_sink, LL_INFO, "ClosingTask of {}[{}] done, {} datas processed totally", ct->getCode(), ct->getExchg(), count); // 输出完成日志
		}
		else                                                     // 如果历史数据被禁用
		{
			pipe_writer_log(_sink, LL_INFO, "ClosingTask of {}[{}] skipped due to history data disabled", ct->getCode(), ct->getExchg()); // 输出跳过日志
		}
	}
}