/*!
 * \file WtDtPorter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtDtPorter模块对外接口实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtDtPorter模块对外提供的所有C语言接口函数。
 * 
 * 主要功能包括：
 * 1. 实现数据服务的初始化和启动功能
 * 2. 实现版本信息查询功能
 * 3. 实现日志输出功能
 * 4. 实现扩展Parser（行情解析器）的创建和管理功能
 * 5. 实现扩展Dumper（数据转储器）的创建和管理功能
 * 6. 实现扩展Parser的回调函数注册功能
 * 7. 实现扩展Dumper的回调函数注册功能
 * 8. 实现行情数据推送功能
 * 9. 提供全局WtDtRunner单例的访问接口
 * 10. 在Windows平台下启用MiniDumper，用于程序崩溃时生成转储文件
 * 
 * 设计思想：
 * - 所有接口函数都是对WtDtRunner的简单封装和转发
 * - 使用全局单例模式管理WtDtRunner，确保系统中只有一个运行器实例
 * - 在Windows平台下启用崩溃转储功能，便于问题诊断
 * - 接口实现简洁清晰，便于维护和调试
 * - 支持跨平台编译，自动识别Windows和Unix平台
 * 
 * 该文件是WtDtPorter模块的核心实现，所有外部调用最终都通过WtDtRunner处理。
 */
#include "WtDtPorter.h"  // 包含WtDtPorter接口声明
#include "WtDtRunner.h"  // 包含WtDtRunner类声明

#include "../WtDtCore/WtHelper.h"  // 包含辅助工具函数
#include "../WTSTools/WTSLogger.h"  // 包含日志工具

#include "../Share/ModuleHelper.hpp"  // 包含模块辅助函数
#include "../Includes/WTSVersion.h"  // 包含版本信息定义

// 根据编译平台定义平台名称字符串
#ifdef _WIN32  // Windows平台
#ifdef _WIN64  // 64位Windows
char PLATFORM_NAME[] = "X64";  // 平台名称：X64
#else  // 32位Windows
char PLATFORM_NAME[] = "X86";  // 平台名称：X86
#endif
#else  // Unix/Linux平台
char PLATFORM_NAME[] = "UNIX";  // 平台名称：UNIX
#endif

// Windows平台下的MiniDumper支持
#ifdef _MSC_VER  // 仅在MSVC编译器下编译
#include "../Common/mdump.h"  // 包含MiniDumper头文件
#include <boost/filesystem.hpp>  // 包含Boost文件系统库

/**
 * @brief 获取当前模块文件名
 * @return const char* 模块文件名字符串
 * 
 * 该函数主要用于MiniDumper，获取当前DLL模块的文件名。
 * 使用静态变量缓存文件名，避免重复获取。
 * 
 * 仅在Windows平台（MSVC编译器）下有效。
 */
const char* getModuleName()
{
	static char MODULE_NAME[250] = { 0 };  // 静态缓存，存储模块文件名
	if (strlen(MODULE_NAME) == 0)  // 如果尚未获取过文件名
	{
		// 获取当前DLL模块的完整路径
		GetModuleFileName(g_dllModule, MODULE_NAME, 250);
		// 提取文件名部分（去掉路径）
		boost::filesystem::path p(MODULE_NAME);
		strcpy(MODULE_NAME, p.filename().string().c_str());
	}

	return MODULE_NAME;  // 返回模块文件名
}
#endif

/**
 * @brief 获取全局WtDtRunner单例
 * @return WtDtRunner& WtDtRunner单例引用
 * 
 * 该函数返回全局唯一的WtDtRunner实例。
 * 使用函数内部静态变量实现单例模式，保证线程安全（C++11起）。
 * 
 * 所有数据服务操作都通过这个单例进行，确保系统的一致性。
 */
WtDtRunner& getRunner()
{
	static WtDtRunner runner;  // 静态局部变量，实现单例模式
	return runner;  // 返回单例引用
}

/**
 * @brief 初始化数据服务
 * @param cfgFile 配置文件路径或配置内容字符串
 * @param logCfg 日志配置文件路径或配置内容字符串
 * @param bCfgFile cfgFile是否为文件路径
 * @param bLogCfgFile logCfg是否为文件路径
 * 
 * 该函数初始化WtDtPorter数据服务。
 * 在Windows平台下，首先启用MiniDumper用于崩溃转储；
 * 然后调用WtDtRunner的初始化方法，加载配置文件和日志配置。
 */
void initialize(WtString cfgFile, WtString logCfg, bool bCfgFile, bool bLogCfgFile)
{
#ifdef _MSC_VER  // Windows平台下启用MiniDumper
	// 启用MiniDumper，程序崩溃时会在当前工作目录生成dump文件
	CMiniDumper::Enable(getModuleName(), true, WtHelper::get_cwd());
#endif
	// 调用WtDtRunner的初始化方法
	// 参数：配置文件、日志配置、模块目录、配置类型标志、日志配置类型标志
	getRunner().initialize(cfgFile, logCfg, getBinDir(), bCfgFile, bLogCfgFile);
}

/**
 * @brief 启动数据服务
 * @param bAsync 是否异步启动，默认为false（同步启动）
 * 
 * 该函数启动数据服务，开始运行行情解析器和数据管理器。
 * 如果bAsync为false，函数会阻塞当前线程，直到接收到退出信号；
 * 如果bAsync为true，函数会立即返回，数据服务在后台运行。
 */
