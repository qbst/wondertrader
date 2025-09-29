/**
 * @file IAresCltStruct.h
 * @brief Ares客户端数据结构定义文件
 * 
 * 本文件定义了Ares行情客户端使用的所有数据结构和类型定义，包括：
 * 1. 字符串类型别名定义 - 统一不同长度的字符数组类型
 * 2. 市场标识符定义 - 支持的各个交易市场枚举值
 * 3. 行情数据结构 - 市场时间、买卖档位、完整行情快照等
 * 4. 合约基础数据结构 - 股票、期权、期货三类金融工具的基础信息
 * 5. 补充数据结构 - 扩展的合约信息存储结构
 * 
 * 设计逻辑：
 * - 使用#pragma pack(1)确保结构体内存紧密排列，避免字节对齐问题
 * - 采用union联合体设计，实现不同类型合约数据的统一存储
 * - 字符串类型统一管理，便于内存分配和数据处理
 * - 结构化设计支持多市场、多品种的金融数据处理需求
 */

#pragma once                                    // 防止头文件重复包含的预处理指令

#ifndef SWESOME_CHAR                            // 条件编译，防止字符类型重复定义
#define SWESOME_CHAR                            // 定义字符类型标识宏
typedef char    char_8[8];                      // 定义8字节字符数组类型，用于短标识符
typedef char	char_10[10];                    // 定义10字节字符数组类型，用于产品ID等
typedef char	char_20[20];                    // 定义20字节字符数组类型，用于合约代码等
typedef char	char_31[31];                    // 定义31字节字符数组类型，用于合约代码（含结束符）
typedef char	char_32[32];                    // 定义32字节字符数组类型，用于名称等中等长度字符串
typedef char	char_64[64];                    // 定义64字节字符数组类型，用于较长名称
typedef char	char_128[128];                  // 定义128字节字符数组类型，用于长字符串
typedef char	char_256[256];                  // 定义256字节字符数组类型，用于描述信息等
#endif                                          // 结束条件编译

#pragma pack(1)                                 // 设置结构体按1字节对齐，确保数据包的紧密性

/**
 * @brief 市场标识符定义 (仅限行情数据使用)
 * 
 * 定义了Ares客户端支持的各个交易市场的数字标识符
 * 每个市场对应一个唯一的数字ID，用于区分不同来源的行情数据
 */
#define		ACLT_MARKET_UNKNOW			0       // 未知市场标识符，用于初始化或错误状态
#define		ACLT_MARKET_SSE				1       // 上海证券交易所 (Shanghai Stock Exchange)
#define		ACLT_MARKET_SZSE			2       // 深圳证券交易所 (Shenzhen Stock Exchange)  
#define		ACLT_MARKET_CFFEX			3       // 中国金融期货交易所 (China Financial Futures Exchange)
#define		ACLT_MARKET_CNF				4       // 中国金融期货交易所的另一标识或其他市场
typedef		char	AClt_Market;                // 市场类型定义，使用char类型节省内存空间

// ================================================================================================
// 行情相关数据结构定义区域
// ================================================================================================

/**
 * @struct tagAClt_MarketField
 * @brief 市场时间信息结构体
 * 
 * 用于存储和传递市场的当前日期和时间信息，服务器会定期推送此信息
 * 以确保客户端与交易所时间保持同步，这对于时间敏感的交易应用至关重要
 */
typedef struct
{
	int								Date;						// 交易日期，格式为YYYYMMDD（如：20231225）
	int								Time;						// 交易时间，格式为HHMMSS（如：093000表示9:30:00）
}tagAClt_MarketField;

/**
 * @struct tagAClt_BuySell
 * @brief 买卖档位信息结构体
 * 
 * 表示单个买入或卖出档位的价格和数量信息
 * 用于构成完整的五档买卖盘口数据，反映市场的供需情况
 */
typedef struct
{
	double							Price;						// 该档位的委托价格（买价或卖价）
	unsigned long long				Volume;						// 该档位的委托数量，单位为股（或手）
}tagAClt_BuySell;

/**
 * @struct tagAClt_QuoteField  
 * @brief 完整行情快照数据结构体
 * 
 * 包含某个金融工具在特定时刻的完整市场行情信息，包括：
 * - 基本价格信息（开高低收）
 * - 成交统计信息（成交量、成交额）  
 * - 五档买卖盘口数据
 * - 期货特有的持仓量和结算价信息
 * - 交易状态标识
 */
