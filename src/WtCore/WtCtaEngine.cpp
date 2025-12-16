/*!
 * \file WtCtaEngine.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略引擎实现文件
 *
 * 文件设计逻辑和作用总结：
 * ========================
 * 本文件实现了 WtCtaEngine 类的所有功能，是CTA策略引擎的核心实现。
 *
 * 主要实现内容：
 * 1. 策略管理：管理多个CTA策略上下文，每个策略上下文对应一个策略实例
 * 2. 行情处理：接收和处理实时行情数据，包括Tick数据和K线数据
 * 3. 策略调度：定时调用策略的on_schedule方法，触发策略逻辑执行
 * 4. 持仓管理：收集策略的目标仓位，并通过执行器管理器分配给执行器执行
 * 5. 执行器管理：管理执行器，负责将策略的目标仓位转换为实际交易
 * 6. 数据复权：支持股票的前复权、后复权处理
 * 7. 事件通知：提供图表标记、指标、成交等事件通知功能
 * 8. 风险控制：支持仓位缩放和风险过滤
 *
 * 关键设计点：
 * - 继承自WtEngine，复用引擎的基础功能（行情订阅、数据管理等）
 * - 实现了IExecuterStub接口，为执行器提供数据访问接口
 * - 支持线程池并发处理策略逻辑，提高性能
 * - 支持策略路由，可以将不同策略的信号路由到不同的执行器
 * - 支持复权处理，可以为策略提供前复权或后复权的行情数据
 */
#define WIN32_LEAN_AND_MEAN

#include "WtCtaEngine.h"
#include "WtDtMgr.h"
#include "WtCtaTicker.h"
#include "WtHelper.h"
#include "TraderAdapter.h"
#include "EventNotifier.h"

#include "../Share/CodeHelper.hpp"
#include "../Includes/WTSVariant.hpp"
#include "../Share/TimeUtils.hpp"
#include "../Includes/IBaseDataMgr.h"
#include "../Includes/IHotMgr.h"
#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSRiskDef.hpp"
#include "../Share/decimal.h"

#include "../WTSTools/WTSLogger.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
namespace rj = rapidjson;

/**
 * @brief 构造函数
 * 
 * 初始化CTA引擎，设置实时时钟对象为空。
 */
WtCtaEngine::WtCtaEngine()
	: _tm_ticker(NULL)											// 初始化实时时钟对象为空
{
	
}


/**
 * @brief 析构函数
 * 
 * 清理资源，释放实时时钟对象和配置对象。
 */
WtCtaEngine::~WtCtaEngine()
{
	if (_tm_ticker)											// 如果实时时钟对象存在
	{
		delete _tm_ticker;										// 释放实时时钟对象
		_tm_ticker = NULL;										// 设置为空
	}

	if (_cfg)													// 如果配置对象存在
		_cfg->release();										// 减少引用计数
}

/**
 * @brief 启动引擎
 * 
 * 启动CTA引擎：
 * 1. 创建实时时钟对象（WtCtaRtTicker）
 * 2. 初始化实时时钟对象（从配置中读取交易时段）
 * 3. 保存运行中的策略信息到marker.json文件（包含策略列表、交易通道列表、执行器列表）
 * 4. 启动实时时钟对象
 * 5. 如果配置了风险监控，启动风险监控
 */
void WtCtaEngine::run()
{
	_tm_ticker = new WtCtaRtTicker(this);									// 创建实时时钟对象
	WTSVariant* cfgProd = _cfg->get("product");								// 获取产品配置
	_tm_ticker->init(_data_mgr->reader(), cfgProd->getCString("session"));	// 初始化实时时钟对象（传入数据读取器和交易时段配置）

	// 启动之前，先把运行中的策略落地
	{
		rj::Document root(rj::kObjectType);								// 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();		// 获取JSON分配器

		rj::Value jStraList(rj::kArrayType);									// 创建策略列表数组
		for (auto& m : _ctx_map)											// 遍历所有策略上下文
		{
			const CtaContextPtr& ctx = m.second;							// 获取策略上下文
			jStraList.PushBack(rj::Value(ctx->name(), allocator), allocator);	// 添加策略名称到数组
		}

		root.AddMember("marks", jStraList, allocator);						// 添加策略列表到JSON对象

		rj::Value jChnlList(rj::kArrayType);								// 创建交易通道列表数组
		for (auto& m : _adapter_mgr->getAdapters())							// 遍历所有交易适配器
		{
			const TraderAdapterPtr& adapter = m.second;						// 获取交易适配器
			jChnlList.PushBack(rj::Value(adapter->id(), allocator), allocator);	// 添加交易通道ID到数组
		}

		root.AddMember("channels", jChnlList, allocator);					// 添加交易通道列表到JSON对象

		rj::Value jExecList(rj::kArrayType);								// 创建执行器列表数组
		_exec_mgr.enum_executer([&jExecList, &allocator](ExecCmdPtr executer) {	// 枚举所有执行器
			if(executer)													// 如果执行器存在
				jExecList.PushBack(rj::Value(executer->name(), allocator), allocator);	// 添加执行器名称到数组
		});

		root.AddMember("executers", jExecList, allocator);					// 添加执行器列表到JSON对象

		root.AddMember("engine", rj::Value("CTA", allocator), allocator);	// 添加引擎类型标识

		std::string filename = WtHelper::getBaseDir();						// 获取基础目录
		filename += "marker.json";											// 拼接文件路径

		rj::StringBuffer sb;												// 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);						// 创建JSON格式化写入器
		root.Accept(writer);												// 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());		// 写入文件
	}

	_tm_ticker->run();														// 启动实时时钟对象

	if (_risk_mon)															// 如果配置了风险监控
		_risk_mon->self()->run();											// 启动风险监控

}

