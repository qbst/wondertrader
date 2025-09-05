/**
 * @file fmtlib.h
 * @brief 格式化字符串工具库封装
 * 
 * 该文件提供了基于spdlog/fmt库的字符串格式化工具，主要包括：
 * 1. 字符串格式化到指定缓冲区的功能
 * 2. 线程安全的字符串格式化功能
 * 3. 对fmt库的简化封装，提供更便捷的接口
 */

 /* 相关语法
 inline
 	内联函数，编译时将函数体插入到调用处，避免函数调用开销

 template<typename... Args>
	可变参数模板，支持任意数量、任意类型的参数列表

 thread_local static
	单独 staic 创建的，是全局的，所有线程可访问的
 	而 thread_local static 确保只能被该线程自己访问，不同线程创建的是不一样
		因此既避免了在函数调用时反复进行内存分配的开销，又避免了多线程间的竞争和数据污染
 */

#pragma once  // 防止头文件重复包含

#ifndef FMT_HEADER_ONLY  // 检查是否已定义FMT_HEADER_ONLY
#define FMT_HEADER_ONLY  // 定义FMT_HEADER_ONLY宏，启用fmt库的头文件模式
#endif
#include <spdlog/fmt/bundled/format.h>  // 包含spdlog的fmt格式化库

/**
 * @namespace fmtutil
 * @brief 格式化工具命名空间
 * 
 * 该命名空间包含对fmt库的封装和扩展，提供更便捷的字符串格式化接口。
 * 所有函数都是内联的，确保编译时优化。
 */
namespace fmtutil
{
	/**
	 * @brief 将格式化结果写入指定缓冲区
	 * @tparam Args 可变参数模板，支持任意类型的参数
	 * @param buffer 目标缓冲区指针
	 * @param format 格式化字符串
	 * @param args 要格式化的参数列表
	 * @return char* 返回格式化后的字符串指针
	 * 
	 * 该函数使用fmt库将格式化结果写入指定的缓冲区，并在末尾添加字符串结束符。
	 * 适用于需要将格式化结果写入预分配缓冲区的场景。
	 */
	template<typename... Args>
	inline char* format_to(char* buffer, const char* format, const Args& ...args)
	{
		char* s = fmt::format_to(buffer, format, args...);  // 调用fmt库进行格式化，写入指定缓冲区
		s[0] = '\0';  // 在格式化结果末尾添加字符串结束符
		return s;  // 返回格式化后的字符串指针
	}

	/**
	 * @brief 使用线程本地缓冲区进行字符串格式化
	 * @tparam BUFSIZE 缓冲区大小，默认为512字节
	 * @tparam Args 可变参数模板，支持任意类型的参数
	 * @param format 格式化字符串
	 * @param args 要格式化的参数列表
	 * @return const char* 返回格式化后的字符串指针
	 * 
	 * 该函数使用线程本地的静态缓冲区进行字符串格式化，确保线程安全。
	 * 缓冲区大小可通过模板参数自定义，适用于临时字符串格式化的场景。
	 * 
	 * 注意：返回的字符串指针指向线程本地缓冲区，在下次调用前有效。
	 */
	template<int BUFSIZE=512, typename... Args>
	inline const char* format(const char* format, const Args& ...args)
	{
		thread_local static char buffer[BUFSIZE];  // 线程本地静态缓冲区，确保线程安全
		char* s = fmt::format_to(buffer, format, args...);  // 调用fmt库进行格式化，写入线程本地缓冲区
		s[0] = '\0';  // 在格式化结果末尾添加字符串结束符
		return buffer;  // 返回缓冲区指针
	}
}  // 命名空间结束
