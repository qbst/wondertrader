/*!
 * \file TraderAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易适配器头文件
 *
 * 本文件定义了TraderAdapter类，用于适配不同的交易接口，提供统一的交易操作接口。
 *
 * 设计逻辑：
 * 1. 适配器模式：封装不同的交易接口，提供统一的交易操作接口
 * 2. 状态管理：管理交易适配器的连接状态和登录状态
 * 3. 订单管理：维护订单列表，跟踪订单状态和未完成数量
 * 4. 持仓管理：维护持仓信息，区分昨仓和今仓，多空双向持仓
 * 5. 动作策略：根据ActionPolicyMgr的策略规则，将策略信号转换为实际订单
 * 6. 风险控制：支持流量风控，限制下单和撤单频率
 * 7. 通知机制：支持多个通知接收器，及时通知订单、成交、持仓等变化
 *
 * 主要功能：
 * - 初始化交易接口和连接
 * - 下单操作：开多、开空、平多、平空
 * - 撤单操作：撤单、全部撤单
 * - 持仓查询：查询持仓、可平仓数量
 * - 订单查询：查询订单列表、订单状态
 * - 风险控制：下单频率限制、撤单频率限制
 * - 状态管理：管理连接状态、登录状态、查询状态
 */
#pragma once

#include "../Includes/FasterDefs.h"  // 快速定义头文件
#include "../Includes/ITraderApi.h"  // 交易接口定义
#include "../Share/BoostFile.hpp"  // Boost文件操作
#include "../Share/StdUtils.hpp"  // 标准工具函数
#include "../Includes/WTSCollection.hpp"  // WonderTrader集合类
#include "../Share/SpinMutex.hpp"  // 自旋锁

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSVariant;  // 前向声明：变体类型
class WTSContractInfo;  // 前向声明：合约信息
class WTSCommodityInfo;  // 前向声明：品种信息
class ITrdNotifySink;  // 前向声明：交易通知接收器接口
class ActionPolicyMgr;  // 前向声明：动作策略管理器

typedef std::vector<uint32_t> OrderIDs;  // 订单ID列表类型
typedef WTSMap<uint32_t> OrderMap;  // 订单映射表类型

/**
 * @class TraderAdapter
 * @brief 交易适配器类
 * 
 * 适配不同的交易接口，提供统一的交易操作接口。
 * 继承自ITraderSpi接口，实现交易接口的回调处理。
 * 
 * 核心功能：
 * - 连接管理和登录管理
 * - 订单下单和撤单
 * - 持仓和订单查询
 * - 风险控制
 * - 策略信号转换
 */
class TraderAdapter : public ITraderSpi
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建交易适配器实例。
	 */
	TraderAdapter();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理交易适配器占用的资源。
	 */
	~TraderAdapter();

	/**
	 * @enum AdapterState
	 * @brief 适配器状态枚举
	 * 
	 * 定义交易适配器的各种状态，用于跟踪适配器的连接和查询进度。
	 */
	typedef enum tagAdapterState
	{
		AS_NOTLOGIN,		// 未登录状态
		AS_LOGINING,		// 正在登录状态
		AS_LOGINED,			// 已登录状态
		AS_LOGINFAILED,		// 登录失败状态
		AS_POSITION_QRYED,	// 仓位已查询状态
		AS_ORDERS_QRYED,	// 订单已查询状态
		AS_TRADES_QRYED,	// 成交已查询状态
		AS_ALLREADY			// 全部就绪状态
	} AdapterState;

	/**
	 * @struct PosItem
	 * @brief 持仓项结构体
	 * 
	 * 定义单个合约的持仓信息，包括多空双向持仓，区分昨仓和今仓。
	 */
	typedef struct _PosItem
	{
		//多仓数据
		double	l_newvol;  // 多今仓数量
		double	l_newavail;  // 多今仓可平数量
		double	l_prevol;  // 多昨仓数量
		double	l_preavail;  // 多昨仓可平数量

		//空仓数据
		double	s_newvol;  // 空今仓数量
		double	s_newavail;  // 空今仓可平数量
		double	s_prevol;  // 空昨仓数量
		double	s_preavail;  // 空昨仓可平数量

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓项，将所有成员变量清零。
		 */
		_PosItem()
		{
			memset(this, 0, sizeof(_PosItem));  // 将结构体内存清零
		}

		/**
		 * @brief 获取总持仓数量
		 * @param isLong 是否多仓，默认true
		 * @return 总持仓数量
		 */
		double total_pos(bool isLong = true) const
		{
			if (isLong)  // 如果是多仓
				return l_newvol + l_prevol;  // 返回多今仓+多昨仓
			else  // 如果是空仓
				return s_newvol + s_prevol;  // 返回空今仓+空昨仓
		}

		/**
		 * @brief 获取可平仓数量
		 * @param isLong 是否多仓，默认true
		 * @return 可平仓数量
		 */
		double avail_pos(bool isLong = true) const
		{
			if (isLong)  // 如果是多仓
				return l_newavail + l_preavail;  // 返回多今仓可平+多昨仓可平
			else  // 如果是空仓
				return s_newavail + s_preavail;  // 返回空今仓可平+空昨仓可平
		}

	} PosItem;

	/**
	 * @struct RiskParams
	 * @brief 风险控制参数结构体
	 * 
	 * 定义风险控制的参数，包括下单频率限制和撤单频率限制。
	 */
	typedef struct _RiskParams
	{
		uint32_t	_order_times_boundary;  // 下单频率边界值（单位时间内允许的最大下单次数）
		uint32_t	_order_stat_timespan;  // 下单统计时间跨度（秒）
		uint32_t	_order_total_limits;  // 下单总限额（总允许的下单次数）

		uint32_t	_cancel_times_boundary;  // 撤单频率边界值（单位时间内允许的最大撤单次数）
		uint32_t	_cancel_stat_timespan;  // 撤单统计时间跨度（秒）
		uint32_t	_cancel_total_limits;  // 撤单总限额（总允许的撤单次数）

		/**
		 * @brief 构造函数
		 * 
		 * 初始化风险控制参数，将所有成员变量清零。
		 */
		_RiskParams()
		{
			memset(this, 0, sizeof(_RiskParams));  // 将结构体内存清零
		}
	} RiskParams;

