/*!
 * \file CtaStraBaseCtx.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略基础上下文头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中CTA策略的基础上下文类，实现了ICtaStraCtx接口的核心功能。
 * 主要功能包括：
 * 1. 策略运行环境管理：提供策略运行所需的数据、状态和接口
 * 2. 持仓管理：维护理论持仓、持仓明细、持仓盈亏等完整持仓信息
 * 3. 交易信号处理：处理开多、开空、平多、平空等交易信号，支持条件单
 * 4. 数据持久化：保存和加载策略的持仓、资金、条件单等数据
 * 5. 市场数据访问：提供K线、Tick、价格等市场数据查询接口
 * 6. 日志记录：记录交易、平仓、信号、持仓、资金等关键信息
 * 7. 图表支持：支持策略图表、指标、标记等功能
 * 8. 用户数据存储：支持策略自定义数据的持久化存储
 * 
 * 该类是CTA策略运行的核心基础类，为策略提供完整的运行环境和交易接口。
 * 通过继承ICtaStraCtx接口，实现了策略所需的所有基础功能。
 */
#pragma once
#include "../Includes/ICtaStraCtx.h"      // 包含CTA策略上下文接口定义
#include "../Includes/FasterDefs.h"      // 包含WonderTrader快速定义
#include "../Includes/WTSDataDef.hpp"    // 包含WTS数据结构定义

#include "../Share/BoostFile.hpp"        // 包含Boost文件操作工具
#include "../Share/fmtlib.h"             // 包含格式化字符串工具
#include "../Share/SpinMutex.hpp"       // 包含自旋锁互斥量

#include <unordered_map>                 // 包含无序映射容器

class CtaStrategy;  // 前向声明：CTA策略类

NS_WTP_BEGIN  // 开始WonderTrader命名空间

class WtCtaEngine;  // 前向声明：CTA引擎类

/**
 * @def COND_ACTION_OL
 * @brief 条件单动作类型：开多
 * 
 * 用于条件单中的动作类型标识，表示开多仓操作。
 */
const char COND_ACTION_OL = 0;	// 开多：买入开仓建立多头仓位

/**
 * @def COND_ACTION_CL
 * @brief 条件单动作类型：平多
 * 
 * 用于条件单中的动作类型标识，表示平多仓操作。
 */
const char COND_ACTION_CL = 1;	// 平多：卖出平仓关闭多头仓位

/**
 * @def COND_ACTION_OS
 * @brief 条件单动作类型：开空
 * 
 * 用于条件单中的动作类型标识，表示开空仓操作。
 */
const char COND_ACTION_OS = 2;	// 开空：卖出开仓建立空头仓位

/**
 * @def COND_ACTION_CS
 * @brief 条件单动作类型：平空
 * 
 * 用于条件单中的动作类型标识，表示平空仓操作。
 */
const char COND_ACTION_CS = 3;	// 平空：买入平仓关闭空头仓位

/**
 * @def COND_ACTION_SP
 * @brief 条件单动作类型：直接设置仓位
 * 
 * 用于条件单中的动作类型标识，表示直接设置目标仓位。
 */
const char COND_ACTION_SP = 4;	// 直接设置仓位：不区分方向，直接设置目标仓位数量

/**
 * @struct CondEntrust
 * @brief 条件单委托结构体
 * 
 * 定义条件单的完整信息，包括触发条件、目标仓位、动作类型等。
 * 条件单是一种特殊的交易指令，当市场数据满足特定条件时自动触发交易。
 */
typedef struct _CondEntrust
{
	WTSCompareField _field;      // 比较字段：指定比较的市场数据字段（如最新价、开盘价等）
	WTSCompareType	_alg;        // 比较算法：指定比较方式（等于、大于、小于、大于等于、小于等于）
	double			_target;     // 目标值：比较的目标价格或数值

	double			_qty;        // 数量：交易的目标数量（手数）

	char			_action;	// 动作类型：0-开多, 1-平多, 2-开空, 3-平空, 4-直接设置仓位

	char			_code[MAX_INSTRUMENT_LENGTH];  // 合约代码：标准合约代码字符串
	char			_usertag[32];                  // 用户标签：用户自定义的交易标记，用于标识交易目的

	/**
	 * @brief 构造函数
	 * 
	 * 初始化条件单委托结构体，将所有成员变量清零。
	 */
	_CondEntrust()
	{
		memset(this, 0, sizeof(_CondEntrust));  // 将整个结构体内存清零
	}

} CondEntrust;

