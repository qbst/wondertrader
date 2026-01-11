/*!
 * \file WtRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader策略运行器类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtRunner类，是WonderTrader框架的核心运行器，负责统一管理和协调各个组件的生命周期。
 * 该类作为WonderTrader系统的入口点和总控制器，负责初始化、配置和运行整个交易系统。
 * 
 * 主要功能：
 * 1. 系统初始化：初始化日志系统、设置安装目录等基础环境
 * 2. 配置加载：从配置文件加载并初始化各组件（基础数据、数据管理器、开平策略、行情通道、交易通道、执行器、策略等）
 * 3. 引擎管理：根据配置选择并初始化对应的交易引擎（CTA引擎、HFT引擎、选股引擎）
 * 4. 策略管理：加载和管理CTA策略、HFT策略、选股策略
 * 5. 组件协调：协调交易适配器、解析器适配器、执行器、数据管理器等组件的协同工作
 * 6. 日志处理：实现ILogHandler接口，处理系统日志的转发和分发
 * 7. 运行控制：启动各组件运行，支持同步和异步运行模式
 * 
 * 设计特点：
 * - 统一入口：作为WonderTrader系统的主入口，统一管理所有组件
 * - 配置驱动：通过配置文件灵活配置各组件，支持多种运行模式
 * - 引擎选择：根据配置自动选择CTA、HFT或选股引擎
 * - 组件化设计：各组件独立初始化和管理，便于扩展和维护
 * - 生命周期管理：统一管理各组件的创建、运行和销毁
 * 
 * 组件架构：
 * - TraderAdapterMgr: 交易适配器管理器，管理多个交易通道
 * - ParserAdapterMgr: 解析器适配器管理器，管理多个行情通道
 * - WtExecuterFactory: 执行器工厂，创建和管理执行器实例
 * - WtCtaEngine/WtHftEngine/WtSelEngine: 交易引擎，根据配置选择使用
 * - WtDtMgr: 数据管理器，管理行情数据和K线数据
 * - WTSBaseDataMgr: 基础数据管理器，管理商品、合约、交易时段等基础数据
 * - WTSHotMgr: 热点合约管理器，管理主力合约、次主力合约等
 * - EventNotifier: 事件通知器，处理系统事件通知
 * - CtaStrategyMgr/HftStrategyMgr/SelStrategyMgr: 策略管理器，管理策略工厂和策略实例
 * - ActionPolicyMgr: 开平策略管理器，管理开仓和平仓策略
 */

#pragma once  // 防止头文件被重复包含

#include <string>  // 包含字符串类型定义
#include <unordered_map>  // 包含无序映射容器，用于存储键值对

#include "../Includes/ILogHandler.h"  // 包含日志处理器接口，提供ILogHandler接口

#include "../WtCore/EventNotifier.h"  // 包含事件通知器头文件，使用EventNotifier类
#include "../WtCore/CtaStrategyMgr.h"  // 包含CTA策略管理器头文件，使用CtaStrategyMgr类
#include "../WtCore/HftStrategyMgr.h"  // 包含HFT策略管理器头文件，使用HftStrategyMgr类
#include "../WtCore/SelStrategyMgr.h"  // 包含选股策略管理器头文件，使用SelStrategyMgr类

#include "../WtCore/WtCtaEngine.h"  // 包含CTA引擎头文件，使用WtCtaEngine类
#include "../WtCore/WtHftEngine.h"  // 包含HFT引擎头文件，使用WtHftEngine类
#include "../WtCore/WtSelEngine.h"  // 包含选股引擎头文件，使用WtSelEngine类
#include "../WtCore/WtLocalExecuter.h"  // 包含本地执行器头文件，使用WtLocalExecuter类
#include "../WtCore/WtDistExecuter.h"  // 包含分布式执行器头文件，使用WtDistExecuter类
#include "../WtCore/TraderAdapter.h"  // 包含交易适配器头文件，使用TraderAdapterMgr类
#include "../WtCore/ParserAdapter.h"  // 包含解析器适配器头文件，使用ParserAdapterMgr类
#include "../WtCore/WtDtMgr.h"  // 包含数据管理器头文件，使用WtDtMgr类
#include "../WtCore/ActionPolicyMgr.h"  // 包含开平策略管理器头文件，使用ActionPolicyMgr类

