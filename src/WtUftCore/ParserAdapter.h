/*!
 * \file ParserAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 行情解析器适配器头文件
 *
 * 本文件定义了ParserAdapter类和ParserAdapterMgr类，用于适配不同的行情数据源。
 *
 * 设计逻辑：
 * 1. 适配器模式：通过适配器模式统一不同行情数据源的接口，实现数据源的解耦
 * 2. 动态加载：支持动态加载不同的行情解析器模块，实现插件化架构
 * 3. 数据过滤：支持按交易所、合约代码过滤行情数据，提高处理效率
 * 4. 标准化处理：将不同数据源的行情数据标准化为统一的格式
 * 5. 数据转发：将解析后的行情数据转发给策略引擎处理
 * 6. 多适配器管理：支持同时管理多个行情解析器适配器
 *
 * 主要功能：
 * - 初始化行情解析器模块
 * - 订阅行情数据（Tick、委托队列、委托明细、逐笔成交）
 * - 处理行情数据回调
 * - 数据过滤和标准化
 * - 管理多个解析器适配器
 */
#pragma once
#include <memory>  // 智能指针
#include <boost/core/noncopyable.hpp>  // Boost不可复制基类

#include "../Includes/FasterDefs.h"  // 快速定义头文件
#include "../Includes/IParserApi.h"  // 行情解析器API接口


NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSVariant;  // 前向声明：变体配置类

/**
 * @class IParserStub
 * @brief 行情解析器存根接口类
 * 
 * 定义行情数据推送的接口，用于接收解析器解析后的行情数据。
 * 策略引擎等组件实现此接口来接收行情数据。
 */
class IParserStub
{
public:
	/**
	 * @brief 处理Tick数据推送
	 * @param curTick 当前Tick数据指针
	 * 
	 * 当解析器接收到Tick数据时调用，推送Tick数据。
	 */
	virtual void			handle_push_quote(WTSTickData* curTick){}

	/**
	 * @brief 处理委托明细数据推送
	 * @param curOrdDtl 当前委托明细数据指针
	 * 
	 * 当解析器接收到委托明细数据时调用，推送委托明细数据。
	 */
	virtual void			handle_push_order_detail(WTSOrdDtlData* curOrdDtl){}
	
	/**
	 * @brief 处理委托队列数据推送
	 * @param curOrdQue 当前委托队列数据指针
	 * 
	 * 当解析器接收到委托队列数据时调用，推送委托队列数据。
	 */
	virtual void			handle_push_order_queue(WTSOrdQueData* curOrdQue) {}
	
	/**
	 * @brief 处理逐笔成交数据推送
	 * @param curTrans 当前逐笔成交数据指针
	 * 
	 * 当解析器接收到逐笔成交数据时调用，推送逐笔成交数据。
	 */
	virtual void			handle_push_transaction(WTSTransData* curTrans) {}
};

/**
 * @class ParserAdapter
 * @brief 行情解析器适配器类
 * 
 * 适配不同的行情数据源，统一行情数据接口，实现数据过滤和标准化处理。
 * 继承自IParserSpi接口，实现行情解析器的回调处理。
 * 
 * 核心功能：
 * - 动态加载行情解析器模块
 * - 订阅和管理行情数据
 * - 数据过滤和标准化
 * - 数据转发给策略引擎
 */
class ParserAdapter : public IParserSpi,
					private boost::noncopyable  // 禁止复制
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建行情解析器适配器实例，初始化成员变量。
	 */
	ParserAdapter();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理行情解析器适配器占用的资源，释放解析器模块。
	 */
	~ParserAdapter();

public:
	/**
	 * @brief 初始化行情解析器适配器（从配置文件）
	 * @param id 适配器ID
	 * @param cfg 配置参数，包含解析器模块路径和订阅配置
	 * @param stub 行情数据存根接口指针，用于接收行情数据
	 * @param bgMgr 基础数据管理器指针，用于获取合约信息
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 从配置文件加载行情解析器模块，初始化解析器，订阅行情数据。
	 */
	bool	init(const char* id, WTSVariant* cfg, IParserStub* stub, IBaseDataMgr* bgMgr);
	
	/**
	 * @brief 初始化行情解析器适配器（外部API）
	 * @param id 适配器ID
	 * @param api 外部行情解析器API指针
	 * @param stub 行情数据存根接口指针，用于接收行情数据
	 * @param bgMgr 基础数据管理器指针，用于获取合约信息
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 使用外部提供的行情解析器API初始化适配器，适用于嵌入式场景。
	 */
	bool	initExt(const char* id, IParserApi* api, IParserStub* stub, IBaseDataMgr* bgMgr);

	/**
	 * @brief 释放资源
	 * 
	 * 释放行情解析器适配器占用的资源，断开连接，释放解析器模块。
	 */
	void	release();

	/**
	 * @brief 启动解析器
	 * @return 启动成功返回true，失败返回false
	 * 
	 * 启动行情解析器，开始接收行情数据。
	 */
	bool	run();

	/**
	 * @brief 获取适配器ID
	 * @return 适配器ID字符串
	 * 
	 * 返回适配器的唯一标识符。
	 */
	const char* id() const{ return _id.c_str(); }

