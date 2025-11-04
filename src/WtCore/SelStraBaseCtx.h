/*!
* \file SelStraBaseCtx.h
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief 选股策略基础上下文头文件
*
* 文件设计逻辑与作用总结：
* 本文件定义了选股策略基础上下文类SelStraBaseCtx。
* 该类是选股策略的核心基础类，为选股策略提供完整的交易上下文环境。
* 
* 主要功能：
* 1. 策略生命周期管理：初始化、交易日开始/结束、定时调度等回调处理。
* 2. 持仓管理：支持多明细持仓、T+1规则、冻结持仓等功能。
* 3. 信号管理：接收和处理策略发出的持仓信号，在合适的时机执行。
* 4. 盈亏计算：实时计算持仓盈亏、累计盈亏、动态盈亏等。
* 5. 数据持久化：保存和加载持仓、资金、信号等数据，支持策略重启恢复。
* 6. 用户数据管理：提供用户自定义数据的保存和加载功能。
* 7. 日志记录：记录交易、平仓、资金、信号、持仓等日志。
* 8. 行情数据访问：提供K线、Tick、价格等行情数据查询接口。
* 9. 策略接口：为策略提供统一的API接口，包括持仓操作、价格查询、日志输出等。
*/
#pragma once  // 防止头文件重复包含
#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap, wt_hashset
#include "../Includes/ISelStraCtx.h"  // 包含选股策略上下文接口
#include "../Includes/WTSDataDef.hpp"  // 包含数据定义头文件

#include "../Share/BoostFile.hpp"  // 包含Boost文件操作类
#include "../Share/fmtlib.h"  // 包含格式化库头文件

NS_WTP_BEGIN  // 开始WonderTrader命名空间

class WtSelEngine;  // 前向声明：选股引擎类


/**
 * @class SelStraBaseCtx
 * @brief 选股策略基础上下文类
 *
 * 该类是选股策略的核心基础类，继承自ISelStraCtx接口。
 * 为选股策略提供完整的交易上下文环境，包括持仓管理、盈亏计算、
 * 数据持久化、行情数据访问、策略接口等功能。
 * 
 * 特点：
 * 1. 支持多明细持仓管理，可以跟踪每笔开仓的详细信息。
 * 2. 支持T+1规则，可以处理冻结持仓。
 * 3. 支持信号机制，策略发出的持仓信号会在合适的时机执行。
 * 4. 支持数据持久化，策略重启后可以恢复之前的状态。
 * 5. 提供完整的日志记录功能。
 */
