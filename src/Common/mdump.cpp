/*!
 * \file mdump.cpp
 * \project WonderTrader
 * 
 * \brief Windows平台程序崩溃转储（MiniDump）管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了CMiniDumper类的所有功能，包括异常过滤器注册、调试库加载、
 * dump文件生成等核心逻辑。通过Windows SEH机制捕获未处理异常，自动生成
 * 包含崩溃现场信息的MiniDump文件，为程序崩溃诊断提供关键数据支持。
 * 
 * 核心实现机制：
 * 
 * 1. 异常处理流程（Exception Handling Flow）：
 *    程序崩溃 → TopLevelFilter被调用 → 加载DBGHELP.DLL → 
 *    生成dump文件名 → MiniDumpWriteDump写入 → 启动错误报告程序 → 
 *    进程退出（Release）或传递给调试器（Debug）
 * 
 * 2. 动态库管理（Dynamic Library Management）：
 *    - 运行时动态加载DBGHELP.DLL避免静态依赖
 *    - 通过GetProcAddress获取MiniDumpWriteDump函数指针
 *    - 使用完毕后立即释放库句柄节省资源
 * 
 * 3. 文件名生成策略（Filename Generation Strategy）：
 *    - 基于应用程序名称生成基础文件名
 *    - 将空格替换为下划线，点号替换为连字符
 *    - 附加完整时间戳（精确到秒）：YYYYMMDDHHmmSS
 *    - 添加.dmp扩展名：例如 WonderTrader_20250101123045.dmp
 * 
 * 4. 错误报告集成（Error Reporting Integration）：
 *    - 生成dump文件后自动启动CrashReporter.exe
 *    - 将dump文件路径作为命令行参数传递
 *    - 支持用户友好的错误报告界面
 * 
 * 技术实现要点：
 * 
 * 1. MiniDumpWriteDump API使用：
 *    - 传入当前进程句柄和进程ID
 *    - 传入异常信息结构体指针
 *    - 使用MiniDumpNormal类型（包含基本调用栈和寄存器信息）
 *    - 可扩展为MiniDumpWithFullMemory等更详细的类型
 * 
 * 2. 字符集兼容性：
 *    - 使用TCHAR宏定义支持ANSI/Unicode双字符集
 *    - _tcsncpy、_tcslen等T系列函数实现字符集无关
 *    - sprintf用于生成时间戳（使用窄字符）
 *    - strcat用于路径拼接（需要注意字符集转换）
 * 
 * 3. 路径处理逻辑：
 *    - 如果用户指定dump路径则使用指定路径
 *    - 否则获取程序所在目录（通过GetModuleFileName）
 *    - 使用_tcsrchr查找最后一个反斜杠定位文件名位置
 * 
 * 4. 编译模式差异：
 *    - Release模式：处理异常后调用ExitProcess(0)直接退出
 *    - Debug模式：返回EXCEPTION_CONTINUE_SEARCH让调试器接管
 *    - 通过_DEBUG预处理宏实现条件编译
 * 
 * 数据结构说明：
 * 
 * 1. MINIDUMPWRITEDUMP函数指针类型：
 *    typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(
 *        HANDLE hProcess,              // 进程句柄
 *        DWORD dwPid,                  // 进程ID
 *        HANDLE hFile,                 // dump文件句柄
 *        MINIDUMP_TYPE DumpType,       // dump类型（Normal/Full等）
 *        PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,  // 异常信息
 *        PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,  // 用户自定义流
 *        PMINIDUMP_CALLBACK_INFORMATION CallbackParam  // 回调信息
 *    );
 * 
 * 2. _MINIDUMP_EXCEPTION_INFORMATION结构体：
 *    - ThreadId：发生异常的线程ID
 *    - ExceptionPointers：异常指针（包含异常记录和上下文）
 *    - ClientPointers：是否为客户端进程指针（调试用）
 * 
 * 错误处理策略：
 * - DBGHELP.DLL加载失败：显示错误对话框并返回NULL
 * - MiniDumpWriteDump函数不存在：提示需要升级DBGHELP.DLL
 * - dump文件创建失败：通过GetLastError获取错误码并提示
 * - dump文件写入失败：记录失败原因并通知用户
 * 
 * 使用示例：
 * 
 * ```cpp
 * // 在程序main函数开始处调用
 * CMiniDumper::Enable(_T("MyApplication"), true, _T("C:\\Logs\\"));
 * 
 * // 程序崩溃时会自动生成：
 * // C:\Logs\MyApplication20250101123045.dmp
 * ```
 * 
 * 注意事项：
 * - 异常过滤器内部不应抛出新异常
 * - dump文件可能包含敏感信息，需要妥善保管
 * - DBGHELP.DLL版本差异可能导致功能不同
 * - 某些严重错误（如栈溢出）可能无法正常生成dump
 * 
 * 调试建议：
 * - 使用WinDbg打开.dmp文件进行分析
 * - !analyze -v 命令可自动分析崩溃原因
 * - k命令查看调用栈
 * - r命令查看寄存器状态
 * 
 * 原始来源：
 * 本代码改编自eMule/easyMule项目的崩溃转储模块
 * Copyright (C)2002-2006 Merkur
 * License: GNU General Public License v2
 */

