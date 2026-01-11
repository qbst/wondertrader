/*!
 * \file WtStockMinImpactExeUnit.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtStockMinImpactExeUnit股票最小冲击执行单元类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了WtStockMinImpactExeUnit类，这是股票最小冲击执行单元的实现类，继承自ExecuteUnit基类。
 * 该执行单元专门用于股票和可转债的智能订单执行，通过控制订单价格和数量，最小化对市场的冲击。
 * 
 * 设计目标：
 * 1. 实现股票市场的最小市场冲击算法，通过控制订单价格和数量减少对市场的影响
 * 2. 支持多种目标模式：股数模式、金额模式、比例模式
 * 3. 支持多种价格模式：最优价、最新价、对手价、自动价格
 * 4. 支持订单超时自动撤单机制
 * 5. 支持按比例或固定数量下单
 * 6. 支持账户信息回调，根据账户资金动态调整执行策略
 * 7. 支持科创板股票的特殊处理（最小下单200股）
 * 8. 支持可转债的特殊处理（T+0交易、最小下单10张）
 * 
 * 核心功能：
 * - 目标驱动执行：支持股数、金额、比例三种目标模式
 * - 价格控制：根据价格模式和价格偏移计算订单价格
 * - 数量控制：根据配置的单次发单手数或比例下单
 * - 订单管理：跟踪订单状态，支持超时撤单
 * - 市场数据响应：根据Tick数据实时调整订单策略
 * - 账户管理：根据账户资金动态调整执行策略
 * - 错单检测：检测无法撤单的订单（可能是错单）
 * 
 * 股票市场特点：
 * - 最小下单单位：普通股票100股，科创板股票200股，可转债10张
 * - T+1交易规则：股票T+1，可转债T+0
 * - 涨跌停限制：涨跌停价的挂单不能撤单
 * - 账户资金限制：买入受可用资金限制，卖出受持仓限制
 * 
 * 执行算法：
 * - 根据目标模式计算目标仓位（股数/金额/比例）
 * - 计算目标仓位与当前仓位的差值
 * - 根据价格模式（最优价/最新价/对手价）确定订单价格
 * - 根据价格偏移调整订单价格（买入+偏移，卖出-偏移）
 * - 根据配置的单次发单手数或比例下单
 * - 监控订单超时，自动撤单并重新下单
 * - 检测错单（超过最大撤单次数仍无法撤单）
 * 
 * 架构特点：
 * - 使用互斥锁保护计算逻辑，确保线程安全
 * - 使用原子标志防止并发计算
 * - 使用订单管理器跟踪订单生命周期
 * - 支持多种价格模式和下单策略
 * - 支持账户资金管理和动态调整
 */
#pragma once
#include "../Includes/ExecuteDefs.h"  // 执行单元定义文件（包含ExecuteUnit基类）
#include "WtOrdMon.h"  // 订单管理器类定义
#include "../Includes/WTSVariant.hpp"  // 变体类型定义
#include "../Includes/WTSContractInfo.hpp"  // 合约信息定义
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息定义
#include "../Share/decimal.h"  // 精确小数计算
#include "../Share/StrUtil.hpp"  // 字符串工具函数
#include "../Share/fmtlib.h"  // 格式化库

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 价格模式常量定义
 * 
 * 定义订单价格模式的常量值
 */
#define BESTPX -1  // 己方最优价（买入用买一价，卖出用卖一价）
#define LASTPX 0   // 最新价（最新成交价）
#define MARKET 1   // 对手价（买入用卖一价，卖出用买一价）
#define AUTOPX 2   // 自动价格（根据市场情况自动选择）

/**
 * @brief 账户持仓枚举回调函数类型
 * 
 * 用于枚举账户持仓时的回调函数
 * 
 * @param stdCode 合约代码
 * @param isBuy true表示多头持仓，false表示空头持仓
 * @param position 持仓数量
 * @param avgpx 持仓均价
 * @param profit 持仓盈亏
 * @param dynprofit 动态盈亏
 */
typedef std::function<void(const char*, bool, double, double, double, double)> FuncEnumChnlPosCallBack;

