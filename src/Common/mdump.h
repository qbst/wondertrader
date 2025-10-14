/*!
 * \file mdump.h
 * \project WonderTrader
 * 
 * \brief Windows平台程序崩溃转储（MiniDump）管理器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了CMiniDumper类，是WonderTrader框架中用于Windows平台程序崩溃诊断的核心工具。
 * 该模块实现了自动捕获未处理异常并生成MiniDump文件的功能，用于事后分析程序崩溃原因。
 * 
 * 核心设计理念：
 * 
 * 1. 异常捕获机制（Exception Capture Mechanism）：
 *    - 通过Windows API SetUnhandledExceptionFilter注册全局异常处理器
 *    - 在程序发生未处理异常时自动触发转储逻辑
 *    - 生成包含崩溃现场信息的.dmp文件
 * 
 * 2. 静态单例模式（Static Singleton Pattern）：
 *    - 使用静态成员变量存储配置信息
 *    - 提供静态方法实现全局唯一的异常处理器
 *    - 通过extern声明的全局对象自动初始化
 * 
 * 3. 诊断信息管理（Diagnostic Information Management）：
 *    - 记录应用程序名称用于标识崩溃来源
 *    - 支持自定义dump文件存储路径
 *    - 生成带时间戳的文件名便于归档管理
 * 
 * 架构设计：
 * 
 *           ┌─────────────────────────────────────┐
 *           │   Windows Exception System          │
 *           │   (Windows异常系统)                 │
 *           └───────────────┬─────────────────────┘
 *                           │ 发生未处理异常
 *                           ↓
 *           ┌─────────────────────────────────────┐
 *           │   CMiniDumper::TopLevelFilter       │  <-- 异常过滤器
 *           │   (顶层异常过滤函数)                │
 *           └───────────────┬─────────────────────┘
 *                           │
 *                     ┌─────┴─────┐
 *                     │           │
 *                     ↓           ↓
 *           ┌──────────────┐  ┌────────────────┐
 *           │ 加载DBGHELP  │  │ 创建dump文件   │
 *           │  动态库      │  │ (带时间戳)     │
 *           └──────┬───────┘  └────────┬───────┘
 *                  │                   │
 *                  ↓                   ↓
 *           ┌──────────────────────────────────┐
 *           │  MiniDumpWriteDump API调用       │
 *           │  (写入崩溃信息到文件)            │
 *           └──────────────┬───────────────────┘
 *                          │
 *                          ↓
 *           ┌─────────────────────────────────┐
 *           │  生成 .dmp 文件                 │
 *           │  (可用WinDbg等工具分析)         │
 *           └─────────────────────────────────┘
 * 
 * 主要功能模块：
 * 
 * 1. 初始化接口（Enable）：
 *    - 配置应用程序名称
 *    - 设置dump文件存储路径
 *    - 注册全局异常处理器
 * 
 * 2. 异常处理流程（TopLevelFilter）：
 *    - 动态加载DBGHELP.DLL库
 *    - 调用MiniDumpWriteDump生成转储文件
 *    - 可选择启动CrashReporter.exe进行错误报告
 * 
 * 3. 动态库管理（GetDebugHelperDll）：
 *    - 动态加载系统调试辅助库
 *    - 获取MiniDumpWriteDump函数指针
 *    - 处理库加载失败的错误提示
 * 
 * 技术要点：
 * - 使用Windows SEH（Structured Exception Handling）机制
 * - 依赖DBGHELP.DLL提供的MiniDumpWriteDump API
 * - 支持Unicode字符集（TCHAR宏定义）
 * - 采用静态类设计，避免对象实例化开销
 * 
 * 使用场景：
 * - 程序发布后的崩溃诊断
 * - 生产环境的异常信息收集
 * - 开发阶段的调试辅助
 * - 自动化的错误报告系统
 * 
 * 注意事项：
 * - 仅适用于Windows平台
 * - 需要系统中存在DBGHELP.DLL
 * - Release模式下异常会导致进程退出
 * - Debug模式下会将异常传递给调试器
 * 
 * 原始来源：
 * 本代码改编自eMule/easyMule项目的崩溃转储模块
 * Copyright (C)2002-2008 VeryCD Dev Team
 * License: GNU General Public License v2
 */

#pragma once                                                    // 防止头文件重复包含（编译器指令）
#include <Windows.h>                                            // 包含Windows API核心定义
struct _EXCEPTION_POINTERS;                                     // 前向声明：Windows异常信息结构体

