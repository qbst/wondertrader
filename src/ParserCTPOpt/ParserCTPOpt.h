/*!
 * \file ParserCTPOpt.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTP期权行情解析器头文件 - WonderTrader框架中的CTP期权行情数据解析模块
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WonderTrader量化交易框架中专门用于解析CTP期权行情数据的核心模块头文件。
 * CTP期权（CTPOpt）是上海期货信息技术有限公司推出的期权行情接口，基于CTPOpt 3.5.8版本，
 * 专门用于处理期权合约的实时行情数据，为期权交易策略提供数据支持。
 * 
 * 核心设计理念：
 * 1. 期权专用设计：基于CTP期权API 3.5.8版本，专门处理期权合约行情数据
 * 2. 标准化接口：实现WonderTrader标准行情解析接口，确保与框架的无缝集成
 * 3. 事件驱动：采用异步回调模式处理期权行情数据，保证实时性和高性能
 * 4. 模块化架构：支持动态加载，便于部署和维护
 * 
 * 主要功能模块：
 * 
 * 1. 连接管理系统：
 *    - 管理与CTP期权行情服务器的TCP连接
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
 * 3. 期权行情订阅管理：
 *    - 管理期权合约行情的订阅和退订
 *    - 支持批量订阅操作，提高效率
 *    - 处理期权合约代码格式转换
 *    - 维护订阅列表缓存，支持断线重连后自动恢复
 * 
 * 4. 期权数据处理引擎：
 *    - 接收CTP期权推送的深度行情数据
 *    - 执行期权数据质量控制和有效性验证
 *    - 将原始CTP期权格式转换为WonderTrader标准格式
 *    - 处理期权特有的数据字段（如隐含波动率、理论价格等）
 * 
 * 5. 时间管理系统：
 *    - 处理期权交易日和时间戳转换
 *    - 支持本地时间戳模式选择
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
 *    - 支持运行时动态加载CTP期权API库
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
 * 与其他CTP Parser的区别：
 * 1. API版本：使用CTP期权API 3.5.8，专门用于期权数据
 * 2. 数据结构：针对期权合约的特殊数据结构优化
 * 3. 时间处理：支持本地时间戳选项，适应期权交易特点
 * 4. 符号解析：使用期权专用的动态库符号名
 * 5. 数据验证：针对期权数据的特殊验证逻辑
 * 
 * 该解析器是WonderTrader框架中重要的期权行情数据源适配器，为期权量化交易策略
 * 提供实时、准确、高效的期权行情数据支持。
 */

#pragma once                                    // 防止头文件重复包含的现代预处理指令
#include "../Includes/IParserApi.h"             // 包含WonderTrader标准行情解析器接口定义
#include "../Share/DLLHelper.hpp"               // 包含动态库操作辅助工具
#include "../API/CTPOpt3.5.8/ThostFtdcMdApi.h" // 包含CTP期权行情API头文件（版本3.5.8）
#include <map>                                  // 包含STL映射容器，用于数据管理

NS_WTP_BEGIN                                    // 开始WonderTrader命名空间
class WTSTickData;                              // 前向声明：WonderTrader Tick数据类
NS_WTP_END                                      // 结束WonderTrader命名空间

USING_NS_WTP;                                   // 使用WonderTrader命名空间

