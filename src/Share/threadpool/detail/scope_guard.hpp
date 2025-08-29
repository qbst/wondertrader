/*!
* \file scope_guard.hpp
* \brief 作用域守卫类实现
*
* 该文件实现了RAII（资源获取即初始化）模式的作用域守卫类。作用域守卫
* 确保在离开作用域时自动执行清理操作，即使发生异常也能保证资源的正确释放。
*
* 设计逻辑：
* - 使用RAII模式管理资源和清理操作
* - 支持手动禁用清理操作
* - 在析构函数中自动执行清理代码
* - 不可拷贝，确保唯一性和安全性
*
* 作用：
* - 提供异常安全的资源管理
* - 自动化清理操作的执行
* - 防止资源泄露和状态不一致
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

#ifndef THREADPOOL_DETAIL_SCOPE_GUARD_HPP_INCLUDED
#define THREADPOOL_DETAIL_SCOPE_GUARD_HPP_INCLUDED

// 引入Boost函数对象库
#include <boost/function.hpp>

// 线程池详细实现命名空间
namespace boost { namespace threadpool { namespace detail 
{

/*!
 * \brief 作用域守卫类
 * 
 * 作用域守卫类实现了RAII模式，在对象析构时自动执行指定的清理函数。
 * 这确保了即使在异常情况下也能正确执行清理操作。该类不可拷贝，
 * 保证了清理操作的唯一性和安全性。
 */
class scope_guard
: private boost::noncopyable
{
	const function0<void> m_function;  ///< 在作用域结束时要执行的清理函数
	bool m_is_active;                  ///< 标记守卫是否处于活动状态

public:
	/*!
	 * \brief 构造函数
	 * 
	 * 创建一个作用域守卫对象，指定在作用域结束时要执行的清理函数。
	 * 
	 * \param call_on_exit 在作用域结束时要调用的函数对象
	 */
	scope_guard(function0<void> const & call_on_exit)
	: m_function(call_on_exit)
	, m_is_active(true)
	{
	}

	/*!
	 * \brief 析构函数
	 * 
	 * 在对象销毁时自动调用。如果守卫处于活动状态且清理函数有效，
	 * 则执行清理函数。这确保了资源的正确释放。
	 */
	~scope_guard()
	{
		if(m_is_active && m_function)
		{
			m_function();
		}
	}

	/*!
	 * \brief 禁用守卫
	 * 
	 * 手动禁用作用域守卫，使其在析构时不执行清理函数。
	 * 这在某些情况下很有用，比如操作成功完成时不需要执行清理。
	 */
	void disable()
	{
		m_is_active = false;
	}
};






} } } // namespace boost::threadpool::detail

#endif // THREADPOOL_DETAIL_SCOPE_GUARD_HPP_INCLUDED


