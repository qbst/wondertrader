/*!
 * \file WtFilterMgr.cpp
 * \project	WonderTrader
 * 
 * \brief 过滤器管理器类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtFilterMgr类的所有方法，提供了过滤器配置的加载和执行功能。
 * 主要实现包括：
 * 1. 配置加载：从JSON配置文件加载过滤器规则，支持热重载
 * 2. 策略过滤：检查策略信号是否被过滤，支持忽略和重定向操作
 * 3. 合约过滤：检查合约信号是否被过滤，支持合约代码和品种代码匹配
 * 4. 执行器过滤：检查执行器是否被禁用
 * 5. 日志记录：记录过滤器触发和配置加载的详细信息
 * 
 * 实现细节：
 * - 使用WTSVariant解析JSON配置文件
 * - 使用boost::filesystem检测文件修改时间
 * - 支持增量头寸的特殊处理逻辑
 * - 合约代码优先级高于品种代码
 */
#include "WtFilterMgr.h"  // 包含过滤器管理器头文件
#include "EventNotifier.h"  // 包含事件通知器头文件

#include "../Share/CodeHelper.hpp"  // 包含合约代码解析辅助工具
#include "../Includes/WTSVariant.hpp"  // 包含变体类型，用于解析JSON配置
#include "../WTSUtils/WTSCfgLoader.h"  // 包含配置文件加载工具
#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

#include <boost/filesystem.hpp>  // 包含boost文件系统库，用于文件时间戳检查

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 加载信号过滤器配置
 * @param fileName 过滤器配置文件路径，为空字符串时使用上次设置的路径
 * 
 * 该函数从配置文件加载过滤器规则，支持热重载。
 * 加载流程：
 * 1. 确定配置文件路径（如果传入新路径则更新，否则使用上次路径）
 * 2. 检查文件是否存在，不存在则记录调试日志并返回
 * 3. 检查文件修改时间，如果未修改则跳过加载
 * 4. 如果文件被修改，记录信息日志并通知事件通知器
 * 5. 解析配置文件，加载三类过滤器：
 *    - strategy_filters: 策略过滤器
 *    - code_filters: 合约代码过滤器
 *    - executer_filters: 执行器过滤器
 * 6. 更新文件时间戳
 * 
 * 配置文件格式（JSON）：
 * {
 *   "strategy_filters": {
 *     "策略名": {
 *       "action": "ignore|redirect",
 *       "target": 目标仓位值（仅redirect时有效）
 *     }
 *   },
 *   "code_filters": {
 *     "合约代码": {
 *       "action": "ignore|redirect",
 *       "target": 目标仓位值（仅redirect时有效）
 *     }
 *   },
 *   "executer_filters": {
 *     "执行器ID": true/false  // true表示禁用，false表示启用
 *   }
 * }
 */
