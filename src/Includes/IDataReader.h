/*!
 * \file IDataReader.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据读取器接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中数据读取器的核心接口，负责提供行情数据的读取功能。
 * 主要功能包括：
 * 1. 定义数据读取回调接口，支持K线闭合事件和全部更新事件的通知
 * 2. 定义历史数据加载器接口，支持最终数据和原始数据的加载
 * 3. 定义数据读取器接口，提供Tick、K线、委托明细、委托队列、逐笔成交等数据的读取
 * 4. 支持复权因子和复权标记的查询
 * 5. 提供数据读取器的创建和删除函数指针类型
 * 
 * 该类主要用于WonderTrader框架中的数据读取系统，为策略引擎和回测系统提供统一的数据访问接口。
 * 通过抽象接口设计，支持多种数据源和不同格式的数据读取。
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含固定大小整数类型

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义
#include "../Includes/WTSTypes.h"  // 包含WTS类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSKlineData;  // 前向声明：WTS K线数据类
class WTSKlineSlice;  // 前向声明：WTS K线切片类
class WTSTickSlice;  // 前向声明：WTS Tick切片类
class WTSOrdQueSlice;  // 前向声明：WTS委托队列切片类
class WTSOrdDtlSlice;  // 前向声明：WTS委托明细切片类
class WTSTransSlice;  // 前向声明：WTS逐笔成交切片类
struct WTSBarStruct;  // 前向声明：WTS K线结构体
class WTSVariant;  // 前向声明：WTS变体类型类
class IBaseDataMgr;  // 前向声明：基础数据管理器接口
class IHotMgr;  // 前向声明：主力切换规则管理接口

/**
 * @class IDataReaderSink
 * @brief 数据读取模块回调接口类
 * 
 * 该类定义了数据读取模块的回调接口，主要用于数据读取模块向调用模块回调。
 * 支持K线闭合事件、全部更新事件、基础数据管理、主力切换规则管理等功能的回调。
 * 
 * 主要特性：
 * - 提供K线闭合事件和全部更新事件的回调接口
 * - 支持基础数据管理器和主力切换规则管理器的获取
 * - 提供当前日期、时间和秒数的查询接口
 * - 支持日志输出的回调接口
 * - 统一的回调接口设计
 */
