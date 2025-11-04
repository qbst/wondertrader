/*!
 * \file WtUftEngine.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT引擎实现文件
 *
 * 本文件实现了WtUftEngine类，是UFT策略运行的核心引擎。
 *
 * 设计逻辑：
 * 1. 策略上下文管理：管理所有策略上下文的生命周期，支持动态添加和查找
 * 2. 数据订阅管理：管理策略对各类市场数据的订阅（tick、订单队列、订单明细、成交明细、K线）
 * 3. 数据分发：接收市场数据推送，根据订阅关系分发到对应的策略上下文
 * 4. 时间管理：维护当前日期、时间、秒数、交易日等时间信息，同步到全局辅助类
 * 5. 交易会话管理：处理交易日开始和结束事件，通知所有策略上下文
 * 6. 解析器接口：实现IParserStub接口，接收行情解析器的数据推送
 *
 * 主要功能：
 * - 实现策略上下文的注册和查找
 * - 实现市场数据的订阅和分发
 * - 实现基础数据查询（商品信息、合约信息、交易时间等）
 * - 实现交易日生命周期管理（开始、结束）
 * - 实现历史数据查询接口（tick、K线、订单队列等）
 */
#define WIN32_LEAN_AND_MEAN  // Windows平台宏定义，排除不常用的Windows头文件

#include "WtUftEngine.h"  // UFT引擎头文件
#include "WtUftTicker.h"  // UFT实时ticker头文件
#include "WtUftDtMgr.h"  // UFT数据管理器头文件
#include "TraderAdapter.h"  // 交易适配器头文件
#include "WtHelper.h"  // 辅助工具类头文件

#include "../Share/decimal.h"  // 十进制数处理头文件
#include "../Share/StrUtil.hpp"  // 字符串工具头文件
#include "../Share/TimeUtils.hpp"  // 时间工具头文件

#include "../Includes/WTSVariant.hpp"  // WTS变体类头文件
#include "../Includes/IBaseDataMgr.h"  // 基础数据管理器接口头文件
#include "../Includes/WTSContractInfo.hpp"  // 合约信息头文件

#include "../WTSTools/WTSLogger.h"  // 日志工具头文件

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 构造函数实现
 * 
 * 创建UFT引擎对象，初始化所有成员变量。
 * 从系统获取当前日期和时间，并同步到全局辅助类。
 */
WtUftEngine::WtUftEngine()
	: _cfg(NULL)  // 初始化配置对象为NULL
	, _tm_ticker(NULL)  // 初始化实时ticker为NULL
	, _notifier(NULL)  // 初始化事件通知器为NULL
{
	TimeUtils::getDateTime(_cur_date, _cur_time);  // 从系统获取当前日期和时间
	_cur_secs = _cur_time % 100000;  // 提取秒数部分（包含毫秒）
	_cur_time /= 100000;  // 提取分钟部分（HHMM格式）
	_cur_raw_time = _cur_time;  // 保存原始时间
	_cur_tdate = _cur_date;  // 初始化交易日为当前日期

	WtHelper::setTime(_cur_date, _cur_time, _cur_secs);  // 同步时间到全局辅助类
}


/**
 * @brief 析构函数实现
 * 
 * 销毁UFT引擎对象，停止实时ticker，释放配置对象。
 */
WtUftEngine::~WtUftEngine()
{
	if (_tm_ticker)  // 如果实时ticker存在
	{
		_tm_ticker->stop();  // 停止实时ticker
		delete _tm_ticker;  // 删除实时ticker对象
		_tm_ticker = NULL;  // 置空指针
	}

	if (_cfg)  // 如果配置对象存在
		_cfg->release();  // 释放配置对象引用
}

/**
 * @brief 设置日期时间实现
 * @param curDate 当前日期（YYYYMMDD格式）
 * @param curTime 当前时间（HHMMSS格式）
 * @param curSecs 当前秒数（包含毫秒），默认0
 * @param rawTime 原始时间（HHMMSS格式），默认0表示使用curTime
 * 
 * 设置引擎的当前日期和时间，并同步到全局辅助类。
 */
