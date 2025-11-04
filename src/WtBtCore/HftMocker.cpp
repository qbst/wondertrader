/*!
 * \file HftMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易策略回测模拟器实现文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是HftMocker类的实现文件，包含了高频交易策略回测模拟器的所有功能实现。
 *
 * 主要功能模块：
 * 1. 构造函数和析构函数：初始化回测环境，清理资源
 * 2. 初始化功能：加载HFT策略模块，创建策略实例
 * 3. 数据接收处理：处理tick数据、订单队列、订单明细、逐笔成交等市场数据
 * 4. 订单处理：处理策略发出的买卖订单，模拟订单撮合过程
 * 5. 成交回报处理：处理成交回报，更新持仓和未完成订单
 * 6. 持仓管理：跟踪策略持仓，计算持仓盈亏、浮动盈亏等
 * 7. 结果输出：将回测结果输出到文件（成交记录、平仓记录、持仓记录、资金记录等）
 * 8. 异步回测支持：通过钩子机制实现步进式回测
 *
 * 核心算法：
 * - 订单撮合：根据tick数据模拟订单成交过程
 * - 持仓管理：跟踪多空持仓，计算持仓明细和盈亏
 * - 动态盈亏计算：根据最新价格计算持仓浮动盈亏
 * - 日志记录：记录成交、平仓、持仓、资金等详细信息
 */
#include "HftMocker.h"
#include "WtHelper.h"                                               // WonderTrader辅助函数

#include <stdarg.h>                                                 // 可变参数支持

#include <boost/filesystem.hpp>                                     // Boost文件系统库

#include "../Includes/WTSVariant.hpp"                               // 变体类型定义
#include "../Includes/WTSContractInfo.hpp"                           // 合约信息定义
#include "../Share/decimal.h"                                       // 小数精度计算工具
#include "../Share/TimeUtils.hpp"                                   // 时间工具函数
#include "../Share/StrUtil.hpp"                                     // 字符串工具函数
#include "../Share/StdUtils.hpp"                                    // 标准工具函数

#include "../WTSTools/WTSLogger.h"                                  // 日志工具

#include <rapidjson/document.h>                                    // RapidJSON文档类
#include <rapidjson/prettywriter.h>                                 // RapidJSON格式化写入器
namespace rj = rapidjson;                                           // RapidJSON命名空间别名

/**
 * @brief 生成本地订单ID
 * 
 * 使用原子变量生成唯一的订单ID
 * 
 * @return 本地订单ID
 */
uint32_t makeLocalOrderID()
{
	static std::atomic<uint32_t> _auto_order_id{ 0 };               // 静态原子变量，用于生成订单ID
	if (_auto_order_id == 0)                                        // 如果订单ID为0（首次调用）
	{
		uint32_t curYear = TimeUtils::getCurDate() / 10000 * 10000 + 101;  // 计算当前年份的第一天（如20240101）
		_auto_order_id = (uint32_t)((TimeUtils::getLocalTimeNow() - TimeUtils::makeTime(curYear, 0)) / 1000 * 50);  // 基于当前时间生成初始订单ID
	}

	return _auto_order_id.fetch_add(1);                              // 原子递增并返回订单ID
}

/**
 * @brief 拆分数量（整数版本）
 * 
 * 将数量拆分成多个随机数量的订单，用于模拟大单拆分
 * 
 * @param vol 总数量
 * @return 拆分后的数量列表
 */
std::vector<uint32_t> splitVolume(uint32_t vol)
{
	if (vol == 0) return std::move(std::vector<uint32_t>());       // 如果数量为0，返回空列表

	uint32_t minQty = 1;                                            // 最小数量
	uint32_t maxQty = 100;                                          // 最大数量
	uint32_t length = maxQty - minQty + 1;                          // 数量范围长度
	std::vector<uint32_t> ret;                                      // 结果列表
	if (vol <= minQty)                                             // 如果总数量小于等于最小数量
	{
		ret.emplace_back(vol);                                      // 直接返回原数量
	}
	else                                                           // 如果总数量大于最小数量
	{
		uint32_t left = vol;                                       // 剩余数量
		srand((uint32_t)time(NULL));                                // 初始化随机数种子
		while (left > 0)                                            // 当还有剩余数量时
		{
			uint32_t curVol = minQty + (uint32_t)rand() % length;   // 生成随机数量

			if (curVol >= left)                                     // 如果当前数量大于等于剩余数量
				curVol = left;                                      // 使用剩余数量

			if (curVol == 0)                                        // 如果当前数量为0
				continue;                                           // 跳过本次循环

			ret.emplace_back(curVol);                                // 添加到结果列表
			left -= curVol;                                         // 减少剩余数量
		}
	}

	return std::move(ret);                                          // 返回结果列表
}

/**
 * @brief 拆分数量（浮点数版本）
 * 
 * 将数量拆分成多个随机数量的订单，支持自定义最小数量、最大数量和数量步进
 * 
 * @param vol 总数量
 * @param minQty 最小数量（默认1.0）
 * @param maxQty 最大数量（默认100.0）
 * @param qtyTick 数量步进（默认1.0）
 * @return 拆分后的数量列表
 */
std::vector<double> splitVolume(double vol, double minQty = 1.0, double maxQty = 100.0, double qtyTick = 1.0)
{
	auto length = (std::size_t)round((maxQty - minQty)/qtyTick) + 1;  // 计算数量范围长度
	std::vector<double> ret;                                         // 结果列表
	if (vol <= minQty)                                              // 如果总数量小于等于最小数量
	{
		ret.emplace_back(vol);                                      // 直接返回原数量
	}
	else                                                           // 如果总数量大于最小数量
	{
		double left = vol;                                         // 剩余数量
		srand((uint32_t)time(NULL));                                // 初始化随机数种子
		while (left > 0)                                            // 当还有剩余数量时
		{
			double curVol = minQty + (rand() % length)*qtyTick;      // 生成随机数量

			if (curVol >= left)                                     // 如果当前数量大于等于剩余数量
				curVol = left;                                      // 使用剩余数量

			if (curVol == 0)                                        // 如果当前数量为0
				continue;                                           // 跳过本次循环

			ret.emplace_back(curVol);                                // 添加到结果列表
			left -= curVol;                                         // 减少剩余数量
		}
	}

	return std::move(ret);                                          // 返回结果列表
}

/**
 * @brief 生成随机数
 * 
 * @param maxVal 最大值（默认10000）
 * @return 随机数（0到maxVal-1）
 */
uint32_t genRand(uint32_t maxVal = 10000)
{
	srand(TimeUtils::getCurMin());                                  // 使用当前分钟数初始化随机数种子
	return rand() % maxVal;                                          // 返回随机数
}

/**
 * @brief 生成HFT上下文ID
 * 
 * 使用原子变量生成唯一的上下文ID，从6000开始
 * 
 * @return 上下文ID
 */
inline uint32_t makeHftCtxId()
{
	static std::atomic<uint32_t> _auto_context_id{ 6000 };          // 静态原子变量，初始值为6000
	return _auto_context_id.fetch_add(1);                            // 原子递增并返回上下文ID
}

/**
 * @brief HftMocker构造函数
 * 
 * 初始化所有成员变量，创建合约映射表和tick缓存
 * 
 * @param replayer 历史数据回放器指针
 * @param name 策略名称
 */
HftMocker::HftMocker(HisDataReplayer* replayer, const char* name)
	: IHftStraCtx(name)                                             // 调用基类构造函数
	, _replayer(replayer)                                            // 初始化历史数据回放器指针
	, _strategy(NULL)                                                // 初始化策略指针为NULL
	, _use_newpx(false)                                             // 初始化使用新价格标志为false
	, _error_rate(0)                                                 // 初始化错误率为0
	, _match_this_tick(false)                                       // 初始化是否在当前tick撮合为false
	, _has_hook(false)                                               // 初始化是否启用钩子为false
	, _hook_valid(true)                                              // 初始化钩子是否有效为true
	, _resumed(false)                                                // 初始化是否已恢复为false
{
	_commodities = CommodityMap::create();                          // 创建合约映射表

	_context_id = makeHftCtxId();                                    // 生成上下文ID

	_ticks = TickCache::create();                                   // 创建tick缓存
}


