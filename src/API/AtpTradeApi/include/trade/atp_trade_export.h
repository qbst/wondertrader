/**
 * @file atp_trade_export.h
 * @brief ATP交易API导出宏定义文件
 * 
 * 本文件定义了ATP交易API在不同平台和编译模式下的符号导出宏。
 * 主要用于控制动态链接库（DLL/SO）中函数和类的可见性，确保API
 * 在不同操作系统和编译器环境下的正确导出和导入。
 * 
 * 设计逻辑：
 * - 支持静态链接和动态链接两种编译模式
 * - 兼容Windows和Linux/Unix平台的不同导出机制
 * - 通过预编译宏控制符号的导出行为
 * - 确保API在不同平台下的一致性和兼容性
 * 
 * 编译模式说明：
 * 1. 静态链接模式 (TRADE_API_USE_STATIC)
 *    - 不需要导出声明，所有符号直接可见
 *    - 适用于静态库的编译和链接
 * 
 * 2. 动态库导出模式 (TRADE_API_BUILD_EXPORT)
 *    - Windows: 使用 __declspec(dllexport) 导出符号
 *    - Linux: 使用 __attribute__((visibility("default"))) 导出符号
 *    - 适用于构建动态链接库时
 * 
 * 3. 动态库导入模式 (默认)
 *    - Windows: 使用 __declspec(dllimport) 导入符号
 *    - Linux: 使用默认可见性导入符号
 *    - 适用于使用动态链接库的客户端代码
 * 
 * 平台兼容性：
 * - Windows: 支持MSVC编译器的DLL导入导出机制
 * - Linux/Unix: 支持GCC的符号可见性控制机制
 * 
 * @author ATP开发团队
 * @version 1.0
 * @date 2023
 */

#pragma once                                // 防止头文件重复包含的现代预处理指令

// 根据平台和编译模式定义TRADE_API宏
#if defined _WIN32                          // Windows平台编译配置
#   if    defined TRADE_API_USE_STATIC      // 静态链接模式
#      define  TRADE_API                    // 空定义，静态链接不需要导出声明
#   elif  defined TRADE_API_BUILD_EXPORT    // 动态库导出模式（构建DLL时）
#      define  TRADE_API __declspec(dllexport)  // Windows DLL符号导出声明
#   else                                    // 动态库导入模式（使用DLL时）
#      define  TRADE_API __declspec(dllimport)  // Windows DLL符号导入声明
#   endif
#else                                       // Linux/Unix平台编译配置
#      define  TRADE_API  __attribute__((visibility("default")))  // GCC符号可见性属性，设置为默认可见
#endif
