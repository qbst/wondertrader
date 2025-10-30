/*!
 * \file WtDtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据辅助工具头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDtHelper（数据辅助工具）模块的C接口，是WonderTrader框架中用于数据格式转换、
 * 数据读取、数据存储的核心工具模块。该模块提供了丰富的数据处理功能，支持多种数据格式之间的
 * 转换，是连接外部数据源与WonderTrader内部数据格式的重要桥梁。
 * 
 * 核心设计理念：
 * 
 * 1. 数据格式标准化（Data Format Standardization）：
 *    - 统一处理WonderTrader的DSB（Data Storage Binary）格式
 *    - 统一处理WonderTrader的DMB（Data Memory Binary）格式
 *    - 提供CSV格式与二进制格式的双向转换
 *    - 支持多种市场数据类型的标准化存储
 * 
 * 2. 跨语言接口设计（Cross-Language Interface）：
 *    - 提供纯C接口，支持Python、C#等多语言调用
 *    - 使用回调函数机制实现异步数据处理
 *    - 统一的错误处理和日志记录机制
 *    - 内存管理由调用方控制，避免跨语言内存问题
 * 
 * 3. 高性能数据处理（High-Performance Data Processing）：
 *    - 支持大批量数据的流式处理
 *    - 内置数据压缩和解压缩功能
 *    - 优化的内存使用和文件I/O操作
 *    - 支持多线程安全的数据访问
 * 
 * 主要功能模块：
 * 
 * 1. 数据导出模块（Data Export Module）：
 *    - dump_bars()：将二进制K线数据导出为CSV格式
 *    - dump_ticks()：将二进制Tick数据导出为CSV格式
 *    - 支持批量文件处理和过滤功能
 * 
 * 2. 数据导入模块（Data Import Module）：
 *    - trans_csv_bars()：将CSV格式K线数据转换为二进制格式
 *    - store_bars()：存储K线数据到二进制文件
 *    - store_ticks()：存储Tick数据到二进制文件
 *    - store_order_details()：存储逐笔委托数据
 *    - store_order_queues()：存储委托队列数据
 *    - store_transactions()：存储逐笔成交数据
 * 
 * 3. 数据读取模块（Data Reading Module）：
 *    - read_dsb_*系列：读取DSB格式的各类数据
 *    - read_dmb_*系列：读取DMB格式的各类数据
 *    - 支持回调方式的流式数据读取
 * 
 * 4. 数据重采样模块（Data Resampling Module）：
 *    - resample_bars()：K线数据重采样功能
 *    - 支持不同周期间的数据转换
 *    - 支持交易时段的精确处理
 * 
 * 数据格式支持：
 * 
 * 1. K线数据（Bar Data）：
 *    - 支持1分钟、5分钟、日线等多种周期
 *    - 包含开高低收、成交量、持仓量等完整信息
 *    - 支持期货、股票、期权等多种品种
 * 
 * 2. Tick数据（Tick Data）：
 *    - 完整的Level-1行情数据
 *    - 包含买卖盘口、成交信息等
 *    - 支持毫秒级时间戳精度
 * 
 * 3. Level-2数据（Level-2 Data）：
 *    - 逐笔委托数据（Order Details）
 *    - 委托队列数据（Order Queues）
 *    - 逐笔成交数据（Transactions）
 * 
 * 回调函数设计：
 * 
 * 1. 数据回调（Data Callbacks）：
 *    - FuncGetBarsCallback：K线数据回调
 *    - FuncGetTicksCallback：Tick数据回调
 *    - FuncGetOrdDtlCallback：逐笔委托回调
 *    - FuncGetOrdQueCallback：委托队列回调
 *    - FuncGetTransCallback：逐笔成交回调
 * 
 * 2. 控制回调（Control Callbacks）：
 *    - FuncCountDataCallback：数据计数回调
 *    - FuncLogCallback：日志记录回调
 * 
 * 使用场景：
 * - 历史数据的格式转换和迁移
 * - 外部数据源的接入和标准化
 * - 数据分析工具的开发
 * - 跨语言数据处理程序的开发
 * - 数据质量检查和验证
 * 
 * 技术特点：
 * - 纯C接口设计，兼容性强
 * - 支持大文件的流式处理
 * - 内置压缩算法，节省存储空间
 * - 完善的错误处理机制
 * - 高效的内存管理
 * 
 * 注意事项：
 * - 所有字符串参数使用WtString类型
 * - 回调函数需要处理多次调用的情况
 * - 文件路径需要使用标准化的路径分隔符
 * - 大数据量处理时需要注意内存使用
 */

