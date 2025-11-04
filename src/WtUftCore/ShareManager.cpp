/*!
 * \file ShareManager.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 共享内存管理器实现文件
 *
 * 本文件实现了ShareManager类的所有功能，包括：
 * 1. 动态加载：加载WtShareHelper模块，获取函数指针
 * 2. 域初始化：初始化交换区和同步区两个共享内存域
 * 3. 参数读写：实现各种数据类型的参数读写操作
 * 4. 参数分配：在共享内存中分配参数，返回指针供直接修改
 * 5. 参数监控：启动工作线程监控参数变更，通知策略引擎
 */
#include "ShareManager.h"  // 共享内存管理器头文件
#include "WtUftEngine.h"  // UFT引擎头文件
#include "../WTSTools/WTSLogger.h"  // 日志工具

#include "../Share/StdUtils.hpp"  // 标准工具函数

#include "../Share/TimeUtils.hpp"  // 时间工具函数


/**
 * @brief 初始化共享内存管理器
 * @param module 共享内存模块路径
 * @return 初始化成功返回true，失败返回false
 * 
 * 加载WtShareHelper模块，获取所有函数指针。
 * 如果模块文件不存在或加载失败，则返回false。
 */
bool ShareManager::initialize(const char* module)
{
	if (_inited)  // 如果已经初始化
		return true;  // 返回true

	if(!StdFile::exists(module))  // 如果模块文件不存在
	{
		WTSLogger::warn("WtShareHelper {} not exist", module);  // 记录警告日志
		return false;  // 返回false
	}

	_module = module;  // 保存模块路径
	_inst = DLLHelper::load_library(_module.c_str());  // 加载动态库
	_inited = (_inst != NULL);  // 根据加载结果设置初始化标志

	_init_master = (func_init_master)DLLHelper::get_symbol(_inst, "init_master");  // 获取初始化主域函数
	_get_section_updatetime = (func_get_section_updatetime)DLLHelper::get_symbol(_inst, "get_section_updatetime");  // 获取分区更新时间函数
	_commit_section = (func_commit_section)DLLHelper::get_symbol(_inst, "commit_section");  // 获取提交分区函数

	_set_double = (func_set_double)DLLHelper::get_symbol(_inst, "set_double");  // 获取设置double类型参数函数
	_set_int32 = (func_set_int32)DLLHelper::get_symbol(_inst, "set_int32");  // 获取设置int32类型参数函数
	_set_int64 = (func_set_int64)DLLHelper::get_symbol(_inst, "set_int64");  // 获取设置int64类型参数函数
	_set_uint32 = (func_set_uint32)DLLHelper::get_symbol(_inst, "set_uint32");  // 获取设置uint32类型参数函数
	_set_uint64 = (func_set_uint64)DLLHelper::get_symbol(_inst, "set_uint64");  // 获取设置uint64类型参数函数
	_set_string = (func_set_string)DLLHelper::get_symbol(_inst, "set_string");  // 获取设置字符串类型参数函数

	_get_double = (func_get_double)DLLHelper::get_symbol(_inst, "get_double");  // 获取获取double类型参数函数
	_get_int32 = (func_get_int32)DLLHelper::get_symbol(_inst, "get_int32");  // 获取获取int32类型参数函数
	_get_int64 = (func_get_int64)DLLHelper::get_symbol(_inst, "get_int64");  // 获取获取int64类型参数函数
	_get_uint32 = (func_get_uint32)DLLHelper::get_symbol(_inst, "get_uint32");  // 获取获取uint32类型参数函数
	_get_uint64 = (func_get_uint64)DLLHelper::get_symbol(_inst, "get_uint64");  // 获取获取uint64类型参数函数
	_get_string = (func_get_string)DLLHelper::get_symbol(_inst, "get_string");  // 获取获取字符串类型参数函数

	_allocate_double = (func_allocate_double)DLLHelper::get_symbol(_inst, "allocate_double");  // 获取分配double类型参数函数
	_allocate_int32 = (func_allocate_int32)DLLHelper::get_symbol(_inst, "allocate_int32");  // 获取分配int32类型参数函数
	_allocate_int64 = (func_allocate_int64)DLLHelper::get_symbol(_inst, "allocate_int64");  // 获取分配int64类型参数函数
	_allocate_uint32 = (func_allocate_uint32)DLLHelper::get_symbol(_inst, "allocate_uint32");  // 获取分配uint32类型参数函数
	_allocate_uint64 = (func_allocate_uint64)DLLHelper::get_symbol(_inst, "allocate_uint64");  // 获取分配uint64类型参数函数
	_allocate_string = (func_allocate_string)DLLHelper::get_symbol(_inst, "allocate_string");  // 获取分配字符串类型参数函数

	return _inited;  // 返回初始化结果
}

