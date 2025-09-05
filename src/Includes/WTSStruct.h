/*!
 * \file WTSStruct.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt基础结构体定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统中所有核心数据结构体，是系统数据交换和存储的基础。
 * 
 * 主要功能：
 * 1. 市场数据结构体：K线数据(WTSBarStruct)、Tick数据(WTSTickStruct)等
 * 2. 交易数据结构体：委托队列(WTSOrdQueStruct)、委托明细(WTSOrdDtlStruct)、成交明细(WTSTransStruct)等
 * 3. 数据兼容性：提供新旧版本结构体的转换操作符，确保系统升级的平滑过渡
 * 
 * 设计特点：
 * - 内存对齐优化：新版本采用8字节对齐，提高数据访问效率
 * - 数据类型统一：使用标准整数类型和双精度浮点数，确保数据精度
 * - 向后兼容：保留旧版本结构体，提供转换操作符
 * - 内存安全：所有结构体构造函数自动清零内存，避免未初始化数据
 * - 灵活扩展：使用联合体(union)支持不同市场的数据字段复用
 * 
 * 应用场景：
 * - 数据存储：本地数据文件和数据库的数据结构定义
 * - 网络传输：各模块间的数据交换格式
 * - 策略计算：策略引擎处理市场数据的基础结构
 * - 回测分析：历史数据回放和分析的数据格式
 */
#pragma once  // 防止头文件被重复包含
#include <memory>      // 智能指针支持
#include <stdint.h>    // 标准整数类型定义
#include <string.h>    // 字符串操作函数
#include "WTSTypes.h"  // WonderTrader类型定义

#ifdef _MSC_VER
#pragma warning(disable:4200)  // 禁用MSVC编译器关于柔性数组成员的警告
#endif

NS_WTP_BEGIN  // 开始WonderTrader命名空间

#pragma pack(push, 1)  // 设置结构体1字节对齐，确保内存布局紧凑

/**
 * 旧版本K线数据结构体
 * 
 * 功能概述：
 * 定义K线数据的标准格式，包含开高低收、成交量、持仓量等关键信息。
 * 采用1字节对齐方式，内存占用紧凑，适合大量数据存储和传输。
 * 
 * 主要字段：
 * - 时间信息：日期、时间
 * - 价格信息：开高低收、结算价
 * - 交易信息：成交金额、成交量、持仓量、增仓量
 * 
 * 注意：此结构体已被WTSBarStruct替代，保留用于向后兼容
 */
struct WTSBarStructOld
{
public:
	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSBarStructOld()
	{
		memset(this, 0, sizeof(WTSBarStructOld));  // 将整个结构体内存清零
	}

	uint32_t	date;		// 交易日期，格式：YYYYMMDD，如20240327
	uint32_t	time;		// 交易时间，格式：HHMMSS，如093000
	double		open;		// 开盘价，该时间段内的第一个成交价格
	double		high;		// 最高价，该时间段内的最高成交价格
	double		low;		// 最低价，该时间段内的最低成交价格
	double		close;		// 收盘价，该时间段内的最后一个成交价格
	double		settle;		// 结算价，期货合约的当日结算价格
	double		money;		// 成交金额，该时间段内的总成交金额

	uint32_t	vol;		// 成交量，该时间段内的总成交手数
	uint32_t	hold;		// 总持仓量，该时间点的总持仓手数
	int32_t		add;		// 增仓量，该时间段内持仓量的变化，正数表示增仓，负数表示减仓
};

/**
 * 旧版本Tick数据结构体
 * 
 * 功能概述：
 * 定义实时Tick数据的标准格式，包含最新价格、买卖盘口、成交量等实时信息。
 * 适用于高频交易和实时监控，提供最细粒度的市场数据。
 * 
 * 主要字段：
 * - 合约信息：交易所代码、合约代码
 * - 价格信息：最新价、开高低收、涨跌停价
 * - 交易信息：成交量、成交额、持仓量
 * - 盘口信息：10档买卖价格和数量
 * 
 * 注意：此结构体已被WTSTickStruct替代，保留用于向后兼容
 */
