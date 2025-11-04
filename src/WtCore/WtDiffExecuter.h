/*!
 * \file WtExecuter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 差量执行器头文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件定义了WtDiffExecuter类，用于实现差量执行策略。
 * 
 * 核心功能：
 * 1. 差量执行：根据目标仓位和当前仓位的差值进行交易执行，而非直接设置目标仓位
 * 2. 仓位管理：维护目标仓位和差量仓位，跟踪仓位变化
 * 3. 执行单元管理：为每个合约创建执行单元（ExecuteUnit），负责具体的交易执行逻辑
 * 4. 线程池支持：可选地使用线程池并发处理多个合约的执行逻辑
 * 5. 数据持久化：支持将目标仓位和差量仓位保存到文件，以便系统重启后恢复
 * 
 * 设计特点：
 * - 继承自ExecuteContext和IExecCommand，实现执行上下文和执行命令接口
 * - 实现ITrdNotifySink和IExecCommand接口，接收交易回报和行情数据
 * - 使用差量方式管理仓位，只执行需要调整的部分
 * - 支持多合约并发执行，通过线程池提高性能
 * - 支持执行器策略配置，可以为不同品种配置不同的执行策略
 * 
 * 与普通执行器的区别：
 * - 差量执行器记录目标仓位和差量仓位，每次只执行差量部分
 * - 成交后会自动更新差量仓位，确保差量准确
 * - 适合需要精确控制执行节奏的场景
 */
#pragma once  // 防止头文件重复包含

#include "ITrdNotifySink.h"  // 包含交易回报通知接口头文件
#include "IExecCommand.h"  // 包含执行命令接口头文件
#include "WtExecuterFactory.h"  // 包含执行器工厂头文件
#include "../Includes/ExecuteDefs.h"  // 包含执行器相关定义头文件
#include "../Share/threadpool.hpp"  // 包含线程池头文件
#include "../Share/SpinMutex.hpp"  // 包含自旋锁头文件

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：变体类型，用于配置参数传递
class IDataManager;  // 前向声明：数据管理器接口
class IBaseDataMgr;  // 前向声明：基础数据管理器接口
class TraderAdapter;  // 前向声明：交易适配器类
class IHotMgr;  // 前向声明：热点合约管理器接口

/**
 * @class WtDiffExecuter
 * @brief 差量执行器类
 * 
 * 该类实现了差量执行策略，根据目标仓位和当前仓位的差值进行交易执行。
 * 继承自ExecuteContext（提供执行上下文接口）和ITrdNotifySink、IExecCommand（提供交易回报和命令接口）。
 * 
 * 主要职责：
 * 1. 管理目标仓位和差量仓位，跟踪仓位变化
 * 2. 为每个合约创建执行单元，负责具体的交易执行
 * 3. 接收交易回报和行情数据，更新内部状态
 * 4. 将仓位变化传递给执行单元，由执行单元执行交易
 * 5. 支持数据持久化，保存和恢复仓位信息
 * 
 * 工作流程：
 * 1. 初始化：加载配置参数，创建线程池（可选），恢复历史仓位数据
 * 2. 设置目标仓位：接收目标仓位映射，计算差量，传递给执行单元
 * 3. 仓位变动：接收仓位变动通知，更新差量，传递给执行单元
 * 4. 交易回报：接收成交回报，更新差量仓位，传递给执行单元
 * 5. 数据持久化：定期保存目标仓位和差量仓位到文件
 */
