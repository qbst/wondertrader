/*!
 * \file WtCtaTicker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略实时行情驱动类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件实现了WtCtaRtTicker类的所有方法，提供CTA策略的实时行情驱动功能。
 * 
 * 实现要点：
 * 1. 行情处理：接收实时行情，更新时间，触发策略逻辑
 * 2. 分钟线闭合：检测分钟线切换，触发闭合事件和数据回放
 * 3. 自动闭合：后台线程定期检查，自动闭合未及时闭合的分钟线
 * 4. 交易日管理：处理交易日切换，确保时间连续性
 * 
 * 关键算法：
 * - 分钟线检测：通过比较当前分钟数和已处理分钟数判断是否需要闭合
 * - 时间转换：将行情时间戳转换为交易分钟的分钟数
 * - 自动闭合：计算下次检查时间，定期检查并触发闭合
 */
#include "WtCtaTicker.h"  // 包含头文件
#include "WtCtaEngine.h"  // 包含CTA引擎头文件
#include "../Includes/IDataReader.h"  // 包含数据读取器接口头文件

#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具（合约代码解析等）
#include "../Share/TimeUtils.hpp"  // 包含时间工具类（时间转换、获取当前时间等）
#include "../Includes/WTSSessionInfo.hpp"  // 包含交易会话信息类头文件
#include "../Includes/WTSDataDef.hpp"  // 包含WonderTrader数据定义头文件
#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息类头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志工具头文件

USING_NS_WTP;  // 使用WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
// WtCtaRtTicker类方法实现
//////////////////////////////////////////////////////////////////////////
/**
 * @brief 初始化驱动器
 * @param store 数据读取器指针，用于数据回放和分钟线闭合通知
 * @param sessionID 交易会话ID字符串，用于获取交易时间段信息
 * 
 * 设置数据读取器和交易会话信息，初始化当前日期和时间。
 * 如果会话ID无效，会记录致命错误日志。
 */
void WtCtaRtTicker::init(IDataReader* store, const char* sessionID)
{
	_store = store;  // 保存数据读取器指针
	_s_info = _engine->get_session_info(sessionID);  // 从引擎获取交易会话信息
	if(_s_info == NULL)  // 如果会话信息无效
		WTSLogger::fatal("Session {} is invalid, CtaTicker cannot run correctly", sessionID);  // 记录致命错误日志
	else
		WTSLogger::info("CtaTicker will drive engine with session {}", sessionID);  // 记录信息日志

	TimeUtils::getDateTime(_date, _time);  // 获取当前系统日期和时间
}

/**
 * @brief 触发价格更新
 * @param curTick 当前的Tick数据指针
 * 
 * 将行情数据传递给CTA引擎，触发策略的on_tick回调。
 * 如果是主力合约系统，还会同步触发主力合约的行情。
 */
void WtCtaRtTicker::trigger_price(WTSTickData* curTick)
{
	if (_engine )  // 如果引擎指针有效
	{
		WTSContractInfo* cInfo = curTick->getContractInfo();  // 获取合约信息
		std::string stdCode = curTick->code();  // 获取标准合约代码
		_engine->on_tick(stdCode.c_str(), curTick);  // 触发引擎的on_tick回调，传递标准合约代码和Tick数据

		if (!cInfo->isFlat())  // 如果不是单合约（即主力合约系统）
		{
			WTSTickData* hotTick = WTSTickData::create(curTick->getTickStruct());  // 创建新的Tick数据对象，复制当前Tick的结构
			const char* hotCode = cInfo->getHotCode();  // 获取主力合约代码
			hotTick->setCode(hotCode);  // 设置主力合约代码
			_engine->on_tick(hotCode, hotTick);  // 触发引擎的on_tick回调，传递主力合约代码和Tick数据
			hotTick->release();  // 释放Tick数据对象（减少引用计数）
		}
	}
}

/**
 * @brief 处理实时行情数据
 * @param curTick 当前的Tick数据指针
 * 
 * 接收实时行情数据，更新内部时间，触发价格更新和分钟线闭合逻辑。
 * 如果未启动后台线程，则直接触发价格更新。
 * 如果启动后台线程，则根据行情时间戳判断是否需要触发分钟线闭合。
 * 
 * 处理流程：
 * 1. 如果未启动后台线程，直接触发价格更新并返回
 * 2. 检查行情时间是否回退（时间戳小于当前时间），如果是则直接触发价格更新
 * 3. 更新内部日期和时间
 * 4. 计算当前分钟数，判断是否需要触发分钟线闭合
 * 5. 如果需要闭合，触发闭合事件和数据回放
 * 6. 更新引擎时间，触发价格更新
 * 7. 计算下次检查时间，供后台线程使用
 */
