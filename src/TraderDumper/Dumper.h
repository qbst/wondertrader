/*!
 * \file Dumper.h
 * \project	WonderTrader
 *
 * \brief 交易数据转储核心类定义
 * 
 * 本文件定义了Dumper类，这是TraderDumper模块的核心类。
 * 负责管理交易适配器、处理交易数据回调、转发数据到外部回调函数。
 * 
 * 设计说明：
 * - 管理TraderAdapterMgr，控制所有交易通道的生命周期
 * - 保存外部注册的回调函数，用于转发交易数据
 * - 支持一次性查询模式和持续刷新模式
 * - 在持续刷新模式下，启动后台线程定时刷新数据
 * 
 * 数据流程：
 * TraderAdapter回调 -> Dumper处理 -> 外部回调函数
 */
#pragma once
#include "PorterDefs.h"             // 包含回调函数类型定义
#include "../Share/StdUtils.hpp"    // 包含标准工具函数，提供StdThreadPtr等类型

/**
 * @class Dumper
 * @brief 交易数据转储核心类
 * 
 * 该类是TraderDumper模块的核心，负责：
 * - 管理交易适配器管理器（TraderAdapterMgr）
 * - 保存和调用外部注册的回调函数
 * - 初始化日志系统和配置
 * - 启动和停止数据转储流程
 * - 处理交易数据的转发
 */
class Dumper
{

public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化所有成员变量：
	 * - 所有回调函数指针初始化为nullptr
	 * - 刷新间隔初始化为10秒
	 * - 停止标志初始化为false
	 */
	Dumper(): 
		_cb_account(nullptr), _cb_order(nullptr), _cb_trade(nullptr),   // 初始化账户、订单、成交回调函数指针为空
		_cb_position(nullptr), _refresh_span(10), _stopped(false){}      // 初始化持仓回调函数指针为空，刷新间隔为10秒，停止标志为false

	/**
	 * @brief 注册数据回调函数
	 * @param cbAccount 账户资金信息回调函数指针
	 * @param cbOrder 订单信息回调函数指针
	 * @param cbTrade 成交信息回调函数指针
	 * @param cbPosition 持仓信息回调函数指针
	 * 
	 * 保存外部传入的四个回调函数指针，用于后续转发交易数据。
	 * 回调函数可以为NULL，表示不需要接收该类型的数据。
	 */
	void register_callbacks(FuncOnAccount cbAccount, FuncOnOrder cbOrder, FuncOnTrade cbTrade, FuncOnPosition cbPosition)
	{
		_cb_account = cbAccount;     // 保存账户回调函数指针
		_cb_order = cbOrder;         // 保存订单回调函数指针
		_cb_position = cbPosition;   // 保存持仓回调函数指针
		_cb_trade = cbTrade;         // 保存成交回调函数指针
	}

	/**
	 * @brief 初始化日志系统
	 * @param logProfile 日志配置文件路径
	 * 
	 * 初始化WonderTrader的日志系统，加载日志配置。
	 */
	void init(const char* logProfile);

	/**
	 * @brief 加载配置文件并初始化交易通道
	 * @param cfgfile 配置文件路径或配置内容
	 * @param isFile 是否为文件路径
	 * @param modDir 模块目录路径
	 * @return 返回配置是否成功
	 * 
	 * 配置流程：
	 * 1. 设置模块目录路径
	 * 2. 加载配置文件（YAML格式）
	 * 3. 加载基础数据文件（交易时段、品种、合约）
	 * 4. 创建并初始化所有活跃的交易通道适配器
	 */
	bool config(const char* cfgfile, bool isFile, const char* modDir);

	/**
	 * @brief 启动数据转储
	 * @param bOnce 是否只运行一次，默认true
	 * 
	 * 启动流程：
	 * - 启动所有交易适配器
	 * - 如果bOnce为true，阻塞等待所有数据查询完成
	 * - 如果bOnce为false，启动后台线程定时刷新数据
	 */
	void run(bool bOnce = true);

	/**
	 * @brief 释放资源并清理连接
	 * 
	 * 释放流程：
	 * - 设置停止标志，停止后台刷新线程
	 * - 等待后台线程结束
	 * - 释放所有交易适配器资源
	 */
	void release();

