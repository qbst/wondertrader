/*!
 * \file ParserCTPMini.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTP Mini行情解析器头文件 - WonderTrader框架中的CTP Mini行情数据解析模块
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader量化交易框架中专门用于解析CTP Mini行情数据的核心模块头文件。
 * CTP Mini是上海期货信息技术有限公司推出的轻量级行情接口，相比标准CTP具有更低的
 * 资源消耗和更快的启动速度，特别适合个人投资者和小型机构使用。
 * 
 * 核心设计理念：
 * 1. 轻量级设计：基于CTP Mini API 1.5.8版本，提供精简高效的行情解析能力
 * 2. 标准化接口：实现WonderTrader标准行情解析接口，确保与框架的无缝集成
 * 3. 事件驱动：采用异步回调模式处理行情数据，保证实时性和高性能
 * 4. 模块化架构：支持动态加载，便于部署和维护
 * 
 * 主要功能模块：
 * 
 * 1. 连接管理系统：
 *    - 管理与CTP Mini行情服务器的TCP连接
 *    - 处理连接建立、维护、断开和重连逻辑
 *    - 支持多前置服务器配置和故障切换
 *    - 实现连接状态监控和异常处理
 * 
 * 2. 用户认证模块：
 *    - 处理用户登录和身份验证
 *    - 管理会话状态和登录令牌
 *    - 支持自动重登录机制
 *    - 维护登录状态枚举和状态转换
 * 
 * 3. 行情订阅管理：
 *    - 管理合约行情的订阅和退订
 *    - 支持批量订阅操作，提高效率
 *    - 处理合约代码格式转换（交易所前缀处理）
 *    - 维护订阅列表缓存，支持断线重连后自动恢复
 * 
 * 4. 数据处理引擎：
 *    - 接收CTP Mini推送的深度行情数据
 *    - 执行数据质量控制和有效性验证
 *    - 将原始CTP格式转换为WonderTrader标准格式
 *    - 处理特殊价格值和异常数据过滤
 * 
 * 5. 时间管理系统：
 *    - 处理交易日和时间戳转换
 *    - 支持夜盘跨日场景的时间修正
 *    - 管理不同时区和交易时段
 *    - 提供统一的时间基准
 * 
 * 6. 错误处理机制：
 *    - 统一的错误信息处理和日志记录
 *    - 网络异常检测和恢复
 *    - 数据异常识别和过滤
 *    - 心跳监控和连接保活
 * 
 * 7. 动态库集成：
 *    - 支持运行时动态加载CTP Mini API库
 *    - 处理不同版本API的兼容性
 *    - 跨平台函数符号解析
 *    - 资源管理和库卸载
 * 
 * 技术特性：
 * - 双重继承设计：同时实现IParserApi标准接口和CThostFtdcMdSpi回调接口
 * - 异步事件驱动：所有操作采用异步模式，避免阻塞主线程
 * - 内存优化：使用对象池和引用计数管理Tick数据对象
 * - 线程安全：处理多线程环境下的数据访问同步
 * - 配置驱动：支持灵活的参数配置，适应不同交易环境
 * 
 * 与标准CTP Parser的区别：
 * 1. API版本：使用CTP Mini 1.5.8，而非标准CTP 6.x版本
 * 2. 资源占用：更低的内存和CPU占用，适合资源受限环境
 * 3. 功能精简：专注核心行情功能，去除不必要的复杂特性
 * 4. 启动速度：更快的初始化和连接建立速度
 * 5. 兼容性：更好的向下兼容性和稳定性
 * 
 * 该解析器是WonderTrader框架中重要的行情数据源适配器，为量化交易策略提供
 * 实时、准确、高效的期货行情数据支持。
 */

#pragma once                                    // 防止头文件重复包含的现代预处理指令
#include "../Includes/IParserApi.h"             // 包含WonderTrader标准行情解析器接口定义
#include "../Share/DLLHelper.hpp"               // 包含动态库操作辅助工具类
#include "../API/CTPMini1.5.8/ThostFtdcMdApi.h" // 包含CTP Mini行情API头文件（版本1.5.8）
#include <map>                                  // 包含STL映射容器，用于数据管理

