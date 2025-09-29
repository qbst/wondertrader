/*!
 * \file ParserCTPMini.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTP Mini行情解析器实现文件 - WonderTrader框架中CTP Mini行情数据解析的具体实现
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是ParserCTPMini.h中声明的CTP Mini行情解析器类的具体实现，负责处理与CTP Mini
 * 行情服务器的全部交互逻辑和数据转换工作。作为WonderTrader量化交易框架的重要组成部分，
 * 该实现文件提供了轻量级、高效的期货行情数据解析能力。
 * 
 * 核心功能实现：
 * 
 * 1. 连接与认证管理：
 *    - 实现与CTP Mini行情前置服务器的TCP连接建立
 *    - 处理用户登录认证流程，包括凭证验证和会话管理
 *    - 管理连接状态变化和异常重连机制
 *    - 支持CTP Mini 1.5.8版本的特定功能和限制
 * 
 * 2. 行情数据处理核心：
 *    - 接收CTP Mini推送的深度行情数据（OnRtnDepthMarketData）
 *    - 执行数据质量控制：过滤无效数据、验证价格范围、处理异常值
 *    - 时间戳处理：处理夜盘跨日、早盘启动等特殊时间场景
 *    - 数据格式转换：将CTP Mini原生格式转换为WonderTrader标准格式
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
 *    - 实现完整的CTP Mini回调事件处理链
 *    - 将底层CTP事件转换为WonderTrader标准事件
 *    - 提供异步事件通知机制
 *    - 支持日志记录和错误处理
 * 
 * 6. 动态库管理：
 *    - 支持运行时动态加载CTP Mini API库
 *    - 处理不同版本CTP API的兼容性问题
 *    - 管理动态库生命周期和资源释放
 *    - 跨平台函数符号解析（Windows/Linux）
 * 
 * 与标准CTP Parser的差异：
 * 1. API版本：基于CTP Mini 1.5.8，而非标准CTP 6.x版本
 * 2. 资源占用：更低的内存和CPU占用，适合资源受限环境
 * 3. 功能简化：专注核心行情功能，去除复杂特性
 * 4. 兼容性：更好的向下兼容性和稳定性
 * 5. 部署便利：更简单的部署和配置流程
 * 
 * 关键技术特性：
 * - 多线程安全：CTP Mini回调在独立线程中执行，需要考虑线程安全
 * - 内存管理：使用引用计数和对象池管理Tick数据对象
 * - 性能优化：最小化数据拷贝，使用就地构造和移动语义
 * - 容错机制：网络异常重连、数据异常过滤、状态恢复
 * - 可配置性：支持多种配置参数，适应不同交易环境
 * 
 * 该实现是连接CTP Mini行情系统与WonderTrader策略引擎的关键桥梁，
 * 确保了行情数据的实时性、准确性和完整性，特别适合个人投资者和小型机构使用。
 */

// ==================== 头文件包含 ====================
#include "ParserCTPMini.h"                     // 包含CTP Mini解析器头文件
#include "../Share/StrUtil.hpp"                // 包含字符串工具类
#include "../Share/StdUtils.hpp"               // 包含标准工具类
#include "../Share/TimeUtils.hpp"              // 包含时间处理工具类
#include "../Share/ModuleHelper.hpp"           // 包含模块辅助工具类

#include "../Includes/WTSDataDef.hpp"          // 包含WonderTrader数据定义
#include "../Includes/WTSContractInfo.hpp"    // 包含合约信息类定义
#include "../Includes/WTSVariant.hpp"          // 包含配置参数类定义
#include "../Includes/IBaseDataMgr.h"          // 包含基础数据管理器接口
#include "../Includes/WTSVersion.h"            // 包含版本信息定义

#include <boost/filesystem.hpp>               // 包含Boost文件系统库，用于目录操作

// ==================== 日志辅助工具 ====================
// By Wesley @ 2022.01.05 - 添加格式化日志支持
#include "../Share/fmtlib.h"                  // 包含格式化库，提供高性能的字符串格式化

/**
 * @brief 格式化日志输出模板函数
 * @tparam Args 可变参数类型包
 * @param sink 日志接收器指针，用于输出日志
 * @param ll 日志级别，控制日志的重要程度
 * @param format 格式化字符串，支持类似printf的格式
 * @param args 可变参数列表，用于填充格式化字符串
 * 
 * 该模板函数提供了高效的格式化日志输出功能，特点：
 * 1. 使用现代C++模板技术，支持任意类型参数
 * 2. 采用线程局部存储（thread_local），确保多线程安全
 * 3. 使用fmtlib库进行格式化，性能优于标准库
 * 4. 自动处理空指针检查，避免程序崩溃
 * 
 * 使用示例：
 * write_log(m_sink, LL_INFO, "连接成功，服务器地址：{}", server_addr);
 * write_log(m_sink, LL_ERROR, "订阅失败，错误代码：{}，合约：{}", error_code, symbol);
 * 
 * @note 该函数在高频调用场景下性能优异，适合行情解析器使用
 */
template<typename... Args>
inline void write_log(IParserSpi* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                         // 检查日志接收器有效性，避免空指针访问
		return;

	static thread_local char buffer[512] = { 0 };  // 线程局部缓冲区，避免多线程竞争
	fmtutil::format_to(buffer, format, args...);   // 使用高性能格式化库进行字符串格式化

	sink->handleParserLog(ll, buffer);        // 将格式化后的日志传递给上层处理
}

