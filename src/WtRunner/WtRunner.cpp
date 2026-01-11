/*!
 * \file WtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader策略运行器类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtRunner类的所有方法，包括初始化、配置、运行等功能。
 * 该类是WonderTrader框架的核心运行器，负责统一管理和协调各个组件的生命周期。
 * 
 * 主要功能：
 * 1. 初始化实现：初始化日志系统和运行环境
 * 2. 配置实现：从配置文件加载并初始化各组件（基础数据、数据管理器、开平策略、行情通道、交易通道、执行器、策略等）
 * 3. 运行实现：启动各组件运行，支持同步和异步运行模式
 * 4. 组件初始化：实现各组件（交易通道、行情通道、执行器、数据管理器、事件通知器、策略等）的初始化逻辑
 * 5. 引擎初始化：根据配置选择并初始化对应的交易引擎（CTA、HFT或选股引擎）
 * 6. 日志处理：实现ILogHandler接口，处理系统日志的转发和分发
 * 
 * 设计特点：
 * - 配置驱动：通过配置文件灵活配置各组件
 * - 组件化设计：各组件独立初始化，便于扩展和维护
 * - 异常处理：使用try-catch捕获异常，确保程序稳定运行
 * - 信号处理：安装信号钩子，捕获异常和错误
 * - 引擎选择：根据配置自动选择CTA、HFT或选股引擎
 */

#include "WtRunner.h"  // 包含当前类的头文件

#include "../WtCore/WtHelper.h"  // 包含WonderTrader辅助工具类，提供路径、时间等工具函数
#include "../WtCore/CtaStraContext.h"  // 包含CTA策略上下文头文件，使用CtaStraContext类
#include "../WtCore/HftStraContext.h"  // 包含HFT策略上下文头文件，使用HftStraContext类
#include "../WtCore/WtDiffExecuter.h"  // 包含差分执行器头文件，使用WtDiffExecuter类

#include "../Includes/WTSVariant.hpp"  // 包含配置变体类，提供WTSVariant类型
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息定义，提供WTSContractInfo等类型
#include "../WTSTools/WTSLogger.h"  // 包含日志工具类，提供日志记录功能
#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置加载器，提供配置文件加载功能
#include "../WTSUtils/SignalHook.hpp"  // 包含信号钩子工具，提供异常捕获功能
#include "../Share/StrUtil.hpp"  // 包含字符串工具函数，提供字符串处理功能


/**
 * @brief 获取二进制文件目录
 * 
 * 获取当前可执行文件所在的目录路径。
 * 使用静态变量缓存路径，避免重复获取。
 * 
 * @return 返回二进制文件目录路径字符串指针
 * 
 * 实现说明：
 * - 使用静态变量缓存路径，提高性能
 * - 通过boost::filesystem获取初始路径
 * - 标准化路径格式（统一使用正斜杠或反斜杠）
 */
const char* getBinDir()
{
	static std::string basePath;  // 静态路径字符串，缓存二进制文件目录路径
	if (basePath.empty())  // 如果路径未初始化
	{
		basePath = boost::filesystem::initial_path<boost::filesystem::path>().string();  // 获取当前工作目录路径

		basePath = StrUtil::standardisePath(basePath);  // 标准化路径格式（统一使用正斜杠或反斜杠）
	}

	return basePath.c_str();  // 返回路径字符串的C字符串指针
}



/**
 * @brief 构造函数实现
 * 
 * 创建策略运行器实例，初始化成员变量。
 * 在构造函数中安装信号钩子，用于捕获异常和错误。
 * 
 * 初始化流程：
 * 1. 使用初始化列表初始化成员变量
 * 2. 安装信号钩子，捕获程序异常和错误
 * 3. 信号钩子会将错误信息记录到日志系统
 * 4. 信号钩子会设置退出标志，用于优雅退出
 */
