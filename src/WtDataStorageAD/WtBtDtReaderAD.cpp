/*!
 * \file WtBtDtReaderAD.cpp
 * \project WonderTrader
 *
 * \author Wesley
 * \date 2022.01.05
 * 
 * \brief WtDataStorageAD模块回测数据读取器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了WtBtDtReaderAD类的具体功能，为WonderTrader回测引擎提供基于LMDB
 * 数据库的高性能历史数据读取服务。该实现专门针对回测场景进行了优化，支持
 * 大批量历史数据的快速加载和原始格式数据的直接访问。
 * 
 * 核心实现特点：
 * 
 * 1. 原始数据直接访问（Raw Data Direct Access）：
 *    - 避免数据格式转换开销
 *    - 直接返回二进制数据缓冲区
 *    - 最大化回测性能
 * 
 * 2. LMDB高效查询（Efficient LMDB Queries）：
 *    - 使用范围查询获取时间序列数据
 *    - 零拷贝内存访问
 *    - 批量数据传输
 * 
 * 3. 动态数据库管理（Dynamic Database Management）：
 *    - 按需打开数据库连接
 *    - 智能缓存数据库句柄
 *    - 自动资源管理
 * 
 * 实现架构：
 * 
 * 1. C接口导出（C Interface Export）：
 *    - createBtDtReader()：创建读取器实例
 *    - deleteBtDtReader()：销毁读取器实例
 *    - 支持动态库加载和跨语言调用
 * 
 * 2. 日志系统集成（Logging System Integration）：
 *    - pipe_btreader_log()：统一日志输出
 *    - 线程局部缓冲区优化
 *    - 格式化字符串支持
 * 
 * 3. 数据读取优化（Data Reading Optimization）：
 *    - 大端序键值确保正确排序
 *    - 批量内存拷贝
 *    - 最小化系统调用
 */

#include "WtBtDtReaderAD.h"                     // 引入回测数据读取器头文件
#include "LMDBKeys.h"                           // 引入LMDB键值结构定义

#include "../Includes/WTSStruct.h"              // 引入WonderTrader数据结构
#include "../Includes/WTSVariant.hpp"           // 引入配置参数类
#include "../Share/StrUtil.hpp"                 // 引入字符串工具类
#include "../Share/StdUtils.hpp"                // 引入标准工具类

USING_NS_WTP;                                   // 使用WonderTrader命名空间

//By Wesley @ 2022.01.05
#include "../Share/fmtlib.h"                    // 引入格式化字符串库

/**
 * @brief 回测读取器日志输出函数
 * 
 * 线程安全的日志输出函数，使用线程局部存储优化性能。
 * 
 * @tparam Args 可变参数类型
 * @param sink 日志输出接口
 * @param ll 日志级别
 * @param format 格式化字符串
 * @param args 格式化参数
 */
template<typename... Args>
inline void pipe_btreader_log(IBtDtReaderSink* sink, WTSLogLevel ll, const char* format, const Args&... args)
{
	if (sink == NULL)                           // 检查接口有效性
		return;

	static thread_local char buffer[512] = { 0 };  // 线程局部缓冲区
	memset(buffer, 0, 512);                     // 清零缓冲区
	fmt::format_to(buffer, format, args...);    // 格式化字符串

	sink->reader_log(ll, buffer);               // 输出日志
}

/**
 * @brief C接口导出函数
 * 
 * 提供标准C接口，支持动态库加载和跨语言调用。
 */
extern "C"
{
	/**
	 * @brief 创建回测数据读取器实例
	 * 
	 * 工厂函数，创建WtBtDtReaderAD实例并返回基类指针。
	 * 
	 * @return 回测数据读取器接口指针
	 */
	EXPORT_FLAG IBtDtReader* createBtDtReader()
	{
		IBtDtReader* ret = new WtBtDtReaderAD(); // 创建实例
		return ret;                             // 返回接口指针
	}

	/**
	 * @brief 销毁回测数据读取器实例
	 * 
	 * 安全销毁读取器实例，释放所有资源。
	 * 
	 * @param reader 要销毁的读取器指针
	 */
	EXPORT_FLAG void deleteBtDtReader(IBtDtReader* reader)
	{
		if (reader != NULL)                     // 检查指针有效性
			delete reader;                      // 销毁实例
	}
};

/**
 * @brief 构造函数
 * 
 * 初始化WtBtDtReaderAD实例，设置默认值。
 */
WtBtDtReaderAD::WtBtDtReaderAD()
{
	// 构造函数中不执行重量级操作
	// 实际初始化在init()方法中完成
}

/**
 * @brief 析构函数
 * 
 * 清理资源，关闭所有数据库连接。
 * LMDB数据库连接由智能指针自动管理。
 */
