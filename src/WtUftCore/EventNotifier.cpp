/*!
 * \file EventCaster.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 事件通知器实现文件
 *
 * 本文件实现了EventNotifier类的所有功能，包括：
 * 1. 构造函数和析构函数：初始化事件通知器，清理资源
 * 2. 消息队列初始化：加载消息队列模块，创建服务器
 * 3. 事件转换：将成交、订单信息转换为JSON格式
 * 4. 异步事件广播：通过消息队列异步发布各种事件
 * 5. 工作线程管理：启动异步处理线程处理事件队列
 */
#include "EventNotifier.h"  // 事件通知器头文件
#include "WtHelper.h"  // WonderTrader辅助工具

#include "../Share/TimeUtils.hpp"  // 时间工具函数
#include "../Share/DLLHelper.hpp"  // 动态库加载工具

#include "../Includes/WTSTradeDef.hpp"  // 交易定义
#include "../Includes/WTSCollection.hpp"  // 集合类
#include "../Includes/WTSVariant.hpp"  // 变体配置类

#include "../WTSTools/WTSLogger.h"  // 日志工具

#include <rapidjson/document.h>  // RapidJSON文档类
#include <rapidjson/prettywriter.h>  // RapidJSON格式化写入器
#include <rapidjson/writer.h>  // RapidJSON写入器
namespace rj = rapidjson;  // RapidJSON命名空间别名

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 消息队列日志回调函数
 * @param id 消息队列服务器ID
 * @param message 日志消息
 * @param bServer 是否为服务器日志
 * 
 * 消息队列模块的日志回调函数，当前实现为空。
 */
void on_mq_log(unsigned long id, const char* message, bool bServer)
{

}

/**
 * @brief 构造函数
 * 
 * 创建事件通知器实例，初始化成员变量。
 */
EventNotifier::EventNotifier()
	: _mq_sid(0)  // 消息队列服务器ID初始化为0
	, _publisher(NULL)  // 发布消息函数指针初始化为NULL
	, _stopped(false)  // 停止标志初始化为false
{
	
}


/**
 * @brief 析构函数
 * 
 * 清理事件通知器占用的资源，停止工作线程，释放消息队列服务器。
 */
EventNotifier::~EventNotifier()
{
	_stopped = true;  // 设置停止标志
	if (_worker)  // 如果工作线程存在
		_worker->join();  // 等待工作线程结束

	_asyncio.stop();  // 停止异步IO服务

	if (_remover && _mq_sid != 0)  // 如果销毁函数存在且消息队列服务器ID不为0
		_remover(_mq_sid);  // 销毁消息队列服务器
}

/**
 * @brief 初始化事件通知器
 * @param cfg 配置参数，包含消息队列URL和模块路径
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置中加载消息队列模块，创建消息队列服务器，启动异步处理线程。
 * 配置参数：
 * - active: 是否启用事件通知
 * - url: 消息队列URL地址
 */
bool EventNotifier::init(WTSVariant* cfg)
{
	if (!cfg->getBoolean("active"))  // 如果事件通知未启用
		return false;  // 返回false

	_url = cfg->getCString("url");  // 获取消息队列URL
	std::string module = DLLHelper::wrap_module("WtMsgQue", "lib");  // 包装消息队列模块名称
	//先看工作目录下是否有对应模块
	std::string dllpath = WtHelper::getCWD() + module;  // 工作目录下的模块路径
	//如果没有,则再看模块目录,即dll同目录下
	if (!StdFile::exists(dllpath.c_str()))  // 如果工作目录下不存在
		dllpath = WtHelper::getInstDir() + module;  // 使用实例目录下的模块路径

	DllHandle dllInst = DLLHelper::load_library(dllpath.c_str());  // 加载动态库
	if (dllInst == NULL)  // 如果加载失败
	{
		WTSLogger::error("MQ module {} loading failed", dllpath.c_str());  // 记录错误日志
		return false;  // 返回false
	}

	_creator = (FuncCreateMQServer)DLLHelper::get_symbol(dllInst, "create_server");  // 获取创建服务器函数
	if (_creator == NULL)  // 如果获取失败
	{
		DLLHelper::free_library(dllInst);  // 释放动态库
		WTSLogger::error("MQ module {} is not compatible", dllpath.c_str());  // 记录错误日志
		return false;  // 返回false
	}

	_remover = (FuncDestroyMQServer)DLLHelper::get_symbol(dllInst, "destroy_server");  // 获取销毁服务器函数
	_publisher = (FundPublishMessage)DLLHelper::get_symbol(dllInst, "publish_message");  // 获取发布消息函数
	_register = (FuncRegCallbacks)DLLHelper::get_symbol(dllInst, "regiter_callbacks");  // 获取注册回调函数

	//注册回调函数
	_register(on_mq_log);  // 注册日志回调函数
	
	//创建一个MQServer
	_mq_sid = _creator(_url.c_str());  // 创建消息队列服务器

	WTSLogger::info("EventNotifier initialized with channel {}", _url.c_str());  // 记录信息日志

	if (_worker == NULL)  // 如果工作线程未创建
	{
		boost::asio::io_service::work work(_asyncio);  // 创建工作对象，防止IO服务在没有任务时退出
		_worker.reset(new StdThread([this]() {  // 创建工作线程
			while (!_stopped)  // 循环直到停止标志为true
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(2));  // 休眠2毫秒
				_asyncio.run_one();  // 执行一个异步任务
				//m_asyncIO.run();
			}
		}));
	}

	return true;  // 返回成功
}