void start(bool bAsync/* = false*/)
{
	// 调用WtDtRunner的启动方法，传递异步标志
	getRunner().start(bAsync);
}

/**
 * @brief 获取版本信息
 * @return const char* 版本信息字符串
 * 
 * 该函数返回WtDtPorter模块的版本信息。
 * 版本信息包括平台类型、版本号、编译日期和时间。
 * 使用静态字符串缓存版本信息，避免重复构建。
 */
const char* get_version()
{
	static std::string _ver;  // 静态变量缓存版本信息
	if (_ver.empty())  // 如果尚未构建版本信息
	{
		// 构建版本信息字符串
		_ver = PLATFORM_NAME;  // 平台名称：X64/X86/UNIX
		_ver += " ";
		_ver += WT_VERSION;  // 版本号，定义在WTSVersion.h中
		_ver += " Build@";
		_ver += __DATE__;  // 编译日期
		_ver += " ";
		_ver += __TIME__;  // 编译时间
	}
	return _ver.c_str();  // 返回版本信息字符串
}

/**
 * @brief 输出日志
 * @param level 日志级别
 * @param message 日志内容
 * @param catName 日志分类名称
 * 
 * 该函数输出日志到系统日志系统。
 * 如果指定了日志分类名称，则按分类输出；否则按默认方式输出。
 */
void write_log(unsigned int level, const char* message, const char* catName)
{
	if (strlen(catName) > 0)  // 如果指定了日志分类名称
	{
		// 按分类输出日志
		WTSLogger::log_raw_by_cat(catName, (WTSLogLevel)level, message);
	}
	else  // 如果未指定日志分类名称
	{
		// 按默认方式输出日志
		WTSLogger::log_raw((WTSLogLevel)level, message);
	}
}

#pragma region "扩展Parser接口"

/**
 * @brief 创建扩展行情解析器
 * @param id 解析器唯一标识符
 * @return bool 创建成功返回true，失败返回false
 * 
 * 该函数创建一个扩展行情解析器实例。
 * 调用WtDtRunner的createExtParser方法创建解析器。
 */
bool create_ext_parser(const char* id)
{
	// 调用WtDtRunner的createExtParser方法，传递解析器ID
	return getRunner().createExtParser(id);
}

/**
 * @brief 向底层推送tick数据
 * @param id 解析器ID
 * @param curTick 最新tick数据指针
 * @param uProcFlag 处理标记
 * 
 * 该函数将外部接收到的tick行情数据推送到系统中。
 * 调用WtDtRunner的on_ext_parser_quote方法处理行情数据。
 */
void parser_push_quote(const char* id, WTSTickStruct* curTick, WtUInt32 uProcFlag)
{
	// 调用WtDtRunner的on_ext_parser_quote方法，传递解析器ID、tick数据和处理标记
	getRunner().on_ext_parser_quote(id, curTick, uProcFlag);
}

/**
 * @brief 注册扩展Parser的回调函数
 * @param cbEvt 行情解析器事件回调函数
 * @param cbSub 行情订阅回调函数
 * 
 * 该函数注册扩展Parser的回调函数。
 * 调用WtDtRunner的registerParserPorter方法注册回调函数。
 */
void register_parser_callbacks(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub)
{
	// 调用WtDtRunner的registerParserPorter方法，传递事件回调和订阅回调
	getRunner().registerParserPorter(cbEvt, cbSub);
}


#pragma endregion "扩展Parser接口"

#pragma region "扩展Dumper接口"

/**
 * @brief 创建扩展数据转储器
 * @param id 转储器唯一标识符
 * @return bool 创建成功返回true，失败返回false
 * 
 * 该函数创建一个扩展数据转储器实例。
 * 调用WtDtRunner的createExtDumper方法创建转储器。
 */
bool create_ext_dumper(const char* id)
{
	// 调用WtDtRunner的createExtDumper方法，传递转储器ID
	return getRunner().createExtDumper(id);
}

/**
 * @brief 注册扩展Dumper的回调函数（K线和Tick）
 * @param barDumper K线数据转储回调函数
 * @param tickDumper Tick数据转储回调函数
 * 
 * 该函数注册扩展Dumper的基础数据转储回调函数。
 * 调用WtDtRunner的registerExtDumper方法注册回调函数。
 */
void register_extended_dumper(FuncDumpBars barDumper, FuncDumpTicks tickDumper)
{
	// 调用WtDtRunner的registerExtDumper方法，传递K线转储回调和Tick转储回调
	getRunner().registerExtDumper(barDumper, tickDumper);
}

/**
 * @brief 注册扩展Dumper的回调函数（高频数据）
 * @param ordQueDumper 委托队列数据转储回调函数
 * @param ordDtlDumper 委托明细数据转储回调函数
 * @param transDumper 逐笔成交数据转储回调函数
 * 
 * 该函数注册扩展Dumper的高频数据转储回调函数。
 * 调用WtDtRunner的registerExtHftDataDumper方法注册回调函数。
 */
void register_extended_hftdata_dumper(FuncDumpOrdQue ordQueDumper, FuncDumpOrdDtl ordDtlDumper, FuncDumpTrans transDumper)
{
	// 调用WtDtRunner的registerExtHftDataDumper方法，传递委托队列、委托明细、逐笔成交转储回调
	getRunner().registerExtHftDataDumper(ordQueDumper, ordDtlDumper, transDumper);
}
#pragma endregion "扩展Dumper接口"

