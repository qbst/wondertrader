/*!
 * \file WtRtRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtRtRunner运行时运行器类定义文件
 * 
 * 本文件定义了WtRtRunner类，这是WtPorter模块的核心类，负责管理整个交易系统的运行时环境。
 * WtRtRunner是WonderTrader框架与外部语言交互的核心桥梁，实现了：
 * 1. 交易引擎管理（CTA、HFT、SEL引擎的创建和管理）
 * 2. 策略上下文管理（创建和管理各种策略上下文实例）
 * 3. 回调函数管理（注册和管理各种回调函数，将事件转发给外部语言）
 * 4. 数据加载管理（支持外部数据源加载历史数据）
 * 5. 扩展组件管理（Parser、Executer的创建和管理）
 * 6. 事件通知（引擎事件、交易日事件等的通知）
 * 
 * 设计逻辑：
 * - WtRtRunner作为单例对象，管理整个系统的生命周期
 * - 通过回调函数机制实现事件驱动的编程模型
 * - 支持多种策略类型（CTA、HFT、SEL），根据配置选择对应的引擎
 * - 通过适配器模式管理交易通道和行情通道
 * - 支持外部数据加载器，允许从自定义数据源加载历史数据
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
 * 定义不同类型的交易引擎
 */
typedef enum tagEngineType
{
	ET_CTA = 999,	// CTA引擎（商品交易顾问策略引擎）
	ET_HFT,			// 高频引擎（高频交易策略引擎）
	ET_SEL			// 选股引擎（选股策略引擎）
} EngineType;

class WtRtRunner : public IEngineEvtListener, public ILogHandler, public IHisDataLoader
{
public:
	WtRtRunner();
	~WtRtRunner();

public:
	//////////////////////////////////////////////////////////////////////////
	//IBtDataLoader
	virtual bool loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) override;

	virtual bool loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) override;

	virtual bool loadAllAdjFactors(void* obj, FuncReadFactors cb) override;

	virtual bool loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb) override;

	void feedRawBars(WTSBarStruct* bars, uint32_t count);

	void feedAdjFactors(const char* stdCode, uint32_t* dates, double* factors, uint32_t count);

public:
	/*
	 *	初始化
	 */
	bool init(const char* logCfg = "logcfg.prop", bool isFile = true, const char* genDir = "");

	bool config(const char* cfgFile, bool isFile = true);

	void run(bool bAsync = false);

	void release();

	void registerCtaCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCondTriggerCallback cbCondTrigger = NULL);
	void registerSelCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt);
	void registerHftCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar,
		FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust,
		FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt, FuncHftPosCallback cbPosition);

	void registerEvtCallback(FuncEventCallback cbEvt);

	void registerParserPorter(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub);

	void registerExecuterPorter(FuncExecInitCallback cbInit, FuncExecCmdCallback cbExec);

	void		registerExtDataLoader(FuncLoadFnlBars fnlBarLoader, FuncLoadRawBars rawBarLoader, FuncLoadAdjFactors fctLoader, FuncLoadRawTicks tickLoader = NULL)
	{
		_ext_fnl_bar_loader = fnlBarLoader;
		_ext_raw_bar_loader = rawBarLoader;
		_ext_adj_fct_loader = fctLoader;
	}

	bool			createExtParser(const char* id);
	bool			createExtExecuter(const char* id);

	uint32_t		createCtaContext(const char* name, int32_t slippage);
	uint32_t		createHftContext(const char* name, const char* trader, bool bAgent, int32_t slippage);
	uint32_t		createSelContext(const char* name, uint32_t date, uint32_t time, const char* period, int32_t slippage, const char* trdtpl = "CHINA", const char* session="TRADING");

	CtaContextPtr	getCtaContext(uint32_t id);
	SelContextPtr	getSelContext(uint32_t id);
	HftContextPtr	getHftContext(uint32_t id);
	WtEngine*		getEngine(){ return _engine; }

	const char*	get_raw_stdcode(const char* stdCode);

//////////////////////////////////////////////////////////////////////////
//ILogHandler
public:
	virtual void handleLogAppend(WTSLogLevel ll, const char* msg) override;

//////////////////////////////////////////////////////////////////////////
//扩展Parser
public:
	void parser_init(const char* id);
	void parser_connect(const char* id);
	void parser_release(const char* id);
	void parser_disconnect(const char* id);
	void parser_subscribe(const char* id, const char* code);
	void parser_unsubscribe(const char* id, const char* code);

	void on_ext_parser_quote(const char* id, WTSTickStruct* curTick, uint32_t uProcFlag);


//////////////////////////////////////////////////////////////////////////
//扩展Executer
public:
	void executer_set_position(const char* id, const char* stdCode, double target);
	void executer_init(const char* id);

//////////////////////////////////////////////////////////////////////////
//ICtaEventListener
public:
	virtual void on_initialize_event() override
	{
		if (_cb_evt)
			_cb_evt(EVENT_ENGINE_INIT, 0, 0);
	}

	virtual void on_schedule_event(uint32_t uDate, uint32_t uTime) override
	{
		if (_cb_evt)
			_cb_evt(EVENT_ENGINE_SCHDL, uDate, uTime);
	}

	virtual void on_session_event(uint32_t uDate, bool isBegin = true) override
	{
		if (_cb_evt)
			_cb_evt(isBegin ? EVENT_SESSION_BEGIN : EVENT_SESSION_END, uDate, 0);
	}