void WtUftEngine::set_date_time(uint32_t curDate, uint32_t curTime, uint32_t curSecs /* = 0 */, uint32_t rawTime /* = 0 */)
{
	_cur_date = curDate;  // 设置当前日期
	_cur_time = curTime;  // 设置当前时间
	_cur_secs = curSecs;  // 设置当前秒数

	if (rawTime == 0)  // 如果原始时间为0
		rawTime = curTime;  // 使用当前时间作为原始时间

	_cur_raw_time = rawTime;  // 设置原始时间

	WtHelper::setTime(_cur_date, _cur_raw_time, _cur_secs);  // 同步时间到全局辅助类
}

/**
 * @brief 设置交易日实现
 * @param curTDate 当前交易日（YYYYMMDD格式）
 * 
 * 设置引擎的当前交易日，并同步到全局辅助类。
 */
void WtUftEngine::set_trading_date(uint32_t curTDate)
{
	_cur_tdate = curTDate;  // 设置当前交易日

	WtHelper::setTDate(curTDate);  // 同步交易日到全局辅助类
}

/**
 * @brief 获取商品信息实现
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如"SHFE.cu2301"）
 * @return 商品信息指针，失败返回NULL
 * 
 * 根据标准化合约代码获取商品信息。
 * 内部会解析合约代码，提取交易所和合约代码。
 */
WTSCommodityInfo* WtUftEngine::get_commodity_info(const char* stdCode)
{
	const StringVector& ay = StrUtil::split(stdCode, ".");  // 按"."分割标准化合约代码
	WTSContractInfo* cInfo = _base_data_mgr->getContract(ay[1].c_str(), ay[0].c_str());  // 获取合约信息（交易所，合约代码）
	if (cInfo == NULL)  // 如果合约信息不存在
		return NULL;  // 返回NULL

	return cInfo->getCommInfo();  // 返回商品信息
}

/**
 * @brief 获取合约信息实现
 * @param stdCode 标准化合约代码（格式：交易所.合约代码）
 * @return 合约信息指针，失败返回NULL
 * 
 * 根据标准化合约代码获取合约信息。
 */
WTSContractInfo* WtUftEngine::get_contract_info(const char* stdCode)
{
	const StringVector& ay = StrUtil::split(stdCode, ".");  // 按"."分割标准化合约代码
	return _base_data_mgr->getContract(ay[1].c_str(), ay[0].c_str());  // 获取合约信息（交易所，合约代码）
}

/**
 * @brief 获取交易时段信息实现
 * @param sid 交易时段ID或合约代码
 * @param isCode 是否为合约代码，默认false
 * @return 交易时段信息指针，失败返回NULL
 * 
 * 根据交易时段ID或合约代码获取交易时段信息。
 * 如果isCode为false，直接通过ID获取；否则通过合约代码获取对应的交易时段。
 */
WTSSessionInfo* WtUftEngine::get_session_info(const char* sid, bool isCode /* = false */)
{
	if (!isCode)  // 如果不是合约代码
		return _base_data_mgr->getSession(sid);  // 直接通过ID获取交易时段信息

	const StringVector& ay = StrUtil::split(sid, ".");  // 按"."分割合约代码
	WTSContractInfo* cInfo = _base_data_mgr->getContract(ay[1].c_str(), ay[0].c_str());  // 获取合约信息
	if (cInfo == NULL)  // 如果合约信息不存在
		return NULL;  // 返回NULL

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取商品信息
	return commInfo->getSessionInfo();  // 返回交易时段信息
}

/**
 * @brief 获取Tick数据切片实现
 * @param sid 策略上下文ID
 * @param code 合约代码
 * @param count 数据条数
 * @return Tick数据切片指针，当前返回NULL
 * 
 * 获取指定合约的Tick数据切片。
 * 注意：当前实现返回NULL，实际实现被注释。
 */
WTSTickSlice* WtUftEngine::get_tick_slice(uint32_t sid, const char* code, uint32_t count)
{
	return NULL;  // 当前未实现，返回NULL
	return _data_mgr->get_tick_slice(code, count);  // 实际实现（被注释）
}

/**
 * @brief 获取最新Tick数据实现
 * @param sid 策略上下文ID
 * @param stdCode 标准化合约代码
 * @return 最新Tick数据指针，失败返回NULL
 * 
 * 获取指定合约的最新Tick数据。
 */
WTSTickData* WtUftEngine::get_last_tick(uint32_t sid, const char* stdCode)
{
	return _data_mgr->grab_last_tick(stdCode);  // 从数据管理器获取最新tick数据
}

