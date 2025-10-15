/*!
 * \file WtDataReader.h
 * \project WonderTrader
 * 
 * \brief WonderTrader数据读取器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataReader类，是WonderTrader框架中用于数据读取的核心组件。
 * 该类实现了IDataReader接口，提供了从WonderTrader数据存储格式中读取各种数据的功能，
 * 支持实时数据和历史数据的读取，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 核心设计理念：
 * 
 * 1. 统一数据访问（Unified Data Access）：
 *    - 提供统一的数据读取接口
 *    - 支持实时数据和历史数据的统一访问
 *    - 隐藏底层数据存储格式的复杂性
 * 
 * 2. 高效数据管理（Efficient Data Management）：
 *    - 使用内存映射文件技术提高读取性能
 *    - 支持数据缓存和预加载
 *    - 提供数据切片和范围查询功能
 * 
 * 3. 灵活数据格式（Flexible Data Format）：
 *    - 支持多种数据格式的自动识别
 *    - 提供数据格式转换和适配
 *    - 支持复权数据的处理
 * 
 * 主要功能：
 * 
 * 1. 实时数据读取：
 *    - 支持实时K线数据读取
 *    - 支持实时Tick数据读取
 *    - 支持实时逐笔数据读取
 * 
 * 2. 历史数据读取：
 *    - 支持历史K线数据读取
 *    - 支持历史Tick数据读取
 *    - 支持历史逐笔数据读取
 * 
 * 3. 数据切片功能：
 *    - 支持按时间范围读取数据
 *    - 支持按数量读取数据
 *    - 支持数据切片和分页
 * 
 * 技术特点：
 * - 使用BoostMappingFile实现高效的文件映射
 * - 支持多种数据格式的自动识别和转换
 * - 提供线程安全的数据读取
 * - 支持大数据量的流式读取和缓存
 * 
 * 使用场景：
 * - 策略回测数据加载
 * - 实时行情数据访问
 * - 历史数据分析和研究
 * - 数据格式转换和迁移
 * 
 * 注意事项：
 * - 需要正确配置数据存储路径
 * - 支持的数据格式需要与存储格式匹配
 * - 大数据量读取时需要注意内存使用
 * - 实时数据读取需要考虑数据更新频率
 */

#pragma once                                                    // 防止头文件重复包含
#include <string>                                              // 字符串处理
#include <stdint.h>                                            // 标准整数类型定义

#include "DataDefine.h"                                         // 数据存储格式定义

#include "../Includes/FasterDefs.h"                            // 性能优化定义
#include "../Includes/IDataReader.h"                           // 数据读取器接口

#include "../Share/BoostMappingFile.hpp"                       // Boost内存映射文件

NS_WTP_BEGIN                                                   // WonderTrader命名空间开始

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;         // Boost内存映射文件智能指针

/*!
 * \class WtDataReader
 * \brief WonderTrader数据读取器
 * 
 * 该类实现了IDataReader接口，提供了从WonderTrader数据存储格式中读取各种数据的功能。
 * 支持实时数据和历史数据的读取，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 主要功能：
 * 1. 实时数据读取（K线、Tick、逐笔数据等）
 * 2. 历史数据读取（历史K线、历史Tick、历史逐笔数据等）
 * 3. 数据切片功能（按时间范围、按数量等）
 * 4. 复权数据处理
 * 5. 数据格式转换和适配
 * 
 * 技术特点：
 * - 使用内存映射文件技术提高读取性能
 * - 支持多种数据格式的自动识别和转换
 * - 提供线程安全的数据读取
 * - 支持大数据量的流式读取和缓存
 * 
 * 使用场景：
 * - 策略回测数据加载
 * - 实时行情数据访问
 * - 历史数据分析和研究
 * - 数据格式转换和迁移
 */
