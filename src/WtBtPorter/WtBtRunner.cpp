/*!
 * \file WtBtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtBtRunner回测运行器类实现文件
 * 
 * 本文件实现了WtBtRunner类的所有方法，这是WtBtPorter模块的核心实现类，负责管理整个回测系统的运行时环境。
 * WtBtRunner作为回测引擎的中心调度器，实现了：
 * 1. 历史数据加载（通过IBtDataLoader接口，支持外部数据加载器）
 * 2. 策略模拟器管理（CTA、HFT、SEL模拟器的创建和生命周期管理）
 * 3. 回调函数管理（注册和管理各种策略事件回调函数，将事件转发给外部语言）
 * 4. 历史数据回放（通过HisDataReplayer组件回放历史数据）
 * 5. 事件通知（引擎初始化、交易日事件、回测结束等事件的通知）
 * 6. 回测执行控制（同步/异步回测执行、停止、释放等）
 * 
 * 设计逻辑：
 * - WtBtRunner作为单例对象，管理整个回测系统的生命周期
 * - 通过回调函数机制实现事件驱动的编程模型，将回测过程中的各种事件转发给外部语言
 * - 支持多种策略类型（CTA、HFT、SEL），每种策略类型都有对应的模拟器（Mocker）
 * - 通过HisDataReplayer组件回放历史数据，模拟器接收数据并执行策略逻辑
 * - 支持外部数据加载器，允许从自定义数据源加载历史数据
 * - 支持同步和异步两种回测模式
 */
#include "WtBtRunner.h"  // WtBtRunner类定义
#include "ExpCtaMocker.h"  // CTA策略模拟器扩展类
#include "ExpSelMocker.h"  // SEL策略模拟器扩展类
#include "ExpHftMocker.h"  // HFT策略模拟器扩展类

#include <iomanip>  // 输入输出流格式化库

#include "../WtBtCore/ExecMocker.h"  // 执行器模拟器
#include "../WtBtCore/WtHelper.h"  // WonderTrader辅助工具类

#include "../Share/TimeUtils.hpp"  // 时间工具函数
#include "../Share/ModuleHelper.hpp"  // 模块辅助函数

#include "../WTSTools/WTSLogger.h"  // 日志记录器
#include "../WTSUtils/WTSCfgLoader.h"  // 配置加载器
#include "../Includes/WTSVariant.hpp"  // 变体类型（用于配置）
#include "../WTSUtils/SignalHook.hpp"  // 信号钩子

#ifdef _MSC_VER  // 如果是MSVC编译器
#include "../Common/mdump.h"  // MiniDumper崩溃转储工具
#include <boost/filesystem.hpp>  // Boost文件系统库
/**
 * @brief 获取模块名称
 * 
 * 这个函数主要是给MiniDumper用的，用于获取当前DLL模块的名称
 * 
 * @return 返回模块名称字符串（静态线程局部存储）
 */
const char* getModuleName()
{
	static char MODULE_NAME[250] = { 0 };  // 静态缓冲区存储模块名称
	if (strlen(MODULE_NAME) == 0)  // 如果尚未初始化
	{
		GetModuleFileName(g_dllModule, MODULE_NAME, 250);  // 获取当前模块的完整路径
		boost::filesystem::path p(MODULE_NAME);  // 转换为文件路径对象
		strcpy(MODULE_NAME, p.filename().string().c_str());  // 提取文件名部分
	}

	return MODULE_NAME;  // 返回模块名称
}
#endif

/**
 * @brief WtBtRunner构造函数
 * 
 * 初始化WtBtRunner对象的所有成员变量，包括：
 * - 策略模拟器指针（CTA、SEL、HFT）初始化为NULL
 * - 所有回调函数指针初始化为NULL
 * - 外部数据加载器指针初始化为NULL
 * - 状态标志（_inited、_running、_async）初始化为false
 * - 安装信号钩子，用于捕获异常信号并记录日志
 */
WtBtRunner::WtBtRunner()
	: _cta_mocker(NULL)  // CTA策略模拟器指针初始化为NULL
	, _sel_mocker(NULL)  // SEL策略模拟器指针初始化为NULL

	, _cb_cta_init(NULL)  // CTA策略初始化回调函数指针初始化为NULL
	, _cb_cta_tick(NULL)  // CTA策略Tick更新回调函数指针初始化为NULL
	, _cb_cta_calc(NULL)  // CTA策略计算回调函数指针初始化为NULL
	, _cb_cta_calc_done(NULL)  // CTA策略计算完成回调函数指针初始化为NULL
	, _cb_cta_bar(NULL)  // CTA策略K线闭合回调函数指针初始化为NULL
	, _cb_cta_sessevt(NULL)  // CTA策略交易日事件回调函数指针初始化为NULL
	, _cb_cta_cond_trigger(NULL)  // CTA策略条件触发回调函数指针初始化为NULL

	, _cb_sel_init(NULL)  // SEL策略初始化回调函数指针初始化为NULL
	, _cb_sel_tick(NULL)  // SEL策略Tick更新回调函数指针初始化为NULL
	, _cb_sel_calc(NULL)  // SEL策略计算回调函数指针初始化为NULL
	, _cb_sel_calc_done(NULL)  // SEL策略计算完成回调函数指针初始化为NULL
	, _cb_sel_bar(NULL)  // SEL策略K线闭合回调函数指针初始化为NULL
	, _cb_sel_sessevt(NULL)  // SEL策略交易日事件回调函数指针初始化为NULL

	, _cb_hft_init(NULL)  // HFT策略初始化回调函数指针初始化为NULL
	, _cb_hft_tick(NULL)  // HFT策略Tick更新回调函数指针初始化为NULL
	, _cb_hft_bar(NULL)  // HFT策略K线闭合回调函数指针初始化为NULL
	, _cb_hft_ord(NULL)  // HFT策略订单回调函数指针初始化为NULL
	, _cb_hft_trd(NULL)  // HFT策略成交回调函数指针初始化为NULL
	, _cb_hft_entrust(NULL)  // HFT策略委托回调函数指针初始化为NULL
	, _cb_hft_chnl(NULL)  // HFT策略通道回调函数指针初始化为NULL

	, _cb_hft_orddtl(NULL)  // HFT策略订单明细回调函数指针初始化为NULL
	, _cb_hft_ordque(NULL)  // HFT策略订单队列回调函数指针初始化为NULL
	, _cb_hft_trans(NULL)  // HFT策略逐笔成交回调函数指针初始化为NULL

	, _cb_hft_sessevt(NULL)  // HFT策略交易日事件回调函数指针初始化为NULL

	, _ext_fnl_bar_loader(NULL)  // 外部最终K线数据加载器指针初始化为NULL
	, _ext_raw_bar_loader(NULL)  // 外部原始K线数据加载器指针初始化为NULL
	, _ext_adj_fct_loader(NULL)  // 外部复权因子加载器指针初始化为NULL
	, _ext_tick_loader(NULL)  // 外部Tick数据加载器指针初始化为NULL

	, _inited(false)  // 初始化标志初始化为false
	, _running(false)  // 运行状态标志初始化为false
	, _async(false)  // 异步模式标志初始化为false
{
	install_signal_hooks([](const char* message) {  // 安装信号钩子，捕获异常信号
		WTSLogger::error(message);  // 记录错误日志
	});
}

