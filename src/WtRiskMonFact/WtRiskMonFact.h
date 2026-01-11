/*!
 * \file WtRiskMonFact.h
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader风控模块工厂类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的风控模块工厂类WtRiskMonFact，实现了IRiskMonitorFact接口。
 * 该工厂类采用工厂模式设计，负责创建、管理和销毁各种风控监控器实例。
 * 
 * 主要功能：
 * 1. 工厂管理：提供风控模块的创建和销毁功能，实现统一的风控模块生命周期管理
 * 2. 模块枚举：支持枚举当前工厂可创建的所有风控模块类型，便于系统发现可用模块
 * 3. 动态加载：通过工厂模式实现风控模块的延迟加载和按需创建，提高系统灵活性
 * 4. 接口实现：完整实现IRiskMonitorFact接口，作为WonderTrader风控系统的核心组件
 * 
 * 设计特点：
 * - 工厂模式：采用经典工厂模式，将对象的创建逻辑封装在工厂类中
 * - 接口隔离：通过IRiskMonitorFact接口实现，保证与框架其他部分的解耦
 * - 扩展性：支持通过工厂注册新的风控模块类型，便于系统扩展
 * - 生命周期管理：统一管理风控模块的创建和销毁，避免内存泄漏
 */

#pragma once  // 防止头文件被重复包含

#include "../Includes/RiskMonDefs.h"  // 包含风控模块定义头文件，提供IRiskMonitorFact接口和WtRiskMonitor基类

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WonderTrader风控模块工厂类
 * 
 * 该类实现了IRiskMonitorFact接口，是WonderTrader框架中负责创建和管理风控监控器的工厂类。
 * 通过工厂模式，实现了风控模块的创建、枚举和销毁的统一管理。
 * 
 * 功能说明：
 * - 创建风控模块：根据模块名称创建对应的风控监控器实例
 * - 枚举模块：提供枚举功能，列出所有可用的风控模块类型
 * - 销毁模块：安全地销毁风控模块实例，确保资源正确释放
 * 
 * 使用场景：
 * - 系统启动时，通过工厂创建所需的风控监控器
 * - 动态加载风控模块时，通过工厂接口进行模块管理
 * - 系统关闭时，通过工厂统一销毁所有风控模块实例
 */
class WtRiskMonFact : public IRiskMonitorFact
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化风控模块工厂对象，准备创建和管理风控模块。
	 */
	WtRiskMonFact();
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 清理工厂对象资源，确保所有资源正确释放。
	 */
	virtual ~WtRiskMonFact();

public:
	/**
	 * @brief 获取工厂名称
	 * 
	 * 返回当前工厂的名称标识，用于区分不同的风控模块工厂。
	 * 
	 * @return 工厂名称字符串指针，返回"WtRiskMonFact"
	 */
	virtual const char* getName() override;
	
	/**
	 * @brief 枚举所有可用的风控模块
	 * 
	 * 遍历当前工厂支持的所有风控模块类型，通过回调函数逐一通知调用者。
	 * 调用者可以通过此方法了解工厂支持哪些风控模块。
	 * 
	 * @param cb 枚举回调函数，函数签名：void(*FuncEnumRiskMonCallback)(const char* factName, const char* unitName, bool isLast)
	 *           - factName: 工厂名称
	 *           - unitName: 风控模块名称
	 *           - isLast: 是否为最后一个模块
	 */
	virtual void enumRiskMonitors(FuncEnumRiskMonCallback cb) override;

	/**
	 * @brief 根据名称创建风控监控器实例
	 * 
	 * 根据传入的风控模块名称，创建对应的风控监控器对象。
	 * 如果名称不匹配任何已知模块，返回NULL。
	 * 
	 * @param name 风控模块名称，如"SimpleRiskMon"
	 * @return 成功返回风控监控器对象指针，失败返回NULL
	 * 
	 * 支持的模块：
	 * - "SimpleRiskMon": 简单风控监控器，实现基础的日内和多日回撤风控
	 */
	virtual WtRiskMonitor* createRiskMonotor(const char* name) override;

	/**
	 * @brief 删除风控监控器实例
	 * 
	 * 安全地销毁传入的风控监控器对象，释放其占用的资源。
	 * 在删除前会检查对象是否属于当前工厂创建，确保安全删除。
	 * 
	 * @param unit 要删除的风控监控器对象指针
	 * @return 删除成功返回true，失败返回false
	 * 
	 * 删除条件：
	 * - unit为NULL时，直接返回true（视为成功）
	 * - unit的工厂名称与当前工厂名称匹配时，执行删除
	 * - 工厂名称不匹配时，返回false（防止误删其他工厂创建的对象）
	 */
	virtual bool deleteRiskMonotor(WtRiskMonitor* unit) override;

};

