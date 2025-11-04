/*!
* \file SelMocker.h
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief 选股策略回测模拟器头文件
*
* 本文件定义了SelMocker类，它是WonderTrader框架中用于选股策略回测的核心模拟器。
* 
* 设计逻辑：
* 1. 策略执行环境：SelMocker实现了ISelStraCtx接口，为选股策略提供完整的回测执行环境
* 2. 数据接收：同时实现IDataSink接口，接收历史数据回放器推送的市场数据
* 3. 信号机制：采用信号队列机制，策略发出的持仓信号会延迟到下一个tick执行，模拟真实交易场景
* 4. 持仓管理：支持多明细持仓管理，可以跟踪每笔开仓的详细信息（开仓时间、价格、盈亏等）
* 5. T+1规则：支持T+1交易规则，正确处理冻结持仓的释放
* 6. 滑点模拟：支持绝对滑点和比例滑点两种模式，模拟真实交易中的滑点成本
* 7. 盈亏计算：实时计算持仓盈亏、已平仓盈亏、动态盈亏等
* 8. 数据持久化：回测结束后输出交易记录、持仓记录、资金曲线等CSV文件，并保存策略状态JSON
*
* 主要功能：
* - 策略生命周期管理：初始化、交易日开始/结束、定时调度等
* - 市场数据接收：Tick数据、K线数据、定时调度事件
* - 持仓信号处理：接收策略持仓信号，在合适的时机执行
* - 持仓管理：维护持仓明细，计算盈亏，处理T+1规则
* - 数据查询：为策略提供价格、持仓、资金等数据查询接口
* - 日志记录：记录交易、平仓、资金、信号等日志
*/
#pragma once
#include <sstream>          // 字符串流，用于日志记录
#include "HisDataReplayer.h"    // 历史数据回放器头文件

#include "../Includes/FasterDefs.h"          // 快速定义文件，包含常用类型定义
#include "../Includes/ISelStraCtx.h"         // 选股策略上下文接口
#include "../Includes/SelStrategyDefs.h"    // 选股策略定义文件
#include "../Includes/WTSDataDef.hpp"         // WTS数据结构定义
#include "../Share/fmtlib.h"                  // 格式化库
#include "../Share/DLLHelper.hpp"            // 动态库加载辅助类

class SelStrategy;  // 前向声明：选股策略类

USING_NS_WTP;   // 使用WonderTrader命名空间

class HisDataReplayer;  // 前向声明：历史数据回放器类

/**
 * @class SelMocker
 * @brief 选股策略回测模拟器类
 * 
 * 该类是选股策略回测的核心模拟器，继承自ISelStraCtx和IDataSink接口。
 * 负责在回测环境中模拟策略的执行，包括数据接收、信号处理、持仓管理、盈亏计算等。
 * 
 * 核心特性：
 * - 信号延迟执行机制：策略发出的持仓信号会在下一个tick执行，模拟真实交易延迟
 * - 多明细持仓管理：每笔持仓都有独立的明细记录，支持精确的盈亏计算
 * - T+1规则支持：正确处理T+1市场的冻结持仓和释放逻辑
 * - 滑点模拟：支持绝对滑点和比例滑点，模拟真实交易成本
 * - 完整的日志记录：记录所有交易、平仓、信号、资金等关键信息
 */
