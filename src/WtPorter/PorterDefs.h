/*!
 * \file PorterDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Porter模块定义文件
 * 
 * 本文件定义了WtPorter模块中使用的所有回调函数类型、事件常量、上下文句柄类型等基础定义。
 * WtPorter模块是WonderTrader框架与外部语言（如Python、C#等）交互的桥梁，通过C接口
 * 和回调函数机制实现跨语言调用。本文件提供了：
 * 1. 策略上下文句柄类型定义
 * 2. 引擎事件常量定义（初始化、交易日开始/结束、调度等）
 * 3. 通道事件常量定义（就绪、断开等）
 * 4. 日志级别常量定义
 * 5. 各种策略回调函数类型定义（CTA、HFT、SEL策略的回调）
 * 6. 扩展Parser和Executer的回调函数类型定义
 * 7. 外部数据加载器的回调函数类型定义
 */
#pragma once

#include <stdint.h>  // 标准整数类型定义
#include "../Includes/WTSTypes.h"  // WonderTrader基础类型定义

NS_WTP_BEGIN
// 前向声明各种数据结构，避免包含完整的头文件
struct WTSBarStruct;      // K线数据结构
struct WTSTickStruct;     // 行情tick数据结构
struct WTSOrdDtlStruct;    // 订单明细数据结构
struct WTSOrdQueStruct;   // 订单队列数据结构
struct WTSTransStruct;     // 逐笔成交数据结构
NS_WTP_END

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 策略上下文句柄类型
 * 
 * 用于标识和操作策略上下文的唯一标识符，通过该句柄可以访问对应的策略上下文对象
 */
typedef unsigned long		CtxHandler;

/**
 * @brief 引擎初始化事件常量
 * 
 * 当交易引擎完成初始化时触发此事件
 */
static const WtUInt32	EVENT_ENGINE_INIT	= 1;	//框架初始化

/**
 * @brief 交易日开始事件常量
 * 
 * 当新的交易日开始时触发此事件
 */
static const WtUInt32	EVENT_SESSION_BEGIN = 2;	//交易日开始

/**
 * @brief 交易日结束事件常量
 * 
 * 当交易日结束时触发此事件
 */
static const WtUInt32	EVENT_SESSION_END	= 3;	//交易日结束

/**
 * @brief 引擎调度事件常量
 * 
 * 当引擎执行定时调度时触发此事件
 */
static const WtUInt32	EVENT_ENGINE_SCHDL	= 4;	//框架调度

/**
 * @brief 交易通道就绪事件常量
 * 
 * 当交易通道连接成功并准备就绪时触发此事件
 */
static const WtUInt32	CHNL_EVENT_READY	= 1000;	//通道就绪事件

/**
 * @brief 交易通道断开事件常量
 * 
 * 当交易通道断开连接时触发此事件
 */
static const WtUInt32	CHNL_EVENT_LOST		= 1001;	//通道断开事件

/**
 * @brief 日志级别常量定义
 * 
 * 定义了日志输出的不同级别，从调试信息到错误信息
 */
static const WtUInt32	LOG_LEVEL_DEBUG		= 0;  // 调试级别，最详细的日志信息
static const WtUInt32	LOG_LEVEL_INFO		= 1;  // 信息级别，一般性信息
static const WtUInt32	LOG_LEVEL_WARN		= 2;  // 警告级别，需要注意但不影响运行的信息
static const WtUInt32	LOG_LEVEL_ERROR		= 3;  // 错误级别，错误信息


/*
*	回调函数定义
*	
*	以下定义了各种策略和功能模块的回调函数类型，用于外部语言（如Python）与C++核心引擎之间的交互
*/

/**
 * @brief 获取K线数据的回调函数类型
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param bar K线数据数组指针
 * @param count K线数据条数
 * @param isLast 是否为最后一批数据
 */
typedef void(PORTER_FLAG *FuncGetBarsCallback)(CtxHandler cHandle, const char* stdCode, const char* period, WTSBarStruct* bar, WtUInt32 count, bool isLast);

/**
 * @brief 获取Tick数据的回调函数类型
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param tick Tick数据数组指针
 * @param count Tick数据条数
 * @param isLast 是否为最后一批数据
 */
typedef void(PORTER_FLAG *FuncGetTicksCallback)(CtxHandler cHandle, const char* stdCode, WTSTickStruct* tick, WtUInt32 count, bool isLast);

/**
 * @brief 策略初始化回调函数类型
 * 
 * 当策略上下文初始化完成时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 */
typedef void(PORTER_FLAG *FuncStraInitCallback)(CtxHandler cHandle);

/**
 * @brief 交易日事件回调函数类型
 * 
 * 当交易日开始或结束时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 * @param curTDate 当前交易日（格式：YYYYMMDD）
 * @param isBegin true表示交易日开始，false表示交易日结束
 */
