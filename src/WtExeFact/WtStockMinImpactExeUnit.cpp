/*!
 * \file WtStockMinImpactExeUnit.cpp
 * \project	WonderTrader
 *
 * \author Huerjie
 * \date 2022/06/01
 * 
 * \brief WtStockMinImpactExeUnit股票最小冲击执行单元类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了WtStockMinImpactExeUnit类的所有成员函数，该类用于股票和可转债的智能订单执行，
 * 通过最小市场冲击算法，控制订单价格和数量，减少对市场的影响。
 * 
 * 实现要点：
 * 1. 目标驱动执行：支持股数、金额、比例三种目标模式
 * 2. 价格控制：根据价格模式和价格偏移计算订单价格
 * 3. 数量控制：根据配置的单次发单手数或比例下单
 * 4. 订单管理：跟踪订单状态，支持超时撤单
 * 5. 市场数据响应：根据Tick数据实时调整订单策略
 * 6. 账户管理：根据账户资金动态调整执行策略
 * 7. 错单检测：检测无法撤单的订单（可能是错单）
 * 
 * 股票市场特点：
 * - 最小下单单位：普通股票100股，科创板股票200股，可转债10张
 * - T+1交易规则：股票T+1，可转债T+0
 * - 涨跌停限制：涨跌停价的挂单不能撤单
 * - 账户资金限制：买入受可用资金限制，卖出受持仓限制
 */
#include "WtStockMinImpactExeUnit.h"  // WtStockMinImpactExeUnit类定义

/**
 * @brief 工厂名称外部声明
 * 
 * 声明FACT_NAME常量，该常量在WtExeFact.cpp中定义。
 */
extern const char* FACT_NAME;

/**
 * @brief WtStockMinImpactExeUnit构造函数实现
 * 
 * 创建股票最小冲击执行单元实例，初始化所有成员变量为默认值。
 */
WtStockMinImpactExeUnit::WtStockMinImpactExeUnit()
	: _last_tick(NULL)  // 上一笔行情数据指针初始化为NULL
	, _comm_info(NULL)  // 合约信息指针初始化为NULL
	, _price_mode(0)  // 价格模式初始化为0（最新价）
	, _price_offset(0)  // 价格偏移跳数初始化为0
	, _expire_secs(0)  // 订单超时秒数初始化为0（不超时）
	//, _cancel_cnt(0)  // 在途撤单量初始化为0（已注释，不使用）
	, _target_pos(0)  // 目标仓位初始化为0
	, _cancel_times(0)  // 撤单次数初始化为0
	, _last_place_time(0)  // 上个下单时间初始化为0
	, _last_tick_time(0)  // 上个tick时间初始化为0
	, _is_clear{ false }  // 是否清仓标志初始化为false
	, _min_order{ 0 }  // 最小下单数量初始化为0
	, _is_KC{ false }  // 是否科创板标志初始化为false
	, _is_cancel_unmanaged_order{ true }  // 是否撤销未管理订单标志初始化为true
	, _is_finish{ true }  // 是否完成标志初始化为true
	, _is_first_tick{ true }  // 是否第一笔tick标志初始化为true
	, _is_ready{ false }  // 是否就绪标志初始化为false
	, _is_total_money_ready{ false }  // 是否总金额就绪标志初始化为false
{
	// 构造函数体为空，所有初始化工作都在初始化列表中完成
}

/**
 * @brief WtStockMinImpactExeUnit析构函数实现
 * 
 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
 */
WtStockMinImpactExeUnit::~WtStockMinImpactExeUnit()
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
const char* WtStockMinImpactExeUnit::getFactName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 获取执行单元名称实现
 * 
 * 返回执行单元的名称，用于标识和管理。
 * 
 * @return const char* 返回执行单元名称字符串（"WtStockMinImpactExeUnit"）
 */
const char* WtStockMinImpactExeUnit::getName()
{
	return "WtStockMinImpactExeUnit";  // 返回执行单元名称
}

/**
 * @brief 初始化执行单元实现
 * 
 * 初始化执行单元，加载配置参数，获取合约信息和交易时段信息。
 * 该函数会识别科创板股票、可转债等特殊品种，并设置相应的最小下单数量。
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
 *   - total_money：执行器使用的金额（可选，配合目标比例使用，该字段留空或值小于0，则以账户当前余额为准）
 *   - is_cancel_unmanaged_order：是否撤销未管理订单（可选，默认true）
 *   - max_cancel_time：最大撤单次数（可选，超过此次数仍无法撤单的订单视为错单）
 *   - min_order：最小下单数量（可选，默认根据品种自动确定）
 */
