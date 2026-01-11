/*!
 * \file WtExecRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader执行器运行器类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtExecRunner类的所有方法，包括初始化、配置、运行、释放等功能。
 * 该类是WonderTrader执行器模块的核心，负责协调各组件协同工作。
 * 
 * 主要功能：
 * 1. 初始化实现：初始化日志系统和运行环境，启用崩溃转储功能
 * 2. 配置实现：从配置文件加载并初始化各组件（基础数据、数据管理器、开平策略、行情通道、交易通道、执行器）
 * 3. 运行实现：启动行情通道和交易通道的运行
 * 4. 组件初始化：实现各组件（交易通道、行情通道、执行器、数据管理器、开平策略）的初始化逻辑
 * 5. 行情处理：实现IParserStub接口，接收并处理实时行情数据
 * 6. 执行器存根：实现IExecuterStub接口，为执行器提供基础信息查询功能
 * 7. 仓位管理：实现目标仓位的设置和提交功能
 * 
 * 设计特点：
 * - 配置驱动：通过配置文件灵活配置各组件
 * - 组件化设计：各组件独立初始化，便于扩展和维护
 * - 异常处理：使用try-catch捕获异常，确保程序稳定运行
 * - 信号处理：安装信号钩子，捕获异常和错误
 * - 崩溃转储：在Windows平台启用MiniDump功能，便于问题排查
 */

#include "WtExecRunner.h"  // 包含当前类的头文件

#include "../WtCore/WtHelper.h"  // 包含WonderTrader辅助工具类，提供路径、时间等工具函数
#include "../WtCore/WtDiffExecuter.h"  // 包含差分执行器头文件，使用WtDiffExecuter类
#include "../WtCore/WtDistExecuter.h"  // 包含分布式执行器头文件，使用WtDistExecuter类

#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能
#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置加载器，提供配置文件加载功能

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息定义，提供WTSCommodityInfo等类型
#include "../Includes/WTSVariant.hpp"  // 包含配置变体类，提供WTSVariant类型
#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具，提供合约代码解析功能
#include "../Share/ModuleHelper.hpp"  // 包含模块辅助工具，提供动态库加载功能
#include "../Share/TimeUtils.hpp"  // 包含时间工具函数，提供时间转换功能
#include "../WTSUtils/SignalHook.hpp"  // 包含信号钩子工具，提供异常捕获功能

#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
#include "../Common/mdump.h"  // 包含MiniDump头文件，提供崩溃转储功能
#include <boost/filesystem.hpp>  // 包含Boost文件系统库，提供路径操作功能

/**
 * @brief 获取模块名称
 * 
 * 获取当前动态库的模块名称（文件名部分）。
 * 用于MiniDump崩溃转储文件的命名。
 * 
 * @return 返回模块名称字符串指针，如"WtExecMon.dll"
 * 
 * 实现说明：
 * - 使用静态变量缓存模块名称，避免重复获取
 * - 通过GetModuleFileName获取完整路径
 * - 使用Boost文件系统库提取文件名部分
 */
const char* getModuleName()
{
	static char MODULE_NAME[250] = { 0 };  // 静态缓冲区，缓存模块名称
	if (strlen(MODULE_NAME) == 0)  // 如果模块名称未初始化
	{
		GetModuleFileName(g_dllModule, MODULE_NAME, 250);  // 获取动态库的完整路径
		boost::filesystem::path p(MODULE_NAME);  // 创建路径对象
		strcpy(MODULE_NAME, p.filename().string().c_str());  // 提取文件名部分并复制到缓冲区
	}

	return MODULE_NAME;  // 返回模块名称
}
#endif

/**
 * @brief 构造函数实现
 * 
 * 创建执行器运行器实例，安装信号钩子用于捕获异常和错误。
 * 
 * 初始化流程：
 * 1. 安装信号钩子，捕获程序异常和错误
 * 2. 信号钩子会将错误信息记录到日志系统
 */
