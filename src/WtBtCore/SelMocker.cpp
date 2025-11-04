/*!
* \file SelMocker.cpp
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief 选股策略回测模拟器实现文件
*
* 本文件实现了SelMocker类的所有功能，包括：
* 1. 构造函数和析构函数：初始化回测模拟器，清理资源
* 2. 数据接收处理：处理Tick数据、K线数据、定时调度事件
* 3. 信号处理：接收策略持仓信号，在合适的时机执行
* 4. 持仓管理：执行开仓、加仓、减仓、平仓操作，维护持仓明细
* 5. 盈亏计算：实时计算持仓盈亏、已平仓盈亏、动态盈亏
* 6. 数据输出：输出交易记录、持仓记录、资金曲线等CSV文件，保存策略状态JSON
* 7. 策略接口：实现ISelStraCtx接口，为策略提供数据查询和交易接口
*/
#include "SelMocker.h"           // 选股策略回测模拟器头文件
#include "WtHelper.h"             // WonderTrader辅助工具头文件

#include <exception>               // 异常处理
#include <boost/filesystem.hpp>   // Boost文件系统库，用于创建目录

#include "../Share/StdUtils.hpp"          // 标准工具库
#include "../Share/StrUtil.hpp"           // 字符串工具库
#include "../Share/decimal.h"             // 小数精度计算库
#include "../Includes/WTSContractInfo.hpp"  // 合约信息类
#include "../Includes/WTSSessionInfo.hpp"   // 交易时间模板信息类
#include "../Includes/WTSVariant.hpp"       // 变体类型类，用于配置参数

#include "../WTSTools/WTSLogger.h"        // 日志工具类

#include <rapidjson/document.h>            // RapidJSON文档类
#include <rapidjson/prettywriter.h>        // RapidJSON格式化写入器
namespace rj = rapidjson;                  // RapidJSON命名空间别名

/**
 * @brief 生成选股策略上下文ID
 * @return 上下文唯一标识符
 * 
 * 使用原子操作生成唯一的上下文ID，起始值为3000，每次调用递增1。
 * 用于区分不同的策略实例。
 */
inline uint32_t makeSelCtxId()
{
	static std::atomic<uint32_t> _auto_context_id{ 3000 };  // 静态原子变量，初始值为3000
	return _auto_context_id.fetch_add(1);                     // 原子递增操作，返回递增前的值
}


/**
 * @brief 构造函数
 * @param replayer 历史数据回放器指针
 * @param name 策略名称
 * @param slippage 滑点设置，默认为0
 * @param isRatioSlp 是否比例滑点，默认为false
 * 
 * 初始化选股策略回测模拟器，设置回放器、策略名称和滑点参数。
 * 调用makeSelCtxId生成唯一的上下文ID。
 */
SelMocker::SelMocker(HisDataReplayer* replayer, const char* name, int32_t slippage /* = 0 */, bool isRatioSlp /* = false */)
	: ISelStraCtx(name)              // 调用基类构造函数，传入策略名称
	, _replayer(replayer)            // 初始化历史数据回放器指针
	, _total_calc_time(0)            // 初始化总计算时间为0
	, _emit_times(0)                 // 初始化总计算次数为0
	, _is_in_schedule(false)         // 初始化调度标志为false
	, _ud_modified(false)            // 初始化用户数据修改标志为false
	, _strategy(NULL)                 // 初始化策略指针为NULL
	, _slippage(slippage)             // 初始化滑点设置
	, _ratio_slippage(isRatioSlp)    // 初始化比例滑点标志
	, _schedule_times(0)              // 初始化调度次数为0
{
	_context_id = makeSelCtxId();    // 生成唯一的上下文ID
}


/**
 * @brief 析构函数
 * 
 * 清理选股策略回测模拟器占用的资源。
 * 注意：策略对象和工厂对象的释放由_StraFactInfo的析构函数处理。
 */
SelMocker::~SelMocker()
{
}

/**
 * @brief 输出策略状态数据
 * 
 * 将策略的持仓数据、资金数据、信号数据等保存为JSON格式文件。
 * 用于策略状态恢复和回测结果分析。
 * 文件保存在输出目录下的策略名称子目录中，文件名为"策略名称.json"。
 */
void SelMocker::dump_stradata()
{
	rj::Document root(rj::kObjectType);  // 创建JSON根对象

	{//持仓数据保存
		rj::Value jPos(rj::kArrayType);  // 创建持仓数组

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		// 遍历所有持仓
		for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			const PosInfo& pInfo = it->second;        // 获取持仓信息

			rj::Value pItem(rj::kObjectType);  // 创建持仓对象
			pItem.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码
			pItem.AddMember("volume", pInfo._volume, allocator);                // 添加持仓数量
			pItem.AddMember("closeprofit", pInfo._closeprofit, allocator);      // 添加已平仓盈亏
			pItem.AddMember("dynprofit", pInfo._dynprofit, allocator);          // 添加动态盈亏
			pItem.AddMember("lastentertime", pInfo._last_entertime, allocator); // 添加最后开仓时间
			pItem.AddMember("lastexittime", pInfo._last_exittime, allocator);   // 添加最后平仓时间

			rj::Value details(rj::kArrayType);  // 创建持仓明细数组
			// 遍历持仓明细
			for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)
			{
				const DetailInfo& dInfo = *dit;  // 获取持仓明细信息
				rj::Value dItem(rj::kObjectType);  // 创建明细对象
				dItem.AddMember("long", dInfo._long, allocator);            // 添加是否做多标志
				dItem.AddMember("price", dInfo._price, allocator);          // 添加开仓价格
				dItem.AddMember("maxprice", dInfo._max_price, allocator);  // 添加最高价格
				dItem.AddMember("minprice", dInfo._min_price, allocator);  // 添加最低价格
				dItem.AddMember("volume", dInfo._volume, allocator);       // 添加持仓数量
				dItem.AddMember("opentime", dInfo._opentime, allocator);    // 添加开仓时间
				dItem.AddMember("opentdate", dInfo._opentdate, allocator);  // 添加开仓交易日

				dItem.AddMember("profit", dInfo._profit, allocator);       // 添加当前盈亏
				dItem.AddMember("maxprofit", dInfo._max_profit, allocator); // 添加最大盈利
				dItem.AddMember("maxloss", dInfo._max_loss, allocator);    // 添加最大亏损
				dItem.AddMember("opentag", rj::Value(dInfo._opentag, allocator), allocator);  // 添加开仓标签

				details.PushBack(dItem, allocator);  // 将明细对象添加到明细数组
			}

			pItem.AddMember("details", details, allocator);  // 将明细数组添加到持仓对象

			jPos.PushBack(pItem, allocator);  // 将持仓对象添加到持仓数组
		}

		root.AddMember("positions", jPos, allocator);  // 将持仓数组添加到根对象
	}

	{//资金保存
		rj::Value jFund(rj::kObjectType);  // 创建资金对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		jFund.AddMember("total_profit", _fund_info._total_profit, allocator);      // 添加总已平仓盈亏
		jFund.AddMember("total_dynprofit", _fund_info._total_dynprofit, allocator); // 添加总动态盈亏
		jFund.AddMember("total_fees", _fund_info._total_fees, allocator);         // 添加总手续费
		jFund.AddMember("tdate", _cur_tdate, allocator);                            // 添加当前交易日

		root.AddMember("fund", jFund, allocator);  // 将资金对象添加到根对象
	}

	{//信号保存
		rj::Value jSigs(rj::kObjectType);  // 创建信号对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		// 遍历所有信号
		for (auto& m : _sig_map)
		{
			const char* stdCode = m.first.c_str();  // 获取合约代码
			const SigInfo& sInfo = m.second;        // 获取信号信息

			rj::Value jItem(rj::kObjectType);  // 创建信号项对象
			jItem.AddMember("usertag", rj::Value(sInfo._usertag.c_str(), allocator), allocator);  // 添加用户标签

			jItem.AddMember("volume", sInfo._volume, allocator);      // 添加目标持仓数量
			jItem.AddMember("sigprice", sInfo._sigprice, allocator);  // 添加信号价格
			jItem.AddMember("gentime", sInfo._gentime, allocator);    // 添加信号生成时间

			jSigs.AddMember(rj::Value(stdCode, allocator), jItem, allocator);  // 将信号项添加到信号对象
		}

		root.AddMember("signals", jSigs, allocator);  // 将信号对象添加到根对象
	}

	{
		std::string folder = WtHelper::getOutputDir();  // 获取输出目录
		folder += _name;                                 // 添加策略名称
		folder += "/";                                  // 添加路径分隔符

		std::string filename = folder;                  // 构建文件路径
		filename += _name;                              // 添加策略名称
		filename += ".json";                            // 添加文件扩展名

		rj::StringBuffer sb;                           // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb); // 创建格式化写入器
		root.Accept(writer);                            // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将内容写入文件
	}
}