class SelMocker : public ISelStraCtx, public IDataSink
{
public:
	/**
	 * @brief 构造函数
	 * @param replayer 历史数据回放器指针，用于获取历史数据和合约信息
	 * @param name 策略名称，用于标识和日志记录
	 * @param slippage 滑点设置，默认0（无滑点），单位取决于isRatioSlp参数
	 * @param isRatioSlp 是否使用比例滑点，默认false（绝对滑点，单位：最小变动价位）
	 *                   如果为true，则slippage单位为万分之一（如100表示1%）
	 * 
	 * 初始化选股策略回测模拟器，设置回放器、策略名称和滑点参数。
	 */
	SelMocker(HisDataReplayer* replayer, const char* name, int32_t slippage = 0, bool isRatioSlp = false);
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理选股策略回测模拟器占用的资源。
	 */
	virtual ~SelMocker();

private:
	/**
	 * @brief 记录调试日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数列表
	 * 
	 * 使用fmtutil格式化字符串，然后调用stra_log_debug记录调试日志。
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_debug(buffer);  // 记录调试日志
	}

	/**
	 * @brief 记录信息日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数列表
	 * 
	 * 使用fmtutil格式化字符串，然后调用stra_log_info记录信息日志。
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_info(buffer);  // 记录信息日志
	}

	/**
	 * @brief 记录错误日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数列表
	 * 
	 * 使用fmtutil格式化字符串，然后调用stra_log_error记录错误日志。
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_error(buffer);  // 记录错误日志
	}

private:
	/**
	 * @brief 输出回测结果文件
	 * 
	 * 将交易日志、平仓日志、资金日志、信号日志、持仓日志等写入CSV文件。
	 * 同时保存用户数据到JSON文件。
	 */
	void	dump_outputs();
	
	/**
	 * @brief 输出策略状态数据
	 * 
	 * 将策略的持仓数据、资金数据、信号数据等保存为JSON格式文件。
	 * 用于策略状态恢复和回测结果分析。
	 */
	void	dump_stradata();
	
	/**
	 * @brief 记录信号日志
	 * @param stdCode 标准化合约代码
	 * @param target 目标持仓数量
	 * @param price 信号价格
	 * @param gentime 信号生成时间
	 * @param usertag 用户标签，默认为空字符串
	 * 
	 * 将持仓信号记录到信号日志流中。
	 */
	inline void log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag = "");
	
	/**
	 * @brief 记录交易日志
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多，true为做多，false为做空
	 * @param isOpen 是否开仓，true为开仓，false为平仓
	 * @param curTime 当前时间（纳秒时间戳）
	 * @param price 成交价格
	 * @param qty 成交数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param fee 手续费，默认为0.0
	 * 
	 * 将交易记录写入交易日志流中。
	 */
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag = "", double fee = 0.0);
	
	/**
	 * @brief 记录平仓日志
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多
	 * @param openTime 开仓时间（纳秒时间戳）
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间（纳秒时间戳）
	 * @param closepx 平仓价格
	 * @param qty 平仓数量
	 * @param profit 盈亏金额
	 * @param maxprofit 最大盈利金额
	 * @param maxloss 最大亏损金额
	 * @param totalprofit 累计盈亏，默认为0
	 * @param enterTag 开仓标签，默认为空字符串
	 * @param exitTag 平仓标签，默认为空字符串
	 * @param openBarNo 开仓时的调度次数（Bar序号），默认为0
	 * @param closeBarNo 平仓时的调度次数（Bar序号），默认为0
	 * 
	 * 将平仓记录写入平仓日志流中。
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double maxprofit, double maxloss, double totalprofit = 0, const char* enterTag = "", const char* exitTag = "", uint32_t openBarNo = 0, uint32_t closeBarNo = 0);

	/**
	 * @brief 更新动态盈亏
	 * @param stdCode 标准化合约代码
	 * @param price 当前价格
	 * 
	 * 根据当前价格更新指定合约的持仓动态盈亏。
	 * 同时更新每笔持仓明细的最大盈利、最大亏损、最高价、最低价等信息。
	 */
	void	update_dyn_profit(const char* stdCode, double price);

	/**
	 * @brief 执行持仓设置
	 * @param stdCode 标准化合约代码
	 * @param qty 目标持仓数量，正数为做多，负数为做空
	 * @param price 成交价格，默认为0.0（使用当前价格）
	 * @param userTag 用户标签，默认为空字符串
	 * @param bTriggered 是否已触发，默认为false
	 * 
	 * 根据目标持仓数量执行实际的开仓、加仓、减仓或平仓操作。
	 * 处理持仓方向变化、T+1规则、滑点计算、手续费计算等。
	 */
	void	do_set_position(const char* stdCode, double qty, double price = 0.0, const char* userTag = "", bool bTriggered = false);
	
	/**
	 * @brief 追加持仓信号
	 * @param stdCode 标准化合约代码
	 * @param qty 目标持仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param price 期望成交价格，默认为0.0（使用当前价格）
	 * 
	 * 将策略发出的持仓信号添加到信号队列中。
	 * 信号会在下一个tick到来时执行（通过proc_tick函数）。
	 */
	void	append_signal(const char* stdCode, double qty, const char* userTag = "", double price = 0.0);

	/**
	 * @brief 处理Tick数据
	 * @param stdCode 标准化合约代码
	 * @param last_px 上一笔价格
	 * @param cur_px 当前价格
	 * 
	 * 在tick数据到来时，检查是否有待执行的持仓信号，如果有则执行。
	 * 同时更新持仓的动态盈亏。
	 */
	void	proc_tick(const char* stdCode, double last_px, double cur_px);

