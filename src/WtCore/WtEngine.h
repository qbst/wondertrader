/*!
 * \file WtEngine.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易引擎基类头文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件定义了WtEngine类，这是WonderTrader交易引擎的核心基类。
 * 
 * 核心功能：
 * 1. 时间管理：管理当前日期、时间、交易日等时间信息
 * 2. 数据访问：提供合约信息、行情数据、K线数据等访问接口
 * 3. 持仓管理：管理组合持仓，包括持仓明细、盈亏计算等
 * 4. 资金管理：管理组合资金，包括余额、盈亏、手续费等
 * 5. 信号处理：处理策略发出的交易信号，支持延迟触发机制
 * 6. 风控管理：集成风控模块，支持仓位缩放等风控功能
 * 7. 任务调度：使用后台线程处理持仓更新、资金更新等任务
 * 8. 日志记录：记录成交记录、平仓记录等交易日志
 * 
 * 设计特点：
 * - 继承自WtPortContext和IParserStub，实现组合上下文和行情解析接口
 * - 使用后台任务线程处理耗时操作，避免阻塞主线程
 * - 支持多策略共享同一引擎实例
 * - 支持信号过滤机制，可以过滤或调整交易信号
 * - 支持手续费模板配置，可以为不同品种配置不同的手续费
 * - 支持持仓明细管理，记录每笔开仓的详细信息
 * 
 * 与策略的关系：
 * - 策略通过引擎访问数据和执行交易
 * - 引擎接收策略的交易信号，转换为实际持仓
 * - 引擎管理策略的持仓和资金，提供统一的资金管理
 * 
 * 工作流程：
 * 1. 初始化：加载配置，初始化数据管理器、风控模块等
 * 2. 运行：启动后台线程，处理行情数据和策略信号
 * 3. 交易：接收策略信号，更新持仓和资金
 * 4. 结算：在交易日结束时进行资金结算
 */
#pragma once  // 防止头文件重复包含
#include <queue>  // 包含标准队列容器头文件（用于任务队列）
#include <functional>  // 包含函数对象头文件（用于定义任务项类型）
#include <stdint.h>  // 包含标准整数类型头文件

#include "ParserAdapter.h"  // 包含行情解析适配器头文件
#include "WtFilterMgr.h"  // 包含过滤器管理器头文件


#include "../Includes/FasterDefs.h"  // 包含快速定义头文件（wt_hashmap等）
#include "../Includes/RiskMonDefs.h"  // 包含风控定义头文件

#include "../Share/StdUtils.hpp"  // 包含标准工具头文件（线程、互斥锁等）
#include "../Share/DLLHelper.hpp"  // 包含动态库加载工具头文件

#include "../Share/BoostFile.hpp"  // 包含Boost文件操作头文件
#include "../Share/SpinMutex.hpp"  // 包含自旋锁头文件


NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSSessionInfo;  // 前向声明：交易会话信息类
class WTSCommodityInfo;  // 前向声明：品种信息类
class WTSContractInfo;  // 前向声明：合约信息类

class IBaseDataMgr;  // 前向声明：基础数据管理器接口
class IHotMgr;  // 前向声明：热点合约管理器接口

class WTSVariant;  // 前向声明：变体类型，用于配置参数传递

class WTSTickData;  // 前向声明：Tick数据类
struct WTSBarStruct;  // 前向声明：K线结构体
class WTSTickSlice;  // 前向声明：Tick切片类
class WTSKlineSlice;  // 前向声明：K线切片类
class WTSPortFundInfo;  // 前向声明：组合资金信息类

class WtDtMgr;  // 前向声明：数据管理器类
class TraderAdapterMgr;  // 前向声明：交易适配器管理器类

class EventNotifier;  // 前向声明：事件通知器类

typedef std::function<void()>	TaskItem;  // 任务项类型定义：无参数无返回值的函数对象

/**
 * @class WtRiskMonWrapper
 * @brief 风控监视器包装类
 * 
 * 用于管理风控监视器的生命周期，确保在析构时正确释放风控监视器资源。
 * 使用智能指针管理，避免内存泄漏。
 */
