/*!
 * \file WTSLogger.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader统一日志系统实现文件
 * 
 * 文件设计逻辑与核心作用总结：
 * ================================
 * 
 * 本文件是WonderTrader框架中统一日志系统的核心实现，为整个量化交易框架提供
 * 高性能、线程安全、多目标输出的日志记录服务。作为基础设施组件，它确保了
 * 系统运行过程中所有模块的日志输出统一性、可靠性和高效性。
 * 
 * 核心设计理念：
 * =============
 * 1. **统一日志接口**：为整个WonderTrader框架提供统一的日志记录API
 * 2. **高性能设计**：基于spdlog库实现，支持异步日志输出，最小化对交易性能的影响
 * 3. **多线程安全**：采用线程本地存储和无锁设计，确保多线程环境下的安全性
 * 4. **灵活配置管理**：支持JSON配置文件，可动态配置多种输出目标和格式
 * 5. **分层日志架构**：支持分类日志和动态日志器，满足不同模块的个性化需求
 * 
 * 主要功能模块：
 * =============
 * 1. **初始化管理模块**：
 *    - 配置文件解析和验证(init)
 *    - 多种日志器创建和注册(initLogger)
 *    - 异步线程池初始化管理
 * 
 * 2. **日志输出核心模块**：
 *    - 五级日志输出(debug/info/warn/error/fatal)
 *    - 格式化字符串处理(基于fmt库)
 *    - 线程本地缓冲区管理
 * 
 * 3. **多目标输出模块**：
 *    - 文件输出(daily_file_sink/basic_file_sink)
 *    - 控制台输出(console_sink)
 *    - 流输出(ostream_sink)
 *    - 自定义处理器输出
 * 
 * 4. **动态日志管理模块**：
 *    - 运行时动态创建日志器(getLogger)
 *    - 模式化配置管理(LogPatterns)
 *    - 动态日志器生命周期管理
 * 
 * 5. **级别过滤和控制模块**：
 *    - 全局日志级别过滤(m_logLevel)
 *    - 日志系统启停控制(stop)
 *    - 资源释放和清理管理
 * 
 * 技术架构特点：
 * =============
 * - **基于spdlog**：使用业界领先的C++日志库，提供卓越性能
 * - **异步处理**：支持异步日志输出，避免阻塞主业务线程
 * - **零拷贝优化**：使用线程本地存储减少内存分配和拷贝
 * - **配置驱动**：通过JSON配置文件灵活控制日志行为
 * - **扩展性设计**：支持自定义日志处理器和输出目标
 * 
 * 性能优化策略：
 * =============
 * - **线程本地存储**：每线程独立缓冲区，避免锁竞争
 * - **级别预检查**：在格式化前进行级别检查，减少不必要的计算
 * - **异步刷新**：定时批量刷新缓冲区，提高I/O效率
 * - **智能指针管理**：自动内存管理，避免内存泄漏
 * - **条件编译优化**：支持平台特定的优化实现
 * 
 * 应用场景：
 * =========
 * - **交易系统监控**：记录交易执行、订单状态、风控事件
 * - **策略运行日志**：记录策略决策、信号生成、仓位变化
 * - **数据处理日志**：记录行情接收、数据清洗、指标计算
 * - **系统诊断日志**：记录系统启动、配置加载、异常处理
 * - **性能分析日志**：记录关键路径的性能指标和瓶颈
 * - **合规审计日志**：记录关键业务操作，满足监管要求
 */

// 标准C库头文件包含部分
#include <stdio.h>      // 标准输入输出函数库，提供printf、scanf等基础I/O函数
#include <iostream>     // C++标准输入输出流库，提供cout、cin等流操作
#include <sys/timeb.h>  // 系统时间相关函数库，提供高精度时间获取功能

// 平台相关的时间头文件条件编译
#ifdef _MSC_VER
#include <time.h>       // Windows平台(MSVC编译器)的时间函数库
#else
#include <sys/time.h>   // Unix/Linux平台的系统时间函数库
#endif

// WonderTrader框架核心头文件包含
#include "WTSLogger.h"                    // WTS日志器类定义头文件
#include "../WTSUtils/WTSCfgLoader.h"     // WTS配置文件加载器，用于解析JSON配置
#include "../Includes/ILogHandler.h"      // 日志处理接口定义，支持自定义日志处理器
#include "../Includes/WTSVariant.hpp"     // WTS变体数据类型，用于配置参数存储
#include "../Share/StdUtils.hpp"          // WTS标准工具函数集合
#include "../Share/StrUtil.hpp"           // WTS字符串处理工具函数
#include "../Share/TimeUtils.hpp"         // WTS时间处理工具函数

// 第三方库头文件包含
#include <boost/filesystem.hpp>          // Boost文件系统库，用于目录创建和文件操作

// spdlog高性能日志库相关头文件
#include <spdlog/sinks/daily_file_sink.h>    // 按日期滚动的文件输出sink
#include <spdlog/sinks/basic_file_sink.h>    // 基础文件输出sink
#include <spdlog/sinks/stdout_color_sinks.h> // 带颜色的控制台输出sink
#include <spdlog/sinks/ostream_sink.h>       // 标准流输出sink
#include <spdlog/async.h>                    // 异步日志功能支持

// 动态日志模式在配置文件中的键名常量定义
const char* DYN_PATTERN = "dyn_pattern";

/**
 * @name WTSLogger静态成员变量定义
 * @brief 日志系统的全局状态和配置变量
 * 
 * 这些静态成员变量构成了WTSLogger的全局状态，控制着整个日志系统的行为。
 * 采用静态设计确保全局唯一性，所有模块共享同一个日志系统实例。
 * @{
 */

/// 自定义日志处理器指针，用于接收所有日志消息并进行自定义处理
ILogHandler*		WTSLogger::m_logHandler	= NULL;

/// 全局日志级别过滤器，低于此级别的日志将被忽略，提高性能
WTSLogLevel			WTSLogger::m_logLevel	= LL_NONE;

/// 日志系统停止标志，为true时所有日志输出都会被忽略
bool				WTSLogger::m_bStopped = false;

