/*!
 * \file HftLatencyTool.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief HFT延迟测试工具类的实现文件
 * 
 * 本文件实现了HftLatencyTool类的所有方法，以及测试用的辅助类。
 * 
 * 实现内容：
 * - HftLatencyTool类的初始化和运行逻辑
 * - TestParser类：模拟行情解析器，生成测试Tick数据
 * - TestTrader类：模拟交易接口，不执行实际交易
 * - TestStrategy类：测试策略，接收Tick并发出交易信号
 * - 延迟测试和统计功能
 * 
 * 测试原理：
 * - 生成指定数量的模拟Tick数据
 * - 通过解析器发送到HFT引擎
 * - 使用高精度计时器统计处理时间
 * - 计算平均延迟（纳秒级）
 */
#include "HftLatencyTool.h"              // 包含类定义头文件
#include "../WtCore/HftStraContext.h"    // HFT策略上下文类

#include "../Includes/WTSVariant.hpp"    // 变体类型，用于配置参数
#include "../Includes/IParserApi.h"      // 解析器接口定义
#include "../Includes/ITraderApi.h"      // 交易接口定义
#include "../Includes/WTSContractInfo.hpp"  // 合约信息类

#include "../WTSTools/WTSLogger.h"        // WonderTrader日志工具
#include "../WTSUtils/WTSCfgLoader.h"     // 配置文件加载器

#include "../Share/StrUtil.hpp"           // 字符串工具函数
#include "../Share/TimeUtils.hpp"         // 时间工具函数
#include "../Share/CpuHelper.hpp"        // CPU辅助工具，提供核心绑定功能


USING_NS_WTP;                            // 使用WonderTrader命名空间

/**
 * @brief HFT延迟测试函数
 * 
 * 测试流程：
 * 1. 创建HftLatencyTool实例
 * 2. 初始化测试环境
 * 3. 运行延迟测试
 * 
 * 该函数是测试程序的入口点，由main.cpp调用。
 */
void test_hft()
{
	hft::HftLatencyTool runner;          // 创建HFT延迟测试工具实例
	runner.init();                        // 初始化测试环境（加载配置、初始化引擎等）

	runner.run();                         // 运行延迟测试（生成模拟数据并统计延迟）
}

namespace hft                             // hft命名空间
{
	/**
	 * @brief 检查浮点数是否有效
	 * @param x 浮点数值
	 * @return 返回有效值，如果x为最大值则返回0.0
	 * 
	 * 检查浮点数是否为最大值（DBL_MAX或FLT_MAX），如果是则返回0.0，否则返回原值。
	 * 用于处理无效的价格数据。
	 */
	inline double checkValid(double x)
	{
		return ((x == DBL_MAX || x == FLT_MAX) ? 0.0 : x);  // 如果x为最大值，返回0.0，否则返回x
	}


	/**
	 * @brief 将时间字符串转换为整数
	 * @param strTime 时间字符串，格式如"10:05:23"
	 * @return 返回时间整数，格式如100523（HHMMSS）
	 * 
	 * 将"HH:MM:SS"格式的时间字符串转换为HHMMSS格式的整数。
	 * 移除冒号分隔符，只保留数字。
	 */
	inline uint32_t strToTime(const char* strTime)
	{
		static char str[10] = { 0 };      // 静态缓冲区，用于存储转换后的字符串
		const char *pos = strTime;       // 字符串位置指针（未使用）
		int idx = 0;                      // 缓冲区索引
		auto len = strlen(strTime);       // 获取字符串长度
		for (std::size_t i = 0; i < len; i++)  // 遍历字符串的每个字符
		{
			if (strTime[i] != ':')        // 如果字符不是冒号
			{
				str[idx] = strTime[i];   // 将字符复制到缓冲区
				idx++;                    // 递增索引
			}
		}
		str[idx] = '\0';                  // 添加字符串结束符

		return strtoul(str, NULL, 10);   // 将字符串转换为无符号长整数（10进制）
	}

