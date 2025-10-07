/*!
 * \file StatHelper.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据统计辅助类定义（Header-Only）
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了StatHelper统计辅助类，用于收集和管理WonderTrader数据传输层的
 * 统计信息。采用Header-Only设计，所有实现都包含在头文件中。使用单例模式
 * （Singleton Pattern）确保全局唯一性。
 * 
 * 核心设计理念：
 * 
 * 1. 统计信息管理：
 *    - 接收包数量统计（_recv_packs）
 *    - 发送包数量统计（_send_packs）
 *    - 发送字节数统计（_send_bytes）
 *    - 支持多种统计类型（当前主要是ST_BROADCAST）
 * 
 * 2. 单例模式实现：
 *    - 使用Meyers' Singleton（C++11线程安全）
 *    - 通过static局部变量实现懒加载
 *    - 保证全局唯一的统计实例
 *    - 避免全局变量的初始化顺序问题
 * 
 * 3. 线程安全设计：
 *    - 使用Boost读写锁（BoostRWMutex）保护数据
 *    - 读操作使用读锁（BoostReadLock）允许并发
 *    - 写操作使用写锁（BoostWriteLock）独占访问
 *    - 适合读多写少的场景
 * 
 * 4. 数据溢出保护：
 *    - 检测UINT64_MAX溢出
 *    - 溢出时重置计数器
 *    - 避免数据回绕导致的错误统计
 * 
 * 架构设计：
 * 
 *   [UDP广播]  [共享内存]  [其他广播器]
 *        |         |            |
 *        +-------- + -----------+
 *                  |
 *            [StatHelper]  <-- 单例，全局唯一
 *                  |
 *        +---------+---------+
 *        |                   |
 *   [统计信息查询]      [监控展示]
 * 
 * 统计类型说明：
 * - ST_BROADCAST：广播统计（UDP/共享内存等数据分发）
 * - 未来可扩展：ST_PARSER、ST_TRADER、ST_ENGINE等
 * 
 * 使用场景：
 * 
 * 1. 性能监控：
 *    - 监控数据吞吐量
 *    - 检测数据传输瓶颈
 *    - 评估系统负载
 * 
 * 2. 运维分析：
 *    - 统计每日数据量
 *    - 分析传输模式
 *    - 生成运营报表
 * 
 * 3. 故障诊断：
 *    - 发现数据丢失
 *    - 定位性能问题
 *    - 分析异常流量
 * 
 * 4. 容量规划：
 *    - 评估带宽需求
 *    - 预测存储需求
 *    - 规划扩容方案
 * 
 * 技术特点：
 * - Header-Only：简化编译和链接
 * - 单例模式：全局唯一实例
 * - 读写锁：高并发性能优化
 * - 溢出保护：数据安全性保障
 * - 类型安全：枚举类型定义
 * 
 * 性能考虑：
 * - 读写锁开销：比互斥锁更适合读多写少场景
 * - 内存占用：每种统计类型20字节（3个计数器）
 * - 锁竞争：高频更新时可能成为瓶颈
 * - 缓存友好：紧凑的数据结构设计
 */

#pragma once                                                    // 防止头文件重复包含

#include <boost/atomic.hpp>                                     // Boost原子操作库（未使用，可能是历史遗留）
#include <boost/interprocess/detail/atomic.hpp>                 // Boost进程间原子操作（未使用）

/**
 * @class StatHelper
 * @brief 数据统计辅助类（单例模式）
 * 
 * 该类用于收集和管理系统运行时的统计信息，主要用于监控数据传输情况。
 * 采用线程安全的单例模式实现，全局唯一，支持并发访问。
 * 
 * 设计模式：
 * - 单例模式（Singleton）：确保全局唯一实例
 * - Meyers' Singleton：利用C++11静态局部变量的线程安全特性
 * 
 * 线程安全性：
 * - 单例获取：C++11保证静态局部变量初始化线程安全
 * - 数据访问：使用Boost读写锁保护
 * - 读操作：多线程并发读取
 * - 写操作：独占访问，但支持批量更新
 * 
 * 使用示例：
 * @code
 *   // 获取单例实例
 *   StatHelper& helper = StatHelper::one();
 *   
 *   // 更新统计信息（写操作）
 *   helper.updateStatInfo(StatHelper::ST_BROADCAST, 100, 50, 1024000);
 *   
 *   // 获取统计信息（读操作）
 *   StatHelper::StatInfo info = helper.getStatInfo(StatHelper::ST_BROADCAST);
 *   
 *   // 使用统计数据
 *   printf("接收: %u 包, 发送: %u 包, 发送字节: %llu\n",
 *          info._recv_packs, info._send_packs, info._send_bytes);
 * @endcode
 * 
 * 性能优化建议：
 * - 批量更新：尽量减少updateStatInfo调用频率
 * - 读缓存：频繁读取时考虑本地缓存
 * - 异步更新：在单独线程中进行统计更新
 */
