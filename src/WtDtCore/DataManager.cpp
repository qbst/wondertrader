/*!
 * \file DataManager.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据管理器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是DataManager类的具体实现，提供了数据管理的核心功能。作为WtDtCore
 * （数据传输核心）的中枢模块，DataManager协调了数据的接收、存储、广播和
 * 状态管理等各个环节。
 * 
 * 核心实现逻辑：
 * 
 * 1. 动态库加载机制：
 *    - 使用DLLHelper动态加载数据存储模块
 *    - 获取createWriter和deleteWriter函数指针
 *    - 运行时创建Writer实例
 *    - 支持多种Writer实现的切换（WtDataStorage、WtDataStorageAD等）
 * 
 * 2. 数据写入代理模式：
 *    - 所有写入方法都委托给Writer执行
 *    - DataManager作为中间层，不直接处理数据
 *    - 提供统一的错误检查（Writer为NULL时返回失败）
 * 
 * 3. 数据广播分发：
 *    - 实现IDataWriterSink接口
 *    - Writer写入成功后回调广播方法
 *    - 遍历所有Caster进行数据分发
 *    - 支持多种广播策略并行工作
 * 
 * 4. 状态控制集成：
 *    - 与StateMonitor协作控制数据接收时机
 *    - 非交易时段自动拒绝数据接收
 *    - 支持全天候模式（StateMonitor为NULL）
 * 
 * 关键实现细节：
 * 
 * 1. 模块路径构建：
 *    - 从配置中读取模块名称
 *    - 使用get_module_dir()获取模块目录
 *    - 使用wrap_module()处理平台差异（.dll/.so）
 * 
 * 2. 函数符号解析：
 *    - 从动态库中获取"createWriter"符号
 *    - 从动态库中获取"deleteWriter"符号
 *    - 使用函数指针调用动态库函数
 * 
 * 3. 资源管理：
 *    - Writer由动态库的create函数创建
 *    - Writer由动态库的delete函数销毁
 *    - 使用RAII思想管理资源生命周期
 * 
 * 4. 回调实现：
 *    - 实现IDataWriterSink的所有虚函数
 *    - 提供BaseDataMgr访问
 *    - 提供状态查询服务
 *    - 提供数据广播服务
 * 
 * 数据流转详解：
 * 
 * 写入流程：
 *   ParserAdapter::handleQuote()
 *   → DataManager::writeTick()
 *   → IDataWriter::writeTick()
 *   → [存储到文件]
 *   → IDataWriter回调DataManager::broadcastTick()
 *   → 遍历Casters → IDataCaster::broadcast()
 *   → [UDP广播/共享内存写入等]
 * 
 * 查询流程：
 *   IndexFactory::sub_ticks()
 *   → DataManager::getCurTick()
 *   → IDataWriter::getCurTick()
 *   → [从缓存中获取]
 *   → 返回WTSTickData指针
 * 
 * 转储流程：
 *   StateMonitor检测到盘后处理时间
 *   → DataManager::transHisData()
 *   → IDataWriter::transHisData()
 *   → [实时数据转为历史数据]
 * 
 * 性能优化策略：
 * - Writer可以使用异步写入提高性能
 * - Caster应该使用队列缓冲避免阻塞
 * - 状态检查开销很小，不会成为瓶颈
 * 
 * 错误处理策略：
 * - 所有写入方法都检查Writer是否为NULL
 * - 动态库加载失败记录详细错误日志
 * - 符号解析失败记录错误但继续尝试
 */

#include "DataManager.h"                        // 包含DataManager类定义
#include "StateMonitor.h"                       // 包含状态监控器定义
#include "UDPCaster.h"                          // 包含UDP广播器定义（用于类型引用）
#include "WtHelper.h"                           // 包含辅助工具类
#include "IDataCaster.h"                        // 包含数据广播器接口定义

#include "../Includes/WTSVariant.hpp"           // 包含配置参数类
#include "../Share/DLLHelper.hpp"               // 包含动态库加载辅助工具

#include "../WTSTools/WTSBaseDataMgr.h"         // 包含基础数据管理器
#include "../WTSTools/WTSLogger.h"              // 包含日志系统


/**
 * @brief 构造函数实现
 * 
 * 使用初始化列表将所有指针成员初始化为NULL，确保对象处于安全的初始状态。
 * 这是C++最佳实践，避免使用未初始化的指针导致的崩溃。
 * 
 * 初始化列表的优点：
 * - 直接初始化成员，而非先默认构造再赋值
 * - 对于const成员和引用成员是必须的
 * - 效率更高，特别是对于复杂类型
 * - 初始化顺序与成员声明顺序一致（而非列表顺序）
 */
