/*!
 * \file WtExecuterFactory.cpp
 * \project	WonderTrader
 * 
 * \brief 执行器工厂类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtExecuterFactory类的所有方法，提供了执行器工厂的加载和执行单元创建功能。
 * 主要实现包括：
 * 1. 工厂加载：扫描指定目录，加载所有DLL/SO插件，提取工厂接口
 * 2. 执行单元创建：根据工厂名称和单元名称创建不同类型的执行单元
 * 3. 错误处理：处理文件不存在、DLL加载失败、符号解析失败等异常情况
 * 4. 日志记录：记录工厂加载和执行单元创建的详细信息
 * 
 * 实现细节：
 * - 使用boost::filesystem遍历目录查找插件文件
 * - 使用DLLHelper加载动态库并获取导出函数
 * - 通过工厂接口创建执行单元并封装为智能指针
 * - 支持Windows和Linux平台的动态库格式（.dll/.so）
 */
#include "WtExecuterFactory.h"  // 包含执行器工厂头文件

#include "../Share/StdUtils.hpp"  // 包含标准工具函数，如文件存在性检查
#include "../Share/StrUtil.hpp"  // 包含字符串处理工具函数
#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

#include <boost/filesystem.hpp>  // 包含boost文件系统库，用于目录遍历和路径操作


USING_NS_WTP;  // 使用WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
//WtExecuterFactory类实现
/**
 * @brief 加载执行器工厂插件
 * @param path 工厂插件所在目录路径
 * @return bool 加载成功返回true，失败返回false
 * 
 * 该函数从指定目录加载所有执行器工厂插件，具体流程：
 * 1. 检查目录是否存在，不存在则记录错误并返回false
 * 2. 遍历目录中的所有文件
 * 3. 过滤出DLL/SO文件（Windows平台为.dll，Linux平台为.so）
 * 4. 加载动态库并获取createExecFact和deleteExecFact函数
 * 5. 调用createExecFact创建工厂实例
 * 6. 将工厂信息注册到内部映射表中
 * 
 * 注意事项：
 * - 如果某个插件加载失败，会跳过该插件继续处理其他插件
 * - 每个插件必须导出createExecFact和deleteExecFact函数
 * - 工厂名称通过工厂的getName()方法获取
 */
bool WtExecuterFactory::loadFactories(const char* path)
{
	if (!StdFile::exists(path))  // 检查目录是否存在
	{
		WTSLogger::error("Directory {} of executer factory not exists", path);  // 记录错误日志：目录不存在
		return false;  // 返回失败
	}

	boost::filesystem::path myPath(path);  // 将字符串路径转换为boost文件系统路径对象
	boost::filesystem::directory_iterator endIter;  // 创建目录迭代器结束标记
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历目录中的所有文件
	{
		if (boost::filesystem::is_directory(iter->path()))  // 如果是目录则跳过
			continue;

#ifdef _WIN32  // Windows平台编译条件
		if (iter->path().extension() != ".dll")  // 检查文件扩展名是否为.dll
			continue;  // 不是.dll文件则跳过
#else //_UNIX  // Unix/Linux平台编译条件
		if (iter->path().extension() != ".so")  // 检查文件扩展名是否为.so
			continue;  // 不是.so文件则跳过
#endif

		const std::string& path = iter->path().string();  // 获取文件完整路径字符串

		DllHandle hInst = DLLHelper::load_library(path.c_str());  // 加载动态库，返回库句柄
		if (hInst == NULL)  // 检查加载是否成功
		{
			continue;  // 加载失败则跳过该文件
		}

		FuncCreateExeFact creator = (FuncCreateExeFact)DLLHelper::get_symbol(hInst, "createExecFact");  // 获取创建工厂函数的符号地址
		if (creator == NULL)  // 检查符号是否存在
		{
			DLLHelper::free_library(hInst);  // 释放动态库句柄
			continue;  // 符号不存在则跳过该文件
		}

		ExeFactInfo fInfo;  // 创建工厂信息结构体
		fInfo._module_inst = hInst;  // 保存模块句柄
		fInfo._module_path = iter->path().string();  // 保存模块文件路径
		fInfo._creator = creator;  // 保存创建函数指针
		fInfo._remover = (FuncDeleteExeFact)DLLHelper::get_symbol(hInst, "deleteExecFact");  // 获取删除工厂函数的符号地址
		fInfo._fact = fInfo._creator();  // 调用创建函数创建工厂实例

		_factories[fInfo._fact->getName()] = fInfo;  // 将工厂信息注册到映射表中，以工厂名称为键

		WTSLogger::info("Executer factory {} loaded", fInfo._fact->getName());  // 记录成功加载日志
	}

	return true;  // 返回成功
}

