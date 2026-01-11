/*!
 * \file WtDiffMinImpactExeUnit.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtDiffMinImpactExeUnit差量最小冲击执行单元类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了WtDiffMinImpactExeUnit类的所有成员函数，该类用于差量模式的最小市场冲击算法，
 * 立即执行指定的数量变化（买入N手或卖出N手），而不是将仓位调整到目标值。
 * 
 * 实现要点：
 * 1. 差量驱动执行：立即执行指令中的数量变化（正数表示买入，负数表示卖出）
 * 2. 价格控制：根据价格模式和价格偏移计算订单价格
 * 3. 数量控制：根据配置的单次发单手数或比例下单
 * 4. 订单管理：跟踪订单状态，支持超时撤单
 * 5. 市场数据响应：根据Tick数据实时调整订单策略
 * 
 * 差量模式特点：
 * - 直接执行指令中的数量变化（买入N手或卖出N手）
 * - 不关心当前仓位，只执行差量指令
 * - 适用于高频交易、做市、抢单等策略
 */
#include "WtDiffMinImpactExeUnit.h"  // WtDiffMinImpactExeUnit类定义

#include "../Includes/WTSVariant.hpp"  // 变体类型定义
#include "../Includes/WTSContractInfo.hpp"  // 合约信息定义
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息定义
#include "../Share/decimal.h"  // 精确小数计算
#include "../Share/StrUtil.hpp"  // 字符串工具函数
#include "../Share/fmtlib.h"  // 格式化库

/**
 * @brief 工厂名称外部声明
 * 
 * 声明FACT_NAME常量，该常量在WtExeFact.cpp中定义。
 */
extern const char* FACT_NAME;

/**
 * @brief 价格模式名称数组外部声明
 * 
 * 声明PriceModeNames数组，该数组在其他文件中定义。
 */
extern const char* PriceModeNames[4];

/**
 * @brief WtDiffMinImpactExeUnit构造函数实现
 * 
 * 创建差量最小冲击执行单元实例，初始化所有成员变量为默认值。
 */
WtDiffMinImpactExeUnit::WtDiffMinImpactExeUnit()
	: _last_tick(NULL)  // 上一笔行情数据指针初始化为NULL
	, _comm_info(NULL)  // 合约信息指针初始化为NULL
	, _price_mode(0)  // 价格模式初始化为0（最新价）
	, _price_offset(0)  // 价格偏移跳数初始化为0
	, _expire_secs(0)  // 订单超时秒数初始化为0（不超时）
	, _cancel_cnt(0)  // 在途撤单量初始化为0
	, _left_diff(0)  // 未执行差量初始化为0
	, _cancel_times(0)  // 撤单次数初始化为0
	, _last_place_time(0)  // 上个下单时间初始化为0
	, _last_tick_time(0)  // 上个tick时间初始化为0
	, _in_calc(false)  // 计算中标志初始化为false
{
	// 构造函数体为空，所有初始化工作都在初始化列表中完成
}

/**
 * @brief WtDiffMinImpactExeUnit析构函数实现
 * 
 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
 */
WtDiffMinImpactExeUnit::~WtDiffMinImpactExeUnit()
{
	if (_last_tick)  // 如果行情数据指针不为空
		_last_tick->release();  // 释放行情数据（减少引用计数）

	if (_comm_info)  // 如果合约信息指针不为空
		_comm_info->release();  // 释放合约信息（减少引用计数）
}

/**
 * @brief 获取所属执行器工厂名称实现
 * 
 * 返回创建该执行单元的工厂名称。
 * 
 * @return const char* 返回工厂名称字符串（"WtExeFact"）
 */
const char* WtDiffMinImpactExeUnit::getFactName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 获取执行单元名称实现
 * 
 * 返回执行单元的名称，用于标识和管理。
 * 
 * @return const char* 返回执行单元名称字符串（"WtDiffMinImpactExeUnit"）
 */
const char* WtDiffMinImpactExeUnit::getName()
{
	return "WtDiffMinImpactExeUnit";  // 返回执行单元名称
}

