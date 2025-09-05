/*! \file
* \brief Main include.
*
* This is the only file you have to include in order to use the 
* complete threadpool library.
*
* Copyright (c) 2005-2007 Philipp Henkel
*
* Use, modification, and distribution are  subject to the
* Boost Software License, Version 1.0. (See accompanying  file
* LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*
* http://threadpool.sourceforge.net
*
*/

/**
 * @file threadpool.hpp
 * @brief 线程池库主包含文件
 * 
 * 该文件是线程池库的主要包含文件，提供了完整的线程池功能，主要包括：
 * 1. 线程池核心功能（pool.hpp）
 * 2. 异步任务支持（future.hpp）
 * 3. 线程池适配器（pool_adaptors.hpp）
 * 4. 任务适配器（task_adaptors.hpp）
 * 
 * 设计逻辑：
 * - 基于Philipp Henkel的开源线程池库实现
 * - 遵循Boost软件许可证，确保开源合规性
 * - 提供统一的包含接口，简化库的使用
 * - 支持多种线程池模式和任务执行方式
 * - 包含完整的适配器系统，支持不同的使用场景
 * 
 * 主要作用：
 * - 为WonderTrader框架提供高性能的线程池管理
 * - 支持异步任务执行和并行计算
 * - 提供灵活的线程池配置和任务调度
 * - 支持高频交易等对性能要求较高的场景
 */

#ifndef THREADPOOL_HPP_INCLUDED  // 防止头文件重复包含的宏定义
#define THREADPOOL_HPP_INCLUDED  // 定义包含标志宏

#include "./threadpool/future.hpp"  // 包含异步任务支持功能
#include "./threadpool/pool.hpp"    // 包含线程池核心功能

#include "./threadpool/pool_adaptors.hpp"  // 包含线程池适配器功能
#include "./threadpool/task_adaptors.hpp"  // 包含任务适配器功能


#endif // THREADPOOL_HPP_INCLUDED  // 结束头文件保护宏

