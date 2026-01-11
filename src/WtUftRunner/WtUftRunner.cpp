/*!
 * \file WtUftRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader UFT策略运行器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtUftRunner类的所有功能，是WonderTrader UFT策略运行器的核心实现。
 * 该文件负责实现UFT策略系统的初始化、配置加载、组件管理和运行控制等功能。
 * 
 * 主要功能实现：
 * 1. 工作目录获取：获取可执行文件所在目录，用于设置工作路径
 * 2. 系统初始化：初始化日志系统，设置工作目录，安装信号钩子
 * 3. 配置加载：从配置文件加载基础数据、引擎配置、数据管理器配置等
 * 4. 组件初始化：初始化交易适配器、解析器适配器、数据管理器、策略管理器等
 * 5. 策略加载：从配置加载UFT策略，创建策略实例和上下文，绑定交易适配器
 * 6. 共享内存管理：初始化共享内存域，支持跨进程参数监控
 * 7. 运行控制：启动引擎、解析器、交易适配器，进入主循环监控运行状态
 * 8. 日志处理：实现日志处理接口（当前为空实现）
 * 
 * 设计特点：
 * - 配置驱动：通过配置文件灵活配置各个组件
 * - 动态加载：支持动态加载策略工厂和适配器
 * - 错误处理：完善的错误处理和日志记录
 * - 信号处理：安装信号钩子，捕获异常和错误
 * - 共享内存：支持共享内存域，实现跨进程参数监控
 * 
 * 初始化流程：
 * 1. 加载基础数据文件（交易时段、商品、合约、节假日）
 * 2. 初始化引擎
 * 3. 初始化数据管理器
 * 4. 初始化共享内存域（如果配置）
 * 5. 初始化动作策略管理器
 * 6. 初始化解析器适配器
 * 7. 初始化交易适配器
 * 8. 初始化UFT策略
 */
#include "WtUftRunner.h"  // 包含UFT策略运行器头文件
#include "../WtUftCore/ShareManager.h"  // 包含共享内存管理器头文件，用于共享内存域管理

#include "../WtUftCore/WtHelper.h"  // 包含WonderTrader辅助工具类，提供路径、时间等工具函数
#include "../WtUftCore/UftStraContext.h"  // 包含UFT策略上下文头文件，用于创建策略上下文

#include "../Includes/WTSVariant.hpp"  // 包含配置变体类，提供WTSVariant类型
#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能
#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置加载器，提供配置文件加载功能
#include "../WTSUtils/SignalHook.hpp"  // 包含信号钩子工具，提供异常捕获功能
#include "../Share/StrUtil.hpp"  // 包含字符串工具函数，提供字符串处理功能

/**
 * @brief 获取可执行文件所在目录
 * @return 可执行文件所在目录的C字符串指针
 * 
 * 获取当前可执行文件所在的工作目录，用于设置WonderTrader的工作路径。
 * 使用静态局部变量缓存路径，避免重复计算。
 * 路径会被标准化处理（统一路径分隔符）。
 */
const char* getBinDir()
{
	static std::string basePath;  // 静态局部变量，缓存工作目录路径
	if (basePath.empty())  // 如果路径为空，则初始化
	{
		basePath = boost::filesystem::initial_path<boost::filesystem::path>().string();  // 获取当前工作目录

		basePath = StrUtil::standardisePath(basePath);  // 标准化路径（统一路径分隔符）
	}

	return basePath.c_str();  // 返回路径的C字符串指针
}



/**
 * @brief 构造函数实现
 * 
 * 创建UFT策略运行器实例，初始化退出标志，安装信号钩子。
 * 信号钩子用于捕获异常和错误，并设置退出标志。
 */
WtUftRunner::WtUftRunner()
	:_to_exit(false)  // 初始化退出标志为false
{
	install_signal_hooks([](const char* message) {  // 安装信号钩子，第一个回调函数用于错误处理
		WTSLogger::error(message);  // 将错误信息记录到日志系统
	}, [this](bool bStopped) {  // 第二个回调函数用于设置退出标志
		_to_exit = bStopped;  // 设置退出标志
		WTSLogger::info("Exit flag is {}", _to_exit);  // 记录退出标志状态到日志
	});
}