/**
 * @brief WtStockMinImpactExeUnit股票最小冲击执行单元类
 * 
 * 继承自ExecuteUnit基类，实现股票市场的最小市场冲击算法，用于股票和可转债的智能订单执行。
 * 该执行单元通过控制订单价格和数量，最小化对市场的冲击，实现最优的执行效果。
 * 
 * 股票市场特点：
 * - 最小下单单位：普通股票100股，科创板股票200股，可转债10张
 * - T+1交易规则：股票T+1，可转债T+0
 * - 涨跌停限制：涨跌停价的挂单不能撤单
 * - 账户资金限制：买入受可用资金限制，卖出受持仓限制
 */
class WtStockMinImpactExeUnit : public ExecuteUnit
{
private:
	const char* cbondStr = "CBOND";  // 可转债产品类型字符串常量
	const char* stockStr = "STK";  // 股票产品类型字符串常量

	/**
	 * @brief 目标模式枚举
	 * 
	 * 定义目标仓位的三种模式
	 */
	enum class TargetMode
	{
		stocks = 0,  // 股数模式：目标仓位以股数为单位
		amount,      // 金额模式：目标仓位以金额为单位（需要根据价格转换为股数）
		ratio,       // 比例模式：目标仓位以持仓比例为基准（需要根据总资金和价格转换为股数）
	};

private:
	/**
	 * @brief 价格模式名称数组
	 * 
	 * 价格模式的字符串名称，用于日志和配置
	 */
	std::vector<std::string> PriceModeNames = {
		"BESTPX",		// 最优价
		"LASTPX",		// 最新价
		"MARKET",		// 对手价
		"AUTOPX"		// 自动
	};

public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建股票最小冲击执行单元实例，初始化所有成员变量。
	 */
	WtStockMinImpactExeUnit();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
	 */
	virtual ~WtStockMinImpactExeUnit();

private:
	/**
	 * @brief 执行计算
	 * 
	 * 核心计算函数，根据目标仓位和当前仓位计算需要执行的差量，
	 * 并根据市场行情和配置参数决定是否下单、撤单等操作。
	 * 
	 * 该函数是私有函数，由on_tick()等回调函数调用。
	 */
	void	do_calc();
	
