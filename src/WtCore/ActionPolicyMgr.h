/*!
 * \file ActionPolicyMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 动作策略管理器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的动作策略管理器，用于管理和配置交易动作的执行规则。
 * 主要功能包括：
 * 1. 定义交易动作类型：开仓、平仓、平今、平昨等
 * 2. 定义动作规则：包括动作类型、手数限制（总体、多头、空头）、净仓判断等
 * 3. 管理不同品种的动作规则配置，支持按品种分组管理
 * 4. 从配置文件加载动作规则，提供规则查询接口
 * 
 * 该类主要用于WonderTrader框架中的交易执行管理，通过预定义的规则来控制不同品种的交易行为，
 * 确保交易动作符合交易所规则和风控要求。支持通过配置文件灵活配置不同品种的交易规则。
 */
#pragma once
#include <vector>        // 包含vector容器支持
#include <stdint.h>      // 包含固定大小整数类型定义
#include <string.h>      // 包含字符串处理函数

#include "../Includes/FasterDefs.h"  // 包含WonderTrader快速定义头文件


NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：WTS变体类型类，用于配置数据解析

/**
 * @enum ActionType
 * @brief 交易动作类型枚举
 * 
 * 定义交易中可能执行的动作类型，包括开仓、平仓、平今、平昨等操作。
 * 用于标识交易指令的具体动作类型。
 */
typedef enum tagActionType
{
	AT_Unknown = 8888,    // 未知动作类型，默认值
	AT_Open = 9999,		// 开仓动作：买入或卖出建立新仓位
	AT_Close,			// 平仓动作：关闭现有仓位（不区分今昨）
	AT_CloseToday,		// 平今动作：只平今日开仓的仓位
	AT_CloseYestoday	// 平昨动作：只平昨日及之前开仓的仓位
} ActionType;

/**
 * @struct ActionRule
 * @brief 动作规则结构体
 * 
 * 定义单个交易动作的执行规则，包括动作类型、手数限制等约束条件。
 * 用于控制交易动作的执行范围和行为。
 */
typedef struct _ActionRule
{
	ActionType	_atype;		// 动作类型：标识执行的具体动作（开仓、平仓等）
	uint32_t	_limit;		// 手数限制：总体手数限制，0表示无限制
	uint32_t	_limit_l;	// 多头手数限制：限制多头方向的最大手数，0表示无限制
	uint32_t	_limit_s;	// 空头手数限制：限制空头方向的最大手数，0表示无限制
	bool		_pure;		// 净仓标志：主要针对AT_CloseToday和AT_CloseYestoday，true表示必须是净今仓或净昨仓才能执行

	/**
	 * @brief 构造函数
	 * 
	 * 初始化动作规则结构体，将所有成员变量清零。
	 */
	_ActionRule()
	{
		memset(this, 0, sizeof(_ActionRule));  // 将整个结构体内存清零
	}
} ActionRule;

/**
 * @typedef ActionRuleGroup
 * @brief 动作规则组类型定义
 * 
 * 使用vector容器存储多个动作规则，组成一个规则组。
 * 一个规则组可以包含多个不同动作类型的规则。
 */
typedef std::vector<ActionRule>	ActionRuleGroup;

/**
 * @class ActionPolicyMgr
 * @brief 动作策略管理器类
 * 
 * 管理交易动作的执行规则，支持从配置文件加载规则，并提供规则查询接口。
 * 通过品种ID映射到对应的规则组，实现不同品种的差异化规则管理。
 * 
 * 主要功能：
 * - 从配置文件加载动作规则
 * - 根据品种ID查询对应的动作规则组
 * - 支持品种到规则组的映射配置
 */
class ActionPolicyMgr
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化动作策略管理器对象。
	 */
	ActionPolicyMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理动作策略管理器对象，释放相关资源。
	 */
	~ActionPolicyMgr();

public:
	/**
	 * @brief 初始化动作策略管理器
	 * @param filename 配置文件路径，JSON格式的配置文件
	 * @return bool 返回true表示初始化成功，false表示失败
	 * 
	 * 从指定的配置文件中加载动作规则。
	 * 配置文件格式为JSON，包含规则组定义和品种到规则组的映射关系。
	 */
	bool init(const char* filename);

	/**
	 * @brief 获取指定品种的动作规则组
	 * @param pid 品种ID，用于查找对应的规则组
	 * @return const ActionRuleGroup& 返回对应的动作规则组的常量引用
	 * 
	 * 根据品种ID查找对应的动作规则组。
	 * 如果品种没有映射到特定规则组，则返回默认规则组。
	 * 如果默认规则组也不存在，会记录错误日志并返回空规则组。
	 */
	const ActionRuleGroup& getActionRules(const char* pid);

private:
	/**
	 * @typedef RulesMap
	 * @brief 规则映射表类型定义
	 * 
	 * 使用哈希表存储规则组名称到规则组的映射关系。
	 * key为规则组名称（字符串），value为对应的动作规则组。
	 */
	typedef wt_hashmap<std::string, ActionRuleGroup> RulesMap;
	RulesMap	_rules;	// 规则表：存储所有已加载的规则组，key为规则组名称

	/**
	 * @brief 品种规则映射表
	 * 
	 * 存储品种ID到规则组名称的映射关系。
	 * key为品种ID（字符串），value为对应的规则组名称（字符串）。
	 * 用于快速查找某个品种应该使用哪个规则组。
	 */
	wt_hashmap<std::string, std::string> _comm_rule_map;	// 品种规则映射：品种ID -> 规则组名称
};

NS_WTP_END  // 结束WonderTrader命名空间
