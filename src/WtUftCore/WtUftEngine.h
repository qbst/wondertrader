/*!
 * \file WtUftEngine.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT引擎头文件
 *
 * 本文件定义了WtUftEngine类，是UFT（Ultra Fast Trading）策略运行的核心引擎。
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
 * - 管理策略上下文的注册和查找
 * - 处理市场数据的订阅和分发
 * - 提供商品信息、合约信息、交易时间等基础数据查询
 * - 管理交易日生命周期（开始、结束）
 * - 提供历史数据查询接口（tick、K线、订单队列等）
 */
#pragma once

#include <queue>  // 队列容器头文件
#include <functional>  // 函数对象头文件
#include <stdint.h>  // 标准整数类型定义

#include "ParserAdapter.h"  // 解析器适配器头文件

#include "../Includes/FasterDefs.h"  // Faster库定义
#include "../Includes/RiskMonDefs.h"  // 风控定义头文件

#include "../Share/StdUtils.hpp"  // 标准工具头文件
#include "../Share/DLLHelper.hpp"  // DLL动态加载辅助工具头文件

#include "../Share/BoostFile.hpp"  // Boost文件操作头文件

#include "../Includes/IUftStraCtx.h"  // UFT策略上下文接口头文件

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSSessionInfo;  // 前置声明：交易时段信息类
class WTSCommodityInfo;  // 前置声明：商品信息类
class WTSContractInfo;  // 前置声明：合约信息类

class IBaseDataMgr;  // 前置声明：基础数据管理器接口
class IHotMgr;  // 前置声明：热点管理器接口

class WTSVariant;  // 前置声明：配置变体类

class WTSTickData;  // 前置声明：Tick数据类
struct WTSBarStruct;  // 前置声明：K线结构体
class WTSTickSlice;  // 前置声明：Tick切片类
class WTSKlineSlice;  // 前置声明：K线切片类
class WTSPortFundInfo;  // 前置声明：组合资金信息类

class WtUftDtMgr;  // 前置声明：UFT数据管理器类
class TraderAdapterMgr;  // 前置声明：交易适配器管理器类

class EventNotifier;  // 前置声明：事件通知器类

typedef std::function<void()>	TaskItem;  // 任务项类型别名：无参数无返回值的函数对象


class WTSVariant;  // 前置声明：配置变体类
class WtUftRtTicker;  // 前置声明：UFT实时ticker类
class EventNotifier;  // 前置声明：事件通知器类

typedef std::shared_ptr<IUftStraCtx> UftContextPtr;  // UFT策略上下文智能指针类型别名

/**
 * @class WtUftEngine
 * @brief UFT引擎类
 * 
 * UFT策略运行的核心引擎，负责管理策略上下文、处理市场数据、管理时间等。
 * 实现IParserStub接口，接收行情解析器的数据推送。
 */
class WtUftEngine : public IParserStub
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建UFT引擎对象，初始化所有成员变量。
	 */
	WtUftEngine();
	
	/**
	 * @brief 析构函数
	 * 
	 * 销毁UFT引擎对象，清理所有资源。
	 */
	virtual ~WtUftEngine();