DataManager::DataManager()
	: _writer(NULL)              // 数据写入器指针初始化为空
	, _bd_mgr(NULL)              // 基础数据管理器指针初始化为空
	, _state_mon(NULL)           // 状态监控器指针初始化为空
	// _casters使用默认构造函数，初始化为空vector
{
	// 构造函数体为空，所有初始化都在初始化列表中完成
}


/**
 * @brief 析构函数实现
 * 
 * 析构函数为空，不执行任何清理操作。资源释放应该由调用者
 * 显式调用release()方法完成。
 * 
 * 设计考虑：
 * - 资源释放需要特定的顺序和方式
 * - release()提供了明确的资源释放点
 * - 避免在析构时进行可能失败的操作
 * 
 * @warning 调用者应该在析构前调用release()
 */
DataManager::~DataManager()
{
	// 析构函数体为空
	// 资源释放通过显式调用release()完成
}

/**
 * @brief 检查交易时段是否已处理完成
 * 
 * 该方法查询指定交易时段的盘后处理是否完成。用于状态监控，
 * 避免重复执行盘后数据转储。
 * 
 * @param sid 交易时段标识符（如"TRADING"）
 * @return bool 已处理完成返回true，未处理或Writer为NULL返回false
 */
bool DataManager::isSessionProceeded(const char* sid)
{
	// 防御性检查：Writer未初始化时返回false
	if (_writer == NULL)
		return false;

	// 委托给Writer查询，Writer内部维护处理状态
	return _writer->isSessionProceeded(sid);
}

/**
 * @brief 初始化数据管理器实现
 * 
 * 这是DataManager的核心初始化方法，负责动态加载数据存储模块并初始化。
 * 
 * 实现步骤详解：
 * 
 * 步骤1：保存依赖组件的引用
 * - 保存基础数据管理器指针
 * - 保存状态监控器指针
 * 
 * 步骤2：确定数据存储模块
 * - 从配置中获取module参数
 * - 如果未指定，使用默认的"WtDataStorage"
 * - 构建模块的完整路径
 * 
 * 步骤3：加载动态库
 * - 使用DLLHelper::load_library()加载模块
 * - Windows加载.dll，Linux加载.so
 * - 失败时记录错误日志并返回false
 * 
 * 步骤4：获取函数指针
 * - 获取createWriter函数指针
 * - 获取deleteWriter函数指针
 * - 任一失败都记录错误日志
 * 
 * 步骤5：创建Writer实例
 * - 调用createWriter()创建实例
 * - 保存deleteWriter函数指针供后续销毁使用
 * 
 * 步骤6：初始化Writer
 * - 调用Writer的init()方法
 * - 传入配置参数和this指针（作为IDataWriterSink）
 * - 返回Writer的初始化结果
 * 
 * @param params 初始化配置参数
 * @param bdMgr 基础数据管理器指针
 * @param stMonitor 状态监控器指针（可为NULL）
 * @return bool 初始化成功返回true，失败返回false
 */
