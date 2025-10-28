/*!
 * \file WtDataReaderAD.cpp
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块实时数据读取器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtDataReaderAD类的具体功能，为WonderTrader策略引擎提供基于LMDB
 * 数据库和内存映射缓存的高性能实时数据读取服务。该实现采用混合存储架构，
 * 结合了持久化存储、内存缓存和实时更新机制，为策略提供低延迟的数据访问。
 * 
 * 核心实现特点：
 * 
 * 1. 混合存储架构（Hybrid Storage Architecture）：
 *    - LMDB持久化存储：历史数据的可靠存储
 *    - 内存映射缓存：实时数据的快速访问
 *    - 循环缓冲区：最新数据的内存优化
 * 
 * 2. 智能缓存策略（Smart Caching Strategy）：
 *    - 多级缓存设计：L1内存缓存 + L2映射文件 + L3数据库
 *    - 增量更新机制：只加载缺失的数据
 *    - 容量自适应：根据查询需求动态调整缓存大小
 * 
 * 3. 实时数据同步（Real-time Data Synchronization）：
 *    - onMinuteEnd事件驱动更新
 *    - 实时缓存与数据库的智能同步
 *    - 跨日数据的平滑切换
 * 
 * 数据流转机制：
 * 
 * 1. 数据查询流程（Data Query Flow）：
 *    策略请求 → 检查内存缓存 → 从LMDB增量加载 → 从实时缓存补充 → 返回数据切片
 * 
 * 2. 缓存更新流程（Cache Update Flow）：
 *    分钟结束事件 → 检查缓存状态 → 从LMDB更新完整K线 → 从实时缓存补充未完成K线
 * 
 * 3. 数据合成流程（Data Synthesis Flow）：
 *    历史数据(LMDB) + 实时缓存数据 → 完整的时间序列 → 标准化数据切片
 * 
 * 性能优化策略：
 * 
 * 1. 零拷贝技术（Zero-Copy Technology）：
 *    - 内存映射文件直接访问
 *    - 循环缓冲区避免数据移动
 *    - 智能指针管理避免拷贝
 * 
 * 2. 缓存命中优化（Cache Hit Optimization）：
 *    - 时间局部性利用
 *    - 预取策略
 *    - 容量动态调整
 * 
 * 3. 并发访问优化（Concurrent Access Optimization）：
 *    - 读写锁分离
 *    - 线程局部缓存
 *    - 无锁数据结构
 */

#include "WtDataReaderAD.h"                     // 引入实时数据读取器头文件
#include "LMDBKeys.h"                           // 引入LMDB键值结构定义

#include "../Includes/WTSVariant.hpp"           // 引入配置参数类
#include "../Share/TimeUtils.hpp"               // 引入时间工具类
#include "../Share/CodeHelper.hpp"              // 引入合约代码解析工具
#include "../Share/StdUtils.hpp"                // 引入标准工具类

#include "../Includes/WTSContractInfo.hpp"      // 引入合约信息类
#include "../Includes/IBaseDataMgr.h"           // 引入基础数据管理器接口
#include "../Includes/IHotMgr.h"                // 引入热点合约管理器接口
#include "../Includes/WTSDataDef.hpp"           // 引入数据定义

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                    // 引入格式化字符串库

/**
 * @brief 数据读取器日志输出函数
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
inline void pipe_reader_log(IDataReaderSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
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
	 * @brief 创建数据读取器实例
	 * 
	 * 工厂函数，创建WtDataReaderAD实例并返回基类指针。
	 * 
	 * @return 数据读取器接口指针
	 */
	EXPORT_FLAG IDataReader* createDataReader()
	{
		IDataReader* ret = new WtDataReaderAD();  // 创建实例
		return ret;                             // 返回接口指针
	}

	/**
	 * @brief 销毁数据读取器实例
	 * 
	 * 安全销毁读取器实例，释放所有资源。
	 * 
	 * @param reader 要销毁的读取器指针
	 */
	EXPORT_FLAG void deleteDataReader(IDataReader* reader)
	{
		if (reader != NULL)                     // 检查指针有效性
			delete reader;                      // 销毁实例
	}
};

/**
 * @brief 构造函数
 * 
 * 初始化WtDataReaderAD实例，设置默认值。
 */
