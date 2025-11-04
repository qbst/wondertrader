/*!
 * \file WtUftDtMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT数据管理器头文件
 *
 * 本文件定义了WtUftDtMgr类，用于管理UFT策略所需的市场数据。
 *
 * 设计逻辑：
 * 1. 数据管理器接口：实现IDataManager接口，提供统一的数据查询接口
 * 2. 实时数据缓存：使用实时tick缓存（_rt_tick_map）存储最新的tick数据
 * 3. 历史数据缓存：使用历史tick缓存（_ticks_cache）存储历史tick数据
 * 4. K线数据缓存：使用K线缓存（_bars_cache）存储K线数据
 * 5. 数据推送处理：通过handle_push_quote接收并处理实时行情推送
 * 6. 数据订阅管理：管理策略对基础K线的订阅（_subed_basic_bars）
 *
 * 主要功能：
 * - 实现IDataManager接口：提供tick、订单队列、订单明细、成交明细、K线数据查询
 * - 实时行情处理：接收并缓存实时tick数据
 * - 历史数据管理：管理历史tick和K线数据缓存
 * - 数据切片查询：提供指定数量的历史数据切片查询接口
 */
#pragma once
#include <vector>  // 向量容器头文件
#include "../Includes/IDataReader.h"  // 数据读取器接口头文件
#include "../Includes/IDataManager.h"  // 数据管理器接口头文件

#include "../Includes/FasterDefs.h"  // Faster库定义
#include "../Includes/WTSCollection.hpp"  // WTS集合类头文件

NS_WTP_BEGIN  // WonderTrader命名空间开始
class WTSVariant;  // 前置声明：配置变体类
class WTSTickData;  // 前置声明：Tick数据类
class WTSKlineSlice;  // 前置声明：K线切片类
class WTSTickSlice;  // 前置声明：Tick切片类
class IBaseDataMgr;  // 前置声明：基础数据管理器接口
class WtUftEngine;  // 前置声明：UFT引擎类

/**
 * @class WtUftDtMgr
 * @brief UFT数据管理器类
 * 
 * 管理UFT策略所需的市场数据，包括实时tick、历史tick、K线等。
 * 实现IDataManager接口，提供统一的数据查询接口。
 */
class WtUftDtMgr : public IDataManager
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建数据管理器对象，初始化所有成员变量。
	 */
	WtUftDtMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 销毁数据管理器对象，释放所有数据缓存。
	 */
	~WtUftDtMgr();

public:
	/**
	 * @brief 初始化数据管理器
	 * @param cfg 配置对象
	 * @param engine UFT引擎指针
	 * @return 是否成功
	 * 
	 * 初始化数据管理器，设置引擎指针。
	 */
	bool	init(WTSVariant* cfg, WtUftEngine* engine);

	/**
	 * @brief 处理行情推送
	 * @param stdCode 标准化合约代码
	 * @param newTick 新的Tick数据
	 * 
	 * 接收并处理实时行情推送。
	 * 将新的tick数据更新到实时缓存和历史缓存中。
	 */
	void	handle_push_quote(const char* stdCode, WTSTickData* newTick);

	//////////////////////////////////////////////////////////////////////////
	//IDataManager 接口
	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @param etime 结束时间，默认0表示最新时间
	 * @return Tick数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的Tick数据切片。
	 * 实现IDataManager接口。
	 */
	virtual WTSTickSlice* get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取订单队列数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @param etime 结束时间，默认0表示最新时间
	 * @return 订单队列数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的订单队列数据切片。
	 * 实现IDataManager接口。
	 */
	virtual WTSOrdQueSlice* get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取订单明细数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @param etime 结束时间，默认0表示最新时间
	 * @return 订单明细数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的订单明细数据切片。
	 * 实现IDataManager接口。
	 */
	virtual WTSOrdDtlSlice* get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取成交明细数据切片
	 * @param stdCode 标准化合约代码
	 * @param count 数据条数
	 * @param etime 结束时间，默认0表示最新时间
	 * @return 成交明细数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的成交明细数据切片。
	 * 实现IDataManager接口。
	 */
	virtual WTSTransSlice* get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取K线数据切片
	 * @param stdCode 标准化合约代码
	 * @param period K线周期
	 * @param times 周期倍数
	 * @param count 数据条数
	 * @param etime 结束时间，默认0表示最新时间
	 * @return K线数据切片指针，失败返回NULL
	 * 
	 * 获取指定合约的K线数据切片。
	 * 实现IDataManager接口。
	 */
	virtual WTSKlineSlice* get_kline_slice(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取最新Tick数据
	 * @param stdCode 标准化合约代码
	 * @return 最新Tick数据指针，失败返回NULL
	 * 
	 * 从实时缓存中获取指定合约的最新Tick数据。
	 * 实现IDataManager接口。
	 */
	virtual WTSTickData* grab_last_tick(const char* stdCode) override;

private:
	WtUftEngine*		_engine;  // UFT引擎指针

	wt_hashset<std::string> _subed_basic_bars;  // 已订阅的基础K线集合
	typedef WTSHashMap<std::string> DataCacheMap;  // 数据缓存映射表类型别名
	DataCacheMap*	_bars_cache;	// K线缓存映射表
	DataCacheMap*	_ticks_cache;	// 历史Tick缓存映射表
	DataCacheMap*	_rt_tick_map;	// 实时tick缓存映射表

	/**
	 * @struct NotifyItem
	 * @brief K线通知项结构体
	 * 
	 * 用于存储K线更新通知信息。
	 */
	typedef struct _NotifyItem
	{
		std::string _code;  // 合约代码
		std::string _period;  // 周期字符串
		uint32_t	_times;  // 周期倍数
		WTSBarStruct* _newBar;  // 新的K线数据指针
	} NotifyItem;  // K线通知项类型别名

	std::vector<NotifyItem> _bar_notifies;  // K线通知项列表
};

NS_WTP_END  // WonderTrader命名空间结束