/*!
 * \file WTSTradeDef.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt交易数据对象定义,包括委托、订单、成交、持仓、资金、持仓明细等数据
 * 
 * 设计逻辑与作用：
 * 本文件定义了WonderTrader系统中所有与交易相关的数据结构，包括委托(WTSEntrust)、
 * 委托操作(WTSEntrustAction)、订单信息(WTSOrderInfo)、成交信息(WTSTradeInfo)、
 * 持仓信息(WTSPositionItem)和账户信息(WTSAccountInfo)等核心交易对象。
 * 这些类都继承自WTSPoolObject，支持对象池管理，提高内存使用效率。
 * 每个类都包含了完整的交易信息字段，如合约代码、交易所、价格、数量、方向、状态等，
 * 为整个交易系统提供了统一的数据结构标准，确保交易数据的完整性和一致性。
 */
#pragma once  // 防止头文件被重复包含
#include "WTSObject.hpp"  // 包含WonderTrader对象基类
#include "WTSTypes.h"     // 包含WonderTrader基本类型定义
#include "WTSMarcos.h"    // 包含WonderTrader宏定义

#include <time.h>         // 包含时间相关函数
#include <string>         // 包含标准字符串库
#include <map>            // 包含标准映射容器
#include<chrono>          // 包含时间库

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSContractInfo;  // 前向声明合约信息类

//////////////////////////////////////////////////////////////////////////
//委托数据结构,用户客户端向服务端发起
class WTSEntrust : public WTSPoolObject<WTSEntrust>  // 委托类，继承自对象池基类
{
public:
	// 构造函数，初始化所有成员变量
	WTSEntrust()
		: m_iPrice(0)           // 初始化价格为0
		, m_dVolume(0)           // 初始化数量为0
		, m_businessType(BT_CASH) // 初始化业务类型为现金交易
		, m_direction(WDT_LONG)   // 初始化方向为做多
		, m_priceType(WPT_ANYPRICE) // 初始化价格类型为市价
		, m_orderFlag(WOF_NOR)     // 初始化订单标志为普通订单
		, m_offsetType(WOT_OPEN)   // 初始化开平方向为开仓
		, m_bIsNet(false)          // 初始化净持仓标志为false
		, m_bIsBuy(true)           // 初始化买入标志为true
		, m_pContract(NULL)        // 初始化合约信息指针为NULL
	{
	}

	virtual ~WTSEntrust(){}  // 虚析构函数

public:
	// 静态工厂方法：创建委托对象
	static inline WTSEntrust* create(const char* code, double vol, double price, const char* exchg = "", WTSBusinessType bType = BT_CASH) noexcept
	{
		WTSEntrust* pRet = WTSEntrust::allocate();  // 从对象池分配内存
		if(pRet)  // 如果分配成功
		{
			//wt_strcpy(pRet->m_strExchg, exchg);  // 注释掉的交易所代码复制
			//wt_strcpy(pRet->m_strCode, code);     // 注释掉的合约代码复制

			auto len = strlen(exchg);  // 获取交易所代码长度
			memcpy(pRet->m_strExchg, exchg, len);  // 复制交易所代码
			pRet->m_strExchg[len] = 0;             // 添加字符串结束符

			len = strlen(code);  // 获取合约代码长度
			memcpy(pRet->m_strCode, code, len);    // 复制合约代码
			pRet->m_strCode[len] = 0;              // 添加字符串结束符

			pRet->m_dVolume = vol;           // 设置数量
			pRet->m_iPrice = price;          // 设置价格
			pRet->m_businessType = bType;    // 设置业务类型
			return pRet;                     // 返回创建的对象
		}

		return NULL;  // 分配失败返回NULL
	}

public:
	// 设置交易所代码
	inline void setExchange(const char* exchg, std::size_t len = 0) noexcept
	{
		wt_strcpy(m_strExchg, exchg, len);  // 复制交易所代码到成员变量
    }
	// 设置合约代码
	inline void setCode(const char* code, std::size_t len = 0) noexcept
	{
		wt_strcpy(m_strCode, code, len);    // 复制合约代码到成员变量
    }

	// 设置交易方向（多空）
	inline void setDirection(WTSDirectionType dType)noexcept {m_direction = dType;}
	// 设置价格类型（市价/限价）
	inline void setPriceType(WTSPriceType pType)noexcept {m_priceType = pType;}
	// 设置订单标志（普通/FAK/FOK）
	inline void setOrderFlag(WTSOrderFlag oFlag)noexcept {m_orderFlag = oFlag;}
	// 设置开平方向（开仓/平仓）
	inline void setOffsetType(WTSOffsetType oType)noexcept {m_offsetType = oType;}

	// 获取交易方向（多空）
	inline WTSDirectionType	getDirection() const noexcept {return m_direction;}
	// 获取价格类型（市价/限价）
	inline WTSPriceType		getPriceType() const noexcept {return m_priceType;}
	// 获取订单标志（普通/FAK/FOK）
	inline WTSOrderFlag		getOrderFlag() const noexcept {return m_orderFlag;}
	// 获取开平方向（开仓/平仓）
	inline WTSOffsetType	getOffsetType() const noexcept {return m_offsetType;}

