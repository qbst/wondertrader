/*!
 * \file CtaMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略回测模拟器头文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * CtaMocker是WonderTrader回测框架中用于CTA（Commodity Trading Advisor）策略回测的核心模拟器类。
 * 
 * 设计目标：
 * 1. 模拟CTA策略在历史数据上的交易执行过程
 * 2. 提供完整的策略执行环境，包括仓位管理、资金管理、成交撮合等
 * 3. 支持条件单、限价单、止损单等多种订单类型
 * 4. 记录完整的回测结果，包括成交记录、持仓明细、资金曲线等
 * 5. 支持增量回测，可以从上次回测结果继续回测
 * 
 * 核心功能：
 * - 继承ICtaStraCtx接口，为策略提供交易上下文环境
 * - 继承IDataSink接口，接收历史数据回放器推送的市场数据
 * - 管理策略持仓，支持多空双向持仓和持仓明细追踪
 * - 处理策略发出的交易信号，包括开多、开空、平多、平空等操作
 * - 支持条件单触发机制，当价格满足条件时自动执行交易
 * - 计算和更新持仓盈亏、资金曲线等统计信息
 * - 支持滑点模拟、手续费计算等交易成本模拟
 * - 支持图表数据输出，包括K线数据、指标数据、交易标记等
 * 
 * 架构特点：
 * - 采用事件驱动模式，通过回调函数响应市场数据变化
 * - 支持同步和异步两种回测模式，通过钩子机制实现异步控制
 * - 采用模板方法模式，策略只需实现特定接口即可参与回测
 * - 使用策略工厂模式动态加载策略模块
 */
#pragma once

// 标准库头文件
#include <sstream>              // 字符串流，用于日志记录
#include <atomic>               // 原子操作，用于多线程安全
#include <unordered_map>        // 无序映射表，用于快速查找

// 项目内部头文件
#include "HisDataReplayer.h"    // 历史数据回放器

// 框架核心接口定义
#include "../Includes/FasterDefs.h"          // 快速定义，包含常用数据结构和宏定义
#include "../Includes/ICtaStraCtx.h"        // CTA策略上下文接口
#include "../Includes/CtaStrategyDefs.h"    // CTA策略相关定义
#include "../Includes/WTSDataDef.hpp"       // WonderTrader数据定义
#include "../Includes/WTSCollection.hpp"    // WonderTrader集合类定义

// 共享工具库
#include "../Share/DLLHelper.hpp"           // 动态库加载助手
#include "../Share/StdUtils.hpp"            // 标准工具类
#include "../Share/fmtlib.h"                // 格式化库

// WonderTrader命名空间开始
NS_WTP_BEGIN
class EventNotifier;                        // 事件通知器前向声明
NS_WTP_END

USING_NS_WTP;                                // 使用WonderTrader命名空间

// 前向声明
class HisDataReplayer;                      // 历史数据回放器类
class CtaStrategy;                          // CTA策略类

// ====================================================================
// 条件单动作类型常量定义
// ====================================================================
const char COND_ACTION_OL = 0;              // 条件单动作：开多（Open Long）
const char COND_ACTION_CL = 1;               // 条件单动作：平多（Close Long）
const char COND_ACTION_OS = 2;               // 条件单动作：开空（Open Short）
const char COND_ACTION_CS = 3;               // 条件单动作：平空（Close Short）
const char COND_ACTION_SP = 4;               // 条件单动作：直接设置仓位（Set Position）

/**
 * @brief 条件单委托结构体
 * 
 * 用于存储条件单的详细信息，当市场价格满足特定条件时触发执行
 */
typedef struct _CondEntrust
{
	WTSCompareField _field;                  // 比较字段类型（如最新价、开盘价等）
	WTSCompareType	_alg;                    // 比较算法类型（等于、大于、小于等）
	double			_target;                 // 目标价格，用于条件判断

	double			_qty;                    // 委托数量

	char			_action;                 // 委托动作：0-开多,1-平多,2-开空,3-平空,4-设置仓位

	char			_code[MAX_INSTRUMENT_LENGTH];  // 合约代码
	char			_usertag[32];            // 用户标签，用于标识交易信号

	/**
	 * @brief 构造函数，初始化结构体为0
	 */
	_CondEntrust()
	{
		memset(this, 0, sizeof(_CondEntrust));
	}

} CondEntrust;

typedef std::vector<CondEntrust>	CondList;                                    // 条件单列表类型
typedef wt_hashmap<std::string, CondList>	CondEntrustMap;                     // 条件单映射表类型（合约代码 -> 条件单列表）