public:
	/**
	 * @brief 初始化交易适配器
	 * @param id 适配器ID
	 * @param params 配置参数
	 * @param bdMgr 基础数据管理器
	 * @param policyMgr 动作策略管理器
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置参数加载交易模块，初始化交易接口。
	 */
	bool init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr);

	/**
	 * @brief 使用外部交易接口初始化适配器
	 * @param id 适配器ID
	 * @param api 交易接口指针
	 * @param bdMgr 基础数据管理器
	 * @param policyMgr 动作策略管理器
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 直接使用外部提供的交易接口初始化适配器，无需加载动态库。
	 */
	bool initExt(const char* id, ITraderApi* api, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr);

	/**
	 * @brief 释放交易适配器资源
	 * 
	 * 断开连接，释放交易接口资源。
	 */
	void release();

	/**
	 * @brief 启动交易适配器
	 * @return 启动成功返回true，失败返回false
	 * 
	 * 注册回调接口，连接交易服务器，开始登录流程。
	 */
	bool run();

	/**
	 * @brief 获取适配器ID
	 * @return 适配器ID字符串
	 */
	inline const char* id() const{ return _id.c_str(); }

	/**
	 * @brief 获取适配器状态
	 * @return 适配器当前状态
	 */
	AdapterState state() const{ return _state; }

	/**
	 * @brief 添加通知接收器
	 * @param sink 通知接收器指针
	 * 
	 * 添加一个通知接收器，用于接收订单、成交、持仓等变化通知。
	 */
	void addSink(ITrdNotifySink* sink)
	{
		_sinks.insert(sink);  // 将接收器添加到集合中
	}

private:
	/**
	 * @brief 执行委托下单
	 * @param entrust 委托单指针
	 * @return 本地订单ID，失败返回UINT_MAX
	 * 
	 * 内部方法，处理委托单的下单逻辑，生成本地订单ID。
	 */
	uint32_t doEntrust(WTSEntrust* entrust);
	
	/**
	 * @brief 执行撤单
	 * @param ordInfo 订单信息指针
	 * @return 撤单成功返回true，失败返回false
	 * 
	 * 内部方法，处理订单的撤单逻辑。
	 */
	bool	doCancel(WTSOrderInfo* ordInfo);

	/**
	 * @brief 打印持仓信息
	 * @param stdCode 合约代码
	 * @param pItem 持仓项引用
	 * 
	 * 内部方法，打印持仓信息到日志。
	 */
	inline void	printPosition(const char* stdCode, const PosItem& pItem);

	/**
	 * @brief 获取合约信息
	 * @param stdCode 标准合约代码
	 * @return 合约信息指针
	 * 
	 * 内部方法，从标准合约代码获取合约信息。
	 */
	inline WTSContractInfo* getContract(const char* stdCode);

	/**
	 * @brief 更新未完成数量
	 * @param stdCode 合约代码
	 * @param qty 数量变化（正数表示增加，负数表示减少）
	 * 
	 * 内部方法，更新合约的未完成订单数量。
	 */
	inline void updateUndone(const char* stdCode, double qty);

	/**
	 * @brief 获取风险控制参数
	 * @param stdCode 标准合约代码
	 * @return 风险控制参数指针，如果未找到则返回默认参数
	 * 
	 * 根据合约代码获取对应的风险控制参数，如果未找到则返回默认参数。
	 */
	const RiskParams* getRiskParams(const char* stdCode);