/**
 * @brief 启动参数监控
 * @param microsecs 监控间隔（微秒），0表示无限循环检查
 * @return 启动成功返回true，失败返回false
 * 
 * 启动工作线程监控参数变更，当参数更新时通知策略引擎。
 * 工作线程会循环检查所有监控分区的更新时间，如果发现更新则通知引擎。
 */
bool ShareManager::start_watching(uint32_t microsecs)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	if (_inited && !_stopped && _worker == nullptr)  // 如果已初始化、未停止且工作线程未创建
	{
		_worker.reset(new StdThread([this, microsecs]() {  // 创建工作线程
			while (!_stopped)  // 循环直到停止标志为true
			{
				for(auto& v : _secnames)  // 遍历所有监控分区
				{
					if(_stopped)  // 如果已停止
						break;  // 跳出循环

					const char* section = v.first.c_str();  // 获取分区名称
					uint64_t& udtTime = (uint64_t&)v.second;  // 获取最后更新时间引用

					uint64_t lastUdtTime = _get_section_updatetime(_exchg.c_str(), section);  // 获取分区的最新更新时间
					if(lastUdtTime > v.second)  // 如果更新时间大于记录的时间
					{
						//触发通知
						_engine->notify_params_update(section);  // 通知引擎参数已更新
						udtTime = lastUdtTime;  // 更新记录的时间
					}
				}

				//如果等待时间为0，则进入无限循环的检查中
				if(microsecs > 0 && !_stopped)  // 如果等待时间大于0且未停止
					std::this_thread::sleep_for(std::chrono::microseconds(microsecs));  // 休眠指定时间
			}
		}));

		WTSLogger::info("Share domain is on watch");  // 记录信息日志
	}

	return true;  // 返回成功
}

/**
 * @brief 初始化共享内存域
 * @param id 域ID
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化交换区和同步区两个共享内存域。
 * 交换区用于参数交换，同步区用于单向同步。
 */
bool ShareManager::init_domain(const char* id)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	bool ret = _init_master(id, ".share");  // 初始化交换区，后缀为.share
	_exchg = id;  // 保存交换区ID
	WTSLogger::info("Share domain [{}] initialing {}", id, ret ? "succeed" : "failed");  // 记录信息日志

	//初始化同步区
	ret = _init_master("sync", ".sync");  // 初始化同步区，域ID为"sync"，后缀为.sync
	WTSLogger::info("Sync domain [sync] initialing {}", ret ? "succeed" : "failed");  // 记录信息日志

	return ret;  // 返回初始化结果
}

/**
 * @brief 提交参数监控分区
 * @param section 分区名称
 * @return 提交成功返回true，失败返回false
 * 
 * 将指定的分区提交到监控列表，开始监控该分区的参数变更。
 * 记录分区的当前时间作为初始监控时间。
 */
bool ShareManager::commit_param_watcher(const char* section)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	bool ret = _commit_section(_exchg.c_str(), section);  // 提交分区到交换区
	_secnames[section] = TimeUtils::getLocalTimeNow();  // 记录分区的当前时间作为初始监控时间
	return ret;  // 返回提交结果
}

