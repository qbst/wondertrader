/*!
 * \file WtTWapExeUnit.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtTWapExeUnit时间加权平均价格执行单元类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了WtTWapExeUnit类的所有成员函数，该类用于实现TWAP（Time-Weighted Average Price）算法，
 * 将订单在指定时间段内均匀分配执行，以实现接近时间加权平均价格的效果。
 * 
 * 实现要点：
 * 1. TWAP算法：将订单在指定时间段内均匀分配执行
 * 2. 分批执行：根据总执行次数将订单分成多批执行
 * 3. 尾部处理：在最后一段时间内集中执行剩余订单，确保完成执行
 * 4. 订单管理：跟踪订单状态，支持超时撤单
 * 5. 价格控制：根据价格模式和价格偏移计算订单价格
 */
#include "WtTWapExeUnit.h"  // WtTWapExeUnit类定义

#include "../Share/TimeUtils.hpp"  // 时间工具函数
#include "../Includes/WTSVariant.hpp"  // 变体类型定义
#include "../Includes/WTSContractInfo.hpp"  // 合约信息定义
#include "../Share/decimal.h"  // 精确小数计算
#include "../Share/fmtlib.h"  // 格式化库
/***---begin---23.5.18---zhaoyk***/
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息定义
/***---end---23.5.18---zhaoyk***/
#include <math.h>  // 数学函数库


/**
 * @brief 工厂名称外部声明
 * 
 * 声明FACT_NAME常量，该常量在WtExeFact.cpp中定义。
 */
extern const char* FACT_NAME;


/**
 * @brief WtTWapExeUnit构造函数实现
 * 
 * 创建TWAP执行单元实例，初始化所有成员变量为默认值。
 */
WtTWapExeUnit::WtTWapExeUnit()
	: _last_tick(NULL)  // 上一笔行情数据指针初始化为NULL
	, _comm_info(NULL)  // 合约信息指针初始化为NULL
	, _ord_sticky(0)  // 挂单时限初始化为0（不超时）
	, _cancel_cnt(0)  // 在途撤单量初始化为0
	, _channel_ready(false)  // 交易通道就绪标志初始化为false
	, _last_fire_time(0)  // 上次已执行的时间初始化为0
	, _fired_times(0)  // 已执行次数初始化为0
	, _total_times(0)  // 总执行次数初始化为0
	, _total_secs(0)  // 执行总时间初始化为0
	, _price_mode(0)  // 价格模式初始化为0（最新价）
	, _price_offset(0)  // 价格偏移跳数初始化为0
	, _target_pos(0)  // 目标仓位初始化为0
	, _cancel_times(0)  // 撤单次数初始化为0
	, _begin_time(0)  // 开始时间初始化为0
	, _end_time(0)  // 结束时间初始化为0
	, _last_place_time(0)  // 上个下单时间初始化为0
	, _last_tick_time(0)  // 上个tick时间初始化为0
	, isCanCancel{ true }  // 是否可撤单标志初始化为true
{
	// 构造函数体为空，所有初始化工作都在初始化列表中完成
}

/**
 * @brief WtTWapExeUnit析构函数实现
 * 
 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
 */
WtTWapExeUnit::~WtTWapExeUnit()
{
	if (_last_tick)  // 如果行情数据指针不为空
		_last_tick->release();  // 释放行情数据（减少引用计数）

	if (_comm_info)  // 如果合约信息指针不为空
		_comm_info->release();  // 释放合约信息（减少引用计数）
}

/**
 * @brief 获取真实目标仓位
 * 
 * 将目标仓位转换为真实的目标仓位值。
 * 如果目标仓位为DBL_MAX（清仓标志），则返回0。
 * 
 * @param _target 目标仓位（DBL_MAX表示清仓）
 * @return double 真实目标仓位（0或目标仓位值）
 */
inline double get_real_target(double _target) {
	if (_target == DBL_MAX)  // 如果目标仓位为DBL_MAX（清仓标志）
		return 0;  // 返回0（清仓时目标仓位为0）

	return _target;  // 否则返回目标仓位值
}

/**
 * @brief 计算执行时间
 * 
 * 根据开始时间和结束时间计算执行总时间（单位：秒）。
 * 
 * @param begintime 开始时间（格式：HHMM，如1000表示10:00）
 * @param endtime 结束时间（格式：HHMM，如1030表示10:30）
 * @return uint32_t 执行总时间（单位：秒）
 */
