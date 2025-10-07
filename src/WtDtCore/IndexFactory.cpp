/*!
 * \file IndexFactory.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 指数工厂类实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了IndexFactory类，提供指数计算工作器的创建、管理和数据分发功能。
 * 该模块是WonderTrader自定义指数功能的核心，支持实时计算各种板块指数、
 * 主题指数等衍生品。
 * 
 * 核心实现逻辑：
 * 
 * 1. 初始化流程（init方法）：
 *    a) 保存依赖组件引用（HotMgr、BaseDataMgr、DataMgr）
 *    b) 创建线程池（如果配置了poolsize）
 *    c) 遍历配置中的指数定义
 *    d) 为每个活跃的指数创建IndexWorker
 *    e) 初始化Worker（Worker会订阅成分合约）
 * 
 * 2. 行情分发流程（handle_quote方法）：
 *    a) 检查该合约是否被订阅
 *    b) 增加Tick的引用计数（多Worker共享）
 *    c) 如果有线程池，异步执行Worker计算
 *    d) 如果无线程池，同步执行Worker计算
 *    e) 在合适的时机释放Tick引用
 * 
 * 3. 订阅管理流程（sub_ticks方法）：
 *    a) Worker调用sub_ticks订阅成分合约
 *    b) 将合约代码加入_subbed集合
 *    c) 从DataManager获取该合约的当前Tick
 *    d) 返回Tick供Worker初始化
 * 
 * 4. 指数推送流程（push_tick方法）：
 *    a) Worker计算出指数后调用push_tick
 *    b) 写入DataManager（procFlag=1，仅写入不缓存）
 *    c) DataManager存储并广播指数数据
 * 
 * 关键技术点：
 * 
 * 1. 引用计数管理：
 *    - Tick对象使用引用计数
 *    - retain()：增加引用计数
 *    - release()：减少引用计数
 *    - 计数为0时自动删除对象
 * 
 * 2. 线程池调度：
 *    - schedule(lambda)：提交任务到线程池
 *    - lambda捕获：[this, newTick, fullCode]
 *    - 异步执行，不阻塞主流程
 * 
 * 3. 智能指针：
 *    - IndexWorkerPtr：智能指针管理Worker
 *    - 自动管理内存，避免泄漏
 * 
 * 性能考虑：
 * - 订阅检查：使用hashset，O(1)查询
 * - 线程池：避免频繁创建销毁线程
 * - 引用计数：避免数据拷贝
 * - 批量处理：一次行情可触发多个指数计算
 */

#include "IndexFactory.h"                       // 包含IndexFactory类定义
#include "DataManager.h"                        // 包含DataManager类定义
#include "../Includes/WTSVariant.hpp"           // 包含配置参数类
#include "../Includes/WTSDataDef.hpp"           // 包含数据定义
#include "../Includes/WTSContractInfo.hpp"      // 包含合约信息类
#include "../Share/StrUtil.hpp"                 // 包含字符串工具类

/**
 * @brief 初始化指数工厂实现
 * 
 * 从配置中加载所有指数定义，为每个活跃的指数创建IndexWorker。
 * 
 * @param config 配置参数对象
 * @param hotMgr 主力合约管理器指针
 * @param bdMgr 基础数据管理器指针
 * @param dataMgr 数据管理器指针
 * @return bool 初始化成功返回true，失败返回false
 */
bool IndexFactory::init(WTSVariant* config, IHotMgr* hotMgr, IBaseDataMgr* bdMgr, DataManager* dataMgr)
{
	// 保存依赖组件的引用
	_hot_mgr = hotMgr;                          // 主力合约管理器，用于解析连续合约代码
	_bd_mgr = bdMgr;                            // 基础数据管理器，用于查询合约信息
	_data_mgr = dataMgr;                        // 数据管理器，用于推送指数数据

	// 读取线程池配置
	uint32_t poolsize = config->getUInt32("poolsize");
	if (poolsize > 0)                           // 如果配置了线程池大小
	{
		// 创建线程池
		// boost::threadpool::pool：Boost线程池实现
		// poolsize：线程数量，通常设置为CPU核心数
		_pool.reset(new boost::threadpool::pool(poolsize));
	}
	// 如果poolsize=0，不创建线程池，使用同步模式

	// 获取指数配置列表
	WTSVariant* cfgIdx = config->get("indice");     // "indice"：指数配置数组
	if(cfgIdx == NULL || !cfgIdx->isArray())        // 配置不存在或不是数组
	{
		return false;                                // 返回失败
	}

	// 获取指数数量
	auto cnt = cfgIdx->size();
	
	// 遍历所有指数配置
	for(std::size_t i = 0; i < cnt; i++)
	{
		// 获取单个指数的配置
		WTSVariant* cfgItem = cfgIdx->get(i);
		
		// 检查是否启用该指数
		if(!cfgItem->getBoolean("active"))          // active=false或未配置
			continue;                                // 跳过该指数

		// 创建IndexWorker实例（使用智能指针）
		// 传入this指针，Worker需要通过Factory访问服务
		IndexWorkerPtr worker(new IndexWorker(this));
		
		// 初始化Worker（Worker会订阅成分合约）
		if (!worker->init(cfgItem))                 // 初始化失败
			continue;                                // 跳过该指数

		// 将Worker添加到集合
		// emplace_back：直接在vector尾部构造，避免拷贝
		_workers.emplace_back(worker);
	}

	return true;
}

