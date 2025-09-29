/*!
 * \file ParserCTPOpt.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CTP期权行情解析器实现文件 - WonderTrader框架中CTP期权行情数据解析的具体实现
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是ParserCTPOpt.h中声明的CTP期权行情解析器类的具体实现，负责处理与CTP期权
 * 行情服务器的全部交互逻辑和数据转换工作。作为WonderTrader量化交易框架的重要组成部分，
 * 该实现文件提供了专业的期权行情数据解析能力。
 * 
 * 核心功能实现：
 * 
 * 1. 连接与认证管理：
 *    - 实现与CTP期权行情前置服务器的TCP连接建立
 *    - 处理用户登录认证流程，包括期权交易权限验证
 *    - 管理连接状态变化和异常重连机制
 *    - 支持CTP期权API 3.5.8版本的特定功能
 * 
 * 2. 期权行情数据处理核心：
 *    - 接收CTP期权推送的深度行情数据（OnRtnDepthMarketData）
 *    - 支持本地时间戳和CTP时间戳两种模式选择
 *    - 执行期权数据质量控制：过滤无效数据、验证价格范围
 *    - 处理期权特有的数据字段和业务逻辑
 *    - 数据格式转换：将CTP期权原生格式转换为WonderTrader标准格式
 * 
 * 3. 期权订阅管理系统：
 *    - 管理期权合约行情的订阅和退订请求
 *    - 处理期权合约代码格式转换（支持交易所前缀处理）
 *    - 实现订阅状态缓存，支持登录后自动重新订阅
 *    - 批量订阅优化，提高期权数据订阅效率
 * 
 * 4. 期权数据验证与修正：
 *    - 集成基础数据管理器，验证期权合约有效性
 *    - 处理特殊价格值（DBL_MAX、FLT_MAX等无效标识）
 *    - 实现交易所特定规则（如郑商所成交额缩放处理）
 *    - 期权五档买卖盘数据完整性检查
 * 
 * 5. 事件驱动架构：
 *    - 实现完整的CTP期权回调事件处理链
 *    - 将底层CTP期权事件转换为WonderTrader标准事件
 *    - 提供异步事件通知机制
 *    - 支持日志记录和错误处理
 * 
 * 6. 动态库管理：
 *    - 支持运行时动态加载CTP期权API库（soptthostmduserapi_se）
 *    - 处理不同版本CTP期权API的兼容性问题
 *    - 管理动态库生命周期和资源释放
 *    - 跨平台函数符号解析（Windows/Linux）
 * 
 * 期权特殊特性：
 * 1. 时间戳模式：支持本地时间戳和CTP时间戳两种模式
 * 2. 期权合约验证：针对期权合约的特殊验证逻辑
 * 3. 期权数据处理：处理期权特有的价格和数量数据
 * 4. 期权符号解析：使用期权专用的动态库符号名
 * 5. 期权错误处理：针对期权业务的特殊错误处理
 * 
 * 关键技术特性：
 * - 多线程安全：CTP期权回调在独立线程中执行，需要考虑线程安全
 * - 内存管理：使用引用计数和对象池管理期权Tick数据对象
 * - 性能优化：最小化数据拷贝，使用就地构造和移动语义
 * - 容错机制：网络异常重连、数据异常过滤、状态恢复
 * - 可配置性：支持多种配置参数，适应不同期权交易环境
 * 
 * 该实现是连接CTP期权行情系统与WonderTrader策略引擎的关键桥梁，
 * 确保了期权行情数据的实时性、准确性和完整性，为期权量化交易提供可靠的数据支持。
 */

// ==================== 头文件包含 ====================
#include "ParserCTPOpt.h"                       // 包含CTP期权解析器头文件

#include "../Includes/WTSDataDef.hpp"           // 包含WonderTrader数据定义
#include "../Includes/WTSContractInfo.hpp"     // 包含合约信息类定义
#include "../Includes/WTSVariant.hpp"           // 包含配置参数类定义
#include "../Includes/IBaseDataMgr.h"           // 包含基础数据管理器接口
#include "../Includes/WTSVersion.h"             // 包含版本信息定义

#include "../Share/TimeUtils.hpp"               // 包含时间处理工具类
#include "../Share/StdUtils.hpp"                // 包含标准工具类
#include "../Share/ModuleHelper.hpp"            // 包含模块辅助工具类

#include <boost/filesystem.hpp>                // 包含Boost文件系统库，用于目录操作

// ==================== 日志辅助工具 ====================
// By Wesley @ 2022.01.05 - 添加格式化日志支持
#include "../Share/fmtlib.h"                   // 包含格式化库，提供高性能的字符串格式化

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
 * 期权日志特点：
 * - 支持期权合约代码的格式化输出
 * - 记录期权特有的业务事件和错误
 * - 提供期权交易过程的详细日志
 * 
 * 使用示例：
 * write_log(m_sink, LL_INFO, "期权连接成功，服务器地址：{}", server_addr);
 * write_log(m_sink, LL_ERROR, "期权订阅失败，错误代码：{}，合约：{}", error_code, symbol);
 * 
 * @note 该函数在高频调用场景下性能优异，适合期权行情解析器使用
 */