inline uint32_t calTmSecs(uint32_t begintime, uint32_t endtime)  // 计算执行时间：s
{
	// 将时间格式（HHMM）转换为秒数：HH * 3600 + MM * 60
	return ((endtime / 100) * 3600 + (endtime % 100) * 60) - ((begintime / 100) * 3600 + (begintime % 100) * 60);
}

/**
 * @brief 检查是否清仓
 * 
 * 检查目标仓位是否为清仓指令（DBL_MAX）。
 * 
 * @param target 目标仓位
 * @return bool true表示清仓，false表示不清仓
 */
inline bool is_clear(double target)
{
	return (target == DBL_MAX);  // 如果目标仓位为DBL_MAX，则表示清仓
}

/**
 * @brief 获取所属执行器工厂名称实现
 * 
 * 返回创建该执行单元的工厂名称。
 * 
 * @return const char* 返回工厂名称字符串（"WtExeFact"）
 */
const char* WtTWapExeUnit::getFactName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 获取执行单元名称实现
 * 
 * 返回执行单元的名称，用于标识和管理。
 * 
 * @return const char* 返回执行单元名称字符串（"WtTWapExeUnit"）
 */
const char* WtTWapExeUnit::getName()
{
	return "WtTWapExeUnit";  // 返回执行单元名称
}

/**
 * @brief 初始化执行单元实现
 * 
 * 初始化执行单元，加载配置参数，获取合约信息和交易时段信息。
 * 
 * @param ctx 执行单元运行环境指针，提供合约信息、交易时段信息等
 * @param stdCode 管理的合约代码（标准格式）
 * @param cfg 配置对象指针，包含执行单元的各种配置参数：
 *   - ord_sticky：挂单时限（单位：秒），订单挂单后超过此时间未成交则自动撤单
 *   - begin_time：开始时间（格式：HHMM，如1000表示10:00）
 *   - end_time：结束时间（格式：HHMM，如1030表示10:30）
 *   - total_secs：执行总时间（单位：秒），从开始时间到结束时间的总时长（可选，如果不提供则根据begin_time和end_time计算）
 *   - tail_secs：执行尾部时间（单位：秒），在最后一段时间内集中执行剩余订单
 *   - total_times：总执行次数，将订单分成多少批执行
 *   - price_mode：价格模式（0-最新价，1-最优价，2-对手价）
 *   - price_offset：挂单价格偏移（相对于基准价格的偏移，买入+偏移，卖出-偏移）
 *   - lots：单次发单手数（每次下单的数量）
 *   - minopenlots：最小开仓数量（可选，默认1）
 */
void WtTWapExeUnit::init(ExecuteContext* ctx, const char* stdCode, WTSVariant* cfg)
{
	ExecuteUnit::init(ctx, stdCode, cfg);  // 调用基类的初始化方法，完成基础初始化

	_comm_info = ctx->getCommodityInfo(stdCode);  // 获取品种参数（合约信息）
	if (_comm_info)  // 如果合约信息指针不为空
		_comm_info->retain();  // 增加引用计数，防止被释放
	_sess_info = ctx->getSessionInfo(stdCode);  // 获取交易时间模板信息（交易时段信息）
	if (_sess_info)  // 如果交易时段信息指针不为空
		_sess_info->retain();  // 增加引用计数，防止被释放
	_ord_sticky = cfg->getUInt32("ord_sticky");  // 挂单时限（单位：秒），订单挂单后超过此时间未成交则自动撤单
	_begin_time = cfg->getUInt32("begin_time");  // 开始时间（格式：HHMM，如1000表示10:00）
	_end_time = cfg->getUInt32("end_time");  // 结束时间（格式：HHMM，如1030表示10:30）
	_total_secs = cfg->getUInt32("total_secs");  // 执行总时间（单位：秒），从开始时间到结束时间的总时长（可选）
	_tail_secs = cfg->getUInt32("tail_secs");  // 执行尾部时间（单位：秒），在最后一段时间内集中执行剩余订单
	_total_times = cfg->getUInt32("total_times");  // 总执行次数，将订单分成多少批执行
	_price_mode = cfg->getUInt32("price_mode");  // 价格模式：0-最新价，1-最优价，2-对手价
	_price_offset = cfg->getUInt32("price_offset");  // 挂单价格偏移（相对于基准价格的偏移，买入+偏移，卖出-偏移）
	_order_lots = cfg->getDouble("lots");  // 单次发单手数（每次下单的数量）
	if (cfg->has("minopenlots"))  // 如果配置中有最小开仓数量参数
		_min_open_lots = cfg->getDouble("minopenlots");  // 最小开仓数量（开仓时，如果差量小于此值则不执行）
	_total_secs = calTmSecs(_begin_time, _end_time);  // 执行总时间：秒（根据开始时间和结束时间计算，覆盖配置中的total_secs）
	_fire_span = (_total_secs - _tail_secs) / _total_times;  // 单次发单时间间隔（单位：毫秒），要去掉尾部时间计算，这样的话，最后剩余的数量就有一个兜底发单的机制了

	ctx->writeLog(fmt::format("执行单元WtTWapExeUnit[{}] 初始化完成,订单超时 {} 秒,执行时限 {} 秒,收尾时间 {} 秒,间隔时间 {} 秒", stdCode, _ord_sticky, _total_secs, _tail_secs, _fire_span).c_str());  // 记录初始化日志
}

