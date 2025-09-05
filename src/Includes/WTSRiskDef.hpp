/*!
 * \file WTSRiskDef.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WT风控相关数据定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统的风险控制相关数据结构，用于监控和管理交易风险。
 * 
 * 主要功能：
 * 1. 交易统计信息(TradeStatInfo)：记录开平仓、挂单、撤单等交易行为统计
 * 2. 交易状态信息(WTSTradeStateInfo)：封装交易统计信息，提供便捷的访问接口
 * 3. 资金结构(WTSFundStruct)：记录账户资金变化和风险指标
 * 4. 组合资金信息(WTSPortFundInfo)：管理投资组合的资金状况
 * 
 * 设计特点：
 * - 全面的交易统计：涵盖开平仓、挂单、撤单、错单等各个方面
 * - 风险指标监控：跟踪最大/最小动态权益、日内高低点等关键指标
 * - 时间维度分析：记录关键时间点和对应的风险状态
 * - 多空方向统计：分别统计多空方向的交易行为
 * - 便于风控系统：为风险控制系统提供完整的数据支持
 */
#pragma once
#include "WTSObject.hpp"  // 包含WonderTrader基础对象类，提供引用计数功能

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * 交易统计信息结构体
 * 
 * 功能概述：
 * 详细记录单个合约的交易行为统计信息，包括开平仓、挂单、撤单、错单等各个维度。
 * 是风险控制系统的基础数据结构，用于监控交易频率、成功率等关键指标。
 * 
 * 统计维度：
 * 1. 开平仓统计：区分多空方向，记录开仓、平仓、平今等操作
 * 2. 挂单统计：记录委买委卖的笔数和数量
 * 3. 撤单统计：包括主动撤单和自动撤单的详细统计
 * 4. 错单统计：记录交易过程中的错误操作
 * 
 * 应用场景：
 * - 日内交易行为分析
 * - 风险控制阈值监控
 * - 交易策略效果评估
 * - 合规性检查和报告
 */
typedef struct _TradeStatInfo
{
	char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码，如"rb2305"、"IF2303"等

	/*
	 * 开平仓统计区域
	 * 区分多空方向，分别统计各种开平仓操作的数量
	 */
	double	l_openvol;	    // 当日开多仓量：买入开仓的总手数
	double	l_closevol;	    // 当日平多仓量：卖出平仓的总手数（包括平今和平昨）
	double	l_closetvol;    // 当日平今多仓量：卖出平今的总手数（当日开仓当日平仓）
	double	s_openvol;	    // 当日开空仓量：卖出开仓的总手数
	double	s_closevol;	    // 当日平空仓量：买入平仓的总手数（包括平今和平昨）
	double	s_closetvol;    // 当日平今空仓量：买入平今的总手数（当日开仓当日平仓）

	/*
	 * 挂单统计区域
	 * 统计委托单的提交情况，用于监控交易活跃度
	 */
	uint32_t	b_orders;	// 委买笔数：提交的买入委托单数量
	double		b_ordqty;	// 委买数量：买入委托的总手数
	uint32_t	s_orders;	// 委卖笔数：提交的卖出委托单数量
	double		s_ordqty;	// 委卖数量：卖出委托的总手数

	/*
	 * 主动撤单统计区域
	 * 统计用户主动取消的委托单情况
	 */
	uint32_t	b_cancels;	// 撤买笔数：主动撤销的买入委托数量
	double		b_canclqty;	// 撤买数量：主动撤销的买入委托总手数
	uint32_t	s_cancels;	// 撤卖笔数：主动撤销的卖出委托数量
	double		s_canclqty;	// 撤卖数量：主动撤销的卖出委托总手数

	/*
	 * 自动撤单统计区域
	 * 统计系统自动撤销的委托单情况（如超时、风控等原因）
	 */
	uint32_t	b_auto_cancels;		// 自动撤买笔数：系统自动撤销的买入委托数量
	double		b_auto_canclqty;	// 自动撤买数量：系统自动撤销的买入委托总手数
	uint32_t	s_auto_cancels;		// 自动撤卖笔数：系统自动撤销的卖出委托数量
	double		s_auto_canclqty;	// 自动撤卖数量：系统自动撤销的卖出委托总手数

	/*
	 * 错单统计区域
	 * 统计交易过程中出现的各种错误情况
	 */
	uint32_t	b_wrongs;	// 买入错单笔数：买入方向的错误委托数量
	double		b_wrongqty;	// 买入错单数量：买入方向的错误委托总手数
	uint32_t	s_wrongs;	// 卖出错单笔数：卖出方向的错误委托数量
	double		s_wrongqty;	// 卖出错单数量：卖出方向的错误委托总手数

	uint32_t	_infos;		// 信息量：记录统计信息的更新次数或其他元数据

	/**
	 * 构造函数
	 * 将所有统计数据初始化为0，确保统计的准确性
	 */
	_TradeStatInfo()
	{
		memset(this, 0, sizeof(_TradeStatInfo));  // 使用memset将整个结构体清零
	}
} TradeStatInfo;

