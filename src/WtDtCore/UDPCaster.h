/*!
 * \file UDPCaster.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UDP数据广播器定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了UDPCaster（UDP数据广播器）类，是WonderTrader框架中基于UDP协议
 * 的网络数据广播模块。该类实现了复杂的UDP广播功能，支持单播、广播、组播三种
 * 模式，并提供订阅服务和多种数据格式。
 * 
 * 核心设计理念：
 * 
 * 1. 多模式支持：
 *    - 单播（Unicast）：点对点发送，精确投递
 *    - 广播（Broadcast）：局域网内所有主机接收
 *    - 组播（Multicast）：特定组内的主机接收
 * 
 * 2. 多格式支持：
 *    - Raw格式（type=2）：二进制结构体，最快
 *    - Flat格式（type=0）：平面格式（未实现）
 *    - JSON格式（type=1）：JSON字符串（未实现）
 * 
 * 3. 双向通信：
 *    - 订阅服务：客户端发送订阅请求，服务端返回数据
 *    - 主动推送：服务端主动广播数据
 * 
 * 4. 异步架构：
 *    - 使用Boost.Asio实现异步IO
 *    - IO线程：处理网络收发
 *    - 广播线程：处理数据广播
 *    - 队列缓冲：解耦数据接收和发送
 * 
 * 架构设计：
 * 
 *   [DataManager]
 *        ↓ broadcastTick()
 *   [UDPCaster]
 *        ↓ do_broadcast()
 *   [数据队列]
 *        ↓
 *   [广播线程] ───────→ [UDP Socket]
 *        │                    ↓
 *        ├─────────→ [单播接收者列表]
 *        ├─────────→ [广播接收者列表]
 *        └─────────→ [组播接收者列表]
 * 
 *   [订阅服务]
 *   客户端 → UDP请求 → do_receive() → 查询数据 → 返回
 * 
 * 网络通信协议：
 * 
 * 1. 消息类型（Message Type）：
 *    - 0x100：订阅请求（UDP_MSG_SUBSCRIBE）
 *    - 0x200：推送Tick（UDP_MSG_PUSHTICK）
 *    - 0x201：推送委托队列（UDP_MSG_PUSHORDQUE）
 *    - 0x202：推送逐笔委托（UDP_MSG_PUSHORDDTL）
 *    - 0x203：推送逐笔成交（UDP_MSG_PUSHTRANS）
 * 
 * 2. 数据包结构：
 *    请求包（UDPReqPacket）：
 *    ┌─────────┬──────────────┐
 *    │  _type  │    _data     │
 *    │ 4字节   │   1020字节   │
 *    └─────────┴──────────────┘
 *    
 *    数据包（UDPDataPacket<T>）：
 *    ┌─────────┬──────────────┐
 *    │  _type  │     _data    │
 *    │ 4字节   │   T结构体    │
 *    └─────────┴──────────────┘
 * 
 * 3. 订阅协议：
 *    客户端发送：
 *    - type: 0x100
 *    - data: "SHFE.rb2105,DCE.i2105"（逗号分隔）
 *    
 *    服务端返回：
 *    - 查询每个合约的当前Tick
 *    - 封装成UDPTickPacket
 *    - 发送回客户端
 * 
 * 配置示例：
 * @code
 *   {
 *     "active": true,
 *     "sport": 3997,                    // 订阅服务端口
 *     "broadcast": [                     // 单播/广播接收者列表
 *       {"host": "192.168.1.100", "port": 9001, "type": 2},
 *       {"host": "255.255.255.255", "port": 9002, "type": 2}
 *     ],
 *     "multicast": [                     // 组播接收者列表
 *       {"host": "239.1.1.1", "port": 9003, "sendport": 0, "type": 2}
 *     ]
 *   }
 * @endcode
 * 
 * 技术特点：
 * 
 * 1. 异步IO：
 *    - Boost.Asio框架
 *    - 事件驱动架构
 *    - 非阻塞IO
 *    - 高并发支持
 * 
 * 2. 引用计数：
 *    - CastData使用引用计数管理对象
 *    - 避免数据在广播过程中被删除
 *    - RAII自动管理
 * 
 * 3. 队列缓冲：
 *    - 使用std::queue缓冲数据
 *    - 解耦接收和发送
 *    - 条件变量通知
 * 
 * 4. 错误处理：
 *    - 发送失败记录日志
 *    - 不中断服务
 *    - 异常安全
 * 
 * 性能指标：
 * - 延迟：1-10毫秒（网络延迟）
 * - 吞吐量：10,000-100,000 消息/秒
 * - 适用：局域网内数据分发
 * 
 * 使用场景：
 * - 局域网行情分发
 * - 多机房数据同步
 * - 实时监控系统
 * - 风控数据推送
 */

