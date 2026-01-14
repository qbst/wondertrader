/*!
 * \file WtRtRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtRtRunner运行时运行器类定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtRtRunner类，这是WtPorter模块的核心类，负责管理整个交易系统的运行时环境。
 * WtRtRunner是WonderTrader框架与外部语言（如Python、C#、Java等）交互的核心桥梁，通过C接口
 * 和回调函数机制，实现了跨语言调用和事件驱动的编程模型。
 * 
 * 主要功能：
 * 1. 交易引擎管理：管理CTA（商品交易顾问）、HFT（高频交易）、SEL（选股）三种引擎的创建、初始化和运行
 * 2. 策略上下文管理：创建和管理各种策略上下文实例（ExpCtaContext、ExpHftContext、ExpSelContext），
 *    这些上下文作为适配器，将引擎事件转发给外部语言回调函数
 * 3. 回调函数管理：注册和管理各种回调函数（策略初始化、Tick更新、K线闭合、订单成交等），
 *    实现引擎事件到外部语言的转发
 * 4. 数据加载管理：实现IHisDataLoader接口，支持外部数据源加载历史K线数据和复权因子数据
 * 5. 扩展组件管理：支持外部语言实现的Parser（行情解析器）和Executer（执行器）的创建和管理
 * 6. 事件通知：实现IEngineEvtListener接口，监听引擎事件（初始化、调度、交易日开始/结束等）
 * 7. 日志处理：实现ILogHandler接口，处理日志消息并转发给事件通知器
 * 
 * 设计特点：
 * - 单例模式：WtRtRunner作为单例对象，管理整个系统的生命周期
 * - 事件驱动：通过回调函数机制实现事件驱动的编程模型，外部语言通过注册回调函数接收事件
 * - 多引擎支持：支持CTA、HFT、SEL三种策略引擎，根据配置选择对应的引擎类型
 * - 适配器模式：通过适配器模式管理交易通道（TraderAdapter）和行情通道（ParserAdapter）
 * - 扩展性：支持外部数据加载器，允许从自定义数据源加载历史数据
 * - 线程安全：使用互斥锁保护数据加载相关的共享资源
 * 
 * 使用流程：
 * 1. 创建WtRtRunner实例
 * 2. 调用init()初始化日志系统和运行环境
 * 3. 注册各种回调函数（registerCtaCallbacks、registerHftCallbacks等）
 * 4. 调用config()加载配置文件并初始化各组件（引擎、数据管理器、交易通道、行情通道等）
 * 5. 创建策略上下文（createCtaContext、createHftContext、createSelContext）
 * 6. 调用run()启动系统运行（会阻塞当前线程，直到系统停止）
 * 7. 调用release()释放资源
 */
#pragma once
#include <string>  // 标准字符串库

#include "PorterDefs.h"  // Porter模块定义文件

#include "../Includes/ILogHandler.h"  // 日志处理器接口
#include "../Includes/IDataReader.h"  // 数据读取器接口

#include "../WtCore/EventNotifier.h"  // 事件通知器
#include "../WtCore/CtaStrategyMgr.h"  // CTA策略管理器
#include "../WtCore/HftStrategyMgr.h"  // HFT策略管理器
#include "../WtCore/SelStrategyMgr.h"  // SEL策略管理器
#include "../WtCore/WtCtaEngine.h"  // CTA引擎
#include "../WtCore/WtHftEngine.h"  // HFT引擎
#include "../WtCore/WtSelEngine.h"  // SEL引擎
#include "../WtCore/WtLocalExecuter.h"  // 本地执行器
#include "../WtCore/WtDiffExecuter.h"  // 差异执行器
#include "../WtCore/WtDistExecuter.h"  // 分布式执行器
#include "../WtCore/WtArbiExecuter.h"  // 套利执行器
#include "../WtCore/TraderAdapter.h"  // 交易适配器
#include "../WtCore/ParserAdapter.h"  // 行情适配器
#include "../WtCore/WtDtMgr.h"  // 数据管理器
#include "../WtCore/ActionPolicyMgr.h"  // 开平策略管理器

#include "../WTSTools/WTSHotMgr.h"  // 主力合约管理器
#include "../WTSTools/WTSBaseDataMgr.h"  // 基础数据管理器

NS_WTP_BEGIN
class WTSVariant;  // 前向声明：变体类型（用于配置）
class WtDataStorage;  // 前向声明：数据存储类
NS_WTP_END

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 引擎类型枚举
 * 
 * 定义不同类型的交易引擎，用于标识当前运行器使用的引擎类型。
 * 不同的引擎类型对应不同的策略类型和交易模式。
 */
typedef enum tagEngineType
{
	ET_CTA = 999,	// CTA引擎（商品交易顾问策略引擎），用于CTA策略交易
	ET_HFT,			// HFT引擎（高频交易策略引擎），用于高频交易策略
	ET_SEL			// SEL引擎（选股策略引擎），用于选股策略
} EngineType;

/**
 * @brief WonderTrader运行时运行器类
 * 
 * WtRtRunner是WtPorter模块的核心类，负责管理整个交易系统的运行时环境。
 * 该类同时实现IEngineEvtListener、ILogHandler和IHisDataLoader三个接口：
 * - IEngineEvtListener：监听引擎事件（初始化、调度、交易日事件等）
 * - ILogHandler：处理日志消息，将日志转发给事件通知器
 * - IHisDataLoader：提供历史数据加载接口，支持外部数据源加载历史K线和复权因子数据
 * 
 * 核心职责：
 * 1. 管理交易引擎的生命周期（创建、初始化、运行、销毁）
 * 2. 管理策略上下文的创建和映射（通过上下文ID管理策略实例）
 * 3. 管理回调函数的注册和事件转发（将引擎事件转发给外部语言）
 * 4. 管理数据加载器（支持外部数据源加载历史数据）
 * 5. 管理扩展组件（Parser、Executer的创建和管理）
 * 6. 管理交易通道和行情通道（通过适配器管理器）
 * 
 * 成员变量说明：
 * - 回调函数指针：存储各种策略和组件的事件回调函数
 * - 引擎对象：CTA、HFT、SEL三种引擎实例
 * - 管理器对象：交易适配器管理器、解析器适配器管理器、执行器工厂、策略管理器等
 * - 数据对象：数据管理器、基础数据管理器、热点合约管理器、事件通知器等
 * - 配置对象：存储加载的配置文件内容
 * - 标志变量：标识当前引擎类型（_is_hft、_is_sel）和退出标志（_to_exit）
 * - 外部数据加载器：外部注册的数据加载函数指针
 * - 数据推送相关：用于数据推送的对象指针、回调函数和互斥锁
 */
class WtRtRunner : public IEngineEvtListener, public ILogHandler, public IHisDataLoader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建运行时运行器实例，初始化所有成员变量为默认值。
	 * 所有回调函数指针初始化为NULL，标志变量初始化为false。
	 */
	WtRtRunner();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理运行时运行器占用的资源。
	 * 注意：析构函数不负责释放引擎和管理器对象，这些对象会在系统停止时自动清理。
	 */
	~WtRtRunner();

