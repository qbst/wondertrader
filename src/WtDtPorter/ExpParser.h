/*!
 * \file ExpParser.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展行情解析器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了ExpParser（扩展行情解析器）类，该类是IParserApi接口的实现类。
 * 
 * 主要功能包括：
 * 1. 实现行情解析器接口，支持初始化、连接、订阅、退订等基本操作
 * 2. 作为外部行情数据源与WonderTrader核心系统之间的桥接器
 * 3. 将解析器操作请求转发给WtDtRunner进行实际处理
 * 4. 通过唯一ID标识不同的解析器实例，支持多个数据源同时工作
 * 5. 保存回调接口和基础数据管理器的引用，便于数据处理和管理
 * 
 * 设计思想：
 * - 采用代理模式，ExpParser作为代理，将实际的解析工作委托给WtDtRunner处理
 * - 实现了IParserApi接口，符合系统的统一行情解析规范
 * - 每个ExpParser实例通过唯一ID标识，便于管理和追踪
 * - 支持扩展，外部可以通过此类接入自定义的行情数据源
 * - 轻量级设计，主要负责请求转发，不包含复杂的业务逻辑
 * 
 * 该类主要用于将外部行情数据源接入WonderTrader系统，实现了数据源与系统核心的解耦。
 * 外部数据源通过ExpParser接口将行情数据推送到系统中，由系统统一处理和分发。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/IParserApi.h"  // 包含行情解析器接口定义

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class ExpParser
 * @brief 扩展行情解析器类
 * 
 * ExpParser是IParserApi接口的实现类，用于接入外部行情数据源。
 * 该类作为桥接器，将外部数据源的行情推送到WonderTrader系统中。
 * 
 * 主要特性：
 * - 实现了IParserApi接口的所有虚函数
 * - 支持初始化、连接、订阅、退订等基本操作
 * - 通过唯一ID标识不同的解析器实例
 * - 采用代理模式，将实际的解析工作委托给WtDtRunner处理
 * - 保存回调接口和基础数据管理器的引用
 * - 轻量级设计，仅保存必要的引用，不保存行情数据
 * 
 * 使用场景：
 * 当需要接入自定义行情数据源（如第三方API、内部数据源）时，
 * 可以创建ExpParser实例并注册相应的回调函数来推送行情数据。
 */
class ExpParser : public IParserApi
{
public:
	/**
	 * @brief 构造函数
	 * @param id 解析器唯一标识符，用于区分不同的解析器实例
	 * 
	 * 创建一个ExpParser实例，并使用指定的ID进行标识。
	 * ID将用于在回调函数中识别是哪个解析器发起的请求。
	 */
	ExpParser(const char* id):_id(id){}  // 初始化解析器ID
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 虚析构函数确保继承类能够正确析构，释放所有资源。
	 * 当前实现为空，因为ExpParser不持有需要手动释放的资源。
	 */
	virtual ~ExpParser(){}  // 虚析构函数

public:
	/**
	 * @brief 初始化解析器
	 * @param config 配置参数，包含解析器的初始化配置
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数重写了IParserApi接口的init方法，
	 * 将初始化请求转发给WtDtRunner进行处理。
	 * WtDtRunner会触发外部注册的初始化回调函数。
	 */
	virtual bool init(WTSVariant* config) override;

	/**
	 * @brief 释放解析器资源
	 * 
	 * 该函数重写了IParserApi接口的release方法，
	 * 将释放请求转发给WtDtRunner进行处理。
	 * WtDtRunner会触发外部注册的释放回调函数，清理资源。
	 */
	virtual void release() override;

	/**
	 * @brief 连接到数据源
	 * @return bool 连接成功返回true，失败返回false
	 * 
	 * 该函数重写了IParserApi接口的connect方法，
	 * 将连接请求转发给WtDtRunner进行处理。
	 * WtDtRunner会触发外部注册的连接回调函数，建立与数据源的连接。
	 */
	virtual bool connect() override;

	/**
	 * @brief 断开与数据源的连接
	 * @return bool 断开成功返回true，失败返回false
	 * 
	 * 该函数重写了IParserApi接口的disconnect方法，
	 * 将断开连接请求转发给WtDtRunner进行处理。
	 * WtDtRunner会触发外部注册的断开连接回调函数，关闭与数据源的连接。
	 */
	virtual bool disconnect() override;

	/**
	 * @brief 查询连接状态
	 * @return bool 已连接返回true，未连接返回false
	 * 
	 * 该函数重写了IParserApi接口的isConnected方法。
	 * 当前实现始终返回true，表示解析器处于连接状态。
	 * 实际的连接状态由外部数据源维护。
	 */
	virtual bool isConnected() override { return true; }  // 始终返回true

	/**
	 * @brief 订阅合约行情
	 * @param setCodes 要订阅的合约代码集合
	 * 
	 * 该函数重写了IParserApi接口的subscribe方法，
	 * 将订阅请求转发给WtDtRunner进行处理。
	 * WtDtRunner会触发外部注册的订阅回调函数，向数据源发送订阅请求。
	 */
	virtual void subscribe(const CodeSet& setCodes) override;

	/**
	 * @brief 退订合约行情
	 * @param setCodes 要退订的合约代码集合
	 * 
	 * 该函数重写了IParserApi接口的unsubscribe方法，
	 * 将退订请求转发给WtDtRunner进行处理。
	 * WtDtRunner会触发外部注册的退订回调函数，向数据源发送退订请求。
	 */
	virtual void unsubscribe(const CodeSet& setCodes) override;

	/**
	 * @brief 注册回调接口
	 * @param listener 回调接口指针，用于接收行情数据和事件
	 * 
	 * 该函数重写了IParserApi接口的registerSpi方法，
	 * 保存回调接口指针，并从回调接口获取基础数据管理器。
	 * 回调接口用于向系统推送行情数据和事件。
	 */
	virtual void registerSpi(IParserSpi* listener) override;

private:
	std::string			_id;            // 解析器唯一标识符，用于在回调函数中识别解析器实例
	IParserSpi*			m_sink;         // 回调接口指针，用于接收行情数据和事件
	IBaseDataMgr*		m_pBaseDataMgr; // 基础数据管理器指针，用于访问合约、交易所等基础信息
};