NS_WTP_BEGIN                                    // 开始WonderTrader命名空间
class WTSTickData;                              // 前向声明：WonderTrader标准Tick数据类
NS_WTP_END                                      // 结束WonderTrader命名空间

USING_NS_WTP;                                   // 使用WonderTrader命名空间，简化类名引用

/**
 * @class ParserCTPMini
 * @brief CTP Mini行情解析器类 - 专门用于处理CTP Mini行情数据的解析器实现
 * 
 * 该类采用双重继承设计模式，既实现了WonderTrader框架的标准行情解析接口（IParserApi），
 * 又继承了CTP Mini官方的行情回调接口（CThostFtdcMdSpi），形成了完整的行情数据处理链路。
 * 
 * 类设计特点：
 * 1. 适配器模式：将CTP Mini API适配为WonderTrader标准接口
 * 2. 观察者模式：通过回调机制通知行情数据变化
 * 3. 状态模式：使用状态机管理连接和登录状态
 * 4. 单例模式：每个实例管理一个独立的CTP Mini连接
 * 
 * 核心职责：
 * - 建立和维护与CTP Mini行情服务器的连接
 * - 处理用户身份验证和会话管理
 * - 管理合约行情的订阅和退订
 * - 接收并转换CTP Mini推送的深度行情数据
 * - 执行数据质量控制和异常处理
 * - 将处理后的数据通过标准接口传递给上层应用
 * 
 * 线程模型：
 * - CTP Mini API回调函数在独立的工作线程中执行
 * - 需要确保数据访问的线程安全性
 * - 使用异步模式避免阻塞主线程
 * 
 * 内存管理：
 * - 使用智能指针和引用计数管理对象生命周期
 * - 及时释放不再使用的资源
 * - 避免内存泄漏和野指针问题
 */
class ParserCTPMini : public IParserApi, public CThostFtdcMdSpi
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化ParserCTPMini对象，设置默认参数和初始状态。
	 * 构造函数不执行实际的连接操作，连接需要通过init()和connect()方法建立。
	 * 
	 * 初始化内容：
	 * - 设置成员变量初始值
	 * - 初始化登录状态为未登录
	 * - 清空连接参数和订阅列表
	 * - 设置请求ID初始值
	 */
	ParserCTPMini();
	
	/**
	 * @brief 虚析构函数
	 * 
	 * 确保派生类对象能够正确析构，释放所有占用的资源。
	 * 包括断开网络连接、释放API对象、清理缓存数据等。
	 * 
	 * 析构操作：
	 * - 断开与CTP Mini服务器的连接
	 * - 释放CTP Mini API对象
	 * - 清理动态分配的内存
	 * - 重置成员变量状态
	 */
	virtual ~ParserCTPMini();

public:
	/**
	 * @enum LoginStatus
	 * @brief 登录状态枚举 - 用于管理与CTP Mini服务器的登录状态
	 * 
	 * 该枚举定义了解析器与CTP Mini服务器连接过程中的不同登录状态，
	 * 用于状态机模式的实现，确保操作在正确的状态下执行。
	 * 
	 * 状态转换流程：
	 * LS_NOTLOGIN -> LS_LOGINING -> LS_LOGINED
	 *      ^                            |
	 *      |____________________________|
	 *              (断线重连)
	 */
	enum LoginStatus
	{
		LS_NOTLOGIN,    ///< 未登录状态：初始状态或登出后的状态，需要执行登录操作
		LS_LOGINING,    ///< 正在登录状态：已发送登录请求，等待服务器响应的中间状态
		LS_LOGINED      ///< 已登录状态：登录成功，可以进行行情订阅等操作的正常工作状态
	};