public:
	//////////////////////////////////////////////////////////////////////////
	//IHisDataLoader接口实现 - 历史数据加载接口
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 加载复权后的历史K线数据（IHisDataLoader接口实现）
	 * 
	 * 从外部数据加载器加载指定合约的复权后历史K线数据。
	 * 
	 * @param obj 数据接收对象指针，用于回调函数传递
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param period K线周期（KP_DAY、KP_Minute1、KP_Minute5等）
	 * @param cb 数据读取回调函数，当数据加载完成后调用此函数
	 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
	 * 
	 * 实现说明：
	 * - 使用互斥锁保护数据推送相关的共享资源
	 * - 将K线周期转换为字符串格式（"d1"、"m1"、"m5"）
	 * - 调用外部注册的复权K线加载器函数
	 * - 数据加载完成后，通过feedRawBars()推送数据
	 */
	virtual bool loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) override;

	/**
	 * @brief 加载原始历史K线数据（IHisDataLoader接口实现）
	 * 
	 * 从外部数据加载器加载指定合约的原始（未复权）历史K线数据。
	 * 
	 * @param obj 数据接收对象指针，用于回调函数传递
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param period K线周期（KP_DAY、KP_Minute1、KP_Minute5等）
	 * @param cb 数据读取回调函数，当数据加载完成后调用此函数
	 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
	 * 
	 * 实现说明：
	 * - 使用互斥锁保护数据推送相关的共享资源
	 * - 将K线周期转换为字符串格式（"d1"、"m1"、"m5"）
	 * - 调用外部注册的原始K线加载器函数
	 * - 数据加载完成后，通过feedRawBars()推送数据
	 */
	virtual bool loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) override;

	/**
	 * @brief 加载所有合约的复权因子数据（IHisDataLoader接口实现）
	 * 
	 * 从外部数据加载器加载所有合约的复权因子数据。
	 * 
	 * @param obj 数据接收对象指针，用于回调函数传递
	 * @param cb 复权因子读取回调函数，当数据加载完成后调用此函数
	 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
	 * 
	 * 实现说明：
	 * - 使用互斥锁保护数据推送相关的共享资源
	 * - 调用外部注册的复权因子加载器函数，传入空字符串表示加载所有合约
	 * - 数据加载完成后，通过feedAdjFactors()推送数据
	 */
	virtual bool loadAllAdjFactors(void* obj, FuncReadFactors cb) override;

	/**
	 * @brief 加载指定合约的复权因子数据（IHisDataLoader接口实现）
	 * 
	 * 从外部数据加载器加载指定合约的复权因子数据。
	 * 
	 * @param obj 数据接收对象指针，用于回调函数传递
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param cb 复权因子读取回调函数，当数据加载完成后调用此函数
	 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
	 * 
	 * 实现说明：
	 * - 使用互斥锁保护数据推送相关的共享资源
	 * - 调用外部注册的复权因子加载器函数，传入合约代码
	 * - 数据加载完成后，通过feedAdjFactors()推送数据
	 */
	virtual bool loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb) override;

	/**
	 * @brief 推送原始K线数据
	 * 
	 * 当外部数据加载器加载完K线数据后，调用此函数将数据推送给数据管理器。
	 * 
	 * @param bars K线数据数组指针
	 * @param count K线数据条数
	 * 
	 * 使用场景：
	 * - 外部数据加载器加载完数据后，调用此函数推送数据
	 * - 数据会通过之前注册的回调函数传递给数据管理器
	 */
	void feedRawBars(WTSBarStruct* bars, uint32_t count);

	/**
	 * @brief 推送复权因子数据
	 * 
	 * 当外部数据加载器加载完复权因子数据后，调用此函数将数据推送给数据管理器。
	 * 
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param dates 复权日期数组指针（格式：YYYYMMDD）
	 * @param factors 复权因子数组指针
	 * @param count 复权因子数据条数
	 * 
	 * 使用场景：
	 * - 外部数据加载器加载完复权因子数据后，调用此函数推送数据
	 * - 数据会通过之前注册的回调函数传递给数据管理器
	 */
	void feedAdjFactors(const char* stdCode, uint32_t* dates, double* factors, uint32_t count);

