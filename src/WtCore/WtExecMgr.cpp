/*!
 * \file WtExecMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 执行器管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件实现了WtExecuterMgr类的所有方法，提供执行器管理功能。
 * 
 * 实现要点：
 * 1. 执行器枚举：遍历所有执行器，调用回调函数
 * 2. 仓位设置：将目标仓位分发给执行器，支持信号过滤
 * 3. 仓位变动处理：处理单个合约的仓位变动，支持路由规则
 * 4. 行情转发：将Tick数据转发给所有执行器
 * 5. 路由规则加载：从配置加载策略到执行器的路由规则
 * 6. 批量提交：支持批量收集和提交目标仓位
 * 
 * 关键算法：
 * - 信号过滤：对仓位信号进行过滤或调整
 * - 路由匹配：根据路由规则匹配执行器
 * - 批量累加：多个策略的信号累加后统一提交
 */
#include "WtExecMgr.h"  // 包含执行器管理器头文件
#include "WtFilterMgr.h"  // 包含过滤器管理器头文件

#include "../Share/decimal.h"  // 包含小数运算工具头文件
#include "../Includes/WTSVariant.hpp"  // 包含变体类型头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志工具头文件

USING_NS_WTP;  // 使用WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
#pragma region "WtExecuterMgr"  // 开始执行器管理器区域
/**
 * @brief 枚举执行器
 * @param cb 回调函数，对每个执行器调用一次
 * 
 * 遍历所有执行器，对每个执行器调用回调函数。
 */
void WtExecuterMgr::enum_executer(EnumExecuterCb cb)
{
	for (auto& v : _executers)  // 遍历执行器映射表
	{
		ExecCmdPtr& executer = (ExecCmdPtr&)v.second;  // 获取执行器智能指针（const_cast，因为回调函数需要非const引用）
		cb(executer);  // 调用回调函数，传递执行器指针
	}
}

/**
 * @brief 设置目标仓位
 * @param target_pos 目标仓位映射表（键为合约代码，值为目标仓位）
 * 
 * 将目标仓位分发给所有执行器。
 * 如果配置了过滤器，会对仓位信号进行过滤或调整。
 * 
 * 实现逻辑：
 * 1. 如果配置了过滤器，对所有仓位信号进行过滤
 * 2. 遍历所有执行器，如果执行器未被过滤，则设置目标仓位
 */
void WtExecuterMgr::set_positions(wt_hashmap<std::string, double> target_pos)
{
	if(_filter_mgr != NULL)  // 如果配置了过滤器管理器
	{
		wt_hashmap<std::string, double> des_port;  // 创建目标仓位映射表（过滤后的）
		for(auto& m : target_pos)  // 遍历原始目标仓位映射表
		{
			const auto& stdCode = m.first;  // 获取合约代码
			double& desVol = (double&)m.second;  // 获取目标仓位（const_cast，因为过滤器可能需要修改）
			double oldVol = desVol;  // 保存原始目标仓位

			bool isFltd = _filter_mgr->is_filtered_by_code(stdCode.c_str(), desVol);  // 检查合约是否被过滤
			if (!isFltd)  // 如果合约未被过滤
			{
				if (!decimal::eq(desVol, oldVol))  // 如果目标仓位被过滤器调整了
				{
					//输出日志
					WTSLogger::info("[Filters] {} target position reset by code filter: {} -> {}", stdCode.c_str(), oldVol, desVol);  // 记录日志：目标仓位被过滤器调整
				}

				des_port[stdCode] = desVol;  // 将过滤后的目标仓位添加到新映射表
			}
			else  // 如果合约被过滤
			{
				//输出日志
				WTSLogger::info("[Filters] {} target position ignored by filter", stdCode.c_str());  // 记录日志：目标仓位被过滤器忽略
			}
		}

		des_port.swap(target_pos);  // 交换映射表（使用过滤后的映射表）
	}

	for (auto& v : _executers)  // 遍历所有执行器
	{
		ExecCmdPtr& executer = (ExecCmdPtr&)v.second;  // 获取执行器智能指针

		if (_filter_mgr && _filter_mgr->is_filtered_by_executer(executer->name()))  // 如果执行器被过滤器过滤
		{
			WTSLogger::info("[Filters] Executer {} is filtered, all signals will be ignored", executer->name());  // 记录日志：执行器被过滤
			continue;  // 跳过当前执行器
		}
		executer->set_position(target_pos);  // 设置执行器的目标仓位
	}
}

