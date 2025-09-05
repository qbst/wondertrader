/*!
 * \file IUftStraCtx.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT极速策略引擎上下文接口定义文件
 * 
 * 本文件定义了WonderTrader框架中UFT（Ultra Fast Trading）极速策略引擎的上下文接口。
 * UFT引擎是专门针对超高频或超低延时策略设计的，系统延迟在200纳秒以内。
 * 该引擎完全从WtCore项目剥离，不向应用层提供接口，全部在C++中进行开发实现。
 * 
 * 主要功能：
 * 1. 极速交易接口：买入、卖出、开多、开空、平多、平空等
 * 2. 高级订单类型：支持FAK、FOK等特殊订单类型
 * 3. 实时数据订阅：Tick、委托队列、委托明细、逐笔成交等
 * 4. 参数监控：支持参数的实时监控和同步
 * 5. 持仓管理：本地持仓查询、持仓盈亏计算等
 * 6. 日志记录：策略运行日志的记录和输出
 */
#pragma once
#include <stdint.h>
#include <string>
#include "ExecuteDefs.h"

#include "../Includes/WTSMarcos.h"

NS_WTP_BEGIN
class WTSCommodityInfo;      // 商品信息类
class WTSTickSlice;          // Tick数据切片类
class WTSKlineSlice;         // K线数据切片类
class WTSTickData;           // Tick数据结构
struct WTSBarStruct;         // K线数据结构

/**
 * @brief 订单标记常量定义
 */
static const int UFT_OrderFlag_Nor = 0;    // 普通订单
static const int UFT_OrderFlag_FAK = 1;    // FAK订单（Fill and Kill）
static const int UFT_OrderFlag_FOK = 2;    // FOK订单（Fill or Kill）

/**
 * @brief UFT极速策略上下文接口
 * 
 * 该接口定义了UFT极速策略引擎中策略上下文的所有功能。
 * UFT策略通过该接口与引擎进行交互，执行极速交易、获取实时数据、管理持仓等。
 * 该接口专为超高频交易设计，提供最低延迟的交易执行能力。
 */
class IUftStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param name 上下文名称
	 */
	IUftStraCtx(const char* name) :_name(name) {}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~IUftStraCtx() {}

	/**
	 * @brief 获取上下文名称
	 * @return 上下文名称字符串
	 */
	const char* name() const { return _name.c_str(); }

