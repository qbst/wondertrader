/*!
 * \file WTSContractInfo.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader品种信息和合约信息定义文件
 * 
 * 本文件定义了WonderTrader框架中品种信息和合约信息的核心数据结构。
 * 品种信息包含商品的基本属性，如名称、交易所、产品类型、货币等。
 * 合约信息包含具体合约的详细信息，如合约代码、交易所、名称、产品类型等。
 * 
 * 主要功能：
 * 1. 品种信息管理：商品基本属性、交易规则、手续费率等
 * 2. 合约信息管理：合约代码、交易所、上市日期、到期日期等
 * 3. 期权特性支持：期权类型、标的合约、行权价格等
 * 4. 交易规则配置：保证金比例、数量限制、价格精度等
 */
#pragma once
#include "WTSObject.hpp"    // WonderTrader对象基类
#include "WTSTypes.h"        // WonderTrader类型定义
#include "FasterDefs.h"      // 快速定义文件
#include <string>             // 字符串支持
#include <sstream>            // 字符串流支持

NS_WTP_BEGIN
class WTSSessionInfo;        // 交易时间模板信息类

/**
 * @brief 品种信息类
 * 
 * 该类定义了商品的基本属性，包括名称、交易所、产品类型、货币等。
 * 还包含交易规则、价格精度、交易模式等交易相关的参数。
 * 品种信息是合约信息的基础，一个品种可以包含多个合约。
 */
class WTSCommodityInfo: public WTSObject
{
public:
	/**
	 * @brief 创建品种信息对象
	 * @param pid 产品ID
	 * @param name 品种名称
	 * @param exchg 交易所代码
	 * @param session 交易时间模板
	 * @param trdtpl 交易模板
	 * @param currency 货币类型，默认为CNY
	 * @return 新创建的品种信息对象指针
	 * 
	 * 静态工厂方法，用于创建新的品种信息对象实例。
	 * 会自动生成完整的品种ID，格式为"交易所.产品ID"。
	 */
	static WTSCommodityInfo* create(const char* pid, const char* name, const char* exchg, const char* session, const char* trdtpl, const char* currency = "CNY")
	{
		WTSCommodityInfo* ret = new WTSCommodityInfo;
		ret->m_strName = name;
		ret->m_strExchg = exchg;
		ret->m_strProduct = pid;
		ret->m_strCurrency = currency;
		ret->m_strSession = session;
		ret->m_strTrdTpl = trdtpl;

		std::stringstream ss;
		ss << exchg << "." << pid;
		ret->m_strFullPid = ss.str();

		return ret;
	}

	/**
	 * @brief 设置数量比例
	 * @param volScale 数量比例
	 */
	inline void	setVolScale(uint32_t volScale){ m_uVolScale = volScale; }
	
	/**
	 * @brief 设置价格变动单位
	 * @param pxTick 价格变动单位
	 */
	inline void	setPriceTick(double pxTick){ m_dPriceTick = pxTick; }
	
	/**
	 * @brief 设置合约类别
	 * @param cat 合约类别
	 */
	inline void	setCategory(ContractCategory cat){ m_ccCategory = cat; }
	
	/**
	 * @brief 设置平仓模式
	 * @param cm 平仓模式
	 */
	inline void	setCoverMode(CoverMode cm){ m_coverMode = cm; }
	
	/**
	 * @brief 设置价格模式
	 * @param pm 价格模式
	 */
	inline void	setPriceMode(PriceMode pm){ m_priceMode = pm; }
	
	/**
	 * @brief 设置交易模式
	 * @param tm 交易模式
	 */
	inline void	setTradingMode(TradingMode tm) { m_tradeMode = tm; }

	/**
	 * @brief 检查是否可以做空
	 * @return 可以做空返回true，否则返回false
	 */
	inline bool canShort() const { return m_tradeMode == TM_Both; }
	
	/**
	 * @brief 检查是否为T+1交易
	 * @return 是T+1交易返回true，否则返回false
	 */
	inline bool isT1() const { return m_tradeMode == TM_LongT1; }

	/**
	 * @brief 获取品种名称
	 * @return 品种名称字符串
	 */
	inline const char* getName()	const{ return m_strName.c_str(); }
	