/**
 * @brief 初始化执行单元实现
 * 
 * 初始化执行单元，加载配置参数，获取合约信息和交易时段信息。
 * 
 * @param ctx 执行单元运行环境指针，提供合约信息、交易时段信息等
 * @param stdCode 管理的合约代码（标准格式）
 * @param cfg 配置对象指针，包含执行单元的各种配置参数：
 *   - offset：价格偏移跳数（相对于基准价格的偏移，买入+偏移，卖出-偏移）
 *   - expire：订单超时秒数（订单创建后超过此时间未成交则自动撤单）
 *   - pricemode：价格模式（0-最新价，-1-最优价，1-对手价，2-自动价格）
 *   - span：发单时间间隔（单位：毫秒，两次下单之间的最小时间间隔）
 *   - byrate：是否按照对手盘挂单数的比例下单（true表示按比例，false表示固定数量）
 *   - lots：单次发单手数（当byrate为false时使用）
 *   - rate：下单手数比例（当byrate为true时使用，如0.1表示每次下对手盘挂单量的10%）
 */
void WtDiffMinImpactExeUnit::init(ExecuteContext* ctx, const char* stdCode, WTSVariant* cfg)
{
	ExecuteUnit::init(ctx, stdCode, cfg);  // 调用基类的初始化方法，完成基础初始化

	_comm_info = ctx->getCommodityInfo(stdCode);  // 获取品种参数（合约信息）
	if (_comm_info)  // 如果合约信息指针不为空
		_comm_info->retain();  // 增加引用计数，防止被释放

	_sess_info = ctx->getSessionInfo(stdCode);  // 获取交易时间模板信息（交易时段信息）
	if (_sess_info)  // 如果交易时段信息指针不为空
		_sess_info->retain();  // 增加引用计数，防止被释放

	_price_offset = cfg->getInt32("offset");  // 价格偏移跳数，一般和订单同方向（买入+偏移，卖出-偏移）
	_expire_secs = cfg->getUInt32("expire");  // 订单超时秒数（订单创建后超过此时间未成交则自动撤单）
	_price_mode = cfg->getInt32("pricemode");  // 价格类型：0-最新价，-1-最优价，1-对手价，2-自动，默认为0
	_entrust_span = cfg->getUInt32("span");  // 发单时间间隔（单位：毫秒，两次下单之间的最小时间间隔）
	_by_rate = cfg->getBoolean("byrate");  // 是否按照对手的挂单数的比例下单，如果是true，则rate字段生效，如果是false则lots字段生效
	_order_lots = cfg->getDouble("lots");  // 单次发单手数（当byrate为false时使用）
	_qty_rate = cfg->getDouble("rate");  // 下单手数比例（当byrate为true时使用，如0.1表示每次下对手盘挂单量的10%）

	// 记录初始化日志，输出所有配置参数
	ctx->writeLog(fmtutil::format("DiffMiniImpactExecUnit {} inited, order price: {} ± {} ticks, order expired: {} secs, order timespan:{} millisec, order qty: {} @ {:.2f}",
		stdCode, PriceModeNames[_price_mode + 1], _price_offset, _expire_secs, _entrust_span, _by_rate ? "byrate" : "byvol", _by_rate ? _qty_rate : _order_lots));
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
void WtDiffMinImpactExeUnit::on_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled)
{
	{
		if (!_orders_mon.has_order(localid))  // 如果没有对应订单，直接返回（可能是其他执行单元的订单）
			return;

		if (isCanceled || leftover == 0)  // 如果订单已撤销或剩余订单为0（全部成交）
		{	
			// By Wesley @ 2022.05.24
			// 这句要注释掉，因为需要在on_trade里处理一些数据
			// 这里如果从订单管理器中删除了订单号，on_trade就会判断失败
			//_orders_mon.erase_order(localid);  // 不在这里删除订单，在on_trade中删除
			if (_cancel_cnt > 0)  // 如果在途撤单量大于0
			{
				_cancel_cnt--;  // 减少在途撤单量
				_ctx->writeLog(fmtutil::format("[{}@{}] Order of {} cancelling done, cancelcnt -> {}", __FILE__, __LINE__, _code.c_str(), _cancel_cnt));  // 记录日志
			}
		}

		if (leftover == 0 && !isCanceled)  // 如果订单全部成交（剩余为0且未撤销）
			_cancel_times = 0;  // 重置撤单次数（订单已成交，不需要继续撤单）
	}

	// 如果有撤单，也触发重新计算（撤单后需要重新下单）
	if (isCanceled)  // 如果订单已撤销
	{
		_ctx->writeLog(fmtutil::format("Order {} of {} canceled, recalc will be done", localid, stdCode));  // 记录撤单日志
		_cancel_times++;  // 增加撤单次数统计
		do_calc();  // 触发重新计算，重新下单
	}
}

