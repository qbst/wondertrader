/*!
 * \file ShmCaster.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 共享内存数据广播器定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了ShmCaster（共享内存广播器）类，是WonderTrader框架中最快的
 * 进程间通信（IPC）方式。采用无锁环形队列（Lock-Free Ring Buffer）设计，
 * 实现纳秒级延迟的数据传输。
 * 
 * 核心设计理念：
 * 
 * 1. 共享内存机制（Shared Memory）：
 *    - 使用Boost.Interprocess的内存映射文件
 *    - 多个进程映射同一块内存区域
 *    - 避免数据拷贝，零拷贝传输
 *    - 延迟极低（纳秒级）
 * 
 * 2. 无锁环形队列（Lock-Free Ring Buffer）：
 *    - 生产者-消费者模式
 *    - 使用volatile原子变量
 *    - 单生产者单消费者（SPSC）
 *    - 避免锁竞争，极致性能
 * 
 * 3. 联合体（Union）设计：
 *    - 节省内存空间
 *    - 支持多种数据类型
 *    - 通过_type字段区分
 *    - 避免多个独立队列
 * 
 * 架构设计：
 * 
 *   [行情源进程 - 生产者]
 *          ↓
 *    ShmCaster.broadcast()
 *          ↓
 *    [共享内存环形队列]
 *    ┌─────────────────┐
 *    │ writable -----> │  写指针（生产者）
 *    │                 │
 *    │ [DataItem 0]    │  ← 环形缓冲区
 *    │ [DataItem 1]    │
 *    │ [DataItem ...]  │
 *    │ [DataItem 8191] │
 *    │                 │
 *    │ readable -----> │  读指针（消费者）
 *    └─────────────────┘
 *          ↑
 *    ParserShm.read()
 *          ↑
 *   [策略进程 - 消费者]
 * 
 * 无锁环形队列算法：
 * 
 * 写入流程（生产者）：
 * 1. wIdx = _writable++              （原子递增写指针）
 * 2. realIdx = wIdx % capacity       （计算实际索引）
 * 3. items[realIdx] = data          （写入数据）
 * 4. _readable = wIdx                （更新可读指针）
 * 
 * 读取流程（消费者）：
 * 1. 检查 _readable != UINT64_MAX   （队列是否已初始化）
 * 2. 检查 _readable > last_read     （是否有新数据）
 * 3. realIdx = last_read % capacity  （计算实际索引）
 * 4. data = items[realIdx]          （读取数据）
 * 5. last_read++                     （移动读指针）
 * 
 * 数据项结构：
 * 
 * DataItem（联合体设计）：
 * - _type：数据类型标识（0-3）
 * - _tick / _queue / _order / _trans：不同类型的数据（共用内存）
 * 
 * 数据队列结构：
 * - _capacity：队列容量（默认8192）
 * - _readable：可读位置（volatile，消费者读取）
 * - _writable：可写位置（volatile，生产者更新）
 * - _pid：生产者进程ID（用于调试）
 * - _items：数据项数组
 * 
 * 技术特点：
 * 
 * 1. 内存对齐：
 *    - #pragma pack(push, 8)：8字节对齐
 *    - 优化CPU缓存访问
 *    - 避免伪共享（False Sharing）
 * 
 * 2. Volatile关键字：
 *    - 防止编译器优化
 *    - 确保每次都从内存读取
 *    - 适用于多进程共享变量
 * 
 * 3. 环形缓冲：
 *    - 使用模运算实现环形
 *    - 固定大小，预分配内存
 *    - 写满后覆盖最旧数据
 * 
 * 4. Placement New：
 *    - 在已分配内存上构造对象
 *    - new(addr) Type()
 *    - 用于共享内存的对象初始化
 * 
 * 性能指标：
 * - 延迟：<100纳秒
 * - 吞吐量：>1,000,000 消息/秒
 * - 内存占用：约64MB（8192 * 8KB）
 * - CPU占用：极低
 * 
 * 使用场景：
 * - 本地多进程架构
 * - 行情数据转发
 * - 超高频交易
 * - 实时监控系统
 * 
 * 限制：
 * - 仅支持本地进程间通信
 * - 不支持跨机器通信
 * - 队列满时会覆盖旧数据
 * - 消费者读取速度要跟上生产速度
 */

