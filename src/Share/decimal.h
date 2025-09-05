/*!
 * \file decimal.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 浮点数辅助类,主要用于浮点数据的比较
 * 
 * 该文件提供了高精度浮点数比较和运算的工具函数，主要用于：
 * 1. 浮点数的精确比较（等于、大于、小于、大于等于、小于等于）
 * 2. 浮点数的四舍五入处理
 * 3. 浮点数的模运算
 * 
 * 设计逻辑：
 * - 使用EPSINON常量定义浮点数比较的精度阈值
 * - 所有比较函数都基于EPSINON进行容差比较，避免浮点数精度问题
 * - 提供noexcept保证的函数，确保异常安全性
 * - 支持默认参数，简化常用比较操作
 * 
 * 主要作用：
 * - 解决金融计算中浮点数精度问题
 * - 为WonderTrader框架提供可靠的数值比较工具
 * - 支持高频交易等对数值精度要求较高的场景
 */
#pragma once  // 防止头文件重复包含
#include <math.h>  // 包含数学函数库

/**
 * @namespace decimal
 * @brief 浮点数辅助工具命名空间
 * 
 * 该命名空间包含所有浮点数比较和运算的辅助函数，
 * 提供高精度的浮点数操作，避免浮点数精度误差。
 */
namespace decimal
{
	const double EPSINON = 1e-6;  // 定义浮点数比较的精度阈值，用于容差比较

	/**
	 * @brief 对浮点数进行四舍五入处理
	 * @param v 需要进行四舍五入的浮点数值
	 * @param exp 精度因子，默认为1（即保留整数位）
	 * @return double 返回四舍五入后的结果
	 * 
	 * 该函数通过乘以精度因子、四舍五入、再除以精度因子的方式
	 * 实现指定精度的四舍五入操作。
	 */
	inline double rnd(double v, int exp = 1) 
	{
		return round(v*exp) / exp;  // 先乘以精度因子，四舍五入，再除以精度因子
	}

	/**
	 * @brief 判断两个浮点数是否相等（基于容差比较）
	 * @param a 第一个浮点数值
	 * @param b 第二个浮点数值，默认为0.0
	 * @return bool 如果两个数在EPSINON范围内相等则返回true，否则返回false
	 * 
	 * 使用EPSINON作为容差，避免浮点数精度误差导致的比较问题。
	 * 函数声明为noexcept，确保异常安全性。
	 */
	inline bool eq(double a, double b = 0.0) noexcept
	{
		return(fabs(a - b) < EPSINON);  // 计算两数差值的绝对值，与精度阈值比较
	}

	/**
	 * @brief 判断第一个浮点数是否大于第二个浮点数（基于容差比较）
	 * @param a 第一个浮点数值
	 * @param b 第二个浮点数值，默认为0.0
	 * @return bool 如果a大于b且差值超过EPSINON则返回true，否则返回false
	 * 
	 * 通过比较差值是否大于精度阈值来判断大小关系，
	 * 避免浮点数精度误差的影响。
	 */
	inline bool gt(double a, double b = 0.0) noexcept
	{
		return a - b > EPSINON;  // 计算a-b的差值，与精度阈值比较
	}

	/**
	 * @brief 判断第一个浮点数是否小于第二个浮点数（基于容差比较）
	 * @param b 第一个浮点数值
	 * @param a 第二个浮点数值，默认为0.0
	 * @return bool 如果b小于a且差值超过EPSINON则返回true，否则返回false
	 * 
	 * 通过比较差值是否大于精度阈值来判断大小关系，
	 * 注意参数顺序，使用b-a进行比较。
	 */
	inline bool lt(double a, double b = 0.0) noexcept
	{
		return b - a > EPSINON;  // 计算b-a的差值，与精度阈值比较
	}

	/**
	 * @brief 判断第一个浮点数是否大于等于第二个浮点数（基于容差比较）
	 * @param a 第一个浮点数值
	 * @param b 第二个浮点数值，默认为0.0
	 * @return bool 如果a大于b或a等于b则返回true，否则返回false
	 * 
	 * 通过组合大于和等于的判断逻辑，实现大于等于的比较。
	 */
	inline bool ge(double a, double b = 0.0) noexcept
	{
		return gt(a, b) || eq(a, b);  // 大于或等于，使用逻辑或组合两个条件
	}

	/**
	 * @brief 判断第一个浮点数是否小于等于第二个浮点数（基于容差比较）
	 * @param a 第一个浮点数值
	 * @param b 第二个浮点数值，默认为0.0
	 * @return bool 如果a小于b或a等于b则返回true，否则返回false
	 * 
	 * 通过组合小于和等于的判断逻辑，实现小于等于的比较。
	 */
	inline bool le(double a, double b = 0.0) noexcept
	{
		return lt(a, b) || eq(a, b);  // 小于或等于，使用逻辑或组合两个条件
	}

	/**
	 * @brief 计算浮点数的模运算结果
	 * @param a 被除数
	 * @param b 除数
	 * @return double 返回a除以b的余数部分
	 * 
	 * 该函数计算浮点数除法的余数部分，通过减去整数商的方式实现。
	 * 适用于需要获取浮点数除法小数部分的场景。
	 */
	inline double mod(double a, double b)
	{
		return a / b - round(a / b);  // 计算除法结果减去四舍五入后的整数部分
	}
	
};  // 命名空间结束