struct WTSTickStructOld
{
	char		exchg[10];		// 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	char		code[MAX_INSTRUMENT_LENGTH];  // 合约代码，如"IF2403"、"AU2406"等

	double		price;				// 最新价，最近一笔成交的价格
	double		open;				// 开盘价，当日第一笔成交的价格
	double		high;				// 最高价，当日最高成交价格
	double		low;				// 最低价，当日最低成交价格
	double		settle_price;		// 结算价，期货合约的当日结算价格

	double		upper_limit;		// 涨停价，当日允许的最高价格
	double		lower_limit;		// 跌停价，当日允许的最低价格

	uint32_t	total_volume;		// 总成交量，当日累计成交手数
	uint32_t	volume;				// 成交量，最近一笔成交的手数
	double		total_turnover;		// 总成交额，当日累计成交金额
	double		turn_over;			// 成交额，最近一笔成交的金额
	uint32_t	open_interest;		// 总持仓量，当前总持仓手数
	int32_t		diff_interest;		// 增仓量，持仓量相对于前一日的变化

	uint32_t	trading_date;		// 交易日，格式：YYYYMMDD，如20240327
	uint32_t	action_date;		// 自然日期，格式：YYYYMMDD，如20240327
	uint32_t	action_time;		// 发生时间，精确到毫秒，格式：HHMMSSmmm，如105932000

	double		pre_close;			// 昨收价，前一交易日的收盘价
	double		pre_settle;			// 昨结算，前一交易日的结算价
	int32_t		pre_interest;		// 上日总持仓，前一交易日的总持仓量

	double		bid_prices[10];		// 委买价格数组，10档买盘价格，从高到低排列
	double		ask_prices[10];		// 委卖价格数组，10档卖盘价格，从低到高排列
	uint32_t	bid_qty[10];		// 委买数量数组，对应10档买盘的数量
	uint32_t	ask_qty[10];		// 委卖数量数组，对应10档卖盘的数量

	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSTickStructOld()
	{
		memset(this, 0, sizeof(WTSTickStructOld));  // 将整个结构体内存清零
	}
};

#pragma pack(pop)  // 恢复默认的内存对齐设置


//By Wesley @ 2021.12.31
//新的结构体，全部改成8字节对齐的方式，提高数据访问效率
#pragma pack(push, 8)  // 设置结构体8字节对齐，优化CPU访问性能

/**
 * 新版本K线数据结构体
 * 
 * 功能概述：
 * 定义K线数据的标准格式，采用8字节对齐优化内存访问性能。
 * 相比旧版本，增加了占位符字段，支持期权市场的买卖价字段复用。
 * 
 * 主要改进：
 * - 内存对齐：8字节对齐提高CPU访问效率
 * - 字段扩展：增加占位符，为未来扩展预留空间
 * - 期权支持：通过联合体支持期权买卖价字段
 * - 向后兼容：提供从旧版本的转换操作符
 * 
 * 应用场景：
 * - 期货K线数据存储和传输
 * - 期权K线数据（买卖价字段复用）
 * - 高频数据访问和计算
 */
struct WTSBarStruct
{
public:
	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSBarStruct()
	{
		memset(this, 0, sizeof(WTSBarStruct));  // 将整个结构体内存清零
	}

	uint32_t	date;		// 交易日期，格式：YYYYMMDD，如20240327
	uint32_t	reserve_;	// 占位符，为未来扩展预留的字段
	uint64_t	time;		// 时间戳，精确到毫秒的时间信息
	double		open;		// 开盘价，该时间段内的第一个成交价格
	double		high;		// 最高价，该时间段内的最高成交价格
	double		low;		// 最低价，该时间段内的最低成交价格
	double		close;		// 收盘价，该时间段内的最后一个成交价格
	double		settle;		// 结算价，期货合约的当日结算价格
	
	double		money;		// 成交金额，该时间段内的总成交金额
	double		vol;		// 成交量，该时间段内的总成交手数