#pragma once                                                    // 防止头文件重复包含

#include "../Includes/WTSTypes.h"                               // 包含WonderTrader基础类型定义

NS_WTP_BEGIN                                                    // WonderTrader命名空间开始
struct WTSBarStruct;                                            // K线数据结构前向声明
struct WTSTickStruct;                                           // Tick数据结构前向声明
struct WTSOrdDtlStruct;                                         // 逐笔委托数据结构前向声明
struct WTSOrdQueStruct;                                         // 委托队列数据结构前向声明
struct WTSTransStruct;                                          // 逐笔成交数据结构前向声明
NS_WTP_END                                                      // WonderTrader命名空间结束

USING_NS_WTP;                                                   // 使用WonderTrader命名空间

/**
 * @brief 日志回调函数类型定义
 * @param message 日志消息内容
 * 
 * 用于接收数据处理过程中的日志信息，包括进度提示、错误信息、调试信息等。
 * 调用方可以通过此回调实现自定义的日志处理逻辑。
 */
typedef void(PORTER_FLAG *FuncLogCallback)(WtString message);

/**
 * @brief K线数据回调函数类型定义
 * @param bar K线数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收读取到的K线数据。支持分批次回调，通过isLast参数判断是否为最后一批。
 * 调用方需要在回调中处理接收到的数据，可以进行存储、分析或其他处理。
 */
typedef void(PORTER_FLAG *FuncGetBarsCallback)(WTSBarStruct* bar, WtUInt32 count, bool isLast);

/**
 * @brief Tick数据回调函数类型定义
 * @param tick Tick数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收读取到的Tick数据。支持分批次回调，适用于大量Tick数据的流式处理。
 */
typedef void(PORTER_FLAG *FuncGetTicksCallback)(WTSTickStruct* tick, WtUInt32 count, bool isLast);

/**
 * @brief 逐笔委托数据回调函数类型定义
 * @param item 逐笔委托数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收读取到的逐笔委托数据，主要用于股票Level-2数据处理。
 */
typedef void(PORTER_FLAG *FuncGetOrdDtlCallback)(WTSOrdDtlStruct* item, WtUInt32 count, bool isLast);

/**
 * @brief 委托队列数据回调函数类型定义
 * @param item 委托队列数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收读取到的委托队列数据，主要用于股票Level-2数据处理。
 */
typedef void(PORTER_FLAG *FuncGetOrdQueCallback)(WTSOrdQueStruct* item, WtUInt32 count, bool isLast);

/**
 * @brief 逐笔成交数据回调函数类型定义
 * @param item 逐笔成交数据数组指针
 * @param count 数据条数
 * @param isLast 是否为最后一批数据
 * 
 * 用于接收读取到的逐笔成交数据，主要用于股票Level-2数据处理。
 */
typedef void(PORTER_FLAG *FuncGetTransCallback)(WTSTransStruct* item, WtUInt32 count, bool isLast);

/**
 * @brief 数据计数回调函数类型定义
 * @param dataCnt 数据总条数
 * 
 * 在开始读取数据前调用，通知调用方即将读取的数据总量，便于进行进度显示或内存预分配。
 */
typedef void(PORTER_FLAG *FuncCountDataCallback)(WtUInt32 dataCnt);

// 以下为历史遗留的回调函数类型定义，已改为直接从Python传递内存块的方式
//typedef bool(PORTER_FLAG *FuncGetBarItem)(WTSBarStruct* curBar,int idx);
//typedef bool(PORTER_FLAG *FuncGetTickItem)(WTSTickStruct* curTick, int idx);

