/*!
 * \file WtRtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtRtRunner运行时运行器类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtRtRunner类的所有方法，包括系统初始化、配置、运行、资源释放等功能。
 * WtRtRunner是整个WtPorter模块的核心，负责协调各个组件的工作，将外部语言的调用
 * 转换为对内部引擎的操作，并将引擎的事件转发给外部语言。
 * 
 * 主要功能实现：
 * 1. 系统初始化：初始化日志系统、设置安装目录、启用崩溃转储功能
 * 2. 系统配置：加载配置文件、初始化各组件（引擎、数据管理器、交易通道、行情通道、执行器等）
 * 3. 回调函数注册：注册各种策略和组件的事件回调函数
 * 4. 策略上下文创建：创建CTA、HFT、SEL策略的扩展上下文实例
 * 5. 事件转发：将引擎事件转发给外部语言注册的回调函数
 * 6. 数据加载：实现IHisDataLoader接口，支持外部数据源加载历史数据
 * 7. 扩展组件管理：创建和管理外部语言实现的Parser和Executer
 * 8. 系统运行：启动行情通道、交易通道和交易引擎的运行
 * 9. 资源释放：清理系统占用的资源，停止日志系统
 * 
 * 设计特点：
 * - 事件驱动：通过回调函数机制实现事件驱动的编程模型
 * - 多引擎支持：支持CTA、HFT、SEL三种策略引擎
 * - 扩展性：支持外部数据加载器和扩展组件（Parser、Executer）
 * - 线程安全：使用互斥锁保护数据加载相关的共享资源
 * - 异常处理：使用try-catch捕获异常，确保程序稳定运行
 * - 信号处理：安装信号钩子，捕获程序异常和退出信号
 * 
 * 数据流程：
 * 1. 外部语言调用C接口函数（WtPorter.h中定义）
 * 2. C接口函数调用WtRtRunner的对应方法
 * 3. WtRtRunner将操作转发给对应的引擎或管理器
 * 4. 引擎或管理器产生事件，调用WtRtRunner的事件处理方法
 * 5. WtRtRunner将事件转发给外部语言注册的回调函数
 */
#include "WtRtRunner.h"
#include "ExpCtaContext.h"
#include "ExpSelContext.h"
#include "ExpHftContext.h"

#include "ExpParser.h"
#include "ExpExecuter.h"

#include "../WtCore/WtHelper.h"
#include "../WtCore/CtaStraContext.h"
#include "../WtCore/HftStraContext.h"
#include "../WtCore/SelStraContext.h"

#include "../WTSTools/WTSLogger.h"
#include "../WTSUtils/WTSCfgLoader.h"
#include "../WTSUtils/SignalHook.hpp"

#include "../Share/TimeUtils.hpp"
#include "../Share/ModuleHelper.hpp"

#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSVariant.hpp"

#ifdef _MSC_VER
#include "../Common/mdump.h"
#include <boost/filesystem.hpp>
 //这个主要是给MiniDumper用的
const char* getModuleName()
{
	static char MODULE_NAME[250] = { 0 };
	if (strlen(MODULE_NAME) == 0)
	{
		GetModuleFileName(g_dllModule, MODULE_NAME, 250);
		boost::filesystem::path p(MODULE_NAME);
		strcpy(MODULE_NAME, p.filename().string().c_str());
	}

	return MODULE_NAME;
}
#endif

/**
 * @brief 构造函数实现
 * 
 * 创建运行时运行器实例，使用初始化列表初始化所有成员变量为默认值。
 * 所有回调函数指针初始化为NULL，标志变量初始化为false。
 * 
 * 初始化说明：
 * - 回调函数指针：全部初始化为NULL，表示未注册回调函数
 * - 配置对象指针：初始化为NULL，需要在config()方法中加载配置
 * - 引擎指针：初始化为NULL，需要在initEngine()方法中设置
 * - 标志变量：_is_hft和_is_sel初始化为false，_to_exit初始化为false
 * - 外部数据加载器：全部初始化为NULL，表示未注册外部数据加载器
 * - 数据推送相关：_feed_obj初始化为NULL，互斥锁会自动初始化
 */
WtRtRunner::WtRtRunner()
	: _data_store(NULL)  // 数据存储对象指针初始化为NULL（当前未使用）
	, _cb_cta_init(NULL)  // CTA策略初始化回调函数指针初始化为NULL
	, _cb_cta_tick(NULL)  // CTA策略Tick数据回调函数指针初始化为NULL
	, _cb_cta_calc(NULL)  // CTA策略计算回调函数指针初始化为NULL
	, _cb_cta_bar(NULL)  // CTA策略K线闭合回调函数指针初始化为NULL
	, _cb_cta_cond_trigger(NULL)  // CTA策略条件单触发回调函数指针初始化为NULL
	, _cb_cta_sessevt(NULL)  // CTA策略交易日事件回调函数指针初始化为NULL

	, _cb_sel_init(NULL)  // SEL策略初始化回调函数指针初始化为NULL
	, _cb_sel_tick(NULL)  // SEL策略Tick数据回调函数指针初始化为NULL
	, _cb_sel_calc(NULL)  // SEL策略计算回调函数指针初始化为NULL
	, _cb_sel_bar(NULL)  // SEL策略K线闭合回调函数指针初始化为NULL
	, _cb_sel_sessevt(NULL)  // SEL策略交易日事件回调函数指针初始化为NULL

	, _cb_hft_init(NULL)  // HFT策略初始化回调函数指针初始化为NULL
	, _cb_hft_tick(NULL)  // HFT策略Tick数据回调函数指针初始化为NULL
	, _cb_hft_bar(NULL)  // HFT策略K线闭合回调函数指针初始化为NULL
	, _cb_hft_ord(NULL)  // HFT策略订单回报回调函数指针初始化为NULL
	, _cb_hft_trd(NULL)  // HFT策略成交回报回调函数指针初始化为NULL
	, _cb_hft_entrust(NULL)  // HFT策略下单结果回调函数指针初始化为NULL
	, _cb_hft_chnl(NULL)  // HFT策略交易通道事件回调函数指针初始化为NULL

	, _cb_hft_orddtl(NULL)  // HFT策略逐笔委托回调函数指针初始化为NULL
	, _cb_hft_ordque(NULL)  // HFT策略委托队列回调函数指针初始化为NULL
	, _cb_hft_trans(NULL)  // HFT策略逐笔成交回调函数指针初始化为NULL
	, _cb_hft_position(NULL)  // HFT策略持仓变化回调函数指针初始化为NULL
	, _cb_hft_sessevt(NULL)  // HFT策略交易日事件回调函数指针初始化为NULL

	, _cb_exec_cmd(NULL)  // Executer命令回调函数指针初始化为NULL
	, _cb_exec_init(NULL)  // Executer初始化回调函数指针初始化为NULL

	, _cb_parser_evt(NULL)  // Parser事件回调函数指针初始化为NULL
	, _cb_parser_sub(NULL)  // Parser订阅回调函数指针初始化为NULL

	, _cb_evt(NULL)  // 引擎事件回调函数指针初始化为NULL
	, _is_hft(false)  // 是否为高频引擎标志初始化为false
	, _is_sel(false)  // 是否为选股引擎标志初始化为false

	, _ext_fnl_bar_loader(NULL)  // 复权K线数据加载器函数指针初始化为NULL
	, _ext_raw_bar_loader(NULL)  // 原始K线数据加载器函数指针初始化为NULL
	, _ext_adj_fct_loader(NULL)  // 复权因子加载器函数指针初始化为NULL

	, _to_exit(false)  // 退出标志初始化为false
{
	// 构造函数体为空，所有初始化都在初始化列表中完成
}


/**
 * @brief 析构函数实现
 * 
 * 清理运行时运行器占用的资源。
 * 注意：析构函数不负责释放引擎和管理器对象，这些对象会在系统停止时自动清理。
 * 
 * 清理说明：
 * - 智能指针管理的对象会自动释放
 * - 配置对象需要调用者负责释放（通常在release()方法中处理）
 * - 日志系统会在release()方法中停止
 */
WtRtRunner::~WtRtRunner()
{
	// 析构函数体为空，智能指针管理的对象会自动释放
}

/**
 * @brief 初始化运行时运行器实现
 * 
 * 初始化日志系统和运行环境，设置安装目录和生成目录。
 * 在Windows平台启用MiniDump崩溃转储功能。
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
 * - 如果日志配置加载失败，会记录错误但不会阻止初始化
 */
bool WtRtRunner::init(const char* logCfg /* = "logcfg.prop" */, bool isFile /* = true */, const char* genDir)
{
#ifdef _MSC_VER  // 如果是Microsoft Visual C++编译器（Windows平台）
	// 启用MiniDump崩溃转储功能，用于程序崩溃时生成转储文件，便于问题排查
	CMiniDumper::Enable(getModuleName(), true, WtHelper::getCWD().c_str());
	// 参数说明：
	// - getModuleName(): 模块名称，用于转储文件命名
	// - true: 是否启用完整转储
	// - WtHelper::getCWD().c_str(): 转储文件保存目录（当前工作目录）
#endif

	if(isFile)  // 如果logCfg是文件路径
	{
		std::string path = WtHelper::getCWD() + logCfg;  // 拼接当前工作目录和配置文件路径
		WTSLogger::init(path.c_str(), true, this);  // 从文件初始化日志系统，this作为日志处理器
		// 参数说明：
		// - path.c_str(): 日志配置文件路径
		// - true: 表示从文件加载配置
		// - this: 日志处理器指针（实现ILogHandler接口）
	}
	else  // 如果logCfg是配置内容（JSON字符串）
	{
		WTSLogger::init(logCfg, false, this);  // 从字符串初始化日志系统，this作为日志处理器
		// 参数说明：
		// - logCfg: 日志配置内容（JSON字符串）
		// - false: 表示从字符串加载配置
		// - this: 日志处理器指针（实现ILogHandler接口）
	}
	

	WtHelper::setInstDir(getBinDir());  // 设置安装目录路径，用于查找动态库和配置文件
	// getBinDir()返回可执行文件所在目录，通常是bin目录
	
	WtHelper::setGenerateDir(StrUtil::standardisePath(genDir).c_str());  // 设置生成目录路径，用于存储生成的文件
	// StrUtil::standardisePath()标准化路径格式（统一使用正斜杠或反斜杠）
	
	return true;  // 初始化成功，返回true
}

/**
 * @brief 注册引擎事件回调函数实现
 * 
 * 注册引擎级别的事件回调函数（初始化、调度、交易日事件等），并将当前对象注册为三个引擎的事件监听器。
 * 
 * @param cbEvt 引擎事件回调函数指针，当引擎事件发生时调用
 * 
 * 实现流程：
 * 1. 保存回调函数指针到成员变量
 * 2. 将当前对象注册为CTA引擎的事件监听器
 * 3. 将当前对象注册为HFT引擎的事件监听器
 * 4. 将当前对象注册为SEL引擎的事件监听器
 * 
 * 事件类型：
 * - EVENT_ENGINE_INIT：引擎初始化完成
 * - EVENT_ENGINE_SCHDL：引擎调度事件（定时触发）
 * - EVENT_SESSION_BEGIN：交易日开始
 * - EVENT_SESSION_END：交易日结束
 * 
 * 注意事项：
 * - 注册后，引擎事件会通过IEngineEvtListener接口方法触发
 * - 回调函数可以为NULL，表示不处理引擎事件
 */
void WtRtRunner::registerEvtCallback(FuncEventCallback cbEvt)
{
	_cb_evt = cbEvt;  // 保存引擎事件回调函数指针

	_cta_engine.regEventListener(this);  // 将当前对象注册为CTA引擎的事件监听器
	_hft_engine.regEventListener(this);  // 将当前对象注册为HFT引擎的事件监听器
	_sel_engine.regEventListener(this);  // 将当前对象注册为SEL引擎的事件监听器
	// 注册后，引擎事件会通过IEngineEvtListener接口方法（on_initialize_event、on_schedule_event、on_session_event）触发
}

/**
 * @brief 注册扩展Parser的回调函数实现
 * 
 * 注册扩展Parser（外部语言实现的行情解析器）的事件回调函数。
 * 
 * @param cbEvt Parser事件回调函数指针，当Parser事件发生时调用（初始化、连接、断开等）
 * @param cbSub Parser订阅回调函数指针，当需要订阅或取消订阅合约时调用
 * 
 * 事件类型：
 * - EVENT_PARSER_INIT：Parser初始化
 * - EVENT_PARSER_CONNECT：Parser连接成功
 * - EVENT_PARSER_DISCONNECT：Parser断开连接
 * - EVENT_PARSER_RELEASE：Parser释放
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理对应的事件
 * - 注册完成后会记录日志
 */
void WtRtRunner::registerParserPorter(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub)
{
	_cb_parser_evt = cbEvt;  // 保存Parser事件回调函数指针
	_cb_parser_sub = cbSub;  // 保存Parser订阅回调函数指针

	WTSLogger::info("Callbacks of Extented Parser registration done");  // 记录日志，表示扩展Parser回调函数注册完成
}

/**
 * @brief 注册扩展Executer的回调函数实现
 * 
 * 注册扩展Executer（外部语言实现的执行器）的事件回调函数。
 * 
 * @param cbInit Executer初始化回调函数指针，当Executer初始化时调用
 * @param cbExec Executer命令回调函数指针，当需要执行交易命令时调用
 * 
 * 使用场景：
 * - 外部语言实现自定义执行器时，需要注册这些回调函数
 * - 执行器初始化时会调用cbInit
 * - 需要设置目标仓位时会调用cbExec
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理对应的事件
 * - 注册完成后会记录日志
 */
void WtRtRunner::registerExecuterPorter(FuncExecInitCallback cbInit, FuncExecCmdCallback cbExec)
{
	_cb_exec_init = cbInit;  // 保存Executer初始化回调函数指针
	_cb_exec_cmd = cbExec;  // 保存Executer命令回调函数指针

	WTSLogger::info("Callbacks of Extented Executer registration done");  // 记录日志，表示扩展Executer回调函数注册完成
}

/**
 * @brief 注册CTA策略引擎的回调函数实现
 * 
 * 注册CTA策略引擎的各种事件回调函数，当策略事件发生时，会调用对应的回调函数。
 * 
 * @param cbInit 策略初始化回调函数指针，当策略初始化完成时调用
 * @param cbTick Tick数据回调函数指针，当收到新的Tick数据时调用
 * @param cbCalc 策略计算回调函数指针，当策略需要计算时调用（定时计算）
 * @param cbBar K线闭合回调函数指针，当K线闭合时调用
 * @param cbSessEvt 交易日事件回调函数指针，当交易日开始或结束时调用
 * @param cbCondTrigger 条件单触发回调函数指针，当条件单触发时调用（可选，默认为NULL）
 * 
 * 实现流程：
 * 1. 保存所有回调函数指针到对应的成员变量
 * 2. 记录日志，表示CTA引擎回调函数注册完成
 * 
 * 注意事项：
 * - 所有回调函数都可以为NULL，表示不处理对应的事件
 * - 回调函数会在策略上下文的事件处理方法中被调用
 * - 回调函数的参数格式必须符合PorterDefs.h中定义的类型
 */
