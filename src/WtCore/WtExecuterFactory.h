/*!
 * \file WtExecuterFactory.h
 * \project	WonderTrader
 * 
 * \brief 执行器工厂类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的执行器工厂类，负责动态加载和管理执行器工厂插件。
 * 主要功能包括：
 * 1. 执行单元封装：提供ExeUnitWrapper类，封装执行单元对象，确保正确的内存管理
 * 2. 工厂管理：动态加载DLL/SO插件中的执行器工厂，并管理工厂的生命周期
 * 3. 执行单元创建：提供多种方法创建不同类型的执行单元（普通、差量、套利）
 * 4. 资源管理：通过智能指针和RAII模式确保执行单元的正确释放
 * 
 * 设计模式：
 * - 工厂模式：通过工厂接口创建不同类型的执行单元
 * - 包装器模式：ExeUnitWrapper封装执行单元，确保正确的释放逻辑
 * - 插件模式：支持动态加载外部DLL/SO插件中的执行器工厂
 * 
 * 使用场景：
 * 该工厂类主要用于交易执行系统中，为策略提供不同类型的执行单元，
 * 支持策略执行的插件化扩展和动态加载。
 */
#pragma once  // 防止头文件重复包含
#include "IExecCommand.h"  // 包含执行命令接口头文件
#include "../Includes/ExecuteDefs.h"  // 包含执行单元相关定义
#include "../Share/DLLHelper.hpp"  // 包含动态库加载辅助工具

#include <boost/core/noncopyable.hpp>  // 包含boost的非拷贝基类，用于禁止拷贝构造和赋值

NS_WTP_BEGIN  // 开始WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
//执行单元封装类
/**
 * @class ExeUnitWrapper
 * @brief 执行单元包装器类
 * 
 * 该类封装了执行单元对象，解决了执行单元在DLL中创建时的内存管理问题。
 * 由于执行单元对象是在DLL中创建的，直接使用delete删除可能会导致内存问题，
 * 因此需要通过工厂实例的deleteExeUnit方法来正确释放执行单元。
 * 
 * 设计原理：
 * - 存储执行单元指针和工厂指针
 * - 析构函数中调用工厂的deleteExeUnit方法释放执行单元
 * - 使用智能指针管理生命周期，确保自动释放
 */
class ExeUnitWrapper
{
public:
	/**
	 * @brief 构造函数
	 * @param unitPtr 执行单元对象指针
	 * @param fact 执行器工厂接口指针
	 * 
	 * 初始化包装器，保存执行单元指针和工厂指针，用于后续的释放操作。
	 */
	ExeUnitWrapper(ExecuteUnit* unitPtr, IExecuterFact* fact) :_unit(unitPtr), _fact(fact) {}  // 初始化执行单元指针和工厂指针
	
	/**
	 * @brief 析构函数
	 * 
	 * 析构时检查执行单元是否存在，如果存在则调用工厂的deleteExeUnit方法释放执行单元。
	 * 这样可以确保执行单元在正确的内存空间中释放，避免跨DLL内存管理问题。
	 */
	~ExeUnitWrapper()
	{
		if (_unit)  // 检查执行单元指针是否有效
		{
			_fact->deleteExeUnit(_unit);  // 调用工厂的删除方法释放执行单元
		}
	}

	/**
	 * @brief 获取执行单元指针
	 * @return ExecuteUnit* 返回封装的执行单元对象指针
	 * 
	 * 返回内部封装的执行单元指针，用于访问执行单元的功能。
	 */
	ExecuteUnit* self() { return _unit; }  // 返回执行单元指针


private:
	ExecuteUnit*	_unit;  // 执行单元对象指针，指向在DLL中创建的执行单元
	IExecuterFact*	_fact;  // 执行器工厂接口指针，用于释放执行单元
};

typedef std::shared_ptr<ExeUnitWrapper>	ExecuteUnitPtr;  // 执行单元智能指针类型别名，使用shared_ptr管理生命周期
typedef wt_hashmap<std::string, ExecuteUnitPtr> ExecuteUnitMap;  // 执行单元映射表类型别名，键为合约代码，值为执行单元智能指针

//////////////////////////////////////////////////////////////////////////
//执行器工厂类
/**
 * @class WtExecuterFactory
 * @brief 执行器工厂管理类
 * 
 * 该类负责加载和管理执行器工厂插件，并提供创建执行单元的统一接口。
 * 支持动态加载DLL/SO文件中的执行器工厂，并管理工厂的生命周期。
 * 提供多种创建执行单元的方法，支持普通执行单元、差量执行单元和套利执行单元。
 * 
 * 主要功能：
 * - 从指定目录加载执行器工厂插件
 * - 根据工厂名称和单元名称创建执行单元
 * - 支持简化的创建接口（使用"工厂名.单元名"格式）
 * - 管理工厂的生命周期，确保正确释放资源
 * 
 * 设计特点：
 * - 使用noncopyable禁止拷贝，确保工厂的唯一性
 * - 使用哈希表管理多个工厂实例
 * - 支持动态加载和卸载工厂插件
 */