class IDataReaderSink
{
public:
	/**
	 * @brief K线闭合事件回调
	 * @param stdCode 标准品种代码，如SSE.600000、SHFE.au.2005
	 * @param period K线周期
	 * @param newBar 闭合的K线结构指针
	 *	
	 * 该函数在K线闭合时被调用，用于通知调用模块K线数据已更新。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_bar(const char* stdCode, WTSKlinePeriod period, WTSBarStruct* newBar) = 0;  // 纯虚函数：K线闭合事件回调

	/**
	 * @brief 所有缓存的K线全部更新的事件回调
	 * @param updateTime K线更新时间，精确到分钟，如202004101500
	 * 
	 * 该函数在所有缓存的K线数据更新完成后被调用，用于通知调用模块数据更新完成。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void on_all_bar_updated(uint32_t updateTime) = 0;  // 纯虚函数：所有缓存的K线全部更新事件回调

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
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期，格式如20100410
	 * 
	 * 该函数返回当前日期，用于时间相关的判断和处理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t	get_date() = 0;  // 纯虚函数：获取当前日期

	/**
	 * @brief 获取当前1分钟线的时间
	 * @return uint32_t 返回当前1分钟线的时间
	 * 
	 * 该函数返回当前1分钟线的时间，这里的分钟线时间是处理过的1分钟线时间。
	 * 如现在是9:00:32秒，真实时间为0900，但是对应的1分钟线时间为0901。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t	get_min_time() = 0;  // 纯虚函数：获取当前1分钟线的时间

	/**
	 * @brief 获取当前的秒数
	 * @return uint32_t 返回当前的秒数，精确到毫秒，如37,500
	 * 
	 * 该函数返回当前的秒数，精确到毫秒，用于精确的时间计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t	get_secs() = 0;  // 纯虚函数：获取当前的秒数

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
 * @typedef FuncReadBars
 * @brief 历史数据加载器的回调函数类型
 * 
 * 该类型定义了历史数据加载器的回调函数签名，用于在数据加载完成后通知调用者。
 * 
 * @param obj 回传用的，原样返回即可
 * @param bars K线数据
 * @param count K线条数
 */
typedef void(*FuncReadBars)(void* obj, WTSBarStruct* bars, uint32_t count);  // 历史数据加载器的回调函数类型定义

/**
 * @typedef FuncReadFactors
 * @brief 加载复权因子回调函数类型
 * 
 * 该类型定义了加载复权因子的回调函数签名，用于在复权因子加载完成后通知调用者。
 * 
 * @param obj 回传用的，原样返回即可
 * @param stdCode 合约代码
 * @param dates 日期数组
 * @param factors 复权因子数组
 * @param count 数据条数
 */
typedef void(*FuncReadFactors)(void* obj, const char* stdCode, uint32_t* dates, double* factors, uint32_t count);  // 加载复权因子回调函数类型定义

/**
 * @class IHisDataLoader
 * @brief 历史数据加载器接口类
 * 
 * 该类定义了历史数据加载器的核心接口，负责加载历史K线数据和复权因子。
 * 支持最终数据和原始数据的加载，为数据读取器提供历史数据支持。
 * 
 * 主要特性：
 * - 支持最终历史K线数据的加载（已加工数据）
 * - 支持原始历史K线数据的加载（未加工数据）
 * - 支持全部除权因子的加载
 * - 支持指定合约除权因子的加载
 * - 统一的接口设计，支持多种数据源
 */
class IHisDataLoader
{
public:
	/**
	 * @brief 加载最终历史K线数据
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param cb 回调函数
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 该函数加载最终历史K线数据，和loadRawHisBars的区别在于，loadFinalHisBars系统认为是最终所需数据，
	 * 不再进行加工，例如复权数据、主力合约数据。loadRawHisBars是加载未加工的原始数据的接口。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool loadFinalHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) = 0;  // 纯虚函数：加载最终历史K线数据

	/**
	 * @brief 加载原始历史K线数据
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param period K线周期
	 * @param cb 回调函数
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 该函数加载原始历史K线数据，这些数据是未加工的原始数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool loadRawHisBars(void* obj, const char* stdCode, WTSKlinePeriod period, FuncReadBars cb) = 0;  // 纯虚函数：加载原始历史K线数据

	/**
	 * @brief 加载全部除权因子
	 * @param obj 回传用的，原样返回即可
	 * @param cb 回调函数
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 该函数加载全部除权因子，用于复权计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool loadAllAdjFactors(void* obj, FuncReadFactors cb) = 0;  // 纯虚函数：加载全部除权因子

	/**
	 * @brief 加载指定合约除权因子
	 * @param obj 回传用的，原样返回即可
	 * @param stdCode 合约代码
	 * @param cb 回调函数
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 该函数加载指定合约的除权因子，用于复权计算。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool loadAdjFactors(void* obj, const char* stdCode, FuncReadFactors cb) = 0;  // 纯虚函数：加载指定合约除权因子
};

/**
 * @class IDataReader
 * @brief 数据读取接口类
 * 
 * 该类定义了WonderTrader框架中数据读取器的核心接口，向核心模块提供行情数据读取功能。
 * 支持Tick、K线、委托明细、委托队列、逐笔成交等数据的读取，并提供复权因子和复权标记的查询。
 * 
 * 主要特性：
 * - 支持多种数据类型的数据读取（Tick、K线、委托明细、委托队列、逐笔成交）
 * - 提供历史数据加载器的集成
 * - 支持复权因子和复权标记的查询
 * - 提供分钟线闭合事件的处理接口
 * - 统一的接口设计，支持多种数据源
 */
class IDataReader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化数据读取器对象，设置回调接口和历史数据加载器为NULL。
	 */
	IDataReader() :_sink(NULL) {}  // 初始化回调接口和历史数据加载器为NULL

	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IDataReader(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 初始化数据读取模块
	 * @param cfg 模块配置项
	 * @param sink 模块回调接口
	 * @param loader 历史数据加载器，默认为NULL
	 * 
	 * 该函数初始化数据读取模块，设置配置参数、回调接口和历史数据加载器。
	 * 默认实现保存回调接口和历史数据加载器，子类可以重写此函数进行自定义初始化。
	 */
	virtual void init(WTSVariant* cfg, IDataReaderSink* sink, IHisDataLoader* loader = NULL) { _sink = sink; _loader = loader; }  // 虚函数：初始化数据读取模块

	/**
	 * @brief 分钟线闭合事件处理接口
	 * @param uDate 闭合的分钟线日期，如20200410，这里不是交易日
	 * @param uTime 闭合的分钟线的分钟时间，如1115
	 * @param endTDate 如果闭合的分钟线是交易日最后一条分钟线，则endTDate为当前交易日，如20200410，其他情况为0
	 * 
	 * 该函数在分钟线闭合时被调用，用于处理分钟线闭合事件。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate = 0) = 0;  // 纯虚函数：分钟线闭合事件处理接口

	/**
	 * @brief 读取Tick数据切片
	 * @param stdCode 标准品种代码，如SSE.600000、SHFE.au.2005
	 * @param count 要读取的tick条数
	 * @param etime 结束时间，精确到毫秒，格式如yyyyMMddhhmmssmmm，如果要读取到最后一条，etime为0，默认为0
	 * @return WTSTickSlice* 返回Tick数据切片指针
	 * 
	 * 该函数读取指定合约的Tick数据切片，切片不会复制数据，只把缓存中的数据指针传递出来，所以叫做切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickSlice*	readTickSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) = 0;  // 纯虚函数：读取Tick数据切片

	/**
	 * @brief 读取逐笔委托数据切片
	 * @param stdCode 标准品种代码，如SSE.600000、SHFE.au.2005
	 * @param count 要读取的tick条数
	 * @param etime 结束时间，精确到毫秒，格式如yyyyMMddhhmmssmmm，如果要读取到最后一条，etime为0，默认为0
	 * @return WTSOrdDtlSlice* 返回逐笔委托数据切片指针
	 * 
	 * 该函数读取指定合约的逐笔委托数据切片，切片不会复制数据，只把缓存中的数据指针传递出来，所以叫做切片。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSOrdDtlSlice*	readOrdDtlSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：读取逐笔委托数据切片，默认返回NULL

	/**
	 * @brief 读取委托队列数据切片
	 * @param stdCode 标准品种代码，如SSE.600000、SHFE.au.2005
	 * @param count 要读取的tick条数
	 * @param etime 结束时间，精确到毫秒，格式如yyyyMMddhhmmssmmm，如果要读取到最后一条，etime为0，默认为0
	 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
	 * 
	 * 该函数读取指定合约的委托队列数据切片，切片不会复制数据，只把缓存中的数据指针传递出来，所以叫做切片。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSOrdQueSlice*	readOrdQueSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：读取委托队列数据切片，默认返回NULL

	/**
	 * @brief 读取逐笔成交数据切片
	 * @param stdCode 标准品种代码，如SSE.600000、SHFE.au.2005
	 * @param count 要读取的tick条数
	 * @param etime 结束时间，精确到毫秒，格式如yyyyMMddhhmmssmmm，如果要读取到最后一条，etime为0，默认为0
	 * @return WTSTransSlice* 返回逐笔成交数据切片指针
	 * 
	 * 该函数读取指定合约的逐笔成交数据切片，切片不会复制数据，只把缓存中的数据指针传递出来，所以叫做切片。
	 * 默认实现返回NULL，子类可以重写此函数。
	 */
	virtual WTSTransSlice*	readTransSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) { return NULL; }  // 虚函数：读取逐笔成交数据切片，默认返回NULL