	// 设置业务类型（现金/ETF/期权等）
	inline void setBusinessType(WTSBusinessType bType) noexcept { m_businessType = bType; }
	// 获取业务类型
	inline WTSBusinessType	getBusinessType() const  noexcept { return m_businessType; }

	// 设置交易数量
	inline void setVolume(double volume) noexcept { m_dVolume = volume; }
	// 设置交易价格
	inline void setPrice(double price) noexcept { m_iPrice = price; }

	// 获取交易数量
	inline double getVolume() const noexcept { return m_dVolume; }
	// 获取交易价格
	inline double getPrice() const noexcept { return m_iPrice; }

	// 获取合约代码
	inline const char* getCode() const noexcept { return m_strCode; }
	// 获取交易所代码
	inline const char* getExchg() const  noexcept { return m_strExchg; }

	// 设置委托编号
	inline void setEntrustID(const char* eid) noexcept { wt_strcpy(m_strEntrustID, eid); }
	// 获取委托编号（只读）
	inline const char* getEntrustID() const  noexcept { return m_strEntrustID; }
	// 获取委托编号（可写）
	inline char* getEntrustID() noexcept { return m_strEntrustID; }

	// 设置用户标签
	inline void setUserTag(const char* tag)  noexcept { wt_strcpy(m_strUserTag, tag); }
	// 获取用户标签（只读）
	inline const char* getUserTag() const  noexcept { return m_strUserTag; }
	// 获取用户标签（可写）
	inline char* getUserTag()  noexcept { return m_strUserTag; }

	// 设置净持仓方向和买入标志
	inline void setNetDirection(bool isBuy) noexcept { m_bIsNet = true; m_bIsBuy = isBuy; }
	// 检查是否为净持仓
	inline bool isNet() const  noexcept { return m_bIsNet; }
	// 检查是否为买入方向
	inline bool isBuy() const  noexcept { return m_bIsBuy; }

	// 设置合约信息指针
	inline void setContractInfo(WTSContractInfo* cInfo) noexcept { m_pContract = cInfo; }
	// 获取合约信息指针
	inline WTSContractInfo* getContractInfo() const  noexcept { return m_pContract; }

protected:
	char			m_strExchg[MAX_EXCHANGE_LENGTH];  // 交易所代码字符串
	char			m_strCode[MAX_INSTRUMENT_LENGTH];  // 合约代码字符串
	double			m_dVolume;                        // 交易数量
	double			m_iPrice;                         // 交易价格

	bool			m_bIsNet;                         // 是否为净持仓
	bool			m_bIsBuy;                         // 是否为买入方向

	WTSDirectionType	m_direction;                    // 交易方向（多空）
	WTSPriceType		m_priceType;                    // 价格类型（市价/限价）
	WTSOrderFlag		m_orderFlag;                    // 订单标志（普通/FAK/FOK）
	WTSOffsetType		m_offsetType;                   // 开平方向（开仓/平仓）

	char				m_strEntrustID[64] = { 0 };    // 委托编号字符串
	char				m_strUserTag[64] = { 0 };      // 用户标签字符串

	WTSBusinessType		m_businessType;                 // 业务类型

	WTSContractInfo*	m_pContract;                    // 合约信息指针
};


//////////////////////////////////////////////////////////////////////////
//委托操作: 撤单、改单
class WTSEntrustAction : public WTSPoolObject<WTSEntrustAction>  // 委托操作类，继承自对象池基类
{
public:
	// 构造函数，初始化委托操作对象
	WTSEntrustAction()
		: m_actionFlag(WAF_CANCEL)    // 初始化操作标志为撤销
		, m_businessType(BT_CASH)     // 初始化业务类型为现金交易
	{

	}

	virtual ~WTSEntrustAction(){}  // 虚析构函数

public:
	// 静态工厂方法：创建委托操作对象
	static inline WTSEntrustAction* create(const char* code, const char* exchg = "", WTSBusinessType bType = BT_CASH) noexcept
	{
		WTSEntrustAction* pRet = WTSEntrustAction::allocate();  // 从对象池分配内存
		if(pRet)  // 如果分配成功
		{
			wt_strcpy(pRet->m_strExchg, exchg);  // 复制交易所代码
			wt_strcpy(pRet->m_strCode, code);    // 复制合约代码
			pRet->m_businessType = bType;        // 设置业务类型
			return pRet;                         // 返回创建的对象
		}

		return NULL;  // 分配失败返回NULL
	}

	// 静态工厂方法：创建撤销操作对象
	static inline WTSEntrustAction* createCancelAction(const char* eid, const char* oid) noexcept
	{
		WTSEntrustAction* pRet = new WTSEntrustAction;  // 创建新的委托操作对象
		if(pRet)  // 如果创建成功
		{
			wt_strcpy(pRet->m_strEnturstID, eid);  // 复制委托编号
			wt_strcpy(pRet->m_strOrderID, oid);    // 复制订单编号
			return pRet;                            // 返回创建的对象
		}

		return NULL;  // 创建失败返回NULL
	}

public:
	// 获取交易所代码
	inline const char* getExchg() const  noexcept { return m_strExchg; }
	// 获取合约代码
	inline const char* getCode() const noexcept {return m_strCode;}

