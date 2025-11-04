/*!
 * \file HftStraBaseCtx.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 高频交易策略基础上下文头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中高频交易策略的基础上下文类，为HFT策略提供完整的运行环境。
 * 
 * 主要功能包括：
 * 1. 策略生命周期管理：初始化、交易日开始/结束、各种市场数据回调
 * 2. 交易执行接口：买入、卖出、开多、开空、平多、平空、撤单等完整交易功能
 * 3. 市场数据访问：K线、Tick、委托明细、委托队列、逐笔成交等数据获取
 * 4. 仓位管理：持仓查询、持仓盈亏、平均持仓价格、未成交数量等
 * 5. 数据订阅管理：订阅Tick、委托明细、委托队列、逐笔成交数据
 * 6. 日志记录：支持info、debug、warn、error级别的日志
 * 7. 用户数据存储：策略自定义数据的保存和加载
 * 8. 交易通知处理：成交、订单、通道状态、持仓变化等通知处理
 * 9. 持仓管理：详细的持仓明细管理，支持开仓、平仓、盈亏计算
 * 
 * 设计特点：
 * - 同时继承IHftStraCtx和ITrdNotifySink接口，实现策略上下文和交易通知接收
 * - 支持合约代码映射（标准代码到实际代码的转换）
 * - 支持滑点模拟（用于回测）
 * - 支持数据托管模式（自动记录交易日志）
 * - 使用循环缓冲区管理订单标签，提高性能
 * - 详细的持仓明细管理，支持多笔开仓和平仓的精确匹配
 */
#pragma once  // 防止头文件重复包含

#include "../Includes/FasterDefs.h"  // 包含快速定义的头文件
#include "../Includes/IHftStraCtx.h"  // 包含高频交易策略上下文接口定义
#include "../Share/BoostFile.hpp"  // 包含Boost文件操作工具类
#include "../Share/fmtlib.h"  // 包含格式化库

#include <boost/circular_buffer.hpp>  // 包含boost循环缓冲区类，用于高效的订单标签管理

#include "ITrdNotifySink.h"  // 包含交易通知接收接口定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

class WtHftEngine;  // 前向声明：高频交易引擎类
class TraderAdapter;  // 前向声明：交易适配器类

/**
 * @class HftStraBaseCtx
 * @brief 高频交易策略基础上下文类
 * 
 * 该类实现了高频交易策略的基础上下文功能，为HFT策略提供完整的运行环境。
 * 同时继承IHftStraCtx和ITrdNotifySink接口，实现策略上下文和交易通知接收的双重功能。
 * 
 * 主要特性：
 * - 完整的策略生命周期管理
 * - 多种交易接口（买入、卖出、开多、开空、平多、平空）
 * - 全面的市场数据访问（K线、Tick、委托明细、委托队列、逐笔成交）
 * - 详细的持仓管理和盈亏计算
 * - 数据订阅管理和日志记录
 * - 用户数据存储和合约代码映射
 * - 支持滑点模拟和数据托管模式
 */
class HftStraBaseCtx : public IHftStraCtx, public ITrdNotifySink  // 继承策略上下文接口和交易通知接收接口
{
public:
	/**
	 * @brief 构造函数
	 * @param engine 高频交易引擎指针
	 * @param name 策略名称
	 * @param bAgent 是否启用数据托管模式
	 * @param slippage 滑点点数（用于回测）
	 * 
	 * 初始化高频交易策略基础上下文对象。
	 * 数据托管模式启用时，会自动记录交易日志到文件。
	 */
	HftStraBaseCtx(WtHftEngine* engine, const char* name, bool bAgent, int32_t slippage);  // 构造函数声明
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理策略上下文对象，释放所有资源。
	 */
	virtual ~HftStraBaseCtx();  // 析构函数声明