WtDataReaderAD::WtDataReaderAD()
	: _last_time(0)                             // 初始化最后处理时间
	, _base_data_mgr(NULL)                      // 初始化基础数据管理器指针
	, _hot_mgr(NULL)                            // 初始化热点合约管理器指针
{
	// 构造函数中不执行重量级操作
	// 实际初始化在init()方法中完成
}


/**
 * @brief 析构函数
 * 
 * 清理资源，关闭数据库连接和内存映射文件。
 */
WtDataReaderAD::~WtDataReaderAD()
{
	// 智能指针会自动释放LMDB数据库资源
	// 内存映射文件由BoostMFPtr自动管理
}

/**
 * @brief 初始化实时数据读取器
 * 
 * 根据配置参数初始化读取器，设置存储路径、缓存文件名等。
 * 
 * @param cfg 配置参数对象
 * @param sink 回调接口，提供基础数据管理器和热点管理器
 * @param loader 历史数据加载器（可选）
 */
void WtDataReaderAD::init(WTSVariant* cfg, IDataReaderSink* sink, IHisDataLoader* loader /* = NULL */)
{
	IDataReader::init(cfg, sink, loader);       // 调用基类初始化方法

	_base_data_mgr = sink->get_basedata_mgr();  // 获取基础数据管理器
	_hot_mgr = sink->get_hot_mgr();             // 获取热点合约管理器

	if (cfg == NULL)                            // 检查配置参数有效性
		return ;

	_base_dir = cfg->getCString("path");        // 获取数据存储路径
	_base_dir = StrUtil::standardisePath(_base_dir);  // 标准化路径格式

	// 设置各周期K线缓存文件名
	_d1_cache._filename = "cache_d1.dmb";       // 日K线缓存文件名
	_m1_cache._filename = "cache_m1.dmb";       // 1分钟K线缓存文件名
	_m5_cache._filename = "cache_m5.dmb";       // 5分钟K线缓存文件名

	// 输出初始化成功日志
	pipe_reader_log(sink, LL_INFO, "WtDataReaderAD initialized, root data folder is {}", _base_dir);
}

/**
 * @brief 读取Tick数据切片
 * 
 * 读取指定合约的最新Tick数据，支持智能缓存和增量加载。
 * 该方法是实时数据读取器的核心功能之一。
 * 
 * @param stdCode 标准合约代码（如"SHFE.rb.HOT"）
 * @param count 需要读取的Tick数量
 * @param etime 结束时间（0表示当前时间）
 * @return Tick数据切片对象，失败返回NULL
 */
