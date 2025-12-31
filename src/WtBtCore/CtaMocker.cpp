/*!
 * \file CtaMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略回测模拟器实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是CtaMocker类的实现文件，包含了CTA策略回测模拟器的所有功能实现。
 * 
 * 主要功能模块：
 * 1. 构造函数和析构函数：初始化回测环境，清理资源
 * 2. 数据接收处理：处理tick数据、K线数据，触发策略计算
 * 3. 交易信号处理：处理开仓、平仓、设置仓位等交易信号
 * 4. 仓位管理：维护持仓明细，计算盈亏，处理T+1规则
 * 5. 条件单处理：监控价格变化，触发条件单执行
 * 6. 资金统计：计算已实现盈亏、浮动盈亏、手续费等
 * 7. 结果输出：将回测结果输出到CSV和JSON文件
 * 8. 策略接口实现：为策略提供各种查询和操作接口
 * 
 * 核心算法：
 * - 滑点计算：支持固定滑点和比例滑点两种模式
 * - 条件单触发：支持tick模式和bar模式下的条件单触发
 * - 持仓管理：采用先进先出（FIFO）方式管理持仓明细
 * - 盈亏计算：实时计算浮动盈亏，平仓时计算已实现盈亏
 */
#include "CtaMocker.h"
#include "WtHelper.h"
#include "EventNotifier.h"

#include <exception>                  // 异常处理
#include <boost/filesystem.hpp>      // boost文件系统库，用于文件操作

#include "../Includes/WTSContractInfo.hpp"    // 合约信息定义
#include "../Includes/WTSSessionInfo.hpp"      // 交易时段信息定义
#include "../Includes/WTSVariant.hpp"          // 变体类型定义
#include "../Share/CodeHelper.hpp"            // 代码辅助工具
#include "../Share/decimal.h"                  // 精确小数计算
#include "../Share/StrUtil.hpp"                // 字符串工具

#include "../WTSTools/WTSLogger.h"            // 日志工具

#include <rapidjson/document.h>                // JSON文档解析
#include <rapidjson/prettywriter.h>            // JSON格式化输出
#include "rapidjson/filereadstream.h"          // JSON文件读取流
#include <fstream>                             // 文件流操作
namespace rj = rapidjson;                      // rapidjson命名空间别名

// ====================================================================
// 常量定义
// ====================================================================

/**
 * @brief 比较算法名称数组
 * 
 * 用于条件单日志输出，显示比较条件
 */
const char* CMP_ALG_NAMES[] =
{
	"＝",    // 等于
	">",     // 大于
	"<",     // 小于
	">=",    // 大于等于
	"<="     // 小于等于
};

/**
 * @brief 委托动作名称数组
 * 
 * 用于日志输出，显示委托动作类型
 */
const char* ACTION_NAMES[] =
{
	"OL",    // 开多（Open Long）
	"CL",    // 平多（Close Long）
	"OS",    // 开空（Open Short）
	"CS",    // 平空（Close Short）
	"SYN"    // 同步（Synchronize）
};


// ====================================================================
// 辅助函数
// ====================================================================

/**
 * @brief 生成上下文ID
 * 
 * 使用原子操作生成唯一的上下文ID
 * 
 * @return 唯一的上下文ID
 */
inline uint32_t makeCtxId()
{
	static std::atomic<uint32_t> _auto_context_id{ 1 };  // 静态原子变量，初始值为1
	return _auto_context_id.fetch_add(1);                // 原子递增并返回旧值
}


/**
 * @brief CtaMocker构造函数
 * 
 * 初始化回测模拟器的所有成员变量，设置回测参数
 * 
 * @param replayer 历史数据回放器指针
 * @param name 策略名称
 * @param slippage 滑点设置
 * @param persistData 是否持久化数据
 * @param notifier 事件通知器指针
 * @param isRatioSlp 是否为比例滑点
 */
CtaMocker::CtaMocker(HisDataReplayer* replayer, const char* name, int32_t slippage /* = 0 */, bool persistData /* = true */, EventNotifier* notifier /* = NULL */, bool isRatioSlp /* = false */)
	: ICtaStraCtx(name)                    // 调用基类构造函数，传入策略名称
	, _replayer(replayer)                  // 初始化历史数据回放器指针
	, _total_calc_time(0)                  // 初始化总计算时间为0
	, _emit_times(0)                        // 初始化计算次数为0
	, _is_in_schedule(false)                // 初始化调度状态为false
	, _ud_modified(false)                   // 初始化用户数据修改标志为false
	, _strategy(NULL)                        // 初始化策略指针为NULL
	, _slippage(slippage)                   // 初始化滑点设置
	, _ratio_slippage(isRatioSlp)           // 初始化比例滑点标志
	, _schedule_times(0)                    // 初始化调度次数为0
	, _total_closeprofit(0)                 // 初始化累计已实现盈亏为0
	, _notifier(notifier)                   // 初始化事件通知器指针
	, _has_hook(false)                      // 初始化钩子标志为false
	, _hook_valid(true)                     // 初始化钩子有效标志为true
	, _cur_step(0)                          // 初始化当前步骤为0
	, _wait_calc(false)                     // 初始化等待计算标志为false
	, _in_backtest(false)                   // 初始化回测状态为false
	, _persist_data(persistData)            // 初始化数据持久化标志
{
	_context_id = makeCtxId();              // 生成唯一的上下文ID
}


/**
 * @brief CtaMocker析构函数
 * 
 * 清理资源，释放策略实例和动态库
 */
CtaMocker::~CtaMocker()
{
}

/**
 * @brief 输出策略数据到JSON文件
 * 
 * 将持仓数据、资金信息、信号信息、条件单信息等保存到JSON文件，用于增量回测
 */
void CtaMocker::dump_stradata()
{
	rj::Document root(rj::kObjectType);  // 创建JSON根对象

	{//持仓数据保存
		rj::Value jPos(rj::kArrayType);  // 创建持仓数组

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)  // 遍历所有持仓
		{
			const char* stdCode = it->first.c_str();        // 获取合约代码
			const PosInfo& pInfo = it->second;             // 获取持仓信息

			rj::Value pItem(rj::kObjectType);               // 创建持仓项对象
			pItem.AddMember("code", rj::Value(stdCode, allocator), allocator);        // 添加合约代码
			pItem.AddMember("volume", pInfo._volume, allocator);                      // 添加持仓数量
			pItem.AddMember("closeprofit", pInfo._closeprofit, allocator);           // 添加已实现盈亏
			pItem.AddMember("dynprofit", pInfo._dynprofit, allocator);               // 添加浮动盈亏
			pItem.AddMember("lastentertime", pInfo._last_entertime, allocator);      // 添加最后开仓时间
			pItem.AddMember("lastexittime", pInfo._last_exittime, allocator);        // 添加最后平仓时间

			rj::Value details(rj::kArrayType);              // 创建持仓明细数组
			for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历持仓明细
			{
				const DetailInfo& dInfo = *dit;            // 获取明细信息
				rj::Value dItem(rj::kObjectType);           // 创建明细项对象
				dItem.AddMember("long", dInfo._long, allocator);                      // 添加是否多头
				dItem.AddMember("price", dInfo._price, allocator);                    // 添加开仓价格
				dItem.AddMember("maxprice", dInfo._max_price, allocator);            // 添加最高价
				dItem.AddMember("minprice", dInfo._min_price, allocator);            // 添加最低价
				dItem.AddMember("volume", dInfo._volume, allocator);                 // 添加持仓数量
				dItem.AddMember("opentime", dInfo._opentime, allocator);             // 添加开仓时间
				dItem.AddMember("opentdate", dInfo._opentdate, allocator);           // 添加开仓交易日

				dItem.AddMember("profit", dInfo._profit, allocator);                 // 添加浮动盈亏
				dItem.AddMember("maxprofit", dInfo._max_profit, allocator);          // 添加最大盈利
				dItem.AddMember("maxloss", dInfo._max_loss, allocator);              // 添加最大亏损
				dItem.AddMember("opentag", rj::Value(dInfo._opentag, allocator), allocator);  // 添加开仓标签

				details.PushBack(dItem, allocator);         // 将明细项添加到数组
			}

			pItem.AddMember("details", details, allocator);  // 将明细数组添加到持仓项
			jPos.PushBack(pItem, allocator);                 // 将持仓项添加到数组
		}

		root.AddMember("positions", jPos, allocator);       // 将持仓数组添加到根对象
	}

	{//资金保存
		rj::Value jFund(rj::kObjectType);                    // 创建资金对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		jFund.AddMember("total_profit", _fund_info._total_profit, allocator);        // 添加累计已实现盈亏
		jFund.AddMember("total_dynprofit", _fund_info._total_dynprofit, allocator); // 添加累计浮动盈亏
		jFund.AddMember("total_fees", _fund_info._total_fees, allocator);            // 添加累计手续费
		jFund.AddMember("tdate", _cur_tdate, allocator);                             // 添加当前交易日

		root.AddMember("fund", jFund, allocator);            // 将资金对象添加到根对象
	}

	{//信号保存
		rj::Value jSigs(rj::kObjectType);                    // 创建信号对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		for (auto& m : _sig_map)                            // 遍历所有信号
		{
			const char* stdCode = m.first.c_str();           // 获取合约代码
			const SigInfo& sInfo = m.second;                // 获取信号信息

			rj::Value jItem(rj::kObjectType);                // 创建信号项对象
			jItem.AddMember("usertag", rj::Value(sInfo._usertag.c_str(), allocator), allocator);  // 添加用户标签

			jItem.AddMember("volume", sInfo._volume, allocator);                      // 添加目标仓位
			jItem.AddMember("sigprice", sInfo._sigprice, allocator);                 // 添加信号价格
			jItem.AddMember("gentime", sInfo._gentime, allocator);                   // 添加信号生成时间

			jSigs.AddMember(rj::Value(stdCode, allocator), jItem, allocator);         // 将信号项添加到信号对象
		}

		root.AddMember("signals", jSigs, allocator);         // 将信号对象添加到根对象
	}

	{//条件单保存
		rj::Value jCond(rj::kObjectType);                    // 创建条件单对象
		rj::Value jItems(rj::kObjectType);                   // 创建条件单项对象

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		for (auto it = _condtions.begin(); it != _condtions.end(); it++)  // 遍历所有条件单
		{
			const char* code = it->first.c_str();            // 获取合约代码
			const CondList& condList = it->second;           // 获取条件单列表

			rj::Value cArray(rj::kArrayType);                // 创建条件单数组
			for (auto& condInfo : condList)                  // 遍历条件单列表
			{
				rj::Value cItem(rj::kObjectType);            // 创建条件单项对象
				cItem.AddMember("code", rj::Value(code, allocator), allocator);      // 添加合约代码
				cItem.AddMember("usertag", rj::Value(condInfo._usertag, allocator), allocator);  // 添加用户标签

				cItem.AddMember("field", (uint32_t)condInfo._field, allocator);      // 添加比较字段类型
				cItem.AddMember("alg", (uint32_t)condInfo._alg, allocator);          // 添加比较算法类型
				cItem.AddMember("target", condInfo._target, allocator);              // 添加目标价格
				cItem.AddMember("qty", condInfo._qty, allocator);                    // 添加委托数量
				cItem.AddMember("action", (uint32_t)condInfo._action, allocator);    // 添加委托动作

				cArray.PushBack(cItem, allocator);          // 将条件单项添加到数组
			}

			jItems.AddMember(rj::Value(code, allocator), cArray, allocator);         // 将条件单数组添加到项对象
		}
		jCond.AddMember("settime", _last_cond_min, allocator);                       // 添加条件单设置时间
		jCond.AddMember("items", jItems, allocator);                                 // 将条件单项添加到条件单对象

		root.AddMember("conditions", jCond, allocator);      // 将条件单对象添加到根对象
	}

	if(_persist_data)                                       // 如果需要持久化数据
	{
		std::string folder = WtHelper::getOutputDir();       // 获取输出目录
		folder += _name;                                     // 添加策略名称
		folder += "/";                                       // 添加路径分隔符

		if (!StdFile::exists(folder.c_str()))               // 如果目录不存在
			boost::filesystem::create_directories(folder.c_str());  // 创建目录

		std::string filename = folder;                       // 构建文件名
		filename += _name;                                  // 添加策略名称
		filename += ".json";                                // 添加JSON扩展名

		rj::StringBuffer sb;                                // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);      // 创建格式化写入器
		root.Accept(writer);                                 // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将缓冲区内容写入文件
	}
}

/**
 * @brief 输出图表数据到JSON和CSV文件
 * 
 * 将K线配置、指标配置、指标数据、交易标记等保存到文件
 */
