/*!
 * \file StrUtil.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 字符串处理的封装
 * 
 * 该文件提供了全面的字符串处理工具类，主要包括：
 * 1. 字符串修剪和清理功能（去除空格、制表符等）
 * 2. 字符串分割和合并功能
 * 3. 字符串大小写转换功能
 * 4. 字符串模式匹配和通配符支持
 * 5. 路径标准化和文件名分割功能
 * 6. 格式化字符串输出功能（类似printf）
 * 7. 字符串扩展和对齐功能
 * 
 * 设计逻辑：
 * - 所有方法都是静态内联函数，提供高效的字符串操作
 * - 支持跨平台的字符串比较（Windows和Unix/Linux）
 * - 使用STL算法和迭代器实现高效的字符串处理
 * - 提供多种重载版本，支持不同的输入类型
 * - 使用移动语义优化返回值性能
 * 
 * 主要作用：
 * - 为WonderTrader框架提供完整的字符串处理能力
 * - 简化字符串操作的代码编写，提高开发效率
 * - 支持高频交易等场景下的字符串处理需求
 * - 提供跨平台的字符串操作兼容性
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串支持
#include <vector>  // 包含向量容器支持
#include <algorithm>  // 包含STL算法支持
#include <sstream>  // 包含字符串流支持
#include <functional>  // 包含函数对象支持
#include <stdarg.h>  // 包含可变参数支持
#include <stdint.h>  // 包含固定大小整数类型
#include <string.h>  // 包含C风格字符串函数
#include <cctype>  // 包含字符类型判断函数

typedef std::vector<std::string> StringVector;  // 字符串向量类型别名，用于存储字符串列表

/**
 * @class StrUtil
 * @brief 字符串工具类
 * 
 * 该类提供了全面的字符串处理功能，包括修剪、分割、转换、匹配等操作。
 * 所有方法都是静态的，无需实例化对象即可使用。
 * 支持跨平台兼容性，自动处理不同操作系统的差异。
 */
class StrUtil
{
public:

	/**
	 * @brief 修剪字符串，去除指定字符
	 * @param str 要修剪的字符串引用
	 * @param delims 分隔符字符串，默认为空格、制表符、回车符
	 * @param left 是否修剪左侧，默认为true
	 * @param right 是否修剪右侧，默认为true
	 * 
	 * 该函数去除字符串开头和/或结尾的指定字符。
	 * 支持自定义分隔符，默认去除常见的空白字符。
	 * 
	 * 注意：该函数直接修改传入的字符串引用。
	 */
	static inline void trim(std::string& str, const char* delims = " \t\r", bool left = true, bool right = true)
	{
		if(right)  // 如果需要修剪右侧
			str.erase(str.find_last_not_of(delims)+1);  // 从右向左查找第一个非分隔符位置，然后删除后面的所有字符
		if(left)  // 如果需要修剪左侧
			str.erase(0, str.find_first_not_of(delims));  // 从左向右查找第一个非分隔符位置，然后删除前面的所有字符
	}

	/**
	 * @brief 修剪字符串，去除指定字符（返回新字符串）
	 * @param str 要修剪的C风格字符串
	 * @param delims 分隔符字符串，默认为空格、制表符、回车符
	 * @param left 是否修剪左侧，默认为true
	 * @param right 是否修剪右侧，默认为true
	 * @return std::string 返回修剪后的新字符串
	 * 
	 * 该函数是trim的重载版本，不修改原字符串，而是返回修剪后的新字符串。
	 * 使用移动语义优化返回值性能。
	 */
	static inline std::string trim(const char* str, const char* delims = " \t\r", bool left = true, bool right = true)
	{
		std::string ret = str;  // 创建字符串副本
		if(right)  // 如果需要修剪右侧
			ret.erase(ret.find_last_not_of(delims)+1);  // 从右向左查找第一个非分隔符位置，然后删除后面的所有字符
		if(left)  // 如果需要修剪左侧
			ret.erase(0, ret.find_first_not_of(delims));  // 从左向右查找第一个非分隔符位置，然后删除前面的所有字符

		return std::move(ret);  // 使用移动语义返回结果
	}