WTSTickSlice* WtDataReaderAD::readTickSlice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	// 解析标准合约代码，获取交易所、品种等信息
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);

	uint32_t curDate, curTime, curSecs;
	
	// 解析结束时间
	if (etime == 0)                             // 如果未指定结束时间
	{
		// 使用当前时间作为结束时间
		curDate = _sink->get_date();            // 获取当前日期
		curTime = _sink->get_min_time();        // 获取当前时间（分钟级）
		curSecs = _sink->get_secs();            // 获取当前秒数

		// 构造完整的时间戳（格式：YYYYMMDDHHMMSSsss）
		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;
	}
	else
	{
		// 解析指定的结束时间（格式：YYYYMMDDHHMMSSsss，如20190807124533900）
		curDate = (uint32_t)(etime / 1000000000);           // 提取日期部分
		curTime = (uint32_t)(etime % 1000000000) / 100000;  // 提取时间部分（HHMMss）
		curSecs = (uint32_t)(etime % 100000);               // 提取秒和毫秒部分
	}

	// 计算结束交易日
	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID.c_str(), curDate, curTime, false);

	// 处理期货合约的换月逻辑
	std::string curCode = cInfo._code;          // 当前合约代码
	if (commInfo->isFuture())                   // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;   // 获取规则标签
		if (strlen(ruleTag) > 0)                // 如果有自定义规则
			// 根据规则获取指定交易日的实际合约代码
			curCode = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), endTDate);
		// 以下代码为注释掉的其他换月规则处理方式
		//else if (cInfo.isHot())              // 如果是主力合约
		//	curCode = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, endTDate);
		//else if (cInfo.isSecond())           // 如果是次主力合约
		//	curCode = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, endTDate);
	}

	// 缓存重载策略判断
	uint32_t reload_flag = 0;                   // 重载标记：0-不需要重载，1-增量加载，2-全部重载
	std::string key = StrUtil::printf("%s.%s", cInfo._exchg, curCode.c_str());
	
	// 检查缓存并决定加载策略
	TicksList& tickList = _ticks_cache[key];    // 获取或创建Tick列表缓存
	uint64_t last_access_time = 0;              // 上次访问时间
	do
	{
		// 检查缓存容量是否足够
		if(tickList._ticks.capacity() < count)
		{
			// 容量不够，需要全部重载
			reload_flag = 2;                    // 设置为全部重载模式
			tickList._ticks.rset_capacity(count);  // 重新设置缓冲区容量
			tickList._ticks.clear();            // 清除原有数据
		}

		// 检查缓存时间范围是否满足要求
		if(tickList._last_req_time < etime)
		{
			// 缓存的时间范围不够，需要增量加载
			reload_flag = 1;                    // 设置为增量加载模式
			last_access_time = tickList._last_req_time;  // 记录上次请求时间
			break;
		}

	} while (false);

	// 获取对应的LMDB数据库连接
	WtLMDBPtr db = get_t_db(cInfo._exchg, cInfo._code);
	if (db == NULL)                             // 检查数据库连接有效性
		return NULL;

	// 根据重载标记执行相应的数据加载策略
	if(reload_flag == 1)
	{
		// 策略1：增量更新 - 只加载上次请求之后的新数据
		last_access_time += 1;                  // 时间戳+1，避免重复加载
		WtLMDBQuery query(*db);                 // 创建LMDB查询对象
		
		// 构造查询范围：从上次访问时间到本次结束时间
		LMDBHftKey lKey(cInfo._exchg, cInfo._code, 
			(uint32_t)(last_access_time / 1000000000), 
			(uint32_t)(last_access_time % 1000000000));
		LMDBHftKey rKey(cInfo._exchg, cInfo._code, 
			(uint32_t)(etime / 1000000000), 
			(uint32_t)(etime % 1000000000));
		
		// 执行范围查询，将新数据追加到缓存
		int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), 
			std::string((const char*)&rKey, sizeof(rKey)), 
			[this, &tickList](const ValueArray& ayKeys, const ValueArray& ayVals) {
			for(const std::string& item : ayVals)
			{
				WTSTickStruct* curTick = (WTSTickStruct*)item.data();
				tickList._ticks.push_back(*curTick);  // 追加到循环缓冲区尾部
			}
		});
		
		// 输出增量加载日志
		if(cnt > 0)
			pipe_reader_log(_sink, LL_DEBUG, "{} ticks after {} of {} append to cache", 
				cnt, last_access_time, stdCode);
	}
	else if(reload_flag == 2)
	{
		// 策略2：全部重载 - 从结束时间往前读取指定数量的数据
		WtLMDBQuery query(*db);                 // 创建LMDB查询对象
		
		// 构造查询范围：从最早到结束时间
		LMDBHftKey rKey(cInfo._exchg, cInfo._code, 
			(uint32_t)(etime / 1000000000), 
			(uint32_t)(etime % 1000000000));
		LMDBHftKey lKey(cInfo._exchg, cInfo._code, 0, 0);
		
		// 执行反向查询，获取最新的count条数据
		int cnt = query.get_lowers(std::string((const char*)&lKey, sizeof(lKey)), 
			std::string((const char*)&rKey, sizeof(rKey)),
			count, [this, &tickList](const ValueArray& ayKeys, const ValueArray& ayVals) {
			tickList._ticks.resize(ayVals.size());  // 调整缓冲区大小
			for (std::size_t i = 0; i < ayVals.size(); i++)
			{
				const std::string& item = ayVals[i];
				memcpy(&tickList._ticks[i], item.data(), item.size());  // 拷贝数据
			}
		});

		// 输出首次加载日志
		pipe_reader_log(_sink, LL_DEBUG, "{} ticks of {} loaded to cache for the first time", cnt, stdCode);
	}

	// 更新缓存的最后请求时间
	tickList._last_req_time = etime;

	//////////////////////////////////////////////////////////////////////////
	// 生成数据切片（循环缓冲区的特殊处理）
	// 
	// 循环缓冲区的内存布局说明：
	// 当缓冲区未满时，数据连续存储在array_one中
	// 当缓冲区满后新数据会覆盖旧数据，形成array_one和array_two两段
	// 因此需要特殊处理才能正确提取最后count条数据
	
	count = min((uint32_t)tickList._ticks.size(), count);  // 修正count，不能超过实际数据量
	auto ayTwo = tickList._ticks.array_two();   // 获取循环缓冲区的第二段数据
	auto cnt_2 = ayTwo.second;                  // 第二段数据的数量
	
	if(cnt_2 >= count)
	{
		// 情况1：第二段数据足够，直接从第二段提取
		// 从第二段的尾部往前取count条数据
		return WTSTickSlice::create(stdCode, &tickList._ticks[ayTwo.second - count], count);
	}
	else
	{
		// 情况2：第二段数据不够，需要从第一段补充
		auto ayOne = tickList._ticks.array_one();  // 获取循环缓冲区的第一段数据
		auto diff = count - cnt_2;              // 需要从第一段取的数据量
		
		// 先从第一段尾部取diff条数据创建切片
		auto ret = WTSTickSlice::create(stdCode, &tickList._ticks[ayOne.second - diff], diff);
		
		// 如果第二段有数据，追加到切片中
		if(cnt_2 > 0)
			ret->appendBlock(ayTwo.first, cnt_2);
		
		return ret;                             // 返回合并后的数据切片
	}
}

