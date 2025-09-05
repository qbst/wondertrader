/*!
 * \file IParserApi.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 行情解析模块接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中行情解析模块的核心接口，负责处理各种数据源的行情数据解析。
 * 主要功能包括：
 * 1. 定义行情解析回调接口，处理合约列表、实时行情、委托队列、委托明细、逐笔成交等数据
 * 2. 定义行情解析模块接口，提供初始化、连接、订阅、退订等基本操作
 * 3. 支持多种数据源和不同格式的行情数据解析
 * 4. 提供事件处理、日志记录、基础数据管理等辅助功能
 * 5. 支持插件化的解析器创建和删除
 * 
 * 该类主要用于WonderTrader框架中的行情数据解析系统，为策略引擎和数据处理系统提供标准化的行情数据接口。
 * 通过抽象接口设计，支持多种数据源和不同格式的行情数据解析。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串类型
#include <stdint.h>  // 包含固定大小整数类型
#include "WTSTypes.h"  // 包含WTS类型定义
#include "FasterDefs.h"  // 包含快速容器定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSTickData;  // 前向声明：WTS Tick数据类
class WTSOrdDtlData;  // 前向声明：WTS委托明细数据类
class WTSOrdQueData;  // 前向声明：WTS委托队列数据类
class WTSTransData;  // 前向声明：WTS逐笔成交数据类
class WTSVariant;  // 前向声明：WTS变体类型类
class WTSArray;  // 前向声明：WTS数组类
class IBaseDataMgr;  // 前向声明：基础数据管理器接口

/**
 * @class IParserSpi
 * @brief 行情解析模块回调接口类
 * 
 * 该类定义了行情解析模块的回调接口，用于处理各种行情数据和事件。
 * 包括合约列表、实时行情、委托队列、委托明细、逐笔成交等数据的处理，以及事件和日志的处理。
 * 
 * 主要特性：
 * - 处理模块事件（连接、断开、登录、登出等）
 * - 处理合约列表信息
 * - 处理实时行情数据，支持不同处理标记
 * - 处理委托队列和委托明细数据（股票level2）
 * - 处理逐笔成交数据
 * - 处理解析模块的日志
 * - 提供基础数据管理器访问接口
 * - 统一的回调接口设计
 */
class IParserSpi
{
public:
	/**
	 * @brief 处理模块事件
	 * @param e 事件类型，如连接、断开、登录、登出
	 * @param ec 错误码，0为没有错误
	 * 
	 * 该函数处理解析模块的各种事件，如连接状态变化、登录状态变化等。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void handleEvent(WTSParserEvent e, int32_t ec){}  // 虚函数：处理模块事件，默认实现为空

	/**
	 * @brief 处理合约列表
	 * @param aySymbols 合约列表，基础元素为WTSContractInfo，WTSArray的用法请参考定义
	 * 
	 * 该函数处理从数据源获取的合约列表信息，用于更新本地合约信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void handleSymbolList(const WTSArray* aySymbols)		= 0;  // 纯虚函数：处理合约列表

	/**
	 * @brief 处理实时行情
	 * @param quote 实时行情数据指针
	 * @param procFlag 处理标记：0-切片行情，无需处理(ParserUDP)；1-完整快照，需要切片(国内各路通道)；2-极简快照，需要缓存累加（主要针对日线、tick，m1和m5都是自动累加的，虚拟货币行情）
	 * 
	 * 该函数处理从数据源获取的实时行情数据，根据处理标记进行相应的数据处理。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void handleQuote(WTSTickData *quote, uint32_t procFlag)	= 0;  // 纯虚函数：处理实时行情

	/**
	 * @brief 处理委托队列数据（股票level2）
	 * @param ordQueData 委托队列数据指针
	 * 
	 * 该函数处理从数据源获取的委托队列数据，主要用于股票level2行情。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void handleOrderQueue(WTSOrdQueData* ordQueData){}  // 虚函数：处理委托队列数据，默认实现为空

	/**
	 * @brief 处理逐笔委托数据（股票level2）
	 * @param ordDetailData 逐笔委托数据指针
	 * 
	 * 该函数处理从数据源获取的逐笔委托数据，主要用于股票level2行情。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void handleOrderDetail(WTSOrdDtlData* ordDetailData){}  // 虚函数：处理逐笔委托数据，默认实现为空

	/**
	 * @brief 处理逐笔成交数据
	 * @param transData 逐笔成交数据指针
	 * 
	 * 该函数处理从数据源获取的逐笔成交数据。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void handleTransaction(WTSTransData* transData){}  // 虚函数：处理逐笔成交数据，默认实现为空

	/**
	 * @brief 处理解析模块的日志
	 * @param ll 日志级别
	 * @param message 日志内容
	 * 
	 * 该函数处理解析模块产生的日志信息，用于日志记录和输出。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void handleParserLog(WTSLogLevel ll, const char* message)	= 0;  // 纯虚函数：处理解析模块的日志

public:
	/**
	 * @brief 获取基础数据管理器接口指针
	 * @return IBaseDataMgr* 返回基础数据管理器接口指针
	 * 
	 * 该函数返回基础数据管理器接口指针，用于访问基础数据管理功能。
	 * 纯虚函数，子类必须实现。
	 */
	virtual IBaseDataMgr*	getBaseDataMgr()	= 0;  // 纯虚函数：获取基础数据管理器接口指针
};