/**
 * @brief 设置double类型参数
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
bool ShareManager::set_value(const char* section, const char* key, double val)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	return _set_double(_exchg.c_str(), section, key, val);  // 调用设置函数，使用交换区
}

/**
 * @brief 设置uint64类型参数
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
bool ShareManager::set_value(const char* section, const char* key, uint64_t val)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	return _set_uint64(_exchg.c_str(), section, key, val);  // 调用设置函数，使用交换区
}

/**
 * @brief 设置uint32类型参数
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
bool ShareManager::set_value(const char* section, const char* key, uint32_t val)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	return _set_uint32(_exchg.c_str(), section, key, val);  // 调用设置函数，使用交换区
}

/**
 * @brief 设置int64类型参数
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
bool ShareManager::set_value(const char* section, const char* key, int64_t val)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	return _set_int64(_exchg.c_str(), section, key, val);  // 调用设置函数，使用交换区
}

/**
 * @brief 设置int32类型参数
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
bool ShareManager::set_value(const char* section, const char* key, int32_t val)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	return _set_int32(_exchg.c_str(), section, key, val);  // 调用设置函数，使用交换区
}

/**
 * @brief 设置字符串类型参数
 * @param section 分区名称
 * @param key 键名
 * @param val 值
 * @return 设置成功返回true，失败返回false
 */
bool ShareManager::set_value(const char* section, const char* key, const char* val)
{
	if (!_inited)  // 如果未初始化
		return false;  // 返回false

	return _set_string(_exchg.c_str(), section, key, val);  // 调用设置函数，使用交换区
}

/**
 * @brief 获取字符串类型参数
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值，默认为空字符串
 * @return 参数值字符串指针
 */
const char* ShareManager::get_value(const char* section, const char* key, const char* defVal /* = "" */)
{
	if (!_inited)  // 如果未初始化
		return defVal;  // 返回默认值

	return _get_string(_exchg.c_str(), section, key, defVal);  // 调用获取函数，使用交换区
}

/**
 * @brief 获取int32类型参数
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值，默认为0
 * @return 参数值
 */
int32_t ShareManager::get_value(const char* section, const char* key, int32_t defVal /* = 0 */)
{
	if (!_inited)  // 如果未初始化
		return defVal;  // 返回默认值

	return _get_int32(_exchg.c_str(), section, key, defVal);  // 调用获取函数，使用交换区
}

/**
 * @brief 获取int64类型参数
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值，默认为0
 * @return 参数值
 */
int64_t ShareManager::get_value(const char* section, const char* key, int64_t defVal /* = 0 */)
{
	if (!_inited)  // 如果未初始化
		return defVal;  // 返回默认值

	return _get_int64(_exchg.c_str(), section, key, defVal);  // 调用获取函数，使用交换区
}

/**
 * @brief 获取uint32类型参数
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值，默认为0
 * @return 参数值
 */
uint32_t ShareManager::get_value(const char* section, const char* key, uint32_t defVal /* = 0 */)
{
	if (!_inited)  // 如果未初始化
		return defVal;  // 返回默认值

	return _get_uint32(_exchg.c_str(), section, key, defVal);  // 调用获取函数，使用交换区
}

/**
 * @brief 获取uint64类型参数
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值，默认为0
 * @return 参数值
 */
uint64_t ShareManager::get_value(const char* section, const char* key, uint64_t defVal /* = 0 */)
{
	if (!_inited)  // 如果未初始化
		return defVal;  // 返回默认值

	return _get_uint64(_exchg.c_str(), section, key, defVal);  // 调用获取函数，使用交换区
}

/**
 * @brief 获取double类型参数
 * @param section 分区名称
 * @param key 键名
 * @param defVal 默认值，默认为0
 * @return 参数值
 */
