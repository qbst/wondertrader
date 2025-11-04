/*!
 * \file WtHftEngine.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高频交易引擎实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtHftEngine类的所有方法，提供了高频交易引擎的核心功能。
 * 主要实现包括：
 * 1. 引擎初始化：初始化引擎配置和基础组件
 * 2. 引擎运行：启动策略上下文，创建实时行情时钟，生成标记文件
 * 3. 行情分发：处理Tick行情、委托队列、委托明细、成交明细的分发逻辑
 * 4. 复权处理：支持前复权和后复权的行情数据处理和价格修正
 * 5. 订阅管理：管理策略对Level2数据的订阅关系
 * 6. 数据查询：提供Level2历史数据查询接口实现
 * 7. 生命周期管理：处理交易日开始和结束事件
 * 
 * 实现细节：
 * - 使用订阅映射表管理策略对合约的订阅关系
 * - Level2数据不进行复权处理，直接使用原始代码
 * - 支持策略的多合约订阅和自动分发
 * - 使用复权因子修正后复权价格数据
 */
#define WIN32_LEAN_AND_MEAN  // Windows平台：排除不常用的Windows API，加快编译速度

#include "WtHftEngine.h"  // 包含HFT引擎头文件
#include "WtHftTicker.h"  // 包含HFT实时行情时钟头文件
#include "WtDtMgr.h"  // 包含数据管理器头文件
#include "TraderAdapter.h"  // 包含交易适配器头文件
#include "WtHelper.h"  // 包含辅助工具类头文件

#include "../Share/decimal.h"  // 包含高精度小数运算工具
#include "../Share/CodeHelper.hpp"  // 包含合约代码解析辅助工具

#include "../Includes/WTSVariant.hpp"  // 包含变体类型定义
#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息定义

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

#include <rapidjson/document.h>  // 包含RapidJSON文档类
#include <rapidjson/prettywriter.h>  // 包含RapidJSON格式化写入器
namespace rj = rapidjson;  // RapidJSON命名空间别名

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @brief 构造函数
 * 
 * 初始化HFT引擎，将配置对象指针和时间时钟指针设置为NULL。
 */
WtHftEngine::WtHftEngine()
	: _cfg(NULL)  // 初始化配置对象指针为NULL
	, _tm_ticker(NULL)  // 初始化时间时钟指针为NULL
{
}


/**
 * @brief 析构函数
 * 
 * 清理资源，停止时间时钟并释放配置对象。
 */
WtHftEngine::~WtHftEngine()
{
	if (_tm_ticker)  // 如果时间时钟指针有效
	{
		_tm_ticker->stop();  // 停止时间时钟
		delete _tm_ticker;  // 释放时间时钟对象
		_tm_ticker = NULL;  // 将指针置为NULL
	}

	if (_cfg)  // 如果配置对象指针有效
		_cfg->release();  // 释放配置对象内存
}

/**
 * @brief 初始化引擎
 * @param cfg 配置对象指针
 * @param bdMgr 基础数据管理器指针
 * @param dataMgr 数据管理器指针
 * @param hotMgr 热点合约管理器指针
 * @param notifier 事件通知器指针
 * 
 * 初始化HFT引擎，调用基类初始化方法，并保存配置对象引用。
 */
void WtHftEngine::init(WTSVariant* cfg, IBaseDataMgr* bdMgr, WtDtMgr* dataMgr, IHotMgr* hotMgr, EventNotifier* notifier /* = NULL */)
{
	WtEngine::init(cfg, bdMgr, dataMgr, hotMgr, notifier);  // 调用基类初始化方法

	_cfg = cfg;  // 保存配置对象指针
	_cfg->retain();  // 增加配置对象引用计数
}

/**
 * @brief 运行引擎
 * 
 * 启动HFT引擎，执行以下操作：
 * 1. 初始化所有策略上下文
 * 2. 创建并初始化实时行情时钟
 * 3. 生成策略和通道标记文件（marker.json）
 * 4. 启动实时行情时钟
 */
