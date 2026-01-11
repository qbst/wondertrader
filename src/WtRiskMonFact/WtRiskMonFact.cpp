/*!
 * \file WtRiskMonFact.cpp
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader风控模块工厂类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtRiskMonFact类的所有方法，包括工厂的创建、销毁以及风控模块的创建和管理功能。
 * 同时提供了C接口函数，供动态库加载时使用，实现风控模块工厂的动态加载机制。
 * 
 * 主要功能：
 * 1. 工厂创建与销毁：提供C接口函数createRiskMonFact和deleteRiskMonFact，供动态库加载器调用
 * 2. 模块枚举实现：实现enumRiskMonitors方法，枚举当前工厂支持的所有风控模块类型
 * 3. 模块创建实现：实现createRiskMonotor方法，根据名称创建对应的风控监控器实例
 * 4. 模块销毁实现：实现deleteRiskMonotor方法，安全地销毁风控监控器实例
 * 
 * 设计特点：
 * - C接口导出：使用extern "C"和EXPORT_FLAG宏，确保函数可以被C语言调用，便于动态库加载
 * - 名称匹配：通过字符串比较确保创建和销毁的模块属于当前工厂
 * - 空指针检查：在删除操作前检查指针有效性，避免空指针解引用
 * - 资源管理：统一管理风控模块的生命周期，确保资源正确释放
 */

#include "WtRiskMonFact.h"  // 包含当前类的头文件
#include "WtSimpRiskMon.h"  // 包含简单风控监控器头文件，用于创建SimpleRiskMon实例

const char* FACT_NAME = "WtRiskMonFact";  // 定义工厂名称常量，用于标识当前工厂

/**
 * @brief C接口：创建风控模块工厂
 * 
 * 该函数供动态库加载器调用，用于创建风控模块工厂实例。
 * 使用extern "C"确保函数名不被C++编译器进行名称修饰，可以被C语言直接调用。
 * 
 * @return 返回创建的IRiskMonitorFact接口指针，失败返回NULL
 * 
 * 使用场景：
 * - 系统启动时，动态库加载器调用此函数创建工厂
 * - 工厂创建后，系统可以通过工厂接口创建和管理风控模块
 */
extern "C"
{
	EXPORT_FLAG IRiskMonitorFact* createRiskMonFact()
	{
		IRiskMonitorFact* fact = new WtRiskMonFact();  // 创建WtRiskMonFact工厂实例
		return fact;  // 返回工厂接口指针
	}

	/**
	 * @brief C接口：删除风控模块工厂
	 * 
	 * 该函数供动态库加载器调用，用于销毁风控模块工厂实例。
	 * 在销毁前会检查指针有效性，确保安全删除。
	 * 
	 * @param fact 要删除的风控模块工厂指针
	 * 
	 * 使用场景：
	 * - 系统关闭时，动态库加载器调用此函数销毁工厂
	 * - 工厂销毁前，应确保所有由其创建的风控模块已正确销毁
	 */
	EXPORT_FLAG void deleteRiskMonFact(IRiskMonitorFact* fact)
	{
		if (fact != NULL)  // 检查指针有效性，避免空指针解引用
			delete fact;  // 删除工厂对象，释放资源
	}
}


/**
 * @brief 构造函数实现
 * 
 * 初始化风控模块工厂对象。当前实现为空构造函数，工厂对象创建后即可使用。
 * 如需初始化成员变量或执行其他准备工作，可在此处添加。
 */
WtRiskMonFact::WtRiskMonFact()
{
}


/**
 * @brief 析构函数实现
 * 
 * 清理工厂对象资源。当前实现为空析构函数。
 * 如需清理成员变量或执行其他清理工作，可在此处添加。
 */
WtRiskMonFact::~WtRiskMonFact()
{
}

/**
 * @brief 获取工厂名称
 * 
 * 返回当前工厂的名称标识字符串，用于区分不同的风控模块工厂。
 * 
 * @return 返回工厂名称常量"WtRiskMonFact"
 */
const char* WtRiskMonFact::getName()
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 枚举所有可用的风控模块
 * 
 * 遍历当前工厂支持的所有风控模块类型，通过回调函数逐一通知调用者。
 * 当前工厂支持"SimpleRiskMon"（简单风控监控器）模块。
 * 
 * @param cb 枚举回调函数，用于接收每个风控模块的信息
 * 
 * 回调函数参数说明：
 * - factName: 工厂名称，值为"WtRiskMonFact"
 * - unitName: 风控模块名称，当前为"SimpleRiskMon"
 * - isLast: 是否为最后一个模块，当前为true（只有一个模块）
 */
void WtRiskMonFact::enumRiskMonitors(FuncEnumRiskMonCallback cb)
{
	//cb(FACT_NAME, "WtSimpExeUnit", false);  // 注释掉的代码：之前可能支持的其他模块
	cb(FACT_NAME, "SimpleRiskMon", true);  // 调用回调函数，通知SimpleRiskMon模块，isLast为true表示这是最后一个模块
}

/**
 * @brief 根据名称创建风控监控器实例
 * 
 * 根据传入的风控模块名称，创建对应的风控监控器对象。
 * 当前支持创建"SimpleRiskMon"模块，其他名称返回NULL。
 * 
 * @param name 风控模块名称字符串
 * @return 成功返回风控监控器对象指针，失败返回NULL
 * 
 * 支持的模块：
 * - "SimpleRiskMon": 创建WtSimpleRiskMon实例，实现基础的日内和多日回撤风控
 * 
 * 注意事项：
 * - 返回的对象需要调用者负责管理生命周期
 * - 删除对象时应使用deleteRiskMonotor方法，确保安全删除
 */
WtRiskMonitor* WtRiskMonFact::createRiskMonotor(const char* name)
{
	if (strcmp(name, "SimpleRiskMon") == 0)  // 比较名称是否匹配"SimpleRiskMon"
		return new WtSimpleRiskMon();  // 创建简单风控监控器实例并返回
	return NULL;  // 名称不匹配，返回NULL
}

/**
 * @brief 删除风控监控器实例
 * 
 * 安全地销毁传入的风控监控器对象，释放其占用的资源。
 * 在删除前会进行多重检查，确保安全删除：
 * 1. 检查对象指针是否为NULL
 * 2. 检查对象是否属于当前工厂创建
 * 
 * @param unit 要删除的风控监控器对象指针
 * @return 删除成功返回true，失败返回false
 * 
 * 删除流程：
 * 1. 如果unit为NULL，直接返回true（视为成功，避免空指针操作）
 * 2. 调用unit->getFactName()获取对象的工厂名称
 * 3. 比较工厂名称是否与当前工厂名称匹配
 * 4. 如果匹配，执行delete删除对象；如果不匹配，返回false（防止误删）
 * 
 * 安全机制：
 * - 防止误删：只删除属于当前工厂创建的对象
 * - 空指针保护：NULL指针直接返回成功，避免崩溃
 */
bool WtRiskMonFact::deleteRiskMonotor(WtRiskMonitor* unit)
{
	if (unit == NULL)  // 检查指针是否为NULL
		return true;  // NULL指针视为删除成功，直接返回

	if (strcmp(unit->getFactName(), FACT_NAME) != 0)  // 比较对象的工厂名称是否与当前工厂名称匹配
		return false;  // 工厂名称不匹配，返回false，防止误删其他工厂创建的对象

	delete unit;  // 工厂名称匹配，安全删除对象
	return true;  // 删除成功，返回true
}
