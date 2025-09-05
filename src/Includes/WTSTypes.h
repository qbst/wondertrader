/*!
 * \file WTSTypes.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader基本数据类型定义文件
 * 
 * 设计逻辑与作用：
 * 本文件定义了WonderTrader系统中使用的所有基本数据类型、枚举类型和常量。
 * 包括合约分类、期权类型、交易模式、价格模式、K线数据类型、日志级别、价格类型、
 * 时间条件、订单标志、开平方向、多空方向、业务类型、订单操作类型、订单状态、
 * 成交类型、错误代码、比较字段、比较类型、事件类型、交易状态等。
 * 这些类型定义为整个交易系统提供了统一的数据类型标准，确保系统各模块间的数据一致性。
 */
#pragma once  // 防止头文件被重复包含
#include "WTSMarcos.h"  // 包含WonderTrader宏定义
#include <stdint.h>      // 包含标准整数类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/*
 *	合约分类枚举
 *	定义了系统中支持的各种合约类型，包括股票、期货、期权、数字货币等
 */
//从CTP复制过来的合约分类定义
typedef enum tagContractCategory
{
	CC_Stock,			// 股票合约
	CC_Future,			// 期货合约
	CC_FutOption,		// 期货期权，商品期权属于这个分类
	CC_Combination,		// 组合合约
	CC_Spot,			// 即期合约
	CC_EFP,				// 期转现合约
	CC_SpotOption,		// 现货期权，股指期权属于这个分类
	CC_ETFOption,		// 个股期权，ETF期权属于这个分类

	CC_DC_Spot	= 20,	// 数字货币现货合约
	CC_DC_Swap,			// 数字货币永续合约
	CC_DC_Future,		// 数字货币期货合约
	CC_DC_Margin,		// 数字货币杠杆合约
	CC_DC_Option,		// 数字货币期权合约

	CC_UserIndex = 90	// 自定义指数合约
} ContractCategory;

/*
 *	期权类型枚举
 *	定义了期权的看涨和看跌类型
 */
typedef enum tagOptionType
{
	OT_None = 0,        // 无期权类型
	OT_Call = '1',		// 看涨期权
	OT_Put	= '2'		// 看跌期权
} OptionType;

/*
 *	平仓类型枚举
 *	定义了期货交易中的平仓方式
 */
typedef enum tagCoverMode
{
	CM_OpenCover,		// 开平：开仓和平仓
	CM_CoverToday,		// 开平昨平今：开仓、平昨仓、平今仓
	CM_UNFINISHED,		// 平未了结的：平掉未完成的持仓
	CM_None			// 不区分开平：不区分开仓和平仓
} CoverMode;

/*
 *	交易模式枚举
 *	定义了合约允许的交易方向
 */
typedef enum tagTradingMode
{
	TM_Both,	// 多空都支持：可以做多也可以做空
	TM_Long,	// 只能做多：只允许做多方向
	TM_LongT1,	// 做多T+1：做多需要T+1才能平仓
	TM_None = 9	// 不能交易：该合约不允许交易
} TradingMode;

/*
*	价格模式枚举
*	定义了合约支持的价格类型
*/
typedef enum tagPriceMode
{
	PM_Both,		// 市价限价都支持：既支持市价单也支持限价单
	PM_Limit,		// 只支持限价：只允许限价单
	PM_Market,		// 只支持市价：只允许市价单
	PM_None	= 9		// 不支持交易：该合约不支持交易
} PriceMode;

/*
 *	K线数据类型枚举
 *	定义了K线数据中包含的各个字段类型
 *	开、高、低、收、量、额、日期、时间
 */
typedef enum tagKlineFieldType
{
	KFT_OPEN,      // 开盘价字段
	KFT_HIGH,      // 最高价字段
	KFT_LOW,       // 最低价字段
	KFT_CLOSE,     // 收盘价字段
	KFT_DATE,      // 日期字段
	KFT_TIME,      // 时间字段
	KFT_VOLUME,    // 成交量字段
	KFT_SVOLUME    // 小计成交量字段
} WTSKlineFieldType;

/*
 *	K线周期枚举
 *	定义了系统中支持的K线时间周期
 */