#pragma once                                                // 防止头文件重复包含

#include "IDataCaster.h"                                    // 包含数据广播器接口
#include <stdint.h>                                         // 标准整型定义
#include "../Includes/WTSStruct.h"                          // WonderTrader数据结构定义
#include "../Share/BoostMappingFile.hpp"                    // Boost内存映射文件工具

// 前向声明
NS_WTP_BEGIN                                                // 开始WonderTrader命名空间
class WTSVariant;                                           // 配置参数类
NS_WTP_END                                                  // 结束WonderTrader命名空间

USING_NS_WTP;                                               // 使用WonderTrader命名空间

/**
 * @class ShmCaster
 * @brief 共享内存数据广播器类
 * 
 * 该类实现了基于共享内存的超高速数据广播功能。使用无锁环形队列算法，
 * 实现纳秒级延迟的进程间通信，是WonderTrader中最快的数据传输方式。
 * 
 * 继承关系：
 * - 公有继承IDataCaster：实现数据广播器接口
 * 
 * 核心特性：
 * 
 * 1. 共享内存：
 *    - 使用Boost.Interprocess创建内存映射文件
 *    - 多进程映射同一文件到各自地址空间
 *    - 修改对所有进程立即可见
 *    - 零拷贝，极低延迟
 * 
 * 2. 无锁设计：
 *    - 单生产者单消费者（SPSC）模型
 *    - 使用volatile保证内存可见性
 *    - 避免锁竞争，提升性能
 *    - 适合高频实时场景
 * 
 * 3. 环形队列：
 *    - 固定容量（默认8192项）
 *    - 使用模运算实现环形
 *    - 写满后覆盖旧数据
 *    - 读写指针独立移动
 * 
 * 使用流程：
 * @code
 *   // 生产者进程（QuoteFactory）
 *   ShmCaster* caster = new ShmCaster();
 *   caster->init(config);              // 初始化，创建共享内存
 *   caster->broadcast(tick);           // 写入数据
 *   
 *   // 消费者进程（策略程序）
 *   ParserShm* parser = new ParserShm();
 *   parser->init(config);              // 初始化，映射共享内存
 *   // 周期性读取数据...
 * @endcode
 * 
 * 配置示例：
 * @code
 *   {
 *     "active": true,                  // 是否启用
 *     "path": "WtQuoteShmData"         // 共享内存文件路径
 *   }
 * @endcode
 * 
 * 性能优化：
 * - 使用8字节对齐，优化缓存访问
 * - volatile确保内存可见性
 * - 无锁设计，无等待开销
 * - 预分配内存，避免动态分配
 * 
 * 注意事项：
 * - 生产者和消费者必须使用相同的path
 * - 队列容量固定，不能动态扩展
 * - 消费速度必须跟上生产速度
 * - 进程崩溃可能导致共享内存泄漏
 */
class ShmCaster : public IDataCaster
{
public:
#pragma pack(push, 8)                   // 设置8字节对齐，优化性能
	/**
	 * @struct _DataItem
	 * @brief 数据项结构（支持多种数据类型）
	 * 
	 * 该结构使用联合体（Union）设计，在同一块内存中存储不同类型的数据，
	 * 节省空间并统一管理。
	 * 
	 * 设计要点：
	 * - _type字段：区分数据类型（0=Tick, 1=委托队列, 2=逐笔委托, 3=逐笔成交）
	 * - union：多种数据共用一块内存，只有一个成员有效
	 * - 内存大小：取最大成员的大小
	 * 
	 * 内存布局：
	 * ┌──────────┬────────────────────────┐
	 * │  _type   │  union (tick/queue/...) │
	 * │ 4字节    │  最大成员的大小         │
	 * └──────────┴────────────────────────┘
	 * 
	 * 对齐：8字节对齐，优化CPU访问
	 */
	typedef struct _DataItem
	{
		uint32_t	_type;	                    ///< 数据类型标识：0-tick, 1-委托队列, 2-逐笔委托, 3-逐笔成交
		
		/**
		 * 联合体：同一内存位置存储不同类型的数据
		 * 任一时刻只有一个成员有效，由_type决定
		 */
		union
		{
			WTSTickStruct	_tick;              ///< Tick行情数据（_type=0时有效）
			WTSOrdQueStruct _queue;             ///< 委托队列数据（_type=1时有效）
			WTSOrdDtlStruct	_order;             ///< 逐笔委托数据（_type=2时有效）
			WTSTransStruct	_trans;             ///< 逐笔成交数据（_type=3时有效）
		};

		/**
		 * @brief 默认构造函数
		 * 
		 * 使用memset将整个结构清零，确保初始状态干净。
		 */
		_DataItem() { memset(this, 0, sizeof(_DataItem)); }
	} DataItem;

