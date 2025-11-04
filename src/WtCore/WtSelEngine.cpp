/*!
 * \file WtSelEngine.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 选股引擎实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtSelEngine类的所有方法，提供了选股引擎的核心功能。
 * 主要实现包括：
 * 1. 引擎初始化：初始化引擎配置和基础组件
 * 2. 引擎运行：启动策略上下文，创建实时行情时钟，生成标记文件
 * 3. 定时任务：检测分钟线闭合，判断任务触发条件，异步执行策略调度
 * 4. 行情分发：处理Tick行情、K线数据的分发逻辑，支持复权处理
 * 5. 仓位管理：处理策略的仓位变化信号，支持过滤器、风险控制和热点合约转换
 * 6. 执行器存根：实现IExecuterStub接口，为执行器提供查询功能
 * 
 * 实现细节：
 * - 使用订阅映射表管理策略对合约的订阅关系
 * - 支持前复权和后复权的行情数据处理和价格修正
 * - 定时任务支持节假日顺延逻辑
 * - 使用独立线程异步执行策略调度，避免阻塞主线程
 * - 支持风险倍数控制和热点合约自动转换
 */
#include "WtSelEngine.h"  // 包含选股引擎头文件
#include "WtDtMgr.h"  // 包含数据管理器头文件
#include "WtSelTicker.h"  // 包含选股实时行情时钟头文件
#include "TraderAdapter.h"  // 包含交易适配器头文件
#include "WtHelper.h"  // 包含辅助工具类头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具
#include "../Share/TimeUtils.hpp"  // 包含时间工具函数
#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件
#include "../Share/StrUtil.hpp"  // 包含字符串处理工具函数
#include "../Includes/WTSVariant.hpp"  // 包含变体类型定义
#include "../Includes/WTSSessionInfo.hpp"  // 包含交易会话信息头文件
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息头文件
#include "../Share/CodeHelper.hpp"  // 包含合约代码解析辅助工具
#include "../Share/decimal.h"  // 包含高精度小数运算工具

#include <rapidjson/document.h>  // 包含RapidJSON文档类
#include <rapidjson/prettywriter.h>  // 包含RapidJSON格式化写入器
namespace rj = rapidjson;  // RapidJSON命名空间别名

#include <atomic>  // 包含原子操作类型定义

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 生成任务ID
 * @return uint32_t 返回新的任务ID
 * 
 * 使用原子操作生成唯一的任务ID，从1开始递增。
 * 使用静态局部变量确保全局唯一性。
 */
inline uint32_t makeTaskId()
{
	static std::atomic<uint32_t> _auto_task_id{ 1 };  // 静态原子变量，初始值为1
	return _auto_task_id.fetch_add(1);  // 原子性地获取当前值并加1，返回旧值
}


/**
 * @brief 构造函数
 * 
 * 初始化选股引擎，将终止标志设置为false，配置对象指针设置为NULL。
 */
WtSelEngine::WtSelEngine()
	: _terminated(false)  // 初始化终止标志为false
	, _cfg(NULL)  // 初始化配置对象指针为NULL
{
}


/**
 * @brief 析构函数
 * 
 * 清理资源。注意：停止操作应在stop()方法中完成，析构函数仅作为占位。
 */
WtSelEngine::~WtSelEngine()
{
}

/**
 * @brief 交易日结束回调
 * 
 * 处理交易日结束事件，如果事件监听器已设置，则通知交易日结束事件。
 */
void WtSelEngine::on_session_end()
{
	if (_evt_listener)  // 如果事件监听器已设置
		_evt_listener->on_session_event(_cur_tdate, false);  // 通知事件监听器交易日结束（false表示结束）
}

/**
 * @brief 交易日开始回调
 * 
 * 处理交易日开始事件，如果事件监听器已设置，则通知交易日开始事件，并设置引擎就绪标志。
 */
void WtSelEngine::on_session_begin()
{
	if (_evt_listener)  // 如果事件监听器已设置
		_evt_listener->on_session_event(_cur_tdate, true);  // 通知事件监听器交易日开始（true表示开始）

	_ready = true;  // 设置引擎就绪标志
}

/**
 * @brief 引擎初始化回调
 * 
 * 处理引擎初始化完成事件，如果事件监听器已设置，则通知初始化完成事件。
 */
