/*!
 * \file PorterDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtBtPorter模块定义文件
 * 
 * 本文件定义了WtBtPorter回测模块的所有公共定义，包括：
 * 1. 回调函数类型定义：定义了各种策略事件、数据获取、HFT交易等回调函数的函数指针类型
 * 2. 事件常量定义：定义了回测引擎的各种事件类型（初始化、交易日开始/结束、回测结束等）
 * 3. 日志级别常量：定义了日志系统的日志级别
 * 4. 数据结构前向声明：声明了回测模块使用的各种数据结构
 * 
 * 设计逻辑：
 * - 本文件作为WtBtPorter模块的基础定义文件，为C接口和C++实现提供统一的类型定义
 * - 通过函数指针类型定义，实现外部语言（如Python）与C++核心引擎之间的回调机制
 * - 事件常量用于标识回测过程中的各种重要事件，便于外部语言监听和处理
 * - 所有回调函数都使用PORTER_FLAG宏，确保跨语言调用的兼容性
 */
#pragma once

#include <stdint.h>  // 标准整数类型定义
#include "../Includes/WTSTypes.h"  // WonderTrader类型定义

NS_WTP_BEGIN  // WonderTrader命名空间开始
struct WTSBarStruct;  // 前向声明：K线数据结构
struct WTSTickStruct;  // 前向声明：Tick数据结构
struct WTSOrdDtlStruct;  // 前向声明：订单明细数据结构
struct WTSOrdQueStruct;  // 前向声明：订单队列数据结构
struct WTSTransStruct;  // 前向声明：逐笔成交数据结构
NS_WTP_END  // WonderTrader命名空间结束

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 策略上下文句柄类型
 * 
 * 用于标识策略实例的唯一句柄，外部语言通过此句柄访问对应的策略上下文
 * 实际对应C++中的策略模拟器对象指针（转换为unsigned long）
 */
typedef unsigned long		CtxHandler;

/**
 * @brief 事件常量定义
 * 
 * 定义回测引擎的各种事件类型，用于事件回调函数中标识事件类型
 */
static const WtUInt32	EVENT_ENGINE_INIT		= 1;	// 框架初始化事件（回测引擎初始化完成时触发）
static const WtUInt32	EVENT_SESSION_BEGIN		= 2;	// 交易日开始事件（新的交易日开始时触发）
static const WtUInt32	EVENT_SESSION_END		= 3;	// 交易日结束事件（交易日结束时触发）
static const WtUInt32	EVENT_ENGINE_SCHDL		= 4;	// 框架调度事件（回测引擎按时间调度时触发）
static const WtUInt32	EVENT_BACKTEST_END		= 5;	// 回测结束事件（回测完成时触发）

/**
 * @brief 日志级别常量定义
 * 
 * 定义日志系统的日志级别，用于控制日志输出的详细程度
 */
static const WtUInt32	LOG_LEVEL_DEBUG			= 0;	// 调试级别（最详细，用于开发调试）
static const WtUInt32	LOG_LEVEL_INFO			= 1;	// 信息级别（一般信息，用于正常运行）
static const WtUInt32	LOG_LEVEL_WARN			= 2;	// 警告级别（警告信息，需要注意但不影响运行）
static const WtUInt32	LOG_LEVEL_ERROR			= 3;	// 错误级别（错误信息，可能影响功能）

/**
 * @brief 回调函数类型定义
 * 
 * 定义各种策略事件和数据获取的回调函数类型，用于外部语言注册回调函数
 */

/**
 * @brief 获取K线数据的回调函数类型
 * 
 * 当策略调用获取K线数据接口时，通过此回调函数返回K线数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param bar K线数据数组指针
 * @param count K线数据条数
 * @param isLast 是否为最后一批数据（true表示数据已全部返回）
 */
typedef void(PORTER_FLAG *FuncGetBarsCallback)(CtxHandler cHandle, const char* stdCode, const char* period, WTSBarStruct* bar, WtUInt32 count, bool isLast);

/**
 * @brief 获取Tick数据的回调函数类型
 * 
 * 当策略调用获取Tick数据接口时，通过此回调函数返回Tick数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param tick Tick数据数组指针
 * @param count Tick数据条数
 * @param isLast 是否为最后一批数据（true表示数据已全部返回）
 */