void WtFilterMgr::load_filters(const char* fileName)
{
	if (_filter_file.empty() && (strlen(fileName) == 0))  // 如果配置文件路径为空且未传入新路径
		return;  // 直接返回，不进行加载

	if(strlen(fileName) > 0)  // 如果传入了新的文件路径
		_filter_file = fileName;  // 更新配置文件路径

	if (!StdFile::exists(_filter_file.c_str()))  // 检查配置文件是否存在
	{
		WTSLogger::debug("Filters configuration file {} not exists", _filter_file);  // 记录调试日志：文件不存在
		return;  // 文件不存在则返回
	}

	uint64_t lastModTime = boost::filesystem::last_write_time(boost::filesystem::path(_filter_file));  // 获取文件最后修改时间戳
	if (lastModTime <= _filter_timestamp)  // 如果文件未被修改
		return;  // 直接返回，不重新加载

	if (_filter_timestamp != 0)  // 如果之前已经加载过（时间戳不为0）
	{
		WTSLogger::info("Filters configuration file {} modified, will be reloaded", _filter_file);  // 记录信息日志：文件被修改，将重新加载
		if (_notifier)  // 如果事件通知器已设置
			_notifier->notify_event("Filter file has been reloaded");  // 通知过滤器文件已重新加载
	}

	WTSVariant* cfg = WTSCfgLoader::load_from_file(_filter_file.c_str());  // 从文件加载JSON配置，返回WTSVariant对象

	_filter_timestamp = lastModTime;  // 更新文件时间戳

	_stra_filters.clear();  // 清空策略过滤器映射表
	_code_filters.clear();  // 清空代码过滤器映射表
	_exec_filters.clear();  // 清空执行器过滤器映射表

	//读策略过滤器
	WTSVariant* filterStra = cfg->get("strategy_filters");  // 获取策略过滤器配置节点
	if (filterStra)  // 如果配置节点存在
	{
		auto keys = filterStra->memberNames();  // 获取所有成员名称（策略名称列表）
		for (const std::string& key : keys)  // 遍历每个策略名称
		{
			WTSVariant* cfgItem = filterStra->get(key.c_str());  // 获取该策略的过滤器配置项
			const char* action = cfgItem->getCString("action");  // 获取操作类型字符串（"ignore"或"redirect"）
			FilterAction fAct = FA_None;  // 初始化过滤器操作为无操作
			if (wt_stricmp(action, "ignore") == 0)  // 如果操作类型为"ignore"（忽略）
				fAct = FA_Ignore;  // 设置为忽略操作
			else if (wt_stricmp(action, "redirect") == 0)  // 如果操作类型为"redirect"（重定向）
				fAct = FA_Redirect;  // 设置为重定向操作

			if (fAct == FA_None)  // 如果操作类型未识别
			{
				WTSLogger::error("Action {} of strategy filter {} not recognized", action, key);  // 记录错误日志：操作类型未识别
				continue;  // 跳过该过滤器项
			}

			FilterItem& fItem = _stra_filters[key];  // 在策略过滤器映射表中创建或获取过滤器项引用
			fItem._key = key;  // 设置过滤器关键字（策略名称）
			fItem._action = fAct;  // 设置过滤器操作类型
			fItem._target = cfgItem->getDouble("target");  // 获取目标仓位值（仅redirect时有效）

			WTSLogger::info("Strategy filter {} loaded", key);  // 记录信息日志：策略过滤器加载成功
		}
	}

	//读代码过滤器
	WTSVariant* filterCodes = cfg->get("code_filters");  // 获取代码过滤器配置节点
	if (filterCodes)  // 如果配置节点存在
	{
		auto codes = filterCodes->memberNames();  // 获取所有成员名称（合约代码列表）
		for (const std::string& stdCode : codes)  // 遍历每个合约代码
		{

			WTSVariant* cfgItem = filterCodes->get(stdCode.c_str());  // 获取该合约的过滤器配置项
			const char* action = cfgItem->getCString("action");  // 获取操作类型字符串（"ignore"或"redirect"）
			FilterAction fAct = FA_None;  // 初始化过滤器操作为无操作
			if (wt_stricmp(action, "ignore") == 0)  // 如果操作类型为"ignore"（忽略）
				fAct = FA_Ignore;  // 设置为忽略操作
			else if (wt_stricmp(action, "redirect") == 0)  // 如果操作类型为"redirect"（重定向）
				fAct = FA_Redirect;  // 设置为重定向操作

			if (fAct == FA_None)  // 如果操作类型未识别
			{
				WTSLogger::error("Action {} of code filter {} not recognized", action, stdCode);  // 记录错误日志：操作类型未识别
				continue;  // 跳过该过滤器项
			}

			FilterItem& fItem = _code_filters[stdCode];  // 在代码过滤器映射表中创建或获取过滤器项引用
			fItem._key = stdCode;  // 设置过滤器关键字（合约代码）
			fItem._action = fAct;  // 设置过滤器操作类型
			fItem._target = cfgItem->getDouble("target");  // 获取目标仓位值（仅redirect时有效）

			WTSLogger::info("Code filter {} loaded", stdCode);  // 记录信息日志：代码过滤器加载成功
		}
	}

	//读通道过滤器
	WTSVariant* filterExecuters = cfg->get("executer_filters");  // 获取执行器过滤器配置节点
	if (filterExecuters)  // 如果配置节点存在
	{
		auto executer_ids = filterExecuters->memberNames();  // 获取所有成员名称（执行器ID列表）
		for (const std::string& execid : executer_ids)  // 遍历每个执行器ID
		{
			bool bDisabled = filterExecuters->getBoolean(execid.c_str());  // 获取该执行器的禁用状态（true表示禁用）
			WTSLogger::info("Executer {} is %s", execid, bDisabled?"disabled":"enabled");  // 记录信息日志：执行器状态
			_exec_filters[execid] = bDisabled;  // 将执行器ID和禁用状态存入映射表
		}
	}

	cfg->release();  // 释放配置对象内存
}

/**
 * @brief 检查执行器是否被过滤
 * @param execid 交易通道ID（执行器标识）
 * @return bool 如果被过滤（禁用）返回true，否则返回false
 * 
 * 该函数检查指定执行器是否在过滤器配置中被禁用。
 * 如果执行器被禁用，该执行器将不执行任何交易信号。
 */
bool WtFilterMgr::is_filtered_by_executer(const char* execid)
{
	auto it = _exec_filters.find(execid);  // 在执行器过滤器映射表中查找指定执行器
	if (it == _exec_filters.end())  // 如果执行器未在配置中
		return false;  // 返回false，表示未被过滤（启用状态）

	return it->second;  // 返回执行器的禁用状态（true表示禁用，false表示启用）
}

/**
 * @brief 过滤器操作名称数组
 * 
 * 该数组用于将过滤器操作枚举值转换为可读的字符串名称，
 * 用于日志输出和调试。
 */
const char* FLTACT_NAMEs[] =
{
	"Ignore",  // FA_Ignore对应的字符串名称
	"Redirect"  // FA_Redirect对应的字符串名称
};