public:
	/**
	 * @brief 获取持仓数量
	 * @param stdCode 标准合约代码
	 * @param bValidOnly 是否只返回可用持仓，true表示只返回可用持仓，false表示返回全部持仓
	 * @param flag 持仓标志：1-多仓，2-空仓，3-全部（默认）
	 * @return 持仓数量，多仓为正，空仓为负
	 */
	double	getPosition(const char* stdCode, bool bValidOnly, int32_t flag = 3);
	
	/**
	 * @brief 枚举持仓并通知接收器
	 * @param stdCode 标准合约代码，空字符串表示枚举所有合约
	 * @return 总持仓数量
	 * 
	 * 遍历持仓，通过回调函数通知所有接收器。
	 */
	double	enumPosition(const char* stdCode = "");
	
	/**
	 * @brief 获取订单列表
	 * @param stdCode 标准合约代码，空字符串表示获取所有订单
	 * @return 订单映射表指针，调用者需要释放
	 */
	OrderMap* getOrders(const char* stdCode);
	
	/**
	 * @brief 获取未完成数量
	 * @param stdCode 标准合约代码
	 * @return 未完成数量
	 * 
	 * 获取指定合约的未完成订单数量。
	 */
	inline double getUndoneQty(const char* stdCode)
	{
		auto it = _undone_qty.find(stdCode);  // 查找未完成数量
		if (it != _undone_qty.end())  // 如果找到
			return it->second;  // 返回数量

		return 0;  // 未找到返回0
	}

	/**
	 * @brief 获取交易统计信息数量
	 * @param stdCode 标准合约代码
	 * @return 统计信息数量
	 */
	uint32_t getInfos(const char* stdCode);

	/**
	 * @brief 买入操作（根据策略规则转换为实际订单）
	 * @param stdCode 标准合约代码
	 * @param price 价格，0表示市价单
	 * @param qty 数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok
	 * @param bForceClose 是否强制平仓
	 * @param cInfo 合约信息指针，可为NULL
	 * @return 订单ID列表
	 * 
	 * 根据动作策略规则，将买入信号转换为实际的开多或平空订单。
	 */
	OrderIDs buy(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 卖出操作（根据策略规则转换为实际订单）
	 * @param stdCode 标准合约代码
	 * @param price 价格，0表示市价单
	 * @param qty 数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok
	 * @param bForceClose 是否强制平仓
	 * @param cInfo 合约信息指针，可为NULL
	 * @return 订单ID列表
	 *	
	 * 根据动作策略规则，将卖出信号转换为实际的开空或平多订单。
	 */
	OrderIDs sell(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo = NULL);

	/**
	 * @brief 开多单
	 * @param stdCode 标准合约代码
	 * @param price 价格，0表示市价单
	 * @param qty 数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t openLong(const char* stdCode, double price, double qty, int flag);

	/**
	 * @brief 开空单
	 * @param stdCode 标准合约代码
	 * @param price 价格，0表示市价单
	 * @param qty 数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t openShort(const char* stdCode, double price, double qty, int flag);

	/**
	 * @brief 平多单
	 * @param stdCode 标准合约代码
	 * @param price 价格，0表示市价单
	 * @param qty 数量
	 * @param isToday 是否平今仓，默认false
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t closeLong(const char* stdCode, double price, double qty, bool isToday, int flag);
	
	/**
	 * @brief 平空单
	 * @param stdCode 标准合约代码
	 * @param price 价格，0表示市价单
	 * @param qty 数量
	 * @param isToday 是否平今仓，默认false
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t closeShort(const char* stdCode, double price, double qty, bool isToday, int flag);
	
	/**
	 * @brief 撤单
	 * @param localid 本地订单ID
	 * @return 撤单成功返回true，失败返回false
	 */
	bool	cancel(uint32_t localid);
	
	/**
	 * @brief 全部撤单
	 * @param stdCode 标准合约代码，空字符串表示撤所有订单
	 * @return 订单ID列表
	 */
	OrderIDs cancelAll(const char* stdCode);

	/**
	 * @brief 检查合约是否允许交易
	 * @param stdCode 标准合约代码
	 * @return 允许交易返回true，禁止交易返回false
	 * 
	 * 检查合约是否在风控排除列表中。
	 */
	inline bool	isTradeEnabled(const char* stdCode) const;

	/**
	 * @brief 检查撤单限制
	 * @param stdCode 标准合约代码
	 * @return 允许撤单返回true，禁止撤单返回false
	 * 
	 * 检查合约的撤单频率是否超过限制。
	 */
	bool	checkCancelLimits(const char* stdCode);
	
	/**
	 * @brief 检查下单限制
	 * @param stdCode 标准合约代码
	 * @return 允许下单返回true，禁止下单返回false
	 * 
	 * 检查合约的下单频率是否超过限制。
	 */
	bool	checkOrderLimits(const char* stdCode);

public:
	//////////////////////////////////////////////////////////////////////////
	//ITraderSpi接口
	/**
	 * @brief 处理交易事件
	 * @param e 交易事件类型
	 * @param ec 事件代码
	 * 
	 * 实现ITraderSpi接口，处理交易接口的事件回调。
	 */
	virtual void handleEvent(WTSTraderEvent e, int32_t ec) override;

	/**
	 * @brief 登录结果回调
	 * @param bSucc 登录是否成功
	 * @param msg 消息
	 * @param tradingdate 交易日
	 * 
	 * 实现ITraderSpi接口，处理登录结果回调。
	 * 登录成功后自动查询持仓、订单、成交。
	 */
	virtual void onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate) override;

	/**
	 * @brief 登出回调
	 * 
	 * 实现ITraderSpi接口，处理登出回调。
	 */
	virtual void onLogout() override;

	/**
	 * @brief 委托回报回调
	 * @param entrust 委托单指针
	 * @param err 错误信息指针
	 * 
	 * 实现ITraderSpi接口，处理委托回报。
	 * 如果委托失败，更新未完成数量并通知接收器。
	 */
	virtual void onRspEntrust(WTSEntrust* entrust, WTSError *err) override;

	/**
	 * @brief 账户查询回调
	 * @param ayAccounts 账户数组
	 * 
	 * 实现ITraderSpi接口，处理账户查询回调。
	 * 账户查询完成后，适配器进入全部就绪状态。
	 */
	virtual void onRspAccount(WTSArray* ayAccounts) override;

	/**
	 * @brief 持仓查询回调
	 * @param ayPositions 持仓数组
	 * 
	 * 实现ITraderSpi接口，处理持仓查询回调。
	 * 更新持仓信息并通知接收器，然后查询订单。
	 */
	virtual void onRspPosition(const WTSArray* ayPositions) override;

	/**
	 * @brief 订单查询回调
	 * @param ayOrders 订单数组
	 * 
	 * 实现ITraderSpi接口，处理订单查询回调。
	 * 更新订单列表和未完成数量，然后查询成交。
	 */
	virtual void onRspOrders(const WTSArray* ayOrders) override;

	/**
	 * @brief 成交查询回调
	 * @param ayTrades 成交数组
	 * 
	 * 实现ITraderSpi接口，处理成交查询回调。
	 * 更新交易统计信息，然后查询账户。
	 */
	virtual void onRspTrades(const WTSArray* ayTrades) override;

	/**
	 * @brief 订单推送回调
	 * @param orderInfo 订单信息指针
	 * 
	 * 实现ITraderSpi接口，处理订单推送。
	 * 更新订单状态、持仓可平数量，并通知接收器。
	 */
	virtual void onPushOrder(WTSOrderInfo* orderInfo) override;

	/**
	 * @brief 成交推送回调
	 * @param tradeRecord 成交记录指针
	 * 
	 * 实现ITraderSpi接口，处理成交推送。
	 * 更新持仓、未完成数量，并通知接收器。
	 */
	virtual void onPushTrade(WTSTradeInfo* tradeRecord) override;

	/**
	 * @brief 交易错误回调
	 * @param err 错误信息指针
	 * @param pData 附加数据指针
	 * 
	 * 实现ITraderSpi接口，处理交易错误。
	 */
	virtual void onTraderError(WTSError* err, void* pData = NULL) override;

	/**
	 * @brief 获取基础数据管理器
	 * @return 基础数据管理器指针
	 * 
	 * 实现ITraderSpi接口，返回基础数据管理器。
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override;

	/**
	 * @brief 处理交易日志
	 * @param ll 日志级别
	 * @param message 日志消息
	 * 
	 * 实现ITraderSpi接口，处理交易接口的日志输出。
	 */
	virtual void handleTraderLog(WTSLogLevel ll, const char* message) override;

