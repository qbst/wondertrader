/*!
 * \file WTSLogger.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader日志系统实现
 * 
 * 本文件实现了WonderTrader框架的统一日志系统，提供高性能、线程安全的日志记录功能。
 * 基于spdlog库实现，支持异步日志、多种输出格式和灵活的配置管理。
 */

// 标准C库头文件
#include <stdio.h>      // 标准输入输出
#include <iostream>     // C++输入输出流
#include <sys/timeb.h>  // 时间相关函数

// 平台相关的时间头文件
#ifdef _MSC_VER
#include <time.h>       // Windows平台时间函数
#else
#include <sys/time.h>   // Unix/Linux平台时间函数
#endif

// WonderTrader框架头文件
#include "WTSLogger.h"                    // 日志器头文件
#include "../WTSUtils/WTSCfgLoader.h"     // 配置加载器
#include "../Includes/ILogHandler.h"      // 日志处理接口
#include "../Includes/WTSVariant.hpp"     // 变体数据类型
#include "../Share/StdUtils.hpp"          // 标准工具函数
#include "../Share/StrUtil.hpp"           // 字符串工具函数
#include "../Share/TimeUtils.hpp"         // 时间工具函数

// 第三方库头文件
#include <boost/filesystem.hpp>          // Boost文件系统库

// spdlog相关头文件
#include <spdlog/sinks/daily_file_sink.h>    // 按日滚动文件输出
#include <spdlog/sinks/basic_file_sink.h>    // 基础文件输出
#include <spdlog/sinks/stdout_color_sinks.h> // 彩色控制台输出
#include <spdlog/sinks/ostream_sink.h>       // 流输出
#include <spdlog/async.h>                    // 异步日志支持

// 动态日志模式的配置键名
const char* DYN_PATTERN = "dyn_pattern";

/**
 * @name WTSLogger静态成员变量定义
 * @brief 日志系统的全局状态和配置变量
 * @{
 */

/// 自定义日志处理器指针
ILogHandler*		WTSLogger::m_logHandler	= NULL;

/// 全局日志级别过滤器
WTSLogLevel			WTSLogger::m_logLevel	= LL_NONE;

/// 日志系统停止标志
bool				WTSLogger::m_bStopped = false;

/// 日志系统初始化标志
bool				WTSLogger::m_bInited = false;

/// 异步日志线程池初始化标志
bool				WTSLogger::m_bTpInited = false;

/// 根日志器（默认日志器）
SpdLoggerPtr		WTSLogger::m_rootLogger = NULL;

/// 动态日志模式配置映射表
WTSLogger::LogPatterns*	WTSLogger::m_mapPatterns = NULL;

/// 线程本地日志缓冲区
thread_local char	WTSLogger::m_buffer[];

/// 动态创建的日志器名称集合
std::set<std::string>	WTSLogger::m_setDynLoggers;

/** @} */

/**
 * @brief 将字符串转换为spdlog日志级别枚举
 * @param slvl 日志级别字符串
 * @return spdlog::level::level_enum 对应的spdlog日志级别
 * 
 * 将配置文件中的日志级别字符串转换为spdlog库使用的日志级别枚举值。
 * 支持的级别包括：debug、info、warn、error、fatal。
 */
inline spdlog::level::level_enum str_to_level( const char* slvl)
{
	if(wt_stricmp(slvl, "debug") == 0)
	{
		return spdlog::level::debug;    // 调试级别
	}
	else if (wt_stricmp(slvl, "info") == 0)
	{
		return spdlog::level::info;     // 信息级别
	}
	else if (wt_stricmp(slvl, "warn") == 0)
	{
		return spdlog::level::warn;     // 警告级别
	}
	else if (wt_stricmp(slvl, "error") == 0)
	{
		return spdlog::level::err;      // 错误级别
	}
	else if (wt_stricmp(slvl, "fatal") == 0)
	{
		return spdlog::level::critical; // 致命错误级别
	}
	else
	{
		return spdlog::level::off;      // 关闭日志
	}
}

