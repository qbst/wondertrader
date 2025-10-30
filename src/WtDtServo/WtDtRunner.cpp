/*!
 * \file WtDtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据服务运行器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WtDtRunner（数据服务运行器）类的具体实现，提供了数据服务运行器的完整功能实现。
 * 该文件实现了数据服务的初始化、启动、数据查询、实时数据处理、订阅管理等核心功能。
 * 
 * 核心实现机制：
 * 
 * 1. 初始化流程（Initialization Flow）：
 *    - 加载配置文件（支持文件路径和配置内容字符串）
 *    - 初始化日志系统
 *    - 加载基础数据（交易时段、商品信息、合约信息、节假日等）
 *    - 加载主力合约规则和次主力合约规则
 *    - 初始化数据管理器和解析器适配器
 * 
 * 2. 订阅管理（Subscription Management）：
 *    - 管理外部订阅和内部订阅两个订阅表
 *    - 使用互斥锁保护订阅表的并发访问
 *    - 支持订阅替换和追加模式
 * 
 * 3. 事件处理（Event Handling）：
 *    - 处理Tick数据推送，更新实时K线
 *    - 检查订阅状态，触发相应的回调函数
 *    - 支持Tick和K线两种回调类型
 * 
 * 4. 数据查询（Data Query）：
 *    - 将查询请求转发给数据管理器
 *    - 处理周期字符串到枚举的转换
 *    - 封装查询结果并返回
 * 
 * 主要功能实现：
 * 
 * 1. 初始化和启动：
 *    - 构造函数和析构函数
 *    - initialize()：初始化数据服务
 *    - start()：启动数据服务
 *    - initDataMgr()：初始化数据管理器
 *    - initParsers()：初始化解析器
 * 
 * 2. 数据查询：
 *    - 各种数据查询接口的实现
 *    - 周期字符串解析和转换
 *    - 查询结果封装和返回
 * 
 * 3. 实时数据处理：
 *    - proc_tick()：处理Tick数据
 *    - trigger_tick()：触发Tick回调
 *    - trigger_bar()：触发K线回调
 * 
 * 4. 订阅管理：
 *    - sub_tick()：订阅Tick数据
 *    - sub_bar()：订阅K线数据
 *    - clear_cache()：清理缓存
 * 
 * 使用场景：
 * - 数据服务的统一入口
 * - 实时行情数据处理
 * - 历史数据查询
 * - 订阅管理
 * 
 * 技术特点：
 * - 组件化设计
 * - 统一的接口抽象
 * - 灵活的订阅机制
 * - 高效的数据查询
 * 
 * 注意事项：
 * - 必须先调用initialize()进行初始化
 * - 订阅管理使用互斥锁保护
 * - 数据查询结果需要调用release()释放
 * - 回调函数应该快速返回
 */
#include "WtDtRunner.h"                                                         // 包含数据服务运行器头文件

#include "../WtDtCore/WtHelper.h"                                               // 包含WtDtCore模块的辅助工具（用于获取模块目录）
#include "../Includes/WTSSessionInfo.hpp"                                        // 包含交易时段信息类
#include "../Includes/WTSVariant.hpp"                                            // 包含配置变体类
#include "../Includes/WTSDataDef.hpp"                                            // 包含数据结构定义
#include "../Includes/WTSContractInfo.hpp"                                       // 包含合约信息类

#include "../WTSUtils/SignalHook.hpp"                                            // 包含信号处理工具（用于崩溃转储等）
#include "../WTSUtils/WTSCfgLoader.h"                                            // 包含配置加载器
#include "../WTSTools/WTSLogger.h"                                               // 包含日志工具类

#include "../Share/StrUtil.hpp"                                                 // 包含字符串工具类
#include "../Share/StdUtils.hpp"                                                 // 包含标准工具类
#include "../Share/CodeHelper.hpp"                                               // 包含代码解析工具

USING_NS_WTP;                                                                    // 使用WonderTrader命名空间

/**
 * @brief WtDtRunner构造函数
 * 
 * 初始化数据服务运行器对象，设置初始化标志为false。
 * 安装信号处理钩子，用于在程序崩溃时记录错误信息。
 */
WtDtRunner::WtDtRunner()
	: _data_store(NULL)                                                          // 数据存储对象指针初始化为NULL
	, _is_inited(false)                                                          // 初始化标志初始化为false
{
	install_signal_hooks([](const char* message) {                               // 安装信号处理钩子（用于处理崩溃信号）
		WTSLogger::error(message);                                               // 记录错误日志
	});
}

/**
 * @brief WtDtRunner析构函数
 * 
 * 清理数据服务运行器资源。
 * 注意：应在所有组件停止后调用。
 */
WtDtRunner::~WtDtRunner()
{
}
#ifdef _MSC_VER                                                                 // 如果是Windows平台
#include "../Common/mdump.h"                                                    // 包含Windows崩溃转储工具
extern const char* getModuleName();                                             // 外部函数声明：获取模块名称（在WtDtServo.cpp中定义）
#endif

