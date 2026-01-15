/*!
 * \file TraderAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易适配器类定义，封装交易接口的调用和数据回调处理
 * 
 * 本文件定义了两个核心类：
 * 1. TraderAdapter - 交易适配器类，封装单个交易通道的接口调用
 * 2. TraderAdapterMgr - 交易适配器管理器，管理多个交易通道适配器
 * 
 * 设计说明：
 * - TraderAdapter实现ITraderSpi接口，接收交易接口的回调通知
 * - 通过动态加载交易模块DLL/so，创建交易接口实例
 * - 将交易接口返回的数据转换为标准格式，通过Dumper传递给外部回调
 * - TraderAdapterMgr管理多个适配器，提供统一的启动、停止、刷新接口
 * 
 * 数据流程：
 * 交易接口 -> TraderAdapter回调 -> Dumper -> 外部回调函数
 */
#pragma once
#include <atomic>                    // 原子操作支持，用于线程安全的计数器
#include <unordered_map>             // 无序映射容器，用于存储适配器映射
#include <boost/noncopyable.hpp>     // Boost库的不可复制基类，禁止拷贝构造和赋值

#include "../Includes/ExecuteDefs.h"  // 执行相关定义

#include "../Includes/ITraderApi.h"   // 交易接口基类定义

NS_WTP_BEGIN                          // WonderTrader命名空间开始
class WTSVariant;                     // 前向声明：变体类型，用于配置参数
class ActionPolicyMgr;                 // 前向声明：动作策略管理器
class WTSContractInfo;                // 前向声明：合约信息类
class WTSCommodityInfo;                // 前向声明：品种信息类
class WtExecuter;                     // 前向声明：执行器类
class EventNotifier;                  // 前向声明：事件通知器

class ITrdNotifySink;                 // 前向声明：交易通知接收接口

class TraderAdapterMgr;               // 前向声明：交易适配器管理器

/**
 * @class TraderAdapter
 * @brief 交易适配器类，封装单个交易通道的接口调用和数据回调处理
 * 
 * 该类负责：
 * - 动态加载交易模块（如TraderCTP、TraderXTP等）
 * - 创建并初始化交易接口实例
 * - 实现ITraderSpi接口，接收交易接口的回调
 * - 将交易数据转换为标准格式，通过Dumper传递给外部
 * - 管理交易通道的连接、登录、查询等生命周期
 * 
 * 继承关系：
 * - 继承自ITraderSpi接口，实现交易接口的回调方法
 */
class TraderAdapter : public ITraderSpi
{
public:
	/**
	 * @brief 构造函数
	 * @param mgr 交易适配器管理器指针，用于与管理器交互
	 * 
	 * 初始化适配器，设置管理器指针，其他成员变量使用默认值。
	 */
	TraderAdapter(TraderAdapterMgr* mgr);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，但实际的资源释放应在release()方法中完成。
	 */
	~TraderAdapter();

public:
	/**
	 * @brief 初始化交易适配器
	 * @param id 交易通道ID（字符串），用于标识该交易通道
	 * @param params 配置参数（WTSVariant指针），包含模块名、账号、密码等配置
	 * @param bdMgr 基础数据管理器指针，用于获取合约、品种等信息
	 * @return 返回初始化是否成功（布尔值）
	 * 
	 * 初始化流程：
	 * 1. 保存通道ID和配置参数
	 * 2. 根据配置中的模块名，动态加载交易模块DLL/so
	 * 3. 调用模块的createTrader函数创建交易接口实例
	 * 4. 初始化交易接口，传入配置参数
	 */
	bool init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr);

	/**
	 * @brief 释放资源
	 * 
	 * 释放交易接口资源：
	 * - 注销回调接口
	 * - 调用交易接口的release方法
	 * - 释放配置参数引用
	 */
	void release();

	/**
	 * @brief 启动交易适配器
	 * @return 返回启动是否成功（布尔值）
	 * 
	 * 启动流程：
	 * - 注册回调接口（this）
	 * - 连接交易服务器
	 * - 连接成功后会自动触发登录
	 */
	bool run();

	/**
	 * @brief 获取交易通道ID
	 * @return 返回通道ID的C风格字符串指针
	 */
	inline const char* id() const{ return _id.c_str(); }

	/**
	 * @brief 检查数据查询是否完成
	 * @return 返回是否已完成（布尔值），true表示已完成所有数据查询
	 * 
	 * 用于判断该交易通道是否已完成账户、持仓、订单、成交的查询。
	 */
	bool isDone() const { return _done; }

	/**
	 * @brief 查询账户资金
	 * 
	 * 主动查询账户资金信息，查询结果会通过onRspAccount回调返回。
	 * 只有在交易日已确定（_date不为0）时才会执行查询。
	 */
	void queryFund();
	
	/**
	 * @brief 查询持仓
	 * 
	 * 主动查询持仓信息，查询结果会通过onRspPosition回调返回。
	 * 只有在交易日已确定（_date不为0）时才会执行查询。
	 */
	void queryPosition();