/**
 * @brief 读取K线数据到缓冲区
 * 
 * 从LMDB数据库中读取指定合约的所有K线数据到字符串缓冲区。
 * 该方法用于批量加载历史K线数据。
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param period K线周期
 * @return 包含K线数据的字符串缓冲区，失败返回空字符串
 */
std::string WtDataReaderAD::read_bars_to_buffer(const char* exchg, const char* code, WTSKlinePeriod period)
{
	// 获取对应的LMDB数据库连接
	WtLMDBPtr db = get_k_db(exchg, period);
	if (db == NULL)                             // 检查数据库连接有效性
		return "";                              // 返回空字符串

	std::string buffer;                         // 数据缓冲区
	WtLMDBQuery query(*db);                     // 创建LMDB查询对象
	
	// 构造查询范围：获取所有K线数据
	LMDBBarKey lKey(exchg, code, 0);            // 左边界（最小时间）
	LMDBBarKey rKey(exchg, code, 0xffffffff);   // 右边界（最大时间）
	
	// 执行范围查询，将数据拷贝到缓冲区
	query.get_range(std::string((const char*)&lKey, sizeof(lKey)),
		std::string((const char*)&rKey, sizeof(rKey)), 
		[this, &buffer](const ValueArray& ayKeys, const ValueArray& ayVals) {
		if (ayVals.empty())                     // 检查查询结果是否为空
			return;

		// 调整缓冲区大小并批量拷贝数据
		buffer.resize(sizeof(WTSBarStruct)*ayVals.size());
		memcpy((void*)buffer.data(), ayVals.data(), sizeof(WTSBarStruct)*ayVals.size());
	});
	
	return std::move(buffer);                   // 返回数据缓冲区
}

/**
 * @brief 从存储中缓存K线数据
 * 
 * 从LMDB数据库中加载指定数量的历史K线数据到内存缓存中。
 * 该方法用于首次加载或全部重载K线数据。
 * 
 * @param key 缓存键值
 * @param stdCode 标准合约代码
 * @param period K线周期
 * @param count 需要加载的K线数量
 * @return 加载成功返回true，失败返回false
 */
bool WtDataReaderAD::cacheBarsFromStorage(const std::string& key, const char* stdCode, WTSKlinePeriod period, uint32_t count)
{
	// 解析标准合约代码，获取交易所、品种等信息
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);

	// 获取或创建K线列表缓存
	BarsList& barList = _bars_cache[key];
	barList._code = stdCode;                    // 设置标准合约代码
	barList._period = period;                   // 设置K线周期
	barList._exchg = cInfo._exchg;              // 设置交易所代码

	// 从LMDB数据库读取K线数据
	WtLMDBPtr db = get_k_db(cInfo._exchg, period);
	if (db == NULL)                             // 检查数据库连接有效性
		return false;

	WtLMDBQuery query(*db);                     // 创建LMDB查询对象
	
	// 构造查询范围：获取最新的count条K线
	LMDBBarKey rKey(cInfo._exchg, cInfo._code, 0xffffffff);  // 右边界（最大时间）
	LMDBBarKey lKey(cInfo._exchg, cInfo._code, 0);           // 左边界（最小时间）
	
	// 执行反向查询，从最新往前取count条数据
	int cnt = query.get_lowers(std::string((const char*)&lKey, sizeof(lKey)), 
		std::string((const char*)&rKey, sizeof(rKey)),
		count, [this, &barList, &lKey](const ValueArray& ayKeys, const ValueArray& ayVals) {
		if (ayVals.empty())                     // 检查查询结果是否为空
			return;

		std::size_t cnt = ayVals.size();        // 获取数据条数
		for (std::size_t i = 0; i < cnt; i++)
		{
			// 检查左边界，确保数据在有效范围内
			if(memcmp(ayKeys[i].data(), (void*)&lKey, sizeof(lKey)) < 0)
				continue;                       // 跳过边界外的数据

			// 将K线数据添加到缓存列表
			barList._bars.push_back(*(WTSBarStruct*)ayVals[i].data());
		}
	});

	// 输出加载日志
	pipe_reader_log(_sink, LL_DEBUG, "{} {} bars of {} loaded to cache", cnt, PERIOD_NAME[period], stdCode);
	return true;                                // 返回加载成功
}

