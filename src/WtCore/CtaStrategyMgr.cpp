/*!
 * \file CtaStrategyMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTA策略管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了CTA策略管理器的所有功能，包括：
 * 1. 策略工厂的动态加载：扫描指定目录，加载所有策略工厂DLL/动态库
 * 2. 策略实例的创建：通过工厂对象创建策略实例，并管理其生命周期
 * 3. 策略实例的查询：根据ID查找已创建的策略实例
 * 
 * 关键实现细节：
 * - 使用boost::filesystem遍历目录，查找DLL/动态库文件
 * - 通过DLLHelper动态加载库并获取函数符号
 * - 使用智能指针管理策略对象，确保资源正确释放
 * - 支持Windows(.dll)和Linux(.so)平台的动态库加载
 */
#include "CtaStrategyMgr.h"  // 包含策略管理器头文件

#include "../Share/StrUtil.hpp"  // 包含字符串工具类
#include "../Share/StdUtils.hpp"  // 包含标准工具类

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

#include <boost/filesystem.hpp>  // 包含boost文件系统库，用于目录遍历


/**
 * @brief 构造函数实现
 * 
 * 初始化策略管理器对象，创建空的工厂映射表和策略映射表。
 */
CtaStrategyMgr::CtaStrategyMgr()
{  // 构造函数实现，初始化成员变量
}


/**
 * @brief 析构函数实现
 * 
 * 清理策略管理器对象。
 * 由于使用智能指针管理策略，会自动释放策略对象。
 * 动态库句柄在工厂信息中保存，但未在此处显式释放。
 */
CtaStrategyMgr::~CtaStrategyMgr()
{  // 析构函数实现，清理资源
}

/**
 * @brief 加载策略工厂实现
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
bool CtaStrategyMgr::loadFactories(const char* path)
{
	if (!StdFile::exists(path))  // 检查目录路径是否存在
	{
		WTSLogger::error("Directory {} of CTA strategy factory not exists", path);  // 记录错误日志：目录不存在
		return false;  // 返回false表示加载失败
	}

	uint32_t count = 0;  // 成功加载的工厂数量计数器
	boost::filesystem::path myPath(path);  // 创建boost文件系统路径对象
	boost::filesystem::directory_iterator endIter;  // 目录迭代器结束标志
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历目录中的所有文件
	{
		if (boost::filesystem::is_directory(iter->path()))  // 如果当前项是目录，则跳过
			continue;  // 跳过目录，只处理文件

#ifdef _WIN32  // Windows平台编译条件
		if (iter->path().extension() != ".dll")  // 如果文件扩展名不是.dll，则跳过
			continue;  // 跳过非DLL文件
#else //_UNIX  // Unix/Linux平台编译条件
		if (iter->path().extension() != ".so")  // 如果文件扩展名不是.so，则跳过
			continue;  // 跳过非SO文件
#endif

		DllHandle hInst = DLLHelper::load_library(iter->path().string().c_str());  // 加载动态库，获取库句柄
		if (hInst == NULL)  // 如果加载失败，库句柄为空
			continue;  // 跳过加载失败的库

		FuncCreateStraFact creator = (FuncCreateStraFact)DLLHelper::get_symbol(hInst, "createStrategyFact");  // 获取创建工厂的函数指针
		if (creator == NULL)  // 如果获取创建函数失败
		{
			DLLHelper::free_library(hInst);  // 释放已加载的库
			continue;  // 跳过该库
		}

		ICtaStrategyFact* fact = creator();  // 调用创建函数创建工厂实例
		if(fact != NULL)  // 如果工厂创建成功
		{
			StraFactInfo& fInfo = _factories[fact->getName()];  // 获取或创建工厂信息结构体（以工厂名称为键）
			fInfo._module_inst = hInst;  // 保存动态库句柄
			fInfo._module_path = iter->path().string();  // 保存动态库文件路径
			fInfo._creator = creator;  // 保存创建工厂的函数指针
			fInfo._remover = (FuncDeleteStraFact)DLLHelper::get_symbol(hInst, "deleteStrategyFact");  // 获取并保存删除工厂的函数指针
			fInfo._fact = fact;  // 保存工厂对象指针

			WTSLogger::info("CTA strategy factory[{}] loaded", fact->getName());  // 记录信息日志：工厂加载成功

			count++;  // 成功加载计数器加1
		}
		else  // 如果工厂创建失败
		{
			DLLHelper::free_library(hInst);  // 释放已加载的库
			continue;  // 跳过该库
		}
		
	}

	WTSLogger::info("{} CTA strategy factories in directory[{}] loaded", count, path);  // 记录信息日志：加载完成，显示加载的工厂数量

	return true;  // 返回true表示加载成功
}

/**
 * @brief 创建策略实例实现（通过工厂名和策略名）
 * @param factname 策略工厂名称
 * @param unitname 策略单元名称
 * @param id 策略实例的唯一标识ID
 * @return CtaStrategyPtr 返回策略智能指针，失败返回空指针
 * 
 * 根据工厂名和策略名创建策略实例。
 * 创建成功后，策略实例会被保存到策略映射表中，可通过ID查询。
 */