	// 设置交易所代码
	inline void setExchange(const char* exchg, std::size_t len = 0) noexcept {
		if (len == 0)  // 如果长度为0，使用默认复制
			wt_strcpy(m_strExchg, exchg);
		else  // 否则使用指定长度复制
			strncpy(m_strExchg, exchg, len);
	}
	// 设置合约代码
	inline void setCode(const char* code, std::size_t len = 0) noexcept {
		if (len == 0)  // 如果长度为0，使用默认复制
			wt_strcpy(m_strCode, code);
		else  // 否则使用指定长度复制
			strncpy(m_strCode, code, len);
	}

	// 设置操作标志（撤销/修改）
	inline void setActionFlag(WTSActionFlag af) noexcept {m_actionFlag = af;}
	// 获取操作标志
	inline WTSActionFlag getActionFlag() const noexcept {return m_actionFlag;}

	// 设置委托编号
	inline void setEntrustID(const char* eid) noexcept { wt_strcpy(m_strEnturstID, eid); }
	// 获取委托编号（只读）
	inline const char* getEntrustID() const noexcept {return m_strEnturstID;}
	// 获取委托编号（可写）
	inline char* getEntrustID() noexcept { return m_strEnturstID; }

	// 设置订单编号
	inline void setOrderID(const char* oid) noexcept { wt_strcpy(m_strOrderID, oid); }
	// 获取订单编号
	inline const char* getOrderID() const noexcept {return m_strOrderID;}

	// 设置业务类型
	inline void setBusinessType(WTSBusinessType bType) noexcept { m_businessType = bType; }
	// 获取业务类型
	inline WTSBusinessType	getBusinessType() const  noexcept { return m_businessType; }

	// 设置用户标签
	inline void setUserTag(const char* tag)  noexcept { wt_strcpy(m_strUserTag, tag); }
	// 获取用户标签（只读）
	inline const char* getUserTag() const  noexcept { return m_strUserTag; }
	// 获取用户标签（可写）
	inline char* getUserTag()  noexcept { return m_strUserTag; }

	// 设置合约信息指针
	inline void setContractInfo(WTSContractInfo* cInfo)  noexcept { m_pContract = cInfo; }
	// 获取合约信息指针
	inline WTSContractInfo* getContractInfo() const  noexcept { return m_pContract; }

protected:
	char			m_strExchg[MAX_EXCHANGE_LENGTH];  // 交易所代码字符串
	char			m_strCode[MAX_INSTRUMENT_LENGTH];  // 合约代码字符串

	char			m_strEnturstID[64] = { 0 };     // 委托编号字符串
	WTSActionFlag	m_actionFlag;                    // 操作标志（撤销/修改）

	char			m_strOrderID[64] = { 0 };       // 订单编号字符串
	char			m_strUserTag[64] = { 0 };       // 用户标签字符串

	WTSBusinessType		m_businessType;                 // 业务类型
	WTSContractInfo*	m_pContract;                    // 合约信息指针
};

//////////////////////////////////////////////////////////////////////////
//订单信息,查看订单状态变化等
class WTSOrderInfo : public WTSPoolObject<WTSOrderInfo>  // 订单信息类，继承自对象池基类
{
public:
	// 构造函数，初始化订单信息对象
	WTSOrderInfo()
		:m_orderState(WOS_Submitting)    // 初始化订单状态为正在提交
		,m_orderType(WORT_Normal)        // 初始化订单类型为正常订单
		,m_uInsertDate(0)                // 初始化下单日期为0
		,m_uInsertTime(0)                // 初始化下单时间为0
		,m_dVolTraded(0)                 // 初始化已成交数量为0
		,m_dVolLeft(0)                   // 初始化剩余数量为0
		,m_bIsError(false)               // 初始化错误标志为false
	{

	}

	virtual ~WTSOrderInfo(){}  // 虚析构函数

public:
	// 静态工厂方法：创建订单信息对象
	static inline WTSOrderInfo* create(WTSEntrust* entrust = NULL) noexcept
	{
		WTSOrderInfo *pRet = WTSOrderInfo::allocate();  // 从对象池分配内存

		if(entrust != NULL)  // 如果委托对象不为空
		{
			wt_strcpy(pRet->m_strCode, entrust->getCode());      // 复制合约代码
			wt_strcpy(pRet->m_strExchg,entrust->getExchg());     // 复制交易所代码
			pRet->m_iPrice = entrust->getPrice();                // 复制价格
			pRet->m_dVolume = entrust->getVolume();              // 复制数量

			pRet->m_direction = entrust->getDirection();         // 复制交易方向
			pRet->m_offsetType = entrust->getOffsetType();       // 复制开平方向
			pRet->m_orderFlag = entrust->getOrderFlag();         // 复制订单标志
			pRet->m_priceType = entrust->getPriceType();         // 复制价格类型
			wt_strcpy(pRet->m_strEntrustID, entrust->getEntrustID());  // 复制委托编号
			wt_strcpy(pRet->m_strUserTag, entrust->getUserTag());      // 复制用户标签

			pRet->m_dVolLeft = entrust->getVolume();             // 设置剩余数量为委托数量
			pRet->m_businessType = entrust->getBusinessType();   // 复制业务类型
		}

		return pRet;  // 返回创建的对象
	}

public:
	//这部分是和WTSEntrust同步的
	// 设置交易所代码
	inline void setExchange(const char* exchg, std::size_t len = 0)  noexcept {
		if (len == 0)  // 如果长度为0，使用默认复制
			wt_strcpy(m_strExchg, exchg);
		else  // 否则使用指定长度复制
			strncpy(m_strExchg, exchg, len);
	}
	// 设置合约代码
	inline void setCode(const char* code, std::size_t len = 0) noexcept {
		if (len == 0)  // 如果长度为0，使用默认复制
			wt_strcpy(m_strCode, code);
		else  // 否则使用指定长度复制
			strncpy(m_strCode, code, len);
	}

