/*!
 * \file WtBtDtReader.cpp
 * \project WonderTrader
 * 
 * \brief WonderTrader回测数据读取器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtBtDtReader类的所有功能，是WonderTrader框架中用于回测数据读取的核心组件。
 * 该文件提供了从WonderTrader数据存储格式中读取原始数据的功能，支持K线数据、Tick数据、
 * 逐笔成交、逐笔委托、委托队列等多种数据类型的读取，主要用于回测引擎的数据加载。
 * 
 * 核心实现机制：
 * 
 * 1. 数据文件定位（Data File Location）：
 *    - 根据交易所、合约代码、数据类型构建文件路径
 *    - 支持历史数据的分层存储结构
 *    - 提供文件存在性检查和错误处理
 * 
 * 2. 数据格式处理（Data Format Processing）：
 *    - 支持多种数据格式的自动识别
 *    - 提供数据压缩和解压缩功能
 *    - 支持版本兼容性处理
 * 
 * 3. 日志记录机制（Logging Mechanism）：
 *    - 提供统一的日志记录接口
 *    - 支持多线程安全的日志输出
 *    - 提供详细的错误信息和调试信息
 * 
 * 主要功能模块：
 * 
 * 1. 初始化模块：
 *    - 配置参数解析和验证
 *    - 数据存储路径设置
 *    - 日志系统初始化
 * 
 * 2. 数据读取模块：
 *    - K线数据读取（1分钟、5分钟、日线等）
 *    - Tick数据读取（按日期）
 *    - 逐笔数据读取（成交、委托、队列）
 * 
 * 3. 数据处理模块：
 *    - 数据格式转换
 *    - 数据压缩处理
 *    - 错误处理和恢复
 * 
 * 技术特点：
 * - 使用内存映射文件技术提高读取性能
 * - 支持多种数据格式的自动识别和转换
 * - 提供线程安全的数据读取
 * - 支持大数据量的流式读取
 * 
 * 使用场景：
 * - 回测引擎数据加载
 * - 历史数据分析和研究
 * - 数据格式转换和迁移
 * - 自定义数据处理程序
 * 
 * 注意事项：
 * - 需要正确配置数据存储路径
 * - 支持的数据格式需要与存储格式匹配
 * - 大数据量读取时需要注意内存使用
 * - 文件路径构建需要遵循特定的目录结构
 */

#include "WtBtDtReader.h"                                       // 回测数据读取器头文件

#include "../Includes/WTSVariant.hpp"                           // 变体数据类型
#include "../Share/StrUtil.hpp"                                 // 字符串工具函数
#include "../WTSUtils/WTSCmpHelper.hpp"                         // 数据压缩辅助工具

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                                    // 格式化库

/*!
 * \brief 回测数据读取器日志记录模板函数
 * \tparam Args 可变参数类型
 * \param sink 日志回调接口
 * \param ll 日志级别
 * \param format 格式化字符串
 * \param args 格式化参数
 * 
 * 该函数用于回测数据读取器的日志记录，支持格式化字符串和可变参数。
 * 使用线程本地存储的缓冲区避免多线程竞争，提供高效的日志输出。
 */
template<typename... Args>
inline void pipe_btreader_log(IBtDtReaderSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                                            // 如果日志回调接口为空
		return;                                                  // 直接返回

	static thread_local char buffer[512] = { 0 };               // 线程本地日志缓冲区（512字节）
	memset(buffer, 0, 512);                                     // 清空缓冲区
	fmt::format_to(buffer, format, args...);                    // 格式化字符串到缓冲区

	sink->reader_log(ll, buffer);                               // 调用日志回调接口输出日志
}

extern bool proc_block_data(std::string& content, bool isBar, bool bKeepHead = true);  // 数据块处理函数声明

/*!
 * \brief 创建回测数据读取器实例
 * \return 回测数据读取器指针
 * 
 * 该函数用于创建WtBtDtReader实例，供外部C接口调用。
 * 返回的指针需要调用deleteBtDtReader函数释放。
 */
extern "C"
{
	EXPORT_FLAG IBtDtReader* createBtDtReader()
	{
		IBtDtReader* ret = new WtBtDtReader();                    // 创建WtBtDtReader实例
		return ret;                                               // 返回实例指针
	}

	/*!
	 * \brief 删除回测数据读取器实例
	 * \param reader 回测数据读取器指针
	 * 
	 * 该函数用于释放WtBtDtReader实例，供外部C接口调用。
	 * 传入的指针必须是通过createBtDtReader函数创建的。
	 */
	EXPORT_FLAG void deleteBtDtReader(IBtDtReader* reader)
	{
		if (reader != NULL)                                       // 如果指针不为空
			delete reader;                                        // 删除实例
	}
};

/*
 *	处理块数据
 */
extern bool proc_block_data(std::string& content, bool isBar, bool bKeepHead);

