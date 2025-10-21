/*!
 * \file WtDataReader.cpp
 * \project WonderTrader
 * 
 * \brief WonderTrader数据读取器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtDataReader类的所有功能，是WonderTrader框架中用于数据读取的核心组件。
 * 该文件提供了从WonderTrader数据存储格式中读取各种数据的功能，支持实时数据和历史数据的读取，
 * 包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型，主要用于策略回测和实时行情数据访问。
 * 
 * 核心实现机制：
 * 
 * 1. 数据文件管理（Data File Management）：
 *    - 使用内存映射文件技术提高读取性能
 *    - 支持数据文件的动态加载和卸载
 *    - 提供数据文件的缓存管理
 * 
 * 2. 数据格式处理（Data Format Processing）：
 *    - 支持多种数据格式的自动识别
 *    - 提供数据压缩和解压缩功能
 *    - 支持版本兼容性处理
 * 
 * 3. 数据切片功能（Data Slicing）：
 *    - 支持按时间范围读取数据
 *    - 支持按数量读取数据
 *    - 支持数据切片和分页
 * 
 * 主要功能模块：
 * 
 * 1. 实时数据读取：
 *    - 实时K线数据读取
 *    - 实时Tick数据读取
 *    - 实时逐笔数据读取
 * 
 * 2. 历史数据读取：
 *    - 历史K线数据读取
 *    - 历史Tick数据读取
 *    - 历史逐笔数据读取
 * 
 * 3. 数据处理功能：
 *    - 数据格式转换
 *    - 复权数据处理
 *    - 数据缓存管理
 * 
 * 技术特点：
 * - 使用内存映射文件技术提高读取性能
 * - 支持多种数据格式的自动识别和转换
 * - 提供线程安全的数据读取
 * - 支持大数据量的流式读取和缓存
 * 
 * 使用场景：
 * - 策略回测数据加载
 * - 实时行情数据访问
 * - 历史数据分析和研究
 * - 数据格式转换和迁移
 * 
 * 注意事项：
 * - 需要正确配置数据存储路径
 * - 支持的数据格式需要与存储格式匹配
 * - 大数据量读取时需要注意内存使用
 * - 实时数据读取需要考虑数据更新频率
 */

#include "WtDataReader.h"                                         // 数据读取器头文件

#include "../Includes/WTSVariant.hpp"                           // 变体数据类型
#include "../Share/TimeUtils.hpp"                                // 时间工具函数
#include "../Share/CodeHelper.hpp"                               // 代码辅助工具
#include "../Share/StdUtils.hpp"                                 // 标准工具函数

#include "../Includes/WTSContractInfo.hpp"                       // 合约信息类
#include "../Includes/IBaseDataMgr.h"                             // 基础数据管理器接口
#include "../Includes/IHotMgr.h"                                 // 热力管理器接口
#include "../Includes/WTSDataDef.hpp"                            // 数据定义

#include "../WTSUtils/WTSCmpHelper.hpp"                          // 数据压缩辅助工具
#include "../WTSUtils/WTSCfgLoader.h"                            // 配置加载器

#include <rapidjson/document.h>                                  // RapidJSON文档类
namespace rj = rapidjson;                                        // RapidJSON命名空间别名

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                                     // 格式化库

/*!
 * \brief 数据读取器日志记录模板函数
 * \tparam Args 可变参数类型
 * \param sink 日志回调接口
 * \param ll 日志级别
 * \param format 格式化字符串
 * \param args 格式化参数
 * 
 * 该函数用于数据读取器的日志记录，支持格式化字符串和可变参数。
 * 使用fmtutil::format进行字符串格式化，提供高效的日志输出。
 */
template<typename... Args>
inline void pipe_reader_log(IDataReaderSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                                             // 如果日志回调接口为空
		return;                                                   // 直接返回

	// 例子：pipe_reader_log(sink, LL_INFO, "WtDataReader initialized, rt dir is {}, hist dir is {}, adjust_flag is {}", _rt_dir, _his_dir, _adjust_flag);
	const char* buffer = fmtutil::format(format, args...);        // 格式化字符串

	sink->reader_log(ll, buffer);                                // 调用日志回调接口输出日志
}

extern "C"
{
	/*!
	 * \brief 创建数据读取器实例
	 * \return 数据读取器接口指针
	 * 
	 * 该函数用于创建WtDataReader实例，通过C接口导出供外部调用。
	 * 返回的指针需要调用deleteDataReader函数释放内存。
	 */
	EXPORT_FLAG IDataReader* createDataReader()
	{
		IDataReader* ret = new WtDataReader();                    // 创建WtDataReader实例
		return ret;                                               // 返回数据读取器接口指针
	}

	/*!
	 * \brief 删除数据读取器实例
	 * \param reader 数据读取器接口指针
	 * 
	 * 该函数用于释放数据读取器实例占用的内存，通过C接口导出供外部调用。
	 * 传入的指针必须是通过createDataReader函数创建的。
	 */
	EXPORT_FLAG void deleteDataReader(IDataReader* reader)
	{
		if (reader != NULL)                                       // 如果指针不为空
			delete reader;                                        // 删除数据读取器实例
	}
};

/*!
 * \brief 处理数据块数据
 * \param content 数据内容字符串引用
 * \param isBar 是否为K线数据（true为K线，false为Tick）
 * \param bKeepHead 是否保留数据块头部（默认为true）
 * \return 处理是否成功
 * 
 * 该函数用于处理WonderTrader数据块，支持数据解压缩、版本转换等功能。
 * 主要处理流程：
 * 1. 检测数据块版本和压缩状态
 * 2. 如果需要，进行数据解压缩
 * 3. 如果是旧版本，进行数据结构转换
 * 4. 根据参数决定是否保留数据块头部
 */
bool proc_block_data(std::string& content, bool isBar, bool bKeepHead /* = true */)
{
	BlockHeader* header = (BlockHeader*)content.data();           // 获取数据块头部指针

	bool bCmped = header->is_compressed();                         // 检测是否为压缩格式
	bool bOldVer = header->is_old_version();                      // 检测是否为旧版本格式

	//如果既没有压缩，也不是老版本结构体，则直接返回
	if (!bCmped && !bOldVer)                                      // 如果数据未压缩且为新版本
	{
		if (!bKeepHead)                                           // 如果不保留头部
			content.erase(0, BLOCK_HEADER_SIZE);                  // 删除数据块头部
		return true;                                              // 直接返回成功
	}

	std::string buffer;                                           // 数据缓冲区
	if (bCmped)                                                   // 如果数据已压缩
	{
		BlockHeaderV2* blkV2 = (BlockHeaderV2*)content.c_str();   // 获取V2版本头部指针

		if (content.size() != (sizeof(BlockHeaderV2) + blkV2->_size))  // 校验数据大小
		{
			return false;                                          // 数据大小不匹配，返回失败
		}

		//将文件头后面的数据进行解压
		buffer = WTSCmpHelper::uncompress_data(content.data() + BLOCK_HEADERV2_SIZE, (std::size_t)blkV2->_size);  // 解压缩数据
	}
	else                                                          // 如果数据未压缩
	{
		if (!bOldVer)                                             // 如果不是旧版本
		{
			//如果不是老版本，直接返回
			if (!bKeepHead)                                       // 如果不保留头部
				content.erase(0, BLOCK_HEADER_SIZE);              // 删除数据块头部
			return true;                                          // 直接返回成功
		}
		else                                                      // 如果是旧版本
		{
			buffer.append(content.data() + BLOCK_HEADER_SIZE, content.size() - BLOCK_HEADER_SIZE);  // 提取数据部分
		}
	}

	if (bOldVer)                                                  // 如果是旧版本格式
	{
		if (isBar)                                                // 如果是K线数据
		{
			std::string bufV2;                                    // V2版本缓冲区
			uint32_t barcnt = buffer.size() / sizeof(WTSBarStructOld);  // 计算K线数量
			bufV2.resize(barcnt * sizeof(WTSBarStruct));          // 调整V2缓冲区大小
			WTSBarStruct* newBar = (WTSBarStruct*)bufV2.data();  // 获取新版本K线数据指针
			WTSBarStructOld* oldBar = (WTSBarStructOld*)buffer.data();  // 获取旧版本K线数据指针
			for (uint32_t idx = 0; idx < barcnt; idx++)           // 遍历所有K线数据
			{
				newBar[idx] = oldBar[idx];                        // 复制K线数据
			}
			buffer.swap(bufV2);                                   // 交换缓冲区
		}
		else                                                      // 如果是Tick数据
		{
			uint32_t tick_cnt = buffer.size() / sizeof(WTSTickStructOld);  // 计算Tick数量
			std::string bufv2;                                    // V2版本缓冲区
			bufv2.resize(sizeof(WTSTickStruct)*tick_cnt);         // 调整V2缓冲区大小
			WTSTickStruct* newTick = (WTSTickStruct*)bufv2.data();  // 获取新版本Tick数据指针
			WTSTickStructOld* oldTick = (WTSTickStructOld*)buffer.data();  // 获取旧版本Tick数据指针
			for (uint32_t i = 0; i < tick_cnt; i++)              // 遍历所有Tick数据
			{
				newTick[i] = oldTick[i];                          // 复制Tick数据
			}
			buffer.swap(bufv2);                                   // 交换缓冲区
		}
	}

	if (bKeepHead)                                                // 如果需要保留头部
	{
		content.resize(BLOCK_HEADER_SIZE);                        // 调整内容大小为头部大小
		content.append(buffer);                                   // 追加处理后的数据
		header = (BlockHeader*)content.data();                    // 重新获取头部指针
		header->_version = BLOCK_VERSION_RAW_V2;                  // 设置版本为V2
	}
	else                                                          // 如果不需要保留头部
	{
		content.swap(buffer);                                     // 交换内容为处理后的数据
	}

	return true;                                                  // 返回处理成功
}


/*!
 * \brief WtDataReader构造函数
 * 
 * 初始化数据读取器的所有成员变量为默认值。
 * 包括时间戳、基础数据管理器、热力管理器等。
 */
WtDataReader::WtDataReader()
	: _last_time(0)                                                // 初始化最后时间戳为0
	, _base_data_mgr(NULL)                                        // 初始化基础数据管理器为空
	, _hot_mgr(NULL)                                              // 初始化热力管理器为空
{
}

/*!
 * \brief WtDataReader析构函数
 * 
 * 清理数据读取器占用的资源。
 * 当前版本无需特殊清理操作。
 */
WtDataReader::~WtDataReader()
{
}

/*!
 * \brief 初始化数据读取器
 * \param cfg 配置参数
 * \param sink 数据读取回调接口
 * \param loader 历史数据加载器（可选）
 * 
 * 该函数用于初始化数据读取器，包括：
 * 1. 设置基础数据管理器和热力管理器
 * 2. 配置实时数据和历史数据存储路径
 * 3. 设置复权标记
 * 4. 加载股票复权因子
 */
void WtDataReader::init(WTSVariant* cfg, IDataReaderSink* sink, IHisDataLoader* loader /* = NULL */)
{
	IDataReader::init(cfg, sink, loader);                         // 调用基类初始化函数

	_base_data_mgr = sink->get_basedata_mgr();                    // 获取基础数据管理器
	_hot_mgr = sink->get_hot_mgr();                               // 获取热力管理器

	if (cfg == NULL)                                              // 如果配置为空
		return ;                                                  // 直接返回

	std::string root_dir = cfg->getCString("path");               // 获取根目录路径
	root_dir = StrUtil::standardisePath(root_dir);               // 标准化路径格式

	_rt_dir = root_dir + "rt/";                                   // 设置实时数据目录

	_his_dir = cfg->getCString("his_path");                      // 获取历史数据目录
	if(!_his_dir.empty())                                         // 如果历史数据目录不为空
		_his_dir = StrUtil::standardisePath(_his_dir);            // 标准化历史数据目录路径
	else                                                          // 如果历史数据目录为空
		_his_dir = root_dir + "his/";                             // 使用默认历史数据目录

	_adjust_flag = cfg->getUInt32("adjust_flag");                // 获取复权标记

	pipe_reader_log(sink, LL_INFO, "WtDataReader initialized, rt dir is {}, hist dir is {}, adjust_flag is {}", _rt_dir, _his_dir, _adjust_flag);  // 记录初始化日志

	/*
	 *	By Wesley @ 2021.12.20
	 *	先从extloader加载除权因子
	 *	如果加载失败，并且配置了除权因子文件，再加载除权因子文件
	 */
	bool bLoaded = loadStkAdjFactorsFromLoader();                // 尝试从加载器加载复权因子

	if (!bLoaded && cfg->has("adjfactor"))                       // 如果加载失败且配置了复权因子文件
		loadStkAdjFactorsFromFile(cfg->getCString("adjfactor"));  // 从文件加载复权因子
	else                                                          // 如果加载成功或未配置复权因子文件
		pipe_reader_log(sink, LL_INFO, "No adjusting factor file configured, loading skipped");  // 记录跳过加载的日志
}

/*!
 * \brief 从加载器加载股票复权因子
 * \return 是否加载成功
 * 
 * 该函数通过外部加载器加载所有股票的复权因子数据。
 * 加载的复权因子会按照日期排序，并添加默认的起始复权因子。
 */