void WtCtaRtTicker::on_tick(WTSTickData* curTick)
{
	if (_thrd == NULL)  // 如果后台线程未启动（单线程模式）
	{
		trigger_price(curTick);  // 直接触发价格更新
		return;  // 返回，不进行时间管理
	}

	uint32_t uDate = curTick->actiondate();  // 获取行情日期（格式：YYYYMMDD）
	uint32_t uTime = curTick->actiontime();  // 获取行情时间（格式：HHMMSSmmm，毫秒级）

	if (_date != 0 && (uDate < _date || (uDate == _date && uTime < _time)))  // 如果行情时间回退（小于当前时间）
	{
		//WTSLogger::info("行情时间{}小于本地时间{}", uTime, _time);  // 记录信息日志（已注释）
		trigger_price(curTick);  // 直接触发价格更新（历史数据或延迟数据）
		return;  // 返回，不更新时间
	}

	_date = uDate;  // 更新内部日期
	_time = uTime;  // 更新内部时间

	uint32_t curMin = _time / 100000;  // 提取分钟部分（HHMM），即时间戳除以100000（去掉秒和毫秒）
	uint32_t curSec = _time % 100000;  // 提取秒和毫秒部分（SSmmm），即时间戳对100000取余

	// 静态变量，用于缓存分钟转换结果，避免重复计算
	static uint32_t prevMin = UINT_MAX;  // 上一次处理的分钟数
	static uint32_t minutes = UINT_MAX;  // 对应的交易分钟数（从交易日开始计算）
	static bool isSecEnd = false;  // 是否是交易段的最后一分钟
	static uint32_t wrapMin = UINT_MAX;  // 包装后的分钟数（考虑跨日等）

	//By Wesley @ 2023.11.01
	//如果新的分钟和上一次处理的分钟数不同，才进行处理
	//否则就不用处理，减少一些开销
	if(prevMin != curMin)  // 如果分钟数发生变化
	{
		minutes = _s_info->timeToMinutes(curMin);  // 将时间转换为交易分钟数（从交易日开始计算）
		isSecEnd = _s_info->isLastOfSection(curMin);  // 判断是否是交易段的最后一分钟
		prevMin = curMin;  // 更新上一次处理的分钟数

		if (isSecEnd)  // 如果是交易段的最后一分钟
		{
			minutes--;  // 分钟数减1（因为下一分钟不在交易段内）
		}
		minutes++;  // 分钟数加1（计算下一分钟）

		wrapMin = _s_info->minuteToTime(minutes);  // 将交易分钟数转换回时间戳
	}

	if (_cur_pos == 0)  // 如果当前位置为0（首次处理）
	{
		//如果当前时间是0, 则直接赋值即可
		_cur_pos = minutes;  // 直接设置当前位置为当前分钟数
	}
	else if (_cur_pos < minutes)  // 如果已记录的分钟数小于新的分钟数（分钟线切换）
	{
		//如果已记录的分钟小于新的分钟, 则需要触发闭合事件
		//这个时候要先触发闭合, 再修改平台时间和价格
		if (_last_emit_pos < _cur_pos)  // 如果最后触发位置小于当前位置（有未闭合的分钟线）
		{
			//触发数据回放模块
			StdUniqueLock lock(_mtx);  // 获取互斥锁，保护关键数据

			//优先修改时间标记
			_last_emit_pos = _cur_pos;  // 更新最后触发位置为当前位置

			uint32_t thisMin = _s_info->minuteToTime(_cur_pos);  // 将当前位置转换为时间戳
			
			bool bEndingTDate = false;  // 是否是交易日结束标志
			uint32_t offMin = _s_info->offsetTime(thisMin, true);  // 计算偏移时间（相对开盘时间）
			if (offMin == _s_info->getCloseTime(true))  // 如果偏移时间等于收盘时间
				bEndingTDate = true;  // 标记为交易日结束

			WTSLogger::info("Minute Bar {}.{:04d} Closed by data", _date, thisMin);  // 记录日志：分钟线被数据闭合
			if (_store)  // 如果数据读取器有效
				_store->onMinuteEnd(_date, thisMin, bEndingTDate ? _engine->getTradingDate() : 0);  // 通知数据读取器分钟线结束

			//任务调度
			_engine->on_schedule(_date, thisMin);  // 触发引擎的调度事件（定时任务）

			if(bEndingTDate)  // 如果是交易日结束
				_engine->on_session_end();  // 触发引擎的交易日结束事件
		}

		//By Wesley @ 2022.02.09
		//这里先修改时间，再调用trigger_price
		//无论分钟线是否切换，先修改时间都是对的
		if (_engine)  // 如果引擎指针有效
		{
			_engine->set_date_time(_date, wrapMin, curSec, prevMin);  // 设置引擎的日期和时间
			_engine->set_trading_date(curTick->tradingdate());  // 设置引擎的交易日期
		}
		trigger_price(curTick);  // 触发价格更新

		_cur_pos = minutes;  // 更新当前位置为新的分钟数
	}
	else  // 如果分钟数还是一致的（同一分钟内）
	{
		//如果分钟数还是一致的, 则直接触发行情和时间即可
		trigger_price(curTick);  // 触发价格更新
		if (_engine)  // 如果引擎指针有效
			_engine->set_date_time(_date, wrapMin, curSec, prevMin);  // 设置引擎的日期和时间
	}

	uint32_t sec = curSec / 1000;  // 提取秒数（curSec除以1000）
	uint32_t msec = curSec % 1000;  // 提取毫秒数（curSec对1000取余）
	uint32_t left_ticks = (60 - sec) * 1000 - msec;  // 计算到下一分钟剩余的毫秒数
	_next_check_time = TimeUtils::getLocalTimeNow() + left_ticks;  // 设置下次检查时间（当前时间加上剩余毫秒数）
}