/**
 * @brief 创建执行单元（完整接口）
 * @param factname 工厂名称
 * @param unitname 执行单元名称
 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
 * 
 * 该函数根据指定的工厂名称和单元名称创建普通执行单元。
 * 流程：
 * 1. 在工厂映射表中查找指定名称的工厂
 * 2. 如果工厂不存在，返回空指针
 * 3. 调用工厂的createExeUnit方法创建执行单元
 * 4. 如果创建失败，记录错误日志并返回空指针
 * 5. 将执行单元封装为ExeUnitWrapper并返回智能指针
 */
ExecuteUnitPtr WtExecuterFactory::createExeUnit(const char* factname, const char* unitname)
{
	auto it = _factories.find(factname);  // 在工厂映射表中查找指定名称的工厂
	if (it == _factories.end())  // 如果工厂不存在
		return ExecuteUnitPtr();  // 返回空指针

	ExeFactInfo& fInfo = (ExeFactInfo&)it->second;  // 获取工厂信息引用
	ExecuteUnit* unit = fInfo._fact->createExeUnit(unitname);  // 调用工厂方法创建执行单元
	if (unit == NULL)  // 检查创建是否成功
	{
		WTSLogger::error("Createing execution unit failed: {}.{}", factname, unitname);  // 记录错误日志：创建失败
		return ExecuteUnitPtr();  // 返回空指针
	}
	return ExecuteUnitPtr(new ExeUnitWrapper(unit, fInfo._fact));  // 封装执行单元为智能指针并返回
}

/**
 * @brief 创建差量执行单元（完整接口）
 * @param factname 工厂名称
 * @param unitname 执行单元名称
 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
 * 
 * 该函数根据指定的工厂名称和单元名称创建差量执行单元。
 * 流程与createExeUnit类似，但调用的是createDiffExeUnit方法。
 */
ExecuteUnitPtr WtExecuterFactory::createDiffExeUnit(const char* factname, const char* unitname)
{
	auto it = _factories.find(factname);  // 在工厂映射表中查找指定名称的工厂
	if (it == _factories.end())  // 如果工厂不存在
		return ExecuteUnitPtr();  // 返回空指针

	ExeFactInfo& fInfo = (ExeFactInfo&)it->second;  // 获取工厂信息引用
	ExecuteUnit* unit = fInfo._fact->createDiffExeUnit(unitname);  // 调用工厂方法创建差量执行单元
	if (unit == NULL)  // 检查创建是否成功
	{
		WTSLogger::error("Createing diff execution unit failed: {}.{}", factname, unitname);  // 记录错误日志：创建失败
		return ExecuteUnitPtr();  // 返回空指针
	}
	return ExecuteUnitPtr(new ExeUnitWrapper(unit, fInfo._fact));  // 封装执行单元为智能指针并返回
}

/**
 * @brief 创建套利执行单元（完整接口）
 * @param factname 工厂名称
 * @param unitname 执行单元名称
 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
 * 
 * 该函数根据指定的工厂名称和单元名称创建套利执行单元。
 * 流程与createExeUnit类似，但调用的是createArbiExeUnit方法。
 */
ExecuteUnitPtr WtExecuterFactory::createArbiExeUnit(const char* factname, const char* unitname)
{
	auto it = _factories.find(factname);  // 在工厂映射表中查找指定名称的工厂
	if (it == _factories.end())  // 如果工厂不存在
		return ExecuteUnitPtr();  // 返回空指针

	ExeFactInfo& fInfo = (ExeFactInfo&)it->second;  // 获取工厂信息引用
	ExecuteUnit* unit = fInfo._fact->createArbiExeUnit(unitname);  // 调用工厂方法创建套利执行单元
	if (unit == NULL)  // 检查创建是否成功
	{
		WTSLogger::error("Createing arbi execution unit failed: {}.{}", factname, unitname);  // 记录错误日志：创建失败
		return ExecuteUnitPtr();  // 返回空指针
	}
	return ExecuteUnitPtr(new ExeUnitWrapper(unit, fInfo._fact));  // 封装执行单元为智能指针并返回
}

/**
 * @brief 创建执行单元（简化接口）
 * @param name 执行单元名称，格式为"工厂名.单元名"
 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
 * 
 * 该函数是createExeUnit的简化版本，支持使用"工厂名.单元名"格式的名称。
 * 流程：
 * 1. 使用点号分割名称字符串，提取工厂名和单元名
 * 2. 如果分割后的元素少于2个，返回空指针
 * 3. 调用完整接口版本创建执行单元
 */
