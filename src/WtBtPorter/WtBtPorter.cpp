/*!
 * \file WtBtPorter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtBtPorter回测模块C接口实现文件
 * 
 * 本文件实现了WtBtPorter回测模块的所有C接口函数，作为C++核心引擎与外部语言（如Python、C#等）
 * 之间的桥梁。所有C接口函数都通过委托模式，将调用转发给内部的WtBtRunner单例对象。
 * 
 * 主要功能模块：
 * 1. 回调函数注册：将外部语言注册的回调函数传递给WtBtRunner
 * 2. 回测引擎管理：初始化、配置、运行、停止回测引擎
 * 3. 策略模拟器管理：创建和管理CTA、HFT、SEL策略模拟器
 * 4. 数据推送：将外部数据推送到回测引擎
 * 5. CTA策略接口：提供CTA策略的完整交易和查询接口
 * 6. SEL策略接口：提供选股策略的完整交易和查询接口
 * 7. HFT策略接口：提供高频交易策略的完整交易和查询接口
 * 
 * 设计逻辑：
 * - 使用单例模式管理WtBtRunner对象，确保全局唯一
 * - 所有C接口函数都是薄包装层，直接委托给WtBtRunner
 * - 通过上下文句柄（CtxHandler）管理策略实例，隐藏C++对象指针
 * - 使用静态变量确保初始化和配置只执行一次
 */
#include "WtBtPorter.h"  // WtBtPorter C接口头文件
#include "WtBtRunner.h"  // 回测运行器类定义

#include "../WtBtCore/WtHelper.h"  // WonderTrader辅助函数
#include "../WtBtCore/CtaMocker.h"  // CTA策略模拟器类定义
#include "../WtBtCore/SelMocker.h"  // SEL策略模拟器类定义
#include "../WtBtCore/HftMocker.h"  // HFT策略模拟器类定义

#include "../WTSTools/WTSLogger.h"  // 日志系统

#include "../Includes/WTSVersion.h"  // 版本信息定义

/**
 * @brief 平台名称定义
 * 
 * 根据编译平台自动定义平台名称，用于版本信息显示
 */
#ifdef _WIN32  // Windows平台
#ifdef _WIN64  // 64位Windows
char PLATFORM_NAME[] = "X64";  // 64位平台名称
#else  // 32位Windows
char PLATFORM_NAME[] = "X86";  // 32位平台名称
#endif
#else  // Unix/Linux平台
char PLATFORM_NAME[] = "UNIX";  // Unix平台名称
#endif

/**
 * @brief 获取WtBtRunner单例对象
 * 
 * 返回WtBtRunner的单例对象引用，使用静态局部变量确保线程安全（C++11标准）
 * 
 * @return WtBtRunner对象的引用
 */
WtBtRunner& getRunner()
{
	static WtBtRunner runner;  // 静态局部变量，确保只创建一次（线程安全）
	return runner;  // 返回单例对象引用
}

/**
 * @brief 注册引擎事件回调函数
 * 
 * 将外部语言注册的事件回调函数传递给WtBtRunner
 * 
 * @param cbEvt 事件回调函数指针
 */
void register_evt_callback(FuncEventCallback cbEvt)
{
	getRunner().registerEvtCallback(cbEvt);  // 调用WtBtRunner的注册事件回调方法
}

/**
 * @brief 注册CTA策略回调函数
 * 
 * 将外部语言注册的CTA策略回调函数传递给WtBtRunner
 * 
 * @param cbInit 策略初始化回调函数
 * @param cbTick Tick更新回调函数
 * @param cbCalc 策略计算回调函数
 * @param cbBar K线闭合回调函数
 * @param cbSessEvt 交易日事件回调函数
 * @param cbCalcDone 策略计算完成回调函数（可选）
 * @param cbCondTrigger 条件单触发回调函数（可选）
 */
void register_cta_callbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
	FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone/* = NULL*/, FuncStraCondTriggerCallback cbCondTrigger)
{
	getRunner().registerCtaCallbacks(cbInit, cbTick, cbCalc, cbBar, cbSessEvt, cbCalcDone, cbCondTrigger);  // 调用WtBtRunner的注册CTA回调方法
}

/**
 * @brief 注册SEL策略回调函数
 * 
 * 将外部语言注册的SEL策略回调函数传递给WtBtRunner
 * 
 * @param cbInit 策略初始化回调函数
 * @param cbTick Tick更新回调函数
 * @param cbCalc 策略计算回调函数
 * @param cbBar K线闭合回调函数
 * @param cbSessEvt 交易日事件回调函数
 * @param cbCalcDone 策略计算完成回调函数（可选）
 */
void register_sel_callbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
	FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone/* = NULL*/)
{
	getRunner().registerSelCallbacks(cbInit, cbTick, cbCalc, cbBar, cbSessEvt, cbCalcDone);  // 调用WtBtRunner的注册SEL回调方法
}

/**
 * @brief 注册HFT策略回调函数
 * 
 * 将外部语言注册的HFT策略回调函数传递给WtBtRunner
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
void register_hft_callbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar,
	FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust,
	FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt)
{
	getRunner().registerHftCallbacks(cbInit, cbTick, cbBar, cbChnl, cbOrd, cbTrd, cbEntrust, cbOrdDtl, cbOrdQue, cbTrans, cbSessEvt);  // 调用WtBtRunner的注册HFT回调方法
}

/**
 * @brief 注册外部数据加载器
 * 
 * 将外部语言注册的数据加载器回调函数传递给WtBtRunner
 * 
 * @param fnlBarLoader 加载最终K线数据的回调函数（已复权）
 * @param rawBarLoader 加载原始K线数据的回调函数（未复权）
 * @param fctLoader 加载复权因子的回调函数
 * @param tickLoader 加载原始Tick数据的回调函数
 * @param bAutoTrans 是否自动转储数据到本地缓存
 */
void register_ext_data_loader(FuncLoadFnlBars fnlBarLoader, FuncLoadRawBars rawBarLoader, FuncLoadAdjFactors fctLoader, FuncLoadRawTicks tickLoader, bool bAutoTrans)
{
	getRunner().registerExtDataLoader(fnlBarLoader, rawBarLoader, fctLoader, tickLoader, bAutoTrans);  // 调用WtBtRunner的注册外部数据加载器方法
}

/**
 * @brief 推送原始K线数据
 * 
 * 将外部数据源的原始K线数据推送到WtBtRunner中
 * 
 * @param bars K线数据数组指针
 * @param count K线数据条数
 */
void feed_raw_bars(WTSBarStruct* bars, WtUInt32 count)
{
	getRunner().feedRawBars(bars, count);  // 调用WtBtRunner的推送原始K线数据方法
}

/**
 * @brief 推送原始Tick数据
 * 
 * 将外部数据源的原始Tick数据推送到WtBtRunner中
 * 
 * @param ticks Tick数据数组指针
 * @param count Tick数据条数
 */
void feed_raw_ticks(WTSTickStruct* ticks, WtUInt32 count)
{
	getRunner().feedRawTicks(ticks, count);  // 调用WtBtRunner的推送原始Tick数据方法
}

/**
 * @brief 初始化回测引擎
 * 
 * 初始化回测引擎，设置日志配置和输出目录
 * 使用静态变量确保只初始化一次
 * 
 * @param logProfile 日志配置文件路径或配置内容
 * @param isFile true表示logProfile是文件路径，false表示logProfile是配置内容
 * @param outDir 回测结果输出目录
 */
void init_backtest(const char* logProfile, bool isFile, const char* outDir)
{
	static bool inited = false;  // 静态变量，确保只初始化一次

	if (inited)  // 如果已经初始化过，直接返回
		return;

	getRunner().init(logProfile, isFile, outDir);  // 调用WtBtRunner的初始化方法

	inited = true;  // 标记为已初始化
}