/**
 * @brief 输出回测结果文件
 * 
 * 将交易日志、平仓日志、资金日志、信号日志、持仓日志等写入CSV文件。
 * 同时保存用户数据到JSON文件。
 * 所有文件保存在输出目录下的策略名称子目录中。
 */
void SelMocker::dump_outputs()
{
	std::string folder = WtHelper::getOutputDir();  // 获取输出目录
	folder += _name;                                 // 添加策略名称
	folder += "/";                                  // 添加路径分隔符
	boost::filesystem::create_directories(folder.c_str());  // 创建目录（如果不存在）

	// 输出交易日志CSV文件
	std::string filename = folder + "trades.csv";
	std::string content = "code,time,direct,action,price,qty,tag,fee\n";  // CSV表头
	content += _trade_logs.str();  // 添加交易日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	// 输出平仓日志CSV文件
	filename = folder + "closes.csv";
	content = "code,direct,opentime,openprice,closetime,closeprice,qty,profit,maxprofit,maxloss,totalprofit,entertag,exittag,openbarno,closebarno\n";  // CSV表头
	content += _close_logs.str();  // 添加平仓日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	// 输出资金日志CSV文件
	filename = folder + "funds.csv";
	content = "date,closeprofit,positionprofit,dynbalance,fee\n";  // CSV表头
	content += _fund_logs.str();  // 添加资金日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	// 输出信号日志CSV文件
	filename = folder + "signals.csv";
	content = "code,target,sigprice,gentime,usertag\n";  // CSV表头
	content += _sig_logs.str();  // 添加信号日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	// 输出持仓日志CSV文件
	filename = folder + "positions.csv";
	content = "date,code,volume,closeprofit,dynprofit\n";  // CSV表头
	if (!_pos_logs.str().empty()) content += _pos_logs.str();  // 如果持仓日志不为空，添加内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	// 输出用户数据JSON文件
	{
		rj::Document root(rj::kObjectType);  // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器
		// 遍历用户数据
		for (auto it = _user_datas.begin(); it != _user_datas.end(); it++)
		{
			root.AddMember(rj::Value(it->first.c_str(), allocator), rj::Value(it->second.c_str(), allocator), allocator);  // 添加用户数据项
		}

		filename = folder;                  // 构建文件路径
		filename += "ud_";                  // 添加前缀
		filename += _name;                  // 添加策略名称
		filename += ".json";                // 添加文件扩展名

		rj::StringBuffer sb;               // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);                // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将内容写入文件
	}
}

/**
 * @brief 记录信号日志
 * @param stdCode 标准化合约代码
 * @param target 目标持仓数量
 * @param price 信号价格
 * @param gentime 信号生成时间
 * @param usertag 用户标签，默认为空字符串
 * 
 * 将持仓信号记录到信号日志流中，格式为CSV格式。
 */
void SelMocker::log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag /* = "" */)
{
	_sig_logs << stdCode << "," << target << "," << price << "," << gentime << "," << usertag << "\n";  // 将信号信息写入日志流
}

/**
 * @brief 记录交易日志
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param isOpen 是否开仓
 * @param curTime 当前时间（纳秒时间戳）
 * @param price 成交价格
 * @param qty 成交数量
 * @param userTag 用户标签
 * @param fee 手续费
 * 
 * 将交易记录写入交易日志流中，格式为CSV格式。
 */
void SelMocker::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag, double fee)
{
	_trade_logs << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE") << "," << price << "," << qty << "," << userTag << "," << fee << "\n";  // 将交易信息写入日志流
}

/**
 * @brief 记录平仓日志
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多
 * @param openTime 开仓时间（纳秒时间戳）
 * @param openpx 开仓价格
 * @param closeTime 平仓时间（纳秒时间戳）
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 盈亏金额
 * @param maxprofit 最大盈利金额
 * @param maxloss 最大亏损金额
 * @param totalprofit 累计盈亏，默认为0
 * @param enterTag 开仓标签，默认为空字符串
 * @param exitTag 平仓标签，默认为空字符串
 * @param openBarNo 开仓时的调度次数，默认为0
 * @param closeBarNo 平仓时的调度次数，默认为0
 * 
 * 将平仓记录写入平仓日志流中，格式为CSV格式。
 */
void SelMocker::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double maxprofit, double maxloss,
	double totalprofit /* = 0 */, const char* enterTag /* = "" */, const char* exitTag /* = "" */, uint32_t openBarNo/* = 0*/, uint32_t closeBarNo/* = 0*/)
{
	_close_logs << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
		<< "," << closeTime << "," << closepx << "," << qty << "," << profit << "," << maxprofit << "," << maxloss << ","
		<< totalprofit << "," << enterTag << "," << exitTag << "," << openBarNo << "," << closeBarNo << "\n";  // 将平仓信息写入日志流
}

/**
 * @brief 初始化选股策略工厂
 * @param cfg 配置参数，包含策略模块路径和策略参数
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中加载策略动态库，创建策略工厂和策略实例，并初始化策略。
 * 如果配置无效或加载失败，返回false。
 */
bool SelMocker::init_sel_factory(WTSVariant* cfg)
{
	if (cfg == NULL)  // 如果配置为空，返回false
		return false;

	const char* module = cfg->getCString("module");  // 获取策略模块路径

	DllHandle hInst = DLLHelper::load_library(module);  // 加载动态库
	if (hInst == NULL)  // 如果加载失败，返回false
		return false;

	FuncCreateSelStraFact creator = (FuncCreateSelStraFact)DLLHelper::get_symbol(hInst, "createSelStrategyFact");  // 获取创建工厂函数
	if (creator == NULL)  // 如果获取失败，释放动态库并返回false
	{
		DLLHelper::free_library(hInst);  // 释放动态库
		return false;
	}

	_factory._module_inst = hInst;  // 保存动态库句柄
	_factory._module_path = module;  // 保存模块路径
	_factory._creator = creator;  // 保存创建函数指针
	_factory._remover = (FuncDeleteSelStraFact)DLLHelper::get_symbol(hInst, "deleteSelStrategyFact");  // 获取删除工厂函数
	_factory._fact = _factory._creator();  // 创建策略工厂

	WTSVariant* cfgStra = cfg->get("strategy");  // 获取策略配置
	if (cfgStra)  // 如果策略配置存在
	{
		_strategy = _factory._fact->createStrategy(cfgStra->getCString("name"), cfgStra->getCString("id"));  // 创建策略实例
		if (_strategy)  // 如果创建成功
		{
			WTSLogger::info("Strategy {}.{} created,strategy ID: {}", _factory._fact->getName(), _strategy->getName(), _strategy->id());  // 记录日志
		}
		_strategy->init(cfgStra->get("params"));  // 初始化策略
		_name = _strategy->id();  // 更新策略名称
	}

	return true;  // 返回成功
}