/**
 * @class ParserCTPOpt
 * @brief CTP期权行情解析器类 - 专门用于处理CTP期权行情数据的解析器实现
 * 
 * 该类采用双重继承设计模式，既实现了WonderTrader框架的标准行情解析接口（IParserApi），
 * 又继承了CTP期权官方的行情回调接口（CThostFtdcMdSpi），形成了完整的期权行情数据处理链路。
 * 
 * 类设计特点：
 * 1. 适配器模式：将CTP期权API适配为WonderTrader标准接口
 * 2. 观察者模式：通过回调机制通知期权行情数据变化
 * 3. 状态模式：使用状态机管理连接和登录状态
 * 4. 单例模式：每个实例管理一个独立的CTP期权连接
 * 
 * 核心职责：
 * - 建立和维护与CTP期权行情服务器的连接
 * - 处理用户身份验证和会话管理
 * - 管理期权合约行情的订阅和退订
 * - 接收并转换CTP期权推送的深度行情数据
 * - 执行期权数据质量控制和异常处理
 * - 将处理后的数据通过标准接口传递给上层应用
 * 
 * 期权特性支持：
 * - 支持期权合约的特殊数据字段处理
 * - 提供本地时间戳模式选择
 * - 针对期权交易时段的特殊处理
 * - 优化的期权数据验证逻辑
 * 
 * 线程模型：
 * - CTP期权API回调函数在独立的工作线程中执行
 * - 需要确保数据访问的线程安全性
 * - 使用异步模式避免阻塞主线程
 * 
 * 内存管理：
 * - 使用智能指针和引用计数管理对象生命周期
 * - 及时释放不再使用的资源
 * - 避免内存泄漏和野指针问题
 */
class ParserCTPOpt :	public IParserApi, public CThostFtdcMdSpi
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化ParserCTPOpt对象，设置默认参数和状态。
	 * 不进行实际的连接操作，连接需要通过init()和connect()方法建立。
	 * 
	 * 初始化内容：
	 * - 设置所有成员变量的默认值
	 * - 初始化登录状态为未登录
	 * - 清空订阅列表和配置参数
	 * - 准备动态库加载环境
	 */
	ParserCTPOpt();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理ParserCTPOpt对象，释放所有占用的资源。
	 * 确保对象销毁时不会造成内存泄漏或资源泄漏。
	 * 
	 * 清理内容：
	 * - 断开与CTP期权服务器的连接
	 * - 释放动态库资源
	 * - 清理订阅列表和缓存数据
	 * - 重置所有状态变量
	 */
	virtual ~ParserCTPOpt();

public:
	/**
	 * @enum LoginStatus
	 * @brief 登录状态枚举 - 定义CTP期权连接的登录状态
	 * 
	 * 该枚举用于跟踪和管理与CTP期权服务器的登录状态，
	 * 确保在正确的状态下执行相应的操作。
	 * 
	 * 状态转换流程：
	 * LS_NOTLOGIN -> LS_LOGINING -> LS_LOGINED
	 *      ^                           |
	 *      +---------------------------+
	 *           (断开连接或登出)
	 */
	enum LoginStatus
	{
		LS_NOTLOGIN,    ///< 未登录状态：尚未开始登录或登录失败
		LS_LOGINING,    ///< 登录中状态：正在进行登录认证过程
		LS_LOGINED      ///< 已登录状态：成功登录，可以进行数据订阅和接收
	};