	/**
	 * @brief 去除字符串中的所有空格字符
	 * @param str 要处理的字符串引用
	 * 
	 * 该函数使用STL的remove_if算法去除字符串中的所有空格字符。
	 * 使用lambda表达式判断字符是否为空格。
	 * 
	 * 注意：该函数直接修改传入的字符串引用。
	 */
	static inline void trimAllSpace(std::string &str)
	{
		std::string::iterator destEnd = std::remove_if(str.begin(), str.end(), [](const char& c){  // 使用lambda表达式定义删除条件
			return c == ' ';  // 判断字符是否为空格
		});
		str.resize(destEnd-str.begin());  // 调整字符串大小，去除被"删除"的字符
	}

	//去除所有特定字符
	//static inline void trimAll(std::string &str,char ch)
	//{
	//	std::string::iterator destEnd=std::remove_if(str.begin(),str.end(),std::bind1st(std::equal_to<char>(),ch));
	//	str.resize(destEnd-str.begin());
	//}

	/**
	 * @brief 查找字符串中第一个指定字符的位置
	 * @param str 要搜索的C风格字符串
	 * @param ch 要查找的字符
	 * @return std::size_t 返回字符位置，未找到返回std::string::npos
	 * 
	 * 该函数使用循环遍历字符串，查找第一个匹配的字符。
	 * 适用于需要手动控制搜索过程的场景。
	 */
	static inline std::size_t findFirst(const char* str, char ch)
	{
		std::size_t i = 0;  // 初始化索引变量
		for(;;)  // 无限循环，直到找到字符或到达字符串末尾
		{
			if (str[i] == ch)  // 检查当前字符是否匹配
				return i;  // 找到匹配字符，返回位置

			if(str[i] == '\0')  // 检查是否到达字符串末尾
				break;  // 到达末尾，跳出循环

			i++;  // 递增索引，检查下一个字符
		}

		return std::string::npos;  // 未找到字符，返回npos
	}

	/**
	 * @brief 查找字符串中最后一个指定字符的位置
	 * @param str 要搜索的C风格字符串
	 * @param ch 要查找的字符
	 * @return std::size_t 返回字符位置，未找到返回std::string::npos
	 * 
	 * 该函数从右向左遍历字符串，查找最后一个匹配的字符。
	 * 适用于需要从末尾开始搜索的场景。
	 */
	static inline std::size_t findLast(const char* str, char ch)
	{
		auto len = strlen(str);  // 获取字符串长度
		std::size_t i = 0;  // 初始化索引变量
		for (; i < len; i++)  // 从右向左遍历字符串
		{
			if (str[len - 1 - i] == ch)  // 从右向左检查字符是否匹配
				return len - 1 - i;  // 找到匹配字符，返回位置
		}

		return std::string::npos;  // 未找到字符，返回npos
	}

	/**
	 * @brief 分割字符串，返回字符串向量
	 * @param str 要分割的字符串
	 * @param delims 分隔符字符串，默认为制表符、换行符、空格
	 * @param maxSplits 最大分割次数，0表示无限制
	 * @return StringVector 返回分割后的字符串向量
	 * 
	 * 该函数根据指定的分隔符将字符串分割成多个子字符串。
	 * 支持限制最大分割次数，避免过度分割。
	 * 使用STL的find_first_of方法进行高效分割。
	 */
	static inline StringVector split( const std::string& str, const std::string& delims = "\t\n ", unsigned int maxSplits = 0)
	{
		StringVector ret;  // 创建返回的字符串向量
		unsigned int numSplits = 0;  // 初始化分割计数器

		// Use STL methods
		size_t start, pos;  // 声明起始位置和分隔符位置变量
		start = 0;  // 设置起始位置为0
		do  // 开始分割循环
		{
			pos = str.find_first_of(delims, start);  // 从起始位置查找第一个分隔符
			if (pos == start)  // 如果分隔符就在起始位置
			{
				ret.emplace_back("");  // 添加空字符串到结果向量
				// Do nothing
				start = pos + 1;  // 更新起始位置到分隔符之后
			}
			else if (pos == std::string::npos || (maxSplits && numSplits == maxSplits))  // 如果到达字符串末尾或达到最大分割次数
			{
				// Copy the rest of the std::string
				ret.emplace_back( str.substr(start) );  // 将剩余部分作为最后一个子字符串
				break;  // 跳出循环
			}
			else  // 正常情况：找到分隔符
			{
				// Copy up to delimiter
				ret.emplace_back( str.substr(start, pos - start) );  // 提取分隔符之前的子字符串
				start = pos + 1;  // 更新起始位置到分隔符之后
			}
			// parse up to next real data
			//start = str.find_first_not_of(delims, start);
			++numSplits;  // 增加分割计数器

		} while (pos != std::string::npos);  // 继续循环直到找不到更多分隔符
		return std::move(ret);  // 使用移动语义返回结果
	}