/**
 * @brief 析构函数实现
 * 
 * 销毁UFT策略运行器实例，清理资源。
 * 当前实现为空，资源由成员变量的析构函数自动清理。
 */
WtUftRunner::~WtUftRunner()
{
}

/**
 * @brief 初始化运行器实现
 * @param filename 日志配置文件路径
 * 
 * 初始化日志系统，设置WonderTrader的工作目录。
 * 该函数应在创建实例后立即调用，在config()之前调用。
 */
void WtUftRunner::init(const std::string& filename)
{
	WTSLogger::init(filename.c_str());  // 初始化日志系统，从文件加载日志配置

	WtHelper::setInstDir(getBinDir());  // 设置WonderTrader的工作目录（可执行文件所在目录）
}

/**
 * @brief 配置运行器实现
 * @param filename 主配置文件路径
 * @return 配置成功返回true，失败返回false
 * 
 * 加载主配置文件，初始化各个组件。
 * 配置流程：
 * 1. 加载配置文件
 * 2. 加载基础数据（交易时段、商品、合约、节假日）
 * 3. 初始化引擎
 * 4. 初始化数据管理器
 * 5. 初始化共享内存域（如果配置）
 * 6. 初始化动作策略管理器
 * 7. 初始化解析器适配器
 * 8. 初始化交易适配器
 * 9. 初始化UFT策略
 */
