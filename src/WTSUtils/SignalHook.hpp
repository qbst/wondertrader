/*!
 * \file SignalHook.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 信号处理钩子模块
 * 
 * 设计逻辑与作用：
 * 这个文件实现了跨平台的信号处理机制，是WonderTrader系统稳定性保障的重要组件。
 * 主要功能包括：
 * 1. 信号捕获：捕获系统发出的各种信号（中断、终止、异常等）
 * 2. 异常诊断：当发生严重错误时自动打印堆栈跟踪信息
 * 3. 优雅退出：提供自定义退出处理机制，确保资源正确释放
 * 4. 跨平台支持：统一处理Windows和Linux/Unix的信号差异
 * 
 * 在量化交易系统中的重要性：
 * - 保护交易数据：确保程序异常时数据不丢失
 * - 快速故障定位：自动收集崩溃现场信息
 * - 系统监控：记录程序异常退出的详细原因
 * - 服务稳定性：提供程序异常恢复的基础机制
 */
#pragma once

#include <signal.h>                        // POSIX信号处理头文件
#include "./StackTracer/StackTracer.h"     // 堆栈跟踪功能

// 全局回调函数指针，用于信号处理时的日志输出
TracerLogCallback g_cbSignalLog = NULL;    // 信号日志回调函数
ExitHandler g_exitHandler = NULL;          // 自定义退出处理函数

/**
 * @brief 信号处理函数
 * 
 * 统一的信号处理入口，根据不同信号类型执行相应的处理逻辑。
 * 对于严重错误信号，会自动打印堆栈跟踪信息以便调试。
 * 
 * @param signum 接收到的信号编号
 */
