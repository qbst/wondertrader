/*!
 * \file main.cpp
 * \project WonderTrader
 * 
 * \brief QuoteFactory主程序入口 - WonderTrader数据落地工厂程序
 * 
 * 文件设计逻辑与作用总结：
 * QuoteFactory是WonderTrader框架中的数据落地工厂程序，负责接收来自各种行情源的数据，
 * 进行解析、处理、存储和分发。该程序是WonderTrader数据流处理的核心组件，实现了完整的
 * 行情数据落地解决方案。
 * 
 * 核心设计理念：
 * 
 * 1. 数据流处理架构（Data Flow Processing Architecture）：
 *    行情源 → Parser解析器 → ParserAdapter适配器 → DataManager数据管理器 → 
 *    存储模块(Writer) + 广播模块(Caster) → 下游系统
 * 
 * 2. 模块化设计（Modular Design）：
 *    - 解析器模块：支持多种行情源（CTP、XTP、飞马等）
 *    - 数据管理模块：统一的数据接收、验证、存储和分发
 *    - 状态监控模块：交易时段状态管理和数据质量控制
 *    - 广播模块：UDP广播和共享内存广播
 *    - 指数计算模块：实时指数计算和推送
 * 
 * 3. 配置驱动架构（Configuration-Driven Architecture）：
 *    - 通过YAML配置文件定义所有组件参数
 *    - 支持动态加载和配置解析器
 *    - 支持全天候模式和交易时段模式切换
 * 
 * 程序执行流程：
 * 
 * 1. 初始化阶段（Initialization Phase）：
 *    - 解析命令行参数（配置文件路径、日志配置等）
 *    - 初始化日志系统
 *    - 设置异常处理和信号处理
 *    - 加载配置文件
 * 
 * 2. 组件初始化（Component Initialization）：
 *    - 加载基础数据（交易时段、合约信息、节假日等）
 *    - 初始化数据管理器（DataManager）
 *    - 初始化状态监控器（StateMonitor）
 *    - 初始化广播器（UDPCaster、ShmCaster）
 *    - 初始化指数工厂（IndexFactory）
 *    - 加载和初始化解析器（Parser）
 * 
 * 3. 运行阶段（Runtime Phase）：
 *    - 启动所有解析器线程
 *    - 启动状态监控器（非全天候模式）
 *    - 进入主循环，等待退出信号
 * 
 * 4. 清理阶段（Cleanup Phase）：
 *    - 接收退出信号
 *    - 释放所有资源
 *    - 正常退出程序
 * 
 * 主要功能模块：
 * 
 * 1. 数据接收模块：
 *    - 支持多种行情源接入（CTP、XTP、飞马、易达等）
 *    - 实时行情数据解析和标准化
 *    - 数据质量验证和异常处理
 * 
 * 2. 数据存储模块：
 *    - 支持多种存储格式（WT自有格式、CSV等）
 *    - 实时数据写入和持久化
 *    - 历史数据管理和归档
 * 
 * 3. 数据分发模块：
 *    - UDP广播：向多个客户端广播行情数据
 *    - 共享内存：高性能本地数据共享
 *    - 指数计算：实时指数计算和推送
 * 
 * 4. 状态管理模块：
 *    - 交易时段状态监控
 *    - 数据完整性检查
 *    - 异常状态处理和恢复
 * 
 * 配置系统：
 * - 主配置文件：dtcfg.yaml（数据落地配置）
 * - 日志配置文件：logcfgdt.yaml（日志系统配置）
 * - 解析器配置：支持独立配置文件或内嵌配置
 * - 指数配置：支持独立指数配置文件
 * 
 * 运行模式：
 * - 标准模式：基于交易时段的状态管理
 * - 全天候模式：24小时连续运行，无状态管理
 * 
 * 技术特点：
 * - 多线程架构：每个解析器独立线程
 * - 异步处理：非阻塞数据流处理
 * - 容错机制：异常恢复和错误处理
 * - 高性能：优化的内存管理和数据处理
 * 
 * 使用场景：
 * - 期货行情数据落地
 * - 股票行情数据落地
 * - 期权行情数据落地
 * - 多源行情数据整合
 * - 实时数据分发服务
 * 
 * 注意事项：
 * - 需要正确配置各种行情源连接参数
 * - 确保存储路径有足够空间
 * - 监控系统资源使用情况
 * - 定期检查数据完整性
 */