bool WtDataReader::loadStkAdjFactorsFromLoader()
{
	if (NULL == _loader)                                          // 如果加载器为空
		return false;                                             // 返回加载失败

	bool ret = _loader->loadAllAdjFactors(&_adj_factors, [](void* obj, const char* stdCode, uint32_t* dates, double* factors, uint32_t count) {  // 调用加载器加载所有复权因子
		AdjFactorMap* fact_map = (AdjFactorMap*)obj;              // 获取复权因子映射表指针
		AdjFactorList& fctrLst = (*fact_map)[stdCode];            // 获取指定合约的复权因子列表

		for(uint32_t i = 0; i < count; i++)                      // 遍历所有复权因子
		{
			AdjFactor adjFact;                                    // 创建复权因子结构
			adjFact._date = dates[i];                             // 设置复权日期
			adjFact._factor = factors[i];                         // 设置复权因子值

			fctrLst.emplace_back(adjFact);                       // 添加到复权因子列表
		}

		//一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
		AdjFactor adjFact;                                        // 创建默认复权因子
		adjFact._date = 19900101;                                 // 设置默认日期为1990年1月1日
		adjFact._factor = 1;                                      // 设置默认复权因子为1
		fctrLst.emplace_back(adjFact);                            // 添加默认复权因子

		std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {  // 按日期排序复权因子列表
			return left._date < right._date;                      // 按日期升序排列
		});
	});

	if (ret && _sink) pipe_reader_log(_sink,LL_INFO, "Adjusting factors of {} contracts loaded via extended loader", _adj_factors.size());  // 记录加载成功日志
	return ret;                                                   // 返回加载结果
}

/*!
 * \brief 从文件加载股票复权因子
 * \param adjfile 复权因子文件路径
 * \return 是否加载成功
 * 
 * 该函数从JSON格式的复权因子文件中加载股票复权因子数据。
 * 文件格式：{交易所: {合约代码: [{date: 日期, factor: 复权因子}]}}
 * 支持自动识别合约代码格式，并添加默认的起始复权因子。
 */
bool WtDataReader::loadStkAdjFactorsFromFile(const char* adjfile)
{
	if(!StdFile::exists(adjfile))                                 // 如果复权因子文件不存在
	{
		pipe_reader_log(_sink,LL_ERROR, "Adjusting factors file {} not exists", adjfile);  // 记录文件不存在错误
		return false;                                             // 返回加载失败
	}

	WTSVariant* doc = WTSCfgLoader::load_from_file(adjfile);     // 从文件加载JSON配置
	if(doc == NULL)                                               // 如果加载失败
	{
		pipe_reader_log(_sink, LL_ERROR, "Loading adjusting factors file {} failed", adjfile);  // 记录加载失败错误
		return false;                                             // 返回加载失败
	}

	uint32_t stk_cnt = 0;                                         // 股票计数器
	uint32_t fct_cnt = 0;                                         // 复权因子计数器
	for (const std::string& exchg : doc->memberNames())           // 遍历所有交易所
	{
		WTSVariant* itemExchg = doc->get(exchg);                  // 获取交易所数据
		for(const std::string& code : itemExchg->memberNames())   // 遍历交易所下的所有合约代码
		{
			WTSVariant* ayFacts = itemExchg->get(code);           // 获取合约的复权因子数组
			if(!ayFacts->isArray() )                               // 如果不是数组格式
				continue;                                          // 跳过该合约

			/*
			 *	By Wesley @ 2021.12.21
			 *	先检查code的格式是不是包含PID，如STK.600000
			 *	如果包含PID，则直接格式化，如果不包含，则强制为STK
			 */
			bool bHasPID = (code.find('.') != std::string::npos);  // 检查是否包含产品ID

			std::string key;                                       // 标准合约代码
			if (bHasPID)                                           // 如果包含产品ID
				key = fmt::format("{}.{}", exchg, code);          // 直接格式化
			else                                                   // 如果不包含产品ID
				key = fmt::format("{}.STK.{}", exchg, code);      // 强制添加STK产品ID

			stk_cnt++;                                             // 增加股票计数

			AdjFactorList& fctrLst = _adj_factors[key];           // 获取该合约的复权因子列表
			for (uint32_t i = 0; i < ayFacts->size(); i++)        // 遍历复权因子数组
			{
				WTSVariant* fItem = ayFacts->get(i);              // 获取单个复权因子项
				AdjFactor adjFact;                                // 创建复权因子结构
				adjFact._date = fItem->getUInt32("date");         // 获取复权日期
				adjFact._factor = fItem->getDouble("factor");     // 获取复权因子值

				fctrLst.emplace_back(adjFact);                   // 添加到复权因子列表
				fct_cnt++;                                        // 增加复权因子计数
			}

			//一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
			AdjFactor adjFact;                                    // 创建默认复权因子
			adjFact._date = 19900101;                             // 设置默认日期为1990年1月1日
			adjFact._factor = 1;                                  // 设置默认复权因子为1
			fctrLst.emplace_back(adjFact);                        // 添加默认复权因子

			std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {  // 按日期排序复权因子列表
				return left._date < right._date;                  // 按日期升序排列
			});
		}
	}

	pipe_reader_log(_sink,LL_INFO, "{} adjusting factors of {} tickers loaded", fct_cnt, stk_cnt);  // 记录加载成功日志
	doc->release();                                               // 释放JSON文档
	return true;                                                   // 返回加载成功
}

/*!
 * \brief 读取Tick数据切片
 * \param stdCode 标准合约代码
 * \param count 数据数量
 * \param etime 结束时间（可选，默认为0）
 * \return Tick数据切片指针
 * 
 * 该函数用于读取指定合约的Tick数据切片，支持实时数据和历史数据。
 * 主要功能：
 * 1. 解析合约代码信息
 * 2. 计算交易日期和时间
 * 3. 根据是否为当日数据选择实时或历史数据源
 * 4. 使用二分查找定位数据位置
 * 5. 创建并返回Tick数据切片
 */
WTSTickSlice* WtDataReader::readTickSlice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息
	const char* stdPID = commInfo->getFullPid();                  // 获取标准产品ID

	uint32_t curDate, curTime, curSecs;                           // 当前日期、时间、秒数
	if (etime == 0)                                              // 如果结束时间为0
	{
		curDate = _sink->get_date();                              // 获取当前日期
		curTime = _sink->get_min_time();                          // 获取当前分钟时间
		curSecs = _sink->get_secs();                              // 获取当前秒数

		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;  // 计算结束时间戳
	}
	else                                                          // 如果指定了结束时间
	{
		//20190807124533900
		curDate = (uint32_t)(etime / 1000000000);                 // 从时间戳提取日期
		curTime = (uint32_t)(etime % 1000000000) / 100000;        // 从时间戳提取分钟时间
		curSecs = (uint32_t)(etime % 100000);                     // 从时间戳提取秒数
	}

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false);  // 计算当前交易日期

	bool isToday = (endTDate == curTDate);                        // 判断是否为当日数据

	std::string curCode = cInfo._code;                            // 获取当前合约代码
	if (commInfo->isFuture())                                     // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                     // 获取规则标签
		if (strlen(ruleTag) > 0)                                  // 如果规则标签不为空
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate);  // 获取自定义原始代码
	}

	//比较时间的对象
	WTSTickStruct eTick;                                          // 用于比较的Tick结构
	eTick.action_date = curDate;                                   // 设置动作日期
	eTick.action_time = curTime * 100000 + curSecs;               // 设置动作时间

	if (isToday)                                                  // 如果是当日数据
	{
		TickBlockPair* tPair = getRTTickBlock(cInfo._exchg, curCode.c_str());  // 获取实时Tick数据块
		if (tPair == NULL)                                        // 如果数据块为空
			return NULL;                                           // 返回空指针

		RTTickBlock* tBlock = tPair->_block;                     // 获取Tick数据块指针

		WTSTickStruct* pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + (tBlock->_size - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b){  // 使用二分查找定位Tick位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pTick - tBlock->_ticks;                   // 计算结束索引

		//如果光标定位的tick时间比目标时间打, 则全部回退一个
		if (pTick->action_date > eTick.action_date || pTick->action_time>eTick.action_time)  // 如果定位的Tick时间大于目标时间
		{
			pTick--;                                               // 回退一个Tick
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks + sIdx, cnt);  // 创建Tick数据切片
		return slice;                                              // 返回数据切片
	}
	else                                                          // 如果是历史数据
	{
		thread_local static char key[64] = { 0 };                 // 线程局部静态键值
		fmtutil::format_to(key, "{}-{}", stdCode, endTDate);       // 格式化键值

		auto it = _his_tick_map.find(key);                        // 查找历史Tick数据块
		if(it == _his_tick_map.end())                             // 如果未找到
		{
			std::stringstream ss;     
			// 构建历史tick数据文件路径 {_his_dir}ticks/{cInfo._exchg}/{endTDate}/{curCode}.dsb
			ss << _his_dir << "ticks/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb";  // 构建文件路径
			std::string filename = ss.str();                      // 获取文件名
			if (!StdFile::exists(filename.c_str()))               // 如果文件不存在
				return NULL;                                       // 返回空指针

			HisTBlockPair& tBlkPair = _his_tick_map[key];         // 获取历史Tick数据块对
			StdFile::read_file_content(filename.c_str(), tBlkPair._buffer);  // 读取文件内容
			if (tBlkPair._buffer.size() < sizeof(HisTickBlock))    // 如果文件大小不足
			{
				pipe_reader_log(_sink,LL_ERROR, "Sizechecking of his tick data file {} failed", filename);  // 记录错误日志
				tBlkPair._buffer.clear();                          // 清空缓冲区
				return NULL;                                       // 返回空指针
			}

			proc_block_data(tBlkPair._buffer, false, true);       // 处理数据块
			tBlkPair._block = (HisTickBlock*)tBlkPair._buffer.c_str();  // 设置数据块指针
		}
		
		HisTBlockPair& tBlkPair = _his_tick_map[key];             // 获取历史Tick数据块对
		if (tBlkPair._block == NULL)                               // 如果数据块为空
			return NULL;                                           // 返回空指针

		HisTickBlock* tBlock = tBlkPair._block;                   // 获取历史Tick数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisTickBlock)) / sizeof(WTSTickStruct);  // 计算Tick数量
		if (tcnt <= 0)                                             // 如果Tick数量为0
			return NULL;                                           // 返回空指针

		WTSTickStruct* pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + (tcnt - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b){  // 使用二分查找定位Tick位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pTick - tBlock->_ticks;                   // 计算结束索引
		if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time)  // 如果定位的Tick时间大于等于目标时间
		{
			pTick--;                                               // 回退一个Tick
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks + sIdx, cnt);  // 创建Tick数据切片
		return slice;                                              // 返回数据切片
	}
}

/*!
 * \brief 读取委托队列数据切片
 * \param stdCode 标准合约代码
 * \param count 数据数量
 * \param etime 结束时间（可选，默认为0）
 * \return 委托队列数据切片指针
 * 
 * 该函数用于读取指定合约的委托队列数据切片，支持实时数据和历史数据。
 * 主要功能：
 * 1. 解析合约代码信息
 * 2. 计算交易日期和时间
 * 3. 根据是否为当日数据选择实时或历史数据源
 * 4. 使用二分查找定位数据位置
 * 5. 创建并返回委托队列数据切片
 */
