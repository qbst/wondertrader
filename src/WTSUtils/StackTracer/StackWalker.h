/*!
 * \file StackWalker.h
 * \project	WonderTrader
 *
 * \brief Windows平台专用堆栈遍历器头文件
 * 
 * 设计逻辑与作用：
 * 这个文件定义了StackWalker类，是Windows平台下功能最强大的堆栈跟踪实现。
 * 该类封装了Windows调试帮助库(dbghelp.dll)的复杂API，提供了简单易用的接口。
 * 
 * 主要功能特性：
 * 1. 符号解析：能够将内存地址转换为可读的函数名、模块名
 * 2. 源码定位：提供源文件名和行号信息（需要调试符号）
 * 3. 模块信息：显示每个函数所属的DLL或EXE模块
 * 4. 灵活配置：支持多种堆栈遍历选项和符号搜索路径
 * 5. 版本兼容：支持不同版本的Windows和Visual Studio
 * 
 * 在WonderTrader量化交易系统中的应用：
 * - 策略执行异常时的精确定位
 * - 交易引擎内存泄漏检测
 * - 性能瓶颈分析和优化
 * - 第三方库集成问题诊断
 */
#pragma once

// 仅在Microsoft Visual C++编译器下编译此代码
#if defined(_MSC_VER)

/**********************************************************************
 *
 * StackWalker.h - Windows堆栈遍历器
 * 
 * 原始项目：https://github.com/JochenKalmbach/StackWalker
 *
 * LICENSE (http://www.opensource.org/licenses/bsd-license.php)
 *
 *   Copyright (c) 2005-2009, Jochen Kalmbach
 *   All rights reserved.
 *
 *   在源代码和二进制形式中重新分发和使用，无论是否经过修改，
 *   都是被允许的，但需要满足以下条件：
 *
 *   源代码的重新分发必须保留上述版权声明、此条件列表和以下免责声明。
 *   二进制形式的重新分发必须在文档和/或其他提供的材料中
 *   复制上述版权声明、此条件列表和以下免责声明。
 *   未经特定的事先书面许可，不得使用Jochen Kalmbach的姓名
 *   或其贡献者的姓名来认可或推广从此软件派生的产品。
 *   
 *   本软件由版权持有者和贡献者"按原样"提供，不承担任何明示或暗示的保证，
 *   包括但不限于对适销性和特定用途适用性的暗示保证。
 *   在任何情况下，版权所有者或贡献者都不对任何直接、间接、偶然、特殊、
 *   惩罚性或后果性损害承担责任。
 *
 * **********************************************************************/

// #pragma once 从 _MSC_VER 1000 开始支持
// 由于我们只支持 _MSC_VER >= 1100，所以无需检查版本
#include <windows.h>      // Windows API核心头文件
#include <functional>     // C++11函数对象库

/**
 * @brief 堆栈遍历器日志回调函数类型定义
 * 
 * 用于接收StackWalker输出的日志信息，包括堆栈帧信息、错误消息等。
 */
typedef std::function<void(const char*)> WalkerLogger;

// 针对Visual Studio 2015及以上版本禁用特定警告
#if _MSC_VER >= 1900
#pragma warning(disable : 4091)  // 禁用'typedef'被忽略的警告
#endif

// 针对VC5/6的特殊定义（如果没有安装实际的PSDK）
#if _MSC_VER < 1300
typedef unsigned __int64 DWORD64, *PDWORD64;    // 64位无符号整数类型定义
#if defined(_WIN64)
typedef unsigned __int64 SIZE_T, *PSIZE_T;      // 64位平台的SIZE_T定义
#else
typedef unsigned long SIZE_T, *PSIZE_T;         // 32位平台的SIZE_T定义
#endif
#endif // _MSC_VER < 1300

class StackWalkerInternal; // 前向声明内部实现类

/**
 * @brief Windows堆栈遍历器主类
 * 
 * 这个类提供了Windows平台下强大的堆栈跟踪功能，能够：
 * 1. 遍历调用堆栈的所有帧
 * 2. 解析符号信息（函数名、模块名、源文件行号等）
 * 3. 支持多种配置选项以适应不同的调试需求
 * 4. 处理不同架构（x86、x64、IA64）的堆栈格式
 * 
 * 使用场景：
 * - 异常处理时获取详细的调用堆栈
 * - 内存泄漏检测和分析
 * - 性能分析和优化
 * - 调试复杂的多层调用问题
 */
