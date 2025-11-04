/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 辅助工具类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtHelper类的所有方法，提供了路径管理和时间管理的具体实现。
 * 主要实现包括：
 * 1. 工作目录获取：使用系统API获取当前工作目录，支持Windows和Unix平台
 * 2. 模块路径构建：根据模块名称和子目录构建完整路径
 * 3. 目录路径获取：获取各种目录路径，如果目录不存在则自动创建
 * 4. 路径标准化：统一路径格式，使用斜杠作为分隔符
 * 5. 目录自动创建：使用boost::filesystem自动创建不存在的目录
 * 
 * 实现细节：
 * - 使用静态变量缓存目录路径，避免重复计算
 * - 使用静态局部变量实现单例模式的效果
 * - 路径自动标准化处理，统一使用斜杠分隔符
 * - 支持Windows和Linux平台的路径处理差异
 */
#include "WtHelper.h"  // 包含辅助工具类头文件

#include "../Share/StrUtil.hpp"  // 包含字符串处理工具函数，如路径标准化
#include "../Share/StdUtils.hpp"  // 包含标准工具函数，如文件存在性检查

#include <boost/filesystem.hpp>  // 包含boost文件系统库，用于目录创建和路径操作

#ifdef _MSC_VER  // Windows平台编译条件
#include <direct.h>  // 包含Windows平台目录操作头文件
#else	//UNIX  // Unix/Linux平台编译条件
#include <unistd.h>  // 包含Unix平台目录操作头文件
#endif

// 静态成员变量初始化
uint32_t WtHelper::_cur_date = 0;  // 初始化当前日期为0
uint32_t WtHelper::_cur_time = 0;  // 初始化当前时间为0
uint32_t WtHelper::_cur_secs = 0;  // 初始化当前秒数为0
uint32_t WtHelper::_cur_tdate = 0;  // 初始化交易日期为0
std::string WtHelper::_inst_dir;  // 初始化实例目录为空字符串
std::string WtHelper::_gen_dir = "./generated/";  // 初始化生成文件输出目录为"./generated/"


/**
 * @brief 获取当前工作目录
 * @return std::string 返回当前工作目录路径
 * 
 * 该函数获取程序运行时的当前工作目录（Current Working Directory）。
 * 实现细节：
 * - 使用静态局部变量缓存结果，避免重复调用系统API
 * - 首次调用时从系统获取工作目录
 * - 使用平台相关的API：Windows使用_getcwd，Unix使用getcwd
 * - 路径通过StrUtil::standardisePath标准化处理，统一使用斜杠分隔符
 */
std::string WtHelper::getCWD()
{
	static std::string _cwd;  // 静态局部变量，缓存当前工作目录
	if(_cwd.empty())  // 如果缓存为空，说明首次调用
	{
		char   buffer[256];  // 缓冲区，用于存储路径字符串
#ifdef _MSC_VER  // Windows平台编译条件
		_getcwd(buffer, 255);  // Windows平台：获取当前工作目录
#else	//UNIX  // Unix/Linux平台编译条件
		getcwd(buffer, 255);  // Unix平台：获取当前工作目录
#endif
		_cwd = StrUtil::standardisePath(buffer);  // 标准化路径格式，统一使用斜杠分隔符
	}	
	return _cwd;  // 返回缓存的工作目录
}

/**
 * @brief 获取模块路径
 * @param moduleName 模块名称
 * @param subDir 子目录名称
 * @param isCWD 是否基于当前工作目录，默认为true；false表示基于实例目录
 * @return std::string 返回完整的模块路径
 * 
 * 该函数根据模块名称和子目录构建完整的模块路径。
 * 路径格式：基础目录 + 子目录 + "/" + 模块名称
 * 
 * 实现细节：
 * - 使用stringstream构建路径字符串
 * - 根据isCWD参数选择基础目录（当前工作目录或实例目录）
 * - 路径会自动拼接斜杠分隔符
 */
std::string WtHelper::getModulePath(const char* moduleName, const char* subDir, bool isCWD /* = true */)
{
	std::stringstream ss;  // 创建字符串流对象，用于构建路径
	ss << (isCWD?getCWD():getInstDir()) << subDir << "/" << moduleName;  // 拼接路径：基础目录 + 子目录 + "/" + 模块名称
	return ss.str();  // 返回构建的路径字符串
}