/**
 * @brief HftMocker析构函数
 * 
 * 释放策略实例，释放合约映射表和tick缓存
 */
HftMocker::~HftMocker()
{
	if(_strategy)                                                    // 如果策略实例存在
	{
		_factory._fact->deleteStrategy(_strategy);                  // 删除策略实例
	}

	_commodities->release();                                         // 释放合约映射表

	_ticks->release();                                                // 释放tick缓存
	_ticks = NULL;                                                   // 清空tick缓存指针
}

/**
 * @brief 处理任务队列
 * 
 * 从任务队列中取出任务并执行，直到队列为空
 */
void HftMocker::procTask()
{
	if (_tasks.empty())                                             // 如果任务队列为空
	{
		return;                                                       // 直接返回
	}

	_mtx_control.lock();                                             // 锁定控制互斥锁

	while (!_tasks.empty())                                         // 当任务队列不为空时
	{
		Task& task = _tasks.front();                                 // 获取队列首任务

		task();                                                      // 执行任务

		{
			std::unique_lock<std::mutex> lck(_mtx);                 // 锁定任务队列互斥锁
			_tasks.pop();                                            // 弹出已执行的任务
		}
	}

	_mtx_control.unlock();                                           // 解锁控制互斥锁
}

/**
 * @brief 提交任务到任务队列
 * 
 * 将任务添加到任务队列中，等待处理
 * 
 * @param task 任务函数
 */
void HftMocker::postTask(Task task)
{
	{
		std::unique_lock<std::mutex> lck(_mtx);                      // 锁定任务队列互斥锁
		_tasks.push(task);                                           // 将任务添加到队列
		return;                                                       // 返回
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
 * @brief 初始化HFT策略工厂
 * 
 * 1. 从配置中读取模块路径、撮合参数等
 * 2. 动态加载HFT策略模块
 * 3. 创建策略工厂实例
 * 4. 创建策略实例并初始化
 * 
 * @param cfg 配置信息
 * @return 是否初始化成功
 */
bool HftMocker::init_hft_factory(WTSVariant* cfg)
{
	if (cfg == NULL)                                                 // 如果配置为空
		return false;                                                 // 返回失败

	const char* module = cfg->getCString("module");                  // 获取模块路径
	
	_use_newpx = cfg->getBoolean("use_newpx");                      // 获取是否使用新价格标志
	_error_rate = cfg->getUInt32("error_rate");                      // 获取错误率
	_match_this_tick = cfg->getBoolean("match_this_tick");           // 获取是否在当前tick撮合标志

	log_info("HFT match params: use_newpx-{}, error_rate-{}, match_this_tick-{}", _use_newpx, _error_rate, _match_this_tick);  // 记录日志

	DllHandle hInst = DLLHelper::load_library(module);              // 加载HFT策略模块动态库
	if (hInst == NULL)                                               // 如果加载失败
		return false;                                                 // 返回失败

	FuncCreateHftStraFact creator = (FuncCreateHftStraFact)DLLHelper::get_symbol(hInst, "createStrategyFact");  // 获取创建工厂函数地址
	if (creator == NULL)                                             // 如果获取失败
	{
		DLLHelper::free_library(hInst);                             // 释放动态库
		return false;                                                 // 返回失败
	}

	_factory._module_inst = hInst;                                   // 保存动态库句柄
	_factory._module_path = module;                                   // 保存模块路径
	_factory._creator = creator;                                      // 保存创建函数指针
	_factory._remover = (FuncDeleteHftStraFact)DLLHelper::get_symbol(hInst, "deleteStrategyFact");  // 获取删除工厂函数地址
	_factory._fact = _factory._creator();                            // 创建策略工厂实例

	WTSVariant* cfgStra = cfg->get("strategy");                     // 获取策略配置
	if(cfgStra)                                                      // 如果配置存在
	{
		_strategy = _factory._fact->createStrategy(cfgStra->getCString("name"), cfgStra->getCString("id"));  // 创建策略实例
		_strategy->init(cfgStra->get("params"));                    // 初始化策略
		_name = _strategy->id();                                     // 获取策略ID
	}
	return true;                                                      // 返回成功
}

/**
 * @brief 处理tick数据（IDataSink接口实现）
 * 
 * @param stdCode 合约代码
 * @param curTick 当前tick数据
 * @param pxType 价格类型
 */
void HftMocker::handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType)
{
	on_tick(stdCode, curTick);                                        // 调用策略tick回调
}

/**
 * @brief 处理订单明细数据（IDataSink接口实现）
 * 
 * @param stdCode 合约代码
 * @param curOrdDtl 当前订单明细数据
 */
void HftMocker::handle_order_detail(const char* stdCode, WTSOrdDtlData* curOrdDtl)
{
	on_order_detail(stdCode, curOrdDtl);                             // 调用策略订单明细回调
}

/**
 * @brief 处理订单队列数据（IDataSink接口实现）
 * 
 * @param stdCode 合约代码
 * @param curOrdQue 当前订单队列数据
 */
void HftMocker::handle_order_queue(const char* stdCode, WTSOrdQueData* curOrdQue)
{
	on_order_queue(stdCode, curOrdQue);                              // 调用策略订单队列回调
}

/**
 * @brief 处理逐笔成交数据（IDataSink接口实现）
 * 
 * @param stdCode 合约代码
 * @param curTrans 当前逐笔成交数据
 */
void HftMocker::handle_transaction(const char* stdCode, WTSTransData* curTrans)
{
	on_transaction(stdCode, curTrans);                               // 调用策略逐笔成交回调
}

/**
 * @brief 处理K线收盘事件（IDataSink接口实现）
 * 
 * @param stdCode 合约代码
 * @param period 周期
 * @param times 周期倍数
 * @param newBar 新的K线数据
 */
void HftMocker::handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	on_bar(stdCode, period, times, newBar);                          // 调用策略K线回调
}

/**
 * @brief 处理初始化事件（IDataSink接口实现）
 * 
 * 调用策略初始化回调和通道就绪回调
 */
void HftMocker::handle_init()
{
	on_init();                                                        // 调用策略初始化回调
	on_channel_ready();                                               // 调用通道就绪回调
}

/**
 * @brief 处理调度事件（IDataSink接口实现）
 * 
 * @param uDate 日期
 * @param uTime 时间
 */
void HftMocker::handle_schedule(uint32_t uDate, uint32_t uTime)
{
	//on_schedule(uDate, uTime);                                      // 未实现
}

/**
 * @brief 处理交易时段开始事件（IDataSink接口实现）
 * 
 * @param curTDate 当前交易日
 */
void HftMocker::handle_session_begin(uint32_t curTDate)
{
	on_session_begin(curTDate);                                      // 调用策略交易时段开始回调
}

/**
 * @brief 处理交易时段结束事件（IDataSink接口实现）
 * 
 * @param curTDate 当前交易日
 */
void HftMocker::handle_session_end(uint32_t curTDate)
{
	on_session_end(curTDate);                                        // 调用策略交易时段结束回调
}

/**
 * @brief 处理回放完成事件（IDataSink接口实现）
 * 
 * 输出回测结果并调用回测结束回调
 */
void HftMocker::handle_replay_done()
{
	dump_outputs();                                                   // 输出回测结果

	this->on_bactest_end();                                          // 调用回测结束回调
}

/**
 * @brief 策略K线回调（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param period 周期
 * @param times 周期倍数
 * @param newBar 新的K线数据
 */
void HftMocker::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (_strategy)                                                    // 如果策略存在
		_strategy->on_bar(this, stdCode, period, times, newBar);     // 调用策略on_bar回调
}

/**
 * @brief 启用/禁用钩子
 * 
 * @param bEnabled 是否启用
 */
