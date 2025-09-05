/*!
 * \file IDataWriter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据落地接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中数据落地系统的核心接口，负责将实时行情数据写入存储系统。
 * 主要功能包括：
 * 1. 定义数据写入回调接口，支持基础数据管理、会话接收判断、数据广播等功能
 * 2. 定义历史数据转储器接口，支持K线、Tick、委托明细、委托队列、逐笔成交等数据的转储
 * 3. 定义数据写入器接口，提供实时数据的写入和历史数据的转储功能
 * 4. 支持扩展转储器的注册和管理
 * 5. 提供数据写入器的创建和删除函数指针类型
 * 
 * 该类主要用于WonderTrader框架中的数据落地系统，为实时数据存储和历史数据管理提供统一接口。
 * 通过抽象接口设计，支持多种存储后端和不同格式的数据写入。
 */
#pragma once  // 防止头文件重复包含
#include <stdint.h>  // 包含固定大小整数类型
#include "WTSTypes.h"  // 包含WTS类型定义
#include "FasterDefs.h"  // 包含快速容器定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSTickData;  // 前向声明：WTS Tick数据类
class WTSOrdQueData;  // 前向声明：WTS委托队列数据类
class WTSOrdDtlData;  // 前向声明：WTS委托明细数据类
class WTSTransData;  // 前向声明：WTS逐笔成交数据类
class WTSVariant;  // 前向声明：WTS变体类型类
class IBaseDataMgr;  // 前向声明：基础数据管理器接口
struct WTSBarStruct;  // 前向声明：WTS K线结构体
struct WTSTickStruct;  // 前向声明：WTS Tick结构体
struct WTSOrdDtlStruct;  // 前向声明：WTS委托明细结构体
struct WTSOrdQueStruct;  // 前向声明：WTS委托队列结构体
struct WTSTransStruct;  // 前向声明：WTS逐笔成交结构体

/**
 * @class IDataWriterSink
 * @brief 数据写入回调接口类
 * 
 * 该类定义了数据写入模块的回调接口，主要用于数据写入模块与调用模块之间的交互。
 * 支持基础数据管理、会话接收判断、数据广播、日志输出等功能的回调。
 * 
 * 主要特性：
 * - 提供基础数据管理器的获取接口
 * - 支持会话接收能力的判断
 * - 提供多种数据类型的广播接口
 * - 支持会话合约集合的获取
 * - 提供交易日期和日志输出的回调接口
 * - 统一的回调接口设计
 */
class IDataWriterSink
{
public:
	/**
	 * @brief 获取基础数据管理器接口指针
	 * @return IBaseDataMgr* 返回基础数据管理器接口指针
	 * 
	 * 该函数返回基础数据管理器接口指针，用于访问基础数据管理功能。
	 * 纯虚函数，子类必须实现。
	 */
	virtual IBaseDataMgr* getBDMgr() = 0;  // 纯虚函数：获取基础数据管理器接口指针

	/**
	 * @brief 判断指定会话是否可以接收数据
	 * @param sid 会话ID
	 * @return bool 可以接收返回true，否则返回false
	 * 
	 * 该函数判断指定会话是否可以接收数据，用于控制数据的分发。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool canSessionReceive(const char* sid) = 0;  // 纯虚函数：判断指定会话是否可以接收数据

	/**
	 * @brief 广播Tick数据
	 * @param curTick 当前Tick数据指针
	 * 
	 * 该函数广播Tick数据，用于向所有订阅者分发最新的Tick数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void broadcastTick(WTSTickData* curTick) = 0;  // 纯虚函数：广播Tick数据

	/**
	 * @brief 广播委托队列数据
	 * @param curOrdQue 当前委托队列数据指针
	 * 
	 * 该函数广播委托队列数据，用于向所有订阅者分发最新的委托队列数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void broadcastOrdQue(WTSOrdQueData* curOrdQue) = 0;  // 纯虚函数：广播委托队列数据

	/**
	 * @brief 广播委托明细数据
	 * @param curOrdDtl 当前委托明细数据指针
	 * 
	 * 该函数广播委托明细数据，用于向所有订阅者分发最新的委托明细数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void broadcastOrdDtl(WTSOrdDtlData* curOrdDtl) = 0;  // 纯虚函数：广播委托明细数据

	/**
	 * @brief 广播逐笔成交数据
	 * @param curTrans 当前逐笔成交数据指针
	 * 
	 * 该函数广播逐笔成交数据，用于向所有订阅者分发最新的逐笔成交数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void broadcastTrans(WTSTransData* curTrans) = 0;  // 纯虚函数：广播逐笔成交数据

	/**
	 * @brief 获取指定会话的合约集合
	 * @param sid 会话ID
	 * @return CodeSet* 返回合约集合指针
	 * 
	 * 该函数获取指定会话的合约集合，用于确定需要处理哪些合约的数据。
	 * 纯虚函数，子类必须实现。
	 */
	virtual CodeSet* getSessionComms(const char* sid) = 0;  // 纯虚函数：获取指定会话的合约集合