/**
 * @brief 配置回测引擎
 * 
 * 加载并应用配置文件，配置回测引擎的各种参数
 * 使用静态变量确保只配置一次
 * 
 * @param cfgfile 配置文件路径或配置内容（JSON格式）
 * @param isFile true表示cfgfile是文件路径，false表示cfgfile是配置内容
 */
void config_backtest(const char* cfgfile, bool isFile)
{
	static bool inited = false;  // 静态变量，确保只配置一次

	if (inited)  // 如果已经配置过，直接返回
		return;

	if (strlen(cfgfile) == 0)  // 如果配置文件路径为空，使用默认配置文件
		getRunner().config("configbt.json", true);  // 使用默认配置文件"configbt.json"
	else
		getRunner().config(cfgfile, isFile);  // 调用WtBtRunner的配置方法
}

/**
 * @brief 设置回测时间范围
 * 
 * 设置回测的开始时间和结束时间
 * 
 * @param stime 开始时间戳（格式：YYYYMMDDHHMMSS）
 * @param etime 结束时间戳（格式：YYYYMMDDHHMMSS）
 */
void set_time_range(WtUInt64 stime, WtUInt64 etime)
{
	getRunner().set_time_range(stime, etime);  // 调用WtBtRunner的设置时间范围方法
}

/**
 * @brief 启用/禁用Tick回放
 * 
 * 控制回测引擎是否回放Tick数据（如果禁用，则只回放K线数据）
 * 
 * @param bEnabled true表示启用Tick回放，false表示禁用Tick回放（默认启用）
 */
void enable_tick(bool bEnabled /* = true */)
{
	getRunner().enable_tick(bEnabled);  // 调用WtBtRunner的启用/禁用Tick回放方法
}

/**
 * @brief 运行回测
 * 
 * 启动回测引擎，开始执行回测
 * 
 * @param bNeedDump true表示需要输出回测结果到文件，false表示不输出
 * @param bAsync true表示异步运行（函数立即返回），false表示同步运行（函数阻塞直到回测完成）
 */
void run_backtest(bool bNeedDump, bool bAsync)
{
	getRunner().run(bNeedDump, bAsync);  // 调用WtBtRunner的运行方法
}

/**
 * @brief 停止回测
 * 
 * 停止正在运行的回测（异步回测模式下使用）
 */
void stop_backtest()
{
	getRunner().stop();  // 调用WtBtRunner的停止方法
}

/**
 * @brief 释放回测引擎
 * 
 * 清理资源，释放回测引擎占用的资源
 */
void release_backtest()
{
	getRunner().release();  // 调用WtBtRunner的释放方法
}

/**
 * @brief 获取原始标准代码
 * 
 * 将标准合约代码转换为原始合约代码（去除复权、主力等后缀）
 * 
 * @param stdCode 标准合约代码（如"SHFE.rb2305.HOT"）
 * @return 原始合约代码（如"SHFE.rb2305"）
 */
WtString get_raw_stdcode(const char* stdCode)
{
	return getRunner().get_raw_stdcode(stdCode);  // 调用WtBtRunner的获取原始标准代码方法
}

/**
 * @brief 获取版本信息
 * 
 * 获取WonderTrader框架的版本信息字符串，包含平台、版本号、编译日期和时间
 * 使用静态变量缓存版本信息，避免重复构建字符串
 * 
 * @return 版本信息字符串（格式：平台 版本号 Build@编译日期 编译时间）
 */
const char* get_version()
{
	static std::string _ver;  // 静态变量，缓存版本信息字符串
	if(_ver.empty())  // 如果版本信息尚未构建，则构建一次
	{
		_ver = PLATFORM_NAME;  // 平台名称（X64/X86/UNIX）
		_ver += " ";
		_ver += WT_VERSION;  // 版本号
		_ver += " Build@";
		_ver += __DATE__;  // 编译日期（宏定义）
		_ver += " ";
		_ver += __TIME__;  // 编译时间（宏定义）
	}
	return _ver.c_str();  // 返回C风格字符串
}

/**
 * @brief 清空缓存
 * 
 * 清空回测引擎的数据缓存
 */
void clear_cache()
{
	getRunner().clear_cache();  // 调用WtBtRunner的清空缓存方法
}

/**
 * @brief 写入日志
 * 
 * 向日志系统写入一条日志记录
 * 
 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
 * @param message 日志消息内容
 * @param catName 日志分类名称（可选，为空则使用默认分类）
 */