class WtExecuterFactory : private boost::noncopyable  // 继承noncopyable，禁止拷贝构造和赋值操作
{
public:
	/**
	 * @brief 析构函数
	 * 
	 * 析构时清理所有已加载的工厂资源。由于使用了智能指针和DLL管理，
	 * 析构时会自动释放相关资源。
	 */
	~WtExecuterFactory() {}  // 析构函数，使用默认实现

public:
	/**
	 * @brief 加载执行器工厂
	 * @param path 工厂插件所在目录路径
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 从指定目录加载所有执行器工厂插件（.dll或.so文件），
	 * 查找并调用插件中的createExecFact函数创建工厂实例，
	 * 并将工厂注册到内部管理表中。
	 */
	bool loadFactories(const char* path);  // 加载执行器工厂插件

	/**
	 * @brief 创建执行单元（简化接口）
	 * @param name 执行单元名称，格式为"工厂名.单元名"
	 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
	 * 
	 * 根据"工厂名.单元名"格式的名称创建普通执行单元。
	 * 内部会解析名称，提取工厂名和单元名，然后调用对应的工厂创建执行单元。
	 */
	ExecuteUnitPtr createExeUnit(const char* name);  // 创建普通执行单元（简化接口）
	
	/**
	 * @brief 创建差量执行单元（简化接口）
	 * @param name 执行单元名称，格式为"工厂名.单元名"
	 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
	 * 
	 * 根据"工厂名.单元名"格式的名称创建差量执行单元。
	 * 差量执行单元用于处理差量交易逻辑。
	 */
	ExecuteUnitPtr createDiffExeUnit(const char* name);  // 创建差量执行单元（简化接口）
	
	/**
	 * @brief 创建套利执行单元（简化接口）
	 * @param name 执行单元名称，格式为"工厂名.单元名"
	 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
	 * 
	 * 根据"工厂名.单元名"格式的名称创建套利执行单元。
	 * 套利执行单元用于处理套利交易逻辑。
	 */
	ExecuteUnitPtr createArbiExeUnit(const char* name);  // 创建套利执行单元（简化接口）

	/**
	 * @brief 创建执行单元（完整接口）
	 * @param factname 工厂名称
	 * @param unitname 执行单元名称
	 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
	 * 
	 * 根据指定的工厂名称和单元名称创建普通执行单元。
	 * 如果工厂不存在或创建失败，返回空指针。
	 */
	ExecuteUnitPtr createExeUnit(const char* factname, const char* unitname);  // 创建普通执行单元（完整接口）
	
	/**
	 * @brief 创建差量执行单元（完整接口）
	 * @param factname 工厂名称
	 * @param unitname 执行单元名称
	 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
	 * 
	 * 根据指定的工厂名称和单元名称创建差量执行单元。
	 * 如果工厂不存在或创建失败，返回空指针。
	 */
	ExecuteUnitPtr createDiffExeUnit(const char* factname, const char* unitname);  // 创建差量执行单元（完整接口）
	
	/**
	 * @brief 创建套利执行单元（完整接口）
	 * @param factname 工厂名称
	 * @param unitname 执行单元名称
	 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
	 * 
	 * 根据指定的工厂名称和单元名称创建套利执行单元。
	 * 如果工厂不存在或创建失败，返回空指针。
	 */
	ExecuteUnitPtr createArbiExeUnit(const char* factname, const char* unitname);  // 创建套利执行单元（完整接口）

private:
	/**
	 * @struct _ExeFactInfo
	 * @brief 执行器工厂信息结构体
	 * 
	 * 该结构体存储执行器工厂的完整信息，包括模块路径、模块句柄、
	 * 工厂接口指针以及创建和删除函数指针。
	 */
	typedef struct _ExeFactInfo
	{
		std::string		_module_path;  // 工厂插件模块的文件路径
		DllHandle		_module_inst;  // 动态库模块句柄，用于后续卸载
		IExecuterFact*	_fact;  // 执行器工厂接口指针，用于创建执行单元
		FuncCreateExeFact	_creator;  // 创建工厂的函数指针，从DLL中获取
		FuncDeleteExeFact	_remover;  // 删除工厂的函数指针，从DLL中获取
	} ExeFactInfo;  // 执行器工厂信息结构体类型别名
	typedef wt_hashmap<std::string, ExeFactInfo> ExeFactMap;  // 工厂映射表类型别名，键为工厂名称，值为工厂信息

	ExeFactMap	_factories;  // 执行器工厂映射表，存储所有已加载的工厂信息
};


NS_WTP_END  // 结束WonderTrader命名空间
