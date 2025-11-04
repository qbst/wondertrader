/*!
 * \file ExecMocker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 执行器模拟器头文件
 *
 * 文件设计逻辑与作用：
 * ====================
 * ExecMocker是WonderTrader回测框架中用于模拟执行器（Executer）行为的回测模拟器类。
 *
 * 设计目标：
 * 1. 模拟执行器在历史数据上的订单执行过程
 * 2. 提供完整的执行器执行环境，包括订单管理、成交撮合、持仓跟踪等
 * 3. 支持执行器策略的动态加载和执行
 * 4. 记录完整的执行结果，包括订单记录、成交记录等
 * 5. 支持多种数量模式：反复正负、一直买、一直卖等
 *
 * 核心功能：
 * - 继承ExecuteContext接口，为执行器提供执行上下文环境
 * - 继承IDataSink接口，接收历史数据回放器推送的市场数据
 * - 继承IMatchSink接口，接收撮合引擎的成交、订单、委托回报
 * - 管理执行器持仓和未完成订单
 * - 处理执行器发出的买卖订单，通过撮合引擎进行撮合
 * - 记录订单执行日志，包括信号时间、下单时间、成交时间等
 *
 * 架构特点：
 * - 采用事件驱动模式，通过回调函数响应市场数据变化
 * - 使用撮合引擎模拟订单成交过程
 * - 采用工厂模式动态加载执行器模块
 * - 支持同步和异步两种回测模式
 */
#pragma once
#include <sstream>                                                   // 字符串流，用于日志记录
#include "HisDataReplayer.h"                                         // 历史数据回放器

#include "../Includes/ExecuteDefs.h"                                 // 执行器定义
#include "../Share/StdUtils.hpp"                                     // 标准工具函数
#include "../Share/DLLHelper.hpp"                                    // 动态库加载辅助类
#include "MatchEngine.h"                                             // 撮合引擎

USING_NS_WTP;                                                       // 使用WonderTrader命名空间

/**
 * @brief 执行器模拟器类
 * 
 * 用于模拟执行器在历史数据上的订单执行过程
 * 继承ExecuteContext、IDataSink、IMatchSink接口，提供完整的执行器执行环境
 */
class ExecMocker : public ExecuteContext, public IDataSink, public IMatchSink
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param replayer 历史数据回放器指针
	 */
	ExecMocker(HisDataReplayer* replayer);
	virtual ~ExecMocker();                                           // 虚析构函数

