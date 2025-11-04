/*!
 * \file HisDataMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 历史数据管理器头文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了HisDataMgr类，用于管理历史数据的加载和读取。
 *
 * 主要功能：
 * 1. 动态加载历史数据读取器模块（IBtDtReader）
 * 2. 提供统一的历史数据加载接口（K线、Tick、订单队列、订单明细、逐笔成交）
 * 3. 作为IBtDtReaderSink接口的实现，处理数据读取器的日志回调
 *
 * 设计特点：
 * - 采用插件化设计，通过动态库加载不同的数据读取器实现
 * - 封装了数据读取的细节，为上层提供统一的接口
 * - 支持多种历史数据类型的加载（K线、Tick、订单队列、订单明细、逐笔成交）
 */
#pragma once
#include <functional>                                                  // 函数对象支持
#include "../Includes/IBtDtReader.h"                                  // 历史数据读取器接口定义

/**
 * @brief 数据加载回调函数类型定义
 * 
 * @param std::string& 加载的数据内容（字符串形式）
 */
typedef std::function<void(std::string&)> FuncLoadDataCallback;

NS_WTP_BEGIN                                                          // WonderTrader命名空间开始
class WTSVariant;                                                     // 变体类型前向声明
NS_WTP_END                                                            // WonderTrader命名空间结束

USING_NS_WTP;                                                         // 使用WonderTrader命名空间

/**
 * @brief 历史数据管理器类
 * 
 * 负责管理历史数据的加载和读取，通过动态加载数据读取器模块来提供数据访问功能
 * 
 * 主要功能：
 * - 初始化历史数据读取器
 * - 加载K线数据（原始数据）
 * - 加载Tick数据
 * - 加载订单队列数据
 * - 加载订单明细数据
 * - 加载逐笔成交数据
 */
class HisDataMgr : public IBtDtReaderSink                             // 继承自数据读取器回调接口
{
public:
	/**
	 * @brief 构造函数
	 */
	HisDataMgr() :_reader(NULL) {}                                    // 初始化数据读取器指针为NULL

	/**
	 * @brief 析构函数
	 */
	~HisDataMgr(){}                                                   // 空实现

public:
	/**
	 * @brief 数据读取器日志回调（IBtDtReaderSink接口实现）
	 * 
	 * @param ll 日志级别
	 * @param message 日志消息
	 */
	virtual void reader_log(WTSLogLevel ll, const char* message) override;  // 重写日志回调函数

public:
	/**
	 * @brief 初始化历史数据管理器
	 * 
	 * 从配置中读取数据读取器模块路径，动态加载模块并创建读取器实例
	 * 
	 * @param cfg 配置信息（包含模块路径等）
	 * @return 是否初始化成功
	 */
	bool	init(WTSVariant* cfg);                                    // 初始化函数

	/**
	 * @brief 加载原始K线数据
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param period K线周期
	 * @param cb 数据加载回调函数
	 * @return 是否加载成功
	 */
	bool	load_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, FuncLoadDataCallback cb);  // 加载原始K线数据

	/**
	 * @brief 加载原始Tick数据
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 日期
	 * @param cb 数据加载回调函数
	 * @return 是否加载成功
	 */
	bool	load_raw_ticks(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb);  // 加载原始Tick数据

	/**
	 * @brief 加载原始订单队列数据
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 日期
	 * @param cb 数据加载回调函数
	 * @return 是否加载成功
	 */
	bool	load_raw_ordque(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb);  // 加载原始订单队列数据

	/**
	 * @brief 加载原始订单明细数据
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 日期
	 * @param cb 数据加载回调函数
	 * @return 是否加载成功
	 */
	bool	load_raw_orddtl(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb);  // 加载原始订单明细数据

	/**
	 * @brief 加载原始逐笔成交数据
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @param uDate 日期
	 * @param cb 数据加载回调函数
	 * @return 是否加载成功
	 */
	bool	load_raw_trans(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb);  // 加载原始逐笔成交数据

private:
	IBtDtReader*	_reader;                                             // 数据读取器指针
};

