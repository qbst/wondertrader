/*!
 * \file TraderAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易适配器头文件
 * 
 * 文件设计逻辑和作用总结：
 * ========================
 * TraderAdapter 是 WonderTrader 交易系统的核心适配器类，负责封装和管理底层交易接口。
 * 
 * 主要功能：
 * 1. 交易接口封装：将底层交易API（ITraderApi）封装成统一的交易适配器接口，提供标准化的交易操作
 * 2. 状态管理：管理交易通道的登录、查询、就绪等状态，确保交易流程的正确执行
 * 3. 持仓管理：维护和管理账户持仓信息，包括多空持仓、今昨持仓、可用持仓等
 * 4. 订单管理：跟踪和管理订单状态，包括订单查询、订单推送、订单撤销等
 * 5. 风控管理：实现流量控制和风险监控，包括下单频率限制、撤单频率限制、自成交检测等
 * 6. 数据持久化：支持交易日志和订单日志的保存，便于后续分析和回溯
 * 7. 事件通知：通过事件通知机制，将交易状态变化通知给注册的监听器
 * 
 * 设计特点：
 * - 采用适配器模式，隔离底层交易接口的具体实现
 * - 支持多交易通道管理（TraderAdapterMgr）
 * - 线程安全的订单管理（使用SpinMutex）
 * - 灵活的风控策略配置
 * - 完善的错误处理和日志记录
 */
#pragma once

#include "../Includes/ExecuteDefs.h"			// 执行定义头文件，包含订单相关结构定义
#include "../Includes/FasterDefs.h"			// 快速定义头文件，包含快速哈希表等定义
#include "../Includes/ITraderApi.h"			// 交易接口头文件，定义底层交易API接口
#include "../Share/BoostFile.hpp"				// Boost文件操作封装
#include "../Share/StdUtils.hpp"				// 标准工具函数封装
#include "../Share/SpinMutex.hpp"				// 自旋锁互斥量封装

NS_WTP_BEGIN									// WonderTrader命名空间开始

// 前向声明
class WTSVariant;								// 变体类型，用于配置参数传递
class ActionPolicyMgr;							// 动作策略管理器，管理开平仓策略
class WTSContractInfo;							// 合约信息类
class WTSCommodityInfo;							// 商品信息类
class WtLocalExecuter;							// 本地执行器类
class EventNotifier;							// 事件通知器类

class ITrdNotifySink;							// 交易通知接收器接口

// 枚举持仓回调函数类型定义
// 参数：标准代码, 是否多头, 昨仓数量, 昨仓可用, 今仓数量, 今仓可用
typedef std::function<void(const char*, bool, double, double, double, double)> FuncEnumChnlPosCallBack;

/**
 * @class TraderAdapter
 * @brief 交易适配器类
 * 
 * TraderAdapter 是交易系统的核心适配器，负责封装底层交易接口，提供统一的交易操作接口。
 * 继承自 ITraderSpi 接口，实现交易回调处理。
 * 
 * 主要职责：
 * 1. 管理交易通道的连接、登录、查询等状态
 * 2. 封装买入、卖出、开仓、平仓等交易操作
 * 3. 维护持仓、订单、成交等交易数据
 * 4. 实现风控检查，包括频率限制、自成交检测等
 * 5. 提供交易日志记录功能
 */
class TraderAdapter : public ITraderSpi
{
public:
	/**
	 * @brief 构造函数
	 * @param caster 事件通知器指针，用于发送交易事件通知，可为NULL
	 */
	TraderAdapter(EventNotifier* caster = NULL);
	
	/**
	 * @brief 析构函数
	 * 释放资源，清理统计数据等
	 */
	~TraderAdapter();

	/**
	 * @enum AdapterState
	 * @brief 交易适配器状态枚举
	 * 
	 * 定义交易通道从连接、登录到就绪的各个状态
	 */
	typedef enum tagAdapterState
	{
		AS_NOTLOGIN,		// 未登录状态：初始状态，尚未开始登录流程
		AS_LOGINING,		// 正在登录：已发起登录请求，等待登录结果
		AS_LOGINED,			// 已登录：登录成功，但尚未完成数据查询
		AS_LOGINFAILED,		// 登录失败：登录请求被拒绝或失败
		AS_POSITION_QRYED,	// 仓位已查：持仓查询完成
		AS_ORDERS_QRYED,	// 订单已查：订单查询完成
		AS_TRADES_QRYED,	// 成交已查：成交查询完成
		AS_ALLREADY			// 全部就绪：所有查询完成，交易通道可以使用
	} AdapterState;