class WtRiskMonWrapper
{
public:
	/**
	 * @brief 构造函数
	 * @param mon 风控监视器指针
	 * @param fact 风控监视器工厂指针
	 * 
	 * 初始化包装类，保存风控监视器和工厂指针。
	 */
	WtRiskMonWrapper(WtRiskMonitor* mon, IRiskMonitorFact* fact) :_mon(mon), _fact(fact){}  // 初始化列表：设置监视器和工厂指针
	/**
	 * @brief 析构函数
	 * 
	 * 如果监视器存在，通过工厂释放监视器资源。
	 */
	~WtRiskMonWrapper()
	{
		if (_mon)  // 如果监视器存在
		{
			_fact->deleteRiskMonotor(_mon);  // 通过工厂删除监视器（释放资源）
		}
	}

	/**
	 * @brief 获取监视器指针
	 * @return WtRiskMonitor* 返回监视器指针
	 */
	WtRiskMonitor* self(){ return _mon; }  // 返回内部监视器指针


private:
	WtRiskMonitor*		_mon;  // 风控监视器指针
	IRiskMonitorFact*	_fact;  // 风控监视器工厂指针
};
typedef std::shared_ptr<WtRiskMonWrapper>	WtRiskMonPtr;  // 风控监视器智能指针类型定义

/**
 * @class IEngineEvtListener
 * @brief 引擎事件监听器接口
 * 
 * 定义引擎事件的监听接口，子类可以实现这些接口来监听引擎的各种事件。
 */
class IEngineEvtListener
{
public:
	/**
	 * @brief 初始化事件回调
	 * 
	 * 当引擎初始化完成时被调用。
	 */
	virtual void on_initialize_event() {}  // 初始化事件回调（空实现，子类可重写）
	/**
	 * @brief 定时事件回调
	 * @param uDate 日期（格式：YYYYMMDD）
	 * @param uTime 时间（格式：HHMM）
	 * 
	 * 当引擎定时触发时被调用。
	 */
	virtual void on_schedule_event(uint32_t uDate, uint32_t uTime) {}  // 定时事件回调（空实现，子类可重写）
	/**
	 * @brief 会话事件回调
	 * @param uDate 日期（格式：YYYYMMDD）
	 * @param isBegin 是否为会话开始，true表示开始，false表示结束
	 * 
	 * 当交易会话开始或结束时被调用。
	 */
	virtual void on_session_event(uint32_t uDate, bool isBegin = true) {}  // 会话事件回调（空实现，子类可重写）
};

/**
 * @class WtEngine
 * @brief 交易引擎基类
 * 
 * 该类是WonderTrader交易引擎的核心基类，提供数据访问、持仓管理、资金管理等功能。
 * 继承自WtPortContext（组合上下文接口）和IParserStub（行情解析接口）。
 * 
 * 主要职责：
 * 1. 管理时间信息（当前日期、时间、交易日等）
 * 2. 提供数据访问接口（合约信息、行情数据、K线数据等）
 * 3. 管理组合持仓（持仓明细、盈亏计算等）
 * 4. 管理组合资金（余额、盈亏、手续费等）
 * 5. 处理交易信号（接收策略信号，转换为持仓）
 * 6. 集成风控模块（仓位缩放、风控检查等）
 * 7. 任务调度（后台线程处理耗时操作）
 * 8. 日志记录（成交记录、平仓记录等）
 * 
 * 设计模式：
 * - 模板方法模式：定义run()等虚函数，子类实现具体逻辑
 * - 观察者模式：通过IEngineEvtListener监听引擎事件
 * - 策略模式：通过WtFilterMgr实现信号过滤策略
 */
