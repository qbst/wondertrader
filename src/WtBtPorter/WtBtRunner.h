/*!
 * \file WtBtRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtBtRunner回测运行器类定义文件
 * 
 * 本文件定义了WtBtRunner类，这是WtBtPorter回测模块的核心类，负责管理整个回测系统的运行时环境。
 * WtBtRunner是WonderTrader回测框架与外部语言交互的核心桥梁，实现了：
 * 1. 历史数据加载管理（实现IBtDataLoader接口，支持从外部数据源加载历史数据）
 * 2. 策略模拟器管理（创建和管理CTA、HFT、SEL策略模拟器实例）
 * 3. 回调函数管理（注册和管理各种回调函数，将事件转发给外部语言）
 * 4. 数据回放管理（通过HisDataReplayer回放历史数据，驱动策略执行）
 * 5. 事件通知（引擎事件、交易日事件、回测结束事件等的通知）
 * 6. 外部数据推送（接收外部推送的K线、Tick、复权因子等数据）
 * 
 * 设计逻辑：
 * - WtBtRunner作为单例对象，管理整个回测系统的生命周期
 * - 通过回调函数机制实现事件驱动的编程模型
 * - 支持多种策略类型（CTA、HFT、SEL），每种策略类型对应一个模拟器实例
 * - 通过HisDataReplayer回放历史数据，模拟真实交易环境
 * - 支持外部数据加载器，允许从自定义数据源加载历史数据
 * - 支持同步和异步两种回测模式
 */
#pragma once
#include "PorterDefs.h"  // Porter模块定义文件
#include "../WtBtCore/EventNotifier.h"  // 事件通知器
#include "../WtBtCore/HisDataReplayer.h"  // 历史数据回放器
#include "../Includes/WTSMarcos.h"  // WonderTrader宏定义


NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSTickData;  // 前向声明：Tick数据类
struct WTSBarStruct;  // 前向声明：K线数据结构
class WTSVariant;  // 前向声明：变体类型（用于配置）
NS_WTP_END  // WonderTrader命名空间结束

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 引擎类型枚举
 * 
 * 定义不同类型的交易引擎，用于标识策略类型
 */
typedef enum tagEngineType
{
	ET_CTA = 999,	// CTA引擎（商品交易顾问策略引擎）
	ET_HFT,			// 高频引擎（高频交易策略引擎）
	ET_SEL			// 选股引擎（选股策略引擎）
} EngineType;

class SelMocker;  // 前向声明：SEL策略模拟器类
class CtaMocker;  // 前向声明：CTA策略模拟器类
class HftMocker;  // 前向声明：HFT策略模拟器类
class ExecMocker;  // 前向声明：执行器模拟器类

/**
 * @brief WtBtRunner回测运行器类
 * 
 * 回测模块的核心类，负责管理整个回测系统的运行时环境。
 * 继承自IBtDataLoader接口，实现历史数据加载功能。
 */
class WtBtRunner : public IBtDataLoader
{
public:
	WtBtRunner();  // 构造函数
	~WtBtRunner();  // 析构函数