/**
 * @brief 订单回报处理实现
 * 
 * 当订单状态发生变化时调用此函数，更新订单状态，处理撤单等操作。
 * 
 * @param localid 本地订单ID（下单时返回的订单ID）
 * @param stdCode 合约代码
 * @param isBuy true表示买入订单，false表示卖出订单
 * @param leftover 剩余未成交数量
 * @param price 委托价格
 * @param isCanceled 是否已撤销，true表示已撤销，false表示未撤销
 */
void WtTWapExeUnit::on_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled)
{
	if (!_orders_mon.has_order(localid))  // 如果没有对应订单，直接返回（可能是其他执行单元的订单）
		return;

	if (isCanceled || leftover == 0)  // 如果订单已撤销或剩余订单为0（全部成交）
	{
		_orders_mon.erase_order(localid);  // 从订单管理器中删除该订单

		if (_cancel_cnt > 0)  // 如果在途撤单量大于0
			_cancel_cnt--;  // 减少在途撤单量
		
		_ctx->writeLog(fmt::format("Order {} updated cancelcnt -> {}", localid, _cancel_cnt).c_str());  // 记录日志
	}
	/***---begin---23.5.19---zhaoyk***/
	if (leftover == 0 && !isCanceled)  // 如果订单全部成交（剩余为0且未撤销）
	{
		_cancel_times = 0;  // 重置撤单次数（订单已成交，不需要继续撤单）
		_ctx->writeLog(fmtutil::format("Order {} has filled", localid));  // 记录日志：订单已成交
	}
	/***---end---23.5.19---zhaoyk***/
	// 如果全部订单已撤销，这个时候一般是遇到要超时撤单（挂单超时）
	if (isCanceled && _cancel_cnt == 0)  // 如果订单已撤销 && 在途撤单量为0
	{
		double realPos = _ctx->getPosition(stdCode);  // 获取当前仓位
		if (!decimal::eq(realPos, _this_target))  // 如果真实仓位和本轮目标仓位不相等时候
		{
			/***---begin---23.5.22---zhaoyk***/
			_ctx->writeLog(fmtutil::format("Order {} of {} canceled, re_fire will be done", localid, stdCode));  // 记录日志
			_cancel_times++;  // 增加撤单次数统计
			
			// 撤单以后重发，一般是加点重发；对最小下单量的校验
			//fire_at_once(_this_target - realPos);  // 原代码：直接执行剩余差量
			fire_at_once(max(_min_open_lots, _this_target - realPos));  // 立即执行剩余差量（至少为最小开仓数量）
			/***---end---23.5.22---zhaoyk***/
		}
	}
	/***---begin---23.5.22---zhaoyk***/
	// 存在（isCanceled && _cancel_cnt != 0）的情况，bug需要返回检查
	if (isCanceled && _cancel_cnt != 0)  // 如果订单已撤销 && 在途撤单量不为0（一般出现问题，需要返回检查）
	{
		_ctx->writeLog(fmtutil::format("Order {} of {}  hasn't canceled, error will be return ", localid, stdCode));  // 记录日志（错误信息）
		return;  // 直接返回，不做任何处理
	}
	/***---end---23.5.22---zhaoyk***/
} 

