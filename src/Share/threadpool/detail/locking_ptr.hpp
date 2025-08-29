/*!
* \file locking_ptr.hpp
* \brief 带作用域锁定机制的智能指针
*
* 该类是volatile指针的包装器，通过锁定传入的互斥量来实现对内部指针的同步访问。
* locking_ptr基于Andrei Alexandrescu的LockingPtr设计。更多信息请参阅
* A. Alexandrescu的文章"volatile - Multithreaded Programmer's Best Friend"。
*
* 设计逻辑：
* - 实现RAII模式的自动锁定和解锁机制
* - 提供线程安全的指针访问接口
* - 结合volatile语义和互斥锁保证数据同步
* - 不可拷贝，确保锁定的唯一性和安全性
*
* 作用：
* - 提供线程安全的对象访问机制
* - 自动管理互斥锁的生命周期
* - 简化多线程环境下的同步访问代码
* - 防止数据竞争和不一致状态
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

#ifndef THREADPOOL_DETAIL_LOCKING_PTR_HPP_INCLUDED
#define THREADPOOL_DETAIL_LOCKING_PTR_HPP_INCLUDED

// 引入Boost实用工具库
#include <boost/utility.hpp>
// 引入Boost互斥锁库
#include <boost/thread/mutex.hpp>

// 线程池详细实现命名空间
namespace boost { namespace threadpool { namespace detail 
{

/*!
 * \brief 带作用域锁定机制的智能指针
 *
 * 该类是volatile指针的包装器，通过在构造时锁定互斥量、析构时解锁互斥量
 * 来实现对内部指针的同步访问。这确保了在智能指针存在期间，对象访问是线程安全的。
 *
 * \tparam T 被指向对象的类型
 * \tparam Mutex 互斥锁的类型
 */
  template <typename T, typename Mutex>
  class locking_ptr 
  : private noncopyable
  {
    T* m_obj;                     ///< 指向实例的指针
    Mutex & m_mutex;              ///< 用于作用域锁定的互斥量引用

  public:
    /*!
     * \brief 构造函数
     *
     * 创建锁定指针并立即锁定关联的互斥量。在构造函数完成后，
     * 对象的访问将是线程安全的。
     *
     * \param obj 要访问的volatile对象引用
     * \param mtx 用于同步的volatile互斥量引用
     */
    locking_ptr(volatile T& obj, const volatile Mutex& mtx)
      : m_obj(const_cast<T*>(&obj))
      , m_mutex(*const_cast<Mutex*>(&mtx))
    {   
      // 锁定互斥量，确保线程安全访问
	  m_mutex.lock();
    }

    /*!
     * \brief 析构函数
     *
     * 销毁锁定指针并自动解锁关联的互斥量。这确保了RAII模式的正确实现，
     * 即使在异常情况下也能保证互斥量被正确释放。
     */
    ~locking_ptr()
    { 
      // 解锁互斥量，释放同步资源
      m_mutex.unlock();
    }

    /*!
     * \brief 解引用操作符
     *
     * 返回对存储实例的引用。由于互斥量已被锁定，
     * 对返回引用的访问是线程安全的。
     *
     * \return 存储实例的引用
     */
    T& operator*() const
    {    
      return *m_obj;    
    }

    /*!
     * \brief 成员访问操作符
     *
     * 返回指向存储实例的指针。由于互斥量已被锁定，
     * 通过返回指针对成员的访问是线程安全的。
     *
     * \return 指向存储实例的指针
     */
    T* operator->() const
    {   
      return m_obj;   
    }
  };


} } } // namespace boost::threadpool::detail


#endif // THREADPOOL_DETAIL_LOCKING_PTR_HPP_INCLUDED

