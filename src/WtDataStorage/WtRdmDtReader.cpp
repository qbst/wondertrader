/*!
 * \file WtRdmDtReader.cpp
 * \project WonderTrader
 * 
 * \brief WonderTrader随机数据读取器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtRdmDtReader类的所有功能，是WonderTrader框架中用于随机数据读取的核心组件。
 * 该文件提供了从WonderTrader数据存储格式中随机读取各种数据的功能，支持按时间范围、按数量、
 * 按日期等多种读取方式，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 核心实现机制：
 * 
 * 1. 随机数据访问（Random Data Access）：
 *    - 支持按时间范围随机读取数据
 *    - 支持按数量随机读取数据
 *    - 支持按日期随机读取数据
 * 
 * 2. 高效数据管理（Efficient Data Management）：
 *    - 使用内存映射文件技术提高读取性能
 *    - 支持数据缓存和预加载
 *    - 提供数据切片和范围查询功能
 * 
 * 3. 灵活数据格式（Flexible Data Format）：
 *    - 支持多种数据格式的自动识别
 *    - 提供数据格式转换和适配
 *    - 支持复权数据的处理
 * 
 * 主要功能模块：
 * 
 * 1. 按时间范围读取：
 *    - 支持按时间范围读取K线数据
 *    - 支持按时间范围读取Tick数据
 *    - 支持按时间范围读取逐笔数据
 * 
 * 2. 按数量读取：
 *    - 支持按数量读取K线数据
 *    - 支持按数量读取Tick数据
 *    - 支持按数量读取逐笔数据
 * 
 * 3. 按日期读取：
 *    - 支持按日期读取Tick数据
 *    - 支持按日期读取逐笔数据
 *    - 支持按日期读取委托数据
 * 
 * 技术特点：
 * - 使用BoostMappingFile实现高效的文件映射
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
 * - 随机读取需要考虑数据定位性能
 */

#include "WtRdmDtReader.h"                                        // 随机数据读取器头文件

#include "../Includes/WTSVariant.hpp"                            // 变体数据类型
#include "../Share/TimeUtils.hpp"                                 // 时间工具函数
#include "../Share/CodeHelper.hpp"                                // 代码辅助工具
#include "../Share/DLLHelper.hpp"                                 // 动态库辅助工具

#include "../Includes/WTSContractInfo.hpp"                        // 合约信息类
#include "../Includes/IBaseDataMgr.h"                             // 基础数据管理器接口
#include "../Includes/IHotMgr.h"                                 // 热力管理器接口
#include "../Includes/WTSDataDef.hpp"                             // 数据定义
#include "../Includes/WTSSessionInfo.hpp"                        // 交易时段信息类

#include "../WTSUtils/WTSCmpHelper.hpp"                          // 数据压缩辅助工具
#include "../WTSUtils/WTSCfgLoader.h"                            // 配置加载器

#include <rapidjson/document.h>                                  // RapidJSON文档类
namespace rj = rapidjson;                                        // RapidJSON命名空间别名

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                                      // 格式化库

/*!
 * \brief 随机数据读取器日志记录模板函数
 * \tparam Args 可变参数类型
 * \param sink 日志回调接口
 * \param ll 日志级别
 * \param format 格式化字符串
 * \param args 格式化参数
 * 
 * 该函数用于随机数据读取器的日志记录，支持格式化字符串和可变参数。
 * 使用线程本地存储的缓冲区避免多线程竞争，提供高效的日志输出。
 */
template<typename... Args>
inline void pipe_rdmreader_log(IRdmDtReaderSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                                             // 如果日志回调接口为空
		return;                                                   // 直接返回

	static thread_local char buffer[512] = { 0 };                // 线程本地日志缓冲区（512字节）
	fmtutil::format_to(buffer, format, args...);                  // 格式化字符串到缓冲区

	sink->reader_log(ll, buffer);                                // 调用日志回调接口输出日志
}

extern "C"
{
	EXPORT_FLAG IRdmDtReader* createRdmDtReader()
	{
		IRdmDtReader* ret = new WtRdmDtReader();
		return ret;
	}

	EXPORT_FLAG void deleteRdmDtReader(IRdmDtReader* reader)
	{
		if (reader != NULL)
			delete reader;
	}
};

/*
 *	处理块数据
 */
extern bool proc_block_data(std::string& content, bool isBar, bool bKeepHead = true);

/*!
 * \brief WtRdmDtReader构造函数
 * 
 * 初始化随机数据读取器对象，设置基础数据管理器、热力管理器和停止标志为默认值。
 * 构造函数采用初始化列表方式，确保所有成员变量都被正确初始化。
 */
WtRdmDtReader::WtRdmDtReader()
	: _base_data_mgr(NULL)                                    // 基础数据管理器指针初始化为空
	, _hot_mgr(NULL)                                         // 热力管理器指针初始化为空
	, _stopped(false)                                        // 停止标志初始化为false
{
}


/*!
 * \brief WtRdmDtReader析构函数
 * 
 * 清理随机数据读取器对象，设置停止标志并等待检查线程结束。
 * 确保所有资源被正确释放，避免内存泄漏。
 */
WtRdmDtReader::~WtRdmDtReader()
{
	_stopped = true;                                         // 设置停止标志为true
	if (_thrd_check)                                         // 如果检查线程存在
		_thrd_check->join();                                 // 等待线程结束
}

/*!
 * \brief 初始化随机数据读取器
 * \param cfg 配置参数，包含数据存储路径等信息
 * \param sink 数据读取器回调接口
 * 
 * 初始化随机数据读取器，设置基础数据管理器、热力管理器、数据存储路径等。
 * 如果配置中包含复权因子文件路径，则加载复权因子数据。
 * 启动后台检查线程，定期清理未使用的数据块。
 */
void WtRdmDtReader::init(WTSVariant* cfg, IRdmDtReaderSink* sink)
{
	_sink = sink;                                            // 设置日志回调接口

	_base_data_mgr = _sink->get_basedata_mgr();             // 获取基础数据管理器
	_hot_mgr = _sink->get_hot_mgr();                        // 获取热力管理器

	if (cfg == NULL)                                         // 如果配置为空
		return ;                                             // 直接返回

	_base_dir = cfg->getCString("path");                    // 获取数据存储基础路径
	_base_dir = StrUtil::standardisePath(_base_dir);        // 标准化路径格式

	bool bAdjLoaded = false;                                // 复权因子加载标志
	
	if (!bAdjLoaded && cfg->has("adjfactor"))               // 如果配置中包含复权因子文件路径
		loadStkAdjFactorsFromFile(cfg->getCString("adjfactor")); // 加载复权因子数据

	// 启动后台检查线程，定期清理未使用的数据块
	_thrd_check.reset(new StdThread([this]() {
		while(!_stopped)                                      // 当未停止时循环
		{
			std::this_thread::sleep_for(std::chrono::seconds(5)); // 休眠5秒
			uint64_t now = TimeUtils::getLocalTimeNow();      // 获取当前时间

			// 清理实时Tick数据块
			for(auto& m : _rt_tick_map)
			{
				// 如果5分钟之内没有访问，则释放掉
				TickBlockPair& tPair = (TickBlockPair&)m.second;
				if(now > tPair._last_time + 300000 && tPair._block != NULL)
				{	
					StdUniqueLock lock(*tPair._mtx);          // 获取互斥锁
					tPair._block = NULL;                      // 清空数据块指针
					tPair._file.reset();                      // 重置文件映射
				}
			}

			// 清理实时委托队列数据块
			for (auto& m : _rt_ordque_map)
			{
				// 如果5分钟之内没有访问，则释放掉
				OrdQueBlockPair& tPair = (OrdQueBlockPair&)m.second;
				if (now > tPair._last_time + 300000 && tPair._block != NULL)
				{
					StdUniqueLock lock(*tPair._mtx);          // 获取互斥锁
					tPair._block = NULL;                      // 清空数据块指针
					tPair._file.reset();                      // 重置文件映射
				}
			}

			// 清理实时逐笔委托数据块
			for (auto& m : _rt_orddtl_map)
			{
				// 如果5分钟之内没有访问，则释放掉
				OrdDtlBlockPair& tPair = (OrdDtlBlockPair&)m.second;
				if (now > tPair._last_time + 300000 && tPair._block != NULL)
				{
					StdUniqueLock lock(*tPair._mtx);          // 获取互斥锁
					tPair._block = NULL;                      // 清空数据块指针
					tPair._file.reset();                      // 重置文件映射
				}
			}

			// 清理实时逐笔成交数据块
			for (auto& m : _rt_trans_map)
			{
				// 如果5分钟之内没有访问，则释放掉
				TransBlockPair& tPair = (TransBlockPair&)m.second;
				if (now > tPair._last_time + 300000 && tPair._block != NULL)
				{
					StdUniqueLock lock(*tPair._mtx);          // 获取互斥锁
					tPair._block = NULL;                      // 清空数据块指针
					tPair._file.reset();                      // 重置文件映射
				}
			}

			// 清理实时1分钟K线数据块
			for (auto& m : _rt_min1_map)
			{
				// 如果5分钟之内没有访问，则释放掉
				RTKlineBlockPair& tPair = (RTKlineBlockPair&)m.second;
				if (now > tPair._last_time + 300000 && tPair._block != NULL)
				{
					StdUniqueLock lock(*tPair._mtx);          // 获取互斥锁
					tPair._block = NULL;                      // 清空数据块指针
					tPair._file.reset();                      // 重置文件映射
				}
			}

			// 清理实时5分钟K线数据块
			for (auto& m : _rt_min5_map)
			{
				// 如果5分钟之内没有访问，则释放掉
				RTKlineBlockPair& tPair = (RTKlineBlockPair&)m.second;
				if (now > tPair._last_time + 300000 && tPair._block != NULL)
				{
					StdUniqueLock lock(*tPair._mtx);          // 获取互斥锁
					tPair._block = NULL;                      // 清空数据块指针
					tPair._file.reset();                      // 重置文件映射
				}
			}
		}
	}));
}


/*!
 * \brief 从文件加载股票复权因子数据
 * \param adjfile 复权因子文件路径
 * \return 是否加载成功
 * 
 * 从JSON格式的复权因子文件中加载股票复权因子数据，支持前复权和后复权。
 * 复权因子用于股票价格调整，确保历史数据的连续性。
 */
bool WtRdmDtReader::loadStkAdjFactorsFromFile(const char* adjfile)
{
	if (!StdFile::exists(adjfile))                           // 如果复权因子文件不存在
	{
		pipe_rdmreader_log(_sink, LL_ERROR, "Adjusting factors file {} not exists", adjfile);
		return false;                                        // 返回失败
	}

	WTSVariant* doc = WTSCfgLoader::load_from_file(adjfile); // 加载JSON配置文件
	if (doc == NULL)                                         // 如果加载失败
	{
		pipe_rdmreader_log(_sink, LL_ERROR, "Loading adjusting factors file {} failed", adjfile);
		return false;                                        // 返回失败
	}

	uint32_t stk_cnt = 0;                                    // 股票数量计数器
	uint32_t fct_cnt = 0;                                    // 复权因子数量计数器
	
	// 遍历所有交易所
	for (const std::string& exchg : doc->memberNames())
	{
		WTSVariant* itemExchg = doc->get(exchg);             // 获取交易所数据
		// 遍历交易所下的所有股票代码
		for (const std::string& code : itemExchg->memberNames())
		{
			WTSVariant* ayFacts = itemExchg->get(code);       // 获取股票复权因子数组
			if (!ayFacts->isArray())                          // 如果不是数组格式
				continue;                                     // 跳过

			/*
			 *	By Wesley @ 2021.12.21
			 *	先检查code的格式是不是包含PID，如STK.600000
			 *	如果包含PID，则直接格式化，如果不包含，则强制为STK
			 */
			bool bHasPID = (code.find('.') != std::string::npos); // 检查是否包含产品ID

			std::string key;                                    // 复权因子键
			if (bHasPID)                                        // 如果包含产品ID
				key = fmt::format("{}.{}", exchg, code);        // 直接格式化
			else                                                // 如果不包含产品ID
				key = fmt::format("{}.STK.{}", exchg, code);    // 强制添加STK前缀

			stk_cnt++;                                          // 股票数量加1

			AdjFactorList& fctrLst = _adj_factors[key];         // 获取复权因子列表
			// 遍历复权因子数组
			for (uint32_t i = 0; i < ayFacts->size(); i++)
			{
				WTSVariant* fItem = ayFacts->get(i);            // 获取复权因子项
				AdjFactor adjFact;                             // 创建复权因子对象
				adjFact._date = fItem->getUInt32("date");       // 获取复权日期
				adjFact._factor = fItem->getDouble("factor");   // 获取复权因子值

				fctrLst.emplace_back(adjFact);                  // 添加到复权因子列表
				fct_cnt++;                                      // 复权因子数量加1
			}

			// 一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
			AdjFactor adjFact;                                 // 创建默认复权因子
			adjFact._date = 19900101;                          // 设置默认日期为1990年1月1日
			adjFact._factor = 1;                               // 设置默认复权因子为1
			fctrLst.emplace_back(adjFact);                     // 添加到复权因子列表

			// 按日期排序复权因子列表
			std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {
				return left._date < right._date;                // 按日期升序排序
			});
		}
	}

	pipe_rdmreader_log(_sink, LL_INFO, "{} adjusting factors of {} tickers loaded", fct_cnt, stk_cnt);
	doc->release();                                          // 释放JSON文档内存
	return true;                                             // 返回成功
}

/*!
 * \brief 按日期读取Tick数据切片
 * \param stdCode 标准合约代码
 * \param uDate 交易日期（YYYYMMDD格式）
 * \return Tick数据切片指针，失败返回NULL
 * 
 * 根据指定日期读取Tick数据，支持历史数据和实时数据。
 * 对于期货合约，会自动处理主力合约切换。
 */