void CtaMocker::dump_chartdata()
{
	rj::Document root(rj::kObjectType);                      // 创建JSON根对象
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

	rj::Value klineItem(rj::kObjectType);                    // 创建K线项对象
	if(_chart_code.empty())                                  // 如果没有设置图表K线
	{
		//如果没有设置主K线，就用主K线落地
		klineItem.AddMember("code", rj::Value(_main_code.c_str(), allocator), allocator);      // 使用主K线合约代码
		klineItem.AddMember("period", rj::Value(_main_period.c_str(), allocator), allocator);  // 使用主K线周期
	}
	else                                                      // 如果设置了图表K线
	{
		klineItem.AddMember("code", rj::Value(_chart_code.c_str(), allocator), allocator);     // 使用图表K线合约代码
		klineItem.AddMember("period", rj::Value(_chart_period.c_str(), allocator), allocator);  // 使用图表K线周期
	}

	root.AddMember("kline", klineItem, allocator);           // 将K线项添加到根对象

	if (!_chart_indice.empty())                              // 如果有指标数据
	{
		rj::Value jIndice(rj::kArrayType);                    // 创建指标数组
		for (const auto& v : _chart_indice)                   // 遍历所有指标
		{
			const ChartIndex& cIndex = v.second;              // 获取指标信息
			rj::Value jIndex(rj::kObjectType);                // 创建指标对象
			jIndex.AddMember("name", rj::Value(cIndex._name.c_str(), allocator), allocator);    // 添加指标名称
			jIndex.AddMember("index_type", cIndex._indexType, allocator);                       // 添加指标类型

			rj::Value jLines(rj::kArrayType);                 // 创建线条数组
			for(const auto& v2 : cIndex._lines)                // 遍历指标线条
			{
				const ChartLine& cLine = v2.second;           // 获取线条信息
				rj::Value jLine(rj::kObjectType);             // 创建线条对象
				jLine.AddMember("name", rj::Value(cLine._name.c_str(), allocator), allocator);  // 添加线条名称
				jLine.AddMember("line_type", cLine._lineType, allocator);                       // 添加线条类型

				//rj::Value jVals(rj::kArrayType);
				//for(const double& val : cLine._values)
				//{
				//	jVals.PushBack(val, allocator);
				//}

				//jLine.AddMember("values", jVals, allocator);

				jLines.PushBack(jLine, allocator);            // 将线条添加到数组
			}

			jIndex.AddMember("lines", jLines, allocator);     // 将线条数组添加到指标对象

			rj::Value jBaseLines(rj::kObjectType);            // 创建基准线对象
			for (const auto& v3 : cIndex._base_lines)         // 遍历基准线
			{
				jBaseLines.AddMember(rj::Value(v3.first.c_str(), allocator), rj::Value(v3.second), allocator);  // 添加基准线
			}

			jIndex.AddMember("baselines", jBaseLines, allocator);  // 将基准线对象添加到指标对象
			jIndice.PushBack(jIndex, allocator);              // 将指标对象添加到数组
		}

		root.AddMember("index", jIndice, allocator);          // 将指标数组添加到根对象
	}

	//if(!_chart_marks.empty())
	//{
	//	rj::Value jMarks(rj::kArrayType);
	//	for(const ChartMark& mark : _chart_marks)
	//	{
	//		rj::Value jMark(rj::kObjectType);
	//		jMark.AddMember("bartime", mark._bartime, allocator);
	//		jMark.AddMember("price", mark._price, allocator);
	//		jMark.AddMember("icon", rj::Value(mark._icon.c_str(), allocator), allocator);
	//		jMark.AddMember("tag", rj::Value(mark._tag.c_str(), allocator), allocator);

	//		jMarks.PushBack(jMark, allocator);
	//	}

	//	root.AddMember("marks", jMarks, allocator);
	//}

	if(_persist_data)                                         // 如果需要持久化数据
	{
		std::string folder = WtHelper::getOutputDir();       // 获取输出目录
		folder += _name;                                      // 添加策略名称
		folder += "/";                                        // 添加路径分隔符

		if(!StdFile::exists(folder.c_str()))                 // 如果目录不存在
			boost::filesystem::create_directories(folder.c_str());  // 创建目录

		std::string filename = folder;                        // 构建文件名
		filename += "btchart.json";                          // 添加图表JSON文件名

		rj::StringBuffer sb;                                  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);       // 创建格式化写入器
		root.Accept(writer);                                  // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将缓冲区内容写入文件

		filename = folder;                                    // 重置文件名
		filename += "indice.csv";                            // 添加指标CSV文件名
		std::string content = "bartime,index_name,line_name,value\n";  // CSV文件标题行
		if (!_index_logs.str().empty()) content += _index_logs.str();  // 添加指标日志内容
		StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入CSV文件

		filename = folder;                                    // 重置文件名
		filename += "marks.csv";                             // 添加标记CSV文件名
		content = "bartime,price,icon,tag\n";                // CSV文件标题行
		if (!_mark_logs.str().empty()) content += _mark_logs.str();    // 添加标记日志内容
		StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入CSV文件
	}
}

/**
 * @brief 输出回测结果到CSV文件
 * 
 * 将成交记录、平仓记录、资金曲线、信号记录、持仓记录等输出到CSV文件
 */
void CtaMocker::dump_outputs()
{
	if (!_persist_data)                                      // 如果不需要持久化数据
		return;                                               // 直接返回

	std::string folder = WtHelper::getOutputDir();           // 获取输出目录
	folder += _name;                                         // 添加策略名称
	folder += "/";                                           // 添加路径分隔符
	boost::filesystem::create_directories(folder.c_str());   // 创建目录（如果不存在）

	std::string filename = folder + "trades.csv";            // 构建成交记录文件名
	std::string content = "code,time,direct,action,price,qty,tag,fee,barno\n";  // CSV文件标题行
	if(!_trade_logs.str().empty()) content += _trade_logs.str();  // 添加成交日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "closes.csv";                         // 构建平仓记录文件名
	content = "code,direct,opentime,openprice,closetime,closeprice,qty,profit,maxprofit,maxloss,totalprofit,entertag,exittag,openbarno,closebarno\n";  // CSV文件标题行
	if (!_close_logs.str().empty()) content += _close_logs.str();  // 添加平仓日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "funds.csv";                          // 构建资金曲线文件名
	content = "date,closeprofit,positionprofit,dynbalance,fee\n";  // CSV文件标题行
	if (!_fund_logs.str().empty()) content += _fund_logs.str();    // 添加资金日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "signals.csv";                        // 构建信号记录文件名
	content = "code,target,sigprice,gentime,usertag\n";       // CSV文件标题行
	if (!_sig_logs.str().empty()) content += _sig_logs.str();      // 添加信号日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	filename = folder + "positions.csv";                      // 构建持仓记录文件名
	content = "date,code,volume,closeprofit,dynprofit\n";    // CSV文件标题行
	if (!_pos_logs.str().empty()) content += _pos_logs.str();        // 添加持仓日志内容
	StdFile::write_file_content(filename.c_str(), (void*)content.c_str(), content.size());  // 写入文件

	{                                                         // 保存用户数据到JSON文件
		rj::Document root(rj::kObjectType);                  // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器
		for (auto it = _user_datas.begin(); it != _user_datas.end(); it++)  // 遍历用户数据
		{
			root.AddMember(rj::Value(it->first.c_str(), allocator), rj::Value(it->second.c_str(), allocator), allocator);  // 添加键值对
		}

		filename = folder;                                    // 构建文件名
		filename += "ud_";                                    // 添加用户数据前缀
		filename += _name;                                    // 添加策略名称
		filename += ".json";                                  // 添加JSON扩展名

		rj::StringBuffer sb;                                  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);       // 创建格式化写入器
		root.Accept(writer);                                  // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将缓冲区内容写入文件
	}
}

/**
 * @brief 记录交易信号日志
 * 
 * @param stdCode 合约代码
 * @param target 目标仓位
 * @param price 信号价格
 * @param gentime 信号生成时间
 * @param usertag 用户标签
 */
void CtaMocker::log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag /* = "" */)
{
	_sig_logs << stdCode << "," << target << "," << price << "," << gentime << "," << usertag << "\n";  // 写入信号日志流
}

/**
 * @brief 记录成交日志
 * 
 * @param stdCode 合约代码
 * @param isLong 是否多头
 * @param isOpen 是否开仓
 * @param curTime 成交时间
 * @param price 成交价格
 * @param qty 成交数量
 * @param userTag 用户标签
 * @param fee 手续费
 * @param barNo K线编号
 */
void CtaMocker::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag, double fee, uint32_t barNo)
{
	_trade_logs << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE") 
		<< "," << price << "," << qty << "," << userTag << "," << fee << "," << barNo << "\n";  // 写入成交日志流
}

/**
 * @brief 记录平仓日志
 * 
 * @param stdCode 合约代码
 * @param isLong 是否多头
 * @param openTime 开仓时间
 * @param openpx 开仓价格
 * @param closeTime 平仓时间
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 盈亏
 * @param maxprofit 最大盈利
 * @param maxloss 最大亏损
 * @param totalprofit 累计盈亏
 * @param enterTag 开仓标签
 * @param exitTag 平仓标签
 * @param openBarNo 开仓K线编号
 * @param closeBarNo 平仓K线编号
 */
void CtaMocker::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double maxprofit, double maxloss, 
	double totalprofit /* = 0 */, const char* enterTag /* = "" */, const char* exitTag /* = "" */, uint32_t openBarNo /* = 0 */, uint32_t closeBarNo /* = 0 */)
{
	_close_logs << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
		<< "," << closeTime << "," << closepx << "," << qty << "," << profit << "," << maxprofit << "," << maxloss << ","
		<< totalprofit << "," << enterTag << "," << exitTag << "," << openBarNo << "," << closeBarNo << "\n";  // 写入平仓日志流
}

/**
 * @brief 初始化CTA策略工厂
 * 
 * 加载策略动态库，创建策略工厂实例和策略实例
 * 
 * @param cfg 配置对象，包含策略模块路径和策略参数
 * @return 是否初始化成功
 */
bool CtaMocker::init_cta_factory(WTSVariant* cfg)
{
	if (cfg == NULL)                                          // 如果配置为空
		return false;                                          // 返回失败

	const char* module = cfg->getCString("module");          // 获取策略模块路径

	DllHandle hInst = DLLHelper::load_library(module);       // 加载动态库
	if (hInst == NULL)                                        // 如果加载失败
		return false;                                          // 返回失败

	FuncCreateStraFact creator = (FuncCreateStraFact)DLLHelper::get_symbol(hInst, "createStrategyFact");  // 获取创建策略工厂的函数指针
	if (creator == NULL)                                      // 如果获取失败
	{
		DLLHelper::free_library(hInst);                       // 释放动态库
		return false;                                          // 返回失败
	}

	_factory._module_inst = hInst;                            // 保存动态库句柄
	_factory._module_path = module;                           // 保存模块路径
	_factory._creator = creator;                              // 保存创建函数指针
	_factory._remover = (FuncDeleteStraFact)DLLHelper::get_symbol(hInst, "deleteStrategyFact");  // 获取删除策略工厂的函数指针
	_factory._fact = _factory._creator();                     // 创建策略工厂实例

	WTSVariant* cfgStra = cfg->get("strategy");              // 获取策略配置
	if (cfgStra)                                              // 如果策略配置存在
	{
		_strategy = _factory._fact->createStrategy(cfgStra->getCString("name"), cfgStra->getCString("id"));  // 创建策略实例
		if(_strategy)                                          // 如果创建成功
		{
			WTSLogger::info("Strategy {}.{} is created,strategy ID: {}", _factory._fact->getName(), _strategy->getName(), _strategy->id());  // 记录日志
		}
		_strategy->init(cfgStra->get("params"));              // 初始化策略
		_name = _strategy->id();                               // 更新策略名称
	}

	return true;                                              // 返回成功
}

/**
 * @brief 加载增量回测数据
 * 
 * 从上次回测的结果文件中加载持仓、资金、信号等数据，用于增量回测
 * 加载数据 输出目录 为：{output_dir}/{incremental_backtest_base}：
 * - trades.csv 加载成交记录到 _trade_logs
 * - closes.csv 加载平仓记录到 _close_logs
 * - funds.csv 加载资金曲线到 _fund_logs
 * - positions.csv 加载持仓记录到 _pos_logs
 * - signals.csv 加载信号记录到 _sig_logs
 * - {incremental_backtest_base}.json 加载策略数据到 _strategy
 * - ud_*.json 加载条件单数据到 _condtions
 * 
 * @param incremental_backtest_base 上次回测的策略名称
 */
