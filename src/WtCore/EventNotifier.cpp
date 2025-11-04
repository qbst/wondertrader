/*!
 * \file EventCaster.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 事件通知器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了事件通知器的所有功能，包括：
 * 1. 事件通知器的初始化：加载MQ模块，创建MQ服务器，启动异步IO工作线程
 * 2. 各种事件的异步通知：交易事件、订单事件、日志事件、图表事件等
 * 3. 数据格式转换：将交易数据、订单数据转换为JSON格式
 * 4. 异步事件处理：使用boost::asio实现异步事件处理，避免阻塞主线程
 * 
 * 关键实现细节：
 * - 使用动态库加载机制加载MQ模块
 * - 使用rapidjson进行JSON格式转换
 * - 使用boost::asio实现异步IO处理
 * - 使用lambda表达式实现异步任务封装
 */
#include "EventNotifier.h"  // 包含事件通知器头文件
#include "WtHelper.h"  // 包含WonderTrader辅助工具类

#include "../Share/TimeUtils.hpp"  // 包含时间工具类
#include "../Share/DLLHelper.hpp"  // 包含动态库加载辅助工具

#include "../Includes/WTSTradeDef.hpp"  // 包含交易定义头文件
#include "../Includes/WTSCollection.hpp"  // 包含集合类头文件
#include "../Includes/WTSVariant.hpp"  // 包含变体类型头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

#include <rapidjson/document.h>  // 包含rapidjson文档类
#include <rapidjson/prettywriter.h>  // 包含rapidjson格式化写入器
#include <rapidjson/writer.h>  // 包含rapidjson写入器
namespace rj = rapidjson;  // 定义rapidjson命名空间别名

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief MQ日志回调函数
 * @param id 消息队列ID
 * @param message 日志消息内容
 * @param bServer 是否为服务器端日志
 * 
 * 消息队列模块的日志回调函数，当前为空实现。
 * 可以在此处添加日志处理逻辑。
 */
void on_mq_log(unsigned long id, const char* message, bool bServer)
{  // MQ日志回调函数实现，当前为空
}

/**
 * @brief 构造函数实现
 * 
 * 初始化事件通知器对象，设置初始状态：
 * - MQ服务器ID初始化为0
 * - 发布函数指针初始化为NULL
 * - 停止标志初始化为false
 */
EventNotifier::EventNotifier()
	: _mq_sid(0)  // 初始化MQ服务器ID为0
	, _publisher(NULL)  // 初始化发布函数指针为NULL
	, _stopped(false)  // 初始化停止标志为false
{  // 构造函数实现体
}

/**
 * @brief 析构函数实现
 * 
 * 清理事件通知器对象：
 * 1. 设置停止标志为true，通知工作线程退出
 * 2. 等待工作线程结束
 * 3. 停止异步IO服务
 * 4. 释放MQ服务器资源
 */
EventNotifier::~EventNotifier()
{
	_stopped = true;  // 设置停止标志为true，通知工作线程退出
	if (_worker)  // 如果工作线程存在
		_worker->join();  // 等待工作线程结束

	_asyncio.stop();  // 停止异步IO服务

	if (_remover && _mq_sid != 0)  // 如果删除函数存在且MQ服务器ID有效
		_remover(_mq_sid);  // 调用删除函数释放MQ服务器资源
}