WTSOrdQueSlice* WtDataReader::readOrdQueSlice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息
	const char* stdPID = commInfo->getFullPid();                  // 获取标准产品ID

	uint32_t curDate, curTime, curSecs;                           // 当前日期、时间、秒数
	if (etime == 0)                                              // 如果结束时间为0
	{
		curDate = _sink->get_date();                              // 获取当前日期
		curTime = _sink->get_min_time();                          // 获取当前分钟时间
		curSecs = _sink->get_secs();                              // 获取当前秒数

		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;  // 计算结束时间戳
	}
	else                                                          // 如果指定了结束时间
	{
		//20190807124533900
		curDate = (uint32_t)(etime / 1000000000);                 // 从时间戳提取日期
		curTime = (uint32_t)(etime % 1000000000) / 100000;        // 从时间戳提取分钟时间
		curSecs = (uint32_t)(etime % 100000);                     // 从时间戳提取秒数
	}

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false);  // 计算当前交易日期

	bool isToday = (endTDate == curTDate);                        // 判断是否为当日数据

	std::string curCode = cInfo._code;                            // 获取当前合约代码
	if (commInfo->isFuture())                                     // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                     // 获取规则标签
		if (strlen(ruleTag) > 0)                                  // 如果规则标签不为空
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate);  // 获取自定义原始代码
	}

	//比较时间的对象
	WTSOrdQueStruct eTick;                                        // 用于比较的委托队列结构
	eTick.action_date = curDate;                                   // 设置动作日期
	eTick.action_time = curTime * 100000 + curSecs;               // 设置动作时间

	if (isToday)                                                  // 如果是当日数据
	{
		OrdQueBlockPair* tPair = getRTOrdQueBlock(cInfo._exchg, curCode.c_str());  // 获取实时委托队列数据块
		if (tPair == NULL)                                        // 如果数据块为空
			return NULL;                                           // 返回空指针

		RTOrdQueBlock* rtBlock = tPair->_block;                   // 获取委托队列数据块指针

		WTSOrdQueStruct* pItem = std::lower_bound(rtBlock->_queues, rtBlock->_queues + (rtBlock->_size - 1), eTick, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {  // 使用二分查找定位委托队列位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pItem - rtBlock->_queues;                 // 计算结束索引

		//如果光标定位的tick时间比目标时间打, 则全部回退一个
		if (pItem->action_date > eTick.action_date || pItem->action_time > eTick.action_time)  // 如果定位的委托队列时间大于目标时间
		{
			pItem--;                                               // 回退一个委托队列
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSOrdQueSlice* slice = WTSOrdQueSlice::create(stdCode, rtBlock->_queues + sIdx, cnt);  // 创建委托队列数据切片
		return slice;                                              // 返回数据切片
	}
	else                                                          // 如果是历史数据
	{
		thread_local static char key[64] = { 0 };                 // 线程局部静态键值
		fmtutil::format_to(key, "{}-{}", stdCode, endTDate);       // 格式化键值

		auto it = _his_ordque_map.find(key);                      // 查找历史委托队列数据块
		if (it == _his_ordque_map.end())                           // 如果未找到
		{
			std::stringstream ss;                                 // 字符串流
			ss << _his_dir << "queue/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb";  // 构建文件路径
			std::string filename = ss.str();                      // 获取文件名
			if (!StdFile::exists(filename.c_str()))               // 如果文件不存在
				return NULL;                                       // 返回空指针

			HisOrdQueBlockPair& hisBlkPair = _his_ordque_map[key]; // 获取历史委托队列数据块对
			StdFile::read_file_content(filename.c_str(), hisBlkPair._buffer);  // 读取文件内容
			if (hisBlkPair._buffer.size() < sizeof(HisOrdQueBlockV2))  // 如果文件大小不足
			{
				pipe_reader_log(_sink,LL_ERROR, "历史委托队列数据文件{}大小校验失败", filename);  // 记录错误日志
				hisBlkPair._buffer.clear();                        // 清空缓冲区
				return NULL;                                       // 返回空指针
			}

			HisOrdQueBlockV2* tBlockV2 = (HisOrdQueBlockV2*)hisBlkPair._buffer.c_str();  // 获取V2版本数据块指针

			if (hisBlkPair._buffer.size() != (sizeof(HisOrdQueBlockV2) + tBlockV2->_size))  // 如果文件大小不匹配
			{
				pipe_reader_log(_sink,LL_ERROR, "历史委托队列数据文件{}大小校验失败", filename);  // 记录错误日志
				return NULL;                                       // 返回空指针
			}

			//需要解压
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (std::size_t)tBlockV2->_size);  // 解压缩数据

			//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			hisBlkPair._buffer.resize(sizeof(HisOrdQueBlock));     // 调整缓冲区大小为V1版本头部大小
			hisBlkPair._buffer.append(buf);                       // 追加解压缩后的数据
			tBlockV2->_version = BLOCK_VERSION_RAW_V2;            // 设置版本为V2

			hisBlkPair._block = (HisOrdQueBlock*)hisBlkPair._buffer.c_str();  // 设置数据块指针
		}

		HisOrdQueBlockPair& tBlkPair = _his_ordque_map[key];      // 获取历史委托队列数据块对
		if (tBlkPair._block == NULL)                               // 如果数据块为空
			return NULL;                                           // 返回空指针

		HisOrdQueBlock* tBlock = tBlkPair._block;                 // 获取历史委托队列数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisOrdQueBlock)) / sizeof(WTSOrdQueStruct);  // 计算委托队列数量
		if (tcnt <= 0)                                             // 如果委托队列数量为0
			return NULL;                                           // 返回空指针

		WTSOrdQueStruct* pTick = std::lower_bound(tBlock->_items, tBlock->_items + (tcnt - 1), eTick, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {  // 使用二分查找定位委托队列位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pTick - tBlock->_items;                   // 计算结束索引
		if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time)  // 如果定位的委托队列时间大于等于目标时间
		{
			pTick--;                                               // 回退一个委托队列
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSOrdQueSlice* slice = WTSOrdQueSlice::create(stdCode, tBlock->_items + sIdx, cnt);  // 创建委托队列数据切片
		return slice;                                              // 返回数据切片
	}
}

/*!
 * \brief 读取逐笔委托数据切片
 * \param stdCode 标准合约代码
 * \param count 数据数量
 * \param etime 结束时间（可选，默认为0）
 * \return 逐笔委托数据切片指针
 * 
 * 该函数用于读取指定合约的逐笔委托数据切片，支持实时数据和历史数据。
 * 主要功能：
 * 1. 解析合约代码信息
 * 2. 计算交易日期和时间
 * 3. 根据是否为当日数据选择实时或历史数据源
 * 4. 使用二分查找定位数据位置
 * 5. 创建并返回逐笔委托数据切片
 */
WTSOrdDtlSlice* WtDataReader::readOrdDtlSlice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息
	const char* stdPID = commInfo->getFullPid();                  // 获取标准产品ID

	uint32_t curDate, curTime, curSecs;                           // 当前日期、时间、秒数
	if (etime == 0)                                              // 如果结束时间为0
	{
		curDate = _sink->get_date();                              // 获取当前日期
		curTime = _sink->get_min_time();                          // 获取当前分钟时间
		curSecs = _sink->get_secs();                              // 获取当前秒数

		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;  // 计算结束时间戳
	}
	else                                                          // 如果指定了结束时间
	{
		//20190807124533900
		curDate = (uint32_t)(etime / 1000000000);                 // 从时间戳提取日期
		curTime = (uint32_t)(etime % 1000000000) / 100000;        // 从时间戳提取分钟时间
		curSecs = (uint32_t)(etime % 100000);                     // 从时间戳提取秒数
	}

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false);  // 计算当前交易日期

	bool isToday = (endTDate == curTDate);                        // 判断是否为当日数据

	std::string curCode = cInfo._code;                            // 获取当前合约代码
	if (commInfo->isFuture())                                     // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                     // 获取规则标签
		if (strlen(ruleTag) > 0)                                  // 如果规则标签不为空
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate);  // 获取自定义原始代码
	}

	//比较时间的对象
	WTSOrdDtlStruct eTick;                                        // 用于比较的逐笔委托结构
	eTick.action_date = curDate;                                   // 设置动作日期
	eTick.action_time = curTime * 100000 + curSecs;               // 设置动作时间

	if (isToday)                                                  // 如果是当日数据
	{
		OrdDtlBlockPair* tPair = getRTOrdDtlBlock(cInfo._exchg, curCode.c_str());  // 获取实时逐笔委托数据块
		if (tPair == NULL)                                        // 如果数据块为空
			return NULL;                                           // 返回空指针

		RTOrdDtlBlock* rtBlock = tPair->_block;                   // 获取逐笔委托数据块指针

		WTSOrdDtlStruct* pItem = std::lower_bound(rtBlock->_details, rtBlock->_details + (rtBlock->_size - 1), eTick, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {  // 使用二分查找定位逐笔委托位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pItem - rtBlock->_details;                 // 计算结束索引

		//如果光标定位的tick时间比目标时间打, 则全部回退一个
		if (pItem->action_date > eTick.action_date || pItem->action_time > eTick.action_time)  // 如果定位的逐笔委托时间大于目标时间
		{
			pItem--;                                               // 回退一个逐笔委托
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSOrdDtlSlice* slice = WTSOrdDtlSlice::create(stdCode, rtBlock->_details + sIdx, cnt);  // 创建逐笔委托数据切片
		return slice;                                              // 返回数据切片
	}
	else                                                          // 如果是历史数据
	{
		thread_local static char key[64] = { 0 };                 // 线程局部静态键值
		fmtutil::format_to(key, "{}-{}", stdCode, endTDate);       // 格式化键值

		auto it = _his_ordque_map.find(key);                      // 查找历史逐笔委托数据块
		if (it == _his_ordque_map.end())                           // 如果未找到
		{
			std::stringstream ss;                                 // 字符串流
			ss << _his_dir << "orders/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb";  // 构建文件路径
			std::string filename = ss.str();                      // 获取文件名
			if (!StdFile::exists(filename.c_str()))               // 如果文件不存在
				return NULL;                                       // 返回空指针

			HisOrdDtlBlockPair& hisBlkPair = _his_orddtl_map[key]; // 获取历史逐笔委托数据块对
			StdFile::read_file_content(filename.c_str(), hisBlkPair._buffer);  // 读取文件内容
			if (hisBlkPair._buffer.size() < sizeof(HisOrdDtlBlockV2))  // 如果文件大小不足
			{
				pipe_reader_log(_sink,LL_ERROR, "历史逐笔委托数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
				hisBlkPair._buffer.clear();                        // 清空缓冲区
				return NULL;                                       // 返回空指针
			}

			HisOrdDtlBlockV2* tBlockV2 = (HisOrdDtlBlockV2*)hisBlkPair._buffer.c_str();  // 获取V2版本数据块指针

			if (hisBlkPair._buffer.size() != (sizeof(HisOrdDtlBlockV2) + tBlockV2->_size))  // 如果文件大小不匹配
			{
				pipe_reader_log(_sink,LL_ERROR, "历史逐笔委托数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
				return NULL;                                       // 返回空指针
			}

			//需要解压
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (std::size_t)tBlockV2->_size);  // 解压缩数据

			//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			hisBlkPair._buffer.resize(sizeof(HisOrdDtlBlock));     // 调整缓冲区大小为V1版本头部大小
			hisBlkPair._buffer.append(buf);                       // 追加解压缩后的数据
			tBlockV2->_version = BLOCK_VERSION_RAW_V2;            // 设置版本为V2

			hisBlkPair._block = (HisOrdDtlBlock*)hisBlkPair._buffer.c_str();  // 设置数据块指针
		}

		HisOrdDtlBlockPair& tBlkPair = _his_orddtl_map[key];      // 获取历史逐笔委托数据块对
		if (tBlkPair._block == NULL)                               // 如果数据块为空
			return NULL;                                           // 返回空指针

		HisOrdDtlBlock* tBlock = tBlkPair._block;                 // 获取历史逐笔委托数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisOrdDtlBlock)) / sizeof(WTSOrdDtlStruct);  // 计算逐笔委托数量
		if (tcnt <= 0)                                             // 如果逐笔委托数量为0
			return NULL;                                           // 返回空指针

		WTSOrdDtlStruct* pTick = std::lower_bound(tBlock->_items, tBlock->_items + (tcnt - 1), eTick, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {  // 使用二分查找定位逐笔委托位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pTick - tBlock->_items;                   // 计算结束索引
		if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time)  // 如果定位的逐笔委托时间大于等于目标时间
		{
			pTick--;                                               // 回退一个逐笔委托
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSOrdDtlSlice* slice = WTSOrdDtlSlice::create(stdCode, tBlock->_items + sIdx, cnt);  // 创建逐笔委托数据切片
		return slice;                                              // 返回数据切片
	}
}

/*!
 * \brief 读取逐笔成交数据切片
 * \param stdCode 标准合约代码
 * \param count 数据数量
 * \param etime 结束时间（可选，默认为0）
 * \return 逐笔成交数据切片指针
 * 
 * 该函数用于读取指定合约的逐笔成交数据切片，支持实时数据和历史数据。
 * 主要功能：
 * 1. 解析合约代码信息
 * 2. 计算交易日期和时间
 * 3. 根据是否为当日数据选择实时或历史数据源
 * 4. 使用二分查找定位数据位置
 * 5. 创建并返回逐笔成交数据切片
 */
WTSTransSlice* WtDataReader::readTransSlice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息
	const char* stdPID = commInfo->getFullPid();                  // 获取标准产品ID

	uint32_t curDate, curTime, curSecs;                           // 当前日期、时间、秒数
	if (etime == 0)                                              // 如果结束时间为0
	{
		curDate = _sink->get_date();                              // 获取当前日期
		curTime = _sink->get_min_time();                          // 获取当前分钟时间
		curSecs = _sink->get_secs();                              // 获取当前秒数

		etime = (uint64_t)curDate * 1000000000 + curTime * 100000 + curSecs;  // 计算结束时间戳
	}
	else                                                          // 如果指定了结束时间
	{
		//20190807124533900
		curDate = (uint32_t)(etime / 1000000000);                 // 从时间戳提取日期
		curTime = (uint32_t)(etime % 1000000000) / 100000;        // 从时间戳提取分钟时间
		curSecs = (uint32_t)(etime % 100000);                     // 从时间戳提取秒数
	}

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false);  // 计算当前交易日期

	bool isToday = (endTDate == curTDate);                        // 判断是否为当日数据

	std::string curCode = cInfo._code;                            // 获取当前合约代码
	if (commInfo->isFuture())                                     // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                     // 获取规则标签
		if (strlen(ruleTag) > 0)                                  // 如果规则标签不为空
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate);  // 获取自定义原始代码
	}

	//比较时间的对象
	WTSTransStruct eTick;                                         // 用于比较的逐笔成交结构
	eTick.action_date = curDate;                                   // 设置动作日期
	eTick.action_time = curTime * 100000 + curSecs;               // 设置动作时间

	if (isToday)                                                  // 如果是当日数据
	{
		TransBlockPair* tPair = getRTTransBlock(cInfo._exchg, curCode.c_str());  // 获取实时逐笔成交数据块
		if (tPair == NULL)                                        // 如果数据块为空
			return NULL;                                           // 返回空指针

		RTTransBlock* rtBlock = tPair->_block;                    // 获取逐笔成交数据块指针

		WTSTransStruct* pItem = std::lower_bound(rtBlock->_trans, rtBlock->_trans + (rtBlock->_size - 1), eTick, [](const WTSTransStruct& a, const WTSTransStruct& b) {  // 使用二分查找定位逐笔成交位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pItem - rtBlock->_trans;                  // 计算结束索引

		//如果光标定位的tick时间比目标时间打, 则全部回退一个
		if (pItem->action_date > eTick.action_date || pItem->action_time > eTick.action_time)  // 如果定位的逐笔成交时间大于目标时间
		{
			pItem--;                                               // 回退一个逐笔成交
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSTransSlice* slice = WTSTransSlice::create(stdCode, rtBlock->_trans + sIdx, cnt);  // 创建逐笔成交数据切片
		return slice;                                              // 返回数据切片
	}
	else                                                          // 如果是历史数据
	{
		thread_local static char key[64] = { 0 };                 // 线程局部静态键值
		fmtutil::format_to(key, "{}-{}", stdCode, endTDate);       // 格式化键值

		auto it = _his_ordque_map.find(key);                      // 查找历史逐笔成交数据块
		if (it == _his_ordque_map.end())                           // 如果未找到
		{
			std::stringstream ss;                                 // 字符串流
			ss << _his_dir << "trans/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb";  // 构建文件路径
			std::string filename = ss.str();                      // 获取文件名
			if (!StdFile::exists(filename.c_str()))               // 如果文件不存在
				return NULL;                                       // 返回空指针

			HisTransBlockPair& hisBlkPair = _his_trans_map[key];   // 获取历史逐笔成交数据块对
			StdFile::read_file_content(filename.c_str(), hisBlkPair._buffer);  // 读取文件内容
			if (hisBlkPair._buffer.size() < sizeof(HisTransBlockV2))  // 如果文件大小不足
			{
				pipe_reader_log(_sink,LL_ERROR, "历史逐笔成交数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
				hisBlkPair._buffer.clear();                        // 清空缓冲区
				return NULL;                                       // 返回空指针
			}

			HisTransBlockV2* tBlockV2 = (HisTransBlockV2*)hisBlkPair._buffer.c_str();  // 获取V2版本数据块指针

			if (hisBlkPair._buffer.size() != (sizeof(HisTransBlockV2) + tBlockV2->_size))  // 如果文件大小不匹配
			{
				pipe_reader_log(_sink,LL_ERROR, "历史逐笔成交数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
				return NULL;                                       // 返回空指针
			}

			//需要解压
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (std::size_t)tBlockV2->_size);  // 解压缩数据

			//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			hisBlkPair._buffer.resize(sizeof(HisTransBlock));      // 调整缓冲区大小为V1版本头部大小
			hisBlkPair._buffer.append(buf);                       // 追加解压缩后的数据
			tBlockV2->_version = BLOCK_VERSION_RAW_V2;            // 设置版本为V2

			hisBlkPair._block = (HisTransBlock*)hisBlkPair._buffer.c_str();  // 设置数据块指针
		}

		HisTransBlockPair& tBlkPair = _his_trans_map[key];         // 获取历史逐笔成交数据块对
		if (tBlkPair._block == NULL)                               // 如果数据块为空
			return NULL;                                           // 返回空指针

		HisTransBlock* tBlock = tBlkPair._block;                  // 获取历史逐笔成交数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisTransBlock)) / sizeof(WTSTransStruct);  // 计算逐笔成交数量
		if (tcnt <= 0)                                             // 如果逐笔成交数量为0
			return NULL;                                           // 返回空指针

		WTSTransStruct* pTick = std::lower_bound(tBlock->_items, tBlock->_items + (tcnt - 1), eTick, [](const WTSTransStruct& a, const WTSTransStruct& b) {  // 使用二分查找定位逐笔成交位置
			if (a.action_date != b.action_date)                   // 如果日期不同
				return a.action_date < b.action_date;             // 按日期比较
			else                                                   // 如果日期相同
				return a.action_time < b.action_time;             // 按时间比较
		});

		uint32_t eIdx = pTick - tBlock->_items;                   // 计算结束索引
		if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time)  // 如果定位的逐笔成交时间大于等于目标时间
		{
			pTick--;                                               // 回退一个逐笔成交
			eIdx--;                                                // 回退索引
		}

		uint32_t cnt = min(eIdx + 1, count);                      // 计算实际数量
		uint32_t sIdx = eIdx + 1 - cnt;                           // 计算起始索引
		WTSTransSlice* slice = WTSTransSlice::create(stdCode, tBlock->_items + sIdx, cnt);  // 创建逐笔成交数据切片
		return slice;                                              // 返回数据切片
	}
}


/*!
 * \brief 从外部加载器缓存最终K线数据
 * \param codeInfo 合约代码信息指针
 * \param key 缓存键值
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \return 是否缓存成功
 * 
 * 该函数通过外部加载器加载最终的K线数据（通常是复权后的数据）并缓存到内存中。
 * 主要功能：
 * 1. 检查外部加载器是否可用
 * 2. 设置K线列表的基本信息
 * 3. 调用外部加载器加载最终K线数据
 * 4. 将数据复制到缓存中
 * 5. 记录加载日志
 */
bool WtDataReader::cacheFinalBarsFromLoader(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period)
{
	if (NULL == _loader)                                          // 如果外部加载器为空
		return false;                                             // 返回失败

	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo;  // 获取合约代码信息指针

	BarsList& barList = _bars_cache[key];                        // 获取K线列表引用
	barList._code = stdCode;                                      // 设置合约代码
	barList._period = period;                                     // 设置K线周期
	barList._exchg = cInfo->_exchg;                              // 设置交易所代码

	std::string pname;                                            // 周期名称
	switch (period)                                               // 根据周期设置名称
	{
	case KP_Minute1: pname = "m1"; break;                        // 1分钟K线
	case KP_Minute5: pname = "m5"; break;                        // 5分钟K线
	case KP_DAY: pname = "d"; break;                            // 日K线
	default: pname = ""; break;                                  // 其他周期
	}

	pipe_reader_log(_sink,LL_INFO, "Reading final bars of {} via extended loader...", stdCode);  // 记录开始加载日志

	bool ret = _loader->loadFinalHisBars(&barList, stdCode, period, [](void* obj, WTSBarStruct* firstBar, uint32_t count) {  // 调用外部加载器加载最终K线数据
		BarsList* bars = (BarsList*)obj;                          // 获取K线列表指针
		bars->_factor = 1.0;                                      // 设置复权因子为1.0
		bars->_bars.resize(count);                                // 调整K线数组大小
		memcpy(bars->_bars.data(), firstBar, sizeof(WTSBarStruct)*count);  // 复制K线数据
	});

	if(ret)                                                       // 如果加载成功
		pipe_reader_log(_sink,LL_INFO, "{} items of back {} data of {} loaded via extended loader", barList._bars.size(), pname.c_str(), stdCode);  // 记录加载成功日志

	return ret;                                                   // 返回加载结果
}


/*!
 * \brief 缓存期货主力连续K线数据
 * \param codeInfo 合约代码信息指针
 * \param key 缓存键值
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \return 是否缓存成功
 * 
 * 该函数用于缓存期货主力连续K线数据，支持主力合约和分月合约的组合。
 * 主要功能：
 * 1. 计算当前交易日期和时间
 * 2. 加载主力合约的K线数据
 * 3. 获取主力合约的历史分段信息
 * 4. 按时间顺序组合各分月合约的K线数据
 * 5. 处理复权因子和价格调整
 * 6. 将组合后的数据缓存到内存中
 */
bool WtDataReader::cacheIntegratedBars(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period)
{
	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo;  // 获取合约代码信息指针

	uint32_t curDate = TimeUtils::getCurDate();                    // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;              // 获取当前时间

	uint32_t endTDate = _base_data_mgr->calcTradingDate(cInfo->stdCommID(), curDate, curTime, false);  // 计算结束交易日期

	std::string pname;                                            // 周期名称
	switch (period)                                               // 根据周期设置名称
	{
	case KP_Minute1: pname = "min1"; break;                      // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                      // 5分钟K线
	default: pname = "day"; break;                               // 日K线
	}

	BarsList& barList = _bars_cache[key];                        // 获取K线列表引用
	barList._code = stdCode;                                      // 设置合约代码
	barList._period = period;                                     // 设置K线周期
	barList._exchg = cInfo->_exchg;                              // 设置交易所代码

	std::vector<std::vector<WTSBarStruct>*> barsSections;         // K线分段数组

	uint32_t realCnt = 0;                                         // 实际K线数量

	//const char* hot_flag = cInfo->isHot() ? FILE_SUF_HOT : FILE_SUF_2ND;
	const char* ruleTag = cInfo->_ruletag;                        // 获取规则标签

	//先按照HOT代码进行读取, 如rb.HOT
	std::vector<WTSBarStruct>* hotAy = NULL;                      // 主力合约K线数组
	uint64_t lastHotTime = 0;                                     // 最后主力合约时间

	do
	{
		/*
		 *	By Wesley @ 2021.12.20
		 *	本来这里是要先调用_loader->loadRawHisBars从外部加载器读取主力合约数据的
		 *	但是上层会调用一次loadFinalHisBars，这里再调用loadRawHisBars就冗余了，所以直接跳过
		 */

		std::stringstream ss;                                     // 字符串流
		ss << _his_dir << pname << "/" << cInfo->_exchg << "/" << cInfo->_exchg << "." << cInfo->_product << "_" << ruleTag;  // 构建主力合约文件路径
		if (cInfo->isExright())                                   // 如果是复权合约
			ss << (cInfo->_exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ);  // 添加复权后缀
		ss << ".dsb";                                             // 添加文件后缀
		std::string filename = ss.str();                          // 获取文件名
		if (!StdFile::exists(filename.c_str()))                   // 如果文件不存在
			break;                                                // 跳出循环

		std::string content;                                      // 文件内容
		StdFile::read_file_content(filename.c_str(), content);    // 读取文件内容
		if (content.size() < sizeof(HisKlineBlock))               // 如果文件大小不足
		{
			pipe_reader_log(_sink, LL_ERROR, "历史K线数据文件{}大小校验失败", filename);  // 记录错误日志
			break;                                                // 跳出循环
		}
		proc_block_data(content, true, false);                    // 处理数据块

		if (content.empty())                                      // 如果内容为空
			break;                                                // 跳出循环

		uint32_t barcnt = content.size() / sizeof(WTSBarStruct);  // 计算K线数量

		hotAy = new std::vector<WTSBarStruct>();                  // 创建主力合约K线数组
		hotAy->resize(barcnt);                                    // 调整数组大小
		memcpy(hotAy->data(), content.data(), content.size());     // 复制K线数据

		if (period != KP_DAY)                                     // 如果不是日K线
			lastHotTime = hotAy->at(barcnt - 1).time;             // 获取最后时间
		else                                                      // 如果是日K线
			lastHotTime = hotAy->at(barcnt - 1).date;             // 获取最后日期

		pipe_reader_log(_sink, LL_INFO, "{} items of back {} data of wrapped contract {} directly loaded", barcnt, pname.c_str(), stdCode);  // 记录加载日志
	} while (false);
	

	HotSections secs;                                             // 主力合约分段信息
	if (strlen(ruleTag) > 0)                                      // 如果规则标签不为空
	{
		if (!_hot_mgr->splitCustomSections(ruleTag, cInfo->stdCommID(), 19900102, endTDate, secs))  // 分割自定义分段
			return false;                                           // 分割失败，返回失败
	}

	if (secs.empty())                                              // 如果分段为空
		return false;                                              // 返回失败

	//根据复权类型确定基础因子
	//如果是前复权，则历史数据会变小，以最后一个复权因子为基础因子
	//如果是后复权，则新数据会变大，基础因子为1
	double baseFactor = 1.0;                                       // 基础复权因子
	if (cInfo->_exright == 1)                                     // 如果是前复权
		baseFactor = secs.back()._factor;                         // 使用最后一个复权因子作为基础因子
	else if (cInfo->_exright == 2)                                // 如果是后复权
		barList._factor = secs.back()._factor;                   // 设置K线列表的复权因子

	bool bAllCovered = false;                                     // 是否全部覆盖标志
	for (auto it = secs.rbegin(); it != secs.rend(); it++)        // 从后往前遍历分段
	{
		const HotSection& hotSec = *it;                           // 获取主力合约分段
		const char* curCode = hotSec._code.c_str();              // 获取当前合约代码
		uint32_t rightDt = hotSec._e_date;                       // 获取结束日期
		uint32_t leftDt = hotSec._s_date;                        // 获取开始日期

		//要先将日期转换为边界时间
		WTSBarStruct sBar, eBar;                                  // 开始和结束K线结构
		if (period != KP_DAY)                                     // 如果不是日K线
		{
			uint64_t sTime = _base_data_mgr->getBoundaryTime(cInfo->stdCommID(), leftDt, false, true);  // 获取开始边界时间
			uint64_t eTime = _base_data_mgr->getBoundaryTime(cInfo->stdCommID(), rightDt, false, false);  // 获取结束边界时间

			sBar.date = leftDt;                                   // 设置开始日期
			sBar.time = ((uint32_t)(sTime / 10000) - 19900000) * 10000 + (uint32_t)(sTime % 10000);  // 设置开始时间

			if (sBar.time < lastHotTime)	//如果边界时间小于主力的最后一根Bar的时间, 说明已经有交叉了, 则不需要再处理了
			{
				bAllCovered = true;                               // 设置全部覆盖标志
				sBar.time = lastHotTime + 1;                      // 调整开始时间
			}

			eBar.date = rightDt;                                  // 设置结束日期
			eBar.time = ((uint32_t)(eTime / 10000) - 19900000) * 10000 + (uint32_t)(eTime % 10000);  // 设置结束时间

			if (eBar.time <= lastHotTime)	//右边界时间小于最后一条Hot时间, 说明全部交叉了, 没有再找的必要了
				break;                                             // 跳出循环
		}
		else                                                      // 如果是日K线
		{
			sBar.date = leftDt;                                   // 设置开始日期
			if (sBar.date < lastHotTime)	//如果边界时间小于主力的最后一根Bar的时间, 说明已经有交叉了, 则不需要再处理了
			{
				bAllCovered = true;                               // 设置全部覆盖标志
				sBar.date = (uint32_t)lastHotTime + 1;            // 调整开始日期
			}

			eBar.date = rightDt;                                  // 设置结束日期

			if (eBar.date <= lastHotTime)                         // 如果结束日期小于等于最后时间
				break;                                            // 跳出循环
		}

		/*
		 *	By Wesley @ 2021.12.20
		 *	先从extloader读取分月合约的K线数据
		 *	如果没有读到，再从文件读取
		 */
		bool bLoaded = false;                                      // 是否加载成功标志
		std::string buffer;                                        // 数据缓冲区
		if (NULL != _loader)                                       // 如果外部加载器存在
		{
			std::string wCode = fmt::format("{}.{}.{}", cInfo->_exchg, cInfo->_product, (char*)curCode + strlen(cInfo->_product));  // 构建完整合约代码
			bLoaded = _loader->loadRawHisBars(&buffer, wCode.c_str(), period, [](void* obj, WTSBarStruct* bars, uint32_t count) {  // 调用外部加载器加载原始K线数据
				std::string* buff = (std::string*)obj;             // 获取缓冲区指针
				buff->resize(sizeof(WTSBarStruct)*count);          // 调整缓冲区大小
				memcpy((void*)buff->c_str(), bars, sizeof(WTSBarStruct)*count);  // 复制K线数据
			});
		}

		if (!bLoaded)                                              // 如果外部加载失败
		{
			std::stringstream ss;                                 // 字符串流
			ss << _his_dir << pname << "/" << cInfo->_exchg << "/" << curCode << ".dsb";  // 构建文件路径
			std::string filename = ss.str();                      // 获取文件名
			if (!StdFile::exists(filename.c_str()))               // 如果文件不存在
				continue;                                          // 跳过该分段

			std::string content;                                  // 文件内容
			StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
			if (content.size() < sizeof(HisKlineBlock))            // 如果文件大小不足
			{
				pipe_reader_log(_sink, LL_ERROR, "Sizechecking of his dta file {} failed", filename.c_str());  // 记录错误日志
				return false;                                      // 返回失败
			}
			proc_block_data(content, true, false);	            // 处理数据块
			buffer.swap(content);                                 // 交换缓冲区内容
		}
		
		if(buffer.empty())                                         // 如果缓冲区为空
			break;                                                 // 跳出循环

		uint32_t barcnt = buffer.size() / sizeof(WTSBarStruct);   // 计算K线数量

		WTSBarStruct* firstBar = (WTSBarStruct*)buffer.data();    // 获取K线数据指针

		WTSBarStruct* pBar = std::lower_bound(firstBar, firstBar + (barcnt - 1), sBar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位开始位置
			if (period == KP_DAY)                                 // 如果是日K线
			{
				return a.date < b.date;                          // 按日期比较
			}
			else                                                  // 如果不是日K线
			{
				return a.time < b.time;                          // 按时间比较
			}
		});

		uint32_t sIdx = pBar - firstBar;                          // 计算开始索引
		if ((period == KP_DAY && pBar->date < sBar.date) || (period != KP_DAY && pBar->time < sBar.time))	//早于边界时间
		{
			//早于边界时间, 说明没有数据了, 因为lower_bound会返回大于等于目标位置的数据
			continue;                                              // 跳过该分段
		}

		pBar = std::lower_bound(firstBar + sIdx, firstBar + (barcnt - 1), eBar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位结束位置
			if (period == KP_DAY)                                 // 如果是日K线
			{
				return a.date < b.date;                          // 按日期比较
			}
			else                                                  // 如果不是日K线
			{
				return a.time < b.time;                          // 按时间比较
			}
		});
		uint32_t eIdx = pBar - firstBar;                          // 计算结束索引
		if ((period == KP_DAY && pBar->date > eBar.date) || (period != KP_DAY && pBar->time > eBar.time))  // 如果定位位置超过结束位置
		{
			pBar--;                                               // 回退一个K线
			eIdx--;                                                // 回退索引
		}

		if (eIdx < sIdx)                                           // 如果结束索引小于开始索引
			continue;                                              // 跳过该分段

		uint32_t curCnt = eIdx - sIdx + 1;                        // 计算当前分段K线数量

		if(cInfo->isExright())                                     // 如果是复权合约
		{	
			double factor = hotSec._factor / baseFactor;           // 计算复权因子
			for (uint32_t idx = sIdx; idx <= eIdx; idx++)          // 遍历K线数据
			{
				firstBar[idx].open *= factor;                     // 复权开盘价
				firstBar[idx].high *= factor;                     // 复权最高价
				firstBar[idx].low *= factor;                      // 复权最低价
				firstBar[idx].close *= factor;                    // 复权收盘价

				if (_adjust_flag & 1)                              // 如果调整成交量
					firstBar[idx].vol /= factor;                   // 调整成交量

				if (_adjust_flag & 2)                              // 如果调整成交额
					firstBar[idx].money *= factor;                 // 调整成交额

				if (_adjust_flag & 4)                              // 如果调整持仓量
				{
					firstBar[idx].hold /= factor;                  // 调整持仓量
					firstBar[idx].add /= factor;                   // 调整增减仓
				}
			}
		}		

		std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>();  // 创建临时K线数组
		tempAy->resize(curCnt);                                    // 调整数组大小
		memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt);  // 复制K线数据
		realCnt += curCnt;                                         // 增加实际数量

		barsSections.emplace_back(tempAy);                        // 添加到分段数组

		if (bAllCovered)                                           // 如果全部覆盖
			break;                                                  // 跳出循环
	}

	if (hotAy)                                                    // 如果主力合约K线数组存在
	{
		barsSections.emplace_back(hotAy);                         // 添加到分段数组
		realCnt += hotAy->size();                                 // 增加实际数量
	}

	if (realCnt > 0)                                              // 如果实际数量大于0
	{
		barList._bars.resize(realCnt);                            // 调整K线数组大小

		uint32_t curIdx = 0;                                      // 当前索引
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)  // 从后往前遍历分段数组
		{
			std::vector<WTSBarStruct>* tempAy = *it;             // 获取临时K线数组
			memcpy(barList._bars.data() + curIdx, tempAy->data(), tempAy->size() * sizeof(WTSBarStruct));  // 复制K线数据
			curIdx += tempAy->size();                             // 更新当前索引
			delete tempAy;                                         // 删除临时数组
		}
		barsSections.clear();                                     // 清空分段数组
	}

	pipe_reader_log(_sink,LL_INFO, "{} items of back {} data of {} cached", realCnt, pname.c_str(), stdCode);  // 记录缓存成功日志

	return true;                                                  // 返回成功
}