class StackWalker
{
public:
	/**
	 * @brief 堆栈遍历选项枚举
	 * 
	 * 定义了堆栈遍历过程中要获取的信息类型，可以通过位运算组合使用。
	 */
	typedef enum StackWalkOptions
	{
		// 不获取额外信息（仅地址可用）
		RetrieveNone = 0,

		// 尝试获取符号名称
		RetrieveSymbol = 1,

		// 尝试获取该符号的行号信息
		RetrieveLine = 2,

		// 尝试获取模块信息
		RetrieveModuleInfo = 4,

		// 同时获取DLL/EXE的版本信息
		RetrieveFileVersion = 8,

		// 包含上述所有信息（详细模式）
		RetrieveVerbose = 0xF,

		// 生成"良好"的符号搜索路径
		SymBuildPath = 0x10,

		// 同时使用公共的Microsoft符号服务器
		SymUseSymSrv = 0x20,

		// 包含上述所有"Sym"相关选项
		SymAll = 0x30,

		// 包含所有选项（默认设置）
		OptionsAll = 0x3F
	} StackWalkOptions;

public:
	/**
	 * @brief StackWalker完整构造函数
	 * 
	 * 创建一个具有完整配置选项的堆栈遍历器实例。
	 * 
	 * @param logger 日志输出回调函数，用于接收堆栈信息
	 * @param options 堆栈遍历选项，默认为OptionsAll（获取所有可用信息）
	 * @param szSymPath 符号搜索路径，NULL表示使用默认路径
	 * @param dwProcessId 目标进程ID，默认为当前进程
	 * @param hProcess 目标进程句柄，默认为当前进程句柄
	 */
	StackWalker(WalkerLogger logger,
		int    options = OptionsAll, // 使用'int'类型以便组合枚举标志
		LPCSTR szSymPath = NULL,
		DWORD  dwProcessId = GetCurrentProcessId(),
		HANDLE hProcess = GetCurrentProcess());

	/**
	 * @brief StackWalker简化构造函数
	 * 
	 * 创建一个用于指定进程的堆栈遍历器实例，使用默认选项。
	 * 
	 * @param logger 日志输出回调函数
	 * @param dwProcessId 目标进程ID
	 * @param hProcess 目标进程句柄
	 */
	StackWalker(WalkerLogger logger, DWORD dwProcessId, HANDLE hProcess);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，包括符号路径内存和内部实现对象。
	 */
	virtual ~StackWalker();

public:
	/**
	 * @brief 进程内存读取例程函数指针类型
	 * 
	 * 用于自定义内存读取行为，在某些特殊情况下可能需要自定义内存读取方式。
	 * 
	 * @param hProcess 进程句柄
	 * @param qwBaseAddress 基地址
	 * @param lpBuffer 缓冲区指针
	 * @param nSize 要读取的字节数
	 * @param lpNumberOfBytesRead 实际读取的字节数
	 * @param pUserData 用户数据，在ShowCallstack中传递的可选数据
	 * @return 成功返回TRUE，失败返回FALSE
	 */
	typedef BOOL(__stdcall* PReadProcessMemoryRoutine)(
		HANDLE  hProcess,
		DWORD64 qwBaseAddress,
		PVOID   lpBuffer,
		DWORD   nSize,
		LPDWORD lpNumberOfBytesRead,
		LPVOID  pUserData // 在"ShowCallstack"中传递的可选数据
		);

	/**
	 * @brief 加载进程模块信息
	 * 
	 * 加载目标进程的所有模块信息，包括DLL和EXE文件。
	 * 这是进行符号解析的前提步骤。
	 * 
	 * @return 成功返回TRUE，失败返回FALSE
	 */
	BOOL LoadModules();

	/**
	 * @brief 显示调用堆栈信息
	 * 
	 * 核心方法，遍历并显示指定线程的完整调用堆栈。
	 * 对于每个堆栈帧，会尝试解析符号信息并通过回调函数输出。
	 * 
	 * @param hThread 目标线程句柄，默认为当前线程
	 * @param context 线程上下文，NULL表示自动获取
	 * @param readMemoryFunction 自定义内存读取函数，NULL使用默认方式
	 * @param pUserData 传递给readMemoryFunction的用户数据
	 * @return 成功返回TRUE，失败返回FALSE
	 */
	BOOL ShowCallstack(
		HANDLE                    hThread = GetCurrentThread(),
		const CONTEXT*            context = NULL,
		PReadProcessMemoryRoutine readMemoryFunction = NULL,
		LPVOID pUserData = NULL // 用于在readMemoryFunction回调中标识数据的可选参数
	);

