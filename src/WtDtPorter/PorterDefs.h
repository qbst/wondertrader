/*!
 * \file PorterDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtDtPorter模块类型定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader数据服务Porter模块的核心类型定义文件，定义了数据导入导出接口所需的所有类型和回调函数。
 * 
 * 主要功能包括：
 * 1. 定义扩展Parser（行情解析器）的事件类型常量，用于标识Parser的各种状态变化
 * 2. 定义扩展Parser的回调函数类型，用于处理Parser事件和订阅请求
 * 3. 定义扩展Dumper（数据转储器）的回调函数类型，支持K线、Tick、委托队列、委托明细、逐笔成交等多种数据的转储
 * 4. 前向声明WonderTrader的核心数据结构，包括Tick、K线、委托队列、委托明细、逐笔成交等
 * 
 * 设计思想：
 * - 通过回调函数机制实现模块间的解耦，允许外部模块注册自定义的数据处理逻辑
 * - 使用函数指针类型定义，支持动态链接和插件化架构
 * - 统一数据接口，支持多种数据类型的标准化处理
 * 
 * 该文件是WtDtPorter模块对外提供扩展功能的基础，所有扩展Parser和Dumper都基于此文件定义的接口实现。
 */
#pragma once  // 防止头文件重复包含

#include <stdint.h>  // 包含标准整数类型定义
#include "../Includes/WTSTypes.h"  // 包含WonderTrader基础类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
struct WTSTickStruct;      // 前向声明：Tick行情数据结构体
struct WTSBarStruct;       // 前向声明：K线数据结构体
struct WTSOrdDtlStruct;    // 前向声明：委托明细数据结构体
struct WTSOrdQueStruct;    // 前向声明：委托队列数据结构体
struct WTSTransStruct;     // 前向声明：逐笔成交数据结构体
NS_WTP_END  // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
// 扩展Parser回调函数事件类型定义
// 这些常量定义了扩展Parser可能触发的各种事件，用于回调函数中识别事件类型
//////////////////////////////////////////////////////////////////////////
static const WtUInt32	EVENT_PARSER_INIT = 1;	       // Parser初始化事件：当Parser完成初始化时触发
static const WtUInt32	EVENT_PARSER_CONNECT = 2;	   // Parser连接事件：当Parser成功连接到数据源时触发
static const WtUInt32	EVENT_PARSER_DISCONNECT = 3;   // Parser断开连接事件：当Parser与数据源断开连接时触发
static const WtUInt32	EVENT_PARSER_RELEASE = 4;	   // Parser释放事件：当Parser释放资源时触发

/**
 * @typedef FuncParserEvtCallback
 * @brief 扩展Parser事件回调函数类型
 * 
 * 该函数指针类型用于定义Parser事件的回调函数，当Parser发生状态变化时会调用此函数。
 * 
 * @param evtId Parser事件ID，取值为EVENT_PARSER_*系列常量
 * @param id Parser的唯一标识符，用于区分不同的Parser实例
 * 
 * 使用场景：外部模块可以注册此类型的回调函数来监听Parser的状态变化，
 *          例如在Parser连接成功后开始订阅行情，或在断开连接后进行重连操作。
 */
typedef void(PORTER_FLAG *FuncParserEvtCallback)(WtUInt32 evtId, const char* id);

/**
 * @typedef FuncParserSubCallback
 * @brief 扩展Parser订阅回调函数类型
 * 
 * 该函数指针类型用于定义Parser订阅操作的回调函数，当Parser需要订阅或退订行情时会调用此函数。
 * 
 * @param id Parser的唯一标识符，用于区分不同的Parser实例
 * @param fullCode 完整的合约代码，包含交易所前缀
 * @param isForSub 订阅标志，true表示订阅操作，false表示退订操作
 * 
 * 使用场景：外部模块可以注册此类型的回调函数来处理订阅请求，
 *          例如将订阅信息发送给数据源，或记录订阅日志。
 */
typedef void(PORTER_FLAG *FuncParserSubCallback)(const char* id, const char* fullCode, bool isForSub);


//////////////////////////////////////////////////////////////////////////
// 扩展Dumper回调函数类型定义
// 这些函数指针类型定义了数据转储器的回调接口，用于将历史数据转储到存储系统
//////////////////////////////////////////////////////////////////////////

