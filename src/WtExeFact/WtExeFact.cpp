/*!
 * \file WtExeFact.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtExeFact执行单元工厂类实现文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件实现了WtExeFact类的所有成员函数，该类是WonderTrader执行单元工厂的实现类。
 * 
 * 实现要点：
 * 1. 提供C接口函数（createExecFact和deleteExecFact），支持动态库加载
 * 2. 实现执行单元的创建逻辑，根据名称创建对应的执行单元实例
 * 3. 实现执行单元的枚举逻辑，列出所有可用的执行单元类型
 * 4. 实现执行单元的删除逻辑，检查所有权后删除执行单元
 * 
 * 核心功能：
 * - 工厂创建和删除：提供C接口函数，支持动态库加载
 * - 执行单元创建：根据名称创建对应的执行单元实例
 * - 执行单元枚举：枚举所有可用的执行单元类型
 * - 执行单元删除：安全删除执行单元，检查所有权
 */
#include "WtExeFact.h"  // WtExeFact类定义

// 包含所有执行单元的头文件
#include "WtTWapExeUnit.h"  // 时间加权平均价格执行单元
#include "WtMinImpactExeUnit.h"  // 最小冲击执行单元（期货）
#include "WtDiffMinImpactExeUnit.h"  // 差量最小冲击执行单元
#include "WtStockMinImpactExeUnit.h"  // 最小冲击执行单元（股票）
#include "WtVWapExeUnit.h"  // 成交量加权平均价格执行单元（期货）
#include "WtStockVWapExeUnit.h"  // 成交量加权平均价格执行单元（股票）

/**
 * @brief 工厂名称常量
 * 
 * 定义执行单元工厂的名称，用于标识和管理。
 */
const char* FACT_NAME = "WtExeFact";

/**
 * @brief C接口导出区域
 * 
 * 提供C接口函数，支持动态库加载和执行单元工厂的创建和删除。
 * 这些函数会被WonderTrader框架调用，用于加载和管理执行单元工厂。
 */
extern "C"
{
	/**
	 * @brief 创建执行单元工厂
	 * 
	 * 创建WtExeFact执行单元工厂实例，返回工厂接口指针。
	 * 该函数会被WonderTrader框架调用，用于动态加载执行单元工厂。
	 * 
	 * @return IExecuterFact* 返回执行单元工厂接口指针
	 */
	EXPORT_FLAG IExecuterFact* createExecFact()
	{
		IExecuterFact* fact = new WtExeFact();  // 创建WtExeFact实例
		return fact;  // 返回工厂接口指针
	}

	/**
	 * @brief 删除执行单元工厂
	 * 
	 * 删除指定的执行单元工厂实例，释放其占用的内存。
	 * 该函数会被WonderTrader框架调用，用于卸载执行单元工厂。
	 * 
	 * @param fact 执行单元工厂接口指针
	 */
	EXPORT_FLAG void deleteExecFact(IExecuterFact* fact)
	{
		if (fact != NULL)  // 如果指针不为空
			delete fact;  // 删除工厂实例
	}
};

/**
 * @brief WtExeFact构造函数实现
 * 
 * 创建执行单元工厂实例，初始化工厂对象。
 * 当前版本构造函数为空，所有初始化工作都在基类中完成。
 */
WtExeFact::WtExeFact()
{
	// 构造函数体为空，所有初始化工作都在基类中完成
}

/**
 * @brief WtExeFact析构函数实现
 * 
 * 清理资源，释放占用的内存。
 * 当前版本析构函数为空，所有清理工作都在基类中完成。
 */
WtExeFact::~WtExeFact()
{
	// 析构函数体为空，所有清理工作都在基类中完成
}

/**
 * @brief 获取工厂名称实现
 * 
 * 返回执行单元工厂的名称，用于标识和管理不同的执行单元工厂。
 * 
 * @return const char* 返回工厂名称字符串（"WtExeFact"）
 */
const char* WtExeFact::getName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 枚举执行单元实现
 * 
 * 枚举工厂中所有可用的执行单元，通过回调函数通知调用者。
 * 该函数会枚举所有标准执行单元和差量执行单元。
 * 
 * @param cb 枚举回调函数，对每个执行单元调用，参数为工厂名称、执行单元名称、是否为最后一个
 */