/**
 * @brief 初始化引擎
 * @param cfg 配置参数对象
 * @param bdMgr 基础数据管理器
 * @param dataMgr 数据管理器
 * @param hotMgr 主力合约管理器
 * @param notifier 事件通知器，可为NULL
 * 
 * 初始化CTA引擎：
 * 1. 调用基类初始化方法
 * 2. 保存配置对象
 * 3. 设置过滤器管理器
 * 4. 创建线程池（如果配置了线程池大小）
 */
void WtCtaEngine::init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier /* = NULL */)
{
	WtEngine::init(cfg, bdMgr, dataMgr, hotMgr, notifier);		// 调用基类初始化方法

	_cfg = cfg;													// 保存配置对象
	_cfg->retain();												// 增加引用计数

	_exec_mgr.set_filter_mgr(&_filter_mgr);						// 设置执行器管理器的过滤器管理器

	uint32_t poolsize = cfg->getUInt32("poolsize");				// 读取线程池大小
	if (poolsize > 0)											// 如果线程池大小大于0
	{
		_pool.reset(new boost::threadpool::pool(poolsize));		// 创建线程池
	}
	WTSLogger::info("Engine task poolsize is {}", poolsize);	// 记录线程池大小日志
}

/**
 * @brief 添加策略上下文
 * @param ctx 策略上下文指针
 * 
 * 将策略上下文添加到引擎中管理。
 */
void WtCtaEngine::addContext(CtaContextPtr ctx)
{
	uint32_t sid = ctx->id();									// 获取策略ID
	_ctx_map[sid] = ctx;										// 保存到策略上下文映射表
}

/**
 * @brief 获取策略上下文
 * @param id 策略ID
 * @return 策略上下文指针，不存在返回空指针
 * 
 * 根据策略ID获取策略上下文。
 */
CtaContextPtr WtCtaEngine::getContext(uint32_t id)
{
	auto it = _ctx_map.find(id);								// 查找策略上下文
	if (it == _ctx_map.end())									// 如果不存在
		return CtaContextPtr();									// 返回空指针

	return it->second;											// 返回策略上下文
}

/**
 * @brief 引擎初始化回调
 * 
 * 引擎初始化时的处理：
 * 1. 清空执行器管理器的缓存目标持仓
 * 2. 遍历所有策略上下文，调用on_init方法
 * 3. 枚举每个策略的目标持仓，应用策略过滤器
 * 4. 处理主力合约代码转换（如果配置了自定义规则）
 * 5. 将目标持仓添加到执行器管理器的缓存中
 * 6. 应用风险缩放（如果启用了风险控制）
 * 7. 提交缓存的目标持仓到执行器管理器
 * 8. 触发初始化事件通知
 */
