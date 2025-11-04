/*!
 * \file WtExecuter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 本地执行器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的本地执行器类，实现了ExecuteContext接口和IExecCommand接口。
 * 主要功能包括：
 * 1. 执行单元管理：为每个合约创建和管理执行单元（ExecuteUnit），执行单元负责具体的交易执行逻辑
 * 2. 仓位管理：接收目标仓位设置，根据配置的倍数和组合规则处理仓位，并转发给执行单元
 * 3. 交易接口：实现ExecuteContext接口，提供买入、卖出、撤单等交易操作
 * 4. 数据查询：提供仓位、订单、行情等数据查询接口
 * 5. 交易回报：实现ITrdNotifySink接口，处理交易回报（成交、订单、持仓、资金等）
 * 6. 自动清理：支持自动清理上一期主力合约的头寸
 * 7. 合约组合：支持合约组合配置，实现组合仓位的自动匹配和拆分
 * 8. 线程池：支持使用线程池并发处理执行单元回调，提高性能
 * 
 * 设计模式：
 * - 适配器模式：实现ExecuteContext接口，适配交易适配器
 * - 命令模式：实现IExecCommand接口，处理执行命令
 * - 工厂模式：使用工厂创建执行单元
 * - 策略模式：不同的合约使用不同的执行策略
 * 
 * 使用场景：
 * 该执行器主要用于交易执行系统中，作为策略和执行单元之间的桥梁，
 * 负责将策略的目标仓位转换为实际的交易指令，并管理执行单元的生命周期。
 */
#pragma once  // 防止头文件重复包含
#include "ITrdNotifySink.h"  // 包含交易回报通知接口头文件
#include "IExecCommand.h"  // 包含执行命令接口头文件
#include "WtExecuterFactory.h"  // 包含执行器工厂头文件
#include "../Includes/ExecuteDefs.h"  // 包含执行单元相关定义
#include "../Share/threadpool.hpp"  // 包含线程池工具
#include "../Share/SpinMutex.hpp"  // 包含自旋锁工具

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：变体类型类
class IDataManager;  // 前向声明：数据管理器接口
class TraderAdapter;  // 前向声明：交易适配器类
class IHotMgr;  // 前向声明：热点合约管理器接口

//本地执行器
/**
 * @class WtLocalExecuter
 * @brief 本地执行器类
 * 
 * 该类实现了ExecuteContext接口和IExecCommand接口，是执行单元和交易系统的桥梁。
 * 负责管理执行单元的生命周期，处理目标仓位设置，提供交易接口，并处理交易回报。
 * 
 * 主要特性：
 * - 为每个合约创建独立的执行单元
 * - 支持仓位倍数缩放
 * - 支持合约组合配置
 * - 支持自动清理过期主力合约头寸
 * - 支持严格同步模式，确保通道持仓与目标仓位一致
 * - 支持线程池并发处理
 */
