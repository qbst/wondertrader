/*!
 * \file MQServer.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief MQServer类的实现文件，实现消息发布服务器的所有功能
 * 
 * 本文件实现了MQServer类的所有方法，包括：
 * - 服务器的创建和初始化
 * - nanomsg套接字的创建和绑定
 * - 消息的发布和异步发送
 * - 后台发送线程的管理
 * - 心跳包机制
 * 
 * 技术实现：
 * - 使用nanomsg库的NN_PUB套接字类型
 * - 使用后台线程异步发送消息，避免阻塞
 * - 使用消息队列缓存待发送的消息
 * - 支持确认模式和心跳包机制
 */
#include "MQServer.h"                // 包含类定义头文件
#include "MQManager.h"               // 包含管理器类定义

#include "../Share/StrUtil.hpp"      // 字符串工具函数

#include <spdlog/fmt/fmt.h>          // 格式化字符串库
#include <atomic>                    // 原子操作支持


#ifndef NN_STATIC_LIB                // 如果未定义静态库宏
#define NN_STATIC_LIB                // 定义为静态库模式
#endif
#include <nanomsg/nn.h>              // nanomsg核心库
#include <nanomsg/pubsub.h>          // nanomsg发布-订阅模式定义


USING_NS_WTP;                        // 使用WonderTrader命名空间


/**
 * @brief 生成唯一的服务器ID
 * @return 返回服务器ID（32位无符号整数）
 * 
 * 使用原子计数器生成唯一的服务器ID。
 * 服务器ID从1001开始，每次调用自动递增。
 * 线程安全。
 */
inline uint32_t makeMQSvrId()
{
	static std::atomic<uint32_t> _auto_server_id{ 1001 };  // 静态原子计数器，初始值为1001
	return _auto_server_id.fetch_add(1);                  // 原子操作：获取当前值并加1，返回旧值
}


/**
 * @brief MQServer构造函数实现
 * @param mgr 消息队列管理器指针
 * 
 * 使用初始化列表初始化所有成员变量：
 * - _sock初始化为-1（未初始化状态）
 * - _ready初始化为false（未就绪）
 * - _mgr设置为传入的管理器指针
 * - _confirm初始化为false（不确认模式）
 * - m_bTerminated初始化为false（未终止）
 * - m_bTimeout初始化为false（未超时）
 * 然后调用makeMQSvrId()生成唯一ID。
 */
MQServer::MQServer(MQManager* mgr)
	: _sock(-1)                      // 初始化套接字为-1（未初始化）
	, _ready(false)                  // 初始化就绪标志为false
	, _mgr(mgr)                      // 初始化管理器指针为传入的值
	, _confirm(false)                // 初始化确认标志为false（不确认模式）
	, m_bTerminated(false)           // 初始化终止标志为false
	, m_bTimeout(false)              // 初始化超时标志为false
{
	_id = makeMQSvrId();             // 生成唯一的服务器ID
}

/**
 * @brief MQServer析构函数实现
 * 
 * 清理资源：
 * - 如果服务器未就绪，直接返回
 * - 设置终止标志，通知后台线程退出
 * - 唤醒所有等待的线程
 * - 等待后台线程结束
 * - 注意：套接字关闭被注释掉，可能由nanomsg自动管理
 */
MQServer::~MQServer()
{
	if (!_ready)                     // 如果服务器未就绪，直接返回
		return;

	m_bTerminated = true;            // 设置终止标志，通知后台线程退出
	m_condCast.notify_all();         // 唤醒所有等待在条件变量上的线程
	if (m_thrdCast)                  // 如果后台线程存在
		m_thrdCast->join();          // 等待后台线程结束

	//if (_sock >= 0)                 // 注释掉的套接字关闭代码
	//	nn_close(_sock);             // nanomsg套接字关闭，可能由库自动管理
}

/**
 * @brief 初始化服务器的实现
 * @param url 服务器地址
 * @param confirm 是否需要确认连接
 * @return 返回初始化是否成功
 * 
 * 初始化流程：
 * 1. 检查套接字是否已初始化
 * 2. 保存确认标志
 * 3. 创建nanomsg PUB套接字
 * 4. 设置发送缓冲区大小（8MB）
 * 5. 绑定到指定URL地址
 * 6. 标记为就绪状态
 */
bool MQServer::init(const char* url, bool confirm /* = false */)
{
	if (_sock >= 0)                  // 如果套接字已初始化，直接返回成功
		return true;

	_confirm = confirm;               // 保存确认标志

	_sock = nn_socket(AF_SP, NN_PUB);  // 创建nanomsg套接字，AF_SP表示单进程模式，NN_PUB表示发布者类型
	if(_sock < 0)                    // 如果创建失败（返回负数表示错误）
	{
		_mgr->log_server(_id, fmt::format("MQServer {} has an error {} while initializing", _id, _sock).c_str());  // 记录错误日志
		return false;
	}

	int bufsize = 8 * 1024 * 1024;   // 设置发送缓冲区大小为8MB（8 * 1024 * 1024字节）
	nn_setsockopt(_sock, NN_SOL_SOCKET, NN_SNDBUF, &bufsize, sizeof(bufsize));  // 设置套接字选项：发送缓冲区大小

	_url = url;                      // 保存URL地址
	if(nn_bind(_sock, url) < 0)      // 绑定套接字到指定URL地址，失败返回负数
	{
		_mgr->log_server(_id, fmt::format("MQServer {} has an error while binding url {}", _id, url).c_str());  // 记录错误日志
		return false;
	}
	else                             // 如果绑定成功
	{
		_mgr->log_server(_id, fmt::format("MQServer {} has binded to {} ", _id, url).c_str());  // 记录成功日志
	}

	_ready = true;                   // 标记为就绪状态

	_mgr->log_server(_id, fmt::format("MQServer {} ready", _id).c_str());  // 记录就绪日志
	return true;                     // 返回成功
}

