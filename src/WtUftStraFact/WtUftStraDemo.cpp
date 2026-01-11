/*!
 * \file WtUftStraDemo.cpp
 * \project	WonderTrader
 * 
 * \brief SimpleUft简单极速交易策略示例类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WonderTrader框架中SimpleUft简单极速交易策略的所有功能。
 * 主要功能包括：
 * 1. 实现策略的初始化和参数配置加载
 * 2. 实现基于理论价格计算的交易信号生成逻辑
 * 3. 实现订单管理和超时撤销机制
 * 4. 处理交易通道状态变化，确保交易安全
 * 5. 提供完整的交易回报处理，包括成交、持仓、订单状态等
 * 6. 支持参数动态更新和监控
 * 
 * 策略算法说明：
 * SimpleUft策略通过计算理论价格（基于买卖盘口的加权平均）与最新价的比较来生成交易信号：
 * 1. 获取最新的Tick数据，包含买卖盘口信息
 * 2. 计算理论价格：理论价格 = (买一价*卖一量 + 卖一价*买一量) / (买一量 + 卖一量)
 * 3. 比较理论价格与最新价：
 *    - 如果理论价格 > 最新价，产生正向信号（做多）
 *    - 如果理论价格 < 最新价，产生反向信号（做空或平多）
 * 4. 根据信号和当前持仓情况执行交易操作
 * 5. 订单超时未成交自动撤销，避免订单滞留
 */
#include "WtUftStraDemo.h"  // 包含SimpleUft策略类头文件
#include "../Includes/IUftStraCtx.h"  // 包含UFT策略上下文接口，提供数据访问和交易执行接口

#include "../Includes/WTSVariant.hpp"  // 包含变体类型类，用于配置参数的读取
#include "../Includes/WTSDataDef.hpp"  // 包含WTS数据结构定义，提供K线、Tick等数据结构
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息类，提供合约相关信息的访问
#include "../Share/TimeUtils.hpp"  // 包含时间工具类，用于时间戳的生成和转换
#include "../Share/decimal.h"  // 包含高精度小数运算工具，用于浮点数比较和运算
#include "../Share/fmtlib.h"  // 包含格式化字符串库，用于格式化日志输出

extern const char* FACT_NAME;  // 外部声明工厂名称常量，定义在WtUftStraFact.cpp中

/**
 * @brief 构造函数实现
 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
 * 
 * 初始化SimpleUft策略对象，调用基类构造函数设置策略ID，并初始化所有成员变量。
 * 成员变量初始化说明：
 * - _last_tick: 初始化为NULL，表示没有保存的Tick数据
 * - _last_entry_time: 初始化为UINT64_MAX，表示从未交易过
 * - _channel_ready: 初始化为false，表示交易通道未就绪
 * - _last_calc_time: 初始化为0，表示从未计算过
 * - _lots: 初始化为1，默认下单数量为1手
 * - _cancel_cnt: 初始化为0，撤销订单计数器为0
 */
WtUftStraDemo::WtUftStraDemo(const char* id)  // 构造函数实现
	: UftStrategy(id)  // 调用基类构造函数，传入策略ID
	, _last_tick(NULL)  // 初始化最后Tick数据指针为空
	, _last_entry_time(UINT64_MAX)  // 初始化最后交易时间为最大值，表示从未交易过
	, _channel_ready(false)  // 初始化交易通道就绪标志为false，表示通道未就绪
	, _last_calc_time(0)  // 初始化最后计算时间为0，表示从未计算过
	, _lots(1)  // 初始化下单数量为1手
	, _cancel_cnt(0)  // 初始化撤销订单计数器为0
{
	// 构造函数体为空，所有初始化在初始化列表中完成
}


/**
 * @brief 析构函数实现
 * 
 * 析构函数在对象销毁时自动调用，释放策略对象占用的资源。
 * 主要释放_last_tick指针指向的Tick数据对象，避免内存泄漏。
 */