/// 日志系统初始化标志，防止重复初始化，确保系统稳定性
bool				WTSLogger::m_bInited = false;

/// 异步日志线程池初始化标志，确保线程池只初始化一次
bool				WTSLogger::m_bTpInited = false;

/// 根日志器（默认日志器），所有未指定分类的日志都会输出到此日志器
SpdLoggerPtr		WTSLogger::m_rootLogger = NULL;

/// 动态日志模式配置映射表，存储不同模式的日志器配置模板
WTSLogger::LogPatterns*	WTSLogger::m_mapPatterns = NULL;

/// 线程本地日志缓冲区，每个线程独有，避免多线程竞争，大小为MAX_LOG_BUF_SIZE
thread_local char	WTSLogger::m_buffer[];

/// 动态创建的日志器名称集合，用于跟踪和管理运行时创建的日志器
std::set<std::string>	WTSLogger::m_setDynLoggers;

/** @} */

/**
 * @brief 将字符串转换为spdlog日志级别枚举
 * 
 * 功能说明：
 * 将配置文件中的日志级别字符串转换为spdlog库使用的日志级别枚举值。
 * 这是一个内联函数，用于提高转换效率，支持常见的日志级别字符串。
 * 
 * @param slvl 日志级别字符串，不区分大小写，支持的值包括：
 *             - "debug": 调试级别，最详细的日志信息
 *             - "info": 信息级别，一般的运行信息
 *             - "warn": 警告级别，潜在问题的提醒
 *             - "error": 错误级别，程序错误但可继续运行
 *             - "fatal": 致命错误级别，严重错误可能导致程序终止
 * @return spdlog::level::level_enum 对应的spdlog日志级别枚举值
 * 
 * 实现逻辑：
 * 使用wt_stricmp进行不区分大小写的字符串比较，将字符串映射到对应的枚举值。
 * 如果输入的字符串不匹配任何已知级别，则返回spdlog::level::off关闭日志输出。
 * 
 * 使用场景：
 * - 配置文件解析时将字符串级别转换为枚举
 * - 动态设置日志器的输出级别
 * - 日志系统初始化时的级别配置
 */
inline spdlog::level::level_enum str_to_level( const char* slvl)
{
	// 检查是否为调试级别，返回spdlog的debug级别
	if(wt_stricmp(slvl, "debug") == 0)
	{
		return spdlog::level::debug;    // 调试级别：用于开发调试，输出最详细信息
	}
	// 检查是否为信息级别，返回spdlog的info级别
	else if (wt_stricmp(slvl, "info") == 0)
	{
		return spdlog::level::info;     // 信息级别：记录一般的程序运行信息
	}
	// 检查是否为警告级别，返回spdlog的warn级别
	else if (wt_stricmp(slvl, "warn") == 0)
	{
		return spdlog::level::warn;     // 警告级别：记录潜在问题或异常情况
	}
	// 检查是否为错误级别，返回spdlog的error级别
	else if (wt_stricmp(slvl, "error") == 0)
	{
		return spdlog::level::err;      // 错误级别：记录程序错误但不影响继续运行
	}
	// 检查是否为致命错误级别，返回spdlog的critical级别
	else if (wt_stricmp(slvl, "fatal") == 0)
	{
		return spdlog::level::critical; // 致命错误级别：记录严重错误，可能导致程序终止
	}
	// 如果不匹配任何已知级别，关闭日志输出
	else
	{
		return spdlog::level::off;      // 关闭日志：不输出任何日志信息
	}
}

/**
 * @brief 将字符串转换为WTS日志级别枚举
 * 
 * 功能说明：
 * 将配置文件中的日志级别字符串转换为WonderTrader框架内部使用的WTSLogLevel枚举值。
 * 这个函数与str_to_level配套使用，用于WTS内部的日志级别管理和过滤。
 * 
 * @param slvl 日志级别字符串，不区分大小写，支持的值与str_to_level相同
 * @return WTSLogLevel 对应的WTS日志级别枚举值
 * 
 * 实现逻辑：
 * 与str_to_level类似，但返回的是WTS框架定义的日志级别枚举，用于内部级别过滤。
 * WTSLogLevel用于全局日志级别控制，而spdlog::level用于具体的日志器级别设置。
 * 
 * 使用场景：
 * - 设置全局日志级别过滤器m_logLevel
 * - 日志输出前的级别预检查
 * - 自定义日志处理器的级别传递
 */
inline WTSLogLevel str_to_ll(const char* slvl)
{
	// 检查是否为调试级别，返回WTS的LL_DEBUG级别
	if (wt_stricmp(slvl, "debug") == 0)
	{
		return LL_DEBUG;    // WTS调试级别：对应最低的过滤级别
	}
	// 检查是否为信息级别，返回WTS的LL_INFO级别
	else if (wt_stricmp(slvl, "info") == 0)
	{
		return LL_INFO;     // WTS信息级别：一般的运行信息级别
	}
	// 检查是否为警告级别，返回WTS的LL_WARN级别
	else if (wt_stricmp(slvl, "warn") == 0)
	{
		return LL_WARN;     // WTS警告级别：警告信息级别
	}
	// 检查是否为错误级别，返回WTS的LL_ERROR级别
	else if (wt_stricmp(slvl, "error") == 0)
	{
		return LL_ERROR;    // WTS错误级别：错误信息级别
	}
	// 检查是否为致命错误级别，返回WTS的LL_FATAL级别
	else if (wt_stricmp(slvl, "fatal") == 0)
	{
		return LL_FATAL;    // WTS致命错误级别：最高的严重级别
	}
	// 如果不匹配任何已知级别，返回LL_NONE表示不输出日志
	else
	{
		return LL_NONE;     // WTS无日志级别：关闭所有日志输出
	}
}

