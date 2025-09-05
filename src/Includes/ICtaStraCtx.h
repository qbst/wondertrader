/*!
 * \file ICtaStraCtx.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略上下文接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中CTA策略上下文的核心接口，为CTA策略提供完整的运行环境和交易接口。
 * 主要功能包括：
 * 1. 定义CTA策略上下文接口，提供策略运行环境
 * 2. 提供完整的交易接口：开多、开空、平多、平空、设置目标仓位等
 * 3. 支持市场数据访问：K线数据、Tick数据、价格查询等
 * 4. 提供持仓管理和查询功能
 * 5. 支持策略日志记录和用户数据存储
 * 6. 提供图表和指标管理功能
 * 
 * 该类主要用于WonderTrader框架中的CTA策略开发，为量化交易策略提供统一的运行环境和交易接口。
 * 通过完整的接口设计，支持策略的完整生命周期管理和交易执行。
 */
#pragma once  // 防止头文件重复包含
#include<string>  // 包含字符串支持
#include <stdint.h>  // 包含固定大小整数类型
#include <functional>  // 包含函数对象支持
#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSCommodityInfo;  // 前向声明：WTS品种信息类
class WTSTickData;  // 前向声明：WTS Tick数据结构类
struct WTSBarStruct;  // 前向声明：WTS K线结构体
class WTSKlineSlice;  // 前向声明：WTS K线切片类
class WTSTickSlice;  // 前向声明：WTS Tick切片类

//typedef void(*FuncEnumPositionCallBack)(const char* stdCode, int32_t qty);  // 注释掉的旧式函数指针类型定义
typedef std::function<void(const char*, double)> FuncEnumCtaPosCallBack;  // CTA持仓枚举回调函数类型定义，使用std::function

/**
 * @class ICtaStraCtx
 * @brief CTA策略上下文接口类
 * 
 * 该类定义了CTA策略上下文的核心接口，为CTA策略提供完整的运行环境和交易接口。
 * 包括市场数据访问、交易执行、持仓管理、日志记录、图表管理等功能。
 * 
 * 主要特性：
 * - 提供完整的交易接口（开多、开空、平多、平空、设置目标仓位）
 * - 支持市场数据访问（K线、Tick、价格查询）
 * - 提供持仓管理和查询功能
 * - 支持策略日志记录和用户数据存储
 * - 提供图表和指标管理功能
 * - 支持策略生命周期管理
 */
class ICtaStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param name 策略上下文名称
	 * 
	 * 初始化CTA策略上下文对象，设置上下文名称。
	 */
	ICtaStraCtx(const char* name) :_name(name){}  // 初始化策略上下文名称

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~ICtaStraCtx(){}  // 虚析构函数，支持继承

	/**
	 * @brief 获取策略上下文名称
	 * @return const char* 返回策略上下文的名称
	 * 
	 * 该函数返回策略上下文的名称，用于标识和管理不同的策略上下文。
	 */
	inline const char* name() const{ return _name.c_str(); }  // 内联函数：返回策略上下文名称