WtUftStraDemo::~WtUftStraDemo()  // 析构函数实现
{
	if (_last_tick)  // 检查最后Tick数据指针是否不为空
		_last_tick->release();  // 释放Tick数据对象，调用release方法释放资源
}

/**
 * @brief 获取策略名称的实现
 * @return const char* 返回策略的名称字符串
 * 
 * 该函数返回策略的名称，用于标识策略类型。
 * 返回值为"UftDemoStrategy"，表示这是UFT策略示例。
 */
const char* WtUftStraDemo::getName()  // 获取策略名称函数实现
{
	return "UftDemoStrategy";  // 返回策略名称字符串
}

/**
 * @brief 获取所属策略工厂名称的实现
 * @return const char* 返回策略所属的工厂名称字符串
 * 
 * 该函数返回策略所属的策略工厂名称，用于标识策略的来源工厂。
 * 返回值为"WtUftStraFact"，表示该策略由WtUftStraFact工厂创建。
 * 
 * @note FACT_NAME常量定义在WtUftStraFact.cpp中，值为"WtUftStraFact"
 */
const char* WtUftStraDemo::getFactName()  // 获取工厂名称函数实现
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 策略初始化实现
 * @param cfg 策略配置参数，包含策略运行所需的所有参数
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 该函数从配置参数中加载策略运行所需的参数，包括：
 * - code: 合约代码，策略交易的合约（必需参数）
 * - second: 订单超时时间（秒），超过此时间未成交的订单会被撤销（必需参数）
 * - freq: 交易频率限制（毫秒），两次交易之间的最小时间间隔（必需参数）
 * - offset: 价格偏移跳数，下单价格相对于最新价的偏移（必需参数）
 * - lots: 下单数量，每次交易的手数（必需参数）
 * 
 * @note 如果配置参数为空或缺少必要参数，可能导致运行时错误
 */
bool WtUftStraDemo::init(WTSVariant* cfg)  // 策略初始化函数实现
{
	//这里演示一下外部传入参数的获取
	_code = cfg->getCString("code");  // 从配置中读取合约代码字符串
	_secs = cfg->getUInt32("second");  // 从配置中读取订单超时时间（秒），转换为32位无符号整数
	_freq = cfg->getUInt32("freq");  // 从配置中读取交易频率限制（毫秒），转换为32位无符号整数
	_offset = cfg->getUInt32("offset");  // 从配置中读取价格偏移跳数，转换为32位无符号整数

	_lots = cfg->getDouble("lots");  // 从配置中读取下单数量，转换为双精度浮点数

	return true;  // 返回true表示初始化成功
}

/**
 * @brief 委托回报回调实现
 * @param localid 本地订单ID，用于标识订单
 * @param bSuccess 委托是否成功，true表示成功，false表示失败
 * @param message 委托结果消息
 * 
 * 该函数在委托回报时被调用，用于处理委托结果。
 * 主要功能：
 * 1. 如果委托失败，从订单集合中移除订单ID
 * 2. 避免管理失败的订单，防止后续处理出错
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_entrust(uint32_t localid, bool bSuccess, const char* message)  // 委托回报回调函数实现
{
	if(!bSuccess)  // 检查委托是否失败
	{
		auto it = _orders.find(localid);  // 在订单集合中查找订单ID
		if(it != _orders.end())  // 检查订单是否存在于订单集合中
			_orders.erase(it);  // 如果存在，从订单集合中移除订单ID，避免管理失败的订单
	}
}

/**
 * @brief 策略初始化完成回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * 
 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
 * 主要功能包括：
 * 1. 注册参数监控，支持参数动态更新
 * 2. 预加载历史K线数据，确保数据可用
 * 3. 订阅合约的Tick数据，用于接收实时行情
 * 4. 保存策略上下文指针，供后续使用
 * 
 * 参数监控说明：
 * UFT策略支持参数动态更新，通过watch_param注册需要监控的参数。
 * 当参数值发生变化时，会触发on_params_updated回调。
 * 
 * @note 该函数重写了UftStrategy基类的纯虚函数
 */