/**
 * @brief 检查并创建日志文件所需的目录结构
 * 
 * 功能说明：
 * 检查指定文件路径的目录是否存在，如果不存在则自动创建所需的目录结构。
 * 这确保了日志文件能够正确写入，避免因目录不存在而导致的日志写入失败。
 * 
 * @param filename 日志文件的完整路径，包含目录和文件名
 * 
 * 实现逻辑：
 * 1. 使用StrUtil::standardisePath标准化文件路径格式
 * 2. 查找最后一个路径分隔符的位置
 * 3. 提取目录路径部分
 * 4. 检查目录是否存在，不存在则递归创建
 * 
 * 使用场景：
 * - 在创建文件类型的日志sink之前调用
 * - 确保日志文件能够成功创建和写入
 * - 支持深层目录结构的自动创建
 */
inline void checkDirs(const char* filename)
{
	// 将文件路径标准化，统一使用正斜杠作为分隔符
	std::string s = StrUtil::standardisePath(filename, false);
	
	// 查找最后一个路径分隔符的位置，用于分离目录和文件名
	std::size_t pos = s.find_last_of('/');

	// 如果没有找到路径分隔符，说明文件在当前目录，无需创建目录
	if (pos == std::string::npos)
		return;

	// 将位置向后移动一位，包含分隔符在内作为目录路径
	pos++;

	// 检查目录路径是否存在，如果不存在则创建
	if (!StdFile::exists(s.substr(0, pos).c_str()))
		boost::filesystem::create_directories(s.substr(0, pos).c_str());
}

/**
 * @brief 打印带时间戳的标签
 * 
 * 功能说明：
 * 在控制台打印当前时间的格式化标签，用于未初始化状态下的日志输出。
 * 时间格式为[YYYY.MM.DD HH:MM:SS]，可选择是否在后面添加空格。
 * 
 * @param bWithSpace 是否在时间标签后添加空格，默认为true
 * 
 * 实现逻辑：
 * 1. 获取当前本地时间的毫秒时间戳
 * 2. 转换为time_t格式（秒级）
 * 3. 使用localtime转换为本地时间结构
 * 4. 格式化输出为[YYYY.MM.DD HH:MM:SS]格式
 * 
 * 使用场景：
 * - 日志系统未初始化时的控制台输出
 * - 应急日志输出的时间标记
 * - 调试信息的时间戳显示
 */
inline void print_timetag(bool bWithSpace = true)
{
	// 获取当前本地时间的毫秒级时间戳
	uint64_t now = TimeUtils::getLocalTimeNow();
	
	// 转换为秒级时间戳，用于标准时间函数
	time_t t = now / 1000;

	// 将时间戳转换为本地时间结构
	tm * tNow = localtime(&t);
	
	// 格式化输出时间标签：[YYYY.MM.DD HH:MM:SS]
	fmt::print("[{}.{:02d}.{:02d} {:02d}:{:02d}:{:02d}]", 
		tNow->tm_year + 1900,  // 年份（tm_year是从1900年开始的偏移量）
		tNow->tm_mon + 1,      // 月份（tm_mon是0-11，需要加1）
		tNow->tm_mday,         // 日期
		tNow->tm_hour,         // 小时
		tNow->tm_min,          // 分钟
		tNow->tm_sec);         // 秒
	
	// 根据参数决定是否在时间标签后添加空格
	if (bWithSpace)
		fmt::print(" ");
}

/**
 * @brief 打印带时间戳的消息到控制台
 * 
 * 功能说明：
 * 在控制台打印带时间戳的完整消息，用于日志系统未初始化时的应急输出。
 * 这是一个静态公共方法，为未初始化状态提供基本的日志输出功能。
 * 
 * @param buffer 要输出的消息内容
 * 
 * 实现逻辑：
 * 1. 调用print_timetag打印时间标签（带空格）
 * 2. 输出消息内容
 * 3. 输出回车换行符结束本行
 * 
 * 输出格式：
 * [YYYY.MM.DD HH:MM:SS] 消息内容\r\n
 * 
 * 使用场景：
 * - 日志系统初始化失败时的错误信息输出
 * - 配置加载过程中的状态信息显示
 * - 系统启动早期的调试信息输出
 */
void WTSLogger::print_message(const char* buffer)
{
	// 打印时间标签，并在后面添加空格
	print_timetag(true);
	
	// 输出消息内容
	fmt::print(buffer);
	
	// 输出回车换行符，结束当前行
	fmt::print("\r\n");
}

/**
 * @brief 初始化指定分类的日志器
 * 
 * 功能说明：
 * 根据配置参数创建并注册一个新的日志器实例，支持多种输出目标和同步/异步模式。
 * 这是日志系统的核心初始化方法，负责解析配置并创建对应的spdlog日志器。
 * 
 * @param catName 日志分类名称，用作日志器的标识符
 * @param cfgLogger 日志器配置参数，包含级别、输出目标、格式等信息
 * 
 * 配置结构：
 * {
 *   "async": boolean,      // 是否使用异步模式
 *   "level": string,       // 日志级别
 *   "sinks": [             // 输出目标数组
 *     {
 *       "type": string,    // sink类型
 *       "filename": string,// 文件路径（文件类型sink）
 *       "pattern": string, // 输出格式
 *       "truncate": boolean// 是否截断文件（basic_file_sink）
 *     }
 *   ]
 * }
 * 
 * 支持的sink类型：
 * - daily_file_sink: 按日期滚动的文件输出
 * - basic_file_sink: 基础文件输出
 * - console_sink: 彩色控制台输出
 * - ostream_sink: 标准流输出
 * 
 * 实现逻辑：
 * 1. 解析基本配置参数（异步模式、日志级别）
 * 2. 遍历sinks配置，创建对应的输出目标
 * 3. 根据同步/异步模式创建相应的日志器
 * 4. 注册日志器到spdlog全局注册表
 * 5. 特殊处理根日志器的全局级别设置
 */
