/*!
 * \file MQClient.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief MQClient类的实现文件，实现消息订阅客户端的所有功能
 * 
 * 本文件实现了MQClient类的所有方法，包括：
 * - 客户端的创建和初始化
 * - nanomsg套接字的创建和连接
 * - 消息的接收和解析
 * - 后台接收线程的管理
 * - 超时检测机制
 * 
 * 技术实现：
 * - 使用nanomsg库的NN_SUB套接字类型
 * - 使用后台线程持续接收消息，避免阻塞
 * - 使用接收缓冲区处理不完整的数据包
 * - 支持主题过滤和超时检测
 */
#include "MQClient.h"                // 包含类定义头文件
#include "MQManager.h"               // 包含管理器类定义

#include "../Share/fmtlib.h"         // 格式化字符串库
#include "../Share/TimeUtils.hpp"    // 时间工具函数
#include <atomic>                    // 原子操作支持

#ifndef NN_STATIC_LIB                // 如果未定义静态库宏
#define NN_STATIC_LIB                // 定义为静态库模式
#endif
#include <nanomsg/nn.h>              // nanomsg核心库
#include <nanomsg/pubsub.h>          // nanomsg发布-订阅模式定义


USING_NS_WTP;                        // 使用WonderTrader命名空间

#pragma warning(disable:4200)        // 禁用警告4200（零长度数组警告）

#define  RECV_BUF_SIZE  1024*1024    // 接收缓冲区大小定义：1MB

/**
 * @brief 生成唯一的客户端ID
 * @return 返回客户端ID（32位无符号整数）
 * 
 * 使用原子计数器生成唯一的客户端ID。
 * 客户端ID从5001开始，每次调用自动递增。
 * 线程安全。
 */
inline uint32_t makeMQCientId()
{
	static std::atomic<uint32_t> _auto_client_id{ 5001 };  // 静态原子计数器，初始值为5001
	return _auto_client_id.fetch_add(1);                   // 原子操作：获取当前值并加1，返回旧值
}


/**
 * @brief MQClient构造函数实现
 * @param mgr 消息队列管理器指针
 * 
 * 使用初始化列表初始化所有成员变量：
 * - _sock初始化为-1（未初始化状态）
 * - m_bReady初始化为false（未就绪）
 * - _mgr设置为传入的管理器指针
 * - m_bTerminated初始化为false（未终止）
 * - _cb_message初始化为NULL（未设置回调）
 * - m_iCheckTime初始化为0（未开始检查）
 * - m_bNeedCheck初始化为false（不需要检查）
 * 然后调用makeMQCientId()生成唯一ID。
 */
MQClient::MQClient(MQManager* mgr)
	: _sock(-1)                      // 初始化套接字为-1（未初始化）
	, m_bReady(false)                // 初始化就绪标志为false
	, _mgr(mgr)                      // 初始化管理器指针为传入的值
	, m_bTerminated(false)           // 初始化终止标志为false
	, _cb_message(NULL)              // 初始化消息回调函数指针为NULL
	, m_iCheckTime(0)                // 初始化检查时间戳为0
	, m_bNeedCheck(false)            // 初始化需要检查标志为false
{
	_id = makeMQCientId();           // 生成唯一的客户端ID
}

/**
 * @brief MQClient析构函数实现
 * 
 * 清理资源：
 * - 如果客户端未就绪，直接返回
 * - 设置终止标志，通知后台线程退出
 * - 等待后台线程结束
 * - 关闭nanomsg套接字
 */
MQClient::~MQClient()
{
	if (!m_bReady)                   // 如果客户端未就绪，直接返回
		return;

	m_bTerminated = true;            // 设置终止标志，通知后台线程退出
	if (m_thrdRecv)                  // 如果后台线程存在
		m_thrdRecv->join();           // 等待后台线程结束

	if (_sock != 0)                  // 如果套接字有效（不为0）
		nn_close(_sock);             // 关闭nanomsg套接字
}

/**
 * @brief 初始化客户端的实现
 * @param url 服务器地址
 * @param cb 消息回调函数指针
 * @return 返回初始化是否成功
 * 
 * 初始化流程：
 * 1. 检查套接字是否已初始化
 * 2. 保存消息回调函数指针
 * 3. 创建nanomsg SUB套接字
 * 4. 订阅所有主题（空字符串表示订阅所有）
 * 5. 设置接收缓冲区大小（1MB）
 * 6. 连接到指定URL地址
 * 7. 标记为就绪状态
 */
bool MQClient::init(const char* url, FuncMQCallback cb)
{
	if (_sock >= 0)                  // 如果套接字已初始化，直接返回成功
		return true;

	_cb_message = cb;                // 保存消息回调函数指针
	_sock = nn_socket(AF_SP, NN_SUB);  // 创建nanomsg套接字，AF_SP表示单进程模式，NN_SUB表示订阅者类型
	if (_sock < 0)                   // 如果创建失败（返回负数表示错误）
	{
		_mgr->log_client(_id, fmtutil::format("MQClient {} has an error {} while initializing", _id, _sock));  // 记录错误日志
		return false;
	}

	nn_setsockopt(_sock, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);  // 订阅所有主题（空字符串表示订阅所有主题）

	int bufsize = RECV_BUF_SIZE;     // 设置接收缓冲区大小为1MB
	nn_setsockopt(_sock, NN_SOL_SOCKET, NN_RCVBUF, &bufsize, sizeof(bufsize));  // 设置套接字选项：接收缓冲区大小

	m_strURL = url;                  // 保存URL地址
	if (nn_connect(_sock, url) < 0)  // 连接到指定URL地址，失败返回负数
	{
		_mgr->log_client(_id, fmtutil::format("MQClient {} has an error while connecting url {}", _id, url));  // 记录错误日志
		return false;
	}
	else                             // 如果连接成功
	{
		_mgr->log_client(_id, fmtutil::format("MQClient {} has connected to {} ", _id, url));  // 记录成功日志
	}

	m_bReady = true;                 // 标记为就绪状态

	_mgr->log_client(_id, fmtutil::format("MQClient {} inited", _id));  // 记录初始化日志
	return true;                     // 返回成功
}

