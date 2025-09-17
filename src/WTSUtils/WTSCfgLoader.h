/*!
 * \file WTSCfgLoader.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 配置文件加载器
 * 
 * 设计逻辑与作用：
 * 这个文件定义了WonderTrader系统的配置文件加载器，是系统配置管理的核心组件。
 * 该加载器支持多种配置文件格式，提供统一的配置数据访问接口。
 * 
 * 主要功能特性：
 * 1. 多格式支持：支持JSON和YAML两种主流配置文件格式
 * 2. 编码处理：自动检测和转换文件编码（UTF-8/GBK）
 * 3. 统一接口：将不同格式的配置转换为统一的WTSVariant对象
 * 4. 灵活加载：支持从文件和内存内容两种方式加载配置
 * 5. 错误处理：提供完善的解析错误处理机制
 * 
 * 在量化交易系统中的应用：
 * - 策略配置：加载交易策略的参数配置
 * - 系统设置：读取系统运行参数和环境配置
 * - 连接配置：管理行情和交易接口的连接参数
 * - 用户偏好：保存和读取用户的个性化设置
 * - 风控参数：加载风险控制相关的配置信息
 */
#pragma once

#include "../Includes/WTSMarcos.h"         // WonderTrader宏定义
#include <string>                          // C++标准字符串类

// 前向声明WTSVariant类
NS_WTP_BEGIN
class WTSVariant;                          // 配置数据容器类
NS_WTP_END

USING_NS_WTP;                             // 使用WonderTrader命名空间

/**
 * @brief 配置文件加载器类
 * 
 * 提供统一的配置文件加载接口，支持JSON和YAML格式的配置文件。
 * 所有方法都是静态方法，可以直接通过类名调用。
 */
class WTSCfgLoader
{
	/**
	 * @brief 从JSON内容加载配置（私有方法）
	 * 
	 * 解析JSON格式的字符串内容，转换为WTSVariant对象。
	 * 
	 * @param content JSON格式的字符串内容
	 * @return WTSVariant* 解析成功返回配置对象指针，失败返回NULL
	 */
	static WTSVariant*	load_from_json(const char* content);
	
	/**
	 * @brief 从YAML内容加载配置（私有方法）
	 * 
	 * 解析YAML格式的字符串内容，转换为WTSVariant对象。
	 * 
	 * @param content YAML格式的字符串内容
	 * @return WTSVariant* 解析成功返回配置对象指针，失败返回NULL
	 */
	static WTSVariant*	load_from_yaml(const char* content);

public:
	/**
	 * @brief 从文件加载配置
	 * 
	 * 根据文件扩展名自动识别配置文件格式并加载。
	 * 支持.json、.yaml、.yml扩展名的文件。
	 * 
	 * @param filename 配置文件路径（C风格字符串）
	 * @return WTSVariant* 加载成功返回配置对象指针，失败返回NULL
	 */
	static WTSVariant*	load_from_file(const char* filename);
	
	/**
	 * @brief 从内容字符串加载配置
	 * 
	 * 从内存中的字符串内容加载配置，支持指定格式类型。
	 * 
	 * @param content 配置内容字符串
	 * @param isYaml 是否为YAML格式，默认为false（JSON格式）
	 * @return WTSVariant* 加载成功返回配置对象指针，失败返回NULL
	 */
	static WTSVariant*	load_from_content(const std::string& content, bool isYaml = false);

	/**
	 * @brief 从文件加载配置（C++字符串版本）
	 * 
	 * C++字符串版本的文件加载接口，内部调用C风格字符串版本。
	 * 
	 * @param filename 配置文件路径（C++字符串）
	 * @return WTSVariant* 加载成功返回配置对象指针，失败返回NULL
	 */
	static WTSVariant*	load_from_file(const std::string& filename)
	{
		return load_from_file(filename.c_str());  // 转换为C风格字符串调用
	}
};

