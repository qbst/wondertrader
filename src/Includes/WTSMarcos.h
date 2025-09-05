/*!
 * \file WTSMarcos.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader基础宏定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader系统的基础宏定义文件，定义了系统中使用的各种常量、类型别名和工具宏。
 * 
 * 主要功能：
 * 1. 系统常量定义：最大长度限制、无效值常量等
 * 2. 类型别名定义：为常用类型提供统一的命名
 * 3. 命名空间宏：定义WonderTrader的命名空间
 * 4. 平台兼容性：处理不同编译器的差异
 * 5. 工具函数：提供字符串处理等常用功能
 * 
 * 设计特点：
 * - 跨平台兼容：支持Windows和Linux等不同操作系统
 * - 性能优化：提供高效的字符串处理函数
 * - 类型安全：定义明确的类型别名和常量
 * - 命名规范：统一的命名空间和命名约定
 */
#pragma once
#include <limits.h>   // 包含各种数据类型的极限值定义
#include <string.h>   // 字符串处理函数库

#ifndef NOMINMAX
#define NOMINMAX  // 禁用Windows系统的min/max宏定义，防止与C++ STL的std::min/std::max函数冲突
#endif

/*
 * 系统常量定义区域
 * 定义了系统中使用的各种长度限制和边界值
 */
#define MAX_INSTRUMENT_LENGTH	32  // 金融工具（合约）代码的最大字符长度，如"rb2305"、"IF2303"等
#define MAX_EXCHANGE_LENGTH		16  // 交易所代码的最大字符长度，如"SHFE"、"CFFEX"等

/*
 * 类型转换工具宏
 * 提供安全的C++类型转换，比C风格转换更安全
 */
#define STATIC_CONVERT(x,T)		static_cast<T>(x)  // 静态类型转换宏，编译时进行类型检查

/*
 * 数值类型极限值定义
 * 确保在不同平台和编译器下都有一致的极限值定义
 */
#ifndef DBL_MAX
#define DBL_MAX 1.7976931348623158e+308  // double类型能表示的最大正值（IEEE 754标准）
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F        // float类型能表示的最大正值（IEEE 754标准）
#endif

/*
 * 无效值常量定义
 * 用于表示无效或未初始化的数据，系统中用作错误标识和边界检查
 * 不同编译器使用不同的实现方式以确保兼容性
 */
#ifdef _MSC_VER  // Microsoft Visual C++编译器
#define INVALID_DOUBLE		DBL_MAX      // 无效double值，使用double类型最大值表示
#define INVALID_INT32		INT_MAX      // 无效32位有符号整数，使用int类型最大值表示
#define INVALID_UINT32		UINT_MAX     // 无效32位无符号整数，使用unsigned int类型最大值表示
#define INVALID_INT64		_I64_MAX     // 无效64位有符号整数，使用MSVC特定的64位最大值
#define INVALID_UINT64		_UI64_MAX    // 无效64位无符号整数，使用MSVC特定的64位无符号最大值
#else  // GCC、Clang等其他编译器
#define INVALID_DOUBLE		1.7976931348623158e+308  // 无效double值，直接使用数值常量
#define INVALID_INT32		2147483647                // 无效32位有符号整数，2^31-1
#define INVALID_UINT32		0xffffffffUL             // 无效32位无符号整数，2^32-1（十六进制表示）
#define INVALID_INT64		9223372036854775807LL     // 无效64位有符号整数，2^63-1
#define INVALID_UINT64		0xffffffffffffffffULL    // 无效64位无符号整数，2^64-1（十六进制表示）
#endif

/*
 * NULL指针定义
 * 确保在不同编译环境下都有正确的NULL定义
 */
#ifndef NULL
#ifdef __cplusplus  // C++环境
#define NULL 0  // C++中NULL定义为整数0，符合C++11标准（推荐使用nullptr）
#else  // C语言环境
#define NULL ((void *)0)  // C语言中NULL定义为void类型的空指针
#endif
#endif

