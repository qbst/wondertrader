/*!
 * \file StateMonitor.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易时段状态监控器定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了StateMonitor（状态监控器）类，是WonderTrader数据传输核心中
 * 负责交易时段状态管理的关键模块。该类采用有限状态机（FSM - Finite State Machine）
 * 设计模式，精确控制数据接收、处理和转储的时机。
 * 
 * 核心设计理念：
 * 
 * 1. 有限状态机（FSM）设计：
 *    - 定义了8种状态（SS_ORIGINAL到SS_Holiday）
 *    - 状态之间有明确的转换条件和路径
 *    - 基于时间和交易日历进行状态转换
 *    - 每个状态对应特定的系统行为
 * 
 * 2. 时间驱动的状态转换：
 *    - 每秒检查一次当前时间
 *    - 根据时间和交易日历自动转换状态
 *    - 处理复杂的时间偏移（夜盘跨日等）
 *    - 支持多交易时段的独立管理
 * 
 * 3. 交易日历集成：
 *    - 与BaseDataMgr协作查询交易日历
 *    - 自动识别节假日
 *    - 处理不同品种的交易日差异
 *    - 支持提前休市、延迟开市等特殊情况
 * 
 * 4. 精细的时段划分：
 *    - 初始化时间：数据接收系统启动时间
 *    - 集合竞价时间：盘前撮合时间
 *    - 连续竞价时间：正常交易时间（可能有多个时段）
 *    - 收盘时间：停止接收行情数据时间
 *    - 盘后处理时间：转储历史数据时间
 * 
 * 状态转换图：
 * 
 *   [原始状态] SS_ORIGINAL
 *        |
 *        | 到达初始化时间
 *        ↓
 *   [已初始化] SS_INITIALIZED
 *        |
 *        | 到达集合竞价时间
 *        ↓
 *   [接收中] SS_RECEIVING ←─┐
 *        |                   │
 *        | 中途休盘          │ 恢复交易
 *        ↓                   │
 *   [暂停] SS_PAUSED ────────┘
 *        |
 *        | 到达收盘时间
 *        ↓
 *   [已收盘] SS_CLOSED
 *        |
 *        | 到达盘后处理时间
 *        ↓
 *   [处理中] SS_PROCING
 *        |
 *        | 处理完成
 *        ↓
 *   [已处理] SS_PROCED
 *        |
 *        | 下一交易日的初始化时间前
 *        ↓
 *   [原始状态] SS_ORIGINAL
 * 
 *   如果检测到节假日：
 *   任意状态 → [节假日] SS_Holiday → 下一交易日 → SS_ORIGINAL
 * 
 * 主要功能模块：
 * 
 * 1. 状态定义与管理：
 *    - SimpleState枚举：定义所有可能的状态
 *    - StateInfo结构：存储每个时段的状态信息
 *    - StateMap映射：管理多个时段的状态
 * 
 * 2. 时间区间管理：
 *    - Section结构：定义时间区间
 *    - 支持多个交易时段（如上午、下午）
 *    - 处理连续竞价和集合竞价时间
 * 
 * 3. 状态查询接口：
 *    - isInState()：检查指定时段是否处于某状态
 *    - isAnyInState()：检查是否有任何时段处于某状态
 *    - isAllInState()：检查是否所有时段都处于某状态
 * 
 * 4. 状态转换控制：
 *    - run()：启动状态监控线程
 *    - stop()：停止状态监控
 *    - 每秒检查并更新状态
 * 
 * 应用场景：
 * - 控制盘前不接收数据
 * - 控制盘后停止接收
 * - 控制中途休盘时间
 * - 触发盘后数据处理
 * - 识别节假日
 * 
 * 技术特点：
 * - 独立线程运行，不阻塞主流程
 * - 高精度时间管理（秒级）
 * - 复杂的时间偏移计算
 * - 多时段并发管理
 */

#pragma once                                                // 防止头文件重复包含
#include <vector>                                           // STL向量容器
#include "../Share/StdUtils.hpp"                            // 标准工具类（智能指针等）
#include "../Includes/FasterDefs.h"                         // 快速数据结构定义
#include "../Includes/WTSMarcos.h"                          // WonderTrader宏定义

// 前向声明
NS_WTP_BEGIN                                                // 开始WonderTrader命名空间
class WTSSessionInfo;                                       // 交易时段信息类
NS_WTP_END                                                  // 结束WonderTrader命名空间