/**
 * @brief 交易通道就绪回调实现
 * 
 * 当交易通道连接成功并准备就绪时调用此函数，可以开始下单。
 * 该函数会检查是否有未管理的订单（可能是上次启动时的未完成单或外部挂单），
 * 如果有则自动撤单，然后触发执行计算。
 */
void WtTWapExeUnit::on_channel_ready()
{
	_channel_ready = true;  // 设置交易通道就绪标志为true
	double undone = _ctx->getUndoneQty(_code.c_str());  // 获取未完成数量（从交易通道获取）
	/***---begin---23.5.18---zhaoyk***/
	if (!decimal::eq(undone, 0) && !_orders_mon.has_order())  // 如果未完成单不为0，而订单管理器中没有订单
	//if (undone != 0 && !_orders_mon.has_order())  // 原代码（使用!=比较，可能有问题）
	/***---end---23.5.18---zhaoyk***/
	{
		// 这说明有未完成单不在监控之中，先撤掉
		_ctx->writeLog(fmt::format("{} unmanaged orders of {}, cancel all", undone, _code).c_str());  // 记录日志

		bool isBuy = (undone > 0);  // 根据未完成数量的正负判断方向（正数表示买入，负数表示卖出）
		OrderIDs ids = _ctx->cancel(_code.c_str(), isBuy);  // 根据方向撤单，返回撤单的订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime());  // 将撤单的订单ID推入订单管理器
		_cancel_cnt += ids.size();  // 增加在途撤单量

		_ctx->writeLog(fmt::format("Unmanaged order updated cancelcnt to {}", _cancel_cnt).c_str());  // 记录日志
	}
	/***---begin---23.5.18---zhaoyk***/
	else if (decimal::eq(undone, 0) && _orders_mon.has_order())  // 如果未完成单为0，但是订单管理器中有订单
	{
		/*
		 * By Wesley @ 2021.12.13
		 * 如果未完成单为0，但是订单管理器中有订单
		 * 说明订单管理器中的订单是错单，需要清理掉，不然超时撤单就会出错
		 * 这种情况，一般是断线重连以后，之前下出去的订单，并没有真正发送到柜台
		 * 所以这里需要清理掉本地订单
		 */
		_ctx->writeLog(fmtutil::format("Local orders of {} not confirmed in trading channel, clear all", _code.c_str()));  // 记录日志
		_orders_mon.clear_orders();  // 清空订单管理器中的所有订单
	}
	else  // 其他情况（未完成单和订单管理器状态一致，参考Minimpactunit）
	{
		_ctx->writeLog(fmtutil::format("Unrecognized condition while channle ready,{:.2f} live orders of {} exists,local orders {}exist", undone, _code.c_str(), _orders_mon.has_order() ? "" : "not"));  // 记录日志（警告信息）
	}
	/***---end---23.5.18---zhaoyk***/
	do_calc();  // 触发执行计算，开始下单
}

/**
 * @brief 交易通道丢失回调实现
 * 
 * 当交易通道断开时调用此函数，停止下单操作。
 * 当前版本该函数为空，不做任何处理。
 */
void WtTWapExeUnit::on_channel_lost()
{
	// 交易通道丢失时，停止下单操作（当前版本不做任何处理）
}

/**
 * @brief Tick数据回调实现
 * 
 * 当有新的Tick数据时调用此函数，更新最新行情，触发执行计算。
 * 该函数会检查订单超时，自动撤单，然后根据TWAP算法触发执行计算。
 * 
 * @param newTick 最新的Tick数据指针，包含最新价格、成交量、持仓量等信息
 */