	/**
	 * @brief 获取交易所代码
	 * @return 交易所代码字符串
	 */
	inline const char* getExchg()	const{ return m_strExchg.c_str(); }
	
	/**
	 * @brief 获取产品ID
	 * @return 产品ID字符串
	 */
	inline const char* getProduct()	const{ return m_strProduct.c_str(); }
	
	/**
	 * @brief 获取货币类型
	 * @return 货币类型字符串
	 */
	inline const char* getCurrency()	const{ return m_strCurrency.c_str(); }
	
	/**
	 * @brief 获取交易时间模板
	 * @return 交易时间模板字符串
	 */
	inline const char* getSession()	const{ return m_strSession.c_str(); }
	
	/**
	 * @brief 获取交易模板
	 * @return 交易模板字符串
	 */
	inline const char* getTradingTpl()	const{ return m_strTrdTpl.c_str(); }
	
	/**
	 * @brief 获取完整品种ID
	 * @return 完整品种ID字符串，格式为"交易所.产品ID"
	 */
	inline const char* getFullPid()	const{ return m_strFullPid.c_str(); }

	/**
	 * @brief 获取数量比例
	 * @return 数量比例
	 */
	inline uint32_t	getVolScale()	const{ return m_uVolScale; }
	
	/**
	 * @brief 获取价格变动单位
	 * @return 价格变动单位
	 */
	inline double	getPriceTick()	const{ return m_dPriceTick; }
	//inline uint32_t	getPrecision()	const{ return m_uPrecision; }

	/**
	 * @brief 获取合约类别
	 * @return 合约类别枚举值
	 */
	inline ContractCategory		getCategoty() const{ return m_ccCategory; }
	
	/**
	 * @brief 获取平仓模式
	 * @return 平仓模式枚举值
	 */
	inline CoverMode			getCoverMode() const{ return m_coverMode; }
	
	/**
	 * @brief 获取价格模式
	 * @return 价格模式枚举值
	 */
	inline PriceMode			getPriceMode() const{ return m_priceMode; }
	
	/**
	 * @brief 获取交易模式
	 * @return 交易模式枚举值
	 */
	inline TradingMode			getTradingMode() const { return m_tradeMode; }

	/**
	 * @brief 添加合约代码
	 * @param code 合约代码
	 */
	inline void		addCode(const char* code){ m_setCodes.insert(code); }
	
	/**
	 * @brief 获取所有合约代码
	 * @return 合约代码集合的常量引用
	 */
	inline const CodeSet& getCodes() const{ return m_setCodes; }

	/**
	 * @brief 设置数量变动单位
	 * @param lotsTick 数量变动单位
	 */
	inline void	setLotsTick(double lotsTick){ m_dLotTick = lotsTick; }
	
	/**
	 * @brief 设置最小数量
	 * @param minLots 最小数量
	 */
	inline void	setMinLots(double minLots) { m_dMinLots = minLots; }

	/**
	 * @brief 检查是否为期权
	 * @return 是期权返回true，否则返回false
	 */
	inline bool isOption() const
	{
		return (m_ccCategory == CC_FutOption || m_ccCategory == CC_ETFOption || m_ccCategory == CC_SpotOption);
	}

	/**
	 * @brief 检查是否为期货
	 * @return 是期货返回true，否则返回false
	 */
	inline bool isFuture() const
	{
		return m_ccCategory == CC_Future;
	}

	/**
	 * @brief 检查是否为股票
	 * @return 是股票返回true，否则返回false
	 */
	inline bool isStock() const
	{
		return m_ccCategory == CC_Stock;
	}

	/**
	 * @brief 获取数量变动单位
	 * @return 数量变动单位
	 */
	inline double	getLotsTick() const { return m_dLotTick; }
	
	/**
	 * @brief 获取最小数量
	 * @return 最小数量
	 */
	inline double	getMinLots() const { return m_dMinLots; }

	/**
	 * @brief 设置交易时间模板信息
	 * @param sInfo 交易时间模板信息对象指针
	 */
	inline void		setSessionInfo(WTSSessionInfo* sInfo) { m_pSession = sInfo; }
	
