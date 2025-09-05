/*!
 * \file IHftStraCtx.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易策略上下文接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中高频交易策略上下文的核心接口，为HFT策略提供完整的执行环境。
 * 主要功能包括：
 * 1. 定义HFT策略的生命周期管理接口，包括初始化、Tick处理、委托队列、委托明细、逐笔成交等事件
 * 2. 提供完整的交易接口，支持买入、卖出、开多、开空、平多、平空等操作
 * 3. 支持多种订单类型（普通、FAK、FOK）和强平标志
 * 4. 提供市场数据访问接口，包括K线、Tick、委托明细、委托队列、逐笔成交等数据
 * 5. 支持仓位管理、价格查询、时间查询、数据订阅、日志记录、用户数据存储等功能
 * 
 * 该类主要用于WonderTrader框架中的高频交易策略系统，为HFT策略提供低延迟、高频率的交易执行环境。
 * 通过完整的接口设计，支持各种高频交易策略的开发和应用。
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含固定大小整数类型
#include <string>  // 包含字符串类型
#include "ExecuteDefs.h"  // 包含执行定义

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSCommodityInfo;  // 前向声明：WTS商品信息类
class WTSTickSlice;  // 前向声明：WTS Tick切片类
class WTSKlineSlice;  // 前向声明：WTS K线切片类
class WTSTickData;  // 前向声明：WTS Tick数据类
struct WTSBarStruct;  // 前向声明：WTS K线结构体

/**
 * @brief 订单标记常量定义
 * 
 * 定义了HFT策略中使用的订单标记类型，用于控制订单的执行方式。
 */
static const int HFT_OrderFlag_Nor = 0;  // 普通订单标记
static const int HFT_OrderFlag_FAK = 1;  // FAK订单标记（Fill and Kill）
static const int HFT_OrderFlag_FOK = 2;  // FOK订单标记（Fill or Kill）

/**
 * @class IHftStraCtx
 * @brief 高频交易策略上下文接口类
 * 
 * 该类定义了WonderTrader框架中高频交易策略上下文的核心接口，为HFT策略提供完整的执行环境。
 * 包括策略生命周期管理、交易执行、市场数据访问、仓位管理、日志记录等全方位功能。
 * 
 * 主要特性：
 * - 完整的策略生命周期管理（初始化、事件处理、会话管理）
 * - 多种交易接口（买入、卖出、开多、开空、平多、平空）
 * - 支持多种订单类型（普通、FAK、FOK）和强平标志
 * - 全面的市场数据访问（K线、Tick、委托明细、委托队列、逐笔成交）
 * - 仓位管理、价格查询、时间查询等核心功能
 * - 数据订阅、日志记录、用户数据存储等辅助功能
 * - 低延迟、高频率的交易执行环境
 */
class IHftStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param name 策略名称
	 * 
	 * 初始化HFT策略上下文对象，设置策略名称。
	 */
	IHftStraCtx(const char* name) :_name(name) {}  // 初始化策略名称

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IHftStraCtx() {}  // 虚析构函数，支持继承

	/**
	 * @brief 获取策略名称
	 * @return const char* 返回策略名称字符串
	 * 
	 * 该函数返回策略的名称，用于标识和调试。
	 */
	const char* name() const { return _name.c_str(); }  // 返回策略名称

