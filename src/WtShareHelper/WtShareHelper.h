/*!
 * \file WtShareHelper.h
 * \project	WonderTrader
 *
 * \brief 共享内存辅助模块的C语言导出接口
 * 
 * 本文件定义了WtShareHelper模块对外提供的C语言接口函数。
 * 这些函数可以被外部程序（如Python、C#等）通过DLL/so动态库方式调用。
 * 
 * 设计说明：
 * - 使用extern "C"确保C++代码可以被C语言调用
 * - 所有函数都使用EXPORT_FLAG导出标志，确保跨平台兼容性
 * - 基于内存映射文件实现进程间共享内存
 * - 支持Master/Slave模式：Master创建和写入，Slave读取
 * - 提供域（domain）、节（section）、键（key）的三级结构
 * - 支持多种数据类型：int32、int64、uint32、uint64、double、string
 * - 提供命令队列功能，支持进程间命令传递
 * 
 * 使用流程：
 * Master模式（创建和写入）：
 * 1. init_master - 初始化Master，创建共享内存块
 * 2. allocate_xxx - 分配键值对（如果不存在）
 * 3. set_xxx - 设置键值
 * 4. commit_section - 提交节（更新修改时间）
 * 
 * Slave模式（读取）：
 * 1. init_slave - 初始化Slave，连接到共享内存块
 * 2. update_slave - 更新Slave（刷新数据）
 * 3. get_xxx - 获取键值
 * 4. release_slave - 释放Slave连接
 */
#pragma once

#include <stdint.h>                  // 标准整数类型定义
#include "../Includes/WTSMarcos.h"   // WonderTrader宏定义，包含PORTER_FLAG、EXPORT_FLAG等

/**
 * @typedef FuncGetSections
 * @brief 获取节列表的回调函数类型
 * 
 * 当调用get_sections时，会通过此回调函数返回每个节的名称。
 * 
 * @param section 节名称（字符串）
 */
typedef void(PORTER_FLAG *FuncGetSections)(const char*);

/**
 * @typedef FuncGetKeys
 * @brief 获取键列表的回调函数类型
 * 
 * 当调用get_keys时，会通过此回调函数返回每个键的信息。
 * 
 * @param key 键名称（字符串）
 * @param type 键的数据类型（64位无符号整数），1=int32, 2=uint32, 3=int64, 4=uint64, 5=double, 6=string
 */
typedef void(PORTER_FLAG *FuncGetKeys)(const char*, uint64_t);

