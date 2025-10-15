/*!
 * \file WtRdmDtReader.h
 * \project WonderTrader
 * 
 * \brief WonderTrader随机数据读取器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtRdmDtReader类，是WonderTrader框架中用于随机数据读取的核心组件。
 * 该类实现了IRdmDtReader接口，提供了从WonderTrader数据存储格式中随机读取各种数据的功能，
 * 支持按时间范围、按数量、按日期等多种读取方式，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 核心设计理念：
 * 
 * 1. 随机数据访问（Random Data Access）：
 *    - 支持按时间范围随机读取数据
 *    - 支持按数量随机读取数据
 *    - 支持按日期随机读取数据
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
 * 1. 按时间范围读取：
 *    - 支持按时间范围读取K线数据
 *    - 支持按时间范围读取Tick数据
 *    - 支持按时间范围读取逐笔数据
 * 
 * 2. 按数量读取：
 *    - 支持按数量读取K线数据
 *    - 支持按数量读取Tick数据
 *    - 支持按数量读取逐笔数据
 * 
 * 3. 按日期读取：
 *    - 支持按日期读取Tick数据
 *    - 支持按日期读取逐笔数据
 *    - 支持按日期读取委托数据
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
 * - 随机读取需要考虑数据定位性能
 */

#pragma once                                                    // 防止头文件重复包含
#include <string>                                              // 字符串处理
#include <stdint.h>                                            // 标准整数类型定义
#include <unordered_map>                                        // 无序映射容器

#include "DataDefine.h"                                         // 数据存储格式定义

#include "../Includes/FasterDefs.h"                            // 性能优化定义
#include "../Includes/IRdmDtReader.h"                           // 随机数据读取器接口

#include "../Share/BoostMappingFile.hpp"                       // Boost内存映射文件
#include "../Share/StdUtils.hpp"                               // 标准工具函数
#include "../Share/fmtlib.h"                                   // 格式化库

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
 * \class WtRdmDtReader
 * \brief WonderTrader随机数据读取器
 * 
 * 该类实现了IRdmDtReader接口，提供了从WonderTrader数据存储格式中随机读取各种数据的功能。
 * 支持按时间范围、按数量、按日期等多种读取方式，包括K线、Tick、逐笔成交、逐笔委托、委托队列等数据类型。
 * 
 * 主要功能：
 * 1. 按时间范围读取数据（K线、Tick、逐笔数据等）
 * 2. 按数量读取数据（K线、Tick、逐笔数据等）
 * 3. 按日期读取数据（Tick、逐笔数据等）
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
class WtRdmDtReader : public IRdmDtReader
{
public:
	WtRdmDtReader();                                           // 构造函数
	virtual ~WtRdmDtReader();                                  // 析构函数

private:
	/*!
	 * \struct _RTKBlockPair
	 * \brief 实时K线数据块对结构
	 * 
	 * 该结构体用于管理实时K线数据块，包含互斥锁、数据块指针、文件映射和容量信息。
	 * 用于实时K线数据的读取和缓存管理，支持线程安全的数据访问。
	 * 
	 * 成员说明：
	 * - _mtx：互斥锁指针，用于线程安全访问
	 * - _block：实时K线数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _last_time：最后访问时间（用于缓存管理）
	 */
	typedef struct _RTKBlockPair
	{
		StdUniqueMutex*	_mtx;                                       // 互斥锁指针，用于线程安全访问
		RTKlineBlock*	_block;                                     // 实时K线数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）
		uint64_t		_last_time;                                 // 最后访问时间（用于缓存管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_RTKBlockPair()
		{
			_mtx = new StdUniqueMutex();                            // 创建互斥锁
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
			_last_time = 0;                                         // 初始化时间为0
		}
		/*!
		 * \brief 析构函数
		 * 释放互斥锁资源
		 */
		~_RTKBlockPair() { delete _mtx; }                           // 释放互斥锁

	} RTKlineBlockPair;
	typedef std::unordered_map<std::string, RTKlineBlockPair>	RTKBlockFilesMap;  // 实时K线数据块文件映射表

	/*!
	 * \struct _TBlockPair
	 * \brief 实时Tick数据块对结构
	 * 
	 * 该结构体用于管理实时Tick数据块，包含互斥锁、数据块指针、文件映射和容量信息。
	 * 用于实时Tick数据的读取和缓存管理，支持线程安全的数据访问。
	 * 
	 * 成员说明：
	 * - _mtx：互斥锁指针，用于线程安全访问
	 * - _block：实时Tick数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _last_time：最后访问时间（用于缓存管理）
	 */
	typedef struct _TBlockPair
	{
		StdUniqueMutex*	_mtx;                                       // 互斥锁指针，用于线程安全访问
		RTTickBlock*	_block;                                     // 实时Tick数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）
		uint64_t		_last_time;                                 // 最后访问时间（用于缓存管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_TBlockPair()
		{
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
			_last_time = 0;                                         // 初始化时间为0
			_mtx = new StdUniqueMutex();                            // 创建互斥锁
		}
		/*!
		 * \brief 析构函数
		 * 释放互斥锁资源
		 */
		~_TBlockPair() { delete _mtx; }                             // 释放互斥锁
	} TickBlockPair;
	typedef std::unordered_map<std::string, TickBlockPair>	TBlockFilesMap;  // 实时Tick数据块文件映射表

