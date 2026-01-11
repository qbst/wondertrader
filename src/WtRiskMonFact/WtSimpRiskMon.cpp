/*!
 * \file WtSimpRiskMon.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader简单风控监控器类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtSimpleRiskMon类的所有方法，包括初始化、启动、停止以及核心的风控检查逻辑。
 * 通过独立线程持续监控组合盘的资金状况，实现基于回撤控制的风险管理功能。
 * 
 * 主要功能：
 * 1. 参数初始化：从配置对象读取各种风控参数，并记录初始化日志
 * 2. 日内回撤风控：监控日内从最高点的回撤幅度，超过阈值时降低仓位
 * 3. 多日回撤风控：监控多日最大动态权益的回撤幅度，超过阈值时清仓
 * 4. 定时检查机制：按照设定的时间间隔定期执行风控检查
 * 5. 线程管理：启动和停止独立的风控检查线程
 * 
 * 风控算法说明：
 * - 日内回撤计算：rate = (maxBal - curBal) * 100 / (maxBal - predynbal)
 *   其中maxBal为当日最大动态权益，curBal为当前动态权益，predynbal为上日动态权益
 * - 多日回撤计算：rate = (maxBal - curBal) * 100 / maxBal
 *   其中maxBal为多日最大动态权益，curBal为当前动态权益
 * - 时间窗口控制：使用日内分钟数计算时间差，避免午盘休息时间影响风控判断
 * 
 * 设计特点：
 * - 多线程异步检查：使用独立线程执行风控检查，不阻塞主交易流程
 * - 精确时间控制：使用毫秒级时间戳和睡眠机制，精确控制检查间隔
 * - 双重保护机制：同时支持日内和多日风控，提供双重保护
 * - 详细日志记录：记录每次检查的详细情况，便于分析和调试
 */

#include "WtSimpRiskMon.h"  // 包含当前类的头文件

#include "../Includes/WTSRiskDef.hpp"  // 包含风控相关数据结构定义，提供WTSPortFundInfo和WTSFundStruct等类型
#include "../Includes/WTSVariant.hpp"  // 包含配置参数变体类，用于读取配置参数
#include "../Share/TimeUtils.hpp"  // 包含时间工具函数，提供getLocalTimeNow等时间相关功能
#include "../Share/decimal.h"  // 包含小数比较工具，提供decimal::eq等精确比较函数
#include "../Share/fmtlib.h"  // 包含格式化字符串库，提供fmt::format等格式化功能

extern const char* FACT_NAME;  // 声明外部工厂名称常量，定义在WtRiskMonFact.cpp中

/**
 * @brief 获取风控监控器名称
 * 
 * 返回当前风控监控器的名称标识，用于区分不同的风控模块。
 * 
 * @return 返回"WtSimpleRiskMon"字符串常量
 */
const char* WtSimpleRiskMon::getName()
{
	return "WtSimpleRiskMon";  // 返回风控监控器名称
}

/**
 * @brief 获取所属工厂名称
 * 
 * 返回创建当前风控监控器的工厂名称，用于工厂管理和对象归属判断。
 * 
 * @return 返回工厂名称常量FACT_NAME的值（"WtRiskMonFact"）
 */
const char* WtSimpleRiskMon::getFactName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 初始化风控监控器
 * 
 * 初始化风控监控器，从配置对象中读取各种风控参数，并记录初始化日志。
 * 首先调用基类的init方法初始化上下文，然后读取所有配置参数。
 * 
 * @param ctx 组合上下文接口指针，提供资金数据、交易状态、日志记录等功能
 * @param cfg 配置参数对象，包含各种风控参数的配置值
 * 
 * 配置参数读取说明：
 * - calc_span: 计算时间间隔（秒），风控检查的执行频率
 * - risk_span: 回撤比较时间（分钟），日内回撤检查的时间窗口
 * - basic_ratio: 基础盈利率（百分比），触发回撤保护的盈利阈值
 * - inner_day_fd: 日内高点回撤边界（百分比），日内回撤触发阈值
 * - inner_day_active: 日内风控是否启用（布尔值）
 * - multi_day_fd: 多日高点回撤边界（百分比），多日回撤触发阈值
 * - multi_day_active: 多日风控是否启用（布尔值）
 * - base_amount: 基础资金规模（金额），用于计算权益比例
 * - risk_scale: 风险控制系数（0-1），触发风控后的仓位比例
 */
