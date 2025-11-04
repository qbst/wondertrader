/*!
 * \file HftStrategyMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易策略管理器实现文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件实现了高频交易策略管理器HftStrategyMgr类的所有功能。
 * 主要包括：
 * 1. 从指定目录动态加载高频交易策略工厂（DLL/SO文件）。
 * 2. 通过加载的工厂创建和管理高频交易策略实例。
 * 3. 提供根据策略ID获取已创建策略实例的功能。
 * 4. 利用Boost.Filesystem库进行文件系统操作，如目录遍历和文件存在性检查。
 * 5. 使用DLLHelper进行动态库的加载和符号查找。
 * 6. 集成WTSLogger进行日志记录，方便调试和运行监控。
 */
#include "HftStrategyMgr.h"  // 包含高频交易策略管理器头文件

#include <boost/filesystem.hpp>  // 包含Boost文件系统库，用于目录遍历和文件操作

#include "../Share/StrUtil.hpp"  // 包含字符串工具类，用于字符串分割
#include "../Share/StdUtils.hpp"  // 包含标准工具类，用于文件操作

#include "../WTSTools/WTSLogger.h"  // 包含日志记录器


/**
 * @brief 构造函数实现
 *
 * 初始化HftStrategyMgr对象。
 */
HftStrategyMgr::HftStrategyMgr()
{
}


/**
 * @brief 析构函数实现
 *
 * 销毁HftStrategyMgr对象。
 * 由于策略实例由HftStraWrapper的共享指针管理，并在其析构时通过工厂删除，
 * 因此这里不需要显式删除策略实例。
 * 工厂实例的清理在_factories的析构中自动完成。
 */
HftStrategyMgr::~HftStrategyMgr()
{
}

/**
 * @brief 加载策略工厂实现
 * @param path 策略工厂动态库所在的目录路径
 * @return bool 加载成功返回true，失败返回false
 *
 * 遍历指定路径下的动态库文件（.dll或.so），尝试加载为高频交易策略工厂。
 * 成功加载的工厂会被存储在_factories映射表中。
 * 1. 检查目录是否存在。
 * 2. 遍历目录下的文件，筛选出动态库文件。
 * 3. 加载动态库，获取创建和删除策略工厂的函数指针。
 * 4. 创建策略工厂实例，并将其信息存储在_factories中。
 * 5. 记录加载日志。
 */
bool HftStrategyMgr::loadFactories(const char* path)
{
	if (!StdFile::exists(path))  // 检查目录是否存在
	{
		WTSLogger::error("Directory {} of HFT strategy factory not exists", path);  // 记录错误日志
		return false;  // 目录不存在，返回false
	}

	uint32_t count = 0;  // 成功加载的工厂数量计数器
	boost::filesystem::path myPath(path);  // 创建Boost文件系统路径对象
	boost::filesystem::directory_iterator endIter;  // 目录迭代器结束标志
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历目录
	{
		if (boost::filesystem::is_directory(iter->path()))  // 如果是子目录
			continue;  // 跳过

#ifdef _WIN32  // Windows平台
		if (iter->path().extension() != ".dll")  // 如果文件扩展名不是.dll
			continue;  // 跳过
#else //_UNIX  // Unix/Linux平台
		if (iter->path().extension() != ".so")  // 如果文件扩展名不是.so
			continue;  // 跳过
#endif

		DllHandle hInst = DLLHelper::load_library(iter->path().string().c_str());  // 加载动态库
		if (hInst == NULL)  // 如果加载失败
			continue;  // 跳过

		FuncCreateHftStraFact creator = (FuncCreateHftStraFact)DLLHelper::get_symbol(hInst, "createStrategyFact");  // 获取创建策略工厂的函数指针
		if (creator == NULL)  // 如果函数指针为空
		{
			DLLHelper::free_library(hInst);  // 释放动态库
			continue;  // 跳过
		}
		
		IHftStrategyFact* pFact = creator();  // 调用创建函数，创建策略工厂实例
		if (pFact != NULL)  // 如果工厂实例创建成功
		{
			StraFactInfo& fInfo = _factories[pFact->getName()];  // 获取或创建工厂信息

			fInfo._module_inst = hInst;  // 保存模块实例句柄
			fInfo._module_path = iter->path().string();  // 保存模块路径
			fInfo._creator = creator;  // 保存创建函数指针
			fInfo._remover = (FuncDeleteHftStraFact)DLLHelper::get_symbol(hInst, "deleteStrategyFact");  // 获取删除策略工厂的函数指针
			fInfo._fact = pFact;  // 保存工厂接口指针
			WTSLogger::info("HFT strategy factory[{}] loaded", pFact->getName());  // 记录信息日志：工厂加载成功

			count++;  // 成功加载的工厂数量加1
		}
		else  // 如果工厂实例创建失败
		{
			DLLHelper::free_library(hInst);  // 释放动态库
			continue;  // 跳过
		}
	}

	WTSLogger::info("{} HFT strategy factories in directory[{}] loaded", count, path);  // 记录信息日志：总共加载的工厂数量

	return true;  // 返回true，表示加载完成
}