/**
 * @brief 发布消息的实现
 * @param topic 消息主题
 * @param data 消息数据指针
 * @param dataLen 消息数据长度
 * 
 * 发布流程：
 * 1. 检查套接字是否已初始化
 * 2. 验证数据有效性
 * 3. 将消息加入发送队列（加锁保护）
 * 4. 如果后台线程未启动，启动后台线程
 * 5. 唤醒等待的线程（如果有）
 */
void MQServer::publish(const char* topic, const void* data, uint32_t dataLen)
{
	if(_sock < 0)                    // 如果套接字未初始化
	{
		_mgr->log_server(_id, fmt::format("MQServer {} has not been initialized yet", _id).c_str());  // 记录错误日志
		return;
	}

	if(data == NULL || dataLen == 0 || m_bTerminated)  // 如果数据为空、长度为0或已终止，直接返回
		return;

	{
		StdUniqueLock lock(m_mtxCast);  // 加锁保护消息队列
		m_dataQue.push(PubData(topic, data, dataLen));  // 将消息加入发送队列
		m_bTimeout = false;             // 重置超时标志，表示有新数据
	}

	if(m_thrdCast == NULL)           // 如果后台线程未启动
	{
		m_thrdCast.reset(new StdThread([this](){  // 创建后台发送线程

			if (m_sendBuf.empty())    // 如果发送缓冲区为空
				m_sendBuf.resize(1024 * 1024, 0);  // 初始化发送缓冲区大小为1MB
			while (!m_bTerminated)    // 循环直到终止标志为true
			{
				int cnt = (int)nn_get_statistic(_sock, NN_STAT_CURRENT_CONNECTIONS);  // 获取当前连接的客户端数量
				if(m_dataQue.empty() || (cnt == 0 && _confirm))  // 如果队列为空，或者确认模式下没有客户端连接
				{
					StdUniqueLock lock(m_mtxCast);  // 加锁
					m_bTimeout = true;              // 设置超时标志
					m_condCast.wait_for(lock, std::chrono::seconds(60));  // 等待60秒，或者被notify唤醒
					//如果有新的数据进来，timeout会被改为false
					//如果没有新的数据进来，timeout会保持为true
					if (m_bTimeout)                 // 如果等待超时（60秒内没有新数据）
					{
						//等待超时以后，广播心跳包
						m_dataQue.push(PubData("HEARTBEAT", "", 0));  // 发送心跳包，保持连接活跃
					}
					else                             // 如果有新数据（被notify唤醒）
					{
						continue;                    // 继续循环处理新数据
					}
				}	

				PubDataQue tmpQue;     // 临时队列，用于批量处理消息
				{
					StdUniqueLock lock(m_mtxCast);  // 加锁保护
					tmpQue.swap(m_dataQue);         // 交换队列，快速清空原队列（避免长时间持锁）
				}
				
				while(!tmpQue.empty())  // 处理临时队列中的所有消息
				{
					const PubData& pubData = tmpQue.front();  // 获取队列头部的消息

					if (!pubData._data.empty())     // 如果消息数据不为空
					{
						std::size_t len = sizeof(MQPacket) + pubData._data.size();  // 计算数据包总长度（结构体+数据）
						if (m_sendBuf.size() < len)  // 如果发送缓冲区不够大
							m_sendBuf.resize(m_sendBuf.size() * 2);  // 将缓冲区大小翻倍
						MQPacket* pack = (MQPacket*)m_sendBuf.data();  // 将缓冲区转换为MQPacket指针
						strncpy(pack->_topic, pubData._topic.c_str(), 32);  // 复制主题字符串（最多32字符）
						pack->_length = (uint32_t)pubData._data.size();  // 设置数据长度
						memcpy(&pack->_data, pubData._data.data(), pubData._data.size());  // 复制数据内容到数据包
						int bytes_snd = 0;          // 已发送字节数
						for(;;)                      // 循环发送，直到全部发送完成
						{
							int bytes = nn_send(_sock, m_sendBuf.data() + bytes_snd, len - bytes_snd, 0);  // 发送数据，返回实际发送的字节数
							if (bytes >= 0)          // 如果发送成功（返回非负数）
							{
								bytes_snd += bytes;   // 累加已发送字节数
								if(bytes_snd == len)  // 如果全部发送完成
									break;            // 退出循环
							}
							else                      // 如果发送失败（返回负数）
								std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 等待1毫秒后重试
						}
						
					}
					tmpQue.pop();                    // 从临时队列中移除已处理的消息
				} 
			}
		}));
	}
	else                             // 如果后台线程已启动
	{
		m_condCast.notify_all();     // 唤醒等待的线程，通知有新消息
	}
}