/*!
 * \file DLLHelper.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 动态库辅助类,主要是把跨平台的差异封装起来,方便调用
 * 
 * 该文件提供了跨平台的动态链接库（DLL/SO）操作封装，主要包括：
 * 1. 动态库的加载和卸载
 * 2. 动态库中符号（函数/变量）的获取
 * 3. 跨平台模块名称的自动包装
 * 
 * 设计逻辑：
 * - 通过条件编译实现Windows和Linux/Unix的跨平台兼容
 * - 封装Windows的LoadLibrary/FreeLibrary/GetProcAddress API
 * - 封装Linux的dlopen/dlclose/dlsym API
 * - 提供统一的接口，隐藏平台差异
 * - 使用RAII原则管理动态库资源
 * 
 * 主要作用：
 * - 为WonderTrader框架提供跨平台的动态库加载能力
 * - 支持插件式架构和模块化设计
 * - 简化跨平台动态库操作的开发工作
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串处理功能

#ifdef _MSC_VER  // Microsoft Visual C++编译器
#include <wtypes.h>  // Windows类型定义头文件
typedef HMODULE		DllHandle;  // Windows平台下的动态库句柄类型
typedef void*		ProcHandle;  // Windows平台下的过程句柄类型
#else  // 非Windows平台（Linux/Unix）
#include <dlfcn.h>  // 动态链接库函数头文件
typedef void*		DllHandle;  // Linux/Unix平台下的动态库句柄类型
typedef void*		ProcHandle;  // Linux/Unix平台下的过程句柄类型
#endif

/**
 * @class DLLHelper
 * @brief 动态链接库辅助工具类
 * 
 * 该类封装了跨平台的动态库操作，包括加载、卸载、符号获取等功能。
 * 通过条件编译实现Windows和Linux/Unix的兼容性，提供统一的接口。
 */
class DLLHelper
{
public:
	/**
	 * @brief 加载动态链接库
	 * @param filename 动态库文件名（Windows下为.dll，Linux下为.so）
	 * @return DllHandle 成功时返回动态库句柄，失败时返回NULL
	 * 
	 * 该函数根据平台自动选择相应的API进行动态库加载：
	 * - Windows平台使用LoadLibrary
	 * - Linux/Unix平台使用dlopen
	 * 
	 * 注意：Linux平台下会输出加载错误信息到控制台。
	 */
	static DllHandle load_library(const char *filename)
	{
		try  // 使用异常处理确保安全性
		{
#ifdef _MSC_VER  // Windows平台
			return ::LoadLibrary(filename);  // 调用Windows API加载动态库
#else  // Linux/Unix平台
			DllHandle ret = dlopen(filename, RTLD_NOW);  // 立即加载动态库，解析所有符号
			if (ret == NULL)  // 检查加载是否成功
				printf("%s\n", dlerror());  // 输出动态库加载错误信息
			return ret;  // 返回加载结果
#endif
		}
		catch(...)  // 捕获所有异常
		{
			return NULL;  // 异常情况下返回NULL
		}
	}

	/**
	 * @brief 卸载动态链接库
	 * @param handle 要卸载的动态库句柄
	 * 
	 * 该函数根据平台自动选择相应的API进行动态库卸载：
	 * - Windows平台使用FreeLibrary
	 * - Linux/Unix平台使用dlclose
	 * 
	 * 注意：如果句柄为NULL，函数会直接返回，不执行任何操作。
	 */
	static void free_library(DllHandle handle)
	{
		if (NULL == handle)  // 检查句柄是否有效
			return;  // 无效句柄直接返回

#ifdef _MSC_VER  // Windows平台
		::FreeLibrary(handle);  // 调用Windows API卸载动态库
#else  // Linux/Unix平台
		dlclose(handle);  // 调用Linux API卸载动态库
#endif
	}

	/**
	 * @brief 获取动态库中的符号（函数或变量）地址
	 * @param handle 动态库句柄
	 * @param name 符号名称
	 * @return ProcHandle 成功时返回符号地址，失败时返回NULL
	 * 
	 * 该函数根据平台自动选择相应的API进行符号获取：
	 * - Windows平台使用GetProcAddress
	 * - Linux/Unix平台使用dlsym
	 * 
	 * 注意：如果句柄为NULL，函数会直接返回NULL。
	 */
	static ProcHandle get_symbol(DllHandle handle, const char* name)
	{
		if (NULL == handle)  // 检查句柄是否有效
			return NULL;  // 无效句柄直接返回NULL

#ifdef _MSC_VER  // Windows平台
		return ::GetProcAddress(handle, name);  // 调用Windows API获取符号地址
#else  // Linux/Unix平台
		return dlsym(handle, name);  // 调用Linux API获取符号地址
#endif
	}

	/**
	 * @brief 包装模块名称，自动添加平台特定的扩展名和前缀
	 * @param name 原始模块名称
	 * @param unixPrefix Unix平台下的库前缀，默认为"lib"
	 * @return std::string 返回包装后的完整模块名称
	 * 
	 * 该函数根据平台自动生成正确的动态库文件名：
	 * - Windows平台：添加.dll扩展名
	 * - Linux/Unix平台：添加lib前缀和.so扩展名
	 * 
	 * 注意：Linux平台下会跳过模块名称开头的非字母字符。
	 */
	static std::string wrap_module(const char* name, const char* unixPrefix = "lib")
	{
#ifdef _WIN32  // Windows平台
		std::string ret = name;  // 创建字符串副本
		ret += ".dll";  // 添加Windows动态库扩展名
		return std::move(ret);  // 使用移动语义返回结果
#else  // Linux/Unix平台
		std::size_t idx = 0;  // 初始化索引变量
		while (!isalpha(name[idx]))  // 跳过开头的非字母字符
			idx++;  // 递增索引
		std::string ret(name, idx);  // 创建包含非字母字符的字符串
		ret.append(unixPrefix);  // 添加Unix库前缀（如"lib"）
		ret.append(name + idx);  // 添加模块名称（跳过非字母字符部分）
		ret += ".so";  // 添加Linux动态库扩展名
		return std::move(ret);  // 使用移动语义返回结果
#endif
	}
};