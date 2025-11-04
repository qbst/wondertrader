/*!
* \file SelStraBaseCtx.cpp
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief 选股策略基础上下文实现文件
*
* 文件设计逻辑与作用总结：
* 本文件实现选股策略基础上下文类SelStraBaseCtx的所有功能。
* 
* 主要实现功能：
* 1. 策略生命周期管理：初始化、交易日开始/结束、定时调度等回调处理。
* 2. 持仓管理：支持多明细持仓、T+1规则、冻结持仓等功能。
* 3. 信号管理：接收和处理策略发出的持仓信号，在合适的时机执行。
* 4. 盈亏计算：实时计算持仓盈亏、累计盈亏、动态盈亏等。
* 5. 数据持久化：保存和加载持仓、资金、信号等数据，支持策略重启恢复。
* 6. 用户数据管理：提供用户自定义数据的保存和加载功能。
* 7. 日志记录：记录交易、平仓、资金、信号、持仓等日志。
* 8. 行情数据访问：提供K线、Tick、价格等行情数据查询接口。
* 9. 策略接口：为策略提供统一的API接口，包括持仓操作、价格查询、日志输出等。
*/
#include "SelStraBaseCtx.h"  // 包含选股策略基础上下文头文件
#include "WtSelEngine.h"  // 包含选股引擎头文件
#include "WtHelper.h"  // 包含WonderTrader辅助工具类

#include <exception>  // 包含异常处理头文件
#include <rapidjson/document.h>  // 包含RapidJSON文档类
#include <rapidjson/prettywriter.h>  // 包含RapidJSON格式化写入器

#include "../Share/StrUtil.hpp"  // 包含字符串工具类
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息头文件
#include "../Includes/WTSSessionInfo.hpp"  // 包含交易会话信息头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件
#include "../Share/decimal.h"  // 包含高精度小数工具类
#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具类

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

namespace rj = rapidjson;  // 使用rapidjson命名空间别名

/**
 * @brief 生成选股策略上下文ID
 * @return uint32_t 返回唯一的上下文ID
 *
 * 使用原子操作生成唯一的上下文ID，从3000开始递增。
 * 这是一个线程安全的ID生成函数。
 */
inline uint32_t makeSelCtxId()  // 生成选股策略上下文ID
{
	static std::atomic<uint32_t> _auto_context_id{ 3000 };  // 静态原子计数器，初始值为3000
	return _auto_context_id.fetch_add(1);  // 原子递增并返回递增前的值
}


/**
 * @brief 构造函数实现
 * @param engine 选股引擎指针
 * @param name 策略名称
 * @param slippage 滑点设置（回测时使用）
 *
 * 初始化选股策略基础上下文对象，设置引擎、名称和滑点参数。
 * 调用makeSelCtxId()生成唯一的上下文ID。
 */
SelStraBaseCtx::SelStraBaseCtx(WtSelEngine* engine, const char* name, int32_t slippage)
	: ISelStraCtx(name)  // 调用基类构造函数，传入策略名称
	, _engine(engine)  // 初始化选股引擎指针
	, _total_calc_time(0)  // 初始化总计算时间为0
	, _emit_times(0)  // 初始化总计算次数为0
	, _is_in_schedule(false)  // 初始化调度标志为false
	, _ud_modified(false)  // 初始化用户数据修改标志为false
	, _schedule_date(0)  // 初始化调度日期为0
	, _schedule_time(0)  // 初始化调度时间为0
	, _slippage(slippage)  // 初始化滑点设置
{
	_context_id = makeSelCtxId();  // 生成唯一的上下文ID
}


/**
 * @brief 析构函数实现
 *
 * 清理解股策略基础上下文对象。
 * 注意：智能指针会自动管理资源释放。
 */
SelStraBaseCtx::~SelStraBaseCtx()
{
}

/**
 * @brief 初始化输出文件实现
 *
 * 创建并初始化交易日志、平仓日志、资金日志、信号日志、持仓日志等输出文件。
 * 如果文件已存在，则追加写入；如果文件不存在，则创建新文件并写入表头。
 */
void SelStraBaseCtx::init_outputs()
{
	std::string folder = WtHelper::getOutputDir();  // 获取输出目录路径
	folder += _name;  // 追加策略名称
	folder += "//";  // 追加目录分隔符
	BoostFile::create_directories(folder.c_str());  // 创建输出目录（如果不存在）

	std::string filename = folder + "trades.csv";  // 交易日志文件名
	_trade_logs.reset(new BoostFile());  // 创建交易日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_trade_logs->create_or_open_file(filename.c_str());  // 创建或打开交易日志文件
		if (isNewFile)  // 如果是新文件
		{
			_trade_logs->write_file("code,time,direct,action,price,qty,tag,fee\n");  // 写入CSV表头：合约代码,时间,方向,动作,价格,数量,标签,手续费
		}
		else  // 如果文件已存在
		{
			_trade_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "closes.csv";  // 平仓日志文件名
	_close_logs.reset(new BoostFile());  // 创建平仓日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_close_logs->create_or_open_file(filename.c_str());  // 创建或打开平仓日志文件
		if (isNewFile)  // 如果是新文件
		{
			_close_logs->write_file("code,direct,opentime,openprice,closetime,closeprice,qty,profit,totalprofit,entertag,exittag\n");  // 写入CSV表头：合约代码,方向,开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,累计盈亏,开仓标签,平仓标签
		}
		else  // 如果文件已存在
		{
			_close_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "funds.csv";  // 资金日志文件名
	_fund_logs.reset(new BoostFile());  // 创建资金日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_fund_logs->create_or_open_file(filename.c_str());  // 创建或打开资金日志文件
		if (isNewFile)  // 如果是新文件
		{
			_fund_logs->write_file("date,closeprofit,positionprofit,dynbalance,fee\n");  // 写入CSV表头：日期,累计盈亏,持仓盈亏,动态权益,手续费
		}
		else  // 如果文件已存在
		{
			_fund_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "signals.csv";  // 信号日志文件名
	_sig_logs.reset(new BoostFile());  // 创建信号日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_sig_logs->create_or_open_file(filename.c_str());  // 创建或打开信号日志文件
		if (isNewFile)  // 如果是新文件
		{
			_sig_logs->write_file("code,target,sigprice,gentime,usertag\n");  // 写入CSV表头：合约代码,目标持仓,信号价格,生成时间,用户标签
		}
		else  // 如果文件已存在
		{
			_sig_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "positions.csv";  // 持仓日志文件名
	_pos_logs.reset(new BoostFile());  // 创建持仓日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_pos_logs->create_or_open_file(filename.c_str());  // 创建或打开持仓日志文件
		if (isNewFile)  // 如果是新文件
		{
			_pos_logs->write_file("date,code,volume,closeprofit,dynprofit\n");  // 写入CSV表头：日期,合约代码,持仓数量,累计盈亏,动态盈亏
		}
		else  // 如果文件已存在
		{
			_pos_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}
}

/**
 * @brief 记录信号日志实现
 * @param stdCode 标准合约代码
 * @param target 目标持仓数量
 * @param price 信号价格
 * @param gentime 信号生成时间
 * @param usertag 用户标签，默认空字符串
 *
 * 将持仓信号记录到信号日志文件中。
 * 格式：合约代码,目标持仓,信号价格,生成时间,用户标签
 */
void SelStraBaseCtx::log_signal(const char* stdCode, double target, double price, uint64_t gentime, const char* usertag /* = "" */)
{
	if (_sig_logs)  // 如果信号日志文件对象存在
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << target << "," << price << "," << gentime << "," << usertag << "\n";  // 格式化信号日志：合约代码,目标持仓,信号价格,生成时间,用户标签
		_sig_logs->write_file(ss.str());  // 写入信号日志文件
	}
}

/**
 * @brief 记录交易日志实现
 * @param stdCode 标准合约代码
 * @param isLong 是否做多
 * @param isOpen 是否开仓
 * @param curTime 当前时间
 * @param price 成交价格
 * @param qty 成交数量
 * @param userTag 用户标签，默认空字符串
 * @param fee 手续费，默认0.0
 *
 * 将交易记录写入交易日志文件。
 * 格式：合约代码,时间,方向(LONG/SHORT),动作(OPEN/CLOSE),价格,数量,标签,手续费
 */
void SelStraBaseCtx::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, const char* userTag, double fee)
{
	if (_trade_logs)  // 如果交易日志文件对象存在
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE") << "," << price << "," << qty << "," << userTag << "," << fee << "\n";  // 格式化交易日志：合约代码,时间,方向,动作,价格,数量,标签,手续费
		_trade_logs->write_file(ss.str());  // 写入交易日志文件
	}
}

/**
 * @brief 记录平仓日志实现
 * @param stdCode 标准合约代码
 * @param isLong 是否做多
 * @param openTime 开仓时间
 * @param openpx 开仓价格
 * @param closeTime 平仓时间
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 平仓盈亏
 * @param totalprofit 累计盈亏，默认0
 * @param enterTag 开仓标签，默认空字符串
 * @param exitTag 平仓标签，默认空字符串
 *
 * 将平仓记录写入平仓日志文件。
 * 格式：合约代码,方向,开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,累计盈亏,开仓标签,平仓标签
 * 
 * 注意：代码中使用了_trade_logs而不是_close_logs，这可能是一个bug。
 */
void SelStraBaseCtx::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
	double profit, double totalprofit /* = 0 */, const char* enterTag /* = "" */, const char* exitTag /* = "" */)
{
	if (_close_logs)  // 如果平仓日志文件对象存在
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
			<< "," << closeTime << "," << closepx << "," << qty << "," << profit << ","
			<< totalprofit << "," << enterTag << "," << exitTag << "\n";  // 格式化平仓日志：合约代码,方向,开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,累计盈亏,开仓标签,平仓标签
		_trade_logs->write_file(ss.str());  // 写入交易日志文件（注意：这里应该使用_close_logs）
	}
}