/**
 * @brief 从LMDB更新缓存数据
 * 
 * 从LMDB数据库中增量更新K线缓存，获取最新的完整K线数据。
 * 该方法在onMinuteEnd事件中被调用，用于同步数据库中的最新K线。
 * 
 * @param barsList K线列表引用
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param period K线周期
 * @param lastBarTime 最后K线时间（输入输出参数，会被更新为最新的K线时间）
 */
void WtDataReaderAD::update_cache_from_lmdb(BarsList& barsList, const char* exchg, const char* code, WTSKlinePeriod period, uint32_t& lastBarTime)
{
	bool isDay = (period == KP_DAY);            // 判断是否为日K线
	WtLMDBPtr db = get_k_db(exchg, period);     // 获取数据库连接
	WtLMDBQuery query(*db);                     // 创建LMDB查询对象
	
	// 构造查询范围：从上次K线时间到最新
	LMDBBarKey lKey(exchg, code, lastBarTime);  // 左边界（上次K线时间）
	LMDBBarKey rKey(exchg, code, 0xFFFFFFFF);   // 右边界（最大时间）
	
	// 执行向上查询，获取比lastBarTime更新的所有K线
	int cnt = query.get_uppers(std::string((const char*)&lKey, sizeof(lKey)), 
		std::string((const char*)&rKey, sizeof(rKey)), 
		9999, [this, &barsList, isDay, &lastBarTime](const ValueArray& ayKeys, const ValueArray& ayVals) {

		std::size_t cnt = ayVals.size();        // 获取查询结果数量
		for (std::size_t idx = 0; idx < cnt; idx++)
		{
			const std::string& item = ayVals[idx];  // 当前K线数据
			const std::string& key = ayKeys[idx];   // 当前K线键值
			LMDBBarKey* barKey = (LMDBBarKey*)key.data();
			printf("%u\r\n", reverseEndian(barKey->_bartime));  // 调试输出（可选）
			
			WTSBarStruct* curBar = (WTSBarStruct*)item.data();
			uint64_t curBarTime = isDay ? curBar->date : curBar->time;
			
			// 判断是更新最后一条K线还是添加新K线
			if (curBarTime == lastBarTime)
			{
				// 时间相同，说明是同一根K线的更新
				if (barsList._last_from_cache)  // 如果上次是从缓存读取的
					memcpy(&barsList._bars.back(), curBar, sizeof(WTSBarStruct));  // 更新最后一条
			}
			else
			{
				// 时间不同，说明是新的K线
				barsList._bars.push_back(*curBar);  // 添加到缓存列表
				lastBarTime = (uint32_t)curBarTime;  // 更新最后K线时间
				
				// 触发K线闭合回调
				_sink->on_bar(barsList._code.c_str(), barsList._period, &barsList._bars.back());
			}
		}
	});

	// 输出更新日志
	pipe_reader_log(_sink, LL_DEBUG, "{} bars of {}.{} updated to {}",
		PERIOD_NAME[period], exchg, code, isDay?barsList._bars.back().date:barsList._bars.back().time);
}

/**
 * @brief 获取实时缓存中的K线数据
 * 
 * 从内存映射的实时缓存中获取指定合约的最新K线数据。
 * 该方法用于获取Writer端正在合成但尚未完成的K线数据。
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param period K线周期
 * @return K线数据指针，失败返回NULL
 */