private:
	WTSVariant*			_cfg;  // 配置参数
	std::string			_id;  // 适配器ID
	std::string			_order_pattern;  // 订单用户标签模式

	uint32_t			_trading_day;  // 交易日

	ITraderApi*			_trader_api;  // 交易接口指针
	FuncDeleteTrader	_remover;  // 删除交易接口的函数指针
	AdapterState		_state;  // 适配器状态

	wt_hashset<ITrdNotifySink*>	_sinks;  // 通知接收器集合

	IBaseDataMgr*		_bd_mgr;  // 基础数据管理器指针
	ActionPolicyMgr*	_policy_mgr;  // 动作策略管理器指针

	wt_hashmap<std::string, PosItem> _positions;  // 持仓映射表，键为合约代码

	SpinMutex	_mtx_orders;  // 订单列表互斥锁
	OrderMap*	_orders;  // 订单映射表
	wt_hashset<std::string> _orderids;	// 订单号集合，主要用于标记是否处理过该订单

	wt_hashmap<std::string, double> _undone_qty;	// 未完成数量映射表，键为合约代码

	typedef WTSHashMap<std::string>	TradeStatMap;  // 交易统计映射表类型
	TradeStatMap*	_stat_map;	// 交易统计映射表，键为合约代码

	//这两个缓存时间内的容器,主要是为了控制瞬间流量而设置的
	typedef std::vector<uint64_t> TimeCacheList;  // 时间缓存列表类型
	typedef wt_hashmap<std::string, TimeCacheList> CodeTimeCacheMap;  // 代码时间缓存映射表类型
	CodeTimeCacheMap	_order_time_cache;	// 下单时间缓存，键为合约代码，值为时间戳列表
	CodeTimeCacheMap	_cancel_time_cache;	// 撤单时间缓存，键为合约代码，值为时间戳列表

	//如果被风控了,就会进入到排除队列
	wt_hashset<std::string>	_exclude_codes;  // 被风控排除的合约代码集合

	typedef wt_hashmap<std::string, RiskParams>	RiskParamsMap;  // 风险参数映射表类型
	RiskParamsMap	_risk_params_map;  // 风险参数映射表，键为品种代码
	bool			_risk_mon_enabled;  // 是否启用风险监控
};