/**
 * @brief 保存用户数据实现
 *
 * 将用户自定义数据保存到JSON文件中。
 * 文件路径：用户数据目录/ud_策略名称.json
 * 使用RapidJSON格式化写入，生成格式化的JSON文件。
 */
void SelStraBaseCtx::save_userdata()
{
	//ini.save(filename.c_str());  // 注释掉的旧代码：使用INI格式保存
	rj::Document root(rj::kObjectType);  // 创建JSON文档对象（对象类型）
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器
	for (auto it = _user_datas.begin(); it != _user_datas.end(); it++)  // 遍历用户数据映射表
	{
		root.AddMember(rj::Value(it->first.c_str(), allocator), rj::Value(it->second.c_str(), allocator), allocator);  // 将键值对添加到JSON对象中
	}

	{
		std::string filename = WtHelper::getStraUsrDatDir();  // 获取策略用户数据目录路径
		filename += "ud_";  // 追加前缀"ud_"
		filename += _name;  // 追加策略名称
		filename += ".json";  // 追加文件扩展名

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 如果成功创建新文件
		{
			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区（格式化）
			bf.write_file(sb.GetString());  // 将缓冲区内容写入文件
			bf.close_file();  // 关闭文件
		}
	}
}

/**
 * @brief 加载用户数据实现
 *
 * 从JSON文件中加载用户自定义数据。
 * 文件路径：用户数据目录/ud_策略名称.json
 * 如果文件不存在或解析失败，则直接返回。
 */
void SelStraBaseCtx::load_userdata()
{
	std::string filename = WtHelper::getStraUsrDatDir();  // 获取策略用户数据目录路径
	filename += "ud_";  // 追加前缀"ud_"
	filename += _name;  // 追加策略名称
	filename += ".json";  // 追加文件扩展名

	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回
	}

	std::string content;  // 定义文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;  // 直接返回

	rj::Document root;  // 创建JSON文档对象
	root.Parse(content.c_str());  // 解析JSON字符串

	if (root.HasParseError())  // 如果解析出错
		return;  // 直接返回

	for (auto& m : root.GetObject())  // 遍历JSON对象的所有成员
	{
		const char* key = m.name.GetString();  // 获取键（字符串）
		const char* val = m.value.GetString();  // 获取值（字符串）
		_user_datas[key] = val;  // 将键值对保存到用户数据映射表中
	}
}

/**
 * @brief 加载数据实现
 * @param flag 加载标志，默认0xFFFFFFFF（加载所有数据）
 *
 * 从JSON文件中加载持仓、资金、信号等数据。
 * 文件路径：策略数据目录/策略名称.json
 * 
 * 处理流程：
 * 1. 读取并解析JSON文件。
 * 2. 加载资金信息（累计盈亏、动态盈亏、手续费等）。
 * 3. 加载持仓信息（包括持仓明细），检查合约是否过期。
 * 4. 加载信号信息，检查合约是否过期。
 * 5. 对于有效持仓和信号，自动订阅Tick数据。
 */
void SelStraBaseCtx::load_data(uint32_t flag /* = 0xFFFFFFFF */)
{
	std::string filename = WtHelper::getStraDataDir();  // 获取策略数据目录路径
	filename += _name;  // 追加策略名称
	filename += ".json";  // 追加文件扩展名

	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回
	}

	std::string content;  // 定义文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;  // 直接返回

	rj::Document root;  // 创建JSON文档对象
	root.Parse(content.c_str());  // 解析JSON字符串

	if (root.HasParseError())  // 如果解析出错
		return;  // 直接返回

	if (root.HasMember("fund"))  // 如果JSON中包含资金信息
	{
		//读取资金
		const rj::Value& jFund = root["fund"];  // 获取资金对象
		if (!jFund.IsNull() && jFund.IsObject())  // 如果资金对象有效且为对象类型
		{
			_fund_info._total_profit = jFund["total_profit"].GetDouble();  // 读取累计盈亏
			_fund_info._total_dynprofit = jFund["total_dynprofit"].GetDouble();  // 读取总动态盈亏
			uint32_t tdate = jFund["tdate"].GetUint();  // 读取交易日
			if (tdate == _engine->get_trading_date())  // 如果交易日与当前交易日相同
				_fund_info._total_fees = jFund["total_fees"].GetDouble();  // 读取总手续费（只有当天的手续费需要恢复）
		}
	}

	{//读取仓位
		double total_profit = 0;  // 初始化累计盈亏总和
		double total_dynprofit = 0;  // 初始化动态盈亏总和
		const rj::Value& jPos = root["positions"];  // 获取持仓数组
		if (!jPos.IsNull() && jPos.IsArray())  // 如果持仓数组有效且为数组类型
		{
			for (const rj::Value& pItem : jPos.GetArray())  // 遍历所有持仓项
			{
				const char* stdCode = pItem["code"].GetString();  // 获取合约代码
				const char* ruleTag = _engine->get_hot_mgr()->getRuleTag(stdCode);  // 获取合约规则标签（用于判断是否为主力合约）
				bool isExpired = (strlen(ruleTag) == 0 && _engine->get_contract_info(stdCode) == NULL);  // 判断合约是否过期（既没有规则标签，也没有合约信息）

				if (isExpired)  // 如果合约已过期
					log_info("{} not exists or expired, position ignored", stdCode);  // 记录信息日志：合约不存在或已过期，持仓被忽略

				PosInfo& pInfo = _pos_map[stdCode];  // 获取或创建持仓信息
				pInfo._closeprofit = pItem["closeprofit"].GetDouble();  // 读取累计平仓盈亏
				pInfo._last_entertime = pItem["lastentertime"].GetUint64();  // 读取最后开仓时间
				pInfo._last_exittime = pItem["lastexittime"].GetUint64();  // 读取最后平仓时间
				pInfo._volume = isExpired ? 0 : pItem["volume"].GetDouble();  // 读取持仓数量（如果合约过期则置为0）
				if (pItem.HasMember("frozen") && !isExpired)  // 如果包含冻结持仓信息且合约未过期
				{
					pInfo._frozen = pItem["frozen"].GetDouble();  // 读取冻结持仓数量
					pInfo._frozen_date = pItem["frozendate"].GetUint();  // 读取冻结日期
				}

				if (pInfo._volume == 0 || isExpired)  // 如果持仓为0或合约已过期
				{
					pInfo._dynprofit = 0;  // 重置动态盈亏为0
					pInfo._frozen = 0;  // 重置冻结持仓为0
				}
				else  // 如果持仓不为0且合约未过期
					pInfo._dynprofit = pItem["dynprofit"].GetDouble();  // 读取动态盈亏				

				total_profit += pInfo._closeprofit;  // 累加累计盈亏
				total_dynprofit += pInfo._dynprofit;  // 累加动态盈亏

				const rj::Value& details = pItem["details"];  // 获取持仓明细数组
				if (details.IsNull() || !details.IsArray() || details.Size() == 0 || isExpired)  // 如果明细数组无效、为空或合约已过期
					continue;  // 跳过，继续下一个持仓

				pInfo._details.resize(details.Size());  // 调整持仓明细列表大小

				for (uint32_t i = 0; i < details.Size(); i++)  // 遍历所有持仓明细
				{
					const rj::Value& dItem = details[i];  // 获取持仓明细项
					DetailInfo& dInfo = pInfo._details[i];  // 获取持仓明细引用
					dInfo._long = dItem["long"].GetBool();  // 读取是否做多
					dInfo._price = dItem["price"].GetDouble();  // 读取开仓价格
					dInfo._volume = dItem["volume"].GetDouble();  // 读取持仓数量
					dInfo._opentime = dItem["opentime"].GetUint64();  // 读取开仓时间
					if (dItem.HasMember("opentdate"))  // 如果包含开仓日期
						dInfo._opentdate = dItem["opentdate"].GetUint();  // 读取开仓日期

					if (dItem.HasMember("maxprice"))  // 如果包含最高价
						dInfo._max_price = dItem["maxprice"].GetDouble();  // 读取最高价
					else  // 如果不包含最高价
						dInfo._max_price = dInfo._price;  // 使用开仓价格作为最高价

					if (dItem.HasMember("minprice"))  // 如果包含最低价
						dInfo._min_price = dItem["minprice"].GetDouble();  // 读取最低价
					else  // 如果不包含最低价
						dInfo._min_price = dInfo._price;  // 使用开仓价格作为最低价

					dInfo._profit = dItem["profit"].GetDouble();  // 读取当前盈亏
					dInfo._max_profit = dItem["maxprofit"].GetDouble();  // 读取最大盈利
					dInfo._max_loss = dItem["maxloss"].GetDouble();  // 读取最大亏损

					strcpy(dInfo._opentag, dItem["opentag"].GetString());  // 复制开仓标签
				}

				if (!isExpired)  // 如果合约未过期
				{
					log_info("Position confirmed,{} -> {}", stdCode, pInfo._volume);  // 记录信息日志：持仓确认
					stra_sub_ticks(stdCode);  // 订阅该合约的Tick数据
				}
			}
		}

		_fund_info._total_profit = total_profit;  // 更新总累计盈亏
		_fund_info._total_dynprofit = total_dynprofit;  // 更新总动态盈亏
	}

	if (root.HasMember("signals"))  // 如果JSON中包含信号信息
	{
		//读取信号
		const rj::Value& jSignals = root["signals"];  // 获取信号对象
		if (!jSignals.IsNull() && jSignals.IsObject())  // 如果信号对象有效且为对象类型
		{
			for (auto& m : jSignals.GetObject())  // 遍历所有信号项
			{
				const char* stdCode = m.name.GetString();  // 获取合约代码（作为键）
				const char* ruleTag = _engine->get_hot_mgr()->getRuleTag(stdCode);  // 获取合约规则标签
				if (strlen(ruleTag) == 0 && _engine->get_contract_info(stdCode) == NULL)  // 如果合约已过期
				{
					log_info("{} not exists or expired, signal ignored", stdCode);  // 记录信息日志：合约不存在或已过期，信号被忽略
					continue;  // 跳过，继续下一个信号
				}

				const rj::Value& jItem = m.value;  // 获取信号值对象

				SigInfo& sInfo = _sig_map[stdCode];  // 获取或创建信号信息
				sInfo._usertag = jItem["usertag"].GetString();  // 读取用户标签
				sInfo._volume = jItem["volume"].GetDouble();  // 读取目标持仓数量
				sInfo._sigprice = jItem["sigprice"].GetDouble();  // 读取信号价格
				sInfo._gentime = jItem["gentime"].GetUint64();  // 读取信号生成时间

				log_info("{} untouched signal recovered, target pos: {}", stdCode, sInfo._volume);  // 记录信息日志：未触发的信号已恢复
				stra_sub_ticks(stdCode);  // 订阅该合约的Tick数据
			}
		}
	}
}