/**
 * @brief WtBtRunner析构函数
 * 
 * 清理WtBtRunner对象的资源
 */
WtBtRunner::~WtBtRunner()
{
}

/**
 * @brief 加载原始历史K线数据
 * 
 * 通过外部原始K线数据加载器加载指定合约的原始历史K线数据（未复权）
 * 
 * @param obj 数据加载上下文对象指针
 * @param stdCode 标准合约代码
 * @param period K线周期类型（日线、1分钟、5分钟等）
 * @param cb 数据读取回调函数，用于接收加载的K线数据
 * @return 如果加载成功返回true，否则返回false
 */
bool WtBtRunner::loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb)
{
	StdUniqueLock lock(_feed_mtx);  // 加锁保护数据加载过程
	if (_ext_raw_bar_loader == NULL)  // 如果外部原始K线数据加载器未注册
		return false;  // 返回失败

	_feed_obj = obj;  // 保存数据加载上下文对象
	_feeder_bars = cb;  // 保存数据读取回调函数

	switch (period)  // 根据K线周期类型选择对应的周期字符串
	{
	case KP_DAY:  // 日线
        return _ext_raw_bar_loader(stdCode, "d1");  // 调用外部加载器加载日线数据
	case KP_Minute1:  // 1分钟线
        return _ext_raw_bar_loader(stdCode, "m1");  // 调用外部加载器加载1分钟线数据
	case KP_Minute5:  // 5分钟线
        return _ext_raw_bar_loader(stdCode, "m5");  // 调用外部加载器加载5分钟线数据
	default:  // 不支持的周期类型
		{
			WTSLogger::error("Unsupported period of extended data loader");  // 记录错误日志
			return false;  // 返回失败
		}
	}
}

/**
 * @brief 加载最终历史K线数据
 * 
 * 通过外部最终K线数据加载器加载指定合约的最终历史K线数据（已复权）
 * 
 * @param obj 数据加载上下文对象指针
 * @param stdCode 标准合约代码
 * @param period K线周期类型（日线、1分钟、5分钟等）
 * @param cb 数据读取回调函数，用于接收加载的K线数据
 * @return 如果加载成功返回true，否则返回false
 */
bool WtBtRunner::loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb)
{
	StdUniqueLock lock(_feed_mtx);  // 加锁保护数据加载过程
	if (_ext_fnl_bar_loader == NULL)  // 如果外部最终K线数据加载器未注册
		return false;  // 返回失败

	_feed_obj = obj;  // 保存数据加载上下文对象
	_feeder_bars = cb;  // 保存数据读取回调函数

	switch (period)  // 根据K线周期类型选择对应的周期字符串
	{
	case KP_DAY:  // 日线
		return _ext_fnl_bar_loader(stdCode, "d1");  // 调用外部加载器加载日线数据
	case KP_Minute1:  // 1分钟线
		return _ext_fnl_bar_loader(stdCode, "m1");  // 调用外部加载器加载1分钟线数据
	case KP_Minute5:  // 5分钟线
		return _ext_fnl_bar_loader(stdCode, "m5");  // 调用外部加载器加载5分钟线数据
	default:  // 不支持的周期类型
		{
			WTSLogger::error("Unsupported period of extended data loader");  // 记录错误日志
			return false;  // 返回失败
		}
	}
}

/**
 * @brief 加载所有合约的复权因子
 * 
 * 通过外部复权因子加载器加载所有合约的复权因子数据
 * 
 * @param obj 数据加载上下文对象指针
 * @param cb 数据读取回调函数，用于接收加载的复权因子数据
 * @return 如果加载成功返回true，否则返回false
 */
bool WtBtRunner::loadAllAdjFactors(void* obj, FuncReadFactors cb)
{
	StdUniqueLock lock(_feed_mtx);  // 加锁保护数据加载过程
	if (_ext_adj_fct_loader == NULL)  // 如果外部复权因子加载器未注册
		return false;  // 返回失败

	_feed_obj = obj;  // 保存数据加载上下文对象
	_feeder_fcts = cb;  // 保存数据读取回调函数

	return _ext_adj_fct_loader("");  // 调用外部加载器加载所有合约的复权因子（空字符串表示所有合约）
}