USING_NS_WTP;                                               // 使用WonderTrader命名空间

/**
 * @enum tagSimpleState
 * @brief 交易时段状态枚举（状态机的状态定义）
 * 
 * 定义了交易时段可能的所有状态，每个状态对应特定的系统行为。
 * 状态转换基于时间触发，形成一个完整的状态机。
 * 
 * 状态转换规则：
 * - 顺序转换：SS_ORIGINAL → ... → SS_PROCED
 * - 循环转换：SS_PROCED → SS_ORIGINAL（下一交易日）
 * - 异常转换：任意状态 → SS_Holiday（检测到节假日）
 * 
 * 每个状态的含义和行为：
 * 
 * 1. SS_ORIGINAL（未初始化）：
 *    - 系统启动或新交易日开始前的状态
 *    - 等待初始化时间到达
 *    - 不接收数据，不进行任何处理
 * 
 * 2. SS_INITIALIZED（已初始化）：
 *    - 系统已完成初始化，等待开盘
 *    - 数据接收系统已就绪
 *    - 还未到集合竞价时间
 *    - 不接收数据
 * 
 * 3. SS_RECEIVING（接收中/交易中）：
 *    - 正在接收实时行情数据
 *    - 对应交易时间段
 *    - 数据正常写入和广播
 *    - 最重要的工作状态
 * 
 * 4. SS_PAUSED（暂停/休息中）：
 *    - 中途休盘时间（如午休）
 *    - 不接收数据
 *    - 等待下一交易时段开始
 * 
 * 5. SS_CLOSED（已收盘）：
 *    - 交易结束，停止接收数据
 *    - 等待盘后处理时间
 *    - 可能还在接收结算价等收盘数据
 * 
 * 6. SS_PROCING（收盘作业中/处理中）：
 *    - 正在执行盘后数据处理
 *    - 转储实时数据为历史数据
 *    - 短暂的过渡状态
 * 
 * 7. SS_PROCED（盘后已处理）：
 *    - 盘后处理完成
 *    - 等待下一交易日
 *    - 数据已归档
 * 
 * 8. SS_Holiday（节假日）：
 *    - 特殊状态，枚举值为99
 *    - 当天是非交易日
 *    - 不进行任何数据操作
 *    - 等待下一交易日
 * 
 * @note SS_Holiday使用特殊值99便于区分
 * @note 状态转换在StateMonitor::run()的监控线程中执行
 */
typedef enum tagSimpleState
{
	SS_ORIGINAL,		///< 未初始化状态（0）- 交易日开始前
	SS_INITIALIZED,		///< 已初始化状态（1）- 系统就绪等待开盘
	SS_RECEIVING,		///< 交易中状态（2）- 正在接收行情数据
	SS_PAUSED,			///< 休息中状态（3）- 中途休盘时间
	SS_CLOSED,			///< 已收盘状态（4）- 停止接收数据
	SS_PROCING,			///< 收盘作业中状态（5）- 正在转储历史数据
	SS_PROCED,			///< 盘后已处理状态（6）- 数据已归档
	SS_Holiday	= 99	///< 节假日状态（99）- 非交易日
} SimpleState;

/**
 * @struct _StateInfo
 * @brief 交易时段状态信息结构体
 * 
 * 该结构体存储单个交易时段的完整状态信息，包括时间配置、当前状态、
 * 交易区间等。每个交易时段（如"TRADING"、"NIGHT"）都有一个StateInfo实例。
 * 
 * 数据成员详解：
 * 
 * 1. 基本标识：
 *    - _session：交易时段ID（如"TRADING"、"ALLDAY"）
 *    - _sInfo：对应的交易时段详细信息
 * 
 * 2. 时间配置（单位：HHMM格式）：
 *    - _init_time：初始化时间（如0830表示8:30）
 *    - _close_time：收盘时间（如1505表示15:05）
 *    - _proc_time：盘后处理时间（如1530表示15:30）
 * 
 * 3. 状态信息：
 *    - _state：当前状态（SimpleState枚举值）
 *    - _sections：交易时间区间集合
 * 
 * 内嵌结构Section：
 * - 定义时间区间（开始时间和结束时间）
 * - 支持多个不连续的交易时段
 * - 例如：上午9:30-11:30，下午13:00-15:00
 */