WtExecRunner::WtExecRunner()
{
	install_signal_hooks([](const char* message) {  // 安装信号钩子，使用lambda表达式作为回调函数
		WTSLogger::error(message);  // 将错误信息记录到日志系统
	});
}

/**
 * @brief 初始化执行器运行器
 * 
 * 初始化日志系统和运行环境。
 * 在Windows平台启用MiniDump崩溃转储功能。
 * 
 * @param logCfg 日志配置文件路径或配置内容，默认为"logcfgexec.json"
 * @param isFile 是否为文件路径，true表示logCfg是文件路径，false表示logCfg是配置内容
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 在Windows平台启用MiniDump崩溃转储功能
 * 2. 初始化日志系统（从文件或字符串加载配置）
 * 3. 设置安装目录路径
 */
bool WtExecRunner::init(const char* logCfg /* = "logcfgexec.json" */, bool isFile /* = true */)
{
#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
	CMiniDumper::Enable(getModuleName(), true, WtHelper::getCWD().c_str());  // 启用MiniDump崩溃转储功能
	// 参数说明：
	// - getModuleName(): 模块名称，用于转储文件命名
	// - true: 是否启用完整转储
	// - WtHelper::getCWD().c_str(): 转储文件保存目录（当前工作目录）
#endif

	if(isFile)  // 如果logCfg是文件路径
	{
		std::string path = WtHelper::getCWD() + logCfg;  // 拼接当前工作目录和配置文件路径
		WTSLogger::init(path.c_str(), true);  // 从文件初始化日志系统
	}
	else  // 如果logCfg是配置内容
	{
		WTSLogger::init(logCfg, false);  // 从字符串初始化日志系统
	}
	

	WtHelper::setInstDir(getBinDir());  // 设置安装目录路径，用于查找动态库和配置文件
	return true;  // 初始化成功，返回true
}

/**
 * @brief 配置执行器运行器
 * 
 * 从配置文件加载配置并初始化各组件。
 * 
 * @param cfgFile 配置文件路径或配置内容
 * @param isFile 是否为文件路径，true表示cfgFile是文件路径，false表示cfgFile是配置内容
 * @return 配置成功返回true，失败返回false
 * 
 * 配置流程：
 * 1. 加载主配置文件（从文件或字符串）
 * 2. 加载基础数据文件（交易时段、商品、合约、节假日等）
 * 3. 初始化数据管理器
 * 4. 初始化开平策略
 * 5. 初始化行情通道（解析器）
 * 6. 初始化交易通道（交易适配器）
 * 7. 初始化执行器
 */
