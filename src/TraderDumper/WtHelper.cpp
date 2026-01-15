/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtHelper类的实现文件
 * 
 * 本文件实现了WtHelper类的所有方法，提供路径管理功能的具体实现。
 * 实现了跨平台的当前工作目录获取功能，支持Windows（MSVC）和Unix/Linux系统。
 */
#include "WtHelper.h"              // 包含类定义头文件

#include "../Share/StrUtil.hpp"   // 包含字符串工具类，用于路径标准化

#ifdef _MSC_VER                    // 如果是Microsoft Visual C++编译器（Windows平台）
#include <direct.h>                // Windows平台的头文件，提供_getcwd函数
#else	//UNIX                      // 否则是Unix/Linux平台
#include <unistd.h>                // Unix标准头文件，提供getcwd函数
#endif

std::string WtHelper::_bin_dir;   // 静态成员变量定义：存储模块目录路径，初始化为空字符串

/**
 * @brief 获取当前工作目录的实现
 * @return 返回当前工作目录的字符串指针（C风格字符串）
 * 
 * 实现逻辑：
 * 1. 使用静态局部变量_cwd缓存工作目录，避免重复获取
 * 2. 如果缓存为空，则调用系统API获取当前工作目录
 *    - Windows平台使用_getcwd函数
 *    - Unix/Linux平台使用getcwd函数
 * 3. 将获取的路径进行标准化处理（统一路径分隔符等）
 * 4. 返回标准化后的路径字符串指针
 */
const char* WtHelper::get_cwd()
{
	static std::string _cwd;      // 静态局部变量：缓存当前工作目录，程序生命周期内只获取一次
	if(_cwd.empty())               // 如果缓存为空，需要获取工作目录
	{
		char   buffer[255];        // 缓冲区：用于存储系统API返回的路径字符串，最大255字符
#ifdef _MSC_VER                    // Windows平台编译分支
		_getcwd(buffer, 255);      // Windows API：获取当前工作目录，存储到buffer中
#else	//UNIX                      // Unix/Linux平台编译分支
		getcwd(buffer, 255);       // Unix API：获取当前工作目录，存储到buffer中
#endif
		_cwd = buffer;             // 将C风格字符串转换为std::string
		_cwd = StrUtil::standardisePath(_cwd);  // 路径标准化：统一路径分隔符（Windows用\，Unix用/），处理相对路径等
	}	
	return _cwd.c_str();           // 返回C风格字符串指针，供C接口使用
}