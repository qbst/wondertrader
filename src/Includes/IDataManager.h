/*!
 * \file IDataManager.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据管理器接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中数据管理器的核心接口，负责管理各种市场数据的访问和查询。
 * 主要功能包括：
 * 1. 提供Tick数据切片的查询接口，支持指定数量和截止时间
 * 2. 提供委托队列、委托明细、逐笔成交等高频数据的查询接口
 * 3. 提供K线数据的查询接口，支持不同周期和倍数
 * 4. 提供最新Tick数据的获取接口
 * 5. 支持复权因子和复权标志的查询
 * 
 * 该类主要用于WonderTrader框架中的数据管理系统，为策略引擎和回测系统提供统一的数据访问接口。
 * 通过抽象接口设计，支持多种数据源和不同格式的数据管理。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/WTSTypes.h"  // 包含WTS类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSTickSlice;  // 前向声明：WTS Tick切片类
class WTSKlineSlice;  // 前向声明：WTS K线切片类
class WTSTickData;  // 前向声明：WTS Tick数据类
class WTSOrdQueSlice;  // 前向声明：WTS委托队列切片类
class WTSOrdDtlSlice;  // 前向声明：WTS委托明细切片类
class WTSTransSlice;  // 前向声明：WTS逐笔成交切片类


/**
 * @class IDataManager
 * @brief 数据管理器接口类
 * 
 * 该类定义了WonderTrader框架中数据管理器的核心接口，负责管理各种市场数据的访问和查询。
 * 包括Tick数据、K线数据、委托队列、委托明细、逐笔成交等高频数据的统一管理。
 * 
 * 主要特性：
 * - 提供多种数据类型的数据切片查询接口
 * - 支持指定数量和截止时间的数据查询
 * - 支持K线数据的不同周期和倍数查询
 * - 提供复权因子和复权标志的查询功能
 * - 统一的接口设计，支持多种数据源
 */
class IDataManager
{
public:
	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return WTSTickSlice* 返回Tick数据切片指针，未找到返回NULL
	 * 
	 * 该函数获取指定合约的Tick数据切片，支持指定数据条数和截止时间。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSTickSlice* get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：获取Tick数据切片，默认返回NULL

	/**
	 * @brief 获取委托队列数据切片
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return WTSOrdQueSlice* 返回委托队列数据切片指针，未找到返回NULL
	 * 
	 * 该函数获取指定合约的委托队列数据切片，支持指定数据条数和截止时间。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSOrdQueSlice* get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：获取委托队列数据切片，默认返回NULL

	/**
	 * @brief 获取委托明细数据切片
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针，未找到返回NULL
	 * 
	 * 该函数获取指定合约的委托明细数据切片，支持指定数据条数和截止时间。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSOrdDtlSlice* get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：获取委托明细数据切片，默认返回NULL

	/**
	 * @brief 获取逐笔成交数据切片
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return WTSTransSlice* 返回逐笔成交数据切片指针，未找到返回NULL
	 * 
	 * 该函数获取指定合约的逐笔成交数据切片，支持指定数据条数和截止时间。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSTransSlice* get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：获取逐笔成交数据切片，默认返回NULL

	/**
	 * @brief 获取K线数据切片
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return WTSKlineSlice* 返回K线数据切片指针，未找到返回NULL
	 * 
	 * 该函数获取指定合约的K线数据切片，支持不同周期、倍数和条数。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSKlineSlice* get_kline_slice(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：获取K线数据切片，默认返回NULL

	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准合约代码
	 * @return WTSTickData* 返回最新Tick数据指针，未找到返回NULL
	 * 
	 * 该函数获取指定合约的最新Tick数据，用于实时市场数据访问。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSTickData* grab_last_tick(const char* stdCode) { return NULL; }  // 虚函数：获取最新Tick数据，默认返回NULL

	/**
	 * @brief 获取复权因子
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @return double 返回复权因子，默认为1.0
	 * 
	 * 该函数获取指定合约在指定日期的复权因子，用于价格复权计算。
	 * 默认实现返回1.0，子类可以重写此函数。
	 */
	virtual double get_adjusting_factor(const char* stdCode, uint32_t uDate) { return 1.0; }  // 虚函数：获取复权因子，默认返回1.0

	/**
	 * @brief 获取复权标志
	 * @return uint32_t 返回复权标志，默认为0
	 * 
	 * 该函数获取系统的复权标志，用于控制复权计算的方式。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t get_adjusting_flag() { return 0; }  // 虚函数：获取复权标志，默认返回0
};

NS_WTP_END  // 结束WonderTrader命名空间
