/*!
 * \file HisDataReplayer.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 历史数据回放器实现文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是HisDataReplayer类的实现文件，提供了历史数据回放器的所有功能实现。
 *
 * 主要功能模块：
 * 1. 数据加载：从二进制文件、CSV文件、外部加载器加载各种历史数据
 * 2. 数据缓存：管理K线、Tick、订单队列、订单明细、逐笔成交等数据缓存
 * 3. 数据回放：支持按K线、按Tick、按定时任务三种回放模式
 * 4. Tick模拟：从K线数据模拟生成Tick数据
 * 5. 复权处理：处理股票的复权数据和复权因子
 * 6. 手续费计算：根据合约配置计算手续费
 * 7. 订阅管理：管理各种数据类型的订阅关系
 * 8. 缓存管理：自动清理过期缓存，优化内存使用
 *
 * 核心算法：
 * - 游标机制：使用游标跟踪数据回放进度
 * - 时间同步：同步多种数据类型的时间戳
 * - 数据合并：合并期货主力合约数据
 * - 复权计算：根据复权因子调整价格和成交量
 * - Tick模拟：从K线数据生成Tick数据
 */
#include "HisDataReplayer.h"                                         // 历史数据回放器头文件
#include "EventNotifier.h"                                           // 事件通知器头文件
#include "WtHelper.h"                                                // WonderTrader辅助函数

#include <fstream>                                                    // 文件流操作

#include "../Includes/WTSVariant.hpp"                                // 变体类型定义
#include "../Includes/WTSDataDef.hpp"                                // WonderTrader数据定义
#include "../Includes/WTSContractInfo.hpp"                           // 合约信息定义
#include "../Includes/WTSSessionInfo.hpp"                            // 交易时段信息定义
#include "../Includes/WTSVariant.hpp"                                // 变体类型定义（重复包含）

#include "../Share/decimal.h"                                        // 小数精度计算工具
#include "../Share/StrUtil.hpp"                                      // 字符串工具函数
#include "../Share/TimeUtils.hpp"                                    // 时间工具函数

#include "../WTSTools/WTSLogger.h"                                   // 日志工具
#include "../WTSTools/WTSDataFactory.h"                              // 数据工厂
#include "../WTSTools/CsvHelper.h"                                   // CSV辅助工具

#include "../WTSUtils/WTSCmpHelper.hpp"                              // 压缩辅助工具
#include "../WTSUtils/WTSCfgLoader.h"                               // 配置加载器

#include "../Share/CodeHelper.hpp"                                   // 代码辅助工具

#include <boost/filesystem.hpp>                                      // Boost文件系统库

#include <rapidjson/document.h>                                      // RapidJSON文档类
#include <rapidjson/prettywriter.h>                                  // RapidJSON格式化写入器
namespace rj = rapidjson;                                            // RapidJSON命名空间别名

using namespace std;                                                 // 使用标准命名空间

/**
 * @brief 处理块数据
 * 
 * 处理压缩或老版本的数据块，解压数据并转换为新版本格式
 * 
 * 处理流程：
 * 1. 检查数据块是否压缩或老版本
 * 2. 如果是压缩数据，解压
 * 3. 如果是老版本数据，转换为新版本格式
 * 4. 如果既未压缩也不是老版本，直接返回
 * 
 * @param tag 数据标签（用于日志）
 * @param content 数据内容（输入输出参数）
 * @param isBar 是否是K线数据（true-K线，false-Tick）
 * @param bKeepHead 是否保留数据头（默认true）
 * @return 是否处理成功
 */
bool proc_block_data(const char* tag, std::string& content, bool isBar, bool bKeepHead = true)
{
	BlockHeader* header = (BlockHeader*)content.data();              // 获取数据块头指针

	bool bCmped = header->is_compressed();                           // 检查是否压缩
	bool bOldVer = header->is_old_version();                         // 检查是否老版本

	//如果既没有压缩，也不是老版本结构体，则直接返回
	if (!bCmped && !bOldVer)                                         // 如果既未压缩也不是老版本
	{
		if (!bKeepHead)                                               // 如果不保留数据头
			content.erase(0, BLOCK_HEADER_SIZE);                      // 删除数据头
		return true;                                                  // 直接返回成功
	}

	std::string buffer;                                               // 数据缓冲区
	if (bCmped)                                                       // 如果数据已压缩
	{
		BlockHeaderV2* blkV2 = (BlockHeaderV2*)content.c_str();      // 获取V2版本数据块头

		if (content.size() != (sizeof(BlockHeaderV2) + blkV2->_size))  // 检查数据大小是否匹配
		{
			WTSLogger::error("Size check failed while processing {} data of {}", isBar ? "bar" : "tick", tag);  // 记录错误日志
			return false;                                              // 返回失败
		}

		//将文件头后面的数据进行解压
		buffer = WTSCmpHelper::uncompress_data(content.data() + BLOCK_HEADERV2_SIZE, blkV2->_size);  // 解压数据
	}
	else                                                              // 如果数据未压缩
	{
		if (!bOldVer)                                                 // 如果不是老版本
		{
			//如果不是老版本，直接返回
			if (!bKeepHead)                                           // 如果不保留数据头
				content.erase(0, BLOCK_HEADER_SIZE);                  // 删除数据头
			return true;                                              // 直接返回成功
		}
		else                                                           // 如果是老版本
		{
			buffer.append(content.data() + BLOCK_HEADER_SIZE, content.size() - BLOCK_HEADER_SIZE);  // 复制数据（跳过数据头）
		}
	}

	if (bOldVer)                                                      // 如果是老版本数据
	{
		if (isBar)                                                    // 如果是K线数据
		{
			std::string bufV2;                                        // V2版本数据缓冲区
			uint32_t barcnt = buffer.size() / sizeof(WTSBarStructOld);  // 计算K线条数
			bufV2.resize(barcnt * sizeof(WTSBarStruct));             // 分配V2版本数据空间
			WTSBarStruct* newBar = (WTSBarStruct*)bufV2.data();      // 获取新版本K线数据指针
			WTSBarStructOld* oldBar = (WTSBarStructOld*)buffer.data();  // 获取老版本K线数据指针
			for (uint32_t idx = 0; idx < barcnt; idx++)               // 遍历所有K线
			{
				newBar[idx] = oldBar[idx];                            // 复制K线数据（结构体转换）
			}
			buffer.swap(bufV2);                                       // 交换缓冲区

			WTSLogger::debug("{} bars of {} transferd to new version...", barcnt, tag);  // 记录调试日志
		}
		else                                                           // 如果是Tick数据
		{
			uint32_t tick_cnt = buffer.size() / sizeof(WTSTickStructOld);  // 计算Tick条数
			std::string bufv2;                                        // V2版本数据缓冲区
			bufv2.resize(sizeof(WTSTickStruct)*tick_cnt);             // 分配V2版本数据空间
			WTSTickStruct* newTick = (WTSTickStruct*)bufv2.data();    // 获取新版本Tick数据指针
			WTSTickStructOld* oldTick = (WTSTickStructOld*)buffer.data();  // 获取老版本Tick数据指针
			for (uint32_t i = 0; i < tick_cnt; i++)                   // 遍历所有Tick
			{
				newTick[i] = oldTick[i];                               // 复制Tick数据（结构体转换）
			}
			buffer.swap(bufv2);                                       // 交换缓冲区

			WTSLogger::debug("{} ticks of {} transferd to new version...", tick_cnt, tag);  // 记录调试日志
		}
	}

	if (bKeepHead)                                                    // 如果保留数据头
	{
		//原来的缓存，resize到文件头大小，再追加最终的数据
		content.resize(BLOCK_HEADER_SIZE);                            // 调整内容大小为数据头大小
		content.append(buffer);                                       // 追加处理后的数据

		//修改数据块的版本号
		header = (BlockHeader*)content.data();                        // 重新获取数据块头指针
		header->_version = BLOCK_VERSION_RAW_V2;                       // 设置版本号为V2
	}
	else                                                               // 如果不保留数据头
	{
		//不保留块头，直接跟数据做一个swap
		content.swap(buffer);                                         // 交换内容（只保留数据部分）
	}

	return true;                                                       // 返回成功
}

/**
 * @brief HisDataReplayer构造函数
 * 
 * 初始化所有成员变量为默认值
 */
HisDataReplayer::HisDataReplayer()
	: _listener(NULL)                                                 // 初始化数据接收器为NULL
	, _cur_date(0)                                                    // 初始化当前日期为0
	, _cur_time(0)                                                    // 初始化当前时间为0
	, _cur_secs(0)                                                    // 初始化当前秒数为0
	, _cur_tdate(0)                                                   // 初始化当前交易日期为0
	, _tick_enabled(true)                                            // 初始化Tick回放为启用
	, _opened_tdate(0)                                                // 初始化已打开交易日期为0
	, _closed_tdate(0)                                                // 初始化已关闭交易日期为0
	, _tick_simulated(true)                                           // 初始化Tick模拟为启用
	, _running(false)                                                 // 初始化运行状态为false
	, _begin_time(0)                                                  // 初始化开始时间为0
	, _end_time(0)                                                    // 初始化结束时间为0
	, _bt_loader(NULL)                                                // 初始化数据加载器为NULL
	, _min_period("d")                                                // 初始化最小周期为日线
	, _cache_clear_days(0)                                            // 初始化缓存清理天数为0
	, _align_by_section(false)                                        // 初始化按小节对齐为false
{
}


/**
 * @brief HisDataReplayer析构函数
 */
HisDataReplayer::~HisDataReplayer()
{
}


/**
 * @brief 初始化历史数据回放器
 * 
 * 1. 设置事件通知器和数据加载器
 * 2. 读取回放模式配置
 * 3. 读取数据存储路径配置
 * 4. 初始化历史数据管理器（如果使用storage/bin/wtp模式）
 * 5. 读取回测时间范围配置
 * 6. 读取缓存清理天数配置
 * 7. 读取Tick回放开关配置
 * 8. 读取复权标记配置
 * 9. 读取基础数据文件配置（交易时段、合约信息等）
 * 10. 加载手续费配置
 * 
 * @param cfg 配置信息
 * @param notifier 事件通知器（可选，默认NULL）
 * @param dataLoader 数据加载器（可选，默认NULL）
 * @return 是否初始化成功
 */
bool HisDataReplayer::init(WTSVariant* cfg, EventNotifier* notifier /* = NULL */, IBtDataLoader* dataLoader /* = NULL */)
{
	_notifier = notifier;                                             // 设置事件通知器
	_bt_loader = dataLoader;                                          // 设置数据加载器

	_mode = cfg->getCString("mode");                                  // 读取回放模式（bars/tasks/ticks）
	/*
	 *	By Wesley @ 2022.01.11
	 *	因为store可能会变复杂，所以这里做一个兼容处理
	 *	如果有store就读取store的path，如果没有store，就还读取root的path
	 */
	if (cfg->has("store"))                                            // 如果配置中有store节点
	{
		_base_dir = StrUtil::standardisePath(cfg->get("store")->getCString("path"));  // 从store节点读取路径
	}
	else                                                               // 如果配置中没有store节点
	{
		_base_dir = StrUtil::standardisePath(cfg->getCString("path"));  // 从根节点读取路径
	}
	
	if(_mode == "storage" || _mode == "bin" || _mode == "wtp")      // 如果使用storage/bin/wtp模式
	{
		if (cfg->has("store"))                                        // 如果配置中有store节点
		{
			_his_dt_mgr.init(cfg->get("store"));                      // 使用store配置初始化历史数据管理器
		}
		else                                                           // 如果配置中没有store节点
		{
			WTSVariant* item = WTSVariant::createObject();           // 创建配置对象
			item->append("path", _base_dir.c_str());                  // 添加路径配置
			_his_dt_mgr.init(item);                                    // 使用路径配置初始化历史数据管理器
			item->release();                                          // 释放配置对象
		}
	}
	
	bool isRangeCfg = (_begin_time == 0 || _end_time == 0);//是否从配置文件读取回测区间  // 判断是否从配置文件读取回测区间
	if(_begin_time == 0)                                              // 如果开始时间为0
		_begin_time = cfg->getUInt64("stime");                        // 从配置读取开始时间

	if(_end_time == 0)                                                // 如果结束时间为0
		_end_time = cfg->getUInt64("etime");                          // 从配置读取结束时间
	WTSLogger::info("Backtest time range is set to be [{},{}] via config", _begin_time, _end_time);  // 记录日志

	_cache_clear_days = cfg->getUInt32("cache_clear_days");            // 读取缓存清理天数配置
	WTSLogger::info("Unused cache data will be cleard in {} days", _cache_clear_days);  // 记录日志
	

	_tick_enabled = cfg->getBoolean("tick");                          // 读取Tick回放开关配置
	WTSLogger::info("Tick data replaying is {}", _tick_enabled ? "enabled" : "disabled");  // 记录日志

	_adjust_flag = cfg->getUInt32("adjust_flag");                     // 读取复权标记配置
	WTSLogger::info("adjust_flag is {}", _adjust_flag);              // 记录日志

	_align_by_section = cfg->getBoolean("align_by_section");          // 读取按小节对齐配置
	WTSLogger::info("Resampled bars will be aligned by section: {}", _align_by_section ? "yes" : " no");  // 记录日志

	_nosim_if_notrade = cfg->getBoolean("dont_simtick_if_notrade");   // 读取无成交不模拟Tick配置
	WTSLogger::info("nosim_if_notrade is {}", _nosim_if_notrade);    // 记录日志

	//基础数据文件
	WTSVariant* cfgBF = cfg->get("basefiles");                        // 获取基础数据文件配置
	if (cfgBF->get("session"))                                         // 如果配置了交易时段文件
		_bd_mgr.loadSessions(cfgBF->getCString("session"));            // 加载交易时段配置

	WTSVariant* cfgItem = cfgBF->get("commodity");                    // 获取合约配置
	if (cfgItem)                                                       // 如果合约配置存在
	{
		if (cfgItem->type() == WTSVariant::VT_String)                  // 如果是字符串类型（单个文件）
		{
			_bd_mgr.loadCommodities(cfgItem->asCString());            // 加载合约配置文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)              // 如果是数组类型（多个文件）
		{
			for(uint32_t i = 0; i < cfgItem->size(); i ++)            // 遍历数组
			{
				_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());  // 加载合约配置文件
			}
		}
	}

	cfgItem = cfgBF->get("contract");                                  // 获取合约文件配置
	if (cfgItem)                                                       // 如果合约文件配置存在
	{
		if (cfgItem->type() == WTSVariant::VT_String)                  // 如果是字符串类型（单个文件）
		{
			_bd_mgr.loadContracts(cfgItem->asCString());               // 加载合约文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)              // 如果是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)             // 遍历数组
			{
				_bd_mgr.loadContracts(cfgItem->get(i)->asCString());  // 加载合约文件
			}
		}
	}

	if (cfgBF->get("holiday"))                                         // 如果配置了节假日文件
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));           // 加载节假日配置

	if (cfgBF->get("hot"))                                             // 如果配置了主力合约文件
		_hot_mgr.loadHots(cfgBF->getCString("hot"));                  // 加载主力合约配置

	if (cfgBF->get("second"))                                          // 如果配置了次主力合约文件
		_hot_mgr.loadSeconds(cfgBF->getCString("second"));            // 加载次主力合约配置

	if (cfgBF->has("rules"))                                           // 如果配置了自定义规则
	{
		auto cfgRules = cfgBF->get("rules");                          // 获取规则配置
		auto tags = cfgRules->memberNames();                          // 获取所有规则标签
		for (const std::string& ruleTag : tags)                       // 遍历所有规则标签
		{
			_hot_mgr.loadCustomRules(ruleTag.c_str(), cfgRules->getCString(ruleTag.c_str()));  // 加载自定义规则
			WTSLogger::info("{} rules loaded from {}", ruleTag, cfgRules->getCString(ruleTag.c_str()));  // 记录日志
		}
	}

	loadFees(cfg->getCString("fees"));                                 // 加载手续费配置

	/*
	 *	By Wesley @ 2021.12.20
	 *	先从extloader加载除权因子
	 *	如果加载失败，并且配置了除权因子文件，再加载除权因子文件
	 */
	bool bLoaded = loadStkAdjFactorsFromLoader();                     // 从外部加载器加载复权因子

	if (!bLoaded && cfg->has("adjfactor"))                           // 如果加载失败且配置了复权因子文件
		loadStkAdjFactorsFromFile(cfg->getCString("adjfactor"));      // 从文件加载复权因子

	return true;                                                       // 返回成功
}

/**
 * @brief 从外部加载器加载股票复权因子
 * 
 * 如果外部加载器存在，则调用其loadAllAdjFactors方法加载所有复权因子
 * 
 * @return 是否加载成功
 */
bool HisDataReplayer::loadStkAdjFactorsFromLoader()
{
	if (NULL == _bt_loader)                                           // 如果数据加载器不存在
		return false;                                                  // 返回失败

	bool ret = _bt_loader->loadAllAdjFactors(this, [](void* obj, const char* stdCode, uint32_t* dates, double* factors, uint32_t count) {  // 调用外部加载器加载所有复权因子
		HisDataReplayer* replayer = (HisDataReplayer*)obj;            // 获取回放器指针
		AdjFactorList& fctrLst = replayer->_adj_factors[stdCode];     // 获取该合约的复权因子列表

		for (uint32_t i = 0; i < count; i++)                          // 遍历所有复权因子
		{
			AdjFactor adjFact;                                         // 创建复权因子结构体
			adjFact._date = dates[i];                                  // 设置日期
			adjFact._factor = factors[i];                               // 设置复权因子

			fctrLst.emplace_back(adjFact);                            // 添加到复权因子列表
		}

		//一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
		AdjFactor adjFact;                                            // 创建默认复权因子
		adjFact._date = 19900101;                                      // 设置日期为1990年1月1日
		adjFact._factor = 1;                                          // 设置复权因子为1
		fctrLst.emplace_back(adjFact);                                // 添加到复权因子列表

		std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {  // 按日期排序复权因子列表
			return left._date < right._date;                          // 比较日期
		});
	});

	if (ret) WTSLogger::info("Adjusting factors of {} contracts loaded via extended loader", _adj_factors.size());  // 如果加载成功，记录日志
	return ret;                                                        // 返回加载结果
}

/**
 * @brief 从文件加载股票复权因子
 * 
 * 从JSON格式的复权因子文件中加载所有股票的复权因子数据
 * 
 * @param adjfile 复权因子文件路径
 * @return 是否加载成功
 */
bool HisDataReplayer::loadStkAdjFactorsFromFile(const char* adjfile)
{
	if (!StdFile::exists(adjfile))                                    // 如果文件不存在
	{
		WTSLogger::error("Adjust factor file {} not exists, skipped", adjfile);  // 记录错误日志
		return false;                                                  // 返回失败
	}

	std::string content;                                               // 文件内容
	StdFile::read_file_content(adjfile, content);                     // 读取文件内容

	WTSVariant* doc = WTSCfgLoader::load_from_file(adjfile);          // 从文件加载配置
	if (doc == NULL)                                                   // 如果加载失败
	{
		WTSLogger::error("Parsing adjust factor file {} faield", adjfile);  // 记录错误日志
		return false;                                                  // 返回失败
	}

	uint32_t stk_cnt = 0;                                             // 股票数量计数器
	uint32_t fct_cnt = 0;                                             // 复权因子数量计数器
	for (const std::string& exchg : doc->memberNames())              // 遍历所有交易所
	{
		WTSVariant* itemExchg = doc->get(exchg);                      // 获取交易所配置
		for (const std::string& code : itemExchg->memberNames())     // 遍历交易所下的所有合约代码
		{
			WTSVariant* ayFacts = itemExchg->get(code);              // 获取复权因子数组
			if (!ayFacts->isArray())                                  // 如果不是数组
				continue;                                              // 跳过

			/*
			 *	By Wesley @ 2021.12.21
			 *	先检查code的格式是不是包含PID，如STK.600000
			 *	如果包含PID，则直接格式化，如果不包含，则强制为STK
			 */
			bool bHasPID = (code.find('.') != std::string::npos);     // 检查是否包含产品ID

			std::string key;                                          // 标准化合约代码
			if(bHasPID)                                                // 如果包含产品ID
				key = fmt::format("{}.{}", exchg, code);             // 直接格式化
			else                                                       // 如果不包含产品ID
				key = fmt::format("{}.STK.{}", exchg, code);          // 强制添加STK产品ID
			stk_cnt++;                                                 // 增加股票计数

			AdjFactorList& fctrLst = _adj_factors[key];              // 获取该合约的复权因子列表
			for (uint32_t i = 0; i < ayFacts->size(); i++)            // 遍历所有复权因子
			{
				WTSVariant* fItem = ayFacts->get(i);                 // 获取复权因子项
				AdjFactor adjFact;                                    // 创建复权因子结构体
				adjFact._date = fItem->getUInt32("date");             // 读取日期
				adjFact._factor = fItem->getDouble("factor");         // 读取复权因子

				fctrLst.emplace_back(adjFact);                       // 添加到复权因子列表
				fct_cnt++;                                            // 增加复权因子计数
			}

			//一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
			AdjFactor adjFact;                                        // 创建默认复权因子
			adjFact._date = 19900101;                                  // 设置日期为1990年1月1日
			adjFact._factor = 1;                                      // 设置复权因子为1
			fctrLst.emplace_back(adjFact);                            // 添加到复权因子列表

			std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {  // 按日期排序复权因子列表
				return left._date < right._date;                      // 比较日期
			});
		}
	}

	WTSLogger::info("{} items of adjust factors for {} tickers loaded from {}", fct_cnt, stk_cnt, adjfile);  // 记录日志
	doc->release();                                                    // 释放配置对象
	return true;                                                       // 返回成功
}

/**
 * @brief 注册定时任务
 * 
 * 注册一个定时任务，用于在指定时间周期执行回调
 * 
 * @param taskid 任务ID
 * @param date 日期，根据周期变化：每日为0，每周为0~6（对应周日到周六），每月为1~31，每年为0101~1231
 * @param time 时间，精确到分钟
 * @param period 时间周期字符串（"d"-每日，"w"-每周，"m"-每月，"y"-每年，"min"-分钟）
 * @param trdtpl 交易日模板（默认"CHINA"）
 * @param session 交易时间模板（默认"TRADING"）
 */
void HisDataReplayer::register_task(uint32_t taskid, uint32_t date, uint32_t time, const char* period, const char* trdtpl /* = "CHINA" */, const char* session /* = "TRADING" */)
{
	TaskPeriodType ptype;                                             // 任务周期类型
	if (wt_stricmp(period, "d") == 0)                                 // 如果是"d"（每日）
		ptype = TPT_Daily;                                            // 设置为每日
	else if (wt_stricmp(period, "w") == 0)                            // 如果是"w"（每周）
		ptype = TPT_Weekly;                                           // 设置为每周
	else if (wt_stricmp(period, "m") == 0)                            // 如果是"m"（每月）
		ptype = TPT_Monthly;                                          // 设置为每月
	else if (wt_stricmp(period, "y") == 0)                            // 如果是"y"（每年）
		ptype = TPT_Yearly;                                           // 设置为每年
	else if (wt_stricmp(period, "min") == 0)                          // 如果是"min"（分钟）
		ptype = TPT_Minute;                                           // 设置为分钟
	else                                                               // 如果未匹配
		ptype = TPT_None;                                             // 设置为不重复

	_task.reset(new TaskInfo);                                        // 创建任务信息对象
	strcpy(_task->_name, "sel");                                      // 设置任务名称为"sel"
	strcpy(_task->_trdtpl, trdtpl);                                   // 设置交易日模板
	strcpy(_task->_session, session);                                  // 设置交易时间模板
	_task->_day = date;                                                // 设置日期
	_task->_time = time;                                               // 设置时间
	_task->_id = taskid;                                              // 设置任务ID
	_task->_period = ptype;                                            // 设置任务周期
	_task->_strict_time = true;                                        // 设置严格时间模式

	WTSLogger::info("Timed task registration succeed, frequency: {}", period);  // 记录日志
}

/**
 * @brief 清空所有缓存
 * 
 * 清空所有类型的数据缓存，包括Tick、订单明细、订单队列、逐笔成交、K线等缓存
 */
void HisDataReplayer::clear_cache()
{
	_ticks_cache.clear();                                             // 清空Tick缓存
	_orddtl_cache.clear();                                            // 清空订单明细缓存
	_ordque_cache.clear();                                            // 清空订单队列缓存
	_trans_cache.clear();                                             // 清空逐笔成交缓存

	_bars_cache.clear();                                              // 清空K线缓存
	_unbars_cache.clear();                                            // 清空未订阅K线缓存
	_unsubbed_in_need.clear();                                        // 清空未订阅但需要的K线集合

	_main_key = "";                                                    // 清空主键
	_main_period = "";                                                 // 清空主周期
	_tick_sub_map.clear();                                            // 清空Tick订阅映射表
	_min_period = "";                                                  // 清空最小周期
	_day_cache.clear();                                               // 清空每日Tick缓存
	_ticker_keys.clear();                                             // 清空Ticker键映射表

	_price_map.clear();                                                // 清空价格映射表

	WTSLogger::log_raw(LL_WARN, "All cached data cleared");           // 记录警告日志
}

/**
 * @brief 重置回放器状态
 * 
 * 重置不会清除缓存，而是将读取的游标标记还原，这样不用重复加载数据。
 * 将所有缓存的游标重置为UINT_MAX，表示未初始化状态。
 */
void HisDataReplayer::reset()
{
	//重置不会清除掉缓存，而是将读取的标记还原，这样不用重复加载主句
	for(auto& m : _ticks_cache)                                       // 遍历Tick缓存
	{
		HftDataList<WTSTickStruct>& cacheItem = (HftDataList<WTSTickStruct>&)m.second;  // 获取缓存项
		cacheItem._cursor = UINT_MAX;                                 // 重置游标为UINT_MAX
	}

	for (auto& m : _orddtl_cache)                                     // 遍历订单明细缓存
	{
		HftDataList<WTSOrdDtlStruct>& cacheItem = (HftDataList<WTSOrdDtlStruct>&)m.second;  // 获取缓存项
		cacheItem._cursor = UINT_MAX;                                 // 重置游标为UINT_MAX
	}

	for (auto& m : _ordque_cache)                                     // 遍历订单队列缓存
	{
		HftDataList<WTSOrdQueStruct>& cacheItem = (HftDataList<WTSOrdQueStruct>&)m.second;  // 获取缓存项
		cacheItem._cursor = UINT_MAX;                                 // 重置游标为UINT_MAX
	}

	for (auto& m : _trans_cache)                                      // 遍历逐笔成交缓存
	{
		HftDataList<WTSTransStruct>& cacheItem = (HftDataList<WTSTransStruct>&)m.second;  // 获取缓存项
		cacheItem._cursor = UINT_MAX;                                 // 重置游标为UINT_MAX
	}

	for (auto& m : _bars_cache)                                       // 遍历K线缓存
	{
		BarsListPtr& cacheItem = (BarsListPtr&)m.second;             // 获取缓存项
		cacheItem->_cursor = UINT_MAX;                                // 重置游标为UINT_MAX

		WTSLogger::info("Reading flag of {} has been reset", m.first.c_str());  // 记录信息日志
	}

	_unbars_cache.clear();                                            // 清空未订阅K线缓存

	_day_cache.clear();                                               // 清空每日Tick缓存
	_ticker_keys.clear();                                             // 清空Ticker键映射表

	_tick_sub_map.clear();                                            // 清空Tick订阅映射表
	_ordque_sub_map.clear();                                          // 清空订单队列订阅映射表
	_orddtl_sub_map.clear();                                          // 清空订单明细订阅映射表
	_trans_sub_map.clear();                                           // 清空逐笔成交订阅映射表

	_price_map.clear();                                                // 清空价格映射表

	_main_key = "";                                                    // 清空主键
	_min_period = "";                                                  // 清空最小周期

	_cur_date = 0;                                                    // 重置当前日期
	_cur_time = 0;                                                    // 重置当前时间
	_cur_secs = 0;                                                     // 重置当前秒数
	_cur_tdate = 0;                                                    // 重置当前交易日期
	_opened_tdate = 0;                                                 // 重置已打开交易日期
	_closed_tdate = 0;                                                 // 重置已关闭交易日期
	_tick_simulated = true;                                            // 重置Tick模拟标志
}

