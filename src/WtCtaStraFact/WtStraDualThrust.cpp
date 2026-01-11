/*!
 * \file WtStraDualThrust.cpp
 * \project	WonderTrader
 * 
 * \brief DualThrust双突破策略类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WonderTrader框架中DualThrust双突破策略的所有功能。
 * 主要功能包括：
 * 1. 实现策略的初始化和参数配置加载
 * 2. 实现DualThrust策略的核心交易逻辑，包括上下轨计算和交易信号生成
 * 3. 处理期货主力合约的自动换月，确保持仓正确转移
 * 4. 实现策略生命周期回调，包括初始化、交易日开始、定时调度等
 * 5. 提供图表和指标支持，在图表上显示上下轨和交易标记
 * 
 * 策略算法说明：
 * DualThrust策略通过计算过去N天的价格波动范围来确定交易信号：
 * 1. 计算过去N天（排除最后1根K线）的最高价HH和最低价LL
 * 2. 计算过去N天（排除最后1根K线）的收盘价最高值HC和最低值LC
 * 3. 计算价格波动范围：Range = max(HH-LC, HC-LL)
 * 4. 计算上轨：上轨 = 当前开盘价 + K1 * Range
 * 5. 计算下轨：下轨 = 当前开盘价 - K2 * Range
 * 6. 根据当前价格与上下轨的关系生成交易信号
 */
#include "WtStraDualThrust.h"  // 包含DualThrust策略类头文件

#include "../Includes/ICtaStraCtx.h"  // 包含CTA策略上下文接口，提供数据访问和交易执行接口

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息类，提供合约相关信息的访问
#include "../Includes/WTSVariant.hpp"  // 包含变体类型类，用于配置参数的读取
#include "../Includes/WTSDataDef.hpp"  // 包含WTS数据结构定义，提供K线、Tick等数据结构
#include "../Share/decimal.h"  // 包含高精度小数运算工具，用于浮点数比较和运算

extern const char* FACT_NAME;  // 外部声明工厂名称常量，定义在WtCtaStraFact.cpp中

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"  // 包含格式化字符串库，用于格式化日志输出

/**
 * @brief 构造函数实现
 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
 * 
 * 初始化DualThrust策略对象，调用基类构造函数设置策略ID。
 * 构造函数体为空，成员变量使用默认值初始化。
 * 实际的参数配置在init函数中完成。
 */
WtStraDualThrust::WtStraDualThrust(const char* id)  // 构造函数实现
	: CtaStrategy(id)  // 调用基类构造函数，传入策略ID
{
	// 构造函数体为空，成员变量使用默认值初始化
}


/**
 * @brief 析构函数实现
 * 
 * 析构函数在对象销毁时自动调用，释放策略对象占用的资源。
 * 当前实现为空析构函数，不执行任何特殊操作。
 * 如果将来需要清理操作（如释放动态分配的内存），可以在此处添加。
 */
WtStraDualThrust::~WtStraDualThrust()  // 析构函数实现
{
	// 析构函数体为空，使用默认清理
}

/**
 * @brief 获取所属策略工厂名称的实现
 * @return const char* 返回策略所属的工厂名称字符串
 * 
 * 该函数返回策略所属的策略工厂名称，用于标识策略的来源工厂。
 * 返回值为"WtCtaStraFact"，表示该策略由WtCtaStraFact工厂创建。
 * 
 * @note FACT_NAME常量定义在WtCtaStraFact.cpp中，值为"WtCtaStraFact"
 */
const char* WtStraDualThrust::getFactName()  // 获取工厂名称函数实现
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 获取策略名称的实现
 * @return const char* 返回策略的名称字符串
 * 
 * 该函数返回策略的名称，用于标识策略类型。
 * 返回值为"DualThrust"，表示这是DualThrust双突破策略。
 */
const char* WtStraDualThrust::getName()  // 获取策略名称函数实现
{
	return "DualThrust";  // 返回策略名称字符串
}

