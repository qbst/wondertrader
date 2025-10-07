/*!
 * \file DataManager.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据管理器定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了DataManager（数据管理器）类，是WonderTrader数据传输核心（WtDtCore）
 * 的中枢模块。该类采用外观模式（Facade Pattern）和适配器模式（Adapter Pattern），
 * 统一管理数据的接收、存储、验证和分发。
 * 
 * 核心设计理念：
 * 
 * 1. 数据流中枢（Data Flow Hub）：
 *    - 接收来自Parser的行情数据
 *    - 委托Writer进行数据持久化
 *    - 通过Caster进行数据广播
 *    - 协调StateMonitor进行状态控制
 * 
 * 2. 插件化架构（Plugin Architecture）：
 *    - 动态加载数据存储模块（IDataWriter）
 *    - 支持多个广播器同时工作（IDataCaster集合）
 *    - 可扩展的历史数据转储器（IHisDataDumper）
 * 
 * 3. 双重角色（Dual Role）：
 *    角色1：数据管理门面（Facade）
 *    - 对外提供简洁的数据管理接口
 *    - 隐藏内部复杂的模块协调逻辑
 *    
 *    角色2：Writer回调接收器（IDataWriterSink）
 *    - 实现IDataWriterSink接口
 *    - 为Writer提供必要的回调服务
 *    - 支持Writer的数据查询和广播需求
 * 
 * 架构设计：
 * 
 *           ┌────────────────────────────────┐
 *           │       ParserAdapter            │
 *           │   (行情数据来源)               │
 *           └───────────┬────────────────────┘
 *                       │ handleQuote()
 *                       ↓
 *           ┌────────────────────────────────┐
 *           │       DataManager              │  <-- 本类
 *           │    (数据管理中枢)              │
 *           └─┬──────────┬─────────┬─────────┘
 *             │          │         │
 *      写入   │    广播  │   查询  │
 *             ↓          ↓         ↓
 *    ┌──────────┐  ┌─────────┐  ┌──────────┐
 *    │IDataWriter│  │Casters  │  │StateMonitor│
 *    │(存储模块)│  │(广播器) │  │(状态控制)  │
 *    └──────────┘  └─────────┘  └──────────┘
 *         │             │              │
 *         ↓             ↓              ↓
 *    [磁盘文件]    [UDP/Shm]    [交易时段状态]
 * 
 * 数据流转过程：
 * 
 * 1. 数据接收流程：
 *    Parser → ParserAdapter → DataManager.writeTick()
 *    
 * 2. 数据存储流程：
 *    DataManager.writeTick() → IDataWriter.writeTick() → 磁盘文件
 *    
 * 3. 数据广播流程：
 *    IDataWriter回调 → DataManager.broadcastTick() → IDataCaster(s)
 *    
 * 4. 状态控制流程：
 *    DataManager.canSessionReceive() → StateMonitor.isInState()
 * 
 * 主要功能模块：
 * 
 * 1. 动态库管理：
 *    - 动态加载数据存储模块（WtDataStorage或自定义模块）
 *    - 获取创建和销毁函数指针
 *    - 管理模块的生命周期
 * 
 * 2. 数据写入接口：
 *    - writeTick()：写入Tick行情数据
 *    - writeOrderQueue()：写入委托队列数据
 *    - writeOrderDetail()：写入逐笔委托数据
 *    - writeTransaction()：写入逐笔成交数据
 * 
 * 3. 数据广播接口：
 *    - broadcastTick()：广播Tick数据到所有Caster
 *    - broadcastOrdQue()：广播委托队列数据
 *    - broadcastOrdDtl()：广播逐笔委托数据
 *    - broadcastTrans()：广播逐笔成交数据
 * 
 * 4. 状态查询接口：
 *    - canSessionReceive()：检查交易时段是否可接收数据
 *    - isSessionProceeded()：检查交易时段是否已处理完成
 *    - getCurTick()：获取合约的当前Tick数据
 * 
 * 5. 辅助管理接口：
 *    - add_caster()：添加数据广播器
 *    - add_ext_dumper()：添加扩展的历史数据转储器
 *    - transHisData()：触发历史数据转储
 * 
 * 设计模式应用：
 * - 外观模式（Facade）：简化子系统访问
 * - 适配器模式（Adapter）：适配IDataWriterSink接口
 * - 策略模式（Strategy）：多种Caster策略
 * - 依赖注入（DI）：注入BaseDataMgr和StateMonitor
 * 
 * 线程安全性：
 * - DataManager本身不提供线程同步
 * - 通常在单一线程（数据接收线程）中使用
 * - Writer和Caster的线程安全由各自实现保证
 * 
 * 使用场景：
 * - 实时行情数据落地
 * - 行情数据广播分发
 * - 历史数据转储
 * - 数据质量控制
 * - 交易时段状态管理
 */