/**
 * @brief 导出回测状态到文件
 * 
 * 将回测进度状态导出为JSON格式文件，用于断点续传
 * 
 * @param stdCode 合约代码
 * @param period K线周期
 * @param times 倍数
 * @param stime 开始时间
 * @param etime 结束时间
 * @param progress 进度（0-1）
 * @param elapse 已用时间（毫秒）
 */
void HisDataReplayer::dump_btstate(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint64_t stime, uint64_t etime, double progress, int64_t elapse)
{
	std::string output;                                                // 输出字符串
	{
		rj::Document root(rj::kObjectType);                          // 创建JSON文档对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取分配器

		root.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码

		std::stringstream ss;                                         // 字符串流
		if (period == KP_DAY)                                          // 如果是日线
			ss << "d";                                                 // 输出"d"
		else if (period == KP_Minute1)                                // 如果是1分钟线
			ss << "m" << times;                                       // 输出"m" + 倍数
		else                                                           // 其他周期
			ss << "m" << times * 5;                                    // 输出"m" + 倍数*5

		root.AddMember("period", rj::Value(ss.str().c_str(), allocator), allocator);  // 添加周期字符串
		
		root.AddMember("stime", stime, allocator);                    // 添加开始时间
		root.AddMember("etime", etime, allocator);                     // 添加结束时间
		root.AddMember("progress", progress, allocator);                // 添加进度
		root.AddMember("elapse", elapse, allocator);                  // 添加已用时间

		rj::StringBuffer sb;                                           // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);                // 创建格式化写入器
		root.Accept(writer);                                            // 将JSON文档写入缓冲区

		output = sb.GetString();                                       // 获取JSON字符串
	}

	std::string folder = WtHelper::getOutputDir();                     // 获取输出目录
	folder += _stra_name;                                              // 添加策略名称
	folder += "/";                                                     // 添加路径分隔符
	boost::filesystem::create_directories(folder.c_str());            // 创建目录（如果不存在）
	std::string filename = folder + "btenv.json";                     // 构造文件名
	StdFile::write_file_content(filename.c_str(), output.c_str(), output.size());  // 写入文件
}

/**
 * @brief 通知回测状态
 * 
 * 通过事件通知器发送回测状态信息（JSON格式）
 * 
 * @param stdCode 合约代码
 * @param period K线周期
 * @param times 倍数
 * @param stime 开始时间
 * @param etime 结束时间
 * @param progress 进度（0-1）
 */
void HisDataReplayer::notify_state(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint64_t stime, uint64_t etime, double progress)
{
	if (!_notifier)                                                    // 如果事件通知器不存在
		return;                                                         // 直接返回

	std::string output;                                                // 输出字符串
	{
		rj::Document root(rj::kObjectType);                          // 创建JSON文档对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取分配器

		root.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码

		std::stringstream ss;                                         // 字符串流
		if (period == KP_DAY)                                          // 如果是日线
			ss << "d";                                                 // 输出"d"
		else if (period == KP_Minute1)                                // 如果是1分钟线
			ss << "m" << times;                                       // 输出"m" + 倍数
		else                                                           // 其他周期
			ss << "m" << times * 5;                                    // 输出"m" + 倍数*5

		root.AddMember("period", rj::Value(ss.str().c_str(), allocator), allocator);  // 添加周期字符串

		root.AddMember("stime", stime, allocator);                    // 添加开始时间
		root.AddMember("etime", etime, allocator);                     // 添加结束时间
		root.AddMember("progress", progress, allocator);                // 添加进度

		rj::StringBuffer sb;                                           // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);                // 创建格式化写入器
		root.Accept(writer);                                            // 将JSON文档写入缓冲区

		output = sb.GetString();                                       // 获取JSON字符串
	}

	_notifier->notifyData("BT_STATE", (void*)output.c_str(), output.size());  // 通知回测状态数据
}

/**
 * @brief 定位K线索引
 * 
 * 根据给定的时间戳，在K线列表中定位对应的K线索引位置
 * 
 * @param key 缓存键（合约代码周期）
 * @param now 当前时间戳（格式：YYYYMMDDHHMM）
 * @param bUpperBound 是否使用上界（默认false）
 * @return K线索引，如果未找到返回UINT32_MAX
 */
uint32_t HisDataReplayer::locate_barindex(const std::string& key, uint64_t now, bool bUpperBound /* = false */)
{
	uint32_t curDate, curTime;                                        // 当前日期和时间
	curDate = (uint32_t)(now / 10000);                                // 提取日期部分（YYYYMMDD）
	curTime = (uint32_t)(now % 10000);                                // 提取时间部分（HHMM）

	BarsListPtr& barsList = _bars_cache[key];                        // 获取K线列表指针
	if (barsList == NULL)                                              // 如果K线列表不存在
		return UINT32_MAX;                                             // 返回UINT32_MAX

	bool isDay = (barsList->_period == KP_DAY);                       // 判断是否是日线

	WTSBarStruct bar;                                                 // 创建临时的K线结构体用于查找
	bar.date = curDate;                                                // 设置日期
	bar.time = (curDate - 19900000) * 10000 + curTime;                // 设置时间（转换为从1990年1月1日开始的秒数）
	auto it = std::lower_bound(barsList->_bars.begin(), barsList->_bars.end(), bar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位位置
		if (isDay)                                                     // 如果是日线
			return a.date < b.date;                                    // 按日期比较
		else                                                           // 如果是分钟线
			return a.time < b.time;                                    // 按时间比较
	});

	std::size_t idx;                                                  // K线索引
	if (it == barsList->_bars.end())                                  // 如果未找到（所有K线都小于目标时间）
		idx = barsList->_bars.size() - 1;                             // 返回最后一个K线的索引
	else                                                               // 如果找到了
	{
		if(bUpperBound)                                                 // 如果使用上界
		{//如果是找上边界，则要比较时间向下修正，因为lower_bound函数找的是大于等于curTime的K线
			if ((isDay && it->date > bar.date) || (!isDay && it->time > bar.time))  // 如果找到的K线时间大于目标时间
			{
				it--;                                                   // 向前移动一位（向下修正）
			}
		}

		idx = it - barsList->_bars.begin();                           // 计算索引位置
	}
	
	return idx;                                                        // 返回索引
}

/**
 * @brief 停止回测
 * 
 * 设置终止标志，回测将在下一轮循环中退出
 */
void HisDataReplayer::stop()
{
	if(!_running)                                                      // 如果回测未运行
	{
		WTSLogger::log_raw(LL_ERROR, "Backtesting is not running, no need to stop");  // 记录错误日志
		return;                                                         // 直接返回
	}

	if (_terminated)                                                   // 如果已经终止
		return;                                                         // 直接返回

	_terminated = true;                                                // 设置终止标志为true
	WTSLogger::log_raw(LL_WARN, "Terminating flag reset to true, backtesting will quit at next round");  // 记录警告日志
}

/**
 * @brief 准备回放
 * 
 * 初始化回放环境，重置状态，设置初始时间
 * 
 * @return 是否准备成功
 */
bool HisDataReplayer::prepare()
{
	if (_running)                                                      // 如果回测正在运行
	{
		WTSLogger::log_raw(LL_ERROR, "Cannot run more than one backtesting task at the same time");  // 记录错误日志
		return false;                                                  // 返回失败
	}

	_running = true;                                                   // 设置运行标志为true
	_terminated = false;                                               // 设置终止标志为false
	reset();                                                           // 重置回放器状态

	_cur_date = (uint32_t)(_begin_time / 10000);                      // 设置当前日期（从开始时间提取）
	_cur_time = (uint32_t)(_begin_time % 10000);                      // 设置当前时间（从开始时间提取）
	_cur_secs = 0;                                                     // 重置当前秒数
	_cur_tdate = _bd_mgr.calcTradingDate(DEFAULT_SESSIONID, _cur_date, _cur_time, true);  // 计算当前交易日期

	if (_notifier)                                                     // 如果事件通知器存在
		_notifier->notifyEvent("BT_START");                            // 通知回测开始事件

	_listener->handle_init();                                          // 调用数据接收器的初始化回调

	if (!_tick_enabled)                                                // 如果未启用Tick回放
		checkUnbars();                                                  // 检查未订阅的K线缓存

	return true;                                                        // 返回成功
}

/**
 * @brief 运行回测
 * 
 * 根据是否有定时任务，选择不同的回放模式：
 * - 如果没有定时任务：采用K线回放模式
 * - 如果有定时任务：采用定时任务回放模式
 * - 如果启用了Tick回放：采用Tick回放模式
 * 
 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
 */
void HisDataReplayer::run(bool bNeedDump/* = false*/)
{
	if(_task == NULL)                                                  // 如果没有定时任务
	{
		//如果没有时间调度任务,则采用主K线回放的模式

		//如果没有确定主K线,则确定一个周期最短的主K线
		if (_main_key.empty() && !_bars_cache.empty())                // 如果主键为空且K线缓存不为空
		{
			WTSKlinePeriod minPeriod = KP_DAY;                         // 初始最小周期为日线
			uint32_t minTimes = 1;                                     // 初始最小倍数为1
			for(auto& m : _bars_cache)                                 // 遍历所有K线缓存
			{
				const BarsListPtr& barsList = m.second;               // 获取K线列表指针
				if (barsList->_period < minPeriod)                    // 如果周期小于当前最小周期
				{
					minPeriod = barsList->_period;                     // 更新最小周期
					minTimes = barsList->_times;                      // 更新最小倍数
					_main_key = m.first;                               // 更新主键
				}
				else if(barsList->_period == minPeriod)                // 如果周期等于当前最小周期
				{
					if(barsList->_times < minTimes)                   // 如果倍数小于当前最小倍数
					{
						_main_key = m.first;                           // 更新主键
						minTimes = barsList->_times;                  // 更新最小倍数
					}
					//By Wesley @ 2022.11.03
					//这里主要修复了只用日线的时候不能正确判断主K线的bug
					else if(_main_key.empty())                         // 如果主键仍为空
					{
						_main_key = m.first;                           // 设置主键
					}
				}
			}

			WTSLogger::info("Main K bars automatic determined: {}", _main_key.c_str());  // 记录日志
		}

		if(!_main_key.empty())                                         // 如果主键不为空
		{
			//如果订阅了K线，则按照主K线进行回放
			run_by_bars(bNeedDump);                                   // 按K线回放
		}
		else if(_tick_enabled)                                         // 如果启用了Tick回放
		{
			run_by_ticks(bNeedDump);                                   // 按Tick回放
		}
		else                                                           // 如果既没有K线也没有Tick
		{
			WTSLogger::log_raw(LL_INFO, "Main K bars not subscribed and backtesting of tick data not available , replaying done");  // 记录信息日志
			_listener->handle_replay_done();                           // 调用回放完成回调
			if (_notifier)                                              // 如果事件通知器存在
				_notifier->notifyEvent("BT_END");                      // 通知回测结束事件
		}
	}
	else //if(_task != NULL)                                            // 如果有定时任务
	{
		run_by_tasks(bNeedDump);                                       // 按定时任务回放
	}

	_running = false;                                                   // 设置运行标志为false
}

/**
 * @brief 按照Tick进行回测
 * 
 * 如果没有订阅K线，且Tick回测是打开的，则按照每日的Tick进行回放
 * 
 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
 */
void HisDataReplayer::run_by_ticks(bool bNeedDump /* = false */)
{
	//如果没有订阅K线，且tick回测是打开的，则按照每日的tick进行回放
	uint32_t edt = (uint32_t)(_end_time / 10000);                      // 提取结束日期（YYYYMMDD）
	uint32_t etime = (uint32_t)(_end_time % 10000);                    // 提取结束时间（HHMM）
	uint64_t end_tdate = _bd_mgr.calcTradingDate(DEFAULT_SESSIONID, edt, etime, true);  // 计算结束交易日期

	while (_cur_tdate <= end_tdate && !_terminated)                    // 当当前交易日期小于等于结束交易日期且未终止时
	{
		if (checkAllTicks(_cur_tdate))                                 // 如果检查到所有Tick数据都已缓存
		{
			WTSLogger::info("Start to replay tick data of {}...", _cur_tdate);  // 记录信息日志
			_listener->handle_session_begin(_cur_tdate);              // 调用交易时段开始回调
			check_cache_days();                                        // 检查缓存天数并清理过期缓存
			replayHftDatasByDay(_cur_tdate);                           // 按天回放HFT数据
			_listener->handle_session_end(_cur_tdate);                 // 调用交易时段结束回调
		}

		_cur_tdate = TimeUtils::getNextDate(_cur_tdate);               // 移动到下一个交易日
	}

	if (_terminated)                                                    // 如果已终止
		WTSLogger::debug("Replaying by ticks terminated forcely");     // 记录调试日志

	WTSLogger::log_raw(LL_INFO, "All back data replayed, replaying done");  // 记录信息日志
	_listener->handle_replay_done();                                   // 调用回放完成回调
	if (_notifier)                                                      // 如果事件通知器存在
		_notifier->notifyEvent("BT_END");                              // 通知回测结束事件
}

/**
 * @brief 按照K线进行回测
 * 
 * 按照主K线周期逐条回放K线数据，并在每条K线收盘时触发回调
 * 
 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
 */
void HisDataReplayer::run_by_bars(bool bNeedDump /* = false */)
{
	TimeUtils::Ticker ticker;                                         // 计时器，用于统计耗时

	BarsListPtr barsList = _bars_cache[_main_key];                   // 获取主K线列表
	WTSSessionInfo* sInfo = get_session_info(barsList->_code.c_str(), true);  // 获取交易时段信息
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(barsList->_code.c_str(), NULL);  // 提取合约代码信息
	std::string commId = codeInfo.stdCommID();                        // 获取标准化合约ID

	uint32_t sIdx = locate_barindex(_main_key, _begin_time, false);  // 定位开始K线索引
	uint32_t eIdx = locate_barindex(_main_key, _end_time, true);     // 定位结束K线索引

	uint32_t total_barcnt = eIdx - sIdx + 1;                          // 计算总K线数量
	uint32_t replayed_barcnt = 0;                                     // 已回放K线数量

	notify_state(barsList->_code.c_str(), barsList->_period, barsList->_times, _begin_time, _end_time, 0);  // 通知初始状态

	if (bNeedDump)                                                    // 如果需要导出状态
		dump_btstate(barsList->_code.c_str(), barsList->_period, barsList->_times, _begin_time, _end_time, 100.0, ticker.nano_seconds());  // 导出回测状态

	WTSLogger::info("Start to replay back data from {}...", _begin_time);  // 记录日志

	for (; !_terminated;)                                             // 当未终止时循环
	{
		bool isDay = barsList->_period == KP_DAY;                    // 判断是否是日线
		if (barsList->_cursor != UINT_MAX)                            // 如果游标已初始化
		{
			uint64_t nextBarTime = 0;                                 // 下一条K线时间
			if (isDay)                                                 // 如果是日线
				nextBarTime = (uint64_t)barsList->_bars[barsList->_cursor].date * 10000 + sInfo->getCloseTime();  // 计算下一条日线时间（日期+收盘时间）
			else                                                       // 如果是分钟线
			{
				nextBarTime = (uint64_t)barsList->_bars[barsList->_cursor].time;  // 获取K线时间戳
				nextBarTime += 199000000000;                          // 加上基准时间（1990-01-01 00:00:00）
			}

			if (nextBarTime > _end_time)                              // 如果下一条K线时间超过结束时间
			{
				WTSLogger::info("{} is beyond ending time {},replaying done", nextBarTime, _end_time);  // 记录日志
				break;                                                 // 退出循环
			}

			uint32_t nextDate = (uint32_t)(nextBarTime / 10000);      // 提取下一条K线的日期
			uint32_t nextTime = (uint32_t)(nextBarTime % 10000);      // 提取下一条K线的时间

			//By Wesley @ 2022.01.10
			//如果和收盘时间一样，进行这个判断
			//主要针对7*24小时的品种，其他的品种不需要
			uint32_t nextTDate = _opened_tdate;                       // 下一个交易日期（默认使用已打开的交易日期）
			if(isDay || (!isDay && sInfo->offsetTime(nextTime, false) != sInfo->getCloseTime(true)))  // 如果是日线，或者分钟线时间不等于收盘时间
			{
				nextTDate = _bd_mgr.calcTradingDate(commId.c_str(), nextDate, nextTime, false);  // 计算下一个交易日期
				if (_opened_tdate != nextTDate)                      // 如果交易日期发生变化
				{
					if(_closed_tdate != _opened_tdate)                // 如果已关闭的交易日期不等于已打开的交易日期
					{
						WTSLogger::debug("Tradingday {} ends", _cur_tdate);  // 记录调试日志
						_listener->handle_session_end(_cur_tdate);    // 调用交易时段结束回调
						_closed_tdate = _cur_tdate;                   // 更新已关闭的交易日期
						_day_cache.clear();                           // 清空每日Tick缓存
					}

					/*
					 *	By Wesley @ 2022.06.23
					 *	因为可能会有人在on_session_begin下单，所以这里把时间戳改成开盘时间
					 *	这样signals里看起来比较容易理解一些
					 */
					uint64_t beginTimeofDay = _bd_mgr.getBoundaryTime(sInfo->id(), nextTDate, true, true);  // 获取交易日开盘时间

					_cur_date = (uint32_t)(beginTimeofDay / 10000);   // 设置当前日期
					_cur_time = beginTimeofDay % 10000;                // 设置当前时间
					_cur_secs = 0;                                     // 重置当前秒数

					WTSLogger::debug("Tradingday {} begins", nextTDate);  // 记录调试日志
					_listener->handle_session_begin(nextTDate);       // 调用交易时段开始回调
					check_cache_days();                                // 检查缓存天数并清理过期缓存
					_opened_tdate = nextTDate;                         // 更新已打开的交易日期
					_cur_tdate = nextTDate;                           // 更新当前交易日期
				}
			}			

			uint64_t curBarTime = (uint64_t)_cur_date * 10000 + _cur_time;  // 计算当前K线时间
			if (_tick_enabled)                                         // 如果启用了Tick回放
			{
				//如果开启了tick回放,则直接回放tick数据
				//如果tick回放失败，说明tick数据不存在，则需要模拟tick
				_tick_simulated = !replayHftDatas(curBarTime, nextBarTime);  // 回放HFT数据，如果失败则标记为模拟Tick
			}

			if (!_tick_enabled)                                        // 如果未启用Tick回放
			{
				checkUnbars();                                         // 检查未订阅的K线缓存
			}

			_cur_date = nextDate;                                     // 更新当前日期
			_cur_time = nextTime;                                     // 更新当前时间
			_cur_secs = 0;                                             // 重置当前秒数

			bool isEndTDate = (sInfo->offsetTime(_cur_time, false) >= sInfo->getCloseTime(true));  // 判断是否是交易日结束时间

			/*
			 *	By Wesley @ 2022.06.23
			 *	tick数据模拟的机制完善
			 *	主要将所有当前应该闭合的bar，按照开高低收的顺序同步模拟tick
			 *	但是这样也是有漏洞的，那就是如果K线周期不统一，如m1和m5同时订阅
			 *	会出现m5在最后一分钟才模拟tick的问题
			 *	不过，真的要精确回测，请使用逐tick回测
			 *	目前这个方案已经算是比较好的了
			 */
			for(int i = 0; i < 4; i++)                                 // 循环4次（模拟开高低收4个价格）
			{
				if (_tick_simulated)                                  // 如果需要模拟Tick
					simTicks(nextDate, nextTime, (isDay || isEndTDate) ? nextTDate : 0, i);  // 模拟Tick数据

				if (!_tick_enabled)                                   // 如果未启用Tick回放
					simTickWithUnsubBars(curBarTime, nextBarTime, (isDay || isEndTDate) ? nextTDate : 0, i);  // 使用未订阅K线模拟Tick
			}

			onMinuteEnd(nextDate, nextTime, (isDay || isEndTDate) ? nextTDate : 0, _tick_simulated);  // 处理分钟线结束

			replayed_barcnt += 1;                                     // 增加已回放K线计数

			if(sInfo->isLastOfSection(nextTime))                      // 如果是小节结束
				_listener->handle_section_end(nextDate, nextTime);    // 调用小节结束回调

			if (isEndTDate && _closed_tdate != _cur_tdate)            // 如果是交易日结束且还未关闭
			{
				WTSLogger::debug("Tradingday {} ends", _cur_tdate);   // 记录调试日志
				_listener->handle_session_end(_cur_tdate);           // 调用交易时段结束回调
				_closed_tdate = _cur_tdate;                           // 更新已关闭的交易日期
				_day_cache.clear();                                   // 清空每日Tick缓存
			}

			notify_state(barsList->_code.c_str(), barsList->_period, barsList->_times, _begin_time, _end_time, replayed_barcnt*100.0 / total_barcnt);  // 通知回测状态

			if (barsList->_cursor >= barsList->_bars.size())          // 如果游标超出K线列表大小
			{
				WTSLogger::log_raw(LL_INFO, "All back data replayed, replaying done");  // 记录信息日志
				break;                                                 // 退出循环
			}
		}
		else                                                           // 如果游标未初始化
		{
			WTSLogger::log_raw(LL_ERROR, "No back data initialized, replaying canceled");  // 记录错误日志
			break;                                                     // 退出循环
		}
	}

	if (_terminated)                                                   // 如果已终止
		WTSLogger::debug("Replaying by bars terminated forcely");     // 记录调试日志

	notify_state(barsList->_code.c_str(), barsList->_period, barsList->_times, _begin_time, _end_time, 100);  // 通知最终状态（100%）
	if (_notifier)                                                     // 如果事件通知器存在
		_notifier->notifyEvent("BT_END");                              // 通知回测结束事件

	if (_closed_tdate != _cur_tdate)                                  // 如果最后一个交易日还未关闭
	{
		WTSLogger::debug("Tradingday {} ends", _cur_tdate);           // 记录调试日志
		_listener->handle_session_end(_cur_tdate);                   // 调用交易时段结束回调
	}

	if (bNeedDump)                                                     // 如果需要导出状态
	{
		dump_btstate(barsList->_code.c_str(), barsList->_period, barsList->_times, _begin_time, _end_time, 100.0, ticker.nano_seconds());  // 导出回测状态
	}

	_listener->handle_replay_done();                                  // 调用回放完成回调
}

/**
 * @brief 按照定时任务进行回测
 * 
 * 时间调度任务不为空，则按照时间调度任务回放
 * 支持分钟、每日、每周、每月、每年等不同周期的定时任务
 * 
 * @param bNeedDump 是否将回测进度落地到文件中（默认false）
 */