	/**
	 * @brief 设置交易适配器
	 * @param trader 交易适配器指针
	 * 
	 * 设置策略使用的交易适配器，用于执行交易操作。
	 */
	void setTrader(TraderAdapter* trader);  // 设置交易适配器函数声明

public:
	//////////////////////////////////////////////////////////////////////////
	//IHftStraCtx 接口实现
	/**
	 * @brief 获取策略上下文ID
	 * @return uint32_t 返回策略上下文的唯一标识ID
	 * 
	 * 返回策略上下文的唯一标识ID，用于系统内部识别。
	 */
	virtual uint32_t id() override;  // 获取上下文ID函数声明

	/**
	 * @brief 策略初始化回调
	 * 
	 * 策略初始化时被调用，执行初始化准备工作，包括初始化输出文件和加载用户数据。
	 */
	virtual void on_init() override;  // 策略初始化回调函数声明

	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当收到新的Tick数据时被调用，处理实时市场数据。
	 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;  // Tick数据回调函数声明

	/**
	 * @brief 委托队列数据回调
	 * @param stdCode 标准合约代码
	 * @param newOrdQue 新的委托队列数据指针
	 * 
	 * 当收到新的委托队列数据时被调用，处理委托队列变化。
	 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) override;  // 委托队列数据回调函数声明

	/**
	 * @brief 委托明细数据回调
	 * @param stdCode 标准合约代码
	 * @param newOrdDtl 新的委托明细数据指针
	 * 
	 * 当收到新的委托明细数据时被调用，处理委托明细变化。
	 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;  // 委托明细数据回调函数声明

	/**
	 * @brief 逐笔成交数据回调
	 * @param stdCode 标准合约代码
	 * @param newTrans 新的逐笔成交数据指针
	 * 
	 * 当收到新的逐笔成交数据时被调用，处理逐笔成交变化。
	 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) override;  // 逐笔成交数据回调函数声明

	/**
	 * @brief K线数据回调
	 * @param stdCode 标准合约代码
	 * @param period 周期（如"m1"、"m5"等）
	 * @param times 周期倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 当收到新的K线数据时被调用，处理K线闭合事件。
	 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;  // K线数据回调函数声明

	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期
	 * 
	 * 当交易日开始时被调用，执行交易日开始前的准备工作。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;  // 交易日开始回调函数声明

	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期
	 * 
	 * 当交易日结束时被调用，执行交易日结束后的清理工作，包括记录资金日志。
	 */
	virtual void on_session_end(uint32_t uTDate) override;  // 交易日结束回调函数声明

	/**
	 * @brief 撤销指定订单
	 * @param localid 本地订单ID
	 * @return bool 撤销成功返回true，失败返回false
	 * 
	 * 根据本地订单ID撤销指定的订单。
	 */
	virtual bool stra_cancel(uint32_t localid) override;  // 撤销指定订单函数声明

	/**
	 * @brief 撤销指定合约的订单
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向
	 * @param qty 撤销数量
	 * @return OrderIDs 返回被撤销的订单ID列表
	 * 
	 * 撤销指定合约、指定方向、指定数量的订单。
	 * 会先进行撤单频率检查。
	 */
	virtual OrderIDs stra_cancel(const char* stdCode, bool isBuy, double qty) override;  // 撤销指定合约订单函数声明

	/**
	 * @brief 买入下单接口
	 * @param stdCode 标准合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
	 * @param bForceClose 是否强平，默认false
	 * @return OrderIDs 返回订单ID列表
	 * 
	 * 执行买入操作，支持限价单和市价单，支持FAK和FOK订单类型。
	 * 支持合约代码映射（标准代码到实际代码的转换）。
	 * 会进行下单限制检查。
	 */
	virtual OrderIDs stra_buy(const char* stdCode, double price, double qty, const char* userTag, int flag = 0, bool bForceClose = false) override;  // 买入下单接口函数声明

	/**
	 * @brief 卖出下单接口
	 * @param stdCode 标准合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
	 * @param bForceClose 是否强平，默认false
	 * @return OrderIDs 返回订单ID列表
	 * 
	 * 执行卖出操作，支持限价单和市价单，支持FAK和FOK订单类型。
	 * 对于不能做空的品种，会检查可用持仓是否足够。
	 * 支持合约代码映射。
	 * 会进行下单限制检查。
	 */
	virtual OrderIDs stra_sell(const char* stdCode, double price, double qty, const char* userTag, int flag = 0, bool bForceClose = false) override;  // 卖出下单接口函数声明

