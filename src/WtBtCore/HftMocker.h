/*!
 * \file HftMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易策略回测模拟器头文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * HftMocker是WonderTrader回测框架中用于模拟高频交易（HFT）策略行为的回测模拟器类。
 *
 * 设计目标：
 * 1. 模拟高频交易策略在历史数据上的执行过程
 * 2. 提供完整的HFT策略执行环境，包括tick数据、订单队列、订单明细、逐笔成交等
 * 3. 支持HFT策略的动态加载和执行
 * 4. 记录完整的执行结果，包括成交记录、平仓记录、持仓记录、资金记录等
 * 5. 支持异步回测模式（通过钩子机制）
 *
 * 核心功能：
 * - 继承IDataSink接口，接收历史数据回放器推送的市场数据（tick、订单队列、订单明细、逐笔成交）
 * - 继承IHftStraCtx接口，为HFT策略提供执行上下文环境
 * - 管理策略持仓和未完成订单
 * - 处理策略发出的买卖订单，模拟订单撮合过程
 * - 记录订单执行日志，包括成交记录、平仓记录、持仓记录、资金记录等
 * - 支持异步回测模式，通过钩子机制实现步进式回测
 *
 * 架构特点：
 * - 采用事件驱动模式，通过回调函数响应市场数据变化
 * - 使用任务队列机制处理异步订单
 * - 采用工厂模式动态加载HFT策略模块
 * - 支持同步和异步两种回测模式
 */
#pragma once
#include <queue>                                                    // 队列容器
#include <sstream>                                                   // 字符串流

#include "HisDataReplayer.h"                                         // 历史数据回放器

#include "../Includes/FasterDefs.h"                                 // 快速定义
#include "../Includes/IHftStraCtx.h"                                // HFT策略上下文接口
#include "../Includes/HftStrategyDefs.h"                            // HFT策略定义

#include "../Share/StdUtils.hpp"                                    // 标准工具函数
#include "../Share/DLLHelper.hpp"                                   // 动态库加载工具
#include "../Share/fmtlib.h"                                        // 格式化工具

class HisDataReplayer;                                               // 前向声明

/**
 * @brief 高频交易策略回测模拟器类
 * 
 * 继承自IDataSink和IHftStraCtx接口，提供HFT策略的回测环境
 */
class HftMocker : public IDataSink, public IHftStraCtx
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param replayer 历史数据回放器指针
	 * @param name 策略名称
	 */
	HftMocker(HisDataReplayer* replayer, const char* name);
	/**
	 * @brief 析构函数
	 */
	virtual ~HftMocker();

private:
	/**
	 * @brief 调试日志模板函数
	 * 
	 * @tparam Args 参数类型
	 * @param format 格式化字符串
	 * @param args 参数列表
	 */
	template<typename... Args>
	void log_debug(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);      // 格式化字符串
		stra_log_debug(buffer);                                      // 记录调试日志
	}

	/**
	 * @brief 信息日志模板函数
	 * 
	 * @tparam Args 参数类型
	 * @param format 格式化字符串
	 * @param args 参数列表
	 */
	template<typename... Args>
	void log_info(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);      // 格式化字符串
		stra_log_info(buffer);                                       // 记录信息日志
	}

	/**
	 * @brief 错误日志模板函数
	 * 
	 * @tparam Args 参数类型
	 * @param format 格式化字符串
	 * @param args 参数列表
	 */
	template<typename... Args>
	void log_error(const char* format, const Args& ...args)
	{
		const char* buffer = fmtutil::format(format, args...);      // 格式化字符串
		stra_log_error(buffer);                                      // 记录错误日志
	}