//==================== IParserApi接口实现 ====================
// 以下方法实现了WonderTrader标准行情解析器接口，提供统一的期权行情数据访问能力
public:
	/**
	 * @brief 初始化期权行情解析器
	 * @param config 配置参数对象，包含连接和认证信息
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该方法是解析器的核心初始化函数，负责以下工作：
	 * 1. 解析和验证配置参数（前置地址、经纪商、用户名、密码等）
	 * 2. 设置流控文件目录，用于存储CTP期权的流控数据
	 * 3. 动态加载CTP期权API库（soptthostmduserapi_se.dll/.so）
	 * 4. 创建CTP期权API实例并进行基本配置
	 * 5. 注册回调接口和前置服务器地址
	 * 
	 * 配置参数说明：
	 * - front: 前置服务器地址，格式"tcp://ip:port"
	 * - broker: 经纪商代码，如"9999"
	 * - user: 用户账号
	 * - pass: 用户密码
	 * - localtime: 是否使用本地时间戳（期权特有配置）
	 * - flowdir: 流控文件目录，默认"CTPOptMDFlow"
	 * - ctpmodule: CTP期权库模块名，默认"soptthostmduserapi_se"
	 * 
	 * 期权特殊配置：
	 * - 支持本地时间戳模式，适应期权交易特点
	 * - 使用期权专用的动态库和符号名
	 * - 针对期权数据的特殊初始化逻辑
	 * 
	 * @note 该方法不建立网络连接，仅进行基础配置和库加载
	 */
	virtual bool init(WTSVariant* config) override;

	/**
	 * @brief 释放解析器资源
	 * 
	 * 该方法负责释放解析器占用的所有资源，包括网络连接、动态库等。
	 * 通常在解析器对象销毁前调用，确保资源得到正确清理。
	 * 
	 * 释放流程：
	 * 1. 调用disconnect()方法断开连接并释放资源
	 * 2. 确保所有占用的资源都得到正确释放
	 * 
	 * @note 该方法是IParserApi接口的实现，提供统一的资源释放接口
	 */
	virtual void release() override;

	/**
	 * @brief 连接到CTP期权行情服务器
	 * @return bool 连接启动成功返回true，失败返回false
	 * 
	 * 该方法启动与CTP期权行情服务器的连接过程。
	 * 连接是异步进行的，实际的连接结果通过OnFrontConnected/OnFrontDisconnected回调通知。
	 * 
	 * 连接流程：
	 * 1. 检查API对象是否已创建
	 * 2. 调用API的Init()方法启动连接过程
	 * 3. CTP期权内部开始TCP连接和握手过程
	 * 4. 连接结果通过回调函数异步通知
	 * 
	 * 状态变化：
	 * - 连接成功：触发OnFrontConnected回调
	 * - 连接失败：触发OnFrontDisconnected回调
	 * - 连接断开：触发OnFrontDisconnected回调
	 * 
	 * @note 该方法是非阻塞的，立即返回，不等待连接完成
	 */
	virtual bool connect() override;

	/**
	 * @brief 断开与CTP期权行情服务器的连接
	 * @return bool 断开成功返回true，失败返回false
	 * 
	 * 该方法负责安全地断开与CTP期权服务器的连接，并释放所有相关资源。
	 * 包括网络连接、API对象、动态库等。
	 * 
	 * 断开流程：
	 * 1. 注销SPI回调接口，避免断开过程中的回调
	 * 2. 调用API的Release()方法释放CTP期权API对象
	 * 3. 将API指针置空，防止悬空指针
	 * 4. 清理相关状态和缓存数据
	 * 
	 * 安全性考虑：
	 * - 先注销回调再释放，避免释放过程中的异常回调
	 * - 按顺序释放资源，确保依赖关系正确处理
	 * - 所有指针置空，防止重复释放或悬空访问
	 * 
	 * @note 该方法确保资源的完全清理，可以安全地重复调用
	 */
	virtual bool disconnect() override;

	/**
	 * @brief 检查连接状态
	 * @return bool 已连接返回true，未连接返回false
	 * 
	 * 该方法用于检查与CTP期权服务器的连接状态。
	 * 
	 * 判断逻辑：
	 * - 通过检查API对象指针是否为空来判断连接状态
	 * - API对象存在表示连接已建立（或正在建立）
	 * - API对象为空表示未连接或连接已断开
	 * 
	 * 注意事项：
	 * - 该方法只检查API对象是否存在，不检查网络连接状态
	 * - 实际的网络连接状态通过回调事件通知
	 * - 登录状态需要通过其他方式检查
	 * 
	 * @note 这是一个轻量级的状态检查方法，不涉及网络操作
	 */
	virtual bool isConnected() override;

	/**
	 * @brief 订阅期权行情数据
	 * @param vecSymbols 要订阅的期权合约代码集合
	 * 
	 * 该方法用于订阅指定期权合约的行情数据。
	 * 根据当前连接状态采用不同的处理策略。
	 * 
	 * 处理策略：
	 * 1. 未登录状态：将合约列表缓存，等待登录成功后自动订阅
	 * 2. 已登录状态：立即发送订阅请求到CTP期权服务器
	 * 
	 * 期权合约代码处理：
	 * - 支持带交易所前缀的格式（"SHFE.cu2105C4500"）
	 * - 支持不带前缀的格式（"cu2105C4500"）
	 * - 自动提取CTP期权需要的合约代码部分
	 * 
	 * 批量订阅：
	 * - 支持同时订阅多个期权合约
	 * - 提高订阅效率，减少网络交互
	 * 
	 * @note 期权合约代码格式需要符合CTP期权规范
	 */
	virtual void subscribe(const CodeSet &vecSymbols) override;
	
	/**
	 * @brief 退订期权行情数据
	 * @param vecSymbols 要退订的期权合约代码集合
	 * 
	 * 该方法用于退订指定期权合约的行情数据。
	 * 
	 * 退订处理：
	 * 1. 处理期权合约代码格式转换
	 * 2. 调用CTP期权API的UnSubscribeMarketData方法
	 * 3. 从订阅缓存中移除相应合约
	 * 4. 记录退订结果
	 * 
	 * @note 当前实现可能为空，期权连接断开时会自动清除所有订阅
	 */
	virtual void unsubscribe(const CodeSet &vecSymbols) override;

	/**
	 * @brief 注册事件监听器
	 * @param listener 事件监听器指针，用于接收期权行情数据和事件通知
	 * 
	 * 该方法用于注册事件监听器，监听器负责接收解析器推送的期权行情数据和各种事件通知。
	 * 
	 * 注册流程：
	 * 1. 保存监听器指针到成员变量
	 * 2. 如果监听器有效，获取基础数据管理器引用
	 * 3. 基础数据管理器用于验证期权合约信息的有效性
	 * 
	 * 监听器功能：
	 * - 接收期权Tick行情数据（handleQuote）
	 * - 接收连接事件通知（handleEvent）
	 * - 接收日志信息（handleParserLog）
	 * - 提供基础数据管理服务（getBaseDataMgr）
	 * 
	 * 依赖关系：
	 * - 监听器必须在连接建立前注册
	 * - 基础数据管理器用于期权行情数据验证
	 * - 所有期权行情处理都依赖于有效的监听器
	 * 
	 * @note 监听器是解析器与上层应用的关键接口，必须正确注册
	 */
	virtual void registerSpi(IParserSpi* listener) override;