#pragma once                                                // 防止头文件重复包含

#include "../Includes/IDataWriter.h"                        // 包含数据写入器接口定义
#include "../Share/StdUtils.hpp"                            // 包含标准工具类（如智能指针等）
#include "../Share/BoostMappingFile.hpp"                    // 包含Boost内存映射文件工具

// 前向声明WonderTrader命名空间中的类
// 使用前向声明减少编译依赖，加快编译速度
NS_WTP_BEGIN                                                // 开始WonderTrader命名空间
class WTSTickData;                                          // Tick行情数据类
class WTSOrdQueData;                                        // 委托队列数据类
class WTSOrdDtlData;                                        // 逐笔委托数据类
class WTSTransData;                                         // 逐笔成交数据类
class WTSVariant;                                           // 配置参数类
class IDataCaster;                                          // 数据广播器接口
NS_WTP_END                                                  // 结束WonderTrader命名空间

USING_NS_WTP;                                               // 使用WonderTrader命名空间

// 前向声明其他类
class WTSBaseDataMgr;                                       // 基础数据管理器（合约、交易日等）
class StateMonitor;                                         // 状态监控器（交易时段状态）
class UDPCaster;                                            // UDP广播器（前向声明但未使用，可能是历史遗留）

/**
 * @class DataManager
 * @brief 数据管理器类（数据传输核心的中枢模块）
 * 
 * 该类是WonderTrader数据传输核心（WtDtCore）的中央协调器，负责：
 * 1. 管理数据的接收和存储
 * 2. 协调数据的广播分发
 * 3. 控制数据接收的时段状态
 * 4. 提供数据查询服务
 * 
 * 继承关系：
 * - 公有继承自IDataWriterSink接口
 * - 为IDataWriter提供回调服务
 * 
 * 主要职责：
 * 
 * 1. 作为外观（Facade）：
 *    - 对外提供简洁的数据管理接口
 *    - 隐藏Writer、Caster、StateMonitor的复杂交互
 *    - 统一管理数据流转过程
 * 
 * 2. 作为适配器（Adapter）：
 *    - 实现IDataWriterSink接口
 *    - 为Writer提供必要的回调功能
 *    - 桥接Writer与其他组件
 * 
 * 3. 作为协调器（Coordinator）：
 *    - 协调Writer进行数据存储
 *    - 协调Caster进行数据广播
 *    - 协调StateMonitor进行状态控制
 * 
 * 典型使用流程：
 * @code
 *   // 1. 创建DataManager
 *   DataManager* dataMgr = new DataManager();
 *   
 *   // 2. 初始化（传入配置、基础数据管理器、状态监控器）
 *   dataMgr->init(config, baseDataMgr, stateMonitor);
 *   
 *   // 3. 添加数据广播器
 *   dataMgr->add_caster(new UDPCaster());
 *   dataMgr->add_caster(new ShmCaster());
 *   
 *   // 4. 接收并处理数据
 *   dataMgr->writeTick(tickData, 0);
 *   
 *   // 5. 盘后处理：转储历史数据
 *   dataMgr->transHisData("TRADING");
 *   
 *   // 6. 清理资源
 *   dataMgr->release();
 *   delete dataMgr;
 * @endcode
 * 
 * 配置示例：
 * @code
 *   {
 *     "module": "WtDataStorage",     // 数据存储模块名称
 *     "path": "./data/",              // 数据存储路径
 *     "async": true,                  // 是否异步写入
 *     "groupsize": 100                // 批量写入大小
 *   }
 * @endcode
 * 
 * 线程安全性：
 * - DataManager本身不是线程安全的
 * - 通常在单一的数据接收线程中使用
 * - Writer和Caster的实现应该考虑线程安全
 * 
 * 性能考虑：
 * - 数据写入：Writer可能使用异步机制
 * - 数据广播：遍历所有Caster，避免Caster阻塞
 * - 状态检查：轻量级操作，不会成为瓶颈
 */
