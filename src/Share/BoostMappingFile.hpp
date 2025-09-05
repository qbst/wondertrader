/*!
 * \file BoostMappingFile.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief boost的内存映射文件组件的封装,方便使用
 * 
 * 文件设计逻辑与作用总结：
 * 本文件封装了boost::interprocess库的内存映射文件功能，提供了高效的文件访问机制。
 * 主要功能包括：
 * 1. 将文件内容映射到内存空间，实现快速随机访问
 * 2. 支持读写模式的内存映射
 * 3. 提供内存地址和大小获取接口
 * 4. 自动资源管理和同步操作
 * 
 * 该类主要用于WonderTrader框架中需要高性能文件访问的场景，如行情数据读取、历史数据访问等。
 * 通过内存映射技术，可以避免频繁的文件I/O操作，显著提升数据访问性能。
 */
#pragma once
#include <boost/filesystem.hpp>                 // boost文件系统操作头文件
#include <boost/interprocess/file_mapping.hpp>  // boost内存映射文件头文件
#include <boost/interprocess/mapped_region.hpp> // boost内存映射区域头文件

/**
 * @brief Boost内存映射文件封装类
 * 
 * 该类封装了boost::interprocess库的内存映射文件功能，提供了高效的文件访问机制。
 * 通过将文件内容映射到内存空间，实现快速随机访问，避免频繁的文件I/O操作。
 * 
 * 主要特性：
 * - 支持读写模式的内存映射
 * - 提供内存地址和大小获取接口
 * - 自动资源管理，防止内存泄漏
 * - 支持文件同步操作
 * - 跨平台兼容性
 * 
 * 适用场景：
 * - 需要频繁随机访问的大文件
 * - 行情数据、历史数据等结构化数据读取
 * - 对性能要求较高的文件操作
 */
class BoostMappingFile
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化内存映射对象和映射区域指针为NULL，确保对象创建时处于安全状态
	 */
	BoostMappingFile()
	{
		_file_map=NULL;                         // 初始化文件映射对象指针为NULL
		_map_region=NULL;                       // 初始化映射区域指针为NULL
	}

	/**
	 * @brief 析构函数
	 * 
	 * 自动释放内存映射资源，实现RAII资源管理，防止内存泄漏
	 */
	~BoostMappingFile()
	{
		close();                                // 自动关闭内存映射
	}

	/**
	 * @brief 关闭内存映射
	 * 
	 * 释放映射区域和文件映射对象，并将指针设置为NULL
	 * 防止重复释放和悬空指针访问
	 */
	void close()
	{
		if(_map_region!=NULL)                   // 检查映射区域指针是否有效
			delete _map_region;                 // 释放映射区域对象

		if(_file_map!=NULL)                     // 检查文件映射对象指针是否有效
			delete _file_map;                   // 释放文件映射对象

		_file_map=NULL;                         // 将文件映射对象指针设置为NULL
		_map_region=NULL;                       // 将映射区域指针设置为NULL
	}

	/**
	 * @brief 同步内存映射到磁盘
	 * 
	 * 将内存中的修改内容刷新到磁盘文件，确保数据持久化
	 */
	void sync()
	{
		if(_map_region)                         // 检查映射区域是否有效
			_map_region->flush();               // 调用boost接口刷新内存映射到磁盘
	}

	/**
	 * @brief 获取内存映射的起始地址
	 * 
	 * @return 内存映射的起始地址指针，如果映射无效则返回NULL
	 * 
	 * 返回映射到内存中的文件内容的起始地址，可以直接通过指针访问文件数据
	 */
	void *addr()
	{
		if(_map_region)                         // 检查映射区域是否有效
			return _map_region->get_address();  // 返回映射区域的起始地址
		return NULL;                            // 如果映射无效，返回NULL
	}

	/**
	 * @brief 获取内存映射的大小
	 * 
	 * @return 内存映射的大小（字节），如果映射无效则返回0
	 * 
	 * 返回映射到内存中的文件内容的大小，用于边界检查和内存访问控制
	 */
	size_t size()
	{
		if(_map_region)                         // 检查映射区域是否有效
			return _map_region->get_size();     // 返回映射区域的大小
		return 0;                               // 如果映射无效，返回0
	}

	/**
	 * @brief 创建内存映射
	 * 
	 * @param filename 要映射的文件名
	 * @param mode 文件打开模式，默认为读写模式
	 * @param mapmode 内存映射模式，默认为读写模式
	 * @param zeroother 是否将其他进程的映射区域清零，默认为true
	 * @return true 映射成功，false 映射失败
	 * 
	 * 将指定文件映射到内存空间，支持不同的打开和映射模式。
	 * 如果文件不存在则返回false，映射成功后可以通过addr()和size()访问数据。
	 */
	bool map(const char *filename,
		int mode=boost::interprocess::read_write,
		int mapmode=boost::interprocess::read_write,bool zeroother=true)
	{
		if (!boost::filesystem::exists(filename))  // 检查文件是否存在
		{
			return false;                       // 如果文件不存在，返回false
		}
		_file_name = filename;                  // 保存文件名

		_file_map = new boost::interprocess::file_mapping(filename,(boost::interprocess::mode_t)mode);  // 创建文件映射对象
		if(_file_map==NULL)                     // 检查文件映射对象是否创建成功
			return false;                       // 如果创建失败，返回false

		_map_region = new boost::interprocess::mapped_region(*_file_map,(boost::interprocess::mode_t)mapmode);  // 创建内存映射区域
		if(_map_region==NULL)                   // 检查映射区域是否创建成功
		{
			delete _file_map;                   // 如果创建失败，释放文件映射对象
			return false;                       // 返回false
		}

		return true;                            // 映射成功，返回true
	}

	/**
	 * @brief 获取映射的文件名
	 * 
	 * @return 当前映射的文件名
	 * 
	 * 返回当前内存映射对应的文件名，用于调试和日志记录
	 */
	const char* filename()
	{
		return _file_name.c_str();              // 返回文件名字符串
	}

	/**
	 * @brief 检查内存映射是否有效
	 * 
	 * @return true 映射有效，false 映射无效
	 * 
	 * 用于判断内存映射操作是否可以进行，以及数据访问是否安全
	 */
	bool valid() const
	{
		return _file_map != NULL;               // 检查文件映射对象指针是否有效
	}

private:
	std::string _file_name;                    // 存储映射的文件名
	boost::interprocess::file_mapping *_file_map;  // 文件映射对象指针，boost::interprocess库的文件映射类型
	boost::interprocess::mapped_region *_map_region;  // 内存映射区域指针，boost::interprocess库的映射区域类型
};

