/*!
 * \file IRdmDtReader.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 随机数据读取器接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中随机数据读取器的核心接口，负责提供随机访问行情数据的功能。
 * 主要功能包括：
 * 1. 定义随机数据读取回调接口，提供基础数据管理和主力切换规则管理访问
 * 2. 定义随机数据读取器接口，支持按时间范围、日期、数量等不同方式读取数据
 * 3. 支持Tick、K线、委托明细、委托队列、逐笔成交等多种数据类型的读取
 * 4. 提供复权因子查询和缓存清理功能
 * 5. 支持插件化的读取器创建和删除
 * 
 * 该类主要用于WonderTrader框架中的随机数据访问系统，为策略引擎和分析工具提供灵活的数据查询接口。
 * 通过抽象接口设计，支持多种数据源和不同格式的随机数据读取。
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含固定大小整数类型

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义
#include "../Includes/WTSTypes.h"  // 包含WTS类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSKlineSlice;  // 前向声明：WTS K线切片类
class WTSTickSlice;  // 前向声明：WTS Tick切片类
class WTSOrdQueSlice;  // 前向声明：WTS委托队列切片类
class WTSOrdDtlSlice;  // 前向声明：WTS委托明细切片类
class WTSTransSlice;  // 前向声明：WTS逐笔成交切片类
class IBaseDataMgr;  // 前向声明：基础数据管理器接口
class IHotMgr;  // 前向声明：主力切换规则管理接口
class WTSVariant;  // 前向声明：WTS变体类型类


/**
 * @class IRdmDtReaderSink
 * @brief 随机数据读取模块回调接口类
 * 
 * 该类定义了随机数据读取模块的回调接口，主要用于数据读取模块向调用模块回调。
 * 提供基础数据管理、主力切换规则管理、日志输出等功能的回调接口。
 * 
 * 主要特性：
 * - 提供基础数据管理器的获取接口
 * - 支持主力切换规则管理器的获取
 * - 提供日志输出的回调接口
 * - 统一的回调接口设计
 * - 支持多种数据管理功能的访问
 */
class IRdmDtReaderSink
{
public:
	/**
	 * @brief 获取基础数据管理接口指针
	 * @return IBaseDataMgr* 返回基础数据管理器接口指针
	 * 
	 * 该函数返回基础数据管理器接口指针，用于访问基础数据管理功能。
	 * 纯虚函数，子类必须实现。
	 */
	virtual IBaseDataMgr*	get_basedata_mgr() = 0;  // 纯虚函数：获取基础数据管理接口指针

	/**
	 * @brief 获取主力切换规则管理接口指针
	 * @return IHotMgr* 返回主力切换规则管理器接口指针
	 * 
	 * 该函数返回主力切换规则管理器接口指针，用于访问主力切换规则管理功能。
	 * 纯虚函数，子类必须实现。
	 */
	virtual IHotMgr*		get_hot_mgr() = 0;  // 纯虚函数：获取主力切换规则管理接口指针