//////////////////////////////////////////////////////////////////////////
//IDataSink接口实现
/**
 * @brief 处理初始化事件（IDataSink接口）
 * 
 * 回测开始时调用，触发策略的on_init回调。
 */
void SelMocker::handle_init()
{
	this->on_init();  // 调用策略初始化回调
}

/**
 * @brief 处理K线闭合事件（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param period K线周期
 * @param times K线倍数
 * @param newBar 新的K线数据指针
 * 
 * 接收K线闭合事件，触发策略的on_bar回调。
 */
void SelMocker::handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	this->on_bar(stdCode, period, times, newBar);  // 调用K线数据回调
}

/**
 * @brief 处理定时调度事件（IDataSink接口）
 * @param uDate 当前日期
 * @param uTime 当前时间（分钟级别）
 * 
 * 接收定时调度事件，计算下一次调度时间，并触发策略的on_schedule回调。
 */
void SelMocker::handle_schedule(uint32_t uDate, uint32_t uTime)
{
	uint32_t nextTime = TimeUtils::getNextMinute(uTime, 1);  // 计算下一分钟时间
	if (nextTime < uTime)  // 如果跨日（下一分钟时间小于当前时间）
		uDate = TimeUtils::getNextDate(uDate);  // 日期加1
	this->on_schedule(uDate, uTime, nextTime);  // 调用定时调度回调
}

/**
 * @brief 处理交易日开始事件（IDataSink接口）
 * @param uCurDate 当前交易日日期
 * 
 * 交易日开始时调用，释放冻结持仓，并触发策略的on_session_begin回调。
 */
void SelMocker::handle_session_begin(uint32_t uCurDate)
{
	this->on_session_begin(uCurDate);  // 调用交易日开始回调
}

/**
 * @brief 处理交易日结束事件（IDataSink接口）
 * @param uCurDate 当前交易日日期
 * 
 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
 */
void SelMocker::handle_session_end(uint32_t uCurDate)
{
	this->on_session_end(uCurDate);  // 调用交易日结束回调
}

/**
 * @brief 处理回测完成事件（IDataSink接口）
 * 
 * 回测结束时调用，输出回测结果文件，并触发策略的on_bactest_end回调。
 */
void SelMocker::handle_replay_done()
{
	WTSLogger::log_dyn("strategy", _name.c_str(), LL_INFO, 
		"Strategy has been scheduled for {} times,totally taking {} microsecs,average of {} microsecs",
		_emit_times, _total_calc_time, _total_calc_time / _emit_times);  // 记录策略执行统计信息

	dump_outputs();  // 输出回测结果文件

	dump_stradata();  // 输出策略状态数据

	this->on_bactest_end();  // 调用回测结束回调
}

/**
 * @brief 处理Tick数据（IDataSink接口）
 * @param stdCode 标准化合约代码
 * @param newTick 当前Tick数据指针
 * @param pxType 价格类型：0-最新价，1-买入价，2-卖出价，3-收盘价模拟
 * 
 * 接收历史数据回放器推送的Tick数据，更新价格缓存，并触发信号执行和盈亏更新。
 * 注意：如果缓存的价格不存在，则上一笔价格就用最新价，主要是为了应对跨日价格跳空的情况。
 */
void SelMocker::handle_tick(const char* stdCode, WTSTickData* newTick, uint32_t pxType)
{
	double cur_px = newTick->price();  // 获取当前价格

	/*
	 *	By Wesley @ 2022.04.19
	 *	这里的逻辑改了一下
	 *	如果缓存的价格不存在，则上一笔价格就用最新价
	 *	这里主要是为了应对跨日价格跳空的情况
	 */
	double last_px = cur_px;  // 默认上一笔价格等于当前价格
	if (pxType != 0)  // 如果不是最新价类型
	{
		auto it = _price_map.find(stdCode);  // 查找价格缓存
		if (it != _price_map.end())  // 如果找到缓存
			last_px = it->second.first;  // 使用缓存的价格
		else
			last_px = cur_px;  // 否则使用当前价格
	}

	_price_map[stdCode].first = cur_px;  // 更新价格缓存
	_price_map[stdCode].second = (uint64_t)newTick->actiondate() * 1000000000 + newTick->actiontime();  // 更新时间戳

	//先检查是否要信号要触发
	proc_tick(stdCode, last_px, cur_px);  // 处理tick数据，执行信号和更新盈亏

	on_tick_updated(stdCode, newTick);  // 触发Tick数据更新回调

	/*
	 *	By Wesley @ 2022.04.19
	 *	isBarEnd，如果是逐tick回放，这个永远都是true，永远也不会触发下面这段逻辑
	 *	如果是模拟的tick数据，用收盘价模拟tick的时候，isBarEnd才会为true
	 *	如果不是收盘价模拟的tick，那么直接在当前tick触发撮合逻辑
	 *	这样做的目的是为了让在模拟tick触发的ontick中下单的信号能够正常处理
	 *	而不至于在回测的时候成交价偏离太远
	 */
	if (pxType != 3)  // 如果不是收盘价模拟类型
		proc_tick(stdCode, last_px, cur_px);  // 再次处理tick数据（确保信号能及时执行）
}

/**
 * @brief 处理Tick数据
 * @param stdCode 标准化合约代码
 * @param last_px 上一笔价格
 * @param cur_px 当前价格
 * 
 * 在tick数据到来时，检查是否有待执行的持仓信号，如果有则执行。
 * 同时更新持仓的动态盈亏。
 */
void SelMocker::proc_tick(const char* stdCode, double last_px, double cur_px)
{
	{
		auto it = _sig_map.find(stdCode);  // 查找该合约的信号
		if (it != _sig_map.end())  // 如果找到信号
		{
			//if (sInfo->isInTradingTime(_replayer->get_raw_time(), true))
			{
				const SigInfo& sInfo = it->second;  // 获取信号信息
				double price;  // 成交价格
				if (decimal::eq(sInfo._desprice, 0.0))  // 如果期望价格为0
					price = cur_px;  // 使用当前价格
				else
					price = sInfo._desprice;  // 否则使用期望价格
				do_set_position(stdCode, sInfo._volume, price, sInfo._usertag.c_str());  // 执行持仓设置
				_sig_map.erase(it);  // 删除已执行的信号
			}
		}
	}

	update_dyn_profit(stdCode, cur_px);  // 更新动态盈亏
}


//////////////////////////////////////////////////////////////////////////
//回调函数
/**
 * @brief K线数据回调
 * @param stdCode 标准化合约代码
 * @param period K线周期
 * @param times K线倍数
 * @param newBar 新的K线数据指针
 * 
 * 接收K线数据，标记K线状态，并触发策略的on_bar_close回调。
 */
void SelMocker::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (newBar == NULL)  // 如果K线数据为空，直接返回
		return;

	std::string realPeriod;  // 实际周期字符串
	if (period[0] == 'd')  // 如果是日线周期
		realPeriod = StrUtil::printf("%s%u", period, times);  // 格式化为"d1"、"d2"等
	else
		realPeriod = StrUtil::printf("m%u", times);  // 格式化为"m1"、"m5"等

	std::string key = StrUtil::printf("%s#%s", stdCode, realPeriod.c_str());  // 构建K线标签键
	KlineTag& tag = _kline_tags[key];  // 获取或创建K线标签
	tag._closed = true;  // 标记为已闭合
	tag._count++;  // 增加闭合次数

	on_bar_close(stdCode, realPeriod.c_str(), newBar);  // 触发K线闭合回调
}