void WtSimpleRiskMon::init(WtPortContext* ctx, WTSVariant* cfg)
{
	WtRiskMonitor::init(ctx, cfg);  // 调用基类init方法，初始化上下文对象_ctx

	_calc_span = cfg->getUInt32("calc_span");  // 读取计算时间间隔参数（秒），风控检查的执行频率
	_risk_span = cfg->getUInt32("risk_span");  // 读取回撤比较时间参数（分钟），日内回撤检查的时间窗口
	_basic_ratio = cfg->getUInt32("basic_ratio");  // 读取基础盈利率参数（百分比），触发回撤保护的盈利阈值
	_inner_day_fd = cfg->getDouble("inner_day_fd");  // 读取日内高点回撤边界参数（百分比），日内回撤触发阈值
	_inner_day_active = cfg->getBoolean("inner_day_active");  // 读取日内风控启用标志，true表示启用，false表示禁用
	_multi_day_fd = cfg->getDouble("multi_day_fd");  // 读取多日高点回撤边界参数（百分比），多日回撤触发阈值
	_multi_day_active = cfg->getBoolean("multi_day_active");  // 读取多日风控启用标志，true表示启用，false表示禁用
	_base_amount = cfg->getDouble("base_amount");  // 读取基础资金规模参数（金额），用于计算权益比例的基础资金
	_risk_scale = cfg->getDouble("risk_scale");  // 读取风险控制系数参数（0-1），触发风控后的仓位比例

	// 记录初始化日志，输出所有配置参数的值，便于调试和监控
	ctx->writeRiskLog(fmt::format("Params inited, Checking frequency: {} s, MaxIDD: {}({:.2f}%), MaxMDD: {}({:.2f}%), Capital: {:.1f}, Profit Boudary: {:.2f}%, Calc Span: {} mins, Risk Scale: {:.2f}",
		_calc_span, _inner_day_active ? "ON" : "OFF", _inner_day_fd, _multi_day_active ? "ON" : "OFF", _multi_day_fd, _base_amount, _basic_ratio, _risk_span, _risk_scale).c_str());
	// 日志格式说明：
	// - Checking frequency: 检查频率（秒）
	// - MaxIDD: 日内最大回撤（ON/OFF表示是否启用，百分比表示阈值）
	// - MaxMDD: 多日最大回撤（ON/OFF表示是否启用，百分比表示阈值）
	// - Capital: 基础资金规模
	// - Profit Boudary: 盈利边界（百分比）
	// - Calc Span: 计算时间跨度（分钟）
	// - Risk Scale: 风险控制系数
}

/**
 * @brief 启动风控监控
 * 
 * 启动独立线程，开始执行风控检查逻辑。
 * 线程会按照设定的时间间隔定期检查组合盘的风险状况，
 * 当检测到风险超过阈值时，自动采取相应的风控措施。
 * 
 * 线程执行流程：
 * 1. 检查线程是否已启动，如果已启动则直接返回
 * 2. 创建新线程，执行风控检查循环
 * 3. 在循环中检查交易状态，如果处于交易状态则执行风控检查
 * 4. 执行日内回撤风控检查
 * 5. 执行多日回撤风控检查
 * 6. 等待指定时间间隔后继续下一次检查
 * 7. 当_stopped为true时退出循环
 * 
 * 注意事项：
 * - 如果线程已启动，直接返回，避免重复启动
 * - 线程会在_stopped为true时退出
 * - 线程退出后需要调用stop方法等待线程结束
 */
