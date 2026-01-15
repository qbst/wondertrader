/*!
 * \file MQManager.cpp
 * \project	WonderTrader
 *
 * \brief MQManager类的实现文件，实现消息队列管理器的所有功能
 * 
 * 本文件实现了MQManager类的所有方法，包括：
 * - 创建和销毁消息发布服务器
 * - 发布消息
 * - 创建和销毁消息订阅客户端
 * - 订阅主题和启动客户端
 * - 日志记录功能
 */
#include "MQManager.h"               // 包含类定义头文件

#include <spdlog/fmt/fmt.h>           // 包含格式化字符串库

USING_NS_WTP;                        // 使用WonderTrader命名空间

/**
 * @brief 创建消息发布服务器的实现
 * @param url 服务器地址
 * @param confirm 是否需要确认连接
 * @return 返回服务器ID
 * 
 * 创建流程：
 * 1. 创建MQServer实例
 * 2. 初始化服务器，绑定到指定URL
 * 3. 获取服务器ID
 * 4. 将服务器添加到映射表
 * 5. 返回服务器ID
 */
WtUInt32 MQManager::create_server(const char* url, bool confirm)
{
	MQServerPtr server(new MQServer(this));  // 创建消息服务器实例，传入管理器指针

	printf("init server\r\n");       // 调试输出：初始化服务器
	server->init(url, confirm);      // 初始化服务器，绑定到指定URL

	auto id = server->id();          // 获取服务器ID

	_servers[id] = server;           // 将服务器添加到映射表，使用ID作为key
	return id;                       // 返回服务器ID
}

/**
 * @brief 销毁消息发布服务器的实现
 * @param id 服务器ID
 * 
 * 销毁流程：
 * 1. 在映射表中查找服务器
 * 2. 如果不存在，记录日志并返回
 * 3. 从映射表中移除服务器（智能指针会自动释放资源）
 * 4. 记录销毁日志
 */
void MQManager::destroy_server(WtUInt32 id)
{
	auto it = _servers.find(id);     // 在映射表中查找服务器
	if(it == _servers.end())         // 如果不存在
	{
		log_server(id, fmt::format("MQServer {} not exists", id).c_str());  // 记录错误日志
		return;
	}

	_servers.erase(it);              // 从映射表中移除服务器（智能指针会自动调用析构函数释放资源）
	log_server(id, fmt::format("MQServer {} has been destroyed", id).c_str());  // 记录销毁日志
}

/**
 * @brief 发布消息的实现
 * @param id 服务器ID
 * @param topic 消息主题
 * @param data 消息数据指针
 * @param dataLen 消息数据长度
 * 
 * 发布流程：
 * 1. 在映射表中查找服务器
 * 2. 如果不存在，记录日志并返回
 * 3. 调用服务器的publish方法发布消息
 */
void MQManager::publish_message(WtUInt32 id, const char* topic, const void* data, WtUInt32 dataLen)
{
	auto it = _servers.find(id);     // 在映射表中查找服务器
	if (it == _servers.end())        // 如果不存在
	{
		log_server(id, fmt::format("MQServer {} not exists", id).c_str());  // 记录错误日志
		return;
	}

	MQServerPtr& server = (MQServerPtr&)it->second;  // 获取服务器智能指针引用
	server->publish(topic, data, dataLen);  // 调用服务器的publish方法发布消息
}

/**
 * @brief 记录服务器日志的实现
 * @param id 服务器ID
 * @param message 日志消息
 * 
 * 如果已注册日志回调函数，则调用回调函数传递日志信息。
 */
void MQManager::log_server(WtUInt32 id, const char* message)
{
	if (_cb_log)                     // 如果日志回调函数已注册
		_cb_log(id, message, true);  // 调用回调函数，bServer参数为true表示服务器日志
}

/**
 * @brief 记录客户端日志的实现
 * @param id 客户端ID
 * @param message 日志消息
 * 
 * 如果已注册日志回调函数，则调用回调函数传递日志信息。
 */
void MQManager::log_client(WtUInt32 id, const char* message)
{
	if (_cb_log)                     // 如果日志回调函数已注册
		_cb_log(id, message, false);  // 调用回调函数，bServer参数为false表示客户端日志
}

/**
 * @brief 创建消息订阅客户端的实现
 * @param url 服务器地址
 * @param cb 消息回调函数指针
 * @return 返回客户端ID
 * 
 * 创建流程：
 * 1. 创建MQClient实例
 * 2. 初始化客户端，连接到指定URL
 * 3. 获取客户端ID
 * 4. 将客户端添加到映射表
 * 5. 返回客户端ID
 */
WtUInt32 MQManager::create_client(const char* url, FuncMQCallback cb)
{
	MQClientPtr client(new MQClient(this));  // 创建消息客户端实例，传入管理器指针
	client->init(url, cb);           // 初始化客户端，连接到指定URL，设置消息回调函数

	auto id = client->id();          // 获取客户端ID

	_clients[id] = client;           // 将客户端添加到映射表，使用ID作为key
	return id;                       // 返回客户端ID
}

/**
 * @brief 销毁消息订阅客户端的实现
 * @param id 客户端ID
 * 
 * 销毁流程：
 * 1. 在映射表中查找客户端
 * 2. 如果不存在，记录日志并返回
 * 3. 从映射表中移除客户端（智能指针会自动释放资源）
 * 4. 记录销毁日志
 */
void MQManager::destroy_client(WtUInt32 id)
{
	auto it = _clients.find(id);     // 在映射表中查找客户端
	if (it == _clients.end())        // 如果不存在
	{
		log_client(id, fmt::format("MQClient {} not exists", id).c_str());  // 记录错误日志
		return;
	}

	_clients.erase(it);              // 从映射表中移除客户端（智能指针会自动调用析构函数释放资源）
	log_client(id, fmt::format("MQClient {} has been destroyed", id).c_str());  // 记录销毁日志
}

/**
 * @brief 订阅消息主题的实现
 * @param id 客户端ID
 * @param topic 消息主题
 * 
 * 订阅流程：
 * 1. 在映射表中查找客户端
 * 2. 如果不存在，记录日志并返回
 * 3. 调用客户端的sub_topic方法订阅主题
 */
void MQManager::sub_topic(WtUInt32 id, const char* topic)
{
	auto it = _clients.find(id);     // 在映射表中查找客户端
	if (it == _clients.end())        // 如果不存在
	{
		log_client(id, fmt::format("MQClient {} not exists", id).c_str());  // 记录错误日志
		return;
	}

	MQClientPtr& client = (MQClientPtr&)it->second;  // 获取客户端智能指针引用
	client->sub_topic(topic);        // 调用客户端的sub_topic方法订阅主题
}

/**
 * @brief 启动客户端接收消息的实现
 * @param id 客户端ID
 * 
 * 启动流程：
 * 1. 在映射表中查找客户端
 * 2. 如果不存在，记录日志并返回
 * 3. 调用客户端的start方法启动接收线程
 */
void MQManager::start_client(WtUInt32 id)
{
	auto it = _clients.find(id);     // 在映射表中查找客户端
	if (it == _clients.end())        // 如果不存在
	{
		log_client(id, fmt::format("MQClient {} not exists", id).c_str());  // 记录错误日志
		return;
	}

	MQClientPtr& client = (MQClientPtr&)it->second;  // 获取客户端智能指针引用
	client->start();                 // 调用客户端的start方法启动接收线程
}