class WtEngine : public WtPortContext, public IParserStub  // 继承组合上下文接口和行情解析接口
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化引擎，设置默认时间，初始化成员变量。
	 */
	WtEngine();

	/**
	 * @brief 设置交易适配器管理器
	 * @param mgr 交易适配器管理器指针
	 * 
	 * 设置交易适配器管理器，用于访问交易接口。
	 */
	inline void set_adapter_mgr(TraderAdapterMgr* mgr) { _adapter_mgr = mgr; }  // 设置交易适配器管理器指针

	/**
	 * @brief 设置当前日期和时间
	 * @param curDate 当前日期（格式：YYYYMMDD）
	 * @param curTime 当前时间（格式：HHMM）
	 * @param curSecs 当前秒数（包含毫秒，格式：SSmmm），默认为0
	 * @param rawTime 当前真实时间（格式：HHMM），默认为0（使用curTime）
	 * 
	 * 设置引擎的当前日期和时间信息。
	 * _cur_time是1分钟线时间，比如0900，这个时候的1分钟线是0901，_cur_time也就是0901。
	 * 这个设计是为了CTA策略里面方便使用。
	 */
	void set_date_time(uint32_t curDate, uint32_t curTime, uint32_t curSecs = 0, uint32_t rawTime = 0);

	/**
	 * @brief 设置当前交易日
	 * @param curTDate 当前交易日（格式：YYYYMMDD）
	 * 
	 * 设置引擎的当前交易日信息。
	 */
	void set_trading_date(uint32_t curTDate);

	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
	 */
	inline uint32_t get_date() { return _cur_date; }  // 返回当前日期
	/**
	 * @brief 获取当前分钟时间
	 * @return uint32_t 返回当前分钟时间（格式：HHMM）
	 * 
	 * 注意：这是1分钟线时间，比如0900，这个时候的1分钟线是0901，_cur_time也就是0901。
	 */
	inline uint32_t get_min_time() { return _cur_time; }  // 返回当前分钟时间
	/**
	 * @brief 获取当前真实时间
	 * @return uint32_t 返回当前真实时间（格式：HHMM）
	 */
	inline uint32_t get_raw_time() { return _cur_raw_time; }  // 返回当前真实时间
	/**
	 * @brief 获取当前秒数
	 * @return uint32_t 返回当前秒数（包含毫秒，格式：SSmmm）
	 */
	inline uint32_t get_secs() { return _cur_secs; }  // 返回当前秒数
	/**
	 * @brief 获取当前交易日
	 * @return uint32_t 返回当前交易日（格式：YYYYMMDD）
	 */
	inline uint32_t get_trading_date() { return _cur_tdate; }  // 返回当前交易日

	/**
	 * @brief 获取基础数据管理器
	 * @return IBaseDataMgr* 返回基础数据管理器指针
	 */
	inline IBaseDataMgr*		get_basedata_mgr(){ return _base_data_mgr; }  // 返回基础数据管理器指针
	/**
	 * @brief 获取热点合约管理器
	 * @return IHotMgr* 返回热点合约管理器指针
	 */
	inline IHotMgr*				get_hot_mgr() { return _hot_mgr; }  // 返回热点合约管理器指针
	/**
	 * @brief 获取交易会话信息
	 * @param sid 会话ID或合约代码字符串
	 * @param isCode 是否为合约代码，true表示是合约代码，false表示是会话ID，默认为false
	 * @return WTSSessionInfo* 返回交易会话信息指针，如果不存在返回NULL
	 * 
	 * 根据会话ID或合约代码获取交易会话信息。
	 */
	WTSSessionInfo*		get_session_info(const char* sid, bool isCode = false);
	/**
	 * @brief 获取品种信息
	 * @param stdCode 标准合约代码字符串
	 * @return WTSCommodityInfo* 返回品种信息指针，如果不存在返回NULL
	 * 
	 * 根据标准合约代码获取品种信息。
	 */
	WTSCommodityInfo*	get_commodity_info(const char* stdCode);
	/**
	 * @brief 获取合约信息
	 * @param stdCode 标准合约代码字符串
	 * @return WTSContractInfo* 返回合约信息指针，如果不存在返回NULL
	 * 
	 * 根据标准合约代码获取合约信息。
	 */
	WTSContractInfo*	get_contract_info(const char* stdCode);
	/**
	 * @brief 获取原始合约代码
	 * @param stdCode 标准合约代码字符串
	 * @return std::string 返回原始合约代码字符串，如果不存在返回空字符串
	 * 
	 * 对于主力合约代码（如SHFE.ag.HOT），转换为实际合约代码（如SHFE.ag.1912）。
	 */
	std::string			get_rawcode(const char* stdCode);

	/**
	 * @brief 获取最后一个Tick数据
	 * @param sid 策略ID（上下文ID）
	 * @param stdCode 标准合约代码字符串
	 * @return WTSTickData* 返回最后一个Tick数据指针，如果不存在返回NULL
	 * 
	 * 从数据管理器获取最新的Tick数据。
	 */
	WTSTickData*	get_last_tick(uint32_t sid, const char* stdCode);
	/**
	 * @brief 获取Tick数据切片
	 * @param sid 策略ID（上下文ID）
	 * @param stdCode 标准合约代码字符串
	 * @param count 获取的Tick数量
	 * @return WTSTickSlice* 返回Tick数据切片指针，如果不存在返回NULL
	 * 
	 * 从数据管理器获取指定数量的Tick数据。
	 */
	WTSTickSlice*	get_tick_slice(uint32_t sid, const char* stdCode, uint32_t count);
	/**
	 * @brief 获取K线数据切片
	 * @param sid 策略ID（上下文ID）
	 * @param stdCode 标准合约代码字符串
	 * @param period 周期字符串（如"m1"表示1分钟，"d"表示日线）
	 * @param count 获取的K线数量
	 * @param times K线倍数，默认为1
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSKlineSlice* 返回K线数据切片指针，如果不存在返回NULL
	 * 
	 * 从数据管理器获取指定数量和周期的K线数据。
	 */
	WTSKlineSlice*	get_kline_slice(uint32_t sid, const char* stdCode, const char* period, uint32_t count, uint32_t times = 1, uint64_t etime = 0);

	/**
	 * @brief 订阅Tick数据
	 * @param sid 策略ID（上下文ID）
	 * @param code 合约代码字符串
	 * 
	 * 注册策略对指定合约的Tick数据订阅。
	 * 支持原始订阅、前复权订阅和后复权订阅。
	 */
	void sub_tick(uint32_t sid, const char* code);

	/**
	 * @brief 获取当前价格
	 * @param stdCode 标准合约代码字符串
	 * @return double 返回当前价格，如果不存在返回0.0
	 * 
	 * 从价格缓存或最新Tick数据中获取当前价格。
	 * 支持不复权、前复权和后复权合约。
	 */
	double get_cur_price(const char* stdCode);

	/**
	 * @brief 获取当日价格
	 * @param stdCode 标准合约代码字符串
	 * @param flag 价格类型标志，0=开盘价，1=最高价，2=最低价，3=最新价，默认为0
	 * @return double 返回当日价格，如果不存在返回0.0
	 * 
	 * 从最新Tick数据中获取当日的指定价格。
	 * 支持不复权、前复权和后复权合约。
	 */
	double get_day_price(const char* stdCode, int flag = 0);

	/**
	 * @brief 获取复权因子
	 * @param stdCode 合约代码字符串
	 * @param commInfo 品种信息指针，如果为NULL则自动获取，默认为NULL
	 * @return double 返回复权因子，如果不存在返回1.0
	 * 
	 * 获取指定合约的复权因子。
	 * 对于股票，从数据管理器获取；对于期货，从热点管理器获取。
	 */
	double get_exright_factor(const char* stdCode, WTSCommodityInfo* commInfo = NULL);

	/**
	 * @brief 获取复权标志
	 * @return uint32_t 返回复权标志（0=不复权，1=前复权，2=后复权）
	 * 
	 * 从数据管理器获取复权标志。
	 */
	uint32_t get_adjusting_flag();

	/**
	 * @brief 计算手续费
	 * @param stdCode 标准合约代码字符串
	 * @param price 价格
	 * @param qty 数量
	 * @param offset 开平仓标志，0=开仓，1=平仓，2=平今
	 * @return double 返回手续费金额（保留两位小数）
	 * 
	 * 根据手续费模板计算手续费。
	 * 支持按手数和按金额两种计费方式。
	 */
	double calc_fee(const char* stdCode, double price, double qty, uint32_t offset);

	/**
	 * @brief 设置风控监视器
	 * @param monitor 风控监视器智能指针引用
	 * 
	 * 设置引擎的风控监视器，用于风控检查。
	 */
	inline void setRiskMonitor(WtRiskMonPtr& monitor)
	{
		_risk_mon = monitor;  // 保存风控监视器智能指针
	}

	/**
	 * @brief 注册事件监听器
	 * @param listener 事件监听器指针
	 * 
	 * 设置引擎的事件监听器，用于监听引擎事件。
	 */
	inline void regEventListener(IEngineEvtListener* listener)
	{
		_evt_listener = listener;  // 保存事件监听器指针
	}

	//////////////////////////////////////////////////////////////////////////
	// WtPortContext接口实现
	// 以下方法实现WtPortContext接口，为组合提供上下文信息
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 获取组合资金信息
	 * @return WTSPortFundInfo* 返回组合资金信息指针
	 * 
	 * 获取组合的资金信息，包括余额、盈亏、手续费等。
	 * 会自动更新浮动盈亏。
	 */
	virtual WTSPortFundInfo* getFundInfo() override;
	/**
	 * @brief 设置仓位缩放系数
	 * @param scale 缩放系数（大于0）
	 * 
	 * 设置风控仓位缩放系数，用于控制仓位大小。
	 */
	virtual void setVolScale(double scale) override;
	/**
	 * @brief 判断是否在交易中
	 * @return bool 返回是否在交易中（引擎基类返回false，子类可重写）
	 */
	virtual bool isInTrading() override;
	/**
	 * @brief 写入风控日志
	 * @param message 日志消息字符串
	 * 
	 * 记录风控相关的日志信息。
	 */
	virtual void writeRiskLog(const char* message) override;
	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
	 */
	virtual uint32_t	getCurDate() override;
	/**
	 * @brief 获取当前时间
	 * @return uint32_t 返回当前时间（格式：HHMM）
	 */
	virtual uint32_t	getCurTime() override;
	/**
	 * @brief 获取交易日
	 * @return uint32_t 返回交易日（格式：YYYYMMDD）
	 */
	virtual uint32_t	getTradingDate() override;
	/**
	 * @brief 将时间转换为分钟时间
	 * @param uTime 时间（格式：HHMMSS）
	 * @return uint32_t 返回分钟时间（格式：HHMM），引擎基类返回0，子类可重写
	 */
	virtual uint32_t	transTimeToMin(uint32_t uTime) override{ return 0; }  // 时间转换为分钟时间（空实现）

	//////////////////////////////////////////////////////////////////////////
	// IParserStub接口实现
	// 以下方法实现IParserStub接口，接收行情数据
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 处理推送的行情数据
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当行情解析器有新的Tick数据时被调用。
	 * 更新数据管理器缓存，并触发on_tick事件。
	 * 如果是主力合约，还会触发对应的主力合约代码的on_tick事件。
	 */
	virtual void handle_push_quote(WTSTickData* newTick) override;

