/*!
 * \file CtaStraBaseCtx.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略基础上下文实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了CTA策略基础上下文类的所有核心功能，是CTA策略运行的核心实现。
 * 主要实现逻辑：
 * 1. 初始化管理：初始化输出文件、加载历史数据、加载用户数据等
 * 2. 持仓管理：维护理论持仓、持仓明细、持仓盈亏，支持开仓、平仓、反手等操作
 * 3. 交易信号处理：处理交易信号，支持条件单，自动触发交易执行
 * 4. 数据持久化：保存和加载策略状态（持仓、资金、条件单、信号等），支持断点续传
 * 5. 市场数据访问：提供K线、Tick、价格等数据查询接口的实现
 * 6. 日志记录：记录交易、平仓、信号、持仓、资金等关键信息到CSV文件
 * 7. 图表支持：支持策略图表、指标、标记等功能
 * 8. 回调处理：处理引擎的各种回调事件（Tick更新、K线更新、调度等）
 * 
 * 该类是CTA策略运行的核心实现，为策略提供完整的运行环境和交易接口。
 * 通过完整的状态管理和数据持久化，确保策略可以稳定运行并支持断点续传。
 */
#include "CtaStraBaseCtx.h"  // 包含CTA策略基础上下文头文件
#include "WtCtaEngine.h"    // 包含CTA引擎头文件
#include "WtHelper.h"        // 包含WonderTrader辅助工具头文件

#include <exception>         // 包含异常处理支持
#include <rapidjson/document.h>      // 包含RapidJSON文档解析器
#include <rapidjson/prettywriter.h> // 包含RapidJSON格式化写入器
namespace rj = rapidjson;    // 使用rapidjson命名空间别名

#include "../Share/StrUtil.hpp"         // 包含字符串工具函数
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息定义
#include "../Includes/WTSSessionInfo.hpp"   // 包含交易时段信息定义
#include "../Includes/IHotMgr.h"            // 包含主力合约管理器接口
#include "../Includes/WTSTradeDef.hpp"      // 包含交易定义
#include "../Share/decimal.h"               // 包含精确小数运算工具
#include "../Share/CodeHelper.hpp"          // 包含合约代码辅助工具

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

/**
 * @brief 比较算法名称数组
 * 
 * 用于将比较算法枚举值转换为可读的字符串，用于日志输出。
 * 索引对应WTSCompareType枚举值。
 */
const char* CMP_ALG_NAMES[] =
{
	"＝",  // 等于
	">",   // 大于
	"<",   // 小于
	">=",  // 大于等于
	"<="   // 小于等于
};

/**
 * @brief 动作名称数组
 * 
 * 用于将条件单动作类型转换为可读的字符串，用于日志输出。
 * 索引对应条件单动作常量（COND_ACTION_OL等）。
 */
const char* ACTION_NAMES[] =
{
	"OL",   // 开多（Open Long）
	"CL",   // 平多（Close Long）
	"OS",   // 开空（Open Short）
	"CS",   // 平空（Close Short）
	"SYN"   // 同步（Synchronize，直接设置仓位）
};

/**
 * @brief 生成CTA上下文ID
 * @return uint32_t 返回新的上下文ID
 * 
 * 使用原子操作生成唯一的上下文ID，从1开始递增。
 * 每个策略上下文实例都会获得一个唯一的ID。
 */
inline uint32_t makeCtaCtxId()
{
	static std::atomic<uint32_t> _auto_context_id{ 1 };  // 静态原子变量，初始值为1
	return _auto_context_id.fetch_add(1);                // 原子性地增加1并返回旧值
}


/**
 * @brief 构造函数实现
 * @param engine CTA引擎指针
 * @param name 策略上下文名称
 * @param slippage 滑点设置
 * 
 * 初始化CTA策略基础上下文对象，设置所有成员变量的初始值。
 * 调用makeCtaCtxId生成唯一的上下文ID。
 */
CtaStraBaseCtx::CtaStraBaseCtx(WtCtaEngine* engine, const char* name, int32_t slippage)
	: ICtaStraCtx(name)           // 调用基类构造函数，设置策略名称
	, _engine(engine)              // 设置CTA引擎指针
	, _total_calc_time(0)         // 初始化总计算时间为0
	, _emit_times(0)              // 初始化总计算次数为0
	, _last_cond_min(0)           // 初始化上次设置条件单的时间为0
	, _is_in_schedule(false)      // 初始化调度标记为false
	, _ud_modified(false)         // 初始化用户数据修改标记为false
	, _last_barno(0)              // 初始化上次K线编号为0
	, _slippage(slippage)         // 设置滑点参数
{
	_context_id = makeCtaCtxId();  // 生成唯一的上下文ID
}

/**
 * @brief 析构函数实现
 * 
 * 清理CTA策略基础上下文对象，释放相关资源。
 * 由于使用的是智能指针和标准容器，会自动释放资源。
 */
CtaStraBaseCtx::~CtaStraBaseCtx()
{
}

/**
 * @brief 初始化输出文件实现
 * 
 * 创建策略运行过程中的各种日志文件，包括：
 * - trades.csv: 交易成交记录
 * - closes.csv: 平仓记录
 * - funds.csv: 资金记录
 * - signals.csv: 交易信号记录
 * - positions.csv: 持仓记录
 * - indice.csv: 指标数据记录
 * - marks.csv: 图表标记记录
 * 
 * 如果文件已存在，则追加写入；如果不存在，则创建新文件并写入CSV表头。
 */
void CtaStraBaseCtx::init_outputs()
{
	std::string folder = WtHelper::getOutputDir();  // 获取输出目录路径
	folder += _name;                                // 追加策略名称
	folder += "//";                                 // 追加路径分隔符（注意：这里使用了双斜杠，可能是Windows路径）
	BoostFile::create_directories(folder.c_str());	// 创建输出目录（如果不存在）

	// 初始化交易日志文件
	std::string filename = folder + "trades.csv";   // 交易日志文件路径
	_trade_logs.reset(new BoostFile());             // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_trade_logs->create_or_open_file(filename.c_str());     // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：合约代码,时间,方向,动作,价格,数量,标签,手续费,K线编号
			_trade_logs->write_file("code,time,direct,action,price,qty,tag,fee,barno\n");
		}
		else  // 如果文件已存在
		{
			_trade_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	// 初始化平仓日志文件
	filename = folder + "closes.csv";               // 平仓日志文件路径
	_close_logs.reset(new BoostFile());             // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_close_logs->create_or_open_file(filename.c_str());     // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：合约代码,方向,开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,累计盈亏,开仓标签,平仓标签,开仓K线编号,平仓K线编号
			_close_logs->write_file("code,direct,opentime,openprice,closetime,closeprice,qty,profit,totalprofit,entertag,exittag,openbarno,closebarno\n");
		}
		else  // 如果文件已存在
		{
			_close_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	// 初始化资金日志文件
	filename = folder + "funds.csv";                // 资金日志文件路径
	_fund_logs.reset(new BoostFile());              // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_fund_logs->create_or_open_file(filename.c_str());      // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：日期,平仓盈亏,持仓盈亏,动态余额,手续费
			_fund_logs->write_file("date,closeprofit,positionprofit,dynbalance,fee\n");
		}
		else  // 如果文件已存在
		{
			_fund_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	// 初始化信号日志文件
	filename = folder + "signals.csv";              // 信号日志文件路径
	_sig_logs.reset(new BoostFile());              // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_sig_logs->create_or_open_file(filename.c_str());       // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：合约代码,目标仓位,信号价格,生成时间,用户标签
			_sig_logs->write_file("code,target,sigprice,gentime,usertag\n");
		}
		else  // 如果文件已存在
		{
			_sig_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	// 初始化持仓日志文件
	filename = folder + "positions.csv";            // 持仓日志文件路径
	_pos_logs.reset(new BoostFile());               // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_pos_logs->create_or_open_file(filename.c_str());        // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：日期,合约代码,持仓数量,平仓盈亏,浮动盈亏
			_pos_logs->write_file("date,code,volume,closeprofit,dynprofit\n");
		}
		else  // 如果文件已存在
		{
			_pos_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	// 初始化指标日志文件
	filename = folder + "indice.csv";               // 指标日志文件路径
	_idx_logs.reset(new BoostFile());               // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_idx_logs->create_or_open_file(filename.c_str());       // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：K线时间,指标名称,线条名称,数值
			_idx_logs->write_file("bartime,index_name,line_name,value\n");
		}
		else  // 如果文件已存在
		{
			_idx_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	// 初始化标记日志文件
	filename = folder + "marks.csv";                // 标记日志文件路径
	_mark_logs.reset(new BoostFile());              // 创建BoostFile对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 检查文件是否存在
		_mark_logs->create_or_open_file(filename.c_str());       // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			// 写入CSV表头：K线时间,价格,图标,标签
			_mark_logs->write_file("bartime,price,icon,tag\n");
		}
		else  // 如果文件已存在
		{
			_mark_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}
}

/**
 * @brief 记录交易信号实现
 * @param stdCode 合约代码
 * @param target 目标仓位
 * @param price 信号价格
 * @param gentime 信号生成时间
 * @param usertag 用户标签，默认为空字符串
 * 
 * 将交易信号信息写入信号日志文件（signals.csv）。
 * 格式：合约代码,目标仓位,信号价格,生成时间,用户标签
 */
void CtaStraBaseCtx::log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag /* = "" */)
{
	if (_sig_logs)  // 如果信号日志文件已初始化
	{
		std::stringstream ss;  // 创建字符串流
		// 格式化信号信息：合约代码,目标仓位,信号价格,生成时间,用户标签
		ss << stdCode << "," << target << "," << price << "," << gentime << "," << usertag << "\n";
		_sig_logs->write_file(ss.str());  // 写入日志文件
	}
}

/**
 * @brief 记录交易成交实现
 * @param stdCode 合约代码
 * @param isLong 是否多头
 * @param isOpen 是否开仓
 * @param curTime 成交时间
 * @param price 成交价格
 * @param qty 成交数量
 * @param userTag 用户标签，默认为空字符串
 * @param fee 手续费，默认为0.0
 * @param barNo K线编号，默认为0
 * 
 * 将交易成交信息写入交易日志文件（trades.csv），并通知引擎。
 * 格式：合约代码,时间,方向,动作,价格,数量,标签,手续费,K线编号
 */
void CtaStraBaseCtx::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag /* = "" */, double fee /* = 0.0 */, uint32_t barNo /* = 0 */)
{
	if (_trade_logs)  // 如果交易日志文件已初始化
	{
		std::stringstream ss;  // 创建字符串流
		// 格式化交易信息：合约代码,时间,方向(LONG/SHORT),动作(OPEN/CLOSE),价格,数量,标签,手续费,K线编号
		ss << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE") << "," << price << "," << qty << "," << userTag << "," << fee << "," << barNo << "\n";
		_trade_logs->write_file(ss.str());  // 写入日志文件
	}

	// 通知引擎交易事件，用于实时监控和图表显示
	_engine->notify_trade(this->name(),stdCode, isLong, isOpen, curTime, price, userTag);
}

/**
 * @brief 记录平仓信息实现
 * @param stdCode 合约代码
 * @param isLong 是否多头
 * @param openTime 开仓时间
 * @param openpx 开仓价格
 * @param closeTime 平仓时间
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 本次平仓盈亏
 * @param totalprofit 累计平仓盈亏，默认为0
 * @param enterTag 开仓标签，默认为空字符串
 * @param exitTag 平仓标签，默认为空字符串
 * @param openBarNo 开仓K线编号，默认为0
 * @param closeBarNo 平仓K线编号，默认为0
 * 
 * 将平仓信息写入平仓日志文件（closes.csv）。
 * 格式：合约代码,方向,开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,累计盈亏,开仓标签,平仓标签,开仓K线编号,平仓K线编号
 */
void CtaStraBaseCtx::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double totalprofit /* = 0 */, 
	const char* enterTag /* = "" */, const char* exitTag /* = "" */, uint32_t openBarNo /* = 0 */, uint32_t closeBarNo /* = 0 */)
{
	if (_close_logs)  // 如果平仓日志文件已初始化
	{
		std::stringstream ss;  // 创建字符串流
		// 格式化平仓信息：合约代码,方向,开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,累计盈亏,开仓标签,平仓标签,开仓K线编号,平仓K线编号
		ss << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
			<< "," << closeTime << "," << closepx << "," << qty << "," << profit << "," 
			<< totalprofit << "," << enterTag << "," << exitTag << "," << openBarNo << "," << closeBarNo << "\n";
		_close_logs->write_file(ss.str());  // 写入日志文件
	}
}
/**
 * @brief 保存用户数据实现
 * 
 * 将策略自定义的用户数据保存到JSON文件中。
 * 文件路径：用户数据目录/ud_策略名称.json
 * 只有用户数据被修改时才会调用此函数。
 */
void CtaStraBaseCtx::save_userdata()
{
	rj::Document root(rj::kObjectType);                    // 创建JSON文档对象
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器
	// 遍历用户数据映射表，将所有键值对添加到JSON文档中
	for (auto it = _user_datas.begin(); it != _user_datas.end(); it++)
	{
		// 添加JSON成员：key为字符串键，value为字符串值
		root.AddMember(rj::Value(it->first.c_str(), allocator), rj::Value(it->second.c_str(), allocator), allocator);
	}

	{
		// 构建文件路径：用户数据目录 + "ud_" + 策略名称 + ".json"
		std::string filename = WtHelper::getStraUsrDatDir();  // 获取用户数据目录
		filename += "ud_";                                    // 添加前缀
		filename += _name;                                    // 添加策略名称
		filename += ".json";                                  // 添加文件扩展名

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 创建新文件（如果已存在则覆盖）
		{
			rj::StringBuffer sb;                                    // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);          // 创建格式化写入器
			root.Accept(writer);                                    // 将JSON文档写入缓冲区（格式化）
			bf.write_file(sb.GetString());                          // 将格式化后的JSON字符串写入文件
			bf.close_file();                                        // 关闭文件
		}
	}
}

/**
 * @brief 加载用户数据实现
 * 
 * 从JSON文件中加载策略自定义的用户数据。
 * 文件路径：用户数据目录/ud_策略名称.json
 * 如果文件不存在或解析失败，则直接返回，不影响策略运行。
 */