	/**
	 * 持仓量/买价联合体
	 * 期货市场：hold字段存储总持仓量
	 * 期权市场：bid字段存储买价，因为期权spread较大需要更高精度
	 */
	union
	{
		double		hold;	// 总持仓量，期货市场使用，该时间点的总持仓手数
		double		bid;	// 买价，期权市场专用，主要期权spread比较大，By Wseley @ 2023.05.04
	};

	/**
	 * 增仓量/卖价联合体
	 * 期货市场：add字段存储增仓量
	 * 期权市场：ask字段存储卖价，因为期权spread较大需要更高精度
	 */
	union
	{
		double		add;	// 增仓量，期货市场使用，该时间段内持仓量的变化
		double		ask;	// 卖价，期权市场专用，主要期权spread比较大，By Wseley @ 2023.05.04
	};	

	/**
	 * 从旧版本结构体复制数据
	 * 实现新旧版本的平滑过渡，确保系统升级时数据不丢失
	 * 
	 * @param bar 旧版本的K线数据结构体
	 * @return 当前结构体的引用，支持链式调用
	 * 
	 * 注意：此函数主要用于数据迁移和兼容性处理
	 */
	//By Wesley @ 2021.12.30
	//直接复制老结构体
	WTSBarStruct& operator = (const WTSBarStructOld& bar)
	{
		date = bar.date;		// 复制日期
		time = bar.time;		// 复制时间

		open = bar.open;		// 复制开盘价
		high = bar.high;		// 复制最高价
		low = bar.low;			// 复制最低价
		close = bar.close;		// 复制收盘价
		settle = bar.settle;	// 复制结算价
		money = bar.money;		// 复制成交金额

		vol = bar.vol;			// 复制成交量
		hold = bar.hold;		// 复制持仓量
		add = bar.add;			// 复制增仓量

		return *this;			// 返回当前对象引用，支持链式调用
	}
};

/**
 * 新版本Tick数据结构体
 * 
 * 功能概述：
 * 定义实时Tick数据的标准格式，采用8字节对齐优化内存访问性能。
 * 相比旧版本，所有数值字段都使用double类型，提高数据精度和一致性。
 * 
 * 主要改进：
 * - 内存对齐：8字节对齐提高CPU访问效率
 * - 数据类型：统一使用double类型，提高精度和一致性
 * - 字段扩展：增加占位符，为未来扩展预留空间
 * - 向后兼容：提供从旧版本的转换操作符
 * 
 * 应用场景：
 * - 实时行情数据广播
 * - 高频交易策略计算
 * - 市场数据分析和监控
 */
struct WTSTickStruct
{
	char		exchg[MAX_EXCHANGE_LENGTH];		// 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	char		code[MAX_INSTRUMENT_LENGTH];		// 合约代码，如"IF2403"、"AU2406"等

	double		price;				// 最新价，最近一笔成交的价格
	double		open;				// 开盘价，当日第一笔成交的价格
	double		high;				// 最高价，当日最高成交价格
	double		low;				// 最低价，当日最低成交价格
	double		settle_price;		// 结算价，期货合约的当日结算价格

	double		upper_limit;		// 涨停价，当日允许的最高价格
	double		lower_limit;		// 跌停价，当日允许的最低价格

	double		total_volume;		// 总成交量，当日累计成交手数
	double		volume;				// 成交量，最近一笔成交的手数
	double		total_turnover;		// 总成交额，当日累计成交金额
	double		turn_over;			// 成交额，最近一笔成交的金额
	double		open_interest;		// 总持仓量，当前总持仓手数
	double		diff_interest;		// 增仓量，持仓量相对于前一日的变化

	uint32_t	trading_date;		// 交易日，格式：YYYYMMDD，如20240327
	uint32_t	action_date;		// 自然日期，格式：YYYYMMDD，如20240327
	uint32_t	action_time;		// 发生时间，精确到毫秒，格式：HHMMSSmmm，如105932000
	uint32_t	reserve_;			// 占位符，为未来扩展预留的字段

	double		pre_close;			// 昨收价，前一交易日的收盘价
	double		pre_settle;			// 昨结算，前一交易日的结算价
	double		pre_interest;		// 上日总持仓，前一交易日的总持仓量