/**
 * @brief 获取K线数据切片实现
 * @param sid 策略上下文ID
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m"表示分钟，"d"表示日）
 * @param count 数据条数
 * @param times 周期倍数，默认1
 * @param etime 结束时间，默认0表示最新时间
 * @return K线数据切片指针，失败返回NULL
 * 
 * 获取指定合约的K线数据切片，并记录订阅关系。
 * 支持分钟线和日线，分钟线会优化5分钟倍数的情况。
 */
WTSKlineSlice* WtUftEngine::get_kline_slice(uint32_t sid, const char* stdCode, const char* period, uint32_t count, uint32_t times /* = 1 */, uint64_t etime /* = 0 */)
{
	return NULL;  // 当前未实现，返回NULL
	WTSCommodityInfo* cInfo = _base_data_mgr->getCommodity(stdCode);  // 获取商品信息
	if (cInfo == NULL)  // 如果商品信息不存在
		return NULL;  // 返回NULL

	WTSSessionInfo* sInfo = cInfo->getSessionInfo();  // 获取交易时段信息

	std::string key = fmt::format("{}-{}-{}", stdCode, period, times);  // 构建订阅key（合约代码-周期-倍数）
	SubList& sids = _bar_sub_map[key];  // 获取或创建订阅列表
	sids.insert(sid);  // 添加策略上下文ID到订阅列表

	WTSKlinePeriod kp;  // K线周期枚举
	if (strcmp(period, "m") == 0)  // 如果是分钟周期
	{
		if (times % 5 == 0)  // 如果是5的倍数，使用5分钟周期
		{
			kp = KP_Minute5;  // 设置为5分钟周期
			times /= 5;  // 倍数除以5
		}
		else  // 否则使用1分钟周期
			kp = KP_Minute1;  // 设置为1分钟周期
	}
	else  // 否则是日线周期
	{
		kp = KP_DAY;  // 设置为日线周期
	}

	return _data_mgr->get_kline_slice(stdCode, kp, times, count, etime);  // 从数据管理器获取K线数据切片
}

/**
 * @brief 订阅Tick数据实现
 * @param sid 策略上下文ID
 * @param stdCode 标准化合约代码
 * 
 * 为指定策略上下文订阅指定合约的Tick数据。
 */
void WtUftEngine::sub_tick(uint32_t sid, const char* stdCode)
{
	SubList& sids = _tick_sub_map[stdCode];  // 获取或创建该合约的订阅列表
	sids.insert(sid);  // 添加策略上下文ID到订阅列表
}

/**
 * @brief 获取当前价格实现
 * @param stdCode 标准化合约代码
 * @return 当前价格，失败返回0.0
 * 
 * 获取指定合约的当前价格。
 * 从数据管理器获取最新tick数据，提取价格后释放数据。
 */
double WtUftEngine::get_cur_price(const char* stdCode)
{
	WTSTickData* lastTick = _data_mgr->grab_last_tick(stdCode);  // 获取最新tick数据
	if (lastTick == NULL)  // 如果数据不存在
		return 0.0;  // 返回0.0

	double ret = lastTick->price();  // 获取价格
	lastTick->release();  // 释放数据引用
	return ret;  // 返回价格
}

/**
 * @brief 通知参数更新实现
 * @param name 策略名称
 * 
 * 通知指定策略的参数已更新。
 * 遍历所有策略上下文，找到匹配名称的策略并调用参数更新回调。
 */
void WtUftEngine::notify_params_update(const char* name)
{
	for(auto& v : _ctx_map)  // 遍历所有策略上下文
	{
		const UftContextPtr& context = v.second;  // 获取策略上下文
		if(strcmp(context->name(), name) == 0)  // 如果策略名称匹配
		{
			context->on_params_updated();  // 调用参数更新回调
			break;  // 找到后退出循环
		}
	}
}

/**
 * @brief 初始化引擎实现
 * @param cfg 配置对象
 * @param bdMgr 基础数据管理器指针
 * @param dataMgr 数据管理器指针
 * @param notifier 事件通知器指针
 * 
 * 初始化引擎，设置基础数据管理器、数据管理器、事件通知器等。
 */