	/**
	 * @brief 获取指定产品的交易日期
	 * @param pid 产品ID
	 * @return uint32_t 返回交易日期，格式为YYYYMMDD
	 * 
	 * 该函数获取指定产品的交易日期，用于数据的时间管理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t getTradingDate(const char* pid) = 0;  // 纯虚函数：获取指定产品的交易日期

	/**
	 * @brief 处理解析模块的日志
	 * @param ll 日志级别
	 * @param message 日志内容
	 * 
	 * 该函数处理解析模块的日志输出，支持不同级别的日志处理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void outputLog(WTSLogLevel ll, const char* message) = 0;  // 纯虚函数：处理解析模块的日志
};

/**
 * @class IHisDataDumper
 * @brief 历史数据转储器接口类
 * 
 * 该类定义了历史数据转储器的核心接口，负责将历史数据转储到存储系统。
 * 支持K线、Tick、委托明细、委托队列、逐笔成交等数据的转储。
 * 
 * 主要特性：
 * - 支持历史K线数据的转储
 * - 支持历史Tick数据的转储
 * - 支持历史委托队列数据的转储
 * - 支持历史委托明细数据的转储
 * - 支持历史逐笔成交数据的转储
 * - 统一的接口设计，支持多种存储后端
 */
class IHisDataDumper
{
public:
	/**
	 * @brief 转储历史K线数据
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param bars K线数据数组
	 * @param count 数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史K线数据转储到存储系统。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool dumpHisBars(const char* stdCode, const char* period, WTSBarStruct* bars, uint32_t count) = 0;  // 纯虚函数：转储历史K线数据

	/**
	 * @brief 转储历史Tick数据
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param ticks Tick数据数组
	 * @param count 数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史Tick数据转储到存储系统。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool dumpHisTicks(const char* stdCode, uint32_t uDate, WTSTickStruct* ticks, uint32_t count) = 0;  // 纯虚函数：转储历史Tick数据

	/**
	 * @brief 转储历史委托队列数据
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param items 委托队列数据数组
	 * @param count 数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史委托队列数据转储到存储系统。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool dumpHisOrdQue(const char* stdCode, uint32_t uDate, WTSOrdQueStruct* items, uint32_t count) { return false; }  // 虚函数：转储历史委托队列数据，默认返回false

	/**
	 * @brief 转储历史委托明细数据
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param items 委托明细数据数组
	 * @param count 数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史委托明细数据转储到存储系统。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool dumpHisOrdDtl(const char* stdCode, uint32_t uDate, WTSOrdDtlStruct* items, uint32_t count) { return false; }  // 虚函数：转储历史委托明细数据，默认返回false

	/**
	 * @brief 转储历史逐笔成交数据
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param items 逐笔成交数据数组
	 * @param count 数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史逐笔成交数据转储到存储系统。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool dumpHisTrans(const char* stdCode, uint32_t uDate, WTSTransStruct* items, uint32_t count) { return false; }  // 虚函数：转储历史逐笔成交数据，默认返回false
};

/**
 * @typedef ExtDumpers
 * @brief 扩展转储器映射表类型
 * 
 * 该类型定义了扩展转储器的映射表，用于管理多个扩展转储器。
 * 使用wt_hashmap容器，键为转储器ID，值为转储器接口指针。
 */
typedef wt_hashmap<std::string, IHisDataDumper*> ExtDumpers;  // 扩展转储器映射表类型定义

/**
 * @class IDataWriter
 * @brief 数据落地接口类
 * 
 * 该类定义了WonderTrader框架中数据落地器的核心接口，负责将实时行情数据写入存储系统。
 * 支持Tick、委托队列、委托明细、逐笔成交等数据的写入，并提供历史数据的转储功能。
 * 
 * 主要特性：
 * - 支持多种数据类型的数据写入（Tick、委托队列、委托明细、逐笔成交）
 * - 提供扩展转储器的注册和管理
 * - 支持历史数据的转储功能
 * - 提供会话处理状态的查询
 * - 统一的接口设计，支持多种存储后端
 */
class IDataWriter
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化数据落地器对象，设置回调接口为NULL。
	 */
	IDataWriter():_sink(NULL){}  // 初始化回调接口为NULL

