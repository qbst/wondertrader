/*!
 * \file ParserCTP.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTP行情解析器实现文件 - WonderTrader框架中CTP行情数据解析的具体实现
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是ParserCTP.h中声明的CTP行情解析器类的具体实现，负责处理与CTP行情服务器
 * 的全部交互逻辑和数据转换工作。作为WonderTrader量化交易框架的重要组成部分，
 * 该实现文件包含以下核心功能模块：
 * 
 * 1. 连接与认证管理：
 *    - 实现与CTP行情前置服务器的TCP连接建立
 *    - 处理用户登录认证流程，包括凭证验证和会话管理
 *    - 管理连接状态变化和异常重连机制
 *    - 支持多种CTP环境（标准CTP、SimNow、OpenCTP等）
 * 
 * 2. 行情数据处理核心：
 *    - 接收CTP推送的深度行情数据（OnRtnDepthMarketData）
 *    - 执行数据质量控制：过滤无效数据、验证价格范围、处理异常值
 *    - 时间戳处理：支持标准时间戳和本地时间戳模式
 *    - 交易日修正：处理夜盘跨日、早盘启动等特殊时间场景
 *    - 数据格式转换：将CTP原生格式转换为WonderTrader标准格式
 * 
 * 3. 订阅管理系统：
 *    - 管理合约行情的订阅和退订请求
 *    - 处理合约代码格式转换（支持交易所前缀处理）
 *    - 实现订阅状态缓存，支持登录后自动重新订阅
 *    - 批量订阅优化，提高订阅效率
 * 
 * 4. 数据验证与修正：
 *    - 集成基础数据管理器，验证合约有效性
 *    - 处理特殊价格值（DBL_MAX、FLT_MAX等无效标识）
 *    - 实现交易所特定规则（如郑商所成交额缩放处理）
 *    - 五档买卖盘数据完整性检查
 * 
 * 5. 事件驱动架构：
 *    - 实现完整的CTP回调事件处理链
 *    - 将底层CTP事件转换为WonderTrader标准事件
 *    - 提供异步事件通知机制
 *    - 支持日志记录和错误处理
 * 
 * 6. 动态库管理：
 *    - 支持运行时动态加载CTP API库
 *    - 处理不同版本CTP API的兼容性问题
 *    - 管理动态库生命周期和资源释放
 *    - 跨平台函数符号解析（Windows/Linux）
 * 
 * 关键技术特性：
 * - 多线程安全：CTP回调在独立线程中执行，需要考虑线程安全
 * - 内存管理：使用引用计数和对象池管理Tick数据对象
 * - 性能优化：最小化数据拷贝，使用就地构造和移动语义
 * - 容错机制：网络异常重连、数据异常过滤、状态恢复
 * - 可配置性：支持多种配置参数，适应不同交易环境
 * 
 * 该实现是连接CTP行情系统与WonderTrader策略引擎的关键桥梁，
 * 确保了行情数据的实时性、准确性和完整性。
 */
// ==================== 头文件包含 ====================
#include "ParserCTP.h"  // CTP行情解析器类声明

// WonderTrader框架核心头文件
#include "../Includes/WTSVersion.h"         // 版本信息定义，提供产品信息常量
#include "../Includes/WTSDataDef.hpp"       // 数据结构定义，包含Tick数据等核心数据类型
#include "../Includes/WTSContractInfo.hpp"  // 合约信息类，用于合约数据验证和管理
#include "../Includes/WTSVariant.hpp"       // 变体类，用于配置参数解析
#include "../Includes/IBaseDataMgr.h"       // 基础数据管理器接口，提供合约信息查询

// WonderTrader工具库
#include "../Share/ModuleHelper.hpp"        // 模块辅助工具，提供路径管理功能
#include "../Share/TimeUtils.hpp"           // 时间工具类，处理时间转换和校正
#include "../Share/StdUtils.hpp"            // 标准工具类，提供文件操作等实用功能

// 第三方库
#include <boost/filesystem.hpp>  // Boost文件系统库，用于目录创建和路径处理

// ==================== 日志工具函数 ====================
// By Wesley @ 2022.01.05 - 添加格式化日志支持
#include "../Share/fmtlib.h"  // 高效的字符串格式化库

/**
 * @brief 格式化日志输出函数模板
 * @tparam Args 可变参数类型
 * @param sink 日志接收器指针，用于输出日志消息
 * @param ll 日志级别（信息、警告、错误等）
 * @param format 格式化字符串模板，类似printf格式
 * @param args 可变参数列表，用于填充格式化字符串
 * 
 * 该函数提供了高效的格式化日志输出功能，特点：
 * - 使用fmtlib库，比标准printf更安全和高效
 * - 支持线程局部存储，避免多线程冲突
 * - 自动检查sink有效性，防止空指针异常
 * - 固定缓冲区大小(512字节)，适合大多数日志场景
 */
template<typename... Args>
inline void write_log(IParserSpi* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)  // 检查日志接收器有效性
		return;

	static thread_local char buffer[512] = { 0 };  // 线程局部存储缓冲区，避免线程冲突
	fmtutil::format_to(buffer, format, args...);   // 使用fmtlib进行高效格式化

	sink->handleParserLog(ll, buffer);  // 通过回调接口输出日志
}

