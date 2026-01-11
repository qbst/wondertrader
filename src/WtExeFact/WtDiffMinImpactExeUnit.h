/*!
 * \file WtDiffMinImpactExeUnit.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtDiffMinImpactExeUnit差量最小冲击执行单元类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了WtDiffMinImpactExeUnit类，这是差量最小冲击执行单元的实现类，继承自ExecuteUnit基类。
 * 差量执行单元用于增量驱动模式（Delta Driven），立即执行指定的数量变化，而不是将仓位调整到目标值。
 * 
 * 设计目标：
 * 1. 实现差量执行模式，立即执行指定的数量变化（买入N手或卖出N手）
 * 2. 实现最小市场冲击算法，通过控制订单价格和数量减少对市场的影响
 * 3. 支持多种价格模式：最优价、最新价、对手价、自动价格
 * 4. 支持订单超时自动撤单机制
 * 5. 支持按比例或固定数量下单
 * 
 * 核心功能：
 * - 差量驱动执行：立即执行指定的数量变化（正数表示买入，负数表示卖出）
 * - 价格控制：根据价格模式和价格偏移计算订单价格
 * - 数量控制：根据配置的单次发单手数或比例下单
 * - 订单管理：跟踪订单状态，支持超时撤单
 * - 市场数据响应：根据Tick数据实时调整订单策略
 * 
 * 差量模式与标准模式的区别：
 * - 标准模式（WtMinImpactExeUnit）：目标驱动，将仓位调整到目标值（目标-当前=差量，然后执行差量）
 * - 差量模式（WtDiffMinImpactExeUnit）：增量驱动，立即执行指令中的数量变化（直接执行N手，N可正可负）
 * 
 * 执行算法：
 * - 接收差量指令（正数表示买入，负数表示卖出）
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
 * @brief WtDiffMinImpactExeUnit差量最小冲击执行单元类
 * 
 * 继承自ExecuteUnit基类，实现差量模式的最小市场冲击算法，用于期货合约的智能订单执行。
 * 该执行单元用于增量驱动模式（Delta Driven），立即执行指定的数量变化，而不是将仓位调整到目标值。
 * 
 * 差量模式的特点：
 * - 直接执行指令中的数量变化（买入N手或卖出N手）
 * - 不关心当前仓位，只执行差量指令
 * - 适用于高频交易、做市、抢单等策略
 */
class WtDiffMinImpactExeUnit : public ExecuteUnit
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建差量最小冲击执行单元实例，初始化所有成员变量。
	 */
	WtDiffMinImpactExeUnit();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
	 */
	virtual ~WtDiffMinImpactExeUnit();

private:
	/**
	 * @brief 执行计算
	 * 
	 * 核心计算函数，根据差量指令计算需要执行的订单数量，
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
	 * @return const char* 返回执行单元名称字符串（"WtDiffMinImpactExeUnit"）
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
	 * 当订单有成交回报时调用此函数，更新未执行差量。
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
	 * @brief 设置新的差量
	 * 
	 * 设置执行单元的差量指令，执行单元会立即执行该差量。
	 * 
	 * @param stdCode 合约代码
	 * @param newVol 新的差量指令（正数表示买入N手，负数表示卖出N手，DBL_MAX表示清仓）
	 * 
	 * 注意：在差量模式下，newVol表示要执行的差量，而不是目标仓位。
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void set_position(const char* stdCode, double newVol) override;

	/**
	 * @brief 清理全部持仓
	 * 
	 * 清理指定合约的全部持仓，将所有订单撤单并清空差量指令。
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
	WTSTickData*	_last_tick;  // 上一笔行情数据指针，用于获取最新价格等信息
	double			_left_diff;  // 未执行差量（剩余需要执行的差量，正数表示买入，负数表示卖出）
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
		_CalcFlag(std::atomic<bool>* flag):_flag(flag)
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
			if(_flag)  // 如果标志指针有效
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

