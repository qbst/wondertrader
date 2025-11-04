/*!
 * \file UftMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT极速策略回测模拟器头文件
 *
 * 本文件定义了UftMocker类，它是WonderTrader框架中用于UFT（Ultra Fast Trading）极速策略回测的核心模拟器。
 * 
 * 设计逻辑：
 * 1. 策略执行环境：UftMocker实现了IUftStraCtx接口，为UFT策略提供完整的回测执行环境
 * 2. 数据接收：同时实现IDataSink接口，接收历史数据回放器推送的市场数据（Tick、委托队列、委托明细、逐笔成交等）
 * 3. 订单管理：采用订单队列机制，策略发出的订单会进入订单队列，在合适的时机撮合成交
 * 4. 持仓管理：支持多空双向持仓，区分昨仓和今仓，正确处理T+1规则
 * 5. 盈亏计算：实时计算持仓盈亏、已平仓盈亏、动态盈亏等
 * 6. 数据持久化：回测结束后输出交易记录、持仓记录、资金曲线等CSV文件
 *
 * 主要功能：
 * - 策略生命周期管理：初始化、交易日开始/结束等
 * - 市场数据接收：Tick数据、委托队列、委托明细、逐笔成交、K线数据
 * - 订单处理：接收策略订单，模拟撮合成交
 * - 持仓管理：维护多空持仓明细，计算盈亏，处理T+1规则
 * - 数据查询：为策略提供价格、持仓、资金等数据查询接口
 * - 日志记录：记录交易、平仓、资金等日志
 */
#pragma once
#include <queue>                  // 队列容器，用于任务队列
#include <sstream>                 // 字符串流，用于日志记录

#include "HisDataReplayer.h"       // 历史数据回放器头文件

#include "../Includes/FasterDefs.h"          // 快速定义文件，包含常用类型定义
#include "../Includes/IUftStraCtx.h"         // UFT策略上下文接口
#include "../Includes/UftStrategyDefs.h"    // UFT策略定义文件

#include "../Share/StdUtils.hpp"            // 标准工具库
#include "../Share/DLLHelper.hpp"            // 动态库加载辅助类
#include "../Share/fmtlib.h"                 // 格式化库

class HisDataReplayer;  // 前向声明：历史数据回放器类

/**
 * @class UftMocker
 * @brief UFT极速策略回测模拟器类
 * 
 * 该类是UFT策略回测的核心模拟器，继承自IUftStraCtx和IDataSink接口。
 * 负责在回测环境中模拟UFT策略的执行，包括数据接收、订单处理、持仓管理、盈亏计算等。
 * 
 * 核心特性：
 * - 订单队列机制：策略发出的订单进入订单队列，在tick数据到来时进行撮合
 * - 多空双向持仓：支持同时持有多头和空头持仓，区分昨仓和今仓
 * - T+1规则支持：正确处理T+1市场的冻结持仓和释放逻辑
 * - 完整的日志记录：记录所有交易、平仓、资金等关键信息
 */
class UftMocker : public IDataSink, public IUftStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param replayer 历史数据回放器指针，用于获取历史数据和合约信息
	 * @param name 策略名称，用于标识和日志记录
	 * 
	 * 初始化UFT策略回测模拟器，设置回放器和策略名称。
	 */
	UftMocker(HisDataReplayer* replayer, const char* name);
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理UFT策略回测模拟器占用的资源。
	 */
	virtual ~UftMocker();

