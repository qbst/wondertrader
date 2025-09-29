/*!
 * \file ParserCTP.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTP行情解析器头文件 - WonderTrader框架中的CTP行情数据解析模块
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader框架中专门用于解析CTP（综合交易平台）行情数据的核心模块。
 * 该模块作为行情数据源和策略引擎之间的桥梁，负责以下主要功能：
 * 
 * 1. CTP行情连接管理：
 *    - 建立与CTP行情服务器的网络连接
 *    - 处理连接状态变化（连接、断开、重连）
 *    - 管理登录认证和会话维护
 * 
 * 2. 行情数据订阅与处理：
 *    - 订阅和退订指定合约的实时行情数据
 *    - 接收CTP推送的深度行情数据
 *    - 将原始CTP数据转换为WonderTrader标准格式
 * 
 * 3. 数据质量控制：
 *    - 验证行情数据的有效性和完整性
 *    - 处理异常价格数据（如DBL_MAX等无效值）
 *    - 时间戳校正和交易日处理
 * 
 * 4. 事件驱动架构：
 *    - 实现IParserApi接口，提供标准化的行情解析能力
 *    - 继承CThostFtdcMdSpi，处理CTP行情回调事件
 *    - 通过回调机制将处理后的数据传递给上层应用
 * 
 * 5. 动态库集成：
 *    - 支持动态加载CTP API库
 *    - 处理不同版本CTP API的兼容性
 *    - 提供插件式的模块化设计
 * 
 * 设计特点：
 * - 采用双重继承设计：既是解析器（IParserApi）又是CTP回调处理器（CThostFtdcMdSpi）
 * - 支持多种CTP环境：标准CTP、SimNow、OpenCTP等
 * - 提供本地时间戳选项，适应不同交易环境需求
 * - 集成基础数据管理，支持合约信息验证
 * - 使用状态机模式管理登录状态
 * 
 * 该解析器是WonderTrader量化交易框架的重要组成部分，为策略执行提供可靠的实时行情数据源。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/IParserApi.h"      // 包含行情解析器基础接口定义
#include "../Share/DLLHelper.hpp"        // 包含动态库操作辅助工具
#include "../API/CTP6.3.15/ThostFtdcMdApi.h"  // 包含CTP行情API头文件（版本6.3.15）
#include <map>  // 包含STL映射容器，用于数据管理

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSTickData;  // 前向声明：WonderTrader Tick数据类
NS_WTP_END    // 结束WonderTrader命名空间

USING_NS_WTP;  // 使用WonderTrader命名空间

/**
 * @class ParserCTP
 * @brief CTP行情解析器类 - 专门用于处理CTP行情数据的解析器实现
 * 
 * 该类采用双重继承设计，既实现了WonderTrader的标准行情解析接口（IParserApi），
 * 又继承了CTP官方的行情回调接口（CThostFtdcMdSpi），形成了完整的行情数据处理链路。
 * 
 * 主要功能特性：
 * 1. 连接管理：建立和维护与CTP行情服务器的连接
 * 2. 认证登录：处理用户身份验证和会话管理
 * 3. 数据订阅：管理合约行情的订阅和退订
 * 4. 数据转换：将CTP原始数据转换为WonderTrader标准格式
 * 5. 质量控制：验证和修正行情数据的有效性
 * 6. 事件分发：将处理后的数据通过回调传递给上层应用
 * 
 * 设计模式：
 * - 适配器模式：将CTP API适配为WonderTrader标准接口
 * - 观察者模式：通过回调机制通知数据变化
 * - 状态模式：管理连接和登录状态
 * - 单例模式：每个实例管理一个CTP连接
 * 
 * 线程安全：
 * - CTP API本身是线程安全的
 * - 回调函数在CTP内部线程中执行
 * - 需要注意数据访问的同步问题
 */
class ParserCTP :	public IParserApi, public CThostFtdcMdSpi
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化ParserCTP对象，设置默认参数和状态。
	 * 不进行实际的连接操作，连接需要通过init()和connect()方法建立。
	 */
	ParserCTP();
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 确保派生类能够正确析构，释放相关资源。
	 * 在析构时会自动断开CTP连接并清理相关资源。
	 */
	virtual ~ParserCTP();