	/**
	 * @brief 开多下单接口
	 * @param stdCode 标准合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
	 * @return uint32_t 返回订单ID
	 * 
	 * 执行开多操作，支持合约代码映射。
	 */
	virtual uint32_t	stra_enter_long(const char* stdCode, double price, double qty, const char* userTag, int flag = 0) override;  // 开多下单接口函数声明

	/**
	 * @brief 开空下单接口
	 * @param stdCode 标准合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
	 * @return uint32_t 返回订单ID
	 * 
	 * 执行开空操作，支持合约代码映射。
	 */
	virtual uint32_t	stra_enter_short(const char* stdCode, double price, double qty, const char* userTag, int flag = 0) override;  // 开空下单接口函数声明

	/**
	 * @brief 平多下单接口
	 * @param stdCode 标准合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param isToday 是否今仓，默认false
	 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
	 * @return uint32_t 返回订单ID
	 * 
	 * 执行平多操作，支持指定今仓或昨仓，支持合约代码映射。
	 */
	virtual uint32_t	stra_exit_long(const char* stdCode, double price, double qty, const char* userTag, bool isToday = false, int flag = 0) override;  // 平多下单接口函数声明

	/**
	 * @brief 平空下单接口
	 * @param stdCode 标准合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param userTag 用户标签
	 * @param isToday 是否今仓，默认false
	 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
	 * @return uint32_t 返回订单ID
	 * 
	 * 执行平空操作，支持指定今仓或昨仓，支持合约代码映射。
	 */
	virtual uint32_t	stra_exit_short(const char* stdCode, double price, double qty, const char* userTag, bool isToday = false, int flag = 0) override;  // 平空下单接口函数声明

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回商品信息对象指针
	 * 
	 * 根据标准合约代码获取商品信息，包括交易规则、手续费等。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;  // 获取商品信息函数声明

	/**
	 * @brief 获取K线数据
	 * @param stdCode 标准合约代码
	 * @param period 周期（如"m1"、"m5"等）
	 * @param count 获取的K线数量
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 * 
	 * 获取指定合约的K线数据切片。
	 * 获取成功后会自动订阅该合约的Tick数据。
	 */
	virtual WTSKlineSlice* stra_get_bars(const char* stdCode, const char* period, uint32_t count) override;  // 获取K线数据函数声明

	/**
	 * @brief 获取Tick数据
	 * @param stdCode 标准合约代码
	 * @param count 获取的Tick数量
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 获取指定合约的Tick数据切片。
	 * 获取成功后会自动订阅该合约的Tick数据。
	 */
	virtual WTSTickSlice* stra_get_ticks(const char* stdCode, uint32_t count) override;  // 获取Tick数据函数声明

	/**
	 * @brief 获取委托明细数据
	 * @param stdCode 标准合约代码
	 * @param count 获取的委托明细数量
	 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针
	 * 
	 * 获取指定合约的委托明细数据切片。
	 * 获取成功后会自动订阅该合约的委托明细数据。
	 */
	virtual WTSOrdDtlSlice*	stra_get_order_detail(const char* stdCode, uint32_t count) override;  // 获取委托明细数据函数声明

	/**
	 * @brief 获取委托队列数据
	 * @param stdCode 标准合约代码
	 * @param count 获取的委托队列数量
	 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
	 * 
	 * 获取指定合约的委托队列数据切片。
	 * 获取成功后会自动订阅该合约的委托队列数据。
	 */
	virtual WTSOrdQueSlice*	stra_get_order_queue(const char* stdCode, uint32_t count) override;  // 获取委托队列数据函数声明