void CtaStraBaseCtx::load_userdata()
{
	// 构建文件路径：用户数据目录 + "ud_" + 策略名称 + ".json"
	std::string filename = WtHelper::getStraUsrDatDir();  // 获取用户数据目录
	filename += "ud_";                                    // 添加前缀
	filename += _name;                                    // 添加策略名称
	filename += ".json";                                  // 添加文件扩展名

	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回，不做任何处理
	}

	std::string content;  // 文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;           // 直接返回

	rj::Document root;              // 创建JSON文档对象
	root.Parse(content.c_str());    // 解析JSON字符串

	if (root.HasParseError())  // 如果解析失败
		return;                 // 直接返回，不加载数据

	// 遍历JSON对象的所有成员，加载到用户数据映射表中
	for (auto& m : root.GetObject())
	{
		const char* key = m.name.GetString();   // 获取键（字符串）
		const char* val = m.value.GetString();  // 获取值（字符串）
		_user_datas[key] = val;                 // 存储到用户数据映射表中
	}
}

/**
 * @brief 加载策略数据实现
 * @param flag 加载标志，默认为0xFFFFFFFF（加载所有数据）
 * 
 * 从JSON文件中加载策略的持仓、资金、条件单、信号等数据。
 * 文件路径：策略数据目录/策略名称.json
 * 用于策略重启后恢复上次的状态，支持断点续传。
 * 
 * 加载的数据包括：
 * 1. 资金信息：累计平仓盈亏、累计浮动盈亏、累计手续费
 * 2. 持仓信息：每个合约的持仓、持仓明细、盈亏等
 * 3. 条件单：待触发的条件单列表
 * 4. 信号：待执行的交易信号
 * 5. 杂项：上次K线编号等
 * 
 * 一个完整的JSON例子：
 * {
 *   "fund": {
 *     "total_profit": 10000,
 *     "total_dynprofit": 10000,
 *     "tdate": 20210101,
 *     "total_fees": 10000
 *   }
 * }
 * {
 *   "positions": [
 *     {
 *       "code": "CFFEX.IF",
 *       "volume": 10,
 *       "closeprofit": 0,
 *       "dynprofit": 0
 *     }
 *   ]
 * }
 * {
 *   "conditions": [
 *     {
 *       "code": "CFFEX.IF",
 *       "volume": 10,
 *       "closeprofit": 0,
 *       "dynprofit": 0
 *     }
 *   ]
 * }
 * {
 *   "signals": [
 *     {
 *       "code": "CFFEX.IF",
 *       "volume": 10,
 *       "closeprofit": 0,
 *       "dynprofit": 0
 *     }
 *   ]
 * }
 * {
 *   "utils": {
 *     "last_barno": 10000
 *   }
 * }
 * {
 *   "signals": [
 *     {
 *       "code": "CFFEX.IF",
 *       "volume": 10,
 *       "closeprofit": 0,
 *       "dynprofit": 0
 *     }
 *   ]
 * }
 */
void CtaStraBaseCtx::load_data(uint32_t flag /* = 0xFFFFFFFF */)
{
	// 构建文件路径：策略数据目录 + 策略名称 + ".json"
	std::string filename = WtHelper::getStraDataDir();  // 获取策略数据目录
	filename += _name;                                   // 追加策略名称
	filename += ".json";                                 // 追加文件扩展名
	
	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回，使用默认值
	}

	std::string content;  // 文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;           // 直接返回

	rj::Document root;              // 创建JSON文档对象
	root.Parse(content.c_str());    // 解析JSON字符串

	if (root.HasParseError())  // 如果解析失败
		return;                 // 直接返回，不加载数据

	// 读取资金信息
	if(root.HasMember("fund"))  // 如果JSON中包含fund字段
	{
		//读取资金
		const rj::Value& jFund = root["fund"];  // 获取资金对象
		if(!jFund.IsNull() && jFund.IsObject())  // 如果资金对象有效
		{
			_fund_info._total_profit = jFund["total_profit"].GetDouble();      // 读取累计平仓盈亏
			_fund_info._total_dynprofit = jFund["total_dynprofit"].GetDouble(); // 读取累计浮动盈亏
			uint32_t tdate = jFund["tdate"].GetUint();                         // 读取交易日（暂不使用）
			_fund_info._total_fees = jFund["total_fees"].GetDouble();          // 读取累计手续费
		}
	}

	{//读取仓位
		double total_profit = 0;      // 累计平仓盈亏（用于验证）
		double total_dynprofit = 0;   // 累计浮动盈亏（用于验证）
		const rj::Value& jPos = root["positions"];  // 获取持仓数组
		if (!jPos.IsNull() && jPos.IsArray())  // 如果持仓数组有效
		{
			// 遍历每个合约的持仓信息
			for (const rj::Value& pItem : jPos.GetArray())
			{
				const char* stdCode = pItem["code"].GetString();                    // 获取合约代码
				const char* ruleTag = _engine->get_hot_mgr()->getRuleTag(stdCode);  // 获取主力合约规则标签
				// 判断合约是否过期：如果没有规则标签且合约信息不存在，则认为已过期
				bool isExpired = (strlen(ruleTag) == 0 && _engine->get_contract_info(stdCode) == NULL);

				if(isExpired)  // 如果合约已过期
					log_info("{} not exists or expired, position ignored", stdCode);  // 记录日志

				PosInfo& pInfo = _pos_map[stdCode];  // 获取或创建该合约的持仓信息
				pInfo._closeprofit = pItem["closeprofit"].GetDouble();        // 读取平仓盈亏
				pInfo._last_entertime = pItem["lastentertime"].GetUint64();   // 读取最后开仓时间
				pInfo._last_exittime = pItem["lastexittime"].GetUint64();     // 读取最后平仓时间
				pInfo._volume = isExpired ? 0 : pItem["volume"].GetDouble();  // 读取持仓数量（如果过期则为0）
				// 读取冻结持仓信息（T+1品种）
				if (pItem.HasMember("frozen") && !isExpired)  // 如果存在冻结持仓字段且合约未过期
				{
					pInfo._frozen = pItem["frozen"].GetDouble();         // 读取冻结持仓数量
					pInfo._frozen_date = pItem["frozendate"].GetUint();  // 读取冻结日期
				}

				if (pInfo._volume == 0 || isExpired)  // 如果持仓为0或合约已过期
				{
					//By Wesley @ 2023.02.21
					//加这一行的原因是，有些期权合约经常会持有到交割日
					//所以如果合约过期了，那么需要把浮动盈亏当做平仓盈亏累加一下
					//处理完以后，下一次加载，浮动盈亏就是0了
					pInfo._closeprofit += pInfo._dynprofit;  // 将浮动盈亏累加到平仓盈亏中

					pInfo._dynprofit = 0;   // 浮动盈亏清零
					pInfo._frozen = 0;      // 冻结持仓清零
				}
				else  // 如果持仓不为0且合约未过期
					pInfo._dynprofit = pItem["dynprofit"].GetDouble();  // 读取浮动盈亏

				total_profit += pInfo._closeprofit;      // 累加平仓盈亏
				total_dynprofit += pInfo._dynprofit;     // 累加浮动盈亏

				// 读取持仓明细列表
				const rj::Value& details = pItem["details"];  // 获取持仓明细数组
				// 如果明细为空、不是数组、大小为0或合约已过期，则跳过
				if (details.IsNull() || !details.IsArray() || details.Size() == 0 || isExpired)
					continue;

				// 遍历每个持仓明细
				for (uint32_t i = 0; i < details.Size(); i++)
				{
					const rj::Value& dItem = details[i];        // 获取第i个明细项
					double vol = dItem["volume"].GetDouble();   // 读取持仓数量
					if(decimal::eq(vol, 0))                     // 如果持仓数量为0
						continue;                               // 跳过该明细

					DetailInfo dInfo;  // 创建持仓明细信息对象
					dInfo._long = dItem["long"].GetBool();      // 读取是否多头
					dInfo._price = dItem["price"].GetDouble();  // 读取开仓价格
					dInfo._volume = dItem["volume"].GetDouble(); // 读取持仓数量
					dInfo._opentime = dItem["opentime"].GetUint64();  // 读取开仓时间
					// 读取开仓交易日（如果存在）
					if(dItem.HasMember("opentdate"))
						dInfo._opentdate = dItem["opentdate"].GetUint();
					else
						dInfo._opentdate = 0;  // 默认为0

					// 读取最高价格（如果存在，否则使用开仓价格）
					if (dItem.HasMember("maxprice"))
						dInfo._max_price = dItem["maxprice"].GetDouble();
					else
						dInfo._max_price = dInfo._price;  // 默认使用开仓价格

					// 读取最低价格（如果存在，否则使用开仓价格）
					if (dItem.HasMember("minprice"))
						dInfo._min_price = dItem["minprice"].GetDouble();
					else
						dInfo._min_price = dInfo._price;  // 默认使用开仓价格

					dInfo._profit = dItem["profit"].GetDouble();       // 读取当前盈亏
					dInfo._max_profit = dItem["maxprofit"].GetDouble(); // 读取最大盈利
					dInfo._max_loss = dItem["maxloss"].GetDouble();     // 读取最大亏损

					strcpy(dInfo._opentag, dItem["opentag"].GetString());  // 读取开仓标签
					// 读取开仓K线编号（如果存在）
					if (dItem.HasMember("openbarno"))
						dInfo._open_barno = dItem["openbarno"].GetUint();
					else
						dInfo._open_barno = 0;  // 默认为0

					pInfo._details.emplace_back(dInfo);  // 将明细添加到持仓明细列表中
				}

				if(!isExpired)  // 如果合约未过期
				{
					log_info("Position confirmed,{} -> {}", stdCode, pInfo._volume);  // 记录日志
					stra_sub_ticks(stdCode);  // 订阅该合约的Tick数据
				}
			}
		}

		// 更新资金信息（使用累加后的总盈亏）
		_fund_info._total_profit = total_profit;       // 设置累计平仓盈亏
		_fund_info._total_dynprofit = total_dynprofit; // 设置累计浮动盈亏
	}

	{//读取条件单
	/* 一个JSON例子：
	{
		"conditions": {
			"settime": 1715769600,
			"items": {
				"CODE1": [
					{
						"usertag": "user1",
						"field": 0,
						"alg": 0,
						"target": 10000,
						"qty": 1,
						"action": 0
					},
					{
						"usertag": "user1",
						"field": 0,
						"alg": 0,
						"target": 10000,
						"qty": 1,
						"action": 0
					}
				],
				"CODE2": [
					{
						"usertag": "user2",
						"field": 0,
						"alg": 0,
						"target": 10000,
						"qty": 1,
						"action": 0
					}
				]
			}
		}
	}
	*/
		uint32_t count = 0;  // 条件单计数器
		const rj::Value& jCond = root["conditions"];  // 获取条件单对象
		if (!jCond.IsNull() && jCond.IsObject())  // 如果条件单对象有效
		{
			_last_cond_min = jCond["settime"].GetUint64();  // 读取条件单设置时间
			const rj::Value& jItems = jCond["items"];        // 获取条件单项对象
			// 遍历每个合约的条件单列表
			for (auto& m : jItems.GetObject())
			{
				const char* stdCode = m.name.GetString();                    // 获取合约代码
				const char* ruleTag = _engine->get_hot_mgr()->getRuleTag(stdCode);  // 获取主力合约规则标签
				// 判断合约是否过期
				if (strlen(ruleTag) == 0 && _engine->get_contract_info(stdCode) == NULL)
				{
					log_info("{} not exists or expired, condition ignored", stdCode);  // 记录日志
					continue;  // 跳过该合约的条件单
				}

				const rj::Value& cListItem = m.value;  // 获取该合约的条件单数组

				CondList& condList = _condtions[stdCode];  // 获取或创建该合约的条件单列表

				// 遍历该合约的每个条件单
				for(auto& cItem : cListItem.GetArray())
				{
					CondEntrust condInfo;  // 创建条件单委托对象
					strcpy(condInfo._code, stdCode);                                    // 设置合约代码
					strcpy(condInfo._usertag, cItem["usertag"].GetString());            // 设置用户标签

					condInfo._field = (WTSCompareField)cItem["field"].GetUint();        // 设置比较字段
					condInfo._alg = (WTSCompareType)cItem["alg"].GetUint();             // 设置比较算法
					condInfo._target = cItem["target"].GetDouble();                     // 设置目标值
					condInfo._qty = cItem["qty"].GetDouble();                           // 设置数量
					condInfo._action = (char)cItem["action"].GetUint();                 // 设置动作类型

					condList.emplace_back(condInfo);  // 将条件单添加到条件单列表中

					// 记录日志：合约代码, 动作, 数量, 比较算法, 目标值
					log_info("{} condition recovered, {} {}, condition: newprice {} {}",
						stdCode, ACTION_NAMES[condInfo._action], condInfo._qty, CMP_ALG_NAMES[condInfo._alg], condInfo._target);
					count++;  // 增加计数器
				}
			}

			// 记录总的条件单恢复数量
			log_info("{} conditions recovered, setup time: {}", count, _last_cond_min);
		}
	}

	// 读取交易信号
	/* 一个JSON例子：
	{
		"signals": {
			"CODE1": {
				"usertag": "user1",
				"volume": 1,
				"sigprice": 10000,
				"gentime": 1715769600
			}
		}
	}
	*/
	if (root.HasMember("signals"))  // 如果JSON中包含signals字段
	{
		//读取信号
		const rj::Value& jSignals = root["signals"];  // 获取信号对象
		if (!jSignals.IsNull() && jSignals.IsObject())  // 如果信号对象有效
		{
			// 遍历每个合约的信号
			for (auto& m : jSignals.GetObject())
			{
				const char* stdCode = m.name.GetString();                    // 获取合约代码
				const char* ruleTag = _engine->get_hot_mgr()->getRuleTag(stdCode);  // 获取主力合约规则标签
				// 判断合约是否过期
				if (strlen(ruleTag) == 0 && _engine->get_contract_info(stdCode) == NULL)
				{
					log_info("{} not exists or expired, signal ignored", stdCode);  // 记录日志
					continue;  // 跳过该合约的信号
				}

				const rj::Value& jItem = m.value;  // 获取信号对象

				SigInfo& sInfo = _sig_map[stdCode];  // 获取或创建该合约的信号信息
				sInfo._usertag = jItem["usertag"].GetString();     // 读取用户标签
				sInfo._volume = jItem["volume"].GetDouble();        // 读取目标仓位
				sInfo._sigprice = jItem["sigprice"].GetDouble();    // 读取信号价格
				sInfo._gentime = jItem["gentime"].GetUint64();     // 读取生成时间
				
				log_info("{} untouched signal recovered, target pos: {}", stdCode, sInfo._volume);  // 记录日志
				stra_sub_ticks(stdCode);  // 订阅该合约的Tick数据
			}
		}
	}

	// 读取杂项信息
	if (root.HasMember("utils"))  // 如果JSON中包含utils字段
	{
		//读取杂项
		const rj::Value& jUtils = root["utils"];  // 获取杂项对象
		if (!jUtils.IsNull() && jUtils.IsObject())  // 如果杂项对象有效
		{
			_last_barno = jUtils["lastbarno"].GetUint();  // 读取上次K线编号
		}
	}
}