#pragma once                                                // 防止头文件重复包含

#include "IDataCaster.h"                                    // 包含数据广播器接口
#include "../Includes/WTSObject.hpp"                        // 包含WonderTrader对象基类
#include "../Share/StdUtils.hpp"                            // 包含标准工具类

#include <boost/asio.hpp>                                   // Boost异步IO库
#include <queue>                                            // STL队列容器

// 前向声明
NS_WTP_BEGIN                                                // 开始WonderTrader命名空间
	class WTSVariant;                                       // 配置参数类
NS_WTP_END                                                  // 结束WonderTrader命名空间

USING_NS_WTP;                                               // 使用WonderTrader命名空间

class WTSBaseDataMgr;                                       // 基础数据管理器
class DataManager;                                          // 数据管理器

/**
 * @class UDPCaster
 * @brief UDP数据广播器类
 * 
 * 该类实现了基于UDP协议的网络数据广播功能。支持单播、广播、组播三种模式，
 * 并提供订阅服务。使用Boost.Asio框架实现异步IO，性能高效。
 * 
 * 继承关系：
 * - 公有继承IDataCaster：实现数据广播器接口
 * 
 * 主要功能：
 * 
 * 1. 数据广播：
 *    - 接收来自DataManager的数据
 *    - 封装为UDP数据包
 *    - 发送到所有注册的接收者
 *    - 支持多种广播模式
 * 
 * 2. 订阅服务：
 *    - 监听订阅端口
 *    - 接收客户端订阅请求
 *    - 查询当前行情数据
 *    - 返回给客户端
 * 
 * 3. 多模式支持：
 *    - 单播/广播：addBRecver()
 *    - 组播：addMRecver()
 *    - 可同时使用多种模式
 * 
 * 线程模型：
 * - IO线程：运行Boost.Asio的io_service
 * - 广播线程：从队列取数据并发送
 * - 主线程：接收broadcast调用
 * 
 * 典型使用：
 * @code
 *   UDPCaster* caster = new UDPCaster();
 *   caster->init(config, bdMgr, dtMgr);
 *   // 添加接收者...
 *   // broadcast会自动被调用...
 *   caster->stop();
 *   delete caster;
 * @endcode
 */
class UDPCaster : public IDataCaster
{
public:
	/**
	 * @brief 构造函数
	 */
	UDPCaster();
	
	/**
	 * @brief 析构函数
	 */
	~UDPCaster();

	/**
	 * @typedef EndPoint
	 * @brief UDP端点类型（IP地址+端口）
	 */
	typedef boost::asio::ip::udp::endpoint EndPoint;
	
	/**
	 * @struct tagUDPReceiver
	 * @brief UDP接收者结构
	 * 
	 * 存储单个UDP接收者的信息。
	 */
	typedef struct tagUDPReceiver
	{
		EndPoint	_ep;                ///< 端点（IP地址+端口）
		uint32_t	_type;              ///< 数据格式类型（0=Flat, 1=JSON, 2=Raw）

		/**
		 * @brief 构造函数
		 * 
		 * @param ep 端点
		 * @param t 数据格式类型
		 */
		tagUDPReceiver(EndPoint ep, uint32_t t)
		{
			_ep = ep;
			_type = t;
		}

	} UDPReceiver;
	