void HftMocker::enable_hook(bool bEnabled /* = true */)
{
	_hook_valid = bEnabled;                                          // 设置钩子是否有效

	WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calculating hook {}", bEnabled ? "enabled" : "disabled");  // 记录日志
}

/**
 * @brief 安装钩子（用于异步回测）
 */
void HftMocker::install_hook()
{
	_has_hook = true;                                                 // 设置已安装钩子标志

	WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "HFT hook installed");  // 记录日志
}

/**
 * @brief 步进tick（用于异步回测）
 * 
 * 等待计算线程完成，然后通知控制线程继续
 */
void HftMocker::step_tick()
{
	if (!_has_hook)                                                   // 如果未安装钩子
		return;                                                       // 直接返回

	WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Notify calc thread, wait for calc done");  // 记录日志
	while (!_resumed)                                                 // 当未恢复时
		_cond_calc.notify_all();                                     // 通知所有等待的线程

	{
		StdUniqueLock lock(_mtx_calc);                                // 锁定计算互斥锁
		_cond_calc.wait(_mtx_calc);                                  // 等待计算完成
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc done notified");  // 记录日志
		_resumed = false;                                             // 重置恢复标志
	}
}

/**
 * @brief 处理tick数据（核心处理函数）
 * 
 * 1. 更新价格映射表
 * 2. 更新动态盈亏
 * 3. 根据_match_this_tick标志决定处理顺序：
 *    - 如果开启同tick撮合：先触发策略on_tick，再处理订单
 *    - 如果未开启同tick撮合：先处理订单，再触发策略on_tick
 * 4. 处理订单撮合
 * 5. 处理异步回测钩子
 * 
 * @param stdCode 合约代码
 * @param newTick 新的tick数据
 */
void HftMocker::on_tick(const char* stdCode, WTSTickData* newTick)
{
	_price_map[stdCode] = newTick->price();                          // 更新价格映射表
	{
		std::unique_lock<std::recursive_mutex> lck(_mtx_control);     // 锁定控制互斥锁（可能用于同步）
	}

	update_dyn_profit(stdCode, newTick);                             // 更新动态盈亏

	OrderIDs all_ids;                                                 // 订单ID列表
	for (auto it = _orders.begin(); it != _orders.end(); it++)       // 遍历所有订单
		all_ids.push_back(it->first);                                // 收集订单ID
	//如果开启了同tick撮合，则先触发策略的ontick，再处理订单
	//如果没开启同tick撮合，则先处理订单，再触发策略的ontick
	if (_match_this_tick)                                             // 如果开启同tick撮合
	{
		if (_has_hook && _hook_valid)                                // 如果启用了钩子
		{
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Waiting for resume notify");  // 记录日志
			StdUniqueLock lock(_mtx_calc);                           // 锁定计算互斥锁
			_cond_calc.wait(_mtx_calc);                             // 等待计算恢复
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc resumed");  // 记录日志
			_resumed = true;                                          // 设置恢复标志
		}

		on_tick_updated(stdCode, newTick);                           // 触发策略tick更新

		procTask();                                                   // 处理任务队列

		if (!_orders.empty())                                        // 如果订单队列不为空
		{
			StdLocker<StdRecurMutex> lock(_mtx_ords);               // 锁定订单互斥锁
			OrderIDs ids;                                            // 需要删除的订单ID列表
			for (uint32_t localid : all_ids)                        // 遍历所有订单ID
			{
				bool bNeedErase = procOrder(localid);                // 处理订单，返回是否需要删除
				if (bNeedErase)                                      // 如果需要删除
					ids.emplace_back(localid);                       // 添加到删除列表
			}

			for (uint32_t localid : ids)                             // 遍历需要删除的订单ID
			{
				_orders.erase(localid);                              // 从订单映射表中删除
			}
		}
	}
	else                                                             // 如果未开启同tick撮合
	{
		if (!_orders.empty())                                        // 如果订单队列不为空
		{
			StdLocker<StdRecurMutex> lock(_mtx_ords);               // 锁定订单互斥锁
			OrderIDs ids;                                            // 需要删除的订单ID列表
			for (uint32_t localid : all_ids)                        // 遍历所有订单ID
			{
				bool bNeedErase = procOrder(localid);                // 处理订单，返回是否需要删除
				if (bNeedErase)                                      // 如果需要删除
					ids.emplace_back(localid);                       // 添加到删除列表
			}

			for (uint32_t localid : ids)                             // 遍历需要删除的订单ID
			{
				auto it = _orders.find(localid);                     // 查找订单
				_orders.erase(it);                                   // 从订单映射表中删除
			}
		}

		if (_has_hook && _hook_valid)                                // 如果启用了钩子
		{
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Waiting for resume notify");  // 记录日志
			StdUniqueLock lock(_mtx_calc);                           // 锁定计算互斥锁
			_cond_calc.wait(_mtx_calc);                             // 等待计算恢复
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc resumed");  // 记录日志
			_resumed = true;                                          // 设置恢复标志
		}

		on_tick_updated(stdCode, newTick);                           // 触发策略tick更新

		procTask();                                                   // 处理任务队列
	}

	if (_has_hook && _hook_valid)                                    // 如果启用了钩子
	{
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc done, notify control thread");  // 记录日志
		while (_resumed)                                              // 当已恢复时
			_cond_calc.notify_all();                                 // 通知所有等待的线程
	}
}

/**
 * @brief tick数据更新回调
 * 
 * 检查合约是否已订阅，如果已订阅则触发策略on_tick回调
 * 
 * @param stdCode 合约代码
 * @param newTick 新的tick数据
 */
void HftMocker::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	auto it = _tick_subs.find(stdCode);                               // 查找合约是否已订阅
	if (it == _tick_subs.end())                                       // 如果未订阅
		return;                                                       // 直接返回

	if (_strategy)                                                     // 如果策略存在
		_strategy->on_tick(this, stdCode, newTick);                   // 调用策略on_tick回调
}

/**
 * @brief 策略订单队列回调（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param newOrdQue 新的订单队列数据
 */
void HftMocker::on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	on_ordque_updated(stdCode, newOrdQue);                            // 调用订单队列更新回调
}

/**
 * @brief 订单队列更新回调
 * 
 * @param stdCode 合约代码
 * @param newOrdQue 新的订单队列数据
 */
void HftMocker::on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_order_queue(this, stdCode, newOrdQue);          // 调用策略on_order_queue回调
}

/**
 * @brief 策略订单明细回调（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param newOrdDtl 新的订单明细数据
 */
void HftMocker::on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	on_orddtl_updated(stdCode, newOrdDtl);                           // 调用订单明细更新回调
}

/**
 * @brief 订单明细更新回调
 * 
 * @param stdCode 合约代码
 * @param newOrdDtl 新的订单明细数据
 */
void HftMocker::on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_order_detail(this, stdCode, newOrdDtl);         // 调用策略on_order_detail回调
}

/**
 * @brief 策略逐笔成交回调（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param newTrans 新的逐笔成交数据
 */
void HftMocker::on_transaction(const char* stdCode, WTSTransData* newTrans)
{
	on_trans_updated(stdCode, newTrans);                              // 调用逐笔成交更新回调
}

/**
 * @brief 逐笔成交更新回调
 * 
 * @param stdCode 合约代码
 * @param newTrans 新的逐笔成交数据
 */
void HftMocker::on_trans_updated(const char* stdCode, WTSTransData* newTrans)
{
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_transaction(this, stdCode, newTrans);           // 调用策略on_transaction回调
}

/**
 * @brief 获取策略上下文ID（IHftStraCtx接口实现）
 * 
 * @return 上下文ID
 */
uint32_t HftMocker::id()
{
	return _context_id;                                                // 返回上下文ID
}

/**
 * @brief 策略初始化回调（IHftStraCtx接口实现）
 */
void HftMocker::on_init()
{
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_init(this);                                     // 调用策略on_init回调
}

/**
 * @brief 策略交易时段开始回调（IHftStraCtx接口实现）
 * 
 * 1. 释放所有冻结持仓
 * 2. 调用策略on_session_begin回调
 * 
 * @param curTDate 当前交易日
 */
