/*!
 * \file MatchEngine.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 撮合引擎实现文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是MatchEngine类的实现文件，提供了订单撮合引擎的所有功能实现。
 *
 * 主要功能模块：
 * 1. 初始化功能：读取配置信息（撤单率等）
 * 2. 订单激活：将待激活订单状态改为已激活，发送委托回报
 * 3. 订单撮合：根据tick数据模拟订单成交过程
 * 4. 限价订单簿更新：维护价格档位的订单队列信息
 * 5. 订单管理：买入/卖出订单的创建和管理
 * 6. 撤单处理：支持按订单ID或合约代码撤单
 *
 * 核心算法：
 * - 价格转换：将浮点价格转换为整数（乘以10000）
 * - 订单排队：模拟订单在价格档位上的排队位置
 * - 撮合逻辑：根据订单类型（主动/被动）和价格条件进行撮合
 * - 限价订单簿：维护买卖盘口的价格档位信息
 */
#include "MatchEngine.h"                                              // 撮合引擎头文件
#include "../Includes/WTSDataDef.hpp"                                // WonderTrader数据定义
#include "../Includes/WTSVariant.hpp"                                 // 变体类型定义

#include "../Share/TimeUtils.hpp"                                    // 时间工具函数
#include "../Share/decimal.h"                                        // 小数精度计算工具
#include "../WTSTools/WTSLogger.h"                                   // 日志工具

/**
 * @brief 价格转整数宏（正数，向上取整）
 * 
 * 将浮点价格乘以10000并向上取整转换为整数
 */
#define PRICE_DOUBLE_TO_INT_P(x) ((int32_t)((x)*10000.0 + 0.5))      // 正数价格转整数（向上取整）

/**
 * @brief 价格转整数宏（负数，向下取整）
 * 
 * 将浮点价格乘以10000并向下取整转换为整数
 */
#define PRICE_DOUBLE_TO_INT_N(x) ((int32_t)((x)*10000.0 - 0.5))      // 负数价格转整数（向下取整）

/**
 * @brief 价格转整数宏（通用）
 * 
 * 根据价格正负选择对应的转换方式，特殊处理DBL_MAX
 */
#define PRICE_DOUBLE_TO_INT(x) (((x)==DBL_MAX)?0:((x)>0?PRICE_DOUBLE_TO_INT_P(x):PRICE_DOUBLE_TO_INT_N(x)))  // 通用价格转整数

/**
 * @brief 外部函数声明：生成本地订单ID
 */
extern uint32_t makeLocalOrderID();                                  // 生成本地订单ID函数声明

/**
 * @brief 初始化撮合引擎
 * 
 * 从配置中读取撤单率参数
 * 
 * @param cfg 配置信息（包含cancelrate等）
 */
void MatchEngine::init(WTSVariant* cfg)
{
	if (cfg == NULL)                                                 // 如果配置为空
		return;                                                       // 直接返回

	_cancelrate = cfg->getDouble("cancelrate");                      // 从配置中读取撤单率
}

/**
 * @brief 清空所有订单
 */
void MatchEngine::clear()
{
	_orders.clear();                                                 // 清空订单映射表
}

/**
 * @brief 激活订单
 * 
 * 遍历所有订单，将状态为0（待激活）的订单改为已激活状态，
 * 并发送委托回报和订单回报
 * 
 * @param stdCode 合约代码
 * @param to_erase 待删除的订单ID列表（输出参数，此处未使用）
 */
void MatchEngine::fire_orders(const char* stdCode, OrderIDs& to_erase)
{
	for (auto& v : _orders)                                          // 遍历所有订单
	{
		uint32_t localid = v.first;                                  // 获取订单ID
		OrderInfo& ordInfo = (OrderInfo&)v.second;                   // 获取订单信息

		if (ordInfo._state == 0)	//需要激活                      // 如果订单状态为0（待激活）
		{
			_sink->handle_entrust(localid, stdCode, true, "", ordInfo._time);  // 发送委托回报（成功）
			_sink->handle_order(localid, stdCode, ordInfo._buy, ordInfo._left, ordInfo._limit, false, ordInfo._time);  // 发送订单回报
			ordInfo._state = 1;                                       // 设置订单状态为1（已激活）
		}
	}
}

