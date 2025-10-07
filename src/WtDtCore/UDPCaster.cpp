/*!
 * \file UDPCaster.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UDP数据广播器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了UDPCaster类，提供基于UDP协议的网络数据广播功能。这是一个
 * 复杂的网络模块，实现了完整的UDP服务器功能，包括数据推送和订阅服务。
 * 
 * 核心实现逻辑：
 * 
 * 1. 初始化流程（init方法）：
 *    a) 保存依赖组件引用
 *    b) 解析广播接收者配置
 *    c) 解析组播接收者配置
 *    d) 启动UDP服务
 * 
 * 2. 启动流程（start方法）：
 *    a) 创建广播Socket（设置广播选项）
 *    b) 创建订阅Socket（绑定监听端口）
 *    c) 启动异步接收
 *    d) 启动IO线程
 * 
 * 3. 广播流程（broadcast → do_broadcast）：
 *    a) 将数据包装并加入队列
 *    b) 广播线程从队列取数据
 *    c) 根据数据类型封装UDP包
 *    d) 发送到所有接收者
 * 
 * 4. 订阅服务流程（do_receive）：
 *    a) 异步接收订阅请求
 *    b) 解析请求的合约列表
 *    c) 查询每个合约的当前Tick
 *    d) 封装并发送回客户端
 *    e) 继续接收下一个请求
 * 
 * UDP协议定义：
 * 
 * 1. 消息类型常量：
 *    - UDP_MSG_SUBSCRIBE (0x100)：订阅请求
 *    - UDP_MSG_PUSHTICK (0x200)：推送Tick
 *    - UDP_MSG_PUSHORDQUE (0x201)：推送委托队列
 *    - UDP_MSG_PUSHORDDTL (0x202)：推送逐笔委托
 *    - UDP_MSG_PUSHTRANS (0x203)：推送逐笔成交
 * 
 * 2. 数据包格式：
 *    - UDPReqPacket：请求包（1024字节，紧凑对齐）
 *    - UDPDataPacket<T>：数据包模板（4字节type + T结构体）
 * 
 * 3. 订阅协议：
 *    客户端 → 服务器：
 *    - type: 0x100
 *    - data: "SHFE.rb2105,DCE.i2105"
 *    
 *    服务器 → 客户端：
 *    - 每个合约返回一个UDPTickPacket
 *    - 异步发送，不阻塞
 * 
 * 技术实现细节：
 * 
 * 1. Boost.Asio异步IO：
 *    - async_receive_from：异步接收
 *    - async_send_to：异步发送
 *    - lambda回调：处理完成事件
 *    - io_service.run()：事件循环
 * 
 * 2. 队列缓冲机制：
 *    - broadcast()：数据入队
 *    - 广播线程：数据出队并发送
 *    - 条件变量：通知有数据
 *    - 解耦生产和消费
 * 
 * 3. 引用计数管理：
 *    - CastData包装：retain/release
 *    - 确保数据在广播完成前不被删除
 *    - RAII自动管理
 * 
 * 4. 错误处理：
 *    - 发送失败记录日志
 *    - 继续处理后续数据
 *    - 异常捕获保护
 * 
 * 性能优化：
 * - 异步IO：非阻塞，高并发
 * - 批量处理：队列批量取出
 * - 二进制序列化：Raw格式最快
 * - 无拷贝发送：使用buffer引用
 * 
 * 潜在问题：
 * - UDP不可靠：可能丢包
 * - 无流控：发送过快可能导致丢包
 * - 队列溢出：数据积压时占用内存
 */

#include "UDPCaster.h"                          // 包含UDPCaster类定义
#include "DataManager.h"                        // 包含DataManager类定义

#include "../Share/StrUtil.hpp"                 // 包含字符串工具类
#include "../Includes/WTSDataDef.hpp"           // 包含数据定义
#include "../Includes/WTSContractInfo.hpp"      // 包含合约信息类
#include "../Includes/WTSVariant.hpp"           // 包含配置参数类

