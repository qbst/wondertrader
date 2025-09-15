/*!
 * \file WTSHotMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader主力合约管理器
 * 
 * 本文件定义了WonderTrader框架中的主力合约管理系统，负责管理期货合约的主力切换规则。
 * 主力合约是指成交量和持仓量最大的合约，通常是投资者关注和交易的重点。
 * 
 * 主要功能：
 * - 管理主力合约和次主力合约的切换规则
 * - 支持自定义主力切换规则配置
 * - 提供主力合约历史分段查询功能
 * - 支持复权因子计算，用于连续合约数据处理
 * - 提供主力合约判断和查询接口
 * 
 * 设计理念：
 * - 支持多种主力规则（HOT主力、2ND次主力、自定义规则）
 * - 基于时间序列的主力切换管理
 * - 提供灵活的规则扩展机制
 */
#pragma once
#include "../Includes/IHotMgr.h"         // 主力管理器接口定义
#include "../Includes/FasterDefs.h"      // 高性能数据结构定义
#include "../Includes/WTSCollection.hpp" // WTS集合类定义
#include <string>                        // 标准字符串库

NS_WTP_BEGIN
	class WTSSwitchItem;  // 主力切换项前置声明
NS_WTP_END

USING_NS_WTP;

/**
 * @name 主力合约映射类型定义
 * @brief 用于构建主力合约管理的数据结构类型
 * @{
 */

/// 换月主力映射：日期 -> 主力切换项，用于按时间序列管理主力切换
typedef WTSMap<uint32_t>			WTSDateHotMap;

/// 品种主力映射：品种代码 -> 日期映射，用于管理单个品种的主力历史
typedef WTSHashMap<std::string>		WTSProductHotMap;

/// 分市场主力映射：交易所代码 -> 品种映射，用于按交易所分类管理
typedef WTSHashMap<std::string>		WTSExchgHotMap;

/// 自定义切换规则映射：规则标签 -> 品种映射，支持多种自定义主力规则
typedef WTSHashMap<std::string>		WTSCustomSwitchMap;

/** @} */

/**
 * @class WTSHotMgr
 * @brief WonderTrader主力合约管理器实现类
 * 
 * WTSHotMgr是IHotMgr接口的具体实现，负责管理期货市场中的主力合约切换规则。
 * 该类支持多种主力规则的同时管理，包括标准的主力合约、次主力合约以及用户自定义的规则。
 * 
 * 核心功能：
 * - 加载和管理主力切换配置文件
 * - 提供主力合约查询和判断功能
 * - 支持历史主力分段查询
 * - 计算复权因子用于连续合约处理
 * - 支持多种自定义主力规则
 * 
 * 数据结构：
 * - 使用时间序列映射管理主力切换历史
 * - 支持按交易所、品种、规则标签的多层级组织
 * - 提供高效的主力查询和判断算法
 */
class WTSHotMgr : public IHotMgr
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化主力管理器，设置初始状态。
	 */
	WTSHotMgr();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理资源，释放内存。
	 */
	~WTSHotMgr();