/**
 * @brief 策略初始化实现
 * @param cfg 策略配置参数，包含策略运行所需的所有参数
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 该函数从配置参数中加载策略运行所需的参数，包括：
 * - days: 回看天数，用于计算价格波动范围（必需参数）
 * - k1: 上轨系数，用于计算上突破轨道（必需参数）
 * - k2: 下轨系数，用于计算下突破轨道（必需参数）
 * - period: K线周期，如"m1"、"m5"、"d1"等（必需参数）
 * - count: K线条数，用于获取历史K线数据（必需参数）
 * - code: 合约代码，策略交易的合约（必需参数）
 * - stock: 是否为股票，true表示股票模式（不支持做空），false表示期货模式（可选参数，默认为false）
 * 
 * @note 如果配置参数为空，函数返回false
 * @note 如果配置参数中缺少必要参数，可能导致运行时错误
 */
bool WtStraDualThrust::init(WTSVariant* cfg)  // 策略初始化函数实现
{
	if (cfg == NULL)  // 检查配置参数是否为空
		return false;  // 如果为空，返回false表示初始化失败

	_days = cfg->getUInt32("days");  // 从配置中读取回看天数，转换为32位无符号整数
	_k1 = cfg->getDouble("k1");  // 从配置中读取上轨系数，转换为双精度浮点数
	_k2 = cfg->getDouble("k2");  // 从配置中读取下轨系数，转换为双精度浮点数

	_period = cfg->getCString("period");  // 从配置中读取K线周期字符串，如"m1"、"m5"、"d1"等
	_count = cfg->getUInt32("count");  // 从配置中读取K线条数，转换为32位无符号整数
	_code = cfg->getCString("code");  // 从配置中读取合约代码字符串

	_isstk = cfg->getBoolean("stock");  // 从配置中读取是否为股票标志，转换为布尔值（如果不存在则默认为false）

	return true;  // 返回true表示初始化成功
}

/**
 * @brief 交易日开始回调实现
 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
 * @param uTDate 交易日日期，格式为YYYYMMDD
 * 
 * 该函数在每个交易日开始时被调用，用于执行交易日开始时的准备工作。
 * 主要功能是处理期货主力合约的自动换月：
 * 1. 获取当前交易日的主力合约代码
 * 2. 如果主力合约发生变化，且旧主力有持仓
 * 3. 将旧主力的持仓平仓，在新主力上开相同方向的持仓
 * 
 * 换月逻辑说明：
 * - 通过stra_get_rawcode获取当前交易日的主力合约代码
 * - 如果主力合约代码发生变化，说明发生了换月
 * - 如果旧主力合约有持仓（持仓不为0），需要转移持仓
 * - 将旧主力的持仓设置为0，在新主力上设置相同的持仓量
 * - 更新_moncode为新的主力合约代码
 * 
 * @note 换月操作不会改变持仓方向，只是将持仓从旧主力转移到新主力
 */
void WtStraDualThrust::on_session_begin(ICtaStraCtx* ctx, uint32_t uTDate)  // 交易日开始回调函数实现
{
	std::string newMonCode = ctx->stra_get_rawcode(_code.c_str());  // 获取当前交易日的主力合约代码（标准合约代码）
	if(newMonCode!=_moncode)  // 检查主力合约代码是否发生变化
	{
		if(!_moncode.empty())  // 检查旧主力合约代码是否为空（首次运行时_moncode为空）
		{
			double curPos = ctx->stra_get_position(_moncode.c_str());  // 获取旧主力合约的当前持仓量
			if (!decimal::eq(curPos, 0))  // 检查持仓量是否不为0（使用高精度小数比较，避免浮点数误差）
			{
				ctx->stra_log_info(fmt::format("主力换月,  老主力{}[{}]将会被清理", _moncode, curPos).c_str());  // 记录换月日志，格式化输出旧主力合约代码和持仓量
				ctx->stra_set_position(_moncode.c_str(), 0, "switchout");  // 将旧主力合约的持仓设置为0，标记为"switchout"（换出）
				ctx->stra_set_position(newMonCode.c_str(), curPos, "switchin");  // 在新主力合约上设置相同的持仓量，标记为"switchin"（换入）
			}
		}

		_moncode = newMonCode;  // 更新当前主力合约代码为新的主力合约代码
	}
}

