/*!
 * \file BoostFile.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief boost库文件操作的辅助对象
 * 
 * 文件设计逻辑与作用总结：
 * 本文件封装了boost::interprocess库的文件操作功能，提供了跨平台的统一文件操作接口。
 * 主要功能包括：
 * 1. 文件创建、打开、关闭等基本操作
 * 2. 文件读写操作，支持二进制和字符串数据
 * 3. 文件指针定位和文件大小获取
 * 4. 跨平台兼容性处理（Windows/Linux）
 * 5. 静态工具方法，如文件内容读写、目录创建等
 * 
 * 该类主要用于WonderTrader框架中的文件I/O操作，为数据存储、日志记录等提供底层支持。
 */
#pragma once
#include <boost/version.hpp>                    // boost版本信息头文件
#include <boost/shared_ptr.hpp>                 // boost智能指针头文件
#include <boost/filesystem.hpp>                 // boost文件系统操作头文件
#include <boost/interprocess/detail/os_file_functions.hpp>  // boost进程间通信文件操作函数头文件
#include <string>                               // 标准字符串头文件

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN                    // 定义精简Windows头文件，减少编译时间
#include <windows.h>                           // Windows API头文件
#else
#include <unistd.h>                            // Unix标准头文件
#endif

//struct OVERLAPPED;                           // 注释掉的Windows异步I/O结构体声明
//extern "C" __declspec(dllimport) int __stdcall ReadFile(void *hnd, void *buffer, unsigned long bytes_to_write,unsigned long *bytes_written, OVERLAPPED* overlapped);  // 注释掉的Windows ReadFile函数声明

/**
 * @brief Boost文件操作封装类
 * 
 * 该类封装了boost::interprocess库的文件操作功能，提供了统一的跨平台文件操作接口。
 * 支持文件的创建、打开、读写、定位等操作，并提供了静态工具方法。
 * 
 * 主要特性：
 * - 跨平台兼容（Windows/Linux）
 * - RAII资源管理，自动关闭文件
 * - 支持二进制和字符串数据读写
 * - 提供文件指针定位和大小获取功能
 * - 包含静态工具方法，无需实例化即可使用
 */