	/**
	 * @brief 读取K线序列，并返回一个存储容器类
	 * @param stdCode 标准品种代码，如SSE.600000、SHFE.au.2005
	 * @param period K线周期
	 * @param count 要读取的K线条数
	 * @param etime 结束时间，格式yyyyMMddhhmm
	 * @return WTSKlineSlice* 返回K线数据切片指针
	 * 
	 * 该函数读取指定合约的K线序列，切片不会复制数据，只把缓存中的数据指针传递出来，所以叫做切片。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSKlineSlice*	readKlineSlice(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime = 0) = 0;  // 纯虚函数：读取K线序列

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
	 * @brief 获取复权标记
	 * @return uint32_t 返回复权标记
	 * 
	 * 该函数获取复权标记，采用位运算1|2|4的形式：
	 * 1表示成交量复权，2表示成交额复权，4表示总持复权，其他待定。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t	getAdjustingFlag() { return 0; }  // 虚函数：获取复权标记，默认返回0

protected:
	IDataReaderSink*	_sink;  // 数据读取模块回调接口指针
	IHisDataLoader*		_loader;  // 历史数据加载器指针
};

/**
 * @typedef FuncCreateDataReader
 * @brief 创建数据读取器函数指针类型
 * 
 * 该类型定义了创建数据读取器的函数指针签名。
 * 用于动态加载数据读取器插件。
 */
typedef IDataReader* (*FuncCreateDataReader)();  // 创建数据读取器函数指针类型定义

/**
 * @typedef FuncDeleteDataReader
 * @brief 删除数据读取器函数指针类型
 * 
 * 该类型定义了删除数据读取器的函数指针签名。
 * 用于动态卸载数据读取器插件。
 */
typedef void(*FuncDeleteDataReader)(IDataReader* store);  // 删除数据读取器函数指针类型定义

NS_WTP_END  // 结束WonderTrader命名空间