/**
 * @file atp_trade_api.h
 * @brief ATP交易API主头文件
 * 
 * 本文件是ATP（Advanced Trading Platform）交易API的主头文件，负责统一包含
 * 所有ATP交易系统相关的核心组件和接口定义。
 * 
 * 设计逻辑：
 * - 采用模块化设计，将不同功能分散到各个专门的头文件中
 * - 通过统一的主头文件简化用户的包含操作
 * - 确保所有必需的类型定义、常量、消息结构、处理器和客户端接口都被正确引入
 * 
 * 包含的核心模块：
 * 1. atp_trade_types.h - 基础数据类型定义
 * 2. atp_trade_constants.h - 常量和枚举定义
 * 3. atp_trade_msg.h - 消息结构定义
 * 4. atp_trade_handler.h - 事件处理器接口
 * 5. atp_trade_client.h - 交易客户端接口
 * 
 * 使用方式：
 * 用户只需包含此头文件即可获得完整的ATP交易API功能
 * 
 * @author ATP开发团队
 * @version 1.0
 * @date 2023
 */

#ifndef ATP_TRADE_API_H_                    // 防止头文件重复包含的预处理保护
#define ATP_TRADE_API_H_                    // 定义头文件保护宏

#include <trade/atp_trade_types.h>          // 包含ATP交易系统基础数据类型定义
#include <trade/atp_trade_constants.h>      // 包含ATP交易系统常量和枚举定义
#include <trade/atp_trade_msg.h>            // 包含ATP交易系统消息结构定义
#include <trade/atp_trade_handler.h>        // 包含ATP交易系统事件处理器接口
#include <trade/atp_trade_client.h>         // 包含ATP交易系统客户端接口

#endif                                      // 结束头文件保护