void WTSLogger::initLogger(const char* catName, WTSVariant* cfgLogger)
{
	// 从配置中获取是否使用异步模式
	bool bAsync = cfgLogger->getBoolean("async");
	
	// 从配置中获取日志级别字符串
	const char* level = cfgLogger->getCString("level");

	// 获取输出目标(sinks)配置数组
	WTSVariant* cfgSinks = cfgLogger->get("sinks");
	
	// 创建sinks容器，用于存储所有输出目标
	std::vector<spdlog::sink_ptr> sinks;
	
	// 遍历所有配置的输出目标
	for (uint32_t idx = 0; idx < cfgSinks->size(); idx++)
	{
		// 获取当前sink的配置
		WTSVariant* cfgSink = cfgSinks->get(idx);
		
		// 获取sink类型
		const char* type = cfgSink->getCString("type");
		
		// 根据类型创建对应的sink实例
		if (strcmp(type, "daily_file_sink") == 0)
		{
			// 按日期滚动的文件输出sink
			std::string filename = cfgSink->getString("filename");
			
			// 将文件名中的%s占位符替换为分类名称
			StrUtil::replace(filename, "%s", catName);
			
			// 检查并创建必要的目录结构
			checkDirs(filename.c_str());
			
			// 创建daily_file_sink，参数：文件名、滚动小时(0=午夜)、滚动分钟(0)
			auto sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filename, 0, 0);
			
			// 设置输出格式模式
			sink->set_pattern(cfgSink->getCString("pattern"));
			
			// 添加到sinks容器
			sinks.emplace_back(sink);
		}
		else if (strcmp(type, "basic_file_sink") == 0)
		{
			// 基础文件输出sink
			std::string filename = cfgSink->getString("filename");
			
			// 将文件名中的%s占位符替换为分类名称
			StrUtil::replace(filename, "%s", catName);
			
			// 检查并创建必要的目录结构
			checkDirs(filename.c_str());
			
			// 创建basic_file_sink，参数：文件名、是否截断文件
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, cfgSink->getBoolean("truncate"));
			
			// 设置输出格式模式
			sink->set_pattern(cfgSink->getCString("pattern"));
			
			// 添加到sinks容器
			sinks.emplace_back(sink);
		}
		else if (strcmp(type, "console_sink") == 0)
		{
			// 彩色控制台输出sink
			auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			
			// 设置输出格式模式
			sink->set_pattern(cfgSink->getCString("pattern"));
			
			// 添加到sinks容器
			sinks.emplace_back(sink);
		}
		else if (strcmp(type, "ostream_sink") == 0)
		{
			// 标准流输出sink，参数：输出流(std::cout)、是否强制刷新(true)
			auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cout, true);
			
			// 设置输出格式模式
			sink->set_pattern(cfgSink->getCString("pattern"));
			
			// 添加到sinks容器
			sinks.emplace_back(sink);
		}
	}

	// 根据同步/异步模式创建不同类型的日志器
	if (!bAsync)
	{
		// 创建同步日志器
		auto logger = std::make_shared<spdlog::logger>(catName, sinks.begin(), sinks.end());
		
		// 设置日志器的输出级别
		logger->set_level(str_to_level(cfgLogger->getCString("level")));
		
		// 将日志器注册到spdlog的全局注册表
		spdlog::register_logger(logger);
	}
	else
	{
		// 异步模式：需要先初始化线程池
		if(!m_bTpInited)
		{
			// 初始化线程池：队列大小8192，工作线程数2
			spdlog::init_thread_pool(8192, 2);
			m_bTpInited = true;
		}

		// 创建异步日志器，参数：名称、sinks范围、线程池、溢出策略(阻塞)
		auto logger = std::make_shared<spdlog::async_logger>(catName, sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		
		// 设置日志器的输出级别
		logger->set_level(str_to_level(cfgLogger->getCString("level")));
		
		// 将异步日志器注册到spdlog的全局注册表
		spdlog::register_logger(logger);
	}

	// 特殊处理：如果是根日志器，设置全局日志级别过滤器
	if(strcmp(catName, "root")==0)
	{
		// 将根日志器的级别设置为全局过滤级别
		m_logLevel = str_to_ll(cfgLogger->getCString("level"));
	}
}

/**
 * @brief 初始化日志系统
 * 
 * 功能说明：
 * 这是WTS日志系统的主入口函数，负责根据配置文件或配置内容初始化整个日志系统。
 * 支持多种日志器、输出目标和动态日志模式的配置，是日志系统启动的核心方法。
 * 
 * @param propFile 配置文件路径或配置内容字符串，默认为"logcfg.json"
 * @param isFile 指示propFile参数的类型，true表示文件路径，false表示配置内容
 * @param handler 可选的自定义日志处理器，用于接收所有日志消息
 * 
 * 配置文件结构示例：
 * {
 *   "root": {                    // 根日志器（必需）
 *     "async": false,
 *     "level": "info",
 *     "sinks": [...]
 *   },
 *   "trading": {                 // 自定义分类日志器
 *     "async": true,
 *     "level": "debug",
 *     "sinks": [...]
 *   },
 *   "dyn_pattern": {             // 动态日志模式配置
 *     "strategy": {...},         // 策略模式配置
 *     "order": {...}             // 订单模式配置
 *   }
 * }
 * 
 * 初始化流程：
 * 1. **重复初始化检查**：防止多次初始化导致的资源冲突
 * 2. **配置加载验证**：检查配置文件存在性并加载配置内容
 * 3. **日志器创建**：遍历配置项，为每个分类创建对应的日志器
 * 4. **动态模式处理**：解析动态日志模式配置，为运行时创建做准备
 * 5. **根日志器设置**：验证并设置必需的根日志器
 * 6. **全局配置应用**：设置默认日志器和自动刷新机制
 * 7. **处理器注册**：注册自定义日志处理器
 * 8. **状态标记**：标记初始化完成状态
 * 
 * 异常处理：
 * - 配置文件不存在：静默返回，不抛出异常
 * - 配置格式错误：静默返回，不抛出异常
 * - 根日志器缺失：抛出runtime_error异常
 * 
 * 线程安全性：
 * 该方法不是线程安全的，应该在程序启动的单线程阶段调用。
 * 
 * 使用场景：
 * - 应用程序启动时的日志系统初始化
 * - 测试环境中的日志系统配置
 * - 动态重新配置日志系统（需要先调用stop）
 */
