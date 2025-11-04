/*!
 * \file UftStraContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT策略上下文类头文件
 *
 * 本文件定义了UftStraContext类，用于管理UFT（Ultra Fast Trading）策略的交易上下文。
 *
 * 设计逻辑：
 * 1. 双重继承：同时继承IUftStraCtx接口和ITrdNotifySink接口，既作为策略上下文提供交易接口，又作为交易通知接收者
 * 2. 本地持仓管理：使用内存映射文件存储持仓、订单、成交、回合等数据，支持程序重启后恢复持仓状态
 * 3. 净持仓模式：采用净持仓模式管理持仓，支持多策略在同一合约上开相反头寸的情况
 * 4. 参数管理：通过ShareManager实现策略参数的共享和持久化存储
 * 5. 数据订阅：管理策略对市场数据的订阅，包括tick、订单队列、订单明细、成交明细等
 * 6. 事件转发：将交易事件、市场数据事件转发给策略对象处理
 *
 * 主要功能：
 * - 提供策略交易接口：买入、卖出、开多、开空、平多、平空、撤单等
 * - 管理本地持仓：维护策略的本地持仓明细，计算持仓盈亏
 * - 数据订阅管理：管理策略对市场数据的订阅关系
 * - 事件回调处理：处理交易回报、市场数据回调等事件
 * - 参数管理：提供策略参数的读取、监控、同步等功能
 * - 日志记录：提供策略日志记录功能
 */
#pragma once
#include "ITrdNotifySink.h"  // 交易通知接口定义
#include "UftDataDefs.h"  // UFT数据定义头文件
#include "../Includes/IUftStraCtx.h"  // UFT策略上下文接口定义
#include "../Includes/FasterDefs.h"  // Faster库定义
#include "../Share/fmtlib.h"  // 格式化库

#include "../Share/BoostMappingFile.hpp"  // Boost内存映射文件类
typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;  // Boost内存映射文件智能指针类型别名

class UftStrategy;  // 前向声明：UFT策略类

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WtUftEngine;  // 前向声明：UFT引擎类
class TraderAdapter;  // 前向声明：交易适配器类

/**
 * @class UftStraContext
 * @brief UFT策略上下文类
 * 
 * 管理UFT策略的交易上下文，包括持仓管理、订单管理、数据订阅、事件处理等功能。
 * 该类同时实现IUftStraCtx接口（提供策略交易接口）和ITrdNotifySink接口（接收交易通知）。
 */
class UftStraContext : public IUftStraCtx, public ITrdNotifySink
{
public:
	/**
	 * @brief 构造函数
	 * @param engine UFT引擎指针
	 * @param name 策略名称
	 * 
	 * 创建策略上下文对象，初始化上下文ID和引擎指针。
	 */
	UftStraContext(WtUftEngine* engine, const char* name);
	
	/**
	 * @brief 析构函数
	 * 
	 * 销毁策略上下文对象，清理资源。
	 */
	virtual ~UftStraContext();

	/**
	 * @brief 设置策略对象
	 * @param stra 策略对象指针
	 * 
	 * 将策略对象绑定到上下文。
	 */
	void set_strategy(UftStrategy* stra){ _strategy = stra; }
	
	/**
	 * @brief 获取策略对象
	 * @return 策略对象指针
	 * 
	 * 返回当前绑定的策略对象。
	 */
	UftStrategy* get_stragety() { return _strategy; }

	/**
	 * @brief 设置交易适配器
	 * @param trader 交易适配器指针
	 * 
	 * 将交易适配器绑定到上下文，用于执行交易操作。
	 */
	void setTrader(TraderAdapter* trader);

public:
	/**
	 * @brief 获取上下文ID
	 * @return 上下文ID
	 * 
	 * 返回策略上下文的唯一标识ID。
	 */
	virtual uint32_t id() { return _context_id; }

	/**
	 * @brief 初始化回调
	 * 
	 * 策略初始化时调用，通知策略进行初始化操作。
	 */
	virtual void on_init() override;