/**
 * @brief 初始化事件通知器实现
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
bool EventNotifier::init(WTSVariant* cfg)
{
	if (!cfg->getBoolean("active"))  // 如果配置中未启用事件通知
		return false;  // 返回false，不进行初始化

	_url = cfg->getCString("url");  // 从配置中获取MQ服务器地址/URL
	std::string module = DLLHelper::wrap_module("WtMsgQue", "lib");  // 包装MQ模块名称（添加平台相关的扩展名）
	//先看工作目录下是否有对应模块
	std::string dllpath = WtHelper::getCWD() + module;  // 构造模块路径：工作目录 + 模块名
	//如果没有,则再看模块目录,即dll同目录下
	if (!StdFile::exists(dllpath.c_str()))  // 如果工作目录下不存在模块文件
		dllpath = WtHelper::getInstDir() + module;  // 使用安装目录构造模块路径

	DllHandle dllInst = DLLHelper::load_library(dllpath.c_str());  // 加载MQ模块动态库
	if (dllInst == NULL)  // 如果加载失败
	{
		WTSLogger::error("MQ module {} loading failed", dllpath.c_str());  // 记录错误日志
		return false;  // 返回false表示初始化失败
	}

	_creator = (FuncCreateMQServer)DLLHelper::get_symbol(dllInst, "create_server");  // 获取创建MQ服务器的函数指针
	if (_creator == NULL)  // 如果获取创建函数失败
	{
		DLLHelper::free_library(dllInst);  // 释放已加载的库
		WTSLogger::error("MQ module {} is not compatible", dllpath.c_str());  // 记录错误日志：模块不兼容
		return false;  // 返回false表示初始化失败
	}

	_remover = (FuncDestroyMQServer)DLLHelper::get_symbol(dllInst, "destroy_server");  // 获取销毁MQ服务器的函数指针
	_publisher = (FundPublishMessage)DLLHelper::get_symbol(dllInst, "publish_message");  // 获取发布消息的函数指针
	_register = (FuncRegCallbacks)DLLHelper::get_symbol(dllInst, "regiter_callbacks");  // 获取注册回调的函数指针

	//注册回调函数
	_register(on_mq_log);  // 注册MQ日志回调函数
	
	//创建一个MQServer
	_mq_sid = _creator(_url.c_str());  // 调用创建函数创建MQ服务器实例，获取服务器ID

	WTSLogger::info("EventNotifier initialized with channel {}", _url.c_str());  // 记录信息日志：初始化成功

	if (_worker == NULL)  // 如果工作线程未创建
	{
		boost::asio::io_service::work work(_asyncio);  // 创建work对象，防止io_service在没有任务时退出
		_worker.reset(new StdThread([this]() {  // 创建工作线程，使用lambda表达式定义线程函数
			while (!_stopped)  // 循环执行，直到停止标志为true
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(2));  // 睡眠2毫秒，避免CPU占用过高
				_asyncio.run_one();  // 运行一个异步IO任务
				//m_asyncIO.run();  // 注释掉的代码：运行所有异步IO任务（可能导致阻塞）
			}
		}));  // 创建工作线程结束
	}

	return true;  // 返回true表示初始化成功
}

/**
 * @brief 通知日志事件实现
 * @param tag 日志标签
 * @param message 日志消息内容
 * 
 * 将日志信息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"LOG"。
 */
void EventNotifier::notify_log(const char* tag, const char* message)
{
	if (_mq_sid == 0)  // 如果MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string strTag = tag;  // 复制日志标签字符串
	std::string strMsg = message;  // 复制日志消息字符串
	_asyncio.post([this, strTag, strMsg]() {  // 将任务投递到异步IO队列，使用lambda表达式捕获变量
		std::string data;  // 定义JSON数据字符串
		{  // 开始JSON构建作用域
			rj::Document root(rj::kObjectType);  // 创建JSON文档对象（对象类型）
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

			root.AddMember("tag", rj::Value(strTag.c_str(), allocator), allocator);  // 添加"tag"字段
			root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加"time"字段（当前时间）
			root.AddMember("message", rj::Value(strMsg.c_str(), allocator), allocator);  // 添加"message"字段

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}  // JSON构建作用域结束

		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "LOG", data.c_str(), (unsigned long)data.size());  // 发布消息到"LOG"主题
	});  // lambda表达式结束
}

/**
 * @brief 通知通用事件实现
 * @param message 事件消息内容
 * 
 * 将通用事件消息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"GRP_EVENT"。
 */
void EventNotifier::notify_event(const char* message)
{
	if (_mq_sid == 0)  // 如果MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string strMsg = message;  // 复制事件消息字符串
	_asyncio.post([this, strMsg]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		{  // 开始JSON构建作用域
			rj::Document root(rj::kObjectType);  // 创建JSON文档对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

			root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加"time"字段
			root.AddMember("message", rj::Value(strMsg.c_str(), allocator), allocator);  // 添加"message"字段

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}  // JSON构建作用域结束
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "GRP_EVENT", data.c_str(), (unsigned long)data.size());  // 发布消息到"GRP_EVENT"主题
	});  // lambda表达式结束
}

/**
 * @brief 通知交易接口消息实现
 * @param trader 交易接口名称
 * @param message 消息内容
 * 
 * 将交易接口消息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"TRD_NOTIFY"。
 */
