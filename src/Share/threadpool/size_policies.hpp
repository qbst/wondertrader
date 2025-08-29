/*!
* \file size_policies.hpp
* \brief 线程池大小策略类
*
* 该文件包含了线程池的大小策略实现。大小策略控制线程池中工作线程的数量，
* 决定了线程池如何响应负载变化和线程异常退出等情况。
*
* 设计逻辑：
* - 使用策略模式实现不同的线程数量管理策略
* - 提供静态和动态的线程池大小控制
* - 支持线程异常处理和自动恢复机制
* - 实现线程池大小的运行时调整功能
*
* 作用：
* - 控制线程池的初始大小和运行时大小
* - 处理工作线程的异常退出情况
* - 提供线程池大小调整的接口
* - 实现不同的线程管理策略
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

#ifndef THREADPOOL_SIZE_POLICIES_HPP_INCLUDED
#define THREADPOOL_SIZE_POLICIES_HPP_INCLUDED

/// 线程池命名空间，包含线程池和相关实用工具类
namespace boost { namespace threadpool
{

  /*!
   * \brief 空的大小策略控制器
   *
   * 该控制器不提供任何功能，用于不需要动态调整线程池大小的场景。
   * 它是一个占位符实现，符合策略模式的接口要求但不执行任何操作。
   *
   * \tparam Pool 线程池的核心类型
   */ 
  template<typename Pool>
  struct empty_controller
  {
    /*!
     * \brief 构造函数
     *
     * 创建一个空的控制器，不执行任何操作。
     *
     * \param policy 大小策略引用（未使用）
     * \param pool 线程池智能指针（未使用）
     */
    empty_controller(typename Pool::size_policy_type&, shared_ptr<Pool>) {}
  };

  /*!
   * \brief 支持调整大小的策略控制器
   *
   * 该控制器允许在运行时动态调整线程池的大小。它持有对大小策略的引用
   * 和对线程池的智能指针，确保在控制器存在期间线程池保持有效。
   *
   * \tparam Pool 线程池的核心类型
   */ 
  template< typename Pool >
  class resize_controller
  {
    typedef typename Pool::size_policy_type size_policy_type;   ///< 大小策略类型定义
    reference_wrapper<size_policy_type> m_policy;              ///< 大小策略的引用包装器
    shared_ptr<Pool> m_pool;                                   ///< 线程池智能指针，确保线程池在控制器存在期间保持有效

  public:
    /*!
     * \brief 构造函数
     *
     * 创建一个可调整大小的控制器。
     *
     * \param policy 大小策略引用
     * \param pool 线程池智能指针
     */
    resize_controller(size_policy_type& policy, shared_ptr<Pool> pool)
      : m_policy(policy)
      , m_pool(pool)
    {
    }

    /*!
     * \brief 调整线程池大小
     *
     * 通过大小策略调整线程池中工作线程的数量。
     *
     * \param worker_count 新的工作线程数量
     * \return true表示调整成功，false表示调整失败
     */
    bool resize(size_t worker_count)
    {
      return m_policy.get().resize(worker_count);
    }
  };

  /*!
   * \brief 静态大小策略
   *
   * 该策略保持线程数量相对稳定，但支持手动调整和异常恢复。
   * 当工作线程意外退出时，会自动创建新线程以维持线程数量。
   *
   * \tparam Pool 线程池的核心类型
   */ 
  template<typename Pool>
  class static_size
  {
    reference_wrapper<Pool volatile> m_pool;  ///< 线程池的引用包装器（volatile确保线程安全）

  public:
    /*!
     * \brief 初始化线程池大小
     *
     * 静态方法，用于初始化线程池的工作线程数量。
     *
     * \param pool 线程池引用
     * \param worker_count 初始工作线程数量
     */
    static void init(Pool& pool, size_t const worker_count)
    {
      pool.resize(worker_count);  // 调整线程池大小到指定数量
    }

    /*!
     * \brief 构造函数
     *
     * 创建一个静态大小策略对象。
     *
     * \param pool 线程池的volatile引用
     */
    static_size(Pool volatile & pool)
      : m_pool(pool)
    {}

    /*!
     * \brief 调整线程池大小
     *
     * 手动调整线程池中工作线程的数量。
     *
     * \param worker_count 新的工作线程数量
     * \return true表示调整成功，false表示调整失败
     */
    bool resize(size_t const worker_count)
    {
      return m_pool.get().resize(worker_count);
    }

    /*!
     * \brief 处理工作线程意外退出
     *
     * 当工作线程意外退出时调用此方法。策略会自动创建一个新线程
     * 来替换退出的线程，以维持线程池的稳定性。
     *
     * \param new_worker_count 当前的工作线程数量
     */
    void worker_died_unexpectedly(size_t const new_worker_count)
    {
      m_pool.get().resize(new_worker_count + 1);  // 创建新线程替换退出的线程
    }

    /*!
     * \brief 任务调度通知（待实现）
     *
     * 当有任务被调度时调用。目前为空实现，预留给将来的扩展。
     */
    void task_scheduled() {}

    /*!
     * \brief 任务完成通知（待实现）
     *
     * 当任务完成时调用。目前为空实现，预留给将来的扩展。
     */
    void task_finished() {}
  };

} } // namespace boost::threadpool

#endif // THREADPOOL_SIZE_POLICIES_HPP_INCLUDED