void write_log(WtUInt32 level, const char* message, const char* catName)
{
	if (strlen(catName) > 0)  // 如果指定了分类名称，使用分类日志
	{
		WTSLogger::log_raw_by_cat(catName, (WTSLogLevel)level, message);  // 按分类记录日志
	}
	else  // 否则使用默认日志
	{
		WTSLogger::log_raw((WTSLogLevel)level, message);  // 记录默认日志
	}
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
 * @param bRatioSlp true表示使用比例滑点（滑点=价格*比例），false表示使用固定滑点（默认false）
 * @return 策略上下文句柄
 */
CtxHandler init_cta_mocker(const char* name, int slippage/* = 0*/, bool hook/* = false*/, bool persistData/* = true*/, bool bIncremental/* = false*/, bool bRatioSlp/* = false*/)
{
	return getRunner().initCtaMocker(name, slippage, hook, persistData, bIncremental, bRatioSlp);  // 调用WtBtRunner的初始化CTA模拟器方法
}

/**
 * @brief 初始化HFT策略模拟器
 * 
 * 创建一个新的HFT策略模拟器实例
 * 
 * @param name 策略名称
 * @param hook true表示启用钩子模式（用于调试），false表示正常模式（默认false）
 * @return 策略上下文句柄
 */
CtxHandler init_hft_mocker(const char* name, bool hook/* = false*/)
{
	return getRunner().initHftMocker(name, hook);  // 调用WtBtRunner的初始化HFT模拟器方法
}

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
CtxHandler init_sel_mocker(const char* name, WtUInt32 date, WtUInt32 time, const char* period, const char* trdtpl/* = "CHINA"*/, const char* session/* = "TRADING"*/, int slippage/* = 0*/, bool bRatioSlp/* = false*/)
{
	return getRunner().initSelMocker(name, date, time, period, trdtpl, session, slippage, bRatioSlp);  // 调用WtBtRunner的初始化SEL模拟器方法
}

#pragma region "CTA策略接口"
// CTA策略接口实现部分
// 所有CTA接口函数都通过getRunner().cta_mocker()获取CTA模拟器对象，然后调用其方法

/**
 * @brief 开多仓
 * 
 * 执行开多仓操作，买入指定数量的合约
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用，保留以保持接口一致性）
 * @param stdCode 标准合约代码
 * @param qty 开仓数量
 * @param userTag 用户标签（用于标识该笔交易）
 * @param limitprice 限价价格（0表示市价）
 * @param stopprice 止损价格（0表示不设置止损）
 */
void cta_enter_long(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_enter_long(stdCode, qty, userTag, limitprice, stopprice);  // 调用模拟器的开多仓方法
}

/**
 * @brief 平多仓
 * 
 * 执行平多仓操作，卖出指定数量的多头持仓
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param qty 平仓数量
 * @param userTag 用户标签
 * @param limitprice 限价价格（0表示市价）
 * @param stopprice 止损价格（0表示不设置止损）
 */
void cta_exit_long(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_exit_long(stdCode, qty, userTag, limitprice, stopprice);  // 调用模拟器的平多仓方法
}

/**
 * @brief 开空仓
 * 
 * 执行开空仓操作，卖出指定数量的合约
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param qty 开仓数量
 * @param userTag 用户标签
 * @param limitprice 限价价格（0表示市价）
 * @param stopprice 止损价格（0表示不设置止损）
 */
void cta_enter_short(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_enter_short(stdCode, qty, userTag, limitprice, stopprice);  // 调用模拟器的开空仓方法
}

/**
 * @brief 平空仓
 * 
 * 执行平空仓操作，买入指定数量的空头持仓
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param qty 平仓数量
 * @param userTag 用户标签
 * @param limitprice 限价价格（0表示市价）
 * @param stopprice 止损价格（0表示不设置止损）
 */
void cta_exit_short(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_exit_short(stdCode, qty, userTag, limitprice, stopprice);  // 调用模拟器的平空仓方法
}

/**
 * @brief 获取K线数据
 * 
 * 获取指定合约和周期的K线数据，通过回调函数返回
 * K线数据可能被分成多个数据块，每个数据块通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param barCnt 需要获取的K线数量
 * @param isMain true表示主K线（用于策略计算），false表示辅助K线（仅用于查询）
 * @param cb 回调函数，用于接收K线数据
 * @return 实际返回的K线数量（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 cta_get_bars(CtxHandler cHandle, const char* stdCode, const char* period, WtUInt32 barCnt, bool isMain, FuncGetBarsCallback cb)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSKlineSlice* kData = ctx->stra_get_bars(stdCode, period, barCnt, isMain);  // 从模拟器获取K线数据切片
		if (kData)  // 如果数据存在
		{
			WtUInt32 reaCnt = (WtUInt32)kData->size();  // 获取实际K线数量

			for (uint32_t i = 0; i < kData->get_block_counts(); i++)  // 遍历所有数据块
				cb(cHandle, stdCode, period, kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts()-1);  // 通过回调函数返回数据块

			kData->release();  // 释放K线数据切片资源
			return reaCnt;  // 返回实际K线数量
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch(...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 获取Tick数据
 * 
 * 获取指定合约的Tick数据，通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param tickCnt 需要获取的Tick数量
 * @param cb 回调函数，用于接收Tick数据
 * @return 实际返回的Tick数量（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32	cta_get_ticks(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetTicksCallback cb)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSTickSlice* tData = ctx->stra_get_ticks(stdCode, tickCnt);  // 从模拟器获取Tick数据切片
		if (tData)  // 如果数据存在
		{
			uint32_t thisCnt = min(tickCnt, (WtUInt32)tData->size());  // 计算实际返回的Tick数量（取请求数量和实际数量的较小值）
			if (thisCnt != 0)  // 如果Tick数量不为0
				cb(cHandle, stdCode, (WTSTickStruct*)tData->at(0), thisCnt, true);  // 通过回调函数返回Tick数据
			else  // 如果Tick数量为0
				cb(cHandle, stdCode, NULL, 0, true);  // 通过回调函数返回空数据（表示无数据）

			tData->release();  // 释放Tick数据切片资源
			return thisCnt;
		}
		else
		{
			return 0;
		}
	}
	catch (...)
	{
		return 0;
	}
}

/**
 * @brief 获取持仓盈亏
 * 
 * 获取指定合约的持仓浮动盈亏
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 持仓盈亏金额（正数表示盈利，负数表示亏损，如果模拟器不存在则返回0）
 */
double cta_get_position_profit(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_position_profit(stdCode);  // 调用模拟器的获取持仓盈亏方法
}

/**
 * @brief 获取明细持仓的入场时间
 * 
 * 获取指定合约和开仓标签对应的持仓明细的入场时间
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param openTag 开仓标签（用于区分不同的开仓批次）
 * @return 入场时间戳（格式：YYYYMMDDHHMMSS，如果模拟器不存在则返回0）
 */
WtUInt64 cta_get_detail_entertime(CtxHandler cHandle, const char* stdCode, const char* openTag)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_detail_entertime(stdCode, openTag);  // 调用模拟器的获取明细入场时间方法
}

/**
 * @brief 获取明细持仓的成本价
 * 
 * 获取指定合约和开仓标签对应的持仓明细的成本价
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param openTag 开仓标签
 * @return 成本价（如果模拟器不存在则返回0）
 */
double cta_get_detail_cost(CtxHandler cHandle, const char* stdCode, const char* openTag)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_detail_cost(stdCode, openTag);  // 调用模拟器的获取明细成本价方法
}

/**
 * @brief 获取明细持仓的盈亏
 * 
 * 获取指定合约和开仓标签对应的持仓明细的盈亏
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param openTag 开仓标签
 * @param flag 盈亏类型标志（0-浮动盈亏，1-平仓盈亏）
 * @return 盈亏金额（如果模拟器不存在则返回0）
 */
double cta_get_detail_profit(CtxHandler cHandle, const char* stdCode, const char* openTag, int flag)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_detail_profit(stdCode, openTag, flag);  // 调用模拟器的获取明细盈亏方法
}

/**
 * @brief 获取持仓均价
 * 
 * 获取指定合约的持仓平均价格
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 持仓均价（如果模拟器不存在则返回0）
 */
double cta_get_position_avgpx(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_position_avgpx(stdCode);  // 调用模拟器的获取持仓均价方法
}

/**
 * @brief 获取所有持仓
 * 
 * 枚举策略的所有持仓，通过回调函数返回每个持仓信息
 * 最后会调用一次回调函数，传入空字符串和isLast=true，表示枚举结束
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param cb 回调函数，用于接收持仓信息
 */
void cta_get_all_position(CtxHandler cHandle, FuncGetPositionCallback cb)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在
	{
		cb(cHandle, "", 0, true);  // 调用回调函数，传入空字符串表示无持仓，isLast=true表示结束
		return;
	}

	ctx->enum_position([cb, cHandle](const char* stdCode, double qty) {  // 使用lambda表达式枚举持仓
		cb(cHandle, stdCode, qty, false);  // 对每个持仓调用回调函数，isLast=false表示还有更多持仓
	}, false);  // false表示不包含未成交持仓

	cb(cHandle, "", 0, true);  // 最后调用一次回调函数，isLast=true表示枚举结束
}

/**
 * @brief 获取持仓数量
 * 
 * 获取指定合约的持仓数量
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param bOnlyValid true表示只返回有效持仓（已成交的），false表示返回所有持仓（包括未成交的）
 * @param openTag 开仓标签（NULL表示所有持仓，否则只返回指定标签的持仓）
 * @return 持仓数量（正数表示多头，负数表示空头，如果模拟器不存在则返回0）
 */
double cta_get_position(CtxHandler cHandle, const char* stdCode, bool bOnlyValid, const char* openTag)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_position(stdCode, bOnlyValid, openTag);  // 调用模拟器的获取持仓方法
}

/**
 * @brief 获取资金数据
 * 
 * 获取策略的资金数据（总资产、可用资金、持仓盈亏等）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param flag 资金类型标志（0-总资产，1-可用资金，2-持仓盈亏等）
 * @return 资金数值（如果模拟器不存在则返回0）
 */
double cta_get_fund_data(CtxHandler cHandle, int flag)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_fund_data(flag);  // 调用模拟器的获取资金数据方法
}

/**
 * @brief 设置目标持仓
 * 
 * 设置指定合约的目标持仓数量，系统会自动计算需要开仓或平仓的数量并执行
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param qty 目标持仓数量（正数表示多头，负数表示空头，0表示平仓）
 * @param userTag 用户标签
 * @param limitprice 限价价格（0表示市价）
 * @param stopprice 止损价格（0表示不设置止损）
 */
void cta_set_position(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag, double limitprice, double stopprice)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_set_position(stdCode, qty, userTag, limitprice, stopprice);  // 调用模拟器的设置目标持仓方法
}

/**
 * @brief 获取首次入场时间
 * 
 * 获取指定合约的首次入场时间
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 首次入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓，如果模拟器不存在则返回0）
 */
WtUInt64 cta_get_first_entertime(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_first_entertime(stdCode);  // 调用模拟器的获取首次入场时间方法
}

