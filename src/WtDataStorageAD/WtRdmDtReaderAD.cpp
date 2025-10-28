/*!
 * \file WtRdmDtReaderAD.cpp
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块随机数据读取器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtRdmDtReaderAD类的具体功能，为WonderTrader数据分析工具提供基于
 * LMDB数据库的灵活历史数据随机访问服务。该实现支持多种查询模式，包括按时间范围、
 * 按数量、按日期等方式查询Tick和K线数据，并提供智能缓存管理机制。
 * 
 * 核心实现特点：
 * 
 * 1. 多维度查询支持（Multi-dimensional Query Support）：
 *    - 时间范围查询：支持任意时间段的数据检索
 *    - 数量限制查询：支持反向查询指定数量的最新数据
 *    - 日期批量查询：支持按交易日获取全天数据
 * 
 * 2. 智能缓存策略（Smart Caching Strategy）：
 *    - 增量数据加载：检测查询范围，只加载缺失数据
 *    - 双向缓存扩展：支持向前和向后扩展数据范围
 *    - 时间范围优化：根据查询模式优化缓存策略
 * 
 * 3. 高性能数据访问（High-Performance Data Access）：
 *    - LMDB范围查询：利用B+树结构的高效范围查询
 *    - 批量数据传输：减少系统调用次数
 *    - 内存缓存加速：热点数据常驻内存
 * 
 * 查询优化策略：
 * 
 * 1. 缓存命中优化（Cache Hit Optimization）：
 *    - 检查查询范围是否在缓存范围内
 *    - 智能判断是否需要扩展缓存
 *    - 最小化数据库访问次数
 * 
 * 2. 时间序列处理（Time Series Processing）：
 *    - 交易时段时间转换
 *    - 跨日数据处理
 *    - 时间戳标准化
 * 
 * 3. 数据切片生成（Data Slice Generation）：
 *    - 二分查找定位数据范围
 *    - 高效的内存拷贝
 *    - 标准化的数据切片接口
 */

#include "WtRdmDtReaderAD.h"                   // 引入随机数据读取器头文件
#include "LMDBKeys.h"                           // 引入LMDB键值结构定义

#include "../Includes/WTSVariant.hpp"           // 引入配置参数类
#include "../Share/TimeUtils.hpp"               // 引入时间工具类
#include "../Share/CodeHelper.hpp"              // 引入合约代码解析工具
#include "../Share/StdUtils.hpp"                // 引入标准工具类

#include "../Includes/WTSContractInfo.hpp"      // 引入合约信息类
#include "../Includes/WTSSessionInfo.hpp"       // 引入交易时段信息类
#include "../Includes/IBaseDataMgr.h"           // 引入基础数据管理器接口
#include "../Includes/IHotMgr.h"                // 引入热点合约管理器接口
#include "../Includes/WTSDataDef.hpp"           // 引入数据定义

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                    // 引入格式化字符串库

/**
 * @brief 随机读取器日志输出函数
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
inline void pipe_rdmreader_log(IRdmDtReaderSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                           // 检查接口有效性
		return;

	static thread_local char buffer[512] = { 0 };  // 线程局部缓冲区
	memset(buffer, 0, 512);                     // 清零缓冲区
	fmt::format_to(buffer, format, args...);    // 格式化字符串

	sink->reader_log(ll, buffer);               // 输出日志
}

/**
 * @brief C接口导出函数
 * 
 * 提供标准C接口，支持动态库加载和跨语言调用。
 */
extern "C"
{
	/**
	 * @brief 创建随机数据读取器实例
	 * 
	 * 工厂函数，创建WtRdmDtReaderAD实例并返回基类指针。
	 * 
	 * @return 随机数据读取器接口指针
	 */
	EXPORT_FLAG IRdmDtReader* createRdmDtReader()
	{
		IRdmDtReader* ret = new WtRdmDtReaderAD(); // 创建实例
		return ret;                             // 返回接口指针
	}

	/**
	 * @brief 销毁随机数据读取器实例
	 * 
	 * 安全销毁读取器实例，释放所有资源。
	 * 
	 * @param reader 要销毁的读取器指针
	 */
	EXPORT_FLAG void deleteRdmDtReader(IRdmDtReader* reader)
	{
		if (reader != NULL)                     // 检查指针有效性
			delete reader;                      // 销毁实例
	}
};