	/**
	 * @class TestParser
	 * @brief 测试解析器类，模拟行情解析器
	 * 
	 * 该类实现IParserApi接口，用于生成模拟的Tick数据并发送到引擎。
	 * 用于测试HFT引擎的处理延迟，不依赖外部数据源。
	 */
	class TestParser : public IParserApi
	{
	public:
		/**
		 * @brief 运行延迟测试
		 * @param times 测试次数（32位无符号整数），要生成的Tick数量
		 * 
		 * 测试流程：
		 * 1. 初始化随机数种子
		 * 2. 启动高精度计时器
		 * 3. 循环生成指定数量的Tick数据
		 * 4. 通过回调接口发送到引擎
		 * 5. 统计总耗时和平均延迟
		 * 6. 输出测试结果
		 */
		void	run(uint32_t times)
		{
			srand(time(NULL));            // 初始化随机数种子，使用当前时间
			TimeUtils::Ticker ticker;     // 创建高精度计时器，用于统计耗时
			for (uint32_t i = 0; i < times; i++)  // 循环生成times个Tick数据
			{
				uint32_t actDate = 20220303;// strtoul("20220303", NULL, 10);  // 动作日期：2022年3月3日
				uint32_t actTime = 100523 * 1000 + 500; //strToTime("10:05:23") * 1000 + 500;  // 动作时间：10:05:23.500（毫秒）

				WTSContractInfo* contract = _bd_mgr->getContract("rb2205", "SHFE");  // 从基础数据管理器获取合约信息（螺纹钢2205合约，上海期货交易所）
				if (contract == NULL)     // 如果合约不存在，退出循环
					return;

				double x = rand();        // 生成随机数作为价格

				WTSCommodityInfo* pCommInfo = contract->getCommInfo();  // 获取品种信息

				WTSTickData* tick = WTSTickData::create("rb2205");  // 创建Tick数据对象
				tick->setContractInfo(contract);  // 设置合约信息

				WTSTickStruct& quote = tick->getTickStruct();  // 获取Tick结构体引用
				wt_strcpy(quote.exchg, pCommInfo->getExchg());  // 复制交易所代码

				quote.action_date = actDate;      // 设置动作日期
				quote.action_time = actTime;      // 设置动作时间

				quote.price = x;                  // 设置最新价
				quote.open = x;                   // 设置开盘价
				quote.high = x;                   // 设置最高价
				quote.low = x;                    // 设置最低价
				quote.total_volume = 0;           // 设置总成交量（0表示无成交）
				quote.trading_date = 20220303;    // 设置交易日
				quote.settle_price = x;           // 设置结算价

				quote.open_interest = 0;          // 设置持仓量（0表示无持仓）

				quote.upper_limit = x;            // 设置涨停价
				quote.lower_limit = x;             // 设置跌停价

				quote.pre_close = x;              // 设置昨收盘价
				quote.pre_settle = x;             // 设置昨结算价
				quote.pre_interest = 0;           // 设置昨持仓量

				//委卖价格
				quote.ask_prices[0] = x;          // 卖一价
				quote.ask_prices[1] = x;          // 卖二价
				quote.ask_prices[2] = x;          // 卖三价
				quote.ask_prices[3] = x;          // 卖四价
				quote.ask_prices[4] = x;          // 卖五价

				//委买价格
				quote.bid_prices[0] = x;          // 买一价
				quote.bid_prices[1] = x;          // 买二价
				quote.bid_prices[2] = x;          // 买三价
				quote.bid_prices[3] = x;          // 买四价
				quote.bid_prices[4] = x;          // 买五价

				//委卖量
				quote.ask_qty[0] = 0;             // 卖一量
				quote.ask_qty[1] = 0;             // 卖二量
				quote.ask_qty[2] = 0;             // 卖三量
				quote.ask_qty[3] = 0;             // 卖四量
				quote.ask_qty[4] = 0;             // 卖五量

				//委买量
				quote.bid_qty[0] = 0;             // 买一量
				quote.bid_qty[1] = 0;             // 买二量
				quote.bid_qty[2] = 0;             // 买三量
				quote.bid_qty[3] = 0;             // 买四量
				quote.bid_qty[4] = 0;             // 买五量

				_parser_spi->handleQuote(tick, 0);  // 通过回调接口发送Tick数据到引擎（0表示数据源标识）
				tick->release();                  // 释放Tick数据对象
			}
			auto total = ticker.nano_seconds();   // 获取总耗时（纳秒）
			double t2t = total * 1.0 / times;    // 计算平均延迟（纳秒）：总耗时除以Tick数量
			WTSLogger::warn("{} ticks simulated in {:.0f} ns, HftEngine Innner Latency: {:.3f} ns", times, total*1.0, t2t);  // 输出测试结果：Tick数量、总耗时、平均延迟
		}

	public:
		/**
		 * @brief 注册解析器回调接口
		 * @param listener 解析器回调接口指针
		 * 
		 * 实现IParserApi接口，保存回调接口指针和基础数据管理器。
		 * 当解析器收到行情数据时，会通过回调接口通知引擎。
		 */
		virtual void registerSpi(IParserSpi* listener) override
		{
			_parser_spi = listener;               // 保存解析器回调接口指针
			_bd_mgr = listener->getBaseDataMgr();  // 从回调接口获取基础数据管理器指针
		}

