/*!
 * \file ExpExecuter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展Executer类定义文件
 * 
 * 本文件定义了ExpExecuter类，用于实现扩展的执行器（Executer）。
 * ExpExecuter继承自IExecCommand接口，是外部语言（如Python）实现执行器的桥梁。
 * 
 * 设计逻辑：
 * - ExpExecuter作为适配器，将执行器的操作（初始化、设置持仓等）转发给外部语言实现的回调函数
 * - 通过WtRtRunner的回调机制，将执行器命令通知给外部语言
 * - 外部语言通过回调函数接收持仓调整命令，并执行实际的交易操作
 */
#pragma once
#include "../WtCore/IExecCommand.h"  // 执行器命令接口

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 扩展Executer类
 * 
 * 扩展的执行器类，用于外部语言实现的执行器与WonderTrader框架的桥接
 */
class ExpExecuter : public IExecCommand
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param name 执行器名称
	 */
	ExpExecuter(const char* name):IExecCommand(name){}

	/**
	 * @brief 初始化执行器
	 * 
	 * 初始化执行器，调用外部语言实现的初始化回调
	 */
	void	init();

	/**
	 * @brief 设置目标持仓
	 * 
	 * 设置多个合约的目标持仓，调用外部语言实现的命令回调
	 * 
	 * @param targets 目标持仓映射表（合约代码->目标持仓数量）
	 */
	virtual void set_position(const wt_hashmap<std::string, double>& targets) override;

	/**
	 * @brief 持仓变化事件
	 * 
	 * 当策略持仓发生变化时调用，通知外部语言调整持仓
	 * 
	 * @param stdCode 标准合约代码
	 * @param targetPos 目标持仓数量
	 */
	virtual void on_position_changed(const char* stdCode, double targetPos) override;

};

