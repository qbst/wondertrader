/*!
 * \file main.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader UFT策略运行器主程序入口文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader UFT策略运行器的主程序入口，提供命令行参数解析和程序启动功能。
 * 该程序是WonderTrader UFT系统的可执行文件入口点，负责解析命令行参数、初始化运行器、加载配置并启动运行。
 * 
 * 主要功能：
 * 1. 命令行参数解析：解析配置文件路径、日志配置文件路径等命令行参数
 * 2. 崩溃转储：在Windows平台启用MiniDump崩溃转储功能，便于问题排查
 * 3. 程序启动：创建WtUftRunner实例，初始化日志系统，加载配置并启动运行
 * 4. 帮助信息：提供命令行帮助信息，显示程序使用方法
 * 
 * 命令行参数说明：
 * - -c, --config: 指定配置文件路径，默认为"./config.yaml"
 * - -l, --logcfg: 指定日志配置文件路径，默认为"./logcfg.yaml"
 * - -h, --help: 显示帮助信息
 * 
 * 使用流程：
 * 1. 解析命令行参数
 * 2. 如果请求帮助信息，显示帮助并退出
 * 3. 创建WtUftRunner实例
 * 4. 初始化日志系统（使用日志配置文件）
 * 5. 加载主配置文件并初始化各组件
 * 6. 启动运行（同步模式，阻塞直到退出）
 * 
 * 设计特点：
 * - 命令行接口：提供友好的命令行参数解析功能
 * - 默认配置：提供默认配置文件路径，简化使用
 * - 崩溃转储：在Windows平台自动启用崩溃转储功能
 * - 同步运行：默认使用同步运行模式，阻塞主线程直到程序退出
 */

#include "WtUftRunner.h"  // 包含UFT策略运行器头文件，使用WtUftRunner类

#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能

#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
#include "../Common/mdump.h"  // 包含MiniDump头文件，提供崩溃转储功能
#endif

#include "../Share/cppcli.hpp"  // 包含命令行参数解析库，提供命令行参数解析功能
//#include <vld.h>  // Visual Leak Detector头文件（内存泄漏检测工具，已注释）

/**
 * @brief 主函数
 * 
 * WonderTrader UFT策略运行器的主程序入口点。
 * 
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码，0表示正常退出
 * 
 * 程序流程：
 * 1. 在Windows平台启用MiniDump崩溃转储功能
 * 2. 解析命令行参数
 * 3. 如果请求帮助信息，显示帮助并退出
 * 4. 创建WtUftRunner实例
 * 5. 初始化日志系统（使用日志配置文件）
 * 6. 加载主配置文件并初始化各组件
 * 7. 启动运行（同步模式，阻塞直到退出）
 */
int main(int argc, char* argv[])
{
#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
	CMiniDumper::Enable("WtUftRunner.exe", true);  // 启用MiniDump崩溃转储功能
	// 参数说明：
	// - "WtUftRunner.exe": 模块名称，用于转储文件命名
	// - true: 是否启用完整转储
#endif

	cppcli::Option opt(argc, argv);  // 创建命令行参数解析器对象

	auto cParam = opt("-c", "--config", "configure filepath, dtcfg.yaml as default", false);  // 定义配置文件参数
	// 参数说明：
	// - "-c": 短参数名
	// - "--config": 长参数名
	// - "configure filepath, dtcfg.yaml as default": 参数描述
	// - false: 是否为必需参数（false表示可选）
	auto lParam = opt("-l", "--logcfg", "logging configure filepath, logcfgbt.yaml as default", false);  // 定义日志配置文件参数
	// 参数说明：
	// - "-l": 短参数名
	// - "--logcfg": 长参数名
	// - "logging configure filepath, logcfgbt.yaml as default": 参数描述
	// - false: 是否为必需参数（false表示可选）

	auto hParam = opt("-h", "--help", "gain help doc", false)->asHelpParam();  // 定义帮助参数
	// 参数说明：
	// - "-h": 短参数名
	// - "--help": 长参数名
	// - "gain help doc": 参数描述
	// - false: 是否为必需参数（false表示可选）
	// - ->asHelpParam(): 设置为帮助参数，解析器会自动显示帮助信息

	opt.parse();  // 解析命令行参数

	if (hParam->exists())  // 如果用户请求帮助信息
		return 0;  // 直接退出（帮助信息已由解析器自动显示）

	std::string filename;  // 定义文件名变量
	if (lParam->exists())  // 如果用户指定了日志配置文件路径
		filename = lParam->get<std::string>();  // 获取用户指定的日志配置文件路径
	else  // 如果用户未指定日志配置文件路径
		filename = "./logcfg.yaml";  // 使用默认日志配置文件路径

	WtUftRunner runner;  // 创建UFT策略运行器实例
	runner.init(filename);  // 初始化UFT策略运行器，传入日志配置文件路径

	if (cParam->exists())  // 如果用户指定了配置文件路径
		filename = cParam->get<std::string>();  // 获取用户指定的配置文件路径
	else  // 如果用户未指定配置文件路径
		filename = "./config.yaml";  // 使用默认配置文件路径
	runner.config(filename);  // 配置UFT策略运行器，传入配置文件路径

	runner.run(false);  // 启动运行，false表示同步运行（阻塞主线程）
	return 0;  // 程序正常退出，返回0
}

