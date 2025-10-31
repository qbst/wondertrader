/*!
 * \file WtDtServo.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据伺服器C接口实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WtDtServo（数据伺服器）模块的C接口具体实现，提供了数据伺服器功能的所有
 * C接口函数的实现。该文件作为C接口层，将C接口调用转发到内部的C++实现（WtDtRunner），
 * 实现了跨语言访问WonderTrader数据服务功能的目的。
 * 
 * 核心实现机制：
 * 
 * 1. C接口包装（C Interface Wrapper）：
 *    - 提供纯C接口函数，支持跨语言调用
 *    - 内部调用WtDtRunner的C++方法
 *    - 处理回调函数的传递和数据转换
 * 
 * 2. 单例模式（Singleton Pattern）：
 *    - 使用getRunner()函数获取全局唯一的WtDtRunner实例
 *    - 确保整个应用程序中只有一个数据服务实例
 *    - 简化资源管理和状态控制
 * 
 * 3. 数据切片处理（Data Slice Processing）：
 *    - 将WTSKlineSlice和WTSTickSlice转换为回调调用
 *    - 支持分批次回调，避免大内存占用
 *    - 自动处理数据切片的释放
 * 
 * 4. 版本信息管理（Version Information Management）：
 *    - 动态构建版本信息字符串
 *    - 包含平台、版本号、编译日期和时间
 *    - 使用静态变量缓存版本信息
 * 
 * 主要功能实现：
 * 
 * 1. 初始化功能：
 *    - initialize()：初始化数据伺服器，转发到WtDtRunner
 *    - get_version()：获取版本信息
 * 
 * 2. 数据查询功能：
 *    - 所有查询函数都转发到WtDtRunner的对应方法
 *    - 将返回的数据切片转换为回调调用
 *    - 支持分批次回调处理
 * 
 * 3. 实时数据订阅：
 *    - subscribe_tick()：订阅Tick数据
 *    - subscribe_bar()：订阅K线数据
 *    - clear_cache()：清理缓存
 * 
 * 使用场景：
 * - Python/C#等外部语言的数据访问
 * - 实时行情数据订阅和监控
 * - 历史数据分析和回测
 * 
 * 技术特点：
 * - 纯C接口设计，兼容性强
 * - 封装复杂的数据处理逻辑
 * - 自动管理内存和资源
 * - 支持分批次数据处理
 * 
 * 注意事项：
 * - 所有数据切片对象都需要释放
 * - 回调函数应该快速返回
 * - 分批次回调需要正确处理isLast标志
 */
#include "WtDtServo.h"                                                          // 包含数据伺服器C接口头文件
#include "WtDtRunner.h"                                                         // 包含数据服务运行器头文件

#include "../WtDtCore/WtHelper.h"                                               // 包含WtDtCore模块的辅助工具（用于获取模块目录）
#include "../WTSTools/WTSLogger.h"                                              // 包含日志工具类

#include "../Share/ModuleHelper.hpp"                                            // 包含模块辅助工具（用于获取二进制目录）
#include "../Includes/WTSVersion.h"                                             // 包含版本信息定义
#include "../Includes/WTSDataDef.hpp"                                           // 包含数据结构定义

#include <boost/filesystem.hpp>                                                  // 包含Boost文件系统库

#ifdef _MSC_VER                                                                 // 如果是Microsoft Visual C++编译器（Windows平台）
#ifdef _WIN64                                                                    // 如果是64位Windows
char PLATFORM_NAME[] = "X64";                                                   // 平台名称：X64
#else                                                                           // 如果是32位Windows
char PLATFORM_NAME[] = "X86";                                                   // 平台名称：X86
#endif
#else                                                                           // 如果是Unix/Linux平台
char PLATFORM_NAME[] = "UNIX";                                                  // 平台名称：UNIX
#endif

#ifdef _MSC_VER                                                                 // 如果是Windows平台
#include "../Common/mdump.h"                                                    // 包含Windows崩溃转储工具
/**
 * @brief 获取模块名称
 * @return 模块文件名（不含路径）
 * 
 * Windows平台专用函数，用于获取当前模块的文件名。
 * 主要用于崩溃转储文件的命名。
 */
