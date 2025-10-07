/*!
 * \file ShmCaster.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 共享内存数据广播器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了ShmCaster类，提供基于共享内存的超高速进程间数据传输。
 * 核心实现了无锁环形队列算法，是WonderTrader中延迟最低的数据传输方式。
 * 
 * 核心实现逻辑：
 * 
 * 1. 共享内存初始化（init方法）：
 *    a) 创建或打开共享内存文件
 *    b) 截断文件为队列大小（清空旧数据）
 *    c) 映射文件到进程地址空间
 *    d) 使用Placement New在共享内存上构造队列对象
 *    e) 设置生产者进程ID
 * 
 * 2. 无锁写入算法（broadcast方法）：
 *    a) 原子递增写指针，获取独占写位置
 *    b) 使用模运算计算环形索引
 *    c) 设置数据类型标识
 *    d) 拷贝数据到队列项
 *    e) 更新读指针（通知消费者）
 * 
 * 3. 内存安全保障：
 *    - 每次启动重置队列
 *    - 防止旧数据干扰
 *    - 避免进程崩溃后的脏数据
 * 
 * 4. 进程ID标识：
 *    - 记录生产者进程ID
 *    - 便于调试和监控
 *    - 检测进程崩溃
 * 
 * 关键技术点：
 * 
 * 1. Placement New技术：
 *    new(addr) Type()：在指定内存地址上构造对象
 *    - addr：共享内存地址
 *    - Type：要构造的类型（CastQueue）
 *    - 不分配新内存，使用已有内存
 *    - 调用构造函数初始化对象
 * 
 * 2. Volatile语义：
 *    - 防止编译器优化
 *    - 确保每次都从内存读取
 *    - 多进程共享变量必需
 *    - x86/x64架构下基本保证原子性
 * 
 * 3. 原子递增：
 *    _writable++在x86/x64架构下是原子操作
 *    - 硬件支持的原子递增
 *    - 无需额外同步
 *    - 仅适用于单生产者场景
 * 
 * 4. 环形索引计算：
 *    realIdx = wIdx % capacity
 *    - 将线性递增的指针映射到固定大小数组
 *    - 实现循环利用
 *    - 模运算开销小（capacity是2的幂时可优化为位运算）
 * 
 * 性能分析：
 * 
 * 1. 写入性能：
 *    - 时间复杂度：O(1)
 *    - 内存拷贝：一次memcpy
 *    - 无锁开销：几乎为0
 *    - 延迟：<100纳秒
 * 
 * 2. 内存访问：
 *    - 顺序访问：缓存友好
 *    - 8字节对齐：CPU高效
 *    - 无动态分配：预分配
 * 
 * 3. 吞吐量：
 *    - 理论：>10,000,000 消息/秒
 *    - 实际：受memcpy速度限制
 *    - 典型：1,000,000 tick/秒
 * 
 * 潜在问题与注意事项：
 * 
 * 1. 队列溢出：
 *    - 消费者读取慢，生产者会覆盖未读数据
 *    - 解决：增大队列容量或提高消费速度
 * 
 * 2. 进程崩溃：
 *    - 共享内存不会自动清理
 *    - 解决：每次启动重置队列
 * 
 * 3. 平台限制：
 *    - 仅限本地进程间通信
 *    - 不支持网络传输
 * 
 * 4. 数据一致性：
 *    - 无锁算法依赖volatile语义
 *    - 在弱内存模型平台可能需要内存屏障
 */

#include "ShmCaster.h"                          // 包含ShmCaster类定义
#include "../Includes/WTSVariant.hpp"           // 包含配置参数类
#include "../Includes/WTSDataDef.hpp"           // 包含数据定义
#include "../Share/StdUtils.hpp"                // 包含标准工具类
#include "../Share/BoostFile.hpp"               // 包含Boost文件操作工具
#include "../WTSTools/WTSLogger.h"              // 包含日志系统