/**
 * @brief 交易通道就绪回调实现
 * 
 * 当交易通道连接成功并准备就绪时调用此函数，可以开始下单。
 * 该函数会检查是否有未管理的订单（可能是上次启动时的未完成单或外部挂单），
 * 如果有则自动撤单，然后触发执行计算。
 */
void WtDiffMinImpactExeUnit::on_channel_ready()
{
	double undone = _ctx->getUndoneQty(_code.c_str());  // 获取未完成数量（从交易通道获取）

	if(!decimal::eq(undone, 0) && !_orders_mon.has_order())  // 如果未完成单不为0，而订单管理器中没有订单
	{
		/*
		 * 如果未完成单不为0，而订单管理器中没有订单
		 * 这说明有未完成单不在监控之中，全部撤销掉
		 * 因为这些订单没有本地订单号，无法直接进行管理
		 * 这种情况，就是刚启动的时候，上次的未完成单或者外部的挂单
		 */
		_ctx->writeLog(fmtutil::format("Unmanaged live orders with qty {} of {} found, cancel all", undone, _code.c_str()));  // 记录日志

		bool isBuy = (undone > 0);  // 根据未完成数量的正负判断方向（正数表示买入，负数表示卖出）
		OrderIDs ids = _ctx->cancel(_code.c_str(), isBuy);  // 根据方向撤单，返回撤单的订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime());  // 将撤单的订单ID推入订单管理器
		_cancel_cnt += ids.size();  // 增加在途撤单量

		_ctx->writeLog(fmtutil::format("[{}@{}]cancelcnt -> {}", __FILE__, __LINE__, _cancel_cnt));  // 记录日志
	}
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
	else  // 其他情况（未完成单和订单管理器状态一致）
	{
		_ctx->writeLog(fmtutil::format("Unrecognized condition while channle ready, {:.2f} live orders of {} exists, local orders {}exist",
			undone, _code.c_str(), _orders_mon.has_order() ? "" : "not "));  // 记录日志（警告信息）
	}

	do_calc();  // 触发执行计算，开始下单
}

/**
 * @brief 交易通道丢失回调实现
 * 
 * 当交易通道断开时调用此函数，停止下单操作。
 * 当前版本该函数为空，不做任何处理。
 */
void WtDiffMinImpactExeUnit::on_channel_lost()
{
	// 交易通道丢失时，停止下单操作（当前版本不做任何处理）
}

/**
 * @brief Tick数据回调实现
 * 
 * 当有新的Tick数据时调用此函数，更新最新行情，触发执行计算。
 * 该函数会检查订单超时，自动撤单，然后触发执行计算。
 * 
 * @param newTick 最新的Tick数据指针，包含最新价格、成交量、持仓量等信息
 */