/**
 * @brief 保存数据实现
 * @param flag 保存标志，默认0xFFFFFFFF（保存所有数据）
 *
 * 将持仓、资金、信号等数据保存到JSON文件中。
 * 文件路径：策略数据目录/策略名称.json
 * 
 * 处理流程：
 * 1. 创建JSON文档对象。
 * 2. 保存持仓数据（包括持仓明细）。
 * 3. 保存资金信息。
 * 4. 保存信号信息。
 * 5. 将JSON文档写入文件。
 */
void SelStraBaseCtx::save_data(uint32_t flag /* = 0xFFFFFFFF */)
{
	rj::Document root(rj::kObjectType);  // 创建JSON文档对象（对象类型）

	{//持仓数据保存
		rj::Value jPos(rj::kArrayType);  // 创建持仓数组

		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)  // 遍历所有持仓
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			const PosInfo& pInfo = it->second;  // 获取持仓信息

			rj::Value pItem(rj::kObjectType);  // 创建持仓项对象
			pItem.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码
			pItem.AddMember("volume", pInfo._volume, allocator);  // 添加持仓数量
			pItem.AddMember("closeprofit", pInfo._closeprofit, allocator);  // 添加累计平仓盈亏
			pItem.AddMember("dynprofit", pInfo._dynprofit, allocator);  // 添加动态盈亏
			pItem.AddMember("lastentertime", pInfo._last_entertime, allocator);  // 添加最后开仓时间
			pItem.AddMember("lastexittime", pInfo._last_exittime, allocator);  // 添加最后平仓时间
			pItem.AddMember("frozen", pInfo._frozen, allocator);  // 添加冻结持仓数量
			pItem.AddMember("frozendate", pInfo._frozen_date, allocator);  // 添加冻结日期

			rj::Value details(rj::kArrayType);  // 创建持仓明细数组
			for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历所有持仓明细
			{
				const DetailInfo& dInfo = *dit;  // 获取持仓明细引用
				rj::Value dItem(rj::kObjectType);  // 创建持仓明细项对象
				dItem.AddMember("long", dInfo._long, allocator);  // 添加是否做多
				dItem.AddMember("price", dInfo._price, allocator);  // 添加开仓价格
				dItem.AddMember("maxprice", dInfo._max_price, allocator);  // 添加最高价
				dItem.AddMember("minprice", dInfo._min_price, allocator);  // 添加最低价
				dItem.AddMember("volume", dInfo._volume, allocator);  // 添加持仓数量
				dItem.AddMember("opentime", dInfo._opentime, allocator);  // 添加开仓时间
				dItem.AddMember("opentdate", dInfo._opentdate, allocator);  // 添加开仓日期

				dItem.AddMember("profit", dInfo._profit, allocator);  // 添加当前盈亏
				dItem.AddMember("maxprofit", dInfo._max_profit, allocator);  // 添加最大盈利
				dItem.AddMember("maxloss", dInfo._max_loss, allocator);  // 添加最大亏损
				dItem.AddMember("opentag", rj::Value(dInfo._opentag, allocator), allocator);  // 添加开仓标签

				details.PushBack(dItem, allocator);  // 将持仓明细项添加到明细数组中
			}

			pItem.AddMember("details", details, allocator);  // 将持仓明细数组添加到持仓项中

			jPos.PushBack(pItem, allocator);  // 将持仓项添加到持仓数组中
		}

		root.AddMember("positions", jPos, allocator);  // 将持仓数组添加到JSON根对象中
	}

	{//资金保存
		rj::Value jFund(rj::kObjectType);  // 创建资金对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		jFund.AddMember("total_profit", _fund_info._total_profit, allocator);  // 添加累计盈亏
		jFund.AddMember("total_dynprofit", _fund_info._total_dynprofit, allocator);  // 添加总动态盈亏
		jFund.AddMember("total_fees", _fund_info._total_fees, allocator);  // 添加总手续费
		jFund.AddMember("tdate", _engine->get_trading_date(), allocator);  // 添加交易日

		root.AddMember("fund", jFund, allocator);  // 将资金对象添加到JSON根对象中
	}

	{//信号保存
		rj::Value jSigs(rj::kObjectType);  // 创建信号对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		for (auto it : _sig_map)  // 遍历所有信号
		{
			const char* stdCode = it.first.c_str();  // 获取合约代码（作为键）
			const SigInfo& sInfo = it.second;  // 获取信号信息

			rj::Value jItem(rj::kObjectType);  // 创建信号项对象
			jItem.AddMember("usertag", rj::Value(sInfo._usertag.c_str(), allocator), allocator);  // 添加用户标签

			jItem.AddMember("volume", sInfo._volume, allocator);  // 添加目标持仓数量
			jItem.AddMember("sigprice", sInfo._sigprice, allocator);  // 添加信号价格
			jItem.AddMember("gentime", sInfo._gentime, allocator);  // 添加信号生成时间

			jSigs.AddMember(rj::Value(stdCode, allocator), jItem, allocator);  // 将信号项添加到信号对象中（以合约代码为键）
		}

		root.AddMember("signals", jSigs, allocator);  // 将信号对象添加到JSON根对象中
	}

	{
		std::string filename = WtHelper::getStraDataDir();  // 获取策略数据目录路径
		filename += _name;  // 追加策略名称
		filename += ".json";  // 追加文件扩展名

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 如果成功创建新文件
		{
			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区（格式化）
			bf.write_file(sb.GetString());  // 将缓冲区内容写入文件
			bf.close_file();  // 关闭文件
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//回调函数
/**
 * @brief K线数据回调实现
 * @param stdCode 标准合约代码
 * @param period 周期
 * @param times 周期倍数
 * @param newBar 新的K线数据
 *
 * 当收到新的K线数据时被调用，用于标记K线已收盘。
 * 处理流程：
 * 1. 检查K线数据是否有效。
 * 2. 构造完整的周期字符串（周期+倍数）。
 * 3. 构造K线标签键（合约代码#周期）。
 * 4. 标记K线已收盘。
 * 5. 调用on_bar_close回调。
 */
void SelStraBaseCtx::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (newBar == NULL)  // 如果K线数据为空
		return;  // 直接返回

	thread_local static char realPeriod[8] = { 0 };  // 线程局部静态变量：完整周期字符串
	fmtutil::format_to(realPeriod, "{}{}", period, times);  // 格式化完整周期字符串（周期+倍数）

	thread_local static char key[64] = { 0 };  // 线程局部静态变量：K线标签键
	fmtutil::format_to(key, "{}#{}", stdCode, realPeriod);  // 格式化K线标签键（合约代码#周期）

	KlineTag& tag = _kline_tags[key];  // 获取或创建K线标签
	tag._closed = true;  // 标记K线已收盘

	on_bar_close(stdCode, realPeriod, newBar);  // 调用K线收盘回调
}

/**
 * @brief 初始化回调实现
 *
 * 策略初始化时被调用，用于初始化输出文件、加载数据等。
 * 处理流程：
 * 1. 初始化输出文件（交易日志、平仓日志、资金日志、信号日志、持仓日志）。
 * 2. 加载数据（持仓、资金、信号等）。
 * 3. 加载用户数据。
 */
void SelStraBaseCtx::on_init()
{
	init_outputs();  // 初始化输出文件

	//读取数据
	load_data();  // 加载数据（持仓、资金、信号等）

	load_userdata();  // 加载用户数据
}

/**
 * @brief 更新动态盈亏实现
 * @param stdCode 标准合约代码
 * @param price 当前价格
 *
 * 根据当前价格更新指定合约的持仓动态盈亏。
 * 处理流程：
 * 1. 查找持仓信息。
 * 2. 如果持仓为0，则重置动态盈亏为0。
 * 3. 否则，遍历所有持仓明细，计算每笔明细的盈亏。
 * 4. 更新每笔明细的最大盈利、最大亏损、最高价、最低价。
 * 5. 累加所有明细的盈亏，得到总动态盈亏。
 * 6. 更新总资金动态盈亏。
 */
void SelStraBaseCtx::update_dyn_profit(const char* stdCode, double price)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it != _pos_map.end())  // 如果找到持仓
	{
		PosInfo& pInfo = (PosInfo&)it->second;  // 获取持仓信息引用
		if (pInfo._volume == 0)  // 如果持仓为0
		{
			pInfo._dynprofit = 0;  // 重置动态盈亏为0
		}
		else  // 如果持仓不为0
		{
			WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取商品信息
			double dynprofit = 0;  // 初始化动态盈亏总和
			for (auto pit = pInfo._details.begin(); pit != pInfo._details.end(); pit++)  // 遍历所有持仓明细
			{
				DetailInfo& dInfo = *pit;  // 获取持仓明细引用
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);  // 计算当前盈亏：（当前价格 - 开仓价格）* 数量 * 合约乘数 * 方向系数（做多为1，做空为-1）
				if (dInfo._profit > 0)  // 如果当前盈亏为正（盈利）
					dInfo._max_profit = std::max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利
				else if (dInfo._profit < 0)  // 如果当前盈亏为负（亏损）
					dInfo._max_loss = std::min(dInfo._profit, dInfo._max_loss);  // 更新最大亏损

				dInfo._max_price = std::max(dInfo._max_price, price);  // 更新最高价
				dInfo._min_price = std::min(dInfo._min_price, price);  // 更新最低价

				dynprofit += dInfo._profit;  // 累加动态盈亏
			}

			pInfo._dynprofit = dynprofit;  // 更新持仓动态盈亏
		}
	}

	double total_dynprofit = 0;  // 初始化总动态盈亏
	for (auto v : _pos_map)  // 遍历所有持仓
	{
		const PosInfo& pInfo = v.second;  // 获取持仓信息
		total_dynprofit += pInfo._dynprofit;  // 累加每个持仓的动态盈亏
	}

	_fund_info._total_dynprofit = total_dynprofit;  // 更新总资金动态盈亏
}