public:
	/**
	 * @brief 初始化引擎
	 * @param cfg 配置参数指针
	 * @param bdMgr 基础数据管理器指针
	 * @param dataMgr 数据管理器指针
	 * @param hotMgr 热点合约管理器指针
	 * @param notifier 事件通知器指针
	 * 
	 * 初始化引擎，加载配置，初始化各种管理器。
	 */
	virtual void init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier);

	/**
	 * @brief 运行引擎
	 * 
	 * 纯虚函数，子类必须实现。
	 * 启动引擎的主循环，处理行情数据和策略逻辑。
	 */
	virtual void run() = 0;

	/**
	 * @brief Tick事件处理
	 * @param stdCode 标准合约代码字符串
	 * @param curTick 当前Tick数据指针
	 * 
	 * 当有新的Tick数据时被调用。
	 * 更新价格缓存，检查是否需要触发信号，更新持仓浮动盈亏。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* curTick);

	/**
	 * @brief K线事件处理
	 * @param stdCode 标准合约代码字符串
	 * @param period 周期字符串（如"m1"表示1分钟）
	 * @param times K线倍数
	 * @param newBar 新的K线数据指针
	 * 
	 * 纯虚函数，子类必须实现。
	 * 当有新的K线数据时被调用。
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) = 0;

	/**
	 * @brief 初始化事件处理
	 * 
	 * 虚函数，子类可重写。
	 * 当引擎初始化完成时被调用。
	 */
	virtual void on_init(){}  // 初始化事件处理（空实现，子类可重写）
	/**
	 * @brief 交易会话开始事件处理
	 * 
	 * 虚函数，子类可重写。
	 * 当交易会话开始时被调用。
	 */
	virtual void on_session_begin();
	/**
	 * @brief 交易会话结束事件处理
	 * 
	 * 虚函数，子类可重写。
	 * 当交易会话结束时被调用。
	 * 进行资金结算，记录资金日志。
	 */
	virtual void on_session_end();