void CtaMocker::load_incremental_data(const char* incremental_backtest_base)
{
	std::string folder = WtHelper::getOutputDir();           // 获取输出目录
	folder += incremental_backtest_base;                     // 添加上次回测名称
	folder += "/";                                            // 添加路径分隔符
	WTSLogger::info("loading incremental data from: {}", folder);  // 记录日志

	std::string tradesFilename = folder + "trades.csv";       // 构建成交记录文件名
	if (boost::filesystem::exists(tradesFilename))            // 如果文件存在
	{
		std::ifstream tradesFile(tradesFilename);             // 打开文件
		std::string str;                                       // 临时字符串
		// 跳过标题行
		std::getline(tradesFile, str);                        // 读取并跳过标题行
		while (std::getline(tradesFile, str))                 // 逐行读取
		{
			_trade_logs << str << "\n";                       // 追加到成交日志流
		}
	}

	std::string closesFilename = folder + "closes.csv";       // 构建平仓记录文件名
	if (boost::filesystem::exists(closesFilename))            // 如果文件存在
	{
		std::ifstream closesFile(closesFilename);            // 打开文件
		std::string str;                                       // 临时字符串
		// 跳过标题行
		std::getline(closesFile, str);                        // 读取并跳过标题行
		while (std::getline(closesFile, str))                 // 逐行读取
		{
			_close_logs << str << "\n";                       // 追加到平仓日志流
		}
	}

	std::string fundsFilename = folder + "funds.csv";         // 构建资金曲线文件名
	if (boost::filesystem::exists(fundsFilename))             // 如果文件存在
	{
		std::ifstream fundsFile(fundsFilename);               // 打开文件
		std::string str;                                       // 临时字符串
		// 跳过标题行
		std::getline(fundsFile, str);                         // 读取并跳过标题行
		while (std::getline(fundsFile, str))                  // 逐行读取
		{
			_fund_logs << str << "\n";                        // 追加到资金日志流
		}
	}

	std::string positionsFilename = folder + "positions.csv";  // 构建持仓记录文件名
	if (boost::filesystem::exists(positionsFilename))          // 如果文件存在
	{
		std::ifstream positionsFile(positionsFilename);       // 打开文件
		std::string str;                                       // 临时字符串
		// 跳过标题行
		std::getline(positionsFile, str);                     // 读取并跳过标题行
		while (std::getline(positionsFile, str))              // 逐行读取
		{
			_pos_logs << str << "\n";                         // 追加到持仓日志流
		}
	}

	std::string signalsFilename = folder + "signals.csv";      // 构建信号记录文件名
	if (boost::filesystem::exists(signalsFilename))           // 如果文件存在
	{
		std::ifstream signalsFile(signalsFilename);           // 打开文件
		std::string str;                                       // 临时字符串
		// 跳过标题行
		std::getline(signalsFile, str);                       // 读取并跳过标题行
		while (std::getline(signalsFile, str))                // 逐行读取
		{
			_sig_logs << str << "\n";                         // 追加到信号日志流
		}
	}

	std::string strategyDumpFilename = folder + fmtutil::format("{}.json", incremental_backtest_base);  // 构建策略数据JSON文件名
	if (boost::filesystem::exists(strategyDumpFilename))      // 如果文件存在
	{
		WTSLogger::info("load incremental data json: {}", strategyDumpFilename);  // 记录日志
		FILE* fp = fopen(strategyDumpFilename.c_str(), "rb");  // 打开文件
		char readBuffer[65536];                                // 读取缓冲区
		rj::FileReadStream strategyDumpFile(fp, readBuffer, sizeof(readBuffer));  // 创建文件读取流
		rj::Document d;                                        // 创建JSON文档
		d.ParseStream(strategyDumpFile);                       // 解析JSON文档
		fclose(fp);                                            // 关闭文件
		if (d.HasMember("positions"))                          // 如果有持仓数据
		{
			const rj::Value& positions = d["positions"];       // 获取持仓数组
			for (rj::SizeType i = 0; i < positions.Size(); i++)  // 遍历持仓数组
			{
				const rj::Value& positionEntry = positions[i];  // 获取持仓项
				const char* positionEntry_code = positionEntry["code"].GetString();  // 获取合约代码
				PosInfo& pInfo = _pos_map[positionEntry_code];   // 获取或创建持仓信息
				pInfo._volume = positionEntry["volume"].GetDouble();              // 恢复持仓数量
				pInfo._closeprofit = positionEntry["closeprofit"].GetDouble();     // 恢复已实现盈亏
				pInfo._dynprofit = positionEntry["dynprofit"].GetDouble();        // 恢复浮动盈亏
				pInfo._last_entertime = positionEntry["lastentertime"].GetUint64();  // 恢复最后开仓时间
				pInfo._last_exittime = positionEntry["lastexittime"].GetUint64();   // 恢复最后平仓时间

				if (positionEntry.HasMember("details"))        // 如果有持仓明细
				{
					const rj::Value& details = positionEntry["details"];  // 获取明细数组
					for (rj::SizeType j = 0; j < details.Size(); j++)      // 遍历明细数组
					{
						const rj::Value& positionDetailEntry = details[j];  // 获取明细项
						DetailInfo curPosDetail;                              // 创建明细信息对象
						curPosDetail._long = positionDetailEntry["long"].GetBool();          // 恢复是否多头
						curPosDetail._price = positionDetailEntry["price"].GetDouble();       // 恢复开仓价格
						curPosDetail._max_price = positionDetailEntry["maxprice"].GetDouble();  // 恢复最高价
						curPosDetail._min_price = positionDetailEntry["minprice"].GetDouble();  // 恢复最低价
						curPosDetail._volume = positionDetailEntry["volume"].GetDouble();      // 恢复持仓数量
						curPosDetail._opentime = positionDetailEntry["opentime"].GetUint64();  // 恢复开仓时间
						curPosDetail._opentdate = positionDetailEntry["opentdate"].GetInt();   // 恢复开仓交易日
						curPosDetail._profit = positionDetailEntry["profit"].GetDouble();      // 恢复浮动盈亏
						curPosDetail._max_profit = positionDetailEntry["maxprofit"].GetDouble();  // 恢复最大盈利
						curPosDetail._max_loss = positionDetailEntry["maxloss"].GetDouble();     // 恢复最大亏损
						strcpy(curPosDetail._opentag, positionDetailEntry["opentag"].GetString());  // 恢复开仓标签
						pInfo._details.push_back(curPosDetail);                // 添加到持仓明细列表
					}
				}
			}
		}

		if (d.HasMember("fund"))                              // 如果有资金数据
		{
			_fund_info._total_profit = d["fund"]["total_profit"].GetDouble();       // 恢复累计已实现盈亏
			_fund_info._total_dynprofit = d["fund"]["total_dynprofit"].GetDouble(); // 恢复累计浮动盈亏
			_fund_info._total_fees = d["fund"]["total_fees"].GetDouble();           // 恢复累计手续费
		}

		if (d.HasMember("signals"))                           // 如果有信号数据
		{
			for (rj::Value::ConstMemberIterator itr = d["signals"].MemberBegin(); itr != d["signals"].MemberEnd(); ++itr)  // 遍历信号对象
			{
				std::string stkCode = itr->name.GetString();   // 获取合约代码
				SigInfo& sInfo = _sig_map[stkCode];            // 获取或创建信号信息
				sInfo._usertag = itr->value["usertag"].GetString();    // 恢复用户标签
				sInfo._volume = itr->value["volume"].GetDouble();      // 恢复目标仓位
				sInfo._sigprice = itr->value["sigprice"].GetDouble();  // 恢复信号价格
				sInfo._gentime = itr->value["gentime"].GetUint64();   // 恢复信号生成时间
			}
		}

		if (d.HasMember("conditions") && d["conditions"].HasMember("items"))  // 如果有条件单数据
		{
			// conditions -> items 下面的内容是两层嵌套   items[CODE] is a list
			rj::Value& conditionItemsEntry = d["conditions"]["items"];  // 获取条件单项对象

			for (rj::Value::ConstMemberIterator itr = conditionItemsEntry.MemberBegin(); itr != conditionItemsEntry.MemberEnd(); ++itr)  // 遍历条件单项
			{
				std::string stkCode = itr->name.GetString();   // 获取合约代码
				for (rj::SizeType i = 0; i < itr->value.Size(); i++)  // 遍历条件单列表
				{
					const rj::Value& conditionItemStkCondEntry = itr->value[i];  // 获取条件单项
					CondEntrust condEntrust;                    // 创建条件单对象
					strcpy(condEntrust._usertag, conditionItemStkCondEntry["usertag"].GetString());  // 恢复用户标签
					condEntrust._field = (WTSCompareField)conditionItemStkCondEntry["field"].GetInt();  // 恢复比较字段类型
					condEntrust._alg = (WTSCompareType)conditionItemStkCondEntry["alg"].GetInt();     // 恢复比较算法类型
					condEntrust._target = conditionItemStkCondEntry["target"].GetDouble();            // 恢复目标价格
					condEntrust._qty = conditionItemStkCondEntry["qty"].GetDouble();                  // 恢复委托数量
					condEntrust._action = (char)conditionItemStkCondEntry["action"].GetUint();        // 恢复委托动作

					_condtions[stkCode].push_back(condEntrust);  // 添加到条件单列表
				}
			}
		}
	}
	else                                                       // 如果文件不存在
	{
		WTSLogger::warn("fail load incremental data json: {}", strategyDumpFilename);  // 记录警告日志
	}
}

//////////////////////////////////////////////////////////////////////////
//IDataSink接口实现
// ====================================================================

/**
 * @brief 处理初始化事件
 * 
 * 当历史数据回放器初始化时调用
 */
void CtaMocker::handle_init()
{
	this->on_init();                                          // 调用策略初始化回调
}

/**
 * @brief 处理K线收盘事件
 * 
 * @param stdCode 合约代码
 * @param period 周期
 * @param times 周期倍数
 * @param newBar 新的K线数据
 */
void CtaMocker::handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	this->on_bar(stdCode, period, times, newBar);            // 调用K线回调
}

/**
 * @brief 处理调度事件
 * 
 * @param uDate 日期
 * @param uTime 时间
 */
void CtaMocker::handle_schedule(uint32_t uDate, uint32_t uTime)
{
	this->on_schedule(uDate, uTime);                         // 调用调度回调
}

/**
 * @brief 处理交易时段开始事件
 * 
 * @param curTDate 当前交易日
 */
void CtaMocker::handle_session_begin(uint32_t curTDate)
{
	this->on_session_begin(curTDate);                         // 调用交易时段开始回调
}

/**
 * @brief 处理交易时段结束事件
 * 
 * @param curTDate 当前交易日
 */
void CtaMocker::handle_session_end(uint32_t curTDate)
{
	this->on_session_end(curTDate);                          // 调用交易时段结束回调
}

/**
 * @brief 处理小节结束事件
 * 
 * 如果小节结束，也需要清理掉价格缓存，防止小节跳空
 * 这种主要是针对夜盘交易
 * 
 * @param curTDate 当前交易日
 * @param curTime 当前时间
 */
void CtaMocker::handle_section_end(uint32_t curTDate, uint32_t curTime)
{
	/*
	 *	By Wesley @ 2022.05.16
	 *	如果小节结束，也需要清理掉价格缓存，防止小节跳空
	 *	这种主要是针对夜盘交易
	 */
	_price_map.clear();                                       // 清空价格缓存
}

/**
 * @brief 处理回放完成事件
 * 
 * 回放完成后，输出回测结果，通知策略回测结束
 */
void CtaMocker::handle_replay_done()
{
	_in_backtest = false;                                     // 设置回测状态为false

	if(_emit_times > 0)                                       // 如果计算次数大于0
	{
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_INFO, 
			"Strategy has been scheduled {} times, totally taking {} us, {:.3f} us each time",
			_emit_times, _total_calc_time, _total_calc_time*1.0 / _emit_times);  // 记录平均计算时间
	}
	else                                                      // 如果计算次数为0
	{
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_INFO, 
			"Strategy has been scheduled for {} times", _emit_times);  // 记录计算次数
	}

	dump_outputs();                                           // 输出回测结果到CSV文件

	dump_stradata();                                          // 输出策略数据到JSON文件

	dump_chartdata();                                         // 输出图表数据到文件

	if (_has_hook && _hook_valid)                            // 如果有钩子且钩子有效
	{
		WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, "Replay done, notify control thread");  // 记录日志
		while(_wait_calc)                                     // 如果等待计算
			_cond_calc.notify_all();                          // 通知所有等待的线程
		WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, "Notify control thread the end done");  // 记录日志
	}

	WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Notify strategy the end of backtest");  // 记录日志
	this->on_bactest_end();                                  // 调用策略回测结束回调
}

/**
 * @brief 处理tick数据
 * 
 * 处理交易信号触发、条件单检查和价格更新
 * 
 * @param stdCode 合约代码
 * @param last_px 上一笔价格
 * @param cur_px 当前价格
 */