void WtSelEngine::on_init()
{
	if (_evt_listener)  // 如果事件监听器已设置
		_evt_listener->on_initialize_event();  // 通知事件监听器初始化完成
}

/**
 * @brief 处理推送的行情数据
 * @param curTick 当前Tick数据指针
 * 
 * 接收外部推送的实时行情数据，如果时间时钟已创建则转发给它处理。
 */
void WtSelEngine::handle_push_quote(WTSTickData* curTick)
{
	if (_tm_ticker)  // 如果时间时钟指针有效
		_tm_ticker->on_tick(curTick);  // 将行情数据转发给时间时钟处理
}

/**
 * @brief K线数据回调
 * @param stdCode 标准合约代码
 * @param period K线周期
 * @param times 周期倍数
 * @param newBar 新的K线数据指针
 * 
 * 处理K线数据闭合事件，构建订阅键（合约代码-周期-倍数），查找订阅了该K线的策略并分发数据。
 */
void WtSelEngine::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	thread_local static char key[64] = { 0 };  // 线程局部静态字符数组，用于存储订阅键
	fmtutil::format_to(key, "{}-{}-{}", stdCode, period, times);  // 格式化订阅键：合约代码-周期-倍数

	const SubList& sids = _bar_sub_map[key];  // 在K线订阅表中查找该键的订阅列表
	for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
	{
		uint32_t sid = it->first;  // 获取策略ID
		auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
		if (cit != _ctx_map.end())  // 如果找到策略上下文
		{
			SelContextPtr& ctx = (SelContextPtr&)cit->second;  // 获取策略上下文引用
			ctx->on_bar(stdCode, period, times, newBar);  // 调用策略的K线回调方法
		}
	}

	WTSLogger::info("KBar [{}] @ {} closed", key, period[0] == 'd' ? newBar->date : newBar->time);  // 记录K线闭合日志（日线使用date，其他使用time）
}

/**
 * @brief Tick行情回调
 * @param stdCode 标准合约代码
 * @param curTick 当前Tick数据指针
 * 
 * 处理Tick行情数据，执行以下操作：
 * 1. 调用基类的on_tick方法处理基础逻辑
 * 2. 将行情数据推送给数据管理器
 * 3. 转发给执行器管理器处理
 * 4. 根据订阅标记进行复权处理：
 *    - 标记为0：无复权，直接使用原始代码和价格
 *    - 标记为1：前复权，使用代码+'-'后缀，价格不变
 *    - 标记为2：后复权，使用代码+'+'后缀，价格乘以复权因子
 * 5. 分发给订阅的策略
 */
