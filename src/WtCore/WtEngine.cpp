/*!
 * \file WtEngine.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易引擎基类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件实现了WtEngine类的所有方法，提供交易引擎的核心功能。
 * 
 * 实现要点：
 * 1. 构造函数和初始化：初始化时间、加载配置、初始化各种管理器
 * 2. 时间管理：设置和获取当前日期、时间、交易日等
 * 3. 数据访问：提供合约信息、行情数据、K线数据等访问接口
 * 4. 价格管理：获取当前价格、当日价格，支持复权处理
 * 5. 持仓管理：管理组合持仓，包括持仓明细、盈亏计算、持仓更新
 * 6. 资金管理：管理组合资金，包括余额、盈亏、手续费、浮动盈亏更新
 * 7. 信号处理：处理策略发出的交易信号，支持延迟触发机制
 * 8. 风控管理：初始化风控模块，支持仓位缩放等风控功能
 * 9. 任务调度：使用后台线程处理持仓更新、资金更新等耗时操作
 * 10. 日志记录：记录成交记录、平仓记录、资金日志等
 * 11. 数据持久化：保存和加载资金数据、持仓数据、风控参数等
 * 
 * 关键算法：
 * - 持仓更新：根据目标仓位和当前仓位，计算差量，更新持仓明细
 * - 盈亏计算：按持仓明细计算浮动盈亏和平仓盈亏
 * - 信号延迟：等待下一个Tick触发信号，确保价格一致性
 * - 资金更新：定期更新浮动盈亏，避免频繁计算
 */
#include "WtEngine.h"  // 包含引擎头文件
#include "WtDtMgr.h"  // 包含数据管理器头文件
#include "WtHelper.h"  // 包含辅助工具头文件

#include "../Share/TimeUtils.hpp"  // 包含时间工具头文件
#include "../Share/StrUtil.hpp"  // 包含字符串工具头文件
#include "../Share/decimal.h"  // 包含小数运算工具头文件
#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具头文件

#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息头文件
#include "../Includes/WTSSessionInfo.hpp"  // 包含会话信息头文件
#include "../Includes/WTSVariant.hpp"  // 包含变体类型头文件

#include "../Includes/WTSDataDef.hpp"  // 包含数据定义头文件
#include "../Includes/WTSRiskDef.hpp"  // 包含风控定义头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志工具头文件
#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置加载器头文件

#include <rapidjson/document.h>  // 包含RapidJSON文档头文件
#include <rapidjson/prettywriter.h>  // 包含RapidJSON美化写入器头文件
namespace rj = rapidjson;  // RapidJSON命名空间别名


USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 构造函数
 * 
 * 初始化引擎，设置默认时间，初始化成员变量。
 * 从系统获取当前日期和时间，初始化时间相关成员。
 */
WtEngine::WtEngine()
	: _port_fund(NULL)  // 初始化组合资金信息指针为NULL
	, _risk_volscale(1.0)  // 初始化风控仓位缩放系数为1.0
	, _risk_date(0)  // 初始化风控参数生效日期为0
	, _terminated(false)  // 初始化终止标志为false
	, _evt_listener(NULL)  // 初始化事件监听器指针为NULL
	, _adapter_mgr(NULL)  // 初始化交易适配器管理器指针为NULL
	, _notifier(NULL)  // 初始化事件通知器指针为NULL
	, _fund_udt_span(0)  // 初始化组合资金更新时间间隔为0（不限制）
	, _ready(false)  // 初始化就绪标志为false
{
	TimeUtils::getDateTime(_cur_date, _cur_time);  // 从系统获取当前日期和时间
	_cur_secs = _cur_time % 100000;  // 提取秒数部分（后5位）
	_cur_time /= 100000;  // 提取分钟时间部分（前部分）
	_cur_raw_time = _cur_time;  // 设置真实时间为分钟时间
	_cur_tdate = _cur_date;  // 设置交易日为当前日期

	WtHelper::setTime(_cur_date, _cur_time, _cur_secs);  // 设置辅助工具的时间信息
}

/**
 * @brief 设置当前日期和时间
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMM）
 * @param curSecs 当前秒数（包含毫秒，格式：SSmmm），默认为0
 * @param rawTime 当前真实时间（格式：HHMM），默认为0（使用curTime）
 * 
 * 设置引擎的当前日期和时间信息。
 */
void WtEngine::set_date_time(uint32_t curDate, uint32_t curTime, uint32_t curSecs /* = 0 */, uint32_t rawTime /* = 0 */)
{
	_cur_date = curDate;  // 设置当前日期
	_cur_time = curTime;  // 设置当前分钟时间
	_cur_secs = curSecs;  // 设置当前秒数

	if (rawTime == 0)  // 如果真实时间未指定
		rawTime = curTime;  // 使用当前分钟时间作为真实时间

	_cur_raw_time = rawTime;  // 设置真实时间

	WtHelper::setTime(_cur_date, _cur_raw_time, _cur_secs);  // 设置辅助工具的时间信息
}

/**
 * @brief 设置当前交易日
 * @param curTDate 当前交易日（格式：YYYYMMDD）
 * 
 * 设置引擎的当前交易日信息。
 */
void WtEngine::set_trading_date(uint32_t curTDate)
{
	_cur_tdate = curTDate;  // 设置当前交易日

	WtHelper::setTDate(curTDate);  // 设置辅助工具的交易日期
}

/**
 * @brief 获取品种信息
 * @param stdCode 标准合约代码字符串
 * @return WTSCommodityInfo* 返回品种信息指针，如果不存在返回NULL
 * 
 * 根据标准合约代码获取品种信息。
 * 先解析合约代码，提取交易所和品种信息，然后从基础数据管理器获取品种信息。
 */
WTSCommodityInfo* WtEngine::get_commodity_info(const char* stdCode)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码，提取交易所、品种等信息
	return _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 从基础数据管理器获取品种信息（参数：交易所代码、品种代码）
}

/**
 * @brief 获取合约信息
 * @param stdCode 标准合约代码字符串
 * @return WTSContractInfo* 返回合约信息指针，如果不存在返回NULL
 * 
 * 根据标准合约代码获取合约信息。
 * 先解析合约代码，提取合约代码和交易所信息，然后从基础数据管理器获取合约信息。
 */
WTSContractInfo* WtEngine::get_contract_info(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码，提取合约代码、交易所等信息
	return _base_data_mgr->getContract(cInfo._code, cInfo._exchg);  // 从基础数据管理器获取合约信息（参数：合约代码、交易所代码）
}

/**
 * @brief 获取原始合约代码
 * @param stdCode 标准合约代码字符串
 * @return std::string 返回原始合约代码字符串，如果不存在返回空字符串
 * 
 * 对于主力合约代码（如SHFE.ag.HOT），转换为实际合约代码（如SHFE.ag.1912）。
 * 如果合约代码不包含规则标签，则返回空字符串。
 */
std::string WtEngine::get_rawcode(const char* stdCode)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	if (cInfo.hasRule())  // 如果合约代码包含规则标签（如.HOT）
	{
		std::string code = _hot_mgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID(), _cur_tdate);  // 从热点管理器获取自定义原始合约代码（根据规则标签、标准品种ID和当前交易日）
		return CodeHelper::rawMonthCodeToStdCode(code.c_str(), cInfo._exchg);  // 将原始月份代码转换为标准合约代码
	}

	return "";  // 如果没有规则标签，返回空字符串
}

/**
 * @brief 获取交易会话信息
 * @param sid 会话ID或合约代码字符串
 * @param isCode 是否为合约代码，true表示是合约代码，false表示是会话ID，默认为false
 * @return WTSSessionInfo* 返回交易会话信息指针，如果不存在返回NULL
 * 
 * 根据会话ID或合约代码获取交易会话信息。
 * 如果isCode为false，直接使用sid作为会话ID查询；如果isCode为true，先解析合约代码获取品种信息，再获取会话信息。
 */
WTSSessionInfo* WtEngine::get_session_info(const char* sid, bool isCode /* = false */)
{
	if (!isCode)  // 如果不是合约代码（是会话ID）
		return _base_data_mgr->getSession(sid);  // 直接从基础数据管理器获取会话信息

	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(sid, _hot_mgr);  // 解析标准合约代码，提取交易所、品种等信息
	WTSCommodityInfo* cInfo = _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 从基础数据管理器获取品种信息
	if (cInfo == NULL)  // 如果品种信息不存在
		return NULL;  // 返回NULL

	return _base_data_mgr->getSession(cInfo->getSession());  // 从基础数据管理器获取会话信息（使用品种信息中的会话ID）
}

/**
 * @brief Tick事件处理
 * @param stdCode 标准合约代码字符串
 * @param curTick 当前Tick数据指针
 * 
 * 当有新的Tick数据时被调用。
 * 更新价格缓存，检查是否需要触发信号，更新持仓浮动盈亏。
 * 
 * 实现逻辑：
 * 1. 更新价格缓存
 * 2. 检查是否有待触发的信号，如果在交易时间内则触发
 * 3. 如果成交量为0，直接返回（价格不会变动）
 * 4. 推送任务更新持仓浮动盈亏
 * 5. 推送任务更新资金浮动盈亏
 */