/**
 * @brief 初始化完成回调
 * 
 * 策略初始化完成后调用，触发策略的on_init回调。
 */
void SelMocker::on_init()
{
	if (_strategy)  // 如果策略存在
		_strategy->on_init(this);  // 调用策略的初始化回调

	WTSLogger::info("SEL Strategy initialized with {} slippage: {}", _ratio_slippage ? "ratio" : "absolute", _slippage);  // 记录初始化日志
}

/**
 * @brief 更新动态盈亏
 * @param stdCode 标准化合约代码
 * @param price 当前价格
 * 
 * 根据当前价格更新指定合约的持仓动态盈亏。
 * 同时更新每笔持仓明细的最大盈利、最大亏损、最高价、最低价等信息。
 */
void SelMocker::update_dyn_profit(const char* stdCode, double price)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it != _pos_map.end())  // 如果找到持仓
	{
		PosInfo& pInfo = (PosInfo&)it->second;  // 获取持仓信息
		if (pInfo._volume == 0)  // 如果持仓为0
		{
			pInfo._dynprofit = 0;  // 动态盈亏为0
		}
		else
		{
			WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
			double dynprofit = 0;  // 总动态盈亏
			// 遍历持仓明细
			for (auto pit = pInfo._details.begin(); pit != pInfo._details.end(); pit++)
			{
				DetailInfo& dInfo = *pit;  // 获取持仓明细信息
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);  // 计算明细盈亏
				if (dInfo._profit > 0)  // 如果盈利
					dInfo._max_profit = max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利
				else if (dInfo._profit < 0)  // 如果亏损
					dInfo._max_loss = min(dInfo._profit, dInfo._max_loss);  // 更新最大亏损

				dInfo._max_price = std::max(dInfo._max_price, price);  // 更新最高价
				dInfo._min_price = std::min(dInfo._min_price, price);  // 更新最低价

				dynprofit += dInfo._profit;  // 累加明细盈亏
			}

			pInfo._dynprofit = dynprofit;  // 更新持仓动态盈亏
		}
	}

	double total_dynprofit = 0;  // 总动态盈亏
	// 遍历所有持仓
	for (auto& v : _pos_map)
	{
		const PosInfo& pInfo = v.second;  // 获取持仓信息
		total_dynprofit += pInfo._dynprofit;  // 累加动态盈亏
	}

	_fund_info._total_dynprofit = total_dynprofit;  // 更新总动态盈亏
}

/**
 * @brief Tick数据回调
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据指针
 * @param bEmitStrategy 是否触发策略回调，默认为true
 * 
 * 接收Tick数据，更新价格缓存，并触发策略的on_tick回调。
 * 注意：实际的Tick处理逻辑已迁移到handle_tick函数。
 */
void SelMocker::on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy /* = true */)
{
	//By Wesley @ 2022.04.19
	//这个逻辑迁移到handle_tick去了
}

/**
 * @brief K线闭合回调
 * @param code 标准化合约代码
 * @param period K线周期
 * @param newBar 新的K线数据指针
 * 
 * K线闭合时调用，触发策略的on_bar回调。
 */
void SelMocker::on_bar_close(const char* code, const char* period, WTSBarStruct* newBar)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_bar(this, code, period, newBar);  // 调用策略的K线回调
}

/**
 * @brief Tick数据更新回调
 * @param code 标准化合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 当订阅的合约有新的Tick数据时调用，触发策略的on_tick回调。
 */
void SelMocker::on_tick_updated(const char* code, WTSTickData* newTick)
{
	auto it = _tick_subs.find(code);  // 查找订阅列表
	if (it == _tick_subs.end())  // 如果没有订阅
		return;

	if (_strategy)  // 如果策略存在
		_strategy->on_tick(this, code, newTick);  // 调用策略的Tick回调
}

/**
 * @brief 策略定时调度回调
 * @param curDate 当前日期
 * @param curTime 当前时间（分钟级别）
 * 
 * 定时调度时调用，触发策略的on_schedule回调。
 */
void SelMocker::on_strategy_schedule(uint32_t curDate, uint32_t curTime)
{
	if (_strategy)  // 如果策略存在
		_strategy->on_schedule(this, curDate, curTime);  // 调用策略的定时调度回调
}


/**
 * @brief 定时调度回调
 * @param curDate 当前日期
 * @param curTime 当前时间（分钟级别）
 * @param fireTime 触发时间（分钟级别）
 * @return 是否继续调度，返回true表示继续
 * 
 * 定时调度时调用，触发策略的on_schedule回调。
 * 同时检查持仓，如果持仓不在信号列表中，则自动清仓。
 */
bool SelMocker::on_schedule(uint32_t curDate, uint32_t curTime, uint32_t fireTime)
{
	_is_in_schedule = true;  // 开始调度，修改标记

	_schedule_times++;  // 增加调度次数

	TimeUtils::Ticker ticker;  // 创建计时器
	on_strategy_schedule(curDate, curTime);  // 调用策略定时调度回调

	wt_hashset<std::string> to_clear;  // 需要清仓的合约集合
	// 遍历所有持仓
	for(auto& v : _pos_map)
	{
		const PosInfo& pInfo = v.second;  // 获取持仓信息
		const char* code = v.first.c_str();  // 获取合约代码
		if(_sig_map.find(code) == _sig_map.end() && !decimal::eq(pInfo._volume, 0.0))  // 如果信号中没有该持仓且持仓不为0
		{
			//新的信号中没有该持仓,则要清空
			to_clear.insert(code);  // 添加到清仓列表
		}
	}

	// 遍历清仓列表，自动清仓
	for(const std::string& code : to_clear)
	{
		append_signal(code.c_str(), 0, "autoexit");  // 追加清仓信号
	}

	_emit_times++;  // 增加计算次数
	_total_calc_time += ticker.micro_seconds();  // 累加计算时间

	_is_in_schedule = false;  // 调度结束，修改标记
	return true;  // 返回继续调度
}

/**
 * @brief 交易日开始回调
 * @param curTDate 当前交易日日期
 * 
 * 交易日开始时调用，释放冻结持仓，并触发策略的on_session_begin回调。
 */
void SelMocker::on_session_begin(uint32_t curTDate)
{
	_cur_tdate = curTDate;  // 更新当前交易日
	//每个交易日开始，要把冻结持仓置零
	for (auto& it : _pos_map)  // 遍历所有持仓
	{
		const char* stdCode = it.first.c_str();  // 获取合约代码
		PosInfo& pInfo = (PosInfo&)it.second;   // 获取持仓信息
		if (!decimal::eq(pInfo._frozen, 0))     // 如果有冻结持仓
		{
			log_debug("{} of {} frozen released on {}", pInfo._frozen, stdCode, curTDate);  // 记录日志
			pInfo._frozen = 0;  // 释放冻结持仓
		}
	}
}

/**
 * @brief 枚举持仓
 * @param cb 回调函数，用于遍历每个合约的持仓
 * 
 * 遍历所有持仓（包括信号队列中的持仓），调用回调函数传递持仓信息。
 */
