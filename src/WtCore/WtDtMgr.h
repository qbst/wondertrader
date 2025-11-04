/*!
 * \file WtDataManager.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据管理器头文件
 * 
 * 文件设计逻辑与作用总结：
 * ========================
 * 本文件定义了WtDtMgr类，用于管理策略运行所需的各种数据。
 * 
 * 核心功能：
 * 1. 数据读取管理：封装IDataReader接口，提供统一的数据读取接口
 * 2. K线数据管理：缓存K线数据，支持多周期K线重采样和缓存
 * 3. Tick数据管理：缓存实时Tick数据，支持后复权Tick数据的处理
 * 4. 数据订阅管理：管理策略对数据的订阅关系
 * 5. K线更新通知：当K线更新时，统一通知引擎，避免重复通知
 * 
 * 设计特点：
 * - 继承自IDataReaderSink和IDataManager，实现数据读取回调和数据管理接口
 * - 支持K线数据的强制缓存和自动缓存策略
 * - 支持K线重采样的小节对齐功能
 * - 支持后复权Tick数据的缓存和处理
 * - 使用延迟通知机制，统一处理K线更新事件
 * 
 * 与引擎的关系：
 * - 作为引擎的数据提供者，为策略提供数据访问接口
 * - 通过回调接口接收数据读取器的数据更新
 * - 将数据更新统一转发给引擎处理
 */
#pragma once  // 防止头文件重复包含
#include <vector>  // 包含标准向量容器头文件
#include "../Includes/IDataReader.h"  // 包含数据读取器接口头文件
#include "../Includes/IDataManager.h"  // 包含数据管理器接口头文件

#include "../Includes/FasterDefs.h"  // 包含快速定义头文件（wt_hashmap等）
#include "../Includes/WTSCollection.hpp"  // 包含WonderTrader集合类头文件

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：变体类型，用于配置参数传递
class WTSTickData;  // 前向声明：Tick数据类
class WTSKlineSlice;  // 前向声明：K线切片类
class WTSTickSlice;  // 前向声明：Tick切片类
class IBaseDataMgr;  // 前向声明：基础数据管理器接口
class WtEngine;  // 前向声明：引擎基类

/**
 * @class WtDtMgr
 * @brief 数据管理器类
 * 
 * 该类负责管理策略运行所需的各种数据，包括K线数据和Tick数据。
 * 继承自IDataReaderSink（数据读取器回调接口）和IDataManager（数据管理器接口）。
 * 
 * 主要职责：
 * 1. 封装数据读取器，提供统一的数据访问接口
 * 2. 管理K线数据缓存，支持多周期K线重采样
 * 3. 管理Tick数据缓存，支持实时Tick和后复权Tick的处理
 * 4. 处理K线更新事件，统一通知引擎
 * 5. 提供数据订阅管理功能
 * 
 * 工作流程：
 * 1. 初始化：加载数据存储模块，初始化数据读取器
 * 2. 数据请求：策略请求数据时，从缓存或数据读取器获取
 * 3. 数据更新：接收数据读取器的更新回调，更新缓存并通知引擎
 * 4. 统一通知：所有K线更新完成后，统一触发引擎的on_bar事件
 */
class WtDtMgr : public IDataReaderSink, public IDataManager  // 继承数据读取器回调接口和数据管理器接口
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化数据管理器，所有指针成员初始化为NULL，标志位初始化为false。
	 */
	WtDtMgr();
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放所有缓存对象。
	 */
	~WtDtMgr();

private:
	/**
	 * @brief 初始化数据存储模块
	 * @param cfg 数据存储配置参数
	 * @return bool 初始化成功返回true，否则返回false
	 * 
	 * 根据配置参数加载数据存储模块（动态库），创建数据读取器实例。
	 * 如果配置中未指定模块，则使用默认模块WtDataStorage。
	 * 
	 * 实现逻辑：
	 * 1. 检查配置参数是否有效
	 * 2. 从配置中获取模块名称（如果未指定则使用默认值）
	 * 3. 加载动态库
	 * 4. 获取创建函数符号
	 * 5. 创建数据读取器实例
	 * 6. 初始化数据读取器
	 */
	bool	initStore(WTSVariant* cfg);