void HftMocker::on_session_begin(uint32_t curTDate)
{
	//每个交易日开始，要把冻结持仓置零
	for (auto& it : _pos_map)                                         // 遍历所有持仓
	{
		const char* stdCode = it.first.c_str();                       // 获取合约代码
		PosInfo& pInfo = (PosInfo&)it.second;                        // 获取持仓信息
		if (!decimal::eq(pInfo._frozen, 0))                          // 如果冻结持仓不为0
		{
			log_debug("{} of {} frozen released on {}", pInfo._frozen, stdCode, curTDate);  // 记录日志
			pInfo._frozen = 0;                                        // 释放冻结持仓
		}
	}

	if (_strategy)                                                     // 如果策略存在
		_strategy->on_session_begin(this, curTDate);                 // 调用策略on_session_begin回调
}

/**
 * @brief 策略交易时段结束回调（IHftStraCtx接口实现）
 * 
 * 1. 汇总所有持仓的盈亏
 * 2. 记录持仓日志和资金日志
 * 3. 调用策略on_session_end回调
 * 
 * @param curTDate 当前交易日
 */
void HftMocker::on_session_end(uint32_t curTDate)
{
	uint32_t curDate = curTDate;// _replayer->get_trading_date();    // 当前日期

	double total_profit = 0;                                          // 总盈亏
	double total_dynprofit = 0;                                       // 总浮动盈亏

	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)     // 遍历所有持仓
	{
		const char* stdCode = it->first.c_str();                     // 获取合约代码
		const PosInfo& pInfo = it->second;                           // 获取持仓信息
		total_profit += pInfo._closeprofit;                          // 累加平仓盈亏
		total_dynprofit += pInfo._dynprofit;                         // 累加浮动盈亏

		if (decimal::eq(pInfo._volume, 0.0))                         // 如果持仓数量为0
			continue;                                                 // 跳过

		_pos_logs << fmt::format("{},{},{},{:.2f},{:.2f}\n", curTDate, stdCode,
			pInfo._volume, pInfo._closeprofit, pInfo._dynprofit);    // 记录持仓日志
	}

	_fund_logs << fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curTDate,
		_fund_info._total_profit, _fund_info._total_dynprofit,
		_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees);  // 记录资金日志

	if (_strategy)                                                     // 如果策略存在
		_strategy->on_session_end(this, curTDate);                   // 调用策略on_session_end回调
}

/**
 * @brief 获取未完成订单数量（IHftStraCtx接口实现）
 * 
 * 遍历所有订单，统计指定合约的未完成订单数量
 * 
 * @param stdCode 合约代码
 * @return 未完成订单数量（买入为正，卖出为负）
 */
double HftMocker::stra_get_undone(const char* stdCode)
{
	double ret = 0;                                                    // 返回值
	for (auto it = _orders.begin(); it != _orders.end(); it++)       // 遍历所有订单
	{
		OrderInfoPtr ordInfo = it->second;                           // 获取订单信息
		if (strcmp(ordInfo->_code, stdCode) == 0)                    // 如果订单属于指定合约
		{
			ret += ordInfo->_left * ordInfo->_isBuy ? 1 : -1;      // 累加未完成数量（买入为正，卖出为负）
		}
	}

	return ret;                                                       // 返回未完成订单数量
}

/**
 * @brief 撤单（按订单ID）（IHftStraCtx接口实现）
 * 
 * 将撤单任务提交到任务队列，异步处理
 * 
 * @param localid 本地订单ID
 * @return 是否成功
 */
bool HftMocker::stra_cancel(uint32_t localid)
{
	postTask([this, localid](){                                      // 提交撤单任务到任务队列
		OrderInfoPtr ordInfo = NULL;                                  // 订单信息指针
		{
			StdLocker<StdRecurMutex> lock(_mtx_ords);               // 锁定订单互斥锁
			auto it = _orders.find(localid);                         // 查找订单
			if (it == _orders.end())                                 // 如果订单不存在
				return;                                               // 直接返回

			ordInfo = it->second;                                     // 获取订单信息
		}
		
		ordInfo->_left = 0;                                           // 设置剩余数量为0（完全撤单）

		on_order(localid, ordInfo->_code, ordInfo->_isBuy, ordInfo->_total, ordInfo->_left, ordInfo->_price, true, ordInfo->_usertag);  // 触发订单回报（撤单）

		{
			StdLocker<StdRecurMutex> lock(_mtx_ords);               // 锁定订单互斥锁
			_orders.erase(localid);                                  // 从订单映射表中删除
		}
	});

	return true;                                                      // 返回成功
}

/**
 * @brief 撤单（按合约和方向）（IHftStraCtx接口实现）
 * 
 * 撤消指定合约和方向的订单，直到达到指定数量或全部撤完
 * 
 * @param stdCode 合约代码
 * @param isBuy 是否买入
 * @param qty 数量（0表示全部）
 * @return 订单ID列表
 */
OrderIDs HftMocker::stra_cancel(const char* stdCode, bool isBuy, double qty /* = 0 */)
{
	OrderIDs ret;                                                      // 返回的订单ID列表
	uint32_t cnt = 0;                                                 // 计数
	for (auto it = _orders.begin(); it != _orders.end(); it++)       // 遍历所有订单
	{
		OrderInfoPtr ordInfo = it->second;                           // 获取订单信息
		if(ordInfo->_isBuy == isBuy && strcmp(ordInfo->_code, stdCode) == 0)  // 如果订单匹配（方向相同且合约相同）
		{
			double left = ordInfo->_left;                            // 保存剩余数量
			stra_cancel(it->first);                                   // 撤单
			ret.emplace_back(it->first);                              // 添加到返回列表
			cnt++;                                                     // 增加计数
			if (left < qty)                                           // 如果剩余数量小于要撤的数量
				qty -= left;                                          // 减少要撤的数量
			else                                                      // 如果剩余数量大于等于要撤的数量
				break;                                                // 退出循环
		}
	}

	return ret;                                                       // 返回订单ID列表
}

/**
 * @brief 买入订单（IHftStraCtx接口实现）
 * 
 * 1. 验证合约信息和数量
 * 2. 创建订单信息
 * 3. 添加到订单映射表
 * 4. 提交委托回报任务
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param userTag 用户标签
 * @param flag 标志（未使用）
 * @param bForceClose 是否强制平仓（未使用）
 * @return 订单ID列表
 */
OrderIDs HftMocker::stra_buy(const char* stdCode, double price, double qty, const char* userTag, int flag /* = 0 */, bool bForceClose /* = false */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                             // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return OrderIDs();                                             // 返回空列表
	}

	if (decimal::le(qty, 0))                                          // 如果数量小于等于0
	{
		log_error("Entrust error: qty {} <= 0", qty);                 // 记录错误日志
		return OrderIDs();                                             // 返回空列表
	}

	uint32_t localid = makeLocalOrderID();                           // 生成本地订单ID

	OrderInfoPtr order(new OrderInfo);                                // 创建订单信息
	order->_localid = localid;                                        // 设置本地订单ID
	strcpy(order->_code, stdCode);                                    // 设置合约代码
	strcpy(order->_usertag, userTag);                                 // 设置用户标签
	order->_isBuy = true;                                             // 设置为买入
	order->_price = price;                                            // 设置订单价格
	order->_total = qty;                                              // 设置总数量
	order->_left = qty;                                               // 设置剩余数量

	{
		StdLocker<StdRecurMutex> lock(_mtx_ords);                    // 锁定订单互斥锁
		_orders[localid] = order;		                            // 添加到订单映射表
	}

	postTask([this, localid](){                                      // 提交委托回报任务
		const OrderInfoPtr& ordInfo = _orders[localid];              // 获取订单信息
		on_entrust(localid, ordInfo->_code, true, "下单成功", ordInfo->_usertag);  // 触发委托回报（成功）
	});

	OrderIDs ids;                                                     // 订单ID列表
	ids.emplace_back(localid);                                        // 添加订单ID
	return ids;                                                       // 返回订单ID列表
}

