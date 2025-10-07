/*!
 * \file WtDtPorter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtDtPorter模块对外接口头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDtPorter模块对外提供的C语言接口，是WonderTrader数据服务模块的对外API。
 * 
 * 主要功能包括：
 * 1. 提供数据服务的初始化和启动接口
 * 2. 提供版本信息查询接口
 * 3. 提供日志输出接口
 * 4. 提供扩展Parser（行情解析器）的创建和管理接口
 * 5. 提供扩展Dumper（数据转储器）的创建和管理接口
 * 6. 定义扩展Parser的回调函数注册接口
 * 7. 定义扩展Dumper的回调函数注册接口
 * 8. 提供行情数据推送接口
 * 
 * 设计思想：
 * - 使用C语言接口，便于跨语言调用（Python、C#等）
 * - 采用extern "C"包装，避免C++名称修饰问题
 * - 使用函数指针类型定义回调机制，实现模块间解耦
 * - 提供清晰的接口文档，包括参数说明和使用场景
 * - 支持扩展机制，允许外部自定义Parser和Dumper
 * 
 * 该文件是WtDtPorter模块的核心接口定义，所有外部调用都通过此文件定义的接口进行。
 * 主要用于Python、C#等语言通过ctypes或P/Invoke方式调用WonderTrader数据服务功能。
 */
#pragma once  // 防止头文件重复包含
#include "PorterDefs.h"  // 包含Porter模块类型定义