void WtEngine::on_tick(const char* stdCode, WTSTickData* curTick)
{
	_price_map[stdCode] = curTick->price();  // 更新价格缓存（合约代码 -> 最新价格）

	//先检查是否要信号要触发
	{
		bool bTriggered = false;  // 信号触发标志
		auto it = _sig_map.find(stdCode);  // 在信号映射表中查找合约代码
		if (it != _sig_map.end())  // 如果找到待触发的信号
		{
			WTSSessionInfo* sInfo = get_session_info(stdCode, true);  // 获取交易会话信息（isCode=true表示stdCode是合约代码）

			if (sInfo->isInTradingTime(_cur_raw_time, true))  // 如果当前时间在交易时间内（第二个参数true表示包含夜盘）
			{
				const SigInfo& sInfo = it->second;  // 获取信号信息（注意：这里变量名与上面的会话信息重复，但作用域不同）
				double pos = sInfo._volume;  // 获取目标仓位
				std::string code = stdCode;  // 保存合约代码字符串（用于lambda捕获）
				do_set_position(code.c_str(), pos, curTick->price());  // 设置持仓（使用当前Tick价格）
				_sig_map.erase(it);  // 从信号映射表中删除已触发的信号
				bTriggered = true;  // 设置触发标志为true
			}

		}

		if(bTriggered)  // 如果信号已触发
			save_datas();  // 保存数据（持久化持仓和资金信息）
	}

	//如果成交量为0，价格也不会有变动
	if (curTick->volume() == 0)  // 如果成交量为0（价格不变动）
		return;  // 直接返回，不更新持仓浮动盈亏

	std::string code = stdCode;  // 保存合约代码字符串（用于lambda捕获）
	double price = curTick->price();  // 保存价格（用于lambda捕获）
	push_task([this, code, price]{  // 推送任务到后台线程（更新持仓浮动盈亏）
		auto it = _pos_map.find(code);  // 在持仓映射表中查找合约代码
		if (it == _pos_map.end())  // 如果未找到持仓信息
			return;  // 直接返回

		PosInfoPtr& pInfo = it->second;  // 获取持仓信息指针
		SpinLock lock(pInfo->_mtx);  // 获取自旋锁（保护持仓数据的线程安全）
		if (pInfo->_volume == 0)  // 如果持仓为0
		{
			pInfo->_dynprofit = 0;  // 浮动盈亏设为0
		}
		else  // 如果持仓不为0
		{
			WTSCommodityInfo* commInfo = get_commodity_info(code.c_str());  // 获取品种信息（用于获取合约乘数）
			double dynprofit = 0;  // 总浮动盈亏
			for (auto pit = pInfo->_details.begin(); pit != pInfo->_details.end(); pit++)  // 遍历持仓明细
			{
				DetailInfo& dInfo = *pit;  // 获取持仓明细引用
				dInfo._profit = dInfo._volume*(price - dInfo._price)*commInfo->getVolScale()*(dInfo._long ? 1 : -1);  // 计算单笔明细的浮动盈亏：数量*(当前价格-开仓价格)*合约乘数*方向（多仓为1，空仓为-1）
				dynprofit += dInfo._profit;  // 累加总浮动盈亏
			}

			pInfo->_dynprofit = dynprofit;  // 更新持仓总浮动盈亏
		}
	});

	push_task([this]() {  // 推送任务到后台线程（更新资金浮动盈亏）
		update_fund_dynprofit();  // 更新资金浮动盈亏
	});
}

/**
 * @brief 更新资金浮动盈亏
 * 
 * 计算所有持仓的浮动盈亏，更新组合资金的浮动盈亏。
 * 同时更新最大最小动态余额等统计信息。
 * 
 * 实现逻辑：
 * 1. 检查是否已经结算（上次结算日期等于当前交易日则不再更新）
 * 2. 检查更新时间间隔（如果设置了间隔，则检查是否到达更新时间）
 * 3. 累加所有持仓的浮动盈亏
 * 4. 更新组合资金的浮动盈亏
 * 5. 更新最大最小动态余额和时间戳
 * 6. 更新最大最小月度动态余额
 */
void WtEngine::update_fund_dynprofit()
{
	WTSFundStruct& fundInfo = _port_fund->fundInfo();  // 获取组合资金信息引用
	if (fundInfo._last_date == _cur_tdate)  // 如果上次结算日期等于当前交易日
	{
		//上次结算日期等于当前交易日,说明已经结算,不再更新了
		return;  // 直接返回，不更新浮动盈亏
	}

	int64_t now = TimeUtils::getLocalTimeNow();  // 获取当前本地时间（毫秒时间戳）
	if(_fund_udt_span != 0)  // 如果设置了更新时间间隔（不为0）
	{
		if (now - fundInfo._update_time < _fund_udt_span * 1000)  // 如果距离上次更新时间未超过间隔（转换为毫秒）
			return;  // 直接返回，不更新浮动盈亏
	}

	double profit = 0.0;  // 总浮动盈亏
	for(const auto& v : _pos_map)  // 遍历所有持仓
	{
		const PosInfoPtr& pItem = v.second;  // 获取持仓信息指针
		profit += pItem->_dynprofit;  // 累加持仓浮动盈亏
	}

	fundInfo._dynprofit = profit;  // 更新组合资金的浮动盈亏
	double dynbal = fundInfo._balance + profit;  // 计算动态余额（余额+浮动盈亏）
	if (fundInfo._max_dyn_bal == DBL_MAX || decimal::gt(dynbal, fundInfo._max_dyn_bal))  // 如果动态余额大于最大动态余额（DBL_MAX表示未初始化）
	{
		fundInfo._max_dyn_bal = dynbal;  // 更新最大动态余额
		fundInfo._max_time = _cur_raw_time * 100000 + _cur_secs;  // 更新最大动态余额时间戳（分钟时间*100000 + 秒数）
	}

	if (fundInfo._min_dyn_bal == DBL_MAX || decimal::lt(dynbal, fundInfo._min_dyn_bal))  // 如果动态余额小于最小动态余额（DBL_MAX表示未初始化）
	{
		fundInfo._min_dyn_bal = dynbal;  // 更新最小动态余额
		fundInfo._min_time = _cur_raw_time * 100000 + _cur_secs;;  // 更新最小动态余额时间戳（分钟时间*100000 + 秒数）
	}

	double dynbalance = fundInfo._balance + profit;  // 计算动态余额（用于月度统计）
	if (fundInfo._max_md_dyn_bal._date == 0 || decimal::gt(dynbalance, fundInfo._max_md_dyn_bal._dyn_balance))  // 如果动态余额大于最大月度动态余额（date为0表示未初始化）
	{
		fundInfo._max_md_dyn_bal._dyn_balance = dynbalance;  // 更新最大月度动态余额
		fundInfo._max_md_dyn_bal._date = _cur_tdate;  // 更新最大月度动态余额日期
	}

	if (fundInfo._min_md_dyn_bal._date == 0 || decimal::lt(dynbalance, fundInfo._min_md_dyn_bal._dyn_balance))  // 如果动态余额小于最小月度动态余额（date为0表示未初始化）
	{
		fundInfo._min_md_dyn_bal._dyn_balance = dynbalance;  // 更新最小月度动态余额
		fundInfo._min_md_dyn_bal._date = _cur_tdate;  // 更新最小月度动态余额日期
	}

	fundInfo._update_time = now;  // 更新最后更新时间
}

/**
 * @brief 写入风控日志
 * @param message 日志消息字符串
 * 
 * 记录风控相关的日志信息。
 * 使用线程本地缓冲区，避免频繁分配内存。
 */
void WtEngine::writeRiskLog(const char* message)
{
	static thread_local char szBuf[2048] = { 0 };  // 线程本地静态缓冲区（用于格式化日志消息）
	auto len = wt_strcpy(szBuf, "[RiskControl] ");  // 复制风控日志前缀到缓冲区，返回复制的长度
	wt_strcpy(szBuf + len, message);  // 复制日志消息到缓冲区（追加到前缀后面）
	WTSLogger::log_raw_by_cat("risk", LL_INFO, szBuf);  // 记录日志（使用"risk"分类，INFO级别）
}

/**
 * @brief 获取当前日期
 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
 * 
 * 返回引擎的当前日期。
 */
uint32_t WtEngine::getCurDate()
{
	return _cur_date;  // 返回当前日期
}

/**
 * @brief 获取当前时间
 * @return uint32_t 返回当前时间（格式：HHMM）
 * 
 * 返回引擎的当前分钟时间。
 */
uint32_t WtEngine::getCurTime()
{
	return _cur_time;  // 返回当前分钟时间
}

/**
 * @brief 获取交易日
 * @return uint32_t 返回交易日（格式：YYYYMMDD）
 * 
 * 返回引擎的当前交易日。
 */
uint32_t WtEngine::getTradingDate()
{
	return _cur_tdate;  // 返回当前交易日
}

/**
 * @brief 判断是否在交易中
 * @return bool 返回是否在交易中（引擎基类返回false，子类可重写）
 * 
 * 引擎基类返回false，子类可以重写此方法来实现具体的交易状态判断。
 */
bool WtEngine::isInTrading()
{
	return false;  // 引擎基类返回false
}

/**
 * @brief 设置仓位缩放系数
 * @param scale 缩放系数（大于0）
 * 
 * 设置风控仓位缩放系数，用于控制仓位大小。
 * 记录日志并保存数据。
 */
void WtEngine::setVolScale(double scale)
{
	double oldScale = _risk_volscale;  // 保存旧的缩放系数
	_risk_volscale = scale;  // 设置新的缩放系数
	_risk_date = _cur_tdate;  // 记录缩放系数生效日期

	WTSLogger::log_by_cat("risk", LL_INFO, "Position risk scale updated: {} - > {}", oldScale, scale);  // 记录日志：仓位风险缩放系数已更新
	save_datas();  // 保存数据（持久化风控参数）
}

/**
 * @brief 获取组合资金信息
 * @return WTSPortFundInfo* 返回组合资金信息指针
 * 
 * 获取组合的资金信息，包括余额、盈亏、手续费等。
 * 会自动更新浮动盈亏并保存数据。
 */
WTSPortFundInfo* WtEngine::getFundInfo()
{
	update_fund_dynprofit();  // 更新资金浮动盈亏
	save_datas();  // 保存数据

	return _port_fund;  // 返回组合资金信息指针
}

/**
 * @brief 初始化引擎
 * @param cfg 配置参数指针
 * @param bdMgr 基础数据管理器指针
 * @param dataMgr 数据管理器指针
 * @param hotMgr 热点合约管理器指针
 * @param notifier 事件通知器指针
 * 
 * 初始化引擎，加载配置，初始化各种管理器。
 * 
 * 实现逻辑：
 * 1. 保存各种管理器指针
 * 2. 设置过滤器管理器的通知器
 * 3. 加载过滤器配置
 * 4. 加载手续费模板
 * 5. 加载历史数据（资金、持仓、风控参数）
 * 6. 初始化输出文件（成交记录、平仓记录）
 * 7. 初始化风控模块（如果配置了）
 */
