/*!
 * \file WtBtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader回测运行器主程序入口文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader回测运行器的主程序入口，提供命令行参数解析和回测程序启动功能。
 * 该程序是WonderTrader回测系统的可执行文件入口点，负责解析命令行参数、初始化回测环境、
 * 加载配置、创建策略模拟器并启动历史数据回放。
 * 
 * 主要功能：
 * 1. 命令行参数解析：解析配置文件路径、日志配置文件路径等命令行参数
 * 2. 崩溃转储：在Windows平台启用MiniDump崩溃转储功能，便于问题排查
 * 3. 回测环境初始化：初始化日志系统、安装信号钩子、加载配置文件
 * 4. 历史数据回放器初始化：创建并初始化历史数据回放器，配置数据源和回放参数
 * 5. 策略模拟器创建：根据配置选择并创建对应的策略模拟器（CTA、HFT、选股、执行器、UFT）
 * 6. 增量回测支持：支持CTA策略的增量回测功能，可以从上次回测结果继续回测
 * 7. 定时任务注册：为选股策略注册定时任务，触发选股逻辑
 * 8. 回测执行：启动历史数据回放，驱动策略模拟器执行回测
 * 
 * 支持的策略类型：
 * - CTA策略：商品交易顾问策略，支持增量回测
 * - HFT策略：高频交易策略
 * - 选股策略：选股策略，支持定时任务
 * - 执行器策略：执行器回测策略
 * - UFT策略：极速交易策略
 * 
 * 命令行参数说明：
 * - -c, --config: 指定配置文件路径，默认为"./configbt.yaml"
 * - -l, --logcfg: 指定日志配置文件路径，默认为"./logcfgdt.yaml"
 * - -h, --help: 显示帮助信息
 * 
 * 使用流程：
 * 1. 解析命令行参数
 * 2. 如果请求帮助信息，显示帮助并退出
 * 3. 初始化日志系统（使用日志配置文件）
 * 4. 安装信号钩子，捕获异常和错误
 * 5. 加载主配置文件
 * 6. 创建并初始化历史数据回放器
 * 7. 根据配置选择并创建对应的策略模拟器
 * 8. 注册模拟器到历史数据回放器
 * 9. 准备回测环境
 * 10. 启动历史数据回放（异步模式）
 * 11. 等待用户按键退出
 * 12. 停止日志系统
 * 
 * 设计特点：
 * - 命令行接口：提供友好的命令行参数解析功能
 * - 默认配置：提供默认配置文件路径，简化使用
 * - 崩溃转储：在Windows平台自动启用崩溃转储功能
 * - 多种策略支持：支持CTA、HFT、选股、执行器、UFT等多种策略类型
 * - 增量回测：支持CTA策略的增量回测功能
 * - 异步回放：使用异步模式回放历史数据，提高回测效率
 */

#include "../WtBtCore/HisDataReplayer.h"  // 包含历史数据回放器头文件，使用HisDataReplayer类
#include "../WtBtCore/CtaMocker.h"  // 包含CTA策略模拟器头文件，使用CtaMocker类
#include "../WtBtCore/ExecMocker.h"  // 包含执行器模拟器头文件，使用ExecMocker类
#include "../WtBtCore/HftMocker.h"  // 包含HFT策略模拟器头文件，使用HftMocker类
#include "../WtBtCore/SelMocker.h"  // 包含选股策略模拟器头文件，使用SelMocker类
#include "../WtBtCore/UftMocker.h"  // 包含UFT策略模拟器头文件，使用UftMocker类
#include "../WtBtCore/WtHelper.h"  // 包含WonderTrader辅助工具类，提供路径、时间等工具函数

#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能
#include "../WTSUtils/SignalHook.hpp"  // 包含信号钩子工具，提供异常捕获功能

#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置加载器，提供配置文件加载功能
#include "../Includes/WTSVariant.hpp"  // 包含配置变体类，提供WTSVariant类型
#include "../Share/StdUtils.hpp"  // 包含标准工具函数，提供文件操作等功能
#include "../Share/cppcli.hpp"  // 包含命令行参数解析库，提供命令行参数解析功能

#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
#include "../Common/mdump.h"  // 包含MiniDump头文件，提供崩溃转储功能
#endif

