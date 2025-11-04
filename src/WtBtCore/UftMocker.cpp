/*!
 * \file UftMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT极速策略回测模拟器实现文件
 *
 * 本文件实现了UftMocker类的所有功能，包括：
 * 1. 构造函数和析构函数：初始化回测模拟器，清理资源
 * 2. 数据接收处理：处理Tick数据、委托队列、委托明细、逐笔成交、K线数据
 * 3. 订单处理：接收策略订单，模拟撮合成交
 * 4. 持仓管理：执行开仓、加仓、减仓、平仓操作，维护多空双向持仓明细
 * 5. 盈亏计算：实时计算持仓盈亏、已平仓盈亏、动态盈亏
 * 6. 数据输出：输出交易记录、持仓记录、资金曲线等CSV文件
 * 7. 策略接口：实现IUftStraCtx接口，为策略提供数据查询和交易接口
 */
#include "UftMocker.h"
#include "WtHelper.h"

#include <stdarg.h>

#include <boost/filesystem.hpp>

#include "../Includes/WTSVariant.hpp"
#include "../Includes/WTSContractInfo.hpp"
#include "../Share/decimal.h"
#include "../Share/TimeUtils.hpp"
#include "../Share/StrUtil.hpp"

#include "../WTSTools/WTSLogger.h"

extern uint32_t makeLocalOrderID();  // 外部函数：生成本地订单ID

/**
 * @brief 开平标志名称数组
 * 
 * 用于将开平标志（0-开仓，1-平仓，2-平今）转换为字符串。
 */
const char* OFFSET_NAMES[] =
{
	"OPEN",    // 0 - 开仓
	"CLOSE",   // 1 - 平仓
	"CLOSET"   // 2 - 平今
};

extern std::vector<uint32_t> splitVolume(uint32_t vol);  // 外部函数：拆分数量（整数）
extern std::vector<double> splitVolume(double vol, double minQty = 1.0, double maxQty = 100.0, double qtyTick = 1.0);  // 外部函数：拆分数量（浮点数）

extern uint32_t genRand(uint32_t maxVal = 10000);  // 外部函数：生成随机数

/**
 * @brief 生成UFT上下文ID
 * @return 上下文唯一标识符
 * 
 * 使用原子操作生成唯一的UFT策略上下文ID，起始值为7000。
 */
inline uint32_t makeUftCtxId()
{
	static std::atomic<uint32_t> _auto_context_id{ 7000 };  // 静态原子变量，起始值为7000
	return _auto_context_id.fetch_add(1);  // 原子递增并返回旧值
}

/**
 * @brief 构造函数
 * @param replayer 历史数据回放器指针，用于获取历史数据和合约信息
 * @param name 策略名称，用于标识和日志记录
 * 
 * 初始化UFT策略回测模拟器，设置回放器、策略名称，并生成上下文ID。
 */
UftMocker::UftMocker(HisDataReplayer* replayer, const char* name)
	: IUftStraCtx(name)  // 调用基类构造函数
	, _replayer(replayer)  // 设置历史数据回放器
	, _strategy(NULL)  // 初始化策略指针为空
	, _use_newpx(false)  // 初始化不使用最新价撮合
	, _error_rate(0)  // 初始化错误率为0
	, _match_this_tick(false)  // 初始化不在当前tick撮合
{
	_context_id = makeUftCtxId();  // 生成上下文ID
}


/**
 * @brief 析构函数
 * 
 * 清理UFT策略回测模拟器占用的资源，删除策略实例。
 */
UftMocker::~UftMocker()
{
	if(_strategy)  // 如果策略存在
	{
		_factory._fact->deleteStrategy(_strategy);  // 删除策略实例
	}
}

/**
 * @brief 处理任务队列
 * 
 * 从任务队列中取出任务并执行。
 * 使用互斥锁保护任务队列的访问。
 */
void UftMocker::procTask()
{
	if (_tasks.empty())  // 如果任务队列为空
	{
		return;  // 直接返回
	}

	_mtx_control.lock();  // 加锁保护任务队列

	while (!_tasks.empty())  // 遍历任务队列
	{
		Task& task = _tasks.front();  // 获取第一个任务

		task();  // 执行任务

		{
			std::unique_lock<std::mutex> lck(_mtx);  // 加锁保护任务队列操作
			_tasks.pop();  // 删除已执行的任务
		}
	}

	_mtx_control.unlock();  // 解锁
}

/**
 * @brief 提交任务到任务队列
 * @param task 任务函数
 * 
 * 将任务添加到任务队列中，等待执行。
 */
void UftMocker::postTask(Task task)
{
	{
		std::unique_lock<std::mutex> lck(_mtx);  // 加锁保护任务队列
		_tasks.push(task);  // 添加任务到队列
		return;  // 返回
	}

	//if(_thrd == NULL)
	//{
	//	_thrd.reset(new std::thread([this](){
	//		while (!_stopped)
	//		{
	//			if(_tasks.empty())
	//			{
	//				std::this_thread::sleep_for(std::chrono::milliseconds(1));
	//				continue;
	//			}

	//			_mtx_control.lock();

	//			while(!_tasks.empty())
	//			{
	//				Task& task = _tasks.front();

	//				task();

	//				{
	//					std::unique_lock<std::mutex> lck(_mtx);
	//					_tasks.pop();
	//				}
	//			}

	//			_mtx_control.unlock();
	//		}
	//	}));
	//}
}

/**
 * @brief 初始化UFT策略工厂
 * @param cfg 配置参数，包含策略模块路径和策略参数
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中加载策略动态库，创建策略工厂和策略实例，并初始化策略。
 * 同时从配置中读取撮合参数：use_newpx（是否使用最新价撮合）、error_rate（错误率）、match_this_tick（是否在当前tick撮合）。
 */
bool UftMocker::init_uft_factory(WTSVariant* cfg)
{
	if (cfg == NULL)  // 如果配置为空，返回false
		return false;

	const char* module = cfg->getCString("module");  // 获取策略模块路径
	
	_use_newpx = cfg->getBoolean("use_newpx");  // 获取是否使用最新价撮合
	_error_rate = cfg->getUInt32("error_rate");  // 获取错误率（万分之一）
	_match_this_tick = cfg->getBoolean("match_this_tick");  // 获取是否在当前tick撮合

	log_info("UFT match params: use_newpx-{}, error_rate-{}, match_this_tick-{}", _use_newpx, _error_rate, _match_this_tick);  // 记录日志

	DllHandle hInst = DLLHelper::load_library(module);  // 加载动态库
	if (hInst == NULL)  // 如果加载失败，返回false
		return false;

	FuncCreateUftStraFact creator = (FuncCreateUftStraFact)DLLHelper::get_symbol(hInst, "createStrategyFact");  // 获取创建工厂函数
	if (creator == NULL)  // 如果获取失败，释放动态库并返回false
	{
		DLLHelper::free_library(hInst);  // 释放动态库
		return false;
	}

	_factory._module_inst = hInst;  // 保存动态库句柄
	_factory._module_path = module;  // 保存模块路径
	_factory._creator = creator;  // 保存创建函数指针
	_factory._remover = (FuncDeleteUftStraFact)DLLHelper::get_symbol(hInst, "deleteStrategyFact");  // 获取删除工厂函数
	_factory._fact = _factory._creator();  // 创建策略工厂

	WTSVariant* cfgStra = cfg->get("strategy");  // 获取策略配置
	if(cfgStra)  // 如果策略配置存在
	{
		_strategy = _factory._fact->createStrategy(cfgStra->getCString("name"), "uft");  // 创建策略实例
		_strategy->init(cfgStra->get("params"));  // 初始化策略
	}
	return true;  // 返回成功
}

/**
 * @brief 处理Tick数据（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param curTick 当前Tick数据指针
 * @param pxType 价格类型：0-最新价，1-买入价，2-卖出价，3-收盘价模拟
 * 
 * 接收历史数据回放器推送的Tick数据，调用on_tick处理。
 */
void UftMocker::handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType)
{
	on_tick(stdCode, curTick);  // 调用on_tick处理
}

/**
 * @brief 处理委托明细数据（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param curOrdDtl 当前委托明细数据指针
 * 
 * 接收历史数据回放器推送的委托明细数据，调用on_order_detail处理。
 */
void UftMocker::handle_order_detail(const char* stdCode, WTSOrdDtlData* curOrdDtl)
{
	on_order_detail(stdCode, curOrdDtl);  // 调用on_order_detail处理
}

/**
 * @brief 处理委托队列数据（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param curOrdQue 当前委托队列数据指针
 * 
 * 接收历史数据回放器推送的委托队列数据，调用on_order_queue处理。
 */