#include "../WTSTools/WTSBaseDataMgr.h"         // 包含基础数据管理器
#include "../WTSTools/WTSLogger.h"              // 包含日志系统


// ===== UDP消息类型常量定义 =====
#define UDP_MSG_SUBSCRIBE	0x100               // 订阅请求消息类型（客户端→服务器）
#define UDP_MSG_PUSHTICK	0x200               // 推送Tick消息类型（服务器→客户端）
#define UDP_MSG_PUSHORDQUE	0x201               // 推送委托队列消息类型
#define UDP_MSG_PUSHORDDTL	0x202               // 推送逐笔委托消息类型
#define UDP_MSG_PUSHTRANS	0x203               // 推送逐笔成交消息类型

// ===== UDP数据包结构定义 =====
#pragma pack(push,1)                            // 设置1字节对齐（紧凑对齐，无填充）

/**
 * @struct _UDPReqPacket
 * @brief UDP请求包结构
 * 
 * 客户端发送订阅请求使用的数据包格式。
 * 总大小：1024字节（4 + 1020）
 */
typedef struct _UDPReqPacket
{
	uint32_t		_type;              ///< 消息类型（如UDP_MSG_SUBSCRIBE）
	char			_data[1020];        ///< 消息数据（如合约列表字符串）
} UDPReqPacket;

/**
 * @struct UDPDataPacket
 * @brief UDP数据包模板结构
 * 
 * 服务端推送数据使用的数据包格式。
 * 
 * @tparam T 数据类型（如WTSTickStruct）
 */
template <typename T>
struct UDPDataPacket
{
	uint32_t	_type;                  ///< 消息类型（如UDP_MSG_PUSHTICK）
	T			_data;                  ///< 数据内容（结构体）
};

#pragma pack(pop)                               // 恢复默认对齐

// 实例化数据包类型（便于使用）
typedef UDPDataPacket<WTSTickStruct>	UDPTickPacket;      ///< Tick数据包类型
typedef UDPDataPacket<WTSOrdQueStruct>	UDPOrdQuePacket;    ///< 委托队列数据包类型
typedef UDPDataPacket<WTSOrdDtlStruct>	UDPOrdDtlPacket;    ///< 逐笔委托数据包类型
typedef UDPDataPacket<WTSTransStruct>	UDPTransPacket;     ///< 逐笔成交数据包类型

/**
 * @brief 构造函数实现
 */
UDPCaster::UDPCaster()
	: m_bTerminated(false)                      // 终止标志初始化为false
	, m_bdMgr(NULL)                             // 基础数据管理器指针初始化为空
	, m_dtMgr(NULL)                             // 数据管理器指针初始化为空
{
	
}


/**
 * @brief 析构函数实现
 */
UDPCaster::~UDPCaster()
{
}

/**
 * @brief 初始化UDP广播器实现
 * 
 * 从配置中读取广播和组播接收者列表，初始化UDP服务。
 * 
 * @param cfg 配置参数对象
 * @param bdMgr 基础数据管理器指针
 * @param dtMgr 数据管理器指针
 * @return bool 初始化成功返回true，失败返回false
 */