typedef struct
{
	AClt_Market						Exchange;					// 市场标识符，指明数据来源的交易所
	char_31							Code;						// 合约代码，如"000001"、"IF2312"等
	double		 					Open;						// 当日开盘价
	double		 					High;						// 当日最高价  
	double		 					Low;						// 当日最低价
	double		 					Now;						// 最新成交价（现价）

	unsigned long long 				Volume;						// 累计成交量，单位为股或手
	double 							Amount;						// 累计成交金额，单位为元

	unsigned long long 				Position;					// 持仓量（期货专用），股票此字段通常为0
	double		 					SettlePrice;				// 结算价（期货专用），股票此字段通常为0

	tagAClt_BuySell					Buy[5];						// 买入五档价格和数量数组，Buy[0]为买一
	tagAClt_BuySell					Sell[5];					// 卖出五档价格和数量数组，Sell[0]为卖一
	char_8 							TradingCode;				// 交易状态代码，具体含义参考API文档
}tagAClt_QuoteField;

// ================================================================================================
// 合约基础信息相关数据结构定义区域  
// ================================================================================================

/**
 * @struct tagAClt_Instrument
 * @brief 金融工具标识结构体
 * 
 * 用于唯一标识一个金融工具（股票、期货、期权等）
 * 通过市场标识符和合约代码的组合来确保全局唯一性
 */
typedef struct
{
	AClt_Market						Exchange;					// 市场标识符，指明该工具所属的交易所
	char_31							Code;						// 合约代码，如股票代码"000001"或期货代码"IF2312"
}tagAClt_Instrument;

/**
 * @brief 金融工具类型常量定义
 * 
 * 定义了系统支持的三种主要金融工具类型
 * 用于区分不同类型工具的数据结构和处理逻辑
 */
#define		AClt_INSTRTYPE_STOCK		1       // 股票类型（包括股票、ETF、基金等）
#define		AClt_INSTRTYPE_OPTION		2       // 期权类型（看涨期权、看跌期权）
#define		AClt_INSTRTYPE_FUTURE		3       // 期货类型（商品期货、金融期货）
typedef     char		ACLT_INSTRUMENT_TYPE;       // 工具类型定义，使用char节省内存

/**
 * @struct tagAClt_StockBaseData
 * @brief 股票、基金基础数据结构体
 * 
 * 存储股票类金融工具的基础静态信息，包括价格限制、交易规则、
 * 产品分类等关键信息。这些数据通常在交易日开始前获取，
 * 用于交易系统的风控检查和订单处理。
 */
typedef struct
{
	tagAClt_Instrument				Instrument;					// 金融工具标识（市场+合约代码）
	char_32							Name;						// 证券名称或简称(GBK编码) [注意:交易所扩位后的完整名称需通过其他接口获取]
	double							PreClose;					// 前一交易日收盘价，用于计算涨跌幅
	double							UpperLimit;					// 当日涨停价格上限
	double							LowerLimit;					// 当日跌停价格下限

	unsigned char					SubType;					// 证券子类型标识（如普通股票、ETF基金等）
	char_10							ProductID;					// 所属产品系列标识符，用于产品分类管理

	unsigned int					LotSize;					// 交易单位（手）比率，如100股为1手则此值为100
	unsigned int					ContractMulti;				// 合约乘数与合约单位的乘积，用于计算合约价值
	double							PriceTick;					// 最小价格变动单位（价格精度），如0.01元
	unsigned char					ShowDot;					// 价格显示的小数位数，用于UI展示格式化
	bool							IsTrading;					// 当前是否允许交易的标志位
}tagAClt_StockBaseData;

// ================================================================================================
// 期权基础数据结构定义
// ================================================================================================

/**
 * @struct tagAClt_OptionBaseData
 * @brief 期权基础数据结构体
 * 
 * 存储期权合约的完整基础信息，包括期权特有的行权价格、到期日、
 * 期权类型（看涨/看跌）、行权方式（美式/欧式）等关键属性。
 * 期权作为衍生品，其定价和风险管理需要更多的基础参数。
 */
typedef struct
{
	tagAClt_Instrument				Instrument;					// 金融工具标识（市场+合约代码，此代码可直接用于下单）
	char_32							Name;						// 期权合约名称或简称(GBK编码) [中金所填合约代码，注意扩位后名称需其他接口获取]
	char_20							ContractID;					// 沪深期权的标准合约代码（仅标识用，不可下单），中金所此字段为空

	double							PreClose;					// 前一交易日收盘价
	double							PreSettle;					// 前一交易日结算价（期权特有）
	unsigned long long				PrePosition;				// 前一交易日持仓量
	double							UpperLimit;					// 当日涨停价格上限
	double							LowerLimit;					// 当日跌停价格下限

	char							OptKind;					// 期权类型标识：'C' = 看涨期权(Call)，'P' = 看跌期权(Put)
	char							ExecKind;					// 行权方式标识：'A' = 美式期权，'E' = 欧式期权

	char_10							ProductID;					// 所属产品系列标识符
	tagAClt_Instrument				UnderlyingCode;				// 标的资产的市场标识和代码（如股票、指数等）

	unsigned int					LotSize;					// 交易单位比率（如1手对应多少份合约）
	unsigned int					ContractMulti;				// 合约乘数与合约单位的乘积，用于计算合约价值
	double							PriceTick;					// 最小价格变动单位

	double							ExecPrice;					// 期权行权价格（执行价格）
	int								LastTradeDay;				// 最后交易日，格式为YYYYMMDD
	int								EndDay;						// 期权到期日，格式为YYYYMMDD

	unsigned char					ShowDot;					// 价格显示小数位数
	double							reserved;					// 保留字段，用于未来扩展
	bool							IsTrading;					// 当前是否允许交易
}tagAClt_OptionBaseData;

