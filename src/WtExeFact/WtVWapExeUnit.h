/*!
 * \file WtVWapExeUnit.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WtVWapExeUnit成交量加权平均价格执行单元类定义文件
 * 
 * 文件设计逻辑与作用：
 * ====================
 * 本文件定义了WtVWapExeUnit类，这是成交量加权平均价格（VWAP，Volume-Weighted Average Price）执行单元的实现类，
 * 继承自ExecuteUnit基类。VWAP执行单元根据历史成交量分布，将订单按比例分配到各个时间段执行，
 * 以实现接近成交量加权平均价格的效果。
 * 
 * 设计目标：
 * 1. 实现VWAP算法，根据历史成交量分布将订单按比例分配到各个时间段执行
 * 2. 支持设置执行时间段（开始时间和结束时间）
 * 3. 支持设置总执行次数，将订单分成多批执行
 * 4. 支持尾部时间处理，在最后一段时间内集中执行剩余订单
 * 5. 支持订单超时自动撤单机制
 * 
 * 核心功能：
 * - 成交量分布预测：根据历史数据预测各时间段的成交量分布
 * - 按比例分配：根据成交量分布将订单按比例分配到各个时间段
 * - 分批执行：根据总执行次数将订单分成多批执行
 * - 尾部处理：在最后一段时间内集中执行剩余订单，确保完成执行
 * - 价格控制：根据价格模式和价格偏移计算订单价格
 * - 订单管理：跟踪订单状态，支持超时撤单
 * 
 * VWAP算法原理：
 * - 根据历史数据预测各时间段的成交量分布（VwapAim数组）
 * - 将执行时间段分成N个时间间隔（N=总执行次数）
 * - 在每个时间间隔开始时，根据成交量分布计算当前应该执行的累计数量
 * - 根据累计数量与已执行数量的差值，决定本次下单数量
 * - 在尾部时间内，集中执行剩余订单，确保完成执行
 * 
 * 架构特点：
 * - 使用互斥锁保护计算逻辑，确保线程安全
 * - 使用原子标志防止并发计算
 * - 使用订单管理器跟踪订单生命周期
 * - 支持时间段控制和尾部时间处理
 * - 支持成交量分布预测和按比例分配
 * 
 * By zhaoyv @ 2023.5.23
 */
#pragma once
#include "WtOrdMon.h"  // 订单管理器类定义
#include "../Includes/ExecuteDefs.h"  // 执行单元定义文件（包含ExecuteUnit基类）
#include "../Share/StdUtils.hpp"  // WonderTrader标准工具类（互斥锁等）
#include <fstream>  // 文件流，用于读取配置文件
#include "rapidjson/document.h"  // JSON解析库，用于解析VWAP配置文件
USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief WtVWapExeUnit成交量加权平均价格执行单元类
 * 
 * 继承自ExecuteUnit基类，实现VWAP（Volume-Weighted Average Price）算法，
 * 根据历史成交量分布，将订单按比例分配到各个时间段执行，以实现接近成交量加权平均价格的效果。
 * 
 * VWAP算法的优势：
 * - 减少市场冲击：通过分散执行，避免大单对市场造成冲击
 * - 接近VWAP价格：执行价格接近时间段内的成交量加权平均价格
 * - 适应市场节奏：根据历史成交量分布执行，更符合市场交易节奏
 * - 可预测性：执行计划明确，便于监控和管理
 */
class WtVWapExeUnit : public ExecuteUnit {

public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建VWAP执行单元实例，初始化所有成员变量。
	 */
	WtVWapExeUnit();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放占用的内存（如行情数据、合约信息等）。
	 */
	virtual ~WtVWapExeUnit();

private:
	/**
	 * @brief 执行计算
	 * 
	 * 核心计算函数，根据VWAP算法计算当前应该执行的订单数量，
	 * 并根据市场行情和配置参数决定是否下单、撤单等操作。
	 * 
	 * 该函数是私有函数，由on_tick()等回调函数调用。
	 */
	void	do_calc();
	
	/**
	 * @brief 立即执行指定数量
	 * 
	 * 立即下单执行指定数量的订单，用于尾部时间集中执行剩余订单。
	 * 
	 * @param qty 要执行的订单数量（正数表示买入，负数表示卖出）
	 */
	void	fire_at_once(double qty);

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
	 * @return const char* 返回执行单元名称字符串（"WtVWapExeUnit"）
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual const char* getName() override;