bool WtExecRunner::config(const char* cfgFile, bool isFile /* = true */)
{
	_config = isFile ? WTSCfgLoader::load_from_file(cfgFile) : WTSCfgLoader::load_from_content(cfgFile, false);  // 加载配置文件（从文件或字符串）
	if(_config == NULL)  // 如果配置文件加载失败
	{
		WTSLogger::log_raw(LL_ERROR, "Loading config file failed");  // 记录错误日志
		return false;  // 返回false，表示配置失败
	}

	// ========== 加载基础数据文件 ==========
	WTSVariant* cfgBF = _config->get("basefiles");  // 获取基础数据文件配置节点
	if (cfgBF->get("session"))  // 如果配置了交易时段文件
	{
		_bd_mgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段数据
		WTSLogger::info("Trading sessions loaded");  // 记录日志
	}

	WTSVariant* cfgItem = cfgBF->get("commodity");  // 获取商品配置文件配置
	if (cfgItem)  // 如果配置了商品文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是单个文件路径（字符串类型）
		{
			_bd_mgr.loadCommodities(cfgItem->asCString());  // 加载商品数据
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是多个文件路径（数组类型）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组中的每个文件路径
			{
				_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());  // 加载每个商品文件
			}
		}
	}

	cfgItem = cfgBF->get("contract");  // 获取合约配置文件配置
	if (cfgItem)  // 如果配置了合约文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是单个文件路径（字符串类型）
		{
			_bd_mgr.loadContracts(cfgItem->asCString());  // 加载合约数据
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是多个文件路径（数组类型）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组中的每个文件路径
			{
				_bd_mgr.loadContracts(cfgItem->get(i)->asCString());  // 加载每个合约文件
			}
		}
	}

	if (cfgBF->get("holiday"))  // 如果配置了节假日文件
	{
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));  // 加载节假日数据
		WTSLogger::info("Holidays loaded");  // 记录日志
	}


	// ========== 初始化数据管理器 ==========
	initDataMgr();  // 初始化数据管理器，加载数据存储模块

	// ========== 初始化开平策略 ==========
	if (!initActionPolicy())  // 如果开平策略初始化失败
		return false;  // 返回false，表示配置失败

	// ========== 初始化行情通道（解析器） ==========
	const char* cfgParser = _config->getCString("parsers");  // 获取行情通道配置文件路径
	if (StdFile::exists(cfgParser))  // 如果配置文件存在
	{
		WTSLogger::info("Reading parser config from {}...", cfgParser);  // 记录日志
		WTSVariant* var = WTSCfgLoader::load_from_file(cfgParser);  // 加载行情通道配置文件
		if (var)  // 如果配置文件加载成功
		{
			if (!initParsers(var))  // 如果解析器初始化失败
				WTSLogger::error("Loading parsers failed");  // 记录错误日志
			var->release();  // 释放配置对象
		}
		else  // 如果配置文件加载失败
		{
			WTSLogger::error("Loading parser config {} failed", cfgParser);  // 记录错误日志
		}
	}

	// ========== 初始化交易通道（交易适配器） ==========
	const char* cfgTraders = _config->getCString("traders");  // 获取交易通道配置文件路径
	if (StdFile::exists(cfgTraders))  // 如果配置文件存在
	{
		WTSLogger::info("Reading trader config from {}...", cfgTraders);  // 记录日志
		WTSVariant* var = WTSCfgLoader::load_from_file(cfgTraders);  // 加载交易通道配置文件
		if (var)  // 如果配置文件加载成功
		{
			if (!initTraders(var))  // 如果交易适配器初始化失败
				WTSLogger::error("Loading traders failed");  // 记录错误日志
			var->release();  // 释放配置对象
		}
		else  // 如果配置文件加载失败
		{
			WTSLogger::error("Loading trader config {} failed", cfgTraders);  // 记录错误日志
		}
	}

	// ========== 初始化执行器 ==========
	const char* cfgExecuters = _config->getCString("executers");  // 获取执行器配置文件路径
	if (StdFile::exists(cfgExecuters))  // 如果配置文件存在
	{
		WTSLogger::info("Reading executer config from {}...", cfgExecuters);  // 记录日志
		WTSVariant* var = WTSCfgLoader::load_from_file(cfgExecuters);  // 加载执行器配置文件
		if (var)  // 如果配置文件加载成功
		{
			if (!initExecuters(var))  // 如果执行器初始化失败
				WTSLogger::error("Loading executers failed");  // 记录错误日志
			var->release();  // 释放配置对象
		}
		else  // 如果配置文件加载失败
		{
			WTSLogger::error("Loading executer config {} failed", cfgExecuters);  // 记录错误日志
		}
	}

	return true;  // 配置成功，返回true
}


/**
 * @brief 运行执行器运行器
 * 
 * 启动行情通道和交易通道的运行。
 * 该函数会阻塞当前线程，直到模块停止运行。
 * 
 * 运行流程：
 * 1. 启动解析器适配器管理器（行情通道），接收实时行情数据
 * 2. 启动交易适配器管理器（交易通道），处理交易指令
 * 3. 执行器根据行情数据和目标仓位执行交易逻辑
 * 
 * 异常处理：
 * - 使用try-catch捕获所有异常
 * - 捕获异常后打印堆栈跟踪信息到日志
 * - 确保程序不会因异常而崩溃
 */