class StatHelper
{
public:
	/**
	 * @brief 获取StatHelper单例实例（Meyers' Singleton）
	 * 
	 * 该方法返回StatHelper的全局唯一实例。采用Meyers' Singleton实现，
	 * 利用C++11静态局部变量的线程安全初始化特性。
	 * 
	 * 实现原理：
	 * - C++11标准保证静态局部变量的初始化是线程安全的
	 * - 首次调用时创建实例
	 * - 后续调用直接返回已创建的实例
	 * - 程序结束时自动销毁
	 * 
	 * 线程安全性：
	 * - 初始化过程是线程安全的（C++11保证）
	 * - 多线程同时首次调用不会创建多个实例
	 * - 不需要额外的同步机制
	 * 
	 * 优点：
	 * - 懒加载：只在需要时才创建
	 * - 线程安全：C++11标准保证
	 * - 自动销毁：程序结束时自动清理
	 * - 简洁优雅：代码量少，易于理解
	 * 
	 * @return StatHelper& 单例实例的引用
	 * 
	 * @note 返回引用而非指针，避免空指针问题
	 * @note 不要尝试删除或复制返回的引用
	 */
	static StatHelper& one()
	{
		// 静态局部变量，程序首次执行到这里时初始化
		// C++11保证这个初始化过程是线程安全的
		// 多线程同时执行时，只有一个线程会执行初始化
		// 其他线程会等待初始化完成
		static StatHelper only;         // 唯一的StatHelper实例
		return only;                    // 返回实例的引用
	}

public:
	/**
	 * @struct _StatInfo
	 * @brief 统计信息数据结构
	 * 
	 * 该结构体存储某一类型的统计数据，包括接收包数、发送包数和发送字节数。
	 * 设计为紧凑型结构，减少内存占用和缓存未命中。
	 * 
	 * 数据成员说明：
	 * - _recv_packs：接收的数据包总数
	 * - _send_packs：发送的数据包总数
	 * - _send_bytes：发送的字节总数
	 * 
	 * 数据类型选择：
	 * - uint32_t用于包数：40亿个包通常足够
	 * - uint64_t用于字节数：支持更大的数据量
	 * 
	 * 内存布局：
	 * - 总大小：16字节（4+4+8）
	 * - 对齐：按8字节对齐
	 * - 缓存友好：一个缓存行可容纳多个实例
	 */
	typedef struct _StatInfo
	{
		uint32_t	_recv_packs;        ///< 接收的数据包数量（4字节）
		uint32_t	_send_packs;        ///< 发送的数据包数量（4字节）
		uint64_t	_send_bytes;        ///< 发送的字节总数（8字节）

		/**
		 * @brief 默认构造函数
		 * 
		 * 初始化所有统计值为0，确保数据的初始状态正确。
		 */
		_StatInfo()
		{
			_recv_packs = 0;            // 初始化接收包数为0
			_send_bytes = 0;            // 初始化发送字节数为0
			_send_packs = 0;            // 初始化发送包数为0
		}
	} StatInfo;

	/**
	 * @enum StatType
	 * @brief 统计类型枚举
	 * 
	 * 定义了不同的统计类型，每种类型维护独立的统计信息。
	 * 当前只定义了广播统计，未来可扩展更多类型。
	 * 
	 * 扩展性：
	 * - 可以添加ST_PARSER（解析器统计）
	 * - 可以添加ST_TRADER（交易通道统计）
	 * - 可以添加ST_ENGINE（引擎统计）
	 */
	typedef enum
	{
		ST_BROADCAST                    ///< 广播统计类型（UDP、共享内存等数据分发）
	} StatType;

