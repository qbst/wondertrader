/*!
 * \file UftStraContext.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT策略上下文类实现文件
 *
 * 本文件实现了UftStraContext类，用于管理UFT（Ultra Fast Trading）策略的交易上下文。
 *
 * 设计逻辑：
 * 1. 本地持仓管理：使用内存映射文件存储持仓、订单、成交、回合等数据，支持程序重启后恢复持仓状态
 * 2. 净持仓模式：采用净持仓模式管理持仓，支持多策略在同一合约上开相反头寸的情况
 * 3. 持仓明细追踪：为每个持仓开仓记录明细，支持按明细平仓和盈亏计算
 * 4. 数据持久化：使用内存映射文件实现数据持久化，交易日切换时自动清理历史数据
 * 5. 事件转发：将交易事件、市场数据事件转发给策略对象处理
 * 6. 手动持仓导入：支持从YAML文件手动导入持仓，用于特殊场景下的持仓恢复
 *
 * 主要功能：
 * - 实现策略交易接口：买入、卖出、开多、开空、平多、平空、撤单等
 * - 管理本地持仓：维护策略的本地持仓明细，计算持仓盈亏
 * - 成交回报处理：处理成交回报，更新持仓明细和盈亏
 * - 订单回报处理：处理订单回报，更新订单状态
 * - 数据加载：从内存映射文件加载历史持仓、订单等数据
 * - 事件回调转发：将各种事件回调转发给策略对象
 */
#include "UftStraContext.h"  // UFT策略上下文头文件
#include "WtUftEngine.h"  // UFT引擎头文件
#include "TraderAdapter.h"  // 交易适配器头文件
#include "WtHelper.h"  // WonderTrader辅助函数头文件
#include "ShareManager.h"  // 共享管理器头文件

#include "../Includes/UftStrategyDefs.h"  // UFT策略定义头文件
#include "../Includes/WTSContractInfo.hpp"  // 合约信息头文件
#include "../Includes/IBaseDataMgr.h"  // 基础数据管理器接口头文件
#include "../Includes/WTSContractInfo.hpp"  // 合约信息头文件（重复包含，可能是历史遗留）
#include "../Includes/WTSVariant.hpp"  // 变体类型头文件

#include "../Share/decimal.h"  // 十进制数处理头文件
#include "../Share/TimeUtils.hpp"  // 时间工具头文件

#include "../WTSTools/WTSLogger.h"  // 日志工具头文件
#include "../WTSUtils/WTSCfgLoader.h"  // 配置加载器头文件

static const uint32_t DATA_SIZE_STEP = 8000;	// 数据块大小步长，信息量每天最多4000

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 生成UFT上下文ID
 * @return 上下文ID
 * 
 * 使用原子操作生成唯一的上下文ID，从6000开始递增。
 */
inline uint32_t makeUftCtxId()
{
	static std::atomic<uint32_t> _auto_context_id{ 6000 };  // 静态原子变量，初始值为6000
	return _auto_context_id.fetch_add(1);  // 原子递增并返回旧值
}

/**
 * @brief 构造函数实现
 * @param engine UFT引擎指针
 * @param name 策略名称
 * 
 * 初始化策略上下文对象，设置引擎指针、策略对象和交易日。
 */
UftStraContext::UftStraContext(WtUftEngine* engine, const char* name)
	: IUftStraCtx(name)  // 调用基类构造函数，传入策略名称
	, _engine(engine)  // 设置UFT引擎指针
	, _strategy(NULL)  // 初始化策略对象指针为空
	, _tradingday(0)  // 初始化交易日为0
{
	_context_id = makeUftCtxId();  // 生成唯一的上下文ID
}


/**
 * @brief 析构函数实现
 * 
 * 清理策略上下文对象资源。
 */
UftStraContext::~UftStraContext()
{
}

/**
 * @brief 设置交易适配器实现
 * @param trader 交易适配器指针
 * 
 * 将交易适配器绑定到上下文，用于执行交易操作。
 */
void UftStraContext::setTrader(TraderAdapter* trader)
{
	_trader = trader;  // 设置交易适配器指针
}

/**
 * @brief 初始化回调实现
 * 
 * 当策略初始化时调用，通知策略进行初始化操作。
 */
void UftStraContext::on_init()
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_init(this);  // 调用策略的初始化方法
}


/**
 * @brief Tick数据回调实现
 * @param stdCode 标准化合约代码
 * @param newTick 新的Tick数据
 * 
 * 当收到新的Tick数据时调用，更新持仓盈亏并通知策略。
 * 计算每个持仓明细的持仓盈亏，以及总持仓的动态盈亏。
 */
void UftStraContext::on_tick(const char* stdCode, WTSTickData* newTick)
{
	auto it = _positions.find(stdCode);  // 查找该合约的持仓信息
	if(it != _positions.end())  // 如果找到持仓信息
	{
		WTSCommodityInfo* commInfo = newTick->getContractInfo()->getCommInfo();  // 获取商品信息
		uint32_t volscale = commInfo->getVolScale();  // 获取数量乘数
		PosInfo& pInfo = it->second;  // 获取持仓信息引用

		// 遍历所有持仓明细，更新每个明细的持仓盈亏
		for(auto i = pInfo._valid_idx; i < pInfo._details.size(); i++)
		{
			auto& ds = pInfo._details[i];  // 获取持仓明细引用
			if (ds->_volume == 0)  // 如果持仓数量为0
				ds->_position_profit = 0;  // 持仓盈亏为0
			else
				// 计算持仓盈亏：(当前价格 - 开仓价格) * 持仓数量 * 数量乘数 * 方向系数（多仓为1，空仓为-1）
				ds->_position_profit = (newTick->price() - ds->_open_price)*ds->_volume*volscale*(ds->_direct == 0 ? 1 : -1);
		}

		// 计算总持仓的动态盈亏
		if (decimal::gt(pInfo._volume, 0.0))  // 如果持仓数量大于0（多仓）
			// 多仓盈亏 = 当前价格 * 持仓数量 * 数量乘数 - 开仓成本
			pInfo._dynprofit = newTick->price()*pInfo._volume*volscale - pInfo._opencost;
		else if (decimal::lt(pInfo._volume, 0.0))  // 如果持仓数量小于0（空仓）
			// 空仓盈亏 = 当前价格 * 持仓数量 * 数量乘数 + 开仓成本（注意空仓开仓成本为负）
			pInfo._dynprofit = newTick->price()*pInfo._volume*volscale + pInfo._opencost;
		else  // 如果持仓数量为0
			pInfo._dynprofit = 0;  // 动态盈亏为0
	}

	// 转发Tick数据给策略处理
	if (_strategy)  // 如果策略对象存在
		_strategy->on_tick(this, stdCode, newTick);  // 调用策略的Tick回调方法
}

/**
 * @brief 订单队列数据回调实现
 * @param stdCode 标准化合约代码
 * @param newOrdQue 新的订单队列数据
 * 
 * 当收到新的订单队列数据时调用，转发给策略处理。
 */
void UftStraContext::on_order_queue(const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_order_queue(this, stdCode, newOrdQue);  // 调用策略的订单队列回调方法
}

/**
 * @brief 订单明细数据回调实现
 * @param stdCode 标准化合约代码
 * @param newOrdDtl 新的订单明细数据
 * 
 * 当收到新的订单明细数据时调用，转发给策略处理。
 */
void UftStraContext::on_order_detail(const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_order_detail(this, stdCode, newOrdDtl);  // 调用策略的订单明细回调方法
}

/**
 * @brief 成交明细数据回调实现
 * @param stdCode 标准化合约代码
 * @param newTrans 新的成交明细数据
 * 
 * 当收到新的成交明细数据时调用，转发给策略处理。
 */
void UftStraContext::on_transaction(const char* stdCode, WTSTransData* newTrans)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_transaction(this, stdCode, newTrans);  // 调用策略的成交明细回调方法
}

/**
 * @brief K线数据回调实现
 * @param code 合约代码
 * @param period 周期字符串
 * @param times 周期倍数
 * @param newBar 新的K线数据
 * 
 * 当收到新的K线数据时调用，转发给策略处理。
 */
void UftStraContext::on_bar(const char* code, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_bar(this, code, period, times, newBar);  // 调用策略的K线回调方法
}

/**
 * @brief 成交回报回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多（true-做多，false-做空）
 * @param offset 开平标志：0-开仓，1-平仓，2-平今
 * @param vol 成交数量
 * @param price 成交价格
 * 
 * 当订单成交时调用，更新本地持仓并通知策略。
 * 
 * 核心逻辑：
 * 1. 判断是否为买入操作：做多开仓或做空平仓都是买入操作
 * 2. 买入时：如果有空头持仓，先平空；剩余数量开多仓
 * 3. 卖出时：如果有多头持仓，先平多；剩余数量开空仓
 * 4. 更新持仓明细、盈亏、回合记录等
 * 
 * 注意：采用净持仓模式，支持多策略在同一合约上开相反头寸的情况。
 */
