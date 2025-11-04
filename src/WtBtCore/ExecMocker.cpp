/*!
 * \file ExecMocker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 执行器模拟器实现文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是ExecMocker类的实现文件，包含了执行器模拟器的所有功能实现。
 *
 * 主要功能模块：
 * 1. 构造函数和析构函数：初始化回测环境，清理资源
 * 2. 初始化功能：加载执行器模块，创建执行器单元，初始化撮合引擎
 * 3. 数据接收处理：处理tick数据，触发执行器计算
 * 4. 订单处理：处理买卖订单，通过撮合引擎进行撮合
 * 5. 成交回报处理：处理成交回报，更新持仓和未完成订单
 * 6. 订单回报处理：处理订单回报，记录订单日志
 * 7. 结果输出：将执行结果输出到CSV文件
 *
 * 核心算法：
 * - 数量模式管理：支持反复正负、一直买、一直卖三种模式
 * - 订单撮合：通过撮合引擎模拟订单成交过程
 * - 持仓管理：跟踪执行器持仓，支持多空双向持仓
 * - 日志记录：记录订单执行时间、信号时间、成交时间等详细信息
 */
#include "ExecMocker.h"
#include "WtHelper.h"                                               // WonderTrader辅助函数

#include "../Includes/WTSVariant.hpp"                               // 变体类型定义
#include "../Share/TimeUtils.hpp"                                   // 时间工具函数
#include "../Share/decimal.h"                                       // 小数精度计算工具
#include "../WTSTools/WTSLogger.h"                                 // 日志工具

#include <boost/filesystem.hpp>                                     // Boost文件系统库

/**
 * @brief 价格转换宏：将正数价格转换为整数（四舍五入）
 */
#define PRICE_DOUBLE_TO_INT_P(x) ((int32_t)((x)*10000.0 + 0.5))
/**
 * @brief 价格转换宏：将负数价格转换为整数（四舍五入）
 */
#define PRICE_DOUBLE_TO_INT_N(x) ((int32_t)((x)*10000.0 - 0.5))
/**
 * @brief 价格转换宏：将价格转换为整数（处理DBL_MAX特殊情况）
 */
#define PRICE_DOUBLE_TO_INT(x) (((x)==DBL_MAX)?0:((x)>0?PRICE_DOUBLE_TO_INT_P(x):PRICE_DOUBLE_TO_INT_N(x)))

/**
 * @brief 生成本地订单ID（外部函数声明）
 * 
 * @return 本地订单ID
 */
extern uint32_t makeLocalOrderID();

/**
 * @brief ExecMocker构造函数
 * 
 * 初始化所有成员变量
 * 
 * @param replayer 历史数据回放器指针
 */
ExecMocker::ExecMocker(HisDataReplayer* replayer)
	: _replayer(replayer)                                            // 初始化历史数据回放器指针
	, _position(0)                                                   // 初始化持仓为0
	, _undone(0)                                                     // 初始化未完成订单数量为0
	, _ord_qty(0)                                                    // 初始化订单数量统计为0
	, _ord_cnt(0)                                                    // 初始化订单数量统计为0
	, _cacl_cnt(0)                                                   // 初始化撤单数量统计为0
	, _cacl_qty(0)                                                   // 初始化撤单数量统计为0
	, _sig_cnt(0)                                                    // 初始化信号数量统计为0
	, _sig_px(DBL_MAX)                                               // 初始化信号价格为最大值
	, _last_tick(NULL)                                                // 初始化最新tick数据为NULL
{
}


/**
 * @brief ExecMocker析构函数
 * 
 * 释放最新tick数据的引用计数
 */
ExecMocker::~ExecMocker()
{
	if (_last_tick)                                                  // 如果最新tick数据存在
		_last_tick->release();                                       // 释放引用计数
}

/**
 * @brief 初始化执行器模拟器
 * 
 * 1. 从配置中读取模块路径、合约代码、周期、数量单位、数量模式等参数
 * 2. 初始化撮合引擎
 * 3. 动态加载执行器模块
 * 4. 创建执行器单元实例
 * 
 * @param cfg 配置信息
 * @return 是否初始化成功
 */