	/**
	 * @brief 获取交易时间模板信息
	 * @return 交易时间模板信息对象指针
	 */
	inline WTSSessionInfo* getSessionInfo() const { return m_pSession; }

	/**
	 * @brief 设置手续费率
	 * @param open 开仓手续费率
	 * @param close 平仓手续费率
	 * @param closeToday 平今手续费率
	 * @param byVolume 是否按手数计算，true为按手数，false为按金额
	 */
	inline void		setFeeRates(double open, double close, double closeToday, bool byVolume)
	{
		m_dOpenFee = open;
		m_dCloseFee = close;
		m_dCloseTFee = closeToday;
		m_nFeeAlg = byVolume ? 0 : 1;
	}

	/**
	 * @brief 设置保证金比例
	 * @param rate 保证金比例
	 */
	inline void		setMarginRate(double rate) { m_dMarginRate = rate; }
	
	/**
	 * @brief 获取保证金比例
	 * @return 保证金比例
	 */
	inline double	getMarginRate() const { return m_dMarginRate; }

	/**
	 * @brief 计算手续费
	 * @param price 价格
	 * @param qty 数量
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @return 计算出的手续费
	 * 
	 * 根据手续费算法和开平标志计算手续费。
	 * 支持按手数和按金额两种计算方式。
	 */
	inline double	calcFee(double price, double qty, uint32_t offset)
	{
		if (m_nFeeAlg == -1)
			return 0.0;

		double ret = 0.0;
		if (m_nFeeAlg == 0)
		{
			switch (offset)
			{
			case 0: ret = m_dOpenFee * qty; break;      // 开仓手续费
			case 1: ret = m_dCloseFee * qty; break;     // 平仓手续费
			case 2: ret = m_dCloseTFee * qty; break;    // 平今手续费
			default: ret = 0.0; break;
			}
		}
		else if(m_nFeeAlg == 1)
		{
			double amount = price * qty * m_uVolScale;  // 按金额计算
			switch (offset)
			{
			case 0: ret = m_dOpenFee * amount; break;   // 开仓手续费
			case 1: ret = m_dCloseFee * amount; break;  // 平仓手续费
			case 2: ret = m_dCloseTFee * amount; break; // 平今手续费
			default: ret = 0.0; break;
			}
		}

		return (int32_t)(ret * 100 + 0.5) / 100.0;    // 四舍五入到分
	}

protected:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化品种信息对象，设置手续费算法为-1（未设置）。
	 */
	WTSCommodityInfo():m_nFeeAlg(-1){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~WTSCommodityInfo() {}

private:
	std::string	m_strName;		// 品种名称
	std::string	m_strExchg;		// 交易所代码
	std::string	m_strProduct;	// 品种ID
	std::string	m_strCurrency;	// 币种
	std::string m_strSession;	// 交易时间模板
	std::string m_strTrdTpl;	// 节假日模板
	std::string m_strFullPid;	// 全品种ID，如CFFEX.IF

	uint32_t	m_uVolScale;	// 合约放大倍数
	double		m_dPriceTick;	// 最小价格变动单位

	double		m_dLotTick;		// 数量精度
	double		m_dMinLots;		// 最小交易数量

	ContractCategory	m_ccCategory;	// 品种分类，期货、股票、期权等
	CoverMode			m_coverMode;	// 平仓类型
	PriceMode			m_priceMode;	// 价格类型
	TradingMode			m_tradeMode;	// 交易类型

	CodeSet				m_setCodes;     // 合约代码集合

	WTSSessionInfo*		m_pSession;     // 交易时间模板信息

	double	m_dOpenFee;		// 开仓手续费
	double	m_dCloseFee;	// 平仓手续费
	double	m_dCloseTFee;	// 平今手续费
	int		m_nFeeAlg;		// 手续费算法，默认为-1，不计算,0是按成交量，1为按成交额
	double	m_dMarginRate;	// 保证金率
};

/**
 * @brief 合约信息类
 * 
 * 该类定义了具体合约的详细信息，包括合约代码、交易所、名称、产品类型等。
 * 还包含交易限制、保证金比例、期权特性等交易相关的参数。
 * 合约信息是品种信息的具体化，一个品种可以包含多个合约。
 */
class WTSContractInfo :	public WTSObject
{
public:
	/**
	 * @brief 创建合约信息对象
	 * @param code 合约代码
	 * @param name 合约名称
	 * @param exchg 交易所代码
	 * @param pid 产品ID
	 * @return 新创建的合约信息对象指针
	 * 
	 * 静态工厂方法，用于创建新的合约信息对象实例。
	 * 会自动生成完整的合约代码和品种ID，格式为"交易所.代码"。
	 */
	static WTSContractInfo* create(const char* code, const char* name, const char* exchg, const char* pid)
	{
		WTSContractInfo* ret = new WTSContractInfo;
		ret->m_strCode = code;
		ret->m_strName = name;
		ret->m_strProduct = pid;
		ret->m_strExchg = exchg;

		std::stringstream ss;
		ss << exchg << "." << code;
		ret->m_strFullCode = ss.str();

		std::stringstream sss;
		sss << exchg << "." << pid;
		ret->m_strFullPid = sss.str();

		return ret;
	}

