
/*!
 * \file StackTracer.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2021/04/13
 * 
 * \brief 堆栈跟踪器实现文件
 * 
 * 设计逻辑与作用：
 * 这个文件实现了跨平台的堆栈跟踪功能，是WonderTrader异常处理和调试系统的核心组件。
 * 主要功能包括：
 * 1. Windows平台实现：
 *    - MSVC编译器：使用StackWalker类和Windows调试API实现完整的符号解析
 *    - GCC编译器：提供有限的错误提示（由于工具链限制）
 * 2. Linux/Unix平台实现：
 *    - 使用backtrace()系列函数获取调用栈地址
 *    - 使用backtrace_symbols()解析符号信息
 *    - 使用__cxa_demangle()进行C++符号反编译，获得可读的函数名
 * 
 * 该模块在量化交易系统中的重要性：
 * - 快速定位策略执行异常
 * - 分析交易引擎崩溃原因  
 * - 提供详细的调试信息，提高系统稳定性
 */

#include "StackTracer.h"      // 堆栈跟踪器头文件
#include "cstdlib"            // C标准库，提供内存管理等基础功能

// Windows平台实现
#ifdef _WIN32
#	ifdef _MSC_VER
#include "StackWalker.h"      // Windows专用的堆栈遍历器
#pragma comment(lib, "psapi.lib")    // 链接进程状态API库，用于获取进程和模块信息
#pragma comment(lib, "dbghelp.lib")  // 链接调试帮助库，提供符号解析功能

/**
 * @brief Windows MSVC环境下的堆栈跟踪实现
 * 
 * 使用StackWalker类提供的高级功能来获取详细的调用栈信息，
 * 包括函数名、模块名、源文件行号等调试信息。
 * 
 * @param cb 日志输出回调函数
 */
void print_stack_trace(TracerLogCallback cb)
{
	cb("Uncaught exception");     // 输出异常提示信息
	StackWalker sw(cb);           // 创建堆栈遍历器实例，传入回调函数
	sw.ShowCallstack();           // 显示完整的调用堆栈
}
#	else //_GCC
/**
 * @brief Windows GCC环境下的堆栈跟踪实现（功能受限）
 * 
 * 由于GCC在Windows下缺少必要的调试信息和工具链支持，
 * 无法提供完整的堆栈跟踪功能，只能输出提示信息。
 * 
 * @param cb 日志输出回调函数
 */
void print_stack_trace(TracerLogCallback cb) {
	cb("Cannot print stack trace due to being build on windows with GCC");
}
#	endif
#else
// Linux/Unix平台实现
#include <cerrno>             // 错误码定义
#include <execinfo.h>         // backtrace系列函数，用于获取调用栈
#include <cxxabi.h>           // C++ ABI，用于符号反编译

/**
 * @brief Linux/Unix环境下的堆栈跟踪实现
 * 
 * 使用POSIX标准的backtrace系列函数获取调用栈地址，然后解析符号信息。
 * 对于C++符号，使用__cxa_demangle进行反编译，获得可读的函数名。
 * 
 * @param cb 日志输出回调函数
 */
void print_stack_trace(TracerLogCallback cb) {
	unsigned int max_frames = 127;                    // 最大堆栈帧数限制
	// 存储堆栈跟踪地址数据的数组
	void *addrlist[max_frames + 1];

	// 获取当前线程的堆栈地址列表
	unsigned int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));

	// 检查是否成功获取到堆栈信息
	if (addrlen == 0) {
		cb("no trace fetched");                       // 输出获取失败信息
		return;
	}

	// 将地址解析为包含"文件名(函数名+地址)"格式的字符串
	// 实际格式为：## 程序地址 函数名 + 偏移量
	// 注意：这个数组需要使用free()释放内存
	char **symbollist = backtrace_symbols(addrlist, addrlen);

	// 遍历返回的符号行，跳过前4个帧（它们是堆栈跟踪函数本身的调用）
	for (unsigned int i = 4; i < addrlen; i++) {
		char *begin_name = nullptr;                   // 函数名开始位置
		char *begin_offset = nullptr;                 // 偏移量开始位置  
		char *end_offset = nullptr;                   // 偏移量结束位置

		// 解析符号字符串格式：./module(function+0x15c) [0x8048a6d]
		for (char *p = symbollist[i]; *p; ++p) {
			if (*p == '(')                            // 找到函数名开始标记
				begin_name = p;
			else if (*p == '+')                       // 找到偏移量开始标记
				begin_offset = p;
			else if (*p == ')' && (begin_offset || begin_name))  // 找到结束标记
				end_offset = p;
		}

		// 检查是否成功解析出各个部分
		if (begin_name && end_offset && (begin_name < end_offset)) {
			*begin_name++ = '\0';                     // 分离模块名
			*end_offset++ = '\0';                     // 分离后续信息
			if (begin_offset)
				*begin_offset++ = '\0';               // 分离偏移量

			// 现在mangled name在[begin_name, begin_offset)区间内
			// 调用者偏移量在[begin_offset, end_offset)区间内
			// 使用__cxa_demangle()进行C++符号反编译

			int status = 0;                           // 反编译状态码
			size_t funcnamesize = 8192;               // 函数名缓冲区大小
			char funcname[8192];                      // 函数名缓冲区
			char *ret = abi::__cxa_demangle(begin_name, funcname, &funcnamesize, &status);
			cb(ret);                                  // 输出反编译后的函数名
			
			// 格式化输出完整的堆栈信息
			char buf[256] = { 0 };
			sprintf(buf, "%30s ( %40s  + %6s) %s", 
				symbollist[i],                        // 模块名
				status == 0 ? ret : begin_name,       // 反编译成功则用反编译结果，否则用原始名称
				begin_offset ? begin_offset : "",     // 偏移量
				end_offset);                          // 地址信息
			cb(buf);
		} else {
			// 无法解析该行？直接输出整行信息
			cb(symbollist[i]);
		}
	}
	free(symbollist);                                 // 释放符号列表内存
}
#endif // _WINDOWS