	//////////////////////////////////////////////////////////////////////////
	//IBtDataLoader接口实现
	/**
	 * @brief 加载最终K线数据
	 * 
	 * 实现IBtDataLoader接口，加载指定合约和周期的最终K线数据（已复权）
	 * 
	 * @param obj 用户对象指针（传递给回调函数）
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param cb 回调函数，用于接收K线数据
	 * @return 是否加载成功
	 */
	virtual bool loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) override;

	/**
	 * @brief 加载原始K线数据
	 * 
	 * 实现IBtDataLoader接口，加载指定合约和周期的原始K线数据（未复权）
	 * 
	 * @param obj 用户对象指针（传递给回调函数）
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param cb 回调函数，用于接收K线数据
	 * @return 是否加载成功
	 */
	virtual bool loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) override;

	/**
	 * @brief 加载所有复权因子
	 * 
	 * 实现IBtDataLoader接口，加载所有合约的复权因子数据
	 * 
	 * @param obj 用户对象指针（传递给回调函数）
	 * @param cb 回调函数，用于接收复权因子数据
	 * @return 是否加载成功
	 */
	virtual bool loadAllAdjFactors(void* obj, FuncReadFactors cb) override;

	/**
	 * @brief 加载复权因子
	 * 
	 * 实现IBtDataLoader接口，加载指定合约的复权因子数据
	 * 
	 * @param obj 用户对象指针（传递给回调函数）
	 * @param stdCode 标准合约代码
	 * @param cb 回调函数，用于接收复权因子数据
	 * @return 是否加载成功
	 */
	virtual bool loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb) override;

	/**
	 * @brief 加载原始Tick数据
	 * 
	 * 实现IBtDataLoader接口，加载指定合约和日期的原始Tick数据
	 * 
	 * @param obj 用户对象指针（传递给回调函数）
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日（格式：YYYYMMDD）
	 * @param cb 回调函数，用于接收Tick数据
	 * @return 是否加载成功
	 */
	virtual bool loadRawHisTicks(void* obj, const char* stdCode, uint32_t uDate, FuncReadTicks cb) override;

	/**
	 * @brief 是否自动转储数据
	 * 
	 * 实现IBtDataLoader接口，返回是否自动转储数据到本地缓存
	 * 
	 * @return true表示自动转储，false表示不转储
	 */
	virtual bool isAutoTrans() override
	{
		return _loader_auto_trans;  // 返回自动转储标志
	}

	/**
	 * @brief 推送原始K线数据
	 * 
	 * 将外部数据源的原始K线数据推送到回测引擎中
	 * 
	 * @param bars K线数据数组指针
	 * @param count K线数据条数
	 */
	void feedRawBars(WTSBarStruct* bars, uint32_t count);

	/**
	 * @brief 推送原始Tick数据
	 * 
	 * 将外部数据源的原始Tick数据推送到回测引擎中
	 * 
	 * @param ticks Tick数据数组指针
	 * @param count Tick数据条数
	 */
	void feedRawTicks(WTSTickStruct* ticks, uint32_t count);

	/**
	 * @brief 推送复权因子数据
	 * 
	 * 将外部数据源的复权因子数据推送到回测引擎中
	 * 
	 * @param stdCode 标准合约代码
	 * @param dates 日期数组指针（格式：YYYYMMDD）
	 * @param factors 复权因子数组指针
	 * @param count 数据条数
	 */
	void feedAdjFactors(const char* stdCode, uint32_t* dates, double* factors, uint32_t count);