class DataManager : public IDataWriterSink
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化所有成员指针为NULL，确保对象的初始状态安全。
	 * 实际的资源分配和初始化在init()方法中完成。
	 */
	DataManager();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理DataManager对象。注意：不会自动调用release()，
	 * 调用者应该在析构前显式调用release()释放资源。
	 * 
	 * @warning 析构前应该先调用release()释放资源
	 */
	~DataManager();

public:
	/**
	 * @brief 初始化数据管理器
	 * 
	 * 该方法是DataManager的核心初始化函数，负责：
	 * 1. 保存基础数据管理器和状态监控器的引用
	 * 2. 动态加载数据存储模块（IDataWriter实现）
	 * 3. 获取模块的创建和销毁函数
	 * 4. 创建Writer实例并初始化
	 * 
	 * 初始化流程：
	 * 1. 从配置中获取模块名称
	 * 2. 构建模块的完整路径
	 * 3. 使用DLLHelper动态加载模块
	 * 4. 获取createWriter和deleteWriter函数指针
	 * 5. 创建Writer实例
	 * 6. 调用Writer的init()方法进行初始化
	 * 
	 * 配置参数说明：
	 * - module: 数据存储模块名称（可选，默认"WtDataStorage"）
	 * - 其他参数：传递给Writer的init()方法
	 * 
	 * 模块加载策略：
	 * - 如果未指定module，使用默认的"WtDataStorage"
	 * - 模块文件位于模块目录（get_module_dir()）
	 * - 自动处理不同平台的动态库扩展名（.dll/.so）
	 * 
	 * @param params 初始化配置参数
	 * @param bdMgr 基础数据管理器指针（管理合约、交易日等）
	 * @param stMonitor 状态监控器指针（管理交易时段状态，可为NULL）
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * @note stMonitor可以为NULL，此时canSessionReceive()总是返回true
	 * @note 初始化失败会记录错误日志
	 * 
	 * @see IDataWriter::init() Writer的初始化方法
	 */
	bool init(WTSVariant* params, WTSBaseDataMgr* bdMgr, StateMonitor* stMonitor);

	/**
	 * @brief 添加扩展的历史数据转储器
	 * 
	 * 该方法用于注册自定义的历史数据转储器（Dumper）。Dumper负责
	 * 在盘后将实时数据转换为历史数据格式并存储。
	 * 
	 * 使用场景：
	 * - 自定义的数据存储格式
	 * - 数据备份到远程服务器
	 * - 数据转换为特定格式（如HDF5、Parquet等）
	 * 
	 * @param id 转储器的唯一标识符
	 * @param dumper 历史数据转储器接口指针
	 * 
	 * @note 该方法直接委托给Writer处理
	 * @note 如果Writer为NULL，方法直接返回
	 * 
	 * @see IHisDataDumper 历史数据转储器接口
	 */
	void add_ext_dumper(const char* id, IHisDataDumper* dumper);

	/**
	 * @brief 添加数据广播器
	 * 
	 * 该方法用于注册数据广播器（Caster）。DataManager支持同时使用
	 * 多个Caster，每次数据写入成功后会通过所有Caster进行广播。
	 * 
	 * 内联函数实现：
	 * - 直接在头文件中实现，编译器可以优化
	 * - 避免函数调用开销
	 * 
	 * 广播器类型：
	 * - UDPCaster：通过UDP网络广播
	 * - ShmCaster：通过共享内存广播
	 * - 自定义Caster：实现IDataCaster接口
	 * 
	 * @param caster 数据广播器指针，如果为NULL则忽略
	 * 
	 * @note 多个Caster按添加顺序依次广播
	 * @note 如果某个Caster阻塞，会影响后续Caster的执行
	 * 
	 * @see IDataCaster 数据广播器接口
	 */
	inline void add_caster(IDataCaster* caster)
	{
		// 空指针检查，避免添加无效的广播器
		if (caster == NULL)
			return;

		// 使用emplace_back直接在vector尾部构造元素
		// 相比push_back，避免了不必要的拷贝
		_casters.emplace_back(caster);
	}

	/**
	 * @brief 释放数据管理器资源
	 * 
	 * 该方法释放DataManager占用的所有资源，主要是Writer对象。
	 * 
	 * 释放流程：
	 * 1. 调用Writer的release()方法释放Writer内部资源
	 * 2. 调用_remover函数指针销毁Writer对象
	 * 
	 * 注意事项：
	 * - 该方法不会自动在析构函数中调用
	 * - 调用者应该在不再使用DataManager时显式调用
	 * - 释放后不应该再调用DataManager的其他方法
	 * - Caster的释放由调用者负责，DataManager不管理其生命周期
	 * 
	 * @warning 调用后对象不可再使用
	 * @warning 不会释放Caster，调用者需要自行管理
	 */
	void release();

	/**
	 * @brief 写入Tick行情数据
	 * 
	 * 该方法是数据写入的主要接口，用于将Tick行情数据写入存储。
	 * Writer在写入成功后会回调broadcastTick()进行数据广播。
	 * 
	 * 处理流程：
	 * 1. 检查Writer是否有效
	 * 2. 委托Writer进行实际写入
	 * 3. Writer内部会调用canSessionReceive()检查是否可接收
	 * 4. 写入成功后Writer回调broadcastTick()进行广播
	 * 
	 * @param curTick 当前Tick数据指针
	 * @param procFlag 处理标志
	 *        - 0: 正常处理
	 *        - 1: 仅写入，不更新缓存（用于指数计算等场景）
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * @note procFlag的具体含义由Writer实现决定
	 * @note Writer为NULL时返回false
	 * 
	 * @see IDataWriter::writeTick() Writer的写入方法
	 */
	bool writeTick(WTSTickData* curTick, uint32_t procFlag);

	/**
	 * @brief 写入委托队列数据
	 * 
	 * 该方法用于将Level-2委托队列数据写入存储。
	 * 
	 * @param curOrdQue 当前委托队列数据指针
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * @see writeOrderDetail() 逐笔委托数据写入
	 */
	bool writeOrderQueue(WTSOrdQueData* curOrdQue);

	/**
	 * @brief 写入逐笔委托数据
	 * 
	 * 该方法用于将Level-2逐笔委托数据写入存储。
	 * 
	 * @param curOrdDetail 当前逐笔委托数据指针
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * @see writeOrderQueue() 委托队列数据写入
	 */
	bool writeOrderDetail(WTSOrdDtlData* curOrdDetail);

	/**
	 * @brief 写入逐笔成交数据
	 * 
	 * 该方法用于将Level-2逐笔成交数据写入存储。
	 * 
	 * @param curTrans 当前逐笔成交数据指针
	 * @return bool 写入成功返回true，失败返回false
	 * 
	 * @see writeTick() Tick数据写入
	 */
	bool writeTransaction(WTSTransData* curTrans);

	/**
	 * @brief 触发历史数据转储
	 * 
	 * 该方法用于将实时数据转换为历史数据并持久化。
	 * 通常在交易日结束后调用，由StateMonitor在适当时机触发。
	 * 
	 * 触发时机：
	 * - 交易日收盘后
	 * - 收盘作业处理时间到达
	 * - StateMonitor状态转换为SS_PROCING时
	 * 
	 * 特殊命令：
	 * - "CMD_CLEAR_CACHE": 清理缓存（所有时段都处理完成后）
	 * 
	 * @param sid 交易时段标识符，或特殊命令
	 * 
	 * @note 实际转储由Writer实现
	 * @note 可能是耗时操作，建议异步执行
	 * 
	 * @see StateMonitor 状态监控器
	 */
	void transHisData(const char* sid);
	
	/**
	 * @brief 检查交易时段是否已处理完成
	 * 
	 * 该方法用于查询指定交易时段的历史数据是否已经转储完成。
	 * 
	 * @param sid 交易时段标识符
	 * @return bool 已处理完成返回true，否则返回false
	 * 
	 * @note Writer为NULL时返回false
	 * 
	 * @see transHisData() 触发历史数据转储
	 */
	bool isSessionProceeded(const char* sid);

	/**
	 * @brief 获取合约的当前Tick数据
	 * 
	 * 该方法用于查询指定合约的最新Tick数据。通常用于：
	 * - 指数计算时获取成分合约的行情
	 * - 策略查询合约的当前价格
	 * - 数据质量检查
	 * 
	 * @param code 合约代码
	 * @param exchg 交易所代码，默认为空字符串
	 * @return WTSTickData* Tick数据指针，不存在返回NULL
	 * 
	 * @note 返回的指针由Writer管理，调用者不应该删除
	 * @note Writer为NULL时返回NULL
	 * 
	 * @see writeTick() 写入Tick数据
	 */
	WTSTickData* getCurTick(const char* code, const char* exchg = "");