/**
 * @brief 保存策略数据实现
 * @param flag 保存标志，默认为0xFFFFFFFF（保存所有数据）
 * 
 * 将策略的持仓、资金、条件单、信号等数据保存到JSON文件中。
 * 文件路径：策略数据目录/策略名称.json
 * 支持断点续传，策略重启后可以恢复到上次的状态。
 * 
 * 保存的数据包括：
 * 1. 持仓信息：每个合约的持仓、持仓明细、盈亏等
 * 2. 资金信息：累计平仓盈亏、累计浮动盈亏、累计手续费
 * 3. 交易信号：待执行的交易信号
 * 4. 条件单：待触发的条件单列表
 * 5. 杂项：上次K线编号等
 */
void CtaStraBaseCtx::save_data(uint32_t flag /* = 0xFFFFFFFF */)
{
	rj::Document root(rj::kObjectType);  // 创建JSON文档对象

	{//持仓数据保存
		rj::Value jPos(rj::kArrayType);  // 创建持仓数组

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器

		// 遍历所有合约的持仓信息
		for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			const PosInfo& pInfo = it->second;        // 获取持仓信息

			rj::Value pItem(rj::kObjectType);  // 创建持仓项对象
			pItem.AddMember("code", rj::Value(stdCode, allocator), allocator);           // 添加合约代码
			pItem.AddMember("volume", pInfo._volume, allocator);                         // 添加持仓数量
			pItem.AddMember("closeprofit", pInfo._closeprofit, allocator);              // 添加平仓盈亏
			pItem.AddMember("dynprofit", pInfo._dynprofit, allocator);                  // 添加浮动盈亏
			pItem.AddMember("lastentertime", pInfo._last_entertime, allocator);         // 添加最后开仓时间
			pItem.AddMember("lastexittime", pInfo._last_exittime, allocator);           // 添加最后平仓时间
			pItem.AddMember("frozen", pInfo._frozen, allocator);                        // 添加冻结持仓
			pItem.AddMember("frozendate", pInfo._frozen_date, allocator);               // 添加冻结日期

			// 保存持仓明细列表
			rj::Value details(rj::kArrayType);  // 创建明细数组
			for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历持仓明细
			{
				const DetailInfo& dInfo = *dit;  // 获取明细信息
				rj::Value dItem(rj::kObjectType);  // 创建明细项对象
				dItem.AddMember("long", dInfo._long, allocator);              // 添加是否多头
				dItem.AddMember("price", dInfo._price, allocator);            // 添加开仓价格
				dItem.AddMember("maxprice", dInfo._max_price, allocator);     // 添加最高价格
				dItem.AddMember("minprice", dInfo._min_price, allocator);     // 添加最低价格
				dItem.AddMember("volume", dInfo._volume, allocator);          // 添加持仓数量
				dItem.AddMember("opentime", dInfo._opentime, allocator);     // 添加开仓时间
				dItem.AddMember("opentdate", dInfo._opentdate, allocator);   // 添加开仓交易日

				dItem.AddMember("profit", dInfo._profit, allocator);         // 添加当前盈亏
				dItem.AddMember("maxprofit", dInfo._max_profit, allocator);  // 添加最大盈利
				dItem.AddMember("maxloss", dInfo._max_loss, allocator);     // 添加最大亏损
				dItem.AddMember("opentag", rj::Value(dInfo._opentag, allocator), allocator);  // 添加开仓标签
				dItem.AddMember("openbarno", dInfo._open_barno, allocator);  // 添加开仓K线编号

				details.PushBack(dItem, allocator);  // 将明细项添加到明细数组中
			}

			pItem.AddMember("details", details, allocator);  // 将明细数组添加到持仓项中

			jPos.PushBack(pItem, allocator);  // 将持仓项添加到持仓数组中
		}

		root.AddMember("positions", jPos, allocator);  // 将持仓数组添加到根对象中
	}

	{//资金保存
		rj::Value jFund(rj::kObjectType);  // 创建资金对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器

		jFund.AddMember("total_profit", _fund_info._total_profit, allocator);        // 添加累计平仓盈亏
		jFund.AddMember("total_dynprofit", _fund_info._total_dynprofit, allocator);  // 添加累计浮动盈亏
		jFund.AddMember("total_fees", _fund_info._total_fees, allocator);            // 添加累计手续费
		jFund.AddMember("tdate", _engine->get_trading_date(), allocator);            // 添加交易日

		root.AddMember("fund", jFund, allocator);  // 将资金对象添加到根对象中
	}

	{//信号保存
		rj::Value jSigs(rj::kObjectType);  // 创建信号对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器

		// 遍历所有合约的交易信号
		for (auto& m:_sig_map)
		{
			const char* stdCode = m.first.c_str();  // 获取合约代码
			const SigInfo& sInfo = m.second;          // 获取信号信息

			rj::Value jItem(rj::kObjectType);  // 创建信号项对象
			jItem.AddMember("usertag", rj::Value(sInfo._usertag.c_str(), allocator), allocator);  // 添加用户标签

			jItem.AddMember("volume", sInfo._volume, allocator);      // 添加目标仓位
			jItem.AddMember("sigprice", sInfo._sigprice, allocator);  // 添加信号价格
			jItem.AddMember("gentime", sInfo._gentime, allocator);    // 添加生成时间

			jSigs.AddMember(rj::Value(stdCode, allocator), jItem, allocator);  // 将信号项添加到信号对象中（key为合约代码）
		}

		root.AddMember("signals", jSigs, allocator);  // 将信号对象添加到根对象中
	}

	{//条件单保存
		rj::Value jCond(rj::kObjectType);  // 创建条件单对象
		rj::Value jItems(rj::kObjectType);  // 创建条件单项对象

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器

		// 遍历所有合约的条件单列表
		for (auto it = _condtions.begin(); it != _condtions.end(); it++)
		{
			const char* code = it->first.c_str();     // 获取合约代码
			const CondList& condList = it->second;     // 获取条件单列表

			rj::Value cArray(rj::kArrayType);  // 创建条件单数组
			// 遍历该合约的每个条件单
			for(auto& condInfo : condList)
			{
				rj::Value cItem(rj::kObjectType);  // 创建条件单项对象
				cItem.AddMember("code", rj::Value(code, allocator), allocator);                    // 添加合约代码
				cItem.AddMember("usertag", rj::Value(condInfo._usertag, allocator), allocator);    // 添加用户标签

				cItem.AddMember("field", (uint32_t)condInfo._field, allocator);     // 添加比较字段
				cItem.AddMember("alg", (uint32_t)condInfo._alg, allocator);         // 添加比较算法
				cItem.AddMember("target", condInfo._target, allocator);              // 添加目标值
				cItem.AddMember("qty", condInfo._qty, allocator);                   // 添加数量
				cItem.AddMember("action", (uint32_t)condInfo._action, allocator);   // 添加动作类型

				cArray.PushBack(cItem, allocator);  // 将条件单项添加到条件单数组中
			}

			jItems.AddMember(rj::Value(code, allocator), cArray, allocator);  // 将条件单数组添加到条件单项对象中（key为合约代码）
		}
		jCond.AddMember("settime", _last_cond_min, allocator);  // 添加条件单设置时间
		jCond.AddMember("items", jItems, allocator);            // 将条件单项对象添加到条件单对象中

		root.AddMember("conditions", jCond, allocator);  // 将条件单对象添加到根对象中
	}

	{//杂项保存
		rj::Value jUtils(rj::kObjectType);  // 创建杂项对象

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器

		jUtils.AddMember("lastbarno", _last_barno, allocator);  // 添加上次K线编号

		root.AddMember("utils", jUtils, allocator);  // 将杂项对象添加到根对象中
	}

	{
		// 构建文件路径：策略数据目录 + 策略名称 + ".json"
		std::string filename = WtHelper::getStraDataDir();  // 获取策略数据目录
		filename += _name;                                   // 追加策略名称
		filename += ".json";                                 // 追加文件扩展名

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 创建新文件（如果已存在则覆盖）
		{
			rj::StringBuffer sb;                                    // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);          // 创建格式化写入器
			root.Accept(writer);                                    // 将JSON文档写入缓冲区（格式化）
			bf.write_file(sb.GetString());                          // 将格式化后的JSON字符串写入文件
			bf.close_file();                                        // 关闭文件
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//回调函数实现
//////////////////////////////////////////////////////////////////////////

/**
 * @brief K线数据更新回调实现
 * @param stdCode 合约代码
 * @param period K线周期，如"m"、"d"等
 * @param times 周期倍数，如"m5"中的5
 * @param newBar 新的K线数据指针
 * 
 * 当收到新的K线数据时调用，用于标记K线闭合状态。
 * 如果该K线订阅了闭合事件通知，则触发on_bar_close回调。
 * 如果是主K线闭合，则记录调试日志。
 */
void CtaStraBaseCtx::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (newBar == NULL)  // 如果K线数据为空
		return;          // 直接返回

	// 构建实际周期字符串（如"m5"、"d1"等）
	thread_local static char realPeriod[8] = { 0 };  // 线程局部静态变量，用于存储实际周期字符串
	fmtutil::format_to(realPeriod, "{}{}", period, times);  // 格式化：周期 + 倍数

	// 构建K线键值（格式：合约代码#周期）
	thread_local static char key[64] = { 0 };  // 线程局部静态变量，用于存储K线键值
	fmtutil::format_to(key, "{}#{}", stdCode, realPeriod);  // 格式化：合约代码 + "#" + 实际周期

	KlineTag& tag = _kline_tags[key];  // 获取或创建该K线的标记
	tag._closed = true;                // 标记K线已闭合

	if(tag._notify)  // 如果该K线订阅了闭合事件通知
		on_bar_close(stdCode, realPeriod, newBar);  // 触发K线闭合回调

	if(key == _main_key)  // 如果是主K线
		log_debug("Main KBars {} closed", key);  // 记录调试日志
}

/**
 * @brief 策略初始化回调实现
 * 
 * 策略初始化时调用，执行顺序：
 * 1. 初始化输出文件（创建各种日志文件）
 * 2. 加载策略数据（持仓、资金、条件单、信号等）
 * 3. 加载用户数据（策略自定义数据）
 * 
 * 该函数在策略启动时自动调用，用于恢复策略的状态。
 */
void CtaStraBaseCtx::on_init()
{
	init_outputs();  // 初始化输出文件（创建各种日志文件）

	//读取数据
	load_data();  // 加载策略数据（持仓、资金、条件单、信号等）

	//加载用户数据
	load_userdata();  // 加载用户数据（策略自定义数据）
}

/**
 * @brief 导出图表信息实现
 * 
 * 将策略的图表配置信息导出到JSON文件中。
 * 文件路径：输出目录/策略名称/rtchart.json
 * 包括主K线、指标、指标线、基准线等配置信息。
 * 用于实时图表显示和历史回放。
 */
void CtaStraBaseCtx::dump_chart_info()
{
	rj::Document root(rj::kObjectType);                    // 创建JSON文档对象
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器

	// 保存K线配置信息
	rj::Value klineItem(rj::kObjectType);  // 创建K线项对象
	if (_chart_code.empty())  // 如果没有设置图表合约代码
	{
		//如果没有设置主K线，就用主K线落地
		klineItem.AddMember("code", rj::Value(_main_code.c_str(), allocator), allocator);      // 使用主K线合约代码
		klineItem.AddMember("period", rj::Value(_main_period.c_str(), allocator), allocator);  // 使用主K线周期
	}
	else  // 如果设置了图表合约代码
	{
		klineItem.AddMember("code", rj::Value(_chart_code.c_str(), allocator), allocator);      // 使用图表合约代码
		klineItem.AddMember("period", rj::Value(_chart_period.c_str(), allocator), allocator);  // 使用图表周期
	}

	root.AddMember("kline", klineItem, allocator);  // 将K线项添加到根对象中

	// 保存指标配置信息
	if (!_chart_indice.empty())  // 如果有指标配置
	{
		rj::Value jIndice(rj::kArrayType);  // 创建指标数组
		// 遍历所有指标
		for (const auto& v : _chart_indice)
		{
			const ChartIndex& cIndex = v.second;  // 获取指标信息
			rj::Value jIndex(rj::kObjectType);    // 创建指标项对象
			jIndex.AddMember("name", rj::Value(cIndex._name.c_str(), allocator), allocator);      // 添加指标名称
			jIndex.AddMember("index_type", cIndex._indexType, allocator);                          // 添加指标类型

			// 保存指标线配置
			rj::Value jLines(rj::kArrayType);  // 创建指标线数组
			for (const auto& v2 : cIndex._lines)  // 遍历指标的所有线条
			{
				const ChartLine& cLine = v2.second;  // 获取线条信息
				rj::Value jLine(rj::kObjectType);     // 创建线条项对象
				jLine.AddMember("name", rj::Value(cLine._name.c_str(), allocator), allocator);    // 添加线条名称
				jLine.AddMember("line_type", cLine._lineType, allocator);                         // 添加线条类型

				jLines.PushBack(jLine, allocator);  // 将线条项添加到指标线数组中
			}

			jIndex.AddMember("lines", jLines, allocator);  // 将指标线数组添加到指标项中

			// 保存基准线配置
			rj::Value jBaseLines(rj::kObjectType);  // 创建基准线对象
			for (const auto& v3 : cIndex._base_lines)  // 遍历指标的所有基准线
			{
				// 添加基准线：key为线条名称，value为基准值
				jBaseLines.AddMember(rj::Value(v3.first.c_str(), allocator), rj::Value(v3.second), allocator);
			}

			jIndex.AddMember("baselines", jBaseLines, allocator);  // 将基准线对象添加到指标项中

			jIndice.PushBack(jIndex, allocator);  // 将指标项添加到指标数组中
		}

		root.AddMember("index", jIndice, allocator);  // 将指标数组添加到根对象中
	}

	// 构建文件路径：输出目录/策略名称/rtchart.json
	std::string folder = WtHelper::getOutputDir();  // 获取输出目录
	folder += _name;                                // 追加策略名称
	folder += "/";                                  // 追加路径分隔符

	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		boost::filesystem::create_directories(folder.c_str());  // 创建目录

	std::string filename = folder;         // 文件路径
	filename += "rtchart.json";           // 追加文件名

	rj::StringBuffer sb;                                    // 创建字符串缓冲区
	rj::PrettyWriter<rj::StringBuffer> writer(sb);          // 创建格式化写入器
	root.Accept(writer);                                    // 将JSON文档写入缓冲区（格式化）
	StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将格式化后的JSON字符串写入文件
}

/**
 * @brief 更新浮动盈亏实现
 * @param stdCode 合约代码
 * @param price 当前价格
 * 
 * 根据当前价格计算并更新指定合约的浮动盈亏。
 * 浮动盈亏计算公式：
 * - 多头：持仓数量 * (当前价格 - 开仓价格) * 合约乘数
 * - 空头：持仓数量 * (开仓价格 - 当前价格) * 合约乘数
 * 
 * 同时更新每个持仓明细的盈亏、最大盈利、最大亏损、最高价格、最低价格等统计信息。
 * 最后更新该合约的总浮动盈亏和策略的总浮动盈亏。
 */
void CtaStraBaseCtx::update_dyn_profit(const char* stdCode, double price)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it != _pos_map.end())  // 如果找到持仓信息
	{
		PosInfo& pInfo = (PosInfo&)it->second;  // 获取持仓信息引用
		if (pInfo._volume == 0)  // 如果持仓数量为0
		{
			pInfo._dynprofit = 0;  // 浮动盈亏清零
		}
		else  // 如果持仓数量不为0
		{
			WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取品种信息
			double dynprofit = 0;  // 该合约的总浮动盈亏
			// 遍历每个持仓明细，计算浮动盈亏
			for (auto pit = pInfo._details.begin(); pit != pInfo._details.end(); pit++)
			{
				DetailInfo& dInfo = *pit;  // 获取持仓明细引用
				// 计算该明细的浮动盈亏：持仓数量 * (当前价格 - 开仓价格) * 合约乘数 * 方向系数（多头为1，空头为-1）
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);
				// 更新最大盈利（如果当前盈亏大于历史最大盈利）
				if (dInfo._profit > 0)
					dInfo._max_profit = std::max(dInfo._profit, dInfo._max_profit);
				// 更新最大亏损（如果当前盈亏小于历史最大亏损）
				else if (dInfo._profit < 0)
					dInfo._max_loss = std::min(dInfo._profit, dInfo._max_loss);

				// 更新最高价格（如果当前价格大于历史最高价格）
				dInfo._max_price = std::max(dInfo._max_price, price);
				// 更新最低价格（如果当前价格小于历史最低价格）
				dInfo._min_price = std::min(dInfo._min_price, price);

				dynprofit += dInfo._profit;  // 累加该明细的浮动盈亏
			}

			pInfo._dynprofit = dynprofit;  // 设置该合约的总浮动盈亏
		}
	}

	// 计算策略的总浮动盈亏（所有合约的浮动盈亏之和）
	double total_dynprofit = 0;  // 策略总浮动盈亏
	for(auto& v : _pos_map)  // 遍历所有合约的持仓
	{
		const PosInfo& pInfo = v.second;  // 获取持仓信息
		total_dynprofit += pInfo._dynprofit;  // 累加浮动盈亏
	}

	_fund_info._total_dynprofit = total_dynprofit;  // 更新策略的总浮动盈亏
}

