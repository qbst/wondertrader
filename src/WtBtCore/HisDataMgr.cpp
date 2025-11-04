/*!
 * \file HisDataMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 历史数据管理器实现文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * 本文件是HisDataMgr类的实现文件，提供了历史数据管理器的所有功能实现。
 *
 * 主要功能模块：
 * 1. 初始化功能：动态加载数据读取器模块，创建读取器实例
 * 2. 日志处理：将数据读取器的日志转发到系统日志
 * 3. 数据加载：提供统一的数据加载接口，支持多种数据类型
 *
 * 核心算法：
 * - 动态库加载：通过DLLHelper加载数据读取器模块
 * - 函数符号获取：获取createBtDtReader函数创建读取器实例
 * - 数据读取：调用读取器的read接口，通过回调函数返回数据
 */
#include "HisDataMgr.h"                                               // 历史数据管理器头文件
#include "WtHelper.h"                                                 // WonderTrader辅助函数
#include "../Share/DLLHelper.hpp"                                    // 动态库加载辅助工具
#include "../Includes/WTSVariant.hpp"                                 // 变体类型定义
#include "../WTSTools/WTSLogger.h"                                   // 日志工具

/**
 * @brief 数据读取器日志回调函数实现
 * 
 * 将数据读取器的日志转发到系统日志系统
 * 
 * @param ll 日志级别
 * @param message 日志消息
 */
void HisDataMgr::reader_log(WTSLogLevel ll, const char* message)
{
	WTSLogger::log_raw(ll, message);                                  // 将日志转发到系统日志
}

/**
 * @brief 初始化历史数据管理器
 * 
 * 1. 从配置中读取模块名称（如果未指定则使用默认模块）
 * 2. 构造模块文件路径
 * 3. 动态加载模块库
 * 4. 获取createBtDtReader函数指针
 * 5. 创建数据读取器实例
 * 6. 初始化数据读取器
 * 
 * @param cfg 配置信息（包含模块路径等）
 * @return 是否初始化成功
 */
bool HisDataMgr::init(WTSVariant* cfg)
{
	std::string module = cfg->getCString("module");                   // 从配置中获取模块名称
	if (module.empty())                                                // 如果模块名称为空
		module = WtHelper::getInstDir() + DLLHelper::wrap_module("WtDataStorage");  // 使用默认模块名称
	else                                                               // 如果指定了模块名称
		module = WtHelper::getInstDir() + DLLHelper::wrap_module(module.c_str());  // 使用指定模块名称

	DllHandle libParser = DLLHelper::load_library(module.c_str());    // 加载动态库
	if (libParser)                                                    // 如果加载成功
	{
		FuncCreateBtDtReader pFuncCreator = (FuncCreateBtDtReader)DLLHelper::get_symbol(libParser, "createBtDtReader");  // 获取创建函数指针
		if (pFuncCreator == NULL)                                      // 如果函数指针为空
		{
			WTSLogger::error("Initializing of backtest data reader failed: function createBtDtReader not found...");  // 记录错误日志
		}

		if (pFuncCreator)                                              // 如果函数指针有效
		{
			_reader = pFuncCreator();                                 // 创建数据读取器实例
		}

		WTSLogger::debug("Back data storage module {} loaded", module);  // 记录调试日志
	}
	else                                                               // 如果加载失败
	{
		WTSLogger::error("Loading module back data storage module {} failed", module);  // 记录错误日志

	}

	_reader->init(cfg, this);                                         // 初始化数据读取器（传入配置和回调对象）

	return true;                                                       // 返回成功
}

/**
 * @brief 加载原始K线数据
 * 
 * 1. 检查数据读取器是否已初始化
 * 2. 调用读取器的read_raw_bars方法读取数据
 * 3. 如果读取成功，通过回调函数返回数据
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param period K线周期
 * @param cb 数据加载回调函数
 * @return 是否加载成功
 */
bool HisDataMgr::load_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, FuncLoadDataCallback cb)
{
	if(_reader == NULL)                                               // 如果数据读取器未初始化
	{
		WTSLogger::log_raw(LL_ERROR, "Backtest Data Reader not initialized");  // 记录错误日志
		return false;                                                  // 返回失败
	}

	std::string buffer;                                                // 数据缓冲区
	bool bSucc = _reader->read_raw_bars(exchg, code, period, buffer);  // 读取原始K线数据
	if (bSucc)                                                         // 如果读取成功
		cb(buffer);                                                    // 调用回调函数返回数据
	return bSucc;                                                      // 返回读取结果
}

