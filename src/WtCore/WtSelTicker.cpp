/*!
 * \file WtSelTicker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 选股实时行情时钟实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtSelRtTicker类的所有方法，提供了实时行情时间管理和分钟线闭合功能。
 * 主要实现包括：
 * 1. 行情时间跟踪：接收行情数据，更新当前时间和分钟位置
 * 2. 分钟线闭合检测：检测分钟推进，触发上一分钟的闭合事件
 * 3. 自动闭合逻辑：使用独立线程监控时间，自动触发闭合
 * 4. 交易日切换：处理交易日结束和日期切换逻辑
 * 5. 非交易时间处理：在非交易时间，如果分钟发生变化，也会触发闭合事件（用于定时任务）
 * 6. 主力合约处理：非平仓合约自动触发主力合约行情
 * 
 * 实现细节：
 * - 使用原子变量保护共享状态，避免竞态条件
 * - 使用互斥锁保护闭合事件触发，防止重复触发
 * - 支持行情数据驱动的闭合和定时器驱动的自动闭合
 * - 处理行情延迟、交易日切换等边界情况
 * - 与非交易时间的分钟变化检测，支持定时任务的执行
 */
#include "WtSelTicker.h"  // 包含选股实时行情时钟头文件
#include "WtSelEngine.h"  // 包含选股引擎头文件
#include "../Includes/IDataReader.h"  // 包含数据读取器接口头文件

#include "../Share/TimeUtils.hpp"  // 包含时间工具函数
#include "../Includes/WTSSessionInfo.hpp"  // 包含交易会话信息头文件
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息头文件
#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件
#include "../Share/CodeHelper.hpp"  // 包含合约代码解析辅助工具

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

USING_NS_WTP;  // 使用WonderTrader命名空间


/**
 * @brief 构造函数
 * @param engine 选股引擎指针
 * 
 * 初始化实时行情时钟，设置引擎指针，初始化所有状态变量为默认值。
 */
WtSelRtTicker::WtSelRtTicker(WtSelEngine* engine)
	: _engine(engine)  // 初始化引擎指针
	, _stopped(false)  // 初始化停止标志为false
	, _date(0)  // 初始化日期为0
	, _time(UINT_MAX)  // 初始化时间为最大值（表示未初始化）
	, _next_check_time(0)  // 初始化下次检查时间为0
	, _last_emit_pos(0)  // 初始化最后触发的分钟位置为0
	, _cur_pos(0)  // 初始化当前分钟位置为0
{
}


/**
 * @brief 析构函数
 * 
 * 清理资源。注意：停止操作应在stop()方法中完成，析构函数仅作为占位。
 */
WtSelRtTicker::~WtSelRtTicker()
{
}

/**
 * @brief 初始化时钟
 * @param store 数据读取器指针
 * @param sessionID 交易会话ID
 * 
 * 初始化时钟，设置数据读取器和交易会话信息，获取当前系统时间作为初始时间。
 */
void WtSelRtTicker::init(IDataReader* store, const char* sessionID)
{
	_store = store;  // 保存数据读取器指针
	_s_info = _engine->get_session_info(sessionID);  // 从引擎获取交易会话信息并保存

	TimeUtils::getDateTime(_date, _time);  // 获取当前系统日期和时间
}

/**
 * @brief 触发行情价格
 * @param curTick 当前Tick数据指针
 * @param hotFlag 热点合约标志，默认为0（未使用）
 * 
 * 将行情数据转发给引擎处理。如果合约不是平仓合约（即有主力合约），
 * 还会创建一个主力合约的行情数据并触发。
 */
void WtSelRtTicker::trigger_price(WTSTickData* curTick, uint32_t hotFlag /* = 0 */)
{
	if (_engine)  // 如果引擎指针有效
	{
		std::string stdCode = curTick->code();  // 获取合约代码
		_engine->on_tick(stdCode.c_str(), curTick);  // 触发引擎的Tick行情回调

		WTSContractInfo* cInfo = curTick->getContractInfo();  // 获取合约信息
		if (!cInfo->isFlat())  // 如果合约不是平仓合约（有主力合约）
		{
			WTSTickData* hotTick = WTSTickData::create(curTick->getTickStruct());  // 创建主力合约Tick数据对象
			const char* hotCode = cInfo->getHotCode();  // 获取主力合约代码
			hotTick->setCode(hotCode);  // 设置主力合约代码
			_engine->on_tick(hotCode, hotTick);  // 触发引擎的主力合约Tick行情回调
			hotTick->release();  // 释放临时创建的Tick数据对象
		}
	}
}

/**
 * @brief 行情数据回调
 * @param curTick 当前Tick数据指针
 * @param hotFlag 热点合约标志，默认为0（未使用）
 * 
 * 接收行情数据，执行以下操作：
 * 1. 如果时钟未启动（无监控线程），直接触发行情并返回
 * 2. 检查行情时间，如果时间回退则直接触发行情
 * 3. 更新当前日期和时间
 * 4. 计算当前分钟位置
 * 5. 如果检测到分钟推进，触发上一分钟的闭合事件
 * 6. 触发行情并更新引擎时间
 * 7. 计算下次检查时间
 */