bool UDPCaster::init(WTSVariant* cfg, WTSBaseDataMgr* bdMgr, DataManager* dtMgr)
{
	// 保存依赖组件引用
	m_bdMgr = bdMgr;
	m_dtMgr = dtMgr;

	// 检查是否启用
	if (!cfg->getBoolean("active"))
		return false;

	// ===== 解析广播接收者配置 =====
	WTSVariant* cfgBC = cfg->get("broadcast");
	if (cfgBC)                                  // 如果配置了broadcast数组
	{
		// 遍历所有广播接收者
		for (uint32_t idx = 0; idx < cfgBC->size(); idx++)
		{
			WTSVariant* cfgItem = cfgBC->get(idx);
			// 添加广播接收者
			// host：IP地址（如"192.168.1.100"或"255.255.255.255"）
			// port：端口号
			// type：数据格式（0=Flat, 1=JSON, 2=Raw）
			addBRecver(cfgItem->getCString("host"), cfgItem->getInt32("port"), cfgItem->getUInt32("type"));
		}
	}

	// ===== 解析组播接收者配置 =====
	WTSVariant* cfgMC = cfg->get("multicast");
	if (cfgMC)                                  // 如果配置了multicast数组
	{
		// 遍历所有组播接收者
		for (uint32_t idx = 0; idx < cfgMC->size(); idx++)
		{
			WTSVariant* cfgItem = cfgMC->get(idx);
			// 添加组播接收者
			// host：组播地址（239.0.0.0 - 239.255.255.255）
			// port：端口号
			// sendport：本地发送端口
			// type：数据格式
			addMRecver(cfgItem->getCString("host"), cfgItem->getInt32("port"), cfgItem->getInt32("sendport"), cfgItem->getUInt32("type"));
		}
	}

	// ===== 处理订阅端口配置 =====
	// By Wesley @ 2022.01.11
	// 这是订阅端口，但是以前全部用的bport，属于笔误
	// 只能写一个兼容了
	int32_t sport = cfg->getInt32("sport");     // 新参数名：sport（subscribe port）
	if (sport == 0)                             // 如果没有配置sport
		sport = cfg->getInt32("bport");         // 尝试读取旧参数名bport（向后兼容）
	
	// 启动UDP服务
	start(sport);

	return true;
}

/**
 * @brief 启动UDP服务实现
 * 
 * 创建UDP Socket，启动IO线程和订阅服务。
 * 
 * @param sport 订阅服务端口
 */
void UDPCaster::start(int sport)
{
	// ===== 创建广播Socket =====
	// 如果有任何接收者列表不为空，创建广播Socket
	if (!m_listFlatRecver.empty() || !m_listJsonRecver.empty() || !m_listRawRecver.empty())
	{
		// 创建UDP Socket
		// endpoint(udp::v4(), 0)：IPv4，系统自动分配端口
		m_sktBroadcast.reset(new UDPSocket(m_ioservice, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)));
		
		// 设置广播选项
		// 允许发送广播包（目标地址255.255.255.255）
		boost::asio::socket_base::broadcast option(true);
		m_sktBroadcast->set_option(option);
	}

	// ===== 创建订阅Socket =====
	try
	{
		// 创建UDP Socket并绑定到订阅端口
		// endpoint(udp::v4(), sport)：IPv4，指定端口
		// 用于接收客户端的订阅请求
		m_sktSubscribe.reset(new UDPSocket(m_ioservice, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), sport)));
	}
	catch(...)                                  // 捕获所有异常（端口被占用等）
	{
		WTSLogger::error("Exception raised while start subscribing service @ port {}", sport);
	}

	// ===== 启动异步接收 =====
	do_receive();                               // 开始异步接收订阅请求

	// ===== 启动IO线程 =====
	// 创建线程运行io_service事件循环
	m_thrdIO.reset(new StdThread([this](){
		try
		{
			// 运行io_service事件循环
			// 此方法会阻塞直到io_service被停止
			// 处理所有异步IO事件（接收、发送回调等）
			m_ioservice.run();
		}
		catch(...)                              // 捕获异常，避免线程崩溃
		{
			m_ioservice.stop();                 // 停止io_service
		}
	}));
}

/**
 * @brief 停止UDP服务实现
 * 
 * 停止所有线程和IO服务。
 */
void UDPCaster::stop()
{
	// 设置终止标志
	m_bTerminated = true;
	
	// 停止IO服务
	// 会导致io_service.run()返回
	m_ioservice.stop();
	
	// 等待IO线程结束
	if (m_thrdIO)
		m_thrdIO->join();

	// 唤醒广播线程（如果在等待）
	m_condCast.notify_all();
	
	// 等待广播线程结束
	if (m_thrdCast)
		m_thrdCast->join();
}