/**
 * @brief 处理仓位变动
 * @param stdCode 标准合约代码字符串
 * @param targetPos 目标仓位
 * @param diffPos 差量仓位（目标仓位 - 当前仓位）
 * @param execid 执行器ID，默认为"ALL"（表示所有执行器）
 * 
 * 处理单个合约的仓位变动，通知对应的执行器。
 * 如果配置了路由规则，只会通知匹配的执行器。
 * 如果配置了过滤器，会对仓位信号进行过滤或调整。
 * 
 * 实现逻辑：
 * 1. 如果配置了过滤器，对仓位信号进行过滤或调整
 * 2. 遍历所有执行器，检查路由规则
 * 3. 如果执行器匹配路由规则，通知执行器仓位变动
 */
void WtExecuterMgr::handle_pos_change(const char* stdCode, double targetPos, double diffPos, const char* execid /* = "ALL" */)
{
	if(_filter_mgr != NULL)  // 如果配置了过滤器管理器
	{
		double oldVol = targetPos;  // 保存原始目标仓位
		bool isFltd = _filter_mgr->is_filtered_by_code(stdCode, targetPos);  // 检查合约是否被过滤，并可能调整目标仓位
		if (!isFltd)  // 如果合约未被过滤
		{
			if (!decimal::eq(targetPos, oldVol))  // 如果目标仓位被过滤器调整了
			{
				//输出日志
				WTSLogger::info("[Filters] {} target position reset by filter: {} -> {}", stdCode, oldVol, targetPos);  // 记录日志：目标仓位被过滤器调整
				//差量也要重算
				diffPos += (targetPos - oldVol);  // 重新计算差量仓位（加上调整的差值）
			}
		}
		else  // 如果合约被过滤
		{
			//输出日志
			WTSLogger::info("[Filters] {} target position ignored by filter", stdCode);  // 记录日志：目标仓位被过滤器忽略
			return;  // 直接返回，不通知执行器
		}
	}

	for (auto& v : _executers)  // 遍历所有执行器
	{
		ExecCmdPtr& executer = (ExecCmdPtr&)v.second;  // 获取执行器智能指针

		if (_filter_mgr && _filter_mgr->is_filtered_by_executer(executer->name()))  // 如果执行器被过滤器过滤
		{
			WTSLogger::info("[Filters] All signals to executer {} are ignored by executer filter", executer->name());  // 记录日志：执行器被过滤
			continue;  // 跳过当前执行器
		}

		auto it = _routed_executers.find(executer->name());  // 查找执行器是否配置了路由规则
		if (it == _routed_executers.end() && strcmp(execid, "ALL") == 0)  // 如果执行器未配置路由规则且execid为"ALL"
			executer->on_position_changed(stdCode, diffPos);  // 通知执行器仓位变动（差量仓位）
		else if(strcmp(executer->name(), execid) == 0)  // 如果执行器名称匹配execid
			executer->on_position_changed(stdCode, diffPos);  // 通知执行器仓位变动（差量仓位）
	}
}

/**
 * @brief 处理Tick数据
 * @param stdCode 标准合约代码字符串
 * @param curTick 当前Tick数据指针
 * 
 * 将Tick数据转发给所有执行器。
 * 
 * 实现逻辑：
 * 1. 遍历所有执行器
 * 2. 将Tick数据传递给每个执行器
 */