void WtSelEngine::on_tick(const char* stdCode, WTSTickData* curTick)
{
	WtEngine::on_tick(stdCode, curTick);  // 调用基类的on_tick方法处理基础逻辑

	_data_mgr->handle_push_quote(stdCode, curTick);  // 将行情数据推送给数据管理器

	//如果是真实代码, 则要传递给执行器
	{
		_exec_mgr.handle_tick(stdCode, curTick);  // 转发给执行器管理器处理Tick行情
	}

	/*
	auto sit = _tick_sub_map.find(stdCode);
	if (sit != _tick_sub_map.end())
	{
		const SubList& sids = sit->second;
		for (auto it = sids.begin(); it != sids.end(); it++)
		{
			uint32_t sid = *it;
			auto cit = _ctx_map.find(sid);
			if (cit != _ctx_map.end() && curTick->volume())
			{
				SelContextPtr& ctx = (SelContextPtr&)cit->second;
				ctx->on_tick(stdCode, curTick);
			}
		}
	}
	*/

	/*
	 *	By Wesley @ 2022.02.07
	 *	这里做了一个彻底的调整
	 *	第一，检查订阅标记，如果标记为0，即无复权模式，则直接按照原始代码触发ontick
	 *	第二，如果标记为1，即前复权模式，则将代码转成xxxx-，再触发ontick
	 *	第三，如果标记为2，即后复权模式，则将代码转成xxxx+，再把tick数据做一个修正，再触发ontick
	 */
	if(_ready)  // 如果引擎已就绪
	{
		auto sit = _tick_sub_map.find(stdCode);  // 在Tick订阅表中查找该合约
		if (sit != _tick_sub_map.end())  // 如果找到订阅记录
		{
			uint32_t flag = get_adjusting_flag();  // 获取复权调整标志（1=成交量，2=成交额，4=持仓量）

			const SubList& sids = sit->second;  // 获取订阅该合约的策略ID列表
			for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
			{
				uint32_t sid = it->first;  // 获取策略ID


				auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
				if (cit != _ctx_map.end())  // 如果找到策略上下文
				{
					SelContextPtr& ctx = (SelContextPtr&)cit->second;  // 获取策略上下文引用
					uint32_t opt = it->second.second;  // 获取订阅标记（0=无复权，1=前复权，2=后复权）

					if (opt == 0)  // 如果标记为0（无复权）
					{
						ctx->on_tick(stdCode, curTick);  // 直接使用原始代码和价格触发策略回调
					}
					else  // 如果标记为1或2（前复权或后复权）
					{
						std::string wCode = stdCode;  // 创建代码字符串副本
						wCode = fmt::format("{}{}", stdCode, opt == 1 ? SUFFIX_QFQ : SUFFIX_HFQ);  // 添加复权后缀（'-'或'+'）
						if (opt == 1)  // 如果标记为1（前复权）
						{
							ctx->on_tick(wCode.c_str(), curTick);  // 使用复权代码和原始价格触发策略回调
						}
						else //(opt == 2)  // 如果标记为2（后复权）
						{
							WTSTickData* newTick = WTSTickData::create(curTick->getTickStruct());  // 创建新的Tick数据对象
							WTSTickStruct& newTS = newTick->getTickStruct();  // 获取新的Tick结构体引用
							newTick->setContractInfo(curTick->getContractInfo());  // 设置合约信息

							//这里做一个复权因子的处理
							double factor = get_exright_factor(stdCode);  // 获取复权因子
							newTS.open *= factor;  // 开盘价乘以复权因子
							newTS.high *= factor;  // 最高价乘以复权因子
							newTS.low *= factor;  // 最低价乘以复权因子
							newTS.price *= factor;  // 最新价乘以复权因子

							/*
							 *	By Wesley @ 2022.08.15
							 *	这里对tick的复权做一个完善
							 */
							if (flag & 1)  // 如果复权标志包含成交量调整（第0位为1）
							{
								newTS.total_volume /= factor;  // 总成交量除以复权因子
								newTS.volume /= factor;  // 成交量除以复权因子
							}

							if (flag & 2)  // 如果复权标志包含成交额调整（第1位为1）
							{
								newTS.total_turnover *= factor;  // 总成交额乘以复权因子
								newTS.turn_over *= factor;  // 成交额乘以复权因子
							}

							if (flag & 4)  // 如果复权标志包含持仓量调整（第2位为1）
							{
								newTS.open_interest /= factor;  // 持仓量除以复权因子
								newTS.diff_interest /= factor;  // 持仓变化量除以复权因子
								newTS.pre_interest /= factor;  // 昨持仓量除以复权因子
							}

							_price_map[wCode] = newTS.price;  // 将修正后的价格存入价格映射表

							ctx->on_tick(wCode.c_str(), newTick);  // 使用复权代码和修正后的价格触发策略回调
							newTick->release();  // 释放临时创建的Tick数据对象
						}
					}

				}


			}

		}
	}
}

/**
 * @brief 分钟结束回调
 * @param curDate 当前日期
 * @param curTime 当前时间（分钟）
 * 
 * 处理分钟线闭合事件，执行以下操作：
 * 1. 计算下一分钟的时间
 * 2. 遍历所有定时任务
 * 3. 检查任务是否到达触发时间
 * 4. 根据任务周期类型判断是否应该触发
 * 5. 如果触发，创建独立线程异步执行策略的on_schedule回调
 * 
 * 任务周期判断逻辑：
 * - TPT_Daily: 每个交易日都触发
 * - TPT_Minute: 分钟数能被指定时间整除时触发
 * - TPT_Monthly: 每月指定日期触发，支持节假日顺延
 * - TPT_Weekly: 每周指定星期触发，支持节假日顺延
 * - TPT_Yearly: 每年指定日期触发
 */