/**
 * @class IParserApi
 * @brief 行情解析模块接口类
 * 
 * 该类定义了WonderTrader框架中行情解析模块的核心接口，提供行情数据解析的基本操作。
 * 包括模块初始化、连接管理、合约订阅、回调注册等功能。
 * 
 * 主要特性：
 * - 模块初始化和释放管理
 * - 服务器连接和断开管理
 * - 连接状态查询
 * - 合约订阅和退订管理
 * - 回调接口注册
 * - 统一的接口设计，支持多种数据源
 * - 插件化的架构设计
 */
class IParserApi
{
public:
	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构。
	 * 支持多态使用和正确的内存管理。
	 */
	virtual ~IParserApi(){}  // 虚析构函数，支持继承

public:
	/**
	 * @brief 初始化解析模块
	 * @param config 模块配置参数
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数初始化行情解析模块，设置配置参数。
	 * 默认实现返回false，子类可以重写此函数进行自定义初始化。
	 */
	virtual bool init(WTSVariant* config) { return false; }  // 虚函数：初始化解析模块，默认返回false

	/**
	 * @brief 释放解析模块
	 * 
	 * 该函数释放解析模块占用的资源，用于退出时的清理。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void release(){}  // 虚函数：释放解析模块，默认实现为空

	/**
	 * @brief 开始连接服务器
	 * @return bool 连接命令发送成功返回true，失败返回false
	 * 
	 * 该函数开始连接行情数据服务器，建立数据通道。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool connect() { return false; }  // 虚函数：开始连接服务器，默认返回false

	/**
	 * @brief 断开连接
	 * @return bool 断开命令发送成功返回true，失败返回false
	 * 
	 * 该函数断开与行情数据服务器的连接。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool disconnect() { return false; }  // 虚函数：断开连接，默认返回false

	/**
	 * @brief 查询是否已连接
	 * @return bool 已连接返回true，未连接返回false
	 * 
	 * 该函数查询当前与行情数据服务器的连接状态。
	 * 默认实现返回false，子类可以重写此函数。
	 */
	virtual bool isConnected() { return false; }  // 虚函数：查询是否已连接，默认返回false

	/**
	 * @brief 订阅合约列表
	 * @param setCodes 要订阅的合约代码集合
	 * 
	 * 该函数订阅指定合约的行情数据，开始接收相关数据。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void subscribe(const CodeSet& setCodes){}  // 虚函数：订阅合约列表，默认实现为空

	/**
	 * @brief 退订合约列表
	 * @param setCodes 要退订的合约代码集合
	 * 
	 * 该函数退订指定合约的行情数据，停止接收相关数据。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void unsubscribe(const CodeSet& setCodes){}  // 虚函数：退订合约列表，默认实现为空

	/**
	 * @brief 注册回调接口
	 * @param spi 回调接口指针
	 * 
	 * 该函数注册回调接口，用于接收解析模块的各种事件和数据。
	 * 默认实现为空，子类可以重写此函数。
	 */
	virtual void registerSpi(IParserSpi* spi) {}  // 虚函数：注册回调接口，默认实现为空
};

NS_WTP_END  // 结束WonderTrader命名空间

/**
 * @typedef FuncCreateParser
 * @brief 创建解析器函数指针类型
 * 
 * 该类型定义了创建行情解析器的函数指针签名。
 * 用于动态加载行情解析器插件。
 */
typedef wtp::IParserApi* (*FuncCreateParser)();  // 创建解析器函数指针类型定义

/**
 * @typedef FuncDeleteParser
 * @brief 删除解析器函数指针类型
 * 
 * 该类型定义了删除行情解析器的函数指针签名。
 * 用于动态卸载行情解析器插件。
 */
typedef void(*FuncDeleteParser)(wtp::IParserApi* &parser);  // 删除解析器函数指针类型定义