/**
 * @brief 获取最后入场时间
 * 
 * 获取指定合约的最后入场时间
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓，如果模拟器不存在则返回0）
 */
WtUInt64 cta_get_last_entertime(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_last_entertime(stdCode);  // 调用模拟器的获取最后入场时间方法
}

/**
 * @brief 获取最后出场时间
 * 
 * 获取指定合约的最后出场时间
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后出场时间戳（格式：YYYYMMDDHHMMSS，0表示从未出场，如果模拟器不存在则返回0）
 */
WtUInt64 cta_get_last_exittime(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_last_exittime(stdCode);  // 调用模拟器的获取最后出场时间方法
}

/**
 * @brief 获取最后入场价格
 * 
 * 获取指定合约的最后入场价格
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后入场价格（0表示无持仓，如果模拟器不存在则返回0）
 */
double cta_get_last_enterprice(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_last_enterprice(stdCode);  // 调用模拟器的获取最后入场价格方法
}

/**
 * @brief 获取最后入场标签
 * 
 * 获取指定合约的最后入场标签
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后入场标签字符串（空字符串表示无持仓，如果模拟器不存在则返回NULL）
 */
WtString cta_get_last_entertag(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回NULL
		return 0;

	return ctx->stra_get_last_entertag(stdCode);  // 调用模拟器的获取最后入场标签方法
}

/**
 * @brief 获取当前价格
 * 
 * 获取指定合约的最新价格（从回放器中获取）
 * 
 * @param stdCode 标准合约代码
 * @return 当前价格（如果没有行情数据则返回0）
 */
double cta_get_price(const char* stdCode)
{
	return getRunner().replayer().get_cur_price(stdCode);  // 通过回放器获取当前价格
}

/**
 * @brief 获取日线价格数据
 * 
 * 获取指定合约的日线价格数据（开盘价、最高价、最低价、收盘价等）
 * 
 * @param stdCode 标准合约代码
 * @param flag 价格类型标志（0-开盘价，1-最高价，2-最低价，3-收盘价）
 * @return 价格值
 */
double cta_get_day_price(const char* stdCode, int flag)
{
	return getRunner().replayer().get_day_price(stdCode, flag);  // 通过回放器获取日线价格
}

/**
 * @brief 获取交易日
 * 
 * 获取当前交易日（格式：YYYYMMDD）
 * 
 * @return 交易日
 */
WtUInt32 cta_get_tdate()
{
	return getRunner().replayer().get_trading_date();  // 通过回放器获取交易日
}

/**
 * @brief 获取当前日期
 * 
 * 获取当前日期（格式：YYYYMMDD）
 * 
 * @return 当前日期
 */
WtUInt32 cta_get_date()
{
	return getRunner().replayer().get_date();  // 通过回放器获取当前日期
}

/**
 * @brief 获取当前时间
 * 
 * 获取当前时间（格式：HHMMSS）
 * 
 * @return 当前时间
 */
WtUInt32 cta_get_time()
{
	return getRunner().replayer().get_min_time();  // 通过回放器获取当前时间（分钟级）
}

/**
 * @brief 记录日志
 * 
 * 在策略上下文中记录一条日志，根据日志级别调用不同的日志方法
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
 * @param message 日志消息内容
 */
void cta_log_text(CtxHandler cHandle, WtUInt32 level, const char* message)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	switch(level)  // 根据日志级别选择对应的日志方法
	{
	case LOG_LEVEL_DEBUG:  // 调试级别
		ctx->stra_log_debug(message);  // 记录调试日志
		break;
	case LOG_LEVEL_INFO:  // 信息级别
		ctx->stra_log_info(message);  // 记录信息日志
		break;
	case LOG_LEVEL_WARN:  // 警告级别
		ctx->stra_log_warn(message);  // 记录警告日志
		break;
	case LOG_LEVEL_ERROR:  // 错误级别
		ctx->stra_log_error(message);  // 记录错误日志
		break;
	default:  // 其他级别
		break;
	}
}

/**
 * @brief 保存用户数据
 * 
 * 保存策略的用户自定义数据（持久化存储，程序重启后仍可读取）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param key 数据键名
 * @param val 数据值（字符串格式）
 */
void cta_save_userdata(CtxHandler cHandle, const char* key, const char* val)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_save_user_data(key, val);  // 调用模拟器的保存用户数据方法
}

/**
 * @brief 加载用户数据
 * 
 * 加载策略的用户自定义数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param key 数据键名
 * @param defVal 默认值（如果数据不存在则返回此值）
 * @return 数据值字符串（如果模拟器不存在则返回默认值）
 */
WtString cta_load_userdata(CtxHandler cHandle, const char* key, const char* defVal)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，返回默认值
		return defVal;

	return ctx->stra_load_user_data(key, defVal);  // 调用模拟器的加载用户数据方法
}

/**
 * @brief 订阅Tick行情
 * 
 * 订阅指定合约的Tick行情，订阅后会在on_tick回调中收到该合约的Tick数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 */
void cta_sub_ticks(CtxHandler cHandle, const char* stdCode)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return ;

	ctx->stra_sub_ticks(stdCode);  // 调用模拟器的订阅Tick方法
}

/**
 * @brief 订阅K线事件
 * 
 * 订阅指定合约和周期的K线闭合事件，订阅后会在on_bar回调中收到该K线的闭合事件
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 */
void cta_sub_bar_events(CtxHandler cHandle, const char* stdCode, const char* period)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_sub_bar_events(stdCode, period);  // 调用模拟器的订阅K线事件方法
}

/**
 * @brief 执行策略单步回测
 * 
 * 在异步回测模式下，手动触发策略的单步执行（用于调试或精确控制回测进度）
 * 注意：只有异步模式才有意义，同步模式下直接返回false
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @return 是否执行成功（同步模式下返回false）
 */
bool cta_step(CtxHandler cHandle)
{
	//只有异步模式才有意义
	if (!getRunner().isAsync())  // 如果不是异步模式，直接返回false
		return false;

	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回false
		return false;

	return ctx->step_calc();  // 调用模拟器的单步计算方法
}

/**
 * @brief 设置图表K线
 * 
 * 为策略图表设置主K线，用于可视化展示
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 */
void cta_set_chart_kline(CtxHandler cHandle, const char* stdCode, const char* period)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->set_chart_kline(stdCode, period);  // 调用模拟器的设置图表K线方法
}

/**
 * @brief 添加图表标记
 * 
 * 在策略图表上添加一个标记点（如买卖信号）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param price 标记点的价格位置
 * @param icon 图标类型（如"buy"、"sell"等）
 * @param tag 标记标签文本
 */
void cta_add_chart_mark(CtxHandler cHandle, double price, const char* icon, const char* tag)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->add_chart_mark(price, icon, tag);  // 调用模拟器的添加图表标记方法
}

/**
 * @brief 注册指标
 * 
 * 在策略图表上注册一个自定义指标
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param idxName 指标名称（唯一标识）
 * @param indexType 指标类型：0-主图指标（叠加在K线上），1-副图指标（独立显示）
 */
void cta_register_index(CtxHandler cHandle, const char* idxName, WtUInt32 indexType)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->register_index(idxName, indexType);  // 调用模拟器的注册指标方法
}

/**
 * @brief 注册指标线
 * 
 * 为已注册的指标添加一条数据线
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param idxName 指标名称
 * @param lineName 线条名称（唯一标识该线条）
 * @param lineType 线条类型：0-曲线，其他值可扩展
 * @return 是否注册成功（如果模拟器不存在则返回false）
 */
bool cta_register_index_line(CtxHandler cHandle, const char* idxName, const char* lineName, WtUInt32 lineType)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回false
		return false;

	return ctx->register_index_line(idxName, lineName, lineType);  // 调用模拟器的注册指标线方法
}