void WtCtaEngine::on_init()
{
	//wt_hashmap<std::string, double> target_pos;							// 原代码：目标持仓映射表（已注释）
	_exec_mgr.clear_cached_targets();										// 清空执行器管理器的缓存目标持仓
	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)			// 遍历所有策略上下文
	{
		CtaContextPtr& ctx = (CtaContextPtr&)it->second;					// 获取策略上下文
		ctx->on_init();														// 调用策略的初始化方法

		const auto& exec_ids = _exec_mgr.get_route(ctx->name());			// 获取策略绑定的执行器ID列表

		ctx->enum_position([this, ctx, exec_ids](const char* stdCode, double qty){	// 枚举策略的目标持仓

			double oldQty = qty;												// 保存原始数量
			bool bFilterd = _filter_mgr.is_filtered_by_strategy(ctx->name(), qty);	// 检查策略过滤器是否过滤该持仓
			if (!bFilterd)														// 如果未被过滤
			{
				if (!decimal::eq(qty, oldQty))									// 如果数量被过滤器修改
				{
					// 输出日志
					WTSLogger::info("[Filters] Target position of {} of strategy {} reset by strategy filter: {} -> {}", 
						stdCode, ctx->name(), oldQty, qty);						// 记录过滤器修改日志
				}

				std::string realCode = stdCode;									// 复制合约代码
				CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);	// 解析标准合约代码
				if(strlen(cInfo._ruletag) > 0)									// 如果配置了自定义规则标签
				{
					std::string code = _hot_mgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);	// 获取自定义规则对应的实际合约代码
					realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);	// 转换为标准代码
				}

				for(auto& execid : exec_ids)									// 遍历策略绑定的执行器ID
					_exec_mgr.add_target_to_cache(realCode.c_str(), qty, execid.c_str());	// 将目标持仓添加到执行器管理器的缓存中
			}
			else																// 如果被过滤
			{
				// 输出日志
				WTSLogger::info("[Filters] Target position of {} of strategy {} ignored by strategy filter", stdCode, ctx->name());	// 记录过滤日志
			}
		}, true);																// true表示只枚举有效持仓
	}

	bool bRiskEnabled = false;												// 风险缩放是否启用标志
	if (!decimal::eq(_risk_volscale, 1.0) && _risk_date == _cur_tdate)		// 如果风险缩放不为1.0且风险日期等于当前交易日
	{
		WTSLogger::log_by_cat("risk", LL_INFO, "Risk scale of portfolio is {:.2f}", _risk_volscale);	// 记录风险缩放日志
		bRiskEnabled = true;													// 启用风险缩放
	}

	////初始化仓位打印出来
	//for (auto it = target_pos.begin(); it != target_pos.end(); it++)		// 原代码：遍历目标持仓（已注释）
	//{
	//	const auto& stdCode = it->first;
	//	double& pos = (double&)it->second;

	//	if (bRiskEnabled && !decimal::eq(pos, 0))
	//	{
	//		double symbol = pos / abs(pos);
	//		pos = decimal::rnd(abs(pos)*_risk_volscale)*symbol;
	//	}

	//	WTSLogger::info("Portfolio initial position of {} is {}", stdCode.c_str(), pos);
	//}

	_exec_mgr.commit_cached_targets(bRiskEnabled?_risk_volscale:1.0);		// 提交缓存的目标持仓到执行器管理器（应用风险缩放）

	if (_evt_listener)														// 如果事件监听器存在
		_evt_listener->on_initialize_event();									// 触发初始化事件通知
}

/**
 * @brief 交易日开始回调
 * 
 * 交易日开始时的处理：
 * 1. 记录交易日开始日志
 * 2. 遍历所有策略上下文，调用on_session_begin方法
 * 3. 触发交易日开始事件通知
 * 4. 设置引擎就绪状态为true
 */
void WtCtaEngine::on_session_begin()
{
	WTSLogger::info("Trading day {} begun", _cur_tdate);						// 记录交易日开始日志
	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)			// 遍历所有策略上下文
	{
		CtaContextPtr& ctx = (CtaContextPtr&)it->second;					// 获取策略上下文
		ctx->on_session_begin(_cur_tdate);									// 调用策略的交易日开始方法
	}

	if (_evt_listener)														// 如果事件监听器存在
		_evt_listener->on_session_event(_cur_tdate, true);					// 触发交易日开始事件通知（true表示开始）

	_ready = true;															// 设置引擎就绪状态为true
}

/**
 * @brief 交易日结束回调
 * 
 * 交易日结束时的处理：
 * 1. 调用基类的交易日结束方法
 * 2. 遍历所有策略上下文，调用on_session_end方法
 * 3. 记录交易日结束日志
 * 4. 触发交易日结束事件通知
 */
void WtCtaEngine::on_session_end()
{
	WtEngine::on_session_end();												// 调用基类的交易日结束方法

	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)			// 遍历所有策略上下文
	{
		CtaContextPtr& ctx = (CtaContextPtr&)it->second;					// 获取策略上下文
		ctx->on_session_end(_cur_tdate);									// 调用策略的交易日结束方法
	}

	WTSLogger::info("Trading day {} ended", _cur_tdate);					// 记录交易日结束日志
	if (_evt_listener)														// 如果事件监听器存在
		_evt_listener->on_session_event(_cur_tdate, false);					// 触发交易日结束事件通知（false表示结束）
}

/**
 * @brief 定时调度回调
 * @param curDate 当前日期（YYYYMMDD格式）
 * @param curTime 当前时间（HHMMSS格式）
 * 
 * 定时调度时的处理（每秒调用一次）：
 * 1. 重新加载过滤器配置
 * 2. 清空执行器管理器的缓存目标持仓
 * 3. 调用所有策略的on_schedule方法（支持线程池并发）
 * 4. 收集所有策略的目标持仓，应用策略过滤器
 * 5. 处理主力合约代码转换（如果配置了自定义规则）
 * 6. 汇总各合约的目标持仓（多个策略的持仓累加）
 * 7. 应用风险缩放（如果启用了风险控制）
 * 8. 更新组合持仓数据
 * 9. 清理不在目标持仓中的合约（设置为0）
 * 10. 提交缓存的目标持仓到执行器管理器
 * 11. 保存数据
 * 12. 刷新资金账户和浮动盈亏
 * 13. 触发调度事件通知
 */