typedef void(PORTER_FLAG *FuncSessionEvtCallback)(CtxHandler cHandle, WtUInt32 curTDate, bool isBegin);

/**
 * @brief 策略Tick更新回调函数类型
 * 
 * 当订阅的合约有新的Tick数据时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param tick 新的Tick数据结构指针
 */
typedef void(PORTER_FLAG *FuncStraTickCallback)(CtxHandler cHandle, const char* stdCode, WTSTickStruct* tick);

/**
 * @brief 策略计算回调函数类型
 * 
 * 当引擎执行定时计算时调用此回调，策略可以在此回调中进行逻辑计算
 * 
 * @param cHandle 策略上下文句柄
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 */
typedef void(PORTER_FLAG *FuncStraCalcCallback)(CtxHandler cHandle, WtUInt32 curDate, WtUInt32 curTime);

/**
 * @brief 策略K线闭合回调函数类型
 * 
 * 当订阅的K线周期完成并生成新的K线时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @param newBar 新生成的K线数据结构指针
 */
typedef void(PORTER_FLAG *FuncStraBarCallback)(CtxHandler cHandle, const char* stdCode, const char* period, WTSBarStruct* newBar);

/**
 * @brief 获取持仓信息的回调函数类型
 * 
 * 用于枚举策略的所有持仓信息
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param position 持仓数量
 * @param isLast 是否为最后一个持仓
 */
typedef void(PORTER_FLAG *FuncGetPositionCallback)(CtxHandler cHandle, const char* stdCode, double position, bool isLast);

/**
 * @brief 条件单触发回调函数类型
 * 
 * 当设置的条件单被触发时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param target 目标价格
 * @param price 触发价格
 * @param usertag 用户标签
 */
typedef void(PORTER_FLAG *FuncStraCondTriggerCallback)(CtxHandler cHandle, const char* stdCode, double target, double price, const char* usertag);

/**
 * @brief 策略订单队列回调函数类型
 * 
 * 当订阅的合约有新的订单队列数据时调用此回调（主要用于HFT策略）
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordQue 订单队列数据结构指针
 */
typedef void(PORTER_FLAG *FuncStraOrdQueCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdQueStruct* ordQue);

/**
 * @brief 获取订单队列数据的回调函数类型
 * 
 * 用于获取历史订单队列数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordQue 订单队列数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 */
typedef void(PORTER_FLAG *FuncGetOrdQueCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdQueStruct* ordQue, WtUInt32 count, bool isLast);

/**
 * @brief 策略订单明细回调函数类型
 * 
 * 当订阅的合约有新的订单明细数据时调用此回调（主要用于HFT策略）
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordDtl 订单明细数据结构指针
 */
typedef void(PORTER_FLAG *FuncStraOrdDtlCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdDtlStruct* ordDtl);

/**
 * @brief 获取订单明细数据的回调函数类型
 * 
 * 用于获取历史订单明细数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param ordDtl 订单明细数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 */
typedef void(PORTER_FLAG *FuncGetOrdDtlCallback)(CtxHandler cHandle, const char* stdCode, WTSOrdDtlStruct* ordDtl, WtUInt32 count, bool isLast);

/**
 * @brief 策略逐笔成交回调函数类型
 * 
 * 当订阅的合约有新的逐笔成交数据时调用此回调（主要用于HFT策略）
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param trans 逐笔成交数据结构指针
 */
typedef void(PORTER_FLAG *FuncStraTransCallback)(CtxHandler cHandle, const char* stdCode, WTSTransStruct* trans);

/**
 * @brief 获取逐笔成交数据的回调函数类型
 * 
 * 用于获取历史逐笔成交数据
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param trans 逐笔成交数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 */
typedef void(PORTER_FLAG *FuncGetTransCallback)(CtxHandler cHandle, const char* stdCode, WTSTransStruct* trans, WtUInt32 count, bool isLast);

//////////////////////////////////////////////////////////////////////////
//HFT回调函数
// 以下回调函数专门用于高频交易（HFT）策略

/**
 * @brief HFT交易通道事件回调函数类型
 * 
 * 当HFT策略绑定的交易通道状态发生变化时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 * @param trader 交易通道ID
 * @param evtid 事件ID（CHNL_EVENT_READY或CHNL_EVENT_LOST）
 */
typedef void(PORTER_FLAG *FuncHftChannelCallback)(CtxHandler cHandle, const char* trader, WtUInt32 evtid);	//交易通道事件回调

/**
 * @brief HFT订单状态回调函数类型
 * 
 * 当订单状态发生变化时调用此回调（如订单提交、部分成交、全部成交、撤单等）
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
 * 当订单有成交回报时调用此回调
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
 * 当委托单提交后收到回报时调用此回调（成功或失败）
 * 
 * @param cHandle 策略上下文句柄
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param bSuccess 是否成功
 * @param message 返回消息（如果失败，包含错误信息）
 * @param userTag 用户标签
 */
