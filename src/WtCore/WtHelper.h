/*!
 * \file WtHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 辅助工具类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的辅助工具类，提供系统路径管理和时间管理功能。
 * 主要功能包括：
 * 1. 路径管理：提供各种目录路径的获取和设置功能，包括工作目录、模块路径、输出目录等
 * 2. 时间管理：提供当前日期、时间、秒数以及交易日的设置和获取功能
 * 3. 目录管理：自动创建策略数据目录、执行数据目录、用户数据目录等
 * 4. 实例管理：支持设置实例目录和生成文件输出目录
 * 
 * 设计特点：
 * - 使用静态成员变量存储全局状态（当前时间、目录路径等）
 * - 提供静态方法访问这些全局状态，实现单例模式的效果
 * - 目录路径会自动创建，确保目录存在
 * - 支持Windows和Unix平台的路径处理
 * 
 * 使用场景：
 * 该类主要用于WonderTrader框架的各个模块中，提供统一的路径和时间管理接口，
 * 简化路径获取和时间访问的代码，提高代码的可维护性。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含标准字符串类型
#include <stdint.h>  // 包含标准整数类型定义

/**
 * @class WtHelper
 * @brief 辅助工具类
 * 
 * 该类提供系统路径管理和时间管理的静态方法，是整个框架的基础工具类。
 * 所有方法都是静态方法，通过静态成员变量存储全局状态。
 * 
 * 主要功能：
 * - 路径管理：获取和设置各种目录路径
 * - 时间管理：获取和设置当前时间信息
 * - 目录创建：自动创建需要的目录
 */
class WtHelper
{
public:
	/**
	 * @brief 获取当前工作目录
	 * @return std::string 返回当前工作目录路径
	 * 
	 * 获取程序运行时的当前工作目录（Current Working Directory）。
	 * 首次调用时会从系统获取，后续调用返回缓存的结果。
	 * 路径已标准化处理（统一使用斜杠分隔符）。
	 */
	static std::string getCWD();  // 获取当前工作目录

	/**
	 * @brief 获取模块路径
	 * @param moduleName 模块名称
	 * @param subDir 子目录名称
	 * @param isCWD 是否基于当前工作目录，默认为true；false表示基于实例目录
	 * @return std::string 返回完整的模块路径
	 * 
	 * 根据模块名称和子目录构建完整的模块路径。
	 * 路径格式：基础目录 + 子目录 + "/" + 模块名称
	 * 基础目录可以是当前工作目录或实例目录。
	 */
	static std::string getModulePath(const char* moduleName, const char* subDir, bool isCWD = true);  // 获取模块路径

	/**
	 * @brief 获取基础目录
	 * @return const char* 返回基础目录路径字符串
	 * 
	 * 获取生成文件的基础输出目录，所有生成的文件都存放在此目录下。
	 * 如果目录不存在，会自动创建。
	 * 默认路径为"./generated/"。
	 */
	static const char* getBaseDir();  // 获取基础目录

	/**
	 * @brief 获取输出目录
	 * @return const char* 返回输出目录路径字符串
	 * 
	 * 获取输出文件目录，用于存放各种输出文件（如日志、报告等）。
	 * 如果目录不存在，会自动创建。
	 * 路径格式：基础目录 + "outputs/"
	 */
	static const char* getOutputDir();  // 获取输出目录

	/**
	 * @brief 获取策略数据目录
	 * @return const char* 返回策略数据目录路径字符串
	 * 
	 * 获取策略数据目录，用于存放策略相关的数据文件。
	 * 如果目录不存在，会自动创建。
	 * 路径格式：基础目录 + "stradata/"
	 */
	static const char* getStraDataDir();  // 获取策略数据目录

	/**
	 * @brief 获取策略用户数据目录
	 * @return const char* 返回策略用户数据目录路径字符串
	 * 
	 * 获取策略用户数据目录，用于存放策略的用户自定义数据文件。
	 * 如果目录不存在，会自动创建。
	 * 路径格式：基础目录 + "userdata/"
	 */
	static const char* getStraUsrDatDir();  // 获取策略用户数据目录

	/**
	 * @brief 获取组合目录
	 * @return const char* 返回组合目录路径字符串
	 * 
	 * 获取组合（Portfolio）目录，用于存放组合相关的数据文件。
	 * 如果目录不存在，会自动创建。
	 * 路径格式：基础目录 + "portfolio/"
	 */
	static const char* getPortifolioDir();  // 获取组合目录