/**
 * @brief 撮合订单
 * 
 * 根据当前tick数据，检查并撮合符合条件的订单
 * 
 * 处理流程：
 * 1. 检查订单状态，处理待撤单订单
 * 2. 对于已激活的订单，根据买入/卖出方向分别处理
 * 3. 对于买单：检查价格是否小于等于限价
 * 4. 对于卖单：检查价格是否大于等于限价
 * 5. 考虑订单排队位置，模拟订单成交过程
 * 6. 发送成交回报和订单回报
 * 
 * @param curTick 当前tick数据
 * @param to_erase 待删除的订单ID列表（输出参数）
 */
void MatchEngine::match_orders(WTSTickData* curTick, OrderIDs& to_erase)
{
	uint64_t curTime = (uint64_t)curTick->actiondate() * 1000000000 + curTick->actiontime();  // 计算当前时间戳（纳秒）
	uint64_t curUnixTime = TimeUtils::makeTime(curTick->actiondate(), curTick->actiontime());  // 计算Unix时间戳（未使用）

	for (auto& v : _orders)                                          // 遍历所有订单
	{
		uint32_t localid = v.first;                                  // 获取订单ID
		OrderInfo& ordInfo = (OrderInfo&)v.second;                   // 获取订单信息

		if (ordInfo._state == 9)//要撤单                           // 如果订单状态为9（待撤单）
		{
			_sink->handle_order(localid, ordInfo._code, ordInfo._buy, 0, ordInfo._limit, true, ordInfo._time);  // 发送订单回报（已撤销）
			ordInfo._state = 99;                                     // 设置订单状态为99（已撤单）

			to_erase.emplace_back(localid);                         // 添加到待删除列表

			WTSLogger::info("订单{}已撤销, 剩余数量: {}", localid, ordInfo._left*(ordInfo._buy ? 1 : -1));  // 记录日志
			ordInfo._left = 0;                                       // 清空剩余数量
			continue;                                                 // 跳过后续处理
		}

		if (ordInfo._state != 1 || curTick->volume() == 0)          // 如果订单未激活或当前tick无成交量
			continue;                                                 // 跳过本次循环

		if (ordInfo._buy)                                             // 如果是买单
		{
			double price;                                              // 成交价格
			double volume;                                             // 可成交量

			//主动订单就按照对手价
			if (ordInfo._positive)                                     // 如果是主动订单（对手价）
			{
				price = curTick->askprice(0);                         // 使用卖一价
				volume = curTick->askqty(0);                          // 使用卖一量
			}
			else                                                       // 如果是被动订单（挂单）
			{
				price = curTick->price();                             // 使用最新价
				volume = curTick->volume();                           // 使用成交量
			}

			if (decimal::le(price, ordInfo._limit))                   // 如果成交价格小于等于限价（可以成交）
			{
				//如果价格相等,需要先看排队位置,如果价格不等说明已经全部被大单吃掉了
				if (!ordInfo._positive && decimal::eq(price, ordInfo._limit))  // 如果是被动订单且价格等于限价
				{
					double& quepos = ordInfo._queue;                  // 获取排队位置

					//如果成交量小于排队位置,则不能成交
					if (volume <= quepos)                             // 如果成交量小于等于排队位置
					{
						quepos -= volume;                             // 减少排队位置
						continue;                                      // 跳过本次循环（不成交）
					}
					else if (quepos != 0)                             // 如果成交量大于排队位置且排队位置不为0
					{
						//如果成交量大于排队位置,则可以成交
						volume -= quepos;                             // 扣除排队位置后的可成交量
						quepos = 0;                                   // 清空排队位置
					}
				}
				else if (!ordInfo._positive)                          // 如果是被动订单且价格不等于限价
				{
					volume = ordInfo._left;                            // 使用剩余数量作为可成交量
				}

				double qty = min(volume, ordInfo._left);             // 计算成交数量（取可成交量和剩余数量的较小值）
				if (decimal::eq(qty, 0.0))                            // 如果成交数量为0
					qty = 1;                                          // 设置为1（最小成交单位）

				_sink->handle_trade(localid, ordInfo._code, ordInfo._buy, qty, ordInfo._price, price, ordInfo._time);  // 发送成交回报

				ordInfo._traded += qty;                               // 累加已成交数量
				ordInfo._left -= qty;                                 // 减少剩余数量

				_sink->handle_order(localid, ordInfo._code, ordInfo._buy, ordInfo._left, price, false, ordInfo._time);  // 发送订单回报（更新剩余数量）

				if (ordInfo._left == 0)                               // 如果剩余数量为0（完全成交）
					to_erase.emplace_back(localid);                  // 添加到待删除列表
			}
		}

		if (!ordInfo._buy)                                            // 如果是卖单
		{
			double price;                                              // 成交价格
			double volume;                                             // 可成交量

			//主动订单就按照对手价
			if (ordInfo._positive)                                     // 如果是主动订单（对手价）
			{
				price = curTick->bidprice(0);                        // 使用买一价
				volume = curTick->bidqty(0);                          // 使用买一量
			}
			else                                                       // 如果是被动订单（挂单）
			{
				price = curTick->price();                             // 使用最新价
				volume = curTick->volume();                           // 使用成交量
			}

			if (decimal::ge(price, ordInfo._limit))                    // 如果成交价格大于等于限价（可以成交）
			{
				//如果价格相等,需要先看排队位置,如果价格不等说明已经全部被大单吃掉了
				if (!ordInfo._positive && decimal::eq(price, ordInfo._limit))  // 如果是被动订单且价格等于限价
				{
					double& quepos = ordInfo._queue;                  // 获取排队位置

					//如果成交量小于排队位置,则不能成交
					if (volume <= quepos)                             // 如果成交量小于等于排队位置
					{
						quepos -= volume;                             // 减少排队位置
						continue;                                      // 跳过本次循环（不成交）
					}
					else if (quepos != 0)                             // 如果成交量大于排队位置且排队位置不为0
					{
						//如果成交量大于排队位置,则可以成交
						volume -= quepos;                             // 扣除排队位置后的可成交量
						quepos = 0;                                   // 清空排队位置
					}
				}
				else if (!ordInfo._positive)                          // 如果是被动订单且价格不等于限价
				{
					volume = ordInfo._left;                            // 使用剩余数量作为可成交量
				}

				double qty = min(volume, ordInfo._left);             // 计算成交数量（取可成交量和剩余数量的较小值）
				if (decimal::eq(qty, 0.0))                            // 如果成交数量为0
					qty = 1;                                          // 设置为1（最小成交单位）

				_sink->handle_trade(localid, ordInfo._code, ordInfo._buy, qty, ordInfo._price, price, ordInfo._time);  // 发送成交回报
				ordInfo._traded += qty;                               // 累加已成交数量
				ordInfo._left -= qty;                                 // 减少剩余数量

				_sink->handle_order(localid, ordInfo._code, ordInfo._buy, ordInfo._left, price, false, ordInfo._time);  // 发送订单回报（更新剩余数量）

				if (ordInfo._left == 0)                               // 如果剩余数量为0（完全成交）
					to_erase.emplace_back(localid);                  // 添加到待删除列表
			}

		}
	}
}

