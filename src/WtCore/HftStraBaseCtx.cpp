/*!
 * \file HftStraBaseCtx.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 高频交易策略基础上下文实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了高频交易策略基础上下文的所有功能，包括：
 * 1. 策略生命周期管理：初始化、各种市场数据回调、交易日开始/结束
 * 2. 交易执行接口：买入、卖出、开多、开空、平多、平空、撤单等完整交易功能
 * 3. 市场数据访问：K线、Tick、委托明细、委托队列、逐笔成交等数据获取
 * 4. 仓位管理：持仓查询、持仓盈亏、平均持仓价格、未成交数量等
 * 5. 持仓明细管理：详细的持仓明细管理，支持开仓、平仓、盈亏计算、滑点模拟
 * 6. 数据订阅管理：订阅Tick、委托明细、委托队列、逐笔成交数据
 * 7. 日志记录：支持info、debug、warn、error级别的日志
 * 8. 用户数据存储：策略自定义数据的保存和加载（JSON格式）
 * 9. 交易通知处理：成交、订单、通道状态、持仓变化等通知处理
 * 10. 数据托管模式：自动记录交易日志、平仓日志、资金日志、信号日志
 * 
 * 关键实现细节：
 * - 使用原子操作生成唯一的上下文ID
 * - 支持合约代码映射（标准代码到实际代码的转换）
 * - 支持滑点模拟（用于回测）
 * - 详细的持仓明细管理，支持多笔开仓和平仓的精确匹配
 * - 使用循环缓冲区管理订单标签，提高性能
 * - 使用rapidjson进行用户数据的JSON格式存储
 */
#include "HftStraBaseCtx.h"  // 包含高频交易策略基础上下文头文件
#include "WtHftEngine.h"  // 包含高频交易引擎头文件
#include "TraderAdapter.h"  // 包含交易适配器头文件
#include "WtHelper.h"  // 包含WonderTrader辅助工具类

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息头文件
#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件

#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具类
#include "../Share/decimal.h"  // 包含高精度小数计算工具

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具
#include "../WTSTools/WTSHotMgr.h"  // 包含热点合约管理器头文件

#include <rapidjson/document.h>  // 包含rapidjson文档类
#include <rapidjson/prettywriter.h>  // 包含rapidjson格式化写入器
namespace rj = rapidjson;  // 定义rapidjson命名空间别名

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 生成高频交易上下文ID
 * @return uint32_t 返回唯一的下文ID
 * 
 * 使用原子操作生成唯一的上下文ID，从6000开始递增。
 * 使用静态原子变量确保线程安全。
 */
inline uint32_t makeHftCtxId()  // 内联函数：生成高频交易上下文ID
{
	static std::atomic<uint32_t> _auto_context_id{ 6000 };  // 静态原子变量，初始值为6000
	return _auto_context_id.fetch_add(1);  // 原子递增操作，返回递增前的值
}

/**
 * @brief 构造函数实现
 * @param engine 高频交易引擎指针
 * @param name 策略名称
 * @param bAgent 是否启用数据托管模式
 * @param slippage 滑点点数（用于回测）
 * 
 * 初始化高频交易策略基础上下文对象。
 * 使用初始化列表初始化基类和成员变量，然后生成唯一的上下文ID。
 */
HftStraBaseCtx::HftStraBaseCtx(WtHftEngine* engine, const char* name, bool bAgent, int32_t slippage)
	: IHftStraCtx(name)  // 初始化基类，传入策略名称
	, _engine(engine)  // 初始化引擎指针
	, _data_agent(bAgent)  // 初始化数据托管标志
	, _slippage(slippage)  // 初始化滑点点数
{
	_context_id = makeHftCtxId();  // 生成唯一的上下文ID
}

/**
 * @brief 析构函数实现
 * 
 * 清理策略上下文对象。
 * 由于使用智能指针管理资源，大部分资源会自动释放。
 */
HftStraBaseCtx::~HftStraBaseCtx()
{  // 析构函数实现体
}

/**
 * @brief 获取策略上下文ID实现
 * @return uint32_t 返回策略上下文的唯一标识ID
 * 
 * 返回策略上下文的唯一标识ID，用于系统内部识别。
 */
uint32_t HftStraBaseCtx::id()
{
	return _context_id;  // 返回上下文ID
}

/**
 * @brief 设置交易适配器实现
 * @param trader 交易适配器指针
 * 
 * 设置策略使用的交易适配器，用于执行交易操作。
 */
void HftStraBaseCtx::setTrader(TraderAdapter* trader)
{
	_trader = trader;  // 保存交易适配器指针
}

/**
 * @brief 初始化输出文件实现
 * 
 * 在数据托管模式下，初始化各种日志输出文件：
 * - 交易日志（trades.csv）：记录所有成交记录
 * - 平仓日志（closes.csv）：记录所有平仓记录
 * - 资金日志（funds.csv）：记录每日资金结算数据
 * - 信号日志（signals.csv）：记录交易信号数据
 * 
 * 如果文件已存在，则追加写入；如果文件不存在，则创建新文件并写入表头。
 */
void HftStraBaseCtx::init_outputs()
{
	if (!_data_agent)  // 如果未启用数据托管模式
		return;  // 直接返回，不初始化输出文件

	std::string folder = WtHelper::getOutputDir();  // 获取输出目录路径
	folder += _name;  // 追加策略名称
	folder += "//";  // 追加目录分隔符
	BoostFile::create_directories(folder.c_str());  // 创建输出目录（如果不存在）

	std::string filename = folder + "trades.csv";  // 构造交易日志文件路径
	_trade_logs.reset(new BoostFile());  // 创建交易日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_trade_logs->create_or_open_file(filename.c_str());  // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			_trade_logs->write_file("code,time,direct,action,price,qty,tag,fee\n");  // 写入CSV表头
		}
		else  // 如果文件已存在
		{
			_trade_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "closes.csv";  // 构造平仓日志文件路径
	_close_logs.reset(new BoostFile());  // 创建平仓日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_close_logs->create_or_open_file(filename.c_str());  // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			_close_logs->write_file("code,direct,opentime,openprice,closetime,closeprice,qty,profit,totalprofit,entertag,exittag\n");  // 写入CSV表头
		}
		else  // 如果文件已存在
		{
			_close_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "funds.csv";  // 构造资金日志文件路径
	_fund_logs.reset(new BoostFile());  // 创建资金日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_fund_logs->create_or_open_file(filename.c_str());  // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			_fund_logs->write_file("date,closeprofit,positionprofit,dynbalance,fee\n");  // 写入CSV表头
		}
		else  // 如果文件已存在
		{
			_fund_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}

	filename = folder + "signals.csv";  // 构造信号日志文件路径
	_sig_logs.reset(new BoostFile());  // 创建信号日志文件对象
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件
		_sig_logs->create_or_open_file(filename.c_str());  // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			_sig_logs->write_file("code,target,sigprice,gentime,usertag\n");  // 写入CSV表头
		}
		else  // 如果文件已存在
		{
			_sig_logs->seek_to_end();  // 定位到文件末尾，准备追加写入
		}
	}
}

/**
 * @brief 策略初始化回调实现
 * 
 * 策略初始化时被调用，执行初始化准备工作：
 * 1. 初始化输出文件（在数据托管模式下）
 * 2. 加载用户数据
 */