/**
 * @brief 加载指定合约的复权因子
 * 
 * 通过外部复权因子加载器加载指定合约的复权因子数据
 * 
 * @param obj 数据加载上下文对象指针
 * @param stdCode 标准合约代码
 * @param cb 数据读取回调函数，用于接收加载的复权因子数据
 * @return 如果加载成功返回true，否则返回false
 */
bool WtBtRunner::loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb)
{
	StdUniqueLock lock(_feed_mtx);  // 加锁保护数据加载过程
	if (_ext_adj_fct_loader == NULL)  // 如果外部复权因子加载器未注册
		return false;  // 返回失败

	_feed_obj = obj;  // 保存数据加载上下文对象
	_feeder_fcts = cb;  // 保存数据读取回调函数

	return _ext_adj_fct_loader(stdCode);  // 调用外部加载器加载指定合约的复权因子
}

/**
 * @brief 加载原始历史Tick数据
 * 
 * 通过外部Tick数据加载器加载指定合约在指定日期的原始历史Tick数据
 * 
 * @param obj 数据加载上下文对象指针
 * @param stdCode 标准合约代码
 * @param uDate 交易日（格式：YYYYMMDD）
 * @param cb 数据读取回调函数，用于接收加载的Tick数据
 * @return 如果加载成功返回true，否则返回false
 */
bool WtBtRunner::loadRawHisTicks(void* obj, const char* stdCode, uint32_t uDate, FuncReadTicks cb)
{
	StdUniqueLock lock(_feed_mtx);  // 加锁保护数据加载过程
	if (_ext_tick_loader == NULL)  // 如果外部Tick数据加载器未注册
		return false;  // 返回失败

	_feed_obj = obj;  // 保存数据加载上下文对象
	_feeder_ticks = cb;  // 保存数据读取回调函数

	return _ext_tick_loader(stdCode, uDate);  // 调用外部加载器加载指定合约在指定日期的Tick数据
}

/**
 * @brief 推送原始K线数据
 * 
 * 将外部加载的原始K线数据推送给回测引擎的数据加载器
 * 
 * @param bars K线数据数组指针
 * @param count K线数据数量
 */
void WtBtRunner::feedRawBars(WTSBarStruct* bars, uint32_t count)
{
	if(_ext_fnl_bar_loader == NULL && _ext_raw_bar_loader == NULL)  // 如果外部K线数据加载器未注册
	{
		WTSLogger::error("Cannot feed bars because of no extented bar loader registered.");  // 记录错误日志
		return;  // 直接返回
	}

	_feeder_bars(_feed_obj, bars, count);  // 调用数据读取回调函数，推送K线数据
}

/**
 * @brief 推送复权因子数据
 * 
 * 将外部加载的复权因子数据推送给回测引擎的数据加载器
 * 
 * @param stdCode 标准合约代码
 * @param dates 复权日期数组指针
 * @param factors 复权因子数组指针
 * @param count 复权因子数量
 */
void WtBtRunner::feedAdjFactors(const char* stdCode, uint32_t* dates, double* factors, uint32_t count)
{
	if(_ext_adj_fct_loader == NULL)  // 如果外部复权因子加载器未注册
	{
		WTSLogger::error("Cannot feed adjusting factors because of no extented adjusting factor loader registered.");  // 记录错误日志
		return;  // 直接返回
	}

	_feeder_fcts(_feed_obj, stdCode, dates, factors, count);  // 调用数据读取回调函数，推送复权因子数据
}

/**
 * @brief 推送原始Tick数据
 * 
 * 将外部加载的原始Tick数据推送给回测引擎的数据加载器
 * 
 * @param ticks Tick数据数组指针
 * @param count Tick数据数量
 */
void WtBtRunner::feedRawTicks(WTSTickStruct* ticks, uint32_t count)
{
	if (_ext_tick_loader == NULL)  // 如果外部Tick数据加载器未注册
	{
		WTSLogger::error("Cannot feed ticks because of no extented tick loader registered.");  // 记录错误日志
		return;  // 直接返回
	}

	_feeder_ticks(_feed_obj, ticks, count);  // 调用数据读取回调函数，推送Tick数据
}

/**
 * @brief 注册CTA策略回调函数
 * 
 * 注册CTA策略相关的所有回调函数，用于接收CTA策略的各种事件通知
 * 
 * @param cbInit CTA策略初始化回调函数
 * @param cbTick CTA策略Tick更新回调函数
 * @param cbCalc CTA策略计算回调函数
 * @param cbBar CTA策略K线闭合回调函数
 * @param cbSessEvt CTA策略交易日事件回调函数
 * @param cbCalcDone CTA策略计算完成回调函数（可选）
 * @param cbCondTrigger CTA策略条件触发回调函数（可选）
 */
void WtBtRunner::registerCtaCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, 
	FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone /* = NULL */, FuncStraCondTriggerCallback cbCondTrigger /* = NULL */)
{
	_cb_cta_init = cbInit;  // 保存CTA策略初始化回调函数
	_cb_cta_tick = cbTick;  // 保存CTA策略Tick更新回调函数
	_cb_cta_calc = cbCalc;  // 保存CTA策略计算回调函数
	_cb_cta_bar = cbBar;  // 保存CTA策略K线闭合回调函数
	_cb_cta_sessevt = cbSessEvt;  // 保存CTA策略交易日事件回调函数

	_cb_cta_calc_done = cbCalcDone;  // 保存CTA策略计算完成回调函数
	_cb_cta_cond_trigger = cbCondTrigger;  // 保存CTA策略条件触发回调函数

	WTSLogger::info("Callbacks of CTA engine registration done");  // 记录注册完成日志
}

