/**
 * @file LoaderDef.hpp
 * @brief 数据加载器定义文件
 * 
 * 本文件定义了WonderTrader框架中数据加载器相关的数据结构和类型。
 * 主要用于定义合约信息、商品信息等基础数据结构，为数据加载和解析提供基础支持。
 * 
 * 主要包含：
 * 1. Contract结构体：定义单个合约的详细信息
 * 2. Commodity结构体：定义商品的基本属性
 * 3. 相关的映射类型定义：用于管理合约和商品信息
 */
#pragma once
#include "WTSTypes.h"

#include <map>

USING_NS_WTP;

/**
 * @brief 合约信息结构体
 * 
 * 该结构体定义了单个合约的详细信息，包括合约代码、交易所、名称、产品类型等。
 * 还包含交易限制、保证金比例、期权特性等交易相关的参数。
 */
typedef struct _Contract
{
	std::string	m_strCode;           // 合约代码
	std::string	m_strExchg;          // 交易所代码
	std::string	m_strName;           // 合约名称
	std::string	m_strProduct;        // 产品类型

	uint32_t	m_maxMktQty;         // 最大市价单数量
	uint32_t	m_maxLmtQty;         // 最大限价单数量
	uint32_t	m_minMktQty;         // 最小市价单数量
	uint32_t	m_minLmtQty;         // 最小限价单数量

	uint32_t	m_uOpenDate;         // 上市日期
	uint32_t	m_uExpireDate;       // 到期日期

	double		m_dLongMarginRatio;  // 多头保证金比例
	double		m_dShortMarginRatio; // 空头保证金比例

	OptionType	m_optType;           // 期权类型
	std::string m_strUnderlying;     // 标的合约代码
	double		m_strikePrice;       // 行权价格
	double		m_dUnderlyingScale;  // 标的比例
} Contract;

/**
 * @brief 合约信息映射类型
 * 
 * 使用合约代码作为键，存储合约信息对象的映射容器。
 */
typedef std::map<std::string, Contract> ContractMap;

/**
 * @brief 商品信息结构体
 * 
 * 该结构体定义了商品的基本属性，包括名称、交易所、产品类型、货币等。
 * 还包含交易规则、价格精度、交易模式等交易相关的参数。
 */
typedef struct _Commodity
{
	std::string	m_strName;           // 商品名称
	std::string	m_strExchg;          // 交易所代码
	std::string	m_strProduct;        // 产品类型
	std::string	m_strCurrency;       // 货币类型
	std::string m_strSession;        // 交易时间模板

	uint32_t	m_uVolScale;         // 数量比例
	double		m_fPriceTick;        // 价格变动单位
	uint32_t	m_uPrecision;        // 价格精度

	ContractCategory	m_ccCategory;    // 合约类别
	CoverMode			m_coverMode;    // 平仓模式
	PriceMode			m_priceMode;    // 价格模式
	TradingMode			m_tradeMode;    // 交易模式

} Commodity;

/**
 * @brief 商品信息映射类型
 * 
 * 使用商品代码作为键，存储商品信息对象的映射容器。
 */
typedef std::map<std::string, Commodity> CommodityMap;