#include "mdump.h"                                              // 包含CMiniDumper类定义
#include <dbghelp.h>                                            // Windows调试辅助库（提供MiniDumpWriteDump等函数）
#include <ShellAPI.h>                                           // Shell API（提供ShellExecute启动外部程序）
#include <tchar.h>                                              // TCHAR字符类型定义（支持Unicode/ANSI）
#include <stdio.h>                                              // 标准输入输出（提供sprintf格式化函数）

#define ARRSIZE(x)	(sizeof(x)/sizeof(x[0]))                    // 宏定义：计算数组元素个数（通过总大小除以单元素大小）


/*!
 * \typedef MINIDUMPWRITEDUMP
 * \brief MiniDumpWriteDump函数指针类型定义
 * 
 * 该函数指针类型指向DBGHELP.DLL中的MiniDumpWriteDump函数，用于将进程的内存
 * 快照写入dump文件。通过函数指针方式调用可以实现运行时动态加载，避免静态链接依赖。
 * 
 * 参数说明：
 * \param hProcess 要转储的进程句柄（通常使用GetCurrentProcess()）
 * \param dwPid 要转储的进程ID（通常使用GetCurrentProcessId()）
 * \param hFile dump文件句柄（需要具有写权限）
 * \param DumpType 转储类型（MiniDumpNormal、MiniDumpWithFullMemory等）
 * \param ExceptionParam 异常信息结构体指针（包含线程ID、异常指针等）
 * \param UserStreamParam 用户自定义数据流（可选，通常传NULL）
 * \param CallbackParam 回调函数信息（可选，通常传NULL）
 * \return 成功返回TRUE，失败返回FALSE（通过GetLastError获取错误码）
 */
typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
	CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

CMiniDumper theCrashDumper;                                     // 全局MiniDumper对象实例（自动构造，确保在main函数前初始化）
TCHAR CMiniDumper::m_szAppName[MAX_PATH] = { 0 };               // 静态成员变量定义：应用程序名称（初始化为空字符串）
TCHAR CMiniDumper::m_szDumpPath[MAX_PATH] = { 0 };              // 静态成员变量定义：dump文件路径（初始化为空字符串，表示使用程序目录）