void WtCtaEngine::on_schedule(uint32_t curDate, uint32_t curTime)
{
	// 去检查一下过滤器
	_filter_mgr.load_filters();												// 重新加载过滤器配置
	_exec_mgr.clear_cached_targets();										// 清空执行器管理器的缓存目标持仓
	wt_hashmap<std::string, double> target_pos;								// 目标持仓映射表（合约代码 -> 持仓数量）
	if(_pool)																// 如果配置了线程池
	{
		/*
		 *	By Wesley @ 2023.06.27
		 *	如果通过线程池并发
		 *	先并发所有的on_schedule
		 *	然后再wait所有任务结束
		 *	最后再统一读取全部持仓
		 */
		for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)		// 遍历所有策略上下文
		{
			CtaContextPtr& ctx = (CtaContextPtr&)it->second;				// 获取策略上下文
			_pool->schedule([ctx, curDate, curTime] (){						// 在线程池中调度任务
				ctx->on_schedule(curDate, curTime);							// 调用策略的定时调度方法
			});
		}

		/*
		 *	By Wesley @ 2023.06.27
		 *	等待全部on_schedule执行完成
		 */
		_pool->wait();														// 等待所有任务完成
		
		for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)		// 遍历所有策略上下文
		{
			CtaContextPtr& ctx = (CtaContextPtr&)it->second;				// 获取策略上下文
			const auto& exec_ids = _exec_mgr.get_route(ctx->name());		// 获取策略绑定的执行器ID列表
			ctx->enum_position([this, ctx, exec_ids, &target_pos](const char* stdCode, double qty) {	// 枚举策略的目标持仓

				double oldQty = qty;											// 保存原始数量
				bool bFilterd = _filter_mgr.is_filtered_by_strategy(ctx->name(), qty);	// 检查策略过滤器是否过滤该持仓
				if (!bFilterd)												// 如果未被过滤
				{
					if (!decimal::eq(qty, oldQty))							// 如果数量被过滤器修改
					{
						// 输出日志
						WTSLogger::info("[Filters] Target position of {} of strategy {} reset by strategy filter: {} -> {}",
							stdCode, ctx->name(), oldQty, qty);					// 记录过滤器修改日志
					}

					std::string realCode = stdCode;							// 复制合约代码
					CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);	// 解析标准合约代码
					if (strlen(cInfo._ruletag) > 0)							// 如果配置了自定义规则标签
					{
						std::string code = _hot_mgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);	// 获取自定义规则对应的实际合约代码
						realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);	// 转换为标准代码
					}

					double& vol = target_pos[realCode];						// 获取或创建目标持仓
					vol += qty;												// 累加持仓（多个策略的持仓累加）
					for (auto& execid : exec_ids)							// 遍历策略绑定的执行器ID
						_exec_mgr.add_target_to_cache(realCode.c_str(), qty, execid.c_str());	// 将目标持仓添加到执行器管理器的缓存中
				}
				else														// 如果被过滤
				{
					// 输出日志
					WTSLogger::info("[Filters] Target position of {} of strategy {} ignored by strategy filter", stdCode, ctx->name());	// 记录过滤日志
				}
			}, true);														// true表示只枚举有效持仓
		}
	}
	else																	// 如果没有配置线程池
	{
		for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)		// 遍历所有策略上下文
		{
			CtaContextPtr& ctx = (CtaContextPtr&)it->second;				// 获取策略上下文
			ctx->on_schedule(curDate, curTime);								// 调用策略的定时调度方法
			const auto& exec_ids = _exec_mgr.get_route(ctx->name());		// 获取策略绑定的执行器ID列表
			ctx->enum_position([this, ctx, exec_ids, &target_pos](const char* stdCode, double qty) {	// 枚举策略的目标持仓

				double oldQty = qty;											// 保存原始数量
				bool bFilterd = _filter_mgr.is_filtered_by_strategy(ctx->name(), qty);	// 检查策略过滤器是否过滤该持仓
				if (!bFilterd)												// 如果未被过滤
				{
					if (!decimal::eq(qty, oldQty))							// 如果数量被过滤器修改
					{
						// 输出日志
						WTSLogger::info("[Filters] Target position of {} of strategy {} reset by strategy filter: {} -> {}",
							stdCode, ctx->name(), oldQty, qty);					// 记录过滤器修改日志
					}

					std::string realCode = stdCode;							// 复制合约代码
					CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);	// 解析标准合约代码
					if (strlen(cInfo._ruletag) > 0)							// 如果配置了自定义规则标签
					{
						std::string code = _hot_mgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);	// 获取自定义规则对应的实际合约代码
						realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);	// 转换为标准代码
					}

					double& vol = target_pos[realCode];						// 获取或创建目标持仓
					vol += qty;												// 累加持仓（多个策略的持仓累加）
					for (auto& execid : exec_ids)							// 遍历策略绑定的执行器ID
						_exec_mgr.add_target_to_cache(realCode.c_str(), qty, execid.c_str());	// 将目标持仓添加到执行器管理器的缓存中
				}
				else														// 如果被过滤
				{ 
					// 输出日志
					WTSLogger::info("[Filters] Target position of {} of strategy {} ignored by strategy filter", stdCode, ctx->name());	// 记录过滤日志
				}
			}, true);														// true表示只枚举有效持仓
		}
	}
	

	bool bRiskEnabled = false;												// 风险缩放是否启用标志
	if(!decimal::eq(_risk_volscale, 1.0) && _risk_date == _cur_tdate)		// 如果风险缩放不为1.0且风险日期等于当前交易日
	{
		WTSLogger::log_by_cat("risk", LL_INFO, "Risk scale of strategy group is {:.2f}", _risk_volscale);	// 记录风险缩放日志
		bRiskEnabled = true;													// 启用风险缩放
	}

	// 处理组合理论部位
	for (auto it = target_pos.begin(); it != target_pos.end(); it++)		// 遍历所有目标持仓
	{
		const auto& stdCode = it->first;										// 获取合约代码
		double& pos = (double&)it->second;									// 获取持仓引用

		if (bRiskEnabled && !decimal::eq(pos, 0))							// 如果启用了风险缩放且持仓不为0
		{
			double symbol = pos / abs(pos);									// 获取持仓方向（正数或负数）
			pos = decimal::rnd(abs(pos)*_risk_volscale)*symbol;				// 应用风险缩放并四舍五入
		}

		append_signal(stdCode.c_str(), pos, true);							// 更新组合持仓信号
	}

	for(auto& m : _pos_map)													// 遍历组合持仓映射表
	{
		const auto& stdCode = m.first;										// 获取合约代码
		if (target_pos.find(stdCode) == target_pos.end())					// 如果合约不在目标持仓中
		{
			if(!decimal::eq(m.second->_volume, 0))							// 如果当前持仓不为0
			{
				// 这里是通知WtEngine去更新组合持仓数据
				append_signal(stdCode.c_str(), 0, true);						// 将持仓设置为0

				WTSLogger::error("Instrument {} not in target positions, setup to 0 automatically", stdCode.c_str());	// 记录错误日志
			}

			// 因为组合持仓里会有过期的合约代码存在，所以这里在丢给执行以前要做一个检查
			auto cInfo = get_contract_info(stdCode.c_str());					// 获取合约信息
			if (cInfo != NULL)												// 如果合约信息存在（说明合约有效）
			{
				//target_pos[stdCode] = 0;									// 原代码：添加到目标持仓（已注释）
				_exec_mgr.add_target_to_cache(stdCode.c_str(), 0);			// 将目标持仓设置为0并添加到缓存
			}
		}
	}

	push_task([this](){														// 推送异步任务
		update_fund_dynprofit();												// 更新浮动盈亏
		/*
		 *	By Wesley @ 2023.01.30
		 *	增加一个定时刷新交易账号资金的入口
		 */
		_adapter_mgr->refresh_funds();										// 刷新所有交易通道的资金账户
	});

	//_exec_mgr.set_positions(target_pos);									// 原代码：设置目标持仓（已注释）
	_exec_mgr.commit_cached_targets(bRiskEnabled ? _risk_volscale : 1);		// 提交缓存的目标持仓到执行器管理器（应用风险缩放）

	save_datas();															// 保存数据

	if (_evt_listener)														// 如果事件监听器存在
		_evt_listener->on_schedule_event(curDate, curTime);					// 触发调度事件通知
}