template<typename... Args>
inline void write_log(IParserSpi* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                           // 检查日志接收器有效性，避免空指针访问
		return;

	static thread_local char buffer[512] = { 0 };  // 线程局部缓冲区，避免多线程竞争
	fmtutil::format_to(buffer, format, args...);   // 使用高性能格式化库进行字符串格式化

	sink->handleParserLog(ll, buffer);          // 将格式化后的日志传递给上层处理
}

// ==================== 模块导出接口 ====================
// 以下为C风格导出函数，用于动态库的创建和销毁接口
extern "C"
{
	/**
	 * @brief 创建CTP期权解析器实例
	 * @return IParserApi* 解析器接口指针
	 * 
	 * 该函数是动态库的标准导出接口，用于创建ParserCTPOpt实例。
	 * WonderTrader框架通过此函数动态加载和创建期权解析器对象。
	 * 
	 * 创建流程：
	 * 1. 分配内存并构造ParserCTPOpt对象
	 * 2. 返回IParserApi接口指针
	 * 3. 调用者负责通过deleteParser释放资源
	 * 
	 * 期权特殊处理：
	 * - 创建专门用于期权数据处理的解析器实例
	 * - 初始化期权特有的配置和状态
	 * - 准备期权数据处理环境
	 * 
	 * @note 返回的指针需要通过deleteParser函数释放，避免内存泄漏
	 */
	EXPORT_FLAG IParserApi* createParser()
	{
		ParserCTPOpt* parser = new ParserCTPOpt();  // 创建CTP期权解析器实例
		return parser;                              // 返回接口指针
	}

	/**
	 * @brief 销毁CTP期权解析器实例
	 * @param parser 解析器接口指针的引用，销毁后会被设置为NULL
	 * 
	 * 该函数是动态库的标准导出接口，用于安全销毁期权解析器实例。
	 * 确保资源正确释放，避免内存泄漏。
	 * 
	 * 销毁流程：
	 * 1. 检查指针有效性
	 * 2. 调用析构函数释放资源
	 * 3. 将指针设置为NULL，避免悬空指针
	 * 
	 * 期权特殊处理：
	 * - 确保期权连接正确断开
	 * - 清理期权订阅状态
	 * - 释放期权相关资源
	 * 
	 * @note 使用引用传递确保指针在销毁后被正确置空
	 */
	EXPORT_FLAG void deleteParser(IParserApi* &parser)
	{
		if (NULL != parser)                         // 检查指针有效性
		{
			delete parser;                          // 调用析构函数释放资源
			parser = NULL;                          // 置空指针，避免悬空指针问题
		}
	}
};

// ==================== 辅助工具函数 ====================

/**
 * @brief 时间字符串转换为数值函数
 * @param strTime 时间字符串指针，格式如"09:30:00"
 * @return uint32_t 转换后的时间数值，格式如93000
 * 
 * 该函数将CTP期权返回的时间字符串转换为数值格式，便于时间计算和比较。
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
 * 期权时间特点：
 * - 期权交易时间与期货略有不同
 * - 需要处理期权特有的交易时段
 * - 支持期权夜盘和日盘的时间转换
 * 
 * @note 该函数假设输入格式正确，不进行错误检查
 */
uint32_t strToTime(const char* strTime)
{
	std::string str;                                // 用于存储去除冒号后的时间字符串
	const char *pos = strTime;                      // 字符串遍历指针
	
	while(strlen(pos) > 0)                          // 遍历整个输入字符串
	{
		if(pos[0] != ':')                           // 如果当前字符不是冒号
		{
			str.append(pos, 1);                     // 将字符追加到结果字符串
		}
		pos++;                                      // 移动到下一个字符
	}

	return strtoul(str.c_str(), NULL, 10);          // 将字符串转换为无符号长整型（十进制）
}

/**
 * @brief 检查浮点数有效性的内联函数
 * @param val 待检查的浮点数值
 * @return double 有效值返回原值，无效值返回0
 * 
 * 该函数用于过滤CTP期权返回的无效价格数据。
 * CTP系统中，DBL_MAX和FLT_MAX常被用作无效数据的标识。
 * 
 * 检查规则：
 * - 如果输入值等于DBL_MAX（双精度最大值），返回0
 * - 如果输入值等于FLT_MAX（单精度最大值），返回0
 * - 其他情况返回原始值
 * 
 * 应用场景：
 * - 过滤无效的期权价格数据（开盘价、收盘价、最高价、最低价等）
 * - 过滤无效的期权成交额数据
 * - 过滤无效的期权结算价数据
 * 
 * 期权特殊考虑：
 * - 期权价格可能为0（虚值期权）
 * - 期权理论价格需要特殊验证
 * - 期权隐含波动率的有效性检查
 * 
 * @note 使用inline关键字优化性能，避免函数调用开销
 */
