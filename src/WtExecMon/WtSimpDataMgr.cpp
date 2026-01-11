/*!
 * \file WtSimpDataMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader简单数据管理器类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtSimpDataMgr类的所有方法，包括初始化、数据读取、数据查询等功能。
 * 该类是WonderTrader执行器模块中的数据管理器，负责管理行情数据和K线数据的读取、缓存和查询。
 * 
 * 主要功能：
 * 1. 数据存储初始化：从配置加载数据存储模块（IDataReader），初始化数据读取器
 * 2. 实时数据管理：接收并处理实时行情数据，更新缓存和时间信息
 * 3. Tick数据查询：通过数据读取器查询历史Tick数据切片
 * 4. K线数据查询：通过数据读取器查询历史K线数据切片，支持K线合成
 * 5. 最新数据获取：获取指定合约的最新Tick数据
 * 6. 时间管理：管理当前日期、时间、交易日等时间信息
 * 7. 接口实现：实现IDataReaderSink和IDataManager接口的所有方法
 * 
 * 设计特点：
 * - 动态加载：动态加载数据存储模块，支持插件化架构
 * - 数据缓存：使用哈希映射缓存K线和实时Tick数据，提高查询效率
 * - K线合成：对于非基础周期的K线，从基础周期K线合成
 * - 时间同步：实时更新当前时间信息，确保时间准确性
 * - 数据过滤：过滤时间回退的数据，确保数据的时间顺序
 */

#include "WtSimpDataMgr.h"  // 包含当前类的头文件
#include "WtExecRunner.h"  // 包含执行器运行器头文件，使用WtExecRunner类
#include "../WtCore/WtHelper.h"  // 包含WonderTrader辅助工具类，提供路径、时间等工具函数

#include "../Share/StrUtil.hpp"  // 包含字符串工具函数，提供格式化字符串功能
#include "../Includes/WTSDataDef.hpp"  // 包含数据定义，提供WTSBarStruct等数据结构
#include "../Includes/WTSVariant.hpp"  // 包含配置变体类，提供WTSVariant类型
#include "../Share/DLLHelper.hpp"  // 包含动态库辅助工具，提供动态库加载功能
#include "../Includes/WTSSessionInfo.hpp"  // 包含交易会话信息定义，提供WTSSessionInfo类型

#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能
#include "../WTSTools/WTSDataFactory.h"  // 包含数据工厂类，提供K线数据合成功能

USING_NS_WTP;  // 使用WonderTrader命名空间


WTSDataFactory g_dataFact;  // 全局数据工厂实例，用于K线数据合成

/**
 * @brief 构造函数实现
 * 
 * 创建数据管理器实例，初始化成员变量。
 * 使用初始化列表初始化指针成员为NULL。
 */
WtSimpDataMgr::WtSimpDataMgr()
	: _reader(NULL)  // 初始化数据读取器指针为NULL
	, _runner(NULL)  // 初始化执行器运行器指针为NULL
	, _bars_cache(NULL)  // 初始化K线缓存指针为NULL
	, _rt_tick_map(NULL)  // 初始化实时Tick缓存指针为NULL
{
}


/**
 * @brief 析构函数实现
 * 
 * 清理数据管理器占用的资源，释放缓存数据。
 * 释放实时Tick缓存映射表。
 */
WtSimpDataMgr::~WtSimpDataMgr()
{
	if (_rt_tick_map)  // 如果实时Tick缓存存在
		_rt_tick_map->release();  // 释放实时Tick缓存映射表
}

/**
 * @brief 初始化数据存储模块
 * 
 * 从配置加载数据存储模块（IDataReader），初始化数据读取器。
 * 
 * @param cfg 数据存储配置对象
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置对象是否有效
 * 2. 从配置中获取数据存储模块名称（默认为WtDataStorage）
 * 3. 拼接模块完整路径（安装目录 + 模块名称）
 * 4. 加载数据存储模块动态库
 * 5. 获取createDataReader函数指针
 * 6. 创建数据读取器实例并初始化
 * 7. 获取交易会话信息
 */
