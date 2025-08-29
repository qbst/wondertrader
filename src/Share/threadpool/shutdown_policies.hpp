/*!
* \file shutdown_policies.hpp
* \brief 线程池关闭策略类
*
* 该文件包含了线程池的关闭策略实现。关闭策略控制线程池在不再被引用时的行为。
* 不同的关闭策略决定了线程池如何处理剩余任务和工作线程的终止方式。
*
* 设计逻辑：
* - 使用策略模式实现不同的关闭行为
* - 提供多种关闭策略以适应不同的应用场景
* - 控制任务完成和线程终止的时序
* - 确保资源的正确清理和释放
*
* 作用：
* - 定义线程池的关闭行为规则
* - 处理剩余任务的执行策略
* - 管理工作线程的优雅退出
* - 提供可配置的关闭策略选项
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

#ifndef THREADPOOL_SHUTDOWN_POLICIES_HPP_INCLUDED
#define THREADPOOL_SHUTDOWN_POLICIES_HPP_INCLUDED

/// 线程池命名空间，包含线程池和相关实用工具类
namespace boost { namespace threadpool
{

/*!
 * \brief 等待所有任务完成的关闭策略
 *
 * 该关闭策略会等待所有任务（包括队列中的待执行任务和正在执行的任务）
 * 完成后，再终止所有工作线程。这是最安全的关闭策略，确保没有任务丢失。
 *
 * \tparam Pool 线程池的核心类型
 */ 
  template<typename Pool>
  class wait_for_all_tasks
  {
  public:
    /*!
     * \brief 执行关闭操作
     *
     * 等待所有任务完成，然后终止所有工作线程。
     *
     * \param pool 要关闭的线程池引用
     */
    static void shutdown(Pool& pool)
    {
      pool.wait();                      // 等待所有任务完成
      pool.terminate_all_workers(true); // 等待工作线程终止
    }
  };

  /*!
   * \brief 等待活动任务完成的关闭策略
   *
   * 该关闭策略会清除队列中的待执行任务，只等待当前正在执行的任务完成，
   * 然后终止所有工作线程。这种策略会丢弃队列中未执行的任务。
   *
   * \tparam Pool 线程池的核心类型
   */ 
  template<typename Pool>
  class wait_for_active_tasks
  {
  public:
    /*!
     * \brief 执行关闭操作
     *
     * 清除待执行任务队列，等待当前活动任务完成，然后终止所有工作线程。
     *
     * \param pool 要关闭的线程池引用
     */
    static void shutdown(Pool& pool)
    {
      pool.clear();                     // 清除队列中的待执行任务
      pool.wait();                      // 等待当前活动任务完成
      pool.terminate_all_workers(true); // 等待工作线程终止
    }
  };

  /*!
   * \brief 立即关闭策略
   *
   * 该关闭策略不等待任何任务或工作线程终止，立即清除任务队列并终止线程池。
   * 尽管如此，所有正在执行的活动任务仍会被完全处理完成。这是最快的关闭策略，
   * 但可能会丢失队列中的待执行任务。
   *
   * \tparam Pool 线程池的核心类型
   */ 
  template<typename Pool>
  class immediately
  {
  public:
    /*!
     * \brief 执行关闭操作
     *
     * 立即清除任务队列并终止所有工作线程，不等待任务完成。
     *
     * \param pool 要关闭的线程池引用
     */
    static void shutdown(Pool& pool)
    {
      pool.clear();                      // 清除队列中的待执行任务
      pool.terminate_all_workers(false); // 不等待，立即终止工作线程
    }
  };

} } // namespace boost::threadpool

#endif // THREADPOOL_SHUTDOWN_POLICIES_HPP_INCLUDED