public:
	/**
	 * @brief 初始化选股策略工厂
	 * @param cfg 配置参数，包含策略模块路径和策略参数
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中加载策略动态库，创建策略工厂和策略实例，并初始化策略。
	 */
	bool	init_sel_factory(WTSVariant* cfg);

public:
	//////////////////////////////////////////////////////////////////////////
	//IDataSink接口实现
	/**
	 * @brief 处理Tick数据（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param curTick 当前Tick数据指针
	 * @param pxType 价格类型：0-最新价，1-买入价，2-卖出价，3-收盘价模拟
	 * 
	 * 接收历史数据回放器推送的Tick数据，更新价格缓存，并触发信号执行和盈亏更新。
	 */
	virtual void	handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType) override;
	
	/**
	 * @brief 处理K线闭合事件（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 接收K线闭合事件，标记K线状态，并触发策略的on_bar回调。
	 */
	virtual void	handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 处理定时调度事件（IDataSink接口）
	 * @param uDate 当前日期
	 * @param uTime 当前时间（分钟级别）
	 * 
	 * 接收定时调度事件，计算下一次调度时间，并触发策略的on_schedule回调。
	 */
	virtual void	handle_schedule(uint32_t uDate, uint32_t uTime) override;

	/**
	 * @brief 处理初始化事件（IDataSink接口）
	 * 
	 * 回测开始时调用，触发策略的on_init回调。
	 */
	virtual void	handle_init() override;
	
	/**
	 * @brief 处理交易日开始事件（IDataSink接口）
	 * @param uCurDate 当前交易日日期
	 * 
	 * 交易日开始时调用，释放冻结持仓，并触发策略的on_session_begin回调。
	 */
	virtual void	handle_session_begin(uint32_t uCurDate) override;
	
	/**
	 * @brief 处理交易日结束事件（IDataSink接口）
	 * @param uCurDate 当前交易日日期
	 * 
	 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
	 */
	virtual void	handle_session_end(uint32_t uCurDate) override;
	
	/**
	 * @brief 处理回测完成事件（IDataSink接口）
	 * 
	 * 回测结束时调用，输出回测结果文件，并触发策略的on_bactest_end回调。
	 */
	virtual void	handle_replay_done() override;

	//////////////////////////////////////////////////////////////////////////
	//ISelStraCtx接口实现
	/**
	 * @brief 获取上下文ID
	 * @return 上下文唯一标识符
	 * 
	 * 返回策略上下文的唯一标识符，用于区分不同的策略实例。
	 */
	virtual uint32_t id() { return _context_id; }

	//回调函数
	/**
	 * @brief 初始化完成回调
	 * 
	 * 策略初始化完成后调用，触发策略的on_init回调。
	 */
	virtual void on_init() override;
	
	/**
	 * @brief 交易日开始回调
	 * @param curTDate 当前交易日日期
	 * 
	 * 交易日开始时调用，释放冻结持仓，并触发策略的on_session_begin回调。
	 */
	virtual void on_session_begin(uint32_t curTDate) override;
	
	/**
	 * @brief 交易日结束回调
	 * @param curTDate 当前交易日日期
	 * 
	 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
	 */
	virtual void on_session_end(uint32_t curTDate) override;
	
	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据指针
	 * @param bEmitStrategy 是否触发策略回调，默认为true
	 * 
	 * 接收Tick数据，更新价格缓存，并触发策略的on_tick回调。
	 * 注意：实际的Tick处理逻辑已迁移到handle_tick函数。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy = true) override;
	
	/**
	 * @brief K线数据回调
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 接收K线数据，标记K线状态，并触发策略的on_bar_close回调。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 定时调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间（分钟级别）
	 * @param fireTime 触发时间（分钟级别）
	 * @return 是否继续调度，返回true表示继续
	 * 
	 * 定时调度时调用，触发策略的on_schedule回调。
	 * 同时检查持仓，如果持仓不在信号列表中，则自动清仓。
	 */
	virtual bool on_schedule(uint32_t curDate, uint32_t curTime, uint32_t fireTime) override;
	
	/**
	 * @brief 枚举持仓
	 * @param cb 回调函数，用于遍历每个合约的持仓
	 * 
	 * 遍历所有持仓（包括信号队列中的持仓），调用回调函数传递持仓信息。
	 */
	virtual void enum_position(FuncEnumSelPositionCallBack cb) override;

	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当订阅的合约有新的Tick数据时调用，触发策略的on_tick回调。
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;
	
	/**
	 * @brief K线闭合回调
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据指针
	 * 
	 * K线闭合时调用，触发策略的on_bar回调。
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 策略定时调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间（分钟级别）
	 * 
	 * 定时调度时调用，触发策略的on_schedule回调。
	 */
	virtual void on_strategy_schedule(uint32_t curDate, uint32_t curTime) override;


	//////////////////////////////////////////////////////////////////////////
	//策略接口（ISelStraCtx接口实现）
	/**
	 * @brief 获取持仓数量
	 * @param stdCode 标准化合约代码
	 * @param bOnlyValid 是否只返回可用持仓（T+1规则下排除冻结持仓），默认为false
	 * @param userTag 用户标签，用于查询特定标签的持仓，默认为空字符串（查询总持仓）
	 * @return 持仓数量，正数为做多，负数为做空，0表示无持仓
	 * 
	 * 查询指定合约的持仓数量。如果指定了userTag，则只返回该标签的持仓；
	 * 否则返回总持仓。如果有待执行的信号，则返回信号中的目标持仓。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, const char* userTag = "") override;
	
	/**
	 * @brief 设置目标持仓
	 * @param stdCode 标准化合约代码
	 * @param qty 目标持仓数量，正数为做多，负数为做空，0表示清仓
	 * @param userTag 用户标签，默认为空字符串
	 * 
	 * 设置指定合约的目标持仓数量。会将信号添加到信号队列中，在下一个tick执行。
	 * 如果目标持仓与当前持仓相同，则不执行任何操作。
	 */
	virtual void stra_set_position(const char* stdCode, double qty, const char* userTag = "") override;
	
	/**
	 * @brief 获取当前价格
	 * @param stdCode 标准化合约代码
	 * @return 当前价格，如果合约不存在返回0.0
	 * 
	 * 从历史数据回放器获取指定合约的当前价格。
	 */
	virtual double stra_get_price(const char* stdCode) override;

	/**
	 * @brief 读取当日价格
	 * @param stdCode 标准化合约代码
	 * @param flag 价格类型标志：0-最新价，1-开盘价，2-最高价，3-最低价，4-收盘价，默认为0
	 * @return 当日价格，如果合约不存在返回0.0
	 * 
	 * 从历史数据回放器获取指定合约的当日价格。
	 */
	virtual double stra_get_day_price(const char* stdCode, int flag = 0) override;

	/**
	 * @brief 获取当前交易日日期
	 * @return 交易日日期（格式：YYYYMMDD）
	 * 
	 * 返回当前回测的交易日日期。
	 */
	virtual uint32_t stra_get_tdate() override;
	
	/**
	 * @brief 获取当前日期
	 * @return 当前日期（格式：YYYYMMDD）
	 * 
	 * 返回当前回测的日期（可能与交易日不同）。
	 */
	virtual uint32_t stra_get_date() override;
	
	/**
	 * @brief 获取当前时间
	 * @return 当前时间（格式：HHMM，分钟级别）
	 * 
	 * 返回当前回测的时间（分钟级别）。
	 */
	virtual uint32_t stra_get_time() override;

	/**
	 * @brief 获取资金数据
	 * @param flag 数据标志：0-总资产（已平仓盈亏+动态盈亏-手续费），1-已平仓盈亏，2-动态盈亏，3-手续费，默认为0
	 * @return 资金数据
	 * 
	 * 返回策略的资金数据，包括总资产、已平仓盈亏、动态盈亏、手续费等。
	 */
	virtual double stra_get_fund_data(int flag = 0) override;

	/**
	 * @brief 获取首次开仓时间
	 * @param stdCode 标准化合约代码
	 * @return 首次开仓时间（纳秒时间戳），如果没有持仓返回0
	 * 
	 * 返回指定合约的首次开仓时间。
	 */
	virtual uint64_t stra_get_first_entertime(const char* stdCode) override;
	
	/**
	 * @brief 获取最后一次开仓时间
	 * @param stdCode 标准化合约代码
	 * @return 最后一次开仓时间（纳秒时间戳），如果没有持仓返回0
	 * 
	 * 返回指定合约的最后一次开仓时间。
	 */
	virtual uint64_t stra_get_last_entertime(const char* stdCode) override;
	
	/**
	 * @brief 获取最后一次平仓时间
	 * @param stdCode 标准化合约代码
	 * @return 最后一次平仓时间（纳秒时间戳），如果没有平仓记录返回0
	 * 
	 * 返回指定合约的最后一次平仓时间。
	 */
	virtual uint64_t stra_get_last_exittime(const char* stdCode) override;
	
	/**
	 * @brief 获取最后一次开仓价格
	 * @param stdCode 标准化合约代码
	 * @return 最后一次开仓价格，如果没有持仓返回0.0
	 * 
	 * 返回指定合约的最后一次开仓价格。
	 */
	virtual double stra_get_last_enterprice(const char* stdCode) override;
	
	/**
	 * @brief 获取最后一次开仓标签
	 * @param stdCode 标准化合约代码
	 * @return 最后一次开仓标签字符串，如果没有持仓返回空字符串
	 * 
	 * 返回指定合约的最后一次开仓的用户标签。
	 */
	virtual const char* stra_get_last_entertag(const char* stdCode) override;
	
	/**
	 * @brief 获取持仓平均成本价
	 * @param stdCode 标准化合约代码
	 * @return 持仓平均成本价，如果没有持仓返回0.0
	 * 
	 * 计算指定合约的持仓平均成本价（加权平均）。
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) override;
	
	/**
	 * @brief 获取持仓盈亏
	 * @param stdCode 标准化合约代码
	 * @return 持仓盈亏金额，如果没有持仓返回0.0
	 * 
	 * 返回指定合约的持仓动态盈亏（未实现盈亏）。
	 */
	virtual double stra_get_position_profit(const char* stdCode) override;

	/**
	 * @brief 获取指定标签的持仓开仓时间
	 * @param stdCode 标准化合约代码
	 * @param userTag 用户标签
	 * @return 开仓时间（纳秒时间戳），如果没有该标签的持仓返回0
	 * 
	 * 返回指定合约和标签的持仓开仓时间。
	 */
	virtual uint64_t stra_get_detail_entertime(const char* stdCode, const char* userTag) override;
	
	/**
	 * @brief 获取指定标签的持仓成本价
	 * @param stdCode 标准化合约代码
	 * @param userTag 用户标签
	 * @return 成本价，如果没有该标签的持仓返回0.0
	 * 
	 * 返回指定合约和标签的持仓成本价。
	 */
	virtual double stra_get_detail_cost(const char* stdCode, const char* userTag) override;
	
	/**
	 * @brief 获取指定标签的持仓盈亏
	 * @param stdCode 标准化合约代码
	 * @param userTag 用户标签
	 * @param flag 盈亏类型标志：0-当前盈亏，1-最大盈利，-1-最大亏损，2-最高价，-2-最低价，默认为0
	 * @return 持仓盈亏数据，如果没有该标签的持仓返回0.0
	 * 
	 * 返回指定合约和标签的持仓盈亏信息。
	 */
	virtual double stra_get_detail_profit(const char* stdCode, const char* userTag, int flag = 0) override;

	/**
	 * @brief 获取合约信息
	 * @param stdCode 标准化合约代码
	 * @return 合约信息指针，如果合约不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的合约信息。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;
	
	/**
	 * @brief 获取交易时间模板信息
	 * @param stdCode 标准化合约代码
	 * @return 交易时间模板信息指针，如果合约不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的交易时间模板信息。
	 */
	virtual WTSSessionInfo* stra_get_sessinfo(const char* stdCode) override;
	
	/**
	 * @brief 获取K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期（如"m1"表示1分钟，"d1"表示1日）
	 * @param count 获取的K线数量
	 * @return K线数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的K线数据切片。
	 * 同时标记该K线周期为未闭合状态（用于K线闭合判断）。
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count) override;
	
	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 获取的Tick数量
	 * @return Tick数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的Tick数据切片。
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) override;
	
	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准化合约代码
	 * @return 最新Tick数据指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的最新Tick数据。
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) override;

	/**
	 * @brief 获取分月合约代码
	 * @param stdCode 标准化合约代码（如"SHFE.au.2005"）
	 * @return 分月合约代码（如"au2005"）
	 * 
	 * 从标准化合约代码中提取分月合约代码。
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) override;

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的Tick数据。订阅后，该合约的Tick数据会触发策略的on_tick回调。
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;

	/**
	 * @brief 记录信息日志
	 * @param message 日志消息
	 * 
	 * 将信息日志记录到策略日志中。
	 */
	virtual void stra_log_info(const char* message) override;
	
	/**
	 * @brief 记录调试日志
	 * @param message 日志消息
	 * 
	 * 将调试日志记录到策略日志中。
	 */
	virtual void stra_log_debug(const char* message) override;
	
	/**
	 * @brief 记录警告日志
	 * @param message 日志消息
	 * 
	 * 将警告日志记录到策略日志中。
	 */
	virtual void stra_log_warn(const char* message) override;
	
	/**
	 * @brief 记录错误日志
	 * @param message 日志消息
	 * 
	 * 将错误日志记录到策略日志中。
	 */
	virtual void stra_log_error(const char* message) override;

	/**
	 * @brief 保存用户数据
	 * @param key 数据键
	 * @param val 数据值
	 * 
	 * 保存用户自定义数据，回测结束后会保存到JSON文件中。
	 */
	virtual void stra_save_user_data(const char* key, const char* val) override;

	/**
	 * @brief 加载用户数据
	 * @param key 数据键
	 * @param defVal 默认值，如果数据不存在则返回此值，默认为空字符串
	 * @return 数据值字符串
	 * 
	 * 加载用户自定义数据。如果数据不存在，返回默认值。
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") override;

protected:
	uint32_t			_context_id;			// 上下文唯一标识符，用于区分不同的策略实例
	HisDataReplayer*	_replayer;				// 历史数据回放器指针，用于获取历史数据和合约信息

	uint64_t		_total_calc_time;			// 总计算时间（微秒），用于统计策略执行性能
	uint32_t		_emit_times;				// 总计算次数，用于统计策略执行次数
	int32_t			_slippage;					// 成交滑点，单位取决于_ratio_slippage
	bool			_ratio_slippage;			// 是否比例滑点，true表示比例滑点（单位：万分之一），false表示绝对滑点（单位：最小变动价位）
	uint32_t		_schedule_times;			// 调度次数，记录定时调度被调用的次数

	std::string		_main_key;					// 主键字符串，用于标识策略实例

	/**
	 * @struct _KlineTag
	 * @brief K线标签结构体
	 * 
	 * 用于标记K线的状态，包括是否闭合和闭合次数。
	 */
	typedef struct _KlineTag
	{
		bool		_closed;					// 是否已闭合，true表示K线已闭合，false表示K线未闭合
		uint32_t	_count;						// 闭合次数，记录该K线周期被闭合的次数

		/**
		 * @brief 构造函数
		 * 
		 * 初始化K线标签，默认为未闭合状态，闭合次数为0。
		 */
		_KlineTag() :_closed(false), _count(0){}

	} KlineTag;
	typedef wt_hashmap<std::string, KlineTag> KlineTags;  // K线标签映射表类型，键为"合约代码#周期"格式
	KlineTags	_kline_tags;					// K线标签映射表，用于跟踪每个合约每个周期的K线状态

	typedef std::pair<double, uint64_t>	PriceInfo;  // 价格信息类型，包含价格和时间戳
	typedef wt_hashmap<std::string, PriceInfo> PriceMap;  // 价格映射表类型
	PriceMap		_price_map;					// 价格映射表，缓存每个合约的最新价格和时间戳

	/**
	 * @struct _DetailInfo
	 * @brief 持仓明细信息结构体
	 * 
	 * 记录每笔持仓的详细信息，包括开仓价格、数量、时间、盈亏等。
	 */
	typedef struct _DetailInfo
	{
		bool		_long;						// 是否做多，true为做多，false为做空
		double		_price;						// 开仓价格
		double		_volume;					// 持仓数量
		uint64_t	_opentime;					// 开仓时间（纳秒时间戳）
		uint32_t	_opentdate;					// 开仓交易日日期（格式：YYYYMMDD）
		double		_max_profit;				// 最大盈利金额，记录该持仓明细的最大盈利
		double		_max_loss;					// 最大亏损金额，记录该持仓明细的最大亏损
		double		_max_price;					// 最高价格，记录该持仓明细期间的最高价格
		double		_min_price;					// 最低价格，记录该持仓明细期间的最低价格
		double		_profit;					// 当前盈亏金额，根据当前价格计算的动态盈亏
		char		_opentag[32];				// 开仓标签，用户自定义的标签字符串
		uint32_t	_open_barno;				// 开仓时的调度次数（Bar序号），用于记录开仓时的调度次数

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓明细信息，所有字段清零。
		 */
		_DetailInfo()
		{
			memset(this, 0, sizeof(_DetailInfo));
		}
	} DetailInfo;

	/**
	 * @struct _PosInfo
	 * @brief 持仓信息结构体
	 * 
	 * 记录每个合约的持仓信息，包括总持仓、盈亏、时间等。
	 */
	typedef struct _PosInfo
	{
		double		_volume;					// 总持仓数量，正数为做多，负数为做空
		double		_closeprofit;				// 已平仓盈亏金额，累计已平仓部分的盈亏
		double		_dynprofit;					// 动态盈亏金额，当前持仓的未实现盈亏
		uint64_t	_last_entertime;				// 最后一次开仓时间（纳秒时间戳）
		uint64_t	_last_exittime;				// 最后一次平仓时间（纳秒时间戳）
		double		_frozen;						// 冻结持仓数量（T+1规则下使用）

		std::vector<DetailInfo> _details;		// 持仓明细列表，记录每笔持仓的详细信息

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓信息，所有字段清零。
		 */
		_PosInfo()
		{
			_volume = 0;
			_closeprofit = 0;
			_dynprofit = 0;
			_frozen = 0;
			_last_entertime = 0;
			_last_exittime = 0;
		}

		/**
		 * @brief 获取可用持仓数量
		 * @return 可用持仓数量（总持仓减去冻结持仓）
		 * 
		 * 计算可用持仓数量，用于T+1规则下的持仓查询。
		 */
		inline double valid() const { return _volume - _frozen; }
	} PosInfo;
	typedef wt_hashmap<std::string, PosInfo> PositionMap;  // 持仓映射表类型
	PositionMap		_pos_map;					// 持仓映射表，记录每个合约的持仓信息

	/**
	 * @struct _SigInfo
	 * @brief 信号信息结构体
	 * 
	 * 记录持仓信号的信息，包括目标持仓、价格、时间等。
	 */
	typedef struct _SigInfo
	{
		double		_volume;					// 目标持仓数量，正数为做多，负数为做空
		std::string	_usertag;					// 用户标签，策略自定义的标签字符串
		double		_sigprice;					// 信号价格，信号生成时的价格
		double		_desprice;					// 期望成交价格，如果为0则使用当前价格
		bool		_triggered;					// 是否已触发，true表示信号已执行，false表示信号待执行
		uint64_t	_gentime;					// 信号生成时间（纳秒时间戳）

		/**
		 * @brief 构造函数
		 * 
		 * 初始化信号信息，所有字段清零。
		 */
		_SigInfo()
		{
			_volume = 0;
			_sigprice = 0;
			_desprice = 0;
			_triggered = false;
			_gentime = 0;
		}
	}SigInfo;
	typedef wt_hashmap<std::string, SigInfo>	SignalMap;  // 信号映射表类型
	SignalMap		_sig_map;					// 信号映射表，记录每个合约的持仓信号

	std::stringstream	_trade_logs;				// 交易日志流，用于记录交易记录
	std::stringstream	_close_logs;				// 平仓日志流，用于记录平仓记录
	std::stringstream	_fund_logs;					// 资金日志流，用于记录资金曲线
	std::stringstream	_sig_logs;					// 信号日志流，用于记录持仓信号
	std::stringstream	_pos_logs;					// 持仓日志流，用于记录持仓变化

	bool			_is_in_schedule;				// 是否在自动调度中，用于标记是否正在执行定时调度

	//用户数据
	typedef wt_hashmap<std::string, std::string> StringHashMap;  // 字符串哈希映射表类型
	StringHashMap	_user_datas;					// 用户数据映射表，存储用户自定义数据
	bool			_ud_modified;					// 用户数据是否已修改，用于判断是否需要保存用户数据

	/**
	 * @struct _StraFundInfo
	 * @brief 策略资金信息结构体
	 * 
	 * 记录策略的资金信息，包括盈亏、手续费等。
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;					// 总已平仓盈亏金额，累计已平仓部分的盈亏
		double	_total_dynprofit;				// 总动态盈亏金额，当前所有持仓的未实现盈亏总和
		double	_total_fees;					// 总手续费金额，累计交易产生的手续费

		/**
		 * @brief 构造函数
		 * 
		 * 初始化资金信息，所有字段清零。
		 */
		_StraFundInfo()
		{
			memset(this, 0, sizeof(_StraFundInfo));
		}
	} StraFundInfo;

	StraFundInfo		_fund_info;					// 策略资金信息对象，记录策略的资金状态

	/**
	 * @struct _StraFactInfo
	 * @brief 策略工厂信息结构体
	 * 
	 * 记录策略工厂的相关信息，包括动态库句柄、工厂指针等。
	 */
	typedef struct _StraFactInfo
	{
		std::string		_module_path;				// 策略模块路径，动态库文件路径
		DllHandle		_module_inst;				// 动态库句柄，用于管理动态库
		ISelStrategyFact*		_fact;					// 策略工厂指针，用于创建策略实例
		FuncCreateSelStraFact	_creator;				// 创建工厂函数指针，用于创建策略工厂
		FuncDeleteSelStraFact	_remover;				// 删除工厂函数指针，用于释放策略工厂

		/**
		 * @brief 构造函数
		 * 
		 * 初始化策略工厂信息，动态库句柄和工厂指针设为NULL。
		 */
		_StraFactInfo()
		{
			_module_inst = NULL;
			_fact = NULL;
		}

		/**
		 * @brief 析构函数
		 * 
		 * 释放策略工厂资源，如果工厂存在则调用删除函数释放。
		 */
		~_StraFactInfo()
		{
			if (_fact)
				_remover(_fact);  // 释放策略工厂
		}
	} StraFactInfo;
	StraFactInfo	_factory;						// 策略工厂信息对象，管理策略工厂的生命周期

	SelStrategy*	_strategy;						// 策略实例指针，指向当前执行的策略对象

	uint32_t		_cur_tdate;					// 当前交易日日期（格式：YYYYMMDD）

	wt_hashset<std::string> _tick_subs;				// Tick订阅列表，记录已订阅Tick数据的合约代码集合
};