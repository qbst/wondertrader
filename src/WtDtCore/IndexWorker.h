/*!
 * \file IndexWorker.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 指数计算工作器类定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了IndexWorker（指数计算工作器）类，负责单个指数的实时计算。
 * 每个Worker管理一个指数，订阅成分合约的行情，根据权重算法实时计算指数值。
 * 
 * 核心设计理念：
 * 
 * 1. 指数计算模型：
 *    指数值 = Σ(成分价格 * 权重因子) / 权重基数 / 总权重 * 标准化系数
 *    
 *    其中：
 *    - 成分价格：各成分合约的最新价
 *    - 权重因子：根据权重算法计算
 *    - 权重基数：归一化因子
 *    - 总权重：所有成分权重之和
 *    - 标准化系数：将指数值调整到期望范围
 * 
 * 2. 三种权重算法：
 *    
 *    算法0 - 固定权重（Fixed Weight）：
 *    - 权重因子 = 配置的权重值
 *    - 权重基数 = 1
 *    - 公式：Σ(价格 * 权重) / 总权重 * 标准化系数
 *    - 适用：一般板块指数
 *    
 *    算法1 - 动态总持权重（Dynamic Interest）：
 *    - 权重因子 = 持仓量 * 权重值
 *    - 权重基数 = Σ持仓量
 *    - 公式：Σ(价格 * 持仓量 * 权重) / Σ持仓量 / 总权重 * 标准化系数
 *    - 适用：流动性加权指数
 *    
 *    算法2 - 动态成交量权重（Dynamic Volume）：
 *    - 权重因子 = 成交量 * 权重值
 *    - 权重基数 = Σ成交量
 *    - 公式：Σ(价格 * 成交量 * 权重) / Σ成交量 / 总权重 * 标准化系数
 *    - 适用：活跃度加权指数
 * 
 * 3. 触发机制：
 *    
 *    触发器类型：
 *    a) 指定合约触发：某个成分合约更新时触发重算
 *       - trigger: "SHFE.rb2105"
 *       - 适用：主力合约触发
 *    
 *    b) 时间触发：任意成分更新时触发
 *       - trigger: "time"
 *       - 可设置延时（timeout），避免频繁计算
 * 
 * 4. 延时计算策略：
 *    - 设置timeout（毫秒）后，不立即计算
 *    - 启动定时器，延时后再计算
 *    - 多次触发只计算一次
 *    - 减少计算频率，提高性能
 * 
 * 架构设计：
 * 
 *   [IndexFactory]
 *          ↓
 *   ┌─────────────────┐
 *   │  IndexWorker    │  (例如：钢铁板块指数)
 *   └─────────────────┘
 *          │
 *   订阅成分合约行情
 *          ↓
 *   ┌─────┬─────┬─────┐
 *   │rb2105│rb2109│rb2201│
 *   └─────┴─────┴─────┘
 *     0.6   0.3   0.1  (权重)
 *          ↓
 *   接收行情更新
 *          ↓
 *   根据权重算法计算
 *          ↓
 *   生成指数Tick
 *          ↓
 *   push_tick() → DataManager
 * 
 * 数据结构：
 * 
 * WeightFactor（权重因子）：
 * - _weight：配置的权重值
 * - _tick：该成分的最新行情
 * 
 * _weight_scales（映射表）：
 * - Key：成分合约代码
 * - Value：WeightFactor
 * 
 * _cache（缓存）：
 * - 存储指数的当前状态
 * - 用于计算开高低收
 * 
 * 性能优化：
 * - 使用SpinMutex保护数据（轻量级锁）
 * - 延时计算减少频率
 * - 缓存避免重复计算
 * 
 * 应用示例：
 * 
 * 计算黑色系板块指数：
 * - 成分：rb（螺纹钢）、i（铁矿石）、j（焦炭）
 * - 权重：0.5、0.3、0.2
 * - 算法：固定权重
 * - 触发：任意成分更新
 */

#pragma once                                                // 防止头文件重复包含

#include "../Includes/WTSMarcos.h"                          // WonderTrader宏定义
#include "../Includes/WTSStruct.h"                          // WonderTrader数据结构
#include "../Includes/FasterDefs.h"                         // 快速数据结构定义

#include "../Share/StdUtils.hpp"                            // 标准工具类
#include "../Share/StdUtils.hpp"                            // 重复包含（可删除）
#include "../Share/SpinMutex.hpp"                           // 自旋锁

// 前向声明
NS_WTP_BEGIN                                                // 开始WonderTrader命名空间
class WTSVariant;                                           // 配置参数类
class WTSTickData;                                          // Tick数据类
class IHotMgr;                                              // 主力合约管理器接口
class IBaseDataMgr;                                         // 基础数据管理器接口
class WTSContractInfo;                                      // 合约信息类
NS_WTP_END                                                  // 结束WonderTrader命名空间

USING_NS_WTP;                                               // 使用WonderTrader命名空间