	/**
	 * @brief 显示对象信息
	 * 
	 * 尝试获取指定内存地址处对象的符号信息。
	 * 主要用于调试内存相关问题。
	 * 
	 * @param pObject 对象指针
	 * @return 成功返回TRUE，失败返回FALSE
	 */
	BOOL ShowObject(LPVOID pObject);

#if _MSC_VER >= 1300
	// 由于某些原因，在旧版编译器中"STACKWALK_MAX_NAMELEN"必须声明为"public"
	// 从VC7开始，我们可以将其声明为"protected"
protected:
#endif
	/**
	 * @brief 符号名称最大长度常量
	 */
	enum
	{
		STACKWALK_MAX_NAMELEN = 1024  // 找到的符号的最大名称长度
	};

protected:
	/**
	 * @brief 调用栈条目结构体
	 * 
	 * 存储单个堆栈帧的所有相关信息，包括地址、符号名称、源文件信息等。
	 */
	typedef struct CallstackEntry
	{
		DWORD64 offset;                                    // 地址偏移量，如果为0表示无效条目
		CHAR    name[STACKWALK_MAX_NAMELEN];              // 符号名称
		CHAR    undName[STACKWALK_MAX_NAMELEN];           // 未修饰的符号名称（简化版）
		CHAR    undFullName[STACKWALK_MAX_NAMELEN];       // 未修饰的完整符号名称
		DWORD64 offsetFromSmybol;                         // 距离符号的偏移量
		DWORD   offsetFromLine;                           // 距离行的偏移量
		DWORD   lineNumber;                               // 源文件行号
		CHAR    lineFileName[STACKWALK_MAX_NAMELEN];      // 源文件名
		DWORD   symType;                                  // 符号类型
		LPCSTR  symTypeString;                            // 符号类型字符串
		CHAR    moduleName[STACKWALK_MAX_NAMELEN];        // 模块名称
		DWORD64 baseOfImage;                              // 映像基地址
		CHAR    loadedImageName[STACKWALK_MAX_NAMELEN];   // 已加载的映像名称
	} CallstackEntry;

	/**
	 * @brief 调用栈条目类型枚举
	 * 
	 * 用于标识当前处理的是堆栈中的哪种类型的条目。
	 */
	typedef enum CallstackEntryType
	{
		firstEntry,    // 第一个条目（堆栈顶部）
		nextEntry,     // 中间条目
		lastEntry      // 最后一个条目（堆栈底部）
	} CallstackEntryType;

	/**
	 * @brief 符号初始化回调函数
	 * 
	 * 当符号系统初始化完成时被调用，输出初始化信息。
	 * 
	 * @param szSearchPath 符号搜索路径
	 * @param symOptions 符号选项
	 * @param szUserName 用户名
	 */
	virtual void OnSymInit(LPCSTR szSearchPath, DWORD symOptions, LPCSTR szUserName);
	
	/**
	 * @brief 模块加载回调函数
	 * 
	 * 当加载一个模块时被调用，输出模块加载信息。
	 * 
	 * @param img 映像文件路径
	 * @param mod 模块名称
	 * @param baseAddr 基地址
	 * @param size 模块大小
	 * @param result 加载结果
	 * @param symType 符号类型
	 * @param pdbName PDB文件名
	 * @param fileVersion 文件版本
	 */
	virtual void OnLoadModule(LPCSTR    img,
		LPCSTR    mod,
		DWORD64   baseAddr,
		DWORD     size,
		DWORD     result,
		LPCSTR    symType,
		LPCSTR    pdbName,
		ULONGLONG fileVersion);
	