	/**
	 * @brief Tick数据回调
	 * @param code 合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 当收到新的Tick数据时调用，更新持仓盈亏并通知策略。
	 */
	virtual void on_tick(const char* code, WTSTickData* newTick) override;

	/**
	 * @brief 订单队列数据回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdQue 新的订单队列数据
	 * 
	 * 当收到新的订单队列数据时调用，转发给策略处理。
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) override;

	/**
	 * @brief 订单明细数据回调
	 * @param stdCode 标准化合约代码
	 * @param newOrdDtl 新的订单明细数据
	 * 
	 * 当收到新的订单明细数据时调用，转发给策略处理。
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;

	/**
	 * @brief 成交明细数据回调
	 * @param stdCode 标准化合约代码
	 * @param newTrans 新的成交明细数据
	 * 
	 * 当收到新的成交明细数据时调用，转发给策略处理。
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) override;

	/**
	 * @brief K线数据回调
	 * @param code 合约代码
	 * @param period 周期字符串
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 * 
	 * 当收到新的K线数据时调用，转发给策略处理。
	 */
	virtual void on_bar(const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 成交回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多（true-做多，false-做空）
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param vol 成交数量
	 * @param price 成交价格
	 * 
	 * 当订单成交时调用，更新本地持仓并通知策略。
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double vol, double price) override;

	/**
	 * @brief 订单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多（true-做多，false-做空）
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @param totalQty 总委托数量
	 * @param leftQty 剩余数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销，默认为false
	 * 
	 * 当订单状态发生变化时调用，更新订单状态并通知策略。
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled = false) override;

	/**
	 * @brief 交易通道就绪回调
	 * @param tradingday 交易日
	 * 
	 * 当交易通道连接成功并准备就绪时调用，加载本地数据并通知策略。
	 */
	virtual void on_channel_ready(uint32_t tradingday) override;

	/**
	 * @brief 交易通道丢失回调
	 * 
	 * 当交易通道断开连接时调用，通知策略交易通道已不可用。
	 */
	virtual void on_channel_lost() override;

	/**
	 * @brief 下单回报回调
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息内容
	 * 
	 * 当下单操作完成时调用，通知策略下单结果。
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;

	/**
	 * @brief 持仓更新回调
	 * @param stdCode 标准化合约代码
	 * @param isLong 是否做多（true-做多，false-做空）
	 * @param prevol 昨仓数量
	 * @param preavail 昨仓可用数量
	 * @param newvol 今仓数量
	 * @param newavail 今仓可用数量
	 * @param tradingday 交易日
	 * 
	 * 当账户持仓发生变化时调用。注意：账户的持仓通知不转发给策略。
	 */
	virtual void on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday) override;

	/**
	 * @brief 交易会话开始回调
	 * @param uTDate 交易日
	 * 
	 * 当交易会话开始时调用，通知策略新交易日开始。
	 */
	virtual void on_session_begin(uint32_t uTDate) override;
	
	/**
	 * @brief 交易会话结束回调
	 * @param uTDate 交易日
	 * 
	 * 当交易会话结束时调用，通知策略交易日结束。
	 */
	virtual void on_session_end(uint32_t uTDate) override;

	/**
	 * @brief 参数更新回调
	 * 
	 * 当策略参数更新时调用，通知策略参数已更新。
	 */
	virtual void on_params_updated() override;