WTSTickSlice* WtRdmDtReader::readTickSliceByDate(const char* stdCode, uint32_t uDate )
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID

	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日
	bool isToday = (uDate == curTDate);                      // 判断是否为今天

	// 这里改成小于等于，主要针对盘后读取的情况
	// 如果已经做了收盘作业，实时数据就读不到了
	if (uDate <= curTDate)                                    // 如果请求日期小于等于当前交易日
	{
		std::string curCode = cInfo._code;                    // 当前合约代码
		std::string hotCode;                                 // 热力合约代码
		if (commInfo->isFuture())                            // 如果是期货合约
		{
			const char* ruleTag = cInfo._ruletag;            // 获取规则标签
			if(strlen(ruleTag) > 0)                          // 如果有规则标签
			{
				curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, uDate); // 获取自定义原始代码
				pipe_rdmreader_log(_sink, LL_INFO, "{} contract on {} confirmed with rule {}: {} -> {}", ruleTag, uDate, stdCode, curCode.c_str());
				hotCode = cInfo._product;                    // 设置热力代码
				hotCode += "_";                              // 添加下划线
				hotCode += ruleTag;                          // 添加规则标签
			}
		}

		std::string key = fmt::format("{}-{}", stdCode, uDate); // 生成缓存键

		auto it = _his_tick_map.find(key);                   // 查找历史Tick数据
		bool bHasHisTick = (it != _his_tick_map.end());      // 判断是否已有历史数据
		if (!bHasHisTick)                                    // 如果没有历史数据
		{
			// 尝试加载历史Tick数据文件
			for (;;)
			{
				std::string filename;                          // 文件名
				bool bHitHot = false;                          // 是否命中热力合约
				if (!hotCode.empty())                          // 如果有热力代码
				{
					std::stringstream ss;                      // 字符串流
					ss << _base_dir << "his/ticks/" << cInfo._exchg << "/" << uDate << "/" << hotCode << ".dsb"; // 构建热力合约文件路径
					filename = ss.str();                       // 获取文件路径
					if (StdFile::exists(filename.c_str()))    // 如果热力合约文件存在
					{
						bHitHot = true;                       // 标记命中热力合约
					}
				}

				if (!bHitHot)                                  // 如果没有命中热力合约
				{
					std::stringstream ss;                      // 字符串流
					ss << _base_dir << "his/ticks/" << cInfo._exchg << "/" << uDate << "/" << curCode << ".dsb"; // 构建普通合约文件路径
					filename = ss.str();                       // 获取文件路径
					if (!StdFile::exists(filename.c_str()))    // 如果文件不存在
					{
						break;                                 // 跳出循环
					}
				}

				HisTBlockPair& tBlkPair = _his_tick_map[key];  // 获取历史Tick数据块对
				StdFile::read_file_content(filename.c_str(), tBlkPair._buffer); // 读取文件内容到缓冲区
				if (tBlkPair._buffer.size() < sizeof(HisTickBlock)) // 如果文件大小小于历史Tick块大小
				{
					pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of tick data file {} failed", filename.c_str());
					tBlkPair._buffer.clear();                  // 清空缓冲区
					break;                                     // 跳出循环
				}

				proc_block_data(tBlkPair._buffer, false, true); // 处理块数据
				tBlkPair._block = (HisTickBlock*)tBlkPair._buffer.c_str(); // 设置数据块指针
				bHasHisTick = true;                           // 标记已有历史Tick数据
				break;                                        // 跳出循环
			}
		}

		// 处理历史Tick数据
		while (bHasHisTick)
		{
			HisTBlockPair& tBlkPair = _his_tick_map[key];     // 获取历史Tick数据块对
			if (tBlkPair._block == NULL)                       // 如果数据块为空
				break;                                        // 跳出循环

			HisTickBlock* tBlock = tBlkPair._block;           // 获取历史Tick数据块指针

			uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisTickBlock)) / sizeof(WTSTickStruct); // 计算Tick数量
			if (tcnt <= 0)                                     // 如果Tick数量小于等于0
				break;                                        // 跳出循环

			WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks, tcnt); // 创建Tick数据切片
			return slice;                                     // 返回切片

			break;                                            // 跳出循环
		}
	}
	
	// 处理实时Tick数据（今天的数据）
	while(isToday)
	{
		std::string curCode = cInfo._code;                    // 当前合约代码
		if(commInfo->isFuture())                              // 如果是期货合约
		{
			const char* ruleTag = cInfo._ruletag;             // 获取规则标签
			if (strlen(ruleTag) > 0)                          // 如果有规则标签
				curCode = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), curTDate); // 获取自定义原始代码
			//else if (cInfo.isHot())                          // 如果是主力合约
			//	curCode = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, curTDate);
			//else if (cInfo.isSecond())                       // 如果是次主力合约
			//	curCode = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, curTDate);
		}
		

		TickBlockPair* tPair = getRTTickBlock(cInfo._exchg, curCode.c_str()); // 获取实时Tick数据块对
		if (tPair == NULL || tPair->_block->_size == 0)       // 如果数据块为空或大小为0
			break;                                            // 跳出循环

		StdUniqueLock lock(*tPair->_mtx);                      // 获取互斥锁
		RTTickBlock* tBlock = tPair->_block;                   // 获取实时Tick数据块指针
		
		WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks, tBlock->_size); // 创建Tick数据切片
		return slice;                                         // 返回切片
	}

	return NULL;
}

/*!
 * \brief 按时间范围读取Tick数据切片
 * \param stdCode 标准合约代码
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳，默认为0）
 * \return Tick数据切片指针，失败返回NULL
 * 
 * 根据指定时间范围读取Tick数据，支持跨交易日的数据读取。
 * 自动处理期货合约的主力切换，支持历史数据和实时数据的混合读取。
 */
WTSTickSlice* WtRdmDtReader::readTickSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID

	pipe_rdmreader_log(_sink, LL_DEBUG, "Reading ticks of {} between {} and {}", stdCode, stime, etime);

	WTSSessionInfo* sInfo = commInfo->getSessionInfo();       // 获取交易时段信息

	uint32_t rDate, rTime, rSecs;                            // 结束时间的日期、时间、秒数
	//20190807124533900
	rDate = (uint32_t)(etime / 1000000000);                  // 提取结束日期
	rTime = (uint32_t)(etime % 1000000000) / 100000;         // 提取结束时间
	rSecs = (uint32_t)(etime % 100000);                      // 提取结束秒数

	uint32_t lDate, lTime, lSecs;                            // 开始时间的日期、时间、秒数
	//20190807124533900
	lDate = (uint32_t)(stime / 1000000000);                  // 提取开始日期
	lTime = (uint32_t)(stime % 1000000000) / 100000;         // 提取开始时间
	lSecs = (uint32_t)(stime % 100000);                      // 提取开始秒数

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日
	uint32_t beginTDate = _base_data_mgr->calcTradingDate(stdPID, lDate, lTime, false); // 计算开始交易日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日

	bool hasToday = (endTDate >= curTDate);                  // 判断是否包含今天

	WTSTickSlice* slice = WTSTickSlice::create(stdCode, NULL, 0); // 创建空的Tick数据切片

	WTSTickStruct sTick;                                     // 开始时间Tick结构
	sTick.action_date = lDate;                               // 设置开始日期
	sTick.action_time = lTime * 100000 + lSecs;             // 设置开始时间
	
	uint32_t nowTDate = beginTDate;                           // 当前处理交易日
	// 遍历历史交易日
	while(nowTDate < curTDate)
	{
		std::string curCode = cInfo._code;                   // 当前合约代码
		std::string hotCode;                                 // 热力合约代码
		if(commInfo->isFuture())                             // 如果是期货合约
		{
			const char* ruleTag = cInfo._ruletag;            // 获取规则标签
			if (strlen(ruleTag) > 0)                         // 如果有规则标签
			{
				curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, nowTDate); // 获取自定义原始代码

				pipe_rdmreader_log(_sink, LL_INFO, "{} contract on {} confirmed: {} -> {}", ruleTag, curTDate, stdCode, curCode.c_str());
				hotCode = cInfo._product;                    // 设置热力代码
				hotCode += "_";                              // 添加下划线
				hotCode += ruleTag;                          // 添加规则标签
			}
		}
		
		std::string key = fmt::format("{}-{}", stdCode, nowTDate); // 生成缓存键

		auto it = _his_tick_map.find(key);                   // 查找历史Tick数据
		bool bHasHisTick = (it != _his_tick_map.end());      // 判断是否已有历史数据
		if(!bHasHisTick)                                    // 如果没有历史数据
		{
			// 尝试加载历史Tick数据文件
			for(;;)
			{
				std::string filename;                        // 文件名
				bool bHitHot = false;                        // 是否命中热力合约
				if (!hotCode.empty())                        // 如果有热力代码
				{
					std::stringstream ss;                    // 字符串流
					ss << _base_dir << "his/ticks/" << cInfo._exchg << "/" << nowTDate << "/" << hotCode << ".dsb"; // 构建热力合约文件路径
					filename = ss.str();                     // 获取文件路径
					if (StdFile::exists(filename.c_str()))  // 如果热力合约文件存在
					{
						bHitHot = true;                     // 标记命中热力合约
					}
				}

				if (!bHitHot)                               // 如果没有命中热力合约
				{
					std::stringstream ss;                    // 字符串流
					ss << _base_dir << "his/ticks/" << cInfo._exchg << "/" << nowTDate << "/" << curCode << ".dsb"; // 构建普通合约文件路径
					filename = ss.str();                     // 获取文件路径
					pipe_rdmreader_log(_sink, LL_DEBUG, "Reading ticks from {}...", filename);
					if (!StdFile::exists(filename.c_str())) // 如果文件不存在
					{
						break;                              // 跳出循环
					}
				}

				HisTBlockPair& tBlkPair = _his_tick_map[key]; // 获取历史Tick数据块对
				StdFile::read_file_content(filename.c_str(), tBlkPair._buffer); // 读取文件内容到缓冲区
				if (tBlkPair._buffer.size() < sizeof(HisTickBlock)) // 如果文件大小小于历史Tick块大小
				{
					pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of tick data file {} failed", filename.c_str());
					tBlkPair._buffer.clear();                // 清空缓冲区
					break;                                  // 跳出循环
				}

				proc_block_data(tBlkPair._buffer, false, true); // 处理块数据
				tBlkPair._block = (HisTickBlock*)tBlkPair._buffer.c_str(); // 设置数据块指针
				bHasHisTick = true;                         // 标记已有历史Tick数据
				break;                                      // 跳出循环
			}
		}
		
		// 处理历史Tick数据
		while(bHasHisTick)
		{
			// 比较时间的对象
			WTSTickStruct eTick;                             // 结束时间Tick结构
			if(nowTDate == endTDate)                         // 如果是结束交易日
			{
				eTick.action_date = rDate;                   // 设置结束日期
				eTick.action_time = rTime * 100000 + rSecs;  // 设置结束时间
			}
			else                                             // 如果不是结束交易日
			{
				eTick.action_date = nowTDate;                // 设置当前交易日
				eTick.action_time = sInfo->getCloseTime() * 100000 + 59999; // 设置收盘时间
			}

			HisTBlockPair& tBlkPair = _his_tick_map[key];    // 获取历史Tick数据块对
			if (tBlkPair._block == NULL)                      // 如果数据块为空
				break;                                       // 跳出循环

			HisTickBlock* tBlock = tBlkPair._block;          // 获取历史Tick数据块指针

			uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisTickBlock)) / sizeof(WTSTickStruct); // 计算Tick数量
			if (tcnt <= 0)                                   // 如果Tick数量小于等于0
				break;                                       // 跳出循环

			// 使用二分查找定位结束时间位置
			WTSTickStruct* pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + (tcnt - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t eIdx = pTick - tBlock->_ticks;       // 计算结束索引
			if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time) // 如果定位的时间大于等于目标时间
			{
				pTick--;                                     // 回退一个位置
				eIdx--;                                      // 索引减1
			}

			if (beginTDate != nowTDate)                      // 如果开始交易日与当前交易日不同
			{
				// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
				//WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks, eIdx + 1);
				//ayTicks->append(slice, false);
				slice->appendBlock(tBlock->_ticks, eIdx + 1); // 追加整个数据块
			}
			else                                             // 如果交易日相同
			{
				// 如果交易日相同，则查找起始的位置
				pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + eIdx, sTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
					if (a.action_date != b.action_date)      // 如果日期不同
						return a.action_date < b.action_date; // 按日期比较
					else                                      // 如果日期相同
						return a.action_time < b.action_time; // 按时间比较
				});

				std::size_t sIdx = pTick - tBlock->_ticks;   // 计算开始索引
				//WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks + sIdx, eIdx - sIdx + 1);
				//ayTicks->append(slice, false);
				slice->appendBlock(tBlock->_ticks + sIdx, eIdx - sIdx + 1); // 追加指定范围的数据
			}

			break;                                           // 跳出循环
		}
		
		nowTDate = TimeUtils::getNextDate(nowTDate);         // 获取下一个交易日
	}

	// 处理实时Tick数据（今天的数据）
	while(hasToday)
	{
		std::string curCode = cInfo._code;                   // 当前合约代码
		if (commInfo->isFuture())                            // 如果是期货合约
		{
			const char* ruleTag = cInfo._ruletag;             // 获取规则标签
			if (strlen(ruleTag) > 0)                         // 如果有规则标签
				curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, curTDate); // 获取自定义原始代码
		}

		TickBlockPair* tPair = getRTTickBlock(cInfo._exchg, curCode.c_str()); // 获取实时Tick数据块对
		if (tPair == NULL || tPair->_block->_size == 0)       // 如果数据块为空或大小为0
			break;                                            // 跳出循环

		StdUniqueLock lock(*tPair->_mtx);                     // 获取互斥锁
		RTTickBlock* tBlock = tPair->_block;                   // 获取实时Tick数据块指针
		WTSTickStruct eTick;                                  // 结束时间Tick结构
		if (curTDate == endTDate)                             // 如果当前交易日是结束交易日
		{
			eTick.action_date = rDate;                        // 设置结束日期
			eTick.action_time = rTime * 100000 + rSecs;       // 设置结束时间
		}
		else                                                  // 如果不是结束交易日
		{
			eTick.action_date = curTDate;                     // 设置当前交易日
			eTick.action_time = sInfo->getCloseTime() * 100000 + 59999; // 设置收盘时间
		}

		// 使用二分查找定位结束时间位置
		WTSTickStruct* pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + (tBlock->_size - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pTick - tBlock->_ticks;           // 计算结束索引

		// 如果光标定位的tick时间比目标时间大, 则全部回退一个
		if (pTick->action_date > eTick.action_date || pTick->action_time > eTick.action_time) // 如果定位的时间大于目标时间
		{
			pTick--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}

		if (beginTDate != curTDate)                          // 如果开始交易日与当前交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			//WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks, eIdx + 1);
			//ayTicks->append(slice, false);
			slice->appendBlock(tBlock->_ticks, eIdx + 1);     // 追加整个数据块
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + eIdx, sTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pTick - tBlock->_ticks;       // 计算开始索引
			//WTSTickSlice* slice = WTSTickSlice::create(stdCode, tBlock->_ticks + sIdx, eIdx - sIdx + 1);
			//ayTicks->append(slice, false);
			slice->appendBlock(tBlock->_ticks + sIdx, eIdx - sIdx + 1); // 追加指定范围的数据
		}
		break;                                               // 跳出循环
	}

	return slice;                                            // 返回数据切片
}

/*!
 * \brief 按时间范围读取委托队列数据切片
 * \param stdCode 标准合约代码
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳，默认为0）
 * \return 委托队列数据切片指针，失败返回NULL
 * 
 * 根据指定时间范围读取委托队列数据，支持跨交易日的数据读取。
 * 委托队列数据包含买卖盘口信息，用于分析市场深度。
 */