	/**
	 * @typedef UDPReceiverPtr
	 * @brief UDP接收者智能指针类型
	 */
	typedef std::shared_ptr<UDPReceiver>	UDPReceiverPtr;
	
	/**
	 * @typedef ReceiverList
	 * @brief 接收者列表类型
	 */
	typedef std::vector<UDPReceiverPtr>		ReceiverList;

private:
	/**
	 * @brief 处理广播发送完成回调
	 * 
	 * @param ep 端点
	 * @param error 错误码
	 * @param bytes_transferred 传输字节数
	 */
	void	handle_send_broad(const EndPoint& ep, const boost::system::error_code& error, std::size_t bytes_transferred); 
	
	/**
	 * @brief 处理组播发送完成回调
	 * 
	 * @param ep 端点
	 * @param error 错误码
	 * @param bytes_transferred 传输字节数
	 */
	void	handle_send_multi(const EndPoint& ep, const boost::system::error_code& error, std::size_t bytes_transferred); 

	/**
	 * @brief 接收订阅请求（异步）
	 * 
	 * 监听订阅端口，接收客户端的订阅请求并处理。
	 */
	void	do_receive();

	/**
	 * @brief 执行数据广播（内部方法）
	 * 
	 * @param data 数据对象指针
	 * @param dataType 数据类型
	 */
	void	do_broadcast(WTSObject* data, uint32_t dataType);

public:
	/**
	 * @brief 初始化UDP广播器
	 * 
	 * @param cfg 配置参数
	 * @param bdMgr 基础数据管理器
	 * @param dtMgr 数据管理器
	 * @return bool 初始化成功返回true
	 */
	bool	init(WTSVariant* cfg, WTSBaseDataMgr* bdMgr, DataManager* dtMgr);
	
	/**
	 * @brief 启动UDP服务
	 * 
	 * @param bport 订阅服务端口
	 */
	void	start(int bport);
	
	/**
	 * @brief 停止UDP服务
	 */
	void	stop();

	/**
	 * @brief 添加广播接收者
	 * 
	 * @param remote 远程地址（IP地址字符串）
	 * @param port 端口号
	 * @param type 数据格式类型（0=Flat, 1=JSON, 2=Raw）
	 * @return bool 添加成功返回true
	 */
	bool	addBRecver(const char* remote, int port, int type = 0);
	
	/**
	 * @brief 添加组播接收者
	 * 
	 * @param remote 组播地址（239.x.x.x）
	 * @param port 端口号
	 * @param sendport 发送端口（本地绑定）
	 * @param type 数据格式类型
	 * @return bool 添加成功返回true
	 */
	bool	addMRecver(const char* remote, int port, int sendport, int type = 0);

public:
	//////////////////////////////////////////////////////////////////////////
	// IDataCaster 接口实现
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 广播Tick数据
	 * 
	 * @param curTick Tick数据指针
	 */
	virtual void	broadcast(WTSTickData* curTick) override;
	
	/**
	 * @brief 广播委托队列数据
	 * 
	 * @param curOrdQue 委托队列数据指针
	 */
	virtual void	broadcast(WTSOrdQueData* curOrdQue) override;
	
	/**
	 * @brief 广播逐笔委托数据
	 * 
	 * @param curOrdDtl 逐笔委托数据指针
	 */
	virtual void	broadcast(WTSOrdDtlData* curOrdDtl) override;
	
	/**
	 * @brief 广播逐笔成交数据
	 * 
	 * @param curTrans 逐笔成交数据指针
	 */
	virtual void	broadcast(WTSTransData* curTrans) override;

private:
	/**
	 * @typedef UDPSocket
	 * @brief UDP套接字类型
	 */
	typedef boost::asio::ip::udp::socket	UDPSocket;
	