	/**
	 * @brief 处理账户资金数据
	 * @param channelid 交易通道ID
	 * @param curTDate 当前交易日
	 * @param currency 货币类型
	 * @param prebalance 上日余额
	 * @param balance 当前余额
	 * @param dynbalance 动态权益
	 * @param closeprofit 平仓盈亏
	 * @param dynprofit 浮动盈亏
	 * @param fee 手续费
	 * @param margin 占用保证金
	 * @param deposit 入金
	 * @param withdraw 出金
	 * @param isLast 是否为最后一条数据
	 * 
	 * 接收TraderAdapter传来的账户数据，转发到外部回调函数。
	 */
	void on_account(const char* channelid, uint32_t curTDate, const char* currency, double prebalance, double balance,
		double dynbalance, double closeprofit, double dynprofit, double fee, double margin, double deposit, double withdraw, bool isLast);

	/**
	 * @brief 处理订单数据
	 * @param channelid 交易通道ID
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param curTDate 当前交易日
	 * @param orderid 订单号
	 * @param direct 买卖方向
	 * @param offset 开平标志
	 * @param volume 委托数量
	 * @param leftover 剩余数量
	 * @param traded 已成交数量
	 * @param price 委托价格
	 * @param ordertype 订单类型
	 * @param pricetype 价格类型
	 * @param ordertime 委托时间
	 * @param state 订单状态
	 * @param statemsg 状态信息
	 * @param isLast 是否为最后一条数据
	 * 
	 * 接收TraderAdapter传来的订单数据，转发到外部回调函数。
	 */
	void on_order(const char* channelid, const char* exchg, const char* code, uint32_t curTDate,
		const char* orderid, uint32_t direct, uint32_t offset, double volume, double leftover, double traded, double price, uint32_t ordertype,
		uint32_t pricetype, WtUInt64 ordertime, uint32_t state, const char* statemsg, bool isLast);

	/**
	 * @brief 处理成交数据
	 * @param channelid 交易通道ID
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param curTDate 当前交易日
	 * @param tradeid 成交编号
	 * @param orderid 订单号
	 * @param direct 买卖方向
	 * @param offset 开平标志
	 * @param volume 成交数量
	 * @param price 成交价格
	 * @param amount 成交金额
	 * @param ordertype 订单类型
	 * @param tradetype 成交类型
	 * @param tradetime 成交时间
	 * @param isLast 是否为最后一条数据
	 * 
	 * 接收TraderAdapter传来的成交数据，转发到外部回调函数。
	 */
	void on_trade(const char* channelid, const char* exchg, const char* code, uint32_t curTDate,
		const char* tradeid, const char* orderid, uint32_t direct, uint32_t offset, double volume, double price,
		double amount, uint32_t ordertype, uint32_t tradetype, WtUInt64 tradetime, bool isLast);

	/**
	 * @brief 处理持仓数据
	 * @param channelid 交易通道ID
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param curTDate 当前交易日
	 * @param direct 持仓方向
	 * @param volume 持仓数量
	 * @param cost 持仓成本
	 * @param margin 占用保证金
	 * @param avgpx 持仓均价
	 * @param dynprofit 浮动盈亏
	 * @param volscale 数量乘数
	 * @param isLast 是否为最后一条数据
	 * 
	 * 接收TraderAdapter传来的持仓数据，转发到外部回调函数。
	 */
	void on_position(const char* channelid, const char* exchg, const char* code, uint32_t curTDate, uint32_t direct,
		double volume, double cost, double margin, double avgpx, double dynprofit, uint32_t volscale, bool isLast);

private:
	FuncOnAccount	_cb_account;     // 账户资金信息回调函数指针
	FuncOnOrder		_cb_order;       // 订单信息回调函数指针
	FuncOnTrade		_cb_trade;       // 成交信息回调函数指针
	FuncOnPosition	_cb_position;    // 持仓信息回调函数指针

	StdThreadPtr	_worker;         // 后台工作线程指针，用于定时刷新数据（持续模式）

	uint32_t		_refresh_span;   // 刷新间隔（秒），定时刷新数据的时间间隔
	bool			_stopped;        // 停止标志（布尔值），用于控制后台线程的停止
};