	/**
	 * @brief 获取逐笔成交数据
	 * @param stdCode 标准合约代码
	 * @param count 获取的逐笔成交数量
	 * @return WTSTransSlice* 返回逐笔成交数据切片指针
	 * 
	 * 获取指定合约的逐笔成交数据切片。
	 * 获取成功后会自动订阅该合约的逐笔成交数据。
	 */
	virtual WTSTransSlice*	stra_get_transaction(const char* stdCode, uint32_t count) override;  // 获取逐笔成交数据函数声明

	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准合约代码
	 * @return WTSTickData* 返回最新Tick数据指针
	 * 
	 * 获取指定合约的最新Tick数据。
	 */
	virtual WTSTickData* stra_get_last_tick(const char* stdCode) override;  // 获取最新Tick数据函数声明

	/**
	 * @brief 获取分月合约代码
	 * @param stdCode 标准合约代码
	 * @return std::string 返回分月合约代码字符串
	 * 
	 * 根据标准合约代码获取对应的分月合约代码。
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) override;  // 获取分月合约代码函数声明

	/**
	 * @brief 记录信息级别日志
	 * @param message 日志消息内容
	 * 
	 * 记录信息级别的日志消息。
	 */
	virtual void stra_log_info(const char* message) override;  // 记录信息日志函数声明
	/**
	 * @brief 记录调试级别日志
	 * @param message 日志消息内容
	 * 
	 * 记录调试级别的日志消息。
	 */
	virtual void stra_log_debug(const char* message) override;  // 记录调试日志函数声明
	/**
	 * @brief 记录警告级别日志
	 * @param message 日志消息内容
	 * 
	 * 记录警告级别的日志消息。
	 */
	virtual void stra_log_warn(const char* message) override;  // 记录警告日志函数声明
	/**
	 * @brief 记录错误级别日志
	 * @param message 日志消息内容
	 * 
	 * 记录错误级别的日志消息。
	 */
	virtual void stra_log_error(const char* message) override;  // 记录错误日志函数声明

	/**
	 * @brief 获取持仓数量
	 * @param stdCode 标准合约代码
	 * @param bOnlyValid 是否只查询可用持仓，默认false
	 * @param flag 持仓标志，默认3（全部）
	 * @return double 返回持仓数量
	 * 
	 * 获取指定合约的持仓数量。
	 * 支持合约代码映射。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, int flag = 3) override;  // 获取持仓数量函数声明
	/**
	 * @brief 获取持仓平均价格
	 * @param stdCode 标准合约代码
	 * @return double 返回持仓平均价格
	 * 
	 * 根据持仓明细计算持仓平均价格。
	 * 如果没有持仓，返回0。
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) override;  // 获取持仓平均价格函数声明
	/**
	 * @brief 获取持仓盈亏
	 * @param stdCode 标准合约代码
	 * @return double 返回持仓盈亏金额
	 * 
	 * 获取指定合约的持仓浮动盈亏。
	 */
	virtual double stra_get_position_profit(const char* stdCode) override;  // 获取持仓盈亏函数声明
	/**
	 * @brief 获取最新价格
	 * @param stdCode 标准合约代码
	 * @return double 返回最新价格
	 * 
	 * 获取指定合约的最新价格。
	 * 先从价格映射表中查找，如果没有则从引擎获取。
	 */
	virtual double stra_get_price(const char* stdCode) override;  // 获取最新价格函数声明
	/**
	 * @brief 获取未成交数量
	 * @param stdCode 标准合约代码
	 * @return double 返回未成交数量
	 * 
	 * 获取指定合约的未成交订单数量。
	 * 支持合约代码映射。
	 */
	virtual double stra_get_undone(const char* stdCode) override;  // 获取未成交数量函数声明

	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
	 * 
	 * 获取当前交易日日期。
	 */
	virtual uint32_t stra_get_date() override;  // 获取当前日期函数声明
	/**
	 * @brief 获取当前时间
	 * @return uint32_t 返回当前时间（格式：HHMMSS）
	 * 
	 * 获取当前交易时间。
	 */
	virtual uint32_t stra_get_time() override;  // 获取当前时间函数声明
	/**
	 * @brief 获取当前秒数
	 * @return uint32_t 返回当前秒数（0-59）
	 * 
	 * 获取当前时间的秒数部分。
	 */
	virtual uint32_t stra_get_secs() override;  // 获取当前秒数函数声明

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准合约代码
	 * 
	 * 主动订阅指定合约的Tick数据。
	 * 订阅后会在本地记录，并在回调时检查。
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;  // 订阅Tick数据函数声明
	/**
	 * @brief 订阅委托明细数据
	 * @param stdCode 标准合约代码
	 * 
	 * 主动订阅指定合约的委托明细数据。
	 */
	virtual void stra_sub_order_details(const char* stdCode) override;  // 订阅委托明细数据函数声明
	/**
	 * @brief 订阅委托队列数据
	 * @param stdCode 标准合约代码
	 * 
	 * 主动订阅指定合约的委托队列数据。
	 */
	virtual void stra_sub_order_queues(const char* stdCode) override;  // 订阅委托队列数据函数声明
	/**
	 * @brief 订阅逐笔成交数据
	 * @param stdCode 标准合约代码
	 * 
	 * 主动订阅指定合约的逐笔成交数据。
	 */
	virtual void stra_sub_transactions(const char* stdCode) override;  // 订阅逐笔成交数据函数声明

