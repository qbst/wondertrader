/*!
 * \file WtSimpRiskMon.h
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader简单风控监控器类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtSimpleRiskMon类，实现了基于回撤控制的风险监控功能。
 * 该风控监控器通过独立线程持续监控组合盘的资金状况，当检测到风险超过预设阈值时，
 * 自动采取降低仓位等风控措施，保护组合资金安全。
 * 
 * 主要功能：
 * 1. 日内回撤风控：监控日内从最高点的回撤幅度，超过阈值时降低仓位
 * 2. 多日回撤风控：监控多日最大动态权益的回撤幅度，超过阈值时清仓
 * 3. 盈利保护：当盈利达到一定比例后，启用回撤保护机制
 * 4. 定时检查：按照设定的时间间隔定期检查风险状况
 * 5. 仓位控制：通过设置数量倍数（vol_scale）来控制整体仓位比例
 * 
 * 风控逻辑：
 * - 日内风控：当日内最大权益超过止盈边界（basic_ratio），且从高点回撤超过阈值（inner_day_fd）
 *   且在指定时间窗口内（risk_span），则降低仓位至risk_scale比例
 * - 多日风控：当当前权益低于多日最大动态权益，且回撤超过阈值（multi_day_fd）时，清仓（vol_scale=0）
 * 
 * 设计特点：
 * - 多线程设计：使用独立线程执行风控检查，不阻塞主交易流程
 * - 可配置参数：支持通过配置文件设置各种风控参数，灵活适应不同策略需求
 * - 双重保护：同时支持日内和多日风控，提供双重保护机制
 * - 时间窗口控制：通过risk_span控制回撤检查的时间窗口，避免误触发
 */

#pragma once  // 防止头文件被重复包含

#include <thread>  // 包含C++11线程库，用于创建独立的风控检查线程
#include <memory>  // 包含智能指针库，用于管理线程对象的生命周期

#include "../Includes/RiskMonDefs.h"  // 包含风控模块定义头文件，提供WtRiskMonitor基类和WtPortContext接口

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WonderTrader简单风控监控器类
 * 
 * 该类继承自WtRiskMonitor基类，实现了基于回撤控制的风险监控功能。
 * 通过独立线程持续监控组合盘的资金状况，当检测到风险超过预设阈值时，
 * 自动采取降低仓位等风控措施，保护组合资金安全。
 * 
 * 风控策略：
 * 1. 日内回撤风控：监控日内从最高点的回撤幅度
 * 2. 多日回撤风控：监控多日最大动态权益的回撤幅度
 * 3. 盈利保护：当盈利达到一定比例后启用回撤保护
 * 
 * 使用流程：
 * 1. 创建对象：通过工厂创建WtSimpleRiskMon实例
 * 2. 初始化：调用init方法，传入上下文和配置参数
 * 3. 启动监控：调用run方法，启动独立线程开始风控检查
 * 4. 停止监控：调用stop方法，停止风控检查线程
 */
class WtSimpleRiskMon : public WtRiskMonitor
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化风控监控器对象，设置初始状态。
	 * 使用初始化列表初始化成员变量：
	 * - _stopped: 设置为false，表示监控未停止
	 * - _limited: 设置为false，表示未触发仓位限制
	 */
	WtSimpleRiskMon() :_stopped(false), _limited(false){}

public:
	/**
	 * @brief 获取风控监控器名称
	 * 
	 * 返回当前风控监控器的名称标识，用于区分不同的风控模块。
	 * 
	 * @return 返回"WtSimpleRiskMon"字符串
	 */
	virtual const char* getName() override;

	/**
	 * @brief 获取所属工厂名称
	 * 
	 * 返回创建当前风控监控器的工厂名称，用于工厂管理。
	 * 
	 * @return 返回工厂名称字符串
	 */
	virtual const char* getFactName() override;

	/**
	 * @brief 初始化风控监控器
	 * 
	 * 初始化风控监控器，设置运行环境和配置参数。
	 * 从配置对象中读取各种风控参数，包括检查间隔、回撤阈值、仓位比例等。
	 * 
	 * @param ctx 组合上下文接口指针，提供资金数据、交易状态、日志记录等功能
	 * @param cfg 配置参数对象，包含各种风控参数的配置值
	 * 
	 * 配置参数说明：
	 * - calc_span: 计算时间间隔（秒），风控检查的执行频率
	 * - risk_span: 回撤比较时间（分钟），日内回撤检查的时间窗口
	 * - basic_ratio: 基础盈利率（百分比），触发回撤保护的盈利阈值
	 * - inner_day_fd: 日内高点回撤边界（百分比），日内回撤触发阈值
	 * - inner_day_active: 日内风控是否启用（布尔值）
	 * - multi_day_fd: 多日高点回撤边界（百分比），多日回撤触发阈值
	 * - multi_day_active: 多日风控是否启用（布尔值）
	 * - base_amount: 基础资金规模（金额），用于计算权益比例
	 * - risk_scale: 风险控制系数（0-1），触发风控后的仓位比例
	 */
	virtual void init(WtPortContext* ctx, WTSVariant* cfg) override;

	/**
	 * @brief 启动风控监控
	 * 
	 * 启动独立线程，开始执行风控检查逻辑。
	 * 线程会按照设定的时间间隔定期检查组合盘的风险状况，
	 * 当检测到风险超过阈值时，自动采取相应的风控措施。
	 * 
	 * 注意事项：
	 * - 如果线程已启动，直接返回，避免重复启动
	 * - 线程会在_stopped为true时退出
	 * - 线程退出后需要调用stop方法等待线程结束
	 */
	virtual void run() override;

	/**
	 * @brief 停止风控监控
	 * 
	 * 停止风控检查线程，等待线程安全退出。
	 * 设置_stopped标志为true，通知线程退出，然后等待线程结束。
	 * 
	 * 注意事项：
	 * - 必须先设置_stopped标志，再等待线程结束
	 * - 如果线程未启动，直接返回
	 */
	virtual void stop() override;

private:
	typedef std::shared_ptr<std::thread> ThreadPtr;  // 定义线程智能指针类型，用于自动管理线程对象的生命周期

	ThreadPtr		_thrd;  // 风控检查线程的智能指针，管理线程对象的生命周期
	bool			_stopped;  // 停止标志，true表示风控监控已停止，false表示正在运行
	bool			_limited;  // 仓位限制标志，true表示已触发仓位限制，false表示未限制

	uint64_t		_last_time;  // 上次检查时间戳（毫秒），用于计算时间间隔

	uint32_t		_calc_span;			// 计算时间间隔，单位：秒，风控检查的执行频率
	uint32_t		_risk_span;			// 回撤比较时间，单位：分钟，日内回撤检查的时间窗口
	double			_basic_ratio;		// 基础盈利率，单位：百分比，触发回撤保护的盈利阈值（如101表示101%）
	double			_risk_scale;		// 风险控制系数，范围：0-1，触发风控后的仓位比例（如0.3表示30%仓位）
	double			_inner_day_fd;		// 日内高点回撤边界，单位：百分比，日内回撤触发阈值（如80表示80%）
	bool			_inner_day_active;	// 日内风控启用标志，true表示启用日内回撤风控，false表示禁用
	double			_multi_day_fd;		// 多日高点回撤边界，单位：百分比，多日回撤触发阈值（如20表示20%）
	bool			_multi_day_active;	// 多日风控启用标志，true表示启用多日回撤风控，false表示禁用
	double			_base_amount;		// 基础资金规模，单位：金额，用于计算权益比例的基础资金
};