void WtRtRunner::registerCtaCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, 
		FuncSessionEvtCallback cbSessEvt, FuncStraCondTriggerCallback cbCondTrigger /* = NULL */)
{
	_cb_cta_init = cbInit;  // 保存CTA策略初始化回调函数指针
	_cb_cta_tick = cbTick;  // 保存CTA策略Tick数据回调函数指针
	_cb_cta_calc = cbCalc;  // 保存CTA策略计算回调函数指针
	_cb_cta_bar = cbBar;  // 保存CTA策略K线闭合回调函数指针
	_cb_cta_sessevt = cbSessEvt;  // 保存CTA策略交易日事件回调函数指针
	_cb_cta_cond_trigger = cbCondTrigger;  // 保存CTA策略条件单触发回调函数指针

	WTSLogger::info("Callbacks of CTA engine registration done");  // 记录日志，表示CTA引擎回调函数注册完成
}

/**
 * @brief 注册SEL策略引擎的回调函数实现
 * 
 * 注册SEL（选股）策略引擎的各种事件回调函数。
 * 
 * @param cbInit 策略初始化回调函数指针，当策略初始化完成时调用
 * @param cbTick Tick数据回调函数指针，当收到新的Tick数据时调用
 * @param cbCalc 策略计算回调函数指针，当策略需要计算时调用（定时计算）
 * @param cbBar K线闭合回调函数指针，当K线闭合时调用
 * @param cbSessEvt 交易日事件回调函数指针，当交易日开始或结束时调用
 * 
 * 实现流程：
 * 1. 保存所有回调函数指针到对应的成员变量
 * 2. 记录日志，表示SEL引擎回调函数注册完成
 * 
 * 注意事项：
 * - SEL策略不支持条件单触发回调
 * - 其他说明同registerCtaCallbacks()
 */
void WtRtRunner::registerSelCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt)
{
	_cb_sel_init = cbInit;  // 保存SEL策略初始化回调函数指针
	_cb_sel_tick = cbTick;  // 保存SEL策略Tick数据回调函数指针
	_cb_sel_calc = cbCalc;  // 保存SEL策略计算回调函数指针
	_cb_sel_bar = cbBar;  // 保存SEL策略K线闭合回调函数指针

	_cb_sel_sessevt = cbSessEvt;  // 保存SEL策略交易日事件回调函数指针

	WTSLogger::info("Callbacks of SEL engine registration done");  // 记录日志，表示SEL引擎回调函数注册完成
}

/**
 * @brief 注册HFT策略引擎的回调函数实现
 * 
 * 注册HFT（高频交易）策略引擎的各种事件回调函数。
 * 
 * @param cbInit 策略初始化回调函数指针，当策略初始化完成时调用
 * @param cbTick Tick数据回调函数指针，当收到新的Tick数据时调用
 * @param cbBar K线闭合回调函数指针，当K线闭合时调用
 * @param cbChnl 交易通道事件回调函数指针，当交易通道就绪或丢失时调用
 * @param cbOrd 订单回报回调函数指针，当订单状态变化时调用
 * @param cbTrd 成交回报回调函数指针，当订单成交时调用
 * @param cbEntrust 下单结果回调函数指针，当下单成功或失败时调用
 * @param cbOrdDtl 逐笔委托回调函数指针，当收到逐笔委托数据时调用
 * @param cbOrdQue 委托队列回调函数指针，当收到委托队列数据时调用
 * @param cbTrans 逐笔成交回调函数指针，当收到逐笔成交数据时调用
 * @param cbSessEvt 交易日事件回调函数指针，当交易日开始或结束时调用
 * @param cbPosition 持仓变化回调函数指针，当持仓发生变化时调用
 * 
 * 实现流程：
 * 1. 保存所有回调函数指针到对应的成员变量
 * 2. 记录日志，表示HFT引擎回调函数注册完成
 * 
 * 注意事项：
 * - HFT策略支持更多的事件类型（订单、成交、Level2数据等）
 * - 所有回调函数都可以为NULL，表示不处理对应的事件
 * - 其他说明同registerCtaCallbacks()
 */
void WtRtRunner::registerHftCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar, 
	FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust,
	FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt, FuncHftPosCallback cbPosition)
{
	_cb_hft_init = cbInit;  // 保存HFT策略初始化回调函数指针
	_cb_hft_tick = cbTick;  // 保存HFT策略Tick数据回调函数指针
	_cb_hft_bar = cbBar;  // 保存HFT策略K线闭合回调函数指针

	_cb_hft_chnl = cbChnl;  // 保存HFT策略交易通道事件回调函数指针
	_cb_hft_ord = cbOrd;  // 保存HFT策略订单回报回调函数指针
	_cb_hft_trd = cbTrd;  // 保存HFT策略成交回报回调函数指针
	_cb_hft_entrust = cbEntrust;  // 保存HFT策略下单结果回调函数指针

	_cb_hft_orddtl = cbOrdDtl;  // 保存HFT策略逐笔委托回调函数指针
	_cb_hft_ordque = cbOrdQue;  // 保存HFT策略委托队列回调函数指针
	_cb_hft_trans = cbTrans;  // 保存HFT策略逐笔成交回调函数指针

	_cb_hft_sessevt = cbSessEvt;  // 保存HFT策略交易日事件回调函数指针

	_cb_hft_position = cbPosition;  // 保存HFT策略持仓变化回调函数指针

	WTSLogger::info("Callbacks of HFT engine registration done");  // 记录日志，表示HFT引擎回调函数注册完成
}

/**
 * @brief 加载复权后的历史K线数据实现（IHisDataLoader接口实现）
 * 
 * 从外部数据加载器加载指定合约的复权后历史K线数据。
 * 
 * @param obj 数据接收对象指针，用于回调函数传递（由数据管理器传入）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param period K线周期（KP_DAY、KP_Minute1、KP_Minute5等）
 * @param cb 数据读取回调函数指针，当数据加载完成后调用此函数推送K线数据
 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
 * 
 * 实现流程：
 * 1. 获取互斥锁，保护数据推送相关的共享资源（线程安全）
 * 2. 检查外部复权K线数据加载器是否已注册
 * 3. 如果未注册，返回false
 * 4. 保存数据接收对象指针和回调函数指针
 * 5. 将K线周期转换为字符串格式（"d1"、"m1"、"m5"）
 * 6. 调用外部注册的复权K线加载器函数
 * 7. 数据加载完成后，外部加载器会通过feedRawBars()推送数据
 * 
 * 注意事项：
 * - 使用互斥锁保护共享资源，确保线程安全
 * - 只支持日线（KP_DAY）、1分钟线（KP_Minute1）和5分钟线（KP_Minute5）
 * - 其他周期会记录错误日志并返回false
 */
bool WtRtRunner::loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb)
{
	StdUniqueLock lock(_feed_mtx);  // 获取互斥锁，保护数据推送相关的共享资源（线程安全）
	if (_ext_fnl_bar_loader == NULL)  // 如果外部复权K线数据加载器未注册
		return false;  // 返回false，表示加载失败

	_feed_obj = obj;  // 保存数据接收对象指针，用于回调函数传递
	_feeder_bars = cb;  // 保存K线数据读取回调函数指针，当数据加载完成后调用此函数推送数据

	switch (period)  // 根据K线周期选择对应的字符串格式
	{
	case KP_DAY:  // 如果是日线周期
		return _ext_fnl_bar_loader(stdCode, "d1");  // 调用外部加载器，传入合约代码和周期字符串"d1"
	case KP_Minute1:  // 如果是1分钟线周期
		return _ext_fnl_bar_loader(stdCode, "m1");  // 调用外部加载器，传入合约代码和周期字符串"m1"
	case KP_Minute5:  // 如果是5分钟线周期
		return _ext_fnl_bar_loader(stdCode, "m5");  // 调用外部加载器，传入合约代码和周期字符串"m5"
	default:  // 如果是其他不支持的周期
	{
		WTSLogger::error("Unsupported period of extended data loader");  // 记录错误日志，表示不支持的周期
		return false;  // 返回false，表示加载失败
	}
	}
}

/**
 * @brief 加载原始历史K线数据实现（IHisDataLoader接口实现）
 * 
 * 从外部数据加载器加载指定合约的原始（未复权）历史K线数据。
 * 
 * @param obj 数据接收对象指针，用于回调函数传递（由数据管理器传入）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param period K线周期（KP_DAY、KP_Minute1、KP_Minute5等）
 * @param cb 数据读取回调函数指针，当数据加载完成后调用此函数推送K线数据
 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
 * 
 * 实现流程：
 * 1. 获取互斥锁，保护数据推送相关的共享资源（线程安全）
 * 2. 检查外部原始K线数据加载器是否已注册
 * 3. 如果未注册，返回false
 * 4. 保存数据接收对象指针和回调函数指针
 * 5. 将K线周期转换为字符串格式（"d1"、"m1"、"m5"）
 * 6. 调用外部注册的原始K线加载器函数
 * 7. 数据加载完成后，外部加载器会通过feedRawBars()推送数据
 * 
 * 注意事项：
 * - 使用互斥锁保护共享资源，确保线程安全
 * - 只支持日线（KP_DAY）、1分钟线（KP_Minute1）和5分钟线（KP_Minute5）
 * - 其他周期会记录错误日志并返回false
 * - 原始K线数据是未复权的，复权K线数据是经过复权处理的
 */
bool WtRtRunner::loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb)
{
	StdUniqueLock lock(_feed_mtx);  // 获取互斥锁，保护数据推送相关的共享资源（线程安全）
	if (_ext_raw_bar_loader == NULL)  // 如果外部原始K线数据加载器未注册
		return false;  // 返回false，表示加载失败

	_feed_obj = obj;  // 保存数据接收对象指针，用于回调函数传递
	_feeder_bars = cb;  // 保存K线数据读取回调函数指针，当数据加载完成后调用此函数推送数据

	switch (period)  // 根据K线周期选择对应的字符串格式
	{
	case KP_DAY:  // 如果是日线周期
		return _ext_raw_bar_loader(stdCode, "d1");  // 调用外部加载器，传入合约代码和周期字符串"d1"
	case KP_Minute1:  // 如果是1分钟线周期
		return _ext_raw_bar_loader(stdCode, "m1");  // 调用外部加载器，传入合约代码和周期字符串"m1"
	case KP_Minute5:  // 如果是5分钟线周期
		return _ext_raw_bar_loader(stdCode, "m5");  // 调用外部加载器，传入合约代码和周期字符串"m5"
	default:  // 如果是其他不支持的周期
	{
		WTSLogger::error("Unsupported period of extended data loader");  // 记录错误日志，表示不支持的周期
		return false;  // 返回false，表示加载失败
	}
	}
}

/**
 * @brief 加载所有合约的复权因子数据实现（IHisDataLoader接口实现）
 * 
 * 从外部数据加载器加载所有合约的复权因子数据。
 * 
 * @param obj 数据接收对象指针，用于回调函数传递（由数据管理器传入）
 * @param cb 复权因子读取回调函数指针，当数据加载完成后调用此函数推送复权因子数据
 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
 * 
 * 实现流程：
 * 1. 获取互斥锁，保护数据推送相关的共享资源（线程安全）
 * 2. 检查外部复权因子加载器是否已注册
 * 3. 如果未注册，返回false
 * 4. 保存数据接收对象指针和回调函数指针
 * 5. 调用外部注册的复权因子加载器函数，传入空字符串表示加载所有合约
 * 6. 数据加载完成后，外部加载器会通过feedAdjFactors()推送数据
 * 
 * 注意事项：
 * - 使用互斥锁保护共享资源，确保线程安全
 * - 传入空字符串表示加载所有合约的复权因子数据
 * - 数据加载完成后，外部加载器会逐个合约调用feedAdjFactors()推送数据
 */
bool WtRtRunner::loadAllAdjFactors(void* obj, FuncReadFactors cb)
{
	StdUniqueLock lock(_feed_mtx);  // 获取互斥锁，保护数据推送相关的共享资源（线程安全）
	if (_ext_adj_fct_loader == NULL)  // 如果外部复权因子加载器未注册
		return false;  // 返回false，表示加载失败

	_feed_obj = obj;  // 保存数据接收对象指针，用于回调函数传递
	_feeder_fcts = cb;  // 保存复权因子读取回调函数指针，当数据加载完成后调用此函数推送复权因子数据

	return _ext_adj_fct_loader("");  // 调用外部加载器，传入空字符串表示加载所有合约的复权因子数据
}

/**
 * @brief 加载指定合约的复权因子数据实现（IHisDataLoader接口实现）
 * 
 * 从外部数据加载器加载指定合约的复权因子数据。
 * 
 * @param obj 数据接收对象指针，用于回调函数传递（由数据管理器传入）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param cb 复权因子读取回调函数指针，当数据加载完成后调用此函数推送复权因子数据
 * @return 如果外部数据加载器已注册且调用成功返回true，否则返回false
 * 
 * 实现流程：
 * 1. 获取互斥锁，保护数据推送相关的共享资源（线程安全）
 * 2. 检查外部复权因子加载器是否已注册
 * 3. 如果未注册，返回false
 * 4. 保存数据接收对象指针和回调函数指针
 * 5. 调用外部注册的复权因子加载器函数，传入合约代码
 * 6. 数据加载完成后，外部加载器会通过feedAdjFactors()推送数据
 * 
 * 注意事项：
 * - 使用互斥锁保护共享资源，确保线程安全
 * - 传入合约代码表示加载指定合约的复权因子数据
 * - 数据加载完成后，外部加载器会调用feedAdjFactors()推送数据
 */
bool WtRtRunner::loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb)
{
	StdUniqueLock lock(_feed_mtx);  // 获取互斥锁，保护数据推送相关的共享资源（线程安全）
	if (_ext_adj_fct_loader == NULL)  // 如果外部复权因子加载器未注册
		return false;  // 返回false，表示加载失败

	_feed_obj = obj;  // 保存数据接收对象指针，用于回调函数传递
	_feeder_fcts = cb;  // 保存复权因子读取回调函数指针，当数据加载完成后调用此函数推送复权因子数据

	return _ext_adj_fct_loader(stdCode);  // 调用外部加载器，传入合约代码，加载指定合约的复权因子数据
}

/**
 * @brief 推送复权因子数据实现
 * 
 * 当外部数据加载器加载完复权因子数据后，调用此函数将数据推送给数据管理器。
 * 
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param dates 复权日期数组指针（格式：YYYYMMDD），每个元素对应一个复权日期
 * @param factors 复权因子数组指针，每个元素对应一个复权因子值
 * @param count 复权因子数据条数，表示dates和factors数组的长度
 * 
 * 实现流程：
 * 1. 调用之前保存的复权因子读取回调函数，将数据推送给数据管理器
 * 2. 回调函数参数：数据接收对象指针、合约代码、复权日期数组、复权因子数组、数据条数
 * 
 * 使用场景：
 * - 外部数据加载器加载完复权因子数据后，调用此函数推送数据
 * - 数据会通过之前注册的回调函数传递给数据管理器
 * 
 * 注意事项：
 * - 此函数由外部数据加载器调用，不是由数据管理器调用
 * - dates和factors数组的长度必须等于count
 * - 数组的内存由调用者负责管理
 */
