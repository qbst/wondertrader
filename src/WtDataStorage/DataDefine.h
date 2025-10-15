/*!
 * \file DataDefine.h
 * \project WonderTrader
 * 
 * \brief WonderTrader数据存储格式定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中数据存储的核心数据结构和格式规范。
 * 该文件是WonderTrader数据存储系统的基础，定义了各种数据块的头部结构、
 * 数据类型枚举、版本控制机制等，为数据的高效存储和读取提供了统一的格式标准。
 * 
 * 核心设计理念：
 * 
 * 1. 统一数据格式（Unified Data Format）：
 *    - 定义了标准的数据块头部结构（BlockHeader/BlockHeaderV2）
 *    - 支持多种数据类型（实时/历史、K线/Tick/逐笔成交/逐笔委托/委托队列）
 *    - 提供版本控制和压缩支持
 * 
 * 2. 高效存储机制（Efficient Storage Mechanism）：
 *    - 使用内存映射文件（Memory-Mapped File）技术
 *    - 支持数据压缩以节省存储空间
 *    - 采用固定大小的数据块结构便于快速定位
 * 
 * 3. 版本兼容性（Version Compatibility）：
 *    - 支持多个版本的数据格式（V1/V2）
 *    - 提供版本检测和转换机制
 *    - 向后兼容旧版本数据
 * 
 * 4. 数据类型分类（Data Type Classification）：
 *    - 实时数据：实时K线、实时Tick、实时逐笔数据等
 *    - 历史数据：历史K线、历史Tick、历史逐笔数据等
 *    - 缓存数据：实时数据缓存、Tick缓存等
 * 
 * 主要数据结构：
 * 
 * 1. 数据块类型枚举（BlockType）：
 *    - 实时数据类型：BT_RT_Minute1、BT_RT_Ticks等
 *    - 历史数据类型：BT_HIS_Minute1、BT_HIS_Day等
 * 
 * 2. 数据块头部结构：
 *    - BlockHeader：基础数据块头部（V1版本）
 *    - BlockHeaderV2：扩展数据块头部（V2版本，支持大小信息）
 *    - RTBlockHeader：实时数据块头部
 *    - RTDayBlockHeader：每日实时数据块头部
 * 
 * 3. 具体数据块结构：
 *    - RTKlineBlock：实时K线数据块
 *    - RTTickBlock：实时Tick数据块
 *    - RTTransBlock：实时逐笔成交数据块
 *    - RTOrdDtlBlock：实时逐笔委托数据块
 *    - RTOrdQueBlock：实时委托队列数据块
 *    - 对应的历史数据块结构
 * 
 * 技术特点：
 * - 使用#pragma pack(1)确保结构体紧凑存储
 * - 支持数据压缩（zlib压缩算法）
 * - 使用魔数标识（BLK_FLAG）确保数据完整性
 * - 提供版本检测和兼容性处理
 * 
 * 使用场景：
 * - 实时行情数据存储
 * - 历史数据归档
 * - 数据备份和恢复
 * - 跨平台数据交换
 * 
 * 注意事项：
 * - 结构体大小必须固定，不能包含动态成员
 * - 版本升级需要考虑向后兼容性
 * - 数据压缩可能影响读取性能
 * - 魔数标识用于数据完整性校验
 */

#pragma once                                                    // 防止头文件重复包含
#include "../Includes/WTSStruct.h"                              // 包含WonderTrader基础数据结构定义

USING_NS_WTP;                                                   // 使用WonderTrader命名空间

#pragma pack(push, 1)                                           // 设置结构体按1字节对齐（紧凑存储）

const char BLK_FLAG[] = "&^%$#@!\0";                           // 数据块魔数标识（用于数据完整性校验）

const int FLAG_SIZE = 8;                                        // 魔数标识长度（8字节）

/*!
 * \enum BlockType
 * \brief 数据块类型枚举定义
 * 
 * 该枚举定义了WonderTrader支持的所有数据类型，包括实时数据和历史数据两大类。
 * 实时数据用于存储当日交易数据，历史数据用于存储历史归档数据。
 * 
 * 实时数据类型（1-7）：
 * - 用于存储当日交易时段内的实时数据
 * - 支持K线、Tick、逐笔成交、逐笔委托、委托队列等
 * - 数据会定期转储为历史数据
 * 
 * 历史数据类型（21-27）：
 * - 用于存储历史归档数据
 * - 支持日线、分钟线、Tick、逐笔数据等
 * - 数据经过压缩和优化存储
 */