void WtUftStraDemo::on_init(IUftStraCtx* ctx)  // 初始化完成回调函数实现
{
	ctx->watch_param("second", _secs);  // 注册参数监控，监控"second"参数的变化，初始值为_secs
	ctx->watch_param("freq", _freq);  // 注册参数监控，监控"freq"参数的变化，初始值为_freq
	ctx->watch_param("offset", _offset);  // 注册参数监控，监控"offset"参数的变化，初始值为_offset
	ctx->watch_param("lots", _lots);  // 注册参数监控，监控"lots"参数的变化，初始值为_lots
	ctx->commit_param_watcher();  // 提交参数监控注册，完成参数监控的设置

	WTSKlineSlice* kline = ctx->stra_get_bars(_code.c_str(), "m1", 30);  // 预加载历史K线数据，获取1分钟K线数据，数量为30根
	if (kline)  // 检查K线数据是否获取成功
		kline->release();  // 释放K线数据切片，避免内存泄漏（预加载只是为了验证数据可用性）

	ctx->stra_sub_ticks(_code.c_str());  // 订阅合约的Tick数据，确保能接收到实时行情数据

	_ctx = ctx;  // 保存策略上下文指针，供后续使用
}

/**
 * @brief Tick数据处理回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * @param code 标准合约代码，触发Tick数据的合约
 * @param newTick 新的Tick数据，包含最新的价格和成交量信息
 * 
 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
 * 主要逻辑：
 * 1. 检查Tick数据是否属于策略关注的合约
 * 2. 如果有未完成的订单，先检查订单状态（超时撤销）
 * 3. 如果交易通道就绪且没有未完成订单，执行策略计算
 * 4. 计算理论价格（基于买卖盘口的加权平均）
 * 5. 根据理论价格与最新价的比较生成交易信号
 * 6. 执行交易操作
 * 
 * 算法步骤：
 * 1. 检查Tick数据的合约代码是否匹配
 * 2. 检查是否有未完成订单，如果有则先处理订单
 * 3. 检查交易通道是否就绪
 * 4. 计算当前时间戳（微秒）
 * 5. 检查交易频率限制
 * 6. 计算理论价格：理论价格 = (买一价*卖一量 + 卖一价*买一量) / (买一量 + 卖一量)
 * 7. 比较理论价格与最新价，生成交易信号
 * 8. 根据信号和当前持仓情况执行交易操作
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_tick(IUftStraCtx* ctx, const char* code, WTSTickData* newTick)  // Tick数据处理回调函数实现
{	
	if (_code.compare(code) != 0)  // 检查Tick数据的合约代码是否与策略关注的合约代码一致
		return;  // 如果不一致，直接返回，不处理该Tick数据

	if (!_orders.empty())  // 检查是否有未完成的订单
	{
		check_orders();  // 检查订单状态，如果订单超时则撤销
		return;  // 如果有未完成订单，先处理订单，不执行策略计算
	}

	if (!_channel_ready)  // 检查交易通道是否就绪
		return;  // 如果通道未就绪，直接返回，不执行交易

	WTSTickData* curTick = ctx->stra_get_last_tick(code);  // 获取最新的Tick数据（虽然获取了但未使用，可能是预留用于将来扩展）
	if (curTick)  // 检查Tick数据是否获取成功
		curTick->release();  // 释放Tick数据对象，避免内存泄漏

	uint32_t curMin = newTick->actiontime() / 100000;	//actiontime是带毫秒的,要取得分钟,则需要除以10w  // 计算当前分钟数，actiontime是带毫秒的时间戳，除以100000得到分钟数
	if (curMin > _last_calc_time)  // 检查当前分钟是否大于上次计算时间（如果spread上次计算的时候小于当前分钟,则重算spread）
	{//如果spread上次计算的时候小于当前分钟,则重算spread
		//WTSKlineSlice* kline = ctx->stra_get_bars(code, "m5", 30);  // 注释掉的代码：获取5分钟K线数据（当前未使用）
		//if (kline)  // 注释掉的代码：检查K线数据是否获取成功
		//	kline->release();  // 注释掉的代码：释放K线数据切片

		//重算晚了以后,更新计算时间
		_last_calc_time = curMin;  // 更新最后计算时间为当前分钟数，用于控制计算频率
	}

	//30秒内不重复计算
	uint64_t now = TimeUtils::makeTime(ctx->stra_get_date(), ctx->stra_get_time() * 100000 + ctx->stra_get_secs());  // 计算当前时间戳（微秒），通过日期、时间和秒数组合生成
	if(now - _last_entry_time <= _freq * 1000)  // 检查距离上次交易的时间是否小于频率限制（毫秒转微秒：_freq * 1000）
	{
		return;  // 如果时间间隔太短，直接返回，避免过度交易
	}

	int32_t signal = 0;  // 初始化交易信号为0，0表示无信号，1表示正向信号（做多），-1表示反向信号（做空或平多）
	double price = newTick->price();  // 获取最新价，作为当前价格
	//计算部分
	double pxInThry = (newTick->bidprice(0)*newTick->askqty(0) + newTick->askprice(0)*newTick->bidqty(0)) / (newTick->bidqty(0) + newTick->askqty(0));  // 计算理论价格：理论价格 = (买一价*卖一量 + 卖一价*买一量) / (买一量 + 卖一量)，这是基于买卖盘口的加权平均价格

	//理论价格大于最新价
	if (pxInThry > price)  // 检查理论价格是否大于最新价
	{
		//正向信号
		signal = 1;  // 设置信号为1，表示正向信号（做多）
	}
	else if (pxInThry < price)  // 检查理论价格是否小于最新价
	{
		//反向信号
		signal = -1;  // 设置信号为-1，表示反向信号（做空或平多）
	}

	if (signal != 0)  // 检查是否有交易信号（信号不为0）
	{
		double curPos = ctx->stra_get_position(code);  // 获取当前持仓量

		WTSCommodityInfo* cInfo = ctx->stra_get_comminfo(code);  // 获取合约信息，用于获取价格最小变动单位

		if(signal > 0  && decimal::le(curPos, 0))  // 检查是否为正向信号且当前持仓小于等于0（正向信号,且当前仓位小于等于0）
		{//正向信号,且当前仓位小于等于0
			//最新价+2跳下单
			double targetPx = price + cInfo->getPriceTick() * _offset;  // 计算目标价格：最新价 + 价格最小变动单位 * 偏移跳数（最新价+2跳下单）

			auto ids = ctx->stra_buy(code, targetPx, _lots, UFT_OrderFlag_Nor);  // 买入下单，返回订单ID列表，UFT_OrderFlag_Nor表示普通订单标志

			{
				_mtx_ords.lock();  // 锁定订单集合自旋锁，保护订单集合的访问
				for (uint32_t localid : ids)  // 遍历订单ID列表
					_orders.insert(localid);  // 将订单ID插入订单集合，用于跟踪订单状态
				_mtx_ords.unlock();  // 解锁订单集合自旋锁
				_last_entry_time = now;  // 更新最后交易时间为当前时间，用于计算交易频率限制
			}
			
		}
		else if (signal < 0 && decimal::ge(curPos, 0))  // 检查是否为反向信号且当前持仓大于等于0（反向信号,且当前仓位大于等于0）
		{//反向信号,且当前仓位大于等于0,或者仓位为0但不是股票,或者仓位为0但是基础仓位有修正
			//最新价-2跳下单
			double targetPx = price - cInfo->getPriceTick()*_offset;  // 计算目标价格：最新价 - 价格最小变动单位 * 偏移跳数（最新价-2跳下单）

			auto ids = ctx->stra_sell(code, targetPx, _lots, UFT_OrderFlag_Nor);  // 卖出下单，返回订单ID列表，UFT_OrderFlag_Nor表示普通订单标志

			{
				_mtx_ords.lock();  // 锁定订单集合自旋锁，保护订单集合的访问
				for (uint32_t localid : ids)  // 遍历订单ID列表
					_orders.insert(localid);  // 将订单ID插入订单集合，用于跟踪订单状态
				_mtx_ords.unlock();  // 解锁订单集合自旋锁
				_last_entry_time = now;  // 更新最后交易时间为当前时间，用于计算交易频率限制
			}
		}
	}
}

/**
 * @brief 检查订单状态的实现
 * 
 * 该函数检查当前未完成的订单，如果订单超时未成交则自动撤销。
 * 订单超时时间由_secs参数控制，从_last_entry_time开始计算。
 * 
 * 算法步骤：
 * 1. 检查是否有未完成订单且最后交易时间有效
 * 2. 计算当前时间戳
 * 3. 检查距离最后交易时间是否超过超时时间
 * 4. 如果超时，撤销所有未完成订单并更新撤销计数器
 * 
 * @note 该函数是私有函数，仅在策略内部调用
 */
