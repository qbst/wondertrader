/*!
 * \file WtDistExecuter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 分布式执行器头文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件定义了WtDistExecuter类，用于实现分布式执行策略。
 * 
 * 核心功能：
 * 1. 目标仓位管理：维护各合约的目标仓位，支持仓位放大
 * 2. 仓位变更通知：接收仓位变动通知，更新目标仓位
 * 3. 分布式执行：实际执行逻辑由外部分布式系统完成，本类只负责管理目标仓位
 * 
 * 设计特点：
 * - 继承自IExecCommand，实现执行命令接口
 * - 轻量级设计，不包含实际的交易执行逻辑
 * - 支持仓位放大，通过scale参数调整目标仓位
 * - 适合在分布式系统中使用，将仓位管理职责分离
 * 
 * 与差量执行器的区别：
 * - 分布式执行器不包含执行单元，不执行实际交易
 * - 只负责维护目标仓位，实际执行由外部系统完成
 * - 不处理行情数据，不接收交易回报
 */
#pragma once  // 防止头文件重复包含
#include "IExecCommand.h"  // 包含执行命令接口头文件

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：变体类型，用于配置参数传递

/**
 * @class WtDistExecuter
 * @brief 分布式执行器类
 * 
 * 该类实现了分布式执行策略，只负责管理目标仓位，不执行实际交易。
 * 继承自IExecCommand，实现执行命令接口。
 * 
 * 主要职责：
 * 1. 管理目标仓位：维护各合约的目标仓位映射表
 * 2. 仓位变更通知：接收仓位变动通知，更新目标仓位
 * 3. 仓位放大：支持通过scale参数放大目标仓位
 * 
 * 工作流程：
 * 1. 初始化：加载配置参数，设置放大倍数
 * 2. 设置目标仓位：接收目标仓位映射表，应用放大倍数，更新内部状态
 * 3. 仓位变动：接收仓位变动通知，应用放大倍数，更新目标仓位
 * 4. 分布式执行：目标仓位信息由外部系统读取并执行
 */
class WtDistExecuter : public IExecCommand
{
public:
	/**
	 * @brief 构造函数
	 * @param name 执行器名称字符串
	 * 
	 * 初始化分布式执行器，设置执行器名称。
	 */
	WtDistExecuter(const char* name);
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源。
	 */
	virtual ~WtDistExecuter();

public:
	/**
	 * @brief 初始化执行器
	 * @param params 初始化参数，包含放大倍数等配置
	 * @return bool 初始化成功返回true，否则返回false
	 * 
	 * 解析初始化参数，设置放大倍数。
	 * 如果参数无效，返回false。
	 */
	bool init(WTSVariant* params);


public:
	//////////////////////////////////////////////////////////////////////////
	// IExecCommand接口实现
	// 以下方法实现IExecCommand接口
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 设置目标仓位
	 * @param targets 目标仓位映射表，键为合约代码，值为目标持仓数量
	 * 
	 * 接收目标仓位映射表，应用放大倍数，更新内部目标仓位映射。
	 * 如果目标仓位发生变化，记录日志。
	 * 
	 * 实现逻辑：
	 * 1. 遍历目标仓位映射表
	 * 2. 对每个目标仓位应用放大倍数
	 * 3. 更新内部目标仓位映射
	 * 4. 如果仓位发生变化，记录日志
	 * 5. 注意：这里只更新目标仓位，实际执行由外部系统完成
	 */
	virtual void set_position(const wt_hashmap<std::string, double>& targets) override;


	/**
	 * @brief 合约仓位变动通知
	 * @param stdCode 标准合约代码字符串
	 * @param targetPos 目标仓位数量
	 * 
	 * 当合约仓位发生变化时被调用，更新目标仓位。
	 * 应用放大倍数后更新内部目标仓位映射。
	 * 如果目标仓位发生变化，记录日志。
	 * 
	 * 实现逻辑：
	 * 1. 对目标仓位应用放大倍数
	 * 2. 更新内部目标仓位映射
	 * 3. 如果仓位发生变化，记录日志
	 * 4. 注意：这里只更新目标仓位，实际执行由外部系统完成
	 */
	virtual void on_position_changed(const char* stdCode, double targetPos) override;


	/**
	 * @brief 实时行情回调
	 * @param stdCode 标准合约代码字符串
	 * @param newTick 新的Tick数据指针
	 * 
	 * 当收到实时行情数据时被调用。
	 * 分布式执行器不需要处理行情数据，此方法为空实现。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;

private:
	WTSVariant*			_config;  // 配置参数指针，包含放大倍数等配置

	uint32_t			_scale;  // 放大倍数，用于调整仓位数量（例如：1表示不放大，2表示放大2倍）

	wt_hashmap<std::string, double> _target_pos;  // 目标仓位映射表，键为合约代码，值为目标持仓数量
};
NS_WTP_END  // 结束WonderTrader命名空间