	/**
	 * @enum UpdateFlag
	 * @brief 更新标志枚举（位标志）
	 * 
	 * 使用位标志表示哪些统计项被更新了，支持组合使用。
	 * 例如：UF_Recv | UF_Send 表示同时更新了接收和发送统计。
	 * 
	 * 设计考虑：
	 * - 使用16进制值便于位运算
	 * - 可以通过按位或（|）组合多个标志
	 * - 便于扩展新的更新类型
	 * 
	 * 注意：
	 * - 当前代码中定义了此枚举但未充分使用
	 * - 在updateStatInfo方法中计算但未返回或存储
	 * - 未来可用于更精细的更新控制
	 */
	typedef enum 
	{
		UF_Recv		= 0x0001,           ///< 接收数据更新标志（位0）
		UF_Send		= 0x0002            ///< 发送数据更新标志（位1）
	} UpdateFlag;

public:
	/**
	 * @brief 更新统计信息（原子操作，线程安全）
	 * 
	 * 该方法用于更新指定类型的统计信息，支持批量更新多个统计项。
	 * 使用写锁保证线程安全，确保数据一致性。
	 * 
	 * 参数说明：
	 * @param sType 统计类型（如ST_BROADCAST）
	 * @param recvPacks 本次接收的包数（增量值）
	 * @param sendPacks 本次发送的包数（增量值）
	 * @param sendBytes 本次发送的字节数（增量值）
	 * 
	 * 更新逻辑：
	 * 1. 对每个参数执行累加操作
	 * 2. 检测sendBytes是否会溢出
	 * 3. 如果溢出，重置为当前值（而非累加）
	 * 4. 计算更新标志（但未使用）
	 * 
	 * 溢出处理：
	 * - 只对sendBytes进行溢出检查
	 * - 检测条件：UINT64_MAX - current < increment
	 * - 溢出时重置为increment值
	 * - recvPacks和sendPacks使用uint32_t，溢出后自然回绕
	 * 
	 * 线程安全：
	 * - 使用写锁（BoostWriteLock）独占访问
	 * - 同一时刻只有一个线程可以更新
	 * - 锁在方法返回时自动释放（RAII）
	 * 
	 * 性能考虑：
	 * - 写锁开销：每次调用需要获取锁
	 * - 批量更新：尽量减少调用频率
	 * - 建议：在调用线程中累积一段时间后批量更新
	 * 
	 * 使用示例：
	 * @code
	 *   // 单次更新
	 *   StatHelper::one().updateStatInfo(StatHelper::ST_BROADCAST, 1, 1, 1024);
	 *   
	 *   // 批量更新（更高效）
	 *   uint32_t recv = 0, send = 0;
	 *   uint64_t bytes = 0;
	 *   // ... 累积多次操作 ...
	 *   StatHelper::one().updateStatInfo(StatHelper::ST_BROADCAST, recv, send, bytes);
	 * @endcode
	 * 
	 * @warning 溢出处理逻辑可能导致统计不准确，建议定期重置
	 */
	void updateStatInfo(StatType sType, uint32_t recvPacks, uint32_t sendPacks, uint64_t sendBytes)
	{
		// 获取写锁，独占访问统计数据
		// BoostWriteLock是RAII类，析构时自动释放锁
		BoostWriteLock lock(_mutexes[sType]);
		
		// 获取对应类型的统计信息引用
		StatInfo& sInfo = _stats[sType];
		
		// 累加接收包数
		// uint32_t溢出后会自动回绕到0（模2^32运算）
		sInfo._recv_packs += recvPacks;
		
		// 累加发送包数
		// uint32_t溢出后会自动回绕到0（模2^32运算）
		sInfo._send_packs += sendPacks;
		
		// 检测sendBytes是否会溢出
		// 如果：UINT64_MAX - 当前值 < 要增加的值
		// 则：当前值 + 要增加的值 会超过UINT64_MAX
		if(UINT64_MAX - sInfo._send_bytes < sendBytes)
			// 溢出：重置为新值（而不是累加）
			// 这会导致统计数据不连续，但避免了溢出
			sInfo._send_bytes = sendBytes;
		else
			// 正常情况：累加字节数
			sInfo._send_bytes += sendBytes;
		
		// 计算更新标志（表示哪些项被更新了）
		// 注意：这个flag被计算但未使用，可能是为未来扩展预留
		uint32_t flag = 0;
		if (recvPacks > 0)
			flag |= UF_Recv;        // 设置接收更新标志位
		if (sendPacks > 0)
			flag |= UF_Send;        // 设置发送更新标志位
		
		// 锁在这里自动释放（BoostWriteLock析构）
	}
	