public:
	//////////////////////////////////////////////////////////////////////////
	//系统初始化和配置
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 初始化运行时运行器
	 * 
	 * 初始化日志系统和运行环境，设置安装目录和生成目录。
	 * 
	 * @param logCfg 日志配置文件路径或配置内容，默认为"logcfg.prop"
	 * @param isFile 是否为文件路径，true表示logCfg是文件路径，false表示logCfg是配置内容（JSON字符串）
	 * @param genDir 生成文件目录路径，用于存储生成的文件（如策略生成的报告等）
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 在Windows平台启用MiniDump崩溃转储功能（用于问题排查）
	 * 2. 初始化日志系统（从文件或字符串加载配置）
	 * 3. 设置安装目录路径（用于查找动态库和配置文件）
	 * 4. 设置生成目录路径（用于存储生成的文件）
	 * 
	 * 注意事项：
	 * - 必须在调用其他方法之前调用此方法
	 * - 日志系统初始化后，后续的日志会记录到日志文件
	 */
	bool init(const char* logCfg = "logcfg.prop", bool isFile = true, const char* genDir = "");

	/**
	 * @brief 配置运行时运行器
	 * 
	 * 从配置文件加载配置并初始化各组件（引擎、数据管理器、交易通道、行情通道、执行器等）。
	 * 
	 * @param cfgFile 配置文件路径或配置内容（JSON格式）
	 * @param isFile 是否为文件路径，true表示cfgFile是文件路径，false表示cfgFile是配置内容（JSON字符串）
	 * @return 配置成功返回true，失败返回false
	 * 
	 * 配置流程：
	 * 1. 加载主配置文件（从文件或字符串）
	 * 2. 加载基础数据文件（交易时段、商品、合约、节假日、主力合约规则等）
	 * 3. 初始化交易引擎（根据配置选择CTA、HFT或SEL引擎）
	 * 4. 初始化数据管理器（注册数据加载器）
	 * 5. 初始化开平策略管理器
	 * 6. 初始化行情通道（解析器适配器）
	 * 7. 初始化交易通道（交易适配器）
	 * 8. 初始化事件通知器
	 * 9. 如果不是高频引擎，初始化执行器（本地执行器、差分执行器、分布式执行器等）
	 * 10. 初始化策略（从配置文件加载策略并创建策略上下文）
	 * 
	 * 注意事项：
	 * - 必须在init()之后调用此方法
	 * - 配置文件格式必须符合WonderTrader的配置规范
	 * - 如果配置加载失败，会记录错误日志并返回false
	 */
	bool config(const char* cfgFile, bool isFile = true);

	/**
	 * @brief 运行运行时运行器
	 * 
	 * 启动行情通道、交易通道和交易引擎的运行。
	 * 
	 * @param bAsync 是否为异步模式，true表示异步模式（不阻塞），false表示同步模式（阻塞直到系统停止）
	 * 
	 * 运行流程：
	 * 1. 启动解析器适配器管理器（行情通道），接收实时行情数据
	 * 2. 启动交易适配器管理器（交易通道），处理交易指令
	 * 3. 启动交易引擎，执行策略逻辑
	 * 4. 如果是同步模式：
	 *    a. 安装信号钩子（捕获程序异常和退出信号）
	 *    b. 进入循环，等待退出信号（每10毫秒检查一次）
	 *    c. 收到退出信号后退出循环
	 * 
	 * 注意事项：
	 * - 同步模式下，此函数会阻塞当前线程，直到系统停止
	 * - 异步模式下，此函数会立即返回，系统在后台运行
	 * - 使用try-catch捕获异常，确保程序不会因异常而崩溃
	 */
	void run(bool bAsync = false);

	/**
	 * @brief 释放运行时运行器资源
	 * 
	 * 清理运行时运行器占用的资源，停止日志系统。
	 * 
	 * 注意事项：
	 * - 调用此函数后，模块将无法继续使用，需要重新初始化
	 * - 此函数会停止日志系统，后续的日志将无法记录
	 */
	void release();

	//////////////////////////////////////////////////////////////////////////
	//回调函数注册接口 - 用于外部语言注册事件回调函数
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 注册CTA策略引擎的回调函数
	 * 
	 * 注册CTA策略引擎的各种事件回调函数，当策略事件发生时，会调用对应的回调函数。
	 * 
	 * @param cbInit 策略初始化回调函数，当策略初始化完成时调用
	 * @param cbTick Tick数据回调函数，当收到新的Tick数据时调用
	 * @param cbCalc 策略计算回调函数，当策略需要计算时调用（定时计算）
	 * @param cbBar K线闭合回调函数，当K线闭合时调用
	 * @param cbSessEvt 交易日事件回调函数，当交易日开始或结束时调用
	 * @param cbCondTrigger 条件单触发回调函数，当条件单触发时调用（可选，默认为NULL）
	 * 
	 * 注意事项：
	 * - 所有回调函数都可以为NULL，表示不处理对应的事件
	 * - 回调函数会在策略上下文的事件处理方法中被调用
	 * - 回调函数的参数格式必须符合PorterDefs.h中定义的类型
	 */
	void registerCtaCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCondTriggerCallback cbCondTrigger = NULL);
	
	/**
	 * @brief 注册SEL策略引擎的回调函数
	 * 
	 * 注册SEL（选股）策略引擎的各种事件回调函数。
	 * 
	 * @param cbInit 策略初始化回调函数，当策略初始化完成时调用
	 * @param cbTick Tick数据回调函数，当收到新的Tick数据时调用
	 * @param cbCalc 策略计算回调函数，当策略需要计算时调用（定时计算）
	 * @param cbBar K线闭合回调函数，当K线闭合时调用
	 * @param cbSessEvt 交易日事件回调函数，当交易日开始或结束时调用
	 * 
	 * 注意事项：
	 * - SEL策略不支持条件单触发回调
	 * - 其他说明同registerCtaCallbacks()
	 */
	void registerSelCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt);
	
	/**
	 * @brief 注册HFT策略引擎的回调函数
	 * 
	 * 注册HFT（高频交易）策略引擎的各种事件回调函数。
	 * 
	 * @param cbInit 策略初始化回调函数，当策略初始化完成时调用
	 * @param cbTick Tick数据回调函数，当收到新的Tick数据时调用
	 * @param cbBar K线闭合回调函数，当K线闭合时调用
	 * @param cbChnl 交易通道事件回调函数，当交易通道就绪或丢失时调用
	 * @param cbOrd 订单回报回调函数，当订单状态变化时调用
	 * @param cbTrd 成交回报回调函数，当订单成交时调用
	 * @param cbEntrust 下单结果回调函数，当下单成功或失败时调用
	 * @param cbOrdDtl 逐笔委托回调函数，当收到逐笔委托数据时调用
	 * @param cbOrdQue 委托队列回调函数，当收到委托队列数据时调用
	 * @param cbTrans 逐笔成交回调函数，当收到逐笔成交数据时调用
	 * @param cbSessEvt 交易日事件回调函数，当交易日开始或结束时调用
	 * @param cbPosition 持仓变化回调函数，当持仓发生变化时调用
	 * 
	 * 注意事项：
	 * - HFT策略支持更多的事件类型（订单、成交、Level2数据等）
	 * - 其他说明同registerCtaCallbacks()
	 */
	void registerHftCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar,
		FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust,
		FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt, FuncHftPosCallback cbPosition);

	/**
	 * @brief 注册引擎事件回调函数
	 * 
	 * 注册引擎级别的事件回调函数（初始化、调度、交易日事件等）。
	 * 
	 * @param cbEvt 引擎事件回调函数，当引擎事件发生时调用
	 * 
	 * 事件类型：
	 * - EVENT_ENGINE_INIT：引擎初始化完成
	 * - EVENT_ENGINE_SCHDL：引擎调度事件（定时触发）
	 * - EVENT_SESSION_BEGIN：交易日开始
	 * - EVENT_SESSION_END：交易日结束
	 * 
	 * 注意事项：
	 * - 注册后，会将当前对象注册为三个引擎的事件监听器
	 * - 引擎事件会通过IEngineEvtListener接口方法触发
	 */
	void registerEvtCallback(FuncEventCallback cbEvt);

	/**
	 * @brief 注册扩展Parser的回调函数
	 * 
	 * 注册扩展Parser（外部语言实现的行情解析器）的事件回调函数。
	 * 
	 * @param cbEvt Parser事件回调函数，当Parser事件发生时调用（初始化、连接、断开等）
	 * @param cbSub Parser订阅回调函数，当需要订阅或取消订阅合约时调用
	 * 
	 * 事件类型：
	 * - EVENT_PARSER_INIT：Parser初始化
	 * - EVENT_PARSER_CONNECT：Parser连接成功
	 * - EVENT_PARSER_DISCONNECT：Parser断开连接
	 * - EVENT_PARSER_RELEASE：Parser释放
	 */
	void registerParserPorter(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub);

	/**
	 * @brief 注册扩展Executer的回调函数
	 * 
	 * 注册扩展Executer（外部语言实现的执行器）的事件回调函数。
	 * 
	 * @param cbInit Executer初始化回调函数，当Executer初始化时调用
	 * @param cbExec Executer命令回调函数，当需要执行交易命令时调用
	 * 
	 * 使用场景：
	 * - 外部语言实现自定义执行器时，需要注册这些回调函数
	 * - 执行器初始化时会调用cbInit
	 * - 需要设置目标仓位时会调用cbExec
	 */
	void registerExecuterPorter(FuncExecInitCallback cbInit, FuncExecCmdCallback cbExec);

	/**
	 * @brief 注册外部数据加载器
	 * 
	 * 注册外部数据加载器函数指针，用于从自定义数据源加载历史数据。
	 * 
	 * @param fnlBarLoader 复权K线数据加载器函数指针，用于加载复权后的K线数据
	 * @param rawBarLoader 原始K线数据加载器函数指针，用于加载原始（未复权）K线数据
	 * @param fctLoader 复权因子加载器函数指针，用于加载复权因子数据
	 * @param tickLoader Tick数据加载器函数指针（可选，当前未使用，默认为NULL）
	 * 
	 * 使用场景：
	 * - 当需要从自定义数据源（如数据库、API等）加载历史数据时，注册这些加载器
	 * - 数据管理器会通过IHisDataLoader接口调用这些加载器
	 * - 加载器加载完数据后，通过feedRawBars()或feedAdjFactors()推送数据
	 * 
	 * 注意事项：
	 * - 所有加载器函数都可以为NULL，表示不使用外部数据加载器
	 * - 如果注册了外部数据加载器，数据管理器会优先使用外部加载器
	 */
	void		registerExtDataLoader(FuncLoadFnlBars fnlBarLoader, FuncLoadRawBars rawBarLoader, FuncLoadAdjFactors fctLoader, FuncLoadRawTicks tickLoader = NULL)
	{
		_ext_fnl_bar_loader = fnlBarLoader;  // 保存复权K线数据加载器函数指针
		_ext_raw_bar_loader = rawBarLoader;  // 保存原始K线数据加载器函数指针
		_ext_adj_fct_loader = fctLoader;  // 保存复权因子加载器函数指针
	}

	//////////////////////////////////////////////////////////////////////////
	//扩展组件创建接口 - 用于创建外部语言实现的组件
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 创建扩展Parser（外部语言实现的行情解析器）
	 * 
	 * 创建扩展Parser实例，并将其添加到解析器适配器管理器。
	 * 
	 * @param id Parser的唯一标识符
	 * @return 创建成功返回true，失败返回false
	 * 
	 * 创建流程：
	 * 1. 创建ParserAdapter适配器实例
	 * 2. 创建ExpParser扩展Parser实例
	 * 3. 初始化适配器，将扩展Parser与引擎关联
	 * 4. 将适配器添加到解析器适配器管理器
	 * 
	 * 注意事项：
	 * - 扩展Parser的事件会通过registerParserPorter()注册的回调函数转发给外部语言
	 * - Parser的ID必须唯一，不能与已存在的Parser重复
	 */
	bool			createExtParser(const char* id);
	
	/**
	 * @brief 创建扩展Executer（外部语言实现的执行器）
	 * 
	 * 创建扩展Executer实例，并将其添加到CTA引擎的执行器列表。
	 * 
	 * @param id Executer的唯一标识符
	 * @return 创建成功返回true，失败返回false
	 * 
	 * 创建流程：
	 * 1. 创建ExpExecuter扩展执行器实例
	 * 2. 初始化执行器
	 * 3. 将执行器添加到CTA引擎的执行器列表
	 * 
	 * 注意事项：
	 * - 扩展Executer的命令会通过registerExecuterPorter()注册的回调函数转发给外部语言
	 * - Executer的ID必须唯一，不能与已存在的Executer重复
	 * - 扩展Executer只能添加到CTA引擎，不支持HFT和SEL引擎
	 */
	bool			createExtExecuter(const char* id);

	//////////////////////////////////////////////////////////////////////////
	//策略上下文创建和管理接口 - 用于创建和管理策略上下文
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 创建CTA策略上下文
	 * 
	 * 创建CTA策略的扩展上下文实例，并将其添加到CTA引擎。
	 * 
	 * @param name 策略名称，用于标识策略
	 * @param slippage 滑点设置（单位：最小变动价位），用于模拟交易时的滑点成本
	 * @return 返回策略上下文的ID，用于后续访问该上下文
	 * 
	 * 创建流程：
	 * 1. 创建ExpCtaContext扩展上下文实例
	 * 2. 将上下文添加到CTA引擎
	 * 3. 返回上下文的ID
	 * 
	 * 注意事项：
	 * - 上下文ID由引擎自动分配，是唯一的
	 * - 滑点设置会影响模拟交易的成本计算
	 * - 创建上下文后，策略的事件会通过registerCtaCallbacks()注册的回调函数转发
	 */
	uint32_t		createCtaContext(const char* name, int32_t slippage);
	
	/**
	 * @brief 创建HFT策略上下文
	 * 
	 * 创建HFT策略的扩展上下文实例，并将其添加到HFT引擎，同时绑定交易通道。
	 * 
	 * @param name 策略名称，用于标识策略
	 * @param trader 交易通道ID，用于绑定交易通道
	 * @param bAgent 是否为代理模式，true表示代理模式（订单直接发送到交易通道），false表示非代理模式
	 * @param slippage 滑点设置（单位：最小变动价位），用于模拟交易时的滑点成本
	 * @return 返回策略上下文的ID，用于后续访问该上下文
	 * 
	 * 创建流程：
	 * 1. 创建ExpHftContext扩展上下文实例
	 * 2. 将上下文添加到HFT引擎
	 * 3. 根据trader参数查找交易适配器
	 * 4. 如果找到交易适配器，将上下文绑定到交易适配器
	 * 5. 返回上下文的ID
	 * 
	 * 注意事项：
	 * - HFT策略必须绑定交易通道，否则无法进行交易
	 * - 如果指定的交易通道不存在，会记录错误日志，但上下文仍会创建成功
	 * - 代理模式下，订单会直接发送到交易通道，不经过执行器
	 */
	uint32_t		createHftContext(const char* name, const char* trader, bool bAgent, int32_t slippage);
	
	/**
	 * @brief 创建SEL策略上下文
	 * 
	 * 创建SEL（选股）策略的扩展上下文实例，并将其添加到SEL引擎。
	 * 
	 * @param name 策略名称，用于标识策略
	 * @param date 调度日期（格式：YYYYMMDD），策略开始执行的日期
	 * @param time 调度时间（格式：HHMM），策略开始执行的时间
	 * @param period 调度周期，可选值："d"（日）、"w"（周）、"m"（月）、"y"（年）、"min"（分钟）
	 * @param slippage 滑点设置（单位：最小变动价位），用于模拟交易时的滑点成本
	 * @param trdtpl 交易日模板，默认为"CHINA"（中国交易日）
	 * @param session 交易时段，默认为"TRADING"（交易时段）
	 * @return 返回策略上下文的ID，用于后续访问该上下文
	 * 
	 * 创建流程：
	 * 1. 解析调度周期字符串，转换为TaskPeriodType枚举值
	 * 2. 创建ExpSelContext扩展上下文实例
	 * 3. 将上下文添加到SEL引擎，并设置调度参数
	 * 4. 返回上下文的ID
	 * 
	 * 注意事项：
	 * - SEL策略是定时执行的，需要设置调度日期、时间和周期
	 * - 调度周期决定了策略的执行频率
	 * - 交易日模板和交易时段用于确定策略的执行时间
	 */
	uint32_t		createSelContext(const char* name, uint32_t date, uint32_t time, const char* period, int32_t slippage, const char* trdtpl = "CHINA", const char* session="TRADING");

	/**
	 * @brief 获取CTA策略上下文
	 * 
	 * 根据上下文ID获取CTA策略上下文指针。
	 * 
	 * @param id 策略上下文的ID（由createCtaContext()返回）
	 * @return 返回策略上下文智能指针，如果ID不存在返回空指针
	 */
	CtaContextPtr	getCtaContext(uint32_t id);
	
	/**
	 * @brief 获取SEL策略上下文
	 * 
	 * 根据上下文ID获取SEL策略上下文指针。
	 * 
	 * @param id 策略上下文的ID（由createSelContext()返回）
	 * @return 返回策略上下文智能指针，如果ID不存在返回空指针
	 */
	SelContextPtr	getSelContext(uint32_t id);
	
	/**
	 * @brief 获取HFT策略上下文
	 * 
	 * 根据上下文ID获取HFT策略上下文指针。
	 * 
	 * @param id 策略上下文的ID（由createHftContext()返回）
	 * @return 返回策略上下文智能指针，如果ID不存在返回空指针
	 */
	HftContextPtr	getHftContext(uint32_t id);
	
	/**
	 * @brief 获取当前使用的交易引擎指针
	 * 
	 * 返回当前使用的交易引擎指针（CTA、HFT或SEL引擎之一）。
	 * 
	 * @return 返回交易引擎指针，用于访问引擎的公共接口
	 * 
	 * 注意事项：
	 * - 引擎类型由config()方法根据配置文件确定
	 * - 返回的指针指向_cta_engine、_hft_engine或_sel_engine之一
	 */
	WtEngine*		getEngine(){ return _engine; }

	/**
	 * @brief 获取原始合约代码
	 * 
	 * 将标准合约代码转换为原始合约代码（去掉.HOT、.2ND等后缀）。
	 * 
	 * @param stdCode 标准合约代码，如"SHFE.rb2305.HOT"
	 * @return 返回原始合约代码字符串指针，如"SHFE.rb2305"
	 * 
	 * 注意事项：
	 * - 返回的字符串指针指向线程局部静态变量，下次调用会被覆盖
	 * - 如果需要保存结果，应该复制字符串内容
	 */
	const char*	get_raw_stdcode(const char* stdCode);

	//////////////////////////////////////////////////////////////////////////
	//ILogHandler接口实现 - 日志处理接口
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 处理日志追加（ILogHandler接口实现）
	 * 
	 * 当日志系统需要记录日志时，会调用此方法。
	 * 此方法将日志消息转发给事件通知器，以便外部系统可以接收日志消息。
	 * 
	 * @param ll 日志级别（LL_DEBUG、LL_INFO、LL_WARN、LL_ERROR、LL_FATAL等）
	 * @param msg 日志消息内容
	 * 
	 * 实现说明：
	 * - 将日志级别转换为字符串标签（"debug"、"info"、"warn"、"error"、"fatal"等）
	 * - 调用事件通知器的notify_log()方法发送日志消息
	 * - 外部系统可以通过事件通知器接收日志消息
	 */
