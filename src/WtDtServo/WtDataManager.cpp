/*!
 * \file WtDataManager.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WtDataManager（数据管理器）类的具体实现，提供了数据管理器的完整功能实现。
 * 该文件实现了历史数据读取、K线数据生成、实时K线更新、数据缓存管理等核心功能。
 * 
 * 核心实现机制：
 * 
 * 1. 数据存储模块加载（Data Storage Module Loading）：
 *    - 动态加载IRdmDtReader模块
 *    - 通过符号表获取createRdmDtReader和deleteRdmDtReader函数
 *    - 支持模块路径的自动查找和解析
 * 
 * 2. 数据查询实现（Data Query Implementation）：
 *    - 实现多种数据查询方式（按日期、范围、数量）
 *    - 处理时间格式转换（统一转换为微秒时间戳）
 *    - 封装数据切片对象，便于返回和释放
 * 
 * 3. K线数据缓存（K-line Data Caching）：
 *    - 缓存已查询的K线数据，避免重复查询
 *    - 使用键值映射存储缓存（键=合约代码+日期+周期）
 *    - 支持缓存清理和释放
 * 
 * 4. 实时K线生成（Real-time Bar Generation）：
 *    - 从Tick数据实时生成K线
 *    - 支持多种K线周期（分钟、秒级等）
 *    - 订阅管理，只生成订阅的K线
 *    - 线程安全的实时K线更新
 * 
 * 5. 交易时段处理（Trading Session Handling）：
 *    - 获取交易时段信息
 *    - 支持按交易时段对齐K线数据
 *    - 处理不同交易所的交易时段差异
 * 
 * 主要功能实现：
 * 
 * 1. 初始化和清理：
 *    - 构造函数和析构函数
 *    - init()：初始化数据管理器
 *    - initStore()：初始化数据存储模块
 *    - clear_cache()：清理缓存
 * 
 * 2. 数据查询功能：
 *    - 各种数据查询接口的实现
 *    - 时间格式转换和封装
 *    - 数据切片的创建和返回
 * 
 * 3. 实时K线管理：
 *    - subscribe_bar()：订阅实时K线
 *    - update_bars()：更新K线数据
 *    - clear_subbed_bars()：清除订阅
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
#include "WtDataManager.h"                                                       // 包含数据管理器头文件
#include "WtDtRunner.h"                                                         // 包含数据服务运行器头文件
#include "WtHelper.h"                                                           // 包含辅助工具类（用于获取模块目录）

#include "../Includes/WTSDataDef.hpp"                                            // 包含数据结构定义
#include "../Includes/WTSVariant.hpp"                                            // 包含配置变体类
#include "../Includes/WTSContractInfo.hpp"                                       // 包含合约信息类

#include "../Share/StrUtil.hpp"                                                 // 包含字符串工具类
#include "../Share/TimeUtils.hpp"                                                // 包含时间工具类
#include "../Share/CodeHelper.hpp"                                               // 包含代码解析工具
#include "../Share/DLLHelper.hpp"                                                // 包含动态库加载工具

#include "../WTSTools/WTSLogger.h"                                               // 包含日志工具类
#include "../WTSTools/WTSDataFactory.h"                                         // 包含数据工厂类（用于生成K线数据）


WTSDataFactory g_dataFact;                                                       // 全局数据工厂对象：用于生成K线数据等

/**
 * @brief WtDataManager构造函数
 * 
 * 初始化数据管理器对象，设置所有成员变量为初始值。
 * 基础数据管理器、主力合约管理器、运行器等指针初始化为NULL。
 */
WtDataManager::WtDataManager()
	: _bd_mgr(NULL)                                                              // 基础数据管理器指针初始化为NULL
	, _hot_mgr(NULL)                                                             // 主力合约管理器指针初始化为NULL
	, _runner(NULL)                                                               // 数据服务运行器指针初始化为NULL
	, _reader(NULL)                                                               // 数据读取器指针初始化为NULL
	, _rt_bars(NULL)                                                              // 实时K线映射表指针初始化为NULL
{
}

/**
 * @brief WtDataManager析构函数
 * 
 * 清理数据管理器资源，释放所有缓存的数据。
 * 遍历K线缓存映射表，释放每个缓存项的K线数据。
 */
WtDataManager::~WtDataManager()
{
	for(auto& m : _bars_cache)                                                   // 遍历K线缓存映射表
	{
		if (m.second._bars != NULL)                                              // 如果缓存项有K线数据
			m.second._bars->release();                                           // 释放K线数据对象
	}
	_bars_cache.clear();                                                         // 清空缓存映射表
}

/**
 * @brief 初始化数据存储模块
 * @param cfg 配置信息（包含数据存储模块配置）
 * @return 是否初始化成功
 * 
 * 根据配置信息加载数据存储模块（IRdmDtReader），用于读取历史数据。
 * 会动态加载数据存储模块动态库，获取创建和删除函数，然后初始化数据读取器。
 */