WtBtDtReaderAD::~WtBtDtReaderAD()
{
	// 智能指针会自动释放LMDB数据库资源
	// 无需手动清理
}

/**
 * @brief 初始化回测数据读取器
 * 
 * 根据配置参数初始化读取器，设置数据存储路径和回调接口。
 * 
 * @param cfg 配置参数对象
 * @param sink 回调接口，用于日志输出
 */
void WtBtDtReaderAD::init(WTSVariant* cfg, IBtDtReaderSink* sink)
{
	_sink = sink;                               // 保存回调接口

	if (cfg == NULL)                            // 检查配置参数有效性
		return;

	_base_dir = cfg->getCString("path");        // 获取数据存储路径
	_base_dir = StrUtil::standardisePath(_base_dir);  // 标准化路径格式

	// 输出初始化成功日志
	pipe_btreader_log(_sink, LL_INFO, "WtBtDtReaderAD initialized, root data dir is {}", _base_dir);
}

/**
 * @brief 读取原始K线数据
 * 
 * 从LMDB数据库中读取指定合约和周期的所有K线数据，以二进制格式返回。
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param period K线周期
 * @param buffer 输出缓冲区，存储原始K线数据
 * @return 读取成功返回true，失败返回false
 */
bool WtBtDtReaderAD::read_raw_bars(const char* exchg, const char* code, WTSKlinePeriod period, std::string& buffer)
{
	// 获取对应的LMDB数据库连接
	WtLMDBPtr db = get_k_db(exchg, period);
	if (db == NULL)                             // 检查数据库连接有效性
		return false;

	// 输出调试日志
	pipe_btreader_log(_sink, LL_DEBUG, "Reading back {} bars of {}.{}...", PERIOD_NAME[period], exchg, code);
	
	WtLMDBQuery query(*db);                     // 创建LMDB查询对象
	
	// 构造查询键值范围（从最小到最大时间）
	LMDBBarKey rKey(exchg, code, 0xffffffff);   // 右边界（最大时间）
	LMDBBarKey lKey(exchg, code, 0);            // 左边界（最小时间）
	
	// 执行范围查询，获取所有K线数据
	int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), 
		std::string((const char*)&rKey, sizeof(rKey)),
		[this, &buffer, &lKey](const ValueArray& ayKeys, const ValueArray& ayVals) {
		if (ayVals.empty())                     // 检查查询结果是否为空
			return;

		std::size_t cnt = ayVals.size();        // 获取数据条数
		auto szUnit = sizeof(WTSBarStruct);     // 单个K线结构体大小
		buffer.resize(szUnit*cnt);              // 调整缓冲区大小
		char* cursor = (char*)buffer.data();    // 获取缓冲区指针
		
		// 逐个拷贝K线数据到缓冲区
		for(const std::string& item : ayVals)
		{
			memcpy(cursor, item.data(), szUnit); // 拷贝K线数据
			cursor += szUnit;                   // 移动指针
		}
	});

	return true;                                // 返回成功
}

/**
 * @brief 读取原始Tick数据
 * 
 * 从LMDB数据库中读取指定合约和日期的所有Tick数据，以二进制格式返回。
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @param uDate 交易日期（格式：YYYYMMDD）
 * @param buffer 输出缓冲区，存储原始Tick数据
 * @return 读取成功返回true，失败返回false
 */
bool WtBtDtReaderAD::read_raw_ticks(const char* exchg, const char* code, uint32_t uDate, std::string& buffer)
{
	// 获取对应的LMDB数据库连接
	WtLMDBPtr db = get_t_db(exchg, code);
	if (db == NULL)                             // 检查数据库连接有效性
		return false;

	// 输出调试日志
	pipe_btreader_log(_sink, LL_DEBUG, "Reading back ticks on {} of {}.{}...", uDate, exchg, code);
	
	WtLMDBQuery query(*db);                     // 创建LMDB查询对象
	
	// 构造查询键值范围（指定日期的全天数据）
	LMDBHftKey rKey(exchg, code, uDate, 240000000);  // 右边界（24:00:00.000）
	LMDBHftKey lKey(exchg, code, uDate, 0);          // 左边界（00:00:00.000）
	
	// 执行范围查询，获取指定日期的所有Tick数据
	int cnt = query.get_range(std::string((const char*)&lKey, sizeof(lKey)), 
		std::string((const char*)&rKey, sizeof(rKey)),
		[this, &buffer, &lKey](const ValueArray& ayKeys, const ValueArray& ayVals) {
		if (ayVals.empty())                     // 检查查询结果是否为空
			ret
		std::size_t cnt = ayVals.size();        // 获取数据条数
		auto szUnit = sizeof(WTSTickStruct);    // 单个Tick结构体大小
		buffer.resize(szUnit*cnt);              // 调整缓冲区大小
		char* cursor = (char*)buffer.data();    // 获取缓冲区指针
		
		// 逐个拷贝Tick数据到缓冲区
		for (const std::string& item : ayVals)
		{
			memcpy(cursor, item.data(), szUnit); // 拷贝Tick数据
			cursor += szUnit;                   // 移动指针
		}
	});

	return true;                                // 返回成功
}