void WtRtRunner::feedAdjFactors(const char* stdCode, uint32_t* dates, double* factors, uint32_t count)
{
	_feeder_fcts(_feed_obj, stdCode, dates, factors, count);  // 调用复权因子读取回调函数，将数据推送给数据管理器
	// 参数说明：
	// - _feed_obj: 数据接收对象指针（由数据管理器传入）
	// - stdCode: 标准合约代码
	// - dates: 复权日期数组指针
	// - factors: 复权因子数组指针
	// - count: 复权因子数据条数
}


/**
 * @brief 推送原始K线数据实现
 * 
 * 当外部数据加载器加载完K线数据后，调用此函数将数据推送给数据管理器。
 * 
 * @param bars K线数据数组指针，包含多个K线数据结构
 * @param count K线数据条数，表示bars数组的长度
 * 
 * 实现流程：
 * 1. 检查外部复权K线数据加载器是否已注册（用于验证）
 * 2. 如果未注册，记录错误日志并返回
 * 3. 调用之前保存的K线数据读取回调函数，将数据推送给数据管理器
 * 4. 回调函数参数：数据接收对象指针、K线数据数组、数据条数
 * 
 * 使用场景：
 * - 外部数据加载器加载完K线数据后，调用此函数推送数据
 * - 数据会通过之前注册的回调函数传递给数据管理器
 * 
 * 注意事项：
 * - 此函数由外部数据加载器调用，不是由数据管理器调用
 * - bars数组的长度必须等于count
 * - 数组的内存由调用者负责管理
 * - 虽然函数名是feedRawBars，但实际用于推送复权后的K线数据（因为检查的是_ext_fnl_bar_loader）
 */
void WtRtRunner::feedRawBars(WTSBarStruct* bars, uint32_t count)
{
	if (_ext_fnl_bar_loader == NULL)  // 如果外部复权K线数据加载器未注册（用于验证）
	{
		WTSLogger::error("Cannot feed bars because of no extented bar loader registered.");  // 记录错误日志
		return;  // 直接返回，不推送数据
	}

	_feeder_bars(_feed_obj, bars, count);  // 调用K线数据读取回调函数，将数据推送给数据管理器
	// 参数说明：
	// - _feed_obj: 数据接收对象指针（由数据管理器传入）
	// - bars: K线数据数组指针
	// - count: K线数据条数
}


/**
 * @brief 创建扩展Parser实现（外部语言实现的行情解析器）
 * 
 * 创建扩展Parser实例，并将其添加到解析器适配器管理器。
 * 
 * @param id Parser的唯一标识符，用于标识该Parser实例
 * @return 创建成功返回true，失败返回false
 * 
 * 创建流程：
 * 1. 创建ParserAdapter适配器智能指针实例
 * 2. 创建ExpParser扩展Parser实例（外部语言实现的Parser）
 * 3. 初始化适配器，将扩展Parser与引擎关联
 *    - 传入Parser ID、扩展Parser实例、引擎指针、基础数据管理器指针、热点合约管理器指针
 * 4. 将适配器添加到解析器适配器管理器
 * 5. 记录日志，表示扩展Parser创建成功
 * 
 * 注意事项：
 * - 扩展Parser的事件会通过registerParserPorter()注册的回调函数转发给外部语言
 * - Parser的ID必须唯一，不能与已存在的Parser重复
 * - 扩展Parser可以接收外部语言推送的行情数据，并转发给引擎
 */
bool WtRtRunner::createExtParser(const char* id)
{
	ParserAdapterPtr adapter(new ParserAdapter);  // 创建Parser适配器智能指针实例
	ExpParser* parser = new ExpParser(id);  // 创建扩展Parser实例（外部语言实现的Parser）
	adapter->initExt(id, parser, _engine, _engine->get_basedata_mgr(), _engine->get_hot_mgr());  // 初始化适配器，将扩展Parser与引擎关联
	// 参数说明：
	// - id: Parser的唯一标识符
	// - parser: 扩展Parser实例指针
	// - _engine: 交易引擎指针
	// - _engine->get_basedata_mgr(): 基础数据管理器指针
	// - _engine->get_hot_mgr(): 热点合约管理器指针
	_parsers.addAdapter(id, adapter);  // 将适配器添加到解析器适配器管理器
	WTSLogger::info("Extended parser created");  // 记录日志，表示扩展Parser创建成功
	return true;  // 返回true，表示创建成功
}

/**
 * @brief 创建扩展Executer实现（外部语言实现的执行器）
 * 
 * 创建扩展Executer实例，并将其添加到CTA引擎的执行器列表。
 * 
 * @param id Executer的唯一标识符，用于标识该Executer实例
 * @return 创建成功返回true，失败返回false
 * 
 * 创建流程：
 * 1. 创建ExpExecuter扩展执行器实例（外部语言实现的Executer）
 * 2. 初始化执行器
 * 3. 将执行器添加到CTA引擎的执行器列表
 * 4. 记录日志，表示扩展Executer创建成功
 * 
 * 注意事项：
 * - 扩展Executer的命令会通过registerExecuterPorter()注册的回调函数转发给外部语言
 * - Executer的ID必须唯一，不能与已存在的Executer重复
 * - 扩展Executer只能添加到CTA引擎，不支持HFT和SEL引擎
 * - 执行器用于执行交易命令，将目标仓位转换为实际的交易指令
 */
bool WtRtRunner::createExtExecuter(const char* id)
{
	ExpExecuter* executer = new ExpExecuter(id);  // 创建扩展执行器实例（外部语言实现的Executer）
	executer->init();  // 初始化执行器
	_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎的执行器列表
	// ExecCmdPtr是执行器智能指针类型，用于管理执行器的生命周期
	WTSLogger::info("Extended Executer created");  // 记录日志，表示扩展Executer创建成功
	return true;  // 返回true，表示创建成功
}

/**
 * @brief 创建CTA策略上下文实现
 * 
 * 创建CTA策略的扩展上下文实例，并将其添加到CTA引擎。
 * 
 * @param name 策略名称，用于标识策略
 * @param slippage 滑点设置（单位：最小变动价位），用于模拟交易时的滑点成本，默认为0
 * @return 返回策略上下文的ID，用于后续访问该上下文
 * 
 * 创建流程：
 * 1. 创建ExpCtaContext扩展上下文实例
 *    - 传入CTA引擎指针、策略名称、滑点设置
 * 2. 将上下文添加到CTA引擎
 *    - 引擎会自动分配上下文ID
 * 3. 返回上下文的ID
 * 
 * 注意事项：
 * - 上下文ID由引擎自动分配，是唯一的
 * - 滑点设置会影响模拟交易的成本计算
 * - 创建上下文后，策略的事件会通过registerCtaCallbacks()注册的回调函数转发
 * - 上下文作为适配器，将引擎事件转发给外部语言回调函数
 */
uint32_t WtRtRunner::createCtaContext(const char* name, int32_t slippage /* = 0 */)
{
	ExpCtaContext* ctx = new ExpCtaContext(&_cta_engine, name, slippage);  // 创建CTA策略扩展上下文实例
	// 参数说明：
	// - &_cta_engine: CTA引擎指针
	// - name: 策略名称
	// - slippage: 滑点设置（单位：最小变动价位）
	_cta_engine.addContext(CtaContextPtr(ctx));  // 将上下文添加到CTA引擎，引擎会自动分配上下文ID
	// CtaContextPtr是上下文智能指针类型，用于管理上下文的生命周期
	return ctx->id();  // 返回上下文的ID，用于后续访问该上下文
}

/**
 * @brief 创建HFT策略上下文实现
 * 
 * 创建HFT策略的扩展上下文实例，并将其添加到HFT引擎，同时绑定交易通道。
 * 
 * @param name 策略名称，用于标识策略
 * @param trader 交易通道ID，用于绑定交易通道
 * @param bAgent 是否为代理模式，true表示代理模式（订单直接发送到交易通道），false表示非代理模式
 * @param slippage 滑点设置（单位：最小变动价位），用于模拟交易时的滑点成本，默认为0
 * @return 返回策略上下文的ID，用于后续访问该上下文
 * 
 * 创建流程：
 * 1. 创建ExpHftContext扩展上下文实例
 *    - 传入HFT引擎指针、策略名称、代理模式标志、滑点设置
 * 2. 将上下文添加到HFT引擎
 *    - 引擎会自动分配上下文ID
 * 3. 根据trader参数查找交易适配器
 * 4. 如果找到交易适配器：
 *    a. 将上下文绑定到交易适配器
 *    b. 将上下文添加到交易适配器的接收者列表（用于接收交易回报）
 * 5. 如果未找到交易适配器，记录错误日志（但上下文仍会创建成功）
 * 6. 返回上下文的ID
 * 
 * 注意事项：
 * - HFT策略必须绑定交易通道，否则无法进行交易
 * - 如果指定的交易通道不存在，会记录错误日志，但上下文仍会创建成功
 * - 代理模式下，订单会直接发送到交易通道，不经过执行器
 * - 非代理模式下，订单会经过执行器处理后再发送到交易通道
 */
uint32_t WtRtRunner::createHftContext(const char* name, const char* trader, bool bAgent, int32_t slippage /* = 0 */)
{
	ExpHftContext* ctx = new ExpHftContext(&_hft_engine, name, bAgent, slippage);  // 创建HFT策略扩展上下文实例
	// 参数说明：
	// - &_hft_engine: HFT引擎指针
	// - name: 策略名称
	// - bAgent: 是否为代理模式
	// - slippage: 滑点设置（单位：最小变动价位）
	_hft_engine.addContext(HftContextPtr(ctx));  // 将上下文添加到HFT引擎，引擎会自动分配上下文ID
	// HftContextPtr是上下文智能指针类型，用于管理上下文的生命周期
	TraderAdapterPtr trdPtr = _traders.getAdapter(trader);  // 根据交易通道ID查找交易适配器
	if(trdPtr)  // 如果找到交易适配器
	{
		ctx->setTrader(trdPtr.get());  // 将上下文绑定到交易适配器
		trdPtr->addSink(ctx);  // 将上下文添加到交易适配器的接收者列表（用于接收交易回报）
		// addSink()会将上下文添加到交易适配器的接收者列表，当交易回报到达时，会通知上下文
	}
	else  // 如果未找到交易适配器
	{
		WTSLogger::error("Trader {} not exists, Binding trader to HFT strategy failed", trader);  // 记录错误日志
		// 注意：即使交易适配器不存在，上下文仍会创建成功，只是无法进行交易
	}
	return ctx->id();  // 返回上下文的ID，用于后续访问该上下文
}

/**
 * @brief 创建SEL策略上下文实现
 * 
 * 创建SEL（选股）策略的扩展上下文实例，并将其添加到SEL引擎。
 * 
 * @param name 策略名称，用于标识策略
 * @param date 调度日期（格式：YYYYMMDD），策略开始执行的日期
 * @param time 调度时间（格式：HHMM），策略开始执行的时间
 * @param period 调度周期字符串，可选值："d"（日）、"w"（周）、"m"（月）、"y"（年）、"min"（分钟）
 * @param slippage 滑点设置（单位：最小变动价位），用于模拟交易时的滑点成本
 * @param trdtpl 交易日模板，默认为"CHINA"（中国交易日）
 * @param session 交易时段，默认为"TRADING"（交易时段）
 * @return 返回策略上下文的ID，用于后续访问该上下文
 * 
 * 创建流程：
 * 1. 解析调度周期字符串，转换为TaskPeriodType枚举值
 *    - "d" -> TPT_Daily（日）
 *    - "w" -> TPT_Weekly（周）
 *    - "m" -> TPT_Monthly（月）
 *    - "y" -> TPT_Yearly（年）
 *    - "min" -> TPT_Minute（分钟）
 *    - 其他 -> TPT_None（无周期）
 * 2. 创建ExpSelContext扩展上下文实例
 *    - 传入SEL引擎指针、策略名称、滑点设置
 * 3. 将上下文添加到SEL引擎，并设置调度参数
 *    - 传入调度日期、调度时间、调度周期、交易日模板、交易时段
 *    - 引擎会自动分配上下文ID
 * 4. 返回上下文的ID
 * 
 * 注意事项：
 * - SEL策略是定时执行的，需要设置调度日期、时间和周期
 * - 调度周期决定了策略的执行频率
 * - 交易日模板和交易时段用于确定策略的执行时间
 * - 上下文创建后，策略会在指定的调度时间执行
 */
uint32_t WtRtRunner::createSelContext(const char* name, uint32_t date, uint32_t time, const char* period, int32_t slippage, const char* trdtpl /* = "CHINA" */, const char* session/* ="TRADING" */)
{
	TaskPeriodType ptype;  // 调度周期类型枚举变量
	if (wt_stricmp(period, "d") == 0)  // 如果周期字符串为"d"（不区分大小写）
		ptype = TPT_Daily;  // 设置为日周期
	else if (wt_stricmp(period, "w") == 0)  // 如果周期字符串为"w"（不区分大小写）
		ptype = TPT_Weekly;  // 设置为周周期
	else if (wt_stricmp(period, "m") == 0)  // 如果周期字符串为"m"（不区分大小写）
		ptype = TPT_Monthly;  // 设置为月周期
	else if (wt_stricmp(period, "y") == 0)  // 如果周期字符串为"y"（不区分大小写）
		ptype = TPT_Yearly;  // 设置为年周期
	else if (wt_stricmp(period, "min") == 0)  // 如果周期字符串为"min"（不区分大小写）
		ptype = TPT_Minute;  // 设置为分钟周期
	else  // 如果是其他不支持的周期字符串
		ptype = TPT_None;  // 设置为无周期

	ExpSelContext* ctx = new ExpSelContext(&_sel_engine, name, slippage);  // 创建SEL策略扩展上下文实例
	// 参数说明：
	// - &_sel_engine: SEL引擎指针
	// - name: 策略名称
	// - slippage: 滑点设置（单位：最小变动价位）

	_sel_engine.addContext(SelContextPtr(ctx), date, time, ptype, true, trdtpl, session);  // 将上下文添加到SEL引擎，并设置调度参数
	// 参数说明：
	// - SelContextPtr(ctx): 上下文智能指针
	// - date: 调度日期（格式：YYYYMMDD）
	// - time: 调度时间（格式：HHMM）
	// - ptype: 调度周期类型
	// - true: 是否启用调度（固定为true）
	// - trdtpl: 交易日模板（如"CHINA"）
	// - session: 交易时段（如"TRADING"）

	return ctx->id();  // 返回上下文的ID，用于后续访问该上下文
}