void HftStraBaseCtx::on_init()
{
	init_outputs();  // 初始化输出文件

	load_userdata();  // 加载用户数据
}

/**
 * @brief Tick数据回调实现
 * @param stdCode 标准合约代码
 * @param newTick 新的Tick数据指针
 * 
 * 当收到新的Tick数据时被调用，处理实时市场数据。
 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
 */
void HftStraBaseCtx::on_tick(const char* stdCode, WTSTickData* newTick)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 委托队列数据回调实现
 * @param stdCode 标准合约代码
 * @param newOrdQue 新的委托队列数据指针
 * 
 * 当收到新的委托队列数据时被调用，处理委托队列变化。
 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
 */
void HftStraBaseCtx::on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 委托明细数据回调实现
 * @param stdCode 标准合约代码
 * @param newOrdDtl 新的委托明细数据指针
 * 
 * 当收到新的委托明细数据时被调用，处理委托明细变化。
 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
 */
void HftStraBaseCtx::on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 逐笔成交数据回调实现
 * @param stdCode 标准合约代码
 * @param newTrans 新的逐笔成交数据指针
 * 
 * 当收到新的逐笔成交数据时被调用，处理逐笔成交变化。
 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
 */
void HftStraBaseCtx::on_transaction(const char* stdCode, WTSTransData* newTrans)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief K线数据回调实现
 * @param stdCode 标准合约代码
 * @param period 周期（如"m1"、"m5"等）
 * @param times 周期倍数
 * @param newBar 新的K线数据指针
 * 
 * 当收到新的K线数据时被调用，处理K线闭合事件。
 * 在数据托管模式下，会检查用户数据是否修改并自动保存。
 */
void HftStraBaseCtx::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 撤销指定订单实现
 * @param localid 本地订单ID
 * @return bool 撤销成功返回true，失败返回false
 * 
 * 根据本地订单ID撤销指定的订单。
 */
bool HftStraBaseCtx::stra_cancel(uint32_t localid)
{
	return _trader->cancel(localid);  // 调用交易适配器的撤销订单方法
}

/**
 * @brief 撤销指定合约的订单实现
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入方向
 * @param qty 撤销数量
 * @return OrderIDs 返回被撤销的订单ID列表
 * 
 * 撤销指定合约、指定方向、指定数量的订单。
 * 会先进行撤单频率检查。
 */
OrderIDs HftStraBaseCtx::stra_cancel(const char* stdCode, bool isBuy, double qty)
{
	//撤单频率检查
	if (!_trader->checkCancelLimits(stdCode))  // 如果撤单频率超限
		return OrderIDs();  // 返回空订单ID列表

	return _trader->cancel(stdCode, isBuy, qty);  // 调用交易适配器的撤销订单方法
}

/**
 * @brief 获取内部合约代码实现
 * @param stdCode 标准合约代码
 * @return const char* 返回内部合约代码，如果不存在映射则返回原代码
 * 
 * 根据代码映射表获取内部合约代码。
 * 用于处理标准代码到实际代码的转换。
 */
const char* HftStraBaseCtx::get_inner_code(const char* stdCode)
{
	auto it = _code_map.find(stdCode);  // 在代码映射表中查找
	if (it == _code_map.end())  // 如果未找到映射
		return stdCode;  // 返回原始代码

	return it->second.c_str();  // 返回映射后的实际代码
}

/**
 * @brief 买入下单接口实现
 * @param stdCode 标准合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param userTag 用户标签
 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
 * @param bForceClose 是否强平，默认false
 * @return OrderIDs 返回订单ID列表
 * 
 * 执行买入操作，支持限价单和市价单，支持FAK和FOK订单类型。
 * 支持合约代码映射（标准代码到实际代码的转换）。
 * 会进行下单限制检查。
 * 
 * 处理逻辑：
 * 1. 提取标准合约代码信息
 * 2. 如果存在自定义规则标签，进行代码映射
 * 3. 获取合约信息
 * 4. 进行下单限制检查
 * 5. 调用交易适配器执行买入操作
 * 6. 为每个订单设置用户标签
 */
OrderIDs HftStraBaseCtx::stra_buy(const char* stdCode, double price, double qty, const char* userTag, int flag /* = 0 */, bool bForceClose /* = false */)
{
	/*
	 *	By Wesley @ 2022.05.26
	 *	如果找到匹配自定义规则，则进行映射处理
	 */
	 //const char* ruleTag = _engine->get_hot_mgr()->getRuleTag(stdCode);  // 注释掉的代码：获取规则标签（已废弃）
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		std::string realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		WTSContractInfo* ct = _engine->get_basedata_mgr()->getContract(code.c_str(), cInfo._exchg);  // 获取合约信息

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码

		if (_trader && !_trader->checkOrderLimits(realCode.c_str()))  // 如果交易适配器存在且下单限制检查失败
		{
			log_info("{} is forbidden to trade", realCode.c_str());  // 记录信息日志：合约被禁止交易
			return OrderIDs();  // 返回空订单ID列表
		}

		auto ids = _trader->buy(realCode.c_str(), price, qty, flag, bForceClose, ct);  // 调用交易适配器执行买入操作
		for (auto localid : ids)  // 遍历返回的订单ID列表
			setUserTag(localid, userTag);  // 为每个订单设置用户标签

		return ids;  // 返回订单ID列表
	}
	else  // 如果不存在自定义规则标签
	{
		WTSContractInfo* ct = _engine->get_basedata_mgr()->getContract(cInfo._code, cInfo._exchg);  // 获取合约信息
		if (ct == NULL)  // 如果合约信息不存在
		{
			log_error("Cannot find corresponding contract info of {}", stdCode);  // 记录错误日志：找不到合约信息
			return OrderIDs();  // 返回空订单ID列表
		}

		if (!_trader->checkOrderLimits(stdCode))  // 如果下单限制检查失败
		{
			log_info("{} is forbidden to trade", stdCode);  // 记录信息日志：合约被禁止交易
			return OrderIDs();  // 返回空订单ID列表
		}

		auto ids = _trader->buy(stdCode, price, qty, flag, bForceClose, ct);  // 调用交易适配器执行买入操作
		for (auto localid : ids)  // 遍历返回的订单ID列表
			setUserTag(localid, userTag);  // 为每个订单设置用户标签
		return ids;  // 返回订单ID列表
	}
}

/**
 * @brief 卖出下单接口实现
 * @param stdCode 标准合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param userTag 用户标签
 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
 * @param bForceClose 是否强平，默认false
 * @return OrderIDs 返回订单ID列表
 * 
 * 执行卖出操作，支持限价单和市价单，支持FAK和FOK订单类型。
 * 对于不能做空的品种，会检查可用持仓是否足够。
 * 支持合约代码映射。
 * 会进行下单限制检查。
 * 
 * 处理逻辑：
 * 1. 提取标准合约代码信息
 * 2. 检查品种是否可以做空，如果不能做空则检查可用持仓
 * 3. 如果存在自定义规则标签，进行代码映射
 * 4. 获取合约信息
 * 5. 进行下单限制检查
 * 6. 调用交易适配器执行卖出操作
 * 7. 为每个订单设置用户标签
 */
