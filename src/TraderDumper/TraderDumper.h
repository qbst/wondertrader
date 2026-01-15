/*!
 * \file TraderDumper.h
 * \project	WonderTrader
 *
 * \brief 交易数据转储模块的C语言导出接口
 * 
 * 本文件定义了TraderDumper模块对外提供的C语言接口函数。
 * 这些函数可以被外部程序（如Python、C#等）通过DLL/so动态库方式调用。
 * 
 * 设计说明：
 * - 使用extern "C"确保C++代码可以被C语言调用
 * - 所有函数都使用EXPORT_FLAG导出标志，确保跨平台兼容性
 * - 提供完整的生命周期管理：注册回调->初始化->配置->运行->释放
 * 
 * 使用流程：
 * 1. register_callbacks - 注册数据回调函数
 * 2. init - 初始化日志系统
 * 3. config - 加载配置文件，初始化交易通道
 * 4. run - 启动数据转储，开始查询和推送数据
 * 5. release - 释放资源，清理连接
 */
#pragma once

#include "PorterDefs.h"  // 包含回调函数类型定义

#ifdef __cplusplus          // 如果是C++编译环境
extern "C"                  // 使用C语言链接约定，确保函数名不被C++名称修饰（name mangling）影响
{
#endif
	/**
	 * @brief 注册数据回调函数
	 * @param cbAccount 账户资金信息回调函数指针，当账户数据更新时调用
	 * @param cbOrder 订单信息回调函数指针，当订单数据更新时调用
	 * @param cbTrade 成交信息回调函数指针，当成交数据更新时调用
	 * @param cbPosition 持仓信息回调函数指针，当持仓数据更新时调用
	 * 
	 * 注册四个回调函数，用于接收交易数据。
	 * 回调函数可以为NULL，表示不需要接收该类型的数据。
	 * 必须在init之前调用。
	 */
	EXPORT_FLAG	void		register_callbacks(FuncOnAccount cbAccount, FuncOnOrder  cbOrder, FuncOnTrade cbTrade, FuncOnPosition cbPosition);

	/**
	 * @brief 初始化日志系统
	 * @param logProfile 日志配置文件路径（字符串），指定日志的配置文件名
	 * 
	 * 初始化WonderTrader的日志系统，加载日志配置。
	 * 必须在config之前调用。
	 * logProfile可以是相对路径或绝对路径。
	 */
	EXPORT_FLAG	void		init(const char* logProfile);

	/**
	 * @brief 加载配置文件并初始化交易通道
	 * @param cfgfile 配置文件路径或配置内容（字符串）
	 * @param isFile 是否为文件路径（布尔值），true表示cfgfile是文件路径，false表示cfgfile是配置内容字符串
	 * @return 返回配置是否成功（布尔值），true表示成功，false表示失败
	 * 
	 * 根据isFile参数决定是从文件加载配置还是从字符串内容加载配置。
	 * 配置文件格式为YAML，包含：
	 * - basefiles: 基础数据文件路径（交易时段、品种、合约等）
	 * - traders: 交易通道配置列表（每个通道的账号、密码、模块等）
	 * - config: 其他配置项（如刷新间隔等）
	 * 
	 * 配置成功后，会根据配置创建并初始化所有活跃的交易通道适配器。
	 */
	EXPORT_FLAG	bool		config(const char* cfgfile, bool isFile);

	/**
	 * @brief 启动数据转储
	 * @param bOnce 是否只运行一次（布尔值），true表示查询一次后退出，false表示持续运行并定时刷新
	 * 
	 * 启动数据转储流程：
	 * - 连接所有交易通道
	 * - 登录交易账号
	 * - 查询账户、持仓、订单、成交数据
	 * - 如果bOnce为false，会启动后台线程定时刷新数据
	 * 
	 * 如果bOnce为true，函数会阻塞直到所有数据查询完成。
	 * 如果bOnce为false，函数会立即返回，后台线程会持续运行。
	 */
	EXPORT_FLAG	void		run(bool bOnce);

	/**
	 * @brief 释放资源并清理连接
	 * 
	 * 释放所有资源：
	 * - 停止后台刷新线程（如果存在）
	 * - 断开所有交易通道连接
	 * - 释放交易接口资源
	 * - 清理内存
	 * 
	 * 应该在程序退出前调用，确保资源正确释放。
	 */
	EXPORT_FLAG	void		release();

#ifdef __cplusplus          // C++编译环境结束
}                           // extern "C"作用域结束
#endif