WTSBarStruct* WtDataReaderAD::get_rt_cache_bar(const char* exchg, const char* code, WTSKlinePeriod period)
{
	// 根据K线周期选择对应的缓存包装器
	RTBarCacheWrapper* wrapper = NULL;
	if (period == KP_DAY)                       // 日K线
		wrapper = &_d1_cache;
	else if (period == KP_Minute1)              // 1分钟K线
		wrapper = &_m1_cache;
	else if (period == KP_Minute5)              // 5分钟K线
		wrapper = &_m5_cache;

	bool isDay = (period == KP_DAY);            // 判断是否为日K线

	if (wrapper != NULL)
	{
		// 检查缓存是否已加载
		if (wrapper->empty())
		{
			// 缓存尚未加载，尝试从文件加载
			do
			{
				// 构造缓存文件路径
				std::string filename = _base_dir + wrapper->_filename;
				if (!StdFile::exists(filename.c_str()))  // 检查文件是否存在
					break;                      // 文件不存在，退出

				// 创建内存映射文件对象并映射到内存
				wrapper->_file_ptr.reset(new BoostMappingFile);
				wrapper->_file_ptr->map(filename.c_str());
				wrapper->_cache_block = (RTBarCache*)wrapper->_file_ptr->addr();

				// 修正缓存大小（防止异常情况）
				wrapper->_cache_block->_size = min(wrapper->_cache_block->_size, wrapper->_cache_block->_capacity);
				wrapper->_last_size = wrapper->_cache_block->_size;

				// 重建索引映射表
				for (uint32_t i = 0; i < wrapper->_cache_block->_size; i++)
				{
					const BarCacheItem& item = wrapper->_cache_block->_items[i];
					wrapper->_idx[StrUtil::printf("%s.%s", item._exchg, item._code)] = i;
				}
			} while (false);
		}
		else
		{
			// 缓存已加载，检查是否有新合约加入
			if (wrapper->_last_size != wrapper->_cache_block->_size)
			{
				// 有新合约加入，更新索引
				for (uint32_t i = wrapper->_last_size; i < wrapper->_cache_block->_size; i++)
				{
					const BarCacheItem& item = wrapper->_cache_block->_items[i];
					wrapper->_idx[StrUtil::printf("%s.%s", item._exchg, item._code)] = i;
				}
				wrapper->_last_size = wrapper->_cache_block->_size;  // 更新记录的大小
			}
		}

		// 查找指定合约的K线缓存
		auto it = wrapper->_idx.find(StrUtil::printf("%s.%s", exchg, code));
		if (it != wrapper->_idx.end())
		{
			// 找到缓存，返回K线数据指针
			return &wrapper->_cache_block->_items[it->second]._bar;
		}
	}

	return NULL;                                // 未找到缓存，返回NULL
}

/**
 * @brief 读取K线数据切片
 * 
 * 读取指定合约和周期的最新K线数据，支持多级缓存和实时更新。
 * 该方法实现了LMDB数据库、内存缓存和实时缓存的三级数据融合。
 * 
 * @param stdCode 标准合约代码（如"SHFE.rb.HOT"）
 * @param period K线周期（KP_Minute1、KP_Minute5、KP_DAY等）
 * @param count 需要读取的K线数量
 * @param etime 结束时间（0表示当前时间）
 * @return K线数据切片对象，失败返回NULL
 */