/**
 * @brief 通知日志事件
 * @param tag 日志标签
 * @param message 日志消息
 * 
 * 异步广播日志事件，将日志信息转换为JSON格式后发布到"LOG"主题。
 */
void EventNotifier::notify_log(const char* tag, const char* message)
{
	if (_mq_sid == 0)  // 如果消息队列服务器未初始化
		return;  // 直接返回

	std::string strTag = tag;  // 保存日志标签
	std::string strMsg = message;  // 保存日志消息
	_asyncio.post([this, strTag, strMsg]() {  // 提交异步任务
		std::string data;  // JSON数据字符串
		{
			rj::Document root(rj::kObjectType);  // 创建JSON根对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

			root.AddMember("tag", rj::Value(strTag.c_str(), allocator), allocator);  // 添加日志标签
			root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加时间戳
			root.AddMember("message", rj::Value(strMsg.c_str(), allocator), allocator);  // 添加日志消息

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON对象写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}

		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "LOG", data.c_str(), (unsigned long)data.size());  // 发布日志消息到"LOG"主题
	});
}

/**
 * @brief 通知通用事件
 * @param message 事件消息
 * 
 * 异步广播通用事件，将事件信息转换为JSON格式后发布到"GRP_EVENT"主题。
 */
void EventNotifier::notify_event(const char* message)
{
	if (_mq_sid == 0)  // 如果消息队列服务器未初始化
		return;  // 直接返回

	std::string strMsg = message;  // 保存事件消息
	_asyncio.post([this, strMsg]() {  // 提交异步任务
		std::string data;  // JSON数据字符串
		{
			rj::Document root(rj::kObjectType);  // 创建JSON根对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

			root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加时间戳
			root.AddMember("message", rj::Value(strMsg.c_str(), allocator), allocator);  // 添加事件消息

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON对象写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "GRP_EVENT", data.c_str(), (unsigned long)data.size());  // 发布事件消息到"GRP_EVENT"主题
	});
}

/**
 * @brief 通知交易消息
 * @param trader 交易接口名称
 * @param message 消息内容
 * 
 * 异步广播交易消息，将消息转换为JSON格式后发布到"TRD_NOTIFY"主题。
 */
void EventNotifier::notify(const char* trader, const char* message)
{
	if (_mq_sid == 0)  // 如果消息队列服务器未初始化
		return;  // 直接返回

	std::string strTrader = trader;  // 保存交易接口名称
	std::string strMsg = message;  // 保存消息内容
	_asyncio.post([this, strTrader, strMsg]() {  // 提交异步任务
		std::string data;  // JSON数据字符串
		{
			rj::Document root(rj::kObjectType);  // 创建JSON根对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

			root.AddMember("trader", rj::Value(strTrader.c_str(), allocator), allocator);  // 添加交易接口名称
			root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加时间戳
			root.AddMember("message", rj::Value(strMsg.c_str(), allocator), allocator);  // 添加消息内容

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON对象写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "TRD_NOTIFY", data.c_str(), (unsigned long)data.size());  // 发布交易消息到"TRD_NOTIFY"主题
	});
}

/**
 * @brief 通知成交事件
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param trdInfo 成交信息指针
 * 
 * 异步广播成交事件，将成交信息转换为JSON格式后发布到"TRD_TRADE"主题。
 */
void EventNotifier::notify(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo)
{
	if (trdInfo == NULL || _mq_sid == 0)  // 如果成交信息为空或消息队列服务器未初始化
		return;  // 直接返回

	std::string strTrader = trader;  // 保存交易接口名称
	std::string strCode = stdCode;  // 保存合约代码
	trdInfo->retain();  // 增加成交信息引用计数
	_asyncio.post([this, strTrader, strCode, localid, trdInfo]() {  // 提交异步任务
		std::string data;  // JSON数据字符串
		tradeToJson(strTrader.c_str(), localid, strCode.c_str(), trdInfo, data);  // 将成交信息转换为JSON
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "TRD_TRADE", data.c_str(), (unsigned long)data.size());  // 发布成交消息到"TRD_TRADE"主题
		trdInfo->release();  // 减少成交信息引用计数
	});
}