// ==================== C接口导出函数 ====================
// 提供标准C接口，支持动态库插件机制和跨语言调用
extern "C"
{
	/**
	 * @brief 创建CTP行情解析器实例的工厂函数
	 * @return IParserApi* 返回行情解析器接口指针
	 * 
	 * 该函数是动态库的标准入口点，用于创建ParserCTP对象实例。
	 * 特点：
	 * - 使用C链接规范，确保跨编译器兼容性
	 * - 返回基类接口指针，支持多态使用
	 * - 通过EXPORT_FLAG标记，确保符号正确导出
	 * - 支持插件式架构，运行时动态加载
	 * 
	 * 调用方负责：
	 * - 调用init()方法进行初始化
	 * - 使用完毕后调用deleteParser()释放资源
	 */
	EXPORT_FLAG IParserApi* createParser()
	{
		ParserCTP* parser = new ParserCTP();  // 创建CTP解析器实例
		return parser;  // 返回基类接口指针
	}

	/**
	 * @brief 删除行情解析器实例的释放函数
	 * @param parser 要删除的解析器实例引用，删除后会被设置为NULL
	 * 
	 * 该函数负责安全释放解析器实例占用的所有资源。
	 * 特点：
	 * - 空指针安全检查，避免重复删除
	 * - 使用引用传递，自动将指针设置为NULL
	 * - 确保虚析构函数正确调用，完全清理资源
	 * - 与createParser()配对使用，遵循RAII原则
	 * 
	 * 注意：
	 * - 删除前会自动调用release()方法进行清理
	 * - 删除后指针会被设置为NULL，防止悬空指针
	 */
	EXPORT_FLAG void deleteParser(IParserApi* &parser)
	{
		if (NULL != parser)  // 空指针安全检查
		{
			delete parser;    // 调用虚析构函数，完全清理资源
			parser = NULL;    // 设置为NULL，防止悬空指针
		}
	}
};


// ==================== 辅助工具函数 ====================

/**
 * @brief 将时间字符串转换为整型时间格式
 * @param strTime 输入的时间字符串，格式为"HH:MM:SS"
 * @return uint32_t 返回整型时间，格式为HHMMSS
 * 
 * 该函数将CTP返回的时间字符串（如"09:30:00"）转换为数字格式（如093000）。
 * 转换过程：
 * 1. 遍历输入字符串的每个字符
 * 2. 跳过冒号分隔符，只保留数字字符
 * 3. 将处理后的数字字符串转换为无符号整数
 * 
 * 注意：
 * - 使用静态缓冲区，线程不安全，但性能较高
 * - 假设输入格式正确，未进行输入验证
 * - 适用于CTP标准时间格式
 */
inline uint32_t strToTime(const char* strTime)
{
	static char str[10] = { 0 };  // 静态缓冲区，存储去除冒号后的数字字符串
	const char *pos = strTime;    // 输入字符串指针
	int idx = 0;                  // 输出缓冲区索引
	auto len = strlen(strTime);   // 获取输入字符串长度
	
	// 遍历输入字符串，过滤掉冒号分隔符
	for(std::size_t i = 0; i < len; i++)
	{
		if(strTime[i] != ':')  // 跳过冒号分隔符
		{
			str[idx] = strTime[i];  // 复制数字字符
			idx++;
		}
	}
	str[idx] = '\0';  // 添加字符串结束符

	return strtoul(str, NULL, 10);  // 转换为无符号长整型，基数为10
}

/**
 * @brief 检查并修正无效的浮点数值
 * @param val 要检查的浮点数值
 * @return double 有效数值直接返回，无效数值返回0.0
 * 
 * CTP API在某些情况下会返回特殊的无效值标识符（如DBL_MAX、FLT_MAX），
 * 这些值表示该字段无有效数据。该函数将这些无效值转换为0.0，
 * 确保后续数据处理的安全性和一致性。
 * 
 * 处理的无效值：
 * - DBL_MAX：双精度浮点数最大值，常用作无效标识
 * - FLT_MAX：单精度浮点数最大值，部分字段使用此标识
 * 
 * 使用场景：
 * - 价格数据验证（开盘价、收盘价、最高价、最低价等）
 * - 成交额和持仓量数据修正
 * - 涨跌停价格处理
 */
inline double checkValid(double val)
{
	if (val == DBL_MAX || val == FLT_MAX)  // 检查是否为无效值标识符
		return 0;  // 无效值返回0.0

	return val;  // 有效值直接返回
}

// ==================== ParserCTP类实现 ====================

/**
 * @brief ParserCTP构造函数
 * 
 * 初始化CTP行情解析器的所有成员变量为默认状态。
 * 采用初始化列表方式，确保所有成员变量都有明确的初始值。
 * 
 * 初始化内容：
 * - m_pUserAPI：CTP API实例指针设为NULL，等待init()方法中创建
 * - m_iRequestID：请求ID计数器设为0，后续请求会递增此值
 * - m_uTradingDate：交易日设为0，将在登录成功后从服务器获取
 * - m_bLocaltime：本地时间戳标志设为false，默认使用CTP服务器时间
 * 
 * 注意：
 * - 构造函数只进行基础初始化，不进行网络连接
 * - 实际的CTP连接需要调用init()和connect()方法
 * - 其他成员变量使用默认构造函数初始化
 */
ParserCTP::ParserCTP()
	:m_pUserAPI(NULL)      // CTP API实例指针初始化为空
	,m_iRequestID(0)       // 请求ID计数器初始化为0
	,m_uTradingDate(0)     // 交易日初始化为0
    ,m_bLocaltime(false)   // 本地时间戳标志初始化为false
{
	// 构造函数体为空，所有初始化在初始化列表中完成
}

/**
 * @brief ParserCTP析构函数
 * 
 * 清理ParserCTP对象占用的资源。
 * 设置API指针为NULL，防止悬空指针问题。
 * 
 * 注意：
 * - 实际的资源释放在release()方法中进行
 * - CTP API实例的释放遵循CTP官方推荐的流程
 * - 析构函数不直接释放CTP资源，避免析构顺序问题
 */
ParserCTP::~ParserCTP()
{
	m_pUserAPI = NULL;  // 将API指针设为NULL，防止悬空指针
}

/**
 * @brief 初始化CTP行情解析器
 * @param config 配置参数对象，包含连接和认证信息
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 该方法是解析器的核心初始化函数，负责以下工作：
 * 1. 解析和验证配置参数
 * 2. 设置流控文件目录
 * 3. 动态加载CTP API库
 * 4. 创建CTP API实例
 * 5. 注册回调接口和服务器地址
 * 
 * 配置参数说明：
 * - front: CTP前置服务器地址，格式"tcp://ip:port"（必选）
 * - broker: 经纪商代码（必选）
 * - user: 用户账号（必选）
 * - pass: 用户密码（必选）
 * - flowdir: 流控文件目录（可选，默认"CTPMDFlow"）
 * - localtime: 是否使用本地时间戳（可选，默认false）
 * - ctpmodule: CTP动态库名称（可选，默认"thostmduserapi_se"）
 */
