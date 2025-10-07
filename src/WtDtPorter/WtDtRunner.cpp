/*!
 * \file WtDtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据服务运行器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtDtRunner类的所有成员函数，完成数据服务运行器的核心功能。
 * 
 * 主要功能包括：
 * 1. 实现数据服务的初始化逻辑，包括配置加载、日志初始化、模块目录设置
 * 2. 实现基础数据的加载，包括交易时段、商品信息、合约信息、节假日等
 * 3. 实现主力合约规则和次主力合约规则的加载
 * 4. 实现数据管理器的初始化，包括数据写入器和状态监控器的配置
 * 5. 实现数据广播器的初始化，包括UDP广播和共享内存广播
 * 6. 实现指数工厂的初始化，支持自定义指数的计算
 * 7. 实现行情解析器的初始化和管理，支持多个解析器并发运行
 * 8. 实现扩展Parser的创建和管理，支持外部自定义数据源
 * 9. 实现扩展Dumper的创建和管理，支持外部自定义存储
 * 10. 实现扩展Parser的事件处理和订阅管理
 * 11. 实现扩展Dumper的数据转储功能
 * 12. 实现数据服务的启动逻辑，支持同步和异步两种模式
 * 13. 实现信号处理，支持优雅退出
 * 14. 实现异步IO操作，提高系统性能
 * 
 * 设计思想：
 * - 模块化设计：将不同功能分离到不同的管理器中
 * - 配置驱动：通过配置文件控制各模块的行为
 * - 回调机制：通过函数指针实现扩展功能
 * - 异步IO：使用boost::asio实现异步IO操作
 * - 生命周期管理：统一管理所有组件的初始化、启动、停止和释放
 * - 错误处理：对各种错误情况进行检查和处理
 * 
 * 该文件是WtDtRunner功能的核心实现，协调所有组件的运行。
 */
#include "WtDtRunner.h"  // 包含WtDtRunner类声明
#include "ExpParser.h"  // 包含扩展Parser类声明

#include "../WtDtCore/WtHelper.h"  // 包含辅助工具函数

#include "../Includes/WTSSessionInfo.hpp"  // 包含交易时段信息
#include "../Includes/WTSVariant.hpp"  // 包含WTS变体类型
#include "../Includes/WTSDataDef.hpp"  // 包含WTS数据定义
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息

#include "../Share/StrUtil.hpp"  // 包含字符串工具函数

#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置加载器
#include "../WTSTools/WTSLogger.h"  // 包含日志工具
#include "../WTSUtils/SignalHook.hpp"  // 包含信号处理钩子


/**
 * @brief 构造函数
 * 
 * 初始化WtDtRunner对象，设置所有转储回调函数指针为NULL，设置退出标志为false。
 * 在对象创建时，所有成员变量都被初始化为默认值。
 */
WtDtRunner::WtDtRunner()
	: _dumper_for_bars(NULL)     // 初始化K线转储回调指针为NULL
	, _dumper_for_ticks(NULL)    // 初始化Tick转储回调指针为NULL
	, _dumper_for_ordque(NULL)   // 初始化委托队列转储回调指针为NULL
	, _dumper_for_orddtl(NULL)   // 初始化委托明细转储回调指针为NULL
	, _dumper_for_trans(NULL)    // 初始化逐笔成交转储回调指针为NULL
	, _to_exit(false)            // 初始化退出标志为false，表示不退出
{
}


/**
 * @brief 析构函数
 * 
 * 释放WtDtRunner对象占用的资源。
 * 当前实现为空，因为大部分资源会在对象销毁时自动释放。
 */
WtDtRunner::~WtDtRunner()
{
}

