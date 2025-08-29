/*!
* \file pool_core.hpp
* \brief 线程池核心实现类
*
* 该文件包含了线程池的核心类：pool_core<Task, SchedulingPolicy>。
* 线程池是在同一进程内进行异步和并行处理的机制。pool_core类提供了
* 将异步任务作为函数对象分发的便捷方式。这些任务的调度可以通过
* 使用自定义调度器轻松控制。
*
* 设计逻辑：
* - 实现线程池的核心功能和生命周期管理
* - 提供线程安全的任务调度和执行机制
* - 支持动态调整工作线程数量
* - 实现多种策略模式（调度、大小、关闭）
* - 提供完整的线程同步和协调机制
*
* 作用：
* - 管理工作线程的创建、销毁和状态
* - 实现任务队列的线程安全操作
* - 提供任务执行的调度和监控功能
* - 支持线程池的优雅关闭和资源清理
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

#ifndef THREADPOOL_POOL_CORE_HPP_INCLUDED
#define THREADPOOL_POOL_CORE_HPP_INCLUDED

// 引入线程安全的锁定指针
#include "locking_ptr.hpp"
// 引入工作线程实现
#include "worker_thread.hpp"

// 引入任务适配器
#include "../task_adaptors.hpp"

// 引入Boost线程库
#include <boost/thread.hpp>
// 引入Boost线程异常处理
#include <boost/thread/exceptions.hpp>
// 引入Boost互斥锁
#include <boost/thread/mutex.hpp>
// 引入Boost条件变量
#include <boost/thread/condition.hpp>
// 引入Boost智能指针
#include <boost/smart_ptr.hpp>
// 引入Boost函数绑定
#include <boost/bind.hpp>
// 引入Boost静态断言
#include <boost/static_assert.hpp>
// 引入Boost类型特征
#include <boost/type_traits.hpp>

// 引入标准向量容器
#include <vector>

/// 线程池命名空间，包含线程池和相关实用工具类
namespace boost { namespace threadpool { namespace detail 
{

  /*!
   * \brief 线程池核心实现类
   *
   * 线程池是在同一进程内进行异步和并行处理的机制。pool_core类提供了
   * 将异步任务作为函数对象分发的便捷方式。这些任务的调度可以通过
   * 使用自定义调度器轻松控制。任务不得抛出异常。
   *
   * pool_core是默认可构造的和不可拷贝的。它使用enable_shared_from_this
   * 来支持从内部获取自身的shared_ptr，确保对象生命周期的安全管理。
   *
   * \tparam Task 实现operator 'void operator() (void) const'的函数对象。
   *              线程池调用operator()来执行任务。异常将被忽略。
   * \tparam SchedulingPolicy 决定任务如何调度的任务容器。保证此容器一次只能
   *                          被一个线程访问。调度器不应抛出异常。
   * \tparam SizePolicy 控制线程池大小的策略类
   * \tparam SizePolicyController 大小策略控制器类
   * \tparam ShutdownPolicy 线程池关闭策略类
   *
   * \note pool_core类是线程安全的。
   * 
   * \see 任务类型: task_func, prio_task_func
   * \see 调度策略: fifo_scheduler, lifo_scheduler, prio_scheduler
   */ 
  template <
    typename Task,                                          ///< 任务类型

    template <typename> class SchedulingPolicy,             ///< 调度策略模板
    template <typename> class SizePolicy,                   ///< 大小策略模板
    template <typename> class SizePolicyController,         ///< 大小控制器模板
    template <typename> class ShutdownPolicy                ///< 关闭策略模板
  > 
  class pool_core
  : public enable_shared_from_this< pool_core<Task, SchedulingPolicy, SizePolicy, SizePolicyController, ShutdownPolicy > > 
  , private noncopyable
  {

  public: // 类型定义
    /// 任务类型定义
    typedef Task task_type;
    /// 调度器类型定义
    typedef SchedulingPolicy<task_type> scheduler_type;
    /// 线程池类型定义
    typedef pool_core<Task, 
                      SchedulingPolicy, 
                      SizePolicy,
                      SizePolicyController,
                      ShutdownPolicy > pool_type;
    /// 大小策略类型定义
    typedef SizePolicy<pool_type> size_policy_type;
    /// 大小控制器类型定义
    typedef SizePolicyController<pool_type> size_controller_type;
    /// 关闭策略类型定义
    typedef ShutdownPolicy<pool_type> shutdown_policy_type;
    /// 工作线程类型定义
    typedef worker_thread<pool_type> worker_type;

    // 静态断言：任务必须是无参数函数
    BOOST_STATIC_ASSERT(function_traits<task_type()>::arity == 0);

    // 静态断言：任务函数的返回类型必须是void
    BOOST_STATIC_ASSERT(is_void<typename result_of<task_type()>::type >::value);


  private:  // 友元类声明
    /// 工作线程类是友元，可以访问私有成员
    friend class worker_thread<pool_type>;

// Sun C++编译器兼容性处理
#if defined(__SUNPRO_CC) && (__SUNPRO_CC <= 0x580)  // 在CC: Sun C++ 5.8 Patch 121018-08 2006/12/06上测试
   friend class SizePolicy;
   friend class ShutdownPolicy;
#else
   /// 大小策略类是友元，可以访问私有成员
   friend class SizePolicy<pool_type>;
   /// 关闭策略类是友元，可以访问私有成员
   friend class ShutdownPolicy<pool_type>;
#endif

  private: // 以下成员可能被多个线程同时访问，使用volatile确保可见性
    volatile size_t m_worker_count;	        ///< 当前工作线程数量
    volatile size_t m_target_worker_count;	///< 目标工作线程数量
    volatile size_t m_active_worker_count;  ///< 当前活动工作线程数量

  private: // 以下成员一次只能被一个线程访问
    scheduler_type  m_scheduler;                                ///< 任务调度器实例
    scoped_ptr<size_policy_type> m_size_policy;                ///< 大小策略智能指针（永不为空）
    
    bool  m_terminate_all_workers;                              ///< 标示是否触发了所有工作线程的终止
    std::vector<shared_ptr<worker_type> > m_terminated_workers; ///< 已终止但未完全销毁的工作线程列表
    
  private: // 以下成员实现了线程安全
    mutable recursive_mutex  m_monitor;                         ///< 递归互斥锁，保护共享资源
    mutable condition m_worker_idle_or_terminated_event;	    ///< 条件变量：工作线程空闲或已终止
    mutable condition m_task_or_terminate_workers_event;        ///< 条件变量：有任务可用或应减少工作线程总数

  public:
    /*!
     * \brief 构造函数
     *
     * 初始化线程池核心对象，设置初始状态并创建大小策略实例。
     * 清空调度器以确保初始状态的一致性。
     */
    pool_core()
      : m_worker_count(0)           // 初始化当前工作线程数为0
      , m_target_worker_count(0)    // 初始化目标工作线程数为0
      , m_active_worker_count(0)    // 初始化活动工作线程数为0
      , m_terminate_all_workers(false)  // 初始化终止标志为false
    {
      // 获取自身的volatile引用用于创建大小策略
      pool_type volatile & self_ref = *this;
      m_size_policy.reset(new size_policy_type(self_ref));

      // 清空调度器，确保初始状态
      m_scheduler.clear();
    }

    /*!
     * \brief 析构函数
     *
     * 销毁线程池核心对象。实际的清理工作由关闭策略处理。
     */
    ~pool_core()
    {
    }

    /*!
     * \brief 获取管理线程池中线程数量的大小控制器
     *
     * 返回用于控制线程池大小的控制器对象。
     *
     * \return 大小控制器实例
     * \see SizePolicy
     */
    size_controller_type size_controller()
    {
      return size_controller_type(*m_size_policy, this->shared_from_this());
    }

    /*!
     * \brief 获取线程池中的线程数量
     *
     * 返回当前线程池中工作线程的总数。
     *
     * \return 线程数量
     */
    size_t size() const volatile
    {
      return m_worker_count;
    }

    /*!
     * \brief 关闭线程池
     *
     * 使用关闭策略来关闭线程池。此方法通常只被调用一次。
     */
    void shutdown()
    {
      ShutdownPolicy<pool_type>::shutdown(*this);
    }

    /*!
     * \brief 调度任务进行异步执行
     *
     * 将任务添加到线程池中进行异步执行，任务只会执行一次。
     * 使用锁定指针确保线程安全的访问调度器。
     *
     * \param task 任务函数对象，不应抛出异常
     * \return 如果任务能够被调度则返回true，否则返回false
     */  
    bool schedule(task_type const & task) volatile
    {	
      // 使用锁定指针确保线程安全访问
      locking_ptr<pool_type, recursive_mutex> lockedThis(*this, m_monitor); 
      
      // 尝试将任务推入调度器
      if(lockedThis->m_scheduler.push(task))
      {
        // 通知一个工作线程有新任务可用
        lockedThis->m_task_or_terminate_workers_event.notify_one();
        return true;
      }
      else
      {
        return false;
      }
    }	

    /*!
     * \brief 返回当前正在执行的任务数量
     *
     * 获取线程池中当前活动（正在执行）的任务数量。
     *
     * \return 活动任务的数量
     */  
    size_t active() const volatile
    {
      return m_active_worker_count;
    }

    /*!
     * \brief 返回准备执行的任务数量
     *
     * 获取调度器中等待执行的任务数量。使用锁定指针确保线程安全访问。
     *
     * \return 待执行任务的数量
     */  
    size_t pending() const volatile
    {
      locking_ptr<const pool_type, recursive_mutex> lockedThis(*this, m_monitor);
      return lockedThis->m_scheduler.size();
    }

    /*!
     * \brief 从线程池的调度器中移除所有待执行任务
     *
     * 清空任务队列，丢弃所有等待执行的任务。正在执行的任务不受影响。
     * 使用锁定指针确保线程安全的清空操作。
     */  
    void clear() volatile
    { 
      locking_ptr<pool_type, recursive_mutex> lockedThis(*this, m_monitor);
      lockedThis->m_scheduler.clear();
    }    

    /*!
     * \brief 指示是否没有待执行的任务
     *
     * 检查线程池是否没有待执行的任务。使用锁定指针确保线程安全访问。
     *
     * \return 如果没有准备执行的任务则返回true，否则返回false
     * \note 此函数比检查'pending() == 0'更高效
     */   
    bool empty() const volatile
    {
      locking_ptr<const pool_type, recursive_mutex> lockedThis(*this, m_monitor);
      return lockedThis->m_scheduler.empty();
    }	


    /*!
     * \brief 阻塞当前线程直到任务数量达到阈值
     *
     * 阻塞当前执行线程，直到所有活动和待执行任务的总数等于或小于给定的阈值。
     * 使用条件变量实现高效的等待机制。
     *
     * \param task_threshold 线程池和调度器中任务的最大数量，默认为0（等待所有任务完成）
     */     
    void wait(size_t const task_threshold = 0) const volatile
    {
      const pool_type* self = const_cast<const pool_type*>(this);
      recursive_mutex::scoped_lock lock(self->m_monitor);

      // 如果阈值为0，等待所有任务完成
      if(0 == task_threshold)
      {
        while(0 != self->m_active_worker_count || !self->m_scheduler.empty())
        { 
          // 等待工作线程空闲或终止事件
          self->m_worker_idle_or_terminated_event.wait(lock);
        }
      }
      else
      {
        // 等待任务总数降到阈值以下
        while(task_threshold < self->m_active_worker_count + self->m_scheduler.size())
        { 
          // 等待工作线程空闲或终止事件
          self->m_worker_idle_or_terminated_event.wait(lock);
        }
      }
    }	

    /*!
     * \brief 带超时的等待任务完成
     *
     * 阻塞当前执行线程，直到达到时间戳或所有活动和待执行任务的总数
     * 等于或小于给定的阈值。提供超时机制避免无限等待。
     *
     * \param timestamp 函数最晚返回的时间戳
     * \param task_threshold 线程池和调度器中任务的最大数量，默认为0
     * \return 如果任务总数等于或小于阈值则返回true，否则返回false（超时）
     */       
    bool wait(xtime const & timestamp, size_t const task_threshold = 0) const volatile
    {
      const pool_type* self = const_cast<const pool_type*>(this);
      recursive_mutex::scoped_lock lock(self->m_monitor);

      // 如果阈值为0，等待所有任务完成
      if(0 == task_threshold)
      {
        while(0 != self->m_active_worker_count || !self->m_scheduler.empty())
        { 
          // 带超时的等待，如果超时则返回false
          if(!self->m_worker_idle_or_terminated_event.timed_wait(lock, timestamp)) return false;
        }
      }
      else
      {
        // 等待任务总数降到阈值以下
        while(task_threshold < self->m_active_worker_count + self->m_scheduler.size())
        { 
          // 带超时的等待，如果超时则返回false
          if(!self->m_worker_idle_or_terminated_event.timed_wait(lock, timestamp)) return false;
        }
      }

      // 成功等到条件满足
      return true;
    }


  private:	

    /*!
     * \brief 终止所有工作线程
     *
     * 设置终止标志并通知所有工作线程停止工作。可选择是否等待
     * 所有线程完全终止。该方法由关闭策略调用。
     *
     * \param wait 如果为true，则等待所有线程完全终止；否则立即返回
     */
    void terminate_all_workers(bool const wait) volatile
    {
      pool_type* self = const_cast<pool_type*>(this);
      recursive_mutex::scoped_lock lock(self->m_monitor);

      // 设置终止所有工作线程的标志
      self->m_terminate_all_workers = true;

      // 将目标工作线程数设为0
      m_target_worker_count = 0;
      // 通知所有工作线程检查终止条件
      self->m_task_or_terminate_workers_event.notify_all();

      // 如果需要等待线程完全终止
      if(wait)
      {
        // 等待所有活动工作线程终止
        while(m_active_worker_count > 0)
        {
          self->m_worker_idle_or_terminated_event.wait(lock);
        }

        // 等待所有已终止的工作线程完全销毁
        for(typename std::vector<shared_ptr<worker_type> >::iterator it = self->m_terminated_workers.begin();
          it != self->m_terminated_workers.end();
          ++it)
        {
          (*it)->join();  // 等待线程结束
        }
        // 清空已终止线程列表
        self->m_terminated_workers.clear();
      }
    }

    /*!
     * \brief 调整线程池中工作线程的数量
     *
     * 改变线程池中工作线程的数量。调整大小的具体策略由SizePolicy处理。
     * 支持增加和减少工作线程数量。
     *
     * \param worker_count 新的工作线程数量
     * \return 如果线程池将被调整大小则返回true，否则返回false
     */
    bool resize(size_t const worker_count) volatile
    {
      locking_ptr<pool_type, recursive_mutex> lockedThis(*this, m_monitor); 

      // 检查是否已经开始终止所有工作线程
      if(!m_terminate_all_workers)
      {
        // 设置新的目标工作线程数量
        m_target_worker_count = worker_count;
      }
      else
      { 
        // 如果正在终止，则不允许调整大小
        return false;
      }

      // 如果需要增加工作线程数量
      if(m_worker_count <= m_target_worker_count)
      { 
        // 创建新的工作线程直到达到目标数量
        while(m_worker_count < m_target_worker_count)
        {
          try
          {
            // 创建并附加新的工作线程
            worker_thread<pool_type>::create_and_attach(lockedThis->shared_from_this());
            m_worker_count++;       // 增加工作线程计数
            m_active_worker_count++;// 增加活动线程计数
          }
          catch(thread_resource_error)
          {
            // 如果线程资源不足，返回失败
            return false;
          }
        }
      }
      else
      { 
        // 如果需要减少工作线程数量，通知工作线程检查终止条件
        // TODO: 优化通知的工作线程数量
        lockedThis->m_task_or_terminate_workers_event.notify_all();
      }

      return true;
    }


    /*!
     * \brief 处理工作线程意外死亡
     *
     * 当工作线程因未处理的异常而意外死亡时调用此方法。
     * 更新线程计数并通知大小策略处理这种异常情况。
     *
     * \param worker 意外死亡的工作线程智能指针
     */
    void worker_died_unexpectedly(shared_ptr<worker_type> worker) volatile
    {
      locking_ptr<pool_type, recursive_mutex> lockedThis(*this, m_monitor);

      // 减少工作线程计数
      m_worker_count--;
      m_active_worker_count--;
      // 通知等待的线程有工作线程死亡
      lockedThis->m_worker_idle_or_terminated_event.notify_all();	

      if(m_terminate_all_workers)
      {
        // 如果正在终止所有工作线程，将死亡的线程加入终止列表
        lockedThis->m_terminated_workers.push_back(worker);
      }
      else
      {
        // 否则通知大小策略处理工作线程意外死亡
        lockedThis->m_size_policy->worker_died_unexpectedly(m_worker_count);
      }
    }

    /*!
     * \brief 处理工作线程正常销毁
     *
     * 当工作线程正常完成并准备销毁时调用此方法。
     * 更新线程计数并处理终止列表。
     *
     * \param worker 要销毁的工作线程智能指针
     */
    void worker_destructed(shared_ptr<worker_type> worker) volatile
    {
      locking_ptr<pool_type, recursive_mutex> lockedThis(*this, m_monitor);
      
      // 减少工作线程计数
      m_worker_count--;
      m_active_worker_count--;
      // 通知等待的线程有工作线程终止
      lockedThis->m_worker_idle_or_terminated_event.notify_all();	

      if(m_terminate_all_workers)
      {
        // 如果正在终止所有工作线程，将销毁的线程加入终止列表
        lockedThis->m_terminated_workers.push_back(worker);
      }
    }

    /*!
     * \brief 执行单个任务
     *
     * 工作线程的主要执行函数。从调度器中获取任务并执行。
     * 如果没有任务可用，则等待新任务或终止信号。
     * 根据线程池状态决定是否继续执行或终止工作线程。
     *
     * \return 如果成功执行任务则返回true，如果工作线程应该终止则返回false
     */
    bool execute_task() volatile
    {
      function0<void> task;

      { // 获取任务的作用域
        pool_type* lockedThis = const_cast<pool_type*>(this);
        recursive_mutex::scoped_lock lock(lockedThis->m_monitor);

        // 如果需要减少线程数量，终止当前工作线程
        if(m_worker_count > m_target_worker_count)
        {	
          return false;	// 终止工作线程
        }

        // 等待任务可用
        while(lockedThis->m_scheduler.empty())
        {	
          // 再次检查是否需要减少工作线程数量
          if(m_worker_count > m_target_worker_count)
          {	
            return false;	// 终止工作线程
          }
          else
          {
            // 标记工作线程为空闲状态
            m_active_worker_count--;
            lockedThis->m_worker_idle_or_terminated_event.notify_all();
            
            // 等待新任务或终止信号
            lockedThis->m_task_or_terminate_workers_event.wait(lock);
            
            // 标记工作线程为活动状态
            m_active_worker_count++;
          }
        }

        // 从调度器中获取任务
        task = lockedThis->m_scheduler.top();
        lockedThis->m_scheduler.pop();
      }

      // 执行任务函数
      if(task)
      {
        task();
      }
 
      // 注释的守卫禁用代码，预留给将来的扩展
      //guard->disable();
      return true;
    }
  };




} } } // namespace boost::threadpool::detail

#endif // THREADPOOL_POOL_CORE_HPP_INCLUDED
