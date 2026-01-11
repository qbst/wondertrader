/*!
 * \file WtUftRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader UFT策略运行器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtUftRunner类，是WonderTrader UFT（Ultra Fast Trading，极速交易）策略运行器的主类。
 * 该运行器负责协调和管理UFT策略系统的各个组件，包括引擎、数据管理器、交易适配器、解析器适配器等。
 * 
 * 主要功能：
 * 1. 系统初始化：初始化日志系统、设置工作目录、安装信号钩子
 * 2. 配置加载：加载并解析配置文件，初始化各个组件
 * 3. 组件管理：管理交易适配器、解析器适配器、数据管理器、策略管理器、事件通知器等
 * 4. 策略管理：加载UFT策略工厂，创建并初始化UFT策略实例
 * 5. 共享内存管理：初始化共享内存域，支持跨进程参数读写和监控
 * 6. 运行控制：启动引擎、解析器、交易适配器，并监控运行状态
 * 7. 日志处理：实现ILogHandler接口，处理日志输出
 * 
 * 组件架构：
 * - WtUftEngine：UFT策略引擎，负责策略上下文管理和数据分发
 * - WtUftDtMgr：UFT数据管理器，负责实时和历史数据管理
 * - TraderAdapterMgr：交易适配器管理器，管理多个交易通道
 * - ParserAdapterMgr：解析器适配器管理器，管理多个行情解析通道
 * - UftStrategyMgr：UFT策略管理器，负责策略工厂和策略实例管理
 * - WTSBaseDataMgr：基础数据管理器，管理合约、商品、交易时段等基础信息
 * - EventNotifier：事件通知器，负责事件通知和回调
 * - ActionPolicyMgr：动作策略管理器，管理交易动作的执行策略
 * 
 * 设计特点：
 * - 模块化设计：各组件职责清晰，便于维护和扩展
 * - 配置驱动：通过配置文件灵活配置各个组件
 * - 动态加载：支持动态加载策略工厂和适配器
 * - 共享内存：支持共享内存域，实现跨进程参数监控
 * - 信号处理：安装信号钩子，捕获异常和错误
 */
#pragma once
#include <string>  // 标准字符串类
#include <unordered_map>  // 无序映射容器

#include "../Includes/ILogHandler.h"  // 日志处理器接口，提供日志处理功能

#include "../WtUftCore/EventNotifier.h"  // 事件通知器，提供事件通知和回调功能
#include "../WtUftCore/UftStrategyMgr.h"  // UFT策略管理器，负责策略工厂和策略实例管理

#include "../WtUftCore/WtUftEngine.h"  // UFT策略引擎，负责策略上下文管理和数据分发
#include "../WtUftCore/TraderAdapter.h"  // 交易适配器，提供交易通道适配功能
#include "../WtUftCore/ParserAdapter.h"  // 解析器适配器，提供行情解析通道适配功能
#include "../WtUftCore/WtUftDtMgr.h"  // UFT数据管理器，负责实时和历史数据管理
#include "../WtUftCore/ActionPolicyMgr.h"  // 动作策略管理器，管理交易动作的执行策略

#include "../WTSTools/WTSHotMgr.h"  // 热点合约管理器，管理热点合约信息
#include "../WTSTools/WTSBaseDataMgr.h"  // 基础数据管理器，管理合约、商品、交易时段等基础信息

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSVariant;  // 前置声明：配置变体类，用于配置数据存储和解析
NS_WTP_END  // WonderTrader命名空间结束

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class WtUftRunner
 * @brief UFT策略运行器类
 * 
 * WonderTrader UFT策略运行器的主类，负责协调和管理UFT策略系统的各个组件。
 * 实现ILogHandler接口，处理日志输出。
 * 
 * 主要职责：
 * - 初始化系统环境（日志、工作目录、信号钩子）
 * - 加载配置文件并初始化各个组件
 * - 管理交易适配器、解析器适配器、数据管理器等组件
 * - 加载和初始化UFT策略
 * - 启动引擎、解析器、交易适配器并监控运行状态
 * - 处理日志输出
 * 
 * 使用流程：
 * 1. 创建WtUftRunner实例
 * 2. 调用init()初始化日志系统
 * 3. 调用config()加载配置文件并初始化组件
 * 4. 调用run()启动运行
 */