typedef enum tagBlockType
{
	BT_RT_Minute1		= 1,	// 实时1分钟K线数据块
	BT_RT_Minute5		= 2,	// 实时5分钟K线数据块
	BT_RT_Ticks			= 3,	// 实时Tick数据块
	BT_RT_Cache			= 4,	// 实时数据缓存块
	BT_RT_Trnsctn		= 5,	// 实时逐笔成交数据块
	BT_RT_OrdDetail		= 6,	// 实时逐笔委托数据块
	BT_RT_OrdQueue		= 7,	// 实时委托队列数据块

	BT_HIS_Minute1		= 21,	// 历史1分钟K线数据块
	BT_HIS_Minute5		= 22,	// 历史5分钟K线数据块
	BT_HIS_Day			= 23,	// 历史日线数据块
	BT_HIS_Ticks		= 24,	// 历史Tick数据块
	BT_HIS_Trnsctn		= 25,	// 历史逐笔成交数据块
	BT_HIS_OrdDetail	= 26,	// 历史逐笔委托数据块
	BT_HIS_OrdQueue		= 27	// 历史委托队列数据块
} BlockType;

// ===== 数据块版本定义 =====
#define BLOCK_VERSION_RAW		0x01	// 老版本结构体未压缩格式
#define BLOCK_VERSION_CMP		0x02	// 老版本结构体压缩格式
#define BLOCK_VERSION_RAW_V2	0x03	// 新版本结构体未压缩格式
#define BLOCK_VERSION_CMP_V2	0x04	// 新版本结构体压缩格式

/*!
 * \struct _BlockHeader
 * \brief 基础数据块头部结构（V1版本）
 * 
 * 该结构体定义了数据块的基础头部信息，包含魔数标识、数据类型和版本信息。
 * 所有数据块都必须以此头部开始，用于数据完整性校验和类型识别。
 * 
 * 成员说明：
 * - _blk_flag：魔数标识，用于数据完整性校验
 * - _type：数据块类型（BlockType枚举值）
 * - _version：数据块版本（支持版本检测和兼容性处理）
 * 
 * 内联方法：
 * - is_old_version()：检测是否为旧版本格式
 * - is_compressed()：检测是否为压缩格式
 */
typedef struct _BlockHeader
{
	char		_blk_flag[FLAG_SIZE];                            // 数据块魔数标识（8字节）
	uint16_t	_type;                                           // 数据块类型（BlockType枚举值）
	uint16_t	_version;                                        // 数据块版本号

	/*!
	 * \brief 检测是否为旧版本格式
	 * \return true表示旧版本，false表示新版本
	 */
	inline bool is_old_version() const {
		return (_version == BLOCK_VERSION_CMP || _version == BLOCK_VERSION_RAW);
	}

	/*!
	 * \brief 检测是否为压缩格式
	 * \return true表示压缩格式，false表示未压缩格式
	 */
	inline bool is_compressed() const {
		return (_version == BLOCK_VERSION_CMP || _version == BLOCK_VERSION_CMP_V2);
	}
} BlockHeader;

/*!
 * \struct _BlockHeaderV2
 * \brief 扩展数据块头部结构（V2版本）
 * 
 * 该结构体在V1版本基础上增加了数据大小信息，支持更精确的数据管理。
 * V2版本主要用于压缩数据块，需要知道压缩后的大小信息。
 * 
 * 成员说明：
 * - _blk_flag：魔数标识，用于数据完整性校验
 * - _type：数据块类型（BlockType枚举值）
 * - _version：数据块版本号
 * - _size：压缩后的数据大小（字节数）
 * 
 * 内联方法：
 * - is_old_version()：检测是否为旧版本格式
 * - is_compressed()：检测是否为压缩格式
 */