bool DataManager::init(WTSVariant* params, WTSBaseDataMgr* bdMgr, StateMonitor* stMonitor)
{
	// 步骤1：保存依赖组件的引用
	_bd_mgr = bdMgr;                    // 保存基础数据管理器指针
	_state_mon = stMonitor;             // 保存状态监控器指针（可为NULL）

	// 步骤2：确定数据存储模块名称和路径
	std::string module = params->getCString("module");  // 从配置中读取module参数
	
	if (module.empty())                 // 如果未指定模块名称
		// 使用默认模块"WtDataStorage"
		// get_module_dir()：获取模块目录路径
		// wrap_module()：处理平台差异（添加lib前缀和.dll/.so后缀）
		module = WtHelper::get_module_dir() + DLLHelper::wrap_module("WtDataStorage");
	else
		// 使用配置指定的模块名称
		// wrap_module()：将模块名转换为平台特定的库文件名
		// 例如："MyWriter" → Windows:"MyWriter.dll", Linux:"libMyWriter.so"
		module = WtHelper::get_module_dir() + DLLHelper::wrap_module(module.c_str());
	
	// 步骤3：加载动态库
	// load_library()：跨平台的动态库加载函数
	// Windows：调用LoadLibrary()
	// Linux：调用dlopen()
	// 返回动态库句柄，失败返回NULL
	DllHandle libWriter = DLLHelper::load_library(module.c_str());
	
	if (libWriter)                      // 如果动态库加载成功
	{
		// 步骤4a：获取createWriter函数指针
		// get_symbol()：从动态库中获取指定名称的符号（函数/变量）
		// "createWriter"：约定的工厂函数名称
		// 返回函数指针，失败返回NULL
		FuncCreateWriter pFuncCreateWriter = (FuncCreateWriter)DLLHelper::get_symbol(libWriter, "createWriter");
		if (pFuncCreateWriter == NULL)
		{
			// 未找到createWriter函数，记录错误
			// 这通常意味着动态库不符合IDataWriter规范
			WTSLogger::error("Initializing of data writer failed: function createWriter not found...");
		}

		// 步骤4b：获取deleteWriter函数指针
		// "deleteWriter"：约定的销毁函数名称
		// 必须使用模块提供的销毁函数，不能直接delete
		FuncDeleteWriter pFuncDeleteWriter = (FuncDeleteWriter)DLLHelper::get_symbol(libWriter, "deleteWriter");
		if (pFuncDeleteWriter == NULL)
		{
			// 未找到deleteWriter函数，记录错误
			// 虽然可以尝试直接delete，但不推荐，可能导致内存问题
			WTSLogger::error("Initializing of data writer failed: function deleteWriter not found...");
		}

		// 步骤5：创建Writer实例（仅当两个函数都获取成功时）
		if (pFuncCreateWriter && pFuncDeleteWriter)
		{
			// 调用工厂函数创建Writer实例
			// Writer在动态库中分配内存，返回接口指针
			_writer = pFuncCreateWriter();
			
			// 保存销毁函数指针，供release()使用
			// 必须使用模块的删除函数，确保内存在正确的堆上释放
			_remover = pFuncDeleteWriter;
		}
		
		// 记录模块加载成功的信息
		WTSLogger::info("Data storage module {} loaded", module);
	}
	else                                // 如果动态库加载失败
	{
		// 记录详细的错误信息，包括模块路径
		// 可能的原因：
		// 1. 文件不存在
		// 2. 文件损坏
		// 3. 依赖库缺失
		// 4. 权限不足
		WTSLogger::error("Initializing of data writer failed: loading module {} failed...", module.c_str());
		return false;                   // 返回失败
	}

	// 步骤6：初始化Writer
	// 调用Writer的init()方法完成初始化
	// 参数1：配置参数（包含存储路径、异步选项等）
	// 参数2：this指针（作为IDataWriterSink回调接收器）
	// Writer通过IDataWriterSink接口回调DataManager的方法
	return _writer->init(params, this);
}

/**
 * @brief 添加扩展的历史数据转储器
 * 
 * 该方法用于注册自定义的历史数据转储器。转储器在盘后处理时被调用，
 * 将实时数据转换为历史数据格式。
 * 
 * 应用场景：
 * - 数据备份到云存储
 * - 转换为其他格式（HDF5、Parquet等）
 * - 数据清洗和预处理
 * - 多份数据副本
 * 
 * @param id 转储器的唯一标识符
 * @param dumper 历史数据转储器接口指针
 */
void DataManager::add_ext_dumper(const char* id, IHisDataDumper* dumper)
{
	// 防御性检查：Writer未初始化时直接返回
	if (_writer == NULL)
		return;

	// 委托给Writer处理，Writer内部维护dumper集合
	_writer->add_ext_dumper(id, dumper);
}

/**
 * @brief 释放数据管理器资源
 * 
 * 该方法释放DataManager占用的资源，主要是Writer对象。
 * 
 * 释放流程：
 * 1. 调用Writer的release()方法，让Writer释放内部资源
 * 2. 调用_remover函数指针，销毁Writer对象本身
 * 
 * 资源释放顺序：
 * - 先释放Writer的内部资源（文件句柄、缓存等）
 * - 再销毁Writer对象本身
 * - 确保资源正确清理
 * 
 * @note Caster的释放由调用者负责
 * @note StateMonitor的释放由调用者负责
 * @note BaseDataMgr的释放由调用者负责
 */
void DataManager::release()
{
	if (_writer)                        // 如果Writer已创建
	{
		// 第一步：让Writer释放其内部资源
		// 包括：关闭文件、释放缓存、停止线程等
		_writer->release();
		
		// 第二步：销毁Writer对象本身
		// 必须使用模块提供的删除函数
		// 不能直接delete，因为内存可能在动态库的堆上分配
		_remover(_writer);
	}
	// _writer指针不置NULL，因为析构后对象不再使用
}

