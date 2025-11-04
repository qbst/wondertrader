/*!
 * \file ShareManager.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 共享内存管理器头文件
 *
 * 本文件定义了ShareManager类，用于管理UFT策略的共享内存域。
 *
 * 设计逻辑：
 * 1. 单例模式：采用单例模式，确保全局只有一个共享内存管理器实例
 * 2. 共享内存域：通过WtShareHelper模块管理共享内存域，支持参数读写和监控
 * 3. 参数管理：支持字符串、整型、浮点型等多种数据类型的参数读写
 * 4. 同步机制：提供交换区和同步区两种域，支持参数变更监控和通知
 * 5. 动态加载：动态加载WtShareHelper模块，实现插件化架构
 * 6. 参数监控：支持参数变更监控，当参数更新时通知策略引擎
 *
 * 主要功能：
 * - 初始化共享内存模块和域
 * - 读写各种类型的参数
 * - 分配共享内存参数（返回指针）
 * - 监控参数变更并通知策略引擎
 */
#pragma once
#include <stdint.h>  // 标准整数类型定义
#include <string>  // 标准字符串类

#include "../Share/StdUtils.hpp"  // 标准工具函数
#include "../Includes/FasterDefs.h"  // 快速定义头文件
#include "../Share/DLLHelper.hpp"  // 动态库加载工具

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WtUftEngine;  // 前向声明：UFT引擎类
NS_WTP_END  // WonderTrader命名空间结束
USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @typedef func_init_master
 * @brief 初始化主域函数指针类型
 * @param id 域ID
 * @param suffix 后缀名
 * @return 初始化成功返回true，失败返回false
 */
typedef bool (*func_init_master)(const char*, const char*);

/**
 * @typedef func_get_section_updatetime
 * @brief 获取分区的更新时间函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @return 更新时间戳（微秒）
 */
typedef uint64_t(*func_get_section_updatetime)(const char*, const char*);

/**
 * @typedef func_commit_section
 * @brief 提交分区函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @return 提交成功返回true，失败返回false
 */
typedef bool(*func_commit_section)(const char*, const char*);

/**
 * @typedef func_allocate_string
 * @brief 分配字符串类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 分配的字符串指针
 */
typedef const char*(*func_allocate_string)(const char*, const char*, const char*, const char*, bool);

/**
 * @typedef func_allocate_int32
 * @brief 分配int32类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 分配的int32指针
 */
typedef int32_t* (*func_allocate_int32)(const char*, const char*, const char*, int32_t, bool);

/**
 * @typedef func_allocate_int64
 * @brief 分配int64类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 分配的int64指针
 */
typedef int64_t* (*func_allocate_int64)(const char*, const char*, const char*, int64_t, bool);

/**
 * @typedef func_allocate_uint32
 * @brief 分配uint32类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 分配的uint32指针
 */
typedef uint32_t* (*func_allocate_uint32)(const char*, const char*, const char*, uint32_t, bool);

/**
 * @typedef func_allocate_uint64
 * @brief 分配uint64类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 分配的uint64指针
 */
typedef uint64_t* (*func_allocate_uint64)(const char*, const char*, const char*, uint64_t, bool);

/**
 * @typedef func_allocate_double
 * @brief 分配double类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 分配的double指针
 */
typedef double*	(*func_allocate_double)(const char*, const char*, const char*, double, bool);

/**
 * @typedef func_set_string
 * @brief 设置字符串类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
typedef bool (*func_set_string)(const char*, const char*, const char*, const char*);

/**
 * @typedef func_set_int32
 * @brief 设置int32类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
typedef bool (*func_set_int32)(const char*, const char*, const char*, int32_t);

/**
 * @typedef func_set_int64
 * @brief 设置int64类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
typedef bool (*func_set_int64)(const char*, const char*, const char*, int64_t);

/**
 * @typedef func_set_uint32
 * @brief 设置uint32类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
typedef bool (*func_set_uint32)(const char*, const char*, const char*, uint32_t);

/**
 * @typedef func_set_uint64
 * @brief 设置uint64类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
typedef bool(*func_set_uint64)(const char*, const char*, const char*, uint64_t);

/**
 * @typedef func_set_double
 * @brief 设置double类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
typedef bool(*func_set_double)(const char*, const char*, const char*, double);

/**
 * @typedef func_get_string
 * @brief 获取字符串类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值
 * @return 参数值字符串指针
 */
typedef const char* (*func_get_string)(const char*, const char*, const char*, const char*);

/**
 * @typedef func_get_int32
 * @brief 获取int32类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值
 * @return 参数值
 */
typedef int32_t (*func_get_int32)(const char*, const char*, const char*, int32_t);

/**
 * @typedef func_get_int64
 * @brief 获取int64类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值
 * @return 参数值
 */
typedef int64_t (*func_get_int64)(const char*, const char*, const char*, int64_t);

/**
 * @typedef func_get_uint32
 * @brief 获取uint32类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值
 * @return 参数值
 */
typedef uint32_t (*func_get_uint32)(const char*, const char*, const char*, uint32_t);

/**
 * @typedef func_get_uint64
 * @brief 获取uint64类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值
 * @return 参数值
 */