OrderIDs HftStraBaseCtx::stra_sell(const char* stdCode, double price, double qty, const char* userTag, int flag /* = 0 */, bool bForceClose /* = false */)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	WTSCommodityInfo* commInfo = _engine->get_basedata_mgr()->getCommodity(cInfo._exchg, cInfo._product);  // 获取商品信息

	//如果不能做空，则要看可用持仓
	if (!commInfo->canShort())  // 如果商品不能做空
	{
		double curPos = stra_get_position(stdCode, true);//只读可用持仓  // 获取可用持仓数量
		if (decimal::gt(qty, curPos))  // 如果卖出数量大于可用持仓
		{
			log_error("No enough position of {} to sell", stdCode);  // 记录错误日志：持仓不足
			return OrderIDs();  // 返回空订单ID列表
		}
	}

	/*
	 *	By Wesley @ 2022.05.26
	 *	如果找到匹配自定义规则，则进行映射处理
	 */
	
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		std::string realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		WTSContractInfo* ct = _engine->get_basedata_mgr()->getContract(code.c_str(), cInfo._exchg);  // 获取合约信息

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码

		if (_trader && !_trader->checkOrderLimits(realCode.c_str()))  // 如果交易适配器存在且下单限制检查失败
		{
			log_info("{} is forbidden to trade", realCode.c_str());  // 记录信息日志：合约被禁止交易
			return OrderIDs();  // 返回空订单ID列表
		}

		auto ids = _trader->sell(realCode.c_str(), price, qty, flag, bForceClose, ct);  // 调用交易适配器执行卖出操作
		for (auto localid : ids)  // 遍历返回的订单ID列表
			setUserTag(localid, userTag);  // 为每个订单设置用户标签
		return ids;  // 返回订单ID列表
	}
	else  // 如果不存在自定义规则标签
	{
		WTSContractInfo* ct = _engine->get_basedata_mgr()->getContract(cInfo._code, cInfo._exchg);  // 获取合约信息
		if (ct == NULL)  // 如果合约信息不存在
		{
			log_error("Cannot find corresponding contract info of {}", stdCode);  // 记录错误日志：找不到合约信息
			return OrderIDs();  // 返回空订单ID列表
		}

		if (_trader && !_trader->checkOrderLimits(stdCode))  // 如果交易适配器存在且下单限制检查失败
		{
			log_info("{} is forbidden to trade", stdCode);  // 记录信息日志：合约被禁止交易
			return OrderIDs();  // 返回空订单ID列表
		}

		auto ids = _trader->sell(stdCode, price, qty, flag, bForceClose, ct);  // 调用交易适配器执行卖出操作
		for (auto localid : ids)  // 遍历返回的订单ID列表
			setUserTag(localid, userTag);  // 为每个订单设置用户标签
		return ids;  // 返回订单ID列表
	}
}

/**
 * @brief 开多下单接口实现
 * @param stdCode 标准合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param userTag 用户标签
 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
 * @return uint32_t 返回订单ID
 * 
 * 执行开多操作，支持合约代码映射。
 */
uint32_t HftStraBaseCtx::stra_enter_long(const char* stdCode, double price, double qty, const char* userTag, int flag/* = 0*/)
{
	std::string realCode = stdCode;  // 初始化实际代码为标准代码
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码
		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码
	}

	return _trader->openLong(realCode.c_str(), price, qty, flag);  // 调用交易适配器执行开多操作
}

/**
 * @brief 平多下单接口实现
 * @param stdCode 标准合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param userTag 用户标签
 * @param isToday 是否今仓，默认false
 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
 * @return uint32_t 返回订单ID
 * 
 * 执行平多操作，支持指定今仓或昨仓，支持合约代码映射。
 */
uint32_t HftStraBaseCtx::stra_exit_long(const char* stdCode, double price, double qty, const char* userTag, bool isToday/* = false*/, int flag/* = 0*/)
{
	std::string realCode = stdCode;  // 初始化实际代码为标准代码
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码
	}

	return _trader->closeLong(realCode.c_str(), price, qty, isToday, flag);  // 调用交易适配器执行平多操作
}

/**
 * @brief 开空下单接口实现
 * @param stdCode 标准合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param userTag 用户标签
 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
 * @return uint32_t 返回订单ID
 * 
 * 执行开空操作，支持合约代码映射。
 */
uint32_t HftStraBaseCtx::stra_enter_short(const char* stdCode, double price, double qty, const char* userTag, int flag/* = 0*/)
{
	std::string realCode = stdCode;  // 初始化实际代码为标准代码
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码
	}
	//else if (CodeHelper::isStdFutHotCode(stdCode))  // 注释掉的代码：检查是否为期货主力合约代码（已废弃）
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);  // 提取标准合约代码信息
	//	std::string code = _engine->get_hot_mgr()->getRawCode(cInfo._exchg, cInfo._product, _engine->get_trading_date());  // 获取主力合约代码
	//	realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

	//	_code_map[realCode] = stdCode;  // 建立代码映射关系
	//}
	//else if (CodeHelper::isStdFut2ndCode(stdCode))  // 注释掉的代码：检查是否为期货次主力合约代码（已废弃）
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);  // 提取标准合约代码信息
	//	std::string code = _engine->get_hot_mgr()->getSecondRawCode(cInfo._exchg, cInfo._product, _engine->get_trading_date());  // 获取次主力合约代码
	//	realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

	//	_code_map[realCode] = stdCode;  // 建立代码映射关系
	//}

	return _trader->openShort(realCode.c_str(), price, qty, flag);  // 调用交易适配器执行开空操作
}

/**
 * @brief 平空下单接口实现
 * @param stdCode 标准合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param userTag 用户标签
 * @param isToday 是否今仓，默认false
 * @param flag 下单标志：0-普通单，1-FAK，2-FOK，默认0
 * @return uint32_t 返回订单ID
 * 
 * 执行平空操作，支持指定今仓或昨仓，支持合约代码映射。
 */
uint32_t HftStraBaseCtx::stra_exit_short(const char* stdCode, double price, double qty, const char* userTag, bool isToday/* = false*/, int flag/* = 0*/)
{
	std::string realCode = stdCode;  // 初始化实际代码为标准代码
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码
	}
	//else if (CodeHelper::isStdFutHotCode(stdCode))  // 注释掉的代码：检查是否为期货主力合约代码（已废弃）
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);  // 提取标准合约代码信息
	//	std::string code = _engine->get_hot_mgr()->getRawCode(cInfo._exchg, cInfo._product, _engine->get_trading_date());  // 获取主力合约代码
	//	realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

	//	_code_map[realCode] = stdCode;  // 建立代码映射关系
	//}
	//else if (CodeHelper::isStdFut2ndCode(stdCode))  // 注释掉的代码：检查是否为期货次主力合约代码（已废弃）
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);  // 提取标准合约代码信息
	//	std::string code = _engine->get_hot_mgr()->getSecondRawCode(cInfo._exchg, cInfo._product, _engine->get_trading_date());  // 获取次主力合约代码
	//	realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

	//	_code_map[realCode] = stdCode;  // 建立代码映射关系
	//}

	return _trader->closeShort(realCode.c_str(), price, qty, isToday, flag);  // 调用交易适配器执行平空操作
}