// ===== 核心数据组件头文件 =====
#include "../WtDtCore/ParserAdapter.h"                          // 解析器适配器（统一解析器接口）
#include "../WtDtCore/DataManager.h"                             // 数据管理器（数据流处理中枢）
#include "../WtDtCore/StateMonitor.h"                           // 状态监控器（交易时段状态管理）
#include "../WtDtCore/UDPCaster.h"                              // UDP广播器（网络数据分发）
#include "../WtDtCore/ShmCaster.h"                               // 共享内存广播器（本地高性能数据共享）
#include "../WtDtCore/WtHelper.h"                                // WonderTrader辅助工具（路径管理等）
#include "../WtDtCore/IndexFactory.h"                            // 指数工厂（实时指数计算）

// ===== 基础数据结构头文件 =====
#include "../Includes/WTSSessionInfo.hpp"                       // 交易时段信息结构体
#include "../Includes/WTSVariant.hpp"                           // 变体数据类型（配置解析用）

// ===== 工具库头文件 =====
#include "../WTSTools/WTSHotMgr.h"                               // 主力合约管理器（主力切换规则）
#include "../WTSTools/WTSBaseDataMgr.h"                          // 基础数据管理器（合约、时段等基础信息）
#include "../WTSTools/WTSLogger.h"                              // 日志系统（统一日志管理）
#include "../WTSUtils/WTSCfgLoader.h"                           // 配置文件加载器（YAML配置解析）
#include "../Share/StrUtil.hpp"                                 // 字符串工具（字符串处理函数）
#include "../Share/cppcli.hpp"                                  // 命令行解析器（命令行参数处理）

// ===== 系统工具头文件 =====
#include "../WTSUtils/SignalHook.hpp"                            // 信号处理钩子（进程信号处理）

// ===== 全局组件实例定义 =====
// 这些全局对象在整个程序生命周期中保持存在，为各个模块提供统一的数据访问接口

WTSBaseDataMgr	g_baseDataMgr;                                  // 基础数据管理器全局实例（管理合约信息、交易时段、节假日等基础数据）
WTSHotMgr		g_hotMgr;                                       // 主力合约管理器全局实例（管理主力合约切换规则和热力值计算）
StateMonitor	g_stateMon;                                     // 状态监控器全局实例（监控交易时段状态和数据完整性）
UDPCaster		g_udpCaster;                                   // UDP广播器全局实例（向网络客户端广播行情数据）
ShmCaster		g_shmCaster;                                    // 共享内存广播器全局实例（通过共享内存向本地进程广播数据）
DataManager		g_dataMgr;                                      // 数据管理器全局实例（数据流处理中枢，协调存储和广播）
ParserAdapterMgr g_parsers;                                     // 解析器适配器管理器全局实例（管理所有行情解析器）
IndexFactory	g_idxFactory;                                   // 指数工厂全局实例（实时计算和推送各种指数）

// ===== Windows平台特定代码 =====
#ifdef _MSC_VER                                                      // 仅在Microsoft Visual C++编译器下编译
#include "../Common/mdump.h"                                       // 包含MiniDump崩溃转储头文件（Windows崩溃诊断）

DWORD g_dwMainThreadId = 0;                                        // 主线程ID全局变量（用于Windows消息处理）

/*!
 * \brief Windows控制台控制处理器（Console Control Handler）
 * \param dwCtrlType 控制事件类型（CTRL_CLOSE_EVENT、CTRL_C_EVENT等）
 * \return TRUE表示已处理该事件，FALSE表示传递给下一个处理器
 * 
 * 该函数处理Windows控制台程序的控制事件，特别是窗口关闭事件。
 * 当用户点击控制台窗口的关闭按钮时，系统会调用此函数。
 * 
 * 处理流程：
 * 1. 检查事件类型是否为CTRL_CLOSE_EVENT（窗口关闭事件）
 * 2. 如果是关闭事件：
 *    a. 调用g_dataMgr.release()释放数据管理器资源
 *    b. 向主线程发送WM_QUIT消息，触发程序正常退出
 * 3. 返回TRUE表示已处理该事件
 * 
 * 注意：此函数在系统线程中执行，不应进行复杂操作
 */