//==================== CThostFtdcMdSpi回调接口实现 ====================
// 以下方法实现了CTP期权官方SPI回调接口，处理CTP期权服务器的各种事件通知
public:
	/**
	 * @brief 响应错误回调方法
	 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
	 * @param nRequestID 请求ID，用于标识具体的请求
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当CTP期权服务器返回错误信息时调用此方法。
	 * 该方法用于处理各种操作产生的错误响应。
	 * 
	 * 处理逻辑：
	 * 1. 检查响应信息中的错误内容
	 * 2. 记录错误信息到日志系统
	 * 3. 根据错误类型决定后续处理策略
	 * 
	 * 期权特殊错误：
	 * - 期权合约不存在或已过期
	 * - 期权交易权限不足
	 * - 期权数据订阅限制
	 * 
	 * @note 错误处理对期权交易的稳定性至关重要
	 */
	virtual void OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 前置连接成功回调方法
	 * 
	 * 当与CTP期权前置服务器建立TCP连接成功后调用此方法。
	 * 连接成功后会自动触发用户登录流程。
	 * 
	 * 处理流程：
	 * 1. 记录连接成功的日志信息
	 * 2. 通知上层应用连接事件（WPE_Connect）
	 * 3. 自动发起用户登录请求
	 * 
	 * 事件传播：
	 * - 向WonderTrader框架发送连接成功事件
	 * - 启动后续的认证流程
	 * 
	 * 期权特性：
	 * - 期权服务器连接可能有特殊的验证流程
	 * - 需要确保期权交易权限的验证
	 * 
	 * @note 这是期权连接流程的第一步，成功后会自动进入登录阶段
	 */
	virtual void OnFrontConnected();

	/**
	 * @brief 用户登录响应回调方法
	 * @param pRspUserLogin 用户登录响应结构体，包含登录结果信息
	 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
	 * @param nRequestID 请求ID，对应登录请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当用户登录请求得到服务器响应时调用此方法。
	 * 登录成功后会获取交易日信息并启动期权行情订阅。
	 * 
	 * 处理流程：
	 * 1. 检查是否为最后一条响应且无错误
	 * 2. 从API获取当前交易日信息
	 * 3. 通知上层应用登录成功事件
	 * 4. 自动订阅缓存的期权行情合约
	 * 
	 * 期权特殊处理：
	 * - 验证期权交易权限
	 * - 获取期权交易日信息
	 * - 初始化期权数据处理环境
	 * 
	 * 关键数据：
	 * - 交易日：用于期权数据时间戳处理和跨日逻辑
	 * - 登录状态：影响后续的订阅和数据接收
	 * 
	 * @note 只有登录成功才能进行期权行情订阅和接收数据
	 */
	virtual void OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 用户登出响应回调方法
	 * @param pUserLogout 用户登出响应结构体
	 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
	 * @param nRequestID 请求ID，对应登出请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当用户登出请求得到服务器响应时调用此方法。
	 * 登出后将停止接收期权行情数据。
	 * 
	 * 处理流程：
	 * 1. 通知上层应用登出事件
	 * 2. 清理登录相关的状态信息
	 * 3. 停止数据接收和处理
	 * 
	 * 期权特殊处理：
	 * - 清理期权订阅状态
	 * - 重置期权数据处理环境
	 * - 释放期权相关资源
	 * 
	 * @note 登出后需要重新登录才能继续接收期权行情数据
	 */
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	/**
	 * @brief 前置连接断开回调方法
	 * @param nReason 断开原因代码
	 * 
	 * 当与CTP期权前置服务器的连接断开时调用此方法。
	 * 连接断开可能由网络异常、服务器维护、认证失败等原因导致。
	 * 
	 * 断开原因代码说明：
	 * - 0x1001: 网络读失败
	 * - 0x1002: 网络写失败  
	 * - 0x2001: 接收心跳超时
	 * - 0x2002: 发送心跳失败
	 * - 0x2003: 收到错误报文
	 * 
	 * 处理流程：
	 * 1. 记录断开原因到日志
	 * 2. 通知上层应用连接关闭事件
	 * 3. 清理连接相关状态
	 * 
	 * 期权特殊处理：
	 * - 清理所有期权订阅状态
	 * - 重置期权数据处理状态
	 * - 准备重连恢复机制
	 * 
	 * @note 连接断开后需要重新调用connect()方法建立连接
	 */
	virtual void OnFrontDisconnected( int nReason );

	/**
	 * @brief 退订行情响应回调方法
	 * @param pSpecificInstrument 指定合约结构体，包含退订的期权合约信息
	 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
	 * @param nRequestID 请求ID，对应退订请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当退订期权行情请求得到服务器响应时调用此方法。
	 * 
	 * 处理流程：
	 * 1. 检查退订结果是否成功
	 * 2. 记录退订结果到日志
	 * 3. 更新内部订阅状态
	 * 
	 * 期权特殊处理：
	 * - 处理期权合约的特殊退订逻辑
	 * - 更新期权订阅缓存
	 * - 验证期权合约状态
	 * 
	 * @note 当前实现可能为空，可根据需要添加退订结果处理逻辑
	 */
	virtual void OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 深度行情数据推送回调方法
	 * @param pDepthMarketData 深度行情数据结构体指针，包含完整的期权tick行情信息
	 * 
	 * 这是CTP期权行情解析器的核心方法，负责处理服务器推送的实时期权行情数据。
	 * 该方法将CTP期权原生格式的行情数据转换为WonderTrader标准格式。
	 * 
	 * 期权行情特殊处理：
	 * 1. 支持本地时间戳模式选择
	 * 2. 处理期权特有的数据字段
	 * 3. 执行期权数据质量控制
	 * 4. 时间戳处理和跨日逻辑
	 * 5. 期权合约信息验证
	 * 6. 数据格式转换和推送
	 * 
	 * 时间处理选项：
	 * - 本地时间戳模式：使用系统当前时间
	 * - CTP时间戳模式：使用CTP服务器时间
	 * 
	 * 期权数据验证：
	 * - 验证期权合约代码有效性
	 * - 检查期权价格数据合理性
	 * - 处理期权特有的数据异常
	 * 
	 * @note 该方法在高频场景下被频繁调用，需要确保处理效率
	 */
	virtual void OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pDepthMarketData );

	/**
	 * @brief 订阅行情响应回调方法
	 * @param pSpecificInstrument 指定合约结构体，包含订阅的期权合约信息
	 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
	 * @param nRequestID 请求ID，对应订阅请求的ID
	 * @param bIsLast 是否为最后一条响应消息
	 * 
	 * 当订阅期权行情请求得到服务器响应时调用此方法。
	 * 可以根据响应结果进行相应的处理。
	 * 
	 * 处理流程：
	 * 1. 检查订阅结果是否成功
	 * 2. 记录订阅结果到日志
	 * 3. 更新内部订阅状态
	 * 
	 * 期权特殊处理：
	 * - 验证期权合约订阅权限
	 * - 处理期权合约状态变化
	 * - 更新期权订阅缓存
	 * 
	 * @note 当前实现可能为空，可根据需要添加订阅结果处理逻辑
	 */
	virtual void OnRspSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast );

	/**
	 * @brief 心跳警告回调方法
	 * @param nTimeLapse 心跳间隔时间（秒），表示距离上次心跳的时间
	 * 
	 * 当CTP期权检测到心跳超时时调用此方法。
	 * 心跳机制用于检测客户端与期权服务器之间的连接状态。
	 * 
	 * 心跳警告原因：
	 * - 网络延迟较高，导致心跳响应缓慢
	 * - 网络不稳定，出现短暂的连接中断
	 * - 期权服务器负载较高，处理心跳请求较慢
	 * - 客户端处理能力不足，响应心跳较慢
	 * 
	 * 处理策略：
	 * 1. 记录心跳警告信息，用于网络质量监控
	 * 2. 如果频繁出现警告，可能需要检查网络状况
	 * 3. 严重情况下可能导致连接断开，需要重连
	 * 
	 * 期权特殊考虑：
	 * - 期权交易对实时性要求较高
	 * - 心跳异常可能影响期权策略执行
	 * - 需要特别关注期权交易时段的网络质量
	 * 
	 * @note 偶尔的心跳警告是正常的，频繁警告需要关注网络质量
	 */
	virtual void OnHeartBeatWarning( int nTimeLapse );