void WtUftStraDemo::check_orders()  // 检查订单状态函数实现
{
	if (!_orders.empty() && _last_entry_time != UINT64_MAX)  // 检查是否有未完成订单且最后交易时间有效（不为UINT64_MAX）
	{
		uint64_t now = TimeUtils::makeTime(_ctx->stra_get_date(), _ctx->stra_get_time() * 100000 + _ctx->stra_get_secs());  // 计算当前时间戳（微秒）
		if (now - _last_entry_time >= _secs * 1000)	//如果超过一定时间没有成交完,则撤销  // 检查距离最后交易时间是否超过超时时间（秒转微秒：_secs * 1000）
		{
			_mtx_ords.lock();  // 锁定订单集合自旋锁，保护订单集合的访问
			for (auto localid : _orders)  // 遍历所有未完成的订单ID
			{
				_ctx->stra_cancel(localid);  // 撤销订单，调用撤销接口
				_cancel_cnt++;  // 增加撤销订单计数器
				_ctx->stra_log_info(fmt::format("Order expired, cancelcnt updated to {}", _cancel_cnt).c_str());  // 记录日志，格式化输出撤销订单信息
			}
			_mtx_ords.unlock();  // 解锁订单集合自旋锁
		}
	}
}

/**
 * @brief K线闭合回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * @param code 标准合约代码，触发K线数据的合约
 * @param period K线周期，如"m1"、"m5"等
 * @param times K线倍数
 * @param newBar 新的K线数据
 * 
 * 该函数在K线闭合时被调用，用于处理K线数据。
 * SimpleUft策略主要基于Tick数据，因此K线数据处理为空实现。
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_bar(IUftStraCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar)  // K线闭合回调函数实现
{
	// K线数据处理为空实现，SimpleUft策略主要基于Tick数据
}

/**
 * @brief 成交回报回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * @param localid 本地订单ID，用于标识订单
 * @param stdCode 标准合约代码
 * @param isLong 是否为做多，true表示做多，false表示做空
 * @param offset 开平标志，0-开仓，1-平仓，2-平今
 * @param qty 成交数量
 * @param price 成交价格
 * 
 * 该函数在订单成交时被调用，用于处理成交回报。
 * SimpleUft策略当前实现为空，可以根据需要扩展处理逻辑。
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_trade(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double qty, double price)  // 成交回报回调函数实现
{
	// 成交回报处理为空实现，可以根据需要扩展
}

/**
 * @brief 持仓回报回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * @param stdCode 标准合约代码
 * @param isLong 是否为多头，true表示多头，false表示空头
 * @param prevol 变化前的持仓量（昨仓）
 * @param preavail 变化前的可用持仓量（可用昨仓）
 * @param newvol 变化后的持仓量（今仓）
 * @param newavail 变化后的可用持仓量（可用今仓）
 * 
 * 该函数在持仓发生变化时被调用，用于处理持仓回报。
 * 主要功能：
 * 1. 检查持仓回报的合约代码是否匹配
 * 2. 记录昨仓信息，用于后续使用
 * 3. 记录日志，输出持仓变化信息
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_position(IUftStraCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail)  // 持仓回报回调函数实现
{
	if (_code != stdCode)  // 检查持仓回报的合约代码是否与策略关注的合约代码一致
		return;  // 如果不一致，直接返回，不处理该持仓回报

	_prev = prevol;  // 记录昨仓数量，用于后续使用
	_ctx->stra_log_info(fmt::format("There are {} of {} before today", _prev, stdCode).c_str());  // 记录日志，格式化输出昨仓信息
}

/**
 * @brief 订单回报回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * @param localid 本地订单ID，用于标识订单
 * @param stdCode 标准合约代码
 * @param isLong 是否为做多，true表示做多，false表示做空
 * @param offset 开平标志，0-开仓，1-平仓，2-平今
 * @param totalQty 订单总数量
 * @param leftQty 订单剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤销，true表示已撤销，false表示未撤销
 * 
 * 该函数在订单状态发生变化时被调用，用于处理订单回报。
 * 主要功能：
 * 1. 检查订单是否属于策略管理的订单
 * 2. 如果订单已撤销或全部成交，从订单集合中移除
 * 3. 更新撤销订单计数器
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_order(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled)  // 订单回报回调函数实现
{
	//如果不是我发出去的订单,我就不管了
	auto it = _orders.find(localid);  // 在订单集合中查找订单ID
	if (it == _orders.end())  // 检查订单是否属于策略管理的订单
		return;  // 如果不属于策略管理的订单，直接返回，不处理

	//如果已撤销或者剩余数量为0,则清除掉原有的id记录
	if(isCanceled || leftQty == 0)  // 检查订单是否已撤销或全部成交（剩余数量为0）
	{
		_mtx_ords.lock();  // 锁定订单集合自旋锁，保护订单集合的访问
		_orders.erase(it);  // 从订单集合中移除订单ID
		if (_cancel_cnt > 0)  // 检查撤销订单计数器是否大于0
		{
			_cancel_cnt--;  // 减少撤销订单计数器（因为订单已撤销或成交，不再需要撤销）
			_ctx->stra_log_info(fmt::format("cancelcnt -> {}", _cancel_cnt).c_str());  // 记录日志，格式化输出撤销订单计数器
		}
		_mtx_ords.unlock();  // 解锁订单集合自旋锁
	}
}


/**
 * @brief 交易通道就绪回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * 
 * 该函数在交易通道就绪时被调用，表示可以开始交易。
 * 主要功能：
 * 1. 检查是否有不在管理中的未完成订单
 * 2. 如果有，撤销这些订单并加入管理
 * 3. 更新撤销订单计数器
 * 4. 设置通道就绪标志，允许策略执行交易
 * 
 * 安全机制：
 * 当交易通道就绪时，检查是否有未完成订单不在策略的管理中。
 * 如果有，说明可能是策略重启前的遗留订单，需要先撤销这些订单，
 * 确保策略能够完全控制所有订单，避免订单管理混乱。
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_channel_ready(IUftStraCtx* ctx)  // 交易通道就绪回调函数实现
{
	double undone = _ctx->stra_get_undone(_code.c_str());  // 获取合约的未完成订单数量
	if (!decimal::eq(undone, 0) && _orders.empty())  // 检查是否有未完成订单且策略管理的订单集合为空（这说明有未完成单不在监控之中,先撤掉）
	{
		//这说明有未完成单不在监控之中,先撤掉
		_ctx->stra_log_info(fmt::format("{}有不在管理中的未完成单 {} 手,全部撤销", _code, undone).c_str());  // 记录日志，格式化输出未完成订单信息

		OrderIDs ids = _ctx->stra_cancel_all(_code.c_str());  // 撤销合约的所有未完成订单，返回撤销的订单ID列表
		for (auto localid : ids)  // 遍历撤销的订单ID列表
		{
			_orders.insert(localid);  // 将订单ID插入订单集合，加入管理（虽然这些订单已经被撤销，但需要跟踪撤销状态）
		}
		_cancel_cnt += ids.size();  // 增加撤销订单计数器，加上撤销的订单数量

		_ctx->stra_log_info(fmt::format("cancelcnt -> {}", _cancel_cnt).c_str());  // 记录日志，格式化输出撤销订单计数器
	}

	_channel_ready = true;  // 设置交易通道就绪标志为true，允许策略执行交易
}

/**
 * @brief 交易通道丢失回调实现
 * @param ctx UFT策略上下文对象，提供数据访问和交易执行接口
 * 
 * 该函数在交易通道丢失时被调用，表示无法进行交易。
 * 主要功能：
 * 1. 设置通道丢失标志，禁止策略执行交易
 * 2. 等待通道恢复后再继续交易
 * 
 * 安全机制：
 * 当交易通道丢失时，立即禁止策略执行交易操作，
 * 避免在通道不稳定时发送订单导致的问题。
 * 通道恢复后会触发on_channel_ready回调，重新允许交易。
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_channel_lost(IUftStraCtx* ctx)  // 交易通道丢失回调函数实现
{
	_channel_ready = false;  // 设置交易通道就绪标志为false，禁止策略执行交易
}

/**
 * @brief 参数更新回调实现
 * 
 * 该函数在策略参数发生变化时被调用，用于处理参数更新。
 * 主要功能：
 * 1. 从上下文读取更新后的参数值
 * 2. 更新策略内部的参数变量
 * 3. 记录日志，输出参数更新信息
 * 
 * UFT策略支持参数动态更新，无需重启策略即可生效。
 * 参数更新流程：
 * 1. 外部系统修改参数值
 * 2. 框架检测到参数变化
 * 3. 触发on_params_updated回调
 * 4. 策略从上下文读取新参数值并更新内部变量
 * 
 * @note 该函数重写了UftStrategy基类的虚函数
 */