void WtExecuterMgr::handle_tick(const char* stdCode, WTSTickData* curTick)
{
	//for (auto it = _executers.begin(); it != _executers.end(); it++)  // 已注释的旧实现方式
	//{
	//	ExecCmdPtr& executer = (*it);
	//	executer->on_tick(stdCode, curTick);
	//}

	for (auto& v : _executers)  // 遍历所有执行器
	{
		ExecCmdPtr& executer = (ExecCmdPtr&)v.second;  // 获取执行器智能指针
		executer->on_tick(stdCode, curTick);  // 将Tick数据传递给执行器
	}
}

/**
 * @brief 将目标仓位加入缓存
 * @param stdCode 合约代码字符串
 * @param targetPos 目标仓位
 * @param execid 执行器ID，默认为"ALL"
 * 
 * 将目标仓位添加到缓存中，等待批量提交。
 * 如果同一合约在同一执行器下已有缓存，则累加仓位。
 */
void WtExecuterMgr::add_target_to_cache(const char* stdCode, double targetPos, const char* execid /* = "ALL" */)
{
	TargetsMap& targets = _all_cached_targets[execid];  // 获取或创建执行器的目标仓位映射表
	double& vol = targets[stdCode];  // 获取或创建合约的目标仓位
	vol += targetPos;  // 累加目标仓位（多个策略的信号累加）
}

/**
 * @brief 提交缓存的目标仓位
 * @param scale 风控系数，用于缩放目标仓位，默认为1.0
 * 
 * 将缓存的目标仓位统一提交给各个执行器。
 * 会对目标仓位应用风控系数和过滤器。
 * 提交完成后会清空缓存。
 * 
 * 实现逻辑：
 * 1. 对每个执行器的目标仓位应用风控系数
 * 2. 对目标仓位进行过滤
 * 3. 根据路由规则，将目标仓位提交给对应的执行器
 * 4. 清空缓存
 */
void WtExecuterMgr::commit_cached_targets(double scale /* = 1.0 */)
{
	for(auto& v : _all_cached_targets)  // 遍历所有缓存的目标仓位
	{	
		//先对组合进行缩放
		const char* execid = v.first.c_str();  // 获取执行器ID
		TargetsMap& target_pos = (TargetsMap&)v.second;  // 获取目标仓位映射表（const_cast）
		for(auto& item : target_pos)  // 遍历目标仓位映射表
		{
			const auto& stdCode = item.first;  // 获取合约代码
			double& pos = (double&)item.second;  // 获取目标仓位（const_cast）

			if(decimal::eq(pos, 0))  // 如果目标仓位为0（使用小数比较，避免浮点误差）
				continue;  // 跳过当前合约

			double symbol = pos / abs(pos);  // 计算仓位符号（1表示多仓，-1表示空仓）
			pos = decimal::rnd(abs(pos)*scale)*symbol;  // 应用风控系数：取绝对值，乘以系数，四舍五入，再乘以符号
		}

		//然后根据过滤器调整目标仓位
		if (_filter_mgr != NULL)  // 如果配置了过滤器管理器
		{
			TargetsMap des_port;  // 创建目标仓位映射表（过滤后的）
			for (auto& m : target_pos)  // 遍历原始目标仓位映射表
			{
				const auto& stdCode = m.first;  // 获取合约代码
				double& desVol = (double&)m.second;  // 获取目标仓位（const_cast）
				double oldVol = desVol;  // 保存原始目标仓位

				bool isFltd = _filter_mgr->is_filtered_by_code(stdCode.c_str(), desVol);  // 检查合约是否被过滤
				if (!isFltd)  // 如果合约未被过滤
				{
					if (!decimal::eq(desVol, oldVol))  // 如果目标仓位被过滤器调整了
					{
						//输出日志
						WTSLogger::info("[Filters] {} target position reset by code filter: {} -> {}", stdCode.c_str(), oldVol, desVol);  // 记录日志：目标仓位被过滤器调整
					}

					des_port[stdCode] = desVol;  // 将过滤后的目标仓位添加到新映射表
				}
				else  // 如果合约被过滤
				{
					//输出日志
					WTSLogger::info("[Filters] {} target position ignored by filter", stdCode.c_str());  // 记录日志：目标仓位被过滤器忽略
				}
			}

			target_pos.swap(des_port);  // 交换映射表（使用过滤后的映射表）
		}
	}

	//遍历执行器
	for (auto& e : _executers)  // 遍历所有执行器
	{
		ExecCmdPtr& executer = (ExecCmdPtr&)e.second;  // 获取执行器智能指针
		if (_filter_mgr && _filter_mgr->is_filtered_by_executer(executer->name()))  // 如果执行器被过滤器过滤
		{
			WTSLogger::info("[Filters] Executer {} is filtered, all signals will be ignored", executer->name());  // 记录日志：执行器被过滤
			continue;  // 跳过当前执行器
		}

		//先找自己对应的组合
		auto it = _all_cached_targets.find(executer->name());  // 查找执行器名称对应的目标仓位映射表

		//如果找不到，就找全部组合
		if (it == _all_cached_targets.end())  // 如果未找到
			it = _all_cached_targets.find("ALL");  // 查找"ALL"对应的目标仓位映射表

		if (it == _all_cached_targets.end())  // 如果仍未找到
			continue;  // 跳过当前执行器

		executer->set_position(it->second);  // 设置执行器的目标仓位（使用找到的目标仓位映射表）
	}

	//提交完了以后，清理掉全部缓存的目标仓位
	_all_cached_targets.clear();  // 清空所有缓存的目标仓位
}