void WtSelEngine::on_minute_end(uint32_t curDate, uint32_t curTime)
{
	//要比较下一分钟的时间
	uint32_t nextTime = TimeUtils::getNextMinute(curTime, 1);  // 计算下一分钟的时间（HHMM格式）
	if (nextTime < curTime)  // 如果下一分钟小于当前时间（跨小时或跨日）
		curDate = TimeUtils::getNextDate(curDate);  // 日期加1天

	uint32_t weekDay = TimeUtils::getWeekDay(curDate);  // 计算当前日期是星期几（0=周日，1=周一，...，6=周六）

	for (auto& v : _tasks)  // 遍历所有定时任务
	{
		TaskInfoPtr& tInfo = (TaskInfoPtr&)v.second;  // 获取任务信息引用
		if (tInfo->_time != nextTime)  // 如果任务的触发时间不等于下一分钟的时间
			continue;  // 跳过该任务

		uint64_t now = (uint64_t)curDate * 10000 + nextTime;  // 构建当前时间戳（YYYYMMDDHHMM格式）
		if (tInfo->_last_exe_time >= now)  // 如果上次执行时间大于等于当前时间（防止重复执行）
			continue;  // 跳过该任务

		if (_base_data_mgr->isHoliday(tInfo->_trdtpl, curDate, true))  // 如果当前日期是节假日
			continue;  // 跳过该任务

		//获取上一个交易日的日期
		uint32_t preTDate = TimeUtils::getNextDate(_cur_date, -1);  // 获取上一个日期（交易日日期减1天）
		bool bHasHoliday = false;  // 是否有节假日标志
		uint32_t days = 1;  // 连续节假日天数
		while (_base_data_mgr->isHoliday(tInfo->_trdtpl, preTDate, true))  // 如果上一个日期也是节假日
		{
			bHasHoliday = true;  // 设置节假日标志
			preTDate = TimeUtils::getNextDate(preTDate, -1);  // 继续往前查找交易日
			days++;  // 连续节假日天数加1
		}
		uint32_t preWD = TimeUtils::getWeekDay(preTDate);  // 计算上一个交易日的星期几

		WTSSessionInfo* sInfo = get_session_info(tInfo->_session, false);  // 获取交易会话信息

		bool bIgnore = true;  // 是否忽略该任务（默认忽略）
		switch (tInfo->_period)  // 根据任务周期类型判断
		{
		case TPT_Daily:  // 每日周期
			bIgnore = false;  // 不忽略，直接触发
			break;
		case TPT_Minute:  // 分钟周期
			{
				uint32_t minutes = sInfo->timeToMinutes(curTime);	//先将时间转换成分钟数（从0开始的交易分钟数）
				if(minutes != 0 && (minutes%tInfo->_time == 0))		//如果分钟数能被整除,且不为0,则可以触发
				{
					bIgnore = false;  // 不忽略，触发任务
				}
			}
			break;
		case TPT_Monthly:  // 每月周期
			//if (preTDate % 1000000 < _task->_day && _cur_date % 1000000 >= _task->_day)
			//	fired = true;
			if (_cur_date % 1000000 == tInfo->_day)  // 如果当前日期的日部分等于任务的日期（MMDD格式）
				bIgnore = false;  // 不忽略，触发任务
			else if (bHasHoliday)  // 如果有节假日
			{
				//上一个交易日在上个月,且当前日期大于触发日期
				//说明这个月的开始日期在节假日内,顺延到今天
				if ((preTDate % 10000 / 100 < _cur_date % 10000 / 100) && _cur_date % 1000000 > tInfo->_day)  // 如果跨月且当前日期大于触发日期
				{
					bIgnore = false;  // 不忽略，触发任务（顺延触发）
				}
				else if (preTDate % 1000000 < tInfo->_day && _cur_date % 1000000 > tInfo->_day)  // 如果上一个交易日小于触发日期且当前日期大于触发日期
				{
					//上一个交易日在同一个月,且小于触发日期,但是今天大于触发日期,说明正确触发日期到节假日内,顺延到今天
					bIgnore = false;  // 不忽略，触发任务（顺延触发）
				}
			}
			break;
		case TPT_Weekly:  // 每周周期
			//if (preWD < _task->_day && weekDay >= _task->_day)
			//	fired = true;
			if (weekDay == tInfo->_day)  // 如果当前星期等于任务的星期
				bIgnore = false;  // 不忽略，触发任务
			else if (bHasHoliday)  // 如果有节假日
			{
				if (days >= 7 && weekDay > tInfo->_day)  // 如果连续节假日超过7天且当前星期大于触发星期
				{
					bIgnore = false;  // 不忽略，触发任务（顺延触发）
				}
				else if (preWD > weekDay && weekDay > tInfo->_day)  // 如果上一个交易日星期大于当前星期且当前星期大于触发星期（跨周）
				{
					//上一个交易日的星期大于今天的星期,说明换了一周了
					bIgnore = false;  // 不忽略，触发任务（顺延触发）
				}
				else if (preWD < tInfo->_day && weekDay > tInfo->_day)  // 如果上一个交易日星期小于触发星期且当前星期大于触发星期
				{
					bIgnore = false;  // 不忽略，触发任务（顺延触发）
				}
			}
			break;
		case TPT_Yearly:  // 每年周期
			if (preTDate % 10000 < tInfo->_day && _cur_date % 10000 >= tInfo->_day)  // 如果跨年且当前日期大于等于触发日期（MMDD格式）
				bIgnore = false;  // 不忽略，触发任务
			break;
		}

		if (bIgnore)  // 如果忽略该任务
			continue;  // 跳过该任务

		//TODO: 回调任务
		SelContextPtr ctx = getContext(tInfo->_id);  // 获取策略上下文
		StdThreadPtr thrd(new StdThread([ctx, curDate, curTime, nextTime](){  // 创建独立线程异步执行
			if (ctx)  // 如果策略上下文有效
				ctx->on_schedule(curDate, curTime, nextTime);  // 调用策略的调度回调方法
		}));	

		tInfo->_last_exe_time = now;  // 更新上次执行时间，防止重复执行
	}
}

