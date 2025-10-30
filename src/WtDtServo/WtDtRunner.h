/*!
 * \file WtDtRunner.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据服务运行器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDtRunner（数据服务运行器）类，是WonderTrader数据伺服器模块的核心
 * 协调类，负责管理整个数据服务的生命周期，包括初始化、数据查询、实时数据处理、
 * 订阅管理等。该类整合了数据管理器、解析器适配器、基础数据管理器等组件，提供
 * 统一的数据服务接口。
 * 
 * 核心设计理念：
 * 
 * 1. 统一协调（Unified Coordination）：
 *    - 整合数据管理器、解析器适配器等组件
 *    - 提供统一的数据服务接口
 *    - 协调各组件之间的交互
 * 
 * 2. 订阅管理（Subscription Management）：
 *    - 管理Tick数据订阅（外部订阅和内部订阅）
 *    - 管理K线数据订阅
 *    - 支持订阅替换和追加模式
 * 
 * 3. 事件处理（Event Handling）：
 *    - 处理Tick数据推送
 *    - 处理K线数据推送
 *    - 触发用户注册的回调函数
 * 
 * 4. 数据查询（Data Query）：
 *    - 提供多种数据查询接口
 *    - 将查询请求转发给数据管理器
 *    - 支持按日期、范围、数量查询
 * 
 * 主要功能模块：
 * 
 * 1. 初始化和启动：
 *    - initialize()：初始化数据服务
 *    - start()：启动数据服务
 *    - initDataMgr()：初始化数据管理器
 *    - initParsers()：初始化解析器
 * 
 * 2. 数据查询：
 *    - K线数据查询（按日期、范围、数量、秒级）
 *    - Tick数据查询（按日期、范围、数量）
 * 
 * 3. 实时数据处理：
 *    - proc_tick()：处理Tick数据
 *    - trigger_tick()：触发Tick回调
 *    - trigger_bar()：触发K线回调
 * 
 * 4. 订阅管理：
 *    - sub_tick()：订阅Tick数据
 *    - sub_bar()：订阅K线数据
 *    - clear_cache()：清理缓存
 * 
 * 使用场景：
 * - 数据服务的统一入口
 * - 实时行情数据处理
 * - 历史数据查询
 * - 订阅管理
 * 
 * 技术特点：
 * - 组件化设计
 * - 统一的接口抽象
 * - 灵活的订阅机制
 * - 高效的数据查询
 * 
 * 注意事项：
 * - 必须先调用initialize()进行初始化
 * - 订阅管理使用互斥锁保护
 * - 数据查询结果需要调用release()释放
 * - 回调函数应该快速返回
 */
#pragma once                                                                     // 防止头文件重复包含
#include "../WTSTools/WTSHotMgr.h"                                               // 包含主力合约管理器
#include "../WTSTools/WTSBaseDataMgr.h"                                          // 包含基础数据管理器
#include "../Share/StdUtils.hpp"                                                 // 包含标准工具类

#include "PorterDefs.h"                                                          // 包含Porter模块类型定义（回调函数类型）
#include "ParserAdapter.h"                                                       // 包含解析器适配器头文件
#include "WtDataManager.h"                                                       // 包含数据管理器头文件

NS_WTP_BEGIN                                                                    // WonderTrader命名空间开始
class WTSVariant;                                                                // 配置变体类前向声明
class WtDataStorage;                                                            // 数据存储类前向声明
class WTSKlineSlice;                                                             // K线数据切片类前向声明
class WTSTickSlice;                                                              // Tick数据切片类前向声明
NS_WTP_END                                                                      // WonderTrader命名空间结束

/**
 * @class WtDtRunner
 * @brief 数据服务运行器类
 * 
 * 负责管理整个数据服务的生命周期，协调各组件的工作。
 * 提供统一的数据服务接口，包括数据查询、实时数据处理、订阅管理等。
 */
class WtDtRunner
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化数据服务运行器对象。
	 */
	WtDtRunner();

	/**
	 * @brief 析构函数
	 * 
	 * 清理数据服务运行器资源。
	 */
	~WtDtRunner();