bool WtDataManager::initStore(WTSVariant* cfg)
{
	if (cfg == NULL)                                                             // 如果配置信息为空
		return false;                                                            // 返回false表示失败

	std::string module = cfg->getCString("module");                              // 从配置中获取模块名（数据存储模块名）
	if (module.empty())                                                          // 如果模块名为空
		module = "WtDataStorage";                                               // 使用默认模块名"WtDataStorage"

	module = WtHelper::get_module_dir() + DLLHelper::wrap_module(module.c_str());  // 拼接完整的模块路径（模块目录 + 包装后的模块名，如"libWtDataStorage.so"）
	DllHandle libParser = DLLHelper::load_library(module.c_str());              // 动态加载数据存储模块动态库
	if (libParser)                                                               // 如果加载成功
	{
		FuncCreateRdmDtReader pFuncCreateReader = (FuncCreateRdmDtReader)DLLHelper::get_symbol(libParser, "createRdmDtReader");  // 从动态库中获取createRdmDtReader函数符号
		if (pFuncCreateReader == NULL)                                           // 如果函数符号不存在
		{
			WTSLogger::error("Initializing of random data reader failed: function createRdmDtReader not found...");  // 记录错误日志
		}

		FuncDeleteRdmDtReader pFuncDeleteReader = (FuncDeleteRdmDtReader)DLLHelper::get_symbol(libParser, "deleteRdmDtReader");  // 从动态库中获取deleteRdmDtReader函数符号
		if (pFuncDeleteReader == NULL)                                           // 如果函数符号不存在
		{
			WTSLogger::error("Initializing of random data reader failed: function deleteRdmDtReader not found...");  // 记录错误日志
		}

		if (pFuncCreateReader && pFuncDeleteReader)                              // 如果两个函数符号都存在
		{
			_reader = pFuncCreateReader();                                       // 调用createRdmDtReader函数创建数据读取器对象
			_remover = pFuncDeleteReader;                                       // 保存删除函数指针（用于后续释放数据读取器）
		}

	}
	else                                                                          // 如果加载失败
	{
		WTSLogger::error("Initializing of random data reader failed: loading module {} failed...", module);  // 记录错误日志

	}

	_reader->init(cfg, this);                                                    // 初始化数据读取器（传入配置信息和this指针作为回调接收者）
	return true;                                                                  // 返回true表示成功
}

/**
 * @brief 初始化数据管理器
 * @param cfg 配置信息
 * @param runner 数据服务运行器指针
 * @return 是否初始化成功
 * 
 * 初始化数据管理器，设置基础数据管理器、主力合约管理器等。
 * 配置信息应包含数据存储模块的配置。
 */
bool WtDataManager::init(WTSVariant* cfg, WtDtRunner* runner)
{
	_runner = runner;                                                             // 保存数据服务运行器指针
	if (_runner)                                                                 // 如果运行器指针有效
	{
		_bd_mgr = &_runner->getBaseDataMgr();                                   // 获取基础数据管理器引用
		_hot_mgr = &_runner->getHotMgr();                                       // 获取主力合约管理器引用
	}

	_align_by_section = cfg->getBoolean("align_by_section");                    // 从配置中获取是否按交易时段对齐K线数据的标志

	WTSLogger::info("Resampled bars will be aligned by section: {}", _align_by_section ? "yes" : " no");  // 记录信息日志，显示K线对齐设置

	return initStore(cfg->get("store"));                                         // 初始化数据存储模块（从配置中获取store配置节点）
}

/**
 * @brief 输出数据读取模块的日志
 * @param ll 日志级别
 * @param message 日志消息
 * 
 * 将数据读取模块的日志转发到WonderTrader的日志系统。
 * 这是IRdmDtReaderSink接口的实现方法。
 */
void WtDataManager::reader_log(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_raw(ll, message);                                            // 将日志转发到WonderTrader的日志系统（使用原始日志接口）
}

/**
 * @brief 按时间范围查询Tick数据切片
 * @param stdCode 标准化合约代码
 * @param stime 开始时间（格式：yyyymmddHHMMSS）
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return Tick数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的Tick数据。
 * 时间会被转换为微秒时间戳（乘以100000）后传递给数据读取器。
 */
WTSTickSlice* WtDataManager::get_tick_slices_by_range(const char* stdCode,uint64_t stime, uint64_t etime /* = 0 */)
{
	stime = stime * 100000;                                                      // 将开始时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	etime = etime * 100000;                                                      // 将结束时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	return _reader->readTickSliceByRange(stdCode, stime, etime);               // 调用数据读取器查询指定时间范围内的Tick数据切片
}

/**
 * @brief 按日期查询Tick数据切片
 * @param stdCode 标准化合约代码
 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
 * @return Tick数据切片指针（需要调用release()释放）
 * 
 * 查询指定交易日的所有Tick数据。
 * 直接调用数据读取器查询指定日期的Tick数据。
 */
WTSTickSlice* WtDataManager::get_tick_slice_by_date(const char* stdCode, uint32_t uDate /* = 0 */)
{
	return _reader->readTickSliceByDate(stdCode, uDate);                        // 调用数据读取器查询指定日期的Tick数据切片
}

/**
 * @brief 查询委托队列数据切片
 * @param stdCode 标准化合约代码
 * @param stime 开始时间（格式：yyyymmddHHMMSS）
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return 委托队列数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的委托队列数据（Level-2数据）。
 * 时间会被转换为微秒时间戳（乘以100000）后传递给数据读取器。
 */
WTSOrdQueSlice* WtDataManager::get_order_queue_slice(const char* stdCode,uint64_t stime, uint64_t etime /* = 0 */)
{
	stime = stime * 100000;                                                      // 将开始时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	etime = etime * 100000;                                                      // 将结束时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	return _reader->readOrdQueSliceByRange(stdCode, stime, etime);              // 调用数据读取器查询指定时间范围内的委托队列数据切片
}

/**
 * @brief 查询逐笔委托数据切片
 * @param stdCode 标准化合约代码
 * @param stime 开始时间（格式：yyyymmddHHMMSS）
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return 逐笔委托数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的逐笔委托数据（Level-2数据）。
 * 时间会被转换为微秒时间戳（乘以100000）后传递给数据读取器。
 */
WTSOrdDtlSlice* WtDataManager::get_order_detail_slice(const char* stdCode,uint64_t stime, uint64_t etime /* = 0 */)
{
	stime = stime * 100000;                                                      // 将开始时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	etime = etime * 100000;                                                      // 将结束时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	return _reader->readOrdDtlSliceByRange(stdCode, stime, etime);             // 调用数据读取器查询指定时间范围内的逐笔委托数据切片
}

