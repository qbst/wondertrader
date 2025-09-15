/*!
 * \file WTSLogger.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader日志系统核心模块
 * 
 * 本文件定义了WonderTrader框架的统一日志系统，提供了高性能、多线程安全的日志记录功能。
 * 日志系统基于spdlog库实现，支持多种输出方式（文件、控制台等）和日志级别控制。
 * 
 * 主要功能：
 * - 统一的日志记录接口，支持格式化输出
 * - 多种日志级别：DEBUG、INFO、WARN、ERROR、FATAL
 * - 支持分类日志和动态日志配置
 * - 异步日志输出，提高系统性能
 * - 线程本地存储缓冲区，避免多线程竞争
 * - 灵活的日志配置管理
 */
#pragma once
#include "../Includes/WTSTypes.h"        // WonderTrader基础类型定义
#include "../Includes/WTSCollection.hpp" // WTS集合类定义
#include "../Share/fmtlib.h"             // 格式化库封装

#include <memory>    // 智能指针支持
#include <sstream>   // 字符串流支持
#include <thread>    // 线程支持
#include <set>       // STL集合容器

// By Wesley @ 2022.01.05
// spdlog升级到1.9.2版本，同时使用外部的fmt 8.1.0库
// spdlog是一个高性能的C++日志库，支持异步日志和多种输出格式
#include <spdlog/spdlog.h>

// spdlog日志器智能指针类型定义
typedef std::shared_ptr<spdlog::logger> SpdLoggerPtr;

NS_WTP_BEGIN
class ILogHandler;  // 日志处理接口前置声明
class WTSVariant;   // 变体数据类型前置声明
NS_WTP_END

USING_NS_WTP;

// 日志缓冲区最大大小定义（2KB）
#define MAX_LOG_BUF_SIZE 2048

/**
 * @class WTSLogger
 * @brief WonderTrader统一日志管理器
 * 
 * WTSLogger是WonderTrader框架的核心日志系统，提供了统一的日志记录接口。
 * 该类采用静态方法设计，全局唯一，支持多线程并发访问。
 * 
 * 设计特点：
 * - 基于spdlog库实现，提供高性能日志输出
 * - 支持多种日志级别和输出目标
 * - 线程安全设计，支持多线程环境
 * - 支持格式化字符串输出（类似printf风格）
 * - 支持分类日志和动态日志配置
 * - 提供日志级别过滤功能
 */
class WTSLogger
{
private:
	/**
	 * @name 日志输出实现方法
	 * @brief 内部日志输出实现函数，负责将格式化后的消息输出到指定的日志器
	 * @{
	 */
	
	/**
	 * @brief 调试级别日志输出实现
	 * @param logger 目标日志器指针
	 * @param message 要输出的消息内容
	 */
	static void debug_imp(SpdLoggerPtr logger, const char* message);
	
	/**
	 * @brief 信息级别日志输出实现
	 * @param logger 目标日志器指针
	 * @param message 要输出的消息内容
	 */
	static void info_imp(SpdLoggerPtr logger, const char* message);
	
	/**
	 * @brief 警告级别日志输出实现
	 * @param logger 目标日志器指针
	 * @param message 要输出的消息内容
	 */
	static void warn_imp(SpdLoggerPtr logger, const char* message);
	
	/**
	 * @brief 错误级别日志输出实现
	 * @param logger 目标日志器指针
	 * @param message 要输出的消息内容
	 */
	static void error_imp(SpdLoggerPtr logger, const char* message);
	
	/**
	 * @brief 致命错误级别日志输出实现
	 * @param logger 目标日志器指针
	 * @param message 要输出的消息内容
	 */
	static void fatal_imp(SpdLoggerPtr logger, const char* message);
	
	/** @} */

	/**
	 * @brief 初始化指定分类的日志器
	 * @param catName 日志分类名称
	 * @param cfgLogger 日志器配置参数
	 * 
	 * 根据配置参数创建并注册一个新的日志器实例，支持同步和异步两种模式。
	 */
	static void initLogger(const char* catName, WTSVariant* cfgLogger);
	