void WtExecRunner::run()
{
	try  // 捕获异常
	{
		_parsers.run();  // 启动解析器适配器管理器（行情通道）
		_traders.run();  // 启动交易适配器管理器（交易通道）
	}
	catch (...)  // 捕获所有异常
	{
		print_stack_trace([](const char* message) {  // 打印堆栈跟踪信息，使用lambda表达式作为回调函数
			WTSLogger::error(message);  // 将堆栈跟踪信息记录到日志
		});
	}
}

/**
 * @brief 初始化行情通道（解析器）
 * 
 * 从配置加载并初始化解析器适配器。
 * 
 * @param cfgParser 行情通道配置对象
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取解析器配置数组
 * 2. 遍历配置数组中的每个解析器配置
 * 3. 检查解析器是否启用（active字段）
 * 4. 获取解析器ID，如果为空则自动生成
 * 5. 创建解析器适配器实例并初始化
 * 6. 将解析器适配器添加到管理器
 */
bool WtExecRunner::initParsers(WTSVariant* cfgParser)
{
	WTSVariant* cfg = cfgParser->get("parsers");  // 获取解析器配置数组
	if (cfg == NULL)  // 如果配置数组不存在
		return false;  // 返回false，表示初始化失败

	uint32_t count = 0;  // 初始化计数器，记录成功加载的解析器数量
	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历配置数组中的每个解析器配置
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前解析器配置项
		if (!cfgItem->getBoolean("active"))  // 如果解析器未启用（active字段为false）
			continue;  // 跳过该解析器，继续处理下一个

		const char* id = cfgItem->getCString("id");  // 获取解析器ID

		// By Wesley @ 2021.12.14
		// 如果id为空，则生成自动id
		std::string realid = id;  // 复制解析器ID
		if (realid.empty())  // 如果ID为空
		{
			static uint32_t auto_parserid = 1000;  // 静态自动ID计数器，从1000开始
			realid = StrUtil::printf("auto_parser_%u", auto_parserid++);  // 生成自动ID，格式：auto_parser_1000、auto_parser_1001等
		}

		ParserAdapterPtr adapter(new ParserAdapter);  // 创建解析器适配器智能指针
		adapter->init(realid.c_str(), cfgItem, this, &_bd_mgr, &_hot_mgr);  // 初始化解析器适配器
		// 参数说明：
		// - realid.c_str(): 解析器ID
		// - cfgItem: 解析器配置项
		// - this: 解析器存根接口指针（用于接收行情数据）
		// - &_bd_mgr: 基础数据管理器指针
		// - &_hot_mgr: 热点合约管理器指针
		_parsers.addAdapter(realid.c_str(), adapter);  // 将解析器适配器添加到管理器

		count++;  // 增加成功加载的解析器计数
	}

	WTSLogger::info("{} parsers loaded", count);  // 记录日志，输出成功加载的解析器数量

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化执行器
 * 
 * 从配置加载并初始化执行器（本地执行器、差分执行器、分布式执行器等）。
 * 
 * @param cfgExecuter 执行器配置对象
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取执行器配置数组
 * 2. 先加载自带的执行器工厂（从executer目录）
 * 3. 遍历配置数组中的每个执行器配置
 * 4. 检查执行器是否启用（active字段）
 * 5. 根据执行器类型（local/diff/dist）创建对应的执行器实例
 * 6. 初始化执行器并配置交易通道
 * 7. 设置执行器存根接口并添加到管理器
 * 
 * 执行器类型说明：
 * - local: 本地执行器，直接连接交易通道执行交易
 * - diff: 差分执行器，支持多账户差分执行
 * - dist: 分布式执行器，支持分布式执行
 */