void WtUftStraDemo::on_params_updated()  // 参数更新回调函数实现
{
	//ctx->watch_param("second", _secs);  // 注释掉的代码：注册参数监控（已在on_init中注册，这里不需要重复注册）
	//ctx->watch_param("freq", _freq);  // 注释掉的代码：注册参数监控（已在on_init中注册，这里不需要重复注册）
	//ctx->watch_param("offset", _offset);  // 注释掉的代码：注册参数监控（已在on_init中注册，这里不需要重复注册）
	//ctx->watch_param("lots", _lots);  // 注释掉的代码：注册参数监控（已在on_init中注册，这里不需要重复注册）

	_secs = _ctx->read_param("second", _secs);  // 从上下文读取"second"参数的新值，如果不存在则使用默认值_secs
	_freq = _ctx->read_param("freq", _freq);  // 从上下文读取"freq"参数的新值，如果不存在则使用默认值_freq
	_offset = _ctx->read_param("offset", _offset);  // 从上下文读取"offset"参数的新值，如果不存在则使用默认值_offset
	_lots = _ctx->read_param("lots", _lots);  // 从上下文读取"lots"参数的新值，如果不存在则使用默认值_lots

	_ctx->stra_log_info(fmtutil::format("[{}] Params updated, second: {}, freq: {}, offset: {}, lots: {}", _id.c_str(), _secs, _freq, _offset, _lots));  // 记录日志，格式化输出参数更新信息，包括策略ID和所有更新的参数值
}