/**
 * @brief 初始化数据服务运行器
 * @param cfgFile 配置文件路径或配置内容
 * @param isFile 是否为文件路径（true=文件路径，false=配置内容字符串）
 * @param modDir 模块目录路径
 * @param logCfg 日志配置文件路径
 * @param cbTick Tick数据回调函数
 * @param cbBar K线数据回调函数
 * 
 * 初始化数据服务运行器，包括：
 * 1. 加载配置文件
 * 2. 初始化日志系统
 * 3. 加载基础数据（交易时段、商品、合约、节假日等）
 * 4. 加载主力合约规则和次主力合约规则
 * 5. 初始化数据管理器
 * 6. 初始化解析器
 * 7. 启动服务
 */
void WtDtRunner::initialize(const char* cfgFile, bool isFile /* = true */, const char* modDir /* = "" */, const char* logCfg /* = "logcfg.yaml" */, 
			FuncOnTickCallback cbTick /* = NULL */, FuncOnBarCallback cbBar /* = NULL */)
{
	if(_is_inited)                                                                // 如果已经初始化过
	{
		WTSLogger::error("WtDtServo has already been initialized");              // 记录错误日志：服务已初始化
		return;                                                                    // 直接返回，不重复初始化
	}

	_cb_tick = cbTick;                                                            // 保存Tick数据回调函数指针
	_cb_bar = cbBar;                                                              // 保存K线数据回调函数指针

	WTSLogger::init(logCfg);                                                      // 初始化日志系统（使用指定的日志配置文件）
	WtHelper::set_module_dir(modDir);                                             // 设置模块目录路径（用于动态库加载）

	WTSVariant* config = isFile ? WTSCfgLoader::load_from_file(cfgFile) : WTSCfgLoader::load_from_content(cfgFile, false);  // 加载配置文件（根据isFile参数决定是从文件加载还是从字符串加载）
	if(config == NULL)                                                            // 如果加载失败
	{
		WTSLogger::error("Loading config failed");                               // 记录错误日志：配置文件加载失败
		WTSLogger::log_raw(LL_INFO, cfgFile);                                    // 记录配置路径或内容（用于调试）
		return;                                                                    // 直接返回，初始化失败
	}

	if(!config->getBoolean("disable_dump"))                                       // 如果配置中没有禁用崩溃转储（enable_dump为true或未配置）
	{
#ifdef _MSC_VER                                                                   // 如果是Windows平台
		CMiniDumper::Enable(getModuleName(), true, WtHelper::get_cwd());        // 启用崩溃转储功能（程序崩溃时自动生成dump文件）
#endif
	}
	//基础数据文件
	WTSVariant* cfgBF = config->get("basefiles");                                 // 从配置中获取基础数据文件配置节点
	if (cfgBF->get("session"))                                                    // 如果配置了交易时段文件
	{
		_bd_mgr.loadSessions(cfgBF->getCString("session"));                     // 加载交易时段数据（从指定文件加载）
		WTSLogger::info("Trading sessions loaded");                               // 记录信息日志：交易时段已加载
	}

	WTSVariant* cfgItem = cfgBF->get("commodity");                                // 从配置中获取商品数据文件配置
	if (cfgItem)                                                                   // 如果配置了商品数据文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)                            // 如果配置项是字符串类型（单个文件）
		{
			_bd_mgr.loadCommodities(cfgItem->asCString());                      // 加载商品数据（从单个文件加载）
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)                       // 如果配置项是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)                       // 遍历数组中的所有文件
			{
				_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());         // 加载每个商品数据文件
			}
		}
	}

	cfgItem = cfgBF->get("contract");                                              // 从配置中获取合约数据文件配置
	if (cfgItem)                                                                   // 如果配置了合约数据文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)                            // 如果配置项是字符串类型（单个文件）
		{
			_bd_mgr.loadContracts(cfgItem->asCString());                       // 加载合约数据（从单个文件加载）
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)                       // 如果配置项是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)                       // 遍历数组中的所有文件
			{
				_bd_mgr.loadContracts(cfgItem->get(i)->asCString());            // 加载每个合约数据文件
			}
		}
	}

	if (cfgBF->get("holiday"))                                                     // 如果配置了节假日文件
	{
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));                     // 加载节假日数据（从指定文件加载）
		WTSLogger::info("Holidays loaded");                                       // 记录信息日志：节假日已加载
	}

	if (cfgBF->get("hot"))                                                         // 如果配置了主力合约规则文件
	{
		_hot_mgr.loadHots(cfgBF->getCString("hot"));                            // 加载主力合约规则（从指定文件加载）
		WTSLogger::info("Hot rules loaded");                                      // 记录信息日志：主力合约规则已加载
	}

	if (cfgBF->get("second"))                                                      // 如果配置了次主力合约规则文件
	{
		_hot_mgr.loadSeconds(cfgBF->getCString("second"));                    // 加载次主力合约规则（从指定文件加载）
		WTSLogger::info("Second rules loaded");                                  // 记录信息日志：次主力合约规则已加载
	}

	WTSArray* ayContracts = _bd_mgr.getContracts();                               // 获取所有合约列表
	for (auto it = ayContracts->begin(); it != ayContracts->end(); it++)          // 遍历所有合约
	{
		WTSContractInfo* cInfo = (WTSContractInfo*)(*it);                        // 获取合约信息指针
		bool isHot = _hot_mgr.isHot(cInfo->getExchg(), cInfo->getCode());        // 判断该合约是否为主力合约
		bool isSecond = _hot_mgr.isSecond(cInfo->getExchg(), cInfo->getCode());  // 判断该合约是否为次主力合约

		std::string hotCode = cInfo->getFullPid();                                // 获取合约的完整品种ID（格式：交易所.品种）
		if (isHot)                                                                // 如果是主力合约
			hotCode += ".HOT";                                                    // 添加.HOT后缀，如"SHFE.ag.HOT"
		else if (isSecond)                                                        // 如果是次主力合约
			hotCode += ".2ND";                                                    // 添加.2ND后缀，如"SHFE.ag.2ND"
		else                                                                       // 如果既不是主力也不是次主力
			hotCode = "";                                                         // 清空hotCode（表示无效）

		cInfo->setHotFlag(isHot ? 1 : (isSecond ? 2 : 0), hotCode.c_str());     // 设置合约的主力标志（1=主力，2=次主力，0=普通）和主力代码
	}
	ayContracts->release();                                                       // 释放合约列表数组对象

	initDataMgr(config->get("data"));                                             // 初始化数据管理器（从配置中获取data配置节点）

	WTSVariant* cfgParser = config->get("parsers");                                // 从配置中获取解析器配置
	if (cfgParser)                                                                 // 如果配置了解析器
	{
		if (cfgParser->type() == WTSVariant::VT_String)                          // 如果配置项是字符串类型（解析器配置文件路径）
		{
			const char* filename = cfgParser->asCString();                        // 获取配置文件路径
			if (StdFile::exists(filename))                                        // 如果配置文件存在
			{
				WTSLogger::info("Reading parser config from {}...", filename);   // 记录信息日志：开始读取解析器配置
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);        // 加载解析器配置文件
				if (var)                                                          // 如果加载成功
				{
					initParsers(var->get("parsers"));                            // 初始化解析器（从配置的parsers节点）
					var->release();                                               // 释放配置对象
				}
				else                                                              // 如果加载失败
				{
					WTSLogger::error("Loading parser config {} failed", filename);  // 记录错误日志：解析器配置文件加载失败
				}
			}
			else                                                                   // 如果配置文件不存在
			{
				WTSLogger::error("Parser configuration {} not exists", filename);  // 记录错误日志：解析器配置文件不存在
			}
		}
		else if (cfgParser->type() == WTSVariant::VT_Array)                      // 如果配置项是数组类型（直接配置解析器列表）
		{
			initParsers(cfgParser);                                               // 直接初始化解析器（使用配置数组）
		}
	}
	else                                                                           // 如果没有配置解析器
		WTSLogger::log_raw(LL_WARN, "No parsers config, skipped loading parsers");  // 记录警告日志：没有解析器配置，跳过加载

	config->release();                                                             // 释放配置对象

	start();                                                                       // 启动数据服务（启动所有解析器）

	_is_inited = true;                                                             // 设置初始化标志为true（表示初始化成功）
}

