/*!
 * \file WtExecPorter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader执行器模块C接口导出实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtExecPorter.h中定义的所有C接口函数，作为执行器模块与外部程序的桥梁。
 * 通过单例模式管理WtExecRunner实例，确保整个模块只有一个执行器运行器实例。
 * 
 * 主要功能：
 * 1. 单例管理：通过getRunner()函数获取全局唯一的WtExecRunner实例
 * 2. 初始化实现：实现init_exec()函数，初始化日志系统和执行器环境
 * 3. 配置实现：实现config_exec()函数，加载配置文件并初始化各组件
 * 4. 运行实现：实现run_exec()函数，启动执行器模块的运行
 * 5. 日志实现：实现write_log()函数，提供日志记录功能
 * 6. 版本实现：实现get_version()函数，返回版本信息字符串
 * 7. 释放实现：实现release_exec()函数，清理模块资源
 * 8. 仓位管理实现：实现set_position()和commit_positions()函数，管理目标仓位
 * 
 * 设计特点：
 * - 单例模式：使用静态局部变量实现单例，确保线程安全（C++11标准保证）
 * - 平台检测：根据编译平台自动设置平台名称（X64/X86/UNIX）
 * - 版本信息：组合平台、版本号、编译时间等信息生成版本字符串
 * - 日志分类：支持按分类记录日志，便于日志管理
 * - 资源管理：统一管理执行器运行器的生命周期
 */

#include "WtExecPorter.h"  // 包含当前头文件，获取函数声明和类型定义
#include "WtExecRunner.h"  // 包含执行器运行器头文件，使用WtExecRunner类

#include "../WtCore/WtHelper.h"  // 包含WonderTrader辅助工具类，提供路径、时间等工具函数
#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能
#include "../Includes/WTSVersion.h"  // 包含版本号定义，提供WT_VERSION宏

#ifdef _WIN32  // 如果是Windows平台
#   ifdef _WIN64  // 如果是64位Windows
    char PLATFORM_NAME[] = "X64";  // 设置平台名称为X64（64位Windows）
#   else  // 如果是32位Windows
    char PLATFORM_NAME[] = "X86";  // 设置平台名称为X86（32位Windows）
#   endif
#else  // 如果是非Windows平台（Linux/Unix）
    char PLATFORM_NAME[] = "UNIX";  // 设置平台名称为UNIX（Linux/Unix系统）
#endif

/**
 * @brief 获取执行器运行器单例实例
 * 
 * 使用静态局部变量实现单例模式，确保整个模块只有一个WtExecRunner实例。
 * C++11标准保证静态局部变量的初始化是线程安全的。
 * 
 * @return 返回WtExecRunner实例的引用
 * 
 * 单例模式说明：
 * - 第一次调用时创建实例，后续调用返回同一个实例
 * - 线程安全：C++11标准保证静态局部变量的初始化是线程安全的
 * - 生命周期：实例在程序结束时自动销毁
 */
WtExecRunner& getRunner()
{
	static WtExecRunner runner;  // 静态局部变量，实现单例模式
	return runner;  // 返回执行器运行器实例的引用
}

/**
 * @brief 初始化执行器模块
 * 
 * 初始化执行器模块的日志系统和运行环境。
 * 该函数使用静态标志确保只初始化一次，避免重复初始化。
 * 
 * @param logCfg 日志配置文件路径或配置内容字符串
 * @param isFile 是否为文件路径，true表示logCfg是文件路径，false表示logCfg是配置内容
 * 
 * 初始化流程：
 * 1. 检查是否已初始化，如果已初始化则直接返回
 * 2. 调用WtExecRunner::init()初始化执行器运行器
 * 3. 设置初始化标志为true
 * 
 * 注意事项：
 * - 该函数只能调用一次，重复调用会被忽略
 * - 必须在调用其他函数之前调用此函数
 */