//==================== 私有辅助方法 ====================
// 以下方法为内部使用的辅助功能，不对外暴露
private:
	/**
	 * @brief 发送用户登录请求
	 * 
	 * 构造并发送用户登录请求到CTP期权服务器。
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
	 * 期权特殊处理：
	 * - 验证期权交易权限
	 * - 设置期权产品信息
	 * - 处理期权登录特殊逻辑
	 * 
	 * @note 登录结果通过OnRspUserLogin回调异步返回
	 */
	void ReqUserLogin();
	
	/**
	 * @brief 订阅期权行情数据
	 * 
	 * 向CTP期权服务器发送行情订阅请求，订阅缓存中的所有期权合约。
	 * 该方法在登录成功后自动调用。
	 * 
	 * 订阅处理逻辑：
	 * 1. 检查订阅列表是否为空
	 * 2. 构造期权合约代码数组
	 * 3. 处理期权合约代码格式（去除交易所前缀）
	 * 4. 调用API批量订阅期权行情
	 * 5. 记录订阅结果和错误信息
	 * 6. 清理临时资源
	 * 
	 * 期权合约代码处理：
	 * - 支持"SHFE.cu2105C4500"格式，自动提取"cu2105C4500"部分
	 * - 支持"cu2105C4500"格式，直接使用
	 * - 批量处理提高订阅效率
	 * 
	 * 期权特殊处理：
	 * - 验证期权合约有效性
	 * - 处理期权合约到期检查
	 * - 优化期权数据订阅策略
	 * 
	 * @note 订阅结果通过OnRspSubMarketData回调异步返回
	 */
	void SubscribeMarketData();
	
	/**
	 * @brief 检查CTP期权响应错误信息
	 * @param pRspInfo CTP期权响应信息结构体指针
	 * @return bool 有错误返回true，无错误返回false
	 * 
	 * 检查CTP期权服务器返回的响应信息中是否包含错误。
	 * 该方法用于统一处理各种CTP期权回调中的错误信息。
	 * 
	 * 检查逻辑：
	 * 1. 验证响应信息指针有效性
	 * 2. 检查错误代码是否为0（成功）
	 * 3. 记录错误信息到日志
	 * 4. 返回错误状态
	 * 
	 * 期权特殊错误处理：
	 * - 期权合约不存在错误
	 * - 期权交易权限不足错误
	 * - 期权数据订阅限制错误
	 * - 期权合约到期错误
	 * 
	 * @note 当前实现返回false，表示不处理错误（简化实现）
	 */
	bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);