	/**
	 * @brief 获取指定名称的日志器
	 * @param logger 日志器名称
	 * @param pattern 动态日志器的模式名称（可选）
	 * @return SpdLoggerPtr 日志器智能指针，如果不存在则返回空指针
	 * 
	 * 如果日志器不存在且提供了pattern参数，会尝试创建动态日志器。
	 */
	static SpdLoggerPtr getLogger(const char* logger, const char* pattern = "");

	/**
	 * @brief 在控制台打印消息（未初始化时使用）
	 * @param buffer 要打印的消息内容
	 * 
	 * 当日志系统未初始化时，直接在控制台输出消息，包含时间戳。
	 */
	static void print_message(const char* buffer);

public:
	/**
	 * @name 原始日志输出接口
	 * @brief 不经过格式化处理的原始日志输出方法
	 * @{
	 */
	
	/**
	 * @brief 直接输出原始日志消息
	 * @param ll 日志级别
	 * @param message 要输出的消息内容
	 * 
	 * 将消息直接输出到根日志器，不进行任何格式化处理。
	 * 如果日志级别低于设定的最低级别，则不会输出。
	 */
	static void log_raw(WTSLogLevel ll, const char* message);

	/**
	 * @brief 按分类输出原始日志消息
	 * @param catName 日志分类名称
	 * @param ll 日志级别
	 * @param message 要输出的消息内容
	 * 
	 * 将消息输出到指定分类的日志器。如果分类日志器不存在，
	 * 则使用根日志器输出。
	 */
	static void log_raw_by_cat(const char* catName, WTSLogLevel ll, const char* message);

	/**
	 * @brief 动态分类输出原始日志消息
	 * @param patttern 日志模式名称
	 * @param catName 日志分类名称
	 * @param ll 日志级别
	 * @param message 要输出的消息内容
	 * 
	 * 使用动态模式创建日志器并输出消息。如果对应的动态日志器
	 * 不存在，会根据模式配置自动创建。
	 */
	static void log_dyn_raw(const char* patttern, const char* catName, WTSLogLevel ll, const char* message);
	
	/** @} */


	/**
	 * @name 格式化日志输出接口
	 * @brief 支持fmt::format风格的格式化日志输出方法
	 * 
	 * 这些模板方法提供了类似于printf的格式化字符串功能，但使用了更现代和安全的fmt库。
	 * 所有方法都会检查日志级别和停止状态，只有在满足条件时才会进行实际的格式化和输出操作。
	 * @{
	 */
	
	/**
	 * @brief 输出调试级别的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 输出调试级别的日志信息，用于开发调试。只有在日志级别为DEBUG或更低时才会输出。
	 * 
	 * 使用示例：
	 * @code
	 * WTSLogger::debug("用户{}登录，时间:{}", userId, timestamp);
	 * @endcode
	 */
	template<typename... Args>
	static void debug(const char* format, const Args& ...args)
	{
		// 检查日志级别和停止状态，避免不必要的格式化操作
		if (m_logLevel > LL_DEBUG || m_bStopped)
			return;

		// 使用线程本地缓冲区进行格式化，避免多线程竞争
		fmtutil::format_to(m_buffer, format, args...);

		// 如果日志系统未初始化，直接打印到控制台
		if (!m_bInited)
		{
			print_message(m_buffer);
			return;
		}

		// 调用内部实现方法输出日志
		debug_imp(m_rootLogger, m_buffer);
	}