/**
 * @brief Tick数据回调实现
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据
 * @param bEmitStrategy 是否触发策略回调，默认true
 *
 * 当收到新的Tick数据时被调用，用于更新价格、触发信号、更新动态盈亏等。
 * 处理流程：
 * 1. 更新价格映射表中的最新价格。
 * 2. 检查是否有待触发的信号，如果有且在交易时间内，则执行信号。
 * 3. 更新动态盈亏。
 * 4. 如果允许，触发策略回调。
 * 5. 如果用户数据已修改，保存用户数据。
 */
void SelStraBaseCtx::on_tick(const char* stdCode, WTSTickData* newTick, bool bEmitStrategy /* = true */)
{
	_price_map[stdCode] = newTick->price();  // 更新价格映射表中的最新价格

	//先检查是否要信号要触发
	{
		auto it = _sig_map.find(stdCode);  // 在信号映射表中查找该合约的信号
		if (it != _sig_map.end())  // 如果找到信号
		{
			WTSSessionInfo* sInfo = _engine->get_session_info(stdCode, true);  // 获取交易会话信息

			if (sInfo->isInTradingTime(_engine->get_raw_time(), true))  // 如果当前时间在交易时间内
			{
				const SigInfo& sInfo = it->second;  // 获取信号信息
				do_set_position(stdCode, sInfo._volume, sInfo._usertag.c_str(), sInfo._triggered);  // 执行信号：设置持仓
				_sig_map.erase(it);  // 从信号映射表中删除已触发的信号
			}

		}
	}

	update_dyn_profit(stdCode, newTick->price());  // 更新动态盈亏

	if (bEmitStrategy)  // 如果允许触发策略回调
		on_tick_updated(stdCode, newTick);  // 调用Tick更新回调，转发给策略实例

	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 定时调度回调实现
 * @param curDate 当前日期
 * @param curTime 当前时间
 * @param fireTime 触发时间
 * @return bool 返回true表示处理成功
 *
 * 定时调度时被调用，用于执行策略逻辑、保存数据等。
 * 处理流程：
 * 1. 设置调度日期和时间。
 * 2. 标记正在调度中。
 * 3. 保存数据（主要用于保存浮动盈亏）。
 * 4. 调用策略定时调度回调。
 * 5. 检查是否有持仓需要自动清仓（如果信号中没有该持仓）。
 * 6. 统计计算时间和次数。
 * 7. 如果用户数据已修改，保存用户数据。
 * 8. 取消调度标记。
 */
bool SelStraBaseCtx::on_schedule(uint32_t curDate, uint32_t curTime, uint32_t fireTime)
{
	_schedule_date = curDate;  // 设置调度日期
	_schedule_time = curTime;  // 设置调度时间

	_is_in_schedule = true;  // 开始调度，修改标记为true

	//主要用于保存浮动盈亏的
	save_data();  // 保存数据（主要用于保存浮动盈亏）

	TimeUtils::Ticker ticker;  // 创建计时器，用于统计计算时间
	on_strategy_schedule(curDate, fireTime);  // 调用策略定时调度回调
	log_debug("Strategy {} scheduled @ {}", _context_id, curTime);  // 记录调试日志：策略已调度

	wt_hashset<std::string> to_clear;  // 创建待清仓合约集合
	for (auto& v : _pos_map)  // 遍历所有持仓
	{
		const PosInfo& pInfo = v.second;  // 获取持仓信息
		const char* code = v.first.c_str();  // 获取合约代码
		if (_sig_map.find(code) == _sig_map.end() && !decimal::eq(pInfo._volume, 0.0))  // 如果信号中没有该持仓且持仓不为0
		{
			//新的信号中没有该持仓,则要清空
			to_clear.insert(code);  // 将该合约添加到待清仓集合中
		}
	}

	for (const std::string& code : to_clear)  // 遍历待清仓合约集合
	{
		append_signal(code.c_str(), 0, "autoexit");  // 添加清仓信号（目标持仓为0，标签为"autoexit"）
	}

	_emit_times++;  // 增加计算次数
	_total_calc_time += ticker.micro_seconds();  // 累加计算时间（微秒）

	if (_emit_times % 20 == 0)  // 如果计算次数是20的倍数
		log_info("Strategy has been scheduled {} times, totally taking {} us, {:.3f} us each time",
			_emit_times, _total_calc_time, _total_calc_time*1.0 / _emit_times);  // 记录信息日志：统计计算时间和次数

	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}

	_is_in_schedule = false;  // 调度结束，修改标记为false
	return true;  // 返回true，表示处理成功
}

/**
 * @brief 交易日开始回调实现
 * @param uTDate 交易日日期
 *
 * 每个交易日开始时被调用，用于处理冻结持仓解冻等。
 * 处理流程：
 * 1. 遍历所有持仓，检查冻结持仓是否需要解冻（冻结日期早于当前交易日）。
 * 2. 如果用户数据已修改，保存用户数据。
 */