/**
 * @brief 启动客户端接收消息的实现
 * 
 * 启动流程：
 * 1. 检查终止标志和套接字状态
 * 2. 如果后台线程未启动，创建后台接收线程
 * 3. 后台线程持续接收消息并处理
 * 4. 如果线程已启动，记录日志
 */
void MQClient::start()
{
	if (m_bTerminated)               // 如果已终止，直接返回
		return;

	if(_sock < 0)                    // 如果套接字未初始化
	{
		_mgr->log_client(_id, fmtutil::format("MQClient {} has not been initialized yet", _id));  // 记录错误日志
		return;
	}

	if (m_thrdRecv == NULL)         // 如果后台线程未启动
	{
		m_thrdRecv.reset(new StdThread([this]() {  // 创建后台接收线程

			while (!m_bTerminated)    // 循环直到终止标志为true
			{
				bool hasData = false;  // 是否有数据标志
				for(;;)                // 循环接收，直到没有更多数据
				{
					int nBytes = nn_recv(_sock, _recv_buf, RECV_BUF_SIZE, NN_DONTWAIT);  // 非阻塞接收数据，返回接收的字节数
					if (nBytes > 0)    // 如果接收到数据
					{
						m_iCheckTime = TimeUtils::getLocalTimeNow();  // 更新检查时间戳为当前时间（毫秒）
						m_bNeedCheck = true;                          // 设置需要检查标志
						hasData = true;                               // 设置有数据标志
						_buffer.append(_recv_buf, nBytes);           // 将接收到的数据追加到缓冲区
					}
					else               // 如果没有更多数据（nBytes <= 0）
					{
						break;         // 退出接收循环
					}
				}

				if (hasData)          // 如果有数据
					extract_buffer();  // 从缓冲区中提取完整的数据包
				else                  // 如果没有数据
				{
					if(m_iCheckTime != 0 && m_bNeedCheck)  // 如果已开始检查且需要检查
					{
						int64_t now = TimeUtils::getLocalTimeNow();     // 获取当前时间
						int64_t elapse = now - m_iCheckTime;           // 计算经过的时间（毫秒）
						if (elapse >= 60 * 1000)                      // 如果超过60秒未收到消息
						{
							//只通知一次，防止重复通知
							_cb_message(_id, "TIMEOUT", "", 0);        // 调用回调函数，通知超时（主题为"TIMEOUT"）
							m_bNeedCheck = false;                     // 重置需要检查标志，避免重复通知
						}
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 等待1毫秒，避免CPU占用过高
				}
				
			}
		}));

		_mgr->log_client(_id, fmtutil::format("MQClient {} has started successfully", _id));  // 记录启动成功日志
	}
	else                             // 如果后台线程已启动
	{
		_mgr->log_client(_id, fmtutil::format("MQClient {} has already started", _id));  // 记录已启动日志
	}
	
}

/**
 * @brief 从接收缓冲区中提取完整数据包的实现
 * 
 * 提取流程：
 * 1. 循环处理缓冲区中的数据
 * 2. 检查是否有足够的数据（至少一个MQPacket头部）
 * 3. 检查是否有完整的数据包（头部+数据）
 * 4. 如果主题被允许，调用回调函数
 * 5. 移除已处理的数据
 */
void MQClient::extract_buffer()
{
	uint32_t proc_len = 0;           // 已处理的数据长度
	for(;;)                          // 循环处理，直到没有完整的数据包
	{
		//先做长度检查
		if (_buffer.length() - proc_len < sizeof(MQPacket))  // 如果剩余数据不足一个MQPacket头部
			break;                    // 退出循环，等待更多数据

		MQPacket* packet = (MQPacket*)(_buffer.data() + proc_len);  // 将缓冲区转换为MQPacket指针

		if (_buffer.length() - proc_len < sizeof(MQPacket) + packet->_length)  // 如果剩余数据不足完整的数据包（头部+数据）
			break;                    // 退出循环，等待更多数据

		char* data = packet->_data;  // 获取数据指针（实际上数据紧跟在结构体后面）

		if (is_allowed(packet->_topic))  // 如果主题被允许接收（已订阅或未订阅任何主题）
			_cb_message(_id, packet->_topic, packet->_data, packet->_length);  // 调用回调函数，传递消息

		proc_len += sizeof(MQPacket) + packet->_length;  // 累加已处理的数据长度（头部+数据）
	}

	if(proc_len > 0)                 // 如果有已处理的数据
		_buffer.erase(0, proc_len);  // 从缓冲区开头移除已处理的数据
}