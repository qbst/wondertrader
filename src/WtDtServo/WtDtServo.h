/*!
 * \file WtDtServo.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据伺服器C接口头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDtServo（数据伺服器）模块的C接口，是WonderTrader框架中用于提供
 * 实时数据随机访问服务的核心接口文件。该文件提供了纯C接口，使得Python、C#等外部
 * 语言可以通过C接口访问WonderTrader的数据服务功能，实现历史数据查询、实时数据
 * 订阅等功能。
 * 
 * 核心设计理念：
 * 
 * 1. 跨语言接口设计（Cross-Language Interface）：
 *    - 提供纯C接口，支持Python、C#等多语言调用
 *    - 使用extern "C"确保C链接规范
 *    - 使用回调函数机制实现异步数据处理
 * 
 * 2. 数据查询功能（Data Query Functions）：
 *    - 支持按时间范围查询K线和Tick数据
 *    - 支持按日期查询K线和Tick数据
 *    - 支持按数量查询K线和Tick数据
 *    - 支持自定义秒级K线查询
 * 
 * 3. 实时数据订阅（Real-time Data Subscription）：
 *    - 支持Tick数据订阅和回调
 *    - 支持K线数据订阅和回调
 *    - 支持缓存清理功能
 * 
 * 主要功能模块：
 * 
 * 1. 初始化功能：
 *    - initialize()：初始化数据伺服器
 *    - get_version()：获取版本信息
 * 
 * 2. 历史数据查询：
 *    - get_bars_by_range()：按时间范围查询K线
 *    - get_bars_by_date()：按日期查询K线
 *    - get_bars_by_count()：按数量查询K线
 *    - get_ticks_by_range()：按时间范围查询Tick
 *    - get_ticks_by_date()：按日期查询Tick
 *    - get_ticks_by_count()：按数量查询Tick
 *    - get_sbars_by_date()：按日期查询秒级K线
 * 
 * 3. 实时数据订阅：
 *    - subscribe_tick()：订阅Tick数据
 *    - subscribe_bar()：订阅K线数据
 * 
 * 4. 缓存管理：
 *    - clear_cache()：清理数据缓存
 * 
 * 使用场景：
 * - Python/C#等外部语言的数据访问
 * - 实时行情数据订阅和监控
 * - 历史数据分析和回测
 * - 自定义数据处理程序开发
 * 
 * 技术特点：
 * - 纯C接口设计，兼容性强
 * - 支持多种数据查询方式
 * - 实时数据推送机制
 * - 高效的数据缓存管理
 * 
 * 注意事项：
 * - 必须先调用initialize()进行初始化
 * - 回调函数需要处理多次调用的情况
 * - 数据切片对象需要调用release()释放
 * - 订阅功能需要配合Parser使用
 */
#pragma once                                                                     // 防止头文件重复包含
#include "PorterDefs.h"                                                          // 包含Porter模块类型定义（回调函数类型等）