	inline void setDirection(WTSDirectionType dType)  noexcept { m_direction = dType; }
	// 设置价格类型（市价/限价）
	inline void setPriceType(WTSPriceType pType)  noexcept { m_priceType = pType; }
	// 设置订单标志（普通/FAK/FOK）
	inline void setOrderFlag(WTSOrderFlag oFlag)  noexcept { m_orderFlag = oFlag; }
	// 设置开平方向（开仓/平仓）
	inline void setOffsetType(WTSOffsetType oType) noexcept { m_offsetType = oType; }

	// 获取交易方向（多空）
	inline WTSDirectionType	getDirection() const  noexcept { return m_direction; }
	// 获取价格类型（市价/限价）
	inline WTSPriceType		getPriceType() const  noexcept { return m_priceType; }
	// 获取订单标志（普通/FAK/FOK）
	inline WTSOrderFlag		getOrderFlag() const  noexcept { return m_orderFlag; }
	// 获取开平方向（开仓/平仓）
	inline WTSOffsetType	getOffsetType() const  noexcept { return m_offsetType; }

	// 设置业务类型
	inline void setBusinessType(WTSBusinessType bType)  noexcept { m_businessType = bType; }
	// 获取业务类型
	inline WTSBusinessType	getBusinessType() const  noexcept { return m_businessType; }

	// 设置交易数量
	inline void setVolume(double volume)  noexcept { m_dVolume = volume; }
	// 设置交易价格
	inline void setPrice(double price)  noexcept { m_iPrice = price; }

	// 获取交易数量
	inline double getVolume() const  noexcept { return m_dVolume; }
	// 获取交易价格
	inline double getPrice() const  noexcept { return m_iPrice; }

	// 获取合约代码
	inline const char* getCode() const noexcept { return m_strCode; }
	// 获取交易所代码
	inline const char* getExchg() const  noexcept { return m_strExchg; }

	// 设置委托编号
	inline void setEntrustID(const char* eid)  noexcept { wt_strcpy(m_strEntrustID, eid); }
	// 获取委托编号（只读）
	inline const char* getEntrustID() const  noexcept { return m_strEntrustID; }
	// 获取委托编号（可写）
	inline char* getEntrustID()  noexcept { return m_strEntrustID; }

	// 设置用户标签
	inline void setUserTag(const char* tag)  noexcept { wt_strcpy(m_strUserTag, tag); }
	// 获取用户标签（只读）
	inline const char* getUserTag() const  noexcept { return m_strUserTag; }
	// 获取用户标签（可写）
	inline char* getUserTag() noexcept { return m_strUserTag; }

	// 设置净持仓方向和买入标志
	inline void setNetDirection(bool isBuy)  noexcept { m_bIsNet = true; m_bIsBuy = isBuy; }
	// 检查是否为净持仓
	inline bool isNet() const  noexcept { return m_bIsNet; }
	// 检查是否为买入方向
	inline bool isBuy() const  noexcept { return m_bIsBuy; }

	// 设置合约信息指针
	inline void setContractInfo(WTSContractInfo* cInfo)  noexcept { m_pContract = cInfo; }
	// 获取合约信息指针
	inline WTSContractInfo* getContractInfo() const  noexcept { return m_pContract; }

public:
	// 设置下单日期
	inline void	setOrderDate(uint32_t uDate) noexcept {m_uInsertDate = uDate;}
	// 设置下单时间
	inline void	setOrderTime(uint64_t uTime) noexcept {m_uInsertTime = uTime;}
	// 设置已成交数量
	inline void	setVolTraded(double vol) noexcept { m_dVolTraded = vol; }
	// 设置剩余数量
	inline void	setVolLeft(double vol) noexcept { m_dVolLeft = vol; }
	
	// 设置订单编号
	inline void	setOrderID(const char* oid) noexcept { wt_strcpy(m_strOrderID, oid); }
	// 设置订单状态
	inline void	setOrderState(WTSOrderState os) noexcept {m_orderState = os;}
	// 设置订单类型
	inline void	setOrderType(WTSOrderType ot) noexcept {m_orderType = ot;}