/**
 * @brief 处理行情推送
 * @param newTick 新的tick数据
 * 
 * 将行情数据推送给实时时钟对象。
 */
void WtCtaEngine::handle_push_quote(WTSTickData* newTick)
{
	if (_tm_ticker)															// 如果实时时钟对象存在
		_tm_ticker->on_tick(newTick);										// 推送tick数据到实时时钟对象
}

/**
 * @brief 处理持仓变化
 * @param straName 策略名称
 * @param stdCode 标准合约代码
 * @param diffPos 持仓变化量（正数表示增加，负数表示减少）
 * 
 * 处理策略的持仓变化通知：
 * 1. 检查策略过滤器是否过滤该持仓变化
 * 2. 处理主力合约代码转换（如果配置了自定义规则）
 * 3. 获取或创建持仓信息对象
 * 4. 应用风险缩放（如果启用了风险控制）
 * 5. 计算目标持仓（当前持仓 + 变化量）
 * 6. 更新组合持仓信号
 * 7. 保存数据
 * 8. 通知执行器管理器处理持仓变化（如果策略绑定了执行器，提交增量；否则提交全量）
 */
void WtCtaEngine::handle_pos_change(const char* straName, const char* stdCode, double diffPos)
{
	// 这里是持仓增量，所以不用处理未过滤的情况，因为增量情况下，不会改变目标diffQty
	if(_filter_mgr.is_filtered_by_strategy(straName, diffPos, true))		// 检查策略过滤器是否过滤该持仓变化（true表示增量模式）
	{
		// 输出日志
		WTSLogger::info("[Filters] Target position of {} of strategy {} ignored by strategy filter", stdCode, straName);	// 记录过滤日志
		return;																// 直接返回，不处理
	}

	std::string realCode = stdCode;											// 复制合约代码
	//const char* ruleTag = _hot_mgr->getRuleTag(stdCode);					// 原代码：获取规则标签（已注释）
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);	// 解析标准合约代码
	if (strlen(cInfo._ruletag) > 0)											// 如果配置了自定义规则标签
	{
		std::string code = _hot_mgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);	// 获取自定义规则对应的实际合约代码
		realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);	// 转换为标准代码
	}

	/*
	 *	这里必须要算一个总的目标仓位
	 */
	PosInfoPtr& pInfo = _pos_map[realCode];									// 获取或创建持仓信息对象
	if (pInfo == NULL)														// 如果持仓信息对象不存在
		pInfo.reset(new PosInfo);											// 创建新的持仓信息对象

	bool bRiskEnabled = false;												// 风险缩放是否启用标志
	if (!decimal::eq(_risk_volscale, 1.0) && _risk_date == _cur_tdate)		// 如果风险缩放不为1.0且风险日期等于当前交易日
	{
		WTSLogger::log_by_cat("risk", LL_INFO, "Risk scale of portfolio is {:.2f}", _risk_volscale);	// 记录风险缩放日志
		bRiskEnabled = true;													// 启用风险缩放
	}

	if (bRiskEnabled && !decimal::eq(diffPos, 0))							// 如果启用了风险缩放且变化量不为0
	{
		double symbol = diffPos / abs(diffPos);								// 获取变化方向（正数或负数）
		diffPos = decimal::rnd(abs(diffPos)*_risk_volscale)*symbol;			// 应用风险缩放并四舍五入
	}

	double targetPos = pInfo->_volume + diffPos;							// 计算目标持仓（当前持仓 + 变化量）

	append_signal(realCode.c_str(), targetPos, false);						// 更新组合持仓信号（false表示增量模式）
	save_datas();															// 保存数据

	/*
	 *	如果策略绑定了执行通道
	 *	那么就只提交增量
	 *	如果策略没有绑定执行通道，就提交全量
	 */
	const auto& exec_ids = _exec_mgr.get_route(straName);					// 获取策略绑定的执行器ID列表
	for(auto& execid : exec_ids)											// 遍历策略绑定的执行器ID
		_exec_mgr.handle_pos_change(realCode.c_str(), targetPos, diffPos, execid.c_str());	// 通知执行器管理器处理持仓变化（提交增量）
}