class WtUftRunner : public ILogHandler  // 继承自日志处理器接口
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建UFT策略运行器实例，初始化成员变量，安装信号钩子。
	 */
	WtUftRunner();
	
	/**
	 * @brief 析构函数
	 * 
	 * 销毁UFT策略运行器实例，清理资源。
	 */
	~WtUftRunner();

public:
	/**
	 * @brief 初始化运行器
	 * @param filename 日志配置文件路径
	 * 
	 * 初始化日志系统，设置工作目录。
	 * 该函数应在创建实例后立即调用。
	 */
	void init(const std::string& filename);

	/**
	 * @brief 配置运行器
	 * @param filename 主配置文件路径
	 * @return 配置成功返回true，失败返回false
	 * 
	 * 加载主配置文件，初始化各个组件：
	 * - 加载基础数据（交易时段、商品、合约、节假日）
	 * - 初始化引擎
	 * - 初始化数据管理器
	 * - 初始化共享内存域（如果配置）
	 * - 初始化动作策略管理器
	 * - 初始化解析器适配器
	 * - 初始化交易适配器
	 * - 初始化UFT策略
	 */
	bool config(const std::string& filename);

	/**
	 * @brief 运行运行器
	 * @param bAsync 是否异步运行（默认false，当前版本未使用）
	 * 
	 * 启动引擎、解析器、交易适配器，并进入主循环监控运行状态。
	 * 如果配置了共享内存域，会启动参数监控。
	 * 主循环会一直运行直到_to_exit标志为true。
	 */
	void run(bool bAsync = false);

private:
	/**
	 * @brief 初始化交易适配器
	 * @param cfgTrader 交易适配器配置节点
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 根据配置创建并初始化交易适配器实例，添加到交易适配器管理器。
	 * 配置可以是数组形式，每个元素包含一个交易适配器的配置。
	 */
	bool initTraders(WTSVariant* cfgTrader);
	
	/**
	 * @brief 初始化解析器适配器
	 * @param cfgParser 解析器适配器配置节点
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 根据配置创建并初始化解析器适配器实例，添加到解析器适配器管理器。
	 * 配置可以是数组形式，每个元素包含一个解析器适配器的配置。
	 * 如果配置项没有指定id，会自动生成一个唯一的id。
	 */
	bool initParsers(WTSVariant* cfgParser);
	
	/**
	 * @brief 初始化数据管理器
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中读取数据管理器配置，初始化UFT数据管理器。
	 */
	bool initDataMgr();
	
	/**
	 * @brief 初始化UFT策略
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中读取UFT策略配置，加载策略工厂，创建策略实例，创建策略上下文，
	 * 绑定交易适配器，并将策略上下文添加到引擎。
	 */
	bool initUftStrategies();
	
	/**
	 * @brief 初始化事件通知器
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中读取事件通知器配置，初始化事件通知器。
	 */
	bool initEvtNotifier();
	
	/**
	 * @brief 初始化引擎
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中读取环境配置，初始化UFT引擎，设置交易适配器管理器。
	 */
	bool initEngine();

//////////////////////////////////////////////////////////////////////////
//ILogHandler
public:
	/**
	 * @brief 处理日志追加
	 * @param ll 日志级别
	 * @param msg 日志消息
	 * 
	 * 实现ILogHandler接口，处理日志输出。
	 * 当前实现为空，日志由WTSLogger统一处理。
	 */
	virtual void handleLogAppend(WTSLogLevel ll, const char* msg) override;

private:
	WTSVariant*			_config;  // 配置对象指针，存储加载的配置文件内容
	TraderAdapterMgr	_traders;  // 交易适配器管理器，管理多个交易通道
	ParserAdapterMgr	_parsers;  // 解析器适配器管理器，管理多个行情解析通道

	WtUftEngine			_uft_engine;  // UFT策略引擎，负责策略上下文管理和数据分发

	WtUftDtMgr			_data_mgr;  // UFT数据管理器，负责实时和历史数据管理

	WTSBaseDataMgr		_bd_mgr;  // 基础数据管理器，管理合约、商品、交易时段等基础信息
	EventNotifier		_notifier;  // 事件通知器，负责事件通知和回调

	UftStrategyMgr		_uft_stra_mgr;  // UFT策略管理器，负责策略工厂和策略实例管理

	ActionPolicyMgr		_act_policy;  // 动作策略管理器，管理交易动作的执行策略

	bool				_to_exit;  // 退出标志，用于控制主循环退出
};