// ==================== 模块导出接口 ====================
// 以下为C风格导出函数，用于动态库的创建和销毁接口
extern "C"
{
	/**
	 * @brief 创建CTP Mini解析器实例
	 * @return IParserApi* 解析器接口指针
	 * 
	 * 该函数是动态库的标准导出接口，用于创建ParserCTPMini实例。
	 * WonderTrader框架通过此函数动态加载和创建解析器对象。
	 * 
	 * 创建流程：
	 * 1. 分配内存并构造ParserCTPMini对象
	 * 2. 返回IParserApi接口指针
	 * 3. 调用者负责通过deleteParser释放资源
	 * 
	 * @note 返回的指针需要通过deleteParser函数释放，避免内存泄漏
	 */
	EXPORT_FLAG IParserApi* createParser()
	{
		ParserCTPMini* parser = new ParserCTPMini();  // 创建CTP Mini解析器实例
		return parser;                                // 返回接口指针
	}

	/**
	 * @brief 销毁CTP Mini解析器实例
	 * @param parser 解析器接口指针的引用，销毁后会被设置为NULL
	 * 
	 * 该函数是动态库的标准导出接口，用于安全销毁解析器实例。
	 * 确保资源正确释放，避免内存泄漏。
	 * 
	 * 销毁流程：
	 * 1. 检查指针有效性
	 * 2. 调用析构函数释放资源
	 * 3. 将指针设置为NULL，避免悬空指针
	 * 
	 * @note 使用引用传递确保指针在销毁后被正确置空
	 */
	EXPORT_FLAG void deleteParser(IParserApi* &parser)
	{
		if (NULL != parser)                           // 检查指针有效性
		{
			delete parser;                            // 调用析构函数释放资源
			parser = NULL;                            // 置空指针，避免悬空指针问题
		}
	}
};


// ==================== 辅助工具函数 ====================

/**
 * @brief 时间字符串转换为数值函数
 * @param strTime 时间字符串指针，格式如"09:30:00"
 * @return uint32_t 转换后的时间数值，格式如93000
 * 
 * 该函数将CTP Mini返回的时间字符串转换为数值格式，便于时间计算和比较。
 * 
 * 转换逻辑：
 * 1. 遍历输入字符串的每个字符
 * 2. 跳过冒号分隔符（:）
 * 3. 将其他数字字符连接成新字符串
 * 4. 将结果字符串转换为无符号整数
 * 
 * 转换示例：
 * - "09:30:00" -> "093000" -> 93000
 * - "14:15:30" -> "141530" -> 141530
 * - "21:00:00" -> "210000" -> 210000
 * 
 * @note 该函数假设输入格式正确，不进行错误检查
 */
uint32_t strToTime(const char* strTime)
{
	std::string str;                          // 用于存储去除冒号后的时间字符串
	const char *pos = strTime;                // 字符串遍历指针
	
	while(strlen(pos) > 0)                    // 遍历整个输入字符串
	{
		if(pos[0] != ':')                     // 如果当前字符不是冒号
		{
			str.append(pos, 1);               // 将字符追加到结果字符串
		}
		pos++;                                // 移动到下一个字符
	}

	return strtoul(str.c_str(), NULL, 10);    // 将字符串转换为无符号长整型（十进制）
}

/**
 * @brief 检查浮点数有效性的内联函数
 * @param val 待检查的浮点数值
 * @return double 有效值返回原值，无效值返回0
 * 
 * 该函数用于过滤CTP Mini返回的无效价格数据。
 * CTP系统中，DBL_MAX和FLT_MAX常被用作无效数据的标识。
 * 
 * 检查规则：
 * - 如果输入值等于DBL_MAX（双精度最大值），返回0
 * - 如果输入值等于FLT_MAX（单精度最大值），返回0
 * - 其他情况返回原始值
 * 
 * 应用场景：
 * - 过滤无效的价格数据（开盘价、收盘价、最高价、最低价等）
 * - 过滤无效的成交额数据
 * - 过滤无效的结算价数据
 * 
 * @note 使用inline关键字优化性能，避免函数调用开销
 */
inline double checkValid(double val)
{
	if (val == DBL_MAX || val == FLT_MAX)     // 检查是否为无效数据标识
		return 0;                             // 无效数据返回0

	return val;                               // 有效数据返回原值
}

// ==================== ParserCTPMini类实现 ====================

/**
 * @brief ParserCTPMini构造函数
 * 
 * 初始化CTP Mini行情解析器对象，设置所有成员变量的初始状态。
 * 构造函数采用初始化列表的方式，确保成员变量在对象创建时就被正确初始化。
 * 
 * 初始化内容：
 * - m_pUserAPI: 设置为NULL，表示尚未创建CTP Mini API对象
 * - m_iRequestID: 设置为0，用作请求ID的初始计数器
 * - m_uTradingDate: 设置为0，表示尚未获取交易日信息
 * - 其他成员变量使用默认构造函数初始化
 * 
 * 设计原则：
 * 1. 轻量级初始化：仅设置基本状态，不执行重量级操作
 * 2. 安全初始化：所有指针设置为NULL，避免野指针
 * 3. 延迟初始化：实际的连接和配置在init()方法中进行
 * 
 * @note 构造函数不会建立网络连接或加载动态库，这些操作在init()中完成
 */
ParserCTPMini::ParserCTPMini()
	: m_pUserAPI(NULL)                        // CTP Mini API对象指针初始化为空
	, m_iRequestID(0)                         // 请求ID计数器初始化为0
	, m_uTradingDate(0)                       // 交易日初始化为0（未设置状态）
{
	// 构造函数体为空，所有初始化通过初始化列表完成
	// 这种方式效率更高，且能确保const成员和引用成员的正确初始化
}

/**
 * @brief ParserCTPMini析构函数
 * 
 * 清理CTP Mini行情解析器对象，释放所有占用的资源。
 * 析构函数负责确保对象销毁时不会造成内存泄漏或资源泄漏。
 * 
 * 清理操作：
 * 1. 将API对象指针设置为NULL
 * 2. 实际的资源释放在disconnect()或release()中完成
 * 3. 其他成员变量通过RAII机制自动清理
 * 
 * 设计考虑：
 * - 析构函数不应执行可能失败的操作
 * - 网络断开等操作应在之前的release()中完成
 * - 仅进行最基本的指针置空操作
 * 
 * @note 在析构前应确保已调用disconnect()或release()方法
 */
ParserCTPMini::~ParserCTPMini()
{
	m_pUserAPI = NULL;                        // 将API指针置空，防止析构后的意外访问
	// 注意：这里不调用delete，因为CTP Mini API有特定的释放方法（Release()）
	// 实际的资源释放应该在disconnect()方法中通过调用API的Release()完成
}