/**
 * @brief 写入Tick行情数据
 * 
 * 该方法是最常用的数据写入接口，处理Tick行情数据。
 * 
 * 处理流程：
 * 1. 检查Writer是否有效
 * 2. 委托Writer执行实际写入
 * 3. Writer内部会：
 *    a) 调用canSessionReceive()检查是否可接收
 *    b) 验证数据有效性
 *    c) 写入磁盘文件
 *    d) 更新内存缓存
 *    e) 回调broadcastTick()进行广播
 * 
 * @param curTick 当前Tick数据指针
 * @param procFlag 处理标志
 *        - 0：正常处理（写入+缓存+广播）
 *        - 1：仅写入，不更新缓存（用于指数等衍生品）
 *        - 其他值：由Writer实现定义
 * @return bool 写入成功返回true，失败返回false
 */
bool DataManager::writeTick(WTSTickData* curTick, uint32_t procFlag)
{
	// 防御性检查：Writer未初始化时返回失败
	if (_writer == NULL)
		return false;

	// 委托给Writer执行实际的写入操作
	// Writer负责：数据验证、文件写入、缓存更新、广播触发
	return _writer->writeTick(curTick, procFlag);
}

/**
 * @brief 写入委托队列数据
 * 
 * 该方法处理Level-2行情中的委托队列数据。委托队列显示了
 * 最优价位上的所有委托明细。
 * 
 * @param curOrdQue 当前委托队列数据指针
 * @return bool 写入成功返回true，失败返回false
 */
bool DataManager::writeOrderQueue(WTSOrdQueData* curOrdQue)
{
	// 防御性检查
	if (_writer == NULL)
		return false;

	// 委托给Writer处理委托队列数据
	return _writer->writeOrderQueue(curOrdQue);
}

/**
 * @brief 写入逐笔委托数据
 * 
 * 该方法处理Level-2行情中的逐笔委托数据。逐笔委托记录了
 * 每一笔委托单的详细信息。
 * 
 * @param curOrdDtl 当前逐笔委托数据指针
 * @return bool 写入成功返回true，失败返回false
 */
bool DataManager::writeOrderDetail(WTSOrdDtlData* curOrdDtl)
{
	// 防御性检查
	if (_writer == NULL)
		return false;

	// 委托给Writer处理逐笔委托数据
	return _writer->writeOrderDetail(curOrdDtl);
}

/**
 * @brief 写入逐笔成交数据
 * 
 * 该方法处理Level-2行情中的逐笔成交数据。逐笔成交记录了
 * 市场上每一笔真实成交的详细信息。
 * 
 * @param curTrans 当前逐笔成交数据指针
 * @return bool 写入成功返回true，失败返回false
 */
bool DataManager::writeTransaction(WTSTransData* curTrans)
{
	// 防御性检查
	if (_writer == NULL)
		return false;

	// 委托给Writer处理逐笔成交数据
	return _writer->writeTransaction(curTrans);
}

/**
 * @brief 获取合约的当前Tick数据
 * 
 * 该方法从Writer的内存缓存中获取指定合约的最新Tick数据。
 * 主要用于指数计算、数据查询等场景。
 * 
 * 使用场景：
 * - IndexFactory订阅成分合约行情
 * - 策略查询当前价格
 * - 数据完整性检查
 * 
 * @param code 合约代码（如"rb2105"）
 * @param exchg 交易所代码（如"SHFE"），默认为空
 * @return WTSTickData* Tick数据指针，不存在返回NULL
 * 
 * @note 返回的指针由Writer管理，调用者不应该删除
 * @note 数据是快照，可能在返回后被更新
 */
WTSTickData* DataManager::getCurTick(const char* code, const char* exchg/* = ""*/)
{
	// 防御性检查
	if (_writer == NULL)
		return NULL;

	// 委托给Writer从缓存中获取当前Tick
	// Writer内部维护一个code -> tick的映射表
	return _writer->getCurTick(code, exchg);
}