bool WtExecRunner::initExecuters(WTSVariant* cfgExecuter)
{
	WTSVariant* cfg = cfgExecuter->get("executers");  // 获取执行器配置数组
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置数组不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	//先加载自带的执行器工厂
	std::string path = WtHelper::getInstDir() + "executer//";  // 拼接执行器工厂目录路径
	_exe_factory.loadFactories(path.c_str());  // 从指定目录加载执行器工厂动态库

	uint32_t count = 0;  // 初始化计数器，记录成功加载的执行器数量
	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历配置数组中的每个执行器配置
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前执行器配置项
		if (!cfgItem->getBoolean("active"))  // 如果执行器未启用（active字段为false）
			continue;  // 跳过该执行器，继续处理下一个

		const char* id = cfgItem->getCString("id");  // 获取执行器ID
		std::string name = cfgItem->getCString("name");	//local,diff,dist  // 获取执行器类型名称
		if (name.empty())  // 如果类型名称为空
			name = "local";  // 默认使用本地执行器

		if (name == "local")  // 如果是本地执行器
		{
			WtLocalExecuter* executer = new WtLocalExecuter(&_exe_factory, id, &_data_mgr);  // 创建本地执行器实例
			if (!executer->init(cfgItem))  // 如果执行器初始化失败
				return false;  // 返回false，表示初始化失败

			const char* tid = cfgItem->getCString("trader");  // 获取交易通道ID
			if (strlen(tid) == 0)  // 如果交易通道ID为空
			{
				WTSLogger::error("No Trader configured for Executer {}", id);  // 记录错误日志
			}
			else  // 如果交易通道ID不为空
			{
				TraderAdapterPtr trader = _traders.getAdapter(tid);  // 从交易适配器管理器中获取交易适配器
				if (trader)  // 如果交易适配器存在
				{
					executer->setTrader(trader.get());  // 设置执行器的交易适配器
					trader->addSink(executer);  // 将执行器添加到交易适配器的接收者列表
				}
				else  // 如果交易适配器不存在
				{
					WTSLogger::error("Trader {} not exists, cannot configured for executer %s", tid, id);  // 记录错误日志
				}
			}

			executer->setStub(this);  // 设置执行器存根接口（用于获取基础信息）
			_exe_mgr.add_executer(ExecCmdPtr(executer));  // 将执行器添加到执行器管理器
		}
		else if (name == "diff")  // 如果是差分执行器
		{
			WtDiffExecuter* executer = new WtDiffExecuter(&_exe_factory, id, &_data_mgr, &_bd_mgr);  // 创建差分执行器实例
			if (!executer->init(cfgItem))  // 如果执行器初始化失败
				return false;  // 返回false，表示初始化失败

			const char* tid = cfgItem->getCString("trader");  // 获取交易通道ID
			if (strlen(tid) == 0)  // 如果交易通道ID为空
			{
				WTSLogger::error("No Trader configured for Executer {}", id);  // 记录错误日志
			}
			else  // 如果交易通道ID不为空
			{
				TraderAdapterPtr trader = _traders.getAdapter(tid);  // 从交易适配器管理器中获取交易适配器
				if (trader)  // 如果交易适配器存在
				{
					executer->setTrader(trader.get());  // 设置执行器的交易适配器
					trader->addSink(executer);  // 将执行器添加到交易适配器的接收者列表
				}
				else  // 如果交易适配器不存在
				{
					WTSLogger::error("Trader {} not exists, cannot configured for executer %s", tid, id);  // 记录错误日志
				}
			}

			executer->setStub(this);  // 设置执行器存根接口（用于获取基础信息）
			_exe_mgr.add_executer(ExecCmdPtr(executer));  // 将执行器添加到执行器管理器
		}
		else  // 如果是分布式执行器或其他类型
		{
			WtDistExecuter* executer = new WtDistExecuter(id);  // 创建分布式执行器实例
			if (!executer->init(cfgItem))  // 如果执行器初始化失败
				return false;  // 返回false，表示初始化失败

			executer->setStub(this);  // 设置执行器存根接口（用于获取基础信息）
			_exe_mgr.add_executer(ExecCmdPtr(executer));  // 将执行器添加到执行器管理器
		}
		count++;  // 增加成功加载的执行器计数
	}

	WTSLogger::info("{} executers loaded", count);  // 记录日志，输出成功加载的执行器数量

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化交易通道（交易适配器）
 * 
 * 从配置加载并初始化交易适配器。
 * 
 * @param cfgTrader 交易通道配置对象
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取交易适配器配置数组
 * 2. 遍历配置数组中的每个交易适配器配置
 * 3. 检查交易适配器是否启用（active字段）
 * 4. 创建交易适配器实例并初始化
 * 5. 将交易适配器添加到管理器
 */