	private:
		IParserSpi*		_parser_spi;             // 解析器回调接口指针，用于发送Tick数据到引擎
		IBaseDataMgr*	_bd_mgr;                 // 基础数据管理器指针，用于获取合约信息
	};

	TestParser* theParser = NULL;        // 全局测试解析器指针，用于在run()方法中调用

	/**
	 * @class TestTrader
	 * @brief 测试交易接口类，模拟交易接口
	 * 
	 * 该类实现ITraderApi接口，用于模拟交易接口。
	 * 不执行实际交易，只提供接口实现，用于测试HFT引擎的交易流程。
	 */
	class TestTrader : public ITraderApi
	{
	public:
		/**
		 * @brief 注册交易回调接口
		 * @param listener 交易回调接口指针
		 * 
		 * 实现ITraderApi接口，保存回调接口指针。
		 * 当交易接口收到交易回报时，会通过回调接口通知引擎。
		 */
		virtual void registerSpi(ITraderSpi* listener) override
		{
			_trader_spi = listener;       // 保存交易回调接口指针
		}

		/**
		 * @brief 生成委托编号
		 * @param buffer 缓冲区指针，用于存储生成的委托编号
		 * @param length 缓冲区长度
		 * @return 返回是否生成成功（布尔值）
		 * 
		 * 实现ITraderApi接口，生成模拟的委托编号。
		 * 测试环境中使用固定值"123456"。
		 */
		virtual bool makeEntrustID(char* buffer, int length) override
		{
			wt_strcpy(buffer, "123456");  // 复制固定委托编号"123456"到缓冲区
			return true;                  // 返回成功
		}

		/**
		 * @brief 下单
		 * @param eutrust 委托单信息指针
		 * @return 返回错误码（整数），0表示成功
		 * 
		 * 实现ITraderApi接口，模拟下单操作。
		 * 测试环境中不执行实际下单，直接返回成功。
		 */
		virtual int orderInsert(WTSEntrust* eutrust) override
		{
			return 0;                     // 返回0表示成功（不执行实际交易）
		}

	private:
		ITraderSpi*	_trader_spi;          // 交易回调接口指针，用于发送交易回报到引擎
	};

	/**
	 * @class TestStrategy
	 * @brief 测试策略类，模拟HFT策略
	 * 
	 * 该类继承自HftStrategy，实现一个简单的测试策略。
	 * 策略逻辑：接收到Tick数据后，发出买入信号。
	 * 用于测试HFT引擎的策略执行流程。
	 */
	class TestStrategy : public HftStrategy
	{
	public:
		/**
		 * @brief 构造函数
		 * @param id 策略ID（字符串）
		 * 
		 * 初始化测试策略，调用基类构造函数。
		 */
		TestStrategy(const char* id) : HftStrategy(id) {}

		/**
		 * @brief 获取执行单元名称
		 * @return 返回策略名称（字符串）
		 * 
		 * 实现HftStrategy接口，返回策略的名称。
		 */
		virtual const char* getName() { return "TestStrategy"; }

		/**
		 * @brief 获取所属执行器工厂名称
		 * @return 返回工厂名称（字符串）
		 * 
		 * 实现HftStrategy接口，返回创建该策略的工厂名称。
		 */
		virtual const char* getFactName() { return "TestStrategyFact"; }


		/**
		 * @brief 策略初始化回调
		 * @param ctx 策略上下文指针
		 * 
		 * 策略初始化时调用，订阅Tick数据。
		 * 订阅"SHFE.rb.2205"合约的Tick数据。
		 */
		virtual void on_init(IHftStraCtx* ctx) override
		{
			ctx->stra_sub_ticks("SHFE.rb.2205");  // 订阅螺纹钢2205合约的Tick数据
		}

		/**
		 * @brief Tick数据回调
		 * @param ctx 策略上下文指针
		 * @param code 合约代码（字符串）
		 * @param newTick 新的Tick数据指针
		 * 
		 * 当接收到订阅的Tick数据时调用。
		 * 策略逻辑：发出买入信号（价格2300，数量1手）。
		 */
		virtual void on_tick(IHftStraCtx* ctx, const char* code, WTSTickData* newTick)
		{
			//ctx->stra_sell("SHFE.rb.2205", 2300, 1, "", HFT_OrderFlag_Nor);  // 注释掉的卖出信号
			ctx->stra_buy("SHFE.rb.2205", 2300, 1, "", HFT_OrderFlag_Nor);  // 发出买入信号：合约、价格、数量、用户标记、订单标志
		}
	};


