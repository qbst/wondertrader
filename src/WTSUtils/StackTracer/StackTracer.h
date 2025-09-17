/*!
 * \file StackTracer.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2021/04/13
 * 
 * \brief 堆栈跟踪器头文件
 * 
 * 设计逻辑与作用：
 * 这个文件是WonderTrader项目中堆栈跟踪模块的核心头文件，主要用于在程序异常或崩溃时
 * 捕获和打印调用堆栈信息，帮助开发者快速定位问题。该模块支持跨平台操作：
 * - Windows平台：使用dbghelp.dll和StackWalker实现详细的符号解析和堆栈遍历
 * - Linux平台：使用backtrace系列函数和C++ ABI进行符号反编译
 * 主要应用场景包括异常处理、程序调试、性能分析等。
 */
#pragma once

// 平台相关的头文件包含
#ifdef _WIN32
#include <cstdio>        // C标准输入输出库
#include <Windows.h>     // Windows API核心头文件
#include <Psapi.h>       // Windows进程状态API，用于获取进程和模块信息
#else
#include <stdio.h>       // POSIX标准输入输出库
#endif // _WINDOWS

#include <functional>    // C++11函数对象库，用于回调函数定义

/**
 * @brief 堆栈跟踪日志回调函数类型定义
 * 
 * 用于接收堆栈跟踪过程中产生的日志信息，允许用户自定义日志输出方式。
 * 参数：const char* - 要输出的日志字符串
 */
typedef std::function<void(const char*)> TracerLogCallback;

/**
 * @brief 退出处理器回调函数类型定义
 * 
 * 用于在程序异常退出时执行清理操作。
 * 参数：int - 退出码
 */
typedef std::function<void(int)> ExitHandler;

/**
 * @brief 打印调用堆栈跟踪信息
 * 
 * 核心函数，用于捕获当前线程的调用堆栈并通过回调函数输出。
 * 该函数会根据不同的平台和编译器选择相应的实现方式：
 * - Windows + MSVC：使用StackWalker类进行详细的符号解析
 * - Windows + GCC：输出提示信息（功能受限）
 * - Linux/Unix：使用backtrace和符号反编译
 * 
 * @param cb 日志输出回调函数，用于接收格式化的堆栈信息
 */
void print_stack_trace(TracerLogCallback cb);