/*!
 * \class CMiniDumper
 * \brief Windows平台MiniDump崩溃转储管理器
 * 
 * 该类提供了一套完整的程序崩溃诊断机制，通过注册Windows全局异常过滤器，
 * 在程序发生未处理异常时自动生成包含崩溃现场信息的MiniDump文件。
 * 
 * 设计模式：
 * - 静态工具类（Static Utility Class）：所有成员都是静态的，不需要实例化
 * - 单例模式（Singleton）：通过静态成员保证全局唯一性
 * 
 * 核心功能：
 * 1. 注册Windows SEH异常过滤器
 * 2. 动态加载DBGHELP.DLL调试库
 * 3. 生成带时间戳的.dmp转储文件
 * 4. 可选的错误报告程序调用
 * 
 * 线程安全性：
 * - 静态成员变量在多线程环境下需要外部同步
 * - 异常过滤器在异常线程中执行，无并发问题
 */
class CMiniDumper
{
public:
	/*!
	 * \brief 启用MiniDump崩溃转储功能
	 * \param pszAppName 应用程序名称，用于生成dump文件名和错误提示
	 * \param bShowErrors 是否显示错误对话框（如DBGHELP.DLL加载失败）
	 * \param pszDumpPath 可选的dump文件存储路径，默认为空表示使用程序所在目录
	 * 
	 * 该函数执行以下操作：
	 * 1. 保存应用程序名称和转储路径到静态成员变量
	 * 2. 尝试加载DBGHELP.DLL并获取MiniDumpWriteDump函数指针
	 * 3. 如果成功，注册TopLevelFilter为全局未处理异常过滤器
	 * 4. 清理加载的动态库句柄
	 * 
	 * 注意：应在程序启动早期调用此函数以确保异常捕获覆盖整个生命周期
	 */
	static void Enable(LPCTSTR pszAppName, bool bShowErrors, LPCTSTR pszDumpPath = "");

private:
	static TCHAR m_szAppName[MAX_PATH];                         // 静态成员：应用程序名称（用于生成dump文件名和错误信息）
	static TCHAR m_szDumpPath[MAX_PATH];                        // 静态成员：dump文件存储路径（空表示使用程序目录）

	/*!
	 * \brief 获取调试辅助动态库句柄和MiniDumpWriteDump函数指针
	 * \param ppfnMiniDumpWriteDump 输出参数，返回MiniDumpWriteDump函数指针
	 * \param bShowErrors 是否显示加载失败的错误对话框
	 * \return 成功返回DBGHELP.DLL的模块句柄，失败返回NULL
	 * 
	 * 该函数执行以下操作：
	 * 1. 使用LoadLibrary动态加载DBGHELP.DLL
	 * 2. 使用GetProcAddress获取MiniDumpWriteDump函数地址
	 * 3. 如果加载或获取失败且bShowErrors为true，显示错误提示框
	 * 
	 * 注意：调用者负责使用FreeLibrary释放返回的模块句柄
	 */
	static HMODULE GetDebugHelperDll(FARPROC* ppfnMiniDumpWriteDump, bool bShowErrors);
	
	/*!
	 * \brief 顶层异常过滤函数（Windows SEH回调函数）
	 * \param pExceptionInfo 指向异常信息结构体的指针，包含异常代码、地址、寄存器状态等
	 * \return EXCEPTION_EXECUTE_HANDLER表示已处理异常，EXCEPTION_CONTINUE_SEARCH表示继续搜索其他处理器
	 * 
	 * 该函数是异常处理的核心，执行以下流程：
	 * 1. 重新加载DBGHELP.DLL（因为异常时可能处于不稳定状态）
	 * 2. 构建包含时间戳的dump文件完整路径
	 * 3. 创建dump文件并调用MiniDumpWriteDump写入崩溃信息
	 * 4. 尝试启动CrashReporter.exe进行错误报告
	 * 5. Release模式下调用ExitProcess(0)终止进程
	 * 6. Debug模式下返回让调试器处理异常
	 * 
	 * 注意：此函数由Windows异常系统自动调用，不应手动调用
	 */
	static LONG WINAPI TopLevelFilter(struct _EXCEPTION_POINTERS* pExceptionInfo);
};

extern CMiniDumper theCrashDumper;                              // 全局MiniDumper对象实例声明（定义在.cpp中，程序启动时自动构造）
