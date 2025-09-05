/*!
 * \file IBaseDataMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 基础数据管理器接口定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中基础数据管理器的核心接口，负责管理交易系统的基础数据。
 * 主要功能包括：
 * 1. 管理品种信息（WTSCommodityInfo），包括合约乘数、最小变动价位等
 * 2. 管理合约信息（WTSContractInfo），包括合约代码、交易所、到期日等
 * 3. 管理交易时间模板（WTSSessionInfo），包括交易时段、休市时间等
 * 4. 提供节假日判断和交易日计算功能
 * 5. 支持合约数量统计和边界时间计算
 * 
 * 该类主要用于WonderTrader框架中的基础数据管理，为交易策略和风控系统提供必要的基础信息。
 * 通过统一的接口设计，支持多种数据源和不同交易所的数据管理。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含字符串支持
#include <stdint.h>  // 包含固定大小整数类型

#include "WTSMarcos.h"  // 包含WonderTrader宏定义
#include "FasterDefs.h"  // 包含高性能哈希容器定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
typedef CodeSet ContractSet;  // 定义合约集合类型别名，使用代码集合存储合约信息

class WTSContractInfo;  // 前向声明：WTS合约信息类
class WTSArray;  // 前向声明：WTS数组类
class WTSSessionInfo;  // 前向声明：WTS交易时间模板信息类
class WTSCommodityInfo;  // 前向声明：WTS品种信息类

typedef wt_hashset<uint32_t> HolidaySet;  // 定义节假日集合类型别名，使用高性能哈希集合存储节假日日期

/**
 * @struct _TradingDayTpl
 * @brief 交易日模板结构体
 * 
 * 该结构体用于存储交易日模板信息，包括当前交易日和节假日集合。
 * 用于交易日计算和节假日判断。
 * 
 * 主要成员：
 * - _cur_tdate：当前交易日日期
 * - _holidays：节假日集合，存储所有节假日日期
 */
typedef struct _TradingDayTpl
{
	uint32_t	_cur_tdate;  // 当前交易日日期，格式为YYYYMMDD
	HolidaySet	_holidays;   // 节假日集合，存储所有节假日日期

	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化交易日模板，设置当前交易日为0。
	 */
	_TradingDayTpl() :_cur_tdate(0){}  // 初始化当前交易日为0
} TradingDayTpl;  // 交易日模板类型定义

/**
 * @class IBaseDataMgr
 * @brief 基础数据管理器接口类
 * 
 * 该类定义了WonderTrader框架中基础数据管理器的核心接口，负责管理交易系统的基础数据。
 * 包括品种信息、合约信息、交易时间模板、节假日信息等。
 * 
 * 主要特性：
 * - 提供品种信息的查询和管理
 * - 支持合约信息的查询和管理
 * - 管理交易时间模板和会话信息
 * - 提供节假日判断和交易日计算功能
 * - 支持合约数量统计和边界时间计算
 * - 统一的接口设计，支持多种数据源
 */