/**
 * @brief 获取商品信息实现
 * @param stdCode 标准合约代码
 * @return WTSCommodityInfo* 返回商品信息对象指针
 * 
 * 根据标准合约代码获取商品信息，包括交易规则、手续费等。
 */
WTSCommodityInfo* HftStraBaseCtx::stra_get_comminfo(const char* stdCode)
{
	return _engine->get_commodity_info(stdCode);  // 从引擎获取商品信息
}

/**
 * @brief 获取分月合约代码实现
 * @param stdCode 标准合约代码
 * @return std::string 返回分月合约代码字符串
 * 
 * 根据标准合约代码获取对应的分月合约代码。
 */
std::string HftStraBaseCtx::stra_get_rawcode(const char* stdCode)
{
	return _engine->get_rawcode(stdCode);  // 从引擎获取分月合约代码
}

/**
 * @brief 获取K线数据实现
 * @param stdCode 标准合约代码
 * @param period 周期（如"m1"、"m5"等）
 * @param count 获取的K线数量
 * @return WTSKlineSlice* 返回K线数据切片指针
 * 
 * 获取指定合约的K线数据切片。
 * 周期格式：基础周期+倍数（如"m5"表示5分钟K线，"d1"表示1日K线）。
 * 获取成功后会自动订阅该合约的Tick数据。
 */
WTSKlineSlice* HftStraBaseCtx::stra_get_bars(const char* stdCode, const char* period, uint32_t count)
{
	thread_local static char basePeriod[2] = { 0 };  // 线程局部静态变量，用于存储基础周期
	basePeriod[0] = period[0];  // 提取周期字符串的第一个字符作为基础周期
	uint32_t times = 1;  // 初始化周期倍数为1
	if (strlen(period) > 1)  // 如果周期字符串长度大于1
		times = strtoul(period + 1, NULL, 10);  // 将周期字符串的第二部分转换为整数作为倍数

	WTSKlineSlice* ret = _engine->get_kline_slice(_context_id, stdCode, basePeriod, count, times);  // 从引擎获取K线数据切片

	if (ret)  // 如果获取成功
		_engine->sub_tick(id(), stdCode);  // 自动订阅该合约的Tick数据

	return ret;  // 返回K线数据切片指针
}

/**
 * @brief 获取Tick数据实现
 * @param stdCode 标准合约代码
 * @param count 获取的Tick数量
 * @return WTSTickSlice* 返回Tick数据切片指针
 * 
 * 获取指定合约的Tick数据切片。
 * 获取成功后会自动订阅该合约的Tick数据。
 */
WTSTickSlice* HftStraBaseCtx::stra_get_ticks(const char* stdCode, uint32_t count)
{
	WTSTickSlice* ticks = _engine->get_tick_slice(_context_id, stdCode, count);  // 从引擎获取Tick数据切片

	if (ticks)  // 如果获取成功
		_engine->sub_tick(id(), stdCode);  // 自动订阅该合约的Tick数据
	return ticks;  // 返回Tick数据切片指针
}

/**
 * @brief 获取委托明细数据实现
 * @param stdCode 标准合约代码
 * @param count 获取的委托明细数量
 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针
 * 
 * 获取指定合约的委托明细数据切片。
 * 获取成功后会自动订阅该合约的委托明细数据。
 */
WTSOrdDtlSlice* HftStraBaseCtx::stra_get_order_detail(const char* stdCode, uint32_t count)
{
	WTSOrdDtlSlice* ret = _engine->get_order_detail_slice(_context_id, stdCode, count);  // 从引擎获取委托明细数据切片

	if (ret)  // 如果获取成功
		_engine->sub_order_detail(id(), stdCode);  // 自动订阅该合约的委托明细数据
	return ret;  // 返回委托明细数据切片指针
}

/**
 * @brief 获取委托队列数据实现
 * @param stdCode 标准合约代码
 * @param count 获取的委托队列数量
 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
 * 
 * 获取指定合约的委托队列数据切片。
 * 获取成功后会自动订阅该合约的委托队列数据。
 */
WTSOrdQueSlice* HftStraBaseCtx::stra_get_order_queue(const char* stdCode, uint32_t count)
{
	WTSOrdQueSlice* ret = _engine->get_order_queue_slice(_context_id, stdCode, count);  // 从引擎获取委托队列数据切片

	if (ret)  // 如果获取成功
		_engine->sub_order_queue(id(), stdCode);  // 自动订阅该合约的委托队列数据
	return ret;  // 返回委托队列数据切片指针
}


/**
 * @brief 获取逐笔成交数据实现
 * @param stdCode 标准合约代码
 * @param count 获取的逐笔成交数量
 * @return WTSTransSlice* 返回逐笔成交数据切片指针
 * 
 * 获取指定合约的逐笔成交数据切片。
 * 获取成功后会自动订阅该合约的逐笔成交数据。
 */
WTSTransSlice* HftStraBaseCtx::stra_get_transaction(const char* stdCode, uint32_t count)
{
	WTSTransSlice* ret = _engine->get_transaction_slice(_context_id, stdCode, count);  // 从引擎获取逐笔成交数据切片

	if (ret)  // 如果获取成功
		_engine->sub_transaction(id(), stdCode);  // 自动订阅该合约的逐笔成交数据
	return ret;  // 返回逐笔成交数据切片指针
}


/**
 * @brief 获取最新Tick数据实现
 * @param stdCode 标准合约代码
 * @return WTSTickData* 返回最新Tick数据指针
 * 
 * 获取指定合约的最新Tick数据。
 */
WTSTickData* HftStraBaseCtx::stra_get_last_tick(const char* stdCode)
{
	return _engine->get_last_tick(_context_id, stdCode);  // 从引擎获取最新Tick数据
}

/**
 * @brief 订阅Tick数据实现
 * @param stdCode 标准合约代码
 * 
 * 主动订阅指定合约的Tick数据。
 * 订阅后会在本地记录，并在回调时检查。
 * 
 * 注意：By Wesley @ 2022.03.01
 * 主动订阅tick会在本地记一下，tick数据回调的时候先检查一下。
 */
void HftStraBaseCtx::stra_sub_ticks(const char* stdCode)
{
	/*
	 *	By Wesley @ 2022.03.01
	 *	主动订阅tick会在本地记一下
	 *	tick数据回调的时候先检查一下
	 */
	_tick_subs.insert(stdCode);  // 将合约代码添加到Tick订阅列表

	_engine->sub_tick(id(), stdCode);  // 通过引擎订阅Tick数据
	log_info("Market Data subscribed: {}", stdCode);  // 记录信息日志：市场数据订阅成功
}

/**
 * @brief 订阅委托明细数据实现
 * @param stdCode 标准合约代码
 * 
 * 主动订阅指定合约的委托明细数据。
 */
void HftStraBaseCtx::stra_sub_order_details(const char* stdCode)
{
	_engine->sub_order_detail(id(), stdCode);  // 通过引擎订阅委托明细数据
	log_info("Order details subscribed: {}", stdCode);  // 记录信息日志：委托明细订阅成功
}

/**
 * @brief 订阅委托队列数据实现
 * @param stdCode 标准合约代码
 * 
 * 主动订阅指定合约的委托队列数据。
 */