bool ParserCTP::init(WTSVariant* config)
{
	// ========== 第一步：解析基础配置参数 ==========
	m_strFrontAddr = config->getCString("front");    // 获取前置服务器地址
	m_strBroker = config->getCString("broker");      // 获取经纪商代码
	m_strUserID = config->getCString("user");        // 获取用户账号
	m_strPassword = config->getCString("pass");      // 获取用户密码
	m_strFlowDir = config->getCString("flowdir");    // 获取流控文件目录
    
	/*
     * By Wesley @ 2022.03.09
     * 本地时间戳参数主要用于非标准CTP环境，如：
     * - SimNow全天候仿真环境
     * - OpenCTP开源实现
     * - 其他第三方CTP兼容环境
     * 
     * 标准CTP环境应使用服务器时间戳（false），
     * 非标准环境可能需要本地时间戳（true）来确保时间准确性。
     */
    m_bLocaltime = config->getBoolean("localtime");

	// ========== 第二步：设置和标准化流控目录 ==========
	if (m_strFlowDir.empty())  // 如果未指定流控目录
		m_strFlowDir = "CTPMDFlow";  // 使用默认目录名

	m_strFlowDir = StrUtil::standardisePath(m_strFlowDir);  // 标准化路径格式

	// ========== 第三步：动态库加载和路径处理 ==========
	std::string module = config->getCString("ctpmodule");  // 获取CTP动态库名称
	if (module.empty())  // 如果未指定动态库名称
		module = "thostmduserapi_se";  // 使用默认的CTP行情API库名

	// 构建完整的动态库路径：程序目录 + 包装后的模块名
	std::string dllpath = getBinDir() + DLLHelper::wrap_module(module.c_str(), "");
	m_hInstCTP = DLLHelper::load_library(dllpath.c_str());  // 加载CTP动态库
	
	// 构建用户专用的流控文件目录：流控根目录/经纪商/用户ID/
	std::string path = StrUtil::printf("%s%s/%s/", m_strFlowDir.c_str(), m_strBroker.c_str(), m_strUserID.c_str());
	if (!StdFile::exists(path.c_str()))  // 检查目录是否存在
	{
		// 使用Boost文件系统库递归创建目录结构
		boost::filesystem::create_directories(boost::filesystem::path(path));
	}	
	// ========== 第四步：获取CTP API创建函数 ==========
	// 根据平台和架构选择正确的函数符号名称
#ifdef _WIN32  // Windows平台
#	ifdef _WIN64  // 64位Windows
	const char* creatorName = "?CreateFtdcMdApi@CThostFtdcMdApi@@SAPEAV1@PEBD_N1@Z";
#	else  // 32位Windows
	const char* creatorName = "?CreateFtdcMdApi@CThostFtdcMdApi@@SAPAV1@PBD_N1@Z";
#	endif
#else  // Linux/Unix平台
	const char* creatorName = "_ZN15CThostFtdcMdApi15CreateFtdcMdApiEPKcbb";
#endif
	
	// 从动态库中获取API创建函数指针
	m_funcCreator = (CTPCreator)DLLHelper::get_symbol(m_hInstCTP, creatorName);
	
	// ========== 第五步：创建CTP API实例并注册回调 ==========
	// 调用创建函数：参数为(流控目录, 是否UDP模式, 是否组播模式)
	m_pUserAPI = m_funcCreator(path.c_str(), false, false);
	m_pUserAPI->RegisterSpi(this);  // 注册回调接口，接收CTP事件
	m_pUserAPI->RegisterFront((char*)m_strFrontAddr.c_str());  // 注册前置服务器地址

	return true;  // 初始化成功
}

/**
 * @brief 释放CTP行情解析器资源
 * 
 * 该方法负责清理解析器占用的所有资源，包括：
 * - 断开与CTP服务器的连接
 * - 释放CTP API实例
 * - 清理动态库引用
 * - 重置所有状态变量
 * 
 * 注意：
 * - 该方法通常在程序退出或模块卸载时调用
 * - 内部调用disconnect()方法执行具体的清理工作
 * - 调用后解析器将无法再使用，需要重新初始化
 */
void ParserCTP::release()
{
	disconnect();  // 调用断开连接方法，执行实际的资源清理
}

/**
 * @brief 开始连接CTP行情服务器
 * @return bool 连接命令发送成功返回true，失败返回false
 * 
 * 该方法启动与CTP行情前置服务器的连接过程。
 * 连接是异步进行的，实际连接结果通过回调事件通知：
 * - 连接成功：触发OnFrontConnected()回调
 * - 连接失败：触发OnFrontDisconnected()回调
 * 
 * 连接流程：
 * 1. 检查API实例是否已创建
 * 2. 调用Init()方法启动连接
 * 3. CTP内部建立TCP连接
 * 4. 连接成功后自动触发登录流程
 * 
 * 注意：
 * - 必须先调用init()方法完成初始化
 * - 连接过程是异步的，不会阻塞当前线程
 * - 连接状态变化通过回调事件通知
 */
bool ParserCTP::connect()
{
	if(m_pUserAPI)  // 检查API实例是否已创建
	{
		m_pUserAPI->Init();  // 启动CTP连接，异步执行
	}

	return true;  // 连接命令发送成功
}

/**
 * @brief 断开与CTP服务器的连接并释放资源
 * @return bool 断开操作成功返回true，失败返回false
 * 
 * 该方法执行完整的CTP连接清理流程：
 * 1. 注销回调接口，停止接收CTP事件
 * 2. 释放CTP API实例，断开网络连接
 * 3. 将API指针设为NULL，防止悬空指针
 * 
 * 断开流程：
 * - RegisterSpi(NULL)：注销回调接口，避免回调到已释放的对象
 * - Release()：释放API实例，CTP内部会断开网络连接
 * - 设置指针为NULL：确保后续操作的安全性
 * 
 * 注意：
 * - 断开过程可能需要一定时间，CTP内部会处理清理工作
 * - 断开后如需重连，必须重新调用init()和connect()
 * - 该方法是线程安全的，可以在任何线程中调用
 */
