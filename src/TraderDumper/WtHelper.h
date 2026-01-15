/*!
 * \file WtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类，提供路径管理功能
 * 
 * 本文件定义了WtHelper辅助工具类，主要用于管理程序运行时的路径信息。
 * 提供获取当前工作目录和模块目录的功能，支持跨平台（Windows和Unix）的路径操作。
 * 该类采用单例模式，通过静态成员变量存储路径信息，确保全局唯一性。
 */
#pragma once
#include <string>        // 标准字符串类
#include <stdint.h>      // 标准整数类型定义

/**
 * @class WtHelper
 * @brief WonderTrader辅助工具类
 * 
 * 提供路径相关的辅助功能，包括：
 * - 获取当前工作目录
 * - 获取和设置模块目录（交易模块所在目录）
 * 
 * 该类所有方法都是静态方法，可以直接通过类名调用，无需实例化。
 */
class WtHelper
{
public:
	/**
	 * @brief 获取当前工作目录
	 * @return 返回当前工作目录的字符串指针（C风格字符串）
	 * 
	 * 该方法首次调用时会获取系统当前工作目录，并进行路径标准化处理。
	 * 后续调用直接返回缓存的路径，避免重复获取。
	 * 支持Windows和Unix系统。
	 */
	static const char* get_cwd();

	/**
	 * @brief 获取模块目录路径
	 * @return 返回模块目录的字符串指针（C风格字符串）
	 * 
	 * 返回已设置的模块目录路径，通常用于定位交易模块DLL/so文件的位置。
	 * 如果未设置，返回空字符串。
	 */
	static const char* get_module_dir(){ return _bin_dir.c_str(); }

	/**
	 * @brief 设置模块目录路径
	 * @param mod_dir 模块目录路径（C风格字符串）
	 * 
	 * 设置交易模块所在的目录路径，用于后续加载交易接口DLL/so文件。
	 * 通常在程序初始化时调用，设置traders目录的路径。
	 */
	static void set_module_dir(const char* mod_dir){ _bin_dir = mod_dir; }

private:
	static std::string	_bin_dir;  // 静态成员变量：存储模块目录路径，全局唯一
};