	/**
	 * @struct PosItem
	 * @brief 持仓项结构体
	 * 
	 * 用于存储单个合约的持仓信息，包括多空两个方向的今昨持仓数据
	 */
	typedef struct _PosItem
	{
		// 多仓数据（做多方向持仓）
		double	l_newvol;		// 多头今仓数量：今日开仓的多头持仓数量
		double	l_newavail;		// 多头今仓可用：今日开仓的多头持仓中可用于平仓的数量
		double	l_prevol;		// 多头昨仓数量：昨日及之前开仓的多头持仓数量
		double	l_preavail;		// 多头昨仓可用：昨日及之前开仓的多头持仓中可用于平仓的数量

		// 空仓数据（做空方向持仓）
		double	s_newvol;		// 空头今仓数量：今日开仓的空头持仓数量
		double	s_newavail;		// 空头今仓可用：今日开仓的空头持仓中可用于平仓的数量
		double	s_prevol;		// 空头昨仓数量：昨日及之前开仓的空头持仓数量
		double	s_preavail;		// 空头昨仓可用：昨日及之前开仓的空头持仓中可用于平仓的数量

		/**
		 * @brief 构造函数
		 * 初始化所有持仓数据为0
		 */
		_PosItem()
		{
			memset(this, 0, sizeof(_PosItem));
		}

		/**
		 * @brief 获取总持仓数量
		 * @param isLong 是否多头，true表示多头，false表示空头
		 * @return 总持仓数量（今仓+昨仓）
		 */
		double total_pos(bool isLong = true) const
		{
			if (isLong)
				return l_newvol + l_prevol;
			else
				return s_newvol + s_prevol;
		}

		/**
		 * @brief 获取可用持仓数量
		 * @param isLong 是否多头，true表示多头，false表示空头
		 * @return 可用持仓数量（今仓可用+昨仓可用）
		 */
		double avail_pos(bool isLong = true) const
		{
			if (isLong)
				return l_newavail + l_preavail;
			else
				return s_newavail + s_preavail;
		}

	} PosItem;

	/**
	 * @struct RiskParams
	 * @brief 风控参数结构体
	 * 
	 * 定义交易风控策略的参数，包括下单和撤单的频率限制
	 */
	typedef struct _RiskParams
	{
		uint32_t	_order_times_boundary;		// 下单频率边界：在统计时间窗口内允许的最大下单次数
		uint32_t	_order_stat_timespan;		// 下单统计时间窗口：统计下单频率的时间跨度（秒）
		uint32_t	_order_total_limits;		// 下单总限额：当日允许的最大下单总次数

		uint32_t	_cancel_times_boundary;		// 撤单频率边界：在统计时间窗口内允许的最大撤单次数
		uint32_t	_cancel_stat_timespan;		// 撤单统计时间窗口：统计撤单频率的时间跨度（秒）
		uint32_t	_cancel_total_limits;		// 撤单总限额：当日允许的最大撤单总次数

		/**
		 * @brief 构造函数
		 * 初始化所有风控参数为0
		 */
		_RiskParams()
		{
			memset(this, 0, sizeof(_RiskParams));
		}
	} RiskParams;

public:
	/**
	 * @brief 初始化交易适配器（从配置文件加载）
	 * @param id 交易通道标识符
	 * @param params 配置参数，包含交易模块路径、登录信息等
	 * @param bdMgr 基础数据管理器，用于获取合约和商品信息
	 * @param policyMgr 动作策略管理器，用于管理开平仓策略
	 * @return 初始化是否成功
	 */
	bool init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr);
	
	/**
	 * @brief 初始化交易适配器（使用外部API）
	 * @param id 交易通道标识符
	 * @param api 外部提供的交易API接口
	 * @param bdMgr 基础数据管理器
	 * @param policyMgr 动作策略管理器
	 * @return 初始化是否成功
	 */
	bool initExt(const char* id, ITraderApi* api, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr);

	/**
	 * @brief 释放资源
	 * 断开交易连接，注销回调接口，释放交易API资源
	 */
	void release();

	/**
	 * @brief 启动交易适配器
	 * 注册回调接口，连接交易服务器，开始登录流程
	 * @return 启动是否成功
	 */
	bool run();

	/**
	 * @brief 获取交易通道ID
	 * @return 交易通道标识符字符串
	 */
	inline const char* id() const{ return _id.c_str(); }

	/**
	 * @brief 获取当前状态
	 * @return 当前交易适配器状态
	 */
	AdapterState state() const{ return _state; }

	/**
	 * @brief 添加交易通知接收器
	 * @param sink 交易通知接收器指针，用于接收交易状态变化通知
	 */
	void addSink(ITrdNotifySink* sink)
	{
		_sinks.insert(sink);
	}

	/**
	 * @brief 检查交易通道是否就绪
	 * @return true表示就绪，可以交易；false表示未就绪
	 */
	inline bool isReady() const { return _state == AS_ALLREADY; }

	/**
	 * @brief 查询资金信息
	 * 向交易服务器发送资金查询请求
	 */
	void queryFund();

