/*!
 * \file ExpParser.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 扩展Parser类定义文件
 * 
 * 本文件定义了ExpParser类，用于实现扩展的行情解析器（Parser）。
 * ExpParser继承自IParserApi接口，是外部语言（如Python）实现Parser的桥梁。
 * 
 * 设计逻辑：
 * - ExpParser作为适配器，将外部语言实现的Parser逻辑桥接到WonderTrader框架
 * - 通过WtRtRunner的回调机制，将Parser的事件（初始化、连接、断开等）和订阅请求
 *   转发给外部语言实现的回调函数
 * - 外部语言通过parser_push_quote接口推送行情数据到ExpParser
 * - ExpParser将接收到的行情数据通过IParserSpi接口分发给策略
 */
#pragma once
#include "../Includes/IParserApi.h"  // Parser API接口定义

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 扩展Parser类
 * 
 * 扩展的行情解析器类，用于外部语言实现的Parser与WonderTrader框架的桥接
 */
class ExpParser : public IParserApi
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param id Parser的唯一标识符
	 */
	ExpParser(const char* id):_id(id){}  // 初始化Parser ID

	/**
	 * @brief 析构函数
	 */
	virtual ~ExpParser(){}

public:
	/**
	 * @brief 初始化Parser
	 * 
	 * 初始化Parser，调用外部语言实现的初始化回调
	 * 
	 * @param config 配置信息
	 * @return 是否初始化成功
	 */
	virtual bool init(WTSVariant* config) override;

	/**
	 * @brief 释放Parser
	 * 
	 * 释放Parser资源，调用外部语言实现的释放回调
	 */
	virtual void release() override;

	/**
	 * @brief 连接数据源
	 * 
	 * 连接到行情数据源，调用外部语言实现的连接回调
	 * 
	 * @return 是否连接成功
	 */
	virtual bool connect() override;

	/**
	 * @brief 断开数据源连接
	 * 
	 * 断开与行情数据源的连接，调用外部语言实现的断开回调
	 * 
	 * @return 是否断开成功
	 */
	virtual bool disconnect() override;

	/**
	 * @brief 检查连接状态
	 * 
	 * 检查Parser是否已连接到数据源
	 * 
	 * @return 连接状态（扩展Parser始终返回true，由外部语言控制实际连接状态）
	 */
	virtual bool isConnected() override { return true; }

	/**
	 * @brief 订阅合约
	 * 
	 * 订阅指定合约的行情数据，调用外部语言实现的订阅回调
	 * 
	 * @param setCodes 合约代码集合
	 */
	virtual void subscribe(const CodeSet& setCodes) override;

	/**
	 * @brief 取消订阅合约
	 * 
	 * 取消订阅指定合约的行情数据，调用外部语言实现的取消订阅回调
	 * 
	 * @param setCodes 合约代码集合
	 */
	virtual void unsubscribe(const CodeSet& setCodes) override;

	/**
	 * @brief 注册事件监听器
	 * 
	 * 注册Parser事件监听器（SPI），用于接收行情数据
	 * 
	 * @param listener 事件监听器指针
	 */
	virtual void registerSpi(IParserSpi* listener) override;

private:
	std::string			_id;              // Parser的唯一标识符
	IParserSpi*			m_sink;           // 事件监听器（用于分发行情数据）
	IBaseDataMgr*		m_pBaseDataMgr;   // 基础数据管理器指针
};