/**
 * @brief 运行引擎
 * 
 * 启动选股引擎，执行以下操作：
 * 1. 从配置中获取产品配置和会话ID
 * 2. 创建并初始化实时行情时钟
 * 3. 生成策略和通道标记文件（marker.json）
 * 4. 启动实时行情时钟
 */
void WtSelEngine::run()
{
	WTSVariant* cfgProd = _cfg->get("product");  // 获取产品配置节点
	_tm_ticker = new WtSelRtTicker(this);  // 创建实时行情时钟对象
	_tm_ticker->init(_data_mgr->reader(), cfgProd->getCString("session"));  // 初始化时间时钟，传入数据读取器和会话ID

	//启动之前,先把运行中的策略落地
	{
		rj::Document root(rj::kObjectType);  // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器引用

		rj::Value jStraList(rj::kArrayType);  // 创建策略列表JSON数组
		for (auto& m : _ctx_map)  // 遍历所有策略上下文
		{
			const SelContextPtr& ctx = m.second;  // 获取策略上下文常量引用
			jStraList.PushBack(rj::Value(ctx->name(), allocator), allocator);  // 将策略名称添加到JSON数组
		}

		root.AddMember("marks", jStraList, allocator);  // 将策略列表添加到根对象，键名为"marks"

		rj::Value jChnlList(rj::kArrayType);  // 创建通道列表JSON数组
		for (auto& m : _adapter_mgr->getAdapters())  // 遍历所有交易适配器
		{
			const TraderAdapterPtr& adapter = m.second;  // 获取交易适配器常量引用
			jChnlList.PushBack(rj::Value(adapter->id(), allocator), allocator);  // 将适配器ID添加到JSON数组
		}

		root.AddMember("channels", jChnlList, allocator);  // 将通道列表添加到根对象，键名为"channels"

		root.AddMember("engine", rj::Value("SEL", allocator), allocator);  // 添加引擎类型标记，值为"SEL"

		std::string filename = WtHelper::getBaseDir();  // 获取基础目录路径
		filename += "marker.json";  // 拼接标记文件名

		rj::StringBuffer sb;  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);  // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将JSON内容写入文件
	}

	_tm_ticker->run();  // 启动实时行情时钟
}

