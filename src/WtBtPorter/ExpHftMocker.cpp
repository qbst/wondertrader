/*!
 * \file ExpHftMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief ExpHftMocker HFT策略扩展模拟器类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了ExpHftMocker类的所有成员函数，该类继承自HftMocker基类，
 * 主要功能是将回测引擎内部的事件转发给外部语言注册的回调函数。
 * 
 * 实现要点：
 * 1. 所有事件处理函数都遵循"先调用基类，再通知外部"的模式（除了on_session_end和Level2行情事件）
 * 2. 通过getRunner()获取WtBtRunner单例对象，统一管理回调函数调用
 * 3. 使用_context_id标识策略实例，确保回调函数能正确识别策略
 * 4. 在on_tick_updated中检查订阅状态，只处理已订阅的合约
 * 5. 在on_bar中需要将K线周期转换为标准格式（如"m5"、"d1"等）
 * 6. Level2行情事件（订单队列、订单明细、逐笔成交）不调用基类，直接转发给外部语言
 * 
 * 事件转发流程：
 * - 策略初始化：on_init() -> ctx_on_init() -> on_initialize_event()
 * - 交易日事件：on_session_begin/end() -> ctx_on_session_event() -> on_session_event()
 * - Tick更新：on_tick_updated() -> ctx_on_tick()
 * - K线闭合：on_bar() -> ctx_on_bar()
 * - 交易通道就绪：on_channel_ready() -> hft_on_channel_ready()
 * - 订单状态变化：on_order() -> hft_on_order()
 * - 成交回报：on_trade() -> hft_on_trade()
 * - 委托回报：on_entrust() -> hft_on_entrust()
 * - Level2行情：on_ordque_updated() -> hft_on_order_queue()
 *                on_orddtl_updated() -> hft_on_order_detail()
 *                on_trans_updated() -> hft_on_transaction()
 * - 回测结束：on_bactest_end() -> on_backtest_end()
 */
#include "ExpHftMocker.h"  // ExpHftMocker类定义
#include "WtBtRunner.h"  // WtBtRunner类定义

#include "../Share/StrUtil.hpp"  // 字符串工具函数

/**
 * @brief 获取WtBtRunner单例对象的外部声明
 * 
 * 声明getRunner()函数，用于获取WtBtRunner单例对象引用
 * 该函数在WtBtPorter.cpp中定义
 */
extern WtBtRunner& getRunner();

/**
 * @brief ExpHftMocker构造函数实现
 * 
 * 创建HFT策略扩展模拟器实例，调用基类HftMocker的构造函数初始化回测环境
 * 
 * @param replayer 历史数据回放器指针，用于回放历史数据驱动策略执行
 * @param name 策略名称，用于标识策略实例
 */
ExpHftMocker::ExpHftMocker(HisDataReplayer* replayer, const char* name)
	: HftMocker(replayer, name)  // 调用基类构造函数，初始化回测环境
{
	// 构造函数体为空，所有初始化工作都在基类构造函数中完成
}

/**
 * @brief K线闭合事件处理函数实现
 * 
 * 当订阅的K线周期完成并生成新的K线时调用此函数，执行以下操作：
 * 1. 检查K线数据是否有效，如果为NULL则直接返回
 * 2. 将K线周期转换为标准格式（如"m5"表示5分钟K线，"d1"表示日线）
 * 3. 调用基类HftMocker::on_bar()，执行基类的K线闭合逻辑
 * 4. 通过WtBtRunner通知外部语言有新的K线闭合事件（ctx_on_bar）
 * 
 * @param stdCode 标准合约代码（如"SHFE.rb2305"）
 * @param period K线周期（如"m"表示分钟，"d"表示日）
 * @param times K线倍数（如period为"m"时，times为5表示5分钟K线）
 * @param newBar 新生成的K线数据指针，包含开盘价、最高价、最低价、收盘价、成交量等信息
 */