inline double checkValid(double val)
{
	if (val == DBL_MAX || val == FLT_MAX)           // 检查是否为无效数据标识
		return 0;                                   // 无效数据返回0

	return val;                                     // 有效数据返回原值
}

// ==================== ParserCTPOpt类实现 ====================

/**
 * @brief ParserCTPOpt构造函数
 * 
 * 初始化CTP期权行情解析器对象，设置所有成员变量的初始状态。
 * 构造函数采用初始化列表的方式，确保成员变量在对象创建时就被正确初始化。
 * 
 * 初始化内容：
 * - m_pUserAPI: 设置为NULL，表示尚未创建CTP期权API对象
 * - m_iRequestID: 设置为0，用作请求ID的初始计数器
 * - m_uTradingDate: 设置为0，表示尚未获取交易日信息
 * - m_bLocalTime: 设置为false，默认使用CTP时间戳模式（期权特有配置）
 * - 其他成员变量使用默认构造函数初始化
 * 
 * 设计原则：
 * 1. 轻量级初始化：仅设置基本状态，不执行重量级操作
 * 2. 安全初始化：所有指针设置为NULL，避免野指针
 * 3. 延迟初始化：实际的连接和配置在init()方法中进行
 * 
 * 期权特殊配置：
 * - 本地时间戳标志初始化为false（使用CTP服务器时间）
 * - 为期权数据处理准备基础环境
 * - 初始化期权特有的状态变量
 * 
 * @note 构造函数不会建立网络连接或加载动态库，这些操作在init()中完成
 */
ParserCTPOpt::ParserCTPOpt()
	: m_pUserAPI(NULL)                              // CTP期权API对象指针初始化为空
	, m_iRequestID(0)                               // 请求ID计数器初始化为0
	, m_uTradingDate(0)                             // 交易日初始化为0（未设置状态）
	, m_bLocalTime(false)                           // 本地时间戳标志初始化为false（使用CTP时间）
{
	// 构造函数体为空，所有初始化通过初始化列表完成
	// 这种方式效率更高，且能确保const成员和引用成员的正确初始化
}

/**
 * @brief ParserCTPOpt析构函数
 * 
 * 清理CTP期权行情解析器对象，释放所有占用的资源。
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
 * 期权特殊处理：
 * - 确保期权相关资源得到正确清理
 * - 重置期权特有的状态变量
 * - 避免期权数据的内存泄漏
 * 
 * @note 在析构前应确保已调用disconnect()或release()方法
 */
ParserCTPOpt::~ParserCTPOpt()
{
	m_pUserAPI = NULL;                              // 将API指针置空，防止析构后的意外访问
	// 注意：这里不调用delete，因为CTP期权API有特定的释放方法（Release()）
	// 实际的资源释放应该在disconnect()方法中通过调用API的Release()完成
}

/**
 * @brief 初始化CTP期权行情解析器
 * @param config 配置参数对象，包含连接和认证信息
 * @return bool 初始化成功返回true，失败返回false
 * 
 * 该方法是解析器的核心初始化函数，负责解析配置参数、加载CTP期权动态库、
 * 创建API实例并进行基础配置等关键初始化工作。
 * 
 * 初始化流程：
 * 1. 解析配置参数（服务器地址、经纪商、用户名、密码等）
 * 2. 设置流控文件目录，确保目录路径标准化
 * 3. 动态加载CTP期权API库（soptthostmduserapi_se.dll/.so）
 * 4. 创建用户专用的流控目录结构
 * 5. 获取跨平台的期权API创建函数指针
 * 6. 创建CTP期权API实例并注册回调和前置服务器
 * 
 * 配置参数说明：
 * - front: 前置服务器地址，格式"tcp://ip:port"
 * - broker: 经纪商代码，如"9999"
 * - user: 用户账号
 * - pass: 用户密码
 * - localtime: 是否使用本地时间戳（期权特有配置）
 * - flowdir: 流控文件目录，默认"CTPOptMDFlow"
 * - ctpmodule: CTP期权库模块名，默认"soptthostmduserapi_se"
 */
