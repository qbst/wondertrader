/*!
 * \file ParserAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader行情解析器适配器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了ParserAdapter（行情解析器适配器）类和ParserAdapterMgr（适配器管理器）类，
 * 是WonderTrader数据伺服器模块中用于管理和适配各种行情解析器（Parser）的核心组件。
 * 该文件实现了插件化的解析器架构，支持动态加载和管理多个解析器，实现不同数据源的接入。
 * 
 * 核心设计理念：
 * 
 * 1. 适配器模式（Adapter Pattern）：
 *    - ParserAdapter作为IParserSpi接口的实现，适配各种解析器
 *    - 提供统一的接口，屏蔽不同解析器的实现差异
 *    - 支持插件化架构，动态加载解析器模块
 * 
 * 2. 过滤器机制（Filter Mechanism）：
 *    - 支持交易所过滤（exchg_filter）
 *    - 支持合约代码过滤（code_filter）
 *    - 减少不必要的数据处理，提高性能
 * 
 * 3. 生命周期管理（Lifecycle Management）：
 *    - 统一管理解析器的初始化、启动、停止和释放
 *    - 支持多个解析器并发运行
 *    - 自动处理解析器的资源清理
 * 
 * 主要功能模块：
 * 
 * 1. ParserAdapter类（解析器适配器）：
 *    - 管理单个解析器的生命周期
 *    - 处理解析器的回调事件
 *    - 实现数据过滤和转发
 * 
 * 2. ParserAdapterMgr类（适配器管理器）：
 *    - 管理多个解析器适配器
 *    - 提供解析器的添加、获取、启动等操作
 *    - 统一管理所有解析器的生命周期
 * 
 * 使用场景：
 * - 多数据源接入（如CTP、XTP、自定义数据源等）
 * - 行情数据的实时接收和处理
 * - 不同交易所数据的统一管理
 * - 自定义解析器的集成
 * 
 * 技术特点：
 * - 插件化架构设计
 * - 多解析器并发支持
 * - 高效的数据过滤机制
 * - 统一的事件处理接口
 * 
 * 注意事项：
 * - 解析器必须实现IParserApi接口
 * - 解析器模块需要导出createParser和deleteParser函数
 * - 过滤器配置支持多种格式（交易所、合约代码、品种ID等）
 * - 解析器停止后不再处理新数据
 */
#pragma once                                                                     // 防止头文件重复包含
#include <set>                                                                    // 包含集合容器（用于过滤器）
#include <vector>                                                                // 包含向量容器
#include <memory>                                                                // 包含智能指针
#include <boost/core/noncopyable.hpp>                                            // 包含Boost不可复制基类（禁止拷贝）
#include "../Includes/IParserApi.h"                                             // 包含解析器API接口定义

NS_WTP_BEGIN                                                                    // WonderTrader命名空间开始
class WTSVariant;                                                                // 配置变体类前向声明
NS_WTP_END                                                                      // WonderTrader命名空间结束

USING_NS_WTP;                                                                    // 使用WonderTrader命名空间
class WTSBaseDataMgr;                                                            // 基础数据管理器类前向声明
class WtDtRunner;                                                                // 数据服务运行器类前向声明

/**
 * @class ParserAdapter
 * @brief 行情解析器适配器类
 * 
 * 适配器类，负责管理和适配各种行情解析器（Parser）。
 * 实现了IParserSpi接口，处理解析器的各种回调事件。
 * 支持动态加载解析器模块，并提供数据过滤功能。
 */
class ParserAdapter : public IParserSpi, private boost::noncopyable           // 继承IParserSpi接口和boost::noncopyable基类
{
public:
	/**
	 * @brief 构造函数
	 * @param bgMgr 基础数据管理器指针
	 * @param runner 数据服务运行器指针
	 * 
	 * 初始化适配器对象，设置基础数据管理器和数据服务运行器。
	 */
	ParserAdapter(WTSBaseDataMgr * bgMgr, WtDtRunner* runner);

	/**
	 * @brief 析构函数
	 * 
	 * 清理适配器资源，注意应在release()之后调用。
	 */
	~ParserAdapter();

public:
	/**
	 * @brief 初始化适配器（从配置文件）
	 * @param id 解析器ID（唯一标识）
	 * @param cfg 配置信息（WTSVariant对象）
	 * @return 是否初始化成功
	 * 
	 * 从配置信息中加载解析器模块，初始化解析器，并设置订阅列表。
	 * 配置信息应包含module（模块名）、filter（交易所过滤）、code（合约过滤）等字段。
	 */
	bool	init(const char* id, WTSVariant* cfg);

	/**
	 * @brief 初始化适配器（外部API）
	 * @param id 解析器ID（唯一标识）
	 * @param api 解析器API指针（外部创建）
	 * @return 是否初始化成功
	 * 
	 * 使用外部创建的解析器API对象初始化适配器。
	 * 适用于解析器已经创建或需要特殊初始化的场景。
	 */
	bool	initExt(const char* id, IParserApi* api);

	/**
	 * @brief 释放适配器资源
	 * 
	 * 停止解析器并释放相关资源。
	 * 会调用解析器的release()方法，并根据创建方式决定是否删除解析器对象。
	 */
	void	release();

	/**
	 * @brief 启动解析器
	 * @return 是否启动成功
	 * 
	 * 启动解析器，开始接收行情数据。
	 * 实际上是调用解析器的connect()方法建立连接。
	 */
	bool	run();