class WtDataReader : public IDataReader
{
public:
	WtDataReader();                                             // 构造函数
	virtual ~WtDataReader();                                    // 析构函数

private:
	/*!
	 * \struct _RTKBlockPair
	 * \brief 实时K线数据块对结构
	 * 
	 * 该结构体用于管理实时K线数据块，包含数据块指针、文件映射和容量信息。
	 * 用于实时K线数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：实时K线数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 */
	typedef struct _RTKBlockPair
	{
		RTKlineBlock*	_block;                                     // 实时K线数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_RTKBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
		}

	} RTKlineBlockPair;
	typedef wt_hashmap<std::string, RTKlineBlockPair>	RTKBlockFilesMap;  // 实时K线数据块文件映射表

	/*!
	 * \struct _TBlockPair
	 * \brief 实时Tick数据块对结构
	 * 
	 * 该结构体用于管理实时Tick数据块，包含数据块指针、文件映射和容量信息。
	 * 用于实时Tick数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：实时Tick数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 */
	typedef struct _TBlockPair
	{
		RTTickBlock*	_block;                                     // 实时Tick数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_TBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
		}
	} TickBlockPair;
	typedef wt_hashmap<std::string, TickBlockPair>	TBlockFilesMap;  // 实时Tick数据块文件映射表

	/*!
	 * \struct _TransBlockPair
	 * \brief 实时逐笔成交数据块对结构
	 * 
	 * 该结构体用于管理实时逐笔成交数据块，包含数据块指针、文件映射、容量信息和文件流。
	 * 用于实时逐笔成交数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：实时逐笔成交数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _fstream：文件流智能指针（用于数据写入）
	 */
	typedef struct _TransBlockPair
	{
		RTTransBlock*	_block;                                     // 实时逐笔成交数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）

		std::shared_ptr< std::ofstream>	_fstream;                   // 文件流智能指针（用于数据写入）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_TransBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
		}
	} TransBlockPair;
	typedef wt_hashmap<std::string, TransBlockPair>	TransBlockFilesMap;  // 实时逐笔成交数据块文件映射表

	/*!
	 * \struct _OdrDtlBlockPair
	 * \brief 实时逐笔委托数据块对结构
	 * 
	 * 该结构体用于管理实时逐笔委托数据块，包含数据块指针、文件映射、容量信息和文件流。
	 * 用于实时逐笔委托数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：实时逐笔委托数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _fstream：文件流智能指针（用于数据写入）
	 */
	typedef struct _OdrDtlBlockPair
	{
		RTOrdDtlBlock*	_block;                                     // 实时逐笔委托数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）

		std::shared_ptr< std::ofstream>	_fstream;                   // 文件流智能指针（用于数据写入）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_OdrDtlBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
		}
	} OrdDtlBlockPair;
	typedef wt_hashmap<std::string, OrdDtlBlockPair>	OrdDtlBlockFilesMap;  // 实时逐笔委托数据块文件映射表

	/*!
	 * \struct _OdrQueBlockPair
	 * \brief 实时委托队列数据块对结构
	 * 
	 * 该结构体用于管理实时委托队列数据块，包含数据块指针、文件映射、容量信息和文件流。
	 * 用于实时委托队列数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：实时委托队列数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _fstream：文件流智能指针（用于数据写入）
	 */
	typedef struct _OdrQueBlockPair
	{
		RTOrdQueBlock*	_block;                                     // 实时委托队列数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）

		std::shared_ptr< std::ofstream>	_fstream;                   // 文件流智能指针（用于数据写入）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_OdrQueBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
		}
	} OrdQueBlockPair;
	typedef wt_hashmap<std::string, OrdQueBlockPair>	OrdQueBlockFilesMap;  // 实时委托队列数据块文件映射表

	// ===== 实时数据块映射表 =====
	RTKBlockFilesMap	_rt_min1_map;                               // 实时1分钟K线数据块映射表
	RTKBlockFilesMap	_rt_min5_map;                               // 实时5分钟K线数据块映射表

	TBlockFilesMap		_rt_tick_map;                               // 实时Tick数据块映射表
	TransBlockFilesMap	_rt_trans_map;                               // 实时逐笔成交数据块映射表
	OrdDtlBlockFilesMap	_rt_orddtl_map;                              // 实时逐笔委托数据块映射表
	OrdQueBlockFilesMap	_rt_ordque_map;                              // 实时委托队列数据块映射表

	/*!
	 * \struct _HisTBlockPair
	 * \brief 历史Tick数据块对结构
	 * 
	 * 该结构体用于管理历史Tick数据块，包含数据块指针、日期和缓冲区。
	 * 用于历史Tick数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：历史Tick数据块指针
	 * - _date：交易日期（YYYYMMDD格式）
	 * - _buffer：数据缓冲区
	 */
	typedef struct _HisTBlockPair
	{
		HisTickBlock*	_block;                                     // 历史Tick数据块指针
		uint64_t		_date;                                      // 交易日期（YYYYMMDD格式）
		std::string		_buffer;                                    // 数据缓冲区

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_HisTBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_date = 0;                                              // 初始化日期为0
			_buffer.clear();                                        // 清空缓冲区
		}
	} HisTBlockPair;

	typedef wt_hashmap<std::string, HisTBlockPair>	HisTickBlockMap;  // 历史Tick数据块映射表

	/*!
	 * \struct _HisTransBlockPair
	 * \brief 历史逐笔成交数据块对结构
	 * 
	 * 该结构体用于管理历史逐笔成交数据块，包含数据块指针、日期和缓冲区。
	 * 用于历史逐笔成交数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：历史逐笔成交数据块指针
	 * - _date：交易日期（YYYYMMDD格式）
	 * - _buffer：数据缓冲区
	 */
	typedef struct _HisTransBlockPair
	{
		HisTransBlock*	_block;                                     // 历史逐笔成交数据块指针
		uint64_t		_date;                                      // 交易日期（YYYYMMDD格式）
		std::string		_buffer;                                    // 数据缓冲区

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_HisTransBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_date = 0;                                              // 初始化日期为0
			_buffer.clear();                                        // 清空缓冲区
		}
	} HisTransBlockPair;

	typedef wt_hashmap<std::string, HisTransBlockPair>	HisTransBlockMap;  // 历史逐笔成交数据块映射表

	/*!
	 * \struct _HisOrdDtlBlockPair
	 * \brief 历史逐笔委托数据块对结构
	 * 
	 * 该结构体用于管理历史逐笔委托数据块，包含数据块指针、日期和缓冲区。
	 * 用于历史逐笔委托数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：历史逐笔委托数据块指针
	 * - _date：交易日期（YYYYMMDD格式）
	 * - _buffer：数据缓冲区
	 */
	typedef struct _HisOrdDtlBlockPair
	{
		HisOrdDtlBlock*	_block;                                     // 历史逐笔委托数据块指针
		uint64_t		_date;                                      // 交易日期（YYYYMMDD格式）
		std::string		_buffer;                                    // 数据缓冲区

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_HisOrdDtlBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_date = 0;                                              // 初始化日期为0
			_buffer.clear();                                        // 清空缓冲区
		}
	} HisOrdDtlBlockPair;

	typedef wt_hashmap<std::string, HisOrdDtlBlockPair>	HisOrdDtlBlockMap;  // 历史逐笔委托数据块映射表

	/*!
	 * \struct _HisOrdQueBlockPair
	 * \brief 历史委托队列数据块对结构
	 * 
	 * 该结构体用于管理历史委托队列数据块，包含数据块指针、日期和缓冲区。
	 * 用于历史委托队列数据的读取和缓存管理。
	 * 
	 * 成员说明：
	 * - _block：历史委托队列数据块指针
	 * - _date：交易日期（YYYYMMDD格式）
	 * - _buffer：数据缓冲区
	 */
	typedef struct _HisOrdQueBlockPair
	{
		HisOrdQueBlock*	_block;                                     // 历史委托队列数据块指针
		uint64_t		_date;                                      // 交易日期（YYYYMMDD格式）
		std::string		_buffer;                                    // 数据缓冲区

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_HisOrdQueBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_date = 0;                                              // 初始化日期为0
			_buffer.clear();                                        // 清空缓冲区
		}
	} HisOrdQueBlockPair;

	typedef wt_hashmap<std::string, HisOrdQueBlockPair>	HisOrdQueBlockMap;  // 历史委托队列数据块映射表

	// ===== 历史数据块映射表 =====
	HisTickBlockMap		_his_tick_map;                               // 历史Tick数据块映射表
	HisOrdDtlBlockMap	_his_orddtl_map;                              // 历史逐笔委托数据块映射表
	HisOrdQueBlockMap	_his_ordque_map;                              // 历史委托队列数据块映射表
	HisTransBlockMap	_his_trans_map;                               // 历史逐笔成交数据块映射表