void WtSelRtTicker::on_tick(WTSTickData* curTick, uint32_t hotFlag /* = 0 */)
{
	if (_thrd == NULL)  // 如果监控线程未创建（时钟未启动）
	{
		trigger_price(curTick, hotFlag);  // 直接触发行情价格
		return;  // 直接返回，不进行时间管理
	}

	uint32_t uDate = curTick->actiondate();  // 获取行情动作日期
	uint32_t uTime = curTick->actiontime();  // 获取行情动作时间（格式：HHMMSSmmm）

	if (_date != 0 && (uDate < _date || (uDate == _date && uTime < _time)))  // 如果已初始化且行情时间小于当前时间（时间回退）
	{
		//WTSLogger::info("行情时间{}小于本地时间{}", uTime, _time);
		trigger_price(curTick, hotFlag);  // 直接触发行情价格，不更新时间
		return;  // 直接返回
	}

	_date = uDate;  // 更新当前日期
	_time = uTime;  // 更新当前时间

	uint32_t curMin = _time / 100000;  // 提取分钟部分（HHMM）
	uint32_t curSec = _time % 100000;  // 提取秒和毫秒部分（SSmmm）
	uint32_t minutes = _s_info->timeToMinutes(curMin);  // 将时间转换为交易分钟数（从0开始）
	bool isSecEnd = _s_info->isLastOfSection(curMin);  // 检查是否为交易段的最后一分钟
	if (isSecEnd)  // 如果是交易段的最后一分钟
	{
		minutes--;  // 分钟数减1（因为该分钟属于下一个交易段）
	}
	minutes++;  // 分钟数加1（因为要计算当前分钟）
	uint32_t rawMin = curMin;  // 保存原始分钟时间（HHMM格式）
	curMin = _s_info->minuteToTime(minutes);  // 将交易分钟数转换回时间格式（HHMM）

	if (_cur_pos == 0)  // 如果当前分钟位置为0（首次初始化）
	{
		//如果当前时间是0, 则直接赋值即可
		_cur_pos = minutes;  // 直接设置当前分钟位置
	}
	else if (_cur_pos < minutes)  // 如果当前分钟位置小于新的分钟位置（分钟推进）
	{
		//如果已记录的分钟小于新的分钟, 则需要触发闭合事件
		//这个时候要先触发闭合, 再修改平台时间和价格
		if (_last_emit_pos < _cur_pos)  // 如果上一分钟还未触发闭合
		{
			//触发数据回放模块
			StdUniqueLock lock(_mtx);  // 获取互斥锁，保护闭合事件触发

			//优先修改时间标记
			_last_emit_pos = _cur_pos;  // 更新最后触发的分钟位置

			uint32_t thisMin = _s_info->minuteToTime(_cur_pos);  // 将分钟位置转换为时间格式（HHMM）

			WTSLogger::info("Minute Bar {}.{:04d} Closed by data", _date, thisMin);  // 记录分钟线闭合日志（由数据驱动）
			if (_store)  // 如果数据读取器有效
				_store->onMinuteEnd(_date, thisMin);  // 触发数据回放模块的分钟结束事件

			_engine->on_minute_end(_date, thisMin);  // 触发引擎的分钟结束回调

			uint32_t offMin = _s_info->offsetTime(thisMin, true);  // 计算偏移时间（相对于开盘的分钟数）
			if (offMin == _s_info->getCloseTime(true))  // 如果偏移时间等于收盘时间
			{
				_engine->on_session_end();  // 触发交易日结束事件
			}
		}

		trigger_price(curTick, hotFlag);  // 触发行情价格
		if (_engine)  // 如果引擎指针有效
		{
			_engine->set_date_time(_date, curMin, curSec, rawMin);  // 更新引擎的日期和时间
			_engine->set_trading_date(curTick->tradingdate());  // 更新引擎的交易日期
		}

		_cur_pos = minutes;  // 更新当前分钟位置
	}
	else  // 如果分钟位置未变化（仍在同一分钟内）
	{
		//如果分钟数还是一致的, 则直接触发行情和时间即可
		trigger_price(curTick, hotFlag);  // 触发行情价格
		if (_engine)  // 如果引擎指针有效
			_engine->set_date_time(_date, curMin, curSec, rawMin);  // 更新引擎的日期和时间
	}

	uint32_t sec = curSec / 1000;  // 提取秒数部分
	uint32_t msec = curSec % 1000;  // 提取毫秒部分
	uint32_t left_ticks = (60 - sec) * 1000 - msec;  // 计算到下一分钟剩余的毫秒数
	_next_check_time = TimeUtils::getLocalTimeNow() + left_ticks;  // 计算下次检查时间（当前本地时间+剩余毫秒数）
}

