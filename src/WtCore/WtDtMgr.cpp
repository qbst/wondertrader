/*!
 * \file WtDataManager.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件实现了WtDtMgr类的所有方法，提供数据管理功能。
 * 
 * 实现要点：
 * 1. 构造函数和析构函数：初始化成员变量，清理资源
 * 2. 初始化：加载数据存储模块，创建数据读取器
 * 3. K线数据管理：缓存K线数据，支持重采样和小节对齐
 * 4. Tick数据管理：缓存实时Tick和后复权Tick数据
 * 5. 数据读取接口：为策略提供各种数据访问接口
 * 6. 回调处理：接收数据读取器的回调，更新缓存并通知引擎
 * 
 * 关键算法：
 * - K线重采样：基于基础周期K线，生成多周期K线
 * - 后复权处理：对Tick数据进行复权处理，生成后复权Tick数据
 * - 延迟通知：收集所有K线更新，统一触发引擎事件
 */
#include "WtDtMgr.h"  // 包含数据管理器头文件
#include "WtEngine.h"  // 包含引擎头文件
#include "WtHelper.h"  // 包含辅助工具头文件

#include "../Share/StrUtil.hpp"  // 包含字符串工具头文件
#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具头文件

#include "../Includes/WTSDataDef.hpp"  // 包含WonderTrader数据定义头文件
#include "../Includes/WTSVariant.hpp"  // 包含变体类型头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志工具头文件
#include "../WTSTools/WTSDataFactory.h"  // 包含数据工厂头文件


WTSDataFactory g_dataFact;  // 全局数据工厂对象，用于创建和更新K线数据

/**
 * @brief 构造函数
 * 
 * 初始化数据管理器，所有指针成员初始化为NULL，标志位初始化为false。
 */
WtDtMgr::WtDtMgr()
	: _reader(NULL)  // 初始化数据读取器指针为NULL
	, _engine(NULL)  // 初始化引擎指针为NULL
	, _loader(NULL)  // 初始化历史数据加载器指针为NULL
	, _bars_cache(NULL)  // 初始化K线缓存指针为NULL
	, _ticks_adjusted(NULL)  // 初始化复权Tick缓存指针为NULL
	, _rt_tick_map(NULL)  // 初始化实时Tick缓存指针为NULL
	, _force_cache(false)  // 初始化强制缓存标志为false
{
}


/**
 * @brief 析构函数
 * 
 * 清理资源，释放所有缓存对象。
 */
WtDtMgr::~WtDtMgr()
{
	if (_bars_cache)  // 如果K线缓存存在
		_bars_cache->release();  // 释放K线缓存（减少引用计数）

	if (_ticks_adjusted)  // 如果复权Tick缓存存在
		_ticks_adjusted->release();  // 释放复权Tick缓存（减少引用计数）

	if (_rt_tick_map)  // 如果实时Tick缓存存在
		_rt_tick_map->release();  // 释放实时Tick缓存（减少引用计数）
}

/**
 * @brief 初始化数据存储模块
 * @param cfg 数据存储配置参数
 * @return bool 初始化成功返回true，否则返回false
 * 
 * 根据配置参数加载数据存储模块（动态库），创建数据读取器实例。
 * 如果配置中未指定模块，则使用默认模块WtDataStorage。
 * 
 * 实现逻辑：
 * 1. 检查配置参数是否有效
 * 2. 从配置中获取模块名称（如果未指定则使用默认值）
 * 3. 加载动态库
 * 4. 获取创建函数符号
 * 5. 创建数据读取器实例
 * 6. 初始化数据读取器
 */