public:
	/**
	 * @brief 初始化数据管理器
	 * @param cfg 配置参数，包含数据存储配置等
	 * @param engine 引擎指针，用于获取会话信息和触发事件
	 * @param bForceCache 是否强制缓存K线，默认为false
	 * @return bool 初始化成功返回true，否则返回false
	 * 
	 * 初始化数据管理器，设置引擎指针和缓存策略。
	 * 如果bForceCache为true，则所有K线都会被缓存（包括1倍周期）。
	 * 
	 * 实现逻辑：
	 * 1. 保存引擎指针
	 * 2. 从配置中读取小节对齐标志
	 * 3. 设置强制缓存标志
	 * 4. 初始化数据存储模块
	 */
	bool	init(WTSVariant* cfg, WtEngine* engine, bool bForceCache = false);

	/**
	 * @brief 注册历史数据加载器
	 * @param loader 历史数据加载器指针
	 * 
	 * 设置历史数据加载器，用于数据读取器加载历史数据。
	 */
	void	regsiter_loader(IHisDataLoader* loader) { _loader = loader; }  // 设置历史数据加载器指针

	/**
	 * @brief 处理推送的行情数据
	 * @param stdCode 标准合约代码字符串
	 * @param newTick 新的Tick数据指针
	 * 
	 * 接收外部推送的实时行情数据，更新实时Tick缓存。
	 * 如果是后复权合约，还会更新后复权Tick缓存。
	 */
	void	handle_push_quote(const char* stdCode, WTSTickData* newTick);

	//////////////////////////////////////////////////////////////////////////
	// IDataManager接口实现
	// 以下方法实现IDataManager接口，为策略提供数据访问接口
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief 获取Tick数据切片
	 * @param stdCode 标准合约代码字符串
	 * @param count 获取的Tick数量
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSTickSlice* 返回Tick数据切片指针，如果数据读取器无效返回NULL
	 * 
	 * 从数据读取器或缓存中获取指定数量的Tick数据。
	 * 如果是后复权合约，会从后复权缓存中获取；否则直接从数据读取器获取。
	 */
	virtual WTSTickSlice* get_tick_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	/**
	 * @brief 获取订单队列切片
	 * @param stdCode 标准合约代码字符串
	 * @param count 获取的数据条数
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSOrdQueSlice* 返回订单队列切片指针，如果数据读取器无效返回NULL
	 * 
	 * 从数据读取器获取订单队列数据。
	 */
	virtual WTSOrdQueSlice* get_order_queue_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	/**
	 * @brief 获取订单明细切片
	 * @param stdCode 标准合约代码字符串
	 * @param count 获取的数据条数
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSOrdDtlSlice* 返回订单明细切片指针，如果数据读取器无效返回NULL
	 * 
	 * 从数据读取器获取订单明细数据。
	 */
	virtual WTSOrdDtlSlice* get_order_detail_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	/**
	 * @brief 获取逐笔成交切片
	 * @param stdCode 标准合约代码字符串
	 * @param count 获取的数据条数
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSTransSlice* 返回逐笔成交切片指针，如果数据读取器无效返回NULL
	 * 
	 * 从数据读取器获取逐笔成交数据。
	 */
	virtual WTSTransSlice* get_transaction_slice(const char* stdCode, uint32_t count, uint64_t etime = 0) override;
	/**
	 * @brief 获取K线数据切片
	 * @param stdCode 标准合约代码字符串
	 * @param period K线周期
	 * @param times K线倍数
	 * @param count 获取的K线数量
	 * @param etime 结束时间戳，默认为0（使用最新时间）
	 * @return WTSKlineSlice* 返回K线数据切片指针，如果数据读取器无效返回NULL
	 * 
	 * 从数据读取器或缓存中获取指定数量和周期的K线数据。
	 * 如果times为1且不强制缓存，则直接从数据读取器获取。
	 * 如果times大于1，则从缓存中获取重采样后的K线数据。
	 */
	virtual WTSKlineSlice* get_kline_slice(const char* stdCode, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime = 0) override;
	/**
	 * @brief 获取最后一个Tick数据
	 * @param stdCode 标准合约代码字符串
	 * @return WTSTickData* 返回最后一个Tick数据指针，如果缓存无效返回NULL
	 * 
	 * 从实时Tick缓存中获取最新的Tick数据。
	 * 返回的数据需要调用者负责释放（调用release方法）。
	 */
	virtual WTSTickData* grab_last_tick(const char* stdCode) override;
	/**
	 * @brief 获取复权因子
	 * @param stdCode 标准合约代码字符串
	 * @param uDate 日期（格式：YYYYMMDD）
	 * @return double 返回复权因子，如果数据读取器无效返回1.0
	 * 
	 * 从数据读取器获取指定日期的复权因子。
	 */
	virtual double get_adjusting_factor(const char* stdCode, uint32_t uDate) override;

	/**
	 * @brief 获取复权标志
	 * @return uint32_t 返回复权标志（0=不复权，1=前复权，2=后复权）
	 * 
	 * 从数据读取器获取复权标志。
	 * 使用静态变量缓存结果，避免重复查询。
	 */
	virtual uint32_t get_adjusting_flag() override;

	//////////////////////////////////////////////////////////////////////////
	// IDataReaderSink接口实现
	// 以下方法实现IDataReaderSink接口，接收数据读取器的回调
	//////////////////////////////////////////////////////////////////////////
	/**
	 * @brief K线更新回调
	 * @param code 合约代码字符串
	 * @param period K线周期
	 * @param newBar 新的K线数据指针
	 * 
	 * 当数据读取器有新的K线数据时被调用。
	 * 更新K线缓存，并将更新事件加入通知队列。
	 * 所有K线更新完成后，统一触发引擎的on_bar事件。
	 */
	virtual void	on_bar(const char* code, WTSKlinePeriod period, WTSBarStruct* newBar) override;
	/**
	 * @brief 所有K线更新完成回调
	 * @param updateTime 更新时间戳
	 * 
	 * 当数据读取器完成所有K线更新时被调用。
	 * 统一处理通知队列中的所有K线更新事件，触发引擎的on_bar事件。
	 */
	virtual void	on_all_bar_updated(uint32_t updateTime) override;

	/**
	 * @brief 获取基础数据管理器
	 * @return IBaseDataMgr* 返回基础数据管理器指针
	 * 
	 * 从引擎获取基础数据管理器，用于获取合约信息等。
	 */
	virtual IBaseDataMgr*	get_basedata_mgr() override;
	/**
	 * @brief 获取热点合约管理器
	 * @return IHotMgr* 返回热点合约管理器指针
	 * 
	 * 从引擎获取热点合约管理器，用于获取主力合约信息等。
	 */
	virtual IHotMgr*		get_hot_mgr() override;
	/**
	 * @brief 获取当前日期
	 * @return uint32_t 返回当前日期（格式：YYYYMMDD）
	 * 
	 * 从引擎获取当前日期。
	 */
	virtual uint32_t	get_date() override;
	/**
	 * @brief 获取当前分钟时间
	 * @return uint32_t 返回当前分钟时间（格式：HHMM）
	 * 
	 * 从引擎获取当前分钟时间。
	 */
	virtual uint32_t	get_min_time()override;
	/**
	 * @brief 获取当前秒数
	 * @return uint32_t 返回当前秒数（包含毫秒，格式：SSmmm）
	 * 
	 * 从引擎获取当前秒数。
	 */
	virtual uint32_t	get_secs() override;

	/**
	 * @brief 数据读取器日志回调
	 * @param ll 日志级别
	 * @param message 日志消息字符串
	 * 
	 * 当数据读取器需要记录日志时被调用，直接转发给日志系统。
	 */
	virtual void		reader_log(WTSLogLevel ll, const char* message) override;

	/**
	 * @brief 获取数据读取器指针
	 * @return IDataReader* 返回数据读取器指针
	 * 
	 * 获取内部数据读取器指针，用于外部直接访问数据读取器。
	 */
	inline IDataReader*	reader() { return _reader; }  // 返回数据读取器指针
	/**
	 * @brief 获取历史数据加载器指针
	 * @return IHisDataLoader* 返回历史数据加载器指针
	 * 
	 * 获取内部历史数据加载器指针，用于外部直接访问历史数据加载器。
	 */
	inline IHisDataLoader*	loader() { return _loader; }  // 返回历史数据加载器指针