void UftStraContext::on_trade(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double vol, double price)
{
	if (!is_my_order(localid))  // 如果不是我的订单，直接返回
		return;

	WTSContractInfo* cInfo = _engine->get_contract_info(stdCode);  // 获取合约信息

	uint32_t volscale = cInfo->getCommInfo()->getVolScale();  // 获取数量乘数

	PosInfo& pItem = _positions[stdCode];  // 获取或创建持仓信息

	uint64_t now = TimeUtils::getLocalTimeNow();  // 获取当前时间

	/*
	 *	By Wesley
	 *	这里要考虑多个策略在相同合约上开相反头寸的情况，开仓可能变成平仓，所以本地持仓只能是净头寸持仓
	 */
	// 判断是否为买入操作：做多开仓（isLong=true && offset=0）或做空平仓（isLong=false && offset!=0）都是买入操作
	bool isBuy = (isLong && offset==0) || (!isLong && offset!=0);

	if(isBuy)  // 如果是买入操作
	{
		double unhandle = vol;  // 未处理的成交数量

		// 买入的时候，如果有空头，就先平空
		if(decimal::lt(pItem._volume, 0))  // 如果当前持仓为空仓（负数）
		{
			double thisQty = min(abs(pItem._volume), vol);  // 计算需要平仓的数量，取空仓数量和成交数量的较小值

			pItem._volume += thisQty;  // 更新持仓数量（减少空仓）
			unhandle -= thisQty;  // 减去已处理的数量

			double left = thisQty;  // 剩余需要平仓的数量
			// 遍历持仓明细，找到空仓明细进行平仓
			for (uint32_t idx = pItem._valid_idx; idx < pItem._details.size(); idx++)
			{
				uft::DetailStruct* pDS = pItem._details[idx];  // 获取持仓明细指针
				// 只有索引递增，才递进，不递增就不递进（用于跳过已平仓的明细）
				if (decimal::eq(pDS->_volume, 0.0) && (idx == pItem._valid_idx + 1))
				{
					pItem._valid_idx++;  // 递增有效索引
					continue;  // 跳过已平仓的明细
				}

				// 只平空头
				if (pDS->_direct != 1)  // 如果不是空仓（direct=1表示空仓）
				{
					continue;  // 跳过非空仓明细
				}

				if(decimal::eq(left, 0))  // 如果已经没有需要平仓的数量
					break;  // 退出循环

				// 计算明细最大平仓量
				double maxQty = std::min(left, pDS->_volume);  // 取剩余平仓数量和明细持仓数量的较小值

				// 生成回合明细（记录完整的开平仓信息）
				{
					SpinLock lock(_rnd_blk._mutex);  // 加锁保护回合数据块
					uint32_t ridx = _rnd_blk._block->_size;  // 获取当前回合索引
					_rnd_blk._block->_size++;  // 增加回合数量
					uft::RoundStruct& rs = _rnd_blk._block->_rounds[ridx];  // 获取回合结构引用
					wt_strcpy(rs._code, cInfo->getCode());  // 复制合约代码
					wt_strcpy(rs._exchg, cInfo->getExchg());  // 复制交易所代码

					rs._open_price = pDS->_open_price;  // 设置开仓价格
					rs._open_time = pDS->_open_time;  // 设置开仓时间
					rs._close_price = price;  // 设置平仓价格
					rs._close_time = now;  // 设置平仓时间
					rs._direct = 1;  // 设置方向为空仓
					rs._volume = maxQty;  // 设置成交数量
					rs._profit = (rs._open_price - rs._close_price)*maxQty*volscale;  // 计算盈亏（空仓：开仓价-平仓价）

					pItem._total_profit += rs._profit;  // 累加总盈亏
					pDS->_closed_profit += rs._profit;  // 累加明细的平仓盈亏
				}

				// 落地成交明细（记录成交信息）
				{
					SpinLock lock(_trd_blk._mutex);  // 加锁保护成交数据块
					uint32_t tidx = _trd_blk._block->_size;  // 获取当前成交索引
					_trd_blk._block->_size++;  // 增加成交数量
					uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];  // 获取成交结构引用
					wt_strcpy(ts._code, cInfo->getCode());  // 复制合约代码
					wt_strcpy(ts._exchg, cInfo->getExchg());  // 复制交易所代码
					ts._direct = 1;  // 设置方向为空仓
					ts._offset = 1;  // 设置开平标志为平仓
					ts._price = price;  // 设置成交价格
					ts._volume = maxQty;  // 设置成交数量
					ts._trading_date = _tradingday;  // 设置交易日期
					ts._trading_time = now;  // 设置交易时间
				}

				// 更新持仓明细
				pDS->_position_profit *= (1 - maxQty / pDS->_volume);  // 按比例更新持仓盈亏
				pDS->_volume -= maxQty;  // 减少持仓数量
				pDS->_closed_volume += maxQty;  // 累加已平仓数量
				pItem._opencost -= maxQty * volscale*pDS->_open_price;  // 减少开仓成本
				left -= maxQty;  // 减少剩余平仓数量
			}
		}

		// 如果还有剩余的没处理，则开多仓
		if (decimal::gt(unhandle, 0))  // 如果还有未处理的成交数量
		{
			SpinLock lock(_pos_blk._mutex);  // 加锁保护持仓数据块
			uint32_t idx = _pos_blk._block->_size;  // 获取当前持仓明细索引
			_pos_blk._block->_size++;  // 增加持仓明细数量
			uft::DetailStruct& ds = _pos_blk._block->_details[idx];  // 获取持仓明细结构引用
			wt_strcpy(ds._code, cInfo->getCode());  // 复制合约代码
			wt_strcpy(ds._exchg, cInfo->getExchg());  // 复制交易所代码
			ds._direct = 0;  // 设置方向为多仓
			ds._open_price = price;  // 设置开仓价格
			ds._open_time = now;  // 设置开仓时间
			ds._position_profit = 0;  // 初始化持仓盈亏为0
			ds._open_tdate = _tradingday;  // 设置开仓交易日
			ds._volume = unhandle;  // 设置持仓数量

			ds._position_profit = 0;  // 初始化持仓盈亏为0
			ds._closed_volume = 0;  // 初始化已平仓数量为0
			ds._closed_profit = 0;  // 初始化已平仓盈亏为0

			pItem._details.emplace_back(&ds);  // 将持仓明细添加到列表
			pItem._opencost += unhandle * volscale*price;  // 累加开仓成本
			pItem._volume += unhandle;  // 增加持仓数量
		}

		// 落地开多成交明细（记录开多仓的成交信息）
		{
			SpinLock lock(_trd_blk._mutex);  // 加锁保护成交数据块
			uint32_t tidx = _trd_blk._block->_size;  // 获取当前成交索引
			_trd_blk._block->_size++;  // 增加成交数量
			uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];  // 获取成交结构引用
			wt_strcpy(ts._code, cInfo->getCode());  // 复制合约代码
			wt_strcpy(ts._exchg, cInfo->getExchg());  // 复制交易所代码
			ts._direct = 0;  // 设置方向为多仓
			ts._offset = 0;  // 设置开平标志为开仓
			ts._price = price;  // 设置成交价格
			ts._volume = unhandle;  // 设置成交数量
			ts._trading_date = _tradingday;  // 设置交易日期
			ts._trading_time = now;  // 设置交易时间
		}
	}
	else  // 如果是卖出操作（做空开仓或做多平仓）
	{
		double unhandle = vol;  // 未处理的成交数量

		// 卖出的时候，有多头就先平多
		if (decimal::gt(pItem._volume, 0))  // 如果当前持仓为多仓（正数）
		{
			double thisQty = min(pItem._volume, vol);  // 计算需要平仓的数量，取多仓数量和成交数量的较小值

			pItem._volume -= thisQty;  // 更新持仓数量（减少多仓）
			unhandle -= thisQty;  // 减去已处理的数量

			double left = thisQty;  // 剩余需要平仓的数量
			// 遍历持仓明细，找到多仓明细进行平仓
			for (uint32_t idx = pItem._valid_idx; idx < pItem._details.size(); idx++)
			{
				uft::DetailStruct* pDS = pItem._details[idx];  // 获取持仓明细指针
				// 只有索引递增，才递进，不递增就不递进（用于跳过已平仓的明细）
				if (decimal::eq(pDS->_volume, 0.0) && (idx == pItem._valid_idx + 1))
				{
					pItem._valid_idx++;  // 递增有效索引
					continue;  // 跳过已平仓的明细
				}

				// 只平多头
				if (pDS->_direct != 0)  // 如果不是多仓（direct=0表示多仓）
				{
					continue;  // 跳过非多仓明细
				}

				if (decimal::eq(left, 0))  // 如果已经没有需要平仓的数量
					break;  // 退出循环

				double maxQty = std::min(left, pDS->_volume);  // 计算明细最大平仓量

				// 生成回合明细（记录完整的开平仓信息）
				{
					SpinLock lock(_rnd_blk._mutex);  // 加锁保护回合数据块
					uint32_t ridx = _rnd_blk._block->_size;  // 获取当前回合索引
					_rnd_blk._block->_size++;  // 增加回合数量
					uft::RoundStruct& rs = _rnd_blk._block->_rounds[ridx];  // 获取回合结构引用
					wt_strcpy(rs._code, cInfo->getCode());  // 复制合约代码
					wt_strcpy(rs._exchg, cInfo->getExchg());  // 复制交易所代码

					rs._open_price = pDS->_open_price;  // 设置开仓价格
					rs._open_time = pDS->_open_time;  // 设置开仓时间
					rs._close_price = price;  // 设置平仓价格
					rs._close_time = now;  // 设置平仓时间
					rs._direct = 0;  // 设置方向为多仓
					rs._volume = maxQty;  // 设置成交数量
					rs._profit = (rs._close_price - rs._open_price)*maxQty*volscale;  // 计算盈亏（多仓：平仓价-开仓价）

					pItem._total_profit += rs._profit;  // 累加总盈亏
					pDS->_closed_profit += rs._profit;  // 累加明细的平仓盈亏
				}

				// 落地平多的成交明细（记录成交信息）
				{
					SpinLock lock(_trd_blk._mutex);  // 加锁保护成交数据块
					uint32_t tidx = _trd_blk._block->_size;  // 获取当前成交索引
					_trd_blk._block->_size++;  // 增加成交数量
					uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];  // 获取成交结构引用
					wt_strcpy(ts._code, cInfo->getCode());  // 复制合约代码
					wt_strcpy(ts._exchg, cInfo->getExchg());  // 复制交易所代码
					ts._direct = 0;  // 设置方向为多仓
					ts._offset = 1;  // 设置开平标志为平仓
					ts._price = price;  // 设置成交价格
					ts._volume = maxQty;  // 设置成交数量
					ts._trading_date = _tradingday;  // 设置交易日期
					ts._trading_time = now;  // 设置交易时间
				}

				// 更新持仓明细
				pDS->_position_profit *= (1 - maxQty / pDS->_volume);  // 按比例更新持仓盈亏
				pDS->_volume -= maxQty;  // 减少持仓数量
				pDS->_closed_volume += maxQty;  // 累加已平仓数量
				pItem._opencost -= maxQty * volscale*pDS->_open_price;  // 减少开仓成本
				left -= maxQty;  // 减少剩余平仓数量
			}
		}

		// 如果还有剩余的没处理，则开空仓
		if (decimal::gt(unhandle, 0))  // 如果还有未处理的成交数量
		{
			SpinLock lock(_pos_blk._mutex);  // 加锁保护持仓数据块
			uint32_t idx = _pos_blk._block->_size;  // 获取当前持仓明细索引
			_pos_blk._block->_size++;  // 增加持仓明细数量
			uft::DetailStruct& ds = _pos_blk._block->_details[idx];  // 获取持仓明细结构引用
			wt_strcpy(ds._code, cInfo->getCode());  // 复制合约代码
			wt_strcpy(ds._exchg, cInfo->getExchg());  // 复制交易所代码
			ds._direct = 1;  // 设置方向为空仓
			ds._open_price = price;  // 设置开仓价格
			ds._open_time = now;  // 设置开仓时间
			ds._position_profit = 0;  // 初始化持仓盈亏为0
			ds._open_tdate = _tradingday;  // 设置开仓交易日
			ds._volume = unhandle;  // 设置持仓数量

			ds._position_profit = 0;  // 初始化持仓盈亏为0
			ds._closed_volume = 0;  // 初始化已平仓数量为0
			ds._closed_profit = 0;  // 初始化已平仓盈亏为0

			pItem._details.emplace_back(&ds);  // 将持仓明细添加到列表
			pItem._opencost += unhandle * volscale*price;  // 累加开仓成本
			pItem._volume -= unhandle;  // 减少持仓数量（开空仓，持仓数量为负）
		}

		// 生成开空成交明细（记录开空仓的成交信息）
		{
			SpinLock lock(_trd_blk._mutex);  // 加锁保护成交数据块
			uint32_t tidx = _trd_blk._block->_size;  // 获取当前成交索引
			_trd_blk._block->_size++;  // 增加成交数量
			uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];  // 获取成交结构引用
			wt_strcpy(ts._code, cInfo->getCode());  // 复制合约代码
			wt_strcpy(ts._exchg, cInfo->getExchg());  // 复制交易所代码
			ts._direct = 1;  // 设置方向为空仓
			ts._offset = 0;  // 设置开平标志为开仓
			ts._price = price;  // 设置成交价格
			ts._volume = vol;  // 设置成交数量（注意：这里使用的是vol而不是unhandle，可能是历史遗留问题）
			ts._trading_date = _tradingday;  // 设置交易日期
			ts._trading_time = now;  // 设置交易时间
		}
	}

	/*
	 * 以下是被注释掉的旧版本代码，使用分离的多空持仓管理方式
	 * 新版本使用净持仓模式，更加简洁高效
	 */
	/*
	if (isLong)
	{
		if (isOpen)
		{
			pItem.l_volume += vol;

			{
				SpinLock lock(_pos_blk._mutex);
				uint32_t idx = _pos_blk._block->_size;
				_pos_blk._block->_size++;
				uft::DetailStruct& ds = _pos_blk._block->_details[idx];
				wt_strcpy(ds._code, cInfo->getCode());
				wt_strcpy(ds._exchg, cInfo->getExchg());
				ds._direct = 0;
				ds._open_price = price;
				ds._open_time = now;
				ds._position_profit = 0;
				ds._open_tdate = _tradingday;
				ds._volume = vol;

				ds._position_profit = 0;
				ds._closed_volume = 0;
				ds._closed_profit = 0;

				pItem._details.emplace_back(&ds);
				pItem.l_opencost += vol * volscale*price;
			}

			{
				SpinLock lock(_trd_blk._mutex);
				uint32_t tidx = _trd_blk._block->_size;
				_trd_blk._block->_size++;
				uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];
				wt_strcpy(ts._code, cInfo->getCode());
				wt_strcpy(ts._exchg, cInfo->getExchg());
				ts._direct = 0;
				ts._offset = offset;
				ts._price = price;
				ts._volume = vol;
				ts._trading_date = _tradingday;
				ts._trading_time = now;
			}
		}
		else
		{
			//处理平仓
			pItem.l_volume -= vol;
			
			double left = vol;
			for(uint32_t idx = pItem._valid_idx; idx < pItem._details.size(); idx++)
			{
				uft::DetailStruct* pDS = pItem._details[idx];
				//只有索引递增，才递进，不递增就不递进
				if(decimal::eq(pDS->_volume, 0.0) && (idx == pItem._valid_idx+1))
				{
					pItem._valid_idx++;
					continue;
				}

				if (pDS->_direct != 0)
				{
					continue;
				}

				double maxQty = std::min(left, pDS->_volume);
				{
					SpinLock lock(_rnd_blk._mutex);
					uint32_t ridx = _rnd_blk._block->_size;
					_rnd_blk._block->_size++;
					uft::RoundStruct& rs = _rnd_blk._block->_rounds[ridx];
					wt_strcpy(rs._code, cInfo->getCode());
					wt_strcpy(rs._exchg, cInfo->getExchg());

					rs._open_price = pDS->_open_price;
					rs._open_time = pDS->_open_time;
					rs._close_price = price;
					rs._close_time = now;
					rs._direct = 0;
					rs._volume = maxQty;
					rs._profit = (rs._close_price - rs._open_price)*maxQty*cInfo->getCommInfo()->getVolScale();
					pItem.total_profit += rs._profit;
				}	

				pDS->_volume -= maxQty;
				pItem.l_opencost -= maxQty * volscale*price;
				left -= maxQty;
			}

			{
				SpinLock lock(_trd_blk._mutex);
				uint32_t tidx = _trd_blk._block->_size;
				_trd_blk._block->_size++;
				uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];
				wt_strcpy(ts._code, cInfo->getCode());
				wt_strcpy(ts._exchg, cInfo->getExchg());
				ts._direct = 0;
				ts._offset = offset;
				ts._price = price;
				ts._volume = vol;
				ts._trading_date = _tradingday;
				ts._trading_time = now;
			}
		}
	}
	else
	{
		if (isOpen)
		{
			pItem.s_volume += vol;

			{
				SpinLock lock(_pos_blk._mutex);
				uint32_t idx = _pos_blk._block->_size;
				_pos_blk._block->_size++;
				uft::DetailStruct& ds = _pos_blk._block->_details[idx];
				wt_strcpy(ds._code, cInfo->getCode());
				wt_strcpy(ds._exchg, cInfo->getExchg());
				ds._direct = 1;
				ds._open_price = price;
				ds._open_time = TimeUtils::getLocalTimeNow();
				ds._position_profit = 0;
				ds._open_tdate = _tradingday;
				ds._volume = vol;

				ds._position_profit = 0;
				ds._closed_volume = 0;
				ds._closed_profit = 0;

				pItem._details.emplace_back(&ds);
				pItem.s_opencost += vol * volscale*price;
			}

			{
				SpinLock lock(_trd_blk._mutex);
				uint32_t tidx = _trd_blk._block->_size;
				_trd_blk._block->_size++;
				uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];
				wt_strcpy(ts._code, cInfo->getCode());
				wt_strcpy(ts._exchg, cInfo->getExchg());
				ts._direct = 1;
				ts._offset = offset;
				ts._price = price;
				ts._volume = vol;
				ts._trading_date = _tradingday;
				ts._trading_time = now;
			}
		}
		else
		{
			pItem.s_volume -= vol;

			double left = vol;
			for (uint32_t idx = pItem._valid_idx; idx < pItem._details.size(); idx++)
			{
				uft::DetailStruct* pDS = pItem._details[idx];
				//只有索引递增，才递进，不递增就不递进
				if (decimal::eq(pDS->_volume, 0.0) && (idx == pItem._valid_idx + 1))
				{
					pItem._valid_idx++;
					continue;
				}

				if (pDS->_direct != 1)
					continue;

				double maxQty = std::min(left, pDS->_volume);
				{
					SpinLock lock(_rnd_blk._mutex);
					uint32_t ridx = _rnd_blk._block->_size;
					_rnd_blk._block->_size++;
					uft::RoundStruct& rs = _rnd_blk._block->_rounds[ridx];
					wt_strcpy(rs._code, cInfo->getCode());
					wt_strcpy(rs._exchg, cInfo->getExchg());

					rs._open_price = pDS->_open_price;
					rs._open_time = pDS->_open_time;
					rs._close_price = price;
					rs._close_time = now;
					rs._direct = 1;
					rs._volume = maxQty;
					rs._profit = -1*(rs._close_price - rs._open_price)*maxQty*cInfo->getCommInfo()->getVolScale();
					pItem.total_profit += rs._profit;
				}

				pDS->_volume -= maxQty;
				left -= maxQty;
				pItem.s_opencost -= maxQty * volscale*price;
			}

			{
				SpinLock lock(_trd_blk._mutex);
				uint32_t tidx = _trd_blk._block->_size;
				_trd_blk._block->_size++;
				uft::TradeStruct& ts = _trd_blk._block->_trades[tidx];
				wt_strcpy(ts._code, cInfo->getCode());
				wt_strcpy(ts._exchg, cInfo->getExchg());
				ts._direct = 1;
				ts._offset = offset;
				ts._price = price;
				ts._volume = vol;
				ts._trading_date = _tradingday;
				ts._trading_time = now;
			}
		}
	}
	*/

	// 转发成交回报给策略处理
	if (_strategy)  // 如果策略对象存在
		_strategy->on_trade(this, localid, stdCode, isLong, offset, vol, price);  // 调用策略的成交回调方法
}