/**
 * @brief 查询逐笔成交数据切片
 * @param stdCode 标准化合约代码
 * @param stime 开始时间（格式：yyyymmddHHMMSS）
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return 逐笔成交数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的逐笔成交数据（Level-2数据）。
 * 时间会被转换为微秒时间戳（乘以100000）后传递给数据读取器。
 */
WTSTransSlice* WtDataManager::get_transaction_slice(const char* stdCode,uint64_t stime, uint64_t etime /* = 0 */)
{
	stime = stime * 100000;                                                      // 将开始时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	etime = etime * 100000;                                                      // 将结束时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	return _reader->readTransSliceByRange(stdCode, stime, etime);               // 调用数据读取器查询指定时间范围内的逐笔成交数据切片
}

/**
 * @brief 获取交易时段信息
 * @param sid 合约代码或品种ID
 * @param isCode 是否为合约代码（true=合约代码，false=品种ID）
 * @return 交易时段信息指针（如果不存在则返回NULL）
 * 
 * 获取指定合约或品种的交易时段信息，用于K线生成和数据对齐。
 * 如果isCode为false，则直接从基础数据管理器获取交易时段；
 * 如果isCode为true，则先解析合约代码，获取品种信息，再获取交易时段。
 */
WTSSessionInfo* WtDataManager::get_session_info(const char* sid, bool isCode /* = false */)
{
	if (!isCode)                                                                 // 如果不是合约代码（即sid是交易时段ID）
		return _bd_mgr->getSession(sid);                                         // 直接从基础数据管理器获取交易时段信息

	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(sid, _hot_mgr);  // 解析标准化合约代码，提取交易所、品种、合约等信息
	WTSCommodityInfo* cInfo = _bd_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 根据交易所和品种获取品种信息
	if (cInfo == NULL)                                                           // 如果找不到品种信息
		return NULL;                                                              // 返回NULL

	return cInfo->getSessionInfo();                                              // 返回品种的交易时段信息
}

/**
 * @brief 按日期查询秒级K线数据切片
 * @param stdCode 标准化合约代码
 * @param secs 秒数（如：60表示60秒K线）
 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 从Tick数据生成指定秒数的K线数据。
 * 使用缓存机制，第一次查询时从Tick数据生成并缓存，后续查询直接使用缓存。
 */
WTSKlineSlice* WtDataManager::get_skline_slice_by_date(const char* stdCode, uint32_t secs, uint32_t uDate /* = 0 */)
{
	std::string key = StrUtil::printf("%s-%u-s%u", stdCode, uDate, secs);        // 构建缓存键（格式：合约代码-日期-s秒数，如"SSE.600000-20240101-s60"）

	//只有非基础周期的会进到下面的步骤（秒级K线需要通过Tick数据生成）
	WTSSessionInfo* sInfo = get_session_info(stdCode, true);                    // 获取合约的交易时段信息（用于K线生成时考虑交易时段）
	BarCache& barCache = _bars_cache[key];                                       // 获取或创建K线缓存项（如果不存在则自动创建）
	barCache._period = KP_Tick;                                                   // 设置K线周期为Tick级别（表示从Tick数据生成）
	barCache._times = secs;                                                       // 设置周期倍数为秒数
	if (barCache._bars == NULL)                                                  // 如果缓存中没有K线数据（第一次查询）
	{
		//第一次将全部数据缓存到内存中
		WTSTickSlice* ticks = _reader->readTickSliceByDate(stdCode, uDate);      // 从数据读取器读取指定日期的所有Tick数据
		if (ticks != NULL)                                                        // 如果读取成功
		{
			WTSKlineData* kData = g_dataFact.extractKlineData(ticks, secs, sInfo, true);  // 使用数据工厂从Tick数据生成指定秒数的K线数据（true表示强制对齐）
			barCache._bars = kData;                                               // 将生成的K线数据保存到缓存中
			ticks->release();                                                     // 释放Tick数据切片对象
		}
		else                                                                     // 如果读取失败
		{
			return NULL;                                                          // 返回NULL表示查询失败
		}
	}
	
	if (barCache._bars == NULL)                                                  // 如果缓存中仍然没有K线数据（异常情况）
		return NULL;                                                              // 返回NULL表示查询失败

	WTSBarStruct* rtHead = barCache._bars->at(0);                                // 获取缓存中第一条K线数据的指针
	WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, KP_Tick, secs, rtHead, barCache._bars->size());  // 创建K线数据切片对象（包装缓存中的数据，方便返回）
	return slice;                                                                 // 返回K线数据切片
}

/**
 * @brief 按日期查询K线数据切片
 * @param stdCode 标准化合约代码
 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定交易日的K线数据。
 * 先获取该交易日的开始和结束时间，然后调用按范围查询的方法。
 */
WTSKlineSlice* WtDataManager::get_kline_slice_by_date(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t uDate /* = 0 */)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准化合约代码，提取交易所、品种、合约等信息
	uint64_t stime = _bd_mgr->getBoundaryTime(codeInfo.stdCommID(), uDate, false, true);  // 获取交易日的开始时间（false表示不包含边界，true表示开始时间）
	uint64_t etime = _bd_mgr->getBoundaryTime(codeInfo.stdCommID(), uDate, false, false);  // 获取交易日的结束时间（false表示不包含边界，false表示结束时间）
	return get_kline_slice_by_range(stdCode, period, times, stime, etime);      // 调用按范围查询的方法，返回该交易日的K线数据切片
}

/**
 * @brief 按时间范围查询K线数据切片
 * @param stdCode 标准化合约代码
 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
 * @param stime 开始时间（格式：yyyymmddHHMMSS）
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的K线数据。
 * 如果times为1（基础周期），直接调用数据读取器查询；
 * 如果times大于1（非基础周期），需要使用缓存机制，先从基础周期数据生成，然后从缓存中提取指定范围的数据。
 */
