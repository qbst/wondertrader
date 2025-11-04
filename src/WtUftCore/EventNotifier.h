/*!
 * \file EventCaster.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 事件通知器头文件
 *
 * 本文件定义了EventNotifier类，用于通过消息队列（MQ）广播交易事件。
 *
 * 设计逻辑：
 * 1. 消息队列集成：通过动态加载WtMsgQue模块实现消息队列功能
 * 2. 异步事件处理：使用Boost.Asio实现异步事件处理，避免阻塞主线程
 * 3. JSON格式转换：将交易事件（成交、订单）转换为JSON格式进行广播
 * 4. 多事件类型支持：支持成交事件、订单事件、日志事件、通用事件等多种事件类型
 * 5. 通道管理：支持通过UDP等方式广播消息，实现跨进程事件通知
 *
 * 主要功能：
 * - 初始化消息队列服务器
 * - 将交易事件转换为JSON格式
 * - 异步广播交易事件（成交、订单、日志等）
 * - 管理消息队列生命周期
 */
#pragma once

#include <boost/asio/io_service.hpp>  // Boost异步IO服务

#include "../Includes/WTSMarcos.h"  // WonderTrader宏定义
#include "../Includes/WTSObject.hpp"  // WonderTrader对象基类
#include "../Share/StdUtils.hpp"  // 标准工具函数

/**
 * @typedef FuncCreateMQServer
 * @brief 创建消息队列服务器函数指针类型
 * @param channel 消息通道地址
 * @return 消息队列服务器ID
 */
typedef unsigned long(*FuncCreateMQServer)(const char*);

/**
 * @typedef FuncDestroyMQServer
 * @brief 销毁消息队列服务器函数指针类型
 * @param id 消息队列服务器ID
 */
typedef void(*FuncDestroyMQServer)(unsigned long);

/**
 * @typedef FundPublishMessage
 * @brief 发布消息函数指针类型
 * @param id 消息队列服务器ID
 * @param topic 消息主题
 * @param data 消息数据
 * @param len 消息数据长度
 */
typedef void(*FundPublishMessage)(unsigned long, const char*, const char*, unsigned long);

/**
 * @typedef FuncLogCallback
 * @brief 日志回调函数指针类型
 * @param id 消息队列服务器ID
 * @param message 日志消息
 * @param bServer 是否为服务器日志
 */
typedef void(*FuncLogCallback)(unsigned long, const char*, bool);

/**
 * @typedef FuncRegCallbacks
 * @brief 注册回调函数指针类型
 * @param logCallback 日志回调函数指针
 */
typedef void(*FuncRegCallbacks)(FuncLogCallback);


NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSTradeInfo;  // 前向声明：成交信息类
class WTSOrderInfo;  // 前向声明：订单信息类
class WTSVariant;  // 前向声明：变体配置类

/**
 * @class EventNotifier
 * @brief 事件通知器类
 * 
 * 通过消息队列（MQ）异步广播交易事件，包括成交、订单、日志等事件。
 * 使用Boost.Asio实现异步处理，避免阻塞主线程。
 * 
 * 核心功能：
 * - 初始化消息队列服务器
 * - 将交易事件转换为JSON格式
 * - 异步广播各种事件类型
 */
class EventNotifier
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建事件通知器实例，初始化成员变量。
	 */
	EventNotifier();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理事件通知器占用的资源，停止工作线程，释放消息队列服务器。
	 */
	~EventNotifier();

private:
	/**
	 * @brief 将成交信息转换为JSON格式
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param trdInfo 成交信息指针
	 * @param output 输出的JSON字符串
	 * 
	 * 将成交信息转换为JSON格式字符串，用于消息广播。
	 */
	void	tradeToJson(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo, std::string& output);
	
	/**
	 * @brief 将订单信息转换为JSON格式
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param ordInfo 订单信息指针
	 * @param output 输出的JSON字符串
	 * 
	 * 将订单信息转换为JSON格式字符串，用于消息广播。
	 */
	void	orderToJson(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo, std::string& output);

public:
	/**
	 * @brief 初始化事件通知器
	 * @param cfg 配置参数，包含消息队列URL和模块路径
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置中加载消息队列模块，创建消息队列服务器，启动异步处理线程。
	 */
	bool	init(WTSVariant* cfg);

	/**
	 * @brief 通知成交事件
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param trdInfo 成交信息指针
	 * 
	 * 异步广播成交事件，将成交信息转换为JSON格式后发布到"TRD_TRADE"主题。
	 */
	void	notify(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo);
	
	/**
	 * @brief 通知订单事件
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准化合约代码
	 * @param ordInfo 订单信息指针
	 * 
	 * 异步广播订单事件，将订单信息转换为JSON格式后发布到"TRD_ORDER"主题。
	 */
	void	notify(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo);
	
	/**
	 * @brief 通知交易消息
	 * @param trader 交易接口名称
	 * @param message 消息内容
	 * 
	 * 异步广播交易消息，将消息转换为JSON格式后发布到"TRD_NOTIFY"主题。
	 */
	void	notify(const char* trader, const char* message);

	/**
	 * @brief 通知日志事件
	 * @param tag 日志标签
	 * @param message 日志消息
	 * 
	 * 异步广播日志事件，将日志信息转换为JSON格式后发布到"LOG"主题。
	 */
	void	notify_log(const char* tag, const char* message);

	/**
	 * @brief 通知通用事件
	 * @param message 事件消息
	 * 
	 * 异步广播通用事件，将事件信息转换为JSON格式后发布到"GRP_EVENT"主题。
	 */
	void	notify_event(const char* message);

private:
	std::string		_url;  // 消息队列URL地址
	uint32_t		_mq_sid;  // 消息队列服务器ID
	FuncCreateMQServer	_creator;  // 创建消息队列服务器函数指针
	FuncDestroyMQServer	_remover;  // 销毁消息队列服务器函数指针
	FundPublishMessage	_publisher;  // 发布消息函数指针
	FuncRegCallbacks	_register;  // 注册回调函数指针

	bool			_stopped;  // 是否已停止标志
	boost::asio::io_service		_asyncio;  // Boost异步IO服务
	StdThreadPtr				_worker;  // 异步处理工作线程指针
};

NS_WTP_END  // WonderTrader命名空间结束