void UftMocker::handle_order_queue(const char* stdCode, WTSOrdQueData* curOrdQue)
{
	on_order_queue(stdCode, curOrdQue);  // 调用on_order_queue处理
}

/**
 * @brief 处理逐笔成交数据（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param curTrans 当前逐笔成交数据指针
 * 
 * 接收历史数据回放器推送的逐笔成交数据，调用on_transaction处理。
 */
void UftMocker::handle_transaction(const char* stdCode, WTSTransData* curTrans)
{
	on_transaction(stdCode, curTrans);  // 调用on_transaction处理
}

/**
 * @brief 处理K线闭合事件（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param period K线周期
 * @param times K线倍数
 * @param newBar 新的K线数据指针
 * 
 * 接收K线闭合事件，调用on_bar处理。
 */
void UftMocker::handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	on_bar(stdCode, period, times, newBar);  // 调用on_bar处理
}

/**
 * @brief 处理初始化事件（IDataSink接口）
 * 
 * 回测开始时调用，触发策略的on_init回调，并通知交易通道就绪。
 */
void UftMocker::handle_init()
{
	on_init();  // 调用初始化回调
	on_channel_ready();  // 通知交易通道就绪
}

/**
 * @brief 处理定时调度事件（IDataSink接口）
 * @param uDate 当前日期
 * @param uTime 当前时间（分钟级别）
 * 
 * 接收定时调度事件。注意：当前实现中未启用定时调度回调。
 */
void UftMocker::handle_schedule(uint32_t uDate, uint32_t uTime)
{
	//on_schedule(uDate, uTime);  // 定时调度回调（已注释）
}

/**
 * @brief 处理交易日开始事件（IDataSink接口）
 * @param curTDate 当前交易日日期
 * 
 * 交易日开始时调用，处理T+1规则的持仓转换，并触发策略的on_session_begin回调。
 */
void UftMocker::handle_session_begin(uint32_t curTDate)
{
	on_session_begin(curTDate);  // 调用交易日开始回调
}

/**
 * @brief 处理交易日结束事件（IDataSink接口）
 * @param curTDate 当前交易日日期
 * 
 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
 */
void UftMocker::handle_session_end(uint32_t curTDate)
{
	on_session_end(curTDate);  // 调用交易日结束回调
}

/**
 * @brief 处理回测完成事件（IDataSink接口）
 * 
 * 回测结束时调用，输出回测结果文件，并触发策略的on_bactest_end回调。
 */
void UftMocker::handle_replay_done()
{
	dump_outputs();  // 输出回测结果文件

	this->on_bactest_end();  // 调用回测结束回调
}

/**
 * @brief K线数据回调
 * @param stdCode 标准化合约代码
 * @param period K线周期
 * @param times K线倍数
 * @param newBar 新的K线数据指针
 * 
 * 接收K线数据，触发策略的on_bar回调。
 */
void UftMocker::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_bar(this, stdCode, period, times, newBar);  // 调用策略的K线回调
}

/**
 * @brief Tick数据回调
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 接收Tick数据，更新价格缓存，更新动态盈亏，并根据_match_this_tick标志决定订单撮合和策略回调的顺序。
 * 如果开启了同tick撮合，则先触发策略的ontick，再处理订单撮合。
 * 如果没开启同tick撮合，则先处理订单撮合，再触发策略的ontick。
 */
void UftMocker::on_tick(const char* stdCode, WTSTickData* newTick)
{
	_price_map[stdCode] = newTick->price();  // 更新价格缓存
	{
		std::unique_lock<std::recursive_mutex> lck(_mtx_control);  // 加锁（用于同步控制）
	}

	update_dyn_profit(stdCode, newTick);  // 更新动态盈亏
	
	//如果开启了同tick撮合，则先触发策略的ontick，再处理订单
	//如果没开启同tick撮合，则先处理订单，再触发策略的ontick
	if(_match_this_tick)  // 如果开启了同tick撮合
	{
		on_tick_updated(stdCode, newTick);  // 先触发策略的Tick回调

		procTask();  // 处理任务队列

		if (!_orders.empty())  // 如果订单队列不为空
		{
			OrderIDs ids;  // 订单ID列表
			// 收集所有订单ID
			for (auto it = _orders.begin(); it != _orders.end(); it++)
			{
				uint32_t localid = it->first;  // 获取本地订单ID
				ids.emplace_back(localid);  // 添加到列表
			}

			OrderIDs to_erase;  // 需要删除的订单ID列表
			// 处理所有订单
			for (uint32_t localid : ids)
			{
				bool bNeedErase = procOrder(localid);  // 处理订单撮合
				if (bNeedErase)  // 如果订单已完成
					to_erase.emplace_back(localid);  // 添加到删除列表
			}

			// 删除已完成的订单
			for (uint32_t localid : to_erase)
			{
				auto it = _orders.find(localid);  // 查找订单
				_orders.erase(it);  // 删除订单
			}
		}
	}
	else  // 如果没开启同tick撮合
	{
		if (!_orders.empty())  // 如果订单队列不为空
		{
			OrderIDs ids;  // 订单ID列表
			// 先处理订单撮合
			for (auto it = _orders.begin(); it != _orders.end(); it++)
			{
				uint32_t localid = it->first;  // 获取本地订单ID
				bool bNeedErase = procOrder(localid);  // 处理订单撮合
				if (bNeedErase)  // 如果订单已完成
					ids.emplace_back(localid);  // 添加到删除列表
			}

			// 删除已完成的订单
			for (uint32_t localid : ids)
			{
				auto it = _orders.find(localid);  // 查找订单
				_orders.erase(it);  // 删除订单
			}
		}

		on_tick_updated(stdCode, newTick);  // 再触发策略的Tick回调

		procTask();  // 处理任务队列
	}
}

/**
 * @brief Tick数据更新回调
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 当订阅的合约有新的Tick数据时调用，触发策略的on_tick回调。
 */
void UftMocker::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	auto it = _tick_subs.find(stdCode);  // 查找订阅列表
	if (it == _tick_subs.end())  // 如果没有订阅
		return;

	if (_strategy)  // 如果策略存在
		_strategy->on_tick(this, stdCode, newTick);  // 调用策略的Tick回调
}

/**
 * @brief 委托队列数据回调
 * @param stdCode 标准化合约代码
 * @param newOrdQue 新的委托队列数据指针
 * 
 * 接收委托队列数据，触发委托队列数据更新回调。
 */
void UftMocker::on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	on_ordque_updated(stdCode, newOrdQue);  // 调用委托队列数据更新回调
}

/**
 * @brief 委托队列数据更新回调
 * @param stdCode 标准化合约代码
 * @param newOrdQue 新的委托队列数据指针
 * 
 * 当订阅的合约有新的委托队列数据时调用，触发策略的on_order_queue回调。
 */
void UftMocker::on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_order_queue(this, stdCode, newOrdQue);  // 调用策略的委托队列回调
}

/**
 * @brief 委托明细数据回调
 * @param stdCode 标准化合约代码
 * @param newOrdDtl 新的委托明细数据指针
 * 
 * 接收委托明细数据，触发委托明细数据更新回调。
 */
void UftMocker::on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	on_orddtl_updated(stdCode, newOrdDtl);  // 调用委托明细数据更新回调
}

/**
 * @brief 委托明细数据更新回调
 * @param stdCode 标准化合约代码
 * @param newOrdDtl 新的委托明细数据指针
 * 
 * 当订阅的合约有新的委托明细数据时调用，触发策略的on_order_detail回调。
 */
void UftMocker::on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_order_detail(this, stdCode, newOrdDtl);  // 调用策略的委托明细回调
}

/**
 * @brief 逐笔成交数据回调
 * @param stdCode 标准化合约代码
 * @param newTrans 新的逐笔成交数据指针
 * 
 * 接收逐笔成交数据，触发逐笔成交数据更新回调。
 */
void UftMocker::on_transaction(const char* stdCode, WTSTransData* newTrans)
{
	on_trans_updated(stdCode, newTrans);  // 调用逐笔成交数据更新回调
}

/**
 * @brief 逐笔成交数据更新回调
 * @param stdCode 标准化合约代码
 * @param newTrans 新的逐笔成交数据指针
 * 
 * 当订阅的合约有新的逐笔成交数据时调用，触发策略的on_transaction回调。
 */
void UftMocker::on_trans_updated(const char* stdCode, WTSTransData* newTrans)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_transaction(this, stdCode, newTrans);  // 调用策略的逐笔成交回调
}

/**
 * @brief 获取上下文ID
 * @return 上下文唯一标识符
 * 
 * 返回策略上下文的唯一标识符，用于区分不同的策略实例。
 */