void WtEngine::init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier)
{
	_base_data_mgr = bdMgr;  // 保存基础数据管理器指针
	_data_mgr = dataMgr;  // 保存数据管理器指针
	_hot_mgr = hotMgr;  // 保存热点合约管理器指针
	_notifier = notifier;  // 保存事件通知器指针

	WTSLogger::info("Running mode: Production");  // 记录日志：运行模式为生产模式

	_filter_mgr.set_notifier(notifier);  // 设置过滤器管理器的通知器

	_filter_mgr.load_filters(cfg->getCString("filters"));  // 加载过滤器配置（从配置中获取"filters"键的值）

	load_fees(cfg->getCString("fees"));  // 加载手续费模板（从配置中获取"fees"键的值）

	load_datas();  // 加载历史数据（资金、持仓、风控参数）

	init_outputs();  // 初始化输出文件（成交记录、平仓记录）

	WTSVariant* cfgRisk = cfg->get("riskmon");  // 获取风控配置（从配置中获取"riskmon"子配置）
	if(cfgRisk)  // 如果配置了风控模块
	{
		init_riskmon(cfgRisk);  // 初始化风控模块
	}
	else  // 如果未配置风控模块
	{
		//如果没有配置风控线程，则需要自己更新浮动盈亏
		//把更新时间间隔设置为5s
		_fund_udt_span = 5;  // 设置资金更新时间间隔为5秒（避免频繁更新）
		WTSLogger::log_raw(LL_WARN, "RiskMon is not configured, portfilio fund will be updated every 5s");  // 记录警告日志：未配置风控模块，组合资金将每5秒更新一次
	}
}

/**
 * @brief 交易会话结束事件处理
 * 
 * 当交易会话结束时被调用。
 * 进行资金结算，记录资金日志。
 * 
 * 实现逻辑：
 * 1. 检查是否需要结算（上次结算日期小于当前交易日）
 * 2. 打开或创建资金日志文件
 * 3. 写入资金记录（包括日期、余额、盈亏、手续费等）
 * 4. 更新资金信息（上次结算日期、预余额等）
 * 5. 重置统计信息（最大最小动态余额等）
 */
void WtEngine::on_session_end()
{
	//资金结算
	WTSFundStruct& fundInfo = _port_fund->fundInfo();  // 获取组合资金信息引用
	if (fundInfo._last_date < _cur_tdate)  // 如果上次结算日期小于当前交易日（需要结算）
	{
		std::string filename = WtHelper::getPortifolioDir();  // 获取组合目录路径
		filename += "funds.csv";  // 拼接资金日志文件名
		BoostFilePtr fund_log(new BoostFile());  // 创建文件对象智能指针
		{
			bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件（不存在）
			fund_log->create_or_open_file(filename.c_str());  // 创建或打开文件
			if (isNewFile)  // 如果是新文件
			{
				fund_log->write_file("date,predynbalance,prebalance,balance,closeprofit,dynprofit,fee,maxdynbalance,maxtime,mindynbalance,mintime,mdmaxbalance,mdmaxdate,mdminbalance,mdmindate\n");  // 写入CSV表头
			}
			else  // 如果是已存在的文件
			{
				fund_log->seek_to_end();  // 定位到文件末尾（追加模式）
			}
		}

		//可能这里还需要写一条资金记录
		//date,predynbalance,prebalance,balance,closeprofit,dynprofit,fee,maxdynbalance,maxtime,mindynbalance,mintime,mdmaxbalance,mdmaxdate,mdminbalance,mdmindate
		fund_log->write_file(fmt::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n",  // 格式化并写入资金记录
			_cur_tdate, fundInfo._predynbal, fundInfo._prebalance, fundInfo._balance,  // 日期、预动态余额、预余额、余额
			fundInfo._profit, fundInfo._dynprofit, fundInfo._fees, fundInfo._max_dyn_bal,  // 平仓盈亏、浮动盈亏、手续费、最大动态余额
			fundInfo._max_time, fundInfo._min_dyn_bal, fundInfo._min_time,  // 最大动态余额时间、最小动态余额、最小动态余额时间
			fundInfo._max_md_dyn_bal._dyn_balance, fundInfo._max_md_dyn_bal._date,  // 最大月度动态余额、最大月度动态余额日期
			fundInfo._min_md_dyn_bal._dyn_balance, fundInfo._min_md_dyn_bal._date));  // 最小月度动态余额、最小月度动态余额日期

		fundInfo._last_date = _cur_tdate;  // 更新上次结算日期为当前交易日
		fundInfo._predynbal = fundInfo._balance + fundInfo._dynprofit;  // 更新预动态余额（余额+浮动盈亏）
		fundInfo._prebalance = fundInfo._balance;  // 更新预余额（当前余额）
		fundInfo._profit = 0;  // 重置平仓盈亏为0
		fundInfo._fees = 0;  // 重置手续费为0
		fundInfo._max_dyn_bal = DBL_MAX;  // 重置最大动态余额为DBL_MAX（未初始化）
		fundInfo._min_dyn_bal = DBL_MAX;  // 重置最小动态余额为DBL_MAX（未初始化）
		fundInfo._max_time = 0;  // 重置最大动态余额时间为0
		fundInfo._min_time = 0;  // 重置最小动态余额时间为0
	}
	save_datas();  // 保存数据（持久化资金和持仓信息）
}

/**
 * @brief 交易会话开始事件处理
 * 
 * 当交易会话开始时被调用。
 * 引擎基类为空实现，子类可重写。
 */
void WtEngine::on_session_begin()
{

}

/**
 * @brief 保存数据
 * 
 * 将当前数据保存到文件，包括资金数据、持仓数据、风控参数等。
 * 使用JSON格式保存。
 * 
 * 实现逻辑：
 * 1. 创建JSON文档对象
 * 2. 保存资金数据（余额、盈亏、手续费、统计信息等）
 * 3. 保存持仓数据（持仓明细、盈亏等）
 * 4. 保存风控参数（缩放系数、生效日期）
 * 5. 将JSON对象序列化为字符串并写入文件
 * 
 * 一个完整的JSON例子：
 * 
 * {
 *   "fund": {
 *     "predynbal": 1000000,
 *     "balance": 1000000,
 *     "prebalance": 1000000,
 *     "profit": 0,
 *     "dynprofit": 0,
 *   }
 *   "positions": [
 *     {
 *       "code": "CFFEX.IF",
 *       "volume": 10,
 *       "closeprofit": 0,
 *       "dynprofit": 0,
 *     }
 *   ]
 * }
 * 
 * 
 */
void WtEngine::save_datas()
{
	rj::Document root(rj::kObjectType);  // 创建JSON文档对象（根对象）
	rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

	if (_port_fund != NULL)  // 如果组合资金信息存在
	{//保存资金数据
		const WTSFundStruct& fundInfo = _port_fund->fundInfo();  // 获取组合资金信息常量引用
		rj::Value jFund(rj::kObjectType);  // 创建资金JSON对象
		jFund.AddMember("predynbal", fundInfo._predynbal, allocator);  // 添加预动态余额字段
		jFund.AddMember("balance", fundInfo._balance, allocator);  // 添加余额字段
		jFund.AddMember("prebalance", fundInfo._prebalance, allocator);  // 添加预余额字段
		jFund.AddMember("profit", fundInfo._profit, allocator);  // 添加平仓盈亏字段
		jFund.AddMember("dynprofit", fundInfo._dynprofit, allocator);  // 添加浮动盈亏字段
		jFund.AddMember("fees", fundInfo._fees, allocator);  // 添加手续费字段

		jFund.AddMember("max_dyn_bal", fundInfo._max_dyn_bal, allocator);  // 添加最大动态余额字段
		jFund.AddMember("max_time", fundInfo._max_time, allocator);  // 添加最大动态余额时间字段
		jFund.AddMember("min_dyn_bal", fundInfo._min_dyn_bal, allocator);  // 添加最小动态余额字段
		jFund.AddMember("min_time", fundInfo._min_time, allocator);  // 添加最小动态余额时间字段

		jFund.AddMember("last_date", fundInfo._last_date, allocator);  // 添加上次结算日期字段
		jFund.AddMember("date", _cur_tdate, allocator);  // 添加当前交易日字段

		rj::Value jMaxMD(rj::kObjectType);  // 创建最大月度动态余额JSON对象
		jMaxMD.AddMember("date", fundInfo._max_md_dyn_bal._date, allocator);  // 添加日期字段
		jMaxMD.AddMember("dyn_balance", fundInfo._max_md_dyn_bal._dyn_balance, allocator);  // 添加动态余额字段

		rj::Value jMinMD(rj::kObjectType);  // 创建最小月度动态余额JSON对象
		jMinMD.AddMember("date", fundInfo._min_md_dyn_bal._date, allocator);  // 添加日期字段
		jMinMD.AddMember("dyn_balance", fundInfo._min_md_dyn_bal._dyn_balance, allocator);  // 添加动态余额字段

		jFund.AddMember("maxmd", jMaxMD, allocator);  // 将最大月度动态余额对象添加到资金对象
		jFund.AddMember("minmd", jMinMD, allocator);  // 将最小月度动态余额对象添加到资金对象

		jFund.AddMember("update_time", fundInfo._update_time, allocator);  // 添加更新时间字段

		root.AddMember("fund", jFund, allocator);  // 将资金对象添加到根对象
	}

	{//持仓数据保存
		rj::Value jPos(rj::kArrayType);  // 创建持仓JSON数组

		for (auto it = _pos_map.begin(); it != _pos_map.end(); it++)  // 遍历所有持仓
		{
			const char* stdCode = it->first.c_str();  // 获取合约代码
			const PosInfoPtr& pInfo = it->second;  // 获取持仓信息指针

			rj::Value pItem(rj::kObjectType);  // 创建持仓项JSON对象
			pItem.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码字段
			pItem.AddMember("volume", pInfo->_volume, allocator);  // 添加持仓数量字段
			pItem.AddMember("closeprofit", pInfo->_closeprofit, allocator);  // 添加平仓盈亏字段
			pItem.AddMember("dynprofit", pInfo->_dynprofit, allocator);  // 添加浮动盈亏字段

			rj::Value details(rj::kArrayType);  // 创建持仓明细JSON数组
			for (auto dit = pInfo->_details.begin(); dit != pInfo->_details.end(); dit++)  // 遍历持仓明细
			{
				const DetailInfo& dInfo = *dit;  // 获取持仓明细引用
				if(decimal::eq(dInfo._volume, 0))  // 如果持仓数量为0（已平仓）
					continue;  // 跳过当前明细
				rj::Value dItem(rj::kObjectType);  // 创建明细项JSON对象
				dItem.AddMember("long", dInfo._long, allocator);  // 添加多空方向字段
				dItem.AddMember("price", dInfo._price, allocator);  // 添加开仓价格字段
				dItem.AddMember("volume", dInfo._volume, allocator);  // 添加持仓数量字段
				dItem.AddMember("opentime", dInfo._opentime, allocator);  // 添加开仓时间字段
				dItem.AddMember("opentdate", dInfo._opentdate, allocator);  // 添加开仓日期字段

				dItem.AddMember("profit", dInfo._profit, allocator);  // 添加浮动盈亏字段

				details.PushBack(dItem, allocator);  // 将明细项添加到明细数组
			}

			pItem.AddMember("details", details, allocator);  // 将明细数组添加到持仓项

			jPos.PushBack(pItem, allocator);  // 将持仓项添加到持仓数组
		}

		root.AddMember("positions", jPos, allocator);  // 将持仓数组添加到根对象
	}

	//风控参数设置
	{
		rj::Value jRisk(rj::kObjectType);  // 创建风控JSON对象

		jRisk.AddMember("scale", _risk_volscale, allocator);  // 添加仓位缩放系数字段
		jRisk.AddMember("date", _risk_date, allocator);  // 添加生效日期字段

		root.AddMember("riskmon", jRisk, allocator);  // 将风控对象添加到根对象
	}

	{
		std::string filename = WtHelper::getPortifolioDir();  // 获取组合目录路径
		filename += "datas.json";  // 拼接数据文件名

		BoostFile bf;  // 创建文件对象
		if (bf.create_new_file(filename.c_str()))  // 如果创建新文件成功
		{
			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建美化写入器
			root.Accept(writer);  // 将JSON对象写入缓冲区（格式化输出）
			bf.write_file(sb.GetString());  // 将缓冲区内容写入文件
			bf.close_file();  // 关闭文件
		}
	}
}

