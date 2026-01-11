/*!
 * \file WtCtaStraFact.h
 * \project	WonderTrader
 * 
 * \brief CTA策略工厂类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中CTA策略工厂类的接口声明。
 * 主要功能包括：
 * 1. 实现ICtaStrategyFact接口，提供策略工厂的基本功能
 * 2. 负责创建和管理CTA策略实例，支持策略的动态加载
 * 3. 提供策略枚举功能，允许查询工厂中可用的策略类型
 * 4. 实现策略的创建和删除功能，管理策略对象的生命周期
 * 5. 作为策略工厂模式的实现，支持插件化策略开发
 * 
 * 该类是WonderTrader框架中CTA策略系统的核心组件之一，
 * 通过工厂模式实现了策略的标准化创建和管理机制。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/CtaStrategyDefs.h"  // 包含CTA策略定义头文件，提供ICtaStrategyFact接口和CtaStrategy基类

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class WtStraFact
 * @brief CTA策略工厂类
 * 
 * 该类实现了ICtaStrategyFact接口，是WonderTrader框架中CTA策略的工厂类。
 * 负责创建、管理和删除CTA策略实例，支持策略的动态加载和插件化开发。
 * 
 * 主要功能：
 * - 创建策略：根据策略名称创建对应的策略对象实例
 * - 删除策略：安全删除策略对象，释放相关资源
 * - 枚举策略：枚举工厂中所有可用的策略类型
 * - 工厂标识：提供工厂名称，用于标识和管理不同的策略工厂
 * 
 * 设计模式：
 * - 工厂模式：通过工厂类统一管理策略的创建过程
 * - 插件化：支持动态加载策略工厂，实现策略的插件化开发
 */
class WtStraFact : public ICtaStrategyFact  // 继承自CTA策略工厂接口
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化CTA策略工厂对象，执行必要的初始化操作。
	 * 构造函数不接收参数，使用默认配置进行初始化。
	 */
	WtStraFact();  // 构造函数声明

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 在对象销毁时自动调用，释放工厂对象占用的资源。
	 */
	virtual ~WtStraFact();  // 虚析构函数声明，支持多态销毁

public:
	/**
	 * @brief 获取工厂名称
	 * @return const char* 返回策略工厂的名称字符串
	 * 
	 * 该函数返回策略工厂的名称，用于标识和管理不同的策略工厂。
	 * 工厂名称在系统中应该是唯一的，用于区分不同的策略工厂实例。
	 * 
	 * @note 该函数重写了ICtaStrategyFact接口的纯虚函数
	 */
	virtual const char* getName() override;  // 重写接口函数：获取工厂名称

	/**
	 * @brief 创建策略实例
	 * @param name 策略名称，用于指定要创建的策略类型（如"DualThrust"）
	 * @param id 策略唯一标识符，用于在系统中唯一标识该策略实例
	 * @return CtaStrategy* 返回创建的策略对象指针，如果策略名称不存在则返回NULL
	 * 
	 * 该函数根据策略名称创建对应的策略对象实例。
	 * 支持的策略类型包括：
	 * - "DualThrust": DualThrust双突破策略
	 * 
	 * @note 该函数重写了ICtaStrategyFact接口的纯虚函数
	 * @warning 调用者负责管理返回的指针，使用完毕后应通过deleteStrategy删除
	 */
	virtual CtaStrategy* createStrategy(const char* name, const char* id) override;  // 重写接口函数：创建策略

	/**
	 * @brief 枚举策略名称
	 * @param cb 枚举策略名称的回调函数，每枚举到一个策略都会调用此回调
	 * 
	 * 该函数枚举工厂中所有可用的策略类型，通过回调函数通知调用者。
	 * 回调函数会被调用多次，每次传入一个策略的信息：
	 * - factName: 工厂名称
	 * - straName: 策略名称
	 * - isLast: 是否为最后一个策略
	 * 
	 * @note 该函数重写了ICtaStrategyFact接口的纯虚函数
	 */
	virtual void enumStrategy(FuncEnumStrategyCallback cb) override;  // 重写接口函数：枚举策略

	/**
	 * @brief 删除策略实例
	 * @param stra 要删除的策略对象指针
	 * @return bool 删除成功返回true，失败返回false
	 * 
	 * 该函数删除指定的策略对象，释放相关资源。
	 * 删除前会检查策略对象是否属于本工厂创建，只有属于本工厂的策略才会被删除。
	 * 
	 * @note 该函数重写了ICtaStrategyFact接口的纯虚函数
	 * @warning 删除后策略指针将失效，调用者不应再使用该指针
	 */
	virtual bool deleteStrategy(CtaStrategy* stra) override;  // 重写接口函数：删除策略
};