/**
 * @brief 异步接收订阅请求实现
 * 
 * 该方法启动异步接收操作，接收客户端的订阅请求并处理。
 * 
 * 工作流程：
 * 1. 异步接收UDP数据包
 * 2. 回调函数处理接收的数据
 * 3. 解析订阅请求
 * 4. 查询数据并返回
 * 5. 递归调用自己继续接收
 */
void UDPCaster::do_receive()
{
	// 启动异步接收操作
	// async_receive_from：异步接收UDP数据包
	// 参数1：接收缓冲区（m_data，大小max_length）
	// 参数2：发送者端点（输出参数，接收时会被填充）
	// 参数3：回调函数（lambda表达式）
	m_sktSubscribe->async_receive_from(boost::asio::buffer(m_data, max_length), m_senderEP,
		[this](boost::system::error_code ec, std::size_t bytes_recvd)  // 接收完成回调
	{
		// 检查是否有错误
		if(ec)                                  // 如果接收出错
		{
			// 继续接收下一个请求（忽略错误）
			do_receive();
			return;
		}

		// ===== 处理订阅请求 =====
		
		// 检查接收的数据大小是否匹配请求包大小
		if (bytes_recvd == sizeof(UDPReqPacket))
		{
			// 将接收缓冲区转换为请求包指针
			UDPReqPacket* req = (UDPReqPacket*)m_data;

			std::string data;
			
			// 根据请求类型处理
			if (req->_type == UDP_MSG_SUBSCRIBE)            // 如果是订阅请求
			{
				// 解析合约列表（逗号分隔）
				// 例如："SHFE.rb2105,DCE.i2105"
				const StringVector& ay = StrUtil::split(req->_data, ",");
				std::string code, exchg;
				
				// 遍历每个合约
				for(const std::string& fullcode : ay)
				{
					// 解析合约代码格式
					auto pos = fullcode.find(".");
					if (pos == std::string::npos)           // 没有点号，只有代码
						code = fullcode;
					else                                    // 有点号，格式为"交易所.代码"
					{
						code = fullcode.substr(pos + 1);    // 点号后是代码
						exchg = fullcode.substr(0, pos);    // 点号前是交易所
					}
					
					// 验证合约是否存在
					WTSContractInfo* ct = m_bdMgr->getContract(code.c_str(), exchg.c_str());
					if (ct == NULL)                         // 合约不存在
						continue;                           // 跳过该合约

					// 查询该合约的当前Tick数据
					WTSTickData* curTick = m_dtMgr->getCurTick(code.c_str(), exchg.c_str());
					if(curTick == NULL)                     // 当前没有行情
						continue;                           // 跳过该合约

					// 构建响应数据包
					std::string* data = new std::string();  // 动态分配字符串（用于异步发送）
					data->resize(sizeof(UDPTickPacket), 0); // 调整大小为数据包大小
					UDPTickPacket* pkt = (UDPTickPacket*)data->data();  // 转换为数据包指针
					
					pkt->_type = req->_type;                // 设置消息类型
					// 拷贝Tick结构体到数据包
					memcpy(&pkt->_data, &curTick->getTickStruct(), sizeof(WTSTickStruct));
					
					curTick->release();                     // 释放Tick引用

					// 异步发送响应数据包
					m_sktSubscribe->async_send_to(
						boost::asio::buffer(*data, data->size()), m_senderEP,  // 发送到请求来源
						[this, data](const boost::system::error_code& ec, std::size_t /*bytes_sent*/)  // 发送完成回调
					{
						delete data;                        // 删除临时数据（在回调中安全删除）
						if (ec)                             // 如果发送失败
						{
							WTSLogger::error("Sending data on UDP failed: {}", ec.message().c_str());
						}
					});
				}
			}			
		}
		else                                    // 接收的数据大小不匹配
		{
			// 返回错误消息
			std::string* data = new std::string("Can not indentify the command");
			m_sktSubscribe->async_send_to(
				boost::asio::buffer(*data, data->size()), m_senderEP,
				[this, data](const boost::system::error_code& ec, std::size_t /*bytes_sent*/)
			{
				delete data;
				if (ec)
				{
					WTSLogger::error("Sending data on UDP failed: {}", ec.message().c_str());
				}
			});
		}

		// 递归调用，继续接收下一个请求
		// 这是异步模式的典型做法
		do_receive();
	});  // lambda表达式结束
}