class SelStraBaseCtx : public ISelStraCtx  // 继承选股策略上下文接口
{
public:
	/**
	 * @brief 构造函数
	 * @param engine 选股引擎指针
	 * @param name 策略名称
	 * @param slippage 滑点设置（回测时使用）
	 *
	 * 初始化选股策略基础上下文对象，设置引擎、名称和滑点参数。
	 */
	SelStraBaseCtx(WtSelEngine* engine, const char* name, int32_t slippage);  // 构造函数
	/**
	 * @brief 析构函数
	 *
	 * 清理解股策略基础上下文对象。
	 */
	virtual ~SelStraBaseCtx();  // 析构函数

private:
	/**
	 * @brief 初始化输出文件
	 *
	 * 创建并初始化交易日志、平仓日志、资金日志、信号日志、持仓日志等输出文件。
	 */
	void	init_outputs();  // 初始化输出文件
	/**
	 * @brief 记录信号日志
	 * @param stdCode 标准合约代码
	 * @param target 目标持仓数量
	 * @param price 信号价格
	 * @param gentime 信号生成时间
	 * @param usertag 用户标签，默认空字符串
	 *
	 * 将持仓信号记录到信号日志文件中。
	 */
	inline void log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag = "");  // 记录信号日志
	/**
	 * @brief 记录交易日志
	 * @param stdCode 标准合约代码
	 * @param isLong 是否做多
	 * @param isOpen 是否开仓
	 * @param curTime 当前时间
	 * @param price 成交价格
	 * @param qty 成交数量
	 * @param userTag 用户标签，默认空字符串
	 * @param fee 手续费，默认0.0
	 *
	 * 将交易记录写入交易日志文件。
	 */
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag = "", double fee = 0.0);  // 记录交易日志
	/**
	 * @brief 记录平仓日志
	 * @param stdCode 标准合约代码
	 * @param isLong 是否做多
	 * @param openTime 开仓时间
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间
	 * @param closepx 平仓价格
	 * @param qty 平仓数量
	 * @param profit 平仓盈亏
	 * @param totalprofit 累计盈亏，默认0
	 * @param enterTag 开仓标签，默认空字符串
	 * @param exitTag 平仓标签，默认空字符串
	 *
	 * 将平仓记录写入平仓日志文件。
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double totalprofit = 0, const char* enterTag = "", const char* exitTag = "");  // 记录平仓日志

	/**
	 * @brief 保存数据
	 * @param flag 保存标志，默认0xFFFFFFFF（保存所有数据）
	 *
	 * 将持仓、资金、信号等数据保存到JSON文件中。
	 */
	void	save_data(uint32_t flag = 0xFFFFFFFF);  // 保存数据
	/**
	 * @brief 加载数据
	 * @param flag 加载标志，默认0xFFFFFFFF（加载所有数据）
	 *
	 * 从JSON文件中加载持仓、资金、信号等数据。
	 */
	void	load_data(uint32_t flag = 0xFFFFFFFF);  // 加载数据

	/**
	 * @brief 加载用户数据
	 *
	 * 从JSON文件中加载用户自定义数据。
	 */
	void	load_userdata();  // 加载用户数据
	/**
	 * @brief 保存用户数据
	 *
	 * 将用户自定义数据保存到JSON文件中。
	 */
	void	save_userdata();  // 保存用户数据

	/**
	 * @brief 更新动态盈亏
	 * @param stdCode 标准合约代码
	 * @param price 当前价格
	 *
	 * 根据当前价格更新指定合约的持仓动态盈亏。
	 */
	void	update_dyn_profit(const char* stdCode, double price);  // 更新动态盈亏

	/**
	 * @brief 设置持仓实现
	 * @param stdCode 标准合约代码
	 * @param qty 目标持仓数量
	 * @param userTag 用户标签，默认空字符串
	 * @param bTriggered 是否已触发，默认false
	 *
	 * 根据目标持仓数量调整实际持仓，并记录交易和平仓日志。
	 * 内部会处理开仓、平仓、反手等逻辑，并计算盈亏和手续费。
	 */
	void	do_set_position(const char* stdCode, double qty, const char* userTag = "", bool bTriggered = false);  // 设置持仓实现
	/**
	 * @brief 添加信号
	 * @param stdCode 标准合约代码
	 * @param qty 目标持仓数量
	 * @param userTag 用户标签，默认空字符串
	 *
	 * 添加持仓信号到信号映射表中，信号会在合适的时机执行。
	 */
	void	append_signal(const char* stdCode, double qty, const char* userTag = "");  // 添加信号