void ExpHftMocker::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	// 检查K线数据是否有效
	if (newBar == NULL)  // 如果K线数据为空，直接返回，不处理
		return;

	// 将K线周期转换为标准格式
	// 如果周期以'd'开头（如"d"），则格式化为"d{times}"（如"d1"表示日线）
	// 否则格式化为"m{times}"（如"m5"表示5分钟K线）
	std::string realPeriod;
	if (period[0] == 'd')  // 日线周期
		realPeriod = StrUtil::printf("%s%u", period, times);  // 格式化为"d{times}"
	else  // 分钟周期
		realPeriod = StrUtil::printf("m%u", times);  // 格式化为"m{times}"

	// 调用基类的K线闭合逻辑，更新策略内部状态，触发策略计算
	HftMocker::on_bar(stdCode, period, times, newBar);

	// 通知外部语言有新的K线闭合事件
	// 参数：策略上下文ID、标准合约代码、标准格式的K线周期、新生成的K线数据指针、ET_HFT表示HFT引擎类型
	getRunner().ctx_on_bar(_context_id, stdCode, realPeriod.c_str(), newBar, ET_HFT);
}

/**
 * @brief 交易通道就绪事件处理函数实现
 * 
 * 当HFT策略的交易通道连接成功并准备就绪时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_channel_ready()，执行基类的通道就绪逻辑
 * 2. 通过WtBtRunner通知外部语言交易通道已就绪（hft_on_channel_ready）
 * 
 * 注意：交易通道ID参数为空字符串，因为回测模式下没有实际的交易通道。
 */
void ExpHftMocker::on_channel_ready()
{
	// 调用基类的通道就绪逻辑，完成交易通道的初始化工作
	HftMocker::on_channel_ready();

	// 通知外部语言交易通道已就绪
	// 参数：策略上下文ID、交易通道ID（回测模式下为空字符串）
	getRunner().hft_on_channel_ready(_context_id, "");
}

/**
 * @brief 委托回报事件处理函数实现
 * 
 * 当HFT策略的委托单提交后收到回报时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_entrust()，执行基类的委托回报逻辑
 * 2. 通过WtBtRunner通知外部语言委托回报信息（hft_on_entrust）
 * 
 * @param localid 本地订单ID（下单时返回的订单ID）
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功，true表示委托成功，false表示委托失败
 * @param message 返回消息（如果失败，包含失败原因）
 * @param userTag 用户标签（用于标识该笔交易）
 */
void ExpHftMocker::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag)
{
	// 调用基类的委托回报逻辑，更新订单状态
	HftMocker::on_entrust(localid, stdCode, bSuccess, message, userTag);

	// 通知外部语言委托回报信息
	// 参数：策略上下文ID、本地订单ID、标准合约代码、是否成功、返回消息、用户标签
	getRunner().hft_on_entrust(_context_id, localid, stdCode, bSuccess, message, userTag);
}

/**
 * @brief 策略初始化事件处理函数实现
 * 
 * 当策略初始化完成时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_init()，执行基类的初始化逻辑
 * 2. 通过WtBtRunner通知外部语言策略已初始化完成（ctx_on_init）
 * 3. 触发引擎初始化事件（on_initialize_event），通知外部语言引擎已初始化
 */
void ExpHftMocker::on_init()
{
	// 调用基类的初始化逻辑，完成策略的基础初始化工作
	HftMocker::on_init();

	// 通知外部语言策略已初始化完成
	// 参数：策略上下文ID、ET_HFT表示HFT引擎类型
	getRunner().ctx_on_init(_context_id, ET_HFT);

	// 触发引擎初始化事件，通知外部语言回测引擎已初始化完成
	getRunner().on_initialize_event();
}

/**
 * @brief 交易日开始事件处理函数实现
 * 
 * 当新的交易日开始时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_session_begin()，执行基类的交易日开始逻辑
 * 2. 通过WtBtRunner通知外部语言策略的交易日已开始（ctx_on_session_event）
 * 3. 触发引擎交易日开始事件（on_session_event），通知外部语言引擎交易日已开始
 * 
 * @param uDate 当前交易日（格式：YYYYMMDD）
 */
