/*!
 * \file WtExecMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 执行器管理器头文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件定义了WtExecuterMgr类，用于管理多个执行器实例。
 * 
 * 核心功能：
 * 1. 执行器管理：注册和管理多个执行器实例
 * 2. 仓位分发：将策略的目标仓位分发给各个执行器
 * 3. 路由管理：支持策略到执行器的路由规则配置
 * 4. 信号过滤：集成信号过滤器，对仓位信号进行过滤或调整
 * 5. 批量提交：支持批量收集目标仓位，统一提交给执行器
 * 6. 行情转发：将行情数据转发给所有执行器
 * 
 * 设计特点：
 * - 私有继承boost::noncopyable，禁止复制和赋值
 * - 支持多个执行器，每个执行器可以处理不同的合约
 * - 支持策略到执行器的路由规则，可以实现策略信号的路由分发
 * - 支持信号过滤，可以对仓位信号进行过滤或调整
 * - 支持批量提交机制，可以收集多个策略的信号后统一提交
 * 
 * 使用场景：
 * - 多策略组合：多个策略共享多个执行器
 * - 路由分发：不同策略的信号路由到不同的执行器
 * - 信号过滤：对所有仓位信号进行统一过滤
 */
#pragma once  // 防止头文件重复包含
#include <functional>  // 包含函数对象头文件（用于定义回调函数类型）
#include "WtLocalExecuter.h"  // 包含本地执行器头文件（定义了ExecCmdPtr类型）

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WtFilterMgr;  // 前向声明：过滤器管理器类

typedef std::function<void(ExecCmdPtr)> EnumExecuterCb;  // 执行器枚举回调类型定义：参数为执行器智能指针，无返回值

/**
 * @class WtExecuterMgr
 * @brief 执行器管理器类
 * 
 * 该类负责管理多个执行器实例，提供统一的接口来操作执行器。
 * 支持仓位分发、路由管理、信号过滤等功能。
 * 
 * 主要职责：
 * 1. 注册和管理执行器实例
 * 2. 将策略的目标仓位分发给执行器
 * 3. 管理策略到执行器的路由规则
 * 4. 集成信号过滤器，对仓位信号进行过滤
 * 5. 支持批量提交机制，统一提交目标仓位
 * 6. 将行情数据转发给所有执行器
 * 
 * 设计模式：
 * - 管理器模式：统一管理多个执行器实例
 * - 策略模式：通过路由规则实现不同的分发策略
 * - 观察者模式：通过回调函数枚举执行器
 */