/**
 * @struct tagAClt_FutureBaseData  
 * @brief 期货基础数据结构体
 * 
 * 存储期货合约的基础静态信息，包括合约规格、交易规则、
 * 到期信息等。期货作为标准化合约，具有固定的交割月份、
 * 合约乘数等特征，这些信息对于风险管理和交易决策至关重要。
 */
typedef struct
{
	tagAClt_Instrument				Instrument;					// 金融工具标识（市场+合约代码）
	char_32							Name;						// 期货合约名称或简称(GBK编码) [注意:交易所扩位后的完整名称需通过其他接口获取]
	double							PreClose;					// 前一交易日收盘价
	double							PreSettle;					// 前一交易日结算价（期货每日结算的基准价）
	unsigned long long				PrePosition;				// 前一交易日总持仓量
	double							UpperLimit;					// 当日涨停价格上限（基于前结算价计算）
	double							LowerLimit;					// 当日跌停价格下限（基于前结算价计算）

	char_10							ProductID;					// 所属产品系列标识符（如铜、铝、沪深300等）
	tagAClt_Instrument				UnderlyingCode;				// 标的资产标识（对于股指期货是指数，商品期货可能为空）

	unsigned int					LotSize;					// 交易单位比率，1手对应的合约数量
	unsigned int					ContractMulti;				// 合约乘数与合约单位的乘积，用于计算合约价值
	double							PriceTick;					// 最小价格变动单位（最小跳动价位）

	int								LastTradeDay;				// 最后交易日，格式为YYYYMMDD
	int								EndDay;						// 合约到期日（交割日），格式为YYYYMMDD
	unsigned char					ShowDot;					// 价格显示的小数位数
	bool							IsTrading;					// 当前是否允许交易
}tagAClt_FutureBaseData;


// ================================================================================================
// 统一数据结构和扩展数据定义
// ================================================================================================

/**
 * @struct tagAClt_CommBaseData
 * @brief 商品基础数据统一结构体（三合一设计）
 * 
 * 采用联合体(union)设计，根据InstrType字段的值来确定使用哪种具体的数据结构。
 * 这种设计的优势：
 * 1. 内存效率：三种类型共享同一块内存空间，节省内存占用
 * 2. 接口统一：上层代码可以用统一的接口处理不同类型的金融工具
 * 3. 类型安全：通过InstrType字段确保数据类型的正确性
 */
typedef	struct
{
	ACLT_INSTRUMENT_TYPE			InstrType;					// 金融工具类型标识，决定union中哪个成员有效
	union                                                       // 联合体，根据InstrType使用对应的数据结构
	{
		tagAClt_StockBaseData			StockBase;					// 股票类型基础数据（当InstrType=STOCK时使用）
		tagAClt_OptionBaseData			OptionBase;					// 期权类型基础数据（当InstrType=OPTION时使用）
		tagAClt_FutureBaseData			FutureBase;					// 期货类型基础数据（当InstrType=FUTURE时使用）
	};
}tagAClt_CommBaseData;

/**
 * @struct tagAClt_SupplementData
 * @brief 补充数据结构体（API版本1.14+新增）
 * 
 * 用于存储合约的扩展信息，如完整名称、额外属性等。
 * 当基础数据结构无法满足所有信息存储需求时，可通过此结构获取补充信息。
 * Reserved字段提供了灵活的扩展能力，可存储各种自定义数据。
 */
typedef struct  
{
	AClt_Market						Exchange;					// 市场标识符
	char_31							Code;						// 合约代码
	char_64							Name;						// 完整名称（可能包含交易所扩位后的完整信息）
	
	char							Reserved[1024];				// 保留字段，用于存储额外的扩展信息（1KB缓冲区）
}tagAClt_SupplementData;

#pragma pack()                                              // 恢复默认的结构体字节对齐方式