//==================== 私有成员变量 ====================
// 以下为类的内部状态和配置数据，不对外暴露
private:
	// ---------- 交易和状态信息 ----------
	uint32_t			m_uTradingDate;		///< 当前交易日，格式YYYYMMDD，从CTP期权服务器获取
	LoginStatus			m_loginState;		///< 登录状态，使用枚举值管理连接状态机
	CThostFtdcMdApi*	m_pUserAPI;			///< CTP期权行情API对象指针，核心接口对象

	// ---------- 连接配置参数 ----------
	std::string			m_strFrontAddr;		///< 前置服务器地址，格式"tcp://ip:port"
	std::string			m_strBroker;		///< 经纪商代码，如"9999"
	std::string			m_strUserID;		///< 用户账号，用于登录认证
	std::string			m_strPassword;		///< 用户密码，用于登录认证
	bool				m_bLocalTime;		///< 本地时间戳标志，true使用本地时间，false使用CTP时间（期权特有配置）
	std::string			m_strFlowDir;		///< 流控文件目录，用于存储CTP期权流控数据

	// ---------- 订阅管理 ----------
	CodeSet				m_filterSubs;		///< 订阅期权合约列表，缓存待订阅的期权合约代码集合

	// ---------- 请求管理 ----------
	int					m_iRequestID;		///< 请求ID计数器，用于标识每个请求的唯一性

	// ---------- 回调和数据管理 ----------
	IParserSpi*			m_sink;				///< 事件监听器指针，用于向上层推送数据和事件
	IBaseDataMgr*		m_pBaseDataMgr;		///< 基础数据管理器指针，用于验证期权合约信息

	// ---------- 动态库管理 ----------
	DllHandle			m_hInstCTP;			///< CTP期权动态库句柄，用于运行时加载API库
	typedef CThostFtdcMdApi* (*CTPCreator)(const char *, const bool, const bool);	///< CTP期权API创建函数指针类型定义
	CTPCreator			m_funcCreator;		///< CTP期权API创建函数指针，用于动态创建API实例
};