bool ExecMocker::init(WTSVariant* cfg)
{
	const char* module = cfg->getCString("module");                  // 获取执行器模块路径
	_code = cfg->getCString("code");                                 // 获取合约代码
	_period = cfg->getCString("period");                            // 获取周期
	_volunit = cfg->getDouble("volunit");                           // 获取数量单位
	_volmode = cfg->getInt32("volmode");	//数量模式：0-反复正负，-1-一直卖，+1-一直买  // 获取数量模式

	_matcher.regisSink(this);                                       // 注册撮合引擎回调接口
	_matcher.init(cfg->get("matcher"));                              // 初始化撮合引擎

	DllHandle hInst = DLLHelper::load_library(module);              // 加载执行器模块动态库
	if (hInst == NULL)                                               // 如果加载失败
		return false;                                                 // 返回失败

	FuncCreateExeFact creator = (FuncCreateExeFact)DLLHelper::get_symbol(hInst, "createExecFact");  // 获取创建工厂函数地址
	if (creator == NULL)                                             // 如果获取失败
	{
		DLLHelper::free_library(hInst);                             // 释放动态库
		return false;                                                 // 返回失败
	}

	_factory._module_inst = hInst;                                   // 保存动态库句柄
	_factory._module_path = module;                                   // 保存模块路径
	_factory._creator = creator;                                      // 保存创建函数指针
	_factory._remover = (FuncDeleteExeFact)DLLHelper::get_symbol(hInst, "deleteExecFact");  // 获取删除工厂函数地址
	_factory._fact = _factory._creator();                            // 创建执行器工厂实例

	WTSVariant* cfgExec = cfg->get("executer");                     // 获取执行器配置
	if (cfgExec)                                                     // 如果配置存在
	{
		_exec_unit = _factory._fact->createExeUnit(cfgExec->getCString("name"));  // 创建执行器单元实例
		_exec_unit->init(this, _code.c_str(), cfgExec->get("params"));  // 初始化执行器单元
		_id = cfgExec->getCString("id");                             // 获取执行器ID
	}

	return true;                                                     // 返回成功
}

/**
 * @brief 获取合约信息
 * 
 * @param stdCode 合约代码
 * @return 合约信息指针
 */
WTSCommodityInfo* ExecMocker::getCommodityInfo(const char* stdCode)
{
	return _replayer->get_commodity_info(stdCode);                  // 从回放器获取合约信息
}

/**
 * @brief 获取交易时段信息
 * 
 * @param stdCode 合约代码
 * @return 交易时段信息指针
 */
WTSSessionInfo* ExecMocker::getSessionInfo(const char* stdCode)
{
	return _replayer->get_session_info(stdCode, true);               // 从回放器获取交易时段信息（true表示使用主合约）
}

/**
 * @brief 获取当前时间
 * 
 * @return 当前时间（Unix时间戳）
 */
uint64_t ExecMocker::getCurTime()
{
	return TimeUtils::makeTime(_replayer->get_date(), _replayer->get_raw_time() * 100000 + _replayer->get_secs());  // 构建Unix时间戳
}

/**
 * @brief 处理K线收盘事件
 * 
 * 当前为空实现
 * 
 * @param stdCode 合约代码
 * @param period 周期
 * @param times 周期倍数
 * @param newBar 新的K线数据
 */
void ExecMocker::handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	//throw std::logic_error("The method or operation is not implemented.");  // 未实现
}

/**
 * @brief 处理交易时段开始事件
 * 
 * 当前为空实现
 * 
 * @param curTDate 当前交易日
 */
void ExecMocker::handle_session_begin(uint32_t curTDate)
{
	//throw std::logic_error("The method or operation is not implemented.");  // 未实现
}

/**
 * @brief 处理交易时段结束事件
 * 
 * 清空撮合引擎，重置未完成订单数量，记录统计信息
 * 
 * @param curTDate 当前交易日
 */
void ExecMocker::handle_session_end(uint32_t curTDate)
{
	_matcher.clear();                                                 // 清空撮合引擎
	_undone = 0;                                                     // 重置未完成订单数量

	WTSLogger::info("Total entrust:{}, total quantity:{}, total cancels:{}, total cancel quantity:{}, total signals:{}", 
		_ord_cnt, _ord_qty, _cacl_cnt, _cacl_qty, _sig_cnt);       // 记录统计信息：委托数量、委托总量、撤单数量、撤单总量、信号数量
}