/**
 * @brief 运行时钟
 * 
 * 启动时钟，执行以下操作：
 * 1. 如果时钟已启动，直接返回
 * 2. 计算当前交易日
 * 3. 触发引擎初始化和交易日开始事件
 * 4. 创建监控线程，定期检查是否需要自动触发分钟线闭合
 * 
 * 监控线程逻辑：
 * - 在交易时间内，每10毫秒检查一次
 * - 如果到达闭合时间且上一分钟未闭合，自动触发闭合
 * - 处理交易日切换（分钟时间为0时自动加1天）
 * - 非交易时间如果分钟发生变化，也会触发闭合事件（用于定时任务）
 */
void WtSelRtTicker::run()
{
	if (_thrd)  // 如果监控线程已创建
		return;  // 直接返回，避免重复启动

	uint32_t curTDate = _engine->get_basedata_mgr()->calcTradingDate(_s_info->id(), _engine->get_date(), _engine->get_min_time(), true);  // 计算当前交易日
	_engine->set_trading_date(curTDate);  // 设置引擎的交易日期

	_engine->on_init();  // 触发引擎初始化事件

	_engine->on_session_begin();  // 触发交易日开始事件

	//先检查当前时间, 如果大于
	//uint32_t offTime = _s_info->offsetTime(_engine->get_min_time());  // 注释掉：计算当前时间的偏移量

	_thrd.reset(new StdThread([this](){  // 创建监控线程
		while (!_stopped)  // 循环直到停止标志为true
		{
			if (_time != UINT_MAX && _s_info->isInTradingTime(_time / 100000, true))  // 如果时间已初始化且在交易时间内
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 休眠10毫秒
				uint64_t now = TimeUtils::getLocalTimeNow();  // 获取当前本地时间戳

				if (now >= _next_check_time && _last_emit_pos < _cur_pos)  // 如果到达检查时间且上一分钟未闭合
				{
					//触发数据回放模块
					StdUniqueLock lock(_mtx);  // 获取互斥锁，保护闭合事件触发

					//优先修改时间标记
					_last_emit_pos = _cur_pos;  // 更新最后触发的分钟位置

					uint32_t thisMin = _s_info->minuteToTime(_cur_pos);  // 将分钟位置转换为时间格式（HHMM）
					_time = thisMin;  // 更新当前时间

					//如果thisMin是0, 说明换日了
					//这里是本地计时导致的换日, 说明日期其实还是老日期, 要自动+1
					//同时因为时间是235959xxx, 所以也要手动置为0
					if (thisMin == 0)  // 如果分钟时间为0（跨日）
					{
						uint32_t lastDate = _date;  // 保存旧日期
						_date = TimeUtils::getNextDate(_date);  // 日期加1天
						_time = 0;  // 时间重置为0
						WTSLogger::info("Data automatically changed at time 00:00: {} -> {}", lastDate, _date);  // 记录日期切换日志
					}

					WTSLogger::info("Minute bar {}.{:04d} closed automatically", _date, thisMin);  // 记录分钟线闭合日志（自动触发）
					if (_store)  // 如果数据读取器有效
						_store->onMinuteEnd(_date, thisMin);  // 触发数据回放模块的分钟结束事件

					_engine->on_minute_end(_date, thisMin);  // 触发引擎的分钟结束回调

					uint32_t offMin = _s_info->offsetTime(thisMin, true);  // 计算偏移时间（相对于开盘的分钟数）
					if (offMin >= _s_info->getCloseTime(true))  // 如果偏移时间大于等于收盘时间
					{
						_engine->on_session_end();  // 触发交易日结束事件
					}

					//145959000
					if (_engine)  // 如果引擎指针有效
						_engine->set_date_time(_date, thisMin, 0);  // 更新引擎的日期和时间（秒数设为0）
				}
			}
			else  // 如果不在交易时间内
			{//如果不在交易时间,则每隔10毫秒检查一次,如果分钟发生变化则触发
				std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 休眠10毫秒
				uint32_t curTime = TimeUtils::getCurMin();  // 获取当前系统时间的分钟部分（HHMM格式）
				if (_time != UINT_MAX && curTime != _time)  // 如果时间已初始化且当前分钟与记录的时间不同（分钟变化）
				{
					_engine->on_minute_end(_date, _time);  // 触发引擎的分钟结束回调（使用记录的时间）
					if (curTime < _time)  // 如果当前分钟小于记录的时间（跨日）
						_date = TimeUtils::getNextDate(_date);  // 日期加1天
					_time = curTime;  // 更新记录的时间
				}
			}
		}
	}));  // 监控线程结束
}

/**
 * @brief 停止时钟
 * 
 * 停止时钟，设置停止标志，并等待监控线程结束。
 */
void WtSelRtTicker::stop()
{
	_stopped = true;  // 设置停止标志为true
	if (_thrd)  // 如果监控线程已创建
		_thrd->join();  // 等待线程结束
}