private:
	/**
	 * @brief 执行委托下单
	 * @param entrust 委托单对象，包含合约、价格、数量等信息
	 * @return 本地订单ID，失败返回UINT_MAX
	 */
	uint32_t doEntrust(WTSEntrust* entrust);
	
	/**
	 * @brief 执行撤单操作
	 * @param ordInfo 订单信息对象
	 * @return 撤单是否成功发送
	 */
	bool	doCancel(WTSOrderInfo* ordInfo);

	/**
	 * @brief 打印持仓信息（用于日志输出）
	 * @param stdCode 标准合约代码
	 * @param pItem 持仓项结构
	 */
	inline void	printPosition(const char* stdCode, const PosItem& pItem);

	/**
	 * @brief 根据标准代码获取合约信息
	 * @param stdCode 标准合约代码
	 * @return 合约信息指针，失败返回NULL
	 */
	inline WTSContractInfo* getContract(const char* stdCode);
	
	/**
	 * @brief 根据标准代码获取商品信息
	 * @param stdCommID 标准商品ID
	 * @return 商品信息指针，失败返回NULL
	 */
	inline WTSCommodityInfo* getCommodify(const char* stdCommID);

	/**
	 * @brief 获取指定合约的风控参数
	 * @param stdCode 标准合约代码
	 * @return 风控参数指针，如果该合约没有配置则返回默认参数
	 */
	const RiskParams* getRiskParams(const char* stdCode);

	/**
	 * @brief 初始化数据保存功能
	 * 创建交易日志和订单日志文件，准备保存交易数据
	 */
	void initSaveData();

	/**
	 * @brief 记录成交日志
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param trdInfo 成交信息对象
	 */
	inline void	logTrade(uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo);
	
	/**
	 * @brief 记录订单日志
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param ordInfo 订单信息对象
	 */
	inline void	logOrder(uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo);

	/**
	 * @brief 保存实时数据到文件
	 * @param ayFunds 资金信息数组，可为NULL
	 */
	void	saveData(WTSArray* ayFunds = NULL);

	/**
	 * @brief 更新未完成订单数量
	 * @param stdCode 标准合约代码
	 * @param qty 数量变化（正数表示增加，负数表示减少）
	 * @param bOuput 是否输出日志
	 */
	inline void updateUndone(const char* stdCode, double qty, bool bOuput = true);

