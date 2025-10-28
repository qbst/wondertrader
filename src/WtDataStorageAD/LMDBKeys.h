/*!
 * \file LMDBKeys.h
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief LMDB数据库键值结构定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtDataStorageAD模块中LMDB数据库使用的键值结构。LMDB（Lightning 
 * Memory-Mapped Database）是一个高性能的嵌入式数据库，本文件中的键结构设计
 * 专门针对金融行情数据的存储和查询需求进行了优化。
 * 
 * 核心设计理念：
 * 
 * 1. 字节序优化（Endian Optimization）：
 *    - 使用大端字节序存储数值类型数据
 *    - 确保键值的字典序与时间序一致
 *    - 提高范围查询的性能和准确性
 * 
 * 2. 复合键设计（Composite Key Design）：
 *    - 交易所 + 合约 + 时间的层次化键结构
 *    - 支持多维度的数据查询和索引
 *    - 优化数据的物理存储布局
 * 
 * 3. 类型化键结构（Typed Key Structure）：
 *    - HFT键：用于高频Tick数据的精确时间索引
 *    - Bar键：用于K线数据的时间周期索引
 *    - 不同数据类型采用不同的键结构优化
 * 
 * LMDB键值设计原理：
 * 
 * 1. 字典序排序（Lexicographic Ordering）：
 *    LMDB使用字典序对键进行排序，通过字节序转换确保：
 *    - 时间较早的数据排在前面
 *    - 相同时间的数据按交易所、合约排序
 *    - 支持高效的范围查询操作
 * 
 * 2. 内存对齐优化（Memory Alignment）：
 *    - 使用#pragma pack(1)确保结构体紧密排列
 *    - 减少内存占用和磁盘空间
 *    - 提高缓存命中率和I/O效率
 * 
 * 3. 构造函数设计（Constructor Design）：
 *    - 自动进行字节序转换
 *    - 确保字符串的安全拷贝
 *    - 简化键对象的创建过程
 * 
 * 数据存储结构：
 * 
 * HFT数据存储：
 * Key: [交易所][合约][日期][时间]  →  Value: [Tick数据]
 * 
 * K线数据存储：
 * Key: [交易所][合约][K线时间]    →  Value: [Bar数据]
 * 
 * 查询优化策略：
 * - 前缀匹配：通过交易所+合约前缀快速定位数据范围
 * - 时间范围：通过时间字段进行高效的区间查询
 * - 顺序访问：利用LMDB的B+树结构进行顺序遍历
 * 
 * 使用场景：
 * - 实时Tick数据的存储和查询
 * - 历史K线数据的管理
 * - 时间序列数据的范围查询
 * - 多合约数据的统一索引
 */

#pragma once                                    // 防止头文件重复包含
#include <stdint.h>                             // 引入标准整数类型定义
#include <string.h>                             // 引入字符串操作函数
#include "../Includes/WTSMarcos.h"              // 引入WonderTrader宏定义

/**
 * @brief 16位整数字节序转换函数
 * 
 * 将16位整数从小端字节序转换为大端字节序，用于确保LMDB键值的
 * 字典序与数值大小顺序一致。这对于时间相关的查询优化至关重要。
 * 
 * @param src 源16位整数（小端字节序）
 * @return 转换后的16位整数（大端字节序）
 * 
 * 转换逻辑：
 * - 提取低8位并左移8位作为高字节
 * - 提取高8位并右移8位作为低字节
 * - 合并两个字节得到大端序结果
 */
static uint16_t reverseEndian(uint16_t src)
{
	uint16_t up = (src & 0x00FF) << 8;         // 低字节移到高位
	uint16_t low = (src & 0xFF00) >> 8;        // 高字节移到低位
	return up + low;                           // 合并结果
}

/**
 * @brief 32位整数字节序转换函数
 * 
 * 将32位整数从小端字节序转换为大端字节序，主要用于日期和时间
 * 字段的转换，确保时间序列数据在LMDB中的正确排序。
 * 
 * @param src 源32位整数（小端字节序）
 * @return 转换后的32位整数（大端字节序）
 * 
 * 转换逻辑：
 * - 将4个字节分别提取并重新排列
 * - 最低字节移到最高位，最高字节移到最低位
 * - 中间两个字节相应调换位置
 */
static uint32_t reverseEndian(uint32_t src)
{
	uint32_t x = (src & 0x000000FF) << 24;     // 字节0移到位置3
	uint32_t y = (src & 0x0000FF00) << 8;      // 字节1移到位置2
	uint32_t z = (src & 0x00FF0000) >> 8;      // 字节2移到位置1
	uint32_t w = (src & 0xFF000000) >> 24;     // 字节3移到位置0
	return x + y + z + w;                      // 合并所有字节
}

#pragma pack(push, 1)                          // 设置结构体按1字节对齐

