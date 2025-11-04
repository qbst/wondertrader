/*!
 * \file EventCaster.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 事件通知器实现文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是EventNotifier类的实现文件，包含了事件通知器的所有功能实现。
 *
 * 主要功能模块：
 * 1. 构造函数和析构函数：初始化成员变量，清理资源
 * 2. 初始化功能：动态加载消息队列服务模块，创建MQ服务器实例
 * 3. 事件通知功能：发送回测事件、数据、资金信息到消息队列
 *
 * 核心算法：
 * - 动态库加载：通过DLLHelper加载WtMsgQue模块
 * - 函数指针绑定：获取动态库中的服务函数地址
 * - JSON序列化：将资金信息序列化为JSON格式后推送
 */
#include "EventNotifier.h"
#include "WtHelper.h"                                               // WonderTrader辅助函数

#include "../Share/TimeUtils.hpp"                                   // 时间工具函数
#include "../Share/DLLHelper.hpp"                                   // 动态库加载辅助类

#include "../Includes/WTSVariant.hpp"                               // 变体类型定义
#include "../WTSTools/WTSLogger.h"                                  // 日志工具

#include <rapidjson/document.h>                                     // RapidJSON文档类
#include <rapidjson/prettywriter.h>                                  // RapidJSON格式化写入器
namespace rj = rapidjson;                                           // RapidJSON命名空间别名

USING_NS_WTP;                                                       // 使用WonderTrader命名空间

/**
 * @brief 消息队列日志回调函数
 * 
 * 目前为空实现，预留接口用于处理MQ服务的日志输出
 * 
 * @param id 服务器ID
 * @param message 日志消息
 * @param bServer 是否来自服务器
 */
void on_mq_log(unsigned long id, const char* message, bool bServer)
{

}

/**
 * @brief EventNotifier构造函数
 * 
 * 初始化成员变量，MQ服务器ID设为0
 */
EventNotifier::EventNotifier()
	: _mq_sid(0)                                                     // 初始化MQ服务器ID为0
{
	
}


/**
 * @brief EventNotifier析构函数
 * 
 * 如果已创建MQ服务器，则销毁它
 */
EventNotifier::~EventNotifier()
{
	if (_remover && _mq_sid != 0)                                  // 如果销毁函数存在且服务器ID不为0
		_remover(_mq_sid);                                          // 销毁MQ服务器
}

/**
 * @brief 初始化事件通知器
 * 
 * 1. 检查配置是否激活
 * 2. 获取消息队列URL
 * 3. 动态加载消息队列服务模块
 * 4. 获取服务函数指针
 * 5. 注册日志回调
 * 6. 创建MQ服务器实例
 * 
 * @param cfg 配置信息
 * @return 是否初始化成功
 */
bool EventNotifier::init(WTSVariant* cfg)
{
	if (!cfg->getBoolean("active"))                                 // 如果配置中active为false
		return false;                                                // 返回失败，不初始化

	m_strURL = cfg->getCString("url");                              // 获取消息队列URL
	std::string module = DLLHelper::wrap_module("WtMsgQue", "lib"); // 构建消息队列模块名称（跨平台兼容）
	//先看工作目录下是否有对应模块
	std::string dllpath = WtHelper::getCWD() + module;              // 先尝试工作目录
	//如果没有,则再看模块目录,即dll同目录下
	if (!StdFile::exists(dllpath.c_str()))                          // 如果工作目录下不存在
		dllpath = WtHelper::getInstDir() + module;                  // 尝试安装目录

	DllHandle dllInst = DLLHelper::load_library(dllpath.c_str());   // 加载动态库
	if (dllInst == NULL)                                            // 如果加载失败
	{
		WTSLogger::error("MQ module %{} loading failed", dllpath.c_str());  // 记录错误日志
		return false;                                                // 返回失败
	}

	_creator = (FuncCreateMQServer)DLLHelper::get_symbol(dllInst, "create_server");  // 获取创建服务器函数地址
	if (_creator == NULL)                                           // 如果获取失败
	{
		DLLHelper::free_library(dllInst);                           // 释放动态库
		WTSLogger::error("MQ module {} is not compatible", dllpath.c_str());  // 记录错误日志
		return false;                                                // 返回失败
	}

	_remover = (FuncDestroyMQServer)DLLHelper::get_symbol(dllInst, "destroy_server");  // 获取销毁服务器函数地址
	_publisher = (FundPublishMessage)DLLHelper::get_symbol(dllInst, "publish_message");  // 获取发布消息函数地址
	_register = (FuncRegCallbacks)DLLHelper::get_symbol(dllInst, "regiter_callbacks");  // 获取注册回调函数地址

	//注册回调函数
	_register(on_mq_log);                                            // 注册日志回调函数
	
	//创建一个MQServer
	_mq_sid = _creator(m_strURL.c_str(), true);                     // 创建MQ服务器实例（true表示服务器模式）

	WTSLogger::info("EventNotifier initialized with channel {}", m_strURL.c_str());  // 记录初始化成功日志

	return true;                                                     // 返回成功
}

/**
 * @brief 通知事件
 * 
 * 发送回测事件到"BT_EVENT"主题
 * 
 * @param evtType 事件类型字符串
 */
void EventNotifier::notifyEvent(const char* evtType)
{
	if (_publisher)                                                 // 如果发布函数存在
		_publisher(_mq_sid, "BT_EVENT", evtType, (unsigned long)strlen(evtType));  // 发布事件到BT_EVENT主题
}

/**
 * @brief 通知数据
 * 
 * 发送原始数据到指定主题
 * 
 * @param topic 主题名称
 * @param data 数据指针
 * @param dataLen 数据长度
 */
void EventNotifier::notifyData(const char* topic, void* data , uint32_t dataLen)
{
	if (_publisher)                                                 // 如果发布函数存在
		_publisher(_mq_sid, topic, (const char*)data, dataLen);     // 发布数据到指定主题
}

/**
 * @brief 通知资金信息
 * 
 * 将资金信息序列化为JSON格式后发送到指定主题
 * 
 * @param topic 主题名称
 * @param uDate 日期
 * @param total_profit 总已实现盈亏
 * @param dynprofit 浮动盈亏
 * @param dynbalance 动态权益
 * @param total_fee 总手续费
 */
void EventNotifier::notifyFund(const char* topic, uint32_t uDate, double total_profit, double dynprofit, double dynbalance, double total_fee)
{
	std::string output;                                              // JSON输出字符串
	{
		rj::Document root(rj::kObjectType);                          // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator(); // 获取内存分配器

		root.AddMember("date", uDate, allocator);                   // 添加日期字段
		root.AddMember("total_profit", total_profit, allocator);     // 添加总已实现盈亏字段
		root.AddMember("dynprofit", dynprofit, allocator);           // 添加浮动盈亏字段
		root.AddMember("dynbalance", dynbalance, allocator);         // 添加动态权益字段
		root.AddMember("total_fee", total_fee, allocator);           // 添加总手续费字段

		rj::StringBuffer sb;                                         // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);               // 创建格式化写入器
		root.Accept(writer);                                         // 将JSON对象写入缓冲区

		output = sb.GetString();                                     // 获取JSON字符串
	}

	if (_publisher)                                                 // 如果发布函数存在
		_publisher(_mq_sid, topic, (const char*)output.c_str(), (unsigned long)output.size());  // 发布JSON数据到指定主题
}
