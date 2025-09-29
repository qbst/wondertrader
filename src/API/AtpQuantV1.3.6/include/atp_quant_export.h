/**
 * @file atp_quant_export.h
 * @brief ATP量化交易API导出符号定义文件
 * 
 * 本文件定义了ATP量化交易API的导出符号宏，用于控制动态库的符号可见性。
 * 主要作用是确保API函数和类在不同平台和编译器下都能正确导出和导入。
 * 
 * 设计逻辑：
 * - 在Windows平台使用__declspec(dllimport)进行符号导入
 * - 在Linux/Unix平台使用__attribute__((visibility("default")))控制符号可见性
 * - 通过条件编译实现跨平台兼容性
 * - 为ATP量化API的所有公共接口提供统一的导出标记
 */

#ifndef _ATP_QUANT_EXPORT_H_              // 防止头文件重复包含的预处理保护
#define _ATP_QUANT_EXPORT_H_              // 定义头文件保护宏

#if defined _WIN32                        // 条件编译：检测是否为Windows平台
#      define  QUANT_API __declspec(dllimport)    // Windows平台：使用__declspec(dllimport)导入动态库符号
#else                                     // 非Windows平台（Linux/Unix等）
#      define  QUANT_API  __attribute__((visibility("default")))  // Linux平台：设置符号默认可见性，确保符号可以被外部访问
#endif                                    // 结束条件编译

#endif  //#ifndef _ATP_QUANT_EXPORT_H_    // 结束头文件保护