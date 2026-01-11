/*!
 * \file WtExeFact.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtExeFact执行单元工厂类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了WtExeFact类，这是WonderTrader执行单元工厂的实现类，继承自IExecuterFact接口。
 * 执行单元工厂负责创建和管理各种执行单元（ExecuteUnit），执行单元用于智能执行交易订单，
 * 实现TWAP、VWAP、最小冲击等算法执行策略。
 * 
 * 设计目标：
 * 1. 实现执行单元工厂接口，支持执行单元的创建、删除和管理
 * 2. 提供多种执行单元类型：标准执行单元、差量执行单元、套利执行单元
 * 3. 支持执行单元的枚举和查询功能
 * 4. 实现执行单元的生命周期管理
 * 
 * 核心功能：
 * - 执行单元创建：根据名称创建对应的执行单元实例
 * - 执行单元枚举：枚举所有可用的执行单元类型
 * - 执行单元删除：删除指定的执行单元实例
 * - 工厂名称管理：返回工厂名称，用于标识和管理
 * 
 * 支持的执行单元类型：
 * - WtTWapExeUnit：时间加权平均价格执行单元（TWAP算法）
 * - WtMinImpactExeUnit：最小冲击执行单元（适用于期货）
 * - WtStockMinImpactExeUnit：最小冲击执行单元（适用于股票）
 * - WtVWapExeUnit：成交量加权平均价格执行单元（VWAP算法，适用于期货）
 * - WtStockVWapExeUnit：成交量加权平均价格执行单元（VWAP算法，适用于股票）
 * - WtDiffMinImpactExeUnit：差量最小冲击执行单元（差量模式）
 * 
 * 架构特点：
 * - 采用工厂模式，统一管理执行单元的创建和删除
 * - 支持插件化开发，可以通过动态库加载不同的执行单元工厂
 * - 使用字符串名称标识执行单元类型，便于配置和扩展
 */
#pragma once
#include "../Includes/ExecuteDefs.h"  // 执行单元定义文件（包含IExecuterFact接口和ExecuteUnit基类）

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WtExeFact执行单元工厂类
 * 
 * 继承自IExecuterFact接口，实现WonderTrader执行单元工厂的功能。
 * 该工厂负责创建和管理各种执行单元，包括TWAP、VWAP、最小冲击等算法执行单元。
 * 
 * 执行单元用于智能执行交易订单，根据市场行情和配置参数，自动调整订单价格和数量，
 * 以实现最优的执行效果（如最小市场冲击、接近VWAP价格等）。
 */
class WtExeFact : public IExecuterFact
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建执行单元工厂实例，初始化工厂对象。
	 */
	WtExeFact();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存。
	 */
	virtual ~WtExeFact();

public:
	/**
	 * @brief 获取工厂名称
	 * 
	 * 返回执行单元工厂的名称，用于标识和管理不同的执行单元工厂。
	 * 
	 * @return const char* 返回工厂名称字符串（"WtExeFact"）
	 * 
	 * 重写自IExecuterFact接口
	 */
	virtual const char* getName() override;

	/**
	 * @brief 枚举执行单元
	 * 
	 * 枚举工厂中所有可用的执行单元，通过回调函数通知调用者。
	 * 该函数会枚举所有标准执行单元和差量执行单元。
	 * 
	 * @param cb 枚举回调函数，对每个执行单元调用，参数为工厂名称、执行单元名称、是否为最后一个
	 * 
	 * 重写自IExecuterFact接口
	 */
	virtual void enumExeUnit(FuncEnumUnitCallback cb) override;

	/**
	 * @brief 创建标准执行单元
	 * 
	 * 根据执行单元名称创建对应的标准执行单元实例。
	 * 标准执行单元用于目标驱动模式（Target Driven），将仓位调整到目标值。
	 * 
	 * @param name 执行单元名称，支持以下类型：
	 *   - "WtTWapExeUnit"：时间加权平均价格执行单元
	 *   - "WtMinImpactExeUnit"：最小冲击执行单元（期货）
	 *   - "WtStockMinImpactExeUnit"：最小冲击执行单元（股票）
	 *   - "WtVWapExeUnit"：成交量加权平均价格执行单元（期货）
	 *   - "WtStockVWapExeUnit"：成交量加权平均价格执行单元（股票）
	 * @return ExecuteUnit* 返回创建的执行单元对象指针，如果名称不存在则返回NULL
	 * 
	 * 重写自IExecuterFact接口
	 */
	virtual ExecuteUnit* createExeUnit(const char* name) override;

	/**
	 * @brief 创建差量执行单元
	 * 
	 * 根据执行单元名称创建对应的差量执行单元实例。
	 * 差量执行单元用于增量驱动模式（Delta Driven），立即执行指定的数量变化。
	 * 
	 * @param name 执行单元名称，支持以下类型：
	 *   - "WtDiffMinImpactExeUnit"：差量最小冲击执行单元
	 * @return ExecuteUnit* 返回创建的差量执行单元对象指针，如果名称不存在则返回NULL
	 * 
	 * 重写自IExecuterFact接口
	 */
	virtual ExecuteUnit* createDiffExeUnit(const char* name) override;

	/**
	 * @brief 创建套利执行单元
	 * 
	 * 根据执行单元名称创建对应的套利执行单元实例。
	 * 套利执行单元用于组合/价差驱动模式（Spread Driven），将组合头寸调整到目标值。
	 * 
	 * 注意：当前版本不支持套利执行单元，该函数始终返回NULL。
	 * 
	 * @param name 执行单元名称
	 * @return ExecuteUnit* 返回创建的套利执行单元对象指针，当前版本始终返回NULL
	 * 
	 * 重写自IExecuterFact接口
	 */
	virtual ExecuteUnit* createArbiExeUnit(const char* name) override;

	/**
	 * @brief 删除执行单元
	 * 
	 * 删除指定的执行单元实例，释放其占用的内存。
	 * 该函数会检查执行单元是否属于本工厂，只有属于本工厂的执行单元才会被删除。
	 * 
	 * @param unit 执行单元对象指针
	 * @return bool 返回是否删除成功，true表示删除成功，false表示删除失败（不属于本工厂或unit为NULL）
	 * 
	 * 重写自IExecuterFact接口
	 */
	virtual bool deleteExeUnit(ExecuteUnit* unit) override;

};

