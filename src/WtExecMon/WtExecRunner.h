/*!
 * \file WtExecRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader执行器运行器类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtExecRunner类，是WonderTrader执行器模块的核心运行器。
 * 该类负责管理执行器、交易通道、行情通道等组件的生命周期，协调各组件协同工作。
 * 
 * 主要功能：
 * 1. 组件管理：管理交易适配器、解析器适配器、执行器工厂、执行器管理器等组件
 * 2. 初始化配置：从配置文件加载并初始化各组件
 * 3. 行情处理：接收并处理实时行情数据，转发给执行器
 * 4. 执行器存根：实现IExecuterStub接口，为执行器提供基础信息查询功能
 * 5. 解析器存根：实现IParserStub接口，接收解析器推送的行情数据
 * 6. 仓位管理：管理目标仓位，支持设置和提交执行
 * 7. 数据管理：管理基础数据（商品、合约、交易时段等）和热点合约数据
 * 
 * 设计特点：
 * - 双重接口实现：同时实现IParserStub和IExecuterStub接口，作为数据接收者和信息提供者
 * - 组件化设计：通过管理器类统一管理多个组件实例
 * - 配置驱动：通过配置文件灵活配置各组件
 * - 生命周期管理：统一管理各组件的创建、运行和销毁
 * 
 * 组件架构：
 * - TraderAdapterMgr: 交易适配器管理器，管理多个交易通道
 * - ParserAdapterMgr: 解析器适配器管理器，管理多个行情通道
 * - WtExecuterFactory: 执行器工厂，创建和管理执行器实例
 * - WtExecuterMgr: 执行器管理器，管理执行器的运行
 * - WtSimpDataMgr: 简单数据管理器，管理行情数据和K线数据
 * - WTSBaseDataMgr: 基础数据管理器，管理商品、合约、交易时段等基础数据
 * - WTSHotMgr: 热点合约管理器，管理主力合约、次主力合约等
 * - ActionPolicyMgr: 开平策略管理器，管理开仓和平仓策略
 */

#pragma once  // 防止头文件被重复包含

#include "WtSimpDataMgr.h"  // 包含简单数据管理器头文件，使用WtSimpDataMgr类

#include "../WtCore/WtExecMgr.h"  // 包含执行器管理器头文件，使用WtExecuterMgr和WtExecuterFactory类
#include "../WtCore/TraderAdapter.h"  // 包含交易适配器头文件，使用TraderAdapterMgr类
#include "../WtCore/ParserAdapter.h"  // 包含解析器适配器头文件，使用ParserAdapterMgr和IParserStub接口
#include "../WtCore/ActionPolicyMgr.h"  // 包含开平策略管理器头文件，使用ActionPolicyMgr类

#include "../WTSTools/WTSHotMgr.h"  // 包含热点合约管理器头文件，使用WTSHotMgr类
#include "../WTSTools/WTSBaseDataMgr.h"  // 包含基础数据管理器头文件，使用WTSBaseDataMgr类

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：配置变体类
NS_WTP_END  // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WonderTrader执行器运行器类
 * 
 * 该类是WonderTrader执行器模块的核心运行器，负责管理执行器、交易通道、行情通道等组件。
 * 同时实现IParserStub和IExecuterStub接口，作为数据接收者和信息提供者。
 * 
 * 核心职责：
 * 1. 组件生命周期管理：初始化、配置、运行、释放各组件
 * 2. 行情数据处理：接收解析器推送的行情数据，转发给执行器
 * 3. 执行器支持：为执行器提供基础信息查询功能（时间、商品信息、交易会话等）
 * 4. 仓位管理：管理目标仓位，支持设置和提交执行
 * 5. 数据管理：管理基础数据和热点合约数据
 * 
 * 使用流程：
 * 1. 创建WtExecRunner实例
 * 2. 调用init()初始化日志系统
 * 3. 调用config()加载配置文件并初始化各组件
 * 4. 调用run()启动运行（会阻塞当前线程）
 * 5. 调用release()释放资源
 */