WTSKlineSlice* WtDataManager::get_kline_slice_by_range(const char* stdCode, WTSKlinePeriod period, uint32_t times,uint64_t stime, uint64_t etime /* = 0 */)
{
	if (times == 1)                                                              // 如果是基础周期（times=1，不需要重采样）
	{
		return _reader->readKlineSliceByRange(stdCode, period, stime, etime);    // 直接调用数据读取器查询基础周期的K线数据
	}

	//只有非基础周期的会进到下面的步骤（需要从基础周期数据重采样生成）
	WTSSessionInfo* sInfo = get_session_info(stdCode, true);                    // 获取合约的交易时段信息（用于K线生成时考虑交易时段）
	std::string key = StrUtil::printf("%s-%u-%u", stdCode, period, times);       // 构建缓存键（格式：合约代码-周期-倍数，如"SSE.600000-1-5"表示1分钟周期的5倍，即5分钟K线）
	BarCache& barCache = _bars_cache[key];                                       // 获取或创建K线缓存项（如果不存在则自动创建）
	barCache._period = period;                                                   // 设置K线周期
	barCache._times = times;                                                     // 设置周期倍数
	if(barCache._bars == NULL)                                                   // 如果缓存中没有K线数据（第一次查询）
	{
		//第一次将全部数据缓存到内存中
		WTSKlineSlice* rawData = _reader->readKlineSliceByCount(stdCode, period, UINT_MAX, 0);  // 从数据读取器读取所有基础周期的K线数据（UINT_MAX表示读取所有数据）
		if (rawData != NULL)                                                     // 如果读取成功
		{
			WTSKlineData* kData = g_dataFact.extractKlineData(rawData, period, times, sInfo, false);  // 使用数据工厂从基础周期K线数据生成目标周期的K线数据（false表示不强制对齐）
			barCache._bars = kData;                                              // 将生成的K线数据保存到缓存中

			//不管如何，都删除最后一条K线
			//不能通过闭合标记判断，因为读取的基础周期可能本身没有闭合
			if (barCache._bars->size() > 0)                                      // 如果生成的K线数据不为空
			{
				auto& bars = barCache._bars->getDataRef();                      // 获取K线数据的引用（用于修改）
				bars.erase(bars.begin() + bars.size() - 1, bars.end());         // 删除最后一条K线（因为可能未闭合）
			}

			if (period == KP_DAY)                                                // 如果是日线周期
				barCache._last_bartime = kData->date(-1);                       // 记录最后一条K线的日期（用于后续增量更新）
			else                                                                 // 如果是分钟线周期
			{
				uint64_t lasttime = kData->time(-1);                             // 获取最后一条K线的时间（相对时间）
				barCache._last_bartime = 199000000000 + lasttime;                // 将相对时间转换为绝对时间戳（199000000000是基准日期的时间戳）
			}

			rawData->release();                                                  // 释放原始数据切片对象
		}
		else                                                                     // 如果读取失败
		{
			return NULL;                                                         // 返回NULL表示查询失败
		}
	}
	else                                                                         // 如果缓存中已有K线数据（后续查询）
	{
		//后面则增量更新
		WTSKlineSlice* rawData = _reader->readKlineSliceByRange(stdCode, period, barCache._last_bartime, 0);  // 从上次记录的最后一条K线时间开始读取新的基础周期数据（增量更新）
		if (rawData != NULL)                                                     // 如果读取成功
		{
			for(int32_t idx = 0; idx < rawData->size(); idx ++)                // 遍历所有新读取的基础周期K线数据
			{
				uint64_t barTime = 0;                                            // K线时间戳
				if (period == KP_DAY)                                            // 如果是日线周期
					barTime = rawData->at(0)->date;                              // 使用日期作为时间戳
				else                                                             // 如果是分钟线周期
					barTime = 199000000000 + rawData->at(0)->time;               // 将相对时间转换为绝对时间戳
				
				//只有时间上次记录的最后一条时间，才可以用于更新K线
				if(barTime <= barCache._last_bartime)                           // 如果K线时间小于等于上次记录的最后时间（可能重复或无效）
					continue;                                                    // 跳过这条数据，不用于更新

				g_dataFact.updateKlineData(barCache._bars, rawData->at(idx), sInfo, _align_by_section);  // 使用数据工厂更新K线数据（将新的基础周期K线合并到目标周期K线中）
			}

			//不管如何，都删除最后一条K线
			//不能通过闭合标记判断，因为读取的基础周期可能本身没有闭合
			if(barCache._bars->size() > 0)                                      // 如果K线数据不为空
			{
				auto& bars = barCache._bars->getDataRef();                      // 获取K线数据的引用（用于修改）
				bars.erase(bars.begin() + bars.size() - 1, bars.end());         // 删除最后一条K线（因为可能未闭合）
			}

			if (period == KP_DAY)                                                // 如果是日线周期
				barCache._last_bartime = barCache._bars->date(-1);              // 更新最后一条K线的日期
			else                                                                 // 如果是分钟线周期
			{
				uint64_t lasttime = barCache._bars->time(-1);                   // 获取最后一条K线的时间（相对时间）
				barCache._last_bartime = 199000000000 + lasttime;                // 更新最后一条K线的绝对时间戳
			}
			

			rawData->release();                                                  // 释放原始数据切片对象
		}
	}

	//最后到缓存中定位指定时间范围的数据
	bool isDay = period == KP_DAY;                                               // 判断是否为日线周期（用于时间比较）
	uint32_t rDate, rTime, lDate, lTime;                                        // 结束日期、结束时间、开始日期、开始时间
	rDate = (uint32_t)(etime / 10000);                                          // 提取结束日期（yyyymmddHHMMSS的前8位）
	rTime = (uint32_t)(etime % 10000);                                          // 提取结束时间（yyyymmddHHMMSS的后4位，HHMM格式）
	lDate = (uint32_t)(stime / 10000);                                           // 提取开始日期（yyyymmddHHMMSS的前8位）
	lTime = (uint32_t)(stime % 10000);                                           // 提取开始时间（yyyymmddHHMMSS的后4位，HHMM格式）

	WTSBarStruct eBar;                                                           // 结束K线结构（用于二分查找）
	eBar.date = rDate;                                                           // 设置结束K线的日期
	eBar.time = (rDate - 19900000) * 10000 + rTime;                              // 设置结束K线的时间（将日期转换为相对时间：日期差*10000+时间）

	WTSBarStruct sBar;                                                           // 开始K线结构（用于二分查找）
	sBar.date = lDate;                                                           // 设置开始K线的日期
	sBar.time = (lDate - 19900000) * 10000 + lTime;                              // 设置开始K线的时间（将日期转换为相对时间：日期差*10000+时间）

	uint32_t eIdx, sIdx;                                                         // 结束索引、开始索引
	auto& bars = barCache._bars->getDataRef();                                  // 获取K线数据的引用
	auto eit = std::lower_bound(bars.begin(), bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找在K线数据中定位结束K线的位置
		if (isDay)                                                               // 如果是日线周期
			return a.date < b.date;                                              // 按日期比较
		else                                                                      // 如果是分钟线周期
			return a.time < b.time;                                              // 按时间比较
	});


	if (eit == bars.end())                                                       // 如果查找结果指向数据末尾（说明结束时间超出数据范围）
		eIdx = bars.size() - 1;                                                  // 结束索引设为最后一条数据的索引
	else                                                                         // 如果找到了
	{
		if ((isDay && eit->date > eBar.date) || (!isDay && eit->time > eBar.time))  // 如果找到的K线时间大于结束时间（需要前移一条）
		{
			eit--;                                                                // 向前移动一个位置
		}

		eIdx = eit - bars.begin();                                               // 计算结束索引（迭代器距离）
	}

	auto sit = std::lower_bound(bars.begin(), eit, sBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找在开始到结束范围内定位开始K线的位置
		if (isDay)                                                               // 如果是日线周期
			return a.date < b.date;                                              // 按日期比较
		else                                                                      // 如果是分钟线周期
			return a.time < b.time;                                              // 按时间比较
	});
	sIdx = sit - bars.begin();                                                   // 计算开始索引（迭代器距离）
	uint32_t rtCnt = eIdx - sIdx + 1;                                            // 计算返回的数据条数（结束索引-开始索引+1）
	WTSBarStruct* rtHead = barCache._bars->at(sIdx);                             // 获取开始位置K线数据的指针
	WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, times, rtHead, rtCnt);  // 创建K线数据切片对象（包装指定范围的数据）
	return slice;                                                                 // 返回K线数据切片
}