//==================== IParserApi接口实现 ====================
// 以下方法实现了WonderTrader标准行情解析器接口，提供统一的行情数据访问能力
public:
	/**
	 * @brief 初始化解析器
	 * @param config 配置参数对象，包含连接和认证信息
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该方法是解析器的核心初始化函数，负责以下工作：
	 * 1. 解析和验证配置参数（前置地址、经纪商、用户名、密码等）
	 * 2. 设置流控文件目录，用于存储CTP Mini的流控数据
	 * 3. 动态加载CTP Mini API库（thostmduserapi.dll/.so）
	 * 4. 创建CTP Mini API实例并进行基本配置
	 * 5. 注册回调接口和前置服务器地址
	 * 
	 * 配置参数说明：
	 * - front: CTP Mini前置服务器地址，格式"tcp://ip:port"（必选）
	 * - broker: 经纪商代码，如"9999"（必选）
	 * - user: 用户账号（必选）
	 * - pass: 用户密码（必选）
	 * - flowdir: 流控文件目录（可选，默认"CTPMiniMDFlow"）
	 * - ctpmodule: CTP Mini动态库名称（可选，默认"thostmduserapi"）
	 * 
	 * @note 此方法仅进行初始化配置，不建立实际连接。连接需调用connect()方法。
	 */
	virtual bool init(WTSVariant* config) override;

	/**
	 * @brief 释放解析器资源
	 * 
	 * 释放解析器占用的所有资源，包括：
	 * - 断开与CTP Mini服务器的连接
	 * - 释放CTP Mini API对象
	 * - 清理内存缓存
	 * - 重置内部状态
	 * 
	 * @note 调用此方法后，解析器将无法继续使用，需要重新初始化
	 */
	virtual void release() override;

	/**
	 * @brief 建立连接
	 * @return bool 连接启动成功返回true，失败返回false
	 * 
	 * 启动与CTP Mini行情服务器的连接过程。该方法是异步的，
	 * 实际连接状态通过OnFrontConnected()回调通知。
	 * 
	 * 连接过程：
	 * 1. 调用CTP Mini API的Init()方法启动连接线程
	 * 2. CTP Mini内部建立TCP连接
	 * 3. 连接成功后触发OnFrontConnected()回调
	 * 4. 在回调中自动发起登录请求
	 * 
	 * @note 连接是异步过程，返回true仅表示连接请求已发送
	 */
	virtual bool connect() override;

	/**
	 * @brief 断开连接
	 * @return bool 断开操作启动成功返回true，失败返回false
	 * 
	 * 主动断开与CTP Mini行情服务器的连接。该方法执行以下操作：
	 * 1. 取消注册回调接口（避免收到断开后的回调）
	 * 2. 调用CTP Mini API的Release()方法释放连接
	 * 3. 清理API对象引用
	 * 4. 重置连接状态
	 * 
	 * @note 断开是异步过程，可能需要一定时间完成
	 */
	virtual bool disconnect() override;

	/**
	 * @brief 检查连接状态
	 * @return bool 已连接返回true，未连接返回false
	 * 
	 * 检查当前是否与CTP Mini服务器保持连接。
	 * 该方法通过检查API对象指针是否有效来判断连接状态。
	 * 
	 * @note 此方法仅检查API对象状态，不检查网络连接质量
	 */
	virtual bool isConnected() override;

	/**
	 * @brief 订阅行情数据
	 * @param vecSymbols 要订阅的合约代码集合
	 * 
	 * 订阅指定合约的实时行情数据。支持以下功能：
	 * 1. 批量订阅多个合约，提高订阅效率
	 * 2. 自动处理合约代码格式（支持交易所前缀）
	 * 3. 缓存订阅列表，支持断线重连后自动恢复
	 * 4. 过滤重复订阅请求
	 * 
	 * 合约代码格式：
	 * - 支持带交易所前缀：如"SHFE.rb2105"
	 * - 支持不带前缀：如"rb2105"
	 * - 自动提取合约代码部分进行订阅
	 * 
	 * @note 如果在未登录状态下调用，订阅请求将被缓存，登录成功后自动执行
	 */
	virtual void subscribe(const CodeSet &vecSymbols) override;
	
	/**
	 * @brief 退订行情数据
	 * @param vecSymbols 要退订的合约代码集合
	 * 
	 * 取消订阅指定合约的实时行情数据。
	 * 
	 * @note 当前实现为空函数，CTP Mini API对退订支持有限
	 */
	virtual void unsubscribe(const CodeSet &vecSymbols) override;

	/**
	 * @brief 注册事件监听器
	 * @param listener 事件监听器指针，用于接收解析器事件和行情数据
	 * 
	 * 注册一个事件监听器来接收解析器产生的各种事件和数据：
	 * 1. 连接状态变化事件（连接、断开、登录等）
	 * 2. 实时行情数据推送
	 * 3. 错误信息和日志消息
	 * 4. 基础数据管理器访问
	 * 
	 * 监听器接口（IParserSpi）包含：
	 * - handleEvent(): 处理连接状态事件
	 * - handleQuote(): 处理实时行情数据
	 * - handleParserLog(): 处理日志消息
	 * - getBaseDataMgr(): 获取基础数据管理器
	 * 
	 * @note 监听器对象的生命周期由调用者管理，解析器仅保存指针引用
	 */
	virtual void registerSpi(IParserSpi* listener) override;