void HisDataReplayer::run_by_tasks(bool bNeedDump /* = false */)
{
	//时间调度任务不为空,则按照时间调度任务回放
	WTSSessionInfo* sInfo = NULL;                                    // 交易时段信息指针
	const char* DEF_SESS = (strlen(_task->_session) == 0) ? DEFAULT_SESSIONID : _task->_session;  // 获取交易时段ID（如果为空则使用默认值）
	sInfo = _bd_mgr.getSession(DEF_SESS);                            // 获取交易时段信息
	WTSLogger::info("Start to backtest with task frequency from {}...", _begin_time);  // 记录日志

	//分钟即任务和日级别任务分开写
	if (_task->_period != TPT_Minute)                                 // 如果不是分钟级任务
	{
		uint32_t endtime = TimeUtils::getNextMinute(_task->_time, -1);  // 计算结束时间（前一天最后一分钟）
		bool bIsPreDay = endtime > _task->_time;                      // 判断是否是前一天
		if (bIsPreDay)                                                 // 如果是前一天
			_cur_date = TimeUtils::getNextDate(_cur_date, -1);        // 调整当前日期为前一天

		for (; !_terminated;)                                          // 当未终止时循环
		{
			bool fired = false;                                        // 任务触发标志
			//获取上一个交易日的日期
			uint32_t preTDate = TimeUtils::getNextDate(_cur_tdate, -1);  // 获取上一个交易日的日期
			if (_cur_time == endtime)                                  // 如果当前时间等于结束时间
			{
				if (!_bd_mgr.isHoliday(_task->_trdtpl, _cur_date, true))  // 如果不是节假日
				{
					uint32_t weekDay = TimeUtils::getWeekDay(_cur_date);  // 获取当前日期是星期几


					bool bHasHoliday = false;                         // 是否有节假日标志
					uint32_t days = 1;                                 // 天数计数器
					while (_bd_mgr.isHoliday(_task->_trdtpl, preTDate, true))  // 如果上一个交易日是节假日
					{
						bHasHoliday = true;                            // 标记有节假日
						preTDate = TimeUtils::getNextDate(preTDate, -1);  // 继续向前查找
						days++;                                        // 增加天数
					}
					uint32_t preWD = TimeUtils::getWeekDay(preTDate);  // 获取上一个交易日的星期

					switch (_task->_period)                            // 根据任务周期判断
					{
					case TPT_Daily:                                    // 如果是每日任务
						fired = true;                                  // 直接触发
						break;
					case TPT_Minute:                                   // 如果是分钟任务（不会到这里）
						break;
					case TPT_Monthly:                                  // 如果是每月任务
						//if (preTDate % 1000000 < _task->_day && _cur_date % 1000000 >= _task->_day)
						//	fired = true;
						if (_cur_date % 1000000 == _task->_day)        // 如果当前日期等于触发日期（MMDD）
							fired = true;                              // 触发任务
						else if (bHasHoliday)                          // 如果有节假日
						{
							//上一个交易日在上个月,且当前日期大于触发日期
							//说明这个月的开始日期在节假日内,顺延到今天
							if ((preTDate % 10000 / 100 < _cur_date % 10000 / 100) && _cur_date % 1000000 > _task->_day)  // 如果跨月且当前日期大于触发日期
							{
								fired = true;                          // 触发任务
							}
							else if (preTDate % 1000000 < _task->_day && _cur_date % 1000000 > _task->_day)  // 如果上一个交易日小于触发日期且当前日期大于触发日期
							{
								//上一个交易日在同一个月,且小于触发日期,但是今天大于触发日期,说明正确触发日期到节假日内,顺延到今天
								fired = true;                          // 触发任务
							}
						}
						break;
					case TPT_Weekly:                                   // 如果是每周任务
						//if (preWD < _task->_day && weekDay >= _task->_day)
						//	fired = true;
						if (weekDay == _task->_day)                     // 如果当前星期等于触发星期
							fired = true;                              // 触发任务
						else if (bHasHoliday)                          // 如果有节假日
						{
							if (days >= 7 && weekDay > _task->_day)    // 如果间隔天数大于等于7天且当前星期大于触发星期
							{
								fired = true;                          // 触发任务
							}
							else if (preWD > weekDay && weekDay > _task->_day)  // 如果上一个交易日星期大于当前星期且当前星期大于触发星期（跨周）
							{
								//上一个交易日的星期大于今天的星期,说明换了一周了
								fired = true;                          // 触发任务
							}
							else if (preWD < _task->_day && weekDay > _task->_day)  // 如果上一个交易日星期小于触发星期且当前星期大于触发星期
							{
								fired = true;                          // 触发任务
							}
						}
						break;
					case TPT_Yearly:                                    // 如果是每年任务
						if (preTDate % 10000 < _task->_day && _cur_date % 10000 >= _task->_day)  // 如果跨年且当前日期大于等于触发日期（MMDD）
							fired = true;                              // 触发任务
						break;
					}
				}
			}

			if (!fired)                                                 // 如果任务未触发
			{
				//调整时间
				//如果当前时间小于任务时间,则直接赋值即可
				//如果当前时间大于任务时间,则至少要等下一天
				if (_cur_time < endtime)                                // 如果当前时间小于结束时间
				{
					_cur_time = endtime;                                // 直接设置为结束时间
					continue;                                           // 继续下一次循环
				}

				uint32_t newTDate = _bd_mgr.calcTradingDate(DEF_SESS, _cur_date, _cur_time, true);  // 计算新的交易日期

				if (newTDate != _cur_tdate)                             // 如果交易日期发生变化
				{
					_cur_tdate = newTDate;                              // 更新当前交易日期
					if (_listener)                                      // 如果数据接收器存在
						_listener->handle_session_begin(newTDate);     // 调用交易时段开始回调
					check_cache_days();                                 // 检查缓存天数并清理过期缓存
					if (_listener)                                      // 如果数据接收器存在
						_listener->handle_session_end(newTDate);       // 调用交易时段结束回调
				}
			}
			else                                                         // 如果任务已触发
			{
				//用前一分钟作为结束时间
				uint32_t curDate = _cur_date;                           // 当前日期
				uint32_t curTime = endtime;                             // 当前时间（使用结束时间）
				bool bEndSession = sInfo->offsetTime(curTime, true) >= sInfo->getCloseTime(true);  // 判断是否是交易日结束
				if (_listener)                                          // 如果数据接收器存在
					_listener->handle_session_begin(_cur_tdate);       // 调用交易时段开始回调
				check_cache_days();                                     // 检查缓存天数并清理过期缓存
				onMinuteEnd(curDate, curTime, bEndSession ? _cur_tdate : preTDate);  // 处理分钟线结束
				if (_listener)                                          // 如果数据接收器存在
					_listener->handle_session_end(_cur_tdate);         // 调用交易时段结束回调
			}

			_cur_date = TimeUtils::getNextDate(_cur_date);             // 移动到下一天
			_cur_time = endtime;                                       // 设置时间为结束时间
			_cur_tdate = _bd_mgr.calcTradingDate(DEF_SESS, _cur_date, _cur_time, true);  // 计算新的交易日期

			uint64_t nextTime = (uint64_t)_cur_date * 10000 + _cur_time;  // 计算下一个时间戳
			if (nextTime > _end_time)                                   // 如果下一个时间超过结束时间
			{
				WTSLogger::log_raw(LL_INFO, "Backtesting with task frequency is done");  // 记录信息日志
				if (_listener)                                          // 如果数据接收器存在
				{
					_listener->handle_session_end(_cur_tdate);         // 调用交易时段结束回调
					_listener->handle_replay_done();                   // 调用回放完成回调
					if (_notifier)                                      // 如果事件通知器存在
						_notifier->notifyEvent("BT_END");              // 通知回测结束事件
				}

				break;                                                   // 退出循环
			}
		}
	}
	else                                                               // 如果是分钟级任务
	{
		if (_listener)                                                  // 如果数据接收器存在
			_listener->handle_session_begin(_cur_tdate);               // 调用交易时段开始回调

		check_cache_days();                                             // 检查缓存天数并清理过期缓存

		for (; !_terminated;)                                           // 当未终止时循环
		{
			//要考虑到跨日的情况
			uint32_t mins = sInfo->timeToMinutes(_cur_time);          // 将时间转换为分钟数
			//如果一开始不能整除,则直接修正一下
			if (mins % _task->_time != 0 && mins < sInfo->getTradingMins())  // 如果分钟数不能整除任务时间且小于交易日总分钟数
			{
				mins = mins / _task->_time + _task->_time;             // 修正分钟数（向上取整）
				_cur_time = sInfo->minuteToTime(mins);                 // 将分钟数转换回时间
			}

			bool bNewTDate = false;                                     // 是否是新交易日标志
			if (mins < sInfo->getTradingMins())                        // 如果分钟数小于交易日总分钟数
			{
				/*
				 *	By Wesley @ 2022.06.23
				 *	tick数据模拟的机制完善
				 *	主要将所有当前应该闭合的bar，按照开高低收的顺序同步模拟tick
				 *	但是这样也是有漏洞的，那就是如果K线周期不统一，如m1和m5同时订阅
				 *	会出现m5在最后一分钟才模拟tick的问题
				 *	不过，真的要精确回测，请使用逐tick回测
				 *	目前这个方案已经算是比较好的了
				 */
				for (int i = 0; i < 4; i++)                             // 循环4次（模拟开高低收4个价格）
				{
					simTicks(_cur_date, _cur_time, _cur_tdate, i);     // 模拟Tick数据
				}

				onMinuteEnd(_cur_date, _cur_time, 0);                  // 处理分钟线结束
			}
			else                                                         // 如果分钟数大于等于交易日总分钟数
			{
				bNewTDate = true;                                       // 标记为新交易日
				mins = sInfo->getTradingMins();                        // 设置为交易日总分钟数
				_cur_time = sInfo->getCloseTime();                     // 设置为收盘时间

				/*
				 *	By Wesley @ 2022.06.23
				 *	tick数据模拟的机制完善
				 *	主要将所有当前应该闭合的bar，按照开高低收的顺序同步模拟tick
				 *	但是这样也是有漏洞的，那就是如果K线周期不统一，如m1和m5同时订阅
				 *	会出现m5在最后一分钟才模拟tick的问题
				 *	不过，真的要精确回测，请使用逐tick回测
				 *	目前这个方案已经算是比较好的了
				 */
				for (int i = 0; i < 4; i++)                             // 循环4次（模拟开高低收4个价格）
				{
					simTicks(_cur_date, _cur_time, _cur_tdate, i);     // 模拟Tick数据
				}
				onMinuteEnd(_cur_date, _cur_time, _cur_tdate);         // 处理分钟线结束

				if (_listener)                                          // 如果数据接收器存在
					_listener->handle_session_end(_cur_tdate);         // 调用交易时段结束回调
			}


			if (bNewTDate)                                               // 如果是新交易日
			{
				//换日了
				mins = _task->_time;                                     // 重置分钟数为任务时间
				uint32_t nextTDate = _bd_mgr.getNextTDate(_task->_trdtpl, _cur_tdate, 1, true);  // 获取下一个交易日

				if (sInfo->getOffsetMins() != 0)                        // 如果交易时段有偏移分钟数
				{
					if (sInfo->getOffsetMins() > 0)                     // 如果偏移分钟数大于0
					{
						//真实时间后移,说明夜盘算作下一天的
						_cur_date = _cur_tdate;                         // 设置当前日期为当前交易日期
						_cur_tdate = nextTDate;                          // 更新当前交易日期为下一个交易日
					}
					else                                                 // 如果偏移分钟数小于0
					{
						//真实时间前移,说明夜盘是上一天的,这种情况就不需要动了
						_cur_tdate = nextTDate;                          // 更新当前交易日期为下一个交易日
						_cur_date = _cur_tdate;                         // 设置当前日期为当前交易日期
					}
				}

				_cur_time = sInfo->minuteToTime(mins);                 // 将分钟数转换回时间

				if (_listener)                                          // 如果数据接收器存在
					_listener->handle_session_begin(nextTDate);        // 调用交易时段开始回调

				check_cache_days();                                     // 检查缓存天数并清理过期缓存
			}
			else                                                         // 如果不是新交易日
			{
				mins += _task->_time;                                   // 增加任务时间分钟数
				if (mins > sInfo->getTradingMins())                    // 如果分钟数超过交易日总分钟数
					mins = sInfo->getTradingMins();                     // 设置为交易日总分钟数

				uint32_t newTime = sInfo->minuteToTime(mins);         // 将分钟数转换回时间
				bool bNewDay = newTime < _cur_time;                    // 判断是否跨日（时间小于当前时间说明跨日）
				if (bNewDay)                                            // 如果跨日
					_cur_date = TimeUtils::getNextDate(_cur_date);     // 移动到下一天

				uint32_t dayMins = _cur_time / 100 * 60 + _cur_time % 100;  // 计算当前时间的分钟数（从0点开始）
				uint32_t nextDMins = newTime / 100 * 60 + newTime % 100;    // 计算下一个时间的分钟数（从0点开始）

				//是否到了一个新的小节
				bool bNewSec = (nextDMins - dayMins > _task->_time) && !bNewDay;  // 判断是否到了新小节（时间间隔大于任务时间且未跨日）

				while (bNewSec && _bd_mgr.isHoliday(_task->_trdtpl, _cur_date, true))  // 如果到了新小节且是节假日
					_cur_date = TimeUtils::getNextDate(_cur_date);     // 移动到下一天

				_cur_time = newTime;                                    // 更新当前时间
			}

			uint64_t nextTime = (uint64_t)_cur_date * 10000 + _cur_time;  // 计算下一个时间戳
			if (nextTime > _end_time)                                   // 如果下一个时间超过结束时间
			{
				WTSLogger::log_raw(LL_INFO, "Backtesting with task frequency is done");  // 记录信息日志
				if (_listener)                                          // 如果数据接收器存在
				{
					_listener->handle_session_end(_cur_tdate);         // 调用交易时段结束回调
					_listener->handle_replay_done();                   // 调用回放完成回调
					if (_notifier)                                      // 如果事件通知器存在
						_notifier->notifyEvent("BT_END");              // 通知回测结束事件
				}
				break;                                                   // 退出循环
			}
		}
	}
}

/**
 * @brief 模拟Tick数据
 * 
 * 从K线数据模拟生成Tick数据，按照开高低收的顺序生成4个Tick
 * 
 * @param uDate 日期
 * @param uTime 时间
 * @param endTDate 结束交易日期（默认0，表示不限制）
 * @param pxType 价格类型（0-开盘价，1-最高价，2-最低价，3-收盘价，默认0）
 */
void HisDataReplayer::simTicks(uint32_t uDate, uint32_t uTime, uint32_t endTDate /* = 0 */, int pxType /* = 0 */)
{
	//这里应该触发检查
	uint64_t nowTime = (uint64_t)uDate * 10000 + uTime;                // 计算当前时间戳

	for (auto it = _bars_cache.begin(); it != _bars_cache.end(); it++)  // 遍历所有K线缓存
	{
		BarsListPtr& barsList = (BarsListPtr&)it->second;             // 获取K线列表指针
		if (barsList->_period != KP_DAY)                               // 如果不是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for(;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					/*
					 *	By Wesley @ 2023.05.05
					 *	如果没有禁止0成交模拟tick，或者K线成交量不为0，就可以模拟tick
					 */
					bool bCanSim = !_nosim_if_notrade || !decimal::eq(nextBar.vol, 0.0);  // 判断是否可以模拟Tick

					uint64_t barTime = 199000000000 + nextBar.time;    // 计算K线时间戳（加上基准时间）
					if (barTime == nowTime && bCanSim)                 // 如果K线时间等于当前时间且可以模拟
					{
						const std::string& ticker = _ticker_keys[barsList->_code];  // 获取Ticker键
						if (ticker == it->first)                       // 如果Ticker键匹配
						{
							//开高低收
							WTSTickStruct& curTS = _day_cache[barsList->_code];  // 获取每日Tick缓存
							strcpy(curTS.code, barsList->_code.c_str());         // 设置合约代码
							curTS.action_date = _cur_date;                      // 设置动作日期
							curTS.action_time = _cur_time * 100000;              // 设置动作时间（转换为微秒）

							double newPx = 0.0;                         // 新价格
							if (pxType == 0)                            // 如果是开盘价
								newPx = nextBar.open;                   // 使用开盘价
							else if (pxType == 1)                      // 如果是最高价
								newPx = nextBar.high;                   // 使用最高价
							else if (pxType == 2)                       // 如果是最低价
								newPx = nextBar.low;                    // 使用最低价
							else if (pxType == 3)                       // 如果是收盘价
								newPx = nextBar.close;                  // 使用收盘价

							curTS.price = newPx;                        // 设置价格
							curTS.volume = nextBar.vol;                 // 设置成交量
							curTS.total_volume += nextBar.vol;          // 累计总成交量

							//更新开高低三个字段
							if (decimal::eq(curTS.open, 0))             // 如果开盘价为0
								curTS.open = curTS.price;               // 设置开盘价
							curTS.high = max(curTS.price, curTS.high);  // 更新最高价（取较大值）
							if (decimal::eq(curTS.low, 0))              // 如果最低价为0
								curTS.low = curTS.price;                // 设置最低价
							else                                         // 如果最低价不为0
								curTS.low = min(curTS.price, curTS.low);  // 更新最低价（取较小值）

							update_price(barsList->_code.c_str(), curTS.price);  // 更新价格映射
							WTSTickData* curTick = WTSTickData::create(curTS);  // 创建Tick数据对象
							_listener->handle_tick(barsList->_code.c_str(), curTick, pxType);  // 调用Tick数据回调
							curTick->release();                         // 释放Tick数据对象
						}

						break;                                           // 退出循环
					}
					else if (barTime < nowTime)                         // 如果K线时间小于当前时间
					{
						barsList->_cursor++;                            // 游标向前移动

						if (barsList->_cursor == barsList->_bars.size())  // 如果游标超出K线列表大小
							break;                                       // 退出循环

						continue;                                        // 继续下一次循环
					}
					else                                                 // 如果K线时间大于当前时间
					{
						break;                                           // 退出循环
					}
				} 
			}
		}
		else                                                             // 如果是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for (;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					if (nextBar.date == endTDate)                       // 如果K线日期等于结束交易日期
					{
						const std::string& ticker = _ticker_keys[barsList->_code];  // 获取Ticker键
						if (ticker == it->first)                       // 如果Ticker键匹配
						{
							CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(barsList->_code.c_str(), &_hot_mgr);  // 提取合约代码信息
							WTSCommodityInfo* commInfo = _bd_mgr.getCommodity(cInfo._exchg, cInfo._product);  // 获取合约信息

							std::string realCode = barsList->_code;     // 真实合约代码
							if (cInfo.isExright())                      // 如果是复权代码
								realCode = realCode.substr(0, realCode.size() - 1);  // 去掉复权标记

							WTSSessionInfo* sInfo = get_session_info(realCode.c_str(), true);  // 获取交易时段信息
							uint32_t curTime = sInfo->getCloseTime();   // 获取收盘时间
							//开高低收
							WTSTickStruct curTS;                        // 创建Tick结构体
							strcpy(curTS.code, realCode.c_str());        // 设置合约代码
							curTS.action_date = _cur_date;              // 设置动作日期
							curTS.action_time = curTime * 100000;        // 设置动作时间（转换为微秒）

							double newPx = 0.0;                         // 新价格
							if (pxType == 0)                            // 如果是开盘价
								newPx = nextBar.open;                   // 使用开盘价
							else if (pxType == 1)                      // 如果是最高价
								newPx = nextBar.high;                   // 使用最高价
							else if (pxType == 2)                       // 如果是最低价
								newPx = nextBar.low;                    // 使用最低价
							else if (pxType == 3)                       // 如果是收盘价
								newPx = nextBar.close;                  // 使用收盘价

							curTS.price = newPx;                        // 设置价格
							curTS.volume = nextBar.vol;                 // 设置成交量
							update_price(barsList->_code.c_str(), curTS.price);  // 更新价格映射
							WTSTickData* curTick = WTSTickData::create(curTS);  // 创建Tick数据对象
							_listener->handle_tick(realCode.c_str(), curTick, pxType);  // 调用Tick数据回调
							curTick->release();                         // 释放Tick数据对象
						}

						break;                                           // 退出循环
					}
					else if (nextBar.date == endTDate)                  // 如果K线日期等于结束交易日期（重复判断？）
					{
						barsList->_cursor++;                            // 游标向前移动

						if (barsList->_cursor == barsList->_bars.size())  // 如果游标超出K线列表大小
							break;                                       // 退出循环
					}
					else                                                 // 如果K线日期不等于结束交易日期
					{
						break;                                           // 退出循环
					}
				}
			}
		}
	}
}

/**
 * @brief 使用未订阅的K线模拟Tick数据
 * 
 * 从未订阅的K线缓存中模拟生成Tick数据，用于未订阅但需要的合约
 * 
 * @param stime 开始时间
 * @param nowTime 当前时间
 * @param endTDate 结束交易日期（默认0，表示不限制）
 * @param pxType 价格类型（0-开盘价，1-最高价，2-最低价，3-收盘价，默认0）
 */
void HisDataReplayer::simTickWithUnsubBars(uint64_t stime, uint64_t nowTime, uint32_t endTDate /* = 0 */, int pxType /* = 0 */)
{
	//uint64_t nowTime = (uint64_t)uDate * 10000 + uTime;
	uint32_t uDate = (uint32_t)(stime / 10000);                        // 提取日期部分

	for (auto& item : _unbars_cache)                                   // 遍历未订阅K线缓存
	{
		BarsListPtr& barsList = (BarsListPtr&)item.second;            // 获取K线列表指针
		if (barsList->_period != KP_DAY)                               // 如果不是日线
		{
			//如果历史数据指标不在尾部, 说明是回测模式, 要继续回放历史数据
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for (;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					/*
					 *	By Wesley @ 2023.05.05
					 *	如果没有禁止0成交模拟tick，或者K线成交量不为0，就可以模拟tick
					 */
					bool bCanSim = !_nosim_if_notrade || !decimal::eq(nextBar.vol, 0.0);  // 判断是否可以模拟Tick

					uint64_t barTime = 199000000000 + nextBar.time;    // 计算K线时间戳（加上基准时间）
					if (barTime == nowTime && bCanSim)                 // 如果K线时间等于当前时间且可以模拟
					{
						//开高低收
						WTSTickStruct& curTS = _day_cache[barsList->_code];  // 获取每日Tick缓存
						strcpy(curTS.code, barsList->_code.c_str());         // 设置合约代码
						curTS.action_date = _cur_date;                      // 设置动作日期
						curTS.action_time = _cur_time * 100000;              // 设置动作时间（转换为微秒）

						curTS.volume = nextBar.vol;                        // 设置成交量
						double newPx = 0.0;                                // 新价格
						if (pxType == 0)                                   // 如果是开盘价
							newPx = nextBar.open;                          // 使用开盘价
						else if (pxType == 1)                              // 如果是最高价
							newPx = nextBar.high;                          // 使用最高价
						else if (pxType == 2)                              // 如果是最低价
							newPx = nextBar.low;                           // 使用最低价
						else if (pxType == 3)                              // 如果是收盘价
							newPx = nextBar.close;                         // 使用收盘价

						curTS.price = newPx;                               // 设置价格
						//更新开高低三个字段
						if (decimal::eq(curTS.open, 0))                    // 如果开盘价为0
							curTS.open = curTS.price;                    // 设置开盘价
						curTS.high = max(curTS.price, curTS.high);        // 更新最高价（取较大值）
						if (decimal::eq(curTS.low, 0))                     // 如果最低价为0
							curTS.low = curTS.price;                      // 设置最低价
						else                                                // 如果最低价不为0
							curTS.low = min(curTS.price, curTS.low);     // 更新最低价（取较小值）


						WTSTickData* curTick = WTSTickData::create(curTS);  // 创建Tick数据对象
						_listener->handle_tick(barsList->_code.c_str(), curTick, pxType);  // 调用Tick数据回调
						curTick->release();                                // 释放Tick数据对象
						break;                                              // 退出循环
					}
					else if (barTime < nowTime)                            // 如果K线时间小于当前时间
					{
						barsList->_cursor++;                               // 游标向前移动

						if (barsList->_cursor == barsList->_bars.size())    // 如果游标超出K线列表大小
							break;                                          // 退出循环

						continue;                                           // 继续下一次循环
					}
					else                                                    // 如果K线时间大于当前时间
					{
						break;                                              // 退出循环
					}
				}
			}
		}
		else                                                               // 如果是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)              // 如果K线列表还有数据
			{
				for (;;)                                                  // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					if (nextBar.date == endTDate)                         // 如果K线日期等于结束交易日期
					{
						CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(barsList->_code.c_str(), &_hot_mgr);  // 提取合约代码信息
						WTSCommodityInfo* commInfo = _bd_mgr.getCommodity(cInfo._exchg, cInfo._product);  // 获取合约信息

						std::string realCode = barsList->_code;          // 真实合约代码
						if (commInfo->isStock() && cInfo.isExright())     // 如果是股票且是复权代码
							realCode = realCode.substr(0, realCode.size() - 1);  // 去掉复权标记

						WTSSessionInfo* sInfo = get_session_info(realCode.c_str(), true);  // 获取交易时段信息
						uint32_t curTime = sInfo->getOpenTime();         // 获取开盘时间
						//开高低收
						WTSTickStruct curTS;                              // 创建Tick结构体
						strcpy(curTS.code, realCode.c_str());             // 设置合约代码
						curTS.action_date = _cur_date;                    // 设置动作日期
						curTS.action_time = curTime * 100000;             // 设置动作时间（转换为微秒）

						curTS.volume = nextBar.vol;                       // 设置成交量
						double newPx = 0.0;                                // 新价格
						if (pxType == 0)                                  // 如果是开盘价
							newPx = nextBar.open;                          // 使用开盘价
						else if (pxType == 1)                              // 如果是最高价
							newPx = nextBar.high;                          // 使用最高价
						else if (pxType == 2)                              // 如果是最低价
							newPx = nextBar.low;                           // 使用最低价
						else if (pxType == 3)                              // 如果是收盘价
							newPx = nextBar.close;                         // 使用收盘价

						curTS.price = newPx;                              // 设置价格
						//更新开高低三个字段
						if (decimal::eq(curTS.open, 0))                    // 如果开盘价为0
							curTS.open = curTS.price;                    // 设置开盘价
						curTS.high = max(curTS.price, curTS.high);        // 更新最高价（取较大值）
						if (decimal::eq(curTS.low, 0))                     // 如果最低价为0
							curTS.low = curTS.price;                      // 设置最低价
						else                                                // 如果最低价不为0
							curTS.low = min(curTS.price, curTS.low);     // 更新最低价（取较小值）

						WTSTickData* curTick = WTSTickData::create(curTS);  // 创建Tick数据对象
						_listener->handle_tick(realCode.c_str(), curTick, pxType);  // 调用Tick数据回调
						curTick->release();                                // 释放Tick数据对象

						break;                                             // 退出循环
					}
					else if (nextBar.date < endTDate)                      // 如果K线日期小于结束交易日期
					{
						barsList->_cursor++;                               // 游标向前移动

						if (barsList->_cursor == barsList->_bars.size())    // 如果游标超出K线列表大小
							break;                                          // 退出循环

						continue;                                           // 继续下一次循环
					}
					else                                                    // 如果K线日期大于结束交易日期
					{
						break;                                              // 退出循环
					}

				}
			}
		}
	}
}

/**
 * @brief 获取下一个Tick时间
 * 
 * 在所有已订阅的合约中，找到下一个Tick数据的时间戳
 * 
 * @param curTDate 当前交易日期
 * @param stime 开始时间（默认UINT64_MAX，表示不限制）
 * @return 下一个Tick时间戳，如果没有则返回UINT64_MAX
 */
uint64_t HisDataReplayer::getNextTickTime(uint32_t curTDate, uint64_t stime /* = UINT64_MAX */)
{
	uint64_t nextTime = UINT64_MAX;                                     // 下一个时间戳（初始化为最大值）
	for (auto& v : _tick_sub_map)                                       // 遍历所有Tick订阅映射
	{
		const char* stdCode = v.first.c_str();                          // 获取合约代码
		if (!checkTicks(stdCode, curTDate))                             // 如果Tick数据未缓存或检查失败
			continue;                                                    // 跳过该合约

		WTSSessionInfo* sInfo = get_session_info(stdCode, true);        // 获取交易时段信息

		auto& tickList = _ticks_cache[stdCode];                        // 获取Tick缓存列表
		if (tickList._cursor == UINT_MAX)                                // 如果游标未初始化
		{
			if (stime == UINT64_MAX)                                    // 如果开始时间为最大值（未指定）
			{
				/*
				 *	如果stime为UINT64_MAX
				 *	则说明还没有初始化
				 *	所以要确定第一笔是什么
				 */
				for(tickList._cursor = 1; ; tickList._cursor++)         // 从第一个Tick开始查找
				{
					//如果时间一直不满足，则直接跳出循环
					if(tickList._cursor > tickList._count)                // 如果游标超出数量
						break;                                            // 退出循环

					uint32_t tickMin = tickList._items[tickList._cursor-1].action_time / 100000;  // 提取Tick的分钟时间
					if (sInfo->isInTradingTime(tickMin))                  // 如果Tick时间在交易时间内
					{
						break;                                            // 退出循环（找到第一个有效Tick）
					}
				}
			}
			else                                                         // 如果开始时间已指定
			{
				uint32_t uDate = (uint32_t)(stime / 10000);             // 提取日期部分
				uint32_t uTime = (uint32_t)(stime % 10000);             // 提取时间部分

				WTSTickStruct curTick;                                   // 创建临时Tick结构体用于查找
				curTick.action_date = uDate;                             // 设置日期
				curTick.action_time = uTime * 100000;                    // 设置时间（转换为微秒）

				auto tit = std::lower_bound(tickList._items.begin(), tickList._items.end(), curTick, [](const WTSTickStruct& a, const WTSTickStruct& b) {  // 使用二分查找定位位置
					if (a.action_date != b.action_date)                  // 如果日期不同
						return a.action_date < b.action_date;            // 按日期比较
					else                                                 // 如果日期相同
						return a.action_time < b.action_time;            // 按时间比较
				});

				std::size_t idx = tit - tickList._items.begin();        // 计算索引位置
				tickList._cursor = idx + 1;                              // 设置游标（指向下一个Tick）
			}
		}

		if (tickList._cursor >= tickList._count)                        // 如果游标超出数量
			continue;                                                    // 跳过该合约

		uint32_t nextActionTime = tickList._items[tickList._cursor - 1].action_time;  // 获取下一个Tick的动作时间
		//By Wesley @ 2022.03.06
		//检查一下时间戳，如果不是交易时间的，就不回放了
		uint32_t nextMinTime = nextActionTime / 100000;                // 转换为分钟时间
		/*
		 *	By Wesley @ 2023.05.05
		 *	这里做了一个调整，主要是针对小节中间出现的tick数据
		 *	部分数据源可能会落地小节中间的数据，导致时间戳不在交易时间
		 *	因此先判断时间戳是否超出收盘时间，如果超出也不回放tick了
		 *	然后如果tick数据处于小节之间，但是不在交易时间，则指针一直步进
		 *	这次修改主要针对Issue#104
		 */
		//超过收盘时间就跳过了
		if(sInfo->offsetTime(nextMinTime, false) > sInfo->getCloseTime(true))  // 如果时间超过收盘时间
			continue;                                                    // 跳过该合约

		while (!sInfo->isInTradingTime(nextMinTime) && tickList._cursor>tickList._items.size())  // 如果不在交易时间且还有数据
		{
			tickList._cursor++;                                          // 游标向前移动
			nextActionTime = tickList._items[tickList._cursor - 1].action_time;  // 获取下一个Tick的动作时间
		}

		const WTSTickStruct& nextTick = tickList._items[tickList._cursor - 1];  // 获取下一个Tick
		uint64_t lastTime = (uint64_t)nextTick.action_date * 1000000000 + nextTick.action_time;  // 计算时间戳（日期*10^9 + 时间）

		nextTime = min(lastTime, nextTime);                              // 取较小的时间戳（最早的下一个Tick）
	}

	return nextTime;                                                     // 返回下一个Tick时间戳
}


