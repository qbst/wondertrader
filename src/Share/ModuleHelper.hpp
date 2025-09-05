/**
 * @file ModuleHelper.hpp
 * @brief 模块路径获取辅助工具
 * 
 * 该文件提供了跨平台的模块路径获取功能，主要包括：
 * 1. Windows平台下的DLL模块句柄管理
 * 2. Linux/Unix平台下的动态库路径获取
 * 3. 统一的二进制文件目录路径获取接口
 * 
 * 设计逻辑：
 * - 通过条件编译实现Windows和Linux/Unix的跨平台兼容
 * - Windows平台使用DllMain和GetModuleFileName获取模块路径
 * - Linux/Unix平台使用dladdr获取动态库路径
 * - 使用静态变量缓存路径信息，避免重复计算
 * - 提供统一的getBinDir接口，隐藏平台差异
 * 
 * 主要作用：
 * - 为WonderTrader框架提供模块路径管理能力
 * - 支持插件和动态库的路径定位
 * - 简化跨平台模块路径获取的开发工作
 */

#pragma once  // 防止头文件重复包含
#include "../Share/StrUtil.hpp"  // 包含字符串工具库，用于路径标准化

#ifdef _MSC_VER  // Microsoft Visual C++编译器（Windows平台）
#include <wtypes.h>  // Windows类型定义头文件
static HMODULE	g_dllModule = NULL;  // 全局DLL模块句柄，用于存储当前模块的句柄

/**
 * @brief Windows DLL入口点函数
 * @param hModule 模块句柄
 * @param ul_reason_for_call 调用原因
 * @param lpReserved 保留参数
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 该函数是Windows DLL的标准入口点，在DLL加载时被系统调用。
 * 主要用于保存模块句柄，供后续路径获取使用。
 */
BOOL APIENTRY DllMain(
	HANDLE hModule,  // 模块句柄
	DWORD  ul_reason_for_call,  // 调用原因（DLL_PROCESS_ATTACH等）
	LPVOID lpReserved  // 保留参数，未使用
)
{
	switch (ul_reason_for_call)  // 根据调用原因进行相应处理
	{
	case DLL_PROCESS_ATTACH:  // DLL被加载到进程时
		g_dllModule = (HMODULE)hModule;  // 保存模块句柄到全局变量
		break;  // 跳出switch语句
	}
	return TRUE;  // 返回成功状态
}
#else  // 非Windows平台（Linux/Unix）
#include <dlfcn.h>  // 动态链接库函数头文件

/**
 * @brief 辅助函数，用于获取函数地址
 * 
 * 该函数仅用于获取其地址，供dladdr函数使用。
 * 函数体为空，不执行任何实际操作。
 */
void inst_hlp() {}  // 空函数，仅用于获取地址

/**
 * @brief 获取当前模块的安装路径（Linux/Unix平台）
 * @return const std::string& 返回模块路径的常量引用
 * 
 * 该函数使用dladdr获取指定函数的动态库信息，从而得到当前模块的路径。
 * 使用静态变量缓存结果，避免重复调用dladdr。
 */
static const std::string& getInstPath()
{
	static std::string moduleName;  // 静态变量，缓存模块路径
	if (moduleName.empty())  // 检查是否已缓存路径
	{
		Dl_info dl_info;  // 动态库信息结构体
		dladdr((void *)inst_hlp, &dl_info);  // 获取inst_hlp函数所在的动态库信息
		moduleName = dl_info.dli_fname;  // 提取动态库文件名
	}

	return moduleName;  // 返回缓存的模块路径
}
#endif

/**
 * @brief 获取二进制文件目录路径
 * @return const char* 返回二进制文件目录的字符串指针
 * 
 * 该函数提供跨平台的二进制文件目录获取功能：
 * - Windows平台：通过GetModuleFileName获取模块路径，然后提取目录部分
 * - Linux/Unix平台：通过dladdr获取模块路径，然后提取目录部分
 * 
 * 使用静态变量缓存结果，避免重复计算路径。
 */
static const char* getBinDir()
{
	static std::string g_bin_dir;  // 静态变量，缓存二进制文件目录路径

	if (g_bin_dir.empty())  // 检查是否已缓存路径
	{
#ifdef _MSC_VER  // Windows平台
		char strPath[MAX_PATH];  // 路径缓冲区，MAX_PATH为Windows最大路径长度
		GetModuleFileName(g_dllModule, strPath, MAX_PATH);  // 获取模块的完整文件路径

		g_bin_dir = StrUtil::standardisePath(strPath, false);  // 使用StrUtil标准化路径格式
#else  // Linux/Unix平台
		g_bin_dir = getInstPath();  // 调用平台特定函数获取模块路径
#endif
		std::size_t nPos = g_bin_dir.find_last_of('/');  // 查找最后一个路径分隔符的位置
		g_bin_dir = g_bin_dir.substr(0, nPos + 1);  // 提取目录部分（包含末尾的分隔符）
	}

	return g_bin_dir.c_str();  // 返回C风格字符串指针
}