/**
 * @brief 加载原始Tick数据
 * 
 * 1. 检查数据读取器是否已初始化
 * 2. 调用读取器的read_raw_ticks方法读取数据
 * 3. 如果读取成功，通过回调函数返回数据
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param uDate 日期
 * @param cb 数据加载回调函数
 * @return 是否加载成功
 */
bool HisDataMgr::load_raw_ticks(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb)
{
	if (_reader == NULL)                                              // 如果数据读取器未初始化
	{
		WTSLogger::log_raw(LL_ERROR, "Backtest Data Reader not initialized");  // 记录错误日志
		return false;                                                  // 返回失败
	}

	std::string buffer;                                                // 数据缓冲区
	bool bSucc = _reader->read_raw_ticks(exchg, code, uDate, buffer);  // 读取原始Tick数据
	if (bSucc)                                                         // 如果读取成功
		cb(buffer);                                                    // 调用回调函数返回数据
	return bSucc;                                                      // 返回读取结果
}

/**
 * @brief 加载原始逐笔成交数据
 * 
 * 1. 检查数据读取器是否已初始化
 * 2. 调用读取器的read_raw_transactions方法读取数据
 * 3. 如果读取成功，通过回调函数返回数据
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param uDate 日期
 * @param cb 数据加载回调函数
 * @return 是否加载成功
 */
bool HisDataMgr::load_raw_trans(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb)
{
	if (_reader == NULL)                                              // 如果数据读取器未初始化
	{
		WTSLogger::log_raw(LL_ERROR, "Backtest Data Reader not initialized");  // 记录错误日志
		return false;                                                  // 返回失败
	}

	std::string buffer;                                                // 数据缓冲区
	bool bSucc = _reader->read_raw_transactions(exchg, code, uDate, buffer);  // 读取原始逐笔成交数据
	if (bSucc)                                                         // 如果读取成功
		cb(buffer);                                                    // 调用回调函数返回数据
	return bSucc;                                                      // 返回读取结果
}

/**
 * @brief 加载原始订单队列数据
 * 
 * 1. 检查数据读取器是否已初始化
 * 2. 调用读取器的read_raw_order_queues方法读取数据
 * 3. 如果读取成功，通过回调函数返回数据
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param uDate 日期
 * @param cb 数据加载回调函数
 * @return 是否加载成功
 */
bool HisDataMgr::load_raw_ordque(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb)
{
	if (_reader == NULL)                                              // 如果数据读取器未初始化
	{
		WTSLogger::log_raw(LL_ERROR, "Backtest Data Reader not initialized");  // 记录错误日志
		return false;                                                  // 返回失败
	}

	std::string buffer;                                                // 数据缓冲区
	bool bSucc = _reader->read_raw_order_queues(exchg, code, uDate, buffer);  // 读取原始订单队列数据
	if (bSucc)                                                         // 如果读取成功
		cb(buffer);                                                    // 调用回调函数返回数据
	return bSucc;                                                      // 返回读取结果
}

/**
 * @brief 加载原始订单明细数据
 * 
 * 1. 检查数据读取器是否已初始化
 * 2. 调用读取器的read_raw_order_details方法读取数据
 * 3. 如果读取成功，通过回调函数返回数据
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param uDate 日期
 * @param cb 数据加载回调函数
 * @return 是否加载成功
 */
bool HisDataMgr::load_raw_orddtl(const char* exchg, const char* code, uint32_t uDate, FuncLoadDataCallback cb)
{
	if (_reader == NULL)                                              // 如果数据读取器未初始化
	{
		WTSLogger::log_raw(LL_ERROR, "Backtest Data Reader not initialized");  // 记录错误日志
		return false;                                                  // 返回失败
	}

	std::string buffer;                                                // 数据缓冲区
	bool bSucc = _reader->read_raw_order_details(exchg, code, uDate, buffer);  // 读取原始订单明细数据
	if (bSucc)                                                         // 如果读取成功
		cb(buffer);                                                    // 调用回调函数返回数据
	return bSucc;                                                      // 返回读取结果
}