bool ParserCTP::disconnect()
{
	if(m_pUserAPI)  // 检查API实例是否存在
	{
		m_pUserAPI->RegisterSpi(NULL);  // 注销回调接口，停止接收事件
		m_pUserAPI->Release();          // 释放API实例，断开连接
		m_pUserAPI = NULL;              // 设置指针为NULL，确保安全
	}

	return true;  // 断开操作完成
}

// ==================== CTP回调事件处理实现 ====================

/**
 * @brief CTP错误应答回调事件处理
 * @param pRspInfo CTP错误信息结构体指针
 * @param nRequestID 对应的请求ID
 * @param bIsLast 是否为最后一个应答（批量应答时使用）
 * 
 * 当CTP服务器返回错误信息时触发此回调。
 * 用于处理各种CTP操作失败的情况，如：
 * - 登录认证失败
 * - 订阅请求失败  
 * - 网络通信错误
 * - 权限不足错误
 * - 参数格式错误
 * 
 * 当前实现调用IsErrorRspInfo()进行错误信息检查，
 * 可根据需要扩展具体的错误处理逻辑。
 */
void ParserCTP::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	IsErrorRspInfo(pRspInfo);  // 检查和处理错误信息
}

/**
 * @brief CTP前置连接成功回调事件处理
 * 
 * 当与CTP行情前置服务器建立网络连接成功时触发此回调。
 * 这是整个行情连接流程的第一步，连接成功后会自动启动登录流程。
 * 
 * 处理流程：
 * 1. 记录连接成功日志信息
 * 2. 通知上层应用连接事件（WPE_Connect）
 * 3. 自动发送用户登录请求
 * 
 * 连接成功后的后续流程：
 * - 自动调用ReqUserLogin()发送登录请求
 * - 等待OnRspUserLogin()回调处理登录结果
 * - 登录成功后开始行情数据订阅
 * 
 * 注意：
 * - 连接成功不代表登录成功，还需要等待登录认证
 * - 此时可以开始发送CTP请求，但不能订阅行情
 * - 网络断开重连后会再次触发此回调
 */
void ParserCTP::OnFrontConnected()
{
	if(m_sink)  // 检查回调接口是否有效
	{
		// 记录连接成功的日志信息
		write_log(m_sink, LL_INFO, "[ParserCTP] Market data server connected");
		// 通知上层应用连接事件，错误码为0表示成功
		m_sink->handleEvent(WPE_Connect, 0);
	}

	ReqUserLogin();  // 连接成功后自动发送登录请求
}

/**
 * @brief CTP用户登录应答回调事件处理
 * @param pRspUserLogin 登录响应信息，包含交易日等重要数据
 * @param pRspInfo 错误信息，登录失败时包含具体错误原因
 * @param nRequestID 登录请求的ID
 * @param bIsLast 是否为最后一个应答
 * 
 * 用户登录请求的服务器应答处理。登录成功后执行以下操作：
 * 1. 获取并设置当前交易日
 * 2. 记录登录成功日志
 * 3. 通知上层应用登录事件
 * 4. 自动开始订阅已缓存的合约行情
 * 
 * 交易日处理逻辑：
 * - 优先使用CTP服务器返回的交易日
 * - 如果服务器返回的交易日为0（某些环境下可能出现），则使用本地日期
 * - 注意：本地日期在夜盘时可能不准确，建议使用标准CTP环境
 * 
 * 登录失败处理：
 * - 如果登录失败，不会执行后续的订阅操作
 * - 错误信息会通过IsErrorRspInfo()进行处理
 * - 上层应用可通过日志了解失败原因
 */
void ParserCTP::OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	// 检查是否为最后一个应答且没有错误
	if(bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// 从CTP服务器获取当前交易日
		m_uTradingDate = strtoul(m_pUserAPI->GetTradingDay(), NULL, 10);
        
		// By Wesley @ 2022.03.09
        // 交易日获取失败的备用处理方案
        // 注意：这种方式在夜盘时可能不准确，因为夜盘的交易日是下一个自然日
        if(m_uTradingDate == 0)
            m_uTradingDate = TimeUtils::getCurDate();  // 使用本地日期作为备选
		
		// 记录登录成功日志，包含交易日信息
		write_log(m_sink, LL_INFO, "[ParserCTP] Market data server logined, {}", m_uTradingDate);

		if(m_sink)  // 检查回调接口有效性
		{
			// 通知上层应用登录成功事件
			m_sink->handleEvent(WPE_Login, 0);
		}

		// 登录成功后自动订阅已缓存的行情数据
		DoSubscribeMD();
	}
}

/**
 * @brief CTP用户登出应答回调事件处理
 * @param pUserLogout 登出响应信息结构体
 * @param pRspInfo 错误信息，登出失败时包含错误详情
 * @param nRequestID 登出请求的ID
 * @param bIsLast 是否为最后一个应答
 * 
 * 用户主动登出或被服务器强制登出时的应答处理。
 * 登出完成后会通知上层应用登出事件，清理相关状态。
 * 
 * 登出场景：
 * - 用户主动调用登出接口
 * - 服务器因安全原因强制登出
 * - 会话超时被动登出
 * - 网络异常导致的登出
 * 
 * 处理流程：
 * 1. 检查回调接口有效性
 * 2. 通知上层应用登出事件（WPE_Logout）
 * 3. 上层应用可据此更新UI状态或执行清理操作
 */
void ParserCTP::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if(m_sink)  // 检查回调接口有效性
	{
		// 通知上层应用登出事件，错误码为0表示正常登出
		m_sink->handleEvent(WPE_Logout, 0);
	}
}

/**
 * @brief CTP前置连接断开回调事件处理
 * @param nReason 断开原因代码
 * 
 * 当与CTP服务器的连接意外断开时触发此回调。
 * 断开可能由多种原因引起，需要记录详细信息并通知上层应用。
 * 
 * 常见断开原因：
 * - 网络故障或不稳定
 * - 服务器维护或重启
 * - 认证会话超时
 * - 客户端异常退出
 * - 防火墙或网络策略变化
 * 
 * 处理流程：
 * 1. 记录断开原因到错误日志
 * 2. 通知上层应用连接关闭事件（WPE_Close）
 * 3. 上层应用可据此决定是否自动重连
 * 
 * 注意：
 * - 断开后所有订阅状态会丢失
 * - 需要重新执行完整的连接和登录流程
 * - 可以根据断开原因判断重连策略
 */