inline WTSLogLevel str_to_ll(const char* slvl)
{
	if (wt_stricmp(slvl, "debug") == 0)
	{
		return LL_DEBUG;
	}
	else if (wt_stricmp(slvl, "info") == 0)
	{
		return LL_INFO;
	}
	else if (wt_stricmp(slvl, "warn") == 0)
	{
		return LL_WARN;
	}
	else if (wt_stricmp(slvl, "error") == 0)
	{
		return LL_ERROR;
	}
	else if (wt_stricmp(slvl, "fatal") == 0)
	{
		return LL_FATAL;
	}
	else
	{
		return LL_NONE;
	}
}

inline void checkDirs(const char* filename)
{
	std::string s = StrUtil::standardisePath(filename, false);
	std::size_t pos = s.find_last_of('/');

	if (pos == std::string::npos)
		return;

	pos++;

	if (!StdFile::exists(s.substr(0, pos).c_str()))
		boost::filesystem::create_directories(s.substr(0, pos).c_str());
}

inline void print_timetag(bool bWithSpace = true)
{
	uint64_t now = TimeUtils::getLocalTimeNow();
	time_t t = now / 1000;

	tm * tNow = localtime(&t);
	fmt::print("[{}.{:02d}.{:02d} {:02d}:{:02d}:{:02d}]", tNow->tm_year + 1900, tNow->tm_mon + 1, tNow->tm_mday, tNow->tm_hour, tNow->tm_min, tNow->tm_sec);
	if (bWithSpace)
		fmt::print(" ");
}

void WTSLogger::print_message(const char* buffer)
{
	print_timetag(true);
	fmt::print(buffer);
	fmt::print("\r\n");
}

void WTSLogger::initLogger(const char* catName, WTSVariant* cfgLogger)
{
	bool bAsync = cfgLogger->getBoolean("async");
	const char* level = cfgLogger->getCString("level");

	WTSVariant* cfgSinks = cfgLogger->get("sinks");
	std::vector<spdlog::sink_ptr> sinks;
	for (uint32_t idx = 0; idx < cfgSinks->size(); idx++)
	{
		WTSVariant* cfgSink = cfgSinks->get(idx);
		const char* type = cfgSink->getCString("type");
		if (strcmp(type, "daily_file_sink") == 0)
		{
			std::string filename = cfgSink->getString("filename");
			StrUtil::replace(filename, "%s", catName);
			checkDirs(filename.c_str());
			auto sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filename, 0, 0);
			sink->set_pattern(cfgSink->getCString("pattern"));
			sinks.emplace_back(sink);
		}
		else if (strcmp(type, "basic_file_sink") == 0)
		{
			std::string filename = cfgSink->getString("filename");
			StrUtil::replace(filename, "%s", catName);
			checkDirs(filename.c_str());
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, cfgSink->getBoolean("truncate"));
			sink->set_pattern(cfgSink->getCString("pattern"));
			sinks.emplace_back(sink);
		}
		else if (strcmp(type, "console_sink") == 0)
		{
			auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sink->set_pattern(cfgSink->getCString("pattern"));
			sinks.emplace_back(sink);
		}
		else if (strcmp(type, "ostream_sink") == 0)
		{
			auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cout, true);
			sink->set_pattern(cfgSink->getCString("pattern"));
			sinks.emplace_back(sink);
		}
	}

	if (!bAsync)
	{
		auto logger = std::make_shared<spdlog::logger>(catName, sinks.begin(), sinks.end());
		logger->set_level(str_to_level(cfgLogger->getCString("level")));
		spdlog::register_logger(logger);
	}
	else
	{
		if(!m_bTpInited)
		{
			spdlog::init_thread_pool(8192, 2);
			m_bTpInited = true;
		}

		auto logger = std::make_shared<spdlog::async_logger>(catName, sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		logger->set_level(str_to_level(cfgLogger->getCString("level")));
		spdlog::register_logger(logger);
	}

	if(strcmp(catName, "root")==0)
	{
		m_logLevel = str_to_ll(cfgLogger->getCString("level"));
	}
}