void WtHftEngine::run()
{
	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)  // 遍历所有策略上下文
	{
		HftContextPtr& ctx = (HftContextPtr&)it->second;  // 获取策略上下文引用
		ctx->on_init();  // 调用策略初始化方法
	}

	_tm_ticker = new WtHftRtTicker(this);  // 创建实时行情时钟对象
	WTSVariant* cfgProd = _cfg->get("product");  // 获取产品配置节点
	_tm_ticker->init(_data_mgr->reader(), cfgProd->getCString("session"));  // 初始化时间时钟，传入数据读取器和会话ID

	//启动之前,先把运行中的策略落地
	{
		rj::Document root(rj::kObjectType);  // 创建JSON根对象
		rj::Document::AllocatorType &allocator = root.GetAllocator();  // 获取内存分配器引用

		rj::Value jStraList(rj::kArrayType);  // 创建策略列表JSON数组
		for (auto& m : _ctx_map)  // 遍历所有策略上下文
		{
			const HftContextPtr& ctx = m.second;  // 获取策略上下文常量引用
			jStraList.PushBack(rj::Value(ctx->name(), allocator), allocator);  // 将策略名称添加到JSON数组
		}

		root.AddMember("marks", jStraList, allocator);  // 将策略列表添加到根对象，键名为"marks"

		rj::Value jChnlList(rj::kArrayType);  // 创建通道列表JSON数组
		for (auto& m : _adapter_mgr->getAdapters())  // 遍历所有交易适配器
		{
			const TraderAdapterPtr& adapter = m.second;  // 获取交易适配器常量引用
			jChnlList.PushBack(rj::Value(adapter->id(), allocator), allocator);  // 将适配器ID添加到JSON数组
		}

		root.AddMember("channels", jChnlList, allocator);  // 将通道列表添加到根对象，键名为"channels"

		root.AddMember("engine", rj::Value("HFT", allocator), allocator);  // 添加引擎类型标记，值为"HFT"

		std::string filename = WtHelper::getBaseDir();  // 获取基础目录路径
		filename += "marker.json";  // 拼接标记文件名

		rj::StringBuffer sb;  // 创建字符串缓冲区
		rj::PrettyWriter<rj::StringBuffer> writer(sb);  // 创建格式化写入器
		root.Accept(writer);  // 将JSON对象写入缓冲区
		StdFile::write_file_content(filename.c_str(), sb.GetString());  // 将JSON内容写入文件
	}

	_tm_ticker->run();  // 启动实时行情时钟
}

/**
 * @brief 处理推送的行情数据
 * @param newTick 新的Tick数据指针
 * 
 * 接收外部推送的实时行情数据，如果时间时钟已创建则转发给它处理。
 */
void WtHftEngine::handle_push_quote(WTSTickData* newTick)
{
	if (_tm_ticker)  // 如果时间时钟指针有效
		_tm_ticker->on_tick(newTick);  // 将行情数据转发给时间时钟处理
}

/**
 * @brief 处理推送的委托明细数据
 * @param curOrdDtl 委托明细数据指针
 * 
 * 接收外部推送的委托明细数据，查找订阅了该合约的策略，并分发数据。
 * Level2数据不进行复权处理，直接使用原始合约代码。
 */
void WtHftEngine::handle_push_order_detail(WTSOrdDtlData* curOrdDtl)
{
	const char* stdCode = curOrdDtl->code();  // 获取合约代码
	auto sit = _orddtl_sub_map.find(stdCode);  // 在委托明细订阅表中查找该合约
	if (sit != _orddtl_sub_map.end())  // 如果找到订阅记录
	{
		const SubList& sids = sit->second;  // 获取订阅该合约的策略ID列表
		for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
		{
			//By Wesley @ 2022.02.07
			//Level2数据一般用于HFT场景，所以不做复权处理
			//所以不读取订阅标记
			uint32_t sid = it->first;  // 获取策略ID
			auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
			if (cit != _ctx_map.end())  // 如果找到策略上下文
			{
				HftContextPtr& ctx = (HftContextPtr&)cit->second;  // 获取策略上下文引用
				ctx->on_order_detail(stdCode, curOrdDtl);  // 调用策略的委托明细回调方法
			}
		}
	}
}