public:
	/**
	 * @brief 获取持仓数量
	 * @param stdCode 标准合约代码
	 * @param bValidOnly 是否只返回可用持仓（true=可用持仓，false=总持仓）
	 * @param flag 持仓标志（1=多头，2=空头，3=净持仓）
	 * @return 持仓数量（正数表示多头，负数表示空头）
	 */
	double getPosition(const char* stdCode, bool bValidOnly, int32_t flag = 3);
	
	/**
	 * @brief 获取订单列表
	 * @param stdCode 标准合约代码，空字符串表示获取所有订单
	 * @return 订单映射表指针，失败返回NULL
	 */
	OrderMap* getOrders(const char* stdCode);
	
	/**
	 * @brief 获取未完成订单数量
	 * @param stdCode 标准合约代码
	 * @return 未完成数量（正数表示买入未完成，负数表示卖出未完成）
	 */
	double getUndoneQty(const char* stdCode)
	{
		auto it = _undone_qty.find(stdCode);
		if (it != _undone_qty.end())
			return it->second;

		return 0;
	}

	/**
	 * @brief 枚举所有持仓
	 * @param cb 回调函数，对每个有持仓的合约调用该回调
	 */
	void enumPosition(FuncEnumChnlPosCallBack cb);

	/**
	 * @brief 开多仓
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param flag 订单标志（用于扩展订单属性）
	 * @param cInfo 合约信息，可为NULL（会自动获取）
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t openLong(const char* stdCode, double price, double qty, int flag, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 开空仓
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param flag 订单标志
	 * @param cInfo 合约信息，可为NULL
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t openShort(const char* stdCode, double price, double qty, int flag, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 平多仓
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param isToday 是否平今仓（true=平今，false=平昨）
	 * @param flag 订单标志
	 * @param cInfo 合约信息，可为NULL
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t closeLong(const char* stdCode, double price, double qty, bool isToday, int flag, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 平空仓
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param isToday 是否平今仓（true=平今，false=平昨）
	 * @param flag 订单标志
	 * @param cInfo 合约信息，可为NULL
	 * @return 本地订单ID，失败返回0
	 */
	uint32_t closeShort(const char* stdCode, double price, double qty, bool isToday, int flag, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 买入操作（智能开平）
	 * 根据持仓情况和策略规则，自动判断是开多还是平空
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param flag 订单标志
	 * @param bForceClose 是否强制平仓（true=优先平仓，false=优先开仓）
	 * @param cInfo 合约信息，可为NULL
	 * @return 订单ID列表
	 */
	OrderIDs buy(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 卖出操作（智能开平）
	 * 根据持仓情况和策略规则，自动判断是开空还是平多
	 * @param stdCode 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param flag 订单标志
	 * @param bForceClose 是否强制平仓
	 * @param cInfo 合约信息，可为NULL
	 * @return 订单ID列表
	 */
	OrderIDs sell(const char* stdCode, double price, double qty, int flag, bool bForceClose, WTSContractInfo* cInfo = NULL);
	
	/**
	 * @brief 撤销指定订单
	 * @param localid 本地订单ID
	 * @return 撤单是否成功发送
	 */
	bool	cancel(uint32_t localid);
	
	/**
	 * @brief 撤销指定合约的订单
	 * @param stdCode 标准合约代码，空字符串表示撤销所有合约
	 * @param isBuy 是否买入方向（true=撤销买入订单，false=撤销卖出订单）
	 * @param qty 撤销数量，0表示撤销全部
	 * @return 撤销的订单ID列表
	 */
	OrderIDs cancel(const char* stdCode, bool isBuy, double qty = 0);

	/**
	 * @brief 检查合约是否允许交易
	 * @param stdCode 标准合约代码
	 * @return true表示允许交易，false表示被风控禁止
	 */
	inline bool	isTradeEnabled(const char* stdCode) const;

	/**
	 * @brief 检查撤单限制
	 * @param stdCode 标准合约代码
	 * @return true表示允许撤单，false表示超过限制
	 */
	bool	checkCancelLimits(const char* stdCode);
	
	/**
	 * @brief 检查下单限制
	 * @param stdCode 标准合约代码
	 * @return true表示允许下单，false表示超过限制
	 */
	bool	checkOrderLimits(const char* stdCode);

	/**
	 * @brief 检查是否自成交
	 * @param stdCode 标准合约代码
	 * @param tInfo 成交信息对象
	 * @return true表示检测到自成交
	 */
	bool	checkSelfMatch(const char* stdCode, WTSTradeInfo* tInfo);

	/**
	 * @brief 检查合约是否在自成交名单中
	 * @param stdCode 标准合约代码
	 * @return true表示该合约发生过自成交，被禁止交易
	 */
	inline	bool isSelfMatched(const char* stdCode)
	{
		// 如果忽略自成交，则直接返回false
		if (_ignore_sefmatch)
			return false;

		auto it = _self_matches.find(stdCode);
		return it != _self_matches.end();
	}

public:
	//////////////////////////////////////////////////////////////////////////
	// ITraderSpi接口实现
	// 以下方法实现ITraderSpi接口，处理底层交易API的回调通知
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 处理交易事件
	 * @param e 交易事件类型（连接、断开等）
	 * @param ec 事件代码（错误码或状态码）
	 */
	virtual void handleEvent(WTSTraderEvent e, int32_t ec) override;

	/**
	 * @brief 登录结果回调
	 * @param bSucc 登录是否成功
	 * @param msg 登录结果消息
	 * @param tradingdate 交易日
	 */
	virtual void onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate) override;

	/**
	 * @brief 登出回调
	 * 交易服务器主动断开连接时调用
	 */
	virtual void onLogout() override;

	/**
	 * @brief 委托响应回调
	 * @param entrust 委托单对象
	 * @param err 错误信息，如果委托成功则为NULL
	 */
	virtual void onRspEntrust(WTSEntrust* entrust, WTSError *err) override;

	/**
	 * @brief 资金查询响应回调
	 * @param ayAccounts 资金账户信息数组
	 */
	virtual void onRspAccount(WTSArray* ayAccounts) override;

	/**
	 * @brief 持仓查询响应回调
	 * @param ayPositions 持仓信息数组
	 */
	virtual void onRspPosition(const WTSArray* ayPositions) override;

	/**
	 * @brief 订单查询响应回调
	 * @param ayOrders 订单信息数组
	 */
	virtual void onRspOrders(const WTSArray* ayOrders) override;

	/**
	 * @brief 成交查询响应回调
	 * @param ayTrades 成交信息数组
	 */
	virtual void onRspTrades(const WTSArray* ayTrades) override;

	/**
	 * @brief 订单推送回调（实时推送）
	 * @param orderInfo 订单信息对象
	 */
	virtual void onPushOrder(WTSOrderInfo* orderInfo) override;

	/**
	 * @brief 成交推送回调（实时推送）
	 * @param tradeRecord 成交信息对象
	 */
	virtual void onPushTrade(WTSTradeInfo* tradeRecord) override;

	/**
	 * @brief 交易错误回调
	 * @param err 错误信息对象
	 * @param pData 附加数据，可为NULL
	 */
	virtual void onTraderError(WTSError* err, void* pData = NULL) override;

	/**
	 * @brief 获取基础数据管理器
	 * @return 基础数据管理器指针
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override;

	/**
	 * @brief 处理交易日志
	 * @param ll 日志级别
	 * @param message 日志消息
	 */
	virtual void handleTraderLog(WTSLogLevel ll, const char* message) override;

private:
	WTSVariant*			_cfg;				// 配置参数对象，保存交易通道的配置信息
	std::string			_id;				// 交易通道标识符
	std::string			_order_pattern;		// 订单标签模式，用于标识本通道发出的订单

	uint32_t			_trading_day;		// 交易日，从登录响应中获取

	ITraderApi*			_trader_api;		// 底层交易API接口指针
	FuncDeleteTrader	_remover;			// 交易API删除函数指针，用于释放API资源
	AdapterState		_state;				// 当前适配器状态

	EventNotifier*		_notifier;			// 事件通知器指针，用于发送交易事件通知

	wt_hashset<ITrdNotifySink*>	_sinks;			// 交易通知接收器集合，用于通知交易状态变化

	IBaseDataMgr*		_bd_mgr;			// 基础数据管理器指针，用于获取合约和商品信息
	ActionPolicyMgr*	_policy_mgr;		// 动作策略管理器指针，用于管理开平仓策略规则

	wt_hashmap<std::string, PosItem> _positions;	// 持仓映射表，key为标准合约代码，value为持仓项

	SpinMutex	_mtx_orders;					// 订单映射表的自旋锁，保证线程安全
	OrderMap*	_orders;						// 订单映射表，key为本地订单ID，value为订单信息
	wt_hashset<std::string> _orderids;			// 订单ID集合，用于标记是否已处理过该订单

	wt_hashmap<std::string, std::string>		_trade_refs;	// 成交单与订单的匹配关系，key为成交单号，value为订单号
	wt_hashset<std::string>					_self_matches;	// 自成交合约集合，记录发生过自成交的合约代码

	/*
	 *	By Wesley @ 2023.03.16
	 *	加一个控制，这样自成交发生以后，还可以恢复交易
	 */
	bool			_ignore_sefmatch;		// 忽略自成交限制标志，true表示允许自成交

	wt_hashmap<std::string, double> _undone_qty;	// 未完成订单数量映射表，key为合约代码，value为未完成数量

	typedef WTSHashMap<std::string>	TradeStatMap;
	TradeStatMap*	_stat_map;				// 交易统计数据映射表，用于记录各合约的交易统计信息

	// 这两个缓存时间内的容器，主要是为了控制瞬间流量而设置的
	typedef std::vector<uint64_t> TimeCacheList;							// 时间戳列表类型
	typedef wt_hashmap<std::string, TimeCacheList> CodeTimeCacheMap;		// 合约代码到时间戳列表的映射类型
	CodeTimeCacheMap	_order_time_cache;		// 下单时间缓存，记录每个合约的下单时间戳，用于频率控制
	CodeTimeCacheMap	_cancel_time_cache;		// 撤单时间缓存，记录每个合约的撤单时间戳，用于频率控制

	// 如果被风控了，就会进入到排除队列
	wt_hashset<std::string>	_exclude_codes;		// 被风控排除的合约集合，这些合约将被禁止交易
	
	typedef wt_hashmap<std::string, RiskParams>	RiskParamsMap;
	RiskParamsMap	_risk_params_map;			// 风控参数映射表，key为品种代码，value为风控参数
	bool			_risk_mon_enabled;			// 风控监控是否启用标志

	bool			_save_data;					// 是否保存交易日志标志
	BoostFilePtr	_trades_log;				// 成交数据日志文件指针
	BoostFilePtr	_orders_log;				// 订单数据日志文件指针
	std::string		_rt_data_file;				// 实时数据文件路径（JSON格式，包含持仓和资金信息）
};