/*!
 * \brief 缓存股票复权K线数据
 * \param codeInfo 合约代码信息指针
 * \param key 缓存键值
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \return 是否缓存成功
 * 
 * 该函数用于缓存股票复权K线数据，支持前复权和后复权处理。
 * 主要功能：
 * 1. 计算当前交易日期和时间
 * 2. 加载已复权的K线数据
 * 3. 加载原始K线数据并进行复权处理
 * 4. 按时间顺序组合复权后的数据
 * 5. 将组合后的数据缓存到内存中
 */
bool WtDataReader::cacheAdjustedStkBars(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period)
{
	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo;  // 获取合约代码信息指针

	uint32_t curDate = TimeUtils::getCurDate();                    // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;              // 获取当前时间

	uint32_t endTDate = _base_data_mgr->calcTradingDate(cInfo->stdCommID(), curDate, curTime, false);  // 计算结束交易日期

	std::string pname;                                            // 周期名称
	switch (period)                                               // 根据周期设置名称
	{
	case KP_Minute1: pname = "min1"; break;                      // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                      // 5分钟K线
	default: pname = "day"; break;                               // 日K线
	}

	BarsList& barList = _bars_cache[key];                        // 获取K线列表引用
	barList._code = stdCode;                                      // 设置合约代码
	barList._period = period;                                     // 设置K线周期
	barList._exchg = cInfo->_exchg;                              // 设置交易所代码

	std::vector<std::vector<WTSBarStruct>*> barsSections;         // K线分段数组

	uint32_t realCnt = 0;                                         // 实际K线数量

	std::vector<WTSBarStruct>* ayAdjusted = NULL;                 // 已复权K线数组
	uint64_t lastQTime = 0;                                       // 最后时间

	do
	{
		/*
		 *	By Wesley @ 2021.12.20
		 *	本来这里是要先调用_loader->loadRawHisBars从外部加载器读取复权数据的
		 *	但是上层会调用一次loadFinalHisBars，这里再调用loadRawHisBars就冗余了，所以直接跳过
		 */
		char flag = cInfo->_exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ;  // 根据复权类型设置标志
		std::stringstream ss;                                     // 字符串流
		ss << _his_dir << pname << "/" << cInfo->_exchg << "/" << cInfo->_code << flag << ".dsb";  // 构建复权文件路径
		std::string filename = ss.str();                          // 获取文件名
		if (!StdFile::exists(filename.c_str()))                   // 如果文件不存在
			break;                                                // 跳出循环

		std::string content;                                      // 文件内容
		StdFile::read_file_content(filename.c_str(), content);     // 读取文件内容
		if (content.size() < sizeof(HisKlineBlock))               // 如果文件大小不足
		{
			pipe_reader_log(_sink,LL_ERROR, "历史K线数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
			break;                                                // 跳出循环
		}

		proc_block_data(content, true, false);                    // 处理数据块

		uint32_t barcnt = content.size() / sizeof(WTSBarStruct);   // 计算K线数量

		ayAdjusted = new std::vector<WTSBarStruct>();             // 创建已复权K线数组
		ayAdjusted->resize(barcnt);                               // 调整数组大小
		memcpy(ayAdjusted->data(), content.data(), content.size()); // 复制K线数据

		if (period != KP_DAY)                                     // 如果不是日K线
			lastQTime = ayAdjusted->at(barcnt - 1).time;         // 获取最后时间
		else                                                      // 如果是日K线
			lastQTime = ayAdjusted->at(barcnt - 1).date;          // 获取最后日期

		pipe_reader_log(_sink,LL_INFO, "{} items of adjusted back {} data of stock {} directly loaded", barcnt, pname.c_str(), stdCode);  // 记录加载日志
	} while (false);


	bool bAllCovered = false;                                     // 是否全部覆盖标志
	do
	{
		const char* curCode = cInfo->_code;                      // 获取当前合约代码

		//要先将日期转换为边界时间
		WTSBarStruct sBar;                                        // 开始K线结构
		if (period != KP_DAY)                                     // 如果不是日K线
		{
			sBar.date = TimeUtils::minBarToDate(lastQTime);       // 将分钟时间转换为日期

			sBar.time = lastQTime + 1;                           // 设置开始时间
		}
		else                                                      // 如果是日K线
		{
			sBar.date = (uint32_t)lastQTime + 1;                 // 设置开始日期
		}

		/*
		 *	By Wesley @ 2021.12.20
		 *	先从extloader读取
		 *	如果没有读到，再从文件读取
		 */
		bool bLoaded = false;                                     // 是否加载成功标志
		std::string buffer;                                       // 数据缓冲区
		std::string rawCode = fmt::format("{}.{}.{}", cInfo->_exchg, cInfo->_product, curCode);  // 构建原始合约代码
		if (NULL != _loader)                                      // 如果外部加载器存在
		{
			bLoaded = _loader->loadRawHisBars(&buffer, rawCode.c_str(), period, [](void* obj, WTSBarStruct* bars, uint32_t count) {  // 调用外部加载器加载原始K线数据
				std::string* buff = (std::string*)obj;            // 获取缓冲区指针
				buff->resize(sizeof(WTSBarStruct)*count);         // 调整缓冲区大小
				memcpy((void*)buff->c_str(), bars, sizeof(WTSBarStruct)*count);  // 复制K线数据
			});
		}

		bool bOldVer = false;                                     // 是否旧版本标志
		if (!bLoaded)                                             // 如果外部加载失败
		{
			std::stringstream ss;                                // 字符串流
			ss << _his_dir << pname << "/" << cInfo->_exchg << "/" << curCode << ".dsb";  // 构建文件路径
			std::string filename = ss.str();                     // 获取文件名
			if (!StdFile::exists(filename.c_str()))              // 如果文件不存在
				continue;                                         // 跳过该循环

			std::string content;                                 // 文件内容
			StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
			if (content.size() < sizeof(HisKlineBlock))           // 如果文件大小不足
			{
				pipe_reader_log(_sink,LL_ERROR, "历史K线数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
				return false;                                     // 返回失败
			}

			proc_block_data(content, true, false);               // 处理数据块
			buffer.swap(content);                                // 交换缓冲区内容
		}

		if(buffer.empty())                                        // 如果缓冲区为空
			break;                                                // 跳出循环

		uint32_t barcnt = buffer.size() / sizeof(WTSBarStruct);   // 计算K线数量

		WTSBarStruct* firstBar = (WTSBarStruct*)buffer.data();   // 获取K线数据指针

		WTSBarStruct* pBar = std::lower_bound(firstBar, firstBar + (barcnt - 1), sBar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位开始位置
			if (period == KP_DAY)                                // 如果是日K线
			{
				return a.date < b.date;                          // 按日期比较
			}
			else                                                 // 如果不是日K线
			{
				return a.time < b.time;                          // 按时间比较
			}
		});

		if (pBar != NULL)                                        // 如果找到位置
		{
			uint32_t sIdx = pBar - firstBar;                     // 计算开始索引
			uint32_t curCnt = barcnt - sIdx;                     // 计算当前数量

			std::vector<WTSBarStruct>* ayRaw = new std::vector<WTSBarStruct>();  // 创建原始K线数组
			ayRaw->resize(curCnt);                               // 调整数组大小
			memcpy(ayRaw->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt);  // 复制K线数据
			realCnt += curCnt;                                   // 增加实际数量

			auto& ayFactors = getAdjFactors(cInfo->_code, cInfo->_exchg, cInfo->_product);  // 获取复权因子列表
			if (!ayFactors.empty())                              // 如果复权因子列表不为空
			{
				//做复权处理
				int32_t lastIdx = curCnt;                        // 最后索引
				WTSBarStruct bar;                                // K线结构
				firstBar = ayRaw->data();                        // 获取K线数据指针

				//根据复权类型确定基础因子
				//如果是前复权，则历史数据会变小，以最后一个复权因子为基础因子
				//如果是后复权，则新数据会变大，基础因子为1
				double baseFactor = 1.0;                          // 基础复权因子
				if (cInfo->_exright == 1)                         // 如果是前复权
					baseFactor = ayFactors.back()._factor;        // 使用最后一个复权因子作为基础因子
				else if (cInfo->_exright == 2)                    // 如果是后复权
					barList._factor = ayFactors.back()._factor;   // 设置K线列表的复权因子

				for (auto it = ayFactors.rbegin(); it != ayFactors.rend(); it++)  // 从后往前遍历复权因子
				{
					const AdjFactor& adjFact = *it;              // 获取复权因子
					bar.date = adjFact._date;                     // 设置复权日期

					//调整因子
					double factor = adjFact._factor / baseFactor; // 计算复权因子

					WTSBarStruct* pBar = NULL;                    // K线指针
					pBar = std::lower_bound(firstBar, firstBar + lastIdx - 1, bar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位复权位置
						return a.date < b.date;                   // 按日期比较
					});

					if (pBar->date < bar.date)                    // 如果定位的日期小于复权日期
						continue;                                  // 跳过该复权因子

					WTSBarStruct* endBar = pBar;                  // 结束K线指针
					if (pBar != NULL)                             // 如果K线指针不为空
					{
						int32_t curIdx = pBar - firstBar;         // 当前索引
						while (pBar && curIdx < lastIdx)          // 遍历K线数据
						{
							pBar->open *= factor;                 // 复权开盘价
							pBar->high *= factor;                 // 复权最高价
							pBar->low *= factor;                  // 复权最低价
							pBar->close *= factor;                // 复权收盘价

							if (_adjust_flag & 1)                 // 如果调整成交量
								pBar->vol /= factor;              // 调整成交量

							if (_adjust_flag & 2)                 // 如果调整成交额
								pBar->money *= factor;            // 调整成交额

							if (_adjust_flag & 4)                 // 如果调整持仓量
							{
								pBar->hold /= factor;             // 调整持仓量
								pBar->add /= factor;              // 调整增减仓
							}

							pBar++;                               // 移动到下一个K线
							curIdx++;                             // 增加索引
						}
						lastIdx = endBar - firstBar;             // 更新最后索引
					}

					if (lastIdx == 0)                             // 如果最后索引为0
						break;                                     // 跳出循环
				}
			}

			barsSections.emplace_back(ayRaw);                    // 添加到分段数组
		}
	} while (false);

	if (ayAdjusted)                                               // 如果已复权K线数组存在
	{
		barsSections.emplace_back(ayAdjusted);                    // 添加到分段数组
		realCnt += ayAdjusted->size();                            // 增加实际数量
	}

	if (realCnt > 0)                                              // 如果实际数量大于0
	{
		barList._bars.resize(realCnt);                            // 调整K线数组大小

		uint32_t curIdx = 0;                                      // 当前索引
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)  // 从后往前遍历分段数组
		{
			std::vector<WTSBarStruct>* tempAy = *it;             // 获取临时K线数组
			memcpy(barList._bars.data() + curIdx, tempAy->data(), tempAy->size() * sizeof(WTSBarStruct));  // 复制K线数据
			curIdx += tempAy->size();                             // 更新当前索引
			delete tempAy;                                         // 删除临时数组
		}
		barsSections.clear();                                     // 清空分段数组
	}

	pipe_reader_log(_sink,LL_INFO, "{} items of back {} data of {} cached", realCnt, pname.c_str(), stdCode);  // 记录缓存成功日志

	return true;                                                  // 返回成功
}