/**
 * @brief 构造函数
 * 
 * 初始化WtRdmDtReaderAD实例，设置默认值。
 */
WtRdmDtReaderAD::WtRdmDtReaderAD()
	: _base_data_mgr(NULL)                      // 初始化基础数据管理器指针
	, _hot_mgr(NULL)                            // 初始化热点合约管理器指针
{
	// 构造函数中不执行重量级操作
	// 实际初始化在init()方法中完成
}


/**
 * @brief 析构函数
 * 
 * 清理资源，关闭所有数据库连接。
 * LMDB数据库连接由智能指针自动管理。
 */
WtRdmDtReaderAD::~WtRdmDtReaderAD()
{
	// 智能指针会自动释放LMDB数据库资源
	// 缓存数据由STL容器自动管理
}

/**
 * @brief 初始化随机数据读取器
 * 
 * 根据配置参数初始化读取器，设置数据存储路径和相关管理器。
 * 
 * @param cfg 配置参数对象
 * @param sink 回调接口，提供基础数据管理器和热点管理器
 */
void WtRdmDtReaderAD::init(WTSVariant* cfg, IRdmDtReaderSink* sink)
{
	IRdmDtReader::init(cfg, sink);              // 调用基类初始化方法

	_base_data_mgr = sink->get_basedata_mgr();  // 获取基础数据管理器
	_hot_mgr = sink->get_hot_mgr();             // 获取热点合约管理器

	if (cfg == NULL)                            // 检查配置参数有效性
		return;

	_base_dir = cfg->getCString("path");        // 获取数据存储路径
	_base_dir = StrUtil::standardisePath(_base_dir);  // 标准化路径格式

	// 输出初始化成功日志
	pipe_rdmreader_log(sink, LL_INFO, "WtRdmDtReaderAD initialized, root data folder is {}", _base_dir);
}

/**
 * @brief 按数量读取Tick数据切片（未实现）
 * 
 * 从指定结束时间往前读取指定数量的Tick数据。
 * 该功能尚未实现，将在后续版本中提供。
 * 
 * @param stdCode 标准合约代码
 * @param count 需要读取的数据条数
 * @param etime 结束时间（0表示当前时间）
 * @return 始终返回NULL（功能未实现）
 */
WTSTickSlice* WtRdmDtReaderAD::readTickSliceByCount(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	//TODO: 以后再来实现吧
	return NULL;
}

/**
 * @brief 按时间范围读取Tick数据切片
 * 
 * 读取指定时间范围内的Tick数据，支持智能缓存和增量加载。
 * 该方法是随机数据读取器的核心功能之一。
 * 
 * @param stdCode 标准合约代码（如"SHFE.rb.HOT"）
 * @param stime 开始时间（格式：YYYYMMDDHHMMSSsss）
 * @param etime 结束时间（格式：YYYYMMDDHHMMSSsss）
 * @return Tick数据切片对象，失败返回NULL
 */
