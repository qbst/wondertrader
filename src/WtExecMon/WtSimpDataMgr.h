/*!
 * \file WtSimpDataMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief WonderTrader简单数据管理器类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WtSimpDataMgr类，是WonderTrader执行器模块中的数据管理器。
 * 该类负责管理行情数据和K线数据的读取、缓存和查询，为执行器提供数据访问接口。
 * 
 * 主要功能：
 * 1. 数据读取：通过IDataReader接口读取历史Tick数据和K线数据
 * 2. 实时数据管理：缓存和管理实时Tick数据
 * 3. K线数据管理：缓存和管理K线数据，支持不同周期和倍数
 * 4. 时间管理：管理当前日期、时间、交易日等时间信息
 * 5. 数据查询：提供Tick切片和K线切片的查询接口
 * 6. 数据监听：实现IDataStoreListener接口，接收数据存储器的回调通知
 * 
 * 设计特点：
 * - 双重接口实现：同时实现IDataReaderSink和IDataManager接口
 * - 数据缓存：使用哈希映射缓存K线和实时Tick数据，提高查询效率
 * - 时间同步：实时更新当前时间信息，确保时间准确性
 * - 数据监听：监听数据存储器的K线更新通知
 * 
 * 数据流程：
 * 1. 初始化：加载数据存储模块（IDataReader），初始化数据读取器
 * 2. 实时数据：接收实时Tick数据，更新缓存和时间信息
 * 3. 历史数据：通过IDataReader读取历史Tick和K线数据
 * 4. K线合成：对于非基础周期的K线，从基础周期K线合成
 */

#pragma once  // 防止头文件被重复包含

#include <vector>  // 包含vector容器，用于存储数据
#include "../Includes/IDataReader.h"  // 包含数据读取器接口，提供IDataReader和IDataReaderSink接口
#include "../Includes/IDataManager.h"  // 包含数据管理器接口，提供IDataManager接口

#include "../Includes/WTSCollection.hpp"  // 包含WonderTrader集合类，提供WTSHashMap等集合类型

class WtExecRunner;  // 前向声明：执行器运行器类

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSVariant;  // 前向声明：配置变体类
class WTSHisTickData;  // 前向声明：历史Tick数据类
class WTSKlineData;  // 前向声明：K线数据类
class WTSTickData;  // 前向声明：Tick数据类
class WTSKlineSlice;  // 前向声明：K线切片类
class WTSTickSlice;  // 前向声明：Tick切片类
class IBaseDataMgr;  // 前向声明：基础数据管理器接口
class WTSSessionInfo;  // 前向声明：交易会话信息类

/**
 * @brief WonderTrader简单数据管理器类
 * 
 * 该类是WonderTrader执行器模块中的数据管理器，负责管理行情数据和K线数据的读取、缓存和查询。
 * 同时实现IDataReaderSink和IDataManager接口，作为数据读取器的接收者和数据管理器。
 * 
 * 核心职责：
 * 1. 数据读取：通过IDataReader接口读取历史Tick数据和K线数据
 * 2. 实时数据管理：缓存和管理实时Tick数据
 * 3. K线数据管理：缓存和管理K线数据，支持不同周期和倍数
 * 4. 时间管理：管理当前日期、时间、交易日等时间信息
 * 5. 数据查询：提供Tick切片和K线切片的查询接口
 * 6. 数据监听：实现IDataStoreListener接口，接收数据存储器的回调通知
 * 
 * 使用流程：
 * 1. 创建WtSimpDataMgr实例
 * 2. 调用init()初始化数据管理器，加载数据存储模块
 * 3. 通过handle_push_quote()接收实时行情数据
 * 4. 通过get_tick_slice()和get_kline_slice()查询历史数据
 */
class WtSimpDataMgr : public IDataReaderSink, public IDataManager
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建数据管理器实例，初始化成员变量。
	 */
	WtSimpDataMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理数据管理器占用的资源，释放缓存数据。
	 */
	~WtSimpDataMgr();