/**
 * @brief 注册SEL策略回调函数
 * 
 * 注册SEL策略相关的所有回调函数，用于接收SEL策略的各种事件通知
 * 
 * @param cbInit SEL策略初始化回调函数
 * @param cbTick SEL策略Tick更新回调函数
 * @param cbCalc SEL策略计算回调函数
 * @param cbBar SEL策略K线闭合回调函数
 * @param cbSessEvt SEL策略交易日事件回调函数
 * @param cbCalcDone SEL策略计算完成回调函数（可选）
 */
void WtBtRunner::registerSelCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, 
	FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt, FuncStraCalcCallback cbCalcDone/* = NULL*/)
{
	_cb_sel_init = cbInit;  // 保存SEL策略初始化回调函数
	_cb_sel_tick = cbTick;  // 保存SEL策略Tick更新回调函数
	_cb_sel_calc = cbCalc;  // 保存SEL策略计算回调函数
	_cb_sel_bar = cbBar;  // 保存SEL策略K线闭合回调函数
	_cb_sel_sessevt = cbSessEvt;  // 保存SEL策略交易日事件回调函数

	_cb_sel_calc_done = cbCalcDone;  // 保存SEL策略计算完成回调函数

	WTSLogger::info("Callbacks of SEL engine registration done");  // 记录注册完成日志
}

/**
 * @brief 注册HFT策略回调函数
 * 
 * 注册HFT策略相关的所有回调函数，用于接收HFT策略的各种事件通知
 * 
 * @param cbInit HFT策略初始化回调函数
 * @param cbTick HFT策略Tick更新回调函数
 * @param cbBar HFT策略K线闭合回调函数
 * @param cbChnl HFT策略通道事件回调函数
 * @param cbOrd HFT策略订单回调函数
 * @param cbTrd HFT策略成交回调函数
 * @param cbEntrust HFT策略委托回调函数
 * @param cbOrdDtl HFT策略订单明细回调函数
 * @param cbOrdQue HFT策略订单队列回调函数
 * @param cbTrans HFT策略逐笔成交回调函数
 * @param cbSessEvt HFT策略交易日事件回调函数
 */
void WtBtRunner::registerHftCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraBarCallback cbBar,
	FuncHftChannelCallback cbChnl, FuncHftOrdCallback cbOrd, FuncHftTrdCallback cbTrd, FuncHftEntrustCallback cbEntrust, 
	FuncStraOrdDtlCallback cbOrdDtl, FuncStraOrdQueCallback cbOrdQue, FuncStraTransCallback cbTrans, FuncSessionEvtCallback cbSessEvt)
{
	_cb_hft_init = cbInit;  // 保存HFT策略初始化回调函数
	_cb_hft_tick = cbTick;  // 保存HFT策略Tick更新回调函数
	_cb_hft_bar = cbBar;  // 保存HFT策略K线闭合回调函数

	_cb_hft_chnl = cbChnl;  // 保存HFT策略通道事件回调函数
	_cb_hft_ord = cbOrd;  // 保存HFT策略订单回调函数
	_cb_hft_trd = cbTrd;  // 保存HFT策略成交回调函数
	_cb_hft_entrust = cbEntrust;  // 保存HFT策略委托回调函数

	_cb_hft_orddtl = cbOrdDtl;  // 保存HFT策略订单明细回调函数
	_cb_hft_ordque = cbOrdQue;  // 保存HFT策略订单队列回调函数
	_cb_hft_trans = cbTrans;  // 保存HFT策略逐笔成交回调函数

	_cb_hft_sessevt = cbSessEvt;  // 保存HFT策略交易日事件回调函数

	WTSLogger::info("Callbacks of HFT engine registration done");  // 记录注册完成日志
}

/**
 * @brief 初始化CTA策略模拟器
 * 
 * 创建并初始化CTA策略模拟器，用于回测CTA策略
 * 
 * @param name 策略名称
 * @param slippage 滑点设置（单位：最小变动价位）
 * @param hook 是否安装钩子（用于异步回测）
 * @param persistData 是否持久化数据
 * @param bIncremental 是否加载增量数据
 * @param isRatioSlp 是否使用比例滑点
 * @return 返回策略上下文ID
 */
uint32_t WtBtRunner::initCtaMocker(const char* name, int32_t slippage /* = 0 */, bool hook /* = false */, 
	bool persistData /* = true */, bool bIncremental /* = false */, bool isRatioSlp /* = false */)
{
	if(_cta_mocker)  // 如果已存在CTA模拟器
	{
		delete _cta_mocker;  // 删除旧的模拟器
		_cta_mocker = NULL;  // 重置指针
	}

	_cta_mocker = new ExpCtaMocker(&_replayer, name, slippage, persistData, &_notifier, isRatioSlp);  // 创建新的CTA模拟器
	if (bIncremental)  // 如果需要加载增量数据
	{
		_cta_mocker->load_incremental_data(name);  // 加载增量数据
	}
	if(hook) _cta_mocker->install_hook();  // 如果需要，安装钩子
	_replayer.register_sink(_cta_mocker, name);  // 将模拟器注册为数据回放器的接收器
	return _cta_mocker->id();  // 返回策略上下文ID
}

/**
 * @brief 初始化HFT策略模拟器
 * 
 * 创建并初始化HFT策略模拟器，用于回测HFT策略
 * 
 * @param name 策略名称
 * @param hook 是否安装钩子（用于异步回测）
 * @return 返回策略上下文ID
 */
