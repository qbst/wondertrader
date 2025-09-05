/*!
 * \file BoostShm.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief boost共享内存组件的封装,方便使用
 * 
 * 文件设计逻辑与作用总结：
 * 本文件封装了boost::interprocess库的共享内存功能，提供了进程间数据共享的机制。
 * 主要功能包括：
 * 1. 创建和打开共享内存对象
 * 2. 将共享内存映射到进程地址空间
 * 3. 提供内存地址和大小访问接口
 * 4. 自动资源管理和异常安全
 * 
 * 该类主要用于WonderTrader框架中需要进程间通信的场景，如：
 * - 多个交易程序共享行情数据
 * - 策略程序与执行器程序之间的数据交换
 * - 实时数据的分发和共享
 * 
 * 通过共享内存技术，可以实现高效的进程间数据共享，避免网络传输开销。
 */
#pragma once

#include <boost/interprocess/shared_memory_object.hpp>  // boost共享内存对象头文件
#include <boost/interprocess/mapped_region.hpp>         // boost内存映射区域头文件

/**
 * @brief Boost共享内存封装类
 * 
 * 该类封装了boost::interprocess库的共享内存功能，提供了进程间数据共享的机制。
 * 支持创建新的共享内存对象或打开已存在的共享内存对象，并可以将其映射到进程地址空间。
 * 
 * 主要特性：
 * - 支持创建和打开共享内存对象
 * - 自动内存映射管理
 * - 异常安全的资源管理
 * - 提供内存地址和大小访问接口
 * - 跨进程数据共享
 * 
 * 适用场景：
 * - 进程间大数据量传输
 * - 实时数据共享（如行情数据）
 * - 高性能进程间通信
 * - 避免数据复制的场景
 */
class BoostShm
{
private:
	std::string	_name;                              // 共享内存对象的名称，用于标识和查找
	boost::interprocess::shared_memory_object*	_obj;   // 共享内存对象指针，boost::interprocess库的共享内存对象类型
	boost::interprocess::mapped_region *		_region; // 内存映射区域指针，boost::interprocess库的映射区域类型

public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化共享内存对象和映射区域指针为nullptr，确保对象创建时处于安全状态
	 */
	BoostShm(): _obj(nullptr), _region(nullptr){}  // 初始化所有指针为nullptr

	/**
	 * @brief 析构函数
	 * 
	 * 自动释放共享内存资源，实现RAII资源管理，防止内存泄漏
	 */
	~BoostShm()
	{
		close();                                // 自动关闭共享内存
	}

	/**
	 * @brief 关闭共享内存
	 * 
	 * 释放映射区域和共享内存对象，并将指针设置为nullptr
	 * 防止重复释放和悬空指针访问
	 */
	void close()
	{
		if (_region)                            // 检查映射区域指针是否有效
			delete _region;                     // 释放映射区域对象

		if (_obj)                               // 检查共享内存对象指针是否有效
			delete _obj;                        // 释放共享内存对象

		_obj = nullptr;                         // 将共享内存对象指针设置为nullptr
		_region = nullptr;                      // 将映射区域指针设置为nullptr
	}

	/**
	 * @brief 打开已存在的共享内存对象
	 * 
	 * @param name 共享内存对象的名称
	 * @return true 打开成功，false 打开失败
	 * 
	 * 打开指定名称的已存在共享内存对象，并将其映射到当前进程地址空间。
	 * 如果共享内存对象不存在或打开失败，则返回false。
	 * 
	 * 注意：该方法只能打开已存在的共享内存对象，不能创建新的对象。
	 */
	bool open(const char* name)
	{
		try
		{
			_obj = new boost::interprocess::shared_memory_object(boost::interprocess::open_only, name, boost::interprocess::read_write);  // 创建共享内存对象，以只打开模式
			_region = new boost::interprocess::mapped_region(*_obj, boost::interprocess::read_write);  // 创建内存映射区域，以读写模式

			return true;                        // 打开成功，返回true
		}
		catch(...)                              // 捕获所有异常
		{
			return false;                       // 如果发生异常，返回false
		}
	}

	/**
	 * @brief 创建新的共享内存对象
	 * 
	 * @param name 共享内存对象的名称
	 * @param size 共享内存的大小（字节）
	 * @return true 创建成功，false 创建失败
	 * 
	 * 创建指定名称和大小的新共享内存对象，并将其映射到当前进程地址空间。
	 * 如果同名的共享内存对象已存在，会先删除再创建。
	 * 
	 * 注意：该方法会创建新的共享内存对象，如果对象已存在会被覆盖。
	 */
	bool create(const char* name, std::size_t size)
	{
		try
		{
			boost::interprocess::shared_memory_object::remove(name);  // 先删除同名的共享内存对象（如果存在）
			_obj = new boost::interprocess::shared_memory_object(boost::interprocess::create_only, name, boost::interprocess::read_write);  // 创建新的共享内存对象
			_obj->truncate(size);               // 设置共享内存的大小
			_region = new boost::interprocess::mapped_region(*_obj, boost::interprocess::read_write);  // 创建内存映射区域

			return true;                        // 创建成功，返回true
		}
		catch (...)                             // 捕获所有异常
		{
			return false;                       // 如果发生异常，返回false
		}
	}

	/**
	 * @brief 获取共享内存的起始地址
	 * 
	 * @return 共享内存的起始地址指针，如果映射无效则返回nullptr
	 * 
	 * 返回映射到进程地址空间中的共享内存的起始地址，可以直接通过指针访问共享数据。
	 * 多个进程可以通过该地址访问同一块共享内存中的数据。
	 */
	inline void *addr()
	{
		if (_region)                            // 检查映射区域是否有效
			return _region->get_address();      // 返回映射区域的起始地址
		return nullptr;                         // 如果映射无效，返回nullptr
	}

	/**
	 * @brief 获取共享内存的大小
	 * 
	 * @return 共享内存的大小（字节），如果映射无效则返回0
	 * 
	 * 返回共享内存对象的大小，用于边界检查和内存访问控制。
	 * 该大小在创建共享内存对象时指定，所有进程访问时大小一致。
	 */
	inline size_t size()
	{
		if (_region)                            // 检查映射区域是否有效
			return _region->get_size();         // 返回映射区域的大小
		return 0;                               // 如果映射无效，返回0
	}

	/**
	 * @brief 检查共享内存是否有效
	 * 
	 * @return true 共享内存有效，false 共享内存无效
	 * 
	 * 用于判断共享内存操作是否可以进行，以及数据访问是否安全。
	 * 只有成功创建或打开共享内存对象后，该方法才返回true。
	 */
	inline bool valid() const
	{
		return _obj != nullptr;                 // 检查共享内存对象指针是否有效
	}
};