public:
	/**
	 * @brief 注册CTA策略回调函数
	 * 
	 * 注册CTA策略的各种事件回调函数
	 * 
	 * @param cbInit 策略初始化回调函数
	 * @param cbTick Tick更新回调函数
	 * @param cbCalc 策略计算回调函数
	 * @param cbBar K线闭合回调函数
	 * @param cbSessEvt 交易日事件回调函数
	 * @param cbCalcDone 策略计算完成回调函数（可选）
	 * @param cbCondTrigger 条件单触发回调函数（可选）
	 */
	void	registerCtaCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
		FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone = NULL, FuncStraCondTriggerCallback cbCondTrigger = NULL);

	/**
	 * @brief 注册SEL策略回调函数
	 * 
	 * 注册选股策略的各种事件回调函数
	 * 
	 * @param cbInit 策略初始化回调函数
	 * @param cbTick Tick更新回调函数
	 * @param cbCalc 策略计算回调函数
	 * @param cbBar K线闭合回调函数
	 * @param cbSessEvt 交易日事件回调函数
	 * @param cbCalcDone 策略计算完成回调函数（可选）
	 */
	void	registerSelCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
		FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone = NULL);

	/**
	 * @brief 注册HFT策略回调函数
	 * 
	 * 注册高频交易策略的各种事件回调函数
	 * 
	 * @param cbInit 策略初始化回调函数
	 * @param cbTick Tick更新回调函数
	 * @param cbBar K线闭合回调函数
	 * @param cbChnl 交易通道事件回调函数
	 * @param cbOrd 订单状态变化回调函数
	 * @param cbTrd 成交回报回调函数
	 * @param cbEntrust 委托回报回调函数
	 * @param cbOrdDtl 订单明细更新回调函数
	 * @param cbOrdQue 订单队列更新回调函数
	 * @param cbTrans 逐笔成交更新回调函数
	 * @param cbSessEvt 交易日事件回调函数
	 */
	void registerHftCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar,
		FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust,
		FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt);

	/**
	 * @brief 注册引擎事件回调函数
	 * 
	 * 注册回测引擎的事件回调函数
	 * 
	 * @param cbEvt 事件回调函数指针
	 */
	void registerEvtCallback(FuncEventCallback cbEvt)
	{
		_cb_evt = cbEvt;  // 保存事件回调函数指针
	}

	/**
	 * @brief 注册外部数据加载器
	 * 
	 * 注册外部数据加载器的回调函数
	 * 
	 * @param fnlBarLoader 加载最终K线数据的回调函数（已复权）
	 * @param rawBarLoader 加载原始K线数据的回调函数（未复权）
	 * @param fctLoader 加载复权因子的回调函数
	 * @param tickLoader 加载原始Tick数据的回调函数
	 * @param bAutoTrans 是否自动转储数据到本地缓存（默认true）
	 */
	void		registerExtDataLoader(FuncLoadFnlBars fnlBarLoader, FuncLoadRawBars rawBarLoader, FuncLoadAdjFactors fctLoader, FuncLoadRawTicks tickLoader, bool bAutoTrans = true)
	{
		_ext_fnl_bar_loader = fnlBarLoader;  // 保存最终K线加载器回调函数
		_ext_raw_bar_loader = rawBarLoader;  // 保存原始K线加载器回调函数
		_ext_adj_fct_loader = fctLoader;  // 保存复权因子加载器回调函数
		_ext_tick_loader = tickLoader;  // 保存Tick加载器回调函数
		_loader_auto_trans = bAutoTrans;  // 保存自动转储标志
	}

	/**
	 * @brief 初始化CTA策略模拟器
	 * 
	 * 创建一个新的CTA策略模拟器实例
	 * 
	 * @param name 策略名称
	 * @param slippage 滑点设置（单位：最小变动价位，0表示不设置滑点）
	 * @param hook true表示启用钩子模式（用于调试），false表示正常模式（默认false）
	 * @param persistData true表示持久化策略数据（程序重启后可恢复），false表示不持久化（默认true）
	 * @param bIncremental true表示增量回测模式，false表示全量回测模式（默认false）
	 * @param isRatioSlp true表示使用比例滑点（滑点=价格*比例），false表示使用固定滑点（默认false）
	 * @return 策略上下文ID（用于标识策略实例）
	 */
	uint32_t	initCtaMocker(const char* name, int32_t slippage = 0, bool hook = false, bool persistData = true, bool bIncremental = false, bool isRatioSlp = false);

	/**
	 * @brief 初始化HFT策略模拟器
	 * 
	 * 创建一个新的HFT策略模拟器实例
	 * 
	 * @param name 策略名称
	 * @param hook true表示启用钩子模式（用于调试），false表示正常模式（默认false）
	 * @return 策略上下文ID（用于标识策略实例）
	 */
	uint32_t	initHftMocker(const char* name, bool hook = false);

	/**
	 * @brief 初始化SEL策略模拟器
	 * 
	 * 创建一个新的选股策略模拟器实例
	 * 
	 * @param name 策略名称
	 * @param date 策略开始日期（格式：YYYYMMDD）
	 * @param time 策略开始时间（格式：HHMMSS）
	 * @param period 策略执行周期（"d"-日线，"w"-周线，"m"-月线，"y"-年线）
	 * @param trdtpl 交易模板名称（默认为"CHINA"，表示中国A股市场）
	 * @param session 交易时段名称（默认为"TRADING"，表示交易时段）
	 * @param slippage 滑点设置（单位：最小变动价位，0表示不设置滑点）
	 * @param isRatioSlp true表示使用比例滑点，false表示使用固定滑点（默认false）
	 * @return 策略上下文ID（用于标识策略实例）
	 */
	uint32_t	initSelMocker(const char* name, uint32_t date, uint32_t time, const char* period, 
		const char* trdtpl = "CHINA", const char* session = "TRADING", int32_t slippage = 0, bool isRatioSlp = false);

	/**
	 * @brief 初始化事件通知器
	 * 
	 * 根据配置初始化事件通知器
	 * 
	 * @param cfg 配置对象指针
	 * @return 是否初始化成功
	 */
	bool	initEvtNotifier(WTSVariant* cfg);

	/**
	 * @brief 策略初始化事件通知
	 * 
	 * 当策略初始化完成时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param eType 引擎类型（ET_CTA、ET_HFT、ET_SEL）
	 */
	void	ctx_on_init(uint32_t id, EngineType eType);

	/**
	 * @brief 交易日事件通知
	 * 
	 * 当交易日开始或结束时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param curTDate 当前交易日（格式：YYYYMMDD）
	 * @param isBegin true表示交易日开始，false表示交易日结束（默认true）
	 * @param eType 引擎类型（默认ET_CTA）
	 */
	void	ctx_on_session_event(uint32_t id, uint32_t curTDate, bool isBegin = true, EngineType eType = ET_CTA);

	/**
	 * @brief Tick更新事件通知
	 * 
	 * 当订阅的合约有新的Tick数据时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * @param eType 引擎类型
	 */
	void	ctx_on_tick(uint32_t id, const char* stdCode, WTSTickData* newTick, EngineType eType);

	/**
	 * @brief 策略计算事件通知
	 * 
	 * 当策略需要执行计算逻辑时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param uDate 当前日期（格式：YYYYMMDD）
	 * @param uTime 当前时间（格式：HHMMSS）
	 * @param eType 引擎类型
	 */
	void	ctx_on_calc(uint32_t id, uint32_t uDate, uint32_t uTime, EngineType eType);

	/**
	 * @brief 策略计算完成事件通知
	 * 
	 * 当策略计算逻辑执行完毕后调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param uDate 当前日期（格式：YYYYMMDD）
	 * @param uTime 当前时间（格式：HHMMSS）
	 * @param eType 引擎类型
	 */
	void	ctx_on_calc_done(uint32_t id, uint32_t uDate, uint32_t uTime, EngineType eType);

	/**
	 * @brief K线闭合事件通知
	 * 
	 * 当订阅的K线周期完成并生成新的K线时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param newBar 新生成的K线数据指针
	 * @param eType 引擎类型
	 */
	void	ctx_on_bar(uint32_t id, const char* stdCode, const char* period, WTSBarStruct* newBar, EngineType eType);

	/**
	 * @brief 条件单触发事件通知
	 * 
	 * 当策略设置的条件单被触发时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param stdCode 标准合约代码
	 * @param target 目标持仓数量
	 * @param price 触发价格
	 * @param usertag 用户标签
	 * @param eType 引擎类型（默认ET_CTA）
	 */
	void	ctx_on_cond_triggered(uint32_t id, const char* stdCode, double target, double price, const char* usertag, EngineType eType = ET_CTA);

	/**
	 * @brief HFT订单队列更新事件通知
	 * 
	 * 当订阅的合约有新的订单队列数据时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 新的订单队列数据指针
	 */
	void	hft_on_order_queue(uint32_t id, const char* stdCode, WTSOrdQueData* newOrdQue);

	/**
	 * @brief HFT订单明细更新事件通知
	 * 
	 * 当订阅的合约有新的订单明细数据时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 新的订单明细数据指针
	 */
	void	hft_on_order_detail(uint32_t id, const char* stdCode, WTSOrdDtlData* newOrdDtl);

	/**
	 * @brief HFT逐笔成交更新事件通知
	 * 
	 * 当订阅的合约有新的逐笔成交数据时调用，通知外部语言
	 * 
	 * @param id 策略上下文ID
	 * @param stdCode 标准合约代码
	 * @param newTranns 新的逐笔成交数据指针
	 */
	void	hft_on_transaction(uint32_t id, const char* stdCode, WTSTransData* newTranns);

	/**
	 * @brief HFT交易通道就绪事件通知
	 * 
	 * 当HFT策略的交易通道连接成功并准备就绪时调用，通知外部语言
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param trader 交易通道ID
	 */
	void	hft_on_channel_ready(uint32_t cHandle, const char* trader);

	/**
	 * @brief HFT订单状态变化事件通知
	 * 
	 * 当HFT策略的订单状态发生变化时调用，通知外部语言
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy true表示买入，false表示卖出
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余未成交数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤单
	 * @param userTag 用户标签
	 */
	void	hft_on_order(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag);

	/**
	 * @brief HFT成交回报事件通知
	 * 
	 * 当HFT策略的订单有成交回报时调用，通知外部语言
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy true表示买入，false表示卖出
	 * @param vol 成交数量
	 * @param price 成交价格
	 * @param userTag 用户标签
	 */
	void	hft_on_trade(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag);

	/**
	 * @brief HFT委托回报事件通知
	 * 
	 * 当HFT策略的委托单提交后收到回报时调用，通知外部语言
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功
	 * @param message 返回消息
	 * @param userTag 用户标签
	 */
	void	hft_on_entrust(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag);

	/**
	 * @brief 初始化回测引擎
	 * 
	 * 初始化回测引擎，设置日志配置和输出目录
	 * 
	 * @param logProfile 日志配置文件路径或配置内容（默认为空字符串）
	 * @param isFile true表示logProfile是文件路径，false表示logProfile是配置内容（默认true）
	 * @param outDir 回测结果输出目录（默认为"./outputs_bt"）
	 */
	void	init(const char* logProfile = "", bool isFile = true, const char* outDir = "./outputs_bt");

	/**
	 * @brief 配置回测引擎
	 * 
	 * 加载并应用配置文件，配置回测引擎的各种参数
	 * 
	 * @param cfgFile 配置文件路径或配置内容（JSON格式）
	 * @param isFile true表示cfgFile是文件路径，false表示cfgFile是配置内容（默认true）
	 */
	void	config(const char* cfgFile, bool isFile = true);

	/**
	 * @brief 运行回测
	 * 
	 * 启动回测引擎，开始执行回测
	 * 
	 * @param bNeedDump true表示需要输出回测结果到文件，false表示不输出（默认false）
	 * @param bAsync true表示异步运行（函数立即返回），false表示同步运行（函数阻塞直到回测完成，默认false）
	 */
	void	run(bool bNeedDump = false, bool bAsync = false);

	/**
	 * @brief 释放回测引擎
	 * 
	 * 清理资源，释放回测引擎占用的资源
	 */
	void	release();

	/**
	 * @brief 停止回测
	 * 
	 * 停止正在运行的回测（异步回测模式下使用）
	 */
	void	stop();

	/**
	 * @brief 设置回测时间范围
	 * 
	 * 设置回测的开始时间和结束时间
	 * 
	 * @param stime 开始时间戳（格式：YYYYMMDDHHMMSS）
	 * @param etime 结束时间戳（格式：YYYYMMDDHHMMSS）
	 */
	void	set_time_range(WtUInt64 stime, WtUInt64 etime);

	/**
	 * @brief 启用/禁用Tick回放
	 * 
	 * 控制回测引擎是否回放Tick数据（如果禁用，则只回放K线数据）
	 * 
	 * @param bEnabled true表示启用Tick回放，false表示禁用Tick回放（默认启用）
	 */
	void	enable_tick(bool bEnabled = true);

	/**
	 * @brief 清空缓存
	 * 
	 * 清空回测引擎的数据缓存
	 */
	void	clear_cache();

	/**
	 * @brief 获取原始标准代码
	 * 
	 * 将标准合约代码转换为原始合约代码（去除复权、主力等后缀）
	 * 
	 * @param stdCode 标准合约代码（如"SHFE.rb2305.HOT"）
	 * @return 原始合约代码（如"SHFE.rb2305"）
	 */
	const char*	get_raw_stdcode(const char* stdCode);

	/**
	 * @brief 获取CTA模拟器对象
	 * 
	 * 返回CTA策略模拟器对象的指针
	 * 
	 * @return CTA模拟器对象指针（如果不存在则返回NULL）
	 */
	inline CtaMocker*		cta_mocker() { return _cta_mocker; }

	/**
	 * @brief 获取SEL模拟器对象
	 * 
	 * 返回SEL策略模拟器对象的指针
	 * 
	 * @return SEL模拟器对象指针（如果不存在则返回NULL）
	 */
	inline SelMocker*		sel_mocker() { return _sel_mocker; }

	/**
	 * @brief 获取HFT模拟器对象
	 * 
	 * 返回HFT策略模拟器对象的指针
	 * 
	 * @return HFT模拟器对象指针（如果不存在则返回NULL）
	 */
	inline HftMocker*		hft_mocker() { return _hft_mocker; }

	/**
	 * @brief 获取历史数据回放器对象
	 * 
	 * 返回历史数据回放器对象的引用
	 * 
	 * @return 历史数据回放器对象的引用
	 */
	inline HisDataReplayer&	replayer() { return _replayer; }

	/**
	 * @brief 是否异步模式
	 * 
	 * 返回当前是否处于异步回测模式
	 * 
	 * @return true表示异步模式，false表示同步模式
	 */
	inline bool	isAsync() const { return _async; }