protected:
	/**
	 * @brief 调试日志记录模板函数
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数
	 *
	 * 格式化日志消息并调用stra_log_debug记录调试日志。
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)  // 调试日志记录模板函数
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化日志消息
		stra_log_debug(buffer);  // 调用调试日志记录函数
	}

	/**
	 * @brief 信息日志记录模板函数
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数
	 *
	 * 格式化日志消息并调用stra_log_info记录信息日志。
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)  // 信息日志记录模板函数
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化日志消息
		stra_log_info(buffer);  // 调用信息日志记录函数
	}

	/**
	 * @brief 错误日志记录模板函数
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 可变参数
	 *
	 * 格式化日志消息并调用stra_log_error记录错误日志。
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)  // 错误日志记录模板函数
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化日志消息
		stra_log_error(buffer);  // 调用错误日志记录函数
	}

public:
	/**
	 * @brief 获取上下文ID
	 * @return uint32_t 返回上下文ID
	 *
	 * 返回当前策略上下文的唯一标识ID。
	 */
	virtual uint32_t id() { return _context_id; }  // 获取上下文ID

	//回调函数
	/**
	 * @brief 初始化回调
	 *
	 * 策略初始化时被调用，用于初始化输出文件、加载数据等。
	 */
	virtual void on_init() override;  // 初始化回调
	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期
	 *
	 * 每个交易日开始时被调用，用于处理冻结持仓解冻等。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;  // 交易日开始回调
	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期
	 *
	 * 每个交易日结束时被调用，用于保存数据、记录日志等。
	 */
	virtual void on_session_end(uint32_t uTDate) override;  // 交易日结束回调
	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据
	 * @param bEmitStrategy 是否触发策略回调，默认true
	 *
	 * 当收到新的Tick数据时被调用，用于更新价格、触发信号、更新动态盈亏等。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy = true) override;  // Tick数据回调
	/**
	 * @brief K线数据回调
	 * @param stdCode 标准合约代码
	 * @param period 周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 *
	 * 当收到新的K线数据时被调用，用于标记K线已收盘。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;  // K线数据回调
	/**
	 * @brief 定时调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * @param fireTime 触发时间
	 * @return bool 返回true表示处理成功
	 *
	 * 定时调度时被调用，用于执行策略逻辑、保存数据等。
	 */
	virtual bool on_schedule(uint32_t curDate, uint32_t curTime, uint32_t fireTime) override;  // 定时调度回调

	/**
	 * @brief 枚举持仓
	 * @param cb 回调函数
	 *
	 * 遍历所有持仓（包括已发信号但未执行的），调用回调函数处理。
	 */
	virtual void enum_position(FuncEnumSelPositionCallBack cb) override;  // 枚举持仓

	//////////////////////////////////////////////////////////////////////////
	//策略接口
	/**
	 * @brief 获取持仓
	 * @param stdCode 标准合约代码
	 * @param bOnlyValid 是否只返回有效持仓（排除冻结持仓），默认false
	 * @param userTag 用户标签，默认空字符串
	 * @return double 返回持仓数量
	 *
	 * 获取指定合约的持仓数量。如果指定了userTag，则返回该标签对应的持仓。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, const char* userTag = "") override;  // 获取持仓
	/**
	 * @brief 设置持仓
	 * @param stdCode 标准合约代码
	 * @param qty 目标持仓数量
	 * @param userTag 用户标签，默认空字符串
	 *
	 * 设置指定合约的目标持仓数量，会添加信号到信号映射表中。
	 */
	virtual void stra_set_position(const char* stdCode, double qty, const char* userTag = "") override;  // 设置持仓
	/**
	 * @brief 获取价格
	 * @param stdCode 标准合约代码
	 * @return double 返回当前价格
	 *
	 * 获取指定合约的当前价格。
	 */
	virtual double stra_get_price(const char* stdCode) override;  // 获取价格

	/**
	 * @brief 读取当日价格
	 * @param stdCode 标准合约代码
	 * @param flag 价格类型标志，默认0（收盘价）
	 * @return double 返回当日价格
	 *
	 * 获取指定合约的当日价格（开盘价、最高价、最低价、收盘价等）。
	 */
	virtual double stra_get_day_price(const char* stdCode, int flag = 0) override;  // 读取当日价格

	/**
	 * @brief 获取交易日
	 * @return uint32_t 返回交易日日期
	 *
	 * 获取当前交易日日期。
	 */
	virtual uint32_t stra_get_tdate() override;  // 获取交易日
	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期
	 *
	 * 获取当前日期（如果在调度中，返回调度日期；否则返回引擎日期）。
	 */
	virtual uint32_t stra_get_date() override;  // 获取当前日期
	/**
	 * @brief 获取当前时间
	 * @return uint32_t 返回当前时间
	 *
	 * 获取当前时间（如果在调度中，返回调度时间；否则返回引擎时间）。
	 */
	virtual uint32_t stra_get_time() override;  // 获取当前时间

	/**
	 * @brief 获取资金数据
	 * @param flag 资金类型标志，默认0（总权益）
	 * @return double 返回资金数据
	 *
	 * 获取资金数据：0-总权益，1-累计盈亏，2-动态盈亏，3-手续费。
	 */
	virtual double stra_get_fund_data(int flag /* = 0 */) override;  // 获取资金数据

	/**
	 * @brief 获取首次开仓时间
	 * @param stdCode 标准合约代码
	 * @return uint64_t 返回首次开仓时间
	 *
	 * 获取指定合约的首次开仓时间（第一笔持仓明细的开仓时间）。
	 */
	virtual uint64_t stra_get_first_entertime(const char* stdCode) override;  // 获取首次开仓时间
	/**
	 * @brief 获取最后开仓时间
	 * @param stdCode 标准合约代码
	 * @return uint64_t 返回最后开仓时间
	 *
	 * 获取指定合约的最后开仓时间（最后一笔持仓明细的开仓时间）。
	 */
	virtual uint64_t stra_get_last_entertime(const char* stdCode) override;  // 获取最后开仓时间
	/**
	 * @brief 获取最后开仓价格
	 * @param stdCode 标准合约代码
	 * @return double 返回最后开仓价格
	 *
	 * 获取指定合约的最后开仓价格（最后一笔持仓明细的开仓价格）。
	 */
	virtual double stra_get_last_enterprice(const char* stdCode) override;  // 获取最后开仓价格
	/**
	 * @brief 获取最后开仓标签
	 * @param stdCode 标准合约代码
	 * @return const char* 返回最后开仓标签
	 *
	 * 获取指定合约的最后开仓标签（最后一笔持仓明细的开仓标签）。
	 */
	virtual const char* stra_get_last_entertag(const char* stdCode) override;  // 获取最后开仓标签

	/**
	 * @brief 获取最后平仓时间
	 * @param stdCode 标准合约代码
	 * @return uint64_t 返回最后平仓时间
	 *
	 * 获取指定合约的最后平仓时间。
	 */
	virtual uint64_t stra_get_last_exittime(const char* stdCode) override;  // 获取最后平仓时间

	/**
	 * @brief 获取持仓均价
	 * @param stdCode 标准合约代码
	 * @return double 返回持仓均价
	 *
	 * 计算并返回指定合约的持仓均价（加权平均）。
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) override;  // 获取持仓均价
	/**
	 * @brief 获取持仓盈亏
	 * @param stdCode 标准合约代码
	 * @return double 返回持仓盈亏
	 *
	 * 获取指定合约的持仓动态盈亏。
	 */
	virtual double stra_get_position_profit(const char* stdCode) override;  // 获取持仓盈亏

	/**
	 * @brief 获取明细开仓时间
	 * @param stdCode 标准合约代码
	 * @param userTag 用户标签
	 * @return uint64_t 返回开仓时间
	 *
	 * 获取指定合约和标签对应的持仓明细的开仓时间。
	 */
	virtual uint64_t stra_get_detail_entertime(const char* stdCode, const char* userTag) override;  // 获取明细开仓时间
	/**
	 * @brief 获取明细成本
	 * @param stdCode 标准合约代码
	 * @param userTag 用户标签
	 * @return double 返回成本价格
	 *
	 * 获取指定合约和标签对应的持仓明细的成本价格。
	 */
	virtual double stra_get_detail_cost(const char* stdCode, const char* userTag) override;  // 获取明细成本
	/**
	 * @brief 获取明细盈亏
	 * @param stdCode 标准合约代码
	 * @param userTag 用户标签
	 * @param flag 盈亏类型标志，默认0（当前盈亏）
	 * @return double 返回盈亏数据
	 *
	 * 获取指定合约和标签对应的持仓明细的盈亏：
	 * 0-当前盈亏，1-最大盈利，-1-最大亏损，2-最高价，-2-最低价。
	 */
	virtual double stra_get_detail_profit(const char* stdCode, const char* userTag, int flag = 0) override;  // 获取明细盈亏

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回商品信息指针
	 *
	 * 获取指定合约的商品信息。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;  // 获取商品信息
	/**
	 * @brief 获取会话信息
	 * @param stdCode 标准合约代码
	 * @return WTSSessionInfo* 返回会话信息指针
	 *
	 * 获取指定合约的交易会话信息。
	 */
	virtual WTSSessionInfo* stra_get_sessinfo(const char* stdCode) override;  // 获取会话信息
	/**
	 * @brief 获取K线数据
	 * @param stdCode 标准合约代码
	 * @param period 周期
	 * @param count 数量
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 *
	 * 获取指定合约的K线数据切片。
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count) override;  // 获取K线数据
	/**
	 * @brief 获取Tick数据
	 * @param stdCode 标准合约代码
	 * @param count 数量
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 *
	 * 获取指定合约的Tick数据切片。
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) override;  // 获取Tick数据
	/**
	 * @brief 获取最后一个Tick
	 * @param stdCode 标准合约代码
	 * @return WTSTickData* 返回最后一个Tick数据指针
	 *
	 * 获取指定合约的最后一个Tick数据。
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) override;  // 获取最后一个Tick

	/**
	 * @brief 获取分月合约代码
	 * @param stdCode 标准合约代码
	 * @return std::string 返回分月合约代码
	 *
	 * 获取指定标准合约对应的分月合约代码。
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) override;  // 获取分月合约代码

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准合约代码
	 *
	 * 订阅指定合约的Tick数据，用于接收实时行情。
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;  // 订阅Tick数据

	/**
	 * @brief 记录信息日志
	 * @param message 日志消息
	 *
	 * 记录信息级别的日志。
	 */
	virtual void stra_log_info(const char* message) override;  // 记录信息日志
	/**
	 * @brief 记录调试日志
	 * @param message 日志消息
	 *
	 * 记录调试级别的日志。
	 */
	virtual void stra_log_debug(const char* message) override;  // 记录调试日志
	/**
	 * @brief 记录警告日志
	 * @param message 日志消息
	 *
	 * 记录警告级别的日志。
	 */
	virtual void stra_log_warn(const char* message) override;  // 记录警告日志
	/**
	 * @brief 记录错误日志
	 * @param message 日志消息
	 *
	 * 记录错误级别的日志。
	 */
	virtual void stra_log_error(const char* message) override;  // 记录错误日志

	/**
	 * @brief 保存用户数据
	 * @param key 键
	 * @param val 值
	 *
	 * 保存用户自定义数据到内存中，会在适当时机持久化到文件。
	 */
	virtual void stra_save_user_data(const char* key, const char* val) override;  // 保存用户数据

	/**
	 * @brief 加载用户数据
	 * @param key 键
	 * @param defVal 默认值，默认空字符串
	 * @return const char* 返回用户数据值
	 *
	 * 从内存中加载用户自定义数据，如果不存在则返回默认值。
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") override;  // 加载用户数据

protected:
	uint32_t		_context_id;  // 上下文ID，策略上下文的唯一标识
	WtSelEngine*	_engine;  // 选股引擎指针
	int32_t			_slippage;  // 滑点设置（回测时使用）

	uint64_t		_total_calc_time;	// 总计算时间（微秒）
	uint32_t		_emit_times;		// 总计算次数

	uint32_t		_schedule_date;  // 调度日期（定时调度时的日期）
	uint32_t		_schedule_time;  // 调度时间（定时调度时的时间）

	/**
	 * @struct KlineTag
	 * @brief K线标签结构体
	 *
	 * 用于标记K线是否已收盘。
	 */
	typedef struct _KlineTag
	{
		bool			_closed;  // 是否已收盘

		_KlineTag() :_closed(false){}  // 构造函数，初始化_closed为false

	} KlineTag;  // K线标签类型
	typedef wt_hashmap<std::string, KlineTag> KlineTags;  // K线标签映射表类型，键为"合约代码#周期"，值为K线标签
	KlineTags	_kline_tags;  // K线标签映射表

	typedef wt_hashmap<std::string, double> PriceMap;  // 价格映射表类型，键为合约代码，值为价格
	PriceMap		_price_map;  // 价格映射表，存储每个合约的最新价格

	/**
	 * @struct DetailInfo
	 * @brief 持仓明细信息结构体
	 *
	 * 存储每笔开仓的详细信息，包括方向、价格、数量、时间、盈亏等。
	 */
	typedef struct _DetailInfo
	{
		bool		_long;  // 是否做多
		double		_price;  // 开仓价格
		double		_volume;  // 持仓数量
		uint64_t	_opentime;  // 开仓时间
		uint32_t	_opentdate;  // 开仓日期
		double		_max_profit;  // 最大盈利
		double		_max_loss;  // 最大亏损
		double		_max_price;  // 最高价
		double		_min_price;  // 最低价
		double		_profit;  // 当前盈亏
		char		_opentag[32];  // 开仓标签

		_DetailInfo()  // 构造函数
		{
			memset(this, 0, sizeof(_DetailInfo));  // 将所有成员初始化为0
		}
	} DetailInfo;  // 持仓明细信息类型

	/**
	 * @struct PosInfo
	 * @brief 持仓信息结构体
	 *
	 * 存储每个合约的持仓信息，包括总持仓、盈亏、明细列表等。
	 */
	typedef struct _PosInfo
	{
		double		_volume;  // 总持仓数量
		double		_closeprofit;  // 累计平仓盈亏
		double		_dynprofit;  // 动态盈亏（浮动盈亏）

		uint64_t	_last_entertime;  // 最后开仓时间
		uint64_t	_last_exittime;  // 最后平仓时间

		double		_frozen;  // 冻结持仓数量（T+1规则）
		uint32_t	_frozen_date;  // 冻结日期

		std::vector<DetailInfo> _details;  // 持仓明细列表

		_PosInfo()  // 构造函数
		{
			_volume = 0;  // 初始化总持仓为0
			_closeprofit = 0;  // 初始化累计平仓盈亏为0
			_dynprofit = 0;  // 初始化动态盈亏为0
			_last_entertime = 0;  // 初始化最后开仓时间为0
			_last_exittime = 0;  // 初始化最后平仓时间为0
			_frozen = 0;  // 初始化冻结持仓为0
			_frozen_date = 0;  // 初始化冻结日期为0
		}
	} PosInfo;  // 持仓信息类型
	typedef wt_hashmap<std::string, PosInfo> PositionMap;  // 持仓映射表类型，键为合约代码，值为持仓信息
	PositionMap		_pos_map;  // 持仓映射表

	/**
	 * @struct SigInfo
	 * @brief 信号信息结构体
	 *
	 * 存储策略发出的持仓信号信息，包括目标持仓、信号价格、生成时间等。
	 */
	typedef struct _SigInfo
	{
		double		_volume;  // 目标持仓数量
		std::string	_usertag;  // 用户标签
		double		_sigprice;  // 信号价格
		bool		_triggered;  // 是否已触发
		uint64_t	_gentime;  // 信号生成时间

		_SigInfo()  // 构造函数
		{
			_volume = 0;  // 初始化目标持仓为0
			_sigprice = 0;  // 初始化信号价格为0
			_triggered = false;  // 初始化触发标志为false
			_gentime = 0;  // 初始化生成时间为0
		}
	}SigInfo;  // 信号信息类型
	typedef wt_hashmap<std::string, SigInfo>	SignalMap;  // 信号映射表类型，键为合约代码，值为信号信息
	SignalMap		_sig_map;  // 信号映射表

	BoostFilePtr	_trade_logs;  // 交易日志文件指针
	BoostFilePtr	_close_logs;  // 平仓日志文件指针
	BoostFilePtr	_fund_logs;  // 资金日志文件指针
	BoostFilePtr	_sig_logs;  // 信号日志文件指针
	BoostFilePtr	_pos_logs;  // 持仓日志文件指针

	//是否处于调度中的标记
	bool			_is_in_schedule;	// 是否在自动调度中

	//用户数据
	typedef wt_hashmap<std::string, std::string> StringHashMap;  // 字符串哈希映射表类型，键值对都为字符串
	StringHashMap	_user_datas;  // 用户数据映射表
	bool			_ud_modified;  // 用户数据是否已修改标志

	/**
	 * @struct StraFundInfo
	 * @brief 策略资金信息结构体
	 *
	 * 存储策略的资金信息，包括累计盈亏、动态盈亏、手续费等。
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;  // 累计平仓盈亏
		double	_total_dynprofit;  // 总动态盈亏
		double	_total_fees;  // 总手续费

		_StraFundInfo()  // 构造函数
		{
			memset(this, 0, sizeof(_StraFundInfo));  // 将所有成员初始化为0
		}
	} StraFundInfo;  // 策略资金信息类型

	StraFundInfo		_fund_info;  // 策略资金信息

	//tick订阅列表
	wt_hashset<std::string> _tick_subs;  // Tick订阅列表，存储已订阅Tick数据的合约代码
};


NS_WTP_END