/**
 * @brief 初始化日志系统
 * @param propFile 配置文件路径或配置内容
 * @param isFile 是否为文件路径
 * @param handler 自定义日志处理器
 * 
 * 根据配置文件或配置内容初始化整个日志系统。这是日志系统的入口函数，
 * 负责解析配置、创建各种日志器、设置输出目标等。
 * 
 * 配置解析流程：
 * 1. 检查初始化状态，避免重复初始化
 * 2. 加载配置文件或解析配置内容
 * 3. 遍历配置项，创建对应的日志器
 * 4. 处理动态日志模式配置
 * 5. 设置根日志器和全局配置
 * 6. 启动定时刷新机制
 */
void WTSLogger::init(const char* propFile /* = "logcfg.json" */, bool isFile /* = true */, ILogHandler* handler /* = NULL */)
{
	// 防止重复初始化
	if (m_bInited)
		return;

	// 如果是文件路径，检查文件是否存在
	if (isFile && !StdFile::exists(propFile))
		return;

	// 加载配置：从文件或直接从内容加载
	WTSVariant* cfg = isFile ? WTSCfgLoader::load_from_file(propFile) : WTSCfgLoader::load_from_content(propFile, false);
	if (cfg == NULL)
		return;

	// 遍历配置中的所有日志器定义
	auto keys = cfg->memberNames();
	for (std::string& key : keys)
	{
		WTSVariant* cfgItem = cfg->get(key.c_str());
		
		// 处理动态日志模式配置
		if (key == DYN_PATTERN)
		{
			auto pkeys = cfgItem->memberNames();
			for(std::string& pkey : pkeys)
			{
				WTSVariant* cfgPattern = cfgItem->get(pkey.c_str());
				if (m_mapPatterns == NULL)
					m_mapPatterns = LogPatterns::create();

				// 保存动态模式配置，用于运行时创建日志器
				m_mapPatterns->add(pkey.c_str(), cfgPattern, true);
			}
			continue;
		}

		// 初始化普通日志器
		initLogger(key.c_str(), cfgItem);
	}

	// 获取根日志器，这是必须的
	m_rootLogger = getLogger("root");
	if(m_rootLogger == NULL)
	{
		throw std::runtime_error("root logger can not be null, please check the config file");
	}
	
	// 设置spdlog的默认日志器和自动刷新
	spdlog::set_default_logger(m_rootLogger);
	spdlog::flush_every(std::chrono::seconds(2));  // 每2秒自动刷新缓冲区

	// 设置自定义日志处理器
	m_logHandler = handler;

	// 标记初始化完成
	m_bInited = true;
}

void WTSLogger::registerHandler(ILogHandler* handler /* = NULL */)
{
	m_logHandler = handler;
}

void WTSLogger::stop()
{
	m_bStopped = true;
	if (m_mapPatterns)
		m_mapPatterns->release();
	spdlog::shutdown();
}

void WTSLogger::debug_imp(SpdLoggerPtr logger, const char* message)
{
	if (logger)
		logger->debug(message);

	if (logger != m_rootLogger)
		m_rootLogger->debug(message);

	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_DEBUG, message);
}

void WTSLogger::info_imp(SpdLoggerPtr logger, const char* message)
{
	if (logger)
		logger->info(message);

	if (logger != m_rootLogger)
		m_rootLogger->info(message);

	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_INFO, message);
}

void WTSLogger::warn_imp(SpdLoggerPtr logger, const char* message)
{
	if (logger)
		logger->warn(message);

	if (logger != m_rootLogger)
		m_rootLogger->warn(message);

	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_WARN, message);
}

