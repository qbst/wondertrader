/**
 * @file IAresCltApi.h
 * @brief Ares客户端API接口定义文件
 * 
 * 本文件定义了Ares行情客户端的核心API接口，主要包含三个核心接口类：
 * 1. IAresCltSpi - 行情数据回调接口，用于接收实时市场数据推送
 * 2. IAresExchange - 交易所数据接口，用于获取合约基础信息和静态数据
 * 3. IAresCltApi - 主API接口，用于控制客户端生命周期和获取交易所对象
 * 
 * 设计逻辑：
 * - 采用观察者模式，通过Spi接口实现异步数据推送
 * - 使用纯虚函数定义接口契约，确保实现类必须重写所有方法
 * - 分离行情接收和数据查询功能，提高系统模块化程度
 * - 支持多市场数据源的统一访问接口
 */

#pragma once                                    // 防止头文件重复包含的预处理指令
#include "IAresCltStruct.h"                     // 包含Ares客户端相关的数据结构定义

/**
 * @class IAresCltSpi
 * @brief Ares客户端回调接口类
 * 
 * 该接口定义了行情数据推送的回调方法，客户端需要实现此接口来接收
 * 来自Ares服务器的实时行情数据和市场时间信息。采用纯虚函数设计，
 * 确保派生类必须实现所有回调方法。
 */
class IAresCltSpi
{
public:
	/**
	 * @brief 市场时间通知回调函数
	 * @param cMarket 市场标识符，指示数据来源的具体市场
	 * @param 指向市场时间字段结构体的指针，包含日期和时间信息
	 * 
	 * 当服务器推送市场时间更新时触发此回调，用于同步客户端的市场时钟
	 */
	virtual		void			OnMarketTime(AClt_Market cMarket, tagAClt_MarketField*) = 0;
	
	/**
	 * @brief 行情快照数据通知回调函数
	 * @param cMarket 市场标识符，指示行情数据所属的市场
	 * @param 指向行情数据结构体的指针，包含完整的tick级行情信息
	 * 
	 * 当接收到实时行情数据时触发此回调，包含价格、成交量、买卖档位等信息
	 */
	virtual		void			OnMarketData(AClt_Market cMarket, tagAClt_QuoteField*) = 0;

};

/**
 * @class IAresExchange
 * @brief 交易所数据访问接口类
 * 
 * 该接口提供对特定交易所合约信息和基础数据的访问功能。
 * 支持批量获取和单个查询两种模式，满足不同场景的数据获取需求。
 * 从版本1.14开始增加了补充数据的获取功能。
 */
class IAresExchange
{
public:
	/**
	 * @brief 获取交易所商品合约总数
	 * @return 返回该交易所支持的商品合约数量
	 * 
	 * 用于在批量获取合约数据前确定需要分配的内存大小
	 */
	virtual		int				GetCommodityCount() = 0;
	
	/**
	 * @brief 批量获取交易所商品合约码表
	 * @param pArr 指向合约信息数组的指针，用于存储获取的合约数据
	 * @param nCount 数组大小，应不小于GetCommodityCount()返回值
	 * @return 实际获取的合约数量，-1表示失败
	 * 
	 * 一次性获取该交易所的所有合约基本信息，包括合约代码和市场标识
	 */
	virtual		int				GetCommodityData(tagAClt_Instrument* pArr, int nCount) = 0;
	
	/**
	 * @brief 获取单个商品的基础数据
	 * @param pInstrument 指向要查询的合约信息结构体
	 * @param pData 指向用于存储基础数据的结构体指针
	 * @return 0表示成功，非0表示失败
	 * 
	 * 根据合约代码获取该合约的详细基础信息，如价格精度、合约乘数等
	 */
	virtual		int				GetOneStaticData(tagAClt_Instrument* pInstrument, tagAClt_CommBaseData* pData) = 0;
	
	/**
	 * @brief 批量获取交易所商品基础数据
	 * @param pArr 指向基础数据数组的指针，用于存储获取的数据
	 * @param nCount 数组大小，应与合约数量匹配
	 * @return 实际获取的数据条数，-1表示失败
	 * 
	 * 一次性获取所有合约的基础数据，提高数据获取效率
	 */
	virtual		int				GetStaticData(tagAClt_CommBaseData* pArr, int nCount) = 0;

	/**
	 * @brief 获取单个商品的补充数据 (API版本1.14+新增功能)
	 * @param pInstrument 指向要查询的合约信息结构体
	 * @param pData 指向用于存储补充数据的结构体指针
	 * @return 0表示成功，非0表示失败
	 * 
	 * 获取合约的扩展信息，如完整名称、额外属性等补充数据
	 */
	virtual		int				GetOneSupplementData(tagAClt_Instrument* pInstrument, tagAClt_SupplementData* pData) = 0;
	
	/**
	 * @brief 批量获取交易所商品补充数据 (API版本1.14+新增功能)
	 * @param pArr 指向补充数据数组的指针，用于存储获取的数据
	 * @param nCount 数组大小，应与合约数量匹配
	 * @return 实际获取的数据条数，-1表示失败
	 * 
	 * 批量获取所有合约的补充数据，提供更丰富的合约信息
	 */
	virtual		int				GetSupplementData(tagAClt_SupplementData* pArr, int nCount) = 0;

};

/**
 * @class IAresCltApi
 * @brief Ares客户端主API接口类
 * 
 * 该接口是Ares客户端的主要控制接口，负责：
 * 1. 管理客户端的生命周期（启动/停止）
 * 2. 注册行情数据回调接口
 * 3. 提供对各个交易所数据接口的访问
 * 
 * 通过此接口可以完成客户端的初始化、数据订阅和资源清理等核心操作
 */
class IAresCltApi
{
public:
	/**
	 * @brief 注册行情数据推送回调接口
	 * @param 指向实现了IAresCltSpi接口的对象指针
	 * 
	 * 注册后，所有的行情数据推送都将通过此Spi接口进行回调通知
	 * 必须在StartWork()之前调用
	 */
	virtual		void			RegisterSpi(IAresCltSpi*) = 0;
	
	/**
	 * @brief 启动客户端工作
	 * @return 0表示启动成功，非0表示启动失败
	 * 
	 * 初始化网络连接，开始接收服务器数据推送
	 * 调用前必须先通过RegisterSpi()注册回调接口
	 */
	virtual		int				StartWork() = 0;
	
	/**
	 * @brief 停止客户端工作
	 * 
	 * 断开网络连接，停止数据接收，清理相关资源
	 * 程序退出前应调用此方法进行优雅关闭
	 */
	virtual		void			EndWork() = 0;

public:
	/**
	 * @brief 获取指定市场的交易所数据接口指针
	 * @param cMarket 市场标识符，指定要获取的市场类型
	 * @return 指向对应市场交易所接口的指针，NULL表示不支持该市场
	 * 
	 * 通过返回的接口指针可以访问该市场的合约信息和基础数据
	 * 不同市场返回不同的接口实例，但都实现相同的IAresExchange接口
	 */
	virtual	 IAresExchange*		GetExchPtr(AClt_Market cMarket) = 0;

};

