void SelStraBaseCtx::on_session_begin(uint32_t uTDate)
{
	//每个交易日开始，要把冻结持仓置零
	for (auto& it : _pos_map)  // 遍历所有持仓
	{
		const char* stdCode = it.first.c_str();  // 获取合约代码
		PosInfo& pInfo = (PosInfo&)it.second;  // 获取持仓信息引用
		if (pInfo._frozen_date != 0 && pInfo._frozen_date < uTDate && !decimal::eq(pInfo._frozen, 0))  // 如果冻结日期不为0且早于当前交易日，且冻结持仓不为0
		{
			log_debug("{} of {} frozen on {} released on {}", pInfo._frozen, stdCode, pInfo._frozen_date, uTDate);  // 记录调试日志：冻结持仓已解冻

			pInfo._frozen = 0;  // 重置冻结持仓为0
			pInfo._frozen_date = 0;  // 重置冻结日期为0
		}
	}

	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 枚举持仓实现
 * @param cb 回调函数
 *
 * 遍历所有持仓（包括已发信号但未执行的），调用回调函数处理。
 * 处理流程：
 * 1. 收集所有持仓的目标持仓（包括实际持仓和信号持仓）。
 * 2. 遍历目标持仓映射表，调用回调函数处理每个持仓。
 */
void SelStraBaseCtx::enum_position(FuncEnumSelPositionCallBack cb)
{
	wt_hashmap<std::string, double> desPos;  // 创建目标持仓映射表
	for (auto& it : _pos_map)  // 遍历所有持仓
	{
		const char* stdCode = it.first.c_str();  // 获取合约代码
		const PosInfo& pInfo = it.second;  // 获取持仓信息
		desPos[stdCode] = pInfo._volume;  // 将实际持仓添加到目标持仓映射表中
	}

	for (auto sit : _sig_map)  // 遍历所有信号
	{
		const char* stdCode = sit.first.c_str();  // 获取合约代码
		const SigInfo& sInfo = sit.second;  // 获取信号信息
		desPos[stdCode] = sInfo._volume;  // 将信号持仓添加到目标持仓映射表中（会覆盖实际持仓，因为信号优先级更高）
	}

	for (auto v : desPos)  // 遍历目标持仓映射表
	{
		cb(v.first.c_str(), v.second);  // 调用回调函数，传入合约代码和目标持仓
	}
}

/**
 * @brief 交易日结束回调实现
 * @param uTDate 交易日日期
 *
 * 每个交易日结束时被调用，用于保存数据、记录日志等。
 * 处理流程：
 * 1. 计算总累计盈亏和总动态盈亏。
 * 2. 记录持仓日志（如果有持仓）。
 * 3. 记录资金日志。
 * 4. 保存数据。
 * 5. 如果用户数据已修改，保存用户数据。
 */
void SelStraBaseCtx::on_session_end(uint32_t uTDate)
{
	uint32_t curDate = uTDate;  // 设置当前日期为交易日日期

	double total_profit = 0;  // 初始化总累计盈亏
	double total_dynprofit = 0;  // 初始化总动态盈亏

	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)  // 遍历所有持仓
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码
		const PosInfo& pInfo = it->second;  // 获取持仓信息
		total_profit += pInfo._closeprofit;  // 累加累计盈亏
		total_dynprofit += pInfo._dynprofit;  // 累加动态盈亏

		if (decimal::eq(pInfo._volume, 0.0))  // 如果持仓为0
			continue;  // 跳过，继续下一个持仓

		if (_pos_logs)  // 如果持仓日志文件对象存在
			_pos_logs->write_file(fmt::format("{},{},{},{:.2f},{:.2f}\n", curDate, stdCode,
				pInfo._volume, pInfo._closeprofit, pInfo._dynprofit));  // 写入持仓日志：日期,合约代码,持仓数量,累计盈亏,动态盈亏
	}

	if (_fund_logs)  // 如果资金日志文件对象存在
		_fund_logs->write_file(fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curDate,
		_fund_info._total_profit, _fund_info._total_dynprofit,
		_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees));  // 写入资金日志：日期,累计盈亏,动态盈亏,动态权益,手续费

	save_data();  // 保存数据

	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}


//////////////////////////////////////////////////////////////////////////
//策略接口
//////////////////////////////////////////////////////////////////////////
//策略接口
#pragma region "策略接口"
/**
 * @brief 获取价格实现
 * @param stdCode 标准合约代码
 * @return double 返回当前价格
 *
 * 获取指定合约的当前价格。
 * 优先从价格映射表中获取，如果不存在则从引擎获取。
 */
double SelStraBaseCtx::stra_get_price(const char* stdCode)
{
	auto it = _price_map.find(stdCode);  // 在价格映射表中查找合约
	if (it != _price_map.end())  // 如果找到
		return it->second;  // 返回价格映射表中的价格

	if (_engine)  // 如果引擎存在
		return _engine->get_cur_price(stdCode);  // 从引擎获取当前价格

	return 0.0;  // 如果都未找到，返回0.0
}

/**
 * @brief 设置持仓实现
 * @param stdCode 标准合约代码
 * @param qty 目标持仓数量
 * @param userTag 用户标签，默认空字符串
 *
 * 设置指定合约的目标持仓数量，会添加信号到信号映射表中。
 * 处理流程：
 * 1. 检查商品信息是否存在。
 * 2. 检查是否可以做空（如果不能做空，则目标仓位不能为负数）。
 * 3. 检查目标仓位是否与当前仓位一致。
 * 4. 如果是T+1规则，检查目标仓位是否小于冻结仓位。
 * 5. 添加持仓信号。
 */
void SelStraBaseCtx::stra_set_position(const char* stdCode, double qty, const char* userTag /* = "" */)
{
	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取商品信息
	if (commInfo == NULL)  // 如果商品信息不存在
	{
		log_error("Cannot find corresponding commodity info of {}", stdCode);  // 记录错误日志：找不到商品信息
		return;  // 直接返回
	}

	//如果不能做空，则目标仓位不能设置负数
	if (!commInfo->canShort() && decimal::lt(qty, 0))  // 如果不能做空且目标仓位为负数
	{
		log_error("Cannot short on {}", stdCode);  // 记录错误日志：不能做空
		return;  // 直接返回
	}

	double total = stra_get_position(stdCode, false);  // 获取当前持仓（包含冻结持仓）
	//如果目标仓位和当前仓位是一致的，直接退出
	if (decimal::eq(total, qty))  // 如果目标仓位与当前仓位一致
		return;  // 直接返回，无需操作

	if (commInfo->isT1())  // 如果是T+1规则
	{
		double valid = stra_get_position(stdCode, true);  // 获取有效持仓（排除冻结持仓）
		double frozen = total - valid;  // 计算冻结持仓数量
		//如果是T+1规则，则目标仓位不能小于冻结仓位
		if (decimal::lt(qty, frozen))  // 如果目标仓位小于冻结仓位
		{
			log_error("New position of {} cannot be set to {} due to {} being frozen", stdCode, qty, frozen);  // 记录错误日志：目标仓位不能小于冻结仓位
			return;  // 直接返回
		}
	}

	append_signal(stdCode, qty, userTag);  // 添加持仓信号
}

/**
 * @brief 添加信号实现
 * @param stdCode 标准合约代码
 * @param qty 目标持仓数量
 * @param userTag 用户标签，默认空字符串
 *
 * 添加持仓信号到信号映射表中，信号会在合适的时机执行。
 * 处理流程：
 * 1. 获取当前价格。
 * 2. 创建或更新信号信息。
 * 3. 记录信号日志。
 * 4. 保存数据。
 */
void SelStraBaseCtx::append_signal(const char* stdCode, double qty, const char* userTag /* = "" */)
{
	double curPx = _price_map[stdCode];  // 从价格映射表获取当前价格

	SigInfo& sInfo = _sig_map[stdCode];  // 获取或创建信号信息
	sInfo._volume = qty;  // 设置目标持仓数量
	sInfo._sigprice = curPx;  // 设置信号价格
	sInfo._usertag = userTag;  // 设置用户标签
	sInfo._gentime = (uint64_t)_engine->get_date() * 1000000000 + (uint64_t)_engine->get_raw_time() * 100000 + _engine->get_secs();  // 构造信号生成时间戳（纳秒级：日期*10^9 + 分钟*10^5 + 秒）
	sInfo._triggered = !_is_in_schedule;  // 设置触发标志：如果不在调度中，则标记为已触发

	log_signal(stdCode, qty, curPx, sInfo._gentime, userTag);  // 记录信号日志

	save_data();  // 保存数据
}