/**
 * @brief 处理行情数据实现
 * 
 * 该方法接收成分合约的行情，并分发给所有订阅了该合约的Worker。
 * 
 * 处理流程详解：
 * 
 * 步骤1：验证数据有效性
 * 步骤2：检查是否被订阅
 * 步骤3：增加引用计数
 * 步骤4：分发给所有Worker（线程池或同步）
 * 步骤5：释放引用计数
 * 
 * @param newTick 新的Tick数据指针
 */
void IndexFactory::handle_quote(WTSTickData* newTick)
{
	// 步骤1：验证数据有效性
	if (newTick == NULL)                        // 空指针检查
		return;

	// 步骤2：检查该合约是否被订阅
	// getFullCode()：获取完整合约代码（如"SHFE.rb2105"）
	const char* fullCode = newTick->getContractInfo()->getFullCode();
	
	// 在订阅集合中查找
	auto it = _subbed.find(fullCode);
	if (it == _subbed.end())                    // 如果没有Worker订阅该合约
		return;                                  // 直接返回，不处理

	// 步骤3：增加Tick的引用计数
	// 因为多个Worker可能共享同一个Tick对象
	// 需要增加引用计数，防止被提前释放
	newTick->retain();

	// 步骤4：分发给所有Worker
	if(_pool)                                   // 如果配置了线程池
	{	
		// 使用线程池异步执行
		// schedule：提交任务到线程池
		// lambda表达式：捕获this、newTick、fullCode
		_pool->schedule([this, newTick, fullCode]() {

			// 遍历所有Worker，让它们处理该行情
			for(IndexWorkerPtr& worker : _workers)
			{
				// Worker会检查该Tick是否是其成分合约
				// 如果是，会参与指数计算
				worker->handle_quote(newTick);
			}
			
			// 这里加一个处理
			// 所有Worker处理完毕后释放Tick的引用
			// 引用计数减1，可能触发Tick对象删除
			newTick->release();
		});
	}
	else                                        // 如果未配置线程池
	{
		// 同步执行，在当前线程中处理
		for (IndexWorkerPtr& worker : _workers)
		{
			// 依次调用每个Worker处理行情
			worker->handle_quote(newTick);
		}
		// 注意：同步模式下，这里没有释放Tick引用
		// 这可能是一个bug，会导致引用计数不匹配
		// 建议添加：newTick->release();
	}
}

/**
 * @brief 推送指数Tick数据实现
 * 
 * Worker计算出指数后调用此方法，将指数作为一个新的Tick推送到系统。
 * 
 * @param newTick 新计算的指数Tick数据
 */
void IndexFactory::push_tick(WTSTickData* newTick)
{
	// 写入DataManager
	// procFlag=1：仅写入，不更新缓存
	// 原因：指数是计算出来的，不需要缓存供查询
	// DataManager会将指数数据存储到文件并广播
	_data_mgr->writeTick(newTick, 1);
}

/**
 * @brief 订阅成分合约的Tick数据实现
 * 
 * Worker在初始化时调用此方法订阅成分合约，并获取当前行情作为基准。
 * 
 * @param fullCode 完整合约代码（如"SHFE.rb2105"）
 * @return WTSTickData* 该合约当前的Tick数据（可能为NULL）
 */
WTSTickData* IndexFactory::sub_ticks(const char* fullCode)
{
	// 将合约代码加入订阅集合
	// 后续该合约的行情会被分发给Worker
	_subbed.insert(fullCode);
	
	// 解析完整代码，提取交易所和合约代码
	// 例如："SHFE.rb2105" → {"SHFE", "rb2105"}
	auto ay = StrUtil::split(fullCode, ".");
	
	// 从DataManager获取该合约的当前Tick
	// ay[1]：合约代码
	// ay[0]：交易所代码
	// 返回值：当前Tick数据，可能为NULL（如果该合约还没有行情）
	return _data_mgr->getCurTick(ay[1].c_str(), ay[0].c_str());
}
