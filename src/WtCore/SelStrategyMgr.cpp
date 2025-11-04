/*!
 * \file SelStrategyMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 选股策略管理器实现文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件实现选股策略管理器类SelStrategyMgr的所有功能。
 * 
 * 主要实现功能：
 * 1. 构造函数和析构函数的实现。
 * 2. 策略工厂加载：从指定目录扫描并加载所有策略工厂动态库。
 * 3. 策略实例创建：通过工厂创建策略实例，使用包装器管理生命周期。
 * 4. 策略实例查询：根据策略ID查询策略实例。
 */
#include "SelStrategyMgr.h"  // 包含选股策略管理器头文件

#include "../Share/StrUtil.hpp"  // 包含字符串工具类
#include "../Share/StdUtils.hpp"  // 包含标准工具类

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

#include <boost/filesystem.hpp>  // 包含Boost文件系统库


/**
 * @brief 构造函数实现
 *
 * 初始化选股策略管理器对象。
 */
SelStrategyMgr::SelStrategyMgr()
{
}


/**
 * @brief 析构函数实现
 *
 * 清理解股策略管理器对象。
 */
SelStrategyMgr::~SelStrategyMgr()
{
}

/**
 * @brief 加载策略工厂实现
 * @param path 工厂动态库所在目录路径
 * @return bool 加载成功返回true，失败返回false
 *
 * 从指定目录扫描并加载所有策略工厂动态库。
 * 处理流程：
 * 1. 检查目录是否存在。
 * 2. 遍历目录中的所有文件。
 * 3. 过滤出动态库文件（.dll或.so）。
 * 4. 加载动态库。
 * 5. 获取创建工厂的函数指针（createMfStrategyFact）。
 * 6. 创建工厂实例。
 * 7. 获取删除工厂的函数指针（deleteMfStrategyFact）。
 * 8. 保存工厂信息到映射表。
 */
bool SelStrategyMgr::loadFactories(const char* path)
{
	if (!StdFile::exists(path))  // 如果目录不存在
	{
		WTSLogger::error("Directory {} of SEL strategy factory not exists", path);  // 记录错误日志：目录不存在
		return false;  // 返回false
	}

	uint32_t count = 0;  // 初始化加载计数器为0
	boost::filesystem::path myPath(path);  // 构造Boost文件系统路径对象
	boost::filesystem::directory_iterator endIter;  // 创建目录迭代器结束标记
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历目录中的所有文件
	{
		if (boost::filesystem::is_directory(iter->path()))  // 如果是目录（不是文件）
			continue;  // 跳过，继续下一个

#ifdef _WIN32  // Windows平台
		if (iter->path().extension() != ".dll")  // 如果文件扩展名不是.dll
			continue;  // 跳过，继续下一个
#else //_UNIX  // Unix/Linux平台
		if (iter->path().extension() != ".so")  // 如果文件扩展名不是.so
			continue;  // 跳过，继续下一个
#endif

		DllHandle hInst = DLLHelper::load_library(iter->path().string().c_str());  // 加载动态库
		if (hInst == NULL)  // 如果加载失败
			continue;  // 跳过，继续下一个

		FuncCreateSelStraFact creator = (FuncCreateSelStraFact)DLLHelper::get_symbol(hInst, "createMfStrategyFact");  // 获取创建工厂的函数指针
		if (creator == NULL)  // 如果函数指针为空（未找到入口函数）
		{
			DLLHelper::free_library(hInst);  // 释放动态库
			continue;  // 跳过，继续下一个
		}

		ISelStrategyFact* fact = creator();  // 调用创建函数，创建工厂实例
		if (fact != NULL)  // 如果工厂实例创建成功
		{
			StraFactInfo& fInfo = _factories[fact->getName()];  // 获取或创建工厂信息（以工厂名称为键）
			fInfo._module_inst = hInst;  // 保存模块句柄
			fInfo._module_path = iter->path().string();  // 保存模块路径
			fInfo._creator = creator;  // 保存创建函数指针
			fInfo._remover = (FuncDeleteSelStraFact)DLLHelper::get_symbol(hInst, "deleteMfStrategyFact");  // 获取并保存删除函数指针
			fInfo._fact = fact;  // 保存工厂实例指针

			WTSLogger::info("SEL strategy factory[{}] loaded", fact->getName());  // 记录信息日志：工厂加载成功

			count++;  // 增加加载计数器
		}
		else  // 如果工厂实例创建失败
		{
			DLLHelper::free_library(hInst);  // 释放动态库
			continue;  // 跳过，继续下一个
		}

	}

	WTSLogger::info("{} SEL strategy factories in directory[{}] loaded", count, path);  // 记录信息日志：总共加载的工厂数量

	return true;  // 返回true，表示加载成功
}