/**
 * @brief 按数量查询K线数据切片
 * @param stdCode 标准化合约代码
 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
 * @param count 查询条数
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定数量的K线数据（从结束时间向前查找）。
 * 如果times为1（基础周期），直接调用数据读取器查询；
 * 如果times大于1（非基础周期），需要使用缓存机制，先从基础周期数据生成，然后从缓存中提取指定数量的数据。
 * 与get_kline_slice_by_range的区别是：这个方法按数量查询，而不是按时间范围查询。
 */
WTSKlineSlice* WtDataManager::get_kline_slice_by_count(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime /* = 0 */)
{
	if (times == 1)                                                              // 如果是基础周期（times=1，不需要重采样）
	{
		return _reader->readKlineSliceByCount(stdCode, period, count, etime);    // 直接调用数据读取器查询基础周期的K线数据（按数量查询）
	}

	//只有非基础周期的会进到下面的步骤（需要从基础周期数据重采样生成）
	WTSSessionInfo* sInfo = get_session_info(stdCode, true);                    // 获取合约的交易时段信息（用于K线生成时考虑交易时段）
	std::string key = StrUtil::printf("%s-%u-%u", stdCode, period, times);       // 构建缓存键（格式：合约代码-周期-倍数）
	BarCache& barCache = _bars_cache[key];                                       // 获取或创建K线缓存项（如果不存在则自动创建）
	barCache._period = period;                                                   // 设置K线周期
	barCache._times = times;                                                     // 设置周期倍数

	const char* tag = PERIOD_NAME[period-KP_Tick];                               // 获取周期名称（用于日志输出）

	if (barCache._bars == NULL)                                                  // 如果缓存中没有K线数据（第一次查询）
	{
		//第一次将全部数据缓存到内存中
		WTSLogger::info("Caching all {} bars of {}...", tag, stdCode);           // 记录信息日志：开始缓存所有K线数据
		WTSKlineSlice* rawData = _reader->readKlineSliceByCount(stdCode, period, UINT_MAX, 0);  // 从数据读取器读取所有基础周期的K线数据（UINT_MAX表示读取所有数据）
		if (rawData != NULL)                                                     // 如果读取成功
		{
			WTSLogger::info("Resampling {} {} bars by {}-TO-1 of {}...", rawData->size(), tag, times, stdCode);  // 记录信息日志：开始重采样K线数据
			WTSKlineData* kData = g_dataFact.extractKlineData(rawData, period, times, sInfo, true);  // 使用数据工厂从基础周期K线数据生成目标周期的K线数据（true表示强制对齐）
			barCache._bars = kData;                                              // 将生成的K线数据保存到缓存中

			//如果不是日线，要考虑最后一条K线是否闭合的情况
			//这里采用保守的方案，如果本地时间大于最后一条K线的时间，则认为真正闭合了
			if (period != KP_DAY)                                                // 如果不是日线周期（分钟线需要考虑是否闭合）
			{
				uint64_t last_bartime = 0;                                       // 最后一条K线的绝对时间戳
				last_bartime = 199000000000 + kData->time(-1);                     // 将相对时间转换为绝对时间戳

				uint64_t now = TimeUtils::getYYYYMMDDhhmmss() / 100;             // 获取当前时间（格式：yyyymmddHHMMSS，除以100是因为函数返回的是更精确的时间）
				if (now <= last_bartime && barCache._bars->size() > 0)           // 如果当前时间小于等于最后一条K线时间（说明最后一条K线可能未闭合）
				{
					auto& bars = barCache._bars->getDataRef();                   // 获取K线数据的引用（用于修改）
					bars.erase(bars.begin() + bars.size() - 1, bars.end());      // 删除最后一条K线（因为可能未闭合）
				}
			}


			if (period == KP_DAY)                                                // 如果是日线周期
				barCache._last_bartime = kData->date(-1);                       // 记录最后一条K线的日期（用于后续增量更新）
			else                                                                 // 如果是分钟线周期
			{
				uint64_t lasttime = kData->time(-1);                             // 获取最后一条K线的时间（相对时间）
				barCache._last_bartime = 199000000000 + lasttime;                // 将相对时间转换为绝对时间戳
			}

			rawData->release();                                                  // 释放原始数据切片对象
		}
		else                                                                     // 如果读取失败
		{
			return NULL;                                                         // 返回NULL表示查询失败
		}
	}
	else                                                                         // 如果缓存中已有K线数据（后续查询）
	{
		//后面则增量更新
		WTSKlineSlice* rawData = _reader->readKlineSliceByRange(stdCode, period, barCache._last_bartime, 0);  // 从上次记录的最后一条K线时间开始读取新的基础周期数据（增量更新）
		if (rawData != NULL)                                                     // 如果读取成功
		{
			WTSLogger::info("{} {} bars of {} updated, adding to cache...", rawData->size(), tag, stdCode);  // 记录信息日志：更新缓存数据
			for (int32_t idx = 0; idx < rawData->size(); idx++)                 // 遍历所有新读取的基础周期K线数据
			{
				uint64_t barTime = 0;                                            // K线时间戳
				if (period == KP_DAY)                                            // 如果是日线周期
					barTime = rawData->at(0)->date;                              // 使用日期作为时间戳
				else                                                             // 如果是分钟线周期
					barTime = 199000000000 + rawData->at(0)->time;               // 将相对时间转换为绝对时间戳

				//只有时间上次记录的最后一条时间，才可以用于更新K线
				if (barTime <= barCache._last_bartime)                           // 如果K线时间小于等于上次记录的最后时间（可能重复或无效）
					continue;                                                    // 跳过这条数据，不用于更新

				g_dataFact.updateKlineData(barCache._bars, rawData->at(idx), sInfo, _align_by_section);  // 使用数据工厂更新K线数据（将新的基础周期K线合并到目标周期K线中）
			}

			//如果不是日线，要考虑最后一条K线是否闭合的情况
			//这里采用保守的方案，如果本地时间大于最后一条K线的时间，则认为真正闭合了
			if (period != KP_DAY)                                                // 如果不是日线周期（分钟线需要考虑是否闭合）
			{
				uint64_t last_bartime = 0;                                       // 最后一条K线的绝对时间戳
				last_bartime = 199000000000 + barCache._bars->time(-1);          // 将相对时间转换为绝对时间戳

				uint64_t now = TimeUtils::getYYYYMMDDhhmmss() / 100;             // 获取当前时间（格式：yyyymmddHHMMSS）
				if (now <= last_bartime && barCache._bars->size() > 0)           // 如果当前时间小于等于最后一条K线时间（说明最后一条K线可能未闭合）
				{
					auto& bars = barCache._bars->getDataRef();                   // 获取K线数据的引用（用于修改）
					bars.erase(bars.begin() + bars.size() - 1, bars.end());      // 删除最后一条K线（因为可能未闭合）
				}
			}

			if (period == KP_DAY)                                                // 如果是日线周期
				barCache._last_bartime = barCache._bars->date(-1);              // 更新最后一条K线的日期
			else                                                                 // 如果是分钟线周期
			{
				uint64_t lasttime = barCache._bars->time(-1);                   // 获取最后一条K线的时间（相对时间）
				barCache._last_bartime = 199000000000 + lasttime;                // 更新最后一条K线的绝对时间戳
			}


			rawData->release();                                                  // 释放原始数据切片对象
		}
	}

	//最后到缓存中定位指定数量的数据
	bool isDay = period == KP_DAY;                                               // 判断是否为日线周期（用于时间比较）
	uint32_t rDate, rTime;                                                       // 结束日期、结束时间
	rDate = (uint32_t)(etime / 10000);                                           // 提取结束日期（yyyymmddHHMMSS的前8位）
	rTime = (uint32_t)(etime % 10000);                                           // 提取结束时间（yyyymmddHHMMSS的后4位，HHMM格式）

	WTSBarStruct eBar;                                                           // 结束K线结构（用于二分查找）
	eBar.date = rDate;                                                           // 设置结束K线的日期
	eBar.time = (rDate - 19900000) * 10000 + rTime;                              // 设置结束K线的时间（将日期转换为相对时间）

	uint32_t eIdx, sIdx;                                                         // 结束索引、开始索引
	auto& bars = barCache._bars->getDataRef();                                  // 获取K线数据的引用
	auto eit = std::lower_bound(bars.begin(), bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找在K线数据中定位结束K线的位置
		if (isDay)                                                               // 如果是日线周期
			return a.date < b.date;                                              // 按日期比较
		else                                                                      // 如果是分钟线周期
			return a.time < b.time;                                              // 按时间比较
	});


	if (eit == bars.end())                                                       // 如果查找结果指向数据末尾（说明结束时间超出数据范围）
		eIdx = bars.size() - 1;                                                  // 结束索引设为最后一条数据的索引
	else                                                                         // 如果找到了
	{
		if ((isDay && eit->date > eBar.date) || (!isDay && eit->time > eBar.time))  // 如果找到的K线时间大于结束时间（需要前移一条）
		{
			eit--;                                                                // 向前移动一个位置
		}

		eIdx = eit - bars.begin();                                               // 计算结束索引（迭代器距离）
	}

	sIdx = (eIdx + 1 >= count) ? (eIdx + 1 - count) : 0;                         // 计算开始索引：如果结束索引+1大于等于数量，则开始索引=结束索引+1-数量；否则开始索引=0（从第一条开始）
	uint32_t rtCnt = eIdx - sIdx + 1;                                            // 计算返回的数据条数（结束索引-开始索引+1）
	WTSBarStruct* rtHead = barCache._bars->at(sIdx);                             // 获取开始位置K线数据的指针
	WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, times, rtHead, rtCnt);  // 创建K线数据切片对象（包装指定数量的数据）
	return slice;                                                                 // 返回K线数据切片
}