/**
 * @brief 订单回报回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多（true-做多，false-做空）
 * @param offset 开平标志：0-开仓，1-平仓，2-平今
 * @param totalQty 总委托数量
 * @param leftQty 剩余数量
 * @param price 委托价格
 * @param isCanceled 是否已撤销，默认为false
 * 
 * 当订单状态发生变化时调用，更新订单状态并通知策略。
 * 如果订单不存在于订单映射表中，直接返回。
 * 如果订单结构还未创建，则在订单数据块中创建订单记录。
 */
void UftStraContext::on_order(uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled /* = false */)
{
	auto it = _order_ids.find(localid);  // 查找订单ID
	if (it == _order_ids.end())  // 如果订单不存在，直接返回
		return;

	WTSContractInfo* cInfo = _engine->get_contract_info(stdCode);  // 获取合约信息
	uft::OrderStruct*& curOrd = it->second;  // 获取订单结构指针的引用
	if(curOrd == NULL)  // 如果订单结构还未创建
	{
		SpinLock lock(_ord_blk._mutex);  // 加锁保护订单数据块
		uint32_t idx = _ord_blk._block->_size;  // 获取当前订单索引
		_ord_blk._block->_size++;  // 增加订单数量
		uft::OrderStruct& os = _ord_blk._block->_orders[idx];  // 获取订单结构引用
		wt_strcpy(os._code, cInfo->getCode());  // 复制合约代码
		wt_strcpy(os._exchg, cInfo->getExchg());  // 复制交易所代码
		os._direct = 0;  // 设置方向（注意：这里固定为0，实际方向应该从isLong参数获取，可能是历史遗留问题）
		os._offset = offset;  // 设置开平标志
		os._volume = totalQty;  // 设置委托数量
		os._price = price;  // 设置委托价格
		os._left = leftQty;  // 设置剩余数量
		os._oder_time = TimeUtils::getLocalTimeNow();  // 设置订单时间

		if (isCanceled)  // 如果订单已撤销
			os._state = 2;  // 设置订单状态为已撤销
		else
			os._state = leftQty == 0 ? 1 : 0;  // 如果剩余数量为0则全部成交，否则为有效状态

		curOrd = &os;  // 保存订单结构指针
	}

	// 转发订单回报给策略处理
	if (_strategy)  // 如果策略对象存在
		_strategy->on_order(this, localid, stdCode, isLong, offset, totalQty, leftQty, price, isCanceled);  // 调用策略的订单回调方法
}

