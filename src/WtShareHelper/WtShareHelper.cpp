/*!
 * \file WtShareHelper.cpp
 * \project	WonderTrader
 *
 * \brief WtShareHelper模块的C语言接口实现
 * 
 * 本文件实现了WtShareHelper.h中声明的所有C语言导出函数。
 * 这些函数作为外部接口，内部调用ShareBlocks类的功能。
 * 
 * 设计说明：
 * - 使用单例模式，通过ShareBlocks::one()获取全局唯一的ShareBlocks实例
 * - 所有C接口函数都是对ShareBlocks类方法的简单封装
 * - 提供类型转换和回调函数适配
 * 
 * 实现逻辑：
 * 1. init_master/init_slave - 初始化Master/Slave模式
 * 2. update_slave/release_slave - 更新和释放Slave
 * 3. get_sections/get_keys - 获取节和键列表（通过回调函数）
 * 4. allocate_xxx - 分配键值对
 * 5. set_xxx/get_xxx - 设置和获取键值
 * 6. init_cmder/add_cmd/get_cmd - 命令队列功能
 */
#include "WtShareHelper.h"           // 包含C接口声明
#include "ShareBlocks.h"             // 包含ShareBlocks类定义

using namespace shareblock;          // 使用shareblock命名空间

/**
 * @brief 初始化Master模式的C接口实现
 * @param id 域ID
 * @param path 文件路径
 * @return 返回初始化是否成功
 * 
 * 调用ShareBlocks的init_master方法初始化Master模式。
 */
bool init_master(const char* id, const char* path/* = ""*/)
{
	return ShareBlocks::one().init_master(id, path);  // 调用ShareBlocks的init_master方法
}


/**
 * @brief 初始化Slave模式的C接口实现
 * @param id 域ID
 * @param path 文件路径
 * @return 返回初始化是否成功
 * 
 * 调用ShareBlocks的init_slave方法初始化Slave模式。
 */
bool init_slave(const char* id, const char* path/* = ""*/)
{
	return ShareBlocks::one().init_slave(id, path);   // 调用ShareBlocks的init_slave方法
}

/**
 * @brief 更新Slave数据的C接口实现
 * @param id 域ID
 * @param bForce 是否强制更新
 * @return 返回是否更新成功
 * 
 * 调用ShareBlocks的update_slave方法更新Slave数据。
 */
bool update_slave(const char* id, bool bForce/* = false*/)
{
	return ShareBlocks::one().update_slave(id, bForce);  // 调用ShareBlocks的update_slave方法
}

/**
 * @brief 释放Slave连接的C接口实现
 * @param name 域ID
 * @return 返回是否释放成功
 * 
 * 调用ShareBlocks的release_slave方法释放Slave连接。
 */
bool release_slave(const char* name)
{
	return ShareBlocks::one().release_slave(name);     // 调用ShareBlocks的release_slave方法
}

/**
 * @brief 获取节列表的C接口实现
 * @param domain 域名称
 * @param cb 回调函数指针
 * @return 返回节的数量
 * 
 * 获取节列表，通过回调函数返回每个节的名称。
 */
uint32_t get_sections(const char* domain, FuncGetSections cb)
{
	auto ay = ShareBlocks::one().get_sections(domain);  // 调用ShareBlocks的get_sections方法，获取节名称向量
	for (const std::string& v : ay)                     // 遍历节名称向量
		cb(v.c_str());                                  // 调用回调函数，传递节名称

	return (uint32_t)ay.size();                         // 返回节的数量
}

/**
 * @brief 获取键列表的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param cb 回调函数指针
 * @return 返回键的数量
 * 
 * 获取键列表，通过回调函数返回每个键的名称和类型。
 */
uint32_t get_keys(const char* domain, const char* section, FuncGetKeys cb)
{
	auto ay = ShareBlocks::one().get_keys(domain, section);  // 调用ShareBlocks的get_keys方法，获取键信息向量
	for (KeyInfo* v : ay)                                    // 遍历键信息向量
		cb(v->_key, v->_type);                               // 调用回调函数，传递键名称和类型

	return (uint32_t)ay.size();                              // 返回键的数量
}

/**
 * @brief 获取节的更新时间的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回更新时间戳
 * 
 * 调用ShareBlocks的get_section_updatetime方法获取节的更新时间。
 */
uint64_t get_section_updatetime(const char* domain, const char* section)
{
	return ShareBlocks::one().get_section_updatetime(domain, section);  // 调用ShareBlocks的get_section_updatetime方法
}