/*!
 * \brief 启用MiniDump崩溃转储功能（CMiniDumper类的核心初始化函数）
 * \param pszAppName 应用程序名称（用于生成dump文件名和错误提示标题）
 * \param bShowErrors 是否显示错误对话框（TRUE=显示加载失败提示，FALSE=静默失败）
 * \param pszDumpPath 可选的dump文件存储路径（默认为空字符串表示使用程序所在目录）
 * 
 * 函数执行流程：
 * 1. 将应用程序名称复制到静态成员变量m_szAppName（用于后续生成文件名）
 * 2. 将dump路径复制到静态成员变量m_szDumpPath（空表示使用默认路径）
 * 3. 调用GetDebugHelperDll尝试加载DBGHELP.DLL并获取MiniDumpWriteDump函数指针
 * 4. 如果成功获取函数指针，调用SetUnhandledExceptionFilter注册TopLevelFilter为全局异常处理器
 * 5. 释放DBGHELP.DLL模块句柄（函数指针在异常时会重新加载库）
 * 6. 清空局部变量避免悬挂指针
 * 
 * 注意事项：
 * - 应在程序启动早期调用（main函数开始处）以覆盖整个生命周期
 * - 只应调用一次，多次调用会覆盖之前的设置
 * - bShowErrors建议在开发阶段设为TRUE，发布版本可设为FALSE
 * - pszDumpPath若指定目录需确保存在且有写权限
 */
void CMiniDumper::Enable(LPCTSTR pszAppName, bool bShowErrors, LPCTSTR pszDumpPath/* = ""*/)
{
	// 原注释：如果此断言触发，说明有两个CMiniDumper实例，这是不允许的
	// 将应用程序名称复制到静态成员变量（使用安全的字符串复制函数，防止缓冲区溢出）
	_tcsncpy(m_szAppName, pszAppName, ARRSIZE(m_szAppName));
	// 将dump文件路径复制到静态成员变量（空字符串表示使用程序所在目录）
	_tcsncpy(m_szDumpPath, pszDumpPath, ARRSIZE(m_szDumpPath));

	MINIDUMPWRITEDUMP pfnMiniDumpWriteDump = NULL;              // 定义MiniDumpWriteDump函数指针变量（初始化为NULL）
	// 调用辅助函数加载DBGHELP.DLL并获取MiniDumpWriteDump函数地址
	HMODULE hDbgHelpDll = GetDebugHelperDll((FARPROC*)&pfnMiniDumpWriteDump, bShowErrors);
	if (hDbgHelpDll)                                            // 如果成功加载调试辅助库
	{
		if (pfnMiniDumpWriteDump)                               // 如果成功获取MiniDumpWriteDump函数指针
			// 注册TopLevelFilter为Windows全局未处理异常过滤器（SEH机制的核心设置）
			SetUnhandledExceptionFilter(TopLevelFilter);
		FreeLibrary(hDbgHelpDll);                               // 释放动态库句柄（异常时会重新加载，此处释放节省资源）
		hDbgHelpDll = NULL;                                     // 将句柄置空，避免悬挂指针
		pfnMiniDumpWriteDump = NULL;                            // 将函数指针置空，避免悬挂指针
	}
}

/*!
 * \brief 获取调试辅助动态库句柄和MiniDumpWriteDump函数指针
 * \param ppfnMiniDumpWriteDump 输出参数，用于返回MiniDumpWriteDump函数指针（通过指针的指针传出）
 * \param bShowErrors 是否在加载失败时显示错误对话框（TRUE=显示，FALSE=静默）
 * \return 成功返回DBGHELP.DLL的模块句柄，失败返回NULL
 * 
 * 函数执行流程：
 * 1. 初始化输出参数为NULL（防止返回未初始化指针）
 * 2. 使用LoadLibrary动态加载DBGHELP.DLL系统库
 * 3. 如果加载失败且bShowErrors=TRUE，显示"DLL未找到"错误对话框
 * 4. 如果加载成功，使用GetProcAddress获取MiniDumpWriteDump函数地址
 * 5. 如果函数地址获取失败且bShowErrors=TRUE，显示"DLL版本过旧"错误对话框
 * 6. 返回模块句柄（无论函数指针是否获取成功）
 * 
 * 错误处理：
 * - 库加载失败：通常是系统中不存在DBGHELP.DLL（需要安装Debugging Tools for Windows）
 * - 函数获取失败：通常是DLL版本太旧不包含MiniDumpWriteDump函数（需要升级到较新版本）
 * 
 * 注意事项：
 * - 调用者负责使用FreeLibrary释放返回的模块句柄
 * - 错误提示字符串不应本地化（使用硬编码英文），因为可能在MFC初始化前调用
 * - GetProcAddress的函数名参数必须使用窄字符串（不能用_T宏）
 * - 即使函数指针获取失败，仍然返回模块句柄（调用者需要检查指针是否为NULL）
 */
