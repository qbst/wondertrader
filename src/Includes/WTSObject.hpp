/*!
 * \file WTSObject.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt基础Object定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统的基础对象类，提供了引用计数和内存管理的核心机制。
 * 
 * 主要功能：
 * 1. 基础对象类(WTSObject)：提供引用计数和自动内存管理
 * 2. 对象池模板类(WTSPoolObject)：基于对象池的高性能对象管理
 * 3. 引用计数机制：自动管理对象生命周期，避免内存泄漏
 * 4. 线程安全：支持多线程环境下的对象管理
 * 
 * 设计特点：
 * - 引用计数：通过retain/release机制管理对象生命周期
 * - 对象池：减少频繁的内存分配和释放，提高性能
 * - 线程安全：使用原子操作和自旋锁保证多线程安全
 * - 模板化：支持不同类型对象的对象池管理
 * - 自动清理：引用计数为0时自动销毁对象
 */
#pragma once
#include <stdint.h>      // 标准整数类型定义
#include <atomic>         // 原子操作支持，用于线程安全的引用计数
#include <boost/smart_ptr/detail/spinlock.hpp>  // Boost自旋锁实现

#include "WTSMarcos.h"           // WonderTrader宏定义
#include "../Share/ObjectPool.hpp"  // 对象池实现
#include "../Share/SpinMutex.hpp"    // 自旋互斥锁实现

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * WonderTrader基础对象类
 * 
 * 功能概述：
 * 提供引用计数和自动内存管理的核心机制，是WonderTrader系统中所有对象的基类。
 * 采用引用计数技术实现智能指针类似的内存管理，避免内存泄漏和野指针问题。
 * 
 * 主要特性：
 * - 自动引用计数：对象创建时引用计数为1，每次retain()增加，release()减少
 * - 自动销毁：引用计数降为0时自动delete自身
 * - 线程安全：使用原子操作保证多线程环境下的安全性
 * - 异常安全：release()操作有异常保护，确保程序稳定性
 * 
 * 使用模式：
 * 1. 对象创建后引用计数为1
 * 2. 需要持有对象时调用retain()增加引用
 * 3. 不再需要对象时调用release()减少引用
 * 4. 引用计数为0时对象自动销毁
 * 
 * 注意事项：
 * - 不要直接使用delete删除对象，应使用release()
 * - 避免循环引用导致的内存泄漏
 * - 在多线程环境中安全使用
 */
class WTSObject
{
public:
	/**
	 * 构造函数
	 * 初始化引用计数为1，表示创建对象的代码持有一个引用
	 */
	WTSObject() :m_uRefs(1){}
	
	/**
	 * 虚析构函数
	 * 支持多态销毁，确保派生类对象能够正确清理资源
	 */
	virtual ~WTSObject(){}

public:
	/**
	 * 增加引用计数
	 * 当需要持有对象的引用时调用，线程安全的原子操作
	 * 
	 * @return 增加后的新引用计数值
	 * 
	 * 使用示例：
	 * WTSObject* obj = SomeFactory::create();  // 引用计数为1
	 * obj->retain();  // 引用计数变为2
	 */
	inline uint32_t		retain(){ return m_uRefs.fetch_add(1) + 1; }

	/**
	 * 减少引用计数
	 * 当不再需要对象时调用，引用计数为0时自动销毁对象
	 * 使用异常保护确保即使在异常情况下也能正确处理
	 * 
	 * 注意：调用release()后不应再使用该对象指针，因为对象可能已被销毁
	 * 
	 * 使用示例：
	 * obj->release();  // 减少引用计数，可能触发对象销毁
	 * obj = NULL;      // 建议将指针置空，避免悬垂指针
	 */
	virtual void	release()
	{
		if (m_uRefs == 0)  // 防御性检查：如果引用计数已经为0，直接返回
			return;

		try
		{
			uint32_t cnt = m_uRefs.fetch_sub(1);  // 原子操作减少引用计数
			if (cnt == 1)  // 如果减少前引用计数为1，说明这是最后一个引用
			{
				delete this;  // 销毁当前对象，会调用虚析构函数
			}
		}
		catch(...)  // 捕获所有异常，确保release操作不会抛出异常
		{
			// 静默处理异常，保证程序稳定性
			// 在实际应用中可以考虑记录日志
		}
	}

	/**
	 * 检查是否只有一个引用
	 * 用于判断当前对象是否只被一个地方引用，常用于优化决策
	 * 
	 * @return 如果引用计数为1返回true，否则返回false
	 * 
	 * 使用场景：
	 * - 在release()时判断是否需要清理关联资源
	 * - 写时复制(Copy-on-Write)优化的判断条件
	 */
	inline bool			isSingleRefs() { return m_uRefs == 1; }