/**
 * @brief 创建高频交易策略实例实现（重载版本）
 * @param factname 策略工厂名称
 * @param unitname 策略单元名称
 * @param id 策略实例的唯一ID
 * @return HftStrategyPtr 返回创建的高频交易策略的共享指针，如果失败则返回空指针
 *
 * 根据指定的工厂名称、单元名称和策略ID创建高频交易策略实例。
 * 1. 根据工厂名称查找对应的策略工厂。
 * 2. 如果找到，通过工厂创建策略实例，并用HftStraWrapper封装。
 * 3. 将创建的策略实例存储在_strategies映射表中。
 */
HftStrategyPtr HftStrategyMgr::createStrategy(const char* factname, const char* unitname, const char* id)
{
	auto it = _factories.find(factname);  // 根据工厂名称查找策略工厂
	if (it == _factories.end())  // 如果未找到
		return HftStrategyPtr();  // 返回空指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息
	HftStrategyPtr ret(new HftStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例，并用HftStraWrapper封装
	_strategies[id] = ret;  // 将策略实例存储在映射表中
	return ret;  // 返回策略实例的共享指针
}

/**
 * @brief 创建高频交易策略实例实现
 * @param name 策略名称，格式为"工厂名.单元名"
 * @param id 策略实例的唯一ID
 * @return HftStrategyPtr 返回创建的高频交易策略的共享指针，如果失败则返回空指针
 *
 * 根据策略名称（格式为"工厂名.单元名"）和策略ID创建高频交易策略实例。
 * 1. 解析策略名称，提取工厂名和单元名。
 * 2. 根据工厂名称查找对应的策略工厂。
 * 3. 如果找到，通过工厂创建策略实例，并用HftStraWrapper封装。
 * 4. 将创建的策略实例存储在_strategies映射表中。
 */
HftStrategyPtr HftStrategyMgr::createStrategy(const char* name, const char* id)
{
	StringVector ay = StrUtil::split(name, ".");  // 分割策略名称，获取工厂名和单元名
	if (ay.size() < 2)  // 如果分割后的部分少于2个
		return HftStrategyPtr();  // 返回空指针

	const char* factname = ay[0].c_str();  // 获取工厂名称
	const char* unitname = ay[1].c_str();  // 获取单元名称

	auto it = _factories.find(factname);  // 根据工厂名称查找策略工厂
	if (it == _factories.end())  // 如果未找到
		return HftStrategyPtr();  // 返回空指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息
	HftStrategyPtr ret(new HftStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例，并用HftStraWrapper封装
	_strategies[id] = ret;  // 将策略实例存储在映射表中
	return ret;  // 返回策略实例的共享指针
}

/**
 * @brief 获取高频交易策略实例实现
 * @param id 策略实例的唯一ID
 * @return HftStrategyPtr 返回对应ID的高频交易策略的共享指针，如果未找到则返回空指针
 *
 * 根据策略ID从_strategies映射表中获取已创建的高频交易策略实例。
 */
HftStrategyPtr HftStrategyMgr::getStrategy(const char* id)
{
	auto it = _strategies.find(id);  // 根据策略ID查找策略实例
	if (it == _strategies.end())  // 如果未找到
		return HftStrategyPtr();  // 返回空指针

	return it->second;  // 返回策略实例的共享指针
}