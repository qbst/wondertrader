/*!
 * \file WTSBaseDataMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader基础数据管理器 - 交易基础信息管理核心模块
 * 
 * 本文件定义了WonderTrader框架中的基础数据管理系统，负责管理交易系统运行所需的
 * 各种基础信息，包括合约信息、品种信息、交易时间、节假日等核心数据。
 * 
 * 主要功能：
 * - 合约信息管理：加载和查询各交易所的合约基本信息
 * - 品种信息管理：管理商品期货、股票等品种的交易规则
 * - 交易时间管理：处理各种交易时段和时区转换
 * - 节假日管理：维护各地区的交易日历和节假日信息
 * - 交易日计算：提供交易日相关的各种计算功能
 * 
 * 设计特点：
 * - 实现IBaseDataMgr接口，提供统一的基础数据访问接口
 * - 支持多交易所、多品种的复杂交易环境
 * - 高效的数据查询和缓存机制
 * - 灵活的交易时间处理，支持夜盘等复杂时段
 * - 完整的交易日历管理系统
 */
#pragma once
#include "../Includes/IBaseDataMgr.h"    // 基础数据管理器接口定义
#include "../Includes/WTSCollection.hpp" // WTS集合类定义
#include "../Includes/FasterDefs.h"      // 高性能数据结构定义

USING_NS_WTP;

/**
 * @name 基础数据管理类型定义
 * @brief 用于构建基础数据管理系统的数据结构类型
 * @{
 */

/// 交易日模板映射：模板ID -> 交易日模板，用于管理不同地区的交易日历
typedef wt_hashmap<std::string, TradingDayTpl>	TradingDayTplMap;

/// 合约列表映射：合约代码 -> 合约信息，单个交易所内的合约管理
typedef WTSHashMap<std::string>		WTSContractList;

/// 交易所合约映射：交易所代码 -> 合约列表，按交易所组织合约信息
typedef WTSHashMap<std::string>		WTSExchgContract;

/// 合约映射表：合约代码 -> 合约信息数组，支持同名合约的多版本管理
typedef WTSHashMap<std::string>		WTSContractMap;

/// 交易时段映射：时段ID -> 交易时段信息，管理各种交易时间模板
typedef WTSHashMap<std::string>		WTSSessionMap;

/// 商品品种映射：品种ID -> 品种信息，管理各种交易品种的基本信息
typedef WTSHashMap<std::string>		WTSCommodityMap;

/// 时段代码映射：时段ID -> 品种代码集合，用于快速查询某时段下的所有品种
typedef wt_hashmap<std::string, CodeSet> SessionCodeMap;

/** @} */

/**
 * @class WTSBaseDataMgr
 * @brief WonderTrader基础数据管理器实现类
 * 
 * WTSBaseDataMgr是IBaseDataMgr接口的具体实现，负责管理交易系统运行所需的
 * 所有基础数据。该类是整个交易系统的基础设施，为策略、风控、数据处理等
 * 各个模块提供统一的基础数据访问服务。
 * 
 * 核心职责：
 * - 合约信息管理：维护各交易所的合约基本信息和生命周期
 * - 品种信息管理：管理交易品种的规格参数和交易规则
 * - 交易时间管理：处理复杂的交易时段和时区转换逻辑
 * - 节假日管理：维护准确的交易日历信息
 * - 交易日计算：提供各种交易日相关的计算服务
 * 
 * 数据组织结构：
 * - 按交易所分层组织合约信息
 * - 支持合约的多版本和生命周期管理
 * - 提供高效的查询和缓存机制
 * - 支持动态数据更新和热加载
 */
class WTSBaseDataMgr : public IBaseDataMgr
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化基础数据管理器，创建各种数据容器。
	 */
	WTSBaseDataMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理所有资源，释放内存。
	 */
	~WTSBaseDataMgr();