/**
 * @brief Tick数据更新回调实现
 * @param stdCode 合约代码
 * @param newTick 新的Tick数据指针
 * @param bEmitStrategy 是否触发策略回调，默认为true
 * 
 * 当收到新的Tick数据时调用，执行以下操作：
 * 1. 更新价格缓存
 * 2. 检查并处理交易信号（如果存在且处于交易时段）
 * 3. 更新浮动盈亏
 * 4. 检查条件单是否触发（如果存在）
 * 5. 触发策略的on_tick_updated回调（如果bEmitStrategy为true）
 * 6. 保存用户数据（如果已修改）
 * 
 * 这是策略运行的核心函数之一，负责处理实时市场数据和交易执行。
 */
void CtaStraBaseCtx::on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy /* = true */)
{
	_price_map[stdCode] = newTick->price();  // 更新价格缓存

	//先检查是否要信号要触发
	{
		auto it = _sig_map.find(stdCode);  // 查找该合约的交易信号
		if(it != _sig_map.end())  // 如果存在交易信号
		{
			WTSSessionInfo* sInfo = _engine->get_session_info(stdCode, true);  // 获取交易时段信息
			if (sInfo->isInTradingTime(_engine->get_raw_time(), true))  // 如果当前处于交易时段
			{
				const SigInfo sInfo = it->second;  // 获取信号信息
				//只有当信号类型不为0，即bar内信号或者条件单触发信号时，且信号没有触发过
				// bFireAtOnce为true表示立即执行（条件单触发或Tick信号且未触发过）
				do_set_position(stdCode, sInfo._volume, sInfo._usertag.c_str(), (sInfo._sigtype != 0 && !sInfo._triggered));
				_sig_map.erase(it);  // 删除已处理的信号

				//如果是条件单触发，则回调on_condition_triggered
				if(sInfo._sigtype == 2)  // 如果信号类型为条件单触发信号
					on_condition_triggered(stdCode, sInfo._volume, newTick->price(), sInfo._usertag.c_str());  // 触发条件单回调
			}
			
		}
	}

	//更新浮动盈亏
	update_dyn_profit(stdCode, newTick->price());  // 根据最新价格更新浮动盈亏

	//////////////////////////////////////////////////////////////////////////
	//检查条件单
	if(!_condtions.empty())  // 如果存在条件单
	{
		auto it = _condtions.find(stdCode);  // 查找该合约的条件单列表
		if (it == _condtions.end())  // 如果该合约没有条件单
			return;                  // 直接返回

		const CondList& condList = it->second;  // 获取条件单列表
		// 遍历该合约的所有条件单
		for (const CondEntrust& entrust : condList)
		{
			double curPrice = newTick->price();  // 获取当前价格

			// 根据比较算法判断条件是否满足
			bool isMatched = false;  // 条件是否匹配
			switch (entrust._alg)  // 根据比较算法类型
			{
			case WCT_Equal:  // 等于
				isMatched = decimal::eq(curPrice, entrust._target);  // 精确比较当前价格是否等于目标价格
				break;
			case WCT_Larger:  // 大于
				isMatched = decimal::gt(curPrice, entrust._target);  // 判断当前价格是否大于目标价格
				break;
			case WCT_LargerOrEqual:  // 大于等于
				isMatched = decimal::ge(curPrice, entrust._target);  // 判断当前价格是否大于等于目标价格
				break;
			case WCT_Smaller:  // 小于
				isMatched = decimal::lt(curPrice, entrust._target);  // 判断当前价格是否小于目标价格
				break;
			case WCT_SmallerOrEqual:  // 小于等于
				isMatched = decimal::le(curPrice, entrust._target);  // 判断当前价格是否小于等于目标价格
				break;
			default:
				break;
			}

			if (isMatched)  // 如果条件满足
			{
				// 记录条件单触发日志：当前价格、比较算法、目标价格、合约代码、动作、数量
				log_info("Condition triggered[newprice {}{} targetprice {}], instrument: {}, {} {}",
					curPrice, CMP_ALG_NAMES[entrust._alg], entrust._target, stdCode, ACTION_NAMES[entrust._action], entrust._qty);

				// 根据动作类型执行相应的交易操作
				switch (entrust._action)
				{
				case COND_ACTION_OL:  // 开多
				{
					double curQty = stra_get_position(stdCode);  // 获取当前持仓
					double desQty = 0;                           // 目标仓位
					if (decimal::lt(curQty, 0))  // 如果当前有空仓
						desQty = entrust._qty;   // 目标仓位为开多数量（先平空再开多）
					else  // 如果当前没有空仓
						desQty = curQty + entrust._qty;  // 目标仓位为当前持仓加上开多数量

					append_signal(stdCode, desQty, entrust._usertag, 2);  // 添加交易信号（类型为条件单触发）
				}
				break;
				case COND_ACTION_CL:  // 平多
				{
					double curQty = stra_get_position(stdCode);  // 获取当前持仓
					if (decimal::gt(curQty, 0))  // 如果当前有多仓
					{
						double maxQty = min(curQty, entrust._qty);  // 平仓数量不能超过当前持仓
						double desQty = curQty - maxQty;            // 目标仓位为当前持仓减去平仓数量
						append_signal(stdCode, desQty, entrust._usertag, 2);  // 添加交易信号（类型为条件单触发）
					}
				}
				break;
				case COND_ACTION_OS:  // 开空
				{
					double curQty = stra_get_position(stdCode);  // 获取当前持仓
					double desQty = 0;                           // 目标仓位
					if (decimal::gt(curQty, 0))  // 如果当前有多仓
						desQty = -entrust._qty;  // 目标仓位为负的开空数量（先平多再开空）
					else  // 如果当前没有多仓
						desQty = curQty - entrust._qty;  // 目标仓位为当前持仓减去开空数量

					append_signal(stdCode, desQty, entrust._usertag, 2);  // 添加交易信号（类型为条件单触发）
				}
				break;
				case COND_ACTION_CS:  // 平空
				{
					double curQty = stra_get_position(stdCode);  // 获取当前持仓
					if (decimal::lt(curQty, 0))  // 如果当前有空仓
					{
						double maxQty = min(abs(curQty), entrust._qty);  // 平仓数量不能超过当前持仓的绝对值
						double desQty = curQty + maxQty;                // 目标仓位为当前持仓加上平仓数量
						append_signal(stdCode, desQty, entrust._usertag, 2);  // 添加交易信号（类型为条件单触发）
					}
				}
				break;
				case COND_ACTION_SP:  // 直接设置仓位
				{
					append_signal(stdCode, entrust._qty, entrust._usertag, 2);  // 添加交易信号，直接设置目标仓位
				}
				break;
				default: break;
				}

				//同一个bar设置针对同一个合约的条件单, 只可能触发一条
				//所以这里直接清理掉即可
				_condtions.erase(it);  // 删除该合约的所有条件单（避免重复触发）
				break;  // 跳出循环
			}
		}
	}

	if (bEmitStrategy)  // 如果允许触发策略回调
		on_tick_updated(stdCode, newTick);  // 触发策略的on_tick_updated回调

	if(_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();   // 保存用户数据
		_ud_modified = false;  // 重置修改标记
	}
}

/**
 * @brief 策略调度回调实现
 * @param curDate 当前日期，格式为YYYYMMDD
 * @param curTime 当前时间，格式为HHMMSS
 * @return bool 返回true表示已触发策略计算，false表示未触发
 * 
 * 定时调度时调用，用于触发策略的on_calculate回调函数，执行策略逻辑。
 * 执行流程：
 * 1. 设置调度标记，保存数据（主要用于保存浮动盈亏）
 * 2. 检查主K线是否闭合，如果闭合则触发策略计算
 * 3. 清理过期的条件单（在策略计算前）
 * 4. 调用on_calculate执行策略逻辑
 * 5. 统计性能数据（计算次数、耗时等）
 * 6. 保存用户数据和条件单（如果已修改）
 * 
 * 只有在主K线闭合时才会触发策略计算，确保策略在主K线更新后才执行。
 */
bool CtaStraBaseCtx::on_schedule(uint32_t curDate, uint32_t curTime)
{
	_is_in_schedule = true;//开始调度, 修改标记

	//主要用于保存浮动盈亏的
	save_data();  // 保存策略数据（主要用于保存浮动盈亏）

	bool isMainUdt = false;  // 主K线是否更新
	bool emmited = false;     // 是否已触发策略计算

	// 遍历所有K线标记，检查主K线是否闭合
	for (auto it = _kline_tags.begin(); it != _kline_tags.end(); it++)
	{
		const char* key = it->first.c_str();        // 获取K线键值
		KlineTag& marker = (KlineTag&)it->second;   // 获取K线标记

		auto idx = StrUtil::findFirst(key, '#');    // 查找'#'的位置

		std::string stdCode(key, idx);               // 提取合约代码（从开始到'#'之前）

		if (key == _main_key)  // 如果是主K线
		{
			if (marker._closed)  // 如果主K线已闭合
			{
				isMainUdt = true;      // 标记主K线已更新
				marker._closed = false;  // 重置闭合标记
			}
			else  // 如果主K线未闭合
			{
				isMainUdt = false;  // 标记主K线未更新
				break;              // 跳出循环，不触发策略计算
			}
		}

		WTSSessionInfo* sInfo = _engine->get_session_info(stdCode.c_str(), true);  // 获取交易时段信息

		// 如果主K线已更新，或者没有K线标记（第一次调用）
		if (isMainUdt || _kline_tags.empty())
		{	
			TimeUtils::Ticker ticker;  // 创建计时器，用于统计性能

			uint32_t offTime = sInfo->offsetTime(curTime, true);  // 获取偏移时间
			if(offTime <= sInfo->getCloseTime(true))  // 如果当前时间在收盘时间之前
			{
				_condtions.clear();  // 清理所有条件单（主K线闭合后，条件单失效）
				on_calculate(curDate, curTime);  // 触发策略的on_calculate回调，执行策略逻辑
				log_debug("Strategy {} scheduled @ {}", _name, curTime);  // 记录调试日志
				emmited = true;  // 标记已触发策略计算

				_emit_times++;                        // 增加计算次数
				_total_calc_time += ticker.micro_seconds();  // 累加计算耗时（微秒）

				// 每20次计算输出一次性能统计
				if (_emit_times % 20 == 0)
				{
					log_info("Strategy has been scheduled {} times, totally taking {} us, {:.3f} us each time",
						_emit_times, _total_calc_time, _total_calc_time*1.0 / _emit_times);
				}

				// 如果用户数据已修改，保存用户数据
				if (_ud_modified)
				{
					save_userdata();   // 保存用户数据
					_ud_modified = false;  // 重置修改标记
				}

				// 如果策略计算后设置了新的条件单，保存条件单数据
				if(!_condtions.empty())
				{
					_last_cond_min = (uint64_t)curDate * 10000 + curTime;  // 更新条件单设置时间
					save_data();  // 保存策略数据（包括条件单）
				}
			}
			else  // 如果当前时间已超过收盘时间
			{
				log_info("{} not in trading time, schedule canceled", curTime);  // 记录日志：不在交易时段
			}
			break;  // 跳出循环
		}
	}

	_is_in_schedule = false;//调度结束, 修改标记
	_last_barno++;	//每次计算，barno加1（K线编号递增）
	return emmited;  // 返回是否已触发策略计算
}