	double		bid_prices[10];		// 委买价格数组，10档买盘价格，从高到低排列
	double		ask_prices[10];		// 委卖价格数组，10档卖盘价格，从低到高排列
	double		bid_qty[10];		// 委买数量数组，对应10档买盘的数量
	double		ask_qty[10];		// 委卖数量数组，对应10档卖盘的数量

	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSTickStruct()
	{
		memset(this, 0, sizeof(WTSTickStruct));  // 将整个结构体内存清零
	}

	/**
	 * 从旧版本结构体复制数据
	 * 实现新旧版本的平滑过渡，确保系统升级时数据不丢失
	 * 
	 * @param tick 旧版本的Tick数据结构体
	 * @return 当前结构体的引用，支持链式调用
	 * 
	 * 注意：此函数主要用于数据迁移和兼容性处理
	 */
	WTSTickStruct& operator = (const WTSTickStructOld& tick)
	{
		strncpy(exchg, tick.exchg, MAX_EXCHANGE_LENGTH);		// 复制交易所代码
		strncpy(code, tick.code, MAX_INSTRUMENT_LENGTH);		// 复制合约代码

		price = tick.price;				// 复制最新价
		open = tick.open;				// 复制开盘价
		high = tick.high;				// 复制最高价
		low = tick.low;					// 复制最低价
		settle_price = tick.settle_price;	// 复制结算价

		upper_limit = tick.upper_limit;		// 复制涨停价
		lower_limit = tick.lower_limit;		// 复制跌停价

		total_volume = tick.total_volume;		// 复制总成交量
		total_turnover = tick.total_turnover;	// 复制总成交额
		open_interest = tick.open_interest;		// 复制总持仓量
		volume = tick.volume;					// 复制成交量
		turn_over = tick.turn_over;				// 复制成交额
		diff_interest = tick.diff_interest;		// 复制增仓量

		trading_date = tick.trading_date;		// 复制交易日
		action_date = tick.action_date;			// 复制自然日期
		action_time = tick.action_time;			// 复制发生时间

		pre_close = tick.pre_close;				// 复制昨收价
		pre_interest = tick.pre_interest;		// 复制上日总持仓
		pre_settle = tick.pre_settle;			// 复制昨结算

		// 复制10档买卖盘口数据
		for(int i = 0; i < 10; i++)
		{
			bid_prices[i] = tick.bid_prices[i];	// 复制买盘价格
			bid_qty[i] = tick.bid_qty[i];		// 复制买盘数量
			ask_prices[i] = tick.ask_prices[i];	// 复制卖盘价格
			ask_qty[i] = tick.ask_qty[i];		// 复制卖盘数量
		}

		return *this;	// 返回当前对象引用，支持链式调用
	}
};

/**
 * 委托队列数据结构体
 * 
 * 功能概述：
 * 定义委托队列数据的标准格式，包含特定价格档位的委托信息。
 * 用于分析市场深度和委托分布，为交易策略提供决策依据。
 * 
 * 主要字段：
 * - 合约信息：交易所代码、合约代码
 * - 时间信息：交易日、自然日、发生时间
 * - 委托信息：价格、方向、订单数量、队列长度
 * - 明细数据：50档委托数量明细
 * 
 * 应用场景：
 * - 市场深度分析
 * - 委托分布统计
 * - 高频交易策略
 */
struct WTSOrdQueStruct
{
	char		exchg[MAX_EXCHANGE_LENGTH];		// 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	char		code[MAX_INSTRUMENT_LENGTH];		// 合约代码，如"IF2403"、"AU2406"等

	uint32_t	trading_date;		// 交易日，格式：YYYYMMDD，如20240327
	uint32_t	action_date;		// 自然日期，格式：YYYYMMDD，如20240327
	uint32_t	action_time;		// 发生时间，精确到毫秒，格式：HHMMSSmmm，如105932000
	WTSBSDirectType	side;			// 委托方向，买入(Buy)或卖出(Sell)

