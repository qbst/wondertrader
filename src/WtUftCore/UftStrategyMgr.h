/*!
 * \file UftStrategyMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT策略管理器头文件
 *
 * 本文件定义了UftStrategyMgr类和UftStraWrapper类，用于管理UFT策略工厂和策略实例。
 *
 * 设计逻辑：
 * 1. 策略工厂管理：通过动态加载DLL/SO文件，加载策略工厂插件，支持运行时扩展
 * 2. 策略实例管理：使用智能指针管理策略实例生命周期，确保资源正确释放
 * 3. 工厂模式：通过工厂接口创建策略实例，实现策略与具体实现的解耦
 * 4. 命名规范：策略名称格式为"工厂名.策略名"，支持多工厂多策略的管理
 * 5. 单例模式：UftStrategyMgr禁止拷贝，通常作为单例使用
 *
 * 主要功能：
 * - 加载策略工厂：从指定目录加载所有策略工厂插件
 * - 创建策略实例：根据工厂名和策略名创建策略实例
 * - 管理策略实例：维护策略实例映射表，支持按ID查找策略
 * - 自动资源管理：通过智能指针自动管理策略实例和工厂的生命周期
 */
#pragma once
#include <memory>  // 智能指针头文件
#include <boost/core/noncopyable.hpp>  // Boost不可拷贝基类

#include "../Includes/FasterDefs.h"  // Faster库定义
#include "../Includes/UftStrategyDefs.h"  // UFT策略定义头文件

#include "../Share/DLLHelper.hpp"  // DLL动态加载辅助工具头文件

/**
 * @class UftStraWrapper
 * @brief UFT策略包装器类
 * 
 * 包装UFT策略实例和策略工厂，使用智能指针管理策略生命周期。
 * 析构时自动调用工厂的deleteStrategy方法释放策略资源。
 */
class UftStraWrapper
{
public:
	/**
	 * @brief 构造函数
	 * @param stra 策略实例指针
	 * @param fact 策略工厂指针
	 * 
	 * 创建策略包装器，保存策略实例和工厂指针。
	 */
	UftStraWrapper(UftStrategy* stra, IUftStrategyFact* fact) :_stra(stra), _fact(fact){}  // 初始化策略实例和工厂指针
	
	/**
	 * @brief 析构函数
	 * 
	 * 析构时自动调用工厂的deleteStrategy方法释放策略资源。
	 */
	~UftStraWrapper()
	{
		if (_stra)  // 如果策略实例存在
		{
			_fact->deleteStrategy(_stra);  // 通过工厂删除策略实例
		}
	}

	/**
	 * @brief 获取策略实例指针
	 * @return 策略实例指针
	 * 
	 * 返回包装的策略实例指针。
	 */
	UftStrategy* self(){ return _stra; }  // 返回策略实例指针


private:
	UftStrategy*		_stra;  // 策略实例指针
	IUftStrategyFact*	_fact;  // 策略工厂指针
};

typedef std::shared_ptr<UftStraWrapper>	UftStrategyPtr;  // UFT策略智能指针类型别名

/**
 * @class UftStrategyMgr
 * @brief UFT策略管理器类
 * 
 * 管理UFT策略工厂和策略实例，负责策略的动态加载和生命周期管理。
 * 继承自boost::noncopyable，禁止拷贝构造和赋值，通常作为单例使用。
 */
class UftStrategyMgr : private boost::noncopyable
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建策略管理器对象。
	 */
	UftStrategyMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 销毁策略管理器对象，清理所有策略工厂和策略实例。
	 */
	~UftStrategyMgr();

public:
	/**
	 * @brief 加载策略工厂
	 * @param path 策略工厂目录路径
	 * @return 是否成功
	 * 
	 * 从指定目录加载所有策略工厂插件（.dll或.so文件）。
	 * 遍历目录中的所有DLL/SO文件，加载并注册策略工厂。
	 */
	bool loadFactories(const char* path);

	/**
	 * @brief 创建策略实例（简化版本）
	 * @param name 策略名称，格式为"工厂名.策略名"
	 * @param id 策略ID
	 * @return 策略智能指针，失败返回空指针
	 * 
	 * 根据策略名称（格式：工厂名.策略名）创建策略实例。
	 * 内部会解析策略名称，提取工厂名和策略名。
	 */
	UftStrategyPtr createStrategy(const char* name, const char* id);
	
	/**
	 * @brief 创建策略实例（完整版本）
	 * @param factname 工厂名称
	 * @param unitname 策略名称
	 * @param id 策略ID
	 * @return 策略智能指针，失败返回空指针
	 * 
	 * 根据工厂名和策略名创建策略实例。
	 * 策略ID用于在策略映射表中唯一标识策略实例。
	 */
	UftStrategyPtr createStrategy(const char* factname, const char* unitname, const char* id);

	/**
	 * @brief 获取策略实例
	 * @param id 策略ID
	 * @return 策略智能指针，不存在返回空指针
	 * 
	 * 根据策略ID查找并返回策略实例。
	 */
	UftStrategyPtr getStrategy(const char* id);

private:
	/**
	 * @struct StraFactInfo
	 * @brief 策略工厂信息结构体
	 * 
	 * 存储策略工厂的相关信息，包括模块路径、句柄、工厂实例和函数指针。
	 */
	typedef struct _StraFactInfo
	{
		std::string		_module_path;  // 模块文件路径（DLL/SO文件路径）
		DllHandle		_module_inst;  // 动态库句柄
		IUftStrategyFact*	_fact;  // 策略工厂实例指针
		FuncCreateUftStraFact	_creator;  // 创建工厂函数指针
		FuncDeleteUftStraFact	_remover;  // 删除工厂函数指针

		/**
		 * @brief 构造函数
		 * 
		 * 初始化策略工厂信息结构体，将指针和句柄置为NULL。
		 */
		_StraFactInfo()
		{
			_module_inst = NULL;  // 初始化为空
			_fact = NULL;  // 初始化为空
		}

		/**
		 * @brief 析构函数
		 * 
		 * 析构时自动调用删除函数释放工厂资源。
		 */
		~_StraFactInfo()
		{
			if (_fact)  // 如果工厂实例存在
				_remover(_fact);  // 调用删除函数释放工厂
		}
	} StraFactInfo;  // 策略工厂信息类型别名
	typedef wt_hashmap<std::string, StraFactInfo> StraFactMap;  // 策略工厂映射表类型别名

	StraFactMap	_factories;  // 策略工厂映射表，key为工厂名称

	typedef wt_hashmap<std::string, UftStrategyPtr> StrategyMap;  // 策略映射表类型别名
	StrategyMap	_strategies;  // 策略实例映射表，key为策略ID
};