/*!
 * \brief 从文件缓存历史K线数据
 * \param codeInfo 合约代码信息指针
 * \param key 缓存键值
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \return 是否缓存成功
 * 
 * 该函数用于从文件缓存历史K线数据，支持多种数据类型的处理。
 * 主要功能：
 * 1. 根据合约类型选择不同的缓存策略
 * 2. 期货主力连续合约调用cacheIntegratedBars
 * 3. 股票复权合约调用cacheAdjustedStkBars
 * 4. 其他合约直接加载原始数据
 * 5. 将数据缓存到内存中
 */
bool WtDataReader::cacheHisBarsFromFile(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period)
{
	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo;  // 获取合约代码信息指针
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo->_exchg, cInfo->_product);  // 获取商品信息
	const char* stdPID = commInfo->getFullPid();                  // 获取标准产品ID

	uint32_t curDate = TimeUtils::getCurDate();                    // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;              // 获取当前时间

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期

	std::string pname;                                            // 周期名称
	switch (period)                                               // 根据周期设置名称
	{
	case KP_Minute1: pname = "min1"; break;                      // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                      // 5分钟K线
	default: pname = "day"; break;                               // 日K线
	}

	BarsList& barList = _bars_cache[key];                        // 获取K线列表引用
	barList._code = stdCode;                                      // 设置合约代码
	barList._period = period;                                     // 设置K线周期
	barList._exchg = cInfo->_exchg;                              // 设置交易所代码

	std::vector<std::vector<WTSBarStruct>*> barsSections;         // K线分段数组

	uint32_t realCnt = 0;                                         // 实际K线数量
	const char* ruleTag = cInfo->_ruletag;                        // 获取规则标签
	if (strlen(ruleTag) > 0)                                      // 如果规则标签不为空
	{
		//如果是读取期货主力连续数据
		return cacheIntegratedBars(cInfo, key, stdCode, period); // 调用期货主力连续缓存函数
	}
	else if(cInfo->isExright() && commInfo->isStock())            // 如果是股票复权合约
	{
		//如果是读取股票复权数据
		return cacheAdjustedStkBars(cInfo, key, stdCode, period); // 调用股票复权缓存函数
	}

	
	//直接原始数据直接加载

	/*
	 *	By Wesley @ 2021.12.20
	 *	先从extloader读取
	 *	如果没有读到，再从文件读取
	 */
	bool bLoaded = false;                                         // 是否加载成功标志
	std::string buffer;                                           // 数据缓冲区
	if (NULL != _loader)                                          // 如果外部加载器存在
	{
		bLoaded = _loader->loadRawHisBars(&buffer, stdCode, period, [](void* obj, WTSBarStruct* bars, uint32_t count) {  // 调用外部加载器加载原始K线数据
			std::string* buff = (std::string*)obj;                 // 获取缓冲区指针
			buff->resize(sizeof(WTSBarStruct)*count);             // 调整缓冲区大小
			memcpy((void*)buff->c_str(), bars, sizeof(WTSBarStruct)*count);  // 复制K线数据
		});
	}

	if (!bLoaded)                                                 // 如果外部加载失败
	{
		//读取历史的
		std::stringstream ss;                                     // 字符串流
		ss << _his_dir << pname << "/" << cInfo->_exchg << "/" << cInfo->_code << ".dsb";  // 构建文件路径
		std::string filename = ss.str();                          // 获取文件名
		if (StdFile::exists(filename.c_str()))                    // 如果文件存在
		{
			//如果有格式化的历史数据文件, 则直接读取
			std::string content;                                  // 文件内容
			StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
			if (content.size() < sizeof(HisKlineBlock))           // 如果文件大小不足
			{
				pipe_reader_log(_sink,LL_ERROR, "历史K线数据文件{}大小校验失败", filename.c_str());  // 记录错误日志
				return false;                                      // 返回失败
			}

			proc_block_data(content, true, false);                // 处理数据块
			buffer.swap(content);                                 // 交换缓冲区内容
		}
	}

	if (buffer.empty())                                           // 如果缓冲区为空
		return false;                                              // 返回失败

	uint32_t barcnt = buffer.size() / sizeof(WTSBarStruct);       // 计算K线数量

	WTSBarStruct* firstBar = (WTSBarStruct*)buffer.data();        // 获取K线数据指针

	if (barcnt > 0)                                               // 如果K线数量大于0
	{
		uint32_t sIdx = 0;                                        // 开始索引
		uint32_t idx = barcnt - 1;                                // 结束索引
		uint32_t curCnt = (idx - sIdx + 1);                       // 当前数量

		std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>();  // 创建临时K线数组
		tempAy->resize(curCnt);                                    // 调整数组大小
		memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt);  // 复制K线数据
		realCnt += curCnt;                                        // 增加实际数量

		barsSections.emplace_back(tempAy);                        // 添加到分段数组
	}

	if (realCnt > 0)                                              // 如果实际数量大于0
	{
		barList._bars.resize(realCnt);                            // 调整K线数组大小

		uint32_t curIdx = 0;                                      // 当前索引
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)  // 从后往前遍历分段数组
		{
			std::vector<WTSBarStruct>* tempAy = *it;             // 获取临时K线数组
			memcpy(barList._bars.data() + curIdx, tempAy->data(), tempAy->size()*sizeof(WTSBarStruct));  // 复制K线数据
			curIdx += tempAy->size();                             // 更新当前索引
			delete tempAy;                                         // 删除临时数组
		}
		barsSections.clear();                                     // 清空分段数组
	}

	pipe_reader_log(_sink,LL_INFO, "{} items of back {} data of {} cached", realCnt, pname.c_str(), stdCode);  // 记录缓存成功日志
	return true;                                                  // 返回成功
}