uint32_t UftMocker::id()
{
	return _context_id;  // 返回上下文ID
}

/**
 * @brief 初始化完成回调
 * 
 * 策略初始化完成后调用，触发策略的on_init回调。
 */
void UftMocker::on_init()
{
	if (_strategy)  // 如果策略存在
		_strategy->on_init(this);  // 调用策略的初始化回调
}

/**
 * @brief 交易日开始回调
 * @param curTDate 当前交易日日期
 * 
 * 交易日开始时调用，处理T+1规则的持仓转换（将今仓转换为昨仓），并触发策略的on_session_begin回调。
 */
void UftMocker::on_session_begin(uint32_t curTDate)
{
	//每个交易日开始，要把冻结持仓置零
	for (auto& it : _pos_map)  // 遍历所有持仓
	{
		const char* stdCode = it.first.c_str();  // 获取合约代码
		PosItem& pInfo = (PosItem&)it.second;   // 获取持仓信息（注意：这里应该是PosInfo，需要修正）
		if (!decimal::eq(pInfo.frozen(), 0))    // 如果有冻结持仓
		{
			log_debug("{} frozen of {} released on {}", pInfo.frozen(), stdCode, curTDate);  // 记录日志
			pInfo._prevol += pInfo._newvol;     // 将今仓数量加到昨仓
			pInfo._preavail = pInfo._prevol;    // 昨仓可用数量等于昨仓数量

			pInfo._newvol = 0;     // 今仓数量清零
			pInfo._newavail = 0;   // 今仓可用数量清零
		}
	}

	_strategy->on_session_begin(this, curTDate);  // 调用策略的交易日开始回调
}

/**
 * @brief 交易日结束回调
 * @param curTDate 当前交易日日期
 * 
 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
 */
void UftMocker::on_session_end(uint32_t curTDate)
{
	_strategy->on_session_end(this, curTDate);  // 调用策略的交易日结束回调

	uint32_t curDate = curTDate;  // 当前日期

	double total_profit = 0;      // 总已平仓盈亏
	double total_dynprofit = 0;   // 总动态盈亏

	// 遍历所有持仓
	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码
		const PosInfo& pInfo = it->second;        // 获取持仓信息
		total_profit += pInfo.closeprofit();      // 累加已平仓盈亏
		total_dynprofit += pInfo.dynprofit();     // 累加动态盈亏

		if (!decimal::eq(pInfo._long.volume(), 0.0))  // 如果多头持仓不为0
			_pos_logs << fmt::format("{},{},LONG,{},{:.2f},{:.2f}\n", curTDate, stdCode, pInfo._long.volume(), pInfo._long._closeprofit, pInfo._long._dynprofit);  // 记录多头持仓日志

		if (!decimal::eq(pInfo._short.volume(), 0.0))  // 如果空头持仓不为0
			_pos_logs << fmt::format("{},{},SHORT,{},{:.2f},{:.2f}\n", curTDate, stdCode, pInfo._short.volume(), pInfo._short._closeprofit, pInfo._short._dynprofit);  // 记录空头持仓日志
	}

	_fund_logs << fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curDate,
		_fund_info._total_profit, _fund_info._total_dynprofit,
		_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees);  // 记录资金日志
}

/**
 * @brief 获取未成交数量
 * @param stdCode 标准化合约代码
 * @return 未成交数量，正数为买入未成交，负数为卖出未成交
 * 
 * 返回指定合约的未成交订单数量总和。
 */
double UftMocker::stra_get_undone(const char* stdCode)
{
	double ret = 0;  // 未成交数量
	// 遍历所有订单
	for (auto it = _orders.begin(); it != _orders.end(); it++)
	{
		const OrderInfo& ordInfo = it->second;  // 获取订单信息
		if (strcmp(ordInfo._code, stdCode) == 0)  // 如果订单合约代码匹配
		{
			ret += ordInfo._left * (ordInfo._isLong ? 1 : -1);  // 累加剩余数量（买入为正，卖出为负）
		}
	}

	return ret;  // 返回未成交数量
}

/**
 * @brief 撤销指定订单
 * @param localid 本地订单ID
 * @return 撤销成功返回true，失败返回false
 * 
 * 撤销指定本地订单ID的订单。订单会被标记为已撤销，并释放冻结持仓。
 * 撤销操作通过任务队列异步执行。
 */
bool UftMocker::stra_cancel(uint32_t localid)
{
	postTask([this, localid](){  // 提交撤销任务到任务队列
		auto it = _orders.find(localid);  // 查找订单
		if (it == _orders.end())  // 如果订单不存在
			return;  // 直接返回

		StdLocker<StdRecurMutex> lock(_mtx_ords);  // 加锁保护订单映射表
		OrderInfo& ordInfo = (OrderInfo&)it->second;  // 获取订单信息
		
		if (ordInfo._offset != 0)  // 如果是平仓订单（offset不为0表示平仓）
		{
			PosInfo& pInfo = _pos_map[ordInfo._code];  // 获取持仓信息
			PosItem& pItem = ordInfo._isLong ? pInfo._long : pInfo._short;  // 获取对应方向的持仓
			WTSCommodityInfo* commInfo = _replayer->get_commodity_info(ordInfo._code);  // 获取合约信息
			if(commInfo->getCoverMode() == CM_CoverToday)  // 如果支持平今仓
			{
				if (ordInfo._offset == 2)  // 如果是平今仓
					pItem._newavail += ordInfo._left;  // 释放今仓可用数量
				else  // 如果是平昨仓
					pItem._preavail += ordInfo._left;  // 释放昨仓可用数量
			}
			else  // 如果不分平昨平今
			{
				//如果不分平昨平今，则先释放今仓
				double maxQty = std::min(ordInfo._left, pItem._newvol - pItem._newavail);  // 计算可释放的今仓数量
				pItem._newavail += maxQty;  // 释放今仓可用数量
				pItem._preavail += ordInfo._left - maxQty;  // 释放昨仓可用数量（剩余部分）
			}
		}

		log_debug("Order {} canceled, action: {} {} @ {}({})", ordInfo._localid, OFFSET_NAMES[ordInfo._offset], ordInfo._isLong?"long":"short", ordInfo._total, ordInfo._left);  // 记录日志
		ordInfo._left = 0;  // 设置剩余数量为0
		on_order(localid, ordInfo._code, ordInfo._isLong, ordInfo._offset, ordInfo._total, ordInfo._left, ordInfo._price, true);  // 触发订单状态回调（已撤销）
		_orders.erase(it);  // 删除订单
	});

	return true;  // 返回成功
}

/**
 * @brief 撤销指定合约的所有订单
 * @param stdCode 标准化合约代码
 * @return 被撤销的订单ID列表
 * 
 * 撤销指定合约的所有未成交订单。
 */
OrderIDs UftMocker::stra_cancel_all(const char* stdCode)
{
	OrderIDs ret;  // 订单ID列表
	uint32_t cnt = 0;  // 计数（未使用）
	// 遍历所有订单
	for (auto it = _orders.begin(); it != _orders.end(); it++)
	{
		const OrderInfo& ordInfo = it->second;  // 获取订单信息
		if(strcmp(ordInfo._code, stdCode) == 0)  // 如果订单合约代码匹配
		{
			stra_cancel(it->first);  // 撤销订单
		}
	}

	return ret;  // 返回订单ID列表（注意：当前实现中ret始终为空，可能需要修复）
}

/**
 * @brief 买入（智能处理平空和开多）
 * @param stdCode 标准化合约代码
 * @param price 委托价格
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
 * @return 订单ID列表
 * 
 * 智能买入：如果有多头持仓，先平空；如果还有剩余数量，则开多。
 * 根据合约的平仓模式（是否支持平今仓）决定平仓顺序。
 */