public:
	//////////////////////////////////////////////////////////////////////////
	//ITraderSpi接口实现
	// 以下方法实现ITraderSpi接口，接收交易接口的回调通知
	
	/**
	 * @brief 处理交易事件
	 * @param e 交易事件类型（WTSTraderEvent枚举）
	 * @param ec 事件代码（32位整数），0表示成功，非0表示错误码
	 * 
	 * 处理交易接口的事件通知：
	 * - WTE_Connect: 连接事件，连接成功后自动登录
	 * - WTE_Close: 连接关闭事件，记录日志
	 */
	virtual void handleEvent(WTSTraderEvent e, int32_t ec) override;

	/**
	 * @brief 登录结果回调
	 * @param bSucc 登录是否成功（布尔值）
	 * @param msg 登录结果消息（字符串），失败时包含错误信息
	 * @param tradingdate 交易日（32位无符号整数），格式为YYYYMMDD
	 * 
	 * 登录成功后会：
	 * - 保存交易日
	 * - 开始查询持仓（然后依次查询账户、成交、订单）
	 * 
	 * 登录失败会：
	 * - 标记适配器为完成状态
	 * - 通知管理器减少活跃计数
	 */
	virtual void onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate) override;

	/**
	 * @brief 登出回调
	 * 
	 * 交易接口登出时的通知，当前实现为空。
	 */
	virtual void onLogout() override;

	/**
	 * @brief 账户资金查询结果回调
	 * @param ayAccounts 账户信息数组（WTSArray指针），包含一个或多个账户的资金信息
	 * 
	 * 处理账户资金查询结果：
	 * - 遍历所有账户信息
	 * - 提取账户数据（余额、盈亏、保证金等）
	 * - 通过Dumper传递给外部回调函数
	 * - 查询完成后，继续查询成交明细
	 */
	virtual void onRspAccount(WTSArray* ayAccounts) override;

	/**
	 * @brief 持仓查询结果回调
	 * @param ayPositions 持仓信息数组（WTSArray指针），包含所有持仓信息
	 * 
	 * 处理持仓查询结果：
	 * - 遍历所有持仓信息
	 * - 提取持仓数据（数量、成本、盈亏等）
	 * - 通过Dumper传递给外部回调函数
	 * - 查询完成后，继续查询账户资金
	 */
	virtual void onRspPosition(const WTSArray* ayPositions) override;

	/**
	 * @brief 成交查询结果回调
	 * @param ayTrades 成交信息数组（WTSArray指针），包含所有成交记录
	 * 
	 * 处理成交查询结果：
	 * - 遍历所有成交记录
	 * - 提取成交数据（价格、数量、方向等）
	 * - 通过Dumper传递给外部回调函数
	 * - 查询完成后，继续查询订单明细
	 */
	virtual void onRspTrades(const WTSArray* ayTrades) override;

	/**
	 * @brief 订单查询结果回调
	 * @param ayOrders 订单信息数组（WTSArray指针），包含所有订单记录
	 * 
	 * 处理订单查询结果：
	 * - 遍历所有订单记录
	 * - 提取订单数据（状态、数量、价格等）
	 * - 通过Dumper传递给外部回调函数
	 * - 查询完成后，标记适配器为完成状态，通知管理器
	 */
	virtual void onRspOrders(const WTSArray* ayOrders) override;

	/**
	 * @brief 成交推送回调
	 * @param tradeRecord 成交记录（WTSTradeInfo指针），实时推送的成交信息
	 * 
	 * 处理实时成交推送：
	 * - 提取成交数据
	 * - 通过Dumper传递给外部回调函数
	 * - 与查询结果不同，推送的isLast始终为true
	 */
	virtual void onPushTrade(WTSTradeInfo* tradeRecord) override;

	/**
	 * @brief 订单推送回调
	 * @param orderInfo 订单信息（WTSOrderInfo指针），实时推送的订单状态变化
	 * 
	 * 处理实时订单推送：
	 * - 如果订单已结束（非活跃状态），刷新账户和持仓
	 * - 提取订单数据
	 * - 通过Dumper传递给外部回调函数
	 */
	virtual void onPushOrder(WTSOrderInfo* orderInfo) override;

	/**
	 * @brief 交易错误回调
	 * @param err 错误信息（WTSError指针），包含错误代码和消息
	 * @param pData 附加数据指针（可选），默认为NULL
	 * 
	 * 处理交易接口的错误通知，记录错误日志。
	 */
	virtual void onTraderError(WTSError* err, void* pData = NULL) override;

	/**
	 * @brief 获取基础数据管理器
	 * @return 返回基础数据管理器指针
	 * 
	 * 返回初始化时传入的基础数据管理器，供交易接口使用。
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override;

	/**
	 * @brief 处理交易日志
	 * @param ll 日志级别（WTSLogLevel枚举）
	 * @param message 日志消息（字符串）
	 * 
	 * 将交易接口的日志转发到WonderTrader日志系统。
	 */
	virtual void handleTraderLog(WTSLogLevel ll, const char* message) override;