	// 获取下单日期
	inline uint32_t getOrderDate() const noexcept {return m_uInsertDate;}
	// 获取下单时间
	inline uint64_t getOrderTime() const noexcept {return m_uInsertTime;}
	// 获取已成交数量
	inline double getVolTraded() const noexcept { return m_dVolTraded; }
	// 获取剩余数量
	inline double getVolLeft() const noexcept { return m_dVolLeft; }
    
	// 获取订单状态
	inline WTSOrderState		getOrderState() const  noexcept { return m_orderState; }
	// 获取订单类型
	inline WTSOrderType			getOrderType() const  noexcept { return m_orderType; }
	// 获取订单编号（只读）
	inline const char*			getOrderID() const  noexcept { return m_strOrderID; }
	// 获取订单编号（可写）
	inline char*			getOrderID()  noexcept { return m_strOrderID; }

	// 设置状态消息
	inline void	setStateMsg(const char* msg) noexcept {m_strStateMsg = msg;}
	// 获取状态消息
	inline const char* getStateMsg() const noexcept {return m_strStateMsg.c_str();}

	// 检查订单是否仍然有效（未完全成交且未撤销）
	inline bool	isAlive() const noexcept
	{
		if (m_bIsError)  // 如果有错误，返回false
			return false;

		switch(m_orderState)  // 根据订单状态判断
		{
		case WOS_AllTraded:   // 全部成交
		case WOS_Canceled:     // 已撤销
			return false;      // 返回false
		default:               // 其他状态
			return true;       // 返回true
		}
	}

	// 设置错误标志
	inline void	setError(bool bError = true) noexcept { m_bIsError = bError; }
	// 检查是否有错误
	inline bool	isError() const noexcept { return m_bIsError; }

private:
	//这部分成员和WTSEntrust一致
	char			m_strExchg[MAX_EXCHANGE_LENGTH];  // 交易所代码字符串
	char			m_strCode[MAX_INSTRUMENT_LENGTH];  // 合约代码字符串
	double			m_dVolume;                        // 交易数量
	double			m_iPrice;                         // 交易价格

	bool			m_bIsNet;                         // 是否为净持仓
	bool			m_bIsBuy;                         // 是否为买入方向

	WTSDirectionType	m_direction;                    // 交易方向（多空）
	WTSPriceType		m_priceType;                    // 价格类型（市价/限价）
	WTSOrderFlag		m_orderFlag;                    // 订单标志（普通/FAK/FOK）
	WTSOffsetType		m_offsetType;                   // 开平方向（开仓/平仓）

	char				m_strEntrustID[64] = { 0 };    // 委托编号字符串
	char				m_strUserTag[64] = { 0 };      // 用户标签字符串

	WTSBusinessType		m_businessType;                 // 业务类型
	WTSContractInfo*	m_pContract;                    // 合约信息指针

	//这部分是Order专有的成员
	uint32_t	m_uInsertDate;                   // 下单日期
	uint64_t	m_uInsertTime;                   // 下单时间
	double		m_dVolTraded;                    // 已成交数量
	double		m_dVolLeft;                      // 剩余数量
	bool		m_bIsError;                      // 错误标志

	WTSOrderState	m_orderState;                   // 订单状态
	WTSOrderType	m_orderType;                     // 订单类型
	char			m_strOrderID[64] = { 0 };       // 订单编号字符串
	std::string		m_strStateMsg;                   // 状态消息字符串
};

//////////////////////////////////////////////////////////////////////////
//成交信息类，继承自对象池基类
class WTSTradeInfo : public WTSPoolObject<WTSTradeInfo>
{
public:
	// 构造函数，初始化成交信息对象
	WTSTradeInfo()
		: m_orderType(WORT_Normal)    // 初始化订单类型为正常订单
		, m_tradeType(WTT_Common)     // 初始化成交类型为普通成交
		, m_uAmount(0)                // 初始化成交数量为0
		, m_dPrice(0)                 // 初始化成交价格为0
		, m_businessType(BT_CASH)     // 初始化业务类型为现金交易
		, m_pContract(NULL)           // 初始化合约信息指针为NULL
	{}
	virtual ~WTSTradeInfo(){}  // 虚析构函数

public:
	// 静态工厂方法：创建成交信息对象
	static inline WTSTradeInfo* create(const char* code, const char* exchg = "", WTSBusinessType bType = BT_CASH)
	{
		WTSTradeInfo *pRet = WTSTradeInfo::allocate();  // 从对象池分配内存
		wt_strcpy(pRet->m_strExchg, exchg);             // 复制交易所代码
		wt_strcpy(pRet->m_strCode, code);               // 复制合约代码
		pRet->m_businessType = bType;                   // 设置业务类型

		return pRet;                                     // 返回创建的对象
	}

	// 设置成交编号
	inline void setTradeID(const char* tradeid) noexcept { wt_strcpy(m_strTradeID, tradeid); }
	// 设置关联订单编号
	inline void setRefOrder(const char* oid) noexcept { wt_strcpy(m_strRefOrder, oid); }
	
