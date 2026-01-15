/*!
 * \file TraderDumper.cpp
 * \project	WonderTrader
 *
 * \brief TraderDumper模块的C语言接口实现
 * 
 * 本文件实现了TraderDumper.h中声明的所有C语言导出函数。
 * 这些函数作为外部接口，内部调用Dumper类的功能。
 * 
 * 设计说明：
 * - 使用单例模式，通过getDumper()获取全局唯一的Dumper实例
 * - 所有C接口函数都是对Dumper类方法的简单封装
 * - 提供统一的模块目录路径（通过getBinDir()获取）
 * 
 * 实现逻辑：
 * 1. getDumper() - 获取全局Dumper单例实例
 * 2. register_callbacks - 将回调函数注册到Dumper
 * 3. init - 初始化Dumper的日志系统
 * 4. config - 配置Dumper，传入模块目录路径
 * 5. run - 启动Dumper运行
 * 6. release - 释放Dumper资源
 */
#include "TraderDumper.h"         // 包含C接口声明
#include "Dumper.h"                // 包含Dumper类定义

#include "../Share/ModuleHelper.hpp"  // 包含模块路径辅助工具，提供getBinDir()函数

/**
 * @brief 获取全局Dumper单例实例
 * @return 返回Dumper类的引用
 * 
 * 使用静态局部变量实现单例模式，确保全局只有一个Dumper实例。
 * 静态局部变量在首次调用时初始化，后续调用直接返回已存在的实例。
 * 线程安全（C++11标准保证静态局部变量初始化的线程安全性）。
 */
Dumper& getDumper()
{
	static Dumper dumper;          // 静态局部变量：全局唯一的Dumper实例，首次调用时初始化
	return dumper;                 // 返回Dumper实例的引用
}

/**
 * @brief 注册数据回调函数的C接口实现
 * @param cbAccount 账户资金信息回调函数指针
 * @param cbOrder 订单信息回调函数指针
 * @param cbTrade 成交信息回调函数指针
 * @param cbPosition 持仓信息回调函数指针
 * 
 * 将外部传入的四个回调函数注册到Dumper实例中。
 * 当交易数据更新时，Dumper会调用这些回调函数通知外部。
 */
void register_callbacks(FuncOnAccount cbAccount, FuncOnOrder  cbOrder, FuncOnTrade cbTrade, FuncOnPosition cbPosition)
{
	getDumper().register_callbacks(cbAccount, cbOrder, cbTrade, cbPosition);  // 调用Dumper的注册回调方法
}

/**
 * @brief 初始化日志系统的C接口实现
 * @param logProfile 日志配置文件路径
 * 
 * 初始化Dumper的日志系统，加载日志配置。
 * 必须在config之前调用。
 */
void init(const char* logProfile)
{
	getDumper().init(logProfile);  // 调用Dumper的初始化方法，传入日志配置文件名
}

/**
 * @brief 加载配置文件的C接口实现
 * @param cfgfile 配置文件路径或配置内容
 * @param isFile 是否为文件路径
 * @return 返回配置是否成功
 * 
 * 配置Dumper，加载交易通道配置和基础数据文件。
 * 同时传入模块目录路径，用于定位交易模块DLL/so文件。
 */
bool config(const char* cfgfile, bool isFile)
{
	return getDumper().config(cfgfile, isFile, getBinDir());  // 调用Dumper的配置方法，传入配置文件和模块目录路径
}

/**
 * @brief 启动数据转储的C接口实现
 * @param bOnce 是否只运行一次
 * 
 * 启动Dumper运行，开始连接交易通道并查询数据。
 * 如果bOnce为true，会阻塞直到所有数据查询完成。
 */
void run(bool bOnce)
{
	getDumper().run(bOnce);        // 调用Dumper的运行方法
}

/**
 * @brief 释放资源的C接口实现
 * 
 * 释放Dumper的所有资源，断开交易连接，停止后台线程。
 * 应该在程序退出前调用。
 */
void release()
{
	getDumper().release();         // 调用Dumper的释放方法
}