public:
	virtual void handleLogAppend(WTSLogLevel ll, const char* msg) override;

	//////////////////////////////////////////////////////////////////////////
	//扩展Parser接口 - 用于外部语言实现的Parser组件
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief Parser初始化事件通知
	 * 
	 * 当扩展Parser初始化时，调用此方法通知外部语言。
	 * 
	 * @param id Parser的唯一标识符
	 * 
	 * 实现说明：
	 * - 调用registerParserPorter()注册的Parser事件回调函数
	 * - 事件类型为EVENT_PARSER_INIT
	 */
public:
	void parser_init(const char* id);
	
	/**
	 * @brief Parser连接事件通知
	 * 
	 * 当扩展Parser连接成功时，调用此方法通知外部语言。
	 * 
	 * @param id Parser的唯一标识符
	 * 
	 * 实现说明：
	 * - 调用registerParserPorter()注册的Parser事件回调函数
	 * - 事件类型为EVENT_PARSER_CONNECT
	 */
	void parser_connect(const char* id);
	
	/**
	 * @brief Parser释放事件通知
	 * 
	 * 当扩展Parser释放时，调用此方法通知外部语言。
	 * 
	 * @param id Parser的唯一标识符
	 * 
	 * 实现说明：
	 * - 调用registerParserPorter()注册的Parser事件回调函数
	 * - 事件类型为EVENT_PARSER_RELEASE
	 */
	void parser_release(const char* id);
	
	/**
	 * @brief Parser断开连接事件通知
	 * 
	 * 当扩展Parser断开连接时，调用此方法通知外部语言。
	 * 
	 * @param id Parser的唯一标识符
	 * 
	 * 实现说明：
	 * - 调用registerParserPorter()注册的Parser事件回调函数
	 * - 事件类型为EVENT_PARSER_DISCONNECT
	 */
	void parser_disconnect(const char* id);
	
	/**
	 * @brief Parser订阅合约事件通知
	 * 
	 * 当扩展Parser需要订阅合约时，调用此方法通知外部语言。
	 * 
	 * @param id Parser的唯一标识符
	 * @param code 合约代码，如"SHFE.rb2305"
	 * 
	 * 实现说明：
	 * - 调用registerParserPorter()注册的Parser订阅回调函数
	 * - 订阅标志为true，表示订阅合约
	 */
	void parser_subscribe(const char* id, const char* code);
	
	/**
	 * @brief Parser取消订阅合约事件通知
	 * 
	 * 当扩展Parser需要取消订阅合约时，调用此方法通知外部语言。
	 * 
	 * @param id Parser的唯一标识符
	 * @param code 合约代码，如"SHFE.rb2305"
	 * 
	 * 实现说明：
	 * - 调用registerParserPorter()注册的Parser订阅回调函数
	 * - 订阅标志为false，表示取消订阅合约
	 */
	void parser_unsubscribe(const char* id, const char* code);

	/**
	 * @brief 处理扩展Parser推送的行情数据
	 * 
	 * 当外部语言实现的Parser收到行情数据时，调用此方法将数据推送给引擎。
	 * 
	 * @param id Parser的唯一标识符
	 * @param curTick 当前Tick数据结构指针
	 * @param uProcFlag 处理标志，用于标识数据的处理方式
	 * 
	 * 处理流程：
	 * 1. 根据Parser ID查找对应的Parser适配器
	 * 2. 如果找到适配器，创建WTSTickData对象并调用适配器的handleQuote()方法
	 * 3. 如果未找到适配器，记录警告日志
	 * 
	 * 注意事项：
	 * - 此方法由外部语言调用，用于推送行情数据
	 * - Tick数据会被转发给引擎，最终传递给策略
	 */
	void on_ext_parser_quote(const char* id, WTSTickStruct* curTick, uint32_t uProcFlag);


	//////////////////////////////////////////////////////////////////////////
	//扩展Executer接口 - 用于外部语言实现的Executer组件
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief Executer初始化事件通知
	 * 
	 * 当扩展Executer初始化时，调用此方法通知外部语言。
	 * 
	 * @param id Executer的唯一标识符
	 * 
	 * 实现说明：
	 * - 调用registerExecuterPorter()注册的Executer初始化回调函数
	 */