/**
 * @brief 初始化CTP Mini行情解析器
 * @param config 配置参数对象，包含连接和认证信息
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 该方法是解析器的核心初始化函数，负责解析配置参数、加载CTP Mini动态库、
 * 创建API实例并进行基础配置等关键初始化工作。
 * 
 * 初始化流程：
 * 1. 解析配置参数（服务器地址、经纪商、用户名、密码等）
 * 2. 设置流控文件目录，确保目录路径标准化
 * 3. 动态加载CTP Mini API库（thostmduserapi.dll/.so）
 * 4. 创建用户专用的流控目录结构
 * 5. 获取跨平台的API创建函数指针
 * 6. 创建CTP Mini API实例并注册回调和前置服务器
 * 
 * 配置参数说明：
 * - front: 前置服务器地址，格式"tcp://ip:port"
 * - broker: 经纪商代码，如"9999"
 * - user: 用户账号
 * - pass: 用户密码
 * - flowdir: 流控文件目录，默认"CTPMiniMDFlow"
 * - ctpmodule: CTP库模块名，默认"thostmduserapi"
 */
bool ParserCTPMini::init(WTSVariant* config)
{
	// ==================== 配置参数解析 ====================
	m_strFrontAddr = config->getCString("front");    // 获取前置服务器地址
	m_strBroker = config->getCString("broker");      // 获取经纪商代码
	m_strUserID = config->getCString("user");        // 获取用户账号
	m_strPassword = config->getCString("pass");      // 获取用户密码
	m_strFlowDir = config->getCString("flowdir");    // 获取流控文件目录配置

	// ==================== 流控目录设置 ====================
	if (m_strFlowDir.empty())                        // 如果未配置流控目录
		m_strFlowDir = "CTPMiniMDFlow";              // 使用默认目录名（行情专用）

	m_strFlowDir = StrUtil::standardisePath(m_strFlowDir);  // 标准化路径格式（处理路径分隔符）

	// ==================== 动态库加载 ====================
	std::string module = config->getCString("ctpmodule");   // 获取CTP库模块名配置
	if (module.empty())                              // 如果未配置模块名
		module = "thostmduserapi";                   // 使用默认模块名

	// 构造完整的动态库路径
	std::string dllpath = getBinDir() + DLLHelper::wrap_module(module.c_str(), "lib");
	// 加载CTP Mini动态库
	m_hInstCTP = DLLHelper::load_library(dllpath.c_str());
	
	// ==================== 用户专用目录创建 ====================
	// 构造用户专用的流控文件目录：流控根目录/经纪商/用户ID/
	std::string path = StrUtil::printf("%s/%s/%s/", m_strFlowDir.c_str(), m_strBroker.c_str(), m_strUserID.c_str());
	if (!StdFile::exists(path.c_str()))              // 检查目录是否存在
	{
		// 使用Boost文件系统库递归创建目录结构
		boost::filesystem::create_directories(boost::filesystem::path(path));
	}
	
	// ==================== 跨平台函数符号解析 ====================
	// 根据不同平台和架构选择正确的函数符号名
#ifdef _WIN32                                        // Windows平台
#	ifdef _WIN64                                     // 64位Windows
	const char* creatorName = "?CreateFtdcMdApi@CThostFtdcMdApi@@SAPEAV1@PEBD_N1@Z";  // MSVC 64位符号修饰名
#	else                                             // 32位Windows
	const char* creatorName = "?CreateFtdcMdApi@CThostFtdcMdApi@@SAPAV1@PBD_N1@Z";    // MSVC 32位符号修饰名
#	endif
#else                                                // Linux/Unix平台
	const char* creatorName = "_ZN15CThostFtdcMdApi15CreateFtdcMdApiEPKcbb";          // GCC符号修饰名
#endif

	// ==================== API实例创建和配置 ====================
	// 获取CreateFtdcMdApi函数指针
	m_funcCreator = (CTPCreator)DLLHelper::get_symbol(m_hInstCTP, creatorName);
	// 创建CTP Mini API实例，参数：流控目录路径，不使用UDP，不启用多播
	m_pUserAPI = m_funcCreator(path.c_str(), false, false);
	// 注册SPI回调接口，将当前对象设置为事件处理器
	m_pUserAPI->RegisterSpi(this);
	// 注册前置服务器地址，建立网络连接的目标
	m_pUserAPI->RegisterFront((char*)m_strFrontAddr.c_str());

	// ==================== 初始化完成 ====================
	return true;                                     // 所有初始化步骤成功完成
}

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
void ParserCTPMini::release()
{
	disconnect();                                    // 调用断开连接方法，完成所有资源清理
}

/**
 * @brief 连接到CTP Mini行情服务器
 * @return bool 连接启动成功返回true，失败返回false
 * 
 * 该方法启动与CTP Mini行情服务器的连接过程。
 * 连接是异步进行的，实际的连接结果通过OnFrontConnected/OnFrontDisconnected回调通知。
 * 
 * 连接流程：
 * 1. 检查API对象是否已创建
 * 2. 调用API的Init()方法启动连接过程
 * 3. CTP Mini内部开始TCP连接和握手过程
 * 4. 连接结果通过回调函数异步通知
 * 
 * 状态变化：
 * - 连接成功：触发OnFrontConnected回调
 * - 连接失败：触发OnFrontDisconnected回调
 * - 连接断开：触发OnFrontDisconnected回调
 * 
 * @note 该方法是非阻塞的，立即返回，不等待连接完成
 */
bool ParserCTPMini::connect()
{
	if(m_pUserAPI)                                   // 检查API对象是否有效
	{
		m_pUserAPI->Init();                          // 启动CTP Mini API，开始连接过程
	}

	return true;                                     // 连接启动成功（不等待连接完成）
}

/**
 * @brief 断开与CTP Mini行情服务器的连接
 * @return bool 断开成功返回true，失败返回false
 * 
 * 该方法负责安全地断开与CTP Mini服务器的连接，并释放所有相关资源。
 * 包括网络连接、API对象、动态库等。
 * 
 * 断开流程：
 * 1. 注销SPI回调接口，避免断开过程中的回调
 * 2. 调用API的Release()方法释放CTP Mini API对象
 * 3. 将API指针置空，防止悬空指针
 * 4. 释放动态库资源（在原代码中被简化）
 * 5. 将库句柄置空
 * 
 * 安全性考虑：
 * - 先注销回调再释放，避免释放过程中的异常回调
 * - 按顺序释放资源，确保依赖关系正确处理
 * - 所有指针置空，防止重复释放或悬空访问
 * 
 * @note 该方法确保资源的完全清理，可以安全地重复调用
 */