/**
 * @brief CTA策略回测模拟器类
 * 
 * 该类是CTA策略回测的核心实现，负责模拟策略在历史数据上的交易执行过程。
 * 
 * 继承关系：
 * - ICtaStraCtx: 提供策略执行所需的上下文环境接口
 * - IDataSink: 接收历史数据回放器推送的市场数据
 * 
 * 工作流程：
 * 1. 初始化：加载策略模块，创建策略实例
 * 2. 数据接收：通过IDataSink接口接收tick数据、K线数据等
 * 3. 策略调度：在K线收盘或定时调度时触发策略计算
 * 4. 信号处理：处理策略发出的交易信号，包括即时成交和条件单
 * 5. 仓位管理：维护持仓明细，计算盈亏和资金曲线
 * 6. 结果输出：回测结束后输出成交记录、持仓明细、资金曲线等
 */
class CtaMocker : public ICtaStraCtx, public IDataSink
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param replayer 历史数据回放器指针，用于获取历史数据和市场信息
	 * @param name 策略名称，用于标识和日志输出
	 * @param slippage 滑点设置，单位：如果是比例滑点则为万分比，否则为价格跳动单位
	 * @param persistData 是否持久化回测结果数据
	 * @param notifier 事件通知器指针，用于通知回测进度和结果
	 * @param isRatioSlp 是否为比例滑点模式
	 */
	CtaMocker(HisDataReplayer* replayer, const char* name, int32_t slippage = 0, bool persistData = true, EventNotifier* notifier = NULL, bool isRatioSlp = false);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放策略实例和动态库
	 */
	virtual ~CtaMocker();

private:
	/**
	 * @brief 输出回测结果到CSV文件
	 * 
	 * 将成交记录、平仓记录、资金曲线、信号记录、持仓记录等输出到CSV文件
	 */
	void	dump_outputs();
	
	/**
	 * @brief 输出策略数据到JSON文件
	 * 
	 * 将持仓数据、资金信息、信号信息、条件单信息等保存到JSON文件
	 */
	void	dump_stradata();
	
	/**
	 * @brief 输出图表数据到JSON和CSV文件
	 * 
	 * 将K线配置、指标配置、指标数据、交易标记等保存到文件
	 */
	void	dump_chartdata();
	
	/**
	 * @brief 记录交易信号日志
	 * 
	 * @param stdCode 合约代码
	 * @param target 目标仓位
	 * @param price 信号价格
	 * @param gentime 信号生成时间
	 * @param usertag 用户标签
	 */
	inline void log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag = "");
	
	/**
	 * @brief 记录成交日志
	 * 
	 * @param stdCode 合约代码
	 * @param isLong 是否多头
	 * @param isOpen 是否开仓
	 * @param curTime 成交时间
	 * @param price 成交价格
	 * @param qty 成交数量
	 * @param userTag 用户标签
	 * @param fee 手续费
	 * @param barNo K线编号
	 */
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag = "", double fee = 0.0, uint32_t barNo = 0);
	
	/**
	 * @brief 记录平仓日志
	 * 
	 * @param stdCode 合约代码
	 * @param isLong 是否多头
	 * @param openTime 开仓时间
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间
	 * @param closepx 平仓价格
	 * @param qty 平仓数量
	 * @param profit 盈亏
	 * @param maxprofit 最大盈利
	 * @param maxloss 最大亏损
	 * @param totalprofit 累计盈亏
	 * @param enterTag 开仓标签
	 * @param exitTag 平仓标签
	 * @param openBarNo 开仓K线编号
	 * @param closeBarNo 平仓K线编号
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double maxprofit, double maxloss, double totalprofit = 0, const char* enterTag = "", const char* exitTag = "", uint32_t openBarNo = 0, uint32_t closeBarNo = 0);

	/**
	 * @brief 更新持仓的动态盈亏
	 * 
	 * 根据最新价格计算持仓的浮动盈亏，并更新最大盈利和最大亏损
	 * 
	 * @param stdCode 合约代码
	 * @param price 最新价格
	 */
	void	update_dyn_profit(const char* stdCode, double price);

	/**
	 * @brief 执行仓位设置操作
	 * 
	 * 根据目标仓位和当前仓位的差异，执行开仓、平仓或反手操作
	 * 
	 * @param stdCode 合约代码
	 * @param qty 目标仓位
	 * @param price 成交价格（如果为0则使用当前价格）
	 * @param userTag 用户标签
	 */
	void	do_set_position(const char* stdCode, double qty, double price = 0.0, const char* userTag = "");
	
	/**
	 * @brief 添加交易信号
	 * 
	 * 将策略发出的交易信号添加到信号映射表中，等待下一个tick到来时执行
	 * 
	 * @param stdCode 合约代码
	 * @param qty 目标仓位
	 * @param userTag 用户标签
	 * @param price 指定价格（如果为0则使用市场价格）
	 * @param sigType 信号类型：0-调度中发出，1-非调度中发出，2-条件单触发
	 */
	void	append_signal(const char* stdCode, double qty, const char* userTag, double price, uint32_t sigType);

	/**
	 * @brief 获取指定合约的条件单列表
	 * 
	 * @param stdCode 合约代码
	 * @return 条件单列表的引用
	 */
	inline CondList& get_cond_entrusts(const char* stdCode);

	/**
	 * @brief 处理tick数据
	 * 
	 * 检查是否有待执行的信号，更新持仓盈亏，检查条件单是否触发
	 * 
	 * @param stdCode 合约代码
	 * @param last_px 上一笔价格
	 * @param cur_px 当前价格
	 */
	void	proc_tick(const char* stdCode, double last_px, double cur_px);