/**
 * @brief 策略调度执行入口实现
 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
 * @param curDate 当前日期，格式为YYYYMMDD
 * @param curTime 当前时间，格式为HHMMSS
 * 
 * 该函数是策略的主体逻辑执行入口，在策略调度时被调用。
 * 主要功能包括：
 * 1. 获取历史K线数据，计算价格波动范围
 * 2. 计算上下突破轨道（上轨和下轨）
 * 3. 根据当前价格与上下轨的关系生成交易信号
 * 4. 执行交易操作（开多、开空、平多、平空）
 * 5. 在图表上显示上下轨和交易标记
 * 
 * 交易逻辑：
 * - 无持仓时：价格突破上轨做多，突破下轨做空（仅期货）
 * - 持多仓时：价格跌破下轨平多
 * - 持空仓时：价格突破上轨平空（仅期货）
 * 
 * 算法步骤：
 * 1. 获取历史K线数据（数量为_count条）
 * 2. 计算过去_days天（排除最后1根K线）的最高价HH和最低价LL
 * 3. 计算过去_days天（排除最后1根K线）的收盘价最高值HC和最低值LC
 * 4. 计算价格波动范围：Range = max(HH-LC, HC-LL)
 * 5. 获取当前K线的开盘价
 * 6. 计算上轨：上轨 = 开盘价 + K1 * Range
 * 7. 计算下轨：下轨 = 开盘价 - K2 * Range
 * 8. 获取当前价格（最后一根K线的收盘价）
 * 9. 根据当前价格与上下轨的关系生成交易信号
 */
