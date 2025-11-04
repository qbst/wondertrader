/*!
 * \file WtUftDtMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT数据管理器实现文件
 *
 * 本文件实现了WtUftDtMgr类，用于管理UFT策略所需的市场数据。
 *
 * 设计逻辑：
 * 1. 数据缓存管理：使用WTSHashMap管理不同类型的数据缓存（K线、历史tick、实时tick）
 * 2. 实时行情处理：通过handle_push_quote接收实时行情，更新实时缓存和历史缓存
 * 3. 数据查询接口：实现IDataManager接口的所有方法，提供统一的数据查询接口
 * 4. 数据有效性检查：在更新历史缓存时检查数据有效性，过滤无效数据
 * 5. 资源管理：析构时释放所有数据缓存资源
 *
 * 主要功能：
 * - 实现数据管理器的初始化
 * - 实现实时行情的接收和处理
 * - 实现最新tick数据的获取
 * - 实现IDataManager接口的所有方法（当前返回NULL，需后续实现）
 */
#include "WtUftDtMgr.h"  // UFT数据管理器头文件
#include "WtUftEngine.h"  // UFT引擎头文件
#include "WtHelper.h"  // 辅助工具类头文件

#include "../Share/StrUtil.hpp"  // 字符串工具头文件
#include "../Includes/WTSDataDef.hpp"  // WTS数据定义头文件
#include "../Includes/WTSVariant.hpp"  // WTS变体类头文件

#include "../WTSTools/WTSLogger.h"  // 日志工具头文件
#include "../WTSTools/WTSDataFactory.h"  // WTS数据工厂头文件


WTSDataFactory g_dataFact;  // 全局数据工厂对象

/**
 * @brief 构造函数实现
 * 
 * 创建数据管理器对象，初始化所有成员变量为NULL。
 */
WtUftDtMgr::WtUftDtMgr()
	: _engine(NULL)  // 初始化引擎指针为NULL
	, _bars_cache(NULL)  // 初始化K线缓存为NULL
	, _ticks_cache(NULL)  // 初始化历史Tick缓存为NULL
	, _rt_tick_map(NULL)  // 初始化实时tick缓存为NULL
{
}


/**
 * @brief 析构函数实现
 * 
 * 销毁数据管理器对象，释放所有数据缓存资源。
 */
WtUftDtMgr::~WtUftDtMgr()
{
	if (_bars_cache)  // 如果K线缓存存在
		_bars_cache->release();  // 释放K线缓存

	if (_ticks_cache)  // 如果历史Tick缓存存在
		_ticks_cache->release();  // 释放历史Tick缓存

	if (_rt_tick_map)  // 如果实时tick缓存存在
		_rt_tick_map->release();  // 释放实时tick缓存
}

/**
 * @brief 初始化数据管理器实现
 * @param cfg 配置对象
 * @param engine UFT引擎指针
 * @return 是否成功
 * 
 * 初始化数据管理器，设置引擎指针。
 * 当前实现仅保存引擎指针，后续可根据配置初始化数据缓存。
 */
bool WtUftDtMgr::init(WTSVariant* cfg, WtUftEngine* engine)
{
	_engine = engine;  // 保存引擎指针

	return true;  // 返回成功
}

/**
 * @brief 处理行情推送实现
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据
 * 
 * 接收并处理实时行情推送。
 * 处理流程：
 * 1. 检查tick数据是否有效
 * 2. 如果实时缓存不存在，创建实时缓存
 * 3. 将新的tick数据添加到实时缓存
 * 4. 如果历史缓存存在，将tick数据追加到历史缓存（过滤无效数据）
 */
void WtUftDtMgr::handle_push_quote(const char* stdCode, WTSTickData* newTick)
{
	if (newTick == NULL)  // 如果tick数据为空，直接返回
		return;

	if (_rt_tick_map == NULL)  // 如果实时缓存不存在
		_rt_tick_map = DataCacheMap::create();  // 创建实时缓存映射表

	_rt_tick_map->add(stdCode, newTick, true);  // 将新的tick数据添加到实时缓存（true表示如果存在则替换）

	if(_ticks_cache != NULL)  // 如果历史缓存存在
	{
		WTSHisTickData* tData = (WTSHisTickData*)_ticks_cache->get(stdCode);  // 获取该合约的历史tick数据
		if (tData == NULL)  // 如果该合约的历史数据不存在，直接返回
			return;

		if (tData->isValidOnly() && newTick->volume() == 0)  // 如果历史数据只接受有效数据，且新tick成交量为0
			return;  // 跳过无效数据

		tData->appendTick(newTick->getTickStruct());  // 将新tick追加到历史数据中
	}
}

/**
 * @brief 获取最新Tick数据实现
 * @param code 合约代码
 * @return 最新Tick数据指针，失败返回NULL
 * 
 * 从实时缓存中获取指定合约的最新Tick数据。
 * 使用grab方法获取数据，调用者需要负责释放返回的数据。
 */
WTSTickData* WtUftDtMgr::grab_last_tick(const char* code)
{
	if (_rt_tick_map == NULL)  // 如果实时缓存不存在
		return NULL;  // 返回NULL

	WTSTickData* curTick = (WTSTickData*)_rt_tick_map->grab(code);  // 从实时缓存中获取最新tick数据
	if (curTick == NULL)  // 如果数据不存在
		return NULL;  // 返回NULL

	return curTick;  // 返回最新tick数据
}


/**
 * @brief 获取Tick数据切片实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @param etime 结束时间，默认0表示最新时间
 * @return Tick数据切片指针，失败返回NULL
 * 
 * 获取指定合约的Tick数据切片。
 * 当前实现返回NULL，需后续实现具体逻辑。
 */
WTSTickSlice* WtUftDtMgr::get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	return NULL;  // 当前未实现，返回NULL
}

/**
 * @brief 获取订单队列数据切片实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @param etime 结束时间，默认0表示最新时间
 * @return 订单队列数据切片指针，失败返回NULL
 * 
 * 获取指定合约的订单队列数据切片。
 * 当前实现返回NULL，需后续实现具体逻辑。
 */
WTSOrdQueSlice* WtUftDtMgr::get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	return NULL;  // 当前未实现，返回NULL
}

/**
 * @brief 获取订单明细数据切片实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @param etime 结束时间，默认0表示最新时间
 * @return 订单明细数据切片指针，失败返回NULL
 * 
 * 获取指定合约的订单明细数据切片。
 * 当前实现返回NULL，需后续实现具体逻辑。
 */
WTSOrdDtlSlice* WtUftDtMgr::get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	return NULL;  // 当前未实现，返回NULL
}

/**
 * @brief 获取成交明细数据切片实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @param etime 结束时间，默认0表示最新时间
 * @return 成交明细数据切片指针，失败返回NULL
 * 
 * 获取指定合约的成交明细数据切片。
 * 当前实现返回NULL，需后续实现具体逻辑。
 */
WTSTransSlice* WtUftDtMgr::get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime /* = 0 */)
{
	return NULL;  // 当前未实现，返回NULL
}

/**
 * @brief 获取K线数据切片实现
 * @param stdCode 标准化合约代码
 * @param period K线周期
 * @param times 周期倍数
 * @param count 数据条数
 * @param etime 结束时间，默认0表示最新时间
 * @return K线数据切片指针，失败返回NULL
 * 
 * 获取指定合约的K线数据切片。
 * 当前实现返回NULL，需后续实现具体逻辑。
 */
WTSKlineSlice* WtUftDtMgr::get_kline_slice(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime /* = 0 */)
{
	return NULL;  // 当前未实现，返回NULL
}
