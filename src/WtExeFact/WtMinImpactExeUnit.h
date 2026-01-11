/*!
 * \file WtMinImpactExeUnit.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtMinImpactExeUnit最小冲击执行单元类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了WtMinImpactExeUnit类，这是最小冲击执行单元的实现类，继承自ExecuteUnit基类。
 * 最小冲击执行单元用于期货合约的智能订单执行，通过控制订单价格和数量，最小化对市场的冲击。
 * 
 * 设计目标：
 * 1. 实现最小市场冲击算法，通过控制订单价格和数量减少对市场的影响
 * 2. 支持多种价格模式：最优价、最新价、对手价、自动价格
 * 3. 支持订单超时自动撤单机制
 * 4. 支持按比例或固定数量下单
 * 5. 支持最小开仓数量限制
 * 
 * 核心功能：
 * - 目标驱动执行：将仓位调整到目标值，自动计算需要执行的差量
 * - 价格控制：根据价格模式和价格偏移计算订单价格
 * - 数量控制：根据配置的单次发单手数或比例下单
 * - 订单管理：跟踪订单状态，支持超时撤单
 * - 市场数据响应：根据Tick数据实时调整订单策略
 * 
 * 执行算法：
 * - 计算目标仓位与当前仓位的差值
 * - 根据价格模式（最优价/最新价/对手价）确定订单价格
 * - 根据价格偏移调整订单价格（买入+偏移，卖出-偏移）
 * - 根据配置的单次发单手数或比例下单
 * - 监控订单超时，自动撤单并重新下单
 * 
 * 架构特点：
 * - 使用互斥锁保护计算逻辑，确保线程安全
 * - 使用原子标志防止并发计算
 * - 使用订单管理器跟踪订单生命周期
 * - 支持多种价格模式和下单策略
 */
#pragma once
#include "../Includes/ExecuteDefs.h"  // 执行单元定义文件（包含ExecuteUnit基类）
#include "WtOrdMon.h"  // 订单管理器类定义

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WtMinImpactExeUnit最小冲击执行单元类
 * 
 * 继承自ExecuteUnit基类，实现最小市场冲击算法，用于期货合约的智能订单执行。
 * 该执行单元通过控制订单价格和数量，最小化对市场的冲击，实现最优的执行效果。
 * 
 * 最小冲击算法的核心思想：
 * - 使用最优价或最新价下单，避免使用对手价造成市场冲击
 * - 控制单次下单数量，避免大单对市场造成冲击
 * - 根据对手盘挂单量按比例下单，避免一次性吃掉所有挂单
 * - 订单超时自动撤单并重新下单，保持订单活跃
 */
class WtMinImpactExeUnit : public ExecuteUnit
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建最小冲击执行单元实例，初始化所有成员变量。
	 */
	WtMinImpactExeUnit();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
	 */
	virtual ~WtMinImpactExeUnit();

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
	 * @return const char* 返回执行单元名称字符串（"WtMinImpactExeUnit"）
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
	 * 
	 * @param stdCode 合约代码
	 * @param newVol 新的目标仓位（正数表示多头，负数表示空头，DBL_MAX表示清仓）
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

private:
	WTSTickData* _last_tick;  // 上一笔行情数据指针，用于获取最新价格等信息
	double		_target_pos;  // 目标仓位（正数表示多头，负数表示空头，DBL_MAX表示清仓）
	StdUniqueMutex	_mtx_calc;  // 计算逻辑的互斥锁，保证线程安全

	WTSCommodityInfo*	_comm_info;  // 合约信息指针，包含合约的基本信息（如最小变动价位、合约乘数等）
	WTSSessionInfo*		_sess_info;  // 交易时段信息指针，包含交易时间、休市时间等信息

	//////////////////////////////////////////////////////////////////////////
	//执行参数
	int32_t		_price_offset;  // 价格偏移跳数（相对于基准价格的偏移，买入+偏移，卖出-偏移）
	uint32_t	_expire_secs;  // 订单超时秒数（订单创建后超过此时间未成交则自动撤单）
	int32_t		_price_mode;  // 价格模式：0-最新价，-1-最优价，1-对手价，2-自动价格
	uint32_t	_entrust_span;  // 发单时间间隔（单位：毫秒，两次下单之间的最小时间间隔）
	bool		_by_rate;  // 是否按照对手盘挂单数的比例下单，true表示按比例（rate字段生效），false表示固定数量（lots字段生效）
	double		_order_lots;  // 单次发单手数（当by_rate为false时使用）
	double		_qty_rate;  // 下单手数比例（当by_rate为true时使用，如0.1表示每次下对手盘挂单量的10%）

	/**
	 * @brief 最小开仓数量
	 * 
	 * 增加一个最小开仓数量限制。
	 * 为什么没有最小平仓数量呢，因为平仓要根据持仓来，所以无法限制。
	 * 
	 * By Wesley @ 2022.12.15
	 */
	double		_min_open_lots;  // 最小开仓数量（开仓时，如果差量小于此值则不执行）

	WtOrdMon	_orders_mon;  // 订单管理器，用于跟踪和管理订单状态
	uint32_t	_cancel_cnt;  // 在途撤单量（正在撤单的订单数量）
	uint32_t	_cancel_times;  // 撤单次数（累计撤单次数统计）

	uint64_t	_last_place_time;  // 上个下单时间（毫秒时间戳，用于控制发单间隔）
	uint64_t	_last_tick_time;  // 上个tick时间（毫秒时间戳，用于判断是否有新行情）

	/**
	 * @brief 计算中标志
	 * 
	 * 原子布尔标志，用于防止并发计算。
	 * 当有计算正在进行时，其他线程不能同时进行计算。
	 */
	std::atomic<bool>	_in_calc;

	/**
	 * @brief 计算标志辅助类
	 * 
	 * RAII辅助类，用于自动管理计算标志的生命周期。
	 * 构造时设置标志为true，析构时恢复为false。
	 * 如果构造时标志已经是true，说明有并发计算，operator bool()返回true。
	 */
	typedef struct _CalcFlag
	{
		bool				_result;  // 构造时标志的原始值（true表示有并发计算）
		std::atomic<bool>*	_flag;  // 指向计算标志的指针
		
		/**
		 * @brief 构造函数
		 * 
		 * 设置计算标志为true，并保存原始值。
		 * 
		 * @param flag 指向计算标志的指针
		 */
		_CalcFlag(std::atomic<bool>* flag) :_flag(flag)
		{
			// 使用原子操作设置标志为true，并返回原始值
			_result = _flag->exchange(true, std::memory_order_acq_rel);
		}

		/**
		 * @brief 析构函数
		 * 
		 * 恢复计算标志为false。
		 */
		~_CalcFlag()
		{
			if (_flag)  // 如果标志指针有效
				_flag->exchange(false, std::memory_order_acq_rel);  // 恢复标志为false
		}

		/**
		 * @brief 布尔转换运算符
		 * 
		 * 返回构造时标志的原始值，true表示有并发计算。
		 * 
		 * @return bool true表示有并发计算，false表示没有并发计算
		 */
		operator bool() const { return _result; }
	} CalcFlag;
};

