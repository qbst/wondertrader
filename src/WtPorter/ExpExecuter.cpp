/*!
 * \file ExpExecuter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展Executer类实现文件
 * 
 * 本文件实现了ExpExecuter类的所有方法，通过WtRtRunner将执行器操作转发给外部语言实现的回调函数。
 * 外部语言通过注册的回调函数接收执行器命令，并执行实际的交易操作。
 */
#include "ExpExecuter.h"  // ExpExecuter类定义
#include "WtRtRunner.h"  // 运行时运行器

extern WtRtRunner& getRunner();  // 获取WtRtRunner单例对象

/**
 * @brief 初始化执行器
 * 
 * 调用WtRtRunner的executer_init方法，触发外部语言实现的初始化回调
 */
void ExpExecuter::init()
{
	getRunner().executer_init(name());  // 通知外部语言执行器初始化事件
}

/**
 * @brief 设置目标持仓
 * 
 * 遍历目标持仓映射表，对每个合约调用WtRtRunner的executer_set_position方法，
 * 触发外部语言实现的命令回调
 * 
 * @param targets 目标持仓映射表（合约代码->目标持仓数量）
 */
void ExpExecuter::set_position(const wt_hashmap<std::string, double>& targets)
{
	for(auto& v : targets)  // 遍历所有目标持仓
	{
		getRunner().executer_set_position(name(), v.first.c_str(), v.second);  // 通知外部语言调整该合约的持仓
	}
}

/**
 * @brief 持仓变化事件处理
 * 
 * 当策略持仓发生变化时调用，通知外部语言调整持仓
 * 
 * @param stdCode 标准合约代码
 * @param targetPos 目标持仓数量
 */
void ExpExecuter::on_position_changed(const char* stdCode, double targetPos)
{
	getRunner().executer_set_position(name(), stdCode, targetPos);  // 通知外部语言调整持仓
}
