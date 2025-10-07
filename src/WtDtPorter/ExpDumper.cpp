/*!
 * \file ExpDumper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展数据转储器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了ExpDumper类的所有成员函数，完成历史数据转储功能的具体逻辑。
 * 
 * 主要功能包括：
 * 1. 实现委托队列、委托明细、逐笔成交等Level2数据的转储
 * 2. 实现K线和Tick等基础行情数据的转储
 * 3. 作为代理，将所有转储请求转发给WtDtRunner的全局单例进行实际处理
 * 4. 在转发时附带转储器ID，便于WtDtRunner识别和追踪
 * 
 * 设计思想：
 * - 采用代理模式，所有实现函数都是简单的转发调用
 * - 通过getRunner()获取全局WtDtRunner单例，保证系统中只有一个数据运行器
 * - 将转储器ID作为第一个参数传递给WtDtRunner，实现多转储器的识别和管理
 * - 函数实现简洁清晰，易于维护和扩展
 * 
 * 该文件是ExpDumper功能的核心实现，所有数据转储操作最终都通过WtDtRunner
 * 转发到外部注册的回调函数进行处理。
 */
#include "ExpDumper.h"  // 包含ExpDumper类声明
#include "WtDtRunner.h"  // 包含WtDtRunner类声明

// 外部声明：获取全局WtDtRunner单例的引用
// 该单例在WtDtPorter.cpp中定义，负责管理所有数据转储操作
extern WtDtRunner& getRunner();

/**
 * @brief 转储历史委托队列数据
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD
 * @param items 委托队列数据数组指针
 * @param count 委托队列数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将委托队列数据转储请求转发给WtDtRunner处理。
 * WtDtRunner会调用外部注册的委托队列转储回调函数来完成实际的存储操作。
 */
bool ExpDumper::dumpHisOrdQue(const char* stdCode, uint32_t uDate, WTSOrdQueStruct* items, uint32_t count)
{
	// 调用WtDtRunner的dumpHisOrdQue方法，将转储器ID作为第一个参数传递
	// 这样WtDtRunner可以识别是哪个转储器发起的请求
	return getRunner().dumpHisOrdQue(_id.c_str(), stdCode, uDate, items, count);
}

/**
 * @brief 转储历史委托明细数据
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD
 * @param items 委托明细数据数组指针
 * @param count 委托明细数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将委托明细数据转储请求转发给WtDtRunner处理。
 * WtDtRunner会调用外部注册的委托明细转储回调函数来完成实际的存储操作。
 */
bool ExpDumper::dumpHisOrdDtl(const char* stdCode, uint32_t uDate, WTSOrdDtlStruct* items, uint32_t count)
{
	// 调用WtDtRunner的dumpHisOrdDtl方法，将转储器ID作为第一个参数传递
	// 便于WtDtRunner进行转储器实例的识别和管理
	return getRunner().dumpHisOrdDtl(_id.c_str(), stdCode, uDate, items, count);
}

/**
 * @brief 转储历史逐笔成交数据
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD
 * @param items 逐笔成交数据数组指针
 * @param count 逐笔成交数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将逐笔成交数据转储请求转发给WtDtRunner处理。
 * WtDtRunner会调用外部注册的逐笔成交转储回调函数来完成实际的存储操作。
 */
bool ExpDumper::dumpHisTrans(const char* stdCode, uint32_t uDate, WTSTransStruct* items, uint32_t count)
{
	// 调用WtDtRunner的dumpHisTrans方法，将转储器ID作为第一个参数传递
	// 支持多个转储器同时工作时的识别和管理
	return getRunner().dumpHisTrans(_id.c_str(), stdCode, uDate, items, count);
}

/**
 * @brief 转储历史K线数据
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param period K线周期，如"m1"、"m5"、"day"等
 * @param bars K线数据数组指针
 * @param count K线数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将K线数据转储请求转发给WtDtRunner处理。
 * WtDtRunner会调用外部注册的K线转储回调函数来完成实际的存储操作。
 */
bool ExpDumper::dumpHisBars(const char* stdCode, const char* period, WTSBarStruct* bars, uint32_t count)
{
	// 调用WtDtRunner的dumpHisBars方法，将转储器ID作为第一个参数传递
	// 便于回调函数识别数据来源和执行相应的存储逻辑
	return getRunner().dumpHisBars(_id.c_str(), stdCode, period, bars, count);
}

/**
 * @brief 转储历史Tick数据
 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
 * @param uDate 交易日期，格式为YYYYMMDD
 * @param ticks Tick数据数组指针
 * @param count Tick数据条数
 * @return bool 转储成功返回true，失败返回false
 * 
 * 该函数将Tick数据转储请求转发给WtDtRunner处理。
 * WtDtRunner会调用外部注册的Tick转储回调函数来完成实际的存储操作。
 */
bool ExpDumper::dumpHisTicks(const char* stdCode, uint32_t uDate, WTSTickStruct* ticks, uint32_t count)
{
	// 调用WtDtRunner的dumpHisTicks方法，将转储器ID作为第一个参数传递
	// 支持外部回调函数根据转储器ID执行不同的存储策略
	return getRunner().dumpHisTicks(_id.c_str(), stdCode, uDate, ticks, count);
}