/**
 * @brief 获取下一个逐笔成交时间
 * 
 * 在所有已订阅的合约中，找到下一个逐笔成交数据的时间戳
 * 
 * @param curTDate 当前交易日期
 * @param stime 开始时间（默认UINT64_MAX，表示不限制）
 * @return 下一个逐笔成交时间戳，如果没有则返回UINT64_MAX
 */
uint64_t HisDataReplayer::getNextTransTime(uint32_t curTDate, uint64_t stime /* = UINT64_MAX */)
{
	uint64_t nextTime = UINT64_MAX;                                     // 下一个时间戳（初始化为最大值）
	for (auto v : _trans_sub_map)                                        // 遍历所有逐笔成交订阅映射
	{
		const char* stdCode = v.first.c_str();                          // 获取合约代码
		if (!checkTransactions(stdCode, curTDate))                      // 如果逐笔成交数据未缓存或检查失败
			continue;                                                    // 跳过该合约

		auto& itemList = _trans_cache[stdCode];                        // 获取逐笔成交缓存列表
		if (itemList._cursor == UINT_MAX)                                // 如果游标未初始化
		{
			if (stime == UINT64_MAX)                                    // 如果开始时间为最大值（未指定）
				itemList._cursor = 1;                                    // 设置为第一个（从第一个开始）
			else                                                         // 如果开始时间已指定
			{
				uint32_t uDate = (uint32_t)(stime / 10000);             // 提取日期部分
				uint32_t uTime = (uint32_t)(stime % 10000);             // 提取时间部分

				WTSTransStruct curItem;                                  // 创建临时逐笔成交结构体用于查找
				curItem.action_date = uDate;                             // 设置日期
				curItem.action_time = uTime * 100000;                    // 设置时间（转换为微秒）

				auto tit = std::lower_bound(itemList._items.begin(), itemList._items.end(), curItem, [](const WTSTransStruct& a, const WTSTransStruct& b) {  // 使用二分查找定位位置
					if (a.action_date != b.action_date)                  // 如果日期不同
						return a.action_date < b.action_date;            // 按日期比较
					else                                                 // 如果日期相同
						return a.action_time < b.action_time;            // 按时间比较
				});

				std::size_t idx = tit - itemList._items.begin();        // 计算索引位置
				itemList._cursor = idx + 1;                              // 设置游标（指向下一个）
			}
		}

		if (itemList._cursor >= itemList._count)                        // 如果游标超出数量
			continue;                                                    // 跳过该合约

		const auto& nextItem = itemList._items[itemList._cursor - 1];    // 获取下一个逐笔成交
		uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳（日期*10^9 + 时间）

		nextTime = min(lastTime, nextTime);                              // 取较小的时间戳（最早的下一个）
	}

	return nextTime;                                                     // 返回下一个逐笔成交时间戳
}


/**
 * @brief 获取下一个订单明细时间
 * 
 * 在所有已订阅的合约中，找到下一个订单明细数据的时间戳
 * 
 * @param curTDate 当前交易日期
 * @param stime 开始时间（默认UINT64_MAX，表示不限制）
 * @return 下一个订单明细时间戳，如果没有则返回UINT64_MAX
 */
uint64_t HisDataReplayer::getNextOrdDtlTime(uint32_t curTDate, uint64_t stime /* = UINT64_MAX */)
{
	uint64_t nextTime = UINT64_MAX;                                     // 下一个时间戳（初始化为最大值）
	for (auto v : _orddtl_sub_map)                                       // 遍历所有订单明细订阅映射
	{
		const char* stdCode = v.first.c_str();                          // 获取合约代码
		if (!checkOrderDetails(stdCode, curTDate))                       // 如果订单明细数据未缓存或检查失败
			continue;                                                    // 跳过该合约

		auto& itemList = _orddtl_cache[stdCode];                       // 获取订单明细缓存列表
		if (itemList._cursor == UINT_MAX)                                // 如果游标未初始化
		{
			if (stime == UINT64_MAX)                                    // 如果开始时间为最大值（未指定）
				itemList._cursor = 1;                                    // 设置为第一个（从第一个开始）
			else                                                         // 如果开始时间已指定
			{
				uint32_t uDate = (uint32_t)(stime / 10000);             // 提取日期部分
				uint32_t uTime = (uint32_t)(stime % 10000);             // 提取时间部分

				WTSOrdDtlStruct curItem;                                 // 创建临时订单明细结构体用于查找
				curItem.action_date = uDate;                             // 设置日期
				curItem.action_time = uTime * 100000;                    // 设置时间（转换为微秒）

				auto tit = std::lower_bound(itemList._items.begin(), itemList._items.end(), curItem, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {  // 使用二分查找定位位置
					if (a.action_date != b.action_date)                  // 如果日期不同
						return a.action_date < b.action_date;            // 按日期比较
					else                                                 // 如果日期相同
						return a.action_time < b.action_time;            // 按时间比较
				});

				std::size_t idx = tit - itemList._items.begin();        // 计算索引位置
				itemList._cursor = idx + 1;                              // 设置游标（指向下一个）
			}
		}

		if (itemList._cursor >= itemList._count)                        // 如果游标超出数量
			continue;                                                    // 跳过该合约

		const auto& nextItem = itemList._items[itemList._cursor - 1];    // 获取下一个订单明细
		uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳（日期*10^9 + 时间）

		nextTime = min(lastTime, nextTime);                              // 取较小的时间戳（最早的下一个）
	}

	return nextTime;                                                     // 返回下一个订单明细时间戳
}


/**
 * @brief 获取下一个订单队列时间
 * 
 * 在所有已订阅的合约中，找到下一个订单队列数据的时间戳
 * 
 * @param curTDate 当前交易日期
 * @param stime 开始时间（默认UINT64_MAX，表示不限制）
 * @return 下一个订单队列时间戳，如果没有则返回UINT64_MAX
 */
uint64_t HisDataReplayer::getNextOrdQueTime(uint32_t curTDate, uint64_t stime /* = UINT64_MAX */)
{
	uint64_t nextTime = UINT64_MAX;                                     // 下一个时间戳（初始化为最大值）
	for (auto v : _ordque_sub_map)                                       // 遍历所有订单队列订阅映射
	{
		const char* stdCode = v.first.c_str();                          // 获取合约代码
		if (!checkOrderQueues(stdCode, curTDate))                        // 如果订单队列数据未缓存或检查失败
			continue;                                                    // 跳过该合约

		auto& itemList = _ordque_cache[stdCode];                       // 获取订单队列缓存列表
		if (itemList._cursor == UINT_MAX)                                // 如果游标未初始化
		{
			if (stime == UINT64_MAX)                                    // 如果开始时间为最大值（未指定）
				itemList._cursor = 1;                                    // 设置为第一个（从第一个开始）
			else                                                         // 如果开始时间已指定
			{
				uint32_t uDate = (uint32_t)(stime / 10000);             // 提取日期部分
				uint32_t uTime = (uint32_t)(stime % 10000);             // 提取时间部分

				WTSOrdQueStruct curItem;                                 // 创建临时订单队列结构体用于查找
				curItem.action_date = uDate;                             // 设置日期
				curItem.action_time = uTime * 100000;                    // 设置时间（转换为微秒）

				auto tit = std::lower_bound(itemList._items.begin(), itemList._items.end(), curItem, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {  // 使用二分查找定位位置
					if (a.action_date != b.action_date)                  // 如果日期不同
						return a.action_date < b.action_date;            // 按日期比较
					else                                                 // 如果日期相同
						return a.action_time < b.action_time;            // 按时间比较
				});

				std::size_t idx = tit - itemList._items.begin();        // 计算索引位置
				itemList._cursor = idx + 1;                              // 设置游标（指向下一个）
			}
		}

		if (itemList._cursor >= itemList._count)                        // 如果游标超出数量
			continue;                                                    // 跳过该合约

		const auto& nextItem = itemList._items[itemList._cursor - 1];    // 获取下一个订单队列
		uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳（日期*10^9 + 时间）

		nextTime = min(lastTime, nextTime);                              // 取较小的时间戳（最早的下一个）
	}

	return nextTime;                                                     // 返回下一个订单队列时间戳
}

/**
 * @brief 按天回放HFT数据
 * 
 * 按照时间顺序回放当前交易日的所有HFT数据（Tick、逐笔成交、订单明细、订单队列）
 * 
 * @param curTDate 当前交易日期
 * @return 回放的Tick总数
 */
uint64_t HisDataReplayer::replayHftDatasByDay(uint32_t curTDate)
{
	uint64_t total_ticks = 0;                                            // Tick总数计数器
	for (;!_terminated;)                                                 // 当未终止时循环
	{
		//先确定下一笔tick的时间
		uint64_t nextTime = min(UINT64_MAX, getNextTickTime(curTDate));  // 获取下一个Tick时间
		nextTime = min(nextTime, getNextOrdDtlTime(curTDate));            // 与下一个订单明细时间比较，取较小值
		nextTime = min(nextTime, getNextOrdQueTime(curTDate));            // 与下一个订单队列时间比较，取较小值
		nextTime = min(nextTime, getNextTransTime(curTDate));             // 与下一个逐笔成交时间比较，取较小值

		if(nextTime == UINT64_MAX)                                       // 如果没有下一个时间（所有数据已回放完）
			break;                                                        // 退出循环

		/*
		 *	By Wesley @ 2022.03.06
		 *	下面的回放逻辑，都改成先修改光标cursor，再触发回调
		 *	这个逻辑也符合实盘情况
		 */

		//再根据时间回放tick数据
		_cur_date = (uint32_t)(nextTime / 1000000000);                  // 提取日期部分（除以10^9）
		_cur_time = nextTime % 1000000000 / 100000;                      // 提取时间部分（取模10^9再除以10^5）
		_cur_secs = nextTime % 100000;                                   // 提取秒数部分（取模10^5）

		//1、首先回放委托明细
		for (auto& v : _orddtl_sub_map)                                  // 遍历所有订单明细订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _orddtl_cache[stdCode];                    // 获取订单明细缓存列表
			//By Wesley @ 2022.03.06 
			//这里加了一个数据的判断
			//如果数据为空，则不再进行回放
			if (itemList._items.empty() || itemList._cursor > itemList._count)  // 如果数据为空或游标超出数量
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个订单明细
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果订单明细时间小于等于下一个时间
			{
				itemList._cursor++;                                      // 游标向前移动

				WTSOrdDtlData* newData = WTSOrdDtlData::create(nextItem);  // 创建订单明细数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_order_detail(stdCode, newData);        // 调用订单明细回调
				newData->release();                                      // 释放数据对象

				total_ticks++;                                           // 增加Tick计数
			}
		}

		//2、其次再回放成交明细
		for (auto& v : _trans_sub_map)                                  // 遍历所有逐笔成交订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _trans_cache[stdCode];                     // 获取逐笔成交缓存列表
			//By Wesley @ 2022.03.06 
			//这里加了一个数据的判断
			//如果数据为空，则不再进行回放
			if (itemList._items.empty() || itemList._cursor > itemList._count)  // 如果数据为空或游标超出数量
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个逐笔成交
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果逐笔成交时间小于等于下一个时间
			{
				itemList._cursor++;                                      // 游标向前移动

				WTSTransData* newData = WTSTransData::create(nextItem);  // 创建逐笔成交数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_transaction(stdCode, newData);         // 调用逐笔成交回调
				newData->release();                                      // 释放数据对象
				
				total_ticks++;                                           // 增加Tick计数
			}
		}

		//3、第三步再回放tick数据
		for (auto& v : _tick_sub_map)                                   // 遍历所有Tick订阅映射
		{
			//std::string stdCode = v.first;
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			HftDataList<WTSTickStruct>& tickList = _ticks_cache[stdCode];  // 获取Tick缓存列表
			//By Wesley @ 2022.03.06 
			//这里加了一个数据的判断
			//如果数据为空，则不再进行回放
			if(tickList._items.empty() || tickList._cursor > tickList._count)  // 如果数据为空或游标超出数量
				continue;                                                // 跳过该合约

			WTSTickStruct& nextTick = tickList._items[tickList._cursor - 1];  // 获取下一个Tick
			uint64_t lastTime = (uint64_t)nextTick.action_date * 1000000000 + nextTick.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果Tick时间小于等于下一个时间
			{
				tickList._cursor++;                                      // 游标向前移动

				update_price(stdCode, nextTick.price);                   // 更新价格映射
				WTSTickData* newTick = WTSTickData::create(nextTick);   // 创建Tick数据对象
				newTick->setCode(stdCode);                               // 设置合约代码
				_listener->handle_tick(stdCode, newTick, 0);             // 调用Tick回调
				newTick->release();                                      // 释放数据对象
				
				total_ticks++;                                           // 增加Tick计数
			}
		}
		
		//4、最后回放委托队列
		for (auto& v : _ordque_sub_map)                                 // 遍历所有订单队列订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _ordque_cache[stdCode];                    // 获取订单队列缓存列表
			//By Wesley @ 2022.03.06 
			//这里加了一个数据的判断
			//如果数据为空，则不再进行回放
			if (itemList._items.empty() || itemList._cursor > itemList._count)  // 如果数据为空或游标超出数量
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个订单队列
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果订单队列时间小于等于下一个时间
			{
				itemList._cursor++;                                      // 游标向前移动

				WTSOrdQueData* newData = WTSOrdQueData::create(nextItem);  // 创建订单队列数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_order_queue(stdCode, newData);         // 调用订单队列回调
				newData->release();                                      // 释放数据对象
				
				total_ticks++;                                           // 增加Tick计数
			}
		}
	}

	return total_ticks;                                                  // 返回Tick总数
}

/**
 * @brief 回放HFT数据
 * 
 * 在指定的时间范围内，按照时间顺序回放HFT数据（Tick、逐笔成交、订单明细、订单队列）
 * 
 * @param stime 开始时间
 * @param etime 结束时间
 * @return 是否成功回放（如果找到数据返回true，否则返回false）
 */
bool HisDataReplayer::replayHftDatas(uint64_t stime, uint64_t etime)
{	
	WTSLogger::log_raw(LL_DEBUG, "replaying hft data...");              // 记录调试日志
	for (;;)                                                             // 无限循环
	{
		uint64_t nextTime = min(UINT64_MAX, getNextTickTime(_cur_tdate, stime));  // 获取下一个Tick时间
		if (nextTime == UINT64_MAX)                                      // 如果没有下一个时间（所有数据已回放完）
			return false;                                                 // 返回false

		nextTime = min(nextTime, getNextOrdDtlTime(_cur_tdate, stime));  // 与下一个订单明细时间比较，取较小值
		nextTime = min(nextTime, getNextOrdQueTime(_cur_tdate, stime));  // 与下一个订单队列时间比较，取较小值
		nextTime = min(nextTime, getNextTransTime(_cur_tdate, stime));   // 与下一个逐笔成交时间比较，取较小值

		if (nextTime/100000 >= etime)                                    // 如果下一个时间（转换为分钟）大于等于结束时间
			break;                                                        // 退出循环

		_cur_date = (uint32_t)(nextTime / 1000000000);                   // 提取日期部分（除以10^9）
		_cur_time = nextTime % 1000000000 / 100000;                      // 提取时间部分（取模10^9再除以10^5）
		_cur_secs = nextTime % 100000;                                   // 提取秒数部分（取模10^5）
		
		//1、首先回放委托明细
		for (auto& v : _orddtl_sub_map)                                  // 遍历所有订单明细订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _orddtl_cache[stdCode];                    // 获取订单明细缓存列表
			if (itemList._cursor > itemList._count)                      // 如果游标超出数量
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个订单明细
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果订单明细时间小于等于下一个时间
			{
				WTSOrdDtlData* newData = WTSOrdDtlData::create(nextItem);  // 创建订单明细数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_order_detail(stdCode, newData);        // 调用订单明细回调
				newData->release();                                      // 释放数据对象

				itemList._cursor++;                                      // 游标向前移动
			}
		}

		//2、其次再回放成交明细
		for (auto& v : _trans_sub_map)                                  // 遍历所有逐笔成交订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _trans_cache[stdCode];                     // 获取逐笔成交缓存列表
			if (itemList._cursor = itemList._count)                     // 如果游标等于数量（已全部回放完）
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个逐笔成交
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果逐笔成交时间小于等于下一个时间
			{
				WTSTransData* newData = WTSTransData::create(nextItem);  // 创建逐笔成交数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_transaction(stdCode, newData);         // 调用逐笔成交回调
				newData->release();                                      // 释放数据对象

				itemList._cursor++;                                      // 游标向前移动
			}
		}

		//3、第三步再回放tick数据
		for (auto& v : _tick_sub_map)                                   // 遍历所有Tick订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _ticks_cache[stdCode];                     // 获取Tick缓存列表
			if (itemList._cursor > itemList._count)                      // 如果游标超出数量
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个Tick
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果Tick时间小于等于下一个时间
			{
				update_price(stdCode, nextItem.price);                   // 更新价格映射
				WTSTickData* newData = WTSTickData::create(nextItem);   // 创建Tick数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_tick(stdCode, newData, 0);             // 调用Tick回调
				newData->release();                                      // 释放数据对象

				itemList._cursor++;                                      // 游标向前移动
			}
		}

		//4、最后回放委托队列
		for (auto& v : _ordque_sub_map)                                 // 遍历所有订单队列订阅映射
		{
			const char* stdCode = v.first.c_str();                      // 获取合约代码
			auto& itemList = _ordque_cache[stdCode];                    // 获取订单队列缓存列表
			if (itemList._cursor > itemList._count)                      // 如果游标超出数量
				continue;                                                // 跳过该合约

			auto& nextItem = itemList._items[itemList._cursor - 1];      // 获取下一个订单队列
			uint64_t lastTime = (uint64_t)nextItem.action_date * 1000000000 + nextItem.action_time;  // 计算时间戳
			if (lastTime <= nextTime)                                    // 如果订单队列时间小于等于下一个时间
			{
				WTSOrdQueData* newData = WTSOrdQueData::create(nextItem);  // 创建订单队列数据对象
				newData->setCode(stdCode);                               // 设置合约代码
				_listener->handle_order_queue(stdCode, newData);         // 调用订单队列回调
				newData->release();                                      // 释放数据对象

				itemList._cursor++;                                      // 游标向前移动
			}
		}
	}

	return true;                                                         // 返回成功
}

/**
 * @brief 处理分钟线结束
 * 
 * 在分钟线结束时，触发所有已到期的K线收盘回调，并更新未订阅K线的游标
 * 
 * @param uDate 日期
 * @param uTime 时间
 * @param endTDate 结束交易日期（默认0，表示不限制）
 * @param tickSimulated 是否模拟Tick（默认true）
 */
void HisDataReplayer::onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate /* = 0 */, bool tickSimulated /* = true */)
{
	//这里应该触发检查
	uint64_t nowTime = (uint64_t)uDate * 10000 + uTime;                // 计算当前时间戳

	for (auto it = _bars_cache.begin(); it != _bars_cache.end(); it++)  // 遍历所有K线缓存
	{
		BarsListPtr& barsList = (BarsListPtr&)it->second;             // 获取K线列表指针
		barsList->mark();                                               // 标记为已使用（重置未使用天数）
		double factor = 1.0;// barsList->_factor;                       // 复权因子（当前未使用）
		if (barsList->_period != KP_DAY)                               // 如果不是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for (;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					uint64_t barTime = 199000000000 + nextBar.time;    // 计算K线时间戳（加上基准时间）
					if (barTime <= nowTime)                             // 如果K线时间小于等于当前时间（K线已到期）
					{
						uint32_t times = barsList->_times;              // 获取倍数
						if (barsList->_period == KP_Minute5)             // 如果是5分钟线
							times *= 5;                                  // 倍数乘以5
						_listener->handle_bar_close(barsList->_code.c_str(), "m", times, &nextBar);  // 调用K线收盘回调
					}
					else                                                 // 如果K线时间大于当前时间
					{
						break;                                           // 退出循环
					}

					barsList->_cursor++;                                // 游标向前移动

					if (barsList->_cursor == barsList->_bars.size())     // 如果游标超出K线列表大小
						break;                                           // 退出循环
				}
			}
		}
		else                                                             // 如果是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for (;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					if (nextBar.date <= endTDate)                       // 如果K线日期小于等于结束交易日期
					{
						_listener->handle_bar_close(barsList->_code.c_str(), "d", barsList->_times, &nextBar);  // 调用K线收盘回调
					}
					else                                                 // 如果K线日期大于结束交易日期
					{
						break;                                           // 退出循环
					}

					barsList->_cursor++;                                // 游标向前移动

					if (barsList->_cursor >= barsList->_bars.size())     // 如果游标超出K线列表大小
						break;                                           // 退出循环
				}
			}
		}
	}

	for (auto it = _unbars_cache.begin(); it != _unbars_cache.end(); it++)  // 遍历未订阅K线缓存
	{
		BarsListPtr& barsList = (BarsListPtr&)it->second;             // 获取K线列表指针
		double factor = 1.0;// barsList->_factor;                       // 复权因子（当前未使用）
		if (barsList->_period != KP_DAY)                               // 如果不是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for (;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					uint64_t barTime = 199000000000 + nextBar.time;    // 计算K线时间戳（加上基准时间）
					if (barTime > nowTime)                              // 如果K线时间大于当前时间
						break;                                           // 退出循环

					barsList->_cursor++;                                // 游标向前移动

					if (barsList->_cursor == barsList->_bars.size())     // 如果游标超出K线列表大小
						break;                                           // 退出循环
				}
			}
		}
		else                                                             // 如果是日线
		{
			if (barsList->_bars.size() > barsList->_cursor)            // 如果K线列表还有数据
			{
				for (;;)                                                 // 无限循环
				{
					WTSBarStruct& nextBar = barsList->_bars[barsList->_cursor];  // 获取当前游标指向的K线

					if (nextBar.date > endTDate)                        // 如果K线日期大于结束交易日期
						break;                                           // 退出循环

					barsList->_cursor++;                                // 游标向前移动

					if (barsList->_cursor >= barsList->_bars.size())     // 如果游标超出K线列表大小
						break;                                           // 退出循环
				}
			}
		}
	}

	if (_listener)                                                       // 如果数据接收器存在
		_listener->handle_schedule(uDate, uTime);                       // 调用定时调度回调
}

/**
 * @brief 获取K线切片
 * 
 * 根据合约代码、周期、数量等参数，从缓存中获取K线切片数据
 * 
 * @param stdCode 合约代码
 * @param period 周期字符串（"m"-分钟，"d"-日线）
 * @param count 数量
 * @param times 倍数（默认1）
 * @param isMain 是否主周期（默认false）
 * @return K线切片指针
 */