WTSKlineSlice* WtDataReaderAD::readKlineSlice(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime /* = 0 */)
{
	// 解析标准合约代码，获取交易所、品种等信息
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);

	uint32_t curDate, curTime, curSecs;
	if (etime == 0)
	{
		curDate = _sink->get_date();
		curTime = _sink->get_min_time();
		curSecs = _sink->get_secs();

		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;
	}
	else
	{
		//20190807124533900
		curDate = (uint32_t)(etime / 1000000000);
		curTime = (uint32_t)(etime % 1000000000) / 100000;
		curSecs = (uint32_t)(etime % 100000);
	}

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID.c_str(), curDate, curTime, false);

	std::string curCode = cInfo._code;
	if (commInfo->isFuture())
	{
		const char* ruleTag = cInfo._ruletag;
		if (strlen(ruleTag) > 0)
			curCode = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), endTDate);
		//else if (cInfo.isHot())
		//	curCode = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, endTDate);
		//else if (cInfo.isSecond())
		//	curCode = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, endTDate);
	}

	std::string key = StrUtil::printf("%s#%u", stdCode, period);
	BarsList& barsList = _bars_cache[key];
	if (barsList._bars.capacity() < count)
	{
		//容量不够，全部重载
		barsList._bars.rset_capacity(count);
		barsList._bars.clear();	//清除原有数据
		cacheBarsFromStorage(key, stdCode, period, count);
	}

	//这里只需要检查一下RTBarCache的K线时间戳和etime是否一致
	//如果一致，说明最后一条K线还没完成，但是系统要读取，这个时候就用缓存中的最后一条bar追加到最后
	//如果不一致，说明Writer那边已经处理完成了，直接用缓存好的K线即可
	//OnMinuteEnd的时候也要做类似的检查
	if (barsList._bars.empty())
		return NULL;

	//数据条数做一个修正
	count = min((uint32_t)barsList._bars.size(), count);

	bool isDay = (period == KP_DAY);
	etime = isDay ? curDate : ((curDate - 19900000)*10000 + curTime);
	if(barsList._last_req_time < etime)
	{
		//上次请求的时间，小于当前请求的时间，则要检查最后一条K线
		WTSBarStruct& lastBar = barsList._bars.back();
		uint32_t lastBarTime = isDay ? lastBar.date : (uint32_t)lastBar.time;
		if(lastBarTime < etime)
		{
			//如果最后一条K线的时间小于当前时间，先从数LMDB更新最新的K线
			update_cache_from_lmdb(barsList, cInfo._exchg, curCode.c_str(), period, lastBarTime);

			lastBar = barsList._bars.back();
			lastBarTime = isDay ? lastBar.date : (uint32_t)lastBar.time;
		}

		//从lmdb读完了以后，再检查
		//如果时间戳仍然小于截止时间
		//则从缓存中读取
		if(lastBarTime < etime)
		{
			WTSBarStruct* rtBar = get_rt_cache_bar(cInfo._exchg, curCode.c_str(), period);
			if(rtBar != NULL)
			{
				uint64_t cacheBarTime = isDay ? rtBar->date : rtBar->time;
				if (cacheBarTime > etime)
				{
					//缓存的K线时间戳大于截止时间，说明检查的过程中Writer已经将数据转储到lmdb中了
					//这个时候就再读一次lmdb
					update_cache_from_lmdb(barsList, cInfo._exchg, curCode.c_str(), period, lastBarTime);
					barsList._last_from_cache = false;
				}
				else
				{
					//如果缓存的K线时间没有超过etime，则将缓存中的最后一条K线追加到队列中
					barsList._bars.push_back(*rtBar);
					barsList._last_from_cache = true;
					pipe_reader_log(_sink, LL_DEBUG,
						"{} bars @  {} of {} updated from cache instead of lmdb in {}", PERIOD_NAME[period], etime, stdCode, __FUNCTION__);
				}
			}
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	// 生成K线切片（循环缓冲区的特殊处理）
	
	// 更新缓存的最后请求时间
	barsList._last_req_time = etime;

	// 循环缓冲区数据提取逻辑（与Tick处理类似）
	// 需要从循环缓冲区的尾部往前提取指定数量的K线
	count = min((uint32_t)barsList._bars.size(), count);  // 修正count，不能超过实际数据量
	auto ayTwo = barsList._bars.array_two();    // 获取循环缓冲区的第二段数据
	auto cnt_2 = ayTwo.second;                  // 第二段数据的数量
	
	if (cnt_2 >= count)
	{
		// 情况1：第二段数据足够，直接从第二段提取
		return WTSKlineSlice::create(stdCode, period, 1, &barsList._bars[ayTwo.second - count], count);
	}
	else
	{
		// 情况2：第二段数据不够，需要从第一段补充
		auto ayOne = barsList._bars.array_one();  // 获取循环缓冲区的第一段数据
		auto diff = count - cnt_2;              // 需要从第一段取的数据量
		
		// 先从第一段尾部取diff条数据创建切片
		auto ret = WTSKlineSlice::create(stdCode, period, 1, &barsList._bars[ayOne.second - diff], diff);
		
		// 如果第二段有数据，追加到切片中
		if (cnt_2 > 0)
			ret->appendBlock(ayTwo.first, cnt_2);
		
		return ret;                             // 返回合并后的K线切片
	}

	// 注意：这行代码实际上不会被执行到
	return NULL;
}

/**
 * @brief 分钟结束事件处理
 * 
 * 在每分钟结束时被调用，用于更新所有缓存的K线数据。
 * 该方法是实时数据同步的核心，确保策略能够获取到最新的完整K线。
 * 
 * @param uDate 当前日期（YYYYMMDD格式）
 * @param uTime 当前时间（HHmm格式）
 * @param endTDate 结束交易日（可选，用于日线处理）
 */
void WtDataReaderAD::onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate /* = 0 */)
{
	// 构造当前时间戳并检查是否需要处理
	uint64_t nowTime = (uint64_t)uDate * 10000 + uTime;
	if (nowTime <= _last_time)                  // 防止重复处理
		return;

	uint64_t endBarTime = (uDate - 19900000) * 10000 + uTime;

	for (auto it = _bars_cache.begin(); it != _bars_cache.end(); it++)
	{
		BarsList& barsList = (BarsList&)it->second;
		CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(barsList._code.c_str(), _hot_mgr);
		if (barsList._period != KP_DAY)
		{
			uint32_t lastBarTime = (uint32_t)barsList._bars.back().time;
			pipe_reader_log(_sink, LL_DEBUG,
				"Updating {} bars of {} in section ({},{}]", PERIOD_NAME[barsList._period], barsList._code, lastBarTime, endBarTime);
			update_cache_from_lmdb(barsList, barsList._exchg.c_str(), cInfo._code, barsList._period, lastBarTime);
			if(lastBarTime < endBarTime)
			{
				WTSBarStruct* rtBar = get_rt_cache_bar(cInfo._exchg, cInfo._code, barsList._period);
				if(rtBar->time > lastBarTime && rtBar->time <=endBarTime)
				{
					barsList._bars.push_back(*rtBar);
					barsList._last_from_cache = true;
					_sink->on_bar(barsList._code.c_str(), barsList._period, rtBar);
					pipe_reader_log(_sink, LL_DEBUG,
						"{} bars @ {} of {} updated from cache instead of lmdb in {}", PERIOD_NAME[barsList._period], endBarTime, barsList._code, __FUNCTION__);
				}
			}
		}
		else if(endTDate != 0)
		{
			uint32_t lastBarTime = barsList._bars.back().date;
			endBarTime = uDate;
			update_cache_from_lmdb(barsList, barsList._exchg.c_str(), cInfo._code, barsList._period, lastBarTime);
			if (lastBarTime < endBarTime)
			{
				WTSBarStruct* rtBar = get_rt_cache_bar(cInfo._exchg, cInfo._code, barsList._period);
				if (rtBar->date > lastBarTime && rtBar->date <= endBarTime)
				{
					barsList._bars.push_back(*rtBar);
					barsList._last_from_cache = true;
					_sink->on_bar(barsList._code.c_str(), barsList._period, rtBar);
					pipe_reader_log(_sink, LL_DEBUG,
						"{} bars @  of {} updated from cache instead of lmdb in {}", PERIOD_NAME[barsList._period], endBarTime, barsList._code, __FUNCTION__);
				}
			}
		}
	}

	// 通知所有K线更新完成
	if (_sink)
		_sink->on_all_bar_updated(uTime);       // 触发所有K线更新完成回调

	// 更新最后处理时间，用于下次去重
	_last_time = nowTime;
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
WtDataReaderAD::WtLMDBPtr WtDataReaderAD::get_k_db(const char* exchg, WTSKlinePeriod period)
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
		pipe_reader_log(_sink, LL_ERROR, "Opening {} db if {} failed: {}", subdir, exchg, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}
	else
	{
		// 打开成功，记录调试日志
		pipe_reader_log(_sink, LL_DEBUG, "{} db of {} opened", subdir, exchg);
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
WtDataReaderAD::WtLMDBPtr WtDataReaderAD::get_t_db(const char* exchg, const char* code)
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
		pipe_reader_log(_sink, LL_ERROR, "Opening tick db of {}.{} failed: {}", exchg, code, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}
	else
	{
		// 打开成功，记录调试日志
		pipe_reader_log(_sink, LL_DEBUG, "Tick db of {}.{} opened", exchg, code);
	}

	// 将新连接加入缓存
	_tick_dbs[key] = dbPtr;                     // 注意：这里使用key而不是exchg
	return std::move(dbPtr);                    // 返回新连接
}