	/**
	 * @brief 初始化数据落地器
	 * @param params 配置参数
	 * @param sink 回调接口指针
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数初始化数据落地器，设置配置参数和回调接口。
	 * 默认实现保存回调接口并返回true，子类可以重写此函数进行自定义初始化。
	 */
	virtual bool init(WTSVariant* params, IDataWriterSink* sink) { _sink = sink; return true; }  // 虚函数：初始化数据落地器

	/**
	 * @brief 释放资源
	 * 
	 * 该函数释放数据落地器占用的资源，用于清理和析构。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void release() = 0;  // 纯虚函数：释放资源

	/**
	 * @brief 添加扩展转储器
	 * @param id 转储器ID
	 * @param dumper 转储器接口指针
	 * 
	 * 该函数注册一个扩展转储器，用于支持额外的数据转储功能。
	 */
	void	add_ext_dumper(const char* id, IHisDataDumper* dumper) { _dumpers[id] = dumper; }  // 添加扩展转储器

public:
	/**
	 * @brief 写入Tick数据
	 * @param curTick 当前Tick数据指针
	 * @param procFlag 处理标志
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * 该函数将Tick数据写入存储系统，支持处理标志的控制。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool writeTick(WTSTickData* curTick, uint32_t procFlag) = 0;  // 纯虚函数：写入Tick数据

	/**
	 * @brief 写入委托队列数据
	 * @param curOrdQue 当前委托队列数据指针
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * 该函数将委托队列数据写入存储系统。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool writeOrderQueue(WTSOrdQueData* curOrdQue) { return false; }  // 虚函数：写入委托队列数据，默认返回false

	/**
	 * @brief 写入委托明细数据
	 * @param curOrdDetail 当前委托明细数据指针
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * 该函数将委托明细数据写入存储系统。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool writeOrderDetail(WTSOrdDtlData* curOrdDetail) { return false; }  // 虚函数：写入委托明细数据，默认返回false

	/**
	 * @brief 写入逐笔成交数据
	 * @param curTrans 当前逐笔成交数据指针
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * 该函数将逐笔成交数据写入存储系统。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool writeTransaction(WTSTransData* curTrans) { return false; }  // 虚函数：写入逐笔成交数据，默认返回false

	/**
	 * @brief 转储历史数据
	 * @param sid 会话ID
	 * 
	 * 该函数转储指定会话的历史数据，用于数据的持久化存储。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void transHisData(const char* sid) {}  // 虚函数：转储历史数据，默认实现为空

	/**
	 * @brief 判断会话是否已处理
	 * @param sid 会话ID
	 * @return bool 已处理返回true，否则返回false
	 * 
	 * 该函数判断指定会话是否已经处理完成，用于控制数据处理的流程。
	 * 默认实现返回true，子类可以重写此函数。
	 */
	virtual bool isSessionProceeded(const char* sid) { return true; }  // 虚函数：判断会话是否已处理，默认返回true

	/**
	 * @brief 获取当前Tick数据
	 * @param code 合约代码
	 * @param exchg 交易所代码，默认为空字符串
	 * @return WTSTickData* 返回当前Tick数据指针
	 * 
	 * 该函数获取指定合约的当前Tick数据，用于数据的查询和访问。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSTickData* getCurTick(const char* code, const char* exchg = "") = 0;  // 纯虚函数：获取当前Tick数据

protected:
	ExtDumpers			_dumpers;  // 扩展转储器映射表
	IDataWriterSink*	_sink;  // 数据写入回调接口指针
};

NS_WTP_END  // 结束WonderTrader命名空间


/**
 * @typedef FuncCreateWriter
 * @brief 创建数据写入器函数指针类型
 * 
 * 该类型定义了创建数据写入器的函数指针签名。
 * 用于动态加载数据写入器插件。
 */
typedef wtp::IDataWriter* (*FuncCreateWriter)();  // 创建数据写入器函数指针类型定义

/**
 * @typedef FuncDeleteWriter
 * @brief 删除数据写入器函数指针类型
 * 
 * 该类型定义了删除数据写入器的函数指针签名。
 * 用于动态卸载数据写入器插件。
 */
typedef void(*FuncDeleteWriter)(wtp::IDataWriter* &writer);  // 删除数据写入器函数指针类型定义