bool WtUftRunner::config(const std::string& filename)
{
	_config = WTSCfgLoader::load_from_file(filename.c_str());  // 加载主配置文件
	if(_config == NULL)  // 如果配置文件加载失败
	{
		WTSLogger::error("Loading config file {} failed", filename);  // 记录错误日志
		return false;  // 返回false，表示配置失败
	}

	// ========== 加载基础数据文件 ==========
	WTSVariant* cfgBF = _config->get("basefiles");  // 获取基础数据文件配置节点
	if (cfgBF->get("session"))  // 如果配置了交易时段文件
		_bd_mgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段数据

	WTSVariant* cfgItem = cfgBF->get("commodity");  // 获取商品配置文件配置
	if (cfgItem)  // 如果配置了商品文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件）
		{
			_bd_mgr.loadCommodities(cfgItem->asCString());  // 加载商品数据
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组中的每个文件
			{
				_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());  // 加载每个商品数据文件
			}
		}
	}

	cfgItem = cfgBF->get("contract");  // 获取合约配置文件配置
	if (cfgItem)  // 如果配置了合约文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件）
		{
			_bd_mgr.loadContracts(cfgItem->asCString());  // 加载合约数据
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组中的每个文件
			{
				_bd_mgr.loadContracts(cfgItem->get(i)->asCString());  // 加载每个合约数据文件
			}
		}
	}

	if (cfgBF->get("holiday"))  // 如果配置了节假日文件
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));  // 加载节假日数据

	// ========== 初始化运行环境 ==========
	initEngine();  // 初始化UFT引擎

	// ========== 初始化数据管理 ==========
	initDataMgr();  // 初始化UFT数据管理器

	// ========== 初始化共享内存域 ==========
	if (_config->has("share_domain"))  // 如果配置了共享内存域
	{
		WTSVariant* cfg = _config->get("share_domain");  // 获取共享内存域配置节点
		ShareManager::self().set_engine(&_uft_engine);  // 设置UFT引擎指针，用于参数变更通知

		ShareManager::self().initialize(cfg->getCString("module"));  // 初始化共享内存模块（加载动态库）
		ShareManager::self().init_domain(cfg->getCString("name"));  // 初始化共享内存域（创建交换区和同步区）
	}

	// ========== 初始化动作策略管理器 ==========
	if(!_act_policy.init(_config->getCString("bspolicy")))  // 初始化动作策略管理器，传入配置文件路径
	{
		WTSLogger::error("ActionPolicyMgr init failed, please check config");  // 如果初始化失败，记录错误日志
	}

	// ========== 初始化行情通道（解析器适配器） ==========
	WTSVariant* cfgParser = _config->get("parsers");  // 获取解析器配置节点
	if (cfgParser)  // 如果配置了解析器
	{
		if (cfgParser->type() == WTSVariant::VT_String)  // 如果是字符串类型（配置文件路径）
		{
			const char* filename = cfgParser->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果配置文件存在
			{
				WTSLogger::info("Reading parser config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载解析器配置文件
				if(var)  // 如果配置文件加载成功
				{
					if (!initParsers(var->get("parsers")))  // 初始化解析器适配器
						WTSLogger::error("Loading parsers failed");  // 如果初始化失败，记录错误日志
					var->release();  // 释放配置文件对象
				}
				else  // 如果配置文件加载失败
				{
					WTSLogger::error("Loading parser config {} failed", filename);  // 记录错误日志
				}
			}
			else  // 如果配置文件不存在
			{
				WTSLogger::error("Parser configuration {} not exists", filename);  // 记录错误日志
			}
		}
		else if (cfgParser->type() == WTSVariant::VT_Array)  // 如果是数组类型（直接配置）
		{
			initParsers(cfgParser);  // 直接初始化解析器适配器
		}
	}

	// ========== 初始化交易通道（交易适配器） ==========
	WTSVariant* cfgTraders = _config->get("traders");  // 获取交易适配器配置节点
	if (cfgTraders)  // 如果配置了交易适配器
	{
		if (cfgTraders->type() == WTSVariant::VT_String)  // 如果是字符串类型（配置文件路径）
		{
			const char* filename = cfgTraders->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果配置文件存在
			{
				WTSLogger::info("Reading trader config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载交易适配器配置文件
				if (var)  // 如果配置文件加载成功
				{
					if (!initTraders(var->get("traders")))  // 初始化交易适配器
						WTSLogger::error("Loading traders failed");  // 如果初始化失败，记录错误日志
					var->release();  // 释放配置文件对象
				}
				else  // 如果配置文件加载失败
				{
					WTSLogger::error("Loading trader config {} failed", filename);  // 记录错误日志
				}
			}
			else  // 如果配置文件不存在
			{
				WTSLogger::error("Trader configuration {} not exists", filename);  // 记录错误日志
			}
		}
		else if (cfgTraders->type() == WTSVariant::VT_Array)  // 如果是数组类型（直接配置）
		{
			initTraders(cfgTraders);  // 直接初始化交易适配器
		}
	}

	// ========== 初始化UFT策略 ==========
	initUftStrategies();  // 加载并初始化UFT策略
	
	return true;  // 返回true，表示配置成功
}

/**
 * @brief 初始化UFT策略实现
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中读取UFT策略配置，加载策略工厂，创建策略实例，创建策略上下文，
 * 绑定交易适配器，并将策略上下文添加到引擎。
 * 
 * 初始化流程：
 * 1. 获取策略配置节点
 * 2. 加载策略工厂（从uft目录加载动态库）
 * 3. 遍历配置中的每个策略
 * 4. 创建策略实例
 * 5. 初始化策略（传入参数配置）
 * 6. 创建策略上下文
 * 7. 绑定交易适配器
 * 8. 将策略上下文添加到引擎
 */
bool WtUftRunner::initUftStrategies()
{
	WTSVariant* cfg = _config->get("strategies");  // 获取策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	cfg = cfg->get("uft");  // 获取UFT策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	std::string path = WtHelper::getCWD() + "uft/";  // 构建策略工厂路径（工作目录下的uft子目录）
	_uft_stra_mgr.loadFactories(path.c_str());  // 加载策略工厂（从uft目录加载动态库）

	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历配置中的每个策略
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前策略的配置项
		if(!cfgItem->getBoolean("active"))  // 如果策略未激活
			continue;  // 跳过该策略
		const char* id = cfgItem->getCString("id");  // 获取策略ID
		const char* name = cfgItem->getCString("name");  // 获取策略名称（对应动态库中的策略类名）
		UftStrategyPtr stra = _uft_stra_mgr.createStrategy(name, id);  // 创建策略实例
		if (stra == NULL)  // 如果策略创建失败
		{
			WTSLogger::error("UFT Strategy {} create failed", name);  // 记录错误日志
			continue;  // 跳过该策略
		}
		else  // 如果策略创建成功
		{
			WTSLogger::info("UFT Strategy {}({}) created", name, id);  // 记录信息日志
		}

		stra->self()->init(cfgItem->get("params"));  // 初始化策略，传入参数配置
		UftStraContext* ctx = new UftStraContext(&_uft_engine, id);  // 创建策略上下文
		ctx->set_strategy(stra->self());  // 设置策略上下文关联的策略实例

		const char* traderid = cfgItem->getCString("trader");  // 获取交易适配器ID
		TraderAdapterPtr trader = _traders.getAdapter(traderid);  // 从交易适配器管理器获取交易适配器
		if(trader)  // 如果交易适配器存在
		{
			ctx->setTrader(trader.get());  // 设置策略上下文的交易适配器
			trader->addSink(ctx);  // 将策略上下文添加到交易适配器的接收者列表（接收交易回报）
		}
		else  // 如果交易适配器不存在
		{
			WTSLogger::error("Trader {} not exists, binding trader to UFT strategy failed", traderid);  // 记录错误日志
		}

		_uft_engine.addContext(UftContextPtr(ctx));  // 将策略上下文添加到引擎（引擎会管理策略上下文）
	}

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化引擎实现
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中读取环境配置，初始化UFT引擎，设置交易适配器管理器。
 * 引擎初始化时会设置基础数据管理器、数据管理器、事件通知器等依赖。
 */
bool WtUftRunner::initEngine()
{
	WTSVariant* cfg = _config->get("env");  // 获取环境配置节点
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	WTSLogger::info("Trading enviroment initialzied with engine: UFT");  // 记录信息日志
	_uft_engine.init(cfg, &_bd_mgr, &_data_mgr, &_notifier);  // 初始化UFT引擎，传入环境配置、基础数据管理器、数据管理器、事件通知器

	_uft_engine.set_adapter_mgr(&_traders);  // 设置交易适配器管理器（引擎需要访问交易适配器）

	return true;  // 返回true，表示初始化成功
}


/**
 * @brief 初始化数据管理器实现
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中读取数据管理器配置，初始化UFT数据管理器。
 * 数据管理器负责管理实时和历史数据，需要关联引擎以获取时间等信息。
 */
bool WtUftRunner::initDataMgr()
{
	WTSVariant*cfg = _config->get("data");  // 获取数据管理器配置节点
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	_data_mgr.init(cfg, &_uft_engine);  // 初始化UFT数据管理器，传入配置和引擎指针
	WTSLogger::info("Data manager initialized");  // 记录信息日志

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化解析器适配器实现
 * @param cfgParser 解析器适配器配置节点（数组类型）
 * @return 初始化成功返回true，失败返回false
 * 
 * 根据配置创建并初始化解析器适配器实例，添加到解析器适配器管理器。
 * 配置可以是数组形式，每个元素包含一个解析器适配器的配置。
 * 如果配置项没有指定id，会自动生成一个唯一的id。
 */
bool WtUftRunner::initParsers(WTSVariant* cfgParser)
{
	if (cfgParser == NULL)  // 如果配置为空
		return false;  // 返回false，表示初始化失败

	uint32_t count = 0;  // 计数器，记录成功加载的解析器数量
	for (uint32_t idx = 0; idx < cfgParser->size(); idx++)  // 遍历配置数组中的每个解析器配置
	{
		WTSVariant* cfgItem = cfgParser->get(idx);  // 获取当前解析器的配置项
		if(!cfgItem->getBoolean("active"))  // 如果解析器未激活
			continue;  // 跳过该解析器

		const char* id = cfgItem->getCString("id");  // 获取解析器ID
		// By Wesley @ 2021.12.14
		// 如果id为空，则生成自动id
		std::string realid = id;  // 保存解析器ID
		if (realid.empty())  // 如果ID为空
		{
			static uint32_t auto_parserid = 1000;  // 静态变量，用于生成自动ID（从1000开始）
			realid = StrUtil::printf("auto_parser_%u", auto_parserid++);  // 生成自动ID（格式：auto_parser_1000, auto_parser_1001, ...）
		}

		ParserAdapterPtr adapter(new ParserAdapter);  // 创建解析器适配器实例
		adapter->init(realid.c_str(), cfgItem, &_uft_engine, &_bd_mgr);  // 初始化解析器适配器，传入ID、配置、引擎指针、基础数据管理器
		_parsers.addAdapter(realid.c_str(), adapter);  // 将解析器适配器添加到管理器

		count++;  // 增加计数器
	}

	WTSLogger::info("{} parsers loaded", count);  // 记录信息日志，显示加载的解析器数量
	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化交易适配器实现
 * @param cfgTrader 交易适配器配置节点（数组类型）
 * @return 初始化成功返回true，失败返回false
 * 
 * 根据配置创建并初始化交易适配器实例，添加到交易适配器管理器。
 * 配置必须是数组类型，每个元素包含一个交易适配器的配置。
 */
bool WtUftRunner::initTraders(WTSVariant* cfgTrader)
{
	if (cfgTrader == NULL || cfgTrader->type() != WTSVariant::VT_Array)  // 如果配置为空或类型不正确
		return false;  // 返回false，表示初始化失败
	
	uint32_t count = 0;  // 计数器，记录成功加载的交易适配器数量
	for (uint32_t idx = 0; idx < cfgTrader->size(); idx++)  // 遍历配置数组中的每个交易适配器配置
	{
		WTSVariant* cfgItem = cfgTrader->get(idx);  // 获取当前交易适配器的配置项
		if (!cfgItem->getBoolean("active"))  // 如果交易适配器未激活
			continue;  // 跳过该交易适配器

		const char* id = cfgItem->getCString("id");  // 获取交易适配器ID

		TraderAdapterPtr adapter(new TraderAdapter());  // 创建交易适配器实例
		adapter->init(id, cfgItem, &_bd_mgr, &_act_policy);  // 初始化交易适配器，传入ID、配置、基础数据管理器、动作策略管理器

		_traders.addAdapter(id, adapter);  // 将交易适配器添加到管理器

		count++;  // 增加计数器
	}

	WTSLogger::info("{} traders loaded", count);  // 记录信息日志，显示加载的交易适配器数量

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化事件通知器实现
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中读取事件通知器配置，初始化事件通知器。
 * 事件通知器负责事件通知和回调功能。
 */
bool WtUftRunner::initEvtNotifier()
{
	WTSVariant* cfg = _config->get("notifier");  // 获取事件通知器配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	_notifier.init(cfg);  // 初始化事件通知器，传入配置

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 运行运行器实现
 * @param bAsync 是否异步运行（默认false，当前版本未使用）
 * 
 * 启动引擎、解析器、交易适配器，并进入主循环监控运行状态。
 * 如果配置了共享内存域，会启动参数监控。
 * 主循环会一直运行直到_to_exit标志为true（由信号钩子设置）。
 */
void WtUftRunner::run(bool bAsync /* = false */)
{
	try  // 异常处理块
	{
		_uft_engine.run();  // 启动UFT引擎（初始化策略上下文，创建实时ticker等）

		_parsers.run();  // 启动解析器适配器（启动行情接收线程）

		_traders.run();  // 启动交易适配器（启动交易通道连接）

		ShareManager::self().start_watching(2);  // 启动共享内存参数监控（监控间隔2微秒，0表示无限循环检查）

		while(!_to_exit)  // 主循环，直到退出标志为true
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 休眠10毫秒，避免CPU占用过高
		}
	}
	catch (...)  // 捕获所有异常
	{
		print_stack_trace([](const char* message) {  // 打印堆栈跟踪信息
			WTSLogger::error(message);  // 将错误信息记录到日志系统
		});
	}
}

/**
 * @brief 日志标签数组
 * 
 * 定义日志级别的字符串标签数组，用于日志输出。
 * 数组索引对应WTSLogLevel枚举值。
 */
const char* LOG_TAGS[] = {
	"all",    // 所有日志级别
	"debug",  // 调试级别
	"info",   // 信息级别
	"warn",   // 警告级别
	"error",  // 错误级别
	"fatal",  // 致命错误级别
	"none"    // 无日志级别
};

/**
 * @brief 处理日志追加实现
 * @param ll 日志级别
 * @param msg 日志消息
 * 
 * 实现ILogHandler接口，处理日志输出。
 * 当前实现为空，日志由WTSLogger统一处理。
 */
void WtUftRunner::handleLogAppend(WTSLogLevel ll, const char* msg)
{
	// 当前实现为空，日志由WTSLogger统一处理
}