OrderIDs UftMocker::stra_buy(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return OrderIDs();  // 返回空列表
	}

	if (decimal::le(qty, 0))  // 如果数量小于等于0
	{
		log_error("Entrust error: qty {} <= 0", qty);  // 记录错误日志
		return OrderIDs();  // 返回空列表
	}

	OrderIDs ids;  // 订单ID列表
	const PosInfo& pInfo = _pos_map[stdCode];  // 获取持仓信息

	double left = qty;  // 剩余数量
	//先检查空头
	const PosItem& pItem = pInfo._short;  // 获取空头持仓
	if(decimal::gt(pItem.valid(), 0.0))  // 如果空头持仓可用数量大于0
	{
		if(commInfo->getCoverMode() != CM_CoverToday)  // 如果不分平昨平今
		{
			double maxQty = std::min(left, pItem.valid());  // 计算可平仓数量
			if (decimal::gt(maxQty, 0.0))  // 如果可平仓数量大于0
			{
				uint32_t localid = stra_exit_short(stdCode, price, maxQty, false, 0);  // 平空仓
				if (localid != 0) ids.emplace_back(localid);  // 添加到订单ID列表
			}
			left -= maxQty;  // 减少剩余数量
		}
		else  // 如果支持平今仓
		{
			double maxQty = std::min(left, pItem._preavail);  // 计算可平昨仓数量
			if (decimal::gt(maxQty, 0.0))  // 如果可平昨仓数量大于0
			{
				uint32_t localid = stra_exit_short(stdCode, price, maxQty, false, 0);  // 平昨空仓
				if (localid != 0) ids.emplace_back(localid);  // 添加到订单ID列表
			}
			left -= maxQty;  // 减少剩余数量

			maxQty = std::min(left, pItem._newavail);  // 计算可平今仓数量
			if (decimal::gt(maxQty, 0.0))  // 如果可平今仓数量大于0
			{
				uint32_t localid = stra_exit_short(stdCode, price, maxQty, true, 0);  // 平今空仓
				if (localid != 0) ids.emplace_back(localid);  // 添加到订单ID列表
			}
			left -= maxQty;  // 减少剩余数量
		}
	}

	//还有剩余则开仓
	if(decimal::gt(left, 0.0))  // 如果还有剩余数量
	{
		ids.emplace_back(stra_enter_long(stdCode, price, left));  // 开多仓
	}

	return ids;  // 返回订单ID列表
}

/**
 * @brief 卖出（智能处理平多和开空）
 * @param stdCode 标准化合约代码
 * @param price 委托价格
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
 * @return 订单ID列表
 * 
 * 智能卖出：如果有多头持仓，先平多；如果还有剩余数量，则开空。
 * 根据合约的平仓模式（是否支持平今仓）决定平仓顺序。
 */
OrderIDs UftMocker::stra_sell(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return OrderIDs();  // 返回空列表
	}

	if (decimal::le(qty, 0))  // 如果数量小于等于0
	{
		log_error("Entrust error: qty {} <= 0", qty);  // 记录错误日志
		return OrderIDs();  // 返回空列表
	}

	OrderIDs ids;  // 订单ID列表
	const PosInfo& pInfo = _pos_map[stdCode];  // 获取持仓信息

	double left = qty;  // 剩余数量
	//先检查多头
	const PosItem& pItem = pInfo._long;  // 获取多头持仓
	if (decimal::gt(pItem.valid(), 0.0))  // 如果多头持仓可用数量大于0
	{
		if (commInfo->getCoverMode() != CM_CoverToday)  // 如果不分平昨平今
		{
			double maxQty = std::min(left, pItem.valid());  // 计算可平仓数量
			if (decimal::gt(maxQty, 0.0))  // 如果可平仓数量大于0
			{
				uint32_t localid = stra_exit_long(stdCode, price, maxQty, false, 0);  // 平多仓
				if (localid != 0) ids.emplace_back(localid);  // 添加到订单ID列表
			}

			left -= maxQty;  // 减少剩余数量
		}
		else  // 如果支持平今仓
		{
			double maxQty = std::min(left, pItem._preavail);  // 计算可平昨仓数量
			if (decimal::gt(maxQty, 0.0))  // 如果可平昨仓数量大于0
			{
				uint32_t localid = stra_exit_long(stdCode, price, maxQty, false, 0);  // 平昨多仓
				if (localid != 0) ids.emplace_back(localid);  // 添加到订单ID列表
			}
			left -= maxQty;  // 减少剩余数量

			maxQty = std::min(left, pItem._newavail);  // 计算可平今仓数量
			if (decimal::gt(maxQty, 0.0))  // 如果可平今仓数量大于0
			{
				uint32_t localid = stra_exit_long(stdCode, price, maxQty, true, 0);  // 平今多仓
				if (localid != 0) ids.emplace_back(localid);  // 添加到订单ID列表
			}
			left -= maxQty;  // 减少剩余数量
		}
	}

	//还有剩余则开仓
	if (decimal::gt(left, 0.0))  // 如果还有剩余数量
	{
		ids.emplace_back(stra_enter_short(stdCode, price, left));  // 开空仓
	}

	return ids;  // 返回订单ID列表
}

/**
 * @brief 开多
 * @param stdCode 标准化合约代码
 * @param price 委托价格
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
 * @return 本地订单ID，失败返回0
 * 
 * 开多仓订单。订单会被添加到订单队列中，等待撮合成交。
 * 订单提交后通过任务队列异步触发委托回调。
 */
uint32_t UftMocker::stra_enter_long(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return 0;  // 返回0
	}

	if (decimal::le(qty, 0))  // 如果数量小于等于0
	{
		log_error("Entrust error: qty {} <= 0", qty);  // 记录错误日志
		return 0;  // 返回0
	}

	uint32_t localid = makeLocalOrderID();  // 生成本地订单ID

	OrderInfo order;  // 创建订单信息
	order._localid = localid;  // 设置本地订单ID
	strcpy(order._code, stdCode);  // 设置合约代码
	order._isLong = true;  // 设置为做多
	order._offset = 0;  // 设置为开仓
	order._price = price;  // 设置委托价格
	order._total = qty;  // 设置总数量
	order._left = qty;  // 设置剩余数量

	{
		_mtx_ords.lock();  // 加锁保护订单映射表
		_orders[localid] = order;  // 添加到订单映射表
		_mtx_ords.unlock();  // 解锁
	}

	postTask([this, localid]() {  // 提交任务到任务队列
		const OrderInfo& ordInfo = _orders[localid];  // 获取订单信息
		log_debug("order placed: open long of {} @ {} by {}", ordInfo._code, ordInfo._price, ordInfo._total);  // 记录日志
		on_entrust(localid, ordInfo._code, true, "entrust success");  // 触发委托回调
	});

	return localid;  // 返回本地订单ID
}

/**
 * @brief 开空
 * @param stdCode 标准化合约代码
 * @param price 委托价格
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
 * @return 本地订单ID，失败返回0
 * 
 * 开空仓订单。订单会被添加到订单队列中，等待撮合成交。
 * 订单提交后通过任务队列异步触发委托回调。
 */
uint32_t UftMocker::stra_enter_short(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return 0;  // 返回0
	}

	if (decimal::le(qty, 0))  // 如果数量小于等于0
	{
		log_error("Entrust error: qty {} <= 0", qty);  // 记录错误日志
		return 0;  // 返回0
	}

	uint32_t localid = makeLocalOrderID();  // 生成本地订单ID

	OrderInfo order;  // 创建订单信息
	order._localid = localid;  // 设置本地订单ID
	strcpy(order._code, stdCode);  // 设置合约代码
	order._isLong = false;  // 设置为做空
	order._offset = 0;  // 设置为开仓
	order._price = price;  // 设置委托价格
	order._total = qty;  // 设置总数量
	order._left = qty;  // 设置剩余数量

	{
		_mtx_ords.lock();  // 加锁保护订单映射表
		_orders[localid] = order;  // 添加到订单映射表
		_mtx_ords.unlock();  // 解锁
	}

	postTask([this, localid]() {  // 提交任务到任务队列
		const OrderInfo& ordInfo = _orders[localid];  // 获取订单信息
		log_debug("order placed: open short of {} @ {} by {}", ordInfo._code, ordInfo._price, ordInfo._total);  // 记录日志
		on_entrust(localid, ordInfo._code, true, "entrust success");  // 触发委托回调
	});

	return localid;  // 返回本地订单ID
}

/**
 * @brief 平多
 * @param stdCode 标准化合约代码
 * @param price 委托价格
 * @param qty 下单数量
 * @param isToday 是否今仓，SHFE、INE专用
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
 * @return 本地订单ID，失败返回0
 * 
 * 平多仓订单。如果是SHFE、INE等需要区分今昨仓的市场，可以通过isToday参数指定平今仓或平昨仓。
 * 订单提交后通过任务队列异步触发委托回调。
 */
