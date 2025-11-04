/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类实现文件
 *
 * 本文件实现了WtHelper类的所有静态方法，提供路径管理和时间管理功能。
 *
 * 设计逻辑：
 * 1. 静态变量初始化：在文件作用域初始化所有静态成员变量
 * 2. 路径标准化：使用StrUtil::standardisePath统一路径格式
 * 3. 目录自动创建：获取目录时自动检查并创建不存在的目录
 * 4. 路径缓存：使用静态局部变量缓存目录路径，避免重复计算
 * 5. 跨平台支持：支持Windows和Unix平台的路径获取
 *
 * 主要功能：
 * - 实现当前工作目录的获取
 * - 实现模块路径的构建
 * - 实现各种目录的获取和自动创建
 */
#include "WtHelper.h"  // WonderTrader辅助工具类头文件

#include "../Share/StrUtil.hpp"  // 字符串工具头文件
#include "../Share/StdUtils.hpp"  // 标准工具头文件

#include <boost/filesystem.hpp>  // Boost文件系统库

#ifdef _MSC_VER  // Windows平台
#include <direct.h>  // Windows目录操作头文件
#else	//UNIX  // Unix平台
#include <unistd.h>  // Unix目录操作头文件
#endif

// 静态成员变量初始化
uint32_t WtHelper::_cur_date = 0;  // 初始化当前日期为0
uint32_t WtHelper::_cur_time = 0;  // 初始化当前时间为0
uint32_t WtHelper::_cur_secs = 0;  // 初始化当前秒数为0
uint32_t WtHelper::_cur_tdate = 0;  // 初始化当前交易日为0
std::string WtHelper::_inst_dir;  // 初始化实例目录为空字符串
std::string WtHelper::_gen_dir = "./generated/";  // 初始化生成目录为"./generated/"


/**
 * @brief 获取当前工作目录实现
 * @return 当前工作目录路径
 * 
 * 获取程序运行时的当前工作目录（Current Working Directory）。
 * 使用静态变量缓存结果，避免重复获取。
 * 跨平台实现：Windows使用_getcwd，Unix使用getcwd。
 */
std::string WtHelper::getCWD()
{
	static std::string _cwd;  // 静态局部变量，缓存工作目录
	if(_cwd.empty())  // 如果缓存为空，需要获取
	{
		char   buffer[256];  // 缓冲区
#ifdef _MSC_VER  // Windows平台
		_getcwd(buffer, 255);  // Windows系统调用获取当前工作目录
#else	//UNIX  // Unix平台
		getcwd(buffer, 255);  // Unix系统调用获取当前工作目录
#endif
		_cwd = StrUtil::standardisePath(buffer);  // 标准化路径格式并缓存
	}	
	return _cwd;  // 返回缓存的工作目录
}

/**
 * @brief 获取模块路径实现
 * @param moduleName 模块名称
 * @param subDir 子目录
 * @param isCWD 是否使用当前工作目录，默认true
 * @return 模块完整路径
 * 
 * 构建模块的完整路径。
 * 路径格式：基础目录 + 子目录 + "/" + 模块名
 * 如果isCWD为true，基于当前工作目录；否则基于实例目录。
 */
std::string WtHelper::getModulePath(const char* moduleName, const char* subDir, bool isCWD /* = true */)
{
	std::stringstream ss;  // 字符串流对象
	ss << (isCWD?getCWD():getInstDir()) << subDir << "/" << moduleName;  // 构建完整路径
	return ss.str();  // 返回路径字符串
}

/**
 * @brief 获取策略数据目录实现
 * @return 策略数据目录路径
 * 
 * 获取策略数据存储目录（generated/stradata目录）。
 * 如果目录不存在，会自动创建。
 * 使用静态局部变量缓存路径，避免重复计算。
 */
const char* WtHelper::getStraDataDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "stradata/";  // 静态局部变量，缓存目录路径
	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		boost::filesystem::create_directories(folder);  // 创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径的C字符串
}

/**
 * @brief 获取策略用户数据目录实现
 * @return 策略用户数据目录路径
 * 
 * 获取策略用户数据存储目录（generated/userdata目录）。
 * 如果目录不存在，会自动创建。
 * 使用静态局部变量缓存路径，避免重复计算。
 */
const char* WtHelper::getStraUsrDatDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "userdata/";  // 静态局部变量，缓存目录路径
	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		boost::filesystem::create_directories(folder);  // 创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径的C字符串
}

/**
 * @brief 获取投资组合目录实现
 * @return 投资组合目录路径
 * 
 * 获取投资组合数据存储目录（generated/portfolio目录）。
 * 如果目录不存在，会自动创建。
 * 使用静态局部变量缓存路径，避免重复计算。
 */
const char* WtHelper::getPortifolioDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "portfolio/";  // 静态局部变量，缓存目录路径
	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		boost::filesystem::create_directories(folder);  // 创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径的C字符串
}

/**
 * @brief 获取输出目录实现
 * @return 输出目录路径
 * 
 * 获取输出文件目录（generated/outputs目录）。
 * 如果目录不存在，会自动创建。
 * 使用静态局部变量缓存路径，避免重复计算。
 */
const char* WtHelper::getOutputDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "outputs/";  // 静态局部变量，缓存目录路径
	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		boost::filesystem::create_directories(folder);  // 创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径的C字符串
}

const char* WtHelper::getBaseDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir);  // 静态局部变量，缓存目录路径
	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		boost::filesystem::create_directories(folder);  // 创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径的C字符串
}