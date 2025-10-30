/*!
 * \file WtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据伺服器辅助工具类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtHelper（辅助工具）类，是WonderTrader数据伺服器模块中的辅助工具类，
 * 主要用于提供模块路径管理和当前工作目录查询功能。该类采用单例模式设计，提供静态
 * 方法访问，确保整个应用程序中模块路径的统一管理。
 * 
 * 核心设计理念：
 * 
 * 1. 模块路径管理（Module Path Management）：
 *    - 统一管理二进制模块的目录路径
 *    - 支持动态设置和获取模块目录
 *    - 便于动态库加载时的路径定位
 * 
 * 2. 工作目录查询（Working Directory Query）：
 *    - 提供当前工作目录的查询功能
 *    - 使用静态变量缓存结果，避免重复系统调用
 *    - 路径标准化处理，确保跨平台兼容性
 * 
 * 主要功能模块：
 * 
 * 1. 模块目录管理：
 *    - get_module_dir()：获取模块目录路径
 *    - set_module_dir()：设置模块目录路径
 *    - 使用静态成员变量存储模块路径
 * 
 * 2. 工作目录查询：
 *    - get_cwd()：获取当前工作目录
 *    - 跨平台支持（Windows和Unix）
 *    - 路径标准化处理
 * 
 * 使用场景：
 * - 动态库加载时的路径定位
 * - 配置文件路径的构建
 * - 日志文件路径的确定
 * - 数据文件路径的规范化
 * 
 * 技术特点：
 * - 单例模式设计，全局唯一实例
 * - 线程安全的静态方法
 * - 跨平台兼容性
 * - 路径标准化处理
 * 
 * 注意事项：
 * - 模块目录应在初始化时设置
 * - 工作目录在首次调用时缓存
 * - 路径使用标准化的路径分隔符
 */
#pragma once                                                                     // 防止头文件重复包含
#include <string>                                                                // 包含标准字符串类
#include <stdint.h>                                                              // 包含标准整数类型定义

/**
 * @class WtHelper
 * @brief WonderTrader辅助工具类
 * 
 * 提供模块路径管理和工作目录查询功能的辅助工具类。
 * 所有方法都是静态方法，使用单例模式确保全局唯一性。
 */
class WtHelper
{
public:
	/**
	 * @brief 获取当前工作目录
	 * @return 当前工作目录的字符串指针（标准化路径格式）
	 * 
	 * 获取当前程序运行的工作目录路径。
	 * 首次调用时会查询系统并缓存结果，后续调用直接返回缓存值。
	 * 路径已经过标准化处理，使用统一的路径分隔符。
	 */
	static const char* get_cwd();

	/**
	 * @brief 获取模块目录路径
	 * @return 模块目录的字符串指针
	 * 
	 * 获取二进制模块所在的目录路径。
	 * 该路径通常用于定位动态库、配置文件等资源文件。
	 * 需要在初始化时通过set_module_dir()设置。
	 */
	static const char* get_module_dir(){ return _bin_dir.c_str(); }

	/**
	 * @brief 设置模块目录路径
	 * @param mod_dir 模块目录路径字符串
	 * 
	 * 设置二进制模块所在的目录路径。
	 * 通常在程序初始化时调用，用于指定动态库等资源的查找路径。
	 */
	static void set_module_dir(const char* mod_dir){ _bin_dir = mod_dir; }

private:
	static std::string	_bin_dir;                                                // 静态成员变量：存储二进制模块目录路径
};

