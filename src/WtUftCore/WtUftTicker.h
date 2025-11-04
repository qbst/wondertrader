/*!
 * \file WtUftTicker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT实时ticker头文件
 *
 * 本文件定义了WtUftRtTicker类，用于处理实时行情数据并触发分钟线闭合事件。
 *
 * 设计逻辑：
 * 1. 实时行情处理：接收实时tick数据，根据交易时段判断是否触发分钟线闭合
 * 2. 时间管理：维护当前日期、时间、分钟位置等信息，用于判断分钟线是否需要闭合
 * 3. 分钟线闭合：当分钟位置发生变化时，触发分钟线闭合事件
 * 4. 交易日管理：自动判断交易日开始和结束，触发交易日生命周期事件
 * 5. 后台线程：使用独立线程定时检查分钟线闭合，避免阻塞主线程
 *
 * 主要功能：
 * - 接收实时tick数据并处理
 * - 判断分钟线是否需要闭合
 * - 触发分钟线闭合事件
 * - 自动判断交易日开始和结束
 * - 后台定时检查分钟线闭合
 */
#pragma once

#include <stdint.h>  // 标准整数类型定义
#include <atomic>  // 原子操作头文件

#include "../Includes/WTSMarcos.h"  // WTS宏定义头文件
#include "../Share/StdUtils.hpp"  // 标准工具头文件

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSSessionInfo;  // 前置声明：交易时段信息类
class WTSTickData;  // 前置声明：Tick数据类

class WtUftEngine;  // 前置声明：UFT引擎类

/**
 * @class WtUftRtTicker
 * @brief UFT实时ticker类
 * 
 * 处理实时行情数据，判断分钟线闭合并触发相应事件。
 * 使用后台线程定时检查分钟线闭合，确保分钟线能够及时闭合。
 */
class WtUftRtTicker
{
public:
	/**
	 * @brief 构造函数
	 * @param engine UFT引擎指针
	 * 
	 * 创建实时ticker对象，保存引擎指针。
	 */
	WtUftRtTicker(WtUftEngine* engine);
	
	/**
	 * @brief 析构函数
	 * 
	 * 销毁实时ticker对象，停止后台线程。
	 */
	~WtUftRtTicker();

public:
	/**
	 * @brief 初始化ticker
	 * @param sessionID 交易时段ID
	 * 
	 * 初始化ticker，设置交易时段信息，获取当前日期和时间。
	 */
	void	init(const char* sessionID);
	
	/**
	 * @brief 处理Tick数据
	 * @param curTick 当前Tick数据
	 * 
	 * 接收并处理实时tick数据，判断是否需要触发分钟线闭合。
	 */
	void	on_tick(WTSTickData* curTick);

	/**
	 * @brief 启动ticker
	 * 
	 * 启动后台线程，开始定时检查分钟线闭合。
	 */
	void	run();
	
	/**
	 * @brief 停止ticker
	 * 
	 * 停止后台线程，等待线程结束。
	 */
	void	stop();

private:
	WTSSessionInfo*	_s_info;  // 交易时段信息指针
	WtUftEngine*	_engine;  // UFT引擎指针

	uint32_t	_date;  // 当前日期（YYYYMMDD格式）
	uint32_t	_time;  // 当前时间（HHMMSS格式）

	uint32_t	_cur_pos;  // 当前分钟位置（交易时段内的分钟数）

	StdUniqueMutex	_mtx;  // 互斥锁，用于保护共享数据
	std::atomic<uint64_t>	_next_check_time;  // 下次检查时间（时间戳，毫秒）
	std::atomic<uint32_t>	_last_emit_pos;  // 上次触发的分钟位置

	bool			_stopped;  // 停止标志
	StdThreadPtr	_thrd;  // 后台线程指针
};

NS_WTP_END  // WonderTrader命名空间结束