/*
 * WonderTrader命名空间宏定义
 * 提供统一的命名空间管理，避免全局命名冲突
 * wtp = WonderTrader Project的缩写
 */
#define NS_WTP_BEGIN	namespace wtp{  // 开始WonderTrader命名空间的宏定义
#define NS_WTP_END	}//namespace wtp   // 结束WonderTrader命名空间的宏定义
#define	USING_NS_WTP	using namespace wtp  // 使用WonderTrader命名空间的宏定义

/*
 * 动态链接库导出标志定义
 * 用于跨平台的符号导出，确保函数和类能被外部调用
 */
#ifndef EXPORT_FLAG
#ifdef _MSC_VER  // Windows MSVC编译器
#	define EXPORT_FLAG __declspec(dllexport)  // Windows DLL导出标记，使符号在DLL中可见
#else  // Linux/Unix GCC编译器
#	define EXPORT_FLAG __attribute__((__visibility__("default")))  // GCC符号可见性标记，使符号在共享库中可见
#endif
#endif

/*
 * 函数调用约定标志定义
 * 确保跨平台的函数调用兼容性，特别是C接口函数
 */
#ifndef PORTER_FLAG
#ifdef _MSC_VER  // Windows MSVC编译器
#	define PORTER_FLAG _cdecl  // Windows C调用约定，参数从右到左压栈，调用者清理栈
#else  // Linux/Unix编译器
#	define PORTER_FLAG          // Linux默认调用约定，通常不需要显式指定
#endif
#endif

/*
 * WonderTrader自定义类型定义
 * 提供统一的类型别名，增强代码可读性和跨平台兼容性
 */
typedef unsigned int		WtUInt32;  // 32位无符号整数类型，用于计数、索引等场景
typedef unsigned long long	WtUInt64;  // 64位无符号整数类型，用于大数值、时间戳等场景
typedef const char*			WtString;  // 常量字符串指针类型，用于只读字符串传递

/*
 * 跨平台字符串比较函数定义
 * 提供不区分大小写的字符串比较功能，统一不同操作系统的API差异
 */
#ifdef _MSC_VER  // Windows MSVC编译器
#define wt_stricmp _stricmp  // 使用Windows特有的_stricmp函数进行不区分大小写比较
#else  // Linux/Unix编译器
#define wt_stricmp strcasecmp  // 使用POSIX标准的strcasecmp函数进行不区分大小写比较
#endif

/*
 * WonderTrader自定义字符串复制函数
 * 
 * 设计历史：
 * - By Wesley @ 2022.03.17：最初设计时认为使用memcpy会比strcpy更高效
 * - By Wesley @ 2023.10.09：重新测试发现性能提升并不明显，但由于已广泛使用，暂时保留
 * 
 * 功能特点：
 * - 使用memcpy替代strcpy，避免逐字符复制的开销
 * - 自动添加字符串结束符'\0'
 * - 支持指定复制长度，提供更灵活的控制
 * - 返回实际复制的字符数
 * 
 * @param des 目标字符串缓冲区
 * @param src 源字符串
 * @param len 要复制的字符数，0表示复制整个源字符串
 * @return 实际复制的字符数（不包括结束符）
 * 
 * 使用示例：
 * char buffer[32];
 * size_t copied = wt_strcpy(buffer, "hello world");  // 复制整个字符串
 * size_t copied2 = wt_strcpy(buffer, "hello world", 5);  // 只复制前5个字符
 */
inline size_t wt_strcpy(char* des, const char* src, size_t len = 0)
{
	len = (len == 0) ? strlen(src) : len;  // 如果未指定长度，使用源字符串的长度
	memcpy(des, src, len);  // 使用memcpy进行内存块复制，比逐字符复制更高效
	des[len] = '\0';        // 添加字符串结束符
	return len;             // 返回实际复制的字符数
}