/**
 * @file SpinMutex.hpp
 * @brief 自旋锁和自旋互斥量实现
 * 
 * 该文件提供了高性能的自旋锁机制，主要包括：
 * 1. SpinMutex类：基于原子操作的自旋互斥量实现
 * 2. SpinLock类：RAII风格的自旋锁包装器
 * 3. 跨平台的CPU暂停指令支持，优化自旋等待性能
 * 4. 基于内存序的内存同步机制
 * 
 * 设计逻辑：
 * - 使用std::atomic<bool>实现无锁的互斥量
 * - 通过自旋等待避免线程阻塞和上下文切换
 * - 使用CPU暂停指令优化自旋等待，减少CPU功耗
 * - 通过内存序控制确保正确的内存同步
 * - 提供RAII风格的锁管理，确保资源的自动释放
 * 
 * 主要作用：
 * - 为WonderTrader框架提供高性能的同步机制
 * - 适用于短时间持有的锁，避免线程阻塞开销
 * - 支持高频交易等对性能要求极高的场景
 * - 提供比传统互斥量更低的延迟
 */

#pragma once  // 防止头文件重复包含
#include <atomic>  // 包含原子操作支持
#ifdef _MSC_VER  // Microsoft Visual C++编译器
#define WIN32_LEAN_AND_MEAN  // 减少Windows头文件包含，提高编译速度
#include <windows.h>  // Windows API头文件
#endif

/**
 * @class SpinMutex
 * @brief 自旋互斥量类
 * 
 * 该类实现了基于原子操作的自旋互斥量，通过自旋等待实现线程同步。
 * 适用于锁持有时间很短的场景，避免了传统互斥量的线程阻塞开销。
 * 使用内存序控制确保正确的内存同步和可见性。
 */
class SpinMutex
{
private:
	std::atomic<bool> flag = { false };  // 原子布尔标志，false表示未锁定，true表示已锁定

public:
	/**
	 * @brief 获取锁
	 * 
	 * 该函数通过自旋等待获取锁。如果锁已被占用，会持续自旋等待直到获得锁。
	 * 使用exchange操作确保原子性，通过内存序控制保证正确的内存同步。
	 * 
	 * 注意：该函数会一直自旋直到获得锁，适用于锁持有时间很短的场景。
	 */
	void lock()
	{
		for (;;)  // 无限循环，直到成功获得锁
		{
			if (!flag.exchange(true, std::memory_order_acquire))  // 尝试将标志设置为true，如果原值为false则成功
				break;  // 成功获得锁，跳出循环

			while (flag.load(std::memory_order_relaxed))  // 等待锁被释放，使用relaxed内存序减少开销
			{
#ifdef _MSC_VER  // Windows平台
				_mm_pause();  // 使用Intel SSE指令暂停CPU，优化自旋等待
#else  // Linux/Unix平台
				__builtin_ia32_pause();  // 使用GCC内置函数暂停CPU，优化自旋等待
#endif
			}
		}
	}

	/**
	 * @brief 释放锁
	 * 
	 * 该函数释放当前持有的锁，允许其他线程获取锁。
	 * 使用release内存序确保锁释放操作对其他线程可见。
	 */
	void unlock()
	{
		flag.store(false, std::memory_order_release);  // 将标志设置为false，表示锁已释放
	}
};

/**
 * @class SpinLock
 * @brief 自旋锁RAII包装器类
 * 
 * 该类提供了RAII风格的自旋锁管理，在构造时自动获取锁，
 * 在析构时自动释放锁，确保锁的正确管理。
 * 禁止拷贝构造和赋值操作，防止意外的锁管理问题。
 */
class SpinLock
{
public:
	/**
	 * @brief 构造函数，自动获取锁
	 * @param mtx 要管理的自旋互斥量引用
	 * 
	 * 在构造时自动调用mutex的lock函数获取锁。
	 * 使用引用确保对同一个互斥量进行操作。
	 */
	SpinLock(SpinMutex& mtx) :_mutex(mtx) { _mutex.lock(); }  // 构造时自动获取锁

	/**
	 * @brief 删除拷贝构造函数
	 * 
	 * 禁止拷贝构造，防止多个SpinLock对象管理同一个锁，
	 * 避免重复加锁或提前释放锁的问题。
	 */
	SpinLock(const SpinLock&) = delete;  // 删除拷贝构造函数

	/**
	 * @brief 删除赋值操作符
	 * 
	 * 禁止赋值操作，防止多个SpinLock对象管理同一个锁，
	 * 确保锁管理的唯一性和安全性。
	 */
	SpinLock& operator=(const SpinLock&) = delete;  // 删除赋值操作符

	/**
	 * @brief 析构函数，自动释放锁
	 * 
	 * 在析构时自动调用mutex的unlock函数释放锁。
	 * 确保即使发生异常，锁也能被正确释放。
	 */
	~SpinLock() { _mutex.unlock(); }  // 析构时自动释放锁

private:
	SpinMutex&	_mutex;  // 自旋互斥量的引用，用于锁的管理
};