	/**
	 * @brief 输出信息级别的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 输出信息级别的日志，用于记录一般的系统信息和状态。
	 * 这是最常用的日志级别，用于记录程序的正常运行状态。
	 */
	template<typename... Args>
	static void info(const char* format, const Args& ...args)
	{
		if (m_logLevel > LL_INFO || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		if (!m_bInited)
		{
			print_message(m_buffer);
			return;
		}

		info_imp(m_rootLogger, m_buffer);
	}

	/**
	 * @brief 输出警告级别的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 输出警告级别的日志，用于记录可能的问题或异常情况。
	 * 警告不会影响程序正常运行，但需要引起关注。
	 */
	template<typename... Args>
	static void warn(const char* format, const Args& ...args)
	{
		if (m_logLevel > LL_WARN || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		if (!m_bInited)
		{
			print_message(m_buffer);
			return;
		}

		warn_imp(m_rootLogger, m_buffer);
	}

	/**
	 * @brief 输出错误级别的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 输出错误级别的日志，用于记录程序运行中的错误。
	 * 错误可能会影响某些功能的正常运行，但不会导致程序崩溃。
	 */
	template<typename... Args>
	static void error(const char* format, const Args& ...args)
	{
		if (m_logLevel > LL_ERROR || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		if (!m_bInited)
		{
			print_message(m_buffer);
			return;
		}

		error_imp(m_rootLogger, m_buffer);
	}

	/**
	 * @brief 输出致命错误级别的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 输出致命错误级别的日志，用于记录严重的系统错误。
	 * 致命错误通常会导致程序无法继续运行或功能严重受损。
	 */
	template<typename... Args>
	static void fatal(const char* format, const Args& ...args)
	{
		if (m_logLevel > LL_FATAL || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		if (!m_bInited)
		{
			print_message(m_buffer);
			return;
		}

		fatal_imp(m_rootLogger, m_buffer);
	}

	/**
	 * @brief 输出指定级别的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param ll 日志级别
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 通用的日志输出方法，可以指定任意的日志级别。
	 * 适用于需要动态决定日志级别的场景。
	 */
	template<typename... Args>
	static void log(WTSLogLevel ll, const char* format, const Args& ...args)
	{
		if (m_logLevel > ll || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		log_raw(ll, m_buffer);
	}

	/**
	 * @brief 按分类输出格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param catName 日志分类名称
	 * @param ll 日志级别
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 将格式化后的日志输出到指定分类的日志器。
	 * 适用于需要按模块或功能分类记录日志的场景。
	 */
	template<typename... Args>
	static void log_by_cat(const char* catName, WTSLogLevel ll, const char* format, const Args& ...args)
	{
		if (m_logLevel > ll || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		log_raw_by_cat(catName, ll, m_buffer);
	}

	/**
	 * @brief 按分类输出带前缀的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param catName 日志分类名称
	 * @param ll 日志级别
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 在日志消息前自动添加分类名称前缀（如[catName]），
	 * 便于在日志中快速识别消息来源。
	 */
	template<typename... Args>
	static void log_by_cat_prefix(const char* catName, WTSLogLevel ll, const char* format, const Args& ...args)
	{
		if (m_logLevel > ll || m_bStopped)
			return;

		// 在缓冲区开头添加分类前缀
		m_buffer[0] = '[';
		strcpy(m_buffer + 1, catName);
		auto offset = strlen(catName);
		m_buffer[offset + 1] = ']';
		char* s = m_buffer + offset + 2;
		log_raw_by_cat(catName, ll, m_buffer);
	}

	/**
	 * @brief 动态模式输出格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param patttern 日志模式名称
	 * @param catName 日志分类名称
	 * @param ll 日志级别
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 使用动态模式创建日志器并输出格式化消息。
	 * 适用于运行时动态创建日志器的场景。
	 */
	template<typename... Args>
	static void log_dyn(const char* patttern, const char* catName, WTSLogLevel ll, const char* format, const Args& ...args)
	{
		if (m_logLevel > ll || m_bStopped)
			return;

		fmtutil::format_to(m_buffer, format, args...);

		log_dyn_raw(patttern, catName, ll, m_buffer);
	}

	/**
	 * @brief 动态模式输出带前缀的格式化日志
	 * @tparam Args 可变参数模板类型
	 * @param patttern 日志模式名称
	 * @param catName 日志分类名称
	 * @param ll 日志级别
	 * @param format 格式化字符串（支持fmt格式）
	 * @param args 格式化参数
	 * 
	 * 使用动态模式创建日志器，并在消息前添加分类前缀。
	 * 结合了动态创建和前缀标识的功能。
	 */
	template<typename... Args>
	static void log_dyn_prefix(const char* patttern, const char* catName, WTSLogLevel ll, const char* format, const Args& ...args)
	{
		if (m_logLevel > ll || m_bStopped)
			return;

		// 在缓冲区开头添加分类前缀
		m_buffer[0] = '[';
		strcpy(m_buffer+1, catName);
		auto offset = strlen(catName);
		m_buffer[offset + 1] = ']';
		char* s = m_buffer + offset + 2;
		fmtutil::format_to(s, format, args...);
		log_dyn_raw(patttern, catName, ll, m_buffer);
	}
	
	/** @} */

	/**
	 * @name 日志系统管理接口
	 * @brief 日志系统的初始化、配置和生命周期管理
	 * @{
	 */

	/**
	 * @brief 初始化日志系统
	 * @param propFile 日志配置文件路径或配置内容，默认为"logcfg.json"
	 * @param isFile 是否为文件路径，true表示propFile是文件路径，false表示是配置内容
	 * @param handler 自定义日志处理器，可选参数
	 * 
	 * 根据配置文件或配置内容初始化日志系统，设置各种日志器和输出目标。
	 * 配置文件支持JSON格式，可以配置多个日志器、输出格式、文件路径等。
	 */
	static void init(const char* propFile = "logcfg.json", bool isFile = true, ILogHandler* handler = NULL);

	/**
	 * @brief 注册自定义日志处理器
	 * @param handler 日志处理器指针，可以为NULL
	 * 
	 * 注册一个自定义的日志处理器，用于接收所有的日志消息。
	 * 处理器可以将日志转发到其他系统或进行特殊处理。
	 */
	static void registerHandler(ILogHandler* handler = NULL);

	/**
	 * @brief 停止日志系统
	 * 
	 * 停止所有日志输出，释放相关资源。调用此方法后，
	 * 所有的日志输出操作都会被忽略。通常在程序退出前调用。
	 */
	static void stop();

	/**
	 * @brief 释放所有动态创建的日志器
	 * 
	 * 清理所有通过动态模式创建的日志器，释放相关资源。
	 * 主要用于内存管理和资源清理。
	 */
	static void freeAllDynLoggers();
	
	/** @} */

private:
	/**
	 * @name 日志系统状态变量
	 * @brief 控制日志系统运行状态的静态变量
	 * @{
	 */
	static bool					m_bInited;      ///< 日志系统是否已初始化标志
	static bool					m_bTpInited;    ///< 线程池是否已初始化标志（异步日志使用）
	static bool					m_bStopped;     ///< 日志系统是否已停止标志
	/** @} */
	
	/**
	 * @name 日志处理组件
	 * @brief 日志系统的核心处理组件
	 * @{
	 */
	static ILogHandler*			m_logHandler;   ///< 自定义日志处理器指针
	static WTSLogLevel			m_logLevel;     ///< 全局日志级别过滤器
	static SpdLoggerPtr			m_rootLogger;   ///< 根日志器（默认日志器）
	/** @} */

	/**
	 * @name 动态日志管理
	 * @brief 用于管理动态创建的日志器
	 * @{
	 */
	typedef WTSHashMap<std::string>	LogPatterns;          ///< 日志模式映射类型定义
	static LogPatterns*				m_mapPatterns;        ///< 日志模式配置映射表
	static std::set<std::string>	m_setDynLoggers;      ///< 动态创建的日志器名称集合
	/** @} */

	/**
	 * @brief 线程本地日志缓冲区
	 * 
	 * 每个线程都有独立的格式化缓冲区，避免多线程环境下的竞争条件。
	 * 缓冲区大小为MAX_LOG_BUF_SIZE（2KB），足够处理大部分日志消息。
	 */
	static thread_local char	m_buffer[MAX_LOG_BUF_SIZE];
};


