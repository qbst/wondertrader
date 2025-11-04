/*!
 * \file WtUftTicker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT实时ticker实现文件
 *
 * 本文件实现了WtUftRtTicker类，用于处理实时行情数据并触发分钟线闭合事件。
 *
 * 设计逻辑：
 * 1. 实时行情处理：接收实时tick数据，根据交易时段判断是否触发分钟线闭合
 * 2. 时间管理：维护当前日期、时间、分钟位置等信息，用于判断分钟线是否需要闭合
 * 3. 分钟线闭合：当分钟位置发生变化时，触发分钟线闭合事件
 * 4. 交易日管理：自动判断交易日开始和结束，触发交易日生命周期事件
 * 5. 后台线程：使用独立线程定时检查分钟线闭合，避免阻塞主线程
 *
 * 主要功能：
 * - 实现实时tick数据的接收和处理
 * - 实现分钟线闭合的判断和触发
 * - 实现交易日开始和结束的判断
 * - 实现后台定时检查分钟线闭合
 */
#include "WtUftTicker.h"  // UFT实时ticker头文件
#include "WtUftEngine.h"  // UFT引擎头文件
#include "../Includes/IDataReader.h"  // 数据读取器接口头文件

#include "../Share/TimeUtils.hpp"  // 时间工具头文件
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息头文件
#include "../Includes/IBaseDataMgr.h"  // 基础数据管理器接口头文件

#include "../WTSTools/WTSLogger.h"  // 日志工具头文件

USING_NS_WTP;  // 使用WonderTrader命名空间


/**
 * @brief 构造函数实现
 * @param engine UFT引擎指针
 * 
 * 创建实时ticker对象，初始化所有成员变量。
 */
WtUftRtTicker::WtUftRtTicker(WtUftEngine* engine)
	: _engine(engine)  // 初始化引擎指针
	, _stopped(false)  // 初始化停止标志为false
	, _date(0)  // 初始化日期为0
	, _time(UINT_MAX)  // 初始化时间为最大值（表示未初始化）
	, _next_check_time(0)  // 初始化下次检查时间为0
	, _last_emit_pos(0)  // 初始化上次触发位置为0
	, _cur_pos(0)  // 初始化当前分钟位置为0
{
}


/**
 * @brief 析构函数实现
 * 
 * 销毁实时ticker对象，停止后台线程。
 */
WtUftRtTicker::~WtUftRtTicker()
{
}

/**
 * @brief 初始化ticker实现
 * @param sessionID 交易时段ID
 * 
 * 初始化ticker，设置交易时段信息，获取当前日期和时间。
 */
void WtUftRtTicker::init(const char* sessionID)
{
	_s_info = _engine->get_session_info(sessionID);  // 获取交易时段信息

	TimeUtils::getDateTime(_date, _time);  // 从系统获取当前日期和时间
}

/**
 * @brief 处理Tick数据实现
 * @param curTick 当前Tick数据
 * 
 * 接收并处理实时tick数据，判断是否需要触发分钟线闭合。
 * 
 * 处理流程：
 * 1. 如果后台线程未启动，直接转发给引擎处理
 * 2. 检查tick时间是否早于当前时间（处理延迟数据）
 * 3. 更新当前日期和时间
 * 4. 计算分钟位置，判断是否需要触发分钟线闭合
 * 5. 如果分钟位置变化，触发分钟线闭合事件
 * 6. 计算下次检查时间
 */