void CtaMocker::proc_tick(const char* stdCode, double last_px, double cur_px)
{
	{                                                         // 处理交易信号
		auto it = _sig_map.find(stdCode);                    // 查找该合约的信号
		if (it != _sig_map.end())                            // 如果找到信号
		{
			//if (sInfo->isInTradingTime(_replayer->get_raw_time(), true))
			{
				const SigInfo& sInfo = it->second;            // 获取信号信息
				double price;                                  // 成交价格
				if (decimal::eq(sInfo._desprice, 0.0))        // 如果指定价格为0（使用市场价格）
					price = cur_px;                            // 使用当前价格
				else                                          // 如果指定价格不为0
					price = sInfo._desprice;                  // 使用指定价格
				do_set_position(stdCode, sInfo._volume, price, sInfo._usertag.c_str());  // 设置仓位

				//如果是条件单触发，则回调on_condition_triggered
				if (sInfo._sigtype == 2)                      // 如果是条件单触发的信号
					on_condition_triggered(stdCode, sInfo._volume, cur_px, sInfo._usertag.c_str());  // 调用条件单触发回调
				_sig_map.erase(it);                            // 删除已处理的信号
			}
		}
	}

	update_dyn_profit(stdCode, cur_px);                       // 更新浮动盈亏

	//////////////////////////////////////////////////////////////////////////
	//检查条件单
	if (!_condtions.empty())                                  // 如果有条件单
	{
		auto it = _condtions.find(stdCode);                   // 查找该合约的条件单
		if (it == _condtions.end())                           // 如果没找到
			return;                                            // 直接返回

		const CondList& condList = it->second;                // 获取条件单列表
		double curPrice = cur_px;                             // 当前价格
		const CondEntrust* matchedEntrust = NULL;             // 匹配的条件单
		for (const CondEntrust& entrust : condList)            // 遍历条件单列表
		{
			/*
			 * 如果开启了tick模式，就正常比较
			 * 但是如果没有开启tick模式，逻辑就非常复杂
			 * 因为不开回测的时候tick是用开高低收模拟出来的，如果直接按照目标价格触发，可能是有问题的
			 * 首先要拿到上一笔价格，和当前最新价格做一个比价，得到左边界和右边界
			 * 这里只能假设前后两笔价格之间是连续的，这样需要将两笔价格都加入判断
			 * 当条件是等于时，如果目标价格在左右边界之间，说明目标价格在这期间是出现过的，则认为价格匹配
			 * 当条件是大于的时候，我们需要判断右边界，即稍大的值是否满足条件，并取左边界与目标价中稍大的作为当前价
			 * 当条件是小于的时候，我们需要判断左边界，即稍小的值是否满足条件，并取右边界与目标价中稍小的作为当前价
			 */

			double left_px = min(last_px, cur_px);            // 左边界价格（较小值）
			double right_px = max(last_px, cur_px);           // 右边界价格（较大值）

			bool isMatched = false;                           // 是否匹配
			if (!_replayer->is_tick_simulated())             // 如果tick数据不是模拟的
			{
				//如果tick数据不是模拟的，则使用最新价格
				switch (entrust._alg)                         // 根据比较算法类型判断
				{
				case WCT_Equal:                                // 等于
					isMatched = decimal::eq(curPrice, entrust._target);
					break;
				case WCT_Larger:                               // 大于
					isMatched = decimal::gt(curPrice, entrust._target);
					break;
				case WCT_LargerOrEqual:                        // 大于等于
					isMatched = decimal::ge(curPrice, entrust._target);
					break;
				case WCT_Smaller:                              // 小于
					isMatched = decimal::lt(curPrice, entrust._target);
					break;
				case WCT_SmallerOrEqual:                       // 小于等于
					isMatched = decimal::le(curPrice, entrust._target);
					break;
				default:
					break;
				}

				if (isMatched)                                // 如果匹配
				{
					matchedEntrust = &entrust;                // 记录匹配的条件单
					break;                                     // 退出循环
				}
			}
			else                                              // 如果tick数据是模拟的
			{
				//如果tick数据是模拟的，则要处理一下
				switch (entrust._alg)                         // 根据比较算法类型判断
				{
				case WCT_Equal:                                // 等于：目标价格在左右边界之间
					isMatched = decimal::le(left_px, entrust._target) && decimal::ge(right_px, entrust._target);
					break;
				case WCT_Larger:                               // 大于：右边界大于目标价格
					isMatched = decimal::gt(right_px, entrust._target);
					break;
				case WCT_LargerOrEqual:                        // 大于等于：右边界大于等于目标价格
					isMatched = decimal::ge(right_px, entrust._target);
					break;
				case WCT_Smaller:                              // 小于：左边界小于目标价格
					isMatched = decimal::lt(left_px, entrust._target);
					break;
				case WCT_SmallerOrEqual:                       // 小于等于：左边界小于等于目标价格
					isMatched = decimal::le(left_px, entrust._target);
					break;
				default:
					break;
				}

				if (isMatched)                                // 如果匹配
				{
					/*
					* By HeJ @ 2023.02.27
					* 在bar回测中，经常会出现同一个价格触发了多个条件单时，要选出一个作为最终的触发价，遵循以下规则：
					* 1 alg不同的条件单，或者alg为WCT_Equal，以最先设置的那个为准
					* 2 alg一样的调价单，如果是WCT_Larger与WCT_LargerOrEqual，取触发价较小的，WCT_Smaller与WCT_SmallerOrEqual，取触发价较大的
					*/
					if (matchedEntrust == NULL)               // 如果还没有匹配的条件单
					{
						matchedEntrust = &entrust;            // 记录匹配的条件单
						if (entrust._alg == WCT_Larger || entrust._alg == WCT_LargerOrEqual)  // 如果是大于或大于等于
							curPrice = max(left_px, entrust._target);  // 取左边界和目标价格中的较大值
						else if (entrust._alg == WCT_Smaller || entrust._alg == WCT_SmallerOrEqual)  // 如果是小于或小于等于
							curPrice = min(right_px, entrust._target);  // 取右边界和目标价格中的较小值
						else                                   // 如果是等于
							curPrice = entrust._target;         // 使用目标价格
					}
					else if (matchedEntrust->_alg == entrust._alg)  // 如果算法类型相同
					{
						if (entrust._alg == WCT_Larger || entrust._alg == WCT_LargerOrEqual)  // 如果是大于或大于等于
						{
							if (entrust._target < matchedEntrust->_target)  // 如果当前目标价格更小
							{
								matchedEntrust = &entrust;     // 更新匹配的条件单
								curPrice = max(left_px, entrust._target);   // 更新成交价格
							}
						}
						else if (entrust._alg == WCT_Smaller || entrust._alg == WCT_SmallerOrEqual)  // 如果是小于或小于等于
						{
							if (entrust._target > matchedEntrust->_target)  // 如果当前目标价格更大
							{
								matchedEntrust = &entrust;     // 更新匹配的条件单
								curPrice = min(right_px, entrust._target);   // 更新成交价格
							}
						}
					}
				}
			}
		}

		if (matchedEntrust != NULL)                           // 如果有匹配的条件单
		{
			const CondEntrust& entrust = *matchedEntrust;     // 获取匹配的条件单
			double price = curPrice;                           // 成交价格
			double curQty = stra_get_position(stdCode);       // 获取当前持仓
			//_replayer->is_tick_enabled() ? newTick->price() : entrust._target;	//如果开启了tick回测,则用tick数据的价格,如果没有开启,则只能用条件单价格
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_INFO,
				"Condition order triggered[newprice: {}{}{}], instrument: {}, {} {}",
				curPrice, CMP_ALG_NAMES[entrust._alg], entrust._target, stdCode, ACTION_NAMES[entrust._action], entrust._qty);  // 记录日志
			switch (entrust._action)                           // 根据委托动作处理
			{
			case COND_ACTION_OL:                               // 开多
			{
				if (decimal::lt(curQty, 0))                    // 如果当前是空仓
					append_signal(stdCode, entrust._qty, entrust._usertag, price, 2);  // 直接开多
				else                                           // 如果当前是多仓或空仓
					append_signal(stdCode, curQty + entrust._qty, entrust._usertag, price, 2);  // 增加多仓
			}
			break;
			case COND_ACTION_CL:                               // 平多
			{
				double maxQty = min(curQty, entrust._qty);     // 取当前持仓和委托数量中的较小值
				append_signal(stdCode, curQty - maxQty, entrust._usertag, price, 2);  // 减少多仓
			}
			break;
			case COND_ACTION_OS:                              // 开空
			{
				if (decimal::gt(curQty, 0))                    // 如果当前是多仓
					append_signal(stdCode, -entrust._qty, entrust._usertag, price, 2);  // 直接开空
				else                                           // 如果当前是空仓或多仓
					append_signal(stdCode, curQty - entrust._qty, entrust._usertag, price, 2);  // 增加空仓
			}
			break;
			case COND_ACTION_CS:                               // 平空
			{
				double maxQty = min(abs(curQty), entrust._qty);  // 取当前空仓绝对值和委托数量中的较小值
				append_signal(stdCode, curQty + maxQty, entrust._usertag, price, 2);  // 减少空仓
			}
			break;
			case COND_ACTION_SP:                               // 同步仓位
			{
				append_signal(stdCode, entrust._qty, entrust._usertag, price, 2);  // 直接设置目标仓位
			}
			default: break;
			}

			//同一个bar设置针对同一个合约的条件单,只可能触发一条
			//所以这里直接清理掉即可
			_condtions.erase(it);                              // 删除该合约的所有条件单
		}
	}
}


/**
 * @brief 处理tick数据
 * 
 * 更新价格缓存，处理交易信号和条件单，触发策略的tick回调
 * 
 * @param stdCode 合约代码
 * @param newTick 新的tick数据
 * @param pxType 价格类型：0-正常tick，3-收盘价模拟的tick
 */
void CtaMocker::handle_tick(const char* stdCode, WTSTickData* newTick, uint32_t pxType /* = 0 */)
{
	double cur_px = newTick->price();                         // 获取当前价格

	/*
	 *	By Wesley @ 2022.04.19
	 *	这里的逻辑改了一下
	 *	如果缓存的价格不存在，则上一笔价格就用最新价
	 *	这里主要是为了应对跨日价格跳空的情况
	 */
	double last_px = cur_px;                                   // 上一笔价格，默认使用当前价格
	if(pxType != 0)                                            // 如果价格类型不为0（需要检查价格缓存）
	{
		auto it = _price_map.find(stdCode);                    // 查找价格缓存
		if (it != _price_map.end())                            // 如果找到缓存的价格
			last_px = it->second;                              // 使用缓存的价格
		else                                                    // 如果没找到缓存的价格
			last_px = cur_px;                                   // 使用当前价格（应对跨日跳空）
	}
	
	
	_price_map[stdCode] = cur_px;                              // 更新价格缓存
	_ticks[stdCode] = newTick->getTickStruct();                // 更新tick缓存

	//先检查是否要信号要触发
	//By Wesley @ 2022.04.19
	//虽然这段逻辑下面也根据isBarEnd复制了一段
	//但是这一段还是要保留
	proc_tick(stdCode, last_px, cur_px);                       // 处理tick数据（信号和条件单）

	on_tick_updated(stdCode, newTick);                         // 调用tick更新回调

	/*
	 *	By Wesley @ 2022.04.19
	 *	isBarEnd，如果是逐tick回放，这个永远都是true，永远也不会触发下面这段逻辑
	 *	如果是模拟的tick数据，用收盘价模拟tick的时候，isBarEnd才会为true
	 *	如果不是收盘价模拟的tick，那么直接在当前tick触发撮合逻辑
	 *	这样做的目的是为了让在模拟tick触发的ontick中下单的信号能够正常处理
	 *	而不至于在回测的时候成交价偏离太远
	 */
	if(pxType != 3)                                            // 如果不是收盘价模拟的tick（类型3）
		proc_tick(stdCode, last_px, cur_px);                   // 再次处理tick数据（确保信号能够及时触发）
}


//////////////////////////////////////////////////////////////////////////
//回调函数
// ====================================================================

/**
 * @brief 处理K线数据
 * 
 * 当K线收盘时调用，标记K线已收盘，并触发策略的K线收盘回调
 * 
 * @param stdCode 合约代码
 * @param period 周期
 * @param times 周期倍数
 * @param newBar 新的K线数据
 */
void CtaMocker::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (newBar == NULL)                                        // 如果K线数据为空
		return;                                                 // 直接返回

	thread_local static char realPeriod[8] = { 0 };           // 线程局部变量：实际周期字符串
	fmtutil::format_to(realPeriod, "{}{}", period, times);    // 格式化周期字符串（周期+倍数）

	thread_local static char key[64] = { 0 };                // 线程局部变量：K线键值
	fmtutil::format_to(key, "{}#{}", stdCode, realPeriod);    // 格式化K线键值（合约代码#周期）

	KlineTag& tag = _kline_tags[key];                         // 获取或创建K线标签
	tag._closed = true;                                        // 标记K线已收盘

	if(tag._notify)                                            // 如果需要通知策略
		on_bar_close(stdCode, realPeriod, newBar);             // 调用K线收盘回调
}

/**
 * @brief 策略初始化回调
 * 
 * 清空tick缓存，设置回测状态，调用策略的初始化方法
 */
void CtaMocker::on_init()
{
	_ticks.clear();                                             // 清空tick缓存
	_in_backtest = true;                                       // 设置回测状态为true
	if (_strategy)                                             // 如果策略实例存在
		_strategy->on_init(this);                              // 调用策略的初始化方法

	WTSLogger::info("CTA Strategy initialized with {} slippage: {}", _ratio_slippage?"ratio":"absolute", _slippage);  // 记录日志
}

/**
 * @brief 更新浮动盈亏
 * 
 * 根据当前价格更新持仓的浮动盈亏
 * 
 * @param stdCode 合约代码
 * @param price 当前价格
 */
void CtaMocker::update_dyn_profit(const char* stdCode, double price)
{
	auto it = _pos_map.find(stdCode);                          // 查找持仓
	if (it != _pos_map.end())                                  // 如果找到持仓
	{
		PosInfo& pInfo = (PosInfo&)it->second;                // 获取持仓信息
		if (pInfo._volume == 0)                                // 如果持仓数量为0
		{
			pInfo._dynprofit = 0;                              // 浮动盈亏为0
		}
		else                                                    // 如果持仓数量不为0
		{
			WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
			double dynprofit = 0;                               // 总浮动盈亏
			for (auto pit = pInfo._details.begin(); pit != pInfo._details.end(); pit++)  // 遍历持仓明细
			{
				DetailInfo& dInfo = *pit;                      // 获取明细信息
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);  // 计算该笔持仓的浮动盈亏
				if (dInfo._profit > 0)                         // 如果盈利
					dInfo._max_profit = max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利
				else if (dInfo._profit < 0)                    // 如果亏损
					dInfo._max_loss = min(dInfo._profit, dInfo._max_loss);     // 更新最大亏损

				dInfo._max_price = std::max(dInfo._max_price, price);  // 更新最高价
				dInfo._min_price = std::min(dInfo._min_price, price);  // 更新最低价

				dynprofit += dInfo._profit;                    // 累加浮动盈亏
			}

			pInfo._dynprofit = dynprofit;                      // 更新持仓的浮动盈亏
		}
	}

	double total_dynprofit = 0;                                // 总浮动盈亏
	for (auto& v : _pos_map)                                   // 遍历所有持仓
	{
		const PosInfo& pInfo = v.second;                       // 获取持仓信息
		total_dynprofit += pInfo._dynprofit;                   // 累加浮动盈亏
	}

	_fund_info._total_dynprofit = total_dynprofit;              // 更新总浮动盈亏
}

/**
 * @brief 处理tick数据（已废弃）
 * 
 * 这个逻辑全部迁移到handle_tick里去了
 * 
 * @param stdCode 合约代码
 * @param newTick 新的tick数据
 * @param bEmitStrategy 是否触发策略（已废弃）
 */
void CtaMocker::on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy /* = true */)
{
	//这个逻辑全部迁移到handle_tick里去了
}

/**
 * @brief K线收盘回调
 * 
 * 当K线收盘时调用策略的on_bar方法
 * 
 * @param code 合约代码
 * @param period 周期
 * @param newBar 新的K线数据
 */
void CtaMocker::on_bar_close(const char* code, const char* period, WTSBarStruct* newBar)
{
	if (_strategy)                                             // 如果策略实例存在
		_strategy->on_bar(this, code, period, newBar);          // 调用策略的K线回调
}

/**
 * @brief tick更新回调
 * 
 * 当tick数据更新时，如果策略订阅了该合约的tick，则调用策略的on_tick方法
 * 
 * @param code 合约代码
 * @param newTick 新的tick数据
 */
void CtaMocker::on_tick_updated(const char* code, WTSTickData* newTick)
{
	auto it = _tick_subs.find(code);                           // 查找是否订阅了该合约的tick
	if (it == _tick_subs.end())                                // 如果没有订阅
		return;                                                 // 直接返回

	if (_strategy)                                             // 如果策略实例存在
		_strategy->on_tick(this, code, newTick);               // 调用策略的tick回调
}

/**
 * @brief 策略计算回调
 * 
 * 调用策略的on_schedule方法进行策略计算
 * 
 * @param curDate 当前日期
 * @param curTime 当前时间
 */
void CtaMocker::on_calculate(uint32_t curDate, uint32_t curTime)
{
	if (_strategy)                                             // 如果策略实例存在
		_strategy->on_schedule(this, curDate, curTime);         // 调用策略的调度回调
}

/**
 * @brief 启用/禁用计算钩子
 * 
 * @param bEnabled 是否启用
 */