public:
	/**
	 * @brief 设置交易适配器管理器
	 * @param mgr 交易适配器管理器指针
	 * 
	 * 设置引擎使用的交易适配器管理器。
	 */
	inline void set_adapter_mgr(TraderAdapterMgr* mgr) { _adapter_mgr = mgr; }  // 设置交易适配器管理器

	/**
	 * @brief 设置日期时间
	 * @param curDate 当前日期（YYYYMMDD格式）
	 * @param curTime 当前时间（HHMMSS格式）
	 * @param curSecs 当前秒数（包含毫秒），默认0
	 * @param rawTime 原始时间（HHMMSS格式），默认0表示使用curTime
	 * 
	 * 设置引擎的当前日期和时间，并同步到全局辅助类。
	 */
	void set_date_time(uint32_t curDate, uint32_t curTime, uint32_t curSecs = 0, uint32_t rawTime = 0);

	/**
	 * @brief 设置交易日
	 * @param curTDate 当前交易日（YYYYMMDD格式）
	 * 
	 * 设置引擎的当前交易日，并同步到全局辅助类。
	 */
	void set_trading_date(uint32_t curTDate);

	/**
	 * @brief 获取当前日期
	 * @return 当前日期（YYYYMMDD格式）
	 * 
	 * 返回引擎的当前日期。
	 */
	inline uint32_t get_date() { return _cur_date; }  // 返回当前日期
	
	/**
	 * @brief 获取当前分钟时间
	 * @return 当前时间（HHMMSS格式）
	 * 
	 * 返回引擎的当前分钟时间（1分钟线时间）。
	 */
	inline uint32_t get_min_time() { return _cur_time; }  // 返回当前分钟时间
	
	/**
	 * @brief 获取原始时间
	 * @return 原始时间（HHMMSS格式）
	 * 
	 * 返回引擎的原始时间（真实时间）。
	 */
	inline uint32_t get_raw_time() { return _cur_raw_time; }  // 返回原始时间
	
	/**
	 * @brief 获取当前秒数
	 * @return 当前秒数（包含毫秒）
	 * 
	 * 返回引擎的当前秒数（包含毫秒）。
	 */
	inline uint32_t get_secs() { return _cur_secs; }  // 返回当前秒数
	
	/**
	 * @brief 获取交易日
	 * @return 当前交易日（YYYYMMDD格式）
	 * 
	 * 返回引擎的当前交易日。
	 */
	inline uint32_t get_trading_date() { return _cur_tdate; }  // 返回当前交易日

	/**
	 * @brief 获取基础数据管理器
	 * @return 基础数据管理器指针
	 * 
	 * 返回引擎使用的基础数据管理器。
	 */
	inline IBaseDataMgr*		get_basedata_mgr() { return _base_data_mgr; }  // 返回基础数据管理器
	
	/**
	 * @brief 获取交易时段信息
	 * @param sid 交易时段ID或合约代码
	 * @param isCode 是否为合约代码，默认false
	 * @return 交易时段信息指针，失败返回NULL
	 * 
	 * 根据交易时段ID或合约代码获取交易时段信息。
	 */
	WTSSessionInfo*		get_session_info(const char* sid, bool isCode = false);
	
	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准化合约代码
	 * @return 商品信息指针，失败返回NULL
	 * 
	 * 根据标准化合约代码获取商品信息。
	 */
	WTSCommodityInfo*	get_commodity_info(const char* stdCode);
	
	/**
	 * @brief 获取合约信息
	 * @param stdCode 标准化合约代码
	 * @return 合约信息指针，失败返回NULL
	 * 
	 * 根据标准化合约代码获取合约信息。
	 */
	WTSContractInfo*	get_contract_info(const char* stdCode);

	/**
	 * @brief 获取最新Tick数据
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * @return 最新Tick数据指针，失败返回NULL
	 * 
	 * 获取指定合约的最新Tick数据。
	 */
	WTSTickData*	get_last_tick(uint32_t sid, const char* stdCode);
	
	/**
	 * @brief 获取Tick数据切片
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return Tick数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的Tick数据切片。
	 */
	WTSTickSlice*	get_tick_slice(uint32_t sid, const char* stdCode, uint32_t count);
	
	/**
	 * @brief 获取K线数据切片
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m"表示分钟，"d"表示日）
	 * @param count 数据条数
	 * @param times 周期倍数，默认1
	 * @param etime 结束时间，默认0表示最新时间
	 * @return K线数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的K线数据切片，并记录订阅关系。
	 */
	WTSKlineSlice*	get_kline_slice(uint32_t sid, const char* stdCode, const char* period, uint32_t count, uint32_t times = 1, uint64_t etime = 0);

	/**
	 * @brief 订阅Tick数据
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * 
	 * 为指定策略上下文订阅指定合约的Tick数据。
	 */
	void sub_tick(uint32_t sid, const char* code);

	/**
	 * @brief 获取当前价格
	 * @param stdCode 标准化合约代码
	 * @return 当前价格，失败返回0.0
	 * 
	 * 获取指定合约的当前价格。
	 */
	double get_cur_price(const char* stdCode);

	/**
	 * @brief 通知参数更新
	 * @param name 策略名称
	 * 
	 * 通知指定策略的参数已更新。
	 */
	void notify_params_update(const char* name);