/**
 * @brief 创建策略实例实现（通过工厂名和单元名）
 * @param factname 工厂名称
 * @param unitname 单元名称
 * @param id 策略ID
 * @return SelStrategyPtr 返回策略实例的共享指针，如果创建失败则返回空指针
 *
 * 根据工厂名和单元名直接创建策略实例。
 * 处理流程：
 * 1. 在工厂映射表中查找工厂。
 * 2. 通过工厂创建策略实例。
 * 3. 使用包装器包装策略实例。
 * 4. 将策略实例保存到策略映射表中。
 */
SelStrategyPtr SelStrategyMgr::createStrategy(const char* factname, const char* unitname, const char* id)
{
	auto it = _factories.find(factname);  // 在工厂映射表中查找工厂
	if (it == _factories.end())  // 如果未找到
		return SelStrategyPtr();  // 返回空指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息
	SelStrategyPtr ret(new SelStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例，并用包装器包装
	_strategies[id] = ret;  // 将策略实例保存到策略映射表中（以策略ID为键）
	return ret;  // 返回策略实例指针
}

/**
 * @brief 创建策略实例实现（通过名称）
 * @param name 策略名称，格式为"工厂名.单元名"
 * @param id 策略ID
 * @return SelStrategyPtr 返回策略实例的共享指针，如果创建失败则返回空指针
 *
 * 根据策略名称创建策略实例，名称格式为"工厂名.单元名"。
 * 处理流程：
 * 1. 解析策略名称，提取工厂名和单元名。
 * 2. 调用另一个createStrategy方法创建策略实例。
 */
SelStrategyPtr SelStrategyMgr::createStrategy(const char* name, const char* id)
{
	StringVector ay = StrUtil::split(name, ".");  // 分割策略名称，提取工厂名和单元名
	if (ay.size() < 2)  // 如果分割后的部分少于2个（格式不正确）
		return SelStrategyPtr();  // 返回空指针

	const char* factname = ay[0].c_str();  // 第一部分为工厂名
	const char* unitname = ay[1].c_str();  // 第二部分为单元名

	auto it = _factories.find(factname);  // 在工厂映射表中查找工厂
	if (it == _factories.end())  // 如果未找到
		return SelStrategyPtr();  // 返回空指针

	StraFactInfo& fInfo = (StraFactInfo&)it->second;  // 获取工厂信息
	SelStrategyPtr ret(new SelStraWrapper(fInfo._fact->createStrategy(unitname, id), fInfo._fact));  // 通过工厂创建策略实例，并用包装器包装
	_strategies[id] = ret;  // 将策略实例保存到策略映射表中（以策略ID为键）
	return ret;  // 返回策略实例指针
}

/**
 * @brief 获取策略实例实现
 * @param id 策略ID
 * @return SelStrategyPtr 返回策略实例的共享指针，如果未找到则返回空指针
 *
 * 根据策略ID从策略映射表中获取对应的策略实例。
 */
SelStrategyPtr SelStrategyMgr::getStrategy(const char* id)
{
	auto it = _strategies.find(id);  // 在策略映射表中查找策略
	if (it == _strategies.end())  // 如果未找到
		return SelStrategyPtr();  // 返回空指针

	return it->second;  // 返回策略实例指针
}