uint32_t WtBtRunner::initHftMocker(const char* name, bool hook/* = false*/)
{
	if (_hft_mocker)  // 如果已存在HFT模拟器
	{
		delete _hft_mocker;  // 删除旧的模拟器
		_hft_mocker = NULL;  // 重置指针
	}

	_hft_mocker = new ExpHftMocker(&_replayer, name);  // 创建新的HFT模拟器
	if (hook) _hft_mocker->install_hook();  // 如果需要，安装钩子
	_replayer.register_sink(_hft_mocker, name);  // 将模拟器注册为数据回放器的接收器
	return _hft_mocker->id();  // 返回策略上下文ID
}

/**
 * @brief 初始化SEL策略模拟器
 * 
 * 创建并初始化SEL策略模拟器，用于回测SEL策略
 * 
 * @param name 策略名称
 * @param date 策略调度日期（格式：YYYYMMDD）
 * @param time 策略调度时间（格式：HHMMSS）
 * @param period 策略调度周期（如"m1"、"d1"等）
 * @param trdtpl 交易日历模板（默认为"CHINA"）
 * @param session 交易时段（默认为"TRADING"）
 * @param slippage 滑点设置（单位：最小变动价位）
 * @param isRatioSlp 是否使用比例滑点
 * @return 返回策略上下文ID
 */
uint32_t WtBtRunner::initSelMocker(const char* name, uint32_t date, uint32_t time, const char* period, 
	const char* trdtpl /* = "CHINA" */, const char* session /* = "TRADING" */, int32_t slippage /* = 0 */, bool isRatioSlp /* = false */)
{
	if (_sel_mocker)  // 如果已存在SEL模拟器
	{
		delete _sel_mocker;  // 删除旧的模拟器
		_sel_mocker = NULL;  // 重置指针
	}

	_sel_mocker = new ExpSelMocker(&_replayer, name, slippage, isRatioSlp);  // 创建新的SEL模拟器
	_replayer.register_sink(_sel_mocker, name);  // 将模拟器注册为数据回放器的接收器

	_replayer.register_task(_sel_mocker->id(), date, time, period, trdtpl, session);  // 注册策略调度任务
	return _sel_mocker->id();  // 返回策略上下文ID
}

/**
 * @brief 通知K线闭合事件
 * 
 * 根据引擎类型，调用对应的K线闭合回调函数，通知外部语言K线闭合事件
 * 
 * @param id 策略上下文ID
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param newBar 新生成的K线数据结构指针
 * @param eType 引擎类型（CTA、HFT或SEL）
 */
void WtBtRunner::ctx_on_bar(uint32_t id, const char* stdCode, const char* period, WTSBarStruct* newBar, EngineType eType/*= ET_CTA*/)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_bar) _cb_cta_bar(id, stdCode, period, newBar); break;  // CTA引擎：调用CTA K线闭合回调
	case ET_HFT: if (_cb_hft_bar) _cb_hft_bar(id, stdCode, period, newBar); break;  // HFT引擎：调用HFT K线闭合回调
	case ET_SEL: if (_cb_sel_bar) _cb_sel_bar(id, stdCode, period, newBar); break;  // SEL引擎：调用SEL K线闭合回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知策略计算事件
 * 
 * 根据引擎类型，调用对应的策略计算回调函数，通知外部语言执行策略计算
 * 
 * @param id 策略上下文ID
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 * @param eType 引擎类型（CTA或SEL）
 */
void WtBtRunner::ctx_on_calc(uint32_t id, uint32_t curDate, uint32_t curTime, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_calc) _cb_cta_calc(id, curDate, curTime); break;  // CTA引擎：调用CTA计算回调
	case ET_SEL: if (_cb_sel_calc) _cb_sel_calc(id, curDate, curTime); break;  // SEL引擎：调用SEL计算回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知策略计算完成事件
 * 
 * 根据引擎类型，调用对应的策略计算完成回调函数，通知外部语言策略计算已完成
 * 
 * @param id 策略上下文ID
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 * @param eType 引擎类型（CTA或SEL）
 */
void WtBtRunner::ctx_on_calc_done(uint32_t id, uint32_t curDate, uint32_t curTime, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_calc_done) _cb_cta_calc_done(id, curDate, curTime); break;  // CTA引擎：调用CTA计算完成回调
	case ET_SEL: if (_cb_sel_calc_done) _cb_sel_calc_done(id, curDate, curTime); break;  // SEL引擎：调用SEL计算完成回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知策略初始化事件
 * 
 * 根据引擎类型，调用对应的策略初始化回调函数，通知外部语言策略已初始化
 * 
 * @param id 策略上下文ID
 * @param eType 引擎类型（CTA、HFT或SEL）
 */
void WtBtRunner::ctx_on_init(uint32_t id, EngineType eType/*= ET_CTA*/)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_init) _cb_cta_init(id); break;  // CTA引擎：调用CTA初始化回调
	case ET_HFT: if (_cb_hft_init) _cb_hft_init(id); break;  // HFT引擎：调用HFT初始化回调
	case ET_SEL: if (_cb_sel_init) _cb_sel_init(id); break;  // SEL引擎：调用SEL初始化回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知条件触发事件
 * 
 * 根据引擎类型，调用对应的条件触发回调函数，通知外部语言条件已触发
 * 
 * @param id 策略上下文ID
 * @param stdCode 标准合约代码
 * @param target 目标价格
 * @param price 触发价格
 * @param usertag 用户标签
 * @param eType 引擎类型（目前仅支持CTA）
 */