WTSOrdQueSlice* WtRdmDtReader::readOrdQueSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID

	uint32_t rDate, rTime, rSecs;                            // 结束时间的日期、时间、秒数
	//20190807124533900
	rDate = (uint32_t)(etime / 1000000000);                  // 提取结束日期
	rTime = (uint32_t)(etime % 1000000000) / 100000;         // 提取结束时间
	rSecs = (uint32_t)(etime % 100000);                      // 提取结束秒数

	uint32_t lDate, lTime, lSecs;                            // 开始时间的日期、时间、秒数
	//20190807124533900
	lDate = (uint32_t)(stime / 1000000000);                  // 提取开始日期
	lTime = (uint32_t)(stime % 1000000000) / 100000;         // 提取开始时间
	lSecs = (uint32_t)(stime % 100000);                      // 提取开始秒数

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日
	uint32_t beginTDate = _base_data_mgr->calcTradingDate(stdPID, lDate, lTime, false); // 计算开始交易日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日

	bool isToday = (endTDate == curTDate);                   // 判断是否为今天

	std::string curCode = cInfo._code;                       // 当前合约代码
	if (commInfo->isFuture())                                // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                 // 获取规则标签
		if (strlen(ruleTag) > 0)                             // 如果有规则标签
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate); // 获取自定义原始代码
	}

	// 比较时间的对象
	WTSOrdQueStruct eTick;                                   // 结束时间委托队列结构
	eTick.action_date = rDate;                               // 设置结束日期
	eTick.action_time = rTime * 100000 + rSecs;               // 设置结束时间

	WTSOrdQueStruct sTick;                                   // 开始时间委托队列结构
	sTick.action_date = lDate;                               // 设置开始日期
	sTick.action_time = lTime * 100000 + lSecs;               // 设置开始时间

	// 处理实时委托队列数据（今天的数据）
	if (isToday)
	{
		OrdQueBlockPair* tPair = getRTOrdQueBlock(cInfo._exchg, curCode.c_str()); // 获取实时委托队列数据块对
		if (tPair == NULL)                                    // 如果数据块为空
			return NULL;                                      // 返回空指针

		RTOrdQueBlock* rtBlock = tPair->_block;               // 获取实时委托队列数据块指针

		// 使用二分查找定位结束时间位置
		WTSOrdQueStruct* pItem = std::lower_bound(rtBlock->_queues, rtBlock->_queues + (rtBlock->_size - 1), eTick, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pItem - rtBlock->_queues;          // 计算结束索引

		// 如果光标定位的tick时间比目标时间大, 则全部回退一个
		if (pItem->action_date > eTick.action_date || pItem->action_time > eTick.action_time) // 如果定位的时间大于目标时间
		{
			pItem--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}

		if (beginTDate != endTDate)                          // 如果开始交易日与结束交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			WTSOrdQueSlice* slice = WTSOrdQueSlice::create(stdCode, rtBlock->_queues, eIdx + 1); // 创建委托队列数据切片
			return slice;                                    // 返回切片
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pItem = std::lower_bound(rtBlock->_queues, rtBlock->_queues + eIdx, sTick, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pItem - rtBlock->_queues;     // 计算开始索引
			WTSOrdQueSlice* slice = WTSOrdQueSlice::create(stdCode, rtBlock->_queues + sIdx, eIdx - sIdx + 1); // 创建指定范围的委托队列数据切片
			return slice;                                    // 返回切片
		}
	}
	else                                                      // 如果不是今天（历史数据）
	{
		std::string key = fmt::format("{}-{}", stdCode, endTDate); // 生成缓存键

		auto it = _his_ordque_map.find(key);                  // 查找历史委托队列数据
		if (it == _his_ordque_map.end())                     // 如果没有找到历史数据
		{
			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/queue/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb"; // 构建历史委托队列文件路径
			std::string filename = ss.str();                 // 获取文件路径
			if (!StdFile::exists(filename.c_str()))         // 如果文件不存在
				return NULL;                                 // 返回空指针

			HisOrdQueBlockPair& hisBlkPair = _his_ordque_map[key]; // 获取历史委托队列数据块对
			StdFile::read_file_content(filename.c_str(), hisBlkPair._buffer); // 读取文件内容到缓冲区
			if (hisBlkPair._buffer.size() < sizeof(HisOrdQueBlockV2)) // 如果文件大小小于历史委托队列块V2大小
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of orderqueue data file {} failed", filename.c_str());
				hisBlkPair._buffer.clear();                  // 清空缓冲区
				return NULL;                                 // 返回空指针
			}

			HisOrdQueBlockV2* tBlockV2 = (HisOrdQueBlockV2*)hisBlkPair._buffer.c_str(); // 获取历史委托队列块V2指针

			if (hisBlkPair._buffer.size() != (sizeof(HisOrdQueBlockV2) + tBlockV2->_size)) // 如果文件大小不匹配
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of orderqueue data file {} failed", filename.c_str());
				return NULL;                                 // 返回空指针
			}

			// 需要解压
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (std::size_t)tBlockV2->_size); // 解压数据

			// 将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			hisBlkPair._buffer.resize(sizeof(HisOrdQueBlock)); // 调整缓冲区大小
			hisBlkPair._buffer.append(buf);                   // 追加解压后的数据
			tBlockV2->_version = BLOCK_VERSION_RAW;          // 设置版本为原始版本

			hisBlkPair._block = (HisOrdQueBlock*)hisBlkPair._buffer.c_str(); // 设置数据块指针
		}

		HisOrdQueBlockPair& tBlkPair = _his_ordque_map[key]; // 获取历史委托队列数据块对
		if (tBlkPair._block == NULL)                          // 如果数据块为空
			return NULL;                                      // 返回空指针

		HisOrdQueBlock* tBlock = tBlkPair._block;            // 获取历史委托队列数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisOrdQueBlock)) / sizeof(WTSOrdQueStruct); // 计算委托队列数量
		if (tcnt <= 0)                                        // 如果委托队列数量小于等于0
			return NULL;                                      // 返回空指针

		// 使用二分查找定位结束时间位置
		WTSOrdQueStruct* pItem = std::lower_bound(tBlock->_items, tBlock->_items + (tcnt - 1), eTick, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pItem - tBlock->_items;            // 计算结束索引
		if (pItem->action_date > eTick.action_date || pItem->action_time >= eTick.action_time) // 如果定位的时间大于等于目标时间
		{
			pItem--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}


		if (beginTDate != endTDate)                          // 如果开始交易日与结束交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			WTSOrdQueSlice* slice = WTSOrdQueSlice::create(stdCode, tBlock->_items, eIdx + 1); // 创建委托队列数据切片
			return slice;                                    // 返回切片
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pItem = std::lower_bound(tBlock->_items, tBlock->_items + eIdx, sTick, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pItem - tBlock->_items;        // 计算开始索引
			WTSOrdQueSlice* slice = WTSOrdQueSlice::create(stdCode, tBlock->_items + sIdx, eIdx - sIdx + 1); // 创建指定范围的委托队列数据切片
			return slice;                                    // 返回切片
		}
	}
}

/*!
 * \brief 按时间范围读取逐笔委托数据切片
 * \param stdCode 标准合约代码
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳，默认为0）
 * \return 逐笔委托数据切片指针，失败返回NULL
 * 
 * 根据指定时间范围读取逐笔委托数据，支持跨交易日的数据读取。
 * 逐笔委托数据包含每笔委托的详细信息，用于分析委托行为。
 */
WTSOrdDtlSlice* WtRdmDtReader::readOrdDtlSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID

	uint32_t rDate, rTime, rSecs;                            // 结束时间的日期、时间、秒数
	//20190807124533900
	rDate = (uint32_t)(etime / 1000000000);                  // 提取结束日期
	rTime = (uint32_t)(etime % 1000000000) / 100000;         // 提取结束时间
	rSecs = (uint32_t)(etime % 100000);                      // 提取结束秒数

	uint32_t lDate, lTime, lSecs;                            // 开始时间的日期、时间、秒数
	//20190807124533900
	lDate = (uint32_t)(stime / 1000000000);                  // 提取开始日期
	lTime = (uint32_t)(stime % 1000000000) / 100000;         // 提取开始时间
	lSecs = (uint32_t)(stime % 100000);                      // 提取开始秒数

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日
	uint32_t beginTDate = _base_data_mgr->calcTradingDate(stdPID, lDate, lTime, false); // 计算开始交易日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日

	bool isToday = (endTDate == curTDate);                   // 判断是否为今天

	std::string curCode = cInfo._code;                        // 当前合约代码
	if (commInfo->isFuture())                                // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                 // 获取规则标签
		if (strlen(ruleTag) > 0)                             // 如果有规则标签
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate); // 获取自定义原始代码
	}

	// 比较时间的对象
	WTSOrdDtlStruct eTick;                                   // 结束时间逐笔委托结构
	eTick.action_date = rDate;                                // 设置结束日期
	eTick.action_time = rTime * 100000 + rSecs;               // 设置结束时间

	WTSOrdDtlStruct sTick;                                   // 开始时间逐笔委托结构
	sTick.action_date = lDate;                                // 设置开始日期
	sTick.action_time = lTime * 100000 + lSecs;                // 设置开始时间

	if (isToday)
	{
		OrdDtlBlockPair* tPair = getRTOrdDtlBlock(cInfo._exchg, curCode.c_str()); // 获取实时逐笔委托数据块对
		if (tPair == NULL)                                    // 如果数据块为空
			return NULL;                                      // 返回空指针

		RTOrdDtlBlock* rtBlock = tPair->_block;               // 获取实时逐笔委托数据块指针

		// 使用二分查找定位结束时间位置
		WTSOrdDtlStruct* pItem = std::lower_bound(rtBlock->_details, rtBlock->_details + (rtBlock->_size - 1), eTick, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pItem - rtBlock->_details;         // 计算结束索引

		// 如果光标定位的tick时间比目标时间大, 则全部回退一个
		if (pItem->action_date > eTick.action_date || pItem->action_time > eTick.action_time) // 如果定位的时间大于目标时间
		{
			pItem--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}

		if (beginTDate != endTDate)                          // 如果开始交易日与结束交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			WTSOrdDtlSlice* slice = WTSOrdDtlSlice::create(stdCode, rtBlock->_details, eIdx + 1); // 创建逐笔委托数据切片
			return slice;                                    // 返回切片
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pItem = std::lower_bound(rtBlock->_details, rtBlock->_details + eIdx, sTick, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pItem - rtBlock->_details;     // 计算开始索引
			WTSOrdDtlSlice* slice = WTSOrdDtlSlice::create(stdCode, rtBlock->_details + sIdx, eIdx - sIdx + 1); // 创建指定范围的逐笔委托数据切片
			return slice;                                    // 返回切片
		}
	}
	else                                                      // 如果不是今天（历史数据）
	{
		std::string key = fmt::format("{}-{}", stdCode, endTDate); // 生成缓存键

		auto it = _his_ordque_map.find(key);                  // 查找历史逐笔委托数据
		if (it == _his_ordque_map.end())                     // 如果没有找到历史数据
		{
			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/orders/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb"; // 构建历史逐笔委托文件路径
			std::string filename = ss.str();                 // 获取文件路径
			if (!StdFile::exists(filename.c_str()))         // 如果文件不存在
				return NULL;                                 // 返回空指针

			HisOrdDtlBlockPair& hisBlkPair = _his_orddtl_map[key]; // 获取历史逐笔委托数据块对
			StdFile::read_file_content(filename.c_str(), hisBlkPair._buffer); // 读取文件内容到缓冲区
			if (hisBlkPair._buffer.size() < sizeof(HisOrdDtlBlockV2)) // 如果文件大小小于历史逐笔委托块V2大小
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of orderdetail data file {} failed", filename.c_str());
				hisBlkPair._buffer.clear();                  // 清空缓冲区
				return NULL;                                // 返回空指针
			}

			HisOrdDtlBlockV2* tBlockV2 = (HisOrdDtlBlockV2*)hisBlkPair._buffer.c_str(); // 获取历史逐笔委托块V2指针

			if (hisBlkPair._buffer.size() != (sizeof(HisOrdDtlBlockV2) + tBlockV2->_size)) // 如果文件大小不匹配
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of orderdetail data file {} failed", filename.c_str());
				return NULL;                                 // 返回空指针
			}

			// 需要解压
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (std::size_t)tBlockV2->_size); // 解压数据

			// 将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			hisBlkPair._buffer.resize(sizeof(HisOrdDtlBlock)); // 调整缓冲区大小
			hisBlkPair._buffer.append(buf);                   // 追加解压后的数据
			tBlockV2->_version = BLOCK_VERSION_RAW;          // 设置版本为原始版本

			hisBlkPair._block = (HisOrdDtlBlock*)hisBlkPair._buffer.c_str(); // 设置数据块指针
		}

		HisOrdDtlBlockPair& tBlkPair = _his_orddtl_map[key]; // 获取历史逐笔委托数据块对
		if (tBlkPair._block == NULL)                          // 如果数据块为空
			return NULL;                                      // 返回空指针

		HisOrdDtlBlock* tBlock = tBlkPair._block;            // 获取历史逐笔委托数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisOrdDtlBlock)) / sizeof(WTSOrdDtlStruct); // 计算逐笔委托数量
		if (tcnt <= 0)                                        // 如果逐笔委托数量小于等于0
			return NULL;                                      // 返回空指针

		// 使用二分查找定位结束时间位置
		WTSOrdDtlStruct* pItem = std::lower_bound(tBlock->_items, tBlock->_items + (tcnt - 1), eTick, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pItem - tBlock->_items;            // 计算结束索引
		if (pItem->action_date > eTick.action_date || pItem->action_time >= eTick.action_time) // 如果定位的时间大于等于目标时间
		{
			pItem--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}

		if (beginTDate != endTDate)                          // 如果开始交易日与结束交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			WTSOrdDtlSlice* slice = WTSOrdDtlSlice::create(stdCode, tBlock->_items, eIdx + 1); // 创建逐笔委托数据切片
			return slice;                                    // 返回切片
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pItem = std::lower_bound(tBlock->_items, tBlock->_items + eIdx, sTick, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pItem - tBlock->_items;        // 计算开始索引
			WTSOrdDtlSlice* slice = WTSOrdDtlSlice::create(stdCode, tBlock->_items + sIdx, eIdx - sIdx + 1); // 创建指定范围的逐笔委托数据切片
			return slice;                                    // 返回切片
		}
	}
}

/*!
 * \brief 按时间范围读取逐笔成交数据切片
 * \param stdCode 标准合约代码
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳，默认为0）
 * \return 逐笔成交数据切片指针，失败返回NULL
 * 
 * 根据指定时间范围读取逐笔成交数据，支持跨交易日的数据读取。
 * 逐笔成交数据包含每笔成交的详细信息，用于分析成交行为。
 */
WTSTransSlice* WtRdmDtReader::readTransSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID

	uint32_t rDate, rTime, rSecs;                            // 结束时间的日期、时间、秒数
	//20190807124533900
	rDate = (uint32_t)(etime / 1000000000);                  // 提取结束日期
	rTime = (uint32_t)(etime % 1000000000) / 100000;         // 提取结束时间
	rSecs = (uint32_t)(etime % 100000);                      // 提取结束秒数

	uint32_t lDate, lTime, lSecs;                            // 开始时间的日期、时间、秒数
	//20190807124533900
	lDate = (uint32_t)(stime / 1000000000);                  // 提取开始日期
	lTime = (uint32_t)(stime % 1000000000) / 100000;         // 提取开始时间
	lSecs = (uint32_t)(stime % 100000);                      // 提取开始秒数

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日
	uint32_t beginTDate = _base_data_mgr->calcTradingDate(stdPID, lDate, lTime, false); // 计算开始交易日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日

	bool isToday = (endTDate == curTDate);                   // 判断是否为今天

	std::string curCode = cInfo._code;                        // 当前合约代码
	if (commInfo->isFuture())                                // 如果是期货合约
	{
		const char* ruleTag = cInfo._ruletag;                 // 获取规则标签
		if (strlen(ruleTag) > 0)                             // 如果有规则标签
			curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, endTDate); // 获取自定义原始代码
	}

	// 比较时间的对象
	WTSTransStruct eTick;                                    // 结束时间逐笔成交结构
	eTick.action_date = rDate;                                // 设置结束日期
	eTick.action_time = rTime * 100000 + rSecs;                // 设置结束时间

	WTSTransStruct sTick;                                    // 开始时间逐笔成交结构
	sTick.action_date = lDate;                                // 设置开始日期
	sTick.action_time = lTime * 100000 + lSecs;                // 设置开始时间

	if (isToday)
	{
		TransBlockPair* tPair = getRTTransBlock(cInfo._exchg, curCode.c_str()); // 获取实时逐笔成交数据块对
		if (tPair == NULL)                                    // 如果数据块为空
			return NULL;                                      // 返回空指针

		RTTransBlock* rtBlock = tPair->_block;                // 获取实时逐笔成交数据块指针

		// 使用二分查找定位结束时间位置
		WTSTransStruct* pItem = std::lower_bound(rtBlock->_trans, rtBlock->_trans + (rtBlock->_size - 1), eTick, [](const WTSTransStruct& a, const WTSTransStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pItem - rtBlock->_trans;           // 计算结束索引

		// 如果光标定位的tick时间比目标时间大, 则全部回退一个
		if (pItem->action_date > eTick.action_date || pItem->action_time > eTick.action_time) // 如果定位的时间大于目标时间
		{
			pItem--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}

		if (beginTDate != endTDate)                          // 如果开始交易日与结束交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			WTSTransSlice* slice = WTSTransSlice::create(stdCode, rtBlock->_trans, eIdx + 1); // 创建逐笔成交数据切片
			return slice;                                    // 返回切片
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pItem = std::lower_bound(rtBlock->_trans, rtBlock->_trans + eIdx, sTick, [](const WTSTransStruct& a, const WTSTransStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pItem - rtBlock->_trans;       // 计算开始索引
			WTSTransSlice* slice = WTSTransSlice::create(stdCode, rtBlock->_trans + sIdx, eIdx - sIdx + 1); // 创建指定范围的逐笔成交数据切片
			return slice;                                    // 返回切片
		}
	}
	else                                                      // 如果不是今天（历史数据）
	{
		std::string key = fmt::format("{}-{}", stdCode, endTDate); // 生成缓存键

		auto it = _his_ordque_map.find(key);                  // 查找历史逐笔成交数据
		if (it == _his_ordque_map.end())                     // 如果没有找到历史数据
		{
			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/trans/" << cInfo._exchg << "/" << endTDate << "/" << curCode << ".dsb"; // 构建历史逐笔成交文件路径
			std::string filename = ss.str();                 // 获取文件路径
			if (!StdFile::exists(filename.c_str()))         // 如果文件不存在
				return NULL;                                 // 返回空指针

			HisTransBlockPair& hisBlkPair = _his_trans_map[key]; // 获取历史逐笔成交数据块对
			StdFile::read_file_content(filename.c_str(), hisBlkPair._buffer); // 读取文件内容到缓冲区
			if (hisBlkPair._buffer.size() < sizeof(HisTransBlockV2)) // 如果文件大小小于历史逐笔成交块V2大小
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of transaction data file {} failed", filename.c_str());
				hisBlkPair._buffer.clear();                  // 清空缓冲区
				return NULL;                                 // 返回空指针
			}

			HisTransBlockV2* tBlockV2 = (HisTransBlockV2*)hisBlkPair._buffer.c_str(); // 获取历史逐笔成交块V2指针

			if (hisBlkPair._buffer.size() != (sizeof(HisTransBlockV2) + tBlockV2->_size)) // 如果文件大小不匹配
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of transaction data file {} failed", filename.c_str());
				return NULL;                                 // 返回空指针
			}

			// 需要解压
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (std::size_t)tBlockV2->_size); // 解压数据

			// 将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			hisBlkPair._buffer.resize(sizeof(HisTransBlock)); // 调整缓冲区大小
			hisBlkPair._buffer.append(buf);                   // 追加解压后的数据
			tBlockV2->_version = BLOCK_VERSION_RAW;          // 设置版本为原始版本

			hisBlkPair._block = (HisTransBlock*)hisBlkPair._buffer.c_str(); // 设置数据块指针
		}

		HisTransBlockPair& tBlkPair = _his_trans_map[key]; // 获取历史逐笔成交数据块对
		if (tBlkPair._block == NULL)                          // 如果数据块为空
			return NULL;                                      // 返回空指针

		HisTransBlock* tBlock = tBlkPair._block;            // 获取历史逐笔成交数据块指针

		uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisTransBlock)) / sizeof(WTSTransStruct); // 计算逐笔成交数量
		if (tcnt <= 0)                                        // 如果逐笔成交数量小于等于0
			return NULL;                                      // 返回空指针

		// 使用二分查找定位结束时间位置
		WTSTransStruct* pItem = std::lower_bound(tBlock->_items, tBlock->_items + (tcnt - 1), eTick, [](const WTSTransStruct& a, const WTSTransStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                              // 如果日期相同
				return a.action_time < b.action_time;         // 按时间比较
		});

		std::size_t eIdx = pItem - tBlock->_items;            // 计算结束索引
		if (pItem->action_date > eTick.action_date || pItem->action_time >= eTick.action_time) // 如果定位的时间大于等于目标时间
		{
			pItem--;                                         // 回退一个位置
			eIdx--;                                          // 索引减1
		}

		if (beginTDate != endTDate)                          // 如果开始交易日与结束交易日不同
		{
			// 如果开始的交易日和当前的交易日不一致，则返回全部的tick数据
			WTSTransSlice* slice = WTSTransSlice::create(stdCode, tBlock->_items, eIdx + 1); // 创建逐笔成交数据切片
			return slice;                                    // 返回切片
		}
		else                                                 // 如果交易日相同
		{
			// 如果交易日相同，则查找起始的位置
			pItem = std::lower_bound(tBlock->_items, tBlock->_items + eIdx, sTick, [](const WTSTransStruct& a, const WTSTransStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                          // 如果日期相同
					return a.action_time < b.action_time;     // 按时间比较
			});

			std::size_t sIdx = pItem - tBlock->_items;        // 计算开始索引
			WTSTransSlice* slice = WTSTransSlice::create(stdCode, tBlock->_items + sIdx, eIdx - sIdx + 1); // 创建指定范围的逐笔成交数据切片
			return slice;                                    // 返回切片
		}
	}
}