typedef uint64_t (*func_get_uint64)(const char*, const char*, const char*, uint64_t);

/**
 * @typedef func_get_double
 * @brief 获取double类型参数函数指针类型
 * @param domain 域名称
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值
 * @return 参数值
 */
typedef double (*func_get_double)(const char*, const char*, const char*, double);

/**
 * @class ShareManager
 * @brief 共享内存管理器类
 * 
 * 管理UFT策略的共享内存域，提供参数读写和监控功能。
 * 采用单例模式，确保全局只有一个实例。
 * 
 * 核心功能：
 * - 初始化共享内存模块和域
 * - 读写各种类型的参数
 * - 分配共享内存参数（返回指针，支持直接修改）
 * - 监控参数变更并通知策略引擎
 */
class ShareManager
{
private:
	/**
	 * @brief 私有构造函数
	 * 
	 * 创建共享内存管理器实例，初始化成员变量。
	 * 构造函数私有化，确保只能通过self()方法获取单例。
	 */
	ShareManager():_inited(false), _stopped(false), _engine(nullptr), _sync("sync"){}  // 初始化标志、停止标志、引擎指针、同步区名称
	
	/**
	 * @brief 私有析构函数
	 * 
	 * 清理共享内存管理器占用的资源，停止监控线程。
	 */
	~ShareManager()
	{
		_stopped = true;  // 设置停止标志
		if (_worker)  // 如果工作线程存在
			_worker->join();  // 等待工作线程结束
	}

public:
	/**
	 * @brief 获取单例实例
	 * @return 共享内存管理器单例引用
	 * 
	 * 返回共享内存管理器的单例实例。
	 * 使用静态局部变量确保线程安全的单例实现。
	 */
	static ShareManager& self()
	{
		static ShareManager inst;  // 静态局部变量，确保线程安全
		return inst;  // 返回单例引用
	}

	/**
	 * @brief 设置UFT引擎指针
	 * @param engine UFT引擎指针
	 * 
	 * 设置UFT引擎指针，用于参数变更通知。
	 */
	void	set_engine(WtUftEngine* engine) { _engine = engine; }  // 保存引擎指针

	/**
	 * @brief 初始化共享内存管理器
	 * @param module 共享内存模块路径
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 加载WtShareHelper模块，获取所有函数指针。
	 */
	bool	initialize(const char* module);

	/**
	 * @brief 启动参数监控
	 * @param microsecs 监控间隔（微秒），0表示无限循环检查
	 * @return 启动成功返回true，失败返回false
	 * 
	 * 启动工作线程监控参数变更，当参数更新时通知策略引擎。
	 */
	bool	start_watching(uint32_t microsecs);

	/**
	 * @brief 初始化共享内存域
	 * @param id 域ID
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化交换区和同步区两个共享内存域。
	 */
	bool	init_domain(const char* id);

	/**
	 * @brief 提交参数监控分区
	 * @param section 分区名称
	 * @return 提交成功返回true，失败返回false
	 * 
	 * 将指定的分区提交到监控列表，开始监控该分区的参数变更。
	 */
	bool	commit_param_watcher(const char* section);

	/**
	 * @brief 设置字符串类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param val 值
	 * @return 设置成功返回true，失败返回false
	 */
	bool	set_value(const char* section, const char* key, const char* val);

	/**
	 * @brief 设置int32类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param val 值
	 * @return 设置成功返回true，失败返回false
	 */
	bool	set_value(const char* section, const char* key, int32_t val);

	/**
	 * @brief 设置int64类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param val 值
	 * @return 设置成功返回true，失败返回false
	 */
	bool	set_value(const char* section, const char* key, int64_t val);

	/**
	 * @brief 设置uint32类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param val 值
	 * @return 设置成功返回true，失败返回false
	 */
	bool	set_value(const char* section, const char* key, uint32_t val);

	/**
	 * @brief 设置uint64类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param val 值
	 * @return 设置成功返回true，失败返回false
	 */
	bool	set_value(const char* section, const char* key, uint64_t val);

	/**
	 * @brief 设置double类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param val 值
	 * @return 设置成功返回true，失败返回false
	 */
	bool	set_value(const char* section, const char* key, double val);

	/**
	 * @brief 获取字符串类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param defVal 默认值，默认为空字符串
	 * @return 参数值字符串指针
	 */
	const char*	get_value(const char* section, const char* key, const char* defVal = "");

	/**
	 * @brief 获取int32类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 */
	int32_t		get_value(const char* section, const char* key, int32_t defVal = 0);

	/**
	 * @brief 获取int64类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 */
	int64_t		get_value(const char* section, const char* key, int64_t defVal = 0);

	/**
	 * @brief 获取uint32类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 */
	uint32_t	get_value(const char* section, const char* key, uint32_t defVal = 0);

	/**
	 * @brief 获取uint64类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 */
	uint64_t	get_value(const char* section, const char* key, uint64_t defVal = 0);

	/**
	 * @brief 获取double类型参数
	 * @param section 分区名称
	 * @param key 键名
	 * @param defVal 默认值，默认为0
	 * @return 参数值
	 */
	double		get_value(const char* section, const char* key, double defVal = 0);