/**
 * @brief 订单回报回调
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy 是否买入
 * @param totalQty 总数量
 * @param leftQty 剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤单
 * @param userTag 用户标签
 */
void HftMocker::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled /* = false */, const char* userTag /* = "" */)
{
	if(_strategy)                                                      // 如果策略存在
		_strategy->on_order(this, localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, userTag);  // 调用策略on_order回调
}

/**
 * @brief 成交回报回调
 * 
 * 1. 更新持仓
 * 2. 调用策略on_trade回调
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy 是否买入
 * @param vol 成交数量
 * @param price 成交价格
 * @param userTag 用户标签
 */
void HftMocker::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag/* = ""*/)
{
	const PosInfo& posInfo = _pos_map[stdCode];                      // 获取持仓信息
	double curPos = posInfo._volume + vol * (isBuy ? 1 : -1);        // 计算新持仓（买入增加，卖出减少）
	do_set_position(stdCode, curPos, price, userTag);                 // 更新持仓
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_trade(this, localid, stdCode, isBuy, vol, price, userTag);  // 调用策略on_trade回调
}

/**
 * @brief 委托回报回调
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param bSuccess 是否成功
 * @param message 消息
 * @param userTag 用户标签
 */
void HftMocker::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag/* = ""*/)
{
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_entrust(localid, bSuccess, message, userTag);   // 调用策略on_entrust回调
}

/**
 * @brief 通道就绪回调
 */
void HftMocker::on_channel_ready()
{
	if (_strategy)                                                     // 如果策略存在
		_strategy->on_channel_ready(this);                            // 调用策略on_channel_ready回调
}

/**
 * @brief 更新动态盈亏
 * 
 * 根据最新tick数据更新持仓的浮动盈亏
 * 
 * @param stdCode 合约代码
 * @param newTick 新的tick数据
 */
void HftMocker::update_dyn_profit(const char* stdCode, WTSTickData* newTick)
{
	auto it = _pos_map.find(stdCode);                                 // 查找持仓
	if (it != _pos_map.end())                                         // 如果持仓存在
	{
		PosInfo& pInfo = (PosInfo&)it->second;                       // 获取持仓信息
		if (pInfo._volume == 0)                                       // 如果持仓数量为0
		{
			pInfo._dynprofit = 0;                                    // 设置浮动盈亏为0
		}
		else                                                          // 如果持仓数量不为0
		{
			bool isLong = decimal::gt(pInfo._volume, 0);             // 判断是否多头
			double price = isLong ? newTick->bidprice(0) : newTick->askprice(0);  // 获取对手盘价格（多头用买一价，空头用卖一价）

			WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
			double dynprofit = 0;                                     // 总浮动盈亏
			for (auto pit = pInfo._details.begin(); pit != pInfo._details.end(); pit++)  // 遍历持仓明细
			{
				
				DetailInfo& dInfo = *pit;                            // 获取明细信息
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);  // 计算明细盈亏
				if (dInfo._profit > 0)                                // 如果盈亏为正
					dInfo._max_profit = max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利
				else if (dInfo._profit < 0)                           // 如果盈亏为负
					dInfo._max_loss = min(dInfo._profit, dInfo._max_loss);  // 更新最大亏损

				dynprofit += dInfo._profit;                           // 累加浮动盈亏
			}

			pInfo._dynprofit = dynprofit;                             // 设置持仓浮动盈亏
		}
	}
}

/**
 * @brief 处理订单撮合
 * 
 * 1. 检查订单是否存在
 * 2. 根据错误率随机撤单
 * 3. 触发订单回报（如果首次处理）
 * 4. 获取最新tick数据
 * 5. 检查成交条件（限价单价格检查）
 * 6. 模拟成交（拆分数量，多次成交）
 * 7. 记录信号日志
 * 
 * @param localid 本地订单ID
 * @return 是否订单已完全成交（需要删除）
 */
bool HftMocker::procOrder(uint32_t localid)
{
	auto it = _orders.find(localid);                                  // 查找订单
	if (it == _orders.end())                                          // 如果订单不存在
		return false;                                                  // 返回false

	OrderInfoPtr ordInfo = it->second;                                // 获取订单信息

	//第一步,如果在撤单概率中,则执行撤单
	if(_error_rate>0 && genRand(10000)<=_error_rate)                  // 如果错误率大于0且随机数在错误率范围内
	{
		on_order(localid, ordInfo->_code, ordInfo->_isBuy, ordInfo->_total, ordInfo->_left, ordInfo->_price, true, ordInfo->_usertag);  // 触发订单回报（撤单）
		log_info("Random error order: {}", localid);                  // 记录日志
		return true;                                                   // 返回true（需要删除）
	}
	else if(!ordInfo->_proced_after_placed)                           // 如果下单后还没处理过
	{
		//如果下单以后，还没处理过，则触发on_order
		on_order(localid, ordInfo->_code, ordInfo->_isBuy, ordInfo->_total, ordInfo->_left, ordInfo->_price, false, ordInfo->_usertag);  // 触发订单回报（正常）
		ordInfo->_proced_after_placed = true;                         // 设置已处理标志
	}

	WTSTickData* curTick = stra_get_last_tick(ordInfo->_code);      // 获取最新tick数据
	if (curTick == NULL)                                              // 如果tick数据不存在
		return false;                                                  // 返回false

	double curPx = curTick->price();                                  // 获取当前价格
	double orderQty = ordInfo->_isBuy ? curTick->askqty(0) : curTick->bidqty(0);	//看对手盘的数量  // 获取对手盘数量（买入看卖一量，卖出看买一量）
	if (decimal::eq(orderQty, 0.0))                                    // 如果对手盘数量为0
		return false;                                                  // 返回false（无法成交）

	if (!_use_newpx)                                                  // 如果不使用新价格
	{
		curPx = ordInfo->_isBuy ? curTick->askprice(0) : curTick->bidprice(0);  // 使用对手盘价格（买入用卖一价，卖出用买一价）
		//if (curPx == 0.0)
		if(decimal::eq(curPx, 0.0))                                   // 如果对手盘价格为0
		{
			curTick->release();                                       // 释放tick数据
			return false;                                              // 返回false
		}
	}
	curTick->release();                                                // 释放tick数据

	//如果没有成交条件,则退出逻辑
	if(!decimal::eq(ordInfo->_price, 0.0))                            // 如果是限价单（价格不为0）
	{
		if(ordInfo->_isBuy && decimal::gt(curPx, ordInfo->_price))    // 如果是买单且当前价大于限价
		{
			//买单,但是当前价大于限价,不成交
			return false;                                              // 返回false（不成交）
		}

		if (!ordInfo->_isBuy && decimal::lt(curPx, ordInfo->_price))  // 如果是卖单且当前价小于限价
		{
			//卖单,但是当前价小于限价,不成交
			return false;                                              // 返回false（不成交）
		}
	}

	/*
	 *	下面就要模拟成交了
	 */
	double maxQty = min(orderQty, ordInfo->_left);                    // 计算最大可成交数量（取对手盘数量和剩余数量中的较小值）
	auto vols = splitVolume((uint32_t)maxQty);                        // 拆分数量（模拟大单拆分）
	for(uint32_t curQty : vols)                                       // 遍历拆分后的数量列表
	{
		on_trade(ordInfo->_localid, ordInfo->_code, ordInfo->_isBuy, curQty, curPx, ordInfo->_usertag);  // 触发成交回报

		ordInfo->_left -= curQty;                                     // 减少剩余数量
		on_order(localid, ordInfo->_code, ordInfo->_isBuy, ordInfo->_total, ordInfo->_left, ordInfo->_price, false, ordInfo->_usertag);  // 触发订单回报（更新剩余数量）

		double curPos = stra_get_position(ordInfo->_code);            // 获取当前持仓

		_sig_logs << _replayer->get_date() << "." << _replayer->get_raw_time() << "." << _replayer->get_secs() << ","
			<< (ordInfo->_isBuy ? "+" : "-") << curQty << "," << curPos << "," << curPx << std::endl;  // 记录信号日志
	}

	//if(ordInfo->_left == 0)
	if(decimal::eq(ordInfo->_left, 0.0))                              // 如果剩余数量为0（完全成交）
	{
		return true;                                                   // 返回true（需要删除）
	}

	return false;                                                      // 返回false（未完全成交）
}