void CtaMocker::enable_hook(bool bEnabled /* = true */)
{
	_hook_valid = bEnabled;                                    // 设置钩子有效标志

	WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calculating hook {}", bEnabled?"enabled":"disabled");  // 记录日志
}

/**
 * @brief 安装计算钩子
 * 
 * 标记已安装钩子，用于异步回测模式
 */
void CtaMocker::install_hook()
{
	_has_hook = true;                                           // 设置钩子已安装标志

	WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "CTA hook installed");  // 记录日志
}

/**
 * @brief 步骤计算（异步回测模式）
 * 
 * 在异步回测模式下，等待策略计算完成
 * 总共分为4个状态：0-初始状态，1-oncalc，2-oncalc结束，3-oncalcdone
 * 
 * @return 是否成功等待计算完成
 */
bool CtaMocker::step_calc()
{
	if (!_has_hook)                                            // 如果没有安装钩子
	{
		return false;                                          // 返回失败
	}

	//总共分为4个状态
	//0-初始状态，1-oncalc，2-oncalc结束，3-oncalcdone
	//所以，如果处于0/2，则说明没有在执行中，需要notify
	bool bNotify = false;                                      // 是否已通知
	while (_in_backtest && (_cur_step == 0 || _cur_step == 2))  // 如果回测中且处于初始或计算结束状态
	{
		_cond_calc.notify_all();                               // 通知所有等待的线程
		bNotify = true;                                         // 标记已通知
	}

	if(bNotify)                                                 // 如果已通知
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Notify calc thread, wait for calc done");  // 记录日志

	if(_in_backtest)                                           // 如果回测中
	{
		_wait_calc = true;                                     // 设置等待计算标志
		StdUniqueLock lock(_mtx_calc);                        // 获取互斥锁
		_cond_calc.wait(_mtx_calc);                            // 等待计算完成通知
		_wait_calc = false;                                    // 清除等待计算标志
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc done notified");  // 记录日志
		_cur_step = (_cur_step + 1) % 4;                      // 更新状态（状态机循环）

		return true;                                            // 返回成功
	}
	else                                                        // 如果回测已结束
	{
		_hook_valid = false;                                   // 禁用钩子
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Backtest exit automatically");  // 记录日志
		return false;                                           // 返回失败
	}
}

/**
 * @brief 调度回调
 * 
 * 当主K线收盘时触发策略计算，处理条件单清理和异步回测控制
 * 
 * @param curDate 当前日期
 * @param curTime 当前时间
 * @return 是否触发了策略计算
 */
bool CtaMocker::on_schedule(uint32_t curDate, uint32_t curTime)
{
	_is_in_schedule = true;//开始调度,修改标记                   // 设置调度状态为true

	_schedule_times++;                                         // 增加调度次数

	bool isMainUdt = false;                                   // 主K线是否更新
	bool emmited = false;                                     // 是否已触发策略计算

	for (auto it = _kline_tags.begin(); it != _kline_tags.end(); it++)  // 遍历K线标签
	{
		const std::string& key = it->first;                    // 获取K线键值
		KlineTag& marker = (KlineTag&)it->second;             // 获取K线标签

		StringVector ay = StrUtil::split(key, "#");            // 分割键值（合约代码#周期）
		const char* stdCode = ay[0].c_str();                   // 获取合约代码

		if (key == _main_key)                                  // 如果是主K线
		{
			if (marker._closed)                                // 如果K线已收盘
			{
				isMainUdt = true;                              // 标记主K线已更新
				marker._closed = false;                        // 重置收盘标志
			}
			else                                                // 如果K线未收盘
			{
				isMainUdt = false;                             // 标记主K线未更新
				break;                                          // 退出循环
			}
		}

		WTSSessionInfo* sInfo = _replayer->get_session_info(stdCode, true);  // 获取交易时段信息

		if (isMainUdt || _kline_tags.empty())                 // 如果主K线已更新或没有K线标签
		{
			TimeUtils::Ticker ticker;                          // 创建计时器

			uint32_t offTime = sInfo->offsetTime(curTime, true);  // 获取偏移时间
			if (offTime <= sInfo->getCloseTime(true))          // 如果在交易时间内
			{
				_condtions.clear();                            // 清空条件单（每个bar只保留一次）
				if(_has_hook && _hook_valid)                   // 如果有钩子且钩子有效
				{
					WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Waiting for resume notify");  // 记录日志
					StdUniqueLock lock(_mtx_calc);             // 获取互斥锁
					_cond_calc.wait(_mtx_calc);                // 等待恢复通知
					WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc resumed");  // 记录日志
					_cur_step = 1;                             // 设置状态为计算中
				}

				on_calculate(curDate, curTime);                 // 调用策略计算

				if (_has_hook && _hook_valid)                  // 如果有钩子且钩子有效
				{
					WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc done, notify control thread");  // 记录日志
					while (_cur_step==1)                        // 如果状态为计算中
						_cond_calc.notify_all();               // 通知控制线程

					WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Waiting for resume notify");  // 记录日志
					StdUniqueLock lock(_mtx_calc);             // 获取互斥锁
					_cond_calc.wait(_mtx_calc);                 // 等待恢复通知
					WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc resumed");  // 记录日志
					_cur_step = 3;                             // 设置状态为计算完成确认
				}

				if(_has_hook)                                  // 如果有钩子
					on_calculate_done(curDate, curTime);       // 调用计算完成回调
				emmited = true;                                // 标记已触发

				if (_condtions.empty())                        // 如果条件单为空
					_last_cond_min = (uint64_t)curDate * 10000 + curTime;  // 更新条件单设置时间

				_emit_times++;                                 // 增加计算次数
				_total_calc_time += ticker.micro_seconds();    // 累加计算时间


				/*
				 *	By Wesley @ 2022.07.16
				 *	策略计算完成，需要把指标数据做一个检查
				 *	如果策略在本轮没有设置指标值，则用上一个数据补齐
				 *	如果是开始，则用默认值补齐
				 */
				//for(auto& v : _chart_indice)
				//{
				//	ChartIndex& cIndex = v.second;
				//	for(auto& line : cIndex._lines)
				//	{
				//		ChartLine& cLine = line.second;
				//		if(cLine._values.size() < _emit_times)
				//		{
				//			double lastVal = DBL_MAX;
				//			if (!cLine._values.empty())
				//				lastVal = cLine._values.back();

				//			cLine._values.emplace_back(lastVal);
				//		}
				//	}
				//}

				if (_has_hook && _hook_valid)                  // 如果有钩子且钩子有效
				{
					WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "Calc done, notify control thread");  // 记录日志
					while(_cur_step == 3)                       // 如果状态为计算完成确认
						_cond_calc.notify_all();               // 通知控制线程
				}
			}
			else                                                // 如果不在交易时间内
			{
				WTSLogger::log_dyn("strategy", _name.c_str(), LL_INFO, "{} is not trading time,strategy will not be scheduled", curTime);  // 记录日志
			}
			break;                                              // 退出循环
		}
	}

	_is_in_schedule = false;//调度结束,修改标记                  // 设置调度状态为false
	return emmited;                                            // 返回是否触发
}


/**
 * @brief 交易时段开始回调
 * 
 * 清空冻结持仓，清空价格缓存，调用策略的交易时段开始回调
 * 
 * @param curTDate 当前交易日
 */
void CtaMocker::on_session_begin(uint32_t curTDate)
{
	_cur_tdate = curTDate;                                     // 更新当前交易日

	//每个交易日开始，要把冻结持仓置零
	for (auto& it : _pos_map)                                  // 遍历所有持仓
	{
		const char* stdCode = it.first.c_str();                // 获取合约代码
		PosInfo& pInfo = (PosInfo&)it.second;                 // 获取持仓信息
		if (!decimal::eq(pInfo._frozen, 0))                    // 如果有冻结持仓
		{
			log_debug("{} of {} frozen released on {}", pInfo._frozen, stdCode, curTDate);  // 记录日志
			pInfo._frozen = 0;                                 // 释放冻结持仓
		}
	}

	/*
	 *	By Wesley @ 2022.04.19
	 *	新交易日开始的时候，价格缓存清掉，要重新处理
	 */
	_price_map.clear();                                        // 清空价格缓存

	if (_strategy)                                             // 如果策略实例存在
		_strategy->on_session_begin(this, curTDate);            // 调用策略的交易时段开始回调
}

/**
 * @brief 枚举持仓
 * 
 * 遍历所有持仓和目标仓位，调用回调函数
 * 
 * @param cb 回调函数
 * @param bForExecute 是否用于执行（未使用）
 */
void CtaMocker::enum_position(FuncEnumCtaPosCallBack cb, bool bForExecute)
{
	wt_hashmap<std::string, double> desPos;                    // 目标仓位映射表
	for (auto& it : _pos_map)                                  // 遍历当前持仓
	{
		const char* stdCode = it.first.c_str();                // 获取合约代码
		const PosInfo& pInfo = it.second;                     // 获取持仓信息
		desPos[stdCode] = pInfo._volume;                       // 添加当前持仓
	}

	for (auto sit : _sig_map)                                  // 遍历交易信号
	{
		const char* stdCode = sit.first.c_str();               // 获取合约代码
		const SigInfo& sInfo = sit.second;                    // 获取信号信息
		desPos[stdCode] = sInfo._volume;                       // 更新目标仓位
	}

	for (auto v : desPos)                                      // 遍历目标仓位
	{
		cb(v.first.c_str(), v.second);                         // 调用回调函数
	}
}

/**
 * @brief 交易时段结束回调
 * 
 * 记录持仓和资金日志，通知资金变化，调用策略的交易时段结束回调
 * 
 * @param curTDate 当前交易日
 */
void CtaMocker::on_session_end(uint32_t curTDate)
{
	if (_strategy)                                             // 如果策略实例存在
		_strategy->on_session_end(this, curTDate);             // 调用策略的交易时段结束回调

	uint32_t curDate = curTDate;//_replayer->get_trading_date();  // 获取当前交易日

	double total_profit = 0;                                   // 总已实现盈亏
	double total_dynprofit = 0;                                // 总浮动盈亏

	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)  // 遍历所有持仓
	{
		const char* stdCode = it->first.c_str();               // 获取合约代码
		const PosInfo& pInfo = it->second;                     // 获取持仓信息
		total_profit += pInfo._closeprofit;                    // 累加已实现盈亏
		total_dynprofit += pInfo._dynprofit;                    // 累加浮动盈亏

		if(decimal::eq(pInfo._volume, 0.0))                    // 如果持仓数量为0
			continue;                                           // 跳过

		_pos_logs << fmt::format("{},{},{},{:.2f},{:.2f}\n", curDate, stdCode,
			pInfo._volume, pInfo._closeprofit, pInfo._dynprofit);  // 记录持仓日志
	}

	_fund_logs << fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curDate,
		_fund_info._total_profit, _fund_info._total_dynprofit,
		_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees);  // 记录资金日志
	
	if (_notifier)                                             // 如果事件通知器存在
		_notifier->notifyFund("BT_FUND", curDate, _fund_info._total_profit, _fund_info._total_dynprofit,
			_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees);  // 通知资金变化
}

/**
 * @brief 获取条件单列表
 * 
 * @param stdCode 合约代码
 * @return 条件单列表的引用
 */
CondList& CtaMocker::get_cond_entrusts(const char* stdCode)
{
	CondList& ce = _condtions[stdCode];                        // 获取或创建条件单列表
	return ce;                                                  // 返回引用
}

//////////////////////////////////////////////////////////////////////////
//策略接口
// ====================================================================

/**
 * @brief 策略接口：开多仓
 * 
 * 如果当前是空仓，直接开多；如果当前是多仓，增加多仓数量
 * 支持限价单和止损单
 * 
 * @param stdCode 合约代码
 * @param qty 开仓数量
 * @param userTag 用户标签
 * @param limitprice 限价（如果为0则不使用限价）
 * @param stopprice 止损价（如果为0则不使用止损）
 */