	/**
	 * @brief 分割字符串，将结果存储到指定的向量中
	 * @param str 要分割的字符串
	 * @param ret 输出参数，存储分割结果的字符串向量
	 * @param delims 分隔符字符串，默认为制表符、换行符、空格
	 * @param maxSplits 最大分割次数，0表示无限制
	 * 
	 * 该函数是split的重载版本，将分割结果存储到传入的向量中，
	 * 避免创建新的向量对象，提高性能。
	 */
	static inline void split(const std::string& str, StringVector& ret, const std::string& delims = "\t\n ", unsigned int maxSplits = 0)
	{
		unsigned int numSplits = 0;  // 初始化分割计数器

		// Use STL methods
		size_t start, pos;  // 声明起始位置和分隔符位置变量
		start = 0;  // 设置起始位置为0
		do  // 开始分割循环
		{
			pos = str.find_first_of(delims, start);  // 从起始位置查找第一个分隔符
			if (pos == start)  // 如果分隔符就在起始位置
			{
				ret.emplace_back("");  // 添加空字符串到结果向量
				// Do nothing
				start = pos + 1;  // 更新起始位置到分隔符之后
			}
			else if (pos == std::string::npos || (maxSplits && numSplits == maxSplits))  // 如果到达字符串末尾或达到最大分割次数
			{
				// Copy the rest of the std::string
				ret.emplace_back(str.substr(start));  // 将剩余部分作为最后一个子字符串
				break;  // 跳出循环
			}
			else  // 正常情况：找到分隔符
			{
				// Copy up to delimiter
				ret.emplace_back(str.substr(start, pos - start));  // 提取分隔符之前的子字符串
				start = pos + 1;  // 更新起始位置到分隔符之后
			}
			// parse up to next real data
			//start = str.find_first_not_of(delims, start);
			++numSplits;  // 增加分割计数器

		} while (pos != std::string::npos);  // 继续循环直到找不到更多分隔符
	}

	/**
	 * @brief 将字符串转换为小写
	 * @param str 要转换的字符串引用
	 * 
	 * 该函数使用STL的transform算法将字符串中的所有字符转换为小写。
	 * 使用C标准库的tolower函数进行字符转换。
	 * 
	 * 注意：该函数直接修改传入的字符串引用。
	 */
	static inline void toLowerCase( std::string& str )
	{
		std::transform(  // 使用STL的transform算法
			str.begin(),  // 源字符串起始迭代器
			str.end(),    // 源字符串结束迭代器
			str.begin(),  // 目标字符串起始迭代器（原地转换）
			(int(*)(int))tolower);  // 转换函数：将字符转换为小写

	}

	/**
	 * @brief 将字符串转换为大写
	 * @param str 要转换的字符串引用
	 * 
	 * 该函数使用STL的transform算法将字符串中的所有字符转换为大写。
	 * 使用C标准库的toupper函数进行字符转换。
	 * 
	 * 注意：该函数直接修改传入的字符串引用。
	 */
	static inline void toUpperCase( std::string& str )
	{
		std::transform(  // 使用STL的transform算法
			str.begin(),  // 源字符串起始迭代器
			str.end(),    // 源字符串结束迭代器
			str.begin(),  // 目标字符串起始迭代器（原地转换）
			(int(*)(int))toupper);  // 转换函数：将字符转换为大写
	}