public:
	/**
	 * @brief 处理合约列表（IParserSpi接口）
	 * @param aySymbols 合约列表数组
	 * 
	 * 当解析器返回合约列表时调用。默认实现为空。
	 */
	virtual void handleSymbolList(const WTSArray* aySymbols) override {}

	/**
	 * @brief 处理实时行情（IParserSpi接口）
	 * @param quote 实时行情数据指针
	 * @param procFlag 处理标志，是否需要切片
	 * 
	 * 当解析器接收到Tick行情数据时调用。
	 * 如果是从外部接入的快照行情数据，则需要切片；如果是内部广播的就不需要切片。
	 */
	virtual void handleQuote(WTSTickData *quote, uint32_t procFlag) override;

	/**
	 * @brief 处理委托队列数据（IParserSpi接口，股票level2）
	 * @param ordQueData 委托队列数据指针
	 * 
	 * 当解析器接收到委托队列数据时调用，用于股票level2行情。
	 */
	virtual void handleOrderQueue(WTSOrdQueData* ordQueData) override;

	/**
	 * @brief 处理逐笔委托数据（IParserSpi接口，股票level2）
	 * @param ordDetailData 逐笔委托数据指针
	 * 
	 * 当解析器接收到逐笔委托数据时调用，用于股票level2行情。
	 */
	virtual void handleOrderDetail(WTSOrdDtlData* ordDetailData) override;

	/**
	 * @brief 处理逐笔成交数据（IParserSpi接口）
	 * @param transData 逐笔成交数据指针
	 * 
	 * 当解析器接收到逐笔成交数据时调用。
	 */
	virtual void handleTransaction(WTSTransData* transData) override;

	/**
	 * @brief 处理解析器日志（IParserSpi接口）
	 * @param ll 日志级别
	 * @param message 日志消息
	 * 
	 * 当解析器输出日志时调用，转发日志到日志系统。
	 */
	virtual void handleParserLog(WTSLogLevel ll, const char* message) override;

	/**
	 * @brief 获取基础数据管理器（IParserSpi接口）
	 * @return 基础数据管理器指针
	 * 
	 * 返回基础数据管理器，供解析器查询合约信息等。
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override { return _bd_mgr; }


private:
	IParserApi*			_parser_api;  // 行情解析器API指针
	FuncDeleteParser	_remover;  // 删除解析器函数指针

	bool				_stopped;  // 是否已停止标志

	/**
	 * @typedef ExchgFilter
	 * @brief 交易所过滤器类型定义
	 * 
	 * 用于存储需要订阅的交易所列表。
	 */
	typedef wt_hashset<std::string>	ExchgFilter;
	ExchgFilter			_exchg_filter;  // 交易所过滤器，只接收指定交易所的数据
	ExchgFilter			_code_filter;  // 合约代码过滤器，只接收指定合约的数据
	IBaseDataMgr*		_bd_mgr;  // 基础数据管理器指针，用于获取合约信息
	IParserStub*		_stub;  // 行情数据存根接口指针，用于接收行情数据
	WTSVariant*			_cfg;  // 配置参数指针
	std::string			_id;  // 适配器ID
};

/**
 * @typedef ParserAdapterPtr
 * @brief 行情解析器适配器智能指针类型
 * 
 * 使用shared_ptr管理适配器生命周期。
 */
typedef std::shared_ptr<ParserAdapter>	ParserAdapterPtr;

/**
 * @typedef ParserAdapterMap
 * @brief 行情解析器适配器映射表类型
 * 
 * 键为适配器ID，值为适配器智能指针。
 */
typedef wt_hashmap<std::string, ParserAdapterPtr>	ParserAdapterMap;

/**
 * @class ParserAdapterMgr
 * @brief 行情解析器适配器管理器类
 * 
 * 管理多个行情解析器适配器，提供统一的添加、查询、启动接口。
 * 禁止复制构造和赋值。
 */
class ParserAdapterMgr : private boost::noncopyable  // 禁止复制
{
public:
	/**
	 * @brief 释放所有适配器
	 * 
	 * 释放所有管理的适配器资源。
	 */
	void	release();

	/**
	 * @brief 启动所有适配器
	 * 
	 * 启动所有管理的适配器，开始接收行情数据。
	 */
	void	run();

	/**
	 * @brief 获取适配器
	 * @param id 适配器ID
	 * @return 适配器智能指针，如果不存在返回空指针
	 * 
	 * 根据适配器ID查找对应的适配器。
	 */
	ParserAdapterPtr getAdapter(const char* id);

	/**
	 * @brief 添加适配器
	 * @param id 适配器ID
	 * @param adapter 适配器智能指针
	 * @return 添加成功返回true，失败返回false
	 * 
	 * 将适配器添加到管理器中。如果ID已存在，则添加失败。
	 */
	bool	addAdapter(const char* id, ParserAdapterPtr& adapter);


public:
	ParserAdapterMap _adapters;  // 适配器映射表，存储所有适配器
};

NS_WTP_END  // WonderTrader命名空间结束