class WtLocalExecuter : public ExecuteContext,  // 继承执行上下文接口
	public ITrdNotifySink, public IExecCommand  // 继承交易回报通知接口和执行命令接口
{
public:
	/**
	 * @brief 构造函数
	 * @param factory 执行器工厂指针
	 * @param name 执行器名称
	 * @param dataMgr 数据管理器指针
	 * 
	 * 初始化本地执行器，设置工厂、名称和数据管理器，初始化配置参数。
	 */
	WtLocalExecuter(WtExecuterFactory* factory, const char* name, IDataManager* dataMgr);  // 构造函数
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理资源，等待线程池任务完成。
	 */
	virtual ~WtLocalExecuter();  // 虚析构函数

public:
	/*
	 *	初始化执行器
	 *	传入初始化参数
	 */
	/**
	 * @brief 初始化执行器
	 * @param params 初始化参数配置对象
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 从配置对象中读取执行器参数，包括：
	 * - scale: 仓位倍数
	 * - strict_sync: 是否严格同步
	 * - poolsize: 线程池大小
	 * - clear: 自动清理配置
	 * - groups: 合约组合配置
	 */
	bool init(WTSVariant* params);  // 初始化执行器

	/**
	 * @brief 设置交易适配器
	 * @param adapter 交易适配器指针
	 * 
	 * 设置交易适配器，并读取其就绪状态。
	 */
	void setTrader(TraderAdapter* adapter);  // 设置交易适配器

private:
	/**
	 * @brief 获取执行单元
	 * @param code 合约代码
	 * @param bAutoCreate 是否自动创建，默认为true
	 * @return ExecuteUnitPtr 返回执行单元智能指针，不存在且不自动创建则返回空指针
	 * 
	 * 根据合约代码获取或创建执行单元。如果执行单元不存在且bAutoCreate为true，
	 * 则根据配置的策略策略创建新的执行单元。
	 */
	ExecuteUnitPtr	getUnit(const char* code, bool bAutoCreate = true);  // 获取执行单元

public:
	//////////////////////////////////////////////////////////////////////////
	//ExecuteContext接口实现
	/**
	 * @brief 获取Tick数据切片
	 * @param code 合约代码
	 * @param count 数据条数
	 * @param etime 截止时间，0表示当前时间
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 从数据管理器获取指定合约的Tick历史数据切片。
	 */
	virtual WTSTickSlice*	getTicks(const char* code, uint32_t count, uint64_t etime = 0) override;  // 获取Tick数据切片

	/**
	 * @brief 获取最近一笔Tick数据
	 * @param code 合约代码
	 * @return WTSTickData* 返回最近一笔Tick数据指针
	 * 
	 * 从数据管理器获取指定合约的最近一笔Tick数据。
	 */
	virtual WTSTickData*	grabLastTick(const char* code) override;  // 获取最近一笔Tick数据

	/**
	 * @brief 获取仓位信息
	 * @param stdCode 标准合约代码
	 * @param validOnly 是否只读取可用持仓，默认为true
	 * @param flag 操作标记：1-多仓，2-空仓，3-多空轧平，默认为3
	 * @return double 返回轧平后的仓位：多仓>0，空仓<0
	 * 
	 * 从交易适配器获取指定合约的仓位信息。
	 */
	virtual double		getPosition(const char* stdCode, bool validOnly = true, int32_t flag = 3) override;  // 获取仓位信息

	/**
	 * @brief 获取订单映射表
	 * @param code 合约代码
	 * @return OrderMap* 返回订单映射表指针
	 * 
	 * 从交易适配器获取指定合约的订单映射表。
	 */
	virtual OrderMap*	getOrders(const char* code) override;  // 获取订单映射表

	/**
	 * @brief 获取未完成数量
	 * @param code 合约代码
	 * @return double 返回未完成数量
	 * 
	 * 从交易适配器获取指定合约的未完成订单数量。
	 */
	virtual double		getUndoneQty(const char* code) override;  // 获取未完成数量

	/**
	 * @brief 买入操作
	 * @param code 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param bForceClose 是否强制平仓，默认为false
	 * @return OrderIDs 返回订单ID列表
	 * 
	 * 通过交易适配器提交买入订单。如果通道未就绪，返回空列表。
	 */
	virtual OrderIDs	buy(const char* code, double price, double qty, bool bForceClose = false) override;  // 买入操作

	/**
	 * @brief 卖出操作
	 * @param code 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param bForceClose 是否强制平仓，默认为false
	 * @return OrderIDs 返回订单ID列表
	 * 
	 * 通过交易适配器提交卖出订单。如果通道未就绪，返回空列表。
	 */
	virtual OrderIDs	sell(const char* code, double price, double qty, bool bForceClose = false) override;  // 卖出操作

	/**
	 * @brief 撤单操作（按订单ID）
	 * @param localid 本地订单ID
	 * @return bool 撤单成功返回true，失败返回false
	 * 
	 * 通过交易适配器撤销指定订单。如果通道未就绪，返回false。
	 */
	virtual bool		cancel(uint32_t localid) override;  // 撤单操作（按订单ID）

	/**
	 * @brief 撤单操作（按合约代码和方向）
	 * @param code 合约代码
	 * @param isBuy 是否买入方向
	 * @param qty 撤单数量
	 * @return OrderIDs 返回被撤销的订单ID列表
	 * 
	 * 通过交易适配器撤销指定合约和方向的订单。如果通道未就绪，返回空列表。
	 */
	virtual OrderIDs	cancel(const char* code, bool isBuy, double qty) override;  // 撤单操作（按合约代码和方向）

	/**
	 * @brief 写入日志
	 * @param message 日志消息
	 * 
	 * 将日志消息写入日志系统，消息前会自动添加执行器名称前缀。
	 */
	virtual void		writeLog(const char* message) override;  // 写入日志

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回商品信息指针
	 * 
	 * 通过执行器存根获取指定合约的商品信息。
	 */
	virtual WTSCommodityInfo*	getCommodityInfo(const char* stdCode) override;  // 获取商品信息

	/**
	 * @brief 获取交易会话信息
	 * @param stdCode 标准合约代码
	 * @return WTSSessionInfo* 返回交易会话信息指针
	 * 
	 * 通过执行器存根获取指定合约的交易会话信息。
	 */
	virtual WTSSessionInfo*		getSessionInfo(const char* stdCode) override;  // 获取交易会话信息

	/**
	 * @brief 获取当前时间
	 * @return uint64_t 返回当前时间戳（纳秒级）
	 * 
	 * 通过执行器存根获取当前实时时间戳。
	 */
	virtual uint64_t	getCurTime() override;  // 获取当前时间

public:
	/*
	 *	设置目标仓位
	 */
	/**
	 * @brief 设置目标仓位
	 * @param targets 目标仓位映射表，键为合约代码，值为目标持仓数量
	 * 
	 * 设置各合约的目标仓位，执行以下操作：
	 * 1. 进行合约组合匹配，将组合仓位转换为单个合约仓位
	 * 2. 根据配置的倍数缩放目标仓位
	 * 3. 转发给对应的执行单元
	 * 4. 对于不在新目标中的合约，自动设置为0
	 * 5. 如果开启严格同步，检查通道持仓并平掉不在管理中的仓位
	 */
	virtual void set_position(const wt_hashmap<std::string, double>& targets) override;  // 设置目标仓位


	/*
	 *	合约仓位变动
	 */
	/**
	 * @brief 合约仓位变动通知
	 * @param stdCode 标准合约代码
	 * @param diffPos 仓位变动数量（正数表示增加，负数表示减少）
	 * 
	 * 处理合约仓位变动通知，更新目标仓位，并根据倍数缩放后转发给执行单元。
	 * 如果是增量头寸变动，会记录日志。
	 */
	virtual void on_position_changed(const char* stdCode, double diffPos) override;  // 合约仓位变动通知

	/*
	 *	实时行情回调
	 */
	/**
	 * @brief 实时行情回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 将实时行情数据转发给对应的执行单元。如果配置了线程池，则异步处理。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;  // 实时行情回调

	/*
	 *	成交回报
	 */
	/**
	 * @brief 成交回报
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否买入
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 将成交回报转发给对应的执行单元。如果配置了线程池，则异步处理。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;  // 成交回报

	/*
	 *	订单回报
	 */
	/**
	 * @brief 订单回报
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param isBuy 是否买入
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销，默认为false
	 * 
	 * 将订单回报转发给对应的执行单元。如果配置了线程池，则异步处理。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled = false) override;  // 订单回报

	/*
	 *
	 */
	/**
	 * @brief 持仓回报
	 * @param stdCode 标准合约代码
	 * @param isLong 是否多头
	 * @param prevol 之前持仓量
	 * @param preavail 之前可用持仓量
	 * @param newvol 新持仓量
	 * @param newavail 新可用持仓量
	 * @param tradingday 交易日
	 * 
	 * 处理持仓回报，记录通道持仓。如果开启了自动清理功能，检查并清理上一期主力合约的头寸。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;  // 持仓回报

	/*
	 *
	 */
	/**
	 * @brief 委托回报
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息
	 * 
	 * 将委托回报转发给对应的执行单元。如果配置了线程池，则异步处理。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;  // 委托回报

	/*
	 *	交易通道就绪
	 */
	/**
	 * @brief 交易通道就绪回调
	 * 
	 * 处理交易通道就绪事件，设置就绪标志，并通知所有执行单元。
	 */
	virtual void on_channel_ready() override;  // 交易通道就绪回调

	/*
	 *	交易通道丢失
	 */
	/**
	 * @brief 交易通道丢失回调
	 * 
	 * 处理交易通道丢失事件，清除就绪标志，并通知所有执行单元。
	 */
	virtual void on_channel_lost() override;  // 交易通道丢失回调

	/*
	 *	资金回报
	 */
	/**
	 * @brief 资金回报
	 * @param currency 币种
	 * @param prebalance 之前余额
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
	 * 将资金回报转发给所有执行单元。如果配置了线程池，则异步处理。
	 */
	virtual void on_account(const char* currency, double prebalance, double balance, double dynbalance, 
		double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw) override;  // 资金回报

private:
	ExecuteUnitMap		_unit_map;  // 执行单元映射表，键为合约代码，值为执行单元智能指针
	TraderAdapter*		_trader;  // 交易适配器指针，用于执行交易操作
	WtExecuterFactory*	_factory;  // 执行器工厂指针，用于创建执行单元
	IDataManager*		_data_mgr;  // 数据管理器指针，用于查询历史数据
	WTSVariant*			_config;  // 配置对象指针，存储执行器配置信息

	double				_scale;				// 放大倍数，用于缩放目标仓位
	bool				_auto_clear;		// 是否自动清理上一期的主力合约头寸
	bool				_strict_sync;		// 是否严格同步目标仓位（确保通道持仓与目标仓位一致）
	bool				_channel_ready;  // 交易通道是否就绪

	SpinMutex			_mtx_units;  // 执行单元映射表的自旋锁，用于线程安全访问

	/**
	 * @struct _CodeGroup
	 * @brief 合约组合结构体
	 * 
	 * 存储合约组合的配置信息，包括组合名称和组合中各合约的权重。
	 */
	typedef struct _CodeGroup
	{
		char	_name[32] = { 0 };  // 组合名称（最多32个字符）
		wt_hashmap<std::string, double>	_items;  // 组合项映射表，键为合约代码，值为权重
	} CodeGroup;  // 合约组合结构体类型别名
	typedef std::shared_ptr<CodeGroup> CodeGroupPtr;  // 合约组合智能指针类型别名
	typedef wt_hashmap<std::string, CodeGroupPtr>	CodeGroups;  // 合约组合映射表类型别名，键为组合名称
	CodeGroups				_groups;			// 合约组合映射表（组合名称到组合的映射）
	CodeGroups				_code_to_groups;	// 合约代码到组合的映射，用于快速查找合约所属的组合

	wt_hashset<std::string>	_clear_includes;	// 自动清理包含品种集合，只有在集合中的品种才会自动清理
	wt_hashset<std::string>	_clear_excludes;	// 自动清理排除品种集合，在集合中的品种不会自动清理

	wt_hashset<std::string> _channel_holds;		// 通道持仓集合，记录交易通道中实际持有的合约

	wt_hashmap<std::string, double> _target_pos;  // 目标仓位映射表，键为合约代码，值为目标持仓数量

	typedef std::shared_ptr<boost::threadpool::pool> ThreadPoolPtr;  // 线程池智能指针类型别名
	ThreadPoolPtr		_pool;  // 线程池指针，用于并发处理执行单元回调
};

typedef std::shared_ptr<IExecCommand> ExecCmdPtr;  // 执行命令智能指针类型别名

NS_WTP_END  // 结束WonderTrader命名空间