	/**
	 * @brief 获取统计信息（线程安全的读操作）
	 * 
	 * 该方法用于读取指定类型的当前统计信息。使用读锁保证线程安全，
	 * 支持多线程并发读取。
	 * 
	 * 参数说明：
	 * @param sType 统计类型（如ST_BROADCAST）
	 * @return StatInfo 统计信息的副本
	 * 
	 * 返回值说明：
	 * - 返回StatInfo的副本，而非引用
	 * - 避免返回后数据被其他线程修改
	 * - 调用者可以安全地使用返回的数据
	 * 
	 * 线程安全：
	 * - 使用读锁（BoostReadLock）共享访问
	 * - 多个线程可以同时读取
	 * - 读操作不会阻塞其他读操作
	 * - 读操作会被写操作阻塞
	 * 
	 * 性能考虑：
	 * - 读锁开销：比写锁小，支持并发
	 * - 数据复制：返回副本有一定开销（16字节）
	 * - 缓存：频繁读取时考虑本地缓存
	 * 
	 * 使用示例：
	 * @code
	 *   // 获取当前统计信息
	 *   StatHelper::StatInfo info = StatHelper::one().getStatInfo(StatHelper::ST_BROADCAST);
	 *   
	 *   // 计算发送速率
	 *   double mbps = (double)info._send_bytes / (1024*1024) / elapsed_seconds;
	 *   
	 *   // 计算平均包大小
	 *   if (info._send_packs > 0) {
	 *       double avg_size = (double)info._send_bytes / info._send_packs;
	 *   }
	 * @endcode
	 * 
	 * @note 返回的是快照数据，可能在返回后立即过时
	 * @note 如需精确计算速率，应该多次采样并计算差值
	 */
	StatInfo getStatInfo(StatType sType)
	{
		// 获取读锁，共享访问统计数据
		// BoostReadLock是RAII类，析构时自动释放锁
		// 多个线程可以同时持有读锁
		BoostReadLock lock(_mutexes[sType]);
		
		// 返回统计信息的副本
		// 这是线程安全的，因为在读锁保护下复制数据
		// 复制完成后，即使原数据被修改，返回的副本也不受影响
		return _stats[sType];
		
		// 锁在这里自动释放（BoostReadLock析构）
	}

private:
	/**
	 * @brief 统计信息数组
	 * 
	 * 存储不同类型的统计信息，数组大小为5，支持5种统计类型。
	 * 当前只使用了ST_BROADCAST（索引0），其他4个位置预留for未来扩展。
	 * 
	 * 设计考虑：
	 * - 固定大小数组：编译期确定，无动态分配开销
	 * - 5个元素：为未来扩展预留空间
	 * - 索引访问：O(1)时间复杂度
	 * - 内存占用：5 * 16 = 80字节
	 */
	StatInfo		_stats[5];
	
	/**
	 * @brief 读写锁数组
	 * 
	 * 为每种统计类型提供独立的读写锁，避免不同类型之间的锁竞争。
	 * 使用Boost库的读写锁（shared_mutex），支持多读单写模式。
	 * 
	 * 读写锁特性：
	 * - 多个读者可以同时持有锁（共享锁）
	 * - 写者独占锁，阻塞所有其他读者和写者
	 * - 适合读多写少的场景
	 * - 比普通互斥锁（mutex）有更好的并发性能
	 * 
	 * 锁粒度：
	 * - 每种统计类型一个锁
	 * - 不同类型的操作不会互相阻塞
	 * - 细粒度锁提高并发性能
	 * 
	 * 内存占用：
	 * - 每个BoostRWMutex约40-80字节（平台相关）
	 * - 5个锁共约200-400字节
	 */
	BoostRWMutex	_mutexes[5];
};

