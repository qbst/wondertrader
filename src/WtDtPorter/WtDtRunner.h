/*!
 * \file WtDtRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据服务运行器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDtRunner类，该类是WonderTrader数据服务模块的核心运行器。
 * 
 * 主要功能包括：
 * 1. 管理数据服务的初始化、启动和运行
 * 2. 管理行情解析器（Parser）的创建、配置和运行
 * 3. 管理数据转储器（Dumper）的创建、配置和回调
 * 4. 管理基础数据（合约、交易所、交易时段等）的加载和维护
 * 5. 管理主力合约规则和次主力合约规则的加载
 * 6. 管理数据写入器（DataWriter）的初始化和运行
 * 7. 管理状态监控器（StateMonitor）的初始化和运行
 * 8. 管理数据广播器（UDPCaster、ShmCaster）的初始化和运行
 * 9. 管理指数工厂（IndexFactory）的初始化和运行
 * 10. 提供扩展Parser和扩展Dumper的创建和管理接口
 * 11. 处理扩展Parser的事件和订阅回调
 * 12. 处理扩展Dumper的数据转储回调
 * 13. 支持异步IO操作，提高系统性能
 * 14. 支持全天候模式（allday mode），无需状态监控
 * 
 * 设计思想：
 * - 单例模式：通过getRunner()函数获取全局唯一实例
 * - 模块化设计：将不同功能分离到不同的管理器中（DataManager、ParserAdapter、StateMonitor等）
 * - 回调机制：通过函数指针实现扩展Parser和Dumper的回调，支持外部自定义逻辑
 * - 异步IO：使用boost::asio实现异步IO操作，提高并发性能
 * - 配置驱动：通过配置文件控制各模块的行为，提高灵活性
 * - 生命周期管理：统一管理所有组件的初始化、启动、停止和释放
 * 
 * 该类是WtDtPorter模块的核心，协调所有组件的运行，确保数据服务的正常运行。
 */
#pragma once  // 防止头文件重复包含
#include "PorterDefs.h"  // 包含Porter模块类型定义
#include "ExpDumper.h"  // 包含扩展转储器类声明

#include "../WtDtCore/DataManager.h"  // 包含数据管理器
#include "../WtDtCore/ParserAdapter.h"  // 包含行情解析器适配器
#include "../WtDtCore/StateMonitor.h"  // 包含状态监控器
#include "../WtDtCore/UDPCaster.h"  // 包含UDP广播器
#include "../WtDtCore/IndexFactory.h"  // 包含指数工厂
#include "../WtDtCore/ShmCaster.h"  // 包含共享内存广播器

#include "../WTSTools/WTSHotMgr.h"  // 包含主力合约管理器
#include "../WTSTools/WTSBaseDataMgr.h"  // 包含基础数据管理器

#include <boost/asio.hpp>  // 包含Boost异步IO库

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：WTS变体类型类
NS_WTP_END  // 结束WonderTrader命名空间

/**
 * @class WtDtRunner
 * @brief 数据服务运行器类
 * 
 * WtDtRunner是WonderTrader数据服务模块的核心运行器类，负责协调和管理所有数据服务组件。
 * 该类采用单例模式，通过getRunner()函数获取全局唯一实例。
 * 
 * 主要特性：
 * - 统一管理所有数据服务组件的生命周期
 * - 支持多种行情解析器的并发运行
 * - 支持扩展Parser和Dumper，实现自定义数据源和存储
 * - 支持多种数据广播方式（UDP、共享内存）
 * - 支持状态监控，实现交易时段的自动管理
 * - 支持指数计算，实现自定义指数的计算和发布
 * - 支持异步IO操作，提高系统性能
 * - 支持全天候模式，适用于7x24交易市场
 * 
 * 使用场景：
 * 作为数据服务的核心引擎，负责接收行情数据、处理数据、存储数据、广播数据等。
 * 主要用于实盘行情接入、数据中心建设、行情转发服务等场景。
 */
class WtDtRunner
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化WtDtRunner对象，设置所有回调函数指针为NULL，设置退出标志为false。
	 */
	WtDtRunner();
	
	/**
	 * @brief 析构函数
	 * 
	 * 释放WtDtRunner对象占用的资源。
	 */
	~WtDtRunner();

