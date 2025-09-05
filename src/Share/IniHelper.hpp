/*!
 * \file IniHelper.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Ini文件辅助类,利用boost的property_tree来实现,可以跨平台使用
 * 
 * 该文件提供了基于Boost.PropertyTree的INI配置文件操作工具，主要包括：
 * 1. INI文件的加载、保存和解析
 * 2. 配置项的读取和写入（支持多种数据类型）
 * 3. 配置节和键的管理（添加、删除、遍历）
 * 4. 跨平台的INI文件操作支持
 * 
 * 设计逻辑：
 * - 基于Boost.PropertyTree库实现高性能的INI文件解析
 * - 使用模板编程提供类型安全的配置项访问
 * - 通过异常处理确保操作的健壮性
 * - 支持配置节和键的层次化访问
 * - 提供默认值机制，增强容错能力
 * 
 * 主要作用：
 * - 为WonderTrader框架提供配置文件管理能力
 * - 支持交易策略、系统参数等配置的持久化
 * - 提供统一的配置接口，简化配置管理代码
 */
#pragma once  // 防止头文件重复包含

#include <string>   // 包含字符串处理功能
#include <vector>   // 包含动态数组功能
#include <map>      // 包含映射容器功能

#include <boost/property_tree/ptree.hpp>      // 包含Boost属性树核心功能
#include <boost/property_tree/ini_parser.hpp> // 包含Boost INI解析器功能

typedef std::vector<std::string>			FieldArray;  // 字段数组类型定义，用于存储字符串列表
typedef std::map<std::string, std::string>	FieldMap;    // 字段映射类型定义，用于存储键值对

/**
 * @class IniHelper
 * @brief INI配置文件辅助工具类
 * 
 * 该类封装了Boost.PropertyTree库，提供完整的INI文件操作功能，
 * 包括文件的加载、保存、配置项的读写、配置节的管理等。
 * 支持跨平台使用，提供异常安全的操作接口。
 */
class IniHelper
{
private:
	boost::property_tree::ptree	_root;  // 属性树根节点，存储所有配置数据
	std::string					_fname;  // 当前加载的INI文件名
	bool						_loaded;  // 标记是否已成功加载配置文件

	static const uint32_t MAX_KEY_LENGTH = 64;  // 配置项键名的最大长度限制

public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化IniHelper对象，设置加载状态为false。
	 */
	IniHelper(): _loaded(false){}  // 初始化加载状态为false

	/**
	 * @brief 加载INI配置文件
	 * @param szFile INI文件路径
	 * 
	 * 该函数使用Boost.PropertyTree解析INI文件，将配置数据加载到内存中。
	 * 使用异常处理确保加载过程的健壮性，即使文件不存在也不会崩溃。
	 */
	void	load(const char* szFile)
	{
		_fname = szFile;  // 保存文件名到成员变量
		try  // 使用异常处理确保安全性
		{
			boost::property_tree::ini_parser::read_ini(szFile, _root);  // 使用Boost解析器读取INI文件
		}
		catch(...)  // 捕获所有异常
		{
			// 异常处理：静默忽略，确保程序继续运行
		}
		
		_loaded = true;  // 标记文件已加载（无论成功与否）
	}

	/**
	 * @brief 保存INI配置文件
	 * @param filename 目标文件名，为空时使用原文件名
	 * 
	 * 该函数将内存中的配置数据保存到INI文件中。
	 * 如果未指定文件名，则使用加载时的原文件名。
	 */
	void	save(const char* filename = "")
	{
		if (strlen(filename) > 0)  // 检查是否指定了新的文件名
			boost::property_tree::ini_parser::write_ini(filename, _root);  // 保存到指定文件名
		else  // 未指定新文件名
			boost::property_tree::ini_parser::write_ini(_fname.c_str(), _root);  // 保存到原文件名
	}

	/**
	 * @brief 检查配置文件是否已加载
	 * @return bool 已加载返回true，未加载返回false
	 * 
	 * 该函数返回内部加载状态标记，用于判断配置文件是否可用。
	 */
	inline bool isLoaded() const{ return _loaded; }  // 返回加载状态

public:
	/**
	 * @brief 删除指定配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * 
	 * 该函数删除指定节中的指定配置项。
	 * 使用异常处理确保操作的安全性，即使节或键不存在也不会崩溃。
	 */
	void	removeValue(const char* szSec, const char* szKey)
	{
		try  // 使用异常处理确保安全性
		{
			boost::property_tree::ptree& sec = _root.get_child(szSec);  // 获取指定配置节
			sec.erase(szKey);  // 删除指定配置项
		}
		catch (...)  // 捕获所有异常
		{
			// 异常处理：静默忽略，确保程序继续运行
		}
	}

