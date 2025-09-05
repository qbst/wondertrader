/*!
 * \file WTSVersion.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader版本信息定义文件
 * 
 * 设计逻辑与作用：
 * 本文件定义了WonderTrader系统的版本信息，包括主版本号、次版本号和修订版本号。
 * 通过宏定义的方式提供版本号常量，便于在编译时确定系统版本，也方便进行版本检查和兼容性判断。
 * 版本信息用于系统升级、功能特性判断、API兼容性检查等场景。
 */

 /* 相关语法
    #pragma once
        防止头文件被重复包含：确保它所在的头文件在一次编译过程中只被包含（included）一次
 */

#pragma once  // 防止头文件被重复包含

// 主版本号：表示重大功能更新或架构变更
#define WT_VER_MAJOR 0

// 次版本号：表示功能增强或新特性添加
#define WT_VER_MINOR 9

// 修订版本号：表示bug修复或小的改进
#define WT_VER_PATCH 9

// 完整版本字符串，用于显示和日志输出
static const char WT_VERSION[] = "v0.9.9";

// 产品名称字符串，包含版本信息
static const char WT_PRODUCT[] = "WT_v0.9.9";