class BoostFile
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化文件句柄为无效值，确保对象创建时处于安全状态
	 */
	BoostFile()
	{
		_handle=boost::interprocess::ipcdetail::invalid_file();  // 将文件句柄初始化为无效值
	}
	
	/**
	 * @brief 析构函数
	 * 
	 * 自动关闭文件，实现RAII资源管理，防止文件句柄泄漏
	 */
	~BoostFile()
	{
		close_file();                           // 自动关闭文件
	}

	/**
	 * @brief 创建新文件
	 * 
	 * @param name 文件名
	 * @param mode 文件打开模式，默认为读写模式
	 * @param temporary 是否为临时文件，默认为false
	 * @return true 创建成功，false 创建失败
	 * 
	 * 创建新文件并截断为0字节大小，如果文件已存在则会被覆盖
	 */
	bool create_new_file(const char *name, boost::interprocess::mode_t mode = boost::interprocess::read_write, bool temporary = false)
	{
		_handle=boost::interprocess::ipcdetail::create_or_open_file(name,mode,boost::interprocess::permissions(),temporary);  // 创建或打开文件

		if (valid())                            // 检查文件句柄是否有效
			return truncate_file(0);            // 如果有效，截断文件为0字节并返回结果
		return false;                           // 如果无效，返回false
	}

	/**
	 * @brief 创建或打开文件
	 * 
	 * @param name 文件名
	 * @param mode 文件打开模式，默认为读写模式
	 * @param temporary 是否为临时文件，默认为false
	 * @return true 操作成功，false 操作失败
	 * 
	 * 如果文件不存在则创建，如果存在则打开，不会覆盖现有文件内容
	 */
	bool create_or_open_file(const char *name, boost::interprocess::mode_t mode = boost::interprocess::read_write, bool temporary = false)
	{
		_handle=boost::interprocess::ipcdetail::create_or_open_file(name,mode,boost::interprocess::permissions(),temporary);  // 创建或打开文件

		return valid();                         // 返回文件句柄是否有效
	}

	/**
	 * @brief 打开已存在的文件
	 * 
	 * @param name 文件名
	 * @param mode 文件打开模式，默认为读写模式
	 * @param temporary 是否为临时文件，默认为false
	 * @return true 打开成功，false 打开失败
	 * 
	 * 只能打开已存在的文件，如果文件不存在则返回false
	 */
	bool open_existing_file(const char *name, boost::interprocess::mode_t mode = boost::interprocess::read_write, bool temporary = false)
	{
		_handle=boost::interprocess::ipcdetail::open_existing_file(name,mode,temporary);  // 打开已存在的文件
		return valid();                         // 返回文件句柄是否有效
	}

	/**
	 * @brief 检查文件句柄是否无效
	 * 
	 * @return true 文件句柄无效，false 文件句柄有效
	 * 
	 * 用于判断文件操作是否可以进行
	 */
	bool is_invalid_file()
	{  
		return _handle==boost::interprocess::ipcdetail::invalid_file();  // 比较文件句柄是否为无效值
	}

	/**
	 * @brief 检查文件句柄是否有效
	 * 
	 * @return true 文件句柄有效，false 文件句柄无效
	 * 
	 * 与is_invalid_file()相反，用于判断文件操作是否可以进行
	 */
	bool valid()
	{
		return _handle!=boost::interprocess::ipcdetail::invalid_file();  // 比较文件句柄是否不为无效值
	}

	/**
	 * @brief 关闭文件
	 * 
	 * 关闭文件句柄并将句柄设置为无效值，防止重复关闭
	 */
	void close_file()
	{
		if(!is_invalid_file())                  // 检查文件句柄是否有效
		{
			boost::interprocess::ipcdetail::close_file(_handle);  // 关闭文件
			_handle=boost::interprocess::ipcdetail::invalid_file();  // 将句柄设置为无效值
		}
	}

	/**
	 * @brief 截断文件到指定大小
	 * 
	 * @param size 目标文件大小（字节）
	 * @return true 截断成功，false 截断失败
	 * 
	 * 将文件大小调整为指定值，如果新大小小于原大小则截断，如果大于则扩展
	 */
	bool truncate_file (std::size_t size)
	{
		return boost::interprocess::ipcdetail::truncate_file(_handle,size);  // 调用boost接口截断文件
	}

	/**
	 * @brief 获取文件大小（引用参数版本）
	 * 
	 * @param size 输出参数，存储文件大小
	 * @return true 获取成功，false 获取失败
	 * 
	 * 通过引用参数返回文件大小，便于错误处理
	 */
	bool get_file_size(boost::interprocess::offset_t &size)
	{
		return boost::interprocess::ipcdetail::get_file_size(_handle,size);  // 调用boost接口获取文件大小
	}

	/**
	 * @brief 获取文件大小（返回值版本）
	 * 
	 * @return 文件大小（字节），失败时返回0
	 * 
	 * 直接返回文件大小，简化调用方式
	 */
	unsigned long long get_file_size()
	{
		boost::interprocess::offset_t size=0;   // 声明文件大小变量
		if(!get_file_size(size))                // 调用引用参数版本获取文件大小
			size=0;                             // 如果失败，设置为0
		return size;                            // 返回文件大小
	}

	/**
	 * @brief 静态方法：获取指定文件的大小
	 * 
	 * @param name 文件名
	 * @return 文件大小（字节），失败时返回0
	 * 
	 * 无需创建实例即可获取文件大小，适合一次性操作
	 */
	static unsigned long long get_file_size(const char *name)
	{
		BoostFile bf;                           // 创建临时BoostFile对象
		if (!bf.open_existing_file(name))       // 尝试打开文件
			return 0;                           // 如果打开失败，返回0

		auto ret = bf.get_file_size();          // 获取文件大小
		bf.close_file();                        // 关闭文件
		return ret;                             // 返回文件大小
	}

	/**
	 * @brief 设置文件指针位置
	 * 
	 * @param off 偏移量
	 * @param pos 定位方式（文件开始、当前位置、文件结尾）
	 * @return true 设置成功，false 设置失败
	 * 
	 * 用于文件读写前的定位操作
	 */
	bool set_file_pointer(boost::interprocess::offset_t off, boost::interprocess::file_pos_t pos)
	{
		return boost::interprocess::ipcdetail::set_file_pointer(_handle,off,pos);  // 调用boost接口设置文件指针
	}

	/**
	 * @brief 定位到文件开始位置
	 * 
	 * @param offsize 相对文件开始的偏移量，默认为0
	 * @return true 定位成功，false 定位失败
	 * 
	 * 将文件指针定位到文件开始位置加上指定偏移量的位置
	 */
	bool seek_to_begin(int offsize=0)
	{
		return set_file_pointer(offsize,boost::interprocess::file_begin);  // 调用通用定位方法，定位到文件开始
	}

	/**
	 * @brief 定位到当前位置
	 * 
	 * @param offsize 相对当前位置的偏移量，默认为0
	 * @return true 定位成功，false 定位失败
	 * 
	 * 将文件指针从当前位置移动指定偏移量
	 */
	bool seek_current(int offsize=0)
	{
		return set_file_pointer(offsize,boost::interprocess::file_current);  // 调用通用定位方法，定位到当前位置
	}

	/**
	 * @brief 定位到文件结尾
	 * 
	 * @param offsize 相对文件结尾的偏移量，默认为0
	 * @return true 定位成功，false 定位失败
	 * 
	 * 将文件指针定位到文件结尾位置加上指定偏移量的位置
	 */
	bool seek_to_end(int offsize=0)
	{
		return set_file_pointer(offsize,boost::interprocess::file_end);  // 调用通用定位方法，定位到文件结尾
	}

	/**
	 * @brief 获取当前文件指针位置（引用参数版本）
	 * 
	 * @param off 输出参数，存储当前文件指针位置
	 * @return true 获取成功，false 获取失败
	 * 
	 * 通过引用参数返回当前文件指针位置
	 */
	bool get_file_pointer(boost::interprocess::offset_t &off)
	{
		return boost::interprocess::ipcdetail::get_file_pointer(_handle,off);  // 调用boost接口获取文件指针位置
	}

	/**
	 * @brief 获取当前文件指针位置（返回值版本）
	 * 
	 * @return 当前文件指针位置（字节），失败时返回0
	 * 
	 * 直接返回当前文件指针位置
	 */
	unsigned long long get_file_pointer()
	{
		boost::interprocess::offset_t off=0;    // 声明偏移量变量
		if(!get_file_pointer(off))              // 调用引用参数版本获取文件指针位置
			return 0;                           // 如果失败，返回0
		return off;                             // 返回文件指针位置
	}

	/**
	 * @brief 写入文件数据
	 * 
	 * @param data 要写入的数据指针
	 * @param numdata 要写入的数据字节数
	 * @return true 写入成功，false 写入失败
	 * 
	 * 将指定字节数的数据写入文件
	 */
	bool write_file(const void *data, std::size_t numdata)
	{
		return boost::interprocess::ipcdetail::write_file(_handle,data,numdata);  // 调用boost接口写入文件
	}

	/**
	 * @brief 写入字符串数据到文件
	 * 
	 * @param data 要写入的字符串
	 * @return true 写入成功，false 写入失败
	 * 
	 * 将整个字符串内容写入文件
	 */
	bool write_file(const std::string& data)
	{
		return boost::interprocess::ipcdetail::write_file(_handle, data.data(), data.size());  // 调用boost接口写入字符串数据
	}

	/**
	 * @brief 读取文件数据
	 * 
	 * @param data 存储读取数据的缓冲区指针
	 * @param numdata 要读取的数据字节数
	 * @return true 读取成功且读取字节数等于请求字节数，false 读取失败或读取字节数不足
	 * 
	 * 从文件读取指定字节数的数据到缓冲区
	 */
	bool read_file(void *data, std::size_t numdata)
	{
		unsigned long readbytes = 0;            // 声明实际读取字节数变量
#ifdef _WIN32
		int ret = ReadFile(_handle, data, (DWORD)numdata, &readbytes, NULL);  // Windows平台调用ReadFile API
#else
		readbytes = read(_handle, data, (std::size_t)numdata);  // Linux平台调用read系统调用
#endif
		return numdata == readbytes;            // 返回是否读取到请求的字节数
	}

	/**
	 * @brief 读取文件数据并返回实际读取字节数
	 * 
	 * @param data 存储读取数据的缓冲区指针
	 * @param numdata 要读取的数据字节数
	 * @return 实际读取的字节数
	 * 
	 * 从文件读取数据并返回实际读取的字节数，便于处理文件末尾等特殊情况
	 */
	int read_file_length(void *data, std::size_t numdata)
	{
		unsigned long readbytes = 0;            // 声明实际读取字节数变量
#ifdef _WIN32
		int ret = ReadFile(_handle, data, (DWORD)numdata, &readbytes, NULL);  // Windows平台调用ReadFile API
#else
		readbytes = read(_handle, data, (std::size_t)numdata);  // Linux平台调用read系统调用
#endif
		return readbytes;                       // 返回实际读取的字节数
	}