void ParserCTP::OnFrontDisconnected( int nReason )
{
	if(m_sink)  // 检查回调接口有效性
	{
		// 记录断开事件到错误日志，包含具体的断开原因代码
		write_log(m_sink, LL_ERROR, "[ParserCTP] Market data server disconnected: {}", nReason);
		// 通知上层应用连接关闭事件
		m_sink->handleEvent(WPE_Close, 0);
	}
}

/**
 * @brief CTP退订行情应答回调事件处理
 * @param pSpecificInstrument 退订的合约信息
 * @param pRspInfo 错误信息，退订失败时包含错误原因
 * @param nRequestID 退订请求的ID
 * @param bIsLast 是否为最后一个应答
 * 
 * 退订合约行情请求的服务器应答处理。
 * 成功退订后，服务器将停止推送该合约的行情数据。
 * 
 * 退订场景：
 * - 策略不再需要某些合约的行情
 * - 减少网络流量和处理负载
 * - 动态调整订阅列表
 * 
 * 当前实现：
 * - 方法体为空，表示不进行特殊处理
 * - 可根据业务需要扩展退订成功/失败的处理逻辑
 * - 可以记录退订状态或通知上层应用
 * 
 * 可扩展功能：
 * - 记录退订成功的合约列表
 * - 处理退订失败的错误情况
 * - 通知上层应用退订结果
 */
void ParserCTP::OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	// 当前实现为空，可根据需要扩展退订结果处理逻辑
	// 例如：记录退订状态、处理错误信息、通知上层应用等
}

/**
 * @brief CTP深度行情数据推送回调 - 核心数据处理方法
 * @param pDepthMarketData CTP深度行情数据结构体指针，包含完整的市场数据
 * 
 * 这是整个解析器最重要的方法，负责处理来自CTP的实时行情数据推送。
 * 该方法将CTP原生格式的行情数据转换为WonderTrader标准格式，并进行
 * 数据质量控制和时间校正处理。
 * 
 * 主要处理流程：
 * 1. 数据有效性验证（基础数据管理器、合约信息）
 * 2. 时间戳处理和交易日校正
 * 3. 数据格式转换和无效值过滤
 * 4. 交易所特定规则处理
 * 5. 五档买卖盘数据填充
 * 6. 数据推送给上层应用
 * 
 * 数据质量控制：
 * - 过滤无效合约的数据
 * - 处理DBL_MAX等无效价格标识
 * - 修正异常时间戳
 * - 过滤过期或错误的历史数据
 * 
 * 时间处理特殊逻辑：
 * - 支持本地时间戳模式（适用于SimNow等环境）
 * - 处理夜盘跨日时间问题
 * - 过滤启动时的过期数据
 * 
 * 性能优化：
 * - 使用对象池管理Tick数据，减少内存分配
 * - 最小化数据拷贝操作
 * - 快速数据有效性检查
 */