WtRunner::WtRunner()
	: _data_store(NULL)  // 初始化数据存储对象指针为NULL
	, _is_hft(false)  // 初始化高频引擎标志为false
	, _is_sel(false)  // 初始化选股引擎标志为false
	, _to_exit(false)  // 初始化退出标志为false
{
	install_signal_hooks([](const char* message) {  // 安装信号钩子，使用lambda表达式作为错误回调函数
		WTSLogger::error(message);  // 将错误信息记录到日志系统
	}, [this](bool bStopped) {  // 使用lambda表达式作为停止回调函数
		_to_exit = bStopped;  // 设置退出标志
		WTSLogger::info("Exit flag is {}", _to_exit);  // 记录退出标志状态到日志
	});
}


/**
 * @brief 析构函数实现
 * 
 * 清理策略运行器占用的资源。
 * 当前实现为空析构函数。
 */
WtRunner::~WtRunner()
{
}

/**
 * @brief 初始化策略运行器
 * 
 * 初始化日志系统和运行环境。
 * 
 * @param filename 日志配置文件路径
 * 
 * 初始化流程：
 * 1. 初始化日志系统（从文件加载配置）
 * 2. 设置安装目录路径（用于查找动态库和配置文件）
 * 3. 检查日志配置文件是否存在（如果不存在则记录警告）
 */
void WtRunner::init(const std::string& filename)
{
	WTSLogger::init(filename.c_str());  // 初始化日志系统，从文件加载日志配置

	WtHelper::setInstDir(getBinDir());  // 设置安装目录路径，用于查找动态库和配置文件

	if(!StdFile::exists(filename.c_str()))  // 如果日志配置文件不存在
	{
		WTSLogger::warn("logging configure {} not exists", filename);  // 记录警告日志
	}
}

/**
 * @brief 配置策略运行器
 * 
 * 从配置文件加载配置并初始化各组件。
 * 
 * @param filename 配置文件路径
 * @return 配置成功返回true，失败返回false
 * 
 * 配置流程：
 * 1. 加载主配置文件
 * 2. 加载基础数据文件（交易时段、商品、合约、节假日、热点合约、次主力合约、自定义规则等）
 * 3. 标记合约的热点标志（主力合约、次主力合约）
 * 4. 初始化交易引擎（根据配置选择CTA、HFT或选股引擎）
 * 5. 初始化数据管理器
 * 6. 初始化开平策略
 * 7. 初始化行情通道（解析器）
 * 8. 初始化交易通道（交易适配器）
 * 9. 初始化事件通知器
 * 10. 如果不是高频引擎，初始化执行器和路由规则
 * 11. 初始化策略（CTA策略或HFT策略）
 */