/**
 * @typedef FuncDumpBars
 * @brief K线数据转储回调函数类型
 * 
 * 该函数指针类型用于定义K线数据的转储回调函数，用于将历史K线数据写入存储系统。
 * 
 * @param id 转储器的唯一标识符，用于区分不同的Dumper实例
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param period K线周期，如"m1"(1分钟)、"m5"(5分钟)、"day"(日线)等
 * @param bars K线数据数组指针，包含开高低收量额等信息
 * @param count K线数据条数，表示bars数组的长度
 * @return bool 转储成功返回true，失败返回false
 * 
 * 使用场景：实现自定义的K线数据存储逻辑，例如存储到数据库、文件或云存储。
 */
typedef bool(PORTER_FLAG *FuncDumpBars)(const char* id, const char* stdCode, const char* period, WTSBarStruct* bars, WtUInt32 count);

/**
 * @typedef FuncDumpTicks
 * @brief Tick行情数据转储回调函数类型
 * 
 * 该函数指针类型用于定义Tick数据的转储回调函数，用于将历史Tick数据写入存储系统。
 * 
 * @param id 转储器的唯一标识符，用于区分不同的Dumper实例
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD的无符号整数
 * @param ticks Tick数据数组指针，包含价格、成交量、买卖盘等信息
 * @param count Tick数据条数，表示ticks数组的长度
 * @return bool 转储成功返回true，失败返回false
 * 
 * 使用场景：实现自定义的Tick数据存储逻辑，用于高频交易分析和回测。
 */
typedef bool(PORTER_FLAG *FuncDumpTicks)(const char* id, const char* stdCode, WtUInt32 uDate, WTSTickStruct* ticks, WtUInt32 count);

/**
 * @typedef FuncDumpOrdQue
 * @brief 委托队列数据转储回调函数类型
 * 
 * 该函数指针类型用于定义委托队列数据的转储回调函数，用于将历史委托队列数据写入存储系统。
 * 委托队列数据主要用于Level2行情，显示买卖盘口的深度信息。
 * 
 * @param id 转储器的唯一标识符，用于区分不同的Dumper实例
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD的无符号整数
 * @param items 委托队列数据数组指针，包含买卖盘口队列信息
 * @param count 委托队列数据条数，表示items数组的长度
 * @return bool 转储成功返回true，失败返回false
 * 
 * 使用场景：实现自定义的委托队列数据存储逻辑，用于Level2行情分析。
 */
typedef bool(PORTER_FLAG *FuncDumpOrdQue)(const char* id, const char* stdCode, WtUInt32 uDate, WTSOrdQueStruct* items, WtUInt32 count);

/**
 * @typedef FuncDumpOrdDtl
 * @brief 委托明细数据转储回调函数类型
 * 
 * 该函数指针类型用于定义委托明细数据的转储回调函数，用于将历史委托明细数据写入存储系统。
 * 委托明细数据主要用于Level2行情，显示每笔委托的详细信息。
 * 
 * @param id 转储器的唯一标识符，用于区分不同的Dumper实例
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD的无符号整数
 * @param items 委托明细数据数组指针，包含每笔委托的详细信息
 * @param count 委托明细数据条数，表示items数组的长度
 * @return bool 转储成功返回true，失败返回false
 * 
 * 使用场景：实现自定义的委托明细数据存储逻辑，用于深度市场微观结构分析。
 */
typedef bool(PORTER_FLAG *FuncDumpOrdDtl)(const char* id, const char* stdCode, WtUInt32 uDate, WTSOrdDtlStruct* items, WtUInt32 count);

/**
 * @typedef FuncDumpTrans
 * @brief 逐笔成交数据转储回调函数类型
 * 
 * 该函数指针类型用于定义逐笔成交数据的转储回调函数，用于将历史逐笔成交数据写入存储系统。
 * 逐笔成交数据记录了每笔实际成交的详细信息，包括成交价格、数量、时间等。
 * 
 * @param id 转储器的唯一标识符，用于区分不同的Dumper实例
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD的无符号整数
 * @param items 逐笔成交数据数组指针，包含每笔成交的详细信息
 * @param count 逐笔成交数据条数，表示items数组的长度
 * @return bool 转储成功返回true，失败返回false
 * 
 * 使用场景：实现自定义的逐笔成交数据存储逻辑，用于交易行为分析和市场微观结构研究。
 */
typedef bool(PORTER_FLAG *FuncDumpTrans)(const char* id, const char* stdCode, WtUInt32 uDate, WTSTransStruct* items, WtUInt32 count);