void WtBtRunner::ctx_on_cond_triggered(uint32_t id, const char* stdCode, double target, double price, const char* usertag, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_cond_trigger) _cb_cta_cond_trigger(id, stdCode, target, price, usertag); break;  // CTA引擎：调用CTA条件触发回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知交易日事件
 * 
 * 根据引擎类型，调用对应的交易日事件回调函数，通知外部语言交易日开始或结束
 * 
 * @param id 策略上下文ID
 * @param curTDate 当前交易日（格式：YYYYMMDD）
 * @param isBegin 是否为交易日开始（true表示开始，false表示结束）
 * @param eType 引擎类型（CTA、HFT或SEL）
 */
void WtBtRunner::ctx_on_session_event(uint32_t id, uint32_t curTDate, bool isBegin /* = true */, EngineType eType /* = ET_CTA */)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_sessevt) _cb_cta_sessevt(id, curTDate, isBegin); break;  // CTA引擎：调用CTA交易日事件回调
	case ET_HFT: if (_cb_hft_sessevt) _cb_hft_sessevt(id, curTDate, isBegin); break;  // HFT引擎：调用HFT交易日事件回调
	case ET_SEL: if (_cb_sel_sessevt) _cb_sel_sessevt(id, curTDate, isBegin); break;  // SEL引擎：调用SEL交易日事件回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知Tick更新事件
 * 
 * 根据引擎类型，调用对应的Tick更新回调函数，通知外部语言Tick数据已更新
 * 
 * @param id 策略上下文ID
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针
 * @param eType 引擎类型（CTA、HFT或SEL）
 */
void WtBtRunner::ctx_on_tick(uint32_t id, const char* stdCode, WTSTickData* newTick, EngineType eType/*= ET_CTA*/)
{
	switch (eType)  // 根据引擎类型选择对应的回调函数
	{
	case ET_CTA: if (_cb_cta_tick) _cb_cta_tick(id, stdCode, &newTick->getTickStruct()); break;  // CTA引擎：调用CTA Tick更新回调
	case ET_HFT: if (_cb_hft_tick) _cb_hft_tick(id, stdCode, &newTick->getTickStruct()); break;  // HFT引擎：调用HFT Tick更新回调
	case ET_SEL: if (_cb_sel_tick) _cb_sel_tick(id, stdCode, &newTick->getTickStruct()); break;  // SEL引擎：调用SEL Tick更新回调
	default:
		break;  // 其他类型：不处理
	}
}

/**
 * @brief 通知HFT订单队列更新事件
 * 
 * 调用HFT订单队列更新回调函数，通知外部语言订单队列数据已更新
 * 
 * @param id 策略上下文ID
 * @param stdCode 标准合约代码
 * @param newOrdQue 新的订单队列数据指针
 */
void WtBtRunner::hft_on_order_queue(uint32_t id, const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_cb_hft_ordque)  // 如果回调函数已注册
		_cb_hft_ordque(id, stdCode, &newOrdQue->getOrdQueStruct());  // 调用订单队列更新回调
}

/**
 * @brief 通知HFT订单明细更新事件
 * 
 * 调用HFT订单明细更新回调函数，通知外部语言订单明细数据已更新
 * 
 * @param id 策略上下文ID
 * @param stdCode 标准合约代码
 * @param newOrdDtl 新的订单明细数据指针
 */
void WtBtRunner::hft_on_order_detail(uint32_t id, const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_cb_hft_orddtl)  // 如果回调函数已注册
		_cb_hft_orddtl(id, stdCode, &newOrdDtl->getOrdDtlStruct());  // 调用订单明细更新回调
}

/**
 * @brief 通知HFT逐笔成交更新事件
 * 
 * 调用HFT逐笔成交更新回调函数，通知外部语言逐笔成交数据已更新
 * 
 * @param id 策略上下文ID
 * @param stdCode 标准合约代码
 * @param newTrans 新的逐笔成交数据指针
 */
void WtBtRunner::hft_on_transaction(uint32_t id, const char* stdCode, WTSTransData* newTrans)
{
	if (_cb_hft_trans)  // 如果回调函数已注册
		_cb_hft_trans(id, stdCode, &newTrans->getTransStruct());  // 调用逐笔成交更新回调
}

/**
 * @brief 通知HFT通道就绪事件
 * 
 * 调用HFT通道事件回调函数，通知外部语言交易通道已就绪
 * 
 * @param cHandle 通道句柄
 * @param trader 交易通道名称
 */
void WtBtRunner::hft_on_channel_ready(uint32_t cHandle, const char* trader)
{
	if (_cb_hft_chnl)  // 如果回调函数已注册
		_cb_hft_chnl(cHandle, trader, 1000/*CHNL_EVENT_READY*/);  // 调用通道事件回调，1000表示通道就绪事件
}

/**
 * @brief 通知HFT委托事件
 * 
 * 调用HFT委托回调函数，通知外部语言委托结果
 * 
 * @param cHandle 通道句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 消息内容
 * @param userTag 用户标签
 */
void WtBtRunner::hft_on_entrust(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag)
{
	if (_cb_hft_entrust)  // 如果回调函数已注册
		_cb_hft_entrust(cHandle, localid, stdCode, bSuccess, message, userTag);  // 调用委托回调
}

/**
 * @brief 通知HFT订单事件
 * 
 * 调用HFT订单回调函数，通知外部语言订单状态变化
 * 
 * @param cHandle 通道句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入
 * @param totalQty 总数量
 * @param leftQty 剩余数量
 * @param price 价格
 * @param isCanceled 是否已撤销
 * @param userTag 用户标签
 */
void WtBtRunner::hft_on_order(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag)
{
	if (_cb_hft_ord)  // 如果回调函数已注册
		_cb_hft_ord(cHandle, localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, userTag);  // 调用订单回调
}