/**
 * @struct LMDBHftKey
 * @brief LMDB高频交易数据键结构
 * 
 * 用于LMDB数据库中高频Tick数据的键值结构。该结构设计用于支持
 * 高精度时间戳的Tick数据存储和快速查询。
 * 
 * 键结构组成：
 * +------------------+------------------+------------------+------------------+
 * | _exchg (8B)      | _code (32B)      | _date (4B)       | _time (4B)       |
 * +------------------+------------------+------------------+------------------+
 * 
 * 排序优先级：
 * 1. 交易所代码（_exchg）- 按字典序排序
 * 2. 合约代码（_code）- 按字典序排序  
 * 3. 交易日期（_date）- 按时间顺序排序
 * 4. 交易时间（_time）- 按时间顺序排序
 * 
 * 查询优化：
 * - 支持按交易所快速过滤数据
 * - 支持按合约进行精确查找
 * - 支持按日期范围进行批量查询
 * - 支持按时间进行高精度范围查询
 */
typedef struct _LMDBHftKey
{
	char		_exchg[MAX_EXCHANGE_LENGTH];    ///< 交易所代码，如"SHFE"、"DCE"等
	char		_code[MAX_INSTRUMENT_LENGTH];   ///< 合约代码，如"rb2305"、"IF2303"等
	uint32_t	_date;                          ///< 交易日期，格式YYYYMMDD（大端序）
	uint32_t	_time;                          ///< 交易时间，格式HHMMSSsss（大端序）

	/**
	 * @brief 构造函数
	 * 
	 * 创建LMDB高频数据键对象，自动进行字节序转换和数据初始化。
	 * 
	 * @param exchg 交易所代码字符串
	 * @param code 合约代码字符串  
	 * @param date 交易日期（小端序输入）
	 * @param time 交易时间（小端序输入）
	 * 
	 * 处理流程：
	 * 1. 清零整个结构体内存
	 * 2. 安全拷贝交易所和合约字符串
	 * 3. 将日期和时间转换为大端序存储
	 */
	_LMDBHftKey(const char* exchg, const char* code, uint32_t date, uint32_t time)
	{
		memset(this, 0, sizeof(_LMDBHftKey));   // 清零结构体内存
		strcpy(_exchg, exchg);                  // 拷贝交易所代码
		strcpy(_code, code);                    // 拷贝合约代码
		_date = reverseEndian(date);            // 转换日期为大端序
		_time = reverseEndian(time);            // 转换时间为大端序
	}
} LMDBHftKey;

/**
 * @struct LMDBBarKey  
 * @brief LMDB K线数据键结构
 * 
 * 用于LMDB数据库中K线数据的键值结构。该结构设计用于支持不同
 * 周期K线数据的存储和时间范围查询。
 * 
 * 键结构组成：
 * +------------------+------------------+------------------+
 * | _exchg (8B)      | _code (32B)      | _bartime (4B)    |
 * +------------------+------------------+------------------+
 * 
 * 排序优先级：
 * 1. 交易所代码（_exchg）- 按字典序排序
 * 2. 合约代码（_code）- 按字典序排序
 * 3. K线时间（_bartime）- 按时间顺序排序
 * 
 * 时间格式说明：
 * - 分钟K线：YYYYMMDDHHmm格式，如202305081030表示2023年5月8日10:30
 * - 日K线：YYYYMMDD格式，如20230508表示2023年5月8日
 * - 其他周期：根据具体需求定义时间格式
 * 
 * 应用场景：
 * - 1分钟、5分钟、15分钟等分钟级K线存储
 * - 日K线、周K线、月K线等较长周期数据存储
 * - 历史K线数据的批量查询和分析
 */
typedef struct  _LMDBBarKey
{
public:
	char		_exchg[MAX_EXCHANGE_LENGTH];    ///< 交易所代码，如"SHFE"、"DCE"等
	char		_code[MAX_INSTRUMENT_LENGTH];   ///< 合约代码，如"rb2305"、"IF2303"等
	uint32_t	_bartime;                       ///< K线时间戳（大端序）

	/**
	 * @brief 构造函数
	 * 
	 * 创建LMDB K线数据键对象，自动进行字节序转换和数据初始化。
	 * 
	 * @param exchg 交易所代码字符串
	 * @param code 合约代码字符串
	 * @param bartime K线时间戳（小端序输入）
	 * 
	 * 处理流程：
	 * 1. 清零整个结构体内存
	 * 2. 安全拷贝交易所和合约字符串  
	 * 3. 将K线时间转换为大端序存储
	 */
	_LMDBBarKey(const char* exchg, const char* code, uint32_t bartime)
	{
		memset(this, 0, sizeof(_LMDBBarKey));   // 清零结构体内存
		strcpy(_exchg, exchg);                  // 拷贝交易所代码
		strcpy(_code, code);                    // 拷贝合约代码
		_bartime = reverseEndian(bartime);      // 转换时间为大端序
	}
} LMDBBarKey;

#pragma pack(pop)                              // 恢复默认的结构体对齐方式