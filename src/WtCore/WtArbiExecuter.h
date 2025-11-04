/*!
 * \file WtExecuter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 套利执行器头文件
 * 
 * 文件设计逻辑和作用总结：
 * ========================
 * WtArbiExecuter 是套利交易执行器类，负责管理套利策略的执行单元和目标仓位。
 * 
 * 主要功能：
 * 1. 执行单元管理：为每个合约创建和管理执行单元（ExecuteUnit），执行单元负责具体的交易执行逻辑
 * 2. 目标仓位管理：接收策略的目标仓位信号，并将目标仓位分配给对应的执行单元
 * 3. 合约组合管理：支持合约组合交易，可以将多个合约组合成一个交易单元
 * 4. 自动清理功能：支持自动清理上一期主力合约的头寸，避免换月风险
 * 5. 严格同步模式：在严格同步模式下，自动清理不在管理范围内的持仓
 * 6. 线程池支持：支持使用线程池并发处理多个执行单元的操作
 * 
 * 设计特点：
 * - 采用工厂模式创建执行单元，支持不同类型的执行策略
 * - 支持仓位放大倍数（scale），可以将策略仓位放大后执行
 * - 实现了交易通知接收器接口，接收交易通道的状态变化
 * - 实现了执行上下文接口，为执行单元提供数据访问接口
 */
#pragma once
#include "ITrdNotifySink.h"					// 交易通知接收器接口头文件
#include "IExecCommand.h"						// 执行命令接口头文件
#include "WtExecuterFactory.h"					// 执行器工厂头文件
#include "../Includes/ExecuteDefs.h"			// 执行定义头文件
#include "../Share/threadpool.hpp"				// 线程池头文件
#include "../Share/SpinMutex.hpp"				// 自旋锁头文件

NS_WTP_BEGIN									// WonderTrader命名空间开始

// 前向声明
class WTSVariant;								// 变体类型，用于配置参数传递
class IDataManager;								// 数据管理器接口
class TraderAdapter;							// 交易适配器类
class IHotMgr;									// 主力合约管理器接口

/**
 * @class WtArbiExecuter
 * @brief 套利执行器类
 * 
 * WtArbiExecuter 是套利交易执行器，负责管理套利策略的执行单元和目标仓位。
 * 继承自 ExecuteContext（提供数据访问接口）、ITrdNotifySink（接收交易通知）、IExecCommand（执行命令接口）。
 * 
 * 主要职责：
 * 1. 管理执行单元：为每个合约创建和管理执行单元，执行单元负责具体的交易执行
 * 2. 处理目标仓位：接收策略的目标仓位信号，分配给对应的执行单元执行
 * 3. 处理交易回调：接收交易通道的状态变化、成交、订单等回调通知
 * 4. 合约组合处理：支持合约组合交易，可以将多个合约组合成一个交易单元
 * 5. 自动清理功能：自动清理上一期主力合约的头寸
 */