void WtStraDualThrust::on_schedule(ICtaStraCtx* ctx, uint32_t curDate, uint32_t curTime)  // 策略调度执行函数实现
{
	std::string code = _code;  // 复制合约代码字符串，用于后续使用

	WTSKlineSlice *kline = ctx->stra_get_bars(code.c_str(), _period.c_str(), _count, true);  // 获取历史K线数据切片，参数：合约代码、周期、条数、是否包含未闭合K线
	if(kline == NULL)  // 检查K线数据是否获取成功
	{
		//这里可以输出一些日志
		return;  // 如果获取失败，直接返回，不执行后续逻辑
	}

	if (kline->size() == 0)  // 检查K线数据是否为空
	{
		kline->release();  // 释放K线数据切片，避免内存泄漏
		return;  // 如果数据为空，直接返回，不执行后续逻辑
	}

	uint32_t trdUnit = 1;  // 初始化交易单位为1（期货默认交易单位为1手）
	if (_isstk)  // 检查是否为股票模式
		trdUnit = 100;  // 如果是股票，交易单位为100股（A股最小交易单位为100股）


	int32_t days = (int32_t)_days;  // 将回看天数转换为32位有符号整数，用于K线数据索引（负数表示从后往前数）

	double hh = kline->maxprice(-days, -2);  // 计算过去_days天（排除最后1根K线，即索引从-days到-2）的最高价
	double ll = kline->minprice(-days, -2);  // 计算过去_days天（排除最后1根K线，即索引从-days到-2）的最低价

	WTSValueArray* closes = kline->extractData(KFT_CLOSE);  // 提取K线数据中的收盘价数组，KFT_CLOSE表示收盘价字段
	double hc = closes->maxvalue(-days, -2);  // 计算过去_days天（排除最后1根K线）的收盘价最高值
	double lc = closes->minvalue(-days, -2);  // 计算过去_days天（排除最后1根K线）的收盘价最低值
	double curPx = closes->at(-1);  // 获取最后一根K线的收盘价（-1表示最后一个元素），作为当前价格
	closes->release();///!!!这个释放一定要做  // 释放收盘价数组，避免内存泄漏（重要：必须释放，否则会导致内存泄漏）

	double openPx = kline->at(-1)->open;  // 获取最后一根K线的开盘价，用于计算上下轨

	double upper_bound = openPx + _k1 * (std::max(hh - lc, hc - ll));  // 计算上轨：上轨 = 开盘价 + K1 * max(HH-LC, HC-LL)，其中max(HH-LC, HC-LL)是价格波动范围
	double lower_bound = openPx - _k2 * std::max(hh - lc, hc - ll);  // 计算下轨：下轨 = 开盘价 - K2 * max(HH-LC, HC-LL)

	//设置指标值
	ctx->set_index_value("DualThrust", "upper_bound", upper_bound);  // 设置指标值，用于在图表上显示上轨
	ctx->set_index_value("DualThrust", "lower_bound", lower_bound);  // 设置指标值，用于在图表上显示下轨

	WTSCommodityInfo* commInfo = ctx->stra_get_comminfo(_code.c_str());  // 获取合约信息（虽然获取了但未使用，可能是预留用于将来扩展）

	double curPos = ctx->stra_get_position(_moncode.c_str()) / trdUnit;  // 获取当前主力合约的持仓量，并转换为交易单位（手数或股数）
	if(decimal::eq(curPos,0))  // 检查当前持仓是否为0（使用高精度小数比较，避免浮点数误差）
	{
		if(curPx >= upper_bound)  // 检查当前价格是否大于等于上轨（向上突破）
		{
			ctx->stra_enter_long(_moncode.c_str(), 2 * trdUnit, "DT_EnterLong");  // 开多仓，数量为2个交易单位，标记为"DT_EnterLong"
			//向上突破
			ctx->stra_log_info(fmt::format("向上突破{}>={},多仓进场", curPx, upper_bound).c_str());  // 记录日志，格式化输出当前价格和上轨值

			//添加图表标记
			ctx->add_chart_mark(curPx, "wt-mark-buy", "DT_EnterLong");  // 在图表上添加买入标记，标记类型为"wt-mark-buy"，标签为"DT_EnterLong"
		}
		else if (curPx <= lower_bound && !_isstk)  // 检查当前价格是否小于等于下轨（向下突破），且不是股票模式（股票不支持做空）
		{
			ctx->stra_enter_short(_moncode.c_str(), 2 * trdUnit, "DT_EnterShort");  // 开空仓，数量为2个交易单位，标记为"DT_EnterShort"
			//向下突破
			ctx->stra_log_info(fmt::format("向下突破{}<={},空仓进场", curPx, lower_bound).c_str());  // 记录日志，格式化输出当前价格和下轨值

			//添加图表标记
			ctx->add_chart_mark(curPx, "wt-mark-sell", "DT_EnterShort");  // 在图表上添加卖出标记，标记类型为"wt-mark-sell"，标签为"DT_EnterShort"
		}
	}
	//else if(curPos > 0)
	else if (decimal::gt(curPos, 0))  // 检查当前持仓是否大于0（持多仓，使用高精度小数比较）
	{
		if(curPx <= lower_bound)  // 检查当前价格是否小于等于下轨（向下突破）
		{
			//多仓出场
			ctx->stra_exit_long(_moncode.c_str(), 2 * trdUnit, "DT_ExitLong");  // 平多仓，数量为2个交易单位，标记为"DT_ExitLong"
			ctx->stra_log_info(fmt::format("向下突破{}<={},多仓出场", curPx, lower_bound).c_str());  // 记录日志，格式化输出当前价格和下轨值

			//添加图表标记
			ctx->add_chart_mark(curPx, "wt-mark-sell", "DT_ExitLong");  // 在图表上添加卖出标记，标记类型为"wt-mark-sell"，标签为"DT_ExitLong"
		}
	}
	//else if(curPos < 0)
	else if (decimal::lt(curPos, 0))  // 检查当前持仓是否小于0（持空仓，使用高精度小数比较）
	{
		if (curPx >= upper_bound && !_isstk)  // 检查当前价格是否大于等于上轨（向上突破），且不是股票模式（股票不支持做空）
		{
			//空仓出场
			ctx->stra_exit_short(_moncode.c_str(), 2 * trdUnit, "DT_ExitShort");  // 平空仓，数量为2个交易单位，标记为"DT_ExitShort"
			ctx->stra_log_info(fmt::format("向上突破{}>={},空仓出场", curPx, upper_bound).c_str());  // 记录日志，格式化输出当前价格和上轨值

			//添加图表标记
			ctx->add_chart_mark(curPx, "wt-mark-buy", "DT_ExitShort");  // 在图表上添加买入标记，标记类型为"wt-mark-buy"，标签为"DT_ExitShort"
		}
	}

	//这个释放一定要做
	kline->release();  // 释放K线数据切片，避免内存泄漏（重要：必须释放，否则会导致内存泄漏）
}