/**
 * @brief 更新限价订单簿
 * 
 * 根据当前tick数据更新限价订单簿（LOB）信息
 * 
 * 处理流程：
 * 1. 更新当前价格、卖一价、买一价
 * 2. 遍历买卖盘口前10档，更新各档位的价格和数量
 * 3. 清除卖一和买一之间的无效报价
 * 
 * @param curTick 当前tick数据
 */
void MatchEngine::update_lob(WTSTickData* curTick)
{
	LmtOrdBook& curBook = _lmt_ord_books[curTick->code()];          // 获取或创建该合约的限价订单簿
	curBook._cur_px = PRICE_DOUBLE_TO_INT(curTick->price());        // 更新当前价格（转整数）
	curBook._ask_px = PRICE_DOUBLE_TO_INT(curTick->askprice(0));    // 更新卖一价（转整数）
	curBook._bid_px = PRICE_DOUBLE_TO_INT(curTick->bidprice(0));    // 更新买一价（转整数）

	for (uint32_t i = 0; i < 10; i++)                                // 遍历买卖盘口前10档
	{
		if (PRICE_DOUBLE_TO_INT(curTick->askprice(i)) == 0 && PRICE_DOUBLE_TO_INT(curTick->bidprice(i)) == 0)  // 如果卖价和买价都为0
			break;                                                     // 退出循环

		uint32_t px = PRICE_DOUBLE_TO_INT(curTick->askprice(i));     // 获取卖价（转整数）
		if (px != 0)                                                  // 如果卖价不为0
		{
			double& volume = curBook._items[px];                     // 获取或创建该价格的订单簿项
			volume = curTick->askqty(i);                              // 更新卖量
		}

		px = PRICE_DOUBLE_TO_INT(curTick->bidprice(i));             // 获取买价（转整数）
		if (px != 0)                                                  // 如果买价不为0
		{
			double& volume = curBook._items[px];                     // 获取或创建该价格的订单簿项
			volume = curTick->askqty(i);                              // 更新买量（注意：这里代码可能有bug，应该是bidqty）
		}
	}

	//卖一和买一之间的报价必须全部清除掉
	if (!curBook._items.empty())                                      // 如果订单簿不为空
	{
		auto sit = curBook._items.lower_bound(curBook._bid_px);      // 找到买一价的迭代器位置
		if (sit->first == curBook._bid_px)                           // 如果正好是买一价
			sit++;                                                    // 向后移动一位（不包括买一价）

		auto eit = curBook._items.lower_bound(curBook._ask_px);      // 找到卖一价的迭代器位置

		if (sit->first <= eit->first)                                // 如果起始位置小于等于结束位置
			curBook._items.erase(sit, eit);                          // 删除卖一和买一之间的所有报价
	}
}