/**
 * @brief 加载数据
 * 
 * 从JSON文件中加载保存的数据，包括资金数据、持仓数据、风控参数等。
 * 如果文件不存在或解析失败，则使用默认值。
 * 
 * 实现逻辑：
 * 1. 创建组合资金信息对象
 * 2. 读取JSON文件
 * 3. 解析并加载资金数据
 * 4. 解析并加载持仓数据（包括持仓明细）
 * 5. 解析并加载风控参数
 */
void WtEngine::load_datas()
{
	_port_fund = WTSPortFundInfo::create();  // 创建组合资金信息对象

	std::string filename = WtHelper::getPortifolioDir();  // 获取组合目录路径
	filename += "datas.json";  // 拼接数据文件名
	/*一个具有实际意义的完整的JSON例子：
	{
		"fund": {
			"predynbal": 1000000, //预动态余额
			"balance": 1000000, //余额
			"prebalance": 1000000, //预余额
			"profit": 0, //平仓盈亏
			"dynprofit": 0, //浮动盈亏
			"fees": 0, //手续费
			"last_date": 20210101, //上次结算日期
			"max_dyn_bal": 1000000, //最大动态余额
			"max_time": 1000000, //最大动态余额时间
			"min_dyn_bal": 1000000, //最小动态余额
			"min_time": 1000000, //最小动态余额时间
			"maxmd": {
				"date": 20210101,
				"dyn_balance": 1000000
			},
			"minmd": {
				"date": 20210101,
				"dyn_balance": 1000000
			},
			"update_time": 1000000
		}
		"positions": [
			{
				"code": "CFFEX.IF", //合约代码
				"volume": 10, //持仓数量
				"closeprofit": 0, //平仓盈亏
				"dynprofit": 0, //浮动盈亏
				"details": [
					{
						"long": true, //多空方向
						"price": 10000, //开仓价格
						"volume": 10, //持仓数量
						"opentime": 1000000, //开仓时间
						"opentdate": 20210101 //开仓日期
					}
				]
			},
			{s
				"code": "CFFEX.IF",
				"volume": 10,
				"closeprofit": 0,
				"dynprofit": 0
			}
		]
	}
	*/

	if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
	{
		return;  // 直接返回，使用默认值
	}

	std::string content;  // 文件内容字符串
	StdFile::read_file_content(filename.c_str(), content);  // 读取文件内容
	if (content.empty())  // 如果文件内容为空
		return;  // 直接返回

	rj::Document root;  // 创建JSON文档对象
	root.Parse(content.c_str());  // 解析JSON字符串

	if (root.HasParseError())  // 如果解析失败
		return;  // 直接返回

	//读取资金
	{
		const rj::Value& jFund = root["fund"];  // 获取资金JSON对象
		if (!jFund.IsNull() && jFund.IsObject())  // 如果资金对象存在且为对象类型
		{
			WTSFundStruct& fundInfo = _port_fund->fundInfo();  // 获取组合资金信息引用
			fundInfo._predynbal = jFund["predynbal"].GetDouble();  // 加载预动态余额
			fundInfo._balance = jFund["balance"].GetDouble();  // 加载余额
			fundInfo._prebalance = jFund["prebalance"].GetDouble();  // 加载预余额
			fundInfo._profit = jFund["profit"].GetDouble();  // 加载平仓盈亏
			fundInfo._dynprofit = jFund["dynprofit"].GetDouble();  // 加载浮动盈亏
			fundInfo._fees = jFund["fees"].GetDouble();  // 加载手续费
			fundInfo._last_date = jFund["last_date"].GetUint();  // 加载上次结算日期
			fundInfo._max_dyn_bal = jFund["max_dyn_bal"].GetDouble();  // 加载最大动态余额
			fundInfo._max_time = jFund["max_time"].GetUint();  // 加载最大动态余额时间
			fundInfo._min_dyn_bal = jFund["min_dyn_bal"].GetDouble();  // 加载最小动态余额
			fundInfo._min_time = jFund["min_time"].GetUint();  // 加载最小动态余额时间

			const rj::Value& jMaxMD = jFund["maxmd"];  // 获取最大月度动态余额JSON对象
			fundInfo._max_md_dyn_bal._dyn_balance = jMaxMD["dyn_balance"].GetDouble();  // 加载最大月度动态余额
			fundInfo._max_md_dyn_bal._date = jMaxMD["date"].GetUint();  // 加载最大月度动态余额日期

			const rj::Value& jMinMD = jFund["minmd"];  // 获取最小月度动态余额JSON对象
			fundInfo._min_md_dyn_bal._dyn_balance = jMinMD["dyn_balance"].GetDouble();  // 加载最小月度动态余额
			fundInfo._min_md_dyn_bal._date = jMinMD["date"].GetUint();  // 加载最小月度动态余额日期

			if(jFund.HasMember("update_time"))  // 如果包含更新时间字段
			{
				fundInfo._update_time = jFund["update_time"].GetInt64();  // 加载更新时间
			}
		}
	}

	{//读取仓位
		double total_profit = 0;  // 总平仓盈亏
		double total_dynprofit = 0;  // 总浮动盈亏
		const rj::Value& jPos = root["positions"];  // 获取持仓JSON数组
		if (!jPos.IsNull() && jPos.IsArray())  // 如果持仓数组存在且为数组类型
		{
			for (const rj::Value& pItem : jPos.GetArray())  // 遍历持仓数组
			{
				const char* stdCode = pItem["code"].GetString();  // 获取合约代码
				PosInfoPtr& pInfo = _pos_map[stdCode];  // 获取或创建持仓信息指针
				if (pInfo == NULL)  // 如果持仓信息不存在
					pInfo.reset(new PosInfo);  // 创建新的持仓信息对象
				pInfo->_closeprofit = pItem["closeprofit"].GetDouble();  // 加载平仓盈亏
				pInfo->_volume = pItem["volume"].GetDouble();  // 加载持仓数量
				if (pInfo->_volume == 0)  // 如果持仓为0
					pInfo->_dynprofit = 0;  // 浮动盈亏设为0
				else  // 如果持仓不为0
					pInfo->_dynprofit = pItem["dynprofit"].GetDouble();  // 加载浮动盈亏

				total_profit += pInfo->_closeprofit;  // 累加总平仓盈亏
				total_dynprofit += pInfo->_dynprofit;  // 累加总浮动盈亏

				const rj::Value& details = pItem["details"];  // 获取持仓明细JSON数组
				if (details.IsNull() || !details.IsArray() || details.Size() == 0)  // 如果明细数组不存在或为空
					continue;  // 跳过当前持仓

				for (uint32_t i = 0; i < details.Size(); i++)  // 遍历持仓明细数组
				{
					const rj::Value& dItem = details[i];  // 获取明细项JSON对象
					DetailInfo dInfo;  // 创建持仓明细对象
					dInfo._long = dItem["long"].GetBool();  // 加载多空方向
					dInfo._price = dItem["price"].GetDouble();  // 加载开仓价格
					dInfo._volume = dItem["volume"].GetDouble();  // 加载持仓数量
					dInfo._opentime = dItem["opentime"].GetUint64();  // 加载开仓时间
					if (dItem.HasMember("opentdate"))  // 如果包含开仓日期字段
						dInfo._opentdate = dItem["opentdate"].GetUint();  // 加载开仓日期

					dInfo._profit = dItem["profit"].GetDouble();  // 加载浮动盈亏
					pInfo->_details.emplace_back(dInfo);  // 将明细添加到持仓明细列表
				}

				WTSLogger::debug("Porfolio position confirmed,{} -> {}", stdCode, pInfo->_volume);  // 记录调试日志：组合持仓确认
			}
		}

		WTSFundStruct& fundInfo = _port_fund->fundInfo();  // 获取组合资金信息引用
		fundInfo._dynprofit = total_dynprofit;  // 更新组合资金的浮动盈亏（使用加载的总浮动盈亏）

		WTSLogger::debug("{} position info of portfolio loaded", _pos_map.size());  // 记录调试日志：持仓信息已加载
	}

	if(root.HasMember("riskmon"))  // 如果包含风控参数
	{
		//读取风控参数
		const rj::Value& jRisk = root["riskmon"];  // 获取风控JSON对象
		if (!jRisk.IsNull() && jRisk.IsObject())  // 如果风控对象存在且为对象类型
		{
			_risk_date = jRisk["date"].GetUint();  // 加载风控参数生效日期
			_risk_volscale = jRisk["scale"].GetDouble();  // 加载仓位缩放系数
		}
	}
}

/**
 * @brief 获取Tick数据切片
 * @param sid 策略ID（未使用）
 * @param code 合约代码字符串
 * @param count 获取的Tick数量
 * @return WTSTickSlice* 返回Tick数据切片指针，如果不存在返回NULL
 * 
 * 从数据管理器获取指定合约的Tick数据切片。
 */