void EventNotifier::notify(const char* trader, const char* message)
{
	if (_mq_sid == 0)  // 如果MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string strTrader = trader;  // 复制交易接口名称字符串
	std::string strMsg = message;  // 复制消息字符串
	_asyncio.post([this, strTrader, strMsg]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		{  // 开始JSON构建作用域
			rj::Document root(rj::kObjectType);  // 创建JSON文档对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

			root.AddMember("trader", rj::Value(strTrader.c_str(), allocator), allocator);  // 添加"trader"字段
			root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加"time"字段
			root.AddMember("message", rj::Value(strMsg.c_str(), allocator), allocator);  // 添加"message"字段

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}  // JSON构建作用域结束
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "TRD_NOTIFY", data.c_str(), (unsigned long)data.size());  // 发布消息到"TRD_NOTIFY"主题
	});  // lambda表达式结束
}

/**
 * @brief 通知交易事件实现
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param trdInfo 交易信息对象指针
 * 
 * 将交易信息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"TRD_TRADE"。
 */
void EventNotifier::notify(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo)
{
	if (trdInfo == NULL || _mq_sid == 0)  // 如果交易信息为空或MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string strTrader = trader;  // 复制交易接口名称字符串
	std::string strCode = stdCode;  // 复制合约代码字符串
	trdInfo->retain();  // 增加交易信息对象的引用计数，防止在异步处理过程中被释放
	_asyncio.post([this, strTrader, strCode, localid, trdInfo]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		tradeToJson(strTrader.c_str(), localid, strCode.c_str(), trdInfo, data);  // 调用函数将交易信息转换为JSON
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "TRD_TRADE", data.c_str(), (unsigned long)data.size());  // 发布消息到"TRD_TRADE"主题
		trdInfo->release();  // 减少交易信息对象的引用计数，释放引用
	});  // lambda表达式结束
}

/**
 * @brief 通知订单事件实现
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param ordInfo 订单信息对象指针
 * 
 * 将订单信息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"TRD_ORDER"。
 */
void EventNotifier::notify(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo)
{
	if (ordInfo == NULL || _mq_sid == 0)  // 如果订单信息为空或MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string strTrader = trader;  // 复制交易接口名称字符串
	std::string strCode = stdCode;  // 复制合约代码字符串
	ordInfo->retain();  // 增加订单信息对象的引用计数，防止在异步处理过程中被释放
	_asyncio.post([this, strTrader, strCode, localid, ordInfo]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		orderToJson(strTrader.c_str(), localid, strCode.c_str(), ordInfo, data);  // 调用函数将订单信息转换为JSON
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "TRD_ORDER", data.c_str(), (unsigned long)data.size());  // 发布消息到"TRD_ORDER"主题
	});  // lambda表达式结束，注意：此处未释放ordInfo引用，可能存在内存泄漏风险

	
}

/**
 * @brief 将交易信息转换为JSON格式实现
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param trdInfo 交易信息对象指针
 * @param output 输出JSON字符串的引用
 * 
 * 将交易信息对象转换为JSON格式字符串，用于消息队列传输。
 * JSON格式包含：trader、time、localid、code、islong、isopen、istoday、volume、price等字段。
 */
void EventNotifier::tradeToJson(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo, std::string& output)
{
	if(trdInfo == NULL)  // 如果交易信息为空
	{
		output = "{}";  // 输出空JSON对象
		return;  // 直接返回
	}

	bool isLong = (trdInfo->getDirection() == WDT_LONG);  // 判断是否为做多方向
	bool isOpen = (trdInfo->getOffsetType() == WOT_OPEN);  // 判断是否为开仓
	bool isToday = (trdInfo->getOffsetType() == WOT_CLOSETODAY);  // 判断是否为今仓平仓

	{  // 开始JSON构建作用域
		rj::Document root(rj::kObjectType);  // 创建JSON文档对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

		root.AddMember("trader", rj::Value(trader, allocator), allocator);  // 添加"trader"字段
		root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加"time"字段（当前时间）
		root.AddMember("localid", localid, allocator);  // 添加"localid"字段（本地订单ID）
		root.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加"code"字段（合约代码）
		root.AddMember("islong", isLong, allocator);  // 添加"islong"字段（是否做多）
		root.AddMember("isopen", isOpen, allocator);  // 添加"isopen"字段（是否开仓）
		root.AddMember("istoday", isToday, allocator);  // 添加"istoday"字段（是否今仓）

		root.AddMember("volume", trdInfo->getVolume(), allocator);  // 添加"volume"字段（成交量）
		root.AddMember("price", trdInfo->getPrice(), allocator);  // 添加"price"字段（成交价格）

		rj::StringBuffer sb;  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);  // 将JSON文档写入缓冲区

		output = sb.GetString();  // 获取JSON字符串并赋值给输出参数
	}  // JSON构建作用域结束
}