private:
	boost::interprocess::file_handle_t _handle;  // 文件句柄，boost::interprocess库的文件句柄类型

public:
	/**
	 * @brief 静态方法：删除文件
	 * 
	 * @param name 要删除的文件名
	 * @return true 删除成功，false 删除失败
	 * 
	 * 删除指定的文件，无需创建BoostFile实例
	 */
	static bool delete_file(const char *name)
	{
		return boost::interprocess::ipcdetail::delete_file(name);  // 调用boost接口删除文件
	}

	/**
	 * @brief 静态方法：读取文件全部内容到字符串
	 * 
	 * @param filename 文件名
	 * @param buffer 输出参数，存储文件内容的字符串
	 * @return true 读取成功，false 读取失败
	 * 
	 * 一次性读取整个文件内容到字符串缓冲区，适合小文件操作
	 */
	static bool read_file_contents(const char *filename,std::string &buffer)
	{
		BoostFile bf;                           // 创建临时BoostFile对象
		if(!bf.open_existing_file(filename,boost::interprocess::read_only))  // 以只读模式打开文件
			return false;                       // 如果打开失败，返回false
		unsigned int filesize=(unsigned int)bf.get_file_size();  // 获取文件大小
		if(filesize==0)                         // 检查文件是否为空
			return false;                       // 如果文件为空，返回false
		buffer.resize(filesize);                // 调整字符串缓冲区大小
		return bf.read_file((void *)buffer.c_str(),filesize);  // 读取文件内容到缓冲区
	}

	/**
	 * @brief 静态方法：将数据写入文件
	 * 
	 * @param filename 文件名
	 * @param pdata 要写入的数据指针
	 * @param datalen 数据长度（字节）
	 * @return true 写入成功，false 写入失败
	 * 
	 * 创建新文件并写入指定数据，如果文件已存在则覆盖
	 */
	static bool write_file_contents(const char *filename,const void *pdata,uint32_t datalen)
	{
		BoostFile bf;                           // 创建临时BoostFile对象
		if(!bf.create_new_file(filename))       // 创建新文件
			return false;                       // 如果创建失败，返回false
		return bf.write_file(pdata,datalen);    // 写入数据并返回结果
	}

	/**
	 * @brief 静态方法：创建目录
	 * 
	 * @param name 目录名
	 * @return true 创建成功或目录已存在，false 创建失败
	 * 
	 * 创建单个目录，如果目录已存在则返回true
	 */
	static bool create_directory(const char *name)
	{
		if(exists(name))                        // 检查目录是否已存在
			return true;                        // 如果已存在，返回true

		return boost::filesystem::create_directory(boost::filesystem::path(name));  // 调用boost::filesystem创建目录
	}

	/**
	 * @brief 静态方法：创建多级目录
	 * 
	 * @param name 目录路径
	 * @return true 创建成功或目录已存在，false 创建失败
	 * 
	 * 创建多级目录结构，如果中间目录不存在会自动创建
	 */
	static bool create_directories(const char *name)
	{
		if(exists(name))                        // 检查目录是否已存在
			return true;                        // 如果已存在，返回true

		return boost::filesystem::create_directories(boost::filesystem::path(name));  // 调用boost::filesystem创建多级目录
	}

	/**
	 * @brief 静态方法：检查文件或目录是否存在
	 * 
	 * @param name 文件或目录名
	 * @return true 存在，false 不存在
	 * 
	 * 检查指定的文件或目录是否存在
	 */
	static bool exists(const char* name)
	{
		return boost::filesystem::exists(boost::filesystem::path(name));  // 调用boost::filesystem检查存在性
	}
};

typedef boost::shared_ptr<BoostFile> BoostFilePtr;  // 定义BoostFile的智能指针类型别名，用于自动内存管理