void HftStraBaseCtx::stra_sub_order_queues(const char* stdCode)
{
	_engine->sub_order_queue(id(), stdCode);  // 通过引擎订阅委托队列数据
	log_info("Order queues subscribed: {}", stdCode);  // 记录信息日志：委托队列订阅成功
}

/**
 * @brief 订阅逐笔成交数据实现
 * @param stdCode 标准合约代码
 * 
 * 主动订阅指定合约的逐笔成交数据。
 */
void HftStraBaseCtx::stra_sub_transactions(const char* stdCode)
{
	_engine->sub_transaction(id(), stdCode);  // 通过引擎订阅逐笔成交数据
	log_info("Transactions subscribed: {}", stdCode);  // 记录信息日志：逐笔成交订阅成功
}

/**
 * @brief 记录信息级别日志实现
 * @param message 日志消息内容
 * 
 * 记录信息级别的日志消息。
 */
void HftStraBaseCtx::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 调用日志记录器记录信息级别日志
}

/**
 * @brief 记录调试级别日志实现
 * @param message 日志消息内容
 * 
 * 记录调试级别的日志消息。
 */
void HftStraBaseCtx::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 调用日志记录器记录调试级别日志
}

/**
 * @brief 记录警告级别日志实现
 * @param message 日志消息内容
 * 
 * 记录警告级别的日志消息。
 */
void HftStraBaseCtx::stra_log_warn(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_WARN, message);  // 调用日志记录器记录警告级别日志
}

/**
 * @brief 记录错误级别日志实现
 * @param message 日志消息内容
 * 
 * 记录错误级别的日志消息。
 */
void HftStraBaseCtx::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 调用日志记录器记录错误级别日志
}

/**
 * @brief 成交通知回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入方向
 * @param vol 成交数量
 * @param price 成交价格
 * 
 * 当订单成交时被调用，更新持仓信息并记录成交日志。
 * 处理逻辑：
 * 1. 检查并保存用户数据（如果已修改）
 * 2. 在数据托管模式下记录信号日志
 * 3. 计算新的持仓数量并更新持仓明细
 */
void HftStraBaseCtx::on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}

	if(_sig_logs && _data_agent)  // 如果信号日志文件存在且启用数据托管模式
	{
		double curPos = stra_get_position(stdCode);  // 获取当前持仓数量
		_sig_logs->write_file(fmt::format("{}.{}.{},{}{},{},{}\n", stra_get_date(), stra_get_time(), stra_get_secs(), isBuy ? "+" : "-", vol, curPos, price));  // 写入信号日志：日期.时间.秒数,方向+数量,当前持仓,成交价格
	}

	const PosInfo& posInfo = _pos_map[stdCode];  // 获取持仓信息（如果不存在会创建）
	double curPos = posInfo._volume + vol * (isBuy ? 1 : -1);  // 计算新的持仓数量：当前持仓 + 成交数量 * 方向（买入+1，卖出-1）
	do_set_position(stdCode, curPos, price, getOrderTag(localid));  // 设置新的目标持仓，传入成交价格和订单标签
}

/**
 * @brief 订单状态通知回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy 是否为买入方向
 * @param totalQty 订单总数量
 * @param leftQty 剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤销
 * 
 * 当订单状态发生变化时被调用，处理订单状态更新。
 * 如果订单已撤销或全部成交，可以清理订单标签。
 */
void HftStraBaseCtx::on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled /* = false */)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}

	if(isCanceled || decimal::eq(leftQty, 0))  // 如果订单已撤销或剩余数量为0（订单已全部成交）
	{
		//订单结束了，要把订单号清理掉，不然开销太大
		// 注意：当前实现中未实际清理订单标签，可能需要后续优化
	}
}

/**
 * @brief 交易通道就绪通知回调实现
 * 
 * 当交易通道就绪时被调用，执行通道就绪后的准备工作。
 * 会检查并保存用户数据（如果已修改）。
 */
void HftStraBaseCtx::on_channel_ready()
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 交易通道丢失通知回调实现
 * 
 * 当交易通道丢失时被调用，执行通道丢失后的清理工作。
 * 会检查并保存用户数据（如果已修改）。
 */
void HftStraBaseCtx::on_channel_lost()
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 委托通知回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 消息内容
 * 
 * 当委托结果返回时被调用，处理委托成功或失败的情况。
 * 会检查并保存用户数据（如果已修改）。
 */
void HftStraBaseCtx::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	if (_ud_modified)  // 如果用户数据已修改
	{
		save_userdata();  // 保存用户数据
		_ud_modified = false;  // 重置修改标志
	}
}

/**
 * @brief 持仓变化通知回调实现
 * @param stdCode 标准合约代码
 * @param isLong 是否为做多方向
 * @param prevol 变化前持仓数量
 * @param preavail 变化前可用持仓数量
 * @param newvol 变化后持仓数量
 * @param newavail 变化后可用持仓数量
 * @param tradingday 交易日
 * 
 * 当持仓发生变化时被调用，处理持仓变化通知。
 * 当前实现为空，可以根据需要添加持仓变化处理逻辑。
 */
void HftStraBaseCtx::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	// 当前实现为空，可以根据需要添加持仓变化处理逻辑
}

/**
 * @brief 获取持仓盈亏实现
 * @param stdCode 标准合约代码
 * @return double 返回持仓盈亏金额
 * 
 * 获取指定合约的持仓浮动盈亏。
 * 如果合约不存在持仓，返回0。
 */
double HftStraBaseCtx::stra_get_position_profit(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找
	if (it == _pos_map.end())  // 如果未找到
		return 0.0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._dynprofit;  // 返回当前浮动盈亏
}

/**
 * @brief 获取持仓平均价格实现
 * @param stdCode 标准合约代码
 * @return double 返回持仓平均价格
 * 
 * 根据持仓明细计算持仓平均价格。
 * 计算公式：平均价格 = 所有持仓明细的价格*数量的总和 / 总持仓数量
 * 如果没有持仓，返回0。
 */
double HftStraBaseCtx::stra_get_position_avgpx(const char* stdCode)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找
	if (it == _pos_map.end())  // 如果未找到
		return 0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	if (pInfo._volume == 0)  // 如果持仓数量为0
		return 0.0;  // 返回0

	double amount = 0.0;  // 初始化总金额为0
	for (auto dit = pInfo._details.begin(); dit != pInfo._details.end(); dit++)  // 遍历所有持仓明细
	{
		const DetailInfo& dInfo = *dit;  // 获取持仓明细信息
		amount += dInfo._price*dInfo._volume;  // 累加：价格 * 数量
	}

	return amount / pInfo._volume;  // 返回平均价格：总金额 / 总持仓数量
}

/**
 * @brief 获取持仓数量实现
 * @param stdCode 标准合约代码
 * @param bOnlyValid 是否只查询可用持仓，默认false
 * @param flag 持仓标志，默认3（全部）
 * @return double 返回持仓数量
 * 
 * 获取指定合约的持仓数量。
 * 支持合约代码映射。
 * 如果合约不存在持仓，返回0。
 */
double HftStraBaseCtx::stra_get_position(const char* stdCode, bool bOnlyValid /* = false */, int flag /* = 3*/)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		std::string realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码

		return _trader->getPosition(realCode.c_str(), bOnlyValid, flag);  // 从交易适配器获取实际代码的持仓数量
	}
	else  // 如果不存在自定义规则标签
	{
		return _trader->getPosition(stdCode, bOnlyValid, flag);  // 从交易适配器获取标准代码的持仓数量
	}
}