/**
 * @brief 买入订单
 * 
 * 创建买入订单并设置订单排队位置
 * 
 * 处理流程：
 * 1. 获取最新tick数据
 * 2. 生成本地订单ID
 * 3. 初始化订单信息（合约代码、方向、价格、数量等）
 * 4. 根据订单价格设置排队位置：
 *    - 如果价格>=卖一价，标记为主动订单（对手价）
 *    - 如果价格=买一价，排队位置=买一量
 *    - 如果价格=最新价，排队位置=买一卖一加权平均
 * 5. 根据撤单率调整排队位置
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param curTime 当前时间
 * @return 订单ID列表
 */
OrderIDs MatchEngine::buy(const char* stdCode, double price, double qty, uint64_t curTime)
{
	WTSTickData* lastTick = grab_last_tick(stdCode);                  // 获取最新tick数据
	if (lastTick == NULL)                                             // 如果tick数据不存在
		return OrderIDs();                                             // 返回空列表

	uint32_t localid = makeLocalOrderID();                           // 生成本地订单ID
	OrderInfo& ordInfo = _orders[localid];                            // 创建订单信息
	strcpy(ordInfo._code, stdCode);                                   // 设置合约代码
	ordInfo._buy = true;                                              // 设置为买入
	ordInfo._limit = price;                                           // 设置限价
	ordInfo._qty = qty;                                               // 设置订单数量
	ordInfo._left = qty;                                              // 设置剩余数量
	ordInfo._price = lastTick->price();                               // 设置订单价格（使用最新价）

	//订单排队,如果是对手价,则按照对手价的挂单量来排队
	//如果是最新价,则按照买一卖一的加权平均
	if (decimal::ge(price, lastTick->askprice(0)))                  // 如果价格大于等于卖一价（主动订单）
		ordInfo._positive = true;                                     // 标记为主动订单
	else if (decimal::eq(price, lastTick->bidprice(0)))              // 如果价格等于买一价
		ordInfo._queue = lastTick->bidqty(0);                        // 排队位置=买一量
	if (decimal::eq(price, lastTick->price()))                       // 如果价格等于最新价
		ordInfo._queue = (uint32_t)round((lastTick->askqty(0)*lastTick->askprice(0) + lastTick->bidqty(0)*lastTick->bidprice(0)) / (lastTick->askprice(0) + lastTick->bidprice(0)));  // 排队位置=买一卖一加权平均

	//排队位置按照平均撤单率,撤销掉部分
	ordInfo._queue -= (uint32_t)round(ordInfo._queue*_cancelrate);    // 根据撤单率调整排队位置
	ordInfo._time = curTime;                                          // 设置订单时间

	lastTick->release();                                               // 释放tick数据

	OrderIDs ret;                                                     // 订单ID列表
	ret.emplace_back(localid);                                         // 添加订单ID
	return ret;                                                        // 返回订单ID列表
}