class WtExecRunner : public IParserStub, public IExecuterStub
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建执行器运行器实例，初始化成员变量。
	 * 在构造函数中安装信号钩子，用于捕获异常和错误。
	 */
	WtExecRunner();

	/**
	 * @brief 初始化执行器运行器
	 * 
	 * 初始化日志系统和运行环境。
	 * 
	 * @param logCfg 日志配置文件路径或配置内容，默认为"logcfgexec.json"
	 * @param isFile 是否为文件路径，true表示logCfg是文件路径，false表示logCfg是配置内容
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 初始化日志系统（从文件或字符串加载配置）
	 * 2. 设置安装目录路径
	 * 3. 在Windows平台启用MiniDump功能（用于崩溃转储）
	 */
	bool init(const char* logCfg = "logcfgexec.json", bool isFile = true);

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
	 * 1. 加载配置文件（从文件或字符串）
	 * 2. 加载基础数据文件（交易时段、商品、合约、节假日等）
	 * 3. 初始化数据管理器
	 * 4. 初始化开平策略
	 * 5. 初始化行情通道（解析器）
	 * 6. 初始化交易通道（交易适配器）
	 * 7. 初始化执行器
	 */
	bool config(const char* cfgFile, bool isFile = true);

	/**
	 * @brief 运行执行器运行器
	 * 
	 * 启动行情通道和交易通道的运行。
	 * 该函数会阻塞当前线程，直到模块停止运行。
	 * 
	 * 运行流程：
	 * 1. 启动解析器适配器管理器（行情通道）
	 * 2. 启动交易适配器管理器（交易通道）
	 * 3. 执行器根据行情数据和目标仓位执行交易逻辑
	 * 
	 * 注意事项：
	 * - 该函数会阻塞当前线程，建议在独立线程中调用
	 * - 使用try-catch捕获异常，确保程序稳定运行
	 */
	void run();

	/**
	 * @brief 释放执行器运行器资源
	 * 
	 * 清理执行器运行器占用的资源，停止日志系统。
	 * 调用此函数后，模块将无法继续使用，需要重新初始化。
	 */
	void release();

	/**
	 * @brief 设置目标仓位
	 * 
	 * 设置指定合约的目标持仓数量。
	 * 目标仓位会被缓存，直到调用commitPositions()提交执行。
	 * 
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"、"CFFEX.IF2303"等
	 * @param targetPos 目标持仓数量，正数表示多头，负数表示空头，0表示平仓
	 */
	void setPosition(const char* stdCode, double targetPos);

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
	void commitPositions();

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
	bool addExeFactories(const char* folder);

	/**
	 * @brief 获取基础数据管理器
	 * 
	 * 返回基础数据管理器的指针，用于访问商品、合约、交易时段等基础数据。
	 * 
	 * @return 返回基础数据管理器指针
	 */
	IBaseDataMgr*	get_bd_mgr() { return &_bd_mgr; }

	/**
	 * @brief 获取热点合约管理器
	 * 
	 * 返回热点合约管理器的指针，用于查询主力合约、次主力合约等。
	 * 
	 * @return 返回热点合约管理器指针
	 */
	IHotMgr* get_hot_mgr() { return &_hot_mgr; }

	/**
	 * @brief 获取交易会话信息
	 * 
	 * 根据会话ID或合约代码获取交易会话信息。
	 * 
	 * @param sid 会话ID或合约代码
	 * @param isCode 是否为合约代码，true表示sid是合约代码，false表示sid是会话ID
	 * @return 返回交易会话信息指针，未找到返回NULL
	 */
	WTSSessionInfo* get_session_info(const char* sid, bool isCode = true);

	//////////////////////////////////////////////////////////////////////////
	/// <summary>
	/// 处理实时主推行情（IParserStub接口实现）
	/// </summary>
	/// <param name="curTick">最新的tick数据</param>
	/// 
	/// 当解析器收到新的行情数据时，通过此方法推送给执行器运行器。
	/// 执行器运行器会更新时间和数据管理器，然后将行情数据转发给执行器管理器。
	/// 
	/// 处理流程：
	/// 1. 更新全局时间（日期、分钟、秒）
	/// 2. 更新交易日
	/// 3. 更新数据管理器的行情数据
	/// 4. 将行情数据转发给执行器管理器处理
	//////////////////////////////////////////////////////////////////////////
	virtual void handle_push_quote(WTSTickData* curTick) override;

	///////////////////////////////////////////////////////////////////////////
	//IExecuterStub 接口实现
	///////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 获取实时时间（IExecuterStub接口实现）
	 * 
	 * 返回当前实时时间戳（纳秒级）。
	 * 
	 * @return 返回当前实时时间戳（纳秒级）
	 * 
	 * 时间计算：
	 * - 从数据管理器获取当前日期和原始时间
	 * - 组合日期、分钟、秒生成完整时间戳
	 */
	virtual uint64_t get_real_time() override;
	
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
	virtual WTSCommodityInfo* get_comm_info(const char* stdCode) override;
	
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
	virtual WTSSessionInfo* get_sess_info(const char* stdCode) override;
	
	/**
	 * @brief 获取热点合约管理器（IExecuterStub接口实现）
	 * 
	 * 返回热点合约管理器的指针。
	 * 
	 * @return 返回热点合约管理器指针
	 */
	virtual IHotMgr* get_hot_mon() override { return &_hot_mgr; }
	
	/**
	 * @brief 获取交易日期（IExecuterStub接口实现）
	 * 
	 * 返回当前交易日期。
	 * 
	 * @return 返回当前交易日期（格式：YYYYMMDD）
	 */
	virtual uint32_t get_trading_day() override;

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
	 * @brief 初始化开平策略
	 * 
	 * 从配置文件加载开平策略。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 */
	bool initActionPolicy();

private:
	TraderAdapterMgr	_traders;  // 交易适配器管理器，管理多个交易通道
	ParserAdapterMgr	_parsers;  // 解析器适配器管理器，管理多个行情通道
	WtExecuterFactory	_exe_factory;  // 执行器工厂，创建和管理执行器实例
	WtExecuterMgr		_exe_mgr;  // 执行器管理器，管理执行器的运行

	WTSVariant*			_config;  // 配置对象指针，存储加载的配置信息

	WtSimpDataMgr		_data_mgr;  // 简单数据管理器，管理行情数据和K线数据

	WTSBaseDataMgr		_bd_mgr;  // 基础数据管理器，管理商品、合约、交易时段等基础数据
	WTSHotMgr			_hot_mgr;  // 热点合约管理器，管理主力合约、次主力合约等
	ActionPolicyMgr		_act_policy;  // 开平策略管理器，管理开仓和平仓策略

	wt_hashmap<std::string, double> _positions;  // 目标仓位映射表，键为合约代码，值为目标持仓数量
};