public:
	void executer_set_position(const char* id, const char* stdCode, double target);
	
	/**
	 * @brief Executer设置目标仓位事件通知
	 * 
	 * 当需要设置Executer的目标仓位时，调用此方法通知外部语言。
	 * 
	 * @param id Executer的唯一标识符
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param target 目标持仓数量，正数表示多头，负数表示空头，0表示平仓
	 * 
	 * 实现说明：
	 * - 调用registerExecuterPorter()注册的Executer命令回调函数
	 * - 外部语言实现的Executer应该根据目标仓位执行相应的交易操作
	 */
	void executer_init(const char* id);

	//////////////////////////////////////////////////////////////////////////
	//IEngineEvtListener接口实现 - 引擎事件监听接口
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 引擎初始化完成事件（IEngineEvtListener接口实现）
	 * 
	 * 当交易引擎初始化完成时，引擎会调用此方法。
	 * 此方法将事件转发给外部语言注册的事件回调函数。
	 * 
	 * 实现说明：
	 * - 如果注册了事件回调函数，调用回调函数通知外部语言
	 * - 事件类型为EVENT_ENGINE_INIT
	 * - 事件参数：日期和时间都为0（初始化事件不需要日期时间信息）
	 */
public:
	virtual void on_initialize_event() override
	{
		if (_cb_evt)  // 如果注册了事件回调函数
			_cb_evt(EVENT_ENGINE_INIT, 0, 0);  // 调用回调函数，事件类型为引擎初始化，日期和时间都为0
	}

	/**
	 * @brief 引擎调度事件（IEngineEvtListener接口实现）
	 * 
	 * 当交易引擎触发调度事件时（定时触发），引擎会调用此方法。
	 * 此方法将事件转发给外部语言注册的事件回调函数。
	 * 
	 * @param uDate 调度日期（格式：YYYYMMDD）
	 * @param uTime 调度时间（格式：HHMM）
	 * 
	 * 实现说明：
	 * - 如果注册了事件回调函数，调用回调函数通知外部语言
	 * - 事件类型为EVENT_ENGINE_SCHDL
	 * - 事件参数：调度日期和时间
	 */
	virtual void on_schedule_event(uint32_t uDate, uint32_t uTime) override
	{
		if (_cb_evt)  // 如果注册了事件回调函数
			_cb_evt(EVENT_ENGINE_SCHDL, uDate, uTime);  // 调用回调函数，事件类型为引擎调度，传入日期和时间
	}

	/**
	 * @brief 交易日事件（IEngineEvtListener接口实现）
	 * 
	 * 当交易日开始或结束时，引擎会调用此方法。
	 * 此方法将事件转发给外部语言注册的事件回调函数。
	 * 
	 * @param uDate 交易日期（格式：YYYYMMDD）
	 * @param isBegin 是否为交易日开始，true表示交易日开始，false表示交易日结束
	 * 
	 * 实现说明：
	 * - 如果注册了事件回调函数，调用回调函数通知外部语言
	 * - 事件类型根据isBegin参数确定：true为EVENT_SESSION_BEGIN，false为EVENT_SESSION_END
	 * - 事件参数：交易日期，时间为0（交易日事件不需要时间信息）
	 */
	virtual void on_session_event(uint32_t uDate, bool isBegin = true) override
	{
		if (_cb_evt)  // 如果注册了事件回调函数
			_cb_evt(isBegin ? EVENT_SESSION_BEGIN : EVENT_SESSION_END, uDate, 0);  // 调用回调函数，事件类型根据isBegin确定，传入日期，时间为0
	}

	//////////////////////////////////////////////////////////////////////////
	//策略上下文事件处理方法 - 由扩展上下文调用，转发事件给外部语言
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 策略初始化事件处理
	 * 
	 * 当策略上下文初始化完成时，扩展上下文会调用此方法。
	 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
	 * 
	 * @param id 策略上下文的ID
	 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
	 * 
	 * 实现说明：
	 * - 根据引擎类型调用对应的初始化回调函数（_cb_cta_init、_cb_hft_init、_cb_sel_init）
	 * - 如果回调函数未注册，则不处理
	 */