	/**
	 * @brief 设置数量限制
	 * @param maxMarketVol 最大市价单数量
	 * @param maxLimitVol 最大限价单数量
	 * @param minMarketVol 最小市价单数量，默认为1
	 * @param minLimitVol 最小限价单数量，默认为1
	 */
	inline void	setVolumeLimits(uint32_t maxMarketVol, uint32_t maxLimitVol, uint32_t minMarketVol = 1, uint32_t minLimitVol = 1)
	{
		m_maxMktQty = maxMarketVol;
		m_maxLmtQty = maxLimitVol;

		m_minLmtQty = minLimitVol;
		m_minMktQty = minMarketVol;
	}

	/**
	 * @brief 设置日期信息
	 * @param openDate 上市日期
	 * @param expireDate 到期日期
	 */
	inline void setDates(uint32_t openDate, uint32_t expireDate)
	{
		m_openDate = openDate;
		m_expireDate = expireDate;
	}

	/**
	 * @brief 设置保证金比例
	 * @param longRatio 多头保证金比例
	 * @param shortRatio 空头保证金比例
	 * @param flag 保证金标志，0-使用品种保证金比例，1-使用合约保证金比例
	 */
	inline void setMarginRatios(double longRatio, double shortRatio, uint32_t flag = 0)
	{
		m_lMarginRatio = longRatio;
		m_sMarginRatio = shortRatio;
		m_uMarginFlag = flag;
	}

	/**
	 * @brief 获取合约代码
	 * @return 合约代码字符串
	 */
	inline const char* getCode()	const{return m_strCode.c_str();}
	
	/**
	 * @brief 获取交易所代码
	 * @return 交易所代码字符串
	 */
	inline const char* getExchg()	const{return m_strExchg.c_str();}
	
	/**
	 * @brief 获取合约名称
	 * @return 合约名称字符串
	 */
	inline const char* getName()	const{return m_strName.c_str();}
	
	/**
	 * @brief 获取产品ID
	 * @return 产品ID字符串
	 */
	inline const char* getProduct()	const{return m_strProduct.c_str();}

	/**
	 * @brief 获取完整合约代码
	 * @return 完整合约代码字符串，格式为"交易所.代码"
	 */
	inline const char* getFullCode()	const{ return m_strFullCode.c_str(); }
	
	/**
	 * @brief 获取完整品种ID
	 * @return 完整品种ID字符串，格式为"交易所.产品ID"
	 */
	inline const char* getFullPid()	const{ return m_strFullPid.c_str(); }

	/**
	 * @brief 获取最大市价单数量
	 * @return 最大市价单数量
	 */
	inline uint32_t	getMaxMktVol() const{ return m_maxMktQty; }
	
	/**
	 * @brief 获取最大限价单数量
	 * @return 最大限价单数量
	 */
	inline uint32_t	getMaxLmtVol() const{ return m_maxLmtQty; }
	