public:
	/**
	 * @brief 初始化数据服务
	 * @param cfgFile 配置文件路径或配置内容（取决于isFile参数）
	 * @param isFile 是否为配置文件路径（true=文件路径，false=配置内容字符串）
	 * @param modDir 模块目录路径（可选）
	 * @param logCfg 日志配置文件路径（可选，默认为"logcfg.yaml"）
	 * @param cbTick 实时Tick数据回调函数（可选）
	 * @param cbBar 实时K线数据回调函数（可选）
	 * 
	 * 初始化数据服务运行器，加载配置文件、初始化日志系统、设置各组件等。
	 * 这是使用数据服务功能的第一步，必须在使用其他功能前调用。
	 */
	void	initialize(const char* cfgFile, bool isFile = true, const char* modDir = "", const char* logCfg = "logcfg.yaml", 
				FuncOnTickCallback cbTick = NULL, FuncOnBarCallback cbBar = NULL);

	/**
	 * @brief 启动数据服务
	 * 
	 * 启动数据服务，包括启动所有解析器适配器。
	 * 调用后，解析器开始接收行情数据。
	 */
	void	start();

	/**
	 * @brief 获取基础数据管理器引用
	 * @return 基础数据管理器引用
	 * 
	 * 返回基础数据管理器，用于获取合约信息、品种信息等基础数据。
	 */
	inline WTSBaseDataMgr& getBaseDataMgr() { return _bd_mgr; }

	/**
	 * @brief 获取主力合约管理器引用
	 * @return 主力合约管理器引用
	 * 
	 * 返回主力合约管理器，用于处理主力合约切换逻辑。
	 */
	inline WTSHotMgr& getHotMgr() { return _hot_mgr; }

public:
	// ===== 实时数据处理接口 =====

	/**
	 * @brief 处理Tick数据
	 * @param curTick 当前Tick数据指针
	 * 
	 * 处理接收到的Tick数据，更新实时K线并触发回调。
	 * 这是解析器适配器调用的事件入口。
	 */
	void	proc_tick(WTSTickData* curTick);

	/**
	 * @brief 触发Tick数据回调
	 * @param stdCode 标准化合约代码
	 * @param curTick 当前Tick数据指针
	 * 
	 * 触发Tick数据回调函数，通知订阅者。
	 * 只触发已订阅的合约的Tick数据。
	 */
	void	trigger_tick(const char* stdCode, WTSTickData* curTick);

	/**
	 * @brief 触发K线数据回调
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m1"、"m5"、"d"等）
	 * @param lastBar 最新K线数据指针
	 * 
	 * 触发K线数据回调函数，通知订阅者。
	 * 只触发已订阅的合约和周期的K线数据。
	 */
	void	trigger_bar(const char* stdCode, const char* period, WTSBarStruct* lastBar);

	/**
	 * @brief 订阅Tick数据
	 * @param stdCode 标准化合约代码
	 * @param bReplace 是否替换现有的订阅列表（true=替换，false=追加）
	 * @param bInner 是否为内部订阅（true=内部订阅，false=外部订阅）
	 * 
	 * 订阅指定合约的Tick数据。
	 * 支持外部订阅和内部订阅两种模式。
	 */
	void	sub_tick(const char* stdCode, bool bReplace, bool bInner = false);

	/**
	 * @brief 订阅K线数据
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m1"、"m5"、"d"等）
	 * 
	 * 订阅指定合约和周期的K线数据。
	 * 订阅后，当有新K线生成时，会触发回调。
	 */
	void	sub_bar(const char* stdCode, const char* period);

	/**
	 * @brief 清理数据缓存
	 * 
	 * 清理所有已缓存的数据，包括数据管理器的缓存。
	 */
	void	clear_cache();

