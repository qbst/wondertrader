/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类实现
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WtHelper辅助工具类的实现文件，提供了路径管理功能的具体实现。
 * 主要实现了跨平台的当前工作目录获取功能，是WonderTrader框架中基础的
 * 工具模块之一。
 * 
 * 核心功能实现：
 * 
 * 1. 当前工作目录获取（get_cwd）：
 *    - Windows平台：使用<direct.h>中的_getcwd()函数
 *    - Unix/Linux平台：使用<unistd.h>中的getcwd()函数
 *    - 路径标准化：将获取的路径转换为统一格式（使用'/'分隔符）
 *    - 懒加载缓存：首次调用时获取并缓存，后续直接返回缓存值
 * 
 * 2. 静态成员变量初始化：
 *    - _bin_dir：模块目录路径，初始化为空字符串
 * 
 * 技术实现细节：
 * 
 * 1. 跨平台兼容性：
 *    - 使用预处理宏（_MSC_VER）区分Windows和Unix平台
 *    - 包含对应平台的系统头文件
 *    - 调用对应平台的API函数
 * 
 * 2. 缓存机制：
 *    - 使用静态局部变量存储工作目录
 *    - 通过empty()判断是否已初始化
 *    - 避免重复的系统调用，提高性能
 * 
 * 3. 路径标准化：
 *    - 调用StrUtil::standardisePath()统一路径格式
 *    - 确保路径使用'/'而非'\\'作为分隔符
 *    - 便于跨平台代码的一致性
 * 
 * 依赖关系：
 * - Share/StrUtil.hpp：提供字符串和路径处理工具
 * - <direct.h>（Windows）：提供_getcwd()函数
 * - <unistd.h>（Unix）：提供getcwd()函数
 * 
 * 使用场景：
 * - 程序启动时确定工作目录
 * - 构建相对路径时需要知道基准目录
 * - 配置文件、日志文件的路径计算
 * - 模块动态加载时的路径解析
 */

#include "WtHelper.h"                   // 包含WtHelper类的头文件

#include "../Share/StrUtil.hpp"        // 包含字符串工具类，用于路径标准化

// 根据编译器类型包含对应的系统头文件
#ifdef _MSC_VER                         // 如果是Microsoft Visual C++编译器（Windows平台）
#include <direct.h>                     // 包含_getcwd()函数声明
#else	//UNIX                           // 否则是Unix/Linux平台
#include <unistd.h>                     // 包含getcwd()函数声明
#endif

/**
 * 静态成员变量定义和初始化
 * 
 * _bin_dir：存储模块（二进制文件）所在的目录路径
 * - 初始化为空字符串
 * - 通过set_module_dir()方法设置
 * - 通过get_module_dir()方法访问
 * - 用于构建动态加载模块的完整路径
 */
std::string WtHelper::_bin_dir;

/**
 * @brief 获取当前工作目录实现
 * 
 * 该方法是get_cwd()的具体实现，使用系统API获取程序的当前工作目录。
 * 采用懒加载模式：首次调用时执行初始化，后续调用直接返回缓存的结果。
 * 
 * 实现逻辑：
 * 1. 检查静态缓存变量_cwd是否为空
 * 2. 如果为空，调用系统API获取当前目录
 * 3. 对获取的路径进行标准化处理
 * 4. 将结果存入静态变量_cwd
 * 5. 返回_cwd的C风格字符串指针
 * 
 * 跨平台实现：
 * - Windows：使用_getcwd(buffer, 255)
 * - Unix/Linux：使用getcwd(buffer, 255)
 * - 两个函数功能相同，只是名称不同
 * 
 * 路径标准化：
 * - 使用StrUtil::standardisePath()处理路径
 * - 将Windows的反斜杠'\\'转换为正斜杠'/'
 * - 确保路径以'/'结尾
 * 
 * 线程安全性：
 * - C++11标准保证静态局部变量的初始化是线程安全的
 * - 多线程同时首次调用时不会导致数据竞争
 * - 但标准不保证函数体其他部分的线程安全性
 * 
 * 性能优化：
 * - 懒加载避免不必要的系统调用
 * - 缓存机制使后续调用的时间复杂度为O(1)
 * - 系统调用（getcwd）相对耗时，缓存能显著提升性能
 * 
 * @return const char* 当前工作目录的C风格字符串指针
 * 
 * @note 返回的指针指向静态局部变量，生命周期与程序相同
 * @note 如果获取目录失败，buffer内容未定义，但不会崩溃
 */
const char* WtHelper::get_cwd()
{
	// 静态局部变量，用于缓存当前工作目录
	// - 只在首次调用时初始化
	// - 生命周期与程序相同
	// - C++11保证多线程初始化安全
	static std::string _cwd;
	
	// 检查是否已经初始化（懒加载模式）
	if(_cwd.empty())  // 如果_cwd为空，说明是首次调用，需要初始化
	{
		// 定义缓冲区用于存储系统返回的路径
		// 255字节足够存储大多数合理的路径
		char   buffer[255];
		
#ifdef _MSC_VER  // 如果是Windows平台（Microsoft Visual C++编译器）
		// 调用Windows API获取当前工作目录
		// _getcwd：Windows平台的获取当前目录函数
		// 参数1：存储路径的缓冲区
		// 参数2：缓冲区大小
		// 返回值：成功返回buffer指针，失败返回NULL
		_getcwd(buffer, 255);
		
#else	//UNIX  // 如果是Unix/Linux平台
		// 调用Unix/Linux API获取当前工作目录
		// getcwd：Unix/Linux平台的获取当前目录函数
		// 参数和返回值与_getcwd相同
		getcwd(buffer, 255);
#endif

		// 将缓冲区的内容赋值给_cwd
		// buffer是C风格字符串，std::string会自动复制内容
		_cwd = buffer;
		
		// 对路径进行标准化处理
		// standardisePath功能：
		// 1. 将Windows的反斜杠'\\'转换为正斜杠'/'
		// 2. 去除路径中的冗余部分（如'./'、'//'等）
		// 3. 确保路径以'/'结尾（如果是目录）
		// 4. 使路径格式在不同平台上保持一致
		_cwd = StrUtil::standardisePath(_cwd);
	}
	
	// 返回缓存的工作目录字符串的C风格指针
	// c_str()：返回指向内部字符数组的const char*指针
	// 指针有效期：直到_cwd被修改或销毁（程序结束时）
	return _cwd.c_str();
}