/**
 * @brief 启动数据服务
 * @param bAsync 是否异步启动，默认为false（同步启动）
 * @param bAlldayMode 是否全天候模式，默认为false（普通模式）
 * 
 * 该函数启动数据服务，开始运行行情解析器和数据管理器。
 * 
 * 同步模式（bAsync=false）：
 * - 安装信号处理钩子，捕获系统信号（如Ctrl+C）
 * - 在异步IO线程池中启动状态监控器（非全天候模式）
 * - 创建一个工作线程循环执行异步IO任务
 * - 阻塞当前线程直到接收到退出信号
 * 
 * 异步模式（bAsync=true）：
 * - 直接启动状态监控器（非全天候模式）
 * - 立即返回，数据服务在后台运行
 * 
 * 全天候模式（bAlldayMode=true）：
 * - 不启动状态监控器
 * - 适用于7x24交易市场（如数字货币）
 */
void WtDtRunner::start(bool bAsync /* = false */, bool bAlldayMode /* = false */)
{
	// 启动所有行情解析器，开始接收和处理行情数据
	_parsers.run();

    if(!bAsync)  // 同步启动模式
    {
		// 安装信号处理钩子，用于捕获系统信号（如SIGINT、SIGTERM）
		install_signal_hooks(
			// 错误消息处理回调：当捕获到信号时，如果未设置退出标志，则记录错误日志
			[this](const char* message) {
				if(!_to_exit)  // 如果尚未设置退出标志
					WTSLogger::error(message);  // 记录错误消息
			}, 
			// 退出标志设置回调：设置退出标志，触发优雅退出
			[this](bool toExit) {
				if (_to_exit)  // 如果已经设置了退出标志
					return;  // 直接返回，避免重复设置
				_to_exit = toExit;  // 设置退出标志
				WTSLogger::info("Exit flag is {}", _to_exit);  // 记录退出标志状态
			});

		// 在异步IO线程池中投递一个任务：启动状态监控器
		_async_io.post([this, bAlldayMode]() {
			if(!bAlldayMode)  // 如果不是全天候模式
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 短暂延迟5毫秒
				_state_mon.run();  // 启动状态监控器，监控交易时段状态
			}
		});

		// 创建一个工作线程，循环执行异步IO任务
		StdThread trd([this] {
			while (!_to_exit)  // 当退出标志未设置时，持续运行
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(2));  // 短暂延迟2毫秒，避免CPU占用过高
				_async_io.run_one();  // 执行一个异步IO任务
			}
		});

		trd.join();  // 等待工作线程结束，阻塞当前线程直到接收到退出信号
    }
	else  // 异步启动模式
	{
		if (!bAlldayMode)  // 如果不是全天候模式
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 短暂延迟5毫秒
			_state_mon.run();  // 启动状态监控器
		}
		// 异步模式立即返回，数据服务在后台运行
	}
}

/**
 * @brief 初始化数据服务
 * @param cfgFile 配置文件路径或配置内容字符串
 * @param logCfg 日志配置文件路径或配置内容字符串
 * @param modDir 模块目录路径，默认为空字符串（使用当前目录）
 * @param bCfgFile cfgFile是否为文件路径，默认为true
 * @param bLogCfgFile logCfg是否为文件路径，默认为true
 * 
 * 该函数初始化数据服务，执行以下步骤：
 * 1. 初始化日志系统
 * 2. 设置模块目录
 * 3. 加载主配置文件
 * 4. 加载基础数据（交易时段、商品信息、合约信息、节假日）
 * 5. 加载主力合约规则和次主力合约规则
 * 6. 初始化数据广播器（UDP广播、共享内存广播）
 * 7. 初始化状态监控器（非全天候模式）
 * 8. 初始化数据管理器
 * 9. 初始化指数工厂
 * 10. 初始化行情解析器
 */