/**
 * @brief 处理推送的委托队列数据
 * @param curOrdQue 委托队列数据指针
 * 
 * 接收外部推送的委托队列数据，查找订阅了该合约的策略，并分发数据。
 * Level2数据不进行复权处理，直接使用原始合约代码。
 */
void WtHftEngine::handle_push_order_queue(WTSOrdQueData* curOrdQue)
{
	const char* stdCode = curOrdQue->code();  // 获取合约代码
	auto sit = _ordque_sub_map.find(stdCode);  // 在委托队列订阅表中查找该合约
	if (sit != _ordque_sub_map.end())  // 如果找到订阅记录
	{
		const SubList& sids = sit->second;  // 获取订阅该合约的策略ID列表
		for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
		{
			//By Wesley @ 2022.02.07
			//Level2数据一般用于HFT场景，所以不做复权处理
			//所以不读取订阅标记
			uint32_t sid = it->first;  // 获取策略ID
			auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
			if (cit != _ctx_map.end())  // 如果找到策略上下文
			{
				HftContextPtr& ctx = (HftContextPtr&)cit->second;  // 获取策略上下文引用
				ctx->on_order_queue(stdCode, curOrdQue);  // 调用策略的委托队列回调方法
			}
		}
	}
}

/**
 * @brief 处理推送的成交明细数据
 * @param curTrans 成交明细数据指针
 * 
 * 接收外部推送的成交明细数据，查找订阅了该合约的策略，并分发数据。
 * Level2数据不进行复权处理，直接使用原始合约代码。
 */
void WtHftEngine::handle_push_transaction(WTSTransData* curTrans)
{
	const char* stdCode = curTrans->code();  // 获取合约代码
	auto sit = _trans_sub_map.find(stdCode);  // 在成交明细订阅表中查找该合约
	if (sit != _trans_sub_map.end())  // 如果找到订阅记录
	{
		const SubList& sids = sit->second;  // 获取订阅该合约的策略ID列表
		for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
		{
			//By Wesley @ 2022.02.07
			//Level2数据一般用于HFT场景，所以不做复权处理
			//所以不读取订阅标记
			uint32_t sid = it->first;  // 获取策略ID
			auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
			if (cit != _ctx_map.end())  // 如果找到策略上下文
			{
				HftContextPtr& ctx = (HftContextPtr&)cit->second;  // 获取策略上下文引用
				ctx->on_transaction(stdCode, curTrans);  // 调用策略的成交明细回调方法
			}
		}
	}
}

/**
 * @brief 订阅委托明细数据
 * @param sid 策略ID
 * @param stdCode 标准合约代码
 * 
 * 为指定策略订阅指定合约的委托明细数据。
 * 如果代码包含复权后缀（'-'或'+'），会自动去除后缀进行订阅。
 */
void WtHftEngine::sub_order_detail(uint32_t sid, const char* stdCode)
{
	std::size_t length = strlen(stdCode);  // 获取合约代码长度
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权后缀（'-'或'+'）
		length--;  // 长度减1，去除复权后缀

	SubList& sids = _orddtl_sub_map[std::string(stdCode, length)];  // 在委托明细订阅表中获取或创建订阅列表（使用去除后缀的代码）
	sids[sid] = std::make_pair(sid, 0);  // 将策略ID添加到订阅列表，订阅标记为0（无复权）
}

/**
 * @brief 订阅委托队列数据
 * @param sid 策略ID
 * @param stdCode 标准合约代码
 * 
 * 为指定策略订阅指定合约的委托队列数据。
 * 如果代码包含复权后缀（'-'或'+'），会自动去除后缀进行订阅。
 */