/**
 * @brief 添加广播接收者实现
 * 
 * 将接收者添加到对应格式的列表中。
 * 
 * @param remote 远程IP地址字符串
 * @param port 端口号
 * @param type 数据格式（0=Flat, 1=JSON, 2=Raw）
 * @return bool 添加成功返回true，失败返回false
 */
bool UDPCaster::addBRecver(const char* remote, int port, int type /* = 0 */)
{
	try
	{
		// 将字符串IP地址转换为地址对象
		boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(remote);
		
		// 创建接收者对象
		UDPReceiverPtr item(new UDPReceiver(EndPoint(addr, port), type));
		
		// 根据类型添加到对应列表
		if(type == 0)
			m_listFlatRecver.emplace_back(item);            // Flat格式列表
		else if(type == 1)
			m_listJsonRecver.emplace_back(item);            // JSON格式列表
		else if(type == 2)
			m_listRawRecver.emplace_back(item);             // Raw格式列表（最常用）
	}
	catch(...)                                  // IP地址格式错误等异常
	{
		return false;
	}

	return true;
}


/**
 * @brief 添加组播接收者实现
 * 
 * 创建组播Socket并加入组播组。
 * 
 * @param remote 组播地址（239.0.0.0 - 239.255.255.255）
 * @param port 端口号
 * @param sendport 发送端口（本地绑定端口）
 * @param type 数据格式
 * @return bool 添加成功返回true，失败返回false
 */
bool UDPCaster::addMRecver(const char* remote, int port, int sendport, int type /* = 0 */)
{
	try
	{
		// 解析组播地址
		boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(remote);
		
		// 创建接收者对象
		UDPReceiverPtr item(new UDPReceiver(EndPoint(addr, port), type));
		
		// 创建专用的组播Socket
		// sendport：本地绑定端口，0表示系统自动分配
		UDPSocketPtr sock(new UDPSocket(m_ioservice, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), sendport)));
		
		// 加入组播组
		// join_group：设置Socket加入指定的组播组
		// 使Socket能够发送到组播地址
		boost::asio::ip::multicast::join_group option(item->_ep.address());
		sock->set_option(option);
		
		// 根据类型添加到对应列表
		// 组播需要保存Socket和接收者的配对
		if(type == 0)
			m_listFlatGroup.emplace_back(std::make_pair(sock, item));
		else if(type == 1)
			m_listJsonGroup.emplace_back(std::make_pair(sock, item));
		else if(type == 2)
			m_listRawGroup.emplace_back(std::make_pair(sock, item));
	}
	catch(...)
	{
		return false;
	}

	return true;
}

/**
 * @brief 广播Tick数据实现（IDataCaster接口）
 * 
 * @param curTick Tick数据指针
 */
void UDPCaster::broadcast(WTSTickData* curTick)
{
	// 委托给do_broadcast执行实际广播
	do_broadcast(curTick, UDP_MSG_PUSHTICK);
}

/**
 * @brief 广播逐笔委托数据实现
 * 
 * @param curOrdDtl 逐笔委托数据指针
 */
void UDPCaster::broadcast(WTSOrdDtlData* curOrdDtl)
{
	do_broadcast(curOrdDtl, UDP_MSG_PUSHORDDTL);
}