	// 设置交易方向（多空）
	inline void setDirection(WTSDirectionType dType)noexcept {m_direction = dType;}
	// 设置开平方向（开仓/平仓）
	inline void setOffsetType(WTSOffsetType oType)noexcept {m_offsetType = oType;}
	// 设置订单类型
	inline void setOrderType(WTSOrderType ot)noexcept {m_orderType = ot;}
	// 设置成交类型
	inline void setTradeType(WTSTradeType tt)noexcept {m_tradeType = tt;}

	// 设置成交数量
	inline void setVolume(double volume)noexcept {m_dVolume = volume;}
	// 设置成交价格
	inline void setPrice(double price)noexcept { m_dPrice = price; }

	// 设置成交日期
	inline void setTradeDate(uint32_t uDate)noexcept {m_uTradeDate = uDate;}
	// 设置成交时间
	inline void setTradeTime(uint64_t uTime)noexcept {m_uTradeTime = uTime;}

	// 设置成交金额
	inline void setAmount(double amount) noexcept { m_uAmount = amount; }

	// 获取交易方向（多空）
	inline WTSDirectionType	getDirection() const noexcept {return m_direction;}
	// 获取开平方向（开仓/平仓）
	inline WTSOffsetType	getOffsetType() const noexcept {return m_offsetType;}
	// 获取订单类型
	inline WTSOrderType		getOrderType() const noexcept {return m_orderType;}
	// 获取成交类型
	inline WTSTradeType		getTradeType() const noexcept {return m_tradeType;}

	// 获取成交数量
	inline double getVolume() const { return m_dVolume; }
	// 获取成交价格
	inline double getPrice() const noexcept { return m_dPrice; }

	// 获取合约代码
	inline const char*	getCode() const noexcept { return m_strCode; }
	// 获取交易所代码
	inline const char*	getExchg() const noexcept { return m_strExchg; }
	// 获取成交编号
	inline const char*	getTradeID() const noexcept { return m_strTradeID; }
	// 获取关联订单编号
	inline const char*	getRefOrder() const noexcept { return m_strRefOrder; }

	// 获取成交编号（可写）
	inline char*	getTradeID() noexcept { return m_strTradeID; }
	// 获取关联订单编号（可写）
	inline char*	getRefOrder() noexcept { return m_strRefOrder; }

	// 获取成交日期
	inline uint32_t getTradeDate() const noexcept {return m_uTradeDate;}
	// 获取成交时间
	inline uint64_t getTradeTime() const noexcept {return m_uTradeTime;}

	// 获取成交金额
	inline double getAmount() const noexcept { return m_uAmount; }

	// 设置用户标签
	inline void setUserTag(const char* tag) noexcept { wt_strcpy(m_strUserTag, tag); }
	// 获取用户标签
	inline const char* getUserTag() const noexcept { return m_strUserTag; }

	// 设置业务类型
	inline void setBusinessType(WTSBusinessType bType) noexcept  { m_businessType = bType; }
	// 获取业务类型
	inline WTSBusinessType	getBusinessType() const noexcept { return m_businessType; }

	// 设置净持仓方向和买入标志
	inline void setNetDirection(bool isBuy) noexcept { m_bIsNet = true; m_bIsBuy = isBuy; }
	// 检查是否为净持仓
	inline bool isNet() const noexcept { return m_bIsNet; }
	// 检查是否为买入方向
	inline bool isBuy() const noexcept { return m_bIsBuy; }

	// 设置合约信息指针
	inline void setContractInfo(WTSContractInfo* cInfo) noexcept { m_pContract = cInfo; }
	// 获取合约信息指针
	inline WTSContractInfo* getContractInfo() const noexcept { return m_pContract; }

protected:
	char	m_strExchg[MAX_EXCHANGE_LENGTH];	// 交易所代码字符串
	char	m_strCode[MAX_INSTRUMENT_LENGTH];	// 合约代码字符串
	char	m_strTradeID[64] = { 0 };			// 成交编号字符串
	char	m_strRefOrder[64] = { 0 };			// 关联订单编号字符串
	char	m_strUserTag[64] = { 0 };			// 用户标签字符串

	uint32_t	m_uTradeDate;                   // 成交日期
	uint64_t	m_uTradeTime;                   // 成交时间
	double		m_dVolume;                      // 成交数量
	double		m_dPrice;                       // 成交价格

	bool		m_bIsNet;                       // 是否为净持仓
	bool		m_bIsBuy;                       // 是否为买入方向

	WTSDirectionType	m_direction;                // 交易方向（多空）
	WTSOffsetType		m_offsetType;               // 开平方向（开仓/平仓）
	WTSOrderType		m_orderType;                 // 订单类型
	WTSTradeType		m_tradeType;                 // 成交类型

	double		m_uAmount;                      // 成交金额

	WTSBusinessType		m_businessType;             // 业务类型
	WTSContractInfo*	m_pContract;                // 合约信息指针
};

