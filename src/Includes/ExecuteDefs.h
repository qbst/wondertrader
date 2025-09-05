/*!
 * \file ExecuteDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 执行单元定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中执行单元的核心接口和类结构，用于实现交易策略的执行逻辑。
 * 主要功能包括：
 * 1. 定义执行上下文接口，提供市场数据、仓位、订单等交易环境信息
 * 2. 定义执行单元基类，实现具体的交易执行逻辑
 * 3. 提供交易接口，包括买入、卖出、撤单等操作
 * 4. 支持多种执行模式：普通模式、差量模式、套利模式
 * 5. 定义执行单元工厂接口，支持动态创建和管理执行单元
 * 
 * 该类主要用于WonderTrader框架中的交易执行系统，为量化交易策略提供统一的执行接口。
 * 通过抽象基类设计，支持多种执行策略的扩展和插件化开发。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/WTSDataDef.hpp"  // 包含WTS数据定义
#include "../Includes/WTSCollection.hpp"  // 包含WTS集合类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSTickData;  // 前向声明：WTS Tick数据结构类
class WTSHisTickData;  // 前向声明：WTS历史Tick数据结构类
class WTSVariant;  // 前向声明：WTS变体类型类
class WTSCommodityInfo;  // 前向声明：WTS品种信息类
class WTSSessionInfo;  // 前向声明：WTS交易时间模板信息类

typedef std::vector<uint32_t> OrderIDs;  // 订单ID向量类型别名
typedef WTSMap<uint32_t> OrderMap;  // 订单映射类型别名

//////////////////////////////////////////////////////////////////////////
//执行环境基础类
/**
 * @class ExecuteContext
 * @brief 执行上下文接口类
 * 
 * 该类定义了执行单元运行所需的交易环境接口，提供市场数据访问、交易操作、
 * 仓位查询、订单管理等功能。所有执行单元都通过此接口与交易系统交互。
 * 
 * 主要特性：
 * - 提供市场数据访问接口（Tick数据、历史数据）
 * - 支持交易操作（买入、卖出、撤单）
 * - 提供仓位和订单查询功能
 * - 支持品种信息和交易时间模板查询
 * - 提供日志记录和定时器注册功能
 */