	/**
	 * @brief 创建小写字符串（返回新字符串）
	 * @param str 要转换的C风格字符串
	 * @return std::string 返回转换后的小写字符串
	 * 
	 * 该函数不修改原字符串，而是返回转换后的新字符串。
	 * 使用移动语义优化返回值性能。
	 */
	static inline std::string makeLowerCase(const char* str)
	{
		std::string strRet = str;  // 创建字符串副本
		std::transform(  // 使用STL的transform算法
			strRet.begin(),  // 源字符串起始迭代器
			strRet.end(),    // 源字符串结束迭代器
			strRet.begin(),  // 目标字符串起始迭代器（原地转换）
			(int(*)(int))tolower);  // 转换函数：将字符转换为小写
		return std::move(strRet);  // 使用移动语义返回结果
	}

	/**
	 * @brief 创建大写字符串（返回新字符串）
	 * @param str 要转换的C风格字符串
	 * @return std::string 返回转换后的大写字符串
	 * 
	 * 该函数不修改原字符串，而是返回转换后的新字符串。
	 * 使用移动语义优化返回值性能。
	 */
	static inline std::string makeUpperCase(const char* str)
	{
		std::string strRet = str;  // 创建字符串副本
		std::transform(  // 使用STL的transform算法
			strRet.begin(),  // 源字符串起始迭代器
			strRet.end(),    // 源字符串结束迭代器
			strRet.begin(),  // 目标字符串起始迭代器（原地转换）
			(int(*)(int))toupper);  // 转换函数：将字符转换为大写
		return std::move(strRet);  // 使用移动语义返回结果
	}

	/**
	 * @brief 检查字符串是否以指定模式开始
	 * @param str 要检查的字符串
	 * @param pattern 要匹配的模式
	 * @param ignoreCase 是否忽略大小写，默认为true
	 * @return bool 匹配成功返回true，失败返回false
	 * 
	 * 该函数检查字符串是否以指定的模式开始。
	 * 支持忽略大小写的比较，通过条件编译实现跨平台兼容性。
	 */
	static inline bool startsWith(const char* str, const char* pattern, bool ignoreCase = true)
	{
		size_t thisLen = strlen(str);  // 获取源字符串长度
		size_t patternLen = strlen(pattern);  // 获取模式字符串长度
		if (thisLen < patternLen || patternLen == 0)  // 检查长度是否满足要求
			return false;  // 长度不满足要求，返回false

		if(ignoreCase)  // 如果需要忽略大小写
		{
#ifdef _MSC_VER  // Windows平台
			return _strnicmp(str, pattern, patternLen) == 0;  // 使用Windows API进行不区分大小写的比较
#else  // Unix/Linux平台
			return strncasecmp(str, pattern, patternLen) == 0;  // 使用Unix API进行不区分大小写的比较
#endif
		}
		else  // 如果区分大小写
		{
			return strncmp(str, pattern, patternLen) == 0;  // 使用标准C函数进行区分大小写的比较
		}
	}