public:
	/**
	 * @brief 初始化数据服务
	 * @param cfgFile 配置文件路径或配置内容字符串
	 * @param logCfg 日志配置文件路径或配置内容字符串
	 * @param modDir 模块目录路径，默认为空字符串（使用当前目录）
	 * @param bCfgFile cfgFile是否为文件路径，默认为true
	 * @param bLogCfgFile logCfg是否为文件路径，默认为true
	 * 
	 * 该函数初始化数据服务，加载配置文件和日志配置。
	 * 配置文件包含数据源、存储、解析器等模块的配置信息。
	 * 初始化完成后，系统进入就绪状态，可以调用start()启动运行。
	 */
	void	initialize(const char* cfgFile, const char* logCfg, const char* modDir = "", bool bCfgFile = true, bool bLogCfgFile = true);
	
	/**
	 * @brief 启动数据服务
	 * @param bAsync 是否异步启动，默认为false（同步启动）
	 * @param bAlldayMode 是否全天候模式，默认为false（普通模式）
	 * 
	 * 该函数启动数据服务，开始运行行情解析器和数据管理器。
	 * 如果bAsync为false，函数会阻塞当前线程，直到接收到退出信号；
	 * 如果bAsync为true，函数会立即返回，数据服务在后台运行。
	 * 如果bAlldayMode为true，不启动状态监控器，适用于7x24交易市场。
	 */
	void	start(bool bAsync = false, bool bAlldayMode = false);

	/**
	 * @brief 创建扩展行情解析器
	 * @param id 解析器唯一标识符
	 * @return bool 创建成功返回true，失败返回false
	 * 
	 * 该函数创建一个扩展行情解析器实例。
	 * 扩展解析器用于接入自定义的行情数据源，将外部行情推送到系统中。
	 */
	bool	createExtParser(const char* id);


//////////////////////////////////////////////////////////////////////////
// 扩展Parser接口
//////////////////////////////////////////////////////////////////////////
public:
	/**
	 * @brief 注册扩展Parser的回调函数
	 * @param cbEvt 行情解析器事件回调函数
	 * @param cbSub 行情订阅回调函数
	 * 
	 * 该函数注册扩展Parser的回调函数，用于处理解析器事件和订阅请求。
	 */
	void registerParserPorter(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub);

	/**
	 * @brief Parser初始化事件处理
	 * @param id 解析器ID
	 * 
	 * 该函数处理Parser的初始化事件，触发外部注册的初始化回调函数。
	 */
	void parser_init(const char* id);
	
	/**
	 * @brief Parser连接事件处理
	 * @param id 解析器ID
	 * 
	 * 该函数处理Parser的连接事件，触发外部注册的连接回调函数。
	 */
	void parser_connect(const char* id);
	
	/**
	 * @brief Parser释放事件处理
	 * @param id 解析器ID
	 * 
	 * 该函数处理Parser的释放事件，触发外部注册的释放回调函数。
	 */
	void parser_release(const char* id);
	
	/**
	 * @brief Parser断开连接事件处理
	 * @param id 解析器ID
	 * 
	 * 该函数处理Parser的断开连接事件，触发外部注册的断开连接回调函数。
	 */
	void parser_disconnect(const char* id);
	
	/**
	 * @brief Parser订阅处理
	 * @param id 解析器ID
	 * @param code 合约代码
	 * 
	 * 该函数处理Parser的订阅请求，触发外部注册的订阅回调函数。
	 */
	void parser_subscribe(const char* id, const char* code);
	
	/**
	 * @brief Parser退订处理
	 * @param id 解析器ID
	 * @param code 合约代码
	 * 
	 * 该函数处理Parser的退订请求，触发外部注册的退订回调函数。
	 */
	void parser_unsubscribe(const char* id, const char* code);

	/**
	 * @brief 处理扩展Parser推送的行情数据
	 * @param id 解析器ID
	 * @param curTick Tick行情数据指针
	 * @param uProcFlag 处理标记
	 * 
	 * 该函数接收扩展Parser推送的行情数据，转发给数据管理器进行处理。
	 */
	void on_ext_parser_quote(const char* id, WTSTickStruct* curTick, uint32_t uProcFlag);