typedef struct _BlockHeaderV2
{
	char		_blk_flag[FLAG_SIZE];                            // 数据块魔数标识（8字节）
	uint16_t	_type;                                           // 数据块类型（BlockType枚举值）
	uint16_t	_version;                                        // 数据块版本号

	uint64_t	_size;                                           // 压缩后的数据大小（字节数）

	/*!
	 * \brief 检测是否为旧版本格式
	 * \return true表示旧版本，false表示新版本
	 */
	inline bool is_old_version() const {
		return (_version == BLOCK_VERSION_CMP || _version == BLOCK_VERSION_RAW);
	}

	/*!
	 * \brief 检测是否为压缩格式
	 * \return true表示压缩格式，false表示未压缩格式
	 */
	inline bool is_compressed() const {
		return (_version == BLOCK_VERSION_CMP || _version == BLOCK_VERSION_CMP_V2);
	}
} BlockHeaderV2;

// ===== 数据块头部大小定义 =====
#define BLOCK_HEADER_SIZE	sizeof(BlockHeader)                  // V1版本数据块头部大小
#define BLOCK_HEADERV2_SIZE sizeof(BlockHeaderV2)                // V2版本数据块头部大小

/*!
 * \struct _RTBlockHeader
 * \brief 实时数据块头部结构
 * 
 * 该结构体继承自BlockHeader，增加了实时数据特有的大小和容量信息。
 * 用于管理实时数据块的内存分配和扩展。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _size：当前数据大小（字节数）
 * - _capacity：数据块容量（字节数）
 */
typedef struct _RTBlockHeader : BlockHeader
{
	uint32_t _size;                                               // 当前数据大小（字节数）
	uint32_t _capacity;                                           // 数据块容量（字节数）
} RTBlockHeader;

/*!
 * \struct _RTDayBlockHeader
 * \brief 每日实时数据块头部结构
 * 
 * 该结构体继承自RTBlockHeader，增加了日期信息。
 * 用于按日期组织实时数据，支持每日数据的独立管理。
 * 
 * 成员说明：
 * - 继承自RTBlockHeader的所有成员
 * - _date：交易日期（YYYYMMDD格式）
 */
typedef struct _RTDayBlockHeader : RTBlockHeader
{
	uint32_t		_date;                                         // 交易日期（YYYYMMDD格式）
} RTDayBlockHeader;

/*!
 * \struct _RTKlineBlock
 * \brief 实时K线数据块结构
 * 
 * 该结构体用于存储实时K线数据，支持1分钟和5分钟K线。
 * 使用柔性数组成员_bars存储K线数据，支持动态扩展。
 * 
 * 成员说明：
 * - 继承自RTDayBlockHeader的所有成员
 * - _bars：K线数据数组（柔性数组成员）
 */
typedef struct _RTKlineBlock : _RTDayBlockHeader
{
	WTSBarStruct	_bars[0];                                      // K线数据数组（柔性数组成员）
} RTKlineBlock;

/*!
 * \struct _RTTickBlock
 * \brief 实时Tick数据块结构
 * 
 * 该结构体用于存储实时Tick数据，支持高频行情数据存储。
 * 使用新版本的Tick结构，提供更好的性能和功能。
 * 
 * 注意事项：
 * - 原注释：By Wesley @ 2021.12.30
 * - 原注释：实时tick缓存，直接用新版本的tick结构
 * - 原注释：切换程序一定要在盘后进行！！！
 * 
 * 成员说明：
 * - 继承自RTDayBlockHeader的所有成员
 * - _ticks：Tick数据数组（柔性数组成员）
 */
typedef struct _RTTickBlock : RTDayBlockHeader
{
	WTSTickStruct	_ticks[0];                                     // Tick数据数组（柔性数组成员）
} RTTickBlock;

/*!
 * \struct _RTTransBlock
 * \brief 实时逐笔成交数据块结构
 * 
 * 该结构体用于存储实时逐笔成交数据，记录每笔交易的详细信息。
 * 支持高频交易数据的实时存储和查询。
 * 
 * 成员说明：
 * - 继承自RTDayBlockHeader的所有成员
 * - _trans：逐笔成交数据数组（柔性数组成员）
 */
typedef struct _RTTransBlock : RTDayBlockHeader
{
	WTSTransStruct	_trans[0];                                     // 逐笔成交数据数组（柔性数组成员）
} RTTransBlock;

/*!
 * \struct _RTOrdDtlBlock
 * \brief 实时逐笔委托数据块结构
 * 
 * 该结构体用于存储实时逐笔委托数据，记录每笔委托的详细信息。
 * 支持委托数据的实时存储和查询。
 * 
 * 成员说明：
 * - 继承自RTDayBlockHeader的所有成员
 * - _details：逐笔委托数据数组（柔性数组成员）
 */