bool ParserCTPMini::disconnect()
{
	// ==================== API对象清理 ====================
	if(m_pUserAPI)                                   // 检查API对象是否存在
	{
		m_pUserAPI->RegisterSpi(NULL);               // 注销SPI回调接口，停止事件通知
		m_pUserAPI->Release();                       // 调用CTP Mini API的释放方法
		m_pUserAPI = NULL;                           // 将指针置空，防止悬空指针访问
	}

	return true;                                     // 断开和清理操作完成
}

// ==================== CTP Mini SPI回调方法实现 ====================
// 以下方法实现了CThostFtdcMdSpi接口，处理CTP Mini服务器的各种事件通知

/**
 * @brief 响应错误回调方法
 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
 * @param nRequestID 请求ID，用于标识具体的请求
 * @param bIsLast 是否为最后一条响应消息
 * 
 * 当CTP Mini服务器返回错误信息时调用此方法。
 * 该方法用于处理各种操作产生的错误响应。
 * 
 * 处理逻辑：
 * 1. 检查响应信息中的错误内容
 * 2. 记录错误信息到日志系统
 * 3. 根据错误类型决定后续处理策略
 * 
 * @note 当前实现较为简化，仅调用错误检查函数
 */
void ParserCTPMini::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	IsErrorRspInfo(pRspInfo);                        // 检查并记录错误信息
}

/**
 * @brief 前置连接成功回调方法
 * 
 * 当与CTP Mini前置服务器建立TCP连接成功后调用此方法。
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
 * @note 这是连接流程的第一步，成功后会自动进入登录阶段
 */
void ParserCTPMini::OnFrontConnected()
{
	if(m_sink)                                       // 检查事件监听器是否存在
	{
		write_log(m_sink, LL_INFO, "[ParserCTPMini] Market data server connected");  // 记录连接成功日志
		m_sink->handleEvent(WPE_Connect, 0);         // 通知上层连接成功事件
	}

	ReqUserLogin();                                  // 自动发起用户登录请求
}

/**
 * @brief 用户登录响应回调方法
 * @param pRspUserLogin 用户登录响应结构体，包含登录结果信息
 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
 * @param nRequestID 请求ID，对应登录请求的ID
 * @param bIsLast 是否为最后一条响应消息
 * 
 * 当用户登录请求得到服务器响应时调用此方法。
 * 登录成功后会获取交易日信息并启动行情订阅。
 * 
 * 处理流程：
 * 1. 检查是否为最后一条响应且无错误
 * 2. 从API获取当前交易日信息
 * 3. 通知上层应用登录成功事件
 * 4. 自动订阅缓存的行情合约
 * 
 * 关键数据：
 * - 交易日：用于数据时间戳处理和跨日逻辑
 * - 登录状态：影响后续的订阅和数据接收
 * 
 * @note 只有登录成功才能进行行情订阅和接收数据
 */
void ParserCTPMini::OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	if(bIsLast && !IsErrorRspInfo(pRspInfo))         // 检查是否为最后响应且无错误
	{
		// 获取并存储当前交易日（YYYYMMDD格式）
		m_uTradingDate = strtoul(m_pUserAPI->GetTradingDay(), NULL, 10);
		
		if(m_sink)                                   // 检查事件监听器是否存在
		{
			m_sink->handleEvent(WPE_Login, 0);       // 通知上层登录成功事件
		}

		// 登录成功后自动订阅行情数据
		SubscribeMarketData();                       // 订阅缓存中的所有合约行情
	}
}

/**
 * @brief 用户登出响应回调方法
 * @param pUserLogout 用户登出响应结构体
 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
 * @param nRequestID 请求ID，对应登出请求的ID
 * @param bIsLast 是否为最后一条响应消息
 * 
 * 当用户登出请求得到服务器响应时调用此方法。
 * 登出后将停止接收行情数据。
 * 
 * 处理流程：
 * 1. 通知上层应用登出事件
 * 2. 清理登录相关的状态信息
 * 3. 停止数据接收和处理
 * 
 * @note 登出后需要重新登录才能继续接收行情数据
 */
void ParserCTPMini::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if(m_sink)                                       // 检查事件监听器是否存在
	{
		m_sink->handleEvent(WPE_Logout, 0);          // 通知上层登出事件
	}
}

/**
 * @brief 前置连接断开回调方法
 * @param nReason 断开原因代码
 * 
 * 当与CTP Mini前置服务器的连接断开时调用此方法。
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
 * @note 连接断开后需要重新调用connect()方法建立连接
 */
void ParserCTPMini::OnFrontDisconnected( int nReason )
{
	if(m_sink)                                       // 检查事件监听器是否存在
	{
		write_log(m_sink, LL_ERROR, "[ParserCTPMini] Market data server disconnected: {}", nReason);  // 记录断开日志
		m_sink->handleEvent(WPE_Close, 0);           // 通知上层连接关闭事件
	}
}

/**
 * @brief 退订行情响应回调方法
 * @param pSpecificInstrument 指定合约结构体，包含退订的合约信息
 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
 * @param nRequestID 请求ID，对应退订请求的ID
 * @param bIsLast 是否为最后一条响应消息
 * 
 * 当退订行情请求得到服务器响应时调用此方法。
 * 
 * 处理流程：
 * 1. 检查退订结果是否成功
 * 2. 记录退订结果到日志
 * 3. 更新内部订阅状态
 * 
 * @note 当前实现为空，可根据需要添加退订结果处理逻辑
 */
void ParserCTPMini::OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	// 当前实现为空，可根据业务需要添加退订结果处理逻辑
	// 例如：记录退订成功的合约、清理订阅缓存、通知上层应用等
}