private:
	TraderAdapterMgr*	_mgr;         // 交易适配器管理器指针，用于与管理器交互
	WTSVariant*			_cfg;         // 配置参数指针，保存交易通道的配置信息
	std::string			_id;          // 交易通道ID字符串，用于标识该通道

	ITraderApi*			_trader_api;  // 交易接口指针，指向动态加载的交易接口实例
	FuncDeleteTrader	_remover;     // 交易接口删除函数指针，用于释放交易接口实例

	IBaseDataMgr*		_bd_mgr;      // 基础数据管理器指针，用于获取合约、品种等信息
	uint32_t			_date;        // 当前交易日（32位无符号整数），格式为YYYYMMDD，登录成功后设置

	bool				_done;        // 数据查询完成标志（布尔值），true表示已完成所有数据查询
};

typedef std::shared_ptr<TraderAdapter>				TraderAdapterPtr;  // 交易适配器智能指针类型定义
typedef std::unordered_map<std::string, TraderAdapterPtr>	TraderAdapterMap;  // 交易适配器映射表类型定义，key为通道ID，value为适配器指针

//////////////////////////////////////////////////////////////////////////
//TraderAdapterMgr - 交易适配器管理器类
/**
 * @class TraderAdapterMgr
 * @brief 交易适配器管理器，管理多个交易通道适配器
 * 
 * 该类负责：
 * - 管理多个TraderAdapter实例
 * - 统一启动和停止所有适配器
 * - 跟踪适配器的完成状态
 * - 提供定时刷新功能
 * 
 * 设计特点：
 * - 继承boost::noncopyable，禁止拷贝构造和赋值
 * - 使用互斥锁保护共享数据
 * - 使用原子计数器跟踪活跃适配器数量
 */
class TraderAdapterMgr : private boost::noncopyable
{
public:
	/**
	 * @brief 释放所有适配器资源
	 * 
	 * 遍历所有适配器，调用其release方法释放资源，然后清空适配器映射表。
	 */
	void	release();

	/**
	 * @brief 启动所有适配器
	 * 
	 * 启动流程：
	 * - 设置活跃计数为适配器总数
	 * - 遍历所有适配器，调用run方法启动
	 * - 记录启动日志
	 */
	void	run();

	/**
	 * @brief 获取所有适配器的映射表
	 * @return 返回适配器映射表的常量引用
	 * 
	 * 用于外部访问适配器映射表，只读访问。
	 */
	const TraderAdapterMap& getAdapters() const { return _adapters; }

	/**
	 * @brief 根据通道ID获取适配器
	 * @param tname 交易通道ID（字符串）
	 * @return 返回适配器智能指针，如果不存在返回空指针
	 * 
	 * 在适配器映射表中查找指定ID的适配器。
	 */
	TraderAdapterPtr getAdapter(const char* tname);

	/**
	 * @brief 添加适配器到管理器
	 * @param tname 交易通道ID（字符串）
	 * @param adapter 适配器智能指针引用
	 * @return 返回添加是否成功（布尔值）
	 * 
	 * 添加适配器到映射表，如果ID已存在则添加失败。
	 */
	bool	addAdapter(const char* tname, TraderAdapterPtr& adapter);

	/**
	 * @brief 检查是否有活跃的适配器
	 * @return 返回是否有活跃适配器（布尔值）
	 * 
	 * 检查活跃计数器是否大于0，用于判断是否还有适配器在工作。
	 */
	bool	isAnyAlive() const {
		return _live_cnt != 0;      // 如果活跃计数不为0，说明还有适配器在工作
	}

	/**
	 * @brief 获取适配器数量
	 * @return 返回适配器映射表的大小（size_t类型）
	 */
	std::size_t size() const { return _adapters.size(); }

	/**
	 * @brief 减少活跃适配器计数
	 * 
	 * 当一个适配器完成数据查询后，调用此方法减少活跃计数。
	 * 使用互斥锁保护计数器的原子操作。
	 * 当剩余活跃数较少时，会记录日志。
	 */
	void decAlive();

	/**
	 * @brief 刷新所有已完成的适配器
	 * 
	 * 遍历所有适配器，对于已完成的适配器，重新查询持仓和资金。
	 * 用于定时刷新功能，保持数据最新。
	 */
	void refresh();

private:
	TraderAdapterMap		_adapters;   // 适配器映射表，key为通道ID，value为适配器智能指针
	std::mutex				_mutex;      // 互斥锁，保护共享数据的并发访问
	std::atomic<uint32_t>	_live_cnt;   // 原子计数器：活跃适配器数量，线程安全
};

NS_WTP_END                          // WonderTrader命名空间结束
