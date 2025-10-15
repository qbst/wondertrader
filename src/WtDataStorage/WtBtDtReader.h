/*!
 * \file WtBtDtReader.h
 * \project WonderTrader
 * 
 * \brief WonderTrader回测数据读取器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtBtDtReader类，是WonderTrader框架中用于回测数据读取的核心组件。
 * 该类实现了IBtDtReader接口，提供了从WonderTrader数据存储格式中读取原始数据的功能，
 * 支持K线数据、Tick数据、逐笔成交、逐笔委托、委托队列等多种数据类型的读取。
 * 
 * 核心设计理念：
 * 
 * 1. 回测数据访问（Backtest Data Access）：
 *    - 提供统一的回测数据读取接口
 *    - 支持多种数据格式的原始数据读取
 *    - 与回测引擎无缝集成
 * 
 * 2. 数据格式兼容（Data Format Compatibility）：
 *    - 支持WonderTrader自有数据格式
 *    - 兼容历史数据和新版本数据
 *    - 提供数据格式转换功能
 * 
 * 3. 高效数据读取（Efficient Data Reading）：
 *    - 使用内存映射文件技术提高读取性能
 *    - 支持大数据量的快速读取
 *    - 提供原始数据缓冲区访问
 * 
 * 主要功能：
 * 
 * 1. K线数据读取：
 *    - 支持1分钟、5分钟、日线等不同周期的K线数据
 *    - 提供原始数据缓冲区，支持自定义数据处理
 *    - 支持复权数据的读取
 * 
 * 2. Tick数据读取：
 *    - 支持按日期读取Tick数据
 *    - 提供高频行情数据的原始访问
 *    - 支持Tick数据的批量读取
 * 
 * 3. 逐笔数据读取：
 *    - 支持逐笔成交数据读取
 *    - 支持逐笔委托数据读取
 *    - 支持委托队列数据读取
 * 
 * 技术特点：
 * - 使用BoostMappingFile实现高效的文件映射
 * - 支持多种数据格式的自动识别
 * - 提供线程安全的数据读取
 * - 支持大数据量的流式读取
 * 
 * 使用场景：
 * - 回测引擎数据加载
 * - 历史数据分析和研究
 * - 数据格式转换和迁移
 * - 自定义数据处理程序
 * 
 * 注意事项：
 * - 需要正确配置数据存储路径
 * - 支持的数据格式需要与存储格式匹配
 * - 大数据量读取时需要注意内存使用
 */

#pragma once                                                    // 防止头文件重复包含
#include <string>                                              // 字符串处理
#include <stdint.h>                                            // 标准整数类型定义

#include "DataDefine.h"                                         // 数据存储格式定义

#include "../Includes/FasterDefs.h"                            // 性能优化定义
#include "../Includes/IBtDtReader.h"                           // 回测数据读取器接口

#include "../Share/BoostMappingFile.hpp"                       // Boost内存映射文件
#include "../Share/StdUtils.hpp"                               // 标准工具函数

NS_WTP_BEGIN                                                   // WonderTrader命名空间开始

// ===== 前向声明 =====
class WTSVariant;                                              // 变体数据类型
class WTSTickSlice;                                            // Tick数据切片
class WTSKlineSlice;                                           // K线数据切片
class WTSOrdDtlSlice;                                          // 逐笔委托数据切片
class WTSOrdQueSlice;                                          // 委托队列数据切片
class WTSTransSlice;                                           // 逐笔成交数据切片
class WTSArray;                                                // 数组类型

class IBaseDataMgr;                                            // 基础数据管理器接口
class IHotMgr;                                                 // 热力管理器接口
typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;         // Boost内存映射文件智能指针

/*!
 * \class WtBtDtReader
 * \brief WonderTrader回测数据读取器
 * 
 * 该类实现了IBtDtReader接口，提供了从WonderTrader数据存储格式中读取原始数据的功能。
 * 主要用于回测引擎的数据加载，支持多种数据类型的读取和格式转换。
 * 
 * 主要功能：
 * 1. 读取K线数据（1分钟、5分钟、日线等）
 * 2. 读取Tick数据（按日期）
 * 3. 读取逐笔成交数据
 * 4. 读取逐笔委托数据
 * 5. 读取委托队列数据
 * 
 * 技术特点：
 * - 使用内存映射文件技术提高读取性能
 * - 支持多种数据格式的自动识别
 * - 提供原始数据缓冲区访问
 * - 支持大数据量的流式读取
 * 
 * 使用场景：
 * - 回测引擎数据加载
 * - 历史数据分析和研究
 * - 数据格式转换和迁移
 * - 自定义数据处理程序
 */
class WtBtDtReader : public IBtDtReader
{
public:
	WtBtDtReader();                                             // 构造函数
	virtual ~WtBtDtReader();                                    // 析构函数

//////////////////////////////////////////////////////////////////////////
//IBtDtReader接口实现
public:
	/*!
	 * \brief 初始化回测数据读取器
	 * \param cfg 配置参数（包含数据存储路径等）
	 * \param sink 数据读取回调接口
	 */
	virtual void init(WTSVariant* cfg, IBtDtReaderSink* sink);

	/*!
	 * \brief 读取原始K线数据
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \param period K线周期
	 * \param buffer 输出缓冲区
	 * \return 是否读取成功
	 */
	virtual bool read_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, std::string& buffer) override;

	/*!
	 * \brief 读取原始Tick数据
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \param uDate 交易日期
	 * \param buffer 输出缓冲区
	 * \return 是否读取成功
	 */
	virtual bool read_raw_ticks(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) override;

	/*!
	 * \brief 读取原始逐笔委托数据
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \param uDate 交易日期
	 * \param buffer 输出缓冲区
	 * \return 是否读取成功
	 */
	virtual bool read_raw_order_details(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) override;

	/*!
	 * \brief 读取原始委托队列数据
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \param uDate 交易日期
	 * \param buffer 输出缓冲区
	 * \return 是否读取成功
	 */
	virtual bool read_raw_order_queues(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) override;

	/*!
	 * \brief 读取原始逐笔成交数据
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \param uDate 交易日期
	 * \param buffer 输出缓冲区
	 * \return 是否读取成功
	 */
	virtual bool read_raw_transactions(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) override;

private:
	std::string		_base_dir;                                     // 数据存储基础目录路径
};

NS_WTP_END                                                      // WonderTrader命名空间结束