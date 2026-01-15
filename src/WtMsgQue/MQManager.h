/*!
 * \file MQManager.h
 * \project	WonderTrader
 *
 * \brief 消息队列管理器类定义
 * 
 * 本文件定义了MQManager类，这是WtMsgQue模块的核心管理类。
 * 负责管理多个消息发布服务器和消息订阅客户端的生命周期。
 * 
 * 设计说明：
 * - 使用哈希映射表管理服务器和客户端实例
 * - 每个服务器和客户端都有唯一的ID标识
 * - 提供统一的创建、销毁、操作接口
 * - 支持日志回调，记录服务器和客户端的运行状态
 * 
 * 数据包格式：
 * MQPacket结构定义了消息队列的数据包格式，包含主题、数据长度和数据内容。
 * 使用紧凑的内存布局（#pragma pack），提高传输效率。
 */
#pragma once
#include "PorterDefs.h"              // 包含回调函数类型定义
#include "MQServer.h"                 // 包含消息服务器类定义
#include "MQClient.h"                // 包含消息客户端类定义

#include "../Includes/FasterDefs.h"  // 包含快速哈希映射等定义
#include "../Share/StdUtils.hpp"     // 包含标准工具函数

NS_WTP_BEGIN                        // WonderTrader命名空间开始

#pragma warning(disable:4200)        // 禁用警告4200（零长度数组警告）

#pragma pack(push,1)                 // 开始紧凑内存布局（1字节对齐）
/**
 * @struct MQPacket
 * @brief 消息队列数据包结构
 * 
 * 定义了消息队列中传输的数据包格式。
 * 使用紧凑的内存布局，减少内存占用和提高传输效率。
 * 
 * 数据包结构：
 * - _topic: 消息主题（32字节固定长度）
 * - _length: 数据长度（4字节）
 * - _data: 数据内容（可变长度，通过零长度数组实现）
 */
typedef struct _MQPacket
{
	char			_topic[32];      // 消息主题（32字节固定长度），标识消息的类型或分类
	uint32_t		_length;         // 数据长度（32位无符号整数），_data字段的字节数
	char			_data[0];        // 数据内容（零长度数组），实际数据紧跟在结构体后面
} MQPacket;
#pragma pack(pop)                    // 恢复默认内存布局

typedef std::shared_ptr<MQServer> MQServerPtr;  // 消息服务器智能指针类型定义
typedef std::shared_ptr<MQClient> MQClientPtr;  // 消息客户端智能指针类型定义

/**
 * @class MQManager
 * @brief 消息队列管理器类
 * 
 * 该类是WtMsgQue模块的核心管理器，负责：
 * - 管理多个消息发布服务器（MQServer）实例
 * - 管理多个消息订阅客户端（MQClient）实例
 * - 提供统一的创建、销毁、操作接口
 * - 处理日志回调，记录运行状态
 * 
 * 设计特点：
 * - 使用哈希映射表快速查找服务器和客户端
 * - 每个实例都有唯一的ID标识
 * - 支持日志回调，方便外部监控和调试
 */
class MQManager
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化日志回调函数指针为NULL。
	 */
	MQManager() : _cb_log(NULL) {}

public:
	/**
	 * @brief 注册日志回调函数
	 * @param cbLog 日志回调函数指针
	 * 
	 * 注册日志回调函数，用于接收服务器和客户端的日志信息。
	 */
	inline void		regiter_callbacks(FuncLogCallback cbLog) { _cb_log = cbLog; }

	/**
	 * @brief 创建消息发布服务器
	 * @param url 服务器地址
	 * @param confirm 是否需要确认连接
	 * @return 返回服务器ID
	 * 
	 * 创建一个消息发布服务器实例，绑定到指定的URL地址。
	 */
	WtUInt32	create_server(const char* url, bool confirm);
	
	/**
	 * @brief 销毁消息发布服务器
	 * @param id 服务器ID
	 * 
	 * 销毁指定的消息发布服务器，从映射表中移除。
	 */
	void		destroy_server(WtUInt32 id);
	
	/**
	 * @brief 发布消息
	 * @param id 服务器ID
	 * @param topic 消息主题
	 * @param data 消息数据指针
	 * @param dataLen 消息数据长度
	 * 
	 * 通过指定的服务器发布一条消息。
	 */
	void		publish_message(WtUInt32 id, const char* topic, const void* data, WtUInt32 dataLen);

	/**
	 * @brief 创建消息订阅客户端
	 * @param url 服务器地址
	 * @param cb 消息回调函数指针
	 * @return 返回客户端ID
	 * 
	 * 创建一个消息订阅客户端实例，连接到指定的服务器地址。
	 */
	WtUInt32	create_client(const char* url, FuncMQCallback cb);
	
	/**
	 * @brief 销毁消息订阅客户端
	 * @param id 客户端ID
	 * 
	 * 销毁指定的消息订阅客户端，从映射表中移除。
	 */
	void		destroy_client(WtUInt32 id);
	
	/**
	 * @brief 订阅消息主题
	 * @param id 客户端ID
	 * @param topic 消息主题
	 * 
	 * 为指定的客户端订阅消息主题。
	 */
	void		sub_topic(WtUInt32 id, const char* topic);
	
	/**
	 * @brief 启动客户端接收消息
	 * @param id 客户端ID
	 * 
	 * 启动指定的客户端开始接收消息。
	 */
	void		start_client(WtUInt32 id);

	/**
	 * @brief 记录服务器日志
	 * @param id 服务器ID
	 * @param message 日志消息
	 * 
	 * 记录服务器的日志信息，如果已注册日志回调，则调用回调函数。
	 */
	void		log_server(WtUInt32 id, const char* message);
	
	/**
	 * @brief 记录客户端日志
	 * @param id 客户端ID
	 * @param message 日志消息
	 * 
	 * 记录客户端的日志信息，如果已注册日志回调，则调用回调函数。
	 */
	void		log_client(WtUInt32 id, const char* message);

private:
	typedef wt_hashmap<uint32_t, MQServerPtr> ServerMap;  // 服务器映射表类型定义，key为服务器ID，value为服务器智能指针
	ServerMap	_servers;              // 服务器映射表，存储所有服务器实例

	typedef wt_hashmap<uint32_t, MQClientPtr> ClientMap;  // 客户端映射表类型定义，key为客户端ID，value为客户端智能指针
	ClientMap	_clients;              // 客户端映射表，存储所有客户端实例

	FuncLogCallback	_cb_log;         // 日志回调函数指针，用于接收服务器和客户端的日志信息
};

NS_WTP_END                          // WonderTrader命名空间结束