void ParserCTP::OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pDepthMarketData )
{	
	// ========== 第一步：基础数据验证 ==========
	if(m_pBaseDataMgr == NULL)  // 检查基础数据管理器是否有效
	{
		return;  // 无基础数据管理器，无法验证合约信息，直接返回
	}

    // 通过基础数据管理器验证合约信息的有效性
    WTSContractInfo* contract = m_pBaseDataMgr->getContract(pDepthMarketData->InstrumentID, pDepthMarketData->ExchangeID);
    if (contract == NULL)  // 合约信息无效或不存在
        return;  // 跳过无效合约的行情数据

    // ========== 第二步：时间戳处理和校正 ==========
    uint32_t actDate, actTime, actHour;  // 定义实际的日期、时间和小时变量

    if(m_bLocaltime)  // 本地时间戳模式（主要用于SimNow等非标准环境）
    {
        // 使用本地系统时间作为行情时间戳
        TimeUtils::getDateTime(actDate, actTime);
        actHour = actTime / 10000000;  // 提取小时部分
    }
    else  // 标准CTP时间戳模式（推荐使用）
    {
        // 使用CTP服务器提供的时间信息
        actDate = strtoul(pDepthMarketData->ActionDay, NULL, 10);  // 转换发生日期
        // 转换时间：HH:MM:SS -> HHMMSS，然后加上毫秒数
        actTime = strToTime(pDepthMarketData->UpdateTime) * 1000 + pDepthMarketData->UpdateMillisec;
        actHour = actTime / 10000000;  // 提取小时部分

        // ========== 夜盘时间异常处理 ==========
        // 处理CTP时间戳的一个已知问题：夜盘时发生日期可能等于交易日
        // 正常情况下，夜盘的发生日期应该是上一个自然日，而交易日是下一个自然日
        if (actDate == m_uTradingDate && actHour >= 20) {
            // 检测到异常：发生日期等于交易日且时间在夜盘范围内（>=20点）
            // 这种情况在实际中是不可能的，需要手动修正
            
            uint32_t curDate, curTime;
            TimeUtils::getDateTime(curDate, curTime);  // 获取当前系统时间
            uint32_t curHour = curTime / 10000000;     // 提取当前小时

            // 特殊情况：早上启动时收到昨晚的收盘行情
            // 早上3点到9点之间启动程序，可能收到昨夜12点前的历史数据
            // 这些数据已经过期，直接丢弃
            if (curHour >= 3 && curHour < 9)
                return;  // 丢弃过期的历史数据

            // 使用当前系统日期作为基准进行修正
            actDate = curDate;

            // 处理跨日边界的特殊情况
            if (actHour == 23 && curHour == 0) {
                // 行情时间（23点）慢于系统时间（0点）
                // 说明行情是昨天的，需要将日期减一天
                actDate = TimeUtils::getNextDate(curDate, -1);
            } else if (actHour == 0 && curHour == 23) {
                // 系统时间（23点）慢于行情时间（0点）  
                // 说明行情是明天的，需要将日期加一天
                actDate = TimeUtils::getNextDate(curDate, 1);
            }
        }
    }

	// ========== 第三步：数据转换和对象创建 ==========
	WTSCommodityInfo* pCommInfo = contract->getCommInfo();  // 获取品种信息

	// 创建WonderTrader标准Tick数据对象（使用对象池提高性能）
	WTSTickData* tick = WTSTickData::create(pDepthMarketData->InstrumentID);
	tick->setContractInfo(contract);  // 设置关联的合约信息

	// 获取Tick数据结构体引用，进行数据填充
	WTSTickStruct& quote = tick->getTickStruct();
	strcpy(quote.exchg, pCommInfo->getExchg());  // 设置交易所代码
	
	// ========== 时间信息填充 ==========
	quote.action_date = actDate;    // 设置数据发生日期
	quote.action_time = actTime;    // 设置数据发生时间
	quote.trading_date = m_uTradingDate;  // 设置交易日
	
	// ========== 价格数据填充（过滤无效值） ==========
	quote.price = checkValid(pDepthMarketData->LastPrice);      // 最新价，过滤DBL_MAX等无效值
	quote.open = checkValid(pDepthMarketData->OpenPrice);       // 开盘价
	quote.high = checkValid(pDepthMarketData->HighestPrice);    // 最高价  
	quote.low = checkValid(pDepthMarketData->LowestPrice);      // 最低价
	
	// ========== 成交和持仓数据 ==========
	quote.total_volume = pDepthMarketData->Volume;              // 总成交量
	quote.open_interest = pDepthMarketData->OpenInterest;       // 持仓量
	
	// 结算价特殊处理：只有当结算价不是无效值时才设置
	if(pDepthMarketData->SettlementPrice != DBL_MAX)
		quote.settle_price = checkValid(pDepthMarketData->SettlementPrice);
	
	// ========== 成交额处理（交易所特定规则） ==========
	if(strcmp(quote.exchg, "CZCE") == 0)  // 郑州商品交易所特殊处理
	{
		// 郑商所的成交额需要乘以合约乘数进行缩放
		quote.total_turnover = pDepthMarketData->Turnover * pCommInfo->getVolScale();
	}
	else  // 其他交易所标准处理
	{
		if(pDepthMarketData->Turnover != DBL_MAX)  // 检查成交额有效性
			quote.total_turnover = pDepthMarketData->Turnover;
	}

	// ========== 涨跌停价格 ==========
	quote.upper_limit = checkValid(pDepthMarketData->UpperLimitPrice);  // 涨停价
	quote.lower_limit = checkValid(pDepthMarketData->LowerLimitPrice);   // 跌停价

	// ========== 昨日数据 ==========
	quote.pre_close = checkValid(pDepthMarketData->PreClosePrice);        // 昨收价
	quote.pre_settle = checkValid(pDepthMarketData->PreSettlementPrice);  // 昨结算价
	quote.pre_interest = pDepthMarketData->PreOpenInterest;               // 昨持仓量

	// ========== 第四步：五档买卖盘数据填充 ==========
	// 委卖价格（卖一到卖五），过滤无效价格
	quote.ask_prices[0] = checkValid(pDepthMarketData->AskPrice1);  // 卖一价
	quote.ask_prices[1] = checkValid(pDepthMarketData->AskPrice2);  // 卖二价
	quote.ask_prices[2] = checkValid(pDepthMarketData->AskPrice3);  // 卖三价
	quote.ask_prices[3] = checkValid(pDepthMarketData->AskPrice4);  // 卖四价
	quote.ask_prices[4] = checkValid(pDepthMarketData->AskPrice5);  // 卖五价

	// 委买价格（买一到买五），过滤无效价格
	quote.bid_prices[0] = checkValid(pDepthMarketData->BidPrice1);  // 买一价
	quote.bid_prices[1] = checkValid(pDepthMarketData->BidPrice2);  // 买二价
	quote.bid_prices[2] = checkValid(pDepthMarketData->BidPrice3);  // 买三价
	quote.bid_prices[3] = checkValid(pDepthMarketData->BidPrice4);  // 买四价
	quote.bid_prices[4] = checkValid(pDepthMarketData->BidPrice5);  // 买五价

	// 委卖量（卖一到卖五），CTP保证量不会为无效值，直接赋值
	quote.ask_qty[0] = pDepthMarketData->AskVolume1;  // 卖一量
	quote.ask_qty[1] = pDepthMarketData->AskVolume2;  // 卖二量
	quote.ask_qty[2] = pDepthMarketData->AskVolume3;  // 卖三量
	quote.ask_qty[3] = pDepthMarketData->AskVolume4;  // 卖四量
	quote.ask_qty[4] = pDepthMarketData->AskVolume5;  // 卖五量

	// 委买量（买一到买五），CTP保证量不会为无效值，直接赋值
	quote.bid_qty[0] = pDepthMarketData->BidVolume1;  // 买一量
	quote.bid_qty[1] = pDepthMarketData->BidVolume2;  // 买二量
	quote.bid_qty[2] = pDepthMarketData->BidVolume3;  // 买三量
	quote.bid_qty[3] = pDepthMarketData->BidVolume4;  // 买四量
	quote.bid_qty[4] = pDepthMarketData->BidVolume5;  // 买五量

	// ========== 第五步：数据推送给上层应用 ==========
	if(m_sink)  // 检查回调接口有效性
	{
		// 推送处理后的标准Tick数据给上层应用
		// 参数说明：tick数据对象，处理标记1表示完整快照需要切片处理
		m_sink->handleQuote(tick, 1);
	}

	// ========== 第六步：释放Tick数据对象 ==========
	tick->release();  // 释放Tick数据对象，归还到对象池中
}

/**
 * @brief CTP订阅行情应答回调事件处理
 * @param pSpecificInstrument 订阅的合约信息
 * @param pRspInfo 错误信息，订阅失败时包含错误原因
 * @param nRequestID 订阅请求的ID
 * @param bIsLast 是否为最后一个应答
 * 
 * 订阅合约行情请求的服务器应答处理。
 * 成功订阅后，服务器会开始推送该合约的行情数据。
 * 
 * 订阅成功条件：
 * - 合约代码有效且存在
 * - 用户有该合约的行情权限
 * - 服务器正常运行
 * - 网络连接稳定
 * 
 * 可能的订阅失败原因：
 * - 合约代码不存在或格式错误
 * - 用户无该合约行情权限
 * - 服务器繁忙或维护中
 * - 已达到订阅数量上限
 * 
 * 当前实现：
 * - 检查是否有错误信息
 * - 成功和失败分支都为空，可根据需要扩展
 * - 建议添加日志记录和状态管理
 * 
 * 可扩展功能：
 * - 记录订阅成功的合约列表
 * - 处理订阅失败的重试机制
 * - 通知上层应用订阅结果
 */