void handle_signal(int signum)
{
	static char buf[64] = { 0 };              // 静态缓冲区，用于格式化信号信息
	memset(buf, 0, 64);                       // 清空缓冲区
	switch (signum)
	{
#ifdef _WIN32
	// Windows平台信号处理
	case SIGINT:          // 中断信号（Ctrl+C）
	case SIGBREAK:        // 中断序列信号（Ctrl+Break）
		g_cbSignalLog("app interrupted");     // 记录程序被中断
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGTERM:         // 终止信号（来自kill命令的软件终止信号）
		g_cbSignalLog("app terminated");     // 记录程序被终止
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGILL:          // 非法指令信号 - 无效的函数映像
	case SIGFPE:          // 浮点异常信号
	case SIGSEGV:         // 段错误信号
	case SIGABRT:         // 异常终止信号（由abort调用触发）
	case SIGABRT_COMPAT:  // SIGABRT兼容信号（与其他平台兼容）
		sprintf(buf, "app stopped by signal %d", signum);  // 格式化错误信息
		g_cbSignalLog(buf);                   // 记录错误信号
		print_stack_trace(g_cbSignalLog);     // 打印堆栈跟踪信息
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
#else
	// Linux/Unix平台信号处理
	case SIGURG:       // 丢弃信号 - socket上存在紧急条件
	case SIGCONT:      // 丢弃信号 - 停止后继续
	case SIGCHLD:      // 丢弃信号 - 子进程状态已改变
	case SIGIO:        // 丢弃信号 - 描述符上可进行I/O操作（参见fcntl(2)）
	case SIGWINCH:     // 丢弃信号 - 窗口大小改变
		sprintf(buf, "app discard signal %d", signum);  // 格式化丢弃信号信息
		g_cbSignalLog(buf);                   // 记录丢弃的信号
		break;
	case SIGSTOP:      // 停止进程 - 停止（无法捕获或忽略）
	case SIGTSTP:      // 停止进程 - 来自键盘的停止信号
	case SIGTTIN:      // 停止进程 - 后台进程试图从控制终端读取
	case SIGTTOU:      // 停止进程 - 后台进程试图写入控制终端
		sprintf(buf, "app stopped by signal %d", signum);  // 格式化停止信号信息
		g_cbSignalLog(buf);                   // 记录停止信号
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGINT:       // 终止进程 - 中断程序
		g_cbSignalLog("app interrupted");     // 记录程序被中断
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGTERM:      // 终止进程 - 软件终止信号
		g_cbSignalLog("app terminated");     // 记录程序被终止
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGKILL:      // 终止进程 - 杀死程序
		g_cbSignalLog("app killed");         // 记录程序被杀死
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGHUP:       // 终止进程 - 终端线路挂起
		g_cbSignalLog("app has received SIGHUP");  // 记录挂起信号
		break;
	case SIGPIPE:      // 终止进程 - 向无读者的管道写入
	case SIGALRM:      // 终止进程 - 实时定时器过期
	case SIGXCPU:      // 终止进程 - CPU时间限制超出（参见setrlimit(2)）
	case SIGXFSZ:      // 终止进程 - 文件大小限制超出（参见setrlimit(2)）
	case SIGVTALRM:    // 终止进程 - 虚拟时间闹钟（参见setitimer(2)）
	case SIGPROF:      // 终止进程 - 性能分析定时器闹钟（参见setitimer(2)）
		sprintf(buf, "app terminated by signal %d", signum);  // 格式化终止信号信息
		g_cbSignalLog(buf);                   // 记录终止信号
		print_stack_trace(g_cbSignalLog);     // 打印堆栈跟踪信息
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGUSR1:      // 终止进程 - 用户定义信号1
	case SIGUSR2:      // 终止进程 - 用户定义信号2
		sprintf(buf, "app caught user defined signal %d", signum);  // 格式化用户信号信息
		g_cbSignalLog(buf);                   // 记录用户定义信号
		print_stack_trace(g_cbSignalLog);     // 打印堆栈跟踪信息
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGQUIT:      // 创建核心映像 - 退出程序
	case SIGILL:       // 创建核心映像 - 非法指令
	case SIGTRAP:      // 创建核心映像 - 跟踪陷阱
	case SIGABRT:      // 创建核心映像 - 中止程序（原SIGIOT）
	case SIGFPE:       // 创建核心映像 - 浮点异常
	case SIGBUS:       // 创建核心映像 - 总线错误
		g_cbSignalLog("bus error");          // 记录总线错误
		print_stack_trace(g_cbSignalLog);     // 打印堆栈跟踪信息
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGSEGV:      // 创建核心映像 - 段错误
		g_cbSignalLog("segmentation violation");  // 记录段错误
		print_stack_trace(g_cbSignalLog);     // 打印堆栈跟踪信息
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
	case SIGSYS:       // 创建核心映像 - 调用了不存在的系统调用
		sprintf(buf, "app caught unexpected signal %d", signum);  // 格式化意外信号信息
		g_cbSignalLog(buf);                   // 记录意外信号
		print_stack_trace(g_cbSignalLog);     // 打印堆栈跟踪信息
		if (g_exitHandler)
			g_exitHandler(signum);            // 调用自定义退出处理器
		else
			exit(signum);                     // 默认退出处理
		break;
#endif // _WINDOWS
	default:
		sprintf(buf, "app caught unknown signal %d, signal ignored", signum);  // 格式化未知信号信息
		g_cbSignalLog(buf);                   // 记录未知信号并忽略
		break;
	}
}

/**
 * @brief 安装信号处理钩子
 * 
 * 为系统中所有可能的信号安装统一的处理函数，实现全局信号监控。
 * 这是系统启动时必须调用的初始化函数。
 * 
 * @param cbLog 信号日志回调函数，用于输出信号处理信息
 * @param sigHandler 可选的自定义退出处理函数，NULL表示使用默认处理
 */
void install_signal_hooks(TracerLogCallback cbLog, ExitHandler sigHandler = NULL)
{
	g_cbSignalLog = cbLog;                    // 设置全局日志回调函数
	g_exitHandler = sigHandler;               // 设置全局退出处理函数
	for (int s = 1; s < NSIG; s++)            // 遍历所有信号编号
	{
		signal(s, handle_signal);             // 为每个信号安装处理函数
	}
}