/**
 * @brief 交易日开始回调实现
 * @param uTDate 交易日日期，格式为YYYYMMDD
 * 
 * 每个交易日开始时调用，用于处理交易日开始时的初始化工作。
 * 主要功能：
 * 1. 解冻持仓：如果冻结日期小于当前交易日，则解冻持仓
 * 2. 保存用户数据：如果用户数据已修改，则保存
 * 
 * 对于T+1品种，当日开仓需次日才能平仓，所以需要冻结持仓。
 * 到了下一个交易日，冻结的持仓会自动解冻。
 */
void CtaStraBaseCtx::on_session_begin(uint32_t uTDate)
{
	//每个交易日开始，要把冻结持仓置零
	for (auto& it : _pos_map)  // 遍历所有合约的持仓
	{
		const char* stdCode = it.first.c_str();   // 获取合约代码
		PosInfo& pInfo = (PosInfo&)it.second;     // 获取持仓信息
		// 如果冻结日期不为0，且冻结日期小于当前交易日，且冻结持仓不为0
		if(pInfo._frozen_date!=0 && pInfo._frozen_date < uTDate && !decimal::eq(pInfo._frozen, 0))
		{
			log_debug("{} of %s frozen on {} released on {}", pInfo._frozen, stdCode, pInfo._frozen_date, uTDate);  // 记录日志

			pInfo._frozen = 0;       // 解冻持仓
			pInfo._frozen_date = 0;   // 清零冻结日期
		}
	}

	// 如果用户数据已修改，保存用户数据
	if (_ud_modified)
	{
		save_userdata();   // 保存用户数据
		_ud_modified = false;  // 重置修改标记
	}
}

/**
 * @brief 枚举持仓实现
 * @param cb 回调函数，用于处理每个合约的持仓
 * @param bForExecute 是否用于执行，默认为false
 * 
 * 遍历所有持仓（包括信号持仓），对每个合约调用回调函数。
 * 使用自旋锁保护，避免多线程竞争导致的数据不一致。
 * 
 * 如果bForExecute为true，则标记信号为已触发，避免重复执行。
 * 
 * 注意：该函数会先收集所有持仓数据（加锁），然后在锁外调用回调函数，
 * 避免在回调函数中持有锁，造成死锁风险。
 */
void CtaStraBaseCtx::enum_position(FuncEnumCtaPosCallBack cb, bool bForExecute /* = false */)
{
	/* By HeJ @ 2023.03.14
	 * 读取理论持仓时，要加个锁，避免出现组合轧差同步与信号同时触发，导致的反复发单和信号覆盖
	 */
	std::unordered_map<std::string, double> desPos;  // 目标持仓映射表
	{
		SpinLock lock(_mutex);  // 加锁保护，避免并发访问
		// 遍历所有持仓，收集持仓数据
		for (auto& it : _pos_map)
		{
			const char* stdCode = it.first.c_str();   // 获取合约代码
			const PosInfo& pInfo = it.second;          // 获取持仓信息
			//cb(stdCode, pInfo._volume);
			desPos[stdCode] = pInfo._volume;  // 将持仓数量添加到目标持仓映射表中
		}

		// 遍历所有信号，将信号持仓也添加到目标持仓映射表中
		for (auto& sit : _sig_map)
		{
			const char* stdCode = sit.first.c_str();   // 获取合约代码
			SigInfo& sInfo = (SigInfo&)sit.second;     // 获取信号信息
			desPos[stdCode] = sInfo._volume;          // 将信号目标仓位添加到目标持仓映射表中（会覆盖实际持仓）
			if (bForExecute)  // 如果用于执行
				sInfo._triggered = true;              // 标记信号为已触发
		}
	}  // 锁在这里释放

	// 在锁外调用回调函数，避免死锁
	for(auto v:desPos)  // 遍历目标持仓映射表
	{
		cb(v.first.c_str(), v.second);  // 调用回调函数，传递合约代码和持仓数量
	}
}

/**
 * @brief 交易日结束回调实现
 * @param uTDate 交易日日期，格式为YYYYMMDD
 * 
 * 每个交易日结束时调用，用于保存数据、记录结算信息等收尾工作。
 * 主要功能：
 * 1. 记录每日持仓信息到持仓日志文件
 * 2. 记录每日资金信息到资金日志文件
 * 3. 保存策略数据（持仓、资金、条件单、信号等）
 * 4. 保存用户数据（如果已修改）
 */
void CtaStraBaseCtx::on_session_end(uint32_t uTDate)
{
	uint32_t curDate = uTDate;//_engine->get_trading_date();  // 当前交易日

	double total_profit = 0;      // 累计平仓盈亏
	double total_dynprofit = 0;   // 累计浮动盈亏

	// 遍历所有合约的持仓，记录持仓日志并累加盈亏
	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)
	{
		const char* stdCode = it->first.c_str();   // 获取合约代码
		const PosInfo& pInfo = it->second;          // 获取持仓信息
		total_profit += pInfo._closeprofit;        // 累加平仓盈亏
		total_dynprofit += pInfo._dynprofit;       // 累加浮动盈亏

		if (decimal::eq(pInfo._volume, 0.0))  // 如果持仓数量为0
			continue;                          // 跳过，不记录日志

		// 记录持仓日志：日期,合约代码,持仓数量,平仓盈亏,浮动盈亏
		if(_pos_logs)  // 如果持仓日志文件已初始化
			_pos_logs->write_file(fmt::format("{},{},{},{:.2f},{:.2f}\n", curDate, stdCode,
				pInfo._volume, pInfo._closeprofit, pInfo._dynprofit));
	}

	//这里要把当日结算的数据写到日志文件里
	//而且这里回测和实盘写法不同, 先留着, 后面来做
	// 记录资金日志：日期,平仓盈亏,持仓盈亏,动态余额,手续费
	if (_fund_logs)  // 如果资金日志文件已初始化
		_fund_logs->write_file(fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curDate, 
		_fund_info._total_profit, _fund_info._total_dynprofit, 
		_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees));

	save_data();  // 保存策略数据（持仓、资金、条件单、信号等）

	// 如果用户数据已修改，保存用户数据
	if (_ud_modified)
	{
		save_userdata();   // 保存用户数据
		_ud_modified = false;  // 重置修改标记
	}
}

/**
 * @brief 获取指定合约的条件单列表实现
 * @param stdCode 合约代码
 * @return CondList& 返回条件单列表的引用
 * 
 * 获取指定合约的条件单列表，如果不存在则创建空列表。
 * 用于添加、查询、删除条件单。
 */
CondList& CtaStraBaseCtx::get_cond_entrusts(const char* stdCode)
{
	CondList& ce = _condtions[stdCode];  // 获取或创建该合约的条件单列表
	return ce;                            // 返回条件单列表的引用
}

//////////////////////////////////////////////////////////////////////////
//策略接口实现
//////////////////////////////////////////////////////////////////////////

/**
 * @brief 开多仓实现
 * @param stdCode 合约代码
 * @param qty 开仓数量
 * @param userTag 用户标签，默认为空字符串
 * @param limitprice 限价，默认为0.0（市价单）
 * @param stopprice 止损价，默认为0.0（无止损）
 * 
 * 执行开多仓操作，如果当前有空仓，则先平空再开多。
 * 如果设置了限价或止损价，则创建条件单；否则直接创建交易信号。
 */