/**
 * @brief 添加指标基准线
 * 
 * 为指标添加一条基准线（如0轴、100轴等）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param val 基准线数值
 * @return 是否添加成功（如果模拟器不存在则返回false）
 */
bool cta_add_index_baseline(CtxHandler cHandle, const char* idxName, const char* lineName, double val)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回false
		return false;

	return ctx->add_index_baseline(idxName, lineName, val);  // 调用模拟器的添加指标基准线方法
}

/**
 * @brief 设置指标值
 * 
 * 更新指标线的当前值（在K线闭合时调用）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param val 指标值
 * @return 是否设置成功（如果模拟器不存在则返回false）
 */
bool cta_set_index_value(CtxHandler cHandle, const char* idxName, const char* lineName, double val)
{
	CtaMocker* ctx = getRunner().cta_mocker();  // 获取CTA模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回false
		return false;

	return ctx->set_index_value(idxName, lineName, val);  // 调用模拟器的设置指标值方法
}

#pragma endregion "CTA策略接口"

#pragma region "SEL策略接口"
// SEL策略接口实现部分
// 所有SEL接口函数都通过getRunner().sel_mocker()获取SEL模拟器对象，然后调用其方法

/**
 * @brief 保存用户数据
 * 
 * 保存策略的用户自定义数据（持久化存储，程序重启后仍可读取）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param key 数据键名
 * @param val 数据值（字符串格式）
 */
void sel_save_userdata(CtxHandler cHandle, const char* key, const char* val)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_save_user_data(key, val);  // 调用模拟器的保存用户数据方法
}

/**
 * @brief 加载用户数据
 * 
 * 加载策略的用户自定义数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param key 数据键名
 * @param defVal 默认值（如果数据不存在则返回此值）
 * @return 数据值字符串（如果模拟器不存在则返回默认值）
 */
WtString sel_load_userdata(CtxHandler cHandle, const char* key, const char* defVal)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，返回默认值
		return defVal;

	return ctx->stra_load_user_data(key, defVal);  // 调用模拟器的加载用户数据方法
}

/**
 * @brief 记录日志
 * 
 * 在策略上下文中记录一条日志，根据日志级别调用不同的日志方法
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
 * @param message 日志消息内容
 */
void sel_log_text(CtxHandler cHandle, WtUInt32 level, const char* message)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	switch (level)  // 根据日志级别选择对应的日志方法
	{
	case LOG_LEVEL_DEBUG:  // 调试级别
		ctx->stra_log_debug(message);  // 记录调试日志
		break;
	case LOG_LEVEL_INFO:  // 信息级别
		ctx->stra_log_info(message);  // 记录信息日志
		break;
	case LOG_LEVEL_WARN:  // 警告级别
		ctx->stra_log_warn(message);  // 记录警告日志
		break;
	case LOG_LEVEL_ERROR:  // 错误级别
		ctx->stra_log_error(message);  // 记录错误日志
		break;
	default:  // 其他级别
		break;
	}
}

/**
 * @brief 获取当前价格
 * 
 * 获取指定合约的最新价格（从回放器中获取）
 * 
 * @param stdCode 标准合约代码
 * @return 当前价格（如果没有行情数据则返回0）
 */
double sel_get_price(const char* stdCode)
{
	return getRunner().replayer().get_cur_price(stdCode);  // 通过回放器获取当前价格
}

/**
 * @brief 获取当前日期
 * 
 * 获取当前日期（格式：YYYYMMDD）
 * 
 * @return 当前日期
 */
WtUInt32 sel_get_date()
{
	return getRunner().replayer().get_date();  // 通过回放器获取当前日期
}

/**
 * @brief 获取当前时间
 * 
 * 获取当前时间（格式：HHMMSS）
 * 
 * @return 当前时间
 */
WtUInt32 sel_get_time()
{
	return getRunner().replayer().get_min_time();  // 通过回放器获取当前时间（分钟级）
}

/**
 * @brief 获取所有持仓
 * 
 * 枚举策略的所有持仓，通过回调函数返回每个持仓信息
 * 最后会调用一次回调函数，传入空字符串和isLast=true，表示枚举结束
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param cb 回调函数，用于接收持仓信息
 */
void sel_get_all_position(CtxHandler cHandle, FuncGetPositionCallback cb)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在
	{
		cb(cHandle, "", 0, true);  // 调用回调函数，传入空字符串表示无持仓，isLast=true表示结束
		return;
	}

	ctx->enum_position([cb, cHandle](const char* stdCode, double qty) {  // 使用lambda表达式枚举持仓
		cb(cHandle, stdCode, qty, false);  // 对每个持仓调用回调函数，isLast=false表示还有更多持仓
	});

	cb(cHandle, "", 0, true);  // 最后调用一次回调函数，isLast=true表示枚举结束
}

/**
 * @brief 获取持仓数量
 * 
 * 获取指定合约的持仓数量
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param bOnlyValid true表示只返回有效持仓（已成交的），false表示返回所有持仓
 * @param openTag 开仓标签（NULL表示所有持仓，否则只返回指定标签的持仓）
 * @return 持仓数量（正数表示多头，负数表示空头，如果模拟器不存在则返回0）
 */
double sel_get_position(CtxHandler cHandle, const char* stdCode, bool bOnlyValid, const char* openTag)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_position(stdCode, bOnlyValid, openTag);  // 调用模拟器的获取持仓方法
}

/**
 * @brief 获取K线数据
 * 
 * 获取指定合约和周期的K线数据，通过回调函数返回
 * K线数据可能被分成多个数据块，每个数据块通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param barCnt 需要获取的K线数量
 * @param cb 回调函数，用于接收K线数据
 * @return 实际返回的K线数量（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 sel_get_bars(CtxHandler cHandle, const char* stdCode, const char* period, WtUInt32 barCnt, FuncGetBarsCallback cb)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSKlineSlice* kData = ctx->stra_get_bars(stdCode, period, barCnt);  // 从模拟器获取K线数据切片
		if (kData)  // 如果数据存在
		{
			WtUInt32 reaCnt = (WtUInt32)kData->size();  // 获取实际K线数量

			for (uint32_t i = 0; i < kData->get_block_counts(); i++)  // 遍历所有数据块
				cb(cHandle, stdCode, period, kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts() - 1);  // 通过回调函数返回数据块

			kData->release();  // 释放K线数据切片资源
			return reaCnt;  // 返回实际K线数量
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 设置目标持仓
 * 
 * 设置指定合约的目标持仓数量，系统会自动计算需要开仓或平仓的数量并执行
 * 注意：多因子引擎中，限价和止损价格都无效，系统会使用市价执行
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param qty 目标持仓数量（正数表示多头，负数表示空头，0表示平仓）
 * @param userTag 用户标签
 */
void sel_set_position(CtxHandler cHandle, const char* stdCode, double qty, const char* userTag)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	//多因子引擎,限价和止价都无效
	ctx->stra_set_position(stdCode, qty, userTag);  // 调用模拟器的设置目标持仓方法（多因子引擎不支持限价和止损）
}

/**
 * @brief 获取Tick数据
 * 
 * 获取指定合约的Tick数据，通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param tickCnt 需要获取的Tick数量
 * @param cb 回调函数，用于接收Tick数据
 * @return 实际返回的Tick数量（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32	sel_get_ticks(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetTicksCallback cb)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSTickSlice* tData = ctx->stra_get_ticks(stdCode, tickCnt);  // 从模拟器获取Tick数据切片
		if (tData)  // 如果数据存在
		{
			uint32_t thisCnt = min(tickCnt, (WtUInt32)tData->size());  // 计算实际返回的Tick数量（取请求数量和实际数量的较小值）
			if (thisCnt != 0)  // 如果Tick数量不为0
				cb(cHandle, stdCode, (WTSTickStruct*)tData->at(0), thisCnt, true);  // 通过回调函数返回Tick数据
			else  // 如果Tick数量为0
				cb(cHandle, stdCode, NULL, 0, true);  // 通过回调函数返回空数据（表示无数据）
			tData->release();  // 释放Tick数据切片资源
			return thisCnt;  // 返回实际Tick数量
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 订阅Tick行情
 * 
 * 订阅指定合约的Tick行情，订阅后会在on_tick回调中收到该合约的Tick数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 */