	/**
	 * @brief 检查字符串是否以指定模式结束
	 * @param str 要检查的字符串
	 * @param pattern 要匹配的模式
	 * @param ignoreCase 是否忽略大小写，默认为true
	 * @return bool 匹配成功返回true，失败返回false
	 * 
	 * 该函数检查字符串是否以指定的模式结束。
	 * 支持忽略大小写的比较，通过条件编译实现跨平台兼容性。
	 */
	static inline bool endsWith(const char* str, const char* pattern, bool ignoreCase = true)
	{
		size_t thisLen = strlen(str);  // 获取源字符串长度
		size_t patternLen = strlen(pattern);  // 获取模式字符串长度
		if (thisLen < patternLen || patternLen == 0)  // 检查长度是否满足要求
			return false;  // 长度不满足要求，返回false

		const char* s = str + (thisLen - patternLen);  // 计算需要比较的字符串起始位置

		if (ignoreCase)  // 如果需要忽略大小写
		{
#ifdef _MSC_VER  // Windows平台
			return _strnicmp(s, pattern, patternLen) == 0;  // 使用Windows API进行不区分大小写的比较
#else  // Unix/Linux平台
			return strncasecmp(s, pattern, patternLen) == 0;  // 使用Unix API进行不区分大小写的比较
#endif
		}
		else  // 如果区分大小写
		{
			return strncmp(s, pattern, patternLen) == 0;  // 使用标准C函数进行区分大小写的比较
		}
	}

	/**
	 * @brief 标准化路径，统一使用正斜杠，目录末尾添加斜杠
	 * @param init 初始路径字符串
	 * @param bIsDir 是否为目录，默认为true
	 * @return std::string 返回标准化后的路径
	 * 
	 * 该函数将路径中的反斜杠替换为正斜杠，确保跨平台兼容性。
	 * 如果指定为目录，会在末尾添加斜杠。
	 */
	static inline std::string standardisePath( const std::string &init, bool bIsDir = true)
	{
		std::string path = init;  // 创建路径字符串副本

		std::replace( path.begin(), path.end(), '\\', '/' );  // 将所有反斜杠替换为正斜杠
		if (path[path.length() - 1] != '/' && bIsDir)  // 如果是目录且末尾没有斜杠
			path += '/';  // 在末尾添加斜杠

		return std::move(path);  // 使用移动语义返回结果
	}

	/**
	 * @brief 分割完整文件名为基本名称和路径
	 * @param qualifiedName 完整的文件名（包含路径）
	 * @param outBasename 输出参数，存储基本文件名
	 * @param outPath 输出参数，存储路径部分
	 * 
	 * 该函数将完整的文件名分割为基本名称和路径两部分。
	 * 路径部分会被标准化，统一使用正斜杠。
	 * 
	 * 注意：如果输入不包含路径分隔符，路径部分为空，基本名称部分为整个输入。
	 */
	static inline void splitFilename(const std::string& qualifiedName,std::string& outBasename, std::string& outPath)
	{
		std::string path = qualifiedName;  // 创建路径字符串副本
		// Replace \ with / first
		std::replace( path.begin(), path.end(), '\\', '/' );  // 将所有反斜杠替换为正斜杠
		// split based on final /
		size_t i = path.find_last_of('/');  // 查找最后一个斜杠的位置

		if (i == std::string::npos)  // 如果没有找到斜杠
		{
			outPath = "";  // 路径部分为空
			outBasename = qualifiedName;  // 基本名称部分为整个输入
		}
		else  // 如果找到了斜杠
		{
			outBasename = path.substr(i+1, path.size() - i - 1);  // 提取斜杠之后的基本名称
			outPath = path.substr(0, i+1);  // 提取斜杠之前的路径部分（包含斜杠）
		}
	}