class IndexFactory;                                         // 指数工厂类

/**
 * @class IndexWorker
 * @brief 指数计算工作器类
 * 
 * 该类负责单个指数的实时计算，管理成分合约、权重配置和触发策略。
 * 
 * 核心职责：
 * 1. 订阅成分合约的行情数据
 * 2. 接收行情更新并缓存
 * 3. 根据触发条件决定是否重算指数
 * 4. 使用权重算法计算指数值
 * 5. 生成指数Tick并推送
 * 
 * 工作流程：
 * 1. init()：加载配置，订阅成分合约
 * 2. handle_quote()：接收成分行情，判断是否触发计算
 * 3. generate_tick()：计算指数值，生成Tick
 * 4. push_tick()：推送指数Tick到Factory
 * 
 * 线程模型：
 * - 主线程：接收行情，更新缓存
 * - 触发线程（可选）：延时触发指数计算
 * - SpinMutex：保护_weight_scales数据
 * 
 * 触发策略：
 * - 即时触发：timeout=0，立即计算
 * - 延时触发：timeout>0，延时后计算
 * 
 * 配置示例：
 * @code
 *   {
 *     "exchg": "SHFE",
 *     "code": "steel_index",
 *     "trigger": "SHFE.rb2105",        // 触发合约
 *     "timeout": 100,                   // 延时100ms
 *     "weight_alg": 0,                  // 固定权重
 *     "stand_scale": 1.0,               // 标准化系数
 *     "codes": [
 *       {"code": "SHFE.rb2105", "weight": 0.6},
 *       {"code": "SHFE.rb2109", "weight": 0.3}
 *     ]
 *   }
 * @endcode
 */
class IndexWorker
{
public:
	/**
	 * @brief 构造函数（内联实现）
	 * 
	 * @param factor 指数工厂指针（Worker需要通过Factory访问服务）
	 */
	IndexWorker(IndexFactory* factor):_factor(factor), _stopped(false), _process(false) {}

public:
	/**
	 * @brief 初始化指数工作器
	 * 
	 * 从配置中读取指数定义，订阅成分合约。
	 * 
	 * @param config 指数配置参数
	 * @return bool 初始化成功返回true，失败返回false
	 */
	bool	init(WTSVariant* config);
	
	/**
	 * @brief 处理成分合约的行情数据
	 * 
	 * 接收成分合约的行情，判断是否触发指数计算。
	 * 
	 * @param newTick 新的Tick数据指针
	 */
	void	handle_quote(WTSTickData* newTick);

private:
	/**
	 * @brief 生成指数Tick数据
	 * 
	 * 根据所有成分的最新行情和权重配置，计算指数值并生成Tick。
	 */
	void	generate_tick();

protected:
	IndexFactory*	_factor;            ///< 指数工厂指针（提供服务访问）
	std::string		_exchg;             ///< 指数所属交易所
	std::string		_code;              ///< 指数代码
	std::string		_trigger;           ///< 触发合约（或"time"）
	uint32_t		_timeout;           ///< 延时时间（毫秒），0=立即触发
	uint64_t		_recalc_time;       ///< 重算时间点（用于延时触发）
	double			_stand_scale;       ///< 标准化系数
	WTSTickStruct	_cache;             ///< 指数Tick缓存（用于计算开高低收）
	WTSContractInfo*	_cInfo;         ///< 指数的合约信息

	/**
	 * @struct _WeightFactor
	 * @brief 权重因子结构
	 * 
	 * 存储单个成分合约的权重和最新行情。
	 */
	typedef struct _WeightFactor
	{
		double			_weight;        ///< 权重值（配置的）
		WTSTickStruct	_tick;          ///< 该成分的最新Tick（结构体拷贝）
		
		/**
		 * @brief 默认构造函数
		 * 
		 * 将整个结构清零。
		 */
		_WeightFactor()
		{
			memset(this, 0, sizeof(_WeightFactor));
		}
	}WeightFactor;
	
	SpinMutex	_mtx_data;              ///< 自旋锁（保护_weight_scales数据）
	wt_hashmap<std::string, WeightFactor>	_weight_scales;  ///< 成分合约映射表（代码 → 权重因子）
	uint32_t	_weight_alg;            ///< 权重算法（0=固定，1=动态总持，2=动态成交量）

	StdThreadPtr	_thrd_trigger;      ///< 触发线程（延时触发时使用）
	StdUniqueMutex	_mtx_trigger;       ///< 触发线程互斥锁
	StdCondVariable	_cond_trigger;      ///< 触发线程条件变量
	bool			_stopped;           ///< 停止标志
	bool			_process;           ///< 处理标志（是否有待处理的触发）
};

/**
 * @typedef IndexWorkerPtr
 * @brief IndexWorker智能指针类型
 * 
 * 使用智能指针自动管理Worker的生命周期。
 */
typedef std::shared_ptr<IndexWorker> IndexWorkerPtr;