/*!
 * \brief 从文件缓存历史K线数据
 * \param codeInfo 代码信息指针
 * \param key 缓存键
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \return 是否缓存成功
 * 
 * 从磁盘文件加载历史K线数据到内存缓存中，支持不同周期的K线数据。
 * 自动处理数据解压和格式转换。
 */
bool WtRdmDtReader::cacheHisBarsFromFile(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period)
{
	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo; // 获取代码信息指针
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo->_exchg, cInfo->_product); // 获取商品信息
	const char* stdPID = cInfo->stdCommID();                 // 获取标准产品ID

	uint32_t curDate = TimeUtils::getCurDate();              // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;         // 获取当前时间（分钟）

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, curDate, curTime, false); // 计算当前交易日

	std::string pname;                                       // 周期名称
	// 根据K线周期设置周期名称
	switch (period)
	{
	case KP_Minute1: pname = "min1"; break;                 // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                 // 5分钟K线
	default: pname = "day"; break;                          // 日K线
	}

	BarsList& barList = _bars_cache[key];                    // 获取K线数据缓存列表
	barList._code = stdCode;                                 // 设置合约代码
	barList._period = period;                                // 设置K线周期
	barList._exchg = cInfo->_exchg;                          // 设置交易所代码

	std::vector<std::vector<WTSBarStruct>*> barsSections;    // K线数据分段列表

	uint32_t realCnt = 0;                                    // 实际数据数量
	const char* ruleTag = cInfo->_ruletag;                   // 获取规则标签
	if (strlen(ruleTag) > 0)                                 // 如果是读取期货主力连续数据
	{
		// 先按照HOT代码进行读取, 如rb.HOT
		std::vector<WTSBarStruct>* hotAy = NULL;             // 主力合约K线数据
		uint64_t lastHotTime = 0;                            // 最后主力合约时间
		// 遍历历史数据文件
		for (;;)
		{
			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/" << pname << "/" << cInfo->_exchg << "/" << cInfo->_exchg << "." << cInfo->_product << "_" << ruleTag; // 构建主力合约文件路径
			if (cInfo->isExright())                          // 如果是除权数据
				ss << (cInfo->_exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ);
			ss << ".dsb";
			std::string filename = ss.str();
			if (!StdFile::exists(filename.c_str()))
				break;

			std::string content;                             // 文件内容
			StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
			if (content.size() < sizeof(HisKlineBlock))      // 如果文件大小小于历史K线块大小
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his kline data file {} failed", filename.c_str());
				break;                                       // 跳出循环
			}

			proc_block_data(content, true, false);           // 处理块数据
			uint32_t barcnt = content.size() / sizeof(WTSBarStruct); // 计算K线数量

			hotAy = new std::vector<WTSBarStruct>();         // 创建主力合约K线数据向量
			hotAy->resize(barcnt);                           // 调整向量大小
			memcpy(hotAy->data(), content.data(), content.size());

			if (period != KP_DAY)                            // 如果不是日K线
				lastHotTime = hotAy->at(barcnt - 1).time;    // 设置最后时间为K线时间
			else                                             // 如果是日K线
				lastHotTime = hotAy->at(barcnt - 1).date;    // 设置最后时间为日期

			pipe_rdmreader_log(_sink, LL_INFO, "{} items of back {} data of hot contract {} directly loaded", barcnt, pname.c_str(), stdCode);
			break;
		}

		HotSections secs;                                    // 热力合约分段列表
		if (strlen(ruleTag))                                 // 如果有规则标签
		{
			if (!_hot_mgr->splitCustomSections(ruleTag, stdPID, 19900102, endTDate, secs)) // 分割自定义分段
				return false;                                // 返回失败
		}

		if (secs.empty())                                    // 如果分段为空
			return false;                                    // 返回失败

		// 根据复权类型确定基础因子
		// 如果是前复权，则历史数据会变小，以最后一个复权因子为基础因子
		// 如果是后复权，则新数据会变大，基础因子为1
		double baseFactor = 1.0;                             // 基础复权因子
		if (cInfo->_exright == 1)                            // 如果是前复权
			baseFactor = secs.back()._factor;                 // 设置基础因子为最后一个复权因子
		else if (cInfo->_exright == 2)                       // 如果是后复权
			barList._factor = secs.back()._factor;           // 设置K线列表复权因子

		bool bAllCovered = false;                            // 是否全部覆盖标志
		// 遍历热力合约分段（从后往前）
		for (auto it = secs.rbegin(); it != secs.rend(); it++)
		{
			const HotSection& hotSec = *it;                  // 获取热力合约分段
			const char* curCode = hotSec._code.c_str();      // 获取当前合约代码
			uint32_t rightDt = hotSec._e_date;               // 获取结束日期
			uint32_t leftDt = hotSec._s_date;                 // 获取开始日期

			// 要先将日期转换为边界时间
			WTSBarStruct sBar, eBar;                         // 开始和结束K线结构
			if (period != KP_DAY)                            // 如果不是日K线
			{
				uint64_t sTime = _base_data_mgr->getBoundaryTime(stdPID, leftDt, false, true); // 获取开始边界时间
				uint64_t eTime = _base_data_mgr->getBoundaryTime(stdPID, rightDt, false, false); // 获取结束边界时间

				sBar.date = leftDt;                          // 设置开始日期
				sBar.time = ((uint32_t)(sTime / 10000) - 19900000) * 10000 + (uint32_t)(sTime % 10000); // 设置开始时间

				if(sBar.time < lastHotTime)                   // 如果边界时间小于主力的最后一根Bar的时间, 说明已经有交叉了, 则不需要再处理了
				{
					bAllCovered = true;                       // 标记全部覆盖
					sBar.time = lastHotTime + 1;              // 设置开始时间为最后主力时间+1
				}

				eBar.date = rightDt;                          // 设置结束日期
				eBar.time = ((uint32_t)(eTime / 10000) - 19900000) * 10000 + (uint32_t)(eTime % 10000); // 设置结束时间

				if (eBar.time <= lastHotTime)                 // 右边界时间小于最后一条Hot时间, 说明全部交叉了, 没有再找的必要了
					break;
			}
			else                                             // 如果是日K线
			{
				sBar.date = leftDt;                          // 设置开始日期
				if (sBar.date < lastHotTime)                 // 如果边界时间小于主力的最后一根Bar的时间, 说明已经有交叉了, 则不需要再处理了
				{
					bAllCovered = true;                       // 标记全部覆盖
					sBar.date = (uint32_t)lastHotTime + 1;   // 设置开始日期为最后主力时间+1
				}

				eBar.date = rightDt;                          // 设置结束日期

				if (eBar.date <= lastHotTime)                 // 如果结束日期小于等于最后主力时间
					break;                                   // 跳出循环
			}

			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/" << pname << "/" << cInfo->_exchg << "/" << curCode << ".dsb"; // 构建历史K线文件路径
			std::string filename = ss.str();                 // 获取文件路径
			if (!StdFile::exists(filename.c_str()))         // 如果文件不存在
				continue;                                   // 继续下一个分段

			{
				std::string content;                         // 文件内容
				StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
				if (content.size() < sizeof(HisKlineBlock))  // 如果文件大小小于历史K线块大小
				{
					pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his kline data file {} failed", filename.c_str());
					return false;                            // 返回失败
				}
				
				proc_block_data(content, true, false);       // 处理块数据

				if(content.empty())                          // 如果内容为空
					break;                                   // 跳出循环

				uint32_t barcnt = content.size() / sizeof(WTSBarStruct); // 计算K线数量
				WTSBarStruct* firstBar = (WTSBarStruct*)content.data(); // 获取第一个K线指针

				// 使用二分查找定位开始K线位置
				WTSBarStruct* pBar = std::lower_bound(firstBar, firstBar + (barcnt - 1), sBar, [period](const WTSBarStruct& a, const WTSBarStruct& b){
					if (period == KP_DAY)                    // 如果是日K线
					{
						return a.date < b.date;             // 按日期比较
					}
					else                                     // 如果不是日K线
					{
						return a.time < b.time;             // 按时间比较
					}
				});

				std::size_t sIdx = pBar - firstBar;           // 计算开始索引
				if ((period == KP_DAY && pBar->date < sBar.date) || (period != KP_DAY && pBar->time < sBar.time)) // 早于边界时间
				{
					// 早于边界时间, 说明没有数据了, 因为lower_bound会返回大于等于目标位置的数据
					continue;                               // 继续下一个分段
				}

				// 使用二分查找定位结束K线位置
				pBar = std::lower_bound(firstBar + sIdx, firstBar + (barcnt - 1), eBar, [period](const WTSBarStruct& a, const WTSBarStruct& b){
					if (period == KP_DAY)                    // 如果是日K线
					{
						return a.date < b.date;             // 按日期比较
					}
					else                                     // 如果不是日K线
					{
						return a.time < b.time;             // 按时间比较
					}
				});

				std::size_t eIdx = pBar - firstBar;           // 计算结束索引
				if ((period == KP_DAY && pBar->date > eBar.date) || (period != KP_DAY && pBar->time > eBar.time)) // 如果定位的K线时间大于目标时间
				{
					pBar--;                                   // 回退一个位置
					eIdx--;                                   // 索引减1
				}

				if (eIdx < sIdx)                              // 如果结束索引小于开始索引
					continue;                               // 继续下一个分段

				uint32_t curCnt = eIdx - sIdx + 1;           // 计算当前分段K线数量

				if (cInfo->isExright())                      // 如果是除权数据
				{
					double factor = hotSec._factor / baseFactor; // 计算复权因子
					for (uint32_t idx = sIdx; idx <= eIdx; idx++) // 遍历K线数据
					{
						firstBar[idx].open *= factor;        // 复权开盘价
						firstBar[idx].high *= factor;        // 复权最高价
						firstBar[idx].low *= factor;         // 复权最低价
						firstBar[idx].close *= factor;       // 复权收盘价
					}
				}

				std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>(); // 创建临时K线数据向量
				tempAy->resize(curCnt);                       // 调整向量大小
				memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt); // 复制K线数据
				realCnt += curCnt;                           // 累加实际数据数量

				barsSections.emplace_back(tempAy);            // 添加到K线数据分段列表

				if(bAllCovered)                               // 如果全部覆盖
					break;                                   // 跳出循环
			}
		}

		if (hotAy)                                          // 如果有主力合约数据
		{
			barsSections.emplace_back(hotAy);               // 添加到K线数据分段列表
			realCnt += hotAy->size();                       // 累加实际数据数量
		}
	}
	else if(cInfo->isExright() && commInfo->isStock())     // 如果是读取股票复权数据
	{
		std::vector<WTSBarStruct>* hotAy = NULL;            // 股票复权K线数据
		uint64_t lastQTime = 0;                            // 最后复权时间
		
		do
		{
			// 先直接读取复权过的历史数据,路径如/his/day/sse/SH600000Q.dsb
			char flag = cInfo->_exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ; // 获取复权标志
			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/" << pname << "/" << cInfo->_exchg << "/" << cInfo->_code << flag << ".dsb"; // 构建股票复权文件路径
			std::string filename = ss.str();                 // 获取文件路径
			if (!StdFile::exists(filename.c_str()))         // 如果文件不存在
				break;                                       // 跳出循环

			std::string content;                             // 文件内容
			StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
			if (content.size() < sizeof(HisKlineBlock))      // 如果文件大小小于历史K线块大小
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his kline data file {} failed", filename.c_str());
				break;                                       // 跳出循环
			}

			HisKlineBlock* kBlock = (HisKlineBlock*)content.c_str(); // 获取历史K线块指针
			uint32_t barcnt = 0;                            // K线数量
			std::string buffer;                              // 缓冲区
			bool bOldVer = kBlock->is_old_version();        // 是否旧版本
			if (kBlock->_version == BLOCK_VERSION_CMP)        // 如果是压缩版本
			{
				if (content.size() < sizeof(HisKlineBlockV2)) // 如果文件大小小于历史K线块V2大小
				{
					pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his kline data file {} failed", filename.c_str());
					break;                                   // 跳出循环
				}

				HisKlineBlockV2* kBlockV2 = (HisKlineBlockV2*)content.c_str(); // 获取历史K线块V2指针
				if (kBlockV2->_size == 0)                    // 如果数据大小为0
					break;                                   // 跳出循环

				buffer = WTSCmpHelper::uncompress_data(kBlockV2->_data, (std::size_t)kBlockV2->_size); // 解压数据
			}
			else                                             // 如果不是压缩版本
			{
				content.erase(0, BLOCK_HEADER_SIZE);         // 删除块头部
				buffer.swap(content);                        // 交换缓冲区内容
			}

			if(buffer.empty())                               // 如果缓冲区为空
				break;                                       // 跳出循环

			if(bOldVer)                                      // 如果是旧版本
			{
				std::string bufV2;                           // 新版本缓冲区
				uint32_t barcnt = buffer.size() / sizeof(WTSBarStructOld); // 计算K线数量
				bufV2.resize(barcnt * sizeof(WTSBarStruct)); // 调整新版本缓冲区大小
				WTSBarStruct* newBar = (WTSBarStruct*)bufV2.data(); // 获取新版本K线指针
				WTSBarStructOld* oldBar = (WTSBarStructOld*)buffer.data(); // 获取旧版本K线指针
				for (uint32_t idx = 0; idx < barcnt; idx++)  // 遍历K线数据
				{
					newBar[idx] = oldBar[idx];              // 转换旧版本K线到新版本
				}
				buffer.swap(bufV2);                          // 交换缓冲区内容
			}

			barcnt = buffer.size() / sizeof(WTSBarStruct);  // 计算K线数量

			hotAy = new std::vector<WTSBarStruct>();         // 创建股票复权K线数据向量
			hotAy->resize(barcnt);                           // 调整向量大小
			memcpy(hotAy->data(), buffer.data(), buffer.size()); // 复制K线数据

			if (period != KP_DAY)                            // 如果不是日K线
				lastQTime = hotAy->at(barcnt - 1).time;    // 设置最后复权时间为K线时间
			else                                             // 如果是日K线
				lastQTime = hotAy->at(barcnt - 1).date;    // 设置最后复权时间为日期

			pipe_rdmreader_log(_sink, LL_INFO, "{} history exrighted {} data of {} directly cached", barcnt, pname.c_str(), stdCode); // 记录日志
			break;                                           // 跳出循环
		} while (false);                                     // 结束do-while循环

		bool bAllCovered = false;                            // 是否全部覆盖标志
		do
		{
			// const char* curCode = it->first.c_str();      // 当前合约代码
			// uint32_t rightDt = it->second.second;         // 结束日期
			// uint32_t leftDt = it->second.first;           // 开始日期
			const char* curCode = cInfo->_code;              // 获取当前合约代码

			// 要先将日期转换为边界时间
			WTSBarStruct sBar;                               // 开始K线结构
			if (period != KP_DAY)                            // 如果不是日K线
			{
				sBar.date = TimeUtils::minBarToDate(lastQTime); // 将分钟时间转换为日期

				sBar.time = lastQTime + 1;                   // 设置开始时间为最后复权时间+1
			}
			else                                             // 如果是日K线
			{
				sBar.date = (uint32_t)lastQTime + 1;        // 设置开始日期为最后复权时间+1
			}

			std::stringstream ss;                            // 字符串流
			ss << _base_dir << "his/" << pname << "/" << cInfo->_exchg << "/" << curCode << ".dsb"; // 构建股票复权文件路径
			std::string filename = ss.str();                 // 获取文件路径
			if (!StdFile::exists(filename.c_str()))         // 如果文件不存在
				continue;                                   // 继续下一个分段

			{
				std::string content;                         // 文件内容
				StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
				if (content.size() < sizeof(HisKlineBlock))  // 如果文件大小小于历史K线块大小
				{
					pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his kline data file {} failed", filename.c_str());
					return false;                            // 返回失败
				}

				proc_block_data(content, true, false);       // 处理块数据
				if(content.empty())                          // 如果内容为空
					break;                                   // 跳出循环

				uint32_t barcnt = content.size() / sizeof(WTSBarStruct); // 计算K线数量
				WTSBarStruct* firstBar = (WTSBarStruct*)content.data(); // 获取第一个K线指针

				// 使用二分查找定位开始K线位置
				WTSBarStruct* pBar = std::lower_bound(firstBar, firstBar + (barcnt - 1), sBar, [period](const WTSBarStruct& a, const WTSBarStruct& b){
					if (period == KP_DAY)                    // 如果是日K线
					{
						return a.date < b.date;             // 按日期比较
					}
					else                                     // 如果不是日K线
					{
						return a.time < b.time;             // 按时间比较
					}
				});

				if(pBar != NULL)                              // 如果找到K线位置
				{
					std::size_t sIdx = pBar - firstBar;       // 计算开始索引
					uint32_t curCnt = barcnt - sIdx;          // 计算当前分段K线数量
					std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>(); // 创建临时K线数据向量
					tempAy->resize(curCnt);                       // 调整向量大小
					memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt); // 复制K线数据
					realCnt += curCnt;                           // 累加实际数据数量

					auto& ayFactors = getAdjFactors(cInfo->_code, cInfo->_exchg, cInfo->_product); // 获取复权因子列表
					if(!ayFactors.empty())                    // 如果复权因子不为空
					{
						double baseFactor = 1.0;             // 基础复权因子
						if (cInfo->_exright == 1)            // 如果是前复权
							baseFactor = ayFactors.back()._factor; // 设置基础因子为最后一个复权因子
						else if (cInfo->_exright == 2)       // 如果是后复权
							barList._factor = ayFactors.back()._factor; // 设置K线列表复权因子

						// 做前复权处理
						std::size_t lastIdx = curCnt;        // 最后索引
						WTSBarStruct bar;                    // K线结构
						firstBar = tempAy->data();           // 获取第一个K线指针
						// 遍历复权因子（从后往前）
						for (auto it = ayFactors.rbegin(); it != ayFactors.rend(); it++)
						{
							const AdjFactor& adjFact = *it;  // 获取复权因子
							bar.date = adjFact._date;        // 设置复权日期

							// 调整因子
							double factor = adjFact._factor / baseFactor; // 计算复权因子

							WTSBarStruct* pBar = NULL;        // K线指针
							// 使用二分查找定位复权日期位置
							pBar = std::lower_bound(firstBar, firstBar + lastIdx - 1, bar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {
								return a.date < b.date;     // 按日期比较
							});

							if (pBar->date < bar.date)      // 如果定位的K线日期小于复权日期
								continue;                   // 继续下一个复权因子

							WTSBarStruct* endBar = pBar;     // 结束K线指针
							if (pBar != NULL)                // 如果找到K线位置
							{
								std::size_t curIdx = pBar - firstBar; // 计算当前索引
								while (pBar && curIdx < lastIdx)      // 遍历K线数据
								{
									pBar->open *= factor;            // 复权开盘价
									pBar->high *= factor;            // 复权最高价
									pBar->low *= factor;             // 复权最低价
									pBar->close *= factor;           // 复权收盘价

									pBar++;                          // 移动到下一个K线
									curIdx++;                        // 索引加1
								}
								lastIdx = endBar - firstBar;        // 更新最后索引
							}

							if (lastIdx == 0)                      // 如果最后索引为0
								break;                             // 跳出循环
						}
					}

					barsSections.emplace_back(tempAy);            // 添加到K线数据分段列表
				}
			}
		} while (false);                                         // 结束do-while循环

		if (hotAy)                                              // 如果有股票复权数据
		{
			barsSections.emplace_back(hotAy);                   // 添加到K线数据分段列表
			realCnt += hotAy->size();                           // 累加实际数据数量
		}
	}
	else                                                     // 如果不是期货主力合约或股票复权数据
	{
		// 读取历史的
		std::stringstream ss;                                // 字符串流
		ss << _base_dir << "his/" << pname << "/" << cInfo->_exchg << "/" << cInfo->_code << ".dsb"; // 构建历史K线文件路径
		std::string filename = ss.str();                     // 获取文件路径
		pipe_rdmreader_log(_sink, LL_DEBUG, "Target file is {}", filename); // 记录日志
		if (StdFile::exists(filename.c_str()))             // 如果文件存在
		{
			// 如果有格式化的历史数据文件, 则直接读取
			std::string content;                             // 文件内容
			StdFile::read_file_content(filename.c_str(), content); // 读取文件内容
			if (content.size() < sizeof(HisKlineBlock))      // 如果文件大小小于历史K线块大小
			{
				pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his kline data file {} failed", filename.c_str());
				return false;                                // 返回失败
			}

			proc_block_data(content, true, false);           // 处理块数据

			if (content.empty())                              // 如果内容为空
				return false;                                // 返回失败

			uint32_t barcnt = content.size() / sizeof(WTSBarStruct); // 计算K线数量
			WTSBarStruct* firstBar = (WTSBarStruct*)content.data(); // 获取第一个K线指针

			if (barcnt > 0)                                    // 如果K线数量大于0
			{
				uint32_t sIdx = 0;                            // 开始索引
				uint32_t idx = barcnt - 1;                    // 结束索引
				uint32_t curCnt = (idx - sIdx + 1);           // 计算当前分段K线数量

				std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>(); // 创建临时K线数据向量
				tempAy->resize(curCnt);                       // 调整向量大小
				memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt); // 复制K线数据
				realCnt += curCnt;                           // 累加实际数据数量

				barsSections.emplace_back(tempAy);            // 添加到K线数据分段列表
			}
		}
	}

	if (realCnt > 0)                                        // 如果实际数据数量大于0
	{
		barList._bars.resize(realCnt);                      // 调整K线列表大小

		uint32_t curIdx = 0;                                // 当前索引
		// 遍历K线数据分段（从后往前）
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)
		{
			std::vector<WTSBarStruct>* tempAy = *it;        // 获取临时K线数据向量
			memcpy(barList._bars.data() + curIdx, tempAy->data(), tempAy->size()*sizeof(WTSBarStruct)); // 复制K线数据
			curIdx += tempAy->size();                       // 更新当前索引
			delete tempAy;                                  // 删除临时K线数据向量
		}
		barsSections.clear();                              // 清空K线数据分段列表
	}

	pipe_rdmreader_log(_sink, LL_INFO, "{} history {} data of {} cached", realCnt, pname.c_str(), stdCode); // 记录日志
	return true;                                            // 返回成功
}

