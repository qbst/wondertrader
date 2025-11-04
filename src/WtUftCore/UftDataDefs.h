/*!
 * \file UftDataDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT数据定义头文件
 *
 * 本文件定义了UFT策略使用的数据结构和块格式。
 *
 * 设计逻辑：
 * 1. 内存对齐：使用1字节对齐（#pragma pack(1)），确保数据结构的紧凑存储
 * 2. 块结构：采用块头+数据数组的结构，支持变长数据存储
 * 3. 数据类型：定义持仓明细、订单、成交、回合等数据结构
 * 4. 标志识别：使用特殊标志识别数据块的完整性
 * 5. 命名空间：使用uft命名空间组织相关数据结构
 *
 * 主要功能：
 * - 定义持仓明细数据结构（DetailStruct）
 * - 定义订单数据结构（OrderStruct）
 * - 定义成交数据结构（TradeStruct）
 * - 定义回合数据结构（RoundStruct）
 * - 定义各种数据块结构（PositionBlock、OrderBlock、TradeBlock、RoundBlock）
 */
#pragma once
#include <stdint.h>  // 标准整数类型定义
#include <string.h>  // 字符串操作函数
#include "../Includes/WTSMarcos.h"  // WonderTrader宏定义

#pragma warning(disable:4200)  // 禁用零长度数组警告

namespace uft {  // UFT命名空间

#pragma pack(push, 1)  // 设置1字节对齐

	const char BLK_FLAG[] = "&^%$#@!\0";  // 数据块标志字符串，用于识别数据块的完整性

	const int FLAG_SIZE = 8;  // 标志大小（字节）

	/**
	 * @struct BlockHeader
	 * @brief 数据块头结构体
	 * 
	 * 定义数据块的头部信息，包括标志、类型、日期、容量和大小。
	 * 用于识别和管理数据块的完整性。
	 */
	typedef struct _BlockHeader
	{
		char		_blk_flag[FLAG_SIZE];  // 数据块标志，用于识别数据块的完整性
		uint32_t	_type;  // 数据块类型
		uint32_t	_date;  // 数据日期
		uint32_t	_capacity;  // 数据块容量（可存储的最大数据项数）
		uint32_t	_size;  // 数据块当前大小（实际存储的数据项数）
	} BlockHeader;

	/**
	 * @struct DetailStruct
	 * @brief 持仓明细结构体
	 * 
	 * 定义持仓明细的详细信息，包括交易所、合约代码、方向、数量、价格、时间、盈亏等。
	 */
	typedef struct _DetailStruct
	{
		char		_exchg[MAX_EXCHANGE_LENGTH];  // 交易所代码
		char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码
		uint32_t	_direct;	// 方向：0-多，1-空
		double		_volume;  // 持仓数量
		double		_open_price;  // 开仓价格
		uint64_t	_open_time;  // 开仓时间（微秒时间戳）
		uint32_t	_open_tdate;  // 开仓交易日
		double		_position_profit;  // 持仓盈亏

		double		_closed_volume;  // 已平仓数量
		double		_closed_profit;  // 已平仓盈亏

		/**
		 * @brief 构造函数
		 * 
		 * 初始化持仓明细结构体，将所有成员变量清零。
		 */
		_DetailStruct()
		{
			memset(this, 0, sizeof(_DetailStruct));  // 将结构体内存清零
		}

	}DetailStruct;

	/**
	 * @struct PositionBlock
	 * @brief 持仓数据块结构体
	 * 
	 * 继承自BlockHeader，包含持仓明细数组。
	 * 使用零长度数组实现变长数据存储。
	 */
	typedef struct _PositionBlock : public BlockHeader
	{
		DetailStruct	_details[0];  // 持仓明细数组（零长度数组，实际长度由_size决定）
	}PositionBlock;

	/**
	 * @struct OrderStruct
	 * @brief 订单结构体
	 * 
	 * 定义订单的详细信息，包括交易所、合约代码、方向、开平标志、数量、价格、成交情况、状态、时间等。
	 */
	typedef struct _OrderStruct
	{
		char		_exchg[MAX_EXCHANGE_LENGTH];  // 交易所代码
		char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码
		uint32_t	_direct;  // 方向：0-多，1-空
		uint32_t	_offset;  // 开平标志：0-开仓，1-平仓，2-平今
		double		_volume;  // 委托数量
		double		_price;  // 委托价格
		double		_traded;  // 已成交数量
		double		_left;  // 剩余数量
		uint32_t	_state;	// 订单状态：0-有效，1-全部成交，2-已撤单
		uint64_t	_oder_time;  // 订单时间（微秒时间戳）
	} OrderStruct;

	/**
	 * @struct OrderBlock
	 * @brief 订单数据块结构体
	 * 
	 * 继承自BlockHeader，包含订单数组。
	 * 使用零长度数组实现变长数据存储。
	 */
	typedef struct _OrderBlock : public BlockHeader
	{
		OrderStruct	_orders[0];  // 订单数组（零长度数组，实际长度由_size决定）
	} OrderBlock;

	/**
	 * @struct TradeStruct
	 * @brief 成交结构体
	 * 
	 * 定义成交的详细信息，包括交易所、合约代码、方向、开平标志、数量、价格、交易日期、交易时间等。
	 */
	typedef struct _TradeStruct
	{
		char		_exchg[MAX_EXCHANGE_LENGTH];  // 交易所代码
		char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码
		uint32_t	_direct;  // 方向：0-多，1-空
		uint32_t	_offset;  // 开平标志：0-开仓，1-平仓，2-平今
		double		_volume;  // 成交数量
		double		_price;  // 成交价格
		uint32_t	_trading_date;  // 交易日期
		uint64_t	_trading_time;  // 交易时间（微秒时间戳）
	} TradeStruct;

	/**
	 * @struct TradeBlock
	 * @brief 成交数据块结构体
	 * 
	 * 继承自BlockHeader，包含成交数组。
	 * 使用零长度数组实现变长数据存储。
	 */
	typedef struct _TradeBlock : public BlockHeader
	{
		TradeStruct	_trades[0];  // 成交数组（零长度数组，实际长度由_size决定）
	} TradeBlock;

	/**
	 * @struct RoundStruct
	 * @brief 回合结构体
	 * 
	 * 定义完整交易回合的详细信息，包括开仓和平仓的信息。
	 * 用于记录完整的交易回合（开仓+平仓）。
	 */
	typedef struct _RoundStruct
	{
		char		_exchg[MAX_EXCHANGE_LENGTH];  // 交易所代码
		char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码
		uint32_t	_direct;  // 方向：0-多，1-空
		double		_open_price;  // 开仓价格
		uint64_t	_open_time;  // 开仓时间（微秒时间戳）
		double		_close_price;  // 平仓价格
		uint64_t	_close_time;  // 平仓时间（微秒时间戳）
		double		_volume;  // 成交数量
		double		_profit;  // 盈亏
	} RoundStruct;

	/**
	 * @struct RoundBlock
	 * @brief 回合数据块结构体
	 * 
	 * 继承自BlockHeader，包含回合数组。
	 * 使用零长度数组实现变长数据存储。
	 */
	typedef struct _RoundBlock : public BlockHeader
	{
		RoundStruct	_rounds[0];  // 回合数组（零长度数组，实际长度由_size决定）
	} RoundBlock;

#pragma pack(pop)  // 恢复默认对齐方式

} //namespace uft  // UFT命名空间结束