	/**
	 * @brief 保存用户数据
	 * @param key 数据键
	 * @param val 数据值
	 * 
	 * 保存策略自定义数据，数据会在适当时机自动持久化到文件。
	 */
	virtual void stra_save_user_data(const char* key, const char* val) override;  // 保存用户数据函数声明

	/**
	 * @brief 加载用户数据
	 * @param key 数据键
	 * @param defVal 默认值，如果数据不存在则返回此值
	 * @return const char* 返回数据值字符串
	 * 
	 * 加载策略自定义数据，如果数据不存在则返回默认值。
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") override;  // 加载用户数据函数声明

	//////////////////////////////////////////////////////////////////////////
	//ITrdNotifySink 接口实现
	/**
	 * @brief 成交通知回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 当订单成交时被调用，更新持仓信息并记录成交日志。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;  // 成交通知回调函数声明

	/**
	 * @brief 订单状态通知回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买入方向
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销
	 * 
	 * 当订单状态发生变化时被调用，处理订单状态更新。
	 * 如果订单已撤销或全部成交，可以清理订单标签。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled) override;  // 订单状态通知回调函数声明

	/**
	 * @brief 交易通道就绪通知回调
	 * 
	 * 当交易通道就绪时被调用，执行通道就绪后的准备工作。
	 */
	virtual void on_channel_ready() override;  // 交易通道就绪通知回调函数声明

	/**
	 * @brief 交易通道丢失通知回调
	 * 
	 * 当交易通道丢失时被调用，执行通道丢失后的清理工作。
	 */
	virtual void on_channel_lost() override;  // 交易通道丢失通知回调函数声明

	/**
	 * @brief 委托通知回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息内容
	 * 
	 * 当委托结果返回时被调用，处理委托成功或失败的情况。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;  // 委托通知回调函数声明

	/**
	 * @brief 持仓变化通知回调
	 * @param stdCode 标准合约代码
	 * @param isLong 是否为做多方向
	 * @param prevol 变化前持仓数量
	 * @param preavail 变化前可用持仓数量
	 * @param newvol 变化后持仓数量
	 * @param newavail 变化后可用持仓数量
	 * @param tradingday 交易日
	 * 
	 * 当持仓发生变化时被调用，处理持仓变化通知。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;  // 持仓变化通知回调函数声明

protected:
	/**
	 * @brief 格式化调试日志
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 参数列表
	 * 
	 * 模板函数，支持格式化字符串的调试日志记录。
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)  // 格式化调试日志模板函数
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_debug(buffer);  // 调用调试日志记录函数
	}

	/**
	 * @brief 格式化信息日志
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 参数列表
	 * 
	 * 模板函数，支持格式化字符串的信息日志记录。
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)  // 格式化信息日志模板函数
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_info(buffer);  // 调用信息日志记录函数
	}

	/**
	 * @brief 格式化错误日志
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 参数列表
	 * 
	 * 模板函数，支持格式化字符串的错误日志记录。
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)  // 格式化错误日志模板函数
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_error(buffer);  // 调用错误日志记录函数
	}

protected:
	/**
	 * @brief 获取内部合约代码
	 * @param stdCode 标准合约代码
	 * @return const char* 返回内部合约代码，如果不存在映射则返回原代码
	 * 
	 * 根据代码映射表获取内部合约代码。
	 * 用于处理标准代码到实际代码的转换。
	 */
	const char* get_inner_code(const char* stdCode);  // 获取内部合约代码函数声明

