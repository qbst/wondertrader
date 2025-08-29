/*!
* \file future.hpp
* \brief 线程池异步任务结果处理类
*
* 该文件实现了线程池的异步任务结果处理机制，提供了Future模式的实现。
* Future允许异步任务的提交者在稍后的时间点获取任务的执行结果，实现了
* 任务提交与结果获取的解耦。
*
* 设计逻辑：
* - 使用Future模式处理异步任务结果
* - 支持任务取消和状态查询
* - 提供同步和超时等待机制
* - 通过模板实现类型安全的结果传递
*
* 作用：
* - 异步任务结果的容器和访问接口
* - 支持任务执行状态的查询和控制
* - 提供线程安全的结果获取机制
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

#ifndef THREADPOOL_FUTURE_HPP_INCLUDED
#define THREADPOOL_FUTURE_HPP_INCLUDED

// 引入Future实现的详细定义
#include "./detail/future.hpp"
// 引入Boost的enable_if工具，用于模板特化
#include <boost/utility/enable_if.hpp>

// 线程池命名空间
namespace boost { namespace threadpool
{

  /*!
  * \brief 异步任务结果容器类（实验性功能）
  *
  * Future类提供了异步任务结果的访问接口，允许调用者在任务执行完成后
  * 获取结果。支持任务状态查询、结果等待和任务取消等功能。
  *
  * \tparam Result 任务返回值的类型
  *
  * \note 这是实验性功能，不建议在生产环境中使用
  */ 
template<class Result> 
class future
{
private:
  /// Future实现的智能指针，使用Pimpl模式隐藏实现细节
  shared_ptr<detail::future_impl<Result> > m_impl;

public:
    /// 结果类型定义，返回常量引用以避免拷贝
    typedef Result const & result_type;
    /// Future结果类型定义
    typedef Result future_result_type;

public:
  /*!
   * \brief 默认构造函数
   * 
   * 创建一个新的Future对象，内部创建对应的实现对象。
   * 
   * \note 这个构造函数主要用于内部使用，外部代码应该通过
   *       schedule函数获得Future对象
   */
  future()
  : m_impl(new detail::future_impl<future_result_type>())
  {
  }

  /*!
   * \brief 内部使用的构造函数
   * 
   * 使用已存在的实现对象创建Future，主要用于内部实现。
   * 
   * \param impl Future实现对象的智能指针
   */
  future(shared_ptr<detail::future_impl<Result> > const & impl)
  : m_impl(impl)
  {
  }

  /*!
   * \brief 检查任务是否已完成
   * 
   * 非阻塞地检查异步任务是否已经执行完成。
   * 
   * \return true表示任务已完成，false表示任务仍在执行中
   */
  bool ready() const
  {
    return m_impl->ready();
  }

  /*!
   * \brief 等待任务完成
   * 
   * 阻塞当前线程直到异步任务执行完成。如果任务已经完成，
   * 则立即返回。
   */
  void wait() const
  {
    m_impl->wait();
  }

  /*!
   * \brief 带超时的等待任务完成
   * 
   * 阻塞当前线程直到异步任务执行完成或者超时。
   * 
   * \param timestamp 超时时间戳
   * \return true表示任务在超时前完成，false表示超时
   */
  bool timed_wait(boost::xtime const & timestamp) const
  {
    return m_impl->timed_wait(timestamp);
  }

  /*!
   * \brief 获取任务执行结果（函数调用操作符）
   * 
   * 获取异步任务的执行结果。如果任务尚未完成，将阻塞等待。
   * 
   * \return 任务执行的结果
   * \note 可能抛出线程取消异常等
   */
   result_type operator()()
   {
     return (*m_impl)();
   }

  /*!
   * \brief 获取任务执行结果
   * 
   * 获取异步任务的执行结果。如果任务尚未完成，将阻塞等待。
   * 这是获取结果的标准方法。
   * 
   * \return 任务执行的结果
   * \note 可能抛出线程取消异常等
   */
   result_type get()
   {
     return (*m_impl)();
   }

  /*!
   * \brief 取消任务执行
   * 
   * 尝试取消异步任务的执行。如果任务尚未开始执行或正在执行中，
   * 可以被取消。
   * 
   * \return true表示取消成功，false表示取消失败（任务可能已完成）
   */
   bool cancel()
   {
     return m_impl->cancel();
   }

  /*!
   * \brief 检查任务是否已被取消
   * 
   * 检查异步任务是否已经被取消。
   * 
   * \return true表示任务已被取消，false表示任务未被取消
   */
   bool is_cancelled() const
   {
     return m_impl->is_cancelled();
   }
};





/*!
 * \brief 调度异步任务并返回Future对象
 * 
 * 这是一个模板函数，用于将任务提交到线程池并返回对应的Future对象。
 * 只有当任务函数的返回类型不是void时才会启用此函数（使用SFINAE技术）。
 * 
 * \tparam Pool 线程池类型
 * \tparam Function 任务函数类型
 * \param pool 线程池引用
 * \param task 要执行的任务函数
 * \return 与任务关联的Future对象，用于获取执行结果
 * 
 * \note 使用SFINAE（Substitution Failure Is Not An Error）技术，
 *       只有当Function的返回类型不是void时才启用此函数
 */
template<class Pool, class Function>
typename disable_if < 
  is_void< typename result_of< Function() >::type >,
  future< typename result_of< Function() >::type >
>::type
schedule(Pool& pool, const Function& task)
{
  // 定义Future结果类型
  typedef typename result_of< Function() >::type future_result_type;

  // 创建Future实现对象和Future对象
  shared_ptr<detail::future_impl<future_result_type> > impl(new detail::future_impl<future_result_type>);
  future <future_result_type> res(impl);

  // 将任务包装成Future任务函数并提交到线程池
  pool.schedule(detail::future_impl_task_func<detail::future_impl, Function>(task, impl));

  // 返回Future对象供调用者使用
  return res;
}



} } // namespace boost::threadpool

#endif // THREADPOOL_FUTURE_HPP_INCLUDED