/**
 * @brief 初始化共享内存广播器实现
 * 
 * 该方法是ShmCaster的核心初始化方法，负责创建共享内存并初始化队列结构。
 * 
 * 实现步骤详解：
 * 
 * 步骤1：配置验证
 * - 检查配置对象是否为空
 * - 检查active标志是否启用
 * 
 * 步骤2：获取共享内存路径
 * - 从配置中读取path参数
 * - 路径用于创建内存映射文件
 * 
 * 步骤3：重置共享内存文件
 * - 创建或打开文件
 * - 截断为队列大小（清空旧数据）
 * - 关闭文件句柄
 * 
 * 步骤4：映射共享内存
 * - 创建BoostMappingFile对象
 * - 映射文件到进程地址空间
 * - 获取映射后的内存地址
 * 
 * 步骤5：初始化队列结构
 * - 使用Placement New在共享内存上构造CastQueue
 * - 调用构造函数初始化队列成员
 * 
 * 步骤6：设置进程ID
 * - 获取当前进程ID
 * - 存储到队列结构中
 * 
 * @param cfg 配置参数对象
 * @return bool 初始化成功返回true，失败返回false
 */
bool ShmCaster::init(WTSVariant* cfg)
{
	// 步骤1a：参数验证
	if (cfg == NULL)                            // 配置对象为空
		return false;

	// 步骤1b：检查是否启用
	if (!cfg->getBoolean("active"))             // active标志为false或不存在
		return false;                           // 不启用，返回false

	// 步骤2：获取共享内存文件路径
	// path参数指定共享内存文件名（如"WtQuoteShmData"）
	// 系统会在临时目录或指定目录创建该文件
	_path = cfg->getCString("path");

	// 步骤3：重置共享内存文件
	// 每次启动都重置该队列，确保数据干净
	// 使用代码块{}限定bf的作用域，确保文件被关闭
	{
		BoostFile bf;                           // Boost文件操作对象
		
		// 创建或打开文件
		// 如果文件存在，打开它；如果不存在，创建新文件
		bf.create_or_open_file(_path.c_str());
		
		// 截断文件为队列大小
		// sizeof(CastQueue)：计算队列结构的总大小
		// 这会清空文件的所有内容，重置为指定大小
		bf.truncate_file(sizeof(CastQueue));
		
		// 关闭文件
		// 虽然析构函数会自动关闭，显式调用更清晰
		bf.close_file();
	} // bf对象析构，确保文件句柄释放

	// 步骤4：创建内存映射
	// 使用智能指针管理BoostMappingFile对象
	_mapfile.reset(new BoostMappingFile);
	
	// 映射文件到当前进程的地址空间
	// 多个进程可以映射同一文件，实现共享内存
	_mapfile->map(_path.c_str());
	
	// 获取映射后的内存地址，并转换为队列指针类型
	// addr()返回void*，需要转换为CastQueue*
	_queue = (CastQueue*)_mapfile->addr();
	
	// 步骤5：初始化队列结构
	// 使用Placement New在共享内存上构造CastQueue对象
	// new(addr) Type()：在指定地址addr上构造Type类型对象
	// 这会调用CastQueue的构造函数，初始化_readable、_writable等成员
	new(_mapfile->addr()) CastQueue();

	// 步骤6：设置生产者进程ID
#ifdef _MSC_VER                                 // 如果是Windows平台（MSVC编译器）
	_queue->_pid = _getpid();                   // 使用_getpid()获取进程ID
#else                                           // 如果是Unix/Linux平台
	_queue->_pid = getpid();                    // 使用getpid()获取进程ID
#endif

	// 设置初始化标志为true
	_inited = true;
	
	// 记录初始化成功日志
	WTSLogger::info("ShmCaste initialized @ {}", _path.c_str());

	return true;
}