	/**
	 * @brief 加载用户数据
	 * 
	 * 从文件加载策略的用户自定义数据。
	 */
	void	load_userdata();  // 加载用户数据函数声明
	/**
	 * @brief 保存用户数据
	 * 
	 * 将策略的用户自定义数据保存到文件。
	 */
	void	save_userdata();  // 保存用户数据函数声明

	/**
	 * @brief 初始化输出文件
	 * 
	 * 在数据托管模式下，初始化各种日志输出文件：
	 * - 交易日志（trades.csv）
	 * - 平仓日志（closes.csv）
	 * - 资金日志（funds.csv）
	 * - 信号日志（signals.csv）
	 */
	void	init_outputs();  // 初始化输出文件函数声明

	/**
	 * @brief 设置目标持仓
	 * @param stdCode 标准合约代码
	 * @param qty 目标持仓数量
	 * @param price 成交价格，0表示使用当前价格
	 * @param userTag 用户标签
	 * 
	 * 设置指定合约的目标持仓数量。
	 * 如果目标持仓与当前持仓不同，会自动计算开仓或平仓操作。
	 * 支持滑点模拟和详细的持仓明细管理。
	 */
	void	do_set_position(const char* stdCode, double qty, double price = 0.0, const char* userTag = "");  // 设置目标持仓函数声明
	/**
	 * @brief 更新浮动盈亏
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 根据最新的Tick数据更新持仓的浮动盈亏。
	 * 同时更新每笔持仓明细的最大盈利和最大亏损。
	 */
	void	update_dyn_profit(const char* stdCode, WTSTickData* newTick);  // 更新浮动盈亏函数声明

	/**
	 * @brief 记录成交日志
	 * @param stdCode 标准合约代码
	 * @param isLong 是否做多
	 * @param isOpen 是否开仓
	 * @param curTime 当前时间戳
	 * @param price 成交价格
	 * @param qty 成交数量
	 * @param fee 手续费
	 * @param userTag 用户标签
	 * 
	 * 在数据托管模式下，记录成交日志到文件。
	 */
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, double fee, const char* userTag);  // 记录成交日志内联函数声明
	/**
	 * @brief 记录平仓日志
	 * @param stdCode 标准合约代码
	 * @param isLong 是否做多
	 * @param openTime 开仓时间戳
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间戳
	 * @param closepx 平仓价格
	 * @param qty 平仓数量
	 * @param profit 盈亏金额
	 * @param maxprofit 最大盈利
	 * @param maxloss 最大亏损
	 * @param totalprofit 累计盈亏
	 * @param enterTag 开仓标签
	 * @param exitTag 平仓标签
	 * 
	 * 在数据托管模式下，记录平仓日志到文件。
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double maxprofit, double maxloss, double totalprofit, const char* enterTag, const char* exitTag);  // 记录平仓日志内联函数声明

	/**
	 * @brief 获取订单标签
	 * @param localid 本地订单ID
	 * @return const char* 返回订单标签字符串，如果不存在返回空字符串
	 * 
	 * 根据本地订单ID从订单标签缓冲区中查找对应的用户标签。
	 * 使用二分查找提高查找效率。
	 */
	inline const char* getOrderTag(uint32_t localid)  // 获取订单标签内联函数实现
	{
		thread_local static OrderTag oTag;  // 线程局部静态变量，用于查找
		oTag._localid = localid;  // 设置查找键
		auto it = std::lower_bound(_orders.begin(), _orders.end(), oTag, [](const OrderTag& a, const OrderTag& b) {  // 使用二分查找定位
			return a._localid < b._localid;  // 按订单ID排序
		});  // 二分查找结束

		if (it == _orders.end())  // 如果未找到
			return "";  // 返回空字符串

		return (*it)._usertag;  // 返回找到的订单标签
	}


	/**
	 * @brief 设置订单标签
	 * @param localid 本地订单ID
	 * @param usertag 用户标签字符串
	 * 
	 * 将订单ID和用户标签添加到订单标签缓冲区中。
	 */
	inline void setUserTag(uint32_t localid, const char* usertag)  // 设置订单标签内联函数实现
	{
		_orders.push_back({ localid, usertag });  // 将订单标签添加到循环缓冲区末尾
	}

	/**
	 * @brief 删除订单标签
	 * @param localid 本地订单ID
	 * 
	 * 从订单标签缓冲区中删除指定的订单标签。
	 * 使用二分查找定位要删除的元素。
	 */
	inline void eraseOrderTag(uint32_t localid)  // 删除订单标签内联函数实现
	{
		thread_local static OrderTag oTag;  // 线程局部静态变量，用于查找
		oTag._localid = localid;  // 设置查找键
		auto it = std::lower_bound(_orders.begin(), _orders.end(), oTag, [](const OrderTag& a, const OrderTag& b) {  // 使用二分查找定位
			return a._localid < b._localid;  // 按订单ID排序
		});  // 二分查找结束

		if (it == _orders.end())  // 如果未找到
			return;  // 直接返回

		_orders.erase(it);  // 删除找到的元素
	}