/**
 * @brief 获取未成交数量实现
 * @param stdCode 标准合约代码
 * @return double 返回未成交数量
 * 
 * 获取指定合约的未成交订单数量。
 * 支持合约代码映射。
 */
double HftStraBaseCtx::stra_get_undone(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _engine->get_hot_mgr());  // 提取标准合约代码信息
	if (strlen(cInfo._ruletag) > 0)  // 如果存在自定义规则标签
	{
		std::string code = _engine->get_hot_mgr()->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _engine->get_trading_date());  // 根据规则标签获取实际合约代码
		std::string realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

		_code_map[realCode] = stdCode;  // 建立代码映射关系：实际代码 -> 标准代码

		return _trader->getUndoneQty(realCode.c_str());  // 从交易适配器获取实际代码的未成交数量
	}
	//else if (CodeHelper::isStdFutHotCode(stdCode))  // 注释掉的代码：检查是否为期货主力合约代码（已废弃）
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);  // 提取标准合约代码信息
	//	std::string code = _engine->get_hot_mgr()->getRawCode(cInfo._exchg, cInfo._product, _engine->get_trading_date());  // 获取主力合约代码
	//	std::string realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

	//	_code_map[realCode] = stdCode;  // 建立代码映射关系

	//	return _trader->getUndoneQty(realCode.c_str());  // 从交易适配器获取实际代码的未成交数量
	//}
	//else if (CodeHelper::isStdFut2ndCode(stdCode))  // 注释掉的代码：检查是否为期货次主力合约代码（已废弃）
	//{
	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);  // 提取标准合约代码信息
	//	std::string code = _engine->get_hot_mgr()->getSecondRawCode(cInfo._exchg, cInfo._product, _engine->get_trading_date());  // 获取次主力合约代码
	//	std::string realCode = CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始合约代码转换为标准合约代码

	//	_code_map[realCode] = stdCode;  // 建立代码映射关系

	//	return _trader->getUndoneQty(realCode.c_str());  // 从交易适配器获取实际代码的未成交数量
	//}
	else  // 如果不存在自定义规则标签
	{
		return _trader->getUndoneQty(stdCode);  // 从交易适配器获取标准代码的未成交数量
	}
}

/**
 * @brief 获取最新价格实现
 * @param stdCode 标准合约代码
 * @return double 返回最新价格
 * 
 * 获取指定合约的最新价格。
 * 先从价格映射表中查找，如果没有则从引擎获取。
 */
double HftStraBaseCtx::stra_get_price(const char* stdCode)
{
	auto it = _price_map.find(stdCode);  // 在价格映射表中查找
	if (it != _price_map.end())  // 如果找到
		return it->second;  // 返回缓存的价格

	return _engine->get_cur_price(stdCode);  // 如果未找到，从引擎获取当前价格
}

/**
 * @brief 获取当前日期实现
 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
 * 
 * 获取当前交易日日期。
 */
uint32_t HftStraBaseCtx::stra_get_date()
{
	return _engine->get_date();  // 从引擎获取当前日期
}

/**
 * @brief 获取当前时间实现
 * @return uint32_t 返回当前时间（格式：HHMMSS）
 * 
 * 获取当前交易时间。
 */
uint32_t HftStraBaseCtx::stra_get_time()
{
	return _engine->get_raw_time();  // 从引擎获取当前时间
}

/**
 * @brief 获取当前秒数实现
 * @return uint32_t 返回当前秒数（0-59）
 * 
 * 获取当前时间的秒数部分。
 */
uint32_t HftStraBaseCtx::stra_get_secs()
{
	return _engine->get_secs();  // 从引擎获取当前秒数
}

/**
 * @brief 加载用户数据实现
 * @param key 数据键
 * @param defVal 默认值，如果数据不存在则返回此值
 * @return const char* 返回数据值字符串
 * 
 * 加载策略自定义数据，如果数据不存在则返回默认值。
 */
const char* HftStraBaseCtx::stra_load_user_data(const char* key, const char* defVal /*= ""*/)
{
	auto it = _user_datas.find(key);  // 在用户数据映射表中查找
	if (it != _user_datas.end())  // 如果找到
		return it->second.c_str();  // 返回数据值

	return defVal;  // 如果未找到，返回默认值
}

/**
 * @brief 保存用户数据实现
 * @param key 数据键
 * @param val 数据值
 * 
 * 保存策略自定义数据，数据会在适当时机自动持久化到文件。
 * 设置修改标志，触发后续自动保存。
 */
void HftStraBaseCtx::stra_save_user_data(const char* key, const char* val)
{
	_user_datas[key] = val;  // 保存用户数据到映射表
	_ud_modified = true;  // 设置修改标志，触发后续自动保存
}

/**
 * @brief 保存用户数据到文件实现
 * 
 * 将策略的用户自定义数据保存到JSON文件。
 * 文件路径：用户数据目录 + "ud_" + 策略名称 + ".json"
 * 使用rapidjson进行JSON格式转换和写入。
 */
void HftStraBaseCtx::save_userdata()
{
	//ini.save(filename.c_str());  // 注释掉的代码：使用INI格式保存（已废弃）
	rj::Document root(rj::kObjectType);  // 创建JSON文档对象（对象类型）
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用
	for (auto it = _user_datas.begin(); it != _user_datas.end(); it++)  // 遍历所有用户数据
	{
		root.AddMember(rj::Value(it->first.c_str(), allocator), rj::Value(it->second.c_str(), allocator), allocator);  // 将键值对添加到JSON对象中
	}

	{
		std::string filename = WtHelper::getStraUsrDatDir();  // 获取用户数据目录路径
		filename += "ud_";  // 追加前缀"ud_"
		filename += _name;  // 追加策略名称
		filename += ".json";  // 追加文件扩展名".json"

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 如果成功创建新文件
		{
			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区
			bf.write_file(sb.GetString());  // 将JSON字符串写入文件
			bf.close_file();  // 关闭文件
		}
	}
}

/**
 * @brief 从文件加载用户数据实现
 * 
 * 从JSON文件加载策略的用户自定义数据。
 * 文件路径：用户数据目录 + "ud_" + 策略名称 + ".json"
 * 使用rapidjson进行JSON格式解析。
 * 如果文件不存在或解析失败，则跳过加载。
 */
void HftStraBaseCtx::load_userdata()
{
	std::string filename = WtHelper::getStraUsrDatDir();  // 获取用户数据目录路径
	filename += "ud_";  // 追加前缀"ud_"
	filename += _name;  // 追加策略名称
	filename += ".json";  // 追加文件扩展名".json"

	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回，不进行加载
	}

	std::string content;  // 定义文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;  // 直接返回

	rj::Document root;  // 创建JSON文档对象
	root.Parse(content.c_str());  // 解析JSON字符串

	if (root.HasParseError())  // 如果解析失败
		return;  // 直接返回，不进行加载

	for (auto& m : root.GetObject())  // 遍历JSON对象的所有成员
	{
		const char* key = m.name.GetString();  // 获取键名
		const char* val = m.value.GetString();  // 获取键值
		_user_datas[key] = val;  // 保存到用户数据映射表
	}
}