typedef struct _RTOrdDtlBlock : RTDayBlockHeader
{
	WTSOrdDtlStruct	_details[0];                                   // 逐笔委托数据数组（柔性数组成员）
} RTOrdDtlBlock;

/*!
 * \struct _RTOrdQueBlock
 * \brief 实时委托队列数据块结构
 * 
 * 该结构体用于存储实时委托队列数据，记录委托队列的详细信息。
 * 支持委托队列数据的实时存储和查询。
 * 
 * 成员说明：
 * - 继承自RTDayBlockHeader的所有成员
 * - _queues：委托队列数据数组（柔性数组成员）
 */
typedef struct _RTOrdQueBlock : RTDayBlockHeader
{
	WTSOrdQueStruct	_queues[0];                                   // 委托队列数据数组（柔性数组成员）
} RTOrdQueBlock;

/*!
 * \struct _TickCacheItem
 * \brief Tick缓存项结构
 * 
 * 该结构体用于Tick数据缓存，包含日期和Tick数据。
 * 用于实时Tick数据的临时存储和批量处理。
 * 
 * 成员说明：
 * - _date：交易日期（YYYYMMDD格式）
 * - _tick：Tick数据结构
 */
typedef struct _TickCacheItem
{
	uint32_t		_date;                                         // 交易日期（YYYYMMDD格式）
	WTSTickStruct	_tick;                                         // Tick数据结构
} TickCacheItem;

/*!
 * \struct _RTTickCache
 * \brief 实时Tick缓存数据块结构
 * 
 * 该结构体用于存储实时Tick缓存数据，支持高频Tick数据的临时存储。
 * 使用TickCacheItem数组存储带日期的Tick数据。
 * 
 * 成员说明：
 * - 继承自RTBlockHeader的所有成员
 * - _ticks：Tick缓存项数组（柔性数组成员）
 */
typedef struct _RTTickCache : RTBlockHeader
{
	TickCacheItem	_ticks[0];                                     // Tick缓存项数组（柔性数组成员）
} RTTickCache;


// ===== 历史数据块结构定义 =====

/*!
 * \struct _HisTickBlock
 * \brief 历史Tick数据块结构（V1版本）
 * 
 * 该结构体用于存储历史Tick数据，支持历史行情数据的归档存储。
 * 使用V1版本的头部结构，数据未压缩存储。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _ticks：历史Tick数据数组（柔性数组成员）
 */
typedef struct _HisTickBlock : BlockHeader
{
	WTSTickStruct	_ticks[0];                                     // 历史Tick数据数组（柔性数组成员）
} HisTickBlock;

/*!
 * \struct _HisTickBlockV2
 * \brief 历史Tick数据块结构（V2版本）
 * 
 * 该结构体用于存储历史Tick数据，支持数据压缩存储。
 * 使用V2版本的头部结构，数据经过压缩处理。
 * 
 * 成员说明：
 * - 继承自BlockHeaderV2的所有成员
 * - _data：压缩后的数据（柔性数组成员）
 */
typedef struct _HisTickBlockV2 : BlockHeaderV2
{
	char			_data[0];                                      // 压缩后的数据（柔性数组成员）
} HisTickBlockV2;

/*!
 * \struct _HisTransBlock
 * \brief 历史逐笔成交数据块结构（V1版本）
 * 
 * 该结构体用于存储历史逐笔成交数据，支持历史交易数据的归档存储。
 * 使用V1版本的头部结构，数据未压缩存储。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _items：历史逐笔成交数据数组（柔性数组成员）
 */
typedef struct _HisTransBlock : BlockHeader
{
	WTSTransStruct	_items[0];                                     // 历史逐笔成交数据数组（柔性数组成员）
} HisTransBlock;

/*!
 * \struct _HisTransBlockV2
 * \brief 历史逐笔成交数据块结构（V2版本）
 * 
 * 该结构体用于存储历史逐笔成交数据，支持数据压缩存储。
 * 使用V2版本的头部结构，数据经过压缩处理。
 * 
 * 成员说明：
 * - 继承自BlockHeaderV2的所有成员
 * - _data：压缩后的数据（柔性数组成员）
 */