	/**
	 * @brief 获取最小市价单数量
	 * @return 最小市价单数量
	 */
	inline uint32_t	getMinMktVol() const { return m_minMktQty; }
	
	/**
	 * @brief 获取最小限价单数量
	 * @return 最小限价单数量
	 */
	inline uint32_t	getMinLmtVol() const { return m_minLmtQty; }

	/**
	 * @brief 获取上市日期
	 * @return 上市日期
	 */
	inline uint32_t	getOpenDate() const { return m_openDate; }
	
	/**
	 * @brief 获取到期日期
	 * @return 到期日期
	 */
	inline uint32_t	getExpireDate() const { return m_expireDate; }

	/**
	 * @brief 获取多头保证金比例
	 * @return 多头保证金比例
	 * 
	 * 如果保证金标志为1，则使用合约保证金比例；
	 * 否则使用品种保证金比例。
	 */
	inline double	getLongMarginRatio() const { 
		if (m_uMarginFlag == 1)
			return m_lMarginRatio;

		static double commRate = m_commInfo->getMarginRate();
		return commRate == 0.0 ? m_lMarginRatio : m_commInfo->getMarginRate();
	}

	/**
	 * @brief 获取空头保证金比例
	 * @return 空头保证金比例
	 * 
	 * 如果保证金标志为1，则使用合约保证金比例；
	 * 否则使用品种保证金比例。
	 */
	inline double	getShortMarginRatio() const {
		if (m_uMarginFlag == 1)
			return m_sMarginRatio;

		static double commRate = m_commInfo->getMarginRate();
		return commRate == 0.0 ? m_sMarginRatio : m_commInfo->getMarginRate();
	}

	/**
	 * @brief 设置品种信息
	 * @param commInfo 品种信息对象指针
	 */
	inline void setCommInfo(WTSCommodityInfo* commInfo) { m_commInfo = commInfo; }
	
	/**
	 * @brief 获取品种信息
	 * @return 品种信息对象指针
	 */
	inline WTSCommodityInfo* getCommInfo() const { return m_commInfo; }

	/**
	 * @brief 设置手续费率
	 * @param open 开仓手续费率
	 * @param close 平仓手续费率
	 * @param closeToday 平今手续费率
	 * @param byVolume 是否按手数计算，true为按手数，false为按金额
	 */
	inline void		setFeeRates(double open, double close, double closeToday, bool byVolume)
	{
		m_dOpenFee = open;
		m_dCloseFee = close;
		m_dCloseTFee = closeToday;
		m_nFeeAlg = byVolume ? 0 : 1;
	}

	/**
	 * @brief 计算手续费
	 * @param price 价格
	 * @param qty 数量
	 * @param offset 开平标志：0-开仓，1-平仓，2-平今
	 * @return 计算出的手续费
	 * 
	 * 如果合约没有手续费率，则调用品种的手续费率。
	 * 根据手续费算法和开平标志计算手续费。
	 */
	inline double	calcFee(double price, double qty, uint32_t offset)
	{
		// 如果合约没有手续费率，则调用品种的手续费率
		if (m_nFeeAlg == -1)
			return m_commInfo->calcFee(price, qty, offset);

		double ret = 0.0;
		if (m_nFeeAlg == 0)
		{
			switch (offset)
			{
			case 0: ret = m_dOpenFee * qty; break;      // 开仓手续费
			case 1: ret = m_dCloseFee * qty; break;     // 平仓手续费
			case 2: ret = m_dCloseTFee * qty; break;    // 平今手续费
			default: ret = 0.0; break;
			}
		}
		else if (m_nFeeAlg == 1)
		{
			double amount = price * qty * m_commInfo->getVolScale();  // 按金额计算
			switch (offset)
			{
			case 0: ret = m_dOpenFee * amount; break;   // 开仓手续费
			case 1: ret = m_dCloseFee * amount; break;  // 平仓手续费
			case 2: ret = m_dCloseTFee * amount; break; // 平今手续费
			default: ret = 0.0; break;
			}
		}

		return (int32_t)(ret * 100 + 0.5) / 100.0;    // 四舍五入到分
	}