	/*!
	 * \struct _TransBlockPair
	 * \brief 实时逐笔成交数据块对结构
	 * 
	 * 该结构体用于管理实时逐笔成交数据块，包含互斥锁、数据块指针、文件映射和容量信息。
	 * 用于实时逐笔成交数据的读取和缓存管理，支持线程安全的数据访问。
	 * 
	 * 成员说明：
	 * - _mtx：互斥锁指针，用于线程安全访问
	 * - _block：实时逐笔成交数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _last_time：最后访问时间（用于缓存管理）
	 */
	typedef struct _TransBlockPair
	{
		StdUniqueMutex*	_mtx;                                       // 互斥锁指针，用于线程安全访问
		RTTransBlock*	_block;                                     // 实时逐笔成交数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）
		uint64_t		_last_time;                                 // 最后访问时间（用于缓存管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_TransBlockPair()
		{
			_mtx = new StdUniqueMutex();                            // 创建互斥锁
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
			_last_time = 0;                                         // 初始化时间为0
		}
		/*!
		 * \brief 析构函数
		 * 释放互斥锁资源
		 */
		~_TransBlockPair() { delete _mtx; }                         // 释放互斥锁
	} TransBlockPair;
	typedef std::unordered_map<std::string, TransBlockPair>	TransBlockFilesMap;  // 实时逐笔成交数据块文件映射表

	/*!
	 * \struct _OdeDtlBlockPair
	 * \brief 实时逐笔委托数据块对结构
	 * 
	 * 该结构体用于管理实时逐笔委托数据块，包含互斥锁、数据块指针、文件映射和容量信息。
	 * 用于实时逐笔委托数据的读取和缓存管理，支持线程安全的数据访问。
	 * 
	 * 成员说明：
	 * - _mtx：互斥锁指针，用于线程安全访问
	 * - _block：实时逐笔委托数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _last_time：最后访问时间（用于缓存管理）
	 */
	typedef struct _OdeDtlBlockPair
	{
		StdUniqueMutex*	_mtx;                                       // 互斥锁指针，用于线程安全访问
		RTOrdDtlBlock*	_block;                                     // 实时逐笔委托数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）
		uint64_t		_last_time;                                 // 最后访问时间（用于缓存管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_OdeDtlBlockPair()
		{
			_mtx = new StdUniqueMutex();                            // 创建互斥锁
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
			_last_time = 0;                                         // 初始化时间为0
		}
		/*!
		 * \brief 析构函数
		 * 释放互斥锁资源
		 */
		~_OdeDtlBlockPair() { delete _mtx; }                       // 释放互斥锁
	} OrdDtlBlockPair;
	typedef std::unordered_map<std::string, OrdDtlBlockPair>	OrdDtlBlockFilesMap;  // 实时逐笔委托数据块文件映射表

	/*!
	 * \struct _OdeQueBlockPair
	 * \brief 实时委托队列数据块对结构
	 * 
	 * 该结构体用于管理实时委托队列数据块，包含互斥锁、数据块指针、文件映射和容量信息。
	 * 用于实时委托队列数据的读取和缓存管理，支持线程安全的数据访问。
	 * 
	 * 成员说明：
	 * - _mtx：互斥锁指针，用于线程安全访问
	 * - _block：实时委托队列数据块指针
	 * - _file：内存映射文件智能指针
	 * - _last_cap：最后容量（用于容量管理）
	 * - _last_time：最后访问时间（用于缓存管理）
	 */
	typedef struct _OdeQueBlockPair
	{
		StdUniqueMutex*	_mtx;                                       // 互斥锁指针，用于线程安全访问
		RTOrdQueBlock*	_block;                                     // 实时委托队列数据块指针
		BoostMFPtr		_file;                                       // 内存映射文件智能指针
		uint64_t		_last_cap;                                  // 最后容量（用于容量管理）
		uint64_t		_last_time;                                 // 最后访问时间（用于缓存管理）

		/*!
		 * \brief 构造函数
		 * 初始化所有成员为默认值
		 */
		_OdeQueBlockPair()
		{
			_mtx = new StdUniqueMutex();                            // 创建互斥锁
			_block = NULL;                                          // 初始化数据块指针为空
			_file = NULL;                                           // 初始化文件指针为空
			_last_cap = 0;                                          // 初始化容量为0
			_last_time = 0;                                         // 初始化时间为0
		}
		/*!
		 * \brief 析构函数
		 * 释放互斥锁资源
		 */
		~_OdeQueBlockPair() { delete _mtx; }                        // 释放互斥锁
	} OrdQueBlockPair;
	typedef std::unordered_map<std::string, OrdQueBlockPair>	OrdQueBlockFilesMap;  // 实时委托队列数据块文件映射表

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