void CtaStraBaseCtx::stra_enter_long(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取品种信息
	if (commInfo == NULL)  // 如果品种信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;  // 直接返回
	}

	_engine->sub_tick(id(), stdCode);  // 订阅该合约的Tick数据
	
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式, 则直接触发
	{
		double curQty = stra_get_position(stdCode);  // 获取当前持仓
		if (decimal::lt(curQty, 0))  // 如果当前有空仓
		{
			//当前持仓小于0,逻辑是反手到qty,所以设置信号目标仓位为qty
			append_signal(stdCode, qty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号（先平空再开多）
		}
		else  // 如果当前没有空仓
		{
			//当前持仓大于等于0,则要增加多仓qty
			append_signal(stdCode, curQty + qty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号（增加多仓）
		}
	}
	else  // 如果设置了限价或止损价，创建条件单
	{
		CondList& condList = get_cond_entrusts(stdCode);  // 获取条件单列表

		CondEntrust entrust;  // 创建条件单委托对象
		wt_strcpy(entrust._code, stdCode);     // 设置合约代码
		wt_strcpy(entrust._usertag, userTag);  // 设置用户标签

		entrust._qty = qty;                    // 设置数量
		entrust._field = WCF_NEWPRICE;         // 设置比较字段为最新价
		// 如果设置了限价
		if(!decimal::eq(limitprice))
		{
			entrust._target = limitprice;              // 设置目标价格为限价
			entrust._alg = WCT_SmallerOrEqual;        // 设置比较算法为小于等于（买入限价单）
		}
		// 如果设置了止损价（且未设置限价）
		else if (!decimal::eq(stopprice))
		{
			entrust._target = stopprice;               // 设置目标价格为止损价
			entrust._alg = WCT_LargerOrEqual;          // 设置比较算法为大于等于（买入止损单）
		}
		
		entrust._action = COND_ACTION_OL;  // 设置动作为开多

		condList.emplace_back(entrust);  // 将条件单添加到条件单列表中
	}
}

/**
 * @brief 开空仓实现
 * @param stdCode 合约代码
 * @param qty 开仓数量
 * @param userTag 用户标签，默认为空字符串
 * @param limitprice 限价，默认为0.0（市价单）
 * @param stopprice 止损价，默认为0.0（无止损）
 * 
 * 执行开空仓操作，如果当前有多仓，则先平多再开空。
 * 如果设置了限价或止损价，则创建条件单；否则直接创建交易信号。
 * 会检查品种是否支持做空。
 */
void CtaStraBaseCtx::stra_enter_short(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取品种信息
	if (commInfo == NULL)  // 如果品种信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;  // 直接返回
	}

	if (!commInfo->canShort())  // 如果品种不支持做空
	{
		log_error("Cannot short on {}", stdCode);  // 记录错误日志
		return;  // 直接返回
	}

	_engine->sub_tick(id(), stdCode);  // 订阅该合约的Tick数据
	
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式, 则直接触发
	{
		double curQty = stra_get_position(stdCode);  // 获取当前持仓
		if (decimal::gt(curQty, 0))  // 如果当前有多仓
		{
			//当前仓位大于0,逻辑是反手到qty手,所以设置信号目标仓位为-qty手
			append_signal(stdCode, -qty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号（先平多再开空）
		}
		else  // 如果当前没有多仓
		{
			//当前仓位小于等于0,则是追加空方手数
			append_signal(stdCode, curQty - qty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号（增加空仓）
		}
	}
	else  // 如果设置了限价或止损价，创建条件单
	{
		CondList& condList = get_cond_entrusts(stdCode);  // 获取条件单列表

		CondEntrust entrust;  // 创建条件单委托对象
		wt_strcpy(entrust._code, stdCode);     // 设置合约代码
		wt_strcpy(entrust._usertag, userTag);  // 设置用户标签

		entrust._qty = qty;                    // 设置数量
		entrust._field = WCF_NEWPRICE;         // 设置比较字段为最新价
		// 如果设置了限价
		if (!decimal::eq(limitprice))
		{
			entrust._target = limitprice;              // 设置目标价格为限价
			entrust._alg = WCT_LargerOrEqual;          // 设置比较算法为大于等于（卖出限价单）
		}
		// 如果设置了止损价（且未设置限价）
		else if (!decimal::eq(stopprice))
		{
			entrust._target = stopprice;               // 设置目标价格为止损价
			entrust._alg = WCT_SmallerOrEqual;         // 设置比较算法为小于等于（卖出止损单）
		}

		entrust._action = COND_ACTION_OS;  // 设置动作为开空

		condList.emplace_back(entrust);  // 将条件单添加到条件单列表中
	}
}

/**
 * @brief 平多仓实现
 * @param stdCode 合约代码
 * @param qty 平仓数量
 * @param userTag 用户标签，默认为空字符串
 * @param limitprice 限价，默认为0.0（市价单）
 * @param stopprice 止损价，默认为0.0（无止损）
 * 
 * 执行平多仓操作，平仓数量不能超过当前多仓数量。
 * 对于T+1品种，会考虑冻结持仓，只有在收盘时才能平全部持仓。
 * 如果设置了限价或止损价，则创建条件单；否则直接创建交易信号。
 */
void CtaStraBaseCtx::stra_exit_long(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取品种信息
	if (commInfo == NULL)  // 如果品种信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;  // 直接返回
	}

	WTSSessionInfo* sInfo = commInfo->getSessionInfo();  // 获取交易时段信息
	uint32_t offTime = sInfo->offsetTime(_engine->get_min_time(), true);  // 获取偏移时间
	bool isLastBarOfDay = (offTime == sInfo->getCloseTime(true));  // 判断是否是收盘前的最后一根K线

	//读取可平持仓,如果是收盘那根bar，则直接读取全部持仓
	double curQty = stra_get_position(stdCode, !isLastBarOfDay);  // 获取可平持仓（收盘时读取全部持仓）
	if (decimal::le(curQty, 0))  // 如果可平持仓小于等于0
		return;                   // 直接返回
	
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式, 则直接触发
	{
		double maxQty = min(curQty, qty);  // 平仓数量不能超过可平持仓
		double totalQty = stra_get_position(stdCode, false);  // 获取总持仓
		append_signal(stdCode, totalQty - maxQty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号
	}
	else  // 如果设置了限价或止损价，创建条件单
	{
		CondList& condList = get_cond_entrusts(stdCode);  // 获取条件单列表

		CondEntrust entrust;  // 创建条件单委托对象
		wt_strcpy(entrust._code, stdCode);     // 设置合约代码
		wt_strcpy(entrust._usertag, userTag);  // 设置用户标签

		entrust._qty = qty;                    // 设置数量
		entrust._field = WCF_NEWPRICE;         // 设置比较字段为最新价
		// 如果设置了限价
		if (!decimal::eq(limitprice))
		{
			entrust._target = limitprice;              // 设置目标价格为限价
			entrust._alg = WCT_LargerOrEqual;          // 设置比较算法为大于等于（卖出限价单）
		}
		// 如果设置了止损价（且未设置限价）
		else if (!decimal::eq(stopprice))
		{
			entrust._target = stopprice;               // 设置目标价格为止损价
			entrust._alg = WCT_SmallerOrEqual;         // 设置比较算法为小于等于（卖出止损单）
		}

		entrust._action = COND_ACTION_CL;  // 设置动作为平多

		condList.emplace_back(entrust);  // 将条件单添加到条件单列表中
	}
}

/**
 * @brief 平空仓实现
 * @param stdCode 合约代码
 * @param qty 平仓数量
 * @param userTag 用户标签，默认为空字符串
 * @param limitprice 限价，默认为0.0（市价单）
 * @param stopprice 止损价，默认为0.0（无止损）
 * 
 * 执行平空仓操作，平仓数量不能超过当前空仓数量。
 * 如果当前持仓是多仓，则不需要执行退出空头的逻辑。
 * 如果设置了限价或止损价，则创建条件单；否则直接创建交易信号。
 */
void CtaStraBaseCtx::stra_exit_short(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice, double stopprice)
{
	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取品种信息
	if (commInfo == NULL)  // 如果品种信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志
		return;  // 直接返回
	}

	if (!commInfo->canShort())  // 如果品种不支持做空
	{
		log_error("Cannot short on {}", stdCode);  // 记录错误日志
		return;  // 直接返回
	}

	double curQty = stra_get_position(stdCode);  // 获取当前持仓
	//如果持仓是多,则不需要执行退出空头的逻辑了
	if (decimal::ge(curQty, 0))  // 如果当前持仓大于等于0（即多仓或空仓）
		return;                   // 直接返回
	
	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式, 则直接触发
	{
		double maxQty = min(abs(curQty), qty);  // 平仓数量不能超过当前空仓的绝对值
		append_signal(stdCode, curQty + maxQty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号
	}
	else  // 如果设置了限价或止损价，创建条件单
	{
		CondList& condList = get_cond_entrusts(stdCode);  // 获取条件单列表

		CondEntrust entrust;  // 创建条件单委托对象
		wt_strcpy(entrust._code, stdCode);     // 设置合约代码
		wt_strcpy(entrust._usertag, userTag);  // 设置用户标签

		entrust._qty = qty;                    // 设置数量
		entrust._field = WCF_NEWPRICE;         // 设置比较字段为最新价
		// 如果设置了限价
		if (!decimal::eq(limitprice))
		{
			entrust._target = limitprice;              // 设置目标价格为限价
			entrust._alg = WCT_SmallerOrEqual;        // 设置比较算法为小于等于（买入限价单）
		}
		// 如果设置了止损价（且未设置限价）
		else if (!decimal::eq(stopprice))
		{
			entrust._target = stopprice;               // 设置目标价格为止损价
			entrust._alg = WCT_LargerOrEqual;          // 设置比较算法为大于等于（买入止损单）
		}

		entrust._action = COND_ACTION_CS;  // 设置动作为平空
		
		condList.emplace_back(entrust);  // 将条件单添加到条件单列表中
	}
}

/**
 * @brief 获取当前价格实现
 * @param stdCode 合约代码
 * @return double 返回当前价格
 * 
 * 获取指定合约的当前价格，优先使用本地缓存的价格，如果没有则从引擎获取。
 * 如果引擎也不存在，则返回0.0。
 */
double CtaStraBaseCtx::stra_get_price(const char* stdCode)
{
	auto it = _price_map.find(stdCode);  // 在价格映射表中查找
	if (it != _price_map.end())  // 如果找到缓存的价格
		return it->second;        // 返回缓存的价格

	if (_engine)  // 如果引擎存在
		return _engine->get_cur_price(stdCode);  // 从引擎获取当前价格
	
	return 0.0;  // 如果都不存在，返回0.0
}

/**
 * @brief 读取当日价格实现
 * @param stdCode 合约代码
 * @param flag 价格标记：0-开盘价，1-最高价，2-最低价，3-收盘价/最新价
 * @return double 返回对应的价格
 * 
 * 读取指定合约的当日价格，支持开盘价、最高价、最低价、收盘价等。
 * 如果引擎不存在，则返回0.0。
 */
double CtaStraBaseCtx::stra_get_day_price(const char* stdCode, int flag /* = 0 */)
{
	if (_engine)  // 如果引擎存在
		return _engine->get_day_price(stdCode, flag);  // 从引擎获取当日价格

	return 0.0;  // 如果引擎不存在，返回0.0
}

/**
 * @brief 设置目标仓位实现
 * @param stdCode 合约代码
 * @param qty 目标仓位数量，正数表示多头，负数表示空头
 * @param userTag 用户标签，默认为空字符串
 * @param limitprice 限价，默认为0.0（市价单）
 * @param stopprice 止损价，默认为0.0（无止损）
 * 
 * 设置指定合约的目标仓位，系统会自动调整持仓以达到目标仓位。
 * 如果目标仓位和当前仓位一致，则不设置条件单。
 * 如果设置了限价或止损价，则创建条件单；否则直接创建交易信号。
 */
void CtaStraBaseCtx::stra_set_position(const char* stdCode, double qty, const char* userTag /* = "" */, double limitprice /* = 0.0 */, double stopprice /* = 0.0 */)
{
	_engine->sub_tick(id(), stdCode);  // 订阅该合约的Tick数据

	if (decimal::eq(limitprice, 0.0) && decimal::eq(stopprice, 0.0))	//如果不是动态下单模式, 则直接触发
	{
		append_signal(stdCode, qty, userTag, _is_in_schedule ? 0 : 1);  // 添加交易信号，直接设置目标仓位
	}
	else  // 如果设置了限价或止损价，创建条件单
	{
		CondList& condList = get_cond_entrusts(stdCode);  // 获取条件单列表

		double curVol = stra_get_position(stdCode);  // 获取当前持仓
		//如果目标仓位和当前仓位是一致的，则不再设置条件单
		if (decimal::eq(curVol, qty))  // 如果目标仓位和当前仓位一致
			return;                     // 直接返回，不设置条件单

		//根据目标仓位和当前仓位,判断是买还是卖
		bool isBuy = decimal::gt(qty, curVol);  // 如果目标仓位大于当前仓位，则为买入

		CondEntrust entrust;  // 创建条件单委托对象
		wt_strcpy(entrust._code, stdCode);     // 设置合约代码
		wt_strcpy(entrust._usertag, userTag);  // 设置用户标签

		entrust._qty = qty;                    // 设置数量
		entrust._field = WCF_NEWPRICE;         // 设置比较字段为最新价
		// 如果设置了限价
		if (!decimal::eq(limitprice))
		{
			entrust._target = limitprice;                    // 设置目标价格为限价
			entrust._alg = isBuy ? WCT_SmallerOrEqual : WCT_LargerOrEqual;  // 买入限价单用小于等于，卖出限价单用大于等于
		}
		// 如果设置了止损价（且未设置限价）
		else if (!decimal::eq(stopprice))
		{
			entrust._target = stopprice;                    // 设置目标价格为止损价
			entrust._alg = isBuy ? WCT_LargerOrEqual : WCT_SmallerOrEqual;  // 买入止损单用大于等于，卖出止损单用小于等于
		}

		entrust._action = COND_ACTION_SP;  // 设置动作为直接设置仓位

		condList.emplace_back(entrust);  // 将条件单添加到条件单列表中
	}
}

/**
 * @brief 追加交易信号实现
 * @param stdCode 合约代码
 * @param qty 目标仓位数量
 * @param userTag 用户标签，默认为空字符串
 * @param sigType 信号类型，0-调度信号，1-Tick信号，2-条件单信号，默认为0
 * 
 * 添加一个交易信号到信号映射表中，等待后续处理。
 * 信号会在下次Tick更新时被处理并执行。
 * 同时记录信号日志并保存策略数据。
 */
void CtaStraBaseCtx::append_signal(const char* stdCode, double qty, const char* userTag /* = "" */, uint32_t sigType)
{
	double curPx = _price_map[stdCode];  // 获取当前价格（从价格映射表）

	SigInfo& sInfo = _sig_map[stdCode];  // 获取或创建该合约的信号信息
	sInfo._volume = qty;                 // 设置目标仓位
	sInfo._sigprice = curPx;              // 设置信号价格
	sInfo._usertag = userTag;            // 设置用户标签
	// 生成信号时间戳：日期 * 1000000000 + 时间 * 100000 + 秒数
	sInfo._gentime = (uint64_t)_engine->get_date() * 1000000000 + (uint64_t)_engine->get_raw_time() * 100000 + _engine->get_secs();
	sInfo._sigtype = sigType;            // 设置信号类型

	log_signal(stdCode, qty, curPx, sInfo._gentime, userTag);  // 记录信号日志

	save_data();  // 保存策略数据（包括信号）
}

/**
 * @brief 设置持仓的核心实现
 * @param stdCode 合约代码
 * @param qty 目标仓位数量，正数表示多头，负数表示空头
 * @param userTag 用户标签，默认为空字符串
 * @param bFireAtOnce 是否立即触发，默认为false（条件单触发时为true）
 * 
 * 这是设置持仓的核心函数，负责更新持仓明细、计算盈亏、记录交易日志等。
 * 主要逻辑：
 * 1. 如果目标仓位和当前仓位一致，直接返回
 * 2. 如果持仓方向和仓位变化方向一致（都是多或都是空），则增加持仓明细
 * 3. 如果持仓方向和仓位变化方向不一致，则需要先平仓，再反手（如果需要）
 * 4. 计算平仓盈亏、手续费、浮动盈亏等
 * 5. 记录交易日志和平仓日志
 * 6. 保存数据，如果bFireAtOnce为true，则通知引擎持仓变化
 * 
 * 使用自旋锁保护，避免并发访问导致的数据不一致。
 */
void CtaStraBaseCtx::do_set_position(const char* stdCode, double qty, const char* userTag /* = "" */, bool bFireAtOnce /* = false */)
{
	PosInfo& pInfo = _pos_map[stdCode];  // 获取或创建该合约的持仓信息
	double curPx = _price_map[stdCode];   // 获取当前价格
	// 构建当前时间戳：日期 * 10000 + 分钟时间
	uint64_t curTm = (uint64_t)_engine->get_date() * 10000 + _engine->get_min_time();
	uint32_t curTDate = _engine->get_trading_date();  // 获取交易日

	if (decimal::eq(pInfo._volume, qty))  // 如果目标仓位和当前仓位一致
		return;                             // 直接返回，无需调整

	double diff = qty - pInfo._volume;  // 计算仓位变化量

	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取品种信息
	if (commInfo == NULL)  // 如果品种信息不存在
		return;             // 直接返回

	//成交价
	double trdPx = curPx;  // 成交价初始化为当前价格
	/* By HeJ @ 2023.03.14
	 * 设置理论持仓时，要加个锁，避免出现组合轧差同步与信号同时触发，导致的反复发单和信号覆盖
	 */
	SpinLock lock(_mutex);  // 加锁保护，避免并发访问
	bool isBuy = decimal::gt(diff, 0.0);  // 判断是买入还是卖出（diff>0为买入）
	// 如果当前持仓和仓位变化方向一致（都是多或都是空），则增加持仓明细
	if (decimal::gt(pInfo._volume*diff, 0))
	{//当前持仓和仓位变化方向一致, 增加一条明细, 增加数量即可
		pInfo._volume = qty;  // 更新持仓数量

		//如果T+1，则冻结仓位要增加
		if (commInfo->isT1())  // 如果是T+1品种
		{
			//ASSERT(diff>0);
			pInfo._frozen += diff;        // 增加冻结持仓
			pInfo._frozen_date = curTDate;  // 设置冻结日期
			log_debug("{} frozen position updated to {}", stdCode, pInfo._frozen);  // 记录调试日志
		}

		// 如果设置了滑点，调整成交价
		if (_slippage != 0)
		{
			// 买入时加滑点，卖出时减滑点
			trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);
		}

		// 创建新的持仓明细
		DetailInfo dInfo;
		dInfo._long = decimal::gt(qty, 0);  // 判断是否多头
		dInfo._price = trdPx;                 // 开仓价格
		dInfo._max_price = trdPx;            // 最高价格（初始为开仓价）
		dInfo._min_price = trdPx;            // 最低价格（初始为开仓价）
		dInfo._volume = abs(diff);           // 持仓数量
		dInfo._opentime = curTm;             // 开仓时间
		dInfo._opentdate = curTDate;         // 开仓交易日
		dInfo._open_barno = _last_barno;     // 开仓K线编号
		wt_strcpy(dInfo._opentag, userTag);  // 开仓标签
		pInfo._details.emplace_back(dInfo);   // 将明细添加到持仓明细列表
		pInfo._last_entertime = curTm;        // 更新最后开仓时间

		//double fee = _engine->calc_fee(stdCode, trdPx, abs(diff), 0);
		double fee = commInfo->calcFee(trdPx, abs(diff), 0);  // 计算手续费（开仓）
		_fund_info._total_fees += fee;                         // 累加手续费
		log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(diff), userTag, fee, _last_barno);  // 记录交易日志
	}
	else  // 如果持仓方向和仓位变化方向不一致，需要先平仓
	{//持仓方向和仓位变化方向不一致, 需要平仓
		double left = abs(diff);  // 需要平仓的数量（绝对值）

		// 如果设置了滑点，调整成交价
		if (_slippage != 0)
			trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 买入时加滑点，卖出时减滑点

		pInfo._volume = qty;  // 更新持仓数量
		if (decimal::eq(pInfo._volume, 0))  // 如果持仓数量为0
			pInfo._dynprofit = 0;            // 浮动盈亏清零
		uint32_t count = 0;  // 已平仓完的明细数量
		// 遍历持仓明细，按FIFO顺序平仓
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
		{
			DetailInfo& dInfo = *it;  // 获取持仓明细引用
			if (decimal::eq(dInfo._volume, 0))  // 如果该明细已平完
			{
				count++;   // 增加已平仓完的明细计数
				continue;  // 跳过
			}

			double maxQty = min(dInfo._volume, left);  // 本次平仓数量（不能超过明细持仓和剩余需平数量）
			if (decimal::eq(maxQty, 0))  // 如果平仓数量为0
				continue;                 // 跳过

			dInfo._volume -= maxQty;  // 减少明细持仓数量
			left -= maxQty;            // 减少剩余需平数量

			if (decimal::eq(dInfo._volume, 0))  // 如果该明细已平完
				count++;                        // 增加已平仓完的明细计数

			//计算平仓盈亏
			// 平仓盈亏 = (平仓价 - 开仓价) * 平仓数量 * 合约乘数
			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();
			if (!dInfo._long)  // 如果是空头
				profit *= -1;   // 盈亏方向相反
			pInfo._closeprofit += profit;  // 累加该合约的平仓盈亏

			//浮盈也要做等比缩放
			// 浮动盈亏按比例缩放：剩余持仓的浮动盈亏 = 原浮动盈亏 * (剩余持仓 / 原持仓)
			pInfo._dynprofit = pInfo._dynprofit*dInfo._volume / (dInfo._volume + maxQty);
			pInfo._last_exittime = curTm;      // 更新最后平仓时间
			_fund_info._total_profit += profit;  // 累加策略的总平仓盈亏

			//计算手续费
			//double fee = _engine->calc_fee(stdCode, trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);
			// 如果是当日开仓当日平仓，手续费类型为2（平今），否则为1（平昨）
			double fee = commInfo->calcFee(trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);
			_fund_info._total_fees += fee;  // 累加手续费

			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, pInfo._closeprofit, dInfo._opentag, userTag, dInfo._open_barno, _last_barno);

			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, trdPx, maxQty, userTag, fee, _last_barno);

			if (decimal::eq(left,0))  // 如果剩余需平数量为0
				break;                 // 跳出循环
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)  // 清理已平仓完的明细
		{
			auto it = pInfo._details.begin();  // 获取第一个明细的迭代器
			pInfo._details.erase(it);          // 删除该明细
			count--;                           // 减少计数
		}

		//最后, 如果还有剩余的, 则需要反手了
		if (decimal::gt(left, 0))  // 如果还有剩余需平数量（说明需要反手）
		{
			left = left * qty / abs(qty);  // 将剩余数量转换为目标方向（正数或负数）

			//如果T+1，则冻结仓位要增加
			if (commInfo->isT1())  // 如果是T+1品种
			{
				pInfo._frozen += left;        // 增加冻结持仓
				pInfo._frozen_date = curTDate;  // 设置冻结日期
				log_debug("{} frozen position up to {}", stdCode, pInfo._frozen);  // 记录调试日志
			}

			// 创建反手持仓明细
			DetailInfo dInfo;
			dInfo._long = decimal::gt(qty, 0);  // 判断是否多头
			dInfo._price = trdPx;                // 开仓价格
			dInfo._max_price = trdPx;           // 最高价格（初始为开仓价）
			dInfo._min_price = trdPx;           // 最低价格（初始为开仓价）
			dInfo._volume = abs(left);          // 持仓数量
			dInfo._opentime = curTm;            // 开仓时间
			dInfo._opentdate = curTDate;        // 开仓交易日
			dInfo._open_barno = _last_barno;   // 开仓K线编号
			wt_strcpy(dInfo._opentag, userTag); // 开仓标签
			pInfo._details.emplace_back(dInfo);  // 将明细添加到持仓明细列表
			pInfo._last_entertime = curTm;       // 更新最后开仓时间

			//这里还需要写一笔成交记录
			double fee = commInfo->calcFee(trdPx, abs(left), 0);  // 计算手续费（开仓）
			_fund_info._total_fees += fee;                        // 累加手续费
			log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(left), userTag, fee, _last_barno);  // 记录交易日志
		}
	}


	//存储数据
	save_data();  // 保存策略数据

	if (bFireAtOnce)	//如果是条件单触发, 则向引擎提交变化量
	{
		_engine->handle_pos_change(_name.c_str(), stdCode, diff);  // 通知引擎持仓变化
	}
}