bool WtExecRunner::initTraders(WTSVariant* cfgTrader)
{
	WTSVariant* cfg = cfgTrader->get("traders");  // 获取交易适配器配置数组
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置数组不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	uint32_t count = 0;  // 初始化计数器，记录成功加载的交易适配器数量
	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历配置数组中的每个交易适配器配置
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前交易适配器配置项
		if (!cfgItem->getBoolean("active"))  // 如果交易适配器未启用（active字段为false）
			continue;  // 跳过该交易适配器，继续处理下一个

		const char* id = cfgItem->getCString("id");  // 获取交易适配器ID

		TraderAdapterPtr adapter(new TraderAdapter);  // 创建交易适配器智能指针
		adapter->init(id, cfgItem, &_bd_mgr, &_act_policy);  // 初始化交易适配器
		// 参数说明：
		// - id: 交易适配器ID
		// - cfgItem: 交易适配器配置项
		// - &_bd_mgr: 基础数据管理器指针
		// - &_act_policy: 开平策略管理器指针

		_traders.addAdapter(id, adapter);  // 将交易适配器添加到管理器
		count++;  // 增加成功加载的交易适配器计数
	}

	WTSLogger::info("{} traders loaded", count);  // 记录日志，输出成功加载的交易适配器数量

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化数据管理器
 * 
 * 从配置初始化数据管理器，加载数据存储模块。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取数据管理器配置
 * 2. 调用数据管理器的init方法初始化
 * 3. 数据管理器会加载数据存储模块（如WtDataStorage）
 */
bool WtExecRunner::initDataMgr()
{
	WTSVariant* cfg = _config->get("data");  // 获取数据管理器配置
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	_data_mgr.init(cfg, this);  // 初始化数据管理器
	// 参数说明：
	// - cfg: 数据管理器配置
	// - this: 执行器运行器指针（作为数据读取器接收者）

	WTSLogger::info("Data Manager initialized");  // 记录日志
	return true;  // 初始化成功，返回true
}

/**
 * @brief 添加执行器工厂目录
 * 
 * 从指定目录加载执行器工厂动态库。
 * 
 * @param folder 执行器工厂目录路径
 * @return 加载成功返回true，失败返回false
 * 
 * 使用场景：
 * - 加载自定义执行器工厂
 * - 扩展执行器功能
 */
bool WtExecRunner::addExeFactories(const char* folder)
{
	return _exe_factory.loadFactories(folder);  // 从指定目录加载执行器工厂动态库
}

/**
 * @brief 获取交易会话信息
 * 
 * 根据会话ID或合约代码获取交易会话信息。
 * 
 * @param sid 会话ID或合约代码
 * @param isCode 是否为合约代码，true表示sid是合约代码，false表示sid是会话ID
 * @return 返回交易会话信息指针，未找到返回NULL
 * 
 * 查询流程：
 * 1. 如果isCode为false，直接从基础数据管理器查询会话信息
 * 2. 如果isCode为true，从合约代码中提取交易所和品种代码
 * 3. 查询商品信息，然后获取交易会话信息
 */