protected:
	/**
	 * @brief 加载手续费模板
	 * @param filename 手续费模板文件路径
	 * 
	 * 从文件加载手续费模板配置。
	 * 手续费模板定义了不同品种的开仓、平仓、平今手续费。
	 */
	void		load_fees(const char* filename);

	/**
	 * @brief 加载数据
	 * 
	 * 从文件加载历史数据，包括资金数据、持仓数据、风控参数等。
	 */
	void		load_datas();

	/**
	 * @brief 保存数据
	 * 
	 * 将当前数据保存到文件，包括资金数据、持仓数据、风控参数等。
	 * 使用JSON格式保存。
	 */
	void		save_datas();

	/**
	 * @brief 追加交易信号
	 * @param stdCode 标准合约代码字符串
	 * @param qty 目标仓位数量
	 * @param bStandBy 是否等待下一个Tick触发，true表示等待，false表示立即执行，默认为true
	 * 
	 * 添加交易信号到信号队列。
	 * 如果bStandBy为true，则等待下一个Tick触发；否则立即执行。
	 * 这样设计是为了确保策略的理论成交价和组合的理论成交价一致。
	 */
	void		append_signal(const char* stdCode, double qty, bool bStandBy);

	/**
	 * @brief 设置持仓
	 * @param stdCode 标准合约代码字符串
	 * @param qty 目标仓位数量
	 * @param curPx 当前价格，如果小于0则自动获取，默认为-1
	 * 
	 * 根据目标仓位和当前仓位，计算差量，更新持仓。
	 * 如果持仓方向一致，增加持仓明细；如果持仓方向不一致，平仓并可能反手。
	 * 同时更新资金信息（手续费、盈亏等）。
	 */
	void		do_set_position(const char* stdCode, double qty, double curPx = -1);

	/**
	 * @brief 任务循环
	 * 
	 * 后台线程的主循环，处理任务队列中的任务。
	 * 包括持仓更新、资金更新等耗时操作。
	 */
	void		task_loop();

	/**
	 * @brief 推送任务
	 * @param task 任务项（函数对象）
	 * 
	 * 将任务添加到任务队列，由后台线程处理。
	 * 如果后台线程未启动，则启动后台线程。
	 */
	void		push_task(TaskItem task);

	/**
	 * @brief 更新资金浮动盈亏
	 * 
	 * 计算所有持仓的浮动盈亏，更新组合资金的浮动盈亏。
	 * 同时更新最大最小动态余额等统计信息。
	 */
	void		update_fund_dynprofit();

	/**
	 * @brief 初始化风控模块
	 * @param cfg 风控配置参数指针
	 * @return bool 初始化成功返回true，否则返回false
	 * 
	 * 根据配置加载风控模块（动态库），创建风控监视器实例。
	 */
	bool		init_riskmon(WTSVariant* cfg);