/**
 * @brief 设置目标持仓实现
 * @param stdCode 标准合约代码
 * @param qty 目标持仓数量
 * @param price 成交价格，默认0.0（表示使用当前市场价格）
 * @param userTag 用户标签，默认空字符串
 * 
 * 根据目标持仓数量调整实际持仓，并记录交易和平仓日志。
 * 内部会处理开仓、平仓、反手等逻辑，并计算盈亏和手续费。
 * 
 * 处理逻辑：
 * 1. 获取当前价格（如果未指定价格，则使用价格映射表中的最新价格）。
 * 2. 如果当前持仓等于目标持仓，则直接返回。
 * 3. 计算持仓差异（diff = 目标持仓 - 当前持仓）。
 * 4. 如果当前持仓与目标持仓方向一致（同为正或同为负），则增加持仓明细。
 * 5. 如果当前持仓与目标持仓方向不一致，则需要平仓：
 *    - 遍历持仓明细，按先进先出（FIFO）原则平仓。
 *    - 计算平仓盈亏、手续费，并更新累计盈亏。
 *    - 记录交易日志和平仓日志。
 *    - 清理已平仓完的明细。
 *    - 如果平仓后还有剩余，则反手开仓。
 * 6. 在回测模式下，根据滑点设置调整成交价格。
 * 7. 计算并累加手续费。
 */
void HftStraBaseCtx::do_set_position(const char* stdCode, double qty, double price /* = 0.0 */, const char* userTag /*= ""*/)
{
	PosInfo& pInfo = _pos_map[stdCode];  // 获取或创建持仓信息
	double curPx = price;  // 初始化成交价格为指定价格
	if (decimal::eq(price, 0.0))  // 如果价格未指定（为0）
		curPx = _price_map[stdCode];  // 使用价格映射表中的最新价格
	uint64_t curTm = (uint64_t)_engine->get_date() * 1000000000 + (uint64_t)_engine->get_raw_time() * 100000 + _engine->get_secs();  // 构造当前时间戳（纳秒级）
	uint32_t curTDate = _engine->get_trading_date();  // 获取当前交易日

	//手数相等则不用操作了
	if (decimal::eq(pInfo._volume, qty))  // 如果当前持仓等于目标持仓
		return;  // 直接返回，无需操作

	log_info("Target position updated: {} -> {}", pInfo._volume, qty);  // 记录信息日志：目标持仓更新

	WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取商品信息

	//成交价
	double trdPx = curPx;  // 初始化实际成交价格为当前价格

	double diff = qty - pInfo._volume;  // 计算持仓差异：目标持仓 - 当前持仓
	bool isBuy = decimal::gt(diff, 0.0);  // 判断是否为买入方向（持仓增加）
	if (decimal::gt(pInfo._volume*diff, 0))  // 如果当前持仓和目标持仓方向一致（同为正或同为负），则增加持仓明细，增加数量即可
	{
		pInfo._volume = qty;  // 更新总持仓数量

		if (_slippage != 0)  // 如果设置了滑点（回测模式）
		{
			trdPx += _slippage * commInfo->getPriceTick()*(isBuy ? 1 : -1);  // 调整成交价格：买入时加滑点，卖出时减滑点
		}

		DetailInfo dInfo;  // 创建新的持仓明细
		dInfo._long = decimal::gt(qty, 0);  // 设置方向：持仓为正则为做多，持仓为负则为做空
		dInfo._price = trdPx;  // 设置开仓价格
		dInfo._volume = abs(diff);  // 设置开仓数量（持仓差异的绝对值）
		dInfo._opentime = curTm;  // 设置开仓时间
		dInfo._opentdate = curTDate;  // 设置开仓日期
		wt_strcpy(dInfo._usertag, userTag);  // 复制用户标签
		pInfo._details.emplace_back(dInfo);  // 将持仓明细添加到明细列表

		double fee = commInfo->calcFee(trdPx, abs(diff), 0);  // 计算手续费（开仓：0）
		_fund_info._total_fees += fee;  // 累加总手续费

		log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(diff), fee, userTag);  // 记录交易日志（开仓）
	}
	else  // 如果持仓方向和仓位变化方向不一致，需要平仓
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
			if (decimal::eq(maxQty, 0))  // 如果平仓数量为0
				continue;  // 跳过，继续下一个明细

			double maxProf = dInfo._max_profit * maxQty / dInfo._volume;  // 按比例计算本次平仓的最大盈利
			double maxLoss = dInfo._max_loss * maxQty / dInfo._volume;  // 按比例计算本次平仓的最大亏损

			dInfo._volume -= maxQty;  // 减少持仓明细的数量
			left -= maxQty;  // 减少剩余需要平仓的数量

			if (decimal::eq(dInfo._volume, 0))  // 如果持仓明细数量为0
				count++;  // 增加已平仓完的明细数量

			double profit = (trdPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算平仓盈亏：（平仓价格 - 开仓价格）* 数量 * 合约乘数
			if (!dInfo._long)  // 如果是做空方向
				profit *= -1;  // 盈亏取反
			pInfo._closeprofit += profit;  // 累加累计平仓盈亏
			pInfo._dynprofit = pInfo._dynprofit*dInfo._volume / (dInfo._volume + maxQty);  // 浮盈也要做等比缩放：动态盈亏 = 原动态盈亏 * (剩余数量 / 原总数量)
			_fund_info._total_profit += profit;  // 累加总平仓盈亏

			double fee = commInfo->calcFee(trdPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费（今仓：2，昨仓：1）
			_fund_info._total_fees += fee;  // 累加总手续费
			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, trdPx, maxQty, fee, userTag);  // 记录交易日志（平仓）
			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, trdPx, maxQty, profit, maxProf, maxLoss, pInfo._closeprofit, dInfo._usertag, userTag);  // 记录平仓日志

			if (left == 0)  // 如果剩余需要平仓的数量为0
				break;  // 跳出循环
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)  // 循环清理已平仓完的明细
		{
			auto it = pInfo._details.begin();  // 获取明细列表的起始迭代器
			pInfo._details.erase(it);  // 删除第一个明细
			count--;  // 减少计数器
		}

		//最后,如果还有剩余的,则需要反手了
		if (left > 0)  // 如果还有剩余需要平仓的数量（说明需要反手开仓）
		{
			left = left * qty / abs(qty);  // 转换剩余数量：如果目标持仓为正则为正，如果目标持仓为负则为负

			DetailInfo dInfo;  // 创建新的持仓明细（反手开仓）
			dInfo._long = decimal::gt(qty, 0);  // 设置方向：持仓为正则为做多，持仓为负则为做空
			dInfo._price = trdPx;  // 设置开仓价格
			dInfo._volume = abs(left);  // 设置开仓数量（剩余数量的绝对值）
			dInfo._opentime = curTm;  // 设置开仓时间
			dInfo._opentdate = curTDate;  // 设置开仓日期
			wt_strcpy(dInfo._usertag, userTag);  // 复制用户标签
			pInfo._details.emplace_back(dInfo);  // 将持仓明细添加到明细列表

			//这里还需要写一笔成交记录
			double fee = commInfo->calcFee(trdPx, abs(left), 0);  // 计算手续费（开仓：0）
			_fund_info._total_fees += fee;  // 累加总手续费
			//_engine->mutate_fund(fee, FFT_Fee);  // 注释掉的代码：更新引擎资金（已废弃）
			log_trade(stdCode, dInfo._long, true, curTm, trdPx, abs(left), fee, userTag);  // 记录交易日志（反手开仓）
		}
	}
}

