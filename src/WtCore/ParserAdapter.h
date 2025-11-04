/*!
 * \file ParserAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 解析器适配器头文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件定义了解析器适配器类ParserAdapter和解析器适配器管理器类ParserAdapterMgr。
 * ParserAdapter作为行情解析器（IParserApi）和策略引擎之间的适配器，
 * 实现了IParserSpi接口，负责接收解析器推送的行情数据，并进行处理、过滤和转发。
 * 
 * 主要功能：
 * 1. 解析器生命周期管理：初始化、运行、释放解析器。
 * 2. 行情数据接收：接收Tick、委托队列、委托明细、逐笔成交等行情数据。
 * 3. 数据过滤：支持交易所过滤和合约代码过滤。
 * 4. 时间校验：可选的时间戳校验功能，过滤错误时间戳的数据。
 * 5. 代码转换：将原始合约代码转换为标准合约代码。
 * 6. 数据转发：将处理后的行情数据转发给策略引擎（通过IParserStub接口）。
 * 7. 日志管理：统一管理解析器内部的日志输出。
 * 8. 适配器管理：ParserAdapterMgr管理多个解析器适配器实例。
 */
#pragma once  // 防止头文件重复包含
#include <memory>  // 包含智能指针头文件
#include <boost/core/noncopyable.hpp>  // 包含Boost的noncopyable类，用于禁止拷贝构造和赋值

#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap, wt_hashset
#include "../Includes/IParserApi.h"  // 包含解析器API接口

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：配置变体类
class IHotMgr;  // 前向声明：热点合约管理器接口

/**
 * @class IParserStub
 * @brief 解析器存根接口类
 *
 * 该接口定义了策略引擎需要实现的行情数据接收方法。
 * 解析器适配器通过该接口将处理后的行情数据推送给策略引擎。
 */
class IParserStub
{
public:
	/**
	 * @brief 推送行情数据
	 * @param curTick 当前Tick数据指针
	 *
	 * 当解析器收到新的行情数据时，通过此方法推送给策略引擎。
	 * 默认实现为空，子类需要重写此方法以实现具体的处理逻辑。
	 */
	virtual void			handle_push_quote(WTSTickData* curTick){}  // 推送行情数据