class WtExecuterMgr : private boost::noncopyable  // 私有继承boost::noncopyable，禁止复制和赋值
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化执行器管理器，过滤器管理器指针初始化为NULL。
	 */
	WtExecuterMgr():_filter_mgr(NULL){}  // 初始化过滤器管理器指针为NULL

	/**
	 * @brief 设置过滤器管理器
	 * @param mgr 过滤器管理器指针
	 * 
	 * 设置过滤器管理器，用于对仓位信号进行过滤或调整。
	 */
	inline void set_filter_mgr(WtFilterMgr* mgr) { _filter_mgr = mgr; }  // 设置过滤器管理器指针

	/**
	 * @brief 添加执行器
	 * @param executer 执行器智能指针
	 * 
	 * 将执行器添加到管理器，使用执行器名称作为键。
	 */
	inline void	add_executer(ExecCmdPtr executer)
	{
		_executers[executer->name()] = executer;  // 将执行器添加到映射表，键为执行器名称
	}

	/**
	 * @brief 枚举执行器
	 * @param cb 回调函数，对每个执行器调用一次
	 * 
	 * 遍历所有执行器，对每个执行器调用回调函数。
	 */
	void	enum_executer(EnumExecuterCb cb);

	/**
	 * @brief 设置目标仓位
	 * @param target_pos 目标仓位映射表（键为合约代码，值为目标仓位）
	 * 
	 * 将目标仓位分发给所有执行器。
	 * 如果配置了过滤器，会对仓位信号进行过滤或调整。
	 */
	void	set_positions(wt_hashmap<std::string, double> target_pos);

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
	 */
	void	handle_pos_change(const char* stdCode, double targetPos, double diffPos, const char* execid = "ALL");

	/**
	 * @brief 处理Tick数据
	 * @param stdCode 标准合约代码字符串
	 * @param curTick 当前Tick数据指针
	 * 
	 * 将Tick数据转发给所有执行器。
	 */
	void	handle_tick(const char* stdCode, WTSTickData* curTick);

	/**
	 * @brief 加载路由规则
	 * @param config 路由规则配置（JSON格式）
	 * @return bool 加载成功返回true，否则返回false
	 * 
	 * 从配置中加载策略到执行器的路由规则。
	 * 配置格式为数组，每个元素包含策略名称和执行器ID列表。
	 */
	bool	load_router_rules(WTSVariant* config);

	/**
	 * @brief 获取路由规则
	 * @param strategyid 策略ID字符串
	 * @return const wt_hashset<std::string>& 返回执行器ID集合的引用
	 *	
	 * 获取指定策略的路由规则，返回该策略应该路由到的执行器ID集合。
	 * 如果未配置路由规则，返回包含"ALL"的集合（表示所有执行器）。
	 */
	inline const wt_hashset<std::string>& get_route(const char* strategyid)
	{
		static wt_hashset<std::string> ALL_EXECUTERS;  // 静态变量，用于存储"ALL"执行器ID
		if (ALL_EXECUTERS.empty())  // 如果集合为空（首次调用）
			ALL_EXECUTERS.insert("ALL");  // 插入"ALL"执行器ID

		if (_router_rules.empty())  // 如果路由规则为空
			return ALL_EXECUTERS;  // 返回包含"ALL"的集合

		auto it = _router_rules.find(strategyid);  // 查找策略的路由规则
		if (it == _router_rules.end())  // 如果未找到
			return ALL_EXECUTERS;  // 返回包含"ALL"的集合

		return it->second;  // 返回该策略的执行器ID集合
	}

	/**
	 * @brief 清除缓存的目标仓位
	 * 
	 * 清除所有缓存的目标仓位，用于重新开始批量收集。
	 */
	inline void	clear_cached_targets()
	{
		_all_cached_targets.clear();  // 清空所有缓存的目标仓位
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
	void	add_target_to_cache(const char* stdCode, double targetPos, const char* execid = "ALL");

	/**
	 * @brief 提交缓存的目标仓位
	 * @param scale 风控系数，用于缩放目标仓位，默认为1.0
	 * 
	 * 将缓存的目标仓位统一提交给各个执行器。
	 * 会对目标仓位应用风控系数和过滤器。
	 * 提交完成后会清空缓存。
	 */
	void	commit_cached_targets(double scale = 1.0);

private:
	typedef wt_hashmap<std::string, ExecCmdPtr> ExecuterMap;  // 执行器映射表类型定义：键为执行器名称，值为执行器智能指针
	ExecuterMap		_executers;  // 执行器映射表，存储所有执行器实例
	WtFilterMgr*	_filter_mgr;  // 过滤器管理器指针，用于对仓位信号进行过滤

	typedef wt_hashmap<std::string, double> TargetsMap;  // 目标仓位映射表类型定义：键为合约代码，值为目标仓位
	wt_hashmap<std::string, TargetsMap>	_all_cached_targets;  // 所有缓存的目标仓位映射表：键为执行器ID，值为目标仓位映射表

	typedef wt_hashset<std::string>	ExecuterSet;  // 执行器ID集合类型定义
	wt_hashmap<std::string, ExecuterSet>	_router_rules;  // 路由规则映射表：键为策略ID，值为执行器ID集合

	wt_hashset<std::string>	_routed_executers;  // 已路由的执行器ID集合，用于快速判断执行器是否配置了路由规则
};
NS_WTP_END  // 结束WonderTrader命名空间