CtaStrategyPtr CtaStrategyMgr::createStrategy(const char* factname, const char* unitname, const char* id)
{
	auto it = _factories.find(factname);  // 在工厂映射表中查找指定的工厂名称
	if (it == _factories.end())  // 如果工厂不存在
		return CtaStrategyPtr();  // 返回空智能指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息结构体的引用
	CtaStrategyPtr ret(new CtaStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例，并用包装类包装
	_strategies[id] = ret;  // 将策略实例保存到策略映射表中，以ID为键
	return ret;  // 返回策略智能指针
}

/**
 * @brief 创建策略实例实现（通过策略名称）
 * @param name 策略名称，格式为"工厂名.策略名"
 * @param id 策略实例的唯一标识ID
 * @return CtaStrategyPtr 返回策略智能指针，失败返回空指针
 * 
 * 根据策略名称创建策略实例。
 * 策略名称格式：工厂名.策略名（例如："WtStraFact.MAStrategy"）
 * 创建成功后，策略实例会被保存到策略映射表中，可通过ID查询。
 */
CtaStrategyPtr CtaStrategyMgr::createStrategy(const char* name, const char* id)
{
	StringVector ay = StrUtil::split(name, ".");  // 将策略名称按"."分割成字符串数组
	if (ay.size() < 2)  // 如果分割后的数组长度小于2，说明格式不正确
		return CtaStrategyPtr();  // 返回空智能指针

	const char* factname = ay[0].c_str();  // 获取第一个元素作为工厂名称
	const char* unitname = ay[1].c_str();  // 获取第二个元素作为策略单元名称

	auto it = _factories.find(factname);  // 在工厂映射表中查找指定的工厂名称
	if (it == _factories.end())  // 如果工厂不存在
		return CtaStrategyPtr();  // 返回空智能指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息结构体的引用
	CtaStrategyPtr ret(new CtaStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例，并用包装类包装
	_strategies[id] = ret;  // 将策略实例保存到策略映射表中，以ID为键
	return ret;  // 返回策略智能指针
}

/**
 * @brief 获取策略实例实现
 * @param id 策略实例的唯一标识ID
 * @return CtaStrategyPtr 返回策略智能指针，不存在返回空指针
 * 
 * 根据策略ID从映射表中查找并返回策略实例。
 * 如果策略不存在，返回空指针。
 */
CtaStrategyPtr CtaStrategyMgr::getStrategy(const char* id)
{
	auto it = _strategies.find(id);  // 在策略映射表中查找指定的策略ID
	if (it == _strategies.end())  // 如果策略不存在
		return CtaStrategyPtr();  // 返回空智能指针

	return it->second;  // 返回找到的策略智能指针
}