HMODULE CMiniDumper::GetDebugHelperDll(FARPROC* ppfnMiniDumpWriteDump, bool bShowErrors)
{
	*ppfnMiniDumpWriteDump = NULL;                              // 首先将输出参数初始化为NULL（防止返回未初始化的指针）
	HMODULE hDll = LoadLibrary(_T("DBGHELP.DLL"));              // 尝试动态加载Windows调试辅助库DBGHELP.DLL
	if (hDll == NULL)                                           // 如果库加载失败（DLL不存在或无法加载）
	{
		if (bShowErrors) {                                      // 如果允许显示错误信息
			// 原注释：不要本地化此字符串（实际上，不要使用MFC加载它）！
			// 显示错误对话框：提示用户DBGHELP.DLL未找到，需要安装
			MessageBox(NULL, _T("DBGHELP.DLL not found. Please install a DBGHELP.DLL."), m_szAppName, MB_ICONSTOP | MB_OK);
		}
	}
	else                                                        // 如果库加载成功
	{
		// 从DLL中获取MiniDumpWriteDump函数地址（注意：函数名必须用窄字符串，不能用_T宏）
		*ppfnMiniDumpWriteDump = GetProcAddress(hDll, "MiniDumpWriteDump");
		if (*ppfnMiniDumpWriteDump == NULL)                     // 如果函数地址获取失败（DLL版本过旧，不包含该函数）
		{
			if (bShowErrors) {                                  // 如果允许显示错误信息
				// 原注释：不要本地化此字符串（实际上，不要使用MFC加载它）！
				// 显示错误对话框：提示用户DBGHELP.DLL版本过旧，需要升级到较新版本
				MessageBox(NULL, _T("DBGHELP.DLL found is too old. Please upgrade to a newer version of DBGHELP.DLL."), m_szAppName, MB_ICONSTOP | MB_OK);
			}
		}
	}
	return hDll;                                                // 返回模块句柄（可能为NULL表示加载失败，调用者需检查）
}