	/**
	 * 获取当前引用计数
	 * 主要用于调试和监控，了解对象的引用状况
	 * 
	 * @return 当前的引用计数值
	 */
	inline uint32_t		retainCount() { return m_uRefs; }

protected:
	volatile std::atomic<uint32_t>	m_uRefs;  // 原子引用计数器，使用volatile确保内存可见性
};

/**
 * 对象池模板类
 * 
 * 功能概述：
 * 基于对象池的高性能对象管理类，继承自WTSObject，在引用计数基础上增加对象池优化。
 * 通过复用对象实例减少频繁的内存分配和释放，显著提高高频场景下的性能。
 * 
 * 主要特性：
 * - 继承引用计数机制：保持WTSObject的所有功能
 * - 对象池优化：对象销毁时回收到池中，而不是直接delete
 * - 线程局部存储：每个线程有独立的对象池，减少锁竞争
 * - 线程安全：使用自旋锁保护对象池操作
 * - 模板化设计：支持任意类型的对象池化
 * 
 * 适用场景：
 * - 高频创建销毁的对象（如Tick数据、订单信息等）
 * - 对象大小相对固定的场景
 * - 对性能要求较高的实时系统
 * 
 * 注意事项：
 * - 使用thread_local可能导致线程销毁时的对象访问问题
 * - 对象池中的对象需要支持重复使用
 * - 不适用于大对象或生命周期很长的对象
 * 
 * @tparam T 要进行对象池化的具体类型，必须继承自WTSPoolObject<T>
 */
template<typename T>
class WTSPoolObject : public WTSObject
{
private:
	typedef ObjectPool<T> MyPool;  // 类型别名：定义当前类型的对象池类型
	MyPool*		_pool;             // 对象池指针，指向创建此对象的对象池
	SpinMutex*	_mutex;            // 自旋互斥锁指针，用于保护对象池操作

public:
	/**
	 * 构造函数
	 * 初始化对象池指针为空，实际的池指针在allocate()中设置
	 */
	WTSPoolObject():_pool(NULL){}
	
	/**
	 * 虚析构函数
	 * 支持多态销毁，但通常不会直接调用，而是通过对象池回收
	 */
	virtual ~WTSPoolObject() {}

public:
	/**
	 * 静态分配方法：从对象池中获取对象实例
	 * 
	 * 设计说明：
	 * - 使用thread_local确保每个线程有独立的对象池，提高并发性能
	 * - 使用自旋锁保护对象池操作，适合短时间持锁的场景
	 * 
	 * 已知问题（By Wesley @ 2022.06.14）：
	 * - thread_local导致线程销毁时对象池也被销毁
	 * - 如果有其他地方持有对象引用，可能导致访问已销毁的对象池
	 * - 权衡性能和安全性，当前保持thread_local设计
	 * 
	 * @return 新分配的对象指针，引用计数已初始化为1
	 * 
	 * 使用示例：
	 * WTSTickData* tick = WTSTickData::allocate();
	 * // 使用对象...
	 * tick->release();  // 对象回收到池中而不是销毁
	 */
	static T*	allocate()
	{
		/*
		 * 线程局部存储的对象池和锁
		 * 每个线程拥有独立的对象池实例，减少线程间竞争
		 */
		thread_local static MyPool		pool;   // 线程局部对象池
		thread_local static SpinMutex	mtx;    // 线程局部自旋锁

		mtx.lock();                    // 加锁保护对象池操作
		T* ret = pool.construct();     // 从对象池中构造对象
		mtx.unlock();                  // 解锁
		
		ret->_pool = &pool;            // 设置对象的池指针
		ret->_mutex = &mtx;            // 设置对象的锁指针
		return ret;                    // 返回构造的对象
	}

public:
	/**
	 * 重写release方法
	 * 当引用计数降为0时，将对象回收到对象池而不是直接销毁
	 * 这是对象池优化的核心逻辑
	 */
	virtual void release() override
	{
		if (m_uRefs == 0)  // 防御性检查：引用计数已为0
			return;

		try
		{
			uint32_t cnt = m_uRefs.fetch_sub(1);  // 原子操作减少引用计数
			if (cnt == 1)  // 如果这是最后一个引用
			{
				_mutex->lock();                    // 加锁保护对象池操作
				_pool->destroy((T*)this);         // 将对象回收到对象池（实际是重置状态）
				_mutex->unlock();                  // 解锁
			}
		}
		catch (...)  // 异常保护，确保release操作的安全性
		{
			// 静默处理异常，保证程序稳定性
		}
	}
};
NS_WTP_END