/**
 * @typedef CondList
 * @brief 条件单列表类型定义
 * 
 * 使用vector容器存储多个条件单委托，组成一个条件单列表。
 * 一个合约可以同时设置多个条件单。
 */
typedef std::vector<CondEntrust>	CondList;

/**
 * @typedef CondEntrustMap
 * @brief 条件单映射表类型定义
 * 
 * 使用哈希表存储合约代码到条件单列表的映射关系。
 * key为合约代码（字符串），value为对应的条件单列表。
 */
typedef wt_hashmap<std::string, CondList>	CondEntrustMap;


/**
 * @class CtaStraBaseCtx
 * @brief CTA策略基础上下文类
 * 
 * 该类是CTA策略运行的核心基础类，实现了ICtaStraCtx接口的所有功能。
 * 提供策略运行所需的环境、数据访问、交易执行、持仓管理等完整功能。
 * 
 * 主要特性：
 * - 完整的持仓管理：维护理论持仓、持仓明细、持仓盈亏等
 * - 交易信号处理：支持开多、开空、平多、平空等交易操作，支持条件单
 * - 数据持久化：自动保存和加载策略状态，支持断点续传
 * - 市场数据访问：提供K线、Tick、价格等数据查询接口
 * - 日志记录：记录交易、平仓、信号等关键信息到CSV文件
 * - 图表支持：支持策略图表、指标、标记等功能
 * - 用户数据存储：支持策略自定义数据的持久化存储
 */
class CtaStraBaseCtx : public ICtaStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * @param engine CTA引擎指针，用于访问引擎提供的功能
	 * @param name 策略上下文名称，用于标识该策略实例
	 * @param slippage 滑点设置，单位为最小价格变动单位，用于模拟交易成本
	 * 
	 * 初始化CTA策略基础上下文对象，设置引擎指针、名称和滑点参数。
	 */
	CtaStraBaseCtx(WtCtaEngine* engine, const char* name, int32_t slippage);
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理CTA策略基础上下文对象，释放相关资源。
	 */
	virtual ~CtaStraBaseCtx();