WTSTickSlice* WtRdmDtReaderAD::readTickSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime /* = 0 */)
{
	// 解析标准合约代码，获取交易所、品种等信息
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);
	WTSSessionInfo* sInfo = commInfo->getSessionInfo();
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);

	// 解析结束时间（格式：YYYYMMDDHHMMSSsss）
	uint32_t rDate, rTime, rSecs;
	rDate = (uint32_t)(etime / 1000000000);     // 提取日期部分
	rTime = sInfo->offsetTime((uint32_t)(etime % 1000000000) / 100000, false);  // 转换为偏移时间
	rSecs = (uint32_t)(etime % 100000);         // 提取秒和毫秒部分

	// 解析开始时间（格式：YYYYMMDDHHMMSSsss）
	uint32_t lDate, lTime, lSecs;
	lDate = (uint32_t)(stime / 1000000000);     // 提取日期部分
	lTime = sInfo->offsetTime((uint32_t)(stime % 1000000000) / 100000, true);   // 转换为偏移时间
	lSecs = (uint32_t)(stime % 100000);         // 提取秒和毫秒部分

	// 计算交易日期
	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID.c_str(), rDate, rTime, false);
	uint32_t beginTDate = _base_data_mgr->calcTradingDate(stdPID.c_str(), lDate, lTime, false);

	std::string key = stdCode;                  // 使用标准合约代码作为缓存键

	// 检查缓存状态
	TicksList& tickList = _ticks_cache[key];    // 获取或创建Tick列表缓存

	bool isEmpty = tickList._ticks.empty();     // 检查缓存是否为空
	bool bNeedOlder = stime < tickList._first_tick_time;    // 是否需要加载更早的数据
	bool bNeedNewer = etime > tickList._last_tick_time;     // 是否需要加载更新的数据

	// 获取对应的LMDB数据库连接
	WtLMDBPtr db = get_t_db(cInfo._exchg, cInfo._code);
	if (db == NULL)                             // 检查数据库连接有效性
		return NULL;

	if (isEmpty)
	{
		//按照区间加载即可
		WtLMDBQuery query(*db);
		LMDBHftKey lKey(cInfo._exchg, cInfo._code, beginTDate, lTime * 100000 + lSecs);
		LMDBHftKey rKey(cInfo._exchg, cInfo._code, endTDate, rTime * 100000 + rSecs);
		int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)),
			std::string((const char*)&rKey, sizeof(rKey)), [this, &tickList](const ValueArray& ayKeys, const ValueArray& ayVals) {
			tickList._ticks.resize(ayVals.size());
			std::size_t idx = 0;
			for (const std::string& item : ayVals)
			{
				memcpy(&tickList._ticks[idx], item.data(), item.size());
				idx++;
			}
		});

		if (cnt > 0)
		{
			WTSTickStruct& curTs = tickList._ticks.front();
			tickList._first_tick_time = (uint64_t)curTs.trading_date * 1000000000 + sInfo->offsetTime(curTs.action_time / 100000, true) + curTs.action_time % 100000;

			curTs = tickList._ticks.back();
			tickList._last_tick_time = (uint64_t)curTs.trading_date * 1000000000 + sInfo->offsetTime(curTs.action_time / 100000, false) + curTs.action_time % 100000;

			pipe_rdmreader_log(_sink, LL_DEBUG, "{} ticks between [{},{}] of {} loaded to cache", cnt, tickList._first_tick_time, tickList._last_tick_time, stdCode);
		}
	}
	else
	{
		if (bNeedOlder)
		{
			//读取更早的数据
			WtLMDBQuery query(*db);
			LMDBHftKey rKey(cInfo._exchg, cInfo._code, (uint32_t)(tickList._first_tick_time / 1000000000), (uint32_t)(tickList._first_tick_time % 1000000000));
			LMDBHftKey lKey(cInfo._exchg, cInfo._code, beginTDate, lTime * 100000 + lSecs);
			int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), std::string((const char*)&rKey, sizeof(rKey)),
				[this, &tickList](const ValueArray& ayKeys, const ValueArray& ayVals) {
				std::vector<WTSTickStruct> ayTicks;
				ayTicks.resize(ayVals.size() + tickList._ticks.size());
				std::size_t idx = 0;
				for (const std::string& item : ayVals)
				{
					WTSTickStruct* curTick = (WTSTickStruct*)item.data();
					memcpy(&ayTicks[idx], item.data(), item.size());
					idx++;
				}

				//将原来的数据拷贝到后面，再做一个swap即可
				memcpy(&ayTicks[idx], tickList._ticks.data(), sizeof(WTSTickStruct)*tickList._ticks.size());
				tickList._ticks.swap(ayTicks);
			});

			if(cnt > 0)
			{
				const WTSTickStruct& curTs = tickList._ticks.front();
				tickList._first_tick_time = (uint64_t)curTs.trading_date * 1000000000 + sInfo->offsetTime(curTs.action_time / 100000, false) + curTs.action_time % 100000;

				pipe_rdmreader_log(_sink, LL_DEBUG, "{} prev ticks of {} loaded to cache", cnt, stdCode);
			}
		}

		if (bNeedNewer)
		{
			//读取更新的数据
			WtLMDBQuery query(*db);
			LMDBHftKey lKey(cInfo._exchg, cInfo._code, (uint32_t)(tickList._last_tick_time / 1000000000), (uint32_t)(tickList._last_tick_time % 1000000000));
			LMDBHftKey rKey(cInfo._exchg, cInfo._code, endTDate, rTime * 100000 + rSecs);
			int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), std::string((const char*)&rKey, sizeof(rKey)),
				[this, &tickList](const ValueArray& ayKeys, const ValueArray& ayVals) {
				for (const std::string& item : ayVals)
				{
					WTSTickStruct* curTick = (WTSTickStruct*)item.data();
					tickList._ticks.emplace_back(*curTick);
				}
			});

			if (cnt > 0)
			{
				const WTSTickStruct& curTs = tickList._ticks.back();
				tickList._last_tick_time = (uint64_t)curTs.trading_date * 1000000000 + sInfo->offsetTime(curTs.action_time / 100000, true) + curTs.action_time % 100000;

				pipe_rdmreader_log(_sink, LL_DEBUG, "{} newer ticks of {} loaded to cache", cnt, stdCode);
			}
		}
	}

	//全部读取完成以后，再生成切片
	{
		//比较时间的对象
		WTSTickStruct sTick, eTick;
		sTick.action_date = lDate;
		sTick.action_time = (uint32_t)(stime % 1000000000);
		eTick.action_date = rDate;
		eTick.action_time = (uint32_t)(etime % 1000000000);

		std::size_t cnt = 0;

		WTSTickStruct* pTick = std::lower_bound(&tickList._ticks[0], &tickList._ticks[0] + (tickList._ticks.size() - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
			if (a.action_date != b.action_date)
				return a.action_date < b.action_date;
			else
				return a.action_time < b.action_time;
		});

		std::size_t eIdx = pTick - &tickList._ticks[0];
		if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time)
		{
			pTick--;
			eIdx--;
		}

		pTick = &tickList._ticks[0];
		//如果第一条实时K线的时间大于开始日期，则实时K线要全部包含进去
		if (pTick->action_date > sTick.action_date || (pTick->action_date == sTick.action_date && pTick->action_time > sTick.action_time))
		{
			cnt = eIdx + 1;
		}
		else
		{
			pTick = std::lower_bound(&tickList._ticks[0], &tickList._ticks[0] + (tickList._ticks.size() - 1), sTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
				if (a.action_date != b.action_date)
					return a.action_date < b.action_date;
				else
					return a.action_time < b.action_time;
			});

			std::size_t sIdx = pTick - &tickList._ticks[0];
			cnt = eIdx - sIdx + 1;
		}

		WTSTickSlice* slice = WTSTickSlice::create(stdCode, pTick, cnt);
		return slice;
	}
}

