/*!
 * \file IHotMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 主力合约管理器接口定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中主力合约管理器的核心接口，负责管理期货合约的主力切换规则。
 * 主要功能包括：
 * 1. 定义主力合约段结构，包含分月代码、开始日期、结束日期和切换因子
 * 2. 提供主力合约和次主力合约的分月代码查询接口
 * 3. 支持主力合约和次主力合约的切换判断
 * 4. 提供主力段分割功能，将主力合约在指定时段的分月合约全部提取
 * 5. 支持自定义主力合约规则的管理和查询
 * 6. 提供规则标签和切换因子的查询功能
 * 
 * 该类主要用于WonderTrader框架中的主力合约管理系统，为策略引擎和数据处理系统提供主力切换规则支持。
 * 通过统一的接口设计，支持多种主力切换规则和不同交易所的合约管理。
 */
#pragma once  // 防止头文件重复包含
#include "WTSMarcos.h"  // 包含WonderTrader宏定义
#include <vector>  // 包含向量容器
#include <string>  // 包含字符串类型
#include <stdint.h>  // 包含固定大小整数类型

/**
 * @struct _HotSection
 * @brief 主力合约段结构体
 * 
 * 该结构体定义了主力合约段的基本信息，包括分月代码、开始日期、结束日期和切换因子。
 * 用于描述主力合约在特定时间段内的分月合约信息。
 */
typedef struct _HotSection
{
	std::string	_code;  // 分月合约代码
	uint32_t	_s_date;  // 开始日期，格式为YYYYMMDD
	uint32_t	_e_date;  // 结束日期，格式为YYYYMMDD
	double		_factor;  // 切换因子，用于价格调整

	/**
	 * @brief 构造函数
	 * @param code 分月合约代码
	 * @param sdate 开始日期
	 * @param edate 结束日期
	 * @param factor 切换因子
	 * 
	 * 初始化主力合约段结构体，设置分月代码、开始日期、结束日期和切换因子。
	 */
	_HotSection(const char* code, uint32_t sdate, uint32_t edate, double factor)
		: _s_date(sdate), _e_date(edate), _code(code),_factor(factor)  // 初始化成员变量
	{
	
	}

} HotSection;  // 主力合约段结构体类型定义

/**
 * @typedef HotSections
 * @brief 主力合约段集合类型
 * 
 * 该类型定义了主力合约段的集合，使用std::vector容器存储多个HotSection对象。
 * 用于管理多个主力合约段的信息。
 */
typedef std::vector<HotSection>	HotSections;  // 主力合约段集合类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * @brief 主力合约市场标识
 * 
 * 定义了主力合约市场的标识常量，用于区分不同的市场类型。
 */
#define HOTS_MARKET		"HOTS_MARKET"  // 主力合约市场标识
#define SECONDS_MARKET	"SECONDS_MARKET"  // 次主力合约市场标识

/**
 * @class IHotMgr
 * @brief 主力合约管理器接口类
 * 
 * 该类定义了WonderTrader框架中主力合约管理器的核心接口，负责管理期货合约的主力切换规则。
 * 包括主力合约、次主力合约和自定义主力合约的管理，支持分月代码查询、切换判断、段分割等功能。
 * 
 * 主要特性：
 * - 主力合约和次主力合约的分月代码查询
 * - 主力合约和次主力合约的切换判断
 * - 主力段分割功能，支持指定时段的分月合约提取
 * - 自定义主力合约规则的管理和查询
 * - 规则标签和切换因子的查询功能
 * - 统一的接口设计，支持多种主力切换规则
 */