class WtArbiExecuter : public ExecuteContext,			// 继承执行上下文接口
	public ITrdNotifySink, public IExecCommand			// 继承交易通知接收器和执行命令接口
{
public:
	/**
	 * @brief 构造函数
	 * @param factory 执行器工厂指针，用于创建执行单元
	 * @param name 执行器名称
	 * @param dataMgr 数据管理器指针，用于获取行情数据
	 */
	WtArbiExecuter(WtExecuterFactory* factory, const char* name, IDataManager* dataMgr);
	
	/**
	 * @brief 析构函数
	 * 释放资源，等待线程池任务完成
	 */
	virtual ~WtArbiExecuter();

public:
	/**
	 * @brief 初始化执行器
	 * @param params 初始化参数，包含仓位放大倍数、自动清理配置、合约组合配置等
	 * @return 初始化是否成功
	 */
	bool init(WTSVariant* params);

	/**
	 * @brief 设置交易适配器
	 * @param adapter 交易适配器指针
	 * 
	 * 设置交易适配器后，会检查交易通道是否已就绪，并更新内部状态。
	 */
	void setTrader(TraderAdapter* adapter);

private:
	/**
	 * @brief 获取执行单元
	 * @param code 标准合约代码
	 * @param bAutoCreate 是否自动创建，如果为true且单元不存在则自动创建
	 * @return 执行单元智能指针，如果不存在且不自动创建则返回空指针
	 */
	ExecuteUnitPtr	getUnit(const char* code, bool bAutoCreate = true);

public:
	//////////////////////////////////////////////////////////////////////////
	// ExecuteContext接口实现
	// 以下方法实现ExecuteContext接口，为执行单元提供数据访问接口
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 获取历史Tick数据
	 * @param code 标准合约代码
	 * @param count 获取数量
	 * @param etime 结束时间戳，0表示获取最新数据
	 * @return Tick数据切片指针，失败返回NULL
	 */
	virtual WTSTickSlice*	getTicks(const char* code, uint32_t count, uint64_t etime = 0) override;

	/**
	 * @brief 获取最新Tick数据
	 * @param code 标准合约代码
	 * @return 最新Tick数据指针，失败返回NULL
	 */
	virtual WTSTickData*	grabLastTick(const char* code) override;

	/**
	 * @brief 获取持仓数量
	 * @param stdCode 标准合约代码
	 * @param validOnly 是否只返回可用持仓（true=可用持仓，false=总持仓）
	 * @param flag 持仓标志（1=多头，2=空头，3=净持仓）
	 * @return 持仓数量（正数表示多头，负数表示空头）
	 */
	virtual double		getPosition(const char* stdCode, bool validOnly = true, int32_t flag = 3) override;
	
	/**
	 * @brief 获取订单列表
	 * @param code 标准合约代码
	 * @return 订单映射表指针，失败返回NULL
	 */
	virtual OrderMap*	getOrders(const char* code) override;
	
	/**
	 * @brief 获取未完成订单数量
	 * @param code 标准合约代码
	 * @return 未完成数量（正数表示买入未完成，负数表示卖出未完成）
	 */
	virtual double		getUndoneQty(const char* code) override;

	/**
	 * @brief 买入操作
	 * @param code 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param bForceClose 是否强制平仓
	 * @return 订单ID列表
	 */
	virtual OrderIDs	buy(const char* code, double price, double qty, bool bForceClose = false) override;
	
	/**
	 * @brief 卖出操作
	 * @param code 标准合约代码
	 * @param price 委托价格，0表示市价
	 * @param qty 委托数量
	 * @param bForceClose 是否强制平仓
	 * @return 订单ID列表
	 */
	virtual OrderIDs	sell(const char* code, double price, double qty, bool bForceClose = false) override;
	
	/**
	 * @brief 撤销指定订单
	 * @param localid 本地订单ID
	 * @return 撤单是否成功
	 */
	virtual bool		cancel(uint32_t localid) override;
	
	/**
	 * @brief 撤销指定合约的订单
	 * @param code 标准合约代码
	 * @param isBuy 是否买入方向
	 * @param qty 撤销数量，0表示撤销全部
	 * @return 撤销的订单ID列表
	 */
	virtual OrderIDs	cancel(const char* code, bool isBuy, double qty) override;
	
	/**
	 * @brief 写入日志
	 * @param message 日志消息
	 */
	virtual void		writeLog(const char* message) override;

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return 商品信息指针，失败返回NULL
	 */
	virtual WTSCommodityInfo*	getCommodityInfo(const char* stdCode) override;
	
	/**
	 * @brief 获取交易时段信息
	 * @param stdCode 标准合约代码
	 * @return 交易时段信息指针，失败返回NULL
	 */
	virtual WTSSessionInfo*		getSessionInfo(const char* stdCode) override;

	/**
	 * @brief 获取当前时间
	 * @return 当前时间戳（微秒）
	 */
	virtual uint64_t	getCurTime() override;

public:
	/**
	 * @brief 设置目标仓位
	 * @param targets 目标仓位映射表，key为标准合约代码，value为目标仓位数量
	 * 
	 * 处理目标仓位设置，包括：
	 * 1. 合约组合匹配：将组合中的合约合并处理
	 * 2. 仓位放大：根据配置的放大倍数调整目标仓位
	 * 3. 分配执行单元：将目标仓位分配给对应的执行单元
	 * 4. 自动清理：清理不在新目标仓位中的旧仓位
	 */
	virtual void set_position(const wt_hashmap<std::string, double>& targets) override;


	/**
	 * @brief 合约仓位变动回调
	 * @param stdCode 标准合约代码
	 * @param diffPos 仓位变化量（正数表示增加，负数表示减少）
	 * 
	 * 当策略的仓位发生变化时调用，会更新目标仓位并通知对应的执行单元。
	 */
	virtual void on_position_changed(const char* stdCode, double diffPos) override;

	/**
	 * @brief 实时行情回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 接收实时行情数据，并转发给对应的执行单元。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;

	/**
	 * @brief 成交回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否买入（true=买入，false=卖出）
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 接收成交回报，并转发给对应的执行单元。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;

	/**
	 * @brief 订单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否买入
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销
	 * 
	 * 接收订单状态变化回报，并转发给对应的执行单元。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled = false) override;

	/**
	 * @brief 持仓回报回调
	 * @param stdCode 标准合约代码
	 * @param isLong 是否多头（true=多头，false=空头）
	 * @param prevol 昨仓数量
	 * @param preavail 昨仓可用
	 * @param newvol 今仓数量
	 * @param newavail 今仓可用
	 * @param tradingday 交易日
	 * 
	 * 接收持仓回报，检查是否需要自动清理上一期主力合约的头寸。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;

	/**
	 * @brief 委托回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 委托是否成功
	 * @param message 委托结果消息
	 * 
	 * 接收委托回报（下单成功或失败），并转发给对应的执行单元。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;

	/**
	 * @brief 交易通道就绪回调
	 * 
	 * 当交易通道就绪时调用，通知所有执行单元交易通道已就绪。
	 */
	virtual void on_channel_ready() override;

	/**
	 * @brief 交易通道丢失回调
	 * 
	 * 当交易通道断开时调用，通知所有执行单元交易通道已丢失。
	 */
	virtual void on_channel_lost() override;

	/**
	 * @brief 资金回报回调
	 * @param currency 货币类型
	 * @param prebalance 上日余额
	 * @param balance 当前余额
	 * @param dynbalance 动态权益
	 * @param avaliable 可用资金
	 * @param closeprofit 平仓盈亏
	 * @param dynprofit 浮动盈亏
	 * @param margin 占用保证金
	 * @param fee 手续费
	 * @param deposit 入金
	 * @param withdraw 出金
	 * 
	 * 接收资金回报，并转发给所有执行单元。
	 */
	virtual void on_account(const char* currency, double prebalance, double balance, double dynbalance, 
		double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw) override;

private:
	ExecuteUnitMap		_unit_map;					// 执行单元映射表，key为标准合约代码，value为执行单元指针
	TraderAdapter*		_trader;					// 交易适配器指针，用于执行交易操作
	WtExecuterFactory*	_factory;					// 执行器工厂指针，用于创建执行单元
	IDataManager*		_data_mgr;					// 数据管理器指针，用于获取行情数据
	WTSVariant*			_config;					// 配置参数对象

	double				_scale;						// 仓位放大倍数：策略仓位乘以该倍数后作为实际交易仓位
	bool				_auto_clear;				// 是否自动清理上一期的主力合约头寸：true表示自动清理换月后的旧主力合约头寸
	bool				_strict_sync;				// 是否严格同步目标仓位：true表示清理不在管理范围内的持仓
	bool				_channel_ready;				// 交易通道是否就绪标志

	SpinMutex			_mtx_units;					// 执行单元映射表的自旋锁，保证线程安全

	/**
	 * @struct CodeGroup
	 * @brief 合约组合结构体
	 * 
	 * 用于定义合约组合，组合中的合约按照固定比例进行交易。
	 */
	typedef struct _CodeGroup
	{
		char	_name[32] = { 0 };								// 组合名称
		wt_hashmap<std::string, double>	_items;					// 组合中的合约及其比例，key为合约代码，value为比例
	} CodeGroup;
	typedef std::shared_ptr<CodeGroup> CodeGroupPtr;				// 合约组合智能指针类型
	typedef wt_hashmap<std::string, CodeGroupPtr>	CodeGroups;	// 合约组合映射表类型
	CodeGroups				_groups;					// 合约组合映射表，key为组合名称，value为组合对象
	CodeGroups				_code_to_groups;			// 合约代码到组合的映射，key为合约代码，value为组合对象（用于快速查找）

	wt_hashset<std::string>	_clear_includes;			// 自动清理包含品种集合：只在包含列表中的品种才会自动清理
	wt_hashset<std::string>	_clear_excludes;			// 自动清理排除品种集合：排除列表中的品种不会自动清理

	wt_hashset<std::string> _channel_holds;			// 通道持仓集合：记录交易通道中实际持有的合约代码

	wt_hashmap<std::string, double> _target_pos;		// 目标仓位映射表，key为标准合约代码，value为目标仓位数量

	typedef std::shared_ptr<boost::threadpool::pool> ThreadPoolPtr;
	ThreadPoolPtr		_pool;						// 线程池指针，用于并发处理多个执行单元的操作
};

typedef std::shared_ptr<IExecCommand> ExecCmdPtr;		// 执行命令智能指针类型

NS_WTP_END											// WonderTrader命名空间结束