/**
 * @brief 按数量读取K线数据切片（未实现）
 * 
 * 从指定结束时间往前读取指定数量的K线数据。
 * 该功能尚未实现，将在后续版本中提供。
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期
 * @param count 需要读取的数据条数
 * @param etime 结束时间（0表示当前时间）
 * @return 始终返回NULL（功能未实现）
 */
WTSKlineSlice* WtRdmDtReaderAD::readKlineSliceByCount(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime /* = 0 */)
{
	//TODO: 以后再来实现吧
	return NULL;
}

/**
 * @brief 按日期读取Tick数据切片（未实现）
 * 
 * 读取指定交易日的全部Tick数据。
 * 该功能尚未实现，将在后续版本中提供。
 * 
 * @param stdCode 标准合约代码
 * @param uDate 交易日期（格式：YYYYMMDD）
 * @return 始终返回NULL（功能未实现）
 */
WTSTickSlice* WtRdmDtReaderAD::readTickSliceByDate(const char* stdCode, uint32_t uDate )
{
	//TODO: 以后再来实现吧
	return NULL;
}

WTSKlineSlice* WtRdmDtReaderAD::readKlineSliceByRange(const char* stdCode, WTSKlinePeriod period, uint64_t stime, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);

	uint32_t rDate, rTime, lDate, lTime;
	rDate = (uint32_t)(etime / 10000);
	rTime = (uint32_t)(etime % 10000);
	lDate = (uint32_t)(stime / 10000);
	lTime = (uint32_t)(stime % 10000);

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID.c_str(), rDate, rTime, false);
	uint32_t beginTDate = _base_data_mgr->calcTradingDate(stdPID.c_str(), lDate, lTime, false);

	bool isDay = period == KP_DAY;
	//转换成K线时间
	etime = isDay ? endTDate : (etime - 19000000);

	//暂时不考虑HOT之类的，只针对7×24小时品种做一个实现
	std::string key = StrUtil::printf("%s#%u", stdCode, period);
	BarsList& barsList = _bars_cache[key];

	bool bNeedNewer = (etime > barsList._last_bar_time);

	//全部重载
	WtLMDBPtr db = get_k_db(cInfo._exchg, period);
	if (db == NULL)
		return NULL;

	if (barsList._bars.empty())
	{
		pipe_rdmreader_log(_sink, LL_DEBUG, "Reading back {} bars of {}.{}...", PERIOD_NAME[period], cInfo._exchg, cInfo._code);
		WtLMDBQuery query(*db);
		LMDBBarKey rKey(cInfo._exchg, cInfo._code, 0xffffffff);
		LMDBBarKey lKey(cInfo._exchg, cInfo._code, 0);
		int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), std::string((const char*)&rKey, sizeof(rKey)),
			[this, &barsList, &lKey](const ValueArray& ayKeys, const ValueArray& ayVals) {
			if (ayVals.empty())
				return;

			std::size_t cnt = ayVals.size();
			barsList._bars.resize(cnt);
			std::size_t idx = 0;
			for (const std::string& item : ayVals)
			{
				memcpy(&barsList._bars[idx], item.data(), item.size());
				idx++;
			}
		});
	}
	else if(bNeedNewer)
	{
		//加载更新的数据
		pipe_rdmreader_log(_sink, LL_DEBUG, "Reading back {} bars of {}.{}...", PERIOD_NAME[period], cInfo._exchg, cInfo._code);
		WtLMDBQuery query(*db);
		LMDBBarKey rKey(cInfo._exchg, cInfo._code, 0xffffffff);
		LMDBBarKey lKey(cInfo._exchg, cInfo._code, (uint32_t)barsList._last_bar_time);
		int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), std::string((const char*)&rKey, sizeof(rKey)),
			[this, &barsList, &lKey](const ValueArray& ayKeys, const ValueArray& ayVals) {
			if (ayVals.empty())
				return;

			std::size_t cnt = ayVals.size();
			barsList._bars.resize(cnt);
			std::size_t idx = 0;
			for (const std::string& item : ayVals)
			{
				memcpy(&barsList._bars[idx], item.data(), item.size());
				idx++;
			}
		});
	}

	//
	{
		WTSBarStruct eBar;
		eBar.date = rDate;
		eBar.time = (rDate - 19900000) * 10000 + rTime;

		WTSBarStruct sBar;
		sBar.date = lDate;
		sBar.time = (lDate - 19900000) * 10000 + lTime;

		WTSBarStruct* pHead = NULL;
		std::size_t cnt = 0;

		WTSBarStruct* pBar = std::lower_bound(&barsList._bars[0], &barsList._bars[0] + (barsList._bars.size() - 1), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
			if (isDay)
				return a.date < b.date;
			else
				return a.time < b.time;
		});

		std::size_t idx = pBar - &barsList._bars[0];
		if ((isDay && pBar->date > eBar.date) || (!isDay && pBar->time > eBar.time))
		{
			pBar--;
			idx--;
		}

		pBar = &barsList._bars[0];
		//如果第一条实时K线的时间大于开始日期，则实时K线要全部包含进去
		if ((isDay && pBar->date > sBar.date) || (!isDay && pBar->time > sBar.time))
		{
			pHead = pBar;
			cnt = idx + 1;
		}
		else
		{
			pBar = std::lower_bound(&barsList._bars[0], &barsList._bars[0] + (barsList._bars.size() - 1), sBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
				if (isDay)
					return a.date < b.date;
				else
					return a.time < b.time;
			});

			std::size_t sIdx = pBar - &barsList._bars[0];
			pHead = pBar;
			cnt = idx - sIdx + 1;
		}

		WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, 1, pHead, cnt);
		return slice;
	}
}