protected:
	uint32_t		_context_id;  // 策略上下文唯一标识ID
	WtHftEngine*	_engine;  // 高频交易引擎指针
	TraderAdapter*	_trader;  // 交易适配器指针
	int32_t			_slippage;  // 滑点点数（用于回测）

	wt_hashmap<std::string, std::string> _code_map;  // 合约代码映射表：标准代码 -> 实际代码

	BoostFilePtr	_sig_logs;  // 信号日志文件指针（数据托管模式）
	BoostFilePtr	_close_logs;  // 平仓日志文件指针（数据托管模式）
	BoostFilePtr	_trade_logs;  // 交易日志文件指针（数据托管模式）
	BoostFilePtr	_fund_logs;  // 资金日志文件指针（数据托管模式）

	//用户数据
	/**
	 * @typedef StringHashMap
	 * @brief 字符串哈希映射表类型别名
	 * 
	 * 定义字符串到字符串的哈希映射表类型，用于存储用户自定义数据。
	 */
	typedef wt_hashmap<std::string, std::string> StringHashMap;  // 定义字符串哈希映射表类型
	StringHashMap	_user_datas;  // 用户自定义数据映射表
	bool			_ud_modified;  // 用户数据是否已修改标志

	bool			_data_agent;	//数据托管标志，true表示启用数据托管模式，会自动记录交易日志

	//tick订阅列表
	wt_hashset<std::string> _tick_subs;  // Tick数据订阅列表，记录主动订阅的合约代码