/**
 * @brief 触发历史数据转储
 * 
 * 该方法触发实时数据到历史数据的转储过程。通常在交易日结束后
 * 由StateMonitor自动调用。
 * 
 * 转储过程：
 * 1. 将当日的实时数据文件整理为历史数据格式
 * 2. 可能涉及数据压缩、索引构建等操作
 * 3. 清理实时数据缓存
 * 
 * 特殊命令：
 * - "CMD_CLEAR_CACHE"：清理所有缓存
 *   当所有交易时段都处理完成后，StateMonitor会发送此命令
 * 
 * @param sid 交易时段标识符，或特殊命令字符串
 * 
 * @note 可能是耗时操作，建议Writer异步执行
 */
void DataManager::transHisData(const char* sid)
{
	if (_writer)                        // 检查Writer是否有效
		// 委托给Writer执行历史数据转储
		// Writer内部会根据sid确定转储范围
		_writer->transHisData(sid);
}

//////////////////////////////////////////////////////////////////////////
// IDataWriterSink 接口实现
// 以下方法实现了IDataWriterSink接口，为Writer提供回调服务
//////////////////////////////////////////////////////////////////////////

#pragma region "IDataWriterSink"

/**
 * @brief 获取基础数据管理器（IDataWriterSink接口实现）
 * 
 * Writer通过此方法获取基础数据管理器，用于：
 * - 验证合约代码是否存在
 * - 获取合约的详细信息
 * - 查询交易日历
 * - 获取交易时段信息
 * 
 * @return IBaseDataMgr* 基础数据管理器接口指针
 */
IBaseDataMgr* DataManager::getBDMgr()
{
	// 直接返回初始化时保存的基础数据管理器指针
	return _bd_mgr;
}

/**
 * @brief 检查交易时段是否可以接收数据（IDataWriterSink接口实现）
 * 
 * Writer在接收数据前会调用此方法，判断当前时间是否允许接收数据。
 * 这是数据质量控制的重要环节，过滤非交易时段的数据。
 * 
 * 实现逻辑：
 * 
 * 1. 全天候模式（StateMonitor为NULL）：
 *    - 始终返回true
 *    - 不进行时段控制
 *    - 适用于24小时运行的场景
 * 
 * 2. 时段控制模式（StateMonitor不为NULL）：
 *    - 查询StateMonitor中的当前状态
 *    - 只有SS_RECEIVING状态才返回true
 *    - 其他状态（初始化、暂停、收盘等）返回false
 * 
 * 应用效果：
 * - 开盘前的数据：拒绝接收（状态为SS_INITIALIZED）
 * - 交易中的数据：正常接收（状态为SS_RECEIVING）
 * - 中途休盘数据：拒绝接收（状态为SS_PAUSED）
 * - 收盘后的数据：拒绝接收（状态为SS_CLOSED）
 * - 节假日的数据：拒绝接收（状态为SS_Holiday）
 * 
 * @param sid 交易时段标识符（如"TRADING"、"ALLDAY"等）
 * @return bool 可以接收返回true，不可接收返回false
 * 
 * @note By Wesley @ 2021.12.27 - 添加了全天候模式支持
 * @note 全天候模式适用于7x24交易的数字货币等市场
 * 
 * @see StateMonitor::isInState() 状态检查方法
 */
bool DataManager::canSessionReceive(const char* sid)
{
	// By Wesley @ 2021.12.27
	// 如果状态机为NULL，说明是全天候模式，直接返回true即可
	if (_state_mon == NULL)
		return true;                    // 全天候模式：始终可以接收数据

	// 时段控制模式：查询StateMonitor中sid对应的状态
	// 只有状态为SS_RECEIVING时才返回true
	// SS_RECEIVING：正常交易时间，可以接收行情数据
	return _state_mon->isInState(sid, SS_RECEIVING);
}

/**
 * @brief 广播Tick数据（IDataWriterSink接口实现）
 * 
 * Writer在成功写入Tick数据后回调此方法，DataManager负责将数据
 * 广播到所有注册的Caster。
 * 
 * 广播流程：
 * 1. 遍历_casters集合（使用范围for循环）
 * 2. 对每个Caster调用broadcast(curTick)
 * 3. 顺序执行，不并发
 * 
 * 设计考虑：
 * - 使用范围for循环，代码简洁
 * - 顺序执行，确保数据一致性
 * - 不捕获异常，Caster应该自行处理错误
 * 
 * @param curTick 当前Tick数据指针
 * 
 * @warning 如果某个Caster阻塞，会影响后续Caster
 * @note Caster应该快速返回，耗时操作应异步执行
 */