void ExpHftMocker::on_session_begin(uint32_t uDate)
{
	// 调用基类的交易日开始逻辑，完成策略的交易日初始化工作
	HftMocker::on_session_begin(uDate);

	// 通知外部语言策略的交易日已开始
	// 参数：策略上下文ID、当前交易日、true表示交易日开始、ET_HFT表示HFT引擎类型
	getRunner().ctx_on_session_event(_context_id, uDate, true, ET_HFT);
	
	// 触发引擎交易日开始事件，通知外部语言回测引擎的交易日已开始
	getRunner().on_session_event(uDate, true);
}

/**
 * @brief 交易日结束事件处理函数实现
 * 
 * 当交易日结束时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_session_end()，执行基类的交易日结束逻辑
 * 2. 通过WtBtRunner通知外部语言策略的交易日已结束（ctx_on_session_event）
 * 3. 触发引擎交易日结束事件（on_session_event），通知外部语言引擎交易日已结束
 * 
 * @param uDate 当前交易日（格式：YYYYMMDD）
 */
void ExpHftMocker::on_session_end(uint32_t uDate)
{
	// 调用基类的交易日结束逻辑，完成策略的交易日清理工作
	HftMocker::on_session_end(uDate);

	// 通知外部语言策略的交易日已结束
	// 参数：策略上下文ID、当前交易日、false表示交易日结束、ET_HFT表示HFT引擎类型
	getRunner().ctx_on_session_event(_context_id, uDate, false, ET_HFT);
	
	// 触发引擎交易日结束事件，通知外部语言回测引擎的交易日已结束
	getRunner().on_session_event(uDate, false);
}

/**
 * @brief 订单状态变化事件处理函数实现
 * 
 * 当HFT策略的订单状态发生变化时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_order()，执行基类的订单状态变化逻辑
 * 2. 通过WtBtRunner通知外部语言订单状态变化信息（hft_on_order）
 * 
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy true表示买入订单，false表示卖出订单
 * @param totalQty 订单总数量
 * @param leftQty 剩余未成交数量
 * @param price 订单价格
 * @param isCanceled 是否已撤单，true表示已撤单，false表示未撤单
 * @param userTag 用户标签
 */
void ExpHftMocker::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag)
{
	// 调用基类的订单状态变化逻辑，更新订单状态
	HftMocker::on_order(localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, userTag);

	// 通知外部语言订单状态变化信息
	// 参数：策略上下文ID、本地订单ID、标准合约代码、是否买入、订单总数量、剩余未成交数量、订单价格、是否已撤单、用户标签
	getRunner().hft_on_order(_context_id, localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, userTag);
}

/**
 * @brief Tick数据更新事件处理函数实现
 * 
 * 当订阅的合约有新的Tick数据时调用此函数，执行以下操作：
 * 1. 检查是否订阅了该合约（通过_tick_subs查找），如果未订阅则直接返回
 * 2. 调用基类HftMocker::on_tick_updated()，执行基类的Tick更新逻辑
 * 3. 通过WtBtRunner通知外部语言有新的Tick数据（ctx_on_tick）
 * 
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针，包含最新价格、成交量、持仓量等信息
 */
void ExpHftMocker::on_tick_updated(const char* stdCode, WTSTickData* newTick)
{
	// 检查是否订阅了该合约的Tick数据
	// _tick_subs是基类HftMocker的成员变量，存储已订阅的合约集合
	auto it = _tick_subs.find(stdCode);
	if (it == _tick_subs.end())  // 如果未订阅该合约，直接返回，不处理
		return;

	// 调用基类的Tick更新逻辑，更新策略内部状态
	HftMocker::on_tick_updated(stdCode, newTick);
	
	// 通知外部语言有新的Tick数据
	// 参数：策略上下文ID、标准合约代码、新的Tick数据指针、ET_HFT表示HFT引擎类型
	getRunner().ctx_on_tick(_context_id, stdCode, newTick, ET_HFT);
}