private:
	/**
	 * @struct _DetailInfo
	 * @brief 持仓明细信息结构体
	 * 
	 * 存储每笔持仓的详细信息，包括：
	 * - 持仓方向（做多/做空）
	 * - 开仓价格和数量
	 * - 开仓时间和日期
	 * - 最大盈利和最大亏损
	 * - 当前盈亏
	 * - 用户标签
	 */
	typedef struct _DetailInfo
	{
		bool		_long;  // 是否做多方向（true-做多，false-做空）
		double		_price;  // 开仓价格
		double		_volume;  // 持仓数量
		uint64_t	_opentime;  // 开仓时间戳
		uint32_t	_opentdate;  // 开仓交易日
		double		_max_profit;  // 最大盈利金额
		double		_max_loss;  // 最大亏损金额
		double		_profit;  // 当前盈亏金额
		char		_usertag[32];  // 用户标签字符串（最大32字节）

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓明细信息结构体，将所有字段清零。
		 */
		_DetailInfo()
		{
			memset(this, 0, sizeof(_DetailInfo));  // 将整个结构体清零
		}
	} DetailInfo;  // 持仓明细信息结构体类型定义

	/**
	 * @struct _PosInfo
	 * @brief 持仓信息结构体
	 * 
	 * 存储每个合约的持仓信息，包括：
	 * - 总持仓数量
	 * - 累计平仓盈亏
	 * - 当前浮动盈亏
	 * - 持仓明细列表
	 */
	typedef struct _PosInfo
	{
		double		_volume;  // 总持仓数量（正数表示做多，负数表示做空）
		double		_closeprofit;  // 累计平仓盈亏
		double		_dynprofit;  // 当前浮动盈亏

		std::vector<DetailInfo> _details;  // 持仓明细列表，存储每笔开仓的详细信息

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓信息结构体，将所有字段清零。
		 */
		_PosInfo()
		{
			_volume = 0;  // 初始化持仓数量为0
			_closeprofit = 0;  // 初始化累计平仓盈亏为0
			_dynprofit = 0;  // 初始化当前浮动盈亏为0
		}
	} PosInfo;  // 持仓信息结构体类型定义
	/**
	 * @typedef PositionMap
	 * @brief 持仓映射表类型
	 * 
	 * 定义标准合约代码到持仓信息的映射表类型。
	 */
	typedef wt_hashmap<std::string, PosInfo> PositionMap;  // 定义持仓映射表类型
	PositionMap		_pos_map;  // 持仓映射表，存储所有合约的持仓信息

	/**
	 * @struct _OrderTag
	 * @brief 订单标签结构体
	 * 
	 * 存储订单ID和对应的用户标签，用于订单跟踪。
	 */
	typedef struct _OrderTag
	{
		uint32_t	_localid;  // 本地订单ID
		char		_usertag[64] = { 0 };  // 用户标签字符串（最大64字节，初始化为0）

		/**
		 * @brief 默认构造函数
		 * 
		 * 初始化订单标签结构体。
		 */
		_OrderTag(){}  // 默认构造函数
		/**
		 * @brief 带参构造函数
		 * @param localid 本地订单ID
		 * @param usertag 用户标签字符串
		 * 
		 * 使用指定的订单ID和用户标签初始化订单标签结构体。
		 */
		_OrderTag(uint32_t localid, const char* usertag)
		{
			_localid = localid;  // 设置本地订单ID
			wt_strcpy(_usertag, usertag);  // 复制用户标签字符串
		}
	} OrderTag;  // 订单标签结构体类型定义
	//typedef wt_hashmap<uint32_t, std::string> OrderMap;  // 注释掉的代码：使用哈希映射表存储订单标签（已废弃）
	//OrderMap		_orders;  // 注释掉的代码：订单映射表（已废弃）
	boost::circular_buffer<OrderTag> _orders;  // 订单标签循环缓冲区，使用循环缓冲区提高性能

	/**
	 * @struct _StraFundInfo
	 * @brief 策略资金信息结构体
	 * 
	 * 存储策略的资金统计信息，包括：
	 * - 累计平仓盈亏
	 * - 当前浮动盈亏
	 * - 累计手续费
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;  // 累计平仓盈亏
		double	_total_dynprofit;  // 当前浮动盈亏
		double	_total_fees;  // 累计手续费

		/**
		 * @brief 构造函数
		 * 
		 * 初始化策略资金信息结构体，将所有字段清零。
		 */
		_StraFundInfo()
		{
			memset(this, 0, sizeof(_StraFundInfo));  // 将整个结构体清零
		}
	} StraFundInfo;  // 策略资金信息结构体类型定义

	StraFundInfo		_fund_info;  // 策略资金信息对象

	/**
	 * @typedef PriceMap
	 * @brief 价格映射表类型
	 * 
	 * 定义标准合约代码到价格的映射表类型，用于缓存最新价格。
	 */
	typedef wt_hashmap<std::string, double> PriceMap;  // 定义价格映射表类型
	PriceMap		_price_map;  // 价格映射表，存储各合约的最新价格
};

NS_WTP_END  // 结束WonderTrader命名空间
