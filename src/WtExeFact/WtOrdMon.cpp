/*!
 * \file WtOrdMon.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtOrdMon订单管理器类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了WtOrdMon类的所有成员函数，该类用于跟踪和管理执行单元中的订单状态。
 * 
 * 实现要点：
 * 1. 所有操作都使用互斥锁保护，确保线程安全
 * 2. 使用StdLocker自动管理锁的生命周期，异常安全
 * 3. 订单信息存储在unordered_map中，提供O(1)查询性能
 * 4. 超时检查逻辑：计算订单创建时间与当前时间的差值，判断是否超时
 * 
 * 核心算法：
 * - 订单注册：将订单ID作为键，订单信息对（创建时间、可撤单标志）作为值存储
 * - 订单查询：使用find()方法查找订单，O(1)时间复杂度
 * - 超时检查：遍历所有订单，计算时间差，找出超时且可撤单的订单
 * - 订单枚举：遍历所有订单，调用回调函数传递订单信息
 */
#include "WtOrdMon.h"  // WtOrdMon类定义

/**
 * @brief 添加订单实现
 * 
 * 将一批订单添加到订单管理器中，记录订单的创建时间和可撤单标志。
 * 该函数是线程安全的，使用互斥锁保护订单数据。
 * 
 * @param ids 订单ID数组指针
 * @param cnt 订单数量
 * @param curTime 当前时间（毫秒时间戳）
 * @param bCanCancel 是否可撤单，true表示可撤单，false表示不可撤单（如涨跌停价的挂单）
 */
void WtOrdMon::push_order(const uint32_t* ids, uint32_t cnt, uint64_t curTime, bool bCanCancel /* = true */)
{
	StdLocker<StdRecurMutex> lock(_mtx_ords);  // 获取互斥锁，自动管理锁的生命周期（RAII模式）
	for (uint32_t idx = 0; idx < cnt; idx++)  // 遍历所有订单ID
	{
		uint32_t localid = ids[idx];  // 获取当前订单ID
		OrderPair& ordInfo = _orders[localid];  // 获取或创建订单信息对（如果不存在则创建）
		ordInfo.first = curTime;  // 设置订单创建时间
		ordInfo.second = bCanCancel;  // 设置订单可撤单标志
	}
	// 锁在函数结束时自动释放（StdLocker析构函数）
}

/**
 * @brief 删除订单实现
 * 
 * 从订单管理器中删除指定的订单。
 * 该函数是线程安全的，使用互斥锁保护订单数据。
 * 
 * @param localid 订单ID
 */
void WtOrdMon::erase_order(uint32_t localid)
{
	StdLocker<StdRecurMutex> lock(_mtx_ords);  // 获取互斥锁，自动管理锁的生命周期
	auto it = _orders.find(localid);  // 查找指定订单ID
	if (it == _orders.end())  // 如果未找到订单
		return;  // 直接返回，不做任何操作

	_orders.erase(it);  // 删除找到的订单
	// 锁在函数结束时自动释放
}

/**
 * @brief 检查订单超时实现
 * 
 * 检查订单管理器中是否有订单超过指定时间未成交，如果有则调用回调函数。
 * 该函数会遍历所有订单，找出超过expiresecs秒未成交且可撤单的订单，并调用回调函数。
 * 
 * @param expiresecs 订单超时秒数（订单创建后超过此时间未成交则视为超时）
 * @param curTime 当前时间（毫秒时间戳）
 * @param callback 回调函数，当发现超时订单时调用，参数为订单ID
 */
void WtOrdMon::check_orders(uint32_t expiresecs, uint64_t curTime, EnumOrderCallback callback)
{
	if (_orders.empty())  // 如果订单映射表为空
		return;  // 直接返回，无需检查

	StdLocker<StdRecurMutex> lock(_mtx_ords);  // 获取互斥锁，自动管理锁的生命周期
	for (auto& m : _orders)  // 遍历所有订单
	{
		uint32_t localid = m.first;  // 获取订单ID
		OrderPair& ordInfo = m.second;  // 获取订单信息对
		if (!ordInfo.second)  // 如果不能撤单，则直接跳过（一般涨跌停价的挂单是不能撤单的）
			continue;  // 跳过该订单，继续检查下一个

		auto entertm = ordInfo.first;  // 获取订单创建时间
		if (curTime - entertm < expiresecs * 1000)  // 如果订单未超时（时间差小于超时秒数*1000毫秒）
			continue;  // 跳过该订单，继续检查下一个

		callback(m.first);  // 订单已超时，调用回调函数通知调用者
	}
	// 锁在函数结束时自动释放
}

/**
 * @brief 枚举所有订单实现
 * 
 * 遍历订单管理器中的所有订单，对每个订单调用回调函数。
 * 该函数是线程安全的，使用互斥锁保护订单数据。
 * 
 * @param cb 回调函数，对每个订单调用，参数为订单ID、创建时间和可撤单标志
 */
void WtOrdMon::enumOrder(EnumAllOrderCallback cb)
{
	if (_orders.empty())  // 如果订单映射表为空
		return;  // 直接返回，无需枚举

	StdLocker<StdRecurMutex> lock(_mtx_ords);  // 获取互斥锁，自动管理锁的生命周期
	for (auto& m : _orders)  // 遍历所有订单
	{
		uint32_t localid = m.first;  // 获取订单ID
		OrderPair& ordInfo = m.second;  // 获取订单信息对
		uint64_t entertm = ordInfo.first;  // 获取订单创建时间
		bool cancancel = ordInfo.second;  // 获取订单可撤单标志
		cb(localid, entertm, cancancel);  // 调用回调函数，传递订单的完整信息
	}
	// 锁在函数结束时自动释放
}