/**
 * @brief 初始化引擎
 * @param cfg 配置对象指针
 * @param bdMgr 基础数据管理器指针
 * @param dataMgr 数据管理器指针
 * @param hotMgr 热点合约管理器指针
 * @param notifier 事件通知器指针
 * 
 * 初始化选股引擎，调用基类初始化方法，并保存配置对象引用。
 */
void WtSelEngine::init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier /* = NULL */)
{
	WtEngine::init(cfg, bdMgr, dataMgr, hotMgr, notifier);  // 调用基类初始化方法

	_cfg = cfg;  // 保存配置对象指针
	_cfg->retain();  // 增加配置对象引用计数
}

/**
 * @brief 添加策略上下文
 * @param ctx 策略上下文智能指针
 * @param date 日期字段（根据周期类型有不同的含义）
 * @param time 执行时间（HHMM格式）
 * @param period 任务周期类型
 * @param bStrict 是否严格时间模式，默认为true
 * @param trdtpl 交易日模板名称，默认为"CHINA"
 * @param sessionID 交易时间模板名称，默认为"TRADING"
 * 
 * 将策略上下文添加到引擎的管理列表中，并创建对应的定时任务。
 * 如果策略ID已存在，记录错误日志并返回。
 */
void WtSelEngine::addContext(SelContextPtr ctx, uint32_t date, uint32_t time, TaskPeriodType period, bool bStrict /* = true */, const char* trdtpl /* = "CHINA" */, const char* sessionID/* ="TRADING" */)
{
	if (ctx == NULL)  // 如果策略上下文指针无效
		return;  // 直接返回

	auto it = _tasks.find(ctx->id());  // 在任务映射表中查找策略ID
	if(it != _tasks.end())  // 如果找到（任务已存在）
	{
		WTSLogger::error("Task registration failed: task id {} already registered", ctx->id());  // 记录错误日志：任务ID已注册
		return;  // 直接返回
	}

	TaskInfoPtr tInfo(new TaskInfo);  // 创建任务信息对象
	wt_strcpy(tInfo->_name, ctx->name());  // 复制策略名称
	wt_strcpy(tInfo->_trdtpl, trdtpl);  // 复制交易日模板名称
	wt_strcpy(tInfo->_session, sessionID);  // 复制交易时间模板名称
	tInfo->_day = date;  // 设置日期字段
	tInfo->_time = time;  // 设置执行时间
	tInfo->_period = period;  // 设置任务周期类型
	tInfo->_strict_time = bStrict;  // 设置是否严格时间模式
	tInfo->_id = makeTaskId();  // 生成任务ID

	_tasks[ctx->id()] = tInfo;  // 将任务信息存入任务映射表，以策略ID为键

	uint32_t sid = ctx->id();  // 获取策略ID
	_ctx_map[sid] = ctx;  // 将策略上下文存入策略上下文映射表
}

/**
 * @brief 获取策略上下文
 * @param id 策略ID
 * @return SelContextPtr 返回策略上下文智能指针，不存在返回空指针
 * 
 * 根据策略ID查找并返回对应的策略上下文。
 */
SelContextPtr WtSelEngine::getContext(uint32_t id)
{
	auto it = _ctx_map.find(id);  // 在策略上下文映射表中查找策略ID
	if (it == _ctx_map.end())  // 如果未找到
		return SelContextPtr();  // 返回空指针

	return it->second;  // 返回策略上下文智能指针
}

/**
 * @brief 处理仓位变化
 * @param straName 策略名称
 * @param stdCode 标准合约代码
 * @param diffQty 仓位变化数量（正数表示增加，负数表示减少）
 * 
 * 处理策略的仓位变化信号，执行以下操作：
 * 1. 检查策略过滤器，如果被过滤则忽略（增量模式下直接忽略）
 * 2. 处理热点合约转换（如果有规则标签，转换为实际合约代码）
 * 3. 应用风险控制（如果有风险倍数，按倍数缩放仓位变化）
 * 4. 更新目标仓位并保存到信号文件
 * 5. 根据策略的路由配置，转发给对应的执行器执行
 */