bool WtDtMgr::initStore(WTSVariant* cfg)
{
	if (cfg == NULL)  // 如果配置参数为NULL
		return false;  // 返回false

	std::string module = cfg->getCString("module");  // 从配置中获取模块名称（键名为"module"）
	if (module.empty())  // 如果模块名称为空
		module = WtHelper::getInstDir() + DLLHelper::wrap_module("WtDataStorage");  // 使用默认模块名称（WtDataStorage）
	else  // 如果指定了模块名称
		module = WtHelper::getInstDir() + DLLHelper::wrap_module(module.c_str());  // 拼接完整模块路径（实例目录 + 模块名称）

	DllHandle hInst = DLLHelper::load_library(module.c_str());  // 加载动态库
	if(hInst == NULL)  // 如果加载失败
	{
		WTSLogger::error("Loading data reader module {} failed", module.c_str());  // 记录错误日志：加载数据读取器模块失败
		return false;  // 返回false
	}

	FuncCreateDataReader funcCreator = (FuncCreateDataReader)DLLHelper::get_symbol(hInst, "createDataReader");  // 获取创建函数符号（函数名为"createDataReader"）
	if(funcCreator == NULL)  // 如果获取符号失败
	{
		WTSLogger::error("Loading data reader module {} failed, entrance function createDataReader not found", module.c_str());  // 记录错误日志：入口函数未找到
		DLLHelper::free_library(hInst);  // 释放动态库
		return false;  // 返回false
	}

	_reader = funcCreator();  // 调用创建函数，创建数据读取器实例
	if(_reader == NULL)  // 如果创建失败
	{
		WTSLogger::error("Creating instance of data reader module {} failed", module.c_str());  // 记录错误日志：创建数据读取器实例失败
		DLLHelper::free_library(hInst);  // 释放动态库
		return false;  // 返回false
	}

	_reader->init(cfg, this, _loader);  // 初始化数据读取器（传入配置、回调接口和数据加载器）

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化数据管理器
 * @param cfg 配置参数，包含数据存储配置等
 * @param engine 引擎指针，用于获取会话信息和触发事件
 * @param bForceCache 是否强制缓存K线，默认为false
 * @return bool 初始化成功返回true，否则返回false
 * 
 * 初始化数据管理器，设置引擎指针和缓存策略。
 * 如果bForceCache为true，则所有K线都会被缓存（包括1倍周期）。
 * 
 * 实现逻辑：
 * 1. 保存引擎指针
 * 2. 从配置中读取小节对齐标志
 * 3. 设置强制缓存标志
 * 4. 初始化数据存储模块
 */
bool WtDtMgr::init(WTSVariant* cfg, WtEngine* engine, bool bForceCache /* = false */)
{
	_engine = engine;  // 保存引擎指针

	_align_by_section = cfg->getBoolean("align_by_section");  // 从配置中获取小节对齐标志（键名为"align_by_section"）

	_force_cache = bForceCache;  // 设置强制缓存标志

	WTSLogger::info("Resampled bars will be aligned by section: {}", _align_by_section?"yes":" no");  // 记录日志：重采样K线是否按小节对齐

	WTSLogger::info("Force to cache bars: {}", _force_cache ? "yes" : " no");  // 记录日志：是否强制缓存K线

	return initStore(cfg->get("store"));  // 初始化数据存储模块（从配置中获取"store"子配置）
}

/**
 * @brief 所有K线更新完成回调
 * @param updateTime 更新时间戳
 * 
 * 当数据读取器完成所有K线更新时被调用。
 * 统一处理通知队列中的所有K线更新事件，触发引擎的on_bar事件。
 * 
 * 实现逻辑：
 * 1. 检查通知队列是否为空，如果为空则直接返回
 * 2. 遍历通知队列，逐个触发引擎的on_bar事件
 * 3. 清空通知队列
 */
void WtDtMgr::on_all_bar_updated(uint32_t updateTime)
{
	if (_bar_notifies.empty())  // 如果通知队列为空
		return;  // 直接返回，不进行通知

	WTSLogger::debug("All bars updated, on_bar will be triggered");  // 记录调试日志：所有K线已更新，将触发on_bar事件

	for (const NotifyItem& item : _bar_notifies)  // 遍历通知队列
	{
		_engine->on_bar(item._code, item._period, item._times, item._newBar);  // 触发引擎的on_bar事件（传递合约代码、周期、倍数和K线数据）
	}

	_bar_notifies.clear();  // 清空通知队列
}

/**
 * @brief 获取基础数据管理器
 * @return IBaseDataMgr* 返回基础数据管理器指针
 * 
 * 从引擎获取基础数据管理器，用于获取合约信息等。
 */
IBaseDataMgr* WtDtMgr::get_basedata_mgr()
{ 
	return _engine->get_basedata_mgr();  // 从引擎获取基础数据管理器指针
}

/**
 * @brief 获取热点合约管理器
 * @return IHotMgr* 返回热点合约管理器指针
 * 
 * 从引擎获取热点合约管理器，用于获取主力合约信息等。
 */
IHotMgr* WtDtMgr::get_hot_mgr() 
{ 
	return _engine->get_hot_mgr();  // 从引擎获取热点合约管理器指针
}

/**
 * @brief 获取当前日期
 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
 * 
 * 从引擎获取当前日期。
 */
uint32_t WtDtMgr::get_date() 
{ 
	return _engine->get_date();  // 从引擎获取当前日期
}

/**
 * @brief 获取当前分钟时间
 * @return uint32_t 返回当前分钟时间（格式：HHMM）
 * 
 * 从引擎获取当前分钟时间。
 */
uint32_t WtDtMgr::get_min_time()
{ 
	return _engine->get_min_time();  // 从引擎获取当前分钟时间
}

/**
 * @brief 获取当前秒数
 * @return uint32_t 返回当前秒数（包含毫秒，格式：SSmmm）
 * 
 * 从引擎获取当前秒数。
 */
uint32_t WtDtMgr::get_secs() 
{ 
	return _engine->get_secs();  // 从引擎获取当前秒数
}

/**
 * @brief 数据读取器日志回调
 * @param ll 日志级别
 * @param message 日志消息字符串
 * 
 * 当数据读取器需要记录日志时被调用，直接转发给日志系统。
 */
void WtDtMgr::reader_log(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_raw(ll, message);  // 直接记录原始日志（使用指定的日志级别）
}

/**
 * @brief K线更新回调
 * @param code 合约代码字符串
 * @param period K线周期
 * @param newBar 新的K线数据指针
 * 
 * 当数据读取器有新的K线数据时被调用。
 * 更新K线缓存，并将更新事件加入通知队列。
 * 所有K线更新完成后，统一触发引擎的on_bar事件。
 * 
 * 实现逻辑：
 * 1. 构建K线键模式（合约代码-周期）
 * 2. 将周期转换为字符表示和倍数
 * 3. 如果是基础周期且已订阅，加入通知队列
 * 4. 遍历K线缓存，更新重采样K线
 * 5. 如果重采样K线闭合，也加入通知队列
 * 6. 如果是强制缓存的一倍周期，直接添加到缓存
 */
void WtDtMgr::on_bar(const char* code, WTSKlinePeriod period, WTSBarStruct* newBar)
{
	std::string key_pattern = fmt::format("{}-{}", code, period);  // 构建K线键模式（合约代码-周期），用于匹配缓存项

	char speriod;  // 周期字符（'m'表示分钟，'d'表示日）
	uint32_t times = 1;  // K线倍数（默认为1）
	switch (period)  // 根据K线周期设置周期字符和倍数
	{
	case KP_Minute1:  // 如果是1分钟周期
		speriod = 'm';  // 周期字符设为'm'
		times = 1;  // 倍数设为1
		break;
	case KP_Minute5:  // 如果是5分钟周期
		speriod = 'm';  // 周期字符设为'm'
		times = 5;  // 倍数设为5
		break;
	default:  // 其他周期（日线等）
		speriod = 'd';  // 周期字符设为'd'
		times = 1;  // 倍数设为1
		break;
	}

	if(_subed_basic_bars.find(key_pattern) != _subed_basic_bars.end())  // 如果是基础周期且已订阅
	{
		//如果是基础周期, 直接触发on_bar事件
		//_engine->on_bar(code, speriod.c_str(), times, newBar);  // 已注释的直接触发方式
		//更新完K线以后, 统一通知交易引擎
		_bar_notifies.emplace_back(NotifyItem(code, speriod, times, newBar));  // 将K线更新项加入通知队列（延迟通知）
	}

	//然后再处理非基础周期
	if (_bars_cache == NULL || _bars_cache->size() == 0)  // 如果K线缓存不存在或为空
		return;  // 直接返回，不处理重采样K线
	
	WTSSessionInfo* sInfo = _engine->get_session_info(code, true);  // 从引擎获取交易会话信息（isCode=true表示code是合约代码）

	for (auto it = _bars_cache->begin(); it != _bars_cache->end(); it++)  // 遍历K线缓存
	{
		const char* key = it->first.c_str();  // 获取缓存键（合约代码-周期-倍数格式）
		if(memcmp(key, key_pattern.c_str(), key_pattern.size()) != 0)  // 如果缓存键的前缀不匹配（不是同一个合约和周期）
			continue;  // 跳过当前项，继续处理下一个

		WTSKlineData* kData = (WTSKlineData*)it->second;  // 获取K线数据对象（从void*转换为WTSKlineData*）
		if(kData->times() != 1)  // 如果K线倍数不为1（需要重采样）
		{
			g_dataFact.updateKlineData(kData, newBar, sInfo, _align_by_section);  // 更新K线数据（使用数据工厂进行重采样，考虑小节对齐）
			if (kData->isClosed())  // 如果K线已闭合
			{
				//如果基础周期K线的时间和自定义周期K线的时间一致, 说明K线关闭了
				//这里也要触发on_bar事件
				WTSBarStruct* lastBar = kData->at(-1);  // 获取最后一条K线数据（索引-1表示最后一条）
				//_engine->on_bar(code, speriod.c_str(), times, lastBar);  // 已注释的直接触发方式
				//更新完K线以后, 统一通知交易引擎
				_bar_notifies.emplace_back(NotifyItem(code, speriod, times*kData->times(), lastBar));  // 将K线更新项加入通知队列（倍数 = 基础倍数 * 重采样倍数）
			}
		}
		else  // 如果K线倍数为1（强制缓存的一倍周期）
		{
			//如果是强制缓存的一倍周期，直接压到缓存队列里
			kData->getDataRef().emplace_back(*newBar);  // 直接将新的K线数据添加到缓存队列末尾
			_bar_notifies.emplace_back(NotifyItem(code, speriod, times, newBar));  // 将K线更新项加入通知队列
		}
	}
}

/**
 * @brief 处理推送的行情数据
 * @param stdCode 标准合约代码字符串
 * @param newTick 新的Tick数据指针
 * 
 * 接收外部推送的实时行情数据，更新实时Tick缓存。
 * 如果是后复权合约，还会更新后复权Tick缓存。
 * 
 * 实现逻辑：
 * 1. 检查Tick数据是否有效
 * 2. 如果实时Tick缓存不存在，创建缓存
 * 3. 将Tick数据添加到实时缓存
 * 4. 如果是后复权合约，更新后复权Tick缓存
 */
void WtDtMgr::handle_push_quote(const char* stdCode, WTSTickData* newTick)
{
	if (newTick == NULL)  // 如果Tick数据无效
		return;  // 直接返回，不进行处理

	if (_rt_tick_map == NULL)  // 如果实时Tick缓存不存在
		_rt_tick_map = DataCacheMap::create();  // 创建实时Tick缓存映射表

	_rt_tick_map->add(stdCode, newTick, true);  // 将Tick数据添加到实时缓存（第三个参数true表示自动增加引用计数）

	if(_ticks_adjusted != NULL)  // 如果复权Tick缓存存在
	{
		WTSHisTickData* tData = (WTSHisTickData*)_ticks_adjusted->get(stdCode);  // 从复权缓存中获取Tick数据（如果不存在则返回NULL）
		if (tData == NULL)  // 如果复权缓存中不存在该合约的数据
			return;  // 直接返回，不更新复权缓存

		if (tData->isValidOnly() && newTick->volume() == 0)  // 如果复权数据只接受有效Tick（成交量不为0）且当前Tick成交量为0
			return;  // 直接返回，不更新复权缓存

		tData->appendTick(newTick->getTickStruct());  // 将Tick数据追加到复权缓存中（复制Tick结构）
	}
}

/**
 * @brief 获取最后一个Tick数据
 * @param stdCode 标准合约代码字符串
 * @return WTSTickData* 返回最后一个Tick数据指针，如果缓存无效返回NULL
 * 
 * 从实时Tick缓存中获取最新的Tick数据。
 * 返回的数据需要调用者负责释放（调用release方法）。
 */
WTSTickData* WtDtMgr::grab_last_tick(const char* code)
{
	if (_rt_tick_map == NULL)  // 如果实时Tick缓存不存在
		return NULL;  // 返回NULL

	WTSTickData* curTick = (WTSTickData*)_rt_tick_map->get(code);  // 从实时缓存中获取Tick数据（如果不存在则返回NULL）
	if (curTick == NULL)  // 如果缓存中不存在该合约的数据
		return NULL;  // 返回NULL

	curTick->retain();  // 增加Tick数据的引用计数（防止在使用时被释放）
	return curTick;  // 返回Tick数据指针
}

/**
 * @brief 获取复权因子
 * @param stdCode 标准合约代码字符串
 * @param uDate 日期（格式：YYYYMMDD）
 * @return double 返回复权因子，如果数据读取器无效返回1.0
 * 
 * 从数据读取器获取指定日期的复权因子。
 */
double WtDtMgr::get_adjusting_factor(const char* stdCode, uint32_t uDate)
{
	if (_reader)  // 如果数据读取器有效
		return _reader->getAdjFactorByDate(stdCode, uDate);  // 从数据读取器获取指定日期的复权因子

	return 1.0;  // 如果数据读取器无效，返回1.0（不复权）
}

/**
 * @brief 获取复权标志
 * @return uint32_t 返回复权标志（0=不复权，1=前复权，2=后复权）
 * 
 * 从数据读取器获取复权标志。
 * 使用静态变量缓存结果，避免重复查询。
 */
uint32_t WtDtMgr::get_adjusting_flag()
{
	static uint32_t flag = UINT_MAX;  // 静态变量，用于缓存复权标志（初始值为UINT_MAX表示未初始化）
	if(flag == UINT_MAX)  // 如果标志未初始化
	{
		if (_reader)  // 如果数据读取器有效
			flag = _reader->getAdjustingFlag();  // 从数据读取器获取复权标志
		else  // 如果数据读取器无效
			flag = 0;  // 设置为0（不复权）
	}

	return flag;  // 返回复权标志
}

/**
 * @brief 获取Tick数据切片
 * @param stdCode 标准合约代码字符串
 * @param count 获取的Tick数量
 * @param etime 结束时间戳，默认为0（使用最新时间）
 * @return WTSTickSlice* 返回Tick数据切片指针，如果数据读取器无效返回NULL
 * 
 * 从数据读取器或缓存中获取指定数量的Tick数据。
 * 如果是后复权合约，会从后复权缓存中获取；否则直接从数据读取器获取。
 * 
 * 实现逻辑：
 * 1. 检查数据读取器是否有效
 * 2. 判断是否为后复权合约（以+结尾）
 * 3. 如果不是后复权，直接从数据读取器获取
 * 4. 如果是后复权，从后复权缓存中获取（如果缓存不存在则创建）
 * 5. 从缓存中查找指定时间范围内的Tick数据
 */
WTSTickSlice* WtDtMgr::get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_reader == NULL)  // 如果数据读取器无效
		return NULL;  // 返回NULL

	/*
	 *	By Wesley @ 2022.02.11
	 *	这里要重新处理一下
	 *	如果是不复权或者前复权，则直接读取底层的实时缓存即可
	 */
	auto len = strlen(stdCode);  // 获取合约代码长度
	bool isHFQ = (stdCode[len - 1] == SUFFIX_HFQ);  // 判断是否为后复权合约（最后一个字符是否为'+'）

	//不是后复权，缓存直接用底层缓存
	if(!isHFQ)  // 如果不是后复权合约
		return _reader->readTickSlice(stdCode, count, etime);  // 直接从数据读取器读取Tick数据切片

	//先转成不带+的标准代码
	std::string pureStdCode(stdCode, len - 1);  // 去掉最后一个字符（+），获取纯合约代码

	if (_ticks_adjusted == NULL)  // 如果复权Tick缓存不存在
		_ticks_adjusted = DataCacheMap::create();  // 创建复权Tick缓存映射表

	//如果缓存没有，先重新生成一下缓存
	auto it = _ticks_adjusted->find(pureStdCode);  // 在复权缓存中查找纯合约代码
	if (it == _ticks_adjusted->end())  // 如果缓存中不存在该合约的数据
	{
		//先读取全部tick数据
		double factor = _engine->get_exright_factor(stdCode, NULL);  // 从引擎获取复权因子
		WTSTickSlice* slice = _reader->readTickSlice(pureStdCode.c_str(), 999999, etime);  // 从数据读取器读取全部Tick数据（数量设为999999表示读取所有）
		std::vector<WTSTickStruct> ayTicks;  // 创建Tick结构体向量
		ayTicks.resize(slice->size());  // 调整向量大小以容纳所有Tick数据
		std::size_t offset = 0;  // 偏移量，用于数据复制
		for (std::size_t bIdx = 0; bIdx < slice->get_block_counts(); bIdx++)  // 遍历切片的所有数据块
		{
			memcpy(&ayTicks[0] + offset, slice->get_block_addr(bIdx), slice->get_block_size(bIdx) * sizeof(WTSTickStruct));  // 将数据块复制到向量中
			offset += slice->get_block_size(bIdx);  // 更新偏移量
		}

		//缓存的数据做一个复权处理
		for (WTSTickStruct& tick : ayTicks)  // 遍历所有Tick数据
		{
			tick.price *= factor;  // 价格乘以复权因子
			tick.open *= factor;  // 开盘价乘以复权因子
			tick.high *= factor;  // 最高价乘以复权因子
			tick.low *= factor;  // 最低价乘以复权因子
		}

		//添加到缓存中
		WTSHisTickData* hisTick = WTSHisTickData::create(stdCode, false, factor);  // 创建历史Tick数据对象（第二个参数false表示不只在有效Tick时追加）
		hisTick->getDataRef().swap(ayTicks);  // 交换向量数据（将复权后的Tick数据移入历史Tick数据对象）
		_ticks_adjusted->add(pureStdCode, hisTick, false);  // 将历史Tick数据添加到缓存（第三个参数false表示不自动增加引用计数）
	}

	WTSHisTickData* hisTick = (WTSHisTickData*)_ticks_adjusted->get(pureStdCode);  // 从缓存中获取历史Tick数据
	uint32_t curDate, curTime, curSecs;  // 当前日期、时间和秒数
	if (etime == 0)  // 如果结束时间为0（使用最新时间）
	{
		curDate = get_date();  // 获取当前日期
		curTime = get_min_time();  // 获取当前分钟时间
		curSecs = get_secs();  // 获取当前秒数

		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;  // 构建时间戳（日期*1000000000 + 分钟*100000 + 秒数）
	}
	else  // 如果指定了结束时间
	{
		//20190807124533900  // 时间戳格式示例：YYYYMMDDHHMMSSmmm
		curDate = (uint32_t)(etime / 1000000000);  // 提取日期部分（前10位）
		curTime = (uint32_t)(etime % 1000000000) / 100000;  // 提取分钟部分（中间部分）
		curSecs = (uint32_t)(etime % 100000);  // 提取秒数部分（后5位）
	}

	//比较时间的对象
	WTSTickStruct eTick;  // 用于比较的Tick结构体
	eTick.action_date = curDate;  // 设置动作日期
	eTick.action_time = curTime * 100000 + curSecs;  // 设置动作时间（分钟*100000 + 秒数）

	auto& ticks = hisTick->getDataRef();  // 获取Tick数据向量的引用

	WTSTickStruct* pTick = std::lower_bound(&ticks.front(), &ticks.back(), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {  // 使用二分查找定位结束时间位置
		if (a.action_date != b.action_date)  // 如果日期不同
			return a.action_date < b.action_date;  // 按日期比较
		else  // 如果日期相同
			return a.action_time < b.action_time;  // 按时间比较
	});

	uint32_t eIdx = pTick - &ticks.front();  // 计算结束位置的索引

	//如果光标定位的tick时间比目标时间打, 则全部回退一个
	if (pTick->action_date > eTick.action_date || pTick->action_time > eTick.action_time)  // 如果找到的Tick时间大于目标时间
	{
		pTick--;  // 指针回退一个位置
		eIdx--;  // 索引减1
	}

	uint32_t cnt = min(eIdx + 1, count);  // 计算实际返回的Tick数量（取结束位置+1和请求数量的较小值）
	uint32_t sIdx = eIdx + 1 - cnt;  // 计算起始位置索引（结束位置+1 - 数量）
	WTSTickSlice* slice = WTSTickSlice::create(stdCode, &ticks.front() + sIdx, cnt);  // 创建Tick数据切片（从起始位置开始，数量为cnt）
	return slice;  // 返回Tick数据切片
}

