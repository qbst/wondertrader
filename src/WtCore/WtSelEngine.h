/*!
 * \file WtSelEngine.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 选股引擎头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的选股引擎类，继承自WtEngine基类和IExecuterStub接口。
 * 主要功能包括：
 * 1. 策略管理：管理多个选股策略上下文（ISelStraCtx），支持策略的初始化、运行和生命周期管理
 * 2. 定时任务：支持多种周期的定时任务（分钟、每日、每周、每月、每年），自动触发策略调度
 * 3. 执行器管理：管理多个执行器，将策略的仓位信号转发给执行器执行
 * 4. 行情处理：处理实时行情数据（Tick、K线），支持复权处理，并分发给订阅的策略
 * 5. 仓位管理：处理策略的仓位变化信号，支持过滤器、风险控制和热点合约转换
 * 6. 存根接口：实现IExecuterStub接口，为执行器提供查询功能（时间、商品信息、会话信息等）
 * 
 * 设计模式：
 * - 继承模式：继承自WtEngine，复用基础引擎功能；实现IExecuterStub，提供执行器存根服务
 * - 观察者模式：策略订阅行情数据，引擎负责分发
 * - 工厂模式：通过工厂创建策略上下文和执行器
 * - 定时器模式：使用定时任务触发策略的on_schedule回调
 * 
 * 使用场景：
 * 该引擎主要用于选股策略场景，需要定时执行选股逻辑，将选出的股票或合约信号转换为实际交易。
 * 适用于量化选股、组合管理、策略轮换等场景。
 */
#pragma once  // 防止头文件重复包含
#include "WtEngine.h"  // 包含基础引擎头文件
#include "WtExecMgr.h"  // 包含执行器管理器头文件

#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap
#include "../Includes/ISelStraCtx.h"  // 包含选股策略上下文接口头文件

#include <memory>  // 包含智能指针类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * @enum TaskPeriodType
 * @brief 任务周期类型枚举
 * 
 * 定义了定时任务的周期类型，用于控制任务的执行频率。
 * 支持不重复、分钟周期、每日、每周、每月、每年等多种周期。
 */
typedef enum tagTaskPeriodType
{
	TPT_None,		// 不重复，任务只执行一次
	TPT_Minute = 4,	// 分钟周期，每隔指定分钟数执行一次
	TPT_Daily = 8,	// 每个交易日执行一次
	TPT_Weekly,		// 每周执行一次，遇到节假日的话要顺延
	TPT_Monthly,	// 每月执行一次，遇到节假日顺延
	TPT_Yearly		// 每年执行一次，遇到节假日顺延
} TaskPeriodType;  // 任务周期类型枚举别名

/**
 * @struct _TaskInfo
 * @brief 任务信息结构体
 * 
 * 存储定时任务的完整信息，包括任务ID、名称、执行时间、周期等。
 */
typedef struct _TaskInfo
{
	uint32_t	_id;  // 任务ID，自动生成
	char		_name[16];		// 任务名称，最多16个字符
	char		_trdtpl[16];	// 交易日模板名称，用于判断节假日
	char		_session[16];	// 交易时间模板名称，用于判断交易时间
	uint32_t	_day;			// 日期字段，根据周期变化：
	                            // - 每日为0
	                            // - 每周为0~6，对应周日到周六
	                            // - 每月为1~31
	                            // - 每年为0101~1231（MMDD格式）
	uint32_t	_time;			// 执行时间，精确到分钟（HHMM格式）
	bool		_strict_time;	// 是否是严格时间模式
	                            // - true：只有时间完全相等才会执行
	                            // - false：大于等于触发时间都会执行

	uint64_t	_last_exe_time;	// 上次执行时间（格式：YYYYMMDDHHMM），主要为了防止重复执行

	TaskPeriodType	_period;	// 任务周期类型
} TaskInfo;  // 任务信息结构体类型别名

typedef std::shared_ptr<TaskInfo> TaskInfoPtr;  // 任务信息智能指针类型别名

typedef std::shared_ptr<ISelStraCtx> SelContextPtr;  // 选股策略上下文智能指针类型别名
class WtSelRtTicker;  // 前向声明：选股实时行情时钟类


