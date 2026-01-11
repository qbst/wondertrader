/*!
 * \file WtOrdMon.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtOrdMon订单管理器类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了WtOrdMon类，这是一个订单管理器，用于跟踪和管理执行单元中的订单状态。
 * 订单管理器的主要功能包括：
 * 1. 订单注册：记录订单的创建时间和是否可撤单标志
 * 2. 订单查询：检查是否存在指定订单或任意订单
 * 3. 订单超时检查：检查订单是否超过指定时间未成交，用于自动撤单
 * 4. 订单枚举：遍历所有订单，执行回调函数
 * 5. 订单清理：删除指定订单或清空所有订单
 * 
 * 设计目标：
 * - 为执行单元提供订单生命周期管理功能
 * - 支持订单超时自动撤单机制
 * - 线程安全：使用互斥锁保护订单数据
 * - 高效查询：使用哈希表存储订单，O(1)时间复杂度查询
 * 
 * 核心功能：
 * - 订单注册：记录订单ID、创建时间和可撤单标志
 * - 订单查询：快速检查订单是否存在
 * - 超时检查：找出超过指定时间未成交的订单
 * - 订单枚举：遍历所有订单执行回调
 * - 订单删除：删除指定订单或清空所有订单
 * 
 * 架构特点：
 * - 使用unordered_map存储订单信息，提供O(1)查询性能
 * - 使用递归互斥锁（StdRecurMutex）保证线程安全
 * - 订单信息包含创建时间和可撤单标志，支持超时撤单逻辑
 */
#pragma once
#include <unordered_map>  // 无序映射表，用于存储订单信息
#include <stdint.h>  // 标准整数类型定义
#include <functional>  // 函数对象和回调函数支持

#include "../Share/StdUtils.hpp"  // WonderTrader标准工具类（互斥锁等）

/**
 * @brief 订单枚举回调函数类型
 * 
 * 用于订单超时检查时的回调函数，参数为订单ID
 * 
 * @param localid 订单ID
 */
typedef std::function<void(uint32_t)> EnumOrderCallback;

/**
 * @brief 订单枚举回调函数类型（完整信息）
 * 
 * 用于枚举所有订单时的回调函数，包含订单的完整信息
 * 
 * @param localid 订单ID
 * @param entertime 订单创建时间（毫秒时间戳）
 * @param cancancel 是否可撤单（true表示可撤单，false表示不可撤单）
 */
typedef std::function<void(uint32_t, uint64_t, bool)> EnumAllOrderCallback;

/**
 * @brief WtOrdMon订单管理器类
 * 
 * 订单管理器用于跟踪和管理执行单元中的订单状态，支持订单注册、查询、超时检查等功能。
 * 该类是执行单元的核心组件，用于实现订单的生命周期管理和自动撤单机制。
 */
class WtOrdMon
{
public:
	/**
	 * @brief 添加订单
	 * 
	 * 将一批订单添加到订单管理器中，记录订单的创建时间和可撤单标志。
	 * 该函数是线程安全的，使用互斥锁保护订单数据。
	 * 
	 * @param ids 订单ID数组指针
	 * @param cnt 订单数量
	 * @param curTime 当前时间（毫秒时间戳）
	 * @param bCanCancel 是否可撤单，true表示可撤单（默认true），false表示不可撤单（如涨跌停价的挂单）
	 */
	void push_order(const uint32_t* ids, uint32_t cnt, uint64_t curTime, bool bCanCancel = true);

	/**
	 * @brief 删除订单
	 * 
	 * 从订单管理器中删除指定的订单。
	 * 该函数是线程安全的，使用互斥锁保护订单数据。
	 * 
	 * @param localid 订单ID
	 */
	void erase_order(uint32_t localid);

	/**
	 * @brief 检查是否有订单
	 * 
	 * 检查订单管理器中是否存在订单。
	 * 如果localid为0，则检查是否存在任意订单；
	 * 如果localid不为0，则检查是否存在指定的订单。
	 * 
	 * @param localid 订单ID，为0时检查是否有任意订单，不为0时检查是否有指定订单（默认0）
	 * @return true表示存在订单，false表示不存在订单
	 */
	inline bool has_order(uint32_t localid = 0)
	{
		if (localid == 0)  // 如果订单ID为0，检查是否存在任意订单
			return !_orders.empty();  // 返回订单映射表是否为空

		auto it = _orders.find(localid);  // 查找指定订单ID
		if (it == _orders.end())  // 如果未找到
			return false;  // 返回false

		return true;  // 找到订单，返回true
	}

	/**
	 * @brief 检查订单超时
	 * 
	 * 检查订单管理器中是否有订单超过指定时间未成交，如果有则调用回调函数。
	 * 该函数会遍历所有订单，找出超过expiresecs秒未成交且可撤单的订单，并调用回调函数。
	 * 
	 * @param expiresecs 订单超时秒数（订单创建后超过此时间未成交则视为超时）
	 * @param curTime 当前时间（毫秒时间戳）
	 * @param callback 回调函数，当发现超时订单时调用，参数为订单ID
	 */
	void check_orders(uint32_t expiresecs, uint64_t curTime, EnumOrderCallback callback);

	/**
	 * @brief 清空所有订单
	 * 
	 * 清空订单管理器中的所有订单。
	 * 该函数是线程安全的，使用互斥锁保护订单数据。
	 */
	inline void clear_orders()
	{
		_orders.clear();  // 清空订单映射表
	}

	/**
	 * @brief 枚举所有订单
	 * 
	 * 遍历订单管理器中的所有订单，对每个订单调用回调函数。
	 * 该函数是线程安全的，使用互斥锁保护订单数据。
	 * 
	 * @param cb 回调函数，对每个订单调用，参数为订单ID、创建时间和可撤单标志
	 */
	void enumOrder(EnumAllOrderCallback cb);

private:
	/**
	 * @brief 订单信息对类型
	 * 
	 * 存储订单的创建时间和可撤单标志
	 * - uint64_t：订单创建时间（毫秒时间戳）
	 * - bool：是否可撤单（true表示可撤单，false表示不可撤单）
	 */
	typedef std::pair<uint64_t, bool> OrderPair;
	
	/**
	 * @brief 订单映射表类型
	 * 
	 * 使用无序映射表存储订单信息，键为订单ID，值为订单信息对
	 */
	typedef std::unordered_map<uint32_t, OrderPair> IDMap;
	
	IDMap			_orders;  // 订单映射表，存储所有订单的信息（订单ID -> 订单信息对）
	StdRecurMutex	_mtx_ords;  // 订单数据的互斥锁，保证线程安全（递归互斥锁，支持同一线程多次加锁）
};