/**
 * @brief 初始化数据管理器
 * @param config 配置信息（数据管理器配置节点）
 * 
 * 初始化数据管理器，用于读取历史数据。
 * 配置信息应包含数据存储模块的配置。
 */
void WtDtRunner::initDataMgr(WTSVariant* config)
{
	if (config == NULL)                                                           // 如果配置信息为空
		return;                                                                    // 直接返回，不初始化

	_data_mgr.init(config, this);                                                 // 初始化数据管理器（传入配置信息和this指针）
	WTSLogger::info("Data manager initialized");                                  // 记录信息日志：数据管理器已初始化
}

/**
 * @brief 按时间范围查询K线数据
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m1"=1分钟，"m5"=5分钟，"d"=日线）
 * @param beginTime 开始时间（格式：yyyymmddHHMMSS）
 * @param endTime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的K线数据。
 * 会将周期字符串解析为周期枚举和倍数，然后调用数据管理器查询。
 */
WTSKlineSlice* WtDtRunner::get_bars_by_range(const char* stdCode, const char* period, uint64_t beginTime, uint64_t endTime /* = 0 */)
{
	if(!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	thread_local static char basePeriod[2] = { 0 };                                // 线程局部静态变量：基础周期字符（如'm'或'd'）
	basePeriod[0] = period[0];                                                     // 获取周期字符串的第一个字符（'m'或'd'）
	uint32_t times = 1;                                                            // 周期倍数（默认为1）
	if (strlen(period) > 1)                                                        // 如果周期字符串长度大于1（有倍数）
		times = strtoul(period + 1, NULL, 10);                                      // 解析倍数（如"m5"中的5）

	WTSKlinePeriod kp;                                                             // K线周期枚举
	uint32_t realTimes = times;                                                    // 实际倍数（用于数据管理器）
	if (basePeriod[0] == 'm')                                                      // 如果是分钟周期（'m'开头）
	{
		if (times % 5 == 0)                                                        // 如果倍数是5的倍数（如5、10、15分钟等）
		{
			kp = KP_Minute5;                                                       // 使用5分钟作为基础周期
			realTimes /= 5;                                                        // 倍数除以5（如15分钟=5分钟*3）
		}
		else                                                                       // 如果倍数不是5的倍数
		{
			kp = KP_Minute1;                                                       // 使用1分钟作为基础周期
		}
	}
	else                                                                           // 如果是日线周期（'d'开头）
		kp = KP_DAY;                                                                // 设置为日线周期

	if (endTime == 0)                                                               // 如果结束时间为0（表示不限制）
	{
		uint32_t curDate = TimeUtils::getCurDate();                               // 获取当前交易日期
		endTime = (uint64_t)curDate * 10000 + 2359;                                // 设置结束时间为当天23:59（格式：yyyymmddHHMM）
	}

	return _data_mgr.get_kline_slice_by_range(stdCode, kp, realTimes, beginTime, endTime);  // 调用数据管理器查询K线数据切片
}

/**
 * @brief 按日期查询K线数据
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m1"=1分钟，"m5"=5分钟）
 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定交易日的K线数据。
 * 注意：此方法只支持分钟周期，不支持日线周期。
 */
WTSKlineSlice* WtDtRunner::get_bars_by_date(const char* stdCode, const char* period, uint32_t uDate /* = 0 */)
{
	if (!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	thread_local static char basePeriod[2] = { 0 };                                // 线程局部静态变量：基础周期字符
	basePeriod[0] = period[0];                                                     // 获取周期字符串的第一个字符
	uint32_t times = 1;                                                            // 周期倍数（默认为1）
	if (strlen(period) > 1)                                                        // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);                                      // 解析倍数

	WTSKlinePeriod kp;                                                             // K线周期枚举
	uint32_t realTimes = times;                                                    // 实际倍数
	if (basePeriod[0] == 'm')                                                      // 如果是分钟周期
	{
		if (times % 5 == 0)                                                        // 如果倍数是5的倍数
		{
			kp = KP_Minute5;                                                       // 使用5分钟作为基础周期
			realTimes /= 5;                                                        // 倍数除以5
		}
		else                                                                       // 如果倍数不是5的倍数
		{
			kp = KP_Minute1;                                                       // 使用1分钟作为基础周期
		}
	}
	else                                                                           // 如果不是分钟周期（如日线）
	{
		WTSLogger::log_raw(LL_ERROR, "get_bars_by_date only supports minute period");  // 记录错误日志：只支持分钟周期
		return NULL;                                                                // 返回NULL表示不支持
	}

	if (uDate == 0)                                                                 // 如果日期为0（表示今天）
	{
		uDate = TimeUtils::getCurDate();                                          // 获取当前交易日期
	}

	return _data_mgr.get_kline_slice_by_date(stdCode, kp, realTimes, uDate);     // 调用数据管理器查询指定日期的K线数据切片
}

/**
 * @brief 按时间范围查询Tick数据
 * @param stdCode 标准化合约代码
 * @param beginTime 开始时间（格式：yyyymmddHHMMSS）
 * @param endTime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
 * @return Tick数据切片指针（需要调用release()释放）
 * 
 * 查询指定时间范围内的Tick数据。
 */
WTSTickSlice* WtDtRunner::get_ticks_by_range(const char* stdCode, uint64_t beginTime, uint64_t endTime /* = 0 */)
{
	if (!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	if(endTime == 0)                                                                // 如果结束时间为0（表示不限制）
	{
		uint32_t curDate = TimeUtils::getCurDate();                               // 获取当前交易日期
		endTime = (uint64_t)curDate * 10000 + 2359;                                // 设置结束时间为当天23:59
	}
	return _data_mgr.get_tick_slices_by_range(stdCode, beginTime, endTime);       // 调用数据管理器查询Tick数据切片
}

/**
 * @brief 按日期查询Tick数据
 * @param stdCode 标准化合约代码
 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
 * @return Tick数据切片指针（需要调用release()释放）
 * 
 * 查询指定交易日的所有Tick数据。
 */
WTSTickSlice* WtDtRunner::get_ticks_by_date(const char* stdCode, uint32_t uDate /* = 0 */)
{
	if (!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	return _data_mgr.get_tick_slice_by_date(stdCode, uDate);                     // 调用数据管理器查询指定日期的Tick数据切片
}

/**
 * @brief 按数量查询K线数据
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m1"=1分钟，"m5"=5分钟，"d"=日线）
 * @param count 查询条数
 * @param endTime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定数量的K线数据（从结束时间向前查找）。
 */
WTSKlineSlice* WtDtRunner::get_bars_by_count(const char* stdCode, const char* period, uint32_t count, uint64_t endTime /* = 0 */)
{
	if (!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	thread_local static char basePeriod[2] = { 0 };                                // 线程局部静态变量：基础周期字符
	basePeriod[0] = period[0];                                                     // 获取周期字符串的第一个字符
	uint32_t times = 1;                                                            // 周期倍数（默认为1）
	if (strlen(period) > 1)                                                        // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);                                      // 解析倍数

	WTSKlinePeriod kp;                                                             // K线周期枚举
	uint32_t realTimes = times;                                                    // 实际倍数
	if (basePeriod[0] == 'm')                                                      // 如果是分钟周期
	{
		if (times % 5 == 0)                                                        // 如果倍数是5的倍数
		{
			kp = KP_Minute5;                                                       // 使用5分钟作为基础周期
			realTimes /= 5;                                                        // 倍数除以5
		}
		else                                                                       // 如果倍数不是5的倍数
		{
			kp = KP_Minute1;                                                       // 使用1分钟作为基础周期
		}
	}
	else                                                                           // 如果是日线周期
		kp = KP_DAY;                                                                // 设置为日线周期

	if (endTime == 0)                                                               // 如果结束时间为0（表示不限制）
	{
		uint32_t curDate = TimeUtils::getCurDate();                               // 获取当前交易日期
		endTime = (uint64_t)curDate * 10000 + 2359;                                // 设置结束时间为当天23:59
	}

	return _data_mgr.get_kline_slice_by_count(stdCode, kp, realTimes, count, endTime);  // 调用数据管理器查询指定数量的K线数据切片
}

/**
 * @brief 按数量查询Tick数据
 * @param stdCode 标准化合约代码
 * @param count 查询条数
 * @param endTime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
 * @return Tick数据切片指针（需要调用release()释放）
 * 
 * 查询指定数量的Tick数据（从结束时间向前查找）。
 */
WTSTickSlice* WtDtRunner::get_ticks_by_count(const char* stdCode, uint32_t count, uint64_t endTime /* = 0 */)
{
	if (!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	if (endTime == 0)                                                               // 如果结束时间为0（表示不限制）
	{
		uint32_t curDate = TimeUtils::getCurDate();                               // 获取当前交易日期
		endTime = (uint64_t)curDate * 10000 + 2359;                                // 设置结束时间为当天23:59
	}
	return _data_mgr.get_tick_slice_by_count(stdCode, count, endTime);            // 调用数据管理器查询指定数量的Tick数据切片
}

/**
 * @brief 按日期查询秒级K线数据
 * @param stdCode 标准化合约代码
 * @param secs 秒数（如：60表示60秒K线）
 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
 * @return K线数据切片指针（需要调用release()释放）
 * 
 * 查询指定交易日的秒级K线数据（从Tick数据生成）。
 */
WTSKlineSlice* WtDtRunner::get_sbars_by_date(const char* stdCode, uint32_t secs, uint32_t uDate /* = 0 */)
{
	if (!_is_inited)                                                                // 如果服务未初始化
	{
		WTSLogger::error("WtDtServo not initialized");                            // 记录错误日志：服务未初始化
		return NULL;                                                                // 返回NULL表示查询失败
	}

	return _data_mgr.get_skline_slice_by_date(stdCode, secs, uDate);             // 调用数据管理器查询秒级K线数据切片
}

/**
 * @brief 初始化解析器
 * @param cfg 配置信息（解析器配置数组）
 * 
 * 根据配置信息初始化解析器适配器。
 * 会遍历配置数组，为每个活跃的解析器创建适配器并初始化。
 * 如果解析器没有配置ID，会自动生成一个ID。
 */
void WtDtRunner::initParsers(WTSVariant* cfg)
{
	for (uint32_t idx = 0; idx < cfg->size(); idx++)                              // 遍历配置数组中的所有解析器配置
	{
		WTSVariant* cfgItem = cfg->get(idx);                                       // 获取第idx个解析器配置项
		if (!cfgItem->getBoolean("active"))                                       // 如果解析器未激活（active=false）
			continue;                                                               // 跳过该解析器，继续处理下一个

		const char* id = cfgItem->getCString("id");                              // 获取解析器ID

		// By Wesley @ 2021.12.14
		// 如果id为空，则生成自动id
		std::string realid = id;                                                  // 保存解析器ID字符串
		if (realid.empty())                                                        // 如果ID为空
		{
			static uint32_t auto_parserid = 1000;                                 // 静态变量：自动生成的解析器ID计数器（从1000开始）
			realid = StrUtil::printf("auto_parser_%u", auto_parserid++);          // 生成自动ID（格式：auto_parser_1000、auto_parser_1001等）
		}

		ParserAdapterPtr adapter(new ParserAdapter(&_bd_mgr, this));             // 创建解析器适配器对象（传入基础数据管理器和this指针）
		adapter->init(realid.c_str(), cfgItem);                                   // 初始化适配器（传入ID和配置信息）
		_parsers.addAdapter(realid.c_str(), adapter);                            // 将适配器添加到解析器管理器
	}

	WTSLogger::info("{} market data parsers loaded in total", _parsers.size());   // 记录信息日志：显示总共加载了多少个解析器
}

/**
 * @brief 启动数据服务
 * 
 * 启动所有解析器，开始接收行情数据。
 * 实际是调用解析器管理器的run()方法，启动所有已注册的解析器。
 */
void WtDtRunner::start()
{
	_parsers.run();                                                                 // 调用解析器管理器的run()方法，启动所有解析器
}

/**
 * @brief 处理Tick数据
 * @param curTick 当前Tick数据指针
 * 
 * 处理从解析器接收到的Tick数据。
 * 主要功能：
 * 1. 获取或设置合约信息
 * 2. 将原始合约代码转换为标准化代码
 * 3. 触发Tick回调（触发外部订阅）
 * 4. 如果是分月合约，还要触发主力合约代码的Tick回调
 */
void WtDtRunner::proc_tick(WTSTickData* curTick)
{
	WTSContractInfo* cInfo = curTick->getContractInfo();                         // 获取Tick数据中的合约信息
	if (cInfo == NULL)                                                             // 如果合约信息为空
	{
		cInfo = _bd_mgr.getContract(curTick->code(), curTick->exchg());           // 根据合约代码和交易所代码从基础数据管理器获取合约信息
		curTick->setContractInfo(cInfo);                                          // 设置合约信息到Tick数据中（避免重复查找）
	}

	if (cInfo == NULL)                                                             // 如果仍然找不到合约信息
		return;                                                                     // 直接返回，不处理未知合约的数据

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();                            // 获取商品信息
	WTSSessionInfo* sInfo = commInfo->getSessionInfo();                           // 获取交易时段信息

	uint32_t hotflag = 0;                                                          // 主力标志（当前未使用，保留用于未来扩展）

	std::string stdCode;                                                           // 标准化合约代码
	if (commInfo->getCategoty() == CC_FutOption)                                  // 如果是期货期权品种
	{
		stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());  // 将期货期权原始代码转换为标准化代码
	}
	else if (CodeHelper::isMonthlyCode(curTick->code()))                          // 如果是分月合约（如IF2005）
	{
		//如果是分月合约，则进行主力和次主力的判断
		stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());  // 将分月合约原始代码转换为标准化代码（如SHFE.ag.1912）
	}
	else                                                                           // 如果是其他类型合约（如股票）
	{
		stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());  // 将扁平合约代码转换为标准化代码（如SSE.600000）
	}
	curTick->setCode(stdCode.c_str());                                             // 设置标准化代码到Tick数据中

	trigger_tick(stdCode.c_str(), curTick);                                        // 触发Tick回调（处理外部订阅和内部订阅）

	if (!cInfo->isFlat())                                                          // 如果不是扁平合约（即分月合约）
	{
		const char* hotCode = cInfo->getHotCode();                                 // 获取主力合约代码（如SHFE.ag.HOT）
		WTSTickData* hotTick = WTSTickData::create(curTick->getTickStruct());     // 创建新的Tick数据对象（复制原始Tick数据）
		hotTick->setCode(hotCode);                                                 // 设置主力合约代码
		hotTick->setContractInfo(curTick->getContractInfo());                     // 设置合约信息

		trigger_tick(hotCode, hotTick);                                            // 触发主力合约的Tick回调（让订阅主力合约的客户端也能收到数据）

		hotTick->release();                                                         // 释放临时创建的Tick数据对象
	}
	//else if (hotflag == 2)                                                       // 如果是次主力合约（当前代码已注释，保留用于未来扩展）
	//{
	//	std::string scndCode = CodeHelper::stdCodeToStd2ndCode(stdCode.c_str());  // 转换为次主力合约代码
	//	WTSTickData* scndTick = WTSTickData::create(curTick->getTickStruct());    // 创建新的Tick数据对象
	//	scndTick->setCode(scndCode.c_str());                                        // 设置次主力合约代码
	//	scndTick->setContractInfo(curTick->getContractInfo());                    // 设置合约信息

	//	trigger_tick(scndCode.c_str(), scndTick);                                  // 触发次主力合约的Tick回调

	//	scndTick->release();                                                       // 释放临时创建的Tick数据对象
	//}
}

/**
 * @brief 触发Tick回调
 * @param stdCode 标准化合约代码
 * @param curTick 当前Tick数据指针
 * 
 * 检查订阅状态，触发相应的回调函数。
 * 处理两种订阅：
 * 1. 外部订阅（_tick_sub_map）：触发外部回调函数（_cb_tick）
 * 2. 内部订阅（_tick_innersub_map）：更新实时K线数据（_data_mgr.update_bars）
 * 
 * 支持三种订阅标志：
 * - flag=0：原始数据（不含复权）
 * - flag=1：前复权数据（QFQ后缀）
 * - flag=2：后复权数据（HFQ后缀，需要乘以复权因子）
 */
void WtDtRunner::trigger_tick(const char* stdCode, WTSTickData* curTick)
{
	if (_cb_tick != NULL)                                                          // 如果设置了外部Tick回调函数
	{
		StdUniqueLock lock(_mtx_subs);                                             // 获取外部订阅映射表的互斥锁（线程安全）
		auto sit = _tick_sub_map.find(stdCode);                                    // 在外部订阅映射表中查找该合约的订阅标志
		if (sit != _tick_sub_map.end())                                            // 如果找到了订阅记录
		{
			SubFlags flags = sit->second;                                          // 获取订阅标志集合（可能包含多个标志）
			for (uint32_t flag : flags)                                            // 遍历所有订阅标志
			{
				if (flag == 0)                                                     // 如果标志为0（原始数据）
				{
					_cb_tick(stdCode, &curTick->getTickStruct());                 // 直接调用外部回调函数，传入原始数据
				}
				else                                                               // 如果标志为1或2（复权数据）
				{
					std::string wCode = fmtutil::format("{}{}", stdCode, (flag == 1) ? SUFFIX_QFQ : SUFFIX_HFQ);  // 构建复权代码（添加QFQ或HFQ后缀）
					if (flag == 1)                                                  // 如果标志为1（前复权）
					{
						_cb_tick(wCode.c_str(), &curTick->getTickStruct());       // 调用外部回调函数，传入前复权代码和原始数据
					}
					else //(flag == 2)                                             // 如果标志为2（后复权）
					{
						WTSTickData* newTick = WTSTickData::create(curTick->getTickStruct());  // 创建新的Tick数据对象（复制原始数据）
						WTSTickStruct& newTS = newTick->getTickStruct();           // 获取Tick结构的引用（用于修改）
						newTick->setContractInfo(curTick->getContractInfo());      // 设置合约信息

						//这里做一个复权因子的处理
						double factor = _data_mgr.get_exright_factor(stdCode, curTick->getContractInfo()->getCommInfo());  // 获取复权因子
						newTS.open *= factor;                                      // 开盘价乘以复权因子
						newTS.high *= factor;                                      // 最高价乘以复权因子
						newTS.low *= factor;                                       // 最低价乘以复权因子
						newTS.price *= factor;                                     // 最新价乘以复权因子

						newTS.settle_price *= factor;                              // 结算价乘以复权因子

						newTS.pre_close *= factor;                                 // 昨收价乘以复权因子
						newTS.pre_settle *= factor;                                // 昨结算价乘以复权因子

						_cb_tick(wCode.c_str(), &newTS);                           // 调用外部回调函数，传入后复权代码和复权后的数据
						newTick->release();                                        // 释放临时创建的Tick数据对象
					}
				}
			}

		}
	}

	{
		StdUniqueLock lock(_mtx_innersubs);                                        // 获取内部订阅映射表的互斥锁（线程安全）
		auto sit = _tick_innersub_map.find(stdCode);                              // 在内部订阅映射表中查找该合约的订阅标志
		if (sit == _tick_innersub_map.end())                                       // 如果没有找到订阅记录
			return;                                                                 // 直接返回，不处理内部订阅

		SubFlags flags = sit->second;                                              // 获取订阅标志集合
		for (uint32_t flag : flags)                                                // 遍历所有订阅标志
		{
			if (flag == 0)                                                         // 如果标志为0（原始数据）
			{
				_data_mgr.update_bars(stdCode, curTick);                          // 更新实时K线数据（使用原始数据）
			}
			else                                                                    // 如果标志为1或2（复权数据）
			{
				std::string wCode = fmtutil::format("{}{}", stdCode, (flag == 1) ? SUFFIX_QFQ : SUFFIX_HFQ);  // 构建复权代码
				curTick->setCode(wCode.c_str());                                   // 临时修改Tick数据的代码（用于更新K线）
				if (flag == 1)                                                      // 如果标志为1（前复权）
				{
					_data_mgr.update_bars(wCode.c_str(), curTick);                 // 更新实时K线数据（使用前复权代码和原始数据）
				}
				else //(flag == 2)                                                  // 如果标志为2（后复权）
				{
					WTSTickData* newTick = WTSTickData::create(curTick->getTickStruct());  // 创建新的Tick数据对象
					WTSTickStruct& newTS = newTick->getTickStruct();              // 获取Tick结构的引用
					newTick->setContractInfo(curTick->getContractInfo());          // 设置合约信息

					//这里做一个复权因子的处理
					double factor = _data_mgr.get_exright_factor(stdCode, curTick->getContractInfo()->getCommInfo());  // 获取复权因子
					newTS.open *= factor;                                          // 开盘价乘以复权因子
					newTS.high *= factor;                                          // 最高价乘以复权因子
					newTS.low *= factor;                                           // 最低价乘以复权因子
					newTS.price *= factor;                                         // 最新价乘以复权因子

					newTS.settle_price *= factor;                                  // 结算价乘以复权因子

					newTS.pre_close *= factor;                                     // 昨收价乘以复权因子
					newTS.pre_settle *= factor;                                    // 昨结算价乘以复权因子

					_data_mgr.update_bars(wCode.c_str(), newTick);                // 更新实时K线数据（使用后复权代码和复权后的数据）
					newTick->release();                                            // 释放临时创建的Tick数据对象
				}
			}
		}
	}
}

/**
 * @brief 订阅Tick数据
 * @param codes 合约代码列表（逗号分隔）
 * @param bReplace 是否替换现有订阅（true=替换，false=追加）
 * @param bInner 是否为内部订阅（true=内部订阅，false=外部订阅）
 * 
 * 订阅指定合约的Tick数据。
 * 支持三种订阅模式：
 * - 原始数据：直接订阅合约代码
 * - 前复权数据：订阅代码+QFQ后缀（如"SSE.600000^"）
 * - 后复权数据：订阅代码+HFQ后缀（如"SSE.600000$"）
 */
void WtDtRunner::sub_tick(const char* codes, bool bReplace, bool bInner /* = false */)
{
	if(bInner)                                                                      // 如果是内部订阅
	{
		StdUniqueLock lock(_mtx_innersubs);                                       // 获取内部订阅映射表的互斥锁（线程安全）
		if (bReplace)                                                              // 如果需要替换现有订阅
			_tick_innersub_map.clear();                                           // 清空内部订阅映射表

		const char* stdCode = codes;                                               // 获取合约代码字符串（单个代码，因为是内部订阅）
		std::size_t length = strlen(stdCode);                                      // 获取代码长度
		uint32_t flag = 0;                                                         // 订阅标志（0=原始，1=前复权，2=后复权）
		if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果代码最后一个字符是复权后缀
		{
			length--;                                                               // 长度减1（去掉后缀字符）

			flag = (stdCode[length] == SUFFIX_QFQ) ? 1 : 2;                        // 设置订阅标志（1=前复权，2=后复权）
		}

		SubFlags& flags = _tick_innersub_map[std::string(stdCode, length)];        // 获取或创建订阅标志集合（键为去掉后缀的代码）
		flags.insert(flag);                                                        // 将订阅标志添加到集合中
		WTSLogger::info("Tick dada of {} subscribed with flag {} for inner use", stdCode, flag);  // 记录信息日志：内部订阅成功
	}
	else                                                                           // 如果是外部订阅
	{
		StdUniqueLock lock(_mtx_subs);                                             // 获取外部订阅映射表的互斥锁（线程安全）
		if (bReplace)                                                              // 如果需要替换现有订阅
			_tick_sub_map.clear();                                                 // 清空外部订阅映射表

		StringVector ayCodes = StrUtil::split(codes, ",");                        // 按逗号分割代码列表，得到代码数组
		for (const std::string& code : ayCodes)                                    // 遍历所有代码
		{
			//如果是主力合约代码, 如SHFE.ag.HOT, 那么要转换成原合约代码, SHFE.ag.1912
			//因为执行器只识别原合约代码
			const char* stdCode = code.c_str();                                    // 获取代码字符串
			std::size_t length = strlen(stdCode);                                  // 获取代码长度
			uint32_t flag = 0;                                                     // 订阅标志
			if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果代码最后一个字符是复权后缀
			{
				length--;                                                           // 长度减1

				flag = (stdCode[length] == SUFFIX_QFQ) ? 1 : 2;                    // 设置订阅标志
			}

			SubFlags& flags = _tick_sub_map[std::string(stdCode, length)];        // 获取或创建订阅标志集合
			flags.insert(flag);                                                    // 将订阅标志添加到集合中
			WTSLogger::info("Tick dada of {} subscribed with flag {}", stdCode, flag);  // 记录信息日志：外部订阅成功
		}
	}
}

/**
 * @brief 订阅K线数据
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m1"=1分钟，"m5"=5分钟，"d"=日线）
 * 
 * 订阅指定合约的实时K线数据。
 * 会先清除所有现有的K线订阅，然后订阅新的K线，并自动订阅对应的Tick数据（用于生成K线）。
 */
void WtDtRunner::sub_bar(const char* stdCode, const char* period)
{
	thread_local static char basePeriod[2] = { 0 };                                // 线程局部静态变量：基础周期字符
	basePeriod[0] = period[0];                                                     // 获取周期字符串的第一个字符
	uint32_t times = 1;                                                            // 周期倍数（默认为1）
	if (strlen(period) > 1)                                                        // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);                                      // 解析倍数

	WTSKlinePeriod kp;                                                             // K线周期枚举
	uint32_t realTimes = times;                                                    // 实际倍数
	if (basePeriod[0] == 'm')                                                      // 如果是分钟周期
	{
		if (times % 5 == 0)                                                        // 如果倍数是5的倍数
		{
			kp = KP_Minute5;                                                       // 使用5分钟作为基础周期
			realTimes /= 5;                                                        // 倍数除以5
		}
		else                                                                       // 如果倍数不是5的倍数
		{
			kp = KP_Minute1;                                                       // 使用1分钟作为基础周期
		}
	}
	else                                                                           // 如果是日线周期
		kp = KP_DAY;                                                                // 设置为日线周期

	_data_mgr.clear_subbed_bars();                                                // 清除所有现有的K线订阅（替换模式）
	_data_mgr.subscribe_bar(stdCode, kp, realTimes);                             // 订阅新的K线数据
	sub_tick(stdCode, true, true);                                                 // 订阅对应的Tick数据（用于生成K线，内部订阅，替换模式）
}

/**
 * @brief 触发K线回调
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m1"、"m5"、"d"）
 * @param lastBar 最后一条K线数据指针
 * 
 * 当实时K线更新时，触发外部K线回调函数。
 * 如果未设置回调函数，则不触发。
 */
void WtDtRunner::trigger_bar(const char* stdCode, const char* period, WTSBarStruct* lastBar)
{
	if (_cb_bar == NULL)                                                           // 如果未设置K线回调函数
		return;                                                                     // 直接返回，不触发回调

	_cb_bar(stdCode, period, lastBar);                                            // 调用外部K线回调函数，传入合约代码、周期和K线数据
}

/**
 * @brief 清除数据缓存
 * 
 * 清除数据管理器的缓存，释放内存。
 * 实际是调用数据管理器的clear_cache()方法。
 */
void WtDtRunner::clear_cache()
{
	_data_mgr.clear_cache();                                                       // 调用数据管理器的clear_cache()方法清除缓存
}