public:
	/**
	 * @brief 初始化引擎
	 * @param cfg 配置对象
	 * @param bdMgr 基础数据管理器指针
	 * @param dataMgr 数据管理器指针
	 * @param notifier 事件通知器指针
	 * 
	 * 初始化引擎，设置基础数据管理器、数据管理器、事件通知器等。
	 */
	void init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtUftDtMgr* dataMgr, EventNotifier* notifier);

	/**
	 * @brief 运行引擎
	 * 
	 * 启动引擎，初始化所有策略上下文，启动实时ticker。
	 */
	void run();

	/**
	 * @brief 处理Tick数据
	 * @param stdCode 标准化合约代码
	 * @param curTick 当前Tick数据
	 * 
	 * 处理收到的Tick数据，更新数据管理器并分发到订阅的策略上下文。
	 */
	void on_tick(const char* stdCode, WTSTickData* curTick);

	/**
	 * @brief 处理K线数据
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 * 
	 * 处理收到的K线数据，分发到订阅的策略上下文。
	 */
	void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar);

	/**
	 * @brief 初始化完成回调
	 * 
	 * 引擎初始化完成时调用，当前为空实现。
	 */
	void on_init(){}

	/**
	 * @brief 交易日开始回调
	 * 
	 * 交易日开始时调用，通知所有策略上下文。
	 */
	void on_session_begin();

	/**
	 * @brief 交易日结束回调
	 * 
	 * 交易日结束时调用，通知所有策略上下文。
	 */
	void on_session_end();

	/**
	 * @brief 处理行情推送（实现IParserStub接口）
	 * @param newTick 新的Tick数据
	 * 
	 * 接收行情解析器推送的Tick数据，转发给实时ticker处理。
	 */
	virtual void handle_push_quote(WTSTickData* newTick) override;
	
	/**
	 * @brief 处理订单明细推送（实现IParserStub接口）
	 * @param curOrdDtl 订单明细数据
	 * 
	 * 接收行情解析器推送的订单明细数据，分发到订阅的策略上下文。
	 */
	virtual void handle_push_order_detail(WTSOrdDtlData* curOrdDtl) override;
	
	/**
	 * @brief 处理订单队列推送（实现IParserStub接口）
	 * @param curOrdQue 订单队列数据
	 * 
	 * 接收行情解析器推送的订单队列数据，分发到订阅的策略上下文。
	 */
	virtual void handle_push_order_queue(WTSOrdQueData* curOrdQue) override;
	
	/**
	 * @brief 处理成交明细推送（实现IParserStub接口）
	 * @param curTrans 成交明细数据
	 * 
	 * 接收行情解析器推送的成交明细数据，分发到订阅的策略上下文。
	 */
	virtual void handle_push_transaction(WTSTransData* curTrans) override;

public:
	/**
	 * @brief 获取订单队列数据切片
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return 订单队列数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的订单队列数据切片。
	 */
	WTSOrdQueSlice* get_order_queue_slice(uint32_t sid, const char* stdCode, uint32_t count);
	
	/**
	 * @brief 获取订单明细数据切片
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return 订单明细数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的订单明细数据切片。
	 */
	WTSOrdDtlSlice* get_order_detail_slice(uint32_t sid, const char* stdCode, uint32_t count);
	
	/**
	 * @brief 获取成交明细数据切片
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return 成交明细数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的成交明细数据切片。
	 */
	WTSTransSlice* get_transaction_slice(uint32_t sid, const char* stdCode, uint32_t count);