public:
	/**
	 * @brief 引擎初始化事件处理
	 * 
	 * 当回测引擎初始化完成时调用，触发引擎初始化事件回调
	 */
	inline void on_initialize_event()
	{
		if (_cb_evt)  // 如果事件回调函数已注册
			_cb_evt(EVENT_ENGINE_INIT, 0, 0);  // 触发引擎初始化事件回调
	}

	/**
	 * @brief 引擎调度事件处理
	 * 
	 * 当回测引擎按时间调度时调用，触发引擎调度事件回调
	 * 
	 * @param uDate 当前日期（格式：YYYYMMDD）
	 * @param uTime 当前时间（格式：HHMMSS）
	 */
	inline void on_schedule_event(uint32_t uDate, uint32_t uTime)
	{
		if (_cb_evt)  // 如果事件回调函数已注册
			_cb_evt(EVENT_ENGINE_SCHDL, uDate, uTime);  // 触发引擎调度事件回调
	}

	/**
	 * @brief 交易日事件处理
	 * 
	 * 当交易日开始或结束时调用，触发交易日事件回调
	 * 
	 * @param uDate 当前交易日（格式：YYYYMMDD）
	 * @param isBegin true表示交易日开始，false表示交易日结束（默认true）
	 */
	inline void on_session_event(uint32_t uDate, bool isBegin = true)
	{
		if (_cb_evt)  // 如果事件回调函数已注册
		{
			_cb_evt(isBegin ? EVENT_SESSION_BEGIN : EVENT_SESSION_END, uDate, 0);  // 触发交易日开始或结束事件回调
		}
	}

	/**
	 * @brief 回测结束事件处理
	 * 
	 * 当回测完成时调用，触发回测结束事件回调
	 */
	inline void on_backtest_end()
	{
		if (_cb_evt)  // 如果事件回调函数已注册
			_cb_evt(EVENT_BACKTEST_END, 0, 0);  // 触发回测结束事件回调
	}

