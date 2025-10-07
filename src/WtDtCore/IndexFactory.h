/*!
 * \file IndexFactory.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 指数工厂类定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了IndexFactory（指数工厂）类，负责管理和协调多个指数计算工作器。
 * 该模块实现了自定义指数的实时计算功能，支持板块指数、主题指数等衍生品的生成。
 * 
 * 核心设计理念：
 * 
 * 1. 工厂模式（Factory Pattern）：
 *    - IndexFactory作为工厂管理多个IndexWorker
 *    - 每个Worker负责一个指数的计算
 *    - 统一的创建、初始化和数据分发接口
 * 
 * 2. 指数计算原理：
 *    - 指数由多个成分合约组成
 *    - 每个成分有对应的权重
 *    - 根据成分价格和权重计算指数值
 *    - 支持多种权重算法（固定、动态总持、动态成交量）
 * 
 * 3. 订阅管理：
 *    - Worker订阅成分合约行情
 *    - Factory维护订阅关系
 *    - 行情到达时分发给相关Worker
 *    - 避免重复订阅
 * 
 * 4. 线程池优化：
 *    - 可选的线程池支持
 *    - 多个指数并行计算
 *    - 提高计算效率
 * 
 * 架构设计：
 * 
 *   [行情数据]
 *        ↓
 *   ParserAdapter
 *        ↓
 *   DataManager
 *        ↓
 *   IndexFactory → handle_quote(tick)
 *        ↓
 *   检查订阅关系：_subbed
 *        ↓
 *   ┌───┴───┬───────┬───────┐
 *   ↓       ↓       ↓       ↓
 * Worker1 Worker2 Worker3 ...
 * (rb指数)(化工板块)(能源板块)
 *   ↓       ↓       ↓
 * 计算指数值
 *   ↓       ↓       ↓
 * push_tick() → DataManager
 *                    ↓
 *              [指数行情存储和广播]
 * 
 * 指数计算示例：
 * 
 * 螺纹钢板块指数（3个成分）：
 * - SHFE.rb2105：权重0.6，价格4500
 * - SHFE.rb2109：权重0.3，价格4520
 * - SHFE.rb2201：权重0.1，价格4530
 * 
 * 固定权重算法：
 * 指数 = (4500*0.6 + 4520*0.3 + 4530*0.1) / (0.6+0.3+0.1)
 *      = (2700 + 1356 + 453) / 1.0
 *      = 4509
 * 
 * 动态总持算法：
 * 假设持仓量分别为：10000, 5000, 2000
 * 指数 = (4500*10000*0.6 + 4520*5000*0.3 + 4530*2000*0.1) / (10000+5000+2000) / (0.6+0.3+0.1)
 * 
 * 应用场景：
 * - 行业板块指数（钢铁、化工、能源等）
 * - 主题指数（新能源、半导体等）
 * - 自定义组合指数
 * - 合成期货（跨品种套利）
 * 
 * 技术特点：
 * - 实时计算：行情更新时立即重算
 * - 多指数并行：支持同时计算多个指数
 * - 灵活配置：权重、算法、触发条件可配置
 * - 线程池优化：可选的并行计算
 */

#pragma once                                                // 防止头文件重复包含

#include "IndexWorker.h"                                    // 包含IndexWorker类定义
#include "../Share/threadpool.hpp"                          // 包含Boost线程池
#include <vector>                                           // STL向量容器

// 前向声明
class DataManager;                                          // 数据管理器

/**
 * @class IndexFactory
 * @brief 指数工厂类
 * 
 * 该类管理多个指数计算工作器（IndexWorker），负责：
 * 1. 创建和初始化所有指数Worker
 * 2. 接收成分合约的行情数据
 * 3. 将行情分发给相关的Worker
 * 4. 管理成分合约的订阅关系
 * 5. 将计算好的指数推送到DataManager
 * 
 * 工作流程：
 * 1. init()：从配置加载所有指数定义，创建Worker
 * 2. Worker初始化时订阅成分合约（通过sub_ticks）
 * 3. handle_quote()：接收行情并分发给Worker
 * 4. Worker计算指数后调用push_tick()
 * 5. push_tick()将指数行情写入DataManager
 * 
 * 配置示例：
 * @code
 *   {
 *     "poolsize": 4,                    // 线程池大小（0=不使用线程池）
 *     "indice": [                        // 指数列表
 *       {
 *         "active": true,
 *         "exchg": "SHFE",
 *         "code": "rb_index",            // 指数代码
 *         "trigger": "SHFE.rb2105",      // 触发合约
 *         "timeout": 100,                 // 超时时间（ms）
 *         "weight_alg": 0,                // 权重算法（0=固定）
 *         "stand_scale": 1.0,             // 标准化系数
 *         "codes": [                      // 成分合约列表
 *           {"code": "SHFE.rb2105", "weight": 0.6},
 *           {"code": "SHFE.rb2109", "weight": 0.3},
 *           {"code": "SHFE.rb2201", "weight": 0.1}
 *         ]
 *       }
 *     ]
 *   }
 * @endcode
 * 
 * 线程安全性：
 * - handle_quote()可能在多线程中调用
 * - 使用线程池时Worker并行执行
 * - 需要注意数据竞争
 */