//////////////////////////////////////////////////////////////////////////
//持仓信息类，继承自对象池基类
class WTSPositionItem : public WTSPoolObject<WTSPositionItem>
{
public:
	// 静态工厂方法：创建持仓信息对象
	static inline WTSPositionItem* create(const char* code, const char* currency = "CNY", const char* exchg = "", WTSBusinessType bType = BT_CASH)
	{
		WTSPositionItem *pRet = WTSPositionItem::allocate();  // 从对象池分配内存
		wt_strcpy(pRet->m_strExchg, exchg);                   // 复制交易所代码
		wt_strcpy(pRet->m_strCode, code);                     // 复制合约代码
		wt_strcpy(pRet->m_strCurrency, currency);             // 复制货币代码
		pRet->m_businessType = bType;                         // 设置业务类型

		return pRet;                                           // 返回创建的对象
	}

	// 设置持仓方向（多空）
	inline void setDirection(WTSDirectionType dType){m_direction = dType;}
	// 设置昨日持仓数量
	inline void setPrePosition(double prePos){ m_dPrePosition = prePos; }
	// 设置今日持仓数量
	inline void setNewPosition(double newPos){ m_dNewPosition = newPos; }
	// 设置昨日可用持仓数量
	inline void setAvailPrePos(double availPos){ m_dAvailPrePos = availPos; }
	// 设置今日可用持仓数量
	inline void setAvailNewPos(double availPos){ m_dAvailNewPos = availPos; }
	// 设置持仓成本
	inline void setPositionCost(double cost){m_dTotalPosCost = cost;}
	// 设置保证金
	inline void setMargin(double margin){ m_dMargin = margin; }
	// 设置平均价格
	inline void setAvgPrice(double avgPrice){ m_dAvgPrice = avgPrice; }
	// 设置动态盈亏
	inline void setDynProfit(double profit){ m_dDynProfit = profit; }

	// 获取持仓方向（多空）
	inline WTSDirectionType getDirection() const{return m_direction;}
	// 获取昨日持仓数量
	inline double	getPrePosition() const{ return m_dPrePosition; }
	// 获取今日持仓数量
	inline double	getNewPosition() const{ return m_dNewPosition; }
	// 获取昨日可用持仓数量
	inline double	getAvailPrePos() const{ return m_dAvailPrePos; }
	// 获取今日可用持仓数量
	inline double	getAvailNewPos() const{ return m_dAvailNewPos; }

	// 获取总持仓数量（昨日+今日）
	inline double	getTotalPosition() const{ return m_dPrePosition + m_dNewPosition; }
	// 获取总可用持仓数量（昨日可用+今日可用）
	inline double	getAvailPosition() const{ return m_dAvailPrePos + m_dAvailNewPos; }

	// 获取冻结持仓数量（总持仓-可用持仓）
	inline double	getFrozenPosition() const{ return getTotalPosition() - getAvailPosition(); }
	// 获取今日冻结持仓数量（今日持仓-今日可用）
	inline double	getFrozenNewPos() const{ return m_dNewPosition - m_dAvailNewPos; }
	// 获取昨日冻结持仓数量（昨日持仓-昨日可用）
	inline double	getFrozenPrePos() const{ return m_dPrePosition - m_dAvailPrePos; }

	// 获取持仓成本
	inline double		getPositionCost() const{ return m_dTotalPosCost; }
	// 获取保证金
	inline double		getMargin() const{ return m_dMargin; }
	// 获取平均价格
	inline double		getAvgPrice() const{ return m_dAvgPrice; }
	// 获取动态盈亏
	inline double		getDynProfit() const{ return m_dDynProfit; }

	// 获取合约代码
	inline const char* getCode() const{ return m_strCode; }
	// 获取货币代码
	inline const char* getCurrency() const{ return m_strCurrency; }
	// 获取交易所代码
	inline const char* getExchg() const{ return m_strExchg; }

	// 设置业务类型
	inline void setBusinessType(WTSBusinessType bType) { m_businessType = bType; }
	// 获取业务类型
	inline WTSBusinessType	getBusinessType() const { return m_businessType; }

	// 设置合约信息指针
	inline void setContractInfo(WTSContractInfo* cInfo) { m_pContract = cInfo; }
	// 获取合约信息指针
	inline WTSContractInfo* getContractInfo() const { return m_pContract; }

public:
	// 构造函数，初始化持仓信息对象
	WTSPositionItem()
		: m_direction(WDT_LONG)        // 初始化持仓方向为做多
		, m_dPrePosition(0)            // 初始化昨日持仓数量为0
		, m_dNewPosition(0)            // 初始化今日持仓数量为0
		, m_dAvailPrePos(0)            // 初始化昨日可用持仓数量为0
		, m_dAvailNewPos(0)            // 初始化今日可用持仓数量为0
		, m_dMargin(0)                 // 初始化保证金为0
		, m_dAvgPrice(0)               // 初始化平均价格为0
		, m_dDynProfit(0)              // 初始化动态盈亏为0
		, m_dTotalPosCost(0)           // 初始化持仓成本为0
		, m_businessType(BT_CASH)      // 初始化业务类型为现金交易
		, m_pContract(NULL)            // 初始化合约信息指针为NULL
	{}
	virtual ~WTSPositionItem(){}  // 虚析构函数

protected:
	char			m_strExchg[MAX_EXCHANGE_LENGTH];  // 交易所代码字符串
	char			m_strCode[MAX_INSTRUMENT_LENGTH];  // 合约代码字符串
	char			m_strCurrency[8] = { 0 };         // 货币代码字符串