/**
 * @brief 交易通道就绪回调实现
 * @param tradingday 交易日
 * 
 * 当交易通道连接成功并准备就绪时调用，加载本地数据并通知策略。
 * 如果交易日发生变化，会重新加载本地持仓、订单等数据。
 * 加载完本地持仓后，会通知策略当前持仓情况。
 */
void UftStraContext::on_channel_ready(uint32_t tradingday)
{
	if (_tradingday != tradingday)  // 如果交易日发生变化
	{
		_tradingday = tradingday;  // 更新交易日
		load_local_data();  // 加载本地数据（持仓、订单、成交、回合等）
	}

	// 通知策略交易通道就绪，并同步当前持仓
	if (_strategy)  // 如果策略对象存在
	{
		// 遍历所有持仓，通知策略当前持仓情况
		for (const auto& v : _positions)  // 遍历持仓映射表
		{
			const char* stdCode = v.first.c_str();  // 获取合约代码
			const PosInfo& pInfo = v.second;  // 获取持仓信息
			if (decimal::gt(pInfo._volume, 0))  // 如果持仓数量大于0（多仓）
			{
				// 通知策略多仓持仓（prevol和preavail都设置为当前持仓数量）
				_strategy->on_position(this, stdCode, true, pInfo._volume, pInfo._volume, 0, 0);
			}
			else if (decimal::lt(pInfo._volume, 0))  // 如果持仓数量小于0（空仓）
			{
				// 通知策略空仓持仓（prevol和preavail都设置为当前持仓数量，注意空仓数量为负）
				_strategy->on_position(this, stdCode, false, pInfo._volume, pInfo._volume, 0, 0);
			}
		}

		// 通知策略交易通道已就绪
		_strategy->on_channel_ready(this);  // 调用策略的交易通道就绪回调方法
	}
}

/**
 * @brief 交易通道丢失回调实现
 * 
 * 当交易通道断开连接时调用，通知策略交易通道已不可用。
 */
void UftStraContext::on_channel_lost()
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_channel_lost(this);  // 调用策略的交易通道丢失回调方法
}

/**
 * @brief 下单回报回调实现
 * @param localid 本地订单ID
 * @param stdCode 标准化合约代码
 * @param bSuccess 是否成功
 * @param message 消息内容
 * 
 * 当下单操作完成时调用，通知策略下单结果。
 * 如果订单不属于当前策略上下文，直接返回。
 */
void UftStraContext::on_entrust(uint32_t localid, const char* stdCode, bool bSuccess, const char* message)
{
	if (!is_my_order(localid))  // 如果不是我的订单，直接返回
		return;

	// 转发下单回报给策略处理
	if (_strategy)  // 如果策略对象存在
		_strategy->on_entrust(localid, bSuccess, message);  // 调用策略的下单回报回调方法
}

/**
 * @brief 持仓更新回调实现
 * @param stdCode 标准化合约代码
 * @param isLong 是否做多（true-做多，false-做空）
 * @param prevol 昨仓数量
 * @param preavail 昨仓可用数量
 * @param newvol 今仓数量
 * @param newavail 今仓可用数量
 * @param tradingday 交易日
 * 
 * 当账户持仓发生变化时调用。
 * 注意：账户的持仓通知不转发给策略，策略使用本地持仓管理。
 */
void UftStraContext::on_position(const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail, uint32_t tradingday)
{
	// 账户的持仓通知不给策略了
	// 策略使用本地持仓管理，不依赖账户持仓通知
	//if (_strategy)
	//	_strategy->on_position(this, stdCode, isLong, prevol, preavail, newvol, newavail);
}

/**
 * @brief 交易会话开始回调实现
 * @param uTDate 交易日
 * 
 * 当交易会话开始时调用，通知策略新交易日开始。
 */
void UftStraContext::on_session_begin(uint32_t uTDate)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_session_begin(this, uTDate);  // 调用策略的交易会话开始回调方法
}

/**
 * @brief 交易会话结束回调实现
 * @param uTDate 交易日
 * 
 * 当交易会话结束时调用，通知策略交易日结束。
 */
void UftStraContext::on_session_end(uint32_t uTDate)
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_session_end(this, uTDate);  // 调用策略的交易会话结束回调方法
}

/**
 * @brief 参数更新回调实现
 * 
 * 当策略参数更新时调用，通知策略参数已更新。
 */
void UftStraContext::on_params_updated()
{
	if (_strategy)  // 如果策略对象存在
		_strategy->on_params_updated();  // 调用策略的参数更新回调方法
}

/**
 * @brief 监控字符串参数实现
 * @param name 参数名称
 * @param val 初始值
 * @return 参数值的指针
 * 
 * 监控一个字符串类型的策略参数，参数值会被持久化存储。
 */
const char* UftStraContext::watch_param(const char* name, const char* val)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, val, false, true);  // 通过ShareManager分配参数值
}

/**
 * @brief 监控有符号64位整数参数实现
 * @param name 参数名称
 * @param val 初始值
 * @return 参数值
 * 
 * 监控一个有符号64位整数类型的策略参数，参数值会被持久化存储。
 */
int64_t UftStraContext::watch_param(const char* name, int64_t val)
{
	return *ShareManager::self().allocate_value(_name.c_str(), name, val, false, true);  // 通过ShareManager分配参数值并解引用
}

/**
 * @brief 监控有符号32位整数参数实现
 * @param name 参数名称
 * @param val 初始值
 * @return 参数值
 * 
 * 监控一个有符号32位整数类型的策略参数，参数值会被持久化存储。
 */
int32_t UftStraContext::watch_param(const char* name, int32_t val)
{
	return *ShareManager::self().allocate_value(_name.c_str(), name, val, false, true);  // 通过ShareManager分配参数值并解引用
}

/**
 * @brief 监控无符号64位整数参数实现
 * @param name 参数名称
 * @param val 初始值
 * @return 参数值
 * 
 * 监控一个无符号64位整数类型的策略参数，参数值会被持久化存储。
 */
uint64_t UftStraContext::watch_param(const char* name, uint64_t val)
{
	return *ShareManager::self().allocate_value(_name.c_str(), name, val, false, true);  // 通过ShareManager分配参数值并解引用
}

/**
 * @brief 监控无符号32位整数参数实现
 * @param name 参数名称
 * @param val 初始值
 * @return 参数值
 * 
 * 监控一个无符号32位整数类型的策略参数，参数值会被持久化存储。
 */
uint32_t UftStraContext::watch_param(const char* name, uint32_t val)
{
	return *ShareManager::self().allocate_value(_name.c_str(), name, val, false, true);  // 通过ShareManager分配参数值并解引用
}

/**
 * @brief 监控浮点数参数实现
 * @param name 参数名称
 * @param val 初始值
 * @return 参数值
 * 
 * 监控一个浮点数类型的策略参数，参数值会被持久化存储。
 */
double UftStraContext::watch_param(const char* name, double val)
{
	return *ShareManager::self().allocate_value(_name.c_str(), name, val, false, true);  // 通过ShareManager分配参数值并解引用
}

/**
 * @brief 提交参数监控实现
 * 
 * 提交所有通过watch_param监控的参数，使其生效。
 */
void UftStraContext::commit_param_watcher()
{
	ShareManager::self().commit_param_watcher(_name.c_str());  // 通过ShareManager提交参数监控
}

/**
 * @brief 读取字符串参数实现
 * @param name 参数名称
 * @param defVal 默认值，默认为空字符串
 * @return 参数值
 * 
 * 读取一个字符串类型的策略参数，如果参数不存在则返回默认值。
 */
const char* UftStraContext::read_param(const char* name, const char* defVal /* = "" */)
{
	return ShareManager::self().get_value(_name.c_str(), name, defVal);  // 通过ShareManager获取参数值
}