public:
	/**
	 * @enum LoginStatus
	 * @brief 登录状态枚举
	 * 
	 * 定义了CTP行情服务器的登录状态，用于状态机管理。
	 * 该状态控制着行情订阅等操作的时机。
	 */
	enum LoginStatus
	{
		LS_NOTLOGIN,  ///< 未登录状态：初始状态或登出后的状态
		LS_LOGINING,  ///< 登录中状态：正在进行登录认证过程
		LS_LOGINED    ///< 已登录状态：登录成功，可以进行行情订阅等操作
	};

// ==================== IParserApi 接口实现 ====================
// 以下方法实现了WonderTrader标准行情解析器接口，提供统一的行情数据访问方式
public:
	/**
	 * @brief 初始化解析器
	 * @param config 配置参数对象，包含CTP连接所需的各种参数
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 从配置中读取以下关键参数：
	 * - front: CTP行情前置服务器地址（必选）
	 * - broker: 期货公司代码（必选） 
	 * - user: 用户账号（必选）
	 * - pass: 用户密码（必选）
	 * - flowdir: 流控文件目录（可选，默认为"CTPMDFlow"）
	 * - localtime: 是否使用本地时间戳（可选，默认false，适用于SimNow等环境）
	 * - ctpmodule: CTP动态库名称（可选，默认"thostmduserapi_se"）
	 * 
	 * 初始化过程包括：
	 * 1. 解析和验证配置参数
	 * 2. 创建流控文件目录
	 * 3. 动态加载CTP API库
	 * 4. 创建CTP行情API实例
	 * 5. 注册回调接口和前置服务器地址
	 */
	virtual bool init(WTSVariant* config) override;

	/**
	 * @brief 释放解析器资源
	 * 
	 * 清理所有占用的资源，包括：
	 * - 断开CTP连接
	 * - 释放API实例
	 * - 清理内存和文件句柄
	 * 
	 * 该方法通常在程序退出或模块卸载时调用。
	 */
	virtual void release() override;

	/**
	 * @brief 开始连接CTP行情服务器
	 * @return bool 连接命令发送成功返回true，失败返回false
	 * 
	 * 启动与CTP行情服务器的连接过程。
	 * 实际的连接建立是异步的，连接结果通过OnFrontConnected()回调通知。
	 * 连接成功后会自动触发登录流程。
	 */
	virtual bool connect() override;

	/**
	 * @brief 断开与CTP行情服务器的连接
	 * @return bool 断开命令发送成功返回true，失败返回false
	 * 
	 * 主动断开与CTP服务器的连接。
	 * 断开完成后会通过OnFrontDisconnected()回调通知。
	 */
	virtual bool disconnect() override;

	/**
	 * @brief 检查是否已连接到CTP服务器
	 * @return bool 已连接返回true，未连接返回false
	 * 
	 * 注意：此方法检查的是API实例是否存在，
	 * 不代表网络连接和登录状态，仅表示初始化状态。
	 */
	virtual bool isConnected() override;

	/**
	 * @brief 订阅指定合约的行情数据
	 * @param vecSymbols 要订阅的合约代码集合，格式为"交易所.合约代码"
	 * 
	 * 如果当前未登录，会将订阅请求缓存，等待登录成功后自动订阅。
	 * 如果已登录，会立即发送订阅请求到CTP服务器。
	 * 订阅结果通过OnRspSubMarketData()回调通知。
	 * 
	 * 注意：合约代码格式需要与基础数据管理器中的格式一致。
	 */
	virtual void subscribe(const CodeSet &vecSymbols) override;
	
	/**
	 * @brief 退订指定合约的行情数据
	 * @param vecSymbols 要退订的合约代码集合
	 * 
	 * 发送退订请求到CTP服务器，停止接收指定合约的行情推送。
	 * 退订结果通过OnRspUnSubMarketData()回调通知。
	 * 
	 * 注意：当前实现中此方法为空，可根据需要进行扩展。
	 */
	virtual void unsubscribe(const CodeSet &vecSymbols) override;

	/**
	 * @brief 注册行情数据回调接口
	 * @param listener 回调接口指针，用于接收解析后的行情数据和事件
	 * 
	 * 设置上层应用的回调接口，所有的行情数据、连接事件、错误信息
	 * 都会通过这个接口传递给上层应用。
	 * 同时获取基础数据管理器接口，用于合约信息验证。
	 */
	virtual void registerSpi(IParserSpi* listener) override;