public:
	void ctx_on_init(uint32_t id, EngineType eType = ET_CTA);
	
	/**
	 * @brief 策略交易日事件处理
	 * 
	 * 当策略的交易日开始或结束时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param curTDate 当前交易日期（格式：YYYYMMDD）
	 * @param isBegin 是否为交易日开始，true表示交易日开始，false表示交易日结束
	 * @param eType 引擎类型，默认为ET_CTA
	 * 
	 * 实现说明：
	 * - 根据引擎类型调用对应的交易日事件回调函数
	 * - 回调函数参数：上下文ID、交易日期、是否开始
	 */
	void ctx_on_session_event(uint32_t id, uint32_t curTDate, bool isBegin = true, EngineType eType = ET_CTA);
	
	/**
	 * @brief 策略Tick数据事件处理
	 * 
	 * 当策略收到新的Tick数据时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param newTick 新的Tick数据指针
	 * @param eType 引擎类型，默认为ET_CTA
	 * 
	 * 实现说明：
	 * - 根据引擎类型调用对应的Tick数据回调函数
	 * - 回调函数参数：上下文ID、合约代码、Tick数据结构指针
	 */
	void ctx_on_tick(uint32_t id, const char* stdCode, WTSTickData* newTick, EngineType eType = ET_CTA);
	
	/**
	 * @brief 策略计算事件处理
	 * 
	 * 当策略需要计算时（定时计算），扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param curDate 当前日期（格式：YYYYMMDD）
	 * @param curTime 当前时间（格式：HHMM）
	 * @param eType 引擎类型，默认为ET_CTA
	 * 
	 * 实现说明：
	 * - 根据引擎类型调用对应的计算回调函数（CTA和SEL支持，HFT不支持）
	 * - 回调函数参数：上下文ID、当前日期、当前时间
	 */
	void ctx_on_calc(uint32_t id, uint32_t curDate, uint32_t curTime, EngineType eType = ET_CTA);
	
	/**
	 * @brief 策略K线闭合事件处理
	 * 
	 * 当K线闭合时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param period K线周期字符串，如"m1"、"m5"、"d1"等
	 * @param newBar 新的K线数据指针
	 * @param eType 引擎类型，默认为ET_CTA
	 * 
	 * 实现说明：
	 * - 根据引擎类型调用对应的K线闭合回调函数
	 * - 回调函数参数：上下文ID、合约代码、K线周期、K线数据结构指针
	 */
	void ctx_on_bar(uint32_t id, const char* stdCode, const char* period, WTSBarStruct* newBar, EngineType eType = ET_CTA);
	
	/**
	 * @brief 策略条件单触发事件处理
	 * 
	 * 当条件单触发时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param target 目标持仓数量
	 * @param price 触发价格
	 * @param usertag 用户标签，用于标识条件单
	 * @param eType 引擎类型，默认为ET_CTA
	 * 
	 * 实现说明：
	 * - 目前只支持CTA引擎的条件单触发回调
	 * - 回调函数参数：上下文ID、合约代码、目标持仓、触发价格、用户标签
	 */
	void ctx_on_cond_triggered(uint32_t id, const char* stdCode, double target, double price, const char* usertag, EngineType eType = ET_CTA);

	//////////////////////////////////////////////////////////////////////////
	//HFT策略专用事件处理方法 - HFT策略特有的交易相关事件
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief HFT策略交易通道就绪事件处理
	 * 
	 * 当HFT策略的交易通道就绪时，扩展上下文会调用此方法。
	 * 
	 * @param cHandle 策略上下文的ID（HFT上下文句柄）
	 * @param trader 交易通道ID
	 * 
	 * 实现说明：
	 * - 调用HFT通道事件回调函数，事件类型为CHNL_EVENT_READY
	 */
	void hft_on_channel_ready(uint32_t cHandle, const char* trader);
	
	/**
	 * @brief HFT策略交易通道丢失事件处理
	 * 
	 * 当HFT策略的交易通道丢失时，扩展上下文会调用此方法。
	 * 
	 * @param cHandle 策略上下文的ID（HFT上下文句柄）
	 * @param trader 交易通道ID
	 * 
	 * 实现说明：
	 * - 调用HFT通道事件回调函数，事件类型为CHNL_EVENT_LOST
	 */
	void hft_on_channel_lost(uint32_t cHandle, const char* trader);
	
	/**
	 * @brief HFT策略订单回报事件处理
	 * 
	 * 当HFT策略的订单状态变化时，扩展上下文会调用此方法。
	 * 
	 * @param cHandle 策略上下文的ID（HFT上下文句柄）
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param isBuy 是否为买入订单，true表示买入，false表示卖出
	 * @param totalQty 订单总数量
	 * @param leftQty 订单剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销，true表示已撤销，false表示未撤销
	 * @param userTag 用户标签，用于标识订单
	 * 
	 * 实现说明：
	 * - 调用HFT订单回报回调函数，将订单信息转发给外部语言
	 */
	void hft_on_order(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag);
	
	/**
	 * @brief HFT策略成交回报事件处理
	 * 
	 * 当HFT策略的订单成交时，扩展上下文会调用此方法。
	 * 
	 * @param cHandle 策略上下文的ID（HFT上下文句柄）
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param isBuy 是否为买入成交，true表示买入，false表示卖出
	 * @param vol 成交数量
	 * @param price 成交价格
	 * @param userTag 用户标签，用于标识订单
	 * 
	 * 实现说明：
	 * - 调用HFT成交回报回调函数，将成交信息转发给外部语言
	 */
	void hft_on_trade(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag);
	
	/**
	 * @brief HFT策略下单结果事件处理
	 * 
	 * 当HFT策略的下单请求有结果时，扩展上下文会调用此方法。
	 * 
	 * @param cHandle 策略上下文的ID（HFT上下文句柄）
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param bSuccess 是否成功，true表示下单成功，false表示下单失败
	 * @param message 结果消息，如果失败则包含错误信息
	 * @param userTag 用户标签，用于标识订单
	 * 
	 * 实现说明：
	 * - 调用HFT下单结果回调函数，将下单结果转发给外部语言
	 */
	void hft_on_entrust(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag);
	
	/**
	 * @brief HFT策略持仓变化事件处理
	 * 
	 * 当HFT策略的持仓发生变化时，扩展上下文会调用此方法。
	 * 
	 * @param cHandle 策略上下文的ID（HFT上下文句柄）
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param isLong 是否为多头持仓，true表示多头，false表示空头
	 * @param prevol 变化前持仓数量
	 * @param preavail 变化前可用持仓数量
	 * @param newvol 变化后持仓数量
	 * @param newavail 变化后可用持仓数量
	 * 
	 * 实现说明：
	 * - 调用HFT持仓变化回调函数，将持仓变化信息转发给外部语言
	 */
	void hft_on_position(uint32_t cHandle, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail);

	/**
	 * @brief HFT策略委托队列事件处理
	 * 
	 * 当HFT策略收到委托队列数据时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param newOrdQue 委托队列数据指针
	 * 
	 * 实现说明：
	 * - 调用HFT委托队列回调函数，将委托队列数据转发给外部语言
	 */
	void hft_on_order_queue(uint32_t id, const char* stdCode, WTSOrdQueData* newOrdQue);
	
	/**
	 * @brief HFT策略逐笔委托事件处理
	 * 
	 * 当HFT策略收到逐笔委托数据时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param newOrdDtl 逐笔委托数据指针
	 * 
	 * 实现说明：
	 * - 调用HFT逐笔委托回调函数，将逐笔委托数据转发给外部语言
	 */
	void hft_on_order_detail(uint32_t id, const char* stdCode, WTSOrdDtlData* newOrdDtl);
	
	/**
	 * @brief HFT策略逐笔成交事件处理
	 * 
	 * 当HFT策略收到逐笔成交数据时，扩展上下文会调用此方法。
	 * 
	 * @param id 策略上下文的ID
	 * @param stdCode 标准合约代码，如"SHFE.rb2305"
	 * @param newTranns 逐笔成交数据指针
	 * 
	 * 实现说明：
	 * - 调用HFT逐笔成交回调函数，将逐笔成交数据转发给外部语言
	 */
	void hft_on_transaction(uint32_t id, const char* stdCode, WTSTransData* newTranns);

	//////////////////////////////////////////////////////////////////////////
	//工厂加载接口 - 用于加载策略工厂和执行器工厂
	//////////////////////////////////////////////////////////////////////////
	
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
	 * @brief 添加CTA策略工厂目录
	 * 
	 * 从指定目录加载CTA策略工厂动态库。
	 * 
	 * @param folder CTA策略工厂目录路径
	 * @return 加载成功返回true，失败返回false
	 */
	bool addCtaFactories(const char* folder);
	
	/**
	 * @brief 添加HFT策略工厂目录
	 * 
	 * 从指定目录加载HFT策略工厂动态库。
	 * 
	 * @param folder HFT策略工厂目录路径
	 * @return 加载成功返回true，失败返回false
	 */
	bool addHftFactories(const char* folder);
	
	/**
	 * @brief 添加SEL策略工厂目录
	 * 
	 * 从指定目录加载SEL策略工厂动态库。
	 * 
	 * @param folder SEL策略工厂目录路径
	 * @return 加载成功返回true，失败返回false
	 */
	bool addSelFactories(const char* folder);

	//////////////////////////////////////////////////////////////////////////
	//私有初始化方法 - 用于初始化各个组件
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 初始化交易通道（交易适配器）
	 * 
	 * 从配置加载并初始化交易适配器。
	 * 
	 * @param cfgTrader 交易通道配置对象
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 检查配置对象是否有效
	 * 2. 遍历配置数组中的每个交易适配器配置
	 * 3. 检查交易适配器是否启用（active字段）
	 * 4. 创建交易适配器实例并初始化
	 * 5. 将交易适配器添加到管理器
	 */