void SelMocker::enum_position(FuncEnumSelPositionCallBack cb)
{
	wt_hashmap<std::string, double> desPos;  // 目标持仓映射表
	// 遍历实际持仓
	for (auto& it : _pos_map)
	{
		const char* stdCode = it.first.c_str();  // 获取合约代码
		const PosInfo& pInfo = it.second;        // 获取持仓信息
		desPos[stdCode] = pInfo._volume;         // 记录持仓数量
	}

	// 遍历信号队列中的持仓
	for (auto sit : _sig_map)
	{
		const char* stdCode = sit.first.c_str();  // 获取合约代码
		const SigInfo& sInfo = sit.second;        // 获取信号信息
		desPos[stdCode] = sInfo._volume;          // 记录目标持仓数量（覆盖实际持仓）
	}

	// 调用回调函数传递持仓信息
	for (auto v : desPos)
	{
		cb(v.first.c_str(), v.second);  // 调用回调函数
	}
}

/**
 * @brief 交易日结束回调
 * @param curTDate 当前交易日日期
 * 
 * 交易日结束时调用，记录持仓和资金日志，并触发策略的on_session_end回调。
 */
void SelMocker::on_session_end(uint32_t curTDate)
{
	uint32_t curDate = curTDate;  // 当前日期

	double total_profit = 0;      // 总已平仓盈亏
	double total_dynprofit = 0;   // 总动态盈亏

	// 遍历所有持仓
	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码
		const PosInfo& pInfo = it->second;        // 获取持仓信息
		total_profit += pInfo._closeprofit;       // 累加已平仓盈亏
		total_dynprofit += pInfo._dynprofit;      // 累加动态盈亏

		if (decimal::eq(pInfo._volume, 0.0))      // 如果持仓为0，跳过
			continue;

		_pos_logs << fmt::format("{},{},{},{:.2f},{:.2f}\n", curDate, stdCode,
			pInfo._volume, pInfo._closeprofit, pInfo._dynprofit);  // 记录持仓日志
	}

	_fund_logs << fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curDate,
		_fund_info._total_profit, _fund_info._total_dynprofit,
		_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees);  // 记录资金日志

	//save_data();
}

//////////////////////////////////////////////////////////////////////////
//策略接口（ISelStraCtx接口实现）
/**
 * @brief 获取当前价格
 * @param stdCode 标准化合约代码
 * @return 当前价格，如果合约不存在返回0.0
 * 
 * 从历史数据回放器获取指定合约的当前价格。
 */
double SelMocker::stra_get_price(const char* stdCode)
{
	if (_replayer)  // 如果回放器存在
		return _replayer->get_cur_price(stdCode);  // 返回当前价格

	return 0.0;  // 否则返回0
}

/**
 * @brief 设置目标持仓
 * @param stdCode 标准化合约代码
 * @param qty 目标持仓数量，正数为做多，负数为做空，0表示清仓
 * @param userTag 用户标签，默认为空字符串
 * 
 * 设置指定合约的目标持仓数量。会将信号添加到信号队列中，在下一个tick执行。
 * 如果目标持仓与当前持仓相同，则不执行任何操作。
 */
void SelMocker::stra_set_position(const char* stdCode, double qty, const char* userTag /* = "" */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;
	}

	//如果不能做空，则目标仓位不能设置负数
	if (!commInfo->canShort() && decimal::lt(qty, 0))  // 如果不能做空且目标仓位为负数
	{
		log_error("Cannot short on {}", stdCode);  // 记录错误日志
		return;
	}

	double total = stra_get_position(stdCode, false);  // 获取当前持仓
	//如果目标仓位和当前仓位是一致的，直接退出
	if (decimal::eq(total, qty))  // 如果目标仓位等于当前仓位
		return;

	if (commInfo->isT1())  // 如果是T+1规则
	{
		double valid = stra_get_position(stdCode, true);  // 获取可用持仓
		double frozen = total - valid;  // 计算冻结持仓
		//如果是T+1规则，则目标仓位不能小于冻结仓位
		if (decimal::lt(qty, frozen))  // 如果目标仓位小于冻结仓位
		{
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_ERROR, 
				"New position of {} cannot be set to {} due to {} being frozen", stdCode, qty, frozen);  // 记录错误日志
			return;
		}
	}

	_replayer->sub_tick(id(), stdCode);  // 订阅Tick数据
	append_signal(stdCode, qty, userTag);  // 追加持仓信号
}

/**
 * @brief 追加持仓信号
 * @param stdCode 标准化合约代码
 * @param qty 目标持仓数量
 * @param userTag 用户标签，默认为空字符串
 * @param price 期望成交价格，默认为0.0（使用当前价格）
 * 
 * 将策略发出的持仓信号添加到信号队列中。
 * 信号会在下一个tick到来时执行（通过proc_tick函数）。
 */
void SelMocker::append_signal(const char* stdCode, double qty, const char* userTag /* = "" */, double price/* = 0.0*/)
{
	double curPx = _price_map[stdCode].first;  // 获取当前价格

	SigInfo& sInfo = _sig_map[stdCode];  // 获取或创建信号信息
	sInfo._volume = qty;                 // 设置目标持仓数量
	sInfo._sigprice = curPx;            // 设置信号价格
	sInfo._desprice = price;            // 设置期望成交价格
	sInfo._usertag = userTag;           // 设置用户标签
	sInfo._gentime = (uint64_t)_replayer->get_date() * 1000000000 + (uint64_t)_replayer->get_raw_time() * 100000 + _replayer->get_secs();  // 设置信号生成时间
	sInfo._triggered = !_is_in_schedule;  // 设置触发标志（如果不在调度中，则已触发）

	log_signal(stdCode, qty, curPx, sInfo._gentime, userTag);  // 记录信号日志

	//save_data();
}

/**
 * @brief 执行持仓设置
 * @param stdCode 标准化合约代码
 * @param qty 目标持仓数量，正数为做多，负数为做空
 * @param price 成交价格，默认为0.0（使用当前价格）
 * @param userTag 用户标签，默认为空字符串
 * @param bTriggered 是否已触发，默认为false
 * 
 * 根据目标持仓数量执行实际的开仓、加仓、减仓或平仓操作。
 * 处理持仓方向变化、T+1规则、滑点计算、手续费计算等。
 */