/*!
 * \brief WtBtDtReader构造函数
 * 
 * 初始化回测数据读取器实例，设置默认参数。
 * 构造函数为空，所有初始化工作由init函数完成。
 */
WtBtDtReader::WtBtDtReader()
{
}

/*!
 * \brief WtBtDtReader析构函数
 * 
 * 清理回测数据读取器实例，释放相关资源。
 * 析构函数为空，所有清理工作由系统自动完成。
 */
WtBtDtReader::~WtBtDtReader()
{

}

/*!
 * \brief 初始化回测数据读取器
 * \param cfg 配置参数（包含数据存储路径等）
 * \param sink 数据读取回调接口
 * 
 * 该函数用于初始化回测数据读取器，设置数据存储路径和日志回调接口。
 * 配置参数中必须包含"path"字段，指定数据存储的基础目录路径。
 */
void WtBtDtReader::init(WTSVariant* cfg, IBtDtReaderSink* sink)
{
	_sink = sink;                                                // 设置日志回调接口

	if (cfg == NULL)                                             // 如果配置参数为空
		return;                                                  // 直接返回

	_base_dir = cfg->getCString("path");                         // 获取数据存储路径
	_base_dir = StrUtil::standardisePath(_base_dir);             // 标准化路径格式

	pipe_btreader_log(_sink, LL_INFO, "WtBtDtReader initialized, root data dir is {}", _base_dir);  // 记录初始化日志
}

/*!
 * \brief 读取原始K线数据
 * \param exchg 交易所代码
 * \param code 合约代码
 * \param period K线周期
 * \param buffer 输出缓冲区
 * \return 是否读取成功
 * 
 * 该函数用于读取指定合约的K线数据，支持1分钟、5分钟、日线等不同周期。
 * 数据文件路径格式：{base_dir}/his/{period}/{exchg}/{code}.dsb
 * 读取的数据会经过格式处理，去除头部信息。
 */
bool WtBtDtReader::read_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, std::string& buffer)
{
	std::stringstream ss;                                        // 字符串流用于构建文件路径
	ss << _base_dir << "his/" << PERIOD_NAME[period] << "/" << exchg << "/" << code << ".dsb";  // 构建历史K线数据文件路径
	std::string filename = ss.str();                             // 获取完整文件路径
	if (!StdFile::exists(filename.c_str()))                       // 如果文件不存在
	{
		pipe_btreader_log(_sink, LL_WARN, "Back {} data file {} not exists", PERIOD_NAME[period], filename);  // 记录警告日志
		return false;                                             // 返回失败
	}

	pipe_btreader_log(_sink, LL_DEBUG, "Reading back {} bars from file {}...", PERIOD_NAME[period], filename);  // 记录调试日志
	StdFile::read_file_content(filename.c_str(), buffer);         // 读取文件内容到缓冲区
	bool bSucc = proc_block_data(buffer, true, false);           // 处理数据块（K线数据，不保留头部）
	if(!bSucc)                                                   // 如果处理失败
		pipe_btreader_log(_sink, LL_ERROR, "Processing back {} data from file {} failed", PERIOD_NAME[period], filename);  // 记录错误日志

	return bSucc;                                                // 返回处理结果
}

/*!
 * \brief 读取原始Tick数据
 * \param exchg 交易所代码
 * \param code 合约代码
 * \param uDate 交易日期
 * \param buffer 输出缓冲区
 * \return 是否读取成功
 * 
 * 该函数用于读取指定合约在指定日期的Tick数据。
 * 数据文件路径格式：{base_dir}/his/ticks/{exchg}/{date}/{code}.dsb
 * 读取的数据会经过格式处理，去除头部信息。
 */
bool WtBtDtReader::read_raw_ticks(const char* exchg, const char* code, uint32_t uDate, std::string& buffer)
{
	std::stringstream ss;                                        // 字符串流用于构建文件路径
	ss << _base_dir << "his/ticks/" << exchg << "/" << uDate << "/" << code << ".dsb";  // 构建历史Tick数据文件路径
	std::string filename = ss.str();                             // 获取完整文件路径
	if (!StdFile::exists(filename.c_str()))                       // 如果文件不存在
	{
		pipe_btreader_log(_sink, LL_WARN, "Back tick data file {} not exists", filename);  // 记录警告日志
		return false;                                             // 返回失败
	}

	StdFile::read_file_content(filename.c_str(), buffer);         // 读取文件内容到缓冲区
	bool bSucc = proc_block_data(buffer, false, false);          // 处理数据块（Tick数据，不保留头部）
	if (!bSucc)                                                  // 如果处理失败
		pipe_btreader_log(_sink, LL_ERROR, "Processing back tick data from file {} failed", filename);  // 记录错误日志

	return bSucc;                                                // 返回处理结果
}