private:
	/**
	 * @brief 初始化输出文件
	 * 
	 * 创建或打开成交记录文件和平仓记录文件。
	 * 如果是新文件，写入CSV表头。
	 */
	void		init_outputs();
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
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, double fee = 0.0);
	/**
	 * @brief 记录平仓日志
	 * @param stdCode 标准合约代码字符串
	 * @param isLong 是否多仓，true表示多仓，false表示空仓
	 * @param openTime 开仓时间戳
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间戳
	 * @param closepx 平仓价格
	 * @param qty 平仓数量
	 * @param profit 本次平仓盈亏
	 * @param totalprofit 累计平仓盈亏，默认为0
	 * 
	 * 将平仓记录写入CSV文件。
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double totalprofit = 0);

protected:
	uint32_t		_cur_date;		// 当前日期（格式：YYYYMMDD）
	uint32_t		_cur_time;		// 当前时间（格式：HHMM），是1分钟线时间，比如0900，这个时候的1分钟线是0901，_cur_time也就是0901，这个是为了CTA里面方便
	uint32_t		_cur_raw_time;	// 当前真实时间（格式：HHMM）
	uint32_t		_cur_secs;		// 当前秒数（包含毫秒，格式：SSmmm）
	uint32_t		_cur_tdate;		// 当前交易日（格式：YYYYMMDD）

	uint32_t		_fund_udt_span;	// 组合资金更新时间间隔（秒），0表示不限制更新间隔

	IBaseDataMgr*	_base_data_mgr;	// 基础数据管理器指针，用于获取合约信息等
	IHotMgr*		_hot_mgr;		// 主力管理器指针，用于获取主力合约信息等
	IEngineEvtListener*	_evt_listener;  // 事件监听器指针，用于监听引擎事件
	WtDtMgr*		_data_mgr;		// 数据管理器指针，用于获取行情数据等

	//By Wesley @ 2022.02.07
	//tick数据订阅项，first是contextid，second是订阅选项，0-原始订阅，1-前复权，2-后复权
	typedef std::pair<uint32_t, uint32_t> SubOpt;  // 订阅选项类型定义：pair<策略ID, 订阅选项>，订阅选项：0=原始订阅，1=前复权，2=后复权
	typedef wt_hashmap<uint32_t, SubOpt> SubList;  // 订阅列表类型定义：键为策略ID，值为订阅选项
	typedef wt_hashmap<std::string, SubList>	StraSubMap;  // 策略订阅映射表类型定义：键为合约代码，值为订阅列表
	StraSubMap		_tick_sub_map;	// Tick数据订阅表，记录每个合约被哪些策略订阅了
	StraSubMap		_bar_sub_map;	// K线数据订阅表，记录每个合约被哪些策略订阅了

	//By Wesley @ 2022.02.07 
	//这个好像没有用到，不需要了
	//wt_hashset<std::string>		_ticksubed_raw_codes;	// Tick订阅表（真实代码模式）


	//////////////////////////////////////////////////////////////////////////
	// 信号管理
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @struct SigInfo
	 * @brief 信号信息结构体
	 * 
	 * 用于存储待触发的交易信号信息。
	 */
	typedef struct _SigInfo
	{
		double		_volume;  // 目标仓位数量
		uint64_t	_gentime;  // 信号生成时间戳

		/**
		 * @brief 构造函数
		 * 
		 * 初始化信号信息，仓位和时间为0。
		 */
		_SigInfo()
		{
			_volume = 0;  // 初始仓位为0
			_gentime = 0;  // 初始时间为0
		}
	}SigInfo;  // 信号信息结构体类型定义
	typedef wt_hashmap<std::string, SigInfo>	SignalMap;  // 信号映射表类型定义：键为合约代码，值为信号信息
	SignalMap		_sig_map;  // 信号映射表，存储待触发的交易信号

	//////////////////////////////////////////////////////////////////////////
	// 信号过滤器
	//////////////////////////////////////////////////////////////////////////
	WtFilterMgr		_filter_mgr;  // 信号过滤器管理器，用于过滤或调整交易信号
	EventNotifier*	_notifier;  // 事件通知器指针，用于发送事件通知

	//////////////////////////////////////////////////////////////////////////
	// 手续费模板
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @struct FeeItem
	 * @brief 手续费项结构体
	 * 
	 * 用于存储单个品种的手续费配置。
	 */
	typedef struct _FeeItem
	{
		double	_open;  // 开仓手续费（按手数或按金额）
		double	_close;  // 平仓手续费（按手数或按金额）
		double	_close_today;  // 平今手续费（按手数或按金额）
		bool	_by_volume;  // 是否按手数计费，true表示按手数，false表示按金额

		/**
		 * @brief 构造函数
		 * 
		 * 初始化手续费项，所有字段为0。
		 */
		_FeeItem()
		{
			memset(this, 0, sizeof(_FeeItem));  // 将所有字段初始化为0
		}
	} FeeItem;  // 手续费项结构体类型定义
	typedef wt_hashmap<std::string, FeeItem>	FeeMap;  // 手续费映射表类型定义：键为品种ID（格式：交易所.品种），值为手续费项
	FeeMap		_fee_map;  // 手续费映射表，存储各品种的手续费配置
	

	WTSPortFundInfo*	_port_fund;  // 组合资金信息指针，存储组合的资金、持仓、盈亏等信息

	//////////////////////////////////////////////////////////////////////////
	// 持仓数据
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @struct DetailInfo
	 * @brief 持仓明细信息结构体
	 * 
	 * 用于存储单笔开仓的详细信息。
	 */
	typedef struct _DetailInfo
	{
		bool		_long;  // 是否多仓，true表示多仓，false表示空仓
		double		_price;  // 开仓价格
		double		_volume;  // 持仓数量（大于0）
		uint64_t	_opentime;  // 开仓时间戳
		uint32_t	_opentdate;  // 开仓交易日（格式：YYYYMMDD）
		double		_profit;  // 浮动盈亏

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓明细，所有字段为0。
		 */
		_DetailInfo()
		{
			memset(this, 0, sizeof(_DetailInfo));  // 将所有字段初始化为0
		}
	} DetailInfo;  // 持仓明细信息结构体类型定义

	/**
	 * @struct PosInfo
	 * @brief 持仓信息结构体
	 * 
	 * 用于存储单个合约的持仓信息，包括总持仓、明细持仓、盈亏等。
	 */
	typedef struct _PosInfo
	{
		double		_volume;  // 总持仓数量（正数表示多仓，负数表示空仓）
		double		_closeprofit;  // 累计平仓盈亏
		double		_dynprofit;  // 浮动盈亏
		SpinMutex	_mtx;  // 自旋锁，用于保护持仓数据的线程安全

		std::vector<DetailInfo> _details;  // 持仓明细列表，存储每笔开仓的详细信息

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓信息，持仓和盈亏为0。
		 */
		_PosInfo()
		{
			_volume = 0;  // 初始持仓为0
			_closeprofit = 0;  // 初始平仓盈亏为0
			_dynprofit = 0;  // 初始浮动盈亏为0
		}
	} PosInfo;  // 持仓信息结构体类型定义
	typedef std::shared_ptr<PosInfo> PosInfoPtr;  // 持仓信息智能指针类型定义
	typedef wt_hashmap<std::string, PosInfoPtr> PositionMap;  // 持仓映射表类型定义：键为合约代码，值为持仓信息指针
	PositionMap		_pos_map;  // 持仓映射表，存储各合约的持仓信息

	//////////////////////////////////////////////////////////////////////////
	// 价格缓存
	//////////////////////////////////////////////////////////////////////////
	typedef wt_hashmap<std::string, double> PriceMap;  // 价格映射表类型定义：键为合约代码，值为价格
	PriceMap		_price_map;  // 价格映射表，缓存各合约的最新价格

	//后台任务线程, 把风控和资金, 持仓更新都放到这个线程里去
	typedef std::queue<TaskItem>	TaskQueue;  // 任务队列类型定义：存储任务项（函数对象）
	StdThreadPtr	_thrd_task;  // 后台任务线程指针
	TaskQueue		_task_queue;  // 任务队列，存储待处理的任务
	StdUniqueMutex	_mtx_task;  // 任务队列互斥锁，用于保护任务队列的线程安全
	StdCondVariable	_cond_task;  // 任务队列条件变量，用于线程间通信
	bool			_terminated;  // 终止标志，true表示线程已终止

	/**
	 * @struct RiskMonFactInfo
	 * @brief 风控监视器工厂信息结构体
	 * 
	 * 用于存储风控模块的动态库信息和工厂指针。
	 */
	typedef struct _RiskMonFactInfo
	{
		std::string		_module_path;  // 风控模块路径（动态库文件路径）
		DllHandle		_module_inst;  // 动态库句柄
		IRiskMonitorFact*	_fact;  // 风控监视器工厂指针
		FuncCreateRiskMonFact	_creator;  // 创建工厂函数指针
		FuncDeleteRiskMonFact	_remover;  // 删除工厂函数指针
	} RiskMonFactInfo;  // 风控监视器工厂信息结构体类型定义
	RiskMonFactInfo	_risk_fact;  // 风控监视器工厂信息
	WtRiskMonPtr	_risk_mon;  // 风控监视器智能指针
	double			_risk_volscale;  // 风控仓位缩放系数
	uint32_t		_risk_date;  // 风控参数生效日期（格式：YYYYMMDD）

	TraderAdapterMgr*	_adapter_mgr;  // 交易适配器管理器指针，用于访问交易接口

	BoostFilePtr	_trade_logs;  // 成交记录文件指针
	BoostFilePtr	_close_logs;  // 平仓记录文件指针

	wt_hashmap<std::string, double>	_factors_cache;  // 复权因子缓存映射表：键为合约代码，值为复权因子

	//用于标记是否可以推送tickle
	bool			_ready;  // 就绪标志，true表示引擎已就绪，可以推送Tick数据
};
NS_WTP_END  // 结束WonderTrader命名空间