	/**
	 * @brief 调用栈条目回调函数
	 * 
	 * 对于每个堆栈帧都会调用此函数，输出格式化的堆栈信息。
	 * 
	 * @param eType 条目类型（首个、中间、最后）
	 * @param entry 调用栈条目信息
	 */
	virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry);
	
	/**
	 * @brief 调试帮助错误回调函数
	 * 
	 * 当调试帮助库发生错误时被调用，输出错误信息。
	 * 
	 * @param szFuncName 出错的函数名
	 * @param gle 最后错误码
	 * @param addr 相关地址
	 */
	virtual void OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr);

	// 成员变量
	StackWalkerInternal* m_sw;              // 内部实现对象指针
	HANDLE               m_hProcess;        // 目标进程句柄
	DWORD                m_dwProcessId;     // 目标进程ID
	BOOL                 m_modulesLoaded;   // 模块是否已加载标志
	LPSTR                m_szSymPath;       // 符号搜索路径
	WalkerLogger		 m_logger;          // 日志输出回调函数

	int m_options;                          // 堆栈遍历选项
	int m_MaxRecursionCount;                // 最大递归计数，防止无限递归

	/**
	 * @brief 内存读取静态回调函数
	 * 
	 * 默认的进程内存读取实现，使用Windows API ReadProcessMemory。
	 * 
	 * @param hProcess 进程句柄
	 * @param qwBaseAddress 基地址
	 * @param lpBuffer 缓冲区
	 * @param nSize 大小
	 * @param lpNumberOfBytesRead 实际读取字节数
	 * @return 成功返回TRUE，失败返回FALSE
	 */
	static BOOL __stdcall myReadProcMem(HANDLE  hProcess,
		DWORD64 qwBaseAddress,
		PVOID   lpBuffer,
		DWORD   nSize,
		LPDWORD lpNumberOfBytesRead);

	friend StackWalkerInternal;  // 允许内部实现类访问私有成员
}; // class StackWalker

// XP之前的系统需要"丑陋"的汇编实现
// 如果您有新的PSDK并且只为XP及更高版本编译，那么可以使用"RtlCaptureContext"
// 目前没有确定PSDK版本的定义...
// 所以我们只使用编译器版本（并假设PSDK是VS-IDE安装的那个）

// 信息：如果您愿意，可以在仅针对XP及更高版本时使用RtlCaptureContext...
//       但我目前在x64/IA64环境中使用它...
//#if defined(_M_IX86) && (_WIN32_WINNT <= 0x0500) && (_MSC_VER < 1400)

#if defined(_M_IX86)
#ifdef CURRENT_THREAD_VIA_EXCEPTION
// TODO: 以下不是一个"好的"实现，
// 因为调用堆栈仅在"__except"块中有效...
/**
 * @brief 通过异常获取当前上下文（x86平台，异常方式）
 * 
 * 这种方式通过人工触发异常来获取线程上下文，但仅在异常处理块中有效。
 * 不推荐使用这种方式，因为有潜在的稳定性问题。
 */
#define GET_CURRENT_CONTEXT_STACKWALKER_CODEPLEX(c, contextFlags)               \
  do                                                                            \
  {                                                                             \
    memset(&c, 0, sizeof(CONTEXT));                                             \
    EXCEPTION_POINTERS* pExp = NULL;                                            \
    __try                                                                       \
    {                                                                           \
      throw 0;                                                                  \
    }                                                                           \
    __except (((pExp = GetExceptionInformation()) ? EXCEPTION_EXECUTE_HANDLER   \
                                                  : EXCEPTION_EXECUTE_HANDLER)) \
    {                                                                           \
    }                                                                           \
    if (pExp != NULL)                                                           \
      memcpy(&c, pExp->ContextRecord, sizeof(CONTEXT));                         \
    c.ContextFlags = contextFlags;                                              \
  } while (0);
#else
// clang-format off
/**
 * @brief 通过内联汇编获取当前上下文（x86平台，汇编方式）
 * 
 * 使用内联汇编直接获取CPU寄存器状态，这对于堆栈遍历来说已经足够。
 * 这种方式更加高效和可靠。
 */
#define GET_CURRENT_CONTEXT_STACKWALKER_CODEPLEX(c, contextFlags) \
  do                                                              \
  {                                                               \
    memset(&c, 0, sizeof(CONTEXT));                               \
    c.ContextFlags = contextFlags;                                \
    __asm    call x                                               \
    __asm x: pop eax                                              \
    __asm    mov c.Eip, eax                                       \
    __asm    mov c.Ebp, ebp                                       \
    __asm    mov c.Esp, esp                                       \
  } while (0)
// clang-format on
#endif

#else

/**
 * @brief 使用RtlCaptureContext获取当前上下文（x64/IA64平台）
 * 
 * 对于x86（XP及更高版本）、x64和IA64平台，使用Windows提供的标准API
 * RtlCaptureContext来获取线程上下文。这是最可靠和推荐的方式。
 */
#define GET_CURRENT_CONTEXT_STACKWALKER_CODEPLEX(c, contextFlags) \
  do                                                              \
  {                                                               \
    memset(&c, 0, sizeof(CONTEXT));                               \
    c.ContextFlags = contextFlags;                                \
    RtlCaptureContext(&c);                                        \
  } while (0);
#endif

#endif //defined(_MSC_VER)