/*!
 * \brief 读取K线数据切片
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \param count 数据数量
 * \param etime 结束时间（可选，默认为0）
 * \return K线数据切片指针
 * 
 * 该函数用于读取指定合约的K线数据切片，支持实时数据和历史数据的组合。
 * 主要功能：
 * 1. 解析合约代码信息
 * 2. 检查缓存中是否存在历史数据
 * 3. 如果不存在，从外部加载器或文件加载历史数据
 * 4. 计算交易日期和时间
 * 5. 组合历史数据和实时数据
 * 6. 创建并返回K线数据切片
 */
WTSKlineSlice* WtDataReader::readKlineSlice(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	const char* stdPID = cInfo.stdCommID();                      // 获取标准商品ID

	thread_local static char key[64] = { 0 };                    // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}#{}", stdCode, period);           // 格式化键值
	auto it = _bars_cache.find(key);                             // 查找K线缓存
	bool bHasHisData = false;                                    // 是否有历史数据标志
	if (it == _bars_cache.end())                                 // 如果缓存中不存在
	{
		/*
		 *	By Wesley @ 2021.12.20
		 *	先从extloader加载最终的K线数据（如果是复权）
		 *	如果加载失败，则再从文件加载K线数据
		 */
		bHasHisData = cacheFinalBarsFromLoader(&cInfo, key, stdCode, period);  // 尝试从外部加载器缓存最终K线数据

		if(!bHasHisData)                                          // 如果外部加载失败
			bHasHisData = cacheHisBarsFromFile(&cInfo, key, stdCode, period);  // 从文件缓存历史K线数据
	}
	else                                                          // 如果缓存中存在
	{
		bHasHisData = true;                                       // 设置历史数据标志
	}

	uint32_t curDate, curTime;                                   // 当前日期、时间
	if (etime == 0)                                              // 如果结束时间为0
	{
		curDate = _sink->get_date();                              // 获取当前日期
		curTime = _sink->get_min_time();                          // 获取当前分钟时间
		etime = (uint64_t)curDate * 10000 + curTime;             // 计算结束时间戳
	}
	else                                                          // 如果指定了结束时间
	{
		curDate = (uint32_t)(etime / 10000);                     // 从时间戳提取日期
		curTime = (uint32_t)(etime % 10000);                     // 从时间戳提取时间
	}

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false);  // 计算当前交易日期

	BarsList& barsList = _bars_cache[key];                       // 获取K线列表引用
	WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, 1, NULL, 0);;  // 创建K线数据切片
	WTSBarStruct* head = NULL;                                   // K线数据头指针
	uint32_t hisCnt = 0;                                         // 历史数据数量
	uint32_t rtCnt = 0;                                          // 实时数据数量
	uint32_t totalCnt = 0;                                       // 总数据数量
	std::string pname;                                           // 周期名称
	switch (period)                                               // 根据周期设置名称
	{
	case KP_Minute1: pname = "min1"; break;                      // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                      // 5分钟K线
	default: pname = "day"; break;                               // 日K线
	}

	uint32_t left = count;                                        // 剩余数量

	//是否包含当天的
	bool bHasToday = (endTDate == curTDate);                      // 是否包含当日数据

	//By Wesley @ 2022.05.28
	//不需要区分是否是期货了
	const char* ruleTag = cInfo._ruletag;                        // 获取规则标签
	if (strlen(ruleTag) > 0)                                     // 如果规则标签不为空
	{
		barsList._raw_code = _hot_mgr->getCustomRawCode(ruleTag, stdPID, curTDate);  // 获取自定义原始代码
		pipe_reader_log(_sink, LL_INFO, "{} contract on {} confirmed: {} -> {}", ruleTag, curTDate, stdCode, barsList._raw_code.c_str());  // 记录合约确认日志
	}
	else                                                          // 如果规则标签为空
	{
		barsList._raw_code = cInfo._code;                        // 设置原始代码为合约代码
	}

	/*
	if (commInfo->isFuture())
	{
		const char* ruleTag = cInfo._ruletag;
		if (strlen(ruleTag) > 0)
		{
			barsList._raw_code = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), curTDate);
			pipe_reader_log(_sink, LL_INFO, "{} contract on {} confirmed with rule {}: {} -> {}", ruleTag, curTDate, stdCode, barsList._raw_code.c_str());
		}
		//else if (cInfo.isHot())
		//{
		//	barsList._raw_code = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, curTDate);
		//	pipe_reader_log(_sink, LL_INFO, "Hot contract on {}  confirmed: {} -> {}", curTDate, stdCode, barsList._raw_code.c_str());
		//}
		//else if (cInfo.isSecond())
		//{
		//	barsList._raw_code = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, curTDate);
		//	pipe_reader_log(_sink, LL_INFO, "Second contract on {} confirmed: {} -> {}", curTDate, stdCode, barsList._raw_code.c_str());
		//}
		else
		{
			barsList._raw_code = cInfo._code;
		}
	}
	else
	{
		barsList._raw_code = cInfo._code;
	}
	*/

	if (bHasToday)                                                // 如果包含当日数据
	{
		WTSBarStruct bar;                                         // K线结构
		bar.date = curDate;                                       // 设置日期
		bar.time = (curDate - 19900000) * 10000 + curTime;       // 设置时间

		const char* curCode = barsList._raw_code.c_str();        // 获取当前合约代码

		//读取实时的
		RTKlineBlockPair* kPair = getRTKilneBlock(cInfo._exchg, curCode, period);  // 获取实时K线数据块
		if (kPair != NULL && kPair->_block && kPair->_block->_size>0)  // 如果数据块存在且不为空
		{
			//读取当日的数据
			WTSBarStruct* pBar = NULL;                           // K线指针
			pBar = std::lower_bound(kPair->_block->_bars, kPair->_block->_bars + (kPair->_block->_size - 1), bar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位K线位置
				if (period == KP_DAY)                            // 如果是日K线
					return a.date < b.date;                      // 按日期比较
				else                                             // 如果不是日K线
					return a.time < b.time;                      // 按时间比较
			});

			uint32_t idx = 0;                                     // 索引
			if (pBar != NULL)                                     // 如果找到位置
				idx = pBar - kPair->_block->_bars;               // 计算索引
			else                                                  // 如果未找到位置
				idx = kPair->_block->_size;                       // 设置为数据块大小

			if ((period == KP_DAY && pBar->date > bar.date) || (period != KP_DAY && pBar->time > bar.time))  // 如果定位位置超过目标位置
			{
				pBar--;                                           // 回退一个K线
				idx--;                                            // 回退索引
			}

			uint32_t sIdx = 0;                                    // 开始索引
			if (left <= idx + 1)                                  // 如果剩余数量小于等于索引+1
			{
				sIdx = idx - left + 1;                           // 计算开始索引
			}

			uint32_t curCnt = (idx - sIdx + 1);                   // 当前数量
			left -= (idx - sIdx + 1);                             // 减少剩余数量
			hisCnt = bHasHisData ? left : 0;                      // 历史数据数量
			rtCnt = curCnt;                                       // 实时数据数量
			//By Wesley @ 2022.05.28
			//连续合约也要支持复权
			if(cInfo._exright == 2/* && commInfo->isStock()*/)     // 如果是后复权
			{
				//后复权数据要把最新的数据进行复权处理，所以要作为历史数据追加到尾部
				//虽然后复权数据要进行复权处理，但是实时数据的位置标记也要更新到最新，不然OnMinuteEnd会从开盘开始回放的
				//复权数据是创建副本后修改
				if (barsList._rt_cursor == UINT_MAX || idx > barsList._rt_cursor)  // 如果光标未初始化或索引大于光标
				{
					barsList._rt_cursor = idx;                    // 更新光标位置
					double factor = barsList._factor;             // 获取复权因子
					uint32_t oldSize = barsList._bars.size();     // 获取旧大小
					uint32_t newSize = oldSize + curCnt;          // 计算新大小
					barsList._bars.resize(newSize);               // 调整K线数组大小
					memcpy(&barsList._bars[oldSize], &kPair->_block->_bars[sIdx], sizeof(WTSBarStruct)* curCnt);  // 复制K线数据
					for(uint32_t thisIdx = oldSize; thisIdx < newSize; thisIdx++)  // 遍历新添加的K线数据
					{
						WTSBarStruct* pBar = &barsList._bars[thisIdx];  // 获取K线指针
						pBar->open *= factor;                    // 复权开盘价
						pBar->high *= factor;                     // 复权最高价
						pBar->low *= factor;                      // 复权最低价
						pBar->close *= factor;                    // 复权收盘价
					}
				}
				totalCnt = hisCnt + rtCnt;                        // 计算总数量
				totalCnt = min(totalCnt, (uint32_t)barsList._bars.size());  // 限制总数量
				// 复权后的数据直接从barlist中截取
				if (totalCnt > 0)                                 // 如果总数量大于0
				{
					head = &barsList._bars[barsList._bars.size() - totalCnt];  // 获取数据头指针
					slice->appendBlock(head, totalCnt);           // 追加数据块
				}
			}
			else                                                  // 如果不是后复权
			{
				// 普通数据由历史和rt拼接，其中rt直接引用
				barsList._rt_cursor = idx;                        // 更新光标位置
				hisCnt = min(hisCnt, (uint32_t)barsList._bars.size());  // 限制历史数据数量
				if (hisCnt > 0)                                   // 如果历史数据数量大于0
				{
					head = &barsList._bars[barsList._bars.size() - hisCnt];  // 获取历史数据头指针
					slice->appendBlock(head, hisCnt);             // 追加历史数据块
				}
				// 添加rt
				if (rtCnt > 0)                                    // 如果实时数据数量大于0
				{
					head = &kPair->_block->_bars[sIdx];           // 获取实时数据头指针
					slice->appendBlock(head, rtCnt);              // 追加实时数据块
				}
			}
		}
		else                                                      // 如果实时数据块不存在或为空
		{
			rtCnt = 0;                                            // 实时数据数量为0
			hisCnt = count;                                       // 历史数据数量为请求数量
			hisCnt = min(hisCnt, (uint32_t)barsList._bars.size()); // 限制历史数据数量
			head = &barsList._bars[barsList._bars.size() - hisCnt]; // 获取历史数据头指针
			slice->appendBlock(head, hisCnt);                     // 追加历史数据块
		}
	}
	else                                                          // 如果不包含当日数据
	{
		rtCnt = 0;                                                // 实时数据数量为0
		hisCnt = count;                                           // 历史数据数量为请求数量
		hisCnt = min(hisCnt, (uint32_t)barsList._bars.size());     // 限制历史数据数量
		head = &barsList._bars[barsList._bars.size() - hisCnt];   // 获取历史数据头指针
		slice->appendBlock(head, hisCnt);                         // 追加历史数据块
	}

	pipe_reader_log(_sink, LL_DEBUG, "His {} bars of {} loaded, {} from history, {} from realtime", PERIOD_NAME[period], stdCode, hisCnt, rtCnt);  // 记录加载日志
	return slice;                                                  // 返回K线数据切片
}