public:
	//////////////////////////////////////////////////////////////////////////
	// IDataWriterSink 接口实现
	// 以下方法实现了IDataWriterSink接口，为Writer提供回调服务
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 获取基础数据管理器
	 * 
	 * Writer需要通过此方法获取基础数据管理器，用于：
	 * - 验证合约代码的有效性
	 * - 查询合约的基本信息
	 * - 获取交易时段信息
	 * 
	 * @return IBaseDataMgr* 基础数据管理器指针
	 * 
	 * @see WTSBaseDataMgr 基础数据管理器
	 */
	virtual IBaseDataMgr* getBDMgr() override;

	/**
	 * @brief 检查交易时段是否可以接收数据
	 * 
	 * Writer在接收数据前会调用此方法检查当前是否处于可接收状态。
	 * 
	 * 判断逻辑：
	 * - 如果StateMonitor为NULL，返回true（全天候模式）
	 * - 否则检查StateMonitor中的状态是否为SS_RECEIVING
	 * 
	 * 交易时段状态：
	 * - SS_RECEIVING：接收中，返回true
	 * - 其他状态：返回false
	 * 
	 * @param sid 交易时段标识符
	 * @return bool 可以接收返回true，不可接收返回false
	 * 
	 * @note 全天候模式（StateMonitor为NULL）总是返回true
	 * @note 用于过滤非交易时间的数据
	 * 
	 * @see StateMonitor::isInState() 状态检查方法
	 */
	virtual bool canSessionReceive(const char* sid) override;

	/**
	 * @brief 广播Tick数据
	 * 
	 * Writer在成功写入Tick数据后回调此方法进行数据广播。
	 * 该方法遍历所有注册的Caster，依次调用其broadcast()方法。
	 * 
	 * 执行流程：
	 * 1. 遍历_casters集合
	 * 2. 对每个Caster调用broadcast(curTick)
	 * 3. 顺序执行，前一个Caster完成后才执行下一个
	 * 
	 * @param curTick 当前Tick数据指针
	 * 
	 * @warning 如果某个Caster执行缓慢，会阻塞后续Caster
	 * @note Caster应该快速返回，耗时操作应异步执行
	 * 
	 * @see IDataCaster::broadcast() 广播器接口方法
	 */
	virtual void broadcastTick(WTSTickData* curTick) override;

	/**
	 * @brief 广播委托队列数据
	 * 
	 * Writer在成功写入委托队列数据后回调此方法进行数据广播。
	 * 
	 * @param curOrdQue 当前委托队列数据指针
	 * 
	 * @see broadcastTick() Tick数据广播
	 */
	virtual void broadcastOrdQue(WTSOrdQueData* curOrdQue) override;

	/**
	 * @brief 广播逐笔委托数据
	 * 
	 * Writer在成功写入逐笔委托数据后回调此方法进行数据广播。
	 * 
	 * @param curOrdDtl 当前逐笔委托数据指针
	 * 
	 * @see broadcastTick() Tick数据广播
	 */
	virtual void broadcastOrdDtl(WTSOrdDtlData* curOrdDtl) override;

	/**
	 * @brief 广播逐笔成交数据
	 * 
	 * Writer在成功写入逐笔成交数据后回调此方法进行数据广播。
	 * 
	 * @param curTrans 当前逐笔成交数据指针
	 * 
	 * @see broadcastTick() Tick数据广播
	 */
	virtual void broadcastTrans(WTSTransData* curTrans) override;

	/**
	 * @brief 获取交易时段对应的品种集合
	 * 
	 * Writer需要知道每个交易时段对应哪些品种，用于：
	 * - 数据转储时确定处理范围
	 * - 交易日设置和验证
	 * 
	 * @param sid 交易时段标识符
	 * @return CodeSet* 品种代码集合指针
	 * 
	 * @note 直接委托给BaseDataMgr处理
	 * 
	 * @see WTSBaseDataMgr::getSessionComms() 基础数据管理器方法
	 */
	virtual CodeSet* getSessionComms(const char* sid) override;

	/**
	 * @brief 获取品种的交易日
	 * 
	 * Writer需要知道品种的交易日，用于：
	 * - 数据文件按交易日组织
	 * - 数据转储时确定目标日期
	 * 
	 * @param pid 品种ID（如SHFE.rb）
	 * @return uint32_t 交易日，格式YYYYMMDD
	 * 
	 * @note 直接委托给BaseDataMgr处理
	 * 
	 * @see WTSBaseDataMgr::getTradingDate() 基础数据管理器方法
	 */
	virtual uint32_t getTradingDate(const char* pid) override;

	/**
	 * @brief 输出日志信息
	 * 
	 * Writer通过此方法输出日志信息，统一使用WonderTrader的日志系统。
	 * 
	 * @param ll 日志级别（LL_DEBUG, LL_INFO, LL_WARN, LL_ERROR等）
	 * @param message 日志内容
	 * 
	 * @note 直接委托给WTSLogger处理
	 * 
	 * @see WTSLogger::log_raw() 日志系统方法
	 */
	virtual void outputLog(WTSLogLevel ll, const char* message) override;