public:
	//////////////////////////////////////////////////////////////////////////
	//IDataSink接口实现
	/**
	 * @brief 处理tick数据
	 * 
	 * @param stdCode 合约代码
	 * @param curTick 当前tick数据
	 * @param pxType 价格类型
	 */
	virtual void	handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType) override;
	/**
	 * @brief 处理订单队列数据
	 * 
	 * @param stdCode 合约代码
	 * @param curOrdQue 当前订单队列数据
	 */
	virtual void	handle_order_queue(const char* stdCode, WTSOrdQueData* curOrdQue) override;
	/**
	 * @brief 处理订单明细数据
	 * 
	 * @param stdCode 合约代码
	 * @param curOrdDtl 当前订单明细数据
	 */
	virtual void	handle_order_detail(const char* stdCode, WTSOrdDtlData* curOrdDtl) override;
	/**
	 * @brief 处理逐笔成交数据
	 * 
	 * @param stdCode 合约代码
	 * @param curTrans 当前逐笔成交数据
	 */
	virtual void	handle_transaction(const char* stdCode, WTSTransData* curTrans) override;

	/**
	 * @brief 处理K线收盘事件
	 * 
	 * @param stdCode 合约代码
	 * @param period 周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 */
	virtual void	handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;
	/**
	 * @brief 处理调度事件
	 * 
	 * @param uDate 日期
	 * @param uTime 时间
	 */
	virtual void	handle_schedule(uint32_t uDate, uint32_t uTime) override;

	/**
	 * @brief 处理初始化事件
	 */
	virtual void	handle_init() override;
	/**
	 * @brief 处理交易时段开始事件
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void	handle_session_begin(uint32_t curTDate) override;
	/**
	 * @brief 处理交易时段结束事件
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void	handle_session_end(uint32_t curTDate) override;

	/**
	 * @brief 处理回放完成事件
	 */
	virtual void	handle_replay_done() override;

	/**
	 * @brief tick数据更新回调
	 * 
	 * @param stdCode 合约代码
	 * @param newTick 新的tick数据
	 */
	virtual void	on_tick_updated(const char* stdCode, WTSTickData* newTick) override;
	/**
	 * @brief 订单队列更新回调
	 * 
	 * @param stdCode 合约代码
	 * @param newOrdQue 新的订单队列数据
	 */
	virtual void	on_ordque_updated(const char* stdCode, WTSOrdQueData* newOrdQue) override;
	/**
	 * @brief 订单明细更新回调
	 * 
	 * @param stdCode 合约代码
	 * @param newOrdDtl 新的订单明细数据
	 */
	virtual void	on_orddtl_updated(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;
	/**
	 * @brief 逐笔成交更新回调
	 * 
	 * @param stdCode 合约代码
	 * @param newTrans 新的逐笔成交数据
	 */
	virtual void	on_trans_updated(const char* stdCode, WTSTransData* newTrans) override;

	//////////////////////////////////////////////////////////////////////////
	//IHftStraCtx接口实现
	/**
	 * @brief 策略tick数据回调
	 * 
	 * @param stdCode 合约代码
	 * @param newTick 新的tick数据
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;

	/**
	 * @brief 策略订单队列回调
	 * 
	 * @param stdCode 合约代码
	 * @param newOrdQue 新的订单队列数据
	 */
	virtual void on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue) override;

	/**
	 * @brief 策略订单明细回调
	 * 
	 * @param stdCode 合约代码
	 * @param newOrdDtl 新的订单明细数据
	 */
	virtual void on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl) override;

	/**
	 * @brief 策略逐笔成交回调
	 * 
	 * @param stdCode 合约代码
	 * @param newTrans 新的逐笔成交数据
	 */
	virtual void on_transaction(const char* stdCode, WTSTransData* newTrans) override;

	/**
	 * @brief 获取策略上下文ID
	 * 
	 * @return 上下文ID
	 */
	virtual uint32_t id() override;

	/**
	 * @brief 策略初始化回调
	 */
	virtual void on_init() override;

	/**
	 * @brief 策略K线回调
	 * 
	 * @param stdCode 合约代码
	 * @param period 周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 */
	virtual void on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 策略交易时段开始回调
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void on_session_begin(uint32_t curTDate) override;

	/**
	 * @brief 策略交易时段结束回调
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void on_session_end(uint32_t curTDate) override;

	/**
	 * @brief 撤单（按订单ID）
	 * 
	 * @param localid 本地订单ID
	 * @return 是否成功
	 */
	virtual bool stra_cancel(uint32_t localid) override;

	/**
	 * @brief 撤单（按合约和方向）
	 * 
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param qty 数量（0表示全部）
	 * @return 订单ID列表
	 */
	virtual OrderIDs stra_cancel(const char* stdCode, bool isBuy, double qty = 0) override;

	/**
	 * @brief 买入订单
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param userTag 用户标签
	 * @param flag 标志
	 * @param bForceClose 是否强制平仓
	 * @return 订单ID列表
	 */
	virtual OrderIDs stra_buy(const char* stdCode, double price, double qty, const char* userTag, int flag = 0, bool bForceClose = false) override;

	/**
	 * @brief 卖出订单
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param userTag 用户标签
	 * @param flag 标志
	 * @param bForceClose 是否强制平仓
	 * @return 订单ID列表
	 */
	virtual OrderIDs stra_sell(const char* stdCode, double price, double qty, const char* userTag, int flag = 0, bool bForceClose = false) override;

	/**
	 * @brief 获取合约信息
	 * 
	 * @param stdCode 合约代码
	 * @return 合约信息指针
	 */
	virtual WTSCommodityInfo* stra_get_comminfo(const char* stdCode) override;

	/**
	 * @brief 获取K线数据
	 * 
	 * @param stdCode 合约代码
	 * @param period 周期
	 * @param count 数量
	 * @return K线数据切片指针
	 */
	virtual WTSKlineSlice* stra_get_bars(const char* stdCode, const char* period, uint32_t count) override;

	/**
	 * @brief 获取tick数据
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @return tick数据切片指针
	 */
	virtual WTSTickSlice* stra_get_ticks(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取订单明细数据
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @return 订单明细数据切片指针
	 */
	virtual WTSOrdDtlSlice*	stra_get_order_detail(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取订单队列数据
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @return 订单队列数据切片指针
	 */
	virtual WTSOrdQueSlice*	stra_get_order_queue(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取逐笔成交数据
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @return 逐笔成交数据切片指针
	 */
	virtual WTSTransSlice*	stra_get_transaction(const char* stdCode, uint32_t count) override;

	/**
	 * @brief 获取最新tick数据
	 * 
	 * @param stdCode 合约代码
	 * @return 最新tick数据指针
	 */
	virtual WTSTickData* stra_get_last_tick(const char* stdCode) override;

	/**
	 * @brief 获取分月合约代码
	 * 
	 * @param stdCode 标准合约代码
	 * @return 分月合约代码
	 */
	virtual std::string		stra_get_rawcode(const char* stdCode) override;

	/**
	 * @brief 获取持仓数量
	 * 
	 * @param stdCode 合约代码
	 * @param bOnlyValid 是否只查询有效持仓
	 * @param flag 标志
	 * @return 持仓数量
	 */
	virtual double stra_get_position(const char* stdCode, bool bOnlyValid = false, int flag = 3) override;

	/**
	 * @brief 获取持仓均价
	 * 
	 * @param stdCode 合约代码
	 * @return 持仓均价
	 */
	virtual double stra_get_position_avgpx(const char* stdCode) override;

	/**
	 * @brief 获取持仓盈亏
	 * 
	 * @param stdCode 合约代码
	 * @return 持仓盈亏
	 */
	virtual double stra_get_position_profit(const char* stdCode) override;

	/**
	 * @brief 获取未完成订单数量
	 * 
	 * @param stdCode 合约代码
	 * @return 未完成订单数量
	 */
	virtual double stra_get_undone(const char* stdCode) override;

	/**
	 * @brief 获取最新价格
	 * 
	 * @param stdCode 合约代码
	 * @return 最新价格
	 */
	virtual double stra_get_price(const char* stdCode) override;

	/**
	 * @brief 获取当前日期
	 * 
	 * @return 当前日期
	 */
	virtual uint32_t stra_get_date() override;

	/**
	 * @brief 获取当前时间
	 * 
	 * @return 当前时间
	 */
	virtual uint32_t stra_get_time() override;

	/**
	 * @brief 获取当前秒数
	 * 
	 * @return 当前秒数
	 */
	virtual uint32_t stra_get_secs() override;

	/**
	 * @brief 订阅tick数据
	 * 
	 * @param stdCode 合约代码
	 */
	virtual void stra_sub_ticks(const char* stdCode) override;

	/**
	 * @brief 订阅订单队列数据
	 * 
	 * @param stdCode 合约代码
	 */
	virtual void stra_sub_order_queues(const char* stdCode) override;

	/**
	 * @brief 订阅订单明细数据
	 * 
	 * @param stdCode 合约代码
	 */
	virtual void stra_sub_order_details(const char* stdCode) override;

	/**
	 * @brief 订阅逐笔成交数据
	 * 
	 * @param stdCode 合约代码
	 */
	virtual void stra_sub_transactions(const char* stdCode) override;

	/**
	 * @brief 记录信息日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_info(const char* message) override;
	/**
	 * @brief 记录调试日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_debug(const char* message) override;
	/**
	 * @brief 记录警告日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_warn(const char* message) override;
	/**
	 * @brief 记录错误日志
	 * 
	 * @param message 日志消息
	 */
	virtual void stra_log_error(const char* message) override;

	/**
	 * @brief 保存用户数据
	 * 
	 * @param key 键
	 * @param val 值
	 */
	virtual void stra_save_user_data(const char* key, const char* val) override;

	/**
	 * @brief 加载用户数据
	 * 
	 * @param key 键
	 * @param defVal 默认值
	 * @return 用户数据值
	 */
	virtual const char* stra_load_user_data(const char* key, const char* defVal = "") override;

	//////////////////////////////////////////////////////////////////////////
	//策略回调接口
	/**
	 * @brief 成交回报回调
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param vol 成交数量
	 * @param price 成交价格
	 * @param userTag 用户标签
	 */
	virtual void on_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag);

	/**
	 * @brief 订单回报回调
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param totalQty 总数量
	 * @param leftQty 剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤单
	 * @param userTag 用户标签
	 */
	virtual void on_order(uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag);

	/**
	 * @brief 通道就绪回调
	 */
	virtual void on_channel_ready();

	/**
	 * @brief 委托回报回调
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息
	 * @param userTag 用户标签
	 */
	virtual void on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag);

public:
	/**
	 * @brief 初始化HFT策略工厂
	 * 
	 * @param cfg 配置信息
	 * @return 是否成功
	 */
	bool	init_hft_factory(WTSVariant* cfg);
	/**
	 * @brief 安装钩子（用于异步回测）
	 */
	void	install_hook();
	/**
	 * @brief 启用/禁用钩子
	 * 
	 * @param bEnabled 是否启用
	 */
	void	enable_hook(bool bEnabled = true);
	/**
	 * @brief 步进tick（用于异步回测）
	 */
	void	step_tick();

private:
	/**
	 * @brief 任务类型定义
	 */
	typedef std::function<void()> Task;
	/**
	 * @brief 提交任务到任务队列
	 * 
	 * @param task 任务函数
	 */
	void	postTask(Task task);
	/**
	 * @brief 处理任务队列
	 */
	void	procTask();

	/**
	 * @brief 处理订单
	 * 
	 * @param localid 本地订单ID
	 * @return 是否成功
	 */
	bool	procOrder(uint32_t localid);

	/**
	 * @brief 设置持仓
	 * 
	 * @param stdCode 合约代码
	 * @param qty 数量
	 * @param price 价格
	 * @param userTag 用户标签
	 */
	void	do_set_position(const char* stdCode, double qty, double price = 0.0, const char* userTag = "");
	/**
	 * @brief 更新动态盈亏
	 * 
	 * @param stdCode 合约代码
	 * @param newTick 新的tick数据
	 */
	void	update_dyn_profit(const char* stdCode, WTSTickData* newTick);

	/**
	 * @brief 输出结果到文件
	 */
	void	dump_outputs();
	/**
	 * @brief 记录成交日志
	 * 
	 * @param stdCode 合约代码
	 * @param isLong 是否多头
	 * @param isOpen 是否开仓
	 * @param curTime 当前时间
	 * @param price 价格
	 * @param qty 数量
	 * @param fee 手续费
	 * @param userTag 用户标签
	 */
	inline void	log_trade(const char* stdCode, bool isLong, bool isOpen, uint64_t curTime, double price, double qty, double fee, const char* userTag);
	/**
	 * @brief 记录平仓日志
	 * 
	 * @param stdCode 合约代码
	 * @param isLong 是否多头
	 * @param openTime 开仓时间
	 * @param openpx 开仓价格
	 * @param closeTime 平仓时间
	 * @param closepx 平仓价格
	 * @param qty 数量
	 * @param profit 盈亏
	 * @param maxprofit 最大盈利
	 * @param maxloss 最大亏损
	 * @param totalprofit 总盈亏
	 * @param enterTag 开仓标签
	 * @param exitTag 平仓标签
	 */
	inline void	log_close(const char* stdCode, bool isLong, uint64_t openTime, double openpx, uint64_t closeTime, double closepx, double qty,
		double profit, double maxprofit, double maxloss, double totalprofit, const char* enterTag, const char* exitTag);

private:
	HisDataReplayer*	_replayer;                                    // 历史数据回放器指针

	bool			_use_newpx;                                     // 是否使用新价格
	uint32_t		_error_rate;                                    // 错误率（用于模拟订单失败）
	bool			_match_this_tick;	//是否在当前tick撮合               // 是否在当前tick撮合

	/**
	 * @brief 价格映射表类型
	 */
	typedef wt_hashmap<std::string, double> PriceMap;
	PriceMap		_price_map;                                     // 价格映射表（合约代码->最新价格）


	/**
	 * @brief 策略工厂信息结构体
	 */
	typedef struct _StraFactInfo
	{
		std::string		_module_path;                                 // 模块路径
		DllHandle		_module_inst;                                 // 动态库句柄
		IHftStrategyFact*	_fact;                                     // 策略工厂指针
		FuncCreateHftStraFact	_creator;                             // 创建工厂函数指针
		FuncDeleteHftStraFact	_remover;                              // 删除工厂函数指针

		/**
		 * @brief 构造函数
		 */
		_StraFactInfo()
		{
			_module_inst = NULL;                                    // 初始化动态库句柄为NULL
			_fact = NULL;                                           // 初始化策略工厂指针为NULL
		}

		/**
		 * @brief 析构函数
		 */
		~_StraFactInfo()
		{
			if (_fact)                                             // 如果策略工厂存在
				_remover(_fact);                                    // 删除策略工厂
		}
	} StraFactInfo;
	StraFactInfo	_factory;                                        // 策略工厂信息

	HftStrategy*	_strategy;                                       // HFT策略实例指针

	StdUniqueMutex		_mtx;                                         // 互斥锁（用于任务队列）
	std::queue<Task>	_tasks;                                       // 任务队列

	StdRecurMutex		_mtx_control;                                 // 递归互斥锁（用于控制）

	/**
	 * @brief 订单信息结构体
	 */
	typedef struct _OrderInfo
	{
		bool	_isBuy;                                            // 是否买入
		char	_code[32];                                         // 合约代码
		double	_price;                                            // 订单价格
		double	_total;                                            // 总数量
		double	_left;                                             // 剩余数量
		char	_usertag[32];                                       // 用户标签
		
		uint32_t	_localid;                                       // 本地订单ID

		bool	_proced_after_placed;	//下单后是否处理过			     // 下单后是否处理过

		/**
		 * @brief 构造函数
		 */
		_OrderInfo()
		{
			memset(this, 0, sizeof(_OrderInfo));                  // 清零初始化
		}

		/**
		 * @brief 拷贝构造函数
		 * 
		 * @param rhs 右值引用
		 */
		_OrderInfo(const struct _OrderInfo& rhs)
		{
			memcpy(this, &rhs, sizeof(_OrderInfo));               // 内存拷贝
		}

		/**
		 * @brief 赋值运算符
		 * 
		 * @param rhs 右值引用
		 * @return 自身引用
		 */
		_OrderInfo& operator =(const struct _OrderInfo& rhs)
		{
			memcpy(this, &rhs, sizeof(_OrderInfo));               // 内存拷贝
			return *this;                                          // 返回自身引用
		}

	} OrderInfo;
	typedef std::shared_ptr<OrderInfo> OrderInfoPtr;                 // 订单信息智能指针类型
	typedef wt_hashmap<uint32_t, OrderInfoPtr> Orders;                // 订单映射表类型（订单ID->订单信息）
	StdRecurMutex	_mtx_ords;                                       // 订单互斥锁
	Orders			_orders;                                         // 订单映射表

	typedef WTSHashMap<std::string> CommodityMap;                     // 合约映射表类型
	CommodityMap*	_commodities;                                     // 合约映射表指针

	//用户数据
	/**
	 * @brief 字符串哈希映射表类型
	 */
	typedef wt_hashmap<std::string, std::string> StringHashMap;
	StringHashMap	_user_datas;                                     // 用户数据映射表
	bool			_ud_modified;                                     // 用户数据是否已修改

	/**
	 * @brief 持仓明细信息结构体
	 */
	typedef struct _DetailInfo
	{
		bool		_long;                                           // 是否多头
		double		_price;                                          // 开仓价格
		double		_volume;                                         // 持仓数量
		uint64_t	_opentime;                                       // 开仓时间
		uint32_t	_opentdate;                                      // 开仓日期
		double		_max_profit;                                     // 最大盈利
		double		_max_loss;                                       // 最大亏损
		double		_profit;                                         // 当前盈亏
		char		_usertag[32];                                     // 用户标签

		/**
		 * @brief 构造函数
		 */
		_DetailInfo()
		{
			memset(this, 0, sizeof(_DetailInfo));                 // 清零初始化
		}
	} DetailInfo;

	/**
	 * @brief 持仓信息结构体
	 */
	typedef struct _PosInfo
	{
		double		_volume;                                         // 持仓数量
		double		_closeprofit;                                    // 平仓盈亏
		double		_dynprofit;                                      // 浮动盈亏
		double		_frozen;                                         // 冻结数量

		std::vector<DetailInfo> _details;                            // 持仓明细列表

		/**
		 * @brief 构造函数
		 */
		_PosInfo()
		{
			_volume = 0;                                           // 初始化持仓数量为0
			_closeprofit = 0;                                      // 初始化平仓盈亏为0
			_dynprofit = 0;                                        // 初始化浮动盈亏为0
			_frozen = 0;                                           // 初始化冻结数量为0
		}

		/**
		 * @brief 获取有效持仓数量
		 * 
		 * @return 有效持仓数量（持仓数量-冻结数量）
		 */
		inline double valid() const { return _volume - _frozen; }
	} PosInfo;
	typedef wt_hashmap<std::string, PosInfo> PositionMap;             // 持仓映射表类型（合约代码->持仓信息）
	PositionMap		_pos_map;                                        // 持仓映射表

	std::stringstream	_trade_logs;                                 // 成交日志流
	std::stringstream	_close_logs;                                 // 平仓日志流
	std::stringstream	_fund_logs;                                  // 资金日志流
	std::stringstream	_sig_logs;                                   // 信号日志流
	std::stringstream	_pos_logs;                                   // 持仓日志流

	/**
	 * @brief 策略资金信息结构体
	 */
	typedef struct _StraFundInfo
	{
		double	_total_profit;                                     // 总盈亏
		double	_total_dynprofit;                                  // 总浮动盈亏
		double	_total_fees;                                       // 总手续费

		/**
		 * @brief 构造函数
		 */
		_StraFundInfo()
		{
			memset(this, 0, sizeof(_StraFundInfo));               // 清零初始化
		}
	} StraFundInfo;

	StraFundInfo		_fund_info;                                   // 策略资金信息

protected:
	uint32_t		_context_id;                                     // 上下文ID

	StdUniqueMutex	_mtx_calc;                                       // 计算互斥锁
	StdCondVariable	_cond_calc;                                      // 计算条件变量
	bool			_has_hook;		//这是人为控制是否启用钩子          // 是否启用钩子（人为控制）
	bool			_hook_valid;	//这是根据是否是异步回测模式而确定钩子是否可用  // 钩子是否有效（根据异步回测模式确定）
	std::atomic<bool>	_resumed;                                    // 临时变量，用于控制状态（是否已恢复）

	//tick订阅列表
	wt_hashset<std::string> _tick_subs;                              // tick订阅列表

	typedef WTSHashMap<std::string>	TickCache;                     // tick缓存类型
	TickCache*	_ticks;                                             // tick缓存指针
};