/**
 * @brief 获取K线数据实现
 * @param stdCode 合约代码
 * @param period K线周期，如"m5"、"d1"等
 * @param count 获取的K线数量
 * @param isMain 是否为主K线，默认为false
 * @return WTSKlineSlice* 返回K线数据切片指针，失败返回NULL
 * 
 * 获取指定合约的K线数据，如果指定为主K线，则设置主K线相关配置。
 * 主K线用于触发策略计算，确保策略在主K线闭合后才执行。
 * 第一次拉取主K线时，会检查条件单是否过期。
 */
WTSKlineSlice* CtaStraBaseCtx::stra_get_bars(const char* stdCode, const char* period, uint32_t count, bool isMain /* = false */)
{
	// 构建K线键值（格式：合约代码#周期）
	thread_local static char key[64] = { 0 };  // 线程局部静态变量，用于存储K线键值
	fmtutil::format_to(key, "{}#{}", stdCode, period);  // 格式化：合约代码 + "#" + 周期

	if (isMain)  // 如果指定为主K线
	{
		if (_main_key.empty())  // 如果主K线键值还未设置
		{
			_main_key = key;  // 设置主K线键值
			log_debug("Main KBars confirmed: {}", key);  // 记录调试日志
		}
		else if (_main_key != key)  // 如果主K线键值已设置，但与当前键值不一致
		{
			log_error("Main KBars already confirmed");  // 记录错误日志
			return NULL;  // 返回NULL，不允许修改主K线
		}

		/*
		 *	By Wesley @ 2022.12.07
		 */
		_main_code = stdCode;    // 设置主K线合约代码
		_main_period = period;    // 设置主K线周期
	}

	// 解析周期字符串，提取基础周期和倍数
	thread_local static char basePeriod[2] = { 0 };  // 线程局部静态变量，用于存储基础周期
	basePeriod[0] = period[0];  // 提取基础周期（如'm'、'd'等）
	uint32_t times = 1;         // 周期倍数，默认为1
	if (strlen(period) > 1)    // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);  // 解析倍数（如"m5"中的5）

	// 从引擎获取K线数据
	WTSKlineSlice* kline = _engine->get_kline_slice(_context_id, stdCode, basePeriod, count, times);
	if(kline)  // 如果成功获取K线数据
	{
		//如果K线获取不到,说明也不会有闭合事件发生,所以不更新本地标记
		bool isFirst = (_kline_tags.find(key) == _kline_tags.end());	//如果没有保存标记,说明是第一次拉取该K线
		KlineTag& tag = _kline_tags[key];  // 获取或创建该K线的标记
		tag._closed = false;                // 标记K线未闭合

		// 更新价格缓存（使用最后一根K线的收盘价）
		double lastClose = kline->at(-1)->close;  // 获取最后一根K线的收盘价
		_price_map[stdCode] = lastClose;           // 更新价格映射表

		// 如果是第一次拉取主K线，且存在条件单，则检查条件单是否过期
		if(isMain && isFirst && !_condtions.empty())
		{
			//如果是第一次拉取主K线,则检查条件单触发时间
			bool isDay = basePeriod[0] == 'd';  // 判断是否是日线
			uint64_t lastBartime = isDay ? kline->at(-1)->date : kline->at(-1)->time;  // 获取最后一根K线的时间
			if(!isDay)  // 如果不是日线
				lastBartime += 199000000000;  // 加上日期前缀（转换为完整时间戳）

			//如果最后一条已闭合的K线的时间大于条件单设置时间，说明条件单已经过期了，则需要清理
			if(lastBartime > _last_cond_min)  // 如果最后K线时间大于条件单设置时间
			{
				log_info("Conditions expired, setup time: {}, time of last bar of main kbars: {}, all cleared", _last_cond_min, lastBartime);  // 记录日志
				_condtions.clear();  // 清理所有条件单
			}
		}

		_engine->sub_tick(id(), stdCode);  // 订阅该合约的Tick数据

		//如果是主K线，并且最后一根bar的编号为0
		//则将最后一根bar的编号设置为主K线的长度
		if(isMain && _last_barno == 0)  // 如果是主K线且K线编号为0
		{
			_last_barno = kline->size();  // 设置K线编号为K线数量
		}
	}

	return kline;  // 返回K线数据切片指针
}

/**
 * @brief 获取Tick数据实现
 * @param stdCode 合约代码
 * @param count 获取的Tick数量
 * @return WTSTickSlice* 返回Tick数据切片指针，失败返回NULL
 * 
 * 获取指定合约的Tick数据，成功获取后会自动订阅该合约的Tick数据。
 */
WTSTickSlice* CtaStraBaseCtx::stra_get_ticks(const char* stdCode, uint32_t count)
{
	WTSTickSlice* ret = _engine->get_tick_slice(_context_id, stdCode, count);  // 从引擎获取Tick数据
	if (ret)  // 如果成功获取
		_engine->sub_tick(id(), stdCode);  // 订阅该合约的Tick数据

	return ret;  // 返回Tick数据切片指针
}

/**
 * @brief 获取最新Tick数据实现
 * @param stdCode 合约代码
 * @return WTSTickData* 返回最新Tick数据指针，失败返回NULL
 * 
 * 获取指定合约的最新一条Tick数据。
 */
WTSTickData* CtaStraBaseCtx::stra_get_last_tick(const char* stdCode)
{
	return _engine->get_last_tick(_context_id, stdCode);  // 从引擎获取最新Tick数据
}

/**
 * @brief 订阅Tick数据实现
 * @param code 合约代码
 * 
 * 主动订阅指定合约的Tick数据，会在本地记录订阅信息。
 * 订阅后，该合约的Tick数据更新时会触发on_tick回调。
 */
void CtaStraBaseCtx::stra_sub_ticks(const char* code)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(code);  // 将合约代码添加到订阅集合中

	_engine->sub_tick(_context_id, code);  // 向引擎订阅Tick数据
	log_info("Market data subscribed: {}", code);  // 记录信息日志
}

/**
 * @brief 订阅K线闭合事件实现
 * @param stdCode 合约代码
 * @param period K线周期，如"m5"、"d1"等
 * 
 * 订阅指定合约和周期的K线闭合事件。
 * 订阅后，当该K线闭合时会触发on_bar_close回调。
 */
void CtaStraBaseCtx::stra_sub_bar_events(const char* stdCode, const char* period)
{
	// 构建K线键值（格式：合约代码#周期）
	thread_local static char key[64] = { 0 };  // 线程局部静态变量，用于存储K线键值
	fmtutil::format_to(key, "{}#{}", stdCode, period);  // 格式化：合约代码 + "#" + 周期

	KlineTag& tag = _kline_tags[key];  // 获取或创建该K线的标记
	tag._notify = true;                 // 设置通知标志，表示订阅了该K线的闭合事件
}

/**
 * @brief 获取品种信息实现
 * @param stdCode 合约代码
 * @return WTSCommodityInfo* 返回品种信息指针，失败返回NULL
 * 
 * 获取指定合约的品种信息，包括合约乘数、最小变动价位、手续费等信息。
 */
WTSCommodityInfo* CtaStraBaseCtx::stra_get_comminfo(const char* stdCode)
{
	return _engine->get_commodity_info(stdCode);  // 从引擎获取品种信息
}

/**
 * @brief 获取原始合约代码实现
 * @param stdCode 标准合约代码
 * @return std::string 返回原始合约代码
 * 
 * 将标准合约代码转换为原始合约代码（交易所格式）。
 */
std::string CtaStraBaseCtx::stra_get_rawcode(const char* stdCode)
{
	return _engine->get_rawcode(stdCode);  // 从引擎获取原始合约代码
}

/**
 * @brief 获取交易日实现
 * @return uint32_t 返回交易日，格式为YYYYMMDD
 * 
 * 获取当前交易日。
 */
uint32_t CtaStraBaseCtx::stra_get_tdate()
{
	return _engine->get_trading_date();  // 从引擎获取交易日
}

/**
 * @brief 获取当前日期实现
 * @return uint32_t 返回当前日期，格式为YYYYMMDD
 * 
 * 获取当前日期（可能是自然日，不是交易日）。
 */
uint32_t CtaStraBaseCtx::stra_get_date()
{
	return _engine->get_date();  // 从引擎获取当前日期
}

/**
 * @brief 获取当前时间实现
 * @return uint32_t 返回当前时间，格式为HHMMSS
 * 
 * 获取当前分钟时间（精确到分钟）。
 */
uint32_t CtaStraBaseCtx::stra_get_time()
{
	return _engine->get_min_time();  // 从引擎获取当前分钟时间
}

/**
 * @brief 获取资金数据实现
 * @param flag 资金数据类型：0-动态余额，1-平仓盈亏，2-浮动盈亏，3-手续费
 * @return double 返回对应的资金数据
 * 
 * 获取策略的资金数据，包括动态余额、平仓盈亏、浮动盈亏、手续费等。
 * 动态余额 = 平仓盈亏 - 手续费 + 浮动盈亏
 */
double CtaStraBaseCtx::stra_get_fund_data(int flag )
{
	switch (flag)
	{
	case 0:  // 动态余额
		return _fund_info._total_profit - _fund_info._total_fees + _fund_info._total_dynprofit;
	case 1:  // 平仓盈亏
		return _fund_info._total_profit;
	case 2:  // 浮动盈亏
		return _fund_info._total_dynprofit;
	case 3:  // 手续费
		return _fund_info._total_fees;
	default:  // 未知类型
		return 0.0;
	}
}

/**
 * @brief 记录信息日志实现
 * @param message 日志消息
 * 
 * 记录策略的信息级别日志。
 */
void CtaStraBaseCtx::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 记录信息日志
}

/**
 * @brief 记录调试日志实现
 * @param message 日志消息
 * 
 * 记录策略的调试级别日志。
 */
void CtaStraBaseCtx::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 记录调试日志
}

/**
 * @brief 记录警告日志实现
 * @param message 日志消息
 * 
 * 记录策略的警告级别日志。
 */
void CtaStraBaseCtx::stra_log_warn(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_WARN, message);  // 记录警告日志
}

/**
 * @brief 记录错误日志实现
 * @param message 日志消息
 * 
 * 记录策略的错误级别日志。
 */
void CtaStraBaseCtx::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 记录错误日志
}

/**
 * @brief 加载用户数据实现
 * @param key 数据键
 * @param defVal 默认值，默认为空字符串
 * @return const char* 返回数据值，如果不存在则返回默认值
 * 
 * 从用户数据映射表中加载指定键的数据。
 * 如果数据不存在，则返回默认值。
 */
const char* CtaStraBaseCtx::stra_load_user_data(const char* key, const char* defVal /*= ""*/)
{
	auto it = _user_datas.find(key);  // 在用户数据映射表中查找
	if (it != _user_datas.end())      // 如果找到
		return it->second.c_str();     // 返回数据值

	return defVal;  // 如果未找到，返回默认值
}

/**
 * @brief 保存用户数据实现
 * @param key 数据键
 * @param val 数据值
 * 
 * 将用户数据保存到用户数据映射表中，并标记数据已修改。
 * 数据会在适当时机自动保存到文件。
 */