/**
 * 交易状态信息类
 * 封装交易统计信息，提供便捷的访问接口
 * 主要功能：
 * - 管理开平仓统计信息
 * - 跟踪挂单和撤单情况
 * - 监控错单统计
 * - 提供多空方向分别统计
 */
/**
 * 交易状态信息类
 * 
 * 功能概述：
 * 封装TradeStatInfo结构体，提供面向对象的访问接口和便捷的统计方法。
 * 继承自WTSObject，支持引用计数和自动内存管理，便于在系统中传递和存储。
 * 
 * 主要特性：
 * - 面向对象封装：将C结构体包装成C++类
 * - 便捷访问接口：提供语义化的方法名访问统计数据
 * - 聚合统计功能：提供总计类的统计方法
 * - 内存管理：继承引用计数机制，安全可靠
 * 
 * 使用场景：
 * - 风险控制模块的数据传递
 * - 交易统计信息的存储和查询
 * - 实时监控系统的数据展示
 * - 历史数据分析和报告生成
 */
class WTSTradeStateInfo : public WTSObject
{
protected:
	/**
	 * 保护构造函数
	 * 防止直接实例化，必须通过静态工厂方法创建
	 */
	WTSTradeStateInfo(){}

public:
	/**
	 * 静态工厂方法：创建交易状态信息对象
	 * 
	 * @param code 合约代码，如"rb2305"、"IF2303"等
	 * @return 新创建的交易状态信息对象指针
	 * 
	 * 使用示例：
	 * WTSTradeStateInfo* tradeState = WTSTradeStateInfo::create("rb2305");
	 */
	static WTSTradeStateInfo* create(const char* code)
	{
		WTSTradeStateInfo* pRet = new WTSTradeStateInfo();  // 创建新实例
		wt_strcpy(pRet->_trd_stat_info._code, code);        // 设置合约代码

		return pRet;  // 返回创建的实例
	}

	/**
	 * 获取内部统计信息结构体的引用（非常量版本）
	 * @return 交易统计信息结构体的引用，可用于修改
	 */
	inline TradeStatInfo&	statInfo(){ return _trd_stat_info; }
	
	/**
	 * 获取内部统计信息结构体的引用（常量版本）
	 * @return 交易统计信息结构体的常量引用，只读访问
	 */
	inline const TradeStatInfo& statInfo() const{ return _trd_stat_info; }

	/**
	 * 获取合约代码
	 * @return 合约代码字符串
	 */
	inline const char* code() const{ return _trd_stat_info._code; }