// ==================== CThostFtdcMdSpi 回调接口实现 ====================
// 以下方法实现了CTP官方行情API的回调接口，处理来自CTP服务器的各种事件和数据
public:
	/**
	 * @brief 错误应答回调
	 * @param pRspInfo 错误信息结构体指针，包含错误代码和错误消息
	 * @param nRequestID 请求ID，用于标识对应的请求
	 * @param bIsLast 是否为最后一个应答，批量数据时使用
	 * 
	 * 当CTP服务器返回错误信息时触发此回调。
	 * 用于处理各种CTP操作失败的情况，如登录失败、订阅失败等。
	 * 错误信息会通过日志系统记录。
	 */
	virtual void OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 前置连接成功回调
	 * 
	 * 当与CTP行情前置服务器建立网络连接成功时触发此回调。
	 * 连接成功后会自动触发用户登录流程。
	 * 通过回调接口通知上层应用连接事件（WPE_Connect）。
	 */
	virtual void OnFrontConnected();

	/**
	 * @brief 用户登录应答回调
	 * @param pRspUserLogin 登录响应信息，包含交易日等重要信息
	 * @param pRspInfo 错误信息，登录失败时包含具体错误原因
	 * @param nRequestID 登录请求ID
	 * @param bIsLast 是否为最后一个应答
	 * 
	 * 用户登录请求的服务器应答。登录成功后：
	 * 1. 获取并设置当前交易日
	 * 2. 通知上层应用登录事件（WPE_Login）
	 * 3. 自动开始订阅已缓存的合约行情
	 * 
	 * 如果获取交易日失败，会使用本地日期作为备选方案。
	 */
	virtual void OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 用户登出应答回调
	 * @param pUserLogout 登出响应信息
	 * @param pRspInfo 错误信息
	 * @param nRequestID 登出请求ID
	 * @param bIsLast 是否为最后一个应答
	 * 
	 * 用户主动登出或被服务器强制登出时的应答。
	 * 通过回调接口通知上层应用登出事件（WPE_Logout）。
	 */
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 前置连接断开回调
	 * @param nReason 断开原因代码
	 * 
	 * 当与CTP服务器的连接意外断开时触发此回调。
	 * 常见的断开原因包括网络故障、服务器维护、认证超时等。
	 * 通过回调接口通知上层应用连接关闭事件（WPE_Close）。
	 * 记录断开原因到日志系统。
	 */
	virtual void OnFrontDisconnected( int nReason );

	/**
	 * @brief 退订行情应答回调
	 * @param pSpecificInstrument 退订的合约信息
	 * @param pRspInfo 错误信息，退订失败时包含错误原因
	 * @param nRequestID 退订请求ID
	 * @param bIsLast 是否为最后一个应答
	 * 
	 * 退订合约行情请求的服务器应答。
	 * 成功退订后，服务器将停止推送该合约的行情数据。
	 * 当前实现中此回调为空，可根据需要进行扩展。
	 */
	virtual void OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 深度行情通知回调 - 核心数据处理方法
	 * @param pDepthMarketData CTP深度行情数据指针，包含完整的市场数据
	 * 
	 * 这是最重要的回调方法，处理来自CTP的实时行情数据。
	 * 主要处理流程：
	 * 1. 验证合约信息的有效性
	 * 2. 处理时间戳和交易日（支持本地时间戳模式）
	 * 3. 修正夜盘时间异常问题
	 * 4. 验证和过滤无效价格数据（DBL_MAX等）
	 * 5. 转换数据格式为WonderTrader标准格式
	 * 6. 处理不同交易所的特殊规则（如郑商所成交额缩放）
	 * 7. 填充五档买卖盘数据
	 * 8. 通过回调接口传递给上层应用
	 * 
	 * 时间处理特殊逻辑：
	 * - 支持本地时间戳模式（适用于SimNow等环境）
	 * - 处理夜盘时间跨日问题
	 * - 过滤启动时的过期数据
	 */
	virtual void OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pDepthMarketData );

	/**
	 * @brief 订阅行情应答回调
	 * @param pSpecificInstrument 订阅的合约信息
	 * @param pRspInfo 错误信息，订阅失败时包含错误原因
	 * @param nRequestID 订阅请求ID
	 * @param bIsLast 是否为最后一个应答
	 * 
	 * 订阅合约行情请求的服务器应答。
	 * 成功订阅后，服务器会开始推送该合约的行情数据。
	 * 可以根据应答结果进行相应的处理。
	 */
	virtual void OnRspSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 心跳超时警告回调
	 * @param nTimeLapse 心跳超时时间（毫秒）
	 * 
	 * 当与CTP服务器的心跳检测超时时触发此回调。
	 * 用于监控网络连接质量，及时发现连接异常。
	 * 超时时间会记录到日志系统中。
	 */
	virtual void OnHeartBeatWarning( int nTimeLapse );

