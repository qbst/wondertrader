/*!
 * \file CtaStrategyMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略管理器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中CTA（Commodity Trading Advisor）策略管理器的核心类。
 * 主要功能包括：
 * 1. 策略工厂的动态加载：从指定目录加载策略工厂DLL/动态库，支持插件化策略开发
 * 2. 策略实例的创建与管理：通过工厂模式创建策略实例，并使用智能指针管理生命周期
 * 3. 策略实例的查询：根据策略ID快速查找已创建的策略实例
 * 4. 采用RAII模式管理策略资源：通过CtaStraWrapper包装类确保策略对象正确释放
 * 
 * 设计特点：
 * - 使用boost::noncopyable禁止拷贝，确保管理器单例特性
 * - 使用shared_ptr智能指针管理策略生命周期，避免内存泄漏
 * - 支持Windows和Linux平台的动态库加载
 * - 通过工厂模式实现策略的插件化加载和管理
 */
#pragma once  // 防止头文件重复包含
#include <memory>  // 包含智能指针相关类（shared_ptr等）
#include <boost/core/noncopyable.hpp>  // 包含boost库的非拷贝基类，禁止对象拷贝

#include "../Includes/FasterDefs.h"  // 包含快速定义的头文件
#include "../Includes/CtaStrategyDefs.h"  // 包含CTA策略定义的头文件

#include "../Share/DLLHelper.hpp"  // 包含动态库加载辅助工具

/**
 * @class CtaStraWrapper
 * @brief CTA策略包装类
 * 
 * 该类负责包装CTA策略对象和其对应的工厂对象，实现RAII模式。
 * 当包装对象析构时，自动通过工厂对象删除策略实例，确保资源正确释放。
 * 主要特点：
 * - 持有策略对象指针和工厂对象指针
 * - 析构时自动调用工厂的deleteStrategy方法删除策略
 * - 提供self()方法获取内部策略对象指针
 */
class CtaStraWrapper
{
public:
	/**
	 * @brief 构造函数
	 * @param stra CTA策略对象指针
	 * @param fact CTA策略工厂对象指针
	 * 
	 * 初始化包装对象，保存策略对象和工厂对象的指针。
	 */
	CtaStraWrapper(CtaStrategy* stra, ICtaStrategyFact* fact) :_stra(stra), _fact(fact){}  // 初始化策略指针和工厂指针
	/**
	 * @brief 析构函数
	 * 
	 * 当包装对象析构时，如果策略对象存在，则通过工厂对象删除策略实例。
	 * 确保策略对象被正确释放，避免内存泄漏。
	 */
	~CtaStraWrapper()
	{
		if (_stra)  // 如果策略对象存在
		{
			_fact->deleteStrategy(_stra);  // 通过工厂对象删除策略实例
		}
	}

	/**
	 * @brief 获取策略对象指针
	 * @return CtaStrategy* 返回策略对象指针
	 * 
	 * 返回内部保存的策略对象指针，供外部使用。
	 */
	CtaStrategy* self(){ return _stra; }  // 返回策略对象指针


private:
	CtaStrategy*		_stra;  // CTA策略对象指针，指向被包装的策略实例
	ICtaStrategyFact*	_fact;  // CTA策略工厂对象指针，用于删除策略实例
};
/**
 * @typedef CtaStrategyPtr
 * @brief CTA策略智能指针类型别名
 * 
 * 定义CTA策略包装类的共享智能指针类型，用于自动管理策略对象的生命周期。
 * 多个地方可以共享同一个策略实例，当最后一个引用释放时自动删除策略对象。
 */
typedef std::shared_ptr<CtaStraWrapper>	CtaStrategyPtr;  // 定义策略包装类的共享智能指针类型


/**
 * @class CtaStrategyMgr
 * @brief CTA策略管理器类
 * 
 * 该类负责管理CTA策略工厂和策略实例的创建、加载和查询。
 * 主要功能：
 * - 从指定目录动态加载策略工厂DLL/动态库
 * - 通过工厂创建策略实例
 * - 管理和查询已创建的策略实例
 * 
 * 设计特点：
 * - 继承boost::noncopyable，禁止拷贝和赋值
 * - 使用哈希表存储工厂信息和策略实例
 * - 支持通过工厂名和策略名创建策略
 * - 支持通过策略ID查询策略实例
 */