class IHotMgr
{
public:
	/**
	 * @brief 获取分月代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 日期（交易日），格式为YYYYMMDD
	 * @return const char* 返回分月代码
	 * 
	 * 该函数根据交易所、品种代码和日期获取对应的分月代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getRawCode(const char* exchg, const char* pid, uint32_t dt)	= 0;  // 纯虚函数：获取分月代码

	/**
	 * @brief 获取主力对应的上一个分月代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 日期（交易日），格式为YYYYMMDD
	 * @return const char* 返回上一个主力合约的分月代码
	 * 
	 * 该函数获取主力对应的上一个分月代码，即上一个主力合约的分月代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getPrevRawCode(const char* exchg, const char* pid, uint32_t dt) = 0;  // 纯虚函数：获取主力对应的上一个分月代码

	/**
	 * @brief 判断是否为主力合约
	 * @param exchg 交易所代码
	 * @param rawCode 分月代码
	 * @param dt 日期（交易日），格式为YYYYMMDD
	 * @return bool 是主力合约返回true，否则返回false
	 * 
	 * 该函数判断指定分月代码在指定日期是否为主力合约。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		isHot(const char* exchg, const char* rawCode, uint32_t dt) = 0;  // 纯虚函数：判断是否为主力合约

	/**
	 * @brief 分割主力段
	 * @param exchg 交易所代码
	 * @param hotCode 主力合约代码
	 * @param sDt 开始日期，格式为YYYYMMDD
	 * @param eDt 结束日期，格式为YYYYMMDD
	 * @param sections 输出参数，主力段集合
	 * @return bool 分割成功返回true，失败返回false
	 * 
	 * 该函数分割主力段，将主力合约在某个时段的分月合约全部提取出来。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		splitHotSecions(const char* exchg, const char* hotCode, uint32_t sDt, uint32_t eDt, HotSections& sections) = 0;  // 纯虚函数：分割主力段

	/**
	 * @brief 获取次主力分月代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 日期（交易日），格式为YYYYMMDD
	 * @return const char* 返回次主力分月代码
	 * 
	 * 该函数根据交易所、品种代码和日期获取对应的次主力分月代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getSecondRawCode(const char* exchg, const char* pid, uint32_t dt) = 0;  // 纯虚函数：获取次主力分月代码

	/**
	 * @brief 获取次主力对应的上一个分月代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 日期（交易日），格式为YYYYMMDD
	 * @return const char* 返回上一个次主力合约的分月代码
	 * 
	 * 该函数获取次主力对应的上一个分月代码，即上一个次主力合约的分月代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getPrevSecondRawCode(const char* exchg, const char* pid, uint32_t dt) = 0;  // 纯虚函数：获取次主力对应的上一个分月代码

	/**
	 * @brief 判断是否为次主力合约
	 * @param exchg 交易所代码
	 * @param rawCode 分月代码
	 * @param dt 日期（交易日），格式为YYYYMMDD
	 * @return bool 是次主力合约返回true，否则返回false
	 * 
	 * 该函数判断指定分月代码在指定日期是否为次主力合约。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		isSecond(const char* exchg, const char* rawCode, uint32_t dt) = 0;  // 纯虚函数：判断是否为次主力合约

	/**
	 * @brief 分割次主力段
	 * @param exchg 交易所代码
	 * @param hotCode 次主力合约代码
	 * @param sDt 开始日期，格式为YYYYMMDD
	 * @param eDt 结束日期，格式为YYYYMMDD
	 * @param sections 输出参数，次主力段集合
	 * @return bool 分割成功返回true，失败返回false
	 * 
	 * 该函数分割次主力段，将次主力合约在某个时段的分月合约全部提取出来。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		splitSecondSecions(const char* exchg, const char* hotCode, uint32_t sDt, uint32_t eDt, HotSections& sections) = 0;  // 纯虚函数：分割次主力段

	/**
	 * @brief 获取自定义主力合约的分月代码
	 * @param tag 规则标签
	 * @param fullPid 完整品种代码
	 * @param dt 日期，默认为0（当前日期）
	 * @return const char* 返回自定义主力合约的分月代码
	 * 
	 * 该函数根据规则标签、完整品种代码和日期获取自定义主力合约的分月代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getCustomRawCode(const char* tag, const char* fullPid, uint32_t dt = 0) = 0;  // 纯虚函数：获取自定义主力合约的分月代码

	/**
	 * @brief 获取自定义连续合约的上一期主力分月代码
	 * @param tag 规则标签
	 * @param fullPid 完整品种代码
	 * @param dt 日期，默认为0（当前日期）
	 * @return const char* 返回自定义连续合约的上一期主力分月代码
	 * 
	 * 该函数获取自定义连续合约的上一期主力分月代码。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getPrevCustomRawCode(const char* tag, const char* fullPid, uint32_t dt = 0) = 0;  // 纯虚函数：获取自定义连续合约的上一期主力分月代码

	/**
	 * @brief 判断是否是自定义主力合约
	 * @param tag 规则标签
	 * @param fullCode 完整合约代码
	 * @param d 日期，默认为0（当前日期）
	 * @return bool 是自定义主力合约返回true，否则返回false
	 * 
	 * 该函数判断指定合约代码在指定日期是否为自定义主力合约。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		isCustomHot(const char* tag, const char* fullCode, uint32_t d = 0) = 0;  // 纯虚函数：判断是否是自定义主力合约

	/**
	 * @brief 分割自定义主力段
	 * @param tag 规则标签
	 * @param hotCode 自定义主力合约代码
	 * @param sDt 开始日期，格式为YYYYMMDD
	 * @param eDt 结束日期，格式为YYYYMMDD
	 * @param sections 输出参数，自定义主力段集合
	 * @return bool 分割成功返回true，失败返回false
	 * 
	 * 该函数分割自定义主力段，将自定义主力合约在某个时段的分月合约全部提取出来。
	 * 纯虚函数，子类必须实现。
	 */
	virtual bool		splitCustomSections(const char* tag, const char* hotCode, uint32_t sDt, uint32_t eDt, HotSections& sections) = 0;  // 纯虚函数：分割自定义主力段

	/**
	 * @brief 根据标准合约代码获取规则标签
	 * @param stdCode 标准合约代码
	 * @return const char* 返回规则标签
	 * 
	 * 该函数根据标准合约代码获取对应的规则标签，用于确定合约使用的主力切换规则。
	 * 纯虚函数，子类必须实现。
	 */
	virtual const char* getRuleTag(const char* stdCode) = 0;  // 纯虚函数：根据标准合约代码获取规则标签

	/**
	 * @brief 获取规则切换因子
	 * @param ruleTag 规则标签
	 * @param fullPid 完整品种代码
	 * @param uDate 日期，默认为0（当前日期）
	 * @return double 返回规则切换因子
	 * 
	 * 该函数根据规则标签、完整品种代码和日期获取规则切换因子，用于价格调整。
	 * 纯虚函数，子类必须实现。
	 */
	virtual double		getRuleFactor(const char* ruleTag, const char* fullPid, uint32_t uDate = 0) = 0;  // 纯虚函数：获取规则切换因子
};

NS_WTP_END  // 结束WonderTrader命名空间
