/*!
* \file task_adaptors.hpp
* \brief 任务适配器类
*
* 该文件包含了任务函数对象的适配器实现。提供了多种任务类型的包装，
* 包括普通任务、优先级任务和循环任务等。
*
* 设计逻辑：
* - 使用适配器模式封装不同类型的任务
* - 提供统一的任务执行接口
* - 支持任务优先级和循环执行
* - 使用Boost.Function实现类型擦除
*
* 作用：
* - 定义标准任务函数类型
* - 实现优先级任务的比较和执行
* - 支持循环任务的定时执行
* - 提供类型安全的任务封装
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


#ifndef THREADPOOL_TASK_ADAPTERS_HPP_INCLUDED
#define THREADPOOL_TASK_ADAPTERS_HPP_INCLUDED


// 引入Boost智能指针库
#include <boost/smart_ptr.hpp>
// 引入Boost函数对象库
#include <boost/function.hpp>
// 引入Boost线程库
#include <boost/thread.hpp>
// 引入时间处理头文件
#include <time.h>

namespace boost { namespace threadpool
{

  /*!
  * \brief 标准任务函数对象
  *
  * 该函数对象封装了一个返回void的无参数函数。
  * 通过调用operator()来调用被封装的函数。
  *
  * \see boost function库
  *
  */ 
  typedef function0<void> task_func;




  /*!
  * \brief 优先级任务函数对象
  *
  * 该函数对象封装了一个task_func对象并为其绑定了优先级。
  * prio_task_func可以使用operator<进行比较，实现了偏序关系。
  * 通过调用operator()来调用被封装的任务函数。
  *
  * \see prio_scheduler
  *
  */ 
  class prio_task_func
  {
  private:
    unsigned int m_priority;  ///< 任务函数的优先级
    task_func m_function;     ///< 任务函数对象

  public:
    /// 表示函数对象的返回类型
    typedef void result_type;

  public:
    /*!
     * \brief 构造函数
     * 
     * 创建一个带优先级的任务函数对象。
     * 
     * \param priority 任务的优先级值，数值越大优先级越高
     * \param function 要执行的任务函数对象
     */
    prio_task_func(unsigned int const priority, task_func const & function)
      : m_priority(priority)
      , m_function(function)
    {
    }

    /*!
     * \brief 执行任务函数
     * 
     * 调用封装的任务函数。如果函数对象有效，则执行它。
     */
    void operator() (void) const
    {
      if(m_function)
      {
        m_function();
      }
    }

    /*!
     * \brief 比较操作符，基于优先级实现偏序关系
     * 
     * 用于在优先级队列中进行任务排序。注意这里使用的是小于比较，
     * 在std::priority_queue中会产生最大堆的效果。
     * 
     * \param rhs 要比较的右操作数对象
     * \return 如果当前对象的优先级小于右操作数的优先级则返回true，否则返回false
     */
    bool operator< (const prio_task_func& rhs) const
    {
      return m_priority < rhs.m_priority; 
    }

  };  // prio_task_func



 




  /*!
  * \brief 循环任务函数对象
  *
  * 该函数对象封装了一个返回布尔值的线程函数对象。通过调用operator()来调用
  * 被封装的任务函数，并且它会以固定的时间间隔执行，直到返回false为止。
  * 间隔长度可以为零。请注意，只要任务在循环中，线程池的一个线程就会被占用。
  *
  */ 
  class looped_task_func
  {
  private:
    function0<bool> m_function;   ///< 任务函数对象，返回bool值控制是否继续循环
    unsigned int m_break_s;       ///< 间隔时间的秒数部分
    unsigned int m_break_ns;      ///< 间隔时间的纳秒数部分

  public:
    /// 表示函数对象的返回类型
    typedef void result_type;

  public:
    /*!
     * \brief 构造函数
     * 
     * 创建一个循环任务函数对象。
     * 
     * \param function 要循环执行的任务函数对象，返回false时停止循环
     * \param interval 最小间隔时间（毫秒），在第一次执行任务函数之前和后续执行之间的等待时间
     */
    looped_task_func(function0<bool> const & function, unsigned int const interval = 0)
      : m_function(function)
    {
      // 将毫秒转换为秒和纳秒
      m_break_s  = interval / 1000;
      m_break_ns = (interval - m_break_s * 1000) * 1000 * 1000;
    }

    /*!
     * \brief 执行循环任务函数
     * 
     * 执行封装的任务函数。首先等待指定的间隔时间，然后循环执行任务函数
     * 直到函数返回false。每次循环之间都会等待指定的间隔时间，如果间隔
     * 为0则调用thread::yield()让出CPU时间片。
     */
    void operator() (void) const
    {
      if(m_function)
      {
        // 第一次执行前先等待指定时间
        if(m_break_s > 0 || m_break_ns > 0)
        {
          xtime xt;
          xtime_get(&xt, boost::TIME_UTC_);
          xt.nsec += m_break_ns;
          xt.sec += m_break_s;
          thread::sleep(xt); 
        }

        // 循环执行任务直到函数返回false
        while(m_function())
        {
          if(m_break_s > 0 || m_break_ns > 0)
          {
            // 等待指定的间隔时间
            xtime xt;
            xtime_get(&xt, boost::TIME_UTC_);
            xt.nsec += m_break_ns;
            xt.sec += m_break_s;
            thread::sleep(xt); 
          }
          else
          {
            // 如果没有间隔时间，让出CPU时间片给其他线程
            thread::yield();
          }
        }
      }
    }

  }; // looped_task_func


} } // namespace boost::threadpool

#endif // THREADPOOL_TASK_ADAPTERS_HPP_INCLUDED