/**
 * @brief 处理tick数据
 * 
 * 更新最新tick数据，触发撮合引擎处理，通知执行器单元
 * 
 * @param stdCode 合约代码
 * @param curTick 当前tick数据
 * @param pxType 价格类型
 */
void ExecMocker::handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType)
{
 	if (_last_tick)                                                  // 如果之前有tick数据
	{
		_last_tick->release();                                       // 释放之前的tick数据引用计数
		_last_tick = NULL;                                           // 清空指针
	}

	_last_tick = curTick;                                            // 保存当前tick数据
	_last_tick->retain();                                            // 增加引用计数
	
	_matcher.handle_tick(stdCode, curTick);                          // 触发撮合引擎处理tick数据

	if (_exec_unit)                                                  // 如果执行器单元存在
		_exec_unit->on_tick(curTick);                                // 通知执行器单元tick数据更新
}

/**
 * @brief 处理初始化事件
 * 
 * 1. 订阅K线数据
 * 2. 订阅tick数据
 * 3. 初始化成交日志表头
 * 4. 通知执行器单元通道就绪
 * 5. 设置初始目标仓位
 */
void ExecMocker::handle_init()
{
	thread_local static char basePeriod[2] = { 0 };                  // 线程局部变量：基础周期
	basePeriod[0] = _period[0];                                      // 获取周期类型（m/d等）
	uint32_t times = 1;                                              // 周期倍数
	if (_period.size() > 1)                                         // 如果周期字符串长度大于1
		times = strtoul(_period.c_str() + 1, NULL, 10);             // 解析周期倍数

	WTSKlineSlice* kline = _replayer->get_kline_slice(_code.c_str(), basePeriod,  10, times, true);  // 获取K线数据（订阅主K线）
	if (kline)                                                       // 如果K线数据存在
		kline->release();                                            // 释放K线数据（已经订阅）

	_replayer->sub_tick(0, _code.c_str());                           // 订阅tick数据（上下文ID为0）

	_trade_logs << "localid,signaltime,ordertime,bs,sigprice,ordprice,lmtprice,tradetime,trdprice,qty,sigtimespan,exectime,cancel" << std::endl;  // 写入成交日志表头

	_exec_unit->on_channel_ready();                                  // 通知执行器单元通道就绪

	_sig_time = (uint64_t)_replayer->get_date() * 10000 + _replayer->get_raw_time();  // 记录初始信号时间

	_exec_unit->set_position(_code.c_str(), _volunit);                // 设置初始目标仓位
	WTSLogger::info("Target position updated at the beginning: {}", _volunit);  // 记录日志
}

/**
 * @brief 处理调度事件
 * 
 * 根据数量模式更新目标仓位，通知执行器单元
 * 
 * @param uDate 日期
 * @param uTime 时间
 */
void ExecMocker::handle_schedule(uint32_t uDate, uint32_t uTime)
{
	if (uTime == 1500)                                                // 如果是15:00（收盘时间）
		return;                                                       // 直接返回，不处理

	_sig_px = _last_tick->price();                                    // 获取当前价格作为信号价格
	if (_sig_px == DBL_MAX || _sig_px == FLT_MAX)                    // 如果价格无效
		_sig_px = _last_tick->preclose();                            // 使用昨收价

	_sig_time = (uint64_t)uDate * 10000 + uTime;                     // 更新信号时间
	if(_volmode == 0)                                                 // 如果数量模式为0（反复正负）
	{
		if (_position <= 0)                                          // 如果当前持仓小于等于0
			_target = _volunit;                                      // 目标仓位设为正数（买入）
		else                                                          // 如果当前持仓大于0
			_target = -_volunit;                                     // 目标仓位设为负数（卖出）
	}
	else if (_volmode == -1)                                          // 如果数量模式为-1（一直卖）
	{
		_target -= _volunit;                                         // 目标仓位减少（持续卖出）
	}
	else if (_volmode == 1)                                           // 如果数量模式为+1（一直买）
	{
		_target += _volunit;                                         // 目标仓位增加（持续买入）
	}

	_exec_unit->set_position(_code.c_str(), _target);                // 通知执行器单元更新目标仓位
	WTSLogger::info("Target position updated @{}.{}: {}", uDate, uTime, _volunit);  // 记录日志（注意：日志中显示的是_volunit，可能是bug）
	_sig_cnt++;                                                       // 增加信号计数
}