void WtUftEngine::init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtUftDtMgr* dataMgr, EventNotifier* notifier)
{
	_base_data_mgr = bdMgr;  // 保存基础数据管理器指针
	_data_mgr = dataMgr;  // 保存数据管理器指针
	_notifier = notifier;  // 保存事件通知器指针

	_cfg = cfg;  // 保存配置对象指针
	if(_cfg) _cfg->retain();  // 如果配置对象存在，增加引用计数
}

/**
 * @brief 运行引擎实现
 * 
 * 启动引擎，初始化所有策略上下文，启动实时ticker。
 * 流程：
 * 1. 遍历所有策略上下文，调用on_init初始化
 * 2. 创建实时ticker对象
 * 3. 从配置中读取交易时段，如果不存在则使用"ALLDAY"
 * 4. 启动实时ticker
 */
void WtUftEngine::run()
{
	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)  // 遍历所有策略上下文
	{
		UftContextPtr& ctx = (UftContextPtr&)it->second;  // 获取策略上下文
		ctx->on_init();  // 调用初始化回调
	}

	_tm_ticker = new WtUftRtTicker(this);  // 创建实时ticker对象
	if(_cfg && _cfg->has("product"))  // 如果配置存在且包含"product"项
	{
		WTSVariant* cfgProd = _cfg->get("product");  // 获取产品配置
		_tm_ticker->init(cfgProd->getCString("session"));  // 从配置中读取交易时段并初始化ticker
	}
	else  // 否则使用默认交易时段
	{
		_tm_ticker->init("ALLDAY");  // 使用"ALLDAY"交易时段初始化ticker
	}

	_tm_ticker->run();  // 启动实时ticker
}

/**
 * @brief 处理行情推送实现
 * @param newTick 新的Tick数据
 * 
 * 接收行情解析器推送的Tick数据，转发给实时ticker处理。
 */
void WtUftEngine::handle_push_quote(WTSTickData* newTick)
{
	if (_tm_ticker)  // 如果实时ticker存在
		_tm_ticker->on_tick(newTick);  // 转发给实时ticker处理
}

/**
 * @brief 处理订单明细推送实现
 * @param curOrdDtl 订单明细数据
 * 
 * 接收行情解析器推送的订单明细数据，分发到订阅的策略上下文。
 * Level2数据一般用于HFT场景，所以不做复权处理。
 */
void WtUftEngine::handle_push_order_detail(WTSOrdDtlData* curOrdDtl)
{
	const char* stdCode = curOrdDtl->code();  // 获取合约代码
	auto sit = _orddtl_sub_map.find(stdCode);  // 查找订阅映射
	if (sit != _orddtl_sub_map.end())  // 如果找到订阅
	{
		const SubList& sids = sit->second;  // 获取订阅列表
		for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略上下文ID
		{
			//By Wesley @ 2022.02.07
			//Level2数据一般用于HFT场景，所以不做复权处理
			//所以不读取订阅标记
			uint32_t sid = *it;  // 获取策略上下文ID
			auto cit = _ctx_map.find(sid);  // 查找策略上下文
			if (cit != _ctx_map.end())  // 如果找到
			{
				UftContextPtr& ctx = (UftContextPtr&)cit->second;  // 获取策略上下文
				ctx->on_order_detail(stdCode, curOrdDtl);  // 调用订单明细回调
			}
		}
	}
}

/**
 * @brief 处理订单队列推送实现
 * @param curOrdQue 订单队列数据
 * 
 * 接收行情解析器推送的订单队列数据，分发到订阅的策略上下文。
 * Level2数据一般用于HFT场景，所以不做复权处理。
 */
void WtUftEngine::handle_push_order_queue(WTSOrdQueData* curOrdQue)
{
	const char* stdCode = curOrdQue->code();  // 获取合约代码
	auto sit = _ordque_sub_map.find(stdCode);  // 查找订阅映射
	if (sit != _ordque_sub_map.end())  // 如果找到订阅
	{
		const SubList& sids = sit->second;  // 获取订阅列表
		for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略上下文ID
		{
			//By Wesley @ 2022.02.07
			//Level2数据一般用于HFT场景，所以不做复权处理
			//所以不读取订阅标记
			uint32_t sid = *it;  // 获取策略上下文ID
			auto cit = _ctx_map.find(sid);  // 查找策略上下文
			if (cit != _ctx_map.end())  // 如果找到
			{
				UftContextPtr& ctx = (UftContextPtr&)cit->second;  // 获取策略上下文
				ctx->on_order_queue(stdCode, curOrdQue);  // 调用订单队列回调
			}
		}
	}
}