public:
	/**
	 * @brief 获取上下文ID
	 * @return 上下文唯一标识符
	 */
	virtual uint32_t id() = 0;

	// 回调函数接口
	/**
	 * @brief 初始化完成回调
	 */
	virtual void on_init() = 0;
	
	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) = 0;
	
	/**
	 * @brief 委托队列数据回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdQue 新的委托队列数据
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) = 0;
	
	/**
	 * @brief 委托明细数据回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdDtl 新的委托明细数据
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) = 0;
	
	/**
	 * @brief 逐笔成交数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTrans 新的逐笔成交数据
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) = 0;
	
	/**
	 * @brief K线数据回调
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) {}
	
	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_begin(uint32_t uTDate) {}
	
	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_end(uint32_t uTDate) {}
	
	/**
	 * @brief 参数更新回调
	 */
	virtual void on_params_updated(){}

	/**
	 * @brief 回测结束事件回调
	 * 只在回测模式下才会触发
	 */
	virtual void	on_bactest_end() {};

	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 更新的Tick数据
	 */
	virtual void	on_tick_updated(const char* stdCode, WTSTickData* newTick) {}
	
	/**
	 * @brief 委托队列数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdQue 更新的委托队列数据
	 */
	virtual void	on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue) {}
	
	/**
	 * @brief 委托明细数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdDtl 更新的委托明细数据
	 */
	virtual void	on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl) {}
	
	/**
	 * @brief 逐笔成交数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newTrans 更新的逐笔成交数据
	 */
	virtual void	on_trans_updated(const char* stdCode, WTSTransData* newTrans) {}

	// 参数监控接口
	/**
	 * @brief 监控字符串类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @return 参数值字符串
	 */
	virtual const char*	watch_param(const char* name, const char* initVal = "") { return initVal; }
	
	/**
	 * @brief 监控双精度浮点类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @return 参数值
	 */
	virtual double		watch_param(const char* name, double initVal = 0) { return initVal; }
	
	/**
	 * @brief 监控32位无符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @return 参数值
	 */
	virtual uint32_t	watch_param(const char* name, uint32_t initVal = 0) { return initVal; }
	
	/**
	 * @brief 监控64位无符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @return 参数值
	 */
	virtual uint64_t	watch_param(const char* name, uint64_t initVal = 0) { return initVal; }
	
	/**
	 * @brief 监控32位有符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @return 参数值
	 */
	virtual int32_t		watch_param(const char* name, int32_t initVal = 0) { return initVal; }
	
	/**
	 * @brief 监控64位有符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @return 参数值
	 */
	virtual int64_t		watch_param(const char* name, int64_t initVal = 0) { return initVal; }

	/**
	 * @brief 提交参数监控器
	 */
	virtual void		commit_param_watcher() {}

	// 参数读取接口
	/**
	 * @brief 读取字符串类型参数
	 * @param name 参数名称
	 * @param defVal 默认值
	 * @return 参数值字符串
	 */
	virtual const char*	read_param(const char* name, const char* defVal = "") { return defVal; }
	
	/**
	 * @brief 读取双精度浮点类型参数
	 * @param name 参数名称
	 * @param defVal 默认值
	 * @return 参数值
	 */
	virtual double		read_param(const char* name, double defVal = 0) { return defVal; }
	
	/**
	 * @brief 读取32位无符号整数类型参数
	 * @param name 参数名称
	 * @param defVal 默认值
	 * @return 参数值
	 */
	virtual uint32_t	read_param(const char* name, uint32_t defVal = 0) { return defVal; }
	
	/**
	 * @brief 读取64位无符号整数类型参数
	 * @param name 参数名称
	 * @param defVal 默认值
	 * @return 参数值
	 */
	virtual uint64_t	read_param(const char* name, uint64_t defVal = 0) { return defVal; }
	
	/**
	 * @brief 读取32位有符号整数类型参数
	 * @param name 参数名称
	 * @param defVal 默认值
	 * @return 参数值
	 */
	virtual int32_t		read_param(const char* name, int32_t defVal = 0) { return defVal; }
	
	/**
	 * @brief 读取64位有符号整数类型参数
	 * @param name 参数名称
	 * @param defVal 默认值
	 * @return 参数值
	 */
	virtual int64_t		read_param(const char* name, int64_t defVal = 0) { return defVal; }

	// 参数同步接口
	/**
	 * @brief 同步字符串类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @param bForceWrite 是否强制写入
	 * @return 参数值字符串指针
	 */
	virtual const char*	sync_param(const char* name, const char* initVal = "", bool bForceWrite = false) { return nullptr; }
	
	/**
	 * @brief 同步双精度浮点类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @param bForceWrite 是否强制写入
	 * @return 参数值指针
	 */
	virtual double*		sync_param(const char* name, double initVal = 0, bool bForceWrite = false) { return nullptr; }
	
	/**
	 * @brief 同步32位无符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @param bForceWrite 是否强制写入
	 * @return 参数值指针
	 */
	virtual uint32_t*	sync_param(const char* name, uint32_t initVal = 0, bool bForceWrite = false) { return nullptr; }
	
	/**
	 * @brief 同步64位无符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @param bForceWrite 是否强制写入
	 * @return 参数值指针
	 */
	virtual uint64_t*	sync_param(const char* name, uint64_t initVal = 0, bool bForceWrite = false) { return nullptr; }
	
	/**
	 * @brief 同步32位有符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @param bForceWrite 是否强制写入
	 * @return 参数值指针
	 */
	virtual int32_t*	sync_param(const char* name, int32_t initVal = 0, bool bForceWrite = false) { return nullptr; }
	
	/**
	 * @brief 同步64位有符号整数类型参数
	 * @param name 参数名称
	 * @param initVal 初始值
	 * @param bForceWrite 是否强制写入
	 * @return 参数值指针
	 */
	virtual int64_t*	sync_param(const char* name, int64_t initVal = 0, bool bForceWrite = false) { return nullptr; }

	// 策略接口
	/**
	 * @brief 获取当前日期
	 * @return 当前日期
	 */
	virtual uint32_t	stra_get_date() = 0;
	
	/**
	 * @brief 获取当前时间
	 * @return 当前时间
	 */
	virtual uint32_t	stra_get_time() = 0;
	
	/**
	 * @brief 获取当前秒数
	 * @return 当前秒数
	 */
	virtual uint32_t	stra_get_secs() = 0;

	/**
	 * @brief 撤单接口
	 * @param localid 本地单号
	 * @return 撤单是否成功
	 */
	virtual bool		stra_cancel(uint32_t localid) = 0;
	
	/**
	 * @brief 一键撤单接口
	 * @param stdCode 合约代码
	 * @return 撤单的订单ID列表
	 */
	virtual OrderIDs	stra_cancel_all(const char* stdCode) = 0;

	/**
	 * @brief 买入下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0则是市价单
	 * @param qty 下单数量
	 * @param flag 下单标志: 0-normal，1-fak，2-fok，默认0
	 * @return 订单ID列表
	 */
	virtual OrderIDs	stra_buy(const char* stdCode, double price, double qty, int flag = 0) { return OrderIDs(); }

	/**
	 * @brief 卖出下单接口
	 * @param stdCode 合约代码
	 * @param price 下单价格，0则是市价单
	 * @param qty 下单数量
	 * @param flag 下单标志: 0-normal，1-fak，2-fok，默认0
	 * @return 订单ID列表
	 */
	virtual OrderIDs	stra_sell(const char* stdCode, double price, double qty, int flag = 0) { return OrderIDs(); }

	/**
	 * @brief 开多接口
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param flag 下单标志: 0-normal，1-fak，2-fok，默认0
	 * @return 本地订单ID
	 */
	virtual uint32_t	stra_enter_long(const char* stdCode, double price, double qty, int flag = 0) { return 0; }

	/**
	 * @brief 开空接口
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param flag 下单标志: 0-normal，1-fak，2-fok，默认0
	 * @return 本地订单ID
	 */
	virtual uint32_t	stra_enter_short(const char* stdCode, double price, double qty, int flag = 0) { return 0; }

	/**
	 * @brief 平多接口
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param isToday 是否今仓，SHFE、INE专用
	 * @param flag 下单标志: 0-normal，1-fak，2-fok，默认0
	 * @return 本地订单ID
	 */
	virtual uint32_t	stra_exit_long(const char* stdCode, double price, double qty, bool isToday = false, int flag = 0) { return 0; }

	/**
	 * @brief 平空接口
	 * @param stdCode 代码，格式如SSE.600000
	 * @param price 委托价格
	 * @param qty 下单数量
	 * @param isToday 是否今仓，SHFE、INE专用
	 * @param flag 下单标志: 0-normal，1-fak，2-fok，默认0
	 * @return 本地订单ID
	 */
	virtual uint32_t	stra_exit_short(const char* stdCode, double price, double qty, bool isToday = false, int flag = 0) { return 0; }

	/**
	 * @brief 获取品种信息
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 品种信息对象指针
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) = 0;

	/**
	 * @brief 获取K线数据，暂未实现
	 * @param stdCode 代码，格式如SSE.600000
	 * @param period 周期，如m1/m5/d1
	 * @param count 条数
	 * @return K线数据切片对象指针
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count) = 0;

	/**
	 * @brief 获取Tick数据，暂未实现
	 * @param stdCode 代码，格式如SSE.600000
	 * @param count 条数
	 * @return Tick数据切片对象指针
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) = 0;

	/**
	 * @brief 获取委托明细，暂未实现
	 * @param stdCode 代码，格式如SSE.600000
	 * @param count 条数
	 * @return 委托明细数据切片对象指针
	 */
	virtual WTSOrdDtlSlice*	stra_get_order_detail(const char* stdCode, uint32_t count) = 0;

	/**
	 * @brief 获取委托队列，暂未实现
	 * @param stdCode 代码，格式如SSE.600000
	 * @param count 条数
	 * @return 委托队列数据切片对象指针
	 */
	virtual WTSOrdQueSlice*	stra_get_order_queue(const char* stdCode, uint32_t count) = 0;

	/**
	 * @brief 获取逐笔成交，暂未实现
	 * @param stdCode 代码，格式如SSE.600000
	 * @param count 条数
	 * @return 逐笔成交数据切片对象指针
	 */
	virtual WTSTransSlice*	stra_get_transaction(const char* stdCode, uint32_t count) = 0;

	/**
	 * @brief 读取最后一笔Tick数据
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 最新Tick数据对象指针
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) = 0;

	/**
	 * @brief 获取持仓
	 * @param stdCode 代码，格式如SSE.600000
	 * @param bOnlyValid 获取可用持仓
	 * @param iFlag 读取标记，1-多头，2-空头，3-净头寸
	 * @return 持仓数量
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, int32_t iFlag = 3) = 0;

	/**
	 * @brief 枚举持仓，会通过on_position回调进来
	 * @param stdCode 代码，格式如SSE.600000，如果传空，则枚举全部的
	 * @return 持仓数量
	 */
	virtual double stra_enum_position(const char* stdCode) = 0;

	/**
	 * @brief 获取本地持仓
	 * @param stdCode 代码，格式如SSE.600000
	 * @param isLong 多头or空头
	 * @return 本地持仓数量
	 */
	virtual double stra_get_local_position(const char* stdCode) = 0;

	/**
	 * @brief 获取本地持仓盈亏
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 本地持仓盈亏
	 */
	virtual double stra_get_local_posprofit(const char* stdCode) { return 0; }

	/**
	 * @brief 获取本地平仓盈亏
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 本地平仓盈亏
	 */
	virtual double stra_get_local_closeprofit(const char* stdCode) { return 0; }

	/**
	 * @brief 获取最新价格
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 最新价格
	 */
	virtual double stra_get_price(const char* stdCode) = 0;

	/**
	 * @brief 获取未完成手数，买入为正，卖出为负
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 未完成手数
	 */
	virtual double stra_get_undone(const char* stdCode) = 0;

	/**
	 * @brief 获取信息数量
	 * @param stdCode 代码，格式如SSE.600000
	 * @return 信息数量
	 */
	virtual uint32_t stra_get_infos(const char* stdCode) { return 0; }

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 代码，格式如SSE.600000
	 */
	virtual void stra_sub_ticks(const char* stdCode) = 0;
	
	/**
	 * @brief 订阅委托队列数据
	 * @param stdCode 代码，格式如SSE.600000
	 */
	virtual void stra_sub_order_queues(const char* stdCode) = 0;
	
	/**
	 * @brief 订阅委托明细数据
	 * @param stdCode 代码，格式如SSE.600000
	 */
	virtual void stra_sub_order_details(const char* stdCode) = 0;
	
	/**
	 * @brief 订阅逐笔成交数据
	 * @param stdCode 代码，格式如SSE.600000
	 */
	virtual void stra_sub_transactions(const char* stdCode) = 0;

	/**
	 * @brief 输出信息日志
	 * @param message 日志消息
	 */
	virtual void stra_log_info(const char* message) = 0;
	
	/**
	 * @brief 输出调试日志
	 * @param message 日志消息
	 */
	virtual void stra_log_debug(const char* message) = 0;
	
	/**
	 * @brief 输出错误日志
	 * @param message 日志消息
	 */
	virtual void stra_log_error(const char* message) = 0;

protected:
	std::string _name;    // 上下文名称
};

NS_WTP_END