void WtTWapExeUnit::on_tick(WTSTickData* newTick)
{
	if (newTick == NULL || _code.compare(newTick->code()) != 0)  // 如果Tick数据为空或合约代码不匹配
		return;  // 直接返回，不处理

	bool isFirstTick = false;  // 是否是第一笔tick标志
	// 如果原来的tick不为空，则要释放掉（减少引用计数）
	if (_last_tick)  // 如果上一笔行情数据指针不为空
	{
		_last_tick->release();  // 释放上一笔行情数据
	}
	/***---begin---23.5.18---zhaoyk***/
	else  // 如果上一笔行情数据为空（第一次收到行情）
	{
		isFirstTick = true;  // 设置第一笔tick标志为true
		// 如果行情时间不在交易时间，这种情况一般是集合竞价的行情进来，下单会失败，所以直接过滤掉这笔行情
		if (_sess_info != NULL && !_sess_info->isInTradingTime(newTick->actiontime() / 100000))  // 如果不在交易时间内
			return;  // 直接返回，不处理
	}
	/***---end---23.5.18---zhaoyk***/

	// 新的tick数据，要保留（增加引用计数）
	_last_tick = newTick;  // 保存新的行情数据指针
	_last_tick->retain();  // 增加引用计数，防止被释放

	/*
	 * 这里可以考虑一下
	 * 如果写的上一次丢出去的单子不够达到目标仓位
	 * 那么在新的行情数据进来的时候可以再次触发核心逻辑
	 */

	if (isFirstTick)  // 如果是第一笔tick，则检查目标仓位，不符合则下单
	{
		double newVol = _target_pos;  // 获取目标仓位
		const char* stdCode = _code.c_str();  // 获取合约代码字符串
		double undone = _ctx->getUndoneQty(stdCode);  // 获取未完成订单数量
		double realPos = _ctx->getPosition(stdCode);  // 获取当前仓位
		if (!decimal::eq(newVol, undone + realPos))  // 如果仓位变化要交易（目标仓位 != 未完成 + 当前仓位）
		{
			do_calc();  // 触发执行计算
		}
	}
	else  // 如果不是第一笔tick
	{
		uint64_t now = TimeUtils::getLocalTimeNow();  // 获取当前时间（毫秒时间戳）
		bool hasCancel = false;  // 是否有撤单标志
		if (_ord_sticky != 0 && _orders_mon.has_order())  // 如果挂单时限不为0 && 有订单
		{			
			// 检查订单是否超时，如果超时则撤单
			_orders_mon.check_orders(_ord_sticky, now, [this, &hasCancel](uint32_t localid) {  // 遍历所有订单，检查是否超时
				if (_ctx->cancel(localid))  // 如果撤单成功
				{
					_cancel_cnt++;  // 增加在途撤单量
					_ctx->writeLog(fmt::format("Order expired, cancelcnt updated to {}", _cancel_cnt).c_str());  // 记录日志：订单过期，撤单量更新
					hasCancel = true;  // 设置撤单标志为true
				}
			});
		}

		if (!hasCancel && (now - _last_fire_time >= _fire_span * 1000))  // 如果没有撤单 && 距离上次执行时间 >= 发单间隔
		{
			do_calc();  // 触发执行计算，根据TWAP算法执行
		}
	}
}

/**
 * @brief 成交回报处理实现
 * 
 * 当订单有成交回报时调用此函数，更新持仓和未完成订单数量。
 * 注意：该函数不触发重新计算，因为成交回报会在on_tick中触发重新计算。
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy true表示买入成交，false表示卖出成交
 * @param vol 成交数量（这里没有正负，通过isBuy确定买入还是卖出）
 * @param price 成交价格
 */
void WtTWapExeUnit::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	// 不用触发，这里在ontick里触发（成交回报会在on_tick中触发重新计算）
}

/**
 * @brief 下单结果回报处理实现
 * 
 * 当下单请求收到回报时调用此函数，处理下单成功或失败的情况。
 * 如果下单失败，则从订单管理器中删除该订单，并触发重新计算。
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param bSuccess 是否成功，true表示下单成功，false表示下单失败
 * @param message 返回消息（如果失败，包含失败原因）
 */
void WtTWapExeUnit::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	if (!bSuccess)  // 如果下单失败
	{
		// 如果不是我发出去的订单，我就不管了
		if (!_orders_mon.has_order(localid))  // 如果订单管理器中不存在该订单
			return;  // 直接返回，不做任何处理

		_orders_mon.erase_order(localid);  // 从订单管理器中删除该订单

		do_calc();  // 触发重新计算，重新下单
	}
}

/**
 * @brief 立即执行指定数量实现
 * 
 * 立即下单执行指定数量的订单，用于尾部时间集中执行剩余订单或撤单后重新下单。
 * 
 * @param qty 要执行的订单数量（正数表示买入，负数表示卖出）
 */