/**
 * @brief 按数量查询Tick数据切片
 * @param stdCode 标准化合约代码
 * @param count 查询条数
 * @param etime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
 * @return Tick数据切片指针（需要调用release()释放）
 * 
 * 查询指定数量的Tick数据（从结束时间向前查找）。
 * 时间会被转换为微秒时间戳（乘以100000）后传递给数据读取器。
 */
WTSTickSlice* WtDataManager::get_tick_slice_by_count(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	etime = etime * 100000;                                                      // 将结束时间转换为微秒时间戳（yyyymmddHHMMSS格式转为微秒）
	return _reader->readTickSliceByCount(stdCode, count, etime);                // 调用数据读取器查询指定数量的Tick数据切片
}

/**
 * @brief 获取复权因子
 * @param stdCode 标准化合约代码
 * @param commInfo 品种信息（如果为NULL则自动获取）
 * @return 复权因子（如果不存在则返回1.0）
 * 
 * 获取指定合约的复权因子，用于股票价格复权计算。
 * 如果是股票，从数据读取器获取复权因子；
 * 如果是期货，从主力合约管理器获取规则因子。
 */
double WtDataManager::get_exright_factor(const char* stdCode, WTSCommodityInfo* commInfo /* = NULL */)
{
	if (commInfo == NULL)                                                        // 如果品种信息为空
		return 1.0;                                                              // 返回默认复权因子1.0

	if (commInfo->isStock())                                                     // 如果是股票品种
		return _reader->getAdjFactorByDate(stdCode, 0);                          // 从数据读取器获取复权因子（0表示最新日期）
	else                                                                          // 如果是期货品种
	{
		const char* ruleTag = _hot_mgr->getRuleTag(stdCode);                     // 从主力合约管理器获取规则标签
		if (strlen(ruleTag) > 0)                                                  // 如果规则标签不为空
			return _hot_mgr->getRuleFactor(ruleTag, commInfo->getFullPid(), 0);  // 从主力合约管理器获取规则因子（0表示最新日期）
	}

	return 1.0;                                                                  // 如果无法获取复权因子，返回默认值1.0
}

