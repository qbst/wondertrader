/*!
 * \file MQServer.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 消息队列服务器类定义，实现消息发布功能
 * 
 * 本文件定义了MQServer类，实现基于nanomsg的发布-订阅模式中的发布端（PUB）。
 * 
 * 设计说明：
 * - 使用nanomsg库的NN_PUB套接字类型实现消息发布
 * - 使用后台线程异步发送消息，避免阻塞
 * - 使用消息队列缓存待发送的消息
 * - 支持确认模式，可以等待客户端连接后再发送
 * - 支持心跳包机制，定期发送心跳保持连接
 * 
 * 工作流程：
 * 1. init - 初始化服务器，绑定到指定URL
 * 2. publish - 将消息加入发送队列
 * 3. 后台线程从队列取出消息并发送
 * 4. 如果启用确认模式，会检查是否有客户端连接
 * 5. 定期发送心跳包保持连接活跃
 */
#pragma once

#include <queue>                     // 标准队列容器，用于消息队列

#include "../Includes/WTSMarcos.h"   // WonderTrader宏定义
#include "../Share/StdUtils.hpp"     // 标准工具函数，提供线程、互斥锁等

NS_WTP_BEGIN                        // WonderTrader命名空间开始
class MQManager;                     // 前向声明：消息队列管理器类

/**
 * @class MQServer
 * @brief 消息队列服务器类，实现消息发布功能
 * 
 * 该类负责：
 * - 创建和管理nanomsg PUB套接字
 * - 绑定到指定的URL地址
 * - 接收消息发布请求，将消息加入发送队列
 * - 后台线程异步发送消息到所有订阅的客户端
 * - 支持确认模式和心跳包机制
 * 
 * 设计特点：
 * - 使用消息队列解耦发布请求和实际发送
 * - 使用条件变量实现线程间通信
 * - 支持批量发送，提高效率
 */
class MQServer
{
public:
	/**
	 * @brief 构造函数
	 * @param mgr 消息队列管理器指针
	 * 
	 * 初始化服务器，设置管理器指针，生成唯一ID。
	 */
	MQServer(MQManager* mgr);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源：
	 * - 停止后台发送线程
	 * - 关闭nanomsg套接字
	 */
	~MQServer();

public:
	/**
	 * @brief 获取服务器ID
	 * @return 返回服务器ID（32位无符号整数）
	 */
	inline uint32_t id() const { return _id; }

	/**
	 * @brief 初始化服务器
	 * @param url 服务器地址（字符串），格式如"tcp://127.0.0.1:5555"、"ipc:///tmp/mq.ipc"等
	 * @param confirm 是否需要确认连接（布尔值），默认false，true表示需要等待至少一个客户端连接后才发送消息
	 * @return 返回初始化是否成功（布尔值）
	 * 
	 * 初始化流程：
	 * 1. 创建nanomsg PUB套接字
	 * 2. 设置发送缓冲区大小
	 * 3. 绑定到指定URL地址
	 * 4. 标记为就绪状态
	 */
	bool	init(const char* url, bool confirm = false);

	/**
	 * @brief 发布消息
	 * @param topic 消息主题（字符串），标识消息的类型或分类，最大32字符
	 * @param data 消息数据指针（void*），要发布的消息内容
	 * @param dataLen 消息数据长度（32位无符号整数），data的字节数
	 * 
	 * 将消息加入发送队列，由后台线程异步发送。
	 * 如果后台线程未启动，会启动后台线程。
	 * 发布是异步的，函数会立即返回。
	 */
	void	publish(const char* topic, const void* data, uint32_t dataLen);

private:
	std::string		_url;            // 服务器URL地址字符串
	bool			_ready;          // 就绪标志（布尔值），true表示服务器已初始化并绑定成功
	int				_sock;           // nanomsg套接字描述符（整数），-1表示未初始化
	MQManager*		_mgr;            // 消息队列管理器指针，用于日志记录
	uint32_t		_id;             // 服务器ID（32位无符号整数），唯一标识该服务器
	bool			_confirm;        // 确认标志（布尔值），true表示需要等待客户端连接后才发送消息

	StdThreadPtr	m_thrdCast;      // 后台发送线程指针，用于异步发送消息
	StdCondVariable	m_condCast;      // 条件变量，用于线程间通信，通知有新消息或超时
	StdUniqueMutex	m_mtxCast;       // 互斥锁，保护消息队列的并发访问
	bool			m_bTerminated;   // 终止标志（布尔值），true表示线程应该退出
	bool			m_bTimeout;      // 超时标志（布尔值），用于心跳包机制

	/**
	 * @struct PubData
	 * @brief 发布数据结构
	 * 
	 * 用于在消息队列中存储待发送的消息。
	 * 包含消息主题和数据内容。
	 */
	typedef struct _PubData
	{
		std::string	_topic;          // 消息主题字符串
		std::string	_data;           // 消息数据字符串

		/**
		 * @brief 构造函数
		 * @param topic 消息主题
		 * @param data 消息数据指针
		 * @param dataLen 消息数据长度
		 * 
		 * 创建发布数据对象，复制主题和数据内容。
		 */
		_PubData(const char* topic, const void* data, uint32_t dataLen)
			: _topic(topic)         // 初始化主题字符串
		{
			if(data !=  NULL && dataLen != 0)  // 如果数据不为空
			{
				_data.append((const char*)data, dataLen);  // 将数据追加到_data字符串中
			}
		}
	} PubData;
	typedef std::queue<PubData> PubDataQue;  // 发布数据队列类型定义

	PubDataQue		m_dataQue;       // 消息队列，存储待发送的消息
	std::string		m_sendBuf;       // 发送缓冲区字符串，用于组装MQPacket数据包
};

NS_WTP_END                          // WonderTrader命名空间结束