void WTSLogger::init(const char* propFile /* = "logcfg.json" */, bool isFile /* = true */, ILogHandler* handler /* = NULL */)
{
	// 防止重复初始化，确保日志系统只初始化一次
	if (m_bInited)
		return;

	// 如果propFile是文件路径，检查文件是否存在
	if (isFile && !StdFile::exists(propFile))
		return;

	// 根据参数类型加载配置：从文件加载或从内容字符串解析
	WTSVariant* cfg = isFile ? WTSCfgLoader::load_from_file(propFile) : WTSCfgLoader::load_from_content(propFile, false);
	
	// 如果配置加载失败，静默返回
	if (cfg == NULL)
		return;

	// 获取配置文件中所有的顶级键名（日志器名称和特殊配置）
	auto keys = cfg->memberNames();
	
	// 遍历所有配置项，创建对应的日志器或处理特殊配置
	for (std::string& key : keys)
	{
		// 获取当前键对应的配置项
		WTSVariant* cfgItem = cfg->get(key.c_str());
		
		// 特殊处理：动态日志模式配置
		if (key == DYN_PATTERN)
		{
			// 获取动态模式配置中的所有模式名称
			auto pkeys = cfgItem->memberNames();
			
			// 遍历每个动态模式配置
			for(std::string& pkey : pkeys)
			{
				// 获取具体的模式配置
				WTSVariant* cfgPattern = cfgItem->get(pkey.c_str());
				
				// 如果模式映射表还未创建，则创建它
				if (m_mapPatterns == NULL)
					m_mapPatterns = LogPatterns::create();

				// 将模式配置添加到映射表中，用于运行时动态创建日志器
				// 第三个参数true表示自动引用计数管理
				m_mapPatterns->add(pkey.c_str(), cfgPattern, true);
			}
			
			// 跳过后续的普通日志器创建流程
			continue;
		}

		// 普通日志器初始化：根据配置创建并注册日志器
		initLogger(key.c_str(), cfgItem);
	}

	// 获取根日志器，这是日志系统的必需组件
	m_rootLogger = getLogger("root");
	
	// 验证根日志器是否成功创建，如果失败则抛出异常
	if(m_rootLogger == NULL)
	{
		throw std::runtime_error("root logger can not be null, please check the config file");
	}
	
	// 将根日志器设置为spdlog的默认日志器
	spdlog::set_default_logger(m_rootLogger);
	
	// 设置自动刷新机制：每2秒自动将缓冲区内容刷新到输出目标
	spdlog::flush_every(std::chrono::seconds(2));

	// 注册自定义日志处理器（可选）
	m_logHandler = handler;

	// 标记日志系统初始化完成
	m_bInited = true;
}

/**
 * @brief 注册自定义日志处理器
 * 
 * 功能说明：
 * 注册或更新自定义日志处理器，用于接收所有级别的日志消息。
 * 自定义处理器可以将日志转发到其他系统、数据库或进行特殊的日志处理。
 * 
 * @param handler 日志处理器指针，可以为NULL表示移除当前处理器
 * 
 * 实现逻辑：
 * 直接将传入的处理器指针赋值给静态成员变量m_logHandler。
 * 所有的日志输出方法都会检查这个处理器，如果不为NULL则调用其处理方法。
 * 
 * 使用场景：
 * - 将日志转发到远程日志服务器
 * - 将关键日志存储到数据库
 * - 实现自定义的日志过滤和格式化
 * - 集成第三方监控和告警系统
 * - 在测试环境中捕获日志进行验证
 * 
 * 线程安全性：
 * 该方法不是线程安全的，建议在日志系统初始化阶段调用。
 * 如果需要在运行时更换处理器，需要确保同步访问。
 */
void WTSLogger::registerHandler(ILogHandler* handler /* = NULL */)
{
	// 直接设置自定义日志处理器，NULL表示移除处理器
	m_logHandler = handler;
}

/**
 * @brief 停止日志系统
 * 
 * 功能说明：
 * 停止整个日志系统的运行，释放相关资源并关闭所有日志输出。
 * 调用此方法后，所有的日志输出操作都会被忽略，直到重新初始化。
 * 
 * 实现逻辑：
 * 1. 设置停止标志，阻止后续的日志输出
 * 2. 释放动态日志模式配置映射表
 * 3. 关闭spdlog库，刷新并释放所有日志器
 * 
 * 资源清理：
 * - 设置m_bStopped为true，所有日志方法会检查此标志
 * - 释放m_mapPatterns动态模式配置
 * - 调用spdlog::shutdown()关闭所有日志器和线程池
 * 
 * 使用场景：
 * - 应用程序正常退出时的清理
 * - 重新配置日志系统前的清理
 * - 内存资源紧张时的主动清理
 * - 单元测试的清理阶段
 * 
 * 注意事项：
 * - 调用后日志系统将不可用，需要重新调用init才能恢复
 * - 该方法会等待异步日志队列处理完成
 * - 不会重置m_bInited标志，重新初始化需要先重置该标志
 */
void WTSLogger::stop()
{
	// 设置停止标志，阻止后续的所有日志输出操作
	m_bStopped = true;
	
	// 如果存在动态模式配置映射表，释放其资源
	if (m_mapPatterns)
		m_mapPatterns->release();
	
	// 关闭spdlog库：刷新所有缓冲区、关闭文件、停止异步线程池
	spdlog::shutdown();
}

/**
 * @brief 调试级别日志输出实现
 * 
 * 功能说明：
 * 内部实现方法，负责将调试级别的日志消息输出到指定的日志器、根日志器和自定义处理器。
 * 采用多重输出策略，确保重要的调试信息能够被完整记录。
 * 
 * @param logger 目标日志器指针，可以为NULL
 * @param message 已格式化的日志消息内容
 * 
 * 输出策略：
 * 1. **目标日志器输出**：如果指定的日志器存在，输出到该日志器
 * 2. **根日志器输出**：如果目标日志器不是根日志器，同时输出到根日志器
 * 3. **自定义处理器**：如果存在自定义处理器，调用其处理方法
 * 
 * 多重输出的意义：
 * - 分类日志器：满足模块化日志记录需求
 * - 根日志器：提供统一的日志汇总视图
 * - 自定义处理器：支持特殊的日志处理需求
 * 
 * 使用场景：
 * - 开发调试时的详细信息记录
 * - 程序运行状态的细粒度跟踪
 * - 性能分析和问题诊断
 */