WTSKlineSlice* HisDataReplayer::get_kline_slice(const char* stdCode, const char* period, uint32_t count, uint32_t times /* = 1 */, bool isMain /* = false */)
{
	thread_local static char key[64] = { 0 };                             // 线程局部静态变量，用于存储键值
	fmtutil::format_to(key, "{}#{}#{}", stdCode, period, times);       // 格式化键值（合约代码#周期#倍数）

	if (isMain)                                                          // 如果是主周期
	{
		_main_key = key;                                                  // 设置主键
		_main_period = fmt::format("{}#{}", period, times);             // 设置主周期字符串
	}

	//if(!_tick_enabled)
	//不做判断,主要为了防止没有tick数据,而采用第二方案
	{
		if(_ticker_keys.find(stdCode) == _ticker_keys.end())             // 如果Ticker键映射表中不存在该合约
			_ticker_keys[stdCode] = key;                                 // 添加映射关系
		else                                                              // 如果已存在
		{
			std::string oldKey = _ticker_keys[stdCode];                  // 获取旧的键值
			oldKey = oldKey.substr(strlen(stdCode) + 1);                 // 截取周期部分
			if (strcmp(period, "m") == 0 && oldKey.at(0) == 'd')       // 如果当前是分钟线且旧的是日线
			{
				_ticker_keys[stdCode] = key;                             // 更新映射关系（分钟线优先）
				_min_period = period;                                     // 更新最小周期
			}
			else if (oldKey.at(0) == period[0] && times < strtoul(oldKey.substr(2).c_str(), NULL, 10))  // 如果周期相同且倍数更小
			{
				_ticker_keys[stdCode] = key;                             // 更新映射关系（更小倍数优先）
				_min_period = period;                                     // 更新最小周期
			}
		}

		auto len = strlen(stdCode);                                       // 获取合约代码长度
		char lastCh = stdCode[len - 1];                                   // 获取最后一个字符
		if(lastCh == SUFFIX_HFQ || lastCh == SUFFIX_QFQ)                 // 如果是复权数据（后复权或前复权）
		{
			//如果是复权数据，则要把原始数据放到需要的列表中，最后再做检查
			std::string tickCode(stdCode, len - 1);                      // 去掉复权标记，获取原始代码
			_unsubbed_in_need.insert(tickCode);                          // 添加到未订阅但需要的集合中
		}
	}

	WTSKlinePeriod kp;                                                   // K线周期枚举
	uint32_t realTimes = times;                                          // 实际倍数
	uint32_t baseTimes = 1;                                              // 基础倍数
	if (strcmp(period, "m") == 0)                                        // 如果是分钟线
	{
		if(times % 5 == 0)                                               // 如果倍数能被5整除
		{
			kp = KP_Minute5;                                             // 使用5分钟线周期
			baseTimes = 5;                                               // 基础倍数为5
			realTimes /= 5;                                              // 实际倍数除以5
		}
		else                                                              // 如果倍数不能被5整除
		{
			kp = KP_Minute1;                                             // 使用1分钟线周期
		}
	}
	else                                                                   // 如果是日线
		kp = KP_DAY;

	bool isDay = kp == KP_DAY;

	auto it = _bars_cache.find(key);
	bool bHasHisData = false;
	bool bHasCache = (it != _bars_cache.end());
	if (!bHasCache)
	{
		if (realTimes != 1)
		{
			std::string rawKey = StrUtil::printf("%s#%s#%u", stdCode, period, baseTimes);
			if (_bars_cache.find(rawKey) == _bars_cache.end())
			{
				/*
				 *	By Wesley @ 2021.12.20
				 *	先从extloader加载数据，如果加载不到，再走原来的历史数据存储引擎加载
				 */
				if(NULL != _bt_loader)
					bHasHisData = cacheFinalBarsFromLoader(rawKey, stdCode, kp);
				
				if(!bHasHisData)
				{
					if (_mode == "csv")
					{
						bHasHisData = cacheRawBarsFromCSV(rawKey, stdCode, kp);
					}
					else
					{
						bHasHisData = cacheRawBarsFromBin(rawKey, stdCode, kp);
					}
				}
				
			}
			else
			{
				bHasHisData = true;
			}
		}
		else
		{
			/*
			 *	By Wesley @ 2021.12.20
			 *	先从extloader加载数据，如果加载不到，再走原来的历史数据存储引擎加载
			 */
			if (NULL != _bt_loader)
			{
				bHasHisData = cacheFinalBarsFromLoader(key, stdCode, kp);
			}

			if(!bHasHisData)
			{
				if (_mode == "csv")
				{
					bHasHisData = cacheRawBarsFromCSV(key, stdCode, kp);
				}
				else
				{
					bHasHisData = cacheRawBarsFromBin(key, stdCode, kp);
				}
			}
			
		}
	}
	else
	{
		bHasHisData = true;
	}

	if (!bHasHisData)
		return NULL;

	WTSSessionInfo* sInfo = get_session_info(stdCode, true);
	if(sInfo == NULL)
	{
		WTSLogger::error("Cannot find corresponding session of {}", stdCode);
		return NULL;
	}

	bool isClosed = (sInfo->offsetTime(_cur_time, true) >= sInfo->getCloseTime(true));
	if (realTimes != 1 && !bHasCache)
	{	
		std::string rawKey = StrUtil::printf("%s#%s#%u", stdCode, period, baseTimes);
		BarsListPtr& rawBars = _bars_cache[rawKey];
		WTSKlineSlice* rawKline = WTSKlineSlice::create(stdCode, kp, realTimes, &rawBars->_bars[0], rawBars->_bars.size());
		rawKline->setCode(stdCode);

		static WTSDataFactory dataFact;
		WTSKlineData* kData = dataFact.extractKlineData(rawKline, kp, realTimes, sInfo, true, _align_by_section);
		rawKline->release();

		if(kData)
		{
			_bars_cache[key].reset(new BarsList());
			BarsListPtr barsList = _bars_cache[key];
			barsList->_code = stdCode;
			barsList->_period = kp;
			barsList->_times = realTimes;
			barsList->_count = kData->size();
			barsList->_bars.swap(kData->getDataRef());
			kData->release();
			WTSLogger::info("{} resampled {}{} back kline of {} ready", barsList->_bars.size(), period, times, stdCode);
		}
		else
		{
			WTSLogger::error("Resampling {}{} back kline of {} failed", period, times, stdCode);
			return NULL;
		}
	}

	BarsListPtr& kBlkPair = _bars_cache[key];
	if(kBlkPair == NULL)
	{
		return NULL;
	}

	_codes_in_subbed.insert(stdCode);

	if (kBlkPair->_cursor == UINT_MAX)
	{
		//还没有经过初始定位
		WTSBarStruct bar;
		bar.date = _cur_tdate;
		if(kp != KP_DAY)
			bar.time = (_cur_date - 19900000) * 10000 + _cur_time;
		
		auto it = std::lower_bound(kBlkPair->_bars.begin(), kBlkPair->_bars.end(), bar, [isDay, isClosed](const WTSBarStruct& a, const WTSBarStruct& b){
			if (isDay)
				if (!isClosed)
					return a.date < b.date;
				else 
					return a.date <= b.date;
			else
				return a.time < b.time;
		});

		std::size_t eIdx = it - kBlkPair->_bars.begin();

		if (it != kBlkPair->_bars.end())
		{
			WTSBarStruct& curBar = *it;
			if (isDay)
			{
				if (curBar.date >= _cur_tdate && !isClosed)
				{
					if (eIdx > 0)
					{
						it--;
						eIdx--;
					}

				}
				else if (curBar.date > _cur_tdate && isClosed)
				{
					if (eIdx > 0)
					{
						it--;
						eIdx--;
					}
				}

				/*
				 *	By Wesley @ 2022.11.04
				 *	根据Issue#122，加了一个兜底的判断
				 *	主要防止日线回测漏掉第一根bar
				 */
				if(eIdx == 0 && curBar.date > _cur_tdate)
				{
					kBlkPair->_cursor = 0;
				}
				else
				{
					kBlkPair->_cursor = eIdx + 1;
				}
			}
			else
			{
				if (curBar.time > bar.time)
				{
					if (eIdx > 0)
					{
						it--;
						eIdx--;
					}
				}

				kBlkPair->_cursor = eIdx + 1;
			}
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		uint32_t curMin = (_cur_date - 19900000) * 10000 + _cur_time;
		if (isDay)
		{
			if (kBlkPair->_cursor <= kBlkPair->_count)
			{
				if(!isClosed)
				{
					while (kBlkPair->_bars[kBlkPair->_cursor - 1].date < _cur_tdate  && kBlkPair->_cursor < kBlkPair->_count && kBlkPair->_bars[kBlkPair->_cursor].date < _cur_tdate)
					{
						kBlkPair->_cursor++;
					}
				}
				else
				{
					while (kBlkPair->_bars[kBlkPair->_cursor - 1].date <= _cur_tdate  && kBlkPair->_cursor < kBlkPair->_count && kBlkPair->_bars[kBlkPair->_cursor].date <= _cur_tdate)
					{
						kBlkPair->_cursor++;
					}
				}
				

				if (kBlkPair->_bars[kBlkPair->_cursor - 1].date > _cur_tdate)
					kBlkPair->_cursor--;
			}
		}
		else
		{
			if (kBlkPair->_cursor <= kBlkPair->_count)
			{
				while (kBlkPair->_bars[kBlkPair->_cursor-1].time < curMin && kBlkPair->_cursor < kBlkPair->_count)
				{
					kBlkPair->_cursor++;
				}

				if (kBlkPair->_bars[kBlkPair->_cursor - 1].time > curMin)
					kBlkPair->_cursor--;
			}
		}
	}


	if (kBlkPair->_cursor == 0)
		return NULL;

	uint32_t sIdx = 0;
	if (kBlkPair->_cursor > count)
		sIdx = kBlkPair->_cursor - count;

	uint32_t realCnt = kBlkPair->_cursor - sIdx;
	WTSKlineSlice* kline = WTSKlineSlice::create(stdCode, kp, 1, kBlkPair->_bars.data() + sIdx, realCnt);
	return kline;
}

WTSTickSlice* HisDataReplayer::get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime)
{
	if (!_tick_enabled)
		return NULL;

	if (!checkTicks(stdCode, _cur_tdate))
		return NULL;

	auto& tickList = _ticks_cache[stdCode];
	if (tickList._cursor == 0)
		return NULL;

	if (tickList._cursor == UINT_MAX)
	{
		uint32_t uDate = _cur_date;
		uint32_t uTime = _cur_time * 100000 + _cur_secs;

		if (etime != 0)
		{
			uDate = (uint32_t)(etime / 10000);
			uTime = (uint32_t)(etime % 10000 * 100000);
		}

		WTSTickStruct curTick;
		curTick.action_date = uDate;
		curTick.action_time = uTime;

		auto tit = std::lower_bound(tickList._items.begin(), tickList._items.end(), curTick, [](const WTSTickStruct& a, const WTSTickStruct& b){
			if (a.action_date != b.action_date)
				return a.action_date < b.action_date;
			else
				return a.action_time < b.action_time;
		});

		if(tit == tickList._items.end())
		{
			tickList._cursor = tickList._items.size();
		}
		else
		{
			
			std::size_t idx = tit - tickList._items.begin();
			const WTSTickStruct& thisTick = *tit;
			if (thisTick.action_date > uDate || (thisTick.action_date == uDate && thisTick.action_time > uTime))
			{
				if(idx > 0)
				{
					tit--;
					idx--;
				}
				else
				{
					return NULL;
				}
			}

			tickList._cursor = idx + 1;
		}
	}
	
	//cursor是下一笔tick的index+1，大于当前截止时间的
	//所以要获取当前截止时间之前的最后一笔tick，需要-2
	if (tickList._cursor < 2)
		return NULL;
	uint32_t eIdx = tickList._cursor - 2;
	uint32_t sIdx = 0;
	if (eIdx >= count - 1)
		sIdx = eIdx + 1 - count;

	uint32_t realCnt = eIdx - sIdx + 1;
	if (realCnt == 0)
		return NULL;

	WTSTickSlice* ticks = WTSTickSlice::create(stdCode, tickList._items.data() + sIdx, realCnt);
	return ticks;
}

WTSOrdDtlSlice* HisDataReplayer::get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (!checkOrderDetails(stdCode, _cur_tdate))
		return NULL;

	auto& dataList = _orddtl_cache[stdCode];
	if (dataList._cursor == 0)
		return NULL;

	if (dataList._cursor == UINT_MAX)
	{
		uint32_t uDate = _cur_date;
		uint32_t uTime = _cur_time * 100000 + _cur_secs;

		if (etime != 0)
		{
			uDate = (uint32_t)(etime / 10000);
			uTime = (uint32_t)(etime % 10000 * 100000);
		}

		WTSOrdDtlStruct curItem;
		curItem.action_date = uDate;
		curItem.action_time = uTime;

		auto tit = std::lower_bound(dataList._items.begin(), dataList._items.end(), curItem, [](const WTSOrdDtlStruct& a, const WTSOrdDtlStruct& b) {
			if (a.action_date != b.action_date)
				return a.action_date < b.action_date;
			else
				return a.action_time < b.action_time;
		});

		std::size_t idx = tit - dataList._items.begin();
		dataList._cursor = idx + 1;
	}

	uint32_t eIdx = dataList._cursor - 1;
	uint32_t sIdx = 0;
	if (eIdx >= count - 1)
		sIdx = eIdx + 1 - count;

	uint32_t realCnt = eIdx - sIdx + 1;
	if (realCnt == 0)
		return NULL;

	WTSOrdDtlSlice* ticks = WTSOrdDtlSlice::create(stdCode, dataList._items.data() + sIdx, realCnt);
	return ticks;
}

WTSOrdQueSlice* HisDataReplayer::get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (!checkOrderQueues(stdCode, _cur_tdate))
		return NULL;

	auto& dataList = _ordque_cache[stdCode];
	if (dataList._cursor == 0)
		return NULL;

	if (dataList._cursor == UINT_MAX)
	{
		uint32_t uDate = _cur_date;
		uint32_t uTime = _cur_time * 100000 + _cur_secs;

		if (etime != 0)
		{
			uDate = (uint32_t)(etime / 10000);
			uTime = (uint32_t)(etime % 10000 * 100000);
		}

		WTSOrdQueStruct curItem;
		curItem.action_date = uDate;
		curItem.action_time = uTime;

		auto tit = std::lower_bound(dataList._items.begin(), dataList._items.end(), curItem, [](const WTSOrdQueStruct& a, const WTSOrdQueStruct& b) {
			if (a.action_date != b.action_date)
				return a.action_date < b.action_date;
			else
				return a.action_time < b.action_time;
		});

		std::size_t idx = tit - dataList._items.begin();
		dataList._cursor = idx + 1;
	}

	uint32_t eIdx = dataList._cursor - 1;
	uint32_t sIdx = 0;
	if (eIdx >= count - 1)
		sIdx = eIdx + 1 - count;

	uint32_t realCnt = eIdx - sIdx + 1;
	if (realCnt == 0)
		return NULL;

	WTSOrdQueSlice* ticks = WTSOrdQueSlice::create(stdCode, dataList._items.data() + sIdx, realCnt);
	return ticks;
}

WTSTransSlice* HisDataReplayer::get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	if (!checkTransactions(stdCode, _cur_tdate))
		return NULL;

	auto& dataList = _trans_cache[stdCode];
	if (dataList._cursor == 0)
		return NULL;

	if (dataList._cursor == UINT_MAX)
	{
		uint32_t uDate = _cur_date;
		uint32_t uTime = _cur_time * 100000 + _cur_secs;

		if (etime != 0)
		{
			uDate = (uint32_t)(etime / 10000);
			uTime = (uint32_t)(etime % 10000 * 100000);
		}

		WTSTransStruct curItem;
		curItem.action_date = uDate;
		curItem.action_time = uTime;

		auto tit = std::lower_bound(dataList._items.begin(), dataList._items.end(), curItem, [](const WTSTransStruct& a, const WTSTransStruct& b) {
			if (a.action_date != b.action_date)
				return a.action_date < b.action_date;
			else
				return a.action_time < b.action_time;
		});

		std::size_t idx = tit - dataList._items.begin();
		dataList._cursor = idx + 1;
	}

	std::size_t eIdx = dataList._cursor - 1;
	std::size_t sIdx = 0;
	if (eIdx >= count - 1)
		sIdx = eIdx + 1 - count;

	std::size_t realCnt = eIdx - sIdx + 1;
	if (realCnt == 0)
		return NULL;

	WTSTransSlice* ticks = WTSTransSlice::create(stdCode, dataList._items.data() + sIdx, realCnt);
	return ticks;
}

/**
 * @brief 检查所有Tick数据是否已缓存
 * 
 * 检查所有已订阅合约的Tick数据是否都已缓存到指定日期
 * 
 * @param uDate 交易日期
 * @return 是否至少有一个合约的Tick数据已缓存
 */
bool HisDataReplayer::checkAllTicks(uint32_t uDate)
{
	bool bHasTick = false;                                               // 是否有Tick数据标志
	for (auto& v : _tick_sub_map)                                        // 遍历所有Tick订阅映射
	{
		bHasTick = checkTicks(v.first.c_str(), uDate) || bHasTick;      // 检查该合约的Tick数据，并更新标志
	}

	return bHasTick;                                                     // 返回是否有Tick数据
}

/**
 * @brief 检查订单明细数据是否已缓存
 * 
 * 检查指定合约的订单明细数据是否已缓存到指定日期，如果未缓存则尝试加载
 * 
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存或已经缓存
 */
bool HisDataReplayer::checkOrderDetails(const char* stdCode, uint32_t uDate)
{
	bool bNeedCache = false;                                             // 是否需要缓存标志
	auto it = _orddtl_cache.find(stdCode);                               // 查找订单明细缓存
	if (it == _orddtl_cache.end())                                       // 如果缓存中不存在
		bNeedCache = true;                                               // 标记为需要缓存
	else                                                                  // 如果缓存中存在
	{
		auto& dataList = it->second;                                     // 获取缓存列表
		if (dataList._date != uDate)                                     // 如果日期不匹配
			bNeedCache = true;                                           // 标记为需要缓存
	}


	if (bNeedCache)                                                      // 如果需要缓存
	{
		bool hasData = false;                                            // 是否有数据标志
		if (_mode == "csv")                                              // 如果是CSV模式
		{
			WTSLogger::error("Cannot use stock level2 data in csv mode!");  // 记录错误日志（CSV模式不支持Level2数据）
			return false;                                                 // 返回失败
		}
		else                                                              // 如果不是CSV模式
		{
			hasData = cacheRawOrdDtlFromBin(stdCode, stdCode, uDate);    // 从二进制文件缓存订单明细数据
		}

		if (!hasData)                                                    // 如果没有数据
		{
			auto& dataList = _trans_cache[stdCode];                     // 获取逐笔成交缓存（注意：这里可能是bug，应该是_orddtl_cache）
			dataList._items.resize(0);                                   // 清空数据项
			dataList._cursor = UINT_MAX;                                  // 重置游标
			dataList._code = stdCode;                                     // 设置合约代码
			dataList._date = uDate;                                      // 设置日期
			dataList._count = 0;                                          // 设置数量为0
			return false;                                                 // 返回失败
		}
	}

	return true;                                                         // 返回成功
}

/**
 * @brief 检查订单队列数据是否已缓存
 * 
 * 检查指定合约的订单队列数据是否已缓存到指定日期，如果未缓存则尝试加载
 * 
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存或已经缓存
 */
bool HisDataReplayer::checkOrderQueues(const char* stdCode, uint32_t uDate)
{
	bool bNeedCache = false;                                             // 是否需要缓存标志
	auto it = _ordque_cache.find(stdCode);                               // 查找订单队列缓存
	if (it == _ordque_cache.end())                                       // 如果缓存中不存在
		bNeedCache = true;                                               // 标记为需要缓存
	else                                                                  // 如果缓存中存在
	{
		auto& dataList = it->second;                                     // 获取缓存列表
		if (dataList._date != uDate)                                     // 如果日期不匹配
			bNeedCache = true;                                           // 标记为需要缓存
	}


	if (bNeedCache)                                                      // 如果需要缓存
	{
		bool hasData = false;                                            // 是否有数据标志
		if (_mode == "csv")                                              // 如果是CSV模式
		{
			WTSLogger::error("Cannot use stock level2 data in csv mode!");  // 记录错误日志（CSV模式不支持Level2数据）
			return false;                                                 // 返回失败
		}
		else                                                              // 如果不是CSV模式
		{
			hasData = cacheRawOrdQueFromBin(stdCode, stdCode, uDate);    // 从二进制文件缓存订单队列数据
		}

		if (!hasData)                                                    // 如果没有数据
		{
			auto& dataList = _trans_cache[stdCode];                     // 获取逐笔成交缓存（注意：这里可能是bug，应该是_ordque_cache）
			dataList._items.resize(0);                                   // 清空数据项
			dataList._cursor = UINT_MAX;                                  // 重置游标
			dataList._code = stdCode;                                     // 设置合约代码
			dataList._date = uDate;                                      // 设置日期
			dataList._count = 0;                                          // 设置数量为0
			return false;                                                 // 返回失败
		}
	}

	return true;                                                         // 返回成功
}

/**
 * @brief 检查逐笔成交数据是否已缓存
 * 
 * 检查指定合约的逐笔成交数据是否已缓存到指定日期，如果未缓存则尝试加载
 * 
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存或已经缓存
 */
bool HisDataReplayer::checkTransactions(const char* stdCode, uint32_t uDate)
{
	bool bNeedCache = false;                                             // 是否需要缓存标志
	auto it = _trans_cache.find(stdCode);                                // 查找逐笔成交缓存
	if (it == _trans_cache.end())                                        // 如果缓存中不存在
		bNeedCache = true;                                               // 标记为需要缓存
	else                                                                  // 如果缓存中存在
	{
		auto& dataList = it->second;                                     // 获取缓存列表
		if (dataList._date != uDate)                                     // 如果日期不匹配
			bNeedCache = true;                                           // 标记为需要缓存
	}


	if (bNeedCache)                                                      // 如果需要缓存
	{
		bool hasData = false;                                            // 是否有数据标志
		if (_mode == "csv")                                              // 如果是CSV模式
		{
			WTSLogger::error("Cannot use stock level2 data in csv mode!");  // 记录错误日志（CSV模式不支持Level2数据）
			return false;                                                 // 返回失败
		}
		else                                                              // 如果不是CSV模式
		{
			hasData = cacheRawTransFromBin(stdCode, stdCode, uDate);     // 从二进制文件缓存逐笔成交数据
		}

		if (!hasData)                                                    // 如果没有数据
		{
			auto& dataList = _trans_cache[stdCode];                     // 获取逐笔成交缓存
			dataList._items.resize(0);                                   // 清空数据项
			dataList._cursor = UINT_MAX;                                  // 重置游标
			dataList._code = stdCode;                                     // 设置合约代码
			dataList._date = uDate;                                      // 设置日期
			dataList._count = 0;                                          // 设置数量为0
			return false;                                                 // 返回失败
		}
	}

	return true;                                                         // 返回成功
}

/**
 * @brief 检查Tick数据是否已缓存
 * 
 * 检查指定合约的Tick数据是否已缓存到指定日期，如果未缓存则尝试加载
 * 
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存或已经缓存
 */
bool HisDataReplayer::checkTicks(const char* stdCode, uint32_t uDate)
{
	if (strlen(stdCode) == 0)                                            // 如果合约代码为空
		return false;                                                     // 返回失败

	bool bNeedCache = false;                                             // 是否需要缓存标志
	auto it = _ticks_cache.find(stdCode);                                // 查找Tick缓存
	if (it == _ticks_cache.end())                                        // 如果缓存中不存在
		bNeedCache = true;                                               // 标记为需要缓存
	else                                                                  // 如果缓存中存在
	{
		HftDataList<WTSTickStruct>& tickList = (HftDataList<WTSTickStruct>&)it->second;  // 获取Tick缓存列表
		if (tickList._date != uDate)                                     // 如果日期不匹配
		{
			tickList._items.clear();                                      // 清空数据项
			bNeedCache = true;                                           // 标记为需要缓存
		}
		else if (tickList._count == 0)                                    // 如果数量为0
			return false;                                                 // 返回失败
	}
	
	if (bNeedCache)                                                      // 如果需要缓存
	{
		bool hasTicks = false;                                           // 是否有Tick数据标志
		if (NULL != _bt_loader)                                          // 如果外部数据加载器存在
		{
			hasTicks = cacheRawTicksFromLoader(stdCode, stdCode, uDate);  // 从外部加载器缓存Tick数据
		}
		else if (_mode == "csv")                                         // 如果是CSV模式
		{
			hasTicks = cacheRawTicksFromCSV(stdCode, stdCode, uDate);    // 从CSV文件缓存Tick数据
		}
		else                                                              // 如果不是CSV模式
		{
			hasTicks = cacheRawTicksFromBin(stdCode, stdCode, uDate);     // 从二进制文件缓存Tick数据
		}

		if (!hasTicks)                                                   // 如果没有Tick数据
		{
			auto& ticksList = _ticks_cache[stdCode];                    // 获取Tick缓存列表
			ticksList._items.resize(0);                                   // 清空数据项
			ticksList._cursor = UINT_MAX;                                 // 重置游标
			ticksList._code = stdCode;                                    // 设置合约代码
			ticksList._date = uDate;                                     // 设置日期
			ticksList._count = 0;                                         // 设置数量为0
			return false;                                                 // 返回失败
		}
	}


	return true;                                                         // 返回成功
}

/**
 * @brief 获取最后一个Tick数据
 * 
 * 获取指定合约在当前交易日的最后一个Tick数据
 * 
 * @param stdCode 合约代码
 * @return Tick数据指针，如果不存在则返回NULL
 */
WTSTickData* HisDataReplayer::get_last_tick(const char* stdCode)
{
	if (!checkTicks(stdCode, _cur_tdate))                                 // 如果Tick数据未缓存或检查失败
		return NULL;                                                      // 返回NULL

	auto& tickList = _ticks_cache[stdCode];                             // 获取Tick缓存列表
	if (tickList._cursor == 0)                                            // 如果游标为0（没有数据）
		return NULL;                                                      // 返回NULL

	if (tickList._cursor == UINT_MAX)                                     // 如果游标未初始化
	{
		uint32_t uDate = _cur_date;                                       // 当前日期
		uint32_t uTime = _cur_time * 100000 + _cur_secs;                  // 当前时间（转换为微秒）

		WTSTickStruct curTick;                                            // 创建临时Tick结构体用于查找
		curTick.action_date = uDate;                                      // 设置日期
		curTick.action_time = uTime;                                      // 设置时间

		auto tit = std::lower_bound(tickList._items.begin(), tickList._items.end(), curTick, [](const WTSTickStruct& a, const WTSTickStruct& b){  // 使用二分查找定位位置
			if (a.action_date != b.action_date)                            // 如果日期不同
				return a.action_date < b.action_date;                     // 按日期比较
			else                                                            // 如果日期相同
				return a.action_time < b.action_time;                     // 按时间比较
		});

		std::size_t idx = tit - tickList._items.begin();                 // 计算索引位置
		tickList._cursor = idx + 1;                                       // 设置游标（指向下一个Tick）
	}
	else if (tickList._cursor > tickList._count)                          // 如果游标超出数量
		return NULL;                                                      // 返回NULL

	return WTSTickData::create(tickList._items[tickList._cursor - 1]);   // 创建并返回最后一个Tick数据对象
}

/**
 * @brief 获取合约信息
 * 
 * 根据合约代码获取合约信息对象
 * 
 * @param stdCode 合约代码
 * @return 合约信息指针
 */
WTSCommodityInfo* HisDataReplayer::get_commodity_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息
	return _bd_mgr.getCommodity(codeInfo._exchg, codeInfo._product);     // 获取合约信息
}

/**
 * @brief 获取原始合约代码
 * 
 * 对于有自定义规则的合约（如主力合约），获取当前交易日的原始合约代码
 * 
 * @param stdCode 标准化合约代码
 * @return 原始合约代码，如果没有规则则返回空字符串
 */
std::string HisDataReplayer::get_rawcode(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息
	if(cInfo.hasRule())                                                    // 如果有自定义规则
	{
		std::string code = _hot_mgr.getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);  // 获取自定义原始合约代码
		return CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 转换为标准化合约代码
	}

	return "";                                                             // 返回空字符串
}

/**
 * @brief 获取交易时段信息
 * 
 * 根据交易时段ID或合约代码获取交易时段信息
 * 
 * @param sid 交易时段ID或合约代码
 * @param isCode 是否是合约代码（默认false，表示是交易时段ID）
 * @return 交易时段信息指针
 */
WTSSessionInfo* HisDataReplayer::get_session_info(const char* sid, bool isCode /* = false */)
{
	if (!isCode)                                                          // 如果不是合约代码
		return _bd_mgr.getSession(sid);                                   // 直接通过ID获取交易时段信息

	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(sid, &_hot_mgr);  // 提取合约代码信息
	WTSCommodityInfo* cInfo = _bd_mgr.getCommodity(codeInfo._exchg, codeInfo._product);  // 获取合约信息
	if (cInfo == NULL)                                                    // 如果合约信息不存在
		return NULL;                                                      // 返回NULL

	return cInfo->getSessionInfo();                                       // 返回合约的交易时段信息
}

/**
 * @brief 加载手续费配置
 * 
 * 从JSON格式的手续费配置文件中加载所有合约的手续费信息
 * 
 * @param filename 手续费配置文件路径
 */
void HisDataReplayer::loadFees(const char* filename)
{
	if (strlen(filename) == 0)                                          // 如果文件名为空
		return;                                                          // 直接返回

	if (!StdFile::exists(filename))                                     // 如果文件不存在
	{
		WTSLogger::error("Fees template file {} not exists", filename);  // 记录错误日志
		return;                                                          // 直接返回
	}

	WTSVariant* cfg = WTSCfgLoader::load_from_file(filename);           // 从文件加载配置
	if (cfg == NULL)                                                    // 如果加载失败
	{
		WTSLogger::error("Converting fees template file {} failed", filename);  // 记录错误日志
		return;                                                          // 直接返回
	}

	auto keys = cfg->memberNames();                                     // 获取所有合约代码
	for (const std::string& key : keys)                                 // 遍历所有合约代码
	{
		WTSVariant* cfgItem = cfg->get(key.c_str());                    // 获取合约手续费配置
		FeeItem& fItem = _fee_map[key];                                 // 获取或创建手续费项
		fItem._by_volume = cfgItem->getBoolean("byvolume");             // 读取是否按手数收费
		fItem._open = cfgItem->getDouble("open");                       // 读取开仓手续费
		fItem._close = cfgItem->getDouble("close");                      // 读取平仓手续费
		fItem._close_today = cfgItem->getDouble("closetoday");          // 读取平今手续费
	}

	cfg->release();                                                     // 释放配置对象

	WTSLogger::info("{} items of fees template loaded", _fee_map.size());  // 记录日志
}


/**
 * @brief 计算手续费
 * 
 * 根据合约代码、价格、数量和开平仓标志计算手续费
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param offset 开平仓标志（0-开仓，1-平昨，2-平今）
 * @return 手续费金额
 */