void init_exec(WtString logCfg, bool isFile /*= true*/)
{
	static bool inited = false;  // 静态初始化标志，确保只初始化一次

	if (inited)  // 如果已初始化，直接返回
		return;

	getRunner().init(logCfg);  // 调用执行器运行器的初始化方法，初始化日志系统

	inited = true;  // 设置初始化标志为true
}

/**
 * @brief 配置执行器模块
 * 
 * 从配置文件加载执行器、交易通道、行情通道等配置信息。
 * 如果配置文件路径为空，则使用默认配置文件"cfgexec.json"。
 * 
 * @param cfgfile 配置文件路径或配置内容字符串，如果为空字符串则使用默认配置文件
 * @param isFile 是否为文件路径，true表示cfgfile是文件路径，false表示cfgfile是配置内容
 * 
 * 配置流程：
 * 1. 检查配置文件路径是否为空
 * 2. 如果为空，使用默认配置文件"cfgexec.json"
 * 3. 调用WtExecRunner::config()加载配置并初始化各组件
 * 
 * 配置项说明：
 * - basefiles: 基础数据文件配置（交易时段、商品、合约、节假日等）
 * - data: 数据管理器配置
 * - bspolicy: 开平策略配置文件路径
 * - parsers: 行情通道配置文件路径
 * - traders: 交易通道配置文件路径
 * - executers: 执行器配置文件路径
 */
void config_exec(WtString cfgfile, bool isFile /*= true*/)
{
	if (strlen(cfgfile) == 0)  // 如果配置文件路径为空
		getRunner().config("cfgexec.json");  // 使用默认配置文件"cfgexec.json"
	else  // 如果配置文件路径不为空
		getRunner().config(cfgfile);  // 使用指定的配置文件路径
}

/**
 * @brief 运行执行器模块
 * 
 * 启动执行器模块的运行，包括启动行情通道和交易通道。
 * 该函数会阻塞当前线程，直到模块停止运行。
 * 
 * 运行流程：
 * 1. 调用WtExecRunner::run()启动执行器运行器
 * 2. 启动行情通道（ParserAdapter），接收实时行情数据
 * 3. 启动交易通道（TraderAdapter），处理交易指令
 * 4. 执行器根据行情数据和目标仓位执行交易逻辑
 * 
 * 注意事项：
 * - 该函数会阻塞当前线程，建议在独立线程中调用
 * - 调用release_exec()可以停止运行
 */
void run_exec()
{
	getRunner().run();  // 调用执行器运行器的运行方法，启动行情通道和交易通道
}

/**
 * @brief 释放执行器模块资源
 * 
 * 清理执行器模块占用的资源，停止日志系统。
 * 调用此函数后，模块将无法继续使用，需要重新初始化。
 * 
 * 释放流程：
 * 1. 调用WtExecRunner::release()释放执行器运行器资源
 * 2. 停止日志系统
 * 3. 清理执行器、交易通道、行情通道等资源
 */
void release_exec()
{
	getRunner().release();  // 调用执行器运行器的释放方法，清理资源并停止日志系统
}

/**
 * @brief 获取执行器模块版本信息
 * 
 * 返回执行器模块的版本信息字符串，包括平台类型、版本号、编译日期和时间。
 * 使用静态变量缓存版本字符串，避免重复构建。
 * 
 * @return 返回版本信息字符串指针，格式如："X64 1.0.0 Build@Mar 30 2020 12:00:00"
 * 
 * 版本信息格式：
 * - 平台类型：X64（64位Windows）、X86（32位Windows）、UNIX（Linux/Unix）
 * - 版本号：从WT_VERSION宏获取
 * - 编译日期：从__DATE__宏获取（格式：MMM DD YYYY）
 * - 编译时间：从__TIME__宏获取（格式：HH:MM:SS）
 * 
 * 注意事项：
 * - 返回的字符串指针指向静态存储区，不需要释放
 * - 多次调用返回相同的字符串指针
 * - 版本字符串在第一次调用时构建，后续调用直接返回缓存值
 */