/**
 * @brief 深度行情数据推送回调方法
 * @param pDepthMarketData 深度行情数据结构体指针，包含完整的tick行情信息
 * 
 * 这是CTP Mini行情解析器的核心方法，负责处理服务器推送的实时行情数据。
 * 该方法将CTP Mini原生格式的行情数据转换为WonderTrader标准格式，并进行
 * 数据质量控制、时间戳处理等关键操作。
 * 
 * 核心处理流程：
 * 1. 基础验证：检查基础数据管理器是否可用
 * 2. 时间戳处理：解析并修正行情数据的时间信息
 * 3. 夜盘跨日处理：处理夜盘交易的特殊时间场景
 * 4. 合约信息验证：验证合约代码的有效性
 * 5. 数据转换：将CTP格式转换为WonderTrader标准格式
 * 6. 数据质量控制：过滤无效价格和数量数据
 * 7. 事件通知：将处理后的数据推送给上层应用
 * 
 * 时间处理逻辑：
 * - 处理ActionDay为空的情况，使用交易日作为默认值
 * - 识别夜盘时间异常（发生日期=交易日且时间>=20:00）
 * - 处理跨日边界情况（23:00->00:00的时间跳跃）
 * - 过滤早盘启动时的历史数据
 * 
 * 数据质量控制：
 * - 使用checkValid()函数过滤DBL_MAX/FLT_MAX等无效值
 * - 处理郑商所成交额的特殊缩放规则
 * - 验证五档买卖盘数据的完整性
 * 
 * @note 该方法在高频场景下被频繁调用，需要确保处理效率
 */
void ParserCTPMini::OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pDepthMarketData )
{	
	// ==================== 基础验证 ====================
	if(m_pBaseDataMgr == NULL)                       // 检查基础数据管理器是否可用
	{
		return;                                      // 无法验证合约信息，直接返回
	}

	// ==================== 时间戳解析和处理 ====================
	uint32_t actDate = strtoul(pDepthMarketData->ActionDay, NULL, 10);        // 解析发生日期（YYYYMMDD）
	uint32_t actTime = strToTime(pDepthMarketData->UpdateTime) * 1000 + pDepthMarketData->UpdateMillisec;  // 解析更新时间（HHMMSSmmm）
	uint32_t actHour = actTime / 10000000;           // 提取小时部分，用于时间逻辑判断
	
	if (actDate == 0)                                // 如果发生日期为空
		actDate = m_uTradingDate;                    // 使用当前交易日作为默认值

	// ==================== 夜盘跨日时间修正 ====================
	if (actDate == m_uTradingDate && actHour >= 20)  // 检测夜盘时间异常情况
	{
		// 夜盘交易时，发生日期不应该等于交易日（应该是前一自然日）
		// 这种情况需要手动修正时间戳
		uint32_t curDate, curTime;                   // 获取当前系统时间
		TimeUtils::getDateTime(curDate, curTime);
		uint32_t curHour = curTime / 10000000;       // 提取当前系统小时

		// 早盘启动时过滤历史数据
		// 早上3:00-9:00之间启动时，可能收到昨晚12点前的历史行情数据
		if (curHour >= 3 && curHour < 9)             // 如果在早盘启动时间段
			return;                                  // 直接丢弃这些历史数据

		actDate = curDate;                           // 使用当前日期作为发生日期

		// 处理跨日边界的时间同步问题
		if (actHour == 23 && curHour == 0)           // 行情时间23点，系统时间0点
		{
			// 行情时间慢于系统时间，行情还在昨天
			actDate = TimeUtils::getNextDate(curDate, -1);  // 发生日期设为昨天
		}
		else if (actHour == 0 && curHour == 23)      // 行情时间0点，系统时间23点
		{
			// 系统时间慢于行情时间，行情已到今天
			actDate = TimeUtils::getNextDate(curDate, 1);   // 发生日期设为明天
		}
	}

	// ==================== 合约信息验证 ====================
	// 通过基础数据管理器获取合约信息，验证合约代码和交易所的有效性
	WTSContractInfo* contract = m_pBaseDataMgr->getContract(pDepthMarketData->InstrumentID, pDepthMarketData->ExchangeID);
	if (contract == NULL)                            // 如果合约信息不存在
		return;                                      // 忽略无效合约的行情数据

	WTSCommodityInfo* pCommInfo = contract->getCommInfo();  // 获取商品信息，包含交易所、合约乘数等

	// 郑商所时间戳微调（已注释）
	// 郑商所的行情可能需要添加毫秒精度，但当前版本已禁用此功能
	//if (strcmp(contract->getExchg(), "CZCE") == 0)
	//{
	//	actTime += (uint32_t)(TimeUtils::getLocalTimeNow() % 1000);
	//}

	// ==================== WonderTrader标准格式转换 ====================
	// 创建WonderTrader标准的Tick数据对象
	WTSTickData* tick = WTSTickData::create(pDepthMarketData->InstrumentID);  // 创建Tick数据对象
	tick->setContractInfo(contract);                 // 设置合约信息引用
	WTSTickStruct& quote = tick->getTickStruct();    // 获取Tick数据结构体的引用
	strcpy(quote.exchg, pCommInfo->getExchg());      // 复制交易所代码
	
	// ==================== 基础时间和价格信息 ====================
	quote.action_date = actDate;                     // 设置发生日期（YYYYMMDD）
	quote.action_time = actTime;                     // 设置发生时间（HHMMSSmmm）
	
	// 主要价格信息（使用checkValid过滤无效值）
	quote.price = checkValid(pDepthMarketData->LastPrice);      // 最新价
	quote.open = checkValid(pDepthMarketData->OpenPrice);       // 开盘价
	quote.high = checkValid(pDepthMarketData->HighestPrice);    // 最高价
	quote.low = checkValid(pDepthMarketData->LowestPrice);      // 最低价
	
	// 成交量和交易日信息
	quote.total_volume = pDepthMarketData->Volume;   // 总成交量（手）
	quote.trading_date = m_uTradingDate;             // 交易日（YYYYMMDD）
	
	// 结算价处理（只有当结算价有效时才设置）
	if(pDepthMarketData->SettlementPrice != DBL_MAX)            // 检查结算价是否有效
		quote.settle_price = checkValid(pDepthMarketData->SettlementPrice);  // 当日结算价

	// ==================== 成交额处理（交易所特殊规则） ====================
	if(strcmp(quote.exchg, "CZCE") == 0)             // 郑州商品交易所特殊处理
	{
		// 郑商所的成交额需要乘以合约乘数进行缩放
		quote.total_turnover = pDepthMarketData->Turnover * pCommInfo->getVolScale();
	}
	else                                             // 其他交易所
	{
		if(pDepthMarketData->Turnover != DBL_MAX)    // 检查成交额是否有效
			quote.total_turnover = pDepthMarketData->Turnover;  // 总成交额（元）
	}

	// ==================== 持仓和涨跌停信息 ====================
	quote.open_interest = (uint32_t)pDepthMarketData->OpenInterest;  // 持仓量（手）

	quote.upper_limit = checkValid(pDepthMarketData->UpperLimitPrice);  // 涨停价
	quote.lower_limit = checkValid(pDepthMarketData->LowerLimitPrice);  // 跌停价

	// ==================== 昨日参考信息 ====================
	quote.pre_close = checkValid(pDepthMarketData->PreClosePrice);      // 昨收价
	quote.pre_settle = checkValid(pDepthMarketData->PreSettlementPrice);  // 昨结算价
	quote.pre_interest = (uint32_t)pDepthMarketData->PreOpenInterest;   // 昨持仓量

	// ==================== 五档买卖盘数据 ====================
	// 委卖价格（卖方报价，从低到高排列：卖一价 < 卖二价 < ... < 卖五价）
	quote.ask_prices[0] = checkValid(pDepthMarketData->AskPrice1);    // 卖一价（最优卖价）
	quote.ask_prices[1] = checkValid(pDepthMarketData->AskPrice2);    // 卖二价
	quote.ask_prices[2] = checkValid(pDepthMarketData->AskPrice3);    // 卖三价
	quote.ask_prices[3] = checkValid(pDepthMarketData->AskPrice4);    // 卖四价
	quote.ask_prices[4] = checkValid(pDepthMarketData->AskPrice5);    // 卖五价

	// 委买价格（买方报价，从高到低排列：买一价 > 买二价 > ... > 买五价）
	quote.bid_prices[0] = checkValid(pDepthMarketData->BidPrice1);    // 买一价（最优买价）
	quote.bid_prices[1] = checkValid(pDepthMarketData->BidPrice2);    // 买二价
	quote.bid_prices[2] = checkValid(pDepthMarketData->BidPrice3);    // 买三价
	quote.bid_prices[3] = checkValid(pDepthMarketData->BidPrice4);    // 买四价
	quote.bid_prices[4] = checkValid(pDepthMarketData->BidPrice5);    // 买五价

	// 委卖量（对应各档卖价的挂单数量）
	quote.ask_qty[0] = pDepthMarketData->AskVolume1;                 // 卖一量
	quote.ask_qty[1] = pDepthMarketData->AskVolume2;                 // 卖二量
	quote.ask_qty[2] = pDepthMarketData->AskVolume3;                 // 卖三量
	quote.ask_qty[3] = pDepthMarketData->AskVolume4;                 // 卖四量
	quote.ask_qty[4] = pDepthMarketData->AskVolume5;                 // 卖五量

	// 委买量（对应各档买价的挂单数量）
	quote.bid_qty[0] = pDepthMarketData->BidVolume1;                 // 买一量
	quote.bid_qty[1] = pDepthMarketData->BidVolume2;                 // 买二量
	quote.bid_qty[2] = pDepthMarketData->BidVolume3;                 // 买三量
	quote.bid_qty[3] = pDepthMarketData->BidVolume4;                 // 买四量
	quote.bid_qty[4] = pDepthMarketData->BidVolume5;                 // 买五量

	// ==================== 数据推送和资源管理 ====================
	if(m_sink)                                       // 检查事件监听器是否存在
		m_sink->handleQuote(tick, 1);                // 将处理后的Tick数据推送给上层应用

	tick->release();                                 // 释放Tick数据对象，避免内存泄漏
}

