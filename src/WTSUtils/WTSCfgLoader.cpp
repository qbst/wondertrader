/*!
 * \file WTSCfgLoader.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 配置文件加载器实现
 * 
 * 设计逻辑与作用：
 * 这个文件实现了WonderTrader配置文件加载器的具体功能，提供JSON和YAML两种
 * 格式的配置文件解析能力。该实现采用了第三方库（RapidJSON和yaml-cpp）
 * 来处理复杂的解析逻辑，并提供了完善的编码转换和错误处理机制。
 * 
 * 核心实现特点：
 * 1. 递归解析：支持嵌套的对象和数组结构
 * 2. 类型识别：自动识别和转换不同的数据类型
 * 3. 编码处理：智能检测UTF-8和GBK编码并自动转换
 * 4. 错误容错：提供完善的解析错误处理
 * 5. 内存管理：正确管理WTSVariant对象的生命周期
 */

#include "WTSCfgLoader.h"                  // 配置加载器头文件
#include "../Share/StrUtil.hpp"            // 字符串工具类
#include "../Share/StdUtils.hpp"           // 标准工具类

#include "../Share/charconv.hpp"           // 字符编码转换工具

#include "../Includes/WTSVariant.hpp"      // WonderTrader变体类
#include <rapidjson/document.h>            // RapidJSON文档解析器
namespace rj = rapidjson;                  // RapidJSON命名空间别名

/**
 * @brief JSON值转换为WTSVariant对象的递归函数
 * 
 * 这是一个递归函数，用于将RapidJSON解析的JSON值转换为WTSVariant对象。
 * 支持对象、数组、数字、字符串、布尔值等所有JSON数据类型。
 * 
 * @param root RapidJSON值对象
 * @param params 目标WTSVariant对象指针
 * @return bool 转换成功返回true，失败返回false
 */
bool json_to_variant(const rj::Value& root, WTSVariant* params)
{
	// 类型匹配检查：确保JSON类型与WTSVariant类型一致
	if (root.IsObject() && params->type() != WTSVariant::VT_Object)
		return false;                         // JSON对象必须对应WTSVariant对象类型

	if (root.IsArray() && params->type() != WTSVariant::VT_Array)
		return false;                         // JSON数组必须对应WTSVariant数组类型

	if (root.IsObject())                      // 处理JSON对象
	{
		for (auto& m : root.GetObject())      // 遍历对象的所有成员
		{
			const char* key = m.name.GetString();     // 获取键名
			const rj::Value& item = m.value;          // 获取键值
			switch (item.GetType())                   // 根据值类型进行处理
			{
			case rj::kObjectType:                     // 嵌套对象类型
			{
				WTSVariant* subObj = WTSVariant::createObject();  // 创建子对象
				if (json_to_variant(item, subObj))    // 递归转换子对象
					params->append(key, subObj, false);  // 添加到父对象中
			}
			break;
			case rj::kArrayType:                      // 数组类型
			{
				WTSVariant* subAy = WTSVariant::createArray();   // 创建子数组
				if (json_to_variant(item, subAy))     // 递归转换子数组
					params->append(key, subAy, false);   // 添加到父对象中
			}
			break;
			case rj::kNumberType:                     // 数字类型（需要细分处理）
				if (item.IsInt())                     // 32位整数
					params->append(key, item.GetInt());
				else if (item.IsUint())               // 32位无符号整数
					params->append(key, item.GetUint());
				else if (item.IsInt64())              // 64位整数
					params->append(key, item.GetInt64());
				else if (item.IsUint64())             // 64位无符号整数
					params->append(key, item.GetUint64());
				else if (item.IsDouble())             // 双精度浮点数
					params->append(key, item.GetDouble());
				break;
			case rj::kStringType:                     // 字符串类型
				params->append(key, item.GetString());
				break;
			case rj::kTrueType:                       // 布尔值true
			case rj::kFalseType:                      // 布尔值false
				params->append(key, item.GetBool());
				break;
			}
		}
	}
	else                                          // 处理JSON数组
	{
		for (auto& item : root.GetArray())        // 遍历数组的所有元素
		{
			switch (item.GetType())               // 根据元素类型进行处理
			{
			case rj::kObjectType:                 // 数组元素为对象
			{
				WTSVariant* subObj = WTSVariant::createObject();  // 创建子对象
				if (json_to_variant(item, subObj))    // 递归转换子对象
					params->append(subObj, false);       // 添加到数组中
			}
			break;
			case rj::kArrayType:                  // 数组元素为数组（嵌套数组）
			{
				WTSVariant* subAy = WTSVariant::createArray();   // 创建子数组
				if (json_to_variant(item, subAy))     // 递归转换子数组
					params->append(subAy, false);        // 添加到父数组中
			}
			break;
			case rj::kNumberType:                 // 数组元素为数字
				if (item.IsInt())                 // 处理各种数字类型
					params->append(item.GetInt());
				else if (item.IsUint())
					params->append(item.GetUint());
				else if (item.IsInt64())
					params->append(item.GetInt64());
				else if (item.IsUint64())
					params->append(item.GetUint64());
				else if (item.IsDouble())
					params->append(item.GetDouble());
				break;
			case rj::kStringType:                 // 数组元素为字符串
				params->append(item.GetString());
				break;
			case rj::kTrueType:                   // 数组元素为布尔值
			case rj::kFalseType:
				params->append(item.GetBool());
				break;
			}
		}
	}
	return true;                                  // 转换成功
}