public:
	// 以下是被注释掉的旧版本参数监控接口
	//virtual void watch_param(const char* name, const char* val) override;
	//virtual void watch_param(const char* name, double val) override;
	//virtual void watch_param(const char* name, uint32_t val) override;
	//virtual void watch_param(const char* name, uint64_t val) override;
	//virtual void watch_param(const char* name, int32_t val) override;
	//virtual void watch_param(const char* name, int64_t val) override;

	/**
	 * @brief 监控字符串参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为空字符串
	 * @return 参数值的指针
	 * 
	 * 监控一个字符串类型的策略参数，参数值会被持久化存储。
	 */
	virtual const char*	watch_param(const char* name, const char* initVal = "") override;
	
	/**
	 * @brief 监控浮点数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @return 参数值
	 * 
	 * 监控一个浮点数类型的策略参数，参数值会被持久化存储。
	 */
	virtual double		watch_param(const char* name, double initVal = 0) override;
	
	/**
	 * @brief 监控无符号32位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @return 参数值
	 * 
	 * 监控一个无符号32位整数类型的策略参数，参数值会被持久化存储。
	 */
	virtual uint32_t	watch_param(const char* name, uint32_t initVal = 0) override;
	
	/**
	 * @brief 监控无符号64位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @return 参数值
	 * 
	 * 监控一个无符号64位整数类型的策略参数，参数值会被持久化存储。
	 */
	virtual uint64_t	watch_param(const char* name, uint64_t initVal = 0) override;
	
	/**
	 * @brief 监控有符号32位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @return 参数值
	 * 
	 * 监控一个有符号32位整数类型的策略参数，参数值会被持久化存储。
	 */
	virtual int32_t		watch_param(const char* name, int32_t initVal = 0) override;
	
	/**
	 * @brief 监控有符号64位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @return 参数值
	 * 
	 * 监控一个有符号64位整数类型的策略参数，参数值会被持久化存储。
	 */
	virtual int64_t		watch_param(const char* name, int64_t initVal = 0) override;

	/**
	 * @brief 提交参数监控
	 * 
	 * 提交所有通过watch_param监控的参数，使其生效。
	 */
	virtual void commit_param_watcher() override;

	/**
	 * @brief 读取字符串参数
	 * @param name 参数名称
	 * @param defVal 默认值，默认为空字符串
	 * @return 参数值
	 * 
	 * 读取一个字符串类型的策略参数，如果参数不存在则返回默认值。
	 */
	virtual const char*	read_param(const char* name, const char* defVal = "") override;
	
	/**
	 * @brief 读取浮点数参数
	 * @param name 参数名称
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 * 
	 * 读取一个浮点数类型的策略参数，如果参数不存在则返回默认值。
	 */
	virtual double		read_param(const char* name, double defVal = 0) override;
	
	/**
	 * @brief 读取无符号32位整数参数
	 * @param name 参数名称
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 * 
	 * 读取一个无符号32位整数类型的策略参数，如果参数不存在则返回默认值。
	 */
	virtual uint32_t	read_param(const char* name, uint32_t defVal = 0) override;
	
	/**
	 * @brief 读取无符号64位整数参数
	 * @param name 参数名称
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 * 
	 * 读取一个无符号64位整数类型的策略参数，如果参数不存在则返回默认值。
	 */
	virtual uint64_t	read_param(const char* name, uint64_t defVal = 0) override;
	
	/**
	 * @brief 读取有符号32位整数参数
	 * @param name 参数名称
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 * 
	 * 读取一个有符号32位整数类型的策略参数，如果参数不存在则返回默认值。
	 */
	virtual int32_t		read_param(const char* name, int32_t defVal = 0) override;
	
	/**
	 * @brief 读取有符号64位整数参数
	 * @param name 参数名称
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 * 
	 * 读取一个有符号64位整数类型的策略参数，如果参数不存在则返回默认值。
	 */
	virtual int64_t		read_param(const char* name, int64_t defVal = 0) override;

	/**
	 * @brief 同步字符串参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为空字符串
	 * @param bForceWrite 是否强制写入，默认为false
	 * @return 参数值的指针
	 * 
	 * 同步一个字符串类型的策略参数，返回参数值的指针，可以直接修改参数值。
	 */
	virtual const char*	sync_param(const char* name, const char* initVal = "", bool bForceWrite = false) override;
	
	/**
	 * @brief 同步浮点数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @return 参数值的指针
	 * 
	 * 同步一个浮点数类型的策略参数，返回参数值的指针，可以直接修改参数值。
	 */
	virtual double*		sync_param(const char* name, double initVal = 0, bool bForceWrite = false) override;
	
	/**
	 * @brief 同步无符号32位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @return 参数值的指针
	 * 
	 * 同步一个无符号32位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
	 */
	virtual uint32_t*	sync_param(const char* name, uint32_t initVal = 0, bool bForceWrite = false) override;
	
	/**
	 * @brief 同步无符号64位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @return 参数值的指针
	 * 
	 * 同步一个无符号64位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
	 */
	virtual uint64_t*	sync_param(const char* name, uint64_t initVal = 0, bool bForceWrite = false) override;
	
	/**
	 * @brief 同步有符号32位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @return 参数值的指针
	 * 
	 * 同步一个有符号32位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
	 */
	virtual int32_t*	sync_param(const char* name, int32_t initVal = 0, bool bForceWrite = false) override;
	
	/**
	 * @brief 同步有符号64位整数参数
	 * @param name 参数名称
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @return 参数值的指针
	 * 
	 * 同步一个有符号64位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
	 */
	virtual int64_t*	sync_param(const char* name, int64_t initVal = 0, bool bForceWrite = false) override;