typedef void(PORTER_FLAG *FuncGetTicksCallback)(CtxHandler cHandle, const char* stdCode, WTSTickStruct* tick, WtUInt32 count, bool isLast);

/**
 * @brief 策略初始化回调函数类型
 * 
 * 当策略初始化完成时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 */
typedef void(PORTER_FLAG *FuncStraInitCallback)(CtxHandler cHandle);

/**
 * @brief 交易日事件回调函数类型
 * 
 * 当交易日开始或结束时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param curTDate 当前交易日（格式：YYYYMMDD）
 * @param isBegin true表示交易日开始，false表示交易日结束
 */
typedef void(PORTER_FLAG *FuncSessionEvtCallback)(CtxHandler cHandle, WtUInt32 curTDate, bool isBegin);

/**
 * @brief Tick更新回调函数类型
 * 
 * 当订阅的合约有新的Tick数据时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param tick 新的Tick数据指针
 */
typedef void(PORTER_FLAG *FuncStraTickCallback)(CtxHandler cHandle, const char* stdCode, WTSTickStruct* tick);

/**
 * @brief 策略计算回调函数类型
 * 
 * 当策略需要执行计算逻辑时调用此回调函数（通常在K线闭合或定时触发）
 * 
 * @param cHandle 策略上下文句柄
 * @param uDate 当前日期（格式：YYYYMMDD）
 * @param uTime 当前时间（格式：HHMMSS）
 */
typedef void(PORTER_FLAG *FuncStraCalcCallback)(CtxHandler cHandle, WtUInt32 uDate, WtUInt32 uTime);

/**
 * @brief K线闭合回调函数类型
 * 
 * 当订阅的K线周期完成并生成新的K线时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param newBar 新生成的K线数据指针
 */
typedef void(PORTER_FLAG *FuncStraBarCallback)(CtxHandler cHandle, const char* stdCode, const char* period, WTSBarStruct* newBar);

/**
 * @brief 获取持仓的回调函数类型
 * 
 * 当策略调用获取所有持仓接口时，通过此回调函数返回每个持仓信息
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码（空字符串表示枚举结束）
 * @param position 持仓数量（正数表示多头，负数表示空头）
 * @param isLast 是否为最后一个持仓（true表示枚举结束）
 */
typedef void(PORTER_FLAG *FuncGetPositionCallback)(CtxHandler cHandle, const char* stdCode, double position, bool isLast);

/**
 * @brief 条件单触发回调函数类型
 * 
 * 当策略设置的条件单被触发时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param target 目标持仓数量
 * @param price 触发价格
 * @param usertag 用户标签
 */
typedef void(PORTER_FLAG *FuncStraCondTriggerCallback)(CtxHandler cHandle, const char* stdCode, double target, double price, const char* usertag);

/**
 * @brief 订单队列更新回调函数类型（策略事件）
 * 
 * 当订阅的合约有新的订单队列数据时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordQue 新的订单队列数据指针
 */
typedef void(PORTER_FLAG *FuncStraOrdQueCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdQueStruct* ordQue);

/**
 * @brief 获取订单队列数据的回调函数类型
 * 
 * 当策略调用获取订单队列数据接口时，通过此回调函数返回订单队列数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordQue 订单队列数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据（true表示数据已全部返回）
 */
typedef void(PORTER_FLAG *FuncGetOrdQueCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdQueStruct* ordQue, WtUInt32 count, bool isLast);

/**
 * @brief 订单明细更新回调函数类型（策略事件）
 * 
 * 当订阅的合约有新的订单明细数据时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordDtl 新的订单明细数据指针
 */
typedef void(PORTER_FLAG *FuncStraOrdDtlCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdDtlStruct* ordDtl);

/**
 * @brief 获取订单明细数据的回调函数类型
 * 
 * 当策略调用获取订单明细数据接口时，通过此回调函数返回订单明细数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordDtl 订单明细数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据（true表示数据已全部返回）
 */
typedef void(PORTER_FLAG *FuncGetOrdDtlCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdDtlStruct* ordDtl, WtUInt32 count, bool isLast);