private:
	/**
	 * @brief 记录调试日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数列表
	 * 
	 * 使用fmt格式化字符串，然后调用stra_log_debug记录调试日志。
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)
	{
		std::string s = fmt::format(format, args...);  // 格式化字符串
		stra_log_debug(s.c_str());  // 记录调试日志
	}

	/**
	 * @brief 记录信息日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数列表
	 * 
	 * 使用fmt格式化字符串，然后调用stra_log_info记录信息日志。
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)
	{
		std::string s = fmt::format(format, args...);  // 格式化字符串
		stra_log_info(s.c_str());  // 记录信息日志
	}

	/**
	 * @brief 记录错误日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数列表
	 * 
	 * 使用fmt格式化字符串，然后调用stra_log_error记录错误日志。
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)
	{
		std::string s = fmt::format(format, args...);  // 格式化字符串
		stra_log_error(s.c_str());  // 记录错误日志
	}

public:
	//////////////////////////////////////////////////////////////////////////
	//IDataSink接口实现
	/**
	 * @brief 处理Tick数据（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param curTick 当前Tick数据指针
	 * @param pxType 价格类型：0-最新价，1-买入价，2-卖出价，3-收盘价模拟
	 * 
	 * 接收历史数据回放器推送的Tick数据，更新价格缓存，并触发订单撮合和策略回调。
	 */
	virtual void	handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType) override;
	
	/**
	 * @brief 处理委托队列数据（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param curOrdQue 当前委托队列数据指针
	 * 
	 * 接收历史数据回放器推送的委托队列数据，触发策略的on_order_queue回调。
	 */
	virtual void	handle_order_queue(const char* stdCode, WTSOrdQueData* curOrdQue) override;
	
	/**
	 * @brief 处理委托明细数据（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param curOrdDtl 当前委托明细数据指针
	 * 
	 * 接收历史数据回放器推送的委托明细数据，触发策略的on_order_detail回调。
	 */
	virtual void	handle_order_detail(const char* stdCode, WTSOrdDtlData* curOrdDtl) override;
	
	/**
	 * @brief 处理逐笔成交数据（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param curTrans 当前逐笔成交数据指针
	 * 
	 * 接收历史数据回放器推送的逐笔成交数据，触发策略的on_transaction回调。
	 */
	virtual void	handle_transaction(const char* stdCode, WTSTransData* curTrans) override;

	/**
	 * @brief 处理K线闭合事件（IDataSink接口）
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 接收K线闭合事件，触发策略的on_bar回调。
	 */
	virtual void	handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 处理定时调度事件（IDataSink接口）
	 * @param uDate 当前日期
	 * @param uTime 当前时间（分钟级别）
	 * 
	 * 接收定时调度事件，触发策略的定时调度回调。
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
	 * @param curTDate 当前交易日日期
	 * 
	 * 交易日开始时调用，处理T+1规则的持仓转换，并触发策略的on_session_begin回调。
	 */
	virtual void	handle_session_begin(uint32_t curTDate) override;
	
	/**
	 * @brief 处理交易日结束事件（IDataSink接口）
	 * @param curTDate 当前交易日日期
	 * 
	 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
	 */
	virtual void	handle_session_end(uint32_t curTDate) override;

	/**
	 * @brief 处理回测完成事件（IDataSink接口）
	 * 
	 * 回测结束时调用，输出回测结果文件，并触发策略的on_bactest_end回调。
	 */
	virtual void	handle_replay_done() override;

	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当订阅的合约有新的Tick数据时调用，触发策略的on_tick回调。
	 */
	virtual void	on_tick_updated(const char* stdCode, WTSTickData* newTick) override;
	
	/**
	 * @brief 委托队列数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdQue 新的委托队列数据指针
	 * 
	 * 当订阅的合约有新的委托队列数据时调用，触发策略的on_order_queue回调。
	 */
	virtual void	on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue) override;
	
	/**
	 * @brief 委托明细数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdDtl 新的委托明细数据指针
	 * 
	 * 当订阅的合约有新的委托明细数据时调用，触发策略的on_order_detail回调。
	 */
	virtual void	on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;
	
	/**
	 * @brief 逐笔成交数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newTrans 新的逐笔成交数据指针
	 * 
	 * 当订阅的合约有新的逐笔成交数据时调用，触发策略的on_transaction回调。
	 */
	virtual void	on_trans_updated(const char* stdCode, WTSTransData* newTrans) override;

	//////////////////////////////////////////////////////////////////////////
	//IUftStraCtx接口实现
	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 接收Tick数据，更新价格缓存，并触发订单撮合和策略回调。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;

	/**
	 * @brief 委托队列数据回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdQue 新的委托队列数据指针
	 * 
	 * 接收委托队列数据，触发策略的on_order_queue回调。
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) override;

	/**
	 * @brief 委托明细数据回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdDtl 新的委托明细数据指针
	 * 
	 * 接收委托明细数据，触发策略的on_order_detail回调。
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;

	/**
	 * @brief 逐笔成交数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTrans 新的逐笔成交数据指针
	 * 
	 * 接收逐笔成交数据，触发策略的on_transaction回调。
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) override;

	/**
	 * @brief 获取上下文ID
	 * @return 上下文唯一标识符
	 * 
	 * 返回策略上下文的唯一标识符，用于区分不同的策略实例。
	 */
	virtual uint32_t id() override;

	/**
	 * @brief 初始化完成回调
	 * 
	 * 策略初始化完成后调用，触发策略的on_init回调。
	 */
	virtual void on_init() override;

	/**
	 * @brief K线数据回调
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 接收K线数据，触发策略的on_bar回调。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 交易日开始回调
	 * @param curTDate 当前交易日日期
	 * 
	 * 交易日开始时调用，处理T+1规则的持仓转换，并触发策略的on_session_begin回调。
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
	 * @brief 撤销指定订单
	 * @param localid 本地订单ID
	 * @return 撤销成功返回true，失败返回false
	 * 
	 * 撤销指定本地订单ID的订单。订单会被标记为已撤销，并释放冻结持仓。
	 */
	virtual bool stra_cancel(uint32_t localid) override;

	/**
	 * @brief 撤销指定合约的所有订单
	 * @param stdCode 标准化合约代码
	 * @return 被撤销的订单ID列表
	 * 
	 * 撤销指定合约的所有未成交订单。
	 */
	virtual OrderIDs stra_cancel_all(const char* stdCode) override;

	/**
	 * @brief 买入（智能处理平空和开多）
	 * @param stdCode 标准化合约代码
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
	 * @return 订单ID列表
	 * 
	 * 智能买入：如果有多头持仓，先平空；如果还有剩余数量，则开多。
	 */
	virtual OrderIDs stra_buy(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 卖出（智能处理平多和开空）
	 * @param stdCode 标准化合约代码
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
	 * @return 订单ID列表
	 * 
	 * 智能卖出：如果有多头持仓，先平多；如果还有剩余数量，则开空。
	 */
	virtual OrderIDs stra_sell(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 开多
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
	 * @return 本地订单ID，失败返回0
	 * 
	 * 开多仓订单。订单会被添加到订单队列中，等待撮合成交。
	 */
	virtual uint32_t	stra_enter_long(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 开空
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
	 * @return 本地订单ID，失败返回0
	 * 
	 * 开空仓订单。订单会被添加到订单队列中，等待撮合成交。
	 */
	virtual uint32_t	stra_enter_short(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 平多
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param isToday 是否今仓，SHFE、INE专用
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
	 * @return 本地订单ID，失败返回0
	 * 
	 * 平多仓订单。如果是SHFE、INE等需要区分今昨仓的市场，可以通过isToday参数指定平今仓或平昨仓。
	 */
	virtual uint32_t	stra_exit_long(const char* stdCode, double price, double qty, bool isToday = false, int flag = 0) override;

	/**
	 * @brief 平空
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param isToday 是否今仓，SHFE、INE专用
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认为0
	 * @return 本地订单ID，失败返回0
	 * 
	 * 平空仓订单。如果是SHFE、INE等需要区分今昨仓的市场，可以通过isToday参数指定平今仓或平昨仓。
	 */
	virtual uint32_t	stra_exit_short(const char* stdCode, double price, double qty, bool isToday = false, int flag = 0) override;

	/**
	 * @brief 获取合约信息
	 * @param stdCode 标准化合约代码
	 * @return 合约信息指针，如果合约不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的合约信息。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;

	/**
	 * @brief 获取K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期（如"m1"表示1分钟，"d1"表示1日）
	 * @param count 获取的K线数量
	 * @return K线数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的K线数据切片。
	 */
	virtual WTSKlineSlice* stra_get_bars(const char* stdCode, const char* period, uint32_t count) override;

	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 获取的Tick数量
	 * @return Tick数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的Tick数据切片。
	 */
	virtual WTSTickSlice* stra_get_ticks(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取委托明细数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 获取的数据数量
	 * @return 委托明细数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的委托明细数据切片。
	 */
	virtual WTSOrdDtlSlice*	stra_get_order_detail(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取委托队列数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 获取的数据数量
	 * @return 委托队列数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的委托队列数据切片。
	 */
	virtual WTSOrdQueSlice*	stra_get_order_queue(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取逐笔成交数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 获取的数据数量
	 * @return 逐笔成交数据切片指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的逐笔成交数据切片。
	 */
	virtual WTSTransSlice*	stra_get_transaction(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准化合约代码
	 * @return 最新Tick数据指针，如果数据不存在返回NULL
	 * 
	 * 从历史数据回放器获取指定合约的最新Tick数据。
	 */
	virtual WTSTickData* stra_get_last_tick(const char* stdCode) override;

	/**
	 * @brief 获取持仓
	 * @param stdCode 代码，格式如SSE.600000
	 * @param bOnlyValid 获取可用持仓（T+1规则下排除冻结持仓），默认为false
	 * @param iFlag 读取标记：1-多头，2-空头，3-净头寸，默认为3
	 * @return 持仓数量，正数为做多，负数为做空，0表示无持仓
	 * 
	 * 查询指定合约的持仓数量。根据iFlag参数返回多头、空头或净头寸。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, int32_t iFlag = 3) override;

	/**
	 * @brief 获取本地持仓（净头寸）
	 * @param stdCode 标准化合约代码
	 * @return 净头寸（多头持仓减去空头持仓）
	 * 
	 * 返回指定合约的本地净头寸（多头持仓减去空头持仓）。
	 */
	virtual double stra_get_local_position(const char* stdCode) override;

	/**
	 * @brief 枚举持仓
	 * @param stdCode 标准化合约代码，如果为空字符串则枚举所有合约
	 * @return 持仓总数量
	 * 
	 * 遍历持仓，调用策略的on_position回调传递持仓信息。
	 * 返回持仓总数量。
	 */
	virtual double stra_enum_position(const char* stdCode) override;

	/**
	 * @brief 获取未成交数量
	 * @param stdCode 标准化合约代码
	 * @return 未成交数量，正数为买入未成交，负数为卖出未成交
	 * 
	 * 返回指定合约的未成交订单数量总和。
	 */
	virtual double stra_get_undone(const char* stdCode) override;

	/**
	 * @brief 获取当前价格
	 * @param stdCode 标准化合约代码
	 * @return 当前价格，如果合约不存在返回0.0
	 * 
	 * 从历史数据回放器获取指定合约的当前价格。
	 */
	virtual double stra_get_price(const char* stdCode) override;

	/**
	 * @brief 获取当前日期
	 * @return 当前日期（格式：YYYYMMDD）
	 * 
	 * 返回当前回测的日期。
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
	 * @brief 获取当前秒数
	 * @return 当前秒数（0-59）
	 * 
	 * 返回当前回测的秒数。
	 */
	virtual uint32_t stra_get_secs() override;

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的Tick数据。订阅后，该合约的Tick数据会触发策略的on_tick回调。
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;

	/**
	 * @brief 订阅委托队列数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的委托队列数据。订阅后，该合约的委托队列数据会触发策略的on_order_queue回调。
	 */
	virtual void stra_sub_order_queues(const char* stdCode) override;

	/**
	 * @brief 订阅委托明细数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的委托明细数据。订阅后，该合约的委托明细数据会触发策略的on_order_detail回调。
	 */
	virtual void stra_sub_order_details(const char* stdCode) override;

	/**
	 * @brief 订阅逐笔成交数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的逐笔成交数据。订阅后，该合约的逐笔成交数据会触发策略的on_transaction回调。
	 */
	virtual void stra_sub_transactions(const char* stdCode) override;

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
	 * @brief 记录错误日志
	 * @param message 日志消息
	 * 
	 * 将错误日志记录到策略日志中。
	 */
	virtual void stra_log_error(const char* message) override;


	//////////////////////////////////////////////////////////////////////////
	//策略回调接口
	/**
	 * @brief 成交回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 订单成交时调用，更新持仓信息，并触发策略的on_trade回调。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double vol, double price);

	/**
	 * @brief 订单状态回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param totalQty 总委托数量
	 * @param leftQty 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销
	 * 
	 * 订单状态变化时调用，触发策略的on_order回调。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled);

	/**
	 * @brief 通道就绪回调
	 * 
	 * 交易通道就绪时调用，触发策略的on_channel_ready回调。
	 */
	virtual void on_channel_ready();

	/**
	 * @brief 委托回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息内容
	 * 
	 * 委托结果返回时调用，触发策略的on_entrust回调。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message);

public:
	/**
	 * @brief 初始化UFT策略工厂
	 * @param cfg 配置参数，包含策略模块路径和策略参数
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中加载策略动态库，创建策略工厂和策略实例，并初始化策略。
	 */
	bool	init_uft_factory(WTSVariant* cfg);

private:
	typedef std::function<void()> Task;  // 任务函数类型定义
	/**
	 * @brief 提交任务到任务队列
	 * @param task 任务函数
	 * 
	 * 将任务添加到任务队列中，等待执行。
	 */
	void	postTask(Task task);
	
	/**
	 * @brief 处理任务队列
	 * 
	 * 从任务队列中取出任务并执行。
	 */
	void	procTask();

	/**
	 * @brief 处理订单撮合
	 * @param localid 本地订单ID
	 * @return 如果订单已完成（全部成交或已撤销），返回true；否则返回false
	 * 
	 * 根据当前市场数据，尝试撮合指定订单。
	 * 如果订单可以成交，则更新订单状态和持仓信息。
	 */
	bool	procOrder(uint32_t localid);

	/**
	 * @brief 更新持仓
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param qty 成交数量
	 * @param price 成交价格，默认为0.0（使用当前价格）
	 * 
	 * 根据成交信息更新持仓。处理开仓、平仓、平今等不同情况。
	 */
	void	update_position(const char* stdCode, bool isLong, uint32_t offset, double qty, double price = 0.0);
	
	/**
	 * @brief 更新动态盈亏
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 根据当前Tick数据更新指定合约的持仓动态盈亏。
	 */
	void	update_dyn_profit(const char* stdCode, WTSTickData* newTick);

	/**
	 * @brief 输出回测结果文件
	 * 
	 * 将交易日志、平仓日志、资金日志、持仓日志等写入CSV文件。
	 * 所有文件保存在输出目录下的策略名称子目录中。
	 */
	void	dump_outputs();
	
	/**
	 * @brief 记录交易日志
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param curTime 当前时间（纳秒时间戳）
	 * @param price 成交价格
	 * @param qty 成交数量
	 * @param fee 手续费
	 * 
	 * 将交易记录写入交易日志流中，格式为CSV格式。
	 */
	void	log_trade(const char* stdCode, bool isLong, uint32_t offset, uint64_t curTime, double price, double qty, double fee);
	
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
	 * @param totalprofit 累计盈亏
	 * 
	 * 将平仓记录写入平仓日志流中，格式为CSV格式。
	 */
	void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double maxprofit, double maxloss, double totalprofit);

private:
	HisDataReplayer*	_replayer;				// 历史数据回放器指针，用于获取历史数据和合约信息

	bool			_use_newpx;					// 是否使用最新价撮合，true表示使用最新价，false表示使用对手价
	uint32_t		_error_rate;				// 错误率（万分之一），用于模拟订单被随机撤销的概率
	bool			_match_this_tick;			// 是否在当前tick撮合，true表示在tick回调后撮合，false表示在tick回调前撮合

	typedef wt_hashmap<std::string, double> PriceMap;  // 价格映射表类型
	PriceMap		_price_map;					// 价格映射表，缓存每个合约的最新价格


	/**
	 * @struct _StraFactInfo
	 * @brief 策略工厂信息结构体
	 * 
	 * 存储策略工厂的相关信息，包括动态库路径、句柄、工厂指针等。
	 */
	typedef struct _StraFactInfo
	{
		std::string		_module_path;				// 策略模块路径
		DllHandle		_module_inst;				// 动态库句柄
		IUftStrategyFact*	_fact;					// 策略工厂指针
		FuncCreateUftStraFact	_creator;				// 创建工厂函数指针
		FuncDeleteUftStraFact	_remover;				// 删除工厂函数指针

		_StraFactInfo()  // 构造函数
		{
			_module_inst = NULL;  // 初始化动态库句柄为空
			_fact = NULL;         // 初始化工厂指针为空
		}

		~_StraFactInfo()  // 析构函数
		{
			if (_fact)  // 如果工厂指针存在
				_remover(_fact);  // 删除工厂
		}
	} StraFactInfo;
	StraFactInfo	_factory;  // 策略工厂信息

	UftStrategy*	_strategy;  // 策略实例指针

	//StdThreadPtr		_thrd;  // 线程指针（已注释）
	StdUniqueMutex		_mtx;   // 互斥锁，用于保护任务队列
	std::queue<Task>	_tasks; // 任务队列
	//bool				_stopped;  // 停止标志（已注释）

	StdRecurMutex		_mtx_control;  // 递归互斥锁，用于控制任务处理

	/**
	 * @struct _OrderInfo
	 * @brief 订单信息结构体
	 * 
	 * 存储订单的详细信息，包括合约代码、价格、数量、开平标志等。
	 */
	typedef struct _OrderInfo
	{
		bool	_isLong;    // 是否做多
		char	_code[32];  // 合约代码
		double	_price;     // 委托价格
		double	_total;     // 总委托数量
		double	_left;      // 剩余数量
		
		uint32_t	_offset;   // 开平标志：0-开仓，1-平仓，2-平今
		uint32_t	_localid;   // 本地订单ID

		_OrderInfo()  // 构造函数
		{
			memset(this, 0, sizeof(_OrderInfo));  // 初始化所有成员为0
		}

	} OrderInfo;
	typedef wt_hashmap<uint32_t, OrderInfo> Orders;  // 订单映射表类型（本地订单ID -> 订单信息）
	StdRecurMutex	_mtx_ords;  // 互斥锁，用于保护订单映射表
	Orders			_orders;    // 订单映射表

	//用户数据
	typedef wt_hashmap<std::string, std::string> StringHashMap;  // 字符串哈希映射表类型
	StringHashMap	_user_datas;  // 用户数据映射表，用于存储策略的自定义数据
	bool			_ud_modified;  // 用户数据是否已修改标志

	/**
	 * @struct _DetailInfo
	 * @brief 持仓明细信息结构体
	 * 
	 * 存储每笔持仓的详细信息，包括开仓价格、数量、时间、盈亏等。
	 */
	typedef struct _DetailInfo
	{
		double		_price;      // 开仓价格
		double		_volume;     // 持仓数量
		uint64_t	_opentime;   // 开仓时间（纳秒时间戳）
		uint32_t	_opentdate;  // 开仓交易日
		double		_max_profit; // 最大盈利金额
		double		_max_loss;   // 最大亏损金额
		double		_profit;     // 当前盈亏金额

		_DetailInfo()  // 构造函数
		{
			memset(this, 0, sizeof(_DetailInfo));  // 初始化所有成员为0
		}
	} DetailInfo;

	/**
	 * @struct _PosItem
	 * @brief 持仓项结构体
	 * 
	 * 存储一个方向的持仓信息（多头或空头），包括昨仓、今仓、明细等。
	 */
	typedef struct _PosItem
	{
		bool		_long;        // 是否做多
		double		_closeprofit; // 已平仓盈亏
		double		_dynprofit;   // 动态盈亏

		double		_prevol;      // 昨仓数量
		double		_newvol;      // 今仓数量
		double		_preavail;    // 昨仓可用数量
		double		_newavail;    // 今仓可用数量

		std::vector<DetailInfo> _details;  // 持仓明细列表

		_PosItem()  // 构造函数
		{
			_prevol = 0;      // 初始化昨仓数量为0
			_newvol = 0;      // 初始化今仓数量为0
			_preavail = 0;    // 初始化昨仓可用数量为0
			_newavail = 0;    // 初始化今仓可用数量为0

			_closeprofit = 0;  // 初始化已平仓盈亏为0
			_dynprofit = 0;    // 初始化动态盈亏为0
		}

		/**
		 * @brief 获取可用持仓数量
		 * @return 可用持仓数量（昨仓可用 + 今仓可用）
		 */
		inline double valid() const { return _preavail + _newavail; }
		
		/**
		 * @brief 获取总持仓数量
		 * @return 总持仓数量（昨仓 + 今仓）
		 */
		inline double volume() const { return _prevol + _newvol; }
		/**
		 * @brief 获取冻结持仓数量
		 * @return 冻结持仓数量（总持仓 - 可用持仓）
		 */
		inline double frozen() const { return volume() - valid(); }
	} PosItem;

	/**
	 * @struct _PosInfo
	 * @brief 持仓信息结构体
	 * 
	 * 存储一个合约的完整持仓信息，包括多头和空头两个方向的持仓。
	 */
	typedef struct _PosInfo
	{
		PosItem	_long;   // 多头持仓
		PosItem	_short;  // 空头持仓

		/**
		 * @brief 获取总已平仓盈亏
		 * @return 总已平仓盈亏（多头已平仓盈亏 + 空头已平仓盈亏）
		 */
		inline double closeprofit() const{ return _long._closeprofit + _short._closeprofit; }
		
		/**
		 * @brief 获取总动态盈亏
		 * @return 总动态盈亏（多头动态盈亏 + 空头动态盈亏）
		 */
		inline double dynprofit() const { return _long._dynprofit + _short._dynprofit; }
	} PosInfo;
	typedef wt_hashmap<std::string, PosInfo> PositionMap;  // 持仓映射表类型（合约代码 -> 持仓信息）
	PositionMap		_pos_map;  // 持仓映射表，存储所有合约的持仓信息

	std::stringstream	_trade_logs;  // 交易日志流，用于记录交易记录
	std::stringstream	_close_logs;  // 平仓日志流，用于记录平仓记录
	std::stringstream	_fund_logs;   // 资金日志流，用于记录资金曲线
	std::stringstream	_pos_logs;    // 持仓日志流，用于记录持仓记录

	/**
	 * @struct _StraFundInfo
	 * @brief 策略资金信息结构体
	 * 
	 * 存储策略的资金相关信息，包括总已平仓盈亏、总动态盈亏、总手续费等。
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;      // 总已平仓盈亏
		double	_total_dynprofit;   // 总动态盈亏
		double	_total_fees;        // 总手续费

		_StraFundInfo()  // 构造函数
		{
			memset(this, 0, sizeof(_StraFundInfo));  // 初始化所有成员为0
		}
	} StraFundInfo;

	StraFundInfo		_fund_info;  // 策略资金信息

protected:
	uint32_t		_context_id;  // 上下文ID，用于标识策略上下文

	//tick订阅列表
	wt_hashset<std::string> _tick_subs;  // Tick数据订阅列表，存储已订阅Tick数据的合约代码集合
};

