/*!
 * \file WtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类头文件
 *
 * 本文件定义了WtHelper类，提供WonderTrader框架中的通用辅助功能。
 * 
 * 设计逻辑：
 * 1. 路径管理：提供当前工作目录、输出目录、实例目录的管理功能
 * 2. 目录创建：自动创建输出目录（如果不存在）
 * 3. 跨平台支持：支持Windows和Unix平台的路径操作
 * 4. 静态工具类：所有方法都是静态方法，无需实例化即可使用
 *
 * 主要功能：
 * - 获取当前工作目录
 * - 获取和设置输出目录
 * - 获取和设置实例目录
 */
#pragma once
#include <string>      // 字符串类
#include <stdint.h>    // 标准整数类型定义

/**
 * @class WtHelper
 * @brief WonderTrader辅助工具类
 * 
 * 该类提供WonderTrader框架中的通用辅助功能，包括路径管理、目录创建等。
 * 所有方法都是静态方法，可以直接通过类名调用，无需实例化。
 */
class WtHelper
{
public:
	/**
	 * @brief 获取当前工作目录
	 * @return 当前工作目录路径字符串
	 * 
	 * 获取程序的当前工作目录，并标准化路径格式。
	 * 使用静态变量缓存结果，提高性能。
	 */
	static std::string getCWD();

	/**
	 * @brief 获取输出目录
	 * @return 输出目录路径字符串（C风格字符串）
	 * 
	 * 获取回测结果输出目录。如果目录不存在，会自动创建。
	 * 默认输出目录为"./outputs_bt/"。
	 */
	static const char* getOutputDir();

	/**
	 * @brief 获取实例目录
	 * @return 实例目录路径字符串的引用
	 * 
	 * 获取实例所在目录的路径。
	 */
	static const std::string& getInstDir() { return _inst_dir; }
	
	/**
	 * @brief 设置实例目录
	 * @param inst_dir 实例目录路径
	 * 
	 * 设置实例所在目录的路径。
	 */
	static void setInstDir(const char* inst_dir) { _inst_dir = inst_dir; }
	
	/**
	 * @brief 设置输出目录
	 * @param out_dir 输出目录路径
	 * 
	 * 设置回测结果输出目录的路径。
	 * 路径会被标准化（统一路径分隔符格式）。
	 */
	static void setOutputDir(const char* out_dir);

private:
	static std::string	_inst_dir;	// 实例所在目录，存储实例目录路径
	static std::string	_out_dir;   // 输出目录，存储回测结果输出目录路径，默认为"./outputs_bt/"
};