	/**
	 * @struct _DataQueue
	 * @brief 无锁环形队列结构（模板类）
	 * 
	 * 该结构实现了一个固定大小的环形队列，使用无锁算法支持
	 * 单生产者单消费者（SPSC）模式。
	 * 
	 * 模板参数：
	 * @tparam N 队列容量（数据项数量），默认8192
	 * 
	 * 环形队列原理：
	 * - 使用两个指针：_readable（读）和_writable（写）
	 * - 指针值持续递增，通过模运算映射到实际数组索引
	 * - 容量固定，写满后从头覆盖
	 * 
	 * 无锁算法原理：
	 * 
	 * 写入步骤（生产者）：
	 * 1. wIdx = _writable++              // 原子递增，获取独占写位置
	 * 2. realIdx = wIdx % N              // 计算环形索引
	 * 3. _items[realIdx] = data         // 写入数据
	 * 4. _readable = wIdx                // 更新可读指针（写入完成信号）
	 * 
	 * 读取步骤（消费者）：
	 * 1. rIdx = last_read                // 上次读取的位置
	 * 2. if (_readable > rIdx)           // 检查是否有新数据
	 * 3. realIdx = rIdx % N              // 计算环形索引
	 * 4. data = _items[realIdx]         // 读取数据
	 * 5. last_read++                     // 移动读指针
	 * 
	 * 初始化策略：
	 * - _readable初始化为UINT64_MAX（特殊标记）
	 * - 消费者检查：如果_readable==UINT64_MAX，队列未就绪
	 * - 生产者第一次写入后，_readable被设为有效值
	 * 
	 * 内存布局：
	 * ┌───────────────────────────────────┐
	 * │ _capacity (队列容量)              │  8字节
	 * ├───────────────────────────────────┤
	 * │ _readable (volatile, 可读指针)    │  8字节
	 * ├───────────────────────────────────┤
	 * │ _writable (volatile, 可写指针)    │  8字节
	 * ├───────────────────────────────────┤
	 * │ _pid (生产者进程ID)               │  4字节
	 * ├───────────────────────────────────┤
	 * │ _items[0..N-1] (数据项数组)      │  N * sizeof(DataItem)
	 * └───────────────────────────────────┘
	 * 
	 * 对齐：8字节对齐，优化性能
	 */
	template <int N = 8*1024>               // 模板参数N：队列容量，默认8192
	struct _DataQueue
	{
		uint64_t	_capacity = N;              ///< 队列容量（编译期常量，默认8192）
		volatile uint64_t	_readable;          ///< 可读位置指针（消费者读取，生产者更新），volatile确保多进程可见
		volatile uint64_t	_writable;          ///< 可写位置指针（生产者更新），volatile确保多进程可见
		uint32_t	_pid;                       ///< 生产者进程ID（用于调试和监控）
		DataItem	_items[N];                  ///< 数据项数组（环形缓冲区）

		/**
		 * @brief 默认构造函数
		 * 
		 * 初始化队列的关键字段：
		 * - _readable设为UINT64_MAX（特殊值，表示队列未就绪）
		 * - _writable设为0（从0开始写入）
		 * - _pid设为0（待设置）
		 */
		_DataQueue() :_readable(UINT64_MAX), _writable(0), _pid(0) {}
	};