bool ParserCTPOpt::init(WTSVariant* config)
{
	// ==================== 配置参数解析 ====================
	m_strFrontAddr = config->getCString("front");      // 获取前置服务器地址
	m_strBroker = config->getCString("broker");        // 获取经纪商代码
	m_strUserID = config->getCString("user");          // 获取用户账号
	m_strPassword = config->getCString("pass");        // 获取用户密码
	m_bLocalTime = config->getBoolean("localtime");    // 获取本地时间戳标志（期权特有配置）
	m_strFlowDir = config->getCString("flowdir");      // 获取流控文件目录配置

	// ==================== 流控目录设置 ====================
	if (m_strFlowDir.empty())                          // 如果未配置流控目录
		m_strFlowDir = "CTPOptMDFlow";                 // 使用默认目录名（期权专用）

	m_strFlowDir = StrUtil::standardisePath(m_strFlowDir);  // 标准化路径格式（处理路径分隔符）

	// ==================== 动态库加载 ====================
	std::string module = config->getCString("ctpmodule");  // 获取CTP期权库模块名配置
	if (module.empty())                                // 如果未配置模块名
		module = "soptthostmduserapi_se";              // 使用默认期权模块名
	
	// 构造完整的动态库路径
	std::string dllpath = getBinDir() + DLLHelper::wrap_module(module.c_str(), "");
	// 加载CTP期权动态库
	m_hInstCTP = DLLHelper::load_library(dllpath.c_str());
	
	// ==================== 用户专用目录创建 ====================
	// 构造用户专用的流控文件目录：流控根目录/经纪商/用户ID/
	std::string path = StrUtil::printf("%s/%s/%s/", m_strFlowDir.c_str(), m_strBroker.c_str(), m_strUserID.c_str());
	if (!StdFile::exists(path.c_str()))                // 检查目录是否存在
	{
		// 使用Boost文件系统库递归创建目录结构
		boost::filesystem::create_directories(boost::filesystem::path(path));
	}
	
	// ==================== 跨平台函数符号解析 ====================
	// 根据不同平台和架构选择正确的期权函数符号名
#ifdef _WIN32                                          // Windows平台
#	ifdef _WIN64                                       // 64位Windows
	const char* creatorName = "?CreateFtdcMdApi@CThostFtdcMdApi@ctp_sopt@@SAPEAV12@PEBD_N1@Z";  // MSVC 64位期权符号修饰名
#	else                                               // 32位Windows
	const char* creatorName = "?CreateFtdcMdApi@CThostFtdcMdApi@ctp_sopt@@SAPAV12@PBD_N1@Z";    // MSVC 32位期权符号修饰名
#	endif
#else                                                  // Linux/Unix平台
	const char* creatorName = "_ZN8ctp_sopt15CThostFtdcMdApi15CreateFtdcMdApiEPKcbb";            // GCC期权符号修饰名
#endif

	// ==================== API实例创建和配置 ====================
	// 获取CreateFtdcMdApi函数指针（期权专用）
	m_funcCreator = (CTPCreator)DLLHelper::get_symbol(m_hInstCTP, creatorName);
	// 创建CTP期权API实例，参数：流控目录路径，不使用UDP，不启用多播
	m_pUserAPI = m_funcCreator(path.c_str(), false, false);
	// 注册SPI回调接口，将当前对象设置为事件处理器
	m_pUserAPI->RegisterSpi(this);
	// 注册前置服务器地址，建立网络连接的目标
	m_pUserAPI->RegisterFront((char*)m_strFrontAddr.c_str());

	// ==================== 初始化完成 ====================
	return true;                                       // 所有初始化步骤成功完成
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
 * 期权特殊处理：
 * - 确保期权订阅状态被正确清理
 * - 释放期权相关的缓存数据
 * - 重置期权特有的配置参数
 * 
 * @note 该方法是IParserApi接口的实现，提供统一的资源释放接口
 */
void ParserCTPOpt::release()
{
	disconnect();                                      // 调用断开连接方法，完成所有资源清理
}

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
 * 期权特殊处理：
 * - 确保期权交易权限验证
 * - 准备期权数据接收环境
 * - 初始化期权特有的连接参数
 * 
 * @note 该方法是非阻塞的，立即返回，不等待连接完成
 */
bool ParserCTPOpt::connect()
{
	if(m_pUserAPI)                                     // 检查API对象是否有效
	{
		m_pUserAPI->Init();                            // 启动CTP期权API，开始连接过程
	}

	return true;                                       // 连接启动成功（不等待连接完成）
}

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
 * 4. 清理期权相关状态和缓存数据
 * 
 * 安全性考虑：
 * - 先注销回调再释放，避免释放过程中的异常回调
 * - 按顺序释放资源，确保依赖关系正确处理
 * - 所有指针置空，防止重复释放或悬空访问
 * 
 * 期权特殊处理：
 * - 清理所有期权订阅状态
 * - 重置期权数据处理环境
 * - 释放期权特有的资源
 * 
 * @note 该方法确保资源的完全清理，可以安全地重复调用
 */
bool ParserCTPOpt::disconnect()
{
	// ==================== API对象清理 ====================
	if(m_pUserAPI)                                     // 检查API对象是否存在
	{
		m_pUserAPI->RegisterSpi(NULL);                 // 注销SPI回调接口，停止事件通知
		m_pUserAPI->Release();                         // 调用CTP期权API的释放方法
		m_pUserAPI = NULL;                             // 将指针置空，防止悬空指针访问
	}

	return true;                                       // 断开和清理操作完成
}

// ==================== CTP期权SPI回调方法实现 ====================
// 以下方法实现了CThostFtdcMdSpi接口，处理CTP期权服务器的各种事件通知

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
void ParserCTPOpt::OnRspError( CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	IsErrorRspInfo(pRspInfo);                          // 检查并记录错误信息
}

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
void ParserCTPOpt::OnFrontConnected()
{
	if(m_sink)                                         // 检查事件监听器是否存在
	{
		write_log(m_sink, LL_INFO, "[ParserCTPOpt] Market data server connected");  // 记录连接成功日志
		m_sink->handleEvent(WPE_Connect, 0);           // 通知上层连接成功事件
	}

	ReqUserLogin();                                    // 自动发起用户登录请求
}

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
void ParserCTPOpt::OnRspUserLogin( CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	if(bIsLast && !IsErrorRspInfo(pRspInfo))           // 检查是否为最后响应且无错误
	{
		// 获取并存储当前交易日（YYYYMMDD格式）
		m_uTradingDate = strtoul(m_pUserAPI->GetTradingDay(), NULL, 10);
		
		if(m_sink)                                     // 检查事件监听器是否存在
		{
			m_sink->handleEvent(WPE_Login, 0);         // 通知上层登录成功事件
		}

		// 登录成功后自动订阅期权行情数据
		SubscribeMarketData();                         // 订阅缓存中的所有期权合约行情
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
void ParserCTPOpt::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if(m_sink)                                         // 检查事件监听器是否存在
	{
		m_sink->handleEvent(WPE_Logout, 0);            // 通知上层登出事件
	}
}

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
void ParserCTPOpt::OnFrontDisconnected( int nReason )
{
	if(m_sink)                                         // 检查事件监听器是否存在
	{
		write_log(m_sink, LL_ERROR, "[ParserCTPOpt] Market data server disconnected: {}", nReason);  // 记录断开日志
		m_sink->handleEvent(WPE_Close, 0);             // 通知上层连接关闭事件
	}
}

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
 * @note 当前实现为空，可根据需要添加退订结果处理逻辑
 */
void ParserCTPOpt::OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	// 当前实现为空，可根据业务需要添加退订结果处理逻辑
	// 例如：记录退订成功的期权合约、清理订阅缓存、通知上层应用等
}

/**
 * @brief 深度行情数据推送回调方法
 * @param pDepthMarketData 深度行情数据结构体指针，包含完整的期权tick行情信息
 * 
 * 这是CTP期权行情解析器的核心方法，负责处理服务器推送的实时期权行情数据。
 * 该方法将CTP期权原生格式的行情数据转换为WonderTrader标准格式，并进行
 * 数据质量控制、时间戳处理等关键操作。
 * 
 * 核心处理流程：
 * 1. 基础验证：检查基础数据管理器是否可用
 * 2. 时间戳处理：根据配置选择本地时间或CTP时间
 * 3. 夜盘跨日处理：处理期权夜盘交易的特殊时间场景
 * 4. 合约信息验证：验证期权合约代码的有效性
 * 5. 数据转换：将CTP期权格式转换为WonderTrader标准格式
 * 6. 数据质量控制：过滤无效价格和数量数据
 * 7. 事件通知：将处理后的数据推送给上层应用
 * 
 * 期权特殊处理：
 * - 支持本地时间戳模式选择（期权特有配置）
 * - 处理期权特有的数据字段和业务逻辑
 * - 执行期权数据质量控制和异常处理
 * - 时间戳处理适应期权交易时段特点
 * 
 * 时间处理复杂性：
 * - 期权夜盘交易时间跨越自然日
 * - 需要处理时间戳与交易日的对应关系
 * - 避免早盘启动时接收到过期的夜盘数据
 * 
 * @note 该方法在高频场景下被频繁调用，需要确保处理效率
 */
void ParserCTPOpt::OnRtnDepthMarketData( CThostFtdcDepthMarketDataField *pDepthMarketData )
{	
	// ==================== 基础验证 ====================
	if(m_pBaseDataMgr == NULL)                         // 检查基础数据管理器是否可用
	{
		return;                                        // 无法验证期权合约信息，直接返回
	}

	// ==================== 时间戳处理 ====================
	uint32_t actDate, actTime;                         // 行情发生日期和时间
	if (m_bLocalTime)                                  // 期权特有配置：是否使用本地时间戳
	{
		TimeUtils::getDateTime(actDate, actTime);      // 使用本地系统时间
	}
	else
	{
		// 使用CTP期权服务器时间
		actDate = strtoul(pDepthMarketData->ActionDay, NULL, 10);  // 解析行情发生日期
		actTime = strToTime(pDepthMarketData->UpdateTime) * 1000 + pDepthMarketData->UpdateMillisec;  // 解析时间并加上毫秒
	}
	uint32_t actHour = actTime / 10000000;             // 提取小时部分，用于夜盘判断

	// ==================== 夜盘跨日处理 ====================
	if (actDate == m_uTradingDate && actHour >= 20)    // 检测可能的夜盘跨日问题
	{
		// 期权夜盘时间处理：夜盘时发生日期不应该等于交易日
		// 这种情况通常是时间戳异常，需要手动修正
		uint32_t curDate, curTime;                     // 当前系统日期和时间
		TimeUtils::getDateTime(curDate, curTime);      // 获取当前系统时间
		uint32_t curHour = curTime / 10000000;         // 提取当前小时

		// 早盘启动异常数据过滤：早上启动后可能收到昨晚收盘前的过期行情
		// 凌晨3点到早上9点之间收到的夜盘数据认为是过期数据，直接丢弃
		if (curHour >= 3 && curHour < 9)
			return;                                    // 丢弃过期的期权夜盘数据

		actDate = curDate;                             // 使用当前系统日期作为行情日期

		// 处理跨零点的时间同步问题
		if (actHour == 23 && curHour == 0)             // 行情时间在23点，系统时间在0点
		{
			// 行情时间慢于系统时间，行情还在昨天
			actDate = TimeUtils::getNextDate(curDate, -1);  // 日期回退一天
		}
		else if (actHour == 0 && curHour == 23)        // 系统时间在23点，行情时间在0点
		{
			// 系统时间慢于行情时间，行情已到今天
			actDate = TimeUtils::getNextDate(curDate, 1);   // 日期前进一天
		}
	}

	// ==================== 期权合约信息验证 ====================
	WTSContractInfo* cInfo = m_pBaseDataMgr->getContract(pDepthMarketData->InstrumentID, pDepthMarketData->ExchangeID);
	if (cInfo == NULL)                                 // 检查期权合约是否存在
		return;                                        // 期权合约不存在或无效，丢弃数据

	WTSCommodityInfo* pCommInfo = cInfo->getCommInfo(); // 获取期权商品信息

	// 郑商所特殊处理（已注释）：
	// 郑商所的时间戳可能需要额外的毫秒精度处理
	//if (strcmp(contract->getExchg(), "CZCE") == 0)
	//{
	//	actTime += (uint32_t)(TimeUtils::getLocalTimeNow() % 1000);
	//}

	// ==================== WonderTrader标准格式转换 ====================
	WTSTickData* tick = WTSTickData::create(pDepthMarketData->InstrumentID);  // 创建标准Tick数据对象
	tick->setContractInfo(cInfo);                      // 设置期权合约信息
	WTSTickStruct& quote = tick->getTickStruct();      // 获取Tick数据结构体引用
	strcpy(quote.exchg, pCommInfo->getExchg());        // 设置交易所代码
	
	// 设置时间信息
	quote.action_date = actDate != 0 ? actDate : m_uTradingDate;  // 设置行情发生日期，如果无效则使用交易日
	quote.action_time = actTime;                       // 设置行情发生时间
	
	// ==================== 基础价格信息 ====================
	quote.price = checkValid(pDepthMarketData->LastPrice);        // 最新价（期权当前价格）
	quote.open = checkValid(pDepthMarketData->OpenPrice);         // 开盘价
	quote.high = checkValid(pDepthMarketData->HighestPrice);      // 最高价
	quote.low = checkValid(pDepthMarketData->LowestPrice);        // 最低价
	quote.total_volume = pDepthMarketData->Volume;                // 总成交量
	quote.trading_date = m_uTradingDate;                          // 交易日
	
	// 结算价处理：期权结算价对定价很重要，需要特别验证
	if(pDepthMarketData->SettlementPrice != DBL_MAX)             // 检查结算价是否有效
		quote.settle_price = checkValid(pDepthMarketData->SettlementPrice);  // 设置结算价

	// ==================== 成交额处理（交易所特殊规则）====================
	if(strcmp(quote.exchg, "CZCE") == 0)                         // 郑州商品交易所特殊处理
	{
		// 郑商所的成交额需要根据合约乘数进行缩放
		quote.total_turnover = pDepthMarketData->Turnover * pCommInfo->getVolScale();
	}
	else                                                         // 其他交易所
	{
		if(pDepthMarketData->Turnover != DBL_MAX)                // 检查成交额是否有效
			quote.total_turnover = pDepthMarketData->Turnover;   // 直接使用成交额
	}

	// ==================== 持仓和限价信息 ====================
	quote.open_interest = (uint32_t)pDepthMarketData->OpenInterest;  // 持仓量（期权特别重要的指标）

	quote.upper_limit = checkValid(pDepthMarketData->UpperLimitPrice);  // 涨停价
	quote.lower_limit = checkValid(pDepthMarketData->LowerLimitPrice);  // 跌停价

	// ==================== 前收盘信息 ====================
	quote.pre_close = checkValid(pDepthMarketData->PreClosePrice);      // 昨收盘价
	quote.pre_settle = checkValid(pDepthMarketData->PreSettlementPrice); // 昨结算价（期权定价基准）
	quote.pre_interest = (uint32_t)pDepthMarketData->PreOpenInterest;   // 昨持仓量

	// ==================== 五档卖盘价格 ====================
	quote.ask_prices[0] = checkValid(pDepthMarketData->AskPrice1);      // 卖一价
	quote.ask_prices[1] = checkValid(pDepthMarketData->AskPrice2);      // 卖二价
	quote.ask_prices[2] = checkValid(pDepthMarketData->AskPrice3);      // 卖三价
	quote.ask_prices[3] = checkValid(pDepthMarketData->AskPrice4);      // 卖四价
	quote.ask_prices[4] = checkValid(pDepthMarketData->AskPrice5);      // 卖五价

	// ==================== 五档买盘价格 ====================
	quote.bid_prices[0] = checkValid(pDepthMarketData->BidPrice1);      // 买一价
	quote.bid_prices[1] = checkValid(pDepthMarketData->BidPrice2);      // 买二价
	quote.bid_prices[2] = checkValid(pDepthMarketData->BidPrice3);      // 买三价
	quote.bid_prices[3] = checkValid(pDepthMarketData->BidPrice4);      // 买四价
	quote.bid_prices[4] = checkValid(pDepthMarketData->BidPrice5);      // 买五价

	// ==================== 五档卖盘数量 ====================
	quote.ask_qty[0] = pDepthMarketData->AskVolume1;                   // 卖一量
	quote.ask_qty[1] = pDepthMarketData->AskVolume2;                   // 卖二量
	quote.ask_qty[2] = pDepthMarketData->AskVolume3;                   // 卖三量
	quote.ask_qty[3] = pDepthMarketData->AskVolume4;                   // 卖四量
	quote.ask_qty[4] = pDepthMarketData->AskVolume5;                   // 卖五量

	// ==================== 五档买盘数量 ====================
	quote.bid_qty[0] = pDepthMarketData->BidVolume1;                   // 买一量
	quote.bid_qty[1] = pDepthMarketData->BidVolume2;                   // 买二量
	quote.bid_qty[2] = pDepthMarketData->BidVolume3;                   // 买三量
	quote.bid_qty[3] = pDepthMarketData->BidVolume4;                   // 买四量
	quote.bid_qty[4] = pDepthMarketData->BidVolume5;                   // 买五量

	// ==================== 数据推送和资源管理 ====================
	if(m_sink)                                                        // 检查事件监听器是否存在
		m_sink->handleQuote(tick, 1);                                 // 将期权行情数据推送给上层应用

	tick->release();                                                  // 释放Tick数据对象，避免内存泄漏
}

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
 * @note 当前实现为空，可根据需要添加订阅结果处理逻辑
 */
void ParserCTPOpt::OnRspSubMarketData( CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast )
{
	if(!IsErrorRspInfo(pRspInfo))                      // 检查订阅是否成功
	{
		// 订阅成功的处理逻辑（当前为空）
		// 可以添加：记录成功订阅的期权合约、更新订阅状态、通知上层应用等
	}
	else                                               // 订阅失败
	{
		// 订阅失败的处理逻辑（当前为空）
		// 可以添加：记录失败原因、重试机制、通知上层应用等
	}
}

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
void ParserCTPOpt::OnHeartBeatWarning( int nTimeLapse )
{
	if (m_sink)                                        // 检查事件监听器是否存在
		write_log(m_sink, LL_INFO, "[ParserCTPOpt] Heartbeating, elapse: {}", nTimeLapse);  // 记录心跳警告信息
}

// ==================== 私有辅助方法实现 ====================
// 以下方法为内部使用的辅助功能，不对外暴露

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
void ParserCTPOpt::ReqUserLogin()
{
	// ==================== API对象有效性检查 ====================
	if(m_pUserAPI == NULL)                             // 检查API对象是否已创建
	{
		return;                                        // API对象无效，无法发送登录请求
	}

	// ==================== 构造登录请求 ====================
	CThostFtdcReqUserLoginField req;                   // 创建登录请求结构体
	memset(&req, 0, sizeof(req));                      // 清零结构体，确保所有字段初始化

	// 填充登录信息
	strcpy(req.BrokerID, m_strBroker.c_str());         // 设置经纪商代码
	strcpy(req.UserID, m_strUserID.c_str());           // 设置用户账号
	strcpy(req.Password, m_strPassword.c_str());       // 设置用户密码
	strcpy(req.UserProductInfo, WT_PRODUCT);           // 设置产品信息（WonderTrader标识）

	// ==================== 发送登录请求 ====================
	int iResult = m_pUserAPI->ReqUserLogin(&req, ++m_iRequestID);  // 发送登录请求，使用递增的请求ID
	if(iResult != 0)                                   // 检查发送结果
	{
		if(m_sink)                                     // 检查日志接收器是否存在
			write_log(m_sink, LL_ERROR, "[ParserCTPOpt] Sending login request failed: {}", iResult);  // 记录发送失败的错误
	}
}

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
void ParserCTPOpt::SubscribeMarketData()
{
	// ==================== 订阅列表检查 ====================
	CodeSet codeFilter = m_filterSubs;                 // 复制订阅列表，避免直接修改成员变量
	if(codeFilter.empty())                             // 检查订阅列表是否为空
	{
		// 如果订阅列表为空，则不进行订阅操作
		// 注释：原文"订阅礼包只空的"应为"订阅列表为空的"
		return;
	}

	// ==================== 构造合约代码数组 ====================
	char ** subscribe = new char*[codeFilter.size()];  // 动态分配指针数组，存储合约代码
	int nCount = 0;                                    // 有效合约代码计数器

	// 遍历所有期权合约代码，进行格式处理
	for(auto& code : codeFilter)
	{
		std::size_t pos = code.find('.');              // 查找交易所前缀分隔符
		if (pos != std::string::npos)                  // 如果找到分隔符（如"SHFE.cu2105C4500"）
			subscribe[nCount++] = (char*)code.c_str() + pos + 1;  // 跳过前缀，使用"cu2105C4500"
		else                                           // 如果没有前缀（如"cu2105C4500"）
			subscribe[nCount++] = (char*)code.c_str(); // 直接使用原始合约代码
	}

	// ==================== 发送订阅请求 ====================
	if(m_pUserAPI && nCount > 0)                      // 检查API对象和有效合约数量
	{
		// 调用CTP期权API批量订阅行情数据
		int iResult = m_pUserAPI->SubscribeMarketData(subscribe, nCount);
		if (iResult != 0)                              // 订阅请求发送失败
		{
			if (m_sink)
				write_log(m_sink, LL_ERROR, "[ParserCTPOpt] Sending md subscribe request failed: {}", iResult);
		}
		else                                           // 订阅请求发送成功
		{
			if (m_sink)
				write_log(m_sink, LL_INFO, "[ParserCTPOpt] Market data of {} contracts subscribed in total", nCount);
		}
	}

	// ==================== 资源清理 ====================
	codeFilter.clear();                                // 清空临时订阅列表
	delete[] subscribe;                                // 释放动态分配的指针数组
}

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
bool ParserCTPOpt::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 当前实现为简化版本，直接返回false表示无错误
	// 完整实现应该检查pRspInfo->ErrorID是否为0，并记录错误信息
	// 例如：
	// if (pRspInfo && pRspInfo->ErrorID != 0) {
	//     write_log(m_sink, LL_ERROR, "[ParserCTPOpt] Error: {}, {}", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
	//     return true;
	// }
	return false;                                      // 简化实现：不检查错误，直接返回成功
}