#ifdef __cplusplus                                                              // C++环境下的extern "C"声明
extern "C"
{
#endif

	/**
	 * @brief 初始化数据伺服器
	 * @param cfgFile 配置文件路径或配置内容（取决于isFile参数）
	 * @param isFile 是否为配置文件路径（true=文件路径，false=配置内容字符串）
	 * @param logCfg 日志配置文件路径（可选，默认为"logcfg.yaml"）
	 * @param cbTick 实时Tick数据回调函数（可选）
	 * @param cbBar 实时K线数据回调函数（可选）
	 * 
	 * 初始化数据伺服器模块，加载配置文件、初始化日志系统、设置数据管理器等。
	 * 这是使用数据伺服器功能的第一步，必须在使用其他功能前调用。
	 */
	EXPORT_FLAG void		initialize(WtString cfgFile, bool isFile, WtString logCfg, FuncOnTickCallback cbTick, FuncOnBarCallback cbBar);

	/**
	 * @brief 获取数据伺服器版本信息
	 * @return 版本信息字符串（包含平台、版本号、编译日期和时间）
	 * 
	 * 返回数据伺服器的版本信息，包括平台名称、版本号、编译日期和时间。
	 * 格式示例："X64 0.9.9 Build@Jan 01 2024 12:00:00"
	 */
	EXPORT_FLAG	WtString	get_version();

	// ===== K线数据查询接口 =====

	/**
	 * @brief 按时间范围查询K线数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param period 周期字符串（"m1"=1分钟, "m5"=5分钟, "d"=日线等）
	 * @param beginTime 开始时间（格式：yyyymmddHHMM或yyyymmdd，取决于周期）
	 * @param endTime 结束时间（格式：yyyymmddHHMM或yyyymmdd，取决于周期）
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定时间范围内的K线数据，通过回调函数返回查询结果。
	 * 支持分批次回调，适用于大量数据的流式处理。
	 */
	EXPORT_FLAG	WtUInt32	get_bars_by_range(const char* stdCode, const char* period, WtUInt64 beginTime, WtUInt64 endTime, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt);

	/**
	 * @brief 按日期查询K线数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param period 周期字符串（"m1"=1分钟, "m5"=5分钟, "d"=日线等）
	 * @param uDate 交易日期（格式：yyyymmdd）
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定交易日的所有K线数据，通过回调函数返回查询结果。
	 */
	EXPORT_FLAG	WtUInt32	get_bars_by_date(const char* stdCode, const char* period, WtUInt32 uDate, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt);

	/**
	 * @brief 按数量查询K线数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param period 周期字符串（"m1"=1分钟, "m5"=5分钟, "d"=日线等）
	 * @param count 查询的数据条数
	 * @param endTime 结束时间（格式：yyyymmddHHMM或yyyymmdd，取决于周期）
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定结束时间之前的N条K线数据，通过回调函数返回查询结果。
	 * 如果数据不足，返回实际可用的数据条数。
	 */
	EXPORT_FLAG	WtUInt32	get_bars_by_count(const char* stdCode, const char* period, WtUInt32 count, WtUInt64 endTime, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt);

	/**
	 * @brief 按日期查询秒级K线数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param secs 秒数（如：60表示60秒K线）
	 * @param uDate 交易日期（格式：yyyymmdd）
	 * @param cb K线数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定交易日的自定义秒级K线数据，通过回调函数返回查询结果。
	 * 秒级K线是从Tick数据中实时生成的，适用于高频数据分析。
	 */
	EXPORT_FLAG	WtUInt32	get_sbars_by_date(const char* stdCode, WtUInt32 secs, WtUInt32 uDate, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt);

	// ===== Tick数据查询接口 =====

	/**
	 * @brief 按时间范围查询Tick数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param beginTime 开始时间（格式：yyyymmddHHMMSS）
	 * @param endTime 结束时间（格式：yyyymmddHHMMSS）
	 * @param cb Tick数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定时间范围内的Tick数据，通过回调函数返回查询结果。
	 * 支持分批次回调，适用于大量Tick数据的流式处理。
	 */
	EXPORT_FLAG	WtUInt32	get_ticks_by_range(const char* stdCode, WtUInt64 beginTime, WtUInt64 endTime, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt);

	/**
	 * @brief 按日期查询Tick数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param uDate 交易日期（格式：yyyymmdd）
	 * @param cb Tick数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定交易日的所有Tick数据，通过回调函数返回查询结果。
	 */
	EXPORT_FLAG	WtUInt32	get_ticks_by_date(const char* stdCode, WtUInt32 uDate, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt);

	/**
	 * @brief 按数量查询Tick数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param count 查询的数据条数
	 * @param endTime 结束时间（格式：yyyymmddHHMMSS）
	 * @param cb Tick数据回调函数
	 * @param cbCnt 数据计数回调函数
	 * @return 查询到的数据条数
	 * 
	 * 查询指定结束时间之前的N条Tick数据，通过回调函数返回查询结果。
	 * 如果数据不足，返回实际可用的数据条数。
	 */
	EXPORT_FLAG	WtUInt32	get_ticks_by_count(const char* stdCode, WtUInt32 count, WtUInt64 endTime, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt);

	// ===== 实时数据订阅接口 =====

	/**
	 * @brief 订阅实时Tick数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param bReplace 是否替换现有的订阅列表（true=替换，false=追加）
	 * 
	 * 订阅指定合约的实时Tick数据。当有新Tick数据时，会通过initialize()中设置的回调函数通知。
	 * 需要配合Parser使用，确保有行情数据源。
	 */
	EXPORT_FLAG void		subscribe_tick(const char* stdCode, bool bReplace);

	/**
	 * @brief 订阅实时K线数据
	 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
	 * @param period 周期字符串（"m1"=1分钟, "m5"=5分钟, "d"=日线等）
	 * 
	 * 订阅指定合约和周期的实时K线数据。当有新K线数据生成时，会通过initialize()中设置的回调函数通知。
	 * 需要配合Parser使用，确保有行情数据源。
	 */
	EXPORT_FLAG void		subscribe_bar(const char* stdCode, const char* period);

	// ===== 缓存管理接口 =====

	/**
	 * @brief 清理数据缓存
	 * 
	 * 清理所有已缓存的数据，释放内存。
	 * 适用于需要重置数据状态或释放内存的场景。
	 */
	EXPORT_FLAG void		clear_cache();

#ifdef __cplusplus                                                              // C++环境下的extern "C"声明结束
}
#endif