WTSTickSlice* WtEngine::get_tick_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_tick_slice(code, count);  // 从数据管理器获取Tick数据切片
}

/**
 * @brief 获取最后一个Tick数据
 * @param sid 策略ID（未使用）
 * @param stdCode 标准合约代码字符串
 * @return WTSTickData* 返回最后一个Tick数据指针，如果不存在返回NULL
 * 
 * 从数据管理器获取指定合约的最后一个Tick数据。
 * 注意：返回的指针需要调用者释放。
 */
WTSTickData* WtEngine::get_last_tick(uint32_t sid, const char* stdCode)
{
	return _data_mgr->grab_last_tick(stdCode);  // 从数据管理器获取最后一个Tick数据
}

/**
 * @brief 获取K线数据切片
 * @param sid 策略ID
 * @param stdCode 标准合约代码字符串
 * @param period 周期字符串（如"m1"表示1分钟，"d"表示日线）
 * @param count 获取的K线数量
 * @param times 周期倍数，默认为1（如period="m1"，times=5表示5分钟K线）
 * @param etime 结束时间（时间戳），默认为0（表示获取最新数据）
 * @return WTSKlineSlice* 返回K线数据切片指针，如果不存在返回NULL
 * 
 * 从数据管理器获取指定合约的K线数据切片。
 * 会记录策略对K线的订阅关系，用于后续K线更新通知。
 */
WTSKlineSlice* WtEngine::get_kline_slice(uint32_t sid, const char* stdCode, const char* period, uint32_t count, uint32_t times /* = 1 */, uint64_t etime /* = 0 */)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* cInfo = _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 获取品种信息
	if (cInfo == NULL)  // 如果品种信息不存在
		return NULL;  // 返回NULL

	thread_local static char key[64] = { 0 };  // 线程本地静态缓冲区（用于生成订阅键）
	fmtutil::format_to(key, "{}-{}-{}", stdCode, period, times);  // 格式化订阅键（合约代码-周期-倍数）

	SubList& sids = _bar_sub_map[key];  // 获取或创建订阅列表（键 -> 策略ID列表）
	sids[sid] = std::make_pair(sid, 0);  // 添加策略ID到订阅列表（pair的第一个元素是策略ID，第二个元素是标志，0表示未复权）

	WTSKlinePeriod kp;  // K线周期枚举
	if (period[0] == 'm')  // 如果周期以'm'开头（分钟周期）
	{
		if (times % 5 == 0)  // 如果倍数是5的倍数
		{
			kp = KP_Minute5;  // 使用5分钟周期
			times /= 5;  // 倍数除以5（如times=5表示5分钟，times=10表示10分钟）
		}
		else  // 如果倍数不是5的倍数
			kp = KP_Minute1;  // 使用1分钟周期
	}
	else  // 如果周期不是分钟周期
	{
		kp = KP_DAY;  // 使用日线周期
	}

	return _data_mgr->get_kline_slice(stdCode, kp, times, count, etime);  // 从数据管理器获取K线数据切片
}


/**
 * @brief 处理推送的行情数据
 * @param curTick 当前Tick数据指针
 * 
 * 处理外部推送的行情数据。
 * 如果合约有主力合约代码，同时推送主力合约的行情数据。
 * 
 * 实现逻辑：
 * 1. 处理原始合约的行情数据
 * 2. 如果合约不是平仓合约（有主力合约代码），同时处理主力合约的行情数据
 */
void WtEngine::handle_push_quote(WTSTickData* curTick)
{
	std::string stdCode = curTick->code();  // 获取合约代码字符串
	_data_mgr->handle_push_quote(stdCode.c_str(), curTick);  // 将行情数据推送到数据管理器
	on_tick(stdCode.c_str(), curTick);  // 处理Tick事件（更新价格、触发信号等）

	double price = curTick->price();  // 获取价格（未使用）
	WTSContractInfo* cInfo = curTick->getContractInfo();  // 获取合约信息

	//if(hotFlag == 1)
	if(!cInfo->isFlat())  // 如果合约不是平仓合约（有主力合约代码）
	{
		const char* hotCode = cInfo->getHotCode();  // 获取主力合约代码
		WTSTickData* hotTick = WTSTickData::create(curTick->getTickStruct());  // 创建新的Tick数据对象（复制原始Tick结构）
		hotTick->setCode(hotCode);  // 设置主力合约代码
		hotTick->setContractInfo(curTick->getContractInfo());  // 设置合约信息（使用原始合约信息）

		_data_mgr->handle_push_quote(hotCode, hotTick);  // 将主力合约行情数据推送到数据管理器
		on_tick(hotCode, hotTick);  // 处理主力合约Tick事件

		hotTick->release();  // 释放Tick数据对象
	}
	//else if (hotFlag == 2)
	//{
	//	std::string scndCode = CodeHelper::stdCodeToStd2ndCode(stdCode.c_str());
	//	WTSTickData* scndTick = WTSTickData::create(curTick->getTickStruct());
	//	scndTick->setCode(scndCode.c_str());
	//	scndTick->setContractInfo(curTick->getContractInfo());

	//	_data_mgr->handle_push_quote(scndCode.c_str(), scndTick);
	//	on_tick(scndCode.c_str(), scndTick);

	//	scndTick->release();
	//}
}

/**
 * @brief 获取当前价格
 * @param stdCode 标准合约代码字符串（支持复权后缀：QFQ表示前复权，HFQ表示后复权）
 * @return double 返回当前价格，如果不存在返回0.0
 * 
 * 获取指定合约的当前价格。
 * 支持前复权和后复权价格计算。
 * 会缓存价格，避免重复查询。
 * 
 * 实现逻辑：
 * 1. 判断是否为复权合约（前复权或后复权）
 * 2. 在价格缓存中查找
 * 3. 如果未找到，从数据管理器获取最新Tick数据
 * 4. 如果是后复权，进行复权处理
 * 5. 缓存价格并返回
 */
double WtEngine::get_cur_price(const char* stdCode)
{
	auto len = strlen(stdCode);  // 获取合约代码长度
	char lastChar = stdCode[len - 1];  // 获取最后一个字符（用于判断复权类型）
	//前复权直接读取标准合约代码
	bool bAdjusted = (lastChar == SUFFIX_QFQ || lastChar == SUFFIX_HFQ);  // 判断是否为复权合约（QFQ=前复权，HFQ=后复权）
	//前复权需要去掉－，后复权和未复权都直接查找
	std::string sCode = (lastChar == SUFFIX_QFQ) ? std::string(stdCode, len - 1) : stdCode;  // 获取用于查找的合约代码（前复权去掉最后一个字符，后复权和未复权保持不变）
	auto it = _price_map.find(sCode);  // 在价格缓存中查找
	if(it == _price_map.end())  // 如果未找到缓存价格
	{
		//找不到的时候，先读取未复权的tick数据
		std::string fCode = bAdjusted ? std::string(stdCode, len - 1) : stdCode;  // 获取基础合约代码（复权合约去掉最后一个字符）
		WTSTickData* lastTick = _data_mgr->grab_last_tick(fCode.c_str());  // 从数据管理器获取最新Tick数据
		if (lastTick == NULL)  // 如果Tick数据不存在
			return 0.0;  // 返回0.0

		WTSContractInfo* cInfo = lastTick->getContractInfo();  // 获取合约信息

		double ret = lastTick->price();  // 获取价格
		lastTick->release();  // 释放Tick数据对象

		//如果是后复权，则进行复权处理
		if (lastChar == SUFFIX_HFQ)  // 如果是后复权
		{
			ret *= get_exright_factor(stdCode, cInfo->getCommInfo());  // 乘以复权因子（后复权需要乘以复权因子）
		}

		_price_map[sCode] = ret;  // 缓存价格（使用查找键）
		return ret;  // 返回价格
	}
	else  // 如果找到缓存价格
	{
		return it->second;  // 返回缓存价格
	}
}

/**
 * @brief 获取当日价格
 * @param stdCode 标准合约代码字符串（支持复权后缀：QFQ表示前复权，HFQ表示后复权）
 * @param flag 价格类型标志，0=开盘价，1=最高价，2=最低价，3=最新价，默认为0
 * @return double 返回当日价格，如果不存在返回0.0
 * 
 * 获取指定合约的当日价格（开盘价、最高价、最低价或最新价）。
 * 支持前复权和后复权价格计算。
 * 
 * 实现逻辑：
 * 1. 判断是否为复权合约
 * 2. 从数据管理器获取最新Tick数据
 * 3. 根据flag获取对应的价格（开盘/最高/最低/最新）
 * 4. 如果是后复权，进行复权处理
 */
double WtEngine::get_day_price(const char* stdCode, int flag /* = 0 */)
{
	auto len = strlen(stdCode);  // 获取合约代码长度
	char lastChar = stdCode[len - 1];  // 获取最后一个字符（用于判断复权类型）
	//前复权直接读取标准合约代码
	bool bAdjusted = (lastChar == SUFFIX_QFQ || lastChar == SUFFIX_HFQ);  // 判断是否为复权合约
	//前复权需要去掉－，后复权和未复权都直接查找
	std::string sCode = (lastChar == SUFFIX_QFQ) ? std::string(stdCode, len - 1) : stdCode;  // 获取用于查找的合约代码（未使用）

	//找不到的时候，先读取未复权的tick数据
	std::string fCode = bAdjusted ? std::string(stdCode, len - 1) : stdCode;  // 获取基础合约代码（复权合约去掉最后一个字符）
	WTSTickData* lastTick = _data_mgr->grab_last_tick(fCode.c_str());  // 从数据管理器获取最新Tick数据
	if (lastTick == NULL)  // 如果Tick数据不存在
		return 0.0;  // 返回0.0

	WTSCommodityInfo* commInfo = get_commodity_info(fCode.c_str());  // 获取品种信息（用于复权计算）

	double ret = 0.0;  // 返回值
	switch (flag)  // 根据flag获取对应的价格
	{
	case 0:
		ret = lastTick->open(); break;  // 开盘价
	case 1:
		ret = lastTick->high(); break;  // 最高价
	case 2:
		ret = lastTick->low(); break;  // 最低价
	case 3:
		ret = lastTick->price(); break;  // 最新价
	default:
		break;  // 其他值返回0.0
	}
	lastTick->release();  // 释放Tick数据对象

	//如果是后复权，则进行复权处理
	if (lastChar == SUFFIX_HFQ)  // 如果是后复权
	{
		ret *= get_exright_factor(stdCode, commInfo);  // 乘以复权因子
	}

	return ret;  // 返回价格
}