void WTSLogger::error_imp(SpdLoggerPtr logger, const char* message)
{
	if (logger)
		logger->error(message);

	if (logger != m_rootLogger)
		m_rootLogger->error(message);

	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_ERROR, message);
}

void WTSLogger::fatal_imp(SpdLoggerPtr logger, const char* message)
{
	if (logger)
		logger->critical(message);

	if (logger != m_rootLogger)
		m_rootLogger->critical(message);

	if (m_logHandler)
		m_logHandler->handleLogAppend(LL_FATAL, message);
}

void WTSLogger::log_raw(WTSLogLevel ll, const char* message)
{
	if (m_logLevel > ll || m_bStopped)
		return;

	if (!m_bInited)
	{
		print_message(message);
		return;
	}

	auto logger = m_rootLogger;

	if (logger)
	{
		switch (ll)
		{
		case LL_DEBUG:
			debug_imp(logger, message); break;
		case LL_INFO:
			info_imp(logger, message); break;
		case LL_WARN:
			warn_imp(logger, message); break;
		case LL_ERROR:
			error_imp(logger, message); break;
		case LL_FATAL:
			fatal_imp(logger, message); break;
		default:
			break;
		}
	}
}

void WTSLogger::log_raw_by_cat(const char* catName, WTSLogLevel ll, const char* message)
{
	if (m_logLevel > ll || m_bStopped)
		return;

	auto logger = getLogger(catName);
	if (logger == NULL)
		logger = m_rootLogger;

	if (!m_bInited)
	{
		print_timetag(true);
		fmt::print(message);
		fmt::print("\n");
		return;
	}

	if (logger)
	{
		switch (ll)
		{
		case LL_DEBUG:
			debug_imp(logger, message);
			break;
		case LL_INFO:
			info_imp(logger, message);
			break;
		case LL_WARN:
			warn_imp(logger, message);
			break;
		case LL_ERROR:
			error_imp(logger, message);
			break;
		case LL_FATAL:
			fatal_imp(logger, message);
			break;
		default:
			break;
		}
	}	
}

void WTSLogger::log_dyn_raw(const char* patttern, const char* catName, WTSLogLevel ll, const char* message)
{
	if (m_logLevel > ll || m_bStopped)
		return;

	auto logger = getLogger(catName, patttern);
	if (logger == NULL)
		logger = m_rootLogger;

	if (!m_bInited)
	{
		print_timetag(true);
		fmt::print(m_buffer);
		fmt::print("\n");
		return;
	}

	switch (ll)
	{
	case LL_DEBUG:
		debug_imp(logger, message);
		break;
	case LL_INFO:
		info_imp(logger, message);
		break;
	case LL_WARN:
		warn_imp(logger, message);
		break;
	case LL_ERROR:
		error_imp(logger, message);
		break;
	case LL_FATAL:
		fatal_imp(logger, message);
		break;
	default:
		break;
	}
}


SpdLoggerPtr WTSLogger::getLogger(const char* logger, const char* pattern /* = "" */)
{
	SpdLoggerPtr ret = spdlog::get(logger);
	if (ret == NULL && strlen(pattern) > 0)
	{
		//当成动态的日志来处理
		if (m_mapPatterns == NULL)
			return SpdLoggerPtr();

		WTSVariant* cfg = (WTSVariant*)m_mapPatterns->get(pattern);
		if (cfg == NULL)
			return SpdLoggerPtr();

		initLogger(logger, cfg);

		m_setDynLoggers.insert(logger);

		return spdlog::get(logger);
	}

	return ret;
}

void WTSLogger::freeAllDynLoggers()
{
	for(const std::string& logger : m_setDynLoggers)
	{
		auto loggerPtr = spdlog::get(logger);
		if(!loggerPtr)
			continue;

		spdlog::drop(logger);
	}
}