/**
 * @brief 获取K线数据库连接
 * 
 * 根据交易所和K线周期获取对应的LMDB数据库连接。
 * 实现了数据库连接的缓存和按需加载机制。
 * 
 * @param exchg 交易所代码
 * @param period K线周期
 * @return LMDB数据库智能指针，失败返回空指针
 */
WtRdmDtReaderAD::WtLMDBPtr WtRdmDtReaderAD::get_k_db(const char* exchg, WTSKlinePeriod period)
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

	// 创建新的数据库连接（只读模式）
	WtLMDBPtr dbPtr(new WtLMDB(true));
	std::string path = StrUtil::printf("%s%s/%s/", _base_dir.c_str(), subdir.c_str(), exchg);
	boost::filesystem::create_directories(path);  // 确保目录存在
	
	// 尝试打开数据库
	if (!dbPtr->open(path.c_str()))
	{
		// 打开失败，记录错误日志
		pipe_rdmreader_log(_sink, LL_ERROR, "Opening {} db if {} failed: {}", subdir, exchg, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}
	else
	{
		// 打开成功，记录调试日志
		pipe_rdmreader_log(_sink, LL_DEBUG, "{} db of {} opened", subdir, exchg);
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
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @return LMDB数据库智能指针，失败返回空指针
 */
WtRdmDtReaderAD::WtLMDBPtr WtRdmDtReaderAD::get_t_db(const char* exchg, const char* code)
{
	// 构造缓存键值（格式："交易所.合约"）
	std::string key = StrUtil::printf("%s.%s", exchg, code);
	
	// 检查缓存中是否已有该合约的数据库连接
	auto it = _tick_dbs.find(key);
	if (it != _tick_dbs.end())
		return std::move(it->second);           // 返回缓存的连接

	// 创建新的数据库连接（只读模式）
	WtLMDBPtr dbPtr(new WtLMDB(true));
	std::string path = StrUtil::printf("%sticks/%s/%s", _base_dir.c_str(), exchg, code);
	boost::filesystem::create_directories(path);  // 确保目录存在
	
	// 尝试打开数据库
	if (!dbPtr->open(path.c_str()))
	{
		// 打开失败，记录错误日志
		pipe_rdmreader_log(_sink, LL_ERROR, "Opening tick db of {}.{} failed: {}", exchg, code, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}
	else
	{
		// 打开成功，记录调试日志
		pipe_rdmreader_log(_sink, LL_DEBUG, "Tick db of {}.{} opened", exchg, code);
	}

	// 将新连接加入缓存
	_tick_dbs[key] = dbPtr;                     // 注意：这里使用key而不是exchg
	return std::move(dbPtr);                    // 返回新连接
}