/**
 * @brief 处理成交明细推送实现
 * @param curTrans 成交明细数据
 * 
 * 接收行情解析器推送的成交明细数据，分发到订阅的策略上下文。
 * Level2数据一般用于HFT场景，所以不做复权处理。
 */
void WtUftEngine::handle_push_transaction(WTSTransData* curTrans)
{
	const char* stdCode = curTrans->code();  // 获取合约代码
	auto sit = _trans_sub_map.find(stdCode);  // 查找订阅映射
	if (sit != _trans_sub_map.end())  // 如果找到订阅
	{
		const SubList& sids = sit->second;  // 获取订阅列表
		for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略上下文ID
		{
			//By Wesley @ 2022.02.07
			//Level2数据一般用于HFT场景，所以不做复权处理
			//所以不读取订阅标记
			uint32_t sid = *it;  // 获取策略上下文ID
			auto cit = _ctx_map.find(sid);  // 查找策略上下文
			if (cit != _ctx_map.end())  // 如果找到
			{
				UftContextPtr& ctx = (UftContextPtr&)cit->second;  // 获取策略上下文
				ctx->on_transaction(stdCode, curTrans);  // 调用成交明细回调
			}
		}
	}
}

/**
 * @brief 订阅订单明细数据实现
 * @param sid 策略上下文ID
 * @param stdCode 标准化合约代码
 * 
 * 为指定策略上下文订阅指定合约的订单明细数据。
 */
void WtUftEngine::sub_order_detail(uint32_t sid, const char* stdCode)
{
	SubList& sids = _orddtl_sub_map[stdCode];  // 获取或创建该合约的订阅列表
	sids.insert(sid);  // 添加策略上下文ID到订阅列表
}

/**
 * @brief 订阅订单队列数据实现
 * @param sid 策略上下文ID
 * @param stdCode 标准化合约代码
 * 
 * 为指定策略上下文订阅指定合约的订单队列数据。
 */
void WtUftEngine::sub_order_queue(uint32_t sid, const char* stdCode)
{
	SubList& sids = _ordque_sub_map[stdCode];  // 获取或创建该合约的订阅列表
	sids.insert(sid);  // 添加策略上下文ID到订阅列表
}

/**
 * @brief 订阅成交明细数据实现
 * @param sid 策略上下文ID
 * @param stdCode 标准化合约代码
 * 
 * 为指定策略上下文订阅指定合约的成交明细数据。
 */
void WtUftEngine::sub_transaction(uint32_t sid, const char* stdCode)
{
	SubList& sids = _trans_sub_map[stdCode];  // 获取或创建该合约的订阅列表
	sids.insert(sid);  // 添加策略上下文ID到订阅列表
}

/**
 * @brief 交易日开始回调实现
 * 
 * 交易日开始时调用，通知所有策略上下文。
 * 记录日志并调用所有策略上下文的on_session_begin回调。
 */
void WtUftEngine::on_session_begin()
{
	WTSLogger::info("Trading day {} begun", _cur_tdate);  // 记录交易日开始日志

	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)  // 遍历所有策略上下文
	{
		UftContextPtr& ctx = (UftContextPtr&)it->second;  // 获取策略上下文
		ctx->on_session_begin(_cur_tdate);  // 调用交易日开始回调
	}
}

/**
 * @brief 交易日结束回调实现
 * 
 * 交易日结束时调用，通知所有策略上下文。
 * 先调用所有策略上下文的on_session_end回调，然后记录日志。
 */
void WtUftEngine::on_session_end()
{
	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)  // 遍历所有策略上下文
	{
		UftContextPtr& ctx = (UftContextPtr&)it->second;  // 获取策略上下文
		ctx->on_session_end(_cur_tdate);  // 调用交易日结束回调
	}

	WTSLogger::info("Trading day {} ended", _cur_tdate);  // 记录交易日结束日志
}

/**
 * @brief 处理Tick数据实现
 * @param stdCode 标准化合约代码
 * @param curTick 当前Tick数据
 * 
 * 处理收到的Tick数据，更新数据管理器并分发到订阅的策略上下文。
 * 流程：
 * 1. 将tick数据推送到数据管理器
 * 2. 查找订阅该合约的策略上下文
 * 3. 将tick数据分发到所有订阅的策略上下文
 */