/*!
 * \brief 从缓存中按时间范围索引K线数据
 * \param key 缓存键
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳）
 * \param count 返回的数据数量
 * \param isDay 是否为日K线
 * \return K线数据指针，失败返回NULL
 * 
 * 从内存缓存中按时间范围索引K线数据，支持不同周期的K线数据。
 * 使用二分查找算法快速定位数据范围。
 */
WTSBarStruct* WtRdmDtReader::indexBarFromCacheByRange(const std::string& key, uint64_t stime, uint64_t etime, uint32_t& count, bool isDay /* = false */)
{
	uint32_t rDate, rTime, lDate, lTime;                   // 右日期、右时间、左日期、左时间
	rDate = (uint32_t)(etime / 10000);                     // 计算右日期
	rTime = (uint32_t)(etime % 10000);                     // 计算右时间
	lDate = (uint32_t)(stime / 10000);                     // 计算左日期
	lTime = (uint32_t)(stime % 10000);                     // 计算左时间

	BarsList& barsList = _bars_cache[key];                 // 获取K线数据缓存列表
	if (barsList._bars.empty())                             // 如果K线数据为空
		return NULL;                                        // 返回空指针
	
	std::size_t eIdx,sIdx;                                  // 结束索引、开始索引
	{
		// 光标尚未初始化, 需要重新定位
		uint64_t nowTime = (uint64_t)rDate * 10000 + rTime; // 计算当前时间

		WTSBarStruct eBar;                                  // 结束K线结构
		eBar.date = rDate;                                  // 设置结束日期
		eBar.time = (rDate - 19900000) * 10000 + rTime;     // 设置结束时间

		WTSBarStruct sBar;                                  // 开始K线结构
		sBar.date = lDate;                                  // 设置开始日期
		sBar.time = (lDate - 19900000) * 10000 + lTime;    // 设置开始时间

		// 使用二分查找定位结束K线位置
		auto eit = std::lower_bound(barsList._bars.begin(), barsList._bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b){
			if (isDay)                                       // 如果是日K线
				return a.date < b.date;                     // 按日期比较
			else                                             // 如果不是日K线
				return a.time < b.time;                     // 按时间比较
		});


		if (eit == barsList._bars.end())                   // 如果定位到末尾
			eIdx = barsList._bars.size() - 1;              // 设置结束索引为最后一个
		else                                               // 如果找到位置
		{
			if ((isDay && eit->date > eBar.date) || (!isDay && eit->time > eBar.time)) // 如果定位的K线时间大于目标时间
			{
				eit--;                                      // 回退一个位置
			}

			eIdx = eit - barsList._bars.begin();
		}

		// 使用二分查找定位开始K线位置
		auto sit = std::lower_bound(barsList._bars.begin(), eit, sBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
			if (isDay)                                       // 如果是日K线
				return a.date < b.date;                     // 按日期比较
			else                                             // 如果不是日K线
				return a.time < b.time;                     // 按时间比较
		});
		sIdx = sit - barsList._bars.begin();                // 计算开始索引
	}

	uint32_t curCnt = eIdx - sIdx + 1;                     // 计算当前分段K线数量
	count = curCnt;                                         // 设置返回数量
	return &barsList._bars[sIdx];                           // 返回K线数据指针
}

/*!
 * \brief 从缓存中按数量索引K线数据
 * \param key 缓存键
 * \param etime 结束时间（微秒时间戳）
 * \param count 返回的数据数量
 * \param isDay 是否为日K线
 * \return K线数据指针，失败返回NULL
 * 
 * 从内存缓存中按数量索引K线数据，支持不同周期的K线数据。
 * 从结束时间往前查找指定数量的K线数据。
 */
WTSBarStruct* WtRdmDtReader::indexBarFromCacheByCount(const std::string& key, uint64_t etime, uint32_t& count, bool isDay /* = false */)
{
	uint32_t rDate, rTime;                                   // 右日期、右时间
	rDate = (uint32_t)(etime / 10000);                     // 计算右日期
	rTime = (uint32_t)(etime % 10000);                     // 计算右时间

	BarsList& barsList = _bars_cache[key];                 // 获取K线数据缓存列表
	if (barsList._bars.empty())                             // 如果K线数据为空
		return NULL;                                        // 返回空指针

	std::size_t eIdx, sIdx;                                  // 结束索引、开始索引
	WTSBarStruct eBar;                                      // 结束K线结构
	eBar.date = rDate;                                      // 设置结束日期
	eBar.time = (rDate - 19900000) * 10000 + rTime;         // 设置结束时间

	// 使用二分查找定位结束K线位置
	auto eit = std::lower_bound(barsList._bars.begin(), barsList._bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
		if (isDay)                                           // 如果是日K线
			return a.date < b.date;                         // 按日期比较
		else                                                 // 如果不是日K线
			return a.time < b.time;                         // 按时间比较
	});


	if (eit == barsList._bars.end())                       // 如果定位到末尾
		eIdx = barsList._bars.size() - 1;                  // 设置结束索引为最后一个
	else                                                   // 如果找到位置
	{
		if ((isDay && eit->date > eBar.date) || (!isDay && eit->time > eBar.time)) // 如果定位的K线时间大于目标时间
		{
			eit--;                                          // 回退一个位置
		}

		eIdx = eit - barsList._bars.begin();                // 计算结束索引
	}

	uint32_t curCnt = min((uint32_t)eIdx + 1, count);      // 计算当前分段K线数量
	sIdx = eIdx + 1 - curCnt;                              // 计算开始索引
	count = curCnt;                                         // 设置返回数量
	return &barsList._bars[sIdx];                           // 返回K线数据指针
}

/*!
 * \brief 从缓存中按时间范围读取K线数据
 * \param key 缓存键
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳）
 * \param ayBars 返回的K线数据向量
 * \param isDay 是否为日K线
 * \return 读取的数据数量
 * 
 * 从内存缓存中按时间范围读取K线数据，支持不同周期的K线数据。
 * 使用二分查找算法快速定位数据范围。
 */