	/**
	 * @brief 删除指定配置节
	 * @param szSec 要删除的配置节名称
	 * 
	 * 该函数删除整个配置节及其下的所有配置项。
	 * 使用异常处理确保操作的安全性。
	 */
	void	removeSection(const char* szSec)
	{
		try  // 使用异常处理确保安全性
		{
			_root.erase(szSec);  // 从根节点删除指定配置节
		}
		catch (...)  // 捕获所有异常
		{
			// 异常处理：静默忽略，确保程序继续运行
		}
	}

	/**
	 * @brief 读取配置项值（模板函数）
	 * @tparam T 数据类型模板参数
	 * @param szPath 配置项路径（格式：节名.键名）
	 * @param defVal 默认值，当配置项不存在时返回
	 * @return T 返回配置项的值，失败时返回默认值
	 * 
	 * 该函数是通用的配置项读取函数，支持任意数据类型。
	 * 使用异常处理确保读取过程的安全性。
	 */
	template<class T>
	T	readValue(const char* szPath, T defVal)
	{
		try  // 使用异常处理确保安全性
		{
			return _root.get<T>(szPath, defVal);  // 从属性树中读取指定类型的值
		}
		catch (...)  // 捕获所有异常
		{
			return defVal;  // 异常情况下返回默认值
		}
	}

	/**
	 * @brief 读取字符串类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param defVal 默认值，默认为空字符串
	 * @return std::string 返回配置项的字符串值
	 * 
	 * 该函数读取指定节和键的字符串配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	std::string	readString(const char* szSec, const char* szKey, const char* defVal = "")
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		return readValue<std::string>(path, defVal);  // 调用模板函数读取字符串值
	}

	/**
	 * @brief 读取整数类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param defVal 默认值，默认为0
	 * @return int 返回配置项的整数值
	 * 
	 * 该函数读取指定节和键的整数配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	int			readInt(const char* szSec, const char* szKey, int defVal = 0)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		return readValue<int>(path, defVal);  // 调用模板函数读取整数值
	}

	/**
	 * @brief 读取无符号整数类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param defVal 默认值，默认为0
	 * @return uint32_t 返回配置项的无符号整数值
	 * 
	 * 该函数读取指定节和键的无符号整数配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	uint32_t	readUInt(const char* szSec, const char* szKey, uint32_t defVal = 0)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		return readValue<uint32_t>(path, defVal);  // 调用模板函数读取无符号整数值
	}

	/**
	 * @brief 读取布尔类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param defVal 默认值，默认为false
	 * @return bool 返回配置项的布尔值
	 * 
	 * 该函数读取指定节和键的布尔配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	bool		readBool(const char* szSec, const char* szKey, bool defVal = false)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		return readValue<bool>(path, defVal);  // 调用模板函数读取布尔值
	}

	/**
	 * @brief 读取双精度浮点数类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param defVal 默认值，默认为0.0
	 * @return double 返回配置项的双精度浮点数值
	 * 
	 * 该函数读取指定节和键的双精度浮点数配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	double		readDouble(const char* szSec, const char* szKey, double defVal = 0.0)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		return readValue<double>(path, defVal);  // 调用模板函数读取双精度浮点数值
	}

	/**
	 * @brief 读取所有配置节名称
	 * @param aySection 输出参数，存储所有配置节名称的数组
	 * @return int 返回配置节的总数量
	 * 
	 * 该函数遍历属性树，获取所有配置节的名称。
	 * 结果存储在传入的数组中，返回配置节的总数。
	 */
	int			readSections(FieldArray &aySection)
	{
		for (auto it = _root.begin(); it != _root.end(); it++)  // 遍历属性树的所有节点
		{
			aySection.emplace_back(it->first.data());  // 将节名添加到输出数组中
		}

		return (int)_root.size();  // 返回配置节的总数量
	}