void WtDiffMinImpactExeUnit::on_tick(WTSTickData* newTick)
{
	if (newTick == NULL || _code.compare(newTick->code()) != 0)  // 如果Tick数据为空或合约代码不匹配
		return;  // 直接返回，不处理

	// 如果原来的tick不为空，则要释放掉（减少引用计数）
	if (_last_tick)  // 如果上一笔行情数据指针不为空
	{
		_last_tick->release();  // 释放上一笔行情数据
	}
	else  // 如果上一笔行情数据为空（第一次收到行情）
	{
		// 如果行情时间不在交易时间，这种情况一般是集合竞价的行情进来，下单会失败，所以直接过滤掉这笔行情
		if (_sess_info != NULL && !_sess_info->isInTradingTime(newTick->actiontime() / 100000))  // 如果不在交易时间内
			return;  // 直接返回，不处理
	}

	// 新的tick数据，要保留（增加引用计数）
	_last_tick = newTick;  // 保存新的行情数据指针
	_last_tick->retain();  // 增加引用计数，防止被释放

	/*
	 * 这里可以考虑一下
	 * 如果写的上一次丢出去的单子不够达到目标仓位
	 * 那么在新的行情数据进来的时候可以再次触发核心逻辑
	 */

	if(_expire_secs != 0 && _orders_mon.has_order() && _cancel_cnt==0)  // 如果订单超时秒数!=0 && 有订单 && 撤单量==0
	{
		uint64_t now = _ctx->getCurTime();  // 获取当前时间（毫秒时间戳）

		// 检查订单是否超时，如果超时则撤单
		_orders_mon.check_orders(_expire_secs, now, [this](uint32_t localid) {  // 遍历所有订单，检查是否超时
			if (_ctx->cancel(localid))  // 如果撤单成功
			{
				_cancel_cnt++;  // 增加在途撤单量
				_ctx->writeLog(fmtutil::format("[{}@{}] Expired order of {} canceled, cancelcnt -> {}", __FILE__, __LINE__, _code.c_str(), _cancel_cnt));  // 记录日志
			}
		});
	}
	
	do_calc();  // 触发执行计算，根据最新行情决定是否下单
}

/**
 * @brief 成交回报处理实现
 * 
 * 当订单有成交回报时调用此函数，更新未执行差量。
 * 在差量模式下，成交会减少未执行差量。
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy true表示买入成交，false表示卖出成交
 * @param vol 成交数量（这里没有正负，通过isBuy确定买入还是卖出）
 * @param price 成交价格
 */
void WtDiffMinImpactExeUnit::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	// 如果是本地订单，则更新差量
	if (!_orders_mon.has_order(localid))  // 如果订单管理器中不存在该订单
		return;  // 直接返回，不做任何处理

	// 更新未执行差量：买入成交减少差量（正数），卖出成交增加差量（负数）
	_left_diff -= vol * (isBuy ? 1 : -1);  // 买入时减去成交量，卖出时加上成交量

	_ctx->writeLog(fmtutil::format("Left diff of {} updated to {}", _code.c_str(), _left_diff));  // 记录日志
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
void WtDiffMinImpactExeUnit::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
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
 * @brief 执行计算实现
 * 
 * 核心计算函数，根据未执行差量计算需要执行的订单数量，
 * 并根据市场行情和配置参数决定是否下单、撤单等操作。
 * 
 * 执行流程：
 * 1. 检查是否有并发计算，如果有则直接返回
 * 2. 检查是否有在途撤单，如果有则等待撤单完成
 * 3. 获取未完成订单数量和未执行差量
 * 4. 检查是否有未完成订单（逐笔发单模式，有未完成订单则暂不发单）
 * 5. 检查未执行差量是否为0（为0则不需要执行）
 * 6. 检查是否有最新行情数据
 * 7. 检查下单时间间隔
 * 8. 计算买卖方向（根据未执行差量的正负）
 * 9. 检查是否有新行情（防止重复下单）
 * 10. 计算下单数量（根据配置的单次发单手数或比例）
 * 11. 修正下单数量（不能超过未执行差量，平仓时不能超过持仓）
 * 12. 计算订单价格（根据价格模式和价格偏移）
 * 13. 检查涨跌停价（涨跌停价的挂单不能撤单）
 * 14. 下单
 */