class WtDiffExecuter : public ExecuteContext,  // 继承执行上下文接口
		public ITrdNotifySink, public IExecCommand  // 继承交易回报通知接口和执行命令接口
{
public:
	/**
	 * @brief 构造函数
	 * @param factory 执行器工厂指针，用于创建执行单元
	 * @param name 执行器名称字符串
	 * @param dataMgr 数据管理器指针，用于获取行情数据
	 * @param bdMgr 基础数据管理器指针，用于获取合约信息
	 * 
	 * 初始化差量执行器，设置工厂指针、名称和数据管理器。
	 * 初始化所有成员变量：交易通道就绪标志为false，放大倍数为1.0，交易适配器为NULL。
	 */
	WtDiffExecuter(WtExecuterFactory* factory, const char* name, IDataManager* dataMgr, IBaseDataMgr* bdMgr);
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，等待线程池中所有任务完成。
	 */
	virtual ~WtDiffExecuter();

public:
	/**
	 * @brief 初始化执行器
	 * @param params 初始化参数，包含放大倍数、线程池大小、执行策略等配置
	 * @return bool 初始化成功返回true，否则返回false
	 * 
	 * 解析初始化参数，设置放大倍数和线程池大小。
	 * 如果配置了线程池大小，创建线程池。
	 * 加载历史仓位数据（目标仓位和差量仓位）。
	 */
	bool init(WTSVariant* params);

	/**
	 * @brief 设置交易适配器
	 * @param adapter 交易适配器指针，用于执行交易
	 * 
	 * 保存交易适配器指针，读取交易适配器的就绪状态。
	 */
	void setTrader(TraderAdapter* adapter);

private:
	/**
	 * @brief 获取执行单元
	 * @param code 标准合约代码字符串
	 * @param bAutoCreate 是否自动创建执行单元，默认为true
	 * @return ExecuteUnitPtr 返回执行单元智能指针，如果不存在且不自动创建则返回空指针
	 * 
	 * 根据合约代码获取执行单元，如果不存在且bAutoCreate为true，则创建新的执行单元。
	 * 创建时会根据合约品种查找对应的执行策略配置，如果未配置则使用默认策略。
	 * 如果交易通道已就绪，会立即通知执行单元。
	 */
	ExecuteUnitPtr	getUnit(const char* code, bool bAutoCreate = true);

	/**
	 * @brief 保存数据到文件
	 * 
	 * 将目标仓位和差量仓位保存到JSON文件，用于系统重启后恢复。
	 * 文件保存在执行器数据目录下，文件名为执行器名称.json。
	 */
	void	save_data();
	/**
	 * @brief 从文件加载数据
	 * 
	 * 从JSON文件加载目标仓位和差量仓位，恢复系统状态。
	 * 如果文件不存在或格式错误，则跳过加载。
	 */
	void	load_data();

public:
	//////////////////////////////////////////////////////////////////////////
	// ExecuteContext接口实现
	// 以下方法实现ExecuteContext接口，为执行单元提供数据访问接口
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 获取Tick数据切片
	 * @param code 标准合约代码字符串
	 * @param count 获取的Tick数量
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSTickSlice* 返回Tick数据切片指针，如果数据管理器无效返回NULL
	 * 
	 * 从数据管理器获取指定数量的Tick数据。
	 */
	virtual WTSTickSlice* getTicks(const char* code, uint32_t count, uint64_t etime = 0) override;

	/**
	 * @brief 获取最后一个Tick数据
	 * @param code 标准合约代码字符串
	 * @return WTSTickData* 返回最后一个Tick数据指针，如果数据管理器无效返回NULL
	 * 
	 * 从数据管理器获取最新的Tick数据。
	 */
	virtual WTSTickData*	grabLastTick(const char* code) override;

	/**
	 * @brief 获取持仓数量
	 * @param stdCode 标准合约代码字符串
	 * @param validOnly 是否只返回有效持仓，默认为true
	 * @param flag 持仓标志（1=今仓，2=昨仓，3=全部），默认为3
	 * @return double 返回持仓数量，如果交易适配器无效返回0.0
	 * 
	 * 从交易适配器获取指定合约的持仓数量。
	 */
	virtual double		getPosition(const char* stdCode, bool validOnly = true, int32_t flag = 3) override;
	/**
	 * @brief 获取订单映射
	 * @param code 标准合约代码字符串
	 * @return OrderMap* 返回订单映射指针，如果交易适配器无效返回NULL
	 * 
	 * 从交易适配器获取指定合约的所有订单。
	 */
	virtual OrderMap*	getOrders(const char* code) override;
	/**
	 * @brief 获取未完成数量
	 * @param code 标准合约代码字符串
	 * @return double 返回未完成数量，如果交易适配器无效返回0.0
	 * 
	 * 从交易适配器获取指定合约的未完成订单数量。
	 */
	virtual double		getUndoneQty(const char* code) override;

	/**
	 * @brief 买入
	 * @param code 标准合约代码字符串
	 * @param price 买入价格
	 * @param qty 买入数量
	 * @param bForceClose 是否强制平仓，默认为false
	 * @return OrderIDs 返回订单ID列表
	 * 
	 * 通过交易适配器提交买入订单。
	 * 如果交易通道未就绪，返回空列表。
	 */
	virtual OrderIDs	buy(const char* code, double price, double qty, bool bForceClose = false) override;
	/**
	 * @brief 卖出
	 * @param code 标准合约代码字符串
	 * @param price 卖出价格
	 * @param qty 卖出数量
	 * @param bForceClose 是否强制平仓，默认为false
	 * @return OrderIDs 返回订单ID列表
	 * 
	 * 通过交易适配器提交卖出订单。
	 * 如果交易通道未就绪，返回空列表。
	 */
	virtual OrderIDs	sell(const char* code, double price, double qty, bool bForceClose = false) override;
	/**
	 * @brief 撤单（按订单ID）
	 * @param localid 本地订单ID
	 * @return bool 撤单成功返回true，否则返回false
	 * 
	 * 通过交易适配器撤销指定订单。
	 * 如果交易通道未就绪，返回false。
	 */
	virtual bool		cancel(uint32_t localid) override;
	/**
	 * @brief 撤单（按合约代码和方向）
	 * @param code 标准合约代码字符串
	 * @param isBuy 是否为买入方向
	 * @param qty 撤销数量
	 * @return OrderIDs 返回撤销的订单ID列表
	 * 
	 * 通过交易适配器撤销指定合约和方向的订单。
	 * 如果交易通道未就绪，返回空列表。
	 */
	virtual OrderIDs	cancel(const char* code, bool isBuy, double qty) override;
	/**
	 * @brief 写入日志
	 * @param message 日志消息字符串
	 * 
	 * 记录执行器日志，日志包含执行器名称前缀。
	 */
	virtual void		writeLog(const char* message) override;

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码字符串
	 * @return WTSCommodityInfo* 返回商品信息指针
	 * 
	 * 从执行器存根获取商品信息。
	 */
	virtual WTSCommodityInfo*	getCommodityInfo(const char* stdCode) override;
	/**
	 * @brief 获取交易会话信息
	 * @param stdCode 标准合约代码字符串
	 * @return WTSSessionInfo* 返回交易会话信息指针
	 * 
	 * 从执行器存根获取交易会话信息。
	 */
	virtual WTSSessionInfo*		getSessionInfo(const char* stdCode) override;

	/**
	 * @brief 获取当前时间
	 * @return uint64_t 返回当前时间戳（纳秒级）
	 * 
	 * 从执行器存根获取当前时间。
	 */
	virtual uint64_t	getCurTime() override;

public:
	/**
	 * @brief 设置目标仓位
	 * @param targets 目标仓位映射表，键为合约代码，值为目标持仓数量
	 * 
	 * 接收目标仓位映射表，计算每个合约的差量，传递给执行单元执行。
	 * 如果目标仓位为0（不在映射表中），会自动将仓位设置为0。
	 * 更新后会保存数据到文件。
	 * 
	 * 实现逻辑：
	 * 1. 遍历目标仓位映射表，计算每个合约的差量
	 * 2. 将差量传递给执行单元执行
	 * 3. 检查原目标仓位中不在新映射表中的合约，将其设置为0
	 */
	virtual void set_position(const wt_hashmap<std::string, double>& targets) override;


	/**
	 * @brief 合约仓位变动通知
	 * @param stdCode 标准合约代码字符串
	 * @param diffPos 仓位变动数量（正数表示增加，负数表示减少）
	 * 
	 * 当合约仓位发生变化时被调用，更新目标仓位和差量仓位。
	 * 如果差量为0，则直接返回。
	 * 更新后会传递给执行单元执行。
	 * 
	 * 实现逻辑：
	 * 1. 获取或创建执行单元
	 * 2. 检查差量是否为0，如果为0则返回
	 * 3. 应用放大倍数，更新目标仓位和差量仓位
	 * 4. 检查交易限制，如果被限制则返回
	 * 5. 将差量传递给执行单元执行（可选使用线程池）
	 */
	virtual void on_position_changed(const char* stdCode, double diffPos) override;

	/**
	 * @brief 实时行情回调
	 * @param stdCode 标准合约代码字符串
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当收到实时行情数据时被调用，传递给执行单元处理。
	 * 如果执行单元不存在，则忽略。
	 * 
	 * 实现逻辑：
	 * 1. 获取执行单元（不自动创建）
	 * 2. 如果执行单元存在，将Tick数据传递给执行单元（可选使用线程池）
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;

	/**
	 * @brief 成交回报
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码字符串
	 * @param isBuy 是否为买入方向
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 当收到成交回报时被调用，更新差量仓位并传递给执行单元。
	 * 如果本地订单ID为0，则忽略。
	 * 
	 * 实现逻辑：
	 * 1. 获取执行单元（不自动创建）
	 * 2. 如果本地订单ID为0，则返回
	 * 3. 更新差量仓位（成交数量乘以方向）
	 * 4. 保存数据到文件
	 * 5. 将成交回报传递给执行单元（可选使用线程池）
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;

	/**
	 * @brief 订单回报
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码字符串
	 * @param isBuy 是否为买入方向
	 * @param totalQty 订单总数量
	 * @param leftQty 剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤销，默认为false
	 * 
	 * 当收到订单回报时被调用，传递给执行单元处理。
	 * 如果执行单元不存在，则忽略。
	 * 
	 * 实现逻辑：
	 * 1. 获取执行单元（不自动创建）
	 * 2. 如果执行单元存在，将订单回报传递给执行单元（可选使用线程池）
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled = false) override;

	/**
	 * @brief 持仓回报
	 * @param stdCode 标准合约代码字符串
	 * @param isLong 是否为多头方向
	 * @param prevol 上一次持仓数量
	 * @param preavail 上一次可用持仓数量
	 * @param newvol 新持仓数量
	 * @param newavail 新可用持仓数量
	 * @param tradingday 交易日（格式：YYYYMMDD）
	 *	
	 * 当收到持仓回报时被调用，目前未实现具体逻辑。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;

	/**
	 * @brief 委托回报
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码字符串
	 * @param bSuccess 是否成功
	 * @param message 消息字符串
	 *	
	 * 当收到委托回报时被调用，传递给执行单元处理。
	 * 如果执行单元不存在，则忽略。
	 * 
	 * 实现逻辑：
	 * 1. 获取执行单元（不自动创建）
	 * 2. 如果执行单元存在，将委托回报传递给执行单元（可选使用线程池）
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;

	/**
	 * @brief 交易通道就绪
	 * 
	 * 当交易通道就绪时被调用，通知所有执行单元，并恢复所有差量仓位。
	 * 
	 * 实现逻辑：
	 * 1. 设置通道就绪标志为true
	 * 2. 通知所有执行单元通道就绪
	 * 3. 恢复所有差量仓位（重新设置每个合约的差量）
	 */
	virtual void on_channel_ready() override;

	/**
	 * @brief 交易通道丢失
	 * 
	 * 当交易通道丢失时被调用，通知所有执行单元。
	 * 
	 * 实现逻辑：
	 * 1. 设置通道就绪标志为false
	 * 2. 通知所有执行单元通道丢失
	 */
	virtual void on_channel_lost() override;

	/**
	 * @brief 资金回报
	 * @param currency 货币类型字符串
	 * @param prebalance 上一次资金余额
	 * @param balance 资金余额
	 * @param dynbalance 动态资金余额
	 * @param avaliable 可用资金
	 * @param closeprofit 平仓盈亏
	 * @param dynprofit 浮动盈亏
	 * @param margin 占用保证金
	 * @param fee 手续费
	 * @param deposit 入金
	 * @param withdraw 出金
	 * 
	 * 当收到资金回报时被调用，传递给所有执行单元处理。
	 * 
	 * 实现逻辑：
	 * 1. 遍历所有执行单元
	 * 2. 将资金回报传递给每个执行单元（可选使用线程池）
	 */
	virtual void on_account(const char* currency, double prebalance, double balance, double dynbalance,
		double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw) override;


private:
	ExecuteUnitMap		_unit_map;  // 执行单元映射表，键为合约代码，值为执行单元智能指针
	TraderAdapter*		_trader;  // 交易适配器指针，用于执行交易
	WtExecuterFactory*	_factory;  // 执行器工厂指针，用于创建执行单元
	IDataManager*		_data_mgr;  // 数据管理器指针，用于获取行情数据
	IBaseDataMgr*		_bd_mgr;  // 基础数据管理器指针，用于获取合约信息
	WTSVariant*			_config;  // 配置参数指针，包含执行策略等配置

	double				_scale;  // 放大倍数，用于调整仓位数量（例如：1.0表示不放大，2.0表示放大2倍）
	bool				_channel_ready;  // 交易通道就绪标志，true表示通道就绪，false表示通道未就绪

	SpinMutex			_mtx_units;  // 自旋锁，用于保护执行单元映射表的线程安全

	wt_hashmap<std::string, double> _target_pos;  // 目标仓位映射表，键为合约代码，值为目标持仓数量
	wt_hashmap<std::string, double> _diff_pos;  // 差量仓位映射表，键为合约代码，值为差量持仓数量（需要调整的数量）

	typedef std::shared_ptr<boost::threadpool::pool> ThreadPoolPtr;  // 线程池智能指针类型定义
	ThreadPoolPtr		_pool;  // 线程池指针，用于并发处理多个合约的执行逻辑（可选）
};
NS_WTP_END  // 结束WonderTrader命名空间
