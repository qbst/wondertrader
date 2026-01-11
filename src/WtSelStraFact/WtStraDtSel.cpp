/*!
 * \file WtStraDtSel.cpp
 * \project	WonderTrader
 * 
 * \brief DualThrust选股策略类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WonderTrader框架中DualThrust选股策略的所有功能。
 * 主要功能包括：
 * 1. 实现策略的初始化和参数配置加载
 * 2. 实现DualThrust选股的核心交易逻辑，包括多标的的上下轨计算和交易信号生成
 * 3. 支持多标的的独立仓位管理
 * 4. 实现策略生命周期回调，包括初始化、定时调度等
 * 
 * 策略算法说明：
 * DualThrust选股策略通过计算每个标的过去N天的价格波动范围来确定交易信号：
 * 1. 遍历所有标的，对每个标的分别执行选股逻辑
 * 2. 检查标的是否在交易时间内
 * 3. 获取历史K线数据（数量为_count条）
 * 4. 计算过去_days天（排除最后1根K线）的最高价HH和最低价LL
 * 5. 计算过去_days天（排除最后1根K线）的收盘价最高值HC和最低值LC
 * 6. 计算价格波动范围：Range = max(HH-LC, HC-LL)
 * 7. 获取当前K线的开盘价、最高价、最低价
 * 8. 计算上轨：上轨 = 开盘价 + K1 * Range
 * 9. 计算下轨：下轨 = 开盘价 - K2 * Range
 * 10. 根据当前价格与上下轨的关系生成交易信号
 * 11. 对每个标的独立管理持仓，执行交易操作
 */
#include "WtStraDtSel.h"  // 包含DualThrust选股策略类头文件

#include "../Includes/ISelStraCtx.h"  // 包含SEL策略上下文接口，提供数据访问和交易执行接口

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息类，提供合约相关信息的访问
#include "../Includes/WTSSessionInfo.hpp"  // 包含交易时段信息类，提供交易时间判断接口
#include "../Includes/WTSVariant.hpp"  // 包含变体类型类，用于配置参数的读取
#include "../Includes/WTSDataDef.hpp"  // 包含WTS数据结构定义，提供K线、Tick等数据结构
#include "../Share/decimal.h"  // 包含高精度小数运算工具，用于浮点数比较和运算
#include "../Share/StrUtil.hpp"  // 包含字符串工具类，用于字符串分割等操作
#include "../Share/fmtlib.h"  // 包含格式化字符串库，用于格式化日志输出

extern const char* FACT_NAME;  // 外部声明工厂名称常量，定义在WtSelStraFact.cpp中

/**
 * @brief 构造函数实现
 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
 * 
 * 初始化DualThrust选股策略对象，调用基类构造函数设置策略ID。
 * 构造函数体为空，成员变量使用默认值初始化。
 * 实际的参数配置在init函数中完成。
 */
WtStraDtSel::WtStraDtSel(const char* id)  // 构造函数实现
	:SelStrategy(id)  // 调用基类构造函数，传入策略ID
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
WtStraDtSel::~WtStraDtSel()  // 析构函数实现
{
	// 析构函数体为空，使用默认清理
}

/**
 * @brief 获取策略名称的实现
 * @return const char* 返回策略的名称字符串
 * 
 * 该函数返回策略的名称，用于标识策略类型。
 * 返回值为"DualThrustSelection"，表示这是DualThrust选股策略。
 */
const char* WtStraDtSel::getName()  // 获取策略名称函数实现
{
	return "DualThrustSelection";  // 返回策略名称字符串
}

/**
 * @brief 获取所属策略工厂名称的实现
 * @return const char* 返回策略所属的工厂名称字符串
 * 
 * 该函数返回策略所属的策略工厂名称，用于标识策略的来源工厂。
 * 返回值为"WtSelStraFact"，表示该策略由WtSelStraFact工厂创建。
 * 
 * @note FACT_NAME常量定义在WtSelStraFact.cpp中，值为"WtSelStraFact"
 */