/**
 * @brief 主函数
 * 
 * WonderTrader回测运行器的主程序入口点。
 * 
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码，0表示正常退出，-1表示配置加载失败
 * 
 * 程序流程：
 * 1. 在Windows平台启用MiniDump崩溃转储功能
 * 2. 解析命令行参数
 * 3. 如果请求帮助信息，显示帮助并退出
 * 4. 初始化日志系统（使用日志配置文件）
 * 5. 安装信号钩子，捕获异常和错误
 * 6. 加载主配置文件
 * 7. 创建并初始化历史数据回放器
 * 8. 根据配置选择并创建对应的策略模拟器
 * 9. 注册模拟器到历史数据回放器
 * 10. 准备回测环境
 * 11. 启动历史数据回放（异步模式）
 * 12. 等待用户按键退出
 * 13. 停止日志系统
 */
int main(int argc, char* argv[])
{
#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
    CMiniDumper::Enable("WtBtRunner.exe", true, WtHelper::getCWD().c_str());  // 启用MiniDump崩溃转储功能
	// 参数说明：
	// - "WtBtRunner.exe": 模块名称，用于转储文件命名
	// - true: 是否启用完整转储
	// - WtHelper::getCWD().c_str(): 转储文件保存目录（当前工作目录）
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
		filename = "./logcfgdt.yaml";  // 使用默认日志配置文件路径
	WTSLogger::init(filename.c_str());  // 初始化日志系统，从文件加载日志配置

	install_signal_hooks([](const char* message) {  // 安装信号钩子，使用lambda表达式作为回调函数
		WTSLogger::error(message);  // 将错误信息记录到日志系统
	});

	if (cParam->exists())  // 如果用户指定了配置文件路径
		filename = cParam->get<std::string>();  // 获取用户指定的配置文件路径
	else  // 如果用户未指定配置文件路径
		filename = "./configbt.yaml";  // 使用默认配置文件路径

	if (!StdFile::exists(filename.c_str()))  // 如果配置文件不存在
	{
		fmt::print("confiture {} not exists", filename);  // 打印错误信息到控制台（注意：这里拼写错误，应该是"configure"）
		return 0;  // 返回0，表示程序退出
	}

	WTSVariant* cfg = WTSCfgLoader::load_from_file(filename.c_str());  // 加载主配置文件
	if (cfg == NULL)  // 如果配置文件加载失败
	{
		WTSLogger::info("Loading configuration file {} failed", filename);  // 记录错误日志
		return -1;  // 返回-1，表示配置加载失败
	}

	// ========== 创建并初始化历史数据回放器 ==========
	HisDataReplayer replayer;  // 创建历史数据回放器实例
	replayer.init(cfg->get("replayer"));  // 初始化历史数据回放器，传入回放器配置节点
	// 回放器配置包括：数据存储路径、回放模式、回测时间范围、Tick回放开关等

	// ========== 根据配置选择并创建对应的策略模拟器 ==========
	WTSVariant* cfgEnv = cfg->get("env");  // 获取环境配置节点
	const char* mode = cfgEnv->getCString("mocker");  // 获取模拟器类型（cta/hft/sel/exec/uft）
	int32_t slippage = cfgEnv->getInt32("slippage");  // 获取滑点参数（单位：最小变动价位）
	
	if (strcmp(mode, "cta") == 0)  // 如果是CTA策略回测
	{
		CtaMocker* mocker = new CtaMocker(&replayer, "cta", slippage);  // 创建CTA策略模拟器实例
		// 参数说明：
		// - &replayer: 历史数据回放器指针
		// - "cta": 策略名称
		// - slippage: 滑点参数
		mocker->init_cta_factory(cfg->get("cta"));  // 初始化CTA策略工厂，加载CTA策略动态库
		const char* stra_id = cfg->get("cta")->get("strategy")->getCString("id");  // 获取策略ID
		
		// 加载增量回测的基础历史回测数据
		const char* incremental_backtest_base = cfg->get("env")->getCString("incremental_backtest_base");  // 获取增量回测基础数据路径
		if (strlen(incremental_backtest_base) > 0)  // 如果配置了增量回测基础数据路径
		{
			mocker->load_incremental_data(incremental_backtest_base);  // 加载增量回测基础数据
			// 增量回测说明：
			// - 可以从上次回测结果继续回测，避免重复计算
			// - 适用于长时间回测场景，提高回测效率
		}
		replayer.register_sink(mocker, stra_id);  // 将CTA模拟器注册到历史数据回放器，作为数据接收者
		// 参数说明：
		// - mocker: 模拟器指针（数据接收者）
		// - stra_id: 策略ID（用于标识模拟器）
	}
	else if (strcmp(mode, "hft") == 0)  // 如果是HFT策略回测
	{
		HftMocker* mocker = new HftMocker(&replayer, "hft");  // 创建HFT策略模拟器实例
		// 参数说明：
		// - &replayer: 历史数据回放器指针
		// - "hft": 策略名称
		mocker->init_hft_factory(cfg->get("hft"));  // 初始化HFT策略工厂，加载HFT策略动态库
		const char* stra_id = cfg->get("hft")->get("strategy")->getCString("id");  // 获取策略ID
		replayer.register_sink(mocker, stra_id);  // 将HFT模拟器注册到历史数据回放器
	}
	else if (strcmp(mode, "sel") == 0)  // 如果是选股策略回测
	{
		SelMocker* mocker = new SelMocker(&replayer, "sel", slippage);  // 创建选股策略模拟器实例
		// 参数说明：
		// - &replayer: 历史数据回放器指针
		// - "sel": 策略名称
		// - slippage: 滑点参数
		mocker->init_sel_factory(cfg->get("sel"));  // 初始化选股策略工厂，加载选股策略动态库
		const char* stra_id = cfg->get("sel")->get("strategy")->getCString("id");  // 获取策略ID
		replayer.register_sink(mocker, stra_id);  // 将选股模拟器注册到历史数据回放器

		// 注册定时任务（选股策略需要定时触发选股逻辑）
		replayer.register_task(mocker->id(), cfg->get("sel")->get("task")->getUInt32("date"),
			cfg->get("sel")->get("task")->getUInt32("time"), cfg->get("sel")->get("task")->getCString("period"));  // 注册定时任务
		// 参数说明：
		// - mocker->id(): 模拟器ID（用于标识任务）
		// - getUInt32("date"): 任务日期（格式：YYYYMMDD）
		// - getUInt32("time"): 任务时间（格式：HHMM）
		// - getCString("period"): 任务周期（如"daily"、"weekly"等）
	}
	else if (strcmp(mode, "exec") == 0)  // 如果是执行器回测
	{
		ExecMocker* mocker = new ExecMocker(&replayer);  // 创建执行器模拟器实例
		// 参数说明：
		// - &replayer: 历史数据回放器指针
		mocker->init(cfg->get("exec"));  // 初始化执行器模拟器，传入执行器配置
		replayer.register_sink(mocker, "exec");  // 将执行器模拟器注册到历史数据回放器，使用固定ID"exec"
	}
	else if (strcmp(mode, "uft") == 0)  // 如果是UFT策略回测
	{
		UftMocker* mocker = new UftMocker(&replayer, "uft");  // 创建UFT策略模拟器实例
		// 参数说明：
		// - &replayer: 历史数据回放器指针
		// - "uft": 策略名称
		mocker->init_uft_factory(cfg->get("uft"));  // 初始化UFT策略工厂，加载UFT策略动态库
		const char* stra_id = cfg->get("uft")->get("strategy")->getCString("id");  // 获取策略ID
		replayer.register_sink(mocker, stra_id);  // 将UFT模拟器注册到历史数据回放器
	}

	// ========== 准备回测环境 ==========
	replayer.prepare();  // 准备回测环境，加载历史数据、初始化模拟器等
	// 准备流程：
	// 1. 加载历史数据（K线、Tick等）
	// 2. 初始化模拟器
	// 3. 设置回测时间范围
	// 4. 准备数据缓存

	// ========== 启动历史数据回放 ==========
	replayer.run(true);  // 启动历史数据回放，true表示异步模式（不阻塞主线程）
	// 回放流程：
	// 1. 按照时间顺序回放历史数据
	// 2. 将数据推送给注册的模拟器
	// 3. 模拟器根据数据执行策略逻辑
	// 4. 记录回测结果（成交记录、持仓记录、资金曲线等）

	printf("press enter key to exit\r\n");  // 提示用户按回车键退出
	getchar();  // 等待用户按键（阻塞主线程）

	WTSLogger::stop();  // 停止日志系统
}