uint32_t WtRdmDtReader::readBarsFromCacheByRange(const std::string& key, uint64_t stime, uint64_t etime, std::vector<WTSBarStruct>& ayBars, bool isDay /* = false */)
{
	uint32_t rDate, rTime, lDate, lTime;                   // 右日期、右时间、左日期、左时间
	rDate = (uint32_t)(etime / 10000);                     // 计算右日期
	rTime = (uint32_t)(etime % 10000);                     // 计算右时间
	lDate = (uint32_t)(stime / 10000);                     // 计算左日期
	lTime = (uint32_t)(stime % 10000);                     // 计算左时间

	BarsList& barsList = _bars_cache[key];                 // 获取K线数据缓存列表
	std::size_t eIdx,sIdx;                                  // 结束索引、开始索引
	{
		WTSBarStruct eBar;                                  // 结束K线结构
		eBar.date = rDate;                                  // 设置结束日期
		eBar.time = (rDate - 19900000) * 10000 + rTime;     // 设置结束时间

		WTSBarStruct sBar;                                  // 开始K线结构
		sBar.date = lDate;                                  // 设置开始日期
		sBar.time = (lDate - 19900000) * 10000 + lTime;    // 设置开始时间

		// 使用二分查找定位结束K线位置
		auto eit = std::lower_bound(barsList._bars.begin(), barsList._bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b){
			if (isDay)                                       // 如果是日K线
				return a.date < b.date;                     // 按日期比较
			else                                             // 如果不是日K线
				return a.time < b.time;                     // 按时间比较
		});
		

		if(eit == barsList._bars.end())                    // 如果定位到末尾
			eIdx = barsList._bars.size() - 1;              // 设置结束索引为最后一个
		else                                               // 如果找到位置
		{
			if ((isDay && eit->date > eBar.date) || (!isDay && eit->time > eBar.time)) // 如果定位的K线时间大于目标时间
			{
				if (eit == barsList._bars.begin())         // 如果定位到开始位置
					return 0;                               // 返回0
				
				eit--;                                      // 回退一个位置
			}

			eIdx = eit - barsList._bars.begin();            // 计算结束索引
		}

		// 使用二分查找定位开始K线位置
		auto sit = std::lower_bound(barsList._bars.begin(), eit, sBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
			if (isDay)                                       // 如果是日K线
				return a.date < b.date;                     // 按日期比较
			else                                             // 如果不是日K线
				return a.time < b.time;                     // 按时间比较
		});
		sIdx = sit - barsList._bars.begin();                // 计算开始索引
	}

	uint32_t curCnt = eIdx - sIdx + 1;                     // 计算当前分段K线数量
	if(curCnt > 0)                                          // 如果K线数量大于0
	{
		ayBars.resize(curCnt);                              // 调整返回向量大小
		memcpy(ayBars.data(), &barsList._bars[sIdx], sizeof(WTSBarStruct)*curCnt); // 复制K线数据
	}
	return curCnt;                                          // 返回K线数量
}

/*!
 * \brief 按时间范围读取K线数据切片
 * \param stdCode 标准合约代码
 * \param period K线周期
 * \param stime 开始时间（微秒时间戳）
 * \param etime 结束时间（微秒时间戳，默认为0）
 * \return K线数据切片指针，失败返回NULL
 * 
 * 根据指定时间范围读取K线数据，支持不同周期的K线数据。
 * 自动处理期货合约的主力切换，支持历史数据和实时数据的混合读取。
 */
/*!
 * \brief 按时间范围读取K线数据切片
 * \param stdCode 标准合约代码，格式如"SHFE.rb.HOT"
 * \param period K线周期，支持日线、1分钟、5分钟等
 * \param stime 开始时间（微秒时间戳，格式：YYYYMMDDHHMM）
 * \param etime 结束时间（微秒时间戳，默认为0表示到未来）
 * \return K线数据切片指针，失败返回NULL
 * 
 * 该函数是WonderTrader框架中K线数据读取的核心方法，支持按时间范围随机读取K线数据。
 * 自动处理期货合约的主力切换，支持历史数据和实时数据的混合读取。
 * 使用内存映射文件技术提高读取性能，支持数据缓存和预加载。
 */
WTSKlineSlice* WtRdmDtReader::readKlineSliceByRange(const char* stdCode, WTSKlinePeriod period, uint64_t stime, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码，提取交易所、品种、合约等信息
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息，包含交易规则、复权信息等
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID，用于交易日计算

	std::string key = fmt::format("{}#{}", stdCode, period); // 生成缓存键，格式：合约代码#周期
	auto it = _bars_cache.find(key);                        // 查找K线数据缓存，避免重复加载
	bool bHasHisData = false;                               // 是否有历史数据标志
	if (it == _bars_cache.end())                           // 如果没有找到缓存
	{
		bHasHisData = cacheHisBarsFromFile(&cInfo, key, stdCode, period); // 从文件缓存历史K线数据到内存
	}
	else                                                     // 如果找到缓存
	{
		bHasHisData = true;                                 // 标记有历史数据
	}

	if (etime == 0)                                          // 如果结束时间为0
		etime = 203012312359;                               // 设置默认结束时间为2030年12月31日23:59

	uint32_t rDate, rTime, lDate, lTime;                   // 右日期、右时间、左日期、左时间
	rDate = (uint32_t)(etime / 10000);                     // 计算右日期（结束日期）
	rTime = (uint32_t)(etime % 10000);                     // 计算右时间（结束时间）
	lDate = (uint32_t)(stime / 10000);                     // 计算左日期（开始日期）
	lTime = (uint32_t)(stime % 10000);                     // 计算左时间（开始时间）

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日，考虑节假日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日
	
	WTSBarStruct* hisHead = NULL;                          // 历史K线数据头指针
	WTSBarStruct* rtHead = NULL;                            // 实时K线数据头指针
	uint32_t hisCnt = 0;                                    // 历史K线数量
	uint32_t rtCnt = 0;                                     // 实时K线数量

	std::string pname;                                      // 周期名称
	// 根据K线周期设置周期名称，用于文件路径构建
	switch (period)
	{
	case KP_Minute1: pname = "min1"; break;                 // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                 // 5分钟K线
	default: pname = "day"; break;                          // 日K线
	}

	bool isDay = period == KP_DAY;                          // 判断是否为日线周期

	//是否包含当天的
	bool bHasToday = (endTDate >= curTDate);               // 判断查询时间范围是否包含当天
	std::string raw_code = cInfo._code;                    // 原始合约代码

	const char* ruleTag = cInfo._ruletag;                  // 获取规则标签，用于主力合约切换
	if (strlen(ruleTag) > 0)                               // 如果有规则标签（如期货主力合约）
	{
		raw_code = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), curTDate); // 获取指定日期的实际合约代码

		pipe_rdmreader_log(_sink, LL_INFO, "{} contract on {} confirmed: {} -> {}", ruleTag, curTDate, stdCode, raw_code);
	}
	else
	{
		raw_code = cInfo._code;                            // 使用原始合约代码
	}

	WTSBarStruct eBar;                                     // 结束时间K线结构
	eBar.date = rDate;                                      // 设置结束日期
	eBar.time = (rDate - 19900000) * 10000 + rTime;         // 设置结束时间（转换为内部时间格式）

	WTSBarStruct sBar;                                     // 开始时间K线结构
	sBar.date = lDate;                                      // 设置开始日期
	sBar.time = (lDate - 19900000) * 10000 + lTime;         // 设置开始时间（转换为内部时间格式）

	bool bNeedHisData = true;                              // 是否需要历史数据标志

	if (bHasToday)                                          // 如果查询时间范围包含当天
	{
		//读取实时的

		const char* curCode = raw_code.c_str();             // 获取当前实际合约代码

		if(cInfo._exright != 2)                              // 如果不是后复权模式
		{
			RTKlineBlockPair* kPair = getRTKilneBlock(cInfo._exchg, curCode, period); // 获取实时K线数据块
			if (kPair != NULL)                               // 如果找到实时数据块
			{
				StdUniqueLock lock(*kPair->_mtx);            // 加锁保护实时数据访问
				//读取当日的数据
				WTSBarStruct* pBar = std::lower_bound(kPair->_block->_bars, kPair->_block->_bars + (kPair->_block->_size - 1), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
					if (isDay)                               // 如果是日线，按日期比较
						return a.date < b.date;
					else                                     // 否则按时间比较
						return a.time < b.time;
				});
				std::size_t idx = pBar - kPair->_block->_bars; // 计算结束位置索引
				if ((isDay && pBar->date > eBar.date) || (!isDay && pBar->time > eBar.time)) // 如果定位的K线时间超过结束时间
				{
					pBar--;                                  // 回退一个K线
					idx--;                                  // 索引也回退
				}

				pBar = &kPair->_block->_bars[0];            // 指向第一条实时K线
				//如果第一条实时K线的时间大于开始日期，则实时K线要全部包含进去
				if ((isDay && pBar->date > sBar.date) || (!isDay && pBar->time > sBar.time)) // 如果第一条实时K线时间大于开始时间
				{
					rtHead = &kPair->_block->_bars[0];      // 实时数据从头开始
					rtCnt = idx + 1;                         // 实时K线数量
				}
				else                                        // 否则需要定位开始位置
				{
					pBar = std::lower_bound(kPair->_block->_bars, kPair->_block->_bars + idx, sBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
						if (isDay)                           // 如果是日线，按日期比较
							return a.date < b.date;
						else                                 // 否则按时间比较
							return a.time < b.time;
					});

					std::size_t sIdx = pBar - kPair->_block->_bars; // 计算开始位置索引
					rtHead = pBar;                           // 设置实时数据头指针
					rtCnt = idx - sIdx + 1;                  // 计算实时K线数量
					bNeedHisData = false;                     // 不需要历史数据
				}
			}
		}
		else                                                 // 如果是后复权模式
		{
			RTKlineBlockPair* kPair = getRTKilneBlock(cInfo._exchg, curCode, period); // 获取实时K线数据块
			if (kPair != NULL)                               // 如果找到实时数据块
			{
				//如果是后复权，实时数据是需要单独缓存的，所以这里处理会很复杂
				BarsList& barsList = _bars_cache[key];       // 获取K线数据缓存列表

				//1、先检查缓存中有多少实时数据
				std::size_t oldSize = barsList._rt_bars.size(); // 获取缓存中实时数据大小
				std::size_t newSize = kPair->_block->_size;     // 获取原始实时数据大小

				//2、再看看原始实时数据有多少，如果不够，就要补充进来
				if (newSize > oldSize)                       // 如果原始数据比缓存数据多
				{
					barsList._rt_bars.resize(newSize);       // 调整缓存大小
					auto idx = oldSize;                      // 从旧大小开始
					if (oldSize != 0)                        // 如果之前有数据
						idx--;                                // 从倒数第二个开始，避免重复

					//因为每次拷贝，最后一条K线都有可能是未闭合的，所以需要把最后一条K线覆盖
					memcpy(&barsList._rt_bars[idx], &kPair->_block->_bars[idx], sizeof(WTSBarStruct)*(newSize - oldSize + 1)); // 复制新增的K线数据

					//最后做复权处理
					double factor = barsList._factor;        // 获取复权因子
					for (; idx < newSize; idx++)              // 遍历新增的K线数据
					{
						WTSBarStruct* pBar = &barsList._rt_bars[idx]; // 获取K线指针
						pBar->open *= factor;                 // 开盘价复权
						pBar->high *= factor;                 // 最高价复权
						pBar->low *= factor;                  // 最低价复权
						pBar->close *= factor;                // 收盘价复权
					}
				}

				//最后做一个定位
				auto it = std::lower_bound(barsList._rt_bars.begin(), barsList._rt_bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
					if (isDay)                               // 如果是日线，按日期比较
						return a.date < b.date;
					else                                     // 否则按时间比较
						return a.time < b.time;
				});
				std::size_t idx = it - barsList._rt_bars.begin(); // 计算结束位置索引
				WTSBarStruct* pBar = &barsList._rt_bars[idx]; // 获取结束位置K线指针
				if ((isDay && pBar->date > eBar.date) || (!isDay && pBar->time > eBar.time)) // 如果定位的K线时间超过结束时间
				{
					pBar--;                                  // 回退一个K线
					idx--;                                  // 索引也回退
				}

				pBar = &barsList._rt_bars[0];               // 指向第一条实时K线
				//如果第一条实时K线的时间大于开始日期，则实时K线要全部包含进去
				if ((isDay && pBar->date > sBar.date) || (!isDay && pBar->time > sBar.time)) // 如果第一条实时K线时间大于开始时间
				{
					rtHead = &barsList._rt_bars[0];          // 实时数据从头开始
					rtCnt = idx + 1;                         // 实时K线数量
				}
				else                                        // 否则需要定位开始位置
				{
					it = std::lower_bound(barsList._rt_bars.begin(), barsList._rt_bars.begin() + idx, sBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
						if (isDay)                           // 如果是日线，按日期比较
							return a.date < b.date;
						else                                 // 否则按时间比较
							return a.time < b.time;
					});

					std::size_t sIdx = it - barsList._rt_bars.begin(); // 计算开始位置索引
					rtHead = &barsList._rt_bars[sIdx];       // 设置实时数据头指针
					rtCnt = idx - sIdx + 1;                  // 计算实时K线数量
					bNeedHisData = false;                     // 不需要历史数据
				}
			}
		}	
		
	}

	if (bNeedHisData)                                        // 如果需要历史数据
	{
		hisHead = indexBarFromCacheByRange(key, stime, etime, hisCnt, period == KP_DAY); // 从缓存中按时间范围获取历史K线数据
	}

	if (hisCnt + rtCnt > 0)                                 // 如果历史数据或实时数据有内容
	{
		WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, 1, hisHead, hisCnt); // 创建K线数据切片，先添加历史数据
		if (rtCnt > 0)                                       // 如果有实时数据
			slice->appendBlock(rtHead, rtCnt);              // 追加实时数据块
		return slice;                                        // 返回完整的K线数据切片
	}

	return NULL;                                            // 没有数据则返回空指针
}


/*!
 * \brief 获取实时Tick数据块
 * \param exchg 交易所代码，如"SHFE"、"DCE"等
 * \param code 合约代码，如"rb2305"
 * \return Tick数据块指针，失败返回NULL
 * 
 * 该函数用于获取指定合约的实时Tick数据块，使用内存映射文件技术提高访问性能。
 * 支持文件大小变化时的自动重新映射，确保数据访问的实时性。
 */
WtRdmDtReader::TickBlockPair* WtRdmDtReader::getRTTickBlock(const char* exchg, const char* code)
{
	std::string key = fmt::format("{}.{}", exchg, code);     // 生成缓存键，格式：交易所.合约代码

	std::string path = fmt::format("{}rt/ticks/{}/{}.dmb", _base_dir.c_str(), exchg, code); // 构建实时Tick数据文件路径
	if (!StdFile::exists(path.c_str()))                     // 如果文件不存在
		return NULL;                                         // 返回空指针

	TickBlockPair& block = _rt_tick_map[key];               // 获取或创建Tick数据块对
	if (block._file == NULL || block._block == NULL)        // 如果文件映射或数据块为空
	{
		if (block._file == NULL)                            // 如果文件映射对象为空
		{
			block._file.reset(new BoostMappingFile());      // 创建新的内存映射文件对象
		}

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 映射文件到内存
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTTickBlock*)block._file->addr();    // 获取数据块地址
		block._last_cap = block._block->_capacity;           // 记录当前容量
	}
	else if (block._last_cap != block._block->_capacity)    // 如果文件大小已变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());           // 重新创建文件映射对象
		block._last_cap = 0;                                 // 重置容量记录
		block._block = NULL;                                // 清空数据块指针

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 重新映射文件
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTTickBlock*)block._file->addr();    // 获取新的数据块地址
		block._last_cap = block._block->_capacity;           // 更新容量记录
	}

	block._last_time = TimeUtils::getLocalTimeNow();        // 更新最后访问时间
	return &block;                                          // 返回数据块指针
}

/*!
 * \brief 获取实时委托明细数据块
 * \param exchg 交易所代码，如"SHFE"、"DCE"等
 * \param code 合约代码，如"rb2305"
 * \return 委托明细数据块指针，失败返回NULL
 * 
 * 该函数用于获取指定合约的实时委托明细数据块，包含逐笔委托的详细信息。
 * 使用内存映射文件技术提高访问性能，支持文件大小变化时的自动重新映射。
 */
WtRdmDtReader::OrdDtlBlockPair* WtRdmDtReader::getRTOrdDtlBlock(const char* exchg, const char* code)
{
	std::string key = fmt::format("{}.{}", exchg, code);     // 生成缓存键，格式：交易所.合约代码

	std::string path = fmt::format("{}rt/orders/{}/{}.dmb", _base_dir.c_str(), exchg, code); // 构建实时委托明细数据文件路径
	if (!StdFile::exists(path.c_str()))                     // 如果文件不存在
		return NULL;                                         // 返回空指针

	OrdDtlBlockPair& block = _rt_orddtl_map[key];           // 获取或创建委托明细数据块对
	if (block._file == NULL || block._block == NULL)        // 如果文件映射或数据块为空
	{
		if (block._file == NULL)                            // 如果文件映射对象为空
		{
			block._file.reset(new BoostMappingFile());      // 创建新的内存映射文件对象
		}

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 映射文件到内存
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTOrdDtlBlock*)block._file->addr(); // 获取数据块地址
		block._last_cap = block._block->_capacity;          // 记录当前容量
	}
	else if (block._last_cap != block._block->_capacity)    // 如果文件大小已变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());           // 重新创建文件映射对象
		block._last_cap = 0;                                 // 重置容量记录
		block._block = NULL;                                // 清空数据块指针

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 重新映射文件
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTOrdDtlBlock*)block._file->addr(); // 获取新的数据块地址
		block._last_cap = block._block->_capacity;           // 更新容量记录
	}

	block._last_time = TimeUtils::getLocalTimeNow();        // 更新最后访问时间
	return &block;                                          // 返回数据块指针
}