const char* getModuleName()
{
	static char MODULE_NAME[250] = { 0 };                                       // 静态缓冲区：缓存模块名称
	if (strlen(MODULE_NAME) == 0)                                               // 如果缓存为空（首次调用）
	{
		GetModuleFileName(g_dllModule, MODULE_NAME, 250);                      // 获取当前模块的完整路径
		boost::filesystem::path p(MODULE_NAME);                                 // 创建路径对象
		strcpy(MODULE_NAME, p.filename().string().c_str());                     // 提取文件名（不含路径）
	}

	return MODULE_NAME;                                                          // 返回模块文件名
}
#endif

/**
 * @brief 获取全局数据服务运行器实例
 * @return WtDtRunner对象的引用
 * 
 * 使用单例模式获取全局唯一的WtDtRunner实例。
 * 确保整个应用程序中只有一个数据服务实例，简化资源管理。
 */
WtDtRunner& getRunner()
{
	static WtDtRunner runner;                                                   // 静态局部变量：全局唯一的数据服务运行器实例
	return runner;                                                               // 返回运行器引用
}

/**
 * @brief 初始化数据伺服器
 * @param cfgFile 配置文件路径或配置内容（取决于isFile参数）
 * @param isFile 是否为配置文件路径（true=文件路径，false=配置内容字符串）
 * @param logCfg 日志配置文件路径
 * @param cbTick 实时Tick数据回调函数（可选）
 * @param cbBar 实时K线数据回调函数（可选）
 * 
 * 初始化数据伺服器模块，将参数转发到WtDtRunner进行初始化。
 * 这是使用数据伺服器功能的第一步，必须在使用其他功能前调用。
 */
void initialize(WtString cfgFile, bool isFile, WtString logCfg, FuncOnTickCallback cbTick, FuncOnBarCallback cbBar)
{
	getRunner().initialize(cfgFile, isFile, getBinDir(), logCfg, cbTick, cbBar);  // 调用运行器的初始化方法，getBinDir()获取二进制目录路径
}

/**
 * @brief 获取数据伺服器版本信息
 * @return 版本信息字符串（包含平台、版本号、编译日期和时间）
 * 
 * 构建并返回数据伺服器的版本信息字符串。
 * 版本信息包含平台名称、版本号、编译日期和时间。
 * 使用静态变量缓存结果，避免重复构建。
 */
const char* get_version()
{
	static std::string _ver;                                                     // 静态变量：缓存版本信息字符串
	if (_ver.empty())                                                            // 如果缓存为空（首次调用）
	{
		_ver = PLATFORM_NAME;                                                    // 添加平台名称
		_ver += " ";                                                             // 添加空格分隔符
		_ver += WT_VERSION;                                                      // 添加WonderTrader版本号
		_ver += " Build@";                                                       // 添加构建标识符
		_ver += __DATE__;                                                        // 添加编译日期（编译时宏）
		_ver += " ";                                                             // 添加空格分隔符
		_ver += __TIME__;                                                        // 添加编译时间（编译时宏）
	}
	return _ver.c_str();                                                         // 返回版本信息字符串指针
}

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
 * 查询指定时间范围内的K线数据，将查询结果通过回调函数返回。
 * 支持分批次回调，通过isLast参数标识是否为最后一批数据。
 */
