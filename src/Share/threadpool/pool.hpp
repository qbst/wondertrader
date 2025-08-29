/*!
* \file pool.hpp
* \brief 线程池核心实现类
*
* 该文件包含了线程池的核心类：thread_pool<Task, SchedulingPolicy>。
* 线程池是在同一进程内进行异步和并行处理的机制。pool类提供了
* 将异步任务作为函数对象分发的便捷方式。这些任务的调度可以通过
* 使用自定义调度器轻松控制。
*
* 设计逻辑：
* - 使用策略模式实现可配置的调度策略
* - 支持动态调整线程池大小
* - 提供完整的任务生命周期管理
* - 实现线程安全的任务队列操作
*
* 作用：
* - 提供高性能的线程池实现
* - 支持多种调度策略（FIFO、LIFO、优先级）
* - 管理工作线程的创建、销毁和调度
* - 提供任务状态监控和等待机制
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


#ifndef THREADPOOL_POOL_HPP_INCLUDED
#define THREADPOOL_POOL_HPP_INCLUDED

// 引入Boost引用包装器
#include <boost/ref.hpp>

// 引入线程池核心实现
#include "./detail/pool_core.hpp"

// 引入任务适配器
#include "task_adaptors.hpp"

// 引入线程安全的锁定指针
#include "./detail/locking_ptr.hpp"

// 引入各种策略类
#include "scheduling_policies.hpp"  // 调度策略
#include "size_policies.hpp"        // 大小策略
#include "shutdown_policies.hpp"    // 关闭策略



/// 线程池命名空间，包含线程池和相关实用工具类
namespace boost { namespace threadpool
{



  /*!
  * \brief 线程池类
  *
  * 线程池是在同一进程内进行异步和并行处理的机制。pool类提供了将异步任务
  * 作为函数对象分发的便捷方式。这些任务的调度可以通过使用自定义调度器
  * 轻松控制。任务不得抛出异常。
  *
  * 线程池具有默认构造、拷贝构造和可赋值的特性。它具有引用语义；同一池的
  * 所有副本都是等价和可互换的。除赋值外，池上的所有操作都是强线程安全或
  * 顺序一致的；也就是说，并发调用的行为就像调用已按未指定的顺序顺序发出一样。
  *
  * \tparam Task 实现operator 'void operator() (void) const'的函数对象。
  *              线程池调用operator()来执行任务。异常将被忽略。
  * \tparam SchedulingPolicy 决定任务如何调度的任务容器。保证此容器一次只能
  *                          被一个线程访问。调度器不应抛出异常。
  * \tparam SizePolicy 控制线程池大小的策略类
  * \tparam SizePolicyController 大小策略控制器类
  * \tparam ShutdownPolicy 线程池关闭策略类
  *
  * \note 线程池类是线程安全的。
  * 
  * \see 任务类型: task_func, prio_task_func
  * \see 调度策略: fifo_scheduler, lifo_scheduler, prio_scheduler
  */ 
  template <
    typename Task                                   = task_func,           ///< 任务类型，默认为task_func
    template <typename> class SchedulingPolicy      = fifo_scheduler,     ///< 调度策略，默认为FIFO
    template <typename> class SizePolicy            = static_size,        ///< 大小策略，默认为静态大小
    template <typename> class SizePolicyController  = resize_controller,  ///< 大小控制器，默认为可调整大小
    template <typename> class ShutdownPolicy        = wait_for_all_tasks  ///< 关闭策略，默认等待所有任务完成
  > 
  class thread_pool 
  {
    /// 线程池核心类型定义，使用Pimpl惯用法隐藏实现细节
    typedef detail::pool_core<Task, 
                              SchedulingPolicy,
                              SizePolicy,
                              SizePolicyController,
                              ShutdownPolicy> pool_core_type;
    /// 指向核心实现的智能指针，使用Pimpl惯用法
    shared_ptr<pool_core_type>          m_core;
    /// 关闭控制器，当持有核心指针的最后一个线程池被删除时，控制器会关闭线程池
    shared_ptr<void>                    m_shutdown_controller;

  public: // 类型定义
    /// 任务类型定义
    typedef Task task_type;
    /// 调度器类型定义
    typedef SchedulingPolicy<task_type> scheduler_type;
    /// 大小策略类型定义
    typedef SizePolicy<pool_core_type> size_policy_type; 
    /// 大小控制器类型定义
    typedef SizePolicyController<pool_core_type> size_controller_type;

  public:
    /*!
     * \brief 构造函数
     *
     * 创建线程池并立即调整到指定的线程数量。线程池的实际线程数量取决于大小策略。
     *
     * \param initial_threads 初始线程数量，默认为0
     */
    thread_pool(size_t initial_threads = 0)
    : m_core(new pool_core_type)
    , m_shutdown_controller(static_cast<void*>(0), bind(&pool_core_type::shutdown, m_core))
    {
      // 使用大小策略初始化线程池
      size_policy_type::init(*m_core, initial_threads);
    }

    /*!
     * \brief 获取管理线程池中线程数量的大小控制器
     *
     * 返回用于控制线程池大小的控制器对象。
     *
     * \return 大小控制器对象
     * \see SizePolicy
     */
    size_controller_type size_controller()
    {
      return m_core->size_controller();
    }

    /*!
     * \brief 获取线程池中的线程数量
     *
     * 返回当前线程池中工作线程的总数。
     *
     * \return 线程数量
     */
    size_t size() const
    {
      return m_core->size();
    }


     /*!
      * \brief 调度任务进行异步执行
      *
      * 将任务添加到线程池中进行异步执行，任务只会执行一次。
      *
      * \param task 任务函数对象，不应抛出异常
      * \return 如果任务能够被调度则返回true，否则返回false
      */  
     bool schedule(task_type const & task)
     {	
       return m_core->schedule(task);
     }

    /*!
     * \brief 返回当前正在执行的任务数量
     *
     * 获取线程池中当前活动（正在执行）的任务数量。
     *
     * \return 活动任务的数量
     */  
    size_t active() const
    {
      return m_core->active();
    }

    /*!
     * \brief 返回准备执行的任务数量
     *
     * 获取调度器中等待执行的任务数量。
     *
     * \return 待执行任务的数量
     */  
    size_t pending() const
    {
      return m_core->pending();
    }

    /*!
     * \brief 从线程池的调度器中移除所有待执行任务
     *
     * 清空任务队列，丢弃所有等待执行的任务。正在执行的任务不受影响。
     */  
    void clear()
    { 
      m_core->clear();
    }    

    /*!
     * \brief 指示是否没有待执行的任务
     *
     * 检查线程池是否没有待执行的任务。
     *
     * \return 如果没有准备执行的任务则返回true，否则返回false
     * \note 此函数比检查'pending() == 0'更高效
     */   
    bool empty() const
    {
      return m_core->empty();
    }	

    /*!
     * \brief 阻塞当前线程直到任务数量达到阈值
     *
     * 阻塞当前执行线程，直到所有活动和待执行任务的总数等于或小于给定的阈值。
     *
     * \param task_threshold 线程池和调度器中任务的最大数量，默认为0（等待所有任务完成）
     */     
    void wait(size_t task_threshold = 0) const
    {
      m_core->wait(task_threshold);
    }	

    /*!
     * \brief 带超时的等待任务完成
     *
     * 阻塞当前执行线程，直到达到时间戳或所有活动和待执行任务的总数
     * 等于或小于给定的阈值。
     *
     * \param timestamp 函数最晚返回的时间
     * \param task_threshold 线程池和调度器中任务的最大数量，默认为0
     * \return 如果任务总数等于或小于阈值则返回true，否则返回false（超时）
     */       
    bool wait(xtime const & timestamp, size_t task_threshold = 0) const
    {
      return m_core->wait(timestamp, task_threshold);
    }
  };



  /*!
   * \brief FIFO线程池
   *
   * 该线程池的任务按FIFO（先进先出）顺序调度执行task_func函数对象。
   * 使用静态大小策略、可调整大小控制器和等待所有任务完成的关闭策略。
   */ 
  typedef thread_pool<task_func, fifo_scheduler, static_size, resize_controller, wait_for_all_tasks> fifo_pool;

  /*!
   * \brief LIFO线程池
   *
   * 该线程池的任务按LIFO（后进先出）顺序调度执行task_func函数对象。
   * 使用静态大小策略、可调整大小控制器和等待所有任务完成的关闭策略。
   */ 
  typedef thread_pool<task_func, lifo_scheduler, static_size, resize_controller, wait_for_all_tasks> lifo_pool;

  /*!
   * \brief 优先级线程池
   *
   * 该线程池的任务按优先级顺序调度执行prio_task_func函数对象。
   * 使用静态大小策略、可调整大小控制器和等待所有任务完成的关闭策略。
   */ 
  typedef thread_pool<prio_task_func, prio_scheduler, static_size, resize_controller, wait_for_all_tasks> prio_pool;

  /*!
   * \brief 标准线程池
   *
   * 标准线程池等同于FIFO线程池，任务按FIFO顺序调度执行task_func函数对象。
   * 这是最常用的线程池类型定义。
   */ 
  typedef fifo_pool pool;



} } // namespace boost::threadpool

#endif // THREADPOOL_POOL_HPP_INCLUDED