//==================== CThostFtdcMdSpi回调接口实现 ====================
// 以下方法实现了CTP Mini官方行情API的回调接口，用于处理服务器推送的各种事件和数据
public:
	/**
	 * @brief 错误响应回调
	 * @param pRspInfo 响应信息结构体指针，包含错误代码和错误描述
	 * @param nRequestID 请求ID，用于标识对应的请求
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当CTP Mini服务器返回错误信息时触发此回调。
	 * 该方法负责处理各种错误情况，包括：
	 * - 网络连接错误
	 * - 认证失败错误
	 * - 订阅失败错误
	 * - 其他业务错误
	 * 
	 * 错误处理策略：
	 * 1. 解析错误代码和描述信息
	 * 2. 记录详细的错误日志
	 * 3. 根据错误类型决定是否需要重试
	 * 4. 通知上层应用错误状态
	 */
	virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 前置连接成功回调
	 * 
	 * 当与CTP Mini前置服务器建立TCP连接成功时触发此回调。
	 * 这是连接过程的第一步，表示网络连接已建立，但还需要进行用户登录。
	 * 
	 * 回调处理逻辑：
	 * 1. 更新连接状态标识
	 * 2. 记录连接成功日志
	 * 3. 通知上层应用连接事件（WPE_Connect）
	 * 4. 自动发起用户登录请求（ReqUserLogin）
	 * 
	 * @note 此回调在CTP Mini内部工作线程中执行，需注意线程安全
	 */
	virtual void OnFrontConnected();

	/**
	 * @brief 用户登录响应回调
	 * @param pRspUserLogin 用户登录响应结构体指针，包含登录结果信息
	 * @param pRspInfo 响应信息结构体指针，包含错误代码和错误描述
	 * @param nRequestID 请求ID，对应登录请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当用户登录请求得到服务器响应时触发此回调。
	 * 登录成功后才能进行行情数据订阅等操作。
	 * 
	 * 处理流程：
	 * 1. 检查登录是否成功（通过pRspInfo判断）
	 * 2. 如果成功：
	 *    - 获取并保存交易日信息
	 *    - 更新登录状态为已登录
	 *    - 通知上层应用登录成功事件（WPE_Login）
	 *    - 执行缓存的行情订阅请求（SubscribeMarketData）
	 * 3. 如果失败：记录错误日志并通知上层应用
	 */
	virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 用户登出响应回调
	 * @param pUserLogout 用户登出结构体指针，包含登出信息
	 * @param pRspInfo 响应信息结构体指针，包含错误代码和错误描述
	 * @param nRequestID 请求ID，对应登出请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当用户主动登出或被服务器强制登出时触发此回调。
	 * 
	 * 处理逻辑：
	 * 1. 更新登录状态为未登录
	 * 2. 清理会话相关数据
	 * 3. 通知上层应用登出事件（WPE_Logout）
	 * 4. 停止所有行情数据推送
	 */
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 前置连接断开回调
	 * @param nReason 断开原因代码
	 * 
	 * 当与CTP Mini前置服务器的连接意外断开时触发此回调。
	 * 
	 * 断开原因代码说明：
	 * - 0x1001: 网络读失败
	 * - 0x1002: 网络写失败
	 * - 0x2001: 接收心跳超时
	 * - 0x2002: 发送心跳失败
	 * - 0x2003: 收到错误报文
	 * 
	 * 处理逻辑：
	 * 1. 记录断开原因和详细日志
	 * 2. 重置登录状态和交易日信息
	 * 3. 通知上层应用连接关闭事件（WPE_Close）
	 * 4. 清理订阅状态（CTP Mini会自动重连并恢复订阅）
	 */
	virtual void OnFrontDisconnected(int nReason);

	/**
	 * @brief 退订行情响应回调
	 * @param pSpecificInstrument 合约信息结构体指针
	 * @param pRspInfo 响应信息结构体指针，包含错误代码和错误描述
	 * @param nRequestID 请求ID，对应退订请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当退订行情请求得到服务器响应时触发此回调。
	 * 
	 * @note 当前实现为空函数，因为CTP Mini对退订支持有限，
	 *       且实际应用中很少需要退订行情数据
	 */
	virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 深度行情数据推送回调 - 核心数据处理方法
	 * @param pDepthMarketData 深度行情数据结构体指针，包含完整的五档行情信息
	 * 
	 * 这是最重要的回调方法，用于处理CTP Mini推送的实时深度行情数据。
	 * 该方法负责将原始CTP数据转换为WonderTrader标准格式。
	 * 
	 * 数据处理流程：
	 * 1. 数据验证：
	 *    - 检查基础数据管理器是否有效
	 *    - 验证合约信息是否存在
	 *    - 过滤无效的价格和数量数据
	 * 
	 * 2. 时间处理：
	 *    - 解析行情时间戳（ActionDay + UpdateTime + UpdateMillisec）
	 *    - 处理夜盘跨日的特殊情况
	 *    - 修正异常时间数据
	 * 
	 * 3. 数据转换：
	 *    - 创建WTSTickData对象
	 *    - 填充价格数据（最新价、开高低收、涨跌停价等）
	 *    - 填充数量数据（成交量、持仓量、成交额等）
	 *    - 填充五档买卖盘数据
	 * 
	 * 4. 质量控制：
	 *    - 处理DBL_MAX等无效价格标识
	 *    - 特殊交易所规则处理（如郑商所成交额缩放）
	 *    - 数据完整性验证
	 * 
	 * 5. 数据分发：
	 *    - 通过监听器接口推送给上层应用
	 *    - 释放临时对象内存
	 * 
	 * @note 此方法在高频场景下被频繁调用，需要特别注意性能优化
	 */
	virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

	/**
	 * @brief 订阅行情响应回调
	 * @param pSpecificInstrument 合约信息结构体指针，包含订阅的合约代码
	 * @param pRspInfo 响应信息结构体指针，包含错误代码和错误描述
	 * @param nRequestID 请求ID，对应订阅请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当订阅行情请求得到服务器响应时触发此回调。
	 * 用于确认订阅操作的成功或失败状态。
	 * 
	 * 处理逻辑：
	 * 1. 检查订阅是否成功（通过pRspInfo判断）
	 * 2. 记录订阅结果日志
	 * 3. 更新内部订阅状态缓存
	 * 4. 如果失败，可以考虑重试机制
	 */
	virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 心跳警告回调
	 * @param nTimeLapse 心跳间隔时间（秒）
	 * 
	 * 当CTP Mini检测到心跳间隔超过预设阈值时触发此回调。
	 * 用于监控网络连接质量和服务器响应状态。
	 * 
	 * 处理逻辑：
	 * 1. 记录心跳警告日志
	 * 2. 监控网络连接质量
	 * 3. 必要时可以触发重连机制
	 * 
	 * @note 频繁的心跳警告可能表示网络不稳定，需要关注连接质量
	 */
	virtual void OnHeartBeatWarning(int nTimeLapse);