private:
	// ===== 实时数据块获取函数 =====
	/*!
	 * \brief 获取实时K线数据块
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \param period K线周期
	 * \return 实时K线数据块对指针
	 */
	RTKlineBlockPair* getRTKilneBlock(const char* exchg, const char* code, WTSKlinePeriod period);

	/*!
	 * \brief 获取实时Tick数据块
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \return 实时Tick数据块对指针
	 */
	TickBlockPair* getRTTickBlock(const char* exchg, const char* code);

	/*!
	 * \brief 获取实时委托队列数据块
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \return 实时委托队列数据块对指针
	 */
	OrdQueBlockPair* getRTOrdQueBlock(const char* exchg, const char* code);

	/*!
	 * \brief 获取实时逐笔委托数据块
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \return 实时逐笔委托数据块对指针
	 */
	OrdDtlBlockPair* getRTOrdDtlBlock(const char* exchg, const char* code);

	/*!
	 * \brief 获取实时逐笔成交数据块
	 * \param exchg 交易所代码
	 * \param code 合约代码
	 * \return 实时逐笔成交数据块对指针
	 */
	TransBlockPair* getRTTransBlock(const char* exchg, const char* code);

	// ===== 数据缓存函数 =====
	/*!
	 * \brief 缓存整合K线数据
	 * \param codeInfo 合约信息
	 * \param key 缓存键
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \return 是否缓存成功
	 */
	bool	cacheIntegratedBars(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period);

	/*!
	 * \brief 缓存复权股票K线数据
	 * \param codeInfo 合约信息
	 * \param key 缓存键
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \return 是否缓存成功
	 */
	bool	cacheAdjustedStkBars(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period);

	/*!
	 * \brief 从文件缓存历史K线数据
	 * \param codeInfo 合约信息
	 * \param key 缓存键
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \return 是否缓存成功
	 */
	bool	cacheHisBarsFromFile(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period);

	/*!
	 * \brief 从加载器缓存最终K线数据
	 * \param codeInfo 合约信息
	 * \param key 缓存键
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \return 是否缓存成功
	 */
	bool	cacheFinalBarsFromLoader(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period);

	// ===== 复权因子加载函数 =====
	/*!
	 * \brief 从文件加载股票复权因子
	 * \param adjfile 复权因子文件路径
	 * \return 是否加载成功
	 */
	bool	loadStkAdjFactorsFromFile(const char* adjfile);

	/*!
	 * \brief 从加载器加载股票复权因子
	 * \return 是否加载成功
	 */
	bool	loadStkAdjFactorsFromLoader();

