/*!
 * \file Dumper.cpp
 * \project	WonderTrader
 *
 * \brief Dumper类的实现文件，实现交易数据转储的所有功能
 * 
 * 本文件实现了Dumper类的所有方法，包括：
 * - 初始化日志系统
 * - 加载配置文件并初始化交易通道
 * - 启动和停止数据转储流程
 * - 处理交易数据的转发
 * 
 * 全局对象：
 * - g_bdMgr: 基础数据管理器，管理交易时段、品种、合约等信息
 * - g_adapterMgr: 交易适配器管理器，管理所有交易通道适配器
 */
#include "Dumper.h"                  // 包含类定义头文件
#include "WtHelper.h"                // 包含辅助工具类
#include "TraderAdapter.h"           // 包含交易适配器类

#include "../WTSTools/WTSLogger.h"   // WonderTrader日志工具
#include "../WTSTools/WTSBaseDataMgr.h"  // 基础数据管理器
#include "../WTSUtils/WTSCfgLoader.h"    // 配置文件加载器

#include "../Includes/WTSVariant.hpp"  // 变体类型，用于配置参数

USING_NS_WTP;                        // 使用WonderTrader命名空间

WTSBaseDataMgr		g_bdMgr;          // 全局基础数据管理器实例，管理交易时段、品种、合约等基础数据
TraderAdapterMgr	g_adapterMgr;     // 全局交易适配器管理器实例，管理所有交易通道适配器

/**
 * @brief 初始化日志系统的实现
 * @param logProfile 日志配置文件路径
 * 
 * 初始化WonderTrader的日志系统，加载日志配置。
 */
void Dumper::init(const char* logProfile)
{
	WTSLogger::init(logProfile);     // 调用WonderTrader日志系统的初始化方法，传入日志配置文件名
}

/**
 * @brief 加载配置文件并初始化交易通道的实现
 * @param cfgfile 配置文件路径或配置内容
 * @param isFile 是否为文件路径
 * @param modDir 模块目录路径
 * @return 返回配置是否成功
 * 
 * 配置流程：
 * 1. 设置模块目录路径
 * 2. 加载配置文件（YAML格式）
 * 3. 读取刷新间隔配置
 * 4. 加载基础数据文件（交易时段、品种、合约）
 * 5. 创建并初始化所有活跃的交易通道适配器
 */
bool Dumper::config(const char* cfgfile, bool isFile, const char* modDir)
{
	WtHelper::set_module_dir(modDir);  // 设置模块目录路径，用于后续定位交易模块DLL/so文件

	WTSVariant* root = NULL;          // 配置根节点指针
	if (isFile)                      // 如果cfgfile是文件路径
		root = WTSCfgLoader::load_from_file(cfgfile);  // 从文件加载配置
	else                             // 如果cfgfile是配置内容字符串
		root = WTSCfgLoader::load_from_content(cfgfile, false);  // 从字符串内容加载配置

	WTSVariant* cfg = root->get("config");  // 获取config配置节点
	if(cfg)                          // 如果config节点存在
	{
		if(cfg->has("refresh_span"))  // 如果配置中有refresh_span项
		{
			_refresh_span = cfg->getUInt32("refresh_span");  // 读取刷新间隔（秒）
		}
		
	}

	//基础数据文件
	WTSVariant* cfgBF = root->get("basefiles");  // 获取basefiles配置节点（基础数据文件配置）
	if (cfgBF->get("session"))      // 如果配置中有session项（交易时段文件）
	{
		g_bdMgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段数据
		WTSLogger::info("Trading sessions loaded");  // 记录日志
	}

	WTSVariant* cfgItem = cfgBF->get("commodity");  // 获取commodity配置项（品种文件）
	if (cfgItem)                     // 如果配置项存在
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件路径）
		{
			g_bdMgr.loadCommodities(cfgItem->asCString());  // 加载品种数据
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件路径）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组
			{
				g_bdMgr.loadCommodities(cfgItem->get(i)->asCString());  // 加载每个品种文件
			}
		}
	}

	cfgItem = cfgBF->get("contract");  // 获取contract配置项（合约文件）
	if (cfgItem)                     // 如果配置项存在
	{
		if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件路径）
		{
			g_bdMgr.loadContracts(cfgItem->asCString());  // 加载合约数据
		}
		else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件路径）
		{
			for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组
			{
				g_bdMgr.loadContracts(cfgItem->get(i)->asCString());  // 加载每个合约文件
			}
		}
	}

	cfg = root->get("traders");      // 获取traders配置节点（交易通道配置列表）
	for (uint32_t idx = 0; idx < cfg->size(); idx++)  // 遍历所有交易通道配置
	{
		WTSVariant* cfgItem = cfg->get(idx);  // 获取第idx个交易通道配置
		if (!cfgItem->getBoolean("active"))  // 如果该通道未激活，跳过
			continue;

		const char* channelid = cfgItem->getCString("channelid");  // 获取通道ID

		TraderAdapterPtr adapter(new TraderAdapter(&g_adapterMgr));  // 创建交易适配器实例
		adapter->init(channelid, cfgItem, &g_bdMgr);  // 初始化适配器，传入通道ID、配置、基础数据管理器
		g_adapterMgr.addAdapter(channelid, adapter);  // 将适配器添加到管理器
	}

	root->release();                 // 释放配置根节点，减少引用计数

	WTSLogger::info("交易数据落地模块初始化完成，主动刷新间隔:{}s", _refresh_span);  // 记录成功日志

	return true;                      // 返回成功
}