/**
 * @brief 通知订单事件
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param ordInfo 订单信息指针
 * 
 * 异步广播订单事件，将订单信息转换为JSON格式后发布到"TRD_ORDER"主题。
 */
void EventNotifier::notify(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo)
{
	if (ordInfo == NULL || _mq_sid == 0)  // 如果订单信息为空或消息队列服务器未初始化
		return;  // 直接返回

	std::string strTrader = trader;  // 保存交易接口名称
	std::string strCode = stdCode;  // 保存合约代码
	ordInfo->retain();  // 增加订单信息引用计数
	_asyncio.post([this, strTrader, strCode, localid, ordInfo]() {  // 提交异步任务
		std::string data;  // JSON数据字符串
		orderToJson(strTrader.c_str(), localid, strCode.c_str(), ordInfo, data);  // 将订单信息转换为JSON
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "TRD_ORDER", data.c_str(), (unsigned long)data.size());  // 发布订单消息到"TRD_ORDER"主题
	});
}

/**
 * @brief 将成交信息转换为JSON格式
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param trdInfo 成交信息指针
 * @param output 输出的JSON字符串
 * 
 * 将成交信息转换为JSON格式字符串，包含交易接口、时间、本地订单ID、合约代码、方向、开平标志、数量、价格等信息。
 */
void EventNotifier::tradeToJson(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo, std::string& output)
{
	if(trdInfo == NULL)  // 如果成交信息为空
	{
		output = "{}";  // 输出空JSON对象
		return;  // 直接返回
	}

	bool isLong = (trdInfo->getDirection() == WDT_LONG);  // 判断是否做多
	bool isOpen = (trdInfo->getOffsetType() == WOT_OPEN);  // 判断是否开仓
	bool isToday = (trdInfo->getOffsetType() == WOT_CLOSETODAY);  // 判断是否平今

	{
		rj::Document root(rj::kObjectType);  // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		root.AddMember("trader", rj::Value(trader, allocator), allocator);  // 添加交易接口名称
		root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加时间戳
		root.AddMember("localid", localid, allocator);  // 添加本地订单ID
		root.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码
		root.AddMember("islong", isLong, allocator);  // 添加是否做多标志
		root.AddMember("isopen", isOpen, allocator);  // 添加是否开仓标志
		root.AddMember("istoday", isToday, allocator);  // 添加是否平今标志

		root.AddMember("volume", trdInfo->getVolume(), allocator);  // 添加成交数量
		root.AddMember("price", trdInfo->getPrice(), allocator);  // 添加成交价格

		rj::StringBuffer sb;  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);  // 将JSON对象写入缓冲区

		output = sb.GetString();  // 获取JSON字符串
	}
}

/**
 * @brief 将订单信息转换为JSON格式
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param ordInfo 订单信息指针
 * @param output 输出的JSON字符串
 * 
 * 将订单信息转换为JSON格式字符串，包含交易接口、时间、本地订单ID、合约代码、方向、开平标志、总数量、剩余数量、已成交数量、价格、状态等信息。
 */
void EventNotifier::orderToJson(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo, std::string& output)
{
	if (ordInfo == NULL)  // 如果订单信息为空
	{
		output = "{}";  // 输出空JSON对象
		return;  // 直接返回
	}

	bool isLong = (ordInfo->getDirection() == WDT_LONG);  // 判断是否做多
	bool isOpen = (ordInfo->getOffsetType() == WOT_OPEN);  // 判断是否开仓
	bool isToday = (ordInfo->getOffsetType() == WOT_CLOSETODAY);  // 判断是否平今
	bool isCanceled = (ordInfo->getOrderState() == WOS_Canceled);  // 判断是否已撤销

	{
		rj::Document root(rj::kObjectType);  // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器

		root.AddMember("trader", rj::Value(trader, allocator), allocator);  // 添加交易接口名称
		root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加时间戳
		root.AddMember("localid", localid, allocator);  // 添加本地订单ID
		root.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加合约代码
		root.AddMember("islong", isLong, allocator);  // 添加是否做多标志
		root.AddMember("isopen", isOpen, allocator);  // 添加是否开仓标志
		root.AddMember("istoday", isToday, allocator);  // 添加是否平今标志
		root.AddMember("canceled", isCanceled, allocator);  // 添加是否已撤销标志

		root.AddMember("total", ordInfo->getVolume(), allocator);  // 添加总委托数量
		root.AddMember("left", ordInfo->getVolLeft(), allocator);  // 添加剩余数量
		root.AddMember("traded", ordInfo->getVolTraded(), allocator);  // 添加已成交数量
		root.AddMember("price", ordInfo->getPrice(), allocator);  // 添加委托价格
		root.AddMember("state", rj::Value(ordInfo->getStateMsg(), allocator), allocator);  // 添加订单状态消息

		rj::StringBuffer sb;  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);  // 将JSON对象写入缓冲区

		output = sb.GetString();  // 获取JSON字符串
	}
}