void CtaMocker::stra_enter_long(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if(commInfo == NULL)                                        // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;                                                  // 返回
	}

	_replayer->sub_tick(_context_id, stdCode);                  // 订阅tick数据
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式,则直接触发
	{
		double curQty = stra_get_position(stdCode);              // 获取当前持仓
		if(decimal::lt(curQty, 0))                              // 如果当前是空仓
			append_signal(stdCode, qty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 直接开多
		else                                                     // 如果当前是多仓或空仓
			append_signal(stdCode, curQty + qty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 增加多仓
	}
	else                                                        // 如果是条件单模式
	{
		CondList& condList = get_cond_entrusts(stdCode);        // 获取条件单列表

		CondEntrust entrust;                                     // 创建条件单
		strcpy(entrust._code, stdCode);                         // 设置合约代码
		strcpy(entrust._usertag, userTag);                      // 设置用户标签

		entrust._qty = qty;                                     // 设置委托数量
		entrust._field = WCF_NEWPRICE;                          // 设置比较字段为最新价
		if (!decimal::eq(limitprice))                           // 如果设置了限价
		{
			entrust._target = limitprice;                       // 设置目标价格为限价
			entrust._alg = WCT_SmallerOrEqual;                  // 设置比较算法为小于等于（限价买入）
		}
		else if (!decimal::eq(stopprice))                       // 如果设置了止损价
		{
			entrust._target = stopprice;                        // 设置目标价格为止损价
			entrust._alg = WCT_LargerOrEqual;                   // 设置比较算法为大于等于（止损买入）
		}

		entrust._action = COND_ACTION_OL;                       // 设置动作为开多

		condList.emplace_back(entrust);                         // 添加到条件单列表
	}
}

/**
 * @brief 策略接口：开空仓
 * 
 * 如果当前是多仓，直接开空；如果当前是空仓，增加空仓数量
 * 支持限价单和止损单
 * 
 * @param stdCode 合约代码
 * @param qty 开仓数量
 * @param userTag 用户标签
 * @param limitprice 限价（如果为0则不使用限价）
 * @param stopprice 止损价（如果为0则不使用止损）
 */
void CtaMocker::stra_enter_short(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                       // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;                                                  // 返回
	}

	if(!commInfo->canShort())                                   // 如果合约不能做空
	{
		log_error("Cannot short on {}", stdCode);               // 记录错误日志
		return;                                                  // 返回
	}

	_replayer->sub_tick(_context_id, stdCode);                  // 订阅tick数据
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式,则直接触发
	{
		double curQty = stra_get_position(stdCode);              // 获取当前持仓
		if(decimal::gt(curQty, 0))                              // 如果当前是多仓
			append_signal(stdCode, -qty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 直接开空
		else                                                     // 如果当前是空仓或多仓
			append_signal(stdCode, curQty - qty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 增加空仓
	}
	else                                                        // 如果是条件单模式
	{
		CondList& condList = get_cond_entrusts(stdCode);        // 获取条件单列表

		CondEntrust entrust;                                     // 创建条件单
		strcpy(entrust._code, stdCode);                         // 设置合约代码
		strcpy(entrust._usertag, userTag);                      // 设置用户标签

		entrust._qty = qty;                                     // 设置委托数量
		entrust._field = WCF_NEWPRICE;                          // 设置比较字段为最新价
		if (!decimal::eq(limitprice))                           // 如果设置了限价
		{
			entrust._target = limitprice;                       // 设置目标价格为限价
			entrust._alg = WCT_LargerOrEqual;                   // 设置比较算法为大于等于（限价卖出）
		}
		else if (!decimal::eq(stopprice))                       // 如果设置了止损价
		{
			entrust._target = stopprice;                        // 设置目标价格为止损价
			entrust._alg = WCT_SmallerOrEqual;                  // 设置比较算法为小于等于（止损卖出）
		}

		entrust._action = COND_ACTION_OS;                       // 设置动作为开空

		condList.emplace_back(entrust);                         // 添加到条件单列表
	}
}

/**
 * @brief 策略接口：平多仓
 * 
 * 平掉指定的多仓数量，支持限价单和止损单
 * 
 * @param stdCode 合约代码
 * @param qty 平仓数量
 * @param userTag 用户标签
 * @param limitprice 限价（如果为0则不使用限价）
 * @param stopprice 止损价（如果为0则不使用止损）
 */
void CtaMocker::stra_exit_long(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                       // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;                                                  // 返回
	}

	WTSSessionInfo* sInfo = commInfo->getSessionInfo();         // 获取交易时段信息
	uint32_t offTime = sInfo->offsetTime(_replayer->get_min_time(), true);  // 获取偏移时间
	bool isLastBarOfDay = (offTime == sInfo->getCloseTime(true));  // 判断是否是当日最后一根K线

	//读取可平持仓,如果是收盘那根bar，则直接读取全部持仓
	double curQty = stra_get_position(stdCode, !isLastBarOfDay);  // 获取可平持仓（T+1规则下，当日开仓的持仓不可平）
	if (decimal::le(curQty, 0))                                 // 如果没有可平持仓
		return;                                                  // 返回

	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式,则直接触发
	{
		double maxQty = min(curQty, qty);                        // 取可平持仓和委托数量中的较小值
		double totalQty = stra_get_position(stdCode, false);   // 获取总持仓
		append_signal(stdCode, totalQty - maxQty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 添加信号（减少多仓）
	}
	else                                                        // 如果是条件单模式
	{
		CondList& condList = get_cond_entrusts(stdCode);        // 获取条件单列表

		CondEntrust entrust;                                     // 创建条件单
		strcpy(entrust._code, stdCode);                         // 设置合约代码
		strcpy(entrust._usertag, userTag);                      // 设置用户标签

		entrust._qty = min(curQty, qty);                        // 设置委托数量为可平持仓和委托数量中的较小值
		entrust._field = WCF_NEWPRICE;                          // 设置比较字段为最新价
		if (!decimal::eq(limitprice))                           // 如果设置了限价
		{
			entrust._target = limitprice;                       // 设置目标价格为限价
			entrust._alg = WCT_LargerOrEqual;                   // 设置比较算法为大于等于（限价卖出）
		}
		else if (!decimal::eq(stopprice))                       // 如果设置了止损价
		{
			entrust._target = stopprice;                        // 设置目标价格为止损价
			entrust._alg = WCT_SmallerOrEqual;                  // 设置比较算法为小于等于（止损卖出）
		}

		entrust._action = COND_ACTION_CL;                       // 设置动作为平多

		condList.emplace_back(entrust);                         // 添加到条件单列表
	}
}

/**
 * @brief 策略接口：平空仓
 * 
 * 平掉指定的空仓数量，支持限价单和止损单
 * 
 * @param stdCode 合约代码
 * @param qty 平仓数量
 * @param userTag 用户标签
 * @param limitprice 限价（如果为0则不使用限价）
 * @param stopprice 止损价（如果为0则不使用止损）
 */
void CtaMocker::stra_exit_short(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                       // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;                                                  // 返回
	}

	if (!commInfo->canShort())                                  // 如果合约不能做空
	{
		log_error("Cannot short on {}", stdCode);               // 记录错误日志
		return;                                                  // 返回
	}

	double curQty = stra_get_position(stdCode);                  // 获取当前持仓
	if (decimal::ge(curQty, 0))                                 // 如果没有空仓
		return;                                                  // 返回

	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式,则直接触发
	{
		double maxQty = min(abs(curQty), qty);                   // 取当前空仓绝对值和委托数量中的较小值
		append_signal(stdCode, curQty + maxQty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 添加信号（减少空仓）
	}
	else                                                        // 如果是条件单模式
	{
		CondList& condList = get_cond_entrusts(stdCode);        // 获取条件单列表

		CondEntrust entrust;                                     // 创建条件单
		strcpy(entrust._code, stdCode);                         // 设置合约代码
		strcpy(entrust._usertag, userTag);                      // 设置用户标签

		entrust._qty = qty;                                     // 设置委托数量
		entrust._field = WCF_NEWPRICE;                          // 设置比较字段为最新价
		if (!decimal::eq(limitprice))                           // 如果设置了限价
		{
			entrust._target = limitprice;                       // 设置目标价格为限价
			entrust._alg = WCT_SmallerOrEqual;                  // 设置比较算法为小于等于（限价买入）
		}
		else if (!decimal::eq(stopprice))                       // 如果设置了止损价
		{
			entrust._target = stopprice;                        // 设置目标价格为止损价
			entrust._alg = WCT_LargerOrEqual;                   // 设置比较算法为大于等于（止损买入）
		}

		entrust._action = COND_ACTION_CS;                       // 设置动作为平空

		condList.emplace_back(entrust);                         // 添加到条件单列表
	}
}

/**
 * @brief 策略接口：获取当前价格
 * 
 * @param stdCode 合约代码
 * @return 当前价格
 */
double CtaMocker::stra_get_price(const char* stdCode)
{
	if (_replayer)                                               // 如果回放器存在
		return _replayer->get_cur_price(stdCode);                // 返回当前价格

	return 0.0;                                                  // 返回0
}

/**
 * @brief 策略接口：获取当日价格
 * 
 * @param stdCode 合约代码
 * @param flag 价格类型：0-最新价，1-开盘价，2-最高价，3-最低价，4-收盘价
 * @return 当日价格
 */
double CtaMocker::stra_get_day_price(const char* stdCode, int flag /* = 0 */)
{
	if (_replayer)                                               // 如果回放器存在
		return _replayer->get_day_price(stdCode, flag);          // 返回当日价格

	return 0.0;                                                  // 返回0
}

/**
 * @brief 策略接口：设置目标仓位
 * 
 * 直接设置目标仓位，支持限价单和止损单
 * 
 * @param stdCode 合约代码
 * @param qty 目标仓位（正数表示多仓，负数表示空仓）
 * @param userTag 用户标签
 * @param limitprice 限价（如果为0则不使用限价）
 * @param stopprice 止损价（如果为0则不使用止损）
 */
void CtaMocker::stra_set_position(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice /* = 0.0 */, double stopprice /* = 0.0 */)
{
	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                       // 如果合约信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;                                                  // 返回
	}

	//如果不能做空，则目标仓位不能设置负数
	if (!commInfo->canShort() && decimal::lt(qty, 0))          // 如果合约不能做空且目标仓位为负数
	{
		log_error("Cannot short on {}", stdCode);               // 记录错误日志
		return;                                                  // 返回
	}

	double total = stra_get_position(stdCode, false);            // 获取总持仓
	//如果目标仓位和当前仓位是一致的，直接退出
	if (decimal::eq(total, qty))                                // 如果目标仓位和当前仓位一致
		return;                                                  // 返回

	if(commInfo->isT1())                                        // 如果是T+1规则
	{
		double valid = stra_get_position(stdCode, true);         // 获取有效持仓
		double frozen = total - valid;                          // 计算冻结持仓
		//如果是T+1规则，则目标仓位不能小于冻结仓位
		if(decimal::lt(qty, frozen))                            // 如果目标仓位小于冻结仓位
		{
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_ERROR, "New position of {} cannot be set to {} due to {} being frozen", stdCode, qty, frozen);  // 记录错误日志
			return;                                              // 返回
		}
	}

	_replayer->sub_tick(_context_id, stdCode);                  // 订阅tick数据
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//没有设置触发条件，则直接添加信号
	{
		append_signal(stdCode, qty, userTag, 0.0, _is_in_schedule ? 0 : 1);  // 添加信号
	}
	else                                                        // 如果是条件单模式
	{
		CondList& condList = get_cond_entrusts(stdCode);        // 获取条件单列表

		bool isBuy = decimal::gt(qty, total);                     // 判断是买入还是卖出

		CondEntrust entrust;                                     // 创建条件单
		strcpy(entrust._code, stdCode);                         // 设置合约代码
		strcpy(entrust._usertag, userTag);                      // 设置用户标签
		entrust._qty = qty;                                     // 设置委托数量
		entrust._field = WCF_NEWPRICE;                          // 设置比较字段为最新价
		if (!decimal::eq(limitprice))                           // 如果设置了限价
		{
			entrust._target = limitprice;                       // 设置目标价格为限价
			entrust._alg = isBuy ? WCT_SmallerOrEqual : WCT_LargerOrEqual;  // 买入用小于等于，卖出用大于等于
		}
		else if (!decimal::eq(stopprice))                       // 如果设置了止损价
		{
			entrust._target = stopprice;                        // 设置目标价格为止损价
			entrust._alg = isBuy ? WCT_LargerOrEqual : WCT_SmallerOrEqual;  // 买入用大于等于，卖出用小于等于
		}

		entrust._action = COND_ACTION_SP;                       // 设置动作为同步仓位

		condList.emplace_back(entrust);                         // 添加到条件单列表
	}
}

/**
 * @brief 添加交易信号
 * 
 * 将交易信号添加到信号映射表中
 * 
 * @param stdCode 合约代码
 * @param qty 目标仓位
 * @param userTag 用户标签
 * @param price 指定成交价格（如果为0则使用市场价格）
 * @param sigType 信号类型：0-调度中发出，1-非调度中发出，2-条件单触发
 */
void CtaMocker::append_signal(const char* stdCode, double qty, const char* userTag /* = "" */, double price /* = 0.0 */, uint32_t sigType /* = 0 */)
{
	double curPx = _price_map[stdCode];                         // 获取当前价格

	SigInfo& sInfo = _sig_map[stdCode];                          // 获取或创建信号信息
	sInfo._volume = qty;                                         // 设置目标仓位
	sInfo._sigprice = curPx;                                    // 设置信号价格
	sInfo._desprice = price;                                     // 设置指定成交价格
	sInfo._usertag = userTag;                                   // 设置用户标签
	sInfo._gentime = (uint64_t)_replayer->get_date() * 1000000000 + (uint64_t)_replayer->get_raw_time() * 100000 + _replayer->get_secs();  // 设置信号生成时间
	sInfo._sigtype = sigType;                                    // 设置信号类型

	log_signal(stdCode, qty, curPx, sInfo._gentime, userTag);   // 记录信号日志

	//save_data();
}

/**
 * @brief 执行设置仓位操作
 * 
 * 根据目标仓位和当前仓位的差异，执行开仓或平仓操作
 * 支持滑点计算、手续费计算、持仓明细管理
 * 
 * @param stdCode 合约代码
 * @param qty 目标仓位
 * @param price 指定成交价格（如果为0则使用市场价格）
 * @param userTag 用户标签
 */