void SelMocker::do_set_position(const char* stdCode, double qty, double price /* = 0.0 */, const char* userTag /* = "" */, bool bTriggered /* = false */)
{
	PosInfo& pInfo = _pos_map[stdCode];  // 获取或创建持仓信息
	double curPx = price;                // 当前价格
	if (decimal::eq(price, 0.0))         // 如果价格为0
		curPx = _price_map[stdCode].first;  // 使用价格缓存中的价格
	uint64_t curTm = (uint64_t)_replayer->get_date() * 10000 + _replayer->get_min_time();  // 当前时间（分钟级别）
	uint32_t curTDate = _replayer->get_trading_date();  // 当前交易日

	if (decimal::eq(pInfo._volume, qty))  // 如果目标持仓等于当前持仓
		return;

	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)  // 如果合约信息不存在
		return;

	//成交价
	double trdPx = curPx;  // 成交价格
	double diff = qty - pInfo._volume;  // 计算持仓差值
	bool isBuy = decimal::gt(diff, 0.0);  // 判断是否为买入（增加持仓）
	if (decimal::gt(pInfo._volume*diff, 0))  // 当前持仓和仓位变动方向一致，增加一条明细，增加数量即可
	{
		pInfo._volume = qty;  // 更新持仓数量

		//如果T+1，则冻结仓位要增加
		if (commInfo->isT1())  // 如果是T+1规则
		{
			//ASSERT(diff>0);
			pInfo._frozen += diff;  // 增加冻结持仓
			log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录日志
		}

		// 计算滑点
		if (_slippage != 0)  // 如果设置了滑点
		{
			if (_ratio_slippage)  // 如果是比例滑点
			{
				//By Wesley @ 2023.05.05
				//如果是比率滑点，则要根据目标成交价计算
				//得到滑点以后，再根据pricetick做一个修正
				double slp = (_slippage * trdPx / 10000.0);  // 计算比例滑点（单位：万分之一）
				slp = round(slp / commInfo->getPriceTick())*commInfo->getPriceTick();  // 根据最小变动价位修正滑点

				trdPx += slp * (isBuy ? 1 : -1);  // 买单加滑点，卖单减滑点
			}
			else  // 如果是绝对滑点
				trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 买单加滑点，卖单减滑点
		}

		// 创建持仓明细
		DetailInfo dInfo;  // 持仓明细信息
		dInfo._long = decimal::gt(qty, 0);  // 设置是否做多
		dInfo._price = trdPx;  // 设置成交价格
		dInfo._max_price = trdPx;  // 初始化最高价为成交价
		dInfo._min_price = trdPx;  // 初始化最低价为成交价
		dInfo._volume = abs(diff);  // 设置持仓数量
		dInfo._opentime = curTm;  // 设置开仓时间
		dInfo._opentdate = curTDate;  // 设置开仓交易日
		strcpy(dInfo._opentag, userTag);  // 设置开仓标签
		dInfo._open_barno = _schedule_times;  // 设置开仓时的调度次数
		pInfo._details.emplace_back(dInfo);  // 添加到持仓明细列表
		pInfo._last_entertime = curTm;  // 更新最后开仓时间

		// 计算手续费
		double fee = _replayer->calc_fee(stdCode, trdPx, abs(diff), 0);  // 计算手续费（开仓）
		_fund_info._total_fees += fee;  // 累加总手续费

		log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(diff), userTag, fee);  // 记录交易日志
	}
	else
	{//持仓方向和目标仓位方向不一致，需要平仓
		double left = abs(diff);  // 需要平仓的数量
		bool isBuy = decimal::gt(diff, 0.0);  // 判断是否为买入（用于滑点计算）

		// 计算滑点
		if (_slippage != 0)  // 如果设置了滑点
		{
			if (_ratio_slippage)  // 如果是比例滑点
			{
				//By Wesley @ 2023.05.05
				//如果是比率滑点，则要根据目标成交价计算
				//得到滑点以后，再根据pricetick做一个修正
				double slp = (_slippage * trdPx / 10000.0);  // 计算比例滑点（单位：万分之一）
				slp = round(slp / commInfo->getPriceTick())*commInfo->getPriceTick();  // 根据最小变动价位修正滑点

				trdPx += slp * (isBuy ? 1 : -1);  // 买单加滑点，卖单减滑点
			}
			else  // 如果是绝对滑点
				trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 买单加滑点，卖单减滑点
		}

		pInfo._volume = qty;  // 更新持仓数量
		if (decimal::eq(pInfo._volume, 0))  // 如果持仓为0
			pInfo._dynprofit = 0;  // 动态盈亏为0
		uint32_t count = 0;  // 已平仓完成的明细数量
		// 遍历持仓明细，按时间顺序平仓
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
		{
			DetailInfo& dInfo = *it;  // 获取持仓明细信息
			double maxQty = min(dInfo._volume, left);  // 计算本次平仓数量（不能超过明细数量和剩余数量）
			if (decimal::eq(maxQty, 0))  // 如果平仓数量为0，跳过
				continue;

			// 按比例计算最大盈利和最大亏损
			double maxProf = dInfo._max_profit * maxQty / dInfo._volume;  // 计算本次平仓的最大盈利
			double maxLoss = dInfo._max_loss * maxQty / dInfo._volume;  // 计算本次平仓的最大亏损

			dInfo._volume -= maxQty;  // 减少明细持仓数量
			left -= maxQty;  // 减少剩余平仓数量

			if (decimal::eq(dInfo._volume, 0))  // 如果明细持仓为0
				count++;  // 增加已平仓完成的明细数量

			// 计算盈亏
			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算盈亏（多空通用公式）
			if (!dInfo._long)  // 如果是做空
				profit *= -1;  // 盈亏取反

			pInfo._closeprofit += profit;  // 累加已平仓盈亏
			pInfo._dynprofit = pInfo._dynprofit*dInfo._volume / (dInfo._volume + maxQty);  // 浮盈也要做等比缩放
			pInfo._last_exittime = curTm;  // 更新最后平仓时间
			_fund_info._total_profit += profit;  // 累加总已平仓盈亏

			// 计算手续费（平仓：今仓为2，昨仓为1）
			double fee = _replayer->calc_fee(stdCode, trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费
			_fund_info._total_fees += fee;  // 累加总手续费
			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, trdPx, maxQty, userTag, fee);  // 记录交易日志
			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, 
				trdPx, maxQty, profit, dInfo._max_profit, dInfo._max_loss, pInfo._closeprofit, dInfo._opentag, userTag, dInfo._open_barno, _schedule_times);  // 记录平仓日志

			if (left == 0)  // 如果剩余平仓数量为0，退出循环
				break;
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)  // 删除已平仓完成的明细
		{
			auto it = pInfo._details.begin();  // 获取第一个明细
			pInfo._details.erase(it);  // 删除明细
			count--;  // 减少计数
		}

		//最后，如果还有剩余的，则需要反手了
		if (left > 0)  // 如果还有剩余平仓数量（需要反手）
		{
			left = left * qty / abs(qty);  // 调整剩余数量符号（确保与目标持仓方向一致）

			//如果T+1，则冻结仓位要增加
			if (commInfo->isT1())  // 如果是T+1规则
			{
				pInfo._frozen += left;  // 增加冻结持仓
				log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录日志
			}

			// 创建反手持仓明细
			DetailInfo dInfo;  // 持仓明细信息
			dInfo._long = decimal::gt(qty, 0);  // 设置是否做多
			dInfo._price = trdPx;  // 设置成交价格
			dInfo._max_price = trdPx;  // 初始化最高价为成交价
			dInfo._min_price = trdPx;  // 初始化最低价为成交价
			dInfo._volume = abs(left);  // 设置持仓数量
			dInfo._opentime = curTm;  // 设置开仓时间
			dInfo._opentdate = curTDate;  // 设置开仓交易日
			strcpy(dInfo._opentag, userTag);  // 设置开仓标签
			dInfo._open_barno = _schedule_times;  // 设置开仓时的调度次数
			pInfo._details.emplace_back(dInfo);  // 添加到持仓明细列表
			pInfo._last_entertime = curTm;  // 更新最后开仓时间

			//这里还需要写一笔成交记录
			double fee = _replayer->calc_fee(stdCode, trdPx, abs(left), 0);  // 计算手续费（开仓）
			_fund_info._total_fees += fee;  // 累加总手续费
			log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(left), userTag, fee);  // 记录交易日志
		}
	}
}

/**
 * @brief 获取K线数据切片
 * @param stdCode 标准化合约代码
 * @param period K线周期（如"m1"表示1分钟，"d1"表示1日）
 * @param count 获取的K线数量
 * @return K线数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的K线数据切片。
 * 同时更新价格缓存，将K线的收盘价和时间戳更新到价格映射表中。
 */