void CtaStraBaseCtx::stra_save_user_data(const char* key, const char* val)
{
	_user_datas[key] = val;  // 保存用户数据到映射表
	_ud_modified = true;     // 标记用户数据已修改
}

/**
 * @brief 获取首次开仓时间实现
 * @param stdCode 合约代码
 * @return uint64_t 返回首次开仓时间，如果不存在则返回0
 * 
 * 获取指定合约的首次开仓时间（最早一条持仓明细的开仓时间）。
 */
uint64_t CtaStraBaseCtx::stra_get_first_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())         // 如果持仓明细为空
		return 0;                        // 返回0

	return pInfo._details[0]._opentime;  // 返回第一条持仓明细的开仓时间
}

/**
 * @brief 获取首次开仓标签实现
 * @param stdCode 合约代码
 * @return const char* 返回首次开仓标签，如果不存在则返回空字符串
 * 
 * 获取指定合约的首次开仓标签（最早一条持仓明细的开仓标签）。
 */
const char* CtaStraBaseCtx::stra_get_last_entertag(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return "";                      // 返回空字符串

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())         // 如果持仓明细为空
		return "";                       // 返回空字符串

	return pInfo._details[0]._opentag;  // 返回第一条持仓明细的开仓标签
}


/**
 * @brief 获取最后平仓时间实现
 * @param stdCode 合约代码
 * @return uint64_t 返回最后平仓时间，如果不存在则返回0
 * 
 * 获取指定合约的最后平仓时间。
 */
uint64_t CtaStraBaseCtx::stra_get_last_exittime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._last_exittime;         // 返回最后平仓时间
}

/**
 * @brief 获取最后开仓时间实现
 * @param stdCode 合约代码
 * @return uint64_t 返回最后开仓时间，如果不存在则返回0
 * 
 * 获取指定合约的最后开仓时间（最新一条持仓明细的开仓时间）。
 */
uint64_t CtaStraBaseCtx::stra_get_last_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())         // 如果持仓明细为空
		return 0;                        // 返回0

	return pInfo._details[pInfo._details.size() - 1]._opentime;  // 返回最后一条持仓明细的开仓时间
}

/**
 * @brief 获取最后开仓价格实现
 * @param stdCode 合约代码
 * @return double 返回最后开仓价格，如果不存在则返回0
 * 
 * 获取指定合约的最后开仓价格（最新一条持仓明细的开仓价格）。
 */
double CtaStraBaseCtx::stra_get_last_enterprice(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())         // 如果持仓明细为空
		return 0;                        // 返回0

	return pInfo._details[pInfo._details.size() - 1]._price;  // 返回最后一条持仓明细的开仓价格
}

/**
 * @brief 获取持仓数量实现
 * @param stdCode 合约代码
 * @param bOnlyValid 是否只返回可平持仓，默认为false
 * @param userTag 用户标签，默认为空字符串
 * @return double 返回持仓数量，正数表示多头，负数表示空头
 * 
 * 获取指定合约的持仓数量。
 * 如果存在未处理的信号，则返回信号的目标仓位。
 * 如果指定了userTag，则返回该标签对应的持仓数量。
 * 如果bOnlyValid为true且userTag为空，则返回可平持仓（总持仓减去冻结持仓）。
 */
double CtaStraBaseCtx::stra_get_position(const char* stdCode, bool bOnlyValid /* = false */, const char* userTag /* = "" */)
{
	double totalPos = 0;  // 总持仓
	auto sit = _sig_map.find(stdCode);  // 查找该合约的交易信号
	if(sit != _sig_map.end())  // 如果存在未处理的信号
	{
		WTSLogger::warn("{} has untouched signal, [userTag] will be ignored", stdCode);  // 记录警告日志
		totalPos = sit->second._volume;  // 返回信号的目标仓位
	}

	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return totalPos;                 // 返回总持仓（可能是信号目标仓位）

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	totalPos = pInfo._volume;            // 获取总持仓
	if (strlen(userTag) == 0)  // 如果用户标签为空
	{
		//只有userTag为空的时候时候，才会用bOnlyValid
		if (bOnlyValid)  // 如果只返回可平持仓
		{
			//这里理论上，只有多头才会进到这里
			//其他地方要保证，空头持仓的话，_frozen要为0
			return totalPos - pInfo._frozen;  // 返回可平持仓（总持仓减去冻结持仓）
		}
		else  // 如果返回总持仓
			return totalPos;  // 返回总持仓
	}
	else  // 如果指定了用户标签
	{
		// 遍历持仓明细，查找匹配用户标签的持仓
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
		{
			const DetailInfo& dInfo = (*it);  // 获取持仓明细
			if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
				continue;                               // 跳过

			return dInfo._volume;  // 返回该明细的持仓数量
		}
	}

	return 0;  // 如果未找到匹配的持仓，返回0
}

/**
 * @brief 获取持仓均价实现
 * @param stdCode 合约代码
 * @return double 返回持仓均价，如果不存在则返回0
 * 
 * 计算指定合约的持仓均价（加权平均）。
 * 持仓均价 = 所有持仓明细的开仓价格 * 持仓数量的总和 / 总持仓数量
 */
double CtaStraBaseCtx::stra_get_position_avgpx(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._volume == 0)            // 如果持仓数量为0
		return 0.0;                      // 返回0

	double amount = 0.0;  // 总金额
	// 遍历所有持仓明细，累加（开仓价格 * 持仓数量）
	for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)
	{
		const DetailInfo& dInfo = *dit;  // 获取持仓明细
		amount += dInfo._price*dInfo._volume;  // 累加（开仓价格 * 持仓数量）
	}

	return amount / pInfo._volume;  // 返回持仓均价（总金额 / 总持仓数量）
}

/**
 * @brief 获取持仓盈亏实现
 * @param stdCode 合约代码
 * @return double 返回持仓盈亏（浮动盈亏），如果不存在则返回0
 * 
 * 获取指定合约的持仓盈亏（浮动盈亏）。
 */
double CtaStraBaseCtx::stra_get_position_profit(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._dynprofit;            // 返回浮动盈亏
}

/**
 * @brief 获取明细开仓时间实现
 * @param stdCode 合约代码
 * @param userTag 用户标签
 * @return uint64_t 返回明细开仓时间，如果不存在则返回0
 * 
 * 根据用户标签查找指定合约的持仓明细，返回该明细的开仓时间。
 */
uint64_t CtaStraBaseCtx::stra_get_detail_entertime(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	// 遍历持仓明细，查找匹配用户标签的明细
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;                               // 跳过

		return dInfo._opentime;  // 返回该明细的开仓时间
	}

	return 0;  // 如果未找到匹配的明细，返回0
}

/**
 * @brief 获取明细成本实现
 * @param stdCode 合约代码
 * @param userTag 用户标签
 * @return double 返回明细成本（开仓价格），如果不存在则返回0
 * 
 * 根据用户标签查找指定合约的持仓明细，返回该明细的开仓价格（成本价）。
 */
double CtaStraBaseCtx::stra_get_detail_cost(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	// 遍历持仓明细，查找匹配用户标签的明细
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;                               // 跳过

		return dInfo._price;  // 返回该明细的开仓价格（成本价）
	}

	return 0.0;  // 如果未找到匹配的明细，返回0
}

/**
 * @brief 获取明细盈亏实现
 * @param stdCode 合约代码
 * @param userTag 用户标签
 * @param flag 数据类型：0-当前盈亏，1-最大盈利，-1-最大亏损，2-最高价格，-2-最低价格
 * @return double 返回对应的数据，如果不存在则返回0
 * 
 * 根据用户标签查找指定合约的持仓明细，返回该明细的盈亏或价格统计数据。
 */
double CtaStraBaseCtx::stra_get_detail_profit(const char* stdCode, const char* userTag, int flag /* = 0 */)
{
	auto it = _pos_map.find(stdCode);  // 查找该合约的持仓信息
	if (it == _pos_map.end())          // 如果未找到
		return 0;                        // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	// 遍历持仓明细，查找匹配用户标签的明细
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果标签不匹配
			continue;                               // 跳过

		// 根据flag返回对应的数据
		switch (flag)
		{
		case 0:  // 当前盈亏
			return dInfo._profit;
		case 1:  // 最大盈利
			return dInfo._max_profit;
		case -1:  // 最大亏损
			return dInfo._max_loss;
		case 2:  // 最高价格
			return dInfo._max_price;
		case -2:  // 最低价格
			return dInfo._min_price;
		}
	}

	return 0.0;  // 如果未找到匹配的明细，返回0
}

/**
 * @brief 设置图表K线实现
 * @param stdCode 合约代码
 * @param period K线周期，如"m5"、"d1"等
 * 
 * 设置策略图表的K线合约和周期。
 * 如果未设置，则使用主K线作为图表K线。
 */
void CtaStraBaseCtx::set_chart_kline(const char* stdCode, const char* period)
{
	_chart_code = stdCode;    // 设置图表合约代码
	_chart_period = period;    // 设置图表周期
}

/**
 * @brief 添加图表标记实现
 * @param price 价格
 * @param icon 图标名称
 * @param tag 标签文本
 * 
 * 在图表上添加标记（如买入点、卖出点等）。
 * 只能在策略调度期间（on_calculate回调中）调用。
 * 标记会记录到日志文件并通知引擎显示。
 */
void CtaStraBaseCtx::add_chart_mark(double price, const char* icon, const char* tag)
{
	if (!_is_in_schedule)  // 如果不在调度期间
	{
		WTSLogger::error("Marks can be added only during schedule");  // 记录错误日志
		return;  // 直接返回
	}

	// 构建当前时间戳：日期 * 10000 + 分钟时间
	uint64_t curTime = stra_get_date();  // 获取当前日期
	curTime = curTime * 10000 + stra_get_time();  // 加上分钟时间

	// 如果标记日志文件已初始化，记录标记到日志文件
	if (_mark_logs)
	{
		std::stringstream ss;  // 创建字符串流
		ss << curTime << "," << price << "," << icon << "," << tag << std::endl;  // 格式化：时间,价格,图标,标签
		_mark_logs->write_file(ss.str());  // 写入日志文件
	}

	_engine->notify_chart_marker(curTime, _name.c_str(), price, icon, tag);  // 通知引擎显示标记
}

/**
 * @brief 注册指标实现
 * @param idxName 指标名称
 * @param indexType 指标类型
 * 
 * 注册一个指标到图表系统中。
 * 注册后可以在该指标下添加指标线和基准线。
 */
void CtaStraBaseCtx::register_index(const char* idxName, uint32_t indexType)
{
	ChartIndex& cIndex = _chart_indice[idxName];  // 获取或创建指标对象
	cIndex._name = idxName;                        // 设置指标名称
	cIndex._indexType = indexType;                 // 设置指标类型
}

/**
 * @brief 注册指标线实现
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param lineType 线条类型
 * @return bool 返回true表示成功，false表示失败（指标未注册）
 * 
 * 在指定指标下注册一条指标线。
 * 指标线用于显示指标的具体数值。
 */
bool CtaStraBaseCtx::register_index_line(const char* idxName, const char* lineName, uint32_t lineType)
{
	auto it = _chart_indice.find(idxName);  // 查找指标
	if (it == _chart_indice.end())          // 如果指标未注册
	{
		WTSLogger::error("Index {} not registered", idxName);  // 记录错误日志
		return false;  // 返回失败
	}

	ChartIndex& cIndex = (ChartIndex&)it->second;  // 获取指标对象
	ChartLine& cLine = cIndex._lines[lineName];     // 获取或创建指标线对象
	cLine._name = lineName;                         // 设置线条名称
	cLine._lineType = lineType;                     // 设置线条类型
	return true;                                    // 返回成功
}

/**
 * @brief 添加指标基准线实现
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param val 基准值
 * @return bool 返回true表示成功，false表示失败（指标未注册）
 * 
 * 为指定指标的线条添加基准线（参考线）。
 * 基准线通常用于显示重要的参考价位（如超买超卖线等）。
 */
bool CtaStraBaseCtx::add_index_baseline(const char* idxName, const char* lineName, double val)
{
	auto it = _chart_indice.find(idxName);  // 查找指标
	if (it == _chart_indice.end())          // 如果指标未注册
	{
		WTSLogger::error("Index {} not registered", idxName);  // 记录错误日志
		return false;  // 返回失败
	}

	ChartIndex& cIndex = (ChartIndex&)it->second;  // 获取指标对象
	cIndex._base_lines[lineName] = val;            // 设置基准线值
	return true;                                    // 返回成功
}

/**
 * @brief 设置指标值实现
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param val 指标值
 * @return bool 返回true表示成功，false表示失败
 * 
 * 设置指定指标的线条在当前时刻的值。
 * 只能在策略调度期间（on_calculate回调中）调用。
 * 指标值会记录到日志文件并通知引擎显示。
 */
bool CtaStraBaseCtx::set_index_value(const char* idxName, const char* lineName, double val)
{
	if (!_is_in_schedule)  // 如果不在调度期间
	{
		WTSLogger::error("Marks can be added only during schedule");  // 记录错误日志
		return false;  // 返回失败
	}

	auto ait = _chart_indice.find(idxName);  // 查找指标
	if (ait == _chart_indice.end())         // 如果指标未注册
	{
		WTSLogger::error("Index {} not registered", idxName);  // 记录错误日志
		return false;  // 返回失败
	}

	ChartIndex& cIndex = (ChartIndex&)ait->second;  // 获取指标对象
	auto bit = cIndex._lines.find(lineName);        // 查找指标线
	if (bit == cIndex._lines.end())                 // 如果指标线未注册
	{
		WTSLogger::error("Line {} of index {} not registered", lineName, idxName);  // 记录错误日志
		return false;  // 返回失败
	}

	// 构建当前时间戳：日期 * 10000 + 分钟时间
	uint64_t curTime = stra_get_date();  // 获取当前日期
	curTime = curTime * 10000 + stra_get_time();  // 加上分钟时间

	// 如果指标日志文件已初始化，记录指标值到日志文件
	if (_idx_logs)
	{
		std::stringstream ss;  // 创建字符串流
		ss << curTime << "," << idxName << "," << lineName << "," << val << std::endl;  // 格式化：时间,指标名,线条名,值
		_idx_logs->write_file(ss.str());  // 写入日志文件
	}

	_engine->notify_chart_index(curTime, _name.c_str(), idxName, lineName, val);  // 通知引擎显示指标值

	return true;  // 返回成功
}