typedef struct _StateInfo
{
	char		_session[16];           ///< 交易时段标识符（如"TRADING"），最大15字符+'\0'
	uint32_t	_init_time;             ///< 初始化时间，格式HHMM（如0830表示8:30）
	uint32_t	_close_time;            ///< 收盘时间，格式HHMM（如1505表示15:05）
	uint32_t	_proc_time;             ///< 盘后处理时间，格式HHMM（如1530表示15:30）
	SimpleState	_state;                 ///< 当前状态（状态机的当前状态）
	WTSSessionInfo*	_sInfo;             ///< 交易时段详细信息指针（包含完整的时段配置）

	/**
	 * @struct _Section
	 * @brief 时间区间结构（表示一个连续的交易时段）
	 * 
	 * 定义了一个时间区间，用于判断某个时间点是否在交易时间内。
	 * 多个Section组成完整的交易时间。
	 * 
	 * 示例：
	 * - 股票：Section{0930, 1130}, Section{1300, 1500}
	 * - 期货：Section{0900, 1015}, Section{1030, 1130}, Section{1330, 1500}
	 * - 夜盘：Section{2100, 2300}（偏移后的时间）
	 */
	typedef struct _Section
	{
		uint32_t _from;                 ///< 区间开始时间，格式HHMM
		uint32_t _end;                  ///< 区间结束时间，格式HHMM
	} Section;
	
	std::vector<Section> _sections;     ///< 交易时间区间集合（支持多个不连续时段）

	/**
	 * @brief 判断指定时间是否在交易时间区间内（内联函数）
	 * 
	 * 该方法遍历所有交易时间区间，判断给定时间是否落在任一区间内。
	 * 
	 * 判断逻辑：
	 * - 遍历_sections中的所有Section
	 * - 检查 curTime 是否满足：from <= curTime < end
	 * - 任一区间满足条件即返回true
	 * - 所有区间都不满足则返回false
	 * 
	 * 时间格式：
	 * - 输入格式：HHMM（如0930表示9:30）
	 * - 闭开区间：[from, end)，包含from但不包含end
	 * 
	 * 使用场景：
	 * - 判断是否应该从SS_PAUSED恢复到SS_RECEIVING
	 * - 判断是否应该从SS_RECEIVING转换到SS_PAUSED
	 * 
	 * @param curTime 当前时间，格式HHMM
	 * @return bool 在交易时间内返回true，否则返回false
	 * 
	 * @note 内联函数，编译器会优化为内联代码
	 */
	inline bool isInSections(uint32_t curTime)
	{
		// 遍历所有时间区间
		for (auto it = _sections.begin(); it != _sections.end(); it++)
		{
			const Section& sec = *it;               // 获取当前区间
			
			// 判断时间是否在当前区间内
			// 使用左闭右开区间：[_from, _end)
			if (sec._from <= curTime && curTime < sec._end)
				return true;                        // 找到匹配的区间，返回true
		}
		
		// 所有区间都不匹配，不在交易时间内
		return false;
	}

	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化StateInfo结构体的所有成员为默认值，确保对象处于安全状态。
	 */
	_StateInfo()
	{
		_session[0] = '\0';                     // 将session字符串初始化为空字符串
		_init_time = 0;                         // 初始化时间设为0
		_close_time = 0;                        // 收盘时间设为0
		_proc_time = 0;                         // 盘后处理时间设为0
		_state = SS_ORIGINAL;                   // 初始状态设为"未初始化"
		_sInfo = nullptr;                       // 交易时段信息指针设为空（C++11风格）
		// _sections使用默认构造函数，初始化为空vector
	}
} StateInfo;

// 类型别名定义，简化代码
typedef std::shared_ptr<StateInfo> StatePtr;                        ///< StateInfo的智能指针类型
typedef wtp::wt_hashmap<std::string, StatePtr>	StateMap;           ///< 时段ID到StateInfo的映射表类型

// 前向声明
class WTSBaseDataMgr;                                               // 基础数据管理器
class DataManager;                                                  // 数据管理器

