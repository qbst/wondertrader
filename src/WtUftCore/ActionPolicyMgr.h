/*!
 * \file ActionPolicyMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 动作策略管理器头文件
 *
 * 本文件定义了ActionPolicyMgr类，用于管理UFT策略的交易动作策略规则。
 *
 * 设计逻辑：
 * 1. 策略规则管理：通过配置文件加载交易动作策略规则，支持按品种设置不同的规则组
 * 2. 动作类型支持：支持开仓、平仓、平今、平昨等不同交易动作类型
 * 3. 手数限制：支持总手数限制、多头手数限制、空头手数限制等多种限制方式
 * 4. 规则组映射：支持将合约品种映射到不同的规则组，实现差异化策略管理
 * 5. 净仓判断：支持判断是否为净今仓或净昨仓的纯仓规则
 *
 * 主要功能：
 * - 从配置文件加载动作策略规则
 * - 根据合约品种获取对应的动作规则组
 * - 提供规则查询接口，供策略执行时使用
 */
#pragma once
#include <vector>  // 标准向量容器
#include <stdint.h>  // 标准整数类型定义
#include <string.h>  // 字符串操作函数

#include "../Includes/FasterDefs.h"  // 快速定义头文件


NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSVariant;  // 前向声明：变体配置类

/**
 * @enum ActionType
 * @brief 交易动作类型枚举
 * 
 * 定义交易动作的类型，包括开仓、平仓、平今、平昨等。
 */
typedef enum tagActionType
{
	AT_Unknown = 8888,      // 未知动作类型
	AT_Open = 9999,		// 开仓动作
	AT_Close,			// 平仓动作
	AT_CloseToday,		// 平今动作
	AT_CloseYestoday	// 平昨动作
} ActionType;

/**
 * @struct ActionRule
 * @brief 动作规则结构体
 * 
 * 定义单个动作规则的详细信息，包括动作类型、手数限制等。
 */
typedef struct _ActionRule
{
	ActionType	_atype;		// 动作类型（开仓、平仓、平今、平昨）
	uint32_t	_limit;		// 总手数限制（多头+空头）
	uint32_t	_limit_l;	// 多头手数限制
	uint32_t	_limit_s;	// 空头手数限制
	bool		_pure;		// 是否纯仓标志，主要针对AT_CloseToday和AT_CloseYestoday，用于判断是否是净今仓或者净昨仓（true表示净仓，false表示允许双向持仓）

	/**
	 * @brief 构造函数
	 * 
	 * 初始化动作规则结构体，将所有成员变量清零。
	 */
	_ActionRule()
	{
		memset(this, 0, sizeof(_ActionRule));  // 将结构体内存清零
	}
} ActionRule;

/**
 * @typedef ActionRuleGroup
 * @brief 动作规则组类型定义
 * 
 * 动作规则组是一个动作规则向量，用于存储某个规则组的所有规则。
 */
typedef std::vector<ActionRule>	ActionRuleGroup;

/**
 * @class ActionPolicyMgr
 * @brief 动作策略管理器类
 * 
 * 管理UFT策略的交易动作策略规则，支持从配置文件加载规则，并根据合约品种查询对应的规则组。
 * 
 * 核心功能：
 * - 从配置文件加载动作策略规则
 * - 建立合约品种到规则组的映射关系
 * - 根据合约品种代码获取对应的动作规则组
 */
class ActionPolicyMgr
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建动作策略管理器实例。
	 */
	ActionPolicyMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理动作策略管理器占用的资源。
	 */
	~ActionPolicyMgr();

public:
	/**
	 * @brief 初始化动作策略管理器
	 * @param filename 配置文件路径
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从指定的配置文件加载动作策略规则，解析规则组和合约品种映射关系。
	 */
	bool init(const char* filename);

	/**
	 * @brief 获取动作规则组
	 * @param pid 合约品种代码
	 * @return 对应的动作规则组引用，如果找不到则返回默认规则组
	 * 
	 * 根据合约品种代码查找对应的动作规则组。
	 * 如果该品种没有配置规则组，则返回默认规则组。
	 */
	const ActionRuleGroup& getActionRules(const char* pid);

private:
	/**
	 * @typedef RulesMap
	 * @brief 规则映射表类型定义
	 * 
	 * 键为规则组名称，值为动作规则组。
	 */
	typedef wt_hashmap<std::string, ActionRuleGroup> RulesMap;
	RulesMap	_rules;	// 规则表，存储所有规则组及其规则列表

	wt_hashmap<std::string, std::string> _comm_rule_map;	// 品种规则映射表，键为合约品种代码，值为规则组名称
};

NS_WTP_END  // WonderTrader命名空间结束