/**
 * @brief 提交节的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回是否提交成功
 * 
 * 调用ShareBlocks的commit_section方法提交节。
 */
bool commit_section(const char* domain, const char* section)
{
	return ShareBlocks::one().commit_section(domain, section);  // 调用ShareBlocks的commit_section方法
}

/**
 * @brief 删除节的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回是否删除成功
 * 
 * 调用ShareBlocks的delete_section方法删除节。
 */
bool delete_section(const char* domain, const char*section)
{
	return ShareBlocks::one().delete_section(domain, section);   // 调用ShareBlocks的delete_section方法
}

/**
 * @brief 分配字符串类型键值对的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回字符串指针
 * 
 * 调用ShareBlocks的allocate_string方法分配字符串类型键值对。
 */
const char* allocate_string(const char* domain, const char* section, const char* key, const char* initVal, bool bForceWrite /*= false*/)
{
	return ShareBlocks::one().allocate_string(domain, section, key, initVal, bForceWrite);  // 调用ShareBlocks的allocate_string方法
}

/**
 * @brief 分配int32类型键值对的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回int32指针
 * 
 * 调用ShareBlocks的allocate_int32方法分配int32类型键值对。
 */
int32_t* allocate_int32(const char* domain, const char* section, const char* key, int32_t initVal, bool bForceWrite /*= false*/)
{
	return ShareBlocks::one().allocate_int32(domain, section, key, initVal, bForceWrite);  // 调用ShareBlocks的allocate_int32方法
}

/**
 * @brief 分配int64类型键值对的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回int64指针
 * 
 * 调用ShareBlocks的allocate_int64方法分配int64类型键值对。
 */
int64_t* allocate_int64(const char* domain, const char* section, const char* key, int64_t initVal, bool bForceWrite /*= false*/)
{
	return ShareBlocks::one().allocate_int64(domain, section, key, initVal, bForceWrite);  // 调用ShareBlocks的allocate_int64方法
}

/**
 * @brief 分配uint32类型键值对的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回uint32指针
 * 
 * 调用ShareBlocks的allocate_uint32方法分配uint32类型键值对。
 */
uint32_t* allocate_uint32(const char* domain, const char* section, const char* key, uint32_t initVal, bool bForceWrite /*= false*/)
{
	return ShareBlocks::one().allocate_uint32(domain, section, key,  initVal, bForceWrite);  // 调用ShareBlocks的allocate_uint32方法
}

/**
 * @brief 分配uint64类型键值对的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回uint64指针
 * 
 * 调用ShareBlocks的allocate_uint64方法分配uint64类型键值对。
 */
uint64_t* allocate_uint64(const char* domain, const char* section, const char* key, uint64_t initVal, bool bForceWrite /*= false*/)
{
	return ShareBlocks::one().allocate_uint64(domain, section, key, initVal, bForceWrite);  // 调用ShareBlocks的allocate_uint64方法
}

/**
 * @brief 分配double类型键值对的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回double指针
 * 
 * 调用ShareBlocks的allocate_double方法分配double类型键值对。
 */
double* allocate_double(const char* domain, const char* section, const char* key, double initVal, bool bForceWrite /*= false*/)
{
	return ShareBlocks::one().allocate_double(domain, section, key, initVal, bForceWrite);  // 调用ShareBlocks的allocate_double方法
}

/**
 * @brief 设置字符串值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 调用ShareBlocks的set_string方法设置字符串值。
 */
bool set_string(const char* domain, const char* section, const char* key, const char* val)
{
	return ShareBlocks::one().set_string(domain, section, key, val);  // 调用ShareBlocks的set_string方法
}

/**
 * @brief 设置int32值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 调用ShareBlocks的set_int32方法设置int32值。
 */
bool set_int32(const char* domain, const char* section, const char* key, int32_t val)
{
	return ShareBlocks::one().set_int32(domain, section, key, val);  // 调用ShareBlocks的set_int32方法
}

/**
 * @brief 设置int64值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 调用ShareBlocks的set_int64方法设置int64值。
 */
bool set_int64(const char* domain, const char* section, const char* key, int64_t val)
{
	return ShareBlocks::one().set_int64(domain, section, key, val);  // 调用ShareBlocks的set_int64方法
}

/**
 * @brief 设置uint32值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 调用ShareBlocks的set_uint32方法设置uint32值。
 */