//==================== 私有辅助方法 ====================
// 以下方法为内部使用的辅助功能，不对外暴露
private:
	/**
	 * @brief 发送用户登录请求
	 * 
	 * 构造并发送用户登录请求到CTP Mini服务器。
	 * 该方法在前置连接成功后自动调用。
	 * 
	 * 登录请求内容：
	 * - BrokerID: 经纪商代码
	 * - UserID: 用户账号
	 * - Password: 用户密码
	 * - UserProductInfo: 产品信息标识（使用WT_PRODUCT宏）
	 * 
	 * 处理流程：
	 * 1. 检查API对象有效性
	 * 2. 构造登录请求结构体
	 * 3. 填充必要的登录信息
	 * 4. 调用API发送登录请求
	 * 5. 处理发送结果和错误情况
	 * 
	 * @note 登录结果通过OnRspUserLogin回调异步返回
	 */
	void ReqUserLogin();
	
	/**
	 * @brief 订阅合约行情数据
	 * 
	 * 向CTP Mini服务器发送行情订阅请求，订阅缓存中的所有合约。
	 * 该方法在登录成功后自动调用。
	 * 
	 * 订阅处理逻辑：
	 * 1. 检查订阅列表是否为空
	 * 2. 构造合约代码数组
	 * 3. 处理合约代码格式（去除交易所前缀）
	 * 4. 调用API批量订阅行情
	 * 5. 记录订阅结果和错误信息
	 * 6. 清理临时资源
	 * 
	 * 合约代码处理：
	 * - 支持"SHFE.rb2105"格式，自动提取"rb2105"部分
	 * - 支持"rb2105"格式，直接使用
	 * - 批量处理提高订阅效率
	 * 
	 * @note 订阅结果通过OnRspSubMarketData回调异步返回
	 */
	void SubscribeMarketData();
	
	/**
	 * @brief 检查CTP响应错误信息
	 * @param pRspInfo CTP响应信息结构体指针
	 * @return bool 有错误返回true，无错误返回false
	 * 
	 * 检查CTP Mini服务器返回的响应信息中是否包含错误。
	 * 该方法用于统一处理各种CTP回调中的错误信息。
	 * 
	 * 检查逻辑：
	 * 1. 验证响应信息指针有效性
	 * 2. 检查错误代码是否为0（成功）
	 * 3. 记录错误信息到日志
	 * 4. 返回错误状态
	 * 
	 * @note 当前实现返回false，表示不处理错误（简化实现）
	 */
	bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);