	/*
	 * 开平仓统计信息访问接口
	 * 提供语义化的方法名，便于理解和使用
	 */
	inline double open_volume_long() const{ return _trd_stat_info.l_openvol; }      // 多头开仓量
	inline double close_volume_long() const{ return _trd_stat_info.l_closevol; }    // 多头平仓量
	inline double closet_volume_long() const{ return _trd_stat_info.l_closetvol; }  // 多头平今量
	inline double open_volume_short() const{ return _trd_stat_info.s_openvol; }     // 空头开仓量
	inline double close_volume_short() const{ return _trd_stat_info.s_closevol; }   // 空头平仓量
	inline double closet_volume_short() const{ return _trd_stat_info.s_closetvol; } // 空头平今量

	/*
	 * 挂单统计信息访问接口
	 * 分别提供买卖方向的委托统计
	 */
	inline uint32_t orders_buy() const{ return _trd_stat_info.b_orders; }   // 买入委托笔数
	inline double ordqty_buy() const{ return _trd_stat_info.b_ordqty; }     // 买入委托数量
	inline uint32_t orders_sell() const{ return _trd_stat_info.s_orders; }  // 卖出委托笔数
	inline double ordqty_sell() const{ return _trd_stat_info.s_ordqty; }    // 卖出委托数量

	/*
	 * 撤单统计信息访问接口
	 * 分别提供买卖方向的撤单统计
	 */
	inline uint32_t cancels_buy() const{ return _trd_stat_info.b_cancels; }     // 买入撤单笔数
	inline double cancelqty_buy() const{ return _trd_stat_info.b_canclqty; }    // 买入撤单数量
	inline uint32_t cancels_sell() const{ return _trd_stat_info.s_cancels; }    // 卖出撤单笔数
	inline double cancelqty_sell() const{ return _trd_stat_info.s_canclqty; }   // 卖出撤单数量

	/*
	 * 聚合统计信息接口
	 * 提供跨买卖方向的总计统计，便于整体分析
	 */
	inline uint32_t total_cancels() const{ return _trd_stat_info.b_cancels + _trd_stat_info.s_cancels; }  // 总撤单笔数
	inline uint32_t total_orders() const { return _trd_stat_info.b_orders + _trd_stat_info.s_orders; }    // 总委托笔数

	/**
	 * 获取信息量统计
	 * @return 信息量数值，通常用于记录统计更新次数
	 */
	inline uint32_t infos() const { return _trd_stat_info._infos; }

private:
	TradeStatInfo	_trd_stat_info;  // 内部交易统计信息结构体实例
};

/**
 * 组合资金数据结构体
 * 
 * 功能概述：
 * 记录投资组合的完整资金状况，包括权益变化、盈亏情况、费用统计等。
 * 同时跟踪日内和历史的风险指标，为风险控制和绩效分析提供数据支持。
 * 
 * 数据类型：
 * 1. 权益数据：静态权益、动态权益、期初权益等
 * 2. 盈亏数据：已实现盈亏（平仓盈亏）和未实现盈亏（浮动盈亏）
 * 3. 费用数据：交易佣金、手续费等成本统计
 * 4. 风险指标：日内和历史的最大/最小动态权益
 * 5. 时间信息：关键时点的时间戳记录
 * 
 * 应用场景：
 * - 实时风险监控和预警
 * - 账户资金状况查询
 * - 绩效分析和报告生成
 * - 风险指标计算（如最大回撤等）
 */