BOOL WINAPI ConsoleCtrlhandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)                                             // 根据控制事件类型进行不同处理
	{
	case CTRL_CLOSE_EVENT:                                         // 控制台窗口关闭事件
	{
		g_dataMgr.release();                                       // 释放数据管理器资源（确保数据完整性）

		PostThreadMessage(g_dwMainThreadId, WM_QUIT, 0, 0);       // 向主线程发送退出消息（触发程序正常退出）
	}
	break;                                                          // 其他事件类型暂不处理
	}

	return TRUE;                                                    // 返回TRUE表示已处理该控制事件
}
#endif                                                              // _MSC_VER条件编译结束

/*!
 * \brief 获取程序所在目录路径
 * \return 程序所在目录的字符串指针（静态存储，程序生命周期内有效）
 * 
 * 该函数获取QuoteFactory程序的可执行文件所在目录路径，用于：
 * - 设置模块加载路径
 * - 定位配置文件
 * - 设置日志文件路径
 * - 其他需要相对路径的操作
 * 
 * 实现特点：
 * - 使用静态变量缓存路径，避免重复计算
 * - 使用boost::filesystem获取当前工作目录
 * - 使用StrUtil::standardisePath标准化路径格式（统一路径分隔符）
 * 
 * 注意：返回的指针指向静态字符串，在程序生命周期内有效
 */
const char* getBinDir()
{
	static std::string basePath;                                   // 静态字符串变量（缓存程序目录路径）
	if (basePath.empty())                                          // 如果路径为空（首次调用）
	{
		// 获取当前工作目录（程序启动时的目录）
		basePath = boost::filesystem::initial_path<boost::filesystem::path>().string();

		// 标准化路径格式（统一使用正斜杠或反斜杠，根据操作系统）
		basePath = StrUtil::standardisePath(basePath);
	}

	return basePath.c_str();                                        // 返回C风格字符串指针
}


/*!
 * \brief 初始化数据管理器（DataManager）
 * \param config 数据管理器配置对象（包含存储模块、路径等配置信息）
 * \param bAlldayMode 是否全天候模式（true=全天候模式，false=标准模式）
 * 
 * 该函数初始化数据管理器，根据运行模式决定是否启用状态监控器：
 * - 标准模式：启用状态监控器，基于交易时段进行数据质量控制
 * - 全天候模式：禁用状态监控器，24小时连续接收和处理数据
 * 
 * 参数说明：
 * - config：包含数据存储模块配置、存储路径、数据格式等参数
 * - bAlldayMode：运行模式标志，影响状态监控器的使用
 * 
 * 注意：此函数应在基础数据加载完成后调用
 */
void initDataMgr(WTSVariant* config, bool bAlldayMode = false)
{
	// 原注释：如果是全天模式，则不传递状态机给DataManager
	// 根据运行模式决定是否传递状态监控器给数据管理器
	g_dataMgr.init(config, &g_baseDataMgr, bAlldayMode ? NULL : &g_stateMon);
}

/*!
 * \brief 初始化行情解析器（Parser）
 * \param cfg 解析器配置数组（包含所有解析器的配置信息）
 * 
 * 该函数遍历配置数组，为每个启用的解析器创建ParserAdapter实例。
 * 支持多种行情源解析器：CTP、XTP、飞马、易达、华鑫等。
 * 
 * 处理流程：
 * 1. 遍历配置数组中的每个解析器配置项
 * 2. 检查解析器是否启用（active字段）
 * 3. 获取解析器ID，如果为空则自动生成
 * 4. 创建ParserAdapter实例并初始化
 * 5. 将解析器添加到管理器中
 * 
 * 自动ID生成规则：
 * - 如果配置中id字段为空，自动生成ID
 * - 格式：auto_parser_XXXX（从1000开始递增）
 * - 确保每个解析器都有唯一标识
 * 
 * 注意：解析器初始化后不会立即启动，需要调用run()方法启动
 */