/**
 * @brief 从JSON内容加载配置的实现方法
 * 
 * 使用RapidJSON库解析JSON字符串，并转换为WTSVariant对象。
 * 
 * @param content JSON格式的字符串内容
 * @return WTSVariant* 解析成功返回配置对象指针，失败返回NULL
 */
WTSVariant* WTSCfgLoader::load_from_json(const char* content)
{
	rj::Document root;                            // 创建RapidJSON文档对象
	root.Parse(content);                          // 解析JSON字符串

	if (root.HasParseError())                     // 检查解析是否有错误
		return NULL;                              // 解析失败返回NULL

	WTSVariant* ret = WTSVariant::createObject(); // 创建WTSVariant对象容器
	if (!json_to_variant(root, ret))              // 转换JSON到WTSVariant
	{
		ret->release();                           // 转换失败释放对象
		return NULL;                              // 返回NULL表示失败
	}

	return ret;                                   // 返回成功解析的配置对象
}

#include "../WTSUtils/yamlcpp/yaml.h"        // yaml-cpp库头文件

/**
 * @brief YAML节点转换为WTSVariant对象的递归函数
 * 
 * 这是一个递归函数，用于将yaml-cpp解析的YAML节点转换为WTSVariant对象。
 * 支持映射（对象）、序列（数组）、标量（基本值）等所有YAML数据类型。
 * 
 * @param root YAML节点对象
 * @param params 目标WTSVariant对象指针
 * @return bool 转换成功返回true，失败返回false
 */
bool yaml_to_variant(const YAML::Node& root, WTSVariant* params)
{
	// 类型匹配检查：确保YAML类型与WTSVariant类型一致
	if (root.IsNull() && params->type() != WTSVariant::VT_Object)
		return false;                         // 空节点对应对象类型

	if (root.IsSequence() && params->type() != WTSVariant::VT_Array)
		return false;                         // YAML序列必须对应WTSVariant数组类型

	bool isMap = root.IsMap();                // 判断是否为映射类型
	for (auto& m : root)                      // 遍历YAML节点的所有子节点
	{
		// 根据节点类型获取键名和值
		std::string key = isMap ? m.first.as<std::string>() : "";  // 映射类型有键名
		const YAML::Node& item = isMap ? m.second : m;             // 获取对应的值节点
		
		switch (item.Type())                  // 根据节点类型进行处理
		{
		case YAML::NodeType::Map:             // 嵌套映射类型
		{
			WTSVariant* subObj = WTSVariant::createObject();  // 创建子对象
			if (yaml_to_variant(item, subObj))    // 递归转换子对象
			{
				if(isMap)                         // 如果父节点是映射
					params->append(key.c_str(), subObj, false);  // 以键值对形式添加
				else                              // 如果父节点是序列
					params->append(subObj, false);    // 直接添加到数组中
			}
		}
		break;
		case YAML::NodeType::Sequence:        // 序列类型（数组）
		{
			WTSVariant* subAy = WTSVariant::createArray();   // 创建子数组
			if (yaml_to_variant(item, subAy))     // 递归转换子数组
			{
				if (isMap)                        // 如果父节点是映射
					params->append(key.c_str(), subAy, false);   // 以键值对形式添加
				else                              // 如果父节点是序列
					params->append(subAy, false);     // 直接添加到数组中
			}
		}
		break;
		case YAML::NodeType::Scalar:          // 标量类型（基本值）
			if (isMap)                            // 如果父节点是映射
				params->append(key.c_str(), item.as<std::string>().c_str());  // 以键值对形式添加
			else                                  // 如果父节点是序列
				params->append(item.as<std::string>().c_str());               // 直接添加到数组中
			break;
		}
	}

	return true;                              // 转换成功
}