/**
 * @brief 启动驱动器
 * 
 * 启动后台线程（如果尚未启动），初始化交易日，触发策略初始化。
 * 后台线程负责自动检测时间，触发未及时闭合的分钟线。
 * 在初始化之前会先确定交易日，确保策略初始化时交易日已经确定。
 * 
 * 启动流程：
 * 1. 检查后台线程是否已启动，如果已启动则直接返回
 * 2. 计算并设置交易日
 * 3. 触发策略初始化
 * 4. 启动后台线程，定期检查时间并触发分钟线闭合
 */
void WtCtaRtTicker::run()
{
	if (_thrd)  // 如果后台线程已启动
		return;  // 直接返回，避免重复启动

	/*
	 *	By Wesley @ 2022.12.06
	 *	这里一定要在初始化之前把交易日确定下来
	 *	不然如果策略在on_init的时候调用一些依赖交易日的接口就会出错
	 */
	uint32_t curTDate = _engine->get_basedata_mgr()->calcTradingDate(_s_info->id(), _engine->get_date(), _engine->get_min_time(), true);  // 计算当前交易日
	_engine->set_trading_date(curTDate);  // 设置引擎的交易日期
	WTSLogger::info("Trading date confirmed: {}", curTDate);  // 记录日志：交易日已确认
	_engine->on_init();  // 触发引擎的初始化事件（策略初始化）
	_engine->on_session_begin();  // 触发引擎的交易日开始事件

	//先检查当前时间, 如果大于

	_thrd.reset(new StdThread([this](){  // 创建并启动后台线程
		while(!_stopped)  // 循环直到停止标志为true
		{
			uint32_t offTime = _s_info->offsetTime(_engine->get_min_time(), true);  // 计算当前时间的偏移时间（相对开盘时间）

			if (_time != UINT_MAX && _s_info->isInTradingTime(_time / 100000, true))  // 如果时间已初始化且在交易时间段内
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 休眠10毫秒，避免CPU占用过高
				uint64_t now = TimeUtils::getLocalTimeNow();  // 获取当前系统时间（毫秒级时间戳）

				if (now >= _next_check_time && _last_emit_pos < _cur_pos)  // 如果当前时间达到检查时间且有待闭合的分钟线
				{
					//触发数据回放模块
					StdUniqueLock lock(_mtx);  // 获取互斥锁，保护关键数据

					//优先修改时间标记
					_last_emit_pos = _cur_pos;  // 更新最后触发位置为当前位置

					uint32_t thisMin = _s_info->minuteToTime(_cur_pos);  // 将当前位置转换为时间戳
					_time = thisMin*100000;  // 这里要还原成毫秒为单位（将分钟数乘以100000）

					//如果thisMin是0, 说明换日了
					//这里是本地计时导致的换日, 说明日期其实还是老日期, 要自动+1
					//同时因为时间是235959xxx, 所以也要手动置为0
					if (thisMin == 0)  // 如果分钟数为0（跨日）
					{
						uint32_t lastDate = _date;  // 保存上一次日期
						_date = TimeUtils::getNextDate(_date);  // 日期加1（获取下一个交易日）
						_time = 0;  // 时间重置为0
						WTSLogger::info("Data automatically changed at time 00:00: {} -> {}", lastDate, _date);  // 记录日志：自动换日
					}

					bool bEndingTDate = false;  // 是否是交易日结束标志
					uint32_t offMin = _s_info->offsetTime(thisMin, true);  // 计算偏移时间（相对开盘时间）
					if (offMin == _s_info->getCloseTime(true))  // 如果偏移时间等于收盘时间
						bEndingTDate = true;  // 标记为交易日结束

					WTSLogger::info("Minute bar {}.{:04d} closed automatically", _date, thisMin);  // 记录日志：分钟线自动闭合
					if (_store)  // 如果数据读取器有效
						_store->onMinuteEnd(_date, thisMin, bEndingTDate ? _engine->getTradingDate() : 0);  // 通知数据读取器分钟线结束

					//任务调度
					_engine->on_schedule(_date, thisMin);  // 触发引擎的调度事件（定时任务）

					if (bEndingTDate)  // 如果是交易日结束
						_engine->on_session_end();  // 触发引擎的交易日结束事件

					//145959000
					if (_engine)  // 如果引擎指针有效
						_engine->set_date_time(_date, thisMin, 0);  // 设置引擎的日期和时间（秒数设为0）
				}
			}
			else //if(offTime >= _s_info->getOpenTime(true) && offTime <= _s_info->getCloseTime(true))  // 如果不在交易时间段内
			{
				//收盘以后，如果发现上次触发的位置不等于总的分钟数，说明少了最后一分钟的闭合逻辑
				uint32_t total_mins = _s_info->getTradingMins();  // 获取交易日的总分钟数
				if(_time != UINT_MAX && _last_emit_pos != 0 && _last_emit_pos < total_mins && offTime >= _s_info->getCloseTime(true))  // 如果时间已初始化、最后触发位置不为0且小于总分钟数、已收盘
				{
					WTSLogger::warn("Tradingday {} will be ended forcely, last_emit_pos: {}, time: {}", _engine->getTradingDate(), _last_emit_pos.fetch_add(0), _time);  // 记录警告日志：交易日强制结束

					//触发数据回放模块
					StdUniqueLock lock(_mtx);  // 获取互斥锁，保护关键数据

					//优先修改时间标记
					_last_emit_pos = total_mins;  // 更新最后触发位置为总分钟数

					bool bEndingTDate = true;  // 标记为交易日结束
					uint32_t thisMin = _s_info->getCloseTime(false);  // 获取收盘时间（绝对时间）
					uint32_t offMin = _s_info->getCloseTime(true);  // 获取收盘时间（偏移时间）

					WTSLogger::info("Minute bar {}.{:04d} closed automatically", _date, thisMin);  // 记录日志：分钟线自动闭合
					if (_store)  // 如果数据读取器有效
						_store->onMinuteEnd(_date, thisMin, _engine->getTradingDate());  // 通知数据读取器分钟线结束

					//任务调度
					_engine->on_schedule(_date, thisMin);  // 触发引擎的调度事件（定时任务）

					_engine->on_session_end();  // 触发引擎的交易日结束事件

				}
				else  // 如果不需要强制结束
				{
					std::this_thread::sleep_for(std::chrono::seconds(10));  // 休眠10秒，避免CPU占用过高
				}
			}
		}
	}));  // 创建并启动后台线程
}

