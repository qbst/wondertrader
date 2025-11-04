/*!
 * \file WtDistExecuter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 分布式执行器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件实现了WtDistExecuter类的所有方法，提供分布式执行功能。
 * 
 * 实现要点：
 * 1. 构造函数和析构函数：初始化成员变量，清理资源
 * 2. 初始化：加载配置参数，设置放大倍数
 * 3. 目标仓位管理：设置目标仓位，处理仓位变动通知
 * 4. 仓位放大：对目标仓位应用放大倍数
 * 
 * 注意事项：
 * - 分布式执行器不执行实际交易，只管理目标仓位
 * - 实际执行由外部分布式系统完成
 * - 不处理行情数据，不接收交易回报
 */
#include "WtDistExecuter.h"  // 包含分布式执行器头文件

#include "../Includes/WTSVariant.hpp"  // 包含变体类型头文件

#include "../Share/decimal.h"  // 包含小数精度工具头文件
#include "../WTSTools/WTSLogger.h"  // 包含日志工具头文件

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 构造函数
 * @param name 执行器名称字符串
 * 
 * 初始化分布式执行器，设置执行器名称。
 */
WtDistExecuter::WtDistExecuter(const char* name)
	: IExecCommand(name)  // 调用父类构造函数，设置执行器名称
{

}

/**
 * @brief 析构函数
 * 
 * 清理资源。
 */
WtDistExecuter::~WtDistExecuter()
{

}

/**
 * @brief 初始化执行器
 * @param params 初始化参数，包含放大倍数等配置
 * @return bool 初始化成功返回true，否则返回false
 * 
 * 解析初始化参数，设置放大倍数。
 * 如果参数无效，返回false。
 */
bool WtDistExecuter::init(WTSVariant* params)
{
	if (params == NULL)  // 如果参数为NULL
		return false;  // 返回false

	_config = params;  // 保存配置参数指针
	_config->retain();  // 增加配置参数的引用计数（防止被释放）

	_scale = params->getUInt32("scale");  // 从配置参数中获取放大倍数（键名为"scale"）

	return true;  // 初始化成功，返回true
}

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
void WtDistExecuter::set_position(const wt_hashmap<std::string, double>& targets)
{
	for (auto it = targets.begin(); it != targets.end(); it++)  // 遍历目标仓位映射表
	{
		const char* stdCode = it->first.c_str();  // 获取合约代码字符串
		double newVol = it->second;  // 获取目标仓位数量

		newVol *= _scale;  // 应用放大倍数（目标仓位乘以放大倍数）
		double oldVol = _target_pos[stdCode];  // 获取旧的目标仓位（如果不存在则为0）
		_target_pos[stdCode] = newVol;  // 更新目标仓位映射表
		if (!decimal::eq(oldVol, newVol))  // 如果目标仓位发生变化（使用小数精度比较）
		{
			WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}]{}目标仓位更新: {} -> {}", _name.c_str(), stdCode, oldVol, newVol);  // 记录日志：目标仓位更新
		}

		//这里广播目标仓位
		// 注意：实际执行由外部分布式系统完成，这里只更新目标仓位
	}
}

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
void WtDistExecuter::on_position_changed(const char* stdCode, double targetPos)
{
	targetPos *= _scale;  // 应用放大倍数（目标仓位乘以放大倍数）

	double oldVol = _target_pos[stdCode];  // 获取旧的目标仓位（如果不存在则为0）
	_target_pos[stdCode] = targetPos;  // 更新目标仓位映射表

	if (!decimal::eq(oldVol, targetPos))  // 如果目标仓位发生变化（使用小数精度比较）
	{
		WTSLogger::log_dyn("executer", _name.c_str(), LL_INFO, "[{}]{}目标仓位更新: {} -> {}", _name.c_str(), stdCode, oldVol, targetPos);  // 记录日志：目标仓位更新
	}

	//这里广播目标仓位
	// 注意：实际执行由外部分布式系统完成，这里只更新目标仓位
}

/**
 * @brief 实时行情回调
 * @param stdCode 标准合约代码字符串
 * @param newTick 新的Tick数据指针
 * 
 * 当收到实时行情数据时被调用。
 * 分布式执行器不需要处理行情数据，此方法为空实现。
 */
void WtDistExecuter::on_tick(const char* stdCode, WTSTickData* newTick)
{
	//分布式执行器不需要处理ontick
	// 分布式执行器不执行实际交易，只管理目标仓位
	// 行情数据由外部分布式系统处理
}