void sel_sub_ticks(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	ctx->stra_sub_ticks(stdCode);  // 调用模拟器的订阅Tick方法
}

/**
 * @brief 获取日线价格数据
 * 
 * 获取指定合约的日线价格数据（开盘价、最高价、最低价、收盘价等）
 * 
 * @param stdCode 标准合约代码
 * @param flag 价格类型标志（0-开盘价，1-最高价，2-最低价，3-收盘价）
 * @return 价格值
 */
double sel_get_day_price(const char* stdCode, int flag)
{
	return getRunner().replayer().get_day_price(stdCode, flag);  // 通过回放器获取日线价格
}

/**
 * @brief 获取交易日
 * 
 * 获取当前交易日（格式：YYYYMMDD）
 * 
 * @return 交易日
 */
WtUInt32 sel_get_tdate()
{
	return getRunner().replayer().get_trading_date();  // 通过回放器获取交易日
}

/**
 * @brief 获取资金数据
 * 
 * 获取策略的资金数据（总资产、可用资金、持仓盈亏等）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param flag 资金类型标志（0-总资产，1-可用资金，2-持仓盈亏等）
 * @return 资金数值（如果模拟器不存在则返回0）
 */
double sel_get_fund_data(CtxHandler cHandle, int flag)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_fund_data(flag);  // 调用模拟器的获取资金数据方法
}

/**
 * @brief 获取持仓盈亏
 * 
 * 获取指定合约的持仓浮动盈亏（扩展接口，与CTA接口同步）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 持仓盈亏金额（正数表示盈利，负数表示亏损，如果模拟器不存在则返回0）
 */
double sel_get_position_profit(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_position_profit(stdCode);  // 调用模拟器的获取持仓盈亏方法
}

/**
 * @brief 获取明细持仓的入场时间
 * 
 * 获取指定合约和开仓标签对应的持仓明细的入场时间（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param openTag 开仓标签
 * @return 入场时间戳（格式：YYYYMMDDHHMMSS，如果模拟器不存在则返回0）
 */
WtUInt64 sel_get_detail_entertime(CtxHandler cHandle, const char* stdCode, const char* openTag)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_detail_entertime(stdCode, openTag);  // 调用模拟器的获取明细入场时间方法
}

/**
 * @brief 获取明细持仓的成本价
 * 
 * 获取指定合约和开仓标签对应的持仓明细的成本价（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param openTag 开仓标签
 * @return 成本价（如果模拟器不存在则返回0）
 */
double sel_get_detail_cost(CtxHandler cHandle, const char* stdCode, const char* openTag)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_detail_cost(stdCode, openTag);  // 调用模拟器的获取明细成本价方法
}

/**
 * @brief 获取明细持仓的盈亏
 * 
 * 获取指定合约和开仓标签对应的持仓明细的盈亏（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param openTag 开仓标签
 * @param flag 盈亏类型标志（0-浮动盈亏，1-平仓盈亏）
 * @return 盈亏金额（如果模拟器不存在则返回0）
 */
double sel_get_detail_profit(CtxHandler cHandle, const char* stdCode, const char* openTag, int flag)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_detail_profit(stdCode, openTag, flag);  // 调用模拟器的获取明细盈亏方法
}

/**
 * @brief 获取持仓均价
 * 
 * 获取指定合约的持仓平均价格（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 持仓均价（如果模拟器不存在则返回0）
 */
double sel_get_position_avgpx(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_position_avgpx(stdCode);  // 调用模拟器的获取持仓均价方法
}

/**
 * @brief 获取首次入场时间
 * 
 * 获取指定合约的首次入场时间（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 首次入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓，如果模拟器不存在则返回0）
 */
WtUInt64 sel_get_first_entertime(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_first_entertime(stdCode);  // 调用模拟器的获取首次入场时间方法
}

/**
 * @brief 获取最后入场时间
 * 
 * 获取指定合约的最后入场时间（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后入场时间戳（格式：YYYYMMDDHHMMSS，0表示无持仓，如果模拟器不存在则返回0）
 */
WtUInt64 sel_get_last_entertime(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_last_entertime(stdCode);  // 调用模拟器的获取最后入场时间方法
}

/**
 * @brief 获取最后出场时间
 * 
 * 获取指定合约的最后出场时间（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后出场时间戳（格式：YYYYMMDDHHMMSS，0表示从未出场，如果模拟器不存在则返回0）
 */
WtUInt64 sel_get_last_exittime(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_last_exittime(stdCode);  // 调用模拟器的获取最后出场时间方法
}

/**
 * @brief 获取最后入场价格
 * 
 * 获取指定合约的最后入场价格（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后入场价格（0表示无持仓，如果模拟器不存在则返回0）
 */
double sel_get_last_enterprice(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return ctx->stra_get_last_enterprice(stdCode);  // 调用模拟器的获取最后入场价格方法
}

/**
 * @brief 获取最后入场标签
 * 
 * 获取指定合约的最后入场标签（扩展接口）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 最后入场标签字符串（空字符串表示无持仓，如果模拟器不存在则返回NULL）
 */
WtString sel_get_last_entertag(CtxHandler cHandle, const char* stdCode)
{
	SelMocker* ctx = getRunner().sel_mocker();  // 获取SEL模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回NULL
		return 0;

	return ctx->stra_get_last_entertag(stdCode);  // 调用模拟器的获取最后入场标签方法
}

#pragma endregion "SEL策略接口"

#pragma region "HFT策略接口"
// HFT策略接口实现部分
// 所有HFT接口函数都通过getRunner().hft_mocker()获取HFT模拟器对象，然后调用其方法

/**
 * @brief 获取持仓数量
 * 
 * 获取指定合约的持仓数量
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param bOnlyValid true表示只返回有效持仓（已成交的），false表示返回所有持仓（包括未成交的）
 * @return 持仓数量（正数表示多头，负数表示空头，如果模拟器不存在则返回0）
 */
double hft_get_position(CtxHandler cHandle, const char* stdCode, bool bOnlyValid)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return mocker->stra_get_position(stdCode, bOnlyValid);  // 调用模拟器的获取持仓方法
}

/**
 * @brief 获取持仓盈亏
 * 
 * 获取指定合约的持仓浮动盈亏
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 持仓盈亏金额（正数表示盈利，负数表示亏损，如果模拟器不存在则返回0）
 */
double hft_get_position_profit(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return mocker->stra_get_position_profit(stdCode);  // 调用模拟器的获取持仓盈亏方法
}

/**
 * @brief 获取持仓均价
 * 
 * 获取指定合约的持仓平均价格
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 持仓均价（如果模拟器不存在则返回0）
 */
double hft_get_position_avgpx(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return mocker->stra_get_position_avgpx(stdCode);  // 调用模拟器的获取持仓均价方法
}

/**
 * @brief 获取未完成订单数量
 * 
 * 获取指定合约的未完成订单数量（包括未成交和部分成交的订单）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @return 未完成订单数量（正数表示买入未完成，负数表示卖出未完成，如果模拟器不存在则返回0）
 */
double hft_get_undone(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	return mocker->stra_get_undone(stdCode);  // 调用模拟器的获取未完成订单数量方法
}