/**
 * @brief 设置持仓实现
 * @param stdCode 标准合约代码
 * @param qty 目标持仓数量
 * @param userTag 用户标签，默认空字符串
 * @param bTriggered 是否已触发，默认false
 *
 * 根据目标持仓数量调整实际持仓，并记录交易和平仓日志。
 * 内部会处理开仓、平仓、反手等逻辑，并计算盈亏和手续费。
 *
 * 处理逻辑：
 * 1. 获取当前价格、时间和交易日。
 * 2. 如果当前持仓等于目标持仓，则直接返回。
 * 3. 计算持仓差异（diff = 目标持仓 - 当前持仓）。
 * 4. 如果当前持仓与目标持仓方向一致（同为正或同为负），则增加持仓明细：
 *    - 更新总持仓数量。
 *    - 如果T+1规则，更新冻结持仓。
 *    - 在回测模式下，根据滑点设置调整成交价格。
 *    - 创建新的持仓明细并添加到明细列表。
 *    - 计算并累加手续费。
 *    - 记录交易日志（开仓）。
 * 5. 如果当前持仓与目标持仓方向不一致，则需要平仓：
 *    - 遍历持仓明细，按先进先出（FIFO）原则平仓。
 *    - 计算平仓盈亏、手续费，并更新累计盈亏。
 *    - 记录交易日志和平仓日志。
 *    - 清理已平仓完的明细。
 *    - 如果平仓后还有剩余，则反手开仓。
 * 6. 保存数据。
 * 7. 通知引擎持仓变化。
 */
void SelStraBaseCtx::do_set_position(const char* stdCode, double qty, const char* userTag /* = "" */, bool bTriggered /* = false */)
{
	PosInfo& pInfo = _pos_map[stdCode];  // 获取或创建持仓信息
	double curPx = _price_map[stdCode];  // 从价格映射表获取当前价格
	uint64_t curTm = (uint64_t)_engine->get_date() * 10000 + _engine->get_min_time();  // 构造当前时间戳（日期*10000 + 分钟）
	uint32_t curTDate = _engine->get_trading_date();  // 获取当前交易日

	if (decimal::eq(pInfo._volume, qty))  // 如果当前持仓等于目标持仓
		return;  // 直接返回，无需操作

	double diff = qty - pInfo._volume;  // 计算持仓差异：目标持仓 - 当前持仓

	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取商品信息
	if (commInfo == NULL)  // 如果商品信息不存在
		return;  // 直接返回

	//成交价
	double trdPx = curPx;  // 初始化实际成交价格为当前价格

	bool isBuy = decimal::gt(diff, 0.0);  // 判断是否为买入方向（持仓增加）
	if (decimal::gt(pInfo._volume*diff, 0))  // 如果当前持仓和目标持仓方向一致（同为正或同为负），则增加持仓明细，增加数量即可
	{
		pInfo._volume = qty;  // 更新总持仓数量
		//如果T+1，则冻结仓位要增加
		if (commInfo->isT1())  // 如果是T+1规则
		{
			//ASSERT(diff>0);  // 注释掉的断言：差异应该大于0
			pInfo._frozen += diff;  // 增加冻结持仓数量
			pInfo._frozen_date = curTDate;  // 设置冻结日期为当前交易日
			log_debug("{} frozen position updated to {}", stdCode, pInfo._frozen);  // 记录调试日志：冻结持仓已更新
		}

		if (_slippage != 0)  // 如果设置了滑点（回测模式）
		{
			trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 调整成交价格：买入时加滑点，卖出时减滑点
		}

		DetailInfo dInfo;  // 创建新的持仓明细
		dInfo._long = decimal::gt(qty, 0);  // 设置方向：持仓为正则为做多，持仓为负则为做空
		dInfo._price = trdPx;  // 设置开仓价格
		dInfo._max_price = trdPx;  // 初始化最高价为开仓价格
		dInfo._min_price = trdPx;  // 初始化最低价为开仓价格
		dInfo._volume = abs(diff);  // 设置开仓数量（持仓差异的绝对值）
		dInfo._opentime = curTm;  // 设置开仓时间
		dInfo._opentdate = curTDate;  // 设置开仓日期
		wt_strcpy(dInfo._opentag, userTag);  // 复制用户标签
		pInfo._details.push_back(dInfo);  // 将持仓明细添加到明细列表
		pInfo._last_entertime = curTm;  // 更新最后开仓时间

		double fee = commInfo->calcFee(trdPx, abs(qty), 0);  // 计算手续费（开仓：0）
		_fund_info._total_fees += fee;  // 累加总手续费
		//_engine->mutate_fund(fee, FFT_Fee);  // 注释掉的代码：更新引擎资金（已废弃）
		log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(qty), userTag, fee);  // 记录交易日志（开仓）
	}
	else  // 如果持仓方向和目标仓位方向不一致，需要平仓
	{
		double left = abs(diff);  // 剩余需要平仓的数量（持仓差异的绝对值）

		if (_slippage != 0)  // 如果设置了滑点（回测模式）
			trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 调整成交价格：买入时加滑点，卖出时减滑点

		pInfo._volume = qty;  // 更新总持仓数量
		if (decimal::eq(pInfo._volume, 0))  // 如果总持仓数量为0
			pInfo._dynprofit = 0;  // 重置动态盈亏为0
		uint32_t count = 0;  // 已平仓完的明细数量计数器
		for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历所有持仓明细（按先进先出原则）
		{
			DetailInfo& dInfo = *it;  // 获取持仓明细引用
			double maxQty = min(dInfo._volume, left);  // 本次平仓数量：取明细持仓数量和剩余平仓数量的最小值
			//if (maxQty == 0)  // 注释掉的代码：使用decimal比较
			if (decimal::eq(maxQty, 0))  // 如果平仓数量为0
				continue;  // 跳过，继续下一个明细

			dInfo._volume -= maxQty;  // 减少持仓明细的数量
			left -= maxQty;  // 减少剩余需要平仓的数量

			if (decimal::eq(dInfo._volume, 0))  // 如果持仓明细数量为0
				count++;  // 增加已平仓完的明细数量

			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算平仓盈亏：（平仓价格 - 开仓价格）* 数量 * 合约乘数
			if (!dInfo._long)  // 如果是做空方向
				profit *= -1;  // 盈亏取反
			pInfo._closeprofit += profit;  // 累加累计平仓盈亏
			pInfo._dynprofit = pInfo._dynprofit*dInfo._volume / (dInfo._volume + maxQty);  // 浮盈也要做等比缩放：动态盈亏 = 原动态盈亏 * (剩余数量 / 原总数量)
			pInfo._last_exittime = curTm;  // 更新最后平仓时间
			_fund_info._total_profit += profit;  // 累加总平仓盈亏

			double fee = commInfo->calcFee(trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费（今仓：2，昨仓：1）
			_fund_info._total_fees += fee;  // 累加总手续费
			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, trdPx, maxQty, userTag, fee);  // 交易日志（平仓）
			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, pInfo._closeprofit, dInfo._opentag, userTag);  // 平仓日志

			//if (left == 0)  // 注释掉的代码：使用decimal比较
			if (decimal::eq(left, 0))  // 如果剩余需要平仓的数量为0
				break;  // 跳出循环
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)  // 循环清理已平仓完的明细
		{
			auto it = pInfo._details.begin();  // 获取明细列表的起始迭代器
			pInfo._details.erase(it);  // 删除第一个明细
			count--;  // 减少计数器
		}

		//最后, 如果还有剩余的, 则需要反手了
		//if (left > 0)  // 注释掉的代码：使用decimal比较
		if (decimal::gt(left, 0))  // 如果还有剩余需要平仓的数量（说明需要反手开仓）
		{
			left = left * qty / abs(qty);  // 转换剩余数量：如果目标持仓为正则为正，如果目标持仓为负则为负

			//如果T+1，则冻结仓位要增加
			if (commInfo->isT1())  // 如果是T+1规则
			{
				//ASSERT(diff>0);  // 注释掉的断言：差异应该大于0
				pInfo._frozen += diff;  // 增加冻结持仓数量
				pInfo._frozen_date = curTDate;  // 设置冻结日期为当前交易日
				log_debug("{} frozen position updated to {}", stdCode, pInfo._frozen);  // 记录调试日志：冻结持仓已更新
			}

			DetailInfo dInfo;  // 创建新的持仓明细（反手开仓）
			dInfo._long = decimal::gt(qty, 0);  // 设置方向：持仓为正则为做多，持仓为负则为做空
			dInfo._price = trdPx;  // 设置开仓价格
			dInfo._max_price = trdPx;  // 初始化最高价为开仓价格
			dInfo._min_price = trdPx;  // 初始化最低价为开仓价格
			dInfo._volume = abs(left);  // 设置开仓数量（剩余数量的绝对值）
			dInfo._opentime = curTm;  // 设置开仓时间
			dInfo._opentdate = curTDate;  // 设置开仓日期
			wt_strcpy(dInfo._opentag, userTag);  // 复制用户标签
			pInfo._details.push_back(dInfo);  // 将持仓明细添加到明细列表
			pInfo._last_entertime = curTm;  // 更新最后开仓时间

			//这里还需要写一笔成交记录
			double fee = commInfo->calcFee(trdPx, abs(qty), 0);  // 计算手续费（开仓：0）
			_fund_info._total_fees += fee;  // 累加总手续费
			//_engine->mutate_fund(fee, FFT_Fee);  // 注释掉的代码：更新引擎资金（已废弃）
			log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(left), userTag, fee);  // 交易日志（反手开仓）
		}
	}

	//存储数据
	save_data();  // 保存数据

	_engine->handle_pos_change(_name.c_str(), stdCode, diff);  // 通知引擎持仓变化，传入策略名称、合约代码和持仓变化量
}