public:
	// ===== IDataReader接口实现 =====
	/*!
	 * \brief 初始化数据读取器
	 * \param cfg 配置参数
	 * \param sink 数据读取回调接口
	 * \param loader 历史数据加载器（可选）
	 */
	virtual void init(WTSVariant* cfg, IDataReaderSink* sink, IHisDataLoader* loader = NULL) override;

	/*!
	 * \brief 分钟结束回调
	 * \param uDate 交易日期
	 * \param uTime 结束时间
	 * \param endTDate 结束交易日期（可选）
	 */
	virtual void onMinuteEnd(uint32_t uDate, uint32_t uTime, uint32_t endTDate = 0) override;

	/*!
	 * \brief 读取Tick数据切片
	 * \param stdCode 标准合约代码
	 * \param count 数据数量
	 * \param etime 结束时间（可选）
	 * \return Tick数据切片指针
	 */
	virtual WTSTickSlice*	readTickSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;

	/*!
	 * \brief 读取逐笔委托数据切片
	 * \param stdCode 标准合约代码
	 * \param count 数据数量
	 * \param etime 结束时间（可选）
	 * \return 逐笔委托数据切片指针
	 */
	virtual WTSOrdDtlSlice*	readOrdDtlSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;

	/*!
	 * \brief 读取委托队列数据切片
	 * \param stdCode 标准合约代码
	 * \param count 数据数量
	 * \param etime 结束时间（可选）
	 * \return 委托队列数据切片指针
	 */
	virtual WTSOrdQueSlice*	readOrdQueSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;

	/*!
	 * \brief 读取逐笔成交数据切片
	 * \param stdCode 标准合约代码
	 * \param count 数据数量
	 * \param etime 结束时间（可选）
	 * \return 逐笔成交数据切片指针
	 */
	virtual WTSTransSlice*	readTransSlice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;

	/*!
	 * \brief 读取K线数据切片
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \param count 数据数量
	 * \param etime 结束时间（可选）
	 * \return K线数据切片指针
	 */
	virtual WTSKlineSlice*	readKlineSlice(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime = 0) override;

	/*!
	 * \brief 获取指定日期的复权因子
	 * \param stdCode 标准合约代码
	 * \param date 交易日期（可选，默认为0）
	 * \return 复权因子
	 */
	virtual double getAdjFactorByDate(const char* stdCode, uint32_t date = 0) override;

	/*!
	 * \brief 获取复权标记
	 * \return 复权标记（位运算表示）
	 */
	virtual uint32_t getAdjustingFlag() override { return _adjust_flag; }