private:
	IDataReader*	_reader;  // 数据读取器指针，用于读取历史数据和实时数据
	IHisDataLoader*	_loader;  // 历史数据加载器指针，用于加载历史数据
	WtEngine*		_engine;  // 引擎指针，用于获取会话信息和触发事件

	bool			_align_by_section;	// 强制小节对齐标志，true表示K线重采样时按小节对齐，false表示按时间对齐
	bool			_force_cache;		// 强制缓存K线标志，true表示所有K线都缓存（包括1倍周期），false表示只有重采样K线缓存

	wt_hashset<std::string> _subed_basic_bars;  // 已订阅的基础周期K线集合，键为"合约代码-周期"格式
	typedef WTSHashMap<std::string> DataCacheMap;  // 数据缓存映射表类型定义，键为字符串，值为void*（实际为各种数据对象）
	DataCacheMap*	_bars_cache;	// K线缓存映射表，键为"合约代码-周期-倍数"格式，值为WTSKlineData*
	DataCacheMap*	_rt_tick_map;	// 实时Tick缓存映射表，键为合约代码，值为WTSTickData*
	//By Wesley @ 2022.02.11
	//这个只有后复权tick数据
	//因为前复权和不复权，都不需要缓存
	DataCacheMap*	_ticks_adjusted;	// 复权Tick缓存映射表，键为合约代码（不含+后缀），值为WTSHisTickData*（仅缓存后复权数据）

	/**
	 * @struct NotifyItem
	 * @brief K线通知项结构体
	 * 
	 * 用于存储待通知的K线更新信息，在on_all_bar_updated时统一处理。
	 */
	typedef struct _NotifyItem
	{
		char		_code[MAX_INSTRUMENT_LENGTH];  // 合约代码字符串（最大长度限制）
		char		_period[2] = { 0 };  // 周期字符（'m'或'd'），包含结束符
		uint32_t	_times;  // K线倍数
		WTSBarStruct* _newBar;  // 新的K线数据指针

		/**
		 * @brief 构造函数
		 * @param code 合约代码字符串
		 * @param period 周期字符（'m'或'd'）
		 * @param times K线倍数
		 * @param newBar 新的K线数据指针
		 * 
		 * 初始化通知项，复制合约代码和周期信息。
		 */
		_NotifyItem(const char* code, char period, uint32_t times, WTSBarStruct* newBar)
			: _times(times), _newBar(newBar)  // 初始化倍数和K线数据指针
		{
			wt_strcpy(_code, code);  // 复制合约代码字符串
			_period[0] = period;  // 设置周期字符
		}
	} NotifyItem;  // K线通知项结构体类型定义

	std::vector<NotifyItem> _bar_notifies;  // K线通知队列，存储待通知的K线更新项
};

NS_WTP_END  // 结束WonderTrader命名空间