void CtaMocker::do_set_position(const char* stdCode, double qty, double price /* = 0.0 */, const char* userTag /* = "" */)
{
	PosInfo& pInfo = _pos_map[stdCode];                         // 获取或创建持仓信息
	double curPx = price;                                        // 成交价格
	if (decimal::eq(price, 0.0))                                 // 如果指定价格为0
		curPx = _price_map[stdCode];                             // 使用市场价格
	uint64_t curTm = (uint64_t)_replayer->get_date() * 10000 + _replayer->get_min_time();  // 获取当前时间（分钟）
	uint32_t curTDate = _replayer->get_trading_date();           // 获取当前交易日

	//手数相等则不用操作了
	if (decimal::eq(pInfo._volume, qty))                        // 如果目标仓位和当前仓位相等
		return;                                                  // 直接返回

	WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
	if (commInfo == NULL)                                       // 如果合约信息不存在
		return;                                                  // 直接返回

	//成交价
	double trdPx = curPx;                                        // 成交价格

	double diff = qty - pInfo._volume;                          // 仓位变化量
	bool isBuy = decimal::gt(diff, 0.0);                        // 是否是买入（增加仓位）
	if (decimal::gt(pInfo._volume*diff, 0))//当前持仓和仓位变化方向一致, 增加一条明细, 增加数量即可
	{
		pInfo._volume = qty;                                     // 更新持仓数量

		//如果T+1，则冻结仓位要增加
		if (commInfo->isT1())                                   // 如果是T+1规则
		{
			//ASSERT(diff>0);
			pInfo._frozen += diff;                              // 增加冻结持仓
			log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录日志
		}
		
		if (_slippage != 0)                                      // 如果设置了滑点
		{
			if (_ratio_slippage)                                // 如果是比例滑点
			{
				//By Wesley @ 2023.05.05
				//如果是比率滑点，则要根据目标成交价计算
				//得到滑点以后，再根据pricetick做一个修正
				double slp = (_slippage * trdPx / 10000.0);     // 计算滑点（基点转换为价格）
				slp = round(slp / commInfo->getPriceTick())*commInfo->getPriceTick();  // 根据价格最小变动单位修正滑点

				trdPx += slp * (isBuy ? 1 : -1);                // 买入加滑点，卖出减滑点
			}
			else                                                 // 如果是固定滑点
				trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 买入加滑点，卖出减滑点
		}

		DetailInfo dInfo;                                        // 创建持仓明细
		dInfo._long = decimal::gt(qty, 0);                      // 设置是否多头
		dInfo._price = trdPx;                                    // 设置开仓价格
		dInfo._max_price = trdPx;                                // 设置最高价
		dInfo._min_price = trdPx;                                // 设置最低价
		dInfo._volume = abs(diff);                               // 设置持仓数量
		dInfo._opentime = curTm;                                 // 设置开仓时间
		dInfo._opentdate = curTDate;                             // 设置开仓交易日
		strcpy(dInfo._opentag, userTag);                        // 设置开仓标签
		dInfo._open_barno = _schedule_times;                    // 设置开仓K线编号
		pInfo._details.emplace_back(dInfo);                     // 添加到持仓明细列表
		pInfo._last_entertime = curTm;                          // 更新最后开仓时间

		double fee = _replayer->calc_fee(stdCode, trdPx, abs(diff), 0);  // 计算手续费
		_fund_info._total_fees += fee;                          // 累加手续费

		log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(diff), userTag, fee, _schedule_times);  // 记录成交日志
	}
	else
	{//持仓方向和仓位变化方向不一致,需要平仓
		double left = abs(diff);                                  // 剩余需要平仓的数量
		if (_slippage != 0)                                      // 如果设置了滑点
		{
			if (_ratio_slippage)                                // 如果是比例滑点
			{
				//By Wesley @ 2023.05.05
				//如果是比率滑点，则要根据目标成交价计算
				//得到滑点以后，再根据pricetick做一个修正
				double slp = (_slippage * trdPx / 10000.0);     // 计算滑点（基点转换为价格）
				slp = round(slp / commInfo->getPriceTick())*commInfo->getPriceTick();  // 根据价格最小变动单位修正滑点

				trdPx += slp * (isBuy ? 1 : -1);                // 买入加滑点，卖出减滑点
			}
			else                                                 // 如果是固定滑点
				trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 买入加滑点，卖出减滑点
		}

		pInfo._volume = qty;                                     // 更新持仓数量
		if (decimal::eq(pInfo._volume, 0))                      // 如果持仓数量为0
			pInfo._dynprofit = 0;                                // 清零浮动盈亏
		uint32_t count = 0;                                      // 需要删除的明细数量
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历持仓明细
		{
			DetailInfo& dInfo = *it;                             // 获取明细信息
			double maxQty = min(dInfo._volume, left);             // 本次平仓数量（取明细持仓和剩余数量中的较小值）
			if (decimal::eq(maxQty, 0))                          // 如果本次平仓数量为0
				continue;                                         // 跳过

			double maxProf = dInfo._max_profit * maxQty / dInfo._volume;  // 按比例计算最大盈利
			double maxLoss = dInfo._max_loss * maxQty / dInfo._volume;    // 按比例计算最大亏损

			dInfo._volume -= maxQty;                             // 减少明细持仓数量
			left -= maxQty;                                      // 减少剩余需要平仓的数量

			if (decimal::eq(dInfo._volume, 0))                    // 如果明细持仓数量为0
				count++;                                          // 增加删除计数

			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算已实现盈亏
			if (!dInfo._long)                                    // 如果是空仓
				profit *= -1;                                     // 盈亏取反
			pInfo._closeprofit += profit;                        // 累加持仓的已实现盈亏
			_total_closeprofit += profit;                         // 累加累计已实现盈亏
			pInfo._dynprofit = pInfo._dynprofit*dInfo._volume / (dInfo._volume + maxQty);//浮盈也要做等比缩放
			pInfo._last_exittime = curTm;                        // 更新最后平仓时间
			_fund_info._total_profit += profit;                  // 累加总已实现盈亏

			double fee = _replayer->calc_fee(stdCode, trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费（当日开仓平仓手续费率不同）
			_fund_info._total_fees += fee;                      // 累加手续费
			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, trdPx, maxQty, userTag, fee, _schedule_times);  // 记录成交日志
			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, maxProf, maxLoss, 
				_total_closeprofit - _fund_info._total_fees, dInfo._opentag, userTag, dInfo._open_barno, _schedule_times);  // 记录平仓日志

			if (left == 0)                                       // 如果剩余数量为0
				break;                                           // 退出循环
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)                                        // 删除已平仓完的明细
		{
			auto it = pInfo._details.begin();                    // 获取第一个明细
			pInfo._details.erase(it);                            // 删除明细
			count--;                                              // 减少计数
		}

		//最后,如果还有剩余的,则需要反手了
		if (left > 0)                                            // 如果还有剩余数量（需要反手）
		{
			left = left * qty / abs(qty);                        // 根据目标仓位方向调整剩余数量

			//如果T+1，则冻结仓位要增加
			if (commInfo->isT1())                                // 如果是T+1规则
			{
				pInfo._frozen += left;                          // 增加冻结持仓
				log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录日志
			}

			DetailInfo dInfo;                                     // 创建持仓明细（反手开仓）
			dInfo._long = decimal::gt(qty, 0);                   // 设置是否多头
			dInfo._price = trdPx;                                // 设置开仓价格
			dInfo._max_price = trdPx;                             // 设置最高价
			dInfo._min_price = trdPx;                             // 设置最低价
			dInfo._volume = abs(left);                            // 设置持仓数量
			dInfo._opentime = curTm;                              // 设置开仓时间
			dInfo._opentdate = curTDate;                          // 设置开仓交易日
			dInfo._open_barno = _schedule_times;                 // 设置开仓K线编号
			strcpy(dInfo._opentag, userTag);                     // 设置开仓标签
			pInfo._details.emplace_back(dInfo);                  // 添加到持仓明细列表
 
			//这里还需要写一笔成交记录
			double fee = _replayer->calc_fee(stdCode, trdPx, abs(left), 0);  // 计算手续费
			_fund_info._total_fees += fee;                      // 累加手续费
			log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(left), userTag, fee, _schedule_times);  // 记录成交日志

			pInfo._last_entertime = curTm;                       // 更新最后开仓时间
		}
	}
}

/**
 * @brief 策略接口：获取K线数据
 * 
 * 获取指定合约和周期的K线数据，如果是主K线则记录主K线信息
 * 
 * @param stdCode 合约代码
 * @param period 周期（如"m1", "d1"等）
 * @param count 获取的K线数量
 * @param isMain 是否为主K线
 * @return K线数据切片指针
 */
WTSKlineSlice* CtaMocker::stra_get_bars(const char* stdCode, const char* period, uint32_t count, bool isMain /* = false */)
{
	thread_local static char key[64] = { 0 };                    // 线程局部变量：K线键值
	fmtutil::format_to(key, "{}#{}", stdCode, period);          // 格式化K线键值（合约代码#周期）

	thread_local static char basePeriod[2] = { 0 };            // 线程局部变量：基础周期
	basePeriod[0] = period[0];                                  // 获取周期类型（m/d等）
	uint32_t times = 1;                                           // 周期倍数
	if (strlen(period) > 1)                                      // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);                  // 解析周期倍数
	else                                                         // 如果周期字符串长度为1
		strcat(key, "1");                                        // 添加倍数1到键值

	if (isMain)                                                  // 如果是主K线
	{
		if (_main_key.empty())                                   // 如果主K线键值未设置
			_main_key = key;                                     // 设置主K线键值
		else if (_main_key != key)                               // 如果主K线键值已设置且不一致
		{
			WTSLogger::error("Main k bars can only be setup once");  // 记录错误日志
			return NULL;                                          // 返回NULL
		}

		/*
		 *	By Wesley @ 2022.07.16
		 */
		_main_code = stdCode;                                    // 设置主K线合约代码
		_main_period = period;                                   // 设置主K线周期
	}

	WTSKlineSlice* kline = _replayer->get_kline_slice(stdCode, basePeriod, count, times, isMain);  // 获取K线数据

	KlineTag& tag = _kline_tags[key];                            // 获取或创建K线标签
	tag._closed = false;                                          // 标记K线未收盘

	if (kline)                                                    // 如果K线数据存在
	{
		//double lastClose = kline->close(-1);
		CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _replayer->get_hot_mgr());  // 解析合约代码
		WTSCommodityInfo* commInfo = _replayer->get_commodity_info(stdCode);  // 获取合约信息
		std::string realCode = stdCode;                           // 实际合约代码
		if(cInfo.isExright())                                    // 如果是复权合约
			realCode = realCode.substr(0, realCode.size()-1);   // 去掉复权标记
		_replayer->sub_tick(id(), realCode.c_str());             // 订阅tick数据
	}

	return kline;                                                 // 返回K线数据
}

/**
 * @brief 策略接口：获取tick数据
 * 
 * @param stdCode 合约代码
 * @param count 获取的tick数量
 * @return tick数据切片指针
 */
WTSTickSlice* CtaMocker::stra_get_ticks(const char* stdCode, uint32_t count)
{
	return _replayer->get_tick_slice(stdCode, count);             // 从回放器获取tick数据切片
}

/**
 * @brief 策略接口：获取最新tick数据
 * 
 * 优先从tick缓存中获取，如果缓存中没有则从回放器获取
 * 
 * @param stdCode 合约代码
 * @return 最新tick数据指针
 */
WTSTickData* CtaMocker::stra_get_last_tick(const char* stdCode)
{
	auto it = _ticks.find(stdCode);                               // 查找tick缓存
	if (it != _ticks.end())                                       // 如果缓存中存在
	{
		WTSTickData* lastTick = WTSTickData::create((WTSTickStruct&)it->second);  // 创建tick数据对象
		return lastTick;                                           // 返回tick数据
	}

	return _replayer->get_last_tick(stdCode);                     // 从回放器获取最新tick
}

/**
 * @brief 策略接口：订阅tick数据
 * 
 * 主动订阅tick会在本地记一下，tick数据回调的时候先检查一下
 * 
 * @param code 合约代码
 */
void CtaMocker::stra_sub_ticks(const char* code)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(code);                                      // 添加到tick订阅列表

	_replayer->sub_tick(_context_id, code);                       // 向回放器订阅tick数据
}

/**
 * @brief 策略接口：订阅K线收盘事件
 * 
 * 标记该K线需要触发收盘事件回调
 * 
 * @param stdCode 合约代码
 * @param period 周期
 */
void CtaMocker::stra_sub_bar_events(const char* stdCode, const char* period)
{
	thread_local static char key[64] = { 0 };                     // 线程局部变量：K线键值
	fmtutil::format_to(key, "{}#{}", stdCode, period);            // 格式化K线键值

	KlineTag& tag = _kline_tags[key];                             // 获取或创建K线标签
	tag._notify = true;                                           // 标记需要通知策略
}

/**
 * @brief 策略接口：获取合约信息
 * 
 * @param stdCode 合约代码
 * @return 合约信息指针
 */
WTSCommodityInfo* CtaMocker::stra_get_comminfo(const char* stdCode)
{
	return _replayer->get_commodity_info(stdCode);               // 从回放器获取合约信息
}

/**
 * @brief 策略接口：获取原始合约代码
 * 
 * @param stdCode 标准合约代码
 * @return 原始合约代码
 */
std::string CtaMocker::stra_get_rawcode(const char* stdCode)
{
	return _replayer->get_rawcode(stdCode);                       // 从回放器获取原始合约代码
}

/**
 * @brief 策略接口：获取当前交易日
 * 
 * @return 当前交易日
 */
uint32_t CtaMocker::stra_get_tdate()
{
	return _replayer->get_trading_date();                         // 从回放器获取当前交易日
}

/**
 * @brief 策略接口：获取当前日期
 * 
 * @return 当前日期
 */
uint32_t CtaMocker::stra_get_date()
{
	return _replayer->get_date();                                 // 从回放器获取当前日期
}

/**
 * @brief 策略接口：获取当前时间（分钟）
 * 
 * @return 当前时间（分钟）
 */
uint32_t CtaMocker::stra_get_time()
{
	return _replayer->get_min_time();                             // 从回放器获取当前时间（分钟）
}

/**
 * @brief 策略接口：获取资金数据
 * 
 * @param flag 资金数据类型：0-动态权益，1-已实现盈亏，2-浮动盈亏，3-手续费
 * @return 资金数据值
 */
double CtaMocker::stra_get_fund_data(int flag)
{
	switch (flag)                                                 // 根据标志返回不同的资金数据
	{
	case 0:                                                       // 动态权益
		return _fund_info._total_profit - _fund_info._total_fees + _fund_info._total_dynprofit;  // 已实现盈亏 - 手续费 + 浮动盈亏
	case 1:                                                       // 已实现盈亏
		return _fund_info._total_profit;
	case 2:                                                       // 浮动盈亏
		return _fund_info._total_dynprofit;
	case 3:                                                       // 手续费
		return _fund_info._total_fees;
	default:
		return 0.0;
	}
}

/**
 * @brief 策略接口：记录信息日志
 * 
 * @param message 日志消息
 */
void CtaMocker::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 记录信息级别日志
}

/**
 * @brief 策略接口：记录调试日志
 * 
 * @param message 日志消息
 */
void CtaMocker::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 记录调试级别日志
}

/**
 * @brief 策略接口：记录警告日志
 * 
 * @param message 日志消息
 */