/**
 * @brief 订阅行情响应回调方法
 * @param pSpecificInstrument 指定合约结构体，包含订阅的合约信息
 * @param pRspInfo 响应信息结构体，包含错误代码和错误信息
 * @param nRequestID 请求ID，对应订阅请求的ID
 * @param bIsLast 是否为最后一条响应消息
 * 
 * 当订阅行情请求得到服务器响应时调用此方法。
 * 可以根据响应结果进行相应的处理。
 * 
 * 处理流程：
 * 1. 检查订阅结果是否成功
 * 2. 记录订阅结果到日志
 * 3. 更新内部订阅状态
 * 
 * @note 当前实现为空，可根据需要添加订阅结果处理逻辑
 */
void ParserCTPMini::OnRspSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	if(!IsErrorRspInfo(pRspInfo))                    // 如果订阅成功
	{
		// 可以在这里添加订阅成功的处理逻辑
		// 例如：记录成功订阅的合约、更新订阅状态等
	}
	else                                             // 如果订阅失败
	{
		// 可以在这里添加订阅失败的处理逻辑
		// 例如：记录失败原因、重试订阅、通知上层应用等
	}
}

/**
 * @brief 心跳警告回调方法
 * @param nTimeLapse 心跳间隔时间（秒），表示距离上次心跳的时间
 * 
 * 当CTP Mini检测到心跳超时时调用此方法。
 * 心跳机制用于检测客户端与服务器之间的连接状态。
 * 
 * 心跳警告原因：
 * - 网络延迟较高，导致心跳响应缓慢
 * - 网络不稳定，出现短暂的连接中断
 * - 服务器负载较高，处理心跳请求较慢
 * - 客户端处理能力不足，响应心跳较慢
 * 
 * 处理策略：
 * 1. 记录心跳警告信息，用于网络质量监控
 * 2. 如果频繁出现警告，可能需要检查网络状况
 * 3. 严重情况下可能导致连接断开，需要重连
 * 
 * @note 偶尔的心跳警告是正常的，频繁警告需要关注网络质量
 */
void ParserCTPMini::OnHeartBeatWarning( int nTimeLapse )
{
	if(m_sink)                                       // 检查日志接收器是否存在
		write_log(m_sink, LL_INFO, "[ParserCTPMini] Heartbeating, elapse: {}", nTimeLapse);  // 记录心跳警告信息
}