	/**
	 * @brief 获取解析器ID
	 * @return 解析器ID字符串指针
	 * 
	 * 返回适配器管理的解析器的唯一标识符。
	 */
	const char* id() const { return _id.c_str(); }

public:
	// ===== IParserSpi接口实现 =====

	/**
	 * @brief 处理合约列表回调
	 * @param aySymbols 合约列表数组
	 * 
	 * 当解析器返回合约列表时调用。
	 * 当前实现为空，可扩展用于处理合约信息。
	 */
	virtual void handleSymbolList(const WTSArray* aySymbols) override;

	/**
	 * @brief 处理行情数据回调
	 * @param quote Tick行情数据指针
	 * @param procFlag 处理标志位
	 * 
	 * 当解析器接收到新的Tick数据时调用。
	 * 会将数据转发给数据服务运行器进行处理。
	 * 支持数据过滤，只处理订阅的合约数据。
	 */
	virtual void handleQuote(WTSTickData *quote, uint32_t procFlag) override;

	/**
	 * @brief 处理委托队列数据回调
	 * @param ordQueData 委托队列数据指针
	 * 
	 * 当解析器接收到委托队列数据时调用。
	 * 当前实现为空，可扩展用于处理Level-2数据。
	 */
	virtual void handleOrderQueue(WTSOrdQueData* ordQueData) override;

	/**
	 * @brief 处理逐笔成交数据回调
	 * @param transData 逐笔成交数据指针
	 * 
	 * 当解析器接收到逐笔成交数据时调用。
	 * 当前实现为空，可扩展用于处理Level-2数据。
	 */
	virtual void handleTransaction(WTSTransData* transData) override;

	/**
	 * @brief 处理逐笔委托数据回调
	 * @param ordDetailData 逐笔委托数据指针
	 * 
	 * 当解析器接收到逐笔委托数据时调用。
	 * 当前实现为空，可扩展用于处理Level-2数据。
	 */
	virtual void handleOrderDetail(WTSOrdDtlData* ordDetailData) override;

	/**
	 * @brief 处理解析器日志回调
	 * @param ll 日志级别
	 * @param message 日志消息
	 * 
	 * 当解析器输出日志时调用。
	 * 将日志转发到WonderTrader的日志系统。
	 */
	virtual void handleParserLog(WTSLogLevel ll, const char* message) override;

	/**
	 * @brief 获取基础数据管理器
	 * @return 基础数据管理器指针
	 * 
	 * 返回适配器使用的基础数据管理器。
	 * 解析器可以通过此接口获取合约信息等基础数据。
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override;

private:
	IParserApi*			_parser_api;                                            // 解析器API接口指针
	FuncDeleteParser	_remover;                                               // 解析器删除函数指针（用于动态库创建的解析器）
	WTSBaseDataMgr*		_bd_mgr;                                                 // 基础数据管理器指针
	WtDtRunner*			_dt_runner;                                              // 数据服务运行器指针

	bool				_stopped;                                                // 停止标志：标识解析器是否已停止

	typedef wt_hashset<std::string>	ExchgFilter;                                // 交易所过滤器类型定义（字符串集合）
	ExchgFilter			_exchg_filter;                                          // 交易所过滤器：只处理指定交易所的数据
	ExchgFilter			_code_filter;                                           // 合约代码过滤器：只处理指定合约的数据
	WTSVariant*			_cfg;                                                   // 配置信息指针（用于初始化）
	std::string			_id;                                                     // 解析器ID（唯一标识）
};

typedef std::shared_ptr<ParserAdapter>	ParserAdapterPtr;                        // 解析器适配器智能指针类型定义
typedef wt_hashmap<std::string, ParserAdapterPtr>	ParserAdapterMap;       // 解析器适配器映射表类型定义（ID->适配器）

/**
 * @class ParserAdapterMgr
 * @brief 解析器适配器管理器类
 * 
 * 管理器类，负责管理多个解析器适配器。
 * 提供统一的接口添加、获取、启动和释放解析器。
 * 支持多个解析器并发运行。
 */
class ParserAdapterMgr : private boost::noncopyable                          // 继承boost::noncopyable基类，禁止拷贝
{
public:
	/**
	 * @brief 释放所有适配器资源
	 * 
	 * 遍历所有适配器，调用它们的release()方法释放资源。
	 * 然后清空适配器映射表。
	 */
	void	release();

	/**
	 * @brief 启动所有适配器
	 * 
	 * 遍历所有适配器，调用它们的run()方法启动解析器。
	 * 用于批量启动所有配置的解析器。
	 */
	void	run();

	/**
	 * @brief 获取指定ID的适配器
	 * @param id 解析器ID
	 * @return 适配器智能指针（如果不存在则返回空指针）
	 * 
	 * 根据ID查找并返回对应的适配器。
	 * 用于访问已注册的解析器。
	 */
	ParserAdapterPtr getAdapter(const char* id);

	/**
	 * @brief 添加适配器
	 * @param id 解析器ID（唯一标识）
	 * @param adapter 适配器智能指针
	 * @return 是否添加成功
	 * 
	 * 将适配器添加到管理器中。
	 * 如果ID已存在，添加失败并返回false。
	 */
	bool	addAdapter(const char* id, ParserAdapterPtr& adapter);

	/**
	 * @brief 获取适配器数量
	 * @return 适配器数量
	 * 
	 * 返回当前管理的适配器数量。
	 */
	uint32_t size() const { return (uint32_t)_adapters.size(); }

public:
	ParserAdapterMap _adapters;                                                  // 适配器映射表：存储所有解析器适配器（ID->适配器指针）
};