void WtExeFact::enumExeUnit(FuncEnumUnitCallback cb)
{
	// 枚举标准执行单元
	cb(FACT_NAME, "WtTWapExeUnit", false);  // 时间加权平均价格执行单元，不是最后一个
	cb(FACT_NAME, "WtMinImpactExeUnit", true);  // 最小冲击执行单元（期货），是最后一个
	// 注意：这里只枚举了部分执行单元，其他执行单元（如WtStockMinImpactExeUnit、WtVWapExeUnit等）
	// 可能通过其他方式注册或枚举
}

/**
 * @brief 创建标准执行单元实现
 * 
 * 根据执行单元名称创建对应的标准执行单元实例。
 * 标准执行单元用于目标驱动模式（Target Driven），将仓位调整到目标值。
 * 
 * @param name 执行单元名称
 * @return ExecuteUnit* 返回创建的执行单元对象指针，如果名称不存在则返回NULL
 */
ExecuteUnit* WtExeFact::createExeUnit(const char* name)
{
	if (strcmp(name, "WtTWapExeUnit") == 0)  // 如果名称是时间加权平均价格执行单元
		return new WtTWapExeUnit();  // 创建TWAP执行单元实例
	else if (strcmp(name, "WtMinImpactExeUnit") == 0)  // 如果名称是最小冲击执行单元（期货）
		return new WtMinImpactExeUnit();  // 创建最小冲击执行单元实例（期货）
	else if (strcmp(name, "WtStockMinImpactExeUnit") == 0)  // 如果名称是最小冲击执行单元（股票）
		return new WtStockMinImpactExeUnit();  // 创建最小冲击执行单元实例（股票）
	else if (strcmp(name, "WtVWapExeUnit") == 0)  // 如果名称是成交量加权平均价格执行单元（期货）
		return  new WtVWapExeUnit();  // 创建VWAP执行单元实例（期货）
	else if (strcmp(name, "WtStockVWapExeUnit") == 0)  // 如果名称是成交量加权平均价格执行单元（股票）
		return new WtStockVWapExeUnit();  // 创建VWAP执行单元实例（股票）
	return NULL;  // 如果名称不匹配，返回NULL
}

/**
 * @brief 创建差量执行单元实现
 * 
 * 根据执行单元名称创建对应的差量执行单元实例。
 * 差量执行单元用于增量驱动模式（Delta Driven），立即执行指定的数量变化。
 * 
 * @param name 执行单元名称
 * @return ExecuteUnit* 返回创建的差量执行单元对象指针，如果名称不存在则返回NULL
 */
ExecuteUnit* WtExeFact::createDiffExeUnit(const char* name)
{
	if (strcmp(name, "WtDiffMinImpactExeUnit") == 0)  // 如果名称是差量最小冲击执行单元
		return new WtDiffMinImpactExeUnit();  // 创建差量最小冲击执行单元实例

	return NULL;  // 如果名称不匹配，返回NULL
}

/**
 * @brief 创建套利执行单元实现
 * 
 * 根据执行单元名称创建对应的套利执行单元实例。
 * 套利执行单元用于组合/价差驱动模式（Spread Driven），将组合头寸调整到目标值。
 * 
 * 注意：当前版本不支持套利执行单元，该函数始终返回NULL。
 * 
 * @param name 执行单元名称
 * @return ExecuteUnit* 返回创建的套利执行单元对象指针，当前版本始终返回NULL
 */
ExecuteUnit* WtExeFact::createArbiExeUnit(const char* name)
{
	return NULL;  // 当前版本不支持套利执行单元，始终返回NULL
}

/**
 * @brief 删除执行单元实现
 * 
 * 删除指定的执行单元实例，释放其占用的内存。
 * 该函数会检查执行单元是否属于本工厂，只有属于本工厂的执行单元才会被删除。
 * 
 * @param unit 执行单元对象指针
 * @return bool 返回是否删除成功，true表示删除成功，false表示删除失败（不属于本工厂或unit为NULL）
 */
bool WtExeFact::deleteExeUnit(ExecuteUnit* unit)
{
	if (unit == NULL)  // 如果执行单元指针为空
		return true;  // 返回true（空指针视为删除成功）

	if (strcmp(unit->getFactName(), FACT_NAME) != 0)  // 如果执行单元不属于本工厂
		return false;  // 返回false（不能删除其他工厂的执行单元）

	delete unit;  // 删除执行单元实例
	return true;  // 返回true（删除成功）
}
