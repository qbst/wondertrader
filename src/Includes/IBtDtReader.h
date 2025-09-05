/*!
 * \file IBtDtReader.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 回测数据读取器接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中回测数据读取器的核心接口，用于在回测过程中读取历史行情数据。
 * 主要功能包括：
 * 1. 定义回测数据读取器接口，提供历史数据读取功能
 * 2. 定义数据读取回调接口，支持日志输出和状态通知
 * 3. 支持多种数据类型的读取：K线数据、Tick数据、委托明细、委托队列、逐笔成交
 * 4. 提供数据读取器的创建和删除函数指针类型
 * 5. 支持配置管理和回调机制
 * 
 * 该类主要用于WonderTrader框架中的回测系统，为策略回测提供历史数据支持。
 * 通过统一的接口设计，支持多种数据源和不同格式的历史数据读取。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串支持

#include "../Includes/WTSTypes.h"  // 包含WTS类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：WTS变体类型类

/**
 * @class IBtDtReaderSink
 * @brief 回测数据读取器回调接口类
 * 
 * 该类定义了回测数据读取器的回调接口，主要用于数据读取模块向调用模块回调。
 * 支持日志输出和状态通知，便于调试和监控。
 * 
 * 主要特性：
 * - 提供日志输出回调接口
 * - 支持不同级别的日志输出
 * - 便于数据读取模块的状态监控
 * - 统一的回调接口设计
 */
class IBtDtReaderSink
{
public:
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
 * @class IBtDtReader
 * @brief 回测数据读取器接口类
 * 
 * 该类定义了回测数据读取器的核心接口，向核心模块提供行情数据读取功能。
 * 支持K线数据、Tick数据、委托明细、委托队列、逐笔成交等多种数据类型的读取。
 * 
 * 主要特性：
 * - 支持K线数据的读取（支持不同周期）
 * - 支持Tick数据的读取
 * - 支持委托明细、委托队列、逐笔成交数据的读取
 * - 提供配置管理和回调机制
 * - 统一的接口设计，支持多种数据源
 */
class IBtDtReader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化回测数据读取器对象，设置回调接口为NULL。
	 */
	IBtDtReader() :_sink(NULL) {}  // 初始化回调接口为NULL

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IBtDtReader(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 初始化数据读取器
	 * @param cfg 配置参数
	 * @param sink 回调接口对象
	 * 
	 * 该函数初始化数据读取器，设置配置参数和回调接口。
	 * 默认实现保存回调接口，子类可以重写此函数进行自定义初始化。
	 */
	virtual void init(WTSVariant* cfg, IBtDtReaderSink* sink) { _sink = sink; }  // 虚函数：初始化数据读取器

	/**
	 * @brief 读取原始K线数据
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param period K线周期
	 * @param buffer 输出缓冲区，存储读取的K线数据
	 * @return bool 读取成功返回true，失败返回false
	 * 
	 * 该函数读取指定合约在指定周期的原始K线数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool read_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, std::string& buffer) = 0;  // 纯虚函数：读取原始K线数据

	/**
	 * @brief 读取原始Tick数据
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param buffer 输出缓冲区，存储读取的Tick数据
	 * @return bool 读取成功返回true，失败返回false
	 * 
	 * 该函数读取指定合约在指定日期的原始Tick数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool read_raw_ticks(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) = 0;  // 纯虚函数：读取原始Tick数据

	/**
	 * @brief 读取原始委托明细数据
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param buffer 输出缓冲区，存储读取的委托明细数据
	 * @return bool 读取成功返回true，失败返回false
	 * 
	 * 该函数读取指定合约在指定日期的原始委托明细数据。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool read_raw_order_details(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) { return false; }  // 虚函数：读取原始委托明细数据，默认返回false

	/**
	 * @brief 读取原始委托队列数据
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param buffer 输出缓冲区，存储读取的委托队列数据
	 * @return bool 读取成功返回true，失败返回false
	 * 
	 * 该函数读取指定合约在指定日期的原始委托队列数据。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool read_raw_order_queues(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) { return false; }  // 虚函数：读取原始委托队列数据，默认返回false

	/**
	 * @brief 读取原始逐笔成交数据
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 交易日期，格式为YYYYMMDD
	 * @param buffer 输出缓冲区，存储读取的逐笔成交数据
	 * @return bool 读取成功返回true，失败返回false
	 * 
	 * 该函数读取指定合约在指定日期的原始逐笔成交数据。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool read_raw_transactions(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) { return false; }  // 虚函数：读取原始逐笔成交数据，默认返回false

protected:
	IBtDtReaderSink*	_sink;  // 回调接口指针，用于向调用模块回调
};

/**
 * @typedef FuncCreateBtDtReader
 * @brief 创建回测数据读取器函数指针类型
 * 
 * 该类型定义了创建回测数据读取器的函数指针签名。
 * 用于动态加载回测数据读取器插件。
 */
typedef IBtDtReader* (*FuncCreateBtDtReader)();  // 创建回测数据读取器函数指针类型定义

/**
 * @typedef FuncDeleteBtDtReader
 * @brief 删除回测数据读取器函数指针类型
 * 
 * 该类型定义了删除回测数据读取器的函数指针签名。
 * 用于动态卸载回测数据读取器插件。
 */
typedef void(*FuncDeleteBtDtReader)(IBtDtReader* store);  // 删除回测数据读取器函数指针类型定义

NS_WTP_END  // 结束WonderTrader命名空间