void initParsers(WTSVariant* cfg)
{
	for (uint32_t idx = 0; idx < cfg->size(); idx++)               // 遍历配置数组中的每个解析器配置项
	{
		WTSVariant* cfgItem = cfg->get(idx);                        // 获取当前解析器的配置项
		if (!cfgItem->getBoolean("active"))                        // 如果解析器未启用（active字段为false）
			continue;                                               // 跳过此解析器，继续处理下一个

		const char* id = cfgItem->getCString("id");                // 获取解析器ID
		// 原注释：By Wesley @ 2021.12.14
		// 原注释：如果id为空，则生成自动id
		std::string realid = id;                                     // 将ID转换为字符串
		if (realid.empty())                                         // 如果ID为空字符串
		{
			static uint32_t auto_parserid = 1000;                  // 自动ID计数器（从1000开始）
			realid = StrUtil::printf("auto_parser_%u", auto_parserid++);  // 生成自动ID：auto_parser_1000, auto_parser_1001...
		}

		// 创建解析器适配器实例（传入基础数据管理器、数据管理器、指数工厂）
		ParserAdapterPtr adapter(new ParserAdapter(&g_baseDataMgr, &g_dataMgr, &g_idxFactory));
		adapter->init(realid.c_str(), cfgItem);                     // 初始化解析器适配器
		g_parsers.addAdapter(realid.c_str(), adapter);              // 将解析器添加到管理器中
	}

	WTSLogger::info("{} market data parsers loaded in total", g_parsers.size());  // 记录加载的解析器总数
}

/*!
 * \brief 初始化QuoteFactory程序（核心初始化函数）
 * \param filename 配置文件路径（通常是dtcfg.yaml）
 * 
 * 该函数是QuoteFactory程序的核心初始化函数，负责：
 * 1. 加载和解析配置文件
 * 2. 初始化基础数据（交易时段、合约信息、节假日等）
 * 3. 初始化各种组件（数据管理器、广播器、状态监控器等）
 * 4. 加载和启动解析器
 * 5. 启动状态监控器（非全天候模式）
 * 
 * 初始化顺序：
 * 1. 设置模块目录路径
 * 2. 加载主配置文件
 * 3. 加载基础数据文件（session、commodity、contract、holiday等）
 * 4. 加载热力规则和自定义规则
 * 5. 初始化广播器（UDP、共享内存）
 * 6. 初始化状态监控器（非全天候模式）
 * 7. 初始化数据管理器
 * 8. 初始化指数工厂
 * 9. 加载和初始化解析器
 * 10. 启动所有组件
 * 
 * 配置结构：
 * - basefiles：基础数据文件配置
 * - broadcaster：UDP广播器配置
 * - shmcaster：共享内存广播器配置
 * - statemonitor：状态监控器配置
 * - writer：数据存储配置
 * - index：指数计算配置
 * - parsers：解析器配置
 * - allday：全天候模式标志
 * 
 * 注意：此函数执行失败会导致程序无法正常工作
 */
