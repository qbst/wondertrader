/*!
 * \file WtHftStraFact.cpp
 * \project	WonderTrader
 * 
 * \brief HFT高频策略工厂类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WonderTrader框架中HFT高频策略工厂类的所有功能。
 * 主要功能包括：
 * 1. 实现策略工厂的创建和删除接口，供动态库导出使用
 * 2. 实现策略的创建逻辑，根据策略名称创建对应的策略实例
 * 3. 实现策略的删除逻辑，安全删除策略对象并验证所有权
 * 4. 实现策略枚举功能，列出工厂中所有可用的策略类型
 * 5. 提供工厂名称查询功能，返回工厂的唯一标识符
 * 
 * 该文件是HFT策略系统的核心实现，通过工厂模式实现了策略的标准化管理。
 * 支持动态库加载，可以作为插件被WonderTrader框架动态加载和卸载。
 */
#include "WtHftStraFact.h"  // 包含策略工厂类头文件
#include "WtHftStraDemo.h"  // 包含SimpleHft策略类头文件

#include <string.h>  // 包含字符串处理函数（如strcmp）

const char* FACT_NAME = "WtHftStraFact";  // 定义工厂名称常量，用于标识本策略工厂


/**
 * @brief 导出C接口函数，供动态库加载使用
 * 
 * 该命名空间包含动态库导出函数，用于创建和删除策略工厂实例。
 * 这些函数会被WonderTrader框架动态调用，实现策略工厂的插件化加载。
 */
extern "C"  // 使用C语言链接规范，确保函数名不被C++编译器修饰
{
	/**
	 * @brief 创建策略工厂实例
	 * @return IHftStrategyFact* 返回创建的策略工厂对象指针
	 * 
	 * 该函数创建一个新的策略工厂实例并返回其指针。
	 * 该函数会被WonderTrader框架在加载动态库时调用，用于获取策略工厂实例。
	 * 
	 * @note 使用EXPORT_FLAG宏标记为导出函数，可以被外部动态调用
	 * @warning 返回的指针由调用者负责管理，使用完毕后应调用deleteStrategyFact删除
	 */
	EXPORT_FLAG IHftStrategyFact* createStrategyFact()  // 导出函数：创建策略工厂
	{
		IHftStrategyFact* fact = new WtHftStraFact();  // 创建策略工厂对象实例
		return fact;  // 返回工厂对象指针
	}

	/**
	 * @brief 删除策略工厂实例
	 * @param fact 要删除的策略工厂对象指针
	 * 
	 * 该函数删除指定的策略工厂实例，释放相关资源。
	 * 该函数会被WonderTrader框架在卸载动态库时调用，用于清理策略工厂实例。
	 * 
	 * @note 使用EXPORT_FLAG宏标记为导出函数，可以被外部动态调用
	 * @note 函数内部会检查指针是否为空，避免重复删除
	 */
	EXPORT_FLAG void deleteStrategyFact(IHftStrategyFact* fact)  // 导出函数：删除策略工厂
	{
		if (fact != NULL)  // 检查指针是否为空
			delete fact;  // 删除工厂对象，调用析构函数释放资源
	}
}


/**
 * @brief 构造函数实现
 * 
 * 初始化HFT策略工厂对象，执行必要的初始化操作。
 * 当前实现为空构造函数，不执行任何特殊操作。
 * 如果将来需要初始化操作，可以在此处添加。
 */
WtHftStraFact::WtHftStraFact()  // 构造函数实现
{
	// 构造函数体为空，使用默认初始化
}


/**
 * @brief 析构函数实现
 * 
 * 析构函数在对象销毁时自动调用，释放工厂对象占用的资源。
 * 当前实现为空析构函数，不执行任何特殊操作。
 * 如果将来需要清理操作，可以在此处添加。
 */
WtHftStraFact::~WtHftStraFact()  // 析构函数实现
{
	// 析构函数体为空，使用默认清理
}

/**
 * @brief 获取工厂名称的实现
 * @return const char* 返回策略工厂的名称字符串
 * 
 * 该函数返回策略工厂的名称，用于标识和管理不同的策略工厂。
 * 返回的名称是常量字符串"WtHftStraFact"，在系统中应该是唯一的。
 * 
 * @note 返回的是常量字符串指针，不需要调用者释放内存
 */
const char* WtHftStraFact::getName()  // 获取工厂名称函数实现
{
	return FACT_NAME;  // 返回工厂名称常量
}

/**
 * @brief 枚举策略名称的实现
 * @param cb 枚举策略名称的回调函数，每枚举到一个策略都会调用此回调
 * 
 * 该函数枚举工厂中所有可用的策略类型，通过回调函数通知调用者。
 * 当前工厂支持的策略：
 * - "SimpleHft": SimpleHft简单高频交易策略示例
 * 
 * 回调函数会被调用一次，传入以下参数：
 * - factName: 工厂名称（"WtHftStraFact"）
 * - straName: 策略名称（"SimpleHft"）
 * - isLast: 是否为最后一个策略（true，因为只有一个策略）
 * 
 * @note 如果将来添加更多策略，需要在此函数中添加更多的回调调用
 */
void WtHftStraFact::enumStrategy(FuncEnumHftStrategyCallback cb)  // 枚举策略函数实现
{
	cb(FACT_NAME, "SimpleHft", true);  // 调用回调函数，传入工厂名称、策略名称和是否为最后一个策略
}

/**
 * @brief 创建策略实例的实现
 * @param name 策略名称，用于指定要创建的策略类型
 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
 * @return HftStrategy* 返回创建的策略对象指针，如果策略名称不存在则返回NULL
 * 
 * 该函数根据策略名称创建对应的策略对象实例。
 * 当前支持的策略类型：
 * - "SimpleHft": 创建SimpleHft简单高频交易策略实例
 * 
 * 如果传入的策略名称不匹配任何已知策略，则返回NULL。
 * 
 * @note 调用者负责管理返回的指针，使用完毕后应通过deleteStrategy删除
 */
HftStrategy* WtHftStraFact::createStrategy(const char* name, const char* id)  // 创建策略函数实现
{
	if(strcmp(name, "SimpleHft") == 0)  // 比较策略名称是否为"SimpleHft"
	{
		return new WtHftStraDemo(id);  // 创建SimpleHft策略实例并返回
	}


	return NULL;  // 如果策略名称不匹配，返回NULL
}

/**
 * @brief 删除策略实例的实现
 * @param stra 要删除的策略对象指针
 * @return bool 删除成功返回true，失败返回false
 * 
 * 该函数删除指定的策略对象，释放相关资源。
 * 删除前会进行以下检查：
 * 1. 检查策略指针是否为空，如果为空则直接返回true（视为成功）
 * 2. 检查策略是否属于本工厂创建，通过比较策略的工厂名称
 * 3. 只有属于本工厂的策略才会被删除，其他策略返回false
 * 
 * @note 删除后策略指针将失效，调用者不应再使用该指针
 */
bool WtHftStraFact::deleteStrategy(HftStrategy* stra)  // 删除策略函数实现
{
	if (stra == NULL)  // 检查策略指针是否为空
		return true;  // 如果为空，返回true（视为删除成功）

	if (strcmp(stra->getFactName(), FACT_NAME) != 0)  // 检查策略是否属于本工厂创建
		return false;  // 如果不属于本工厂，返回false（拒绝删除）

	delete stra;  // 删除策略对象，调用析构函数释放资源
	return true;  // 返回true表示删除成功
}