/**
 * @class WtSelEngine
 * @brief 选股引擎类
 * 
 * 该类是WonderTrader框架中的选股引擎实现，继承自WtEngine基类和IExecuterStub接口。
 * 负责管理选股策略的运行环境，处理实时行情数据，管理定时任务，并协调执行器执行交易。
 * 
 * 主要特性：
 * - 支持多个选股策略同时运行
 * - 支持多种周期的定时任务
 * - 实时处理Tick行情和K线数据
 * - 支持前复权和后复权数据处理
 * - 管理执行器并转发仓位信号
 * - 提供执行器存根服务
 */
class WtSelEngine : public WtEngine, public IExecuterStub  // 继承自WtEngine和IExecuterStub
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化选股引擎，设置终止标志和配置对象指针为默认值。
	 */
	WtSelEngine();  // 构造函数
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，停止时间时钟并释放配置对象。
	 */
	~WtSelEngine();  // 析构函数

public:
	//////////////////////////////////////////////////////////////////////////
	//WtEngine接口实现
	/**
	 * @brief 初始化引擎
	 * @param cfg 配置对象指针
	 * @param bdMgr 基础数据管理器指针
	 * @param dataMgr 数据管理器指针
	 * @param hotMgr 热点合约管理器指针
	 * @param notifier 事件通知器指针
	 * 
	 * 初始化选股引擎，调用基类初始化方法，并保存配置对象。
	 */
	virtual void init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier) override;  // 初始化引擎

	/**
	 * @brief 运行引擎
	 * 
	 * 启动选股引擎，创建并启动实时行情时钟，生成策略和通道标记文件。
	 */
	virtual void run() override;  // 运行引擎

	/**
	 * @brief Tick行情回调
	 * @param stdCode 标准合约代码
	 * @param curTick 当前Tick数据指针
	 * 
	 * 处理Tick行情数据，根据订阅标记进行复权处理，然后分发给订阅的策略和执行器。
	 * 支持无复权、前复权和后复权三种模式。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* curTick) override;  // Tick行情回调

	/**
	 * @brief K线数据回调
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 处理K线数据闭合事件，分发给订阅了该K线的策略。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;  // K线数据回调

	/**
	 * @brief 处理推送的行情数据
	 * @param newTick 新的Tick数据指针
	 * 
	 * 接收外部推送的实时行情数据，转发给实时行情时钟处理。
	 */
	virtual void handle_push_quote(WTSTickData* newTick) override;  // 处理推送的行情数据

	/**
	 * @brief 引擎初始化回调
	 * 
	 * 处理引擎初始化完成事件，通知事件监听器。
	 */
	virtual void on_init() override;  // 引擎初始化回调

	/**
	 * @brief 交易日开始回调
	 * 
	 * 处理交易日开始事件，通知事件监听器并设置引擎就绪标志。
	 */
	virtual void on_session_begin() override;  // 交易日开始回调

	/**
	 * @brief 交易日结束回调
	 * 
	 * 处理交易日结束事件，通知事件监听器。
	 */
	virtual void on_session_end() override;  // 交易日结束回调

	///////////////////////////////////////////////////////////////////////////
	//IExecuterStub 接口实现
	/**
	 * @brief 获取实时时间
	 * @return uint64_t 返回当前实时时间戳（纳秒级）
	 * 
	 * 获取引擎当前的实时时间戳，用于执行器查询当前时间。
	 */
	virtual uint64_t get_real_time() override;  // 获取实时时间
	
	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回商品信息对象指针
	 * 
	 * 根据标准合约代码获取对应的商品信息，包括交易规则、手续费等。
	 */
	virtual WTSCommodityInfo* get_comm_info(const char* stdCode) override;  // 获取商品信息
	
	/**
	 * @brief 获取交易会话信息
	 * @param stdCode 标准合约代码
	 * @return WTSSessionInfo* 返回交易会话信息对象指针
	 * 
	 * 根据标准合约代码获取对应的交易会话信息，包括交易时间段、开盘时间等。
	 */
	virtual WTSSessionInfo* get_sess_info(const char* stdCode) override;  // 获取交易会话信息
	
	/**
	 * @brief 获取热点合约管理器
	 * @return IHotMgr* 返回热点合约管理器接口指针
	 * 
	 * 获取热点合约管理器，用于查询主力合约、次主力合约等。
	 */
	virtual IHotMgr* get_hot_mon() { return _hot_mgr; }  // 获取热点合约管理器
	
	/**
	 * @brief 获取交易日期
	 * @return uint32_t 返回当前交易日期（格式：YYYYMMDD）
	 * 
	 * 获取引擎当前的交易日期。
	 */
	virtual uint32_t get_trading_day() { return _cur_tdate; }  // 获取交易日期