typedef enum tagKlinePeriod
{
	KP_Tick,       // Tick级别：最小时间单位
	KP_Minute1,    // 1分钟K线
	KP_Minute5,    // 5分钟K线
	KP_DAY,        // 日K线
	KP_Week,       // 周K线
	KP_Month       // 月K线
} WTSKlinePeriod;

// K线周期名称字符串数组，与枚举值对应
static const char* PERIOD_NAME[] = 
{
	"tick",        // Tick级别名称
	"min1",        // 1分钟名称
	"min5",        // 5分钟名称
	"day",         // 日线名称
	"week",        // 周线名称
	"month"        // 月线名称
};

/*
 *	日志级别枚举
 *	定义了系统中日志记录的详细程度级别
 */
typedef enum tagLogLevel
{
	LL_ALL	= 100, // 所有日志：记录所有级别的日志
	LL_DEBUG,      // 调试日志：记录调试信息
	LL_INFO,       // 信息日志：记录一般信息
	LL_WARN,       // 警告日志：记录警告信息
	LL_ERROR,      // 错误日志：记录错误信息
	LL_FATAL,      // 致命错误：记录致命错误信息
	LL_NONE        // 无日志：不记录任何日志
} WTSLogLevel;

/*
 *	价格类型枚举
 *	定义了订单中使用的各种价格类型
 */
typedef enum tagPriceType
{
	WPT_ANYPRICE	= 0,			// 市价单：以任意价格成交
	WPT_LIMITPRICE,					// 限价单：以指定价格或更好价格成交
	WPT_BESTPRICE,					// 最优价：以最优价格成交
	WPT_LASTPRICE,					// 最新价：以最新成交价成交

	//////////////////////////////////////////////////////////////////////////
	//以下对标CTP的价格类型
	WPT_CTP_LASTPLUSONETICKS = 20,	// 最新价+1ticks：CTP最新价加一个最小变动价位
	WPT_CTP_LASTPLUSTWOTICKS,		// 最新价+2ticks：CTP最新价加两个最小变动价位
	WPT_CTP_LASTPLUSTHREETICKS,		// 最新价+3ticks：CTP最新价加三个最小变动价位
	WPT_CTP_ASK1,					// 卖一价：CTP卖一价
	WPT_CTP_ASK1PLUSONETICKS,		// 卖一价+1ticks：CTP卖一价加一个最小变动价位
	WPT_CTP_ASK1PLUSTWOTICKS,		// 卖一价+2ticks：CTP卖一价加两个最小变动价位
	WPT_CTP_ASK1PLUSTHREETICKS,		// 卖一价+3ticks：CTP卖一价加三个最小变动价位
	WPT_CTP_BID1,					// 买一价：CTP买一价
	WPT_CTP_BID1PLUSONETICKS,		// 买一价+1ticks：CTP买一价加一个最小变动价位
	WPT_CTP_BID1PLUSTWOTICKS,		// 买一价+2ticks：CTP买一价加两个最小变动价位
	WPT_CTP_BID1PLUSTHREETICKS,		// 买一价+3ticks：CTP买一价加三个最小变动价位
	WPT_CTP_FIVELEVELPRICE,			// 五档价，中金所市价：CTP五档价格

	//////////////////////////////////////////////////////////////////////////
	//以下对标DC的价格类型
	WPT_DC_POSTONLY	= 100,			// 只做maker单：DC只做挂单方
	WPT_DC_FOK,						// 全部成交或立即取消：DC全部成交或立即取消
	WPT_DC_IOC,						// 立即成交并取消剩余：DC立即成交并取消剩余
	WPT_DC_OPTLIMITIOC				// 市价委托立即成交并取消剩余：DC市价委托立即成交并取消剩余
} WTSPriceType;

/*
 *	时间条件枚举
 *	定义了订单的有效时间条件
 */
typedef enum tagTimeCondition
{
	WTC_IOC		= '1',	// 立即完成,否则撤销：立即成交否则撤销
	WTC_GFS,			// 本节有效：当前交易节有效
	WTC_GFD,			// 当日有效：当日有效
} WTSTimeCondition;

/*
 *	订单标志枚举
 *	定义了订单的特殊标志
 */