	typedef std::unordered_map<std::string, HisTBlockPair>	HisTickBlockMap;  // 历史Tick数据块映射表

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

	typedef std::unordered_map<std::string, HisTransBlockPair>	HisTransBlockMap;  // 历史逐笔成交数据块映射表

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

	typedef std::unordered_map<std::string, HisOrdDtlBlockPair>	HisOrdDtlBlockMap;  // 历史逐笔委托数据块映射表

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

	typedef std::unordered_map<std::string, HisOrdQueBlockPair>	HisOrdQueBlockMap;  // 历史委托队列数据块映射表

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
	 * \brief 从文件缓存历史K线数据
	 * \param codeInfo 合约信息
	 * \param key 缓存键
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \return 是否缓存成功
	 */
	bool		cacheHisBarsFromFile(void* codeInfo, const std::string& key, const char* stdCode, WTSKlinePeriod period);

	/*!
	 * \brief 从缓存按时间范围读取K线数据
	 * \param key 缓存键
	 * \param stime 开始时间
	 * \param etime 结束时间
	 * \param ayBars K线数据向量
	 * \param isDay 是否为日线数据
	 * \return 读取的数据条数
	 */
	uint32_t		readBarsFromCacheByRange(const std::string& key, uint64_t stime, uint64_t etime, std::vector<WTSBarStruct>& ayBars, bool isDay = false);
	
	/*!
	 * \brief 从缓存按时间范围索引K线数据
	 * \param key 缓存键
	 * \param stime 开始时间
	 * \param etime 结束时间
	 * \param count 数据条数（输出参数）
	 * \param isDay 是否为日线数据
	 * \return K线数据指针
	 */
	WTSBarStruct*	indexBarFromCacheByRange(const std::string& key, uint64_t stime, uint64_t etime, uint32_t& count, bool isDay = false);

	/*!
	 * \brief 从缓存按数量索引K线数据
	 * \param key 缓存键
	 * \param etime 结束时间
	 * \param count 数据条数（输入输出参数）
	 * \param isDay 是否为日线数据
	 * \return K线数据指针
	 */
	WTSBarStruct*	indexBarFromCacheByCount(const std::string& key, uint64_t etime, uint32_t& count, bool isDay = false);

	/*!
	 * \brief 从文件加载股票复权因子
	 * \param adjfile 复权因子文件路径
	 * \return 是否加载成功
	 */
	bool	loadStkAdjFactorsFromFile(const char* adjfile);
	

//////////////////////////////////////////////////////////////////////////
//IRdmDtReader接口实现
public:
	/*!
	 * \brief 初始化随机数据读取器
	 * \param cfg 配置参数
	 * \param sink 数据读取回调接口
	 */
	virtual void init(WTSVariant* cfg, IRdmDtReaderSink* sink);

	// ===== 按时间范围读取数据接口 =====
	/*!
	 * \brief 按时间范围读取逐笔委托数据切片
	 * \param stdCode 标准合约代码
	 * \param stime 开始时间
	 * \param etime 结束时间（可选，默认为0）
	 * \return 逐笔委托数据切片指针
	 */
	virtual WTSOrdDtlSlice*	readOrdDtlSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override;
	
	/*!
	 * \brief 按时间范围读取委托队列数据切片
	 * \param stdCode 标准合约代码
	 * \param stime 开始时间
	 * \param etime 结束时间（可选，默认为0）
	 * \return 委托队列数据切片指针
	 */
	virtual WTSOrdQueSlice*	readOrdQueSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override;
	
	/*!
	 * \brief 按时间范围读取逐笔成交数据切片
	 * \param stdCode 标准合约代码
	 * \param stime 开始时间
	 * \param etime 结束时间（可选，默认为0）
	 * \return 逐笔成交数据切片指针
	 */
	virtual WTSTransSlice*	readTransSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override;

	/*!
	 * \brief 按时间范围读取Tick数据切片
	 * \param stdCode 标准合约代码
	 * \param stime 开始时间
	 * \param etime 结束时间（可选，默认为0）
	 * \return Tick数据切片指针
	 */
	virtual WTSTickSlice*	readTickSliceByRange(const char* stdCode, uint64_t stime, uint64_t etime = 0) override;
	