void WtTWapExeUnit::fire_at_once(double qty)
{
	if (decimal::eq(qty, 0))  // 如果要执行的数量为0
		return;  // 直接返回，不做任何处理

	_last_tick->retain();  // 增加行情数据引用计数（防止被释放）
	WTSTickData* curTick = _last_tick;  // 保存当前tick数据指针（用于后续释放）
	const char* code = _code.c_str();  // 获取合约代码字符串
	uint64_t now = TimeUtils::getLocalTimeNow();  // 获取当前时间（毫秒时间戳）
	bool isBuy = decimal::gt(qty, 0);  // 判断买卖方向（数量 > 0，则为买入）
	double targetPx = 0;  // 目标价格
	// 根据价格模式设置，确定委托基准价格：0-最新价，1-最优价，2-对手价
	if (_price_mode == 0)  // 如果价格模式为最新价（0）
	{
		targetPx = curTick->price();  // 目标价格用最新成交价
	}
	else if (_price_mode == 1)  // 如果价格模式为最优价（1）
	{
		targetPx = isBuy ? curTick->bidprice(0) : curTick->askprice(0);  // 买入用买一价，卖出用卖一价
	}
	else  // if(_price_mode == 2) 如果价格模式为对手价（2）
	{
		targetPx = isBuy ? curTick->askprice(0) : curTick->bidprice(0);  // 买入用卖一价，卖出用买一价
	}

	/***---begin---23.5.22---zhaoyk***/
	targetPx += _comm_info->getPriceTick() * _cancel_times * (isBuy ? 1 : -1);  // 增加价格偏移（根据撤单次数调整价格，用于追单）
	/***---end---23.5.22---zhaoyk***/
	// 检查涨跌停价
	isCanCancel = true;  // 是否可撤单标志（默认true）
	if (isBuy && !decimal::eq(_last_tick->upperlimit(), 0) && decimal::gt(targetPx, _last_tick->upperlimit()))  // 如果买入价格超过涨停价
	{
		_ctx->writeLog(fmt::format("Buy price {} of {} modified to upper limit price", targetPx, _code.c_str(), _last_tick->upperlimit()).c_str());  // 记录日志：买入价改为上限价
		targetPx = _last_tick->upperlimit();  // 买入价格修正为涨停价
		isCanCancel = false;  // 如果价格被修正为涨跌停价，订单不可撤销（涨跌停价的挂单不能撤单）
	}
	if (isBuy != 1 && !decimal::eq(_last_tick->lowerlimit(), 0) && decimal::lt(targetPx, _last_tick->lowerlimit()))  // 如果卖出价格低于跌停价
	{
		_ctx->writeLog(fmt::format("Sell price {} of {} modified to lower limit price", targetPx, _code.c_str(), _last_tick->lowerlimit()).c_str());  // 记录日志：卖出价改为下限价
		targetPx = _last_tick->lowerlimit();  // 卖出价格修正为跌停价
		isCanCancel = false;  // 如果价格被修正为涨跌停价，订单不可撤销（涨跌停价的挂单不能撤单）
	}

	OrderIDs ids;  // 订单ID列表
	if (qty > 0)  // 如果要执行的数量为正数（买入）
		ids = _ctx->buy(code, targetPx, abs(qty));  // 提交买入订单，返回订单ID列表
	else  // 如果要执行的数量为负数（卖出）
		ids = _ctx->sell(code, targetPx, abs(qty));  // 提交卖出订单，返回订单ID列表

	_orders_mon.push_order(ids.data(), ids.size(), now, isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志

	curTick->release();  // 释放当前tick数据（减少引用计数）
}

/**
 * @brief 执行计算实现
 * 
 * 核心计算函数，根据TWAP算法计算当前应该执行的订单数量，并在指定时间段内均匀分配执行。
 */
void WtTWapExeUnit::do_calc()
{
	CalcFlag flag(&_in_calc);  // 创建计算标志辅助对象，自动管理计算标志（RAII模式）
	if (flag)  // 如果构造时标志已经是true，说明有并发计算
		return;  // 直接返回，不执行计算（防止并发计算）

	if (!_channel_ready)  // 如果交易通道未就绪
		return;  // 直接返回，等待通道就绪
	// 这里加一个锁，主要原因是实盘过程中发现
	// 在修改目标仓位的时候，会触发一次do_calc
	// 而ontick也会触发一次do_calc，两次调用是从两个线程分别触发的，所以会出现同时触发的情况
	// 如果不加锁，就会引起问题
	// 这种情况在原来的SimpleExecUnit没有出现，因为SimpleExecUnit只在set_position的时候触发
	StdUniqueLock lock(_mtx_calc);  // 获取计算逻辑的互斥锁，保证线程安全

	const char* code = _code.c_str();  // 获取合约代码字符串
	double undone = _ctx->getUndoneQty(code);  // 获取未完成订单数量
	double newVol = get_real_target(_target_pos);  // 获取真实目标仓位（如果_target_pos为DBL_MAX则返回0）
	double realPos = _ctx->getPosition(code);  // 获取当前仓位
	double diffQty = newVol - realPos;  // 计算目标差量

	// 有正在撤销的订单，则不能进行下一轮计算
	if (_cancel_cnt != 0)  // 如果有在途撤单
	{
		_ctx->writeLog(fmt::format("{}尚有未完成撤单指令,暂时退出本轮执行", _code).c_str());  // 记录日志
		return;  // 直接返回，等待撤单完成后再执行计算
	}

	if (decimal::eq(diffQty, 0))  // 如果目标差量为0
		return;  // 直接返回，不需要执行
	// 每一次发单要保障成交，所以如果有未完成单，说明上一轮没完成
	// 有未完成订单，与实际仓位变动方向相反
	// 则需要撤销现有订单
	if (decimal::lt(diffQty * undone, 0))  // 如果差量与未完成订单方向相反
	{
		bool isBuy = decimal::gt(undone, 0);  // 根据未完成订单的正负判断方向
		OrderIDs ids = _ctx->cancel(code, isBuy);  // 根据方向撤单
		if (!ids.empty())  // 如果撤单成功
		{
			_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime());  // 将撤单的订单ID推入订单管理器
			_cancel_cnt += ids.size();  // 增加在途撤单量
			_ctx->writeLog(fmtutil::format("[{}@{}] live opposite order of {} canceled, cancelcnt -> {}", __FILE__, __LINE__, _code.c_str(), _cancel_cnt));  // 记录日志：相反的订单已取消
		}
		return;  // 撤单后直接返回，等待撤单完成后再执行计算
	}
	// 因为是逐笔发单，所以如果有不需要撤销的未完成单，则暂不发单
	if (!decimal::eq(undone, 0))  // 如果有未完成订单（且方向一致）
	{
		_ctx->writeLog(fmt::format("{}上一轮有挂单未完成,暂时退出本轮执行", _code).c_str());  // 记录日志
		return;  // 直接返回，暂不发单（等待上一笔订单完成）
	}
	double curPos = realPos;  // 当前仓位
	if (_last_tick == NULL)  // 如果没有最新行情数据
	{
		_ctx->writeLog(fmt::format("{}没有最新tick数据,退出执行逻辑", _code).c_str());  // 记录日志
		return;  // 直接返回，等待行情数据
	}
	if (decimal::eq(curPos, newVol))  // 如果当前仓位已经等于目标仓位
	{
		// 当前仓位和最新仓位匹配时，如果不是全部清仓的需求，则直接退出计算了
		if (!is_clear(_target_pos))  // 如果不是清仓需求
			return;  // 直接返回，不需要执行

		// 如果是清仓的需求，还要再进行对比
		// 如果多头为0，说明已经全部清理掉了，则直接退出
		double lPos = _ctx->getPosition(code, true, 1);  // 获取多头持仓
		if (decimal::eq(lPos, 0))  // 如果多头持仓为0
			return;  // 直接返回，清仓完成

		// 如果还有多头仓位，则将目标仓位设置为非0，强制触发
		newVol = -min(lPos, _order_lots);  // 设置目标仓位为负值（卖出）
		_ctx->writeLog(fmtutil::format("Clearing process triggered, target position of {} has been set to {}", _code.c_str(), newVol));  // 记录日志
	}

	// 如果相比上次没有更新的tick进来，则先不下单，防止开盘前集中下单导致通道被封
	uint64_t curTickTime = (uint64_t)_last_tick->actiondate() * 1000000000 + _last_tick->actiontime();  // 计算当前tick的时间戳（纳秒）
	if (curTickTime <= _last_tick_time)  // 如果当前tick时间小于等于上次tick时间（没有新行情）
	{
		_ctx->writeLog(fmtutil::format("No tick of {} updated, {} <= {}, execute later", _code, curTickTime, _last_tick_time));  // 记录日志
		return;  // 直接返回，等待新行情
	}
	_last_tick_time = curTickTime;  // 更新上次tick时间
	double this_qty = _order_lots; 	//单次发单手数
	/***---end---23.5.26---zhaoyk***/

	uint32_t leftTimes = _total_times - _fired_times;
	/***---begin---23.5.22---zhaoyk***/
	//_ctx->writeLog(fmt::format("第 {} 次发单", _fired_times).c_str());
	_ctx->writeLog(fmt::format("第 {} 次发单", _fired_times+1).c_str());

	/***---end---23.5.22---zhaoyk***/
	bool bNeedShowHand = false;
	//剩余次数为0,剩余数量不为0,说明要全部发出去了
	//剩余次数为0,说明已经到了兜底时间了,如果这个时候还有未完成数量,则需要发单
	/***---begin---23.5.22---zhaoyk***/
	//如果剩余此处为0 ,则需要全部下单
	//否则,取整(剩余数量/剩余次数)与1的最大值,即最小为_min_open_lots,但是要注意符号处理
	double curQty = 0;
	if (leftTimes == 0 && !decimal::eq(diffQty, 0))
	{
		bNeedShowHand = true;
		curQty = max(_min_open_lots, diffQty); 
	}
	else {
		curQty = std::max(_min_open_lots, round(abs(diffQty) / leftTimes)) * abs(diffQty) / diffQty;
	}
	
	/***---end---23.5.22---zhaoyk***/
	
	//设定本轮目标仓位
	_this_target = realPos + curQty;	//现仓位量+本轮下单量

	WTSTickData* curTick = _last_tick;
	uint64_t now = TimeUtils::getLocalTimeNow();
	bool isBuy = decimal::gt(diffQty, 0);
	double targetPx = 0;
	//根据价格模式设置,确定委托基准价格: 0-最新价,1-最优价,2-对手价
	if (_price_mode == 0)
	{
		targetPx = curTick->price();
	}
	else if (_price_mode == 1)
	{
		targetPx = isBuy ? curTick->bidprice(0) : curTick->askprice(0);
	}
	else // if(_price_mode == 2)
	{
		targetPx = isBuy ? curTick->askprice(0) : curTick->bidprice(0);
	}

	//如果需要全部发单,则价格偏移5跳,以保障执行
	if (bNeedShowHand) //  last showhand time
	{
		targetPx += _comm_info->getPriceTick() * 5 * (isBuy ? 1 : -1);
	}
	else if (_price_offset != 0)	//如果设置了价格偏移,也要处理一下
	{
		targetPx += _comm_info->getPriceTick() * _price_offset * (isBuy ? 1 : -1);
	}
	// 如果最后价格为0，再做一个修正
	if (decimal::eq(targetPx, 0.0))
		targetPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();

	//检查涨跌停价
	isCanCancel = true;
	if (isBuy && !decimal::eq(_last_tick->upperlimit(), 0) && decimal::gt(targetPx, _last_tick->upperlimit()))
	{
		_ctx->writeLog(fmt::format("Buy price {} of {} modified to upper limit price", targetPx, _code.c_str(), _last_tick->upperlimit()).c_str());
		targetPx = _last_tick->upperlimit();
		isCanCancel = false;//如果价格被修正为涨跌停价，订单不可撤销
	}
	if (isBuy != 1 && !decimal::eq(_last_tick->lowerlimit(), 0) && decimal::lt(targetPx, _last_tick->lowerlimit()))
	{
		_ctx->writeLog(fmt::format("Sell price {} of {} modified to lower limit price", targetPx, _code.c_str(), _last_tick->lowerlimit()).c_str());
		targetPx = _last_tick->lowerlimit();
		isCanCancel = false;	//如果价格被修正为涨跌停价，订单不可撤销
	}
	//最后发出指令
	OrderIDs ids;
	if (curQty > 0)
		ids = _ctx->buy(code, targetPx, abs(curQty));
	else
		ids = _ctx->sell(code, targetPx, abs(curQty));

	_orders_mon.push_order(ids.data(), ids.size(), now, isCanCancel);
	_last_fire_time = now;
	_fired_times += 1;

	curTick->release();
}

void WtTWapExeUnit::set_position(const char* stdCode, double newVol)
{
	if (_code.compare(stdCode) != 0)
		return;

	//double diff = newVol - _target_pos;
	if (decimal::eq(newVol, _target_pos))
		return;

	_target_pos = newVol;

	_fired_times = 0;

	do_calc();
}