double HisDataReplayer::calc_fee(const char* stdCode, double price, double qty, uint32_t offset)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息
	const char* stdPID = codeInfo.stdCommID();                          // 获取标准化产品ID
	auto it = _fee_map.find(stdPID);                                     // 查找手续费配置
	if (it == _fee_map.end())                                            // 如果未找到
		return 0.0;                                                      // 返回0（无手续费）

	double ret = 0.0;                                                    // 手续费金额
	WTSCommodityInfo* commInfo = _bd_mgr.getCommodity(stdPID);          // 获取合约信息
	const FeeItem& fItem = it->second;                                  // 获取手续费项
	if (fItem._by_volume)                                                // 如果按手数收费
	{
		switch (offset)                                                   // 根据开平仓标志
		{
		case 0: ret = fItem._open*qty; break;                             // 开仓：开仓手续费 * 数量
		case 1: ret = fItem._close*qty; break;                           // 平昨：平仓手续费 * 数量
		case 2: ret = fItem._close_today*qty; break;                     // 平今：平今手续费 * 数量
		default: ret = 0.0; break;                                        // 默认：无手续费
		}
	}
	else                                                                   // 如果按金额收费
	{
		double amount = price*qty*commInfo->getVolScale();              // 计算成交金额（价格 * 数量 * 合约乘数）
		switch (offset)                                                    // 根据开平仓标志
		{
		case 0: ret = fItem._open*amount; break;                         // 开仓：开仓手续费率 * 成交金额
		case 1: ret = fItem._close*amount; break;                        // 平昨：平仓手续费率 * 成交金额
		case 2: ret = fItem._close_today*amount; break;                  // 平今：平今手续费率 * 成交金额
		default: ret = 0.0; break;                                        // 默认：无手续费
		}
	}

	return (int32_t)(ret * 100 + 0.5) / 100.0;                           // 四舍五入到分（保留两位小数）
}

/**
 * @brief 获取当前价格
 * 
 * 获取指定合约的当前价格（最新Tick价格）
 * 
 * @param stdCode 合约代码
 * @return 当前价格，如果不存在则返回0.0
 */
double HisDataReplayer::get_cur_price(const char* stdCode)
{
	auto it = _price_map.find(stdCode);                                   // 查找价格映射
	if (it == _price_map.end())                                           // 如果未找到
	{
		return 0.0;                                                       // 返回0.0
	}
	else                                                                   // 如果找到了
	{
		return it->second;                                                 // 返回价格
	}
}

/**
 * @brief 获取当日价格
 * 
 * 获取指定合约的当日价格（开盘价、最高价、最低价或最新价）
 * 
 * @param stdCode 合约代码
 * @param flag 价格标志（0-开盘价，1-最高价，2-最低价，3-最新价，默认0）
 * @return 价格，如果不存在则返回0.0
 */
double HisDataReplayer::get_day_price(const char* stdCode, int flag /* = 0 */)
{
	if(_tick_enabled)                                                     // 如果Tick回放已启用
	{
		WTSTickData* lastTick = get_last_tick(stdCode);                  // 获取最后一个Tick数据
		if(lastTick != NULL)                                              // 如果存在
		{
			const WTSTickStruct& curTs = lastTick->getTickStruct();      // 获取Tick结构体

			double price = 0.0;                                            // 价格
			switch (flag)                                                  // 根据标志选择价格
			{
			case 0:                                                        // 开盘价
				price = curTs.open;
				break;
			case 1:                                                        // 最高价
				price = curTs.high;
				break;
			case 2:                                                        // 最低价
				price = curTs.low;
				break;
			case 3:                                                        // 最新价
				price = curTs.price;
				break;
			default:                                                       // 默认
				break;
			}

			lastTick->release();                                           // 释放Tick数据对象

			return price;                                                  // 返回价格
		}
	}

	auto it = _day_cache.find(stdCode);                                   // 查找每日Tick缓存
	if (it == _day_cache.end())                                           // 如果未找到
		return 0.0;                                                       // 返回0.0

	const WTSTickStruct& curTs = it->second;                             // 获取Tick结构体
	double price = 0.0;                                                    // 价格
	switch (flag)                                                          // 根据标志选择价格
	{
	case 0:                                                                // 开盘价
		return curTs.open;
	case 1:                                                                // 最高价
		return curTs.high;
	case 2:                                                                // 最低价
		return curTs.low;
	case 3:                                                                // 最新价
		return curTs.price;
	default:                                                               // 默认
		return 0.0;
	}
}

/**
 * @brief 订阅Tick数据
 * 
 * 订阅指定合约的Tick数据，支持复权数据的订阅
 * 
 * @param sid 订阅ID
 * @param stdCode 合约代码（可能包含复权标记）
 */
void HisDataReplayer::sub_tick(uint32_t sid, const char* stdCode)
{
	if (strlen(stdCode) == 0)                                            // 如果合约代码为空
		return;                                                          // 直接返回

	std::size_t length = strlen(stdCode);                                // 获取合约代码长度
	uint32_t flag = 0;                                                   // 复权标志（0-不复权，1-前复权，2-后复权）
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权标记
	{
		length--;                                                         // 长度减1（去掉复权标记）

		flag = (stdCode[length - 1] == SUFFIX_QFQ) ? 1 : 2;             // 设置复权标志（1-前复权，2-后复权）
	}

	std::string hitCode(stdCode, length);                                // 构造真实合约代码（去掉复权标记）
	SubList& sids = _tick_sub_map[hitCode];                             // 获取订阅列表
	sids[sid] = std::make_pair(sid, flag);                               // 添加订阅项（订阅ID和复权标志）

	if (_tick_enabled)                                                   // 如果Tick回放已启用
		return;                                                          // 直接返回（不需要额外处理）

	_unsubbed_in_need.insert(hitCode);                                   // 添加到未订阅但需要的集合中
}

/**
 * @brief 订阅订单明细数据
 * 
 * 订阅指定合约的订单明细数据，支持复权数据的订阅
 * 
 * @param sid 订阅ID
 * @param stdCode 合约代码（可能包含复权标记）
 */
void HisDataReplayer::sub_order_detail(uint32_t sid, const char* stdCode)
{
	if (strlen(stdCode) == 0)                                            // 如果合约代码为空
		return;                                                          // 直接返回

	std::size_t length = strlen(stdCode);                                // 获取合约代码长度
	uint32_t flag = 0;                                                   // 复权标志（0-不复权，1-前复权，2-后复权）
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权标记
	{
		length--;                                                         // 长度减1（去掉复权标记）

		flag = (stdCode[length - 1] == SUFFIX_QFQ) ? 1 : 2;             // 设置复权标志（1-前复权，2-后复权）
	}

	SubList& sids = _orddtl_sub_map[std::string(stdCode, length)];      // 获取订阅列表（构造真实合约代码）
	sids[sid] = std::make_pair(sid, flag);                               // 添加订阅项（订阅ID和复权标志）
}

/**
 * @brief 订阅订单队列数据
 * 
 * 订阅指定合约的订单队列数据，支持复权数据的订阅
 * 
 * @param sid 订阅ID
 * @param stdCode 合约代码（可能包含复权标记）
 */
void HisDataReplayer::sub_order_queue(uint32_t sid, const char* stdCode)
{
	if (strlen(stdCode) == 0)                                            // 如果合约代码为空
		return;                                                          // 直接返回

	std::size_t length = strlen(stdCode);                                // 获取合约代码长度
	uint32_t flag = 0;                                                   // 复权标志（0-不复权，1-前复权，2-后复权）
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权标记
	{
		length--;                                                         // 长度减1（去掉复权标记）

		flag = (stdCode[length - 1] == SUFFIX_QFQ) ? 1 : 2;             // 设置复权标志（1-前复权，2-后复权）
	}

	SubList& sids = _ordque_sub_map[std::string(stdCode, length)];      // 获取订阅列表（构造真实合约代码）
	sids[sid] = std::make_pair(sid, flag);                               // 添加订阅项（订阅ID和复权标志）
}

/**
 * @brief 订阅逐笔成交数据
 * 
 * 订阅指定合约的逐笔成交数据，支持复权数据的订阅
 * 
 * @param sid 订阅ID
 * @param stdCode 合约代码（可能包含复权标记）
 */
void HisDataReplayer::sub_transaction(uint32_t sid, const char* stdCode)
{
	if (strlen(stdCode) == 0)                                            // 如果合约代码为空
		return;                                                          // 直接返回

	std::size_t length = strlen(stdCode);                                // 获取合约代码长度
	uint32_t flag = 0;                                                   // 复权标志（0-不复权，1-前复权，2-后复权）
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权标记
	{
		length--;                                                         // 长度减1（去掉复权标记）

		flag = (stdCode[length - 1] == SUFFIX_QFQ) ? 1 : 2;             // 设置复权标志（1-前复权，2-后复权）
	}

	SubList& sids = _trans_sub_map[std::string(stdCode, length)];      // 获取订阅列表（构造真实合约代码）
	sids[sid] = std::make_pair(sid, flag);                               // 添加订阅项（订阅ID和复权标志）
}

/**
 * @brief 检查并加载未订阅的K线数据
 * 
 * 对于未订阅但需要的合约，自动加载主K线周期的数据，用于Tick模拟
 */
void HisDataReplayer::checkUnbars()
{
	for(const std::string& stdCode : _unsubbed_in_need)                  // 遍历未订阅但需要的合约代码集合
	{
		//先检查是否已经在未订阅K线中
		bool bHasBars = _codes_in_unsubbed.find(stdCode) != _codes_in_unsubbed.end();  // 检查是否已在未订阅K线集合中
		if(bHasBars)                                                     // 如果已存在
			continue;                                                    // 跳过该合约

		//再检查是否在已订阅K线中
		bHasBars = _codes_in_subbed.find(stdCode) != _codes_in_subbed.end();  // 检查是否已在已订阅K线集合中
		if (bHasBars)                                                    // 如果已存在
			continue;                                                    // 跳过该合约

		//如果订阅了tick,但是没有对应的K线数据,则自动加载主K线周期的数据
		bool bHasHisData = false;                                         // 是否有历史数据标志
		std::string key = fmt::format("{}#{}", stdCode, _main_period);  // 构造缓存键（合约代码#主周期）

		WTSKlinePeriod kp;                                               // K线周期枚举
		uint32_t realTimes = strtoul(_main_period.c_str() + 2, NULL, 10);  // 提取倍数（从第3个字符开始）
		if (_main_period[0] == 'm')                                      // 如果是分钟线
		{
			if (realTimes % 5 == 0)
			{
				kp = KP_Minute5;
			}
			else
			{
				kp = KP_Minute1;
			}
		}
		else
			kp = KP_DAY;

		/*
		 *	By Wesley @ 2021.12.20
		 *	先从extloader加载最终的K线数据
		 *	如果加载不到，再从配置的历史数据存储引擎加载数据
		 */
		if (NULL != _bt_loader)                                           // 如果外部数据加载器存在
		{
			bHasHisData = cacheFinalBarsFromLoader(key, stdCode.c_str(), kp, false);  // 从外部加载器缓存K线数据
		}

		if(!bHasHisData)                                                  // 如果外部加载器未加载到数据
		{
			if (_mode == "csv")                                          // 如果是CSV模式
			{
				bHasHisData = cacheRawBarsFromCSV(key, stdCode.c_str(), kp, false);  // 从CSV文件缓存K线数据
			}
			else                                                           // 如果不是CSV模式
			{
				bHasHisData = cacheRawBarsFromBin(key, stdCode.c_str(), kp, false);  // 从二进制文件缓存K线数据
			}
		}
		
		if (!bHasHisData)                                                 // 如果没有历史数据
			continue;                                                     // 跳过该合约

		WTSSessionInfo* sInfo = get_session_info(stdCode.c_str(), true);  // 获取交易时段信息

		BarsListPtr& kBlkPair = _unbars_cache[key];                     // 获取未订阅K线缓存

		_codes_in_unsubbed.insert(stdCode);                              // 添加到未订阅K线集合中
		
		//还没有经过初始定位
		WTSBarStruct bar;                                                 // 创建临时K线结构体用于查找
		bar.date = _cur_tdate;                                            // 设置日期为当前交易日期
		bar.time = (_cur_date - 19900000) * 10000 + _cur_time;            // 设置时间（转换为从1990年1月1日开始的秒数）

		auto it = std::lower_bound(kBlkPair->_bars.begin(), kBlkPair->_bars.end(), bar, [](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位位置
			return a.time < b.time;                                       // 按时间比较
		});

		std::size_t eIdx = it - kBlkPair->_bars.begin();                 // 计算索引位置

		if (it != kBlkPair->_bars.end())                                  // 如果找到了
		{
			WTSBarStruct& curBar = *it;                                    // 获取当前K线
			if (curBar.time > bar.time)                                   // 如果当前K线时间大于目标时间
			{
				if (eIdx > 0)                                             // 如果索引大于0
				{
					it--;                                                  // 向前移动一位
					eIdx--;                                                // 索引减1
				}
			}

			kBlkPair->_cursor = eIdx + 1;                                  // 设置游标（指向下一个K线）
		}
	}
}

/**
 * @brief 字符串转换为时间
 * 
 * 将时间字符串（如"09:30:00"或"0930"）转换为整数时间（HHMM格式）
 * 
 * @param strTime 时间字符串
 * @param bHasSec 是否包含秒数（默认false）
 * @return 时间整数（HHMM格式）
 */
uint32_t strToTime(const char* strTime, bool bHasSec = false)
{
	std::string str;                                                      // 结果字符串
	const char *pos = strTime;                                            // 当前位置指针
	while (strlen(pos) > 0)                                               // 当还有字符时
	{
		if (pos[0] != ':')                                                // 如果不是冒号
		{
			str.append(pos, 1);                                           // 追加字符
		}
		pos++;                                                             // 指针向前移动
	}

	uint32_t ret = strtoul(str.c_str(), NULL, 10);                      // 转换为无符号整数
	if (str.size() > 4 && !bHasSec)                                      // 如果字符串长度大于4且不包含秒数
		ret /= 100;                                                       // 除以100（去掉秒数部分）

	return ret;                                                           // 返回时间整数
}

/**
 * @brief 字符串转换为日期
 * 
 * 将日期字符串（如"2023/01/01"或"2023-01-01"）转换为整数日期（YYYYMMDD格式）
 * 
 * @param strDate 日期字符串
 * @return 日期整数（YYYYMMDD格式）
 */
uint32_t strToDate(const char* strDate)
{
	StringVector ay = StrUtil::split(strDate, "/");                      // 使用"/"分割字符串
	if(ay.size() == 1)                                                   // 如果分割结果只有一个元素
		ay = StrUtil::split(strDate, "-");                               // 使用"-"分割字符串
	std::stringstream ss;                                                 // 字符串流
	if (ay.size() > 1)                                                   // 如果分割结果有多个元素
	{
		auto pos = ay[2].find(" ");                                       // 查找第三个元素中的空格位置
		if (pos != std::string::npos)                                     // 如果找到空格
			ay[2] = ay[2].substr(0, pos);                                // 截取空格之前的部分（去掉时间部分）
		ss << ay[0] << (ay[1].size() == 1 ? "0" : "") << ay[1] << (ay[2].size() == 1 ? "0" : "") << ay[2];  // 格式化日期（年+月+日，月份和日期不足两位时补0）
	}
	else                                                                   // 如果分割结果只有一个元素
		ss << ay[0];                                                      // 直接输出该元素

	return strtoul(ss.str().c_str(), NULL, 10);                         // 转换为无符号整数并返回
}

/**
 * @brief 从二进制文件缓存原始Tick数据
 * 
 * 从二进制文件中加载指定合约指定日期的Tick数据到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawTicksFromBin(const std::string& key, const char* stdCode, uint32_t uDate)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);  // 构造标准化产品ID（交易所.品种）
	
	std::string rawCode = cInfo._code;                                    // 原始合约代码
	if(strlen(cInfo._ruletag) > 0)                                        // 如果有自定义规则标签
	{
		rawCode = _hot_mgr.getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), uDate);  // 获取自定义原始合约代码
	}


	std::string content;                                                  // 文件内容
	bool bHit = false;                                                    // 是否命中标志
	//先检查有没有HOT、SND的主力次主力的tick文件
	const char* ruleTag = cInfo._ruletag;                                 // 规则标签
	if(strlen(ruleTag) > 0)                                               // 如果有规则标签
	{
		const char* hot_flag = ruleTag;                                    // 规则标签（如HOT、SND）
		std::string wrappCode = StrUtil::printf("%s_%s", cInfo._product, hot_flag);  // 构造包装代码（品种_规则标签）
		bHit = _his_dt_mgr.load_raw_ticks(cInfo._exchg, wrappCode.c_str(), uDate, [&content](std::string& data) {  // 尝试加载主力/次主力合约的Tick数据
			content.swap(data);                                            // 交换数据内容
		});
	}

	//如果没有找到，则读取分月合约
	if (!bHit)                                                             // 如果未找到主力合约数据
	{
		/*
		 *	By Wesley @ 2022.01.11
		 *	这里将直接从文件读取，改成从HisDtMgr封装的接口加载
		 */
		bHit = _his_dt_mgr.load_raw_ticks(cInfo._exchg, rawCode.c_str(), uDate, [&content](std::string& data) {  // 加载分月合约的Tick数据
			content.swap(data);                                            // 交换数据内容
		});
	}

	if(!bHit)                                                              // 如果仍未找到数据
	{
		WTSLogger::warn("No ticks data of {} on {} found", stdCode, uDate);  // 记录警告日志
		return false;                                                      // 返回失败
	}

	auto& ticksList = _ticks_cache[key];                                 // 获取Tick缓存列表
	uint32_t tickcnt = 0;                                                  // Tick数量
	tickcnt = content.size() / sizeof(WTSTickStruct);                     // 计算Tick数量（文件大小除以结构体大小）
	ticksList._items.resize(tickcnt);                                      // 调整缓存列表大小
	memcpy(ticksList._items.data(), content.data(), content.size());      // 复制数据到缓存列表
	
	ticksList._cursor = UINT_MAX;                                          // 重置游标（未初始化）
	ticksList._code = stdCode;                                             // 设置合约代码
	ticksList._date = uDate;                                               // 设置日期
	ticksList._count = tickcnt;                                            // 设置数量

	return true;                                                           // 返回成功
}

/**
 * @brief 从二进制文件缓存原始订单明细数据
 * 
 * 从二进制文件中加载指定合约指定日期的订单明细数据到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawOrdDtlFromBin(const std::string& key, const char* stdCode, uint32_t uDate)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息

	std::string content;                                                  // 文件内容
	bool bHit = _his_dt_mgr.load_raw_orddtl(cInfo._exchg, cInfo._code, uDate, [&content](std::string& data) {  // 加载订单明细数据
		content.swap(data);                                                // 交换数据内容
	});

	if (!bHit)                                                             // 如果未找到数据
	{
		WTSLogger::warn("No order detail data of {} on {} found", stdCode, uDate);  // 记录警告日志
		return false;                                                      // 返回失败
	}

	auto& dataList = _orddtl_cache[key];                                // 获取订单明细缓存列表
	uint32_t dataCnt = 0;                                                  // 数据数量
	dataCnt = content.size() / sizeof(WTSOrdDtlStruct);                   // 计算数据数量（文件大小除以结构体大小）
	dataList._items.resize(dataCnt);                                       // 调整缓存列表大小
	memcpy(dataList._items.data(), content.data(), content.size());       // 复制数据到缓存列表

	dataList._cursor = UINT_MAX;                                           // 重置游标（未初始化）
	dataList._code = stdCode;                                              // 设置合约代码
	dataList._date = uDate;                                                // 设置日期
	dataList._count = dataCnt;                                             // 设置数量

	return true;                                                           // 返回成功
}

/**
 * @brief 从二进制文件缓存原始订单队列数据
 * 
 * 从二进制文件中加载指定合约指定日期的订单队列数据到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawOrdQueFromBin(const std::string& key, const char* stdCode, uint32_t uDate)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息

	std::string content;                                                  // 文件内容
	bool bHit = _his_dt_mgr.load_raw_ordque(cInfo._exchg, cInfo._code, uDate, [&content](std::string& data) {  // 加载订单队列数据
		content.swap(data);                                                // 交换数据内容
	});

	if (!bHit)                                                             // 如果未找到数据
	{
		WTSLogger::warn("No order queue data of {} on {} found", stdCode, uDate);  // 记录警告日志
		return false;                                                      // 返回失败
	}

	auto& dataList = _ordque_cache[key];                                // 获取订单队列缓存列表
	uint32_t dataCnt = 0;                                                  // 数据数量
	dataCnt = content.size() / sizeof(WTSOrdQueStruct);                  // 计算数据数量（文件大小除以结构体大小）
	dataList._items.resize(dataCnt);                                       // 调整缓存列表大小
	memcpy(dataList._items.data(), content.data(), content.size());       // 复制数据到缓存列表

	dataList._cursor = UINT_MAX;                                           // 重置游标（未初始化）
	dataList._code = stdCode;                                              // 设置合约代码
	dataList._date = uDate;                                                // 设置日期
	dataList._count = dataCnt;                                             // 设置数量

	return true;                                                           // 返回成功
}

/**
 * @brief 从二进制文件缓存原始逐笔成交数据
 * 
 * 从二进制文件中加载指定合约指定日期的逐笔成交数据到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawTransFromBin(const std::string& key, const char* stdCode, uint32_t uDate)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息

	std::string content;                                                  // 文件内容
	bool bHit = _his_dt_mgr.load_raw_trans(cInfo._exchg, cInfo._code, uDate, [&content](std::string& data) {  // 加载逐笔成交数据
		content.swap(data);                                                // 交换数据内容
	});

	if (!bHit)                                                             // 如果未找到数据
	{
		WTSLogger::warn("No transaction data of {} on {} found", stdCode, uDate);  // 记录警告日志
		return false;                                                      // 返回失败
	}

	auto& dataList = _trans_cache[key];                                 // 获取逐笔成交缓存列表
	uint32_t dataCnt = 0;                                                  // 数据数量
	dataCnt = content.size() / sizeof(WTSTransStruct);                    // 计算数据数量（文件大小除以结构体大小）
	dataList._items.resize(dataCnt);                                       // 调整缓存列表大小
	memcpy(dataList._items.data(), content.data(), content.size());       // 复制数据到缓存列表

	dataList._cursor = UINT_MAX;                                           // 重置游标（未初始化）
	dataList._code = stdCode;                                              // 设置合约代码
	dataList._date = uDate;                                                // 设置日期
	dataList._count = dataCnt;                                             // 设置数量

	return true;                                                           // 返回成功
}

/**
 * @brief 从外部加载器缓存原始Tick数据
 * 
 * 通过外部数据加载器（IBtDataLoader）加载指定合约指定日期的Tick数据到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawTicksFromLoader(const std::string& key, const char* stdCode, uint32_t uDate)
{
	if (NULL == _bt_loader)                                                // 如果外部数据加载器不存在
		return false;                                                      // 返回失败

	auto& dataList = _ticks_cache[key];                                 // 获取Tick缓存列表
	dataList._cursor = UINT_MAX;                                           // 重置游标（未初始化）
	dataList._code = stdCode;                                              // 设置合约代码
	dataList._date = uDate;                                                // 设置日期
	dataList._count = 0;                                                   // 设置数量为0

	bool bSucc = _bt_loader->loadRawHisTicks(&dataList, stdCode, uDate, [](void* obj, WTSTickStruct* firstItem, uint32_t count) {  // 通过外部加载器加载Tick数据
		HftDataList<WTSTickStruct>* ticks = (HftDataList<WTSTickStruct>*)obj;  // 转换为Tick缓存列表指针
		ticks->_items.resize(count);                                       // 调整缓存列表大小
		ticks->_count = count;                                             // 设置数量
		memcpy(ticks->_items.data(), firstItem, sizeof(WTSTickStruct)*count);  // 复制数据到缓存列表
	});

	if (!bSucc)                                                            // 如果加载失败
		return false;                                                      // 返回失败

	if (dataList._count > 0)                                              // 如果数据数量大于0
		WTSLogger::info("{} items of back tick data of {} on {} loaded via extended loader", dataList._count, stdCode, uDate);  // 记录信息日志

	return true;                                                           // 返回成功
}

/**
 * @brief 从CSV文件缓存原始Tick数据
 * 
 * 从CSV格式的文件中加载指定合约指定日期的Tick数据到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param uDate 交易日期
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawTicksFromCSV(const std::string& key, const char* stdCode, uint32_t uDate)
{
	if (strlen(stdCode) == 0)                                              // 如果合约代码为空
		return false;                                                      // 返回失败

	std::stringstream ss;                                                  // 字符串流
	ss << _base_dir << "bin/ticks/";                                       // 构造Tick数据目录路径
	std::string path = ss.str();                                           // 获取路径字符串
	if (!StdFile::exists(path.c_str()))                                    // 如果目录不存在
		boost::filesystem::create_directories(path.c_str());              // 创建目录
	ss << stdCode << "_tick_" << uDate << ".dsb";                         // 构造文件名（合约代码_tick_日期.dsb）
	std::string filename = ss.str();                                       // 获取完整文件名
	if (StdFile::exists(filename.c_str()))                                 // 如果文件存在
	{
		//如果有格式化的历史数据文件, 则直接读取
		WTSLogger::info("Reading data from {}...", filename);            // 记录信息日志
		std::string content;                                               // 文件内容
		StdFile::read_file_content(filename.c_str(), content);            // 读取文件内容
		if (content.size() < sizeof(HisTickBlockV2))                      // 如果文件大小小于块头大小
		{
			WTSLogger::error("Sizechecking of back tick data file {} failed", filename);  // 记录错误日志
			return false;                                                  // 返回失败
		}

		WTSLogger::info("Processing file content of {}...", filename);   // 记录信息日志
		proc_block_data(filename.c_str(), content, false, false);        // 处理块数据（解压缩或版本转换）
		uint32_t tickcnt = content.size() / sizeof(WTSTickStruct);        // 计算Tick数量（文件大小除以结构体大小）
		auto& ticksList = _ticks_cache[key];                             // 获取Tick缓存列表
		ticksList._items.resize(tickcnt);                                  // 调整缓存列表大小
		memcpy(ticksList._items.data(), content.data(), content.size());   // 复制数据到缓存列表
		ticksList._cursor = UINT_MAX;                                      // 重置游标（未初始化）
		ticksList._code = stdCode;                                         // 设置合约代码
		ticksList._date = uDate;                                           // 设置日期
		ticksList._count = tickcnt;                                        // 设置数量
	}
	else                                                                // 如果文件不存在
	{
		WTSLogger::error("Back tick data file {} not exists", filename.c_str());  // 记录错误日志
		WTSLogger::warn("If you want to use tick data in csv mode, you can use wtpy.WtDataHelper.store_ticks to generate dsb file", filename.c_str());  // 记录警告日志（提示使用wtpy工具生成dsb文件）
		return false;                                                      // 返回失败

		/*
		 *	By Wesley @ 2023.05.18
		 *	回测的tick数据不再支持从csv读取，因为tick数据维度更多，有处理csv的时间，直接生成dsb了
		 */
		//如果没有格式化的历史数据文件, 则从csv加载
		//（以下代码已被注释，不再支持从CSV读取Tick数据）
	}
	return true;                                                          // 返回成功
}

