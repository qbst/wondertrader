/*!
 * \file WtMsgQue.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtMsgQue模块的C语言接口实现
 * 
 * 本文件实现了WtMsgQue.h中声明的所有C语言导出函数。
 * 这些函数作为外部接口，内部调用MQManager类的功能。
 * 
 * 设计说明：
 * - 使用单例模式，通过getMgr()获取全局唯一的MQManager实例
 * - 所有C接口函数都是对MQManager类方法的简单封装
 * - 在Windows平台下自动链接必要的库文件
 * 
 * 实现逻辑：
 * 1. getMgr() - 获取全局MQManager单例实例
 * 2. regiter_callbacks - 将日志回调函数注册到MQManager
 * 3. create_server/destroy_server - 创建和销毁服务器
 * 4. publish_message - 发布消息
 * 5. create_client/destroy_client - 创建和销毁客户端
 * 6. subscribe_topic/start_client - 订阅主题和启动客户端
 */
#include "WtMsgQue.h"                // 包含C接口声明
#include "MQManager.h"              // 包含MQManager类定义

#ifdef _MSC_VER                      // 如果是Microsoft Visual C++编译器（Windows平台）
#pragma comment(lib, "Ws2_32.lib")   // 自动链接Windows Socket库
#pragma comment(lib, "Mswsock.lib")  // 自动链接Microsoft Windows Socket扩展库
#pragma comment(lib, "nanomsg.lib")  // 自动链接nanomsg消息队列库
#endif

USING_NS_WTP;                        // 使用WonderTrader命名空间

/**
 * @brief 获取全局MQManager单例实例
 * @return 返回MQManager类的引用
 * 
 * 使用静态局部变量实现单例模式，确保全局只有一个MQManager实例。
 * 静态局部变量在首次调用时初始化，后续调用直接返回已存在的实例。
 * 线程安全（C++11标准保证静态局部变量初始化的线程安全性）。
 */
MQManager& getMgr()
{
	static MQManager runner;         // 静态局部变量：全局唯一的MQManager实例，首次调用时初始化
	return runner;                    // 返回MQManager实例的引用
}

/**
 * @brief 注册日志回调函数的C接口实现
 * @param cbLog 日志回调函数指针
 * 
 * 将外部传入的日志回调函数注册到MQManager实例中。
 * 当服务器或客户端产生日志时，MQManager会调用此回调函数通知外部。
 */
void regiter_callbacks(FuncLogCallback cbLog)
{
	getMgr().regiter_callbacks(cbLog);  // 调用MQManager的注册回调方法
}

/**
 * @brief 创建消息发布服务器的C接口实现
 * @param url 服务器地址
 * @param confirm 是否需要确认连接
 * @return 返回服务器ID
 * 
 * 创建消息发布服务器，绑定到指定的URL地址。
 */
WtUInt32 create_server(const char* url, bool confirm)
{
	printf("create server\r\n");      // 调试输出：创建服务器
	return getMgr().create_server(url, confirm);  // 调用MQManager的创建服务器方法
}

/**
 * @brief 销毁消息发布服务器的C接口实现
 * @param id 服务器ID
 * 
 * 销毁指定的消息发布服务器，释放相关资源。
 */
void destroy_server(WtUInt32 id)
{
	getMgr().destroy_server(id);     // 调用MQManager的销毁服务器方法
}

/**
 * @brief 发布消息的C接口实现
 * @param id 服务器ID
 * @param topic 消息主题
 * @param data 消息数据
 * @param dataLen 消息数据长度
 * 
 * 通过指定的服务器发布一条消息。
 */
void publish_message(WtUInt32 id, const char* topic, const char* data, WtUInt32 dataLen)
{
	getMgr().publish_message(id, topic, data, dataLen);  // 调用MQManager的发布消息方法
}

/**
 * @brief 创建消息订阅客户端的C接口实现
 * @param url 服务器地址
 * @param cb 消息回调函数指针
 * @return 返回客户端ID
 * 
 * 创建消息订阅客户端，连接到指定的服务器地址。
 */
WtUInt32 create_client(const char* url, FuncMQCallback cb)
{
	return getMgr().create_client(url, cb);  // 调用MQManager的创建客户端方法
}

/**
 * @brief 销毁消息订阅客户端的C接口实现
 * @param id 客户端ID
 * 
 * 销毁指定的消息订阅客户端，释放相关资源。
 */
void destroy_client(WtUInt32 id)
{
	getMgr().destroy_client(id);     // 调用MQManager的销毁客户端方法
}

/**
 * @brief 订阅消息主题的C接口实现
 * @param id 客户端ID
 * @param topic 消息主题
 * 
 * 订阅指定的消息主题。
 */
void subscribe_topic(WtUInt32 id, const char* topic)
{
	return getMgr().sub_topic(id, topic);  // 调用MQManager的订阅主题方法
}

/**
 * @brief 启动客户端接收消息的C接口实现
 * @param id 客户端ID
 * 
 * 启动客户端开始接收消息。
 */
void start_client(WtUInt32 id)
{
	getMgr().start_client(id);       // 调用MQManager的启动客户端方法
}