	/**
	 * @brief 设置活跃标志
	 * @param hotFlag 活跃标志：0-平仓，1-活跃，2-次活跃
	 * @param hotCode 活跃代码，默认为空
	 */
	inline void setHotFlag(uint32_t hotFlag, const char* hotCode = "") 
	{ 
		m_uHotFlag = hotFlag; 
		m_strHotCode = hotCode;
	}
	
	/**
	 * @brief 检查是否为平仓合约
	 * @return 是平仓合约返回true，否则返回false
	 */
	inline bool isFlat() const { return m_uHotFlag == 0; }
	
	/**
	 * @brief 检查是否为活跃合约
	 * @return 是活跃合约返回true，否则返回false
	 */
	inline bool isHot() const { return m_uHotFlag == 1; }
	
	/**
	 * @brief 检查是否为次活跃合约
	 * @return 是次活跃合约返回true，否则返回false
	 */
	inline bool isSecond() const { return m_uHotFlag == 2; }
	
	/**
	 * @brief 获取活跃代码
	 * @return 活跃代码字符串
	 */
	inline const char* getHotCode() const { return m_strHotCode.c_str(); }

	/**
	 * @brief 设置总索引
	 * @param idx 总索引值
	 */
	inline void	setTotalIndex(uint32_t idx) noexcept { m_uTotalIdx = idx; }
	
	/**
	 * @brief 获取总索引
	 * @return 总索引值
	 */
	inline uint32_t getTotalIndex() const noexcept { return m_uTotalIdx; }

	/**
	 * @brief 设置扩展数据
	 * @param pExtData 扩展数据指针
	 */
	inline void setExtData(void* pExtData) noexcept { m_pExtData = pExtData; }
	
	/**
	 * @brief 获取扩展数据
	 * @return 扩展数据指针，转换为指定类型
	 * 
	 * 模板方法，用于获取扩展数据并转换为指定类型。
	 */
	template<typename T>
	inline T*	getExtData() noexcept { return static_cast<T*>(m_pExtData); }

protected:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化合约信息对象，设置默认值。
	 */
	WTSContractInfo()
		: m_commInfo(NULL), m_openDate(19900101), m_expireDate(30991231)
		, m_lMarginRatio(0), m_sMarginRatio(0), m_nFeeAlg(-1), m_uMarginFlag(0)
		, m_uHotFlag(0), m_uTotalIdx(UINT_MAX), m_pExtData(NULL){}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~WTSContractInfo(){}

private:
	std::string	m_strCode;        // 合约代码
	std::string	m_strExchg;       // 交易所代码
	std::string	m_strName;        // 合约名称
	std::string	m_strProduct;     // 产品ID

	std::string m_strFullPid;     // 完整品种ID
	std::string m_strFullCode;    // 完整合约代码

	uint32_t	m_maxMktQty;     // 最大市价单数量
	uint32_t	m_maxLmtQty;     // 最大限价单数量
	uint32_t	m_minMktQty;     // 最小市价单数量
	uint32_t	m_minLmtQty;     // 最小限价单数量

	uint32_t	m_openDate;		// 上市日期
	uint32_t	m_expireDate;	// 交割日

	double		m_lMarginRatio;	// 交易所多头保证金率
	double		m_sMarginRatio;	// 交易所空头保证金率
	uint32_t	m_uMarginFlag;	// 0-合约信息读取的，1-手工设置的

	double		m_dOpenFee;		// 开仓手续费
	double		m_dCloseFee;	// 平仓手续费
	double		m_dCloseTFee;	// 平今手续费
	int			m_nFeeAlg;		// 手续费算法，默认为-1，不计算,0是按成交量，1为按成交额

	WTSCommodityInfo*	m_commInfo;     // 品种信息对象指针
	uint32_t	m_uHotFlag;       // 活跃标志：0-平仓，1-活跃，2-次活跃
	std::string	m_strHotCode;     // 活跃代码

	uint32_t	m_uTotalIdx;	// 合约全局索引，每次启动可能不同，只能在内存里用
	void*		m_pExtData;		// 扩展数据，主要是绑定一些和合约相关的数据，这样可以避免在很多地方建map，导致多次查找
};


NS_WTP_END