/**
 * @class StateMonitor
 * @brief 交易时段状态监控器类
 * 
 * 该类实现了一个完整的状态机系统，用于管理多个交易时段的状态。
 * 通过独立线程每秒检查时间和交易日历，自动进行状态转换。
 * 
 * 主要职责：
 * 
 * 1. 状态初始化：
 *    - 从配置文件加载状态控制规则
 *    - 为每个交易时段创建StateInfo
 *    - 设置初始化、收盘、处理时间
 *    - 构建交易时间区间
 * 
 * 2. 状态监控：
 *    - 启动独立的监控线程
 *    - 每秒检查当前时间
 *    - 根据时间和日历自动转换状态
 *    - 处理复杂的跨日逻辑
 * 
 * 3. 状态查询：
 *    - 提供多种状态查询方法
 *    - 支持指定时段查询
 *    - 支持全局状态查询
 * 
 * 4. 事件触发：
 *    - 状态转换时记录日志
 *    - 在适当时机触发数据转储
 *    - 协调DataManager的数据处理
 * 
 * 典型配置示例：
 * @code
 *   {
 *     "TRADING": {                  // 交易时段ID
 *       "inittime": 830,             // 初始化时间 8:30
 *       "closetime": 1505,           // 收盘时间 15:05
 *       "proctime": 1530             // 盘后处理时间 15:30
 *     }
 *   }
 * @endcode
 * 
 * 工作流程：
 * 1. initialize()：加载配置，创建状态信息
 * 2. run()：启动监控线程
 * 3. 线程每秒执行一次状态检查和转换
 * 4. stop()：停止监控线程
 * 
 * 线程安全性：
 * - 监控线程读取状态
 * - 查询方法读取状态
 * - 没有写写冲突（只有监控线程写入）
 * - 读操作是const的，多线程安全
 * 
 * 性能特点：
 * - 每秒一次检查，开销极小
 * - 状态查询是O(1)或O(n)操作
 * - 不会成为系统瓶颈
 */
class StateMonitor
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化StateMonitor对象的基本状态。
	 */
	StateMonitor();
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理StateMonitor对象。应该在析构前调用stop()停止监控线程。
	 */
	~StateMonitor();