class IndexFactory
{
public:
	/**
	 * @brief 默认构造函数（内联实现）
	 * 
	 * 初始化成员指针为NULL。
	 */
	IndexFactory():_hot_mgr(NULL), _bd_mgr(NULL){}

public:
	/**
	 * @brief 初始化指数工厂
	 * 
	 * 从配置中加载所有指数定义，创建对应的IndexWorker。
	 * 
	 * @param config 配置参数对象
	 * @param hotMgr 主力合约管理器（用于解析连续合约代码）
	 * @param bdMgr 基础数据管理器
	 * @param dataMgr 数据管理器
	 * @return bool 初始化成功返回true，失败返回false
	 */
	bool	init(WTSVariant* config, IHotMgr* hotMgr, IBaseDataMgr* bdMgr, DataManager* dataMgr);
	
	/**
	 * @brief 处理行情数据
	 * 
	 * 接收成分合约的行情，分发给订阅了该合约的Worker。
	 * 
	 * 处理流程：
	 * 1. 检查该合约是否被订阅
	 * 2. 增加引用计数
	 * 3. 分发给所有Worker（可能使用线程池）
	 * 4. 释放引用计数
	 * 
	 * @param newTick 新的Tick数据指针
	 * 
	 * @note 如果使用线程池，会异步执行
	 */
	void	handle_quote(WTSTickData* newTick);

public:
	/**
	 * @brief 获取主力合约管理器（内联函数）
	 * 
	 * @return IHotMgr* 主力合约管理器指针
	 */
	inline IHotMgr*			get_hot_mgr() { return _hot_mgr; }
	
	/**
	 * @brief 获取基础数据管理器（内联函数）
	 * 
	 * @return IBaseDataMgr* 基础数据管理器指针
	 */
	inline IBaseDataMgr*	get_bd_mgr() { return _bd_mgr; }

	/**
	 * @brief 订阅成分合约的Tick数据
	 * 
	 * Worker通过此方法订阅成分合约的行情。
	 * 
	 * @param fullCode 完整合约代码（如"SHFE.rb2105"）
	 * @return WTSTickData* 该合约当前的Tick数据（可能为NULL）
	 * 
	 * @note 返回的Tick可用于初始化Worker的基准数据
	 */
	WTSTickData*	sub_ticks(const char* fullCode);

	/**
	 * @brief 推送指数Tick数据
	 * 
	 * Worker计算出指数后通过此方法推送到DataManager。
	 * 
	 * @param newTick 新计算的指数Tick数据
	 */
	void			push_tick(WTSTickData* newTick);

private:
	/**
	 * @typedef IndexWorkers
	 * @brief IndexWorker集合类型
	 */
	typedef std::vector<IndexWorkerPtr>	IndexWorkers;
	
	/**
	 * @brief 指数工作器集合
	 * 
	 * 存储所有的IndexWorker实例，每个Worker负责一个指数的计算。
	 */
	IndexWorkers	_workers;
	
	/**
	 * @brief 主力合约管理器指针
	 * 
	 * 用于解析连续合约代码（如"SHFE.rb.HOT"）。
	 */
	IHotMgr*		_hot_mgr;
	
	/**
	 * @brief 基础数据管理器指针
	 * 
	 * 用于查询合约信息、品种信息等。
	 */
	IBaseDataMgr*	_bd_mgr;
	
	/**
	 * @brief 数据管理器指针
	 * 
	 * 用于推送计算好的指数数据。
	 */
	DataManager*	_data_mgr;

	/**
	 * @typedef ThreadPoolPtr
	 * @brief 线程池智能指针类型
	 */
	typedef std::shared_ptr<boost::threadpool::pool> ThreadPoolPtr;
	
	/**
	 * @brief 线程池智能指针
	 * 
	 * 如果配置了poolsize>0，会创建线程池。
	 * Worker的计算任务在线程池中并行执行。
	 */
	ThreadPoolPtr	_pool;

	/**
	 * @brief 已订阅合约集合
	 * 
	 * 存储所有被Worker订阅的合约代码。
	 * 用于快速判断某个行情是否需要处理。
	 */
	wt_hashset<std::string>	_subbed;
};

