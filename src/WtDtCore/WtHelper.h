/*!
 * \file WtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtHelper辅助工具类，提供WonderTrader框架中常用的路径管理功能。
 * 该类采用静态方法设计，作为全局工具类使用，主要解决以下问题：
 * 
 * 1. 当前工作目录（CWD）管理：
 *    - 获取程序运行时的当前工作目录
 *    - 提供标准化的路径格式（统一使用'/'分隔符）
 *    - 使用懒加载模式，首次调用时初始化并缓存
 * 
 * 2. 模块目录管理：
 *    - 存储和管理框架模块所在的目录路径
 *    - 用于动态加载各种模块（如Parser、Trader等）
 *    - 支持运行时设置和获取模块路径
 * 
 * 设计特点：
 * - 静态工具类：所有方法均为静态方法，无需实例化
 * - 路径缓存：避免重复的系统调用，提高性能
 * - 跨平台兼容：自动处理Windows和Unix系统的路径差异
 * - 线程安全：静态变量的初始化是线程安全的（C++11标准）
 * 
 * 使用场景：
 * - 动态库加载：构建模块的完整路径
 * - 配置文件定位：相对于工作目录查找配置文件
 * - 日志文件生成：确定日志文件的存储位置
 * - 数据文件访问：定位数据存储目录
 */
#pragma once
#include <string>       // 使用std::string存储路径
#include <stdint.h>     // 使用标准整型定义

/**
 * @class WtHelper
 * @brief WonderTrader辅助工具类
 * 
 * 该类提供了WonderTrader框架中常用的辅助功能，主要是路径管理。
 * 设计为静态工具类，所有方法均为静态方法，无需创建实例即可使用。
 * 
 * 主要功能：
 * 1. 获取当前工作目录（Current Working Directory）
 * 2. 管理模块目录路径（用于动态加载各类模块）
 * 
 * 实现特点：
 * - 采用懒加载模式，延迟初始化
 * - 使用内部缓存避免重复的系统调用
 * - 跨平台兼容（Windows使用_getcwd，Unix使用getcwd）
 * 
 * 使用示例：
 * @code
 *   // 获取当前工作目录
 *   const char* cwd = WtHelper::get_cwd();
 *   
 *   // 设置模块目录
 *   WtHelper::set_module_dir("/usr/local/wondertrader/");
 *   
 *   // 获取模块目录
 *   const char* mod_dir = WtHelper::get_module_dir();
 *   
 *   // 构建模块完整路径
 *   std::string parser_path = std::string(mod_dir) + "parsers/ParserCTP.so";
 * @endcode
 */
class WtHelper
{
public:
	/**
	 * @brief 获取当前工作目录（CWD）
	 * 
	 * 该方法返回程序运行时的当前工作目录。使用懒加载模式，首次调用时
	 * 通过系统API获取当前目录并缓存，后续调用直接返回缓存值。
	 * 
	 * 实现细节：
	 * - Windows平台使用_getcwd()系统调用
	 * - Unix/Linux平台使用getcwd()系统调用
	 * - 路径会被标准化（统一使用'/'作为分隔符）
	 * - 使用静态局部变量缓存，避免重复的系统调用
	 * 
	 * 注意事项：
	 * - 返回的指针指向内部静态字符串，生命周期与程序相同
	 * - 调用者不应该修改或释放返回的指针
	 * - 如果程序运行期间工作目录被改变，本方法返回的仍是首次调用时的目录
	 * 
	 * @return const char* 当前工作目录的C风格字符串指针，以'/'结尾
	 * 
	 * @note 线程安全：C++11保证静态局部变量的初始化是线程安全的
	 */
	static const char* get_cwd();

	/**
	 * @brief 获取模块目录路径
	 * 
	 * 该方法返回WonderTrader框架模块所在的目录路径。模块目录用于存放
	 * 各类动态加载的模块，如行情解析器（Parser）、交易通道（Trader）等。
	 * 
	 * 典型的模块目录结构：
	 * <module_dir>/
	 *   ├── parsers/          # 行情解析器模块
	 *   │   ├── ParserCTP.so
	 *   │   ├── ParserXTP.so
	 *   │   └── ...
	 *   ├── traders/          # 交易通道模块
	 *   │   ├── TraderCTP.so
	 *   │   └── ...
	 *   └── ...
	 * 
	 * 使用场景：
	 * - 动态加载Parser模块：module_dir + "parsers/ParserCTP.so"
	 * - 动态加载Trader模块：module_dir + "traders/TraderCTP.so"
	 * - 动态加载其他插件模块
	 * 
	 * @return const char* 模块目录路径的C风格字符串指针
	 * 
	 * @see set_module_dir() 设置模块目录路径
	 * 
	 * @note 返回的指针指向内部静态字符串，调用者不应该修改或释放
	 */
	static const char* get_module_dir() { return _bin_dir.c_str(); }

	/**
	 * @brief 设置模块目录路径
	 * 
	 * 该方法用于设置WonderTrader框架模块所在的目录路径。通常在程序
	 * 启动初始化阶段调用，设置好模块目录后，后续的动态模块加载会
	 * 从该目录下查找相应的模块文件。
	 * 
	 * 调用时机：
	 * - 程序启动时，解析配置文件后调用
	 * - 在加载任何动态模块之前必须调用
	 * - 通常只在程序初始化时调用一次
	 * 
	 * 路径格式：
	 * - 推荐使用绝对路径，避免相对路径引起的问题
	 * - 路径应该以'/'结尾（标准化格式）
	 * - Windows路径会被自动转换为使用'/'分隔符
	 * 
	 * 使用示例：
	 * @code
	 *   // 方式1：使用绝对路径
	 *   WtHelper::set_module_dir("/usr/local/wondertrader/");
	 *   
	 *   // 方式2：使用相对于工作目录的路径
	 *   std::string mod_dir = std::string(WtHelper::get_cwd()) + "modules/";
	 *   WtHelper::set_module_dir(mod_dir.c_str());
	 * @endcode
	 * 
	 * @param mod_dir 模块目录的路径字符串，建议以'/'结尾
	 * 
	 * @see get_module_dir() 获取模块目录路径
	 * 
	 * @warning 该方法不检查路径是否存在，调用者需要确保路径有效
	 * @warning 不是线程安全的，应该只在单线程初始化阶段调用
	 */
	static void set_module_dir(const char* mod_dir) { _bin_dir = mod_dir; }

private:
	/**
	 * @brief 模块目录路径静态存储
	 * 
	 * 存储WonderTrader框架模块所在的目录路径。该路径通过set_module_dir()
	 * 方法设置，通过get_module_dir()方法获取。
	 * 
	 * 特点：
	 * - 静态成员变量，在程序启动时初始化为空字符串
	 * - 全局唯一，所有WtHelper的调用者共享同一个模块目录
	 * - 生命周期与程序相同
	 * 
	 * 注意：
	 * - 作为私有成员，只能通过公有静态方法访问
	 * - 不提供线程同步机制，应该在单线程环境下设置
	 */
	static std::string	_bin_dir;
};