double ShareManager::get_value(const char* section, const char* key, double defVal /* = 0 */)
{
	if (!_inited)  // 如果未初始化
		return defVal;  // 返回默认值

	return _get_double(_exchg.c_str(), section, key, defVal);  // 调用获取函数，使用交换区
}

/**
 * @brief 在共享内存中分配字符串类型字段
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
const char* ShareManager::allocate_value(const char* section, const char* key, const char* initVal/* = ""*/, bool bForceWrite/* = false*/, bool isExchg/* = false*/)
{
	if (!_inited)  // 如果未初始化
		return nullptr;  // 返回空指针

	return _allocate_string(isExchg ? _exchg.c_str() : _sync.c_str(), section, key, initVal, bForceWrite);  // 根据isExchg选择域，调用分配函数
}

/**
 * @brief 在共享内存中分配int32类型字段
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @param isExchg 是否使用交换区，默认为false（使用同步区）
 * @return 分配的int32指针，失败返回nullptr
 */
int32_t* ShareManager::allocate_value(const char* section, const char* key, int32_t initVal/* = 0*/, bool bForceWrite/* = false*/, bool isExchg/* = false*/)
{
	if (!_inited)  // 如果未初始化
		return nullptr;  // 返回空指针

	return _allocate_int32(isExchg ? _exchg.c_str() : _sync.c_str(), section, key, initVal, bForceWrite);  // 根据isExchg选择域，调用分配函数
}

/**
 * @brief 在共享内存中分配int64类型字段
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @param isExchg 是否使用交换区，默认为false（使用同步区）
 * @return 分配的int64指针，失败返回nullptr
 */
int64_t* ShareManager::allocate_value(const char* section, const char* key, int64_t initVal/* = 0*/, bool bForceWrite/* = false*/, bool isExchg/* = false*/)
{
	if (!_inited)  // 如果未初始化
		return nullptr;  // 返回空指针

	return _allocate_int64(isExchg ? _exchg.c_str() : _sync.c_str(), section, key, initVal, bForceWrite);  // 根据isExchg选择域，调用分配函数
}

/**
 * @brief 在共享内存中分配uint32类型字段
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @param isExchg 是否使用交换区，默认为false（使用同步区）
 * @return 分配的uint32指针，失败返回nullptr
 */
uint32_t* ShareManager::allocate_value(const char* section, const char* key, uint32_t initVal/* = 0*/, bool bForceWrite/* = false*/, bool isExchg/* = false*/)
{
	if (!_inited)  // 如果未初始化
		return nullptr;  // 返回空指针

	return _allocate_uint32(isExchg ? _exchg.c_str() : _sync.c_str(), section, key, initVal, bForceWrite);  // 根据isExchg选择域，调用分配函数
}

/**
 * @brief 在共享内存中分配uint64类型字段
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @param isExchg 是否使用交换区，默认为false（使用同步区）
 * @return 分配的uint64指针，失败返回nullptr
 */
uint64_t* ShareManager::allocate_value(const char* section, const char* key, uint64_t initVal/* = 0*/, bool bForceWrite/* = false*/, bool isExchg/* = false*/)
{
	if (!_inited)  // 如果未初始化
		return nullptr;  // 返回空指针

	return _allocate_uint64(isExchg ? _exchg.c_str() : _sync.c_str(), section, key, initVal, bForceWrite);  // 根据isExchg选择域，调用分配函数
}

/**
 * @brief 在共享内存中分配double类型字段
 * @param section 分区名称
 * @param key 键名
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @param isExchg 是否使用交换区，默认为false（使用同步区）
 * @return 分配的double指针，失败返回nullptr
 */
double* ShareManager::allocate_value(const char* section, const char* key, double initVal/* = 0*/, bool bForceWrite/* = false*/, bool isExchg/* = false*/)
{
	if (!_inited)  // 如果未初始化
		return nullptr;  // 返回空指针

	return _allocate_double(isExchg ? _exchg.c_str() : _sync.c_str(), section, key, initVal, bForceWrite);  // 根据isExchg选择域，调用分配函数
}