/**
 * @brief 获取订单队列切片
 * @param stdCode 标准合约代码字符串
 * @param count 获取的数据条数
 * @param etime 结束时间戳，默认为0（使用最新时间）
 * @return WTSOrdQueSlice* 返回订单队列切片指针，如果数据读取器无效返回NULL
 * 
 * 从数据读取器获取订单队列数据。
 */
WTSOrdQueSlice* WtDtMgr::get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_reader == NULL)  // 如果数据读取器无效
		return NULL;  // 返回NULL

	return _reader->readOrdQueSlice(stdCode, count, etime);  // 从数据读取器读取订单队列切片
}

/**
 * @brief 获取订单明细切片
 * @param stdCode 标准合约代码字符串
 * @param count 获取的数据条数
 * @param etime 结束时间戳，默认为0（使用最新时间）
 * @return WTSOrdDtlSlice* 返回订单明细切片指针，如果数据读取器无效返回NULL
 * 
 * 从数据读取器获取订单明细数据。
 */
WTSOrdDtlSlice* WtDtMgr::get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_reader == NULL)  // 如果数据读取器无效
		return NULL;  // 返回NULL

	return _reader->readOrdDtlSlice(stdCode, count, etime);  // 从数据读取器读取订单明细切片
}

/**
 * @brief 获取逐笔成交切片
 * @param stdCode 标准合约代码字符串
 * @param count 获取的数据条数
 * @param etime 结束时间戳，默认为0（使用最新时间）
 * @return WTSTransSlice* 返回逐笔成交切片指针，如果数据读取器无效返回NULL
 * 
 * 从数据读取器获取逐笔成交数据。
 */