/**
 * @brief 卖出订单
 * 
 * 创建卖出订单并设置订单排队位置
 * 
 * 处理流程：
 * 1. 获取最新tick数据
 * 2. 生成本地订单ID
 * 3. 初始化订单信息（合约代码、方向、价格、数量等）
 * 4. 根据订单价格设置排队位置：
 *    - 如果价格=卖一价，排队位置=卖一量
 *    - 如果价格<=买一价，标记为主动订单（对手价）
 *    - 如果价格=最新价，排队位置=买一卖一加权平均
 * 5. 根据撤单率调整排队位置
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param curTime 当前时间
 * @return 订单ID列表
 */
OrderIDs MatchEngine::sell(const char* stdCode, double price, double qty, uint64_t curTime)
{
	WTSTickData* lastTick = grab_last_tick(stdCode);                  // 获取最新tick数据
	if (lastTick == NULL)                                             // 如果tick数据不存在
		return OrderIDs();                                             // 返回空列表

	uint32_t localid = makeLocalOrderID();                           // 生成本地订单ID
	OrderInfo& ordInfo = _orders[localid];                            // 创建订单信息
	strcpy(ordInfo._code, stdCode);                                   // 设置合约代码
	ordInfo._buy = false;                                             // 设置为卖出
	ordInfo._limit = price;                                           // 设置限价
	ordInfo._qty = qty;                                               // 设置订单数量
	ordInfo._left = qty;                                              // 设置剩余数量
	ordInfo._price = lastTick->price();                               // 设置订单价格（使用最新价）

	//订单排队,如果是对手价,则按照对手价的挂单量来排队
	//如果是最新价,则按照买一卖一的加权平均
	if (decimal::eq(price, lastTick->askprice(0)))                  // 如果价格等于卖一价
		ordInfo._queue = lastTick->askqty(0);                        // 排队位置=卖一量
	else if (decimal::le(price, lastTick->bidprice(0)))              // 如果价格小于等于买一价（主动订单）
		ordInfo._positive = true;                                     // 标记为主动订单
	if (decimal::eq(price, lastTick->price()))                       // 如果价格等于最新价
		ordInfo._queue = (uint32_t)round((lastTick->askqty(0)*lastTick->askprice(0) + lastTick->bidqty(0)*lastTick->bidprice(0)) / (lastTick->askprice(0) + lastTick->bidprice(0)));  // 排队位置=买一卖一加权平均

	ordInfo._queue -= (uint32_t)round(ordInfo._queue*_cancelrate);    // 根据撤单率调整排队位置
	ordInfo._time = curTime;                                          // 设置订单时间

	lastTick->release();                                               // 释放tick数据

	OrderIDs ret;                                                     // 订单ID列表
	ret.emplace_back(localid);                                        // 添加订单ID
	return ret;                                                        // 返回订单ID列表
}

/**
 * @brief 撤销订单（按合约代码和方向）
 * 
 * 撤销指定合约代码和方向的订单
 * 
 * @param stdCode 合约代码（未使用）
 * @param isBuy 是否买入
 * @param qty 数量（0表示全部）
 * @param cb 撤单回调函数
 * @return 订单ID列表
 */