class CtaStrategyMgr : private boost::noncopyable  // 继承非拷贝基类，禁止对象拷贝和赋值
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化策略管理器对象，创建空的工厂映射表和策略映射表。
	 */
	CtaStrategyMgr();  // 构造函数声明
	/**
	 * @brief 析构函数
	 * 
	 * 清理策略管理器对象，释放所有资源。
	 * 由于使用智能指针管理策略，会自动释放策略对象。
	 */
	~CtaStrategyMgr();  // 析构函数声明

public:
	/**
	 * @brief 加载策略工厂
	 * @param path 策略工厂DLL/动态库所在的目录路径
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 从指定目录扫描并加载所有策略工厂动态库。
	 * 加载过程：
	 * 1. 检查目录是否存在
	 * 2. 遍历目录中的所有DLL/动态库文件
	 * 3. 加载动态库并获取创建和删除工厂的函数指针
	 * 4. 创建工厂实例并保存到映射表中
	 */
	bool loadFactories(const char* path);  // 加载策略工厂函数声明

	/**
	 * @brief 创建策略实例（通过策略名称）
	 * @param name 策略名称，格式为"工厂名.策略名"
	 * @param id 策略实例的唯一标识ID
	 * @return CtaStrategyPtr 返回策略智能指针，失败返回空指针
	 * 
	 * 根据策略名称创建策略实例。
	 * 策略名称格式：工厂名.策略名（例如："WtStraFact.MAStrategy"）
	 * 创建成功后，策略实例会被保存到策略映射表中，可通过ID查询。
	 */
	CtaStrategyPtr createStrategy(const char* name, const char* id);  // 通过策略名称创建策略的函数声明
	/**
	 * @brief 创建策略实例（通过工厂名和策略名）
	 * @param factname 策略工厂名称
	 * @param unitname 策略单元名称
	 * @param id 策略实例的唯一标识ID
	 * @return CtaStrategyPtr 返回策略智能指针，失败返回空指针
	 * 
	 * 根据工厂名和策略名创建策略实例。
	 * 这种方式可以直接指定工厂名和策略名，不需要解析名称字符串。
	 * 创建成功后，策略实例会被保存到策略映射表中，可通过ID查询。
	 */
	CtaStrategyPtr createStrategy(const char* factname, const char* unitname, const char* id);  // 通过工厂名和策略名创建策略的函数声明

	/**
	 * @brief 获取策略实例
	 * @param id 策略实例的唯一标识ID
	 * @return CtaStrategyPtr 返回策略智能指针，不存在返回空指针
	 * 
	 * 根据策略ID从映射表中查找并返回策略实例。
	 * 如果策略不存在，返回空指针。
	 */
	CtaStrategyPtr getStrategy(const char* id);  // 获取策略实例的函数声明
private:
	/**
	 * @struct _StraFactInfo
	 * @brief 策略工厂信息结构体
	 * 
	 * 存储策略工厂的所有相关信息，包括：
	 * - 动态库路径和句柄
	 * - 工厂对象指针
	 * - 创建和删除工厂的函数指针
	 */
	typedef struct _StraFactInfo
	{
		std::string		_module_path;  // 动态库文件路径
		DllHandle		_module_inst;  // 动态库句柄，用于后续释放库
		ICtaStrategyFact*	_fact;  // 策略工厂对象指针
		FuncCreateStraFact	_creator;  // 创建工厂的函数指针
		FuncDeleteStraFact	_remover;  // 删除工厂的函数指针
	} StraFactInfo;  // 策略工厂信息结构体类型定义
	/**
	 * @typedef StraFactMap
	 * @brief 策略工厂映射表类型
	 * 
	 * 使用字符串作为键（工厂名称），StraFactInfo作为值，存储所有已加载的策略工厂。
	 */
	typedef wt_hashmap<std::string, StraFactInfo> StraFactMap;  // 定义策略工厂映射表类型

	StraFactMap	_factories;  // 策略工厂映射表，存储所有已加载的策略工厂信息

	/**
	 * @typedef StrategyMap
	 * @brief 策略实例映射表类型
	 * 
	 * 使用字符串作为键（策略ID），CtaStrategyPtr作为值，存储所有已创建的策略实例。
	 */
	typedef wt_hashmap<std::string, CtaStrategyPtr> StrategyMap;  // 定义策略实例映射表类型
	StrategyMap	_strategies;  // 策略实例映射表，存储所有已创建的策略实例
};