typedef struct _WTSFundStruct
{
	/*
	 * 基础权益信息
	 * 记录账户的各类权益数据，是资金管理的核心指标
	 */
	double		_predynbal;		// 期初动态权益：交易日开始时的动态权益值
	double		_prebalance;	// 期初静态权益：交易日开始时的静态权益值
	double		_balance;		// 当前静态权益：不含浮动盈亏的账户净值
	
	/*
	 * 盈亏统计信息
	 * 分别记录已实现和未实现的盈亏情况
	 */
	double		_profit;		// 平仓盈亏：已实现的盈亏，即已平仓位的损益
	double		_dynprofit;		// 浮动盈亏：未实现的盈亏，即持仓的浮动损益
	
	/*
	 * 费用统计信息
	 * 记录交易过程中产生的各种费用
	 */
	double		_fees;			// 佣金费用：交易佣金、手续费等总计
	
	/*
	 * 时间信息
	 * 记录关键的时间节点，用于数据有效性判断
	 */
	uint32_t	_last_date;		// 上次结算交易日：格式YYYYMMDD，如20230315
	int64_t		_update_time;	// 数据更新时间：时间戳格式，记录最后更新时刻

	/*
	 * 日内风险指标
	 * 跟踪当日交易过程中的权益波动情况
	 */
	double		_max_dyn_bal;	// 日内最大动态权益：当日达到的最高净值
	uint32_t	_max_time;		// 日内高点时间：最大权益出现的时间（HHMM格式）
	double		_min_dyn_bal;	// 日内最小动态权益：当日达到的最低净值
	uint32_t	_min_time;		// 日内低点时间：最小权益出现的时间（HHMM格式）

	/**
	 * 动态权益配对结构体
	 * 用于记录特定日期的权益值，便于历史数据管理
	 */
	typedef struct _DynBalPair
	{
		uint32_t	_date;			// 日期：YYYYMMDD格式
		double		_dyn_balance;	// 该日期的动态权益值

		/**
		 * 构造函数：初始化所有字段为0
		 */
		_DynBalPair()
		{
			memset(this, 0, sizeof(_DynBalPair));
		}
	} DynBalPair;

	/*
	 * 历史风险指标
	 * 跟踪账户历史上的极值情况，用于长期风险评估
	 */
	DynBalPair	_max_md_dyn_bal;	// 历史最大动态权益：包含日期和权益值
	DynBalPair	_min_md_dyn_bal;	// 历史最小动态权益：包含日期和权益值

	/**
	 * 构造函数
	 * 初始化所有数据为0，并设置特殊字段的初始值
	 */
	_WTSFundStruct()
	{
		memset(this, 0, sizeof(_WTSFundStruct));  // 将整个结构体清零
		_max_dyn_bal = DBL_MAX;  // 初始化日内最大值为极大值，便于后续比较
		_min_dyn_bal = DBL_MAX;  // 初始化日内最小值为极大值，便于后续比较
	}
} WTSFundStruct;


/**
 * 组合资金信息类
 * 管理投资组合的资金状况和风险指标
 * 主要功能：
 * - 跟踪权益变化
 * - 监控盈亏情况
 * - 记录费用统计
 * - 跟踪日内和跨日风险指标
 */
class WTSPortFundInfo : public WTSObject
{
protected:
	WTSPortFundInfo(){}

public:
	static WTSPortFundInfo* create()
	{
		WTSPortFundInfo* pRet = new WTSPortFundInfo();
		return pRet;
	}

	WTSFundStruct&	fundInfo(){ return _fund_info; }
	const WTSFundStruct& fundInfo() const{ return _fund_info; }

	double predynbalance() const{ return _fund_info._predynbal; }
	double balance() const{ return _fund_info._balance; }
	double profit() const{ return _fund_info._profit; }
	double dynprofit() const{ return _fund_info._dynprofit; }
	double fees() const{ return _fund_info._fees; }

	double max_dyn_balance() const{ return _fund_info._max_dyn_bal; }
	double min_dyn_balance() const{ return _fund_info._min_dyn_bal; }

	double max_md_dyn_balance() const{ return _fund_info._max_md_dyn_bal._dyn_balance; }
	double min_md_dyn_balance() const{ return _fund_info._min_md_dyn_bal._dyn_balance; }

	uint32_t max_dynbal_time() const{ return _fund_info._max_time; }
	uint32_t min_dynbal_time() const{ return _fund_info._min_time; }

	uint32_t last_settle_date() const{ return _fund_info._last_date; }

	uint32_t max_md_dynbal_date() const{ return _fund_info._max_md_dyn_bal._date; }
	uint32_t min_md_dynbal_date() const{ return _fund_info._min_md_dyn_bal._date; }


private:
	WTSFundStruct	_fund_info;
};

NS_WTP_END