bool WtRunner::config(const std::string& filename)
{
	_config = WTSCfgLoader::load_from_file(filename);  // 加载主配置文件
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
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));  // 加载节假日数据

	if (cfgBF->get("hot"))  // 如果配置了主力合约文件
		_hot_mgr.loadHots(cfgBF->getCString("hot"));  // 加载主力合约数据

	if (cfgBF->get("second"))  // 如果配置了次主力合约文件
		_hot_mgr.loadSeconds(cfgBF->getCString("second"));  // 加载次主力合约数据

	// ========== 标记合约的热点标志 ==========
	WTSArray* ayContracts = _bd_mgr.getContracts();  // 获取所有合约数组
	for (auto it = ayContracts->begin(); it != ayContracts->end(); it++)  // 遍历所有合约
	{
		WTSContractInfo* cInfo = (WTSContractInfo*)(*it);  // 获取合约信息对象
		bool isHot = _hot_mgr.isHot(cInfo->getExchg(), cInfo->getCode());  // 检查是否是主力合约
		bool isSecond = _hot_mgr.isSecond(cInfo->getExchg(), cInfo->getCode());  // 检查是否是次主力合约

		std::string hotCode = cInfo->getFullPid();  // 获取合约的完整品种代码
		if (isHot)  // 如果是主力合约
			hotCode += ".HOT";  // 添加.HOT后缀
		else if (isSecond)  // 如果是次主力合约
			hotCode += ".2ND";  // 添加.2ND后缀
		else  // 如果既不是主力也不是次主力
			hotCode = "";  // 清空热点代码

		cInfo->setHotFlag(isHot ? 1 : (isSecond ? 2 : 0), hotCode.c_str());  // 设置合约的热点标志
		// 参数说明：
		// - 第一个参数：热点标志（1表示主力，2表示次主力，0表示普通）
		// - 第二个参数：热点代码（如"rb.HOT"、"rb.2ND"或空字符串）
	}
	ayContracts->release();  // 释放合约数组

	// ========== 加载自定义规则 ==========
	if (cfgBF->has("rules"))  // 如果配置了自定义规则
	{
		auto cfgRules = cfgBF->get("rules");  // 获取规则配置对象
		auto tags = cfgRules->memberNames();  // 获取所有规则标签名称
		for (const std::string& ruleTag : tags)  // 遍历所有规则标签
		{
			_hot_mgr.loadCustomRules(ruleTag.c_str(), cfgRules->getCString(ruleTag.c_str()));  // 加载自定义规则
			WTSLogger::info("{} rules loaded from {}", ruleTag, cfgRules->getCString(ruleTag.c_str()));  // 记录日志
		}
	}

	// ========== 初始化运行环境 ==========
	initEngine();  // 初始化交易引擎（根据配置选择CTA、HFT或选股引擎）

	// ========== 初始化数据管理 ==========
	initDataMgr();  // 初始化数据管理器，加载数据存储模块

	// ========== 初始化开平策略 ==========
	if (!initActionPolicy())  // 如果开平策略初始化失败
		return false;  // 返回false，表示配置失败

	// ========== 初始化行情通道（解析器） ==========
	WTSVariant* cfgParser = _config->get("parsers");  // 获取行情通道配置
	if (cfgParser)  // 如果配置了行情通道
	{
		if (cfgParser->type() == WTSVariant::VT_String)  // 如果是配置文件路径（字符串类型）
		{
			const char* filename = cfgParser->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果配置文件存在
			{
				WTSLogger::info("Reading parser config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载行情通道配置文件
				if(var)  // 如果配置文件加载成功
				{
					if (!initParsers(var->get("parsers")))  // 如果解析器初始化失败
						WTSLogger::error("Loading parsers failed");  // 记录错误日志
					var->release();  // 释放配置对象
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
		else if (cfgParser->type() == WTSVariant::VT_Array)  // 如果是配置数组（数组类型）
		{
			initParsers(cfgParser);  // 直接初始化解析器（配置已包含在数组中）
		}
	}

	// ========== 初始化交易通道（交易适配器） ==========
	WTSVariant* cfgTraders = _config->get("traders");  // 获取交易通道配置
	if (cfgTraders)  // 如果配置了交易通道
	{
		if (cfgTraders->type() == WTSVariant::VT_String)  // 如果是配置文件路径（字符串类型）
		{
			const char* filename = cfgTraders->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果配置文件存在
			{
				WTSLogger::info("Reading trader config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载交易通道配置文件
				if (var)  // 如果配置文件加载成功
				{
					if (!initTraders(var->get("traders")))  // 如果交易适配器初始化失败
						WTSLogger::error("Loading traders failed");  // 记录错误日志
					var->release();  // 释放配置对象
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
		else if (cfgTraders->type() == WTSVariant::VT_Array)  // 如果是配置数组（数组类型）
		{
			initTraders(cfgTraders);  // 直接初始化交易适配器（配置已包含在数组中）
		}
	}

	// ========== 初始化事件通知器 ==========
	initEvtNotifier();  // 初始化事件通知器，用于系统事件通知

	// ========== 初始化执行器（仅非高频引擎需要） ==========
	//如果不是高频引擎,则需要配置执行模块
	if (!_is_hft)  // 如果不是高频引擎（CTA引擎和选股引擎需要执行器）
	{
		WTSVariant* cfgExec = _config->get("executers");  // 获取执行器配置
		if (cfgExec != NULL)  // 如果配置了执行器
		{
			if (cfgExec->type() == WTSVariant::VT_String)  // 如果是配置文件路径（字符串类型）
			{
				const char* filename = cfgExec->asCString();  // 获取配置文件路径
				if (StdFile::exists(filename))  // 如果配置文件存在
				{
					WTSLogger::info("Reading executer config from {}...", filename);  // 记录日志
					WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载执行器配置文件
					if (var)  // 如果配置文件加载成功
					{
						if (!initExecuters(var->get("executers")))  // 如果执行器初始化失败
							WTSLogger::error("Loading executers failed");  // 记录错误日志

						WTSVariant* c = var->get("routers");  // 获取路由规则配置
						if (c != NULL)  // 如果配置了路由规则
							_cta_engine.loadRouterRules(c);  // 加载路由规则到CTA引擎

						var->release();  // 释放配置对象
					}
					else  // 如果配置文件加载失败
					{
						WTSLogger::error("Loading executer config {} failed", filename);  // 记录错误日志
					}
				}
				else  // 如果配置文件不存在
				{
					WTSLogger::error("Trader configuration {} not exists", filename);  // 记录错误日志
				}
			}
			else if (cfgExec->type() == WTSVariant::VT_Array)  // 如果是配置数组（数组类型）
			{
				initExecuters(cfgExec);  // 直接初始化执行器（配置已包含在数组中）
			}
		}

		// 从主配置加载路由规则（如果执行器配置文件中没有）
		WTSVariant* cfgRouter = _config->get("routers");  // 获取路由规则配置
		if (cfgRouter != NULL)  // 如果配置了路由规则
			_cta_engine.loadRouterRules(cfgRouter);  // 加载路由规则到CTA引擎
	}

	// ========== 初始化策略 ==========
	if (!_is_hft)  // 如果不是高频引擎
		initCtaStrategies();  // 初始化CTA策略
	else  // 如果是高频引擎
		initHftStrategies();  // 初始化HFT策略
	
	return true;  // 配置成功，返回true
}

/**
 * @brief 初始化CTA策略
 * 
 * 从配置加载并初始化CTA策略。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取策略配置对象（strategies节点）
 * 2. 获取CTA策略配置数组（strategies.cta节点）
 * 3. 加载CTA策略工厂（从cta目录加载动态库）
 * 4. 遍历配置数组中的每个策略配置
 * 5. 检查策略是否启用（active字段）
 * 6. 创建策略实例并初始化
 * 7. 创建策略上下文并设置策略
 * 8. 将策略上下文添加到CTA引擎
 */
bool WtRunner::initCtaStrategies()
{
	WTSVariant* cfg = _config->get("strategies");  // 获取策略配置对象
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	cfg = cfg->get("cta");  // 获取CTA策略配置数组
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	std::string path = WtHelper::getCWD() + "cta/";  // 拼接CTA策略工厂目录路径
	_cta_stra_mgr.loadFactories(path.c_str());  // 从指定目录加载CTA策略工厂动态库

	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历配置数组中的每个策略配置
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前策略配置项
		if (!cfgItem->getBoolean("active"))  // 如果策略未启用（active字段为false）
			continue;  // 跳过该策略，继续处理下一个

		const char* id = cfgItem->getCString("id");  // 获取策略ID
		const char* name = cfgItem->getCString("name");  // 获取策略名称
		int32_t slippage = cfgItem->getInt32("slippage");  // 获取滑点参数（单位：最小变动价位）
		CtaStrategyPtr stra = _cta_stra_mgr.createStrategy(name, id);  // 创建CTA策略实例
		stra->self()->init(cfgItem->get("params"));  // 初始化策略，传入策略参数配置
		CtaStraContext* ctx = new CtaStraContext(&_cta_engine, id, slippage);  // 创建CTA策略上下文
		// 参数说明：
		// - &_cta_engine: CTA引擎指针
		// - id: 策略ID
		// - slippage: 滑点参数
		ctx->set_strategy(stra->self());  // 设置策略上下文关联的策略实例
		_cta_engine.addContext(CtaContextPtr(ctx));  // 将策略上下文添加到CTA引擎
	}

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化HFT策略
 * 
 * 从配置加载并初始化HFT策略。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取策略配置对象（strategies节点）
 * 2. 获取HFT策略配置数组（strategies.hft节点）
 * 3. 加载HFT策略工厂（从hft目录加载动态库）
 * 4. 遍历配置数组中的每个策略配置
 * 5. 检查策略是否启用（active字段）
 * 6. 创建策略实例并初始化
 * 7. 创建策略上下文并设置策略
 * 8. 绑定交易通道到策略上下文
 * 9. 将策略上下文添加到HFT引擎
 */
bool WtRunner::initHftStrategies()
{
	WTSVariant* cfg = _config->get("strategies");  // 获取策略配置对象
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	cfg = cfg->get("hft");  // 获取HFT策略配置数组
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	std::string path = WtHelper::getCWD() + "hft/";  // 拼接HFT策略工厂目录路径
	_hft_stra_mgr.loadFactories(path.c_str());  // 从指定目录加载HFT策略工厂动态库

	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历配置数组中的每个策略配置
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前策略配置项
		if (!cfgItem->getBoolean("active"))  // 如果策略未启用（active字段为false）
			continue;  // 跳过该策略，继续处理下一个

		const char* id = cfgItem->getCString("id");  // 获取策略ID
		const char* name = cfgItem->getCString("name");  // 获取策略名称
		bool agent = cfgItem->getBoolean("agent");  // 获取代理标志（是否使用代理模式）
		int32_t slippage = cfgItem->getInt32("slippage");  // 获取滑点参数（单位：最小变动价位）
		HftStrategyPtr stra = _hft_stra_mgr.createStrategy(name, id);  // 创建HFT策略实例
		if (stra == NULL)  // 如果策略创建失败
			continue;  // 跳过该策略，继续处理下一个

		stra->self()->init(cfgItem->get("params"));  // 初始化策略，传入策略参数配置
		HftStraContext* ctx = new HftStraContext(&_hft_engine, id, agent, slippage);  // 创建HFT策略上下文
		// 参数说明：
		// - &_hft_engine: HFT引擎指针
		// - id: 策略ID
		// - agent: 代理标志
		// - slippage: 滑点参数
		ctx->set_strategy(stra->self());  // 设置策略上下文关联的策略实例

		const char* traderid = cfgItem->getCString("trader");  // 获取交易通道ID
		TraderAdapterPtr trader = _traders.getAdapter(traderid);  // 从交易适配器管理器中获取交易适配器
		if(trader)  // 如果交易适配器存在
		{
			ctx->setTrader(trader.get());  // 设置策略上下文的交易适配器
			trader->addSink(ctx);  // 将策略上下文添加到交易适配器的接收者列表
		}
		else  // 如果交易适配器不存在
		{
			WTSLogger::error("Trader {} not exists, binding trader to HFT strategy failed", traderid);  // 记录错误日志
		}

		_hft_engine.addContext(HftContextPtr(ctx));  // 将策略上下文添加到HFT引擎
	}

	return true;  // 初始化成功，返回true
}


/**
 * @brief 初始化交易引擎
 * 
 * 根据配置选择并初始化对应的交易引擎（CTA、HFT或选股引擎）。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 引擎选择逻辑：
 * - 如果配置名称为空或"cta"，使用CTA引擎
 * - 如果配置名称为"sel"，使用选股引擎
 * - 如果配置名称为"hft"，使用HFT引擎
 * 
 * 初始化流程：
 * 1. 获取环境配置（env节点）
 * 2. 获取引擎名称（env.name字段）
 * 3. 根据引擎名称设置标志位
 * 4. 初始化对应的引擎实例
 * 5. 设置引擎的交易适配器管理器
 */
bool WtRunner::initEngine()
{
	WTSVariant* cfg = _config->get("env");  // 获取环境配置节点
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	const char* name = cfg->getCString("name");  // 获取引擎名称
	
	if (strlen(name) == 0 || wt_stricmp(name, "cta") == 0)  // 如果名称为空或"cta"（不区分大小写）
	{
		_is_hft = false;  // 设置高频引擎标志为false
		_is_sel = false;  // 设置选股引擎标志为false
	}
	else if (wt_stricmp(name, "sel") == 0)  // 如果名称为"sel"（不区分大小写）
	{
		_is_sel = true;  // 设置选股引擎标志为true
	}
	else //if (wt_stricmp(name, "hft") == 0)  // 如果名称为"hft"或其他（默认使用HFT引擎）
	{
		_is_hft = true;  // 设置高频引擎标志为true
	}

	if (_is_hft)  // 如果是高频引擎
	{
		WTSLogger::info("Trading enviroment initialzied with engine: HFT");  // 记录日志
		_hft_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr, &_notifier);  // 初始化HFT引擎
		// 参数说明：
		// - cfg: 环境配置对象
		// - &_bd_mgr: 基础数据管理器指针
		// - &_data_mgr: 数据管理器指针
		// - &_hot_mgr: 热点合约管理器指针
		// - &_notifier: 事件通知器指针
		_engine = &_hft_engine;  // 设置当前引擎指针指向HFT引擎
	}
	else if (_is_sel)  // 如果是选股引擎
	{
		WTSLogger::info("Trading enviroment initialzied with engine: SEL");  // 记录日志
		_sel_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr, &_notifier);  // 初始化选股引擎
		_engine = &_sel_engine;  // 设置当前引擎指针指向选股引擎
	}
	else  // 如果是CTA引擎（默认）
	{
		WTSLogger::info("Trading enviroment initialzied with engine: CTA");  // 记录日志
		_cta_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr, &_notifier);  // 初始化CTA引擎
		_engine = &_cta_engine;  // 设置当前引擎指针指向CTA引擎
	}

	_engine->set_adapter_mgr(&_traders);  // 设置引擎的交易适配器管理器，用于访问交易接口

	return true;  // 初始化成功，返回true
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
bool WtRunner::initActionPolicy()
{
	return _act_policy.init(_config->getCString("bspolicy"));  // 初始化开平策略管理器，加载策略文件
}

/**
 * @brief 初始化数据管理器
 * 
 * 从配置初始化数据管理器，加载数据存储模块。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取数据管理器配置（data节点）
 * 2. 调用数据管理器的init方法初始化
 * 3. 数据管理器会加载数据存储模块（如WtDataStorage）
 */
bool WtRunner::initDataMgr()
{
	WTSVariant*cfg = _config->get("data");  // 获取数据管理器配置
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	_data_mgr.init(cfg, _engine);  // 初始化数据管理器
	// 参数说明：
	// - cfg: 数据管理器配置
	// - _engine: 交易引擎指针（作为数据读取器接收者）
	WTSLogger::info("Data manager initialized");  // 记录日志

	return true;  // 初始化成功，返回true
}

/**
 * @brief 初始化行情通道（解析器）
 * 
 * 从配置加载并初始化解析器适配器。
 * 
 * @param cfgParser 行情通道配置对象（解析器配置数组）
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置对象是否有效
 * 2. 遍历配置数组中的每个解析器配置
 * 3. 检查解析器是否启用（active字段）
 * 4. 获取解析器ID，如果为空则自动生成
 * 5. 创建解析器适配器实例并初始化
 * 6. 将解析器适配器添加到管理器
 */
bool WtRunner::initParsers(WTSVariant* cfgParser)
{
	if (cfgParser == NULL)  // 如果配置对象为空
		return false;  // 返回false，表示初始化失败

	uint32_t count = 0;  // 初始化计数器，记录成功加载的解析器数量
	for (uint32_t idx = 0; idx < cfgParser->size(); idx++)  // 遍历配置数组中的每个解析器配置
	{
		WTSVariant* cfgItem = cfgParser->get(idx);  // 获取当前解析器配置项
		if(!cfgItem->getBoolean("active"))  // 如果解析器未启用（active字段为false）
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
		adapter->init(realid.c_str(), cfgItem, _engine, &_bd_mgr, &_hot_mgr);  // 初始化解析器适配器
		// 参数说明：
		// - realid.c_str(): 解析器ID
		// - cfgItem: 解析器配置项
		// - _engine: 交易引擎指针（用于接收行情数据）
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
 * @param cfgExecuter 执行器配置对象（执行器配置数组）
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置对象是否有效
 * 2. 先加载自带的执行器工厂（从executer目录）
 * 3. 遍历配置数组中的每个执行器配置
 * 4. 检查执行器是否启用（active字段）
 * 5. 根据执行器类型（local/diff/dist）创建对应的执行器实例
 * 6. 初始化执行器并配置交易通道
 * 7. 将执行器添加到CTA引擎
 * 
 * 执行器类型说明：
 * - local: 本地执行器，直接连接交易通道执行交易
 * - diff: 差分执行器，支持多账户差分执行
 * - dist: 分布式执行器，支持分布式执行
 */
bool WtRunner::initExecuters(WTSVariant* cfgExecuter)
{
	if (cfgExecuter == NULL || cfgExecuter->type() != WTSVariant::VT_Array)  // 如果配置对象不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	std::string path = WtHelper::getCWD() + "executer/";  // 拼接执行器工厂目录路径
	_exe_factory.loadFactories(path.c_str());  // 从指定目录加载执行器工厂动态库

	uint32_t count = 0;  // 初始化计数器，记录成功加载的执行器数量
	for (uint32_t idx = 0; idx < cfgExecuter->size(); idx++)  // 遍历配置数组中的每个执行器配置
	{
		WTSVariant* cfgItem = cfgExecuter->get(idx);  // 获取当前执行器配置项
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

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎
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

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎
		}
		else  // 如果是分布式执行器或其他类型
		{
			WtDistExecuter* executer = new WtDistExecuter(id);  // 创建分布式执行器实例
			if (!executer->init(cfgItem))  // 如果执行器初始化失败
				return false;  // 返回false，表示初始化失败

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎
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
 * @param cfgTrader 交易通道配置对象（交易适配器配置数组）
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置对象是否有效
 * 2. 遍历配置数组中的每个交易适配器配置
 * 3. 检查交易适配器是否启用（active字段）
 * 4. 创建交易适配器实例并初始化
 * 5. 将交易适配器添加到管理器
 */
bool WtRunner::initTraders(WTSVariant* cfgTrader)
{
	if (cfgTrader == NULL || cfgTrader->type() != WTSVariant::VT_Array)  // 如果配置对象不存在或类型不正确
		return false;  // 返回false，表示初始化失败
	
	uint32_t count = 0;  // 初始化计数器，记录成功加载的交易适配器数量
	for (uint32_t idx = 0; idx < cfgTrader->size(); idx++)  // 遍历配置数组中的每个交易适配器配置
	{
		WTSVariant* cfgItem = cfgTrader->get(idx);  // 获取当前交易适配器配置项
		if (!cfgItem->getBoolean("active"))  // 如果交易适配器未启用（active字段为false）
			continue;  // 跳过该交易适配器，继续处理下一个

		const char* id = cfgItem->getCString("id");  // 获取交易适配器ID

		TraderAdapterPtr adapter(new TraderAdapter(&_notifier));  // 创建交易适配器智能指针，传入事件通知器
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
 * @brief 运行策略运行器
 * 
 * 启动各组件运行。
 * 
 * @param bAsync 是否异步运行，true表示异步运行（不阻塞），false表示同步运行（阻塞）
 * 
 * 运行流程：
 * 1. 启动解析器适配器管理器（行情通道），接收实时行情数据
 * 2. 启动交易适配器管理器（交易通道），处理交易指令
 * 3. 启动交易引擎，运行策略逻辑
 * 4. 如果同步运行，等待退出信号；如果异步运行，直接返回
 * 
 * 异常处理：
 * - 使用try-catch捕获所有异常
 * - 捕获异常后打印堆栈跟踪信息到日志
 * - 确保程序不会因异常而崩溃
 */
void WtRunner::run(bool bAsync /* = false */)
{
	try  // 捕获异常
	{
		_parsers.run();  // 启动解析器适配器管理器（行情通道）
		_traders.run();  // 启动交易适配器管理器（交易通道）

		_engine->run();  // 启动交易引擎，运行策略逻辑

		if(!bAsync)  // 如果同步运行
		{
			while(!_to_exit)  // 循环等待退出信号
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 每次睡眠10毫秒，避免CPU占用过高
			}
		}
	}
	catch (...)  // 捕获所有异常
	{
		print_stack_trace([](const char* message) {  // 打印堆栈跟踪信息，使用lambda表达式作为回调函数
			WTSLogger::error(message);  // 将堆栈跟踪信息记录到日志
		});
	}
}

/**
 * @brief 日志标签数组
 * 
 * 定义日志级别对应的标签字符串数组。
 * 用于将WTSLogLevel枚举值转换为字符串标签。
 * 
 * 日志级别说明：
 * - all: 所有日志
 * - debug: 调试信息
 * - info: 一般信息
 * - warn: 警告信息
 * - error: 错误信息
 * - fatal: 致命错误
 * - none: 无日志
 * 
 * 注意：WTSLogLevel枚举值从100开始，所以数组索引需要减去100
 */
const char* LOG_TAGS[] = {
	"all",    // 索引0：所有日志（WTSLogLevel = 100）
	"debug",  // 索引1：调试信息（WTSLogLevel = 101）
	"info",   // 索引2：一般信息（WTSLogLevel = 102）
	"warn",   // 索引3：警告信息（WTSLogLevel = 103）
	"error",  // 索引4：错误信息（WTSLogLevel = 104）
	"fatal",  // 索引5：致命错误（WTSLogLevel = 105）
	"none"    // 索引6：无日志（WTSLogLevel = 106）
};

/**
 * @brief 处理日志追加事件（ILogHandler接口实现）
 * 
 * 当系统产生新的日志时调用，将日志转发给事件通知器。
 * 
 * @param ll 日志级别（WTSLogLevel枚举值）
 * @param msg 日志消息内容
 * 
 * 处理流程：
 * 1. 将日志级别转换为日志标签字符串（通过LOG_TAGS数组）
 * 2. 通过事件通知器通知日志事件
 * 
 * 日志级别转换：
 * - WTSLogLevel枚举值从100开始
 * - 数组索引 = ll - 100
 */
void WtRunner::handleLogAppend(WTSLogLevel ll, const char* msg)
{
	_notifier.notify_log(LOG_TAGS[ll - 100], msg);  // 通过事件通知器通知日志事件
	// 参数说明：
	// - LOG_TAGS[ll - 100]: 日志标签字符串（将日志级别转换为标签）
	// - msg: 日志消息内容
}

/**
 * @brief 初始化事件通知器
 * 
 * 从配置初始化事件通知器。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取事件通知器配置（notifier节点）
 * 2. 检查配置是否存在且类型正确（VT_Object）
 * 3. 调用事件通知器的init方法初始化
 */
bool WtRunner::initEvtNotifier()
{
	WTSVariant* cfg = _config->get("notifier");  // 获取事件通知器配置
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	_notifier.init(cfg);  // 初始化事件通知器，传入配置对象

	return true;  // 初始化成功，返回true
}