void WtStockMinImpactExeUnit::init(ExecuteContext* ctx, const char* stdCode, WTSVariant* cfg)
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
	if (cfg->has("total_money"))  // 如果配置中有总金额参数
	{
		_is_total_money_ready = true;  // 设置总金额就绪标志为true
		_total_money = cfg->getDouble("total_money");  // 执行器使用的金额，配合目标比例使用，该字段留空或值小于0，则以账户当前余额为准
	}
	if (cfg->has("is_cancel_unmanaged_order"))  // 如果配置中有是否撤销未管理订单参数
		_is_cancel_unmanaged_order = cfg->getBoolean("is_cancel_unmanaged_order");  // 是否撤销未管理订单（默认true）
	if (cfg->has("max_cancel_time"))  // 如果配置中有最大撤单次数参数
		_max_cancel_time = cfg->getInt32("max_cancel_time");  // 最大撤单次数（超过此次数仍无法撤单的订单视为错单）

	// 识别科创板股票（代码>=688000）
	int code = std::stoi(StrUtil::split(stdCode, ".")[2]);  // 提取合约代码的数字部分
	if (code >= 688000)  // 如果代码>=688000（科创板股票）
	{
		_is_KC = true;  // 设置科创板标志为true
	}
	_min_hands = get_minOrderQty(stdCode);  // 获取最小下单数量（根据品种自动确定：普通股票100股，科创板200股，可转债10张）
	if (cfg->has("min_order"))  // 如果配置中有最小下单数参数
		_min_order = cfg->getDouble("min_order");  // 最小下单数（用户自定义）

	if (_min_order != 0)  // 如果用户设置了最小下单数
	{
		if (_is_KC)  // 如果是科创板股票
		{
			_min_order = max(_min_order, _min_hands);  // 最小下单数 = max(用户设置值, 品种最小下单数)，确保不小于200股
		}
		else  // 如果不是科创板股票
		{
			//_min_order = max(_min_order, _min_hands);  // 原代码：确保不小于品种最小下单数
			_min_order = min(_min_order, _min_hands);  // 2023.6.5-zhaoyk：最小下单数 = min(用户设置值, 品种最小下单数)，确保不超过品种限制
		}
	}

	// 确定T0交易模式（可转债等支持T+0交易）
	if (_comm_info->getTradingMode() == TradingMode::TM_Long)  // 如果交易模式为T+0（可转债等）
		_is_t0 = true;  // 设置T+0标志为true

	//auto ticks = _ctx->getTicks(_code.c_str(),1);  // 原代码：获取历史tick数据（已注释）
	//ticks->release();  // 原代码：释放tick数据（已注释）
	// 记录初始化日志，输出所有配置参数
	ctx->writeLog(fmt::format("MiniImpactExecUnit {} inited, order price: {} ± {} ticks, order expired: {} secs, order timespan:{} millisec, order qty: {} @ {:.2f} min_order: {:.2f} is_cancel_unmanaged_order: {}",
		stdCode, PriceModeNames[_price_mode + 1], _price_offset, _expire_secs, _entrust_span, _by_rate ? "byrate" : "byvol", _by_rate ? _qty_rate : _order_lots, _min_order, _is_cancel_unmanaged_order ? "true" : "false").c_str());
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
void WtStockMinImpactExeUnit::on_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled)
{
	{
		StdUniqueLock lock(_mtx_calc);  // 获取计算逻辑的互斥锁，保证线程安全
		_ctx->writeLog(fmtutil::format("on_order localid:{} stdCode:{} isBuy:{} leftover:{} price:{} isCanceled:{}", localid, stdCode, isBuy, leftover, price, isCanceled));  // 记录订单回报日志
		if (!_orders_mon.has_order(localid))  // 如果没有对应订单，直接返回（可能是其他执行单元的订单）
		{
			// 非管理的合约订单
			_ctx->writeLog(fmtutil::format("{} {} didnt in mon", stdCode, localid));  // 记录日志：订单不在订单管理器中
			return;  // 直接返回，不做任何处理
		}

		if (isCanceled || leftover == 0)  // 如果订单已撤销或剩余订单为0（全部成交）
		{
			_orders_mon.erase_order(localid);  // 从订单管理器中删除该订单
			if (isCanceled)  // 如果订单已撤销
				_ctx->writeLog(fmtutil::format("{} {} canceled, earse from mon", stdCode, localid));  // 记录日志：订单已撤销，从订单管理器中删除
			else  // 如果订单全部成交
				_ctx->writeLog(fmtutil::format("{} {} done, earse from mon", stdCode, localid));  // 记录日志：订单已完成，从订单管理器中删除
		}

		if (leftover == 0 && !isCanceled)  // 如果订单全部成交（剩余为0且未撤销）
			_cancel_times = 0;  // 重置撤单次数（订单已成交，不需要继续撤单）
	}

	// 如果有撤单，也触发重新计算（撤单后需要重新下单）
	if (isCanceled)  // 如果订单已撤销
	{
		_ctx->writeLog(fmt::format("Order {} of {} canceled, recalc will be done", localid, stdCode).c_str());  // 记录撤单日志
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
void WtStockMinImpactExeUnit::on_channel_ready()
{
	_ctx->writeLog("=================================channle ready==============================");  // 记录日志：交易通道就绪
	_is_ready = true;  // 设置就绪标志为true
	check_unmanager_order();  // 检查并处理未管理的订单
	do_calc();  // 触发执行计算，开始下单
}

/**
 * @brief 交易通道丢失回调实现
 * 
 * 当交易通道断开时调用此函数，停止下单操作。
 * 当前版本该函数为空，不做任何处理。
 */
void WtStockMinImpactExeUnit::on_channel_lost()
{
	// 交易通道丢失时，停止下单操作（当前版本不做任何处理）
}

/**
 * @brief 账户信息回调实现
 * 
 * 当账户信息更新时调用此函数，更新可用资金等信息。
 * 
 * @param currency 货币类型（如"CNY"表示人民币）
 * @param prebalance 上日余额
 * @param balance 当前余额
 * @param dynbalance 动态余额
 * @param avaliable 可用资金（买入时需要考虑此限制）
 * @param closeprofit 平仓盈亏
 * @param dynprofit 浮动盈亏
 * @param margin 保证金
 * @param fee 手续费
 * @param deposit 入金
 * @param withdraw 出金
 */
void WtStockMinImpactExeUnit::on_account(const char* currency, double prebalance, double balance, double dynbalance, double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw)
{
	if (strcmp(currency, "CNY") == 0)  // 如果是人民币账户
	{
		_ctx->writeLog(fmtutil::format("avaliable update {}->:{}", _avaliable, avaliable));  // 记录日志：可用资金更新
		_avaliable = avaliable;  // 更新可用资金（买入时需要考虑此限制）
	}
}

/**
 * @brief 检查未管理订单实现
 * 
 * 检查是否有未管理的订单（可能是上次启动时的未完成单或外部挂单），
 * 如果配置允许撤销未管理订单，则自动撤单。
 */
void WtStockMinImpactExeUnit::check_unmanager_order()
{
	double undone = _ctx->getUndoneQty(_code.c_str());  // 获取未完成数量（从交易通道获取）
	_orders_mon.clear_orders();  // 清空订单管理器中的所有订单（重新开始管理）

	if (!decimal::eq(undone, 0) && _is_cancel_unmanaged_order)  // 如果未完成单不为0 && 允许撤销未管理订单
	{
		_ctx->writeLog(fmt::format("{} Unmanaged live orders with qty {} of {} found, cancel all", _code, undone, _code.c_str()).c_str());  // 记录日志：发现未管理订单
		bool isBuy = (undone > 0);  // 根据未完成数量的正负判断方向（正数表示买入，负数表示卖出）
		OrderIDs ids = _ctx->cancel(_code.c_str(), isBuy);  // 根据方向撤单，返回撤单的订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _now);  // 将撤单的订单ID推入订单管理器
		for (auto id : ids)  // 遍历所有撤单的订单ID
			_ctx->writeLog(fmt::format("{} mon push unmanager order {} enter time:{}", _code.c_str(), id, _now).c_str());  // 记录日志：将未管理订单推入订单管理器
	}
}

/**
 * @brief Tick数据回调实现
 * 
 * 当有新的Tick数据时调用此函数，更新最新行情，触发执行计算。
 * 该函数会检查订单超时，自动撤单，并检测错单（超过最大撤单次数仍无法撤单的订单）。
 * 
 * @param newTick 最新的Tick数据指针，包含最新价格、成交量、持仓量等信息
 */
void WtStockMinImpactExeUnit::on_tick(WTSTickData* newTick)
{
	_now = TimeUtils::getLocalTimeNow();  // 更新当前时间（毫秒时间戳）
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

	// 如果相比上次没有更新的tick进来，则先不下单，防止开盘前集中下单导致通道被封
	uint64_t curTickTime = TimeUtils::makeTime(_last_tick->actiondate(), _last_tick->actiontime());  // 计算当前tick的时间戳（毫秒）
	if (curTickTime <= _last_tick_time)  // 如果当前tick时间小于等于上次tick时间（没有新行情）
	{
		_ctx->writeLog(fmt::format("No tick of {} updated, {} <= {}, execute later",
			_code, curTickTime, _last_tick_time).c_str());  // 记录日志
		return;  // 直接返回，等待新行情
	}
	_last_tick_time = curTickTime;  // 更新上次tick时间

	/*
	 * 这里可以考虑一下
	 * 如果写的上一次丢出去的单子不够达到目标仓位
	 * 那么在新的行情数据进来的时候可以再次触发核心逻辑
	 */

	// 枚举所有订单，记录订单状态（用于调试）
	_orders_mon.enumOrder([this](uint32_t localid, uint64_t entertime, bool cancancel) {  // 遍历所有订单
		_ctx->writeLog(fmtutil::format("[{}]{} entertime:{} cancancel:{} now:{} last_tick_time:{} live_time:{}", _code, localid, entertime, cancancel, _now, _last_tick_time, _now - entertime));  // 记录订单状态日志
	});

	// 检查订单是否超时，如果超时则撤单
	if (_expire_secs != 0 && _orders_mon.has_order())  // 如果订单超时秒数!=0 && 有订单
	{
		_orders_mon.check_orders(_expire_secs, _now, [this](uint32_t localid) {  // 遍历所有订单，检查是否超时
			if (_ctx->cancel(localid))  // 如果撤单成功
			{
				_ctx->writeLog(fmt::format("[{}] Expired order of {} canceled", localid, _code.c_str()).c_str());  // 记录日志：订单过期，已撤单
				if (_cancel_map.find(localid) == _cancel_map.end())  // 如果订单ID不在撤单次数映射中
				{
					_cancel_map[localid] = 0;  // 初始化撤单次数为0
				}
				_cancel_map[localid] += 1;  // 增加该订单的撤单次数
			}
		});
	}
	// 检测错单（超过最大撤单次数仍无法撤单的订单）
	if (!_cancel_map.empty())  // 如果有订单的撤单次数记录
	{
		std::vector<uint32_t> erro_cancel_orders{};  // 错单列表
		for (auto item : _cancel_map)  // 遍历所有撤单次数记录
		{
			if (item.second > _max_cancel_time)  // 如果撤单次数超过最大撤单次数
			{
				erro_cancel_orders.push_back(item.first);  // 将该订单ID加入错单列表
			}
		}
		for (uint32_t localid : erro_cancel_orders)  // 遍历所有错单
		{
			_cancel_map.erase(localid);  // 从撤单次数映射中删除该订单
			_orders_mon.erase_order(localid);  // 从订单管理器中删除该订单（强制删除）
			_ctx->writeLog(fmtutil::format("error order:{} canceled by {} times,erase forcely", localid, _max_cancel_time));  // 记录日志：错单强制删除
		}
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
void WtStockMinImpactExeUnit::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	// 成交回报会在on_tick中触发重新计算（当前版本不做任何处理）
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
void WtStockMinImpactExeUnit::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	if (!bSuccess)  // 如果下单失败
	{
		StdUniqueLock lock(_mtx_calc);  // 获取计算逻辑的互斥锁，保证线程安全
		// 如果不是我发出去的订单，我就不管了
		if (!_orders_mon.has_order(localid))  // 如果订单管理器中不存在该订单
			return;  // 直接返回，不做任何处理

		_orders_mon.erase_order(localid);  // 从订单管理器中删除该订单
		_ctx->writeLog(fmtutil::format("{} {} entrust failed erase from mon", _code.c_str(), localid));  // 记录日志：下单失败，从订单管理器中删除
	}
	// 委托不成功，重新处理
	do_calc();  // 触发重新计算，重新下单
}

/**
 * @brief 设置新的目标仓位实现
 * 
 * 设置执行单元的目标仓位（股数模式），执行单元会将仓位调整到目标值。
 * 
 * @param stdCode 合约代码
 * @param newVol 新的目标仓位（正数表示目标股数，0表示清仓，负数表示错误值）
 */
void WtStockMinImpactExeUnit::set_position(const char* stdCode, double newVol)
{
	if (_code.compare(stdCode) != 0)  // 如果合约代码不匹配
		return;  // 直接返回，不做任何处理

	// 如果原来的目标仓位是DBL_MAX，说明已经进入清理逻辑
	// 如果这个时候又设置为0，则直接跳过了
	if (is_clear() && decimal::eq(newVol, 0))  // 如果正在清仓 && 新目标仓位为0
	{
		_ctx->writeLog(fmt::format("{} is in clearing processing, position can not be set to 0", stdCode).c_str());  // 记录日志：正在清仓，不能设置为0
		return;  // 直接返回，不做任何处理
	}
	double cur_pos = _ctx->getPosition(stdCode);  // 获取当前仓位

	if (decimal::eq(cur_pos, newVol))  // 如果当前仓位已经等于目标仓位
		return;  // 直接返回，不需要执行

	if (decimal::lt(newVol, 0))  // 如果目标仓位为负数（股票不支持负数仓位）
	{
		_ctx->writeLog(fmt::format("{} is an error stock target position", newVol).c_str());  // 记录日志：错误的目标仓位
		return;  // 直接返回，不做任何处理
	}

	_target_pos = newVol;  // 更新目标仓位

	_target_mode = TargetMode::stocks;  // 设置目标模式为股数模式
	if (is_clear())  // 如果正在清仓
		_ctx->writeLog(fmt::format("{} is set to be in clearing processing", stdCode).c_str());  // 记录日志：设置为清仓处理
	else  // 如果不是清仓
		_ctx->writeLog(fmt::format("Target position of {} is set tb be {}", stdCode, _target_pos).c_str());  // 记录日志：目标仓位已设置

	_is_finish = false;  // 设置完成标志为false（重新开始执行）
	_start_time = TimeUtils::getLocalTimeNow();  // 记录开始时间
	WTSTickData* tick = _ctx->grabLastTick(_code.c_str());  // 获取最新tick数据
	if (tick)  // 如果tick数据不为空
	{
		_start_price = tick->price();  // 记录开始价格（用于后续计算）
		tick->release();  // 释放tick数据
	}
	do_calc();  // 触发执行计算，根据新目标仓位执行
}

/**
 * @brief 清理全部持仓实现
 * 
 * 设置执行单元为清仓模式，将目标仓位设置为0，执行单元会将所有持仓卖出。
 * 
 * @param stdCode 合约代码
 */
void WtStockMinImpactExeUnit::clear_all_position(const char* stdCode)
{
	if (_code.compare(stdCode) != 0)  // 如果合约代码不匹配
		return;  // 直接返回，不做任何处理

	_is_clear = true;  // 设置清仓标志为true
	_target_pos = 0;  // 设置目标仓位为0（清仓）
	_target_amount = 0;  // 设置目标金额为0（清仓）
	do_calc();  // 触发执行计算，开始清仓
}

/**
 * @brief 检查是否清仓实现
 * 
 * 检查执行单元是否处于清仓模式。
 * 
 * @return bool true表示正在清仓，false表示不清仓
 */
inline bool WtStockMinImpactExeUnit::is_clear()
{
	return _is_clear;  // 返回清仓标志
}

/**
 * @brief 执行计算实现
 * 
 * 核心计算函数，根据目标仓位计算需要执行的订单数量，
 * 并根据市场行情和配置参数决定是否下单、撤单等操作。
 * 
 * 执行流程：
 * 1. 检查是否有最新行情数据、是否已完成、是否就绪
 * 2. 获取未完成订单数量、当前仓位、可用仓位
 * 3. 根据T+1规则调整目标仓位（卖出时不能超过可用仓位）
 * 4. 检查是否已达到目标仓位（考虑碎股情况）
 * 5. 检查是否需要撤单（未完成订单与目标方向相反）
 * 6. 检查是否有未完成订单（逐笔发单模式，有未完成订单则暂不发单）
 * 7. 检查下单时间间隔
 * 8. 计算下单数量（根据配置的单次发单手数或比例，考虑资金限制和碎股）
 * 9. 计算订单价格（根据价格模式和价格偏移）
 * 10. 检查涨跌停价（涨跌停价的挂单不能撤单）
 * 11. 下单
 */
void WtStockMinImpactExeUnit::do_calc()
{
	if (!_last_tick)  // 如果没有最新行情数据
		return;  // 直接返回，等待行情数据
	if (_is_finish)  // 如果已完成
		return;  // 直接返回，不需要执行
	if (!_is_ready)  // 如果未就绪
	{
		_ctx->writeLog(fmtutil::format("{} wait channel ready", _code));  // 记录日志：等待通道就绪
		return;  // 直接返回，等待通道就绪
	}

	// 这里加一个锁，主要原因是实盘过程中发现
	// 在修改目标仓位的时候，会触发一次do_calc
	// 而ontick也会触发一次do_calc，两次调用是从两个线程分别触发的，所以会出现同时触发的情况
	// 如果不加锁，就会引起问题
	// 这种情况在原来的SimpleExecUnit没有出现，因为SimpleExecUnit只在set_position的时候触发
	StdUniqueLock lock(_mtx_calc);  // 获取计算逻辑的互斥锁，保证线程安全

	const char* stdCode = _code.c_str();  // 获取合约代码字符串

	double undone = _ctx->getUndoneQty(stdCode);  // 获取未完成订单数量（正数表示买入未完成，负数表示卖出未完成）
	// 总仓位，等于昨仓 + 今仓买入的
	double curPos = _ctx->getPosition(stdCode, false);  // 获取总仓位（昨仓 + 今仓买入的）
	// 可用仓位，即昨仓的
	double vailyPos = _ctx->getPosition(stdCode, true);  // 获取可用仓位（即昨仓的，T+1规则下只能卖出昨仓）
	if (_is_t0)  // 如果是T+0交易模式（可转债等）
		vailyPos = curPos;  // 可用仓位 = 总仓位（T+0规则下可以卖出今仓）
	// 根据T+1规则调整目标仓位（卖出时不能超过可用仓位）
	double target_pos = max(curPos - vailyPos, _target_pos);  // 目标仓位 = max(今仓买入的, 用户设置的目标仓位)，确保卖出时不超过可用仓位

	if (!decimal::eq(target_pos, _target_pos))  // 如果调整后的目标仓位与用户设置的目标仓位不一致
	{
		_ctx->writeLog(fmtutil::format("{} can sell hold pos not enough, target adjust {}->{}", stdCode, _target_pos, target_pos));  // 记录日志：可用仓位不足，调整目标仓位
		_target_pos = target_pos;  // 更新目标仓位
	}

	// 补一次（如果开始价格未设置，则设置为当前价格）
	if (decimal::ge(_start_price, 0))  // 如果开始价格>=0（未设置或已设置）
	{
		_start_price = _last_tick->price();  // 设置开始价格为当前价格
	}

	double diffPos = target_pos - curPos;  // 计算目标差量（目标仓位 - 当前仓位，正数表示需要买入，负数表示需要卖出）
	_ctx->writeLog(fmtutil::format("{}: target: {} hold:{} left {} wait to execute", _code.c_str(), target_pos, curPos, diffPos));  // 记录日志：目标仓位、当前仓位、待执行差量
	// 在判断的时候，要两边四舍五入，防止一些碎股导致一直无法完成执行
	if (decimal::eq(round_hands(target_pos, _min_hands), round_hands(curPos, _min_hands)) && !(target_pos == 0 && curPos < _min_hands && curPos > target_pos))  // 如果目标仓位和当前仓位四舍五入后相等（考虑碎股）
	{
		_ctx->writeLog(fmtutil::format("{}: target position {} set finish", _code.c_str(), _target_pos));  // 记录日志：目标仓位已达成
		_is_finish = true;  // 设置完成标志为true
		return;  // 直接返回，不需要执行
	}

	bool isBuy = decimal::gt(diffPos, 0);  // 判断买卖方向（目标差量 > 0，则为买入）
	// 有未完成订单，与实际仓位变动方向相反
	// 则需要撤销现有订单
	if (decimal::lt(diffPos * undone, 0))  // 如果差量与未完成订单方向相反（diffPos * undone < 0）
	{
		_ctx->writeLog(fmt::format("{} undone:{} diff:{} cancel", stdCode, undone, diffPos).c_str());  // 记录日志：未完成订单与目标方向相反，需要撤单
		bool isBuy = decimal::gt(undone, 0);  // 根据未完成订单的正负判断方向（正数表示买入，负数表示卖出）
		OrderIDs ids = _ctx->cancel(stdCode, isBuy);  // 根据方向撤单，返回撤单的订单ID列表
		if (!ids.empty())  // 如果撤单成功（返回了订单ID列表）
		{
			_orders_mon.push_order(ids.data(), ids.size(), _now);  // 将撤单的订单ID推入订单管理器
			for (auto localid : ids)  // 遍历所有撤单的订单ID
			{
				_ctx->writeLog(fmt::format("{} mon push wait cancel order {} enter time:{}", _code.c_str(), localid, _now).c_str());  // 记录日志：将等待撤单的订单推入订单管理器
				_ctx->writeLog(fmt::format("[{}] live opposite order of {} canceled", localid, _code.c_str()).c_str());  // 记录日志：相反的订单已取消
			}
		}
		return;  // 撤单后直接返回，等待撤单完成后再执行计算
	}

	// 因为是逐笔发单，所以如果有不需要撤销的未完成单，则暂不发单
	if (!decimal::eq(undone, 0))  // 如果有未完成订单（且方向一致）
	{
		_ctx->writeLog(fmtutil::format("{} undone {} wait...", _code, undone));  // 记录日志：有未完成订单，等待
		return;  // 直接返回，暂不发单（等待上一笔订单完成）
	}

	if (_last_tick == NULL)  // 如果没有最新行情数据
	{
		_ctx->writeLog(fmt::format("No lastest tick data of {}, execute later", _code.c_str()).c_str());  // 记录日志
		return;  // 直接返回，等待行情数据
	}

	// 检查下单时间间隔
	if (_now - _last_place_time < _entrust_span)  // 如果距离上次下单时间小于发单时间间隔
	{
		_ctx->writeLog(fmtutil::format("entrust span {} last_place_time {} _now {}", _entrust_span, _last_place_time, _now));  // 记录日志：下单时间间隔未到
		return;  // 直接返回，等待时间间隔
	}

	double this_qty = _order_lots;  // 单次发单手数（默认值）
	if (_by_rate)  // 如果按照对手盘挂单数的比例下单
	{
		double book_qty = isBuy ? _last_tick->askqty(0) : _last_tick->bidqty(0);  // 获取对手盘挂单量（买入用卖一量，卖出用买一量）
		book_qty = book_qty * _qty_rate;  // 按比例计算下单数量
		//book_qty = round_hands(book_qty, _min_hands);  // 原代码：按品种最小下单数取整
		book_qty = round_hands(book_qty, _min_order);  // 2023.6.5-zhaoyk：按用户设置的最小下单数取整
		book_qty = max(_min_order, book_qty);  // 确保不小于最小下单数
		this_qty = book_qty;  // 更新本次下单数量
	}
	diffPos = abs(diffPos);  // 取目标差量的绝对值
	this_qty = min(this_qty, diffPos);  // 下单数量不能超过目标差量的绝对值
	// 买
	if (isBuy)  // 如果是买入
	{
		// 如果是买的话，要考虑取整和资金余额
		//this_qty = round_hands(this_qty, _min_hands);  // 原代码：按品种最小下单数取整
		this_qty = round_hands(this_qty, _min_order);  // 2023.6.5-zhaoyk：按用户设置的最小下单数取整
		if (_avaliable)  // 如果可用资金不为0
		{
			double max_can_buy = _avaliable / _last_tick->price();  // 根据可用资金计算最大可买数量
			//max_can_buy = (int)(max_can_buy / _min_hands) * _min_hands;  // 原代码：按品种最小下单数取整
			max_can_buy = (int)(max_can_buy / _min_order) * _min_order;  // 2023.6.5-zhaoyk：按用户设置的最小下单数取整
			this_qty = min(max_can_buy, this_qty);  // 下单数量不能超过最大可买数量（受资金限制）
		}
	}
	// 卖要对碎股做检查
	else  // 如果是卖出
	{
		//double chip_stk = vailyPos - int(vailyPos / _min_hands) * _min_hands;  // 原代码：计算碎股（已注释）
		//if (decimal::lt(vailyPos, _min_hands))  // 原代码：如果可用仓位小于品种最小下单数（已注释）
		if (decimal::lt(vailyPos, _min_order))  // 2023.6.5-zhaoyk：如果可用仓位小于用户设置的最小下单数
		{
			this_qty = vailyPos;  // 下单数量 = 可用仓位（全部卖出，包括碎股）
		}
		else  // 如果可用仓位 >= 最小下单数
		{
			//this_qty = round_hands(this_qty, _min_hands);  // 原代码：按品种最小下单数取整（已注释）
			this_qty = round_hands(this_qty, _min_order);  // 2023.6.5-zhaoyk：按用户设置的最小下单数取整
		}
		this_qty = min(vailyPos, this_qty);  // 下单数量不能超过可用仓位（T+1规则限制）
	}

	if (decimal::eq(this_qty, 0))  // 如果本次下单数量为0
		return;  // 直接返回，不需要下单

	double buyPx, sellPx;  // 买入价格和卖出价格
	if (_price_mode == AUTOPX)  // 如果价格模式为自动价格（2）
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
	}
	else  // 如果价格模式不是自动价格
	{
		if (_price_mode == BESTPX)  // 如果价格模式为最优价（-1）
		{
			buyPx = _last_tick->bidprice(0);  // 买入价格用买一价（最优买入价）
			sellPx = _last_tick->askprice(0);  // 卖出价格用卖一价（最优卖出价）
		}
		else if (_price_mode == LASTPX)  // 如果价格模式为最新价（0）
		{
			buyPx = _last_tick->price();  // 买入价格用最新成交价
			sellPx = _last_tick->price();  // 卖出价格用最新成交价
		}
		else if (_price_mode == MARKET)  // 如果价格模式为对手价（1）
		{
			buyPx = _last_tick->askprice(0) + _comm_info->getPriceTick() * _price_offset;  // 买入价格用卖一价+偏移（对手价，主动买入）
			sellPx = _last_tick->bidprice(0) - _comm_info->getPriceTick() * _price_offset;  // 卖出价格用买一价-偏移（对手价，主动卖出）
		}
	}

	// 如果最后价格为0，再做一个修正
	if (decimal::eq(buyPx, 0.0))  // 如果买入价格为0
		buyPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();  // 如果最新价也为0，则用昨收价，否则用最新价

	if (decimal::eq(sellPx, 0.0))  // 如果卖出价格为0
		sellPx = decimal::eq(_last_tick->price(), 0.0) ? _last_tick->preclose() : _last_tick->price();  // 如果最新价也为0，则用昨收价，否则用最新价

	// 根据撤单次数调整价格（撤单次数越多，价格调整越大，用于追单）
	buyPx += _comm_info->getPriceTick() * _cancel_times;  // 买入价格 += 最小变动价位 * 撤单次数（扩大追单步幅）
	sellPx -= _comm_info->getPriceTick() * _cancel_times;  // 卖出价格 -= 最小变动价位 * 撤单次数（扩大追单步幅）

	// 检查涨跌停价
	bool isCanCancel = true;  // 是否可撤单标志（默认true）
	if (!decimal::eq(_last_tick->upperlimit(), 0) && decimal::gt(buyPx, _last_tick->upperlimit()))  // 如果买入价格超过涨停价
	{
		_ctx->writeLog(fmt::format("Buy price {} of {} modified to upper limit price", buyPx, _code.c_str(), _last_tick->upperlimit()).c_str());  // 记录日志：买入价改为上限价
		buyPx = _last_tick->upperlimit();  // 买入价格修正为涨停价
		isCanCancel = false;  // 如果价格被修正为涨跌停价，订单不可撤销（涨跌停价的挂单不能撤单）
	}

	if (!decimal::eq(_last_tick->lowerlimit(), 0) && decimal::lt(sellPx, _last_tick->lowerlimit()))  // 如果卖出价格低于跌停价
	{
		_ctx->writeLog(fmt::format("Sell price {} of {} modified to lower limit price", sellPx, _code.c_str(), _last_tick->lowerlimit()).c_str());  // 记录日志：卖出价改为下限价
		sellPx = _last_tick->lowerlimit();  // 卖出价格修正为跌停价
		isCanCancel = false;  // 如果价格被修正为涨跌停价，订单不可撤销（涨跌停价的挂单不能撤单）
	}

	// 根据买卖方向下单
	if (isBuy)  // 如果是买入
	{
		OrderIDs ids = _ctx->buy(stdCode, buyPx, this_qty);  // 提交买入订单，返回订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _now, isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志
		for (auto id : ids)  // 遍历所有订单ID
		{
			_ctx->writeLog(fmt::format("{} mon push buy order {} enter time:{}", _code.c_str(), id, _now).c_str());  // 记录日志：将买入订单推入订单管理器
		}
	}
	else  // 如果是卖出
	{
		OrderIDs ids = _ctx->sell(stdCode, sellPx, this_qty);  // 提交卖出订单，返回订单ID列表
		_orders_mon.push_order(ids.data(), ids.size(), _now, isCanCancel);  // 将订单ID推入订单管理器，记录创建时间和可撤单标志
		for (auto id : ids)  // 遍历所有订单ID
		{
			_ctx->writeLog(fmt::format("{} mon push sell order {} enter time:{}", _code.c_str(), id, _now).c_str());  // 记录日志：将卖出订单推入订单管理器
		}
	}

	_last_place_time = _now;  // 更新上次下单时间
}