/**
 * @brief 获取复权因子
 * @param stdCode 标准合约代码字符串
 * @param commInfo 品种信息指针，如果为NULL则自动获取，默认为NULL
 * @return double 返回复权因子，如果不存在返回1.0（无复权）
 * 
 * 获取指定合约的复权因子。
 * 对于股票，从数据管理器获取复权因子；对于期货主力合约，从热点管理器获取规则因子。
 */
double WtEngine::get_exright_factor(const char* stdCode, WTSCommodityInfo* commInfo /* = NULL */)
{
	if (commInfo == NULL)  // 如果品种信息未提供
		commInfo = get_commodity_info(stdCode);  // 自动获取品种信息

	if (commInfo == NULL)  // 如果品种信息不存在
		return 1.0;  // 返回1.0（无复权）

	if (commInfo->isStock())  // 如果是股票
		return _data_mgr->get_adjusting_factor(stdCode, get_trading_date());  // 从数据管理器获取复权因子（参数：合约代码、交易日）
	else  // 如果是期货等其他品种
	{
		const char* ruleTag = _hot_mgr->getRuleTag(stdCode);  // 获取规则标签（如.HOT）
		if(strlen(ruleTag) > 0)  // 如果有规则标签
			return _hot_mgr->getRuleFactor(ruleTag, commInfo->getFullPid(), get_trading_date());  // 从热点管理器获取规则因子（参数：规则标签、品种全称、交易日）
	}

	return 1.0;  // 返回1.0（无复权）
}

/**
 * @brief 获取复权标志
 * @return uint32_t 返回复权标志，如果数据管理器不存在返回0
 * 
 * 从数据管理器获取复权标志。
 * 注意：当前实现有bug，没有返回数据管理器的返回值。
 */
uint32_t WtEngine::get_adjusting_flag()
{
	if (_data_mgr)  // 如果数据管理器存在
		_data_mgr->get_adjusting_flag();  // 调用数据管理器的获取复权标志方法（但未返回结果）

	return 0;  // 返回0
}

/**
 * @brief 订阅Tick数据
 * @param sid 策略ID
 * @param stdCode 标准合约代码字符串（支持复权后缀和主力合约代码）
 * 
 * 记录策略对Tick数据的订阅关系。
 * 如果是主力合约代码（如SHFE.ag.HOT），会转换为实际合约代码（如SHFE.ag.1912），因为执行器只识别原合约代码。
 * 
 * 实现逻辑：
 * 1. 判断是否为有规则标签的合约（如.HOT）
 * 2. 处理复权后缀（QFQ/HFQ）
 * 3. 记录订阅关系（策略ID -> 合约代码）
 * 4. 如果是主力合约，获取实际合约代码（用于执行器）
 */
void WtEngine::sub_tick(uint32_t sid, const char* stdCode)
{
	//如果是主力合约代码, 如SHFE.ag.HOT, 那么要转换成原合约代码, SHFE.ag.1912
	//因为执行器只识别原合约代码
	const char* ruleTag = _hot_mgr->getRuleTag(stdCode);  // 获取规则标签（如.HOT）
	if(strlen(ruleTag) > 0)  // 如果有规则标签（主力合约）
	{
		//SubList& sids = _tick_sub_map[stdCode];
		//sids[sid] = std::make_pair(sid, 0);

		std::size_t length = strlen(stdCode);  // 获取合约代码长度
		uint32_t flag = 0;  // 复权标志（0=未复权，1=前复权，2=后复权）
		if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权后缀
		{
			length--;  // 长度减1（去掉复权后缀）

			flag = (stdCode[length] == SUFFIX_QFQ) ? 1 : 2;  // 设置复权标志（注意：这里应该检查length-1，可能有bug）
		}

		SubList& sids = _tick_sub_map[std::string(stdCode, length)];  // 获取或创建订阅列表（键：去掉复权后缀的合约代码）
		sids[sid] = std::make_pair(sid, flag);  // 添加策略ID到订阅列表（pair的第一个元素是策略ID，第二个元素是复权标志）

		CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
		std::string rawCode = _hot_mgr->getCustomRawCode(ruleTag, cInfo.stdCommID(), _cur_tdate);  // 获取自定义原始合约代码（根据规则标签、标准品种ID和当前交易日）
		std::string stdRawCode = CodeHelper::rawMonthCodeToStdCode(rawCode.c_str(), cInfo._exchg);  // 将原始月份代码转换为标准合约代码（用于执行器）
	}
	//if (CodeHelper::isStdFutHotCode(stdCode))
	//{
	//	SubList& sids = _tick_sub_map[stdCode];
	//	sids[sid] = std::make_pair(sid, 0);

	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);
	//	std::string rawCode = _hot_mgr->getRawCode(cInfo._exchg, cInfo._product, _cur_tdate);
	//	std::string stdRawCode = CodeHelper::rawMonthCodeToStdCode(rawCode.c_str(), cInfo._exchg);
	//	//_ticksubed_raw_codes.insert(stdRawCode);
	//}
	//else if (CodeHelper::isStdFut2ndCode(stdCode))
	//{
	//	SubList& sids = _tick_sub_map[stdCode];
	//	sids[sid] = std::make_pair(sid, 0);

	//	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode);
	//	std::string rawCode = _hot_mgr->getSecondRawCode(cInfo._exchg, cInfo._product, _cur_tdate);
	//	std::string stdRawCode = CodeHelper::rawMonthCodeToStdCode(rawCode.c_str(), cInfo._exchg);
	//	//_ticksubed_raw_codes.insert(stdRawCode);
	//}
	else  // 如果没有规则标签（普通合约）
	{
		std::size_t length = strlen(stdCode);  // 获取合约代码长度
		uint32_t flag = 0;  // 复权标志
		if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权后缀
		{
			length--;  // 长度减1（去掉复权后缀）

			flag = (stdCode[length - 1] == SUFFIX_QFQ) ? 1 : 2;  // 设置复权标志（检查length-1位置）
		}

		SubList& sids = _tick_sub_map[std::string(stdCode, length)];  // 获取或创建订阅列表（键：去掉复权后缀的合约代码）
		sids[sid] = std::make_pair(sid, flag);  // 添加策略ID到订阅列表

		//_ticksubed_raw_codes.insert(std::string(stdCode, length));
	}
}

/**
 * @brief 加载手续费模板
 * @param filename 手续费模板文件路径
 * 
 * 从配置文件中加载手续费模板，设置各品种的开仓、平仓、平今手续费率和保证金率。
 * JSON例子：
 * {
 *   "CFFEX.IF": {
 *     "open": 10,
 *     "close": 10,
 *     "closetoday": 10,
 *     "byvolume": true
 *   }
 * }
 * "CFFEX.IF" 是品种ID，open 是开仓手续费，close 是平仓手续费，closetoday 是平今手续费，byvolume 是是否按数量计算。
 * 
 * 实现逻辑：
 * 1. 检查文件是否存在
 * 2. 加载配置文件
 * 3. 遍历配置项，解析品种ID（交易所.品种）
 * 4. 获取品种信息并设置手续费率和保证金率
 */
void WtEngine::load_fees(const char* filename)
{
	if (strlen(filename) == 0)  // 如果文件名为空
		return;  // 直接返回

	if (!StdFile::exists(filename))  // 如果文件不存在
	{
		WTSLogger::error("Fee templates file {} not exists", filename);  // 记录错误日志
		return;  // 直接返回
	}

	WTSVariant* cfg = WTSCfgLoader::load_from_file(filename);  // 从文件加载配置
	if (cfg == NULL)  // 如果加载失败
	{
		WTSLogger::error("Fee templates file {} loading failed", filename);  // 记录错误日志
		return;  // 直接返回
	}

	auto keys = cfg->memberNames();  // 获取所有配置项的键名（品种ID列表）
	for (const std::string& fullPid : keys)  // 遍历所有配置项
	{
		WTSVariant* cfgItem = cfg->get(fullPid.c_str());  // 获取配置项（品种的手续费配置）
		const StringVector& ay = StrUtil::split(fullPid, ".");  // 分割品种ID（格式：交易所.品种）
		WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(ay[0].c_str(), ay[1].c_str());  // 获取品种信息（参数：交易所代码、品种代码）
		if (commInfo == NULL)  // 如果品种信息不存在
			continue;  // 跳过当前配置项

		commInfo->setFeeRates(cfgItem->getDouble("open"), cfgItem->getDouble("close"), cfgItem->getDouble("closetoday"), cfgItem->getBoolean("byvolume"));  // 设置手续费率（开仓、平仓、平今、是否按数量计算）
		commInfo->setMarginRate(cfgItem->getDouble("margin"));  // 设置保证金率
	}

	cfg->release();  // 释放配置对象

	WTSLogger::info("{} fee templates loaded", _fee_map.size());  // 记录信息日志：手续费模板已加载（注意：这里使用了_fee_map.size()，但实际应该记录加载的品种数量）
}

/**
 * @brief 计算手续费
 * @param stdCode 标准合约代码字符串
 * @param price 价格
 * @param qty 数量
 * @param offset 开平仓类型，0=开仓，1=平仓，2=平今
 * @return double 返回手续费（四舍五入到分）
 * 
 * 根据合约代码、价格、数量和开平仓类型计算手续费。
 * 支持按数量计算和按金额计算两种方式。
 * 
 * 实现逻辑：
 * 1. 解析合约代码，获取品种ID
 * 2. 查找手续费模板
 * 3. 根据是否按数量计算，选择不同的计算方式
 * 4. 四舍五入到分（保留两位小数）
 */