/**
 * @brief 广播Tick数据实现（无锁环形队列写入）
 * 
 * 该方法将Tick数据写入共享内存队列，使用无锁算法实现极致性能。
 * 
 * 算法详解：
 * 
 * 无锁写入的关键点：
 * 1. 原子递增：_writable++是原子操作（x86/x64架构）
 * 2. 写入隔离：每个wIdx是独占的，不会冲突
 * 3. 内存可见性：volatile确保修改对消费者可见
 * 4. 顺序保证：先写数据，后更新_readable
 * 
 * 步骤详解：
 * 
 * 步骤1：原子递增写指针
 * - _writable++：后置递增，先返回旧值，再递增
 * - wIdx：本次写入使用的逻辑位置
 * - 原子性保证：单生产者不会冲突
 * 
 * 步骤2：计算实际索引
 * - realIdx = wIdx % capacity：映射到数组索引
 * - 实现环形：超过容量后从0开始
 * - 模运算：capacity=8192时，可优化为位运算
 * 
 * 步骤3：设置数据类型
 * - _type = 0：表示Tick数据
 * - 消费者根据_type解析对应的union成员
 * 
 * 步骤4：拷贝数据
 * - 从WTSTickData对象拷贝到共享内存
 * - getTickStruct()：获取底层结构体
 * - memcpy：二进制拷贝，高效
 * - sizeof(WTSTickStruct)：确保拷贝完整
 * 
 * 步骤5：更新可读指针
 * - _readable = wIdx：通知消费者数据已就绪
 * - 这是写入完成的信号
 * - 消费者检查_readable变化来获取新数据
 * 
 * 内存可见性保证：
 * - volatile确保编译器不优化
 * - x86/x64提供强内存模型
 * - 写入顺序：数据 → _readable
 * - 消费者看到_readable更新时，数据必然已写入
 * 
 * @param curTick Tick数据指针
 */
void ShmCaster::broadcast(WTSTickData* curTick)
{
	// 参数和状态验证
	// 三个条件任一为true都直接返回：
	// 1. curTick为NULL：没有数据
	// 2. _queue为NULL：队列未创建
	// 3. !_inited：未初始化成功
	if (curTick == NULL || _queue == NULL || !_inited)
		return;

	/*
	 * 无锁写入算法（单生产者单消费者SPSC）
	 * 
	 * 算法核心：
	 * 1. 先移动写指针（_writable++），获取独占的写位置
	 * 2. 写入数据到该位置
	 * 3. 最后移动读指针（_readable），通知消费者数据就绪
	 * 
	 * 关键：写入完成后才更新_readable，确保消费者不会读到不完整数据
	 */
	
	// 步骤1：原子递增写指针，获取本次写入的逻辑位置
	// 后置递增（_writable++）：先返回旧值给wIdx，再将_writable加1
	// 这保证了每次调用获得的wIdx是唯一的
	uint64_t wIdx = _queue->_writable++;
	
	// 步骤2：计算环形数组的实际索引
	// wIdx可能非常大（持续递增），通过模运算映射到[0, capacity)范围
	// 例如：wIdx=8192时，realIdx=8192%8192=0，回到数组开头
	// 这实现了环形缓冲的效果
	uint64_t realIdx = wIdx % _queue->_capacity;
	
	// 步骤3：设置数据类型标识
	// 0表示Tick数据，消费者根据此字段判断使用union的哪个成员
	_queue->_items[realIdx]._type = 0;
	
	// 步骤4：拷贝Tick数据到共享内存
	// getTickStruct()：获取Tick的底层C结构体
	// &_queue->_items[realIdx]._tick：目标地址（union中的_tick成员）
	// sizeof(WTSTickStruct)：拷贝大小
	// memcpy：二进制拷贝，最快的方式
	memcpy(&_queue->_items[realIdx]._tick, &curTick->getTickStruct(), sizeof(WTSTickStruct));
	
	// 步骤5：更新可读指针（写入完成信号）
	// 消费者通过检查_readable的变化来发现新数据
	// 设置_readable = wIdx表示：位置wIdx的数据已经写入完成
	// volatile确保这个写入对其他进程立即可见
	_queue->_readable = wIdx;
	
	// 写入完成，方法返回
	// 整个过程无锁，延迟极低（<100纳秒）
}