	/**
	 * @brief 初始化执行单元
	 * 
	 * 初始化执行单元，加载配置参数，获取合约信息和交易时段信息，
	 * 并加载VWAP成交量分布预测数据。
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
	 * 设置执行单元的目标仓位，执行单元会根据VWAP算法在指定时间段内按成交量分布执行。
	 * 
	 * @param stdCode 合约代码
	 * @param newVol 新的目标仓位（正数表示多头，负数表示空头，DBL_MAX表示清仓）
	 * 
	 * 重写自ExecuteUnit基类
	 */
	virtual void set_position(const char* stdCode, double newVol) override;

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
	bool		_channel_ready;  // 交易通道是否就绪标志，true表示通道就绪可以下单，false表示通道未就绪
	StdUniqueMutex	_mtx_calc;  // 计算逻辑的互斥锁，保证线程安全

	WTSCommodityInfo* _comm_info;  // 合约信息指针，包含合约的基本信息（如最小变动价位、合约乘数等）
	WTSSessionInfo*	_sess_info;  // 交易时段信息指针，包含交易时间、休市时间等信息
	uint32_t	_cancel_times;  // 撤单次数（累计撤单次数统计）

	//////////////////////////////////////////////////////////////////////////
	//执行参数
	WtOrdMon		_orders_mon;  // 订单管理器，用于跟踪和管理订单状态
	uint32_t		_cancel_cnt;  // 在途撤单量（正在撤单的订单数量）
	vector<double>	VwapAim;  // 分钟记，目标VWAP预测总报单量（数组，每个元素表示一个时间段的预测成交量比例）
	//////////////////////////////////////////////////////////////////////////
	//参数
	uint32_t		_total_secs;  // 执行总时间（单位：秒），从开始时间到结束时间的总时长
	uint32_t		_total_times;  // 总执行次数，将订单分成多少批执行
	uint32_t		_tail_secs;  // 执行尾部时间（单位：秒），在最后一段时间内集中执行剩余订单
	uint32_t		_ord_sticky;  // 挂单时限（单位：秒），订单挂单后超过此时间未成交则自动撤单
	uint32_t		_price_mode;  // 价格模式：0-最新价，1-最优价，2-对手价
	uint32_t		_price_offset;  // 挂单价格偏移（相对于基准价格的偏移，买入+偏移，卖出-偏移）
	uint32_t        _begin_time;  // 开始时间（格式：HHMM，如1000表示10:00）
	uint32_t		_end_time;  // 结束时间（格式：HHMM，如1030表示10:30）
	double			_min_open_lots;  // 最小开仓数量（开仓时，如果差量小于此值则不执行）
	double			_order_lots;  // 单次发单手数（每次下单的数量）
	bool			isCanCancel;  // 是否可撤单标志，true表示可撤单，false表示不可撤单（如涨跌停价的挂单）
	//////////////////////////////////////////////////////////////////////////
	//临时变量
	double			_this_target;  // 本轮目标仓位（当前执行周期的目标仓位）
	uint32_t		_fire_span;  // 发单间隔（单位：毫秒），两次下单之间的时间间隔（总时间-尾部时间）/总执行次数
	uint32_t		_fired_times;  // 已执行次数（当前已执行的批次数量）
	uint64_t		_last_fire_time;  // 上次已执行的时间（毫秒时间戳，用于判断是否到了下次执行时间）
	uint64_t		_last_place_time;  // 上个下单时间（毫秒时间戳，用于控制发单间隔）
	uint64_t		_last_tick_time;  // 上个tick时间（毫秒时间戳，用于判断是否有新行情）
	double			_Vwap_vol;  // VWAP单位时间下单量（根据成交量分布计算的当前时间段应该下单的数量）
	double			_Vwap_prz;  // VWAP价格（根据VWAP算法计算的目标价格）

	std::atomic<bool> _in_calc;  // 计算中标志（原子布尔标志，用于防止并发计算）

	/**
	 * @brief 计算标志辅助类
	 * 
	 * RAII辅助类，用于自动管理计算标志的生命周期。
	 * 构造时设置标志为true，析构时恢复为false。
	 * 如果构造时标志已经是true，说明有并发计算，operator bool()返回true。
	 */
	typedef struct _CalcFlag
	{
		bool _result;  // 构造时标志的原始值（true表示有并发计算）
		std::atomic<bool>* _flag;  // 指向计算标志的指针
		
		/**
		 * @brief 构造函数
		 * 
		 * 设置计算标志为true，并保存原始值。
		 * 
		 * @param flag 指向计算标志的指针
		 */
		_CalcFlag(std::atomic<bool>*flag) :_flag(flag) {
			// 使用原子操作设置标志为true，并返回原始值
			_result = _flag->exchange(true, std::memory_order_acq_rel);
		}

		/**
		 * @brief 析构函数
		 * 
		 * 恢复计算标志为false。
		 */
		~_CalcFlag() {
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
	}CalcFlag;
};