void initialize(const std::string& filename)
{
	WtHelper::set_module_dir(getBinDir());                         // 设置WonderTrader模块目录路径（用于动态库加载）

	WTSVariant* config = WTSCfgLoader::load_from_file(filename.c_str());  // 加载YAML配置文件
	if(config == NULL)                                             // 如果配置文件加载失败
	{
		WTSLogger::error("Loading config file {} failed", filename);  // 记录错误日志
		return;                                                     // 退出初始化
	}

	// 原注释：加载市场信息
	// ===== 第一步：加载基础数据文件 =====
	WTSVariant* cfgBF = config->get("basefiles");                 // 获取基础数据文件配置节点
	if (cfgBF->get("session"))                                     // 如果配置了交易时段文件
	{
		g_baseDataMgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段信息
		WTSLogger::info("Trading sessions loaded");                // 记录加载成功日志
	}

	// 加载商品信息（支持单个文件或多个文件）
	WTSVariant* cfgItem = cfgBF->get("commodity");                 // 获取商品配置文件节点
	if (cfgItem)                                                   // 如果配置了商品文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)              // 如果是单个文件（字符串类型）
		{
			g_baseDataMgr.loadCommodities(cfgItem->asCString());   // 加载单个商品文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)         // 如果是多个文件（数组类型）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)        // 遍历文件数组
			{
				g_baseDataMgr.loadCommodities(cfgItem->get(i)->asCString());  // 逐个加载商品文件
			}
		}
	}

	// 加载合约信息（支持单个文件或多个文件）
	cfgItem = cfgBF->get("contract");                              // 获取合约配置文件节点
	if (cfgItem)                                                   // 如果配置了合约文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)              // 如果是单个文件（字符串类型）
		{
			g_baseDataMgr.loadContracts(cfgItem->asCString());    // 加载单个合约文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)         // 如果是多个文件（数组类型）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)        // 遍历文件数组
			{
				g_baseDataMgr.loadContracts(cfgItem->get(i)->asCString());   // 逐个加载合约文件
			}
		}
	}

	// 加载节假日信息
	if (cfgBF->get("holiday"))                                     // 如果配置了节假日文件
	{
		g_baseDataMgr.loadHolidays(cfgBF->getCString("holiday"));   // 加载节假日信息
		WTSLogger::info("Holidays loaded");                        // 记录加载成功日志
	}
	
	// ===== 第二步：加载热力规则和自定义规则 =====
	if (cfgBF->get("hot"))                                         // 如果配置了主力合约热力规则文件
	{
		g_hotMgr.loadHots(cfgBF->getCString("hot"));               // 加载主力合约热力规则
		WTSLogger::log_raw(LL_INFO, "Hot rules loaded");           // 记录加载成功日志
	}

	if (cfgBF->get("second"))                                       // 如果配置了次主力合约规则文件
	{
		g_hotMgr.loadSeconds(cfgBF->getCString("second"));          // 加载次主力合约规则
		WTSLogger::log_raw(LL_INFO, "Second rules loaded");        // 记录加载成功日志
	}

	if (cfgBF->has("rules"))                                        // 如果配置了自定义规则
	{
		auto cfgRules = cfgBF->get("rules");                       // 获取自定义规则配置节点
		auto tags = cfgRules->memberNames();                       // 获取所有规则标签名
		for (const std::string& ruleTag : tags)                    // 遍历每个规则标签
		{
			// 加载自定义规则（规则名称，规则文件路径）
			g_hotMgr.loadCustomRules(ruleTag.c_str(), cfgRules->getCString(ruleTag.c_str()));
			WTSLogger::info("{} rules loaded from {}", ruleTag, cfgRules->getCString(ruleTag.c_str()));  // 记录规则加载日志
		}
	}

	// ===== 第三步：初始化广播器 =====
	if (config->has("shmcaster"))                                  // 如果配置了共享内存广播器
	{
		g_shmCaster.init(config->get("shmcaster"));                // 初始化共享内存广播器
		g_dataMgr.add_caster(&g_shmCaster);                        // 将共享内存广播器添加到数据管理器
	}

	if (config->has("broadcaster"))                               // 如果配置了UDP广播器
	{
		g_udpCaster.init(config->get("broadcaster"), &g_baseDataMgr, &g_dataMgr);  // 初始化UDP广播器
		g_dataMgr.add_caster(&g_udpCaster);                        // 将UDP广播器添加到数据管理器
	}

	// ===== 第四步：初始化状态监控器（根据运行模式） =====
	// 原注释：By Wesley @ 2021.12.27
	// 原注释：全天候模式，不需要再使用状态机
	bool bAlldayMode = config->getBoolean("allday");               // 获取全天候模式标志
	if (!bAlldayMode)                                              // 如果不是全天候模式（标准模式）
	{
		// 初始化状态监控器（传入状态监控器配置文件、基础数据管理器、数据管理器）
		g_stateMon.initialize(config->getCString("statemonitor"), &g_baseDataMgr, &g_dataMgr);
	}
	else                                                           // 如果是全天候模式
	{
		WTSLogger::info("QuoteFactory will run in allday mode");    // 记录全天候模式日志
	}
	
	// ===== 第五步：初始化数据管理器 =====
	initDataMgr(config->get("writer"), bAlldayMode);                // 初始化数据管理器（传入存储配置和运行模式）

	// ===== 第六步：初始化指数工厂 =====
	if(config->has("index"))                                        // 如果配置了指数计算模块
	{
		// 原注释：如果存在指数模块要，配置指数
		const char* filename = config->getCString("index");         // 获取指数配置文件路径
		WTSLogger::info("Reading index config from {}...", filename);  // 记录指数配置加载日志
		WTSVariant* var = WTSCfgLoader::load_from_file(filename);   // 加载指数配置文件
		if (var)                                                    // 如果配置文件加载成功
		{
			// 初始化指数工厂（传入指数配置、热力管理器、基础数据管理器、数据管理器）
			g_idxFactory.init(var, &g_hotMgr, &g_baseDataMgr, &g_dataMgr);
			var->release();                                         // 释放配置文件对象
		}
		else                                                        // 如果配置文件加载失败
		{
			WTSLogger::error("Loading index config {} failed", filename);  // 记录错误日志
		}		
	}

	// ===== 第七步：加载和初始化解析器 =====
	WTSVariant* cfgParser = config->get("parsers");                 // 获取解析器配置节点
	if (cfgParser)                                                  // 如果配置了解析器
	{
		if (cfgParser->type() == WTSVariant::VT_String)             // 如果是独立配置文件（字符串类型）
		{
			const char* filename = cfgParser->asCString();          // 获取解析器配置文件路径
			if (StdFile::exists(filename))                          // 如果配置文件存在
			{
				WTSLogger::info("Reading parser config from {}...", filename);  // 记录解析器配置加载日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载解析器配置文件
				if (var)                                            // 如果配置文件加载成功
				{
					initParsers(var->get("parsers"));               // 初始化解析器（从配置文件中获取parsers节点）
					var->release();                                 // 释放配置文件对象
				}
				else                                                // 如果配置文件加载失败
				{
					WTSLogger::error("Loading parser config {} failed", filename);  // 记录错误日志
				}
			}
			else                                                    // 如果配置文件不存在
			{
				WTSLogger::error("Parser configuration {} not exists", filename);  // 记录错误日志
			}
		}
		else if (cfgParser->type() == WTSVariant::VT_Array)       // 如果是内嵌配置（数组类型）
		{
			initParsers(cfgParser);                                 // 直接初始化解析器（使用内嵌配置）
		}
	}

	config->release();                                             // 释放主配置文件对象

	// ===== 第八步：启动所有组件 =====
	g_parsers.run();                                                // 启动所有解析器（开始接收行情数据）

	// 原注释：全天候模式，不启动状态机
	if(!bAlldayMode)                                                // 如果不是全天候模式（标准模式）
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 等待5毫秒（确保解析器完全启动）
		g_stateMon.run();                                          // 启动状态监控器（开始监控交易时段状态）
	}
}