/**
 * @brief 从YAML内容加载配置的实现方法
 * 
 * 使用yaml-cpp库解析YAML字符串，并转换为WTSVariant对象。
 * 
 * @param content YAML格式的字符串内容
 * @return WTSVariant* 解析成功返回配置对象指针，失败返回NULL
 */
WTSVariant* WTSCfgLoader::load_from_yaml(const char* content)
{
	YAML::Node root = YAML::Load(content);    // 使用yaml-cpp加载YAML内容

	if (root.IsNull())                        // 检查是否加载成功
		return NULL;                          // 加载失败返回NULL

	WTSVariant* ret = WTSVariant::createObject(); // 创建WTSVariant对象容器
	if (!yaml_to_variant(root, ret))          // 转换YAML到WTSVariant
	{
		ret->release();                       // 转换失败释放对象
		return NULL;                          // 返回NULL表示失败
	}

	return ret;                               // 返回成功解析的配置对象
}

/**
 * @brief 从内容字符串加载配置的实现方法
 * 
 * 从内存中的字符串内容加载配置，自动处理编码转换。
 * 支持UTF-8和GBK编码的自动检测和转换。
 * 
 * @param content 配置内容字符串
 * @param isYaml 是否为YAML格式，默认为false（JSON格式）
 * @return WTSVariant* 加载成功返回配置对象指针，失败返回NULL
 */
WTSVariant* WTSCfgLoader::load_from_content(const std::string& content, bool isYaml /* = false */)
{
	// 添加自动检测编码的逻辑
	bool isUTF8 = EncodingHelper::isUtf8((unsigned char*)content.data(), content.size());

	std::string buffer;                       // 编码转换后的内容缓冲区
	// 根据平台进行编码转换
	// Linux平台使用UTF-8编码
	// Windows平台使用GBK编码
#ifdef _WIN32
	if (isUTF8)                               // Windows下如果是UTF-8则转换为GBK
		buffer = UTF8toChar(content);
#else
	if (!isUTF8)                              // Linux下如果不是UTF-8则转换为UTF-8
		buffer = ChartoUTF8(content);
#endif

	if (buffer.empty())                       // 如果转换后为空，使用原始内容
		buffer = content;

	// 根据指定格式调用相应的解析方法
	if (isYaml)
		return load_from_yaml(buffer.c_str()); // 解析YAML格式
	else
		return load_from_json(buffer.c_str()); // 解析JSON格式
}

/**
 * @brief 从文件加载配置的实现方法
 * 
 * 从指定文件路径读取配置文件内容，根据文件扩展名自动识别格式。
 * 支持编码自动检测和转换。
 * 
 * @param filename 配置文件路径
 * @return WTSVariant* 加载成功返回配置对象指针，失败返回NULL
 */
WTSVariant* WTSCfgLoader::load_from_file(const char* filename)
{
	if (!StdFile::exists(filename))           // 检查文件是否存在
		return NULL;                          // 文件不存在返回NULL

	std::string content;                      // 文件内容缓冲区
	StdFile::read_file_content(filename, content);  // 读取文件内容
	if (content.empty())                      // 检查文件内容是否为空
		return NULL;                          // 内容为空返回NULL

	// 添加自动检测编码的逻辑
	bool isUTF8 = EncodingHelper::isUtf8((unsigned char*)content.data(), content.size());

	// By Wesley @ 2022.01.07
	// 根据平台进行编码转换
	// Linux平台使用UTF-8编码
	// Windows平台使用GBK编码
#ifdef _WIN32
	if(isUTF8)                                // Windows下如果是UTF-8则转换为GBK
		content = UTF8toChar(content);
#else
	if (!isUTF8)                              // Linux下如果不是UTF-8则转换为UTF-8
		content = ChartoUTF8(content);
#endif

	// 根据文件扩展名自动识别配置文件格式
	if (StrUtil::endsWith(filename, ".json"))
		return load_from_json(content.c_str()); // 解析JSON格式文件
	else if (StrUtil::endsWith(filename, ".yaml") || StrUtil::endsWith(filename, ".yml"))
		return load_from_yaml(content.c_str()); // 解析YAML格式文件

	return NULL;                              // 不支持的文件格式返回NULL
}