void WtSelEngine::handle_pos_change(const char* straName, const char* stdCode, double diffQty)
{
	//这里是持仓增量,所以不用处理未过滤的情况,因为增量情况下,不会改变目标diffQty
	if (_filter_mgr.is_filtered_by_strategy(straName, diffQty, true))  // 检查策略过滤器，增量模式（第三个参数为true）
	{
		//输出日志
		WTSLogger::info("[Filters] Target position of {} of strategy {} ignored by strategy filter", stdCode, straName);  // 记录过滤日志
		return;  // 被过滤，直接返回
	}

	std::string realCode = stdCode;  // 初始化实际合约代码为原始代码
	//const char* ruleTag = _hot_mgr->getRuleTag(stdCode);
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码，提取品种信息和规则标签
	if (strlen(cInfo._ruletag) > 0)  // 如果存在规则标签（如主力合约、次主力合约等）
	{
		std::string code = _hot_mgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);  // 根据规则标签获取实际合约代码
		realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码
	}
	//else if (CodeHelper::isStdFutHotCode(stdCode))
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);
	//	std::string code = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, _cur_tdate);
	//	realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);
	//}
	//else if (CodeHelper::isStdFut2ndCode(stdCode))
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);
	//	std::string code = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, _cur_tdate);
	//	realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);
	//}

	PosInfoPtr& pInfo = _pos_map[realCode];  // 在仓位映射表中获取或创建仓位信息引用
	if (pInfo == NULL)  // 如果仓位信息不存在
		pInfo.reset(new PosInfo);  // 创建新的仓位信息对象
	bool bRiskEnabled = false;  // 风险控制是否启用标志
	if (!decimal::eq(_risk_volscale, 1.0) && _risk_date == _cur_tdate)  // 如果风险倍数不等于1.0且风险日期等于当前交易日
	{
		WTSLogger::log_by_cat("risk", LL_INFO, "Risk scale of portfolio is {:.2f}", _risk_volscale);  // 记录风险倍数日志
		bRiskEnabled = true;  // 设置风险控制启用标志
	}
	if (bRiskEnabled && diffQty != 0)  // 如果风险控制启用且仓位变化不为0
	{
		double symbol = diffQty / abs(diffQty);  // 获取仓位变化的符号（+1或-1）
		diffQty = decimal::rnd(abs(diffQty)*_risk_volscale)*symbol;  // 按风险倍数缩放仓位变化并四舍五入
	}
	double targetPos = pInfo->_volume + diffQty;  // 计算目标仓位（当前仓位+仓位变化）

	append_signal(realCode.c_str(), targetPos, false);  // 将信号追加到信号文件（false表示非增量模式）
	save_datas();  // 保存数据到文件

	const auto& exec_ids = _exec_mgr.get_route(straName);  // 获取策略的路由配置（执行器ID列表）
	for (auto& execid : exec_ids)  // 遍历执行器ID列表
		_exec_mgr.handle_pos_change(realCode.c_str(), targetPos, diffQty, execid.c_str());  // 转发仓位变化给对应的执行器
}

/**
 * @brief 获取商品信息
 * @param stdCode 标准合约代码
 * @return WTSCommodityInfo* 返回商品信息指针
 * 
 * 根据标准合约代码获取对应的商品信息，包括交易规则、手续费等。
 */
WTSCommodityInfo* WtSelEngine::get_comm_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码，提取品种信息
	return _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 从基础数据管理器获取商品信息
}

/**
 * @brief 获取交易会话信息
 * @param stdCode 标准合约代码
 * @return WTSSessionInfo* 返回交易会话信息指针
 * 
 * 根据标准合约代码获取对应的交易会话信息，包括交易时间段、开盘时间等。
 */
WTSSessionInfo* WtSelEngine::get_sess_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码，提取品种信息
	WTSCommodityInfo* cInfo = _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 从基础数据管理器获取商品信息
	if (cInfo == NULL)  // 如果商品信息不存在
		return NULL;  // 返回空指针

	return cInfo->getSessionInfo();  // 返回交易会话信息
}

/**
 * @brief 获取实时时间
 * @return uint64_t 返回当前实时时间戳（纳秒级）
 * 
 * 根据引擎的当前日期、时间和秒数构建实时时间戳。
 */
uint64_t WtSelEngine::get_real_time()
{
	return TimeUtils::makeTime(_cur_date, _cur_raw_time * 100000 + _cur_secs);  // 构建时间戳：日期 + 时间（分钟转换为微秒）+ 秒数
}