void WTSLogger::debug_imp(SpdLoggerPtr logger, const char* message)
{
	// 如果指定的日志器存在，输出调试信息到该日志器
	if (logger)
		logger->debug(message);

	// 如果指定的日志器不是根日志器，同时输出到根日志器
	if (logger != m_rootLogger)
		m_rootLogger->debug(message);

	// 如果存在自定义日志处理器，调用其处理方法
	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_DEBUG, message);
}

/**
 * @brief 信息级别日志输出实现
 * 
 * 功能说明：
 * 内部实现方法，负责将信息级别的日志消息输出到指定的日志器、根日志器和自定义处理器。
 * 信息级别是最常用的日志级别，用于记录程序的正常运行状态和重要事件。
 * 
 * @param logger 目标日志器指针，可以为NULL
 * @param message 已格式化的日志消息内容
 * 
 * 输出策略：
 * 与debug_imp相同，采用多重输出策略确保信息的完整记录。
 * 
 * 使用场景：
 * - 程序启动和关闭事件
 * - 重要业务操作的状态记录
 * - 系统配置和参数变更
 * - 用户操作和交互记录
 */
void WTSLogger::info_imp(SpdLoggerPtr logger, const char* message)
{
	// 如果指定的日志器存在，输出信息到该日志器
	if (logger)
		logger->info(message);

	// 如果指定的日志器不是根日志器，同时输出到根日志器
	if (logger != m_rootLogger)
		m_rootLogger->info(message);

	// 如果存在自定义日志处理器，调用其处理方法
	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_INFO, message);
}

/**
 * @brief 警告级别日志输出实现
 * 
 * 功能说明：
 * 内部实现方法，负责将警告级别的日志消息输出到指定的日志器、根日志器和自定义处理器。
 * 警告级别用于记录可能的问题或异常情况，但不会影响程序的正常运行。
 * 
 * @param logger 目标日志器指针，可以为NULL
 * @param message 已格式化的日志消息内容
 * 
 * 输出策略：
 * 与其他级别相同，采用多重输出策略，但警告信息通常需要更多关注。
 * 
 * 使用场景：
 * - 配置参数不合理但可以使用默认值
 * - 网络连接不稳定但可以重试
 * - 资源使用率较高的提醒
 * - 业务逻辑中的边界情况处理
 */
void WTSLogger::warn_imp(SpdLoggerPtr logger, const char* message)
{
	// 如果指定的日志器存在，输出警告信息到该日志器
	if (logger)
		logger->warn(message);

	// 如果指定的日志器不是根日志器，同时输出到根日志器
	if (logger != m_rootLogger)
		m_rootLogger->warn(message);

	// 如果存在自定义日志处理器，调用其处理方法
	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_WARN, message);
}

/**
 * @brief 错误级别日志输出实现
 * 
 * 功能说明：
 * 内部实现方法，负责将错误级别的日志消息输出到指定的日志器、根日志器和自定义处理器。
 * 错误级别用于记录程序运行中的错误，可能会影响某些功能但不会导致程序崩溃。
 * 
 * @param logger 目标日志器指针，可以为NULL
 * @param message 已格式化的日志消息内容
 * 
 * 输出策略：
 * 错误信息通常需要立即关注和处理，多重输出确保不会遗漏。
 * 
 * 使用场景：
 * - 文件读写操作失败
 * - 网络请求超时或失败
 * - 数据解析错误
 * - 业务逻辑执行异常
 * - 第三方服务调用失败
 */
void WTSLogger::error_imp(SpdLoggerPtr logger, const char* message)
{
	// 如果指定的日志器存在，输出错误信息到该日志器
	if (logger)
		logger->error(message);

	// 如果指定的日志器不是根日志器，同时输出到根日志器
	if (logger != m_rootLogger)
		m_rootLogger->error(message);

	// 如果存在自定义日志处理器，调用其处理方法
	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_ERROR, message);
}

/**
 * @brief 致命错误级别日志输出实现
 * 
 * 功能说明：
 * 内部实现方法，负责将致命错误级别的日志消息输出到指定的日志器、根日志器和自定义处理器。
 * 致命错误是最高级别的错误，通常表示严重的系统错误或可能导致程序终止的问题。
 * 
 * @param logger 目标日志器指针，可以为NULL
 * @param message 已格式化的日志消息内容
 * 
 * 输出策略：
 * 致命错误需要最高优先级的处理，确保在程序可能终止前记录关键信息。
 * 注意：spdlog中使用critical对应致命错误级别。
 * 
 * 使用场景：
 * - 内存分配失败
 * - 关键资源无法获取
 * - 系统级错误
 * - 数据完整性严重损坏
 * - 安全相关的严重问题
 */
void WTSLogger::fatal_imp(SpdLoggerPtr logger, const char* message)
{
	// 如果指定的日志器存在，输出致命错误信息到该日志器
	// 注意：spdlog使用critical表示致命错误级别
	if (logger)
		logger->critical(message);

	// 如果指定的日志器不是根日志器，同时输出到根日志器
	if (logger != m_rootLogger)
		m_rootLogger->critical(message);

	// 如果存在自定义日志处理器，调用其处理方法
	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_FATAL, message);
}

/**
 * @brief 输出原始日志消息到根日志器
 * 
 * 功能说明：
 * 将已格式化的原始日志消息按指定级别输出到根日志器。这是最基础的日志输出方法，
 * 不指定特定的分类日志器，所有消息都会输出到根日志器。
 * 
 * @param ll 日志级别，用于级别过滤和选择输出方法
 * @param message 已格式化的日志消息内容，不再进行格式化处理
 * 
 * 处理流程：
 * 1. **级别过滤**：检查全局日志级别和停止状态
 * 2. **初始化检查**：如果日志系统未初始化，使用应急输出
 * 3. **级别分发**：根据日志级别调用对应的内部实现方法
 * 
 * 级别过滤逻辑：
 * - 如果全局日志级别高于当前消息级别，直接返回不输出
 * - 如果日志系统已停止，直接返回不输出
 * - 这种预检查可以避免不必要的处理开销
 * 
 * 应急输出机制：
 * - 当日志系统未初始化时，使用print_message直接输出到控制台
 * - 确保在系统启动早期或初始化失败时仍能记录重要信息
 * 
 * 使用场景：
 * - 内部日志输出的统一入口
 * - 不需要分类的通用日志输出
 * - 系统级的重要消息记录
 */