/**
 * @brief 获取K线数据实现
 * @param stdCode 标准合约代码
 * @param period 周期字符串（如"m1", "d"）
 * @param count 获取的K线数量
 * @return WTSKlineSlice* 返回K线切片指针
 *
 * 获取指定合约的K线数据。
 * 处理流程：
 * 1. 构造K线标签键（合约代码#周期）。
 * 2. 解析周期字符串（提取基础周期和倍数）。
 * 3. 计算结束时间（根据周期类型）。
 * 4. 从引擎获取K线数据。
 * 5. 标记K线为未收盘状态。
 * 6. 更新价格映射表中的最新价格（使用最后一根K线的收盘价）。
 */
WTSKlineSlice* SelStraBaseCtx::stra_get_bars(const char* stdCode, const char* period, uint32_t count)
{
	thread_local static char key[64] = { 0 };  // 线程局部静态变量：K线标签键
	fmtutil::format_to(key, "{}#{}", stdCode, period);  // 格式化K线标签键（合约代码#周期）

	thread_local static char basePeriod[2] = { 0 };  // 线程局部静态变量：基础周期
	basePeriod[0] = period[0];  // 提取基础周期字符（如"m"、"d"）
	uint32_t times = 1;  // 初始化周期倍数为1
	if (strlen(period) > 1)  // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);  // 解析周期倍数（如"m5"中的"5"）
	
	uint64_t etime = 0;  // 初始化结束时间
	if (period[0] == 'd')  // 如果周期是日线
	{
		WTSSessionInfo* sInfo = _engine->get_session_info(stdCode, true);  // 获取交易会话信息
		etime = (uint64_t)_schedule_date * 10000 + sInfo->getCloseTime();  // 构造结束时间：调度日期*10000 + 收盘时间
	}
	else  // 如果周期不是日线
		etime = (uint64_t)_schedule_date * 10000 + _schedule_time;  // 构造结束时间：调度日期*10000 + 调度时间

	WTSKlineSlice* kline = _engine->get_kline_slice(_context_id, stdCode, basePeriod, count, times, etime);  // 从引擎获取K线数据切片

	KlineTag& tag = _kline_tags[key];  // 获取或创建K线标签
	tag._closed = false;  // 标记K线为未收盘状态

	if (kline)  // 如果K线数据存在
	{
		double lastClose = kline->at(-1)->close;  // 获取最后一根K线的收盘价
		_price_map[stdCode] = lastClose;  // 更新价格映射表中的最新价格
	}

	return kline;  // 返回K线数据切片
}

/**
 * @brief 获取Tick数据切片实现
 * @param stdCode 标准合约代码
 * @param count 获取的Tick数量
 * @return WTSTickSlice* 返回Tick切片指针
 *
 * 从引擎获取指定合约的Tick数据切片。
 */
WTSTickSlice* SelStraBaseCtx::stra_get_ticks(const char* stdCode, uint32_t count)
{
	return _engine->get_tick_slice(_context_id, stdCode, count);  // 从引擎获取Tick数据切片
}

/**
 * @brief 获取最新Tick数据实现
 * @param stdCode 标准合约代码
 * @return WTSTickData* 返回最新Tick数据指针
 *
 * 从引擎获取指定合约的最新Tick数据。
 */
WTSTickData* SelStraBaseCtx::stra_get_last_tick(const char* stdCode)
{
	return _engine->get_last_tick(_context_id, stdCode);  // 从引擎获取最新Tick数据
}

/**
 * @brief 订阅Tick数据实现
 * @param stdCode 标准合约代码
 *
 * 订阅指定合约的Tick数据。
 * 处理流程：
 * 1. 将合约代码添加到本地订阅集合中。
 * 2. 通知引擎订阅该合约的Tick数据。
 * 3. 记录信息日志。
 *
 * 注意：主动订阅tick会在本地记录，tick数据回调时会先检查一下。
 */
void SelStraBaseCtx::stra_sub_ticks(const char* stdCode)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(stdCode);  // 将合约代码添加到本地订阅集合中

	_engine->sub_tick(_context_id, stdCode);  // 通知引擎订阅该合约的Tick数据
	log_info("Market data subscribed: {}", stdCode);  // 记录信息日志：市场数据已订阅
}

/**
 * @brief 获取商品信息实现
 * @param stdCode 标准合约代码
 * @return WTSCommodityInfo* 返回商品信息指针
 *
 * 从引擎获取指定合约的商品信息。
 */
WTSCommodityInfo* SelStraBaseCtx::stra_get_comminfo(const char* stdCode)
{
	return _engine->get_commodity_info(stdCode);  // 从引擎获取商品信息
}

/**
 * @brief 获取原始合约代码实现
 * @param stdCode 标准合约代码
 * @return std::string 返回原始合约代码
 *
 * 从引擎获取指定标准合约代码对应的原始合约代码。
 */
std::string SelStraBaseCtx::stra_get_rawcode(const char* stdCode)
{
	return _engine->get_rawcode(stdCode);  // 从引擎获取原始合约代码
}

/**
 * @brief 获取交易会话信息实现
 * @param stdCode 标准合约代码
 * @return WTSSessionInfo* 返回交易会话信息指针
 *
 * 从引擎获取指定合约的交易会话信息。
 */
WTSSessionInfo* SelStraBaseCtx::stra_get_sessinfo(const char* stdCode)
{
	return _engine->get_session_info(stdCode, true);  // 从引擎获取交易会话信息
}

/**
 * @brief 获取日线价格实现
 * @param stdCode 标准合约代码
 * @param flag 价格类型标志，默认0
 * @return double 返回日线价格
 *
 * 从引擎获取指定合约的日线价格。
 * flag参数含义：
 * - 0: 收盘价
 * - 1: 开盘价
 * - 2: 最高价
 * - 3: 最低价
 */
double SelStraBaseCtx::stra_get_day_price(const char* stdCode, int flag /* = 0 */)
{
	if (_engine)  // 如果引擎存在
		return _engine->get_day_price(stdCode, flag);  // 从引擎获取日线价格

	return 0.0;  // 如果引擎不存在，返回0.0
}

/**
 * @brief 获取交易日实现
 * @return uint32_t 返回交易日日期
 *
 * 从引擎获取当前交易日日期。
 */
uint32_t SelStraBaseCtx::stra_get_tdate()
{
	return _engine->get_trading_date();  // 从引擎获取交易日日期
}

/**
 * @brief 获取当前日期实现
 * @return uint32_t 返回当前日期
 *
 * 获取当前日期。
 * 如果正在调度中，返回调度日期；否则返回引擎日期。
 */
uint32_t SelStraBaseCtx::stra_get_date()
{
	return _is_in_schedule ? _schedule_date : _engine->get_date();  // 如果正在调度中，返回调度日期；否则返回引擎日期
}

/**
 * @brief 获取当前时间实现
 * @return uint32_t 返回当前时间（分钟）
 *
 * 获取当前时间（分钟）。
 * 如果正在调度中，返回调度时间；否则返回引擎时间。
 */
uint32_t SelStraBaseCtx::stra_get_time()
{
	return _is_in_schedule ? _schedule_time : _engine->get_min_time();  // 如果正在调度中，返回调度时间；否则返回引擎时间（分钟）
}

/**
 * @brief 获取资金数据实现
 * @param flag 数据标志
 * @return double 返回资金数据
 *
 * 根据标志获取不同的资金数据。
 * flag参数含义：
 * - 0: 动态权益（累计盈亏 - 手续费 + 动态盈亏）
 * - 1: 累计盈亏
 * - 2: 动态盈亏
 * - 3: 手续费
 * - 其他: 0.0
 */
double SelStraBaseCtx::stra_get_fund_data(int flag)
{
	switch (flag)  // 根据标志返回不同的资金数据
	{
	case 0:  // 动态权益
		return _fund_info._total_profit - _fund_info._total_fees + _fund_info._total_dynprofit;
	case 1:  // 累计盈亏
		return _fund_info._total_profit;
	case 2:  // 动态盈亏
		return _fund_info._total_dynprofit;
	case 3:  // 手续费
		return _fund_info._total_fees;
	default:  // 其他
		return 0.0;
	}
}

/**
 * @brief 记录信息日志实现
 * @param message 日志消息
 *
 * 使用策略名称记录信息级别的日志。
 */
void SelStraBaseCtx::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 记录信息级别日志
}

/**
 * @brief 记录调试日志实现
 * @param message 日志消息
 *
 * 使用策略名称记录调试级别的日志。
 */
void SelStraBaseCtx::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 记录调试级别日志
}

/**
 * @brief 记录警告日志实现
 * @param message 日志消息
 *
 * 使用策略名称记录警告级别的日志。
 */