void CtaMocker::stra_log_warn(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_WARN, message);  // 记录警告级别日志
}

/**
 * @brief 策略接口：记录错误日志
 * 
 * @param message 日志消息
 */
void CtaMocker::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 记录错误级别日志
}

/**
 * @brief 策略接口：加载用户数据
 * 
 * @param key 数据键
 * @param defVal 默认值（如果键不存在时返回）
 * @return 数据值（如果不存在则返回默认值）
 */
const char* CtaMocker::stra_load_user_data(const char* key, const char* defVal /*= ""*/)
{
	auto it = _user_datas.find(key);                              // 查找用户数据
	if (it != _user_datas.end())                                  // 如果找到
		return it->second.c_str();                                // 返回数据值

	return defVal;                                                 // 返回默认值
}

/**
 * @brief 策略接口：保存用户数据
 * 
 * @param key 数据键
 * @param val 数据值
 */
void CtaMocker::stra_save_user_data(const char* key, const char* val)
{
	_user_datas[key] = val;                                        // 保存用户数据
	_ud_modified = true;                                          // 标记用户数据已修改
}

/**
 * @brief 策略接口：获取首次开仓时间
 * 
 * @param stdCode 合约代码
 * @return 首次开仓时间（如果不存在则返回0）
 */
uint64_t CtaMocker::stra_get_first_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                             // 查找持仓
	if (it == _pos_map.end())                                     // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	if (pInfo._details.empty())                                   // 如果持仓明细为空
		return 0;                                                  // 返回0

	return pInfo._details[0]._opentime;                           // 返回第一条明细的开仓时间
}

/**
 * @brief 策略接口：获取最后开仓时间
 * 
 * @param stdCode 合约代码
 * @return 最后开仓时间（如果不存在则返回0）
 */
uint64_t CtaMocker::stra_get_last_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                             // 查找持仓
	if (it == _pos_map.end())                                     // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	if (pInfo._details.empty())                                   // 如果持仓明细为空
		return 0;                                                  // 返回0

	return pInfo._details[pInfo._details.size() - 1]._opentime;  // 返回最后一条明细的开仓时间
}

/**
 * @brief 策略接口：获取最后开仓标签
 * 
 * @param stdCode 合约代码
 * @return 最后开仓标签（如果不存在则返回空字符串）
 */
const char* CtaMocker::stra_get_last_entertag(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                             // 查找持仓
	if (it == _pos_map.end())                                     // 如果不存在
		return "";                                                  // 返回空字符串

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	if (pInfo._details.empty())                                   // 如果持仓明细为空
		return "";                                                  // 返回空字符串

	return pInfo._details[pInfo._details.size() - 1]._opentag;  // 返回最后一条明细的开仓标签
}

/**
 * @brief 策略接口：获取最后平仓时间
 * 
 * @param stdCode 合约代码
 * @return 最后平仓时间（如果不存在则返回0）
 */
uint64_t CtaMocker::stra_get_last_exittime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                             // 查找持仓
	if (it == _pos_map.end())                                     // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	return pInfo._last_exittime;                                  // 返回最后平仓时间
}

/**
 * @brief 策略接口：获取最后开仓价格
 * 
 * @param stdCode 合约代码
 * @return 最后开仓价格（如果不存在则返回0）
 */
double CtaMocker::stra_get_last_enterprice(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                             // 查找持仓
	if (it == _pos_map.end())                                     // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	if (pInfo._details.empty())                                   // 如果持仓明细为空
		return 0;                                                  // 返回0

	return pInfo._details[pInfo._details.size() - 1]._price;      // 返回最后一条明细的开仓价格
}

/**
 * @brief 策略接口：获取持仓数量
 * 
 * 如果有信号，说明刚下了指令，还没等到下一个tick进来，用户就在读取仓位
 * 支持按用户标签查询和查询有效持仓（T+1规则）
 * 
 * @param stdCode 合约代码
 * @param bOnlyValid 是否只查询有效持仓（T+1规则下，当日开仓的持仓不可平）
 * @param userTag 用户标签（如果为空则查询所有持仓）
 * @return 持仓数量
 */
double CtaMocker::stra_get_position(const char* stdCode, bool bOnlyValid /* = false */, const char* userTag /* = "" */)
{
	//By Wesley @ 2022.05.22
	//如果有信号，说明刚下了指令，还没等到下一个tick进来，用户就在读取仓位
	double totalPos = 0;                                           // 总持仓
	auto sit = _sig_map.find(stdCode);                             // 查找交易信号
	if (sit != _sig_map.end())                                     // 如果存在信号
	{
		totalPos = sit->second._volume;                           // 使用信号中的目标仓位
	}

	auto it = _pos_map.find(stdCode);                              // 查找持仓
	if (it == _pos_map.end())                                      // 如果不存在
		return totalPos;                                            // 返回信号中的仓位

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	totalPos = pInfo._volume;                                      // 使用实际持仓

	if (strlen(userTag) == 0)                                      // 如果用户标签为空（查询所有持仓）
	{
		if (bOnlyValid)                                            // 如果只查询有效持仓
		{
			//只有userTag为空的时候时候，才会用bOnlyValid
			//这里理论上，只有多头才会进到这里
			//其他地方要保证，空头持仓的话，_frozen要为0
			return totalPos - pInfo._frozen;                       // 返回总持仓减去冻结持仓
		}
		else                                                        // 如果查询所有持仓
			return totalPos;                                        // 返回总持仓
	}
	else                                                           // 如果用户标签不为空（查询指定标签的持仓）
	{
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历持仓明细
		{
			const DetailInfo& dInfo = (*it);                      // 获取明细信息
			if (strcmp(dInfo._opentag, userTag) != 0)             // 如果标签不匹配
				continue;                                           // 跳过

			return dInfo._volume;                                  // 返回该明细的持仓数量
		}
	}

	return 0;                                                       // 如果没找到则返回0
}

/**
 * @brief 策略接口：获取持仓平均价格
 * 
 * @param stdCode 合约代码
 * @return 持仓平均价格（如果不存在则返回0）
 */
double CtaMocker::stra_get_position_avgpx(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                              // 查找持仓
	if (it == _pos_map.end())                                      // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	if (pInfo._volume == 0)                                        // 如果持仓数量为0
		return 0.0;                                                 // 返回0

	double amount = 0.0;                                           // 总金额
	for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历持仓明细
	{
		const DetailInfo& dInfo = *dit;                            // 获取明细信息
		amount += dInfo._price*dInfo._volume;                      // 累加价格*数量
	}

	return amount / pInfo._volume;                                  // 返回平均价格
}

/**
 * @brief 策略接口：获取持仓浮动盈亏
 * 
 * @param stdCode 合约代码
 * @return 持仓浮动盈亏
 */
double CtaMocker::stra_get_position_profit(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);                              // 查找持仓
	if (it == _pos_map.end())                                      // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	return pInfo._dynprofit;                                       // 返回浮动盈亏
}

/**
 * @brief 策略接口：获取持仓明细的开仓时间
 * 
 * @param stdCode 合约代码
 * @param userTag 用户标签
 * @return 开仓时间（如果不存在则返回0）
 */
uint64_t CtaMocker::stra_get_detail_entertime(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);                              // 查找持仓
	if (it == _pos_map.end())                                      // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历持仓明细
	{
		const DetailInfo& dInfo = (*it);                          // 获取明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)                 // 如果标签不匹配
			continue;                                               // 跳过

		return dInfo._opentime;                                    // 返回开仓时间
	}

	return 0;                                                       // 如果没找到则返回0
}

/**
 * @brief 策略接口：获取持仓明细的开仓成本
 * 
 * @param stdCode 合约代码
 * @param userTag 用户标签
 * @return 开仓成本（如果不存在则返回0）
 */
double CtaMocker::stra_get_detail_cost(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);                              // 查找持仓
	if (it == _pos_map.end())                                      // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历持仓明细
	{
		const DetailInfo& dInfo = (*it);                          // 获取明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)                 // 如果标签不匹配
			continue;                                               // 跳过

		return dInfo._price;                                        // 返回开仓价格
	}

	return 0.0;                                                     // 如果没找到则返回0
}

/**
 * @brief 策略接口：获取持仓明细的盈亏
 * 
 * @param stdCode 合约代码
 * @param userTag 用户标签
 * @param flag 盈亏类型：0-当前盈亏，1-最大盈利，-1-最大亏损，2-最高价，-2-最低价
 * @return 盈亏值（如果不存在则返回0）
 */
double CtaMocker::stra_get_detail_profit(const char* stdCode, const char* userTag, int flag /* = 0 */)
{
	auto it = _pos_map.find(stdCode);                              // 查找持仓
	if (it == _pos_map.end())                                      // 如果不存在
		return 0;                                                  // 返回0

	const PosInfo& pInfo = it->second;                            // 获取持仓信息
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历持仓明细
	{
		const DetailInfo& dInfo = (*it);                          // 获取明细信息
		if (strcmp(dInfo._opentag, userTag) != 0)                 // 如果标签不匹配
			continue;                                               // 跳过

		switch (flag)                                               // 根据标志返回不同的值
		{
		case 0:                                                     // 当前盈亏
			return dInfo._profit;
		case 1:                                                     // 最大盈利
			return dInfo._max_profit;
		case -1:                                                    // 最大亏损
			return dInfo._max_loss;
		case 2:                                                     // 最高价
			return dInfo._max_price;
		case -2:                                                    // 最低价
			return dInfo._min_price;
		}
	}

	return 0.0;                                                     // 如果没找到则返回0
}

/**
 * @brief 策略接口：设置图表K线
 * 
 * @param stdCode 合约代码
 * @param period 周期
 */
void CtaMocker::set_chart_kline(const char* stdCode, const char* period)
{
	_chart_code = stdCode;                                         // 设置图表合约代码
	_chart_period = period;                                        // 设置图表周期
}

/**
 * @brief 策略接口：添加图表标记
 * 
 * 标记只能在调度期间添加
 * 
 * @param price 价格
 * @param icon 图标
 * @param tag 标签
 */
void CtaMocker::add_chart_mark(double price, const char* icon, const char* tag)
{
	if (!_is_in_schedule)                                         // 如果不在调度期间
	{
		WTSLogger::error("Marks can be added only during schedule");  // 记录错误日志
		return;                                                    // 返回
	}

	uint64_t curTime = _replayer->get_date();                     // 获取当前日期
	curTime = curTime*10000 + _replayer->get_min_time();          // 格式化时间（日期*10000+分钟时间）

	_mark_logs << curTime << "," << price << "," << icon << "," << tag << std::endl;  // 写入标记日志
}

/**
 * @brief 策略接口：注册图表指标
 * 
 * @param idxName 指标名称
 * @param indexType 指标类型
 */
void CtaMocker::register_index(const char* idxName, uint32_t indexType)
{
	ChartIndex& cIndex = _chart_indice[idxName];                  // 获取或创建指标
	cIndex._name = idxName;                                        // 设置指标名称
	cIndex._indexType = indexType;                                 // 设置指标类型
}

/**
 * @brief 策略接口：注册指标线
 * 
 * @param idxName 指标名称
 * @param lineName 线名称
 * @param lineType 线类型
 * @return 是否成功
 */
bool CtaMocker::register_index_line(const char* idxName, const char* lineName, uint32_t lineType)
{
	auto it = _chart_indice.find(idxName);                        // 查找指标
	if (it == _chart_indice.end())                                // 如果指标不存在
	{
		WTSLogger::error("Index {} not registered", idxName);     // 记录错误日志
		return false;                                              // 返回失败
	}

	ChartIndex& cIndex = it->second;                              // 获取指标
	ChartLine& cLine = cIndex._lines[lineName];                  // 获取或创建指标线
	cLine._name = lineName;                                        // 设置线名称
	cLine._lineType = lineType;                                    // 设置线类型
	return true;                                                    // 返回成功
}

/**
 * @brief 策略接口：添加指标基准线
 * 
 * @param idxName 指标名称
 * @param lineName 线名称
 * @param val 基准值
 * @return 是否成功
 */
bool CtaMocker::add_index_baseline(const char* idxName, const char* lineName, double val)
{
	auto it = _chart_indice.find(idxName);                        // 查找指标
	if (it == _chart_indice.end())                                // 如果指标不存在
	{
		WTSLogger::error("Index {} not registered", idxName);     // 记录错误日志
		return false;                                              // 返回失败
	}

	ChartIndex& cIndex = it->second;                              // 获取指标
	cIndex._base_lines[lineName] = val;                            // 设置基准线值
	return true;                                                    // 返回成功
}

/**
 * @brief 策略接口：设置指标值
 * 
 * 只能在调度期间设置
 * 
 * @param idxName 指标名称
 * @param lineName 线名称
 * @param val 指标值
 * @return 是否成功
 */
bool CtaMocker::set_index_value(const char* idxName, const char* lineName, double val)
{
	if (!_is_in_schedule)                                         // 如果不在调度期间
	{
		WTSLogger::error("Marks can be added only during schedule");  // 记录错误日志
		return false;                                              // 返回失败
	}

	auto ait = _chart_indice.find(idxName);                       // 查找指标
	if (ait == _chart_indice.end())                               // 如果指标不存在
	{
		WTSLogger::error("Index {} not registered", idxName);     // 记录错误日志
		return false;                                              // 返回失败
	}

	ChartIndex& cIndex = ait->second;                              // 获取指标
	auto bit = cIndex._lines.find(lineName);                      // 查找指标线
	if (bit == cIndex._lines.end())                               // 如果指标线不存在
	{
		WTSLogger::error("Line {} of index {} not registered", lineName, idxName);  // 记录错误日志
		return false;                                              // 返回失败
	}

	uint64_t curTime = _replayer->get_date();                     // 获取当前日期
	curTime = curTime * 10000 + _replayer->get_min_time();        // 格式化时间（日期*10000+分钟时间）
	_index_logs << curTime << "," << idxName << "," << lineName << "," << val << std::endl;  // 写入指标日志
	return true;                                                    // 返回成功
}