private:
	bool initTraders(WTSVariant* cfgTrader);
	
	/**
	 * @brief 初始化行情通道（解析器适配器）
	 * 
	 * 从配置加载并初始化解析器适配器。
	 * 
	 * @param cfgParser 行情通道配置对象
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 检查配置对象是否有效
	 * 2. 遍历配置数组中的每个解析器配置
	 * 3. 检查解析器是否启用（active字段）
	 * 4. 如果ID为空，自动生成ID
	 * 5. 创建解析器适配器实例并初始化
	 * 6. 将解析器适配器添加到管理器
	 */
	bool initParsers(WTSVariant* cfgParser);
	
	/**
	 * @brief 初始化执行器
	 * 
	 * 从配置加载并初始化执行器（本地执行器、差分执行器、分布式执行器等）。
	 * 
	 * @param cfgExecuter 执行器配置对象
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 检查配置对象是否有效
	 * 2. 先加载自带的执行器工厂（从executer目录）
	 * 3. 遍历配置数组中的每个执行器配置
	 * 4. 检查执行器是否启用（active字段）
	 * 5. 根据执行器类型（local/diff/dist/arbi）创建对应的执行器实例
	 * 6. 初始化执行器并配置交易通道
	 * 7. 将执行器添加到CTA引擎
	 */
	bool initExecuters(WTSVariant* cfgExecuter);
	
	/**
	 * @brief 初始化数据管理器
	 * 
	 * 从配置初始化数据管理器，注册数据加载器。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 获取数据管理器配置
	 * 2. 将当前对象注册为数据加载器（实现IHisDataLoader接口）
	 * 3. 初始化数据管理器
	 */
	bool initDataMgr();
	
	/**
	 * @brief 初始化事件通知器
	 * 
	 * 从配置初始化事件通知器，用于事件推送。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 获取事件通知器配置
	 * 2. 初始化事件通知器
	 */
	bool initEvtNotifier();
	
	/**
	 * @brief 初始化CTA策略
	 * 
	 * 从配置文件加载CTA策略并创建策略上下文。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 获取策略配置对象
	 * 2. 获取CTA策略配置数组
	 * 3. 遍历配置数组中的每个策略配置
	 * 4. 检查策略是否启用（active字段）
	 * 5. 从策略管理器创建策略实例
	 * 6. 初始化策略（传入参数配置）
	 * 7. 创建策略上下文并设置策略
	 * 8. 将上下文添加到CTA引擎
	 */
	bool initCtaStrategies();
	
	/**
	 * @brief 初始化HFT策略
	 * 
	 * 从配置文件加载HFT策略并创建策略上下文，绑定交易通道。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 获取策略配置对象
	 * 2. 获取HFT策略配置数组
	 * 3. 遍历配置数组中的每个策略配置
	 * 4. 检查策略是否启用（active字段）
	 * 5. 从策略管理器创建策略实例
	 * 6. 初始化策略（传入参数配置）
	 * 7. 创建策略上下文并设置策略
	 * 8. 根据配置绑定交易通道
	 * 9. 将上下文添加到HFT引擎
	 */
	bool initHftStrategies();
	
	/**
	 * @brief 初始化SEL策略
	 * 
	 * 从配置文件加载SEL策略并创建策略上下文，设置调度参数。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 获取策略配置对象
	 * 2. 获取SEL策略配置数组（注意：配置中可能使用"cta"字段名）
	 * 3. 遍历配置数组中的每个策略配置
	 * 4. 检查策略是否启用（active字段）
	 * 5. 解析调度周期字符串
	 * 6. 从策略管理器创建策略实例
	 * 7. 初始化策略（传入参数配置）
	 * 8. 创建策略上下文并设置策略
	 * 9. 将上下文添加到SEL引擎，设置调度参数
	 */
	bool initSelStrategies();
	
	/**
	 * @brief 初始化开平策略管理器
	 * 
	 * 从配置文件加载开平策略。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 从配置中获取开平策略配置文件路径（bspolicy字段）
	 * 2. 调用开平策略管理器的init方法加载策略文件
	 */
	bool initActionPolicy();

	/**
	 * @brief 初始化交易引擎
	 * 
	 * 根据配置选择并初始化对应的交易引擎（CTA、HFT或SEL）。
	 * 
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 获取环境配置（env节点）
	 * 2. 获取引擎名称（name字段）
	 * 3. 根据引擎名称设置引擎类型标志（_is_hft、_is_sel）
	 * 4. 根据引擎类型初始化对应的引擎
	 * 5. 设置引擎的交易适配器管理器
	 * 6. 将引擎指针保存到_engine成员变量
	 */
	bool initEngine();