/**
 * @brief 获取原始合约代码实现
 * 
 * 将标准合约代码转换为原始合约代码（去掉.HOT、.2ND等后缀）。
 * 
 * @param stdCode 标准合约代码，如"SHFE.rb2305.HOT"
 * @return 返回原始合约代码字符串指针，如"SHFE.rb2305"
 * 
 * 实现流程：
 * 1. 使用线程局部静态变量存储结果字符串（线程安全）
 * 2. 调用引擎的get_rawcode()方法获取原始合约代码
 * 3. 返回字符串的C风格指针
 * 
 * 注意事项：
 * - 返回的字符串指针指向线程局部静态变量，下次调用会被覆盖
 * - 如果需要保存结果，应该复制字符串内容
 * - 线程局部存储确保多线程环境下不会相互干扰
 */
const char* WtRtRunner::get_raw_stdcode(const char* stdCode)
{
	static thread_local std::string s;  // 线程局部静态变量，用于存储结果字符串（线程安全）
	s = _engine->get_rawcode(stdCode);  // 调用引擎的get_rawcode()方法获取原始合约代码
	return s.c_str();  // 返回字符串的C风格指针
}


/**
 * @brief 获取CTA策略上下文实现
 * 
 * 根据上下文ID获取CTA策略上下文指针。
 * 
 * @param id 策略上下文的ID（由createCtaContext()返回）
 * @return 返回策略上下文智能指针，如果ID不存在返回空指针
 * 
 * 实现说明：
 * - 从CTA引擎中查找指定ID的上下文
 * - 返回智能指针，自动管理内存
 */
CtaContextPtr WtRtRunner::getCtaContext(uint32_t id)
{
	return _cta_engine.getContext(id);  // 从CTA引擎中查找指定ID的上下文，返回智能指针
}

/**
 * @brief 获取HFT策略上下文实现
 * 
 * 根据上下文ID获取HFT策略上下文指针。
 * 
 * @param id 策略上下文的ID（由createHftContext()返回）
 * @return 返回策略上下文智能指针，如果ID不存在返回空指针
 * 
 * 实现说明：
 * - 从HFT引擎中查找指定ID的上下文
 * - 返回智能指针，自动管理内存
 */
HftContextPtr WtRtRunner::getHftContext(uint32_t id)
{
	return _hft_engine.getContext(id);  // 从HFT引擎中查找指定ID的上下文，返回智能指针
}

/**
 * @brief 获取SEL策略上下文实现
 * 
 * 根据上下文ID获取SEL策略上下文指针。
 * 
 * @param id 策略上下文的ID（由createSelContext()返回）
 * @return 返回策略上下文智能指针，如果ID不存在返回空指针
 * 
 * 实现说明：
 * - 从SEL引擎中查找指定ID的上下文
 * - 返回智能指针，自动管理内存
 */
SelContextPtr WtRtRunner::getSelContext(uint32_t id)
{
	return _sel_engine.getContext(id);  // 从SEL引擎中查找指定ID的上下文，返回智能指针
}

/**
 * @brief 策略K线闭合事件处理实现
 * 
 * 当K线闭合时，扩展上下文会调用此方法。
 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param period K线周期字符串，如"m1"、"m5"、"d1"等
 * @param newBar 新的K线数据指针，包含K线的开高低收等数据
 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
 * 
 * 实现流程：
 * 1. 根据引擎类型选择对应的K线闭合回调函数
 * 2. 如果回调函数已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、合约代码、K线周期、K线数据结构指针
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理K线闭合事件
 * - K线数据指针由调用者管理，回调函数不应该释放该指针
 * - 支持CTA、HFT、SEL三种引擎类型
 */
void WtRtRunner::ctx_on_bar(uint32_t id, const char* stdCode, const char* period, WTSBarStruct* newBar, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_bar) _cb_cta_bar(id, stdCode, period, newBar); break;  // 如果是CTA引擎，调用CTA策略K线闭合回调函数
	case ET_HFT: if (_cb_hft_bar) _cb_hft_bar(id, stdCode, period, newBar); break;  // 如果是HFT引擎，调用HFT策略K线闭合回调函数
	case ET_SEL: if (_cb_sel_bar) _cb_sel_bar(id, stdCode, period, newBar); break;  // 如果是SEL引擎，调用SEL策略K线闭合回调函数
	default:  // 如果是其他不支持的引擎类型
		break;  // 不做处理
	}
}

/**
 * @brief 策略计算事件处理实现
 * 
 * 当策略需要计算时（定时计算），扩展上下文会调用此方法。
 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMM）
 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
 * 
 * 实现流程：
 * 1. 根据引擎类型选择对应的计算回调函数
 * 2. 如果回调函数已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、当前日期、当前时间
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理计算事件
 * - 只有CTA和SEL引擎支持计算事件，HFT引擎不支持
 * - 计算事件通常用于定时执行策略逻辑
 */
void WtRtRunner::ctx_on_calc(uint32_t id, uint32_t curDate, uint32_t curTime, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_calc) _cb_cta_calc(id, curDate, curTime); break;  // 如果是CTA引擎，调用CTA策略计算回调函数
	case ET_SEL: if (_cb_sel_calc) _cb_sel_calc(id, curDate, curTime); break;  // 如果是SEL引擎，调用SEL策略计算回调函数
	default:  // 如果是HFT引擎或其他不支持的引擎类型
		break;  // 不做处理（HFT引擎不支持计算事件）
	}
}

/**
 * @brief 策略条件单触发事件处理实现
 * 
 * 当条件单触发时，扩展上下文会调用此方法。
 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param target 目标持仓数量，正数表示多头，负数表示空头，0表示平仓
 * @param price 触发价格，条件单触发时的价格
 * @param usertag 用户标签，用于标识条件单
 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
 * 
 * 实现流程：
 * 1. 根据引擎类型选择对应的条件单触发回调函数
 * 2. 如果回调函数已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、合约代码、目标持仓、触发价格、用户标签
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理条件单触发事件
 * - 目前只支持CTA引擎的条件单触发回调
 * - 条件单触发后，策略需要根据目标持仓执行相应的交易操作
 */
void WtRtRunner::ctx_on_cond_triggered(uint32_t id, const char* stdCode, double target, double price, const char* usertag, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_cond_trigger) _cb_cta_cond_trigger(id, stdCode, target, price, usertag); break;  // 如果是CTA引擎，调用CTA策略条件单触发回调函数
	default:  // 如果是HFT引擎、SEL引擎或其他不支持的引擎类型
		break;  // 不做处理（只有CTA引擎支持条件单触发）
	}
}

/**
 * @brief 策略初始化事件处理实现
 * 
 * 当策略上下文初始化完成时，扩展上下文会调用此方法。
 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID
 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
 * 
 * 实现流程：
 * 1. 根据引擎类型选择对应的初始化回调函数
 * 2. 如果回调函数已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理初始化事件
 * - 初始化事件在策略上下文创建后立即触发
 * - 支持CTA、HFT、SEL三种引擎类型
 */
void WtRtRunner::ctx_on_init(uint32_t id, EngineType eType/* = ET_CTA*/)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_init) _cb_cta_init(id); break;  // 如果是CTA引擎，调用CTA策略初始化回调函数
	case ET_HFT: if (_cb_hft_init) _cb_hft_init(id); break;  // 如果是HFT引擎，调用HFT策略初始化回调函数
	case ET_SEL: if (_cb_sel_init) _cb_sel_init(id); break;  // 如果是SEL引擎，调用SEL策略初始化回调函数
	default:  // 如果是其他不支持的引擎类型
		break;  // 不做处理
	}
}

/**
 * @brief 策略交易日事件处理实现
 * 
 * 当策略的交易日开始或结束时，扩展上下文会调用此方法。
 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID
 * @param curTDate 当前交易日期（格式：YYYYMMDD）
 * @param isBegin 是否为交易日开始，true表示交易日开始，false表示交易日结束
 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
 * 
 * 实现流程：
 * 1. 根据引擎类型选择对应的交易日事件回调函数
 * 2. 如果回调函数已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、交易日期、是否开始
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理交易日事件
 * - 交易日开始事件在交易日开始时触发，交易日结束事件在交易日结束时触发
 * - 支持CTA、HFT、SEL三种引擎类型
 */
void WtRtRunner::ctx_on_session_event(uint32_t id, uint32_t curTDate, bool isBegin /* = true */, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_sessevt) _cb_cta_sessevt(id, curTDate, isBegin); break;  // 如果是CTA引擎，调用CTA策略交易日事件回调函数
	case ET_HFT: if (_cb_hft_sessevt) _cb_hft_sessevt(id, curTDate, isBegin); break;  // 如果是HFT引擎，调用HFT策略交易日事件回调函数
	case ET_SEL: if (_cb_sel_sessevt) _cb_sel_sessevt(id, curTDate, isBegin); break;  // 如果是SEL引擎，调用SEL策略交易日事件回调函数
	default:  // 如果是其他不支持的引擎类型
		break;  // 不做处理
	}
}

/**
 * @brief 策略Tick数据事件处理实现
 * 
 * 当策略收到新的Tick数据时，扩展上下文会调用此方法。
 * 此方法根据引擎类型调用对应的回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param newTick 新的Tick数据指针，包含Tick的开高低收、成交量、持仓量等数据
 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL），默认为ET_CTA
 * 
 * 实现流程：
 * 1. 根据引擎类型选择对应的Tick数据回调函数
 * 2. 如果回调函数已注册，调用回调函数将事件转发给外部语言
 * 3. 从WTSTickData对象中提取Tick数据结构，传递给回调函数
 * 4. 回调函数参数：上下文ID、合约代码、Tick数据结构指针
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理Tick数据事件
 * - Tick数据指针由调用者管理，回调函数不应该释放该指针
 * - 支持CTA、HFT、SEL三种引擎类型
 * - Tick数据是实时行情数据，频率较高
 */
void WtRtRunner::ctx_on_tick(uint32_t id, const char* stdCode, WTSTickData* newTick, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_tick) _cb_cta_tick(id, stdCode, &newTick->getTickStruct()); break;  // 如果是CTA引擎，调用CTA策略Tick数据回调函数，传入Tick数据结构指针
	case ET_HFT: if (_cb_hft_tick) _cb_hft_tick(id, stdCode, &newTick->getTickStruct()); break;  // 如果是HFT引擎，调用HFT策略Tick数据回调函数，传入Tick数据结构指针
	case ET_SEL: if (_cb_sel_tick) _cb_sel_tick(id, stdCode, &newTick->getTickStruct()); break;  // 如果是SEL引擎，调用SEL策略Tick数据回调函数，传入Tick数据结构指针
	default:  // 如果是其他不支持的引擎类型
		break;  // 不做处理
	}
}

/**
 * @brief HFT策略交易通道丢失事件处理实现
 * 
 * 当HFT策略的交易通道丢失时，扩展上下文会调用此方法。
 * 此方法调用HFT通道事件回调函数，将事件转发给外部语言。
 * 
 * @param cHandle 策略上下文的ID（HFT上下文句柄）
 * @param trader 交易通道ID，标识哪个交易通道丢失
 * 
 * 实现流程：
 * 1. 检查HFT通道事件回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、交易通道ID、事件类型（CHNL_EVENT_LOST）
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理交易通道丢失事件
 * - 交易通道丢失后，策略无法进行交易，需要重新连接或切换到其他通道
 */
void WtRtRunner::hft_on_channel_lost(uint32_t cHandle, const char* trader)
{
	if (_cb_hft_chnl)  // 如果HFT通道事件回调函数已注册
		_cb_hft_chnl(cHandle, trader, CHNL_EVENT_LOST);  // 调用回调函数，事件类型为通道丢失
}

/**
 * @brief HFT策略交易通道就绪事件处理实现
 * 
 * 当HFT策略的交易通道就绪时，扩展上下文会调用此方法。
 * 此方法调用HFT通道事件回调函数，将事件转发给外部语言。
 * 
 * @param cHandle 策略上下文的ID（HFT上下文句柄）
 * @param trader 交易通道ID，标识哪个交易通道就绪
 * 
 * 实现流程：
 * 1. 检查HFT通道事件回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、交易通道ID、事件类型（CHNL_EVENT_READY）
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理交易通道就绪事件
 * - 交易通道就绪后，策略可以正常进行交易
 */
void WtRtRunner::hft_on_channel_ready(uint32_t cHandle, const char* trader)
{
	if (_cb_hft_chnl)  // 如果HFT通道事件回调函数已注册
		_cb_hft_chnl(cHandle, trader, CHNL_EVENT_READY);  // 调用回调函数，事件类型为通道就绪
}

/**
 * @brief HFT策略下单结果事件处理实现
 * 
 * 当HFT策略的下单请求有结果时，扩展上下文会调用此方法。
 * 此方法调用HFT下单结果回调函数，将事件转发给外部语言。
 * 
 * @param cHandle 策略上下文的ID（HFT上下文句柄）
 * @param localid 本地订单ID，用于标识订单
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param bSuccess 是否成功，true表示下单成功，false表示下单失败
 * @param message 结果消息，如果失败则包含错误信息
 * @param userTag 用户标签，用于标识订单（由策略在下单时传入）
 * 
 * 实现流程：
 * 1. 检查HFT下单结果回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、本地订单ID、合约代码、是否成功、结果消息、用户标签
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理下单结果事件
 * - 下单结果事件在下单请求发出后立即返回，不等待订单成交
 * - 如果下单失败，message会包含错误信息
 */
void WtRtRunner::hft_on_entrust(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag)
{
	if (_cb_hft_entrust)  // 如果HFT下单结果回调函数已注册
		_cb_hft_entrust(cHandle, localid, stdCode, bSuccess, message, userTag);  // 调用回调函数，将下单结果转发给外部语言
}

/**
 * @brief HFT策略订单回报事件处理实现
 * 
 * 当HFT策略的订单状态变化时，扩展上下文会调用此方法。
 * 此方法调用HFT订单回报回调函数，将事件转发给外部语言。
 * 
 * @param cHandle 策略上下文的ID（HFT上下文句柄）
 * @param localid 本地订单ID，用于标识订单
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param isBuy 是否为买入订单，true表示买入，false表示卖出
 * @param totalQty 订单总数量，下单时的总数量
 * @param leftQty 订单剩余数量，未成交的数量
 * @param price 订单价格
 * @param isCanceled 是否已撤销，true表示已撤销，false表示未撤销
 * @param userTag 用户标签，用于标识订单（由策略在下单时传入）
 * 
 * 实现流程：
 * 1. 检查HFT订单回报回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、本地订单ID、合约代码、是否买入、总数量、剩余数量、价格、是否撤销、用户标签
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理订单回报事件
 * - 订单回报事件在订单状态变化时触发（如部分成交、全部成交、撤销等）
 * - leftQty = 0表示订单已全部成交或已撤销
 */
void WtRtRunner::hft_on_order(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag)
{
	if (_cb_hft_ord)  // 如果HFT订单回报回调函数已注册
		_cb_hft_ord(cHandle, localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, userTag);  // 调用回调函数，将订单回报转发给外部语言
}