/**
 * @brief Tick数据推送回调
 * @param stdCode 标准合约代码
 * @param curTick 当前tick数据
 * 
 * 处理tick数据推送：
 * 1. 调用基类的on_tick方法
 * 2. 将tick数据推送给数据管理器
 * 3. 将tick数据推送给执行器管理器（支持未订阅的合约，主要用于主力合约换月期间）
 * 4. 根据订阅模式和复权模式，将tick数据推送给订阅的策略：
 *    - 无复权模式（opt=0）：直接推送原始代码和tick数据
 *    - 前复权模式（opt=1）：推送代码+后缀"-"和原始tick数据
 *    - 后复权模式（opt=2）：推送代码+后缀"+"和复权后的tick数据
 * 5. 如果使用线程池，等待所有任务完成
 */
void WtCtaEngine::on_tick(const char* stdCode, WTSTickData* curTick)
{
	WtEngine::on_tick(stdCode, curTick);									// 调用基类的on_tick方法

	_data_mgr->handle_push_quote(stdCode, curTick);						// 将tick数据推送给数据管理器

	// 如果是真实代码，则要传递给执行器
	/*
	 *	这里不再做判断，直接全部传递给执行器管理器，因为执行器可能会处理未订阅的合约
	 *	主要场景为主力合约换月期间
	 *	By Wesley @ 2021.08.19
	 */
	{
		// 是否主力合约代码的标记，主要用于给执行器发数据的
		_exec_mgr.handle_tick(stdCode, curTick);							// 将tick数据推送给执行器管理器
	}

	/*
	 *	By Wesley @ 2022.02.07
	 *	这里做了一个彻底的调整
	 *	第一，检查订阅标记，如果标记为0，即无复权模式，则直接按照原始代码触发ontick
	 *	第二，如果标记为1，即前复权模式，则将代码转成xxxx-，再触发ontick
	 *	第三，如果标记为2，即后复权模式，则将代码转成xxxx+，再把tick数据做一个修正，再触发ontick
	 */
	if(_ready)																// 如果引擎已就绪
	{
		auto sit = _tick_sub_map.find(stdCode);								// 查找合约的订阅列表
		if (sit == _tick_sub_map.end())									// 如果未订阅，直接返回
			return;

		uint32_t flag = get_adjusting_flag();								// 获取复权调整标志位
		WTSTickData* adjTick = nullptr;									// 复权后的tick数据指针（用于后复权）

		// By Wesley
		// 这里做一个拷贝，虽然有点开销，但是可以规避掉一些问题，比如ontick的时候订阅tick
		SubList sids = sit->second;											// 复制订阅列表（防止在循环中修改导致问题）
		for (auto it = sids.begin(); it != sids.end(); it++)				// 遍历订阅列表
		{
			uint32_t sid = it->first;										// 获取策略ID
				

			auto cit = _ctx_map.find(sid);									// 查找策略上下文
			if (cit != _ctx_map.end())										// 如果策略上下文存在
			{
				CtaContextPtr& ctx = (CtaContextPtr&)cit->second;			// 获取策略上下文
				uint32_t opt = it->second.second;								// 获取订阅选项（0=无复权，1=前复权，2=后复权）
					
				if (opt == 0)												// 如果无复权模式
				{
					/*
					 *	By Wesley @ 2023.06.27
					 *	如果使用线程池，则到线程池里去调度
					 */
					if(_pool)												// 如果配置了线程池
					{
						_pool->schedule([ctx, stdCode, curTick]() {			// 在线程池中调度任务
							ctx->on_tick(stdCode, curTick);					// 调用策略的on_tick方法（原始代码和tick数据）
						});
					}
					else													// 如果没有配置线程池
						ctx->on_tick(stdCode, curTick);						// 直接调用策略的on_tick方法
				}
				else														// 如果复权模式（前复权或后复权）
				{
					std::string wCode = stdCode;							// 复制合约代码
					wCode = fmt::format("{}{}", stdCode, opt == 1 ? SUFFIX_QFQ : SUFFIX_HFQ);	// 添加复权后缀（"-"或"+"）
					if (opt == 1)											// 如果是前复权模式
					{
						if (_pool)											// 如果配置了线程池
						{
							_pool->schedule([ctx, wCode, curTick]() {		// 在线程池中调度任务
								ctx->on_tick(wCode.c_str(), curTick);		// 调用策略的on_tick方法（前复权代码和原始tick数据）
							});
						}
						else												// 如果没有配置线程池
							ctx->on_tick(wCode.c_str(), curTick);			// 直接调用策略的on_tick方法
					}
					else //(opt == 2)										// 如果是后复权模式
					{
						if (adjTick == nullptr)								// 如果复权tick数据未创建
						{
							WTSTickData* adjTick = WTSTickData::create(curTick->getTickStruct());	// 创建复权tick数据（复制原始tick结构）
							WTSTickStruct& adjTS = adjTick->getTickStruct();	// 获取tick结构引用
							adjTick->setContractInfo(curTick->getContractInfo());	// 设置合约信息

							// 这里做一个复权因子的处理
							double factor = get_exright_factor(stdCode);		// 获取复权因子
							adjTS.open *= factor;							// 开盘价复权
							adjTS.high *= factor;							// 最高价复权
							adjTS.low *= factor;								// 最低价复权
							adjTS.price *= factor;							// 最新价复权

							adjTS.settle_price *= factor;					// 结算价复权

							adjTS.pre_close *= factor;						// 昨收价复权
							adjTS.pre_settle *= factor;						// 昨结价复权

							/*
							 *	By Wesley @ 2022.08.15
							 *	这里对tick的复权做一个完善
							 */
							if (flag & 1)									// 如果标志位第1位为1（成交量复权）
							{
								adjTS.total_volume /= factor;				// 总成交量复权（除以因子）
								adjTS.volume /= factor;						// 成交量复权（除以因子）
							}

							if (flag & 2)									// 如果标志位第2位为1（成交额复权）
							{
								adjTS.total_turnover *= factor;				// 总成交额复权（乘以因子）
								adjTS.turn_over *= factor;					// 成交额复权（乘以因子）
							}

							if (flag & 4)									// 如果标志位第3位为1（持仓量复权）
							{
								adjTS.open_interest /= factor;				// 持仓量复权（除以因子）
								adjTS.diff_interest /= factor;				// 持仓变化复权（除以因子）
								adjTS.pre_interest /= factor;				// 昨持仓复权（除以因子）
							}

							_price_map[wCode] = adjTS.price;				// 保存复权后的价格到价格映射表
						}

						if (_pool)											// 如果配置了线程池
						{
							_pool->schedule([ctx, wCode, adjTick]() {		// 在线程池中调度任务
								ctx->on_tick(wCode.c_str(), adjTick);		// 调用策略的on_tick方法（后复权代码和复权tick数据）
							});
						}
						else												// 如果没有配置线程池
							ctx->on_tick(wCode.c_str(), adjTick);			// 直接调用策略的on_tick方法

					}
				}
			}				
		}

		if(nullptr != adjTick)												// 如果复权tick数据已创建
			adjTick->release();												// 释放复权tick数据
		/*
		 *	By Wesley @ 223.06.27
		 *	这里一定要等待线程池全部调度完成
		 */
		if (_pool)															// 如果配置了线程池
			_pool->wait();													// 等待所有任务完成
	}
	
}