/*!
 * \brief 获取实时委托队列数据块
 * \param exchg 交易所代码，如"SHFE"、"DCE"等
 * \param code 合约代码，如"rb2305"
 * \return 委托队列数据块指针，失败返回NULL
 * 
 * 该函数用于获取指定合约的实时委托队列数据块，包含买卖盘口的委托信息。
 * 使用内存映射文件技术提高访问性能，支持文件大小变化时的自动重新映射。
 */
WtRdmDtReader::OrdQueBlockPair* WtRdmDtReader::getRTOrdQueBlock(const char* exchg, const char* code)
{
	std::string key = fmt::format("{}.{}", exchg, code);     // 生成缓存键，格式：交易所.合约代码

	std::string path = fmt::format("{}rt/queue/{}/{}.dmb", _base_dir.c_str(), exchg, code); // 构建实时委托队列数据文件路径
	if (!StdFile::exists(path.c_str()))                     // 如果文件不存在
		return NULL;                                         // 返回空指针

	OrdQueBlockPair& block = _rt_ordque_map[key];           // 获取或创建委托队列数据块对
	if (block._file == NULL || block._block == NULL)        // 如果文件映射或数据块为空
	{
		if (block._file == NULL)                            // 如果文件映射对象为空
		{
			block._file.reset(new BoostMappingFile());      // 创建新的内存映射文件对象
		}

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 映射文件到内存
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTOrdQueBlock*)block._file->addr(); // 获取数据块地址
		block._last_cap = block._block->_capacity;           // 记录当前容量
	}
	else if (block._last_cap != block._block->_capacity)    // 如果文件大小已变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());           // 重新创建文件映射对象
		block._last_cap = 0;                                 // 重置容量记录
		block._block = NULL;                                // 清空数据块指针

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 重新映射文件
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTOrdQueBlock*)block._file->addr(); // 获取新的数据块地址
		block._last_cap = block._block->_capacity;           // 更新容量记录
	}

	block._last_time = TimeUtils::getLocalTimeNow();        // 更新最后访问时间
	return &block;                                          // 返回数据块指针
}

/*!
 * \brief 获取实时逐笔成交数据块
 * \param exchg 交易所代码，如"SHFE"、"DCE"等
 * \param code 合约代码，如"rb2305"
 * \return 逐笔成交数据块指针，失败返回NULL
 * 
 * 该函数用于获取指定合约的实时逐笔成交数据块，包含每笔成交的详细信息。
 * 使用内存映射文件技术提高访问性能，支持文件大小变化时的自动重新映射。
 */
WtRdmDtReader::TransBlockPair* WtRdmDtReader::getRTTransBlock(const char* exchg, const char* code)
{
	std::string key = fmt::format("{}.{}", exchg, code);     // 生成缓存键，格式：交易所.合约代码

	std::string path = fmt::format("{}rt/trans/{}/{}.dmb", _base_dir.c_str(), exchg, code); // 构建实时逐笔成交数据文件路径
	if (!StdFile::exists(path.c_str()))                     // 如果文件不存在
		return NULL;                                         // 返回空指针

	TransBlockPair& block = _rt_trans_map[key];             // 获取或创建逐笔成交数据块对
	if (block._file == NULL || block._block == NULL)        // 如果文件映射或数据块为空
	{
		if (block._file == NULL)                            // 如果文件映射对象为空
		{
			block._file.reset(new BoostMappingFile());      // 创建新的内存映射文件对象
		}

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 映射文件到内存
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTTransBlock*)block._file->addr();  // 获取数据块地址
		block._last_cap = block._block->_capacity;            // 记录当前容量
	}
	else if (block._last_cap != block._block->_capacity)    // 如果文件大小已变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());           // 重新创建文件映射对象
		block._last_cap = 0;                                 // 重置容量记录
		block._block = NULL;                                // 清空数据块指针

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 重新映射文件
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTTransBlock*)block._file->addr();  // 获取新的数据块地址
		block._last_cap = block._block->_capacity;            // 更新容量记录
	}

	block._last_time = TimeUtils::getLocalTimeNow();        // 更新最后访问时间
	return &block;                                          // 返回数据块指针
}

/*!
 * \brief 获取实时K线数据块
 * \param exchg 交易所代码，如"SHFE"、"DCE"等
 * \param code 合约代码，如"rb2305"
 * \param period K线周期，仅支持1分钟和5分钟
 * \return K线数据块指针，失败返回NULL
 * 
 * 该函数用于获取指定合约的实时K线数据块，仅支持1分钟和5分钟周期。
 * 使用内存映射文件技术提高访问性能，支持文件大小变化时的自动重新映射。
 */
WtRdmDtReader::RTKlineBlockPair* WtRdmDtReader::getRTKilneBlock(const char* exchg, const char* code, WTSKlinePeriod period)
{
	if (period != KP_Minute1 && period != KP_Minute5)      // 仅支持1分钟和5分钟K线
		return NULL;                                         // 其他周期返回空指针

	char key[64] = { 0 };                                   // 缓存键缓冲区
	fmtutil::format_to(key, "{}.{}", exchg, code);          // 生成缓存键，格式：交易所.合约代码

	std::string subdir = "";                                // 子目录名称
	switch (period)                                         // 根据K线周期设置子目录
	{
	case KP_Minute1:                                        // 1分钟K线
		subdir = "min1";
		break;
	case KP_Minute5:                                        // 5分钟K线
		subdir = "min5";
		break;
	default: 
		return NULL;                                         // 其他周期返回空指针
	}

	std::string path = fmtutil::format("{}rt/{}/{}/{}.dmb", _base_dir.c_str(), subdir.c_str(), exchg, code); // 构建实时K线数据文件路径
	if (!StdFile::exists(path.c_str()))                     // 如果文件不存在
		return NULL;                                         // 返回空指针

	RTKlineBlockPair& block = (period == KP_Minute1 ? _rt_min1_map[key] : _rt_min5_map[key]); // 根据周期选择对应的缓存映射
	if (block._file == NULL || block._block == NULL)        // 如果文件映射或数据块为空
	{
		if (block._file == NULL)                            // 如果文件映射对象为空
		{
			block._file.reset(new BoostMappingFile());      // 创建新的内存映射文件对象
		}

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 映射文件到内存
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTKlineBlock*)block._file->addr();  // 获取数据块地址
		block._last_cap = block._block->_capacity;            // 记录当前容量
	}
	else if (block._last_cap != block._block->_capacity)    // 如果文件大小已变化
	{
		//说明文件大小已变, 需要重新映射
		block._file.reset(new BoostMappingFile());           // 重新创建文件映射对象
		block._last_cap = 0;                                 // 重置容量记录
		block._block = NULL;                                // 清空数据块指针

		if (!block._file->map(path.c_str(), boost::interprocess::read_only, boost::interprocess::read_only)) // 重新映射文件
			return NULL;                                     // 映射失败返回空指针

		block._block = (RTKlineBlock*)block._file->addr();  // 获取新的数据块地址
		block._last_cap = block._block->_capacity;            // 更新容量记录
	}

	block._last_time = TimeUtils::getLocalTimeNow();        // 更新最后访问时间
	return &block;                                          // 返回数据块指针
}

/*!
 * \brief 按数量读取K线数据切片
 * \param stdCode 标准合约代码，格式如"SHFE.rb.HOT"
 * \param period K线周期，支持日线、1分钟、5分钟等
 * \param count 需要读取的K线数量
 * \param etime 结束时间（微秒时间戳，默认为0表示到未来）
 * \return K线数据切片指针，失败返回NULL
 * 
 * 该函数是WonderTrader框架中按数量读取K线数据的核心方法，支持从指定时间点向前读取指定数量的K线。
 * 自动处理期货合约的主力切换，支持历史数据和实时数据的混合读取。
 * 优先从实时数据中读取，不足时从历史数据中补充。
 */
WTSKlineSlice* WtRdmDtReader::readKlineSliceByCount(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码，提取交易所、品种、合约等信息
	pipe_rdmreader_log(_sink, LL_INFO, "CodeInfo of {}: {},{},{}", stdCode, cInfo._exchg, cInfo._product, cInfo._code); // 记录代码信息日志
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息，包含交易规则、复权信息等
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID，用于交易日计算

	std::string key = fmtutil::format("{}#{}", stdCode, period); // 生成缓存键，格式：合约代码#周期
	auto it = _bars_cache.find(key);                        // 查找K线数据缓存，避免重复加载
	bool bHasHisData = false;                               // 是否有历史数据标志
	if (it == _bars_cache.end())                           // 如果没有找到缓存
	{
		bHasHisData = cacheHisBarsFromFile(&cInfo, key, stdCode, period); // 从文件缓存历史K线数据到内存
	}
	else                                                     // 如果找到缓存
	{
		bHasHisData = true;                                 // 标记有历史数据
	}

	if (etime == 0)                                          // 如果结束时间为0
		etime = 203012312359;                               // 设置默认结束时间为2030年12月31日23:59

	uint32_t rDate, rTime;                                  // 右日期、右时间
	rDate = (uint32_t)(etime / 10000);                      // 计算右日期（结束日期）
	rTime = (uint32_t)(etime % 10000);                      // 计算右时间（结束时间）

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日，考虑节假日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日

	WTSBarStruct* hisHead = NULL;                          // 历史K线数据头指针
	WTSBarStruct* rtHead = NULL;                            // 实时K线数据头指针
	uint32_t hisCnt = 0;                                    // 历史K线数量
	uint32_t rtCnt = 0;                                     // 实时K线数量

	std::string pname;                                      // 周期名称
	switch (period)                                         // 根据K线周期设置周期名称，用于文件路径构建
	{
	case KP_Minute1: pname = "min1"; break;                // 1分钟K线
	case KP_Minute5: pname = "min5"; break;                 // 5分钟K线
	default: pname = "day"; break;                           // 日K线
	}

	bool isDay = period == KP_DAY;                          // 判断是否为日线周期

	//是否包含当天的
	bool bHasToday = (endTDate >= curTDate);                // 判断查询时间范围是否包含当天
	std::string raw_code = cInfo._code;                    // 原始合约代码

	const char* ruleTag = cInfo._ruletag;                   // 获取规则标签，用于主力合约切换
	if (strlen(ruleTag) > 0)                                // 如果有规则标签（如期货主力合约）
	{
		raw_code = _hot_mgr->getCustomRawCode(ruleTag, stdPID, curTDate); // 获取指定日期的实际合约代码
		pipe_rdmreader_log(_sink, LL_INFO, "{} contract on {} confirmed: {} -> {}", ruleTag, curTDate, stdCode, raw_code.c_str()); // 记录合约切换日志
	}
	else
	{
		raw_code = cInfo._code;                             // 使用原始合约代码
	}

	WTSBarStruct eBar;                                     // 结束时间K线结构
	eBar.date = rDate;                                      // 设置结束日期
	eBar.time = (rDate - 19900000) * 10000 + rTime;          // 设置结束时间（转换为内部时间格式）

	bool bNeedHisData = true;                              // 是否需要历史数据标志

	if (bHasToday)                                          // 如果查询时间范围包含当天
	{
		const char* curCode = raw_code.c_str();             // 获取当前实际合约代码
		if(cInfo._exright != 2)                              // 如果不是后复权模式
		{
			//读取实时的
			RTKlineBlockPair* kPair = getRTKilneBlock(cInfo._exchg, curCode, period); // 获取实时K线数据块
			if (kPair != NULL)                               // 如果找到实时数据块
			{
				StdUniqueLock lock(*(kPair->_mtx));          // 加锁保护实时数据访问
				//读取当日的数据
				WTSBarStruct* pBar = std::lower_bound(kPair->_block->_bars, kPair->_block->_bars + (kPair->_block->_size - 1), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
					if (isDay)                               // 如果是日线，按日期比较
						return a.date < b.date;
					else                                     // 否则按时间比较
						return a.time < b.time;
				});
				std::size_t idx = pBar - kPair->_block->_bars; // 计算结束位置索引
				if ((isDay && pBar->date > eBar.date) || (!isDay && pBar->time > eBar.time)) // 如果定位的K线时间超过结束时间
				{
					pBar--;                                  // 回退一个K线
					idx--;                                  // 索引也回退
				}

				//如果第一条实时K线的时间大于开始日期，则实时K线要全部包含进去
				rtCnt = min((uint32_t)idx + 1, count);      // 计算实时K线数量，不超过请求数量
				std::size_t sIdx = idx + 1 - rtCnt;         // 计算开始位置索引
				rtHead = kPair->_block->_bars + sIdx;        // 设置实时数据头指针
				bNeedHisData = (rtCnt < count);              // 如果实时数据不足，需要历史数据
			}
		}
		else                                                 // 如果是后复权模式
		{
			RTKlineBlockPair* kPair = getRTKilneBlock(cInfo._exchg, curCode, period); // 获取实时K线数据块
			if (kPair != NULL)                               // 如果找到实时数据块
			{
				//如果是后复权，实时数据是需要单独缓存的，所以这里处理会很复杂
				BarsList& barsList = _bars_cache[key];       // 获取K线数据缓存列表

				//1、先检查缓存中有多少实时数据
				std::size_t oldSize = barsList._rt_bars.size(); // 获取缓存中实时数据大小
				std::size_t newSize = kPair->_block->_size;     // 获取原始实时数据大小

				//2、再看看原始实时数据有多少，如果不够，就要补充进来
				if(newSize > oldSize)                        // 如果原始数据比缓存数据多
				{
					barsList._rt_bars.resize(newSize);        // 调整缓存大小
					auto idx = oldSize;                       // 从旧大小开始
					if (oldSize != 0)                         // 如果之前有数据
						idx--;                                // 从倒数第二个开始，避免重复

					//因为每次拷贝，最后一条K线都有可能是未闭合的，所以需要把最后一条K线覆盖
					memcpy(&barsList._rt_bars[idx], &kPair->_block->_bars[idx], sizeof(WTSBarStruct)*(newSize - idx)); // 复制新增的K线数据

					//最后做复权处理
					double factor = barsList._factor;         // 获取复权因子
					for(; idx < newSize; idx++)               // 遍历新增的K线数据
					{
						WTSBarStruct* pBar = &barsList._rt_bars[idx]; // 获取K线指针
						pBar->open *= factor;                 // 开盘价复权
						pBar->high *= factor;                 // 最高价复权
						pBar->low *= factor;                  // 最低价复权
						pBar->close *= factor;                // 收盘价复权
					}
				}

				//最后做一个定位
				auto it = std::lower_bound(barsList._rt_bars.begin(), barsList._rt_bars.end(), eBar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
					if (isDay)                               // 如果是日线，按日期比较
						return a.date < b.date;
					else                                     // 否则按时间比较
						return a.time < b.time;
				});
				std::size_t idx = it - barsList._rt_bars.begin(); // 计算结束位置索引
				WTSBarStruct* pBar = &barsList._rt_bars[idx]; // 获取结束位置K线指针
				if ((isDay && pBar->date > eBar.date) || (!isDay && pBar->time > eBar.time)) // 如果定位的K线时间超过结束时间
				{
					pBar--;                                  // 回退一个K线
					idx--;                                  // 索引也回退
				}

				//如果第一条实时K线的时间大于开始日期，则实时K线要全部包含进去
				rtCnt = min((uint32_t)idx + 1, count);      // 计算实时K线数量，不超过请求数量
				std::size_t sIdx = idx + 1 - rtCnt;         // 计算开始位置索引
				rtHead = &barsList._rt_bars[sIdx];          // 设置实时数据头指针
				bNeedHisData = (rtCnt < count);              // 如果实时数据不足，需要历史数据
			}
		}
	}
	

	if (bNeedHisData)                                        // 如果需要历史数据
	{
		hisCnt = count - rtCnt;                              // 计算需要的历史K线数量
		hisHead = indexBarFromCacheByCount(key, etime, hisCnt, period == KP_DAY); // 从缓存中按数量获取历史K线数据
	}

	pipe_rdmreader_log(_sink, LL_DEBUG, "His {} bars of {} loaded, {} from history, {} from realtime", PERIOD_NAME[period], stdCode, hisCnt, rtCnt); // 记录数据加载日志

	if (hisCnt + rtCnt > 0)                                 // 如果历史数据或实时数据有内容
	{
		WTSKlineSlice* slice = WTSKlineSlice::create(stdCode, period, 1, hisHead, hisCnt); // 创建K线数据切片，先添加历史数据
		if (rtCnt > 0)                                       // 如果有实时数据
			slice->appendBlock(rtHead, rtCnt);              // 追加实时数据块
		return slice;                                        // 返回完整的K线数据切片
	}

	return NULL;                                            // 没有数据则返回空指针
}