public:
	/**
	 * @name IBaseDataMgr接口实现 - 基础数据查询接口
	 * @brief 实现基础数据管理器的核心查询功能
	 * @{
	 */
	
	/**
	 * @brief 根据标准品种ID获取商品信息
	 * @param stdPID 标准品种ID（格式：交易所.品种代码）
	 * @return WTSCommodityInfo* 商品信息指针，未找到返回NULL
	 */
	virtual WTSCommodityInfo*	getCommodity(const char* stdPID) override;
	
	/**
	 * @brief 根据交易所和品种代码获取商品信息
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @return WTSCommodityInfo* 商品信息指针，未找到返回NULL
	 */
	virtual WTSCommodityInfo*	getCommodity(const char* exchg, const char* pid) override;

	/**
	 * @brief 获取合约信息
	 * @param code 合约代码
	 * @param exchg 交易所代码，默认为空（搜索所有交易所）
	 * @param uDate 查询日期，用于检查合约有效期，默认为0（不检查）
	 * @return WTSContractInfo* 合约信息指针，未找到返回NULL
	 */
	virtual WTSContractInfo*	getContract(const char* code, const char* exchg = "", uint32_t uDate = 0) override;
	
	/**
	 * @brief 获取合约列表
	 * @param exchg 交易所代码，默认为空（获取所有交易所）
	 * @param uDate 查询日期，用于筛选有效合约，默认为0（不筛选）
	 * @return WTSArray* 合约信息数组，调用者负责释放
	 */
	virtual WTSArray*			getContracts(const char* exchg = "", uint32_t uDate = 0) override;

	/**
	 * @brief 根据时段ID获取交易时段信息
	 * @param sid 时段ID
	 * @return WTSSessionInfo* 交易时段信息指针，未找到返回NULL
	 */
	virtual WTSSessionInfo*		getSession(const char* sid) override;
	
	/**
	 * @brief 根据合约代码获取对应的交易时段信息
	 * @param code 合约代码
	 * @param exchg 交易所代码，默认为空
	 * @return WTSSessionInfo* 交易时段信息指针，未找到返回NULL
	 */
	virtual WTSSessionInfo*		getSessionByCode(const char* code, const char* exchg = "") override;
	
	/**
	 * @brief 获取所有交易时段信息
	 * @return WTSArray* 交易时段信息数组，调用者负责释放
	 */
	virtual WTSArray*			getAllSessions() override;
	
	/**
	 * @brief 判断指定日期是否为节假日
	 * @param stdPID 标准品种ID或模板ID
	 * @param uDate 查询日期
	 * @param isTpl 是否为模板ID，默认false
	 * @return bool 是节假日返回true，否则返回false
	 */
	virtual bool				isHoliday(const char* stdPID, uint32_t uDate, bool isTpl = false) override;

	/**
	 * @brief 计算交易日期
	 * @param stdPID 标准品种ID或时段ID
	 * @param uDate 自然日期
	 * @param uTime 时间（格式：HHMMSS）
	 * @param isSession 是否为时段ID，默认false
	 * @return uint32_t 对应的交易日期
	 */
	virtual uint32_t			calcTradingDate(const char* stdPID, uint32_t uDate, uint32_t uTime, bool isSession = false) override;
	
	/**
	 * @brief 获取交易边界时间
	 * @param stdPID 标准品种ID或时段ID
	 * @param tDate 交易日期
	 * @param isSession 是否为时段ID，默认false
	 * @param isStart 是否获取开始时间，true为开始时间，false为结束时间
	 * @return uint64_t 边界时间（格式：YYYYMMDDHHMM）
	 */
	virtual uint64_t			getBoundaryTime(const char* stdPID, uint32_t tDate, bool isSession = false, bool isStart = true) override;

	/**
	 * @brief 获取合约数量
	 * @param exchg 交易所代码，默认为空（统计所有交易所）
	 * @param uDate 查询日期，用于筛选有效合约，默认为0（不筛选）
	 * @return uint32_t 合约数量
	 */
	virtual uint32_t			getContractSize(const char* exchg = "", uint32_t uDate = 0) override;
	
	/** @} */

	/**
	 * @name 资源管理接口
	 * @brief 资源加载和释放管理
	 * @{
	 */
	
	/**
	 * @brief 释放所有资源
	 * 
	 * 清理所有加载的基础数据，释放相关内存。
	 * 通常在程序退出或重新初始化时调用。
	 */
	void		release();

	/**
	 * @brief 加载交易时段配置
	 * @param filename 配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 */
	bool		loadSessions(const char* filename);
	
	/**
	 * @brief 加载商品品种配置
	 * @param filename 配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 */
	bool		loadCommodities(const char* filename);
	
	/**
	 * @brief 加载合约信息配置
	 * @param filename 配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 */
	bool		loadContracts(const char* filename);
	
	/**
	 * @brief 加载节假日配置
	 * @param filename 配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 */
	bool		loadHolidays(const char* filename);
	
	/** @} */

	/**
	 * @name 扩展功能接口
	 * @brief 提供额外的交易日计算和查询功能
	 * @{
	 */
	
	/**
	 * @brief 获取交易日期
	 * @param stdPID 标准品种ID或模板ID
	 * @param uOffDate 偏移日期，默认为0表示当前日期
	 * @param uOffMinute 偏移分钟数，默认为0
	 * @param isTpl 是否为模板ID，默认false
	 * @return uint32_t 交易日期
	 */
	uint32_t	getTradingDate(const char* stdPID, uint32_t uOffDate = 0, uint32_t uOffMinute = 0, bool isTpl = false);
	
	/**
	 * @brief 获取下一个交易日
	 * @param stdPID 标准品种ID或模板ID
	 * @param uDate 基准日期
	 * @param days 天数，默认为1天
	 * @param isTpl 是否为模板ID，默认false
	 * @return uint32_t 下一个交易日
	 */
	uint32_t	getNextTDate(const char* stdPID, uint32_t uDate, int days = 1, bool isTpl = false);
	
	/**
	 * @brief 获取上一个交易日
	 * @param stdPID 标准品种ID或模板ID
	 * @param uDate 基准日期
	 * @param days 天数，默认为1天
	 * @param isTpl 是否为模板ID，默认false
	 * @return uint32_t 上一个交易日
	 */
	uint32_t	getPrevTDate(const char* stdPID, uint32_t uDate, int days = 1, bool isTpl = false);
	
	/**
	 * @brief 判断是否为交易日
	 * @param stdPID 标准品种ID或模板ID
	 * @param uDate 查询日期
	 * @param isTpl 是否为模板ID，默认false
	 * @return bool 是交易日返回true，否则返回false
	 */
	bool		isTradingDate(const char* stdPID, uint32_t uDate, bool isTpl = false);
	
	/**
	 * @brief 设置当前交易日
	 * @param stdPID 标准品种ID或模板ID
	 * @param uDate 交易日期
	 * @param isTpl 是否为模板ID，默认false
	 */
	void		setTradingDate(const char* stdPID, uint32_t uDate, bool isTpl = false);

	/**
	 * @brief 获取交易时段对应的品种集合
	 * @param sid 时段ID
	 * @return CodeSet* 品种代码集合指针，未找到返回NULL
	 */
	CodeSet*	getSessionComms(const char* sid);
	
	/** @} */

private:
	/**
	 * @brief 根据品种ID获取对应的模板ID
	 * @param stdPID 标准品种ID
	 * @return const char* 模板ID字符串
	 */
	const char* getTplIDByPID(const char* stdPID);

private:
	/**
	 * @name 私有成员变量
	 * @brief 基础数据管理器的内部数据存储
	 * @{
	 */
	
	/// 交易日模板映射表，存储各地区的交易日历模板
	TradingDayTplMap	m_mapTradingDay;

	/// 时段代码映射表，维护时段与品种的对应关系
	SessionCodeMap		m_mapSessionCode;

	/// 按交易所组织的合约信息映射表
	WTSExchgContract*	m_mapExchgContract;
	
	/// 交易时段信息映射表
	WTSSessionMap*		m_mapSessions;
	
	/// 商品品种信息映射表
	WTSCommodityMap*	m_mapCommodities;
	
	/// 合约信息映射表，支持同名合约的多版本管理
	WTSContractMap*		m_mapContracts;
	
	/** @} */
};