/*!
 * \brief 获取实时Tick数据块
 * \param exchg 交易所代码
 * \param code 合约代码
 * \return 实时Tick数据块对指针
 * 
 * 该函数用于获取指定合约的实时Tick数据块，使用内存映射文件技术。
 * 主要功能：
 * 1. 构建数据文件路径
 * 2. 检查文件是否存在
 * 3. 创建或更新内存映射文件
 * 4. 处理文件大小变化的情况
 * 5. 返回数据块对指针
 */
WtDataReader::TickBlockPair* WtDataReader::getRTTickBlock(const char* exchg, const char* code)
{
	thread_local static char key[64] = { 0 };                     // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}#{}", exchg, code);                // 格式化键值

	thread_local static char path[256] = { 0 };                   // 线程局部静态路径缓冲区
	fmtutil::format_to(path, "{}ticks/{}/{}.dmb", _rt_dir.c_str(), exchg, code);  // 构建文件路径

	if (!StdFile::exists(path))                                   // 如果文件不存在
		return NULL;                                               // 返回空指针

	TickBlockPair& block = _rt_tick_map[key];                     // 获取Tick数据块对引用
	if (block._file == NULL || block._block == NULL)              // 如果文件或数据块为空
	{
		if (block._file == NULL)                                  // 如果文件为空
		{
			block._file.reset(new BoostMappingFile());             // 创建新的内存映射文件
		}

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 映射文件到内存
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTTickBlock*)block._file->addr();         // 获取数据块指针
		block._last_cap = block._block->_capacity;                // 记录容量
	}
	else if (block._last_cap != block._block->_capacity)          // 如果容量发生变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());                 // 重新创建内存映射文件
		block._last_cap = 0;                                       // 重置容量
		block._block = NULL;                                       // 重置数据块指针

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 重新映射文件
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTTickBlock*)block._file->addr();         // 获取新的数据块指针
		block._last_cap = block._block->_capacity;                // 记录新容量
	}

	return &block;                                                 // 返回数据块对指针
}

/*!
 * \brief 获取实时逐笔委托数据块
 * \param exchg 交易所代码
 * \param code 合约代码
 * \return 实时逐笔委托数据块对指针
 * 
 * 该函数用于获取指定合约的实时逐笔委托数据块，使用内存映射文件技术。
 * 主要功能：
 * 1. 构建数据文件路径
 * 2. 检查文件是否存在
 * 3. 创建或更新内存映射文件
 * 4. 处理文件大小变化的情况
 * 5. 返回数据块对指针
 */
WtDataReader::OrdDtlBlockPair* WtDataReader::getRTOrdDtlBlock(const char* exchg, const char* code)
{
	thread_local static char key[64] = { 0 };                     // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}#{}", exchg, code);                 // 格式化键值

	thread_local static char path[256] = { 0 };                   // 线程局部静态路径缓冲区
	fmtutil::format_to(path, "{}orders/{}/{}.dmb", _rt_dir.c_str(), exchg, code);  // 构建文件路径

	if (!StdFile::exists(path))                                   // 如果文件不存在
		return NULL;                                               // 返回空指针

	OrdDtlBlockPair& block = _rt_orddtl_map[key];                 // 获取逐笔委托数据块对引用
	if (block._file == NULL || block._block == NULL)              // 如果文件或数据块为空
	{
		if (block._file == NULL)                                  // 如果文件为空
		{
			block._file.reset(new BoostMappingFile());             // 创建新的内存映射文件
		}

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 映射文件到内存
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTOrdDtlBlock*)block._file->addr();       // 获取数据块指针
		block._last_cap = block._block->_capacity;                // 记录容量
	}
	else if (block._last_cap != block._block->_capacity)          // 如果容量发生变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());                 // 重新创建内存映射文件
		block._last_cap = 0;                                       // 重置容量
		block._block = NULL;                                       // 重置数据块指针

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 重新映射文件
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTOrdDtlBlock*)block._file->addr();       // 获取新的数据块指针
		block._last_cap = block._block->_capacity;                // 记录新容量
	}

	return &block;                                                 // 返回数据块对指针
}

/*!
 * \brief 获取实时委托队列数据块
 * \param exchg 交易所代码
 * \param code 合约代码
 * \return 实时委托队列数据块对指针
 * 
 * 该函数用于获取指定合约的实时委托队列数据块，使用内存映射文件技术。
 * 主要功能：
 * 1. 构建数据文件路径
 * 2. 检查文件是否存在
 * 3. 创建或更新内存映射文件
 * 4. 处理文件大小变化的情况
 * 5. 返回数据块对指针
 */
WtDataReader::OrdQueBlockPair* WtDataReader::getRTOrdQueBlock(const char* exchg, const char* code)
{
	thread_local static char key[64] = { 0 };                     // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}#{}", exchg, code);                 // 格式化键值

	thread_local static char path[256] = { 0 };                   // 线程局部静态路径缓冲区
	fmtutil::format_to(path, "{}queue/{}/{}.dmb", _rt_dir.c_str(), exchg, code);  // 构建文件路径

	if (!StdFile::exists(path))                                   // 如果文件不存在
		return NULL;                                               // 返回空指针

	OrdQueBlockPair& block = _rt_ordque_map[key];                 // 获取委托队列数据块对引用
	if (block._file == NULL || block._block == NULL)              // 如果文件或数据块为空
	{
		if (block._file == NULL)                                  // 如果文件为空
		{
			block._file.reset(new BoostMappingFile());             // 创建新的内存映射文件
		}

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 映射文件到内存
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTOrdQueBlock*)block._file->addr();        // 获取数据块指针
		block._last_cap = block._block->_capacity;                // 记录容量
	}
	else if (block._last_cap != block._block->_capacity)          // 如果容量发生变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());                 // 重新创建内存映射文件
		block._last_cap = 0;                                       // 重置容量
		block._block = NULL;                                       // 重置数据块指针

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 重新映射文件
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTOrdQueBlock*)block._file->addr();        // 获取新的数据块指针
		block._last_cap = block._block->_capacity;                // 记录新容量
	}

	return &block;                                                 // 返回数据块对指针
}

/*!
 * \brief 获取实时逐笔成交数据块
 * \param exchg 交易所代码
 * \param code 合约代码
 * \return 实时逐笔成交数据块对指针
 * 
 * 该函数用于获取指定合约的实时逐笔成交数据块，使用内存映射文件技术。
 * 主要功能：
 * 1. 构建数据文件路径
 * 2. 检查文件是否存在
 * 3. 创建或更新内存映射文件
 * 4. 处理文件大小变化的情况
 * 5. 返回数据块对指针
 */
WtDataReader::TransBlockPair* WtDataReader::getRTTransBlock(const char* exchg, const char* code)
{
	thread_local static char key[64] = { 0 };                     // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}#{}", exchg, code);                 // 格式化键值

	thread_local static char path[256] = { 0 };                   // 线程局部静态路径缓冲区
	fmtutil::format_to(path, "{}trans/{}/{}.dmb", _rt_dir.c_str(), exchg, code);  // 构建文件路径

	if (!StdFile::exists(path))                                   // 如果文件不存在
		return NULL;                                               // 返回空指针

	TransBlockPair& block = _rt_trans_map[key];                    // 获取逐笔成交数据块对引用
	if (block._file == NULL || block._block == NULL)              // 如果文件或数据块为空
	{
		if (block._file == NULL)                                  // 如果文件为空
		{
			block._file.reset(new BoostMappingFile());             // 创建新的内存映射文件
		}

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 映射文件到内存
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTTransBlock*)block._file->addr();         // 获取数据块指针
		block._last_cap = block._block->_capacity;                // 记录容量
	}
	else if (block._last_cap != block._block->_capacity)          // 如果容量发生变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());                 // 重新创建内存映射文件
		block._last_cap = 0;                                       // 重置容量
		block._block = NULL;                                       // 重置数据块指针

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 重新映射文件
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTTransBlock*)block._file->addr();         // 获取新的数据块指针
		block._last_cap = block._block->_capacity;                // 记录新容量
	}

	return &block;                                                 // 返回数据块对指针
}

