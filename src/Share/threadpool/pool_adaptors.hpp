/*!
* \file pool_adaptors.hpp
* \brief 线程池适配器类
*
* 该文件包含了线程池的便利适配器函数，提供了类似智能指针的易用接口。
* 这些适配器函数简化了任务的调度操作，提供了多种任务提交方式。
*
* 设计逻辑：
* - 提供便利的任务调度函数重载
* - 支持Runnable对象的直接调度
* - 使用SFINAE技术进行模板特化
* - 简化线程池的使用接口
*
* 作用：
* - 简化任务提交的接口调用
* - 支持不同类型的任务对象调度
* - 提供类型安全的模板函数
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

#ifndef THREADPOOL_POOL_ADAPTORS_HPP_INCLUDED
#define THREADPOOL_POOL_ADAPTORS_HPP_INCLUDED

// 引入Boost智能指针库
#include <boost/smart_ptr.hpp>

// 线程池命名空间
namespace boost { namespace threadpool
{

    /*!
     * \brief 调度Runnable对象进行异步执行
     * 
     * 调度一个实现了run()成员函数的Runnable对象。这是一个便利函数，
     * 等价于pool->schedule(bind(&Runnable::run, task_object))。
     * 
     * \tparam Pool 线程池类型
     * \tparam Runnable 可运行对象类型，必须有run()成员函数
     * \param pool 线程池引用
     * \param obj Runnable对象的智能指针，其run()方法将被执行且不应抛出异常
     * \return true表示任务调度成功，false表示调度失败
     */  
    template<typename Pool, typename Runnable>
    bool schedule(Pool& pool, shared_ptr<Runnable> const & obj)
    {	
      return pool->schedule(bind(&Runnable::run, obj));
    }	
    
    /*!
     * \brief 调度任务进行异步执行（返回void的任务）
     * 
     * 调度一个任务函数进行异步执行，任务只会执行一次。
     * 使用SFINAE技术确保只有返回void的任务函数才会匹配此重载。
     * 
     * \tparam Pool 线程池类型
     * \param pool 线程池引用
     * \param task 任务函数对象
     * \return true表示任务调度成功，false表示调度失败
     */  
    template<typename Pool>
    typename enable_if < 
      is_void< typename result_of< typename Pool::task_type() >::type >,
      bool
    >::type
    schedule(Pool& pool, typename Pool::task_type const & task)
    {	
      return pool.schedule(task);
    }	

    /*!
     * \brief 通过智能指针调度任务进行异步执行（返回void的任务）
     * 
     * 通过线程池的智能指针调度一个任务函数进行异步执行。
     * 使用SFINAE技术确保只有返回void的任务函数才会匹配此重载。
     * 
     * \tparam Pool 线程池类型
     * \param pool 线程池的智能指针
     * \param task 任务函数对象
     * \return true表示任务调度成功，false表示调度失败
     */
    template<typename Pool>
    typename enable_if < 
      is_void< typename result_of< typename Pool::task_type() >::type >,
      bool
    >::type
    schedule(shared_ptr<Pool> const pool, typename Pool::task_type const & task)
    {	
      return pool->schedule(task);
    }	

} } // namespace boost::threadpool

#endif // THREADPOOL_POOL_ADAPTORS_HPP_INCLUDED


