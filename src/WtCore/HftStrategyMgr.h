/*!
 * \file HftStrategyMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易策略管理器头文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件定义了高频交易策略管理器HftStrategyMgr类，负责加载高频交易策略工厂，
 * 创建和管理高频交易策略实例。通过策略工厂模式，实现了策略的动态加载和生命周期管理。
 * HftStraWrapper是一个辅助类，用于封装HftStrategy指针和其所属的工厂，
 * 确保策略在不再使用时能够通过工厂正确删除。
 * HftStrategyMgr使用boost::noncopyable确保其单例或唯一性。
 */
#pragma once  // 防止头文件重复包含
#include <memory>  // 包含智能指针头文件
#include <boost/core/noncopyable.hpp>  // 包含Boost的noncopyable类，用于禁止拷贝构造和赋值

#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap
#include "../Includes/HftStrategyDefs.h"  // 包含高频策略相关的定义，如HftStrategy和IHftStrategyFact

#include "../Share/DLLHelper.hpp"  // 包含DLLHelper，用于动态加载DLL/SO

/**
 * @class HftStraWrapper
 * @brief 高频交易策略智能指针封装类
 *
 * 该类用于封装HftStrategy指针和其所属的IHftStrategyFact工厂指针。
 * 通过RAII（资源获取即初始化）机制，确保在HftStraWrapper对象生命周期结束时，
 * 能够通过工厂的deleteStrategy方法正确删除HftStrategy实例，
 * 避免内存泄漏，并支持策略的引用计数管理。
 */
class HftStraWrapper
{
public:
	/**
	 * @brief 构造函数
	 * @param stra 高频交易策略实例指针
	 * @param fact 高频交易策略工厂实例指针
	 *
	 * 初始化HftStraWrapper对象，保存策略实例和工厂实例的指针。
	 */
	HftStraWrapper(HftStrategy* stra, IHftStrategyFact* fact) :_stra(stra), _fact(fact){}  // 构造函数，初始化策略和工厂指针
	/**
	 * @brief 析构函数
	 *
	 * 在对象销毁时，如果策略实例存在，则通过工厂的deleteStrategy方法删除策略实例。
	 */
	~HftStraWrapper()
	{
		if (_stra)  // 如果策略实例存在
		{
			_fact->deleteStrategy(_stra);  // 通过工厂删除策略实例
		}
	}

	/**
	 * @brief 获取策略实例指针
	 * @return HftStrategy* 返回高频交易策略实例的原始指针
	 *
	 * 提供对内部封装的HftStrategy实例的访问。
	 */
	HftStrategy* self(){ return _stra; }  // 返回策略实例的原始指针


private:
	HftStrategy*		_stra;  // 高频交易策略实例指针
	IHftStrategyFact*	_fact;  // 高频交易策略工厂实例指针
};
typedef std::shared_ptr<HftStraWrapper>	HftStrategyPtr;  // 定义HftStrategyPtr为HftStraWrapper的共享指针类型

/**
 * @class HftStrategyMgr
 * @brief 高频交易策略管理器类
 *
 * 该类负责管理高频交易策略工厂和高频交易策略实例。
 * 它能够从指定路径加载策略工厂动态库，并根据工厂创建和获取策略实例。
 * 使用boost::noncopyable继承，禁止对象的拷贝构造和赋值，确保其单例或唯一性。
 */
class HftStrategyMgr : private boost::noncopyable  // 继承boost::noncopyable，禁止拷贝构造和赋值
{
public:
	/**
	 * @brief 构造函数
	 *
	 * 初始化HftStrategyMgr对象。
	 */
	HftStrategyMgr();  // 构造函数
	/**
	 * @brief 析构函数
	 *
	 * 销毁HftStrategyMgr对象，释放所有加载的策略工厂和策略实例。
	 */
	~HftStrategyMgr();  // 析构函数

public:
	/**
	 * @brief 加载策略工厂
	 * @param path 策略工厂动态库所在的目录路径
	 * @return bool 加载成功返回true，失败返回false
	 *
	 * 遍历指定路径下的动态库文件（.dll或.so），尝试加载为高频交易策略工厂。
	 * 成功加载的工厂会被存储起来，并通过工厂创建策略实例。
	 */
	bool loadFactories(const char* path);  // 加载策略工厂

	/**
	 * @brief 创建高频交易策略实例
	 * @param name 策略名称，格式为"工厂名.单元名"
	 * @param id 策略实例的唯一ID
	 * @return HftStrategyPtr 返回创建的高频交易策略的共享指针，如果失败则返回空指针
	 *
	 * 根据策略名称（包含工厂名和单元名）和策略ID创建高频交易策略实例。
	 * 内部会解析名称以找到对应的工厂。
	 */
	HftStrategyPtr createStrategy(const char* name, const char* id);  // 创建高频交易策略实例
	/**
	 * @brief 创建高频交易策略实例（重载）
	 * @param factname 策略工厂名称
	 * @param unitname 策略单元名称
	 * @param id 策略实例的唯一ID
	 * @return HftStrategyPtr 返回创建的高频交易策略的共享指针，如果失败则返回空指针
	 *
	 * 根据策略工厂名称、策略单元名称和策略ID创建高频交易策略实例。
	 */
	HftStrategyPtr createStrategy(const char* factname, const char* unitname, const char* id);  // 创建高频交易策略实例（重载）

	/**
	 * @brief 获取高频交易策略实例
	 * @param id 策略实例的唯一ID
	 * @return HftStrategyPtr 返回对应ID的高频交易策略的共享指针，如果未找到则返回空指针
	 *
	 * 根据策略ID获取已创建的高频交易策略实例。
	 */
	HftStrategyPtr getStrategy(const char* id);  // 获取高频交易策略实例
private:
	/**
	 * @struct _StraFactInfo
	 * @brief 策略工厂信息结构体
	 *
	 * 存储加载的策略工厂的详细信息，包括模块路径、实例句柄、工厂接口指针、
	 * 创建函数和删除函数指针。
	 */
	typedef struct _StraFactInfo  // 策略工厂信息结构体
	{
		std::string		_module_path;  // 模块文件路径
		DllHandle		_module_inst;  // 模块实例句柄
		IHftStrategyFact*	_fact;  // 策略工厂接口指针
		FuncCreateHftStraFact	_creator;  // 创建策略工厂的函数指针
		FuncDeleteHftStraFact	_remover;  // 删除策略工厂的函数指针

		/**
		 * @brief 构造函数
		 *
		 * 初始化结构体成员，将模块实例句柄和工厂指针设置为NULL。
		 */
		_StraFactInfo()
		{
			_module_inst = NULL;  // 初始化模块实例句柄为NULL
			_fact = NULL;  // 初始化工厂指针为NULL
		}

		/**
		 * @brief 析构函数
		 *
		 * 在对象销毁时，如果工厂指针存在，则通过删除函数删除工厂实例。
		 */
		~_StraFactInfo()
		{
			if (_fact)  // 如果工厂指针存在
				_remover(_fact);  // 通过删除函数删除工厂实例
		}
	} StraFactInfo;  // 策略工厂信息
	typedef wt_hashmap<std::string, StraFactInfo> StraFactMap;  // 策略工厂映射表，键为工厂名称，值为工厂信息

	StraFactMap	_factories;  // 已加载的策略工厂映射表

	typedef wt_hashmap<std::string, HftStrategyPtr> StrategyMap;  // 策略实例映射表，键为策略ID，值为策略共享指针
	StrategyMap	_strategies;  // 已创建的策略实例映射表
};