/**
 * @brief 读取有符号32位整数参数实现
 * @param name 参数名称
 * @param defVal 默认值，默认为0
 * @return 参数值
 * 
 * 读取一个有符号32位整数类型的策略参数，如果参数不存在则返回默认值。
 */
int32_t UftStraContext::read_param(const char* name, int32_t defVal /* = 0 */)
{
	return ShareManager::self().get_value(_name.c_str(), name, defVal);  // 通过ShareManager获取参数值
}

/**
 * @brief 读取无符号32位整数参数实现
 * @param name 参数名称
 * @param defVal 默认值，默认为0
 * @return 参数值
 * 
 * 读取一个无符号32位整数类型的策略参数，如果参数不存在则返回默认值。
 */
uint32_t UftStraContext::read_param(const char* name, uint32_t defVal /* = 0 */)
{
	return ShareManager::self().get_value(_name.c_str(), name, defVal);  // 通过ShareManager获取参数值
}

/**
 * @brief 读取有符号64位整数参数实现
 * @param name 参数名称
 * @param defVal 默认值，默认为0
 * @return 参数值
 * 
 * 读取一个有符号64位整数类型的策略参数，如果参数不存在则返回默认值。
 */
int64_t UftStraContext::read_param(const char* name, int64_t defVal /* = 0 */)
{
	return ShareManager::self().get_value(_name.c_str(), name, defVal);  // 通过ShareManager获取参数值
}

/**
 * @brief 读取无符号64位整数参数实现
 * @param name 参数名称
 * @param defVal 默认值，默认为0
 * @return 参数值
 * 
 * 读取一个无符号64位整数类型的策略参数，如果参数不存在则返回默认值。
 */
uint64_t UftStraContext::read_param(const char* name, uint64_t defVal /* = 0 */)
{
	return ShareManager::self().get_value(_name.c_str(), name, defVal);  // 通过ShareManager获取参数值
}

/**
 * @brief 读取浮点数参数实现
 * @param name 参数名称
 * @param defVal 默认值，默认为0
 * @return 参数值
 * 
 * 读取一个浮点数类型的策略参数，如果参数不存在则返回默认值。
 */
double UftStraContext::read_param(const char* name, double defVal /* = 0 */)
{
	return ShareManager::self().get_value(_name.c_str(), name, defVal);  // 通过ShareManager获取参数值
}

/**
 * @brief 同步有符号32位整数参数实现
 * @param name 参数名称
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @return 参数值的指针
 * 
 * 同步一个有符号32位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
 */
int32_t* UftStraContext::sync_param(const char* name, int32_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, initVal, bForceWrite, false);  // 通过ShareManager分配参数值
}

/**
 * @brief 同步无符号32位整数参数实现
 * @param name 参数名称
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @return 参数值的指针
 * 
 * 同步一个无符号32位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
 */
uint32_t* UftStraContext::sync_param(const char* name, uint32_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, initVal, bForceWrite, false);  // 通过ShareManager分配参数值
}

/**
 * @brief 同步有符号64位整数参数实现
 * @param name 参数名称
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @return 参数值的指针
 * 
 * 同步一个有符号64位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
 */
int64_t* UftStraContext::sync_param(const char* name, int64_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, initVal, bForceWrite, false);  // 通过ShareManager分配参数值
}

/**
 * @brief 同步无符号64位整数参数实现
 * @param name 参数名称
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @return 参数值的指针
 * 
 * 同步一个无符号64位整数类型的策略参数，返回参数值的指针，可以直接修改参数值。
 */
uint64_t* UftStraContext::sync_param(const char* name, uint64_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, initVal, bForceWrite, false);  // 通过ShareManager分配参数值
}

/**
 * @brief 同步浮点数参数实现
 * @param name 参数名称
 * @param initVal 初始值，默认为0
 * @param bForceWrite 是否强制写入，默认为false
 * @return 参数值的指针
 * 
 * 同步一个浮点数类型的策略参数，返回参数值的指针，可以直接修改参数值。
 */
double* UftStraContext::sync_param(const char* name, double initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, initVal, bForceWrite, false);  // 通过ShareManager分配参数值
}

/**
 * @brief 同步字符串参数实现
 * @param name 参数名称
 * @param initVal 初始值，默认为空字符串
 * @param bForceWrite 是否强制写入，默认为false
 * @return 参数值的指针
 * 
 * 同步一个字符串类型的策略参数，返回参数值的指针，可以直接修改参数值。
 */
const char* UftStraContext::sync_param(const char* name, const char* initVal /* = "" */, bool bForceWrite/* = false*/)
{
	return ShareManager::self().allocate_value(_name.c_str(), name, initVal, bForceWrite);  // 通过ShareManager分配参数值
}

/**
 * @brief 获取账户持仓实现
 * @param stdCode 标准化合约代码
 * @param bOnlyValid 是否只返回有效持仓，默认false
 * @param iFlag 持仓标志，默认0（表示多空都返回）
 * @return 持仓数量
 * 
 * 获取账户在指定合约上的持仓数量。
 */
double UftStraContext::stra_get_position(const char* stdCode, bool bOnlyValid /* = false */, int32_t iFlag /* = 0 */)
{
	return _trader->getPosition(stdCode, bOnlyValid, iFlag);  // 通过交易适配器获取账户持仓
}

/**
 * @brief 获取本地持仓实现
 * @param stdCode 标准化合约代码
 * @return 本地持仓数量
 * 
 * 获取策略在指定合约上的本地持仓数量（净持仓）。
 */
double UftStraContext::stra_get_local_position(const char* stdCode)
{
	auto it = _positions.find(stdCode);  // 查找持仓信息
	if (it == _positions.end())  // 如果不存在持仓
		return 0.0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._volume;  // 返回持仓数量（净持仓，正数表示多仓，负数表示空仓）
}

/**
 * @brief 获取本地持仓盈亏实现
 * @param stdCode 标准化合约代码
 * @return 持仓盈亏
 * 
 * 获取策略在指定合约上的本地持仓盈亏（动态盈亏）。
 */
double UftStraContext::stra_get_local_posprofit(const char* stdCode)
{
	auto it = _positions.find(stdCode);  // 查找持仓信息
	if (it == _positions.end())  // 如果不存在持仓
		return 0.0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._dynprofit;  // 返回动态盈亏
}

/**
 * @brief 获取本地平仓盈亏实现
 * @param stdCode 标准化合约代码
 * @return 平仓盈亏
 * 
 * 获取策略在指定合约上的本地平仓盈亏（累计已平仓盈亏）。
 */
double UftStraContext::stra_get_local_closeprofit(const char* stdCode)
{
	auto it = _positions.find(stdCode);  // 查找持仓信息
	if (it == _positions.end())  // 如果不存在持仓
		return 0.0;  // 返回0

	const PosInfo& pInfo = it->second;  // 获取持仓信息
	return pInfo._total_profit;  // 返回总盈亏（包括持仓盈亏和平仓盈亏）
}


/**
 * @brief 枚举持仓实现
 * @param stdCode 标准化合约代码
 * @return 持仓数量
 * 
 * 枚举账户在指定合约上的持仓数量。
 */
double UftStraContext::stra_enum_position(const char* stdCode)
{
	return _trader->enumPosition(stdCode);  // 通过交易适配器枚举账户持仓
}

/**
 * @brief 获取未完成数量实现
 * @param stdCode 标准化合约代码
 * @return 未完成数量
 * 
 * 获取指定合约的未完成订单数量。
 */
double UftStraContext::stra_get_undone(const char* stdCode)
{
	return _trader->getUndoneQty(stdCode);  // 通过交易适配器获取未完成订单数量
}

/**
 * @brief 获取信息数量实现
 * @param stdCode 标准化合约代码
 * @return 信息数量
 * 
 * 获取指定合约的信息数量（用途待确认）。
 */
uint32_t UftStraContext::stra_get_infos(const char* stdCode)
{
	return _trader->getInfos(stdCode);  // 通过交易适配器获取信息数量
}

/**
 * @brief 获取当前价格实现
 * @param stdCode 标准化合约代码
 * @return 当前价格
 * 
 * 获取指定合约的当前价格。
 */
double UftStraContext::stra_get_price(const char* stdCode)
{
	return _engine->get_cur_price(stdCode);  // 通过引擎获取当前价格
}

/**
 * @brief 获取当前日期实现
 * @return 当前日期（YYYYMMDD格式）
 * 
 * 返回当前系统日期。
 */
uint32_t UftStraContext::stra_get_date()
{
	return _engine->get_date();  // 通过引擎获取当前日期
}

/**
 * @brief 获取当前时间实现
 * @return 当前时间（HHMMSS格式）
 * 
 * 返回当前系统时间。
 */
uint32_t UftStraContext::stra_get_time()
{
	return _engine->get_raw_time();  // 通过引擎获取当前真实时间
}

/**
 * @brief 获取当前秒数实现
 * @return 当前秒数（包含毫秒）
 * 
 * 返回当前时间的秒数（包含毫秒）。
 */
uint32_t UftStraContext::stra_get_secs()
{
	return _engine->get_secs();  // 通过引擎获取当前秒数
}

/**
 * @brief 撤销订单实现
 * @param localid 本地订单ID
 * @return 是否成功
 * 
 * 撤销指定ID的订单。
 */
bool UftStraContext::stra_cancel(uint32_t localid)
{
	return _trader->cancel(localid);  // 通过交易适配器撤销订单
}

/**
 * @brief 撤销所有订单实现
 * @param stdCode 标准化合约代码
 * @return 订单ID列表
 * 
 * 撤销指定合约的所有订单，返回被撤销的订单ID列表。
 */
OrderIDs UftStraContext::stra_cancel_all(const char* stdCode)
{
	// 撤单频率检查（已注释，可能在某些场景下需要）
	//if (!_trader->checkCancelLimits(stdCode))
	//	return OrderIDs();

	return _trader->cancelAll(stdCode);  // 通过交易适配器撤销所有订单
}

