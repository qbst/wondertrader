/*!
 * \file DataDefineAD.h
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块数据结构定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataStorageAD（高级数据存储模块）中使用的核心数据结构。
 * WtDataStorageAD是WonderTrader框架中基于LMDB（Lightning Memory-Mapped Database）
 * 的高性能数据存储解决方案，专门用于处理实时行情数据的高速读写和缓存管理。
 * 
 * 核心设计理念：
 * 
 * 1. 内存映射缓存架构（Memory-Mapped Cache Architecture）：
 *    - 使用内存映射文件技术实现高性能数据缓存
 *    - 支持进程间共享的实时数据访问
 *    - 提供持久化的缓存数据结构
 * 
 * 2. 分层数据组织（Layered Data Organization）：
 *    - 块头（BlockHeader）：定义数据块的基本属性
 *    - 实时块头（RTBlockHeader）：扩展块头，增加容量管理
 *    - 数据项（CacheItem）：具体的数据存储单元
 * 
 * 3. 类型化缓存设计（Typed Cache Design）：
 *    - Tick缓存（RTTickCache）：存储实时逐笔行情数据
 *    - K线缓存（RTBarCache）：存储实时K线数据
 *    - 统一的缓存管理接口和数据格式
 * 
 * 数据结构层次关系：
 * 
 *     BlockHeader (基础块头)
 *         ↓
 *     RTBlockHeader (实时块头)
 *         ↓
 *   ┌─────────────────────┐
 *   │   RTTickCache       │   RTBarCache
 *   │  (Tick数据缓存)     │  (K线数据缓存)
 *   │                     │
 *   │  TickCacheItem[]    │  BarCacheItem[]
 *   │  - _date            │  - _exchg[16]
 *   │  - _tick            │  - _code[32]
 *   │                     │  - _bar
 *   └─────────────────────┘
 * 
 * 内存布局设计：
 * 
 * 1. 字节对齐（Byte Alignment）：
 *    - 使用#pragma pack(push, 1)确保结构体紧密排列
 *    - 避免编译器自动填充，保证跨平台兼容性
 *    - 提高内存使用效率和访问性能
 * 
 * 2. 标识符系统（Identifier System）：
 *    - BLK_FLAG：8字节魔数，用于验证数据块完整性
 *    - BlockType：枚举类型，标识不同类型的数据块
 *    - Version：版本号，支持数据格式的向后兼容
 * 
 * 3. 动态容量管理（Dynamic Capacity Management）：
 *    - _size：当前已使用的数据项数量
 *    - _capacity：数据块的最大容量
 *    - 支持运行时动态扩容
 * 
 * 使用场景：
 * - 实时行情数据的高速缓存
 * - 多进程间的数据共享
 * - 数据持久化和恢复
 * - 内存映射文件的数据组织
 * 
 * 性能特点：
 * - 零拷贝数据访问
 * - 内存映射的高速I/O
 * - 紧凑的内存布局
 * - 高效的缓存命中率
 */

#pragma once                                    // 防止头文件重复包含
#include "../Includes/WTSStruct.h"              // 引入WonderTrader基础数据结构定义

USING_NS_WTP;                                   // 使用WonderTrader命名空间

#pragma pack(push, 1)                          // 设置结构体按1字节对齐，确保内存布局紧凑

/**
 * @brief 数据块标识魔数
 * 
 * 8字节的魔数标识符，用于验证内存映射文件中数据块的完整性和有效性。
 * 在文件损坏或格式错误时，通过检查此魔数可以快速判断数据块是否有效。
 */
const char BLK_FLAG[] = "&^%$#@!\0";

/**
 * @brief 标识符长度常量
 * 
 * 定义BLK_FLAG标识符的字节长度，用于内存操作和验证时的长度检查。
 */
const int FLAG_SIZE = 8;

/**
 * @enum BlockType
 * @brief 数据块类型枚举
 * 
 * 定义不同类型的数据块标识符，用于区分内存映射文件中存储的数据类型。
 * 每种类型对应不同的数据结构和处理逻辑。
 */
typedef enum tagBlockType
{
	BT_RT_Cache			= 4		///< 实时缓存数据块类型，用于存储实时行情和K线数据
} BlockType;

/**
 * @brief 数据块版本号定义
 * 
 * 定义数据块的版本号，用于数据格式的版本控制和向后兼容性管理。
 * 当数据结构发生变化时，可以通过版本号进行格式转换和兼容性处理。
 */
#define BLOCK_VERSION_RAW	1	///< 普通版本号，表示标准的数据块格式

/**
 * @struct BlockHeader
 * @brief 数据块基础头部结构
 * 
 * 所有数据块的基础头部结构，包含数据块的基本标识信息。
 * 这是所有具体数据块类型的基类结构，提供统一的块识别机制。
 * 
 * 内存布局：
 * +------------------+------------------+------------------+
 * | _blk_flag (8B)   | _type (2B)       | _version (2B)    |
 * +------------------+------------------+------------------+
 * 
 * 使用场景：
 * - 数据块类型识别
 * - 文件格式验证
 * - 版本兼容性检查
 */
