/*!
 * \file UftStrategyMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT策略管理器实现文件
 *
 * 本文件实现了UftStrategyMgr类，用于管理UFT策略工厂和策略实例。
 *
 * 设计逻辑：
 * 1. 动态加载：通过DLLHelper动态加载DLL/SO文件，获取策略工厂创建和删除函数
 * 2. 工厂注册：加载成功后，将工厂注册到工厂映射表中，key为工厂名称
 * 3. 策略创建：通过工厂接口创建策略实例，并用智能指针包装
 * 4. 生命周期管理：通过智能指针和工厂析构函数自动管理策略和工厂的生命周期
 * 5. 跨平台支持：支持Windows（.dll）和Unix（.so）平台的动态库加载
 *
 * 主要功能：
 * - 实现策略工厂的加载和注册
 * - 实现策略实例的创建和管理
 * - 实现策略实例的查找
 * - 实现资源的自动释放
 */
#include "UftStrategyMgr.h"  // UFT策略管理器头文件

#include <boost/filesystem.hpp>  // Boost文件系统库

#include "../Share/StrUtil.hpp"  // 字符串工具头文件
#include "../Share/StdUtils.hpp"  // 标准工具头文件

#include "../WTSTools/WTSLogger.h"  // 日志工具头文件


/**
 * @brief 构造函数实现
 * 
 * 创建策略管理器对象，初始化工厂映射表和策略映射表。
 */
UftStrategyMgr::UftStrategyMgr()
{
}


/**
 * @brief 析构函数实现
 * 
 * 销毁策略管理器对象，智能指针会自动释放策略实例和工厂资源。
 */
UftStrategyMgr::~UftStrategyMgr()
{
}

/**
 * @brief 加载策略工厂实现
 * @param path 策略工厂目录路径
 * @return 是否成功
 * 
 * 从指定目录加载所有策略工厂插件。
 * 遍历目录中的所有DLL/SO文件，动态加载并注册策略工厂。
 * 
 * 加载流程：
 * 1. 检查目录是否存在
 * 2. 遍历目录中的所有文件
 * 3. 过滤出DLL/SO文件（Windows为.dll，Unix为.so）
 * 4. 加载动态库
 * 5. 获取createStrategyFact和deleteStrategyFact函数
 * 6. 创建工厂实例并注册到映射表
 */
bool UftStrategyMgr::loadFactories(const char* path)
{
	if (!StdFile::exists(path))  // 如果目录不存在
	{
		WTSLogger::error("Directory {} of UFT strategy factory not exists", path);  // 记录错误日志
		return false;  // 返回失败
	}

	uint32_t count = 0;  // 成功加载的工厂数量
	boost::filesystem::path myPath(path);  // 创建路径对象
	boost::filesystem::directory_iterator endIter;  // 目录迭代器结束标志
	// 遍历目录中的所有文件
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)
	{
		if (boost::filesystem::is_directory(iter->path()))  // 如果是目录，跳过
			continue;

#ifdef _WIN32  // Windows平台
		if (iter->path().extension() != ".dll")  // 如果扩展名不是.dll，跳过
			continue;
#else //_UNIX  // Unix平台
		if (iter->path().extension() != ".so")  // 如果扩展名不是.so，跳过
			continue;
#endif

		DllHandle hInst = DLLHelper::load_library(iter->path().string().c_str());  // 加载动态库
		if (hInst == NULL)  // 如果加载失败，跳过
			continue;

		FuncCreateUftStraFact creator = (FuncCreateUftStraFact)DLLHelper::get_symbol(hInst, "createStrategyFact");  // 获取创建工厂函数
		if (creator == NULL)  // 如果获取失败
		{
			DLLHelper::free_library(hInst);  // 释放动态库
			continue;  // 跳过该文件
		}
		
		IUftStrategyFact* pFact = creator();  // 调用创建函数创建工厂实例
		if (pFact != NULL)  // 如果创建成功
		{
			StraFactInfo& fInfo = _factories[pFact->getName()];  // 获取或创建工厂信息（key为工厂名称）

			fInfo._module_inst = hInst;  // 保存动态库句柄
			fInfo._module_path = iter->path().string();  // 保存模块文件路径
			fInfo._creator = creator;  // 保存创建函数指针
			fInfo._remover = (FuncDeleteUftStraFact)DLLHelper::get_symbol(hInst, "deleteStrategyFact");  // 获取并保存删除函数指针
			fInfo._fact = pFact;  // 保存工厂实例指针
			WTSLogger::info("UFT strategy factory[{}] loaded", pFact->getName());  // 记录加载成功日志

			count++;  // 增加成功计数
		}
		else  // 如果创建失败
		{
			DLLHelper::free_library(hInst);  // 释放动态库
			continue;  // 跳过该文件
		}
	}

	WTSLogger::info("{} UFT strategy factories in directory[{}] loaded", count, path);  // 记录加载完成日志

	return true;  // 返回成功
}

/**
 * @brief 创建策略实例实现（完整版本）
 * @param factname 工厂名称
 * @param unitname 策略名称
 * @param id 策略ID
 * @return 策略智能指针，失败返回空指针
 * 
 * 根据工厂名和策略名创建策略实例。
 * 策略ID用于在策略映射表中唯一标识策略实例。
 */
UftStrategyPtr UftStrategyMgr::createStrategy(const char* factname, const char* unitname, const char* id)
{
	auto it = _factories.find(factname);  // 查找工厂
	if (it == _factories.end())  // 如果工厂不存在
		return UftStrategyPtr();  // 返回空指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息
	UftStrategyPtr ret(new UftStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例并包装
	_strategies[id] = ret;  // 将策略实例添加到映射表（key为策略ID）
	return ret;  // 返回策略智能指针
}

/**
 * @brief 创建策略实例实现（简化版本）
 * @param name 策略名称，格式为"工厂名.策略名"
 * @param id 策略ID
 * @return 策略智能指针，失败返回空指针
 * 
 * 根据策略名称（格式：工厂名.策略名）创建策略实例。
 * 内部会解析策略名称，提取工厂名和策略名。
 */
UftStrategyPtr UftStrategyMgr::createStrategy(const char* name, const char* id)
{
	StringVector ay = StrUtil::split(name, ".");  // 按"."分割策略名称
	if (ay.size() < 2)  // 如果分割后少于2部分，说明格式不正确
		return UftStrategyPtr();  // 返回空指针

	const char* factname = ay[0].c_str();  // 第一部分为工厂名
	const char* unitname = ay[1].c_str();  // 第二部分为策略名

	auto it = _factories.find(factname);  // 查找工厂
	if (it == _factories.end())  // 如果工厂不存在
		return UftStrategyPtr();  // 返回空指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息
	UftStrategyPtr ret(new UftStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例并包装
	_strategies[id] = ret;  // 将策略实例添加到映射表（key为策略ID）
	return ret;  // 返回策略智能指针
}

/**
 * @brief 获取策略实例实现
 * @param id 策略ID
 * @return 策略智能指针，不存在返回空指针
 * 
 * 根据策略ID查找并返回策略实例。
 */
UftStrategyPtr UftStrategyMgr::getStrategy(const char* id)
{
	auto it = _strategies.find(id);  // 查找策略
	if (it == _strategies.end())  // 如果策略不存在
		return UftStrategyPtr();  // 返回空指针

	return it->second;  // 返回策略智能指针
}