double WtEngine::calc_fee(const char* stdCode, double price, double qty, uint32_t offset)
{
	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	const char* stdPID = codeInfo.stdCommID();  // 获取标准品种ID（交易所.品种）
	auto it = _fee_map.find(stdPID);  // 在手续费映射表中查找品种ID
	if (it == _fee_map.end())  // 如果未找到手续费模板
	{
		WTSLogger::warn("Fee template of {} not found, return 0.0 as default", stdPID);  // 记录警告日志
		return 0.0;  // 返回0.0作为默认值
	}

	double ret = 0.0;  // 返回值
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(stdPID);  // 获取品种信息（用于获取合约乘数）
	const FeeItem& fItem = it->second;  // 获取手续费项
	if(fItem._by_volume)  // 如果按数量计算手续费
	{
		switch (offset)  // 根据开平仓类型计算手续费
		{
		case 0: ret = fItem._open*qty; break;  // 开仓手续费 = 开仓费率 * 数量
		case 1: ret = fItem._close*qty; break;  // 平仓手续费 = 平仓费率 * 数量
		case 2: ret = fItem._close_today*qty; break;  // 平今手续费 = 平今费率 * 数量
		default: ret = 0.0; break;  // 其他值返回0.0
		}
	}
	else  // 如果按金额计算手续费
	{
		double amount = price*qty*commInfo->getVolScale();  // 计算成交金额（价格 * 数量 * 合约乘数）
		switch (offset)  // 根据开平仓类型计算手续费
		{
		case 0: ret = fItem._open*amount; break;  // 开仓手续费 = 开仓费率 * 成交金额
		case 1: ret = fItem._close*amount; break;  // 平仓手续费 = 平仓费率 * 成交金额
		case 2: ret = fItem._close_today*amount; break;  // 平今手续费 = 平今费率 * 成交金额
		default: ret = 0.0; break;  // 其他值返回0.0
		}
	}

	return (int32_t)(ret * 100 + 0.5) / 100.0;  // 四舍五入到分（保留两位小数）：乘以100，加0.5，取整，再除以100
}

/**
 * @brief 添加交易信号
 * @param stdCode 标准合约代码字符串
 * @param qty 目标仓位数量（正数表示多仓，负数表示空仓）
 * @param bStandBy 是否等待下一个Tick触发，true表示等待，false表示立即执行，默认为true
 * 
 * 添加策略发出的交易信号。
 * 如果bStandBy为true或当前价格为0，则延迟到下一个Tick触发（确保价格一致性）。
 * 如果bStandBy为false且当前价格不为0，则立即执行。
 * 
 * 实现逻辑：
 * 1. 获取当前价格
 * 2. 如果需要等待或价格为0，将信号加入信号映射表，等待下一个Tick触发
 * 3. 如果不需要等待且价格不为0，立即设置持仓
 */
void WtEngine::append_signal(const char* stdCode, double qty, bool bStandBy /* = true */)
{
	/*
	 *	By Wesley @ 2021.12.16
	 *	这里发现一个问题，就是组合的理论成交价和策略的理论成交价不一致
	 *	检查以后发现，策略的理论成交价会在下一个tick更新
	 *	但是组合的理论成交价这一个tick就直接更新了
	 *	这就导致组合成交价永远比策略提前一个tick
	 *	这里做一个修正，等下一个tick进来，触发signal
	 *	如果是bar内触发的，bStandBy为false，则直接修改持仓
	 */
	double curPx = get_cur_price(stdCode);  // 获取当前价格
	if(bStandBy || decimal::eq(curPx, 0.0))  // 如果需要等待或当前价格为0
	{
		SigInfo& sInfo = _sig_map[stdCode];  // 获取或创建信号信息
		sInfo._volume = qty;  // 设置目标仓位
		sInfo._gentime = (uint64_t)_cur_date * 1000000000 + (uint64_t)_cur_raw_time * 100000 + _cur_secs;  // 设置信号生成时间（格式：日期*1000000000 + 分钟时间*100000 + 秒数）
	}
	else  // 如果不需要等待且当前价格不为0
	{
		do_set_position(stdCode, qty);  // 立即设置持仓
	}

	/*
	double curPx = get_cur_price(stdCode);
	if(decimal::eq(curPx, 0.0))
	{
		SigInfo& sInfo = _sig_map[stdCode];
		sInfo._volume = qty;
		sInfo._gentime = (uint64_t)_cur_date * 1000000000 + (uint64_t)_cur_raw_time * 100000 + _cur_secs;
	}
	else
	{
		do_set_position(stdCode, qty);
	}
	*/
}

/**
 * @brief 设置持仓
 * @param stdCode 标准合约代码字符串
 * @param qty 目标仓位数量（正数表示多仓，负数表示空仓）
 * @param curPx 当前价格，如果小于0则自动获取，默认为-1
 * 
 * 根据目标仓位设置持仓，处理开仓、平仓和反手操作。
 * 使用持仓明细管理，支持FIFO平仓规则。
 * 更新资金信息（余额、手续费、盈亏等）并记录交易日志。
 * 
 * 实现逻辑：
 * 1. 获取或创建持仓信息
 * 2. 如果持仓未变化，直接返回
 * 3. 计算持仓差量
 * 4. 如果持仓方向和目标仓位方向一致，增加持仓明细（开仓）
 * 5. 如果持仓方向和目标仓位方向不一致，平仓并可能需要反手
 * 6. 更新资金信息（手续费、盈亏、余额）
 * 7. 记录交易日志和平仓日志
 */
void WtEngine::do_set_position(const char* stdCode, double qty, double curPx /* = -1 */)
{
	PosInfoPtr& pInfo = _pos_map[stdCode];  // 获取或创建持仓信息指针
	if (pInfo == NULL)  // 如果持仓信息不存在
		pInfo.reset(new PosInfo);  // 创建新的持仓信息对象

	SpinLock lock(pInfo->_mtx);  // 获取自旋锁（保护持仓数据的线程安全）

	if(decimal::lt(curPx, 0))  // 如果当前价格无效（小于0）
		curPx = get_cur_price(stdCode);  // 自动获取当前价格

	uint64_t curTm = (uint64_t)_cur_date * 10000 + _cur_time;  // 计算当前时间戳（格式：日期*10000 + 分钟时间）
	uint32_t curTDate = _cur_tdate;  // 获取当前交易日

	if (decimal::eq(pInfo->_volume, qty))  // 如果持仓数量等于目标仓位（无需变动）
		return;  // 直接返回

	double diff = qty - pInfo->_volume;  // 计算持仓差量（目标仓位 - 当前仓位）

	CodeHelper::CodeInfo codeInfo = CodeHelper::extractStdCode(stdCode, _hot_mgr);  // 解析标准合约代码
	WTSCommodityInfo* commInfo = _base_data_mgr->getCommodity(codeInfo._exchg, codeInfo._product);  // 获取品种信息（用于计算手续费和盈亏）

	WTSFundStruct& fundInfo = _port_fund->fundInfo();  // 获取组合资金信息引用

	if (decimal::gt(pInfo->_volume*diff, 0))//当前持仓和目标仓位方向一致, 增加一条明细, 增加数量即可
	{
		pInfo->_volume = qty;  // 更新持仓数量

		DetailInfo dInfo;  // 创建持仓明细对象
		dInfo._long = decimal::gt(qty, 0);  // 设置多空方向（正数为多仓，负数为空仓）
		dInfo._price = curPx;  // 设置开仓价格
		dInfo._volume = abs(diff);  // 设置持仓数量（差量的绝对值）
		dInfo._opentime = curTm;  // 设置开仓时间
		dInfo._opentdate = curTDate;  // 设置开仓日期
		pInfo->_details.emplace_back(dInfo);  // 将明细添加到持仓明细列表

		double fee = commInfo->calcFee(curPx, abs(qty), 0);  // 计算手续费（开仓手续费，注意：这里使用了abs(qty)而不是abs(diff)，可能是bug）
		fundInfo._fees += fee;  // 累加手续费
		fundInfo._balance -= fee;  // 扣除手续费（从余额中扣除）

		log_trade(stdCode, dInfo._long, true, curTm, curPx, abs(diff), fee);  // 记录成交日志（开仓）
	}
	else  // 如果持仓方向和目标仓位方向不一致，需要平仓
	{//持仓方向和目标仓位方向不一致, 需要平仓
		double left = abs(diff);  // 需要平仓的数量（差量的绝对值）

		pInfo->_volume = qty;  // 更新持仓数量为目标仓位
		if (decimal::eq(pInfo->_volume, 0))  // 如果持仓为0
			pInfo->_dynprofit = 0;  // 浮动盈亏设为0
		uint32_t count = 0;  // 已平仓的明细数量（用于清理）
		for (auto it = pInfo->_details.begin(); it != pInfo->_details.end(); it++)  // 遍历持仓明细（FIFO顺序）
		{
			DetailInfo& dInfo = *it;  // 获取持仓明细引用
			if (decimal::eq(dInfo._volume, 0))  // 如果明细数量为0（已平仓）
			{
				count++;  // 计数加1
				continue;  // 跳过当前明细
			}

			double maxQty = min(dInfo._volume, left);  // 本次平仓数量（取明细数量和剩余平仓数量的最小值）
			if (decimal::eq(maxQty, 0))  // 如果平仓数量为0
				continue;  // 跳过当前明细

			dInfo._volume -= maxQty;  // 减少明细数量
			left -= maxQty;  // 减少剩余平仓数量

			//if (dInfo._volume == 0)
			if (decimal::eq(dInfo._volume, 0))  // 如果明细数量为0（已平仓）
				count++;  // 计数加1

			double profit = (curPx - dInfo._price) * maxQty * commInfo->getVolScale();  // 计算平仓盈亏（价格差 * 数量 * 合约乘数）
			if (!dInfo._long)  // 如果是空仓
				profit *= -1;  // 盈亏反向（空仓盈亏方向相反）
			pInfo->_closeprofit += profit;  // 累加持仓平仓盈亏
			// 计算被平仓部分的浮动盈亏
			double closedDynProfit = (curPx - dInfo._price) * maxQty * commInfo->getVolScale() * (!dInfo._long > 0 ? -1 : 1);
			// 从总浮动盈亏中减去被平仓部分的浮动盈亏
			pInfo->_dynprofit -= closedDynProfit;
			fundInfo._profit += profit;  // 累加组合平仓盈亏
			fundInfo._balance += profit;  // 增加余额（盈亏计入余额）

			double fee = commInfo->calcFee(curPx, maxQty, dInfo._opentdate == curTDate ? 2 : 1);  // 计算手续费（如果开仓日期等于当前交易日则为平今，否则为平仓）
			fundInfo._fees += fee;  // 累加手续费
			fundInfo._balance -= fee;  // 扣除手续费（从余额中扣除）

			//这里写成交记录
			log_trade(stdCode, dInfo._long, false, curTm, curPx, maxQty, fee);  // 记录成交日志（平仓）
			//这里写平仓记录
			log_close(stdCode, dInfo._long, dInfo._opentime, dInfo._price, curTm, curPx, maxQty, profit, pInfo->_closeprofit);  // 记录平仓日志

			if (left == 0)  // 如果剩余平仓数量为0
				break;  // 退出循环
		}

		//需要清理掉已经平仓完的明细
		while (count > 0)  // 清理已平仓的明细
		{
			auto it = pInfo->_details.begin();  // 获取第一个明细
			pInfo->_details.erase(it);  // 删除第一个明细
			count--;  // 计数减1
		}

		//最后, 如果还有剩余的, 则需要反手了
		//if (left > 0)
		if(decimal::gt(left, 0))  // 如果还有剩余平仓数量（需要反手）
		{
			left = left * qty / abs(qty);  // 将剩余数量转换为目标方向（乘以目标仓位方向）

			DetailInfo dInfo;  // 创建持仓明细对象（反手开仓）
			dInfo._long = qty > 0;  // 设置多空方向（根据目标仓位方向）
			dInfo._price = curPx;  // 设置开仓价格
			dInfo._volume = abs(left);  // 设置持仓数量（剩余数量的绝对值）
			dInfo._opentime = curTm;  // 设置开仓时间
			dInfo._opentdate = curTDate;  // 设置开仓日期
			pInfo->_details.emplace_back(dInfo);  // 将明细添加到持仓明细列表

			//这里还需要写一笔成交记录
			double fee = commInfo->calcFee(curPx, abs(qty), 0);  // 计算手续费（开仓手续费，注意：这里使用了abs(qty)而不是abs(left)，可能是bug）
			fundInfo._fees += fee;  // 累加手续费
			fundInfo._balance -= fee;  // 扣除手续费（从余额中扣除）

			log_trade(stdCode, dInfo._long, true, curTm, curPx, abs(left), fee);  // 记录成交日志（反手开仓）
		}
	}
}