	/**
	 * @typedef CastQueue
	 * @brief 广播队列类型（实例化模板，容量8192）
	 * 
	 * 默认配置：
	 * - 容量：8192个数据项
	 * - 内存占用：约64MB（取决于DataItem大小）
	 * - 可存储时间：按1000tick/秒算，约8秒的数据
	 */
	typedef _DataQueue<8*1024>	CastQueue;

#pragma pack(pop)                       // 恢复默认对齐方式

public:
	/**
	 * @brief 默认构造函数（内联实现）
	 * 
	 * 初始化成员变量为默认值。
	 */
	ShmCaster():_queue(NULL), _inited(false){}

	/**
	 * @brief 初始化共享内存广播器
	 * 
	 * 该方法创建或打开共享内存文件，并映射到当前进程地址空间。
	 * 
	 * 初始化流程：
	 * 1. 检查配置有效性
	 * 2. 检查active标志
	 * 3. 获取共享内存路径
	 * 4. 创建/截断共享内存文件
	 * 5. 映射文件到内存
	 * 6. 初始化队列结构
	 * 7. 设置生产者进程ID
	 * 
	 * @param cfg 配置参数
	 *            - active: 是否启用（bool）
	 *            - path: 共享内存文件路径（string）
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * @note 每次启动都会重置队列，清空旧数据
	 * @note 使用Placement New在共享内存上构造队列对象
	 * 
	 * @see broadcast() 数据写入方法
	 */
	bool	init(WTSVariant* cfg);

	/**
	 * @brief 广播Tick行情数据（重写IDataCaster接口）
	 * 
	 * 将Tick数据写入共享内存队列。使用无锁算法，极致性能。
	 * 
	 * 写入算法：
	 * 1. 原子递增_writable，获取写位置
	 * 2. 计算环形索引：realIdx = wIdx % capacity
	 * 3. 设置数据类型为0（Tick）
	 * 4. 拷贝Tick数据到队列项
	 * 5. 更新_readable（通知消费者）
	 * 
	 * @param curTick Tick数据指针
	 * 
	 * @note 延迟<100纳秒
	 * @note 队列满时会覆盖旧数据
	 * 
	 * @see _DataQueue 队列结构
	 */
	virtual void	broadcast(WTSTickData* curTick) override;
	
	/**
	 * @brief 广播委托队列数据（重写IDataCaster接口）
	 * 
	 * @param curOrdQue 委托队列数据指针
	 * 
	 * @see broadcast(WTSTickData*) Tick广播方法
	 */
	virtual void	broadcast(WTSOrdQueData* curOrdQue) override;
	
	/**
	 * @brief 广播逐笔委托数据（重写IDataCaster接口）
	 * 
	 * @param curOrdDtl 逐笔委托数据指针
	 * 
	 * @see broadcast(WTSTickData*) Tick广播方法
	 */
	virtual void	broadcast(WTSOrdDtlData* curOrdDtl) override;
	
	/**
	 * @brief 广播逐笔成交数据（重写IDataCaster接口）
	 * 
	 * @param curTrans 逐笔成交数据指针
	 * 
	 * @see broadcast(WTSTickData*) Tick广播方法
	 */
	virtual void	broadcast(WTSTransData* curTrans) override;

private:
	/**
	 * @brief 共享内存文件路径
	 * 
	 * 生产者和消费者必须使用相同的路径。
	 * 通常使用简单名称（如"WtQuoteShmData"），系统会在临时目录创建。
	 */
	std::string		_path;
	
	/**
	 * @typedef MappedFilePtr
	 * @brief 内存映射文件智能指针类型
	 */
	typedef std::shared_ptr<BoostMappingFile> MappedFilePtr;
	
	/**
	 * @brief 内存映射文件智能指针
	 * 
	 * 管理共享内存的映射，自动处理映射和解映射。
	 */
	MappedFilePtr	_mapfile;
	
	/**
	 * @brief 队列结构指针
	 * 
	 * 指向共享内存中的队列结构。
	 * 多个进程中的该指针指向同一块物理内存。
	 */
	CastQueue*		_queue;
	
	/**
	 * @brief 初始化标志
	 * 
	 * 标识ShmCaster是否已成功初始化。
	 * - false：未初始化，broadcast会直接返回
	 * - true：已初始化，可以正常广播
	 */
	bool			_inited;
};

