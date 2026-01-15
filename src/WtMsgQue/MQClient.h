/*!
 * \file MQClient.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 消息队列客户端类定义，实现消息订阅功能
 * 
 * 本文件定义了MQClient类，实现基于nanomsg的发布-订阅模式中的订阅端（SUB）。
 * 
 * 设计说明：
 * - 使用nanomsg库的NN_SUB套接字类型实现消息订阅
 * - 使用后台线程持续接收消息，避免阻塞
 * - 使用接收缓冲区缓存不完整的数据包
 * - 支持主题过滤，只接收订阅的主题
 * - 支持超时检测，检测连接是否断开
 * 
 * 工作流程：
 * 1. init - 初始化客户端，连接到指定URL
 * 2. sub_topic - 订阅消息主题
 * 3. start - 启动后台接收线程
 * 4. 后台线程持续接收消息并解析
 * 5. 通过回调函数通知外部接收到的消息
 */
#pragma once
#include "PorterDefs.h"              // 包含回调函数类型定义
#include <queue>                     // 标准队列容器

#include "../Includes/WTSMarcos.h"   // WonderTrader宏定义
#include "../Includes/FasterDefs.h"  // 快速哈希集合等定义
#include "../Share/StdUtils.hpp"     // 标准工具函数，提供线程等

NS_WTP_BEGIN                        // WonderTrader命名空间开始
class MQManager;                     // 前向声明：消息队列管理器类

/**
 * @class MQClient
 * @brief 消息队列客户端类，实现消息订阅功能
 * 
 * 该类负责：
 * - 创建和管理nanomsg SUB套接字
 * - 连接到指定的服务器URL地址
 * - 订阅消息主题
 * - 后台线程持续接收消息
 * - 解析数据包并通过回调函数通知外部
 * - 检测连接超时
 * 
 * 设计特点：
 * - 使用接收缓冲区处理不完整的数据包
 * - 使用主题集合过滤消息
 * - 支持超时检测机制
 */
class MQClient
{
public:
	/**
	 * @brief 构造函数
	 * @param mgr 消息队列管理器指针
	 * 
	 * 初始化客户端，设置管理器指针，生成唯一ID。
	 */
	MQClient(MQManager* mgr);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源：
	 * - 停止后台接收线程
	 * - 关闭nanomsg套接字
	 */
	~MQClient();

private:
	/**
	 * @brief 从接收缓冲区中提取完整的数据包
	 * 
	 * 从接收缓冲区中解析MQPacket数据包：
	 * - 检查是否有足够的数据（至少一个MQPacket头部）
	 * - 检查是否有完整的数据包（头部+数据）
	 * - 提取数据包并调用回调函数
	 * - 移除已处理的数据
	 */
	void	extract_buffer();

	/**
	 * @brief 检查主题是否被允许接收
	 * @param topic 消息主题（字符串）
	 * @return 返回是否允许接收（布尔值）
	 * 
	 * 检查逻辑：
	 * - 如果未订阅任何主题，允许接收所有消息
	 * - 如果订阅了主题，只允许接收已订阅的主题
	 */
	inline bool	is_allowed(const char* topic)
	{
		if (_topics.empty())         // 如果未订阅任何主题
			return true;              // 允许接收所有消息

		auto it = _topics.find(topic);  // 在主题集合中查找
		if (it != _topics.end())     // 如果找到
			return true;              // 允许接收

		return false;                 // 否则不允许接收
	}

public:
	/**
	 * @brief 获取客户端ID
	 * @return 返回客户端ID（32位无符号整数）
	 */
	inline uint32_t id() const { return _id; }

	/**
	 * @brief 初始化客户端
	 * @param url 服务器地址（字符串），格式如"tcp://127.0.0.1:5555"、"ipc:///tmp/mq.ipc"等
	 * @param cb 消息回调函数指针，当接收到消息时调用
	 * @return 返回初始化是否成功（布尔值）
	 * 
	 * 初始化流程：
	 * 1. 创建nanomsg SUB套接字
	 * 2. 订阅所有主题（空字符串表示订阅所有）
	 * 3. 设置接收缓冲区大小
	 * 4. 连接到指定URL地址
	 * 5. 标记为就绪状态
	 */
	bool	init(const char* url, FuncMQCallback cb);

	/**
	 * @brief 启动客户端接收消息
	 * 
	 * 启动后台接收线程，开始持续接收消息。
	 * 如果线程已启动，则不做任何操作。
	 */
	void	start();

	/**
	 * @brief 订阅消息主题
	 * @param topic 消息主题（字符串）
	 * 
	 * 将主题添加到订阅集合中。
	 * 客户端只会接收已订阅主题的消息。
	 */
	inline void	sub_topic(const char* topic)
	{
		_topics.insert(topic);       // 将主题添加到订阅集合
	}

private:
	std::string		m_strURL;        // 服务器URL地址字符串
	bool			m_bReady;        // 就绪标志（布尔值），true表示客户端已初始化并连接成功
	int				_sock;           // nanomsg套接字描述符（整数），-1表示未初始化
	MQManager*		_mgr;            // 消息队列管理器指针，用于日志记录
	uint32_t		_id;             // 客户端ID（32位无符号整数），唯一标识该客户端

	StdThreadPtr	m_thrdRecv;      // 后台接收线程指针，用于异步接收消息
	bool			m_bTerminated;   // 终止标志（布尔值），true表示线程应该退出
	int64_t			m_iCheckTime;    // 检查时间戳（64位整数），上次接收到消息的时间（毫秒）
	bool			m_bNeedCheck;    // 需要检查标志（布尔值），true表示需要检查超时

	std::string		_buffer;         // 接收缓冲区字符串，用于缓存不完整的数据包
	FuncMQCallback	_cb_message;     // 消息回调函数指针，当接收到消息时调用

	wt_hashset<std::string> _topics;  // 主题集合，存储已订阅的主题
	char			_recv_buf[1024 * 1024];  // 接收缓冲区（1MB），用于从套接字接收数据
};

NS_WTP_END                          // WonderTrader命名空间结束