/**
 * @brief 推送任务到后台线程
 * @param task 任务函数对象
 * 
 * 将任务添加到任务队列，并启动后台线程（如果尚未启动）执行任务。
 * 使用条件变量通知后台线程有新任务。
 */
void WtEngine::push_task(TaskItem task)
{
	{
		StdUniqueLock lock(_mtx_task);  // 获取任务队列互斥锁
		_task_queue.push(task);  // 将任务添加到任务队列
	}
	

	if (_thrd_task == NULL)  // 如果后台线程未启动
	{
		_thrd_task.reset(new StdThread([this]{  // 创建后台线程
			task_loop();  // 执行任务循环
		}));
	}

	_cond_task.notify_all();  // 通知所有等待的线程（有新任务）
}

/**
 * @brief 任务循环
 * 
 * 后台线程的主循环，从任务队列中取出任务并执行。
 * 使用条件变量等待新任务，避免忙等待。
 * 
 * 实现逻辑：
 * 1. 等待任务队列非空（使用条件变量）
 * 2. 一次性取出所有任务（交换队列）
 * 3. 执行所有任务
 * 4. 重复上述过程
 */
void WtEngine::task_loop()
{
	while (!_terminated)  // 循环直到引擎终止
	{
		TaskQueue temp;  // 临时任务队列
		{
			StdUniqueLock lock(_mtx_task);  // 获取任务队列互斥锁
			if(_task_queue.empty())  // 如果任务队列为空
			{
				_cond_task.wait(_mtx_task);  // 等待条件变量通知（释放锁并等待）
				continue;  // 继续循环
			}

			temp.swap(_task_queue);  // 交换任务队列（一次性取出所有任务）
		}

		for (;;)  // 执行所有任务
		{
			if(temp.empty())  // 如果临时队列为空
				break;  // 退出循环

			TaskItem& item = temp.front();  // 获取第一个任务
			item();  // 执行任务
			temp.pop();  // 移除第一个任务
		}
	}
}

/**
 * @brief 初始化风控模块
 * @param cfg 风控配置参数指针
 * @return bool 返回是否初始化成功
 * 
 * 动态加载风控模块DLL，创建风控监控器实例。
 * 
 * 实现逻辑：
 * 1. 检查配置是否启用
 * 2. 查找风控模块DLL（先查找工作目录，再查找模块目录）
 * 3. 加载DLL并获取创建和删除函数
 * 4. 创建风控监控器实例并初始化
 */
bool WtEngine::init_riskmon(WTSVariant* cfg)
{
	if (cfg == NULL)  // 如果配置为空
		return false;  // 返回失败

	if (!cfg->getBoolean("active"))  // 如果配置中未启用风控模块
		return false;  // 返回失败

	std::string module = DLLHelper::wrap_module(cfg->getCString("module"));  // 获取模块名称并包装（添加.dll后缀等）
	//先看工作目录下是否有对应模块
	std::string dllpath = WtHelper::getCWD() + module;  // 拼接工作目录路径和模块名
	//如果没有,则再看模块目录,即dll同目录下
	if (!StdFile::exists(dllpath.c_str()))  // 如果工作目录下不存在
		dllpath = WtHelper::getInstDir() + module;  // 使用模块目录路径（DLL同目录）

	DllHandle hInst = DLLHelper::load_library(dllpath.c_str());  // 加载DLL
	if (hInst == NULL)  // 如果加载失败
	{
		WTSLogger::log_by_cat("risk", LL_ERROR, "Riskmon module {} loading failed", dllpath.c_str());  // 记录错误日志
		return false;  // 返回失败
	}

	FuncCreateRiskMonFact creator = (FuncCreateRiskMonFact)DLLHelper::get_symbol(hInst, "createRiskMonFact");  // 获取创建函数指针
	if (creator == NULL)  // 如果获取失败
	{
		DLLHelper::free_library(hInst);  // 释放DLL
		WTSLogger::log_by_cat("risk", LL_ERROR, "Riskmon module {} is not compatible", module.c_str());  // 记录错误日志：模块不兼容
		return false;  // 返回失败
	}

	_risk_fact._module_inst = hInst;  // 保存DLL句柄
	_risk_fact._module_path = module;  // 保存模块路径
	_risk_fact._creator = creator;  // 保存创建函数指针
	_risk_fact._remover = (FuncDeleteRiskMonFact)DLLHelper::get_symbol(hInst, "deleteRiskMonFact");  // 获取删除函数指针
	_risk_fact._fact = _risk_fact._creator();  // 创建风控工厂实例

	const char* name = cfg->getCString("name");  // 获取风控监控器名称
	
	_risk_mon.reset(new WtRiskMonWrapper(_risk_fact._fact->createRiskMonotor(name), _risk_fact._fact));  // 创建风控监控器包装器（使用工厂创建监控器实例）
	_risk_mon->self()->init(this, cfg);  // 初始化风控监控器（传入引擎指针和配置）

	return true;  // 返回成功
}

/**
 * @brief 初始化输出文件
 * 
 * 初始化成交记录和平仓记录的输出文件。
 * 如果文件已存在，则以追加模式打开；如果文件不存在，则创建文件并写入表头。
 */
void WtEngine::init_outputs()
{
	std::string folder = WtHelper::getPortifolioDir();  // 获取组合目录路径
	std::string filename = folder + "trades.csv";  // 拼接成交记录文件名
	_trade_logs.reset(new BoostFile());  // 创建文件对象智能指针
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件（不存在）
		_trade_logs->create_or_open_file(filename.c_str());  // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			_trade_logs->write_file("code,time,direct,action,price,qty,fee\n");  // 写入CSV表头（合约代码、时间、方向、操作、价格、数量、手续费）
		}
		else  // 如果是已存在的文件
		{
			_trade_logs->seek_to_end();  // 定位到文件末尾（追加模式）
		}
	}

	filename = folder + "closes.csv";  // 拼接平仓记录文件名
	_close_logs.reset(new BoostFile());  // 创建文件对象智能指针
	{
		bool isNewFile = !BoostFile::exists(filename.c_str());  // 判断文件是否为新文件（不存在）
		_close_logs->create_or_open_file(filename.c_str());  // 创建或打开文件
		if (isNewFile)  // 如果是新文件
		{
			_close_logs->write_file("code,direct,opentime,openprice,closetime,closeprice,qty,profit,totalprofit\n");  // 写入CSV表头（合约代码、方向、开仓时间、开仓价格、平仓时间、平仓价格、数量、盈亏、总盈亏）
		}
		else  // 如果是已存在的文件
		{
			_close_logs->seek_to_end();  // 定位到文件末尾（追加模式）
		}
	}
}

/**
 * @brief 记录成交日志
 * @param stdCode 标准合约代码字符串
 * @param isLong 是否多仓，true表示多仓，false表示空仓
 * @param isOpen 是否开仓，true表示开仓，false表示平仓
 * @param curTime 成交时间戳
 * @param price 成交价格
 * @param qty 成交数量
 * @param fee 手续费，默认为0.0
 * 
 * 将成交记录写入CSV文件。
 */
void WtEngine::log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, double fee /* = 0.0 */)
{
	if (_trade_logs)  // 如果成交日志文件对象存在
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << curTime << "," << (isLong ? "LONG" : "SHORT") << "," << (isOpen ? "OPEN" : "CLOSE") << "," << price << "," << qty << "," << fee << "\n";  // 格式化成交记录（合约代码、时间、方向、操作、价格、数量、手续费）
		_trade_logs->write_file(ss.str());  // 写入文件
	}
}

/**
 * @brief 记录平仓日志
 * @param stdCode 标准合约代码字符串
 * @param isLong 是否多仓，true表示多仓，false表示空仓
 * @param openTime 开仓时间戳
 * @param openpx 开仓价格
 * @param closeTime 平仓时间戳
 * @param closepx 平仓价格
 * @param qty 平仓数量
 * @param profit 盈亏
 * @param totalprofit 总盈亏，默认为0
 * 
 * 将平仓记录写入CSV文件。
 */
void WtEngine::log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty, double profit, double totalprofit /* = 0 */)
{
	if (_close_logs)  // 如果平仓日志文件对象存在
	{
		std::stringstream ss;  // 创建字符串流
		ss << stdCode << "," << (isLong ? "LONG" : "SHORT") << "," << openTime << "," << openpx
			<< "," << closeTime << "," << closepx << "," << qty << "," << profit << ","
			<< totalprofit << "\n";  // 格式化平仓记录（合约代码、方向、开仓时间、开仓价格、平仓时间、平仓价格、数量、盈亏、总盈亏）
		_close_logs->write_file(ss.str());  // 写入文件
	}
}