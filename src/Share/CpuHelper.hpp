/**
 * @file CpuHelper.hpp
 * @brief CPU核心管理辅助工具类
 * 
 * 该文件提供了跨平台的CPU核心管理功能，包括：
 * 1. 获取系统CPU核心数量
 * 2. 将当前线程绑定到指定的CPU核心上
 * 
 * 设计逻辑：
 * - 使用静态方法提供全局访问点
 * - 通过条件编译实现Windows和Linux的跨平台兼容
 * - 提供线程亲和性设置，用于性能优化和负载均衡
 * - 适用于高频交易等对CPU性能要求较高的场景
 * 
 * 主要作用：
 * - 为WonderTrader框架提供CPU资源管理能力
 * - 支持线程与CPU核心的精确绑定
 * - 帮助优化多核系统的性能表现
 */

#pragma once  // 防止头文件重复包含
#include <thread>  // 包含线程相关功能

/**
 * @class CpuHelper
 * @brief CPU核心管理辅助工具类
 * 
 * 该类提供了跨平台的CPU核心管理功能，包括获取CPU核心数量和
 * 将线程绑定到指定CPU核心。主要用于性能优化和负载均衡。
 */
class CpuHelper
{
public:
	/**
	 * @brief 获取系统CPU核心数量
	 * @return uint32_t 返回系统可用的CPU核心数量
	 * 
	 * 该函数使用静态变量缓存结果，避免重复调用系统API。
	 * 基于std::thread::hardware_concurrency()实现，提供跨平台兼容性。
	 */
	static uint32_t get_cpu_cores()
	{
		static uint32_t cores = std::thread::hardware_concurrency();  // 获取硬件并发线程数（CPU核心数）
		return cores;  // 返回缓存的CPU核心数
	}

#ifdef _WIN32  // Windows平台特定代码
#include <thread>  // Windows平台下重新包含线程头文件
	/**
	 * @brief 将当前线程绑定到指定的CPU核心（Windows平台）
	 * @param i 目标CPU核心索引（从0开始）
	 * @return bool 绑定成功返回true，失败返回false
	 * 
	 * 使用Windows API SetThreadAffinityMask实现线程亲和性设置。
	 * 绑定成功后，线程将只在指定的CPU核心上运行，有助于提高缓存命中率和性能。
	 */
	static bool bind_core(uint32_t i)
	{
		uint32_t cores = get_cpu_cores();  // 获取系统CPU核心总数
		if (i >= cores)  // 检查目标核心索引是否超出范围
			return false;  // 超出范围则返回失败

		HANDLE hThread = GetCurrentThread();  // 获取当前线程句柄
		DWORD_PTR mask = SetThreadAffinityMask(hThread, (DWORD_PTR)(1 << i));  // 设置线程亲和性掩码，绑定到指定核心
		return (mask != 0);  // 返回设置是否成功（非零表示成功）
	}
#else  // Linux/Unix平台特定代码
#include <pthread.h>  // POSIX线程库
#include <sched.h>    // 调度相关功能
#include <unistd.h>   // UNIX标准定义
#include <string.h>   // 字符串处理函数
	/**
	 * @brief 将当前线程绑定到指定的CPU核心（Linux/Unix平台）
	 * @param i 目标CPU核心索引（从0开始）
	 * @return bool 绑定成功返回true，失败返回false
	 * 
	 * 使用POSIX线程库的pthread_setaffinity_np实现线程亲和性设置。
	 * 通过cpu_set_t结构体设置CPU掩码，实现精确的CPU核心绑定。
	 */
	static bool bind_core(uint32_t i)
	{
		int cores = get_cpu_cores();  // 获取系统CPU核心总数
		if (i >= cores)  // 检查目标核心索引是否超出范围
			return false;  // 超出范围则返回失败

		cpu_set_t mask;  // 定义CPU掩码结构体
		CPU_ZERO(&mask);  // 清空CPU掩码，将所有位设置为0
		CPU_SET(i, &mask);  // 设置指定CPU核心位为1，表示绑定到该核心
		return (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) >= 0);  // 设置线程亲和性，返回是否成功
	}
#endif
};