/**
 * @brief HFT策略成交回报事件处理实现
 * 
 * 当HFT策略的订单成交时，扩展上下文会调用此方法。
 * 此方法调用HFT成交回报回调函数，将事件转发给外部语言。
 * 
 * @param cHandle 策略上下文的ID（HFT上下文句柄）
 * @param localid 本地订单ID，用于标识订单
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param isBuy 是否为买入成交，true表示买入，false表示卖出
 * @param vol 成交数量
 * @param price 成交价格
 * @param userTag 用户标签，用于标识订单（由策略在下单时传入）
 * 
 * 实现流程：
 * 1. 检查HFT成交回报回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、本地订单ID、合约代码、是否买入、成交数量、成交价格、用户标签
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理成交回报事件
 * - 成交回报事件在订单部分成交或全部成交时触发
 * - 一个订单可能产生多次成交回报（如果分批成交）
 */
void WtRtRunner::hft_on_trade(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag)
{
	if (_cb_hft_trd)  // 如果HFT成交回报回调函数已注册
		_cb_hft_trd(cHandle, localid, stdCode, isBuy, vol, price, userTag);  // 调用回调函数，将成交回报转发给外部语言
}

/**
 * @brief HFT策略持仓变化事件处理实现
 * 
 * 当HFT策略的持仓发生变化时，扩展上下文会调用此方法。
 * 此方法调用HFT持仓变化回调函数，将事件转发给外部语言。
 * 
 * @param cHandle 策略上下文的ID（HFT上下文句柄）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param isLong 是否为多头持仓，true表示多头，false表示空头
 * @param prevol 变化前持仓数量
 * @param preavail 变化前可用持仓数量
 * @param newvol 变化后持仓数量
 * @param newavail 变化后可用持仓数量
 * 
 * 实现流程：
 * 1. 检查HFT持仓变化回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、合约代码、是否多头、变化前持仓、变化前可用、变化后持仓、变化后可用
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理持仓变化事件
 * - 持仓变化事件在订单成交后触发，用于更新持仓信息
 * - 可用持仓数量 = 持仓数量 - 冻结数量（挂单未成交的数量）
 */
void WtRtRunner::hft_on_position(uint32_t cHandle, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail)
{
	if (_cb_hft_position)  // 如果HFT持仓变化回调函数已注册
		_cb_hft_position(cHandle, stdCode, isLong, prevol, preavail, newvol, newavail);  // 调用回调函数，将持仓变化信息转发给外部语言
}

/**
 * @brief 配置运行时运行器实现
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
bool WtRtRunner::config(const char* cfgFile, bool isFile /* = true */)
{
	// 加载主配置文件（从文件或字符串）
	_config = isFile ? WTSCfgLoader::load_from_file(cfgFile) : WTSCfgLoader::load_from_content(cfgFile, false);
	// 如果isFile为true，从文件加载配置；否则从字符串加载配置
	// 加载失败时_config为NULL，但后续代码会继续执行（可能导致崩溃，实际应该检查）

	// ========== 加载基础数据文件 ==========
	WTSVariant* cfgBF = _config->get("basefiles");  // 获取基础数据文件配置节点
	if (cfgBF->get("session"))  // 如果配置了交易时段文件
	{
		_bd_mgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段数据
		WTSLogger::info("Trading sessions loaded");  // 记录日志，表示交易时段数据加载成功
	}

	// 加载商品基础数据（支持单个文件路径或文件路径数组）
	WTSVariant* cfgItem = cfgBF->get("commodity");  // 获取商品数据配置项
	if (cfgItem)  // 如果配置了商品数据文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是单个文件路径（字符串类型）
		{
			_bd_mgr.loadCommodities(cfgItem->asCString());  // 加载商品基础数据文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是文件路径数组
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组中的每个文件路径
			{
				_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());  // 逐个加载商品基础数据文件
			}
		}
	}

	// 加载合约基础数据（支持单个文件路径或文件路径数组）
	cfgItem = cfgBF->get("contract");  // 获取合约数据配置项
	if (cfgItem)  // 如果配置了合约数据文件
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是单个文件路径（字符串类型）
		{
			_bd_mgr.loadContracts(cfgItem->asCString());  // 加载合约基础数据文件
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是文件路径数组
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组中的每个文件路径
			{
				_bd_mgr.loadContracts(cfgItem->get(i)->asCString());  // 逐个加载合约基础数据文件
			}
		}
	}

	// 加载节假日数据（用于判断交易日和非交易日）
	if (cfgBF->get("holiday"))  // 如果配置了节假日文件
	{
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));  // 加载节假日数据文件
		WTSLogger::log_raw(LL_INFO, "Holidays loaded");  // 记录日志，表示节假日数据加载成功
	}

	// 加载主力合约规则（用于确定主力合约）
	if (cfgBF->get("hot"))  // 如果配置了主力合约规则文件
	{
		_hot_mgr.loadHots(cfgBF->getCString("hot"));  // 加载主力合约规则文件
		WTSLogger::log_raw(LL_INFO, "Hot rules loaded");  // 记录日志，表示主力合约规则加载成功
	}

	// 加载次主力合约规则（用于确定次主力合约）
	if (cfgBF->get("second"))  // 如果配置了次主力合约规则文件
	{
		_hot_mgr.loadSeconds(cfgBF->getCString("second"));  // 加载次主力合约规则文件
		WTSLogger::log_raw(LL_INFO, "Second rules loaded");  // 记录日志，表示次主力合约规则加载成功
	}

	// 设置合约的主力/次主力标志（用于后续查询主力合约）
	WTSArray* ayContracts = _bd_mgr.getContracts();  // 获取所有合约信息数组
	for (auto it = ayContracts->begin(); it != ayContracts->end(); it++)  // 遍历所有合约
	{
		WTSContractInfo* cInfo = (WTSContractInfo*)(*it);  // 获取合约信息对象
		bool isHot = _hot_mgr.isHot(cInfo->getExchg(), cInfo->getCode());  // 判断是否为主力合约
		bool isSecond = _hot_mgr.isSecond(cInfo->getExchg(), cInfo->getCode());  // 判断是否为次主力合约
		
		// 构建主力合约代码（格式：交易所.品种.HOT 或 交易所.品种.2ND）
		std::string hotCode = cInfo->getFullPid();  // 获取完整的品种代码（如"SHFE.rb"）
		if (isHot)  // 如果为主力合约
			hotCode += ".HOT";  // 追加.HOT后缀
		else if (isSecond)  // 如果为次主力合约
			hotCode += ".2ND";  // 追加.2ND后缀
		else
			hotCode = "";  // 否则清空（不是主力也不是次主力）

		// 设置合约的主力标志（1=主力，2=次主力，0=普通合约）和主力代码
		cInfo->setHotFlag(isHot ? 1 : (isSecond ? 2 : 0), hotCode.c_str());
	}
	ayContracts->release();  // 释放合约数组资源

	// 加载自定义规则（支持多个规则文件，通过标签区分）
	if(cfgBF->has("rules"))  // 如果配置了自定义规则
	{
		auto cfgRules = cfgBF->get("rules");  // 获取规则配置节点
		auto tags = cfgRules->memberNames();  // 获取所有规则标签（键名）
		for(const std::string& ruleTag : tags)  // 遍历每个规则标签
		{
			// 加载指定标签的自定义规则文件
			_hot_mgr.loadCustomRules(ruleTag.c_str(), cfgRules->getCString(ruleTag.c_str()));
			WTSLogger::info("{} rules loaded from {}", ruleTag, cfgRules->getCString(ruleTag.c_str()));  // 记录日志
		}
	}

	// ========== 初始化运行环境（引擎） ==========
	initEngine();  // 初始化交易引擎（CTA、HFT或SEL引擎）

	// ========== 初始化数据管理 ==========
	initDataMgr();  // 初始化数据管理器（注册数据加载器接口）

	// ========== 初始化开平策略 ==========
	if (!initActionPolicy())  // 初始化开平策略管理器（用于确定开仓和平仓方向）
		return false;  // 如果初始化失败，返回false

	// ========== 初始化行情通道（解析器适配器） ==========
	WTSVariant* cfgParser = _config->get("parsers");  // 获取解析器配置节点
	if (cfgParser)  // 如果配置了解析器
	{
		if (cfgParser->type() == WTSVariant::VT_String)  // 如果是文件路径（字符串类型）
		{
			const char* filename = cfgParser->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果文件存在
			{
				WTSLogger::info("Reading parser config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 从文件加载配置
				if (var)  // 如果加载成功
				{
					if (!initParsers(var->get("parsers")))  // 初始化解析器适配器
						WTSLogger::error("Loading parsers failed");  // 如果失败，记录错误日志
					var->release();  // 释放配置对象
				}
				else  // 如果加载失败
				{
					WTSLogger::error("Loading parser config {} failed", filename);  // 记录错误日志
				}
			}
			else  // 如果文件不存在
			{
				WTSLogger::error("Parser configuration {} not exists", filename);  // 记录错误日志
			}
		}
		else if (cfgParser->type() == WTSVariant::VT_Array)  // 如果是配置数组（直接内嵌配置）
		{
			initParsers(cfgParser);  // 直接初始化解析器适配器
		}
	}

	// ========== 初始化交易通道（交易适配器） ==========
	WTSVariant* cfgTraders = _config->get("traders");  // 获取交易适配器配置节点
	if(cfgTraders)  // 如果配置了交易适配器
	{
		if (cfgTraders->type() == WTSVariant::VT_String)  // 如果是文件路径（字符串类型）
		{
			const char* filename = cfgTraders->asCString();  // 获取配置文件路径
			if (StdFile::exists(filename))  // 如果文件存在
			{
				WTSLogger::info("Reading trader config from {}...", filename);  // 记录日志
				WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 从文件加载配置
				if (var)  // 如果加载成功
				{
					if (!initTraders(var->get("traders")))  // 初始化交易适配器
						WTSLogger::error("Loading traders failed");  // 如果失败，记录错误日志
					var->release();  // 释放配置对象
				}
				else  // 如果加载失败
				{
					WTSLogger::error("Loading trader config {} failed", filename);  // 记录错误日志
				}
			}
			else  // 如果文件不存在
			{
				WTSLogger::error("Trader configuration {} not exists", filename);  // 记录错误日志
			}
		}
		else if (cfgTraders->type() == WTSVariant::VT_Array)  // 如果是配置数组（直接内嵌配置）
		{
			initTraders(cfgTraders);  // 直接初始化交易适配器
		}
	}

	// ========== 初始化事件推送器 ==========
	initEvtNotifier();  // 初始化事件通知器（用于发送系统事件通知，如日志、错误等）

	// ========== 初始化执行模块（仅非高频引擎需要） ==========
	if (!_is_hft)  // 如果不是高频引擎（CTA或SEL引擎需要执行模块）
	{
		WTSVariant* cfgExec = _config->get("executers");  // 获取执行器配置节点
		if(cfgExec != NULL)  // 如果配置了执行器
		{
			if(cfgExec->type() == WTSVariant::VT_String)  // 如果是文件路径（字符串类型）
			{
				const char* filename = cfgExec->asCString();  // 获取配置文件路径
				if (StdFile::exists(filename))  // 如果文件存在
				{
					WTSLogger::info("Reading executer config from {}...", filename);  // 记录日志
					WTSVariant* var = WTSCfgLoader::load_from_file(filename);  // 从文件加载配置
					if (var)  // 如果加载成功
					{
						if (!initExecuters(var->get("executers")))  // 初始化执行器（本地执行器、差分执行器等）
							WTSLogger::error("Loading executers failed");  // 如果失败，记录错误日志

						// 加载路由规则（用于确定订单的路由目标）
						WTSVariant* c = var->get("routers");  // 获取路由规则配置节点
						if (c != NULL)  // 如果配置了路由规则
							_cta_engine.loadRouterRules(c);  // 加载路由规则到CTA引擎

						var->release();  // 释放配置对象
					}
					else  // 如果加载失败
					{
						WTSLogger::error("Loading executer config {} failed", filename);  // 记录错误日志
					}
				}
				else  // 如果文件不存在
				{
					WTSLogger::error("Trader configuration {} not exists", filename);  // 记录错误日志
				}
			}
			else if(cfgExec->type() == WTSVariant::VT_Array)  // 如果是配置数组（直接内嵌配置）
			{
				initExecuters(cfgExec);  // 直接初始化执行器
			}
		}

		// 加载路由规则（如果主配置文件中直接配置了路由规则）
		WTSVariant* cfgRouter = _config->get("routers");  // 获取路由规则配置节点
		if (cfgRouter != NULL)  // 如果配置了路由规则
			_cta_engine.loadRouterRules(cfgRouter);  // 加载路由规则到CTA引擎
		
	}

	// ========== 初始化策略（根据引擎类型选择） ==========
	if (!_is_hft)  // 如果不是高频引擎（CTA或SEL引擎）
		initCtaStrategies();  // 初始化CTA策略（如果是SEL引擎，此方法会处理SEL策略）
	else  // 如果是高频引擎
		initHftStrategies();  // 初始化HFT策略
	
	return true;
}

/**
 * @brief 初始化CTA策略实现
 * 
 * 从配置文件中加载并初始化CTA策略，创建策略实例和策略上下文，并添加到CTA引擎中。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取策略配置节点（strategies.cta）
 * 2. 遍历配置中的每个策略项
 * 3. 检查策略是否激活（active字段）
 * 4. 从策略管理器创建策略实例
 * 5. 初始化策略（传入策略参数）
 * 6. 创建策略上下文（绑定策略、滑点等信息）
 * 7. 将策略上下文添加到CTA引擎中
 * 
 * 注意事项：
 * - 只有active字段为true的策略才会被加载
 * - 策略ID必须唯一，否则会覆盖同名策略
 * - 滑点参数用于模拟交易成本（价格滑点）
 */
bool WtRtRunner::initCtaStrategies()
{
	WTSVariant* cfg = _config->get("strategies");  // 获取策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	cfg = cfg->get("cta");  // 获取CTA策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不是数组
		return false;  // 返回false，表示初始化失败

	// 遍历配置数组中的每个策略项
	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前策略配置项
		if (!cfgItem->getBoolean("active"))  // 如果策略未激活（active字段为false）
			continue;  // 跳过该策略，不加载

		// 获取策略配置参数
		const char* id = cfgItem->getCString("id");  // 获取策略ID（唯一标识符）
		const char* name = cfgItem->getCString("name");  // 获取策略名称（策略类名）
		int32_t slippage = cfgItem->getInt32("slippage");  // 获取滑点参数（单位：最小变动单位，用于模拟交易成本）

		// 从策略管理器创建策略实例（工厂模式）
		CtaStrategyPtr stra = _cta_mgr.createStrategy(name, id);  // 根据策略名称创建策略实例
		stra->self()->init(cfgItem->get("params"));  // 初始化策略（传入策略参数配置）

		// 创建策略上下文（将策略与引擎关联）
		CtaStraContext* ctx = new CtaStraContext(&_cta_engine, id, slippage);  // 创建策略上下文对象
		ctx->set_strategy(stra->self());  // 将策略实例设置到上下文中
		_cta_engine.addContext(CtaContextPtr(ctx));  // 将策略上下文添加到CTA引擎中（引擎会管理策略的生命周期）
	}

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化SEL策略实现
 * 
 * 从配置文件中加载并初始化SEL（选股）策略，创建策略实例和策略上下文，并添加到SEL引擎中。
 * SEL策略支持按周期调度执行（每日、每周、每月、每年）。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取策略配置节点（strategies.cta，注意SEL策略配置在cta节点下）
 * 2. 遍历配置中的每个策略项
 * 3. 检查策略是否激活（active字段）
 * 4. 解析策略调度参数（日期、时间、周期）
 * 5. 从策略管理器创建策略实例
 * 6. 初始化策略（传入策略参数）
 * 7. 创建策略上下文（绑定策略、滑点等信息）
 * 8. 将策略上下文添加到SEL引擎中（带调度参数）
 * 
 * 注意事项：
 * - SEL策略配置在strategies.cta节点下（与CTA策略共用配置结构）
 * - 只有active字段为true的策略才会被加载
 * - 策略支持按周期调度：每日(d)、每周(w)、每月(m)、每年(y)
 * - 调度参数：date（日期，格式YYYYMMDD）、time（时间，格式HHMMSS）、period（周期类型）
 */