void WtHftEngine::sub_order_queue(uint32_t sid, const char* stdCode)
{
	std::size_t length = strlen(stdCode);  // 获取合约代码长度
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权后缀（'-'或'+'）
		length--;  // 长度减1，去除复权后缀

	SubList& sids = _ordque_sub_map[std::string(stdCode, length)];  // 在委托队列订阅表中获取或创建订阅列表（使用去除后缀的代码）
	sids[sid] = std::make_pair(sid, 0);  // 将策略ID添加到订阅列表，订阅标记为0（无复权）
}

/**
 * @brief 订阅成交明细数据
 * @param sid 策略ID
 * @param stdCode 标准合约代码
 * 
 * 为指定策略订阅指定合约的成交明细数据。
 * 如果代码包含复权后缀（'-'或'+'），会自动去除后缀进行订阅。
 */
void WtHftEngine::sub_transaction(uint32_t sid, const char* stdCode)
{
	std::size_t length = strlen(stdCode);  // 获取合约代码长度
	if (stdCode[length - 1] == SUFFIX_QFQ || stdCode[length - 1] == SUFFIX_HFQ)  // 如果最后一个字符是复权后缀（'-'或'+'）
		length--;  // 长度减1，去除复权后缀

	SubList& sids = _trans_sub_map[std::string(stdCode, length)];  // 在成交明细订阅表中获取或创建订阅列表（使用去除后缀的代码）
	sids[sid] = std::make_pair(sid, 0);  // 将策略ID添加到订阅列表，订阅标记为0（无复权）
}

/**
 * @brief Tick行情回调
 * @param stdCode 标准合约代码
 * @param curTick 当前Tick数据指针
 * 
 * 处理Tick行情数据，执行以下操作：
 * 1. 调用基类的on_tick方法处理基础逻辑
 * 2. 将行情数据推送给数据管理器
 * 3. 根据订阅标记进行复权处理：
 *    - 标记为0：无复权，直接使用原始代码和价格
 *    - 标记为1：前复权，使用代码+'-'后缀，价格不变
 *    - 标记为2：后复权，使用代码+'+'后缀，价格乘以复权因子
 * 4. 分发给订阅的策略
 */
void WtHftEngine::on_tick(const char* stdCode, WTSTickData* curTick)
{
	WtEngine::on_tick(stdCode, curTick);  // 调用基类的on_tick方法处理基础逻辑

	_data_mgr->handle_push_quote(stdCode, curTick);  // 将行情数据推送给数据管理器

	/*
	 *	By Wesley @ 2022.02.07
	 *	这里做了一个彻底的调整
	 *	第一，检查订阅标记，如果标记为0，即无复权模式，则直接按照原始代码触发ontick
	 *	第二，如果标记为1，即前复权模式，则将代码转成xxxx-，再触发ontick
	 *	第三，如果标记为2，即后复权模式，则将代码转成xxxx+，再把tick数据做一个修正，再触发ontick
	 */
	if(_ready)  // 如果引擎已就绪
	{
		auto sit = _tick_sub_map.find(stdCode);  // 在Tick订阅表中查找该合约
		if (sit != _tick_sub_map.end())  // 如果找到订阅记录
		{
			const SubList& sids = sit->second;  // 获取订阅该合约的策略ID列表
			for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
			{
				uint32_t sid = it->first;  // 获取策略ID

				auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
				if (cit != _ctx_map.end())  // 如果找到策略上下文
				{
					HftContextPtr& ctx = (HftContextPtr&)cit->second;  // 获取策略上下文引用
					uint32_t opt = it->second.second;  // 获取订阅标记（0=无复权，1=前复权，2=后复权）

					if (opt == 0)  // 如果标记为0（无复权）
					{
						ctx->on_tick(stdCode, curTick);  // 直接使用原始代码和价格触发策略回调
					}
					else  // 如果标记为1或2（前复权或后复权）
					{
						std::string wCode = stdCode;  // 创建代码字符串副本
						wCode = fmt::format("{}{}", stdCode, opt == 1 ? SUFFIX_QFQ : SUFFIX_HFQ);  // 添加复权后缀（'-'或'+'）
						if (opt == 1)  // 如果标记为1（前复权）
						{
							ctx->on_tick(wCode.c_str(), curTick);  // 使用复权代码和原始价格触发策略回调
						}
						else //(opt == 2)  // 如果标记为2（后复权）
						{
							WTSTickData* newTick = WTSTickData::create(curTick->getTickStruct());  // 创建新的Tick数据对象
							WTSTickStruct& newTS = newTick->getTickStruct();  // 获取新的Tick结构体引用
							newTick->setContractInfo(curTick->getContractInfo());  // 设置合约信息

							//这里做一个复权因子的处理
							double factor = get_exright_factor(stdCode, curTick->getContractInfo()->getCommInfo());  // 获取复权因子
							newTS.open *= factor;  // 开盘价乘以复权因子
							newTS.high *= factor;  // 最高价乘以复权因子
							newTS.low *= factor;  // 最低价乘以复权因子
							newTS.price *= factor;  // 最新价乘以复权因子

							_price_map[wCode] = newTS.price;  // 将修正后的价格存入价格映射表

							ctx->on_tick(wCode.c_str(), newTick);  // 使用复权代码和修正后的价格触发策略回调
							newTick->release();  // 释放临时创建的Tick数据对象
						}
					}
				}
			}
		}
	}
}