//////////////////////////////////////////////////////////////////////////
// 扩展Dumper接口
//////////////////////////////////////////////////////////////////////////
public:
	/**
	 * @brief 创建扩展数据转储器
	 * @param id 转储器唯一标识符
	 * @return bool 创建成功返回true，失败返回false
	 * 
	 * 该函数创建一个扩展数据转储器实例。
	 */
	bool createExtDumper(const char* id);

	/**
	 * @brief 注册扩展Dumper的回调函数（K线和Tick）
	 * @param barDumper K线数据转储回调函数
	 * @param tickDumper Tick数据转储回调函数
	 * 
	 * 该函数注册扩展Dumper的基础数据转储回调函数。
	 */
	void registerExtDumper(FuncDumpBars barDumper, FuncDumpTicks tickDumper);

	/**
	 * @brief 注册扩展Dumper的回调函数（高频数据）
	 * @param ordQueDumper 委托队列数据转储回调函数
	 * @param ordDtlDumper 委托明细数据转储回调函数
	 * @param transDumper 逐笔成交数据转储回调函数
	 * 
	 * 该函数注册扩展Dumper的高频数据转储回调函数。
	 */
	void registerExtHftDataDumper(FuncDumpOrdQue ordQueDumper, FuncDumpOrdDtl ordDtlDumper, FuncDumpTrans transDumper);

	/**
	 * @brief 转储历史K线数据
	 * @param id 转储器ID
	 * @param stdCode 标准合约代码
	 * @param period K线周期
	 * @param bars K线数据数组指针
	 * @param count K线数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史K线数据转储到外部存储系统。
	 */
	bool dumpHisBars(const char* id, const char* stdCode, const char* period, WTSBarStruct* bars, uint32_t count);

	/**
	 * @brief 转储历史Tick数据
	 * @param id 转储器ID
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param ticks Tick数据数组指针
	 * @param count Tick数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史Tick数据转储到外部存储系统。
	 */
	bool dumpHisTicks(const char* id, const char* stdCode, uint32_t uDate, WTSTickStruct* ticks, uint32_t count);

	/**
	 * @brief 转储历史委托队列数据
	 * @param id 转储器ID
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param item 委托队列数据数组指针
	 * @param count 委托队列数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史委托队列数据转储到外部存储系统。
	 */
	bool dumpHisOrdQue(const char* id, const char* stdCode, uint32_t uDate, WTSOrdQueStruct* item, uint32_t count);

	/**
	 * @brief 转储历史委托明细数据
	 * @param id 转储器ID
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param items 委托明细数据数组指针
	 * @param count 委托明细数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史委托明细数据转储到外部存储系统。
	 */
	bool dumpHisOrdDtl(const char* id, const char* stdCode, uint32_t uDate, WTSOrdDtlStruct* items, uint32_t count);

	/**
	 * @brief 转储历史逐笔成交数据
	 * @param id 转储器ID
	 * @param stdCode 标准合约代码
	 * @param uDate 交易日期
	 * @param items 逐笔成交数据数组指针
	 * @param count 逐笔成交数据条数
	 * @return bool 转储成功返回true，失败返回false
	 * 
	 * 该函数将历史逐笔成交数据转储到外部存储系统。
	 */
	bool dumpHisTrans(const char* id, const char* stdCode, uint32_t uDate, WTSTransStruct* items, uint32_t count);

private:
	/**
	 * @brief 初始化数据管理器
	 * @param config 配置参数
	 * @param bAlldayMode 是否全天候模式，默认为false
	 * 
	 * 该函数初始化数据管理器，设置数据写入器和状态监控器。
	 */
	void initDataMgr(WTSVariant* config, bool bAlldayMode = false);
	
	/**
	 * @brief 初始化行情解析器
	 * @param cfg 配置参数
	 * 
	 * 该函数初始化行情解析器，加载解析器配置并创建解析器实例。
	 */
	void initParsers(WTSVariant* cfg);

private:
	WTSBaseDataMgr	_bd_mgr;        // 基础数据管理器：管理合约、交易所、交易时段等基础信息
	WTSHotMgr		_hot_mgr;       // 主力合约管理器：管理主力合约规则和次主力合约规则
	boost::asio::io_service _async_io;  // Boost异步IO服务：用于异步IO操作
	StateMonitor	_state_mon;     // 状态监控器：监控交易时段状态，管理数据存储的打开和关闭
	UDPCaster		_udp_caster;    // UDP广播器：通过UDP协议广播行情数据
	ShmCaster		_shm_caster;    // 共享内存广播器：通过共享内存广播行情数据
	DataManager		_data_mgr;      // 数据管理器：管理行情数据的接收、处理、存储和分发
	IndexFactory	_idx_factory;   // 指数工厂：管理指数的计算和发布
	ParserAdapterMgr	_parsers;   // 行情解析器管理器：管理所有行情解析器的运行

	FuncParserEvtCallback	_cb_parser_evt;  // 扩展Parser事件回调函数指针
	FuncParserSubCallback	_cb_parser_sub;  // 扩展Parser订阅回调函数指针

	FuncDumpBars	_dumper_for_bars;    // K线数据转储回调函数指针
	FuncDumpTicks	_dumper_for_ticks;   // Tick数据转储回调函数指针

	FuncDumpOrdQue	_dumper_for_ordque;  // 委托队列数据转储回调函数指针
	FuncDumpOrdDtl	_dumper_for_orddtl;  // 委托明细数据转储回调函数指针
	FuncDumpTrans	_dumper_for_trans;   // 逐笔成交数据转储回调函数指针

	typedef std::shared_ptr<ExpDumper> ExpDumperPtr;  // 扩展转储器智能指针类型
	typedef std::map<std::string, ExpDumperPtr>  ExpDumpers;  // 扩展转储器映射表类型
	ExpDumpers		_dumpers;  // 扩展转储器映射表：管理所有扩展转储器实例

	bool _to_exit;  // 退出标志：true表示需要退出，false表示继续运行
};