WTSTransSlice* WtDtMgr::get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_reader == NULL)  // 如果数据读取器无效
		return NULL;  // 返回NULL

	return _reader->readTransSlice(stdCode, count, etime);  // 从数据读取器读取逐笔成交切片
}

/**
 * @brief 获取K线数据切片
 * @param stdCode 标准合约代码字符串
 * @param period K线周期
 * @param times K线倍数
 * @param count 获取的K线数量
 * @param etime 结束时间戳，默认为0（使用最新时间）
 * @return WTSKlineSlice* 返回K线数据切片指针，如果数据读取器无效返回NULL
 * 
 * 从数据读取器或缓存中获取指定数量和周期的K线数据。
 * 如果times为1且不强制缓存，则直接从数据读取器获取。
 * 如果times大于1，则从缓存中获取重采样后的K线数据。
 * 
 * 实现逻辑：
 * 1. 检查数据读取器是否有效
 * 2. 构建缓存键
 * 3. 如果times为1且不强制缓存，直接从数据读取器获取
 * 4. 如果times大于1或强制缓存，从缓存中获取
 * 5. 如果缓存不存在或数量不足，从数据读取器读取并重采样
 * 6. 只返回已闭合的K线（如果最后一条未闭合则排除）
 */
WTSKlineSlice* WtDtMgr::get_kline_slice(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime /* = 0 */)
{
	if (_reader == NULL)  // 如果数据读取器无效
		return NULL;  // 返回NULL

	thread_local static char key[64] = { 0 };  // 线程本地静态缓冲区（用于构建缓存键）
	fmtutil::format_to(key, "{}-{}", stdCode, (uint32_t)period);  // 格式化缓存键（合约代码-周期）

	// 如果不强制缓存，并且重采样倍数为1，则直接读取slice返回
	if (times == 1 && !_force_cache)  // 如果倍数为1且不强制缓存
	{
		_subed_basic_bars.insert(key);  // 将基础周期K线添加到订阅集合中

		return _reader->readKlineSlice(stdCode, period, count, etime);  // 直接从数据读取器读取K线切片
	}

	//只有非基础周期的会进到下面的步骤
	WTSSessionInfo* sInfo = _engine->get_session_info(stdCode, true);  // 从引擎获取交易会话信息（isCode=true表示stdCode是合约代码）

	if (_bars_cache == NULL)  // 如果K线缓存不存在
		_bars_cache = DataCacheMap::create();  // 创建K线缓存映射表

	fmtutil::format_to(key, "{}-{}-{}", stdCode, (uint32_t)period, times);  // 格式化完整缓存键（合约代码-周期-倍数）

	WTSKlineData* kData = (WTSKlineData*)_bars_cache->get(key);  // 从缓存中获取K线数据（如果不存在则返回NULL）
	//如果缓存里的K线条数大于请求的条数, 则直接返回
	if (kData == NULL || kData->size() < count)  // 如果缓存不存在或缓存中的K线数量不足
	{
		uint32_t realCount = times==1 ? count: (count*times + times);  // 计算需要读取的基础K线数量（重采样时需要多读一些）
		WTSKlineSlice* rawData = _reader->readKlineSlice(stdCode, period, realCount, etime);  // 从数据读取器读取基础K线数据
		if (rawData != NULL && rawData->size() > 0)  // 如果读取成功且数据不为空
		{
			if(times != 1)  // 如果倍数不为1（需要重采样）
			{
				kData = g_dataFact.extractKlineData(rawData, period, times, sInfo, true, _align_by_section);  // 使用数据工厂提取重采样K线数据（考虑小节对齐）
			}
			else  // 如果倍数为1（不需要重采样）
			{
				kData = WTSKlineData::create(stdCode, rawData->size());  // 创建K线数据对象
				kData->setPeriod(period, 1);  // 设置K线周期和倍数
				kData->setClosed(true);  // 设置为已闭合（所有历史数据都是已闭合的）
				WTSBarStruct* pBar = kData->getDataRef().data();  // 获取K线数据数组的起始地址
				for(uint32_t bIdx = 0; bIdx < rawData->get_block_counts(); bIdx++ )  // 遍历切片的所有数据块
				{
					memcpy(pBar, rawData->get_block_addr(bIdx), sizeof(WTSBarStruct)*rawData->get_block_size(bIdx));  // 将数据块复制到K线数据对象中
					pBar += rawData->get_block_size(bIdx);  // 更新目标地址指针
				}
			}
			
			rawData->release();  // 释放原始数据切片（减少引用计数）
		}
		else  // 如果读取失败或数据为空
		{
			return NULL;  // 返回NULL
		}

		if (kData)  // 如果K线数据创建成功
		{
			_bars_cache->add(key, kData, false);  // 将K线数据添加到缓存（第三个参数false表示不自动增加引用计数）
			if(times != 1)  // 如果倍数不为1
				WTSLogger::debug("{} bars of {} resampled every {} bars: {} -> {}", 
					PERIOD_NAME[period], stdCode, times, realCount, kData->size());  // 记录调试日志：K线重采样结果
		}
	}

	/*
	 *	By Wesley @ 2023.03.03
	 *	当多周期K线跨越小节时，如果重启了组合
	 *	这个时候就会在启动的时候拉到一条未闭合的K线
	 *	但是未闭合的K线等一下还会重新推一遍
	 *	所以这里必须要做一个修正
	 *	只处理已经闭合的K线
	 */
	uint32_t closedSz = kData->size();  // 获取K线数据的总数量
	if (closedSz > 0 && !kData->isClosed())  // 如果K线数据不为空且最后一条K线未闭合
		closedSz--;  // 数量减1（排除未闭合的K线）

	int32_t sIdx = 0;  // 起始索引
	uint32_t rtCnt = min(closedSz, count);  // 计算实际返回的K线数量（取闭合数量和请求数量的较小值）
	sIdx = closedSz - rtCnt;  // 计算起始索引（从后往前取rtCnt条）
	WTSBarStruct* rtHead = kData->at(sIdx);  // 获取起始K线数据的指针
	WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, times, rtHead, rtCnt);  // 创建K线切片（从起始位置开始，数量为rtCnt）
	return slice;  // 返回K线切片
}