/**
 * @brief 从外部加载器缓存最终K线数据
 * 
 * 通过外部数据加载器（IBtDataLoader）加载指定合约的最终K线数据（已处理复权等）到缓存
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param period K线周期
 * @param bSubbed 是否已订阅（默认true）
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheFinalBarsFromLoader(const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed /* = true */)
{
	if (NULL == _bt_loader)                                                // 如果外部数据加载器不存在
		return false;                                                      // 返回失败

	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息
	WTSCommodityInfo* commInfo = _bd_mgr.getCommodity(cInfo._exchg, cInfo._product);  // 获取合约信息
	const char* stdPID = cInfo.stdCommID();                                // 获取标准化产品ID

	std::string pname;                                                     // 周期名称（如m1、m5、d）
	std::string dirname;                                                   // 目录名称（如min1、min5、day）
	switch (period)                                                        // 根据周期设置名称
	{
	case KP_Minute1: pname = "m1"; dirname = "min1"; break;              // 1分钟线
	case KP_Minute5: pname = "m5"; dirname = "min5"; break;              // 5分钟线
	case KP_DAY: pname = "d"; dirname = "day"; break;                   // 日线
	default: pname = ""; break;                                           // 默认：空
	}

	bool isDay = (period == KP_DAY);                                       // 是否为日线

	std::stringstream ss;                                                  // 字符串流
	ss << _base_dir << "his/" << dirname << "/" << cInfo._exchg << "/";  // 构造K线数据目录路径
	if (!StdFile::exists(ss.str().c_str()))                                // 如果目录不存在
        boost::filesystem::create_directories(ss.str().c_str());           // 创建目录

	const char* ruleTag = cInfo._ruletag;                                 // 规则标签

	if (strlen(ruleTag) > 0)                                               // 如果有规则标签（如主力合约）
	{
		ss << cInfo._exchg << "." << cInfo._product << "_" << ruleTag;   // 构造文件名（交易所.品种_规则标签）
		if (cInfo.isExright())                                             // 如果是复权数据
			ss << (cInfo._exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ);       // 添加复权后缀（前复权或后复权）
		ss << ".dsb";                                                      // 添加文件扩展名
	}
	else if (cInfo.isExright() && commInfo->isStock())                   // 如果是股票复权数据
	{
		//复权数据，采用SSE.600000+.dsb这样的文件名
		ss << cInfo._exchg << "." << cInfo._code << (cInfo._exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ) << ".dsb";  // 构造文件名（交易所.代码+复权后缀.dsb）
	}
	else                                                                   // 如果是普通合约
		ss << cInfo._code << ".dsb";                                      // 构造文件名（代码.dsb）
	std::string filename = ss.str();                                       // 获取完整文件名

	bool bHit = false;                                                     // 是否命中标志
	if(_bt_loader->isAutoTrans() && StdFile::exists(filename.c_str()))    // 如果支持自动转储且文件存在
	{
		//如果支持自动转储，则先读取已经转储的dsb文件
		std::string content;                                               // 文件内容
		StdFile::read_file_content(filename.c_str(), content);            // 读取文件内容
		if (content.size() < sizeof(HisKlineBlockV2))                      // 如果文件大小小于块头大小
		{
			WTSLogger::error("Sizechecking of back kbar data file {} failed", filename.c_str());  // 记录错误日志
		}
		else                                                               // 如果文件大小正常
		{
			HisKlineBlockV2* kBlock = (HisKlineBlockV2*)content.c_str();  // 转换为块头指针
			std::string rawData = WTSCmpHelper::uncompress_data(kBlock->_data, kBlock->_size);  // 解压缩数据
			uint32_t barcnt = rawData.size() / sizeof(WTSBarStruct);      // 计算K线数量

			if (bSubbed)                                                   // 如果已订阅
				_bars_cache[key].reset(new BarsList);                    // 创建订阅K线缓存
			else                                                           // 如果未订阅
				_unbars_cache[key].reset(new BarsList);                 // 创建未订阅K线缓存

			BarsListPtr& barsList = bSubbed ? _bars_cache[key] : _unbars_cache[key];  // 获取K线列表指针
			barsList->_bars.resize(barcnt);                                // 调整缓存列表大小
			memcpy(barsList->_bars.data(), rawData.data(), rawData.size());  // 复制数据到缓存列表
			barsList->_cursor = UINT_MAX;                                  // 重置游标（未初始化）
			barsList->_code = stdCode;                                     // 设置合约代码
			barsList->_period = period;                                    // 设置周期
			barsList->_count = barcnt;                                      // 设置数量

			uint64_t stime = isDay ? barsList->_bars[0].date : barsList->_bars[0].time;  // 计算开始时间
			uint64_t etime = isDay ? barsList->_bars[barcnt - 1].date : barsList->_bars[barcnt - 1].time;  // 计算结束时间

			WTSLogger::info("{} items of back {} data of {} directly loaded from dsb file, from {} to {}", barcnt, pname.c_str(), stdCode, stime, etime);  // 记录信息日志
			bHit = true;                                                   // 标记为已命中
		}
	}

	if(!bHit)                                                              // 如果未从转储文件加载
	{
		//如果没有转储的历史数据文件, 则从csv加载
		WTSLogger::log_raw(LL_INFO, "Reading data via extended loader...");  // 记录信息日志

		if (bSubbed)                                                       // 如果已订阅
			_bars_cache[key].reset(new BarsList);                        // 创建订阅K线缓存
		else                                                               // 如果未订阅
			_unbars_cache[key].reset(new BarsList);                     // 创建未订阅K线缓存

		BarsListPtr& barsList = bSubbed ? _bars_cache[key] : _unbars_cache[key];  // 获取K线列表指针
		barsList->_code = stdCode;                                         // 设置合约代码
		barsList->_period = period;                                        // 设置周期
		barsList->_cursor = UINT_MAX;                                      // 重置游标（未初始化）
		barsList->_count = 0;                                              // 设置数量为0

		std::string buffer;                                                 // 缓冲区
		bool bSucc = _bt_loader->loadFinalHisBars(barsList.get(), stdCode, period, [](void* obj, WTSBarStruct* firstBar, uint32_t count) {  // 通过外部加载器加载最终K线数据
			BarsList* bars = (BarsList*)obj;                              // 转换为K线列表指针
			bars->_count = count;                                           // 设置数量
			bars->_bars.resize(count);                                      // 调整缓存列表大小
			memcpy((void*)bars->_bars.data(), firstBar, sizeof(WTSBarStruct)*count);  // 复制数据到缓存列表
		});

		if (!bSucc)                                                        // 如果加载失败
			return false;                                                  // 返回失败

		bool isDay = (period == KP_DAY);                                   // 是否为日线

		uint64_t stime = isDay ? barsList->_bars[0].date : barsList->_bars[0].time;  // 计算开始时间
		uint64_t etime = isDay ? barsList->_bars[barsList->_count - 1].date : barsList->_bars[barsList->_count - 1].time;  // 计算结束时间

		WTSLogger::info("{} items of back {} data of {} loaded via extended loader, from {} to {}", barsList->_count, pname.c_str(), stdCode, stime, etime);  // 记录信息日志

		if(_bt_loader->isAutoTrans())                                       // 如果支持自动转储
		{
			BlockType btype;                                                // 块类型
			switch (period)                                                 // 根据周期设置块类型
			{
			case KP_Minute1: btype = BT_HIS_Minute1; break;              // 1分钟线块类型
			case KP_Minute5: btype = BT_HIS_Minute5; break;              // 5分钟线块类型
			default: btype = BT_HIS_Day; break;                           // 默认：日线块类型
			}

			/*
			 *	By Wesley @ 2021.12.14
			 *	这一段之前有bug，之前没有把文件头写到文件里，所以转储的dsb解析的时候会抛出异常
			 */
			std::string content;                                             // 文件内容
			content.resize(sizeof(HisKlineBlockV2));                        // 调整大小为块头大小
			HisKlineBlockV2 *kBlock = (HisKlineBlockV2*)content.data();    // 转换为块头指针
			strcpy(kBlock->_blk_flag, BLK_FLAG);                            // 设置块标志
			kBlock->_type = btype;                                           // 设置块类型
			kBlock->_version = BLOCK_VERSION_CMP_V2;                        // 设置块版本

			std::string cmpData = WTSCmpHelper::compress_data(barsList->_bars.data(), sizeof(WTSBarStruct)*barsList->_count);  // 压缩K线数据
			kBlock->_size = cmpData.size();                                 // 设置压缩后数据大小
			content.append(cmpData);                                         // 追加压缩数据

			StdFile::write_file_content(filename.c_str(), content.c_str(), content.size());  // 写入文件
			WTSLogger::info("Bars transfered to file {}", filename);       // 记录信息日志
		}
	}

	return true;                                                           // 返回成功
}

/**
 * @brief 从CSV文件缓存原始K线数据
 * 
 * 从CSV格式的文件中加载指定合约的K线数据到缓存，如果文件不存在则尝试从CSV文件加载并转储为dsb格式
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param period K线周期
 * @param bSubbed 是否已订阅（默认true）
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawBarsFromCSV(const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed/* = true*/)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 提取合约代码信息
	WTSCommodityInfo* commInfo = _bd_mgr.getCommodity(cInfo._exchg, cInfo._product);  // 获取合约信息
	std::string stdPID = StrUtil::printf("%s.%s", cInfo._exchg, cInfo._product);  // 构造标准化产品ID

	std::string p_suffix;                                                   // 周期后缀（如m1、m5、d）
	std::string dirname = PERIOD_NAME[period];                              // 目录名称
	switch (period)                                                         // 根据周期设置后缀
	{
	case KP_Minute1: p_suffix = "m1"; break;                              // 1分钟线
	case KP_Minute5: p_suffix = "m5"; break;                              // 5分钟线
	case KP_DAY: p_suffix = "d"; break;                                   // 日线
	default: p_suffix = ""; break;                                         // 默认：空
	}

	bool isDay = (period == KP_DAY);                                        // 是否为日线

	std::stringstream ss;                                                   // 字符串流
	ss << _base_dir << "his/" << dirname << "/" << cInfo._exchg << "/";   // 构造K线数据目录路径

	//这里自动创建，是因为后面转储需要
	if (!StdFile::exists(ss.str().c_str()))                                 // 如果目录不存在
		boost::filesystem::create_directories(ss.str().c_str());           // 创建目录

	const char* ruleTag = cInfo._ruletag;                                  // 规则标签
	if (strlen(ruleTag) > 0 && commInfo->isFuture())                       // 如果有规则标签且是期货
	{
		if(cInfo.isExright())                                               // 如果是复权数据
		{
			ss << cInfo._exchg << "." << cInfo._product << "_" << ruleTag << (cInfo._exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ) << ".dsb";  // 构造文件名（交易所.品种_规则标签+复权后缀.dsb）
		}
		else                                                                 // 如果不是复权数据
			ss << cInfo._exchg << "." << cInfo._product << "_" << ruleTag << ".dsb";  // 构造文件名（交易所.品种_规则标签.dsb）
	}
	else if (cInfo.isExright() && commInfo->isStock())                     // 如果是股票复权数据
	{
		//复权数据，采用SSE.600000+.dsb这样的文件名
		ss << cInfo._exchg << "." << cInfo._code << (cInfo._exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ) << ".dsb";  // 构造文件名（交易所.代码+复权后缀.dsb）
	}
	else                                                                    // 如果是普通合约
		ss << cInfo._code << ".dsb";                                       // 构造文件名（代码.dsb）
	std::string filename = ss.str();                                        // 获取完整文件名
	if (StdFile::exists(filename.c_str()))                                 // 如果dsb文件存在
	{
		//如果有格式化的历史数据文件, 则直接读取
		std::string content;                                                // 文件内容
		StdFile::read_file_content(filename.c_str(), content);             // 读取文件内容
		if (content.size() < sizeof(HisKlineBlockV2))                       // 如果文件大小小于块头大小
		{
			WTSLogger::error("Sizechecking of back kbar data file {} failed", filename);  // 记录错误日志
			return false;                                                   // 返回失败
		}

		//By Wesley @ 2021.12.30
		//转储的数据不做检查，直接重新生成即可
		proc_block_data(filename.c_str(), content, true, false);           // 处理块数据（解压缩或版本转换）
		uint32_t barcnt = content.size() / sizeof(WTSBarStruct);           // 计算K线数量

		if (bSubbed)                                                        // 如果已订阅
			_bars_cache[key].reset(new BarsList);                         // 创建订阅K线缓存
		else                                                                // 如果未订阅
			_unbars_cache[key].reset(new BarsList);                        // 创建未订阅K线缓存

		BarsListPtr& barsList = bSubbed ? _bars_cache[key] : _unbars_cache[key];  // 获取K线列表指针
		barsList->_bars.resize(barcnt);                                     // 调整缓存列表大小
		memcpy(barsList->_bars.data(), content.data(), content.size());     // 复制数据到缓存列表
		barsList->_cursor = UINT_MAX;                                       // 重置游标（未初始化）
		barsList->_code = stdCode;                                          // 设置合约代码
		barsList->_period = period;                                         // 设置周期
		barsList->_count = barcnt;                                           // 设置数量

		uint64_t stime = isDay ? barsList->_bars[0].date : barsList->_bars[0].time;  // 计算开始时间
		uint64_t etime = isDay ? barsList->_bars[barcnt-1].date : barsList->_bars[barcnt-1].time;  // 计算结束时间

		WTSLogger::info("{} items of back {} data of {} directly loaded from dsb file, from {} to {}", barcnt, p_suffix.c_str(), stdCode, stime, etime);  // 记录信息日志
	}
	else                                                                    // 如果dsb文件不存在
	{
		//如果没有格式化的历史数据文件, 则从csv加载
		std::stringstream ss;                                               // 字符串流
		ss << _base_dir << "csv/" << stdCode << "_" << p_suffix << ".csv";  // 构造CSV文件名
		std::string csvfile = ss.str();                                     // 获取CSV文件名

		if (!StdFile::exists(csvfile.c_str()))                              // 如果CSV文件不存在
		{
			WTSLogger::error("Back kbar data file {} not exists", csvfile);  // 记录错误日志
			return false;                                                   // 返回失败
		}

		CsvReader reader;                                                   // CSV读取器
		reader.load_from_file(csvfile.c_str());                            // 从文件加载CSV

		WTSLogger::info("Reading data from {}, with fields: {}...", csvfile, reader.fields());  // 记录信息日志

		if (bSubbed)                                                        // 如果已订阅
			_bars_cache[key].reset(new BarsList);                         // 创建订阅K线缓存
		else                                                                // 如果未订阅
			_unbars_cache[key].reset(new BarsList);                        // 创建未订阅K线缓存

		BarsListPtr& barsList = bSubbed ? _bars_cache[key] : _unbars_cache[key];  // 获取K线列表指针
		barsList->_code = stdCode;                                          // 设置合约代码
		barsList->_period = period;                                         // 设置周期
		while (reader.next_row())                                           // 逐行读取CSV数据
		{
			//逐行读取
			WTSBarStruct bs;                                                 // K线结构体
			bs.date = strToDate(reader.get_string("date"));                // 读取日期
			if (period != KP_DAY)                                            // 如果不是日线
				bs.time = TimeUtils::timeToMinBar(bs.date, strToTime(reader.get_string("time")));  // 转换时间格式
			bs.open = reader.get_double("open");                            // 读取开盘价
			bs.high = reader.get_double("high");                            // 读取最高价
			bs.low = reader.get_double("low");                              // 读取最低价
			bs.close = reader.get_double("close");                          // 读取收盘价
			bs.vol = reader.get_double("volume");                           // 读取成交量
			bs.money = reader.get_double("turnover");                       // 读取成交额
			bs.hold = reader.get_double("open_interest");                   // 读取持仓量
			bs.add = reader.get_double("diff_interest");                    // 读取持仓变化
			bs.settle = reader.get_double("settle");                        // 读取结算价
			barsList->_bars.emplace_back(bs);                               // 添加到缓存列表

			if (barsList->_bars.size() % 1000 == 0)                         // 每1000条记录
			{
				WTSLogger::info("{} lines of data loaded", barsList->_bars.size());  // 记录进度日志
			}
		}
		barsList->_count = barsList->_bars.size();                          // 设置数量

		uint64_t stime = isDay ? barsList->_bars[0].date : barsList->_bars[0].time;  // 计算开始时间
		uint64_t etime = isDay ? barsList->_bars[barsList->_count - 1].date : barsList->_bars[barsList->_count - 1].time;  // 计算结束时间

		WTSLogger::info("Data file {} all loaded, totally {} items, from {} to {}", csvfile.c_str(), barsList->_bars.size(), stime, etime);  // 记录信息日志

		BlockType btype;                                                    // 块类型
		switch (period)                                                      // 根据周期设置块类型
		{
		case KP_Minute1: btype = BT_HIS_Minute1; break;                  // 1分钟线块类型
		case KP_Minute5: btype = BT_HIS_Minute5; break;                  // 5分钟线块类型
		default: btype = BT_HIS_Day; break;                                // 默认：日线块类型
		}

		/*
		 *	By Wesley @ 2021.12.14
		 *	这一段之前有bug，之前没有把文件头写到文件里，所以转储的dsb解析的时候会抛出异常
		 */
		std::string content;                                                // 文件内容
		content.resize(sizeof(HisKlineBlockV2));                            // 调整大小为块头大小
		HisKlineBlockV2 *kBlock = (HisKlineBlockV2*)content.data();        // 转换为块头指针
		strcpy(kBlock->_blk_flag, BLK_FLAG);                                // 设置块标志
		kBlock->_type = btype;                                              // 设置块类型
		kBlock->_version = BLOCK_VERSION_CMP_V2;                            // 设置块版本

		std::string cmpData = WTSCmpHelper::compress_data(barsList->_bars.data(), sizeof(WTSBarStruct)*barsList->_count);  // 压缩K线数据
		kBlock->_size = cmpData.size();                                     // 设置压缩后数据大小
		content.append(cmpData);                                            // 追加压缩数据

		StdFile::write_file_content(filename.c_str(), content.c_str(), content.size());  // 写入文件（转储为dsb格式）
		WTSLogger::info("Bars transfered to file {}", filename);            // 记录信息日志
	}

	return true;                                                            // 返回成功
}

/**
 * @brief 从二进制文件缓存期货主力连续K线数据
 * 
 * 从二进制文件中加载并整合期货主力连续合约的K线数据，根据主力切换规则将不同月份的合约数据拼接起来
 * 
 * @param codeInfo 合约代码信息指针
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param period K线周期
 * @param bSubbed 是否已订阅（默认true）
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheIntegratedFutBarsFromBin(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed /* = true */)
{
	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo;      // 转换合约代码信息指针
	const char* stdPID = cInfo->stdCommID();                              // 获取标准化产品ID

	uint32_t curDate = TimeUtils::getCurDate();                           // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;                      // 获取当前时间（分钟）

	uint32_t endTDate = _bd_mgr.calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期

	std::string pname;                                                     // 周期名称
	switch (period)                                                        // 根据周期设置名称
	{
	case KP_Minute1: pname = "min1"; break;                              // 1分钟线
	case KP_Minute5: pname = "min5"; break;                              // 5分钟线
	default: pname = "day"; break;                                        // 默认：日线
	}

	if(bSubbed)                                                            // 如果已订阅
		_bars_cache[key].reset(new BarsList());                            // 创建订阅K线缓存
	else                                                                    // 如果未订阅
		_unbars_cache[key].reset(new BarsList());                          // 创建未订阅K线缓存
	BarsListPtr& barsList = bSubbed ? _bars_cache[key] : _unbars_cache[key];  // 获取K线列表指针
	barsList->_code = stdCode;                                             // 设置合约代码
	barsList->_period = period;                                            // 设置周期

	std::vector<std::vector<WTSBarStruct>*> barsSections;                 // K线分段列表（用于拼接不同月份的合约数据）

	uint32_t realCnt = 0;                                                  // 实际K线数量
	bool isDay = (period == KP_DAY);                                       // 是否为日线

	//const char* hot_flag = cInfo.isHot() ? FILE_SUF_HOT : FILE_SUF_2ND;
	const char* ruleTag = cInfo->_ruletag;                                 // 规则标签

	std::vector<WTSBarStruct>* hotAy = NULL;                               // 主力合约K线数据
	uint64_t lastHotTime = 0;                                              // 主力合约最后一条K线的时间
	do                                                                      // 使用do-while(false)实现代码块，方便break
	{
		/*
		 *	By Wesley @ 2021.12.20
		 *	本来这里是要先调用_loader->loadRawHisBars从外部加载器读取主力合约数据的
		 *	但是上层会调用一次loadFinalHisBars，这里再调用loadRawHisBars就冗余了，所以直接跳过
		 *
		 *	@ 2022.01.11
		 *	将直接从文件读取，改成从HisDtMgr读取
		 */
		std::string content;                                                // 文件内容
		std::string wrappCode = StrUtil::printf("%s.%s_%s", cInfo->_exchg, cInfo->_product, ruleTag);  // 构造包装代码（交易所.品种_规则标签）
		if (cInfo->isExright())                                             // 如果是复权数据
			wrappCode += cInfo->_exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ;  // 添加复权后缀
		bool bSucc = _his_dt_mgr.load_raw_bars(cInfo->_exchg, wrappCode.c_str(), period, [&content](std::string& data) {  // 加载主力合约K线数据
			content.swap(data);                                             // 交换数据内容
		});

		if(!bSucc)                                                          // 如果加载失败
		{
			WTSLogger::warn("Loading {} bars of {} via HisDtMgr failed", PERIOD_NAME[period], stdCode);  // 记录警告日志
			break;                                                          // 退出代码块
		}

		uint32_t barcnt = content.size() / sizeof(WTSBarStruct);           // 计算K线数量
		if (barcnt <= 0)                                                    // 如果数量为0
			break;                                                          // 退出代码块

		hotAy = new std::vector<WTSBarStruct>();                          // 创建主力合约K线数据向量
		hotAy->resize(barcnt);                                              // 调整大小
		memcpy(hotAy->data(), content.data(), content.size());            // 复制数据

		if (period != KP_DAY)                                               // 如果不是日线
			lastHotTime = hotAy->at(barcnt - 1).time;                     // 获取最后一条K线的时间
		else                                                                // 如果是日线
			lastHotTime = hotAy->at(barcnt - 1).date;                     // 获取最后一条K线的日期

		uint64_t stime = isDay ? hotAy->at(0).date : hotAy->at(0).time;   // 计算开始时间
		uint64_t etime = isDay ? hotAy->at(barcnt - 1).date : hotAy->at(barcnt - 1).time;  // 计算结束时间

		WTSLogger::info("{} items of back {} data of hot contract {} directly loaded, from {} to {}", barcnt, pname.c_str(), stdCode, stime, etime);  // 记录信息日志

	} while (false);

	HotSections secs;                                                      // 主力切换时间段列表
	//if (cInfo.isHot())
	//{
	//	if (!_hot_mgr.splitHotSecions(cInfo._exchg, cInfo._product, 19900102, endTDate, secs))
	//		return false;
	//}
	//else if (cInfo.isSecond())
	//{
	//	if (!_hot_mgr.splitSecondSecions(cInfo._exchg, cInfo._product, 19900102, endTDate, secs))
	//		return false;
	//}
	if(strlen(ruleTag) > 0)                                                // 如果有规则标签
	{
		if (!_hot_mgr.splitCustomSections(ruleTag, cInfo->stdCommID(), 19900102, endTDate, secs))  // 分割自定义时间段
			return false;                                                   // 返回失败
	}

	if (secs.empty())                                                      // 如果时间段列表为空
		return false;                                                       // 返回失败

	double baseFactor = 1.0;                                               // 基础复权因子
	if (cInfo->_exright == 1)                                              // 如果是前复权
		baseFactor = secs.back()._factor;                                   // 使用最后一个复权因子
	else if (cInfo->_exright == 2)                                         // 如果是后复权
		barsList->_factor = secs.back()._factor;                            // 设置K线列表的复权因子

	bool bAllCovered = false;                                              // 是否全部覆盖标志
	for (auto it = secs.rbegin(); it != secs.rend(); it++)                 // 从后往前遍历时间段（从最新到最旧）
	{
		const HotSection& hotSec = *it;                                    // 获取时间段
		const char* curCode = hotSec._code.c_str();                        // 获取当前合约代码
		uint32_t rightDt = hotSec._e_date;                                  // 结束日期
		uint32_t leftDt = hotSec._s_date;                                  // 开始日期

		//要先将日期转换为边界时间
		WTSBarStruct sBar, eBar;                                           // 开始和结束K线结构体
		if (period != KP_DAY)                                              // 如果不是日线
		{
			uint64_t sTime = _bd_mgr.getBoundaryTime(stdPID, leftDt, false, true);  // 获取开始边界时间（开盘时间）
			uint64_t eTime = _bd_mgr.getBoundaryTime(stdPID, rightDt, false, false);  // 获取结束边界时间（收盘时间）

			sBar.date = leftDt;                                            // 设置开始日期
			sBar.time = ((uint32_t)(sTime / 10000) - 19900000) * 10000 + (uint32_t)(sTime % 10000);  // 设置开始时间（转换为从1990年1月1日开始的秒数）

			if (sBar.time < lastHotTime)	//如果边界时间小于主力的最后一根Bar的时间, 说明已经有交叉了, 则不需要再处理了
			{
				bAllCovered = true;                                        // 标记为全部覆盖
				sBar.time = lastHotTime + 1;                               // 调整开始时间
			}

			eBar.date = rightDt;                                           // 设置结束日期
			eBar.time = ((uint32_t)(eTime / 10000) - 19900000) * 10000 + (uint32_t)(eTime % 10000);  // 设置结束时间

			if (eBar.time <= lastHotTime)	//右边界时间小于最后一条Hot时间, 说明全部交叉了, 没有再找的必要了
				break;                                                     // 退出循环
		}
		else                                                                // 如果是日线
		{
			sBar.date = leftDt;                                            // 设置开始日期
			if (sBar.date < lastHotTime)	//如果边界时间小于主力的最后一根Bar的时间, 说明已经有交叉了, 则不需要再处理了
			{
				bAllCovered = true;                                        // 标记为全部覆盖
				sBar.date = (uint32_t)lastHotTime + 1;                     // 调整开始日期
			}

			eBar.date = rightDt;                                           // 设置结束日期

			if (eBar.date <= lastHotTime)                                  // 如果结束日期小于等于主力最后时间
				break;                                                     // 退出循环
		}

		/*
		 *	By Wesley @ 2021.12.20
		 *	先从extloader读取分月合约的K线数据
		 *	如果没有读到，再从文件读取
		 */
		bool bLoaded = false;                                               // 是否加载成功标志
		std::string buffer;                                                 // 缓冲区
		if (NULL != _bt_loader)                                             // 如果外部数据加载器存在
		{
			//分月合约代码
			std::string wCode = StrUtil::printf("%s.%s.%s", cInfo->_exchg, cInfo->_product, (char*)curCode + strlen(cInfo->_product));  // 构造分月合约代码
			bLoaded = _bt_loader->loadRawHisBars(&buffer, wCode.c_str(), period, [](void* obj, WTSBarStruct* bars, uint32_t count) {  // 从外部加载器加载分月合约K线数据
				std::string* buff = (std::string*)obj;                     // 转换为缓冲区指针
				buff->resize(sizeof(WTSBarStruct)*count);                  // 调整缓冲区大小
				memcpy((void*)buff->c_str(), bars, sizeof(WTSBarStruct)*count);  // 复制数据
			});

		}

		if (!bLoaded)                                                       // 如果外部加载器未加载成功
		{
			bLoaded = _his_dt_mgr.load_raw_bars(cInfo->_exchg, curCode, period, [&buffer](std::string& data) {  // 从历史数据管理器加载分月合约K线数据
				buffer.swap(data);                                          // 交换数据内容
			});

			if (!bLoaded)                                                   // 如果仍未加载成功
			{
				WTSLogger::warn("Loading {} bars of {} via HisDtMgr failed", PERIOD_NAME[period], curCode);  // 记录警告日志
				break;                                                      // 退出循环
			}
		}

		if (buffer.empty())                                                 // 如果缓冲区为空
			break;                                                          // 退出循环

		uint32_t barcnt = buffer.size() / sizeof(WTSBarStruct);           // 计算K线数量

		WTSBarStruct* firstBar = (WTSBarStruct*)buffer.data();             // 获取第一条K线指针

		WTSBarStruct* pBar = std::lower_bound(firstBar, firstBar + (barcnt - 1), sBar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位开始位置
			if (period == KP_DAY)                                           // 如果是日线
			{
				return a.date < b.date;                                     // 按日期比较
			}
			else                                                            // 如果不是日线
			{
				return a.time < b.time;                                     // 按时间比较
			}
		});

		std::size_t sIdx = pBar - firstBar;                                // 计算开始索引
		if ((period == KP_DAY && pBar->date < sBar.date) || (period != KP_DAY && pBar->time < sBar.time))	//早于边界时间
		{
			//早于边界时间, 说明没有数据了, 因为lower_bound会返回大于等于目标位置的数据
			continue;                                                       // 跳过该时间段
		}

		pBar = std::lower_bound(firstBar + sIdx, firstBar + (barcnt - 1), eBar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位结束位置
			if (period == KP_DAY)                                           // 如果是日线
			{
				return a.date < b.date;                                     // 按日期比较
			}
			else                                                            // 如果不是日线
			{
				return a.time < b.time;                                     // 按时间比较
			}
		});
		std::size_t eIdx = pBar - firstBar;                                // 计算结束索引
		if ((period == KP_DAY && pBar->date > eBar.date) || (period != KP_DAY && pBar->time > eBar.time))  // 如果定位的位置超过结束边界
		{
			pBar--;                                                         // 向前移动一位
			eIdx--;                                                         // 索引减1
		}

		if (eIdx < sIdx)                                                    // 如果结束索引小于开始索引
			continue;                                                       // 跳过该时间段

		uint32_t curCnt = eIdx - sIdx + 1;                                 // 计算当前时间段的K线数量

		if (cInfo->isExright())                                             // 如果是复权数据
		{
			double factor = hotSec._factor / baseFactor;                    // 计算复权因子
			for (uint32_t idx = sIdx; idx <= eIdx; idx++)                  // 遍历该时间段的K线
			{
				firstBar[idx].open *= factor;                               // 调整开盘价
				firstBar[idx].high *= factor;                               // 调整最高价
				firstBar[idx].low *= factor;                                // 调整最低价
				firstBar[idx].close *= factor;                              // 调整收盘价
				firstBar[idx].settle *= factor;                             // 调整结算价

				if (_adjust_flag & 1)                                       // 如果调整标志包含成交量调整
					firstBar[idx].vol /= factor;                            // 调整成交量（除以因子）

				if (_adjust_flag & 2)                                       // 如果调整标志包含成交额调整
					firstBar[idx].money *= factor;                          // 调整成交额（乘以因子）

				if (_adjust_flag & 4)                                       // 如果调整标志包含持仓量调整
				{
					firstBar[idx].hold /= factor;                            // 调整持仓量（除以因子）
					firstBar[idx].add /= factor;                             // 调整持仓变化（除以因子）
				}
			}
		}

		std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>();  // 创建临时K线向量
		tempAy->resize(curCnt);                                               // 调整大小
		memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt);  // 复制该时间段的K线数据
		realCnt += curCnt;                                                    // 累计实际数量

		barsSections.emplace_back(tempAy);                                    // 添加到分段列表

		if (bAllCovered)                                                      // 如果全部覆盖
			break;                                                            // 退出循环
	}

	if (hotAy)                                                                // 如果主力合约数据存在
	{
		barsSections.emplace_back(hotAy);                                     // 添加到分段列表（放在最后）
		realCnt += hotAy->size();                                             // 累计实际数量
	}

	if (realCnt > 0)                                                          // 如果实际数量大于0
	{
		barsList->_bars.resize(realCnt);                                      // 调整K线列表大小

		uint32_t curIdx = 0;                                                  // 当前索引
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)  // 从后往前遍历分段列表（从最新到最旧）
		{
			std::vector<WTSBarStruct>* tempAy = *it;                          // 获取分段K线向量
			memcpy(barsList->_bars.data() + curIdx, tempAy->data(), tempAy->size() * sizeof(WTSBarStruct));  // 复制分段数据到最终列表
			curIdx += tempAy->size();                                          // 更新当前索引
			delete tempAy;                                                     // 释放临时向量
		}
		barsSections.clear();                                                 // 清空分段列表
	}

	WTSLogger::info("{} items of back {} data of {} cached", realCnt, pname, stdCode);  // 记录信息日志

	return true;                                                               // 返回成功
}