// ==================== 私有辅助方法 ====================
private:
	/**
	 * @brief 发送用户登录请求到CTP服务器
	 * 
	 * 构造登录请求结构体，填入用户凭证信息，并发送到CTP服务器。
	 * 登录请求包含经纪商代码、用户ID、密码和产品信息。
	 * 登录结果通过OnRspUserLogin()回调返回。
	 * 
	 * 注意：此方法在连接成功后自动调用，也可手动调用重新登录。
	 */
	void ReqUserLogin();
	
	/**
	 * @brief 执行合约行情订阅操作
	 * 
	 * 将缓存的订阅合约列表发送到CTP服务器进行实际订阅。
	 * 会自动处理合约代码格式（去除交易所前缀）。
	 * 订阅成功后开始接收相应合约的实时行情推送。
	 * 
	 * 注意：
	 * - 只在登录成功后调用此方法
	 * - 如果订阅列表为空则直接返回
	 * - 订阅结果会记录到日志中
	 */
	void DoSubscribeMD();
	
	/**
	 * @brief 检查CTP返回的错误信息
	 * @param pRspInfo CTP错误信息结构体指针
	 * @return bool 如果有错误返回true，无错误返回false
	 * 
	 * 解析CTP服务器返回的错误信息结构体。
	 * 当前实现返回false，可根据需要扩展错误处理逻辑。
	 * 
	 * 可扩展功能：
	 * - 错误码分析和分类
	 * - 错误信息本地化
	 * - 特定错误的自动处理
	 */
	bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);

// ==================== 私有成员变量 ====================
private:
	// ---------- 交易日和API实例 ----------
	uint32_t			m_uTradingDate;    ///< 当前交易日，格式为YYYYMMDD，从CTP服务器获取
	CThostFtdcMdApi*	m_pUserAPI;        ///< CTP行情API实例指针，管理与服务器的连接

	// ---------- 连接配置参数 ----------
	std::string			m_strFrontAddr;    ///< CTP行情前置服务器地址，格式为"tcp://ip:port"
	std::string			m_strBroker;       ///< 期货公司代码，用于标识经纪商
	std::string			m_strUserID;       ///< 用户账号，用于登录认证
	std::string			m_strPassword;     ///< 用户密码，用于登录认证
	std::string			m_strFlowDir;      ///< 流控文件目录，用于存储CTP的流量控制文件
	bool 				m_bLocaltime;      ///< 是否使用本地时间戳标志，适用于SimNow等非标准环境

	// ---------- 订阅管理 ----------
	CodeSet				m_filterSubs;      ///< 待订阅的合约代码集合，缓存订阅请求

	// ---------- 请求管理 ----------
	int					m_iRequestID;      ///< CTP请求ID计数器，用于标识不同的请求

	// ---------- 回调接口 ----------
	IParserSpi*			m_sink;            ///< 上层应用回调接口指针，用于数据和事件通知
	IBaseDataMgr*		m_pBaseDataMgr;    ///< 基础数据管理器接口指针，用于合约信息验证

	// ---------- 动态库管理 ----------
	DllHandle			m_hInstCTP;        ///< CTP动态库句柄，用于管理动态库的生命周期
	
	/**
	 * @typedef CTPCreator
	 * @brief CTP API创建函数指针类型
	 * 
	 * 定义了CTP行情API创建函数的签名，用于动态加载API库。
	 * 参数说明：
	 * - const char*: 流控文件目录路径
	 * - const bool: 是否仅仅UDP组播模式
	 * - const bool: 是否使用多播
	 */
	typedef CThostFtdcMdApi* (*CTPCreator)(const char *, const bool, const bool);
	CTPCreator			m_funcCreator;     ///< CTP API创建函数指针，从动态库中获取
};