void SelStraBaseCtx::stra_log_warn(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_WARN, message);  // 记录警告级别日志
}

/**
 * @brief 记录错误日志实现
 * @param message 日志消息
 *
 * 使用策略名称记录错误级别的日志。
 */
void SelStraBaseCtx::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 记录错误级别日志
}

/**
 * @brief 加载用户数据实现
 * @param key 键
 * @param defVal 默认值，默认空字符串
 * @return const char* 返回用户数据值
 *
 * 从用户数据映射表中加载指定键的值。
 * 如果找不到，返回默认值。
 */
const char* SelStraBaseCtx::stra_load_user_data(const char* key, const char* defVal /*= ""*/)
{
	auto it = _user_datas.find(key);  // 在用户数据映射表中查找键
	if (it != _user_datas.end())  // 如果找到
		return it->second.c_str();  // 返回对应的值

	return defVal;  // 如果找不到，返回默认值
}

/**
 * @brief 保存用户数据实现
 * @param key 键
 * @param val 值
 *
 * 将用户数据保存到用户数据映射表中，并标记为已修改。
 * 用户数据会在适当的时机自动保存到文件。
 */
void SelStraBaseCtx::stra_save_user_data(const char* key, const char* val)
{
	_user_datas[key] = val;  // 将键值对保存到用户数据映射表中
	_ud_modified = true;  // 标记用户数据已修改
}

/**
 * @brief 获取首次开仓时间实现
 * @param stdCode 标准合约代码
 * @return uint64_t 返回首次开仓时间
 *
 * 获取指定合约的首次开仓时间。
 * 返回持仓明细列表中第一笔明细的开仓时间。
 */
uint64_t SelStraBaseCtx::stra_get_first_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return 0;  // 返回0

	return pInfo._details[0]._opentime;  // 返回第一笔明细的开仓时间
}

/**
 * @brief 获取首次开仓标签实现
 * @param stdCode 标准合约代码
 * @return const char* 返回首次开仓标签
 *
 * 获取指定合约的首次开仓标签。
 * 返回持仓明细列表中第一笔明细的开仓标签。
 */
const char* SelStraBaseCtx::stra_get_last_entertag(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return "";  // 返回空字符串

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return "";  // 返回空字符串

	return pInfo._details[0]._opentag;  // 返回第一笔明细的开仓标签（注意：函数名是get_last_entertag，但实际返回的是第一笔明细的标签）
}


/**
 * @brief 获取最后平仓时间实现
 * @param stdCode 标准合约代码
 * @return uint64_t 返回最后平仓时间
 *
 * 获取指定合约的最后平仓时间。
 * 返回持仓信息中的最后平仓时间。
 */
uint64_t SelStraBaseCtx::stra_get_last_exittime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._last_exittime;  // 返回最后平仓时间
}

/**
 * @brief 获取最后开仓时间实现
 * @param stdCode 标准合约代码
 * @return uint64_t 返回最后开仓时间
 *
 * 获取指定合约的最后开仓时间。
 * 返回持仓明细列表中最后一笔明细的开仓时间。
 */
uint64_t SelStraBaseCtx::stra_get_last_entertime(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return 0;  // 返回0

	return pInfo._details[pInfo._details.size() - 1]._opentime;  // 返回最后一笔明细的开仓时间
}

/**
 * @brief 获取最后开仓价格实现
 * @param stdCode 标准合约代码
 * @return double 返回最后开仓价格
 *
 * 获取指定合约的最后开仓价格。
 * 返回持仓明细列表中最后一笔明细的开仓价格。
 */
double SelStraBaseCtx::stra_get_last_enterprice(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._details.empty())  // 如果持仓明细为空
		return 0;  // 返回0

	return pInfo._details[pInfo._details.size() - 1]._price;  // 返回最后一笔明细的开仓价格
}

/**
 * @brief 获取持仓实现
 * @param stdCode 标准合约代码
 * @param bOnlyValid 是否只返回有效持仓（排除冻结持仓），默认false
 * @param userTag 用户标签，默认空字符串
 * @return double 返回持仓数量
 *
 * 获取指定合约的持仓数量。
 * 处理逻辑：
 * 1. 如果用户标签为空：
 *    - 如果只返回有效持仓，则返回总持仓减去冻结持仓。
 *    - 否则返回总持仓。
 * 2. 如果用户标签不为空：
 *    - 遍历持仓明细，查找匹配用户标签的明细。
 *    - 返回匹配明细的持仓数量。
 */
double SelStraBaseCtx::stra_get_position(const char* stdCode, bool bOnlyValid /* = false */, const char* userTag /* = "" */)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (strlen(userTag) == 0)  // 如果用户标签为空
	{
		//只有userTag为空的时候时候，才会用bOnlyValid
		if (bOnlyValid)  // 如果只返回有效持仓
		{
			//这里理论上，只有多头才会进到这里
			//其他地方要保证，空头持仓的话，_frozen要为0
			return pInfo._volume - pInfo._frozen;  // 返回有效持仓（总持仓减去冻结持仓）
		}
		else  // 如果返回总持仓
			return pInfo._volume;  // 返回总持仓
	}

	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历所有持仓明细
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细引用
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果用户标签不匹配
			continue;  // 跳过，继续下一个明细

		return dInfo._volume;  // 返回匹配明细的持仓数量
	}

	return 0;  // 如果找不到匹配的明细，返回0
}

/**
 * @brief 获取持仓平均价格实现
 * @param stdCode 标准合约代码
 * @return double 返回持仓平均价格
 *
 * 计算并返回指定合约的持仓平均价格。
 * 计算公式：平均价格 = 所有明细的（价格 * 数量）之和 / 总持仓数量
 */
double SelStraBaseCtx::stra_get_position_avgpx(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._volume == 0)  // 如果持仓为0
		return 0.0;  // 返回0.0

	double amount = 0.0;  // 初始化总金额
	for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历所有持仓明细
	{
		const DetailInfo& dInfo = *dit;  // 获取持仓明细引用
		amount += dInfo._price*dInfo._volume;  // 累加价格乘以数量的乘积
	}

	return amount / pInfo._volume;  // 返回平均价格：总金额 / 总持仓数量
}

/**
 * @brief 获取持仓盈亏实现
 * @param stdCode 标准合约代码
 * @return double 返回持仓盈亏
 *
 * 获取指定合约的持仓动态盈亏。
 */
double SelStraBaseCtx::stra_get_position_profit(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._dynprofit;  // 返回持仓动态盈亏
}

/**
 * @brief 获取明细开仓时间实现
 * @param stdCode 标准合约代码
 * @param userTag 用户标签
 * @return uint64_t 返回明细开仓时间
 *
 * 根据用户标签查找持仓明细，返回匹配明细的开仓时间。
 * 如果找不到匹配的明细，返回0。
 */
uint64_t SelStraBaseCtx::stra_get_detail_entertime(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历所有持仓明细
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细引用
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果用户标签不匹配
			continue;  // 跳过，继续下一个明细

		return dInfo._opentime;  // 返回匹配明细的开仓时间
	}

	return 0;  // 如果找不到匹配的明细，返回0
}

/**
 * @brief 获取明细成本价实现
 * @param stdCode 标准合约代码
 * @param userTag 用户标签
 * @return double 返回明细成本价
 *
 * 根据用户标签查找持仓明细，返回匹配明细的开仓价格（成本价）。
 * 如果找不到匹配的明细，返回0.0。
 */
double SelStraBaseCtx::stra_get_detail_cost(const char* stdCode, const char* userTag)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历所有持仓明细
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细引用
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果用户标签不匹配
			continue;  // 跳过，继续下一个明细

		return dInfo._price;  // 返回匹配明细的开仓价格（成本价）
	}

	return 0.0;  // 如果找不到匹配的明细，返回0.0
}

/**
 * @brief 获取明细盈亏实现
 * @param stdCode 标准合约代码
 * @param userTag 用户标签
 * @param flag 盈亏类型标志，默认0
 * @return double 返回明细盈亏或价格
 *
 * 根据用户标签查找持仓明细，返回匹配明细的盈亏或价格信息。
 * flag参数含义：
 * - 0: 当前盈亏
 * - 1: 最大盈利
 * - -1: 最大亏损
 * - 2: 最高价
 * - -2: 最低价
 * - 其他: 0.0
 */
double SelStraBaseCtx::stra_get_detail_profit(const char* stdCode, const char* userTag, int flag /* = 0 */)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找合约
	if (it == _pos_map.end())  // 如果找不到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	for (auto it = pInfo._details.begin(); it != pInfo._details.end(); it++)  // 遍历所有持仓明细
	{
		const DetailInfo& dInfo = (*it);  // 获取持仓明细引用
		if (strcmp(dInfo._opentag, userTag) != 0)  // 如果用户标签不匹配
			continue;  // 跳过，继续下一个明细

		switch (flag)  // 根据标志返回不同的盈亏或价格信息
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

	return 0.0;  // 如果找不到匹配的明细，返回0.0
}

#pragma endregion 