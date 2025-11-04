/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类实现文件
 *
 * 本文件实现了WtHelper类的所有功能，包括：
 * 1. 当前工作目录获取：跨平台获取当前工作目录
 * 2. 输出目录管理：获取和设置回测结果输出目录，自动创建目录
 * 3. 实例目录管理：获取和设置实例目录
 */
#include "WtHelper.h"              // WonderTrader辅助工具头文件

#include "../Share/StrUtil.hpp"    // 字符串工具库
#include <boost/filesystem.hpp>    // Boost文件系统库，用于目录操作

#ifdef _MSC_VER                    // Windows平台
#include <direct.h>                // Windows目录操作头文件
#else	//UNIX                      // Unix/Linux平台
#include <unistd.h>                // Unix目录操作头文件
#endif

std::string WtHelper::_inst_dir;                           // 静态成员变量：实例目录，初始化为空字符串
std::string WtHelper::_out_dir = "./outputs_bt/";          // 静态成员变量：输出目录，默认值为"./outputs_bt/"

/**
 * @brief 获取当前工作目录
 * @return 当前工作目录路径字符串
 * 
 * 获取程序的当前工作目录，并标准化路径格式。
 * 使用静态变量缓存结果，提高性能。
 * 支持Windows和Unix平台。
 */
std::string WtHelper::getCWD()
{
	static std::string _cwd;       // 静态变量，缓存当前工作目录
	if(_cwd.empty())               // 如果缓存为空，则获取当前工作目录
	{
		char   buffer[255];         // 缓冲区，用于存储路径
#ifdef _MSC_VER                    // Windows平台
		_getcwd(buffer, 255);       // Windows API：获取当前工作目录
#else	//UNIX                      // Unix/Linux平台
		getcwd(buffer, 255);        // Unix API：获取当前工作目录
#endif
		_cwd = buffer;              // 将缓冲区内容转换为字符串
		_cwd = StrUtil::standardisePath(_cwd);  // 标准化路径格式（统一路径分隔符）
	}	
	return _cwd;                    // 返回缓存的当前工作目录
}

/**
 * @brief 设置输出目录
 * @param out_dir 输出目录路径
 * 
 * 设置回测结果输出目录的路径。
 * 路径会被标准化（统一路径分隔符格式）。
 */
void WtHelper::setOutputDir(const char* out_dir)
{
	_out_dir = StrUtil::standardisePath(std::string(out_dir));  // 标准化路径格式并设置
}

/**
 * @brief 获取输出目录
 * @return 输出目录路径字符串（C风格字符串）
 * 
 * 获取回测结果输出目录。如果目录不存在，会自动创建。
 * 默认输出目录为"./outputs_bt/"。
 */
const char* WtHelper::getOutputDir()
{
	if (!boost::filesystem::exists(_out_dir.c_str()))        // 如果目录不存在
        boost::filesystem::create_directories(_out_dir.c_str());  // 创建目录（包括所有父目录）
	return _out_dir.c_str();                                 // 返回目录路径（C风格字符串）
}