	/**
	 * @brief 获取执行数据目录
	 * @return const char* 返回执行数据目录路径字符串
	 * 
	 * 获取执行数据目录，用于存放执行器相关的数据文件。
	 * 如果目录不存在，会自动创建。
	 * 路径格式：基础目录 + "execdata/"
	 */
	static const char* getExecDataDir();  // 获取执行数据目录

	/**
	 * @brief 设置当前时间
	 * @param date 当前日期，格式为YYYYMMDD
	 * @param time 当前时间（分钟），格式为HHMM
	 * @param secs 当前秒数（包含毫秒），默认为0
	 * 
	 * 设置框架的当前时间信息，包括日期、时间和秒数。
	 * 这些时间信息会被框架的其他模块使用，用于时间相关的判断和计算。
	 */
	static inline void setTime(uint32_t date, uint32_t time, uint32_t secs = 0)  // 内联函数：设置当前时间
	{
		_cur_date = date;  // 设置当前日期
		_cur_time = time;  // 设置当前时间（分钟）
		_cur_secs = secs;  // 设置当前秒数
	}

	/**
	 * @brief 设置交易日期
	 * @param tDate 交易日期，格式为YYYYMMDD
	 * 
	 * 设置当前交易日，交易日可能与实际日期不同（如夜盘交易）。
	 * 交易日用于确定交易逻辑和结算逻辑。
	 */
	static inline void setTDate(uint32_t tDate){ _cur_tdate = tDate; }  // 内联函数：设置交易日期

	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期，格式为YYYYMMDD
	 * 
	 * 获取框架设置的当前日期。
	 */
	static inline uint32_t getDate(){ return _cur_date; }  // 内联函数：获取当前日期

	/**
	 * @brief 获取当前时间
	 * @return uint32_t 返回当前时间（分钟），格式为HHMM
	 * 
	 * 获取框架设置的当前时间（分钟级别）。
	 */
	static inline uint32_t getTime(){ return _cur_time; }  // 内联函数：获取当前时间

	/**
	 * @brief 获取当前秒数
	 * @return uint32_t 返回当前秒数（包含毫秒）
	 * 
	 * 获取框架设置的当前秒数，包含毫秒信息。
	 */
	static inline uint32_t getSecs(){ return _cur_secs; }  // 内联函数：获取当前秒数

	/**
	 * @brief 获取交易日期
	 * @return uint32_t 返回交易日期，格式为YYYYMMDD
	 * 
	 * 获取框架设置的当前交易日期。
	 */
	static inline uint32_t getTradingDate(){ return _cur_tdate; }  // 内联函数：获取交易日期

	/**
	 * @brief 获取实例目录
	 * @return const std::string& 返回实例目录路径字符串引用
	 * 
	 * 获取当前实例所在的目录路径。
	 * 实例目录用于区分不同的运行实例，每个实例可以有独立的配置和数据。
	 */
	static const std::string& getInstDir() { return _inst_dir; }  // 获取实例目录

	/**
	 * @brief 设置实例目录
	 * @param inst_dir 实例目录路径
	 * 
	 * 设置当前实例所在的目录路径。
	 * 该目录用于存放实例相关的配置和数据文件。
	 */
	static void setInstDir(const char* inst_dir){ _inst_dir = inst_dir; }  // 设置实例目录

	/**
	 * @brief 设置生成文件输出目录
	 * @param gen_dir 生成文件输出目录路径
	 * 
	 * 设置生成文件的基础输出目录。
	 * 所有生成的文件（策略数据、执行数据、用户数据等）都会存放在此目录下。
	 * 默认值为"./generated/"。
	 */
	static void setGenerateDir(const char* gen_dir) { _gen_dir = gen_dir; }  // 设置生成文件输出目录

private:
	static uint32_t		_cur_date;	// 静态成员变量：当前日期，格式为YYYYMMDD
	static uint32_t		_cur_time;	// 静态成员变量：当前时间（分钟），格式为HHMM
	static uint32_t		_cur_secs;	// 静态成员变量：当前秒数，包含毫秒信息
	static uint32_t		_cur_tdate;	// 静态成员变量：当前交易日，格式为YYYYMMDD
	static std::string	_inst_dir;	// 静态成员变量：实例所在目录路径
	static std::string	_gen_dir;	// 静态成员变量：生成文件输出目录路径，默认为"./generated/"
};