bool WtSimpDataMgr::initStore(WTSVariant* cfg)
{
	if (cfg == NULL)  // 如果配置对象为空
		return false;  // 返回false，表示初始化失败

	std::string module = cfg->getCString("module");  // 从配置中获取数据存储模块名称
	if (module.empty())  // 如果模块名称为空
		module = WtHelper::getInstDir() + DLLHelper::wrap_module("WtDataStorage");  // 使用默认模块名称"WtDataStorage"
	else  // 如果模块名称不为空
		module = WtHelper::getInstDir() + DLLHelper::wrap_module(module.c_str());  // 使用指定的模块名称

	DllHandle hInst = DLLHelper::load_library(module.c_str());  // 加载数据存储模块动态库
	if (hInst == NULL)  // 如果动态库加载失败
	{
		WTSLogger::error("Data reader {} loading failed", module.c_str());  // 记录错误日志
		return false;  // 返回false，表示初始化失败
	}

	FuncCreateDataReader funcCreator = (FuncCreateDataReader)DLLHelper::get_symbol(hInst, "createDataReader");  // 获取createDataReader函数指针
	if (funcCreator == NULL)  // 如果函数指针获取失败
	{
		WTSLogger::error("Data reader {} loading failed: entrance function createDataReader not found", module.c_str());  // 记录错误日志
		DLLHelper::free_library(hInst);  // 释放动态库
		return false;  // 返回false，表示初始化失败
	}

	_reader = funcCreator();  // 调用createDataReader函数创建数据读取器实例
	if (_reader == NULL)  // 如果数据读取器创建失败
	{
		WTSLogger::error("Data reader {} creating api failed", module.c_str());  // 记录错误日志
		DLLHelper::free_library(hInst);  // 释放动态库
		return false;  // 返回false，表示初始化失败
	}

	_reader->init(cfg, this);  // 初始化数据读取器，传入配置对象和接收者指针（this）

	_s_info = _runner->get_session_info(cfg->getCString("session"), false);  // 获取交易会话信息，用于时间转换

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化数据管理器
 * 
 * 初始化数据管理器，加载数据存储模块。
 * 
 * @param cfg 数据管理器配置对象
 * @param runner 执行器运行器指针，用于获取基础数据管理器和热点合约管理器
 * @return 初始化成功返回true，失败返回false
 */
bool WtSimpDataMgr::init(WTSVariant* cfg, WtExecRunner* runner)
{
	_runner = runner;  // 保存执行器运行器指针
	return initStore(cfg->get("store"));  // 调用initStore方法初始化数据存储模块，传入store配置节点
}

/**
 * @brief K线数据更新完成回调（IDataStoreListener接口实现）
 * 
 * 当数据存储器完成所有K线数据更新时调用。
 * 
 * @param updateTime 更新时间戳
 * 
 * 当前实现：空实现，不处理更新完成通知
 */
void WtSimpDataMgr::on_all_bar_updated(uint32_t updateTime)
{

}

/**
 * @brief 获取基础数据管理器（IDataReaderSink接口实现）
 * 
 * 返回基础数据管理器的指针。
 * 
 * @return 返回基础数据管理器指针
 */
IBaseDataMgr* WtSimpDataMgr::get_basedata_mgr()
{
	return _runner->get_bd_mgr();  // 从执行器运行器获取基础数据管理器指针
}

/**
 * @brief 获取热点合约管理器（IDataReaderSink接口实现）
 * 
 * 返回热点合约管理器的指针。
 * 
 * @return 返回热点合约管理器指针
 */
IHotMgr* WtSimpDataMgr::get_hot_mgr()
{
	return _runner->get_hot_mgr();  // 从执行器运行器获取热点合约管理器指针
}

/**
 * @brief 获取当前日期（IDataReaderSink接口实现）
 * 
 * 返回当前日期。
 * 
 * @return 返回当前日期（格式：YYYYMMDD）
 */
uint32_t WtSimpDataMgr::get_date()
{
	return _cur_date;  // 返回当前日期
}

/**
 * @brief 获取当前分钟时间（IDataReaderSink接口实现）
 * 
 * 返回当前1分钟线时间。
 * 
 * @return 返回当前1分钟线时间（格式：HHMM）
 */
uint32_t WtSimpDataMgr::get_min_time()
{
	return _cur_min_time;  // 返回当前1分钟线时间
}

/**
 * @brief 获取当前秒数（IDataReaderSink接口实现）
 * 
 * 返回当前秒数（包括毫秒）。
 * 
 * @return 返回当前秒数（格式：SSmmm）
 */
uint32_t WtSimpDataMgr::get_secs()
{
	return _cur_secs;  // 返回当前秒数
}

/**
 * @brief 数据读取器日志回调（IDataReaderSink接口实现）
 * 
 * 当数据读取器需要记录日志时调用。
 * 
 * @param ll 日志级别
 * @param message 日志消息
 */
void WtSimpDataMgr::reader_log(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_raw(ll, message);  // 将日志消息记录到日志系统
}

/**
 * @brief K线数据更新回调（IDataStoreListener接口实现）
 * 
 * 当数据存储器更新K线数据时调用。
 * 
 * @param code 合约代码
 * @param period K线周期
 * @param newBar 新的K线数据指针
 * 
 * 当前实现：空实现，不处理K线更新通知
 */
void WtSimpDataMgr::on_bar(const char* code, WTSKlinePeriod period, WTSBarStruct* newBar)
{

}

/**
 * @brief 处理实时行情推送
 * 
 * 接收并处理实时行情数据，更新缓存和时间信息。
 * 
 * @param stdCode 标准合约代码
 * @param curTick 新的Tick数据指针
 * 
 * 处理流程：
 * 1. 检查Tick数据指针是否有效
 * 2. 如果实时Tick缓存不存在，创建缓存映射表
 * 3. 将Tick数据添加到缓存（如果已存在则更新）
 * 4. 检查时间是否回退（过滤过期数据）
 * 5. 更新当前日期和完整时间
 * 6. 计算当前原始时间（分钟）和秒数
 * 7. 将时间转换为日内分钟数
 * 8. 处理交易时段边界情况（如果是时段结束时间，分钟数减1）
 * 9. 计算当前1分钟线时间
 * 10. 更新交易日信息
 */
void WtSimpDataMgr::handle_push_quote(const char* stdCode, WTSTickData* curTick)
{
	if (curTick == NULL)  // 如果Tick数据指针为空
		return;  // 直接返回，不做处理

	if (_rt_tick_map == NULL)  // 如果实时Tick缓存不存在
		_rt_tick_map = DataCacheMap::create();  // 创建实时Tick缓存映射表

	_rt_tick_map->add(stdCode, curTick, true);  // 将Tick数据添加到缓存，true表示如果已存在则更新

	uint32_t uDate = curTick->actiondate();  // 获取Tick数据的动作日期（格式：YYYYMMDD）
	uint32_t uTime = curTick->actiontime();  // 获取Tick数据的动作时间（格式：HHMMSSmmm，毫秒级）

	// 检查时间是否回退（过滤过期数据）
	if (_cur_date != 0 && (uDate < _cur_date || (uDate == _cur_date && uTime < _cur_act_time)))  // 如果日期小于当前日期，或日期相同但时间小于当前时间
	{
		return;  // 直接返回，不更新时间和缓存（过滤过期数据）
	}

	_cur_date = uDate;  // 更新当前日期
	_cur_act_time = uTime;  // 更新当前完整时间

	uint32_t _cur_raw_time = _cur_act_time / 100000;  // 计算当前原始时间（分钟格式：HHMM），去掉秒和毫秒部分
	uint32_t _cur_secs = _cur_act_time % 100000;  // 计算当前秒数（格式：SSmmm），提取秒和毫秒部分
	uint32_t minutes = _s_info->timeToMinutes(_cur_raw_time);  // 将时间转换为日内分钟数（从交易日开始计算的分钟数）
	bool isSecEnd = _s_info->isLastOfSection(_cur_raw_time);  // 检查是否是交易时段结束时间
	if (isSecEnd)  // 如果是交易时段结束时间
	{
		minutes--;  // 分钟数减1（因为时段结束时间属于下一个时段）
	}
	minutes++;  // 分钟数加1（因为1分钟K线的时间是下一分钟的开始时间）
	_cur_min_time = _s_info->minuteToTime(minutes);  // 将分钟数转换回时间格式（HHMM），作为当前1分钟线时间
	_cur_tdate = curTick->tradingdate();  // 更新当前交易日（格式：YYYYMMDD）
}

/**
 * @brief 获取最新Tick数据（IDataManager接口实现）
 * 
 * 获取指定合约的最新Tick数据。
 * 
 * @param code 合约代码
 * @return 返回最新Tick数据指针，未找到返回NULL
 * 
 * 注意事项：
 * - 返回的Tick数据需要调用者负责释放（调用retain/release）
 * - 如果实时Tick缓存不存在，返回NULL
 */
WTSTickData* WtSimpDataMgr::grab_last_tick(const char* code)
{
	if (_rt_tick_map == NULL)  // 如果实时Tick缓存不存在
		return NULL;  // 返回NULL

	WTSTickData* curTick = (WTSTickData*)_rt_tick_map->get(code);  // 从缓存中获取指定合约的最新Tick数据
	if (curTick == NULL)  // 如果Tick数据不存在
		return NULL;  // 返回NULL

	curTick->retain();  // 增加引用计数，确保数据不会被释放
	return curTick;  // 返回Tick数据指针
}


/**
 * @brief 获取Tick数据切片（IDataManager接口实现）
 * 
 * 获取指定合约的Tick数据切片。
 * 
 * @param code 合约代码
 * @param count 数据条数
 * @param etime 截止时间戳，默认为0（当前时间）
 * @return 返回Tick数据切片指针，未找到返回NULL
 * 
 * 查询流程：
 * 1. 检查数据读取器是否存在
 * 2. 调用数据读取器的readTickSlice方法读取Tick数据切片
 */
WTSTickSlice* WtSimpDataMgr::get_tick_slice(const char* code, uint32_t count, uint64_t etime /*= 0*/)
{
	if (_reader == NULL)  // 如果数据读取器不存在
		return NULL;  // 返回NULL

	return _reader->readTickSlice(code, count, etime);  // 调用数据读取器读取Tick数据切片
}


/**
 * @brief 获取K线数据切片（IDataManager接口实现）
 * 
 * 获取指定合约的K线数据切片，支持不同周期和倍数。
 * 
 * @param stdCode 合约代码
 * @param period K线周期（如PERIOD_M1、PERIOD_M5等）
 * @param times 周期倍数，1表示基础周期，大于1表示合成周期
 * @param count 数据条数
 * @param etime 截止时间戳，默认为0（当前时间）
 * @return 返回K线数据切片指针，未找到返回NULL
 * 
 * K线合成说明：
 * - 如果times为1，直接从数据读取器读取基础周期K线
 * - 如果times大于1，从基础周期K线合成目标周期K线
 * - 合成后的K线会被缓存，提高后续查询效率
 * 
 * 查询流程：
 * 1. 检查数据读取器是否存在
 * 2. 如果times为1，直接从数据读取器读取基础周期K线
 * 3. 如果times大于1：
 *    a. 生成缓存键（合约代码-周期-倍数）
 *    b. 检查缓存中是否存在足够的K线数据
 *    c. 如果缓存不足，从数据读取器读取更多基础周期K线
 *    d. 使用数据工厂合成目标周期K线
 *    e. 将合成后的K线添加到缓存
 *    f. 从缓存中提取指定数量的K线数据
 *    g. 创建K线切片并返回
 */
WTSKlineSlice* WtSimpDataMgr::get_kline_slice(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime /*= 0*/)
{
	if (_reader == NULL)  // 如果数据读取器不存在
		return NULL;  // 返回NULL

	std::string key = StrUtil::printf("%s-%u", stdCode, period);  // 生成缓存键（合约代码-周期）

	if (times == 1)  // 如果周期倍数为1（基础周期）
	{
		return _reader->readKlineSlice(stdCode, period, count, etime);  // 直接从数据读取器读取基础周期K线切片
	}

	//只有非基础周期的会进到下面的步骤
	WTSSessionInfo* sInfo = _runner->get_session_info(stdCode, true);  // 获取交易会话信息，用于K线合成

	if (_bars_cache == NULL)  // 如果K线缓存不存在
		_bars_cache = DataCacheMap::create();  // 创建K线缓存映射表

	key = StrUtil::printf("%s-%u-%u", stdCode, period, times);  // 生成完整缓存键（合约代码-周期-倍数）

	WTSKlineData* kData = (WTSKlineData*)_bars_cache->get(key);  // 从缓存中获取K线数据
	//如果缓存里的K线条数大于请求的条数, 则直接返回
	if (kData == NULL || kData->size() < count)  // 如果缓存不存在或K线条数不足
	{
		uint32_t realCount = count * times + times;  // 计算需要读取的基础周期K线条数（多读一些，确保合成后有足够的数据）
		WTSKlineSlice* rawData = _reader->readKlineSlice(stdCode, period, realCount, etime);  // 从数据读取器读取基础周期K线切片
		if (rawData != NULL)  // 如果读取成功
		{
			kData = g_dataFact.extractKlineData(rawData, period, times, sInfo, true);  // 使用数据工厂合成目标周期K线数据
			// 参数说明：
			// - rawData: 基础周期K线切片
			// - period: K线周期
			// - times: 周期倍数
			// - sInfo: 交易会话信息（用于处理交易时段边界）
			// - true: 是否包含不完整的K线
			rawData->release();  // 释放基础周期K线切片
		}
		else  // 如果读取失败
		{
			return NULL;  // 返回NULL
		}

		if (kData)  // 如果K线数据合成成功
			_bars_cache->add(key, kData, false);  // 将K线数据添加到缓存，false表示如果已存在则不更新
	}

	int32_t sIdx = 0;  // 起始索引
	uint32_t rtCnt = min(kData->size(), count);  // 实际返回的K线条数（取缓存条数和请求条数的最小值）
	sIdx = kData->size() - rtCnt;  // 计算起始索引（从后往前取指定数量的K线）
	WTSBarStruct* rtHead = kData->at(sIdx);  // 获取起始K线数据指针
	WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, times, rtHead, rtCnt);  // 创建K线切片
	return slice;  // 返回K线切片指针
}