public:
	/**
	 * @name 配置加载接口
	 * @brief 用于加载各种主力规则配置文件
	 * @{
	 */
	
	/**
	 * @brief 加载主力合约配置
	 * @param filename 主力配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 加载标准的主力合约切换规则配置文件，通常包含各个品种的
	 * 主力合约切换时间和对应的合约代码。
	 */
	bool loadHots(const char* filename);
	
	/**
	 * @brief 加载次主力合约配置
	 * @param filename 次主力配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 加载次主力合约切换规则配置文件，次主力通常是成交量
	 * 和持仓量排第二的合约。
	 */
	bool loadSeconds(const char* filename);
	
	/**
	 * @brief 释放所有资源
	 * 
	 * 清理所有加载的主力规则数据，释放相关内存资源。
	 * 通常在程序退出或重新初始化时调用。
	 */
	void release();

	/**
	 * @brief 加载自定义主力规则
	 * @param tag 规则标签，用于标识不同的自定义规则
	 * @param filename 自定义规则配置文件路径
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 支持加载用户自定义的主力切换规则，可以同时加载多套规则，
	 * 通过tag参数进行区分。
	 */
	bool loadCustomRules(const char* tag, const char* filename);

	/**
	 * @brief 检查管理器是否已初始化
	 * @return bool 已初始化返回true，否则返回false
	 * 
	 * 内联函数，用于快速检查主力管理器的初始化状态。
	 */
	inline bool isInitialized() const {return m_bInitialized;}
	
	/** @} */

	/**
	 * @name IHotMgr接口实现
	 * @brief 实现主力管理器的核心接口方法
	 * @{
	 */
	
	/**
	 * @brief 获取标准合约代码对应的规则标签
	 * @param stdCode 标准合约代码
	 * @return const char* 规则标签字符串，如果未找到返回空字符串
	 * 
	 * 根据标准合约代码查找对应的主力规则标签，用于确定
	 * 该合约使用哪种主力切换规则。
	 */
	virtual const char* getRuleTag(const char* stdCode) override;

	/**
	 * @brief 获取规则复权因子
	 * @param ruleTag 规则标签
	 * @param fullPid 完整品种代码（格式：交易所.品种）
	 * @param uDate 查询日期，默认为0表示最新
	 * @return double 复权因子值，用于价格复权计算
	 * 
	 * 获取指定规则和品种在特定日期的复权因子，用于处理
	 * 主力切换时的价格连续性问题。
	 */
	virtual double		getRuleFactor(const char* ruleTag, const char* fullPid, uint32_t uDate  = 0 ) override;

	/**
	 * @name 主力合约接口
	 * @brief 标准主力合约相关的查询和操作接口
	 * @{
	 */
	
	/**
	 * @brief 获取主力合约的原始代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 查询日期，默认为0表示当前日期
	 * @return const char* 主力合约的原始代码
	 * 
	 * 根据交易所、品种和日期获取对应的主力合约代码。
	 */
	virtual const char* getRawCode(const char* exchg, const char* pid, uint32_t dt = 0) override;

	/**
	 * @brief 获取上一期主力合约的原始代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 查询日期，默认为0表示当前日期
	 * @return const char* 上一期主力合约的原始代码
	 * 
	 * 获取指定日期前一期的主力合约代码，用于主力切换分析。
	 */
	virtual const char* getPrevRawCode(const char* exchg, const char* pid, uint32_t dt = 0) override;

	/**
	 * @brief 判断合约是否为主力合约
	 * @param exchg 交易所代码
	 * @param rawCode 原始合约代码
	 * @param dt 查询日期，默认为0表示当前日期
	 * @return bool 是主力合约返回true，否则返回false
	 * 
	 * 判断指定合约在给定日期是否为主力合约。
	 */
	virtual bool	isHot(const char* exchg, const char* rawCode, uint32_t dt = 0) override;

	/**
	 * @brief 分割主力合约历史段
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param sDt 开始日期
	 * @param eDt 结束日期
	 * @param sections 输出的主力段列表
	 * @return bool 分割成功返回true，否则返回false
	 * 
	 * 将指定时间段内的主力合约历史分割成不同的段，
	 * 每段对应一个主力合约的生效期间。
	 */
	virtual bool	splitHotSecions(const char* exchg, const char* pid, uint32_t sDt, uint32_t eDt, HotSections& sections) override;
	
	/** @} */

	/**
	 * @name 次主力合约接口
	 * @brief 次主力合约相关的查询和操作接口
	 * @{
	 */
	
	/**
	 * @brief 获取次主力合约的原始代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 查询日期，默认为0表示当前日期
	 * @return const char* 次主力合约的原始代码
	 */
	virtual const char* getSecondRawCode(const char* exchg, const char* pid, uint32_t dt = 0) override;

	/**
	 * @brief 获取上一期次主力合约的原始代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @param dt 查询日期，默认为0表示当前日期
	 * @return const char* 上一期次主力合约的原始代码
	 */
	virtual const char* getPrevSecondRawCode(const char* exchg, const char* pid, uint32_t dt = 0) override;

	/**
	 * @brief 判断合约是否为次主力合约
	 * @param exchg 交易所代码
	 * @param rawCode 原始合约代码
	 * @param dt 查询日期，默认为0表示当前日期
	 * @return bool 是次主力合约返回true，否则返回false
	 */
	virtual bool		isSecond(const char* exchg, const char* rawCode, uint32_t dt = 0) override;

	/**
	 * @brief 分割次主力合约历史段
	 * @param exchg 交易所代码
	 * @param hotCode 主力合约代码
	 * @param sDt 开始日期
	 * @param eDt 结束日期
	 * @param sections 输出的次主力段列表
	 * @return bool 分割成功返回true，否则返回false
	 */
	virtual bool		splitSecondSecions(const char* exchg, const char* hotCode, uint32_t sDt, uint32_t eDt, HotSections& sections) override;
	
	/** @} */

	/**
	 * @name 自定义规则接口
	 * @brief 支持用户自定义主力规则的通用接口
	 * @{
	 */
	
	/**
	 * @brief 获取自定义规则的原始合约代码
	 * @param tag 规则标签
	 * @param fullPid 完整品种代码
	 * @param dt 查询日期
	 * @return const char* 自定义规则对应的原始合约代码
	 */
	virtual const char* getCustomRawCode(const char* tag, const char* fullPid, uint32_t dt) override;

	/**
	 * @brief 获取自定义规则的上一期原始合约代码
	 * @param tag 规则标签
	 * @param fullPid 完整品种代码
	 * @param dt 查询日期
	 * @return const char* 上一期的原始合约代码
	 */
	virtual const char* getPrevCustomRawCode(const char* tag, const char* fullPid, uint32_t dt) override;

	/**
	 * @brief 判断合约是否符合自定义主力规则
	 * @param tag 规则标签
	 * @param fullCode 完整合约代码
	 * @param dt 查询日期
	 * @return bool 符合规则返回true，否则返回false
	 */
	virtual bool		isCustomHot(const char* tag, const char* fullCode, uint32_t dt) override;

	/**
	 * @brief 分割自定义规则的主力历史段
	 * @param tag 规则标签
	 * @param fullPid 完整品种代码
	 * @param sDt 开始日期
	 * @param eDt 结束日期
	 * @param sections 输出的主力段列表
	 * @return bool 分割成功返回true，否则返回false
	 */
	virtual bool		splitCustomSections(const char* tag, const char* fullPid, uint32_t sDt, uint32_t eDt, HotSections& sections) override;
	
	/** @} */
	
	/** @} */


private:
	/**
	 * @name 私有成员变量
	 * @brief 主力管理器的内部数据存储
	 * @{
	 */
	
	// 以下为历史遗留的注释代码，保留用于参考
	//WTSExchgHotMap*	m_pExchgHotMap;   ///< 历史版本：按交易所组织的主力映射
	//WTSExchgHotMap*	m_pExchgScndMap;  ///< 历史版本：按交易所组织的次主力映射
	//wt_hashset<std::string>	m_curHotCodes;    ///< 历史版本：当前主力合约代码集合
	//wt_hashset<std::string>	m_curSecCodes;    ///< 历史版本：当前次主力合约代码集合
	
	/// 管理器初始化状态标志
	bool			m_bInitialized;

	/// 自定义主力切换规则映射表，存储所有自定义规则配置
	WTSCustomSwitchMap*	m_mapCustRules;
	
	/// 自定义切换代码类型定义：规则标签 -> 合约代码集合
	typedef wt_hashmap<std::string, wt_hashset<std::string>>	CustomSwitchCodes;
	
	/// 自定义规则对应的当前主力合约代码集合，用于快速判断
	CustomSwitchCodes	m_mapCustCodes;
	
	/** @} */
};