	/**
	 * @brief HftLatencyTool构造函数实现
	 * 
	 * 初始化HftLatencyTool对象，所有成员变量使用默认值。
	 */
	HftLatencyTool::HftLatencyTool()
	{
	}


	/**
	 * @brief HftLatencyTool析构函数实现
	 * 
	 * 清理资源，智能指针和对象会自动释放。
	 */
	HftLatencyTool::~HftLatencyTool()
	{
	}

	/**
	 * @brief 初始化测试环境的实现
	 * @return 返回初始化是否成功
	 * 
	 * 初始化流程：
	 * 1. 初始化日志系统
	 * 2. 加载配置文件
	 * 3. 加载基础数据（交易时段、品种、合约）
	 * 4. 加载主力合约规则
	 * 5. 初始化动作策略管理器
	 * 6. 读取测试参数（次数、CPU核心）
	 * 7. 初始化引擎、模块、策略
	 */
	bool HftLatencyTool::init()
	{
		WTSLogger::init("logcfg.yaml");  // 初始化日志系统，加载日志配置文件

		WTSVariant* _config = WTSCfgLoader::load_from_file("config.yaml");  // 从文件加载配置文件
		if (_config == NULL)              // 如果加载失败
		{
			WTSLogger::log_raw(LL_ERROR, "Loading config file config.yaml failed");  // 记录错误日志
			return false;                 // 返回失败
		}

		//基础数据文件
		WTSVariant* cfgBF = _config->get("basefiles");  // 获取basefiles配置节点（基础数据文件配置）
		bool isUTF8 = cfgBF->getBoolean("utf-8");      // 获取UTF-8编码标志（未使用）
		if (cfgBF->get("session"))       // 如果配置中有session项（交易时段文件）
			_bd_mgr.loadSessions(cfgBF->getCString("session"));  // 加载交易时段数据

		WTSVariant* cfgItem = cfgBF->get("commodity");  // 获取commodity配置项（品种文件）
		if (cfgItem)                     // 如果配置项存在
		{
			if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件路径）
			{
				_bd_mgr.loadCommodities(cfgItem->asCString());  // 加载品种数据
			}
			else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件路径）
			{
				for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组
				{
					_bd_mgr.loadCommodities(cfgItem->get(i)->asCString());  // 加载每个品种文件
				}
			}
		}

		cfgItem = cfgBF->get("contract");  // 获取contract配置项（合约文件）
		if (cfgItem)                     // 如果配置项存在
		{
			if (cfgItem->type() == WTSVariant::VT_String)  // 如果是字符串类型（单个文件路径）
			{
				_bd_mgr.loadContracts(cfgItem->asCString());  // 加载合约数据
			}
			else if (cfgItem->type() == WTSVariant::VT_Array)  // 如果是数组类型（多个文件路径）
			{
				for (uint32_t i = 0; i < cfgItem->size(); i++)  // 遍历数组
				{
					_bd_mgr.loadContracts(cfgItem->get(i)->asCString());  // 加载每个合约文件
				}
			}
		}

		if (cfgBF->get("hot"))           // 如果配置中有hot项（主力合约规则文件）
		{
			_hot_mgr.loadHots(cfgBF->getCString("hot"));  // 加载主力合约规则
			WTSLogger::log_raw(LL_INFO, "Hot rules loades");  // 记录日志（注意：原文拼写错误，应为"loaded"）
		}

		_act_mgr.init("actpolicy.yaml");  // 初始化动作策略管理器，加载动作策略配置文件

		_times = _config->getUInt32("times");  // 读取测试次数配置
		WTSLogger::warn("{} ticks will be simulated", _times);  // 记录日志：将要模拟的Tick数量

		_core = _config->getUInt32("core");  // 读取CPU核心配置
		WTSLogger::warn("Testing thread will be bind to core {}", _core);  // 记录日志：测试线程将绑定的CPU核心

		initEngine(_config->get("env"));  // 初始化HFT引擎，传入环境配置
		initModules();                   // 初始化模块（解析器和交易接口）
		initStrategies();                // 初始化策略

		_config->release();              // 释放配置对象，减少引用计数
		return true;                     // 返回成功
	}