void WtUftRtTicker::on_tick(WTSTickData* curTick)
{
	if (_thrd == NULL)  // 如果后台线程未启动
	{
		if (_engine)  // 如果引擎存在
			_engine->on_tick(curTick->code(), curTick);  // 直接转发给引擎处理
		return;  // 返回
	}

	uint32_t uDate = curTick->actiondate();  // 获取tick日期
	uint32_t uTime = curTick->actiontime();  // 获取tick时间

	if (_date != 0 && (uDate < _date || (uDate == _date && uTime < _time)))  // 如果tick时间早于当前时间（延迟数据）
	{
		//WTSLogger::info("行情时间{}小于本地时间{}", uTime, _time);
		if (_engine)  // 如果引擎存在
			_engine->on_tick(curTick->code(), curTick);  // 直接转发给引擎处理
		return;  // 返回
	}

	_date = uDate;  // 更新当前日期
	_time = uTime;  // 更新当前时间

	uint32_t curMin = _time / 100000;  // 提取分钟部分（HHMM格式）
	uint32_t curSec = _time % 100000;  // 提取秒数部分（包含毫秒）
	uint32_t minutes = _s_info->timeToMinutes(curMin);  // 将时间转换为交易时段内的分钟数
	bool isSecEnd = _s_info->isLastOfSection(curMin);  // 判断是否为时段最后一个分钟
	if (isSecEnd)  // 如果是时段最后一个分钟
	{
		minutes--;  // 分钟数减1（因为时段最后一个分钟属于下一个分钟线）
	}
	minutes++;  // 分钟数加1（因为分钟线是下一个分钟闭合）
	uint32_t rawMin = curMin;  // 保存原始分钟时间
	curMin = _s_info->minuteToTime(minutes);  // 将分钟数转换回时间

	if (_cur_pos == 0)  // 如果当前分钟位置为0（首次）
	{
		//如果当前时间是0, 则直接赋值即可
		_cur_pos = minutes;  // 直接设置当前分钟位置
	}
	else if (_cur_pos < minutes)  // 如果已记录的分钟位置小于新的分钟位置（分钟线需要闭合）
	{
		//如果已记录的分钟小于新的分钟, 则需要触发闭合事件
		//这个时候要先触发闭合, 再修改平台时间和价格
		if (_last_emit_pos < _cur_pos)  // 如果上次触发位置小于当前分钟位置（需要触发闭合）
		{
			//触发数据回放模块
			StdUniqueLock lock(_mtx);  // 加锁保护共享数据

			//优先修改时间标记
			_last_emit_pos = _cur_pos;  // 更新上次触发位置

			uint32_t thisMin = _s_info->minuteToTime(_cur_pos);  // 将分钟位置转换为时间

			WTSLogger::info("Minute Bar {}.{:04d} Closed by data", _date, thisMin);  // 记录分钟线闭合日志
			_engine->on_minute_end(_date, thisMin);  // 触发分钟线闭合事件
		}
			
		if (_engine)  // 如果引擎存在
		{
			_engine->on_tick(curTick->code(), curTick);  // 转发tick数据给引擎
			_engine->set_date_time(_date, curMin, curSec, rawMin);  // 设置引擎时间和日期
			_engine->set_trading_date(curTick->tradingdate());  // 设置引擎交易日
		}

		_cur_pos = minutes;  // 更新当前分钟位置
	}
	else  // 如果分钟位置没有变化（同一分钟内的tick）
	{
		//如果分钟数还是一致的, 则直接触发行情和时间即可
		if (_engine)  // 如果引擎存在
		{
			_engine->on_tick(curTick->code(), curTick);  // 转发tick数据给引擎
			_engine->set_date_time(_date, curMin, curSec, rawMin);  // 设置引擎时间和日期
		}
	}

	uint32_t sec = curSec / 1000;  // 提取秒数部分
	uint32_t msec = curSec % 1000;  // 提取毫秒部分
	uint32_t left_ticks = (60 - sec) * 1000 - msec;  // 计算到下一分钟剩余的毫秒数
	_next_check_time = TimeUtils::getLocalTimeNow() + left_ticks;  // 计算下次检查时间（当前时间+剩余毫秒数）
}