/**
 * @brief 获取tick数据切片
 * 
 * @param stdCode 合约代码
 * @param count 数量
 * @param etime 结束时间（可选）
 * @return tick数据切片指针
 */
WTSTickSlice* ExecMocker::getTicks(const char* stdCode, uint32_t count, uint64_t etime /*= 0*/)
{
	return _replayer->get_tick_slice(stdCode, count, etime);         // 从回放器获取tick数据切片
}

/**
 * @brief 获取最新tick数据
 * 
 * @param stdCode 合约代码
 * @return 最新tick数据指针
 */
WTSTickData* ExecMocker::grabLastTick(const char* stdCode)
{
	return _replayer->get_last_tick(stdCode);                        // 从回放器获取最新tick数据
}

/**
 * @brief 获取持仓数量
 * 
 * @param stdCode 合约代码（未使用）
 * @param validOnly 是否只查询有效持仓（未使用）
 * @param flag 标志（未使用）
 * @return 持仓数量
 */
double ExecMocker::getPosition(const char* stdCode, bool validOnly /* = true */, int32_t flag /* = 3 */)
{
	return _position;                                                 // 返回当前持仓
}

/**
 * @brief 获取订单映射表
 * 
 * @param stdCode 合约代码（未使用）
 * @return 订单映射表指针（当前返回NULL）
 */
OrderMap* ExecMocker::getOrders(const char* stdCode)
{
	return NULL;                                                      // 返回NULL（未实现）
}

/**
 * @brief 获取未完成订单数量
 * 
 * @param stdCode 合约代码（未使用）
 * @return 未完成订单数量
 */
double ExecMocker::getUndoneQty(const char* stdCode)
{
	return _undone;                                                   // 返回未完成订单数量
}

/**
 * @brief 买入订单
 * 
 * 通过撮合引擎提交买入订单，更新统计信息
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param bForceClose 是否强制平仓（未使用）
 * @return 订单ID列表
 */
OrderIDs ExecMocker::buy(const char* stdCode, double price, double qty, bool bForceClose /* = false */)
{
	uint64_t curTime = (uint64_t)_replayer->get_date() * 1000000000 + (uint64_t)_replayer->get_raw_time() * 100000 + _replayer->get_secs();  // 构建当前时间（纳秒精度）
	OrderIDs ret = _matcher.buy(stdCode, price, qty, curTime);       // 通过撮合引擎提交买入订单

	if(!ret.empty())                                                 // 如果订单提交成功
	{
		_ord_cnt++;                                                  // 增加订单数量统计
		_ord_qty += qty;                                             // 累加订单数量统计

		_undone += (int32_t)qty;                                     // 增加未完成订单数量（买入为正）
		WTSLogger::info("{}, undone orders updated: {}", __FUNCTION__, _undone);  // 记录日志
	}

	return ret;                                                       // 返回订单ID列表
}

/**
 * @brief 卖出订单
 * 
 * 通过撮合引擎提交卖出订单，更新统计信息
 * 
 * @param stdCode 合约代码
 * @param price 价格
 * @param qty 数量
 * @param bForceClose 是否强制平仓（未使用）
 * @return 订单ID列表
 */
OrderIDs ExecMocker::sell(const char* stdCode, double price, double qty, bool bForceClose /* = false */)
{
	uint64_t curTime = (uint64_t)_replayer->get_date() * 1000000000 + (uint64_t)_replayer->get_raw_time() * 100000 + _replayer->get_secs();  // 构建当前时间（纳秒精度）
	OrderIDs ret = _matcher.sell(stdCode, price, qty, curTime);      // 通过撮合引擎提交卖出订单

	if(!ret.empty())                                                 // 如果订单提交成功
	{
		_ord_cnt++;                                                  // 增加订单数量统计
		_ord_qty += qty;                                             // 累加订单数量统计
	
		_undone -= (int32_t)qty;                                     // 减少未完成订单数量（卖出为负）
		WTSLogger::info("{}, undone orders updated: {}", __FUNCTION__, _undone);  // 记录日志
	}

	return ret;                                                       // 返回订单ID列表
}