const char* WtStraDtSel::getFactName()  // 获取工厂名称函数实现
{
	return FACT_NAME;  // 返回工厂名称常量
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
 * - codes: 合约代码列表，逗号分隔的多个合约代码（必需参数）
 * - stock: 是否为股票，true表示股票模式（不支持做空），false表示期货模式（可选参数，默认为false）
 * 
 * @note 如果配置参数为空，函数返回false
 * @note 如果配置参数中缺少必要参数，可能导致运行时错误
 */
bool WtStraDtSel::init(WTSVariant* cfg)  // 策略初始化函数实现
{
	if (cfg == NULL)  // 检查配置参数是否为空
		return false;  // 如果为空，返回false表示初始化失败

	_days = cfg->getUInt32("days");  // 从配置中读取回看天数，转换为32位无符号整数
	_k1 = cfg->getDouble("k1");  // 从配置中读取上轨系数，转换为双精度浮点数
	_k2 = cfg->getDouble("k2");  // 从配置中读取下轨系数，转换为双精度浮点数

	_period = cfg->getCString("period");  // 从配置中读取K线周期字符串，如"m1"、"m5"、"d1"等
	_count = cfg->getUInt32("count");  // 从配置中读取K线条数，转换为32位无符号整数

	_isstk = cfg->getBoolean("stock");  // 从配置中读取是否为股票标志，转换为布尔值（如果不存在则默认为false）

	//通过参数确定初始化交易代码
	std::string codes = cfg->getCString("codes");  // 从配置中读取合约代码列表字符串，多个代码用逗号分隔
	auto ayCodes = StrUtil::split(codes, ",");  // 使用字符串工具类分割代码列表，按逗号分割成字符串数组
	for (auto& code : ayCodes)  // 遍历分割后的代码数组
		_codes.insert(code);  // 将每个代码插入到代码集合中，用于后续遍历和处理

	return true;  // 返回true表示初始化成功
}

/**
 * @brief 策略初始化完成回调实现
 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
 * 
 * 该函数在策略初始化完成后被调用，用于执行初始化后的准备工作。
 * 主要功能包括：
 * 1. 订阅所有标的的Tick数据，用于接收实时行情
 * 
 * @note 该函数重写了SelStrategy基类的虚函数
 */
void WtStraDtSel::on_init(ISelStraCtx* ctx)  // 初始化完成回调函数实现
{
	for(auto& code : _codes)  // 遍历所有标的代码
	{
		ctx->stra_sub_ticks(code.c_str());  // 订阅标的的Tick数据，确保能接收到实时行情数据
	}
}

/**
 * @brief 策略调度执行入口实现
 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
 * @param uDate 当前日期，格式为YYYYMMDD
 * @param uTime 当前时间，格式为HHMMSS
 * 
 * 该函数是策略的主体逻辑执行入口，在策略调度时被调用。
 * 主要功能包括：
 * 1. 遍历所有标的，对每个标的分别执行选股逻辑
 * 2. 检查标的是否在交易时间内
 * 3. 获取历史K线数据，计算价格波动范围
 * 4. 计算上下突破轨道（上轨和下轨）
 * 5. 根据当前价格与上下轨的关系生成交易信号
 * 6. 执行交易操作（开多、开空、平多、平空）
 * 
 * 交易逻辑：
 * - 无持仓时：价格突破上轨做多，突破下轨做空（仅期货）
 * - 持多仓时：价格跌破下轨平多
 * - 持空仓时：价格突破上轨平空（仅期货）
 * 
 * 算法步骤：
 * 1. 遍历所有标的代码
 * 2. 检查标的是否在交易时间内
 * 3. 获取历史K线数据（数量为_count条）
 * 4. 计算过去_days天（排除最后1根K线）的最高价HH和最低价LL
 * 5. 计算过去_days天（排除最后1根K线）的收盘价最高值HC和最低值LC
 * 6. 计算价格波动范围：Range = max(HH-LC, HC-LL)
 * 7. 获取当前K线的开盘价、最高价、最低价
 * 8. 计算上轨：上轨 = 开盘价 + K1 * Range
 * 9. 计算下轨：下轨 = 开盘价 - K2 * Range
 * 10. 根据当前价格与上下轨的关系生成交易信号
 * 11. 执行交易操作
 * 
 * @note 该函数重写了SelStrategy基类的虚函数
 */
void WtStraDtSel::on_schedule(ISelStraCtx* ctx, uint32_t uDate, uint32_t uTime)  // 策略调度执行函数实现
{
	for (auto& curCode : _codes)  // 遍历所有标的代码，对每个标的分别执行选股逻辑
	{
		WTSSessionInfo* sInfo = ctx->stra_get_sessinfo(curCode.c_str());  // 获取标的的交易时段信息，用于判断是否在交易时间内
		if(!sInfo->isInTradingTime(uTime))  // 检查当前时间是否在交易时间内
			continue;  // 如果不在交易时间内，跳过该标的，继续处理下一个标的

		std::string code = curCode;  // 复制标的代码字符串，用于后续使用
		if (_isstk)  // 检查是否为股票模式
			code += "-";  // 如果是股票，在代码后添加"-"后缀（股票代码格式要求）

		WTSKlineSlice *kline = ctx->stra_get_bars(code.c_str(), _period.c_str(), _count);  // 获取历史K线数据切片，参数：合约代码、周期、条数
		if (kline == NULL)  // 检查K线数据是否获取成功
		{
			//这里可以输出一些日志
			continue;  // 如果获取失败，跳过该标的，继续处理下一个标的
		}

		if (kline->size() == 0)  // 检查K线数据是否为空
		{
			kline->release();  // 释放K线数据切片，避免内存泄漏
			continue;  // 如果数据为空，跳过该标的，继续处理下一个标的
		}

		int32_t trdUnit = 1;  // 初始化交易单位为1（期货默认交易单位为1手）
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
		double highPx = kline->at(-1)->high;  // 获取最后一根K线的最高价，用于判断是否突破上轨
		double lowPx = kline->at(-1)->low;  // 获取最后一根K线的最低价，用于判断是否突破下轨

		double upper_bound = openPx + _k1 * (std::max(hh - lc, hc - ll));  // 计算上轨：上轨 = 开盘价 + K1 * max(HH-LC, HC-LL)，其中max(HH-LC, HC-LL)是价格波动范围
		double lower_bound = openPx - _k2 * std::max(hh - lc, hc - ll);  // 计算下轨：下轨 = 开盘价 - K2 * max(HH-LC, HC-LL)

		WTSCommodityInfo* commInfo = ctx->stra_get_comminfo(curCode.c_str());  // 获取合约信息（虽然获取了但未使用，可能是预留用于将来扩展）

		double curPos = ctx->stra_get_position(curCode.c_str()) / trdUnit;  // 获取当前标的的持仓量，并转换为交易单位（手数或股数）
		if (decimal::eq(curPos, 0))  // 检查当前持仓是否为0（使用高精度小数比较，避免浮点数误差）
		{
			if (highPx >= upper_bound)  // 检查最高价是否大于等于上轨（向上突破）
			{
				ctx->stra_set_position(curCode.c_str(), 1 * trdUnit, "DT_EnterLong");  // 设置持仓为1个交易单位（做多），标记为"DT_EnterLong"
				//向上突破
				ctx->stra_log_info(fmt::format("{} 向上突破{}>={},多仓进场", curCode.c_str(), highPx, upper_bound).c_str());  // 记录日志，格式化输出标的代码、最高价和上轨值
			}
			else if (lowPx <= lower_bound && !_isstk)  // 检查最低价是否小于等于下轨（向下突破），且不是股票模式（股票不支持做空）
			{
				ctx->stra_set_position(curCode.c_str(), -1 * trdUnit, "DT_EnterShort");  // 设置持仓为-1个交易单位（做空），标记为"DT_EnterShort"
				//向下突破
				ctx->stra_log_info(fmt::format("{} 向下突破{}<={},空仓进场", curCode.c_str(), lowPx, lower_bound).c_str());  // 记录日志，格式化输出标的代码、最低价和下轨值
			}
		}
		//else if(curPos > 0)
		else if (decimal::gt(curPos, 0))  // 检查当前持仓是否大于0（持多仓，使用高精度小数比较）
		{
			if (lowPx <= lower_bound)  // 检查最低价是否小于等于下轨（向下突破）
			{
				//多仓出场
				ctx->stra_set_position(curCode.c_str(), 0, "DT_ExitLong");  // 设置持仓为0（平多），标记为"DT_ExitLong"
				ctx->stra_log_info(fmt::format("{} 向下突破{}<={},多仓出场", curCode.c_str(), lowPx, lower_bound).c_str());  // 记录日志，格式化输出标的代码、最低价和下轨值
			}
		}
		//else if(curPos < 0)
		else if (decimal::lt(curPos, 0))  // 检查当前持仓是否小于0（持空仓，使用高精度小数比较）
		{
			if (highPx >= upper_bound && !_isstk)  // 检查最高价是否大于等于上轨（向上突破），且不是股票模式（股票不支持做空）
			{
				//空仓出场
				ctx->stra_set_position(curCode.c_str(), 0, "DT_ExitShort");  // 设置持仓为0（平空），标记为"DT_ExitShort"
				ctx->stra_log_info(fmt::format("{} 向上突破{}>={},空仓出场", curCode.c_str(), highPx, upper_bound).c_str());  // 记录日志，格式化输出标的代码、最高价和上轨值
			}
		}

		//这个释放一定要做
		kline->release();  // 释放K线数据切片，避免内存泄漏（重要：必须释放，否则会导致内存泄漏）
	}
}

/**
 * @brief Tick数据处理回调实现
 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
 * @param stdCode 标准合约代码，触发Tick数据的合约
 * @param newTick 新的Tick数据，包含最新的价格和成交量信息
 * 
 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
 * DualThrust选股策略主要基于K线数据进行交易决策，因此Tick数据处理为空实现。
 * 
 * 如果将来需要基于Tick数据做更精细的交易决策（如：
 * - 基于Tick级别的价格变化进行更精确的入场/出场
 * - 基于Tick级别的成交量进行过滤
 * - 基于Tick级别的买卖盘口进行决策），可以在此函数中实现。
 * 
 * @note 该函数重写了SelStrategy基类的虚函数
 */
void WtStraDtSel::on_tick(ISelStraCtx* ctx, const char* stdCode, WTSTickData* newTick)  // Tick数据处理回调函数实现
{
	// Tick数据处理为空实现，DualThrust选股策略主要基于K线数据进行交易决策
	// 如果将来需要基于Tick数据做更精细的交易决策，可以在此函数中实现
}

/**
 * @brief K线闭合回调实现
 * @param ctx SEL策略上下文对象，提供数据访问和交易执行接口
 * @param stdCode 标准合约代码，触发K线数据的合约
 * @param period K线周期，如"m1"、"m5"等
 * @param newBar 新的K线数据
 * 
 * 该函数在K线闭合时被调用，用于处理K线数据。
 * DualThrust选股策略主要基于定时调度执行，因此K线数据处理为空实现。
 * 
 * @note 该函数重写了SelStrategy基类的虚函数
 */
void WtStraDtSel::on_bar(ISelStraCtx* ctx, const char* stdCode, const char* period, WTSBarStruct* newBar)  // K线闭合回调函数实现
{
	// K线闭合处理为空实现，DualThrust选股策略主要基于定时调度执行
}
