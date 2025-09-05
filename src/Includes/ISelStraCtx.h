/*!
* \file ISelStraCtx.h
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief SEL策略上下文接口定义文件
* 
* 本文件定义了WonderTrader框架中SEL策略引擎的上下文接口。
* SEL策略上下文是策略与引擎之间的桥梁，为策略提供数据访问、交易执行、日志记录等核心功能。
* 
* 主要功能包括：
* 1. 策略生命周期管理：初始化、交易日开始/结束、定时调度等
* 2. 市场数据访问：Tick数据、K线数据、历史数据等
* 3. 交易接口：持仓查询、目标仓位设置、价格查询等
* 4. 策略工具：日志记录、用户数据存储、合约信息查询等
* 5. 时间管理：交易日期、当前时间、时间转换等
*/
#pragma once
#include <stdint.h>
#include <functional>
#include "../Includes/WTSMarcos.h"

NS_WTP_BEGIN
class WTSCommodityInfo;      // 商品信息类
class WTSSessionInfo;        // 交易时间模板信息类
class WTSTickData;           // Tick数据结构
struct WTSBarStruct;         // K线数据结构
class WTSKlineSlice;         // K线数据切片类
class WTSTickSlice;          // Tick数据切片类

// 持仓枚举回调函数类型定义
typedef std::function<void(const char*, double)> FuncEnumSelPositionCallBack;

/**
 * @brief SEL策略上下文接口
 * 
 * 该接口定义了SEL策略引擎中策略上下文的所有功能。
 * 策略通过该接口与引擎进行交互，获取数据、执行交易、记录日志等。
 * 每个策略实例都有独立的上下文对象，确保策略间的数据隔离。
 */
class ISelStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param name 上下文名称
	 */
	ISelStraCtx(const char* name) :_name(name){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~ISelStraCtx(){}

	/**
	 * @brief 获取上下文名称
	 * @return 上下文名称字符串
	 */
	inline const char* name() const{ return _name.c_str(); }

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
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_begin(uint32_t uTDate) = 0;
	
	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期
	 */
	virtual void on_session_end(uint32_t uTDate) = 0;
	
	/**
	 * @brief Tick数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据
	 * @param bEmitStrategy 是否触发策略回调
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy = true) = 0;
	
	/**
	 * @brief K线数据回调
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times K线倍数
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) = 0;
	
	/**
	 * @brief 定时调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * @param fireTime 触发时间
	 * @return 是否继续调度
	 */
	virtual bool on_schedule(uint32_t curDate, uint32_t curTime, uint32_t fireTime) = 0;
	
	/**
	 * @brief 回测结束事件回调
	 * 只在回测模式下才会触发
	 */
	virtual void on_bactest_end() {};

	/**
	 * @brief K线闭合回调
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) = 0;
	
	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 标准化合约代码
	 * @param newTick 更新的Tick数据
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick){}
	
	/**
	 * @brief 策略定时调度回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 */
	virtual void on_strategy_schedule(uint32_t curDate, uint32_t curTime) {}

	/**
	 * @brief 枚举持仓
	 * @param cb 持仓枚举回调函数
	 */
	virtual void enum_position(FuncEnumSelPositionCallBack cb) = 0;

	// 策略接口
	/**
	 * @brief 获取策略持仓
	 * @param stdCode 标准化合约代码
	 * @param bOnlyValid 是否只获取可用持仓
	 * @param userTag 用户标记
	 * @return 持仓数量
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, const char* userTag = "") = 0;
	
	/**
	 * @brief 设置策略目标持仓
	 * @param stdCode 标准化合约代码
	 * @param qty 目标持仓数量
	 * @param userTag 用户标记
	 */
	virtual void stra_set_position(const char* stdCode, double qty, const char* userTag = "") = 0;

	/**
	 * @brief 获取最新价格
	 * @param stdCode 标准化合约代码
	 * @return 最新价格
	 */
	virtual double stra_get_price(const char* stdCode) = 0;

	/**
	 * @brief 读取当日价格
	 * @param stdCode 合约代码
	 * @param flag 价格标记：0-开盘价，1-最高价，2-最低价，3-收盘价/最新价
	 * @return 对应价格
	 */
	virtual double stra_get_day_price(const char* stdCode, int flag = 0) = 0;

	/**
	 * @brief 获取交易日期
	 * @return 交易日期
	 */
	virtual uint32_t stra_get_tdate() = 0;
	
	/**
	 * @brief 获取当前日期
	 * @return 当前日期
	 */
	virtual uint32_t stra_get_date() = 0;
	
	/**
	 * @brief 获取当前时间
	 * @return 当前时间
	 */
	virtual uint32_t stra_get_time() = 0;

	/**
	 * @brief 获取资金数据
	 * @param flag 资金数据类型标记
	 * @return 资金数据值
	 */
	virtual double stra_get_fund_data(int flag = 0) = 0;

	/**
	 * @brief 获取首次入场时间
	 * @param stdCode 标准化合约代码
	 * @return 首次入场时间戳
	 */
	virtual uint64_t stra_get_first_entertime(const char* stdCode) = 0;
	
	/**
	 * @brief 获取最后入场时间
	 * @param stdCode 标准化合约代码
	 * @return 最后入场时间戳
	 */
	virtual uint64_t stra_get_last_entertime(const char* stdCode) = 0;
	
	/**
	 * @brief 获取最后出场时间
	 * @param stdCode 标准化合约代码
	 * @return 最后出场时间戳
	 */
	virtual uint64_t stra_get_last_exittime(const char* stdCode) = 0;
	
	/**
	 * @brief 获取最后入场价格
	 * @param stdCode 标准化合约代码
	 * @return 最后入场价格
	 */
	virtual double stra_get_last_enterprice(const char* stdCode) = 0;
	
	/**
	 * @brief 获取最后入场标记
	 * @param stdCode 标准化合约代码
	 * @return 最后入场标记字符串
	 */
	virtual const char* stra_get_last_entertag(const char* stdCode)  = 0;
	
	/**
	 * @brief 获取持仓均价
	 * @param stdCode 标准化合约代码
	 * @return 持仓均价
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) = 0;
	
	/**
	 * @brief 获取持仓盈亏
	 * @param stdCode 标准化合约代码
	 * @return 持仓盈亏
	 */
	virtual double stra_get_position_profit(const char* stdCode) = 0;

	/**
	 * @brief 获取明细入场时间
	 * @param stdCode 标准化合约代码
	 * @param userTag 用户标记
	 * @return 明细入场时间戳
	 */
	virtual uint64_t stra_get_detail_entertime(const char* stdCode, const char* userTag) = 0;
	
	/**
	 * @brief 获取明细成本
	 * @param stdCode 标准化合约代码
	 * @param userTag 用户标记
	 * @return 明细成本
	 */
	virtual double stra_get_detail_cost(const char* stdCode, const char* userTag) = 0;

	/**
	 * @brief 读取持仓明细的浮盈
	 * @param stdCode 合约代码
	 * @param userTag 下单标记
	 * @param flag 浮盈标志：0-浮动盈亏，1-最大浮盈，2-最高浮动价格，-1-最大浮亏，-2-最小浮动价格
	 * @return 浮盈值
	 */
	virtual double stra_get_detail_profit(const char* stdCode, const char* userTag, int flag = 0) = 0;

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准化合约代码
	 * @return 商品信息对象指针
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) = 0;
	
	/**
	 * @brief 获取交易时间模板信息
	 * @param stdCode 标准化合约代码
	 * @return 交易时间模板信息对象指针
	 */
	virtual WTSSessionInfo* stra_get_sessinfo(const char* stdCode) = 0;
	
	/**
	 * @brief 获取K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param count 数据条数
	 * @return K线数据切片对象指针
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count) = 0;
	
	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return Tick数据切片对象指针
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) = 0;
	
	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准化合约代码
	 * @return 最新Tick数据对象指针
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) = 0;

	/**
	 * @brief 获取分月合约代码
	 * @param stdCode 标准化合约代码
	 * @return 分月合约代码字符串
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) = 0;

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准化合约代码
	 */
	virtual void stra_sub_ticks(const char* stdCode) = 0;

	/**
	 * @brief 记录信息日志
	 * @param message 日志消息
	 */
	virtual void stra_log_info(const char* message) = 0;
	
	/**
	 * @brief 记录调试日志
	 * @param message 日志消息
	 */
	virtual void stra_log_debug(const char* message) = 0;
	
	/**
	 * @brief 记录错误日志
	 * @param message 日志消息
	 */
	virtual void stra_log_error(const char* message) = 0;
	
	/**
	 * @brief 记录警告日志
	 * @param message 日志消息
	 */
	virtual void stra_log_warn(const char* message) {}

	/**
	 * @brief 保存用户数据
	 * @param key 数据键
	 * @param val 数据值
	 */
	virtual void stra_save_user_data(const char* key, const char* val){}

	/**
	 * @brief 加载用户数据
	 * @param key 数据键
	 * @param defVal 默认值
	 * @return 数据值字符串
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") { return defVal; }

protected:
	std::string _name;    // 上下文名称
};

NS_WTP_END