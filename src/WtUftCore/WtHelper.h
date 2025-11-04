/*!
 * \file WtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader辅助工具类头文件
 *
 * 本文件定义了WtHelper类，提供路径管理和时间管理的静态方法。
 *
 * 设计逻辑：
 * 1. 静态工具类：所有方法都是静态方法，无需实例化即可使用
 * 2. 路径管理：提供统一的路径获取接口，包括工作目录、输出目录、策略数据目录等
 * 3. 时间管理：提供全局时间状态管理，包括当前日期、时间、秒数、交易日等
 * 4. 目录管理：自动创建不存在的目录，确保路径可用
 * 5. 实例目录：支持设置实例目录，用于多实例运行场景
 *
 * 主要功能：
 * - 获取当前工作目录
 * - 获取模块路径
 * - 获取基础目录、输出目录、策略数据目录等
 * - 设置和获取当前时间、交易日
 * - 设置实例目录和生成目录
 */
#pragma once
#include <string>  // 字符串头文件
#include <stdint.h>  // 标准整数类型定义

/**
 * @class WtHelper
 * @brief WonderTrader辅助工具类
 * 
 * 提供路径管理和时间管理的静态方法。
 * 所有方法都是静态方法，无需实例化即可使用。
 */
class WtHelper
{
public:
	/**
	 * @brief 获取当前工作目录
	 * @return 当前工作目录路径
	 * 
	 * 获取程序运行时的当前工作目录（Current Working Directory）。
	 * 使用静态变量缓存结果，避免重复获取。
	 */
	static std::string getCWD();

	/**
	 * @brief 获取模块路径
	 * @param moduleName 模块名称
	 * @param subDir 子目录
	 * @param isCWD 是否使用当前工作目录，默认true
	 * @return 模块完整路径
	 * 
	 * 构建模块的完整路径。
	 * 如果isCWD为true，基于当前工作目录；否则基于实例目录。
	 */
	static std::string getModulePath(const char* moduleName, const char* subDir, bool isCWD = true);

	/**
	 * @brief 获取基础目录
	 * @return 基础目录路径
	 * 
	 * 获取生成文件的基础目录（generated目录）。
	 * 如果目录不存在，会自动创建。
	 */
	static const char* getBaseDir();
	
	/**
	 * @brief 获取输出目录
	 * @return 输出目录路径
	 * 
	 * 获取输出文件目录（generated/outputs目录）。
	 * 如果目录不存在，会自动创建。
	 */
	static const char* getOutputDir();
	
	/**
	 * @brief 获取策略数据目录
	 * @return 策略数据目录路径
	 * 
	 * 获取策略数据存储目录（generated/stradata目录）。
	 * 如果目录不存在，会自动创建。
	 */
	static const char* getStraDataDir();
	
	/**
	 * @brief 获取策略用户数据目录
	 * @return 策略用户数据目录路径
	 * 
	 * 获取策略用户数据存储目录（generated/userdata目录）。
	 * 如果目录不存在，会自动创建。
	 */
	static const char* getStraUsrDatDir();

	/**
	 * @brief 获取投资组合目录
	 * @return 投资组合目录路径
	 * 
	 * 获取投资组合数据存储目录（generated/portfolio目录）。
	 * 如果目录不存在，会自动创建。
	 */
	static const char* getPortifolioDir();

	/**
	 * @brief 设置当前时间
	 * @param date 当前日期（YYYYMMDD格式）
	 * @param time 当前时间（HHMMSS格式）
	 * @param secs 当前秒数（包含毫秒），默认0
	 * 
	 * 设置全局的当前日期和时间。
	 */
	static inline void setTime(uint32_t date, uint32_t time, uint32_t secs = 0)
	{
		_cur_date = date;  // 设置当前日期
		_cur_time = time;  // 设置当前时间
		_cur_secs = secs;  // 设置当前秒数
	}

	/**
	 * @brief 设置当前交易日
	 * @param tDate 交易日（YYYYMMDD格式）
	 * 
	 * 设置全局的当前交易日。
	 */
	static inline void setTDate(uint32_t tDate){ _cur_tdate = tDate; }  // 设置当前交易日

	/**
	 * @brief 获取当前日期
	 * @return 当前日期（YYYYMMDD格式）
	 * 
	 * 返回全局的当前日期。
	 */
	static inline uint32_t getDate(){ return _cur_date; }  // 返回当前日期
	
	/**
	 * @brief 获取当前时间
	 * @return 当前时间（HHMMSS格式）
	 * 
	 * 返回全局的当前时间（以分钟为准）。
	 */
	static inline uint32_t getTime(){ return _cur_time; }  // 返回当前时间
	
	/**
	 * @brief 获取当前秒数
	 * @return 当前秒数（包含毫秒）
	 * 
	 * 返回全局的当前秒数（包含毫秒）。
	 */
	static inline uint32_t getSecs(){ return _cur_secs; }  // 返回当前秒数
	
	/**
	 * @brief 获取当前交易日
	 * @return 当前交易日（YYYYMMDD格式）
	 * 
	 * 返回全局的当前交易日。
	 */
	static inline uint32_t getTradingDate(){ return _cur_tdate; }  // 返回当前交易日

	/**
	 * @brief 获取实例目录
	 * @return 实例目录路径
	 * 
	 * 返回实例所在的目录路径。
	 */
	static const std::string& getInstDir() { return _inst_dir; }  // 返回实例目录
	
	/**
	 * @brief 设置实例目录
	 * @param inst_dir 实例目录路径
	 * 
	 * 设置实例所在的目录路径。
	 */
	static void setInstDir(const char* inst_dir){ _inst_dir = inst_dir; }  // 设置实例目录

	/**
	 * @brief 设置生成目录
	 * @param gen_dir 生成目录路径
	 * 
	 * 设置生成文件的输出目录路径。
	 */
	static void setGenerateDir(const char* gen_dir) { _gen_dir = gen_dir; }  // 设置生成目录

private:
	static uint32_t		_cur_date;	// 当前日期（YYYYMMDD格式）
	static uint32_t		_cur_time;	// 当前时间（HHMMSS格式），以分钟为准
	static uint32_t		_cur_secs;	// 当前秒数（包含毫秒）
	static uint32_t		_cur_tdate;	// 当前交易日（YYYYMMDD格式）
	static std::string	_inst_dir;	// 实例所在目录
	static std::string	_gen_dir;	// 生成文件输出目录
};