private:
	/**
	 * @brief 初始化数据存储模块
	 * 
	 * 从配置加载数据存储模块（IDataReader），初始化数据读取器。
	 * 
	 * @param cfg 数据存储配置对象
	 * @return 初始化成功返回true，失败返回false
	 * 
	 * 初始化流程：
	 * 1. 从配置中获取数据存储模块名称（默认为WtDataStorage）
	 * 2. 加载数据存储模块动态库
	 * 3. 获取createDataReader函数指针
	 * 4. 创建数据读取器实例并初始化
	 */
	bool	initStore(WTSVariant* cfg);

public:
	/**
	 * @brief 初始化数据管理器
	 * 
	 * 初始化数据管理器，加载数据存储模块。
	 * 
	 * @param cfg 数据管理器配置对象
	 * @param runner 执行器运行器指针，用于获取基础数据管理器和热点合约管理器
	 * @return 初始化成功返回true，失败返回false
	 */
	bool	init(WTSVariant* cfg, WtExecRunner* runner);

	/**
	 * @brief 处理实时行情推送
	 * 
	 * 接收并处理实时行情数据，更新缓存和时间信息。
	 * 
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 * 
	 * 处理流程：
	 * 1. 检查Tick数据指针是否有效
	 * 2. 更新实时Tick缓存
	 * 3. 检查时间是否回退（过滤过期数据）
	 * 4. 更新当前日期和时间信息
	 * 5. 更新交易日信息
	 */
	void	handle_push_quote(const char* stdCode, WTSTickData* newTick);

	//////////////////////////////////////////////////////////////////////////
	//IDataManager接口实现
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 获取Tick数据切片（IDataManager接口实现）
	 * 
	 * 获取指定合约的Tick数据切片。
	 * 
	 * @param code 合约代码
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return 返回Tick数据切片指针，未找到返回NULL
	 */
	WTSTickSlice* get_tick_slice(const char* code, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取K线数据切片（IDataManager接口实现）
	 * 
	 * 获取指定合约的K线数据切片，支持不同周期和倍数。
	 * 
	 * @param code 合约代码
	 * @param period K线周期（如PERIOD_M1、PERIOD_M5等）
	 * @param times 周期倍数，1表示基础周期，大于1表示合成周期
	 * @param count 数据条数
	 * @param etime 截止时间戳，默认为0（当前时间）
	 * @return 返回K线数据切片指针，未找到返回NULL
	 * 
	 * K线合成说明：
	 * - 如果times为1，直接从数据读取器读取基础周期K线
	 * - 如果times大于1，从基础周期K线合成目标周期K线
	 * - 合成后的K线会被缓存，提高后续查询效率
	 */
	WTSKlineSlice* get_kline_slice(const char* code, WTSKlinePeriod period, uint32_t times, uint32_t count, uint64_t etime = 0) override;
	
	/**
	 * @brief 获取最新Tick数据（IDataManager接口实现）
	 * 
	 * 获取指定合约的最新Tick数据。
	 * 
	 * @param code 合约代码
	 * @return 返回最新Tick数据指针，未找到返回NULL
	 * 
	 * 注意事项：
	 * - 返回的Tick数据需要调用者负责释放（调用retain/release）
	 */
	WTSTickData* grab_last_tick(const char* code) override;

	//////////////////////////////////////////////////////////////////////////
	//IDataStoreListener接口实现
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief K线数据更新回调（IDataStoreListener接口实现）
	 * 
	 * 当数据存储器更新K线数据时调用。
	 * 
	 * @param code 合约代码
	 * @param period K线周期
	 * @param newBar 新的K线数据指针
	 * 
	 * 当前实现：空实现，不处理K线更新通知
	 */
	virtual void	on_bar(const char* code, WTSKlinePeriod period, WTSBarStruct* newBar) override;
	
	/**
	 * @brief 所有K线数据更新完成回调（IDataStoreListener接口实现）
	 * 
	 * 当数据存储器完成所有K线数据更新时调用。
	 * 
	 * @param updateTime 更新时间戳
	 * 
	 * 当前实现：空实现，不处理更新完成通知
	 */
	virtual void	on_all_bar_updated(uint32_t updateTime) override;

	/**
	 * @brief 获取基础数据管理器（IDataReaderSink接口实现）
	 * 
	 * 返回基础数据管理器的指针。
	 * 
	 * @return 返回基础数据管理器指针
	 */
	virtual IBaseDataMgr* get_basedata_mgr() override;
	
	/**
	 * @brief 获取热点合约管理器（IDataReaderSink接口实现）
	 * 
	 * 返回热点合约管理器的指针。
	 * 
	 * @return 返回热点合约管理器指针
	 */
	virtual IHotMgr*	get_hot_mgr() override;
	
	/**
	 * @brief 获取当前日期（IDataReaderSink接口实现）
	 * 
	 * 返回当前日期。
	 * 
	 * @return 返回当前日期（格式：YYYYMMDD）
	 */
	virtual uint32_t	get_date() override;
	
	/**
	 * @brief 获取当前分钟时间（IDataReaderSink接口实现）
	 * 
	 * 返回当前1分钟线时间。
	 * 
	 * @return 返回当前1分钟线时间（格式：HHMM）
	 */
	virtual uint32_t	get_min_time()override;
	
	/**
	 * @brief 获取当前秒数（IDataReaderSink接口实现）
	 * 
	 * 返回当前秒数（包括毫秒）。
	 * 
	 * @return 返回当前秒数（格式：SSmmm）
	 */
	virtual uint32_t	get_secs() override;

	/**
	 * @brief 数据读取器日志回调（IDataReaderSink接口实现）
	 * 
	 * 当数据读取器需要记录日志时调用。
	 * 
	 * @param ll 日志级别
	 * @param message 日志消息
	 */
	virtual void		reader_log(WTSLogLevel ll, const char* message) override;

	/**
	 * @brief 获取数据读取器指针
	 * 
	 * 返回数据读取器的指针。
	 * 
	 * @return 返回数据读取器指针
	 */
	inline IDataReader* reader() { return _reader; }

	/**
	 * @brief 获取当前原始时间
	 * 
	 * 返回当前真实分钟时间（不包括秒和毫秒）。
	 * 
	 * @return 返回当前真实分钟时间（格式：HHMM）
	 */
	inline uint32_t	get_raw_time() const { return _cur_raw_time; }
	
	/**
	 * @brief 获取当前交易日
	 * 
	 * 返回当前交易日。
	 * 
	 * @return 返回当前交易日（格式：YYYYMMDD）
	 */
	inline uint32_t	get_trading_day() const { return _cur_tdate; }

private:
	IDataReader*	_reader;  // 数据读取器指针，用于读取历史Tick和K线数据
	WtExecRunner*	_runner;  // 执行器运行器指针，用于获取基础数据管理器和热点合约管理器
	WTSSessionInfo*	_s_info;  // 交易会话信息指针，用于时间转换和交易时段判断

	typedef WTSHashMap<std::string> DataCacheMap;  // 定义数据缓存映射表类型，键为字符串，值为数据对象指针
	DataCacheMap* _bars_cache;	//K线缓存，缓存合成后的K线数据，键为"合约代码-周期-倍数"，值为K线数据对象
	DataCacheMap* _rt_tick_map;	//实时tick缓存，缓存实时Tick数据，键为合约代码，值为Tick数据对象

	uint32_t		_cur_date;		//当前日期，格式如yyyyMMdd（YYYYMMDD）
	uint32_t		_cur_act_time;	//当前完整时间，格式如hhmmssmmm（HHMMSSmmm，毫秒级）
	uint32_t		_cur_raw_time;	//当前真实分钟，格式如hhmm（HHMM，不包括秒和毫秒）
	uint32_t		_cur_min_time;	//当前1分钟线时间，格式如hhmm（HHMM，1分钟K线的时间）
	uint32_t		_cur_secs;		//当前秒数，格式如ssmmm（SSmmm，包括秒和毫秒）
	uint32_t		_cur_tdate;		//当前交易日，格式如yyyyMMdd（YYYYMMDD）

};

NS_WTP_END  // 结束WonderTrader命名空间
