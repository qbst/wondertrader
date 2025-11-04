/*!
 * \file MatchEngine.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 撮合引擎头文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了MatchEngine类，用于模拟订单撮合过程。
 *
 * 主要功能：
 * 1. 订单管理：接收买入/卖出订单，管理订单状态
 * 2. 订单撮合：根据tick数据模拟订单成交过程
 * 3. 限价订单簿（LOB）管理：维护价格档位的订单队列信息
 * 4. 订单排队：模拟订单在价格档位上的排队位置
 * 5. 撤单处理：支持订单撤销操作
 *
 * 设计特点：
 * - 支持限价单和市价单的撮合
 * - 模拟订单排队机制（考虑撤单率）
 * - 支持主动订单（对手价）和被动订单（挂单）
 * - 维护限价订单簿数据结构
 */
#pragma once
#include <stdint.h>                                                   // 标准整数类型定义
#include <map>                                                        // 映射容器
#include <vector>                                                     // 向量容器
#include <functional>                                                // 函数对象支持
#include <string.h>                                                   // 字符串操作函数

#include "../Includes/WTSMarcos.h"                                   // WonderTrader宏定义
#include "../Includes/WTSCollection.hpp"                             // WonderTrader集合类
#include "../Includes/FasterDefs.h"                                  // 快速定义

NS_WTP_BEGIN                                                          // WonderTrader命名空间开始
class WTSTickData;                                                   // Tick数据前向声明
class WTSVariant;                                                     // 变体类型前向声明
NS_WTP_END                                                            // WonderTrader命名空间结束

USING_NS_WTP;                                                         // 使用WonderTrader命名空间

/**
 * @brief 订单ID列表类型定义
 */
typedef std::vector<uint32_t> OrderIDs;                              // 订单ID列表

/**
 * @brief Tick缓存类型定义
 */
typedef WTSHashMap<std::string>	WTSTickCache;                      // Tick缓存哈希映射表

/**
 * @brief 撮合引擎回调接口
 * 
 * 用于接收撮合引擎的订单状态变化通知
 */
class IMatchSink
{
public:
	/**
	 * @brief 成交回报回调
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param vol 成交数量（这里没有正负，通过isBuy确定买入还是卖出）
	 * @param fireprice 触发价格
	 * @param price 成交价格
	 * @param ordTime 订单时间
	 */
	virtual void handle_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double fireprice, double price, uint64_t ordTime) = 0;  // 成交回报

	/**
	 * @brief 订单回报回调
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param leftover 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销
	 * @param ordTime 订单时间
	 */
	virtual void handle_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled, uint64_t ordTime) = 0;  // 订单回报

	/**
	 * @brief 委托回报回调
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息
	 * @param ordTime 订单时间
	 */
	virtual void handle_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, uint64_t ordTime) = 0;  // 委托回报
};

/**
 * @brief 撤单回调函数类型定义
 * 
 * @param double 撤单数量
 */
typedef std::function<void(double)> FuncCancelCallback;              // 撤单回调函数类型

/**
 * @brief 撮合引擎类
 * 
 * 负责模拟订单撮合过程，包括订单管理、撮合逻辑、限价订单簿维护等
 */
class MatchEngine
{
public:
	/**
	 * @brief 构造函数
	 */
	MatchEngine() : _tick_cache(NULL),_cancelrate(0), _sink(NULL)    // 初始化tick缓存、撤单率、回调接口
	{

	}
private:
	/**
	 * @brief 激活订单
	 * 
	 * 将待激活的订单状态改为已激活，并发送委托回报和订单回报
	 * 
	 * @param stdCode 合约代码
	 * @param to_erase 待删除的订单ID列表（输出参数）
	 */
	void	fire_orders(const char* stdCode, OrderIDs& to_erase);     // 激活订单

	/**
	 * @brief 撮合订单
	 * 
	 * 根据当前tick数据，检查并撮合符合条件的订单
	 * 
	 * @param curTick 当前tick数据
	 * @param to_erase 待删除的订单ID列表（输出参数）
	 */
	void	match_orders(WTSTickData* curTick, OrderIDs& to_erase);  // 撮合订单

	/**
	 * @brief 更新限价订单簿
	 * 
	 * 根据当前tick数据更新限价订单簿（LOB）信息
	 * 
	 * @param curTick 当前tick数据
	 */
	void	update_lob(WTSTickData* curTick);                         // 更新限价订单簿

	/**
	 * @brief 获取最新tick数据
	 * 
	 * @param stdCode 合约代码
	 * @return 最新tick数据指针
	 */
	inline WTSTickData*	grab_last_tick(const char* stdCode);          // 获取最新tick数据

public:
	/**
	 * @brief 初始化撮合引擎
	 * 
	 * @param cfg 配置信息（包含撤单率等）
	 */
	void	init(WTSVariant* cfg);                                     // 初始化函数