WtString get_version()
{
	static std::string _ver;  // 静态版本字符串，缓存版本信息
	if (_ver.empty())  // 如果版本字符串为空，构建版本信息
	{
		_ver = PLATFORM_NAME;  // 添加平台名称（X64/X86/UNIX）
		_ver += " ";  // 添加空格分隔符
		_ver += WT_VERSION;  // 添加版本号（从WT_VERSION宏获取）
		_ver += " Build@";  // 添加构建标识
		_ver += __DATE__;  // 添加编译日期（从__DATE__宏获取，格式：MMM DD YYYY）
		_ver += " ";  // 添加空格分隔符
		_ver += __TIME__;  // 添加编译时间（从__TIME__宏获取，格式：HH:MM:SS）
	}
	return _ver.c_str();  // 返回版本字符串的C字符串指针
}

/**
 * @brief 写入日志
 * 
 * 记录日志信息，支持不同日志级别和日志分类。
 * 如果指定了分类名称，则使用分类日志记录；否则使用默认日志记录。
 * 
 * @param level 日志级别，使用WTSLogLevel枚举值（如LL_DEBUG、LL_INFO、LL_WARN、LL_ERROR）
 * @param message 日志消息内容
 * @param catName 日志分类名称，如果为空字符串则使用默认分类
 * 
 * 日志级别说明：
 * - LL_DEBUG: 调试信息，用于开发调试
 * - LL_INFO: 一般信息，用于记录正常运行状态
 * - LL_WARN: 警告信息，用于记录潜在问题
 * - LL_ERROR: 错误信息，用于记录错误情况
 * 
 * 日志分类说明：
 * - 如果catName不为空，使用log_raw_by_cat()按分类记录日志
 * - 如果catName为空，使用log_raw()记录到默认分类
 */
void write_log(unsigned int level, WtString message, WtString catName)
{
	if (strlen(catName) > 0)  // 如果指定了日志分类名称
	{
		WTSLogger::log_raw_by_cat(catName, (WTSLogLevel)level, message);  // 按分类记录日志
	}
	else  // 如果未指定日志分类名称
	{
		WTSLogger::log_raw((WTSLogLevel)level, message);  // 记录到默认分类
	}
}

/**
 * @brief 设置目标仓位
 * 
 * 设置指定合约的目标持仓数量。
 * 目标仓位会被缓存，直到调用commit_positions()提交执行。
 * 
 * @param stdCode 标准合约代码，如"SHFE.rb2305"、"CFFEX.IF2303"等
 * @param targetPos 目标持仓数量，正数表示多头，负数表示空头，0表示平仓
 * 
 * 使用流程：
 * 1. 多次调用set_position()设置各合约的目标仓位
 * 2. 调用commit_positions()提交所有目标仓位，执行器会根据当前持仓和目标持仓的差异生成交易指令
 * 
 * 注意事项：
 * - 目标仓位是累计的，多次设置同一合约会覆盖之前的值
 * - 目标仓位不会立即执行，需要调用commit_positions()才会生效
 */
void set_position(WtString stdCode, double targetPos)
{
	getRunner().setPosition(stdCode, targetPos);  // 调用执行器运行器的设置仓位方法，缓存目标仓位
}

/**
 * @brief 提交目标仓位
 * 
 * 将所有已设置的目标仓位提交给执行器执行。
 * 执行器会根据当前持仓和目标持仓的差异，生成相应的交易指令。
 * 
 * 执行流程：
 * 1. 获取各合约的当前持仓
 * 2. 计算目标持仓与当前持仓的差异
 * 3. 根据差异生成买入/卖出/平仓指令
 * 4. 通过交易通道执行交易指令
 * 5. 清空目标仓位缓存
 * 
 * 注意事项：
 * - 提交后目标仓位缓存会被清空，需要重新设置
 * - 执行是异步的，不会等待交易完成
 */
void commit_positions()
{
	getRunner().commitPositions();  // 调用执行器运行器的提交仓位方法，执行所有目标仓位
}