/**
 * @brief 停止驱动器
 * 
 * 设置停止标志，等待后台线程结束。
 * 停止后会清理线程资源。
 */
void WtCtaRtTicker::stop()
{
	_stopped = true;  // 设置停止标志为true，通知后台线程退出
	if (_thrd)  // 如果后台线程存在
		_thrd->join();  // 等待后台线程结束（阻塞直到线程退出）
}

/**
 * @brief 判断是否在交易时间段内
 * @return bool 如果在交易时间段内返回true，否则返回false
 * 
 * 根据交易会话信息和当前时间判断是否在交易时间段内。
 * 如果交易会话信息无效，返回false。
 */
bool WtCtaRtTicker::is_in_trading() const 
{
	if (_s_info == NULL)  // 如果交易会话信息无效
		return false;  // 返回false

	return _s_info->isInTradingTime(_time/100000, true);  // 判断当前时间是否在交易时间段内（_time/100000提取分钟部分）
}

/**
 * @brief 将时间转换为分钟数
 * @param uTime 时间（格式：HHMMSS）
 * @return uint32_t 返回对应的分钟数（从交易日开始计算的分钟数）
 * 
 * 将时间戳转换为从交易日开始计算的分钟数。
 * 如果交易会话信息无效，直接返回原始时间。
 */
uint32_t WtCtaRtTicker::time_to_mins(uint32_t uTime) const
{
	if (_s_info == NULL)  // 如果交易会话信息无效
		return uTime;  // 直接返回原始时间

	return _s_info->timeToMinutes(uTime, true);  // 将时间转换为交易分钟数（从交易日开始计算）
}