	/**
	 * @brief 注册回调接口
	 * 
	 * @param sink 回调接口指针
	 */
	void	regisSink(IMatchSink* sink) { _sink = sink; }            // 注册回调接口

	/**
	 * @brief 清空所有订单
	 */
	void	clear();                                                  // 清空订单

	/**
	 * @brief 处理tick数据
	 * 
	 * 处理tick数据，更新限价订单簿，激活订单，撮合订单
	 * 
	 * @param stdCode 合约代码
	 * @param curTick 当前tick数据
	 */
	void	handle_tick(const char* stdCode, WTSTickData* curTick);  // 处理tick数据

	/**
	 * @brief 买入订单
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param curTime 当前时间
	 * @return 订单ID列表
	 */
	OrderIDs	buy(const char* stdCode, double price, double qty, uint64_t curTime);  // 买入订单

	/**
	 * @brief 卖出订单
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param curTime 当前时间
	 * @return 订单ID列表
	 */
	OrderIDs	sell(const char* stdCode, double price, double qty, uint64_t curTime);  // 卖出订单

	/**
	 * @brief 撤销订单（按订单ID）
	 * 
	 * @param localid 本地订单ID
	 * @return 撤销数量
	 */
	double		cancel(uint32_t localid);                             // 撤销订单（按ID）

	/**
	 * @brief 撤销订单（按合约代码和方向）
	 * 
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param qty 数量（0表示全部）
	 * @param cb 撤单回调函数
	 * @return 订单ID列表
	 */
	virtual OrderIDs cancel(const char* stdCode, bool isBuy, double qty, FuncCancelCallback cb);  // 撤销订单（按合约和方向）

private:
	/**
	 * @brief 订单信息结构体
	 */
	typedef struct _OrderInfo
	{
		char		_code[32];                                          // 合约代码
		bool		_buy;                                               // 是否买入
		double		_qty;                                               // 订单数量
		double		_left;                                              // 剩余数量
		double		_traded;                                            // 已成交数量
		double		_limit;                                              // 限价
		double		_price;                                              // 订单价格
		uint32_t	_state;                                             // 订单状态（0-待激活，1-已激活，9-待撤单，99-已撤单）
		uint64_t	_time;                                               // 订单时间
		double		_queue;                                              // 排队位置
		bool		_positive;                                          // 是否主动订单（对手价）

		/**
		 * @brief 构造函数
		 */
		_OrderInfo()
		{
			memset(this, 0, sizeof(_OrderInfo));                      // 清零初始化
		}
	} OrderInfo;                                                       // 订单信息结构体

	/**
	 * @brief 订单映射表类型定义
	 */
	typedef wt_hashmap<uint32_t, OrderInfo> Orders;                  // 订单映射表（订单ID -> 订单信息）
	Orders	_orders;                                                   // 订单映射表

	/**
	 * @brief 限价订单簿项类型定义
	 */
	typedef std::map<uint32_t, double>	LOBItems;                     // 限价订单簿项（价格（整数）-> 数量）

	/**
	 * @brief 限价订单簿结构体
	 */
	typedef struct _LmtOrdBook
	{
		LOBItems	_items;                                             // 订单簿项
		uint32_t	_cur_px;                                            // 当前价格（整数）
		uint32_t	_ask_px;                                             // 卖一价（整数）
		uint32_t	_bid_px;                                             // 买一价（整数）

		/**
		 * @brief 清空订单簿
		 */
		void clear()
		{
			_items.clear();                                           // 清空订单簿项
			_cur_px = 0;                                              // 重置当前价格
			_ask_px = 0;                                              // 重置卖一价
			_bid_px = 0;                                              // 重置买一价
		}

		/**
		 * @brief 构造函数
		 */
		_LmtOrdBook()
		{
			_cur_px = 0;                                              // 初始化当前价格
			_ask_px = 0;                                              // 初始化卖一价
			_bid_px = 0;                                              // 初始化买一价
		}
	} LmtOrdBook;                                                     // 限价订单簿结构体

	/**
	 * @brief 限价订单簿映射表类型定义
	 */
	typedef wt_hashmap<std::string, LmtOrdBook> LmtOrdBooks;         // 限价订单簿映射表（合约代码 -> 订单簿）
	LmtOrdBooks	_lmt_ord_books;                                        // 限价订单簿映射表

	IMatchSink*	_sink;                                                 // 回调接口指针

	double			_cancelrate;                                        // 撤单率（0-1）
	WTSTickCache*	_tick_cache;                                        // Tick缓存指针
};