#include "../WTSTools/WTSHotMgr.h"  // 包含热点合约管理器头文件，使用WTSHotMgr类
#include "../WTSTools/WTSBaseDataMgr.h"  // 包含基础数据管理器头文件，使用WTSBaseDataMgr类

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：配置变体类
class WtDataStorage;  // 前向声明：数据存储类
NS_WTP_END  // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WonderTrader策略运行器类
 * 
 * 该类是WonderTrader框架的核心运行器，负责统一管理和协调各个组件的生命周期。
 * 作为WonderTrader系统的主入口，负责初始化、配置和运行整个交易系统。
 * 
 * 核心职责：
 * 1. 系统初始化：初始化日志系统和基础环境
 * 2. 配置加载：从配置文件加载并初始化各组件
 * 3. 引擎管理：根据配置选择并初始化对应的交易引擎
 * 4. 策略管理：加载和管理各种策略（CTA、HFT、选股）
 * 5. 组件协调：协调各组件协同工作
 * 6. 日志处理：实现ILogHandler接口，处理系统日志
 * 7. 运行控制：启动各组件运行
 * 
 * 使用流程：
 * 1. 创建WtRunner实例
 * 2. 调用init()初始化日志系统
 * 3. 调用config()加载配置文件并初始化各组件
 * 4. 调用run()启动运行（支持同步和异步模式）
 */
class WtRunner : public ILogHandler
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建策略运行器实例，初始化成员变量。
	 * 在构造函数中安装信号钩子，用于捕获异常和错误。
	 */
	WtRunner();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理策略运行器占用的资源。
	 */
	~WtRunner();

public:
	/**
	 * @brief 初始化策略运行器
	 * 
	 * 初始化日志系统和运行环境。
	 * 
	 * @param filename 日志配置文件路径
	 * 
	 * 初始化流程：
	 * 1. 初始化日志系统（从文件加载配置）
	 * 2. 设置安装目录路径
	 * 3. 检查日志配置文件是否存在（如果不存在则记录警告）
	 */
	void init(const std::string& filename);

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
	 * 2. 加载基础数据文件（交易时段、商品、合约、节假日、热点合约等）
	 * 3. 初始化交易引擎（根据配置选择CTA、HFT或选股引擎）
	 * 4. 初始化数据管理器
	 * 5. 初始化开平策略
	 * 6. 初始化行情通道（解析器）
	 * 7. 初始化交易通道（交易适配器）
	 * 8. 初始化事件通知器
	 * 9. 如果不是高频引擎，初始化执行器
	 * 10. 初始化策略（CTA策略或HFT策略）
	 */
	bool config(const std::string& filename);

	/**
	 * @brief 运行策略运行器
	 * 
	 * 启动各组件运行。
	 * 
	 * @param bAsync 是否异步运行，true表示异步运行（不阻塞），false表示同步运行（阻塞）
	 * 
	 * 运行流程：
	 * 1. 启动解析器适配器管理器（行情通道）
	 * 2. 启动交易适配器管理器（交易通道）
	 * 3. 启动交易引擎
	 * 4. 如果同步运行，等待退出信号；如果异步运行，直接返回
	 */
	void run(bool bAsync = false);