/**
 * @brief 启动ticker实现
 * 
 * 启动后台线程，开始定时检查分钟线闭合。
 * 流程：
 * 1. 如果线程已启动，直接返回
 * 2. 调用引擎的on_init回调
 * 3. 计算当前交易日
 * 4. 触发交易日开始事件
 * 5. 启动后台线程定时检查分钟线闭合
 */
void WtUftRtTicker::run()
{
	if (_thrd)  // 如果线程已启动
		return;  // 直接返回

	_engine->on_init();  // 调用引擎初始化回调

	uint32_t curTDate = _engine->get_basedata_mgr()->calcTradingDate(_s_info->id(), _engine->get_date(), _engine->get_min_time(), true);  // 计算当前交易日
	_engine->set_trading_date(curTDate);  // 设置引擎交易日

	_engine->on_session_begin();  // 触发交易日开始事件

	//先检查当前时间, 如果大于
	uint32_t offTime = _s_info->offsetTime(_engine->get_min_time(), true);  // 计算当前时间的偏移量

	_thrd.reset(new StdThread([this, offTime](){  // 创建后台线程
		while (!_stopped)  // 循环直到停止标志为true
		{
			if (_time != UINT_MAX && _s_info->isInTradingTime(_time / 100000, true))  // 如果时间已初始化且在交易时间内
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 休眠10毫秒
				uint64_t now = TimeUtils::getLocalTimeNow();  // 获取当前时间戳

				if (now >= _next_check_time && _last_emit_pos < _cur_pos)  // 如果到达检查时间且需要触发闭合
				{
					//触发数据回放模块
					StdUniqueLock lock(_mtx);  // 加锁保护共享数据

					//优先修改时间标记
					_last_emit_pos = _cur_pos;  // 更新上次触发位置

					uint32_t thisMin = _s_info->minuteToTime(_cur_pos);  // 将分钟位置转换为时间
					_time = thisMin;  // 更新当前时间

					//如果thisMin是0, 说明换日了
					//这里是本地计时导致的换日, 说明日期其实还是老日期, 要自动+1
					//同时因为时间是235959xxx, 所以也要手动置为0
					if (thisMin == 0)  // 如果分钟时间为0（跨日）
					{
						uint32_t lastDate = _date;  // 保存上次日期
						_date = TimeUtils::getNextDate(_date);  // 日期加1
						_time = 0;  // 时间置为0
						WTSLogger::info("Data automatically changed at time 00:00: {} -> {}", lastDate, _date);  // 记录跨日日志
					}

					WTSLogger::info("Minute bar {}.{:04d} closed automatically", _date, thisMin);  // 记录分钟线闭合日志
					//if (_store)
					//	_store->onMinuteEnd(_date, thisMin);

					_engine->on_minute_end(_date, thisMin);  // 触发分钟线闭合事件

					uint32_t offMin = _s_info->offsetTime(thisMin, true);  // 计算偏移分钟数
					if (offMin >= _s_info->getCloseTime(true))  // 如果达到收盘时间
					{
						_engine->on_session_end();  // 触发交易日结束事件
					}

					//145959000
					if (_engine)  // 如果引擎存在
						_engine->set_date_time(_date, thisMin, 0);  // 设置引擎时间和日期
				}
			}
			else //if (offTime >= _s_info->getOpenTime(true) && offTime <= _s_info->getCloseTime(true))  // 如果不在交易时间
			{
				//不在交易时间，则休息10s再进行检查
				//因为这个逻辑是处理分钟线的，所以休盘时间休息10s，不会引起数据踏空的问题
				std::this_thread::sleep_for(std::chrono::seconds(10));  // 休眠10秒
			}
			
		}
	}));  // 线程创建完成
}

/**
 * @brief 停止ticker实现
 * 
 * 停止后台线程，等待线程结束。
 */
void WtUftRtTicker::stop()
{
	_stopped = true;  // 设置停止标志为true
	if (_thrd)  // 如果线程存在
		_thrd->join();  // 等待线程结束
}