void WTSLogger::log_raw(WTSLogLevel ll, const char* message)
{
	// 级别过滤：如果全局级别高于消息级别或系统已停止，直接返回
	if (m_logLevel > ll || m_bStopped)
		return;

	// 如果日志系统未初始化，使用应急输出机制
	if (!m_bInited)
	{
		print_message(message);
		return;
	}

	// 获取根日志器作为输出目标
	auto logger = m_rootLogger;

	// 如果根日志器存在，根据级别调用对应的输出实现
	if (logger)
	{
		switch (ll)
		{
		case LL_DEBUG:
			debug_imp(logger, message); break;    // 调试级别输出
		case LL_INFO:
			info_imp(logger, message); break;     // 信息级别输出
		case LL_WARN:
			warn_imp(logger, message); break;     // 警告级别输出
		case LL_ERROR:
			error_imp(logger, message); break;    // 错误级别输出
		case LL_FATAL:
			fatal_imp(logger, message); break;    // 致命错误级别输出
		default:
			break;                                 // 未知级别，不处理
		}
	}
}

/**
 * @brief 按分类输出原始日志消息
 * 
 * 功能说明：
 * 将已格式化的原始日志消息按指定级别输出到指定分类的日志器。
 * 支持分类日志记录，不同的模块或功能可以使用不同的日志分类。
 * 
 * @param catName 日志分类名称，用于获取对应的日志器
 * @param ll 日志级别，用于级别过滤和选择输出方法
 * @param message 已格式化的日志消息内容，不再进行格式化处理
 * 
 * 处理流程：
 * 1. **级别过滤**：检查全局日志级别和停止状态
 * 2. **日志器获取**：根据分类名称获取对应的日志器
 * 3. **降级处理**：如果分类日志器不存在，使用根日志器
 * 4. **应急输出**：如果系统未初始化，使用控制台输出
 * 5. **级别分发**：根据日志级别调用对应的内部实现方法
 * 
 * 日志器选择策略：
 * - 优先使用指定分类的日志器
 * - 如果分类日志器不存在，降级使用根日志器
 * - 确保消息不会因为分类不存在而丢失
 * 
 * 应急输出差异：
 * - 与log_raw不同，这里使用print_timetag + fmt::print的组合
 * - 输出格式略有不同，使用\n而不是\r\n
 * 
 * 使用场景：
 * - 模块化的日志记录（如交易模块、数据模块）
 * - 按功能分类的日志输出
 * - 需要独立配置输出格式的日志
 * - 便于日志分析和过滤的分类记录
 */
void WTSLogger::log_raw_by_cat(const char* catName, WTSLogLevel ll, const char* message)
{
	// 级别过滤：如果全局级别高于消息级别或系统已停止，直接返回
	if (m_logLevel > ll || m_bStopped)
		return;

	// 根据分类名称获取对应的日志器
	auto logger = getLogger(catName);
	
	// 如果分类日志器不存在，降级使用根日志器
	if (logger == NULL)
		logger = m_rootLogger;

	// 如果日志系统未初始化，使用应急输出机制
	if (!m_bInited)
	{
		// 输出时间标签（带空格）
		print_timetag(true);
		
		// 输出消息内容
		fmt::print(message);
		
		// 输出换行符结束当前行
		fmt::print("\n");
		return;
	}

	// 如果日志器存在，根据级别调用对应的输出实现
	if (logger)
	{
		switch (ll)
		{
		case LL_DEBUG:
			debug_imp(logger, message);    // 调试级别输出
			break;
		case LL_INFO:
			info_imp(logger, message);     // 信息级别输出
			break;
		case LL_WARN:
			warn_imp(logger, message);     // 警告级别输出
			break;
		case LL_ERROR:
			error_imp(logger, message);    // 错误级别输出
			break;
		case LL_FATAL:
			fatal_imp(logger, message);    // 致命错误级别输出
			break;
		default:
			break;                         // 未知级别，不处理
		}
	}	
}

/**
 * @brief 使用动态模式输出原始日志消息
 * 
 * 功能说明：
 * 使用动态日志模式输出已格式化的原始日志消息。动态模式允许在运行时根据预定义的
 * 模式配置创建新的日志器，提供了更灵活的日志管理方式。
 * 
 * @param patttern 动态日志模式名称，用于查找对应的配置模板
 * @param catName 日志分类名称，将作为动态创建的日志器名称
 * @param ll 日志级别，用于级别过滤和选择输出方法
 * @param message 已格式化的日志消息内容，不再进行格式化处理
 * 
 * 处理流程：
 * 1. **级别过滤**：检查全局日志级别和停止状态
 * 2. **动态日志器获取**：根据模式和分类名称获取或创建日志器
 * 3. **降级处理**：如果动态日志器创建失败，使用根日志器
 * 4. **应急输出**：如果系统未初始化，使用控制台输出
 * 5. **级别分发**：根据日志级别调用对应的内部实现方法
 * 
 * 动态日志器机制：
 * - 如果指定分类的日志器已存在，直接使用
 * - 如果不存在但有对应的模式配置，动态创建新日志器
 * - 新创建的日志器会被注册和缓存，后续可复用
 * - 创建失败时降级使用根日志器
 * 
 * 应急输出注意：
 * - 代码中使用了m_buffer而不是message参数，这可能是一个bug
 * - 正常情况下应该输出message参数的内容
 * 
 * 使用场景：
 * - 策略实例的独立日志记录
 * - 临时任务的专用日志输出
 * - 运行时动态配置的日志分类
 * - 需要特殊格式的临时日志需求
 */