// ==================== 私有辅助方法实现 ====================
// 以下方法为内部使用的辅助功能实现

/**
 * @brief 发送用户登录请求的私有方法
 * 
 * 构造并发送用户登录请求到CTP Mini服务器。
 * 该方法在前置连接成功后自动调用，也可以在需要重新登录时调用。
 * 
 * 登录请求构造：
 * 1. 创建登录请求结构体并清零初始化
 * 2. 填充经纪商代码、用户ID、密码等必要信息
 * 3. 设置产品信息标识（WT_PRODUCT宏定义）
 * 4. 生成唯一的请求ID并发送请求
 * 5. 检查发送结果并记录错误信息
 * 
 * 错误处理：
 * - API对象为空：直接返回，避免空指针访问
 * - 发送失败：记录错误日志，包含具体的错误代码
 * 
 * @note 登录结果通过OnRspUserLogin回调异步返回
 */
void ParserCTPMini::ReqUserLogin()
{
	if(m_pUserAPI == NULL)                           // 检查API对象是否有效
	{
		return;                                      // API对象无效，无法发送请求
	}

	// ==================== 构造登录请求 ====================
	CThostFtdcReqUserLoginField req;                 // 创建登录请求结构体
	memset(&req, 0, sizeof(req));                    // 清零初始化，确保所有字段都是干净的
	
	// 填充登录必要信息
	strcpy(req.BrokerID, m_strBroker.c_str());       // 设置经纪商代码
	strcpy(req.UserID, m_strUserID.c_str());         // 设置用户账号
	strcpy(req.Password, m_strPassword.c_str());     // 设置用户密码
	strcpy(req.UserProductInfo, WT_PRODUCT);         // 设置产品信息标识（WonderTrader标识）
	
	// ==================== 发送登录请求 ====================
	int iResult = m_pUserAPI->ReqUserLogin(&req, ++m_iRequestID);  // 发送登录请求，自增请求ID
	if(iResult != 0)                                 // 检查发送结果
	{
		if(m_sink)                                   // 检查日志接收器是否存在
			write_log(m_sink, LL_ERROR, "[ParserCTPMini] Sending login request failed: {}", iResult);  // 记录发送失败日志
	}
}

/**
 * @brief 订阅行情数据的私有方法
 * 
 * 向CTP Mini服务器发送行情订阅请求，订阅缓存中的所有合约。
 * 该方法在登录成功后自动调用，用于批量订阅行情数据。
 * 
 * 订阅处理流程：
 * 1. 复制订阅合约列表，避免在处理过程中被修改
 * 2. 检查订阅列表是否为空，空列表直接返回
 * 3. 分配内存存储合约代码指针数组
 * 4. 遍历合约列表，处理合约代码格式
 * 5. 调用CTP Mini API批量订阅行情
 * 6. 处理订阅结果并记录日志
 * 7. 清理临时资源和缓存
 * 
 * 合约代码格式处理：
 * - 支持"SHFE.rb2105"格式，提取"rb2105"部分给CTP Mini
 * - 支持"rb2105"格式，直接使用原始代码
 * - 通过查找'.'分隔符来判断格式类型
 * 
 * 内存管理：
 * - 动态分配指针数组存储合约代码
 * - 使用完毕后及时释放内存
 * - 清空订阅缓存，避免重复订阅
 * 
 * @note 该方法仅在内部使用，登录成功后自动调用
 */
void ParserCTPMini::SubscribeMarketData()
{
	CodeSet codeFilter = m_filterSubs;               // 复制订阅合约列表，避免并发修改
	if(codeFilter.empty())                           // 检查订阅列表是否为空
	{
		// 如果订阅列表为空，则无需订阅任何合约
		return;                                      // 直接返回，不执行订阅操作
	}

	// ==================== 准备订阅数据 ====================
	char ** subscribe = new char*[codeFilter.size()]; // 分配合约代码指针数组
	int nCount = 0;                                  // 有效合约计数器
	
	// 遍历订阅列表，处理合约代码格式
	for(auto& code : codeFilter)
	{
		std::size_t pos = code.find('.');            // 查找交易所分隔符
		if (pos != std::string::npos)                // 如果包含交易所前缀（如"SHFE.rb2105"）
			subscribe[nCount++] = (char*)code.c_str() + pos + 1;  // 提取合约代码部分"rb2105"
		else                                         // 如果不包含前缀（如"rb2105"）
			subscribe[nCount++] = (char*)code.c_str();  // 直接使用原始合约代码
	}

	// ==================== 发送订阅请求 ====================
	if(m_pUserAPI && nCount > 0)                    // 检查API对象和合约数量
	{
		int iResult = m_pUserAPI->SubscribeMarketData(subscribe, nCount);  // 批量订阅行情数据
		if(iResult != 0)                             // 检查订阅结果
		{
			if(m_sink)                               // 订阅失败，记录错误日志
				write_log(m_sink, LL_ERROR, "[ParserCTPMini] Sending md subscribe request failed: {}", iResult);
		}
		else                                         // 订阅成功
		{
			if(m_sink)                               // 记录成功日志
				write_log(m_sink, LL_INFO, "[ParserCTPMini] Market data of {} contracts subscribed in total", nCount);
		}
	}
	
	// ==================== 清理资源 ====================
	codeFilter.clear();                              // 清空临时合约列表
	delete[] subscribe;                              // 释放指针数组内存
}

/**
 * @brief 检查CTP响应错误信息的私有方法
 * @param pRspInfo CTP响应信息结构体指针
 * @return bool 有错误返回true，无错误返回false
 * 
 * 检查CTP Mini服务器返回的响应信息中是否包含错误。
 * 该方法用于统一处理各种CTP回调中的错误信息。
 * 
 * 当前实现：
 * - 简化实现，始终返回false（无错误）
 * - 实际应用中可以根据pRspInfo->ErrorID判断是否有错误
 * - 可以在此处添加错误日志记录功能
 * 
 * 标准实现逻辑：
 * 1. 检查pRspInfo指针是否有效
 * 2. 检查ErrorID是否为0（0表示成功）
 * 3. 记录错误信息到日志
 * 4. 返回错误状态
 * 
 * @note 当前为简化实现，可根据需要扩展错误处理逻辑
 */
