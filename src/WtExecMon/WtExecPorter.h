/*!
 * \file WtExecPorter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader执行器模块C接口导出头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader执行器模块的C语言接口，供外部程序（如Python、C#等）通过动态库调用。
 * 采用C接口设计，确保跨语言兼容性和动态库的稳定导出。
 * 
 * 主要功能：
 * 1. 模块初始化：初始化日志系统和执行器运行环境
 * 2. 配置加载：从配置文件加载执行器、交易通道、行情通道等配置
 * 3. 运行控制：启动和停止执行器模块的运行
 * 4. 日志记录：提供统一的日志记录接口，支持不同日志级别和分类
 * 5. 版本查询：获取执行器模块的版本信息
 * 6. 仓位管理：设置目标仓位并提交执行
 * 7. 资源释放：清理执行器模块占用的资源
 * 
 * 设计特点：
 * - C接口导出：使用extern "C"确保函数名不被C++编译器名称修饰，可以被C语言直接调用
 * - 跨语言兼容：C接口设计使得Python、C#、Java等语言可以通过FFI调用
 * - 动态库导出：使用EXPORT_FLAG宏确保函数可以被动态库正确导出
 * - 参数简化：使用WtString类型别名简化字符串参数类型
 * - 默认参数：C++默认参数仅在C++调用时有效，C语言调用需要显式传递所有参数
 * 
 * 使用场景：
 * - Python通过ctypes调用执行器模块
 * - C#通过P/Invoke调用执行器模块
 * - 其他语言通过FFI机制调用执行器模块
 * - 作为独立进程运行时，通过命令行参数初始化
 */

#pragma once  // 防止头文件被重复包含

#include <stdint.h>  // 包含标准整数类型定义，提供uint32_t等类型
#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义，提供EXPORT_FLAG等导出宏

typedef const char*			WtString;  // 定义字符串类型别名，用于C接口的字符串参数

#ifdef __cplusplus  // 如果是C++编译环境
extern "C"  // 使用C语言链接规范，确保函数名不被C++编译器进行名称修饰
{
#endif

	/**
	 * @brief 初始化执行器模块
	 * 
	 * 初始化执行器模块的日志系统和运行环境。
	 * 该函数必须在调用其他函数之前调用，且只能调用一次。
	 * 
	 * @param logCfg 日志配置文件路径或配置内容字符串
	 * @param isFile 是否为文件路径，true表示logCfg是文件路径，false表示logCfg是配置内容
	 * 
	 * 使用示例：
	 * - init_exec("logcfgexec.json", true);  // 从文件加载日志配置
	 * - init_exec("{\"level\":\"info\"}", false);  // 从字符串加载日志配置
	 */
	EXPORT_FLAG	void		init_exec(WtString logCfg, bool isFile = true);

	/**
	 * @brief 配置执行器模块
	 * 
	 * 从配置文件加载执行器、交易通道、行情通道等配置信息。
	 * 配置包括：基础数据文件、数据管理器、开平策略、行情通道、交易通道、执行器等。
	 * 
	 * @param cfgfile 配置文件路径或配置内容字符串，如果为空字符串则使用默认配置文件"cfgexec.json"
	 * @param isFile 是否为文件路径，true表示cfgfile是文件路径，false表示cfgfile是配置内容
	 * 
	 * 配置项说明：
	 * - basefiles: 基础数据文件配置（交易时段、商品、合约、节假日等）
	 * - data: 数据管理器配置
	 * - bspolicy: 开平策略配置文件路径
	 * - parsers: 行情通道配置文件路径
	 * - traders: 交易通道配置文件路径
	 * - executers: 执行器配置文件路径
	 */
	EXPORT_FLAG	void		config_exec(WtString cfgfile, bool isFile = true);

	/**
	 * @brief 运行执行器模块
	 * 
	 * 启动执行器模块的运行，包括启动行情通道和交易通道。
	 * 该函数会阻塞当前线程，直到模块停止运行。
	 * 
	 * 运行流程：
	 * 1. 启动行情通道（ParserAdapter），接收实时行情数据
	 * 2. 启动交易通道（TraderAdapter），处理交易指令
	 * 3. 执行器根据行情数据和目标仓位执行交易逻辑
	 * 
	 * 注意事项：
	 * - 该函数会阻塞当前线程，建议在独立线程中调用
	 * - 调用release_exec()可以停止运行
	 */
	EXPORT_FLAG	void		run_exec();

	/**
	 * @brief 写入日志
	 * 
	 * 记录日志信息，支持不同日志级别和日志分类。
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
	 */
	EXPORT_FLAG	void		write_log(unsigned int level, WtString message, WtString catName);

	/**
	 * @brief 获取执行器模块版本信息
	 * 
	 * 返回执行器模块的版本信息字符串，包括平台类型、版本号、编译日期和时间。
	 * 
	 * @return 返回版本信息字符串指针，格式如："X64 1.0.0 Build@Mar 30 2020 12:00:00"
	 * 
	 * 版本信息格式：
	 * - 平台类型：X64（64位Windows）、X86（32位Windows）、UNIX（Linux/Unix）
	 * - 版本号：从WT_VERSION宏获取
	 * - 编译日期：从__DATE__宏获取
	 * - 编译时间：从__TIME__宏获取
	 * 
	 * 注意事项：
	 * - 返回的字符串指针指向静态存储区，不需要释放
	 * - 多次调用返回相同的字符串指针
	 */
	EXPORT_FLAG	WtString	get_version();

	/**
	 * @brief 释放执行器模块资源
	 * 
	 * 清理执行器模块占用的资源，停止日志系统。
	 * 调用此函数后，模块将无法继续使用，需要重新初始化。
	 * 
	 * 释放流程：
	 * 1. 停止日志系统
	 * 2. 清理执行器、交易通道、行情通道等资源
	 * 3. 释放配置对象和缓存数据
	 */
	EXPORT_FLAG	void		release_exec();

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
	EXPORT_FLAG	void		set_position(WtString stdCode, double targetPos);

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
	EXPORT_FLAG	void		commit_positions();

#ifdef __cplusplus  // 如果是C++编译环境
}  // extern "C"结束
#endif