	/**
	 * @brief 简单的模式匹配，支持通配符
	 * @param str 要测试的字符串
	 * @param pattern 匹配模式，可以包含简单的*通配符
	 * @param caseSensitive 是否区分大小写，默认为true
	 * @return bool 匹配成功返回true，失败返回false
	 * 
	 * 该函数实现了简单的通配符模式匹配，支持*通配符。
	 * 如果不区分大小写，会先将字符串和模式都转换为小写。
	 * 使用迭代器进行高效的字符串匹配。
	 */
	static inline bool match(const std::string& str, const std::string& pattern, bool caseSensitive = true)
	{
		std::string tmpStr = str;  // 创建字符串副本
		std::string tmpPattern = pattern;  // 创建模式副本
		if (!caseSensitive)  // 如果不区分大小写
		{
			toLowerCase(tmpStr);  // 将字符串转换为小写
			toLowerCase(tmpPattern);  // 将模式转换为小写
		}

		std::string::const_iterator strIt = tmpStr.begin();  // 字符串迭代器
		std::string::const_iterator patIt = tmpPattern.begin();  // 模式迭代器
		std::string::const_iterator lastWildCardIt = tmpPattern.end();  // 最后一个通配符位置
		while (strIt != tmpStr.end() && patIt != tmpPattern.end())  // 当两个迭代器都未到达末尾时继续
		{
			if (*patIt == '*')  // 如果当前模式字符是通配符
			{
				lastWildCardIt = patIt;  // 记录通配符位置
				// Skip over looking for next character
				++patIt;  // 移动到下一个模式字符
				if (patIt == tmpPattern.end())  // 如果通配符是模式的最后一个字符
				{
					// Skip right to the end since * matches the entire rest of the string
					strIt = tmpStr.end();  // 通配符匹配整个剩余字符串
				}
				else  // 如果通配符后面还有字符
				{
					// scan until we find next pattern character
					while(strIt != tmpStr.end() && *strIt != *patIt)  // 扫描直到找到下一个模式字符
						++strIt;  // 移动字符串迭代器
				}
			}
			else  // 如果当前模式字符不是通配符
			{
				if (*patIt != *strIt)  // 如果模式字符和字符串字符不匹配
				{
					if (lastWildCardIt != tmpPattern.end())  // 如果之前有通配符
					{
						// The last wildcard can match this incorrect sequence
						// rewind pattern to wildcard and keep searching
						patIt = lastWildCardIt;  // 将模式迭代器回退到通配符位置
						lastWildCardIt = tmpPattern.end();  // 清除通配符位置记录
					}
					else  // 如果没有通配符
					{
						// no wildwards left
						return false;  // 匹配失败
					}
				}
				else  // 如果模式字符和字符串字符匹配
				{
					++patIt;  // 移动模式迭代器
					++strIt;  // 移动字符串迭代器
				}
			}

		}
		// If we reached the end of both the pattern and the string, we succeeded
		if (patIt == tmpPattern.end() && strIt == tmpStr.end())  // 如果两个迭代器都到达末尾
		{
			return true;  // 匹配成功
		}
		else  // 否则
		{
			return false;  // 匹配失败
		}
	}

	/**
	 * @brief 返回空白字符串常量
	 * @return const std::string 返回空白字符串的常量引用
	 * 
	 * 该函数返回一个静态的空白字符串常量，用于返回引用但本地变量不存在的情况。
	 * 使用静态变量确保字符串的生命周期。
	 */
	static inline const std::string BLANK()
	{
		static const std::string temp = std::string("");  // 静态空白字符串常量
		return std::move(temp);  // 使用移动语义返回结果
	}

	/**
	 * @brief 格式化字符串输出（类似printf）
	 * @param pszFormat 格式字符串
	 * @param ... 可变参数列表
	 * @return std::string 返回格式化后的字符串
	 * 
	 * 该函数实现了类似C语言printf的字符串格式化功能。
	 * 使用可变参数和va_list处理参数列表。
	 * 
	 * 注意：这是对std::string没有Format函数的补充实现。
	 */
	static inline std::string printf(const char *pszFormat, ...)
	{
		va_list argptr;  // 声明可变参数列表
		va_start(argptr, pszFormat);  // 初始化可变参数列表
		std::string result=printf(pszFormat,argptr);  // 调用重载版本处理参数
		va_end(argptr);  // 结束可变参数列表
		return std::move(result);  // 使用移动语义返回结果
	}

	/**
	 * @brief 格式化字符串输出（类似printf，重载版本）
	 * @param pszFormat 格式字符串
	 * @param ... 可变参数列表
	 * @return std::string 返回格式化后的字符串
	 * 
	 * 该函数是printf的重载版本，使用不同的实现方式。
	 * 适用于需要不同格式化行为的场景。
	 */
	static inline std::string printf2(const char *pszFormat, ...)
	{
		va_list argptr;  // 声明可变参数列表
		va_start(argptr, pszFormat);  // 初始化可变参数列表
		std::string result=printf2(pszFormat,argptr);  // 调用重载版本处理参数
		va_end(argptr);  // 结束可变参数列表
		return std::move(result);  // 使用移动语义返回结果
	}

