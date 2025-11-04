/*!
 * \file EventCaster.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 事件通知器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的事件通知器类，用于将交易系统中的各种事件
 * 通过消息队列（MQ）进行异步广播。
 * 
 * 主要功能包括：
 * 1. 交易事件通知：将成交信息、订单状态变化等交易事件转换为JSON格式并广播
 * 2. 日志事件通知：将策略日志、系统日志等日志信息进行广播
 * 3. 图表事件通知：将图表标记、指标数据等图表相关事件进行广播
 * 4. 异步事件处理：使用boost::asio实现异步事件处理，避免阻塞主线程
 * 
 * 设计特点：
 * - 使用消息队列（MQ）作为事件传输通道，支持多订阅者模式
 * - 采用异步IO模式，提高系统响应性能
 * - 将交易数据转换为JSON格式，便于跨平台和跨语言使用
 * - 支持动态加载MQ模块，提高系统灵活性
 */
#pragma once  // 防止头文件重复包含

#include <boost/asio/io_service.hpp>  // 包含boost异步IO服务类，用于异步事件处理

#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义
#include "../Includes/WTSObject.hpp"  // 包含WonderTrader基础对象类
#include "../Share/StdUtils.hpp"  // 包含标准工具类

/**
 * @typedef FuncCreateMQServer
 * @brief 创建MQ服务器函数指针类型
 * 
 * 定义创建消息队列服务器的函数指针签名。
 * @param 消息队列服务器地址/URL
 * @return 返回MQ服务器ID（无符号长整型）
 */
typedef unsigned long(*FuncCreateMQServer)(const char*);  // 定义创建MQ服务器的函数指针类型
/**
 * @typedef FuncDestroyMQServer
 * @brief 销毁MQ服务器函数指针类型
 * 
 * 定义销毁消息队列服务器的函数指针签名。
 * @param mq_id MQ服务器ID
 */
typedef void(*FuncDestroyMQServer)(unsigned long);  // 定义销毁MQ服务器的函数指针类型
/**
 * @typedef FundPublishMessage
 * @brief 发布消息函数指针类型
 * 
 * 定义发布消息到消息队列的函数指针签名。
 * @param mq_id MQ服务器ID
 * @param topic 消息主题
 * @param data 消息数据内容
 * @param length 消息数据长度
 */
typedef void(*FundPublishMessage)(unsigned long, const char*, const char*, unsigned long);  // 定义发布消息的函数指针类型

/**
 * @typedef FuncLogCallback
 * @brief 日志回调函数指针类型
 * 
 * 定义消息队列日志回调函数的签名。
 * @param id 消息队列ID
 * @param message 日志消息内容
 * @param bServer 是否为服务器端日志
 */
typedef void(*FuncLogCallback)(unsigned long, const char*, bool);  // 定义日志回调函数指针类型
/**
 * @typedef FuncRegCallbacks
 * @brief 注册回调函数指针类型
 * 
 * 定义注册回调函数到消息队列的函数指针签名。
 * @param callback 日志回调函数指针
 */
typedef void(*FuncRegCallbacks)(FuncLogCallback);  // 定义注册回调的函数指针类型


NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSTradeInfo;  // 前向声明：交易信息类
class WTSOrderInfo;  // 前向声明：订单信息类
class WTSVariant;  // 前向声明：变体类型类

/**
 * @class EventNotifier
 * @brief 事件通知器类
 * 
 * 该类负责将交易系统中的各种事件通过消息队列进行异步广播。
 * 主要功能：
 * - 交易事件通知：成交信息、订单状态变化等
 * - 日志事件通知：策略日志、系统日志等
 * - 图表事件通知：图表标记、指标数据等
 * - 异步事件处理：使用boost::asio实现异步IO
 * 
 * 设计特点：
 * - 使用消息队列作为事件传输通道
 * - 采用异步IO模式，提高性能
 * - 支持JSON格式数据转换
 * - 支持动态加载MQ模块
 */
class EventNotifier
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化事件通知器对象，设置初始状态。
	 */
	EventNotifier();  // 构造函数声明
	/**
	 * @brief 析构函数
	 * 
	 * 清理事件通知器对象，停止异步IO服务，释放MQ服务器资源。
	 */
	~EventNotifier();  // 析构函数声明