void DataManager::broadcastTick(WTSTickData* curTick)
{
	// 使用C++11范围for循环遍历所有广播器
	// 等价于：for (auto it = _casters.begin(); it != _casters.end(); ++it)
	for(IDataCaster* caster : _casters)
		// 调用每个广播器的broadcast方法
		// 通过虚函数机制，调用具体Caster的实现
		// UDPCaster会通过UDP发送，ShmCaster会写入共享内存
		caster->broadcast(curTick);
}

/**
 * @brief 广播逐笔委托数据（IDataWriterSink接口实现）
 * 
 * Writer在成功写入逐笔委托数据后回调此方法。
 * 
 * @param curOrdDtl 当前逐笔委托数据指针
 */
void DataManager::broadcastOrdDtl(WTSOrdDtlData* curOrdDtl)
{
	// 遍历所有广播器，广播逐笔委托数据
	for (IDataCaster* caster : _casters)
		caster->broadcast(curOrdDtl);
}

/**
 * @brief 广播委托队列数据（IDataWriterSink接口实现）
 * 
 * Writer在成功写入委托队列数据后回调此方法。
 * 
 * @param curOrdQue 当前委托队列数据指针
 */
void DataManager::broadcastOrdQue(WTSOrdQueData* curOrdQue)
{
	// 遍历所有广播器，广播委托队列数据
	for (IDataCaster* caster : _casters)
		caster->broadcast(curOrdQue);
}

/**
 * @brief 广播逐笔成交数据（IDataWriterSink接口实现）
 * 
 * Writer在成功写入逐笔成交数据后回调此方法。
 * 
 * @param curTrans 当前逐笔成交数据指针
 */
void DataManager::broadcastTrans(WTSTransData* curTrans)
{
	// 遍历所有广播器，广播逐笔成交数据
	for (IDataCaster* caster : _casters)
		caster->broadcast(curTrans);
}

/**
 * @brief 获取交易时段对应的品种集合（IDataWriterSink接口实现）
 * 
 * Writer需要知道每个交易时段对应哪些品种，用于数据转储时
 * 确定处理范围。
 * 
 * 例如：
 * - "TRADING"时段可能包含：SHFE.rb、DCE.i、CZCE.CF等
 * - "NIGHT"时段可能包含：有夜盘交易的品种
 * 
 * @param sid 交易时段标识符
 * @return CodeSet* 品种代码集合指针（如{"SHFE.rb", "DCE.i"}）
 */
CodeSet* DataManager::getSessionComms(const char* sid)
{
	// 直接委托给BaseDataMgr处理
	// BaseDataMgr维护了时段与品种的映射关系
	return  _bd_mgr->getSessionComms(sid);
}

/**
 * @brief 获取品种的交易日（IDataWriterSink接口实现）
 * 
 * Writer需要知道品种的当前交易日，用于：
 * - 数据文件按交易日组织（如20210101.dsb）
 * - 数据转储时确定目标日期
 * - 跨日行情的正确归属
 * 
 * 交易日说明：
 * - 交易日不等于自然日
 * - 夜盘属于下一个交易日
 * - 例如：2021-01-04晚上的夜盘，交易日是20210105
 * 
 * @param pid 品种ID（如"SHFE.rb"）
 * @return uint32_t 交易日，格式YYYYMMDD（如20210105）
 */
uint32_t DataManager::getTradingDate(const char* pid)
{
	// 直接委托给BaseDataMgr处理
	// BaseDataMgr维护了每个品种的当前交易日
	return  _bd_mgr->getTradingDate(pid);
}

/**
 * @brief 输出日志信息（IDataWriterSink接口实现）
 * 
 * Writer通过此方法输出日志，统一使用WonderTrader的日志系统。
 * 这确保了所有模块的日志格式一致，便于问题排查。
 * 
 * 日志级别：
 * - LL_FATAL：致命错误，程序无法继续
 * - LL_ERROR：错误，功能异常但程序可继续
 * - LL_WARN：警告，潜在问题
 * - LL_INFO：信息，重要的状态变化
 * - LL_DEBUG：调试信息，详细的执行过程
 * 
 * @param ll 日志级别
 * @param message 日志内容（C风格字符串）
 * 
 * @see WTSLogger 日志系统
 */
void DataManager::outputLog(WTSLogLevel ll, const char* message)
{
	// 使用WTSLogger的原始日志接口
	// log_raw：直接输出消息，不添加额外前缀
	// 适合输出Writer内部生成的格式化日志
	WTSLogger::log_raw(ll, message);
}

#pragma endregion "IDataWriterSink"
// IDataWriterSink接口实现区域结束