/**
 * @brief 检查策略是否被过滤
 * @param straName 策略名称
 * @param targetPos 目标仓位引用，如果过滤器是重定向操作，该值会被修改为目标仓位
 * @param isDiff 是否是增量头寸，默认为false
 * @return bool 如果被过滤（忽略）返回true，否则返回false
 * 
 * 该函数检查指定策略的信号是否被过滤器过滤掉。
 * 处理逻辑：
 * 1. 在策略过滤器映射表中查找指定策略
 * 2. 如果找到过滤器：
 *    - 如果是增量头寸（isDiff=true），直接返回true忽略该变动
 *    - 如果是忽略操作（FA_Ignore），返回true
 *    - 如果是重定向操作（FA_Redirect），修改targetPos为目标值，返回false
 * 3. 如果未找到过滤器，返回false（未被过滤）
 * 
 * 特殊处理：
 * - 增量头寸如果被过滤器触发，直接忽略，不进行重定向操作
 */
bool WtFilterMgr::is_filtered_by_strategy(const char* straName, double& targetPos, bool isDiff /* = false */)
{
	auto it = _stra_filters.find(straName);  // 在策略过滤器映射表中查找指定策略
	if (it != _stra_filters.end())  // 如果找到了过滤器
	{
		const FilterItem& fItem = it->second;  // 获取过滤器项引用
		if(isDiff)  // 如果是增量头寸
		{
			//如果过滤器触发，并且是增量头寸，则直接过滤掉
			WTSLogger::info("[Filters] Strategy filter {} triggered, the change of position ignored directly", straName);  // 记录信息日志：增量头寸被直接忽略
			return true;  // 返回true，表示被过滤
		}

		WTSLogger::info("[Filters] Strategy filter {} triggered, action: {}", straName, fItem._action <= FA_Redirect ? FLTACT_NAMEs[fItem._action] : "Unknown");  // 记录信息日志：过滤器触发，显示操作类型
		if (fItem._action == FA_Ignore)  // 如果操作类型为忽略
		{
			return true;  // 返回true，表示被过滤
		}
		else if (fItem._action == FA_Redirect)  // 如果操作类型为重定向
		{
			//只有不是增量的时候,才有效
			targetPos = fItem._target;  // 修改目标仓位为目标值
		}

		return false;  // 返回false，表示未被过滤（但可能已被重定向）
	}

	return false;  // 未找到过滤器，返回false（未被过滤）
}

/**
 * @brief 检查合约是否被过滤
 * @param stdCode 标准合约代码
 * @param targetPos 目标仓位引用，如果过滤器是重定向操作，该值会被修改为目标仓位
 * @return bool 如果被过滤（忽略）返回true，否则返回false
 * 
 * 该函数检查指定合约的信号是否被过滤器过滤掉。
 * 处理逻辑：
 * 1. 解析合约代码，提取品种代码
 * 2. 首先在代码过滤器映射表中查找完整合约代码
 * 3. 如果找到，执行相应的过滤操作（忽略或重定向）
 * 4. 如果未找到完整合约代码，则查找品种代码
 * 5. 如果找到品种代码过滤器，执行相应的过滤操作
 * 6. 如果都未找到，返回false（未被过滤）
 * 
 * 匹配优先级：
 * - 合约代码优先级高于品种代码
 * - 同一时间只有一个过滤器生效
 */
bool WtFilterMgr::is_filtered_by_code(const char* stdCode, double& targetPos)
{
	CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(stdCode, NULL);  // 解析标准合约代码，提取品种代码等信息
	auto cit = _code_filters.find(stdCode);  // 在代码过滤器映射表中查找完整合约代码
	if (cit != _code_filters.end())  // 如果找到了合约代码过滤器
	{
		const FilterItem& fItem = cit->second;  // 获取过滤器项引用
		WTSLogger::info("[Filters] Code filter {} triggered, action: {}", stdCode, fItem._action <= FA_Redirect ? FLTACT_NAMEs[fItem._action] : "Unknown");  // 记录信息日志：过滤器触发，显示操作类型
		if (fItem._action == FA_Ignore)  // 如果操作类型为忽略
		{
			return true;  // 返回true，表示被过滤
		}
		else if (fItem._action == FA_Redirect)  // 如果操作类型为重定向
		{
			targetPos = fItem._target;  // 修改目标仓位为目标值
		}

		return false;  // 返回false，表示未被过滤（但可能已被重定向）
	}

	cit = _code_filters.find(cInfo.stdCommID());  // 在代码过滤器映射表中查找品种代码
	if (cit != _code_filters.end())  // 如果找到了品种代码过滤器
	{
		const FilterItem& fItem = cit->second;  // 获取过滤器项引用
		WTSLogger::info("[Filters] CommID filter {} triggered, action: {}", cInfo.stdCommID(), fItem._action <= FA_Redirect ? FLTACT_NAMEs[fItem._action] : "Unknown");  // 记录信息日志：品种代码过滤器触发，显示操作类型
		if (fItem._action == FA_Ignore)  // 如果操作类型为忽略
		{
			return true;  // 返回true，表示被过滤
		}
		else if (fItem._action == FA_Redirect)  // 如果操作类型为重定向
		{
			targetPos = fItem._target;  // 修改目标仓位为目标值
		}

		return false;  // 返回false，表示未被过滤（但可能已被重定向）
	}

	return false;  // 未找到过滤器，返回false（未被过滤）
}