/**
 * @brief 订阅实时K线
 * @param stdCode 标准化合约代码
 * @param period K线周期（KP_Tick、KP_Minute1、KP_Day等）
 * @param times 周期倍数（如period=KP_Minute1，times=5表示5分钟K线）
 * 
 * 订阅指定合约的实时K线数据。
 * 如果times为1（基础周期），从数据读取器读取最近10条K线数据并初始化实时K线映射表；
 * 如果times大于1（非基础周期），从数据读取器读取最近10*times条基础周期K线数据，重采样生成目标周期K线，并初始化实时K线映射表。
 * 订阅后，当接收到新的Tick数据时，会自动更新对应的实时K线。
 */
void WtDataManager::subscribe_bar(const char* stdCode, WTSKlinePeriod period, uint32_t times)
{
	std::string key = fmtutil::format("{}-{}-{}", stdCode, (uint32_t)period, times);  // 构建实时K线键（格式：合约代码-周期-倍数）

	uint32_t curDate = TimeUtils::getCurDate();                                   // 获取当前交易日期
	uint64_t etime = (uint64_t)curDate * 10000 + 2359;                             // 构建结束时间（当天23:59，格式：yyyymmddHHMM）

	if (times == 1)                                                                // 如果是基础周期（times=1，不需要重采样）
	{
		WTSKlineSlice* slice = _reader->readKlineSliceByCount(stdCode, period, 10, etime);  // 从数据读取器读取最近10条基础周期K线数据
		if (slice == NULL)                                                         // 如果读取失败
			return;                                                                // 直接返回，不订阅

		WTSKlineData* kline = WTSKlineData::create(stdCode, slice->size());        // 创建K线数据对象（用于存储实时K线）
		kline->setPeriod(period);                                                  // 设置K线周期
		uint32_t offset = 0;                                                       // 数据偏移量（用于复制数据）
		for(uint32_t blkIdx = 0; blkIdx < slice->get_block_counts(); blkIdx++)   // 遍历K线切片的所有数据块
		{
			memcpy(kline->getDataRef().data() + offset, slice->get_block_addr(blkIdx), sizeof(WTSBarStruct)*slice->get_block_size(blkIdx));  // 将数据块复制到K线数据对象中
			offset += slice->get_block_size(blkIdx);                              // 更新偏移量
		}
		
		{
			StdUniqueLock lock(_mtx_rtbars);                                       // 获取实时K线映射表的互斥锁（线程安全）
			if (_rt_bars == NULL)                                                   // 如果实时K线映射表为空
				_rt_bars = RtBarMap::create();                                     // 创建实时K线映射表对象

			_rt_bars->add(key, kline, false);                                      // 将K线数据添加到映射表中（false表示不覆盖已存在的项）
		}

		slice->release();                                                          // 释放K线切片对象
	}
	else                                                                           // 如果是非基础周期（times>1，需要重采样）
	{
		//只有非基础周期的会进到下面的步骤
		WTSSessionInfo* sInfo = get_session_info(stdCode, true);                  // 获取合约的交易时段信息（用于K线生成时考虑交易时段）
		WTSKlineSlice* rawData = _reader->readKlineSliceByCount(stdCode, period, 10*times, 0);  // 从数据读取器读取最近10*times条基础周期K线数据（乘以times是为了确保有足够的数据进行重采样）
		if (rawData != NULL)                                                       // 如果读取成功
		{
			WTSKlineData* kData = g_dataFact.extractKlineData(rawData, period, times, sInfo, true);  // 使用数据工厂从基础周期K线数据生成目标周期的K线数据（true表示强制对齐）
			{
				StdUniqueLock lock(_mtx_rtbars);                                   // 获取实时K线映射表的互斥锁（线程安全）
				if (_rt_bars == NULL)                                               // 如果实时K线映射表为空
					_rt_bars = RtBarMap::create();                                 // 创建实时K线映射表对象
				_rt_bars->add(key, kData, false);                                  // 将K线数据添加到映射表中（false表示不覆盖已存在的项）
			}
			rawData->release();                                                    // 释放原始数据切片对象
		}
	}

	WTSLogger::info("Realtime bar {} has subscribed", key);                       // 记录信息日志：实时K线订阅成功
}