#ifdef __cplusplus                                              // C++环境下的extern "C"声明
extern "C"
{
#endif

	// ===== 数据导出功能接口 =====

	/**
	 * @brief 将二进制K线数据导出为CSV格式
	 * @param binFolder 二进制数据文件夹路径
	 * @param csvFolder CSV输出文件夹路径
	 * @param strFilter 文件过滤器（可选，默认为空表示处理所有文件）
	 * @param cbLogger 日志回调函数（可选）
	 * 
	 * 批量将指定文件夹下的DSB格式K线数据文件转换为CSV格式。
	 * 支持多种周期的K线数据，自动识别数据类型和周期。
	 * CSV格式包含：date,time,open,high,low,close,settle,volume,turnover,open_interest,diff_interest
	 */
	EXPORT_FLAG	void		dump_bars(WtString binFolder, WtString csvFolder, WtString strFilter = "", FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 将二进制Tick数据导出为CSV格式
	 * @param binFolder 二进制数据文件夹路径
	 * @param csvFolder CSV输出文件夹路径
	 * @param strFilter 文件过滤器（可选，默认为空表示处理所有文件）
	 * @param cbLogger 日志回调函数（可选）
	 * 
	 * 批量将指定文件夹下的DSB格式Tick数据文件转换为CSV格式。
	 * CSV格式包含完整的Tick信息，包括买卖盘口、成交量等。
	 */
	EXPORT_FLAG	void		dump_ticks(WtString binFolder, WtString csvFolder, WtString strFilter = "", FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 将CSV格式K线数据转换为二进制格式
	 * @param csvFolder CSV数据文件夹路径
	 * @param binFolder 二进制输出文件夹路径
	 * @param period 数据周期（"m1"=1分钟, "m5"=5分钟, "d"=日线）
	 * @param cbLogger 日志回调函数（可选）
	 * 
	 * 批量将CSV格式的K线数据转换为WonderTrader的DSB二进制格式。
	 * 转换后的数据会进行压缩存储，节省磁盘空间。
	 */
	EXPORT_FLAG	void		trans_csv_bars(WtString csvFolder, WtString binFolder, WtString period, FuncLogCallback cbLogger = NULL);

	// ===== DSB格式数据读取接口 =====

	/**
	 * @brief 读取DSB格式的Tick数据
	 * @param tickFile Tick数据文件路径
	 * @param cb Tick数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DSB格式的Tick数据文件，通过回调函数返回数据。
	 * 支持压缩数据的自动解压和版本兼容性处理。
	 */
	EXPORT_FLAG	WtUInt32	read_dsb_ticks(WtString tickFile, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 读取DSB格式的逐笔委托数据
	 * @param dataFile 数据文件路径
	 * @param cb 逐笔委托数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DSB格式的逐笔委托数据，主要用于股票Level-2数据处理。
	 */
	EXPORT_FLAG	WtUInt32	read_dsb_order_details(WtString dataFile, FuncGetOrdDtlCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 读取DSB格式的委托队列数据
	 * @param dataFile 数据文件路径
	 * @param cb 委托队列数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DSB格式的委托队列数据，主要用于股票Level-2数据处理。
	 */
	EXPORT_FLAG	WtUInt32	read_dsb_order_queues(WtString dataFile, FuncGetOrdQueCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 读取DSB格式的逐笔成交数据
	 * @param dataFile 数据文件路径
	 * @param cb 逐笔成交数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DSB格式的逐笔成交数据，主要用于股票Level-2数据处理。
	 */
	EXPORT_FLAG	WtUInt32	read_dsb_transactions(WtString dataFile, FuncGetTransCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 读取DSB格式的K线数据
	 * @param barFile K线数据文件路径
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DSB格式的K线数据文件，支持多种周期的K线数据。
	 * 自动处理数据压缩和版本兼容性问题。
	 */
	EXPORT_FLAG	WtUInt32	read_dsb_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	// ===== DMB格式数据读取接口 =====

	/**
	 * @brief 读取DMB格式的Tick数据
	 * @param tickFile Tick数据文件路径
	 * @param cb Tick数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DMB（Data Memory Binary）格式的Tick数据，主要用于实时数据的内存映射访问。
	 */
	EXPORT_FLAG	WtUInt32	read_dmb_ticks(WtString tickFile, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 读取DMB格式的K线数据
	 * @param barFile K线数据文件路径
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 读取的数据条数
	 * 
	 * 读取DMB格式的K线数据，主要用于实时数据的快速访问。
	 */
	EXPORT_FLAG	WtUInt32	read_dmb_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger = NULL);

	// 以下为历史遗留的函数声明，已注释掉
	//EXPORT_FLAG bool		trans_bars(WtString barFile, FuncGetBarItem getter, int count, WtString period, FuncLogCallback cbLogger = NULL);
	//EXPORT_FLAG bool		trans_ticks(WtString tickFile, FuncGetTickItem getter, int count, FuncLogCallback cbLogger = NULL);

	// ===== 数据存储功能接口 =====

	/**
	 * @brief 存储K线数据到二进制文件
	 * @param barFile 输出文件路径
	 * @param firstBar K线数据数组首地址
	 * @param count 数据条数
	 * @param period 数据周期（"m1"=1分钟, "m5"=5分钟, "d"=日线）
	 * @param cbLogger 日志回调函数（可选）
	 * @return 存储是否成功
	 * 
	 * 将内存中的K线数据存储为DSB格式的二进制文件。
	 * 数据会进行压缩处理，减少文件大小。
	 */
	EXPORT_FLAG bool		store_bars(WtString barFile, WTSBarStruct* firstBar, int count, WtString period, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 存储Tick数据到二进制文件
	 * @param tickFile 输出文件路径
	 * @param firstTick Tick数据数组首地址
	 * @param count 数据条数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 存储是否成功
	 * 
	 * 将内存中的Tick数据存储为DSB格式的二进制文件。
	 */
	EXPORT_FLAG bool		store_ticks(WtString tickFile, WTSTickStruct* firstTick, int count, FuncLogCallback cbLogger = NULL);

	// ===== 股票Level-2数据存储接口 =====

	/**
	 * @brief 存储逐笔委托数据到二进制文件
	 * @param tickFile 输出文件路径
	 * @param firstItem 逐笔委托数据数组首地址
	 * @param count 数据条数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 存储是否成功
	 * 
	 * 将逐笔委托数据存储为DSB格式，主要用于股票Level-2数据的持久化。
	 */
	EXPORT_FLAG bool		store_order_details(WtString tickFile, WTSOrdDtlStruct* firstItem, int count, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 存储委托队列数据到二进制文件
	 * @param tickFile 输出文件路径
	 * @param firstItem 委托队列数据数组首地址
	 * @param count 数据条数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 存储是否成功
	 * 
	 * 将委托队列数据存储为DSB格式，主要用于股票Level-2数据的持久化。
	 */
	EXPORT_FLAG bool		store_order_queues(WtString tickFile, WTSOrdQueStruct* firstItem, int count, FuncLogCallback cbLogger = NULL);

	/**
	 * @brief 存储逐笔成交数据到二进制文件
	 * @param tickFile 输出文件路径
	 * @param firstItem 逐笔成交数据数组首地址
	 * @param count 数据条数
	 * @param cbLogger 日志回调函数（可选）
	 * @return 存储是否成功
	 * 
	 * 将逐笔成交数据存储为DSB格式，主要用于股票Level-2数据的持久化。
	 */
	EXPORT_FLAG bool		store_transactions(WtString tickFile, WTSTransStruct* firstItem, int count, FuncLogCallback cbLogger = NULL);

	// ===== 数据重采样功能接口 =====

	/**
	 * @brief K线数据重采样功能
	 * @param barFile 源K线数据文件路径
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @param fromTime 开始时间（日线：yyyymmdd，分钟线：yyyymmddHHMM）
	 * @param endTime 结束时间（日线：yyyymmdd，分钟线：yyyymmddHHMM）
	 * @param period 基础周期（"m1"=1分钟, "m5"=5分钟, "d"=日线）
	 * @param times 重采样倍数（如：基础周期m1，times=5，得到5分钟线）
	 * @param sessInfo 交易时段信息（JSON格式）
	 * @param cbLogger 日志回调函数（可选）
	 * @param bAlignSec 是否按秒对齐（可选，默认false）
	 * @return 重采样后的数据条数
	 * 
	 * 对K线数据进行重采样，将基础周期的数据转换为更大周期的数据。
	 * 支持精确的交易时段处理，确保重采样结果的准确性。
	 * 
	 * 交易时段信息格式示例：
	 * {
	 *   "offset": 0,
	 *   "auction": {"from": 925, "to": 930},
	 *   "sections": [
	 *     {"from": 930, "to": 1130},
	 *     {"from": 1300, "to": 1500}
	 *   ]
	 * }
	 */
	EXPORT_FLAG WtUInt32	resample_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, 
		WtUInt64 fromTime, WtUInt64 endTime, WtString period, WtUInt32 times, WtString sessInfo, FuncLogCallback cbLogger = NULL, bool bAlignSec = false);

#ifdef __cplusplus                                              // C++环境下的extern "C"声明结束
}
#endif