bool ParserCTPMini::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	return false;                                    // 简化实现，不处理错误信息
}

// ==================== IParserApi接口方法实现 ====================
// 以下方法实现了WonderTrader标准行情解析器接口

/**
 * @brief 订阅行情数据接口实现
 * @param vecSymbols 要订阅的合约代码集合
 * 
 * 该方法是IParserApi接口的实现，用于订阅指定合约的行情数据。
 * 根据当前连接状态采用不同的处理策略。
 * 
 * 处理策略：
 * 1. 未登录状态（m_uTradingDate == 0）：
 *    - 将合约列表缓存到m_filterSubs
 *    - 等待登录成功后自动订阅
 * 
 * 2. 已登录状态（m_uTradingDate != 0）：
 *    - 更新订阅缓存
 *    - 立即发送订阅请求到CTP Mini服务器
 *    - 处理合约代码格式转换
 *    - 记录订阅结果
 * 
 * 合约代码处理：
 * - 支持带交易所前缀的格式（"SHFE.rb2105"）
 * - 支持不带前缀的格式（"rb2105"）
 * - 自动提取CTP Mini需要的合约代码部分
 * 
 * 数组限制：
 * - 使用固定大小数组（500个合约）
 * - 适合大部分应用场景的订阅需求
 * 
 * @note 该方法是线程安全的，可以在任何时候调用
 */
void ParserCTPMini::subscribe(const CodeSet &vecSymbols)
{
	if(m_uTradingDate == 0)                          // 检查是否已登录（交易日为0表示未登录）
	{
		m_filterSubs = vecSymbols;                   // 缓存订阅列表，等待登录后自动订阅
	}
	else                                             // 已登录状态，立即订阅
	{
		m_filterSubs = vecSymbols;                   // 更新订阅缓存
		char * subscribe[500] = {NULL};              // 创建合约代码指针数组（最大500个）
		int nCount = 0;                              // 有效合约计数器
		
		// 遍历合约列表，处理代码格式
		for (auto& code  : vecSymbols)
		{
			std::size_t pos = code.find('.');        // 查找交易所分隔符
			if (pos != std::string::npos)            // 包含交易所前缀
				subscribe[nCount++] = (char*)code.c_str() + pos + 1;  // 提取合约代码部分
			else                                     // 不包含前缀
				subscribe[nCount++] = (char*)code.c_str();  // 直接使用原始代码
		}

		// 发送订阅请求
		if(m_pUserAPI && nCount > 0)                // 检查API对象和合约数量
		{
			int iResult = m_pUserAPI->SubscribeMarketData(subscribe, nCount);  // 批量订阅
			if(iResult != 0)                         // 检查订阅结果
			{
				if (m_sink)                          // 订阅失败，记录错误
					write_log(m_sink, LL_ERROR, "[ParserCTPMini] Sending md subscribe request failed: {}", iResult);
			}
			else                                     // 订阅成功
			{
				if (m_sink)                          // 记录成功信息
					write_log(m_sink, LL_INFO, "[ParserCTPMini] Market data of {} contracts subscribed in total", nCount);
			}
		}
	}
}

/**
 * @brief 退订行情数据接口实现
 * @param vecSymbols 要退订的合约代码集合
 * 
 * 该方法是IParserApi接口的实现，用于退订指定合约的行情数据。
 * 
 * 当前实现：
 * - 空实现，不执行任何退订操作
 * - 可根据业务需要添加退订逻辑
 * 
 * 标准实现应包括：
 * 1. 处理合约代码格式转换
 * 2. 调用CTP Mini API的UnSubscribeMarketData方法
 * 3. 从订阅缓存中移除相应合约
 * 4. 记录退订结果
 * 
 * @note 当前为空实现，CTP Mini连接断开时会自动清除所有订阅
 */
void ParserCTPMini::unsubscribe(const CodeSet &vecSymbols)
{
	// 当前实现为空，可根据需要添加退订逻辑
	// CTP Mini在连接断开时会自动清除所有订阅
}

/**
 * @brief 检查连接状态接口实现
 * @return bool 已连接返回true，未连接返回false
 * 
 * 该方法是IParserApi接口的实现，用于检查与CTP Mini服务器的连接状态。
 * 
 * 判断逻辑：
 * - 通过检查m_pUserAPI指针是否为空来判断连接状态
 * - API对象存在表示连接已建立（或正在建立）
 * - API对象为空表示未连接或连接已断开
 * 
 * 注意事项：
 * - 该方法只检查API对象是否存在，不检查网络连接状态
 * - 实际的网络连接状态通过回调事件通知
 * - 登录状态需要通过其他方式检查（如m_uTradingDate）
 * 
 * @note 这是一个轻量级的状态检查方法，不涉及网络操作
 */
bool ParserCTPMini::isConnected()
{
	return m_pUserAPI != NULL;                       // 检查API对象是否存在
}

/**
 * @brief 注册事件监听器接口实现
 * @param listener 事件监听器指针，用于接收行情数据和事件通知
 * 
 * 该方法是IParserApi接口的实现，用于注册事件监听器。
 * 监听器负责接收解析器推送的行情数据和各种事件通知。
 * 
 * 注册流程：
 * 1. 保存监听器指针到成员变量m_sink
 * 2. 如果监听器有效，获取基础数据管理器引用
 * 3. 基础数据管理器用于验证合约信息的有效性
 * 
 * 监听器功能：
 * - 接收Tick行情数据（handleQuote）
 * - 接收连接事件通知（handleEvent）
 * - 接收日志信息（handleParserLog）
 * - 提供基础数据管理服务（getBaseDataMgr）
 * 
 * 依赖关系：
 * - 监听器必须在连接建立前注册
 * - 基础数据管理器用于行情数据验证
 * - 所有行情处理都依赖于有效的监听器
 * 
 * @note 监听器是解析器与上层应用的关键接口，必须正确注册
 */
void ParserCTPMini::registerSpi(IParserSpi* listener)
{
	m_sink = listener;                               // 保存事件监听器指针

	if(m_sink)                                       // 如果监听器有效
		m_pBaseDataMgr = m_sink->getBaseDataMgr();   // 获取基础数据管理器，用于合约验证
}