public:
	/**
	 * @brief 初始化状态监控器
	 * 
	 * 从配置文件加载状态控制规则，为每个交易时段创建状态信息。
	 * 
	 * 初始化步骤：
	 * 1. 读取配置文件
	 * 2. 遍历所有交易时段配置
	 * 3. 创建StateInfo并设置时间参数
	 * 4. 从WTSSessionInfo提取交易时间区间
	 * 5. 处理时间偏移（将偏移分钟转换为标准时间）
	 * 6. 初始化交易日信息
	 * 
	 * @param filename 状态配置文件路径
	 * @param bdMgr 基础数据管理器指针
	 * @param dtMgr 数据管理器指针
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * @see run() 启动监控
	 */
	bool initialize(const char* filename, WTSBaseDataMgr* bdMgr, DataManager* dtMgr);
	
	/**
	 * @brief 启动状态监控线程
	 * 
	 * 创建并启动独立的监控线程，线程每秒检查一次时间和状态。
	 * 
	 * 线程执行逻辑：
	 * 1. 每秒检查一次（实际是1000ms间隔）
	 * 2. 获取当前日期和时间
	 * 3. 遍历所有交易时段
	 * 4. 根据当前状态执行对应的转换逻辑
	 * 5. 记录状态转换日志
	 * 6. 在适当时机触发数据转储
	 * 
	 * @note 该方法会创建线程，非阻塞
	 * @note 调用前应该先调用initialize()
	 * 
	 * @see stop() 停止监控
	 */
	void run();
	
	/**
	 * @brief 停止状态监控
	 * 
	 * 设置停止标志，等待监控线程结束。
	 * 
	 * @note 会阻塞直到线程完全停止
	 * @note 应该在程序退出前调用
	 */
	void stop();

	/**
	 * @brief 检查是否有任何时段处于指定状态（内联函数）
	 * 
	 * 该方法遍历所有交易时段，检查是否至少有一个时段处于指定状态。
	 * 
	 * 使用场景：
	 * - 检查是否有任何时段正在接收数据
	 * - 检查是否有任何时段正在处理
	 * 
	 * @param ss 要检查的状态
	 * @return bool 至少有一个时段处于该状态返回true，否则返回false
	 * 
	 * @note const方法，不修改对象状态
	 */
	inline bool	isAnyInState(SimpleState ss) const
	{
		// 遍历状态映射表
		auto it = _map.begin();
		for (; it != _map.end(); it++)
		{
			const StatePtr& sInfo = it->second;     // 获取StateInfo智能指针
			if (sInfo->_state == ss)                // 检查状态是否匹配
				return true;                        // 找到匹配的，立即返回true
		}

		// 没有任何时段处于该状态
		return false;
	}

	/**
	 * @brief 检查是否所有时段都处于指定状态（内联函数）
	 * 
	 * 该方法遍历所有交易时段，检查是否所有时段都处于指定状态。
	 * 
	 * 特殊处理：
	 * - SS_Holiday状态的时段会被忽略
	 * - 只检查非节假日时段
	 * 
	 * 使用场景：
	 * - 检查是否所有时段都处理完成（SS_PROCING）
	 * - 确定是否可以清理全局缓存
	 * 
	 * @param ss 要检查的状态
	 * @return bool 所有非节假日时段都处于该状态返回true，否则返回false
	 * 
	 * @note const方法，不修改对象状态
	 * @note 节假日时段不参与判断
	 */
	inline bool	isAllInState(SimpleState ss) const
	{
		// 遍历状态映射表
		auto it = _map.begin();
		for (; it != _map.end(); it++)
		{
			const StatePtr& sInfo = it->second;     // 获取StateInfo智能指针
			
			// 如果当前时段不是节假日，且状态不等于指定状态
			// 说明不是所有时段都处于指定状态
			if (sInfo->_state != SS_Holiday && sInfo->_state != ss)
				return false;                       // 找到不匹配的，返回false
		}

		// 所有非节假日时段都处于指定状态
		return true;
	}

	/**
	 * @brief 检查指定时段是否处于指定状态（内联函数）
	 * 
	 * 该方法查询特定交易时段的当前状态。这是最常用的状态查询方法。
	 * 
	 * 查询逻辑：
	 * 1. 在映射表中查找sid对应的StateInfo
	 * 2. 如果找不到，返回false
	 * 3. 如果找到，比较状态是否匹配
	 * 
	 * 使用场景：
	 * - DataManager::canSessionReceive()检查是否可接收数据
	 * - 判断特定时段的处理状态
	 * 
	 * @param sid 交易时段标识符（如"TRADING"）
	 * @param ss 要检查的状态
	 * @return bool 时段存在且状态匹配返回true，否则返回false
	 * 
	 * @note const方法，多线程读取安全
	 * @note 时段ID不存在时返回false
	 */
	inline bool	isInState(const char* sid, SimpleState ss) const
	{
		// 在哈希映射表中查找sid对应的StateInfo
		auto it = _map.find(sid);
		if (it == _map.end())                       // 如果找不到该时段
			return false;                           // 返回false

		// 获取StateInfo智能指针
		const StatePtr& sInfo = it->second;
		
		// 比较状态是否匹配
		return sInfo->_state == ss;
	}

private:
	/**
	 * @brief 状态映射表
	 * 
	 * 存储所有交易时段的状态信息。
	 * - Key：交易时段ID（如"TRADING"、"NIGHT"）
	 * - Value：StateInfo智能指针
	 * 
	 * 使用hashmap：
	 * - O(1)查询性能
	 * - 支持动态添加时段
	 * - 自动管理内存（智能指针）
	 */
	StateMap		_map;
	
	/**
	 * @brief 基础数据管理器指针
	 * 
	 * 用于：
	 * - 获取交易时段信息（WTSSessionInfo）
	 * - 查询交易日历（是否交易日）
	 * - 获取时段对应的品种集合
	 * - 设置和获取交易日
	 */
	WTSBaseDataMgr*	_bd_mgr;
	
	/**
	 * @brief 数据管理器指针
	 * 
	 * 用于：
	 * - 触发历史数据转储（transHisData）
	 * - 查询处理状态（isSessionProceeded）
	 */
	DataManager*	_dt_mgr;

	/**
	 * @brief 监控线程智能指针
	 * 
	 * 指向状态监控线程，线程每秒检查一次状态并执行转换。
	 * 使用智能指针自动管理线程生命周期。
	 */
	StdThreadPtr	_thrd;

	/**
	 * @brief 停止标志
	 * 
	 * 控制监控线程的运行。
	 * - false：线程继续运行
	 * - true：线程应该退出
	 * 
	 * @note 由主线程设置，监控线程读取
	 */
	bool			_stopped;
};