/**
 * @brief K线数据推送回调
 * @param stdCode 标准合约代码
 * @param period 周期类型（如"m1"、"m5"、"d"等）
 * @param times 周期倍数
 * @param newBar 新的K线数据
 * 
 * 处理K线数据推送：
 * 1. 构造K线订阅键（格式：合约代码-周期-倍数）
 * 2. 查找订阅该K线的策略列表
 * 3. 将K线数据推送给订阅的策略（支持线程池并发）
 * 4. 如果使用线程池，等待所有任务完成
 * 5. 记录K线关闭日志
 */
void WtCtaEngine::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	thread_local static char key[64] = { 0 };								// 线程局部静态缓冲区（用于格式化K线键）
	fmtutil::format_to(key, "{}-{}-{}", stdCode, period, times);			// 格式化K线键（格式：合约代码-周期-倍数）

	const SubList& sids = _bar_sub_map[key];								// 查找该K线的订阅列表
	for (auto it = sids.begin(); it != sids.end(); it++)					// 遍历订阅列表
	{
		uint32_t sid = it->first;											// 获取策略ID
		auto cit = _ctx_map.find(sid);										// 查找策略上下文
		if(cit != _ctx_map.end())											// 如果策略上下文存在
		{
			CtaContextPtr& ctx = (CtaContextPtr&)cit->second;				// 获取策略上下文
			if (_pool)														// 如果配置了线程池
			{
				_pool->schedule([ctx, stdCode, period, times, newBar]() {	// 在线程池中调度任务
					ctx->on_bar(stdCode, period, times, newBar);			// 调用策略的on_bar方法
				});
			}
			else															// 如果没有配置线程池
				ctx->on_bar(stdCode, period, times, newBar);				// 直接调用策略的on_bar方法
		}
	}

	/*
	 *	By Wesley @ 223.06.27
	 *	这里一定要等待线程池全部调度完成
	 */
	if (_pool)																// 如果配置了线程池
		_pool->wait();														// 等待所有任务完成

	WTSLogger::info("KBar [{}] @ {} closed", key, period[0] == 'd' ? newBar->date : newBar->time);	// 记录K线关闭日志（日期或时间）
}

