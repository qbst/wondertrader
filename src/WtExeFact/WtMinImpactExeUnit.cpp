/*!
 * \file WtMinImpactExeUnit.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtMinImpactExeUnit最小冲击执行单元类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了WtMinImpactExeUnit类的所有成员函数，该类用于期货合约的智能订单执行，
 * 通过最小市场冲击算法，控制订单价格和数量，减少对市场的影响。
 * 
 * 实现要点：
 * 1. 目标驱动执行：计算目标仓位与当前仓位的差值，执行差量
 * 2. 价格控制：根据价格模式（最优价/最新价/对手价/自动）确定订单价格
 * 3. 数量控制：根据配置的单次发单手数或比例下单
 * 4. 订单管理：跟踪订单状态，支持超时撤单
 * 5. 错单检测：检测无法撤单的订单（可能是错单）
 * 6. 涨跌停处理：涨跌停价的挂单不能撤单
 * 
 * 核心算法：
 * - 差量计算：目标仓位 - 当前仓位 = 需要执行的差量
 * - 价格计算：根据价格模式和价格偏移计算订单价格
 * - 数量计算：根据配置的单次发单手数或比例计算下单数量
 * - 超时撤单：订单超时自动撤单并重新下单
 * - 错单检测：超过最大撤单次数仍无法撤单的订单视为错单
 */
#include "WtMinImpactExeUnit.h"  // WtMinImpactExeUnit类定义

#include "../Includes/WTSVariant.hpp"  // 变体类型定义
#include "../Includes/WTSContractInfo.hpp"  // 合约信息定义
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息定义
#include "../Share/decimal.h"  // 精确小数计算
#include "../Share/StrUtil.hpp"  // 字符串工具函数
#include "../Share//fmtlib.h"  // 格式化库

/**
 * @brief 工厂名称外部声明
 * 
 * 声明FACT_NAME常量，该常量在WtExeFact.cpp中定义。
 */
extern const char* FACT_NAME;

/**
 * @brief 价格模式名称数组
 * 
 * 价格模式的字符串名称数组，用于日志输出和配置显示。
 * 数组索引与价格模式值的对应关系：
 * - 索引0（BESTPX）：最优价（-1）
 * - 索引1（LASTPX）：最新价（0）
 * - 索引2（MARKET）：对手价（1）
 * - 索引3（AUTOPX）：自动价格（2）
 */
const char* PriceModeNames[] =
{
	"BESTPX",		// 最优价（买入用买一价，卖出用卖一价）
	"LASTPX",		// 最新价（最新成交价）
	"MARKET",		// 对手价（买入用卖一价，卖出用买一价）
	"AUTOPX"		// 自动价格（根据市场情况自动选择）
};

/**
 * @brief 获取真实目标仓位
 * 
 * 将目标仓位转换为真实的目标仓位值。
 * 如果目标仓位为DBL_MAX（清仓标志），则返回0。
 * 
 * @param target 目标仓位（DBL_MAX表示清仓）
 * @return double 真实目标仓位（0或目标仓位值）
 */
