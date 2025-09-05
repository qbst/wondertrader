/*!
 * \file StdUtils.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief C++标准库一些定义的简单封装,方便调用
 * 
 * 该文件提供了C++标准库常用功能的封装和类型别名，主要包括：
 * 1. 标准线程相关类型别名和智能指针
 * 2. 标准互斥量和锁的类型别名和RAII包装器
 * 3. 文件操作辅助类的静态方法
 * 4. 跨平台的文件存在性检查
 * 
 * 设计逻辑：
 * - 通过类型别名简化标准库类型的使用
 * - 提供RAII风格的锁管理，确保资源的自动释放
 * - 封装常用的文件操作，提供统一的接口
 * - 通过条件编译实现跨平台兼容性
 * - 使用静态方法提供工具函数，无需实例化对象
 * 
 * 主要作用：
 * - 为WonderTrader框架提供标准库的便捷封装
 * - 简化线程、锁和文件操作的代码编写
 * - 提供跨平台的统一接口，减少平台差异处理
 * - 增强代码的可读性和可维护性
 */
#pragma once  // 防止头文件重复包含

#include <memory>  // 包含智能指针支持
#include <thread>  // 包含线程支持
#include <mutex>  // 包含互斥量支持
#include <condition_variable>  // 包含条件变量支持
#include <stdint.h>  // 包含固定大小整数类型
#include <string>  // 包含字符串支持

#if _MSC_VER  // Microsoft Visual C++编译器
#include <io.h>  // Windows平台的文件访问函数
#else  // 非Windows平台
#include <unistd.h>  // Unix/Linux平台的文件访问函数
#endif

//////////////////////////////////////////////////////////////////////////
//std线程类
typedef std::thread StdThread;  // 标准线程类型别名，简化类型名称
typedef std::shared_ptr<StdThread> StdThreadPtr;  // 标准线程智能指针类型别名

//////////////////////////////////////////////////////////////////////////
//std互斥量和锁
typedef std::recursive_mutex	StdRecurMutex;  // 递归互斥量类型别名，支持同一线程多次加锁
typedef std::mutex				StdUniqueMutex;  // 普通互斥量类型别名，不支持递归加锁
typedef std::condition_variable_any	StdCondVariable;  // 任意类型互斥量的条件变量类型别名

typedef std::unique_lock<StdUniqueMutex>	StdUniqueLock;  // 唯一锁类型别名，RAII风格的锁管理

/**
 * @class StdLocker
 * @brief 标准锁RAII包装器模板类
 * @tparam T 互斥量类型模板参数
 * 
 * 该类提供了RAII风格的锁管理，在构造时自动加锁，
 * 在析构时自动解锁，确保锁的正确管理。
 * 适用于需要手动控制锁生命周期的场景。
 */
template<typename T>
class StdLocker
{
public:
	/**
	 * @brief 构造函数，自动加锁
	 * @param mtx 要管理的互斥量引用
	 * 
	 * 在构造时自动调用互斥量的lock函数加锁。
	 * 保存互斥量指针，供析构时解锁使用。
	 */
	StdLocker(T& mtx)
	{
		mtx.lock();  // 自动加锁
		_mtx = &mtx;  // 保存互斥量指针
	}

	/**
	 * @brief 析构函数，自动解锁
	 * 
	 * 在析构时自动调用互斥量的unlock函数解锁。
	 * 确保即使发生异常，锁也能被正确释放。
	 */
	~StdLocker(){
		_mtx->unlock();  // 自动解锁
	}

private:
	T* _mtx;  // 互斥量指针，用于析构时解锁
};

//////////////////////////////////////////////////////////////////////////
//文件辅助类
/**
 * @class StdFile
 * @brief 标准文件操作辅助类
 * 
 * 该类提供了常用的文件操作功能，包括文件读取、写入和存在性检查。
 * 所有方法都是静态的，无需实例化对象即可使用。
 * 支持跨平台的文件操作，自动处理平台差异。
 */
class StdFile
{
public:
	/**
	 * @brief 读取文件内容到字符串
	 * @param filename 要读取的文件名
	 * @param content 输出参数，存储文件内容的字符串
	 * @return uint64_t 返回读取的字节数
	 * 
	 * 该函数以二进制模式打开文件，读取全部内容到指定的字符串中。
	 * 使用fseek和ftell获取文件大小，然后分配内存并读取内容。
	 * 
	 * 注意：该函数会覆盖content字符串的原有内容。
	 */
	static inline uint64_t read_file_content(const char* filename, std::string& content)
	{
		FILE* f = fopen(filename, "rb");  // 以二进制只读模式打开文件
		fseek(f, 0, SEEK_END);  // 将文件指针移动到文件末尾
		uint32_t length = ftell(f);  // 获取文件大小（字节数）
		content.resize(length);   // 调整字符串大小，分配足够的内存空间
		fseek(f, 0, 0);  // 将文件指针移动到文件开头
		fread((void*)content.data(), sizeof(char), length, f);  // 读取文件内容到字符串缓冲区
		fclose(f);  // 关闭文件
		return length;  // 返回读取的字节数
	}

	/**
	 * @brief 将字符串内容写入文件
	 * @param filename 目标文件名
	 * @param content 要写入的字符串内容
	 * 
	 * 该函数以二进制写入模式打开文件，将字符串内容写入文件。
	 * 如果文件已存在，会覆盖原有内容；如果不存在，会创建新文件。
	 */
	static inline void write_file_content(const char* filename, const std::string& content)
	{
		FILE* f = fopen(filename, "wb");  // 以二进制写入模式打开文件
		fwrite((void*)content.data(), sizeof(char), content.size(), f);  // 将字符串内容写入文件
		fclose(f);  // 关闭文件
	}

	/**
	 * @brief 将二进制数据写入文件
	 * @param filename 目标文件名
	 * @param data 要写入的二进制数据指针
	 * @param length 数据长度（字节数）
	 * 
	 * 该函数以二进制写入模式打开文件，将指定长度的二进制数据写入文件。
	 * 适用于写入任意类型的二进制数据，如结构体、数组等。
	 */
	static inline void write_file_content(const char* filename, const void* data, std::size_t length)
	{
		FILE* f = fopen(filename, "wb");  // 以二进制写入模式打开文件
		fwrite(data, sizeof(char), length, f);  // 将二进制数据写入文件
		fclose(f);  // 关闭文件
	}

	/**
	 * @brief 检查文件是否存在
	 * @param filename 要检查的文件名
	 * @return bool 文件存在返回true，不存在返回false
	 * 
	 * 该函数通过尝试访问文件来检查文件是否存在。
	 * 使用条件编译实现跨平台兼容性：
	 * - Windows平台使用_access函数
	 * - Unix/Linux平台使用access函数
	 */
	static inline bool exists(const char* filename)
	{
#if _WIN32  // Windows平台
		int ret = _access(filename, 0);  // 使用Windows API检查文件是否存在
#else  // Unix/Linux平台
		int ret = access(filename, 0);  // 使用Unix API检查文件是否存在
#endif
		return ret == 0;  // 返回值为0表示文件存在，非0表示不存在或无法访问
	}
};
