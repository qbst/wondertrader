/*!
* \file scheduling_policies.hpp
* \brief 任务调度策略类
*
* 该文件包含了线程池类的一些基本调度策略。调度策略通过任务容器来实现，
* 该容器控制对任务的访问。从根本上说，容器决定了线程池处理任务的顺序。
* 任务容器不需要是线程安全的，因为它们被线程池以线程安全的方式使用。
*
* 设计逻辑：
* - 实现多种任务调度策略（FIFO、LIFO、优先级）
* - 使用策略模式实现可插拔的调度算法
* - 封装底层容器的访问接口
* - 提供统一的任务管理操作
*
* 作用：
* - 定义任务执行的顺序规则
* - 支持不同的调度需求和场景
* - 提供高效的任务存储和检索机制
* - 实现可扩展的调度策略框架
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


#ifndef THREADPOOL_SCHEDULING_POLICIES_HPP_INCLUDED
#define THREADPOOL_SCHEDULING_POLICIES_HPP_INCLUDED


// 引入标准队列容器
#include <queue>
// 引入标准双端队列容器
#include <deque>

// 引入任务适配器定义
#include "task_adaptors.hpp"

namespace boost { namespace threadpool
{

  /*!
   * \brief 实现FIFO（先进先出）排序的调度策略
   *
   * 该容器实现了FIFO调度策略。第一个添加到调度器的任务将是第一个被移除的任务。
   * 处理按相同的顺序依次进行。FIFO代表"先进先出"。
   *
   * \tparam Task 实现operator()(void)的函数对象类型
   */ 
  template <typename Task = task_func>  
  class fifo_scheduler
  {
  public:
    /// 调度器的任务类型定义
    typedef Task task_type;

  protected:
    /// 内部任务容器，使用双端队列实现FIFO语义
    std::deque<task_type> m_container;

  public:
    /*!
     * \brief 向调度器添加新任务
     *
     * 将任务添加到队列的末尾，遵循FIFO原则。
     *
     * \param task 要添加的任务对象
     * \return 如果任务能够被调度则返回true，否则返回false
     */
    bool push(task_type const & task)
    {
      m_container.push_back(task);  // 添加到队列末尾
      return true;
    }

    /*!
     * \brief 移除下一个应该执行的任务
     *
     * 从队列的前端移除任务，遵循FIFO原则。
     */
    void pop()
    {
      m_container.pop_front();  // 从队列前端移除
    }

    /*!
     * \brief 获取下一个应该执行的任务
     *
     * 返回队列前端的任务对象，但不移除它。
     *
     * \return 要执行的任务对象引用
     */
    task_type const & top() const
    {
      return m_container.front();  // 返回队列前端的任务
    }

    /*!
     * \brief 获取调度器中当前的任务数量
     *
     * 返回队列中等待执行的任务总数。
     *
     * \return 任务数量
     * \note 建议使用empty()而不是size() == 0来检查调度器是否为空
     */
    size_t size() const
    {
      return m_container.size();
    }

    /*!
     * \brief 检查调度器是否为空
     *
     * 检查队列中是否还有待执行的任务。
     *
     * \return 如果调度器不包含任务则返回true，否则返回false
     * \note 比size() == 0更高效
     */
    bool empty() const
    {
      return m_container.empty();
    }

    /*!
     * \brief 从调度器中移除所有任务
     *
     * 清空任务队列，丢弃所有等待执行的任务。
     */  
    void clear()
    {   
      m_container.clear();
    } 
  };



  /*!
   * \brief 实现LIFO（后进先出）排序的调度策略
   *
   * 该容器实现了LIFO调度策略。最后添加到调度器的任务将是第一个被移除的任务。
   * LIFO代表"后进先出"，类似于栈的行为。
   *
   * \tparam Task 实现operator()(void)的函数对象类型
   */ 
  template <typename Task = task_func>  
  class lifo_scheduler
  {
  public:
    /// 调度器的任务类型定义
    typedef Task task_type;

  protected:
    /// 内部任务容器，使用双端队列实现LIFO语义
    std::deque<task_type> m_container;

  public:
    /*!
     * \brief 向调度器添加新任务
     *
     * 将任务添加到队列的前端，遵循LIFO原则。
     *
     * \param task 要添加的任务对象
     * \return 如果任务能够被调度则返回true，否则返回false
     */
    bool push(task_type const & task)
    {
      m_container.push_front(task);  // 添加到队列前端（栈顶）
      return true;
    }

    /*!
     * \brief 移除下一个应该执行的任务
     *
     * 从队列的前端移除任务，遵循LIFO原则。
     */
    void pop()
    {
      m_container.pop_front();  // 从队列前端移除（栈顶弹出）
    }

    /*!
     * \brief 获取下一个应该执行的任务
     *
     * 返回队列前端的任务对象，但不移除它。
     *
     * \return 要执行的任务对象引用
     */
    task_type const & top() const
    {
      return m_container.front();  // 返回队列前端的任务（栈顶）
    }

    /*!
     * \brief 获取调度器中当前的任务数量
     *
     * 返回队列中等待执行的任务总数。
     *
     * \return 任务数量
     * \note 建议使用empty()而不是size() == 0来检查调度器是否为空
     */
    size_t size() const
    {
      return m_container.size();
    }

    /*!
     * \brief 检查调度器是否为空
     *
     * 检查队列中是否还有待执行的任务。
     *
     * \return 如果调度器不包含任务则返回true，否则返回false
     * \note 比size() == 0更高效
     */
    bool empty() const
    {
      return m_container.empty();
    }

    /*!
     * \brief 从调度器中移除所有任务
     *
     * 清空任务队列，丢弃所有等待执行的任务。
     */  
    void clear()
    {    
      m_container.clear();
    } 

  };



  /*!
   * \brief 实现基于优先级排序的调度策略
   *
   * 该容器实现了基于任务优先级的调度策略。具有最高优先级的任务将是第一个被移除的任务。
   * 任务对象必须支持使用operator<进行比较。operator<必须实现偏序关系。
   *
   * \tparam Task 实现operator()和operator<的函数对象类型。operator<必须是偏序关系
   *
   * \see prio_task_func
   */ 
  template <typename Task = prio_task_func>  
  class prio_scheduler
  {
  public:
    /// 调度器的任务类型定义
    typedef Task task_type;

  protected:
    /// 内部任务容器，使用优先级队列实现基于优先级的调度
    std::priority_queue<task_type> m_container;

  public:
    /*!
     * \brief 向调度器添加新任务
     *
     * 将任务添加到优先级队列中，任务会根据其优先级自动排序。
     *
     * \param task 要添加的任务对象
     * \return 如果任务能够被调度则返回true，否则返回false
     */
    bool push(task_type const & task)
    {
      m_container.push(task);  // 添加到优先级队列，自动按优先级排序
      return true;
    }

    /*!
     * \brief 移除下一个应该执行的任务
     *
     * 从优先级队列中移除优先级最高的任务。
     */
    void pop()
    {
      m_container.pop();  // 移除优先级最高的任务
    }

    /*!
     * \brief 获取下一个应该执行的任务
     *
     * 返回优先级最高的任务对象，但不移除它。
     *
     * \return 要执行的任务对象引用
     */
    task_type const & top() const
    {
      return m_container.top();  // 返回优先级最高的任务
    }

    /*!
     * \brief 获取调度器中当前的任务数量
     *
     * 返回优先级队列中等待执行的任务总数。
     *
     * \return 任务数量
     * \note 建议使用empty()而不是size() == 0来检查调度器是否为空
     */
    size_t size() const
    {
      return m_container.size();
    }

    /*!
     * \brief 检查调度器是否为空
     *
     * 检查优先级队列中是否还有待执行的任务。
     *
     * \return 如果调度器不包含任务则返回true，否则返回false
     * \note 比size() == 0更高效
     */
    bool empty() const
    {
      return m_container.empty();
    }

    /*!
     * \brief 从调度器中移除所有任务
     *
     * 清空优先级队列，丢弃所有等待执行的任务。由于std::priority_queue
     * 不提供clear()方法，所以通过循环弹出所有元素来实现清空。
     */  
    void clear()
    {    
      // 循环弹出所有任务直到队列为空
      while(!m_container.empty())
      {
        m_container.pop();
      }
    } 
  };


} } // namespace boost::threadpool


#endif // THREADPOOL_SCHEDULING_POLICIES_HPP_INCLUDED