/**
 * @brief 卖出订单（IHftStraCtx接口实现）
 * 
 * 1. 验证合约信息和数量
 * 2. 检查是否可以做空（如果不能做空，检查可用持仓）
 * 3. 创建订单信息
 * 4. 添加到订单映射表
 * 5. 提交委托回报任务
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param userTag 用户标签
 * @param flag 标志（未使用）
 * @param bForceClose 是否强制平仓（未使用）
 * @return 订单ID列表
 */
OrderIDs HftMocker::stra_sell(const char* stdCode, double price, double qty, const char* userTag, int flag /* = 0 */, bool bForceClose /* = false */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                             // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of %s", stdCode);  // 记录错误日志
		return OrderIDs();                                             // 返回空列表
	}

	if (decimal::le(qty, 0))                                          // 如果数量小于等于0
	{
		log_error("Entrust error: qty {} <= 0", qty);                 // 记录错误日志
		return OrderIDs();                                             // 返回空列表
	}

	//如果不能做空，则要看可用持仓
	if(!commInfo->canShort())                                        // 如果不能做空
	{
		double curPos = stra_get_position(stdCode, true);//只读可用持仓  // 获取可用持仓（只读有效持仓）
		if(decimal::gt(qty, curPos))                                  // 如果卖出数量大于可用持仓
		{
			log_error("No enough position of {} to sell", stdCode);   // 记录错误日志
			return OrderIDs();                                         // 返回空列表
		}
	}

	uint32_t localid = makeLocalOrderID();                           // 生成本地订单ID

	OrderInfoPtr order(new OrderInfo);                                // 创建订单信息
	order->_localid = localid;                                        // 设置本地订单ID
	strcpy(order->_code, stdCode);                                    // 设置合约代码
	strcpy(order->_usertag, userTag);                                 // 设置用户标签
	order->_isBuy = false;                                            // 设置为卖出
	order->_price = price;                                            // 设置订单价格
	order->_total = qty;                                              // 设置总数量
	order->_left = qty;                                               // 设置剩余数量

	{
		StdLocker<StdRecurMutex> lock(_mtx_ords);                    // 锁定订单互斥锁
		_orders[localid] = order;                                    // 添加到订单映射表
	}

	postTask([this, localid]() {                                     // 提交委托回报任务
		const OrderInfoPtr& ordInfo = _orders[localid];              // 获取订单信息
		on_entrust(localid, ordInfo->_code, true, "下单成功", ordInfo->_usertag);  // 触发委托回报（成功）
	});

	OrderIDs ids;                                                     // 订单ID列表
	ids.emplace_back(localid);                                        // 添加订单ID
	return ids;                                                       // 返回订单ID列表
}

/**
 * @brief 获取合约信息（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @return 合约信息指针
 */
WTSCommodityInfo* HftMocker::stra_get_comminfo(const char* stdCode)
{
	return _replayer->get_commodity_info(stdCode);                    // 返回合约信息
}

/**
 * @brief 获取原始合约代码（IHftStraCtx接口实现）
 * 
 * @param stdCode 标准化合约代码
 * @return 原始合约代码
 */
std::string HftMocker::stra_get_rawcode(const char* stdCode)
{
	return _replayer->get_rawcode(stdCode);                          // 返回原始合约代码
}

/**
 * @brief 获取K线数据（IHftStraCtx接口实现）
 * 
 * 解析周期字符串（如"m1"、"d1"），提取基础周期和倍数，获取K线切片
 * 
 * @param stdCode 合约代码
 * @param period 周期字符串（如"m1"、"d1"）
 * @param count 数量
 * @return K线切片指针
 */
WTSKlineSlice* HftMocker::stra_get_bars(const char* stdCode, const char* period, uint32_t count)
{
	thread_local static char basePeriod[2] = { 0 };                  // 线程局部静态变量，存储基础周期
	basePeriod[0] = period[0];                                         // 提取基础周期字符（如'm'、'd'）
	uint32_t times = 1;                                                // 默认倍数为1
	if (strlen(period) > 1)                                            // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);                         // 解析倍数（如"m5"中的5）

	return _replayer->get_kline_slice(stdCode, basePeriod, count, times);  // 返回K线切片
}

/**
 * @brief 获取Tick数据切片（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param count 数量
 * @return Tick数据切片指针
 */
WTSTickSlice* HftMocker::stra_get_ticks(const char* stdCode, uint32_t count)
{
	return _replayer->get_tick_slice(stdCode, count);                 // 返回Tick数据切片
}

/**
 * @brief 获取订单队列数据切片（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param count 数量
 * @return 订单队列数据切片指针
 */
WTSOrdQueSlice* HftMocker::stra_get_order_queue(const char* stdCode, uint32_t count)
{
	return _replayer->get_order_queue_slice(stdCode, count);          // 返回订单队列数据切片
}

/**
 * @brief 获取订单明细数据切片（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param count 数量
 * @return 订单明细数据切片指针
 */
WTSOrdDtlSlice* HftMocker::stra_get_order_detail(const char* stdCode, uint32_t count)
{
	return _replayer->get_order_detail_slice(stdCode, count);         // 返回订单明细数据切片
}

/**
 * @brief 获取逐笔成交数据切片（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param count 数量
 * @return 逐笔成交数据切片指针
 */
WTSTransSlice* HftMocker::stra_get_transaction(const char* stdCode, uint32_t count)
{
	return _replayer->get_transaction_slice(stdCode, count);          // 返回逐笔成交数据切片
}

/**
 * @brief 获取最新Tick数据（IHftStraCtx接口实现）
 * 
 * 1. 先从本地缓存的_ticks中查找
 * 2. 如果找到，增加引用计数并返回
 * 3. 否则从replayer获取最新tick
 * 
 * @param stdCode 合约代码
 * @return 最新Tick数据指针（需要调用者释放）
 */
WTSTickData* HftMocker::stra_get_last_tick(const char* stdCode)
{
	if (_ticks != NULL)                                                // 如果本地Tick缓存不为空
	{
		auto it = _ticks->find(stdCode);                               // 查找合约代码
		if (it != _ticks->end())                                       // 如果找到
		{
			WTSTickData* lastTick = (WTSTickData*)it->second;          // 获取Tick数据
			if (lastTick)                                              // 如果Tick数据不为空
				lastTick->retain();                                    // 增加引用计数
			return lastTick;                                           // 返回Tick数据
		}
	}

	return _replayer->get_last_tick(stdCode);                         // 从replayer获取最新tick
}

/**
 * @brief 获取持仓数量（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @param bOnlyValid 是否只返回可用持仓（扣除冻结持仓）
 * @param flag 标志（未使用）
 * @return 持仓数量
 */
double HftMocker::stra_get_position(const char* stdCode, bool bOnlyValid/* = false*/, int flag/* = 3*/)
{
	const PosInfo& pInfo = _pos_map[stdCode];                         // 获取持仓信息
	if (bOnlyValid)                                                    // 如果只返回可用持仓
	{
		//这里理论上，只有多头才会进到这里
		//其他地方要保证，空头持仓的话，_frozen要为0
		return pInfo._volume - pInfo._frozen;                          // 返回可用持仓（总持仓减去冻结持仓）
	}
	else
		return pInfo._volume;                                          // 返回总持仓
}

/**
 * @brief 获取持仓浮动盈亏（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @return 持仓浮动盈亏
 */
double HftMocker::stra_get_position_profit(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                                  // 查找持仓信息
	if (it == _pos_map.end())                                          // 如果未找到
		return 0.0;                                                    // 返回0

	const PosInfo& pInfo = it->second;                                 // 获取持仓信息
	return pInfo._dynprofit;                                           // 返回浮动盈亏
}