typedef struct _HisTransBlockV2 : BlockHeaderV2
{
	char			_data[0];                                      // 压缩后的数据（柔性数组成员）
} HisTransBlockV2;

/*!
 * \struct _HisOrdDtlBlock
 * \brief 历史逐笔委托数据块结构（V1版本）
 * 
 * 该结构体用于存储历史逐笔委托数据，支持历史委托数据的归档存储。
 * 使用V1版本的头部结构，数据未压缩存储。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _items：历史逐笔委托数据数组（柔性数组成员）
 */
typedef struct _HisOrdDtlBlock : BlockHeader
{
	WTSOrdDtlStruct	_items[0];                                     // 历史逐笔委托数据数组（柔性数组成员）
} HisOrdDtlBlock;

/*!
 * \struct _HisOrdDtlBlockV2
 * \brief 历史逐笔委托数据块结构（V2版本）
 * 
 * 该结构体用于存储历史逐笔委托数据，支持数据压缩存储。
 * 使用V2版本的头部结构，数据经过压缩处理。
 * 
 * 成员说明：
 * - 继承自BlockHeaderV2的所有成员
 * - _data：压缩后的数据（柔性数组成员）
 */
typedef struct _HisOrdDtlBlockV2 : BlockHeaderV2
{
	char			_data[0];                                      // 压缩后的数据（柔性数组成员）
} HisOrdDtlBlockV2;

/*!
 * \struct _HisOrdQueBlock
 * \brief 历史委托队列数据块结构（V1版本）
 * 
 * 该结构体用于存储历史委托队列数据，支持历史委托队列数据的归档存储。
 * 使用V1版本的头部结构，数据未压缩存储。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _items：历史委托队列数据数组（柔性数组成员）
 */
typedef struct _HisOrdQueBlock : BlockHeader
{
	WTSOrdQueStruct	_items[0];                                     // 历史委托队列数据数组（柔性数组成员）
} HisOrdQueBlock;

/*!
 * \struct _HisOrdQueBlockV2
 * \brief 历史委托队列数据块结构（V2版本）
 * 
 * 该结构体用于存储历史委托队列数据，支持数据压缩存储。
 * 使用V2版本的头部结构，数据经过压缩处理。
 * 
 * 成员说明：
 * - 继承自BlockHeaderV2的所有成员
 * - _data：压缩后的数据（柔性数组成员）
 */
typedef struct _HisOrdQueBlockV2 : BlockHeaderV2
{
	char			_data[0];                                      // 压缩后的数据（柔性数组成员）
} HisOrdQueBlockV2;

/*!
 * \struct _HisKlineBlock
 * \brief 历史K线数据块结构（V1版本）
 * 
 * 该结构体用于存储历史K线数据，支持历史K线数据的归档存储。
 * 使用V1版本的头部结构，数据未压缩存储。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _bars：历史K线数据数组（柔性数组成员）
 */
typedef struct _HisKlineBlock : BlockHeader
{
	WTSBarStruct	_bars[0];                                      // 历史K线数据数组（柔性数组成员）
} HisKlineBlock;

/*!
 * \struct _HisKlineBlockV2
 * \brief 历史K线数据块结构（V2版本）
 * 
 * 该结构体用于存储历史K线数据，支持数据压缩存储。
 * 使用V2版本的头部结构，数据经过压缩处理。
 * 
 * 成员说明：
 * - 继承自BlockHeaderV2的所有成员
 * - _data：压缩后的数据（柔性数组成员）
 */
typedef struct _HisKlineBlockV2 : BlockHeaderV2
{
	char			_data[0];                                      // 压缩后的数据（柔性数组成员）
} HisKlineBlockV2;

/*!
 * \struct _HisKlineBlockOld
 * \brief 历史K线数据块结构（旧版本）
 * 
 * 该结构体用于存储历史K线数据，使用旧版本的K线结构。
 * 主要用于向后兼容旧版本的数据格式。
 * 
 * 成员说明：
 * - 继承自BlockHeader的所有成员
 * - _bars：历史K线数据数组（旧版本结构，柔性数组成员）
 */
typedef struct _HisKlineBlockOld : BlockHeader
{
	WTSBarStructOld	_bars[0];                                      // 历史K线数据数组（旧版本结构，柔性数组成员）
} HisKlineBlockOld;

#pragma pack(pop)                                                // 恢复默认的结构体对齐方式