/**
 * @brief 获取K线数据库连接
 * 
 * 根据交易所和K线周期获取对应的LMDB数据库连接。
 * 实现了数据库连接的缓存和按需加载机制。
 * 
 * @param exchg 交易所代码
 * @param period K线周期
 * @return LMDB数据库智能指针，失败返回空指针
 */
WtBtDtReaderAD::WtLMDBPtr WtBtDtReaderAD::get_k_db(const char* exchg, WTSKlinePeriod period)
{
	WtLMDBMap* the_map = NULL;                  // 数据库映射表指针
	std::string subdir;                         // 子目录名称
	
	// 根据K线周期选择对应的数据库映射表和子目录
	if (period == KP_Minute1)
	{
		the_map = &_exchg_m1_dbs;               // 1分钟K线数据库映射表
		subdir = "min1";                        // 1分钟数据子目录
	}
	else if (period == KP_Minute5)
	{
		the_map = &_exchg_m5_dbs;               // 5分钟K线数据库映射表
		subdir = "min5";                        // 5分钟数据子目录
	}
	else if (period == KP_DAY)
	{
		the_map = &_exchg_d1_dbs;               // 日K线数据库映射表
		subdir = "day";                         // 日线数据子目录
	}
	else
		return std::move(WtLMDBPtr());          // 不支持的周期，返回空指针

	// 检查缓存中是否已有该交易所的数据库连接
	auto it = the_map->find(exchg);
	if (it != the_map->end())
		return std::move(it->second);           // 返回缓存的连接

	// 创建新的数据库连接（只读模式）
	WtLMDBPtr dbPtr(new WtLMDB(true));
	std::string path = StrUtil::printf("%s%s/%s/", _base_dir.c_str(), subdir.c_str(), exchg);
	
	// 检查数据库路径是否存在
	if (!StdFile::exists(path.c_str()))
		return std::move(WtLMDBPtr());          // 路径不存在，返回空指针

	// 尝试打开数据库
	if (!dbPtr->open(path.c_str()))
	{
		// 打开失败，记录错误日志
		pipe_btreader_log(_sink, LL_ERROR, "Opening {} db if {} failed: {}", subdir, exchg, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}
	else
	{
		// 打开成功，记录调试日志
		pipe_btreader_log(_sink, LL_DEBUG, "{} db of {} opened", subdir, exchg);
	}

	// 将新连接加入缓存
	(*the_map)[exchg] = dbPtr;
	return std::move(dbPtr);                    // 返回新连接
}

/**
 * @brief 获取Tick数据库连接
 * 
 * 根据交易所和合约代码获取对应的LMDB数据库连接。
 * 实现了数据库连接的缓存和按需加载机制。
 * 
 * @param exchg 交易所代码
 * @param code 合约代码
 * @return LMDB数据库智能指针，失败返回空指针
 */
WtBtDtReaderAD::WtLMDBPtr WtBtDtReaderAD::get_t_db(const char* exchg, const char* code)
{
	// 构造缓存键值（格式："交易所.合约"）
	std::string key = StrUtil::printf("%s.%s", exchg, code);
	
	// 检查缓存中是否已有该合约的数据库连接
	auto it = _tick_dbs.find(key);
	if (it != _tick_dbs.end())
		return std::move(it->second);           // 返回缓存的连接

	// 创建新的数据库连接（只读模式）
	WtLMDBPtr dbPtr(new WtLMDB(true));
	std::string path = StrUtil::printf("%sticks/%s/%s", _base_dir.c_str(), exchg, code);
	
	// 检查数据库路径是否存在
	if (!StdFile::exists(path.c_str()))
		return std::move(WtLMDBPtr());          // 路径不存在，返回空指针

	// 尝试打开数据库
	if (!dbPtr->open(path.c_str()))
	{
		// 打开失败，记录错误日志
		pipe_btreader_log(_sink, LL_ERROR, "Opening tick db of {}.{} failed: {}", exchg, code, dbPtr->errmsg());
		return std::move(WtLMDBPtr());
	}
	else
	{
		// 打开成功，记录调试日志
		pipe_btreader_log(_sink, LL_DEBUG, "Tick db of {}.{} opened", exchg, code);
	}

	// 将新连接加入缓存
	_tick_dbs[key] = dbPtr;                     // 注意：这里使用key而不是exchg
	return std::move(dbPtr);                    // 返回新连接
}