#ifdef __cplusplus
extern "C"  // 使用C语言链接方式，避免C++名称修饰
{
#endif

	//////////////////////////////////////////////////////////////////////////
	// 基础接口
	//////////////////////////////////////////////////////////////////////////

	/**
	 * @brief 初始化数据服务
	 * @param cfgFile 配置文件路径或配置内容字符串
	 * @param logCfg 日志配置文件路径或配置内容字符串
	 * @param bCfgFile cfgFile是否为文件路径，true表示文件路径，false表示配置内容字符串
	 * @param bLogCfgFile logCfg是否为文件路径，true表示文件路径，false表示配置内容字符串
	 * 
	 * 该函数初始化WtDtPorter数据服务，加载配置文件和日志配置。
	 * 配置文件包含数据源、存储、解析器等模块的配置信息。
	 * 必须在调用其他接口之前先调用此函数进行初始化。
	 * 
	 * 使用场景：在程序启动时调用，完成数据服务的初始化工作。
	 */
	EXPORT_FLAG void		initialize(WtString cfgFile, WtString logCfg, bool bCfgFile, bool bLogCfgFile);

	/**
	 * @brief 启动数据服务
	 * @param bAsync 是否异步启动，true表示异步启动（立即返回），false表示同步启动（阻塞直到退出）
	 * 
	 * 该函数启动数据服务，开始运行行情解析器和数据管理器。
	 * 如果bAsync为false，函数会阻塞当前线程，直到接收到退出信号；
	 * 如果bAsync为true，函数会立即返回，数据服务在后台运行。
	 * 
	 * 使用场景：在完成初始化后调用，启动数据服务的运行。
	 */
	EXPORT_FLAG void		start(bool bAsync = false);

	/**
	 * @brief 获取版本信息
	 * @return WtString 版本信息字符串，包含平台、版本号、编译日期等
	 * 
	 * 该函数返回WtDtPorter模块的版本信息。
	 * 版本信息包括平台类型（X64/X86/UNIX）、版本号、编译日期和时间。
	 * 
	 * 使用场景：用于调试和日志记录，确认使用的模块版本。
	 */
	EXPORT_FLAG	WtString	get_version();

	/**
	 * @brief 输出日志
	 * @param level 日志级别，对应WTSLogLevel枚举值
	 * @param message 日志内容
	 * @param catName 日志分类名称，用于区分不同模块的日志，可为空字符串
	 * 
	 * 该函数输出日志到系统日志系统。
	 * 支持按分类和级别记录日志，便于日志的过滤和管理。
	 * 
	 * 使用场景：外部模块需要输出日志时调用，统一日志输出格式和管理。
	 */
	EXPORT_FLAG	void		write_log(unsigned int level, const char* message, const char* catName);


#pragma region "扩展Parser接口"
	/**
	 * @brief 创建扩展行情解析器
	 * @param id 解析器唯一标识符，用于区分不同的解析器实例
	 * @return bool 创建成功返回true，失败返回false
	 * 
	 * 该函数创建一个扩展行情解析器实例。
	 * 扩展解析器用于接入自定义的行情数据源，将外部行情推送到系统中。
	 * 创建成功后，需要注册相应的回调函数来处理解析器事件和订阅请求。
	 * 
	 * 使用场景：当需要接入自定义行情数据源时，先调用此函数创建解析器，
	 *          然后注册回调函数，最后通过parser_push_quote推送行情数据。
	 */
	EXPORT_FLAG	bool		create_ext_parser(const char* id);

	/**
	 * @brief 注册扩展Parser的回调函数
	 * @param cbEvt 行情解析器事件回调函数，处理连接、断开、初始化、释放等事件
	 * @param cbSub 行情订阅回调函数，处理订阅和退订请求
	 * 
	 * 该函数注册扩展Parser的回调函数。
	 * cbEvt回调函数处理解析器的事件，如连接、断开、初始化、释放等；
	 * cbSub回调函数处理订阅和退订请求，外部模块在此回调中向数据源发送订阅请求。
	 * 
	 * 使用场景：在创建扩展Parser后调用，注册回调函数来处理解析器事件和订阅请求。
	 *          通常在程序初始化阶段，create_ext_parser之前或之后调用。
	 */
	EXPORT_FLAG void		register_parser_callbacks(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub);

	/**
	 * @brief 向底层推送tick数据
	 * @param id 解析器ID，标识数据来源
	 * @param curTick 最新tick数据指针
	 * @param uProcFlag 处理标记：
	 *                  0 - 切片行情，无需处理（ParserUDP）
	 *                  1 - 完整快照，需要切片（国内各路通道，如CTP、XTP等）
	 *                  2 - 极简快照，需要缓存累加（主要针对日线、tick，m1和m5都是自动累加的，虚拟货币行情）
	 * 
	 * 该函数将外部接收到的tick行情数据推送到系统中。
	 * 系统会根据uProcFlag进行相应的处理，如切片、累加等。
	 * 推送的数据会被分发到订阅了该合约的策略和数据处理模块。
	 * 
	 * 使用场景：外部数据源接收到行情数据后，通过此函数推送到系统中。
	 *          不同的数据源类型使用不同的uProcFlag值。
	 */
	EXPORT_FLAG	void		parser_push_quote(const char* id, WTSTickStruct* curTick, WtUInt32 uProcFlag);
#pragma endregion "扩展Parser接口"

#pragma region "扩展Dumper接口"
	/**
	 * @brief 创建扩展数据转储器
	 * @param id 转储器唯一标识符，用于区分不同的转储器实例
	 * @return bool 创建成功返回true，失败返回false
	 * 
	 * 该函数创建一个扩展数据转储器实例。
	 * 扩展转储器用于将历史数据导出到自定义存储系统（如数据库、文件、云存储）。
	 * 创建成功后，需要注册相应的回调函数来处理数据转储操作。
	 * 
	 * 使用场景：当需要自定义数据存储方式时，先调用此函数创建转储器，
	 *          然后注册回调函数来实现自定义的存储逻辑。
	 */
	EXPORT_FLAG	bool		create_ext_dumper(const char* id);

	/**
	 * @brief 注册扩展Dumper的回调函数（K线和Tick）
	 * @param barDumper K线数据转储回调函数
	 * @param tickDumper Tick数据转储回调函数
	 * 
	 * 该函数注册扩展Dumper的基础数据转储回调函数。
	 * barDumper回调函数处理K线数据的转储；
	 * tickDumper回调函数处理Tick数据的转储。
	 * 
	 * 使用场景：在创建扩展Dumper后调用，注册K线和Tick数据的转储回调函数。
	 *          外部模块在回调函数中实现自定义的数据存储逻辑。
	 */
	EXPORT_FLAG void		register_extended_dumper(FuncDumpBars barDumper, FuncDumpTicks tickDumper);

	/**
	 * @brief 注册扩展Dumper的回调函数（高频数据）
	 * @param ordQueDumper 委托队列数据转储回调函数
	 * @param ordDtlDumper 委托明细数据转储回调函数
	 * @param transDumper 逐笔成交数据转储回调函数
	 * 
	 * 该函数注册扩展Dumper的高频数据转储回调函数。
	 * ordQueDumper回调函数处理委托队列数据的转储（Level2行情）；
	 * ordDtlDumper回调函数处理委托明细数据的转储（Level2行情）；
	 * transDumper回调函数处理逐笔成交数据的转储。
	 * 
	 * 使用场景：在创建扩展Dumper后调用，注册Level2高频数据的转储回调函数。
	 *          主要用于股票Level2行情数据的存储。
	 */
	EXPORT_FLAG void		register_extended_hftdata_dumper(FuncDumpOrdQue ordQueDumper, FuncDumpOrdDtl ordDtlDumper, FuncDumpTrans transDumper);
#pragma endregion "扩展Dumper接口"

#ifdef __cplusplus
}  // 结束extern "C"
#endif