	WTSDirectionType	m_direction;                    // 多空方向
	double		m_dPrePosition;		// 昨日持仓数量
	double		m_dNewPosition;		// 今日持仓数量
	double		m_dAvailPrePos;		// 可平昨日持仓数量
	double		m_dAvailNewPos;		// 可平今日持仓数量
	double		m_dTotalPosCost;	// 持仓总成本
	double		m_dMargin;			// 占用保证金
	double		m_dAvgPrice;		// 持仓均价
	double		m_dDynProfit;		// 浮动盈亏

	WTSBusinessType		m_businessType;                 // 业务类型
	WTSContractInfo*	m_pContract;                    // 合约信息指针
};

//////////////////////////////////////////////////////////////////////////
//账户信息类，继承自对象池基类
class WTSAccountInfo : public WTSPoolObject<WTSAccountInfo>
{
public:
	// 构造函数，初始化账户信息对象
	WTSAccountInfo()
		: m_strCurrency("CNY")         // 初始化货币代码为人民币
		, m_dBalance(0)                // 初始化账户余额为0
		, m_dPreBalance(0)             // 初始化上日余额为0
		, m_dCommission(0)             // 初始化手续费为0
		, m_dFrozenMargin(0)           // 初始化冻结保证金为0
		, m_dFrozenCommission(0)       // 初始化冻结手续费为0
		, m_dCloseProfit(0)            // 初始化平仓盈亏为0
		, m_dDynProfit(0)              // 初始化浮动盈亏为0
		, m_dDeposit(0)                // 初始化入金为0
		, m_dWithdraw(0)               // 初始化出金为0
		, m_dAvailable(0)              // 初始化可用资金为0
	{
	}
	virtual ~WTSAccountInfo(){}  // 虚析构函数

public:
	// 静态工厂方法：创建账户信息对象
	static inline WTSAccountInfo* create(){return WTSAccountInfo::allocate();}

	// 设置货币代码
	inline void	setCurrency(const char* currency){ m_strCurrency = currency; }

	// 设置账户余额
	inline void	setBalance(double balance){m_dBalance = balance;}
	// 设置上日余额
	inline void	setPreBalance(double prebalance){m_dPreBalance = prebalance;}
	// 设置保证金
	inline void	setMargin(double margin){m_uMargin = margin;}
	// 设置冻结保证金
	inline void	setFrozenMargin(double frozenmargin){m_dFrozenMargin = frozenmargin;}
	// 设置平仓盈亏
	inline void	setCloseProfit(double closeprofit){m_dCloseProfit = closeprofit;}
	// 设置浮动盈亏
	inline void	setDynProfit(double dynprofit){m_dDynProfit = dynprofit;}
	// 设置入金
	inline void	setDeposit(double deposit){m_dDeposit = deposit;}
	// 设置出金
	inline void	setWithdraw(double withdraw){m_dWithdraw = withdraw;}
	// 设置手续费
	inline void	setCommission(double commission){m_dCommission = commission;}
	// 设置冻结手续费
	inline void	setFrozenCommission(double frozencommission){m_dFrozenCommission = frozencommission;}
	// 设置可用资金
	inline void	setAvailable(double available){m_dAvailable = available;}

	// 获取账户余额
	inline double	getBalance() const{return m_dBalance;}
	// 获取上日余额
	inline double	getPreBalance() const{return m_dPreBalance;}
	// 获取保证金
	inline double	getMargin() const{return m_uMargin;}
	// 获取冻结保证金
	inline double	getFrozenMargin() const{return m_dFrozenMargin;}
	// 获取平仓盈亏
	inline double	getCloseProfit() const{return m_dCloseProfit;}
	// 获取浮动盈亏
	inline double	getDynProfit() const{return m_dDynProfit;}
	// 获取入金
	inline double	getDeposit() const{return m_dDeposit;}
	// 获取出金
	inline double	getWithdraw() const{return m_dWithdraw;}
	// 获取手续费
	inline double	getCommission() const{return m_dCommission;}
	// 获取冻结手续费
	inline double	getFrozenCommission() const{return m_dFrozenCommission;}
	// 获取可用资金
	inline double	getAvailable() const{return m_dAvailable;}

	// 获取货币代码
	inline const char* getCurrency() const{ return m_strCurrency.c_str(); }

protected:
	std::string m_strCurrency;              // 货币代码字符串

	double		m_dBalance;					// 账户余额
	double		m_dPreBalance;				// 上次结算准备金
	double		m_uMargin;					// 当前保证金总额
	double		m_dCommission;				// 手续费
	double		m_dFrozenMargin;			// 冻结的保证金
	double		m_dFrozenCommission;		// 冻结的手续费
	double		m_dCloseProfit;				// 平仓盈亏
	double		m_dDynProfit;				// 持仓盈亏
	double		m_dDeposit;					// 入金金额
	double		m_dWithdraw;				// 出金金额
	double		m_dAvailable;				// 可用资金
};


NS_WTP_END  // 结束WonderTrader命名空间