public:
	/**
	 * @brief 添加策略上下文
	 * @param ctx 策略上下文智能指针
	 * @param date 日期字段（根据周期类型有不同的含义）
	 * @param time 执行时间（HHMM格式）
	 * @param period 任务周期类型
	 * @param bStrict 是否严格时间模式，默认为true
	 * @param trdtpl 交易日模板名称，默认为"CHINA"
	 * @param sessionID 交易时间模板名称，默认为"TRADING"
	 * 
	 * 将策略上下文添加到引擎的管理列表中，并创建对应的定时任务。
	 * 定时任务会在指定的时间触发策略的on_schedule回调。
	 */
	void			addContext(SelContextPtr ctx, uint32_t date, uint32_t time, TaskPeriodType period, bool bStrict = true, const char* trdtpl = "CHINA", const char* sessionID="TRADING");  // 添加策略上下文

	/**
	 * @brief 获取策略上下文
	 * @param id 策略ID
	 * @return SelContextPtr 返回策略上下文智能指针，不存在返回空指针
	 * 
	 * 根据策略ID查找并返回对应的策略上下文。
	 */
	SelContextPtr	getContext(uint32_t id);  // 获取策略上下文

	/**
	 * @brief 添加执行器
	 * @param executer 执行器智能指针引用
	 * 
	 * 将执行器添加到执行器管理器中，并设置执行器存根为当前引擎实例。
	 */
	inline void addExecuter(ExecCmdPtr& executer)  // 内联函数：添加执行器
	{
		_exec_mgr.add_executer(executer);  // 将执行器添加到执行器管理器
		executer->setStub(this);  // 设置执行器存根为当前引擎实例
	}

	/**
	 * @brief 分钟结束回调
	 * @param uDate 当前日期
	 * @param uTime 当前时间（分钟）
	 * 
	 * 处理分钟线闭合事件，检查所有定时任务是否到达触发时间，如果到达则触发任务执行。
	 * 由实时行情时钟调用。
	 */
	void	on_minute_end(uint32_t uDate, uint32_t uTime);  // 分钟结束回调

	/**
	 * @brief 处理仓位变化
	 * @param straName 策略名称
	 * @param stdCode 标准合约代码
	 * @param diffQty 仓位变化数量（正数表示增加，负数表示减少）
	 * 
	 * 处理策略的仓位变化信号，执行以下操作：
	 * 1. 检查策略过滤器，如果被过滤则忽略
	 * 2. 处理热点合约转换（如果有规则标签）
	 * 3. 应用风险控制（如果有风险倍数）
	 * 4. 更新目标仓位并保存
	 * 5. 转发给执行器执行
	 */
	void	handle_pos_change(const char* straName, const char* stdCode, double diffQty);  // 处理仓位变化

private:
	wt_hashmap<uint32_t, TaskInfoPtr>	_tasks;  // 任务映射表，键为策略ID，值为任务信息智能指针

	typedef wt_hashmap<uint32_t, SelContextPtr> ContextMap;  // 策略上下文映射表类型别名，键为策略ID，值为策略上下文指针
	ContextMap		_ctx_map;  // 策略上下文映射表，存储所有注册的选股策略上下文

	WtExecuterMgr	_exec_mgr;  // 执行器管理器，管理所有执行器实例

	bool	_terminated;  // 终止标志，表示引擎是否已终止

	WtSelRtTicker*	_tm_ticker;  // 实时行情时钟指针，用于管理实时行情时间和触发分钟线闭合
	WTSVariant*		_cfg;  // 配置对象指针，存储引擎配置信息
};

NS_WTP_END  // 结束WonderTrader命名空间