	/**
	 * @brief 推送委托明细数据
	 * @param curOrdDtl 当前委托明细数据指针
	 *
	 * 当解析器收到新的委托明细数据时，通过此方法推送给策略引擎。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void			handle_push_order_detail(WTSOrdDtlData* curOrdDtl){}  // 推送委托明细数据
	/**
	 * @brief 推送委托队列数据
	 * @param curOrdQue 当前委托队列数据指针
	 *
	 * 当解析器收到新的委托队列数据时，通过此方法推送给策略引擎。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void			handle_push_order_queue(WTSOrdQueData* curOrdQue) {}  // 推送委托队列数据
	/**
	 * @brief 推送逐笔成交数据
	 * @param curTrans 当前逐笔成交数据指针
	 *
	 * 当解析器收到新的逐笔成交数据时，通过此方法推送给策略引擎。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void			handle_push_transaction(WTSTransData* curTrans) {}  // 推送逐笔成交数据
};

/**
 * @class ParserAdapter
 * @brief 解析器适配器类
 *
 * 该类作为行情解析器（IParserApi）和策略引擎之间的适配器，
 * 实现了IParserSpi接口，负责接收解析器推送的行情数据，
 * 并进行处理、过滤、代码转换后转发给策略引擎。
 * 使用boost::noncopyable继承，禁止对象的拷贝构造和赋值。
 */
class ParserAdapter : public IParserSpi,  // 继承解析器SPI接口
					private boost::noncopyable  // 继承boost::noncopyable，禁止拷贝构造和赋值
{
public:
	/**
	 * @brief 构造函数
	 *
	 * 初始化解析器适配器对象，设置默认值。
	 */
	ParserAdapter();  // 构造函数
	/**
	 * @brief 析构函数
	 *
	 * 销毁解析器适配器对象，释放资源。
	 */
	~ParserAdapter();  // 析构函数

public:
	/**
	 * @brief 初始化解析器适配器
	 * @param id 解析器ID
	 * @param cfg 配置参数
	 * @param stub 解析器存根指针，用于接收行情数据
	 * @param bgMgr 基础数据管理器指针
	 * @param hotMgr 热点合约管理器指针，可选参数，默认NULL
	 * @return bool 初始化成功返回true，失败返回false
	 *
	 * 根据配置参数初始化解析器适配器，包括：
	 * 1. 加载解析器模块（动态库）。
	 * 2. 创建解析器API实例。
	 * 3. 注册SPI回调接口。
	 * 4. 初始化解析器。
	 * 5. 根据过滤器配置订阅合约。
	 */
	bool	init(const char* id, WTSVariant* cfg, IParserStub* stub, IBaseDataMgr* bgMgr, IHotMgr* hotMgr = NULL);  // 初始化解析器适配器

	/**
	 * @brief 扩展初始化解析器适配器
	 * @param id 解析器ID
	 * @param api 解析器API指针（已创建好的）
	 * @param stub 解析器存根指针，用于接收行情数据
	 * @param bgMgr 基础数据管理器指针
	 * @param hotMgr 热点合约管理器指针，可选参数，默认NULL
	 * @return bool 初始化成功返回true，失败返回false
	 *
	 * 使用已创建的解析器API实例初始化适配器。
	 * 与init方法不同，此方法不需要加载动态库，直接使用提供的API实例。
	 */
	bool	initExt(const char* id, IParserApi* api, IParserStub* stub, IBaseDataMgr* bgMgr, IHotMgr* hotMgr = NULL);  // 扩展初始化解析器适配器

	/**
	 * @brief 释放解析器适配器
	 *
	 * 释放解析器资源，包括调用解析器的release方法和删除解析器实例。
	 */
	void	release();  // 释放解析器适配器

	/**
	 * @brief 运行解析器适配器
	 * @return bool 运行成功返回true，失败返回false
	 *
	 * 连接解析器，开始接收行情数据。
	 */
	bool	run();  // 运行解析器适配器

	/**
	 * @brief 获取解析器ID
	 * @return const char* 返回解析器ID字符串
	 *
	 * 获取当前解析器适配器的唯一标识ID。
	 */
	const char* id() const{ return _id.c_str(); }  // 获取解析器ID

public:
	/**
	 * @brief 处理合约列表（IParserSpi接口）
	 * @param aySymbols 合约列表数组
	 *
	 * 当解析器推送合约列表时被调用。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void handleSymbolList(const WTSArray* aySymbols) override {}  // 处理合约列表

	/**
	 * @brief 处理实时行情（IParserSpi接口）
	 * @param quote 实时行情数据指针
	 * @param procFlag 处理标志
	 *
	 * 当解析器收到实时行情时被调用。
	 * 处理流程：
	 * 1. 检查行情数据是否有效。
	 * 2. 应用交易所过滤器。
	 * 3. 可选的时间戳校验。
	 * 4. 将原始合约代码转换为标准合约代码。
	 * 5. 通过存根接口推送给策略引擎。
	 */
	virtual void handleQuote(WTSTickData *quote, uint32_t procFlag) override;  // 处理实时行情

	/**
	 * @brief 处理委托队列数据（IParserSpi接口）
	 * @param ordQueData 委托队列数据指针
	 *
	 * 当解析器收到委托队列数据时被调用。
	 * 处理流程：
	 * 1. 检查数据是否有效。
	 * 2. 应用交易所过滤器。
	 * 3. 将原始合约代码转换为标准合约代码。
	 * 4. 通过存根接口推送给策略引擎。
	 */
	virtual void handleOrderQueue(WTSOrdQueData* ordQueData) override;  // 处理委托队列数据

	/**
	 * @brief 处理逐笔委托数据（IParserSpi接口）
	 * @param ordDetailData 逐笔委托数据指针
	 *
	 * 当解析器收到逐笔委托数据时被调用。
	 * 处理流程：
	 * 1. 检查数据是否有效。
	 * 2. 应用交易所过滤器。
	 * 3. 将原始合约代码转换为标准合约代码。
	 * 4. 通过存根接口推送给策略引擎。
	 */
	virtual void handleOrderDetail(WTSOrdDtlData* ordDetailData) override;  // 处理逐笔委托数据

	/**
	 * @brief 处理逐笔成交数据（IParserSpi接口）
	 * @param transData 逐笔成交数据指针
	 *
	 * 当解析器收到逐笔成交数据时被调用。
	 * 处理流程：
	 * 1. 检查数据是否有效。
	 * 2. 应用交易所过滤器。
	 * 3. 将原始合约代码转换为标准合约代码。
	 * 4. 通过存根接口推送给策略引擎。
	 */
	virtual void handleTransaction(WTSTransData* transData) override;  // 处理逐笔成交数据

	/**
	 * @brief 处理解析器日志（IParserSpi接口）
	 * @param ll 日志级别
	 * @param message 日志消息内容
	 *
	 * 当解析器产生日志时被调用，将日志转发给统一的日志系统。
	 */
	virtual void handleParserLog(WTSLogLevel ll, const char* message) override;  // 处理解析器日志

	/**
	 * @brief 获取基础数据管理器（IParserSpi接口）
	 * @return IBaseDataMgr* 返回基础数据管理器指针
	 *
	 * 返回解析器适配器关联的基础数据管理器，供解析器查询合约信息等。
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override { return _bd_mgr; }  // 获取基础数据管理器


private:
	IParserApi*			_parser_api;  // 解析器API指针
	FuncDeleteParser	_remover;  // 删除解析器的函数指针

	bool				_stopped;  // 停止标志

	/**
	 * @brief 检查时间设置项
	 * 
	 * 如果为true，则在收到行情的时候进行时间检查。
	 * 主要适用于直接从行情源接入的情况。
	 * 因为直接从行情源接入很可能会有错误时间戳的数据进来。
	 * 该选项默认为false。
	 */
	bool				_check_time;  // 是否检查时间戳标志

	typedef wt_hashset<std::string>	ExchgFilter;  // 交易所过滤器类型，使用字符串集合
	ExchgFilter			_exchg_filter;  // 交易所过滤器，只处理指定交易所的数据
	ExchgFilter			_code_filter;  // 合约代码过滤器，只处理指定合约的数据
	IBaseDataMgr*		_bd_mgr;  // 基础数据管理器指针，用于查询合约信息
	IHotMgr*			_hot_mgr;  // 热点合约管理器指针，用于查询主力合约等
	IParserStub*		_stub;  // 解析器存根指针，用于推送行情数据
	WTSVariant*			_cfg;  // 配置参数指针
	std::string			_id;  // 解析器ID
};

typedef std::shared_ptr<ParserAdapter>	ParserAdapterPtr;  // 定义ParserAdapterPtr为ParserAdapter的共享指针类型
typedef wt_hashmap<std::string, ParserAdapterPtr>	ParserAdapterMap;  // 定义ParserAdapterMap为解析器适配器映射表类型，键为解析器ID，值为适配器指针

/**
 * @class ParserAdapterMgr
 * @brief 解析器适配器管理器类
 *
 * 该类负责管理多个解析器适配器实例。
 * 提供添加、获取、运行和释放适配器的功能。
 * 使用boost::noncopyable继承，禁止对象的拷贝构造和赋值。
 */
class ParserAdapterMgr : private boost::noncopyable  // 继承boost::noncopyable，禁止拷贝构造和赋值
{
public:
	/**
	 * @brief 释放所有解析器适配器
	 *
	 * 释放所有已添加的解析器适配器，并清空映射表。
	 */
	void	release();  // 释放所有解析器适配器

	/**
	 * @brief 运行所有解析器适配器
	 *
	 * 启动所有已添加的解析器适配器，开始接收行情数据。
	 */
	void	run();  // 运行所有解析器适配器

	/**
	 * @brief 获取解析器适配器
	 * @param id 解析器ID
	 * @return ParserAdapterPtr 返回解析器适配器的共享指针，如果未找到则返回空指针
	 *
	 * 根据解析器ID获取对应的解析器适配器实例。
	 */
	ParserAdapterPtr getAdapter(const char* id);  // 获取解析器适配器

	/**
	 * @brief 添加解析器适配器
	 * @param id 解析器ID
	 * @param adapter 解析器适配器共享指针
	 * @return bool 添加成功返回true，失败返回false
	 *
	 * 将解析器适配器添加到管理器中。
	 * 如果ID已存在，则添加失败。
	 */
	bool	addAdapter(const char* id, ParserAdapterPtr& adapter);  // 添加解析器适配器


public:
	ParserAdapterMap _adapters;  // 解析器适配器映射表，键为解析器ID，值为适配器指针
};

NS_WTP_END  // 结束WonderTrader命名空间