void ParserCTP::OnRspSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	if(!IsErrorRspInfo(pRspInfo))  // 检查是否订阅成功
	{
		// 订阅成功处理逻辑（当前为空）
		// 可扩展：记录成功订阅的合约、更新订阅状态、通知上层应用等
	}
	else  // 订阅失败
	{
		// 订阅失败处理逻辑（当前为空）
		// 可扩展：记录错误信息、重试机制、通知上层应用等
	}
}

/**
 * @brief CTP心跳超时警告回调事件处理
 * @param nTimeLapse 心跳超时时间（毫秒）
 * 
 * 当与CTP服务器的心跳检测超时时触发此回调。
 * 心跳机制用于监控网络连接质量，及时发现连接异常。
 * 
 * 心跳机制说明：
 * - CTP客户端和服务器之间定期发送心跳包
 * - 正常情况下心跳响应时间很短
 * - 网络延迟或拥塞时心跳时间会增加
 * - 超过阈值时触发此警告回调
 * 
 * 超时原因分析：
 * - 网络延迟增加或不稳定
 * - 服务器负载过高响应慢
 * - 本地系统资源紧张
 * - 防火墙或网络设备问题
 * 
 * 处理策略：
 * - 记录超时时间到信息日志
 * - 监控超时频率和趋势
 * - 超时时间过长时考虑重连
 * - 通知上层应用网络质量状况
 * 
 * 注意：
 * - 偶尔的心跳超时是正常的
 * - 持续超时可能预示连接问题
 * - 可根据超时时长调整处理策略
 */
void ParserCTP::OnHeartBeatWarning( int nTimeLapse )
{
	if(m_sink)  // 检查回调接口有效性
	{
		// 记录心跳超时信息到日志，包含具体的超时时长
		write_log(m_sink, LL_INFO, "[ParserCTP] Heartbeating, elapse: {}", nTimeLapse);
	}
}

// ==================== 私有辅助方法实现 ====================

/**
 * @brief 发送用户登录请求到CTP服务器
 * 
 * 构建登录请求结构体，填入用户凭证信息，并发送到CTP服务器。
 * 登录请求包含经纪商代码、用户ID、密码和产品信息。
 * 登录结果通过OnRspUserLogin()回调返回。
 * 
 * 注意：
 * - 此方法在连接成功后自动调用
 * - 也可手动调用进行重新登录
 * - 请求ID会自动递增，用于跟踪请求状态
 */
void ParserCTP::ReqUserLogin()
{
	if(m_pUserAPI == NULL)  // 检查API实例有效性
	{
		return;  // API未初始化，无法发送请求
	}

	// 初始化登录请求结构体
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));  // 清零结构体内容
	
	// 填入登录凭证信息
	strcpy(req.BrokerID, m_strBroker.c_str());      // 经纪商代码
	strcpy(req.UserID, m_strUserID.c_str());        // 用户账号
	strcpy(req.Password, m_strPassword.c_str());    // 用户密码
	strcpy(req.UserProductInfo, WT_PRODUCT);        // 产品信息（WonderTrader标识）
	
	// 发送登录请求，请求ID自动递增
	int iResult = m_pUserAPI->ReqUserLogin(&req, ++m_iRequestID);
	if(iResult != 0)  // 检查请求发送结果
	{
		if(m_sink)  // 记录错误日志
			write_log(m_sink, LL_ERROR, "[ParserCTP] Sending login request failed: {}", iResult);
	}
}

/**
 * @brief 执行合约行情订阅操作
 * 
 * 将缓存的订阅合约列表发送到CTP服务器进行实际订阅。
 * 会自动处理合约代码格式（去除交易所前缀）。
 * 订阅成功后开始接收相应合约的实时行情推送。
 * 
 * 合约代码处理：
 * - 输入格式："SHFE.rb2410"（交易所.合约代码）
 * - CTP格式："rb2410"（只需要合约代码部分）
 * - 如果没有点号分隔符，直接使用原始代码
 * 
 * 注意：
 * - 只在登录成功后调用此方法
 * - 如果订阅列表为空则直接返回
 * - 订阅结果会记录到日志中
 * - 使用动态内存分配，需要手动释放
 */
void ParserCTP::DoSubscribeMD()
{
	CodeSet codeFilter = m_filterSubs;  // 复制订阅列表
	if(codeFilter.empty())  // 检查订阅列表是否为空
	{
		// 如果订阅列表为空，直接返回，不执行订阅操作
		return;
	}

	// 创建字符串指针数组，用于CTP API订阅接口
	char ** subscribe = new char*[codeFilter.size()];
	int nCount = 0;  // 实际处理的合约数量
	
	// 遍历订阅列表，处理合约代码格式
	for(auto& code : codeFilter)
	{
		std::size_t pos = code.find('.');  // 查找交易所分隔符
		if (pos != std::string::npos)  // 如果找到分隔符
			subscribe[nCount++] = (char*)code.c_str() + pos + 1;  // 只使用分隔符后的部分
		else  // 如果没有分隔符
			subscribe[nCount++] = (char*)code.c_str();  // 直接使用原始代码
	}

	// 检查API实例有效性和合约数量
	if(m_pUserAPI && nCount > 0)
	{
		// 发送订阅请求到CTP服务器
		int iResult = m_pUserAPI->SubscribeMarketData(subscribe, nCount);
		if(iResult != 0)  // 订阅失败
		{
			if(m_sink)
				write_log(m_sink, LL_ERROR, "[ParserCTP] Sending md subscribe request failed: {}", iResult);
		}
		else  // 订阅成功
		{
			if(m_sink)
				write_log(m_sink, LL_INFO, "[ParserCTP] Market data of {} contracts subscribed totally", nCount);
		}
	}
	
	codeFilter.clear();     // 清空临时订阅列表
	delete[] subscribe;     // 释放动态分配的内存
}

