/*!
* \file worker_thread.hpp
* \brief 线程池工作线程类
*
* 工作线程实例附属于线程池并执行该线程池的任务。工作线程代表一个执行线程，
* 其生命周期和内部boost::thread的生命周期都是自动管理的。
*
* 设计逻辑：
* - 实现工作线程的生命周期管理
* - 提供任务执行的主循环逻辑
* - 使用RAII模式处理异常和资源清理
* - 支持线程的创建、运行和销毁
*
* 作用：
* - 执行线程池分配的任务
* - 处理任务执行过程中的异常情况
* - 管理线程的生命周期和状态
* - 提供线程间的同步和通信机制
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

#ifndef THREADPOOL_DETAIL_WORKER_THREAD_HPP_INCLUDED
#define THREADPOOL_DETAIL_WORKER_THREAD_HPP_INCLUDED

// 引入作用域守卫类
#include "scope_guard.hpp"

// 引入Boost智能指针库
#include <boost/smart_ptr.hpp>
// 引入Boost线程库
#include <boost/thread.hpp>
// 引入Boost线程异常处理
#include <boost/thread/exceptions.hpp>
// 引入Boost互斥锁库
#include <boost/thread/mutex.hpp>
// 引入Boost函数绑定库
#include <boost/bind.hpp>

// 线程池详细实现命名空间
namespace boost { namespace threadpool { namespace detail 
{

  /*!
   * \brief 线程池工作线程类
   *
   * worker_thread代表一个执行线程。工作线程附属于线程池并处理该线程池的任务。
   * 工作线程及其内部boost::thread的生命周期都是自动管理的。
   *
   * 该类是一个辅助类，不能直接构造或访问。工作线程通过静态方法创建并附加到线程池。
   *
   * \tparam Pool 线程池的类型
   * 
   * \see pool_core
   */ 
  template <typename Pool>
  class worker_thread
  : public enable_shared_from_this< worker_thread<Pool> > 
  , private noncopyable
  {
  public:
    /// 线程池类型定义
    typedef Pool pool_type;

  private:
    shared_ptr<pool_type>      m_pool;     ///< 指向创建该工作线程的线程池的智能指针
    shared_ptr<boost::thread>  m_thread;   ///< 指向执行运行循环的线程的智能指针

    /*!
     * \brief 私有构造函数
     *
     * 构造一个新的工作线程。该构造函数是私有的，只能通过静态方法创建工作线程。
     *
     * \param pool 指向父线程池的智能指针
     * \see create_and_attach函数
     */
    worker_thread(shared_ptr<pool_type> const & pool)
    : m_pool(pool)
    {
      assert(pool);  // 确保线程池指针有效
    }

    /*!
     * \brief 通知运行循环中发生异常
     *
     * 当工作线程在执行任务时发生未捕获的异常时调用此方法。
     * 该方法会通知线程池工作线程意外死亡，以便线程池采取相应的恢复措施。
     */
	void died_unexpectedly()
	{
		m_pool->worker_died_unexpectedly(this->shared_from_this());
	}

  public:
	  /*!
	   * \brief 顺序执行线程池的任务
	   *
	   * 工作线程的主要执行函数。该函数会持续从线程池中获取并执行任务，
	   * 直到线程池指示停止。使用作用域守卫确保在异常情况下也能正确
	   * 通知线程池工作线程的状态变化。
	   */
	  void run()
	  { 
		  // 创建异常通知守卫，确保在异常情况下通知线程池
		  scope_guard notify_exception(bind(&worker_thread::died_unexpectedly, this));

		  // 持续执行任务直到线程池指示停止
		  while(m_pool->execute_task()) {}

		  // 正常退出时禁用异常通知守卫
		  notify_exception.disable();
		  // 通知线程池工作线程正常销毁
		  m_pool->worker_destructed(this->shared_from_this());
	  }

	  /*!
	   * \brief 等待工作线程结束
	   *
	   * 阻塞调用线程直到该工作线程完成执行。这是线程同步的标准方法，
	   * 确保在销毁工作线程对象前所有任务都已完成。
	   */
	  void join()
	  {
		  m_thread->join();
	  }

	  /*!
	   * \brief 创建新的工作线程并将其附加到线程池
	   *
	   * 静态工厂方法，用于创建新的工作线程实例并将其附加到指定的线程池。
	   * 该方法创建工作线程对象和底层的boost::thread，并启动线程执行。
	   *
	   * \param pool 指向线程池的智能指针
	   */
	  static void create_and_attach(shared_ptr<pool_type> const & pool)
	  {
		  // 创建新的工作线程实例
		  shared_ptr<worker_thread> worker(new worker_thread(pool));
		  if(worker)
		  {
			  // 创建并启动底层的boost::thread
			  worker->m_thread.reset(new boost::thread(bind(&worker_thread::run, worker)));
		  }
	  }

  };


} } } // namespace boost::threadpool::detail

#endif // THREADPOOL_DETAIL_WORKER_THREAD_HPP_INCLUDED

