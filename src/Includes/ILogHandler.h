/*!
 * \file ILogHandler.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 日志转发模块接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中日志转发模块的核心接口，负责处理系统日志的转发和分发。
 * 主要功能包括：
 * 1. 定义日志处理器接口，提供统一的日志处理入口
 * 2. 支持不同日志级别的日志消息处理
 * 3. 为日志系统提供可扩展的插件化架构
 * 4. 支持多种日志输出目标的集成
 * 
 * 该类主要用于WonderTrader框架中的日志管理系统，为各个模块提供统一的日志处理接口。
 * 通过抽象接口设计，支持多种日志处理方式和不同输出目标的扩展。
 */
#pragma once  // 防止头文件重复包含
#include "WTSMarcos.h"  // 包含WonderTrader宏定义
#include "WTSTypes.h"  // 包含WTS类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * @class ILogHandler
 * @brief 日志处理器接口类
 * 
 * 该类定义了WonderTrader框架中日志处理器的核心接口，负责处理系统日志的转发和分发。
 * 提供统一的日志处理入口，支持不同日志级别的日志消息处理。
 * 
 * 主要特性：
 * - 统一的日志处理接口设计
 * - 支持多种日志级别的处理
 * - 可扩展的插件化架构
 * - 支持多种日志输出目标的集成
 * - 简洁高效的接口设计
 */
class ILogHandler
{
public:
	/**
	 * @brief 处理日志追加事件
	 * @param ll 日志级别，定义日志的重要程度
	 * @param msg 日志消息内容，包含具体的日志信息
	 * 
	 * 该函数处理日志追加事件，当系统产生新的日志时会被调用。
	 * 根据日志级别和消息内容进行相应的处理，如输出到控制台、写入文件、发送到远程服务器等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual void handleLogAppend(WTSLogLevel ll, const char* msg)	= 0;  // 纯虚函数：处理日志追加事件
};

NS_WTP_END  // 结束WonderTrader命名空间