inline double get_real_target(double target)
{
	if (target == DBL_MAX)  // 如果目标仓位为DBL_MAX（清仓标志）
		return 0;  // 返回0（清仓时目标仓位为0）

	return target;  // 否则返回目标仓位值
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
 * @brief WtMinImpactExeUnit构造函数实现
 * 
 * 创建最小冲击执行单元实例，初始化所有成员变量为默认值。
 */
WtMinImpactExeUnit::WtMinImpactExeUnit()
	: _last_tick(NULL)  // 上一笔行情数据指针初始化为NULL
	, _comm_info(NULL)  // 合约信息指针初始化为NULL
	, _price_mode(0)  // 价格模式初始化为0（最新价）
	, _price_offset(0)  // 价格偏移跳数初始化为0
	, _expire_secs(0)  // 订单超时秒数初始化为0（不超时）
	, _cancel_cnt(0)  // 在途撤单量初始化为0
	, _target_pos(0)  // 目标仓位初始化为0
	, _cancel_times(0)  // 撤单次数初始化为0
	, _last_place_time(0)  // 上个下单时间初始化为0
	, _last_tick_time(0)  // 上个tick时间初始化为0
	, _in_calc(false)  // 计算中标志初始化为false
	, _min_open_lots(1)  // 最小开仓数量初始化为1
{
	// 构造函数体为空，所有初始化工作都在初始化列表中完成
}

/**
 * @brief WtMinImpactExeUnit析构函数实现
 * 
 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
 */
WtMinImpactExeUnit::~WtMinImpactExeUnit()
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
const char* WtMinImpactExeUnit::getFactName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 获取执行单元名称实现
 * 
 * 返回执行单元的名称，用于标识和管理。
 * 
 * @return const char* 返回执行单元名称字符串（"WtMinImpactExeUnit"）
 */
const char* WtMinImpactExeUnit::getName()
{
	return "WtMinImpactExeUnit";  // 返回执行单元名称
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
 *   - minopenlots：最小开仓数量（可选，默认1）
 */
void WtMinImpactExeUnit::init(ExecuteContext* ctx, const char* stdCode, WTSVariant* cfg)
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

	if (cfg->has("minopenlots"))  // 如果配置中有最小开仓数量参数
		_min_open_lots = cfg->getDouble("minopenlots");  // 最小开仓数量（开仓时，如果差量小于此值则不执行）

	// 记录初始化日志，输出所有配置参数
	ctx->writeLog(fmtutil::format("MiniImpactExecUnit of {} inited, order price @ {}±{} ticks, expired after {} secs, reorder after {} millisec, lots policy: {} @ {:.2f}, min open lots: {}",
		stdCode, PriceModeNames[_price_mode + 1], _price_offset, _expire_secs, _entrust_span, _by_rate ? "byrate" : "byvol", _by_rate ? _qty_rate : _order_lots, _min_open_lots));
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
void WtMinImpactExeUnit::on_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled)
{
	{
		if (!_orders_mon.has_order(localid))  // 如果没有对应订单，直接返回（可能是其他执行单元的订单）
			return;

		if (isCanceled || leftover == 0)  // 如果订单已撤销或剩余订单为0（全部成交）
		{
			_orders_mon.erase_order(localid);  // 从订单管理器中删除该订单
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
void WtMinImpactExeUnit::on_channel_ready()
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
void WtMinImpactExeUnit::on_channel_lost()
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
void WtMinImpactExeUnit::on_tick(WTSTickData* newTick)
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
	// 在ontick中对订单管理进行校验。例如有不活跃合约，校验减少，反之增多
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
 * 当订单有成交回报时调用此函数，更新持仓和未完成订单数量。
 * 注意：该函数不触发重新计算，因为成交回报会在on_tick中触发重新计算。
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy true表示买入成交，false表示卖出成交
 * @param vol 成交数量（这里没有正负，通过isBuy确定买入还是卖出）
 * @param price 成交价格
 */
void WtMinImpactExeUnit::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	// 不用触发重新计算，这里在ontick里触发（成交回报会在on_tick中触发重新计算）
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
void WtMinImpactExeUnit::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
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
 * 核心计算函数，根据目标仓位和当前仓位计算需要执行的差量，
 * 并根据市场行情和配置参数决定是否下单、撤单等操作。
 * 
 * 执行流程：
 * 1. 检查是否有并发计算，如果有则直接返回
 * 2. 检查是否有在途撤单，如果有则等待撤单完成
 * 3. 获取真实目标仓位、当前仓位、未完成订单数量
 * 4. 检查是否需要撤单（未完成订单与目标方向相反）
 * 5. 检查是否有未完成订单（逐笔发单模式，有未完成订单则暂不发单）
 * 6. 检查是否有最新行情数据
 * 7. 检查下单时间间隔
 * 8. 检查是否已达到目标仓位（清仓逻辑）
 * 9. 计算买卖方向
 * 10. 检查是否有新行情（防止重复下单）
 * 11. 计算下单数量（根据配置的单次发单手数或比例）
 * 12. 判断是开仓还是平仓
 * 13. 修正下单数量（平仓时不能超过持仓，开仓时不能小于最小开仓数量）
 * 14. 计算订单价格（根据价格模式和价格偏移）
 * 15. 检查涨跌停价（涨跌停价的挂单不能撤单）
 * 16. 下单
 */
void WtMinImpactExeUnit::do_calc()
{
	CalcFlag flag(&_in_calc);  // 创建计算标志辅助对象，自动管理计算标志（RAII模式）
	if (flag)  // 如果构造时标志已经是true，说明有并发计算
		return;  // 直接返回，不执行计算（防止并发计算）

	if (_cancel_cnt != 0)  // 如果有在途撤单
		return;  // 直接返回，等待撤单完成后再执行计算

	// 这里加一个锁，主要原因是实盘过程中发现
	// 在修改目标仓位的时候，会触发一次do_calc
	// 而ontick也会触发一次do_calc，两次调用是从两个线程分别触发的，所以会出现同时触发的情况
	// 如果不加锁，就会引起问题
	// 这种情况在原来的SimpleExecUnit没有出现，因为SimpleExecUnit只在set_position的时候触发
	StdUniqueLock lock(_mtx_calc);  // 获取计算逻辑的互斥锁，保证线程安全

	double newVol = get_real_target(_target_pos);  // 真实价格目标仓位（如果_target_pos为DBL_MAX则返回0）
	const char* stdCode = _code.c_str();  // 获取合约代码字符串

	double undone = _ctx->getUndoneQty(stdCode);  // 获取未完成订单数量（正数表示买入未完成，负数表示卖出未完成）
	double realPos = _ctx->getPosition(stdCode);  // 获取当前仓位（正数表示多头，负数表示空头）
	double diffPos = newVol - realPos;  // 计算差量（真实目标仓位 - 当前仓位，正数表示需要买入，负数表示需要卖出）

	// 有未完成订单，与实际仓位变动方向相反
	// 则需要撤销现有订单
	if (decimal::lt(diffPos * undone, 0))  // 如果差量与未完成订单方向相反（diffPos * undone < 0）
	{
		bool isBuy = decimal::gt(undone, 0);  // 根据未完成订单的正负判断方向（正数表示买入，负数表示卖出）
		OrderIDs ids = _ctx->cancel(stdCode, isBuy);  // 根据方向撤单，返回撤单的订单ID列表
		if(!ids.empty())  // 如果撤单成功（返回了订单ID列表）
		{
			_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime());  // 将撤单的订单ID推入订单管理器
			_cancel_cnt += ids.size();  // 增加在途撤单量
			_ctx->writeLog(fmtutil::format("[{}@{}] live opposite order of {} canceled, cancelcnt -> {}", __FILE__, __LINE__, _code.c_str(), _cancel_cnt));  // 记录日志：相反的订单已取消
		}
		return;  // 撤单后直接返回，等待撤单完成后再执行计算
	}

	// 因为是逐笔发单，所以如果有不需要撤销的未完成单，则暂不发单
	// 此处逐笔指：拆单的回合中上一笔订单量未完结，则不发单；逐笔发单指每个回合成交的单
	if (!decimal::eq(undone, 0))  // 如果有未完成订单（且方向一致）
		return;  // 直接返回，暂不发单（等待上一笔订单完成）

	double curPos = realPos;  // 当前仓位（用于后续计算）

	if (_last_tick == NULL)  // 如果没有最新行情数据
	{
		_ctx->writeLog(fmtutil::format("No lastest tick data of {}, execute later", _code.c_str()));  // 记录日志
		return;  // 直接返回，等待行情数据
	}

	// 检查下单时间间隔
	uint64_t now = TimeUtils::makeTime(_last_tick->actiondate(), _last_tick->actiontime());  // 获取当前时间（毫秒时间戳）
	if (now - _last_place_time < _entrust_span)  // 如果距离上次下单时间小于发单时间间隔
		return;  // 直接返回，等待时间间隔

	if (decimal::eq(curPos, newVol))  // 如果当前仓位已经等于目标仓位
	{
		// 当前仓位和最新仓位匹配时，如果不是全部清仓的需求，则直接退出计算了
		if (!is_clear(_target_pos))  // 如果不是清仓需求
			return;  // 直接返回，不需要执行

		// 如果是清仓的需求，还要再进行对比
		// 如果多头为0，说明已经全部清理掉了，则直接退出
		double lPos = _ctx->getPosition(stdCode, true, 1);  // 获取多头持仓（可用持仓，返回值：多仓>0，空仓<0）
		if (decimal::eq(lPos, 0))  // 如果多头持仓为0
			return;  // 直接返回，清仓完成

		// 如果还有多头仓位，则将目标仓位设置为非0，强制触发
		newVol = -min(lPos, _order_lots);  // 设置目标仓位为负值（卖出），数量为多头持仓和单次发单手数的最小值
		_ctx->writeLog(fmtutil::format("Clearing process triggered, target position of {} has been set to {}", _code.c_str(), newVol));  // 记录日志
	}

	bool bForceClose = is_clear(_target_pos);  // 是否强制平仓标志（清仓时设置为true）

	bool isBuy = decimal::gt(newVol, curPos);  // 判断买卖方向（真实目标仓位 > 当前仓位，则为买入）

	// 如果相比上次没有更新的tick进来，则先不下单，防止开盘前集中下单导致通道被封
	uint64_t curTickTime = (uint64_t)_last_tick->actiondate() * 1000000000 + _last_tick->actiontime();  // 计算当前tick的时间戳（纳秒）
	if (curTickTime <= _last_tick_time)  // 如果当前tick时间小于等于上次tick时间（没有新行情）
	{
		_ctx->writeLog(fmtutil::format("No tick of {} updated, {} <= {}, execute later", _code, curTickTime, _last_tick_time));  // 记录日志
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
	// 这里要对下单数量做一个修正（不能超过需要执行的差量）
	this_qty = min(this_qty, abs(newVol - curPos));  // 下单数量不能超过需要执行的差量

	// 是否开仓，如果持仓大于等于0且买入，或者持仓小于等于0且卖出，就是开仓
	bool isOpen = (isBuy && decimal::ge(curPos, 0)) || (!isBuy && decimal::le(curPos, 0));  // 判断是开仓还是平仓

	// 如果平仓的话
	// 对单次下单做一个修正，保证平仓和开仓不会同时下单
	if (!isOpen)  // 如果是平仓
	{
		this_qty = min(this_qty, abs(curPos));  // 平仓数量不能超过当前持仓的绝对值
	}									

	/*
	 * By Wesley @ 2022.12.15
	 * 增加一个对最小下单数量的修正逻辑
	 */
	if (isOpen && decimal::lt(this_qty, _min_open_lots))  // 如果是开仓 && 下单数量 < 最小开仓数量
	{
		this_qty = _min_open_lots;  // 设置为最小开仓数量
		_ctx->writeLog(fmtutil::format("Lots of {} changed from {} to {} due to minimum open lots", _code, this_qty, _min_open_lots));  // 记录日志
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
			sellPx = _last_tick->bidprice(0);  // 卖出价格用买一价（对手价，主动卖出）
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

	// 根据买卖方向下单
	if (isBuy)  // 如果是买入
	{
		OrderIDs ids = _ctx->buy(stdCode, buyPx, this_qty, bForceClose);  // 提交买入订单，返回订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime(), isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志
	}
	else  // 如果是卖出
	{
		OrderIDs ids = _ctx->sell(stdCode, sellPx, this_qty, bForceClose);  // 提交卖出订单，返回订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _ctx->getCurTime(), isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志
	}

	_last_place_time = now;  // 更新上次下单时间
}
/**
 * @brief 设置新的目标仓位实现
 * 
 * 设置执行单元的目标仓位，执行单元会自动计算差量并执行。
 * 
 * @param stdCode 合约代码
 * @param newVol 新的目标仓位（正数表示多头，负数表示空头，DBL_MAX表示清仓）
 */
void WtMinImpactExeUnit::set_position(const char* stdCode, double newVol)
{
	if (_code.compare(stdCode) != 0)  // 如果合约代码不匹配
		return;  // 直接返回，不做任何处理

	// 如果原来的目标仓位是DBL_MAX，说明已经进入清理逻辑
	// 如果这个时候又设置为0，则直接跳过了（避免清仓过程中被重置）
	if (is_clear(_target_pos) && decimal::eq(newVol, 0))  // 如果正在清仓 && 新目标仓位为0
	{
		_ctx->writeLog(fmtutil::format("{} is in clearing processing, position can not be set to 0", stdCode));  // 记录日志
		return;  // 直接返回，不处理
	}

	if (decimal::eq(_target_pos, newVol))  // 如果目标仓位没有变化
		return;  // 直接返回，不需要重新计算

	_target_pos = newVol;  // 更新目标仓位

	if (is_clear(_target_pos))  // 如果新目标仓位是清仓指令
		_ctx->writeLog(fmtutil::format("{} is set to be in clearing processing", stdCode));  // 记录日志：设置为清仓处理
	else  // 如果新目标仓位不是清仓指令
		_ctx->writeLog(fmtutil::format("Target position of {} is set tb be {}", stdCode, _target_pos));  // 记录日志：设置目标仓位

	do_calc();  // 触发执行计算，根据新目标仓位执行
}

/**
 * @brief 清理全部持仓实现
 * 
 * 清理指定合约的全部持仓，将所有订单撤单并清空目标仓位。
 * 
 * @param stdCode 合约代码
 */
void WtMinImpactExeUnit::clear_all_position(const char* stdCode)
{
	if (_code.compare(stdCode) != 0)  // 如果合约代码不匹配
		return;  // 直接返回，不做任何处理

	_target_pos = DBL_MAX;  // 设置目标仓位为DBL_MAX（清仓标志）

	do_calc();  // 触发执行计算，开始清仓
}