/**
 * @brief 获取当前价格
 * 
 * 获取指定合约的最新价格（从回放器中获取）
 * 
 * @param stdCode 标准合约代码
 * @return 当前价格（如果没有行情数据则返回0）
 */
double hft_get_price(const char* stdCode)
{
	return getRunner().replayer().get_cur_price(stdCode);  // 通过回放器获取当前价格
}

/**
 * @brief 获取当前日期
 * 
 * 获取当前日期（格式：YYYYMMDD）
 * 
 * @return 当前日期
 */
WtUInt32 hft_get_date()
{
	return getRunner().replayer().get_date();  // 通过回放器获取当前日期
}

/**
 * @brief 获取当前时间
 * 
 * 获取当前时间（格式：HHMMSS，原始时间，包含毫秒信息）
 * 
 * @return 当前时间
 */
WtUInt32 hft_get_time()
{
	return getRunner().replayer().get_raw_time();  // 通过回放器获取当前原始时间（包含毫秒）
}

/**
 * @brief 获取当前秒数
 * 
 * 获取当前时间的秒数部分（0-59）
 * 
 * @return 当前秒数
 */
WtUInt32 hft_get_secs()
{
	return getRunner().replayer().get_secs();  // 通过回放器获取当前秒数
}

/**
 * @brief 获取K线数据
 * 
 * 获取指定合约和周期的K线数据，通过回调函数返回
 * K线数据可能被分成多个数据块，每个数据块通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param barCnt 需要获取的K线数量
 * @param cb 回调函数，用于接收K线数据
 * @return 实际返回的K线数量（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 hft_get_bars(CtxHandler cHandle, const char* stdCode, const char* period, WtUInt32 barCnt, FuncGetBarsCallback cb)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;

	try  // 使用try-catch捕获可能的异常
	{
		WTSKlineSlice* kData = mocker->stra_get_bars(stdCode, period, barCnt);  // 从模拟器获取K线数据切片
		if (kData)  // 如果数据存在
		{
			WtUInt32 reaCnt = (WtUInt32)kData->size();  // 获取实际K线数量

			for (uint32_t i = 0; i < kData->get_block_counts(); i++)  // 遍历所有数据块
				cb(cHandle, stdCode, period, kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts() - 1);  // 通过回调函数返回数据块

			kData->release();  // 释放K线数据切片资源
			return reaCnt;  // 返回实际K线数量
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 获取Tick数据
 * 
 * 获取指定合约的Tick数据，通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param tickCnt 需要获取的Tick数量
 * @param cb 回调函数，用于接收Tick数据
 * @return 实际返回的Tick数量（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 hft_get_ticks(CtxHandler cHandle, const char* stdCode, WtUInt32 tickCnt, FuncGetTicksCallback cb)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSTickSlice* tData = mocker->stra_get_ticks(stdCode, tickCnt);  // 从模拟器获取Tick数据切片
		if (tData)  // 如果数据存在
		{
			uint32_t thisCnt = min(tickCnt, (WtUInt32)tData->size());  // 计算实际返回的Tick数量（取请求数量和实际数量的较小值）
			if(thisCnt != 0)  // 如果Tick数量不为0
				cb(cHandle, stdCode, (WTSTickStruct*)tData->at(0), thisCnt, true);  // 通过回调函数返回Tick数据
			else  // 如果Tick数量为0
				cb(cHandle, stdCode, NULL, 0, true);  // 通过回调函数返回空数据（表示无数据）
			tData->release();  // 释放Tick数据切片资源
			return thisCnt;  // 返回实际Tick数量
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 获取订单队列数据
 * 
 * 获取指定合约的订单队列数据（Level2行情），通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param itemCnt 需要获取的数据条数
 * @param cb 回调函数，用于接收订单队列数据
 * @return 实际返回的数据条数（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 hft_get_ordque(CtxHandler cHandle, const char* stdCode, WtUInt32 itemCnt, FuncGetOrdQueCallback cb)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSOrdQueSlice* dataSlice = mocker->stra_get_order_queue(stdCode, itemCnt);  // 从模拟器获取订单队列数据切片
		if (dataSlice)  // 如果数据存在
		{
			uint32_t thisCnt = min(itemCnt, (WtUInt32)dataSlice->size());  // 计算实际返回的数据条数（取请求数量和实际数量的较小值）
			cb(cHandle, stdCode, (WTSOrdQueStruct*)dataSlice->at(0), thisCnt, true);  // 通过回调函数返回订单队列数据
			dataSlice->release();  // 释放订单队列数据切片资源
			return thisCnt;  // 返回实际数据条数
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 获取订单明细数据
 * 
 * 获取指定合约的订单明细数据（Level2行情），通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param itemCnt 需要获取的数据条数
 * @param cb 回调函数，用于接收订单明细数据
 * @return 实际返回的数据条数（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 hft_get_orddtl(CtxHandler cHandle, const char* stdCode, WtUInt32 itemCnt, FuncGetOrdDtlCallback cb)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSOrdDtlSlice* dataSlice = mocker->stra_get_order_detail(stdCode, itemCnt);  // 从模拟器获取订单明细数据切片
		if (dataSlice)  // 如果数据存在
		{
			uint32_t thisCnt = min(itemCnt, (WtUInt32)dataSlice->size());  // 计算实际返回的数据条数（取请求数量和实际数量的较小值）
			cb(cHandle, stdCode, (WTSOrdDtlStruct*)dataSlice->at(0), thisCnt, true);  // 通过回调函数返回订单明细数据
			dataSlice->release();  // 释放订单明细数据切片资源
			return thisCnt;  // 返回实际数据条数
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 获取逐笔成交数据
 * 
 * 获取指定合约的逐笔成交数据（Level2行情），通过回调函数返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param itemCnt 需要获取的数据条数
 * @param cb 回调函数，用于接收逐笔成交数据
 * @return 实际返回的数据条数（如果模拟器不存在或发生异常则返回0）
 */
WtUInt32 hft_get_trans(CtxHandler cHandle, const char* stdCode, WtUInt32 itemCnt, FuncGetTransCallback cb)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回0
		return 0;
	try  // 使用try-catch捕获可能的异常
	{
		WTSTransSlice* dataSlice = mocker->stra_get_transaction(stdCode, itemCnt);  // 从模拟器获取逐笔成交数据切片
		if (dataSlice)  // 如果数据存在
		{
			uint32_t thisCnt = min(itemCnt, (WtUInt32)dataSlice->size());  // 计算实际返回的数据条数（取请求数量和实际数量的较小值）
			cb(cHandle, stdCode, (WTSTransStruct*)dataSlice->at(0), thisCnt, true);  // 通过回调函数返回逐笔成交数据
			dataSlice->release();  // 释放逐笔成交数据切片资源
			return thisCnt;  // 返回实际数据条数
		}
		else  // 如果数据不存在
		{
			return 0;  // 返回0
		}
	}
	catch (...)  // 捕获所有异常
	{
		return 0;  // 发生异常时返回0
	}
}

/**
 * @brief 记录日志
 * 
 * 在策略上下文中记录一条日志，根据日志级别调用不同的日志方法
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param level 日志级别（LOG_LEVEL_DEBUG、LOG_LEVEL_INFO、LOG_LEVEL_WARN、LOG_LEVEL_ERROR）
 * @param message 日志消息内容
 */
void hft_log_text(CtxHandler cHandle, WtUInt32 level, const char* message)
{
	HftMocker* ctx = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (ctx == NULL)  // 如果模拟器不存在，直接返回
		return;

	switch (level)  // 根据日志级别选择对应的日志方法
	{
	case LOG_LEVEL_DEBUG:  // 调试级别
		ctx->stra_log_debug(message);  // 记录调试日志
		break;
	case LOG_LEVEL_INFO:  // 信息级别
		ctx->stra_log_info(message);  // 记录信息日志
		break;
	case LOG_LEVEL_WARN:  // 警告级别
		ctx->stra_log_warn(message);  // 记录警告日志
		break;
	case LOG_LEVEL_ERROR:  // 错误级别
		ctx->stra_log_error(message);  // 记录错误日志
		break;
	default:  // 其他级别
		break;
	}
}