private:
	//////////////////////////////////////////////////////////////////////////
	//回调函数指针成员变量 - 存储外部语言注册的回调函数
	//////////////////////////////////////////////////////////////////////////
	
	// CTA策略引擎回调函数
	FuncStraInitCallback	_cb_cta_init;  // CTA策略初始化回调函数指针
	FuncSessionEvtCallback	_cb_cta_sessevt;  // CTA策略交易日事件回调函数指针
	FuncStraTickCallback	_cb_cta_tick;  // CTA策略Tick数据回调函数指针
	FuncStraCalcCallback	_cb_cta_calc;  // CTA策略计算回调函数指针
	FuncStraBarCallback		_cb_cta_bar;  // CTA策略K线闭合回调函数指针
	FuncStraCondTriggerCallback _cb_cta_cond_trigger;  // CTA策略条件单触发回调函数指针

	// SEL策略引擎回调函数
	FuncStraInitCallback	_cb_sel_init;  // SEL策略初始化回调函数指针
	FuncSessionEvtCallback	_cb_sel_sessevt;  // SEL策略交易日事件回调函数指针
	FuncStraTickCallback	_cb_sel_tick;  // SEL策略Tick数据回调函数指针
	FuncStraCalcCallback	_cb_sel_calc;  // SEL策略计算回调函数指针
	FuncStraBarCallback		_cb_sel_bar;  // SEL策略K线闭合回调函数指针

	// HFT策略引擎回调函数
	FuncStraInitCallback	_cb_hft_init;  // HFT策略初始化回调函数指针
	FuncSessionEvtCallback	_cb_hft_sessevt;  // HFT策略交易日事件回调函数指针
	FuncStraTickCallback	_cb_hft_tick;  // HFT策略Tick数据回调函数指针
	FuncStraBarCallback		_cb_hft_bar;  // HFT策略K线闭合回调函数指针
	FuncHftChannelCallback	_cb_hft_chnl;  // HFT策略交易通道事件回调函数指针
	FuncHftOrdCallback		_cb_hft_ord;  // HFT策略订单回报回调函数指针
	FuncHftTrdCallback		_cb_hft_trd;  // HFT策略成交回报回调函数指针
	FuncHftEntrustCallback	_cb_hft_entrust;  // HFT策略下单结果回调函数指针
	FuncHftPosCallback		_cb_hft_position;  // HFT策略持仓变化回调函数指针

	// HFT策略Level2数据回调函数
	FuncStraOrdQueCallback	_cb_hft_ordque;  // HFT策略委托队列回调函数指针
	FuncStraOrdDtlCallback	_cb_hft_orddtl;  // HFT策略逐笔委托回调函数指针
	FuncStraTransCallback	_cb_hft_trans;  // HFT策略逐笔成交回调函数指针

	// 引擎事件回调函数
	FuncEventCallback		_cb_evt;  // 引擎事件回调函数指针（初始化、调度、交易日事件等）

	// 扩展Parser回调函数
	FuncParserEvtCallback	_cb_parser_evt;  // Parser事件回调函数指针（初始化、连接、断开等）
	FuncParserSubCallback	_cb_parser_sub;  // Parser订阅回调函数指针（订阅、取消订阅合约）

	// 扩展Executer回调函数
	FuncExecCmdCallback		_cb_exec_cmd;  // Executer命令回调函数指针（设置目标仓位）
	FuncExecInitCallback	_cb_exec_init;  // Executer初始化回调函数指针

	//////////////////////////////////////////////////////////////////////////
	//核心组件成员变量 - 管理系统的各个组件
	//////////////////////////////////////////////////////////////////////////
	
	WTSVariant*			_config;  // 配置对象指针，存储加载的配置文件内容
	TraderAdapterMgr	_traders;  // 交易适配器管理器，管理多个交易通道
	ParserAdapterMgr	_parsers;  // 解析器适配器管理器，管理多个行情通道
	WtExecuterFactory	_exe_factory;  // 执行器工厂，创建和管理执行器实例

	// 交易引擎对象
	WtCtaEngine			_cta_engine;  // CTA策略引擎实例
	WtHftEngine			_hft_engine;  // HFT策略引擎实例
	WtSelEngine			_sel_engine;  // SEL策略引擎实例
	WtEngine*			_engine;  // 当前使用的交易引擎指针（指向_cta_engine、_hft_engine或_sel_engine之一）

	WtDataStorage*		_data_store;  // 数据存储对象指针（当前未使用，保留用于未来扩展）

	WtDtMgr				_data_mgr;  // 数据管理器，管理行情数据和K线数据

	// 基础数据管理器
	WTSBaseDataMgr		_bd_mgr;  // 基础数据管理器，管理商品、合约、交易时段等基础数据
	WTSHotMgr			_hot_mgr;  // 热点合约管理器，管理主力合约、次主力合约等
	EventNotifier		_notifier;  // 事件通知器，用于事件推送和日志通知

	// 策略管理器
	CtaStrategyMgr		_cta_mgr;  // CTA策略管理器，创建和管理CTA策略实例
	HftStrategyMgr		_hft_mgr;  // HFT策略管理器，创建和管理HFT策略实例
	SelStrategyMgr		_sel_mgr;  // SEL策略管理器，创建和管理SEL策略实例
	ActionPolicyMgr		_act_policy;  // 开平策略管理器，管理开仓和平仓策略

	//////////////////////////////////////////////////////////////////////////
	//状态标志成员变量 - 标识系统的运行状态
	//////////////////////////////////////////////////////////////////////////
	
	bool				_is_hft;  // 是否为高频引擎标志，true表示使用HFT引擎，false表示使用CTA或SEL引擎
	bool				_is_sel;  // 是否为选股引擎标志，true表示使用SEL引擎，false表示使用CTA或HFT引擎
	bool				_to_exit;  // 退出标志，true表示系统需要退出，false表示系统正常运行

	//////////////////////////////////////////////////////////////////////////
	//外部数据加载器成员变量 - 用于外部数据源加载历史数据
	//////////////////////////////////////////////////////////////////////////
	
	FuncLoadFnlBars		_ext_fnl_bar_loader;  // 复权K线数据加载器函数指针，用于加载复权后的K线数据
	FuncLoadRawBars		_ext_raw_bar_loader;  // 原始K线数据加载器函数指针，用于加载原始（未复权）K线数据
	FuncLoadAdjFactors	_ext_adj_fct_loader;  // 复权因子加载器函数指针，用于加载复权因子数据

	//////////////////////////////////////////////////////////////////////////
	//数据推送相关成员变量 - 用于数据加载器推送数据给数据管理器
	//////////////////////////////////////////////////////////////////////////
	
	void*			_feed_obj;  // 数据接收对象指针，用于回调函数传递（由数据管理器传入）
	FuncReadBars	_feeder_bars;  // K线数据读取回调函数指针，当数据加载完成后调用此函数推送K线数据
	FuncReadFactors	_feeder_fcts;  // 复权因子读取回调函数指针，当数据加载完成后调用此函数推送复权因子数据
	StdUniqueMutex	_feed_mtx;  // 数据推送互斥锁，保护数据推送相关的共享资源（线程安全）
};