/**
 * @brief 将订单信息转换为JSON格式实现
 * @param trader 交易接口名称
 * @param localid 本地订单ID
 * @param stdCode 标准合约代码
 * @param ordInfo 订单信息对象指针
 * @param output 输出JSON字符串的引用
 * 
 * 将订单信息对象转换为JSON格式字符串，用于消息队列传输。
 * JSON格式包含：trader、time、localid、code、islong、isopen、istoday、canceled、total、left、traded、price、state等字段。
 */
void EventNotifier::orderToJson(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo, std::string& output)
{
	if (ordInfo == NULL)  // 如果订单信息为空
	{
		output = "{}";  // 输出空JSON对象
		return;  // 直接返回
	}

	bool isLong = (ordInfo->getDirection() == WDT_LONG);  // 判断是否为做多方向
	bool isOpen = (ordInfo->getOffsetType() == WOT_OPEN);  // 判断是否为开仓
	bool isToday = (ordInfo->getOffsetType() == WOT_CLOSETODAY);  // 判断是否为今仓平仓
	bool isCanceled = (ordInfo->getOrderState() == WOS_Canceled);  // 判断订单是否已撤销

	{  // 开始JSON构建作用域
		rj::Document root(rj::kObjectType);  // 创建JSON文档对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

		root.AddMember("trader", rj::Value(trader, allocator), allocator);  // 添加"trader"字段
		root.AddMember("time", TimeUtils::getLocalTimeNow(), allocator);  // 添加"time"字段（当前时间）
		root.AddMember("localid", localid, allocator);  // 添加"localid"字段（本地订单ID）
		root.AddMember("code", rj::Value(stdCode, allocator), allocator);  // 添加"code"字段（合约代码）
		root.AddMember("islong", isLong, allocator);  // 添加"islong"字段（是否做多）
		root.AddMember("isopen", isOpen, allocator);  // 添加"isopen"字段（是否开仓）
		root.AddMember("istoday", isToday, allocator);  // 添加"istoday"字段（是否今仓）
		root.AddMember("canceled", isCanceled, allocator);  // 添加"canceled"字段（是否已撤销）

		root.AddMember("total", ordInfo->getVolume(), allocator);  // 添加"total"字段（订单总量）
		root.AddMember("left", ordInfo->getVolLeft(), allocator);  // 添加"left"字段（剩余数量）
		root.AddMember("traded", ordInfo->getVolTraded(), allocator);  // 添加"traded"字段（已成交数量）
		root.AddMember("price", ordInfo->getPrice(), allocator);  // 添加"price"字段（订单价格）
		root.AddMember("state", rj::Value(ordInfo->getStateMsg(), allocator), allocator);  // 添加"state"字段（订单状态消息）

		rj::StringBuffer sb;  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);  // 将JSON文档写入缓冲区

		output = sb.GetString();  // 获取JSON字符串并赋值给输出参数
	}  // JSON构建作用域结束
}

/**
 * @brief 通知图表指标事件实现
 * @param time 时间戳
 * @param straId 策略ID
 * @param idxName 指标名称
 * @param lineName 线条名称
 * @param val 指标值
 * 
 * 将图表指标数据转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"CHART_INDEX"。
 */
void EventNotifier::notify_chart_index(uint64_t time, const char* straId, const char* idxName, const char* lineName, double val)
{
	if (_mq_sid == 0)  // 如果MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string sid = straId;  // 复制策略ID字符串
	std::string iname = idxName;  // 复制指标名称字符串
	std::string lname = lineName;  // 复制线条名称字符串
	_asyncio.post([this, time, sid, iname, lname, val]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		{  // 开始JSON构建作用域
			rj::Document root(rj::kObjectType);  // 创建JSON文档对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

			root.AddMember("strategy", rj::Value(sid.c_str(), allocator), allocator);  // 添加"strategy"字段
			root.AddMember("index_name", rj::Value(iname.c_str(), allocator), allocator);  // 添加"index_name"字段
			root.AddMember("line_name", rj::Value(lname.c_str(), allocator), allocator);  // 添加"line_name"字段
			root.AddMember("time", time, allocator);  // 添加"time"字段（时间戳）
			root.AddMember("value", val, allocator);  // 添加"value"字段（指标值）

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
			root.Accept(writer);  // 将JSON文档写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}  // JSON构建作用域结束
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "CHART_INDEX", data.c_str(), (unsigned long)data.size());  // 发布消息到"CHART_INDEX"主题
	});  // lambda表达式结束
}