/*!
 * \brief QuoteFactory程序主入口函数
 * \param argc 命令行参数个数
 * \param argv 命令行参数数组
 * \return 程序退出码（0表示正常退出）
 * 
 * 该函数是QuoteFactory程序的主入口，负责：
 * 1. 解析命令行参数
 * 2. 初始化日志系统
 * 3. 设置异常处理和信号处理
 * 4. 加载配置文件
 * 5. 初始化所有组件
 * 6. 进入主循环等待退出信号
 * 
 * 命令行参数：
 * - -c, --config：指定配置文件路径（默认：dtcfg.yaml）
 * - -l, --logcfg：指定日志配置文件路径（默认：logcfgdt.yaml）
 * - -h, --help：显示帮助信息
 * 
 * 程序执行流程：
 * 1. 解析命令行参数
 * 2. 初始化日志系统
 * 3. 设置Windows平台特定的异常处理
 * 4. 安装信号处理钩子
 * 5. 检查配置文件是否存在
 * 6. 调用initialize()初始化所有组件
 * 7. 进入主循环，等待退出信号
 * 8. 正常退出程序
 * 
 * 退出条件：
 * - 接收到SIGTERM或SIGINT信号
 * - 控制台窗口关闭（Windows）
 * - 程序异常退出
 * 
 * 注意：程序运行期间会持续处理行情数据，直到收到退出信号
 */
