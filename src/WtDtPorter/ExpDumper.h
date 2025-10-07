/*!
 * \file ExpDumper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展数据转储器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了ExpDumper（扩展数据转储器）类，该类是IHisDataDumper接口的实现类。
 * 
 * 主要功能包括：
 * 1. 实现历史数据转储接口，支持K线、Tick、委托队列、委托明细、逐笔成交等多种数据类型的转储
 * 2. 作为外部数据转储逻辑与WonderTrader核心系统之间的桥接器
 * 3. 将数据转储请求委托给WtDtRunner进行实际处理
 * 4. 通过唯一ID标识不同的转储器实例，支持多个转储器同时工作
 * 
 * 设计思想：
 * - 采用代理模式，ExpDumper作为代理，将实际的转储工作委托给WtDtRunner处理
 * - 实现了IHisDataDumper接口，符合系统的统一数据转储规范
 * - 每个ExpDumper实例通过唯一ID标识，便于管理和追踪
 * - 支持扩展，外部可以注册自定义的数据转储回调函数
 * 
 * 该类主要用于将WonderTrader核心系统的数据转储请求转发给外部注册的转储函数，
 * 实现了系统内部与外部存储逻辑的解耦。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/IDataWriter.h"  // 包含数据写入器接口定义

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class ExpDumper
 * @brief 扩展数据转储器类
 * 
 * ExpDumper是IHisDataDumper接口的实现类，用于将历史数据转储到外部存储系统。
 * 该类作为桥接器，将WonderTrader核心系统的数据转储请求转发给外部注册的回调函数处理。
 * 
 * 主要特性：
 * - 实现了IHisDataDumper接口的所有纯虚函数
 * - 支持K线、Tick、委托队列、委托明细、逐笔成交等多种数据类型的转储
 * - 通过唯一ID标识不同的转储器实例
 * - 采用代理模式，将实际的转储工作委托给WtDtRunner处理
 * - 轻量级设计，仅保存转储器ID，不保存实际的数据
 * 
 * 使用场景：
 * 当需要将历史数据导出到自定义存储系统（如数据库、文件、云存储）时，
 * 可以创建ExpDumper实例并注册相应的回调函数来实现自定义的存储逻辑。
 */
class ExpDumper : public IHisDataDumper
{
public:
	/**
	 * @brief 构造函数
	 * @param id 转储器唯一标识符，用于区分不同的转储器实例
	 * 
	 * 创建一个ExpDumper实例，并使用指定的ID进行标识。
	 * ID将用于在回调函数中识别是哪个转储器发起的请求。
	 */
	ExpDumper(const char* id) :_id(id) {}  // 初始化转储器ID
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构，释放所有资源。
	 * 当前实现为空，因为ExpDumper不持有需要手动释放的资源。
	 */
	virtual ~ExpDumper() {}  // 虚析构函数

public:
	/**
	 * @brief 转储历史K线数据
	 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
	 * @param period K线周期，如"m1"、"m5"、"day"等
	 * @param bars K线数据数组指针，包含开高低收量额等信息
	 * @param count K线数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数重写了IHisDataDumper接口的dumpHisBars方法，
	 * 将K线数据转储请求转发给WtDtRunner进行处理。
	 */
	virtual bool dumpHisBars(const char* stdCode, const char* period, WTSBarStruct* bars, uint32_t count) override;

	/**
	 * @brief 转储历史Tick数据
	 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param ticks Tick数据数组指针，包含价格、成交量、买卖盘等信息
	 * @param count Tick数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数重写了IHisDataDumper接口的dumpHisTicks方法，
	 * 将Tick数据转储请求转发给WtDtRunner进行处理。
	 */
	virtual bool dumpHisTicks(const char* stdCode, uint32_t uDate, WTSTickStruct* ticks, uint32_t count) override;

	/**
	 * @brief 转储历史委托队列数据
	 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param items 委托队列数据数组指针，包含买卖盘口队列信息
	 * @param count 委托队列数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数重写了IHisDataDumper接口的dumpHisOrdQue方法，
	 * 将委托队列数据转储请求转发给WtDtRunner进行处理。
	 * 委托队列数据主要用于Level2行情分析。
	 */
	virtual bool dumpHisOrdQue(const char* stdCode, uint32_t uDate, WTSOrdQueStruct* items, uint32_t count) override;

	/**
	 * @brief 转储历史委托明细数据
	 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param items 委托明细数据数组指针，包含每笔委托的详细信息
	 * @param count 委托明细数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数重写了IHisDataDumper接口的dumpHisOrdDtl方法，
	 * 将委托明细数据转储请求转发给WtDtRunner进行处理。
	 * 委托明细数据用于深度市场微观结构分析。
	 */
	virtual bool dumpHisOrdDtl(const char* stdCode, uint32_t uDate, WTSOrdDtlStruct* items, uint32_t count) override;

	/**
	 * @brief 转储历史逐笔成交数据
	 * @param stdCode 标准合约代码，格式为"交易所.合约代码"
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param items 逐笔成交数据数组指针，包含每笔成交的详细信息
	 * @param count 逐笔成交数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数重写了IHisDataDumper接口的dumpHisTrans方法，
	 * 将逐笔成交数据转储请求转发给WtDtRunner进行处理。
	 * 逐笔成交数据用于交易行为分析和市场微观结构研究。
	 */
	virtual bool dumpHisTrans(const char* stdCode, uint32_t uDate, WTSTransStruct* items, uint32_t count) override;

private:
	std::string	_id;  // 转储器唯一标识符，用于在回调函数中识别转储器实例
};