/**
 * @brief 买入接口实现
 * @param stdCode 标准化合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 订单ID列表
 * 
 * 买入指定合约，返回生成的订单ID列表。
 * 将订单ID添加到订单映射表中，用于后续的订单追踪。
 */
OrderIDs UftStraContext::stra_buy(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	auto ids = _trader->buy(stdCode, price, qty, flag, false);  // 通过交易适配器买入

	// 将所有订单ID添加到订单映射表中
	for(uint32_t localid : ids)  // 遍历订单ID列表
	{
		_order_ids[localid] = NULL;  // 初始化订单结构指针为NULL，后续在on_order回调中创建
	}
	return std::move(ids);  // 移动返回订单ID列表
}

/**
 * @brief 卖出接口实现
 * @param stdCode 标准化合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 订单ID列表
 * 
 * 卖出指定合约，返回生成的订单ID列表。
 * 将订单ID添加到订单映射表中，用于后续的订单追踪。
 */
OrderIDs UftStraContext::stra_sell(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	auto ids = _trader->sell(stdCode, price, qty, flag, false);  // 通过交易适配器卖出
	// 将所有订单ID添加到订单映射表中
	for (uint32_t localid : ids)  // 遍历订单ID列表
	{
		_order_ids[localid] = NULL;  // 初始化订单结构指针为NULL，后续在on_order回调中创建
	}
	return std::move(ids);  // 移动返回订单ID列表
}

/**
 * @brief 开多接口实现
 * @param stdCode 标准化合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 订单ID
 * 
 * 开多仓，返回生成的订单ID。
 * 将订单ID添加到订单映射表中，用于后续的订单追踪。
 */
uint32_t UftStraContext::stra_enter_long(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	uint32_t localid = _trader->openLong(stdCode, price, qty, flag);  // 通过交易适配器开多仓
	_order_ids[localid] = NULL;  // 初始化订单结构指针为NULL，后续在on_order回调中创建
	return localid;  // 返回订单ID
}

/**
 * @brief 平多接口实现
 * @param stdCode 标准化合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param isToday 是否今仓，默认false
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 订单ID
 * 
 * 平多仓，返回生成的订单ID。
 * 将订单ID添加到订单映射表中，用于后续的订单追踪。
 */
uint32_t UftStraContext::stra_exit_long(const char* stdCode, double price, double qty, bool isToday /* = false */, int flag /* = 0 */)
{
	uint32_t localid = _trader->closeLong(stdCode, price, qty, isToday, flag);  // 通过交易适配器平多仓
	_order_ids[localid] = NULL;  // 初始化订单结构指针为NULL，后续在on_order回调中创建
	return localid;  // 返回订单ID
}

/**
 * @brief 开空接口实现
 * @param stdCode 标准化合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 订单ID
 * 
 * 开空仓，返回生成的订单ID。
 * 将订单ID添加到订单映射表中，用于后续的订单追踪。
 */
uint32_t UftStraContext::stra_enter_short(const char* stdCode, double price, double qty, int flag /* = 0 */)
{
	uint32_t localid = _trader->openShort(stdCode, price, qty, flag);  // 通过交易适配器开空仓
	_order_ids[localid] = NULL;  // 初始化订单结构指针为NULL，后续在on_order回调中创建
	return localid;  // 返回订单ID
}

/**
 * @brief 平空接口实现
 * @param stdCode 标准化合约代码
 * @param price 下单价格，0表示市价单
 * @param qty 下单数量
 * @param isToday 是否今仓，默认false
 * @param flag 下单标志：0-normal，1-fak，2-fok，默认0
 * @return 订单ID
 * 
 * 平空仓，返回生成的订单ID。
 * 将订单ID添加到订单映射表中，用于后续的订单追踪。
 */
uint32_t UftStraContext::stra_exit_short(const char* stdCode, double price, double qty, bool isToday /* = false */, int flag /* = 0 */)
{
	uint32_t localid = _trader->closeShort(stdCode, price, qty, isToday, flag);  // 通过交易适配器平空仓
	_order_ids[localid] = NULL;  // 初始化订单结构指针为NULL，后续在on_order回调中创建
	return localid;  // 返回订单ID
}

/**
 * @brief 获取商品信息实现
 * @param stdCode 标准化合约代码
 * @return 商品信息指针
 * 
 * 获取指定合约的商品信息。
 */
WTSCommodityInfo* UftStraContext::stra_get_comminfo(const char* stdCode)
{
	return _engine->get_commodity_info(stdCode);  // 通过引擎获取商品信息
}

/**
 * @brief 获取K线数据实现
 * @param stdCode 标准化合约代码
 * @param period 周期字符串（如"m1", "m5", "d1"等）
 * @param count 数据条数
 * @return K线数据切片指针
 * 
 * 获取指定合约的K线数据，如果成功获取则自动订阅该合约的tick数据。
 * 周期字符串格式：第一个字符表示周期类型（m-分钟，d-日等），后续数字表示倍数。
 */
WTSKlineSlice* UftStraContext::stra_get_bars(const char* stdCode, const char* period, uint32_t count)
{
	thread_local static char basePeriod[2] = { 0 };  // 线程局部静态变量，存储基础周期
	basePeriod[0] = period[0];  // 提取周期类型（第一个字符）
	uint32_t times = 1;  // 周期倍数，默认为1
	if (strlen(period) > 1)  // 如果周期字符串长度大于1，说明有倍数
		times = strtoul(period + 1, NULL, 10);  // 解析倍数（从第二个字符开始）

	WTSKlineSlice* ret = _engine->get_kline_slice(_context_id, stdCode, basePeriod, count, times);  // 通过引擎获取K线数据

	if (ret)  // 如果成功获取K线数据
		_engine->sub_tick(id(), stdCode);  // 自动订阅该合约的tick数据

	return ret;  // 返回K线数据切片指针
}

/**
 * @brief 获取Tick数据实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @return Tick数据切片指针
 * 
 * 获取指定合约的Tick数据，如果成功获取则自动订阅该合约的tick数据。
 */
WTSTickSlice* UftStraContext::stra_get_ticks(const char* stdCode, uint32_t count)
{
	WTSTickSlice* ticks = _engine->get_tick_slice(_context_id, stdCode, count);  // 通过引擎获取Tick数据

	if (ticks)  // 如果成功获取Tick数据
		_engine->sub_tick(id(), stdCode);  // 自动订阅该合约的tick数据
	return ticks;  // 返回Tick数据切片指针
}

/**
 * @brief 获取订单明细数据实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @return 订单明细数据切片指针
 * 
 * 获取指定合约的订单明细数据，如果成功获取则自动订阅该合约的订单明细数据。
 */
WTSOrdDtlSlice* UftStraContext::stra_get_order_detail(const char* stdCode, uint32_t count)
{
	WTSOrdDtlSlice* ret = _engine->get_order_detail_slice(_context_id, stdCode, count);  // 通过引擎获取订单明细数据

	if (ret)  // 如果成功获取订单明细数据
		_engine->sub_order_detail(id(), stdCode);  // 自动订阅该合约的订单明细数据
	return ret;  // 返回订单明细数据切片指针
}

/**
 * @brief 获取订单队列数据实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @return 订单队列数据切片指针
 * 
 * 获取指定合约的订单队列数据，如果成功获取则自动订阅该合约的订单队列数据。
 */
WTSOrdQueSlice* UftStraContext::stra_get_order_queue(const char* stdCode, uint32_t count)
{
	WTSOrdQueSlice* ret = _engine->get_order_queue_slice(_context_id, stdCode, count);  // 通过引擎获取订单队列数据

	if (ret)  // 如果成功获取订单队列数据
		_engine->sub_order_queue(id(), stdCode);  // 自动订阅该合约的订单队列数据
	return ret;  // 返回订单队列数据切片指针
}


/**
 * @brief 获取成交明细数据实现
 * @param stdCode 标准化合约代码
 * @param count 数据条数
 * @return 成交明细数据切片指针
 * 
 * 获取指定合约的成交明细数据，如果成功获取则自动订阅该合约的成交明细数据。
 */
WTSTransSlice* UftStraContext::stra_get_transaction(const char* stdCode, uint32_t count)
{
	WTSTransSlice* ret = _engine->get_transaction_slice(_context_id, stdCode, count);  // 通过引擎获取成交明细数据

	if (ret)  // 如果成功获取成交明细数据
		_engine->sub_transaction(id(), stdCode);  // 自动订阅该合约的成交明细数据
	return ret;  // 返回成交明细数据切片指针
}


/**
 * @brief 获取最新Tick数据实现
 * @param stdCode 标准化合约代码
 * @return 最新Tick数据指针
 * 
 * 获取指定合约的最新Tick数据。
 */
WTSTickData* UftStraContext::stra_get_last_tick(const char* stdCode)
{
	return _engine->get_last_tick(_context_id, stdCode);  // 通过引擎获取最新Tick数据
}

/**
 * @brief 订阅Tick数据实现
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的Tick数据，订阅后会自动收到该合约的Tick数据回调。
 */
void UftStraContext::stra_sub_ticks(const char* stdCode)
{
	_engine->sub_tick(id(), stdCode);  // 通过引擎订阅Tick数据
	log_info("Market Data subscribed: {}", stdCode);  // 记录订阅日志
}

/**
 * @brief 订阅订单明细数据实现
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的订单明细数据，订阅后会自动收到该合约的订单明细数据回调。
 */
void UftStraContext::stra_sub_order_details(const char* stdCode)
{
	_engine->sub_order_detail(id(), stdCode);  // 通过引擎订阅订单明细数据
	log_info("Order details subscribed: {}", stdCode);  // 记录订阅日志
}

/**
 * @brief 订阅订单队列数据实现
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的订单队列数据，订阅后会自动收到该合约的订单队列数据回调。
 */
void UftStraContext::stra_sub_order_queues(const char* stdCode)
{
	_engine->sub_order_queue(id(), stdCode);  // 通过引擎订阅订单队列数据
	log_info("Order queues subscribed: {}", stdCode);  // 记录订阅日志
}