/**
 * @brief 通知HFT成交事件
 * 
 * 调用HFT成交回调函数，通知外部语言成交结果
 * 
 * @param cHandle 通道句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入
 * @param vol 成交量
 * @param price 成交价格
 * @param userTag 用户标签
 */
void WtBtRunner::hft_on_trade(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag)
{
	if (_cb_hft_trd)  // 如果回调函数已注册
		_cb_hft_trd(cHandle, localid, stdCode, isBuy, vol, price, userTag);  // 调用成交回调
}

/**
 * @brief 初始化回测运行器
 * 
 * 初始化日志系统、设置工作目录和输出目录，并启用MiniDumper（Windows平台）
 * 
 * @param logProfile 日志配置文件路径或内容
 * @param isFile 是否为文件路径（true表示文件路径，false表示配置内容）
 * @param outDir 输出目录路径
 */
void WtBtRunner::init(const char* logProfile /* = "" */, bool isFile /* = true */, const char* outDir/* = "./outputs_bt"*/)
{
#ifdef _MSC_VER  // 如果是MSVC编译器
	CMiniDumper::Enable(getModuleName(), true, WtHelper::getCWD().c_str());  // 启用MiniDumper崩溃转储功能
#endif

	WTSLogger::init(logProfile, isFile);  // 初始化日志系统

	WtHelper::setInstDir(getBinDir());  // 设置安装目录
	WtHelper::setOutputDir(outDir);  // 设置输出目录
}

/**
 * @brief 配置回测运行器
 * 
 * 从配置文件加载配置信息，初始化事件推送器、历史数据回放器，并根据配置创建对应的策略模拟器
 * 
 * @param cfgFile 配置文件路径或内容
 * @param isFile 是否为文件路径（true表示文件路径，false表示配置内容）
 */
void WtBtRunner::config(const char* cfgFile, bool isFile /* = true */)
{
	if(_inited)  // 如果已经初始化
	{
		WTSLogger::error("WtBtEngine has already been inited");  // 记录错误日志
		return;  // 直接返回
	}

	_cfg = isFile ? WTSCfgLoader::load_from_file(cfgFile) : WTSCfgLoader::load_from_content(cfgFile, false);  // 加载配置文件
	if(_cfg == NULL)  // 如果加载失败
	{
		WTSLogger::error("Loading config failed");  // 记录错误日志
		return;  // 直接返回
	}

	//初始化事件推送器
	initEvtNotifier(_cfg->get("notifier"));  // 初始化事件推送器

	_replayer.init(_cfg->get("replayer"), &_notifier, _ext_fnl_bar_loader != NULL ? this : NULL);  // 初始化历史数据回放器，如果外部数据加载器已注册，则使用this作为数据加载器

	WTSVariant* cfgEnv = _cfg->get("env");  // 获取环境配置
	const char* mode = cfgEnv->getCString("mocker");  // 获取模拟器类型（cta、hft、sel或exec）
	WTSVariant* cfgMode = _cfg->get(mode);  // 获取对应类型的配置
	if (strcmp(mode, "cta") == 0 && cfgMode)  // 如果是CTA模式
	{
		const char* name = cfgMode->getCString("name");  // 获取策略名称
		int32_t slippage = cfgMode->getInt32("slippage");  // 获取滑点设置
		_cta_mocker = new ExpCtaMocker(&_replayer, name, slippage, &_notifier);  // 创建CTA模拟器
		_cta_mocker->init_cta_factory(cfgMode);  // 初始化CTA工厂
		_replayer.register_sink(_cta_mocker, name);  // 将CTA模拟器注册为数据回放器的接收器
	}
	else if (strcmp(mode, "hft") == 0 && cfgMode)  // 如果是HFT模式
	{
		const char* name = cfgMode->getCString("name");  // 获取策略名称
		_hft_mocker = new ExpHftMocker(&_replayer, name);  // 创建HFT模拟器
		_hft_mocker->init_hft_factory(cfgMode);  // 初始化HFT工厂
		_replayer.register_sink(_hft_mocker, name);  // 将HFT模拟器注册为数据回放器的接收器
	}
	else if (strcmp(mode, "sel") == 0 && cfgMode)  // 如果是SEL模式
	{
		const char* name = cfgMode->getCString("name");  // 获取策略名称
		int32_t slippage = cfgMode->getInt32("slippage");  // 获取滑点设置
		_sel_mocker = new ExpSelMocker(&_replayer, name, slippage);  // 创建SEL模拟器
		_sel_mocker->init_sel_factory(cfgMode);  // 初始化SEL工厂
		_replayer.register_sink(_sel_mocker, name);  // 将SEL模拟器注册为数据回放器的接收器

		WTSVariant* cfgTask = cfgMode->get("task");  // 获取任务配置
		if(cfgTask)  // 如果任务配置存在
			_replayer.register_task(_sel_mocker->id(), cfgTask->getUInt32("date"), cfgTask->getUInt32("time"),
				cfgTask->getCString("period"), cfgTask->getCString("trdtpl"), cfgTask->getCString("session"));  // 注册策略调度任务
	}
	else if (strcmp(mode, "exec") == 0 && cfgMode)  // 如果是执行器模式
	{
		const char* name = cfgMode->getCString("name");  // 获取执行器名称
		_exec_mocker = new ExecMocker(&_replayer);  // 创建执行器模拟器
		_exec_mocker->init(cfgMode);  // 初始化执行器模拟器
		_replayer.register_sink(_exec_mocker, name);  // 将执行器模拟器注册为数据回放器的接收器
	}
}