/**
 * @brief 通知图表标记事件实现
 * @param time 时间戳
 * @param straId 策略ID
 * @param price 价格
 * @param icon 图标名称
 * @param tag 标记标签
 * 
 * 将图表标记信息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"CHART_MARKER"。
 */
void EventNotifier::notify_chart_marker(uint64_t time, const char* straId, double price, const char* icon, const char* tag)
{
	if (_mq_sid == 0)  // 如果MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string sid = straId;  // 复制策略ID字符串
	std::string sIcon = icon;  // 复制图标名称字符串
	std::string sTag = tag;  // 复制标记标签字符串
	_asyncio.post([this, time, sid, sIcon, sTag, price]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		{  // 开始JSON构建作用域
			rj::Document root(rj::kObjectType);  // 创建JSON文档对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

			root.AddMember("strategy", rj::Value(sid.c_str(), allocator), allocator);  // 添加"strategy"字段
			root.AddMember("icon", rj::Value(sIcon.c_str(), allocator), allocator);  // 添加"icon"字段
			root.AddMember("tag", rj::Value(sTag.c_str(), allocator), allocator);  // 添加"tag"字段
			root.AddMember("time", time, allocator);  // 添加"time"字段（时间戳）
			root.AddMember("price", price, allocator);  // 添加"price"字段（价格）

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::Writer<rj::StringBuffer> writer(sb);  // 创建写入器（不使用格式化写入器，节省空间）
			root.Accept(writer);  // 将JSON文档写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}  // JSON构建作用域结束
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "CHART_MARKER", data.c_str(), (unsigned long)data.size());  // 发布消息到"CHART_MARKER"主题
	});  // lambda表达式结束
}

/**
 * @brief 通知策略交易事件实现
 * @param straId 策略ID
 * @param stdCode 标准合约代码
 * @param isLong 是否做多
 * @param isOpen 是否开仓
 * @param curTime 当前时间戳
 * @param price 成交价格
 * @param userTag 用户标签
 * 
 * 将策略交易信息转换为JSON格式并通过消息队列异步广播。
 * 消息主题为"STRA_TRADE"。
 */
void EventNotifier::notify_trade(const char* straId, const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, const char* userTag)
{
	if (_mq_sid == 0)  // 如果MQ服务器未初始化
		return;  // 直接返回，不进行处理

	std::string sid = straId;  // 复制策略ID字符串
	std::string code = stdCode;  // 复制合约代码字符串
	std::string tag = userTag;  // 复制用户标签字符串
	_asyncio.post([this, sid, code, tag, isLong, isOpen, curTime, price]() {  // 将任务投递到异步IO队列
		std::string data;  // 定义JSON数据字符串
		{  // 开始JSON构建作用域
			rj::Document root(rj::kObjectType);  // 创建JSON文档对象
			rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取JSON分配器引用

			root.AddMember("strategy", rj::Value(sid.c_str(), allocator), allocator);  // 添加"strategy"字段
			root.AddMember("code", rj::Value(code.c_str(), allocator), allocator);  // 添加"code"字段
			root.AddMember("tag", rj::Value(tag.c_str(), allocator), allocator);  // 添加"tag"字段
			root.AddMember("long", isLong, allocator);  // 添加"long"字段（是否做多）
			root.AddMember("open", isOpen, allocator);  // 添加"open"字段（是否开仓）
			root.AddMember("time", curTime, allocator);  // 添加"time"字段（时间戳）
			root.AddMember("price", price, allocator);  // 添加"price"字段（成交价格）

			rj::StringBuffer sb;  // 创建字符串缓冲区
			rj::Writer<rj::StringBuffer> writer(sb);  // 创建写入器（不使用格式化写入器，节省空间）
			root.Accept(writer);  // 将JSON文档写入缓冲区

			data = sb.GetString();  // 获取JSON字符串
		}  // JSON构建作用域结束
		if (_publisher)  // 如果发布函数存在
			_publisher(_mq_sid, "STRA_TRADE", data.c_str(), (unsigned long)data.size());  // 发布消息到"STRA_TRADE"主题
	});  // lambda表达式结束
}