	/**
	 * @brief 格式化字符串输出（使用va_list）
	 * @param pszFormat 格式字符串
	 * @param argptr 可变参数列表
	 * @return std::string 返回格式化后的字符串
	 * 
	 * 该函数使用va_list处理可变参数，实现字符串格式化。
	 * 使用动态缓冲区分配，自动调整大小以适应格式化结果。
	 * 支持跨平台的vsnprintf函数。
	 */
	static inline std::string printf2(const char *pszFormat,va_list argptr)
	{
		int         size   = 1024;  // 初始缓冲区大小
		char*       buffer = new char[size];  // 分配初始缓冲区

		while (1)  // 无限循环，直到格式化成功
		{
#ifdef _MSC_VER  // Windows平台
			int n = _vsnprintf(buffer, size, pszFormat, argptr);  // 使用Windows API进行格式化
#else  // Unix/Linux平台
			int n = vsnprintf(buffer, size, pszFormat, argptr);  // 使用Unix API进行格式化
#endif

			// If that worked, return a string.
			if (n > -1 && n < size)  // 如果格式化成功且结果长度在缓冲区范围内
			{
				std::string s(buffer);  // 创建字符串对象
				delete [] buffer;  // 释放缓冲区
				return s;  // 返回结果字符串
			}

			if (n > -1)     size  = n+1; // ISO/IEC 9899:1999  // 如果返回长度有效，设置新大小
			else            size *= 2;   // twice the old size  // 否则将大小翻倍

			delete [] buffer;  // 释放旧缓冲区
			buffer = new char[size];  // 分配新缓冲区
		}
	}

	/**
	 * @brief 扩展字符串到指定长度，通过添加空格实现居中对齐
	 * @param str 要扩展的字符串
	 * @param length 目标长度
	 * @return std::string 返回扩展后的字符串
	 * 
	 * 该函数通过在前端和后端添加空格，将字符串扩展到指定长度。
	 * 空格分布尽量均匀，实现居中对齐效果。
	 */
	static inline std::string extend(const char* str, uint32_t length)
	{
		if(strlen(str) >= length)  // 如果字符串已经达到或超过目标长度
			return str;  // 直接返回原字符串

		std::string ret = str;  // 创建字符串副本
		uint32_t spaces = length - (uint32_t)strlen(str);  // 计算需要添加的空格数量
		uint32_t former = spaces/2;  // 前端空格数量（一半）
		uint32_t after = spaces - former;  // 后端空格数量（剩余部分）
		for(uint32_t i = 0; i < former; i++)  // 在前端添加空格
		{
			ret.insert(0, " ");  // 在开头插入空格
		}

		for(uint32_t i = 0; i < after; i++)  // 在后端添加空格
		{
			ret += " ";  // 在末尾添加空格
		}
		return std::move(ret);  // 使用移动语义返回结果
	}

	/**
	 * @brief 格式化字符串输出（使用va_list，重载版本）
	 * @param pszFormat 格式字符串
	 * @param argptr 可变参数列表
	 * @return std::string 返回格式化后的字符串
	 * 
	 * 该函数是printf的重载版本，使用不同的实现方式。
	 * 使用字符串的resize方法动态调整大小，避免手动内存管理。
	 */
	static inline std::string printf(const char* pszFormat, va_list argptr)
	{
		int size = 1024;  // 初始缓冲区大小
		int len=0;  // 格式化结果长度
		std::string ret;  // 返回字符串对象
		for ( ;; )  // 无限循环，直到格式化成功
		{
			ret.resize(size + 1,0);  // 调整字符串大小，多分配一个字符用于结束符
			char *buf=(char *)ret.c_str();   // 获取字符串的C风格指针
			if ( !buf )  // 如果获取指针失败
			{
				return BLANK();  // 返回空白字符串
			}

			va_list argptrcopy;  // 声明参数列表副本
			va_copy(argptrcopy, argptr);  // 复制参数列表

#ifdef _MSC_VER  // Windows平台
			len = _vsnprintf(buf, size, pszFormat, argptrcopy);  // 使用Windows API进行格式化
#else  // Unix/Linux平台
			len = vsnprintf(buf, size, pszFormat, argptrcopy);  // 使用Unix API进行格式化
#endif
			va_end(argptrcopy);  // 结束参数列表副本

			if ( len >= 0 && len <= size )  // 如果格式化成功且结果长度在范围内
			{
				// ok, there was enough space
				break;  // 跳出循环
			}
			size *= 2;  // 将大小翻倍，继续尝试
		}
		ret.resize(len);  // 调整字符串到实际长度
		return std::move(ret);  // 使用移动语义返回结果
	}