uint32_t UftMocker::stra_exit_long(const char* stdCode, double price, double qty, bool isToday /* = false */, int flag /* = 0 */)
{
	PosInfo& pInfo = _pos_map[stdCode];  // 获取持仓信息
	PosItem& pItem = pInfo._long;  // 获取多头持仓
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	uint32_t offset = 1;  // 开平标志，默认为平仓（1）
	if(commInfo->getCoverMode() != CM_CoverToday)  // 如果不分平昨平今
	{
		if(decimal::lt(pItem.valid(), qty))  // 如果可用持仓小于平仓数量
		{
			log_error("Entrust error: no enough available position");  // 记录错误日志
			return 0;  // 返回0
		}

		double maxQty = std::min(qty, pItem._preavail);  // 计算可平昨仓数量
		pItem._preavail -= maxQty;  // 减少昨仓可用数量
		pItem._newavail -= qty - maxQty;  // 减少今仓可用数量（剩余部分）
	}
	else  // 如果支持平今仓
	{
		if (isToday) offset = 2;  // 如果是平今仓，设置标志为2

		double valid = isToday ? pItem._newavail : pItem._preavail;  // 获取可用持仓
		if (decimal::lt(valid, qty))  // 如果可用持仓小于平仓数量
		{
			log_error("Entrust error: no enough available {} position", isToday?"new":"old");  // 记录错误日志
			return 0;  // 返回0
		}

		if (isToday)  // 如果是平今仓
			pItem._newavail -= qty;  // 减少今仓可用数量
		else  // 如果是平昨仓
			pItem._preavail -= qty;  // 减少昨仓可用数量
	}

	uint32_t localid = makeLocalOrderID();  // 生成本地订单ID

	OrderInfo order;  // 创建订单信息
	order._localid = localid;  // 设置本地订单ID
	strcpy(order._code, stdCode);  // 设置合约代码
	order._isLong = true;  // 设置为做多
	order._offset = offset;  // 设置开平标志
	order._price = price;  // 设置委托价格
	order._total = qty;  // 设置总数量
	order._left = qty;  // 设置剩余数量

	{
		_mtx_ords.lock();  // 加锁保护订单映射表
		_orders[localid] = order;  // 添加到订单映射表
		_mtx_ords.unlock();  // 解锁
	}

	postTask([this, localid]() {  // 提交任务到任务队列
		const OrderInfo& ordInfo = _orders[localid];  // 获取订单信息
		log_debug("order placed: {} long of {} @ {} by {}", OFFSET_NAMES[ordInfo._offset], ordInfo._code, ordInfo._price, ordInfo._total);  // 记录日志
		on_entrust(localid, ordInfo._code, true, "entrust success");  // 触发委托回调
	});

	return localid;  // 返回本地订单ID
}

/**
 * @brief 平空
 * @param stdCode 标准化合约代码
 * @param price 委托价格
 * @param qty 下单数量
 * @param isToday 是否今仓，SHFE、INE专用
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
 * @return 本地订单ID，失败返回0
 * 
 * 平空仓订单。如果是SHFE、INE等需要区分今昨仓的市场，可以通过isToday参数指定平今仓或平昨仓。
 * 订单提交后通过任务队列异步触发委托回调。
 */
uint32_t UftMocker::stra_exit_short(const char* stdCode, double price, double qty, bool isToday /* = false */, int flag /* = 0 */)
{
	PosInfo& pInfo = _pos_map[stdCode];  // 获取持仓信息
	PosItem& pItem = pInfo._short;  // 获取空头持仓
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	uint32_t offset = 1;  // 开平标志，默认为平仓（1）
	if (commInfo->getCoverMode() != CM_CoverToday)  // 如果不分平昨平今
	{
		if (decimal::lt(pItem.valid(), qty))  // 如果可用持仓小于平仓数量
		{
			log_error("Entrust error: no enough available position");  // 记录错误日志
			return 0;  // 返回0
		}

		double maxQty = std::min(qty, pItem._preavail);  // 计算可平昨仓数量
		pItem._preavail -= maxQty;  // 减少昨仓可用数量
		pItem._newavail -= qty - maxQty;  // 减少今仓可用数量（剩余部分）
	}
	else  // 如果支持平今仓
	{
		if (isToday) offset = 2;  // 如果是平今仓，设置标志为2

		double valid = isToday ? pItem._newavail : pItem._preavail;  // 获取可用持仓
		if (decimal::lt(valid, qty))  // 如果可用持仓小于平仓数量
		{
			log_error("Entrust error: no enough available {} position", isToday ? "new" : "old");  // 记录错误日志
			return 0;  // 返回0
		}

		if (isToday)  // 如果是平今仓
			pItem._newavail -= qty;  // 减少今仓可用数量
		else  // 如果是平昨仓
			pItem._preavail -= qty;  // 减少昨仓可用数量
	}

	uint32_t localid = makeLocalOrderID();  // 生成本地订单ID

	OrderInfo order;  // 创建订单信息
	order._localid = localid;  // 设置本地订单ID
	strcpy(order._code, stdCode);  // 设置合约代码
	order._isLong = false;  // 设置为做空
	order._offset = offset;  // 设置开平标志
	order._price = price;  // 设置委托价格
	order._total = qty;  // 设置总数量
	order._left = qty;  // 设置剩余数量

	{
		_mtx_ords.lock();  // 加锁保护订单映射表
		_orders[localid] = order;  // 添加到订单映射表
		_mtx_ords.unlock();  // 解锁
	}

	postTask([this, localid]() {  // 提交任务到任务队列
		const OrderInfo& ordInfo = _orders[localid];  // 获取订单信息
		log_debug("order placed: {} short of {} @ {} by {}", OFFSET_NAMES[ordInfo._offset], ordInfo._code, ordInfo._price, ordInfo._total);  // 记录日志
		on_entrust(localid, ordInfo._code, true, "entrust success");  // 触发委托回调
	});

	return localid;  // 返回本地订单ID
}

/**
 * @brief 订单状态回调
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param offset 开平标志：0-开仓，1-平仓，2-平今
 * @param totalQty 总委托数量
 * @param leftQty 剩余数量
 * @param price 委托价格
 * @param isCanceled 是否已撤销
 * 
 * 订单状态变化时调用，触发策略的on_order回调。
 */
void UftMocker::on_order(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled)
{
	if(_strategy)  // 如果策略存在
		_strategy->on_order(this, localid, stdCode, isLong, offset, totalQty, leftQty, price, isCanceled);  // 调用策略的订单回调
}

/**
 * @brief 成交回调
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param offset 开平标志：0-开仓，1-平仓，2-平今
 * @param vol 成交数量
 * @param price 成交价格
 * 
 * 订单成交时调用，更新持仓信息，并触发策略的on_trade回调。
 */
void UftMocker::on_trade(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double vol, double price)
{
	//PosInfo& posInfo = _pos_map[stdCode];  // 获取持仓信息（已注释）
	//PosItem& posItem = isLong ? posInfo._long : posInfo._short;  // 获取对应方向的持仓（已注释）
	update_position(stdCode, isLong, offset, vol, price);  // 更新持仓
	if (_strategy)  // 如果策略存在
		_strategy->on_trade(this, localid, stdCode, isLong, offset, vol, price);  // 调用策略的成交回调
}

/**
 * @brief 委托回调
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param bSuccess 是否成功
 * @param message 消息内容
 * 
 * 委托结果返回时调用，触发策略的on_entrust回调。
 */
void UftMocker::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_entrust(localid, bSuccess, message);  // 调用策略的委托回调
}

/**
 * @brief 通道就绪回调
 * 
 * 交易通道就绪时调用，触发策略的on_channel_ready回调。
 */
void UftMocker::on_channel_ready()
{
	if (_strategy)  // 如果策略存在
		_strategy->on_channel_ready(this);  // 调用策略的通道就绪回调
}

/**
 * @brief 更新动态盈亏
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 根据当前Tick数据更新指定合约的持仓动态盈亏。
 * 分别计算多头和空头的动态盈亏，并更新每笔持仓明细的最大盈利、最大亏损。
 */