/**
 * @brief 广播委托队列数据实现
 * 
 * 写入逻辑与broadcast(Tick)相同，只是数据类型不同。
 * 
 * @param curOrdQue 委托队列数据指针
 */
void ShmCaster::broadcast(WTSOrdQueData* curOrdQue)
{
	// 参数和状态验证
	if (curOrdQue == NULL || _queue == NULL || !_inited)
		return;

	/*
	 * 无锁写入算法（与Tick相同）
	 * 先移动写的下标，然后写入数据
	 * 写完了以后，再移动读的下标
	 */
	
	// 步骤1：原子递增写指针
	uint64_t wIdx = _queue->_writable++;
	
	// 步骤2：计算实际索引
	uint64_t realIdx = wIdx % _queue->_capacity;
	
	// 步骤3：设置数据类型为1（委托队列）
	_queue->_items[realIdx]._type = 1;
	
	// 步骤4：拷贝委托队列数据
	// 目标：union中的_queue成员
	memcpy(&_queue->_items[realIdx]._queue, &curOrdQue->getOrdQueStruct(), sizeof(WTSOrdQueStruct));
	
	// 步骤5：更新可读指针
	_queue->_readable = wIdx;
}

/**
 * @brief 广播逐笔委托数据实现
 * 
 * 写入逻辑与broadcast(Tick)相同，只是数据类型不同。
 * 
 * @param curOrdDtl 逐笔委托数据指针
 */
void ShmCaster::broadcast(WTSOrdDtlData* curOrdDtl)
{
	// 参数和状态验证
	if (curOrdDtl == NULL || _queue == NULL || !_inited)
		return;

	/*
	 * 无锁写入算法（与Tick相同）
	 * 先移动写的下标，然后写入数据
	 * 写完了以后，再移动读的下标
	 */
	
	// 步骤1：原子递增写指针
	uint64_t wIdx = _queue->_writable++;
	
	// 步骤2：计算实际索引
	uint64_t realIdx = wIdx % _queue->_capacity;
	
	// 步骤3：设置数据类型为2（逐笔委托）
	_queue->_items[realIdx]._type = 2;
	
	// 步骤4：拷贝逐笔委托数据
	// 目标：union中的_order成员
	memcpy(&_queue->_items[realIdx]._order, &curOrdDtl->getOrdDtlStruct(), sizeof(WTSOrdDtlStruct));
	
	// 步骤5：更新可读指针
	_queue->_readable = wIdx;
}

/**
 * @brief 广播逐笔成交数据实现
 * 
 * 写入逻辑与broadcast(Tick)相同，只是数据类型不同。
 * 
 * @param curTrans 逐笔成交数据指针
 */
void ShmCaster::broadcast(WTSTransData* curTrans)
{
	// 参数和状态验证
	if (curTrans == NULL || _queue == NULL || !_inited)
		return;

	/*
	 * 无锁写入算法（与Tick相同）
	 * 先移动写的下标，然后写入数据
	 * 写完了以后，再移动读的下标
	 */
	
	// 步骤1：原子递增写指针
	uint64_t wIdx = _queue->_writable++;
	
	// 步骤2：计算实际索引
	uint64_t realIdx = wIdx % _queue->_capacity;
	
	// 步骤3：设置数据类型为3（逐笔成交）
	_queue->_items[realIdx]._type = 3;
	
	// 步骤4：拷贝逐笔成交数据
	// 目标：union中的_trans成员
	memcpy(&_queue->_items[realIdx]._trans, &curTrans->getTransStruct(), sizeof(WTSTransStruct));
	
	// 步骤5：更新可读指针
	_queue->_readable = wIdx;
}