public:
	void ctx_on_init(uint32_t id, EngineType eType = ET_CTA);
	void ctx_on_session_event(uint32_t id, uint32_t curTDate, bool isBegin = true, EngineType eType = ET_CTA);
	void ctx_on_tick(uint32_t id, const char* stdCode, WTSTickData* newTick, EngineType eType = ET_CTA);
	void ctx_on_calc(uint32_t id, uint32_t curDate, uint32_t curTime, EngineType eType = ET_CTA);
	void ctx_on_bar(uint32_t id, const char* stdCode, const char* period, WTSBarStruct* newBar, EngineType eType = ET_CTA);
	void ctx_on_cond_triggered(uint32_t id, const char* stdCode, double target, double price, const char* usertag, EngineType eType = ET_CTA);

	void hft_on_channel_ready(uint32_t cHandle, const char* trader);
	void hft_on_channel_lost(uint32_t cHandle, const char* trader);
	void hft_on_order(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag);
	void hft_on_trade(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag);
	void hft_on_entrust(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag);
	void hft_on_position(uint32_t cHandle, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail);

	void hft_on_order_queue(uint32_t id, const char* stdCode, WTSOrdQueData* newOrdQue);
	void hft_on_order_detail(uint32_t id, const char* stdCode, WTSOrdDtlData* newOrdDtl);
	void hft_on_transaction(uint32_t id, const char* stdCode, WTSTransData* newTranns);

	bool addExeFactories(const char* folder);
	bool addCtaFactories(const char* folder);
	bool addHftFactories(const char* folder);
	bool addSelFactories(const char* folder);

private:
	bool initTraders(WTSVariant* cfgTrader);
	bool initParsers(WTSVariant* cfgParser);
	bool initExecuters(WTSVariant* cfgExecuter);
	bool initDataMgr();
	bool initEvtNotifier();
	bool initCtaStrategies();
	bool initHftStrategies();
	bool initSelStrategies();
	bool initActionPolicy();

	bool initEngine();

private:
	FuncStraInitCallback	_cb_cta_init;
	FuncSessionEvtCallback	_cb_cta_sessevt;
	FuncStraTickCallback	_cb_cta_tick;
	FuncStraCalcCallback	_cb_cta_calc;
	FuncStraBarCallback		_cb_cta_bar;
	FuncStraCondTriggerCallback _cb_cta_cond_trigger;

	FuncStraInitCallback	_cb_sel_init;
	FuncSessionEvtCallback	_cb_sel_sessevt;
	FuncStraTickCallback	_cb_sel_tick;
	FuncStraCalcCallback	_cb_sel_calc;
	FuncStraBarCallback		_cb_sel_bar;

	FuncStraInitCallback	_cb_hft_init;
	FuncSessionEvtCallback	_cb_hft_sessevt;
	FuncStraTickCallback	_cb_hft_tick;
	FuncStraBarCallback		_cb_hft_bar;
	FuncHftChannelCallback	_cb_hft_chnl;
	FuncHftOrdCallback		_cb_hft_ord;
	FuncHftTrdCallback		_cb_hft_trd;
	FuncHftEntrustCallback	_cb_hft_entrust;
	FuncHftPosCallback		_cb_hft_position;

	FuncStraOrdQueCallback	_cb_hft_ordque;
	FuncStraOrdDtlCallback	_cb_hft_orddtl;
	FuncStraTransCallback	_cb_hft_trans;

	FuncEventCallback		_cb_evt;

	FuncParserEvtCallback	_cb_parser_evt;
	FuncParserSubCallback	_cb_parser_sub;

	FuncExecCmdCallback		_cb_exec_cmd;
	FuncExecInitCallback	_cb_exec_init;

	WTSVariant*			_config;
	TraderAdapterMgr	_traders;
	ParserAdapterMgr	_parsers;
	WtExecuterFactory	_exe_factory;

	WtCtaEngine			_cta_engine;
	WtHftEngine			_hft_engine;
	WtSelEngine			_sel_engine;
	WtEngine*			_engine;

	WtDataStorage*		_data_store;

	WtDtMgr				_data_mgr;

	WTSBaseDataMgr		_bd_mgr;
	WTSHotMgr			_hot_mgr;
	EventNotifier		_notifier;

	CtaStrategyMgr		_cta_mgr;
	HftStrategyMgr		_hft_mgr;
	SelStrategyMgr		_sel_mgr;
	ActionPolicyMgr		_act_policy;

	bool				_is_hft;
	bool				_is_sel;
	bool				_to_exit;

	FuncLoadFnlBars		_ext_fnl_bar_loader;
	FuncLoadRawBars		_ext_raw_bar_loader;
	FuncLoadAdjFactors	_ext_adj_fct_loader;

	void*			_feed_obj;
	FuncReadBars	_feeder_bars;
	FuncReadFactors	_feeder_fcts;
	StdUniqueMutex	_feed_mtx;
};