WTSKlineSlice* SelMocker::stra_get_bars(const char* stdCode, const char* period, uint32_t count)
{
	thread_local static char key[64] = { 0 };  // 线程局部静态变量，用于构建K线标签键
	fmtutil::format_to(key, "{}#{}", stdCode, period);  // 格式化K线标签键：合约代码#周期

	thread_local static char basePeriod[2] = { 0 };  // 线程局部静态变量，用于存储基础周期
	basePeriod[0] = period[0];  // 获取周期类型（'d'或'm'）
	uint32_t times = 1;  // 周期倍数，默认为1
	if (strlen(period) > 1)  // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);  // 解析周期倍数（如"m5"中的5）
	else
		strcat(key, "1");  // 如果没有倍数，添加"1"

	WTSKlineSlice* kline = _replayer->get_kline_slice(stdCode, basePeriod, count, times, false);  // 获取K线数据切片

	KlineTag& tag = _kline_tags[key];  // 获取或创建K线标签
	tag._closed = false;  // 标记K线未闭合（因为是通过get_bars获取的，不是闭合事件）

	if (kline)  // 如果K线数据存在
	{
		double lastClose = kline->at(-1)->close;  // 获取最后一根K线的收盘价
		uint64_t lastTime = 0;  // 最后时间戳
		if(basePeriod[0] == 'd')  // 如果是日线周期
		{
			lastTime = kline->at(-1)->date;  // 获取日期
			WTSSessionInfo* sInfo = _replayer->get_session_info(stdCode, true);  // 获取交易时段信息
			lastTime *= 1000000000;  // 转换为纳秒时间戳（日期部分）
			lastTime += (uint64_t)sInfo->getCloseTime() * 100000;  // 添加收盘时间（分钟级别）
		}
		else  // 如果是分钟周期
		{
			lastTime = kline->at(-1)->time;  // 获取时间
			lastTime += 199000000000;  // 添加日期部分（1990-01-01作为基准）
			lastTime *= 100000;  // 转换为纳秒时间戳
		}

		if(lastTime > _price_map[stdCode].second)  // 如果K线时间戳大于当前价格缓存的时间戳
		{
			_price_map[stdCode].second = lastTime;  // 更新时间戳
			_price_map[stdCode].first = lastClose;  // 更新价格
		}
	}

	return kline;  // 返回K线数据切片
}

/**
 * @brief 获取Tick数据切片
 * @param stdCode 标准化合约代码
 * @param count 获取的Tick数量
 * @return Tick数据切片指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的Tick数据切片。
 */
WTSTickSlice* SelMocker::stra_get_ticks(const char* stdCode, uint32_t count)
{
	return _replayer->get_tick_slice(stdCode, count);  // 返回Tick数据切片
}

/**
 * @brief 获取最新Tick数据
 * @param stdCode 标准化合约代码
 * @return 最新Tick数据指针，如果数据不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的最新Tick数据。
 */
WTSTickData* SelMocker::stra_get_last_tick(const char* stdCode)
{
	return _replayer->get_last_tick(stdCode);  // 返回最新Tick数据
}

/**
 * @brief 订阅Tick数据
 * @param code 标准化合约代码
 * 
 * 订阅指定合约的Tick数据。订阅后，该合约的Tick数据会触发策略的on_tick回调。
 * 
 * 注意：主动订阅tick会在本地记录一下，tick数据回调的时候会先检查一下。
 */
void SelMocker::stra_sub_ticks(const char* code)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(code);  // 添加到Tick订阅列表

	_replayer->sub_tick(_context_id, code);  // 向回放器订阅Tick数据
}

/**
 * @brief 获取合约信息
 * @param stdCode 标准化合约代码
 * @return 合约信息指针，如果合约不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的合约信息。
 */
WTSCommodityInfo* SelMocker::stra_get_comminfo(const char* stdCode)
{
	return _replayer->get_commodity_info(stdCode);  // 返回合约信息
}

/**
 * @brief 获取原始合约代码
 * @param stdCode 标准化合约代码
 * @return 原始合约代码字符串
 * 
 * 从历史数据回放器获取指定标准化合约代码对应的原始合约代码。
 */
std::string SelMocker::stra_get_rawcode(const char* stdCode)
{
	return _replayer->get_rawcode(stdCode);  // 返回原始合约代码
}

/**
 * @brief 获取交易时段信息
 * @param stdCode 标准化合约代码
 * @return 交易时段信息指针，如果合约不存在返回NULL
 * 
 * 从历史数据回放器获取指定合约的交易时段信息。
 */
WTSSessionInfo* SelMocker::stra_get_sessinfo(const char* stdCode)
{
	return _replayer->get_session_info(stdCode, true);  // 返回交易时段信息
}

/**
 * @brief 获取当前交易日
 * @return 当前交易日（格式：YYYYMMDD）
 * 
 * 返回当前回测的交易日日期。
 */
uint32_t SelMocker::stra_get_tdate()
{
	return _replayer->get_trading_date();  // 返回交易日日期
}

/**
 * @brief 获取当前日期
 * @return 当前日期（格式：YYYYMMDD）
 * 
 * 返回当前回测的日期。
 */
uint32_t SelMocker::stra_get_date()
{
	return _replayer->get_date();  // 返回日期
}

/**
 * @brief 获取当前时间
 * @return 当前时间（格式：HHMM，分钟级别）
 * 
 * 返回当前回测的时间（分钟级别）。
 */
uint32_t SelMocker::stra_get_time()
{
	return _replayer->get_min_time();  // 返回分钟级别时间
}

/**
 * @brief 获取资金数据
 * @param flag 数据标志：0-总资产（已平仓盈亏-手续费+动态盈亏），1-已平仓盈亏，2-动态盈亏，3-手续费
 * @return 资金数据值
 * 
 * 根据标志返回相应的资金数据。
 */
double SelMocker::stra_get_fund_data(int flag)
{
	switch (flag)  // 根据标志返回相应数据
	{
	case 0:  // 总资产
		return _fund_info._total_profit - _fund_info._total_fees + _fund_info._total_dynprofit;  // 已平仓盈亏-手续费+动态盈亏
	case 1:  // 已平仓盈亏
		return _fund_info._total_profit;
	case 2:  // 动态盈亏
		return _fund_info._total_dynprofit;
	case 3:  // 手续费
		return _fund_info._total_fees;
	default:  // 其他情况
		return 0.0;  // 返回0
	}
}


/**
 * @brief 记录信息日志
 * @param message 日志消息
 * 
 * 将信息日志记录到策略日志中。
 */
void SelMocker::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 记录信息日志
}

/**
 * @brief 记录调试日志
 * @param message 日志消息
 * 
 * 将调试日志记录到策略日志中。
 */
void SelMocker::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 记录调试日志
}

/**
 * @brief 记录警告日志
 * @param message 日志消息
 * 
 * 将警告日志记录到策略日志中。
 */
void SelMocker::stra_log_warn(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_WARN, message);  // 记录警告日志
}

/**
 * @brief 记录错误日志
 * @param message 日志消息
 * 
 * 将错误日志记录到策略日志中。
 */
void SelMocker::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 记录错误日志
}

/**
 * @brief 加载用户数据
 * @param key 数据键
 * @param defVal 默认值，默认为空字符串
 * @return 数据值字符串指针，如果不存在返回默认值
 * 
 * 从用户数据映射表中加载指定键的数据值。
 */
const char* SelMocker::stra_load_user_data(const char* key, const char* defVal /*= ""*/)
{
	auto it = _user_datas.find(key);  // 查找用户数据
	if (it != _user_datas.end())  // 如果找到
		return it->second.c_str();  // 返回数据值

	return defVal;  // 否则返回默认值
}

/**
 * @brief 保存用户数据
 * @param key 数据键
 * @param val 数据值
 * 
 * 将用户数据保存到用户数据映射表中，并标记为已修改。
 */
void SelMocker::stra_save_user_data(const char* key, const char* val)
{
	_user_datas[key] = val;  // 保存用户数据
	_ud_modified = true;  // 标记为已修改
}

