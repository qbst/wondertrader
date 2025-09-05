/**
 * @file ObjectPool.hpp
 * @brief 对象池模板类
 * 
 * 该文件提供了基于Boost.Pool的高性能对象池实现，主要包括：
 * 1. 模板化的对象池类，支持任意类型的对象管理
 * 2. 基于Boost.Pool的内存池管理，提高内存分配效率
 * 3. 对象的构造和析构管理，支持RAII原则
 * 4. 手动内存释放功能，优化内存使用
 * 
 * 设计逻辑：
 * - 使用Boost.Pool库实现高效的内存池管理
 * - 通过模板编程支持任意类型的对象
 * - 使用placement new进行对象构造，避免额外的内存分配
 * - 提供手动内存释放接口，支持内存优化
 * - 遵循RAII原则，确保资源的正确管理
 * 
 * 主要作用：
 * - 为WonderTrader框架提供高性能的对象池管理
 * - 减少频繁的对象创建和销毁开销
 * - 提高高频交易等场景下的性能表现
 * - 支持内存使用优化和性能调优
 */

#pragma once  // 防止头文件重复包含
#include <boost/pool/pool.hpp>  // 包含Boost内存池库
#include <atomic>  // 包含原子操作支持

/**
 * @class ObjectPool
 * @brief 对象池模板类
 * @tparam T 对象类型模板参数
 * 
 * 该类提供了基于Boost.Pool的高性能对象池功能，
 * 支持任意类型对象的快速分配和回收。
 * 通过内存池技术减少内存碎片，提高分配效率。
 */
template < typename T>
class ObjectPool
{
	boost::pool<> _pool;  // Boost内存池对象，管理指定大小的内存块

public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化对象池，设置内存块大小为模板类型T的大小。
	 * 内存池会自动管理内存的分配和回收。
	 */
	ObjectPool() :_pool(sizeof(T)) {}  // 初始化内存池，块大小为T类型的大小

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * Boost.Pool会自动管理内存的释放。
	 */
	virtual ~ObjectPool() {}  // 虚析构函数，支持继承

	/**
	 * @brief 构造并返回一个新对象
	 * @return T* 成功时返回对象指针，失败时返回nullptr
	 * 
	 * 该函数从内存池中分配内存，然后使用placement new构造对象。
	 * 如果内存分配失败，返回nullptr。
	 * 
	 * 注意：返回的对象需要手动调用destroy函数进行销毁。
	 */
	T* construct()
	{
		void * mem = _pool.malloc();  // 从内存池中分配内存块
		if (!mem)  // 检查内存分配是否成功
			return nullptr;  // 分配失败时返回nullptr

		T* pobj = new(mem) T();  // 使用placement new在指定内存上构造对象
		return pobj;  // 返回构造好的对象指针
	}

	/**
	 * @brief 销毁对象并回收内存
	 * @param pobj 要销毁的对象指针
	 * 
	 * 该函数先调用对象的析构函数，然后将内存归还给内存池。
	 * 使用RAII原则确保资源的正确释放。
	 * 
	 * 注意：该函数会调用对象的析构函数，确保对象正确清理。
	 */
	void destroy(T* pobj)
	{
		pobj->~T();  // 调用对象的析构函数，清理对象资源
		_pool.free(pobj);  // 将内存归还给内存池，供后续使用
	}

	/**
	 * @brief 手动释放未使用的内存
	 * 
	 * 该函数释放内存池中未使用的内存块，减少内存占用。
	 * 适用于需要优化内存使用的场景。
	 * 
	 * 注意：释放后，已分配的对象仍然有效，但新分配可能触发新的内存分配。
	 */
	void release()
	{
		_pool.release_memory();  // 释放内存池中未使用的内存块
	}
};

