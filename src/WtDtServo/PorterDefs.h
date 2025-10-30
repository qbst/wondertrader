/*!
 * \file PorterDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据服务模块类型定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDtServo（数据伺服器）模块的C接口回调函数类型，是WonderTrader框架中
 * 用于提供实时数据随机访问服务的核心定义文件。该文件定义了所有跨语言调用的回调函数
 * 类型，使得Python、C#等外部语言可以通过C接口访问WonderTrader的数据服务功能。
 * 
 * 核心设计理念：
 * 
 * 1. 跨语言接口设计（Cross-Language Interface）：
 *    - 定义纯C函数指针类型，便于跨语言调用
 *    - 使用PORTER_FLAG宏确保函数调用约定一致
 *    - 支持异步回调机制，提高数据访问效率
 * 
 * 2. 数据类型抽象（Data Type Abstraction）：
 *    - 使用前向声明避免循环依赖
 *    - 通过命名空间封装数据结构
 *    - 提供统一的数据访问接口
 * 
 * 回调函数类型说明：
 * 
 * 1. 数据查询回调（Data Query Callbacks）：
 *    - FuncGetBarsCallback：K线数据查询回调
 *    - FuncGetTicksCallback：Tick数据查询回调
 *    - FuncCountDataCallback：数据计数回调
 * 
 * 2. 实时数据回调（Real-time Data Callbacks）：
 *    - FuncOnTickCallback：实时Tick数据回调
 *    - FuncOnBarCallback：实时K线数据回调
 * 
 * 使用场景：
 * - 跨语言数据访问接口开发
 * - 实时行情数据订阅和回调
 * - 历史数据查询和统计
 * - 自定义数据处理程序开发
 * 
 * 注意事项：
 * - 所有回调函数必须是C链接规范
 * - 回调函数需要处理多次调用的情况
 * - 内存管理由调用方或回调方负责
 * - 回调函数应该快速返回，避免阻塞
 */
#pragma once                                                                     // 防止头文件重复包含

#include <stdint.h>                                                              // 包含标准整数类型定义
#include "../Includes/WTSTypes.h"                                               // 包含WonderTrader基础类型定义

NS_WTP_BEGIN                                                                    // WonderTrader命名空间开始
struct WTSBarStruct;                                                            // K线数据结构前向声明
struct WTSTickStruct;                                                           // Tick数据结构前向声明
NS_WTP_END                                                                      // WonderTrader命名空间结束

USING_NS_WTP;                                                                   // 使用WonderTrader命名空间

/**
 * @brief K线数据查询回调函数类型定义
 * @param bar K线数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收查询到的K线数据。支持分批次回调，通过isLast参数判断是否为最后一批。
 * 调用方需要在回调中处理接收到的数据，可以进行存储、分析或其他处理。
 */
typedef void(PORTER_FLAG *FuncGetBarsCallback)(WTSBarStruct* bar, WtUInt32 count, bool isLast);

/**
 * @brief Tick数据查询回调函数类型定义
 * @param tick Tick数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收查询到的Tick数据。支持分批次回调，适用于大量Tick数据的流式处理。
 */
typedef void(PORTER_FLAG *FuncGetTicksCallback)(WTSTickStruct* tick, WtUInt32 count, bool isLast);

/**
 * @brief 数据计数回调函数类型定义
 * @param dataCnt 数据总条数
 * 
 * 在开始查询数据前调用，通知调用方即将查询的数据总量，便于进行进度显示或内存预分配。
 */
typedef void(PORTER_FLAG *FuncCountDataCallback)(WtUInt32 dataCnt);

/**
 * @brief 实时Tick数据回调函数类型定义
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param tick Tick数据指针
 * 
 * 用于接收实时推送的Tick数据。当订阅的合约有新Tick数据时，该回调函数会被调用。
 * 主要用于实时行情监控、策略计算等场景。
 */
typedef void(PORTER_FLAG *FuncOnTickCallback)(const char* stdCode, WTSTickStruct* tick);

/**
 * @brief 实时K线数据回调函数类型定义
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param period 周期字符串（如"m1"=1分钟, "m5"=5分钟, "d"=日线）
 * @param bar K线数据指针
 * 
 * 用于接收实时推送的K线数据。当订阅的合约有新K线数据生成时，该回调函数会被调用。
 * 主要用于实时K线监控、技术指标计算等场景。
 */
typedef void(PORTER_FLAG *FuncOnBarCallback)(const char* stdCode, const char* period, WTSBarStruct* bar);