/**
 * @brief 清除所有订阅的实时K线
 * 
 * 清空实时K线映射表，取消所有实时K线订阅。
 * 使用互斥锁保护，确保线程安全。
 */
void WtDataManager::clear_subbed_bars()
{
	StdUniqueLock lock(_mtx_rtbars);                                               // 获取实时K线映射表的互斥锁（线程安全）
	if (_rt_bars)                                                                  // 如果实时K线映射表存在
		_rt_bars->clear();                                                         // 清空映射表（清除所有订阅）
}

/**
 * @brief 更新实时K线数据
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据
 * 
 * 当接收到新的Tick数据时，更新所有订阅的实时K线。
 * 遍历实时K线映射表，找到匹配的合约代码，使用新的Tick数据更新对应的K线。
 * 如果K线更新后产生新的K线（K线闭合），会触发回调通知外部。
 * 使用互斥锁保护，确保线程安全。
 */
void WtDataManager::update_bars(const char* stdCode, WTSTickData* newTick)
{
	if (_rt_bars == NULL)                                                          // 如果实时K线映射表为空（没有订阅）
		return;                                                                    // 直接返回，不处理

	StdUniqueLock lock(_mtx_rtbars);                                               // 获取实时K线映射表的互斥锁（线程安全）
	auto it = _rt_bars->begin();                                                   // 获取实时K线映射表的迭代器起始位置
	for(; it != _rt_bars->end(); it++)                                            // 遍历所有订阅的实时K线
	{
		WTSKlineData* kData = (WTSKlineData*)it->second;                          // 获取K线数据对象（从映射表的值中获取）
		if (strcmp(kData->code(), stdCode) != 0)                                  // 如果K线数据的合约代码与当前Tick数据的合约代码不匹配
			continue;                                                              // 跳过这条K线，继续处理下一条

		WTSSessionInfo* sInfo = NULL;                                              // 交易时段信息
		if (newTick->getContractInfo())                                            // 如果Tick数据包含合约信息
			sInfo = newTick->getContractInfo()->getCommInfo()->getSessionInfo();  // 从Tick数据中获取交易时段信息
		else                                                                       // 如果Tick数据不包含合约信息
			sInfo = get_session_info(kData->code(), true);                        // 从合约代码获取交易时段信息
		g_dataFact.updateKlineData(kData, newTick, sInfo, _align_by_section);     // 使用数据工厂更新K线数据（将新的Tick数据合并到K线中）
		WTSBarStruct* lastBar = kData->at(-1);                                    // 获取更新后的最后一条K线数据

		std::string speriod;                                                       // 周期字符串（用于回调）
		uint32_t times = kData->times();                                          // 获取周期倍数
		switch (kData->period())                                                   // 根据K线周期生成周期字符串
		{
		case KP_Minute1:                                                           // 如果是1分钟周期
			speriod = fmtutil::format("m{}", times);                              // 格式化为"m倍数"，如"m5"表示5分钟
			break;
		case KP_Minute5:                                                           // 如果是5分钟周期
			speriod = fmtutil::format("m{}", times*5);                             // 格式化为"m倍数*5"，如"m25"表示25分钟
			break;
		default:                                                                    // 其他周期（如日线）
			speriod = fmtutil::format("d{}", times);                              // 格式化为"d倍数"，如"d1"表示1日
			break;
		}

		_runner->trigger_bar(stdCode, speriod.c_str(), lastBar);                   // 触发K线回调通知外部（通知外部新的K线数据已更新）
	}
}

/**
 * @brief 清除数据读取器的缓存
 * 
 * 清除数据读取器的内部缓存，释放内存。
 * 如果数据读取器未初始化，则记录警告日志并返回。
 */
void WtDataManager::clear_cache()
{
	if (_reader == NULL)                                                          // 如果数据读取器未初始化
	{
		WTSLogger::warn("DataReader not initialized, clearing canceled");         // 记录警告日志：数据读取器未初始化，取消清除操作
		return;                                                                    // 直接返回
	}

	_reader->clearCache();                                                         // 调用数据读取器的clearCache()方法清除缓存
	WTSLogger::warn("All cache cleared");                                         // 记录警告日志：所有缓存已清除
}