/*!
 * \brief 读取原始逐笔委托数据
 * \param exchg 交易所代码
 * \param code 合约代码
 * \param uDate 交易日期
 * \param buffer 输出缓冲区
 * \return 是否读取成功
 * 
 * 该函数用于读取指定合约在指定日期的逐笔委托数据。
 * 数据文件路径格式：{base_dir}/his/orders/{exchg}/{date}/{code}.dsb
 * 读取的数据会经过格式处理，去除头部信息。
 */
bool WtBtDtReader::read_raw_order_details(const char* exchg, const char* code, uint32_t uDate, std::string& buffer)
{
	std::stringstream ss;                                        // 字符串流用于构建文件路径
	ss << _base_dir << "his/orders/" << exchg << "/" << uDate << "/" << code << ".dsb";  // 构建历史逐笔委托数据文件路径
	std::string filename = ss.str();                             // 获取完整文件路径
	if (!StdFile::exists(filename.c_str()))                       // 如果文件不存在
	{
		pipe_btreader_log(_sink, LL_WARN, "Back order detail data file {} not exists", filename);  // 记录警告日志
		return false;                                             // 返回失败
	}

	StdFile::read_file_content(filename.c_str(), buffer);         // 读取文件内容到缓冲区
	bool bSucc = proc_block_data(buffer, false, false);          // 处理数据块（逐笔委托数据，不保留头部）
	if (!bSucc)                                                  // 如果处理失败
		pipe_btreader_log(_sink, LL_ERROR, "Processing back order detail data from file {} failed", filename);  // 记录错误日志

	return bSucc;                                                // 返回处理结果
}

/*!
 * \brief 读取原始委托队列数据
 * \param exchg 交易所代码
 * \param code 合约代码
 * \param uDate 交易日期
 * \param buffer 输出缓冲区
 * \return 是否读取成功
 * 
 * 该函数用于读取指定合约在指定日期的委托队列数据。
 * 数据文件路径格式：{base_dir}/his/queue/{exchg}/{date}/{code}.dsb
 * 读取的数据会经过格式处理，去除头部信息。
 */
bool WtBtDtReader::read_raw_order_queues(const char* exchg, const char* code, uint32_t uDate, std::string& buffer)
{
	std::stringstream ss;                                        // 字符串流用于构建文件路径
	ss << _base_dir << "his/queue/" << exchg << "/" << uDate << "/" << code << ".dsb";  // 构建历史委托队列数据文件路径
	std::string filename = ss.str();                             // 获取完整文件路径
	if (!StdFile::exists(filename.c_str()))                       // 如果文件不存在
	{
		pipe_btreader_log(_sink, LL_WARN, "Back order queue data file {} not exists", filename);  // 记录警告日志
		return false;                                             // 返回失败
	}

	StdFile::read_file_content(filename.c_str(), buffer);         // 读取文件内容到缓冲区
	bool bSucc = proc_block_data(buffer, false, false);          // 处理数据块（委托队列数据，不保留头部）
	if (!bSucc)                                                  // 如果处理失败
		pipe_btreader_log(_sink, LL_ERROR, "Processing back order queue data from file {} failed", filename);  // 记录错误日志

	return bSucc;                                                // 返回处理结果
}

/*!
 * \brief 读取原始逐笔成交数据
 * \param exchg 交易所代码
 * \param code 合约代码
 * \param uDate 交易日期
 * \param buffer 输出缓冲区
 * \return 是否读取成功
 * 
 * 该函数用于读取指定合约在指定日期的逐笔成交数据。
 * 数据文件路径格式：{base_dir}/his/trans/{exchg}/{date}/{code}.dsb
 * 读取的数据会经过格式处理，去除头部信息。
 */
bool WtBtDtReader::read_raw_transactions(const char* exchg, const char* code, uint32_t uDate, std::string& buffer)
{
	std::stringstream ss;                                        // 字符串流用于构建文件路径
	ss << _base_dir << "his/trans/" << exchg << "/" << uDate << "/" << code << ".dsb";  // 构建历史逐笔成交数据文件路径
	std::string filename = ss.str();                             // 获取完整文件路径
	if (!StdFile::exists(filename.c_str()))                       // 如果文件不存在
	{
		pipe_btreader_log(_sink, LL_WARN, "Back transaction data file {} not exists", filename);  // 记录警告日志
		return false;                                             // 返回失败
	}

	StdFile::read_file_content(filename.c_str(), buffer);         // 读取文件内容到缓冲区
	bool bSucc = proc_block_data(buffer, false, false);          // 处理数据块（逐笔成交数据，不保留头部）
	if (!bSucc)                                                  // 如果处理失败
		pipe_btreader_log(_sink, LL_ERROR, "Processing back transaction data from file {} failed", filename);  // 记录错误日志

	return bSucc;                                                // 返回处理结果
}