	/**
	 * @brief 在单向同步区分配字符串类型字段
	 * @param section 分区名称
	 * @param key 键名
	 * @param initVal 初始值，默认为空字符串
	 * @param bForceWrite 是否强制写入，默认为false
	 * @param isExchg 是否使用交换区，默认为false（使用同步区）
	 * @return 分配的字符串指针，失败返回nullptr
	 * 
	 * 在共享内存中分配字符串类型字段，返回指针供直接修改。
	 * 如果isExchg为true，则在交换区分配；否则在同步区分配。
	 */
	const char*	allocate_value(const char* section, const char* key, const char* initVal = "", bool bForceWrite = false, bool isExchg = false);
	
	/**
	 * @brief 在单向同步区分配int32类型字段
	 * @param section 分区名称
	 * @param key 键名
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @param isExchg 是否使用交换区，默认为false（使用同步区）
	 * @return 分配的int32指针，失败返回nullptr
	 */
	int32_t*	allocate_value(const char* section, const char* key, int32_t initVal = 0, bool bForceWrite = false, bool isExchg = false);
	
	/**
	 * @brief 在单向同步区分配int64类型字段
	 * @param section 分区名称
	 * @param key 键名
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @param isExchg 是否使用交换区，默认为false（使用同步区）
	 * @return 分配的int64指针，失败返回nullptr
	 */
	int64_t*	allocate_value(const char* section, const char* key, int64_t initVal = 0, bool bForceWrite = false, bool isExchg = false);
	
	/**
	 * @brief 在单向同步区分配uint32类型字段
	 * @param section 分区名称
	 * @param key 键名
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @param isExchg 是否使用交换区，默认为false（使用同步区）
	 * @return 分配的uint32指针，失败返回nullptr
	 */
	uint32_t*	allocate_value(const char* section, const char* key, uint32_t initVal = 0, bool bForceWrite = false, bool isExchg = false);
	
	/**
	 * @brief 在单向同步区分配uint64类型字段
	 * @param section 分区名称
	 * @param key 键名
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @param isExchg 是否使用交换区，默认为false（使用同步区）
	 * @return 分配的uint64指针，失败返回nullptr
	 */
	uint64_t*	allocate_value(const char* section, const char* key, uint64_t initVal = 0, bool bForceWrite = false, bool isExchg = false);
	
	/**
	 * @brief 在单向同步区分配double类型字段
	 * @param section 分区名称
	 * @param key 键名
	 * @param initVal 初始值，默认为0
	 * @param bForceWrite 是否强制写入，默认为false
	 * @param isExchg 是否使用交换区，默认为false（使用同步区）
	 * @return 分配的double指针，失败返回nullptr
	 */
	double*		allocate_value(const char* section, const char* key, double initVal = 0, bool bForceWrite = false, bool isExchg = false);

private:
	bool			_inited;  // 是否已初始化标志
	std::string		_exchg;	// 交换区名称
	std::string		_sync;  // 同步区名称

	wt_hashmap<std::string, uint64_t>	_secnames;  // 监控分区名称映射表，键为分区名称，值为最后更新时间

	bool			_stopped;  // 是否已停止标志
	StdThreadPtr	_worker;  // 监控工作线程指针
	WtUftEngine*	_engine;  // UFT引擎指针，用于参数变更通知

	DllHandle		_inst;  // 动态库句柄
	std::string		_module;  // 模块路径

	func_init_master _init_master;  // 初始化主域函数指针
	func_get_section_updatetime _get_section_updatetime;  // 获取分区更新时间函数指针
	func_commit_section _commit_section;  // 提交分区函数指针

	func_set_double _set_double;  // 设置double类型参数函数指针
	func_set_int32 _set_int32;  // 设置int32类型参数函数指针
	func_set_int64 _set_int64;  // 设置int64类型参数函数指针
	func_set_uint32 _set_uint32;  // 设置uint32类型参数函数指针
	func_set_uint64 _set_uint64;  // 设置uint64类型参数函数指针
	func_set_string _set_string;  // 设置字符串类型参数函数指针

	func_get_double _get_double;  // 获取double类型参数函数指针
	func_get_int32 _get_int32;  // 获取int32类型参数函数指针
	func_get_int64 _get_int64;  // 获取int64类型参数函数指针
	func_get_uint32 _get_uint32;  // 获取uint32类型参数函数指针
	func_get_uint64 _get_uint64;  // 获取uint64类型参数函数指针
	func_get_string _get_string;  // 获取字符串类型参数函数指针

	func_allocate_double _allocate_double;  // 分配double类型参数函数指针
	func_allocate_int32 _allocate_int32;  // 分配int32类型参数函数指针
	func_allocate_int64 _allocate_int64;  // 分配int64类型参数函数指针
	func_allocate_uint32 _allocate_uint32;  // 分配uint32类型参数函数指针
	func_allocate_uint64 _allocate_uint64;  // 分配uint64类型参数函数指针
	func_allocate_string _allocate_string;  // 分配字符串类型参数函数指针
};