private:
	/**
	 * @brief 初始化输出文件
	 * 
	 * 创建策略运行过程中的各种日志文件，包括交易记录、平仓记录、资金记录等。
	 * 如果文件已存在，则追加写入；如果不存在，则创建新文件并写入表头。
	 */
	void	init_outputs();
	
	/**
	 * @brief 记录交易信号
	 * @param stdCode 合约代码
	 * @param target 目标仓位
	 * @param price 信号价格
	 * @param gentime 信号生成时间
	 * @param usertag 用户标签，默认为空字符串
	 * 
	 * 将交易信号记录到信号日志文件中，用于后续分析和回放。
	 */
	inline void log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag = "");
	
	/**
	 * @brief 记录交易成交
	 * @param stdCode 合约代码
	 * @param isLong 是否多头，true表示多头，false表示空头
	 * @param isOpen 是否开仓，true表示开仓，false表示平仓
	 * @param curTime 成交时间
	 * @param price 成交价格
	 * @param qty 成交数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param fee 手续费，默认为0.0
	 * @param barNo K线编号，默认为0
	 * 
	 * 将交易成交信息记录到交易日志文件中，并通知引擎。
	 */
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag = "", double fee = 0.0, uint32_t barNo = 0);
	
	/**
	 * @brief 记录平仓信息
	 * @param stdCode 合约代码
	 * @param isLong 是否多头
	 * @param openTime 开仓时间
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间
	 * @param closepx 平仓价格
	 * @param qty 平仓数量
	 * @param profit 本次平仓盈亏
	 * @param totalprofit 累计平仓盈亏，默认为0
	 * @param enterTag 开仓标签，默认为空字符串
	 * @param exitTag 平仓标签，默认为空字符串
	 * @param openBarNo 开仓K线编号，默认为0
	 * @param closeBarNo 平仓K线编号，默认为0
	 * 
	 * 将平仓信息记录到平仓日志文件中，包括开平仓时间、价格、盈亏等详细信息。
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double totalprofit = 0, const char* enterTag = "", const char* exitTag = "", uint32_t openBarNo = 0, uint32_t closeBarNo = 0);

	/**
	 * @brief 保存策略数据
	 * @param flag 保存标志，默认为0xFFFFFFFF（保存所有数据）
	 * 
	 * 将策略的持仓、资金、条件单、信号等数据保存到JSON文件中。
	 * 支持断点续传，策略重启后可以恢复到上次的状态。
	 */
	void	save_data(uint32_t flag = 0xFFFFFFFF);
	
	/**
	 * @brief 加载策略数据
	 * @param flag 加载标志，默认为0xFFFFFFFF（加载所有数据）
	 * 
	 * 从JSON文件中加载策略的持仓、资金、条件单、信号等数据。
	 * 用于策略重启后恢复上次的状态。
	 */
	void	load_data(uint32_t flag = 0xFFFFFFFF);

	/**
	 * @brief 加载用户数据
	 * 
	 * 从JSON文件中加载策略自定义的用户数据。
	 * 用户数据是策略开发者自定义的键值对数据。
	 */
	void	load_userdata();
	
	/**
	 * @brief 保存用户数据
	 * 
	 * 将策略自定义的用户数据保存到JSON文件中。
	 * 只有用户数据被修改时才会保存。
	 */
	void	save_userdata();

	/**
	 * @brief 更新浮动盈亏
	 * @param stdCode 合约代码
	 * @param price 当前价格
	 * 
	 * 根据当前价格计算并更新指定合约的浮动盈亏。
	 * 浮动盈亏 = (当前价格 - 开仓价格) * 持仓数量 * 合约乘数 * 方向系数
	 */
	void	update_dyn_profit(const char* stdCode, double price);

	/**
	 * @brief 执行设置仓位操作
	 * @param stdCode 合约代码
	 * @param qty 目标仓位数量，正数表示多头，负数表示空头
	 * @param userTag 用户标签，默认为空字符串
	 * @param bFireAtOnce 是否立即通知引擎，默认为false
	 * 
	 * 核心的仓位设置函数，根据目标仓位和当前持仓计算需要进行的交易操作。
	 * 支持开仓、平仓、反手等操作，并自动记录交易明细和盈亏。
	 */
	void	do_set_position(const char* stdCode, double qty, const char* userTag = "", bool bFireAtOnce = false);
	
	/**
	 * @brief 追加交易信号
	 * @param stdCode 合约代码
	 * @param qty 目标仓位数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param sigType 信号类型，0-调度信号，1-Tick信号，2-条件单信号，默认为0
	 * 
	 * 添加一个交易信号到信号映射表中，等待后续处理。
	 * 信号会在下次Tick更新时被处理并执行。
	 */
	void	append_signal(const char* stdCode, double qty, const char* userTag = "", uint32_t sigType = 0);

	/**
	 * @brief 获取指定合约的条件单列表
	 * @param stdCode 合约代码
	 * @return CondList& 返回条件单列表的引用
	 * 
	 * 获取指定合约的条件单列表，如果不存在则创建空列表。
	 * 用于添加、查询、删除条件单。
	 */
	inline CondList& get_cond_entrusts(const char* stdCode);

protected:
	/**
	 * @brief 调试日志记录模板函数
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 格式化参数
	 * 
	 * 使用格式化字符串记录调试级别的日志。
	 * 支持类似printf的格式化语法。
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_debug(buffer);  // 调用基类的调试日志接口
	}

	/**
	 * @brief 信息日志记录模板函数
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 格式化参数
	 * 
	 * 使用格式化字符串记录信息级别的日志。
	 * 支持类似printf的格式化语法。
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_info(buffer);  // 调用基类的信息日志接口
	}

	/**
	 * @brief 错误日志记录模板函数
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 格式化参数
	 * 
	 * 使用格式化字符串记录错误级别的日志。
	 * 支持类似printf的格式化语法。
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_error(buffer);  // 调用基类的错误日志接口
	}

	/**
	 * @brief 导出图表信息
	 * 
	 * 将策略的图表配置信息导出到JSON文件中。
	 * 包括主K线、指标、指标线、基准线等配置信息。
	 */
	void	dump_chart_info();