/*!
 * \brief 获取实时K线数据块
 * \param exchg 交易所代码
 * \param code 合约代码
 * \param period K线周期
 * \return 实时K线数据块对指针
 * 
 * 该函数用于获取指定合约的实时K线数据块，使用内存映射文件技术。
 * 主要功能：
 * 1. 检查K线周期是否支持（仅支持1分钟和5分钟）
 * 2. 根据周期选择对应的缓存映射和子目录
 * 3. 构建数据文件路径
 * 4. 检查文件是否存在
 * 5. 创建或更新内存映射文件
 * 6. 处理文件大小变化的情况
 * 7. 返回数据块对指针
 */
WtDataReader::RTKlineBlockPair* WtDataReader::getRTKilneBlock(const char* exchg, const char* code, WTSKlinePeriod period)
{
	if (period != KP_Minute1 && period != KP_Minute5)             // 如果周期不是1分钟或5分钟
		return NULL;                                               // 返回空指针

	thread_local static char key[64] = { 0 };                     // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}.{}", exchg, code);                 // 格式化键值

	RTKBlockFilesMap* cache_map = NULL;                           // 缓存映射指针
	std::string subdir = "";                                      // 子目录
	BlockType bType;                                              // 数据块类型
	switch (period)                                               // 根据周期设置参数
	{
	case KP_Minute1:                                              // 1分钟K线
		cache_map = &_rt_min1_map;                                // 设置1分钟缓存映射
		subdir = "min1";                                          // 设置子目录
		bType = BT_RT_Minute1;                                    // 设置数据块类型
		break;
	case KP_Minute5:                                              // 5分钟K线
		cache_map = &_rt_min5_map;                                // 设置5分钟缓存映射
		subdir = "min5";                                          // 设置子目录
		bType = BT_RT_Minute5;                                    // 设置数据块类型
		break;
	default: break;                                               // 其他周期
	}

	thread_local static char path[256] = { 0 };                   // 线程局部静态路径缓冲区
	fmtutil::format_to(path, "{}{}/{}/{}.dmb", _rt_dir, subdir, exchg, code);  // 构建文件路径

	if (!StdFile::exists(path))                                   // 如果文件不存在
		return NULL;                                               // 返回空指针

	RTKlineBlockPair& block = (*cache_map)[key];                  // 获取K线数据块对引用
	if (block._file == NULL || block._block == NULL)              // 如果文件或数据块为空
	{
		if (block._file == NULL)                                  // 如果文件为空
		{
			block._file.reset(new BoostMappingFile());             // 创建新的内存映射文件
		}

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 映射文件到内存
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTKlineBlock*)block._file->addr();         // 获取数据块指针
		block._last_cap = block._block->_capacity;                // 记录容量
		pipe_reader_log(_sink, LL_DEBUG, "RT {} block of {}.{} loaded", subdir.c_str(), exchg, code);  // 记录加载日志
	}
	else if (block._last_cap != block._block->_capacity)          // 如果容量发生变化
	{
		//说明文件大小已变, 需要重新映射
		pipe_reader_log(_sink, LL_DEBUG, "RT {} block of {}.{} expanded to {}, remapping...", subdir.c_str(), exchg, code, block._block->_capacity);  // 记录扩展日志

		block._file.reset(new BoostMappingFile());                 // 重新创建内存映射文件
		block._last_cap = 0;                                       // 重置容量
		block._block = NULL;                                       // 重置数据块指针

		if (!block._file->map(path, boost::interprocess::read_only, boost::interprocess::read_only))  // 重新映射文件
			return NULL;                                           // 映射失败，返回空指针

		block._block = (RTKlineBlock*)block._file->addr();         // 获取新的数据块指针
		block._last_cap = block._block->_capacity;                // 记录新容量
	}	

	return &block;                                                 // 返回数据块对指针
}

/*!
 * \brief 分钟结束回调函数
 * \param uDate 交易日期
 * \param uTime 结束时间
 * \param endTDate 结束交易日期（可选，默认为0）
 * 
 * 该函数在每分钟结束时被调用，用于处理实时K线数据的更新。
 * 主要功能：
 * 1. 检查时间是否有效
 * 2. 遍历所有缓存的K线数据
 * 3. 处理非日线周期的实时K线数据
 * 4. 支持复权数据的处理
 * 5. 回调K线数据更新事件
 */
void WtDataReader::onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate /* = 0 */)
{
	//这里应该触发检查
	uint64_t nowTime = (uint64_t)uDate * 10000 + uTime;           // 计算当前时间戳
	if (nowTime <= _last_time)                                    // 如果时间没有前进
		return;                                                   // 直接返回

	for (auto it = _bars_cache.begin(); it != _bars_cache.end(); it++)  // 遍历所有K线缓存
	{
		BarsList& barsList = (BarsList&)it->second;              // 获取K线列表引用
		if (barsList._period != KP_DAY)                           // 如果不是日线周期
		{
			if (!barsList._raw_code.empty())                      // 如果原始代码不为空
			{
				RTKlineBlockPair* kBlk = getRTKilneBlock(barsList._exchg.c_str(), barsList._raw_code.c_str(), barsList._period);  // 获取实时K线数据块
				if (kBlk == NULL)                                 // 如果数据块为空
					continue;                                     // 跳过该K线列表

				//确定上一次的读取过的实时K线条数
				uint32_t preCnt = 0;                              // 预计数
				//如果实时K线没有初始化过，则已读取的条数为0
				//如果已经初始化过，则已读取的条数为光标+1
				if (barsList._rt_cursor == UINT_MAX)              // 如果光标未初始化
					preCnt = 0;                                   // 预计数为0
				else                                              // 如果光标已初始化
					preCnt = barsList._rt_cursor + 1;            // 预计数为光标+1

				for (;;)                                          // 无限循环处理K线数据
				{
					if (kBlk->_block->_size <= preCnt)            // 如果数据块大小小于等于预计数
						break;                                    // 跳出循环

					WTSBarStruct& nextBar = kBlk->_block->_bars[preCnt];  // 获取下一个K线数据

					uint64_t barTime = 199000000000 + nextBar.time;  // 计算K线时间戳
					if (barTime <= nowTime)                       // 如果K线时间小于等于当前时间
					{
						//如果不是后复权，则直接回调onbar
						//如果是后复权，则将最新bar复权处理以后，添加到cache中，再回调onbar
						if(barsList._factor == DBL_MAX)            // 如果不是后复权
						{
							_sink->on_bar(barsList._code.c_str(), barsList._period, &nextBar);  // 直接回调K线数据
						}
						else                                      // 如果是后复权
						{
							WTSBarStruct cpBar = nextBar;         // 复制K线数据
							cpBar.open *= barsList._factor;       // 复权开盘价
							cpBar.high *= barsList._factor;       // 复权最高价
							cpBar.low *= barsList._factor;        // 复权最低价
							cpBar.close *= barsList._factor;      // 复权收盘价

							barsList._bars.emplace_back(cpBar);   // 添加到K线列表

							_sink->on_bar(barsList._code.c_str(), barsList._period, &barsList._bars[barsList._bars.size()-1]);  // 回调复权后的K线数据
						}
					}
					else                                          // 如果K线时间大于当前时间
					{
						break;                                    // 跳出循环
					}

					preCnt++;                                     // 增加预计数
				}

				//如果已处理的K线条数不为0，则修改光标位置
				if (preCnt > 0)                                  // 如果处理了K线数据
					barsList._rt_cursor = preCnt - 1;            // 更新光标位置
			}
		}
		//这一段逻辑没有用了，在实盘中日线是不会闭合的，所以也不存在当日K线闭合的情况
		//实盘中都通过ontick处理当日实时数据
		//else if (barsList._period == KP_DAY)
		//{
		//	if (barsList._his_cursor != UINT_MAX && barsList._bars.size() - 1 > barsList._his_cursor)
		//	{
		//		for (;;)
		//		{
		//			WTSBarStruct& nextBar = barsList._bars[barsList._his_cursor + 1];

		//			if (nextBar.date <= endTDate)
		//			{
		//				_sink->on_bar(barsList._code.c_str(), barsList._period, &nextBar);
		//			}
		//			else
		//			{
		//				break;
		//			}

		//			barsList._his_cursor++;

		//			if (barsList._his_cursor == barsList._bars.size() - 1)
		//				break;
		//		}
		//	}
		//}
	}

	if (_sink)                                                    // 如果回调接口存在
		_sink->on_all_bar_updated(uTime);                         // 回调所有K线更新事件

	_last_time = nowTime;                                         // 更新最后时间
}

/*!
 * \brief 获取指定日期的复权因子
 * \param stdCode 标准合约代码
 * \param date 交易日期（可选，默认为0）
 * \return 复权因子
 * 
 * 该函数用于获取指定合约在指定日期的复权因子。
 * 主要功能：
 * 1. 解析合约代码信息
 * 2. 检查是否为股票合约
 * 3. 构建复权因子查找键
 * 4. 使用二分查找定位复权因子
 * 5. 返回对应的复权因子值
 */
double WtDataReader::getAdjFactorByDate(const char* stdCode, uint32_t date /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息
	if (!commInfo->isStock())                                     // 如果不是股票合约
		return 1.0;                                               // 返回默认复权因子1.0

	AdjFactor factor = { date, 1.0 };                            // 创建复权因子结构

	std::string key = stdCode;                                    // 获取标准合约代码
	if (cInfo.isExright())                                        // 如果是复权合约
		key = key.substr(0, key.size() - 1);                      // 去掉复权标记
	const AdjFactorList& factList = _adj_factors[key];            // 获取复权因子列表
	if (factList.empty())                                         // 如果复权因子列表为空
		return 1.0;                                               // 返回默认复权因子1.0

	auto it = std::lower_bound(factList.begin(), factList.end(), factor, [](const AdjFactor& a, const AdjFactor&b) {  // 使用二分查找定位复权因子
		return a._date < b._date;                                 // 按日期比较
	});

	if(it == factList.end())                                      // 如果未找到
	{
		//找不到，则说明目标日期大于最后一条的日期，直接返回最后一条除权因子
		return factList.back()._factor;                          // 返回最后一个复权因子
	}
	else                                                          // 如果找到了
	{
		//如果找到了，但是命中的日期大于目标日期，则用上一条
		//如果等于目标日期，则用命中这一条
		if ((*it)._date > date)                                   // 如果命中的日期大于目标日期
			it--;                                                 // 回退到上一个复权因子

		return (*it)._factor;                                     // 返回复权因子值
	}
}

/*!
 * \brief 获取复权因子列表
 * \param code 合约代码
 * \param exchg 交易所代码
 * \param pid 产品ID
 * \return 复权因子列表引用
 * 
 * 该函数用于获取指定合约的复权因子列表，支持按需加载。
 * 主要功能：
 * 1. 构建复权因子查找键
 * 2. 检查缓存中是否存在复权因子
 * 3. 如果不存在，通过外部加载器按需加载
 * 4. 添加默认的起始复权因子
 * 5. 按日期排序复权因子列表
 */
const WtDataReader::AdjFactorList& WtDataReader::getAdjFactors(const char* code, const char* exchg, const char* pid)
{
	thread_local static char key[20] = { 0 };                     // 线程局部静态键值缓冲区
	fmtutil::format_to(key, "{}.{}.{}", exchg, pid, code);         // 格式化键值

	auto it = _adj_factors.find(key);                             // 查找复权因子
	if (it == _adj_factors.end())                                 // 如果未找到
	{
		//By Wesley @ 2021.12.21
		//如果没有复权因子，就从extloader按需读一次
		if (_loader)                                              // 如果外部加载器存在
		{
			if(_sink) pipe_reader_log(_sink,LL_INFO, "No adjusting factors of {} cached, searching via extented loader...", key);  // 记录按需加载日志
			_loader->loadAdjFactors(this, key, [](void* obj, const char* stdCode, uint32_t* dates, double* factors, uint32_t count) {  // 调用外部加载器加载复权因子
				WtDataReader* self = (WtDataReader*)obj;          // 获取数据读取器指针
				AdjFactorList& fctrLst = self->_adj_factors[stdCode];  // 获取复权因子列表引用

				for (uint32_t i = 0; i < count; i++)              // 遍历所有复权因子
				{
					AdjFactor adjFact;                            // 创建复权因子结构
					adjFact._date = dates[i];                     // 设置复权日期
					adjFact._factor = factors[i];                 // 设置复权因子值

					fctrLst.emplace_back(adjFact);               // 添加到复权因子列表
				}

				//一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
				AdjFactor adjFact;                                // 创建默认复权因子
				adjFact._date = 19900101;                         // 设置默认日期为1990年1月1日
				adjFact._factor = 1;                              // 设置默认复权因子为1
				fctrLst.emplace_back(adjFact);                    // 添加默认复权因子

				std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {  // 按日期排序复权因子列表
					return left._date < right._date;              // 按日期升序排列
				});

				pipe_reader_log(self->_sink, LL_INFO, "{} items of adjusting factors of {} loaded via extended loader", count, stdCode);  // 记录加载成功日志
			});
		}
	}

	return _adj_factors[key];                                     // 返回复权因子列表引用
}