/**
 * @brief 运行回测
 * 
 * 启动历史数据回放，执行回测。支持同步和异步两种模式。
 * 
 * @param bNeedDump 是否需要转储数据
 * @param bAsync 是否异步运行（true表示异步，false表示同步）
 */
void WtBtRunner::run(bool bNeedDump /* = false */, bool bAsync /* = false */)
{
	if (_running)  // 如果已经在运行
		return;  // 直接返回

	_async = bAsync;  // 保存异步模式标志

	WTSLogger::info("Backtesting will run in {} mode", _async ? "async" : "sync");  // 记录运行模式日志

	if (_cta_mocker)  // 如果使用CTA模拟器
		_cta_mocker->enable_hook(_async);  // 启用或禁用钩子（用于异步回测）
	else if (_hft_mocker)  // 如果使用HFT模拟器
		_hft_mocker->enable_hook(_async);  // 启用或禁用钩子（用于异步回测）

	_replayer.prepare();  // 准备历史数据回放器
	if (!bAsync)  // 如果是同步模式
	{
		_replayer.run(bNeedDump);  // 同步运行回放器
	}
	else  // 如果是异步模式
	{
		_worker.reset(new StdThread([this, bNeedDump]() {  // 创建工作线程
			_running = true;  // 设置运行标志
			try
			{
				_replayer.run(bNeedDump);  // 在独立线程中运行回放器
			}
			catch (...)  // 捕获所有异常
			{
				WTSLogger::error("Exception raised while worker running");  // 记录错误日志
				//print_stack_trace([](const char* message) {
				//	WTSLogger::error(message);
				//});
			}
			WTSLogger::debug("Worker thread of backtest finished");  // 记录线程结束日志
			_running = false;  // 清除运行标志

		}));
	}
}

/**
 * @brief 停止回测
 * 
 * 停止正在运行的回测，等待回测完成并清理资源
 */
void WtBtRunner::stop()
{
	if (!_running)  // 如果未在运行
	{
		if (_worker)  // 如果工作线程存在
		{
			_worker->join();  // 等待工作线程结束
			_worker.reset();  // 释放工作线程
		}
		return;  // 直接返回
	}

	_replayer.stop();  // 停止历史数据回放器

	WTSLogger::debug("Notify to finish last round");  // 记录日志

	if (_cta_mocker)  // 如果使用CTA模拟器
		_cta_mocker->step_calc();  // 执行最后一次计算步骤

	if (_hft_mocker)  // 如果使用HFT模拟器
		_hft_mocker->step_tick();  // 执行最后一次Tick步骤

	WTSLogger::debug("Last round ended");  // 记录日志

	if (_worker)  // 如果工作线程存在
	{
		_worker->join();  // 等待工作线程结束
		_worker.reset();  // 释放工作线程
	}

	WTSLogger::freeAllDynLoggers();  // 释放所有动态日志记录器

	WTSLogger::debug("Backtest stopped");  // 记录停止日志
}

/**
 * @brief 释放回测运行器
 * 
 * 停止日志系统，清理资源
 */
void WtBtRunner::release()
{
	WTSLogger::stop();  // 停止日志系统
}

/**
 * @brief 设置回测时间范围
 * 
 * 手动设置回测的起始时间和结束时间
 * 
 * @param stime 起始时间（时间戳）
 * @param etime 结束时间（时间戳）
 */
void WtBtRunner::set_time_range(WtUInt64 stime, WtUInt64 etime)
{
	_replayer.set_time_range(stime, etime);  // 设置历史数据回放器的时间范围

	WTSLogger::info("Backtest time range is set to be [{},{}] mannually", stime, etime);  // 记录日志
}

/**
 * @brief 启用或禁用Tick数据回放
 * 
 * 控制是否回放Tick级别的数据
 * 
 * @param bEnabled 是否启用（true表示启用，false表示禁用）
 */
void WtBtRunner::enable_tick(bool bEnabled /* = true */)
{
	_replayer.enable_tick(bEnabled);  // 设置历史数据回放器的Tick回放标志

	WTSLogger::info("Tick data replaying is {}", bEnabled ? "enabled" : "disabled");  // 记录日志
}

/**
 * @brief 清除缓存
 * 
 * 清除历史数据回放器的缓存数据
 */
void WtBtRunner::clear_cache()
{
	_replayer.clear_cache();  // 清除历史数据回放器的缓存
}

/**
 * @brief 获取原始合约代码
 * 
 * 将标准合约代码转换为原始合约代码
 * 
 * @param stdCode 标准合约代码
 * @return 返回原始合约代码字符串（线程局部存储）
 */
const char* WtBtRunner::get_raw_stdcode(const char* stdCode)
{
	static thread_local std::string s;  // 线程局部存储的字符串
	s = _replayer.get_rawcode(stdCode);  // 获取原始合约代码
	return s.c_str();  // 返回C字符串
}

/**
 * @brief 日志标签数组
 * 
 * 定义所有日志级别的标签字符串
 */
const char* LOG_TAGS[] = {
	"all",    // 所有日志
	"debug",  // 调试日志
	"info",   // 信息日志
	"warn",   // 警告日志
	"error",  // 错误日志
	"fatal",  // 致命错误日志
	"none",   // 无日志
};

/**
 * @brief 初始化事件推送器
 * 
 * 根据配置信息初始化事件推送器
 * 
 * @param cfg 事件推送器配置（WTSVariant对象）
 * @return 如果初始化成功返回true，否则返回false
 */
bool WtBtRunner::initEvtNotifier(WTSVariant* cfg)
{
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)  // 如果配置无效或类型不正确
		return false;  // 返回失败

	_notifier.init(cfg);  // 初始化事件推送器

	return true;  // 返回成功
}