/**
 * @brief 启动数据转储的实现
 * @param bOnce 是否只运行一次，默认true
 * 
 * 启动流程：
 * - 启动所有交易适配器
 * - 如果bOnce为true，阻塞等待所有数据查询完成
 * - 如果bOnce为false，启动后台线程定时刷新数据
 */
void Dumper::run(bool bOnce /* = true */)
{
	g_adapterMgr.run();              // 启动所有交易适配器，开始连接和查询数据

	if(bOnce)                        // 如果是一次性模式
	{
		for (;;)                     // 无限循环
		{
			if (g_adapterMgr.isAnyAlive())  // 如果还有活跃的适配器在工作
				std::this_thread::sleep_for(std::chrono::seconds(1));  // 等待1秒
			else                     // 如果所有适配器都已完成
				break;               // 退出循环
		}
	}
	else                             // 如果是持续模式
	{
		_worker.reset(new StdThread([this]() {  // 创建后台工作线程
			
			while(!_stopped)         // 如果未停止
			{
				std::this_thread::sleep_for(std::chrono::seconds(_refresh_span));  // 等待刷新间隔时间
				g_adapterMgr.refresh();  // 刷新所有已完成的适配器数据
			}
			
		}));
	}
}

/**
 * @brief 释放资源并清理连接的实现
 * 
 * 释放流程：
 * - 设置停止标志，停止后台刷新线程
 * - 等待后台线程结束
 * - 释放所有交易适配器资源（在适配器管理器的release中完成）
 */
void Dumper::release()
{
	_stopped = true;                  // 设置停止标志，通知后台线程停止
	if (_worker)                     // 如果后台线程存在
		_worker->join();              // 等待后台线程结束
}

/**
 * @brief 处理账户资金数据的实现
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
 * 接收TraderAdapter传来的账户数据，如果回调函数已注册，则转发到外部回调函数。
 */
void Dumper::on_account(const char* channelid, uint32_t curTDate, const char* currency, double prebalance, 
		double balance, double dynbalance, double closeprofit, double dynprofit, double fee, 
		double margin, double deposit, double withdraw, bool isLast)
{
	if (_cb_account)                 // 如果账户回调函数已注册
		_cb_account(channelid, curTDate, currency, prebalance, balance, dynbalance, closeprofit, dynprofit, fee, margin, deposit, withdraw, isLast);  // 调用外部回调函数
}

/**
 * @brief 处理持仓数据的实现
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
 * 接收TraderAdapter传来的持仓数据，如果回调函数已注册，则转发到外部回调函数。
 */
void Dumper::on_position(const char* channelid, const char* exchg, const char* code, uint32_t curTDate, uint32_t direct,
		double volume, double cost, double margin, double avgpx, double dynprofit, uint32_t volscale, bool isLast)
{
	if (_cb_position)                // 如果持仓回调函数已注册
		_cb_position(channelid, exchg, code, curTDate, direct, volume, cost, margin, avgpx, dynprofit, volscale, isLast);  // 调用外部回调函数
}

/**
 * @brief 处理成交数据的实现
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
 * 接收TraderAdapter传来的成交数据，如果回调函数已注册，则转发到外部回调函数。
 */
void Dumper::on_trade(const char* channelid, const char* exchg, const char* code, uint32_t curTDate, const char* tradeid, const char* orderid, 
		uint32_t direct, uint32_t offset, double volume, double price, double amount, uint32_t ordertype, uint32_t tradetype, WtUInt64 tradetime, bool isLast)
{
	if (_cb_trade)                   // 如果成交回调函数已注册
		_cb_trade(channelid, exchg, code, curTDate, tradeid, orderid, direct, offset, volume, price, amount, ordertype, tradetype, tradetime, isLast);  // 调用外部回调函数
}

/**
 * @brief 处理订单数据的实现
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
 * 接收TraderAdapter传来的订单数据，如果回调函数已注册，则转发到外部回调函数。
 */
void Dumper::on_order(const char* channelid, const char* exchg, const char* code, uint32_t curTDate, const char* orderid, uint32_t direct, uint32_t offset, double volume, double leftover, double traded, double price, uint32_t ordertype, uint32_t pricetype, WtUInt64 ordertime, uint32_t state, const char* statemsg, bool isLast)
{
	if (_cb_order)                   // 如果订单回调函数已注册
		_cb_order(channelid, exchg, code, curTDate, orderid, direct, offset, volume, leftover, traded, price, ordertype, pricetype, ordertime, state, statemsg, isLast);  // 调用外部回调函数
}