public:
	// ===== 数据查询接口 =====

	/**
	 * @brief 按时间范围查询K线数据
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m1"、"m5"、"d"等）
	 * @param beginTime 开始时间（格式：yyyymmddHHMM或yyyymmdd，取决于周期）
	 * @param endTime 结束时间（格式：yyyymmddHHMM或yyyymmdd，取决于周期）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice*	get_bars_by_range(const char* stdCode, const char* period, uint64_t beginTime, uint64_t endTime = 0);

	/**
	 * @brief 按日期查询K线数据
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m1"、"m5"、"d"等）
	 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice*	get_bars_by_date(const char* stdCode, const char* period, uint32_t uDate = 0);

	/**
	 * @brief 按时间范围查询Tick数据
	 * @param stdCode 标准化合约代码
	 * @param beginTime 开始时间（格式：yyyymmddHHMMSS）
	 * @param endTime 结束时间（格式：yyyymmddHHMMSS，0表示不限制）
	 * @return Tick数据切片指针（需要调用release()释放）
	 */
	WTSTickSlice*	get_ticks_by_range(const char* stdCode, uint64_t beginTime, uint64_t endTime = 0);

	/**
	 * @brief 按数量查询K线数据
	 * @param stdCode 标准化合约代码
	 * @param period 周期字符串（如"m1"、"m5"、"d"等）
	 * @param count 查询的数据条数
	 * @param endTime 结束时间（格式：yyyymmddHHMM或yyyymmdd，取决于周期）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice*	get_bars_by_count(const char* stdCode, const char* period, uint32_t count, uint64_t endTime = 0);

	/**
	 * @brief 按数量查询Tick数据
	 * @param stdCode 标准化合约代码
	 * @param count 查询的数据条数
	 * @param endTime 结束时间（格式：yyyymmddHHMMSS，0表示当前时间）
	 * @return Tick数据切片指针（需要调用release()释放）
	 */
	WTSTickSlice*	get_ticks_by_count(const char* stdCode, uint32_t count, uint64_t endTime = 0);

	/**
	 * @brief 按日期查询Tick数据
	 * @param stdCode 标准化合约代码
	 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
	 * @return Tick数据切片指针（需要调用release()释放）
	 */
	WTSTickSlice*	get_ticks_by_date(const char* stdCode, uint32_t uDate = 0);

	/**
	 * @brief 按日期查询秒级K线数据
	 * @param stdCode 标准化合约代码
	 * @param secs 秒数（如：60表示60秒K线）
	 * @param uDate 交易日期（格式：yyyymmdd，0表示今天）
	 * @return K线数据切片指针（需要调用release()释放）
	 */
	WTSKlineSlice*	get_sbars_by_date(const char* stdCode, uint32_t secs, uint32_t uDate = 0);

private:
	/**
	 * @brief 初始化数据管理器
	 * @param config 配置信息（包含数据管理器配置）
	 * 
	 * 根据配置信息初始化数据管理器。
	 */
	void	initDataMgr(WTSVariant* config);

	/**
	 * @brief 初始化解析器
	 * @param cfg 配置信息（包含解析器配置）
	 * 
	 * 根据配置信息初始化解析器适配器。
	 * 支持从文件或配置数组加载解析器配置。
	 */
	void	initParsers(WTSVariant* cfg);

private:
	FuncOnTickCallback	_cb_tick;                                               // 实时Tick数据回调函数指针
	FuncOnBarCallback	_cb_bar;                                                // 实时K线数据回调函数指针
	WTSBaseDataMgr	_bd_mgr;                                                 // 基础数据管理器对象（管理合约、品种等基础数据）
	WTSHotMgr		_hot_mgr;                                                 // 主力合约管理器对象（管理主力合约切换规则）

	WtDataStorage*	_data_store;                                                // 数据存储对象指针（可选，用于数据持久化）
	WtDataManager	_data_mgr;                                                   // 数据管理器对象（管理数据查询、缓存等）
	ParserAdapterMgr	_parsers;                                               // 解析器适配器管理器（管理所有解析器适配器）

	bool			_is_inited;                                                 // 初始化标志：标识是否已初始化

	typedef std::set<uint32_t> SubFlags;                                         // 订阅标志集合类型定义（用于标识订阅状态）
	typedef wt_hashmap<std::string, SubFlags>	StraSubMap;                    // 订阅映射表类型定义（键=合约代码，值=订阅标志集合）
	StraSubMap		_tick_sub_map;                                              // Tick数据订阅表：存储外部订阅的Tick数据（键=合约代码）
	StdUniqueMutex	_mtx_subs;                                                   // 订阅表互斥锁：保护多线程访问订阅表

	StraSubMap		_tick_innersub_map;                                          // Tick数据内部订阅表：存储内部订阅的Tick数据（键=合约代码）
	StdUniqueMutex	_mtx_innersubs;                                              // 内部订阅表互斥锁：保护多线程访问内部订阅表
};