/**
 * @brief 获取持仓均价（IHftStraCtx接口实现）
 * 
 * 根据持仓明细计算加权平均开仓价格
 * 
 * @param stdCode 合约代码
 * @return 持仓均价
 */
double HftMocker::stra_get_position_avgpx(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                                  // 查找持仓信息
	if (it == _pos_map.end())                                          // 如果未找到
		return 0.0;                                                    // 返回0

	const PosInfo& pInfo = it->second;                                 // 获取持仓信息
	if (decimal::eq(pInfo._volume, 0.0))                              // 如果持仓为0
		return 0;                                                      // 返回0

	double amount = 0.0;                                               // 总金额
	for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历持仓明细
	{
		const DetailInfo& dInfo = *dit;                                // 获取明细信息
		amount += dInfo._price*dInfo._volume;                          // 累加金额（价格*数量）
	}

	return amount / pInfo._volume;                                     // 返回均价（总金额/总数量）
}

/**
 * @brief 获取当前价格（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 * @return 当前价格
 */
double HftMocker::stra_get_price(const char* stdCode)
{
	return _replayer->get_cur_price(stdCode);                          // 返回当前价格
}

/**
 * @brief 获取当前日期（IHftStraCtx接口实现）
 * 
 * @return 当前日期（YYYYMMDD格式）
 */
uint32_t HftMocker::stra_get_date()
{
	return _replayer->get_date();                                      // 返回当前日期
}

/**
 * @brief 获取当前时间（IHftStraCtx接口实现）
 * 
 * @return 当前时间（HHMMSS格式）
 */
uint32_t HftMocker::stra_get_time()
{
	return _replayer->get_raw_time();                                  // 返回当前时间
}

/**
 * @brief 获取当前秒数（IHftStraCtx接口实现）
 * 
 * @return 当前秒数（0-86399）
 */
uint32_t HftMocker::stra_get_secs()
{
	return _replayer->get_secs();                                     // 返回当前秒数
}

/**
 * @brief 订阅Tick数据（IHftStraCtx接口实现）
 * 
 * 1. 在本地记录订阅列表
 * 2. 向replayer注册订阅
 * 
 * @param stdCode 合约代码
 */
void HftMocker::stra_sub_ticks(const char* stdCode)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(stdCode);                                        // 添加到本地订阅列表

	_replayer->sub_tick(_context_id, stdCode);                        // 向replayer注册订阅
}

/**
 * @brief 订阅订单队列数据（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 */
void HftMocker::stra_sub_order_queues(const char* stdCode)
{
	_replayer->sub_order_queue(_context_id, stdCode);                // 向replayer注册订阅
}

/**
 * @brief 订阅订单明细数据（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 */
void HftMocker::stra_sub_order_details(const char* stdCode)
{
	_replayer->sub_order_detail(_context_id, stdCode);               // 向replayer注册订阅
}

/**
 * @brief 订阅逐笔成交数据（IHftStraCtx接口实现）
 * 
 * @param stdCode 合约代码
 */
void HftMocker::stra_sub_transactions(const char* stdCode)
{
	_replayer->sub_transaction(_context_id, stdCode);                 // 向replayer注册订阅
}

/**
 * @brief 记录信息日志（IHftStraCtx接口实现）
 * 
 * @param message 日志消息
 */
void HftMocker::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 记录信息级别日志
}

/**
 * @brief 记录调试日志（IHftStraCtx接口实现）
 * 
 * @param message 日志消息
 */
void HftMocker::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 记录调试级别日志
}

/**
 * @brief 记录警告日志（IHftStraCtx接口实现）
 * 
 * @param message 日志消息
 */
void HftMocker::stra_log_warn(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_WARN, message);  // 记录警告级别日志
}

/**
 * @brief 记录错误日志（IHftStraCtx接口实现）
 * 
 * @param message 日志消息
 */
void HftMocker::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 记录错误级别日志
}

/**
 * @brief 加载用户数据（IHftStraCtx接口实现）
 * 
 * @param key 数据键
 * @param defVal 默认值（如果键不存在）
 * @return 数据值（如果不存在返回默认值）
 */
const char* HftMocker::stra_load_user_data(const char* key, const char* defVal /*= ""*/)
{
	auto it = _user_datas.find(key);                                   // 查找用户数据
	if (it != _user_datas.end())                                       // 如果找到
		return it->second.c_str();                                     // 返回数据值

	return defVal;                                                      // 返回默认值
}

/**
 * @brief 保存用户数据（IHftStraCtx接口实现）
 * 
 * @param key 数据键
 * @param val 数据值
 */
void HftMocker::stra_save_user_data(const char* key, const char* val)
{
	_user_datas[key] = val;                                            // 保存用户数据
	_ud_modified = true;                                               // 设置修改标志
}

/**
 * @brief 导出回测结果到文件
 * 
 * 1. 创建输出目录
 * 2. 导出成交记录（trades.csv）
 * 3. 导出平仓记录（closes.csv）
 * 4. 导出资金记录（funds.csv）
 * 5. 导出信号记录（signals.csv）
 * 6. 导出持仓记录（positions.csv）
 * 7. 导出用户数据（ud_*.json）
 * 
 */
void HftMocker::dump_outputs()
{
	std::string folder = WtHelper::getOutputDir();                    // 获取输出目录
	folder += _name;                                                   // 拼接策略名称
	folder += "/";                                                     // 添加路径分隔符
	boost::filesystem::create_directories(folder.c_str());            // 创建目录（如果不存在）

	std::string filename = folder + "trades.csv";                    // 成交记录文件名
	std::string content = "code,time,direct,action,price,qty,fee,usertag\n";  // CSV表头
	content += _trade_logs.str();                                     // 追加成交记录内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "closes.csv";                                 // 平仓记录文件名
	content = "code,direct,opentime,openprice,closetime,closeprice,qty,profit,maxprofit,maxloss,totalprofit,entertag,exittag\n";  // CSV表头
	content += _close_logs.str();                                     // 追加平仓记录内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件


	filename = folder + "funds.csv";                                  // 资金记录文件名
	content = "date,closeprofit,positionprofit,dynbalance,fee\n";     // CSV表头
	content += _fund_logs.str();                                      // 追加资金记录内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件


	filename = folder + "signals.csv";                                // 信号记录文件名
	content = "time, action, position, price\n";                     // CSV表头
	content += _sig_logs.str();                                        // 追加信号记录内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "positions.csv";                               // 持仓记录文件名
	content = "date,code,volume,closeprofit,dynprofit\n";             // CSV表头
	if (!_pos_logs.str().empty()) content += _pos_logs.str();        // 追加持仓记录内容（如果不为空）
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	{
		rj::Document root(rj::kObjectType);                           // 创建JSON文档对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取分配器
		for (auto it = _user_datas.begin(); it != _user_datas.end(); it++)  // 遍历用户数据
		{
			root.AddMember(rj::Value(it->first.c_str(), allocator), rj::Value(it->second.c_str(), allocator), allocator);  // 添加键值对
		}

		filename = folder;                                             // 用户数据文件名
		filename += "ud_";                                             // 前缀
		filename += _name;                                             // 策略名称
		filename += ".json";                                           // 扩展名

		rj::StringBuffer sb;                                           // 字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);               // JSON写入器
		root.Accept(writer);                                           // 写入JSON
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 写入文件
	}
}

/**
 * @brief 记录成交日志
 * 
 * @param stdCode 合约代码
 * @param isLong 是否多头
 * @param isOpen 是否开仓
 * @param curTime 当前时间
 * @param price 成交价格
 * @param qty 成交数量
 * @param fee 手续费
 * @param userTag 用户标签
 */
void HftMocker::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, double fee, const char* userTag/* = ""*/)
{
	_trade_logs << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE")
		<< "," << price << "," << qty << "," << fee << "," << userTag << "\n";  // 写入成交记录（CSV格式）
}