private:
	/**
	 * @brief CTA策略回调函数成员变量
	 * 
	 * 存储CTA策略的各种事件回调函数指针
	 */
	FuncStraInitCallback	_cb_cta_init;  // CTA策略初始化回调函数
	FuncSessionEvtCallback	_cb_cta_sessevt;  // CTA策略交易日事件回调函数
	FuncStraTickCallback	_cb_cta_tick;  // CTA策略Tick更新回调函数
	FuncStraCalcCallback	_cb_cta_calc;  // CTA策略计算回调函数
	FuncStraCalcCallback	_cb_cta_calc_done;  // CTA策略计算完成回调函数
	FuncStraBarCallback		_cb_cta_bar;  // CTA策略K线闭合回调函数
	FuncStraCondTriggerCallback _cb_cta_cond_trigger;  // CTA策略条件单触发回调函数

	/**
	 * @brief SEL策略回调函数成员变量
	 * 
	 * 存储SEL策略的各种事件回调函数指针
	 */
	FuncStraInitCallback	_cb_sel_init;  // SEL策略初始化回调函数
	FuncSessionEvtCallback	_cb_sel_sessevt;  // SEL策略交易日事件回调函数
	FuncStraTickCallback	_cb_sel_tick;  // SEL策略Tick更新回调函数
	FuncStraCalcCallback	_cb_sel_calc;  // SEL策略计算回调函数
	FuncStraCalcCallback	_cb_sel_calc_done;  // SEL策略计算完成回调函数
	FuncStraBarCallback		_cb_sel_bar;  // SEL策略K线闭合回调函数

	/**
	 * @brief HFT策略回调函数成员变量
	 * 
	 * 存储HFT策略的各种事件回调函数指针
	 */
	FuncStraInitCallback	_cb_hft_init;  // HFT策略初始化回调函数
	FuncSessionEvtCallback	_cb_hft_sessevt;  // HFT策略交易日事件回调函数
	FuncStraTickCallback	_cb_hft_tick;  // HFT策略Tick更新回调函数
	FuncStraBarCallback		_cb_hft_bar;  // HFT策略K线闭合回调函数
	FuncHftChannelCallback	_cb_hft_chnl;  // HFT策略交易通道事件回调函数
	FuncHftOrdCallback		_cb_hft_ord;  // HFT策略订单状态变化回调函数
	FuncHftTrdCallback		_cb_hft_trd;  // HFT策略成交回报回调函数
	FuncHftEntrustCallback	_cb_hft_entrust;  // HFT策略委托回报回调函数

	FuncStraOrdQueCallback	_cb_hft_ordque;  // HFT策略订单队列更新回调函数
	FuncStraOrdDtlCallback	_cb_hft_orddtl;  // HFT策略订单明细更新回调函数
	FuncStraTransCallback	_cb_hft_trans;  // HFT策略逐笔成交更新回调函数

	FuncEventCallback		_cb_evt;  // 引擎事件回调函数

	/**
	 * @brief 外部数据加载器回调函数成员变量
	 * 
	 * 存储外部数据加载器的回调函数指针
	 */
	FuncLoadFnlBars			_ext_fnl_bar_loader;  // 最终K线加载器回调函数（已复权）
	FuncLoadRawBars			_ext_raw_bar_loader;  // 原始K线加载器回调函数（未复权）
	FuncLoadAdjFactors		_ext_adj_fct_loader;  // 复权因子加载器回调函数
	FuncLoadRawTicks		_ext_tick_loader;  // Tick加载器回调函数
	bool					_loader_auto_trans;  // 是否自动转储数据到本地缓存

	/**
	 * @brief 策略模拟器对象指针
	 * 
	 * 存储各种策略类型的模拟器对象指针
	 */
	CtaMocker*		_cta_mocker;  // CTA策略模拟器对象指针
	SelMocker*		_sel_mocker;  // SEL策略模拟器对象指针
	ExecMocker*		_exec_mocker;  // 执行器模拟器对象指针
	HftMocker*		_hft_mocker;  // HFT策略模拟器对象指针

	/**
	 * @brief 核心组件对象
	 * 
	 * 存储回测引擎的核心组件对象
	 */
	HisDataReplayer	_replayer;  // 历史数据回放器对象（负责回放历史数据，驱动策略执行）
	EventNotifier	_notifier;  // 事件通知器对象（负责管理事件通知）

	/**
	 * @brief 状态标志
	 * 
	 * 存储回测引擎的运行状态标志
	 */
	bool			_inited;  // 是否已初始化
	bool			_running;  // 是否正在运行

	/**
	 * @brief 异步回测相关
	 * 
	 * 存储异步回测模式相关的成员变量
	 */
	StdThreadPtr	_worker;  // 工作线程指针（异步模式下使用）
	bool			_async;  // 是否异步模式

	/**
	 * @brief 数据推送相关
	 * 
	 * 存储外部数据推送相关的成员变量
	 */
	void*			_feed_obj;  // 数据推送用户对象指针
	FuncReadBars	_feeder_bars;  // K线数据推送回调函数
	FuncReadTicks	_feeder_ticks;  // Tick数据推送回调函数
	FuncReadFactors	_feeder_fcts;  // 复权因子推送回调函数
	StdUniqueMutex	_feed_mtx;  // 数据推送互斥锁（保证线程安全）

	WTSVariant* _cfg;  // 配置对象指针（存储回测引擎的配置信息）
};

