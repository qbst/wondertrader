/*!
 * \file WtBtPorter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtBtPorter回测模块C接口头文件
 * 
 * 本文件声明了WtBtPorter回测模块的所有C风格导出函数，为外部语言（如Python、C#等）
 * 提供统一的C接口，实现与WonderTrader回测引擎的交互。
 * 
 * 主要功能模块：
 * 1. 回调函数注册：注册策略事件回调、数据加载回调等
 * 2. 回测引擎管理：初始化、配置、运行、停止回测引擎
 * 3. 策略模拟器管理：创建和管理CTA、HFT、SEL策略模拟器
 * 4. 数据推送：推送K线、Tick、复权因子等数据到回测引擎
 * 5. CTA策略接口：提供CTA策略的交易、查询、订阅等功能
 * 6. SEL策略接口：提供选股策略的交易、查询、订阅等功能
 * 7. HFT策略接口：提供高频交易策略的交易、查询、订阅等功能
 * 
 * 设计逻辑：
 * - 使用extern "C"确保C++函数可以被C语言调用
 * - 所有导出函数使用EXPORT_FLAG宏，确保跨平台兼容性
 * - 通过上下文句柄（CtxHandler）管理策略实例，隐藏C++对象指针
 * - 采用回调函数机制实现事件驱动的编程模型
 * - 支持外部数据加载器，允许从自定义数据源加载历史数据
 */
#pragma once
#include "PorterDefs.h"  // Porter模块定义文件