//==================== 私有成员变量 ====================
// 以下为类的内部状态和配置数据，不对外暴露
private:
	// ---------- 交易和状态信息 ----------
	uint32_t			m_uTradingDate;		///< 当前交易日，格式YYYYMMDD，从CTP服务器获取
	LoginStatus			m_loginState;		///< 登录状态，使用枚举值管理连接状态机
	CThostFtdcMdApi*	m_pUserAPI;			///< CTP Mini行情API对象指针，核心接口对象

	// ---------- 连接配置参数 ----------
	std::string			m_strFrontAddr;		///< 前置服务器地址，格式"tcp://ip:port"
	std::string			m_strBroker;		///< 经纪商代码，如"9999"
	std::string			m_strUserID;		///< 用户账号，用于登录认证
	std::string			m_strPassword;		///< 用户密码，用于登录认证
	std::string			m_strFlowDir;		///< 流控文件目录，用于存储CTP流控数据

	// ---------- 订阅管理 ----------
	CodeSet				m_filterSubs;		///< 订阅合约列表，缓存待订阅的合约代码集合

	// ---------- 请求管理 ----------
	int					m_iRequestID;		///< 请求ID计数器，用于标识每个请求的唯一性

	// ---------- 回调和数据管理 ----------
	IParserSpi*			m_sink;				///< 事件监听器指针，用于向上层推送数据和事件
	IBaseDataMgr*		m_pBaseDataMgr;		///< 基础数据管理器指针，用于验证合约信息

	// ---------- 动态库管理 ----------
	DllHandle			m_hInstCTP;			///< CTP Mini动态库句柄，用于运行时加载API库
	typedef CThostFtdcMdApi* (*CTPCreator)(const char *, const bool, const bool);	///< CTP API创建函数指针类型定义
	CTPCreator			m_funcCreator;		///< CTP API创建函数指针，用于动态创建API实例
};