/**
 * @brief 订阅成交明细数据实现
 * @param stdCode 标准化合约代码
 * 
 * 订阅指定合约的成交明细数据，订阅后会自动收到该合约的成交明细数据回调。
 */
void UftStraContext::stra_sub_transactions(const char* stdCode)
{
	_engine->sub_transaction(id(), stdCode);  // 通过引擎订阅成交明细数据
	log_info("Transactions subscribed: {}", stdCode);  // 记录订阅日志
}

/**
 * @brief 记录信息日志实现
 * @param message 日志消息
 * 
 * 记录信息级别的日志。
 */
void UftStraContext::stra_log_info(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_INFO, message);  // 通过日志工具记录信息日志
}

/**
 * @brief 记录调试日志实现
 * @param message 日志消息
 * 
 * 记录调试级别的日志。
 */
void UftStraContext::stra_log_debug(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_DEBUG, message);  // 通过日志工具记录调试日志
}

/**
 * @brief 记录错误日志实现
 * @param message 日志消息
 * 
 * 记录错误级别的日志。
 */
void UftStraContext::stra_log_error(const char* message)
{
	WTSLogger::log_dyn_raw("strategy", _name.c_str(), LL_ERROR, message);  // 通过日志工具记录错误日志
}

/**
 * @brief 加载本地数据实现
 * 
 * 从内存映射文件中加载本地持仓、订单、成交、回合等数据。
 * 
 * 加载流程：
 * 1. 检查交易日是否有效，如果无效则直接返回
 * 2. 创建策略数据目录（如果不存在）
 * 3. 优先加载手动持仓文件（mannual.yaml），如果存在则覆盖现有持仓
 * 4. 加载持仓数据块（position.membin），恢复持仓明细到内存
 * 5. 加载订单数据块（order.membin），恢复未完成订单
 * 6. 加载成交数据块（trade.membin），成交数据不读取到内存（仅用于持久化）
 * 7. 加载回合数据块（round.membin），回合数据不读取到内存（仅用于持久化）
 * 
 * 交易日切换处理：
 * - 如果数据块的交易日与当前交易日不一致，会清理旧数据
 * - 持仓数据会保留未完成的持仓明细，重置平仓盈亏
 * - 订单、成交、回合数据会清空
 * 
 * 手动持仓导入：
 * - 支持从YAML文件手动导入持仓，用于特殊场景下的持仓恢复
 * - 文件格式：包含details数组，每个元素包含exchg、code、direct、volume、openprice等字段
 * - 导入后会将文件重命名，避免重复导入
 */