bool set_uint32(const char* domain, const char* section, const char* key, uint32_t val)
{
	return ShareBlocks::one().set_uint32(domain, section, key, val);  // 调用ShareBlocks的set_uint32方法
}

/**
 * @brief 设置uint64值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 调用ShareBlocks的set_uint64方法设置uint64值。
 */
bool set_uint64(const char* domain, const char* section, const char* key, uint64_t val)
{
	return ShareBlocks::one().set_uint64(domain, section, key, val);  // 调用ShareBlocks的set_uint64方法
}

/**
 * @brief 设置double值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 调用ShareBlocks的set_double方法设置double值。
 */
bool set_double(const char* domain, const char* section, const char* key, double val)
{
	return ShareBlocks::one().set_double(domain, section, key, val);  // 调用ShareBlocks的set_double方法
}

/**
 * @brief 获取字符串值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回字符串指针
 * 
 * 调用ShareBlocks的get_string方法获取字符串值。
 */
const char* get_string(const char* domain, const char* section, const char* key, const char* defVal /* = "" */)
{
	return ShareBlocks::one().get_string(domain, section, key, defVal);  // 调用ShareBlocks的get_string方法
}

/**
 * @brief 获取int32值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回int32值
 * 
 * 调用ShareBlocks的get_int32方法获取int32值。
 */
int32_t get_int32(const char* domain, const char* section, const char* key, int32_t defVal /* = 0 */)
{
	return ShareBlocks::one().get_int32(domain, section, key, defVal);  // 调用ShareBlocks的get_int32方法
}

/**
 * @brief 获取int64值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回int64值
 * 
 * 调用ShareBlocks的get_int64方法获取int64值。
 */
int64_t get_int64(const char* domain, const char* section, const char* key, int64_t defVal /* = 0 */)
{
	return ShareBlocks::one().get_int64(domain, section, key, defVal);  // 调用ShareBlocks的get_int64方法
}

/**
 * @brief 获取uint32值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回uint32值
 * 
 * 调用ShareBlocks的get_uint32方法获取uint32值。
 */
uint32_t get_uint32(const char* domain, const char* section, const char* key, uint32_t defVal /* = 0 */)
{
	return ShareBlocks::one().get_uint32(domain, section, key, defVal);  // 调用ShareBlocks的get_uint32方法
}

/**
 * @brief 获取uint64值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回uint64值
 * 
 * 调用ShareBlocks的get_uint64方法获取uint64值。
 */
uint64_t get_uint64(const char* domain, const char* section, const char* key, uint64_t defVal /* = 0 */)
{
	return ShareBlocks::one().get_uint64(domain, section, key, defVal);  // 调用ShareBlocks的get_uint64方法
}

/**
 * @brief 获取double值的C接口实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回double值
 * 
 * 调用ShareBlocks的get_double方法获取double值。
 */
double get_double(const char* domain, const char* section, const char* key, double defVal /* = 0 */)
{
	return ShareBlocks::one().get_double(domain, section, key, defVal);  // 调用ShareBlocks的get_double方法
}

/**
 * @brief 初始化命令队列的C接口实现
 * @param name 命令队列名称
 * @param isCmder 是否为命令下达者
 * @param path 文件路径
 * @return 返回初始化是否成功
 * 
 * 调用ShareBlocks的init_cmder方法初始化命令队列。
 */
bool init_cmder(const char* name, bool isCmder /* = false */, const char* path /* = "" */)
{
	return ShareBlocks::one().init_cmder(name, isCmder, path);  // 调用ShareBlocks的init_cmder方法
}

/**
 * @brief 添加命令的C接口实现
 * @param name 命令队列名称
 * @param cmd 命令内容
 * @return 返回是否添加成功
 * 
 * 调用ShareBlocks的add_cmd方法添加命令。
 */
bool add_cmd(const char* name, const char* cmd)
{
	return ShareBlocks::one().add_cmd(name, cmd);  // 调用ShareBlocks的add_cmd方法
}

/**
 * @brief 获取命令的C接口实现
 * @param name 命令队列名称
 * @param lastIdx 上次读取的索引（引用）
 * @return 返回命令内容
 * 
 * 调用ShareBlocks的get_cmd方法获取命令。
 */
const char* get_cmd(const char* name, uint32_t& lastIdx)
{
	return ShareBlocks::one().get_cmd(name, lastIdx);  // 调用ShareBlocks的get_cmd方法
}