/**
 * @brief 广播委托队列数据实现
 * 
 * @param curOrdQue 委托队列数据指针
 */
void UDPCaster::broadcast(WTSOrdQueData* curOrdQue)
{
	do_broadcast(curOrdQue, UDP_MSG_PUSHORDQUE);
}

/**
 * @brief 广播逐笔成交数据实现
 * 
 * @param curTrans 逐笔成交数据指针
 */
void UDPCaster::broadcast(WTSTransData* curTrans)
{
	do_broadcast(curTrans, UDP_MSG_PUSHTRANS);
}

/**
 * @brief 执行数据广播实现（核心方法）
 * 
 * 该方法将数据加入队列，由广播线程异步发送。
 * 
 * 工作流程：
 * 1. 将数据包装为CastData（增加引用计数）
 * 2. 加入队列（加锁保护）
 * 3. 第一次调用时创建广播线程
 * 4. 唤醒广播线程处理数据
 * 
 * @param data 数据对象指针
 * @param dataType 数据类型（消息类型）
 */
void UDPCaster::do_broadcast(WTSObject* data, uint32_t dataType)
{
	// 参数和状态验证
	if(m_sktBroadcast == NULL || data == NULL || m_bTerminated)
		return;

	// ===== 将数据加入队列 =====
	{
		StdUniqueLock lock(m_mtxCast);          // 加锁保护队列
		
		// push：将CastData对象加入队列
		// CastData构造时会retain()增加引用计数
		m_dataQue.push(CastData(data, dataType));
	} // 锁释放

	// ===== 创建或唤醒广播线程 =====
	
	if(m_thrdCast == NULL)                      // 如果广播线程还未创建
	{
		// 创建广播线程
		m_thrdCast.reset(new StdThread([this](){

			// 线程主循环
			while (!m_bTerminated)
			{
				// 检查队列是否为空
				if(m_dataQue.empty())
				{
					// 队列为空，等待条件变量
					StdUniqueLock lock(m_mtxCast);
					m_condCast.wait(lock);      // 阻塞等待，直到有数据或被唤醒
					continue;                   // 继续下一次循环
				}	

				// ===== 批量取出队列数据 =====
				std::queue<CastData> tmpQue;    // 临时队列
				{
					StdUniqueLock lock(m_mtxCast);
					tmpQue.swap(m_dataQue);     // 交换队列（清空m_dataQue，获取所有数据）
				} // 锁释放，允许新数据入队
				
				// ===== 处理临时队列中的所有数据 =====
				while(!tmpQue.empty())
				{
					const CastData& castData = tmpQue.front();  // 获取队首数据

					if (castData._data == NULL)
						break;                  // 数据无效，退出处理

					// ===== Raw格式广播（最常用，二进制格式） =====
					if (!m_listRawGroup.empty() || !m_listRawRecver.empty())
					{
						std::string buf_raw;    // 序列化缓冲区
						
						// 根据数据类型序列化
						if (castData._datatype == UDP_MSG_PUSHTICK)  // Tick数据
						{
							// 分配缓冲区
							buf_raw.resize(sizeof(UDPTickPacket));
							
							// 构建数据包
							UDPTickPacket* pack = (UDPTickPacket*)buf_raw.data();
							pack->_type = castData._datatype;
							
							// 类型转换并拷贝数据
							WTSTickData* curObj = (WTSTickData*)castData._data;
							memcpy(&pack->_data, &curObj->getTickStruct(), sizeof(WTSTickStruct));
						}
						else if (castData._datatype == UDP_MSG_PUSHORDDTL)  // 逐笔委托
						{
							buf_raw.resize(sizeof(UDPOrdDtlPacket));
							UDPOrdDtlPacket* pack = (UDPOrdDtlPacket*)buf_raw.data();
							pack->_type = castData._datatype;
							WTSOrdDtlData* curObj = (WTSOrdDtlData*)castData._data;
							memcpy(&pack->_data, &curObj->getOrdDtlStruct(), sizeof(WTSOrdDtlStruct));
						}
						else if (castData._datatype == UDP_MSG_PUSHORDQUE)  // 委托队列
						{
							buf_raw.resize(sizeof(UDPOrdQuePacket));
							UDPOrdQuePacket* pack = (UDPOrdQuePacket*)buf_raw.data();
							pack->_type = castData._datatype;
							WTSOrdQueData* curObj = (WTSOrdQueData*)castData._data;
							memcpy(&pack->_data, &curObj->getOrdQueStruct(), sizeof(WTSOrdQueStruct));
						}
						else if (castData._datatype == UDP_MSG_PUSHTRANS)   // 逐笔成交
						{
							buf_raw.resize(sizeof(UDPTransPacket));
							UDPTransPacket* pack = (UDPTransPacket*)buf_raw.data();
							pack->_type = castData._datatype;
							WTSTransData* curObj = (WTSTransData*)castData._data;
							memcpy(&pack->_data, &curObj->getTransStruct(), sizeof(WTSTransStruct));
						}
						else                            // 未知数据类型
						{
							break;                      // 跳出处理
						}

						// ===== 发送到单播/广播接收者 =====
						boost::system::error_code ec;
						for (auto it = m_listRawRecver.begin(); it != m_listRawRecver.end(); it++)
						{
							const UDPReceiverPtr& receiver = (*it);
							// 同步发送（不使用async，因为已在专用线程中）
							m_sktBroadcast->send_to(boost::asio::buffer(buf_raw), receiver->_ep, 0, ec);
							if (ec)                     // 发送失败
							{
								// 记录错误日志（包含目标地址和端口）
								WTSLogger::error("Error occured while sending to ({}:{}): {}({})", 
									receiver->_ep.address().to_string(), receiver->_ep.port(), ec.value(), ec.message());
							}
						}

						// ===== 发送到组播接收者 =====
						for (auto it = m_listRawGroup.begin(); it != m_listRawGroup.end(); it++)
						{
							const MulticastPair& item = *it;
							// 使用专用的组播Socket发送
							it->first->send_to(boost::asio::buffer(buf_raw), item.second->_ep, 0, ec);
							if (ec)
							{
								WTSLogger::error("Error occured while sending to ({}:{}): {}({})",
									item.second->_ep.address().to_string(), item.second->_ep.port(), ec.value(), ec.message());
							}
						}
					}
					// Flat和JSON格式的处理未实现（代码中未包含）

					tmpQue.pop();               // 移除已处理的数据
				} // while(!tmpQue.empty()) 结束
			} // while (!m_bTerminated) 结束
		}));  // lambda和thread创建结束
	}
	else                                        // 如果广播线程已存在
	{
		// 唤醒线程处理新数据
		m_condCast.notify_all();
	}
}

/**
 * @brief 处理广播发送完成回调实现
 * 
 * @param ep 目标端点
 * @param error 错误码
 * @param bytes_transferred 传输字节数
 */
void UDPCaster::handle_send_broad(const EndPoint& ep, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)                                   // 如果发送失败
	{
		// 记录错误日志
		WTSLogger::error("Broadcasting of market data failed, remote addr: {}, error message: {}", ep.address().to_string().c_str(), error.message().c_str());
	}
}

/**
 * @brief 处理组播发送完成回调实现
 * 
 * @param ep 目标端点
 * @param error 错误码
 * @param bytes_transferred 传输字节数
 */
void UDPCaster::handle_send_multi(const EndPoint& ep, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)                                   // 如果组播发送失败
	{
		// 记录错误日志
		WTSLogger::error("Multicasting of market data failed, remote addr: {}, error message: {}", ep.address().to_string().c_str(), error.message().c_str());
	}
}

