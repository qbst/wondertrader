/*!
 * \file ExpParser.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展行情解析器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了ExpParser类的所有成员函数，完成扩展行情解析器的具体功能逻辑。
 * 
 * 主要功能包括：
 * 1. 实现解析器的初始化、连接、断开、释放等生命周期管理操作
 * 2. 实现合约订阅和退订功能，支持批量操作
 * 3. 实现回调接口的注册，建立与数据管理模块的连接
 * 4. 作为代理，将所有操作请求转发给WtDtRunner的全局单例进行实际处理
 * 5. 在转发时附带解析器ID，便于WtDtRunner识别和追踪
 * 
 * 设计思想：
 * - 采用代理模式，所有实现函数都是简单的转发调用
 * - 通过getRunner()获取全局WtDtRunner单例，保证系统中只有一个数据运行器
 * - 将解析器ID作为第一个参数传递给WtDtRunner，实现多解析器的识别和管理
 * - 订阅和退订操作遍历合约集合，逐个处理每个合约代码
 * - 函数实现简洁清晰，易于维护和扩展
 * 
 * 该文件是ExpParser功能的核心实现，所有解析器操作最终都通过WtDtRunner
 * 转发到外部注册的回调函数进行处理。
 */
#include "ExpParser.h"  // 包含ExpParser类声明
#include "WtDtRunner.h"  // 包含WtDtRunner类声明

// 外部声明：获取全局WtDtRunner单例的引用
// 该单例在WtDtPorter.cpp中定义，负责管理所有解析器操作
extern WtDtRunner& getRunner();

/**
 * @brief 初始化解析器
 * @param config 配置参数，包含解析器的初始化配置信息
 * @return bool 初始化成功返回true
 * 
 * 该函数将初始化请求转发给WtDtRunner处理。
 * WtDtRunner会触发外部注册的初始化回调函数，通知外部模块进行初始化操作。
 * 当前实现总是返回true，表示初始化成功。
 */
bool ExpParser::init(WTSVariant* config)
{
	// 调用WtDtRunner的parser_init方法，将解析器ID传递给回调函数
	// 外部模块根据解析器ID进行相应的初始化操作
	getRunner().parser_init(_id.c_str());
	return true;  // 总是返回true，表示初始化成功
}

/**
 * @brief 释放解析器资源
 * 
 * 该函数将释放请求转发给WtDtRunner处理。
 * WtDtRunner会触发外部注册的释放回调函数，通知外部模块清理资源。
 * 外部模块应在此回调中关闭连接、释放内存等清理操作。
 */
void ExpParser::release()
{
	// 调用WtDtRunner的parser_release方法，通知外部模块释放资源
	// 外部模块根据解析器ID识别需要释放的解析器实例
	getRunner().parser_release(_id.c_str());
}

/**
 * @brief 连接到数据源
 * @return bool 连接成功返回true
 * 
 * 该函数将连接请求转发给WtDtRunner处理。
 * WtDtRunner会触发外部注册的连接回调函数，通知外部模块建立与数据源的连接。
 * 当前实现总是返回true，表示连接请求已发送。
 */
bool ExpParser::connect()
{
	// 调用WtDtRunner的parser_connect方法，通知外部模块建立连接
	// 外部模块根据解析器ID识别需要连接的数据源
	getRunner().parser_connect(_id.c_str());
	return true;  // 总是返回true，表示连接请求已发送
}

/**
 * @brief 断开与数据源的连接
 * @return bool 断开成功返回true
 * 
 * 该函数将断开连接请求转发给WtDtRunner处理。
 * WtDtRunner会触发外部注册的断开连接回调函数，通知外部模块关闭与数据源的连接。
 * 当前实现总是返回true，表示断开连接请求已发送。
 */
bool ExpParser::disconnect()
{
	// 调用WtDtRunner的parser_disconnect方法，通知外部模块断开连接
	// 外部模块根据解析器ID识别需要断开连接的数据源
	getRunner().parser_disconnect(_id.c_str());
	return true;  // 总是返回true，表示断开连接请求已发送
}

/**
 * @brief 订阅合约行情
 * @param setCodes 要订阅的合约代码集合
 * 
 * 该函数将订阅请求转发给WtDtRunner处理。
 * 遍历合约集合，对每个合约代码调用WtDtRunner的订阅方法。
 * WtDtRunner会触发外部注册的订阅回调函数，通知外部模块向数据源发送订阅请求。
 */
void ExpParser::subscribe(const CodeSet& setCodes)
{
	// 遍历合约代码集合，对每个合约进行订阅
	for(const auto& code : setCodes)
		// 调用WtDtRunner的parser_subscribe方法，传递解析器ID和合约代码
		// 外部模块根据这些信息向数据源发送订阅请求
		getRunner().parser_subscribe(_id.c_str(), code.c_str());
}

/**
 * @brief 退订合约行情
 * @param setCodes 要退订的合约代码集合
 * 
 * 该函数将退订请求转发给WtDtRunner处理。
 * 遍历合约集合，对每个合约代码调用WtDtRunner的退订方法。
 * WtDtRunner会触发外部注册的退订回调函数，通知外部模块向数据源发送退订请求。
 */
void ExpParser::unsubscribe(const CodeSet& setCodes)
{
	// 遍历合约代码集合，对每个合约进行退订
	for (const auto& code : setCodes)
		// 调用WtDtRunner的parser_unsubscribe方法，传递解析器ID和合约代码
		// 外部模块根据这些信息向数据源发送退订请求
		getRunner().parser_unsubscribe(_id.c_str(), code.c_str());
}

/**
 * @brief 注册回调接口
 * @param listener 回调接口指针，用于接收行情数据和事件
 * 
 * 该函数保存回调接口指针，并从回调接口获取基础数据管理器。
 * 回调接口用于向系统推送行情数据和事件。
 * 基础数据管理器用于访问合约、交易所等基础信息。
 */
void ExpParser::registerSpi(IParserSpi* listener)
{
	// 保存回调接口指针
	m_sink = listener;

	// 如果回调接口指针有效，则从回调接口获取基础数据管理器
	// 基础数据管理器用于访问合约、交易所、交易时段等基础信息
	if (m_sink)
		m_pBaseDataMgr = m_sink->getBaseDataMgr();
}
