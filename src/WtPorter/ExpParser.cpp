/*!
 * \file ExpParser.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展Parser类实现文件
 * 
 * 本文件实现了ExpParser类的所有方法，通过WtRtRunner将Parser的操作转发给外部语言实现的回调函数。
 * 外部语言通过注册的回调函数接收Parser事件通知，并通过parser_push_quote接口推送行情数据。
 */
#include "ExpParser.h"  // ExpParser类定义
#include "WtRtRunner.h"  // 运行时运行器

extern WtRtRunner& getRunner();  // 获取WtRtRunner单例对象

/**
 * @brief 初始化Parser
 * 
 * 调用WtRtRunner的parser_init方法，触发外部语言实现的初始化回调
 * 
 * @param config 配置信息（当前未使用）
 * @return 总是返回true
 */
bool ExpParser::init(WTSVariant* config)
{
	getRunner().parser_init(_id.c_str());  // 通知外部语言Parser初始化事件
	return true;
}

/**
 * @brief 释放Parser
 * 
 * 调用WtRtRunner的parser_release方法，触发外部语言实现的释放回调
 */
void ExpParser::release()
{
	getRunner().parser_release(_id.c_str());  // 通知外部语言Parser释放事件
}

/**
 * @brief 连接数据源
 * 
 * 调用WtRtRunner的parser_connect方法，触发外部语言实现的连接回调
 * 
 * @return 总是返回true（实际连接状态由外部语言控制）
 */
bool ExpParser::connect()
{
	getRunner().parser_connect(_id.c_str());  // 通知外部语言Parser连接事件
	return true;
}

/**
 * @brief 断开数据源连接
 * 
 * 调用WtRtRunner的parser_disconnect方法，触发外部语言实现的断开回调
 * 
 * @return 总是返回true
 */
bool ExpParser::disconnect()
{
	getRunner().parser_disconnect(_id.c_str());  // 通知外部语言Parser断开连接事件
	return true;
}

/**
 * @brief 订阅合约
 * 
 * 遍历合约代码集合，对每个合约调用WtRtRunner的parser_subscribe方法，
 * 触发外部语言实现的订阅回调
 * 
 * @param setCodes 合约代码集合
 */
void ExpParser::subscribe(const CodeSet& setCodes)
{
	for(const auto& code : setCodes)  // 遍历所有合约代码
		getRunner().parser_subscribe(_id.c_str(), code.c_str());  // 通知外部语言订阅该合约
}

/**
 * @brief 取消订阅合约
 * 
 * 遍历合约代码集合，对每个合约调用WtRtRunner的parser_unsubscribe方法，
 * 触发外部语言实现的取消订阅回调
 * 
 * @param setCodes 合约代码集合
 */
void ExpParser::unsubscribe(const CodeSet& setCodes)
{
	for (const auto& code : setCodes)  // 遍历所有合约代码
		getRunner().parser_unsubscribe(_id.c_str(), code.c_str());  // 通知外部语言取消订阅该合约
}

/**
 * @brief 注册事件监听器
 * 
 * 保存事件监听器指针，并获取基础数据管理器指针
 * 
 * @param listener 事件监听器指针（用于接收和分发行情数据）
 */
void ExpParser::registerSpi(IParserSpi* listener)
{
	m_sink = listener;  // 保存事件监听器指针

	if (m_sink)  // 如果监听器有效，获取基础数据管理器
		m_pBaseDataMgr = m_sink->getBaseDataMgr();  // 获取基础数据管理器，用于查询合约信息等
}