/**
 * @brief 订单队列更新事件处理函数实现
 * 
 * 当订阅的合约有新的订单队列数据（Level2行情）时调用此函数，
 * 直接通过WtBtRunner通知外部语言有新的订单队列数据。
 * 
 * 注意：此函数不调用基类实现，因为基类可能没有此函数或实现不同，直接转发给外部语言。
 * 
 * @param stdCode 标准合约代码
 * @param newOrdQue 新的订单队列数据指针，包含买卖盘口信息（买一、买二、卖一、卖二等）
 */
void ExpHftMocker::on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	// 直接通知外部语言有新的订单队列数据（Level2行情）
	// 参数：策略上下文ID、标准合约代码、新的订单队列数据指针
	getRunner().hft_on_order_queue(_context_id, stdCode, newOrdQue);
}

/**
 * @brief 订单明细更新事件处理函数实现
 * 
 * 当订阅的合约有新的订单明细数据（Level2行情）时调用此函数，
 * 直接通过WtBtRunner通知外部语言有新的订单明细数据。
 * 
 * 注意：此函数不调用基类实现，因为基类可能没有此函数或实现不同，直接转发给外部语言。
 * 
 * @param stdCode 标准合约代码
 * @param newOrdDtl 新的订单明细数据指针，包含订单簿信息（每个价位的订单数量等）
 */
void ExpHftMocker::on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	// 直接通知外部语言有新的订单明细数据（Level2行情）
	// 参数：策略上下文ID、标准合约代码、新的订单明细数据指针
	getRunner().hft_on_order_detail(_context_id, stdCode, newOrdDtl);
}

/**
 * @brief 逐笔成交更新事件处理函数实现
 * 
 * 当订阅的合约有新的逐笔成交数据（Level2行情）时调用此函数，
 * 直接通过WtBtRunner通知外部语言有新的逐笔成交数据。
 * 
 * 注意：此函数不调用基类实现，因为基类可能没有此函数或实现不同，直接转发给外部语言。
 * 
 * @param stdCode 标准合约代码
 * @param newTrans 新的逐笔成交数据指针，包含每笔成交的详细信息（价格、数量、方向等）
 */
void ExpHftMocker::on_trans_updated(const char* stdCode, WTSTransData* newTrans)
{
	// 直接通知外部语言有新的逐笔成交数据（Level2行情）
	// 参数：策略上下文ID、标准合约代码、新的逐笔成交数据指针
	getRunner().hft_on_transaction(_context_id, stdCode, newTrans);
}

/**
 * @brief 成交回报事件处理函数实现
 * 
 * 当HFT策略的订单有成交回报时调用此函数，执行以下操作：
 * 1. 调用基类HftMocker::on_trade()，执行基类的成交回报逻辑
 * 2. 通过WtBtRunner通知外部语言成交回报信息（hft_on_trade）
 * 
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy true表示买入成交，false表示卖出成交
 * @param vol 成交数量
 * @param price 成交价格
 * @param userTag 用户标签
 */
void ExpHftMocker::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag)
{
	// 调用基类的成交回报逻辑，更新持仓和资金
	HftMocker::on_trade(localid, stdCode, isBuy, vol, price, userTag);

	// 通知外部语言成交回报信息
	// 参数：策略上下文ID、本地订单ID、标准合约代码、是否买入、成交数量、成交价格、用户标签
	getRunner().hft_on_trade(_context_id, localid, stdCode, isBuy, vol, price, userTag);
}

/**
 * @brief 回测结束事件处理函数实现
 * 
 * 当回测完成时调用此函数，通过WtBtRunner通知外部语言回测已结束。
 * 
 * 注意：此函数不调用基类实现，因为基类可能没有此函数或实现不同。
 */
void ExpHftMocker::on_bactest_end()
{
	// 触发回测结束事件，通知外部语言回测已完成
	getRunner().on_backtest_end();
}