/*!
 * \brief 顶层异常过滤函数（Windows SEH全局异常处理回调）
 * \param pExceptionInfo 指向异常信息结构体的指针，包含异常代码、地址、线程上下文等详细信息
 * \return EXCEPTION_EXECUTE_HANDLER表示已处理异常，EXCEPTION_CONTINUE_SEARCH表示继续搜索其他异常处理器
 * 
 * 该函数是整个崩溃转储机制的核心，当程序发生未处理异常时由Windows系统自动调用。
 * 函数执行完整的dump文件生成流程，包括库加载、文件创建、数据写入、错误报告等。
 * 
 * 函数执行流程：
 * 1. 初始化返回值为EXCEPTION_CONTINUE_SEARCH（默认继续搜索其他处理器）
 * 2. 准备结果消息缓冲区（用于存储成功或失败的提示信息）
 * 3. 重新加载DBGHELP.DLL并获取MiniDumpWriteDump函数指针（异常时可能处于不稳定状态，需重新加载）
 * 4. 如果成功加载库和函数：
 *    a. 构建dump文件完整路径（使用应用名称和时间戳）
 *    b. 创建dump文件（使用CreateFile API）
 *    c. 填充异常信息结构体（包含线程ID和异常指针）
 *    d. 调用MiniDumpWriteDump写入崩溃数据
 *    e. 如果写入成功：
 *       - 生成成功提示信息
 *       - 将返回值设为EXCEPTION_EXECUTE_HANDLER（表示已处理）
 *       - 启动CrashReporter.exe进行错误报告
 *    f. 如果写入失败：生成失败提示信息（包含错误码）
 *    g. 关闭文件句柄
 * 5. 释放DBGHELP.DLL库句柄
 * 6. Release模式下：如果异常已处理，调用ExitProcess(0)终止进程
 * 7. Debug模式下：返回让调试器处理异常
 * 
 * 路径生成逻辑：
 * - 如果m_szDumpPath为空：使用GetModuleFileName获取程序所在目录
 * - 如果m_szDumpPath非空：直接使用指定路径
 * - 文件名格式：应用名称（空格→下划线，点→连字符）+ 时间戳（YYYYMMDDHHmmSS）+ .dmp
 * 
 * 错误报告机制：
 * - 成功生成dump后，尝试启动CrashReporter.exe
 * - 将dump文件完整路径作为命令行参数传递
 * - 如果CrashReporter启动失败（返回值<=32），将返回值改为EXCEPTION_CONTINUE_SEARCH
 * 
 * 编译模式差异：
 * - Release模式（_DEBUG未定义）：处理异常后直接退出进程
 * - Debug模式（_DEBUG已定义）：返回EXCEPTION_CONTINUE_SEARCH让调试器接管
 * 
 * 注意事项：
 * - 此函数在异常状态下执行，应避免复杂操作和内存分配
 * - 不应在此函数内抛出新异常，否则可能导致循环崩溃
 * - MiniDumpNormal类型包含基本信息，可改为MiniDumpWithFullMemory获取完整内存
 * - CrashReporter.exe需要与程序在同一目录或系统PATH中
 */
