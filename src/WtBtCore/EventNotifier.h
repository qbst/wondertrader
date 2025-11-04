/*!
 * \file EventCaster.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UDP广播对象定义
 *
 * 文件设计逻辑与作用：
 * ====================
 * EventNotifier是WonderTrader回测框架中的事件通知器，用于在回测过程中向外发送事件和数据。
 *
 * 设计目标：
 * 1. 提供回测过程中的事件通知机制，支持外部系统监听回测状态
 * 2. 通过消息队列服务（MQ）实现异步事件推送
 * 3. 支持多种类型的事件通知：通用事件、数据推送、资金信息推送等
 * 4. 通过动态库加载方式实现消息队列服务的可插拔设计
 *
 * 核心功能：
 * - 初始化消息队列服务，建立通信通道
 * - 发送回测事件通知（如回测开始、结束等）
 * - 推送回测数据（如成交记录、持仓变化等）
 * - 推送资金信息（如每日盈亏、浮动盈亏、手续费等）
 *
 * 架构特点：
 * - 采用工厂模式动态加载消息队列服务模块
 * - 使用函数指针方式调用动态库中的服务接口
 * - 支持配置化的消息队列URL和激活状态
 */
#pragma once

#include <queue>                                                    // 队列容器，用于消息缓冲

#include "../Includes/WTSMarcos.h"                                  // WonderTrader宏定义
#include "../Includes/WTSObject.hpp"                                // WonderTrader对象基类
#include "../Share/StdUtils.hpp"                                    // 标准工具函数

/**
 * @brief 创建MQ服务器的函数指针类型
 * 
 * @param url 消息队列URL
 * @param bServer 是否作为服务器模式
 * @return 服务器ID
 */
typedef unsigned long(*FuncCreateMQServer)(const char*, bool);

/**
 * @brief 销毁MQ服务器的函数指针类型
 * 
 * @param sid 服务器ID
 */
typedef void(*FuncDestroyMQServer)(unsigned long);

/**
 * @brief 发布消息的函数指针类型
 * 
 * @param sid 服务器ID
 * @param topic 主题
 * @param data 消息数据
 * @param dataLen 数据长度
 */
typedef void(*FundPublishMessage)(unsigned long, const char*, const char*, unsigned long);

/**
 * @brief 日志回调函数指针类型
 * 
 * @param id 服务器ID
 * @param message 日志消息
 * @param bServer 是否来自服务器
 */
typedef void(*FuncLogCallback)(unsigned long, const char*, bool);

/**
 * @brief 注册回调函数的函数指针类型
 * 
 * @param logCb 日志回调函数
 */
typedef void(*FuncRegCallbacks)(FuncLogCallback);


NS_WTP_BEGIN                                                         // WonderTrader命名空间开始
class WTSVariant;                                                    // 前向声明：变体类型

/**
 * @brief 事件通知器类
 * 
 * 用于在回测过程中向外发送事件和数据通知
 * 通过消息队列服务实现异步推送
 */
class EventNotifier
{
public:
	EventNotifier();                                                 // 构造函数
	~EventNotifier();                                                // 析构函数

public:
	/**
	 * @brief 初始化事件通知器
	 * 
	 * 加载消息队列服务模块，创建MQ服务器实例
	 * 
	 * @param cfg 配置信息（包含active、url等配置项）
	 * @return 是否初始化成功
	 */
	bool	init(WTSVariant* cfg);

	/**
	 * @brief 通知事件
	 * 
	 * 发送回测事件通知到"BT_EVENT"主题
	 * 
	 * @param evtType 事件类型字符串
	 */
	void	notifyEvent(const char* evtType);

	/**
	 * @brief 通知数据
	 * 
	 * 发送原始数据到指定主题
	 * 
	 * @param topic 主题名称
	 * @param data 数据指针
	 * @param dataLen 数据长度
	 */
	void	notifyData(const char* topic, void* data , uint32_t dataLen);

	/**
	 * @brief 通知资金信息
	 * 
	 * 发送资金信息到指定主题，数据格式为JSON
	 * 
	 * @param topic 主题名称
	 * @param uDate 日期
	 * @param total_profit 总已实现盈亏
	 * @param dynprofit 浮动盈亏
	 * @param dynbalance 动态权益
	 * @param total_fee 总手续费
	 */
	void	notifyFund(const char* topic, uint32_t uDate, double total_profit, double dynprofit, double dynbalance, double total_fee);
	

private:
	std::string		m_strURL;                                       // 消息队列URL地址
	uint32_t		_mq_sid;                                        // 消息队列服务器ID
	FuncCreateMQServer	_creator;                                  // 创建MQ服务器的函数指针
	FuncDestroyMQServer	_remover;                                 // 销毁MQ服务器的函数指针
	FundPublishMessage	_publisher;                                // 发布消息的函数指针
	FuncRegCallbacks	_register;                                 // 注册回调函数的函数指针
};

NS_WTP_END                                                           // WonderTrader命名空间结束