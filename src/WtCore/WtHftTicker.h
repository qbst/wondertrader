/*!
 * \file WtHftTicker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易实时行情时钟头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的高频交易实时行情时钟类，用于管理实时行情时间和触发分钟线闭合。
 * 主要功能包括：
 * 1. 时间管理：跟踪当前行情时间和交易日，管理时间推进
 * 2. 分钟线闭合：检测分钟线闭合时间点，触发分钟线闭合事件
 * 3. 交易日切换：检测交易日结束，触发交易日结束事件
 * 4. 行情触发：将接收到的行情数据转发给引擎处理
 * 5. 自动闭合：在交易时间内，如果行情数据延迟，自动触发分钟线闭合
 * 6. 强制结束：在收盘后检查并强制触发最后一分钟的闭合逻辑
 * 
 * 设计模式：
 * - 观察者模式：监听行情数据，触发时间事件
 * - 线程模式：使用独立线程监控时间，自动触发闭合事件
 * - 状态模式：根据交易时间和行情状态决定是否触发闭合
 * 
 * 使用场景：
 * 该时钟主要用于高频交易场景，需要精确管理时间进度，确保分钟线能够及时闭合，
 * 同时处理行情延迟和交易日切换等特殊情况。
 */
#pragma once  // 防止头文件重复包含

#include <stdint.h>  // 包含标准整数类型定义
#include <atomic>  // 包含原子操作类型定义

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader的宏定义
#include "../Share/StdUtils.hpp"  // 包含标准工具函数

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSSessionInfo;  // 前向声明：交易会话信息类
class IDataReader;  // 前向声明：数据读取器接口
class WTSTickData;  // 前向声明：Tick数据类

class WtHftEngine;  // 前向声明：HFT引擎类

/**
 * @class WtHftRtTicker
 * @brief 高频交易实时行情时钟类
 * 
 * 该类负责管理实时行情时间和触发分钟线闭合事件。
 * 主要功能：
 * - 接收实时行情数据，更新当前时间
 * - 检测分钟线闭合时间点，触发闭合事件
 * - 使用独立线程监控时间，自动触发闭合
 * - 处理交易日切换和收盘强制闭合逻辑
 * 
 * 工作原理：
 * 1. 接收到行情数据时，更新当前时间和分钟位置
 * 2. 如果检测到分钟推进，触发上一分钟的闭合事件
 * 3. 使用独立线程每秒检查一次，如果到达闭合时间则自动触发
 * 4. 收盘后检查是否有未闭合的分钟线，强制闭合
 */
class WtHftRtTicker
{
public:
	/**
	 * @brief 构造函数
	 * @param engine HFT引擎指针
	 * 
	 * 初始化实时行情时钟，设置引擎指针，初始化所有状态变量。
	 */
	WtHftRtTicker(WtHftEngine* engine);  // 构造函数
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，停止监控线程。
	 */
	~WtHftRtTicker();  // 析构函数

public:
	/**
	 * @brief 初始化时钟
	 * @param store 数据读取器指针
	 * @param sessionID 交易会话ID
	 * 
	 * 初始化时钟，设置数据读取器和交易会话信息，获取当前系统时间。
	 */
	void	init(IDataReader* store, const char* sessionID);  // 初始化时钟
	
	/**
	 * @brief 行情数据回调
	 * @param curTick 当前Tick数据指针
	 * 
	 * 接收行情数据，更新当前时间，检测分钟线闭合，并将行情转发给引擎。
	 * 如果时钟未启动（无监控线程），则直接触发行情。
	 */
	void	on_tick(WTSTickData* curTick);  // 行情数据回调

	/**
	 * @brief 运行时钟
	 * 
	 * 启动时钟，创建监控线程，初始化交易日，触发引擎初始化和交易日开始事件。
	 */
	void	run();  // 运行时钟
	
	/**
	 * @brief 停止时钟
	 * 
	 * 停止时钟，等待监控线程结束。
	 */
	void	stop();  // 停止时钟

private:
	/**
	 * @brief 触发行情价格
	 * @param curTick 当前Tick数据指针
	 * 
	 * 将行情数据转发给引擎处理，如果是非平仓合约，还会触发主力合约行情。
	 */
	void	trigger_price(WTSTickData* curTick);  // 触发行情价格

private:
	WTSSessionInfo*	_s_info;  // 交易会话信息指针，用于查询交易时间和时间转换
	WtHftEngine*	_engine;  // HFT引擎指针，用于触发时间事件和行情回调
	IDataReader*		_store;  // 数据读取器指针，用于触发数据回放模块的分钟结束事件

	uint32_t	_date;  // 当前日期，格式为YYYYMMDD
	uint32_t	_time;  // 当前时间（分钟），格式为HHMM

	uint32_t	_cur_pos;  // 当前分钟位置（交易分钟数，从0开始）

	StdUniqueMutex	_mtx;  // 互斥锁，用于保护共享数据的线程安全访问
	std::atomic<uint64_t>	_next_check_time;  // 下次检查时间（本地时间戳），用于自动闭合逻辑
	std::atomic<uint32_t>	_last_emit_pos;  // 最后触发的分钟位置，用于防止重复触发

	bool			_stopped;  // 停止标志，用于通知监控线程退出
	StdThreadPtr	_thrd;  // 监控线程指针，用于自动触发分钟线闭合
};

NS_WTP_END  // 结束WonderTrader命名空间
