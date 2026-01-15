/*!
 * \file UftLatencyTool.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UFT延迟测试工具类定义
 * 
 * 本文件定义了UftLatencyTool类，用于测试WonderTrader UFT（Ultra Fast Trading，超快交易）引擎的延迟性能。
 * 
 * 设计说明：
 * - 模拟完整的UFT交易环境（解析器、交易接口、策略引擎）
 * - 使用模拟的Tick数据测试引擎处理延迟
 * - 支持CPU核心绑定，确保测试的准确性
 * - 统计并输出平均延迟时间
 * 
 * 测试流程：
 * 1. init - 初始化引擎、解析器、交易接口、策略
 * 2. run - 运行测试，生成模拟Tick数据并统计延迟
 * 3. 输出平均延迟时间（纳秒级）
 * 
 * 使用场景：
 * - 评估UFT引擎的性能
 * - 优化策略执行速度
 * - 测试系统延迟
 * 
 * 与HFT引擎的区别：
 * - UFT引擎专注于超快交易，延迟要求更高
 * - 策略接口和上下文略有不同
 */
#pragma once
#include "../WtUftCore/WtUftEngine.h"      // UFT引擎类
#include "../WtUftCore/UftStrategyMgr.h"   // UFT策略管理器类
#include "../WtUftCore/TraderAdapter.h"     // 交易适配器类
#include "../WtUftCore/ParserAdapter.h"    // 解析器适配器类

#include "../WTSTools/WTSBaseDataMgr.h"    // 基础数据管理器类

NS_WTP_BEGIN                                // WonderTrader命名空间开始
class WTSVariant;                          // 前向声明：变体类型，用于配置参数
NS_WTP_END                                 // WonderTrader命名空间结束

namespace uft                               // uft命名空间
{
	/**
	 * @class UftLatencyTool
	 * @brief UFT延迟测试工具类
	 * 
	 * 该类负责：
	 * - 初始化UFT引擎和测试环境
	 * - 创建模拟的解析器和交易接口
	 * - 加载测试策略
	 * - 运行延迟测试并统计结果
	 * 
	 * 设计特点：
	 * - 使用模拟数据避免依赖外部数据源
	 * - 支持CPU核心绑定，减少测试干扰
	 * - 统计纳秒级延迟，精确测量性能
	 */
	class UftLatencyTool
	{
	public:
		/**
		 * @brief 构造函数
		 * 
		 * 初始化UftLatencyTool对象，所有成员变量使用默认值。
		 */
		UftLatencyTool();
		
		/**
		 * @brief 析构函数
		 * 
		 * 清理资源，智能指针和对象会自动释放。
		 */
		~UftLatencyTool();

	public:
		/**
		 * @brief 初始化测试环境
		 * @return 返回初始化是否成功（布尔值）
		 * 
		 * 初始化流程：
		 * 1. 初始化日志系统
		 * 2. 加载配置文件
		 * 3. 加载基础数据（交易时段、品种、合约）
		 * 4. 读取测试参数（次数、CPU核心）
		 * 5. 初始化引擎、模块、策略
		 */
		bool init();

		/**
		 * @brief 运行延迟测试
		 * 
		 * 运行流程：
		 * 1. 绑定CPU核心（如果配置了）
		 * 2. 启动解析器和交易接口
		 * 3. 启动UFT引擎
		 * 4. 运行模拟Tick数据测试
		 * 5. 输出延迟统计结果
		 */
		void run();

	private:
		/**
		 * @brief 初始化模块（解析器和交易接口）
		 * @return 返回初始化是否成功（布尔值）
		 * 
		 * 创建并初始化测试用的解析器和交易接口适配器。
		 */
		bool initModules();

		/**
		 * @brief 初始化策略
		 * @return 返回初始化是否成功（布尔值）
		 * 
		 * 创建测试策略并添加到引擎中。
		 */
		bool initStrategies();

		/**
		 * @brief 初始化UFT引擎
		 * @param cfg 引擎配置（WTSVariant指针）
		 * @return 返回初始化是否成功（布尔值）
		 * 
		 * 初始化UFT引擎，设置数据管理器和适配器管理器。
		 */
		bool initEngine(WTSVariant* cfg);

	private:
		TraderAdapterMgr	_traders;      // 交易适配器管理器，管理交易接口适配器
		ParserAdapterMgr	_parsers;      // 解析器适配器管理器，管理行情解析器适配器
		UftStrategyMgr		_stra_mgr;     // UFT策略管理器，管理策略实例

		WtUftEngine			_engine;        // UFT引擎实例，核心交易引擎

		WTSBaseDataMgr		_bd_mgr;        // 基础数据管理器，管理交易时段、品种、合约等基础数据

		uint32_t			_times;         // 测试次数（32位无符号整数），要模拟的Tick数量
		uint32_t			_core;          // CPU核心编号（32位无符号整数），测试线程要绑定的CPU核心（0表示不绑定）
	};
}