private:
	/**
	 * @brief 初始化交易通道
	 * 
	 * 从配置加载并初始化交易适配器。
	 * 
	 * @param cfgTrader 交易通道配置对象
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initTraders(WTSVariant* cfgTrader);
	
	/**
	 * @brief 初始化行情通道
	 * 
	 * 从配置加载并初始化解析器适配器。
	 * 
	 * @param cfgParser 行情通道配置对象
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initParsers(WTSVariant* cfgParser);
	
	/**
	 * @brief 初始化执行器
	 * 
	 * 从配置加载并初始化执行器（本地执行器、差分执行器、分布式执行器等）。
	 * 
	 * @param cfgExecuter 执行器配置对象
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initExecuters(WTSVariant* cfgExecuter);
	
	/**
	 * @brief 初始化数据管理器
	 * 
	 * 从配置初始化数据管理器。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initDataMgr();
	
	/**
	 * @brief 初始化事件通知器
	 * 
	 * 从配置初始化事件通知器。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initEvtNotifier();
	
	/**
	 * @brief 初始化CTA策略
	 * 
	 * 从配置加载并初始化CTA策略。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initCtaStrategies();
	
	/**
	 * @brief 初始化HFT策略
	 * 
	 * 从配置加载并初始化HFT策略。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initHftStrategies();
	
	/**
	 * @brief 初始化开平策略
	 * 
	 * 从配置文件加载开平策略。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initActionPolicy();

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
	 */
	bool initEngine();

//////////////////////////////////////////////////////////////////////////
//ILogHandler接口实现
//////////////////////////////////////////////////////////////////////////
public:
	/**
	 * @brief 处理日志追加事件（ILogHandler接口实现）
	 * 
	 * 当系统产生新的日志时调用，将日志转发给事件通知器。
	 * 
	 * @param ll 日志级别（WTSLogLevel枚举值）
	 * @param msg 日志消息内容
	 * 
	 * 处理流程：
	 * 1. 将日志级别转换为日志标签字符串
	 * 2. 通过事件通知器通知日志事件
	 */
	virtual void handleLogAppend(WTSLogLevel ll, const char* msg) override;

private:
	WTSVariant*			_config;  // 配置对象指针，存储加载的配置信息
	TraderAdapterMgr	_traders;  // 交易适配器管理器，管理多个交易通道
	ParserAdapterMgr	_parsers;  // 解析器适配器管理器，管理多个行情通道
	WtExecuterFactory	_exe_factory;  // 执行器工厂，创建和管理执行器实例

	WtCtaEngine			_cta_engine;  // CTA引擎实例，用于CTA策略运行
	WtHftEngine			_hft_engine;  // HFT引擎实例，用于HFT策略运行
	WtSelEngine			_sel_engine;  // 选股引擎实例，用于选股策略运行
	WtEngine*			_engine;  // 当前使用的引擎指针，指向_cta_engine、_hft_engine或_sel_engine之一

	WtDataStorage*		_data_store;  // 数据存储对象指针，用于数据持久化（当前未使用）

	WtDtMgr				_data_mgr;  // 数据管理器，管理行情数据和K线数据

	WTSBaseDataMgr		_bd_mgr;  // 基础数据管理器，管理商品、合约、交易时段等基础数据
	WTSHotMgr			_hot_mgr;  // 热点合约管理器，管理主力合约、次主力合约等
	EventNotifier		_notifier;  // 事件通知器，处理系统事件通知

	CtaStrategyMgr		_cta_stra_mgr;  // CTA策略管理器，管理CTA策略工厂和策略实例
	HftStrategyMgr		_hft_stra_mgr;  // HFT策略管理器，管理HFT策略工厂和策略实例
	SelStrategyMgr		_sel_stra_mgr;  // 选股策略管理器，管理选股策略工厂和策略实例
	ActionPolicyMgr		_act_policy;  // 开平策略管理器，管理开仓和平仓策略

	bool				_is_hft;  // 是否为高频引擎标志，true表示使用HFT引擎，false表示使用CTA或选股引擎
	bool				_is_sel;  // 是否为选股引擎标志，true表示使用选股引擎，false表示使用CTA或HFT引擎

	bool				_to_exit;  // 退出标志，true表示需要退出，false表示继续运行
};