	/**
	 * @brief 获取字符串右边的N个字符
	 * @param src 源字符串
	 * @param nCount 要获取的字符数量
	 * @return std::string 返回右边的N个字符
	 * 
	 * 该函数从字符串末尾开始，提取指定数量的字符。
	 * 如果请求的字符数量超过字符串长度，返回空白字符串。
	 */
	static inline std::string right(const std::string &src,size_t nCount)
	{
		if(nCount>src.length())  // 如果请求的字符数量超过字符串长度
			return BLANK();  // 返回空白字符串
		return std::move(src.substr(src.length()-nCount,nCount));  // 使用移动语义返回右边的N个字符
	}

	/**
	 * @brief 获取字符串左边的N个字符
	 * @param src 源字符串
	 * @param nCount 要获取的字符数量
	 * @return std::string 返回左边的N个字符
	 * 
	 * 该函数从字符串开头开始，提取指定数量的字符。
	 * 如果请求的字符数量超过字符串长度，会返回整个字符串。
	 */
	static inline std::string left(const std::string &src,size_t nCount)
	{
		return std::move(src.substr(0,nCount));  // 使用移动语义返回左边的N个字符
	}

	/**
	 * @brief 统计字符串中指定字符的出现次数
	 * @param src 源字符串
	 * @param ch 要统计的字符
	 * @return size_t 返回字符出现的次数
	 * 
	 * 该函数遍历字符串，统计指定字符的出现次数。
	 * 使用简单的循环实现，适用于短字符串。
	 */
	static inline size_t charCount(const std::string &src,char ch)
	{
		size_t result=0;  // 初始化计数器
		for(size_t i=0;i<src.length();i++)  // 遍历字符串的每个字符
		{
			if(src[i]==ch)result++;  // 如果字符匹配，增加计数器
		}
		return result;  // 返回统计结果
	}

	/**
	 * @brief 替换字符串中的指定子字符串
	 * @param str 要处理的字符串引用
	 * @param src 要替换的源子字符串
	 * @param des 替换后的目标子字符串
	 * 
	 * 该函数将字符串中所有的源子字符串替换为目标子字符串。
	 * 使用find方法查找所有匹配位置，然后进行替换。
	 * 
	 * 注意：该函数直接修改传入的字符串引用。
	 */
	static inline void replace(std::string& str, const char* src, const char* des)
	{
		std::string ret = "";  // 创建结果字符串
		std::size_t srcLen = strlen(src);  // 获取源子字符串长度
		std::size_t lastPos = 0;  // 上次处理位置
		std::size_t pos = str.find(src);  // 查找第一个匹配位置
		while(pos != std::string::npos)  // 当找到匹配时继续循环
		{
			ret += str.substr(lastPos, pos-lastPos);  // 添加匹配位置之前的部分
			ret += des;  // 添加目标子字符串

			lastPos = pos + srcLen;  // 更新上次处理位置
			pos = str.find(src, lastPos);  // 从上次处理位置之后查找下一个匹配
		}
		ret += str.substr(lastPos, pos);  // 添加最后一个匹配之后的部分

		str = ret;  // 将结果赋值给原字符串
	}
};
