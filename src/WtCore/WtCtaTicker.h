/*!
 * \file WtCtaTicker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略实时行情驱动类头文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件定义了WtCtaRtTicker类，用于CTA策略的实时行情驱动和时间管理。
 * 
 * 核心功能：
 * 1. 行情驱动：接收实时行情数据，驱动CTA引擎运行，触发策略逻辑
 * 2. 时间管理：管理交易时间，处理分钟线闭合逻辑，确保策略在正确的时间点执行
 * 3. 自动闭合：当行情数据延迟或缺失时，通过后台线程自动触发分钟线闭合事件
 * 4. 会话管理：根据交易会话信息判断交易时间段，处理交易日切换逻辑
 * 
 * 设计特点：
 * - 支持单线程模式（无后台线程）和多线程模式（有后台线程自动闭合）
 * - 时间驱动：基于行情时间戳驱动策略执行，而非简单的回调
 * - 分钟线闭合：自动检测分钟线切换，触发闭合事件和数据回放
 * - 线程安全：使用互斥锁保护关键数据，原子变量保证线程安全
 * 
 * 工作流程：
 * 1. 初始化：设置数据读取器和交易会话信息
 * 2. 运行：启动后台线程（如果启用），初始化交易日并触发策略初始化
 * 3. 行情处理：接收行情数据，更新时间，触发价格更新和分钟线闭合
 * 4. 自动闭合：后台线程检测时间，自动触发未闭合的分钟线
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含标准整数类型定义（uint32_t等）
#include <atomic>  // 包含原子操作类型定义（std::atomic）

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader的宏定义
#include "../Share/StdUtils.hpp"  // 包含标准工具类（互斥锁、线程等）

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSSessionInfo;  // 前向声明：交易会话信息类
class IDataReader;  // 前向声明：数据读取器接口
class WTSTickData;  // 前向声明：Tick数据类

class WtCtaEngine;  // 前向声明：CTA引擎类
//////////////////////////////////////////////////////////////////////////
/**
 * @class WtCtaRtTicker
 * @brief CTA策略实时行情驱动类
 * 
 * 该类负责CTA策略的实时行情驱动和时间管理。
 * 主要功能包括：
 * 1. 接收实时行情数据，驱动CTA引擎执行策略逻辑
 * 2. 管理交易时间，处理分钟线闭合逻辑
 * 3. 当行情数据延迟或缺失时，通过后台线程自动触发分钟线闭合
 * 4. 根据交易会话信息判断交易时间段，处理交易日切换
 * 
 * 工作原理：
 * - 通过on_tick接收实时行情，根据行情时间戳更新内部时间
 * - 检测分钟线切换，触发分钟线闭合事件和数据回放
 * - 后台线程定期检查时间，自动闭合未及时闭合的分钟线
 */
class WtCtaRtTicker
{
public:
	/**
	 * @brief 构造函数
	 * @param engine CTA引擎指针，用于触发策略逻辑
	 * 
	 * 初始化实时行情驱动器，设置引擎指针并初始化所有成员变量。
	 * 初始状态：停止标志为false，日期为0，时间为UINT_MAX（表示未初始化），
	 * 检查时间和位置标记均为0。
	 */
	WtCtaRtTicker(WtCtaEngine* engine) 
		: _engine(engine)  // 设置CTA引擎指针
		, _stopped(false)  // 初始化停止标志为false
		, _date(0)  // 初始化日期为0（未设置）
		, _time(UINT_MAX)  // 初始化时间为最大值（表示未初始化）
		, _next_check_time(0)  // 初始化下次检查时间为0
		, _last_emit_pos(0)  // 初始化最后触发位置为0
		, _cur_pos(0){}  // 初始化当前位置为0
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，如果后台线程正在运行，需要先调用stop()停止线程。
	 */
	~WtCtaRtTicker(){}

public:
	/**
	 * @brief 初始化驱动器
	 * @param store 数据读取器指针，用于数据回放和分钟线闭合通知
	 * @param sessionID 交易会话ID字符串，用于获取交易时间段信息
	 * 
	 * 设置数据读取器和交易会话信息，初始化当前日期和时间。
	 * 如果会话ID无效，会记录致命错误日志。
	 */
	void	init(IDataReader* store, const char* sessionID);
	//void	set_time(uint32_t uDate, uint32_t uTime);  // 已废弃的时间设置方法
	/**
	 * @brief 处理实时行情数据
	 * @param curTick 当前的Tick数据指针
	 * 
	 * 接收实时行情数据，更新内部时间，触发价格更新和分钟线闭合逻辑。
	 * 如果未启动后台线程，则直接触发价格更新。
	 * 如果启动后台线程，则根据行情时间戳判断是否需要触发分钟线闭合。
	 */
	void	on_tick(WTSTickData* curTick);

	/**
	 * @brief 启动驱动器
	 * 
	 * 启动后台线程（如果尚未启动），初始化交易日，触发策略初始化。
	 * 后台线程负责自动检测时间，触发未及时闭合的分钟线。
	 * 在初始化之前会先确定交易日，确保策略初始化时交易日已经确定。
	 */
	void	run();
	/**
	 * @brief 停止驱动器
	 * 
	 * 设置停止标志，等待后台线程结束。
	 * 停止后会清理线程资源。
	 */
	void	stop();

	/**
	 * @brief 判断是否在交易时间段内
	 * @return bool 如果在交易时间段内返回true，否则返回false
	 * 
	 * 根据交易会话信息和当前时间判断是否在交易时间段内。
	 */
	bool		is_in_trading() const;
	/**
	 * @brief 将时间转换为分钟数
	 * @param uTime 时间（格式：HHMMSS）
	 * @return uint32_t 返回对应的分钟数（从交易日开始计算的分钟数）
	 * 
	 * 将时间戳转换为从交易日开始计算的分钟数。
	 * 如果交易会话信息无效，直接返回原始时间。
	 */
	uint32_t	time_to_mins(uint32_t uTime) const;

private:
	/**
	 * @brief 触发价格更新
	 * @param curTick 当前的Tick数据指针
	 * 
	 * 将行情数据传递给CTA引擎，触发策略的on_tick回调。
	 * 如果是主力合约系统，还会同步触发主力合约的行情。
	 */
	void	trigger_price(WTSTickData* curTick);

private:
	WTSSessionInfo*	_s_info;  // 交易会话信息指针，包含交易时间段、开盘收盘时间等
	WtCtaEngine*	_engine;  // CTA引擎指针，用于触发策略逻辑
	IDataReader*	_store;  // 数据读取器指针，用于数据回放和分钟线闭合通知

	uint32_t	_date;  // 当前日期（格式：YYYYMMDD）
	uint32_t	_time;  // 当前时间（格式：HHMMSSmmm，即毫秒级时间戳）

	uint32_t	_cur_pos;  // 当前位置（分钟数），表示当前已处理的分钟数

	StdUniqueMutex	_mtx;  // 互斥锁，用于保护关键数据的线程安全
	std::atomic<uint64_t>	_next_check_time;  // 下次检查时间（原子变量，毫秒级时间戳）
	std::atomic<uint32_t>	_last_emit_pos;  // 最后触发位置（原子变量，分钟数）

	bool			_stopped;  // 停止标志，用于控制后台线程退出
	StdThreadPtr	_thrd;  // 后台线程指针，用于自动触发分钟线闭合

};
NS_WTP_END  // 结束WonderTrader命名空间