/**
 * @brief 加载路由规则
 * @param config 路由规则配置（JSON格式）
 * @return bool 加载成功返回true，否则返回false
 * 
 * 从配置中加载策略到执行器的路由规则。
 * 配置格式为数组，每个元素包含策略名称和执行器ID列表。
 * 
 * 实现逻辑：
 * 1. 检查配置是否为数组格式
 * 2. 遍历数组，解析每个路由规则
 * 3. 支持单个执行器ID和多个执行器ID列表
 * 4. 记录已路由的执行器ID
 */
bool WtExecuterMgr::load_router_rules(WTSVariant* config)
{
	if (config == NULL || !config->isArray())  // 如果配置为空或不是数组格式
		return false;  // 返回false

	for(uint32_t i = 0; i < config->size(); i++)  // 遍历配置数组
	{
		WTSVariant* item = config->get(i);  // 获取数组元素
		const char* straName = item->getCString("strategy");  // 获取策略名称（键名为"strategy"）
		WTSVariant* itemExec = item->get("executer");  // 获取执行器配置（键名为"executer"）
		if(itemExec->isArray())  // 如果执行器配置是数组（多个执行器）
		{
			uint32_t cnt = itemExec->size();  // 获取数组长度
			for(uint32_t k = 0; k < cnt; k++)  // 遍历执行器数组
			{
				const char* execId = itemExec->get(k)->asCString();  // 获取执行器ID（字符串）
				_router_rules[straName].insert(execId);  // 将执行器ID添加到策略的路由规则中
				WTSLogger::info("Signal of strategy {} will be routed to executer {}", straName, execId);  // 记录日志：策略信号将路由到执行器
				_routed_executers.insert(execId);  // 将执行器ID添加到已路由的执行器集合中
			}
		}
		else  // 如果执行器配置是单个值（单个执行器）
		{
			const char* execId = itemExec->asCString();  // 获取执行器ID（字符串）
			_router_rules[straName].insert(execId);  // 将执行器ID添加到策略的路由规则中
			WTSLogger::info("Signal of strategy {} will be routed to executer {}", straName, execId);  // 记录日志：策略信号将路由到执行器
			_routed_executers.insert(execId);  // 将执行器ID添加到已路由的执行器集合中
		}
	}

	WTSLogger::info("{} router rules loaded", _router_rules.size());  // 记录日志：加载了多少个路由规则

	return true;  // 加载成功，返回true
}

#pragma endregion "WtExecuterMgr"  // 结束执行器管理器区域