void WTSLogger::log_dyn_raw(const char* patttern, const char* catName, WTSLogLevel ll, const char* message)
{
	// 级别过滤：如果全局级别高于消息级别或系统已停止，直接返回
	if (m_logLevel > ll || m_bStopped)
		return;

	// 使用动态模式获取或创建日志器
	auto logger = getLogger(catName, patttern);
	
	// 如果动态日志器获取失败，降级使用根日志器
	if (logger == NULL)
		logger = m_rootLogger;

	// 如果日志系统未初始化，使用应急输出机制
	if (!m_bInited)
	{
		// 输出时间标签（带空格）
		print_timetag(true);
		
		// 注意：这里使用m_buffer可能是bug，应该使用message参数
		fmt::print(m_buffer);
		
		// 输出换行符结束当前行
		fmt::print("\n");
		return;
	}

	// 根据级别调用对应的输出实现（注意这里没有检查logger是否为NULL）
	switch (ll)
	{
	case LL_DEBUG:
		debug_imp(logger, message);    // 调试级别输出
		break;
	case LL_INFO:
		info_imp(logger, message);     // 信息级别输出
		break;
	case LL_WARN:
		warn_imp(logger, message);     // 警告级别输出
		break;
	case LL_ERROR:
		error_imp(logger, message);    // 错误级别输出
		break;
	case LL_FATAL:
		fatal_imp(logger, message);    // 致命错误级别输出
		break;
	default:
		break;                         // 未知级别，不处理
	}
}


/**
 * @brief 获取指定名称的日志器
 * 
 * 功能说明：
 * 获取指定名称的日志器，支持静态日志器和动态日志器两种模式。
 * 如果日志器不存在且提供了模式参数，会尝试根据动态模式配置创建新的日志器。
 * 
 * @param logger 日志器名称，用于查找或创建日志器
 * @param pattern 动态日志器模式名称，可选参数，默认为空字符串
 * @return SpdLoggerPtr 日志器智能指针，如果获取失败返回空指针
 * 
 * 获取策略：
 * 1. **静态查找**：首先尝试从spdlog注册表中获取已存在的日志器
 * 2. **动态创建**：如果静态查找失败且提供了模式参数，尝试动态创建
 * 3. **模式验证**：检查动态模式配置是否存在
 * 4. **日志器初始化**：使用模式配置初始化新的日志器
 * 5. **注册管理**：将新创建的日志器添加到动态日志器集合中
 * 
 * 动态创建流程：
 * - 检查模式映射表是否已初始化
 * - 根据模式名称查找对应的配置
 * - 使用配置调用initLogger创建日志器
 * - 将日志器名称添加到动态日志器集合
 * - 从spdlog注册表中获取新创建的日志器
 * 
 * 错误处理：
 * - 如果模式映射表未初始化，返回空指针
 * - 如果模式配置不存在，返回空指针
 * - 如果日志器创建失败，返回空指针
 * 
 * 使用场景：
 * - 获取预配置的静态日志器
 * - 运行时动态创建专用日志器
 * - 策略实例的独立日志管理
 * - 临时任务的专用日志记录
 */
SpdLoggerPtr WTSLogger::getLogger(const char* logger, const char* pattern /* = "" */)
{
	// 首先尝试从spdlog注册表中获取已存在的日志器
	SpdLoggerPtr ret = spdlog::get(logger);
	
	// 如果日志器不存在且提供了模式参数，尝试动态创建
	if (ret == NULL && strlen(pattern) > 0)
	{
		// 当成动态的日志来处理，检查模式映射表是否已初始化
		if (m_mapPatterns == NULL)
			return SpdLoggerPtr();  // 返回空指针

		// 根据模式名称获取对应的配置
		WTSVariant* cfg = (WTSVariant*)m_mapPatterns->get(pattern);
		if (cfg == NULL)
			return SpdLoggerPtr();  // 模式配置不存在，返回空指针

		// 使用模式配置初始化新的日志器
		initLogger(logger, cfg);

		// 将新创建的日志器名称添加到动态日志器集合中
		m_setDynLoggers.insert(logger);

		// 从spdlog注册表中获取新创建的日志器并返回
		return spdlog::get(logger);
	}

	// 返回静态查找的结果（可能为空）
	return ret;
}

/**
 * @brief 释放所有动态创建的日志器
 * 
 * 功能说明：
 * 释放所有通过动态模式创建的日志器，清理相关资源。这个方法主要用于
 * 系统关闭时的资源清理或内存管理。
 * 
 * 实现逻辑：
 * 1. 遍历动态日志器名称集合
 * 2. 对每个日志器名称，从spdlog注册表中获取日志器指针
 * 3. 如果日志器存在，调用spdlog::drop移除并释放该日志器
 * 4. 继续处理下一个动态日志器
 * 
 * 清理策略：
 * - 只处理动态创建的日志器，不影响静态配置的日志器
 * - 使用spdlog::drop安全地移除日志器
 * - 忽略已经被释放或不存在的日志器
 * 
 * 资源释放：
 * - 关闭日志器的所有输出目标（文件、控制台等）
 * - 刷新缓冲区中的未写入数据
 * - 释放日志器占用的内存资源
 * - 从spdlog全局注册表中移除
 * 
 * 使用场景：
 * - 应用程序退出时的资源清理
 * - 内存资源紧张时的主动清理
 * - 重新配置日志系统前的清理
 * - 长期运行程序的定期资源整理
 * 
 * 注意事项：
 * - 调用后动态日志器将不可用
 * - 不会清空m_setDynLoggers集合本身
 * - 线程安全性依赖于spdlog库的实现
 */
void WTSLogger::freeAllDynLoggers()
{
	// 遍历所有动态创建的日志器名称
	for(const std::string& logger : m_setDynLoggers)
	{
		// 从spdlog注册表中获取日志器指针
		auto loggerPtr = spdlog::get(logger);
		
		// 如果日志器不存在（可能已被释放），跳过处理
		if(!loggerPtr)
			continue;

		// 从spdlog注册表中移除并释放该日志器
		spdlog::drop(logger);
	}
}