	double		price;			// 委托价格，该档位的委托价格
	uint32_t	order_items;	// 订单个数，该价格档位的委托订单数量
	uint32_t	qsize;			// 队列长度，该价格档位的委托队列长度
	uint32_t	volumes[50];	// 委托明细，50档委托数量的详细分布

	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSOrdQueStruct()
	{
		memset(this, 0, sizeof(WTSOrdQueStruct));  // 将整个结构体内存清零
	}
};

/**
 * 委托明细数据结构体
 * 
 * 功能概述：
 * 定义委托明细数据的标准格式，包含每笔委托的详细信息。
 * 用于分析委托行为和市场微观结构，为交易策略提供决策依据。
 * 
 * 主要字段：
 * - 合约信息：交易所代码、合约代码
 * - 时间信息：交易日、自然日、发生时间
 * - 委托信息：编号、价格、数量、方向、类型
 * 
 * 应用场景：
 * - 委托行为分析
 * - 市场微观结构研究
 * - 高频交易策略
 */
struct WTSOrdDtlStruct
{
	char		exchg[MAX_EXCHANGE_LENGTH];		// 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	char		code[MAX_INSTRUMENT_LENGTH];		// 合约代码，如"IF2403"、"AU2406"等

	uint32_t		trading_date;		// 交易日，格式：YYYYMMDD，如20240327
	uint32_t		action_date;		// 自然日期，格式：YYYYMMDD，如20240327
	uint32_t		action_time;		// 发生时间，精确到毫秒，格式：HHMMSSmmm，如105932000

	uint64_t			index;			// 委托编号，从1开始递增的唯一标识
	double				price;			// 委托价格，该笔委托的价格
	uint32_t			volume;			// 委托数量，该笔委托的手数
	WTSBSDirectType		side;		// 委托方向，买入(Buy)或卖出(Sell)
	WTSOrdDetailType	otype;		// 委托类型，如限价单、市价单等

	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSOrdDtlStruct()
	{
		memset(this, 0, sizeof(WTSOrdDtlStruct));  // 将整个结构体内存清零
	}
};

/**
 * 成交明细数据结构体
 * 
 * 功能概述：
 * 定义成交明细数据的标准格式，包含每笔成交的详细信息。
 * 用于分析成交行为和市场流动性，为交易策略提供决策依据。
 * 
 * 主要字段：
 * - 合约信息：交易所代码、合约代码
 * - 时间信息：交易日、自然日、发生时间
 * - 成交信息：编号、类型、方向、价格、数量
 * - 订单信息：叫买序号、叫卖序号
 * 
 * 应用场景：
 * - 成交行为分析
 * - 市场流动性研究
 * - 高频交易策略
 * - 交易成本分析
 */
struct WTSTransStruct
{
	char		exchg[MAX_EXCHANGE_LENGTH];		// 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	char		code[MAX_INSTRUMENT_LENGTH];		// 合约代码，如"IF2403"、"AU2406"等

	uint32_t	trading_date;		// 交易日，格式：YYYYMMDD，如20240327
	uint32_t	action_date;		// 自然日期，格式：YYYYMMDD，如20240327
	uint32_t	action_time;		// 发生时间，精确到毫秒，格式：HHMMSSmmm，如105932000
	int64_t		index;			// 成交编号，从1开始递增的唯一标识

	WTSTransType	ttype;			// 成交类型，如主动买入('B')、主动卖出('S')、中性('C')等
	WTSBSDirectType	side;			// BS标志，买入(Buy)或卖出(Sell)方向

	double			price;			// 成交价格，该笔成交的价格
	uint32_t		volume;			// 成交数量，该笔成交的手数
	int64_t			askorder;		// 叫卖序号，卖方委托的序号
	int64_t			bidorder;		// 叫买序号，买方委托的序号

	/**
	 * 构造函数
	 * 自动初始化所有字段为0，确保数据安全
	 */
	WTSTransStruct()
	{
		memset(this, 0, sizeof(WTSTransStruct));  // 将整个结构体内存清零
	}
};

#pragma pack(pop)  // 恢复默认的内存对齐设置

NS_WTP_END  // 结束WonderTrader命名空间