void WtUftEngine::on_tick(const char* stdCode, WTSTickData* curTick)
{
	if(_data_mgr)  // 如果数据管理器存在
		_data_mgr->handle_push_quote(stdCode, curTick);  // 推送tick数据到数据管理器

	{
		auto sit = _tick_sub_map.find(stdCode);  // 查找订阅映射
		if (sit != _tick_sub_map.end())  // 如果找到订阅
		{
			const SubList& sids = sit->second;  // 获取订阅列表
			for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略上下文ID
			{
				uint32_t sid = *it;  // 获取策略上下文ID

				auto cit = _ctx_map.find(sid);  // 查找策略上下文
				if (cit != _ctx_map.end())  // 如果找到
				{
					UftContextPtr& ctx = (UftContextPtr&)cit->second;  // 获取策略上下文
					ctx->on_tick(stdCode, curTick);  // 调用tick回调
				}
			}
		}
	}
}

/**
 * @brief 处理K线数据实现
 * @param stdCode 标准化合约代码
 * @param period 周期字符串
 * @param times 周期倍数
 * @param newBar 新的K线数据
 * 
 * 处理收到的K线数据，分发到订阅的策略上下文。
 */
void WtUftEngine::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	std::string key = fmt::format("{}-{}-{}", stdCode, period, times);  // 构建订阅key（合约代码-周期-倍数）
	const SubList& sids = _bar_sub_map[key];  // 获取订阅列表
	for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略上下文ID
	{
		uint32_t sid = *it;  // 获取策略上下文ID
		auto cit = _ctx_map.find(sid);  // 查找策略上下文
		if (cit != _ctx_map.end())  // 如果找到
		{
			UftContextPtr& ctx = (UftContextPtr&)cit->second;  // 获取策略上下文
			ctx->on_bar(stdCode, period, times, newBar);  // 调用K线回调
		}
	}
}

/**
 * @brief 分钟结束回调实现
 * @param curDate 当前日期
 * @param curTime 当前时间
 * 
 * 当分钟线结束时调用，当前为空实现。
 */
void WtUftEngine::on_minute_end(uint32_t curDate, uint32_t curTime)
{

}

/**
 * @brief 添加策略上下文实现
 * @param ctx 策略上下文智能指针
 * 
 * 将策略上下文添加到引擎管理中。
 */
void WtUftEngine::addContext(UftContextPtr ctx)
{
	uint32_t sid = ctx->id();  // 获取策略上下文ID
	_ctx_map[sid] = ctx;  // 添加到映射表（key为策略上下文ID）
}

/**
 * @brief 获取策略上下文实现
 * @param id 策略上下文ID
 * @return 策略上下文智能指针，不存在返回空指针
 * 
 * 根据ID查找并返回策略上下文。
 */
UftContextPtr WtUftEngine::getContext(uint32_t id)
{
	auto it = _ctx_map.find(id);  // 查找策略上下文
	if (it == _ctx_map.end())  // 如果不存在
		return UftContextPtr();  // 返回空指针

	return it->second;  // 返回策略上下文智能指针
}

/**
 * @brief 获取订单队列数据切片实现
 * @param sid 策略上下文ID
 * @param code 合约代码
 * @param count 数据条数
 * @return 订单队列数据切片指针，失败返回NULL
 * 
 * 获取指定合约的订单队列数据切片。
 */
WTSOrdQueSlice* WtUftEngine::get_order_queue_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_order_queue_slice(code, count);  // 从数据管理器获取订单队列数据切片
}

/**
 * @brief 获取订单明细数据切片实现
 * @param sid 策略上下文ID
 * @param code 合约代码
 * @param count 数据条数
 * @return 订单明细数据切片指针，失败返回NULL
 * 
 * 获取指定合约的订单明细数据切片。
 */
WTSOrdDtlSlice* WtUftEngine::get_order_detail_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_order_detail_slice(code, count);  // 从数据管理器获取订单明细数据切片
}

/**
 * @brief 获取成交明细数据切片实现
 * @param sid 策略上下文ID
 * @param code 合约代码
 * @param count 数据条数
 * @return 成交明细数据切片指针，失败返回NULL
 * 
 * 获取指定合约的成交明细数据切片。
 */
WTSTransSlice* WtUftEngine::get_transaction_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_transaction_slice(code, count);  // 从数据管理器获取成交明细数据切片
}