void UftMocker::update_dyn_profit(const char* stdCode, WTSTickData* newTick)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it != _pos_map.end())  // 如果找到持仓
	{
		WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
		PosInfo& pInfo = (PosInfo&)it->second;  // 获取持仓信息
		{
			bool isLong = true;  // 多头标志
			PosItem& pItem = pInfo._long;  // 获取多头持仓
			if (pItem.volume() == 0)  // 如果持仓为0
				pItem._dynprofit = 0;  // 动态盈亏为0
			else
			{
				double price = isLong ? newTick->bidprice(0) : newTick->askprice(0);  // 获取结算价格（多头用买一价，空头用卖一价）
				double dynprofit = 0;  // 总动态盈亏
				// 遍历持仓明细
				for (auto pit = pItem._details.begin(); pit != pItem._details.end(); pit++)
				{

					DetailInfo& dInfo = *pit;  // 获取持仓明细信息
					dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale();  // 计算明细盈亏
					if (dInfo._profit > 0)  // 如果盈利
						dInfo._max_profit = max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利
					else if (dInfo._profit < 0)  // 如果亏损
						dInfo._max_loss = min(dInfo._profit, dInfo._max_loss);  // 更新最大亏损

					dynprofit += dInfo._profit;  // 累加明细盈亏
				}

				pItem._dynprofit = dynprofit;  // 更新持仓动态盈亏
			}
		}

		{
			bool isLong = false;  // 空头标志
			PosItem& pItem = pInfo._short;  // 获取空头持仓
			if (pItem.volume() == 0)  // 如果持仓为0
				pItem._dynprofit = 0;  // 动态盈亏为0
			else
			{
				double price = isLong ? newTick->bidprice(0) : newTick->askprice(0);  // 获取结算价格（多头用买一价，空头用卖一价）
				double dynprofit = 0;  // 总动态盈亏
				// 遍历持仓明细
				for (auto pit = pItem._details.begin(); pit != pItem._details.end(); pit++)
				{

					DetailInfo& dInfo = *pit;  // 获取持仓明细信息
					dInfo._profit = dInfo._volume*(dInfo._price - price)*commInfo->getVolScale();  // 计算明细盈亏（空头：开仓价-当前价）
					if (dInfo._profit > 0)  // 如果盈利
						dInfo._max_profit = max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利
					else if (dInfo._profit < 0)  // 如果亏损
						dInfo._max_loss = min(dInfo._profit, dInfo._max_loss);  // 更新最大亏损

					dynprofit += dInfo._profit;  // 累加明细盈亏
				}

				pItem._dynprofit = dynprofit;  // 更新持仓动态盈亏
			}
		}
	}
}

/**
 * @brief 处理订单撮合
 * @param localid 本地订单ID
 * @return 如果订单已完成（全部成交或已撤销），返回true；否则返回false
 * 
 * 根据当前市场数据，尝试撮合指定订单。
 * 如果订单可以成交，则更新订单状态和持仓信息。
 * 支持错误率模拟（随机撤单）和限价订单的价格检查。
 */
bool UftMocker::procOrder(uint32_t localid)
{
	auto it = _orders.find(localid);  // 查找订单
	if (it == _orders.end())  // 如果订单不存在
		return false;  // 返回false

	OrderInfo ordInfo = (OrderInfo&)it->second;  // 获取订单信息

	//第一步,如果在撤单概率中,则执行撤单
	if(_error_rate>0 && genRand(10000)<=_error_rate)  // 如果错误率大于0且随机数在错误率范围内
	{
		on_order(localid, ordInfo._code, ordInfo._isLong, ordInfo._offset, ordInfo._total, ordInfo._left, ordInfo._price, true);  // 触发订单状态回调（已撤销）
		log_info("Random error order: {}", localid);  // 记录日志
		return true;  // 返回true（订单已完成）
	}
	else  // 如果不在错误率范围内
	{
		on_order(localid, ordInfo._code, ordInfo._isLong, ordInfo._offset, ordInfo._total, ordInfo._left, ordInfo._price, false);  // 触发订单状态回调（未撤销）
	}

	WTSTickData* curTick = stra_get_last_tick(ordInfo._code);  // 获取最新Tick数据
	if (curTick == NULL)  // 如果Tick数据不存在
		return false;  // 返回false

	double curPx = curTick->price();  // 获取当前价格
	double orderQty = ordInfo._isLong ? curTick->askqty(0) : curTick->bidqty(0);	//看对手盘的数量
	if (decimal::eq(orderQty, 0.0))  // 如果对手盘数量为0
	{
		curTick->release();  // 释放Tick数据
		return false;  // 返回false
	}

	if (!_use_newpx)  // 如果不使用最新价撮合
	{
		curPx = ordInfo._isLong ? curTick->askprice(0) : curTick->bidprice(0);  // 使用对手价
		//if (curPx == 0.0)
		if(decimal::eq(curPx, 0.0))  // 如果对手价为0
		{
			curTick->release();  // 释放Tick数据
			return false;  // 返回false
		}
	}
	curTick->release();  // 释放Tick数据

	//如果没有成交条件,则退出逻辑
	if(!decimal::eq(ordInfo._price, 0.0))  // 如果是限价订单
	{
		if(ordInfo._isLong && decimal::gt(curPx, ordInfo._price))  // 如果是买单且当前价大于限价
		{
			//买单,但是当前价大于限价,不成交
			return false;  // 返回false（不成交）
		}

		if (!ordInfo._isLong && decimal::lt(curPx, ordInfo._price))  // 如果是卖单且当前价小于限价
		{
			//卖单,但是当前价小于限价,不成交
			return false;  // 返回false（不成交）
		}
	}

	/*
	 *	下面就要模拟成交了
	 */
	double maxQty = min(orderQty, ordInfo._left);  // 计算可成交数量（取对手盘数量和剩余数量中的较小值）
	auto vols = splitVolume((uint32_t)maxQty);  // 拆分数量（模拟真实交易中的分批成交）
	for(uint32_t curQty : vols)  // 遍历拆分后的数量
	{
		on_trade(ordInfo._localid, ordInfo._code, ordInfo._isLong, ordInfo._offset, curQty, curPx);  // 触发成交回调

		ordInfo._left -= curQty;  // 减少剩余数量
		on_order(localid, ordInfo._code, ordInfo._isLong, ordInfo._offset, ordInfo._total, ordInfo._left, ordInfo._price, false);  // 触发订单状态回调
	}

	//if(ordInfo._left == 0)
	if(decimal::eq(ordInfo._left, 0.0))  // 如果剩余数量为0
	{
		return true;  // 返回true（订单已完成）
	}

	return false;  // 返回false（订单未完成）
}

/**
 * @brief 获取合约信息
 * @param stdCode 标准化合约代码
 * @return 合约信息指针，如果合约不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的合约信息。
 */
WTSCommodityInfo* UftMocker::stra_get_comminfo(const char* stdCode)
{
	return _replayer->get_commodity_info(stdCode);  // 返回合约信息
}

/**
 * @brief 获取K线数据切片
 * @param stdCode 标准化合约代码
 * @param period K线周期（如"m1"表示1分钟，"d1"表示1日）
 * @param count 获取的K线数量
 * @return K线数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的K线数据切片。
 */
WTSKlineSlice* UftMocker::stra_get_bars(const char* stdCode, const char* period, uint32_t count)
{
	thread_local static char basePeriod[2] = { 0 };  // 线程局部静态变量，用于存储基础周期
	basePeriod[0] = period[0];  // 获取周期类型（'d'或'm'）
	uint32_t times = 1;  // 周期倍数，默认为1
	if (strlen(period) > 1)  // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);  // 解析周期倍数（如"m5"中的5）

	return _replayer->get_kline_slice(stdCode, basePeriod, count, times);  // 返回K线数据切片
}

/**
 * @brief 获取Tick数据切片
 * @param stdCode 标准化合约代码
 * @param count 获取的Tick数量
 * @return Tick数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的Tick数据切片。
 */
WTSTickSlice* UftMocker::stra_get_ticks(const char* stdCode, uint32_t count)
{
	return _replayer->get_tick_slice(stdCode, count);  // 返回Tick数据切片
}

/**
 * @brief 获取委托队列数据切片
 * @param stdCode 标准化合约代码
 * @param count 获取的数据数量
 * @return 委托队列数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的委托队列数据切片。
 */
WTSOrdQueSlice* UftMocker::stra_get_order_queue(const char* stdCode, uint32_t count)
{
	return _replayer->get_order_queue_slice(stdCode, count);  // 返回委托队列数据切片
}

/**
 * @brief 获取委托明细数据切片
 * @param stdCode 标准化合约代码
 * @param count 获取的数据数量
 * @return 委托明细数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的委托明细数据切片。
 */
WTSOrdDtlSlice* UftMocker::stra_get_order_detail(const char* stdCode, uint32_t count)
{
	return _replayer->get_order_detail_slice(stdCode, count);  // 返回委托明细数据切片
}

/**
 * @brief 获取逐笔成交数据切片
 * @param stdCode 标准化合约代码
 * @param count 获取的数据数量
 * @return 逐笔成交数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的逐笔成交数据切片。
 */
WTSTransSlice* UftMocker::stra_get_transaction(const char* stdCode, uint32_t count)
{
	return _replayer->get_transaction_slice(stdCode, count);  // 返回逐笔成交数据切片
}

/**
 * @brief 获取最新Tick数据
 * @param stdCode 标准化合约代码
 * @return 最新Tick数据指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的最新Tick数据。
 */
WTSTickData* UftMocker::stra_get_last_tick(const char* stdCode)
{
	return _replayer->get_last_tick(stdCode);  // 返回最新Tick数据
}

/**
 * @brief 获取持仓
 * @param stdCode 标准化合约代码
 * @param bOnlyValid 获取可用持仓（T+1规则下排除冻结持仓），默认为false
 * @param iFlag 读取标记：1-多头，2-空头，3-净头寸，默认为3
 * @return 持仓数量，正数为做多，负数为做空，0表示无持仓
 * 
 * 查询指定合约的持仓数量。根据iFlag参数返回多头、空头或净头寸。
 */