typedef enum tagOrderFlag
{
	WOF_NOR = '0',		// 普通订单：标准订单
	WOF_FAK,			// FAK：Fill and Kill，部分成交后撤销剩余
	WOF_FOK,			// FOK：Fill or Kill，全部成交或全部撤销
} WTSOrderFlag;

/*
 *	开平方向枚举
 *	定义了期货交易中的开仓和平仓方向
 */
typedef enum tagOffsetType
{
	WOT_OPEN			= '0',	// 开仓：建立新的持仓
	WOT_CLOSE,					// 平仓,上期为平昨：平仓，上期指平昨仓
	WOT_FORCECLOSE,				// 强平：强制平仓
	WOT_CLOSETODAY,				// 平今：平掉今日建立的持仓
	WOT_CLOSEYESTERDAY,			// 平昨：平掉昨日建立的持仓
} WTSOffsetType;

/*
 *	多空方向枚举
 *	定义了交易的多空方向
 */
typedef enum tagDirectionType
{
	WDT_LONG			= '0',	// 做多：买入做多
	WDT_SHORT,					// 做空：卖出做空
	WDT_NET						// 净：净持仓方向
} WTSDirectionType;

/*
 *	业务类型枚举
 *	定义了交易业务的不同类型
 */
typedef enum tagBusinessType
{
	BT_CASH		= '0',	// 普通买卖：现金交易
	BT_ETF		= '1',	// ETF申赎：ETF申购赎回
	BT_EXECUTE	= '2',	// 期权行权：期权执行
	BT_QUOTE	= '3',	// 期权报价：期权报价
	BT_FORQUOTE = '4',	// 期权询价：期权询价
	BT_FREEZE	= '5',	// 期权对锁：期权对锁
	BT_CREDIT	= '6',	// 融资融券：信用交易
	BT_UNKNOWN			// 未知业务类型：未识别的业务类型
} WTSBusinessType;

/*
 *	订单操作类型枚举
 *	定义了对订单可以进行的操作
 */
typedef enum tagActionFlag
{
	WAF_CANCEL			= '0',	// 撤销：撤销订单
	WAF_MODIFY			= '3',	// 修改：修改订单
} WTSActionFlag;

/*
 *	订单状态枚举
 *	定义了订单在交易过程中的各种状态
 */
typedef enum tagOrderState
{
	WOS_AllTraded				= '0',	// 全部成交：订单已全部成交
	WOS_PartTraded_Queuing,				// 部分成交,仍在队列中：部分成交且仍在排队
	WOS_PartTraded_NotQueuing,			// 部分成交,未在队列：部分成交但已不在排队
	WOS_NotTraded_Queuing,				// 未成交：未成交且在排队
	WOS_NotTraded_NotQueuing,			// 未成交,未在队列：未成交且不在排队
	WOS_Canceled,						// 已撤销：订单已被撤销
	WOS_Submitting				= 'a',	// 正在提交：订单正在提交中
	WOS_Cancelling,						// 在撤：订单正在撤销中
	WOS_Nottouched,						// 未触发：条件单未触发
} WTSOrderState;

/*
 *	订单类型枚举
 *	定义了订单的不同类型
 */
typedef enum tagOrderType
{
	WORT_Normal			= 0,		// 正常订单：普通交易订单
	WORT_Exception,					// 异常订单：异常情况下的订单
	WORT_System,					// 系统订单：系统自动生成的订单
	WORT_Hedge						// 对冲订单：对冲交易订单
} WTSOrderType;

/*
 *	成交类型枚举
 *	定义了成交的不同类型
 */
typedef enum tagTradeType
{
	WTT_Common				= '0',	// 普通：普通成交
	WTT_OptionExecution		= '1',	// 期权执行：期权执行成交
	WTT_OTC					= '2',	// OTC成交：场外成交
	WTT_EFPDerived			= '3',	// 期转现衍生成交：期转现衍生成交
	WTT_CombinationDerived	= '4'	// 组合衍生成交：组合衍生成交
} WTSTradeType;


/*
 *	错误代码枚举
 *	定义了系统中可能出现的各种错误类型
 */