bool WtRtRunner::initSelStrategies()
{
	WTSVariant* cfg = _config->get("strategies");  // 获取策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	cfg = cfg->get("cta");  // 获取CTA策略配置节点（注意：SEL策略配置也在cta节点下）
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不是数组
		return false;  // 返回false，表示初始化失败

	// 遍历配置数组中的每个策略项
	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前策略配置项
		if (!cfgItem->getBoolean("active"))  // 如果策略未激活（active字段为false）
			continue;  // 跳过该策略，不加载

		// 获取策略配置参数
		const char* id = cfgItem->getCString("id");  // 获取策略ID（唯一标识符）
		const char* name = cfgItem->getCString("name");  // 获取策略名称（策略类名）
		int32_t slippage = cfgItem->getInt32("slippage");  // 获取滑点参数（单位：最小变动单位）

		// 获取策略调度参数（用于确定策略执行时间）
		uint32_t date = cfgItem->getUInt32("date");  // 获取调度日期（格式：YYYYMMDD，如20231201）
		uint32_t time = cfgItem->getUInt32("time");  // 获取调度时间（格式：HHMMSS，如93000表示9:30:00）
		const char* period = cfgItem->getCString("period");  // 获取调度周期（"d"=每日，"w"=每周，"m"=每月，"y"=每年）

		// 解析调度周期类型
		TaskPeriodType ptype;  // 调度周期类型枚举
		if (wt_stricmp(period, "d") == 0)  // 如果周期为"d"（每日）
			ptype = TPT_Daily;  // 设置为每日周期
		else if (wt_stricmp(period, "w") == 0)  // 如果周期为"w"（每周）
			ptype = TPT_Weekly;  // 设置为每周周期
		else if (wt_stricmp(period, "m") == 0)  // 如果周期为"m"（每月）
			ptype = TPT_Monthly;  // 设置为每月周期
		else if (wt_stricmp(period, "y") == 0)  // 如果周期为"y"（每年）
			ptype = TPT_Yearly;  // 设置为每年周期
		else
			ptype = TPT_None;  // 否则设置为无周期（不调度）

		// 从策略管理器创建策略实例（工厂模式）
		SelStrategyPtr stra = _sel_mgr.createStrategy(name, id);  // 根据策略名称创建策略实例
		stra->self()->init(cfgItem->get("params"));  // 初始化策略（传入策略参数配置）

		// 创建策略上下文（将策略与引擎关联）
		SelStraContext* ctx = new SelStraContext(&_sel_engine, id, slippage);  // 创建策略上下文对象
		ctx->set_strategy(stra->self());  // 将策略实例设置到上下文中
		// 将策略上下文添加到SEL引擎中（带调度参数：日期、时间、周期）
		_sel_engine.addContext(SelContextPtr(ctx), date, time, ptype);
	}

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化HFT策略实现
 * 
 * 从配置文件中加载并初始化HFT（高频交易）策略，创建策略实例和策略上下文，并添加到HFT引擎中。
 * HFT策略需要绑定交易适配器，用于直接下单交易。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取策略配置节点（strategies.hft）
 * 2. 遍历配置中的每个策略项
 * 3. 检查策略是否激活（active字段）
 * 4. 从策略管理器创建策略实例
 * 5. 初始化策略（传入策略参数）
 * 6. 创建策略上下文（绑定策略、代理模式、滑点等信息）
 * 7. 绑定交易适配器（HFT策略需要直接绑定交易通道）
 * 8. 将策略上下文添加到HFT引擎中
 * 
 * 注意事项：
 * - 只有active字段为true的策略才会被加载
 * - agent字段表示是否为代理模式（代理模式下，策略使用代理订单，不直接下单）
 * - HFT策略必须绑定交易适配器，否则无法下单
 * - 如果交易适配器不存在，会记录错误日志但不会中断初始化流程
 */
bool WtRtRunner::initHftStrategies()
{
	WTSVariant* cfg = _config->get("strategies");  // 获取策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	cfg = cfg->get("hft");  // 获取HFT策略配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不是数组
		return false;  // 返回false，表示初始化失败

	// 遍历配置数组中的每个策略项
	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取当前策略配置项
		if (!cfgItem->getBoolean("active"))  // 如果策略未激活（active字段为false）
			continue;  // 跳过该策略，不加载

		// 获取策略配置参数
		const char* id = cfgItem->getCString("id");  // 获取策略ID（唯一标识符）
		const char* name = cfgItem->getCString("name");  // 获取策略名称（策略类名）
		bool bAgent = cfgItem->getBoolean("agent");  // 获取代理模式标志（true=代理模式，false=直接模式）
		int32_t slippage = cfgItem->getInt32("slippage");  // 获取滑点参数（单位：最小变动单位）

		// 从策略管理器创建策略实例（工厂模式）
		HftStrategyPtr stra = _hft_mgr.createStrategy(name, id);  // 根据策略名称创建策略实例
		if (stra == NULL)  // 如果创建失败（策略类不存在或加载失败）
			continue;  // 跳过该策略，继续处理下一个

		stra->self()->init(cfgItem->get("params"));  // 初始化策略（传入策略参数配置）

		// 创建策略上下文（将策略与引擎关联）
		HftStraContext* ctx = new HftStraContext(&_hft_engine, id, bAgent, slippage);  // 创建策略上下文对象
		ctx->set_strategy(stra->self());  // 将策略实例设置到上下文中

		// 绑定交易适配器（HFT策略需要直接绑定交易通道，用于下单）
		const char* traderid = cfgItem->getCString("trader");  // 获取交易适配器ID
		TraderAdapterPtr trader = _traders.getAdapter(traderid);  // 从交易适配器管理器获取交易适配器
		if (trader)  // 如果交易适配器存在
		{
			ctx->setTrader(trader.get());  // 将交易适配器设置到策略上下文中
			trader->addSink(ctx);  // 将策略上下文添加到交易适配器的接收者列表（用于接收交易回报）
		}
		else  // 如果交易适配器不存在
		{
			WTSLogger::error("Trader {} not exists, Binding trader to HFT strategy failed", traderid);  // 记录错误日志
			// 注意：即使交易适配器不存在，也不会中断初始化流程，策略仍会被添加到引擎中
		}

		_hft_engine.addContext(HftContextPtr(ctx));  // 将策略上下文添加到HFT引擎中（引擎会管理策略的生命周期）
	}

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化交易引擎实现
 * 
 * 根据配置中的引擎类型（env.name），初始化相应的交易引擎（CTA、HFT或SEL引擎）。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取环境配置节点（env）
 * 2. 读取引擎名称（env.name）
 * 3. 根据引擎名称判断引擎类型：
 *    - "cta"或空：CTA引擎（默认）
 *    - "sel"：SEL引擎
 *    - "hft"：HFT引擎
 * 4. 设置引擎类型标志（_is_hft、_is_sel）
 * 5. 初始化对应的引擎（传入配置和依赖的管理器）
 * 6. 设置引擎指针（_engine）
 * 7. 设置交易适配器管理器到引擎
 * 
 * 注意事项：
 * - 必须正确配置env.name，否则会使用默认的CTA引擎
 * - 引擎初始化需要传入基础数据管理器、数据管理器、主力合约管理器和事件通知器
 * - 引擎类型会影响后续的组件初始化（如执行器只在非HFT引擎中初始化）
 */
bool WtRtRunner::initEngine()
{
	WTSVariant* cfg = _config->get("env");  // 获取环境配置节点
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	const char* name = cfg->getCString("name");  // 获取引擎名称（"cta"、"hft"或"sel"）

	// 根据引擎名称判断引擎类型（设置引擎类型标志）
	if (strlen(name) == 0 || wt_stricmp(name, "cta") == 0)  // 如果名称为空或"cta"（CTA引擎）
	{
		_is_hft = false;  // 设置为非高频引擎
		_is_sel = false;  // 设置为非选股引擎
	}
	else if (wt_stricmp(name, "sel") == 0)  // 如果名称为"sel"（SEL引擎）
	{
		_is_sel = true;  // 设置为选股引擎
	}
	else //if (wt_stricmp(name, "hft") == 0)  // 如果名称为"hft"或其他（HFT引擎）
	{
		_is_hft = true;  // 设置为高频引擎
	}

	// 根据引擎类型初始化对应的引擎
	if (_is_hft)  // 如果是高频引擎
	{
		WTSLogger::info("Trading environment initialized, engine name: HFT");  // 记录日志
		_hft_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr, &_notifier);  // 初始化HFT引擎（传入配置和依赖的管理器）
		_engine = &_hft_engine;  // 设置引擎指针为HFT引擎
	}
	else if (_is_sel)  // 如果是选股引擎
	{
		WTSLogger::info("Trading environment initialized, engine name: SEL");  // 记录日志
		_sel_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr, &_notifier);  // 初始化SEL引擎（传入配置和依赖的管理器）
		_engine = &_sel_engine;  // 设置引擎指针为SEL引擎
	}
	else  // 如果是CTA引擎（默认）
	{
		WTSLogger::info("Trading environment initialized, engine name: CTA");  // 记录日志
		_cta_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr, &_notifier);  // 初始化CTA引擎（传入配置和依赖的管理器）
		_engine = &_cta_engine;  // 设置引擎指针为CTA引擎
	}

	_engine->set_adapter_mgr(&_traders);  // 设置交易适配器管理器到引擎（引擎需要管理交易通道）
	
	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化数据管理器实现
 * 
 * 初始化数据管理器，注册数据加载器接口，使数据管理器能够通过外部数据源加载历史数据。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取数据配置节点（data）
 * 2. 注册数据加载器接口（this指向WtRtRunner，实现IBtDataLoader接口）
 * 3. 初始化数据管理器（传入配置、引擎指针和初始标志）
 * 
 * 注意事项：
 * - WtRtRunner实现IBtDataLoader接口，提供外部数据加载能力（如从数据库、文件等加载历史数据）
 * - 数据管理器初始化后，可以通过IBtDataLoader接口加载历史K线、Tick等数据
 * - 初始化标志为true表示立即初始化（加载基础数据等）
 */