/**
 * @brief 获取复权因子列表
 * 
 * 获取指定合约的复权因子列表，如果缓存中没有则尝试从外部加载器加载
 * 
 * @param code 合约代码
 * @param exchg 交易所代码
 * @param pid 产品ID（默认空字符串）
 * @return 复权因子列表引用
 */
const HisDataReplayer::AdjFactorList& HisDataReplayer::getAdjFactors(const char* code, const char* exchg, const char* pid /* = "" */)
{
	static char key[20] = { 0 };                                               // 静态键缓冲区（线程不安全，但用于缓存键构造）
	fmtutil::format_to(key, "{}.{}.{}", exchg, pid, code);                    // 构造复权因子键（交易所.产品.代码）

	auto it = _adj_factors.find(key);                                          // 查找复权因子
	if (it == _adj_factors.end())                                              // 如果未找到
	{
		//By Wesley @ 2021.12.21
		//如果没有复权因子，就从extloader按需读一次
		if (_bt_loader)                                                        // 如果外部数据加载器存在
		{
            WTSLogger::info("No adjusting factors of {} cached, searching via extented loader...", key);  // 记录信息日志
			_bt_loader->loadAdjFactors(this, key, [](void* obj, const char* stdCode, uint32_t* dates, double* factors, uint32_t count) {  // 通过外部加载器加载复权因子
				HisDataReplayer* self = (HisDataReplayer*)obj;                // 转换为回放器指针
				AdjFactorList& fctrLst = self->_adj_factors[stdCode];         // 获取复权因子列表

				for (uint32_t i = 0; i < count; i++)                          // 遍历复权因子
				{
					AdjFactor adjFact;                                         // 复权因子结构体
					adjFact._date = dates[i];                                  // 设置日期
					adjFact._factor = factors[i];                              // 设置因子
					fctrLst.emplace_back(adjFact);                              // 添加到列表
				}

				//一定要把第一条加进去，不然如果是前复权的话，可能会漏处理最早的数据
				AdjFactor adjFact;                                             // 创建默认复权因子
				adjFact._date = 19900101;                                      // 设置日期为1990年1月1日
				adjFact._factor = 1;                                           // 设置因子为1（不复权）
				fctrLst.emplace_back(adjFact);                                  // 添加到列表

				std::sort(fctrLst.begin(), fctrLst.end(), [](const AdjFactor& left, const AdjFactor& right) {  // 按日期排序
					return left._date < right._date;                             // 日期小的在前
				});

                WTSLogger::info("{} items of adjusting factors of {} loaded via extended loader", count, stdCode);  // 记录信息日志
			});
		}
	}

	return _adj_factors[key];                                                  // 返回复权因子列表引用
}

/**
 * @brief 从二进制文件缓存股票复权K线数据
 * 
 * 从二进制文件中加载并处理股票复权K线数据，先加载已复权的数据，然后加载未复权的历史数据并应用复权因子
 * 
 * @param codeInfo 合约代码信息指针
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param period K线周期
 * @param bSubbed 是否已订阅（默认true）
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheAdjustedStkBarsFromBin(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed /* = true */)
{
	CodeHelper::CodeInfo* cInfo = (CodeHelper::CodeInfo*)codeInfo;          // 转换合约代码信息指针
	const char* stdPID = cInfo->stdCommID();                                 // 获取标准化产品ID

	uint32_t curDate = TimeUtils::getCurDate();                              // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;                         // 获取当前时间（分钟）

	uint32_t endTDate = _bd_mgr.calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期

	_bars_cache[key].reset(new BarsList());                                  // 创建K线缓存（股票复权数据必须订阅）
	BarsListPtr& barsList = _bars_cache[key];                                // 获取K线列表指针
	barsList->_code = stdCode;                                                // 设置合约代码
	barsList->_period = period;                                               // 设置周期

	std::vector<std::vector<WTSBarStruct>*> barsSections;                    // K线分段列表（用于拼接不同时间段的K线）

	uint32_t realCnt = 0;                                                     // 实际K线数量

	std::vector<WTSBarStruct>* adjustedBars = NULL;                          // 已复权K线数据
	uint64_t lastQTime = 0;                                                   // 已复权K线最后一条的时间

	WTSLogger::info("Loading adjusted bars of {}...", stdCode);             // 记录信息日志
	do                                                                         // 使用do-while(false)实现代码块，方便break
	{
		//先直接读取复权过的历史数据,路径如/his/day/sse/SH600000Q.dsb

		/*
		 *	By Wesley @ 2021.12.20
		 *	本来这里是要先调用_loader->loadRawHisBars从外部加载器读取主力合约数据的
		 *	但是上层会调用一次loadFinalHisBars，这里再调用loadRawHisBars就冗余了，所以直接跳过
		 *	
		 *	@ 2022.01.11
		 *	这里将文件读取改为从HisDtMgr封装的接口读取
		 */
		std::string wrappCode = fmt::format("{}{}", cInfo->_code, (cInfo->_exright == 1 ? SUFFIX_QFQ : SUFFIX_HFQ));  // 构造包装代码（代码+复权后缀）
		std::string content;                                                   // 文件内容
		bool bSucc = _his_dt_mgr.load_raw_bars(cInfo->_exchg, wrappCode.c_str(), period, [&content](std::string& data) {  // 加载已复权K线数据
			content.swap(data);                                                // 交换数据内容
		});

		if(!bSucc)                                                             // 如果加载失败
		{
			WTSLogger::warn("Loading {} bars of {} via HisDtMgr failed", PERIOD_NAME[period], stdCode);  // 记录警告日志
			break;                                                             // 退出代码块
		}

		uint32_t barcnt = content.size() / sizeof(WTSBarStruct);              // 计算K线数量
		if (barcnt <= 0)                                                       // 如果数量为0
			break;                                                             // 退出代码块

		adjustedBars = new std::vector<WTSBarStruct>();                       // 创建已复权K线数据向量
		adjustedBars->resize(barcnt);                                          // 调整大小
		memcpy(adjustedBars->data(), content.data(), content.size());         // 复制数据

		if (period != KP_DAY)                                                  // 如果不是日线
			lastQTime = adjustedBars->at(barcnt - 1).time;                  // 获取最后一条K线的时间
		else                                                                    // 如果是日线
			lastQTime = adjustedBars->at(barcnt - 1).date;                  // 获取最后一条K线的日期

		WTSLogger::info("{} items of adjusted back {} data of {} directly loaded", barcnt, PERIOD_NAME[period], stdCode);  // 记录信息日志
	} while (false);

	WTSLogger::info("Loading raw bars of {}...", stdCode);                  // 记录信息日志
	do                                                                         // 使用do-while(false)实现代码块，方便break
	{
		const char* curCode = cInfo->_code;                                   // 获取原始合约代码

		//要先将日期转换为边界时间
		WTSBarStruct sBar;                                                    // 开始K线结构体
		if (period != KP_DAY)                                                  // 如果不是日线
		{
			sBar.date = TimeUtils::minBarToDate(lastQTime);                  // 从分钟时间转换为日期
			sBar.time = lastQTime + 1;                                         // 设置开始时间（已复权K线最后时间+1）
		}
		else                                                                    // 如果是日线
		{
			sBar.date = (uint32_t)lastQTime + 1;                               // 设置开始日期（已复权K线最后日期+1）
		}

		/*
		 *	By Wesley @ 2021.12.20
		 *	先从extloader读取未复权K线数据
		 *	如果没有读到，再从文件读取
		 */
		bool bLoaded = false;                                                  // 是否加载成功标志
		std::string buffer;                                                    // 缓冲区
		if (NULL != _bt_loader)                                                // 如果外部数据加载器存在
		{
			std::string wCode = StrUtil::printf("%s.%s.%s", cInfo->_exchg, cInfo->_product, curCode);  // 构造完整合约代码（交易所.产品.代码）
			bLoaded = _bt_loader->loadRawHisBars(&buffer, wCode.c_str(), period, [](void* obj, WTSBarStruct* bars, uint32_t count) {  // 通过外部加载器加载未复权K线数据
				std::string* buff = (std::string*)obj;                        // 转换为字符串指针
				buff->resize(sizeof(WTSBarStruct)*count);                     // 调整缓冲区大小
				memcpy((void*)buff->c_str(), bars, sizeof(WTSBarStruct)*count);  // 复制K线数据
			});

			if (bLoaded)                                                       // 如果加载成功
				WTSLogger::debug("Raw bars of {} loaded via extended loader", stdCode);  // 记录调试日志
		}

		if (!bLoaded)                                                          // 如果外部加载器未加载成功
		{
			/*
			 *	By Wesley @ 2022.01.11
			 *	这里将文件读取改为从HisDtMgr封装的接口读取
			 */
			bLoaded = _his_dt_mgr.load_raw_bars(cInfo->_exchg, curCode, period, [&buffer](std::string& data) {  // 通过历史数据管理器加载未复权K线数据
				buffer.swap(data);                                             // 交换数据内容
			});

			if (!bLoaded)                                                      // 如果加载失败
			{
				WTSLogger::warn("Loading {} bars of {} via HisDtMgr failed", PERIOD_NAME[period], curCode);  // 记录警告日志
				continue;                                                      // 继续下一次循环（但这是do-while(false)的最后一个循环）
			}
		}

		if (buffer.empty())                                                    // 如果缓冲区为空
			break;                                                             // 退出代码块
		
		std::size_t barcnt = buffer.size() / sizeof(WTSBarStruct);            // 计算K线数量

		WTSBarStruct* firstBar = (WTSBarStruct*)buffer.data();                // 获取K线数据起始指针

		WTSBarStruct* pBar = std::lower_bound(firstBar, firstBar + (barcnt - 1), sBar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 二分查找开始位置（第一个大于等于sBar的K线）
			if (period == KP_DAY)                                              // 如果是日线
			{
				return a.date < b.date;                                        // 按日期比较
			}
			else                                                                // 如果不是日线
			{
				return a.time < b.time;                                        // 按时间比较
			}
		});

		if (pBar != NULL)                                                      // 如果找到K线
		{
			std::size_t sIdx = pBar - firstBar;                                // 计算开始索引
			std::size_t curCnt = barcnt - sIdx;                                // 计算K线数量
			std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>();  // 创建临时K线向量
			tempAy->resize(curCnt);                                             // 调整大小
			memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt);  // 复制K线数据
			realCnt += curCnt;                                                 // 累计实际数量

			auto& ayFactors = getAdjFactors(cInfo->_code, cInfo->_exchg, cInfo->_product);  // 获取复权因子列表
			if (!ayFactors.empty())                                             // 如果复权因子列表不为空
			{
				WTSLogger::info("Adjusting bars of {} with adjusting factors...", stdCode);  // 记录信息日志
				//做复权处理
				std::size_t lastIdx = curCnt;                                  // 最后处理的索引（从后往前处理）
				WTSBarStruct bar;                                               // 临时K线结构体
				firstBar = tempAy->data();                                      // 更新firstBar指针

				//根据复权类型确定基础因子
				//如果是前复权，则历史数据会变小，以最后一个复权因子为基础因子
				//如果是后复权，则新数据会变大，基础因子为1
				double baseFactor = 1.0;                                        // 基础因子（默认1.0）
				if (cInfo->_exright == 1)                                       // 如果是前复权
					baseFactor = ayFactors.back()._factor;                      // 使用最后一个复权因子作为基础因子
				else if (cInfo->_exright == 2)                                  // 如果是后复权
					barsList->_factor = ayFactors.back()._factor;                // 设置K线列表的复权因子

				for (auto it = ayFactors.rbegin(); it != ayFactors.rend(); it++)  // 从后往前遍历复权因子（从最新到最旧）
				{
					const AdjFactor& adjFact = *it;                             // 获取复权因子
					bar.date = adjFact._date;                                   // 设置日期（用于查找）

					//调整因子
					double factor = adjFact._factor / baseFactor;                // 计算调整因子（复权因子/基础因子）

					WTSBarStruct* pBar = NULL;                                  // K线指针
					pBar = std::lower_bound(firstBar, firstBar + lastIdx - 1, bar, [period](const WTSBarStruct& a, const WTSBarStruct& b) {  // 二分查找该复权因子对应的K线位置
						return a.date < b.date;                                 // 按日期比较
					});

					if (pBar->date < bar.date)                                  // 如果找到的K线日期小于复权因子日期
						continue;                                               // 跳过该复权因子

					WTSBarStruct* endBar = pBar;                                // 记录结束位置
					if (pBar != NULL)                                           // 如果找到K线
					{
						std::size_t curIdx = pBar - firstBar;                   // 计算当前索引
						while (pBar && curIdx < lastIdx)                        // 遍历从该位置到lastIdx的所有K线
						{
							pBar->open *= factor;                                // 调整开盘价
							pBar->high *= factor;                                // 调整最高价
							pBar->low *= factor;                                 // 调整最低价
							pBar->close *= factor;                               // 调整收盘价

							if (_adjust_flag & 1)                               // 如果调整标志包含成交量（位1）
								pBar->vol /= factor;                             // 调整成交量（除以因子）

							if (_adjust_flag & 2)                               // 如果调整标志包含成交额（位2）
								pBar->money *= factor;                           // 调整成交额（乘以因子）

							if (_adjust_flag & 4)                               // 如果调整标志包含持仓量（位4）
							{
								pBar->hold /= factor;                            // 调整持仓量（除以因子）
								pBar->add /= factor;                             // 调整持仓变化（除以因子）
							}

							pBar++;                                             // 移动到下一条K线
							curIdx++;                                           // 更新索引
						}
						lastIdx = endBar - firstBar;                            // 更新最后处理的索引（从后往前缩小范围）
					}

					if (lastIdx == 0)                                           // 如果处理完所有K线
						break;                                                  // 退出循环
				}
			}
			else                                                                // 如果复权因子列表为空
			{
				WTSLogger::info("No adjusting factors of {} found, ajusting task skipped...", stdCode);  // 记录信息日志
			}

			barsSections.emplace_back(tempAy);                                  // 添加到分段列表
		}
	} while (false);

	if (adjustedBars)                                                          // 如果已复权K线数据存在
	{
		barsSections.emplace_back(adjustedBars);                                // 添加到分段列表（放在最后）
		realCnt += adjustedBars->size();                                       // 累计实际数量
	}

	if (realCnt > 0)                                                           // 如果实际数量大于0
	{
		barsList->_bars.resize(realCnt);                                       // 调整K线列表大小

		uint32_t curIdx = 0;                                                   // 当前索引
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)  // 从后往前遍历分段列表（从最新到最旧）
		{
			std::vector<WTSBarStruct>* tempAy = *it;                            // 获取分段K线向量
			memcpy(barsList->_bars.data() + curIdx, tempAy->data(), tempAy->size() * sizeof(WTSBarStruct));  // 复制分段数据到最终列表
			curIdx += tempAy->size();                                          // 更新当前索引
			delete tempAy;                                                     // 释放临时向量
		}
		barsSections.clear();                                                  // 清空分段列表
	}

	WTSLogger::info("{} items of adjusted back {} data of {} cached", realCnt, PERIOD_NAME[period], stdCode);  // 记录信息日志（修正：使用adjusted而不是back）

	return true;
}


/**
 * @brief 从二进制文件缓存原始K线数据
 * 
 * 从二进制文件中加载并缓存原始K线数据，根据合约类型判断是否需要调用特殊处理函数（期货主力连续或股票复权）
 * 
 * @param key 缓存键
 * @param stdCode 合约代码
 * @param period K线周期
 * @param bSubbed 是否已订阅（默认true）
 * @return 是否成功缓存
 */
bool HisDataReplayer::cacheRawBarsFromBin(const std::string& key, const char* stdCode, WTSKlinePeriod period, bool bSubbed/* = true*/)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, &_hot_mgr);  // 解析合约代码信息
	WTSCommodityInfo* commInfo = _bd_mgr.getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息
	const char* stdPID = cInfo.stdCommID();                                    // 获取标准化产品ID

	uint32_t curDate = TimeUtils::getCurDate();                                // 获取当前日期
	uint32_t curTime = TimeUtils::getCurMin() / 100;                           // 获取当前时间（分钟）

	uint32_t endTDate = _bd_mgr.calcTradingDate(stdPID, curDate, curTime, false);  // 计算结束交易日期

	bool isDay = (period == KP_DAY);                                           // 是否为日线

	if (bSubbed)                                                                // 如果已订阅
		_bars_cache[key].reset(new BarsList);                                   // 创建已订阅K线缓存
	else                                                                        // 如果未订阅
		_unbars_cache[key].reset(new BarsList);                                 // 创建未订阅K线缓存

	BarsListPtr& barsList = bSubbed ? _bars_cache[key] : _unbars_cache[key];  // 获取K线列表指针
	barsList->_code = stdCode;                                                  // 设置合约代码
	barsList->_period = period;                                                 // 设置周期

	std::vector<std::vector<WTSBarStruct>*> barsSections;                      // K线分段列表（用于拼接不同时间段的K线）

	uint32_t realCnt = 0;                                                       // 实际K线数量
	const char* ruleTag = cInfo._ruletag;                                       // 获取规则标签
	if (strlen(ruleTag) > 0)//如果是读取期货主力连续数据
	{
		return cacheIntegratedFutBarsFromBin(&cInfo, key, stdCode, period, bSubbed);  // 调用期货主力连续K线缓存函数
	}
	else if (cInfo.isExright() && commInfo->isStock())//如果是读取股票复权数据
	{
		return cacheAdjustedStkBarsFromBin(&cInfo, key, stdCode, period, bSubbed);  // 调用股票复权K线缓存函数
	}
	

	/*
	 *	By Wesley @ 2021.12.20
	 *	先从extloader读取
	 *	如果没有读到，再从文件读取
	 */
	bool bLoaded = false;                                                       // 是否加载成功标志
	std::string buffer;                                                         // 缓冲区
	if (NULL != _bt_loader)                                                     // 如果外部数据加载器存在
	{
		bLoaded = _bt_loader->loadRawHisBars(&buffer, stdCode, period, [](void* obj, WTSBarStruct* bars, uint32_t count) {  // 通过外部加载器加载K线数据
			std::string* buff = (std::string*)obj;                              // 转换为字符串指针
			buff->resize(sizeof(WTSBarStruct)*count);                           // 调整缓冲区大小
			memcpy((void*)buff->c_str(), bars, sizeof(WTSBarStruct)*count);     // 复制K线数据
		});
	}

	if(!bLoaded)                                                                // 如果外部加载器未加载成功
	{
		//读取历史的
		//std::stringstream ss;
		//ss << _base_dir << "his/" << pname << "/" << cInfo._exchg << "/" << cInfo._code << ".dsb";
		//std::string filename = ss.str();
		//if (StdFile::exists(filename.c_str()))
		//{
		//	//如果有格式化的历史数据文件, 则直接读取
		//	std::string content;
		//	StdFile::read_file_content(filename.c_str(), content);
		//	if (content.size() < sizeof(HisKlineBlock))
		//	{
		//		WTSLogger::error("Sizechecking of back kbar data file {} failed", filename.c_str());
		//		return false;
		//	}

		//	proc_block_data(filename.c_str(), content, true, false);
		//	buffer.swap(content);
		//}
		bLoaded = _his_dt_mgr.load_raw_bars(cInfo._exchg, cInfo._code, period, [&buffer](std::string& data) {  // 通过历史数据管理器加载K线数据
			buffer.swap(data);                                                  // 交换数据内容
		});

		if(!bLoaded)                                                            // 如果加载失败
		{
			WTSLogger::warn("Loading {} bars of {} via HisDtMgr failed", PERIOD_NAME[period], stdCode);  // 记录警告日志
		}
	}

	if (buffer.empty())                                                         // 如果缓冲区为空
		return false;                                                            // 返回失败

	uint32_t barcnt = buffer.size() / sizeof(WTSBarStruct);                    // 计算K线数量

	WTSBarStruct* firstBar = (WTSBarStruct*)buffer.data();                     // 获取K线数据起始指针
	if (barcnt > 0)                                                             // 如果K线数量大于0
	{

		uint32_t sIdx = 0;                                                      // 开始索引
		uint32_t idx = barcnt - 1;                                              // 结束索引（最后一条）
		uint32_t curCnt = (idx - sIdx + 1);                                     // 计算K线数量

		std::vector<WTSBarStruct>* tempAy = new std::vector<WTSBarStruct>();     // 创建临时K线向量
		tempAy->resize(curCnt);                                                  // 调整大小
		memcpy(tempAy->data(), &firstBar[sIdx], sizeof(WTSBarStruct)*curCnt);  // 复制K线数据
		realCnt += curCnt;                                                      // 累计实际数量

		barsSections.emplace_back(tempAy);                                      // 添加到分段列表
	}

	if (realCnt > 0)                                                            // 如果实际数量大于0
	{
		barsList->_bars.resize(realCnt);                                        // 调整K线列表大小

		uint32_t curIdx = 0;                                                    // 当前索引
		for (auto it = barsSections.rbegin(); it != barsSections.rend(); it++)  // 从后往前遍历分段列表（从最新到最旧）
		{
			std::vector<WTSBarStruct>* tempAy = *it;                            // 获取分段K线向量
			memcpy(barsList->_bars.data() + curIdx, tempAy->data(), tempAy->size()*sizeof(WTSBarStruct));  // 复制分段数据到最终列表
			curIdx += tempAy->size();                                           // 更新当前索引
			delete tempAy;                                                      // 释放临时向量
		}
		barsList->_count = barsList->_bars.size();                              // 设置K线数量
		barsSections.clear();                                                   // 清空分段列表
	}

	WTSLogger::info("{} items of back {} data of {} cached", realCnt, PERIOD_NAME[period], stdCode);  // 记录信息日志
	return true;                                                                // 返回成功
}

/**
 * @brief 检查并清理过期缓存
 * 
 * 根据配置的缓存清理天数，清理长时间未使用的K线缓存
 */
void HisDataReplayer::check_cache_days()
{
	if (_cache_clear_days == 0)                                          // 如果缓存清理天数为0（不清理）
		return;                                                          // 直接返回

	std::set<std::string> to_clear;                                     // 待清理的缓存键集合
	std::string codes;                                                   // 待清理的合约代码字符串（用于日志）
	for(auto& v : _bars_cache)                                           // 遍历所有K线缓存
	{
		if(v.first == _main_key)                                         // 如果是主K线（主键）
			continue;                                                    // 跳过（主K线不清理）

		BarsListPtr& barsList = (BarsListPtr&)v.second;                 // 获取K线列表指针
		barsList->_untouch_days++;                                        // 增加未使用天数

		if (barsList->_untouch_days >= _cache_clear_days)                // 如果未使用天数大于等于清理天数
		{
			to_clear.insert(v.first);                                    // 添加到待清理集合
			if (codes.size() > 0)                                        // 如果代码字符串不为空
				codes += ",";                                            // 添加逗号分隔符
			codes += v.first;                                            // 添加缓存键
		}
	}

	for (const std::string& key : to_clear)                              // 遍历待清理的缓存键
		_bars_cache.erase(key);                                          // 从缓存中删除

	WTSLogger::info("Cached bars of {} cleared due to outdated", codes);  // 记录日志
}