class IBaseDataMgr
{
public:
	/**
	 * @brief 根据交易所品种ID获取品种信息
	 * @param exchgpid 交易所品种ID，格式为"交易所.品种"
	 * @return WTSCommodityInfo* 返回品种信息对象指针，未找到返回NULL
	 * 
	 * 该函数根据交易所品种ID获取对应的品种信息，包括合约乘数、最小变动价位等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSCommodityInfo*	getCommodity(const char* exchgpid)						= 0;  // 纯虚函数：根据交易所品种ID获取品种信息

	/**
	 * @brief 根据交易所和品种获取品种信息
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @return WTSCommodityInfo* 返回品种信息对象指针，未找到返回NULL
	 * 
	 * 该函数根据交易所代码和品种代码获取对应的品种信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSCommodityInfo*	getCommodity(const char* exchg, const char* pid)		= 0;  // 纯虚函数：根据交易所和品种获取品种信息

	/**
	 * @brief 根据合约代码获取合约信息
	 * @param code 合约代码
	 * @param exchg 交易所代码，默认为空字符串
	 * @param uDate 查询日期，默认为0（当前日期）
	 * @return WTSContractInfo* 返回合约信息对象指针，未找到返回NULL
	 * 
	 * 该函数根据合约代码获取对应的合约信息，包括合约代码、交易所、到期日等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSContractInfo*	getContract(const char* code, const char* exchg = "", uint32_t uDate = 0)	= 0;  // 纯虚函数：根据合约代码获取合约信息

	/**
	 * @brief 获取指定交易所的所有合约信息
	 * @param exchg 交易所代码，默认为空字符串（所有交易所）
	 * @param uDate 查询日期，默认为0（当前日期）
	 * @return WTSArray* 返回合约信息数组指针，未找到返回NULL
	 * 
	 * 该函数获取指定交易所的所有合约信息，返回合约信息数组。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSArray*			getContracts(const char* exchg = "", uint32_t uDate = 0)					= 0;  // 纯虚函数：获取指定交易所的所有合约信息

	/**
	 * @brief 根据会话ID获取交易时间模板信息
	 * @param sid 会话ID
	 * @return WTSSessionInfo* 返回交易时间模板信息对象指针，未找到返回NULL
	 * 
	 * 该函数根据会话ID获取对应的交易时间模板信息，包括交易时段、休市时间等。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSSessionInfo*		getSession(const char* sid)						= 0;  // 纯虚函数：根据会话ID获取交易时间模板信息

	/**
	 * @brief 根据合约代码获取交易时间模板信息
	 * @param code 合约代码
	 * @param exchg 交易所代码，默认为空字符串
	 * @return WTSSessionInfo* 返回交易时间模板信息对象指针，未找到返回NULL
	 * 
	 * 该函数根据合约代码获取对应的交易时间模板信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSSessionInfo*		getSessionByCode(const char* code, const char* exchg = "") = 0;  // 纯虚函数：根据合约代码获取交易时间模板信息

	/**
	 * @brief 获取所有交易时间模板信息
	 * @return WTSArray* 返回所有交易时间模板信息数组指针
	 * 
	 * 该函数获取系统中所有可用的交易时间模板信息。
	 * 纯虚函数，子类必须实现。
	 */
	virtual WTSArray*			getAllSessions() = 0;  // 纯虚函数：获取所有交易时间模板信息

	/**
	 * @brief 判断指定日期是否为节假日
	 * @param pid 品种代码
	 * @param uDate 查询日期，格式为YYYYMMDD
	 * @param isTpl 是否使用模板判断，默认为false
	 * @return bool 是节假日返回true，否则返回false
	 * 
	 * 该函数判断指定品种在指定日期是否为节假日。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool				isHoliday(const char* pid, uint32_t uDate, bool isTpl = false) = 0;  // 纯虚函数：判断指定日期是否为节假日

	/**
	 * @brief 计算交易日
	 * @param stdPID 标准品种代码
	 * @param uDate 基准日期，格式为YYYYMMDD
	 * @param uTime 基准时间，格式为HHMMSS
	 * @param isSession 是否基于会话时间，默认为false
	 * @return uint32_t 返回计算得到的交易日，格式为YYYYMMDD
	 * 
	 * 该函数根据基准日期和时间计算对应的交易日，自动处理节假日和交易时间。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint32_t			calcTradingDate(const char* stdPID, uint32_t uDate, uint32_t uTime, bool isSession = false) = 0;  // 纯虚函数：计算交易日

	/**
	 * @brief 获取边界时间
	 * @param stdPID 标准品种代码
	 * @param tDate 交易日，格式为YYYYMMDD
	 * @param isSession 是否基于会话时间，默认为false
	 * @param isStart 是否为开始时间，true为开始时间，false为结束时间
	 * @return uint64_t 返回边界时间，格式为YYYYMMDDHHMMSS
	 * 
	 * 该函数获取指定交易日的开始时间或结束时间，用于交易时间判断。
	 * 纯虚函数，子类必须实现。
	 */
	virtual uint64_t			getBoundaryTime(const char* stdPID, uint32_t tDate, bool isSession = false, bool isStart = true) = 0;  // 纯虚函数：获取边界时间

	/**
	 * @brief 获取合约数量
	 * @param exchg 交易所代码，默认为空字符串（所有交易所）
	 * @param uDate 查询日期，默认为0（当前日期）
	 * @return uint32_t 返回合约数量
	 * 
	 * 该函数获取指定交易所的合约数量，用于统计和监控。
	 * 默认实现返回0，子类可以重写此函数。
	 */
	virtual uint32_t			getContractSize(const char* exchg = "", uint32_t uDate = 0) { return 0; }  // 虚函数：获取合约数量，默认返回0
};
NS_WTP_END  // 结束WonderTrader命名空间