/**
 * @brief K线数据回调
 * @param stdCode 标准合约代码
 * @param period K线周期
 * @param times 周期倍数
 * @param newBar 新的K线数据指针
 * 
 * 处理K线数据闭合事件，构建订阅键（合约代码-周期-倍数），查找订阅了该K线的策略并分发数据。
 */
void WtHftEngine::on_bar(const char* stdCode, const char* period, uint32_t times, WTSBarStruct* newBar)
{
	thread_local static char key[64] = { 0 };  // 线程局部静态字符数组，用于存储订阅键
	fmtutil::format_to(key, "{}-{}-{}", stdCode, period, times);  // 格式化订阅键：合约代码-周期-倍数

	const SubList& sids = _bar_sub_map[key];  // 在K线订阅表中查找该键的订阅列表
	for (auto it = sids.begin(); it != sids.end(); it++)  // 遍历订阅的策略ID列表
	{
		uint32_t sid = it->first;  // 获取策略ID
		auto cit = _ctx_map.find(sid);  // 在策略上下文映射表中查找策略
		if (cit != _ctx_map.end())  // 如果找到策略上下文
		{
			HftContextPtr& ctx = (HftContextPtr&)cit->second;  // 获取策略上下文引用
			ctx->on_bar(stdCode, period, times, newBar);  // 调用策略的K线回调方法
		}
	}
}

/**
 * @brief 交易日开始回调
 * 
 * 处理交易日开始事件，记录日志，调用基类方法，通知所有策略，并设置引擎就绪标志。
 */
void WtHftEngine::on_session_begin()
{
	WTSLogger::info("Trading day {} begun", _cur_tdate);  // 记录交易日开始日志
	WtEngine::on_session_begin();  // 调用基类的交易日开始方法

	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)  // 遍历所有策略上下文
	{
		HftContextPtr& ctx = (HftContextPtr&)it->second;  // 获取策略上下文引用
		ctx->on_session_begin(_cur_tdate);  // 调用策略的交易日开始回调方法
	}

	if (_evt_listener)  // 如果事件监听器已设置
		_evt_listener->on_session_event(_cur_tdate, true);  // 通知事件监听器交易日开始

	_ready = true;  // 设置引擎就绪标志
}

/**
 * @brief 交易日结束回调
 * 
 * 处理交易日结束事件，调用基类方法，通知所有策略，并记录日志。
 */