/**
 * @brief 逐笔成交更新回调函数类型（策略事件）
 * 
 * 当订阅的合约有新的逐笔成交数据时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param trans 新的逐笔成交数据指针
 */
typedef void(PORTER_FLAG *FuncStraTransCallback)(CtxHandler cHandle, const char* stdCode, WTSTransStruct* trans);

/**
 * @brief 获取逐笔成交数据的回调函数类型
 * 
 * 当策略调用获取逐笔成交数据接口时，通过此回调函数返回逐笔成交数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param trans 逐笔成交数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据（true表示数据已全部返回）
 */
typedef void(PORTER_FLAG *FuncGetTransCallback)(CtxHandler cHandle, const char* stdCode, WTSTransStruct* trans, WtUInt32 count, bool isLast);

//////////////////////////////////////////////////////////////////////////
//HFT回调函数
/**
 * @brief HFT交易通道事件回调函数类型
 * 
 * 当HFT策略的交易通道状态发生变化时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param trader 交易通道ID
 * @param evtid 事件ID（1-通道就绪，2-通道断开）
 */
typedef void(PORTER_FLAG *FuncHftChannelCallback)(CtxHandler cHandle, const char* trader, WtUInt32 evtid);	//交易通道事件回调

/**
 * @brief HFT订单状态变化回调函数类型
 * 
 * 当HFT策略的订单状态发生变化时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy true表示买入，false表示卖出
 * @param totalQty 订单总数量
 * @param leftQty 剩余未成交数量
 * @param price 订单价格
 * @param isCanceled 是否已撤单
 * @param userTag 用户标签
 */
typedef void(PORTER_FLAG *FuncHftOrdCallback)(CtxHandler cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag);

/**
 * @brief HFT成交回报回调函数类型
 * 
 * 当HFT策略的订单有成交回报时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param isBuy true表示买入，false表示卖出
 * @param vol 成交数量
 * @param price 成交价格
 * @param userTag 用户标签
 */
typedef void(PORTER_FLAG *FuncHftTrdCallback)(CtxHandler cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag);

/**
 * @brief HFT委托回报回调函数类型
 * 
 * 当HFT策略的委托单提交后收到回报时调用此回调函数
 * 
 * @param cHandle 策略上下文句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 返回消息
 * @param userTag 用户标签
 */
typedef void(PORTER_FLAG *FuncHftEntrustCallback)(CtxHandler cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag);

/**
 * @brief 引擎事件回调函数类型
 * 
 * 当回测引擎发生重要事件时调用此回调函数（初始化、交易日开始/结束、回测结束等）
 * 
 * @param evtId 事件ID（EVENT_ENGINE_INIT、EVENT_SESSION_BEGIN等）
 * @param curDate 当前日期（格式：YYYYMMDD，某些事件可能为0）
 * @param curTime 当前时间（格式：HHMMSS，某些事件可能为0）
 */
typedef void(PORTER_FLAG *FuncEventCallback)(WtUInt32 evtId, WtUInt32 curDate, WtUInt32 curTime);

//////////////////////////////////////////////////////////////////////////
//外部数据加载模块
/**
 * @brief 加载最终K线数据的回调函数类型
 * 
 * 外部数据加载器需要实现此函数，用于加载指定合约和周期的最终K线数据（已复权）
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadFnlBars)(const char* stdCode, const char* period);

/**
 * @brief 加载原始K线数据的回调函数类型
 * 
 * 外部数据加载器需要实现此函数，用于加载指定合约和周期的原始K线数据（未复权）
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadRawBars)(const char* stdCode, const char* period);

/**
 * @brief 加载复权因子的回调函数类型
 * 
 * 外部数据加载器需要实现此函数，用于加载指定合约的复权因子数据
 * 
 * @param stdCode 标准合约代码
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadAdjFactors)(const char* stdCode);

/**
 * @brief 加载原始Tick数据的回调函数类型
 * 
 * 外部数据加载器需要实现此函数，用于加载指定合约和日期的原始Tick数据
 * 
 * @param stdCode 标准合约代码
 * @param uDate 交易日（格式：YYYYMMDD）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadRawTicks)(const char* stdCode, uint32_t uDate);
