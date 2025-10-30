/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据伺服器辅助工具类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WtHelper（辅助工具）类的具体实现，提供了模块路径管理和工作目录查询的
 * 具体功能。该文件实现了跨平台的工作目录查询功能，支持Windows和Unix系统，并提供了
 * 路径标准化处理。
 * 
 * 核心实现机制：
 * 
 * 1. 跨平台工作目录查询（Cross-Platform Working Directory Query）：
 *    - Windows平台：使用_getcwd()函数
 *    - Unix平台：使用getcwd()函数
 *    - 使用条件编译确保跨平台兼容性
 * 
 * 2. 路径标准化处理（Path Standardization）：
 *    - 使用StrUtil::standardisePath()统一路径格式
 *    - 确保路径分隔符的一致性
 *    - 处理路径末尾的斜杠问题
 * 
 * 3. 性能优化（Performance Optimization）：
 *    - 使用静态变量缓存工作目录结果
 *    - 避免重复的系统调用
 *    - 首次调用时进行查询和标准化
 * 
 * 主要功能实现：
 * 
 * 1. 静态成员变量初始化：
 *    - _bin_dir：模块目录路径，初始为空字符串
 * 
 * 2. 工作目录查询实现：
 *    - 使用静态局部变量缓存结果
 *    - 跨平台API调用
 *    - 路径标准化处理
 * 
 * 使用场景：
 * - 程序启动时的路径初始化
 * - 动态库加载路径的构建
 * - 配置文件路径的确定
 * - 日志文件路径的规范化
 * 
 * 技术特点：
 * - 跨平台兼容性
 * - 性能优化（缓存机制）
 * - 路径标准化
 * - 简单的API设计
 * 
 * 注意事项：
 * - 工作目录在首次调用后不再变化
 * - 模块目录需要手动设置
 * - 路径使用标准化的格式
 */
#include "WtHelper.h"                                                           // 包含辅助工具类头文件

#include "../Share/StrUtil.hpp"                                                 // 包含字符串工具类

#ifdef _MSC_VER                                                                 // 如果是Microsoft Visual C++编译器（Windows平台）
#include <direct.h>                                                             // 包含Windows目录操作函数（_getcwd）
#else	//UNIX                                                                   // 否则是Unix/Linux平台
#include <unistd.h>                                                              // 包含Unix标准库（getcwd）
#endif

std::string WtHelper::_bin_dir;                                                 // 静态成员变量定义：二进制模块目录路径，初始为空字符串

/**
 * @brief 获取当前工作目录的实现
 * @return 当前工作目录的字符串指针（标准化路径格式）
 * 
 * 该函数使用静态局部变量缓存工作目录结果，避免重复的系统调用。
 * 首次调用时会查询系统获取当前工作目录，并经过标准化处理。
 * 后续调用直接返回缓存的结果。
 */
const char* WtHelper::get_cwd()
{
	static std::string _cwd;                                                     // 静态局部变量：缓存工作目录路径，确保只查询一次
	if(_cwd.empty())                                                             // 如果缓存为空（首次调用）
	{
		char   buffer[255];                                                      // 临时缓冲区，用于存储系统返回的路径
#ifdef _MSC_VER                                                                 // 如果是Windows平台
		_getcwd(buffer, 255);                                                    // Windows平台：获取当前工作目录
#else	//UNIX                                                                   // 如果是Unix/Linux平台
		getcwd(buffer, 255);                                                    // Unix平台：获取当前工作目录
#endif
		_cwd = buffer;                                                          // 将缓冲区内容复制到字符串
		_cwd = StrUtil::standardisePath(_cwd);                                  // 标准化路径格式（统一路径分隔符，处理末尾斜杠等）
	}	
	return _cwd.c_str();                                                         // 返回标准化后的路径字符串指针
}