/**
 * @brief 订阅Tick行情
 * 
 * 订阅指定合约的Tick行情，订阅后会在on_tick回调中收到该合约的Tick数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 */
void hft_sub_ticks(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回
		return;

	mocker->stra_sub_ticks(stdCode);  // 调用模拟器的订阅Tick方法
}

/**
 * @brief 订阅订单明细
 * 
 * 订阅指定合约的订单明细数据，订阅后会在on_order_detail回调中收到该合约的订单明细数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 */
void hft_sub_order_detail(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回
		return;

	mocker->stra_sub_order_details(stdCode);  // 调用模拟器的订阅订单明细方法
}

/**
 * @brief 订阅订单队列
 * 
 * 订阅指定合约的订单队列数据，订阅后会在on_order_queue回调中收到该合约的订单队列数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 */
void hft_sub_order_queue(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回
		return;

	mocker->stra_sub_order_queues(stdCode);  // 调用模拟器的订阅订单队列方法
}

/**
 * @brief 订阅逐笔成交
 * 
 * 订阅指定合约的逐笔成交数据，订阅后会在on_transaction回调中收到该合约的逐笔成交数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 */
void hft_sub_transaction(CtxHandler cHandle, const char* stdCode)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回
		return;

	mocker->stra_sub_transactions(stdCode);  // 调用模拟器的订阅逐笔成交方法
}

/**
 * @brief 撤销订单
 * 
 * 撤销指定的订单
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param localid 本地订单ID（下单时返回的订单ID）
 * @return 是否撤销成功（如果模拟器不存在则返回false）
 */
bool hft_cancel(CtxHandler cHandle, WtUInt32 localid)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回false
		return false;

	return mocker->stra_cancel(localid);  // 调用模拟器的撤销订单方法
}

/**
 * @brief 撤销所有订单
 * 
 * 撤销指定合约和方向的所有未完成订单
 * 返回被撤销的订单ID列表（逗号分隔的字符串）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码（NULL或空字符串表示所有合约）
 * @param isBuy true表示撤销买入订单，false表示撤销卖出订单
 * @return 被撤销的订单ID列表（逗号分隔的字符串，如果模拟器不存在则返回空字符串）
 */
WtString hft_cancel_all(CtxHandler cHandle, const char* stdCode, bool isBuy)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，返回空字符串
		return "";

	static thread_local std::string ret;  // 线程局部静态变量，用于存储返回的订单ID列表字符串

	std::stringstream ss;  // 字符串流，用于构建订单ID列表
	OrderIDs ids = mocker->stra_cancel(stdCode, isBuy, DBL_MAX);  // 调用模拟器的撤销所有订单方法，DBL_MAX表示撤销所有价格
	for (WtUInt32 localid : ids)  // 遍历所有被撤销的订单ID
	{
		ss << localid << ",";  // 将订单ID添加到字符串流中，用逗号分隔
	}

	ret = ss.str();  // 将字符串流转换为字符串
	if (ret.size() > 0)  // 如果字符串不为空
		ret = ret.substr(0, ret.size() - 1);  // 移除最后一个逗号
	return ret.c_str();  // 返回C风格字符串
}

/**
 * @brief 买入
 * 
 * 执行买入操作，提交买入订单
 * 如果订单被拆单，会返回多个订单ID（逗号分隔）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param price 买入价格（0表示市价）
 * @param qty 买入数量
 * @param userTag 用户标签（用于标识该笔交易）
 * @param flag 订单标志（0-普通单，其他值可扩展）
 * @return 订单ID列表（逗号分隔的字符串，如果拆单则返回多个订单ID，如果模拟器不存在则返回空字符串）
 */
WtString hft_buy(CtxHandler cHandle, const char* stdCode, double price, double qty, const char* userTag, int flag)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，返回空字符串
		return "";

	static std::string ret;  // 静态变量，用于存储返回的订单ID列表字符串

	std::stringstream ss;  // 字符串流，用于构建订单ID列表
	OrderIDs ids = mocker->stra_buy(stdCode, price, qty, userTag, flag);  // 调用模拟器的买入方法
	for (WtUInt32 localid : ids)  // 遍历所有订单ID（如果拆单则可能有多个）
	{
		ss << localid << ",";  // 将订单ID添加到字符串流中，用逗号分隔
	}

	ret = ss.str();  // 将字符串流转换为字符串
	ret = ret.substr(0, ret.size() - 1);  // 移除最后一个逗号
	return ret.c_str();  // 返回C风格字符串
}

/**
 * @brief 卖出
 * 
 * 执行卖出操作，提交卖出订单
 * 如果订单被拆单，会返回多个订单ID（逗号分隔）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param stdCode 标准合约代码
 * @param price 卖出价格（0表示市价）
 * @param qty 卖出数量
 * @param userTag 用户标签（用于标识该笔交易）
 * @param flag 订单标志（0-普通单，其他值可扩展）
 * @return 订单ID列表（逗号分隔的字符串，如果拆单则返回多个订单ID，如果模拟器不存在则返回空字符串）
 */
WtString hft_sell(CtxHandler cHandle, const char* stdCode, double price, double qty, const char* userTag, int flag)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，返回空字符串
		return "";

	static std::string ret;  // 静态变量，用于存储返回的订单ID列表字符串

	std::stringstream ss;  // 字符串流，用于构建订单ID列表
	OrderIDs ids = mocker->stra_sell(stdCode, price, qty, userTag, flag);  // 调用模拟器的卖出方法
	for (WtUInt32 localid : ids)  // 遍历所有订单ID（如果拆单则可能有多个）
	{
		ss << localid << ",";  // 将订单ID添加到字符串流中，用逗号分隔
	}

	ret = ss.str();  // 将字符串流转换为字符串
	ret = ret.substr(0, ret.size() - 1);  // 移除最后一个逗号
	return ret.c_str();  // 返回C风格字符串
}

/**
 * @brief 保存用户数据
 * 
 * 保存策略的用户自定义数据（持久化存储，程序重启后仍可读取）
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param key 数据键名
 * @param val 数据值（字符串格式）
 */
void hft_save_userdata(CtxHandler cHandle, const char* key, const char* val)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回
		return;

	mocker->stra_save_user_data(key, val);  // 调用模拟器的保存用户数据方法
}

/**
 * @brief 加载用户数据
 * 
 * 加载策略的用户自定义数据
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 * @param key 数据键名
 * @param defVal 默认值（如果数据不存在则返回此值）
 * @return 数据值字符串（如果模拟器不存在则返回默认值）
 */
WtString hft_load_userdata(CtxHandler cHandle, const char* key, const char* defVal)
{
	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，返回默认值
		return defVal;

	return mocker->stra_load_user_data(key, defVal);  // 调用模拟器的加载用户数据方法
}

/**
 * @brief 执行策略单步回测
 * 
 * 在异步回测模式下，手动触发策略的单步执行（用于调试或精确控制回测进度）
 * 注意：只有异步模式才有意义，同步模式下直接返回
 * 
 * @param cHandle 策略上下文句柄（回测模式下此参数未使用）
 */
void hft_step(CtxHandler cHandle)
{
	//只有异步模式才有意义
	if (!getRunner().isAsync())  // 如果不是异步模式，直接返回
		return;

	HftMocker* mocker = getRunner().hft_mocker();  // 获取HFT模拟器对象
	if (mocker == NULL)  // 如果模拟器不存在，直接返回
		return;

	mocker->step_tick();  // 调用模拟器的单步Tick处理方法
}
#pragma endregion "HFT策略接口"