void UftStraContext::load_local_data()
{
	if (_tradingday == 0)  // 如果交易日无效，直接返回
		return;

	std::string folder = fmtutil::format("{}{}/", WtHelper::getOutputDir(), _name);  // 构建策略数据目录路径
	if (!StdFile::exists(folder.c_str()))  // 如果目录不存在
		BoostFile::create_directories(folder.c_str());  // 创建目录

	/*
	 *	By Wesley @ 2023.09.08
	 *	这里增加一个逻辑，从yaml文件读取手动生成的持仓
	 */
	// 优先处理手动持仓文件（mannual.yaml）
	std::string mannualfile = folder + "mannual.yaml";  // 手动持仓文件路径
	do  // 使用do-while(false)实现一次性流程控制
	{
		if (!StdFile::exists(mannualfile.c_str()))  // 如果文件不存在，跳过处理
			break;

		WTSLogger::log_dyn("strategy", _name.c_str(), LL_WARN, "{} detected, positions will be overwrited", mannualfile);  // 记录警告日志

		WTSVariant* manual = WTSCfgLoader::load_from_file(mannualfile);  // 从YAML文件加载配置
		if (manual == NULL)  // 如果加载失败
		{
			WTSLogger::log_dyn("strategy", _name.c_str(), LL_ERROR, "parsing mannual file {} failed", mannualfile);  // 记录错误日志
			break;
		}

		WTSVariant* ayDetails = manual->get("details");  // 获取持仓明细数组
		if(ayDetails == NULL)  // 如果不存在明细数组，跳过处理
			break;

		// 解析成功，开始处理持仓
		// 持仓明细字段说明：
		//char		_exchg[MAX_EXCHANGE_LENGTH];  // 交易所代码
		//char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码
		//uint32_t	_direct;	//方向0-多，1-空
		//double		_volume;  // 持仓数量
		//double		_open_price;  // 开仓价格

		SpinLock lock(_pos_blk._mutex);  // 加锁保护持仓数据块
		std::string filename = folder + "position.membin";  // 持仓数据文件路径

		// 强制新建持仓数据文件
		{
			std::size_t uSize = sizeof(uft::PositionBlock) + sizeof(uft::DetailStruct) * DATA_SIZE_STEP;  // 计算文件大小
			BoostFile bf;  // 创建文件对象
			bf.create_new_file(filename.c_str());  // 创建新文件
			bf.truncate_file(uSize);  // 设置文件大小
			bf.close_file();  // 关闭文件
		}

		_pos_blk._file.reset(new BoostMappingFile);  // 创建内存映射文件对象
		if (_pos_blk._file->map(filename.c_str()))  // 如果映射成功
		{
			_pos_blk._block = (uft::PositionBlock*)_pos_blk._file->addr();  // 获取数据块指针
			strcpy(_pos_blk._block->_blk_flag, uft::BLK_FLAG);  // 设置数据块标志
			_pos_blk._block->_date = _tradingday;  // 设置交易日
			_pos_blk._block->_capacity = DATA_SIZE_STEP;  // 设置容量

			// 遍历YAML文件中的持仓明细，写入数据块
			for (uint32_t i = 0; i < ayDetails->size(); i++)
			{
				WTSVariant* objDetail = ayDetails->get(i);  // 获取持仓明细对象
				const char* exchg = objDetail->getCString("exchg");  // 获取交易所代码
				const char* code = objDetail->getCString("code");  // 获取合约代码
				WTSContractInfo* cInfo = _engine->get_basedata_mgr()->getContract(code, exchg);  // 获取合约信息
				if(cInfo == NULL)  // 如果合约不存在
				{
					WTSLogger::log_dyn("strategy", _name.c_str(), LL_ERROR, "{}.{} not exist, skip this details", exchg, code);  // 记录错误日志
					continue;  // 跳过该明细
				}

				uft::DetailStruct& ds = _pos_blk._block->_details[i];  // 获取持仓明细结构引用
				wt_strcpy(ds._exchg, exchg);  // 复制交易所代码
				wt_strcpy(ds._code, code);  // 复制合约代码
				ds._direct = objDetail->getUInt32("direct");  // 设置方向（0-多，1-空）
				ds._volume = objDetail->getDouble("volume");  // 设置持仓数量
				ds._open_price = objDetail->getDouble("openprice");  // 设置开仓价格
				ds._open_time = TimeUtils::getLocalTimeNow();  // 设置开仓时间
				ds._open_tdate = _tradingday;  // 设置开仓交易日

				_pos_blk._block->_size++;  // 增加持仓明细数量
			}
		}

		WTSLogger::log_dyn("strategy", _name.c_str(), LL_WARN, "loading mannual file {} done, {} details imported", mannualfile, _pos_blk._block->_size);  // 记录导入完成日志

		// 把mmap释放掉，不影响后面的逻辑
		{
			_pos_blk._file.reset();  // 释放内存映射文件
			_pos_blk._block = NULL;  // 清空数据块指针
		}
	} while (false);

	// 不管前面解析的情况如何，文件都重命名（避免重复导入）
	if (StdFile::exists(mannualfile.c_str()))  // 如果手动持仓文件存在
		boost::filesystem::rename(boost::filesystem::path(mannualfile), boost::filesystem::path(fmtutil::format("{}.{}", mannualfile, TimeUtils::getYYYYMMDDhhmmss())));  // 重命名文件（添加时间戳）

	// 加载持仓数据块
	if(_pos_blk._block == NULL || _pos_blk._block->_date != _tradingday)  // 如果数据块未加载或交易日不一致
	{
		SpinLock lock(_pos_blk._mutex);  // 加锁保护持仓数据块
		std::string filename = folder + "position.membin";  // 持仓数据文件路径
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "loading local positions from {}", filename);  // 记录加载日志
		bool isNew = false;  // 标记是否为新文件
		if(!StdFile::exists(filename.c_str()))  // 如果文件不存在
		{
			std::size_t uSize = sizeof(uft::PositionBlock) + sizeof(uft::DetailStruct) * DATA_SIZE_STEP;  // 计算文件大小
			BoostFile bf;  // 创建文件对象
			bf.create_new_file(filename.c_str());  // 创建新文件
			bf.truncate_file(uSize);  // 设置文件大小
			bf.close_file();  // 关闭文件

			isNew = true;  // 标记为新文件
		}

		_pos_blk._file.reset(new BoostMappingFile);  // 创建内存映射文件对象
		if (_pos_blk._file->map(filename.c_str()))  // 如果映射成功
		{
			_pos_blk._block = (uft::PositionBlock*)_pos_blk._file->addr();  // 获取数据块指针
			if(isNew)  // 如果是新文件
			{
				strcpy(_pos_blk._block->_blk_flag, uft::BLK_FLAG);  // 设置数据块标志
				_pos_blk._block->_date = _tradingday;  // 设置交易日
				_pos_blk._block->_capacity = DATA_SIZE_STEP;  // 设置容量
			}

			// 复用原文件的好处就是，mmap文件大小会满足历史出现过的单日最高数据量，以后再扩的概率就很低了
			if(_pos_blk._block->_date != 0 && _pos_blk._block->_date != _tradingday)  // 如果交易日不一致
			{	
				WTSLogger::log_dyn("strategy", _name.c_str(), LL_INFO, "Clearing local position of {}", _pos_blk._block->_date);  // 记录清理日志
				// 如果日期不同，先读进来未完成的持仓，再清理掉原始数据
				std::vector<uft::DetailStruct> details;  // 创建临时持仓明细向量
				for(uint32_t i = 0; i < _pos_blk._block->_size; i++)  // 遍历所有持仓明细
				{
					uft::DetailStruct& ds = _pos_blk._block->_details[i];  // 获取持仓明细引用
					ds._closed_profit = 0;	// 交易日切换以后，平仓盈亏置为0
					if(decimal::eq(ds._volume, 0))  // 如果持仓数量为0，跳过
						continue;

					WTSContractInfo* cInfo = _engine->get_basedata_mgr()->getContract(ds._code, ds._exchg);  // 获取合约信息
					if (cInfo == NULL)  // 如果合约不存在，跳过
						continue;

					details.emplace_back(ds);  // 保存未完成的持仓明细
				}

				memset(_pos_blk._block->_details, 0, sizeof(uft::DetailStruct)*_pos_blk._block->_size);  // 清空原数据

				if (!details.empty())  // 如果有未完成的持仓
					memcpy(_pos_blk._block->_details, details.data(), sizeof(uft::DetailStruct)*details.size());  // 复制未完成的持仓明细
				_pos_blk._block->_size = details.size();  // 更新持仓明细数量
				_pos_blk._block->_date = _tradingday;  // 更新交易日
				
			}

			{
				// 把剩余数量不为0的持仓读进来，恢复到内存中的持仓映射表
				for (uint32_t i = 0; i < _pos_blk._block->_size; i++)  // 遍历所有持仓明细
				{
					uft::DetailStruct& ds = _pos_blk._block->_details[i];  // 获取持仓明细引用

					WTSContractInfo* cInfo = _engine->get_basedata_mgr()->getContract(ds._code, ds._exchg);  // 获取合约信息
					if (cInfo == NULL)  // 如果合约不存在，跳过
						continue;

					PosInfo& posInfo = _positions[cInfo->getFullCode()];  // 获取或创建持仓信息
					posInfo._total_profit += ds._closed_profit;  // 累加平仓盈亏

					if (decimal::eq(ds._volume, 0))  // 如果持仓数量为0，跳过
						continue;

					posInfo._dynprofit += ds._position_profit;  // 累加持仓盈亏
					posInfo._opencost += ds._volume*ds._open_price*cInfo->getCommInfo()->getVolScale();  // 累加开仓成本
					posInfo._volume += ds._volume*(ds._direct == 0 ? 1 : -1);  // 累加持仓数量（多仓为正，空仓为负）

					posInfo._details.emplace_back(&ds);  // 将持仓明细添加到列表
				}
			}
		}
		else  // 如果映射失败
		{
			_pos_blk._file.reset();  // 释放内存映射文件
			_pos_blk._block = NULL;  // 清空数据块指针
		}
	}

	// 加载订单数据块
	if (_ord_blk._block == NULL || _ord_blk._block->_date != _tradingday)  // 如果数据块未加载或交易日不一致
	{
		SpinLock lock(_ord_blk._mutex);  // 加锁保护订单数据块
		std::string filename = folder + "order.membin";  // 订单数据文件路径
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "loading local orders from {}", filename);  // 记录加载日志
		bool isNew = false;  // 标记是否为新文件
		if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
		{
			std::size_t uSize = sizeof(uft::OrderBlock) + sizeof(uft::OrderStruct) * DATA_SIZE_STEP;  // 计算文件大小
			BoostFile bf;  // 创建文件对象
			bf.create_new_file(filename.c_str());  // 创建新文件
			bf.truncate_file(uSize);  // 设置文件大小
			bf.close_file();  // 关闭文件

			isNew = true;  // 标记为新文件
		}

		_ord_blk._file.reset(new BoostMappingFile);  // 创建内存映射文件对象
		if (_ord_blk._file->map(filename.c_str()))  // 如果映射成功
		{
			_ord_blk._block = (uft::OrderBlock*)_ord_blk._file->addr();  // 获取数据块指针
			if (isNew)  // 如果是新文件
			{
				strcpy(_ord_blk._block->_blk_flag, uft::BLK_FLAG);  // 设置数据块标志
				_ord_blk._block->_date = _tradingday;  // 设置交易日
				_ord_blk._block->_capacity = DATA_SIZE_STEP;  // 设置容量
			}

			// 交易日不一致就把数据清掉
			// 复用原文件的好处就是，mmap文件大小会满足历史出现过的单日最高数据量，以后再扩的概率就很低了
			if (_ord_blk._block->_date != 0 && _ord_blk._block->_date != _tradingday)  // 如果交易日不一致
			{
				memset(_ord_blk._block->_orders, 0, sizeof(uft::OrderStruct)*_ord_blk._block->_size);  // 清空订单数据
				_ord_blk._block->_size = 0;  // 重置订单数量
				_ord_blk._block->_date = _tradingday;  // 更新交易日
			}
			else  // 如果交易日一致
			{
				// 把未完成单读到内存里来（当前未实现，可能是历史遗留）
			}
		}
		else  // 如果映射失败
		{
			_ord_blk._file.reset();  // 释放内存映射文件
			_ord_blk._block = NULL;  // 清空数据块指针
		}
	}

	// 加载成交数据块
	if (_trd_blk._block == NULL || _trd_blk._block->_date != _tradingday)  // 如果数据块未加载或交易日不一致
	{
		SpinLock lock(_trd_blk._mutex);  // 加锁保护成交数据块
		std::string filename = folder + "trade.membin";  // 成交数据文件路径
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "loading local trades from {}", filename);  // 记录加载日志
		bool isNew = false;  // 标记是否为新文件
		if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
		{
			std::size_t uSize = sizeof(uft::TradeBlock) + sizeof(uft::TradeStruct) * DATA_SIZE_STEP;  // 计算文件大小
			BoostFile bf;  // 创建文件对象
			bf.create_new_file(filename.c_str());  // 创建新文件
			bf.truncate_file(uSize);  // 设置文件大小
			bf.close_file();  // 关闭文件

			isNew = true;  // 标记为新文件
		}

		_trd_blk._file.reset(new BoostMappingFile);  // 创建内存映射文件对象
		if (_trd_blk._file->map(filename.c_str()))  // 如果映射成功
		{
			_trd_blk._block = (uft::TradeBlock*)_trd_blk._file->addr();  // 获取数据块指针
			if (isNew)  // 如果是新文件
			{
				strcpy(_trd_blk._block->_blk_flag, uft::BLK_FLAG);  // 设置数据块标志
				_trd_blk._block->_date = _tradingday;  // 设置交易日
				_trd_blk._block->_capacity = DATA_SIZE_STEP;  // 设置容量
			}

			// 交易日不一致就把数据清掉
			// 复用原文件的好处就是，mmap文件大小会满足历史出现过的单日最高数据量，以后再扩的概率就很低了
			if (_trd_blk._block->_date != 0 && _trd_blk._block->_date != _tradingday)  // 如果交易日不一致
			{
				memset(_trd_blk._block->_trades, 0, sizeof(uft::TradeStruct)*_trd_blk._block->_size);  // 清空成交数据
				_trd_blk._block->_size = 0;  // 重置成交数量
				_trd_blk._block->_date = _tradingday;  // 更新交易日
			}
			else  // 如果交易日一致
			{
				// 成交数据不用读进来了（仅用于持久化，不加载到内存）
			}			
		}
		else  // 如果映射失败
		{
			_trd_blk._file.reset();  // 释放内存映射文件
			_trd_blk._block = NULL;  // 清空数据块指针
		}
	}

	// 加载回合数据块
	if (_rnd_blk._block == NULL || _rnd_blk._block->_date != _tradingday)  // 如果数据块未加载或交易日不一致
	{
		SpinLock lock(_rnd_blk._mutex);  // 加锁保护回合数据块
		std::string filename = folder + "round.membin";  // 回合数据文件路径
		WTSLogger::log_dyn("strategy", _name.c_str(), LL_DEBUG, "loading local rouds from {}", filename);  // 记录加载日志
		bool isNew = false;  // 标记是否为新文件
		if (!StdFile::exists(filename.c_str()))  // 如果文件不存在
		{
			std::size_t uSize = sizeof(uft::RoundBlock) + sizeof(uft::RoundStruct) * DATA_SIZE_STEP;  // 计算文件大小
			BoostFile bf;  // 创建文件对象
			bf.create_new_file(filename.c_str());  // 创建新文件
			bf.truncate_file(uSize);  // 设置文件大小
			bf.close_file();  // 关闭文件

			isNew = true;  // 标记为新文件
		}

		_rnd_blk._file.reset(new BoostMappingFile);  // 创建内存映射文件对象
		if (_rnd_blk._file->map(filename.c_str()))  // 如果映射成功
		{
			_rnd_blk._block = (uft::RoundBlock*)_rnd_blk._file->addr();  // 获取数据块指针
			if (isNew)  // 如果是新文件
			{
				strcpy(_rnd_blk._block->_blk_flag, uft::BLK_FLAG);  // 设置数据块标志
				_rnd_blk._block->_date = _tradingday;  // 设置交易日
				_rnd_blk._block->_capacity = DATA_SIZE_STEP;  // 设置容量
			}

			// 交易日不一致就把数据清掉
			// 复用原文件的好处就是，mmap文件大小会满足历史出现过的单日最高数据量，以后再扩的概率就很低了
			if (_rnd_blk._block->_date != 0 && _rnd_blk._block->_date != _tradingday)  // 如果交易日不一致
			{
				memset(_rnd_blk._block->_rounds, 0, sizeof(uft::RoundStruct)*_rnd_blk._block->_size);  // 清空回合数据
				_rnd_blk._block->_size = 0;  // 重置回合数量
				_rnd_blk._block->_date = _tradingday;  // 更新交易日
			}
			else  // 如果交易日一致
			{
				// 回合数据不用读到进来了（仅用于持久化，不加载到内存）
			}
		}
		else  // 如果映射失败
		{
			_rnd_blk._file.reset();  // 释放内存映射文件
			_rnd_blk._block = NULL;  // 清空数据块指针
		}
	}
}