WTSSessionInfo* WtExecRunner::get_session_info(const char* sid, bool isCode /* = true */)
{
	if (!isCode)  // 如果sid不是合约代码，而是会话ID
		return _bd_mgr.getSession(sid);  // 直接从基础数据管理器查询会话信息

	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(sid, NULL);  // 从标准合约代码中提取交易所和品种代码
	WTSCommodityInfo* cInfo = _bd_mgr.getCommodity(codeInfo._exchg, codeInfo._product);  // 查询商品信息
	if (cInfo == NULL)  // 如果商品信息不存在
		return NULL;  // 返回NULL

	return cInfo->getSessionInfo();  // 返回商品信息中的交易会话信息
}

/**
 * @brief 处理实时主推行情（IParserStub接口实现）
 * 
 * 当解析器收到新的行情数据时，通过此方法推送给执行器运行器。
 * 执行器运行器会更新时间和数据管理器，然后将行情数据转发给执行器管理器。
 * 
 * @param quote 最新的tick数据指针
 * 
 * 处理流程：
 * 1. 检查行情数据指针是否有效
 * 2. 从行情数据中提取日期和时间信息
 * 3. 更新全局时间（日期、分钟、秒）
 * 4. 更新交易日
 * 5. 更新数据管理器的行情数据
 * 6. 将行情数据转发给执行器管理器处理
 */
void WtExecRunner::handle_push_quote(WTSTickData* quote)
{
	if (quote == NULL)  // 如果行情数据指针为空
		return;  // 直接返回，不做处理

	uint32_t uDate = quote->actiondate();  // 获取行情数据的动作日期（格式：YYYYMMDD）
	uint32_t uTime = quote->actiontime();  // 获取行情数据的动作时间（格式：HHMMSSmmm，毫秒级）
	uint32_t curMin = uTime / 100000;  // 提取分钟部分（HHMM格式）
	uint32_t curSec = uTime % 100000;  // 提取秒和毫秒部分（SSmmm格式）
	WtHelper::setTime(uDate, curMin, curSec);  // 更新全局时间（日期、分钟、秒）
	WtHelper::setTDate(quote->tradingdate());  // 更新全局交易日（格式：YYYYMMDD）

	_data_mgr.handle_push_quote(quote->code(), quote);  // 更新数据管理器的行情数据

	_exe_mgr.handle_tick(quote->code(), quote);  // 将行情数据转发给执行器管理器处理
}

/**
 * @brief 释放执行器运行器资源
 * 
 * 清理执行器运行器占用的资源，停止日志系统。
 * 调用此函数后，模块将无法继续使用，需要重新初始化。
 */
void WtExecRunner::release()
{
	WTSLogger::stop();  // 停止日志系统
}


/**
 * @brief 设置目标仓位
 * 
 * 设置指定合约的目标持仓数量。
 * 目标仓位会被缓存，直到调用commitPositions()提交执行。
 * 
 * @param stdCode 标准合约代码，如"SHFE.rb2305"、"CFFEX.IF2303"等
 * @param targetPos 目标持仓数量，正数表示多头，负数表示空头，0表示平仓
 */
void WtExecRunner::setPosition(const char* stdCode, double targetPos)
{
	_positions[stdCode] = targetPos;  // 将目标仓位存储到映射表中
}

/**
 * @brief 提交目标仓位
 * 
 * 将所有已设置的目标仓位提交给执行器执行。
 * 执行器会根据当前持仓和目标持仓的差异，生成相应的交易指令。
 * 
 * 执行流程：
 * 1. 将目标仓位传递给执行器管理器
 * 2. 执行器管理器根据目标仓位生成交易指令
 * 3. 清空目标仓位缓存
 */
void WtExecRunner::commitPositions()
{
	_exe_mgr.set_positions(_positions);  // 将目标仓位传递给执行器管理器
	_positions.clear();  // 清空目标仓位缓存
}