/**
 * @brief 获取持仓
 * @param stdCode 标准化合约代码
 * @param bOnlyValid 获取可用持仓（T+1规则下排除冻结持仓），默认为false
 * @param userTag 用户标签，默认为空字符串
 * @return 持仓数量，正数为做多，负数为做空，0表示无持仓
 * 
 * 查询指定合约的持仓数量。
 * 如果指定了userTag，则返回该标签对应的持仓明细数量。
 * 如果有信号但还未执行，则返回信号中的目标持仓数量。
 */
double SelMocker::stra_get_position(const char* stdCode, bool bOnlyValid/* = false*/, const char* userTag /* = "" */)
{
	//By Wesley @ 2023.04.17
	//如果有信号，说明刚下了指令，还没等到下一个tick进来，用户就在读取仓位
	//但是如果用户读取，还是要返回
	auto sit = _sig_map.find(stdCode);  // 查找信号
	if (sit != _sig_map.end())  // 如果找到信号
	{
		return sit->second._volume;  // 返回信号中的目标持仓数量
	}

	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (strlen(userTag) == 0)  // 如果用户标签为空
	{
		//只有userTag为空的时候时候，才会用bOnlyValid
		if (bOnlyValid)  // 如果只获取可用持仓
		{
			//这里理论上，只有多头才会进到这里
			//其他地方要保证，空头持仓的话，_frozen要为0
			return pInfo._volume - pInfo._frozen;  // 返回总持仓减去冻结持仓
		}
		else
			return pInfo._volume;  // 返回总持仓
	}

	// 如果指定了userTag，则查找对应的持仓明细
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;  // 跳过

		return dInfo._volume;  // 返回该明细的持仓数量
	}

	return 0;  // 如果没有找到对应的明细，返回0
}

/**
 * @brief 获取日线价格
 * @param stdCode 标准化合约代码
 * @param flag 价格标志：0-收盘价，1-开盘价，2-最高价，3-最低价，默认为0
 * @return 日线价格，如果合约不存在返回0.0
 * 
 * 从历史数据回放器获取指定合约的日线价格。
 */
double SelMocker::stra_get_day_price(const char* stdCode, int flag /* = 0 */)
{
	if (_replayer)  // 如果回放器存在
		return _replayer->get_day_price(stdCode, flag);  // 返回日线价格

	return 0.0;  // 否则返回0
}

/**
 * @brief 获取首次开仓时间
 * @param stdCode 标准化合约代码
 * @return 首次开仓时间（纳秒时间戳），如果没有持仓返回0
 * 
 * 返回指定合约的首次开仓时间（即最早一笔持仓明细的开仓时间）。
 */
uint64_t SelMocker::stra_get_first_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return 0;  // 返回0

	return pInfo._details[0]._opentime;  // 返回第一个明细的开仓时间
}

/**
 * @brief 获取最后开仓时间
 * @param stdCode 标准化合约代码
 * @return 最后开仓时间（纳秒时间戳），如果没有持仓返回0
 * 
 * 返回指定合约的最后开仓时间（即最新一笔持仓明细的开仓时间）。
 */
uint64_t SelMocker::stra_get_last_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return 0;  // 返回0

	return pInfo._details[pInfo._details.size() - 1]._opentime;  // 返回最后一个明细的开仓时间
}

/**
 * @brief 获取最后开仓标签
 * @param stdCode 标准化合约代码
 * @return 最后开仓标签字符串，如果没有持仓返回空字符串
 * 
 * 返回指定合约的最后开仓标签（即最新一笔持仓明细的开仓标签）。
 */
const char* SelMocker::stra_get_last_entertag(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return "";  // 返回空字符串

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return "";  // 返回空字符串

	return pInfo._details[pInfo._details.size() - 1]._opentag;  // 返回最后一个明细的开仓标签
}

/**
 * @brief 获取最后平仓时间
 * @param stdCode 标准化合约代码
 * @return 最后平仓时间（纳秒时间戳），如果没有持仓返回0
 * 
 * 返回指定合约的最后平仓时间。
 */
uint64_t SelMocker::stra_get_last_exittime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._last_exittime;  // 返回最后平仓时间
}

/**
 * @brief 获取最后开仓价格
 * @param stdCode 标准化合约代码
 * @return 最后开仓价格，如果没有持仓返回0
 * 
 * 返回指定合约的最后开仓价格（即最新一笔持仓明细的开仓价格）。
 */
double SelMocker::stra_get_last_enterprice(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return 0;  // 返回0

	return pInfo._details[pInfo._details.size() - 1]._price;  // 返回最后一个明细的开仓价格
}

/**
 * @brief 获取持仓均价
 * @param stdCode 标准化合约代码
 * @return 持仓均价，如果没有持仓返回0
 * 
 * 计算指定合约的持仓均价（按持仓数量加权平均）。
 */
double SelMocker::stra_get_position_avgpx(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._volume == 0)  // 如果持仓为0
		return 0.0;  // 返回0

	double amount = 0.0;  // 总金额
	// 遍历持仓明细，累加价格*数量的乘积
	for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)
	{
		const DetailInfo& dInfo = *dit;  // 获取持仓明细信息
		amount += dInfo._price*dInfo._volume;  // 累加价格*数量
	}

	return amount / pInfo._volume;  // 返回均价（总金额/总数量）
}

/**
 * @brief 获取持仓盈亏
 * @param stdCode 标准化合约代码
 * @return 持仓动态盈亏，如果没有持仓返回0
 * 
 * 返回指定合约的持仓动态盈亏。
 */
double SelMocker::stra_get_position_profit(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._dynprofit;  // 返回动态盈亏
}

/**
 * @brief 获取持仓明细开仓时间
 * @param stdCode 标准化合约代码
 * @param userTag 用户标签
 * @return 开仓时间（纳秒时间戳），如果没有找到返回0
 * 
 * 根据用户标签查找对应的持仓明细，返回其开仓时间。
 */
uint64_t SelMocker::stra_get_detail_entertime(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	// 遍历持仓明细，查找匹配的标签
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;  // 跳过

		return dInfo._opentime;  // 返回开仓时间
	}

	return 0;  // 如果没有找到，返回0
}

/**
 * @brief 获取持仓明细成本价
 * @param stdCode 标准化合约代码
 * @param userTag 用户标签
 * @return 成本价，如果没有找到返回0
 * 
 * 根据用户标签查找对应的持仓明细，返回其开仓价格（成本价）。
 */
double SelMocker::stra_get_detail_cost(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	// 遍历持仓明细，查找匹配的标签
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;  // 跳过

		return dInfo._price;  // 返回开仓价格
	}

	return 0.0;  // 如果没有找到，返回0
}

/**
 * @brief 获取持仓明细盈亏
 * @param stdCode 标准化合约代码
 * @param userTag 用户标签
 * @param flag 盈亏标志：0-当前盈亏，1-最大盈利，-1-最大亏损，2-最高价，-2-最低价，默认为0
 * @return 盈亏数据值，如果没有找到返回0
 * 
 * 根据用户标签查找对应的持仓明细，返回其盈亏相关信息。
 */
double SelMocker::stra_get_detail_profit(const char* stdCode, const char* userTag, int flag /* = 0 */)
{
	auto it = _pos_map.find(stdCode);  // 查找持仓
	if (it == _pos_map.end())  // 如果不存在持仓
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	// 遍历持仓明细，查找匹配的标签
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;  // 跳过

		switch (flag)  // 根据标志返回相应数据
		{
		case 0:  // 当前盈亏
			return dInfo._profit;
		case 1:  // 最大盈利
			return dInfo._max_profit;
		case -1:  // 最大亏损
			return dInfo._max_loss;
		case 2:  // 最高价
			return dInfo._max_price;
		case -2:  // 最低价
			return dInfo._min_price;
		}
	}

	return 0.0;  // 如果没有找到，返回0
}