/**
 * @brief 撤单（按订单ID）
 * 
 * 通过撮合引擎撤单，更新统计信息
 * 
 * @param localid 本地订单ID
 * @return 是否成功
 */
bool ExecMocker::cancel(uint32_t localid)
{
	double change = _matcher.cancel(localid);                        // 通过撮合引擎撤单，返回订单数量变化（正数表示买入，负数表示卖出）
	if (decimal::eq(change, 0))                                      // 如果变化为0（订单不存在或已撤单）
		return false;                                                 // 返回失败

	_undone -= change;                                                // 更新未完成订单数量（买入撤单减少，卖出撤单增加）
	_cacl_cnt++;                                                      // 增加撤单数量统计
	_cacl_qty += abs(change);                                         // 累加撤单数量统计
	WTSLogger::info("{}, undone orders updated: {}", __FUNCTION__, _undone);  // 记录日志

	return true;                                                      // 返回成功
}

/**
 * @brief 撤单（按合约和方向）
 * 
 * 通过撮合引擎撤单，使用回调函数更新统计信息
 * 
 * @param stdCode 合约代码
 * @param isBuy 是否买入
 * @param qty 数量（0表示全部）
 * @return 订单ID列表
 */
OrderIDs ExecMocker::cancel(const char* stdCode, bool isBuy, double qty /*= 0*/)
{
	OrderIDs ret = _matcher.cancel(stdCode, isBuy, qty, [this](double change) {  // 通过撮合引擎撤单，使用lambda回调
		_undone -= change;                                            // 更新未完成订单数量

		_cacl_cnt++;                                                  // 增加撤单数量统计
		_cacl_qty += abs(change);                                     // 累加撤单数量统计
	});
	WTSLogger::info("{}, undone orders updated: {}", __FUNCTION__, _undone);  // 记录日志

	return ret;                                                       // 返回订单ID列表
}

/**
 * @brief 写日志
 * 
 * @param message 日志消息
 */
void ExecMocker::writeLog(const char* message)
{
	WTSLogger::log_dyn_raw("executer", _id.c_str(), LL_INFO, message);  // 记录执行器日志
}

/**
 * @brief 处理委托回报
 * 
 * 通知执行器单元委托回报
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param bSuccess 是否成功
 * @param message 消息
 * @param ordTime 订单时间
 */
void ExecMocker::handle_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, uint64_t ordTime)
{
	_exec_unit->on_entrust(localid, stdCode, bSuccess, message);     // 通知执行器单元委托回报
}

/**
 * @brief 处理订单回报
 * 
 * 记录订单日志（包括撤单情况），更新未完成订单数量，通知执行器单元
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy 是否买入
 * @param leftover 剩余数量
 * @param price 订单价格
 * @param isCanceled 是否已撤单
 * @param ordTime 订单时间
 */
void ExecMocker::handle_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled, uint64_t ordTime)
{
	uint64_t curTime = (uint64_t)_last_tick->actiondate() * 1000000000 + _last_tick->actiontime();  // 获取当前时间（纳秒精度）
	uint64_t curUnixTime = TimeUtils::makeTime(_last_tick->actiondate(), _last_tick->actiontime());  // 获取当前Unix时间戳

	uint64_t sigUnixTime = TimeUtils::makeTime((uint32_t)(_sig_time / 10000), _sig_time % 10000 * 100000);  // 计算信号Unix时间戳
	uint64_t ordUnixTime = TimeUtils::makeTime((uint32_t)(ordTime / 1000000000), ordTime % 1000000000);  // 计算订单Unix时间戳

	if(isCanceled)                                                    // 如果是撤单
	{
		if (_sig_px == DBL_MAX)                                      // 如果信号价格无效
			_sig_px = _last_tick->preclose();                        // 使用昨收价

		_trade_logs << localid << ","                                // 记录订单日志：本地订单ID
			<< _sig_time << ","                                      // 信号时间
			<< ordTime << ","                                        // 订单时间
			<< (isBuy ? "B" : "S") << ","                           // 买卖方向
			<< _sig_px << ","                                        // 信号价格
			<< 0 << ","                                               // 订单价格（撤单时为0）
			<< price << ","                                          // 限价
			<< curTime << ","                                        // 成交时间
			<< price << ","                                          // 成交价格
			<< 0 << ","                                               // 成交数量（撤单时为0）
			<< curUnixTime - sigUnixTime << ","                      // 信号到当前的时间差
			<< curUnixTime - ordUnixTime << ","                      // 订单到当前的时间差
			<< "true" << std::endl;                                  // 撤单标志

		_undone -= leftover * (isBuy ? 1 : -1);                     // 更新未完成订单数量（买入撤单减少，卖出撤单增加）
		WTSLogger::info("{}, undone orders updated: {}", __FUNCTION__, _undone);  // 记录日志
	}

	_exec_unit->on_order(localid, stdCode, isBuy, leftover, price, isCanceled);  // 通知执行器单元订单回报
}