/**
 * @brief 初始化开平策略
 * 
 * 从配置文件加载开平策略。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 从配置中获取开平策略配置文件路径（bspolicy字段）
 * 2. 调用开平策略管理器的init方法加载策略文件
 */
bool WtExecRunner::initActionPolicy()
{
	const char* action_file = _config->getCString("bspolicy");  // 获取开平策略配置文件路径
	if (strlen(action_file) <= 0)  // 如果配置文件路径为空
		return false;  // 返回false，表示初始化失败

	bool ret = _act_policy.init(action_file);  // 初始化开平策略管理器，加载策略文件
	WTSLogger::info("Action policies initialized");  // 记录日志
	return ret;  // 返回初始化结果
}

/**
 * @brief 获取实时时间（IExecuterStub接口实现）
 * 
 * 返回当前实时时间戳（纳秒级）。
 * 
 * @return 返回当前实时时间戳（纳秒级）
 * 
 * 时间计算：
 * - 从数据管理器获取当前日期和原始时间（分钟格式）
 * - 将分钟时间转换为纳秒级时间戳
 * - 组合日期、分钟、秒生成完整时间戳
 */
uint64_t WtExecRunner::get_real_time()
{
	return TimeUtils::makeTime(_data_mgr.get_date(), _data_mgr.get_raw_time() * 100000 + _data_mgr.get_secs());  // 组合日期和时间生成时间戳
	// 参数说明：
	// - _data_mgr.get_date(): 当前日期（格式：YYYYMMDD）
	// - _data_mgr.get_raw_time() * 100000 + _data_mgr.get_secs(): 当前时间（纳秒级）
	//   - _data_mgr.get_raw_time(): 原始分钟时间（格式：HHMM）
	//   - * 100000: 转换为纳秒级（分钟部分）
	//   - + _data_mgr.get_secs(): 加上秒和毫秒部分
}

/**
 * @brief 获取商品信息（IExecuterStub接口实现）
 * 
 * 根据标准合约代码获取对应的商品信息。
 * 
 * @param stdCode 标准合约代码
 * @return 返回商品信息对象指针，未找到返回NULL
 * 
 * 查询流程：
 * 1. 从标准合约代码中提取交易所和品种代码
 * 2. 从基础数据管理器中查询商品信息
 */
WTSCommodityInfo* WtExecRunner::get_comm_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 从标准合约代码中提取交易所和品种代码
	return _bd_mgr.getCommodity(codeInfo._exchg, codeInfo._product);  // 从基础数据管理器查询商品信息
}

/**
 * @brief 获取交易会话信息（IExecuterStub接口实现）
 * 
 * 根据标准合约代码获取对应的交易会话信息。
 * 
 * @param stdCode 标准合约代码
 * @return 返回交易会话信息对象指针，未找到返回NULL
 * 
 * 查询流程：
 * 1. 从标准合约代码中提取交易所和品种代码
 * 2. 从基础数据管理器中查询商品信息
 * 3. 从商品信息中获取交易会话信息
 */
WTSSessionInfo* WtExecRunner::get_sess_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 从标准合约代码中提取交易所和品种代码
	WTSCommodityInfo* cInfo = _bd_mgr.getCommodity(codeInfo._exchg, codeInfo._product);  // 查询商品信息
	if (cInfo == NULL)  // 如果商品信息不存在
		return NULL;  // 返回NULL

	return cInfo->getSessionInfo();  // 返回商品信息中的交易会话信息
}

/**
 * @brief 获取交易日期（IExecuterStub接口实现）
 * 
 * 返回当前交易日期。
 * 
 * @return 返回当前交易日期（格式：YYYYMMDD）
 */
uint32_t WtExecRunner::get_trading_day()
{
	return _data_mgr.get_trading_day();  // 从数据管理器获取当前交易日
}