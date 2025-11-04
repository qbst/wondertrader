/*!
 * \file ActionPolicyMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 动作策略管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了动作策略管理器的核心功能，包括从配置文件加载动作规则和查询规则组。
 * 主要实现逻辑：
 * 1. 从JSON配置文件解析动作规则，支持多个规则组配置
 * 2. 解析每个规则组中的订单规则（order）和品种过滤器（filters）
 * 3. 建立品种ID到规则组的映射关系，实现快速查找
 * 4. 提供根据品种ID查询规则组的接口，支持默认规则组回退机制
 * 
 * 配置文件格式说明：
 * {
 *   "规则组名称": {
 *     "order": [
 *       {
 *         "action": "open|close|closetoday|closeyestoday",
 *         "limit": 手数限制,
 *         "limit_s": 空头手数限制,
 *         "limit_l": 多头手数限制,
 *         "pure": 净仓标志
 *       }
 *     ],
 *     "filters": ["品种ID1", "品种ID2", ...]
 *   }
 * }
 */
#include "ActionPolicyMgr.h"  // 包含动作策略管理器头文件

#include "../Share/StdUtils.hpp"      // 包含标准工具函数
#include "../WTSTools/WTSLogger.h"   // 包含日志记录工具

#include "../Includes/WTSVariant.hpp"    // 包含WTS变体类型定义
#include "../WTSUtils/WTSCfgLoader.h"   // 包含配置文件加载器

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 构造函数实现
 * 
 * 初始化动作策略管理器对象，此时所有成员变量使用默认值。
 */
ActionPolicyMgr::ActionPolicyMgr()
{
}


/**
 * @brief 析构函数实现
 * 
 * 清理动作策略管理器对象，由于使用的是标准容器，会自动释放资源。
 */
ActionPolicyMgr::~ActionPolicyMgr()
{
}

/**
 * @brief 初始化动作策略管理器
 * @param filename 配置文件路径，JSON格式
 * @return bool 返回true表示初始化成功，false表示失败
 * 
 * 从指定的JSON配置文件中加载动作规则。
 * 配置文件包含多个规则组，每个规则组可以定义订单规则和品种过滤器。
 */
bool ActionPolicyMgr::init(const char* filename)
{
	// 从文件加载配置，返回WTSVariant对象
	WTSVariant* cfg = WTSCfgLoader::load_from_file(filename);
	if (cfg == NULL)  // 如果加载失败，返回false
		return false;

	// 获取配置文件中所有顶层成员的名称（规则组名称）
	auto keys = cfg->memberNames();
	// 遍历所有规则组
	for (auto it = keys.begin(); it != keys.end(); it++)
	{
		const char* gpName = (*it).c_str();           // 获取规则组名称
		WTSVariant*	vGpItem = cfg->get(gpName);       // 获取规则组配置对象
		ActionRuleGroup& gp = _rules[gpName];         // 创建或获取对应的规则组引用

		// 解析订单规则数组
		WTSVariant* vOrds = vGpItem->get("order");
		if(vOrds != NULL && vOrds->isArray())  // 检查order字段是否存在且为数组类型
		{
			// 遍历订单规则数组中的每个规则项
			for (uint32_t i = 0; i < vOrds->size(); i++)
			{
				WTSVariant* vObj = vOrds->get(i);           // 获取第i个规则对象
				ActionRule aRule;                           // 创建动作规则对象
				const char* action = vObj->getCString("action");  // 获取动作类型字符串
				uint32_t uLimit = vObj->getUInt32("limit");       // 获取总体手数限制
				uint32_t uLimitS = vObj->getUInt32("limit_s");    // 获取空头手数限制
				uint32_t uLimitL = vObj->getUInt32("limit_l");    // 获取多头手数限制
				
				// 根据动作类型字符串设置枚举值（不区分大小写比较）
				if (wt_stricmp(action, "open") == 0)
					aRule._atype = AT_Open;              // 设置为开仓动作
				else if (wt_stricmp(action, "close") == 0)
					aRule._atype = AT_Close;             // 设置为平仓动作
				else if (wt_stricmp(action, "closetoday") == 0)
					aRule._atype = AT_CloseToday;        // 设置为平今动作
				else if (wt_stricmp(action, "closeyestoday") == 0)
					aRule._atype = AT_CloseYestoday;     // 设置为平昨动作
				else 
				{
					// 如果动作类型无法识别，记录错误日志并跳过该规则
					WTSLogger::error("Loading action policy failed: unrecognized type {}", action);
					continue;
				}

				// 设置规则的其他属性
				aRule._limit = uLimit;                   // 设置总体手数限制
				aRule._limit_s = uLimitS;                // 设置空头手数限制
				aRule._limit_l = uLimitL;                // 设置多头手数限制
				aRule._pure = vObj->getBoolean("pure");  // 设置净仓标志
				gp.emplace_back(aRule);                  // 将规则添加到规则组中
			}
		}

		// 解析品种过滤器数组，建立品种到规则组的映射
		WTSVariant* filters = vGpItem->get("filters");
		if(filters!=NULL && filters->isArray() && filters->size()>0)  // 检查filters字段是否存在且为非空数组
		{
			// 遍历过滤器数组中的每个品种ID
			for (uint32_t i = 0; i < filters->size(); i++)
			{
				const char* commid = filters->get(i)->asCString();  // 获取品种ID字符串
				_comm_rule_map[commid] = gpName;                    // 建立品种ID到规则组名称的映射
			}
		}
	}

	cfg->release();  // 释放配置对象资源
	return true;     // 返回成功标志
}

/**
 * @brief 获取指定品种的动作规则组
 * @param pid 品种ID，用于查找对应的规则组
 * @return const ActionRuleGroup& 返回对应的动作规则组的常量引用
 * 
 * 查询逻辑：
 * 1. 首先在品种规则映射表中查找该品种对应的规则组名称
 * 2. 如果找到，使用该规则组名称；否则使用"default"作为默认规则组名称
 * 3. 在规则表中查找对应的规则组
 * 4. 如果找不到指定的规则组，尝试查找"default"规则组并记录错误日志
 * 5. 返回找到的规则组引用（如果都不存在会触发断言）
 */
const ActionRuleGroup& ActionPolicyMgr::getActionRules(const char* pid)
{
	std::string gpName = "default";  // 默认规则组名称

	{//先找到品种对应的规则组名称
		auto it = _comm_rule_map.find(pid);  // 在品种规则映射表中查找品种ID
		if (it != _comm_rule_map.end())      // 如果找到映射关系
			gpName = it->second;             // 使用映射到的规则组名称
	}

	{
		// 在规则表中查找对应的规则组
		auto it = _rules.find(gpName);
		if (it == _rules.end())  // 如果找不到指定的规则组
		{
			it = _rules.find("default");  // 尝试查找默认规则组
			WTSLogger::error("Action policy group {} not exists, changed to default group", gpName.c_str());  // 记录错误日志
		}

		assert(it != _rules.end());  // 断言：规则组必须存在（至少应该有default组）
		return it->second;           // 返回找到的规则组引用
	}
}