ExecuteUnitPtr WtExecuterFactory::createExeUnit(const char* name)
{
	StringVector ay = StrUtil::split(name, ".");  // 使用点号分割名称字符串，返回字符串向量
	if (ay.size() < 2)  // 检查分割后的元素数量是否足够
		return ExecuteUnitPtr();  // 元素不足则返回空指针

	const char* factname = ay[0].c_str();  // 获取第一个元素作为工厂名称
	const char* unitname = ay[1].c_str();  // 获取第二个元素作为单元名称

	auto it = _factories.find(factname);  // 在工厂映射表中查找指定名称的工厂
	if (it == _factories.end())  // 如果工厂不存在
		return ExecuteUnitPtr();  // 返回空指针

	ExeFactInfo& fInfo = (ExeFactInfo&)it->second;  // 获取工厂信息引用
	ExecuteUnit* unit = fInfo._fact->createExeUnit(unitname);  // 调用工厂方法创建执行单元
	if (unit == NULL)  // 检查创建是否成功
	{
		WTSLogger::error("Createing execution unit failed: {}", name);  // 记录错误日志：创建失败
		return ExecuteUnitPtr();  // 返回空指针
	}
	return ExecuteUnitPtr(new ExeUnitWrapper(unit, fInfo._fact));  // 封装执行单元为智能指针并返回
}

/**
 * @brief 创建差量执行单元（简化接口）
 * @param name 执行单元名称，格式为"工厂名.单元名"
 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
 * 
 * 该函数是createDiffExeUnit的简化版本，支持使用"工厂名.单元名"格式的名称。
 * 流程与createExeUnit类似，但调用的是createDiffExeUnit方法。
 */
ExecuteUnitPtr WtExecuterFactory::createDiffExeUnit(const char* name)
{
	StringVector ay = StrUtil::split(name, ".");  // 使用点号分割名称字符串，返回字符串向量
	if (ay.size() < 2)  // 检查分割后的元素数量是否足够
		return ExecuteUnitPtr();  // 元素不足则返回空指针

	const char* factname = ay[0].c_str();  // 获取第一个元素作为工厂名称
	const char* unitname = ay[1].c_str();  // 获取第二个元素作为单元名称

	auto it = _factories.find(factname);  // 在工厂映射表中查找指定名称的工厂
	if (it == _factories.end())  // 如果工厂不存在
		return ExecuteUnitPtr();  // 返回空指针

	ExeFactInfo& fInfo = (ExeFactInfo&)it->second;  // 获取工厂信息引用
	ExecuteUnit* unit = fInfo._fact->createDiffExeUnit(unitname);  // 调用工厂方法创建差量执行单元
	if (unit == NULL)  // 检查创建是否成功
	{
		WTSLogger::error("Createing execution unit failed: {}", name);  // 记录错误日志：创建失败
		return ExecuteUnitPtr();  // 返回空指针
	}
	return ExecuteUnitPtr(new ExeUnitWrapper(unit, fInfo._fact));  // 封装执行单元为智能指针并返回
}

/**
 * @brief 创建套利执行单元（简化接口）
 * @param name 执行单元名称，格式为"工厂名.单元名"
 * @return ExecuteUnitPtr 返回执行单元智能指针，失败返回空指针
 * 
 * 该函数是createArbiExeUnit的简化版本，支持使用"工厂名.单元名"格式的名称。
 * 流程与createExeUnit类似，但调用的是createArbiExeUnit方法。
 */
ExecuteUnitPtr WtExecuterFactory::createArbiExeUnit(const char* name)
{
	StringVector ay = StrUtil::split(name, ".");  // 使用点号分割名称字符串，返回字符串向量
	if (ay.size() < 2)  // 检查分割后的元素数量是否足够
		return ExecuteUnitPtr();  // 元素不足则返回空指针

	const char* factname = ay[0].c_str();  // 获取第一个元素作为工厂名称
	const char* unitname = ay[1].c_str();  // 获取第二个元素作为单元名称

	auto it = _factories.find(factname);  // 在工厂映射表中查找指定名称的工厂
	if (it == _factories.end())  // 如果工厂不存在
		return ExecuteUnitPtr();  // 返回空指针

	ExeFactInfo& fInfo = (ExeFactInfo&)it->second;  // 获取工厂信息引用
	ExecuteUnit* unit = fInfo._fact->createArbiExeUnit(unitname);  // 调用工厂方法创建套利执行单元
	if (unit == NULL)  // 检查创建是否成功
	{
		WTSLogger::error("Createing execution unit failed: {}", name);  // 记录错误日志：创建失败
		return ExecuteUnitPtr();  // 返回空指针
	}
	return ExecuteUnitPtr(new ExeUnitWrapper(unit, fInfo._fact));  // 封装执行单元为智能指针并返回
}