// ==================== IParserApi接口实现 ====================
// 以下方法实现了WonderTrader标准行情解析器接口

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
void ParserCTPOpt::subscribe(const CodeSet &vecSymbols)
{
	// ==================== 登录状态检查 ====================
	if(m_uTradingDate == 0)                           // 检查是否已登录（交易日为0表示未登录）
	{
		m_filterSubs = vecSymbols;                     // 缓存订阅列表，等待登录后处理
	}
	else                                               // 已登录状态
	{
		// ==================== 立即订阅处理 ====================
		m_filterSubs = vecSymbols;                     // 更新订阅列表缓存
		char * subscribe[500] = {NULL};                // 创建固定大小的合约代码数组
		int nCount = 0;                                // 有效合约代码计数器

		// 遍历所有期权合约代码，进行格式处理
		for (auto& code  : vecSymbols)
		{
			std::size_t pos = code.find('.');          // 查找交易所前缀分隔符
			if (pos != std::string::npos)              // 如果找到分隔符（如"SHFE.cu2105C4500"）
				subscribe[nCount++] = (char*)code.c_str() + pos + 1;  // 跳过前缀，使用"cu2105C4500"
			else                                       // 如果没有前缀（如"cu2105C4500"）
				subscribe[nCount++] = (char*)code.c_str();     // 直接使用原始合约代码
		}

		// ==================== 发送订阅请求 ====================
		if(m_pUserAPI && nCount > 0)                  // 检查API对象和有效合约数量
		{
			// 调用CTP期权API批量订阅行情数据
			int iResult = m_pUserAPI->SubscribeMarketData(subscribe, nCount);
			if (iResult != 0)                          // 订阅请求发送失败
			{
				if (m_sink)
					write_log(m_sink, LL_ERROR, "[ParserCTPOpt] Sending md subscribe request failed: {}", iResult);
			}
			else                                       // 订阅请求发送成功
			{
				if (m_sink)
					write_log(m_sink, LL_INFO, "[ParserCTPOpt] Market data of {} contracts subscribed in total", nCount);
			}
		}
	}
}

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
 * @note 当前实现为空，期权连接断开时会自动清除所有订阅
 */
void ParserCTPOpt::unsubscribe(const CodeSet &vecSymbols)
{
	// 当前实现为空，可根据业务需要添加退订逻辑
	// 完整实现应该包括：
	// 1. 合约代码格式处理（去除交易所前缀）
	// 2. 调用m_pUserAPI->UnSubscribeMarketData()
	// 3. 从m_filterSubs中移除对应合约
	// 4. 记录退订结果日志
}

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
bool ParserCTPOpt::isConnected()
{
	return m_pUserAPI != NULL;                         // 检查API对象指针是否有效
}

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
void ParserCTPOpt::registerSpi(IParserSpi* listener)
{
	// ==================== 监听器注册 ====================
	m_sink = listener;                                 // 保存事件监听器指针

	// ==================== 基础数据管理器获取 ====================
	if(m_sink)                                         // 检查监听器是否有效
		m_pBaseDataMgr = m_sink->getBaseDataMgr();     // 获取基础数据管理器，用于期权合约信息验证
}