typedef std::shared_ptr<TraderAdapter>					TraderAdapterPtr;  // 交易适配器智能指针类型
typedef wt_hashmap<std::string, TraderAdapterPtr>	TraderAdapterMap;  // 交易适配器映射表类型


//////////////////////////////////////////////////////////////////////////
//TraderAdapterMgr
/**
 * @class TraderAdapterMgr
 * @brief 交易适配器管理器类
 * 
 * 管理多个交易适配器实例，提供统一的访问接口。
 */
class TraderAdapterMgr
{
public:
	/**
	 * @brief 释放所有适配器
	 * 
	 * 释放所有交易适配器占用的资源。
	 */
	void	release();

	/**
	 * @brief 启动所有适配器
	 * 
	 * 启动所有交易适配器，开始连接和登录流程。
	 */
	void	run();

	/**
	 * @brief 获取适配器映射表
	 * @return 适配器映射表常量引用
	 */
	const TraderAdapterMap& getAdapters() const { return _adapters; }

	/**
	 * @brief 获取指定名称的适配器
	 * @param tname 交易通道名称
	 * @return 交易适配器智能指针，如果不存在则返回空指针
	 */
	TraderAdapterPtr getAdapter(const char* tname);

	/**
	 * @brief 添加适配器
	 * @param tname 交易通道名称
	 * @param adapter 交易适配器智能指针
	 * @return 添加成功返回true，失败返回false
	 * 
	 * 将交易适配器添加到管理器中，如果名称已存在则添加失败。
	 */
	bool	addAdapter(const char* tname, TraderAdapterPtr& adapter);

private:
	TraderAdapterMap	_adapters;  // 交易适配器映射表，键为交易通道名称
};

NS_WTP_END