/**
 * @brief 更新动态盈亏实现
 * @param stdCode 标准合约代码
 * @param newTick 最新Tick数据指针
 * 
 * 根据最新Tick数据更新持仓的动态盈亏。
 * 对于做多持仓，使用买一价计算盈亏；对于做空持仓，使用卖一价计算盈亏。
 * 同时更新每个持仓明细的最大盈利和最大亏损。
 */
void HftStraBaseCtx::update_dyn_profit(const char* stdCode, WTSTickData* newTick)
{
	auto it = _pos_map.find(stdCode);  // 在持仓映射表中查找
	if (it != _pos_map.end())  // 如果找到
	{
		PosInfo& pInfo = (PosInfo&)it->second;  // 获取持仓信息引用
		if (pInfo._volume == 0)  // 如果持仓数量为0
		{
			pInfo._dynprofit = 0;  // 重置动态盈亏为0
		}
		else  // 如果持仓数量不为0
		{
			bool isLong = decimal::gt(pInfo._volume, 0);  // 判断是否为做多方向（持仓为正）
			double price = isLong ? newTick->bidprice(0) : newTick->askprice(0);  // 获取计算价格：做多使用买一价，做空使用卖一价

			WTSCommodityInfo* commInfo = _engine->get_commodity_info(stdCode);  // 获取商品信息
			double dynprofit = 0;  // 初始化总动态盈亏为0
			for (auto pit = pInfo._details.begin(); pit != pInfo._details.end(); pit++)  // 遍历所有持仓明细
			{

				DetailInfo& dInfo = *pit;  // 获取持仓明细引用
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);  // 计算当前盈亏：（数量 * (当前价格 - 开仓价格) * 合约乘数 * 方向系数）
				if (dInfo._profit > 0)  // 如果当前盈亏为正
					dInfo._max_profit = std::max(dInfo._profit, dInfo._max_profit);  // 更新最大盈利：取当前盈亏和最大盈利的最大值
				else if (dInfo._profit < 0)  // 如果当前盈亏为负
					dInfo._max_loss = std::min(dInfo._profit, dInfo._max_loss);  // 更新最大亏损：取当前盈亏和最大亏损的最小值

				dynprofit += dInfo._profit;  // 累加总动态盈亏
			}

			pInfo._dynprofit = dynprofit;  // 更新总动态盈亏
		}
	}
}

/**
 * @brief 交易日开始回调实现
 * @param uTDate 交易日期
 * 
 * 在每个交易日开始时被调用。
 * 当前实现为空，可以根据需要添加初始化逻辑。
 */
void HftStraBaseCtx::on_session_begin(uint32_t uTDate)
{

}

/**
 * @brief 交易日结束回调实现
 * @param uTDate 交易日期
 * 
 * 在每个交易日结束时被调用。
 * 计算并记录当日的总平仓盈亏、总动态盈亏、总手续费和动态余额。
 * 在数据托管模式下，将资金结算数据写入资金日志文件。
 */
void HftStraBaseCtx::on_session_end(uint32_t uTDate)
{
	uint32_t curDate = uTDate;  // 获取交易日期
	//_engine->get_trading_date();  // 注释掉的代码：从引擎获取交易日期（已废弃）

	double total_profit = 0;  // 初始化总平仓盈亏为0
	double total_dynprofit = 0;  // 初始化总动态盈亏为0

	for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)  // 遍历所有持仓
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码
		const PosInfo& pInfo = it->second;  // 获取持仓信息
		total_profit += pInfo._closeprofit;  // 累加累计平仓盈亏
		total_dynprofit += pInfo._dynprofit;  // 累加动态盈亏
	}

	//这里要把当日结算的数据写到日志文件里
	//而且这里回测和实盘写法不同, 先留着, 后面来做
	if (_fund_logs && _data_agent)  // 如果资金日志文件存在且启用了数据托管模式
		_fund_logs->write_file(fmt::format("{},{:.2f},{:.2f},{:.2f},{:.2f}\n", curDate,  // 写入资金结算数据：日期、总平仓盈亏、总动态盈亏、动态余额、总手续费
			_fund_info._total_profit, _fund_info._total_dynprofit,
			_fund_info._total_profit + _fund_info._total_dynprofit - _fund_info._total_fees, _fund_info._total_fees));
}

/**
 * @brief 记录交易日志实现
 * @param stdCode 标准合约代码
 * @param isLong 是否为做多方向
 * @param isOpen 是否为开仓
 * @param curTime 当前时间戳
 * @param price 成交价格
 * @param qty 成交数量
 * @param fee 手续费
 * @param userTag 用户标签
 * 
 * 在数据托管模式下，将交易记录写入交易日志文件（trades.csv）。
 * 记录格式：合约代码,时间,方向(LONG/SHORT),动作(OPEN/CLOSE),价格,数量,手续费,用户标签
 */
void HftStraBaseCtx::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, double fee, const char* userTag/* = ""*/)
{
	if(_trade_logs && _data_agent)  // 如果交易日志文件存在且启用了数据托管模式
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE")  // 格式化交易记录：合约代码,时间,方向,动作
			<< "," << price << "," << qty << "," << fee << "," << userTag << "\n";  // 继续格式化：价格,数量,手续费,用户标签
		_trade_logs->write_file(ss.str());  // 写入交易日志文件
	}
}

/**
 * @brief 记录平仓日志实现
 * @param stdCode 标准合约代码
 * @param isLong 是否为做多方向
 * @param openTime 开仓时间戳
 * @param openpx 开仓价格
 * @param closeTime 平仓时间戳
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 本次平仓盈亏
 * @param maxprofit 最大盈利
 * @param maxloss 最大亏损
 * @param totalprofit 累计平仓盈亏
 * @param enterTag 开仓用户标签
 * @param exitTag 平仓用户标签
 * 
 * 在数据托管模式下，将平仓记录写入平仓日志文件（closes.csv）。
 * 记录格式：合约代码,方向(LONG/SHORT),开仓时间,开仓价格,平仓时间,平仓价格,数量,盈亏,最大盈利,最大亏损,累计盈亏,开仓标签,平仓标签
 */
void HftStraBaseCtx::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double maxprofit, double maxloss,
	double totalprofit /* = 0 */, const char* enterTag/* = ""*/, const char* exitTag/* = ""*/)
{
	if (_close_logs && _data_agent)  // 如果平仓日志文件存在且启用了数据托管模式
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx  // 格式化平仓记录：合约代码,方向,开仓时间,开仓价格
			<< "," << closeTime << "," << closepx << "," << qty << "," << profit << "," << maxprofit << "," << maxloss << ","  // 继续格式化：平仓时间,平仓价格,数量,盈亏,最大盈利,最大亏损
			<< totalprofit << "," << enterTag << "," << exitTag << "\n";  // 继续格式化：累计盈亏,开仓标签,平仓标签
		_close_logs->write_file(ss.str());  // 写入平仓日志文件
	}
}