#ifdef __cplusplus
extern "C"  // C++代码中使用C链接规范，确保可以被C语言调用
{
#endif
	/**
	 * @brief 注册引擎事件回调函数
	 * 
	 * 注册回测引擎的事件回调函数，用于接收引擎初始化、交易日开始/结束、回测结束等事件
	 * 
	 * @param cbEvt 事件回调函数指针
	 */
	EXPORT_FLAG	void		register_evt_callback(FuncEventCallback cbEvt);

	/**
	 * @brief 注册CTA策略回调函数
	 * 
	 * 注册CTA策略的各种事件回调函数，包括初始化、Tick更新、K线闭合、交易日事件等
	 * 
	 * @param cbInit 策略初始化回调函数
	 * @param cbTick Tick更新回调函数
	 * @param cbCalc 策略计算回调函数（K线闭合或定时触发）
	 * @param cbBar K线闭合回调函数
	 * @param cbSessEvt 交易日事件回调函数
	 * @param cbCalcDone 策略计算完成回调函数（计算逻辑执行完毕后触发）
	 * @param cbCondTrigger 条件单触发回调函数（可选，默认为NULL）
	 */
	EXPORT_FLAG	void		register_cta_callbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
		FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone, FuncStraCondTriggerCallback cbCondTrigger = NULL);

	/**
	 * @brief 注册SEL策略回调函数
	 * 
	 * 注册选股策略的各种事件回调函数，包括初始化、Tick更新、K线闭合、交易日事件等
	 * 
	 * @param cbInit 策略初始化回调函数
	 * @param cbTick Tick更新回调函数
	 * @param cbCalc 策略计算回调函数
	 * @param cbBar K线闭合回调函数
	 * @param cbSessEvt 交易日事件回调函数
	 * @param cbCalcDone 策略计算完成回调函数
	 */
	EXPORT_FLAG	void		register_sel_callbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
		FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone);

	/**
	 * @brief 注册HFT策略回调函数
	 * 
	 * 注册高频交易策略的各种事件回调函数，包括初始化、Tick更新、K线闭合、交易通道事件、
	 * 订单状态变化、成交回报、委托回报、Level2行情等
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
	EXPORT_FLAG	void		register_hft_callbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar,
		FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust,
		FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt);

	/**
	 * @brief 注册外部数据加载器
	 * 
	 * 注册外部数据加载器的回调函数，用于从自定义数据源加载历史数据
	 * 
	 * @param fnlBarLoader 加载最终K线数据的回调函数（已复权）
	 * @param rawBarLoader 加载原始K线数据的回调函数（未复权）
	 * @param fctLoader 加载复权因子的回调函数
	 * @param tickLoader 加载原始Tick数据的回调函数
	 * @param bAutoTrans true表示自动转储数据到本地缓存，false表示不转储
	 */
	EXPORT_FLAG void		register_ext_data_loader(FuncLoadFnlBars fnlBarLoader, FuncLoadRawBars rawBarLoader, FuncLoadAdjFactors fctLoader, FuncLoadRawTicks tickLoader, bool bAutoTrans);

	/**
	 * @brief 推送原始K线数据
	 * 
	 * 将外部数据源的原始K线数据推送到回测引擎中
	 * 
	 * @param bars K线数据数组指针
	 * @param count K线数据条数
	 */
	EXPORT_FLAG void		feed_raw_bars(WTSBarStruct* bars, WtUInt32 count);

	/**
	 * @brief 推送原始Tick数据
	 * 
	 * 将外部数据源的原始Tick数据推送到回测引擎中
	 * 
	 * @param ticks Tick数据数组指针
	 * @param count Tick数据条数
	 */
	EXPORT_FLAG void		feed_raw_ticks(WTSTickStruct* ticks, WtUInt32 count);

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
	EXPORT_FLAG void		feed_adj_factors(WtString stdCode, WtUInt32* dates, double* factors, WtUInt32 count);

	/**
	 * @brief 初始化回测引擎
	 * 
	 * 初始化回测引擎，设置日志配置和输出目录
	 * 
	 * @param logProfile 日志配置文件路径或配置内容
	 * @param isFile true表示logProfile是文件路径，false表示logProfile是配置内容
	 * @param outDir 回测结果输出目录
	 */
	EXPORT_FLAG	void		init_backtest(const char* logProfile, bool isFile, const char* outDir);

	/**
	 * @brief 配置回测引擎
	 * 
	 * 加载并应用配置文件，配置回测引擎的各种参数
	 * 
	 * @param cfgfile 配置文件路径或配置内容（JSON格式）
	 * @param isFile true表示cfgfile是文件路径，false表示cfgfile是配置内容
	 */
	EXPORT_FLAG	void		config_backtest(const char* cfgfile, bool isFile);

	/**
	 * @brief 设置回测时间范围
	 * 
	 * 设置回测的开始时间和结束时间
	 * 
	 * @param stime 开始时间戳（格式：YYYYMMDDHHMMSS）
	 * @param etime 结束时间戳（格式：YYYYMMDDHHMMSS）
	 */
	EXPORT_FLAG	void		set_time_range(WtUInt64 stime, WtUInt64 etime);

	/**
	 * @brief 启用/禁用Tick回放
	 * 
	 * 控制回测引擎是否回放Tick数据（如果禁用，则只回放K线数据）
	 * 
	 * @param bEnabled true表示启用Tick回放，false表示禁用Tick回放（默认启用）
	 */
	EXPORT_FLAG	void		enable_tick(bool bEnabled = true);

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
	 * @param bRatioSlp true表示使用比例滑点（滑点=价格*比例），false表示使用固定滑点（默认false）
	 * @return 策略上下文句柄
	 */
	EXPORT_FLAG	CtxHandler	init_cta_mocker(const char* name, int slippage = 0, bool hook = false, bool persistData = true, bool bIncremental = false, bool bRatioSlp = false);

	/**
	 * @brief 初始化HFT策略模拟器
	 * 
	 * 创建一个新的HFT策略模拟器实例
	 * 
	 * @param name 策略名称
	 * @param hook true表示启用钩子模式（用于调试），false表示正常模式（默认false）
	 * @return 策略上下文句柄
	 */
	EXPORT_FLAG	CtxHandler	init_hft_mocker(const char* name, bool hook = false);

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
	 * @param bRatioSlp true表示使用比例滑点，false表示使用固定滑点（默认false）
	 * @return 策略上下文句柄
	 */
	EXPORT_FLAG	CtxHandler	init_sel_mocker(const char* name, WtUInt32 date, WtUInt32 time, const char* period, const char* trdtpl = "CHINA", const char* session = "TRADING", int slippage = 0, bool bRatioSlp = false);

	/**
	 * @brief 运行回测
	 * 
	 * 启动回测引擎，开始执行回测
	 * 
	 * @param bNeedDump true表示需要输出回测结果到文件，false表示不输出（默认false）
	 * @param bAsync true表示异步运行（函数立即返回），false表示同步运行（函数阻塞直到回测完成）
	 */
	EXPORT_FLAG	void		run_backtest(bool bNeedDump, bool bAsync);

	/**
	 * @brief 写入日志
	 * 
	 * 向日志系统写入一条日志记录
	 * 
	 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
	 * @param message 日志消息内容
	 * @param catName 日志分类名称（可选，为空则使用默认分类）
	 */
	EXPORT_FLAG	void		write_log(WtUInt32 level, const char* message, const char* catName);

	/**
	 * @brief 获取版本信息
	 * 
	 * 获取WonderTrader框架的版本信息字符串
	 * 
	 * @return 版本信息字符串
	 */
	EXPORT_FLAG	WtString	get_version();

	/**
	 * @brief 释放回测引擎
	 * 
	 * 清理资源，释放回测引擎占用的资源
	 */
	EXPORT_FLAG	void		release_backtest();

	/**
	 * @brief 清空缓存
	 * 
	 * 清空回测引擎的数据缓存
	 */
	EXPORT_FLAG	void		clear_cache();

	/**
	 * @brief 停止回测
	 * 
	 * 停止正在运行的回测（异步回测模式下使用）
	 */
	EXPORT_FLAG	void		stop_backtest();

	/**
	 * @brief 获取原始标准代码
	 * 
	 * 将标准合约代码转换为原始合约代码（去除复权、主力等后缀）
	 * 
	 * @param stdCode 标准合约代码（如"SHFE.rb2305.HOT"）
	 * @return 原始合约代码（如"SHFE.rb2305"）
	 */
	EXPORT_FLAG	WtString	get_raw_stdcode(const char* stdCode);


	//////////////////////////////////////////////////////////////////////////
	//CTA策略接口
	/**
	 * @brief CTA策略接口区域
	 * 
	 * 提供CTA（Commodity Trading Advisor）商品交易顾问策略的完整接口，
	 * 包括交易操作、持仓查询、数据获取、订阅等功能
	 */