/**
 * @brief 策略初始化完成回调实现
 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
 * 
 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
 * 主要功能包括：
 * 1. 订阅合约的Tick数据，用于接收实时市场数据
 * 2. 预加载历史K线数据，确保数据可用
 * 3. 注册策略图表，用于显示K线和指标
 * 4. 注册指标和指标线，用于显示上下轨
 * 
 * 初始化步骤：
 * 1. 订阅合约的Tick数据，确保能接收到实时行情
 * 2. 预加载历史K线数据，验证数据是否可用
 * 3. 注册图表K线，用于在图表上显示K线数据
 * 4. 注册指标"DualThrust"，用于显示策略指标
 * 5. 注册指标线"upper_bound"和"lower_bound"，用于显示上下轨
 * 
 * @note 如果K线数据获取失败，函数会提前返回，但不会影响策略运行
 */
void WtStraDualThrust::on_init(ICtaStraCtx* ctx)  // 初始化完成回调函数实现
{
	std::string code = _code;  // 复制合约代码字符串，用于后续使用
	ctx->stra_sub_ticks(_code.c_str());  // 订阅合约的Tick数据，确保能接收到实时行情数据
	WTSKlineSlice *kline = ctx->stra_get_bars(code.c_str(), _period.c_str(), _count, true);  // 预加载历史K线数据，验证数据是否可用
	if (kline == NULL)  // 检查K线数据是否获取成功
	{
		//这里可以输出一些日志
		return;  // 如果获取失败，直接返回（但不影响策略运行，后续调度时会再次尝试获取）
	}

	kline->release();  // 释放K线数据切片，避免内存泄漏（预加载只是为了验证数据可用性）

	//注册指标和图表K线
	ctx->set_chart_kline(_code.c_str(), _period.c_str());  // 注册图表K线，用于在策略图表上显示K线数据，参数：合约代码、K线周期

	//注册指标
	ctx->register_index("DualThrust", 0);  // 注册指标"DualThrust"，用于显示策略指标，参数：指标名称、指标类型（0表示自定义指标）

	//注册指标线
	ctx->register_index_line("DualThrust", "upper_bound", 0);  // 注册指标线"upper_bound"，用于显示上轨，参数：指标名称、指标线名称、指标线类型（0表示线型）
	ctx->register_index_line("DualThrust", "lower_bound", 0);  // 注册指标线"lower_bound"，用于显示下轨，参数：指标名称、指标线名称、指标线类型（0表示线型）
}

/**
 * @brief Tick数据处理回调实现
 * @param ctx 策略上下文对象，提供数据访问和交易执行接口
 * @param stdCode 标准合约代码，触发Tick数据的合约
 * @param newTick 新的Tick数据，包含最新的价格和成交量信息
 * 
 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
 * DualThrust策略主要基于K线数据进行交易决策，因此Tick数据处理为空实现。
 * 
 * 如果将来需要基于Tick数据做更精细的交易决策（如：
 * - 基于Tick级别的价格变化进行更精确的入场/出场
 * - 基于Tick级别的成交量进行过滤
 * - 基于Tick级别的买卖盘口进行决策），可以在此函数中实现。
 * 
 * @note 该函数虽然为空实现，但必须保留，因为它是基类的虚函数
 */
void WtStraDualThrust::on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick)  // Tick数据处理回调函数实现
{
	//没有什么要处理
	// DualThrust策略主要基于K线数据进行交易决策，Tick数据处理为空实现
	// 如果将来需要基于Tick数据做更精细的交易决策，可以在此函数中实现
}