private:
	/**
	 * @brief 将交易信息转换为JSON格式
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param trdInfo 交易信息对象指针
	 * @param output 输出JSON字符串的引用
	 * 
	 * 将交易信息对象转换为JSON格式字符串，用于消息队列传输。
	 */
	void	tradeToJson(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo, std::string& output);  // 交易信息转JSON函数声明
	/**
	 * @brief 将订单信息转换为JSON格式
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param ordInfo 订单信息对象指针
	 * @param output 输出JSON字符串的引用
	 * 
	 * 将订单信息对象转换为JSON格式字符串，用于消息队列传输。
	 */
	void	orderToJson(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo, std::string& output);  // 订单信息转JSON函数声明

public:
	/**
	 * @brief 初始化事件通知器
	 * @param cfg 配置参数对象指针
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 根据配置参数初始化事件通知器：
	 * 1. 检查是否启用事件通知
	 * 2. 加载MQ模块动态库
	 * 3. 获取MQ相关函数指针
	 * 4. 创建MQ服务器实例
	 * 5. 启动异步IO工作线程
	 */
	bool	init(WTSVariant* cfg);  // 初始化函数声明

	/**
	 * @brief 通知交易事件
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param trdInfo 交易信息对象指针
	 * 
	 * 将交易信息转换为JSON格式并通过消息队列异步广播。
	 */
	void	notify(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo);  // 通知交易事件函数声明
	/**
	 * @brief 通知订单事件
	 * @param trader 交易接口名称
	 * @param localid 本地订单ID
	 * @param stdCode 标准合约代码
	 * @param ordInfo 订单信息对象指针
	 * 
	 * 将订单信息转换为JSON格式并通过消息队列异步广播。
	 */
	void	notify(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo);  // 通知订单事件函数声明
	/**
	 * @brief 通知交易接口消息
	 * @param trader 交易接口名称
	 * @param message 消息内容
	 * 
	 * 将交易接口消息通过消息队列异步广播。
	 */
	void	notify(const char* trader, const char* message);  // 通知交易接口消息函数声明

	/**
	 * @brief 通知日志事件
	 * @param tag 日志标签
	 * @param message 日志消息内容
	 * 
	 * 将日志信息通过消息队列异步广播。
	 */
	void	notify_log(const char* tag, const char* message);  // 通知日志事件函数声明

	/**
	 * @brief 通知通用事件
	 * @param message 事件消息内容
	 * 
	 * 将通用事件消息通过消息队列异步广播。
	 */
	void	notify_event(const char* message);  // 通知通用事件函数声明

	/**
	 * @brief 通知图表标记事件
	 * @param time 时间戳
	 * @param straId 策略ID
	 * @param price 价格
	 * @param icon 图标名称
	 * @param tag 标记标签
	 * 
	 * 将图表标记信息通过消息队列异步广播。
	 */
	void	notify_chart_marker(uint64_t time, const char* straId, double price, const char* icon, const char* tag);  // 通知图表标记函数声明
	/**
	 * @brief 通知图表指标事件
	 * @param time 时间戳
	 * @param straId 策略ID
	 * @param idxName 指标名称
	 * @param lineName 线条名称
	 * @param val 指标值
	 * 
	 * 将图表指标数据通过消息队列异步广播。
	 */
	void	notify_chart_index(uint64_t time, const char* straId, const char* idxName, const char* lineName, double val);  // 通知图表指标函数声明
	/**
	 * @brief 通知策略交易事件
	 * @param straId 策略ID
	 * @param stdCode 标准合约代码
	 * @param isLong 是否做多
	 * @param isOpen 是否开仓
	 * @param curTime 当前时间戳
	 * @param price 成交价格
	 * @param userTag 用户标签
	 * 
	 * 将策略交易信息通过消息队列异步广播。
	 */
	void	notify_trade(const char* straId, const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, const char* userTag);  // 通知策略交易事件函数声明

private:
	std::string		_url;  // 消息队列服务器地址/URL
	uint32_t		_mq_sid;  // 消息队列服务器ID
	FuncCreateMQServer	_creator;  // 创建MQ服务器的函数指针
	FuncDestroyMQServer	_remover;  // 销毁MQ服务器的函数指针
	FundPublishMessage	_publisher;  // 发布消息的函数指针
	FuncRegCallbacks	_register;  // 注册回调的函数指针

	bool			_stopped;  // 停止标志，用于控制异步IO工作线程退出
	boost::asio::io_service		_asyncio;  // 异步IO服务对象，用于异步事件处理
	StdThreadPtr				_worker;  // 异步IO工作线程指针
};

NS_WTP_END  // 结束WonderTrader命名空间