	/**
	 * @brief 检查是否清仓
	 * 
	 * 检查目标仓位是否为清仓指令（DBL_MAX）。
	 * 
	 * @return bool true表示清仓，false表示不清仓
	 */
	bool	is_clear();
	
public:
	/**
	 * @brief 获取所属执行器工厂名称
	 * 
	 * 返回创建该执行单元的工厂名称。
	 * 
	 * @return const char* 返回工厂名称字符串（"WtExeFact"）
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual const char* getFactName() override;

	/**
	 * @brief 获取执行单元名称
	 * 
	 * 返回执行单元的名称，用于标识和管理。
	 * 
	 * @return const char* 返回执行单元名称字符串（"WtStockMinImpactExeUnit"）
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual const char* getName() override;

	/**
	 * @brief 初始化执行单元
	 * 
	 * 初始化执行单元，加载配置参数，获取合约信息和交易时段信息。
	 * 
	 * @param ctx 执行单元运行环境指针，提供合约信息、交易时段信息等
	 * @param stdCode 管理的合约代码（标准格式）
	 * @param cfg 配置对象指针，包含执行单元的各种配置参数
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void init(ExecuteContext* ctx, const char* stdCode, WTSVariant* cfg) override;

	/**
	 * @brief 订单回报处理
	 * 
	 * 当订单状态发生变化时调用此函数，更新订单状态，处理撤单等操作。
	 * 
	 * @param localid 本地订单ID（下单时返回的订单ID）
	 * @param stdCode 合约代码
	 * @param isBuy true表示买入订单，false表示卖出订单
	 * @param leftover 剩余未成交数量
	 * @param price 委托价格
	 * @param isCanceled 是否已撤销，true表示已撤销，false表示未撤销
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled) override;

	/**
	 * @brief Tick数据回调
	 * 
	 * 当有新的Tick数据时调用此函数，更新最新行情，触发执行计算。
	 * 
	 * @param newTick 最新的Tick数据指针，包含最新价格、成交量、持仓量等信息
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_tick(WTSTickData* newTick) override;

	/**
	 * @brief 成交回报处理
	 * 
	 * 当订单有成交回报时调用此函数，更新持仓和未完成订单数量。
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy true表示买入成交，false表示卖出成交
	 * @param vol 成交数量（这里没有正负，通过isBuy确定买入还是卖出）
	 * @param price 成交价格
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price) override;

	/**
	 * @brief 下单结果回报处理
	 * 
	 * 当下单请求收到回报时调用此函数，处理下单成功或失败的情况。
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param bSuccess 是否成功，true表示下单成功，false表示下单失败
	 * @param message 返回消息（如果失败，包含失败原因）
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message) override;

	/**
	 * @brief 设置新的目标仓位
	 * 
	 * 设置执行单元的目标仓位，执行单元会自动计算差量并执行。
	 * 根据目标模式（股数/金额/比例），目标仓位的含义不同。
	 * 
	 * @param stdCode 合约代码
	 * @param newVol 新的目标仓位：
	 *   - 股数模式：目标股数（正数表示买入，负数表示卖出，DBL_MAX表示清仓）
	 *   - 金额模式：目标金额（正数表示买入金额，负数表示卖出金额）
	 *   - 比例模式：目标持仓比例（0-1之间的比例值）
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void set_position(const char* stdCode, double newVol) override;

	/**
	 * @brief 清理全部持仓
	 * 
	 * 清理指定合约的全部持仓，将所有订单撤单并清空目标仓位。
	 * 
	 * @param stdCode 合约代码
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void clear_all_position(const char* stdCode) override;

	/**
	 * @brief 交易通道就绪回调
	 * 
	 * 当交易通道连接成功并准备就绪时调用此函数，可以开始下单。
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_channel_ready() override;

	/**
	 * @brief 交易通道丢失回调
	 * 
	 * 当交易通道断开时调用此函数，停止下单操作。
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_channel_lost() override;

	/**
	 * @brief 账户信息回调
	 * 
	 * 当账户信息更新时调用此函数，更新账户资金信息，用于动态调整执行策略。
	 * 
	 * @param currency 币种
	 * @param prebalance 上日余额
	 * @param balance 当前余额
	 * @param dynbalance 动态权益
	 * @param avaliable 可用资金
	 * @param closeprofit 已实现盈亏
	 * @param dynprofit 动态盈亏
	 * @param margin 保证金占用
	 * @param fee 手续费
	 * @param deposit 入金
	 * @param withdraw 出金
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void on_account(const char* currency, double prebalance, double balance, double dynbalance, double avaliable, double closeprofit, double dynprofit, double margin, double fee, double deposit, double withdraw) override;

private:
	/**
	 * @brief 检查未管理订单
	 * 
	 * 检查是否有未管理的订单（可能是错单或外部订单），
	 * 如果配置允许，则自动撤单。
	 */
	void check_unmanager_order();

private:
	WTSTickData* _last_tick;  // 上一笔行情数据指针，用于获取最新价格等信息
	double		_target_pos;  // 目标仓位（股数模式下的目标股数，正数表示买入，负数表示卖出，DBL_MAX表示清仓）
	double		_target_amount;  // 目标金额（金额模式下的目标金额，正数表示买入金额，负数表示卖出金额）
	double		_target_ratio;  // 目标持仓比例（比例模式下的目标比例，0-1之间的值）

	double		_avaliable{ 0 };  // 账户可用资金（用于金额模式和比例模式的计算）

	StdUniqueMutex	_mtx_calc;  // 计算逻辑的互斥锁，保证线程安全
	WTSCommodityInfo* _comm_info;  // 合约信息指针，包含合约的基本信息（如最小变动价位、合约乘数等）
	WTSSessionInfo* _sess_info;  // 交易时段信息指针，包含交易时间、休市时间等信息

	//////////////////////////////////////////////////////////////////////////
	//执行参数
	int32_t		_price_offset;  // 价格偏移跳数（相对于基准价格的偏移，买入+偏移，卖出-偏移）
	uint32_t	_expire_secs;  // 订单超时秒数（订单创建后超过此时间未成交则自动撤单）
	int32_t		_price_mode;  // 价格模式：-1-最优价，0-最新价，1-对手价，2-自动价格
	uint32_t	_entrust_span;  // 发单时间间隔（单位：毫秒，两次下单之间的最小时间间隔）
	bool		_by_rate;  // 是否按照对手盘挂单数的比例下单，true表示按比例（rate字段生效），false表示固定数量（lots字段生效）
	double		_order_lots;  // 单次发单手数（当by_rate为false时使用）
	double		_qty_rate;  // 下单手数比例（当by_rate为true时使用，如0.1表示每次下对手盘挂单量的10%）
	double		_min_order;  // 最小下单数量（根据股票类型自动计算：普通股票100股，科创板200股，可转债10张）
	bool		_is_finish;  // 是否已完成执行标志，true表示已完成，false表示未完成
	uint64_t	_start_time;  // 开始执行时间（毫秒时间戳，用于统计执行时间）
	double		_start_price{ 0 };  // 开始执行时的价格（用于计算执行效果）
	bool		_is_first_tick{ true };  // 是否是第一个tick标志，true表示是第一个tick
	double		_max_cancel_time{ 3 };  // 最大撤单次数（如果超过这个次数仍然未撤单，则说明是错单）
	double		_total_money{ -1 };  // 总资本（用于比例模式的计算，-1表示未设置）
	double		_is_t0{ false };  // 是否T+0交易标志（对于转债等来说，这个需要是true，股票为false）
	wt_hashmap< uint32_t, uint32_t > _cancel_map{};  // 撤单次数映射表（订单ID -> 撤单次数，用于错单检测）