/*!
 * \brief 按数量读取Tick数据切片
 * \param stdCode 标准合约代码，格式如"SHFE.rb.HOT"
 * \param count 需要读取的Tick数量
 * \param etime 结束时间（微秒时间戳，默认为0表示到未来）
 * \return Tick数据切片指针，失败返回NULL
 * 
 * 该函数是WonderTrader框架中按数量读取Tick数据的核心方法，支持从指定时间点向前读取指定数量的Tick。
 * 自动处理期货合约的主力切换，支持历史数据和实时数据的混合读取。
 * 优先从实时数据中读取，不足时从历史数据中补充。
 */
WTSTickSlice* WtRdmDtReader::readTickSliceByCount(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码，提取交易所、品种、合约等信息
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息，包含交易规则、复权信息等
	const char* stdPID = commInfo->getFullPid();             // 获取标准产品ID，用于交易日计算

	WTSSessionInfo* sInfo = _base_data_mgr->getSession(_base_data_mgr->getCommodity(cInfo._exchg, cInfo._code)->getSession()); // 获取交易时段信息

	uint32_t rDate, rTime, rSecs;                           // 右日期、右时间、右秒数
	//20190807124533900
	rDate = (uint32_t)(etime / 1000000000);                // 计算右日期（结束日期）
	rTime = (uint32_t)(etime % 1000000000) / 100000;        // 计算右时间（结束时间，分钟）
	rSecs = (uint32_t)(etime % 100000);                    // 计算右秒数（结束秒数）

	uint32_t endTDate = _base_data_mgr->calcTradingDate(stdPID, rDate, rTime, false); // 计算结束交易日，考虑节假日
	uint32_t curTDate = _base_data_mgr->calcTradingDate(stdPID, 0, 0, false); // 计算当前交易日

	bool hasToday = (endTDate >= curTDate);                // 判断查询时间范围是否包含当天

	WTSTickSlice* slice = WTSTickSlice::create(stdCode);   // 创建Tick数据切片

	uint32_t left = count;                                  // 剩余需要读取的Tick数量
	while (hasToday)                                         // 当查询时间范围包含当天时
	{
		std::string curCode = cInfo._code;                    // 获取当前合约代码
		if(commInfo->isFuture())                              // 如果是期货合约
		{
			const char* ruleTag = cInfo._ruletag;             // 获取规则标签，用于主力合约切换
			if (strlen(ruleTag) > 0)                          // 如果有规则标签（如期货主力合约）
			{
				curCode = _hot_mgr->getCustomRawCode(ruleTag, stdPID, curTDate); // 获取指定日期的实际合约代码

				pipe_rdmreader_log(_sink, LL_INFO, "{} contract on {} confirmed: {} -> {}", ruleTag, curTDate, stdCode, curCode.c_str()); // 记录合约切换日志
			}
		}		

		TickBlockPair* tPair = getRTTickBlock(cInfo._exchg, curCode.c_str()); // 获取实时Tick数据块
		if (tPair == NULL || tPair->_block->_size == 0)      // 如果没有找到数据块或数据为空
			break;                                            // 跳出循环

		StdUniqueLock lock(*tPair->_mtx);                    // 加锁保护实时数据访问
		RTTickBlock* tBlock = tPair->_block;                 // 获取Tick数据块指针
		WTSTickStruct eTick;                                 // 结束时间Tick结构
		if (curTDate == endTDate)                            // 如果当前交易日等于结束交易日
		{
			eTick.action_date = rDate;                       // 设置结束日期
			eTick.action_time = rTime * 100000 + rSecs;      // 设置结束时间（转换为微秒）
		}
		else                                                 // 否则使用交易日收盘时间
		{
			eTick.action_date = curTDate;                    // 设置当前交易日
			eTick.action_time = sInfo->getCloseTime() * 100000 + 59999; // 设置收盘时间（微秒）
		}

		WTSTickStruct* pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + (tBlock->_size - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
			if (a.action_date != b.action_date)              // 如果日期不同
				return a.action_date < b.action_date;         // 按日期比较
			else                                             // 否则按时间比较
				return a.action_time < b.action_time;
		});

		std::size_t eIdx = pTick - tBlock->_ticks;           // 计算结束位置索引

		//如果光标定位的tick时间比目标时间大, 则全部回退一个
		if (pTick->action_date > eTick.action_date || pTick->action_time > eTick.action_time) // 如果定位的Tick时间超过结束时间
		{
			pTick--;                                         // 回退一个Tick
			eIdx--;                                          // 索引也回退
		}

		uint32_t thisCnt = min((uint32_t)eIdx + 1, left);    // 计算本次读取的Tick数量，不超过剩余数量
		uint32_t sIdx = eIdx + 1 - thisCnt;                   // 计算开始位置索引
		slice->insertBlock(0, tBlock->_ticks + sIdx, thisCnt); // 将Tick数据插入到切片开头
		left -= thisCnt;                                      // 减少剩余需要读取的数量
		break;                                                // 跳出循环（只处理当天数据）
	}

	uint32_t nowTDate = min(endTDate, curTDate);             // 计算当前处理的交易日
	if (nowTDate == curTDate)                                // 如果当前交易日等于今天
		nowTDate = TimeUtils::getNextDate(nowTDate, -1);     // 回退到前一天
	uint32_t missingCnt = 0;                                // 连续缺失数据计数
	while (left > 0)                                         // 当还有剩余Tick需要读取时
	{
		if(missingCnt >= 30)                                 // 如果连续缺失30个交易日的数据
			break;                                            // 跳出循环，避免无限循环

		std::string curCode = cInfo._code;                    // 获取当前合约代码
		std::string hotCode;                                  // 主力合约代码
		if(commInfo->isFuture())                              // 如果是期货合约
		{
			const char* ruleTag = cInfo._ruletag;             // 获取规则标签，用于主力合约切换
			if (strlen(ruleTag) > 0)                          // 如果有规则标签（如期货主力合约）
			{
				curCode = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), nowTDate); // 获取指定日期的实际合约代码

				hotCode = cInfo._product;                     // 获取品种代码
				hotCode += "_";                               // 添加下划线
				hotCode += ruleTag;                           // 添加规则标签
				pipe_rdmreader_log(_sink, LL_INFO, "{} contract on {} confirmed: {} -> {}", ruleTag, curTDate, stdCode, curCode.c_str()); // 记录合约切换日志
			}
			//else if (cInfo.isHot())
			//{
			//	curCode = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, nowTDate);
			//	hotCode = cInfo._product;
			//	hotCode += "_HOT";
			//	pipe_rdmreader_log(_sink, LL_INFO, "Hot contract on {} confirmed: {} -> {}", curTDate, stdCode, curCode.c_str());
			//}
			//else if (cInfo.isSecond())
			//{
			//	curCode = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, nowTDate);
			//	hotCode = cInfo._product;
			//	hotCode += "_2ND";
			//	pipe_rdmreader_log(_sink, LL_INFO, "Second contract on {} confirmed: {} -> {}", curTDate, stdCode, curCode.c_str());
			//}
		}
		

		std::string key = fmt::format("{}-{}", stdCode, nowTDate); // 生成历史Tick数据缓存键

		auto it = _his_tick_map.find(key);                    // 查找历史Tick数据缓存
		bool bHasHisTick = (it != _his_tick_map.end());       // 是否有历史Tick数据标志
		if (!bHasHisTick)                                     // 如果没有找到缓存
		{
			for (;;)                                          // 无限循环尝试加载数据
			{
				std::string filename;                         // 文件名
				bool bHitHot = false;                          // 是否命中主力合约文件
				if(!hotCode.empty())                          // 如果有主力合约代码
				{
					std::stringstream ss;                     // 字符串流
					ss << _base_dir << "his/ticks/" << cInfo._exchg << "/" << nowTDate << "/" << hotCode << ".dsb"; // 构建主力合约文件路径
					filename = ss.str();                      // 获取文件路径字符串
					if (StdFile::exists(filename.c_str()))    // 如果主力合约文件存在
					{
						bHitHot = true;                       // 标记命中主力合约
					}
				}

				if(!bHitHot)                                  // 如果没有命中主力合约文件
				{
					std::stringstream ss;                     // 字符串流
					ss << _base_dir << "his/ticks/" << cInfo._exchg << "/" << nowTDate << "/" << curCode << ".dsb"; // 构建普通合约文件路径
					filename = ss.str();                      // 获取文件路径字符串
					if (!StdFile::exists(filename.c_str()))   // 如果文件不存在
					{
						missingCnt++;                         // 增加缺失计数
						break;                                 // 跳出循环
					}
				}

				missingCnt = 0;                               // 重置缺失计数

				HisTBlockPair& tBlkPair = _his_tick_map[key]; // 获取或创建历史Tick数据块对
				StdFile::read_file_content(filename.c_str(), tBlkPair._buffer); // 读取文件内容到缓冲区
				if (tBlkPair._buffer.size() < sizeof(HisTickBlock)) // 如果文件大小小于数据块头部大小
				{
					pipe_rdmreader_log(_sink, LL_ERROR, "Sizechecking of his tick data file {} failed", filename.c_str()); // 记录错误日志
					tBlkPair._buffer.clear();                 // 清空缓冲区
					break;                                     // 跳出循环
				}

				proc_block_data(tBlkPair._buffer, false, true); // 处理数据块，解压缩等操作
				tBlkPair._block = (HisTickBlock*)tBlkPair._buffer.c_str(); // 设置数据块指针
				bHasHisTick = true;                           // 标记有历史Tick数据
				break;                                         // 跳出循环
			}
		}

		while (bHasHisTick)                                  // 当有历史Tick数据时
		{
			//比较时间的对象
			WTSTickStruct eTick;                             // 结束时间Tick结构
			if (nowTDate == endTDate)                        // 如果当前交易日等于结束交易日
			{
				eTick.action_date = rDate;                   // 设置结束日期
				eTick.action_time = rTime * 100000 + rSecs;  // 设置结束时间（转换为微秒）
			}
			else                                             // 否则使用交易日收盘时间
			{
				eTick.action_date = nowTDate;                // 设置当前交易日
				eTick.action_time = sInfo->getCloseTime() * 100000 + 59999; // 设置收盘时间（微秒）
			}

			HisTBlockPair& tBlkPair = _his_tick_map[key];    // 获取历史Tick数据块对
			if (tBlkPair._block == NULL)                     // 如果数据块为空
				break;                                        // 跳出循环

			HisTickBlock* tBlock = tBlkPair._block;          // 获取历史Tick数据块指针

			uint32_t tcnt = (tBlkPair._buffer.size() - sizeof(HisTickBlock)) / sizeof(WTSTickStruct); // 计算Tick数据数量
			if (tcnt <= 0)                                   // 如果没有Tick数据
				break;                                        // 跳出循环

			WTSTickStruct* pTick = std::lower_bound(tBlock->_ticks, tBlock->_ticks + (tcnt - 1), eTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {
				if (a.action_date != b.action_date)          // 如果日期不同
					return a.action_date < b.action_date;     // 按日期比较
				else                                         // 否则按时间比较
					return a.action_time < b.action_time;
			});

			std::size_t eIdx = pTick - tBlock->_ticks;       // 计算结束位置索引
			if (pTick->action_date > eTick.action_date || pTick->action_time >= eTick.action_time) // 如果定位的Tick时间超过结束时间
			{
				pTick--;                                     // 回退一个Tick
				eIdx--;                                      // 索引也回退
			}

			uint32_t thisCnt = min((uint32_t)eIdx + 1, left); // 计算本次读取的Tick数量，不超过剩余数量
			uint32_t sIdx = eIdx + 1 - thisCnt;               // 计算开始位置索引
			slice->insertBlock(0, tBlock->_ticks + sIdx, thisCnt); // 将Tick数据插入到切片开头
			left -= thisCnt;                                  // 减少剩余需要读取的数量
			break;                                            // 跳出循环（只处理一天数据）
		}

		nowTDate = TimeUtils::getNextDate(nowTDate, -1);     // 回退到前一天
	}

	return slice;                                            // 返回Tick数据切片
}

/*!
 * \brief 根据日期获取复权因子
 * \param stdCode 标准合约代码，格式如"SHFE.rb.HOT"
 * \param date 查询日期（格式：YYYYMMDD，默认为0表示当前日期）
 * \return 复权因子，非股票返回1.0
 * 
 * 该函数用于获取指定日期和合约的复权因子，主要用于股票数据的复权处理。
 * 支持前复权和后复权模式，自动处理除权除息对价格的影响。
 * 使用二分查找算法快速定位指定日期的复权因子。
 */
double WtRdmDtReader::getAdjFactorByDate(const char* stdCode, uint32_t date /* = 0 */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr); // 解析标准合约代码，提取交易所、品种、合约等信息
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(cInfo._exchg, cInfo._product); // 获取商品信息，包含交易规则、复权信息等
	if (!commInfo->isStock())                              // 如果不是股票
		return 1.0;                                          // 返回默认复权因子1.0

	AdjFactor factor = { date, 1.0 };                      // 创建复权因子结构，用于查找

	std::string key = stdCode;                              // 使用标准合约代码作为键
	if (cInfo.isExright())                                  // 如果是复权模式
		key = key.substr(0, key.size() - 1);                 // 去掉复权标识符，获取基础代码
	const AdjFactorList& factList = _adj_factors[key];      // 获取复权因子列表
	if (factList.empty())                                   // 如果复权因子列表为空
		return 1.0;                                          // 返回默认复权因子1.0

	auto it = std::lower_bound(factList.begin(), factList.end(), factor, [](const AdjFactor& a, const AdjFactor&b) {
		return a._date < b._date;                           // 按日期升序比较
	});

	if (it == factList.end())                              // 如果找不到匹配的日期
	{
		//找不到，则说明目标日期大于最后一条的日期，直接返回最后一条除权因子
		return factList.back()._factor;                     // 返回最后一个复权因子
	}
	else                                                    // 如果找到匹配的日期
	{
		//如果找到了，但是命中的日期大于目标日期，则用上一条
		//如果等于目标日期，则用命中这一条
		if ((*it)._date > date)                             // 如果命中的日期大于目标日期
			it--;                                           // 使用前一个复权因子

		return (*it)._factor;                               // 返回对应的复权因子
	}
}

/*!
 * \brief 清理所有数据缓存
 * 
 * 该函数用于清理随机数据读取器中的所有内存缓存，包括：
 * - K线数据缓存
 * - 实时1分钟K线数据缓存
 * - 实时5分钟K线数据缓存
 * - 实时Tick数据缓存
 * - 实时逐笔成交数据缓存
 * - 实时委托明细数据缓存
 * - 实时委托队列数据缓存
 * 
 * 通常在系统重启或内存不足时调用，释放所有缓存占用的内存。
 */
void WtRdmDtReader::clearCache()
{
	_bars_cache.clear();                                    // 清理K线数据缓存

	_rt_min1_map.clear();                                   // 清理实时1分钟K线数据缓存
	_rt_min5_map.clear();                                   // 清理实时5分钟K线数据缓存

	_rt_tick_map.clear();                                   // 清理实时Tick数据缓存
	_rt_trans_map.clear();                                  // 清理实时逐笔成交数据缓存
	_rt_orddtl_map.clear();                                 // 清理实时委托明细数据缓存
	_rt_ordque_map.clear();                                 // 清理实时委托队列数据缓存
}