public:
	//////////////////////////////////////////////////////////////////////////
	//IMatchSink接口实现
	/**
	 * @brief 处理成交回报
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param vol 成交数量
	 * @param fireprice 触发价格
	 * @param price 成交价格
	 * @param ordTime 订单时间
	 */
	virtual void handle_trade(uint32_t localid, const char* stdCode, bool isBuy, double vol, double fireprice, double price, uint64_t ordTime) override;
	
	/**
	 * @brief 处理订单回报
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param leftover 剩余数量
	 * @param price 订单价格
	 * @param isCanceled 是否已撤单
	 * @param ordTime 订单时间
	 */
	virtual void handle_order(uint32_t localid, const char* stdCode, bool isBuy, double leftover, double price, bool isCanceled, uint64_t ordTime) override;
	
	/**
	 * @brief 处理委托回报
	 * 
	 * @param localid 本地订单ID
	 * @param stdCode 合约代码
	 * @param bSuccess 是否成功
	 * @param message 消息
	 * @param ordTime 订单时间
	 */
	virtual void handle_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message, uint64_t ordTime) override;

	//////////////////////////////////////////////////////////////////////////
	//IDataSink接口实现
	/**
	 * @brief 处理tick数据
	 * 
	 * @param stdCode 合约代码
	 * @param curTick 当前tick数据
	 * @param pxType 价格类型
	 */
	virtual void handle_tick(const char* stdCode, WTSTickData* curTick, uint32_t pxType) override;
	
	/**
	 * @brief 处理调度事件
	 * 
	 * @param uDate 日期
	 * @param uTime 时间
	 */
	virtual void handle_schedule(uint32_t uDate, uint32_t uTime) override;
	
	/**
	 * @brief 处理初始化事件
	 */
	virtual void handle_init() override;

	/**
	 * @brief 处理K线收盘事件
	 * 
	 * @param stdCode 合约代码
	 * @param period 周期
	 * @param times 周期倍数
	 * @param newBar 新的K线数据
	 */
	virtual void handle_bar_close(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	/**
	 * @brief 处理交易时段开始事件
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void handle_session_begin(uint32_t curTDate) override;

	/**
	 * @brief 处理交易时段结束事件
	 * 
	 * @param curTDate 当前交易日
	 */
	virtual void handle_session_end(uint32_t curTDate) override;

	/**
	 * @brief 处理回放完成事件
	 */
	virtual void handle_replay_done() override;

	//////////////////////////////////////////////////////////////////////////
	//ExecuteContext接口实现
	/**
	 * @brief 获取tick数据切片
	 * 
	 * @param stdCode 合约代码
	 * @param count 数量
	 * @param etime 结束时间（可选）
	 * @return tick数据切片指针
	 */
	virtual WTSTickSlice* getTicks(const char* stdCode, uint32_t count, uint64_t etime = 0) override;

	/**
	 * @brief 获取最新tick数据
	 * 
	 * @param stdCode 合约代码
	 * @return 最新tick数据指针
	 */
	virtual WTSTickData* grabLastTick(const char* stdCode) override;

	/**
	 * @brief 获取持仓数量
	 * 
	 * @param stdCode 合约代码
	 * @param validOnly 是否只查询有效持仓
	 * @param flag 标志（未使用）
	 * @return 持仓数量
	 */
	virtual double getPosition(const char* stdCode, bool validOnly = true, int32_t flag = 3) override;

	/**
	 * @brief 获取订单映射表
	 * 
	 * @param stdCode 合约代码
	 * @return 订单映射表指针（当前返回NULL）
	 */
	virtual OrderMap* getOrders(const char* stdCode) override;

	/**
	 * @brief 获取未完成订单数量
	 * 
	 * @param stdCode 合约代码
	 * @return 未完成订单数量
	 */
	virtual double getUndoneQty(const char* stdCode) override;

	/**
	 * @brief 买入订单
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param bForceClose 是否强制平仓（未使用）
	 * @return 订单ID列表
	 */
	virtual OrderIDs buy(const char* stdCode, double price, double qty, bool bForceClose = false) override;

	/**
	 * @brief 卖出订单
	 * 
	 * @param stdCode 合约代码
	 * @param price 价格
	 * @param qty 数量
	 * @param bForceClose 是否强制平仓（未使用）
	 * @return 订单ID列表
	 */
	virtual OrderIDs sell(const char* stdCode, double price, double qty, bool bForceClose = false) override;

	/**
	 * @brief 撤单（按订单ID）
	 * 
	 * @param localid 本地订单ID
	 * @return 是否成功
	 */
	virtual bool cancel(uint32_t localid) override;

	/**
	 * @brief 撤单（按合约和方向）
	 * 
	 * @param stdCode 合约代码
	 * @param isBuy 是否买入
	 * @param qty 数量（0表示全部）
	 * @return 订单ID列表
	 */
	virtual OrderIDs cancel(const char* stdCode, bool isBuy, double qty = 0) override;

	/**
	 * @brief 写日志
	 * 
	 * @param message 日志消息
	 */
	virtual void writeLog(const char* message) override;

	/**
	 * @brief 获取合约信息
	 * 
	 * @param stdCode 合约代码
	 * @return 合约信息指针
	 */
	virtual WTSCommodityInfo* getCommodityInfo(const char* stdCode) override;
	
	/**
	 * @brief 获取交易时段信息
	 * 
	 * @param stdCode 合约代码
	 * @return 交易时段信息指针
	 */
	virtual WTSSessionInfo* getSessionInfo(const char* stdCode) override;

	/**
	 * @brief 获取当前时间
	 * 
	 * @return 当前时间（Unix时间戳）
	 */
	virtual uint64_t getCurTime() override;

public:
	/**
	 * @brief 初始化执行器模拟器
	 * 
	 * @param cfg 配置信息
	 * @return 是否初始化成功
	 */
	bool	init(WTSVariant* cfg);

private:
	HisDataReplayer*	_replayer;                                   // 历史数据回放器指针

	/**
	 * @brief 执行器工厂信息结构体
	 */
	typedef struct _ExecFactInfo
	{
		std::string		_module_path;                               // 模块路径
		DllHandle		_module_inst;                               // 动态库句柄
		IExecuterFact*	_fact;                                       // 执行器工厂指针
		FuncCreateExeFact	_creator;                                // 创建工厂函数指针
		FuncDeleteExeFact	_remover;                                // 删除工厂函数指针

		_ExecFactInfo()                                             // 构造函数
		{
			_module_inst = NULL;                                    // 初始化动态库句柄为NULL
			_fact = NULL;                                           // 初始化工厂指针为NULL
		}

		~_ExecFactInfo()                                            // 析构函数
		{
			if (_fact)                                             // 如果工厂指针存在
				_remover(_fact);                                   // 删除工厂
		}
	} ExecFactInfo;
	ExecFactInfo	_factory;                                        // 执行器工厂信息

	ExecuteUnit*	_exec_unit;                                     // 执行器单元指针
	std::string		_code;                                           // 合约代码
	std::string		_period;                                         // 周期
	double			_volunit;                                        // 数量单位
	int32_t			_volmode;                                        // 数量模式：0-反复正负，-1-一直卖，+1-一直买

	double			_target;                                         // 目标仓位

	double			_position;                                       // 当前持仓
	double			_undone;                                         // 未完成订单数量
	WTSTickData*	_last_tick;                                      // 最新tick数据
	double			_sig_px;                                         // 信号价格
	uint64_t		_sig_time;                                       // 信号时间

	std::stringstream	_trade_logs;                                // 成交日志流
	uint32_t	_ord_cnt;                                         // 订单数量统计
	double		_ord_qty;                                          // 订单数量统计
	uint32_t	_cacl_cnt;                                         // 撤单数量统计
	double		_cacl_qty;                                         // 撤单数量统计
	uint32_t	_sig_cnt;                                          // 信号数量统计

	std::string	_id;                                               // 执行器ID

	MatchEngine	_matcher;                                          // 撮合引擎
};