void WtDiffMinImpactExeUnit::do_calc()
{
	CalcFlag flag(&_in_calc);  // 创建计算标志辅助对象，自动管理计算标志（RAII模式）
	if (flag)  // 如果构造时标志已经是true，说明有并发计算
	{
		_ctx->writeLog(fmtutil::format("Duplicated calculating, DiffMinImpactExeUnit of {}", _code));  // 记录日志
		return;  // 直接返回，不执行计算（防止并发计算）
	}

	if (_cancel_cnt != 0)  // 如果有在途撤单
	{
		_ctx->writeLog(fmtutil::format("In Cancelling, DiffMinImpactExeUnit of {}", _code));  // 记录日志
		return;  // 直接返回，等待撤单完成后再执行计算
	}

	// 这里加一个锁，主要原因是实盘过程中发现
	// 在修改目标仓位的时候，会触发一次do_calc
	// 而ontick也会触发一次do_calc，两次调用是从两个线程分别触发的，所以会出现同时触发的情况
	// 如果不加锁，就会引起问题
	// 这种情况在原来的SimpleExecUnit没有出现，因为SimpleExecUnit只在set_position的时候触发
	StdUniqueLock lock(_mtx_calc);  // 获取计算逻辑的互斥锁，保证线程安全

	const char* stdCode = _code.c_str();  // 获取合约代码字符串

	double undone = _ctx->getUndoneQty(stdCode);  // 获取未完成订单数量（正数表示买入未完成，负数表示卖出未完成）
	double diffPos = _left_diff;  // 获取未执行差量（正数表示需要买入，负数表示需要卖出）

	// 因为是逐笔发单，所以如果有不需要撤销的未完成单，则暂不发单
	if (!decimal::eq(undone, 0))  // 如果有未完成订单
	{
		_ctx->writeLog(fmtutil::format("Live orders exist, DiffMinImpactExeUnit of {}", _code));  // 记录日志
		return;  // 直接返回，暂不发单（等待上一笔订单完成）
	}

	if (decimal::eq(diffPos, 0))  // 如果未执行差量为0
		return;  // 直接返回，不需要执行

	if (_last_tick == NULL)  // 如果没有最新行情数据
	{
		_ctx->writeLog(fmtutil::format("No lastest tick data of {}, execute later", _code.c_str()));  // 记录日志
		return;  // 直接返回，等待行情数据
	}

	// 检查下单时间间隔
	uint64_t now = TimeUtils::makeTime(_last_tick->actiondate(), _last_tick->actiontime());  // 获取当前时间（毫秒时间戳）
	if (now - _last_place_time < _entrust_span)  // 如果距离上次下单时间小于发单时间间隔
		return;  // 直接返回，等待时间间隔

	bool isBuy = decimal::gt(diffPos, 0);  // 判断买卖方向（未执行差量 > 0，则为买入）

	// 如果相比上次没有更新的tick进来，则先不下单，防止开盘前集中下单导致通道被封
	uint64_t curTickTime = (uint64_t)_last_tick->actiondate() * 1000000000 + _last_tick->actiontime();  // 计算当前tick的时间戳（纳秒）
	if (curTickTime <= _last_tick_time)  // 如果当前tick时间小于等于上次tick时间（没有新行情）
	{
		_ctx->writeLog(fmtutil::format("No tick of {} updated, {} <= {}, execute later",
			_code, curTickTime, _last_tick_time));  // 记录日志
		return;  // 直接返回，等待新行情
	}

	_last_tick_time = curTickTime;  // 更新上次tick时间

	double this_qty = _order_lots;  // 单次发单手数（默认值）
	if (_by_rate)  // 如果按照对手盘挂单数的比例下单
	{
		this_qty = isBuy ? _last_tick->askqty(0) : _last_tick->bidqty(0);  // 获取对手盘挂单量（买入用卖一量，卖出用买一量）
		this_qty = round(this_qty * _qty_rate);  // 按比例计算下单数量（四舍五入）
		if (decimal::lt(this_qty, 1))  // 如果计算出的数量小于1
			this_qty = 1;  // 设置为1（至少下1手）
	}

	// By Wesley @ 2022.09.13
	// 这里要对下单数量做一个修正（不能超过未执行差量）
	this_qty = min(this_qty, abs(diffPos));  // 下单数量不能超过未执行差量的绝对值

	// 如果买入且有空头持仓，或者卖出且有多头持仓
	// 对单次下单做一个修正，保证平仓和开仓不会同时下单
	double curPos = _ctx->getPosition(stdCode);  // 获取当前仓位
	if((isBuy && decimal::lt(curPos, 0)) || (!isBuy && decimal::gt(curPos, 0)))  // 如果买入且有空头持仓，或者卖出且有多头持仓（平仓情况）
	{
		this_qty = min(this_qty, abs(curPos));  // 平仓数量不能超过当前持仓的绝对值
	}

	double buyPx, sellPx;  // 买入价格和卖出价格
	if (_price_mode == 2)  // 如果价格模式为自动价格（2）
	{
		// 自动价格模式：根据买卖盘口的力量对比自动选择价格
		// 计算买卖盘口力量对比：mp = (买一量 - 卖一量) / (买一量 + 卖一量)
		// mp > 0 表示买盘力量强，mp < 0 表示卖盘力量强
		double mp = (_last_tick->bidqty(0) - _last_tick->askqty(0)) * 1.0 / (_last_tick->bidqty(0) + _last_tick->askqty(0));
		bool isUp = (mp > 0);  // 判断买盘力量是否强于卖盘
		if (isUp)  // 如果买盘力量强（买一量 > 卖一量）
		{
			buyPx = _last_tick->askprice(0);  // 买入价格用卖一价（主动买入）
			sellPx = _last_tick->askprice(0);  // 卖出价格也用卖一价（被动卖出）
		}
		else  // 如果卖盘力量强（卖一量 > 买一量）
		{
			buyPx = _last_tick->bidprice(0);  // 买入价格用买一价（被动买入）
			sellPx = _last_tick->bidprice(0);  // 卖出价格也用买一价（主动卖出）
		}

		/*
		 * By Wesley @ 2022.03.07
		 * 如果最后价格为0，再做一个修正
		 * 价格为0，可能当日没有交易，所以取上一个交易日的收盘价
		 */
		if (decimal::eq(buyPx, 0.0))  // 如果买入价格为0
			buyPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();  // 如果最新价也为0，则用昨收价，否则用最新价

		if (decimal::eq(sellPx, 0.0))  // 如果卖出价格为0
			sellPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();  // 如果最新价也为0，则用昨收价，否则用最新价

		// 根据撤单次数调整价格（撤单次数越多，价格调整越大，用于追单）
		buyPx += _comm_info->getPriceTick() * _cancel_times;  // 买入价格 += 最小变动价位 * 撤单次数（扩大追单步幅）
		sellPx -= _comm_info->getPriceTick() * _cancel_times;  // 卖出价格 -= 最小变动价位 * 撤单次数（扩大追单步幅）
	}
	else  // 如果价格模式不是自动价格
	{
		if (_price_mode == -1)  // 如果价格模式为最优价（-1）
		{
			buyPx = _last_tick->bidprice(0);  // 买入价格用买一价（最优买入价）
			sellPx = _last_tick->askprice(0);  // 卖出价格用卖一价（最优卖出价）
		}
		else if (_price_mode == 0)  // 如果价格模式为最新价（0）
		{
			buyPx = _last_tick->price();  // 买入价格用最新成交价
			sellPx = _last_tick->price();  // 卖出价格用最新成交价
		}
		else if (_price_mode == 1)  // 如果价格模式为对手价（1）
		{
			buyPx = _last_tick->askprice(0);  // 买入价格用卖一价（对手价，主动买入）
			sellPx = _last_tick->bidprice(0) - _comm_info->getPriceTick() * _price_offset;  // 卖出价格用买一价减去偏移（对手价，主动卖出）
		}

		/*
		 * By Wesley @ 2022.03.07
		 * 如果最后价格为0，再做一个修正
		 */
		if (decimal::eq(buyPx, 0.0))  // 如果买入价格为0
			buyPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();  // 如果最新价也为0，则用昨收价，否则用最新价

		if (decimal::eq(sellPx, 0.0))  // 如果卖出价格为0
			sellPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();  // 如果最新价也为0，则用昨收价，否则用最新价

		// 根据价格偏移调整价格
		buyPx += _comm_info->getPriceTick() * _price_offset;  // 买入价格 += 最小变动价位 * 价格偏移跳数（买入+偏移）
		sellPx -= _comm_info->getPriceTick() * _price_offset;  // 卖出价格 -= 最小变动价位 * 价格偏移跳数（卖出-偏移）
	}
	

	// 检查涨跌停价
	bool isCanCancel = true;  // 是否可撤单标志（默认true）
	if (!decimal::eq(_last_tick->upperlimit(), 0) && decimal::gt(buyPx, _last_tick->upperlimit()))  // 如果买入价格超过涨停价
	{
		_ctx->writeLog(fmtutil::format("Buy price {} of {} modified to upper limit price", buyPx, _code.c_str(), _last_tick->upperlimit()));  // 记录日志：买入价改为上限价
		buyPx = _last_tick->upperlimit();  // 买入价格修正为涨停价
		isCanCancel = false;  // 如果价格被修正为涨跌停价，订单不可撤销（涨跌停价的挂单不能撤单）
	}
	
	if (!decimal::eq(_last_tick->lowerlimit(), 0) && decimal::lt(sellPx, _last_tick->lowerlimit()))  // 如果卖出价格低于跌停价
	{
		_ctx->writeLog(fmtutil::format("Sell price {} of {} modified to lower limit price", sellPx, _code.c_str(), _last_tick->lowerlimit()));  // 记录日志：卖出价改为下限价
		sellPx = _last_tick->lowerlimit();  // 卖出价格修正为跌停价
		isCanCancel = false;  // 如果价格被修正为涨跌停价，订单不可撤销（涨跌停价的挂单不能撤单）
	}

	// 根据买卖方向下单（差量模式下不使用强制平仓标志）
	if (isBuy)  // 如果是买入
	{
		OrderIDs ids = _ctx->buy(stdCode, buyPx, this_qty, false);  // 提交买入订单，返回订单ID列表（false表示不强制平仓）
		_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime(), isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志
	}
	else  // 如果是卖出
	{
		OrderIDs ids = _ctx->sell(stdCode, sellPx, this_qty, false);  // 提交卖出订单，返回订单ID列表（false表示不强制平仓）
		_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime(), isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志
	}

	_last_place_time = now;  // 更新上次下单时间
}