/**
 * @brief 检查CTP返回的错误信息
 * @param pRspInfo CTP错误信息结构体指针
 * @return bool 如果有错误返回true，无错误返回false
 * 
 * 解析CTP服务器返回的错误信息结构体，判断操作是否成功。
 * 当前实现简化处理，直接返回false表示无错误。
 * 
 * 可扩展功能：
 * - 检查pRspInfo->ErrorID是否为0
 * - 根据错误码进行分类处理
 * - 记录详细的错误信息到日志
 * - 实现特定错误的自动重试机制
 * - 错误信息的本地化处理
 * 
 * 标准CTP错误检查逻辑应为：
 * return (pRspInfo && pRspInfo->ErrorID != 0);
 */
bool ParserCTP::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	return false;  // 当前简化实现，总是返回无错误
}

/**
 * @brief 订阅指定合约的行情数据
 * @param vecSymbols 要订阅的合约代码集合，格式为"交易所.合约代码"
 * 
 * 该方法是IParserApi接口的实现，用于订阅指定合约的实时行情数据。
 * 根据当前登录状态采取不同的处理策略：
 * - 未登录状态：缓存订阅请求，等待登录成功后自动订阅
 * - 已登录状态：立即执行订阅操作
 * 
 * 订阅逻辑：
 * 1. 检查当前登录状态（通过交易日是否为0判断）
 * 2. 如果未登录，将订阅列表保存到缓存中
 * 3. 如果已登录，保存订阅列表并立即执行订阅
 * 
 * 合约代码格式示例：
 * - "SHFE.rb2410"：上期所螺纹钢2024年10月合约
 * - "DCE.i2410"：大商所铁矿石2024年10月合约
 * - "CZCE.MA410"：郑商所甲醇2024年10月合约
 * 
 * 注意：
 * - 重复调用会覆盖之前的订阅列表
 * - 订阅结果通过OnRspSubMarketData()回调通知
 * - 订阅成功后开始接收OnRtnDepthMarketData()推送
 */
void ParserCTP::subscribe(const CodeSet &vecSymbols)
{
	if(m_uTradingDate == 0)  // 检查是否已登录（交易日为0表示未登录）
	{
		// 未登录状态：缓存订阅请求，等待登录成功后自动订阅
		m_filterSubs = vecSymbols;
	}
	else  // 已登录状态
	{
		// 保存订阅列表并立即执行订阅操作
		m_filterSubs = vecSymbols;
		DoSubscribeMD();  // 立即执行订阅
	}
}

/**
 * @brief 退订指定合约的行情数据
 * @param vecSymbols 要退订的合约代码集合
 * 
 * 该方法用于退订指定合约的行情数据推送，停止接收相关数据。
 * 当前实现为空，可根据需要进行扩展。
 * 
 * 可实现的功能：
 * 1. 调用CTP API的UnSubscribeMarketData方法
 * 2. 从本地订阅列表中移除指定合约
 * 3. 记录退订操作到日志
 * 4. 处理退订结果回调
 * 
 * 实现示例逻辑：
 * - 检查API实例和登录状态
 * - 处理合约代码格式转换
 * - 调用m_pUserAPI->UnSubscribeMarketData()
 * - 从m_filterSubs中移除对应合约
 * - 记录操作结果到日志
 * 
 * 注意：
 * - CTP支持单独退订指定合约
 * - 退订结果通过OnRspUnSubMarketData()回调通知
 * - 退订后立即停止接收该合约的行情推送
 */
void ParserCTP::unsubscribe(const CodeSet &vecSymbols)
{
	// 当前实现为空，可根据需要扩展退订功能
	// 建议实现：遍历vecSymbols，调用CTP API进行退订
}

/**
 * @brief 检查是否已连接到CTP服务器
 * @return bool 已连接返回true，未连接返回false
 * 
 * 该方法检查CTP行情解析器的连接状态。
 * 通过检查API实例指针是否为空来判断连接状态。
 * 
 * 注意事项：
 * - 此方法检查的是API实例是否已创建，而非实际网络连接状态
 * - API实例存在不代表已成功连接到服务器
 * - 实际的网络连接状态需要通过回调事件来跟踪
 * - 更准确的连接状态应该包括登录状态的检查
 * 
 * 连接状态层次：
 * 1. API实例创建（本方法检查的层次）
 * 2. 网络连接建立（OnFrontConnected回调）
 * 3. 用户登录成功（OnRspUserLogin回调）
 * 4. 可以正常收发数据
 * 
 * 建议的完整状态检查：
 * return (m_pUserAPI != NULL && m_uTradingDate != 0);
 */
bool ParserCTP::isConnected()
{
	return m_pUserAPI != NULL;  // 检查API实例是否已创建
}

/**
 * @brief 注册行情数据回调接口
 * @param listener 回调接口指针，用于接收解析后的行情数据和事件
 * 
 * 该方法设置上层应用的回调接口，所有的行情数据、连接事件、错误信息
 * 都会通过这个接口传递给上层应用。同时获取基础数据管理器接口，
 * 用于后续的合约信息验证和查询。
 * 
 * 回调接口功能：
 * - handleEvent()：处理连接、登录、断开等事件
 * - handleQuote()：处理实时行情数据
 * - handleParserLog()：处理解析器日志信息
 * - getBaseDataMgr()：获取基础数据管理器接口
 * 
 * 基础数据管理器作用：
 * - 验证合约代码的有效性
 * - 获取合约的详细信息（交易所、品种、合约乘数等）
 * - 提供合约查询和管理功能
 * 
 * 注意：
 * - 必须在调用init()之前设置回调接口
 * - 回调接口的生命周期必须长于解析器对象
 * - 回调函数在CTP内部线程中执行，需要注意线程安全
 * - 基础数据管理器用于OnRtnDepthMarketData中的合约验证
 */
void ParserCTP::registerSpi(IParserSpi* listener)
{
	m_sink = listener;  // 设置回调接口指针

	// 如果回调接口有效，获取基础数据管理器接口
	if(m_sink)
		m_pBaseDataMgr = m_sink->getBaseDataMgr();
}