typedef struct _BlockHeader
{
	char		_blk_flag[FLAG_SIZE];	///< 数据块标识魔数，用于验证数据块的有效性
	uint16_t	_type;					///< 数据块类型，对应BlockType枚举值
	uint16_t	_version;				///< 数据块版本号，用于格式兼容性管理
} BlockHeader;

/**
 * @brief 数据块头部大小常量
 * 
 * 计算BlockHeader结构体的字节大小，用于内存操作和偏移计算。
 */
#define BLOCK_HEADER_SIZE	sizeof(BlockHeader)

/**
 * @struct RTBlockHeader
 * @brief 实时数据块头部结构
 * 
 * 继承自BlockHeader，扩展了容量管理功能的实时数据块头部。
 * 用于管理动态大小的数据数组，支持运行时扩容和容量监控。
 * 
 * 内存布局：
 * +------------------+------------------+------------------+------------------+------------------+
 * | _blk_flag (8B)   | _type (2B)       | _version (2B)    | _size (4B)       | _capacity (4B)   |
 * +------------------+------------------+------------------+------------------+------------------+
 * 
 * 容量管理逻辑：
 * - _size <= _capacity 始终成立
 * - 当_size接近_capacity时触发扩容
 * - 扩容时重新映射更大的内存区域
 */
typedef struct _RTBlockHeader : BlockHeader
{
	uint32_t _size;						///< 当前已使用的数据项数量
	uint32_t _capacity;					///< 数据块的最大容量（数据项数量）
} RTBlockHeader;

/**
 * @struct TickCacheItem
 * @brief Tick数据缓存项结构
 * 
 * 存储单个Tick行情数据的缓存项，包含交易日期和完整的Tick数据结构。
 * 用于实时Tick数据的内存缓存和快速访问。
 * 
 * 数据组织：
 * - _date：交易日期，用于数据分组和查询优化
 * - _tick：完整的Tick数据结构，包含价格、成交量等信息
 * 
 * 使用场景：
 * - 实时行情数据缓存
 * - 最新Tick数据查询
 * - 行情数据的临时存储
 */
typedef struct _TickCacheItem
{
	uint32_t		_date;				///< 交易日期（格式：YYYYMMDD）
	WTSTickStruct	_tick;				///< Tick行情数据结构
} TickCacheItem;

/**
 * @struct RTTickCache
 * @brief 实时Tick数据缓存结构
 * 
 * 继承自RTBlockHeader，用于管理实时Tick数据的内存缓存。
 * 采用变长数组设计，支持动态数量的Tick数据存储。
 * 
 * 内存布局：
 * +------------------+------------------+------------------+
 * | RTBlockHeader    | TickCacheItem[0] | TickCacheItem[1] | ...
 * +------------------+------------------+------------------+
 * 
 * 特点：
 * - 零长度数组设计，支持动态大小
 * - 连续内存布局，提高访问效率
 * - 内存映射友好，支持进程间共享
 */
typedef struct _RTTickCache : RTBlockHeader
{
	TickCacheItem	_items[0];			///< 变长数组，存储Tick缓存项数据
} RTTickCache;

/**
 * @struct BarCacheItem
 * @brief K线数据缓存项结构
 * 
 * 存储单个K线数据的缓存项，包含交易所代码、合约代码和完整的K线数据。
 * 用于实时K线数据的内存缓存和快速访问。
 * 
 * 数据组织：
 * - _exchg：交易所代码（如"SHFE"、"DCE"等）
 * - _code：合约代码（如"rb2305"、"IF2303"等）
 * - _bar：完整的K线数据结构
 * 
 * 索引策略：
 * - 通过"交易所.合约"组合键进行快速查找
 * - 支持多交易所、多合约的混合缓存
 */
typedef struct _BarCacheItem
{
	char			_exchg[16];			///< 交易所代码，固定16字节长度
	char			_code[32];			///< 合约代码，固定32字节长度
	WTSBarStruct	_bar;				///< K线数据结构
} BarCacheItem;

/**
 * @struct RTBarCache
 * @brief 实时K线数据缓存结构
 * 
 * 继承自RTBlockHeader，用于管理实时K线数据的内存缓存。
 * 采用变长数组设计，支持动态数量的K线数据存储。
 * 
 * 内存布局：
 * +------------------+------------------+------------------+
 * | RTBlockHeader    | BarCacheItem[0]  | BarCacheItem[1]  | ...
 * +------------------+------------------+------------------+
 * 
 * 应用场景：
 * - 1分钟K线实时缓存
 * - 5分钟K线实时缓存  
 * - 日K线实时缓存
 * - 多周期K线数据的统一管理
 */
typedef struct _RTBarCache : RTBlockHeader
{
	BarCacheItem	_items[0];			///< 变长数组，存储K线缓存项数据
} RTBarCache;

#pragma pack(pop)                              // 恢复默认的结构体对齐方式