// 类型定义
typedef std::shared_ptr<TraderAdapter>				TraderAdapterPtr;		// 交易适配器智能指针类型
typedef wt_hashmap<std::string, TraderAdapterPtr>	TraderAdapterMap;		// 交易适配器映射表类型


//////////////////////////////////////////////////////////////////////////
// TraderAdapterMgr - 交易适配器管理器
/**
 * @class TraderAdapterMgr
 * @brief 交易适配器管理器类
 * 
 * 用于管理多个交易适配器实例，提供统一的接口来操作所有交易通道。
 * 采用单例模式思想，禁止拷贝构造和赋值操作。
 */
class TraderAdapterMgr : private boost::noncopyable
{
public:
	/**
	 * @brief 释放所有交易适配器资源
	 * 调用所有适配器的release方法，清空适配器映射表
	 */
	void	release();

	/**
	 * @brief 启动所有交易适配器
	 * 遍历所有适配器，调用它们的run方法启动交易通道
	 */
	void	run();

	/**
	 * @brief 获取所有适配器的引用
	 * @return 适配器映射表的常量引用
	 */
	const TraderAdapterMap& getAdapters() const { return _adapters; }

	/**
	 * @brief 根据名称获取交易适配器
	 * @param tname 交易通道名称
	 * @return 交易适配器智能指针，如果不存在则返回空指针
	 */
	TraderAdapterPtr getAdapter(const char* tname);

	/**
	 * @brief 添加交易适配器
	 * @param tname 交易通道名称
	 * @param adapter 交易适配器智能指针
	 * @return 添加是否成功（如果名称已存在则失败）
	 */
	bool	addAdapter(const char* tname, TraderAdapterPtr& adapter);

	/**
	 * @brief 刷新所有交易通道的资金信息
	 * 遍历所有适配器，调用它们的queryFund方法查询资金
	 */
	void	refresh_funds();

private:
	TraderAdapterMap	_adapters;		// 交易适配器映射表，key为交易通道名称，value为适配器指针
};

NS_WTP_END