void WtDtRunner::initialize(const char* cfgFile, const char* logCfg, const char* modDir /* = "" */, bool bCfgFile /* = true */, bool bLogCfgFile /* = true */)
{
	// 初始化日志系统，配置日志输出格式、级别、文件路径等
	WTSLogger::init(logCfg, bLogCfgFile);
	
	// 设置模块目录，用于加载动态链接库（DLL/SO）
	WtHelper::set_module_dir(modDir);

	// 加载主配置文件
	WTSVariant* config = NULL;  // 配置对象指针
	if (bCfgFile)  // 如果cfgFile是文件路径
		config = WTSCfgLoader::load_from_file(cfgFile);  // 从文件加载配置
	else  // 如果cfgFile是配置内容字符串
		config = WTSCfgLoader::load_from_content(cfgFile, false);  // 从字符串加载配置

	// 检查配置是否加载成功
	if(config == NULL)
	{
		WTSLogger::error("Loading config file {} failed", cfgFile);  // 记录错误日志
		return;  // 返回，初始化失败
	}

	//////////////////////////////////////////////////////////////////////////
	// 加载基础数据文件
	//////////////////////////////////////////////////////////////////////////
	WTSVariant* cfgBF = config->get("basefiles");  // 获取basefiles配置节点
	
	// 加载交易时段配置
	if (cfgBF->get("session"))
	{
		_bd_mgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段文件
		WTSLogger::info("Trading sessions loaded");  // 记录日志
	}

	// 加载商品信息配置
	WTSVariant* cfgItem = cfgBF->get("commodity");  // 获取commodity配置节点
	if (cfgItem)
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件）
		{
			_bd_mgr.loadCommodities(cfgItem->asCString());  // 加载商品信息文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组
			{
				_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());  // 逐个加载商品信息文件
			}
		}
	}

	// 加载合约信息配置
	cfgItem = cfgBF->get("contract");  // 获取contract配置节点
	if (cfgItem)
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件）
		{
			_bd_mgr.loadContracts(cfgItem->asCString());  // 加载合约信息文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组
			{
				_bd_mgr.loadContracts(cfgItem->get(i)->asCString());  // 逐个加载合约信息文件
			}
		}
	}

	// 加载节假日配置
	if (cfgBF->get("holiday"))
	{
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));  // 加载节假日文件
		WTSLogger::info("Holidays loaded");  // 记录日志
	}


	// 加载主力合约规则配置
	if (cfgBF->get("hot"))
	{
		_hot_mgr.loadHots(cfgBF->getCString("hot"));  // 加载主力合约规则文件
		WTSLogger::log_raw(LL_INFO, "Hot rules loaded");  // 记录日志
	}

	// 加载次主力合约规则配置
	if (cfgBF->get("second"))
	{
		_hot_mgr.loadSeconds(cfgBF->getCString("second"));  // 加载次主力合约规则文件
		WTSLogger::log_raw(LL_INFO, "Second rules loaded");  // 记录日志
	}

	// 加载自定义合约规则配置
	if (cfgBF->has("rules"))
	{
		auto cfgRules = cfgBF->get("rules");  // 获取rules配置节点
		auto tags = cfgRules->memberNames();  // 获取所有规则标签名称
		for (const std::string& ruleTag : tags)  // 遍历规则标签
		{
			_hot_mgr.loadCustomRules(ruleTag.c_str(), cfgRules->getCString(ruleTag.c_str()));  // 加载自定义规则文件
			WTSLogger::info("{} rules loaded from {}", ruleTag, cfgRules->getCString(ruleTag.c_str()));  // 记录日志
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 初始化数据广播器
	//////////////////////////////////////////////////////////////////////////
	
	// 初始化共享内存广播器
	if (config->has("shmcaster"))
	{
		_shm_caster.init(config->get("shmcaster"));  // 初始化共享内存广播器
		_data_mgr.add_caster(&_shm_caster);  // 将广播器添加到数据管理器
	}

	// 初始化UDP广播器
	if(config->has("broadcaster"))
	{
		_udp_caster.init(config->get("broadcaster"), &_bd_mgr, &_data_mgr);  // 初始化UDP广播器
		_data_mgr.add_caster(&_udp_caster);  // 将广播器添加到数据管理器
	}

	//////////////////////////////////////////////////////////////////////////
	// 初始化状态监控器
	//////////////////////////////////////////////////////////////////////////
	
	// By Wesley @ 2021.12.27
	// 全天候模式，不需要再使用状态机
	bool bAlldayMode = config->getBoolean("allday");  // 获取allday配置项
	if (!bAlldayMode)  // 如果不是全天候模式
	{
		_state_mon.initialize(config->getCString("statemonitor"), &_bd_mgr, &_data_mgr);  // 初始化状态监控器
	}
	else  // 如果是全天候模式
	{
		WTSLogger::info("datakit will run in allday mode");  // 记录日志
	}

	//////////////////////////////////////////////////////////////////////////
	// 初始化数据管理器
	//////////////////////////////////////////////////////////////////////////
	initDataMgr(config->get("writer"), bAlldayMode);

	//////////////////////////////////////////////////////////////////////////
	// 初始化指数工厂
	//////////////////////////////////////////////////////////////////////////
	if (config->has("index"))
	{
		// 如果存在指数模块配置，配置指数
		const char* filename = config->getCString("index");  // 获取指数配置文件路径
		WTSLogger::info("Reading index config from {}...", filename);  // 记录日志
		WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载指数配置文件
		if (var)  // 如果加载成功
		{
			_idx_factory.init(var, &_hot_mgr, &_bd_mgr, &_data_mgr);  // 初始化指数工厂
			var->release();  // 释放配置对象
		}
		else  // 如果加载失败
		{
			WTSLogger::error("Loading index config {} failed", filename);  // 记录错误日志
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 初始化行情解析器
	//////////////////////////////////////////////////////////////////////////
	WTSVariant* cfgParser = config->get("parsers");  // 获取parsers配置节点
	if (cfgParser)
	{
		if (cfgParser->type() == WTSVariant::VT_String)  // 如果是字符串类型（配置文件路径）
		{
			const char* filename = cfgParser->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果配置文件存在
			{
				WTSLogger::info("Reading parser config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 加载解析器配置文件
				if (var)  // 如果加载成功
				{
					initParsers(var->get("parsers"));  // 初始化解析器
					var->release();  // 释放配置对象
				}
				else  // 如果加载失败
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
			initParsers(cfgParser);  // 初始化解析器
		}
	}
	else  // 如果没有解析器配置
		WTSLogger::log_raw(LL_WARN, "No parsers config, skipped loading parsers");  // 记录警告日志

	config->release();  // 释放主配置对象
}

/**
 * @brief 初始化数据管理器
 * @param config 数据写入器配置
 * @param bAlldayMode 是否全天候模式，默认为false
 * 
 * 该函数初始化数据管理器，设置数据写入器和状态监控器。
 * 如果是全天候模式，不关联状态监控器。
 */
void WtDtRunner::initDataMgr(WTSVariant* config, bool bAlldayMode /* = false */)
{
	// 初始化数据管理器
	// 参数：配置、基础数据管理器、状态监控器（全天候模式为NULL）
	_data_mgr.init(config, &_bd_mgr, bAlldayMode ? NULL : &_state_mon);
}

/**
 * @brief 初始化行情解析器
 * @param cfg 解析器配置数组
 * 
 * 该函数遍历解析器配置数组，为每个激活的解析器创建ParserAdapter实例。
 * ParserAdapter负责管理解析器的生命周期，处理行情数据和事件。
 * 如果解析器ID为空，会自动生成唯一ID。
 */
void WtDtRunner::initParsers(WTSVariant* cfg)
{
	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历解析器配置数组
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取单个解析器配置
		if (!cfgItem->getBoolean("active"))  // 如果解析器未激活
			continue;  // 跳过该解析器

		const char* id = cfgItem->getCString("id");  // 获取解析器ID

		// By Wesley @ 2021.12.14
		// 如果id为空，则生成自动id
		std::string realid = id;  // 实际使用的ID
		if (realid.empty())  // 如果ID为空
		{
			static uint32_t auto_parserid = 1000;  // 自动ID计数器，从1000开始
			realid = StrUtil::printf("auto_parser_%u", auto_parserid++);  // 生成自动ID，格式为"auto_parser_1000"
		}
		
		// 创建ParserAdapter实例
		ParserAdapterPtr adapter(new ParserAdapter(&_bd_mgr, &_data_mgr, &_idx_factory));
		adapter->init(realid.c_str(), cfgItem);  // 初始化解析器适配器
		_parsers.addAdapter(realid.c_str(), adapter);  // 将适配器添加到管理器
	}

	WTSLogger::info("{} market data parsers loaded in total", _parsers.size());  // 记录日志，显示加载的解析器数量
}

#pragma region "Extended Parser"

/**
 * @brief 注册扩展Parser的回调函数
 * @param cbEvt 行情解析器事件回调函数
 * @param cbSub 行情订阅回调函数
 * 
 * 该函数保存扩展Parser的回调函数指针，用于处理解析器事件和订阅请求。
 */
void WtDtRunner::registerParserPorter(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub)
{
	_cb_parser_evt = cbEvt;  // 保存事件回调函数指针
	_cb_parser_sub = cbSub;  // 保存订阅回调函数指针

	WTSLogger::info("Callbacks of Extented Parser registration done");  // 记录日志
}

/**
 * @brief Parser初始化事件处理
 * @param id 解析器ID
 * 
 * 该函数处理Parser的初始化事件，如果注册了事件回调函数，则触发回调。
 */
void WtDtRunner::parser_init(const char* id)
{
	if (_cb_parser_evt)  // 如果注册了事件回调函数
		_cb_parser_evt(EVENT_PARSER_INIT, id);  // 触发初始化事件回调
}

/**
 * @brief Parser连接事件处理
 * @param id 解析器ID
 * 
 * 该函数处理Parser的连接事件，如果注册了事件回调函数，则触发回调。
 */
void WtDtRunner::parser_connect(const char* id)
{
	if (_cb_parser_evt)  // 如果注册了事件回调函数
		_cb_parser_evt(EVENT_PARSER_CONNECT, id);  // 触发连接事件回调
}

/**
 * @brief Parser断开连接事件处理
 * @param id 解析器ID
 * 
 * 该函数处理Parser的断开连接事件，如果注册了事件回调函数，则触发回调。
 */
void WtDtRunner::parser_disconnect(const char* id)
{
	if (_cb_parser_evt)  // 如果注册了事件回调函数
		_cb_parser_evt(EVENT_PARSER_DISCONNECT, id);  // 触发断开连接事件回调
}

/**
 * @brief Parser释放事件处理
 * @param id 解析器ID
 * 
 * 该函数处理Parser的释放事件，如果注册了事件回调函数，则触发回调。
 */
void WtDtRunner::parser_release(const char* id)
{
	if (_cb_parser_evt)  // 如果注册了事件回调函数
		_cb_parser_evt(EVENT_PARSER_RELEASE, id);  // 触发释放事件回调
}

/**
 * @brief Parser订阅处理
 * @param id 解析器ID
 * @param code 合约代码
 * 
 * 该函数处理Parser的订阅请求，如果注册了订阅回调函数，则触发回调。
 */
void WtDtRunner::parser_subscribe(const char* id, const char* code)
{
	if (_cb_parser_sub)  // 如果注册了订阅回调函数
		_cb_parser_sub(id, code, true);  // 触发订阅回调，第三个参数true表示订阅
}

/**
 * @brief Parser退订处理
 * @param id 解析器ID
 * @param code 合约代码
 * 
 * 该函数处理Parser的退订请求，如果注册了订阅回调函数，则触发回调。
 */
void WtDtRunner::parser_unsubscribe(const char* id, const char* code)
{
	if (_cb_parser_sub)  // 如果注册了订阅回调函数
		_cb_parser_sub(id, code, false);  // 触发退订回调，第三个参数false表示退订
}

/**
 * @brief 处理扩展Parser推送的行情数据
 * @param id 解析器ID
 * @param curTick Tick行情数据指针
 * @param uProcFlag 处理标记
 * 
 * 该函数接收扩展Parser推送的行情数据，转发给对应的ParserAdapter进行处理。
 * 如果找不到对应的解析器，记录警告日志。
 */
void WtDtRunner::on_ext_parser_quote(const char* id, WTSTickStruct* curTick, uint32_t uProcFlag)
{
	ParserAdapterPtr adapter = _parsers.getAdapter(id);  // 根据ID获取解析器适配器
	if (adapter)  // 如果找到了对应的适配器
	{
		WTSTickData* newTick = WTSTickData::create(*curTick);  // 创建WTSTickData对象
		adapter->handleQuote(newTick, uProcFlag);  // 处理行情数据
		newTick->release();  // 释放Tick数据对象
	}
	else  // 如果未找到对应的适配器
	{
		WTSLogger::warn("Parser {} not exists", id);  // 记录警告日志
	}
}

/**
 * @brief 创建扩展行情解析器
 * @param id 解析器唯一标识符
 * @return bool 创建成功返回true
 * 
 * 该函数创建一个扩展行情解析器实例。
 * 扩展解析器用于接入自定义的行情数据源，将外部行情推送到系统中。
 */
bool WtDtRunner::createExtParser(const char* id)
{
	// 创建ParserAdapter实例
	ParserAdapterPtr adapter(new ParserAdapter(&_bd_mgr, &_data_mgr, &_idx_factory));
	
	// 创建ExpParser实例
	ExpParser* parser = new ExpParser(id);
	
	// 初始化扩展解析器，将ExpParser与ParserAdapter关联
	adapter->initExt(id, parser);
	
	// 将适配器添加到管理器
	_parsers.addAdapter(id, adapter);
	
	WTSLogger::info("Extended parser {} created", id);  // 记录日志
	return true;  // 返回成功
}

#pragma endregion 

/**
 * @brief 创建扩展数据转储器
 * @param id 转储器唯一标识符
 * @return bool 创建成功返回true
 * 
 * 该函数创建一个扩展数据转储器实例。
 * 扩展转储器用于将历史数据导出到自定义存储系统。
 */
bool WtDtRunner::createExtDumper(const char* id)
{
	// 创建ExpDumper实例，使用智能指针管理
	ExpDumperPtr dumper(new ExpDumper(id));
	
	// 将转储器添加到映射表
	_dumpers[id] = dumper;

	// 将转储器注册到数据管理器
	_data_mgr.add_ext_dumper(id, dumper.get());

	WTSLogger::info("Extended dumper {} created", id);  // 记录日志
	return true;  // 返回成功
}

/**
 * @brief 注册扩展Dumper的回调函数（K线和Tick）
 * @param barDumper K线数据转储回调函数
 * @param tickDumper Tick数据转储回调函数
 * 
 * 该函数保存扩展Dumper的基础数据转储回调函数指针。
 */
void WtDtRunner::registerExtDumper(FuncDumpBars barDumper, FuncDumpTicks tickDumper)
{
	_dumper_for_bars = barDumper;   // 保存K线转储回调函数指针
	_dumper_for_ticks = tickDumper;  // 保存Tick转储回调函数指针
}

/**
 * @brief 注册扩展Dumper的回调函数（高频数据）
 * @param ordQueDumper 委托队列数据转储回调函数
 * @param ordDtlDumper 委托明细数据转储回调函数
 * @param transDumper 逐笔成交数据转储回调函数
 * 
 * 该函数保存扩展Dumper的高频数据转储回调函数指针。
 */
void WtDtRunner::registerExtHftDataDumper(FuncDumpOrdQue ordQueDumper, FuncDumpOrdDtl ordDtlDumper, FuncDumpTrans transDumper)
{
	_dumper_for_ordque = ordQueDumper;  // 保存委托队列转储回调函数指针
	_dumper_for_orddtl = ordDtlDumper;  // 保存委托明细转储回调函数指针
	_dumper_for_trans = transDumper;    // 保存逐笔成交转储回调函数指针
}

/**
 * @brief 转储历史Tick数据
 * @param id 转储器ID
 * @param stdCode 标准合约代码
 * @param uDate 交易日期
 * @param ticks Tick数据数组指针
 * @param count Tick数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将历史Tick数据转储到外部存储系统。
 * 如果未注册Tick转储回调函数，返回false。
 */
bool WtDtRunner::dumpHisTicks(const char* id, const char* stdCode, uint32_t uDate, WTSTickStruct* ticks, uint32_t count)
{
	if (NULL == _dumper_for_ticks)  // 如果未注册Tick转储回调函数
	{
		WTSLogger::error("Extended tick dumper not enabled");  // 记录错误日志
		return false;  // 返回失败
	}

	// 调用Tick转储回调函数，执行实际的转储操作
	return _dumper_for_ticks(id, stdCode, uDate, ticks, count);
}

/**
 * @brief 转储历史K线数据
 * @param id 转储器ID
 * @param stdCode 标准合约代码
 * @param period K线周期
 * @param bars K线数据数组指针
 * @param count K线数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将历史K线数据转储到外部存储系统。
 * 如果未注册K线转储回调函数，返回false。
 */
bool WtDtRunner::dumpHisBars(const char* id, const char* stdCode, const char* period, WTSBarStruct* bars, uint32_t count)
{
	if (NULL == _dumper_for_bars)  // 如果未注册K线转储回调函数
	{
		WTSLogger::error("Extended bar dumper not enabled");  // 记录错误日志
		return false;  // 返回失败
	}

	// 调用K线转储回调函数，执行实际的转储操作
	return _dumper_for_bars(id, stdCode, period, bars, count);
}

/**
 * @brief 转储历史委托明细数据
 * @param id 转储器ID
 * @param stdCode 标准合约代码
 * @param uDate 交易日期
 * @param items 委托明细数据数组指针
 * @param count 委托明细数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将历史委托明细数据转储到外部存储系统。
 * 如果未注册委托明细转储回调函数，返回false。
 */
bool WtDtRunner::dumpHisOrdDtl(const char* id, const char* stdCode, uint32_t uDate, WTSOrdDtlStruct* items, uint32_t count)
{
	if (NULL == _dumper_for_orddtl)  // 如果未注册委托明细转储回调函数
	{
		WTSLogger::error("Extended order detail dumper not enabled");  // 记录错误日志
		return false;  // 返回失败
	}

	// 调用委托明细转储回调函数，执行实际的转储操作
	return _dumper_for_orddtl(id, stdCode, uDate, items, count);
}

/**
 * @brief 转储历史委托队列数据
 * @param id 转储器ID
 * @param stdCode 标准合约代码
 * @param uDate 交易日期
 * @param items 委托队列数据数组指针
 * @param count 委托队列数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将历史委托队列数据转储到外部存储系统。
 * 如果未注册委托队列转储回调函数，返回false。
 */
bool WtDtRunner::dumpHisOrdQue(const char* id, const char* stdCode, uint32_t uDate, WTSOrdQueStruct* items, uint32_t count)
{
	if (NULL == _dumper_for_ordque)  // 如果未注册委托队列转储回调函数
	{
		WTSLogger::error("Extended order queue dumper not enabled");  // 记录错误日志
		return false;  // 返回失败
	}

	// 调用委托队列转储回调函数，执行实际的转储操作
	return _dumper_for_ordque(id, stdCode, uDate, items, count);
}

/**
 * @brief 转储历史逐笔成交数据
 * @param id 转储器ID
 * @param stdCode 标准合约代码
 * @param uDate 交易日期
 * @param items 逐笔成交数据数组指针
 * @param count 逐笔成交数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将历史逐笔成交数据转储到外部存储系统。
 * 如果未注册逐笔成交转储回调函数，返回false。
 */
bool WtDtRunner::dumpHisTrans(const char* id, const char* stdCode, uint32_t uDate, WTSTransStruct* items, uint32_t count)
{
	if (NULL == _dumper_for_trans)  // 如果未注册逐笔成交转储回调函数
	{
		WTSLogger::error("Extended transaction dumper not enabled");  // 记录错误日志
		return false;  // 返回失败
	}

	// 调用逐笔成交转储回调函数，执行实际的转储操作
	return _dumper_for_trans(id, stdCode, uDate, items, count);
}