public:
	/**
	 * @brief 分钟结束回调
	 * @param curDate 当前日期
	 * @param curTime 当前时间
	 * 
	 * 当分钟线结束时调用，当前为空实现。
	 */
	void on_minute_end(uint32_t curDate, uint32_t curTime);

	/**
	 * @brief 添加策略上下文
	 * @param ctx 策略上下文智能指针
	 * 
	 * 将策略上下文添加到引擎管理中。
	 */
	void addContext(UftContextPtr ctx);

	/**
	 * @brief 获取策略上下文
	 * @param id 策略上下文ID
	 * @return 策略上下文智能指针，不存在返回空指针
	 * 
	 * 根据ID查找并返回策略上下文。
	 */
	UftContextPtr	getContext(uint32_t id);

	/**
	 * @brief 订阅订单队列数据
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * 
	 * 为指定策略上下文订阅指定合约的订单队列数据。
	 */
	void sub_order_queue(uint32_t sid, const char* stdCode);
	
	/**
	 * @brief 订阅订单明细数据
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * 
	 * 为指定策略上下文订阅指定合约的订单明细数据。
	 */
	void sub_order_detail(uint32_t sid, const char* stdCode);
	
	/**
	 * @brief 订阅成交明细数据
	 * @param sid 策略上下文ID
	 * @param stdCode 标准化合约代码
	 * 
	 * 为指定策略上下文订阅指定合约的成交明细数据。
	 */
	void sub_transaction(uint32_t sid, const char* stdCode);

private:
	uint32_t		_cur_date;	// 当前日期（YYYYMMDD格式）
	uint32_t		_cur_time;		// 当前时间（HHMMSS格式），是1分钟线时间，比如0900，这个时候的1分钟线是0901，_cur_time也就是0901，这个是为了CTA里面方便
	uint32_t		_cur_raw_time;	// 当前真实时间（HHMMSS格式）
	uint32_t		_cur_secs;	// 当前秒数（包含毫秒）
	uint32_t		_cur_tdate;	// 当前交易日（YYYYMMDD格式）

	IBaseDataMgr*	_base_data_mgr;	// 基础数据管理器指针
	WtUftDtMgr*		_data_mgr;		// 数据管理器指针

	//By Wesley @ 2022.02.07
	//tick数据订阅项，first是contextid，second是订阅选项，0-原始订阅，1-前复权，2-后复权
	typedef wt_hashset<uint32_t> SubList;  // 订阅列表类型别名：策略上下文ID集合
	typedef wt_hashmap<std::string, SubList>	StraSubMap;  // 策略订阅映射表类型别名：key为合约代码，value为订阅该合约的策略上下文ID集合
	StraSubMap		_tick_sub_map;	// tick数据订阅表
	StraSubMap		_ordque_sub_map;	// 委托队列订阅表
	StraSubMap		_orddtl_sub_map;	// 委托明细订阅表
	StraSubMap		_trans_sub_map;		// 成交明细订阅表
	StraSubMap		_bar_sub_map;	// K线数据订阅表（key格式：合约代码-周期-倍数）	
	
	TraderAdapterMgr*	_adapter_mgr;  // 交易适配器管理器指针

	typedef wt_hashmap<uint32_t, UftContextPtr> ContextMap;  // 策略上下文映射表类型别名：key为策略上下文ID
	ContextMap		_ctx_map;  // 策略上下文映射表

	WtUftRtTicker*	_tm_ticker;  // 实时ticker指针
	WTSVariant*		_cfg;  // 配置对象指针

	bool			_dependent;	// 子策略独立记账标志

	EventNotifier*	_notifier;  // 事件通知器指针
};

NS_WTP_END  // WonderTrader命名空间结束