void WtSimpleRiskMon::run()
{
	if (_thrd)  // 检查线程是否已启动
		return;  // 如果已启动，直接返回，避免重复启动

	// 创建新线程，使用lambda表达式作为线程执行函数
	_thrd.reset(new std::thread([this](){
		// 主循环：持续执行风控检查，直到_stopped标志为true
		while (!_stopped)
		{
			// 检查上下文是否有效且处于交易状态
			if (_ctx && _ctx->isInTrading())
			{
				WTSPortFundInfo* fundInfo = _ctx->getFundInfo();  // 获取组合资金信息对象指针
				const WTSFundStruct& fs = fundInfo->fundInfo();  // 获取资金结构体引用，包含资金和风险指标数据
				
				/*
				 * 风控策略说明（注释中的原始设计思路）：
				 * 条件1: 整体盘子的浮动收益比上一交易日结束时（收盘价计）, 增长 1% 以上
				 *		组合盘的动态权益 ≥ 上日收盘时的动态权益的 101%
				 * 条件2: 30min以内, 从今日高点回调到 80%以下
				 *		30min以内, 今日收益从高点回调到 80%以下
				 *
				 * 动作: 
				 * 方式A:  所有品种减仓（减少到 30% 仓位）, 下一交易日重新按策略新仓位补齐
				 * 方式B:  所有盈利品种都 平仓, 下一交易日重新按策略新仓位补齐
				 */

				// ========== 日内回撤风控检查 ==========
				// 检查日内风控是否启用，且当日最大动态权益已初始化（不为DBL_MAX）
				if (_inner_day_active && fs._max_dyn_bal != DBL_MAX)
				{
					double predynbal = fundInfo->predynbalance() + _base_amount;	// 计算上日动态权益：上日收盘时的动态权益 + 基础资金
					double maxBal = fs._max_dyn_bal + _base_amount;					// 计算当日最大动态权益：当日最高动态权益 + 基础资金
					double curBal = fs._balance + fs._dynprofit + _base_amount;		// 计算当前动态权益：当前资金余额 + 浮动盈亏 + 基础资金

					double rate = 0.0;  // 初始化回撤比例
					// 计算当日盈利回撤比例：如果最大权益不等于上日权益，则计算回撤比例
					if(!decimal::eq(maxBal, predynbal))  // 使用精确比较函数，避免浮点数比较误差
						rate = (maxBal - curBal) * 100 / (maxBal - predynbal);	// 回撤比例 = (最大权益 - 当前权益) * 100 / (最大权益 - 上日权益)

					// 如果当日最大权益超过止盈边界条件（达到盈利保护阈值）
					if (maxBal > (_basic_ratio*predynbal / 100.0))
					{
						/*
						 *	时间窗口处理说明：
						 *	这里要转成日内分钟数处理
						 *	不然如果遇到午盘启动或早盘启动, 
						 *	可能会因为中途休息时间过长, 而不触发风控
						 *	导致更大风险的发生
						 * 
						 * 例如：如果使用绝对时间，午盘休息2小时，即使回撤发生在休息前，
						 * 但检查在休息后，时间差会超过30分钟，导致不触发风控。
						 * 使用日内分钟数可以避免这个问题。
						 */
						uint32_t maxTime = _ctx->transTimeToMin(fundInfo->max_dynbal_time());	// 将最大权益出现时间转换为日内分钟数
						uint32_t curTime = _ctx->transTimeToMin(_ctx->getCurTime());			// 将当前时间转换为日内分钟数

						// 检查是否触发日内回撤风控：
						// 1. 回撤比例 >= 日内回撤阈值
						// 2. 时间差 <= 回撤比较时间窗口
						// 3. 尚未触发仓位限制（避免重复触发）
						if (rate >= _inner_day_fd && curTime - maxTime <= _risk_span && !_limited)
						{
							// 记录风控触发日志
							_ctx->writeRiskLog(fmt::format("Current IDD {:.2f}%, ≥MaxIDD {:.2f}%, Position down to {:.1f}%", rate, _inner_day_fd, _risk_scale).c_str());
							// IDD: Intraday Drawdown（日内回撤）
							// 设置仓位倍数为风险控制系数，降低整体仓位
							_ctx->setVolScale(_risk_scale);
							_limited = true;  // 设置仓位限制标志，避免重复触发
						}
						else
						{
							// 记录当前状态日志（未触发风控）
							_ctx->writeRiskLog(fmt::format("Current Balance Ratio: {:.2f}%, Current IDD: {:.2f}%", curBal*100.0 / predynbal, rate).c_str());
							// Balance Ratio: 当前权益相对于上日权益的比例
							// IDD: 当前日内回撤比例
							//_limited = false;  // 注释掉的代码：不重置限制标志，保持限制状态
						}
					}
					else
					{
						// 如果当日最大权益没有超过盈利边界条件（未达到盈利保护阈值）
						// 记录当前权益比例日志
						_ctx->writeRiskLog(fmt::format("Current Balance Ratio: {:.2f}%", curBal*100.0 / predynbal).c_str());
						//_limited = false;  // 注释掉的代码：不重置限制标志
					}
				}

				// ========== 多日回撤风控检查 ==========
				// 检查多日风控是否启用，且多日最大动态权益已初始化（日期不为0）
				if (_multi_day_active && fs._max_md_dyn_bal._date != 0)
				{
					double maxBal = fs._max_md_dyn_bal._dyn_balance + _base_amount;  // 计算多日最大动态权益：多日最高动态权益 + 基础资金
					double curBal = fs._balance + fs._dynprofit + _base_amount;  // 计算当前动态权益：当前资金余额 + 浮动盈亏 + 基础资金

					// 检查当前权益是否低于多日最大权益（出现回撤）
					if (curBal < maxBal)
					{
						double rate = (maxBal - curBal) * 100 / maxBal;  // 计算多日回撤比例：(最大权益 - 当前权益) * 100 / 最大权益
						// 检查回撤比例是否超过多日回撤阈值
						if (rate >= _multi_day_fd)
						{
							// 记录多日回撤风控触发日志
							_ctx->writeRiskLog(fmt::format("Current MDD {:.2f}%, >= MaxMDD {:.2f}%, Position down to 0.0%", rate, _multi_day_fd).c_str());
							// MDD: Maximum Drawdown（最大回撤）
							// 设置仓位倍数为0，清空所有仓位
							_ctx->setVolScale(0.0);
						}
					}
				}
			}

			// 记录本次检查的时间戳
			_last_time = TimeUtils::getLocalTimeNow();

			// 等待指定时间间隔后继续下一次检查
			// 使用精确的时间控制，避免时间漂移
			while (!_stopped)  // 循环等待，直到_stopped标志为true或时间间隔到达
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(2));  // 每次睡眠2毫秒，避免CPU占用过高
				uint64_t now = TimeUtils::getLocalTimeNow();;  // 获取当前时间戳（毫秒）
				if(now - _last_time >= _calc_span*1000)  // 检查时间间隔是否到达（_calc_span单位为秒，转换为毫秒）
					break;  // 时间间隔到达，退出等待循环，继续下一次风控检查
			}
		}
	}));  // lambda表达式结束，线程创建完成
}

/**
 * @brief 停止风控监控
 * 
 * 停止风控检查线程，等待线程安全退出。
 * 设置_stopped标志为true，通知线程退出，然后等待线程结束。
 * 
 * 停止流程：
 * 1. 设置_stopped标志为true，通知线程退出循环
 * 2. 如果线程已启动，调用join方法等待线程结束
 * 3. 线程结束后，线程对象会被自动销毁（通过智能指针）
 * 
 * 注意事项：
 * - 必须先设置_stopped标志，再等待线程结束
 * - 如果线程未启动，直接返回
 * - join方法会阻塞当前线程，直到目标线程结束
 */
void WtSimpleRiskMon::stop()
{
	_stopped = true;  // 设置停止标志为true，通知风控检查线程退出循环
	if (_thrd)  // 检查线程是否已启动
		_thrd->join();  // 等待线程结束，join方法会阻塞当前线程直到目标线程退出
}