private:
	// ===== 成员变量 =====
	std::string		_rt_dir;                                      // 实时数据存储目录
	std::string		_his_dir;                                     // 历史数据存储目录
	IBaseDataMgr*	_base_data_mgr;                               // 基础数据管理器指针
	IHotMgr*		_hot_mgr;                                     // 热力管理器指针

	//By Wesley @ 2022.08.15
	//复权标记，采用位运算表示，1|2|4,1表示成交量复权，2表示成交额复权，4表示总持复权，其他待定
	uint32_t		_adjust_flag;                                 // 复权标记（位运算表示）

	/*!
	 * \struct _BarsList
	 * \brief K线数据列表结构
	 * 
	 * 该结构体用于管理K线数据列表，包含交易所、合约代码、周期、光标位置、原始代码、
	 * K线数据向量和复权因子等信息。
	 * 
	 * 成员说明：
	 * - _exchg：交易所代码
	 * - _code：合约代码
	 * - _period：K线周期
	 * - _rt_cursor：实时数据光标位置
	 * - _raw_code：原始合约代码
	 * - _bars：K线数据向量
	 * - _factor：复权因子
	 */
	typedef struct _BarsList
	{
		std::string		_exchg;                                     // 交易所代码
		std::string		_code;                                      // 合约代码
		WTSKlinePeriod	_period;                                    // K线周期
		uint32_t		_rt_cursor;                                 // 实时数据光标位置
		std::string		_raw_code;                                  // 原始合约代码

		std::vector<WTSBarStruct>	_bars;                          // K线数据向量
		double			_factor;                                    // 复权因子

		/*!
		 * \brief 构造函数
		 * 初始化光标位置为最大值，复权因子为最大值
		 */
		_BarsList() :_rt_cursor(UINT_MAX), _factor(DBL_MAX){}       // 初始化光标位置和复权因子
	} BarsList;

	typedef wt_hashmap<std::string, BarsList> BarsCache;            // K线数据缓存映射表
	BarsCache	_bars_cache;                                       // K线数据缓存

	uint64_t	_last_time;                                        // 最后时间戳

	/*!
	 * \struct _AdjFactor
	 * \brief 复权因子结构
	 * 
	 * 该结构体用于存储复权因子信息，包含日期和复权因子值。
	 * 
	 * 成员说明：
	 * - _date：交易日期
	 * - _factor：复权因子值
	 */
	typedef struct _AdjFactor
	{
		uint32_t	_date;                                          // 交易日期
		double		_factor;                                        // 复权因子值
	} AdjFactor;
	typedef std::vector<AdjFactor> AdjFactorList;                   // 复权因子列表
	typedef wt_hashmap<std::string, AdjFactorList>	AdjFactorMap;  // 复权因子映射表
	AdjFactorMap	_adj_factors;                                   // 复权因子映射表

	/*!
	 * \brief 获取复权因子列表
	 * \param code 合约代码
	 * \param exchg 交易所代码
	 * \param pid 产品ID
	 * \return 复权因子列表引用
	 */
	const AdjFactorList& getAdjFactors(const char* code, const char* exchg, const char* pid);
	
};

NS_WTP_END