typedef enum tagErrorCode
{
	WEC_NONE			=	0,		// 没有错误：操作成功
	WEC_ORDERINSERT,				// 下单错误：下单操作失败
	WEC_ORDERCANCEL,				// 撤单错误：撤单操作失败
	WEC_EXECINSERT,					// 行权指令错误：行权指令操作失败
	WEC_EXECCANCEL,					// 行权撤销错误：行权撤销操作失败
	WEC_UNKNOWN			=	9999	// 未知错误：未识别的错误类型
} WTSErroCode;

/*
 *	比较字段枚举
 *	定义了条件单中用于比较的字段类型
 */
typedef enum tagCompareField
{
	WCF_NEWPRICE			=	0,	// 最新价：与最新价格比较
	WCF_BIDPRICE,					// 买一价：与买一价格比较
	WCF_ASKPRICE,					// 卖一价：与卖一价格比较
	WCF_PRICEDIFF,					// 价差,止盈止损专用：与价格差值比较
	WCF_NONE				=	9	// 不比较：不进行价格比较
} WTSCompareField;

/*
 *	比较类型枚举
 *	定义了条件单中价格比较的方式
 */
typedef enum tagCompareType
{
	WCT_Equal			= 0,		// 等于：价格等于条件
	WCT_Larger,						// 大于：价格大于条件
	WCT_Smaller,					// 小于：价格小于条件
	WCT_LargerOrEqual,				// 大于等于：价格大于等于条件
	WCT_SmallerOrEqual				// 小于等于：价格小于等于条件
}WTSCompareType;

/*
 *	行情解析器事件枚举
 *	定义了行情解析器可能产生的事件类型
 */
typedef enum tagParserEvent
{
	WPE_Connect			= 0,		// 连接事件：连接建立
	WPE_Close,						// 关闭事件：连接关闭
	WPE_Login,						// 登录：登录成功
	WPE_Logout						// 注销：注销登录
}WTSParserEvent;

/*
 *	交易模块事件枚举
 *	定义了交易模块可能产生的事件类型
 */
typedef enum tagTraderEvent
{
	WTE_Connect			= 0,		// 连接事件：连接建立
	WTE_Close,						// 关闭事件：连接关闭
	WTE_Login,						// 登录：登录成功
	WTE_Logout						// 注销：注销登录
}WTSTraderEvent;

/*
 *	交易状态枚举
 *	定义了交易时段的不同状态
 */
typedef enum tagTradeStatus
{
	TS_BeforeTrading	= '0',	// 开盘前：交易开始前的状态
	TS_NotTrading		= '1',	// 非交易：非交易时段
	TS_Continous		= '2',	// 连续竞价：连续竞价时段
	TS_AuctionOrdering	= '3',	// 集合竞价下单：集合竞价下单时段
	TS_AuctionBalance	= '4',	// 集合竞价平衡：集合竞价平衡时段
	TS_AuctionMatch		= '5',	// 集合竞价撮合：集合竞价撮合时段
	TS_Closed			= '6'	// 收盘：交易结束
}WTSTradeStatus;

/*
 *	买卖方向类型定义
 *	使用32位无符号整数表示买卖方向
 */
typedef uint32_t WTSBSDirectType;
#define BDT_Buy		'B'	// 买入：买入方向标识
#define BDT_Sell	'S'	// 卖出：卖出方向标识
#define BDT_Unknown ' '	// 未知：未知方向标识
#define BDT_Borrow	'G'	// 借入：借入方向标识
#define BDT_Lend	'F'	// 借出：借出方向标识

/*
 *	成交类型定义
 *	使用32位无符号整数表示成交类型
 */
typedef uint32_t WTSTransType;
#define TT_Unknown	'U'	// 未知类型：未识别的成交类型
#define TT_Match	'M'	// 撮合成交：通过撮合机制成交
#define TT_Cancel	'C'	// 撤单：订单撤销

/*
 *	委托明细类型定义
 *	使用32位无符号整数表示委托明细类型
 */
typedef uint32_t WTSOrdDetailType;
#define ODT_Unknown		0	// 未知类型：未识别的委托类型
#define ODT_BestPrice	'U'	// 本方最优：本方最优价格
#define ODT_AnyPrice	'1'	// 市价：市价委托
#define ODT_LimitPrice	'2'	// 限价：限价委托

NS_WTP_END  // 结束WonderTrader命名空间  // 结束WonderTrader命名空间