public:
	/**
	 * @brief 获取策略上下文ID
	 * @return uint32_t 返回策略上下文的唯一标识符
	 * 
	 * 返回策略上下文的唯一ID，用于在引擎中标识该策略实例。
	 */
	virtual uint32_t id() { return _context_id; }

	//////////////////////////////////////////////////////////////////////////
	//回调函数（继承自ICtaStraCtx接口）
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 策略初始化回调
	 * 
	 * 策略初始化时调用，用于初始化输出文件、加载历史数据等准备工作。
	 */
	virtual void on_init() override;
	
	/**
	 * @brief 交易日开始回调
	 * @param uTDate 交易日日期，格式为YYYYMMDD
	 * 
	 * 每个交易日开始时调用，用于处理交易日开始时的初始化工作，如解冻持仓等。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;
	
	/**
	 * @brief 交易日结束回调
	 * @param uTDate 交易日日期，格式为YYYYMMDD
	 * 
	 * 每个交易日结束时调用，用于保存数据、记录结算信息等收尾工作。
	 */
	virtual void on_session_end(uint32_t uTDate) override;
	
	/**
	 * @brief Tick数据更新回调
	 * @param stdCode 合约代码
	 * @param newTick 新的Tick数据指针
	 * @param bEmitStrategy 是否触发策略回调，默认为true
	 * 
	 * 当收到新的Tick数据时调用，用于处理条件单触发、更新浮动盈亏、处理交易信号等。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy = true) override;
	
	/**
	 * @brief K线数据更新回调
	 * @param stdCode 合约代码
	 * @param period K线周期，如"m1"、"d1"等
	 * @param times 周期倍数，如"m5"中的5
	 * @param newBar 新的K线数据指针
	 * 
	 * 当收到新的K线数据时调用，用于标记K线闭合状态。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 策略调度回调
	 * @param curDate 当前日期，格式为YYYYMMDD
	 * @param curTime 当前时间，格式为HHMMSS
	 * @return bool 返回true表示已触发策略计算，false表示未触发
	 * 
	 * 定时调度时调用，用于触发策略的on_calculate回调函数，执行策略逻辑。
	 */
	virtual bool on_schedule(uint32_t curDate, uint32_t curTime) override;

	/**
	 * @brief 枚举持仓
	 * @param cb 回调函数，用于处理每个合约的持仓
	 * @param bForExecute 是否用于执行，默认为false
	 * 
	 * 遍历所有持仓（包括信号持仓），对每个合约调用回调函数。
	 * 如果bForExecute为true，则标记信号为已触发。
	 */
	virtual void enum_position(FuncEnumCtaPosCallBack cb, bool bForExecute = false) override;


	//////////////////////////////////////////////////////////////////////////
	//策略接口（继承自ICtaStraCtx接口）
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 开多仓
	 * @param stdCode 合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 执行开多仓操作，如果当前有空仓，则先平空再开多。
	 * 如果设置了限价或止损价，则创建条件单。
	 */
	virtual void stra_enter_long(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 开空仓
	 * @param stdCode 合约代码
	 * @param qty 开仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 执行开空仓操作，如果当前有多仓，则先平多再开空。
	 * 如果设置了限价或止损价，则创建条件单。
	 */
	virtual void stra_enter_short(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 平多仓
	 * @param stdCode 合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 执行平多仓操作，平仓数量不能超过当前多仓数量。
	 * 如果设置了限价或止损价，则创建条件单。
	 */
	virtual void stra_exit_long(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 平空仓
	 * @param stdCode 合约代码
	 * @param qty 平仓数量
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 执行平空仓操作，平仓数量不能超过当前空仓数量。
	 * 如果设置了限价或止损价，则创建条件单。
	 */
	virtual void stra_exit_short(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;

	/**
	 * @brief 获取当前持仓
	 * @param stdCode 合约代码
	 * @param bOnlyValid 是否只读可用持仓，默认为false
	 * @param userTag 用户标签，如果为空则读取持仓汇总，否则读取对应的持仓明细
	 * @return double 返回持仓数量，正数表示多头，负数表示空头
	 * 
	 * 获取指定合约的当前持仓数量。
	 * 如果userTag为空，则返回持仓汇总；如果userTag不为空，则返回对应标签的持仓明细。
	 * 只有当userTag为空时，bOnlyValid参数才生效，主要用于T+1品种的可用持仓查询。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, const char* userTag = "") override;
	
	/**
	 * @brief 设置目标仓位
	 * @param stdCode 合约代码
	 * @param qty 目标仓位数量，正数表示多头，负数表示空头
	 * @param userTag 用户标签，默认为空字符串
	 * @param limitprice 限价，默认为0.0（市价单）
	 * @param stopprice 止损价，默认为0.0（无止损）
	 * 
	 * 设置指定合约的目标仓位，系统会自动调整持仓以达到目标仓位。
	 * 如果设置了限价或止损价，则创建条件单。
	 */
	virtual void stra_set_position(const char* stdCode, double qty, const char* userTag = "", double limitprice = 0.0, double stopprice = 0.0) override;
	
	/**
	 * @brief 获取当前价格
	 * @param stdCode 合约代码
	 * @return double 返回当前价格
	 * 
	 * 获取指定合约的当前价格，优先使用本地缓存的价格，如果没有则从引擎获取。
	 */
	virtual double stra_get_price(const char* stdCode) override;

	/**
	 * @brief 读取当日价格
	 * @param stdCode 合约代码
	 * @param flag 价格标记：0-开盘价，1-最高价，2-最低价，3-收盘价/最新价
	 * @return double 返回对应的价格
	 * 
	 * 读取指定合约的当日价格，支持开盘价、最高价、最低价、收盘价等。
	 */
	virtual double stra_get_day_price(const char* stdCode, int flag = 0) override;

	virtual uint32_t stra_get_tdate() override;
	virtual uint32_t stra_get_date() override;
	virtual uint32_t stra_get_time() override;

	virtual double stra_get_fund_data(int flag /* = 0 */) override;

	virtual uint64_t stra_get_first_entertime(const char* stdCode) override;
	virtual uint64_t stra_get_last_entertime(const char* stdCode) override;
	virtual uint64_t stra_get_last_exittime(const char* stdCode) override;
	virtual double stra_get_last_enterprice(const char* stdCode) override;
	virtual double stra_get_position_avgpx(const char* stdCode) override;
	virtual double stra_get_position_profit(const char* stdCode) override;

	virtual uint64_t stra_get_detail_entertime(const char* stdCode, const char* userTag) override;
	virtual double stra_get_detail_cost(const char* stdCode, const char* userTag) override;
	virtual double stra_get_detail_profit(const char* stdCode, const char* userTag, int flag = 0) override;

	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;
	virtual WTSKlineSlice*	stra_get_bars(const char* stdCode, const char* period, uint32_t count, bool isMain = false) override;
	virtual WTSTickSlice*	stra_get_ticks(const char* stdCode, uint32_t count) override;
	virtual WTSTickData*	stra_get_last_tick(const char* stdCode) override;

	/*
	 *	获取分月合约代码
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) override;

	virtual void stra_sub_ticks(const char* stdCode) override;
	virtual void stra_sub_bar_events(const char* stdCode, const char* period) override;

	virtual void stra_log_info(const char* message) override;
	virtual void stra_log_debug(const char* message) override;
	virtual void stra_log_warn(const char* message) override;
	virtual void stra_log_error(const char* message) override;

	virtual void stra_save_user_data(const char* key, const char* val) override;

	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") override;

	virtual const char* stra_get_last_entertag(const char* stdCode) override;

public:
	/*
	 *	设置图表K线
	 */
	virtual void set_chart_kline(const char* stdCode, const char* period) override;

	/*
	 *	添加信号
	 */
	virtual void add_chart_mark(double price, const char* icon, const char* tag) override;

	/*
	 *	添加指标
	 */
	virtual void register_index(const char* idxName, uint32_t indexType) override;

	/*
	 *	添加指标线
	 */
	virtual bool register_index_line(const char* idxName, const char* lineName, uint32_t lineType) override;

	/*
	 *	添加基准线
	 *	@idxName	指标名称
	 *	@lineName	线条名称
	 *	@val		数值
	 */
	virtual bool add_index_baseline(const char* idxName, const char* lineName, double val) override;

	/*
	 *	设置指标值
	 */
	virtual bool set_index_value(const char* idxName, const char* lineName, double val) override;

protected:
	uint32_t		_context_id;      // 策略上下文ID：策略上下文的唯一标识符，用于在引擎中标识该策略实例
	WtCtaEngine*	_engine;          // CTA引擎指针：指向CTA引擎对象，用于访问引擎提供的功能（如获取市场数据、通知交易等）

	int32_t			_slippage;        // 滑点设置：单位为最小价格变动单位，用于模拟交易成本，正数表示买入时加滑点、卖出时减滑点

	uint64_t		_total_calc_time;	// 总计算时间：策略执行的总耗时（微秒），用于性能统计
	uint32_t		_emit_times;		// 总计算次数：策略被调度的总次数，用于性能统计

	std::string		_main_key;        // 主K线键值：主K线的唯一标识，格式为"合约代码#周期"
	std::string		_main_code;       // 主K线合约代码：主K线对应的合约代码
	std::string		_main_period;     // 主K线周期：主K线的周期字符串，如"m5"、"d1"等

	/**
	 * @struct KlineTag
	 * @brief K线标记结构体
	 * 
	 * 用于标记K线的状态，包括是否闭合、是否需要通知策略等。
	 */
	typedef struct _KlineTag
	{
		bool	_closed;   // 是否闭合：标记K线是否已经闭合
		bool	_notify;   // 是否需要通知：标记K线闭合时是否需要触发策略的on_bar_close回调

		/**
		 * @brief 构造函数
		 * 
		 * 初始化K线标记，默认_closed和_notify都为false。
		 */
		_KlineTag() :_closed(false), _notify(false){}  // 初始化：K线未闭合，不需要通知

	} KlineTag;
	/**
	 * @typedef KlineTags
	 * @brief K线标记映射表类型定义
	 * 
	 * 使用哈希表存储K线键值到K线标记的映射关系。
	 * key为K线键值（格式为"合约代码#周期"），value为对应的K线标记。
	 */
	typedef wt_hashmap<std::string, KlineTag> KlineTags;
	KlineTags	_kline_tags;  // K线标记映射表：存储所有K线的状态标记

	/**
	 * @typedef PriceMap
	 * @brief 价格映射表类型定义
	 * 
	 * 使用哈希表存储合约代码到当前价格的映射关系。
	 * key为合约代码（字符串），value为对应的当前价格（double）。
	 */
	typedef wt_hashmap<std::string, double> PriceMap;
	PriceMap		_price_map;  // 价格映射表：存储各合约的当前价格，用于快速查询

	/**
	 * @struct DetailInfo
	 * @brief 持仓明细信息结构体
	 * 
	 * 记录每笔开仓的详细信息，包括开仓价格、数量、时间、盈亏等。
	 * 支持按用户标签区分不同的持仓明细。
	 */
	typedef struct _DetailInfo
	{
		bool		_long;          // 是否多头：true表示多头，false表示空头
		double		_price;         // 开仓价格：该笔持仓的开仓价格
		double		_volume;        // 持仓数量：该笔持仓的当前数量（平仓后会减少）
		uint64_t	_opentime;      // 开仓时间：该笔持仓的开仓时间，格式为YYYYMMDDHHMM
		uint32_t	_opentdate;     // 开仓交易日：该笔持仓的开仓交易日，格式为YYYYMMDD
		double		_max_profit;   // 最大盈利：该笔持仓的历史最大盈利
		double		_max_loss;     // 最大亏损：该笔持仓的历史最大亏损
		double		_max_price;    // 最高价格：该笔持仓持仓期间的最高价格
		double		_min_price;    // 最低价格：该笔持仓持仓期间的最低价格
		double		_profit;       // 当前盈亏：该笔持仓的当前浮动盈亏
		char		_opentag[32];  // 开仓标签：用户自定义的开仓标记，用于标识交易目的
		uint32_t	_open_barno;  // 开仓K线编号：该笔持仓开仓时的K线编号

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓明细信息，将所有成员变量清零。
		 */
		_DetailInfo()
		{
			memset(this, 0, sizeof(_DetailInfo));  // 将整个结构体内存清零
		}
	} DetailInfo;

	/**
	 * @struct PosInfo
	 * @brief 持仓信息结构体
	 * 
	 * 记录某个合约的完整持仓信息，包括总持仓、平仓盈亏、浮动盈亏、持仓明细等。
	 */
	typedef struct _PosInfo
	{
		double		_volume;        // 持仓数量：该合约的总持仓数量，正数表示多头，负数表示空头
		double		_closeprofit;   // 平仓盈亏：该合约的累计平仓盈亏
		double		_dynprofit;     // 浮动盈亏：该合约的当前浮动盈亏

		uint64_t	_last_entertime;  // 最后开仓时间：最后一次开仓的时间
		uint64_t	_last_exittime;   // 最后平仓时间：最后一次平仓的时间

		double		_frozen;        // 冻结持仓：T+1品种的冻结持仓数量（当日开仓需次日才能平）
		uint32_t	_frozen_date;   // 冻结日期：冻结持仓的日期，用于判断何时解冻

		std::vector<DetailInfo> _details;  // 持仓明细列表：该合约的所有持仓明细，按开仓时间顺序排列

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓信息，将所有数值型成员变量清零。
		 */
		_PosInfo()
		{
			_volume = 0;         // 持仓数量初始化为0
			_closeprofit = 0;    // 平仓盈亏初始化为0
			_dynprofit = 0;      // 浮动盈亏初始化为0
			_last_entertime = 0;  // 最后开仓时间初始化为0
			_last_exittime = 0;  // 最后平仓时间初始化为0
			_frozen = 0;         // 冻结持仓初始化为0
			_frozen_date = 0;    // 冻结日期初始化为0
		}
	} PosInfo;
	/**
	 * @typedef PositionMap
	 * @brief 持仓映射表类型定义
	 * 
	 * 使用哈希表存储合约代码到持仓信息的映射关系。
	 * key为合约代码（字符串），value为对应的持仓信息。
	 */
	typedef wt_hashmap<std::string, PosInfo> PositionMap;
	PositionMap		_pos_map;  // 持仓映射表：存储所有合约的持仓信息

	/**
	 * @struct SigInfo
	 * @brief 交易信号信息结构体
	 * 
	 * 记录待执行的交易信号信息，包括目标仓位、信号价格、信号类型等。
	 */
	typedef struct _SigInfo
	{
		double		_volume;      // 目标仓位：信号的目标仓位数量，正数表示多头，负数表示空头
		std::string	_usertag;     // 用户标签：用户自定义的交易标记
		double		_sigprice;    // 信号价格：信号生成时的价格
		uint32_t	_sigtype;	// 信号类型：0-调度信号（on_schedule中生成），1-Tick信号（on_tick中生成），2-条件单信号（条件单触发生成）
		uint64_t	_gentime;     // 生成时间：信号生成的时间戳
		bool		_triggered;   // 是否已触发：标记信号是否已经被执行器处理

		/**
		 * @brief 构造函数
		 * 
		 * 初始化交易信号信息，将所有成员变量设置为默认值。
		 */
		_SigInfo()
		{
			_volume = 0;         // 目标仓位初始化为0
			_sigprice = 0;       // 信号价格初始化为0
			_sigtype = 0;        // 信号类型初始化为0（调度信号）
			_gentime = 0;        // 生成时间初始化为0
			_triggered = false;  // 触发标志初始化为false
		}
	}SigInfo;
	/**
	 * @typedef SignalMap
	 * @brief 信号映射表类型定义
	 * 
	 * 使用哈希表存储合约代码到交易信号的映射关系。
	 * key为合约代码（字符串），value为对应的交易信号信息。
	 */
	typedef wt_hashmap<std::string, SigInfo>	SignalMap;
	SignalMap		_sig_map;  // 信号映射表：存储所有待执行的交易信号

	BoostFilePtr	_trade_logs;   // 交易日志文件：记录所有交易成交的CSV文件
	BoostFilePtr	_close_logs;   // 平仓日志文件：记录所有平仓信息的CSV文件
	BoostFilePtr	_fund_logs;    // 资金日志文件：记录每日资金变化的CSV文件
	BoostFilePtr	_sig_logs;     // 信号日志文件：记录所有交易信号的CSV文件
	BoostFilePtr	_pos_logs;     // 持仓日志文件：记录每日持仓变化的CSV文件
	BoostFilePtr	_idx_logs;     // 指标日志文件：记录图表指标数据的CSV文件
	BoostFilePtr	_mark_logs;    // 标记日志文件：记录图表标记的CSV文件

	CondEntrustMap	_condtions;      // 条件单映射表：存储所有待触发的条件单，key为合约代码
	uint64_t		_last_cond_min;	// 上次设置条件单的时间：最后一次设置条件单的时间戳，用于判断条件单是否过期
	uint32_t		_last_barno;	// 上次设置的K线编号：最后一次设置条件单时的K线编号

	//是否处于调度中的标记
	bool			_is_in_schedule;	// 是否在自动调度中：标记当前是否正在执行on_schedule回调，用于区分信号类型

	//用户数据
	/**
	 * @typedef StringHashMap
	 * @brief 字符串哈希映射表类型定义
	 * 
	 * 使用哈希表存储键值对形式的用户数据。
	 * key和value都是字符串类型。
	 */
	typedef wt_hashmap<std::string, std::string> StringHashMap;
	StringHashMap	_user_datas;  // 用户数据映射表：存储策略自定义的键值对数据
	bool			_ud_modified;  // 用户数据是否已修改：标记用户数据是否被修改，用于决定是否需要保存

	/**
	 * @struct StraFundInfo
	 * @brief 策略资金信息结构体
	 * 
	 * 记录策略的资金统计信息，包括累计平仓盈亏、累计浮动盈亏、累计手续费等。
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;    // 累计平仓盈亏：策略累计的平仓盈亏
		double	_total_dynprofit; // 累计浮动盈亏：策略当前的总浮动盈亏
		double	_total_fees;      // 累计手续费：策略累计支付的手续费

		/**
		 * @brief 构造函数
		 * 
		 * 初始化策略资金信息，将所有成员变量清零。
		 */
		_StraFundInfo()
		{
			memset(this, 0, sizeof(_StraFundInfo));  // 将整个结构体内存清零
		}
	} StraFundInfo;

	StraFundInfo		_fund_info;  // 策略资金信息：存储策略的资金统计信息

	//tick订阅列表
	wt_hashset<std::string> _tick_subs;    // Tick订阅集合：存储已订阅Tick数据的合约代码集合
	wt_hashset<std::string> _barevt_subs;  // K线事件订阅集合：存储已订阅K线闭合事件的合约代码集合

	//////////////////////////////////////////////////////////////////////////
	//图表相关
	std::string		_chart_code;    // 图表合约代码：图表显示的主合约代码
	std::string		_chart_period;  // 图表周期：图表显示的主K线周期

	/**
	 * @struct ChartLine
	 * @brief 图表指标线结构体
	 * 
	 * 定义图表中的一条指标线，包括名称和线型。
	 */
	typedef struct _ChartLine
	{
		std::string	_name;      // 线条名称：指标线的名称
		uint32_t	_lineType;  // 线条类型：指标线的类型（如折线、柱状图等）
	} ChartLine;

	/**
	 * @struct ChartIndex
	 * @brief 图表指标结构体
	 * 
	 * 定义图表中的一个指标，包括指标名称、类型、指标线和基准线。
	 */
	typedef struct _ChartIndex
	{
		std::string	_name;                      // 指标名称：指标的标识名称
		uint32_t	_indexType;                 // 指标类型：指标的类型（如MA、MACD等）
		wt_hashmap<std::string, ChartLine> _lines;      // 指标线映射表：存储该指标的所有指标线，key为线条名称
		wt_hashmap<std::string, double> _base_lines;    // 基准线映射表：存储该指标的所有基准线，key为线条名称，value为基准值
	} ChartIndex;

	wt_hashmap<std::string, ChartIndex>	_chart_indice;  // 图表指标映射表：存储所有图表指标，key为指标名称

private:
	SpinMutex		_mutex;  // 自旋锁互斥量：用于保护持仓和信号数据的并发访问，避免多线程竞争
};


NS_WTP_END