/**
 * @brief 记录平仓日志
 * 
 * @param stdCode 合约代码
 * @param isLong 是否多头
 * @param openTime 开仓时间
 * @param openpx 开仓价格
 * @param closeTime 平仓时间
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 盈亏
 * @param maxprofit 最大盈利
 * @param maxloss 最大亏损
 * @param totalprofit 累计盈亏
 * @param enterTag 开仓标签
 * @param exitTag 平仓标签
 */
void HftMocker::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double maxprofit, double maxloss,
	double totalprofit /* = 0 */, const char* enterTag/* = ""*/, const char* exitTag/* = ""*/)
{
	_close_logs << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
		<< "," << closeTime << "," << closepx << "," << qty << "," << profit << "," << maxprofit << "," << maxloss << ","
		<< totalprofit << "," << enterTag << "," << exitTag << "\n";  // 写入平仓记录（CSV格式）
}

/**
 * @brief 设置持仓（直接设置目标持仓）
 * 
 * 1. 获取当前持仓信息
 * 2. 如果目标持仓等于当前持仓，直接返回
 * 3. 计算持仓变化
 * 4. 如果持仓方向一致（都是多头或都是空头），增加持仓明细
 * 5. 如果持仓方向不一致，先平仓再开仓
 * 6. 处理T+1合约的冻结持仓
 * 7. 计算手续费和盈亏
 * 
 * @param stdCode 合约代码
 * @param qty 目标持仓数量
 * @param price 成交价格（如果为0，使用当前价格）
 * @param userTag 用户标签
 */
void HftMocker::do_set_position(const char* stdCode, double qty, double price /* = 0.0 */, const char* userTag /*= ""*/)
{
	PosInfo& pInfo = _pos_map[stdCode];                               // 获取持仓信息
	double curPx = price;                                              // 当前价格
	if (decimal::eq(price, 0.0))                                       // 如果价格未指定
		curPx = _price_map[stdCode];                                   // 使用当前价格映射
	uint64_t curTm = (uint64_t)_replayer->get_date() * 1000000000 + (uint64_t)_replayer->get_min_time()*100000 + _replayer->get_secs();  // 计算当前时间戳
	uint32_t curTDate = _replayer->get_trading_date();                 // 获取交易日期

	//手数相等则不用操作了
	if (decimal::eq(pInfo._volume, qty))                               // 如果目标持仓等于当前持仓
		return;                                                         // 直接返回

	log_debug("[{:04d}.{:05d}] {} position updated: {} -> {}", _replayer->get_min_time(), _replayer->get_secs(), stdCode, pInfo._volume, qty);  // 记录调试日志

	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                              // 如果合约信息不存在
		return;                                                         // 直接返回

	//成交价
	double trdPx = curPx;                                              // 成交价格

	double diff = qty - pInfo._volume;                                 // 计算持仓变化
	bool isBuy = decimal::gt(diff, 0.0);                               // 是否买入（持仓增加）
	if (decimal::gt(pInfo._volume*diff, 0))//当前持仓和仓位变化方向一致, 增加一条明细, 增加数量即可  // 如果持仓方向一致（都是正数或都是负数）
	{
		pInfo._volume = qty;                                           // 更新持仓数量
		//如果T+1，则冻结仓位要增加
		if (commInfo->isT1())                                          // 如果是T+1合约
		{
			//ASSERT(diff>0);
			pInfo._frozen += diff;                                     // 增加冻结持仓
			log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录调试日志
		}

		DetailInfo dInfo;                                              // 创建持仓明细
		dInfo._long = decimal::gt(qty, 0);                             // 是否多头
		dInfo._price = trdPx;                                          // 开仓价格
		dInfo._volume = abs(diff);                                     // 数量
		dInfo._opentime = curTm;                                       // 开仓时间
		dInfo._opentdate = curTDate;                                   // 开仓日期
		strcpy(dInfo._usertag, userTag);                                // 用户标签
		pInfo._details.emplace_back(dInfo);                            // 添加到持仓明细列表

		double fee = _replayer->calc_fee(stdCode, trdPx, abs(diff), 0);  // 计算手续费（开仓）
		_fund_info._total_fees += fee;                                 // 累加总手续费

		log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(diff), fee, userTag);  // 记录成交日志
	}
	else
	{//持仓方向和仓位变化方向不一致,需要平仓  // 如果持仓方向不一致（需要平仓）
		double left = abs(diff);                                       // 剩余需要平仓的数量

		pInfo._volume = qty;                                           // 更新持仓数量
		if (decimal::eq(pInfo._volume, 0))                             // 如果持仓为0
			pInfo._dynprofit = 0;                                      // 浮动盈亏清零
		uint32_t count = 0;                                            // 需要清理的明细数量
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历持仓明细
		{
			DetailInfo& dInfo = *it;                                   // 获取明细信息
			double maxQty = min(dInfo._volume, left);                 // 本次平仓数量（取明细数量和剩余数量的较小值）
			if (decimal::eq(maxQty, 0))                                // 如果平仓数量为0
				continue;                                              // 跳过

			double maxProf = dInfo._max_profit * maxQty / dInfo._volume;  // 按比例计算最大盈利
			double maxLoss = dInfo._max_loss * maxQty / dInfo._volume;  // 按比例计算最大亏损

			dInfo._volume -= maxQty;                                   // 减少明细数量
			left -= maxQty;                                            // 减少剩余数量

			if (decimal::eq(dInfo._volume, 0))                         // 如果明细数量为0
				count++;                                                // 增加清理计数

			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算盈亏（价格差*数量*合约乘数）
			if (!dInfo._long)                                          // 如果是空头
				profit *= -1;                                          // 盈亏取反
			pInfo._closeprofit += profit;                              // 累加平仓盈亏
			pInfo._dynprofit = pInfo._dynprofit*dInfo._volume / (dInfo._volume + maxQty);//浮盈也要做等比缩放  // 等比缩放浮动盈亏
			_fund_info._total_profit += profit;                        // 累加总盈亏

			double fee = _replayer->calc_fee(stdCode, trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费（平仓，T+0为2，T+1为1）
			_fund_info._total_fees += fee;                             // 累加总手续费
			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, trdPx, maxQty, fee, userTag);  // 记录成交日志（平仓）
			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, maxProf, maxLoss, pInfo._closeprofit, dInfo._usertag, userTag);  // 记录平仓日志

			if (left == 0)                                             // 如果剩余数量为0
				break;                                                 // 跳出循环
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)                                              // 清理已平仓完的明细
		{
			auto it = pInfo._details.begin();                          // 获取第一个明细
			pInfo._details.erase(it);                                  // 删除明细
			count--;                                                   // 减少计数
		}

		//最后,如果还有剩余的,则需要反手了
		if (left > 0)                                                  // 如果还有剩余数量（需要反手）
		{
			left = left * qty / abs(qty);                              // 调整剩余数量符号（与目标持仓方向一致）

			//如果T+1，则冻结仓位要增加
			if (commInfo->isT1())                                       // 如果是T+1合约
			{
				pInfo._frozen += left;                                 // 增加冻结持仓
				log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录调试日志
			}

			DetailInfo dInfo;                                          // 创建持仓明细
			dInfo._long = decimal::gt(qty, 0);                        // 是否多头
			dInfo._price = trdPx;                                      // 开仓价格
			dInfo._volume = abs(left);                                 // 数量
			dInfo._opentime = curTm;                                   // 开仓时间
			dInfo._opentdate = curTDate;                               // 开仓日期
			strcpy(dInfo._usertag, userTag);                           // 用户标签
			pInfo._details.emplace_back(dInfo);                        // 添加到持仓明细列表

			//这里还需要写一笔成交记录
			double fee = _replayer->calc_fee(stdCode, trdPx, abs(left), 0);  // 计算手续费（开仓）
			_fund_info._total_fees += fee;                             // 累加总手续费
			log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(left), fee, userTag);  // 记录成交日志（开仓）
		}
	}
}