public:
	/**
	 * @brief 初始化CTA策略工厂
	 * 
	 * 加载策略动态库，创建策略工厂实例和策略实例
	 * 
	 * @param cfg 配置对象，包含策略模块路径和策略参数
	 * @return 是否初始化成功
	 */
	bool	init_cta_factory(WTSVariant* cfg);
	
	/**
	 * @brief 加载增量回测数据
	 * 
	 * 从上次回测的结果文件中加载持仓、资金、信号等数据，用于增量回测
	 * 
	 * @param lastBacktestName 上次回测的策略名称
	 */
	void	load_incremental_data(const char* lastBacktestName);
	
	/**
	 * @brief 安装计算钩子
	 * 
	 * 启用异步回测模式的计算钩子，用于控制回测执行流程
	 */
	void	install_hook();
	
	/**
	 * @brief 启用或禁用计算钩子
	 * 
	 * @param bEnabled 是否启用
	 */
	void	enable_hook(bool bEnabled = true);
	
	/**
	 * @brief 单步计算
	 * 
	 * 在异步回测模式下，等待一次策略计算完成
	 * 
	 * @return 是否还在回测中
	 */
	bool	step_calc();

public:
	//////////////////////////////////////////////////////////////////////////
	// IDataSink接口实现 - 接收历史数据回放器推送的市场数据
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 处理tick数据推送
	 * 
	 * 当历史数据回放器推送tick数据时调用，检查信号执行、更新持仓盈亏、检查条件单触发
	 * 
	 * @param stdCode 合约代码
	 * @param curTick 当前tick数据
	 * @param pxType 价格类型：0-普通tick，3-收盘价模拟的tick
	 */
	virtual void	handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType = 0) override;
	
	/**
	 * @brief 处理K线收盘事件
	 * 
	 * 当K线收盘时调用，标记K线已收盘，并触发策略的on_bar_close回调
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 */
	virtual void	handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 处理定时调度事件
	 * 
	 * 在设定的时间点触发策略计算，这是策略执行的主要入口
	 * 
	 * @param uDate 当前日期
	 * @param uTime 当前时间
	 */
	virtual void	handle_schedule(uint32_t uDate, uint32_t uTime) override;

	/**
	 * @brief 处理初始化事件
	 * 
	 * 回测开始时调用，初始化策略
	 */
	virtual void	handle_init() override;
	
	/**
	 * @brief 处理交易日开始事件
	 * 
	 * 每个交易日开始时调用，清理冻结仓位，触发策略的on_session_begin回调
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void	handle_session_begin(uint32_t curTDate) override;
	
	/**
	 * @brief 处理交易日结束事件
	 * 
	 * 每个交易日结束时调用，输出资金日志，触发策略的on_session_end回调
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void	handle_session_end(uint32_t curTDate) override;

	/**
	 * @brief 处理小节结束事件
	 * 
	 * 交易小节结束时调用，清理价格缓存，防止小节跳空
	 * 
	 * @param curTDate 当前交易日
	 * @param curTime 当前时间
	 */
	virtual void	handle_section_end(uint32_t curTDate, uint32_t curTime) override;

	/**
	 * @brief 处理回放完成事件
	 * 
	 * 历史数据回放完成时调用，输出回测结果，清理资源
	 */
	virtual void	handle_replay_done() override;

	//////////////////////////////////////////////////////////////////////////
	// ICtaStraCtx接口实现 - 为策略提供交易上下文环境
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 获取上下文ID
	 * 
	 * @return 上下文唯一标识符
	 */
	virtual uint32_t id() { return _context_id; }

	// ====================================================================
	// 策略生命周期回调函数
	// ====================================================================
	
	/**
	 * @brief 策略初始化回调
	 * 
	 * 策略初始化时调用，通知策略开始回测
	 */
	virtual void on_init() override;
	
	/**
	 * @brief 交易日开始回调
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void on_session_begin(uint32_t curTDate) override;
	
	/**
	 * @brief 交易日结束回调
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void on_session_end(uint32_t curTDate) override;
	
	/**
	 * @brief tick数据回调（已废弃）
	 * 
	 * 该函数逻辑已迁移到handle_tick中
	 * 
	 * @param stdCode 合约代码
	 * @param newTick 新的tick数据
	 * @param bEmitStrategy 是否触发策略（已废弃）
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy = true) override;
	
	/**
	 * @brief K线数据回调
	 * 
	 * 当K线收盘时调用，标记K线状态
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 定时调度回调
	 * 
	 * 在设定的时间点触发策略计算
	 * 
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * @return 是否触发了策略计算
	 */
	virtual bool on_schedule(uint32_t curDate, uint32_t curTime) override;
	
	/**
	 * @brief 枚举持仓
	 * 
	 * 遍历所有持仓，调用回调函数
	 * 
	 * @param cb 回调函数
	 * @param bForExecute 是否用于执行（暂未使用）
	 */
	virtual void enum_position(FuncEnumCtaPosCallBack cb, bool bForExecute) override;

	/**
	 * @brief tick数据更新回调
	 * 
	 * 当订阅的tick数据更新时调用，通知策略
	 * 
	 * @param stdCode 合约代码
	 * @param newTick 新的tick数据
	 */
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;
	
	/**
	 * @brief K线收盘回调
	 * 
	 * 当订阅的K线收盘时调用，通知策略
	 * 
	 * @param code 合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 策略计算回调
	 * 
	 * 在定时调度时调用，触发策略的on_schedule方法
	 * 
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 */
	virtual void on_calculate(uint32_t curDate, uint32_t curTime) override;


	//////////////////////////////////////////////////////////////////////////
	// 策略接口 - 策略可通过这些接口进行交易操作和查询信息
	//////////////////////////////////////////////////////////////////////////
	
	// ====================================================================
	// 交易操作接口
	// ====================================================================
	
	/**
	 * @brief 开多仓
	 * 
	 * 如果当前有空仓，则先平空再开多；如果当前有多仓，则增加多仓
	 * 
	 * @param stdCode 合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价（0表示市价）
	 * @param stopprice 止损价（0表示不限价）
	 */
	virtual void stra_enter_long(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 开空仓
	 * 
	 * 如果当前有多仓，则先平多再开空；如果当前有空仓，则增加空仓
	 * 
	 * @param stdCode 合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价（0表示市价）
	 * @param stopprice 止损价（0表示不限价）
	 */
	virtual void stra_enter_short(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 平多仓
	 * 
	 * 平掉指定数量的多仓，如果数量超过当前多仓，则全部平掉
	 * 
	 * @param stdCode 合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价（0表示市价）
	 * @param stopprice 止损价（0表示不限价）
	 */
	virtual void stra_exit_long(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 平空仓
	 * 
	 * 平掉指定数量的空仓，如果数量超过当前空仓，则全部平掉
	 * 
	 * @param stdCode 合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签
	 * @param limitprice 限价（0表示市价）
	 * @param stopprice 止损价（0表示不限价）
	 */
	virtual void stra_exit_short(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;

	/**
	 * @brief 获取持仓数量
	 * 
	 * @param stdCode 合约代码
	 * @param bOnlyValid 是否只返回有效持仓（排除T+1冻结的持仓）
	 * @param userTag 用户标签，如果指定则只返回该标签的持仓
	 * @return 持仓数量（正数表示多仓，负数表示空仓）
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, const char* userTag = "") override;
	
	/**
	 * @brief 设置目标仓位
	 * 
	 * 直接设置目标仓位，系统会自动计算需要开仓或平仓的数量
	 * 
	 * @param stdCode 合约代码
	 * @param qty 目标仓位（正数表示多仓，负数表示空仓）
	 * @param userTag 用户标签
	 * @param limitprice 限价（0表示市价）
	 * @param stopprice 止损价（0表示不限价）
	 */
	virtual void stra_set_position(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 获取最新价格
	 * 
	 * @param stdCode 合约代码
	 * @return 最新价格
	 */
	virtual double stra_get_price(const char* stdCode) override;

	/**
	 * @brief 读取当日价格
	 * 
	 * @param stdCode 合约代码
	 * @param flag 价格类型：0-最新价，1-开盘价，2-最高价，3-最低价，4-收盘价
	 * @return 当日价格
	 */
	virtual double stra_get_day_price(const char* stdCode, int flag = 0) override;

	// ====================================================================
	// 时间信息接口
	// ====================================================================
	
	/**
	 * @brief 获取当前交易日
	 * 
	 * @return 交易日（格式：YYYYMMDD）
	 */
	virtual uint32_t stra_get_tdate() override;
	
	/**
	 * @brief 获取当前日期
	 * 
	 * @return 日期（格式：YYYYMMDD）
	 */
	virtual uint32_t stra_get_date() override;
	
	/**
	 * @brief 获取当前时间
	 * 
	 * @return 时间（格式：HHMM）
	 */
	virtual uint32_t stra_get_time() override;

	// ====================================================================
	// 资金信息接口
	// ====================================================================
	
	/**
	 * @brief 获取资金数据
	 * 
	 * @param flag 数据类型：0-总权益，1-已实现盈亏，2-浮动盈亏，3-手续费
	 * @return 资金数据
	 */
	virtual double stra_get_fund_data(int flag = 0) override;

	// ====================================================================
	// 持仓信息接口
	// ====================================================================
	
	/**
	 * @brief 获取首次开仓时间
	 * 
	 * @param stdCode 合约代码
	 * @return 首次开仓时间（时间戳）
	 */
	virtual uint64_t stra_get_first_entertime(const char* stdCode) override;
	
	/**
	 * @brief 获取最后开仓时间
	 * 
	 * @param stdCode 合约代码
	 * @return 最后开仓时间（时间戳）
	 */
	virtual uint64_t stra_get_last_entertime(const char* stdCode) override;
	
	/**
	 * @brief 获取最后平仓时间
	 * 
	 * @param stdCode 合约代码
	 * @return 最后平仓时间（时间戳）
	 */
	virtual uint64_t stra_get_last_exittime(const char* stdCode) override;
	
	/**
	 * @brief 获取最后开仓价格
	 * 
	 * @param stdCode 合约代码
	 * @return 最后开仓价格
	 */
	virtual double stra_get_last_enterprice(const char* stdCode) override;
	
	/**
	 * @brief 获取最后开仓标签
	 * 
	 * @param stdCode 合约代码
	 * @return 最后开仓标签
	 */
	virtual const char* stra_get_last_entertag(const char* stdCode) override;
	
	/**
	 * @brief 获取持仓均价
	 * 
	 * @param stdCode 合约代码
	 * @return 持仓均价
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) override;
	
	/**
	 * @brief 获取持仓盈亏
	 * 
	 * @param stdCode 合约代码
	 * @return 持仓浮动盈亏
	 */
	virtual double stra_get_position_profit(const char* stdCode) override;

	/**
	 * @brief 获取指定标签的持仓开仓时间
	 * 
	 * @param stdCode 合约代码
	 * @param userTag 用户标签
	 * @return 开仓时间（时间戳）
	 */
	virtual uint64_t stra_get_detail_entertime(const char* stdCode, const char* userTag) override;
	
	/**
	 * @brief 获取指定标签的持仓成本
	 * 
	 * @param stdCode 合约代码
	 * @param userTag 用户标签
	 * @return 持仓成本价格
	 */
	virtual double stra_get_detail_cost(const char* stdCode, const char* userTag) override;
	
	/**
	 * @brief 获取指定标签的持仓盈亏
	 * 
	 * @param stdCode 合约代码
	 * @param userTag 用户标签
	 * @param flag 盈亏类型：0-当前盈亏，1-最大盈利，-1-最大亏损，2-最高价，-2-最低价
	 * @return 持仓盈亏
	 */
	virtual double stra_get_detail_profit(const char* stdCode, const char* userTag, int flag = 0) override;

	// ====================================================================
	// 市场数据接口
	// ====================================================================
	
	/**
	 * @brief 获取合约信息
	 * 
	 * @param stdCode 合约代码
	 * @return 合约信息对象指针
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;
	
	/**
	 * @brief 获取K线数据切片
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期（如"m5"表示5分钟K线）
	 * @param count 获取的K线数量
	 * @param isMain 是否为主K线（用于触发策略调度）
	 * @return K线数据切片指针
	 */
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count, bool isMain = false) override;
	
	/**
	 * @brief 获取tick数据切片
	 * 
	 * @param stdCode 合约代码
	 * @param count 获取的tick数量
	 * @return tick数据切片指针
	 */
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) override;
	
	/**
	 * @brief 获取最新tick数据
	 * 
	 * @param stdCode 合约代码
	 * @return 最新tick数据指针
	 */
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) override;

	/**
	 * @brief 订阅tick数据
	 * 
	 * 订阅后，当tick数据更新时会触发on_tick回调
	 * 
	 * @param stdCode 合约代码
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;
	
	/**
	 * @brief 订阅K线收盘事件
	 * 
	 * 订阅后，当K线收盘时会触发on_bar_close回调
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期
	 */
	virtual void stra_sub_bar_events(const char* stdCode, const char* period) override;

	/**
	 * @brief 获取分月合约代码
	 * 
	 * 将标准化合约代码转换为分月合约代码
	 * 
	 * @param stdCode 标准化合约代码
	 * @return 分月合约代码
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) override;

	// ====================================================================
	// 日志接口
	// ====================================================================
	
	/**
	 * @brief 记录信息日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_info(const char* message) override;
	
	/**
	 * @brief 记录调试日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_debug(const char* message) override;
	
	/**
	 * @brief 记录警告日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_warn(const char* message) override;
	
	/**
	 * @brief 记录错误日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_error(const char* message) override;

	// ====================================================================
	// 用户数据接口
	// ====================================================================
	
	/**
	 * @brief 保存用户数据
	 * 
	 * 保存的数据会在回测结果中持久化
	 * 
	 * @param key 数据键
	 * @param val 数据值
	 */
	virtual void stra_save_user_data(const char* key, const char* val) override;
	
	/**
	 * @brief 加载用户数据
	 * 
	 * @param key 数据键
	 * @param defVal 默认值（如果数据不存在）
	 * @return 数据值
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") override;

	// ====================================================================
	// 图表数据接口
	// ====================================================================
	
	/**
	 * @brief 设置图表K线
	 * 
	 * 设置用于图表展示的K线周期
	 * 
	 * @param stdCode 合约代码
	 * @param period K线周期
	 */
	virtual void set_chart_kline(const char* stdCode, const char* period) override;

	/**
	 * @brief 添加图表标记
	 * 
	 * 在图表上添加交易标记（如买入、卖出标记）
	 * 
	 * @param price 标记价格
	 * @param icon 标记图标
	 * @param tag 标记标签
	 */
	virtual void add_chart_mark(double price, const char* icon, const char* tag) override;

	/**
	 * @brief 注册指标
	 * 
	 * @param idxName 指标名称
	 * @param indexType 指标类型
	 */
	virtual void register_index(const char* idxName, uint32_t indexType) override;

	/**
	 * @brief 注册指标线
	 * 
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param lineType 线条类型
	 * @return 是否注册成功
	 */
	virtual bool register_index_line(const char* idxName, const char* lineName, uint32_t lineType) override;

	/**
	 * @brief 添加指标基准线
	 * 
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 基准值
	 * @return 是否添加成功
	 */
	virtual bool add_index_baseline(const char* idxName, const char* lineName, double val) override;

	/**
	 * @brief 设置指标值
	 * 
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 指标值
	 * @return 是否设置成功
	 */
	virtual bool set_index_value(const char* idxName, const char* lineName, double val) override;

private:
	// ====================================================================
	// 内部日志模板函数 - 用于格式化日志输出
	// ====================================================================
	
	/**
	 * @brief 格式化调试日志
	 * 
	 * @tparam Args 可变参数类型
	 * @param format 格式字符串
	 * @param args 格式化参数
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_debug(buffer);                                // 调用策略日志接口
	}

	/**
	 * @brief 格式化信息日志
	 * 
	 * @tparam Args 可变参数类型
	 * @param format 格式字符串
	 * @param args 格式化参数
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_info(buffer);                                  // 调用策略日志接口
	}

	/**
	 * @brief 格式化错误日志
	 * 
	 * @tparam Args 可变参数类型
	 * @param format 格式字符串
	 * @param args 格式化参数
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_error(buffer);                                 // 调用策略日志接口
	}

protected:
	// ====================================================================
	// 核心成员变量
	// ====================================================================
	
	uint32_t			_context_id;        // 上下文唯一标识符，用于区分不同的策略实例
	HisDataReplayer*	_replayer;          // 历史数据回放器指针，用于获取历史数据和市场信息

	// ====================================================================
	// 统计信息
	// ====================================================================
	
	uint64_t		_total_calc_time;     // 策略总计算时间（微秒），用于性能统计
	uint32_t		_emit_times;          // 策略总计算次数，用于性能统计

	// ====================================================================
	// 滑点设置
	// ====================================================================
	
	int32_t			_slippage;            // 成交滑点，如果是比例滑点则为万分比，否则为价格跳动单位
	bool			_ratio_slippage;      // 是否为比例滑点模式

	uint32_t		_schedule_times;      // 调度次数，用于标识当前是第几次调度

	// ====================================================================
	// 主K线信息
	// ====================================================================
	
	std::string		_main_key;            // 主K线键值（格式：合约代码#周期）
	std::string		_main_code;           // 主K线合约代码
	std::string		_main_period;         // 主K线周期

	// ====================================================================
	// K线状态管理
	// ====================================================================
	
	/**
	 * @brief K线标签结构体
	 * 
	 * 用于标记K线的收盘状态和通知状态
	 */
	typedef struct _KlineTag
	{
		bool	_closed;                 // K线是否已收盘
		bool	_notify;                 // 是否已通知策略K线收盘

		/**
		 * @brief 构造函数，初始化状态为false
		 */
		_KlineTag() :_closed(false), _notify(false){}

	} KlineTag;
	typedef wt_hashmap<std::string, KlineTag> KlineTags;  // K线标签映射表（键值 -> K线标签）
	KlineTags	_kline_tags;                              // K线标签映射表实例

	// ====================================================================
	// 价格缓存
	// ====================================================================
	
	typedef wt_hashmap<std::string, double> PriceMap;  // 价格映射表类型（合约代码 -> 价格）
	PriceMap		_price_map;                          // 价格映射表实例，用于缓存上一笔价格

	// ====================================================================
	// 持仓明细信息
	// ====================================================================
	
	/**
	 * @brief 持仓明细信息结构体
	 * 
	 * 用于记录每笔持仓的详细信息，支持多笔持仓的平均成本计算
	 */
	typedef struct _DetailInfo
	{
		bool		_long;                // 是否多头持仓（true-多头，false-空头）
		double		_price;                // 开仓价格
		double		_volume;               // 持仓数量
		uint64_t	_opentime;             // 开仓时间（时间戳）
		uint32_t	_opentdate;            // 开仓交易日
		double		_max_profit;           // 持仓期间最大盈利
		double		_max_loss;             // 持仓期间最大亏损
		double		_max_price;            // 持仓期间最高价
		double		_min_price;            // 持仓期间最低价
		double		_profit;               // 当前浮动盈亏
		char		_opentag[32];          // 开仓标签，用于标识不同的交易信号
		uint32_t	_open_barno;           // 开仓K线编号

		/**
		 * @brief 构造函数，初始化所有字段为0
		 */
		_DetailInfo()
		{
			memset(this, 0, sizeof(_DetailInfo));
		}
	} DetailInfo;

	// ====================================================================
	// 持仓信息
	// ====================================================================
	
	/**
	 * @brief 持仓信息结构体
	 * 
	 * 用于记录每个合约的持仓汇总信息
	 */
	typedef struct _PosInfo
	{
		double		_volume;               // 总持仓数量（正数表示多仓，负数表示空仓）
		double		_closeprofit;          // 已实现盈亏
		double		_dynprofit;            // 浮动盈亏
		uint64_t	_last_entertime;       // 最后开仓时间
		uint64_t	_last_exittime;        // 最后平仓时间
		double		_frozen;               // 冻结持仓（T+1规则下，当日开仓的持仓会被冻结）

		std::vector<DetailInfo> _details;  // 持仓明细列表，支持多笔持仓的平均成本计算

		/**
		 * @brief 构造函数，初始化所有字段为0
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
		 * @brief 获取有效持仓数量
		 * 
		 * @return 有效持仓数量（总持仓 - 冻结持仓）
		 */
		inline double valid() const { return _volume - _frozen; }
	} PosInfo;
	typedef wt_hashmap<std::string, PosInfo> PositionMap;  // 持仓映射表类型（合约代码 -> 持仓信息）
	PositionMap		_pos_map;                              // 持仓映射表实例
	double	_total_closeprofit;                           // 累计已实现盈亏

	// ====================================================================
	// 交易信号信息
	// ====================================================================
	
	/**
	 * @brief 交易信号信息结构体
	 * 
	 * 用于记录待执行的交易信号
	 */
	typedef struct _SigInfo
	{
		double		_volume;               // 目标仓位
		std::string	_usertag;              // 用户标签
		double		_sigprice;             // 信号生成时的价格
		double		_desprice;             // 指定成交价格（如果为0则使用市场价格）
		uint32_t	_sigtype;              // 信号类型：0-调度中发出，1-非调度中发出，2-条件单触发
		uint64_t	_gentime;              // 信号生成时间（时间戳）

		/**
		 * @brief 构造函数，初始化所有字段为0
		 */
		_SigInfo()
		{
			_volume = 0;
			_sigprice = 0;
			_desprice = 0;
			_sigtype = 0;
			_gentime = 0;
		}
	}SigInfo;
	typedef wt_hashmap<std::string, SigInfo>	SignalMap;  // 信号映射表类型（合约代码 -> 信号信息）
	SignalMap		_sig_map;                              // 信号映射表实例

	// ====================================================================
	// 日志流
	// ====================================================================
	
	std::stringstream	_trade_logs;       // 成交日志流，用于记录所有成交记录
	std::stringstream	_close_logs;       // 平仓日志流，用于记录所有平仓记录
	std::stringstream	_fund_logs;        // 资金日志流，用于记录每日资金曲线
	std::stringstream	_sig_logs;         // 信号日志流，用于记录所有交易信号
	std::stringstream	_pos_logs;         // 持仓日志流，用于记录每日持仓情况
	std::stringstream	_index_logs;       // 指标日志流，用于记录指标数据
	std::stringstream	_mark_logs;        // 标记日志流，用于记录图表标记

	CondEntrustMap		_condtions;        // 条件单映射表（合约代码 -> 条件单列表）

	// ====================================================================
	// 调度状态
	// ====================================================================
	
	bool			_is_in_schedule;     // 是否在自动调度中，用于标识当前是否在策略计算过程中

	// ====================================================================
	// 用户数据
	// ====================================================================
	
	typedef wt_hashmap<std::string, std::string> StringHashMap;  // 字符串映射表类型
	StringHashMap	_user_datas;                                 // 用户数据映射表（键 -> 值），用于策略存储自定义数据
	bool			_ud_modified;                                 // 用户数据是否被修改，用于判断是否需要持久化

	// ====================================================================
	// 资金信息
	// ====================================================================
	
	/**
	 * @brief 策略资金信息结构体
	 * 
	 * 用于记录策略的资金统计信息
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;          // 累计已实现盈亏
		double	_total_dynprofit;       // 累计浮动盈亏
		double	_total_fees;             // 累计手续费

		/**
		 * @brief 构造函数，初始化所有字段为0
		 */
		_StraFundInfo()
		{
			memset(this, 0, sizeof(_StraFundInfo));
		}
	} StraFundInfo;
	StraFundInfo		_fund_info;        // 策略资金信息实例

	// ====================================================================
	// 策略工厂和策略实例
	// ====================================================================
	
	/**
	 * @brief 策略工厂信息结构体
	 * 
	 * 用于管理策略动态库的加载和策略实例的创建
	 */
	typedef struct _StraFactInfo
	{
		std::string		_module_path;      // 策略模块路径
		DllHandle		_module_inst;      // 动态库句柄
		ICtaStrategyFact*	_fact;        // 策略工厂指针
		FuncCreateStraFact	_creator;      // 创建策略工厂的函数指针
		FuncDeleteStraFact	_remover;      // 删除策略工厂的函数指针

		/**
		 * @brief 构造函数，初始化指针为NULL
		 */
		_StraFactInfo()
		{
			_module_inst = NULL;
			_fact = NULL;
		}

		/**
		 * @brief 析构函数，释放策略工厂
		 */
		~_StraFactInfo()
		{
			if (_fact)
				_remover(_fact);         // 调用删除函数释放策略工厂
		}
	} StraFactInfo;
	StraFactInfo	_factory;            // 策略工厂信息实例

	CtaStrategy*	_strategy;            // CTA策略实例指针
	EventNotifier*	_notifier;           // 事件通知器指针，用于通知回测进度和结果

	// ====================================================================
	// 异步回测控制
	// ====================================================================
	
	StdUniqueMutex	_mtx_calc;           // 计算互斥锁，用于异步回测模式下的线程同步
	StdCondVariable	_cond_calc;           // 计算条件变量，用于异步回测模式下的线程等待和通知
	bool			_has_hook;            // 是否安装了计算钩子（人为控制是否启用钩子）
	bool			_hook_valid;          // 钩子是否有效（根据是否是异步回测模式而确定钩子是否可用）
	std::atomic<uint32_t>		_cur_step;  // 当前步骤，用于控制异步回测的状态机（0-初始，1-计算中，2-计算完成，3-计算完成确认）

	bool			_in_backtest;         // 是否在回测中
	bool			_wait_calc;           // 是否等待计算完成

	// ====================================================================
	// 数据持久化
	// ====================================================================
	
	bool			_persist_data;        // 是否对回测结果持久化

	// ====================================================================
	// 时间信息
	// ====================================================================
	
	uint32_t		_cur_tdate;           // 当前交易日
	uint32_t		_cur_bartime;         // 当前K线时间
	uint64_t		_last_cond_min;       // 最后设置条件单的时间（分钟）

	// ====================================================================
	// tick订阅
	// ====================================================================
	
	wt_hashset<std::string> _tick_subs;  // tick订阅列表，记录已订阅tick数据的合约代码

	// ====================================================================
	// 图表配置
	// ====================================================================
	
	std::string		_chart_code;         // 图表K线合约代码
	std::string		_chart_period;       // 图表K线周期

	/**
	 * @brief 图表线条结构体
	 * 
	 * 用于记录指标线条的配置信息
	 */
	typedef struct _ChartLine
	{
		std::string	_name;                // 线条名称
		uint32_t	_lineType;            // 线条类型
	} ChartLine;

	/**
	 * @brief 图表指标结构体
	 * 
	 * 用于记录指标的配置信息和数据
	 */
	typedef struct _ChartIndex
	{
		std::string	_name;                // 指标名称
		uint32_t	_indexType;           // 指标类型
		std::unordered_map<std::string, ChartLine> _lines;      // 指标线条映射表（线条名称 -> 线条信息）
		std::unordered_map<std::string, double> _base_lines;    // 基准线映射表（线条名称 -> 基准值）
	} ChartIndex;

	std::unordered_map<std::string, ChartIndex>	_chart_indice;  // 图表指标映射表（指标名称 -> 指标信息）

	// ====================================================================
	// tick缓存
	// ====================================================================
	
	typedef wt_hashmap<std::string, WTSTickStruct>	TickCache;  // tick缓存映射表类型（合约代码 -> tick结构）
	TickCache	_ticks;                                          // tick缓存映射表实例，用于缓存最新的tick数据
};