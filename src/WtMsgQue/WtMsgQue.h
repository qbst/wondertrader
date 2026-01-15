/*!
 * \file WtMsgQue.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 消息队列模块的C语言导出接口
 * 
 * 本文件定义了WtMsgQue模块对外提供的C语言接口函数。
 * 这些函数可以被外部程序（如Python、C#等）通过DLL/so动态库方式调用。
 * 
 * 设计说明：
 * - 使用extern "C"确保C++代码可以被C语言调用
 * - 所有函数都使用EXPORT_FLAG导出标志，确保跨平台兼容性
 * - 基于nanomsg库实现发布-订阅（PUB-SUB）模式的消息队列
 * - 提供服务器（发布者）和客户端（订阅者）的完整生命周期管理
 * 
 * 使用流程：
 * 服务器端：
 * 1. regiter_callbacks - 注册日志回调函数（可选）
 * 2. create_server - 创建消息发布服务器
 * 3. publish_message - 发布消息
 * 4. destroy_server - 销毁服务器
 * 
 * 客户端端：
 * 1. regiter_callbacks - 注册日志回调函数（可选）
 * 2. create_client - 创建消息订阅客户端
 * 3. subscribe_topic - 订阅消息主题
 * 4. start_client - 启动客户端接收消息
 * 5. destroy_client - 销毁客户端
 */
#pragma once
#include "PorterDefs.h"  // 包含回调函数类型定义

#ifdef __cplusplus          // 如果是C++编译环境
extern "C"                  // 使用C语言链接约定，确保函数名不被C++名称修饰（name mangling）影响
{
#endif
	/**
	 * @brief 注册日志回调函数
	 * @param cbLog 日志回调函数指针，当服务器或客户端产生日志时调用
	 * 
	 * 注册日志回调函数，用于接收服务器和客户端的日志信息。
	 * 回调函数可以为NULL，表示不需要接收日志。
	 * 建议在创建服务器或客户端之前调用。
	 */
	EXPORT_FLAG void		regiter_callbacks(FuncLogCallback cbLog);

	/**
	 * @brief 创建消息发布服务器
	 * @param url 服务器地址（字符串），格式如"tcp://127.0.0.1:5555"、"ipc:///tmp/mq.ipc"等
	 * @param confirm 是否需要确认连接（布尔值），true表示需要等待至少一个客户端连接后才发送消息
	 * @return 返回服务器ID（32位无符号整数），用于后续操作该服务器
	 * 
	 * 创建一个消息发布服务器，绑定到指定的URL地址。
	 * 服务器可以发布消息到所有订阅的客户端。
	 * 如果confirm为true，服务器会等待至少一个客户端连接后才开始发送消息。
	 */
	EXPORT_FLAG WtUInt32	create_server(const char* url, bool confirm);

	/**
	 * @brief 销毁消息发布服务器
	 * @param id 服务器ID（32位无符号整数），由create_server返回
	 * 
	 * 销毁指定的消息发布服务器，释放相关资源。
	 * 销毁后不能再使用该服务器ID进行任何操作。
	 */
	EXPORT_FLAG void		destroy_server(WtUInt32 id);

	/**
	 * @brief 发布消息
	 * @param id 服务器ID（32位无符号整数），由create_server返回
	 * @param topic 消息主题（字符串），标识消息的类型或分类，最大32字符
	 * @param data 消息数据（字节数组指针），要发布的消息内容
	 * @param dataLen 消息数据长度（32位无符号整数），data数组的字节数
	 * 
	 * 通过指定的服务器发布一条消息。
	 * 消息会被发送到所有订阅了该主题（或订阅了所有主题）的客户端。
	 * 消息发布是异步的，函数会立即返回。
	 */
	EXPORT_FLAG void		publish_message(WtUInt32 id, const char* topic, const char* data, WtUInt32 dataLen);

	/**
	 * @brief 创建消息订阅客户端
	 * @param url 服务器地址（字符串），格式如"tcp://127.0.0.1:5555"、"ipc:///tmp/mq.ipc"等，必须与服务器地址一致
	 * @param cb 消息回调函数指针，当接收到消息时调用
	 * @return 返回客户端ID（32位无符号整数），用于后续操作该客户端
	 * 
	 * 创建一个消息订阅客户端，连接到指定的服务器地址。
	 * 客户端通过回调函数接收订阅的消息。
	 * 创建后需要调用subscribe_topic订阅主题，然后调用start_client启动接收。
	 */
	EXPORT_FLAG WtUInt32	create_client(const char* url, FuncMQCallback cb);

	/**
	 * @brief 销毁消息订阅客户端
	 * @param id 客户端ID（32位无符号整数），由create_client返回
	 * 
	 * 销毁指定的消息订阅客户端，释放相关资源。
	 * 销毁后不能再使用该客户端ID进行任何操作。
	 */
	EXPORT_FLAG void		destroy_client(WtUInt32 id);

	/**
	 * @brief 订阅消息主题
	 * @param id 客户端ID（32位无符号整数），由create_client返回
	 * @param topic 消息主题（字符串），要订阅的主题名称
	 * 
	 * 订阅指定的消息主题。
	 * 客户端只会接收到已订阅主题的消息。
	 * 可以多次调用此函数订阅多个主题。
	 * 如果不订阅任何主题，客户端会接收所有消息。
	 */
	EXPORT_FLAG void		subscribe_topic(WtUInt32 id, const char* topic);

	/**
	 * @brief 启动客户端接收消息
	 * @param id 客户端ID（32位无符号整数），由create_client返回
	 * 
	 * 启动客户端开始接收消息。
	 * 启动后会创建一个后台线程持续接收消息，并通过回调函数通知外部。
	 * 必须在subscribe_topic之后调用。
	 */
	EXPORT_FLAG void		start_client(WtUInt32 id);
#ifdef __cplusplus          // C++编译环境结束
}                           // extern "C"作用域结束
#endif