/**
 * @brief 设置新的差量实现
 * 
 * 设置执行单元的差量指令，执行单元会立即执行该差量。
 * 
 * @param stdCode 合约代码
 * @param newDiff 新的差量指令（正数表示买入N手，负数表示卖出N手）
 * 
 * 注意：差量执行单元不支持清仓指令（DBL_MAX），如果传入DBL_MAX则直接返回。
 */
void WtDiffMinImpactExeUnit::set_position(const char* stdCode, double newDiff)
{
	if (_code.compare(stdCode) != 0)  // 如果合约代码不匹配
		return;  // 直接返回，不做任何处理

	if (newDiff == DBL_MAX)  // 如果差量指令为DBL_MAX（清仓标志）
	{
		_ctx->writeLog("Diff execute unit do not support clear command");  // 记录日志：差量执行单元不支持清仓指令
		return;  // 直接返回，不做任何处理
	}

	// 这里就是最新的差量
	if(_left_diff != newDiff)  // 如果差量指令有变化
	{
		_left_diff = newDiff;  // 更新未执行差量

		_ctx->writeLog(fmtutil::format("Diff of {} updated to {}", stdCode, _left_diff));  // 记录日志

		do_calc();  // 触发执行计算，根据新差量执行
	}
}

/**
 * @brief 清理全部持仓实现
 * 
 * 差量执行单元不支持清仓指令，该函数直接返回并记录日志。
 * 
 * @param stdCode 合约代码
 */
void WtDiffMinImpactExeUnit::clear_all_position(const char* stdCode)
{
	_ctx->writeLog("Diff execute unit do not support clear command");  // 记录日志：差量执行单元不支持清仓指令
	return;  // 直接返回，不做任何处理
}