	WtOrdMon	_orders_mon;  // 订单管理器，用于跟踪和管理订单状态
	//uint32_t	_cancel_cnt;  // 在途撤单量（已废弃）
	uint32_t	_cancel_times;  // 撤单次数（累计撤单次数统计）
	bool		_is_cancel_unmanaged_order{ true };  // 是否撤消未管理订单标志，true表示自动撤消未管理订单，false表示不撤消
	uint64_t	_last_place_time;  // 上个下单时间（毫秒时间戳，用于控制发单间隔）
	uint64_t	_last_tick_time;  // 上个tick时间（毫秒时间戳，用于判断是否有新行情）
	bool		_is_clear;  // 是否清仓标志，true表示正在清仓，false表示不清仓
	TargetMode  _target_mode{ TargetMode::stocks };  // 目标模式（股数/金额/比例）
	bool		_is_KC{ false };  // 是否是科创板股票标志（科创板代码>=688000）
	double		_min_hands{ 0 };  // 最小手数（根据股票类型自动计算）
	bool		_is_ready{ false };  // 是否准备就绪标志（交易通道就绪且账户信息就绪）
	bool		_is_total_money_ready{ false };  // 总资本是否就绪标志（用于比例模式）
	std::map<std::string, double> _market_value{};  // 市值映射表（合约代码 -> 市值，用于比例模式的计算）
	uint64_t _now;  // 当前时间（毫秒时间戳）

public:
	/**
	 * @brief 手数取整
	 * 
	 * 将手数取整到最小手数的整数倍。
	 * 例如：如果最小手数是100，则123股取整为100股，156股取整为200股。
	 * 
	 * @param hands 原始手数
	 * @param min_hands 最小手数
	 * @return int 取整后的手数
	 */
	inline int round_hands(double hands, double min_hands)
	{
		// 四舍五入到最小手数的整数倍
		return (int)((hands + min_hands / 2) / min_hands) * min_hands;
	}

	/**
	 * @brief 获取最小下单数量
	 * 
	 * 根据合约代码判断合约类型，返回对应的最小下单数量。
	 * - 可转债：10张
	 * - 科创板股票（代码>=688000）：200股
	 * - 普通股票：100股
	 * 
	 * @param stdCode 合约代码（标准格式，如"SSE.600000.STK"）
	 * @return double 最小下单数量
	 */
	inline double get_minOrderQty(std::string stdCode)
	{
		// 从标准代码中提取数字代码部分（如"SSE.600000.STK" -> "600000"）
		int code = std::stoi(StrUtil::split(stdCode, ".")[2]);
		bool is_KC = false;
		if (code >= 688000)  // 如果代码>=688000，则是科创板股票
		{
			is_KC = true;
		}
		WTSCommodityInfo* comm_info = _ctx->getCommodityInfo(stdCode.c_str());  // 获取合约信息
		double min_order = 1.0;  // 默认最小下单数量
		if (strcmp(comm_info->getProduct(), cbondStr) == 0)  // 如果是可转债
			min_order = 10.0;  // 可转债最小下单10张
		else if (strcmp(comm_info->getProduct(), stockStr) == 0)  // 如果是股票
			if (is_KC)  // 如果是科创板股票
				min_order = 200.0;  // 科创板股票最小下单200股
			else  // 如果是普通股票
				min_order = 100.0;  // 普通股票最小下单100股
		if (comm_info)  // 如果合约信息指针有效
			comm_info->release();  // 释放合约信息
		return min_order;  // 返回最小下单数量
	}
};