	/*!
	 * \brief 按时间范围读取K线数据切片
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \param stime 开始时间
	 * \param etime 结束时间（可选，默认为0）
	 * \return K线数据切片指针
	 */
	virtual WTSKlineSlice*	readKlineSliceByRange(const char* stdCode, WTSKlinePeriod period, uint64_t stime, uint64_t etime = 0) override;

	// ===== 按数量读取数据接口 =====
	/*!
	 * \brief 按数量读取Tick数据切片
	 * \param stdCode 标准合约代码
	 * \param count 数据数量
	 * \param etime 结束时间（可选，默认为0）
	 * \return Tick数据切片指针
	 */
	virtual WTSTickSlice*	readTickSliceByCount(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	
	/*!
	 * \brief 按数量读取K线数据切片
	 * \param stdCode 标准合约代码
	 * \param period K线周期
	 * \param count 数据数量
	 * \param etime 结束时间（可选，默认为0）
	 * \return K线数据切片指针
	 */
	virtual WTSKlineSlice*	readKlineSliceByCount(const char* stdCode, WTSKlinePeriod period, uint32_t count, uint64_t etime = 0) override;

	// ===== 按日期读取数据接口 =====
	/*!
	 * \brief 按日期读取Tick数据切片
	 * \param stdCode 标准合约代码
	 * \param uDate 交易日期（可选，默认为0）
	 * \return Tick数据切片指针
	 */
	virtual WTSTickSlice*	readTickSliceByDate(const char* stdCode, uint32_t uDate = 0 ) override;

	// ===== 复权因子接口 =====
	/*!
	 * \brief 获取指定日期的复权因子
	 * \param stdCode 标准合约代码
	 * \param date 交易日期（可选，默认为0）
	 * \return 复权因子
	 */
	virtual double		getAdjFactorByDate(const char* stdCode, uint32_t date = 0) override;

	// ===== 缓存管理接口 =====
	/*!
	 * \brief 清空数据缓存
	 */
	virtual void		clearCache() override;

private:
	// ===== 基础配置成员变量 =====
	std::string		_base_dir;                                    // 数据存储基础目录
	IBaseDataMgr*	_base_data_mgr;                               // 基础数据管理器指针
	IHotMgr*		_hot_mgr;                                     // 热力管理器指针
	StdThreadPtr	_thrd_check;                                  // 检查线程指针
	bool			_stopped;                                     // 停止标志

	/*!
	 * \struct _BarsList
	 * \brief K线数据列表结构
	 * 
	 * 该结构体用于管理K线数据列表，包含交易所、合约代码、周期、原始代码、
	 * K线数据向量和复权因子等信息。
	 * 
	 * 成员说明：
	 * - _exchg：交易所代码
	 * - _code：合约代码
	 * - _period：K线周期
	 * - _raw_code：原始合约代码
	 * - _factor：复权因子
	 * - _bars：K线数据向量
	 * - _rt_bars：实时K线数据向量（如果是后复权，就需要把实时数据拷贝到这里来）
	 */
	typedef struct _BarsList
	{
		std::string		_exchg;                                     // 交易所代码
		std::string		_code;                                      // 合约代码
		WTSKlinePeriod	_period;                                    // K线周期
		std::string		_raw_code;                                  // 原始合约代码
		double			_factor;                                    // 复权因子

		/*!
		 * \brief 构造函数
		 * 初始化复权因子为1.0
		 */
		_BarsList():_factor(1.0){}                                  // 初始化复权因子为1.0

		std::vector<WTSBarStruct>	_bars;                          // K线数据向量
		std::vector<WTSBarStruct>	_rt_bars;                       // 实时K线数据向量（如果是后复权，就需要把实时数据拷贝到这里来）
	} BarsList;

	typedef std::unordered_map<std::string, BarsList> BarsCache;     // K线数据缓存映射表
	BarsCache	_bars_cache;                                       // K线数据缓存

	// ===== 复权因子相关结构 =====
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
	typedef std::unordered_map<std::string, AdjFactorList>	AdjFactorMap;  // 复权因子映射表
	AdjFactorMap	_adj_factors;                                   // 复权因子映射表

	/*!
	 * \brief 获取复权因子列表
	 * \param code 合约代码
	 * \param exchg 交易所代码
	 * \param pid 产品ID
	 * \return 复权因子列表引用
	 */
	inline const AdjFactorList& getAdjFactors(const char* code, const char* exchg, const char* pid)
	{
		thread_local static char key[20] = { 0 };                   // 线程本地存储的键缓冲区
		fmtutil::format_to(key, "{}.{}.{}", exchg, pid, code);      // 格式化键字符串
		return _adj_factors[key];                                   // 返回复权因子列表引用
	}
};

NS_WTP_END