double UftMocker::stra_get_position(const char* stdCode, bool bOnlyValid /* = false */, int32_t iFlag /* = 3 */)
{
	const PosInfo& posInfo = _pos_map[stdCode];  // 获取持仓信息
	if (iFlag == 1)  // 如果查询多头
		return bOnlyValid ? posInfo._long.valid() : posInfo._long.volume();  // 返回多头持仓（可用或总持仓）
	else if (iFlag == 2)  // 如果查询空头
		return bOnlyValid ? posInfo._short.valid() : posInfo._short.volume();  // 返回空头持仓（可用或总持仓）
	else  // 如果查询净头寸
		return bOnlyValid ? (posInfo._long.valid() - posInfo._short.valid()) : (posInfo._long.volume() - posInfo._short.volume());  // 返回净头寸（多头-空头）
}

/**
 * @brief 获取本地持仓（净头寸）
 * @param stdCode 标准化合约代码
 * @return 净头寸（多头持仓减去空头持仓）
 * 
 * 返回指定合约的本地净头寸（多头持仓减去空头持仓）。
 */
double UftMocker::stra_get_local_position(const char* stdCode)
{
	const PosInfo& posInfo = _pos_map[stdCode];  // 获取持仓信息
	return posInfo._long.volume() - posInfo._short.volume();  // 返回净头寸（多头-空头）
}

/**
 * @brief 枚举持仓
 * @param stdCode 标准化合约代码，如果为空字符串则枚举所有合约
 * @return 持仓总数量
 * 
 * 遍历持仓，调用策略的on_position回调传递持仓信息。
 * 返回持仓总数量。
 */
double UftMocker::stra_enum_position(const char* stdCode)
{
	uint32_t tdate = _replayer->get_trading_date();  // 获取交易日（未使用）
	double ret = 0;  // 持仓总数量
	bool bAll = (strlen(stdCode) == 0);  // 是否枚举所有合约
	// 遍历持仓映射表
	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)
	{
		if (!bAll && strcmp(it->first.c_str(), stdCode) != 0)  // 如果指定了合约代码且不匹配
			continue;  // 跳过

		const PosInfo& pInfo = it->second;  // 获取持仓信息
		_strategy->on_position(this, stdCode, true, pInfo._long._prevol, pInfo._long._preavail, pInfo._long._newvol, pInfo._long._newavail);  // 调用策略的多头持仓回调
		_strategy->on_position(this, stdCode, false, pInfo._short._prevol, pInfo._short._preavail, pInfo._short._newvol, pInfo._short._newavail);  // 调用策略的空头持仓回调
		ret += pInfo._long.volume() + pInfo._short.volume();  // 累加持仓数量
	}

	return ret;  // 返回持仓总数量
}

/**
 * @brief 获取当前价格
 * @param stdCode 标准化合约代码
 * @return 当前价格，如果合约不存在返回0.0
 * 
 * 从历史数据回放器获取指定合约的当前价格。
 */
double UftMocker::stra_get_price(const char* stdCode)
{
	return _replayer->get_cur_price(stdCode);  // 返回当前价格
}

/**
 * @brief 获取当前日期
 * @return 当前日期（格式：YYYYMMDD）
 * 
 * 返回当前回测的日期。
 */
uint32_t UftMocker::stra_get_date()
{
	return _replayer->get_date();  // 返回日期
}

/**
 * @brief 获取当前时间
 * @return 当前时间（格式：HHMM，分钟级别）
 * 
 * 返回当前回测的时间（分钟级别）。
 */
uint32_t UftMocker::stra_get_time()
{
	return _replayer->get_raw_time();  // 返回分钟级别时间
}

/**
 * @brief 获取当前秒数
 * @return 当前秒数（0-59）
 * 
 * 返回当前回测的秒数。
 */
uint32_t UftMocker::stra_get_secs()
{
	return _replayer->get_secs();  // 返回秒数
}

/**
 * @brief 订阅Tick数据
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的Tick数据。订阅后，该合约的Tick数据会触发策略的on_tick回调。
 * 
 * 注意：主动订阅tick会在本地记录一下，tick数据回调的时候会先检查一下。
 */
void UftMocker::stra_sub_ticks(const char* stdCode)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(stdCode);  // 添加到Tick订阅列表

	_replayer->sub_tick(_context_id, stdCode);  // 向回放器订阅Tick数据
}

/**
 * @brief 订阅委托队列数据
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的委托队列数据。订阅后，该合约的委托队列数据会触发策略的on_order_queue回调。
 */
void UftMocker::stra_sub_order_queues(const char* stdCode)
{
	_replayer->sub_order_queue(_context_id, stdCode);  // 向回放器订阅委托队列数据
}

/**
 * @brief 订阅委托明细数据
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的委托明细数据。订阅后，该合约的委托明细数据会触发策略的on_order_detail回调。
 */
void UftMocker::stra_sub_order_details(const char* stdCode)
{
	_replayer->sub_order_detail(_context_id, stdCode);  // 向回放器订阅委托明细数据
}

/**
 * @brief 订阅逐笔成交数据
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的逐笔成交数据。订阅后，该合约的逐笔成交数据会触发策略的on_transaction回调。
 */
void UftMocker::stra_sub_transactions(const char* stdCode)
{
	_replayer->sub_transaction(_context_id, stdCode);  // 向回放器订阅逐笔成交数据
}

/**
 * @brief 记录信息日志
 * @param message 日志消息
 * 
 * 将信息日志记录到策略日志中。
 */
void UftMocker::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 记录信息日志
}

/**
 * @brief 记录调试日志
 * @param message 日志消息
 * 
 * 将调试日志记录到策略日志中。
 */
void UftMocker::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 记录调试日志
}

/**
 * @brief 记录错误日志
 * @param message 日志消息
 * 
 * 将错误日志记录到策略日志中。
 */
void UftMocker::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 记录错误日志
}


/**
 * @brief 输出回测结果文件
 * 
 * 将回测过程中记录的交易日志、平仓日志、资金日志、持仓日志输出为CSV文件。
 * 文件保存在输出目录下的策略名称子目录中。
 */
void UftMocker::dump_outputs()
{
	std::string folder = WtHelper::getOutputDir();  // 获取输出目录
	folder += _name;  // 添加策略名称
	folder += "/";  // 添加路径分隔符
	boost::filesystem::create_directories(folder.c_str());  // 创建目录（如果不存在）

	std::string filename = folder + "trades.csv";  // 交易日志文件名
	std::string content = "code,time,direct,action,price,qty,fee,usertag\n";  // CSV表头
	content += _trade_logs.str();  // 添加交易日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "closes.csv";  // 平仓日志文件名
	content = "code,direct,opentime,openprice,closetime,closeprice,qty,profit,maxprofit,maxloss,totalprofit,entertag,exittag\n";  // CSV表头
	content += _close_logs.str();  // 添加平仓日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件


	filename = folder + "funds.csv";  // 资金日志文件名
	content = "date,closeprofit,positionprofit,dynbalance,fee\n";  // CSV表头
	content += _fund_logs.str();  // 添加资金日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "positions.csv";  // 持仓日志文件名
	content = "date,code,direct,volume,closeprofit,dynprofit\n";  // CSV表头
	if (!_pos_logs.str().empty()) content += _pos_logs.str();  // 如果持仓日志不为空，添加持仓日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件
}

/**
 * @brief 记录交易日志
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param offset 开平标志：0-开仓，1-平仓，2-平今
 * @param curTime 当前时间（纳秒时间戳）
 * @param price 成交价格
 * @param qty 成交数量
 * @param fee 手续费
 * 
 * 记录一笔交易到交易日志中。
 */
void UftMocker::log_trade(const char* stdCode, bool isLong, uint32_t offset, uint64_t curTime, double price, double qty, double fee)
{
	_trade_logs << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << OFFSET_NAMES[offset]
		<< "," << price << "," << qty << "," << fee  << "\n";  // 格式化交易日志并写入日志流
}

/**
 * @brief 记录平仓日志
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param openTime 开仓时间（纳秒时间戳）
 * @param openpx 开仓价格
 * @param closeTime 平仓时间（纳秒时间戳）
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 盈亏
 * @param maxprofit 最大盈利
 * @param maxloss 最大亏损
 * @param totalprofit 总盈亏，默认为0
 * 
 * 记录一笔平仓到平仓日志中。
 */