int main(int argc, char* argv[])
{
	// ===== 第一步：解析命令行参数 =====
	cppcli::Option opt(argc, argv);                                 // 创建命令行解析器

	auto cParam = opt("-c", "--config", "configure filepath, dtcfg.yaml as default", false);  // 配置文件参数
	auto lParam = opt("-l", "--logcfg", "logging configure filepath, logcfgdt.yaml as default", false);  // 日志配置参数

	auto hParam = opt("-h", "--help", "gain help doc", false)->asHelpParam();  // 帮助参数

	opt.parse();                                                    // 解析命令行参数

	if (hParam->exists())                                           // 如果用户请求帮助信息
		return 0;                                                   // 显示帮助后退出

	// ===== 第二步：初始化日志系统 =====
	std::string filename;                                           // 日志配置文件路径变量
	if (lParam->exists())                                           // 如果用户指定了日志配置文件
		filename = lParam->get<std::string>();                       // 使用用户指定的路径
	else                                                            // 如果用户未指定日志配置文件
		filename = "./logcfgdt.yaml";                               // 使用默认路径
	WTSLogger::init(filename.c_str());                              // 初始化日志系统

	// ===== 第三步：Windows平台特定设置 =====
#ifdef _MSC_VER                                                      // 仅在Microsoft Visual C++编译器下编译
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);              // 设置警告报告模式为调试模式
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);             // 设置错误报告模式为调试模式
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);            // 设置断言报告模式为调试模式

	_set_error_mode(_OUT_TO_STDERR);                               // 设置错误输出到标准错误流
	_set_abort_behavior(0, _WRITE_ABORT_MSG);                       // 设置程序中止行为（写入中止消息）

	g_dwMainThreadId = GetCurrentThreadId();                       // 记录主线程ID（用于控制台事件处理）
	SetConsoleCtrlHandler(ConsoleCtrlhandler, TRUE);               // 设置控制台控制处理器

	CMiniDumper::Enable("QuoteFactory.exe", true);                 // 启用MiniDump崩溃转储功能
#endif                                                              // _MSC_VER条件编译结束

	// ===== 第四步：安装信号处理钩子 =====
	bool bExit = false;                                             // 退出标志（控制主循环）
	install_signal_hooks([&bExit](const char* message) {            // 安装信号处理钩子（错误处理回调）
		if(!bExit)                                                  // 如果程序未退出
			WTSLogger::error(message);                              // 记录错误消息
	}, [&bExit](bool toExit) {                                     // 退出处理回调
		if (bExit)                                                  // 如果已经退出
			return;                                                 // 直接返回

		bExit = toExit;                                             // 设置退出标志
		WTSLogger::info("Exit flag is {}", bExit);                 // 记录退出标志日志
	});

	// ===== 第五步：确定配置文件路径 =====
	if (cParam->exists())                                           // 如果用户指定了配置文件
		filename = cParam->get<std::string>();                      // 使用用户指定的配置文件
	else                                                            // 如果用户未指定配置文件
		filename = "./dtcfg.yaml";                                  // 使用默认配置文件

	if(!StdFile::exists(filename.c_str()))                          // 如果配置文件不存在
	{
		fmt::print("confiture {} not exists", filename);            // 输出错误信息（注意：这里有个拼写错误，应该是"configure"）
		return 0;                                                   // 退出程序
	}

	// ===== 第六步：初始化所有组件 =====
	initialize(filename);                                           // 调用初始化函数（传入配置文件路径）

	// ===== 第七步：进入主循环 =====
	while (!bExit)                                                  // 主循环：直到收到退出信号
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 休眠10毫秒（避免CPU占用过高）
	}
	
	return 0;                                                       // 正常退出程序
}