WtUInt32 get_bars_by_range(const char* stdCode, const char* period, WtUInt64 beginTime, WtUInt64 endTime, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt)
{
	WTSKlineSlice* kData = getRunner().get_bars_by_range(stdCode, period, beginTime, endTime);  // 调用运行器查询K线数据，返回K线数据切片
	if (kData)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = kData->size();                                         // 获取数据总条数
		cbCnt(kData->size());                                                    // 调用计数回调，通知数据总条数

		for (std::size_t i = 0; i < kData->get_block_counts(); i++)            // 遍历数据切片的所有数据块
		
		cb(kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts() - 1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识
		kData->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

/**
 * @brief 按日期查询K线数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param period 周期字符串（"m1"=1分钟, "m5"=5分钟, "d"=日线等）
 * @param uDate 交易日期（格式：yyyymmdd）
 * @param cb K线数据回调函数
 * @param cbCnt 数据计数回调函数
 * @return 查询到的数据条数
 * 
 * 查询指定交易日的所有K线数据，将查询结果通过回调函数返回。
 * 支持分批次回调，通过isLast参数标识是否为最后一批数据。
 */
WtUInt32 get_bars_by_date(const char* stdCode, const char* period, WtUInt32 uDate, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt)
{
	WTSKlineSlice* kData = getRunner().get_bars_by_date(stdCode, period, uDate);  // 调用运行器查询指定日期的K线数据，返回K线数据切片
	if (kData)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = kData->size();                                         // 获取数据总条数
		cbCnt(kData->size());                                                    // 调用计数回调，通知数据总条数

		for (std::size_t i = 0; i < kData->get_block_counts(); i++)            // 遍历数据切片的所有数据块
			cb(kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts() - 1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识

		kData->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

/**
 * @brief 按时间范围查询Tick数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param beginTime 开始时间（格式：yyyymmddHHMMSS）
 * @param endTime 结束时间（格式：yyyymmddHHMMSS）
 * @param cb Tick数据回调函数
 * @param cbCnt 数据计数回调函数
 * @return 查询到的数据条数
 * 
 * 查询指定时间范围内的Tick数据，将查询结果通过回调函数返回。
 * 支持分批次回调，通过isLast参数标识是否为最后一批数据。
 */
WtUInt32	get_ticks_by_range(const char* stdCode, WtUInt64 beginTime, WtUInt64 endTime, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt)
{
	WTSTickSlice* slice = getRunner().get_ticks_by_range(stdCode, beginTime, endTime);  // 调用运行器查询Tick数据，返回Tick数据切片
	if (slice)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = 0;                                                     // 实际数据条数累计器
		uint32_t blkCnt = slice->get_block_counts();                             // 获取数据块总数
		cbCnt(slice->size());                                                    // 调用计数回调，通知数据总条数

		for(uint32_t sIdx = 0; sIdx < blkCnt; sIdx++)                          // 遍历数据切片的所有数据块
		{
			cb(slice->get_block_addr(sIdx), slice->get_block_size(sIdx), sIdx == blkCnt - 1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识
			reaCnt += slice->get_block_size(sIdx);                               // 累加实际数据条数
		}
		
		slice->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

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
 * 查询指定结束时间之前的N条K线数据，将查询结果通过回调函数返回。
 * 如果数据不足，返回实际可用的数据条数。
 */
WtUInt32 get_bars_by_count(const char* stdCode, const char* period, WtUInt32 count, WtUInt64 endTime, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt)
{
	WTSKlineSlice* kData = getRunner().get_bars_by_count(stdCode, period, count, endTime);  // 调用运行器查询指定数量的K线数据，返回K线数据切片
	if (kData)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = kData->size();                                         // 获取数据总条数
		cbCnt(kData->size());                                                    // 调用计数回调，通知数据总条数

		for(std::size_t i = 0; i< kData->get_block_counts(); i++)              // 遍历数据切片的所有数据块
			cb(kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts()-1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识

		kData->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

/**
 * @brief 按数量查询Tick数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param count 查询的数据条数
 * @param endTime 结束时间（格式：yyyymmddHHMMSS）
 * @param cb Tick数据回调函数
 * @param cbCnt 数据计数回调函数
 * @return 查询到的数据条数
 * 
 * 查询指定结束时间之前的N条Tick数据，将查询结果通过回调函数返回。
 * 如果数据不足，返回实际可用的数据条数。
 */
WtUInt32	get_ticks_by_count(const char* stdCode, WtUInt32 count, WtUInt64 endTime, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt)
{
	WTSTickSlice* slice = getRunner().get_ticks_by_count(stdCode, count, endTime);  // 调用运行器查询指定数量的Tick数据，返回Tick数据切片
	if (slice)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = 0;                                                     // 实际数据条数累计器
		uint32_t blkCnt = slice->get_block_counts();                             // 获取数据块总数
		cbCnt(slice->size());                                                    // 调用计数回调，通知数据总条数

		for (uint32_t sIdx = 0; sIdx < blkCnt; sIdx++)                          // 遍历数据切片的所有数据块
		{
			cb(slice->get_block_addr(sIdx), slice->get_block_size(sIdx), sIdx == blkCnt - 1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识
			reaCnt += slice->get_block_size(sIdx);                               // 累加实际数据条数
		}

		slice->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

/**
 * @brief 按日期查询Tick数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param uDate 交易日期（格式：yyyymmdd）
 * @param cb Tick数据回调函数
 * @param cbCnt 数据计数回调函数
 * @return 查询到的数据条数
 * 
 * 查询指定交易日的所有Tick数据，将查询结果通过回调函数返回。
 * 支持分批次回调，通过isLast参数标识是否为最后一批数据。
 */
WtUInt32 get_ticks_by_date(const char* stdCode, WtUInt32 uDate, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt)
{
	WTSTickSlice* slice = getRunner().get_ticks_by_date(stdCode, uDate);        // 调用运行器查询指定日期的Tick数据，返回Tick数据切片
	if (slice)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = 0;                                                     // 实际数据条数累计器
		uint32_t blkCnt = slice->get_block_counts();                             // 获取数据块总数
		cbCnt(slice->size());                                                    // 调用计数回调，通知数据总条数

		for (uint32_t sIdx = 0; sIdx < blkCnt; sIdx++)                          // 遍历数据切片的所有数据块
		{
			cb(slice->get_block_addr(sIdx), slice->get_block_size(sIdx), sIdx == blkCnt - 1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识
			reaCnt += slice->get_block_size(sIdx);                               // 累加实际数据条数
		}

		slice->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

/**
 * @brief 按日期查询秒级K线数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param secs 秒数（如：60表示60秒K线）
 * @param uDate 交易日期（格式：yyyymmdd）
 * @param cb K线数据回调函数
 * @param cbCnt 数据计数回调函数
 * @return 查询到的数据条数
 * 
 * 查询指定交易日的自定义秒级K线数据，将查询结果通过回调函数返回。
 * 秒级K线是从Tick数据中实时生成的，适用于高频数据分析。
 */
WtUInt32 get_sbars_by_date(const char* stdCode, WtUInt32 secs, WtUInt32 uDate, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt)
{
	WTSKlineSlice* kData = getRunner().get_sbars_by_date(stdCode, secs, uDate);  // 调用运行器查询指定日期的秒级K线数据，返回K线数据切片
	if (kData)                                                                   // 如果查询成功，返回了数据切片
	{
		uint32_t reaCnt = kData->size();                                         // 获取数据总条数
		cbCnt(kData->size());                                                    // 调用计数回调，通知数据总条数

		for (std::size_t i = 0; i < kData->get_block_counts(); i++)            // 遍历数据切片的所有数据块
			cb(kData->get_block_addr(i), kData->get_block_size(i), i == kData->get_block_counts() - 1);  // 调用数据回调，传递每个数据块的地址、大小和是否为最后一批的标识

		kData->release();                                                        // 释放数据切片对象
		return reaCnt;                                                           // 返回查询到的数据条数
	}
	else                                                                         // 如果查询失败
	{
		return 0;                                                                // 返回0表示没有数据
	}
}

/**
 * @brief 订阅实时Tick数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param bReplace 是否替换现有的订阅列表（true=替换，false=追加）
 * 
 * 订阅指定合约的实时Tick数据。当有新Tick数据时，会通过initialize()中设置的回调函数通知。
 * 需要配合Parser使用，确保有行情数据源。
 */
void subscribe_tick(const char* stdCode, bool bReplace)
{
	getRunner().sub_tick(stdCode, bReplace);                                     // 调用运行器的订阅Tick方法
}

/**
 * @brief 订阅实时K线数据
 * @param stdCode 标准化合约代码（格式：交易所.合约代码，如SSE.600000）
 * @param period 周期字符串（"m1"=1分钟, "m5"=5分钟, "d"=日线等）
 * 
 * 订阅指定合约和周期的实时K线数据。当有新K线数据生成时，会通过initialize()中设置的回调函数通知。
 * 需要配合Parser使用，确保有行情数据源。
 */
void subscribe_bar(const char* stdCode, const char* period)
{
	getRunner().sub_bar(stdCode, period);                                        // 调用运行器的订阅K线方法
}

/**
 * @brief 清理数据缓存
 * 
 * 清理所有已缓存的数据，释放内存。
 * 适用于需要重置数据状态或释放内存的场景。
 */
void clear_cache()
{
	getRunner().clear_cache();                                                  // 调用运行器的清理缓存方法
}