	/**
	 * @brief 读取指定配置节下的所有键名
	 * @param szSec 配置节名称
	 * @param ayKey 输出参数，存储所有键名的数组
	 * @return int 返回键的总数量，失败时返回0
	 * 
	 * 该函数获取指定配置节下的所有键名。
	 * 使用异常处理确保操作的安全性。
	 */
	int			readSecKeyArray(const char* szSec, FieldArray &ayKey)
	{
		try  // 使用异常处理确保安全性
		{
			const boost::property_tree::ptree& _sec = _root.get_child(szSec);  // 获取指定配置节
			for (auto it = _sec.begin(); it != _sec.end(); it++)  // 遍历该节下的所有配置项
			{
				ayKey.emplace_back(it->first.data());  // 将键名添加到输出数组中
			}

			return (int)_sec.size();  // 返回键的总数量
		}
		catch (...)  // 捕获所有异常
		{
			return 0;  // 异常情况下返回0
		}
		
	}

	/**
	 * @brief 读取指定配置节下的所有键值对
	 * @param szSec 配置节名称
	 * @param ayKey 输出参数，存储所有键名的数组
	 * @param ayVal 输出参数，存储所有值的数组
	 * @return int 返回键值对的总数量，失败时返回0
	 * 
	 * 该函数获取指定配置节下的所有键名和对应的值。
	 * 键名和值分别存储在两个数组中，索引一一对应。
	 * 使用异常处理确保操作的安全性。
	 */
	int			readSecKeyValArray(const char* szSec, FieldArray &ayKey, FieldArray &ayVal)
	{
		try  // 使用异常处理确保安全性
		{
			const boost::property_tree::ptree& _sec = _root.get_child(szSec);  // 获取指定配置节
			for (auto it = _sec.begin(); it != _sec.end(); it++)  // 遍历该节下的所有配置项
			{
				ayKey.emplace_back(it->first.data());  // 将键名添加到键名数组中
				ayVal.emplace_back(it->second.data());  // 将值添加到值数组中
			}

			return (int)_sec.size();  // 返回键值对的总数量
		}
		catch (...)  // 捕获所有异常
		{
			return 0;  // 异常情况下返回0
		}
	}

	/**
	 * @brief 写入配置项值（模板函数）
	 * @tparam T 数据类型模板参数
	 * @param szPath 配置项路径（格式：节名.键名）
	 * @param val 要写入的值
	 * 
	 * 该函数是通用的配置项写入函数，支持任意数据类型。
	 * 如果配置项不存在，会自动创建；如果已存在，会覆盖原值。
	 */
	template<class T>
	void		writeValue(const char* szPath, T val)
	{
		_root.put<T>(szPath, val);  // 向属性树中写入指定类型的值
	}

	/**
	 * @brief 写入字符串类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param val 要写入的字符串值
	 * 
	 * 该函数写入指定节和键的字符串配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	void		writeString(const char* szSec, const char* szKey, const char* val)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		writeValue<std::string>(path, val);  // 调用模板函数写入字符串值
	}

	/**
	 * @brief 写入整数类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param val 要写入的整数值
	 * 
	 * 该函数写入指定节和键的整数配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	void		writeInt(const char* szSec, const char* szKey, int val)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		writeValue<int>(path, val);  // 调用模板函数写入整数值
	}

	/**
	 * @brief 写入无符号整数类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param val 要写入的无符号整数值
	 * 
	 * 该函数写入指定节和键的无符号整数配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	void		writeUInt(const char* szSec, const char* szKey, uint32_t val)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		writeValue<uint32_t>(path, val);  // 调用模板函数写入无符号整数值
	}

	/**
	 * @brief 写入布尔类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param val 要写入的布尔值
	 * 
	 * 该函数写入指定节和键的布尔配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	void		writeBool(const char* szSec, const char* szKey, bool val)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		writeValue<bool>(path, val);  // 调用模板函数写入布尔值
	}

	/**
	 * @brief 写入双精度浮点数类型的配置项
	 * @param szSec 配置节名称
	 * @param szKey 配置项键名
	 * @param val 要写入的双精度浮点数值
	 * 
	 * 该函数写入指定节和键的双精度浮点数配置项。
	 * 使用静态缓冲区构建配置项路径，提高性能。
	 */
	void		writeDouble(const char* szSec, const char* szKey, double val)
	{
		static char path[MAX_KEY_LENGTH] = { 0 };  // 静态缓冲区，用于构建配置项路径
		sprintf(path, "%s.%s", szSec, szKey);  // 构建配置项路径（节名.键名）
		writeValue<double>(path, val);  // 调用模板函数写入双精度浮点数值
	}
};