class ExecuteContext
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化执行上下文对象，不执行任何特殊操作。
	 */
	ExecuteContext(){}  // 默认构造函数

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~ExecuteContext(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @param etime 截止时间，0表示当前时间
	 * @return WTSTickSlice* 返回历史数据封装类指针
	 *	
	 * 该函数获取指定合约的Tick数据切片，支持指定数据条数和截止时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	getTicks(const char* stdCode, uint32_t count, uint64_t etime = 0) = 0;  // 纯虚函数：获取Tick数据切片

	/**
	 * @brief 获取最近一笔Tick数据
	 * @param stdCode 标准合约代码
	 * @return WTSTickData* 返回WTSTickData指针
	 *	
	 * 该函数获取指定合约的最近一笔Tick数据，用于实时市场数据访问。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickData*	grabLastTick(const char* stdCode) = 0;  // 纯虚函数：获取最近一笔Tick数据

	/**
	 * @brief 获取仓位信息
	 * @param stdCode 标准合约代码
	 * @param validOnly 是否只读取可用持仓，默认为true
	 * @param flag 操作标记：1-多仓，2-空仓，3-多空轧平，默认为3
	 * @return double 返回轧平后的仓位：多仓>0，空仓<0
	 *	
	 * 该函数获取指定合约的仓位信息，支持多空仓位轧平计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double getPosition(const char* stdCode, bool validOnly = true, int32_t flag = 3) = 0;  // 纯虚函数：获取仓位信息

	/**
	 * @brief 获取未完成订单
	 * @param stdCode 标准合约代码
	 * @return OrderMap* 返回localid-WTSOrderInfo的映射
	 * 
	 * 该函数获取指定合约的未完成订单信息，返回订单ID到订单信息的映射。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderMap* getOrders(const char* stdCode) = 0;  // 纯虚函数：获取未完成订单

	/**
	 * @brief 获取未完成数量
	 * @param stdCode 标准合约代码
	 * @return double 返回买卖轧平以后的未完成数量
	 *	
	 * 该函数获取指定合约的未完成数量，自动进行买卖方向的轧平计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double getUndoneQty(const char* stdCode) = 0;  // 纯虚函数：获取未完成数量

	/**
	 * @brief 买入接口
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0为市价单
	 * @param qty 委托数量
	 * @param bForceClose 是否强制平仓，默认为false
	 * @return OrderIDs 返回本地订单号数组，一个买入操作可能会拆成最多3个订单发出
	 * 
	 * 该函数执行买入操作，支持市价单和限价单，可能产生多个订单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderIDs buy(const char* stdCode, double price, double qty, bool bForceClose = false) = 0;  // 纯虚函数：买入接口

	/**
	 * @brief 卖出接口
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0为市价单
	 * @param qty 委托数量
	 * @param bForceClose 是否强制平仓，默认为false
	 * @return OrderIDs 返回本地订单号数组，一个卖出操作可能会拆成最多3个订单发出
	 * 
	 * 该函数执行卖出操作，支持市价单和限价单，可能产生多个订单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderIDs sell(const char* stdCode, double price, double qty, bool bForceClose = false) = 0;  // 纯虚函数：卖出接口

	/**
	 * @brief 根据本地订单号撤单
	 * @param localid 本地订单号
	 * @return bool 返回撤单指令是否发送成功
	 * 
	 * 该函数根据指定的本地订单号撤销对应的订单。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool	cancel(uint32_t localid) = 0;  // 纯虚函数：根据本地订单号撤单

	/**
	 * @brief 根据指定的方向和数量撤单
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否为买单，true为买单，false为卖单
	 * @param qty 最少撤单数量，0表示撤销全部对应方向的订单
	 * @return OrderIDs 返回实际发送了撤单指令的订单ID数组
	 *	
	 * 该函数根据合约代码、买卖方向和数量撤销订单。
	 * 如果有多个委托，按照时间顺序一个一个撤单，直到撤销的数量大于等于qty。
	 * 纯虚函数，子类必须实现。
	 */
	virtual OrderIDs cancel(const char* stdCode, bool isBuy, double qty = 0) = 0;  // 纯虚函数：根据方向和数量撤单

	/**
	 * @brief 写日志
	 * @param message 日志消息内容
	 * 
	 * 该函数用于记录执行过程中的日志信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void writeLog(const char* message) = 0;  // 纯虚函数：写日志

	/**
	 * @brief 获取品种参数
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回品种信息对象指针
	 * 
	 * 该函数获取指定合约的品种参数信息，包括合约乘数、最小变动价位等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSCommodityInfo* getCommodityInfo(const char* stdCode) = 0;  // 纯虚函数：获取品种参数

	/**
	 * @brief 获取交易时间模板信息
	 * @param stdCode 标准合约代码
	 * @return WTSSessionInfo* 返回交易时间模板信息对象指针
	 * 
	 * 该函数获取指定合约的交易时间模板信息，包括交易时段、休市时间等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSSessionInfo* getSessionInfo(const char* stdCode) = 0;  // 纯虚函数：获取交易时间模板信息

	/**
	 * @brief 获取当前时间
	 * @return uint64_t 返回当前时间，精确到毫秒，格式如20191127174139500
	 * 
	 * 该函数获取当前系统时间，用于策略的时间判断和日志记录。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint64_t	getCurTime() = 0;  // 纯虚函数：获取当前时间

	/**
	 * @brief 注册定时器
	 * @param stdCode 合约代码
	 * @param elapse 时间间隔，单位毫秒
	 * @return bool 返回是否注册成功
	 * 
	 * 该函数为指定合约注册定时器，用于定时执行策略逻辑。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool		registerTimer(const char* stdCode, uint32_t elapse){ return false; }  // 虚函数：注册定时器，默认返回false
};

//////////////////////////////////////////////////////////////////////////
//执行单元基础类
/**
 * @class ExecuteUnit
 * @brief 执行单元基类
 * 
 * 该类定义了交易策略执行单元的基本接口，负责实现具体的交易执行逻辑。
 * 支持多种执行模式：普通模式、差量模式、套利模式。
 * 
 * 主要特性：
 * - 支持多种执行模式（普通、差量、套利）
 * - 提供完整的交易生命周期回调
 * - 支持仓位管理和订单管理
 * - 提供市场数据回调处理
 * - 支持交易通道状态监控
 */
class ExecuteUnit
{
public:
	/**
	 * @brief 构造函数
	 * @param bDiffMode 是否为差量模式，默认为false
	 * 
	 * 初始化执行单元对象，设置执行上下文和合约代码为空。
	 * 差量模式用于特殊的执行策略。
	 */
	ExecuteUnit(bool bDiffMode = false) :_ctx(NULL), _code("") {}  // 初始化执行上下文和合约代码

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~ExecuteUnit(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 获取执行单元名称
	 * @return const char* 返回执行单元的名称
	 * 
	 * 该函数返回执行单元的名称，用于标识和管理不同的执行单元。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getName() = 0;  // 纯虚函数：获取执行单元名称

	/**
	 * @brief 获取所属执行器工厂名称
	 * @return const char* 返回执行单元所属的执行器工厂名称
	 * 
	 * 该函数返回执行单元所属的执行器工厂名称，用于执行单元的管理和分类。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getFactName() = 0;  // 纯虚函数：获取工厂名称

	/**
	 * @brief 初始化执行单元
	 * @param ctx 执行单元运行环境
	 * @param stdCode 管理的合约代码
	 * @param cfg 配置参数
	 * 
	 * 该函数初始化执行单元，设置执行上下文和管理的合约代码。
	 * 默认实现保存上下文和合约代码，子类可以重写此函数进行自定义初始化。
	 */
	virtual void init(ExecuteContext* ctx, const char* stdCode, WTSVariant* cfg){ _ctx = ctx; _code = stdCode; }  // 虚函数：初始化执行单元

public:
	/**
	 * @brief 设置新的目标仓位
	 * @param stdCode 合约代码
	 * @param newVol 新的目标仓位
	 * 
	 * 该函数设置指定合约的新目标仓位，执行单元会根据目标仓位自动调整持仓。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void set_position(const char* stdCode, double newVol) = 0;  // 纯虚函数：设置新的目标仓位

	/**
	 * @brief 清理全部持仓
	 * @param stdCode 合约代码
	 * 
	 * 该函数清理指定合约的全部持仓，包括锁仓情况下的持仓。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void clear_all_position(const char* stdCode){}  // 虚函数：清理全部持仓，默认实现为空

	/**
	 * @brief Tick数据回调
	 * @param newTick 最新的tick数据
	 * 
	 * 该函数在接收到新的Tick数据时被调用，用于处理实时市场数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_tick(WTSTickData* newTick) = 0;  // 纯虚函数：Tick数据回调

	/**
	 * @brief 成交回报回调
	 * @param localid 本地订单号
	 * @param stdCode 合约代码
	 * @param isBuy 是否为买入，true为买入，false为卖出
	 * @param vol 成交数量，这里没有正负，通过isBuy确定买入还是卖出
	 * @param price 成交价格
	 * 
	 * 该函数在订单成交时被调用，用于处理成交回报信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) = 0;  // 纯虚函数：成交回报

	/**
	 * @brief 订单回报回调
	 * @param localid 本地订单号
	 * @param stdCode 合约代码
	 * @param isBuy 是否为买入，true为买入，false为卖出
	 * @param leftover 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销
	 * 
	 * 该函数在订单状态变化时被调用，用于处理订单回报信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled) = 0;  // 纯虚函数：订单回报

	/**
	 * @brief 委托回报回调
	 * @param localid 本地订单号
	 * @param stdCode 合约代码
	 * @param bSuccess 委托是否成功
	 * @param message 委托结果消息
	 *	
	 * 该函数在委托指令发送后返回结果时被调用，用于处理委托回报信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) = 0;  // 纯虚函数：委托回报

	/**
	 * @brief 交易通道就绪回调
	 * 
	 * 该函数在交易通道初始化完成并准备就绪时被调用。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_channel_ready() = 0;  // 纯虚函数：交易通道就绪回调

	/**
	 * @brief 交易通道丢失回调
	 * 
	 * 该函数在交易通道连接丢失时被调用，用于处理连接异常情况。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_channel_lost() = 0;  // 纯虚函数：交易通道丢失回调

	/**
	 * @brief 资金回报回调
	 * @param currency 货币类型
	 * @param prebalance 上日余额
	 * @param balance 当前余额
	 * @param dynbalance 动态余额
	 * @param avaliable 可用资金
	 * @param closeprofit 平仓盈亏
	 * @param dynprofit 浮动盈亏
	 * @param margin 保证金
	 * @param fee 手续费
	 * @param deposit 入金
	 * @param withdraw 出金
	 * 
	 * 该函数在交易通道初始化完成以后调用一次，用于获取账户资金信息。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void on_account(const char* currency, double prebalance, double balance, double dynbalance, double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw) {}  // 虚函数：资金回报，默认实现为空

protected:
	ExecuteContext*	_ctx;  // 执行上下文指针，指向执行单元运行环境
	std::string		_code;  // 管理的合约代码字符串
};

//////////////////////////////////////////////////////////////////////////
//执行单元工厂接口
/**
 * @typedef FuncEnumUnitCallback
 * @brief 执行单元枚举回调函数类型
 * 
 * 该类型定义了执行单元枚举时的回调函数签名。
 * 用于在枚举执行单元时通知调用者每个执行单元的信息。
 * 
 * @param factName 工厂名称
 * @param unitName 执行单元名称
 * @param isLast 是否为最后一个执行单元
 */
typedef void(*FuncEnumUnitCallback)(const char* factName, const char* unitName, bool isLast);  // 执行单元枚举回调函数类型定义

/**
 * @class IExecuterFact
 * @brief 执行单元工厂接口
 * 
 * 该类定义了执行单元工厂的基本接口，负责执行单元的创建、删除和管理。
 * 通过工厂模式实现执行单元的动态加载和管理，支持插件化开发。
 * 
 * 主要特性：
 * - 支持执行单元的枚举和查询
 * - 提供多种执行模式的执行单元创建功能
 * - 支持执行单元的删除和资源管理
 * - 实现执行单元的生命周期管理
 */
class IExecuterFact
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化执行单元工厂对象，不执行任何特殊操作。
	 */
	IExecuterFact(){}  // 默认构造函数

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IExecuterFact(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 获取工厂名称
	 * @return const char* 返回执行单元工厂的名称
	 * 
	 * 该函数返回执行单元工厂的名称，用于标识和管理不同的执行单元工厂。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getName() = 0;  // 纯虚函数：获取工厂名称

	/**
	 * @brief 枚举执行单元
	 * @param cb 枚举回调函数
	 * 
	 * 该函数枚举工厂中所有可用的执行单元，通过回调函数通知调用者。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void enumExeUnit(FuncEnumUnitCallback cb) = 0;  // 纯虚函数：枚举执行单元

	/**
	 * @brief 根据名称创建执行单元
	 * @param name 执行单元名称
	 * @return ExecuteUnit* 返回创建的执行单元对象指针
	 * 
	 * 该函数根据执行单元名称创建对应的执行单元对象实例。
	 * 纯虚函数，子类必须实现。
	 */
	virtual ExecuteUnit* createExeUnit(const char* name) = 0;  // 纯虚函数：根据名称创建执行单元

	/**
	 * @brief 根据名称创建差量执行单元
	 * @param name 执行单元名称
	 * @return ExecuteUnit* 返回创建的差量执行单元对象指针
	 * 
	 * 该函数根据执行单元名称创建差量模式的执行单元对象实例。
	 * 差量模式用于特殊的执行策略。
	 * 纯虚函数，子类必须实现。
	 */
	virtual ExecuteUnit* createDiffExeUnit(const char* name) = 0;  // 纯虚函数：根据名称创建差量执行单元

	/**
	 * @brief 根据名称创建套利执行单元
	 * @param name 执行单元名称
	 * @return ExecuteUnit* 返回创建的套利执行单元对象指针
	 * 
	 * 该函数根据执行单元名称创建套利模式的执行单元对象实例。
	 * 套利模式用于套利策略的执行。
	 * 纯虚函数，子类必须实现。
	 */
	virtual ExecuteUnit* createArbiExeUnit(const char* name) = 0;  // 纯虚函数：根据名称创建套利执行单元

	/**
	 * @brief 删除执行单元
	 * @param unit 要删除的执行单元对象指针
	 * @return bool 删除成功返回true，失败返回false
	 * 
	 * 该函数删除指定的执行单元对象，释放相关资源。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool deleteExeUnit(ExecuteUnit* unit) = 0;  // 纯虚函数：删除执行单元
};

/**
 * @typedef FuncCreateExeFact
 * @brief 创建执行工厂函数指针类型
 * 
 * 该类型定义了创建执行工厂的函数指针签名。
 * 用于动态加载执行工厂插件。
 */
typedef IExecuterFact* (*FuncCreateExeFact)();  // 创建执行工厂函数指针类型定义

/**
 * @typedef FuncDeleteExeFact
 * @brief 删除执行工厂函数指针类型
 * 
 * 该类型定义了删除执行工厂的函数指针签名。
 * 用于动态卸载执行工厂插件。
 */
typedef void(*FuncDeleteExeFact)(IExecuterFact* &fact);  // 删除执行工厂函数指针类型定义

NS_WTP_END  // 结束WonderTrader命名空间