	/**
	 * @brief 输出数据读取模块的日志
	 * @param ll 日志级别
	 * @param message 日志消息内容
	 * 
	 * 该函数用于输出数据读取模块的日志信息，支持不同级别的日志输出。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void		reader_log(WTSLogLevel ll, const char* message) = 0;  // 纯虚函数：输出数据读取模块的日志
};

/**
 * @class IRdmDtReader
 * @brief 随机数据读取接口类
 * 
 * 该类定义了WonderTrader框架中随机数据读取器的核心接口，向核心模块提供行情数据读取功能。
 * 支持按时间范围、日期、数量等不同方式读取Tick、K线、委托明细、委托队列、逐笔成交等数据。
 * 
 * 主要特性：
 * - 支持按时间范围读取多种类型的数据
 * - 支持按日期读取Tick数据
 * - 支持按数量读取Tick和K线数据
 * - 提供复权因子查询功能
 * - 支持缓存清理功能
 * - 统一的接口设计，支持多种数据源
 * - 灵活的查询方式，满足不同应用场景
 */
class IRdmDtReader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化随机数据读取器对象，设置回调接口为NULL。
	 */
	IRdmDtReader() :_sink(NULL) {}  // 初始化回调接口为NULL

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IRdmDtReader(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 初始化随机数据读取器
	 * @param cfg 配置参数
	 * @param sink 回调接口指针
	 * 
	 * 该函数初始化随机数据读取器，设置配置参数和回调接口。
	 * 默认实现保存回调接口，子类可以重写此函数进行自定义初始化。
	 */
	virtual void init(WTSVariant* cfg, IRdmDtReaderSink* sink) { _sink = sink; }  // 虚函数：初始化随机数据读取器

	/**
	 * @brief 按时间范围读取委托明细数据切片
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间戳
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针
	 * 
	 * 该函数按时间范围读取指定合约的委托明细数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSOrdDtlSlice*	readOrdDtlSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) = 0;  // 纯虚函数：按时间范围读取委托明细数据切片

	/**
	 * @brief 按时间范围读取委托队列数据切片
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间戳
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
	 * 
	 * 该函数按时间范围读取指定合约的委托队列数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSOrdQueSlice*	readOrdQueSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) = 0;  // 纯虚函数：按时间范围读取委托队列数据切片

	/**
	 * @brief 按时间范围读取逐笔成交数据切片
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间戳
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSTransSlice* 返回逐笔成交数据切片指针
	 * 
	 * 该函数按时间范围读取指定合约的逐笔成交数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTransSlice*	readTransSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) = 0;  // 纯虚函数：按时间范围读取逐笔成交数据切片

	/**
	 * @brief 按日期读取Tick数据切片
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期，格式为YYYYMMDD，默认为0（当前日期）
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 该函数按指定日期读取指定合约的Tick数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	readTickSliceByDate(const char* stdCode, uint32_t uDate = 0) = 0;  // 纯虚函数：按日期读取Tick数据切片

	/**
	 * @brief 按时间范围读取Tick数据切片
	 * @param stdCode 标准合约代码
	 * @param stime 开始时间戳
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 该函数按时间范围读取指定合约的Tick数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	readTickSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) = 0;  // 纯虚函数：按时间范围读取Tick数据切片

	/**
	 * @brief 按时间范围读取K线数据切片
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param stime 开始时间戳
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 * 
	 * 该函数按时间范围读取指定合约的K线数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineSlice*	readKlineSliceByRange(const char* stdCode, WTSKlinePeriod period, uint64_t stime, uint64_t etime = 0) = 0;  // 纯虚函数：按时间范围读取K线数据切片

	/**
	 * @brief 按数量读取Tick数据切片
	 * @param stdCode 标准合约代码
	 * @param count 数据条数
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 该函数按指定数量读取指定合约的Tick数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	readTickSliceByCount(const char* stdCode, uint32_t count, uint64_t etime = 0) = 0;  // 纯虚函数：按数量读取Tick数据切片

	/**
	 * @brief 按数量读取K线数据切片
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param count 数据条数
	 * @param etime 结束时间戳，默认为0（当前时间）
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 * 
	 * 该函数按指定数量读取指定合约的K线数据切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineSlice*	readKlineSliceByCount(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime = 0) = 0;  // 纯虚函数：按数量读取K线数据切片

	/**
	 * @brief 获取个股指定日期的复权因子
	 * @param stdCode 标准品种代码，如SSE.600000
	 * @param date 指定日期，格式yyyyMMdd，默认为0，为0则按当前日期处理
	 * @return double 返回复权因子，默认为1.0
	 * 
	 * 该函数获取个股指定日期的复权因子，用于复权计算。
	 * 默认实现返回1.0，子类可以重写此函数。
	 */
	virtual double		getAdjFactorByDate(const char* stdCode, uint32_t date = 0) { return 1.0; }  // 虚函数：获取个股指定日期的复权因子，默认返回1.0

	/**
	 * @brief 清理缓存
	 * 
	 * 该函数清理随机数据读取器的内部缓存，释放内存资源。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void		clearCache(){}  // 虚函数：清理缓存，默认实现为空

protected:
	IRdmDtReaderSink*	_sink;  // 随机数据读取模块回调接口指针
};

/**
 * @typedef FuncCreateRdmDtReader
 * @brief 创建随机数据读取器函数指针类型
 * 
 * 该类型定义了创建随机数据读取器的函数指针签名。
 * 用于动态加载随机数据读取器插件。
 */
typedef IRdmDtReader* (*FuncCreateRdmDtReader)();  // 创建随机数据读取器函数指针类型定义

/**
 * @typedef FuncDeleteRdmDtReader
 * @brief 删除随机数据读取器函数指针类型
 * 
 * 该类型定义了删除随机数据读取器的函数指针签名。
 * 用于动态卸载随机数据读取器插件。
 */
typedef void(*FuncDeleteRdmDtReader)(IRdmDtReader* store);  // 删除随机数据读取器函数指针类型定义

NS_WTP_END  // 结束WonderTrader命名空间