void WtHftEngine::on_session_end()
{
	WtEngine::on_session_end();  // 调用基类的交易日结束方法

	for (auto it = _ctx_map.begin(); it != _ctx_map.end(); it++)  // 遍历所有策略上下文
	{
		HftContextPtr& ctx = (HftContextPtr&)it->second;  // 获取策略上下文引用
		ctx->on_session_end(_cur_tdate);  // 调用策略的交易日结束回调方法
	}

	WTSLogger::info("Trading day {} ended", _cur_tdate);  // 记录交易日结束日志
	if (_evt_listener)  // 如果事件监听器已设置
		_evt_listener->on_session_event(_cur_tdate, false);  // 通知事件监听器交易日结束
}

/**
 * @brief 分钟结束回调
 * @param curDate 当前日期
 * @param curTime 当前时间（分钟）
 * 
 * 处理分钟线闭合事件，由实时行情时钟调用。
 * 目前HFT策略不再调用on_schedule方法，此函数保留为空实现，仅作为接口占位。
 */
void WtHftEngine::on_minute_end(uint32_t curDate, uint32_t curTime)
{
	//已去掉高频策略的on_schedule
	//for(auto& cit : _ctx_map)
	//{
	//	HftContextPtr& ctx = cit.second;
	//	ctx->on_schedule(curDate, curTime);
	//}
}

/**
 * @brief 添加策略上下文
 * @param ctx 策略上下文智能指针
 * 
 * 将策略上下文添加到引擎的管理列表中，使用策略ID作为键。
 */
void WtHftEngine::addContext(HftContextPtr ctx)
{
	uint32_t sid = ctx->id();  // 获取策略ID
	_ctx_map[sid] = ctx;  // 将策略上下文存入映射表
}

/**
 * @brief 获取策略上下文
 * @param id 策略ID
 * @return HftContextPtr 返回策略上下文智能指针，不存在返回空指针
 * 
 * 根据策略ID查找并返回对应的策略上下文。
 */
HftContextPtr WtHftEngine::getContext(uint32_t id)
{
	auto it = _ctx_map.find(id);  // 在策略上下文映射表中查找策略ID
	if (it == _ctx_map.end())  // 如果未找到
		return HftContextPtr();  // 返回空指针

	return it->second;  // 返回策略上下文智能指针
}

/**
 * @brief 获取委托队列历史数据切片
 * @param sid 策略ID（未使用，保留用于接口一致性）
 * @param code 标准合约代码
 * @param count 数据条数
 * @return WTSOrdQueSlice* 返回委托队列数据切片指针
 * 
 * 查询指定合约的委托队列历史数据，返回指定条数的数据切片。
 */
WTSOrdQueSlice* WtHftEngine::get_order_queue_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_order_queue_slice(code, count);  // 调用数据管理器获取委托队列历史数据切片
}

/**
 * @brief 获取委托明细历史数据切片
 * @param sid 策略ID（未使用，保留用于接口一致性）
 * @param code 标准合约代码
 * @param count 数据条数
 * @return WTSOrdDtlSlice* 返回委托明细数据切片指针
 * 
 * 查询指定合约的委托明细历史数据，返回指定条数的数据切片。
 */
WTSOrdDtlSlice* WtHftEngine::get_order_detail_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_order_detail_slice(code, count);  // 调用数据管理器获取委托明细历史数据切片
}

/**
 * @brief 获取成交明细历史数据切片
 * @param sid 策略ID（未使用，保留用于接口一致性）
 * @param code 标准合约代码
 * @param count 数据条数
 * @return WTSTransSlice* 返回成交明细数据切片指针
 * 
 * 查询指定合约的成交明细历史数据，返回指定条数的数据切片。
 */
WTSTransSlice* WtHftEngine::get_transaction_slice(uint32_t sid, const char* code, uint32_t count)
{
	return _data_mgr->get_transaction_slice(code, count);  // 调用数据管理器获取成交明细历史数据切片
}