public:
	/**
	 * @brief 获取策略上下文ID
	 * @return uint32_t 返回策略上下文的唯一标识符
	 * 
	 * 该函数返回策略上下文的唯一标识符，用于在系统中识别策略上下文。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t id() = 0;  // 纯虚函数：获取策略上下文ID

	//回调函数
	/**
	 * @brief 策略初始化回调
	 * 
	 * 该函数在策略初始化时被调用，用于执行初始化后的准备工作。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_init() = 0;  // 纯虚函数：策略初始化回调

	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期
	 * 
	 * 该函数在每个交易日开始时被调用，用于执行交易日开始时的准备工作。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_session_begin(uint32_t uTDate) = 0;  // 纯虚函数：交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期
	 * 
	 * 该函数在每个交易日结束时被调用，用于执行交易日结束时的清理工作。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_session_end(uint32_t uTDate) = 0;  // 纯虚函数：交易日结束回调

	/**
	 * @brief Tick数据处理回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据
	 * @param bEmitStrategy 是否触发策略回调，默认为true
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy = true) = 0;  // 纯虚函数：Tick数据处理回调

	/**
	 * @brief K线数据处理回调
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据
	 * 
	 * 该函数在接收到新的K线数据时被调用，用于处理K线数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) = 0;  // 纯虚函数：K线数据处理回调

	/**
	 * @brief 策略调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * @return bool 调度成功返回true，失败返回false
	 * 
	 * 该函数在策略调度时被调用，用于执行策略的主要逻辑。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool on_schedule(uint32_t curDate, uint32_t curTime) = 0;  // 纯虚函数：策略调度回调

	/**
	 * @brief 回测结束事件回调
	 * 
	 * 该函数在回测结束时被调用，只在回测模式下才会触发。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_bactest_end() {};  // 虚函数：回测结束事件回调，默认实现为空

	/**
	 * @brief 重算完成回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * 
	 * 该函数在重算完成后被调用，设计目的是要把on_calculate分成两步。
	 * 方便一些外挂的逻辑接入进来，可以在on_calculate_done执行信号。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_calculate_done(uint32_t curDate, uint32_t curTime) { };  // 虚函数：重算完成回调，默认实现为空

	/**
	 * @brief K线闭合回调
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据
	 * 
	 * 该函数在K线闭合时被调用，用于处理K线闭合事件。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) = 0;  // 纯虚函数：K线闭合回调

	/**
	 * @brief 策略计算回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * 
	 * 该函数在策略计算时被调用，用于执行策略的计算逻辑。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_calculate(uint32_t curDate, uint32_t curTime) = 0;  // 纯虚函数：策略计算回调

	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 该函数在Tick数据更新时被调用，用于处理Tick数据更新事件。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick){}  // 虚函数：Tick数据更新回调，默认实现为空

	/**
	 * @brief 条件单触发回调
	 * @param stdCode 标准合约代码
	 * @param target 目标价格
	 * @param price 触发价格
	 * @param usertag 用户标签
	 * 
	 * 该函数在条件单触发时被调用，用于处理条件单触发事件。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_condition_triggered(const char* stdCode, double target, double price, const char* usertag){}  // 虚函数：条件单触发回调，默认实现为空

	/**
	 * @brief 枚举持仓
	 * @param cb 持仓枚举回调函数
	 * @param bForExecute 是否用于执行，默认为false
	 * 
	 * 该函数枚举当前所有持仓，通过回调函数通知调用者每个持仓的信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void enum_position(FuncEnumCtaPosCallBack cb, bool bForExecute = false) = 0;  // 纯虚函数：枚举持仓

	//策略接口
	/**
	 * @brief 开多仓
	 * @param stdCode 标准合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 该函数执行开多仓操作，支持市价单和限价单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_enter_long(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) = 0;  // 纯虚函数：开多仓

	/**
	 * @brief 开空仓
	 * @param stdCode 标准合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 该函数执行开空仓操作，支持市价单和限价单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_enter_short(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) = 0;  // 纯虚函数：开空仓

	/**
	 * @brief 平多仓
	 * @param stdCode 标准合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 该函数执行平多仓操作，支持市价单和限价单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_exit_long(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) = 0;  // 纯虚函数：平多仓

	/**
	 * @brief 平空仓
	 * @param stdCode 标准合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 该函数执行平空仓操作，支持市价单和限价单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_exit_short(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) = 0;  // 纯虚函数：平空仓

	/**
	 * @brief 获取当前持仓
	 * @param stdCode 合约代码
	 * @param bOnlyValid 是否只读可用持仓，默认为false
	 * @param userTag 下单标记，如果为空则读取持仓汇总，否则读取对应的持仓明细
	 * @return double 返回持仓数量，正数表示多头，负数表示空头
	 * 
	 * 该函数获取指定合约的当前持仓数量。
	 * 如果下单标记为空，则读取持仓汇总；如果下单标记不为空，则读取对应的持仓明细。
	 * 只有当userTag为空时，bOnlyValid参数才生效，主要针对T+1的品种。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, const char* userTag = "") = 0;  // 纯虚函数：获取当前持仓

	/**
	 * @brief 设置目标仓位
	 * @param stdCode 标准合约代码
	 * @param qty 目标仓位数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 该函数设置指定合约的目标仓位，系统会自动调整持仓以达到目标仓位。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_set_position(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) = 0;  // 纯虚函数：设置目标仓位

	/**
	 * @brief 获取当前价格
	 * @param stdCode 标准合约代码
	 * @return double 返回当前价格
	 * 
	 * 该函数获取指定合约的当前价格。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_price(const char* stdCode) = 0;  // 纯虚函数：获取当前价格

	/**
	 * @brief 读取当日价格
	 * @param stdCode 合约代码
	 * @param flag 价格标记：0-开盘价，1-最高价，2-最低价，3-收盘价/最新价
	 * @return double 返回对应的价格
	 * 
	 * 该函数读取指定合约的当日价格，支持开盘价、最高价、最低价、收盘价等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_day_price(const char* stdCode, int flag = 0) = 0;  // 纯虚函数：读取当日价格

	/**
	 * @brief 获取交易日
	 * @return uint32_t 返回当前交易日，格式为YYYYMMDD
	 * 
	 * 该函数获取当前交易日。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t stra_get_tdate() = 0;  // 纯虚函数：获取交易日

	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期，格式为YYYYMMDD
	 * 
	 * 该函数获取当前日期。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t stra_get_date() = 0;  // 纯虚函数：获取当前日期

	/**
	 * @brief 获取当前时间
	 * @return uint32_t 返回当前时间，格式为HHMMSS
	 * 
	 * 该函数获取当前时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t stra_get_time() = 0;  // 纯虚函数：获取当前时间

	/**
	 * @brief 获取资金数据
	 * @param flag 资金数据标记，默认为0
	 * @return double 返回资金数据
	 * 
	 * 该函数获取资金相关数据，如可用资金、总资金等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_fund_data(int flag = 0) = 0;  // 纯虚函数：获取资金数据

	/**
	 * @brief 获取首次进场时间
	 * @param stdCode 标准合约代码
	 * @return uint64_t 返回首次进场时间，格式为YYYYMMDDHHMMSS
	 * 
	 * 该函数获取指定合约的首次进场时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint64_t stra_get_first_entertime(const char* stdCode) = 0;  // 纯虚函数：获取首次进场时间

	/**
	 * @brief 获取最后进场时间
	 * @param stdCode 标准合约代码
	 * @return uint64_t 返回最后进场时间，格式为YYYYMMDDHHMMSS
	 * 
	 * 该函数获取指定合约的最后进场时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint64_t stra_get_last_entertime(const char* stdCode) = 0;  // 纯虚函数：获取最后进场时间

	/**
	 * @brief 获取最后出场时间
	 * @param stdCode 标准合约代码
	 * @return uint64_t 返回最后出场时间，格式为YYYYMMDDHHMMSS
	 * 
	 * 该函数获取指定合约的最后出场时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint64_t stra_get_last_exittime(const char* stdCode) = 0;  // 纯虚函数：获取最后出场时间

	/**
	 * @brief 获取最后进场价格
	 * @param stdCode 标准合约代码
	 * @return double 返回最后进场价格
	 * 
	 * 该函数获取指定合约的最后进场价格。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_last_enterprice(const char* stdCode) = 0;  // 纯虚函数：获取最后进场价格

	/**
	 * @brief 获取持仓均价
	 * @param stdCode 标准合约代码
	 * @return double 返回持仓均价
	 * 
	 * 该函数获取指定合约的持仓均价。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) = 0;  // 纯虚函数：获取持仓均价

	/**
	 * @brief 获取持仓盈亏
	 * @param stdCode 标准合约代码
	 * @return double 返回持仓盈亏
	 * 
	 * 该函数获取指定合约的持仓盈亏。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_position_profit(const char* stdCode) = 0;  // 纯虚函数：获取持仓盈亏

	/**
	 * @brief 获取持仓明细进场时间
	 * @param stdCode 标准合约代码
	 * @param userTag 用户标签
	 * @return uint64_t 返回持仓明细进场时间，格式为YYYYMMDDHHMMSS
	 * 
	 * 该函数获取指定合约和用户标签的持仓明细进场时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint64_t stra_get_detail_entertime(const char* stdCode, const char* userTag) = 0;  // 纯虚函数：获取持仓明细进场时间

	/**
	 * @brief 获取持仓明细成本
	 * @param stdCode 标准合约代码
	 * @param userTag 用户标签
	 * @return double 返回持仓明细成本
	 * 
	 * 该函数获取指定合约和用户标签的持仓明细成本。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_detail_cost(const char* stdCode, const char* userTag) = 0;  // 纯虚函数：获取持仓明细成本

	/**
	 * @brief 读取持仓明细的浮盈
	 * @param stdCode 合约代码
	 * @param userTag 下单标记
	 * @param flag 浮盈标志：0-浮动盈亏，1-最大浮盈，2-最高浮动价格，-1-最大浮亏，-2-最小浮动价格
	 * @return double 返回对应的浮盈数据
	 * 
	 * 该函数读取指定合约和用户标签的持仓明细浮盈信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_detail_profit(const char* stdCode, const char* userTag, int flag = 0) = 0;  // 纯虚函数：读取持仓明细的浮盈

	/**
	 * @brief 获取品种信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回品种信息对象指针
	 * 
	 * 该函数获取指定合约的品种信息，包括合约乘数、最小变动价位等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) = 0;  // 纯虚函数：获取品种信息

	/**
	 * @brief 获取K线数据
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param count 数据条数
	 * @param isMain 是否为主图，默认为false
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 * 
	 * 该函数获取指定合约的K线数据，支持不同周期和条数。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count, bool isMain = false) = 0;  // 纯虚函数：获取K线数据

	/**
	 * @brief 获取Tick数据
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 该函数获取指定合约的Tick数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) = 0;  // 纯虚函数：获取Tick数据

	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准合约代码
	 * @return WTSTickData* 返回最新Tick数据指针
	 * 
	 * 该函数获取指定合约的最新Tick数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) = 0;  // 纯虚函数：获取最新Tick数据

	/**
	 * @brief 获取分月合约代码
	 * @param stdCode 标准合约代码
	 * @return std::string 返回分月合约代码
	 * 
	 * 该函数获取指定合约的分月合约代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) = 0;  // 纯虚函数：获取分月合约代码

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准合约代码
	 * 
	 * 该函数订阅指定合约的Tick数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_sub_ticks(const char* stdCode) = 0;  // 纯虚函数：订阅Tick数据

	/**
	 * @brief 订阅K线事件
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * 
	 * 该函数订阅指定合约的K线事件。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_sub_bar_events(const char* stdCode, const char* period) = 0;  // 纯虚函数：订阅K线事件

	/**
	 * @brief 记录信息日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录信息级别的日志。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_log_info(const char* message) = 0;  // 纯虚函数：记录信息日志

	/**
	 * @brief 记录调试日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录调试级别的日志。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_log_debug(const char* message) = 0;  // 纯虚函数：记录调试日志

	/**
	 * @brief 记录错误日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录错误级别的日志。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_log_error(const char* message) = 0;  // 纯虚函数：记录错误日志

	/**
	 * @brief 记录警告日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录警告级别的日志。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void stra_log_warn(const char* message) {}  // 虚函数：记录警告日志，默认实现为空

	/**
	 * @brief 保存用户数据
	 * @param key 数据键
	 * @param val 数据值
	 * 
	 * 该函数保存用户自定义数据，用于策略的状态保存。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void stra_save_user_data(const char* key, const char* val){}  // 虚函数：保存用户数据，默认实现为空

	/**
	 * @brief 加载用户数据
	 * @param key 数据键
	 * @param defVal 默认值，默认为空字符串
	 * @return const char* 返回数据值，未找到返回默认值
	 * 
	 * 该函数加载用户自定义数据，用于策略的状态恢复。
	 * 默认实现返回默认值，子类可以重写此函数。
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") { return defVal; }  // 虚函数：加载用户数据，默认返回默认值

	/**
	 * @brief 设置图表K线
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * 
	 * 该函数设置图表的K线显示，用于可视化分析。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void set_chart_kline(const char* stdCode, const char* period) {}  // 虚函数：设置图表K线，默认实现为空

	/**
	 * @brief 添加信号标记
	 * @param price 价格位置
	 * @param icon 图标类型
	 * @param tag 标记标签
	 * 
	 * 该函数在图表上添加信号标记，用于可视化分析。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void add_chart_mark(double price, const char* icon, const char* tag) {}  // 虚函数：添加信号标记，默认实现为空

	/**
	 * @brief 注册指标
	 * @param idxName 指标名称
	 * @param indexType 指标类型：0-主图指标，1-副图指标
	 * 
	 * 该函数注册图表指标，用于技术分析。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void register_index(const char* idxName, uint32_t indexType) {}  // 虚函数：注册指标，默认实现为空


	/**
	 * @brief 添加指标线
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param lineType 线性，0-曲线
	 * @return bool 添加成功返回true，失败返回false
	 * 
	 * 该函数为指定指标添加指标线，用于技术分析。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool register_index_line(const char* idxName, const char* lineName, uint32_t lineType) { return false; }  // 虚函数：添加指标线，默认返回false

	/**
	 * @brief 添加基准线
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 数值
	 * @return bool 添加成功返回true，失败返回false
	 * 
	 * 该函数为指定指标添加基准线，用于技术分析。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool add_index_baseline(const char* idxName, const char* lineName, double val) { return false; }  // 虚函数：添加基准线，默认返回false

	/**
	 * @brief 设置指标值
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 指标值
	 * @return bool 设置成功返回true，失败返回false
	 * 
	 * 该函数设置指定指标的数值，用于技术分析。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool set_index_value(const char* idxName, const char* lineName, double val) { return false; }  // 虚函数：设置指标值，默认返回false

	/**
	 * @brief 获取最后的进场标记
	 * @param stdCode 合约代码
	 * @return const char* 返回最后的进场标记
	 * 
	 * 该函数获取指定合约的最后进场标记。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* stra_get_last_entertag(const char* stdCode) = 0;  // 纯虚函数：获取最后的进场标记

protected:
	std::string _name;  // 策略上下文名称，存储策略上下文的名称字符串
};

NS_WTP_END  // 结束WonderTrader命名空间