public:
	//////////////////////////////////////////////////////////////////////////
	// IUftStraCtx 接口实现
	/**
	 * @brief 获取当前日期
	 * @return 当前日期（YYYYMMDD格式）
	 * 
	 * 返回当前系统日期。
	 */
	virtual uint32_t stra_get_date() override;
	
	/**
	 * @brief 获取当前时间
	 * @return 当前时间（HHMMSS格式）
	 * 
	 * 返回当前系统时间。
	 */
	virtual uint32_t stra_get_time() override;
	
	/**
	 * @brief 获取当前秒数
	 * @return 当前秒数（包含毫秒）
	 * 
	 * 返回当前时间的秒数（包含毫秒）。
	 */
	virtual uint32_t stra_get_secs() override;

	/**
	 * @brief 撤销订单
	 * @param localid 本地订单ID
	 * @return 是否成功
	 * 
	 * 撤销指定ID的订单。
	 */
	virtual bool stra_cancel(uint32_t localid) override;

	/**
	 * @brief 撤销所有订单
	 * @param stdCode 标准化合约代码
	 * @return 订单ID列表
	 * 
	 * 撤销指定合约的所有订单，返回被撤销的订单ID列表。
	 */
	virtual OrderIDs stra_cancel_all(const char* stdCode) override;

	/**
	 * @brief 买入接口
	 * @param stdCode 标准化合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 订单ID列表
	 * 
	 * 买入指定合约，返回生成的订单ID列表。
	 */
	virtual OrderIDs	stra_buy(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 卖出接口
	 * @param stdCode 标准化合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 订单ID列表
	 * 
	 * 卖出指定合约，返回生成的订单ID列表。
	 */
	virtual OrderIDs	stra_sell(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 开多接口
	 * @param stdCode 标准化合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 订单ID
	 * 
	 * 开多仓，返回生成的订单ID。
	 */
	virtual uint32_t	stra_enter_long(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 开空接口
	 * @param stdCode 标准化合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 订单ID
	 * 
	 * 开空仓，返回生成的订单ID。
	 */
	virtual uint32_t	stra_enter_short(const char* stdCode, double price, double qty, int flag = 0) override;

	/**
	 * @brief 平多接口
	 * @param stdCode 标准化合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param isToday 是否今仓，默认false
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 订单ID
	 * 
	 * 平多仓，返回生成的订单ID。
	 */
	virtual uint32_t	stra_exit_long(const char* stdCode, double price, double qty, bool isToday = false, int flag = 0) override;

	/**
	 * @brief 平空接口
	 * @param stdCode 标准化合约代码
	 * @param price 下单价格，0表示市价单
	 * @param qty 下单数量
	 * @param isToday 是否今仓，默认false
	 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
	 * @return 订单ID
	 * 
	 * 平空仓，返回生成的订单ID。
	 */
	virtual uint32_t	stra_exit_short(const char* stdCode, double price, double qty, bool isToday = false, int flag = 0) override;

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准化合约代码
	 * @return 商品信息指针
	 * 
	 * 获取指定合约的商品信息。
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;

	/**
	 * @brief 获取K线数据
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m1", "m5", "d1"等）
	 * @param count 数据条数
	 * @return K线数据切片指针
	 * 
	 * 获取指定合约的K线数据，如果成功获取则自动订阅该合约的tick数据。
	 */
	virtual WTSKlineSlice* stra_get_bars(const char* stdCode, const char* period, uint32_t count) override;

	/**
	 * @brief 获取Tick数据
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return Tick数据切片指针
	 * 
	 * 获取指定合约的Tick数据，如果成功获取则自动订阅该合约的tick数据。
	 */
	virtual WTSTickSlice* stra_get_ticks(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取订单明细数据
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return 订单明细数据切片指针
	 * 
	 * 获取指定合约的订单明细数据，如果成功获取则自动订阅该合约的订单明细数据。
	 */
	virtual WTSOrdDtlSlice*	stra_get_order_detail(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取订单队列数据
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return 订单队列数据切片指针
	 * 
	 * 获取指定合约的订单队列数据，如果成功获取则自动订阅该合约的订单队列数据。
	 */
	virtual WTSOrdQueSlice*	stra_get_order_queue(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取成交明细数据
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @return 成交明细数据切片指针
	 * 
	 * 获取指定合约的成交明细数据，如果成功获取则自动订阅该合约的成交明细数据。
	 */
	virtual WTSTransSlice*	stra_get_transaction(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准化合约代码
	 * @return 最新Tick数据指针
	 * 
	 * 获取指定合约的最新Tick数据。
	 */
	virtual WTSTickData* stra_get_last_tick(const char* stdCode) override;

	/**
	 * @brief 记录信息日志
	 * @param message 日志消息
	 * 
	 * 记录信息级别的日志。
	 */
	virtual void stra_log_info(const char* message) override;
	
	/**
	 * @brief 记录调试日志
	 * @param message 日志消息
	 * 
	 * 记录调试级别的日志。
	 */
	virtual void stra_log_debug(const char* message) override;
	
	/**
	 * @brief 记录错误日志
	 * @param message 日志消息
	 * 
	 * 记录错误级别的日志。
	 */
	virtual void stra_log_error(const char* message) override;

	/**
	 * @brief 获取账户持仓
	 * @param stdCode 标准化合约代码
	 * @param bOnlyValid 是否只返回有效持仓，默认false
	 * @param iFlag 持仓标志，默认3（表示多空都返回）
	 * @return 持仓数量
	 * 
	 * 获取账户在指定合约上的持仓数量。
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, int32_t iFlag = 3) override;
	
	/**
	 * @brief 获取本地持仓
	 * @param stdCode 标准化合约代码
	 * @return 本地持仓数量
	 * 
	 * 获取策略在指定合约上的本地持仓数量（净持仓）。
	 */
	virtual double stra_get_local_position(const char* stdCode) override;
	
	/**
	 * @brief 获取本地持仓盈亏
	 * @param stdCode 标准化合约代码
	 * @return 持仓盈亏
	 * 
	 * 获取策略在指定合约上的本地持仓盈亏。
	 */
	virtual double stra_get_local_posprofit(const char* stdCode) override;
	
	/**
	 * @brief 获取本地平仓盈亏
	 * @param stdCode 标准化合约代码
	 * @return 平仓盈亏
	 * 
	 * 获取策略在指定合约上的本地平仓盈亏。
	 */
	virtual double stra_get_local_closeprofit(const char* stdCode) override;
	
	/**
	 * @brief 枚举持仓
	 * @param stdCode 标准化合约代码
	 * @return 持仓数量
	 * 
	 * 枚举账户在指定合约上的持仓数量。
	 */
	virtual double stra_enum_position(const char* stdCode) override;
	
	/**
	 * @brief 获取当前价格
	 * @param stdCode 标准化合约代码
	 * @return 当前价格
	 * 
	 * 获取指定合约的当前价格。
	 */
	virtual double stra_get_price(const char* stdCode) override;
	
	/**
	 * @brief 获取未完成数量
	 * @param stdCode 标准化合约代码
	 * @return 未完成数量
	 * 
	 * 获取指定合约的未完成订单数量。
	 */
	virtual double stra_get_undone(const char* stdCode) override;
	
	/**
	 * @brief 获取信息数量
	 * @param stdCode 标准化合约代码
	 * @return 信息数量
	 * 
	 * 获取指定合约的信息数量（用途待确认）。
	 */
	virtual uint32_t stra_get_infos(const char* stdCode) override;

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的Tick数据，订阅后会自动收到该合约的Tick数据回调。
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;
	
	/**
	 * @brief 订阅订单明细数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的订单明细数据，订阅后会自动收到该合约的订单明细数据回调。
	 */
	virtual void stra_sub_order_details(const char* stdCode) override;
	
	/**
	 * @brief 订阅订单队列数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的订单队列数据，订阅后会自动收到该合约的订单队列数据回调。
	 */
	virtual void stra_sub_order_queues(const char* stdCode) override;
	
	/**
	 * @brief 订阅成交明细数据
	 * @param stdCode 标准化合约代码
	 * 
	 * 订阅指定合约的成交明细数据，订阅后会自动收到该合约的成交明细数据回调。
	 */
	virtual void stra_sub_transactions(const char* stdCode) override;

private:
	/**
	 * @brief 记录调试日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 格式化参数
	 * 
	 * 使用格式化字符串记录调试日志。
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_debug(buffer);  // 调用调试日志接口
	}

	/**
	 * @brief 记录信息日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 格式化参数
	 * 
	 * 使用格式化字符串记录信息日志。
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_info(buffer);  // 调用信息日志接口
	}

	/**
	 * @brief 记录错误日志（模板函数）
	 * @tparam Args 可变参数类型
	 * @param format 格式化字符串
	 * @param args 格式化参数
	 * 
	 * 使用格式化字符串记录错误日志。
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);  // 格式化字符串
		stra_log_error(buffer);  // 调用错误日志接口
	}

private:
	/**
	 * @struct PosBlkPair
	 * @brief 持仓数据块配对结构体
	 * 
	 * 用于管理持仓数据的内存映射文件和互斥锁。
	 */
	typedef struct _PosBlkPair
	{
		uft::PositionBlock*	_block;  // 持仓数据块指针
		BoostMFPtr			_file;  // 内存映射文件智能指针
		SpinMutex			_mutex;  // 自旋互斥锁，用于线程安全

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓数据块配对结构体，将所有成员变量置为NULL。
		 */
		_PosBlkPair()
		{
			_block = NULL;  // 初始化为空指针
			_file = NULL;  // 初始化为空指针
		}

	} PosBlkPair;  // 持仓数据块配对类型别名

	/**
	 * @struct OrdBlkPair
	 * @brief 订单数据块配对结构体
	 * 
	 * 用于管理订单数据的内存映射文件和互斥锁。
	 */
	typedef struct _OrdBlkPair
	{
		uft::OrderBlock*	_block;  // 订单数据块指针
		BoostMFPtr			_file;  // 内存映射文件智能指针
		SpinMutex			_mutex;  // 自旋互斥锁，用于线程安全

		/**
		 * @brief 构造函数
		 * 
		 * 初始化订单数据块配对结构体，将所有成员变量置为NULL。
		 */
		_OrdBlkPair()
		{
			_block = NULL;  // 初始化为空指针
			_file = NULL;  // 初始化为空指针
		}

	} OrdBlkPair;  // 订单数据块配对类型别名

	/**
	 * @struct TrdBlkPair
	 * @brief 成交数据块配对结构体
	 * 
	 * 用于管理成交数据的内存映射文件和互斥锁。
	 */
	typedef struct _TrdBlkPair
	{
		uft::TradeBlock*	_block;  // 成交数据块指针
		BoostMFPtr			_file;  // 内存映射文件智能指针
		SpinMutex			_mutex;  // 自旋互斥锁，用于线程安全

		/**
		 * @brief 构造函数
		 * 
		 * 初始化成交数据块配对结构体，将所有成员变量置为NULL。
		 */
		_TrdBlkPair()
		{
			_block = NULL;  // 初始化为空指针
			_file = NULL;  // 初始化为空指针
		}

	} TrdBlkPair;  // 成交数据块配对类型别名

	/**
	 * @struct RndBlkPair
	 * @brief 回合数据块配对结构体
	 * 
	 * 用于管理回合数据的内存映射文件和互斥锁。
	 */
	typedef struct _RndBlkPair
	{
		uft::RoundBlock*	_block;  // 回合数据块指针
		BoostMFPtr			_file;  // 内存映射文件智能指针
		SpinMutex			_mutex;  // 自旋互斥锁，用于线程安全

		/**
		 * @brief 构造函数
		 * 
		 * 初始化回合数据块配对结构体，将所有成员变量置为NULL。
		 */
		_RndBlkPair()
		{
			_block = NULL;  // 初始化为空指针
			_file = NULL;  // 初始化为空指针
		}

	} RndBlkPair;  // 回合数据块配对类型别名

	/**
	 * @brief 加载本地数据
	 * 
	 * 从内存映射文件中加载本地持仓、订单、成交、回合等数据。
	 */
	void	load_local_data();

	PosBlkPair		_pos_blk;  // 持仓数据块配对对象
	OrdBlkPair		_ord_blk;  // 订单数据块配对对象
	TrdBlkPair		_trd_blk;  // 成交数据块配对对象
	RndBlkPair		_rnd_blk;  // 回合数据块配对对象

	/**
	 * @struct PosInfo
	 * @brief 持仓信息结构体
	 * 
	 * 用于管理单个合约的持仓信息，包括持仓数量、开仓成本、盈亏等。
	 */
	typedef struct _Position
	{
		// 多仓数据（净持仓，正数表示多仓，负数表示空仓）
		double	_volume;  // 持仓数量（净持仓）
		double	_opencost;  // 开仓成本
		double	_dynprofit;  // 动态盈亏（持仓盈亏）

		double	_total_profit;  // 总盈亏（持仓盈亏+平仓盈亏）

		uint32_t _valid_idx;  // 有效索引，用于跳过已平仓的持仓明细

		std::vector<uft::DetailStruct*> _details;  // 持仓明细列表

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓信息结构体，将所有成员变量清零。
		 */
		_Position():_volume(0),_valid_idx(0), _total_profit(0),
			_opencost(0),_dynprofit(0)
		{
		}
	} PosInfo;  // 持仓信息类型别名

	wt_hashmap<std::string, PosInfo> _positions;  // 持仓信息映射表，key为合约代码

	wt_hashmap<uint32_t, uft::OrderStruct*> _order_ids;  // 订单ID映射表，key为本地订单ID

	/**
	 * @brief 判断是否为我的订单
	 * @param localid 本地订单ID
	 * @return 是否为我的订单
	 * 
	 * 判断指定订单ID是否属于当前策略上下文。
	 */
	inline bool is_my_order(uint32_t localid) const
	{
		auto it = _order_ids.find(localid);  // 查找订单ID
		return it != _order_ids.end();  // 返回是否找到
	}

private:
	uint32_t		_context_id;  // 上下文ID，策略上下文的唯一标识
	WtUftEngine*	_engine;  // UFT引擎指针
	TraderAdapter*	_trader;  // 交易适配器指针
	uint32_t		_tradingday;  // 当前交易日

	UftStrategy*	_strategy;  // 策略对象指针
};

NS_WTP_END