/*!
* \file SelStrategyMgr.h
* \project	WonderTrader
*
* \author Wesley
* \date 2020/03/30
*
* \brief 选股策略管理器头文件
*
* 文件设计逻辑与作用总结：
* 本文件定义了选股策略管理器类SelStrategyMgr和策略包装器类SelStraWrapper。
* SelStrategyMgr负责管理选股策略工厂和策略实例，使用工厂模式动态加载策略。
* SelStraWrapper是一个RAII包装器，确保策略实例通过工厂正确释放。
* 
* 主要功能：
* 1. 策略工厂管理：从动态库加载策略工厂，管理工厂的生命周期。
* 2. 策略实例创建：通过工厂创建策略实例，使用包装器管理生命周期。
* 3. 策略实例管理：维护策略实例映射表，提供查询功能。
* 4. 资源管理：使用RAII模式确保资源正确释放。
*/
#pragma once  // 防止头文件重复包含
#include <memory>  // 包含智能指针头文件
#include <boost/core/noncopyable.hpp>  // 包含Boost的noncopyable类，用于禁止拷贝构造和赋值

#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap
#include "../Includes/SelStrategyDefs.h"  // 包含选股策略定义头文件

#include "../Share/DLLHelper.hpp"  // 包含动态库辅助工具类

/**
 * @class SelStraWrapper
 * @brief 选股策略包装器类
 *
 * 该类是一个RAII（Resource Acquisition Is Initialization）包装器，
 * 用于管理策略实例的生命周期。当包装器对象销毁时，会自动通过工厂删除策略实例。
 */
class SelStraWrapper
{
public:
	/**
	 * @brief 构造函数
	 * @param stra 策略实例指针
	 * @param fact 策略工厂指针
	 *
	 * 初始化包装器，保存策略实例和工厂指针。
	 */
	SelStraWrapper(SelStrategy* stra, ISelStrategyFact* fact) :_stra(stra), _fact(fact){}  // 构造函数，初始化策略实例和工厂指针
	/**
	 * @brief 析构函数
	 *
	 * 当包装器对象销毁时，通过工厂删除策略实例。
	 */
	~SelStraWrapper()  // 析构函数
	{
		if (_stra)  // 如果策略实例存在
		{
			_fact->deleteStrategy(_stra);  // 通过工厂删除策略实例
		}
	}

	/**
	 * @brief 获取策略实例
	 * @return SelStrategy* 返回策略实例指针
	 *
	 * 返回包装的策略实例指针。
	 */
	SelStrategy* self(){ return _stra; }  // 获取策略实例


private:
	SelStrategy*		_stra;  // 策略实例指针
	ISelStrategyFact*	_fact;  // 策略工厂指针
};
typedef std::shared_ptr<SelStraWrapper>	SelStrategyPtr;  // 定义SelStrategyPtr为SelStraWrapper的共享指针类型


/**
 * @class SelStrategyMgr
 * @brief 选股策略管理器类
 *
 * 该类负责管理选股策略工厂和策略实例。
 * 使用工厂模式动态加载策略，通过策略包装器管理策略实例的生命周期。
 * 使用boost::noncopyable继承，禁止对象的拷贝构造和赋值。
 */
class SelStrategyMgr : private boost::noncopyable  // 继承boost::noncopyable，禁止拷贝构造和赋值
{
public:
	/**
	 * @brief 构造函数
	 *
	 * 初始化选股策略管理器对象。
	 */
	SelStrategyMgr();  // 构造函数
	/**
	 * @brief 析构函数
	 *
	 * 清理解股策略管理器对象。
	 */
	~SelStrategyMgr();  // 析构函数

public:
	/**
	 * @brief 加载策略工厂
	 * @param path 工厂动态库所在目录路径
	 * @return bool 加载成功返回true，失败返回false
	 *
	 * 从指定目录加载所有策略工厂动态库（DLL/SO），
	 * 创建工厂实例并保存到工厂映射表中。
	 */
	bool loadFactories(const char* path);  // 加载策略工厂

	/**
	 * @brief 创建策略实例（通过名称）
	 * @param name 策略名称，格式为"工厂名.单元名"
	 * @param id 策略ID
	 * @return SelStrategyPtr 返回策略实例的共享指针，如果创建失败则返回空指针
	 *
	 * 根据策略名称创建策略实例，名称格式为"工厂名.单元名"。
	 * 会自动解析名称，提取工厂名和单元名。
	 */
	SelStrategyPtr createStrategy(const char* name, const char* id);  // 创建策略实例（通过名称）
	/**
	 * @brief 创建策略实例（通过工厂名和单元名）
	 * @param factname 工厂名称
	 * @param unitname 单元名称
	 * @param id 策略ID
	 * @return SelStrategyPtr 返回策略实例的共享指针，如果创建失败则返回空指针
	 *
	 * 根据工厂名和单元名直接创建策略实例。
	 */
	SelStrategyPtr createStrategy(const char* factname, const char* unitname, const char* id);  // 创建策略实例（通过工厂名和单元名）

	/**
	 * @brief 获取策略实例
	 * @param id 策略ID
	 * @return SelStrategyPtr 返回策略实例的共享指针，如果未找到则返回空指针
	 *
	 * 根据策略ID从策略映射表中获取对应的策略实例。
	 */
	SelStrategyPtr getStrategy(const char* id);  // 获取策略实例
private:
	/**
	 * @struct StraFactInfo
	 * @brief 策略工厂信息结构体
	 *
	 * 存储策略工厂的相关信息，包括模块路径、模块句柄、工厂实例、创建函数和删除函数。
	 */
	typedef struct _StraFactInfo
	{
		std::string			_module_path;  // 模块路径（动态库文件路径）
		DllHandle			_module_inst;  // 模块句柄（动态库句柄）
		ISelStrategyFact*	_fact;  // 策略工厂指针
		FuncCreateSelStraFact	_creator;  // 创建工厂的函数指针
		FuncDeleteSelStraFact	_remover;  // 删除工厂的函数指针
	} StraFactInfo;  // 策略工厂信息类型
	typedef wt_hashmap<std::string, StraFactInfo> StraFactMap;  // 策略工厂映射表类型，键为工厂名称，值为工厂信息
	StraFactMap	_factories;  // 策略工厂映射表

	typedef wt_hashmap<std::string, SelStrategyPtr> StrategyMap;  // 策略映射表类型，键为策略ID，值为策略实例指针
	StrategyMap	_strategies;  // 策略映射表
};

