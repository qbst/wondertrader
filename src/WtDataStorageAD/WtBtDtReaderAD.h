/*!
 * \file WtBtDtReaderAD.h
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块回测数据读取器头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtBtDtReaderAD类，这是WonderTrader框架中专门用于回测的高级数据
 * 读取器。该类基于LMDB数据库实现，为回测引擎提供高性能的历史数据访问服务。
 * 
 * 核心设计理念：
 * 
 * 1. 回测专用设计（Backtesting-Oriented Design）：
 *    - 专门为回测场景优化的数据读取接口
 *    - 支持大批量历史数据的高效加载
 *    - 提供原始数据格式的直接访问
 * 
 * 2. LMDB高性能存储（High-Performance LMDB Storage）：
 *    - 基于LMDB的零拷贝数据访问
 *    - 内存映射技术提供极高的读取性能
 *    - 支持并发读取，适合多策略回测
 * 
 * 3. 分层数据组织（Hierarchical Data Organization）：
 *    - 按交易所分组的K线数据库
 *    - 按合约分组的Tick数据库
 *    - 多周期数据的统一管理
 * 
 * 数据存储架构：
 * 
 * K线数据存储结构：
 * ├── min1/              (1分钟K线数据)
 * │   ├── SHFE/          (上期所数据库)
 * │   ├── DCE/           (大商所数据库)
 * │   └── CZCE/          (郑商所数据库)
 * ├── min5/              (5分钟K线数据)
 * │   ├── SHFE/
 * │   └── ...
 * └── day/               (日K线数据)
 *     ├── SHFE/
 *     └── ...
 * 
 * Tick数据存储结构：
 * └── ticks/             (Tick数据)
 *     ├── SHFE/          (上期所)
 *     │   ├── rb2305/    (螺纹钢2305合约)
 *     │   └── au2306/    (黄金2306合约)
 *     └── DCE/           (大商所)
 *         ├── i2305/     (铁矿石2305合约)
 *         └── ...
 * 
 * 接口设计特点：
 * 
 * 1. 原始数据访问（Raw Data Access）：
 *    - read_raw_bars()：直接读取原始K线数据
 *    - read_raw_ticks()：直接读取原始Tick数据
 *    - 避免数据转换开销，提高回测性能
 * 
 * 2. 缓存优化（Cache Optimization）：
 *    - 数据库连接池管理
 *    - 智能缓存策略
 *    - 减少重复数据库操作
 * 
 * 3. 错误处理（Error Handling）：
 *    - 完善的异常处理机制
 *    - 详细的错误日志记录
 *    - 优雅的降级处理
 * 
 * 使用场景：
 * - 策略回测的历史数据加载
 * - 大规模历史数据分析
 * - 多品种、多周期数据的批量处理
 * - 高频策略的Tick级别回测
 * 
 * 性能特点：
 * - 基于LMDB的极高读取性能
 * - 零拷贝的内存访问模式
 * - 支持大文件和大数据量处理
 * - 优化的数据库连接管理
 */

#pragma once                                    // 防止头文件重复包含
#include <string>                               // 引入字符串类

#include "../Includes/FasterDefs.h"             // 引入高性能数据结构定义
#include "../Includes/IBtDtReader.h"            // 引入回测数据读取器接口

#include "../WTSUtils/WtLMDB.hpp"               // 引入LMDB数据库封装类

NS_WTP_BEGIN                                    // 开始WonderTrader命名空间

/**
 * @class WtBtDtReaderAD
 * @brief WonderTrader高级回测数据读取器
 * 
 * 基于LMDB数据库的高性能回测数据读取器，专门为回测引擎提供历史数据访问服务。
 * 该类实现了IBtDtReader接口，支持K线和Tick数据的高效读取。
 * 
 * 主要特性：
 * 
 * 1. 高性能数据访问：
 *    - 基于LMDB内存映射数据库
 *    - 零拷贝数据读取机制
 *    - 支持大文件高速访问
 * 
 * 2. 多数据源支持：
 *    - 支持多个交易所的数据
 *    - 支持多种K线周期（1分钟、5分钟、日线等）
 *    - 支持Tick级别的高频数据
 * 
 * 3. 智能缓存管理：
 *    - 数据库连接池
 *    - 按需加载数据库
 *    - 自动资源管理
 * 
 * 典型使用流程：
 * @code
 *   // 1. 创建读取器实例
 *   WtBtDtReaderAD* reader = new WtBtDtReaderAD();
 *   
 *   // 2. 初始化配置
 *   WTSVariant* config = WTSVariant::createObject();
 *   config->append("path", "./data/");
 *   reader->init(config, sink);
 *   
 *   // 3. 读取K线数据
 *   std::string buffer;
 *   bool success = reader->read_raw_bars("SHFE", "rb2305", KP_Minute1, buffer);
 *   
 *   // 4. 读取Tick数据
 *   success = reader->read_raw_ticks("SHFE", "rb2305", 20230508, buffer);
 *   
 *   // 5. 清理资源
 *   delete reader;
 * @endcode
 */
class WtBtDtReaderAD : public IBtDtReader
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建WtBtDtReaderAD实例，初始化基本成员变量。
	 * 构造函数不执行任何重量级操作，实际的初始化在init()方法中完成。
	 */
	WtBtDtReaderAD();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，关闭所有打开的数据库连接。
	 * 析构函数会自动释放所有LMDB数据库资源。
	 */
	virtual ~WtBtDtReaderAD();	

