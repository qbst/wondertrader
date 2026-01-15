/*!
 * \file PorterDefs.h
 * \project	WonderTrader
 *
 * \brief 消息队列模块的回调函数类型定义
 * 
 * 本文件定义了WtMsgQue模块对外提供的所有回调函数类型。
 * 这些回调函数用于消息队列的消息接收和日志记录。
 * 所有回调函数都使用PORTER_FLAG导出标志，确保跨DLL/so调用的兼容性。
 * 
 * 设计说明：
 * - 使用函数指针类型定义，支持C语言接口
 * - FuncMQCallback用于接收订阅的消息
 * - FuncLogCallback用于接收服务器和客户端的日志信息
 */
#pragma once
#include "../Includes/WTSMarcos.h"  // 包含WonderTrader宏定义，包含PORTER_FLAG、WtUInt32等

/**
 * @typedef FuncMQCallback
 * @brief 消息队列消息回调函数类型
 * 
 * 当客户端接收到订阅的消息时，会调用此回调函数。
 * 回调函数会将消息的主题和数据传递给外部调用者。
 * 
 * @param id 客户端ID（32位无符号整数），标识是哪个客户端接收到的消息
 * @param topic 消息主题（字符串），标识消息的类型或分类
 * @param data 消息数据（字节数组指针），消息的实际内容
 * @param dataLen 消息数据长度（32位无符号整数），data数组的字节数
 */
typedef void(PORTER_FLAG *FuncMQCallback)(WtUInt32 id, const char* topic, const char* data, WtUInt32 dataLen);

/**
 * @typedef FuncLogCallback
 * @brief 消息队列日志回调函数类型
 * 
 * 当服务器或客户端产生日志信息时，会调用此回调函数。
 * 回调函数会将日志信息传递给外部调用者。
 * 
 * @param id 服务器或客户端ID（32位无符号整数），标识是哪个实例产生的日志
 * @param message 日志消息（字符串），日志的具体内容
 * @param bServer 是否为服务器（布尔值），true表示是服务器日志，false表示是客户端日志
 */
typedef void(PORTER_FLAG *FuncLogCallback)(WtUInt32 id, const char* message, bool bServer);