void UftMocker::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double maxprofit, double maxloss,
	double totalprofit /* = 0 */)
{
	_close_logs << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
		<< "," << closeTime << "," << closepx << "," << qty << "," << profit << "," << maxprofit << "," << maxloss << ","
		<< totalprofit << "\n";  // 格式化平仓日志并写入日志流
}

/**
 * @brief 更新持仓
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param offset 开平标志：0-开仓，1-平仓，2-平今
 * @param qty 成交数量
 * @param price 成交价格，默认为0.0（如果为0则使用当前价格）
 * 
 * 根据成交信息更新持仓。如果是开仓，则增加持仓明细；如果是平仓，则减少持仓明细并计算盈亏。
 * 支持T+1规则的持仓冻结和释放。
 */
void UftMocker::update_position(const char* stdCode, bool isLong, uint32_t offset, double qty, double price /* = 0.0 */)
{
	PosItem& pItem = isLong ? _pos_map[stdCode]._long : _pos_map[stdCode]._short;  // 获取对应方向的持仓

	//先确定成交价格
	double curPx = price;  // 成交价格
	if (decimal::eq(price, 0.0))  // 如果价格为0
		curPx = _price_map[stdCode];  // 使用当前价格

	const char* pos_dir = isLong ? "long" : "short";  // 持仓方向字符串

	//获取时间
	uint64_t curTm = (uint64_t)_replayer->get_date() * 1000000000 + (uint64_t)_replayer->get_min_time()*100000 + _replayer->get_secs();  // 当前时间（纳秒时间戳）
	uint32_t curTDate = _replayer->get_trading_date();  // 当前交易日

	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
		return;  // 直接返回

	//成交价
	double trdPx = curPx;  // 成交价格

	if (offset == 0)  // 如果是开仓
	{
		//如果是开仓，则直接增加明细即可
		pItem._newvol += qty;  // 增加今仓数量
		//如果T+1，则冻结仓位要增加
		if (commInfo->isT1())  // 如果是T+1合约
		{
			//ASSERT(diff>0);
			log_debug("{} position of {} frozen up to {}", pos_dir, stdCode, pItem.frozen());  // 记录日志
		}
		else  // 如果不是T+1合约
		{
			pItem._newavail += qty;  // 增加今仓可用数量
		}

		DetailInfo dInfo;  // 创建持仓明细
		dInfo._price = trdPx;  // 设置开仓价格
		dInfo._volume = qty;  // 设置持仓数量
		dInfo._opentime = curTm;  // 设置开仓时间
		dInfo._opentdate = curTDate;  // 设置开仓交易日
		pItem._details.emplace_back(dInfo);  // 添加到持仓明细列表

		double fee = _replayer->calc_fee(stdCode, trdPx, qty, 0);  // 计算手续费（开仓）
		_fund_info._total_fees += fee;  // 累加总手续费

		log_trade(stdCode, isLong, 0, curTm, trdPx, qty, fee);  // 记录交易日志
	}
	else if(offset == 1)  // 如果是平仓（平昨）
	{
		//如果是平仓（平昨也是这个），则根据明细的时间先后处理平仓
		double maxQty = min(pItem._prevol, qty);  // 计算可平昨仓数量
		pItem._prevol -= maxQty;  // 减少昨仓数量
		pItem._newvol -= qty - maxQty;  // 减少今仓数量（剩余部分）

		std::vector<DetailInfo>::iterator eit = pItem._details.end();  // 需要删除的明细结束位置
		double left = qty;  // 剩余平仓数量
		// 遍历持仓明细，按时间先后平仓
		for (auto it = pItem._details.begin(); it != pItem._details.end(); it++)
		{
			DetailInfo& dInfo = *it;  // 获取持仓明细信息
			double maxQty = min(dInfo._volume, left);  // 计算可平仓数量
			if (decimal::eq(maxQty, 0))  // 如果可平仓数量为0
				continue;  // 跳过

			double maxProf = dInfo._max_profit * maxQty / dInfo._volume;  // 计算比例最大盈利
			double maxLoss = dInfo._max_loss * maxQty / dInfo._volume;  // 计算比例最大亏损

			dInfo._volume -= maxQty;  // 减少持仓数量
			left -= maxQty;  // 减少剩余平仓数量

			if (decimal::eq(dInfo._volume, 0))  // 如果持仓数量为0
				eit = it;  // 记录需要删除的位置

			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算盈亏
			if (isLong)  // 如果是多头
				profit *= -1;  // 盈亏取反（多头平仓：开仓价-平仓价）
			pItem._closeprofit += profit;  // 累加已平仓盈亏

			//等比缩放明细的相关浮盈
			dInfo._profit = dInfo._profit*dInfo._volume / (dInfo._volume + maxQty);  // 更新明细盈亏
			dInfo._max_profit = dInfo._max_profit*dInfo._volume / (dInfo._volume + maxQty);  // 更新明细最大盈利
			dInfo._max_loss = dInfo._max_loss*dInfo._volume / (dInfo._volume + maxQty);  // 更新明细最大亏损
			_fund_info._total_profit += profit;  // 累加总已平仓盈亏
			double fee = _replayer->calc_fee(stdCode, trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费（平今仓为2，平昨仓为1）
			_fund_info._total_fees += fee;  // 累加总手续费
			//这里写成交记录
			log_trade(stdCode, isLong, offset, curTm, trdPx, maxQty, fee);  // 记录交易日志
			//这里写平仓记录
			log_close(stdCode, isLong, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, maxProf, maxLoss, pItem._closeprofit);  // 记录平仓日志

			if (left == 0)  // 如果剩余平仓数量为0
				break;  // 退出循环
		}

		//需要清理掉已经平仓完的明细
		if (eit != pItem._details.end())  // 如果有需要删除的明细
			pItem._details.erase(pItem._details.begin(), eit);  // 删除已平仓完的明细

	}
	else if (offset == 2)  // 如果是平今仓
	{
		//如果是平今，只更新今仓，先找到今仓起始的位置，再开始处理
		pItem._newvol -= qty;  // 减少今仓数量
		std::vector<DetailInfo>::iterator sit = pItem._details.end();  // 今仓起始位置
		std::vector<DetailInfo>::iterator eit = pItem._details.end();  // 需要删除的明细结束位置

		uint32_t count = 0;  // 计数（未使用）
		double left = qty;  // 剩余平仓数量
		// 遍历持仓明细，只处理今仓
		for (auto it = pItem._details.begin(); it != pItem._details.end(); it++)
		{
			DetailInfo& dInfo = *it;  // 获取持仓明细信息
			//如果不是今仓，就直接跳过
			if(dInfo._opentdate != curTDate)  // 如果开仓交易日不是当前交易日
				continue;  // 跳过

			double maxQty = min(dInfo._volume, left);  // 计算可平仓数量
			if (decimal::eq(maxQty, 0))  // 如果可平仓数量为0
				continue;  // 跳过

			if (sit == pItem._details.end())  // 如果还没有记录今仓起始位置
				sit = it;  // 记录今仓起始位置

			eit = it;  // 更新需要删除的位置

			double maxProf = dInfo._max_profit * maxQty / dInfo._volume;  // 计算比例最大盈利
			double maxLoss = dInfo._max_loss * maxQty / dInfo._volume;  // 计算比例最大亏损

			dInfo._volume -= maxQty;  // 减少持仓数量
			left -= maxQty;  // 减少剩余平仓数量

			if (decimal::eq(dInfo._volume, 0))  // 如果持仓数量为0
				count++;

			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();
			if (!isLong)
				profit *= -1;
			pItem._closeprofit += profit;
			pItem._dynprofit = pItem._dynprofit*dInfo._volume / (dInfo._volume + maxQty);//浮盈也要做等比缩放
			_fund_info._total_profit += profit;

			uint32_t offset = dInfo._opentdate == curTDate ? 2 : 1;
			double fee = _replayer->calc_fee(stdCode, trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);
			_fund_info._total_fees += fee;
			//这里写成交记录
			log_trade(stdCode, isLong, offset, curTm, trdPx, maxQty, fee);
			//这里写平仓记录
			log_close(stdCode, isLong, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, maxProf, maxLoss, pItem._closeprofit);

			if (left == 0)
				break;
		}

		//需要清理掉已经平仓完的明细
		if (sit != pItem._details.end())
			pItem._details.erase(sit, eit);
	}

	log_info("[{:04d}.{:05d}] {} position of {} updated: {} {} to {}", _replayer->get_min_time(), _replayer->get_secs(), pos_dir, stdCode, OFFSET_NAMES[offset], qty, pItem.volume());

	double dynprofit = 0;
	for (const DetailInfo& dInfo : pItem._details)
	{
		dynprofit += dInfo._profit;
	}
	pItem._dynprofit = dynprofit;
}