//////////////////////////////////////////////////////////////////////////
// IBtDtReader接口实现
public:
	/**
	 * @brief 初始化回测数据读取器
	 * 
	 * 根据配置参数初始化数据读取器，设置数据存储路径和相关参数。
	 * 
	 * @param cfg 配置参数对象，包含以下配置项：
	 *            - path: 数据存储根目录路径
	 * @param sink 回调接口，用于日志输出和状态通知
	 * 
	 * 初始化过程：
	 * 1. 解析配置参数
	 * 2. 设置数据存储路径
	 * 3. 验证路径有效性
	 * 4. 输出初始化日志
	 */
	virtual void init(WTSVariant* cfg, IBtDtReaderSink* sink);

	/**
	 * @brief 读取原始K线数据
	 * 
	 * 从LMDB数据库中读取指定合约和周期的原始K线数据。
	 * 数据以二进制格式直接返回，避免序列化开销。
	 * 
	 * @param exchg 交易所代码（如"SHFE"、"DCE"、"CZCE"等）
	 * @param code 合约代码（如"rb2305"、"IF2303"等）
	 * @param period K线周期（KP_Minute1、KP_Minute5、KP_DAY等）
	 * @param buffer 输出缓冲区，存储读取的原始K线数据
	 * @return 读取成功返回true，失败返回false
	 * 
	 * 数据格式：
	 * - buffer中存储连续的WTSBarStruct结构体数组
	 * - 数据按时间顺序排列
	 * - 可直接转换为WTSBarStruct*使用
	 */
	virtual bool read_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, std::string& buffer) override;
	
	/**
	 * @brief 读取原始Tick数据
	 * 
	 * 从LMDB数据库中读取指定合约和日期的原始Tick数据。
	 * 数据以二进制格式直接返回，适合高频数据处理。
	 * 
	 * @param exchg 交易所代码（如"SHFE"、"DCE"、"CZCE"等）
	 * @param code 合约代码（如"rb2305"、"IF2303"等）
	 * @param uDate 交易日期（格式：YYYYMMDD，如20230508）
	 * @param buffer 输出缓冲区，存储读取的原始Tick数据
	 * @return 读取成功返回true，失败返回false
	 * 
	 * 数据格式：
	 * - buffer中存储连续的WTSTickStruct结构体数组
	 * - 数据按时间顺序排列
	 * - 可直接转换为WTSTickStruct*使用
	 */
	virtual bool read_raw_ticks(const char* exchg, const char* code, uint32_t uDate, std::string& buffer) override;

private:
	std::string		_base_dir;				///< 数据存储根目录路径

private:
	//////////////////////////////////////////////////////////////////////////
	// LMDB数据库管理
	/*
	 * LMDB数据库组织结构：
	 * 
	 * K线数据：按交易所和周期分组
	 * - 路径格式：{_base_dir}/{period}/{exchg}/
	 * - 示例：./data/min1/SHFE/、./data/day/DCE/
	 * 
	 * Tick数据：按交易所和合约分组  
	 * - 路径格式：{_base_dir}/ticks/{exchg}/{code}/
	 * - 示例：./data/ticks/SHFE/rb2305/、./data/ticks/DCE/i2305/
	 */
	
	typedef std::shared_ptr<WtLMDB> WtLMDBPtr;                  ///< LMDB数据库智能指针类型
	typedef wt_hashmap<std::string, WtLMDBPtr> WtLMDBMap;       ///< 数据库映射表类型

	WtLMDBMap	_exchg_m1_dbs;              ///< 1分钟K线数据库映射表（key: 交易所代码）
	WtLMDBMap	_exchg_m5_dbs;              ///< 5分钟K线数据库映射表（key: 交易所代码）
	WtLMDBMap	_exchg_d1_dbs;              ///< 日K线数据库映射表（key: 交易所代码）

	WtLMDBMap	_tick_dbs;                  ///< Tick数据库映射表（key: "交易所.合约"格式）

	/**
	 * @brief 获取K线数据库连接
	 * 
	 * 根据交易所和K线周期获取对应的LMDB数据库连接。
	 * 如果数据库尚未打开，会自动创建连接并加入缓存。
	 * 
	 * @param exchg 交易所代码
	 * @param period K线周期
	 * @return LMDB数据库智能指针，失败返回空指针
	 * 
	 * 缓存策略：
	 * - 首次访问时创建数据库连接
	 * - 后续访问直接从缓存中获取
	 * - 自动管理连接生命周期
	 */
	WtLMDBPtr	get_k_db(const char* exchg, WTSKlinePeriod period);

	/**
	 * @brief 获取Tick数据库连接
	 * 
	 * 根据交易所和合约代码获取对应的LMDB数据库连接。
	 * 如果数据库尚未打开，会自动创建连接并加入缓存。
	 * 
	 * @param exchg 交易所代码
	 * @param code 合约代码
	 * @return LMDB数据库智能指针，失败返回空指针
	 * 
	 * 键值格式：
	 * - 使用"交易所.合约"作为缓存键
	 * - 示例："SHFE.rb2305"、"DCE.i2305"
	 */
	WtLMDBPtr	get_t_db(const char* exchg, const char* code);
};

NS_WTP_END                                      // 结束WonderTrader命名空间