/**
 * @brief 判断是否在交易时段内
 * @return true表示在交易时段内，false表示不在交易时段内
 * 
 * 查询实时时钟对象，判断当前是否在交易时段内。
 */
bool WtCtaEngine::isInTrading()
{
	return _tm_ticker->is_in_trading();										// 查询实时时钟对象是否在交易时段内
}

/**
 * @brief 将时间转换为分钟数
 * @param uTime 时间（HHMMSS格式）
 * @return 分钟数（从0点开始计算）
 * 
 * 将时间（HHMMSS格式）转换为从0点开始的分钟数。
 */
uint32_t WtCtaEngine::transTimeToMin(uint32_t uTime)
{
	return _tm_ticker->time_to_mins(uTime);									// 调用实时时钟对象的时间转换方法
}

/**
 * @brief 获取商品信息
 * @param stdCode 标准合约代码
 * @return 商品信息指针，如果不存在则返回NULL
 * 
 * 实现IExecuterStub接口，为执行器提供商品信息查询功能。
 */
WTSCommodityInfo* WtCtaEngine::get_comm_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);	// 解析标准合约代码
	return _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);		// 从基础数据管理器获取商品信息
}

/**
 * @brief 获取交易时段信息
 * @param stdCode 标准合约代码
 * @return 交易时段信息指针，如果不存在则返回NULL
 * 
 * 实现IExecuterStub接口，为执行器提供交易时段信息查询功能。
 */
WTSSessionInfo* WtCtaEngine::get_sess_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);	// 解析标准合约代码
	WTSCommodityInfo* cInfo = _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);	// 从基础数据管理器获取商品信息
	if (cInfo == NULL)														// 如果商品信息不存在
		return NULL;															// 返回NULL

	return cInfo->getSessionInfo();											// 返回交易时段信息
}

/**
 * @brief 获取当前实时时间戳
 * @return 当前时间戳（微秒精度）
 * 
 * 实现IExecuterStub接口，为执行器提供当前时间查询功能。
 */
uint64_t WtCtaEngine::get_real_time()
{
	return TimeUtils::makeTime(_cur_date, _cur_raw_time * 100000 + _cur_secs);	// 根据当前日期、原始时间和秒数构造时间戳
}

/**
 * @brief 通知图表标记
 * @param time 时间戳
 * @param straId 策略ID
 * @param price 价格
 * @param icon 图标标识
 * @param tag 标签
 * 
 * 通知事件通知器添加图表标记。
 */
void WtCtaEngine::notify_chart_marker(uint64_t time, const char* straId, double price, const char* icon, const char* tag)
{
	if (_notifier)															// 如果事件通知器存在
		_notifier->notify_chart_marker(time, straId, price, icon, tag);		// 通知添加图表标记
}

/**
 * @brief 通知图表指标
 * @param time 时间戳
 * @param straId 策略ID
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param val 指标值
 * 
 * 通知事件通知器添加图表指标数据。
 */
void WtCtaEngine::notify_chart_index(uint64_t time, const char* straId, const char* idxName, const char* lineName, double val)
{
	if (_notifier)															// 如果事件通知器存在
		_notifier->notify_chart_index(time, straId, idxName, lineName, val);	// 通知添加图表指标
}

/**
 * @brief 通知成交
 * @param straId 策略ID
 * @param stdCode 标准合约代码
 * @param isLong 是否多头（true=多头，false=空头）
 * @param isOpen 是否开仓（true=开仓，false=平仓）
 * @param curTime 当前时间戳
 * @param price 成交价格
 * @param userTag 用户标签
 * 
 * 通知事件通知器添加成交记录。
 */
void WtCtaEngine::notify_trade(const char* straId, const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, const char* userTag)
{
	if (_notifier)															// 如果事件通知器存在
		_notifier->notify_trade(straId, stdCode, isLong, isOpen, curTime, price, userTag);	// 通知添加成交记录
}