/**
 * @brief 获取策略数据目录
 * @return const char* 返回策略数据目录路径字符串
 * 
 * 该函数获取策略数据目录，用于存放策略相关的数据文件。
 * 实现细节：
 * - 使用静态局部变量缓存目录路径
 * - 路径格式：生成文件输出目录 + "stradata/"
 * - 如果目录不存在，使用boost::filesystem自动创建
 * - 路径通过StrUtil::standardisePath标准化处理
 */
const char* WtHelper::getStraDataDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "stradata/";  // 构建策略数据目录路径并标准化
	if (!StdFile::exists(folder.c_str()))  // 检查目录是否存在
		boost::filesystem::create_directories(folder);  // 如果不存在则创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径字符串
}

/**
 * @brief 获取执行数据目录
 * @return const char* 返回执行数据目录路径字符串
 * 
 * 该函数获取执行数据目录，用于存放执行器相关的数据文件。
 * 实现细节：
 * - 使用静态局部变量缓存目录路径
 * - 路径格式：生成文件输出目录 + "execdata/"
 * - 如果目录不存在，使用boost::filesystem自动创建
 * - 路径通过StrUtil::standardisePath标准化处理
 */
const char* WtHelper::getExecDataDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "execdata/";  // 构建执行数据目录路径并标准化
	if (!StdFile::exists(folder.c_str()))  // 检查目录是否存在
		boost::filesystem::create_directories(folder);  // 如果不存在则创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径字符串
}

/**
 * @brief 获取策略用户数据目录
 * @return const char* 返回策略用户数据目录路径字符串
 * 
 * 该函数获取策略用户数据目录，用于存放策略的用户自定义数据文件。
 * 实现细节：
 * - 使用静态局部变量缓存目录路径
 * - 路径格式：生成文件输出目录 + "userdata/"
 * - 如果目录不存在，使用boost::filesystem自动创建
 * - 路径通过StrUtil::standardisePath标准化处理
 */
const char* WtHelper::getStraUsrDatDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "userdata/";  // 构建策略用户数据目录路径并标准化
	if (!StdFile::exists(folder.c_str()))  // 检查目录是否存在
		boost::filesystem::create_directories(folder);  // 如果不存在则创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径字符串
}

/**
 * @brief 获取组合目录
 * @return const char* 返回组合目录路径字符串
 * 
 * 该函数获取组合（Portfolio）目录，用于存放组合相关的数据文件。
 * 实现细节：
 * - 使用静态局部变量缓存目录路径
 * - 路径格式：生成文件输出目录 + "portfolio/"
 * - 如果目录不存在，使用boost::filesystem自动创建
 * - 路径通过StrUtil::standardisePath标准化处理
 */
const char* WtHelper::getPortifolioDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "portfolio/";  // 构建组合目录路径并标准化
	if (!StdFile::exists(folder.c_str()))  // 检查目录是否存在
		boost::filesystem::create_directories(folder);  // 如果不存在则创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径字符串
}

/**
 * @brief 获取输出目录
 * @return const char* 返回输出目录路径字符串
 * 
 * 该函数获取输出文件目录，用于存放各种输出文件（如日志、报告等）。
 * 实现细节：
 * - 使用静态局部变量缓存目录路径
 * - 路径格式：生成文件输出目录 + "outputs/"
 * - 如果目录不存在，使用boost::filesystem自动创建
 * - 路径通过StrUtil::standardisePath标准化处理
 */
const char* WtHelper::getOutputDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir) + "outputs/";  // 构建输出目录路径并标准化
	if (!StdFile::exists(folder.c_str()))  // 检查目录是否存在
		boost::filesystem::create_directories(folder);  // 如果不存在则创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径字符串
}

/**
 * @brief 获取基础目录
 * @return const char* 返回基础目录路径字符串
 * 
 * 该函数获取生成文件的基础输出目录，所有生成的文件都存放在此目录下。
 * 实现细节：
 * - 使用静态局部变量缓存目录路径
 * - 路径直接使用生成文件输出目录（_gen_dir）
 * - 如果目录不存在，使用boost::filesystem自动创建
 * - 路径通过StrUtil::standardisePath标准化处理
 */
const char* WtHelper::getBaseDir()
{
	static std::string folder = StrUtil::standardisePath(_gen_dir);  // 标准化生成文件输出目录路径
	if (!StdFile::exists(folder.c_str()))  // 检查目录是否存在
		boost::filesystem::create_directories(folder);  // 如果不存在则创建目录（包括所有父目录）
	return folder.c_str();  // 返回目录路径字符串
}