#ifdef __cplusplus          // 如果是C++编译环境
extern "C"                  // 使用C语言链接约定，确保函数名不被C++名称修饰（name mangling）影响
{
#endif
	/**
	 * @brief 初始化Master模式
	 * @param id 域ID（字符串），标识共享内存块的名称
	 * @param path 文件路径（字符串，可选），如果为空则使用id作为文件名
	 * @return 返回初始化是否成功（布尔值）
	 * 
	 * 初始化Master模式，创建或打开共享内存块。
	 * Master模式可以创建和写入数据。
	 * 如果文件不存在，会自动创建。
	 */
	EXPORT_FLAG	bool	init_master(const char* id, const char* path = "");

	/**
	 * @brief 初始化Slave模式
	 * @param id 域ID（字符串），标识共享内存块的名称
	 * @param path 文件路径（字符串，可选），如果为空则使用id作为文件名
	 * @return 返回初始化是否成功（布尔值）
	 * 
	 * 初始化Slave模式，连接到已存在的共享内存块。
	 * Slave模式只能读取数据，不能创建或修改。
	 * 如果文件不存在，初始化会失败。
	 */
	EXPORT_FLAG	bool	init_slave(const char* id, const char* path = "");

	/**
	 * @brief 更新Slave数据
	 * @param id 域ID（字符串）
	 * @param bForce 是否强制更新（布尔值，默认false），true表示强制刷新，false表示只在数据变化时更新
	 * @return 返回是否更新成功（布尔值）
	 * 
	 * 刷新Slave的数据，从共享内存块中重新加载数据。
	 * 如果数据未变化且bForce为false，则不会更新。
	 */
	EXPORT_FLAG	bool	update_slave(const char* id, bool bForce = false);

	/**
	 * @brief 释放Slave连接
	 * @param name 域ID（字符串）
	 * @return 返回是否释放成功（布尔值）
	 * 
	 * 释放Slave连接，清理相关资源。
	 * 只有Slave模式可以释放，Master模式不能释放。
	 */
	EXPORT_FLAG bool	release_slave(const char* name);

	/**
	 * @brief 获取节列表
	 * @param domain 域名称（字符串）
	 * @param cb 回调函数指针，用于返回每个节的名称
	 * @return 返回节的数量（32位无符号整数）
	 * 
	 * 获取指定域下的所有节名称，通过回调函数返回。
	 */
	EXPORT_FLAG	uint32_t	get_sections(const char* domain, FuncGetSections cb);

	/**
	 * @brief 获取键列表
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param cb 回调函数指针，用于返回每个键的名称和类型
	 * @return 返回键的数量（32位无符号整数）
	 * 
	 * 获取指定域和节下的所有键信息，通过回调函数返回。
	 */
	EXPORT_FLAG	uint32_t	get_keys(const char* domain, const char* section, FuncGetKeys cb);

	/**
	 * @brief 获取节的更新时间
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @return 返回更新时间戳（64位无符号整数，毫秒）
	 * 
	 * 获取指定节的最后更新时间。
	 */
	EXPORT_FLAG uint64_t	get_section_updatetime(const char* domain, const char* section);

	/**
	 * @brief 提交节（更新修改时间）
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @return 返回是否提交成功（布尔值）
	 * 
	 * 提交节，更新节的修改时间为当前时间。
	 * 用于通知Slave数据已更新。
	 */
	EXPORT_FLAG bool		commit_section(const char* domain, const char* section);

	/**
	 * @brief 删除节
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @return 返回是否删除成功（布尔值）
	 * 
	 * 删除指定的节，标记为无效状态。
	 * 只有Master模式可以删除节。
	 */
	EXPORT_FLAG bool		delete_section(const char* domain, const char*section);

	/**
	 * @brief 分配字符串类型的键值对
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param initVal 初始值（字符串，可选），如果键不存在则使用此值初始化
	 * @param bForceWrite 是否强制写入（布尔值，默认false），true表示即使键已存在也使用初始值
	 * @return 返回字符串指针，如果失败返回NULL
	 * 
	 * 分配或获取字符串类型的键值对。
	 * 如果键不存在，会创建并初始化为initVal。
	 * 返回的指针可以直接修改字符串内容。
	 */
	EXPORT_FLAG const char*	allocate_string(const char* domain, const char* section, const char* key, const char* initVal, bool bForceWrite = false);

	/**
	 * @brief 分配int32类型的键值对
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param initVal 初始值（32位有符号整数，默认0）
	 * @param bForceWrite 是否强制写入（布尔值，默认false）
	 * @return 返回int32指针，如果失败返回NULL
	 * 
	 * 分配或获取int32类型的键值对。
	 * 返回的指针可以直接修改值。
	 */
	EXPORT_FLAG int32_t*	allocate_int32(const char* domain, const char* section, const char* key, int32_t initVal, bool bForceWrite = false);

	/**
	 * @brief 分配int64类型的键值对
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param initVal 初始值（64位有符号整数，默认0）
	 * @param bForceWrite 是否强制写入（布尔值，默认false）
	 * @return 返回int64指针，如果失败返回NULL
	 * 
	 * 分配或获取int64类型的键值对。
	 * 返回的指针可以直接修改值。
	 */
	EXPORT_FLAG int64_t*	allocate_int64(const char* domain, const char* section, const char* key, int64_t initVal, bool bForceWrite = false);

	/**
	 * @brief 分配uint32类型的键值对
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param initVal 初始值（32位无符号整数，默认0）
	 * @param bForceWrite 是否强制写入（布尔值，默认false）
	 * @return 返回uint32指针，如果失败返回NULL
	 * 
	 * 分配或获取uint32类型的键值对。
	 * 返回的指针可以直接修改值。
	 */
	EXPORT_FLAG uint32_t*	allocate_uint32(const char* domain, const char* section, const char* key, uint32_t initVal, bool bForceWrite = false);

	/**
	 * @brief 分配uint64类型的键值对
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param initVal 初始值（64位无符号整数，默认0）
	 * @param bForceWrite 是否强制写入（布尔值，默认false）
	 * @return 返回uint64指针，如果失败返回NULL
	 * 
	 * 分配或获取uint64类型的键值对。
	 * 返回的指针可以直接修改值。
	 */
	EXPORT_FLAG uint64_t*	allocate_uint64(const char* domain, const char* section, const char* key, uint64_t initVal, bool bForceWrite = false);

	/**
	 * @brief 分配double类型的键值对
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param initVal 初始值（双精度浮点数，默认0）
	 * @param bForceWrite 是否强制写入（布尔值，默认false）
	 * @return 返回double指针，如果失败返回NULL
	 * 
	 * 分配或获取double类型的键值对。
	 * 返回的指针可以直接修改值。
	 */
	EXPORT_FLAG double*		allocate_double(const char* domain, const char* section, const char* key, double initVal, bool bForceWrite = false);

	/**
	 * @brief 设置字符串值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param val 值（字符串）
	 * @return 返回是否设置成功（布尔值）
	 * 
	 * 设置字符串类型的键值。
	 * 如果键不存在，会自动创建。
	 */
	EXPORT_FLAG bool	set_string(const char* domain, const char* section, const char* key, const char* val);

	/**
	 * @brief 设置int32值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param val 值（32位有符号整数）
	 * @return 返回是否设置成功（布尔值）
	 * 
	 * 设置int32类型的键值。
	 */
	EXPORT_FLAG bool	set_int32(const char* domain, const char* section, const char* key, int32_t val);

	/**
	 * @brief 设置int64值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param val 值（64位有符号整数）
	 * @return 返回是否设置成功（布尔值）
	 * 
	 * 设置int64类型的键值。
	 */
	EXPORT_FLAG bool	set_int64(const char* domain, const char* section, const char* key, int64_t val);

	/**
	 * @brief 设置uint32值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param val 值（32位无符号整数）
	 * @return 返回是否设置成功（布尔值）
	 * 
	 * 设置uint32类型的键值。
	 */
	EXPORT_FLAG bool	set_uint32(const char* domain, const char* section, const char* key, uint32_t val);

	/**
	 * @brief 设置uint64值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param val 值（64位无符号整数）
	 * @return 返回是否设置成功（布尔值）
	 * 
	 * 设置uint64类型的键值。
	 */
	EXPORT_FLAG bool	set_uint64(const char* domain, const char* section, const char* key, uint64_t val);

	/**
	 * @brief 设置double值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param val 值（双精度浮点数）
	 * @return 返回是否设置成功（布尔值）
	 * 
	 * 设置double类型的键值。
	 */
	EXPORT_FLAG bool	set_double(const char* domain, const char* section, const char* key, double val);

	/**
	 * @brief 获取字符串值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param defVal 默认值（字符串，默认空字符串），如果键不存在则返回此值
	 * @return 返回字符串指针，如果键不存在返回defVal
	 * 
	 * 获取字符串类型的键值。
	 */
	EXPORT_FLAG const char*	get_string(const char* domain, const char* section, const char* key, const char* defVal = "");

	/**
	 * @brief 获取int32值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param defVal 默认值（32位有符号整数，默认0）
	 * @return 返回int32值，如果键不存在返回defVal
	 * 
	 * 获取int32类型的键值。
	 */
	EXPORT_FLAG int32_t		get_int32(const char* domain, const char* section, const char* key, int32_t defVal = 0);

	/**
	 * @brief 获取int64值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param defVal 默认值（64位有符号整数，默认0）
	 * @return 返回int64值，如果键不存在返回defVal
	 * 
	 * 获取int64类型的键值。
	 */
	EXPORT_FLAG int64_t		get_int64(const char* domain, const char* section, const char* key, int64_t defVal = 0);

	/**
	 * @brief 获取uint32值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param defVal 默认值（32位无符号整数，默认0）
	 * @return 返回uint32值，如果键不存在返回defVal
	 * 
	 * 获取uint32类型的键值。
	 */
	EXPORT_FLAG uint32_t	get_uint32(const char* domain, const char* section, const char* key, uint32_t defVal = 0);

	/**
	 * @brief 获取uint64值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param defVal 默认值（64位无符号整数，默认0）
	 * @return 返回uint64值，如果键不存在返回defVal
	 * 
	 * 获取uint64类型的键值。
	 */
	EXPORT_FLAG uint64_t	get_uint64(const char* domain, const char* section, const char* key, uint64_t defVal = 0);

	/**
	 * @brief 获取double值
	 * @param domain 域名称（字符串）
	 * @param section 节名称（字符串）
	 * @param key 键名称（字符串）
	 * @param defVal 默认值（双精度浮点数，默认0）
	 * @return 返回double值，如果键不存在返回defVal
	 * 
	 * 获取double类型的键值。
	 */
	EXPORT_FLAG double		get_double(const char* domain, const char* section, const char* key, double defVal = 0);


	/**
	 * @brief 初始化命令队列
	 * @param name 命令队列名称（字符串）
	 * @param isCmder 是否为命令下达者（布尔值，默认false），true表示下达命令，false表示接收命令
	 * @param path 文件路径（字符串，可选），如果为空则使用".cmd"作为文件名
	 * @return 返回初始化是否成功（布尔值）
	 * 
	 * 初始化命令队列，用于进程间命令传递。
	 * 命令下达者（isCmder=true）可以添加命令，接收者（isCmder=false）可以获取命令。
	 */
	EXPORT_FLAG bool		init_cmder(const char* name, bool isCmder = false, const char* path = "");

	/**
	 * @brief 添加命令
	 * @param name 命令队列名称（字符串）
	 * @param cmd 命令内容（字符串）
	 * @return 返回是否添加成功（布尔值）
	 * 
	 * 向命令队列添加一条命令。
	 * 只有命令下达者（isCmder=true）可以添加命令。
	 */
	EXPORT_FLAG bool		add_cmd(const char* name, const char* cmd);

	/**
	 * @brief 获取命令
	 * @param name 命令队列名称（字符串）
	 * @param lastIdx 上次读取的索引（引用，用于跟踪读取位置）
	 * @return 返回命令内容（字符串），如果没有新命令返回空字符串
	 * 
	 * 从命令队列获取下一条命令。
	 * 只有命令接收者（isCmder=false）可以获取命令。
	 * lastIdx用于跟踪读取位置，首次调用时应该设置为UINT32_MAX。
	 */
	EXPORT_FLAG const char*	get_cmd(const char* name, uint32_t& lastIdx);
	
#ifdef __cplusplus          // C++编译环境结束
}                           // extern "C"作用域结束
#endif