public:
	/**
	 * @brief 获取策略ID
	 * @return uint32_t 返回策略的唯一标识ID
	 * 
	 * 该函数返回策略的唯一标识ID，用于系统内部管理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t id() = 0;  // 纯虚函数：获取策略ID

	//回调函数
	/**
	 * @brief 策略初始化回调
	 * 
	 * 该函数在策略初始化时被调用，用于策略的初始化设置。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_init() = 0;  // 纯虚函数：策略初始化回调

	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 该函数在收到新的Tick数据时被调用，用于处理实时市场数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) = 0;  // 纯虚函数：Tick数据回调

	/**
	 * @brief 委托队列数据回调
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 新的委托队列数据指针
	 * 
	 * 该函数在收到新的委托队列数据时被调用，用于处理委托队列变化。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) = 0;  // 纯虚函数：委托队列数据回调

	/**
	 * @brief 委托明细数据回调
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 新的委托明细数据指针
	 * 
	 * 该函数在收到新的委托明细数据时被调用，用于处理委托明细变化。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) = 0;  // 纯虚函数：委托明细数据回调

	/**
	 * @brief 逐笔成交数据回调
	 * @param stdCode 标准合约代码
	 * @param newTrans 新的逐笔成交数据指针
	 * 
	 * 该函数在收到新的逐笔成交数据时被调用，用于处理成交变化。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) = 0;  // 纯虚函数：逐笔成交数据回调

	/**
	 * @brief K线数据回调
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据结构指针
	 * 
	 * 该函数在K线闭合时被调用，用于处理K线数据更新。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) {}  // 虚函数：K线数据回调，默认实现为空

	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日期
	 * 
	 * 该函数在交易日开始时被调用，用于交易日开始时的初始化。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_session_begin(uint32_t uTDate) {}  // 虚函数：交易日开始回调，默认实现为空

	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日期
	 * 
	 * 该函数在交易日结束时被调用，用于交易日结束时的清理。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_session_end(uint32_t uTDate) {}  // 虚函数：交易日结束回调，默认实现为空

	/**
	 * @brief 回测结束事件回调
	 * 
	 * 该函数在回测结束时被调用，只在回测环境下才会触发。
	 * 用于回测结束后的结果分析和清理。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_bactest_end() {};  // 虚函数：回测结束事件回调，默认实现为空

	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 标准合约代码
	 * @param newTick 更新的Tick数据指针
	 * 
	 * 该函数在Tick数据更新时被调用，用于处理Tick数据的更新。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) {}  // 虚函数：Tick数据更新回调，默认实现为空

	/**
	 * @brief 委托队列更新回调
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 更新的委托队列数据指针
	 * 
	 * 该函数在委托队列更新时被调用，用于处理委托队列的更新。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue) {}  // 虚函数：委托队列更新回调，默认实现为空

	/**
	 * @brief 委托明细更新回调
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 更新的委托明细数据指针
	 * 
	 * 该函数在委托明细更新时被调用，用于处理委托明细的更新。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl) {}  // 虚函数：委托明细更新回调，默认实现为空

	/**
	 * @brief 逐笔成交更新回调
	 * @param stdCode 标准合约代码
	 * @param newTrans 更新的逐笔成交数据指针
	 * 
	 * 该函数在逐笔成交更新时被调用，用于处理逐笔成交的更新。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_trans_updated(const char* stdCode, WTSTransData* newTrans) {}  // 虚函数：逐笔成交更新回调，默认实现为空

	//策略接口
	/**
	 * @brief 撤销指定本地ID的订单
	 * @param localid 本地订单ID
	 * @return bool 撤销成功返回true，失败返回false
	 * 
	 * 该函数撤销指定本地ID的订单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		stra_cancel(uint32_t localid) = 0;  // 纯虚函数：撤销指定本地ID的订单

	/**
	 * @brief 撤销指定合约和方向的订单
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向，true为买入，false为卖出
	 * @param qty 撤销数量
	 * @return OrderIDs 返回撤销的订单ID数组
	 * 
	 * 该函数撤销指定合约和方向的订单，支持指定数量。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderIDs	stra_cancel(const char* stdCode, bool isBuy, double qty) = 0;  // 纯虚函数：撤销指定合约和方向的订单

	/**
	 * @brief 买入下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通，1-FAK，2-FOK，默认为0
	 * @param bForceClose 强平标志，如果为true，则强制优先平仓
	 * @return OrderIDs 返回订单ID数组
	 * 
	 * 该函数执行买入下单操作，支持多种订单类型和强平标志。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderIDs	stra_buy(const char* stdCode, double price, double qty, const char* userTag, int flag = 0, bool bForceClose = false) = 0;  // 纯虚函数：买入下单接口

	/**
	 * @brief 卖出下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通，1-FAK，2-FOK，默认为0
	 * @param bForceClose 强平标志，如果为true，则强制优先平仓
	 * @return OrderIDs 返回订单ID数组
	 * 
	 * 该函数执行卖出下单操作，支持多种订单类型和强平标志。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderIDs	stra_sell(const char* stdCode, double price, double qty, const char* userTag, int flag = 0, bool bForceClose = false) = 0;  // 纯虚函数：卖出下单接口

	/**
	 * @brief 开多下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通，1-FAK，2-FOK
	 * @return uint32_t 返回订单ID
	 * 
	 * 该函数执行开多下单操作，支持多种订单类型。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t	stra_enter_long(const char* stdCode, double price, double qty, const char* userTag, int flag = 0) { return 0; }  // 虚函数：开多下单接口，默认返回0

	/**
	 * @brief 开空下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通，1-FAK，2-FOK
	 * @return uint32_t 返回订单ID
	 * 
	 * 该函数执行开空下单操作，支持多种订单类型。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t	stra_enter_short(const char* stdCode, double price, double qty, const char* userTag, int flag = 0) { return 0; }  // 虚函数：开空下单接口，默认返回0

	/**
	 * @brief 平多下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param isToday 是否今仓，默认为false
	 * @param flag 下单标志：0-普通，1-FAK，2-FOK，默认为0
	 * @return uint32_t 返回订单ID
	 * 
	 * 该函数执行平多下单操作，支持多种订单类型和今仓标识。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t	stra_exit_long(const char* stdCode, double price, double qty, const char* userTag, bool isToday = false, int flag = 0) { return 0; }  // 虚函数：平多下单接口，默认返回0

	/**
	 * @brief 平空下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param isToday 是否今仓，默认为false
	 * @param flag 下单标志：0-普通，1-FAK，2-FOK，默认为0
	 * @return uint32_t 返回订单ID
	 * 
	 * 该函数执行平空下单操作，支持多种订单类型和今仓标识。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t	stra_exit_short(const char* stdCode, double price, double qty, const char* userTag, bool isToday = false, int flag = 0) { return 0; }  // 虚函数：平空下单接口，默认返回0

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回商品信息指针
	 * 
	 * 该函数获取指定合约的商品信息，包括商品代码、交易所等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) = 0;  // 纯虚函数：获取商品信息

	/**
	 * @brief 获取K线数据
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param count 数据条数
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 * 
	 * 该函数获取指定合约的K线数据，支持不同周期和条数。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count) = 0;  // 纯虚函数：获取K线数据

	/**
	 * @brief 获取Tick数据
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 该函数获取指定合约的Tick数据，支持指定条数。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) = 0;  // 纯虚函数：获取Tick数据

	/**
	 * @brief 获取委托明细数据
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针
	 * 
	 * 该函数获取指定合约的委托明细数据，支持指定条数。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSOrdDtlSlice*	stra_get_order_detail(const char* stdCode, uint32_t count) = 0;  // 纯虚函数：获取委托明细数据

	/**
	 * @brief 获取委托队列数据
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
	 * 
	 * 该函数获取指定合约的委托队列数据，支持指定条数。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSOrdQueSlice*	stra_get_order_queue(const char* stdCode, uint32_t count) = 0;  // 纯虚函数：获取委托队列数据

	/**
	 * @brief 获取逐笔成交数据
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @return WTSTransSlice* 返回逐笔成交数据切片指针
	 * 
	 * 该函数获取指定合约的逐笔成交数据，支持指定条数。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTransSlice*	stra_get_transaction(const char* stdCode, uint32_t count) = 0;  // 纯虚函数：获取逐笔成交数据

	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准合约代码
	 * @return WTSTickData* 返回最新Tick数据指针
	 * 
	 * 该函数获取指定合约的最新Tick数据，用于实时市场数据访问。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) = 0;  // 纯虚函数：获取最新Tick数据

	/**
	 * @brief 获取分月合约代码
	 * @param stdCode 标准合约代码
	 * @return std::string 返回分月合约代码
	 * 
	 * 该函数获取指定合约的分月合约代码，用于合约代码的转换。
	 * 纯虚函数，子类必须实现。
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) = 0;  // 纯虚函数：获取分月合约代码

	/**
	 * @brief 获取仓位
	 * @param stdCode 标准合约代码
	 * @param bOnlyValid 是否只返回有效仓位，默认为false
	 * @param flag 操作标记：1-多仓，2-空仓，3-多空轧平，默认为3
	 * @return double 返回仓位数量：多仓>0，空仓<0
	 * 
	 * 该函数获取指定合约的仓位信息，支持多空仓位轧平计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, int flag = 3) = 0;  // 纯虚函数：获取仓位

	/**
	 * @brief 获取仓位平均价格
	 * @param stdCode 标准合约代码
	 * @return double 返回仓位平均价格
	 * 
	 * 该函数获取指定合约的仓位平均价格，用于盈亏计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) = 0;  // 纯虚函数：获取仓位平均价格

	/**
	 * @brief 获取仓位盈亏
	 * @param stdCode 标准合约代码
	 * @return double 返回仓位盈亏金额
	 * 
	 * 该函数获取指定合约的仓位盈亏，用于风险监控。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_position_profit(const char* stdCode) = 0;  // 纯虚函数：获取仓位盈亏

	/**
	 * @brief 获取最新价格
	 * @param stdCode 标准合约代码
	 * @return double 返回最新价格
	 * 
	 * 该函数获取指定合约的最新价格，用于价格查询。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_price(const char* stdCode) = 0;  // 纯虚函数：获取最新价格

	/**
	 * @brief 获取未完成数量
	 * @param stdCode 标准合约代码
	 * @return double 返回买卖轧平以后的未完成数量
	 * 
	 * 该函数获取指定合约的未完成数量，自动进行买卖方向的轧平计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double stra_get_undone(const char* stdCode) = 0;  // 纯虚函数：获取未完成数量

	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期，格式为YYYYMMDD
	 * 
	 * 该函数获取当前日期，用于时间相关的判断和处理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t stra_get_date() = 0;  // 纯虚函数：获取当前日期

	/**
	 * @brief 获取当前时间
	 * @return uint32_t 返回当前时间，格式为HHMM
	 * 
	 * 该函数获取当前时间，用于时间相关的判断和处理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t stra_get_time() = 0;  // 纯虚函数：获取当前时间

	/**
	 * @brief 获取当前秒数
	 * @return uint32_t 返回当前秒数，精确到毫秒
	 * 
	 * 该函数获取当前秒数，用于精确的时间计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t stra_get_secs() = 0;  // 纯虚函数：获取当前秒数

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准合约代码
	 * 
	 * 该函数订阅指定合约的Tick数据，用于接收实时市场数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_sub_ticks(const char* stdCode) = 0;  // 纯虚函数：订阅Tick数据

	/**
	 * @brief 订阅委托队列数据
	 * @param stdCode 标准合约代码
	 * 
	 * 该函数订阅指定合约的委托队列数据，用于接收委托队列变化。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_sub_order_queues(const char* stdCode) = 0;  // 纯虚函数：订阅委托队列数据

	/**
	 * @brief 订阅委托明细数据
	 * @param stdCode 标准合约代码
	 * 
	 * 该函数订阅指定合约的委托明细数据，用于接收委托明细变化。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_sub_order_details(const char* stdCode) = 0;  // 纯虚函数：订阅委托明细数据

	/**
	 * @brief 订阅逐笔成交数据
	 * @param stdCode 标准合约代码
	 * 
	 * 该函数订阅指定合约的逐笔成交数据，用于接收成交变化。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_sub_transactions(const char* stdCode) = 0;  // 纯虚函数：订阅逐笔成交数据

	/**
	 * @brief 记录信息日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录信息级别的日志，用于策略运行信息的记录。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_log_info(const char* message) = 0;  // 纯虚函数：记录信息日志

	/**
	 * @brief 记录调试日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录调试级别的日志，用于策略调试信息的记录。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_log_debug(const char* message) = 0;  // 纯虚函数：记录调试日志

	/**
	 * @brief 记录错误日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录错误级别的日志，用于策略错误信息的记录。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void stra_log_error(const char* message) = 0;  // 纯虚函数：记录错误日志

	/**
	 * @brief 记录警告日志
	 * @param message 日志消息内容
	 * 
	 * 该函数记录警告级别的日志，用于策略警告信息的记录。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void stra_log_warn(const char* message) {}  // 虚函数：记录警告日志，默认实现为空

	/**
	 * @brief 保存用户数据
	 * @param key 数据键
	 * @param val 数据值
	 * 
	 * 该函数保存用户自定义数据，用于策略数据的持久化存储。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void stra_save_user_data(const char* key, const char* val) {}  // 虚函数：保存用户数据，默认实现为空

	/**
	 * @brief 加载用户数据
	 * @param key 数据键
	 * @param defVal 默认值，默认为空字符串
	 * @return const char* 返回数据值，如果不存在则返回默认值
	 * 
	 * 该函数加载用户自定义数据，用于策略数据的读取。
	 * 默认实现返回默认值，子类可以重写此函数。
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") { return defVal; }  // 虚函数：加载用户数据，默认返回默认值

protected:
	std::string _name;  // 策略名称
};

NS_WTP_END  // 结束WonderTrader命名空间