	/**
	 * @brief 初始化策略的实现
	 * @return 返回初始化是否成功
	 * 
	 * 初始化流程：
	 * 1. 创建策略上下文
	 * 2. 创建并设置测试策略
	 * 3. 获取交易适配器并设置到上下文
	 * 4. 将上下文添加到引擎
	 */
	bool HftLatencyTool::initStrategies()
	{
		HftStraContext* ctx = new HftStraContext(&_engine, "stra", false, 0);  // 创建策略上下文，传入引擎指针、策略ID、是否延迟加载、延迟时间
		ctx->set_strategy(new TestStrategy("stra"));  // 创建测试策略并设置到上下文

		TraderAdapterPtr trader = _traders.getAdapter("trader");  // 从交易适配器管理器获取交易适配器
		ctx->setTrader(trader.get());     // 将交易适配器设置到策略上下文
		trader->addSink(ctx);             // 将策略上下文添加到交易适配器的接收者列表（接收交易回报）

		_engine.addContext(HftContextPtr(ctx));  // 将策略上下文添加到引擎

		return true;                      // 返回成功
	}

	/**
	 * @brief 初始化HFT引擎的实现
	 * @param cfg 引擎配置（WTSVariant指针）
	 * @return 返回初始化是否成功
	 * 
	 * 初始化流程：
	 * 1. 记录日志
	 * 2. 初始化引擎，传入配置和数据管理器
	 * 3. 设置交易适配器管理器
	 */
	bool HftLatencyTool::initEngine(WTSVariant* cfg)
	{
		WTSLogger::warn("Trading enviroment initialzied with engine: HFT");  // 记录日志：交易环境已初始化，使用HFT引擎
		_engine.init(cfg, &_bd_mgr, &_dt_mgr, &_hot_mgr, NULL);  // 初始化引擎，传入配置、基础数据管理器、数据管理器、主力合约管理器、风险监控器（NULL）
		_engine.set_adapter_mgr(&_traders);  // 设置交易适配器管理器

		return true;                      // 返回成功
	}


	/**
	 * @brief 初始化模块的实现
	 * @return 返回初始化是否成功
	 * 
	 * 初始化流程：
	 * 1. 创建测试解析器并添加到解析器适配器管理器
	 * 2. 创建测试交易接口并添加到交易适配器管理器
	 */
	bool HftLatencyTool::initModules()
	{
		{
			theParser = new TestParser();  // 创建测试解析器实例
			ParserAdapterPtr adapter(new ParserAdapter);  // 创建解析器适配器
			adapter->initExt("parser", theParser, &_engine, &_bd_mgr, &_hot_mgr);  // 初始化适配器，传入ID、解析器接口、引擎、基础数据管理器、主力合约管理器
			_parsers.addAdapter("parser", adapter);  // 将适配器添加到解析器适配器管理器
		}

		{
			TestTrader * tester = new TestTrader();  // 创建测试交易接口实例
			TraderAdapterPtr adapter(new TraderAdapter());  // 创建交易适配器
			adapter->initExt("trader", tester, &_bd_mgr, &_act_mgr);  // 初始化适配器，传入ID、交易接口、基础数据管理器、动作策略管理器
			_traders.addAdapter("trader", adapter);  // 将适配器添加到交易适配器管理器
		}

		return true;                      // 返回成功
	}

	/**
	 * @brief 运行延迟测试的实现
	 * 
	 * 运行流程：
	 * 1. 如果配置了CPU核心，绑定当前线程到指定核心
	 * 2. 启动解析器适配器管理器（启动解析器）
	 * 3. 启动交易适配器管理器（启动交易接口）
	 * 4. 启动HFT引擎
	 * 5. 运行测试解析器，生成模拟Tick数据并统计延迟
	 * 6. 捕获异常（防止测试过程中崩溃）
	 */
	void HftLatencyTool::run()
	{
		if (_core != 0)                  // 如果配置了CPU核心（不为0）
		{
			if (!CpuHelper::bind_core(_core - 1))  // 绑定当前线程到指定CPU核心（_core-1因为核心编号从0开始）
			{
				WTSLogger::error("Binding to core {} failed", _core);  // 如果绑定失败，记录错误日志
			}
		}

		try                                 // 异常处理块
		{
			_parsers.run();                 // 启动解析器适配器管理器，启动所有解析器
			_traders.run();                 // 启动交易适配器管理器，启动所有交易接口

			_engine.run();                  // 启动HFT引擎，开始处理数据

			theParser->run(_times);        // 运行测试解析器，生成_times个模拟Tick数据并统计延迟
		}
		catch (...)                         // 捕获所有异常
		{
			// 异常处理：静默处理，防止测试过程中崩溃
		}
	}
}