bool WtRtRunner::initDataMgr()
{
	WTSVariant* cfg = _config->get("data");  // 获取数据配置节点
	if (cfg == NULL)  // 如果配置不存在
		return false;  // 返回false，表示初始化失败

	_data_mgr.regsiter_loader(this);  // 注册数据加载器接口（this指向WtRtRunner，实现IBtDataLoader接口）
	// 注册后，数据管理器可以通过IBtDataLoader接口加载历史数据（如历史K线、Tick等）

	_data_mgr.init(cfg, _engine, true);  // 初始化数据管理器（传入配置、引擎指针和初始标志）
	// 参数说明：
	// - cfg: 数据配置（包含数据路径、数据格式等）
	// - _engine: 交易引擎指针（数据管理器需要与引擎交互）
	// - true: 初始标志（true表示立即初始化，加载基础数据等）

	WTSLogger::log_raw(LL_INFO, "Data manager initialized");  // 记录日志，表示数据管理器初始化成功
	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化解析器适配器实现
 * 
 * 从配置数组中加载并初始化行情解析器适配器（ParserAdapter），用于接收和处理实时行情数据。
 * 
 * @param cfgParsers 解析器配置数组
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置是否为数组类型
 * 2. 遍历配置数组中的每个解析器项
 * 3. 检查解析器是否激活（active字段）
 * 4. 获取解析器ID（如果为空，自动生成ID）
 * 5. 创建解析器适配器对象
 * 6. 初始化解析器适配器（传入ID、配置、引擎和管理器）
 * 7. 将解析器适配器添加到解析器适配器管理器
 * 
 * 注意事项：
 * - 只有active字段为true的解析器才会被加载
 * - 如果解析器ID为空，会自动生成ID（格式：auto_parser_1000、auto_parser_1001等）
 * - 解析器适配器负责连接行情数据源、解析行情数据并推送给引擎
 * - 支持多个解析器同时运行（如同时连接多个行情源）
 */
bool WtRtRunner::initParsers(WTSVariant* cfgParsers)
{
	if (cfgParsers == NULL || cfgParsers->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不是数组
		return false;  // 返回false，表示初始化失败

	uint32_t count = 0;  // 统计成功加载的解析器数量
	// 遍历配置数组中的每个解析器项
	for (uint32_t idx = 0; idx < cfgParsers->size(); idx++)
	{
		WTSVariant* cfgItem = cfgParsers->get(idx);  // 获取当前解析器配置项
		if (!cfgItem->getBoolean("active"))  // 如果解析器未激活（active字段为false）
			continue;  // 跳过该解析器，不加载

		const char* id = cfgItem->getCString("id");  // 获取解析器ID

		// By Wesley @ 2021.12.14
		// 如果id为空，则生成自动id（避免ID冲突）
		std::string realid = id;  // 保存解析器ID
		if (realid.empty())  // 如果ID为空
		{
			static uint32_t auto_parserid = 1000;  // 静态变量，用于生成唯一ID
			realid = fmt::format("auto_parser_{}", auto_parserid++);  // 生成自动ID（格式：auto_parser_1000、auto_parser_1001等）
		}

		// 创建解析器适配器对象（智能指针管理）
		ParserAdapterPtr adapter(new ParserAdapter);  // 创建解析器适配器对象
		// 初始化解析器适配器（传入ID、配置、引擎和管理器）
		adapter->init(realid.c_str(), cfgItem, _engine, _engine->get_basedata_mgr(), _engine->get_hot_mgr());
		// 参数说明：
		// - realid.c_str(): 解析器ID（唯一标识符）
		// - cfgItem: 解析器配置（包含连接参数、数据格式等）
		// - _engine: 交易引擎指针（解析器需要将行情数据推送给引擎）
		// - _engine->get_basedata_mgr(): 基础数据管理器（用于查询合约信息等）
		// - _engine->get_hot_mgr(): 主力合约管理器（用于查询主力合约信息）

		_parsers.addAdapter(realid.c_str(), adapter);  // 将解析器适配器添加到解析器适配器管理器（管理器会管理解析器的生命周期）

		count++;  // 增加成功加载的解析器计数
	}

	WTSLogger::info("{} parsers loaded", count);  // 记录日志，显示成功加载的解析器数量

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化执行器实现
 * 
 * 从配置数组中加载并初始化执行器（Executer），用于执行策略的交易指令。
 * 支持多种执行器类型：本地执行器（local）、差分执行器（diff）、套利执行器（arbi）、分布式执行器（dist）。
 * 
 * @param cfgExecuter 执行器配置数组
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置是否为数组类型
 * 2. 加载自带的执行器工厂（从安装目录的executer文件夹加载）
 * 3. 遍历配置数组中的每个执行器项
 * 4. 检查执行器是否激活（active字段）
 * 5. 根据执行器类型（name字段）创建对应的执行器对象：
 *    - "local"：本地执行器（WtLocalExecuter）
 *    - "diff"：差分执行器（WtDiffExecuter）
 *    - "arbi"：套利执行器（WtArbiExecuter）
 *    - 其他：分布式执行器（WtDistExecuter）
 * 6. 初始化执行器（传入配置）
 * 7. 绑定交易适配器（本地、差分、套利执行器需要）
 * 8. 将执行器添加到CTA引擎中
 * 
 * 注意事项：
 * - 只有active字段为true的执行器才会被加载
 * - 默认执行器类型为"local"（如果name为空）
 * - 本地、差分、套利执行器必须绑定交易适配器，否则无法下单
 * - 分布式执行器不需要绑定交易适配器（通过网络通信）
 * - 如果执行器初始化失败，会返回false（中断初始化流程）
 */
bool WtRtRunner::initExecuters(WTSVariant* cfgExecuter)
{
	if (cfgExecuter == NULL || cfgExecuter->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不是数组
		return false;  // 返回false，表示初始化失败

	// 先加载自带的执行器工厂（从安装目录的executer文件夹加载执行单元工厂）
	std::string path = WtHelper::getInstDir() + "executer//";  // 获取执行器工厂目录路径
	_exe_factory.loadFactories(path.c_str());  // 加载执行器工厂（加载执行单元的动态库，如TWAP、VWAP等）

	uint32_t count = 0;  // 统计成功加载的执行器数量
	// 遍历配置数组中的每个执行器项
	for (uint32_t idx = 0; idx < cfgExecuter->size(); idx++)
	{
		WTSVariant* cfgItem = cfgExecuter->get(idx);  // 获取当前执行器配置项
		if (!cfgItem->getBoolean("active"))  // 如果执行器未激活（active字段为false）
			continue;  // 跳过该执行器，不加载

		const char* id = cfgItem->getCString("id");  // 获取执行器ID（唯一标识符）
		std::string name = cfgItem->getCString("name");	// 获取执行器类型（local、diff、arbi、dist）
		if (name.empty())  // 如果类型为空
			name = "local";  // 默认为本地执行器

		// 根据执行器类型创建对应的执行器对象
		if(name == "local")  // 如果是本地执行器
		{
			// 创建本地执行器对象（用于执行策略的交易指令，直接下单）
			WtLocalExecuter* executer = new WtLocalExecuter(&_exe_factory, id, &_data_mgr);
			if (!executer->init(cfgItem))  // 初始化执行器（传入配置）
				return false;  // 如果初始化失败，返回false（中断初始化流程）

			// 绑定交易适配器（本地执行器需要交易通道下单）
			const char* tid = cfgItem->getCString("trader");  // 获取交易适配器ID
			if(strlen(tid) == 0)  // 如果交易适配器ID为空
			{
				WTSLogger::error("No Trader configured for Executer {}", id);  // 记录错误日志
			}
			else
			{
				TraderAdapterPtr trader = _traders.getAdapter(tid);  // 从交易适配器管理器获取交易适配器
				if (trader)  // 如果交易适配器存在
				{
					executer->setTrader(trader.get());  // 将交易适配器设置到执行器中
					trader->addSink(executer);  // 将执行器添加到交易适配器的接收者列表（用于接收交易回报）
				}
				else  // 如果交易适配器不存在
				{
					WTSLogger::error("Trader {} not exists, cannot configured for executer %s", tid, id);  // 记录错误日志
				}
			}

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎中（引擎会管理执行器的生命周期）
		}
		else if (name == "diff")  // 如果是差分执行器
		{
			// 创建差分执行器对象（用于执行增量交易指令，只执行目标持仓与当前持仓的差值）
			WtDiffExecuter* executer = new WtDiffExecuter(&_exe_factory, id, &_data_mgr, &_bd_mgr);
			if (!executer->init(cfgItem))  // 初始化执行器（传入配置）
				return false;  // 如果初始化失败，返回false（中断初始化流程）

			// 绑定交易适配器（差分执行器需要交易通道下单）
			const char* tid = cfgItem->getCString("trader");  // 获取交易适配器ID
			if (strlen(tid) == 0)  // 如果交易适配器ID为空
			{
				WTSLogger::error("No Trader configured for Executer {}", id);  // 记录错误日志
			}
			else
			{
				TraderAdapterPtr trader = _traders.getAdapter(tid);  // 从交易适配器管理器获取交易适配器
				if (trader)  // 如果交易适配器存在
				{
					executer->setTrader(trader.get());  // 将交易适配器设置到执行器中
					trader->addSink(executer);  // 将执行器添加到交易适配器的接收者列表（用于接收交易回报）
				}
				else  // 如果交易适配器不存在
				{
					WTSLogger::error("Trader {} not exists, cannot configured for executer %s", tid, id);  // 记录错误日志
				}
			}

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎中（引擎会管理执行器的生命周期）
		}
		else if (name == "arbi")  // 如果是套利执行器
		{
			// 创建套利执行器对象（用于执行套利交易指令，同时管理多个合约的持仓）
			WtArbiExecuter* executer = new WtArbiExecuter(&_exe_factory, id, &_data_mgr);
			if (!executer->init(cfgItem))  // 初始化执行器（传入配置）
				return false;  // 如果初始化失败，返回false（中断初始化流程）

			// 绑定交易适配器（套利执行器需要交易通道下单）
			const char* tid = cfgItem->getCString("trader");  // 获取交易适配器ID
			if (strlen(tid) == 0)  // 如果交易适配器ID为空
			{
				WTSLogger::error("No Trader configured for Executer {}", id);  // 记录错误日志
			}
			else
			{
				TraderAdapterPtr trader = _traders.getAdapter(tid);  // 从交易适配器管理器获取交易适配器
				if (trader)  // 如果交易适配器存在
				{
					executer->setTrader(trader.get());  // 将交易适配器设置到执行器中
					trader->addSink(executer);  // 将执行器添加到交易适配器的接收者列表（用于接收交易回报）
				}
				else  // 如果交易适配器不存在
				{
					WTSLogger::error("Trader {} not exists, cannot configured for executer %s", tid, id);  // 记录错误日志
				}
			}

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎中（引擎会管理执行器的生命周期）
		}
		else  // 如果是分布式执行器或其他类型
		{
			// 创建分布式执行器对象（用于通过网络通信执行交易指令，不直接连接交易通道）
			WtDistExecuter* executer = new WtDistExecuter(id);
			if (!executer->init(cfgItem))  // 初始化执行器（传入配置）
				return false;  // 如果初始化失败，返回false（中断初始化流程）

			// 注意：分布式执行器不需要绑定交易适配器（通过网络通信执行交易）

			_cta_engine.addExecuter(ExecCmdPtr(executer));  // 将执行器添加到CTA引擎中（引擎会管理执行器的生命周期）
		}
		
		count++;  // 增加成功加载的执行器计数
	}

	WTSLogger::info("{} executers loaded", count);  // 记录日志，显示成功加载的执行器数量

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化事件通知器实现
 * 
 * 从配置中初始化事件通知器，用于发送系统事件通知（如日志、错误、警告等）。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取事件通知器配置节点（notifier）
 * 2. 检查配置是否为对象类型
 * 3. 初始化事件通知器（传入配置）
 * 
 * 注意事项：
 * - 事件通知器用于将系统事件（日志、错误等）发送到外部（如邮件、短信、Webhook等）
 * - 配置中包含事件通知的渠道和规则（如什么级别的日志需要通知）
 */
bool WtRtRunner::initEvtNotifier()
{
	WTSVariant* cfg = _config->get("notifier");  // 获取事件通知器配置节点
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置不存在或类型不正确
		return false;  // 返回false，表示初始化失败

	_notifier.init(cfg);  // 初始化事件通知器（传入配置）
	// 配置中包含事件通知的渠道和规则（如邮件、短信、Webhook等）

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化交易适配器实现
 * 
 * 从配置数组中加载并初始化交易适配器（TraderAdapter），用于连接交易通道并处理交易指令。
 * 
 * @param cfgTraders 交易适配器配置数组
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 检查配置是否为数组类型
 * 2. 遍历配置数组中的每个交易适配器项
 * 3. 检查交易适配器是否激活（active字段）
 * 4. 创建交易适配器对象
 * 5. 初始化交易适配器（传入ID、配置、基础数据管理器和开平策略管理器）
 * 6. 将交易适配器添加到交易适配器管理器
 * 
 * 注意事项：
 * - 只有active字段为true的交易适配器才会被加载
 * - 交易适配器负责连接交易通道（如CTP、XTP等），处理下单、撤单等交易操作
 * - 开平策略管理器用于确定开仓和平仓方向（如T+0、T+1规则）
 * - 支持多个交易适配器同时运行（如同时连接多个交易通道）
 */
bool WtRtRunner::initTraders(WTSVariant* cfgTraders)
{
	if (cfgTraders == NULL || cfgTraders->type() != WTSVariant::VT_Array)  // 如果配置不存在或类型不是数组
		return false;  // 返回false，表示初始化失败
	
	uint32_t count = 0;  // 统计成功加载的交易适配器数量
	// 遍历配置数组中的每个交易适配器项
	for (uint32_t idx = 0; idx < cfgTraders->size(); idx++)
	{
		WTSVariant* cfgItem = cfgTraders->get(idx);  // 获取当前交易适配器配置项
		if (!cfgItem->getBoolean("active"))  // 如果交易适配器未激活（active字段为false）
			continue;  // 跳过该交易适配器，不加载

		const char* id = cfgItem->getCString("id");  // 获取交易适配器ID（唯一标识符）

		// 创建交易适配器对象（智能指针管理）
		TraderAdapterPtr adapter(new TraderAdapter(&_notifier));  // 创建交易适配器对象，传入事件通知器
		// 初始化交易适配器（传入ID、配置、基础数据管理器和开平策略管理器）
		adapter->init(id, cfgItem, &_bd_mgr, &_act_policy);
		// 参数说明：
		// - id: 交易适配器ID（唯一标识符）
		// - cfgItem: 交易适配器配置（包含连接参数、账户信息等）
		// - &_bd_mgr: 基础数据管理器（用于查询合约信息等）
		// - &_act_policy: 开平策略管理器（用于确定开仓和平仓方向）

		_traders.addAdapter(id, adapter);  // 将交易适配器添加到交易适配器管理器（管理器会管理交易适配器的生命周期）
		count++;  // 增加成功加载的交易适配器计数
	}

	WTSLogger::info("{} traders loaded", count);  // 记录日志，显示成功加载的交易适配器数量

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 运行运行时运行器实现
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
void WtRtRunner::run(bool bAsync /* = false */)
{
	try  // 捕获异常，确保程序不会因异常而崩溃
	{
		_parsers.run();  // 启动解析器适配器管理器（行情通道），开始接收实时行情数据
		_traders.run();  // 启动交易适配器管理器（交易通道），开始处理交易指令

		_engine->run();  // 启动交易引擎，开始执行策略逻辑
		// 引擎会根据行情数据和策略逻辑执行交易操作

		if (!bAsync)  // 如果是同步模式（阻塞模式）
		{
			// 安装信号钩子，用于捕获程序异常和退出信号（如SIGINT、SIGTERM等）
			install_signal_hooks([this](const char* message) {  // 异常处理回调函数
				if (!_to_exit)  // 如果系统未退出，才记录错误日志
					WTSLogger::error(message);  // 将错误信息记录到日志系统
			}, [this](bool toExit) {  // 退出信号处理回调函数
				if (_to_exit)  // 如果已经设置了退出标志，直接返回
					return;
				_to_exit = toExit;  // 设置退出标志
				WTSLogger::info("Exit flag is {}", _to_exit);  // 记录日志，表示退出标志已设置
			});

			// 进入循环，等待退出信号
			while (!_to_exit)  // 如果退出标志为false，继续循环
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 休眠10毫秒，避免CPU占用过高
				// 每10毫秒检查一次退出标志，收到退出信号后退出循环
			}
		}
		// 如果是异步模式，函数会立即返回，系统在后台运行
	}
	catch (...)  // 捕获所有异常
	{
		// 打印堆栈跟踪信息，用于问题排查
		print_stack_trace([](const char* message) {  // 堆栈跟踪回调函数
			WTSLogger::error(message);  // 将堆栈跟踪信息记录到日志系统
		});
		// 异常被捕获后，程序不会崩溃，但系统可能处于异常状态
	}
}

/**
 * @brief 日志级别标签数组
 * 
 * 用于将日志级别枚举值转换为字符串标签，供事件通知器使用。
 * 日志级别从100开始（100=all, 101=debug, 102=info, 103=warn, 104=error, 105=fatal, 106=none）。
 */
const char* LOG_TAGS[] = {
	"all",    // 全部日志（级别100）
	"debug",  // 调试日志（级别101）
	"info",   // 信息日志（级别102）
	"warn",   // 警告日志（级别103）
	"error",  // 错误日志（级别104）
	"fatal",  // 致命错误日志（级别105）
	"none",   // 无日志（级别106）
};

/**
 * @brief 处理日志追加事件实现
 * 
 * 当日志系统输出日志时，会调用此方法将日志转发给事件通知器。
 * 事件通知器可以将日志发送到外部（如邮件、短信、Webhook等）。
 * 
 * @param ll 日志级别（WTSLogLevel枚举，从100开始）
 * @param msg 日志消息内容
 * 
 * 实现流程：
 * 1. 将日志级别转换为字符串标签（通过数组索引：ll-100）
 * 2. 调用事件通知器发送日志通知
 * 
 * 注意事项：
 * - 此方法由日志系统回调，不应直接调用
 * - 日志级别必须大于等于100，否则数组索引会越界
 */
void WtRtRunner::handleLogAppend(WTSLogLevel ll, const char* msg)
{
	_notifier.notify_log(LOG_TAGS[ll-100], msg);  // 将日志转发给事件通知器（通过日志级别标签和消息内容）
	// LOG_TAGS[ll-100]：将日志级别（从100开始）转换为字符串标签（数组索引从0开始）
}

/**
 * @brief 释放运行时运行器资源实现
 * 
 * 停止日志系统，释放运行时运行器占用的资源。
 * 通常在程序退出前调用，确保资源正确释放。
 * 
 * 释放流程：
 * 1. 停止日志系统（停止日志输出和事件通知）
 * 
 * 注意事项：
 * - 调用此方法后，日志系统将不再输出日志
 * - 此方法应在程序退出前调用，确保资源正确释放
 */
void WtRtRunner::release()
{
	WTSLogger::stop();  // 停止日志系统（停止日志输出和事件通知）
}

/**
 * @brief 初始化开平策略管理器实现
 * 
 * 从配置文件中加载开平策略规则，用于确定开仓和平仓方向（如T+0、T+1规则、锁仓规则等）。
 * 
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化流程：
 * 1. 获取开平策略配置文件路径（bspolicy字段）
 * 2. 检查配置文件路径是否为空
 * 3. 初始化开平策略管理器（从文件加载策略规则）
 * 
 * 注意事项：
 * - 开平策略用于确定交易方向（开仓、平仓）和持仓计算规则
 * - 不同市场有不同的规则（如股票T+1、期货T+0、锁仓规则等）
 * - 配置文件包含各种情况下的开平策略规则
 */
bool WtRtRunner::initActionPolicy()
{
	const char* action_file = _config->getCString("bspolicy");  // 获取开平策略配置文件路径
	if (strlen(action_file) <= 0)  // 如果配置文件路径为空
		return false;  // 返回false，表示初始化失败

	bool ret = _act_policy.init(action_file);  // 初始化开平策略管理器（从文件加载策略规则）
	// 配置文件包含各种情况下的开平策略规则（如T+0、T+1规则、锁仓规则等）

	WTSLogger::info("Action policies initialized");  // 记录日志，表示开平策略管理器初始化成功
	return ret;  // 返回初始化结果
}

/**
 * @brief 添加CTA策略工厂实现
 * 
 * 从指定文件夹加载CTA策略工厂动态库，注册策略类供后续创建策略实例使用。
 * 
 * @param folder 策略工厂文件夹路径（包含策略动态库的目录）
 * @return 加载成功返回true，失败返回false
 * 
 * 加载流程：
 * 1. 扫描文件夹中的动态库文件
 * 2. 加载动态库并查找策略工厂接口
 * 3. 注册策略工厂到策略管理器
 * 
 * 注意事项：
 * - 动态库必须实现IStrategyFactory接口
 * - 策略工厂在运行时动态加载，支持热插拔策略
 */
bool WtRtRunner::addCtaFactories(const char* folder)
{
	return _cta_mgr.loadFactories(folder);  // 从指定文件夹加载CTA策略工厂动态库
}

/**
 * @brief 添加SEL策略工厂实现
 * 
 * 从指定文件夹加载SEL策略工厂动态库，注册策略类供后续创建策略实例使用。
 * 
 * @param folder 策略工厂文件夹路径（包含策略动态库的目录）
 * @return 加载成功返回true，失败返回false
 * 
 * 加载流程：
 * 1. 扫描文件夹中的动态库文件
 * 2. 加载动态库并查找策略工厂接口
 * 3. 注册策略工厂到策略管理器
 * 
 * 注意事项：
 * - 动态库必须实现IStrategyFactory接口
 * - 策略工厂在运行时动态加载，支持热插拔策略
 */
bool WtRtRunner::addSelFactories(const char* folder)
{
	return _sel_mgr.loadFactories(folder);  // 从指定文件夹加载SEL策略工厂动态库
}

/**
 * @brief 添加执行器工厂实现
 * 
 * 从指定文件夹加载执行单元工厂动态库，注册执行单元类供后续创建执行单元实例使用。
 * 
 * @param folder 执行单元工厂文件夹路径（包含执行单元动态库的目录）
 * @return 加载成功返回true，失败返回false
 * 
 * 加载流程：
 * 1. 扫描文件夹中的动态库文件
 * 2. 加载动态库并查找执行单元工厂接口
 * 3. 注册执行单元工厂到执行器工厂管理器
 * 
 * 注意事项：
 * - 动态库必须实现IExecuterFact接口
 * - 执行单元工厂在运行时动态加载，支持热插拔执行单元（如TWAP、VWAP等）
 */
bool WtRtRunner::addExeFactories(const char* folder)
{
	return _exe_factory.loadFactories(folder);  // 从指定文件夹加载执行单元工厂动态库
}

/**
 * @brief 添加HFT策略工厂实现
 * 
 * 从指定文件夹加载HFT策略工厂动态库，注册策略类供后续创建策略实例使用。
 * 
 * @param folder 策略工厂文件夹路径（包含策略动态库的目录）
 * @return 加载成功返回true，失败返回false
 * 
 * 加载流程：
 * 1. 扫描文件夹中的动态库文件
 * 2. 加载动态库并查找策略工厂接口
 * 3. 注册策略工厂到策略管理器
 * 
 * 注意事项：
 * - 动态库必须实现IStrategyFactory接口
 * - 策略工厂在运行时动态加载，支持热插拔策略
 */
bool WtRtRunner::addHftFactories(const char* folder)
{
	return _hft_mgr.loadFactories(folder);  // 从指定文件夹加载HFT策略工厂动态库
}

/**
 * @brief HFT策略委托队列数据回调实现
 * 
 * 当HFT策略的委托队列数据更新时，扩展上下文会调用此方法。
 * 此方法调用HFT委托队列回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID（HFT上下文句柄）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param newOrdQue 委托队列数据对象指针
 * 
 * 实现流程：
 * 1. 检查HFT委托队列回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、合约代码、委托队列结构体指针
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理委托队列数据
 * - 委托队列数据包含买一、卖一委托队列的详细信息（价格、数量等）
 */
void WtRtRunner::hft_on_order_queue(uint32_t id, const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_cb_hft_ordque)  // 如果HFT委托队列回调函数已注册
		_cb_hft_ordque(id, stdCode, &newOrdQue->getOrdQueStruct());  // 调用回调函数，将委托队列数据转发给外部语言
}

/**
 * @brief HFT策略逐笔委托数据回调实现
 * 
 * 当HFT策略的逐笔委托数据更新时，扩展上下文会调用此方法。
 * 此方法调用HFT逐笔委托回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID（HFT上下文句柄）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param newOrdDtl 逐笔委托数据对象指针
 * 
 * 实现流程：
 * 1. 检查HFT逐笔委托回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、合约代码、逐笔委托结构体指针
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理逐笔委托数据
 * - 逐笔委托数据包含每笔委托的详细信息（委托价格、数量、方向等）
 */
void WtRtRunner::hft_on_order_detail(uint32_t id, const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_cb_hft_orddtl)  // 如果HFT逐笔委托回调函数已注册
		_cb_hft_orddtl(id, stdCode, &newOrdDtl->getOrdDtlStruct());  // 调用回调函数，将逐笔委托数据转发给外部语言
}

/**
 * @brief HFT策略逐笔成交数据回调实现
 * 
 * 当HFT策略的逐笔成交数据更新时，扩展上下文会调用此方法。
 * 此方法调用HFT逐笔成交回调函数，将事件转发给外部语言。
 * 
 * @param id 策略上下文的ID（HFT上下文句柄）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param newTrans 逐笔成交数据对象指针
 * 
 * 实现流程：
 * 1. 检查HFT逐笔成交回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：上下文ID、合约代码、逐笔成交结构体指针
 * 
 * 注意事项：
 * - 回调函数可以为NULL，表示不处理逐笔成交数据
 * - 逐笔成交数据包含每笔成交的详细信息（成交价格、数量、方向等）
 */
void WtRtRunner::hft_on_transaction(uint32_t id, const char* stdCode, WTSTransData* newTrans)
{
	if (_cb_hft_trans)  // 如果HFT逐笔成交回调函数已注册
		_cb_hft_trans(id, stdCode, &newTrans->getTransStruct());  // 调用回调函数，将逐笔成交数据转发给外部语言
}

#pragma region "Extended Parser"
/**
 * @brief 扩展行情解析器初始化事件回调实现
 * 
 * 当扩展行情解析器初始化时，会调用此方法通知外部语言。
 * 
 * @param id 解析器ID（唯一标识符）
 * 
 * 实现流程：
 * 1. 检查解析器事件回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：事件类型（EVENT_PARSER_INIT）、解析器ID
 */
void WtRtRunner::parser_init(const char* id)
{
	if (_cb_parser_evt)  // 如果解析器事件回调函数已注册
		_cb_parser_evt(EVENT_PARSER_INIT, id);  // 调用回调函数，将初始化事件转发给外部语言
}

/**
 * @brief 扩展行情解析器连接事件回调实现
 * 
 * 当扩展行情解析器连接成功时，会调用此方法通知外部语言。
 * 
 * @param id 解析器ID（唯一标识符）
 * 
 * 实现流程：
 * 1. 检查解析器事件回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：事件类型（EVENT_PARSER_CONNECT）、解析器ID
 */
void WtRtRunner::parser_connect(const char* id)
{
	if (_cb_parser_evt)  // 如果解析器事件回调函数已注册
		_cb_parser_evt(EVENT_PARSER_CONNECT, id);  // 调用回调函数，将连接事件转发给外部语言
}

/**
 * @brief 扩展行情解析器断开连接事件回调实现
 * 
 * 当扩展行情解析器断开连接时，会调用此方法通知外部语言。
 * 
 * @param id 解析器ID（唯一标识符）
 * 
 * 实现流程：
 * 1. 检查解析器事件回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：事件类型（EVENT_PARSER_DISCONNECT）、解析器ID
 */
void WtRtRunner::parser_disconnect(const char* id)
{
	if (_cb_parser_evt)  // 如果解析器事件回调函数已注册
		_cb_parser_evt(EVENT_PARSER_DISCONNECT, id);  // 调用回调函数，将断开连接事件转发给外部语言
}

/**
 * @brief 扩展行情解析器释放事件回调实现
 * 
 * 当扩展行情解析器释放时，会调用此方法通知外部语言。
 * 
 * @param id 解析器ID（唯一标识符）
 * 
 * 实现流程：
 * 1. 检查解析器事件回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：事件类型（EVENT_PARSER_RELEASE）、解析器ID
 */
void WtRtRunner::parser_release(const char* id)
{
	if (_cb_parser_evt)  // 如果解析器事件回调函数已注册
		_cb_parser_evt(EVENT_PARSER_RELEASE, id);  // 调用回调函数，将释放事件转发给外部语言
}

/**
 * @brief 扩展行情解析器订阅事件回调实现
 * 
 * 当扩展行情解析器订阅行情时，会调用此方法通知外部语言。
 * 
 * @param id 解析器ID（唯一标识符）
 * @param code 合约代码，如"SHFE.rb2305"
 * 
 * 实现流程：
 * 1. 检查解析器订阅回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：解析器ID、合约代码、订阅标志（true表示订阅）
 */
void WtRtRunner::parser_subscribe(const char* id, const char* code)
{
	if (_cb_parser_sub)  // 如果解析器订阅回调函数已注册
		_cb_parser_sub(id, code, true);  // 调用回调函数，将订阅事件转发给外部语言（true表示订阅）
}

/**
 * @brief 扩展行情解析器取消订阅事件回调实现
 * 
 * 当扩展行情解析器取消订阅行情时，会调用此方法通知外部语言。
 * 
 * @param id 解析器ID（唯一标识符）
 * @param code 合约代码，如"SHFE.rb2305"
 * 
 * 实现流程：
 * 1. 检查解析器订阅回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：解析器ID、合约代码、订阅标志（false表示取消订阅）
 */
void WtRtRunner::parser_unsubscribe(const char* id, const char* code)
{
	if (_cb_parser_sub)  // 如果解析器订阅回调函数已注册
		_cb_parser_sub(id, code, false);  // 调用回调函数，将取消订阅事件转发给外部语言（false表示取消订阅）
}

/**
 * @brief 扩展行情解析器行情数据回调实现
 * 
 * 当外部语言实现的扩展行情解析器收到行情数据时，会调用此方法将行情数据推送给系统。
 * 
 * @param id 解析器ID（唯一标识符）
 * @param curTick 行情数据结构体指针（包含价格、数量等信息）
 * @param uProcFlag 处理标志（位运算，表示数据的处理方式）
 * 
 * 实现流程：
 * 1. 从解析器适配器管理器中获取解析器适配器
 * 2. 如果适配器存在，创建行情数据对象
 * 3. 调用适配器的handleQuote方法处理行情数据
 * 4. 释放行情数据对象
 * 
 * 注意事项：
 * - 此方法由外部语言调用，用于将行情数据推送给系统
 * - 如果解析器适配器不存在，会记录警告日志
 * - 处理标志用于控制数据的处理方式（如是否需要更新K线等）
 */
void WtRtRunner::on_ext_parser_quote(const char* id, WTSTickStruct* curTick, uint32_t uProcFlag)
{
	ParserAdapterPtr adapter = _parsers.getAdapter(id);  // 从解析器适配器管理器中获取解析器适配器
	if (adapter)  // 如果适配器存在
	{
		WTSTickData* newTick = WTSTickData::create(*curTick);  // 创建行情数据对象（从结构体复制数据）
		adapter->handleQuote(newTick, uProcFlag);  // 调用适配器的handleQuote方法处理行情数据
		newTick->release();  // 释放行情数据对象（释放内存）
	}
	else  // 如果适配器不存在
	{
		WTSLogger::warn("Parser {} not exists", id);  // 记录警告日志，表示解析器不存在
	}
}

#pragma endregion 

#pragma region "Extended Executer"
/**
 * @brief 扩展执行器初始化事件回调实现
 * 
 * 当扩展执行器初始化时，会调用此方法通知外部语言。
 * 
 * @param id 执行器ID（唯一标识符）
 * 
 * 实现流程：
 * 1. 检查执行器初始化回调函数是否已注册
 * 2. 如果已注册，调用回调函数将事件转发给外部语言
 * 3. 回调函数参数：执行器ID
 */
void WtRtRunner::executer_init(const char* id)
{
	if (_cb_exec_init)  // 如果执行器初始化回调函数已注册
		_cb_exec_init(id);  // 调用回调函数，将初始化事件转发给外部语言
}

/**
 * @brief 扩展执行器设置目标持仓回调实现
 * 
 * 当需要设置扩展执行器的目标持仓时，会调用此方法通知外部语言。
 * 
 * @param id 执行器ID（唯一标识符）
 * @param stdCode 标准合约代码，如"SHFE.rb2305"
 * @param target 目标持仓数量（正数表示多头，负数表示空头，0表示平仓）
 * 
 * 实现流程：
 * 1. 检查执行器命令回调函数是否已注册
 * 2. 如果已注册，调用回调函数将命令转发给外部语言
 * 3. 回调函数参数：执行器ID、合约代码、目标持仓数量
 * 
 * 注意事项：
 * - 此方法由系统调用，用于向外部语言实现的执行器发送交易指令
 * - 外部语言执行器收到指令后，需要执行相应的交易操作
 */
void WtRtRunner::executer_set_position(const char* id, const char* stdCode, double target)
{
	if (_cb_exec_cmd)  // 如果执行器命令回调函数已注册
		_cb_exec_cmd(id, stdCode, target);  // 调用回调函数，将目标持仓命令转发给外部语言
}
#pragma endregion