LONG CMiniDumper::TopLevelFilter(struct _EXCEPTION_POINTERS* pExceptionInfo)
{
	LONG lRetValue = EXCEPTION_CONTINUE_SEARCH;                 // 初始化返回值为继续搜索（默认行为：传递给其他异常处理器或调试器）
	TCHAR szResult[_MAX_PATH + 1024] = { 0 };                   // 结果消息缓冲区（用于存储成功/失败提示信息，大小为路径长度+1024字节）
	MINIDUMPWRITEDUMP pfnMiniDumpWriteDump = NULL;              // MiniDumpWriteDump函数指针（初始化为NULL）
	// 重新加载DBGHELP.DLL并获取函数指针（异常时需重新加载，bShowErrors=true显示错误）
	HMODULE hDll = GetDebugHelperDll((FARPROC*)&pfnMiniDumpWriteDump, true);
	// 原注释：ADDED by fengwen on 2006/11/15 : 使用新的发送错误报告机制。
	HINSTANCE	hInstCrashReporter = NULL;                      // CrashReporter.exe程序实例句柄（用于启动错误报告程序）

	if (hDll)                                                   // 如果成功加载DBGHELP.DLL
	{
		if (pfnMiniDumpWriteDump)                               // 如果成功获取MiniDumpWriteDump函数指针
		{
			//MessageBox(NULL,"test","test",MB_OK);            // 调试用代码（已注释）
			// 原注释：Ask user if they want to save a dump file
			// 原注释：Do *NOT* localize that string (in fact, do not use MFC to load it)!
			// 原注释：COMMENTED by fengwen on 2006/11/15 <begin> : 使用新的发送错误报告机制。
			// 原代码：弹出对话框询问用户是否创建dump文件（现已注释，改为自动创建）
			//if (MessageBox(NULL, _T("eMule crashed :-(\r\n\r\nA diagnostic file can be created which will help the author to resolve this problem. This file will be saved on your Disk (and not sent).\r\n\r\nDo you want to create this file now?"), m_szAppName, MB_ICONSTOP | MB_YESNO) == IDYES)
			// 原注释：COMMENTED by fengwen on 2006/11/15 <end> : 使用新的发送错误报告机制。
			{
				// 原注释：Create full path for DUMP file
				// ===== 第一步：构建dump文件的完整路径 =====
				TCHAR szDumpPath[_MAX_PATH] = { 0 };            // dump文件路径缓冲区（初始化为空）
				if(_tcsclen(m_szDumpPath) == 0)                 // 如果m_szDumpPath为空（未指定dump路径）
				{
					// 获取当前程序的完整路径（包含程序名）
					GetModuleFileName(NULL, szDumpPath, ARRSIZE(szDumpPath));
					// 查找路径中最后一个反斜杠（定位文件名位置）
					LPTSTR pszFileName = _tcsrchr(szDumpPath, _T('\\'));
					if (pszFileName) {                          // 如果找到反斜杠
						pszFileName++;                          // 移动到文件名开始位置
						*pszFileName = _T('\0');                // 截断字符串，只保留目录部分
					}
				}
				else                                            // 如果m_szDumpPath非空（用户指定了dump路径）
				{
					// 将用户指定的路径复制到szDumpPath
					_tcsncpy(szDumpPath, m_szDumpPath, _tcsclen(m_szDumpPath));
					szDumpPath[_tcsclen(m_szDumpPath)] = _T('\0');  // 确保字符串以NULL结尾
				}

				// 原注释：Replace spaces and dots in file name.
				// ===== 第二步：处理应用程序名称（替换特殊字符） =====
				TCHAR szBaseName[_MAX_PATH] = { 0 };            // 基础文件名缓冲区（经过字符替换处理）
				// 将应用程序名称追加到szBaseName（使用安全的字符串拼接函数）
				_tcsncat(szBaseName, m_szAppName, ARRSIZE(szBaseName) - 1);
				LPTSTR psz = szBaseName;                        // 获取字符串指针，用于遍历
				while (*psz != _T('\0')) {                      // 遍历字符串直到结尾
					if (*psz == _T('.'))                        // 如果当前字符是点号
						*psz = _T('-');                         // 替换为连字符（避免文件名中的点号混淆扩展名）
					else if (*psz == _T(' '))                   // 如果当前字符是空格
						*psz = _T('_');                         // 替换为下划线（避免文件名中的空格）
					psz++;                                      // 移动到下一个字符
				}
				// 将处理后的基础文件名追加到dump路径
				_tcsncat(szDumpPath, szBaseName, ARRSIZE(szDumpPath) - 1);
				
				// ===== 第三步：生成时间戳并附加到文件名 =====
				SYSTEMTIME curTime;                             // 系统时间结构体
				GetLocalTime(&curTime);                         // 获取当前本地时间
				char buf[64];                                   // 时间戳字符串缓冲区（使用窄字符）
				// 格式化时间戳：YYYYMMDDHHmmSS（例如：20250101123045）
				sprintf(buf, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", curTime.wYear, curTime.wMonth, curTime.wDay, curTime.wHour, curTime.wMinute, curTime.wSecond);
				strcat(szDumpPath, buf);                        // 将时间戳追加到文件路径

				// 追加.dmp扩展名（标识为MiniDump文件）
				_tcsncat(szDumpPath, _T(".dmp"), ARRSIZE(szDumpPath) - 1);

				// ===== 第四步：创建dump文件 =====
				// 创建文件句柄（写权限，共享写，总是创建新文件，普通属性）
				HANDLE hFile = CreateFile(szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile != INVALID_HANDLE_VALUE)              // 如果文件创建成功（句柄有效）
				{
					// ===== 第五步：填充异常信息结构体 =====
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo = { 0 };  // 异常信息结构体（初始化为0）
					ExInfo.ThreadId = GetCurrentThreadId();     // 设置发生异常的线程ID
					ExInfo.ExceptionPointers = pExceptionInfo;  // 设置异常指针（包含异常记录和上下文）
					ExInfo.ClientPointers = NULL;               // 设置为NULL（表示指针在当前进程地址空间）

					// ===== 第六步：调用MiniDumpWriteDump写入dump数据 =====
					// 调用函数指针：传入进程句柄、进程ID、文件句柄、dump类型、异常信息、用户流、回调
					BOOL bOK = (*pfnMiniDumpWriteDump)(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
					if (bOK)                                    // 如果dump文件写入成功
					{
						// 原注释：Do *NOT* localize that string (in fact, do not use MFC to load it)!
						// 格式化成功消息（不要本地化此字符串，避免MFC依赖）
						_sntprintf(szResult, ARRSIZE(szResult), _T("Saved dump file to \"%s\".\r\n\r\nPlease send this file together with a detailed bug report to bastet.wang@gmail.com !\r\n\r\nThank you for helping to improve Tsts."), szDumpPath);
						lRetValue = EXCEPTION_EXECUTE_HANDLER;  // 设置返回值为已处理异常

						// 原注释：ADDED by fengwen on 2006/11/15 <begin> : 使用新的发送错误报告机制。
						// ===== 第七步：启动错误报告程序 =====
						// 使用ShellExecute启动CrashReporter.exe，将dump文件路径作为参数传递
						hInstCrashReporter = ShellExecute(NULL, _T("open"), _T("CrashReporter.exe"), szDumpPath, NULL, SW_SHOW);
						if (hInstCrashReporter <= (HINSTANCE)32)  // 如果启动失败（返回值<=32表示错误）
							lRetValue = EXCEPTION_CONTINUE_SEARCH;  // 改为继续搜索其他异常处理器
						// 原注释：ADDED by fengwen on 2006/11/15 <end> : 使用新的发送错误报告机制。
					}
					else                                        // 如果dump文件写入失败
					{
						// 原注释：Do *NOT* localize that string (in fact, do not use MFC to load it)!
						// 格式化失败消息，包含错误码（通过GetLastError获取）
						_sntprintf(szResult, ARRSIZE(szResult), _T("Failed to save dump file to \"%s\".\r\n\r\nError: %u"), szDumpPath, GetLastError());
					}
					CloseHandle(hFile);                         // 关闭文件句柄
				}
				else                                            // 如果文件创建失败
				{
					// 原注释：Do *NOT* localize that string (in fact, do not use MFC to load it)!
					// 格式化失败消息，包含错误码
					_sntprintf(szResult, ARRSIZE(szResult), _T("Failed to create dump file \"%s\".\r\n\r\nError: %u"), szDumpPath, GetLastError());
				}
			}
		}
		FreeLibrary(hDll);                                      // 释放DBGHELP.DLL库句柄
		hDll = NULL;                                            // 将句柄置空
		pfnMiniDumpWriteDump = NULL;                            // 将函数指针置空
	}

	//COMMENTED by fengwen on 2006/11/15	<begin> : 使用新的发送错误报告机制。
	//if (szResult[0] != _T('\0'))
	//	MessageBox(NULL, szResult, m_szAppName, MB_ICONINFORMATION | MB_OK);
	//COMMENTED by fengwen on 2006/11/15	<end> : 使用新的发送错误报告机制。

#ifndef _DEBUG
	if (EXCEPTION_EXECUTE_HANDLER == lRetValue)		//ADDED by fengwen on 2006/11/15 : 由此filter处理了异常,才去中止进程。
	{
		// Exit the process only in release builds, so that in debug builds the exceptio is passed to a possible
		// installed debugger
		ExitProcess(0);
	}
	else
		return lRetValue;

#else

	return lRetValue;
#endif
}
