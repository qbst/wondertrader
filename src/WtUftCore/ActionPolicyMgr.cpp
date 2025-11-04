/*!
 * \file ActionPolicyMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 动作策略管理器实现文件
 *
 * 本文件实现了ActionPolicyMgr类的所有功能，包括：
 * 1. 构造函数和析构函数：初始化动作策略管理器，清理资源
 * 2. 配置文件加载：从JSON配置文件加载动作策略规则
 * 3. 规则解析：解析规则组、动作类型、手数限制、品种映射等
 * 4. 规则查询：根据合约品种代码查找对应的动作规则组
 */
#include "ActionPolicyMgr.h"  // 动作策略管理器头文件

#include "../Share/StdUtils.hpp"  // 标准工具函数
#include "../WTSTools/WTSLogger.h"  // 日志工具

#include "../Includes/WTSVariant.hpp"  // 变体配置类
#include "../WTSUtils/WTSCfgLoader.h"  // 配置加载器

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 构造函数
 * 
 * 创建动作策略管理器实例，初始化成员变量。
 */
ActionPolicyMgr::ActionPolicyMgr()
{
}


/**
 * @brief 析构函数
 * 
 * 清理动作策略管理器占用的资源。
 */
ActionPolicyMgr::~ActionPolicyMgr()
{
}

/**
 * @brief 初始化动作策略管理器
 * @param filename 配置文件路径
 * @return 初始化成功返回true，失败返回false
 * 
 * 从指定的JSON配置文件加载动作策略规则。
 * 配置文件格式示例：
 * {
 *   "规则组名称": {
 *     "order": [
 *       {
 *         "action": "open|close|closetoday|closeyestoday",
 *         "limit": 总手数限制,
 *         "limit_l": 多头手数限制,
 *         "limit_s": 空头手数限制,
 *         "pure": 是否纯仓
 *       }
 *     ],
 *     "filters": ["合约品种1", "合约品种2"]
 *   }
 * }
 */
bool ActionPolicyMgr::init(const char* filename)
{
	WTSVariant* cfg = WTSCfgLoader::load_from_file(filename);  // 从文件加载配置
	if (cfg == NULL)  // 如果加载失败
		return false;  // 返回false

	auto keys = cfg->memberNames();  // 获取配置中的所有键（规则组名称）
	// 遍历所有规则组
	for (auto it = keys.begin(); it != keys.end(); it++)
	{
		const char* gpName = (*it).c_str();  // 获取规则组名称
		WTSVariant*	vGpItem = cfg->get(gpName);  // 获取规则组配置项
		ActionRuleGroup& gp = _rules[gpName];  // 获取或创建规则组

		WTSVariant* vOrds = vGpItem->get("order");  // 获取订单规则数组
		if(vOrds != NULL && vOrds->isArray())  // 如果订单规则数组存在且为数组类型
		{
			// 遍历订单规则数组
			for (uint32_t i = 0; i < vOrds->size(); i++)
			{
				WTSVariant* vObj = vOrds->get(i);  // 获取单个规则对象
				ActionRule aRule;  // 创建动作规则
				const char* action = vObj->getCString("action");  // 获取动作类型字符串
				uint32_t uLimit = vObj->getUInt32("limit");  // 获取总手数限制
				uint32_t uLimitS = vObj->getUInt32("limit_s");  // 获取空头手数限制
				uint32_t uLimitL = vObj->getUInt32("limit_l");  // 获取多头手数限制
				// 解析动作类型
				if (wt_stricmp(action, "open") == 0)  // 如果是开仓
					aRule._atype = AT_Open;
				else if (wt_stricmp(action, "close") == 0)  // 如果是平仓
					aRule._atype = AT_Close;
				else if (wt_stricmp(action, "closetoday") == 0)  // 如果是平今
					aRule._atype = AT_CloseToday;
				else if (wt_stricmp(action, "closeyestoday") == 0)  // 如果是平昨
					aRule._atype = AT_CloseYestoday;
				else  // 如果动作类型无法识别
				{
					WTSLogger::error("Loading action policy failed: unrecognized type {}", action);  // 记录错误日志
					continue;  // 跳过该规则
				}

				aRule._limit = uLimit;  // 设置总手数限制
				aRule._limit_s = uLimitS;  // 设置空头手数限制
				aRule._limit_l = uLimitL;  // 设置多头手数限制
				aRule._pure = vObj->getBoolean("pure");  // 设置是否纯仓标志
				gp.emplace_back(aRule);  // 将规则添加到规则组
			}
		}

		WTSVariant* filters = vGpItem->get("filters");  // 获取品种过滤器数组
		if(filters!=NULL && filters->isArray() && filters->size()>0)  // 如果过滤器数组存在且不为空
		{
			// 遍历过滤器数组，建立品种到规则组的映射
			for (uint32_t i = 0; i < filters->size(); i++)
			{
				const char* commid = filters->get(i)->asCString();  // 获取合约品种代码
				_comm_rule_map[commid] = gpName;  // 建立品种到规则组的映射
			}
		}
	}

	cfg->release();  // 释放配置对象
	return true;  // 返回成功
}

/**
 * @brief 获取动作规则组
 * @param pid 合约品种代码
 * @return 对应的动作规则组引用，如果找不到则返回默认规则组
 * 
 * 根据合约品种代码查找对应的动作规则组。
 * 查找流程：
 * 1. 首先在品种规则映射表中查找该品种对应的规则组名称
 * 2. 如果找到规则组名称，则在规则表中查找该规则组
 * 3. 如果找不到规则组，则使用默认规则组
 * 4. 如果默认规则组也不存在，则记录错误日志并返回默认规则组
 */
const ActionRuleGroup& ActionPolicyMgr::getActionRules(const char* pid)
{
	std::string gpName = "default";  // 默认规则组名称

	{//先找到品种对应的规则组名称
		auto it = _comm_rule_map.find(pid);  // 查找品种规则映射表
		if (it != _comm_rule_map.end())  // 如果找到
			gpName = it->second;  // 获取规则组名称
	}

	{
		auto it = _rules.find(gpName);  // 查找规则组
		if (it == _rules.end())  // 如果找不到规则组
		{
			it = _rules.find("default");  // 查找默认规则组
			WTSLogger::error("Action policy group {} not exists, changed to default group", gpName.c_str());  // 记录错误日志
		}

		assert(it != _rules.end());  // 断言规则组必须存在（至少默认规则组应该存在）
		return it->second;  // 返回规则组引用
	}
}