typedef void(PORTER_FLAG *FuncHftEntrustCallback)(CtxHandler cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag);

/**
 * @brief HFT持仓变化回调函数类型
 * 
 * 当持仓发生变化时调用此回调
 * 
 * @param cHandle 策略上下文句柄
 * @param stdCode 标准合约代码
 * @param isLong true表示多头持仓，false表示空头持仓
 * @param prevol 变化前持仓数量
 * @param preavail 变化前可用持仓数量
 * @param newvol 变化后持仓数量
 * @param newavail 变化后可用持仓数量
 */
typedef void(PORTER_FLAG *FuncHftPosCallback)(CtxHandler cHandle, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail);


/**
 * @brief 引擎事件回调函数类型
 * 
 * 当引擎发生全局事件时调用此回调（如初始化、交易日开始/结束、调度等）
 * 
 * @param evtId 事件ID（EVENT_ENGINE_INIT、EVENT_SESSION_BEGIN等）
 * @param curDate 当前日期（格式：YYYYMMDD）
 * @param curTime 当前时间（格式：HHMMSS）
 */
typedef void(PORTER_FLAG *FuncEventCallback)(WtUInt32 evtId, WtUInt32 curDate, WtUInt32 curTime);

//////////////////////////////////////////////////////////////////////////
//扩展Parser回调函数
// 以下回调函数用于扩展的行情解析器（Parser）

/**
 * @brief Parser初始化事件常量
 */
static const WtUInt32	EVENT_PARSER_INIT		= 1;	//Parser初始化

/**
 * @brief Parser连接事件常量
 */
static const WtUInt32	EVENT_PARSER_CONNECT	= 2;	//Parser连接

/**
 * @brief Parser断开连接事件常量
 */
static const WtUInt32	EVENT_PARSER_DISCONNECT = 3;	//Parser断开连接

/**
 * @brief Parser释放事件常量
 */
static const WtUInt32	EVENT_PARSER_RELEASE	= 4;	//Parser释放

/**
 * @brief Parser事件回调函数类型
 * 
 * 当扩展Parser发生事件时调用此回调
 * 
 * @param evtId 事件ID（EVENT_PARSER_INIT、EVENT_PARSER_CONNECT等）
 * @param id Parser的ID标识
 */
typedef void(PORTER_FLAG *FuncParserEvtCallback)(WtUInt32 evtId, const char* id);

/**
 * @brief Parser订阅回调函数类型
 * 
 * 当Parser需要订阅或取消订阅合约时调用此回调
 * 
 * @param id Parser的ID标识
 * @param fullCode 完整的合约代码
 * @param isForSub true表示订阅，false表示取消订阅
 */
typedef void(PORTER_FLAG *FuncParserSubCallback)(const char* id, const char* fullCode, bool isForSub);

//////////////////////////////////////////////////////////////////////////
//扩展Executer回调函数
// 以下回调函数用于扩展的执行器（Executer）

/**
 * @brief 执行器初始化回调函数类型
 * 
 * 当执行器初始化完成时调用此回调
 * 
 * @param id 执行器的ID标识
 */
typedef void(PORTER_FLAG *FuncExecInitCallback)(const char* id);

/**
 * @brief 执行器命令回调函数类型
 * 
 * 当需要执行器调整持仓时调用此回调
 * 
 * @param id 执行器的ID标识
 * @param StdCode 标准合约代码
 * @param targetPos 目标持仓数量
 */
typedef void(PORTER_FLAG *FuncExecCmdCallback)(const char* id, const char* StdCode, double targetPos);

//////////////////////////////////////////////////////////////////////////
//外部数据加载模块
// 以下回调函数用于从外部数据源加载历史数据

/**
 * @brief 加载复权K线数据的回调函数类型
 * 
 * 用于从外部数据源加载复权后的K线数据
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadFnlBars)(const char* stdCode, const char* period);

/**
 * @brief 加载原始K线数据的回调函数类型
 * 
 * 用于从外部数据源加载未复权的原始K线数据
 * 
 * @param stdCode 标准合约代码
 * @param period K线周期（如"m1"、"d1"等）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadRawBars)(const char* stdCode, const char* period);

/**
 * @brief 加载复权因子的回调函数类型
 * 
 * 用于从外部数据源加载复权因子数据
 * 
 * @param stdCode 标准合约代码（如果为空字符串，则加载所有合约的复权因子）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadAdjFactors)(const char* stdCode);

/**
 * @brief 加载原始Tick数据的回调函数类型
 * 
 * 用于从外部数据源加载指定日期的Tick数据
 * 
 * @param stdCode 标准合约代码
 * @param uDate 交易日期（格式：YYYYMMDD）
 * @return 是否加载成功
 */
typedef bool(PORTER_FLAG *FuncLoadRawTicks)(const char* stdCode, uint32_t uDate);