	/**
	 * @typedef UDPSocketPtr
	 * @brief UDP套接字智能指针类型
	 */
	typedef std::shared_ptr<UDPSocket>		UDPSocketPtr;

	/**
	 * @brief 最大接收长度常量
	 */
	enum 
	{ 
		max_length = 2048                   ///< 接收缓冲区大小（2KB）
	};

	boost::asio::ip::udp::endpoint	m_senderEP;         ///< 发送者端点（接收订阅请求时记录）
	char			m_data[max_length];                 ///< 接收缓冲区

	// 单播/广播接收者列表（按数据格式分类）
	ReceiverList	m_listFlatRecver;                   ///< Flat格式接收者列表
	ReceiverList	m_listJsonRecver;                   ///< JSON格式接收者列表
	ReceiverList	m_listRawRecver;                    ///< Raw格式接收者列表
	
	UDPSocketPtr	m_sktBroadcast;                     ///< 广播套接字（用于发送）
	UDPSocketPtr	m_sktSubscribe;                     ///< 订阅套接字（用于接收订阅请求）

	/**
	 * @typedef MulticastPair
	 * @brief 组播对（Socket + 接收者信息）
	 */
	typedef std::pair<UDPSocketPtr,UDPReceiverPtr>	MulticastPair;
	
	/**
	 * @typedef MulticastList
	 * @brief 组播列表类型
	 */
	typedef std::vector<MulticastPair>	MulticastList;
	
	// 组播接收者列表（按数据格式分类，每个有独立的Socket）
	MulticastList	m_listFlatGroup;                    ///< Flat格式组播列表
	MulticastList	m_listJsonGroup;                    ///< JSON格式组播列表
	MulticastList	m_listRawGroup;                     ///< Raw格式组播列表
	
	boost::asio::io_service		m_ioservice;            ///< Boost.Asio IO服务对象
	StdThreadPtr	m_thrdIO;                           ///< IO线程（运行io_service）

	StdThreadPtr	m_thrdCast;                         ///< 广播线程（处理数据队列）
	StdCondVariable	m_condCast;                         ///< 广播线程条件变量
	StdUniqueMutex	m_mtxCast;                          ///< 广播线程互斥锁
	bool			m_bTerminated;                      ///< 终止标志

	WTSBaseDataMgr*	m_bdMgr;                            ///< 基础数据管理器指针
	DataManager*	m_dtMgr;                            ///< 数据管理器指针

	/**
	 * @struct _CastData
	 * @brief 广播数据包装结构（带引用计数）
	 * 
	 * 该结构包装数据对象，使用引用计数管理生命周期。
	 * 确保数据在广播过程中不会被删除。
	 */
	typedef struct _CastData
	{
		uint32_t	_datatype;          ///< 数据类型（消息类型）
		WTSObject*	_data;              ///< 数据对象指针（基类指针）

		/**
		 * @brief 构造函数
		 * 
		 * @param obj 数据对象指针
		 * @param dataType 数据类型
		 */
		_CastData(WTSObject* obj = NULL, uint32_t dataType = 0)
			: _data(obj), _datatype(dataType)
		{
			if (_data)
				_data->retain();                            // 增加引用计数
		}

		/**
		 * @brief 拷贝构造函数
		 * 
		 * @param data 源对象
		 */
		_CastData(const _CastData& data)
			: _data(data._data), _datatype(data._datatype)
		{
			if (_data)
				_data->retain();                            // 增加引用计数
		}

		/**
		 * @brief 析构函数
		 * 
		 * 释放数据对象的引用。
		 */
		~_CastData()
		{
			if (_data)
			{
				_data->release();                           // 减少引用计数
				_data = NULL;
			}
		}
	} CastData;

	std::queue<CastData>		m_dataQue;                  ///< 数据队列（缓冲待广播的数据）
};