#pragma region "CTA接口"
	/**
	 * @brief 开多仓
	 * 
	 * 执行开多仓操作，买入指定数量的合约
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签（用于标识该笔交易）
	 * @param limitprice 限价价格（0表示市价）
	 * @param stopprice 止损价格（0表示不设置止损）
	 */
	EXPORT_FLAG	void		cta_enter_long(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice);

	/**
	 * @brief 平多仓
	 * 
	 * 执行平多仓操作，卖出指定数量的多头持仓
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价价格（0表示市价）
	 * @param stopprice 止损价格（0表示不设置止损）
	 */
	EXPORT_FLAG	void		cta_exit_long(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice);

	/**
	 * @brief 开空仓
	 * 
	 * 执行开空仓操作，卖出指定数量的合约
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价价格（0表示市价）
	 * @param stopprice 止损价格（0表示不设置止损）
	 */
	EXPORT_FLAG	void		cta_enter_short(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice);

	/**
	 * @brief 平空仓
	 * 
	 * 执行平空仓操作，买入指定数量的空头持仓
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价价格（0表示市价）
	 * @param stopprice 止损价格（0表示不设置止损）
	 */
	EXPORT_FLAG	void		cta_exit_short(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice);

	/**
	 * @brief 获取持仓盈亏
	 * 
	 * 获取指定合约的持仓浮动盈亏
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 持仓盈亏金额（正数表示盈利，负数表示亏损）
	 */
	EXPORT_FLAG	double		cta_get_position_profit(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取明细持仓的入场时间
	 * 
	 * 获取指定合约和开仓标签对应的持仓明细的入场时间
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param openTag 开仓标签（用于区分不同的开仓批次）
	 * @return 入场时间戳（格式：YYYYMMDDHHMMSS）
	 */
	EXPORT_FLAG	WtUInt64	cta_get_detail_entertime(CtxHandler cHandle, const char* stdCode, const char* openTag);

	/**
	 * @brief 获取明细持仓的成本价
	 * 
	 * 获取指定合约和开仓标签对应的持仓明细的成本价
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param openTag 开仓标签
	 * @return 成本价
	 */
	EXPORT_FLAG	double		cta_get_detail_cost(CtxHandler cHandle, const char* stdCode, const char* openTag);

	/**
	 * @brief 获取明细持仓的盈亏
	 * 
	 * 获取指定合约和开仓标签对应的持仓明细的盈亏
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param openTag 开仓标签
	 * @param flag 盈亏类型标志（0-浮动盈亏，1-平仓盈亏）
	 * @return 盈亏金额
	 */
	EXPORT_FLAG	double		cta_get_detail_profit(CtxHandler cHandle, const char* stdCode, const char* openTag, int flag);

	/**
	 * @brief 获取持仓均价
	 * 
	 * 获取指定合约的持仓平均价格
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 持仓均价
	 */
	EXPORT_FLAG	double		cta_get_position_avgpx(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取持仓数量
	 * 
	 * 获取指定合约的持仓数量
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param bOnlyValid true表示只返回有效持仓（已成交的），false表示返回所有持仓（包括未成交的）
	 * @param openTag 开仓标签（NULL表示所有持仓，否则只返回指定标签的持仓）
	 * @return 持仓数量（正数表示多头，负数表示空头）
	 */
	EXPORT_FLAG	double		cta_get_position(CtxHandler cHandle, const char* stdCode, bool bOnlyValid, const char* openTag);

	/**
	 * @brief 设置目标持仓
	 * 
	 * 设置指定合约的目标持仓数量，系统会自动计算需要开仓或平仓的数量并执行
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param qty 目标持仓数量（正数表示多头，负数表示空头，0表示平仓）
	 * @param uesrTag 用户标签
	 * @param limitprice 限价价格（0表示市价）
	 * @param stopprice 止损价格（0表示不设置止损）
	 */
	EXPORT_FLAG	void		cta_set_position(CtxHandler cHandle, const char* stdCode, double qty, const char* uesrTag, double limitprice, double stopprice);

	/**
	 * @brief 获取当前价格
	 * 
	 * 获取指定合约的最新价格
	 * 
	 * @param stdCode 标准合约代码
	 * @return 当前价格（如果没有行情数据则返回0）
	 */
	EXPORT_FLAG	double 		cta_get_price(const char* stdCode);

	/**
	 * @brief 获取日线价格数据
	 * 
	 * 获取指定合约的日线价格数据（开盘价、最高价、最低价、收盘价等）
	 * 
	 * @param stdCode 标准合约代码
	 * @param flag 价格类型标志（0-开盘价，1-最高价，2-最低价，3-收盘价）
	 * @return 价格值
	 */
	EXPORT_FLAG	double 		cta_get_day_price(const char* stdCode, int flag);

	/**
	 * @brief 获取资金数据
	 * 
	 * 获取策略的资金数据（总资产、可用资金、持仓盈亏等）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param flag 资金类型标志（0-总资产，1-可用资金，2-持仓盈亏等）
	 * @return 资金数值
	 */
	EXPORT_FLAG	double		cta_get_fund_data(CtxHandler cHandle, int flag);

	/**
	 * @brief 获取交易日
	 * 
	 * 获取当前交易日（格式：YYYYMMDD）
	 * 
	 * @return 交易日
	 */
	EXPORT_FLAG	WtUInt32 	cta_get_tdate();

	/**
	 * @brief 获取当前日期
	 * 
	 * 获取当前日期（格式：YYYYMMDD）
	 * 
	 * @return 当前日期
	 */
	EXPORT_FLAG	WtUInt32 	cta_get_date();

	/**
	 * @brief 获取当前时间
	 * 
	 * 获取当前时间（格式：HHMMSS）
	 * 
	 * @return 当前时间
	 */
	EXPORT_FLAG	WtUInt32 	cta_get_time();

	/**
	 * @brief 获取K线数据
	 * 
	 * 获取指定合约和周期的K线数据，通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param barCnt 需要获取的K线数量
	 * @param isMain true表示主K线（用于策略计算），false表示辅助K线（仅用于查询）
	 * @param cb 回调函数，用于接收K线数据
	 * @return 实际返回的K线数量
	 */
	EXPORT_FLAG	WtUInt32	cta_get_bars(CtxHandler cHandle, const char* stdCode, const char* period, WtUInt32 barCnt, bool isMain, FuncGetBarsCallback cb);

	/**
	 * @brief 获取Tick数据
	 * 
	 * 获取指定合约的Tick数据，通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param tickCnt 需要获取的Tick数量
	 * @param cb 回调函数，用于接收Tick数据
	 * @return 实际返回的Tick数量
	 */
	EXPORT_FLAG	WtUInt32	cta_get_ticks(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetTicksCallback cb);

	/**
	 * @brief 获取所有持仓
	 * 
	 * 枚举策略的所有持仓，通过回调函数返回每个持仓信息
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param cb 回调函数，用于接收持仓信息
	 */
	EXPORT_FLAG void		cta_get_all_position(CtxHandler cHandle, FuncGetPositionCallback cb);

	/**
	 * @brief 获取首次入场时间
	 * 
	 * 获取指定合约的首次入场时间
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 首次入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓）
	 */
	EXPORT_FLAG	WtUInt64	cta_get_first_entertime(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后入场时间
	 * 
	 * 获取指定合约的最后入场时间
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓）
	 */
	EXPORT_FLAG	WtUInt64	cta_get_last_entertime(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后出场时间
	 * 
	 * 获取指定合约的最后出场时间
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后出场时间戳（格式：YYYYMMDDHHMMSS，0表示从未出场）
	 */
	EXPORT_FLAG	WtUInt64	cta_get_last_exittime(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后入场价格
	 * 
	 * 获取指定合约的最后入场价格
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后入场价格（0表示无持仓）
	 */
	EXPORT_FLAG	double		cta_get_last_enterprice(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后入场标签
	 * 
	 * 获取指定合约的最后入场标签
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后入场标签字符串（空字符串表示无持仓）
	 */
	EXPORT_FLAG	WtString	cta_get_last_entertag(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 记录日志
	 * 
	 * 在策略上下文中记录一条日志
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
	 * @param message 日志消息内容
	 */
	EXPORT_FLAG	void		cta_log_text(CtxHandler cHandle, WtUInt32 level, const char* message);

	/**
	 * @brief 保存用户数据
	 * 
	 * 保存策略的用户自定义数据（持久化存储，程序重启后仍可读取）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param key 数据键名
	 * @param val 数据值（字符串格式）
	 */
	EXPORT_FLAG	void		cta_save_userdata(CtxHandler cHandle, const char* key, const char* val);

	/**
	 * @brief 加载用户数据
	 * 
	 * 加载策略的用户自定义数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param key 数据键名
	 * @param defVal 默认值（如果数据不存在则返回此值）
	 * @return 数据值字符串
	 */
	EXPORT_FLAG	WtString	cta_load_userdata(CtxHandler cHandle, const char* key, const char* defVal);

	/**
	 * @brief 订阅Tick行情
	 * 
	 * 订阅指定合约的Tick行情，订阅后会在on_tick回调中收到该合约的Tick数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 */
	EXPORT_FLAG	void		cta_sub_ticks(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 订阅K线事件
	 * 
	 * 订阅指定合约和周期的K线闭合事件，订阅后会在on_bar回调中收到该K线的闭合事件
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 */
	EXPORT_FLAG	void		cta_sub_bar_events(CtxHandler cHandle, const char* stdCode, const char* period);

	/**
	 * @brief 执行策略单步回测
	 * 
	 * 在回测模式下，手动触发策略的单步执行（用于调试或精确控制回测进度）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @return 是否执行成功
	 */
	EXPORT_FLAG	bool		cta_step(CtxHandler cHandle);

	/**
	 * @brief 设置图表K线
	 * 
	 * 为策略图表设置主K线，用于可视化展示
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 */
	EXPORT_FLAG void		cta_set_chart_kline(CtxHandler cHandle, const char* stdCode, const char* period);

	/**
	 * @brief 添加图表标记
	 * 
	 * 在策略图表上添加一个标记点（如买卖信号）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param price 标记点的价格位置
	 * @param icon 图标类型（如"buy"、"sell"等）
	 * @param tag 标记标签文本
	 */
	EXPORT_FLAG void		cta_add_chart_mark(CtxHandler cHandle, double price, const char* icon, const char* tag);

	/**
	 * @brief 注册指标
	 * 
	 * 在策略图表上注册一个自定义指标
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param idxName 指标名称（唯一标识）
	 * @param indexType 指标类型：0-主图指标（叠加在K线上），1-副图指标（独立显示）
	 */
	EXPORT_FLAG void		cta_register_index(CtxHandler cHandle, const char* idxName, WtUInt32 indexType);

	/**
	 * @brief 注册指标线
	 * 
	 * 为已注册的指标添加一条数据线
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param idxName 指标名称
	 * @param lineName 线条名称（唯一标识该线条）
	 * @param lineType 线条类型：0-曲线，其他值可扩展
	 * @return 是否注册成功
	 */
	EXPORT_FLAG bool		cta_register_index_line(CtxHandler cHandle, const char* idxName, const char* lineName, WtUInt32 lineType);

	/**
	 * @brief 添加指标基准线
	 * 
	 * 为指标添加一条基准线（如0轴、100轴等）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 基准线数值
	 * @return 是否添加成功
	 */
	EXPORT_FLAG bool		cta_add_index_baseline(CtxHandler cHandle, const char* idxName, const char* lineName, double val);

	/**
	 * @brief 设置指标值
	 * 
	 * 更新指标线的当前值（在K线闭合时调用）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 指标值
	 * @return 是否设置成功
	 */
	EXPORT_FLAG bool		cta_set_index_value(CtxHandler cHandle, const char* idxName, const char* lineName, double val);

#pragma endregion "CTA接口"

	//////////////////////////////////////////////////////////////////////////
	//选股策略接口
	/**
	 * @brief SEL策略接口区域
	 * 
	 * 提供SEL（Selection）选股策略的完整接口，
	 * 包括交易操作、持仓查询、数据获取、订阅等功能
	 */
#pragma  region "SEL接口"
	/**
	 * @brief 获取持仓数量
	 * 
	 * 获取指定合约的持仓数量
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param bOnlyValid true表示只返回有效持仓（已成交的），false表示返回所有持仓
	 * @param openTag 开仓标签（NULL表示所有持仓，否则只返回指定标签的持仓）
	 * @return 持仓数量（正数表示多头，负数表示空头）
	 */
	EXPORT_FLAG	double		sel_get_position(CtxHandler cHandle, const char* stdCode, bool bOnlyValid, const char* openTag);

	/**
	 * @brief 设置目标持仓
	 * 
	 * 设置指定合约的目标持仓数量，系统会自动计算需要开仓或平仓的数量并执行
	 * 注意：多因子引擎中，限价和止损价格都无效，系统会使用市价执行
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param qty 目标持仓数量（正数表示多头，负数表示空头，0表示平仓）
	 * @param uesrTag 用户标签
	 */
	EXPORT_FLAG	void		sel_set_position(CtxHandler cHandle, const char* stdCode, double qty, const char* uesrTag);

	/**
	 * @brief 获取当前价格
	 * 
	 * 获取指定合约的最新价格
	 * 
	 * @param stdCode 标准合约代码
	 * @return 当前价格（如果没有行情数据则返回0）
	 */
	EXPORT_FLAG	double 		sel_get_price(const char* stdCode);

	/**
	 * @brief 获取当前日期
	 * 
	 * 获取当前日期（格式：YYYYMMDD）
	 * 
	 * @return 当前日期
	 */
	EXPORT_FLAG	WtUInt32 	sel_get_date();

	/**
	 * @brief 获取当前时间
	 * 
	 * 获取当前时间（格式：HHMMSS）
	 * 
	 * @return 当前时间
	 */
	EXPORT_FLAG	WtUInt32 	sel_get_time();

	/**
	 * @brief 获取K线数据
	 * 
	 * 获取指定合约和周期的K线数据，通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param barCnt 需要获取的K线数量
	 * @param cb 回调函数，用于接收K线数据
	 * @return 实际返回的K线数量
	 */
	EXPORT_FLAG	WtUInt32	sel_get_bars(CtxHandler cHandle, const char* stdCode, const char* period, WtUInt32 barCnt,FuncGetBarsCallback cb);

	/**
	 * @brief 获取Tick数据
	 * 
	 * 获取指定合约的Tick数据，通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param tickCnt 需要获取的Tick数量
	 * @param cb 回调函数，用于接收Tick数据
	 * @return 实际返回的Tick数量
	 */
	EXPORT_FLAG	WtUInt32	sel_get_ticks(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt,FuncGetTicksCallback cb);

	/**
	 * @brief 获取所有持仓
	 * 
	 * 枚举策略的所有持仓，通过回调函数返回每个持仓信息
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param cb 回调函数，用于接收持仓信息
	 */
	EXPORT_FLAG void		sel_get_all_position(CtxHandler cHandle, FuncGetPositionCallback cb);

	/**
	 * @brief 记录日志
	 * 
	 * 在策略上下文中记录一条日志
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
	 * @param message 日志消息内容
	 */
	EXPORT_FLAG	void		sel_log_text(CtxHandler cHandle, WtUInt32 level, const char* message);

	/**
	 * @brief 保存用户数据
	 * 
	 * 保存策略的用户自定义数据（持久化存储，程序重启后仍可读取）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param key 数据键名
	 * @param val 数据值（字符串格式）
	 */
	EXPORT_FLAG	void		sel_save_userdata(CtxHandler cHandle, const char* key, const char* val);

	/**
	 * @brief 加载用户数据
	 * 
	 * 加载策略的用户自定义数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param key 数据键名
	 * @param defVal 默认值（如果数据不存在则返回此值）
	 * @return 数据值字符串
	 */
	EXPORT_FLAG	WtString	sel_load_userdata(CtxHandler cHandle, const char* key, const char* defVal);

	/**
	 * @brief 订阅Tick行情
	 * 
	 * 订阅指定合约的Tick行情，订阅后会在on_tick回调中收到该合约的Tick数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 */
	EXPORT_FLAG	void		sel_sub_ticks(CtxHandler cHandle, const char* stdCode);

	//By Wesley @ 2023.05.17
	//扩展SEL的接口，主要是和CTA接口做一个同步
	/**
	 * @brief 获取持仓盈亏
	 * 
	 * 获取指定合约的持仓浮动盈亏（扩展接口，与CTA接口同步）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 持仓盈亏金额（正数表示盈利，负数表示亏损）
	 */
	EXPORT_FLAG	double		sel_get_position_profit(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取明细持仓的入场时间
	 * 
	 * 获取指定合约和开仓标签对应的持仓明细的入场时间（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param openTag 开仓标签
	 * @return 入场时间戳（格式：YYYYMMDDHHMMSS）
	 */
	EXPORT_FLAG	WtUInt64	sel_get_detail_entertime(CtxHandler cHandle, const char* stdCode, const char* openTag);

	/**
	 * @brief 获取明细持仓的成本价
	 * 
	 * 获取指定合约和开仓标签对应的持仓明细的成本价（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param openTag 开仓标签
	 * @return 成本价
	 */
	EXPORT_FLAG	double		sel_get_detail_cost(CtxHandler cHandle, const char* stdCode, const char* openTag);

	/**
	 * @brief 获取明细持仓的盈亏
	 * 
	 * 获取指定合约和开仓标签对应的持仓明细的盈亏（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param openTag 开仓标签
	 * @param flag 盈亏类型标志（0-浮动盈亏，1-平仓盈亏）
	 * @return 盈亏金额
	 */
	EXPORT_FLAG	double		sel_get_detail_profit(CtxHandler cHandle, const char* stdCode, const char* openTag, int flag);

	/**
	 * @brief 获取持仓均价
	 * 
	 * 获取指定合约的持仓平均价格（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 持仓均价
	 */
	EXPORT_FLAG	double		sel_get_position_avgpx(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取日线价格数据
	 * 
	 * 获取指定合约的日线价格数据（扩展接口）
	 * 
	 * @param stdCode 标准合约代码
	 * @param flag 价格类型标志（0-开盘价，1-最高价，2-最低价，3-收盘价）
	 * @return 价格值
	 */
	EXPORT_FLAG	double 		sel_get_day_price(const char* stdCode, int flag);

	/**
	 * @brief 获取资金数据
	 * 
	 * 获取策略的资金数据（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param flag 资金类型标志（0-总资产，1-可用资金，2-持仓盈亏等）
	 * @return 资金数值
	 */
	EXPORT_FLAG	double		sel_get_fund_data(CtxHandler cHandle, int flag);

	/**
	 * @brief 获取交易日
	 * 
	 * 获取当前交易日（格式：YYYYMMDD）（扩展接口）
	 * 
	 * @return 交易日
	 */
	EXPORT_FLAG	WtUInt32 	sel_get_tdate();

	/**
	 * @brief 获取首次入场时间
	 * 
	 * 获取指定合约的首次入场时间（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 首次入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓）
	 */
	EXPORT_FLAG	WtUInt64	sel_get_first_entertime(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后入场时间
	 * 
	 * 获取指定合约的最后入场时间（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓）
	 */
	EXPORT_FLAG	WtUInt64	sel_get_last_entertime(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后出场时间
	 * 
	 * 获取指定合约的最后出场时间（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后出场时间戳（格式：YYYYMMDDHHMMSS，0表示从未出场）
	 */
	EXPORT_FLAG	WtUInt64	sel_get_last_exittime(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后入场价格
	 * 
	 * 获取指定合约的最后入场价格（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后入场价格（0表示无持仓）
	 */
	EXPORT_FLAG	double		sel_get_last_enterprice(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取最后入场标签
	 * 
	 * 获取指定合约的最后入场标签（扩展接口）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 最后入场标签字符串（空字符串表示无持仓）
	 */
	EXPORT_FLAG	WtString	sel_get_last_entertag(CtxHandler cHandle, const char* stdCode);

#pragma endregion "SEL接口"

	//////////////////////////////////////////////////////////////////////////
//HFT策略接口
	/**
	 * @brief HFT策略接口区域
	 * 
	 * 提供HFT（High Frequency Trading）高频交易策略的完整接口，
	 * 包括交易操作、持仓查询、数据获取、Level2行情订阅等功能
	 */
#pragma  region "HFT接口"

	/**
	 * @brief 获取持仓数量
	 * 
	 * 获取指定合约的持仓数量
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param bOnlyValid true表示只返回有效持仓（已成交的），false表示返回所有持仓（包括未成交的）
	 * @return 持仓数量（正数表示多头，负数表示空头）
	 */
	EXPORT_FLAG	double		hft_get_position(CtxHandler cHandle, const char* stdCode, bool bOnlyValid);

	/**
	 * @brief 获取持仓盈亏
	 * 
	 * 获取指定合约的持仓浮动盈亏
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 持仓盈亏金额（正数表示盈利，负数表示亏损）
	 */
	EXPORT_FLAG	double		hft_get_position_profit(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取持仓均价
	 * 
	 * 获取指定合约的持仓平均价格
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 持仓均价
	 */
	EXPORT_FLAG	double		hft_get_position_avgpx(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取未完成订单数量
	 * 
	 * 获取指定合约的未完成订单数量（包括未成交和部分成交的订单）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @return 未完成订单数量（正数表示买入未完成，负数表示卖出未完成）
	 */
	EXPORT_FLAG	double		hft_get_undone(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 获取当前价格
	 * 
	 * 获取指定合约的最新价格
	 * 
	 * @param stdCode 标准合约代码
	 * @return 当前价格（如果没有行情数据则返回0）
	 */
	EXPORT_FLAG	double 		hft_get_price(const char* stdCode);

	/**
	 * @brief 获取当前日期
	 * 
	 * 获取当前日期（格式：YYYYMMDD）
	 * 
	 * @return 当前日期
	 */
	EXPORT_FLAG	WtUInt32 	hft_get_date();

	/**
	 * @brief 获取当前时间
	 * 
	 * 获取当前时间（格式：HHMMSS，原始时间，包含毫秒信息）
	 * 
	 * @return 当前时间
	 */
	EXPORT_FLAG	WtUInt32 	hft_get_time();

	/**
	 * @brief 获取当前秒数
	 * 
	 * 获取当前时间的秒数部分（0-59）
	 * 
	 * @return 当前秒数
	 */
	EXPORT_FLAG	WtUInt32 	hft_get_secs();

	/**
	 * @brief 获取K线数据
	 * 
	 * 获取指定合约和周期的K线数据，通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param period K线周期（如"m1"、"d1"等）
	 * @param barCnt 需要获取的K线数量
	 * @param cb 回调函数，用于接收K线数据
	 * @return 实际返回的K线数量
	 */
	EXPORT_FLAG	WtUInt32	hft_get_bars(CtxHandler cHandle, const char* stdCode, const char* period, WtUInt32 barCnt, FuncGetBarsCallback cb);

	/**
	 * @brief 获取Tick数据
	 * 
	 * 获取指定合约的Tick数据，通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param tickCnt 需要获取的Tick数量
	 * @param cb 回调函数，用于接收Tick数据
	 * @return 实际返回的Tick数量
	 */
	EXPORT_FLAG	WtUInt32	hft_get_ticks(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetTicksCallback cb);

	/**
	 * @brief 获取订单队列数据
	 * 
	 * 获取指定合约的订单队列数据（Level2行情），通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param tickCnt 需要获取的数据条数（参数名保持与历史版本兼容）
	 * @param cb 回调函数，用于接收订单队列数据
	 * @return 实际返回的数据条数
	 */
	EXPORT_FLAG	WtUInt32	hft_get_ordque(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetOrdQueCallback cb);

	/**
	 * @brief 获取订单明细数据
	 * 
	 * 获取指定合约的订单明细数据（Level2行情），通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param tickCnt 需要获取的数据条数（参数名保持与历史版本兼容）
	 * @param cb 回调函数，用于接收订单明细数据
	 * @return 实际返回的数据条数
	 */
	EXPORT_FLAG	WtUInt32	hft_get_orddtl(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetOrdDtlCallback cb);

	/**
	 * @brief 获取逐笔成交数据
	 * 
	 * 获取指定合约的逐笔成交数据（Level2行情），通过回调函数返回
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param tickCnt 需要获取的数据条数（参数名保持与历史版本兼容）
	 * @param cb 回调函数，用于接收逐笔成交数据
	 * @return 实际返回的数据条数
	 */
	EXPORT_FLAG	WtUInt32	hft_get_trans(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetTransCallback cb);

	/**
	 * @brief 记录日志
	 * 
	 * 在策略上下文中记录一条日志
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
	 * @param message 日志消息内容
	 */
	EXPORT_FLAG	void		hft_log_text(CtxHandler cHandle, WtUInt32 level, const char* message);

	/**
	 * @brief 订阅Tick行情
	 * 
	 * 订阅指定合约的Tick行情，订阅后会在on_tick回调中收到该合约的Tick数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 */
	EXPORT_FLAG	void		hft_sub_ticks(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 订阅订单队列
	 * 
	 * 订阅指定合约的订单队列数据，订阅后会在on_order_queue回调中收到该合约的订单队列数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 */
	EXPORT_FLAG	void		hft_sub_order_queue(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 订阅订单明细
	 * 
	 * 订阅指定合约的订单明细数据，订阅后会在on_order_detail回调中收到该合约的订单明细数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 */
	EXPORT_FLAG	void		hft_sub_order_detail(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 订阅逐笔成交
	 * 
	 * 订阅指定合约的逐笔成交数据，订阅后会在on_transaction回调中收到该合约的逐笔成交数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 */
	EXPORT_FLAG	void		hft_sub_transaction(CtxHandler cHandle, const char* stdCode);

	/**
	 * @brief 撤销订单
	 * 
	 * 撤销指定的订单
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param localid 本地订单ID（下单时返回的订单ID）
	 * @return 是否撤销成功
	 */
	EXPORT_FLAG	bool		hft_cancel(CtxHandler cHandle, WtUInt32 localid);

	/**
	 * @brief 撤销所有订单
	 * 
	 * 撤销指定合约和方向的所有未完成订单
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码（NULL或空字符串表示所有合约）
	 * @param isBuy true表示撤销买入订单，false表示撤销卖出订单
	 * @return 被撤销的订单ID列表（逗号分隔的字符串）
	 */
	EXPORT_FLAG	WtString	hft_cancel_all(CtxHandler cHandle, const char* stdCode, bool isBuy);

	/**
	 * @brief 买入
	 * 
	 * 执行买入操作，提交买入订单
	 * 如果订单被拆单，会返回多个订单ID（逗号分隔）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param price 买入价格（0表示市价）
	 * @param qty 买入数量
	 * @param userTag 用户标签（用于标识该笔交易）
	 * @param flag 订单标志（0-普通单，其他值可扩展）
	 * @return 订单ID列表（逗号分隔的字符串，如果拆单则返回多个订单ID）
	 */
	EXPORT_FLAG	WtString	hft_buy(CtxHandler cHandle, const char* stdCode, double price, double qty, const char* userTag, int flag);

	/**
	 * @brief 卖出
	 * 
	 * 执行卖出操作，提交卖出订单
	 * 如果订单被拆单，会返回多个订单ID（逗号分隔）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param stdCode 标准合约代码
	 * @param price 卖出价格（0表示市价）
	 * @param qty 卖出数量
	 * @param userTag 用户标签（用于标识该笔交易）
	 * @param flag 订单标志（0-普通单，其他值可扩展）
	 * @return 订单ID列表（逗号分隔的字符串，如果拆单则返回多个订单ID）
	 */
	EXPORT_FLAG	WtString	hft_sell(CtxHandler cHandle, const char* stdCode, double price, double qty, const char* userTag, int flag);

	/**
	 * @brief 保存用户数据
	 * 
	 * 保存策略的用户自定义数据（持久化存储，程序重启后仍可读取）
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param key 数据键名
	 * @param val 数据值（字符串格式）
	 */
	EXPORT_FLAG	void		hft_save_userdata(CtxHandler cHandle, const char* key, const char* val);

	/**
	 * @brief 加载用户数据
	 * 
	 * 加载策略的用户自定义数据
	 * 
	 * @param cHandle 策略上下文句柄
	 * @param key 数据键名
	 * @param defVal 默认值（如果数据不存在则返回此值）
	 * @return 数据值字符串
	 */
	EXPORT_FLAG	WtString	hft_load_userdata(CtxHandler cHandle, const char* key, const char* defVal);

	/**
	 * @brief 执行策略单步回测
	 * 
	 * 在回测模式下，手动触发策略的单步执行（用于调试或精确控制回测进度）
	 * 
	 * @param cHandle 策略上下文句柄
	 */
	EXPORT_FLAG	void		hft_step(CtxHandler cHandle);
#pragma endregion "HFT接口"
#ifdef __cplusplus
}
#endif