/**
 * @brief 处理成交回报
 * 
 * 记录成交日志，更新持仓和未完成订单数量，通知执行器单元
 * 
 * @param localid 本地订单ID
 * @param stdCode 合约代码
 * @param isBuy 是否买入
 * @param vol 成交数量
 * @param fireprice 触发价格
 * @param price 成交价格
 * @param ordTime 订单时间
 */
void ExecMocker::handle_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double fireprice, double price, uint64_t ordTime)
{
	uint64_t curTime = (uint64_t)_last_tick->actiondate() * 1000000000 + _last_tick->actiontime();  // 获取当前时间（纳秒精度）
	uint64_t curUnixTime = TimeUtils::makeTime(_last_tick->actiondate(), _last_tick->actiontime());  // 获取当前Unix时间戳

	uint64_t sigUnixTime = TimeUtils::makeTime((uint32_t)(_sig_time / 10000), _sig_time % 10000 * 100000);  // 计算信号Unix时间戳
	uint64_t ordUnixTime = TimeUtils::makeTime((uint32_t)(ordTime / 1000000000), ordTime % 1000000000);  // 计算订单Unix时间戳

	if (_sig_px == DBL_MAX)                                          // 如果信号价格无效
		_sig_px = _last_tick->preclose();                            // 使用昨收价

	_trade_logs << localid << ","                                    // 记录成交日志：本地订单ID
		<< _sig_time << ","                                          // 信号时间
		<< ordTime << ","                                            // 订单时间
		<< (isBuy?"B":"S") << ","                                    // 买卖方向
		<< _sig_px << ","                                            // 信号价格
		<< fireprice << ","                                          // 触发价格
		<< price << ","                                              // 成交价格
		<< curTime << ","                                            // 成交时间
		<< price << ","                                              // 成交价格（重复，可能是历史兼容）
		<< vol << ","                                                // 成交数量
		<< curUnixTime - sigUnixTime << ","                          // 信号到当前的时间差
		<< curUnixTime - ordUnixTime << ","                          // 订单到当前的时间差
		<< "false" << std::endl;                                     // 撤单标志（false表示成交）

	_position += vol* (isBuy?1:-1);                                  // 更新持仓（买入增加，卖出减少）
	_undone -= vol * (isBuy ? 1 : -1);                               // 更新未完成订单数量（买入成交减少，卖出成交增加）
	WTSLogger::info("{}, undone orders updated: {}", __FUNCTION__, _undone);  // 记录日志
	WTSLogger::info("Position updated: {}", _position);              // 记录持仓更新日志

	_exec_unit->on_trade(localid, stdCode, isBuy, vol, price);       // 通知执行器单元成交回报
}

/**
 * @brief 处理回放完成事件
 * 
 * 输出成交日志到CSV文件
 */
void ExecMocker::handle_replay_done()
{
	std::string folder = WtHelper::getOutputDir();                  // 获取输出目录
	folder += "exec/";                                                // 添加执行器子目录
	boost::filesystem::create_directories(folder.c_str());           // 创建目录（如果不存在）

	std::stringstream ss;                                             // 创建字符串流
	ss << folder << "trades_" << _id << ".csv";                     // 构建文件名（trades_执行器ID.csv）
	std::string filename = ss.str();                                 // 获取文件名字符串
	StdFile::write_file_content(filename.c_str(), _trade_logs.str()); // 写入成交日志到文件
}