OrderIDs MatchEngine::cancel(const char* stdCode, bool isBuy, double qty, FuncCancelCallback cb)
{
	OrderIDs ret;                                                     // 订单ID列表
	for (auto& v : _orders)                                           // 遍历所有订单
	{
		OrderInfo& ordInfo = (OrderInfo&)v.second;                   // 获取订单信息
		if (ordInfo._state != 1)                                      // 如果订单未激活
			continue;                                                  // 跳过本次循环

		double left = qty;                                            // 剩余需要撤销的数量
		if (ordInfo._buy == isBuy)                                    // 如果订单方向匹配
		{
			uint32_t localid = v.first;                               // 获取订单ID
			ret.emplace_back(localid);                                // 添加到结果列表
			ordInfo._state = 9;                                       // 设置订单状态为9（待撤单）
			cb(ordInfo._left*(ordInfo._buy ? 1 : -1));               // 调用撤单回调函数

			if (qty != 0)                                             // 如果指定了数量
			{
				if ((int32_t)left <= ordInfo._left)                   // 如果剩余数量小于等于订单剩余数量
					break;                                            // 退出循环

				left -= ordInfo._left;                                // 减少剩余数量
			}
		}
	}

	return ret;                                                       // 返回订单ID列表
}

/**
 * @brief 撤销订单（按订单ID）
 * 
 * @param localid 本地订单ID
 * @return 撤销数量（正数表示买入，负数表示卖出）
 */
double MatchEngine::cancel(uint32_t localid)
{
	auto it = _orders.find(localid);                                  // 查找订单
	if (it == _orders.end())                                          // 如果订单不存在
		return 0.0;                                                   // 返回0

	OrderInfo& ordInfo = (OrderInfo&)it->second;                     // 获取订单信息
	ordInfo._state = 9;                                               // 设置订单状态为9（待撤单）

	return ordInfo._left*(ordInfo._buy ? 1 : -1);                    // 返回撤销数量（正数=买入，负数=卖出）
}

/**
 * @brief 处理tick数据
 * 
 * 处理tick数据，更新限价订单簿，激活订单，撮合订单
 * 
 * 处理流程：
 * 1. 检查tick数据是否有效
 * 2. 如果tick缓存不存在，创建缓存
 * 3. 将tick数据添加到缓存
 * 4. 更新限价订单簿
 * 5. 激活待激活的订单
 * 6. 撮合订单
 * 7. 删除已完全成交或已撤单的订单
 * 
 * @param stdCode 合约代码
 * @param curTick 当前tick数据
 */
void MatchEngine::handle_tick(const char* stdCode, WTSTickData* curTick)
{
	if (NULL == curTick)                                              // 如果tick数据为空
		return;                                                        // 直接返回

	if (NULL == _tick_cache)                                         // 如果tick缓存不存在
		_tick_cache = WTSTickCache::create();                        // 创建tick缓存

	_tick_cache->add(stdCode, curTick, true);                        // 将tick数据添加到缓存

	update_lob(curTick);                                              // 更新限价订单簿

	OrderIDs to_erase;                                                // 待删除的订单ID列表
	//检查订单状态
	fire_orders(stdCode, to_erase);                                  // 激活订单

	//撮合
	match_orders(curTick, to_erase);                                 // 撮合订单

	for (uint32_t localid : to_erase)                                // 遍历待删除的订单ID列表
	{
		auto it = _orders.find(localid);                              // 查找订单
		if (it != _orders.end())                                      // 如果订单存在
			_orders.erase(it);                                        // 删除订单
	}
}

/**
 * @brief 获取最新tick数据
 * 
 * 从tick缓存中获取指定合约的最新tick数据
 * 
 * @param stdCode 合约代码
 * @return 最新tick数据指针（需要调用者释放）
 */
WTSTickData* MatchEngine::grab_last_tick(const char* stdCode)
{
	if (NULL == _tick_cache)                                         // 如果tick缓存不存在
		return NULL;                                                   // 返回NULL

	return (WTSTickData*)_tick_cache->grab(stdCode);                // 从缓存中获取tick数据
}