private:
	/**
	 * @brief 数据写入器接口指针
	 * 
	 * 指向动态加载的数据存储模块实例，负责实际的数据持久化工作。
	 * 通过动态库加载机制创建，生命周期由DataManager管理。
	 * 
	 * 典型实现：
	 * - WtDataStorage：默认的数据存储实现
	 * - 自定义Writer：用户自定义的存储实现
	 * 
	 * @see IDataWriter 数据写入器接口
	 */
	IDataWriter*		_writer;
	
	/**
	 * @brief Writer销毁函数指针
	 * 
	 * 指向动态库中的deleteWriter函数，用于销毁Writer实例。
	 * 必须使用模块提供的销毁函数，不能直接delete，因为：
	 * 1. Writer可能在动态库中分配内存
	 * 2. 不同模块可能使用不同的内存分配器
	 * 3. 确保资源正确释放
	 * 
	 * @see FuncDeleteWriter 函数指针类型定义
	 */
	FuncDeleteWriter	_remover;
	
	/**
	 * @brief 基础数据管理器指针
	 * 
	 * 管理框架的基础数据，包括：
	 * - 合约信息（代码、交易所、品种等）
	 * - 交易时段信息（开盘时间、收盘时间等）
	 * - 交易日历（交易日、节假日等）
	 * - 主力合约映射
	 * 
	 * @see WTSBaseDataMgr 基础数据管理器
	 */
	WTSBaseDataMgr*		_bd_mgr;
	
	/**
	 * @brief 状态监控器指针
	 * 
	 * 管理交易时段的状态，控制数据接收的时机。
	 * 可以为NULL，表示全天候模式（不进行状态控制）。
	 * 
	 * 状态包括：
	 * - SS_ORIGINAL：原始状态（未初始化）
	 * - SS_INITIALIZED：已初始化
	 * - SS_RECEIVING：接收中（可以接收数据）
	 * - SS_PAUSED：暂停（中途休盘）
	 * - SS_CLOSED：已收盘
	 * - SS_PROCING：处理中（盘后作业）
	 * - SS_PROCED：已处理
	 * - SS_Holiday：节假日
	 * 
	 * @note 为NULL时表示全天候模式
	 * 
	 * @see StateMonitor 状态监控器
	 */
	StateMonitor*		_state_mon;
	
	/**
	 * @brief 数据广播器集合
	 * 
	 * 存储所有注册的数据广播器（Caster），支持多个Caster同时工作。
	 * 数据写入成功后，会依次调用每个Caster的broadcast()方法。
	 * 
	 * 典型包含：
	 * - UDPCaster：UDP网络广播
	 * - ShmCaster：共享内存广播
	 * - 自定义Caster：用户自定义的广播实现
	 * 
	 * 注意事项：
	 * - vector保证按添加顺序执行
	 * - Caster的生命周期由调用者管理
	 * - DataManager不负责Caster的创建和销毁
	 * 
	 * @see IDataCaster 数据广播器接口
	 */
	std::vector<IDataCaster*>	_casters;
};

