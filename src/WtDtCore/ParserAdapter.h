/*!
 * \file ParserAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 行情解析器适配器类定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了ParserAdapter（行情解析器适配器）类，是连接行情解析器（IParserApi）
 * 和数据管理器（DataManager）的桥梁。采用适配器模式（Adapter Pattern），将Parser
 * 产生的行情数据适配到框架的数据处理流程中。
 * 
 * 核心设计理念：
 * 
 * 1. 适配器模式（Adapter Pattern）：
 *    - ParserAdapter实现IParserSpi接口
 *    - 接收来自Parser的行情数据回调
 *    - 将数据转发给DataManager处理
 *    - 为Parser提供必要的回调服务（如BaseDataMgr）
 * 
 * 2. 动态加载机制：
 *    - 支持从动态库加载Parser模块
 *    - 也支持直接注入Parser实例（initExt）
 *    - 灵活的模块化设计
 * 
 * 3. 数据过滤功能：
 *    - 支持按交易所过滤（exchg_filter）
 *    - 支持按合约代码过滤（code_filter）
 *    - 减少不必要的数据订阅和处理
 * 
 * 4. 智能订阅策略：
 *    - 自动订阅符合过滤条件的合约
 *    - 支持全市场订阅
 *    - 支持品种级订阅（订阅整个品种的所有合约）
 * 
 * 架构设计：
 * 
 *   ┌─────────────┐
 *   │IParserApi   │ (ParserCTP/ParserXTP等)
 *   │(行情解析器) │
 *   └──────┬──────┘
 *          │ 行情回调
 *          ↓
 *   ┌─────────────────┐
 *   │ParserAdapter    │ <-- 本类（适配器）
 *   │ 实现IParserSpi  │
 *   └──────┬──────────┘
 *          │ 数据转发
 *          ↓
 *   ┌─────────────┐        ┌──────────────┐
 *   │DataManager  │  ←───  │IndexFactory  │
 *   │(数据管理器) │        │(指数工厂)    │
 *   └─────────────┘        └──────────────┘
 * 
 * 数据流转过程：
 * 
 * 1. 初始化流程：
 *    init() → 加载Parser模块 → 创建Parser实例
 *    → 设置过滤器 → 订阅合约 → Parser连接服务器
 * 
 * 2. 行情接收流程：
 *    Parser收到行情 → handleQuote()回调
 *    → 验证数据 → DataManager.writeTick()
 *    → IndexFactory.handle_quote() → 数据存储和广播
 * 
 * 3. 清理流程：
 *    release() → Parser.release() → 销毁Parser对象
 * 
 * 过滤器使用场景：
 * 
 * 1. 交易所过滤（_exchg_filter）：
 *    - 只订阅特定交易所的行情
 *    - 配置："filter": "SHFE,DCE"
 *    - 效果：只接收上期所和大商所的行情
 * 
 * 2. 合约过滤（_code_filter）：
 *    - 只订阅指定的合约
 *    - 配置："code": "SHFE.rb2105,DCE.i2105"
 *    - 效果：只接收指定的两个合约
 * 
 * 3. 品种订阅：
 *    - 配置："code": "SHFE.rb"
 *    - 效果：订阅rb品种的所有合约
 * 
 * 4. 全市场订阅：
 *    - 不配置filter和code
 *    - 效果：订阅所有合约
 * 
 * ParserAdapterMgr管理器类：
 * - 管理多个ParserAdapter实例
 * - 支持同时连接多个行情源
 * - 提供统一的启动和释放接口
 * 
 * 设计模式应用：
 * - 适配器模式（Adapter）：适配Parser到框架
 * - 代理模式（Proxy）：代理Parser的行为
 * - 外观模式（Facade）：简化Parser的使用
 * - 策略模式（Strategy）：不同的Parser策略
 * 
 * 技术特点：
 * - 私有继承boost::noncopyable：禁止拷贝和赋值
 * - 智能指针管理：自动管理生命周期
 * - RAII设计：资源在构造时获取，析构时释放
 * - 异常安全：使用智能指针和RAII保证异常安全
 */

#pragma once                                                // 防止头文件重复包含

#include <set>                                              // STL集合容器
#include <vector>                                           // STL向量容器
#include <memory>                                           // 智能指针
#include <boost/core/noncopyable.hpp>                       // Boost禁止拷贝基类
#include "../Includes/IParserApi.h"                         // 行情解析器接口

// 前向声明
NS_WTP_BEGIN                                                // 开始WonderTrader命名空间
class WTSVariant;                                           // 配置参数类
NS_WTP_END                                                  // 结束WonderTrader命名空间

USING_NS_WTP;                                               // 使用WonderTrader命名空间

class wxMainFrame;                                          // wxWidgets主窗口类（未使用，可能是历史遗留）
class WTSBaseDataMgr;                                       // 基础数据管理器
class DataManager;                                          // 数据管理器
class IndexFactory;                                         // 指数工厂

/**
 * @class ParserAdapter
 * @brief 行情解析器适配器类
 * 
 * 该类是行情解析器（IParserApi）的适配器，负责：
 * 1. 管理Parser的生命周期（创建、初始化、运行、销毁）
 * 2. 接收Parser的行情数据回调（实现IParserSpi接口）
 * 3. 将数据转发给DataManager和IndexFactory
 * 4. 提供数据过滤功能（交易所过滤、合约过滤）
 * 5. 为Parser提供必要的服务（BaseDataMgr、日志等）
 * 
 * 继承关系：
 * - 公有继承IParserSpi：实现Parser回调接口
 * - 私有继承boost::noncopyable：禁止拷贝
 * 
 * 禁止拷贝的原因：
 * - ParserAdapter管理着Parser的所有权
 * - 拷贝会导致所有权不明确
 * - 包含不可拷贝的成员（如配置对象）
 * - 使用智能指针管理，避免拷贝问题
 * 
 * 典型使用流程：
 * @code
 *   // 1. 创建适配器
 *   ParserAdapterPtr adapter(new ParserAdapter(bdMgr, dtMgr, idxFactory));
 *   
 *   // 2. 初始化（方式1：从配置加载Parser）
 *   adapter->init("parser_ctp", config);
 *   
 *   // 2. 初始化（方式2：直接注入Parser实例）
 *   adapter->initExt("parser_ext", customParser);
 *   
 *   // 3. 启动Parser
 *   adapter->run();
 *   
 *   // 4. 接收数据...（自动通过回调处理）
 *   
 *   // 5. 清理
 *   adapter->release();
 * @endcode
 * 
 * 配置示例：
 * @code
 *   {
 *     "module": "ParserCTP",              // Parser模块名称
 *     "filter": "SHFE,DCE",               // 交易所过滤（可选）
 *     "code": "SHFE.rb2105,DCE.i2105",   // 合约过滤（可选）
 *     "front": "tcp://180.168.146.187:10131",  // Parser特定配置
 *     "broker": "9999",
 *     "user": "username",
 *     "pass": "password"
 *   }
 * @endcode
 * 
 * 线程模型：
 * - ParserAdapter本身不创建线程
 * - Parser内部可能创建工作线程
 * - 回调在Parser的线程中执行
 * - 需要注意线程安全问题
 */
class ParserAdapter : public IParserSpi, private boost::noncopyable
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化ParserAdapter对象，保存必要的依赖组件引用。
	 * 
	 * @param bgMgr 基础数据管理器指针（提供合约信息、交易日历等）
	 * @param dtMgr 数据管理器指针（接收行情数据）
	 * @param idxFactory 指数工厂指针（处理指数计算，可为NULL）
	 */
	ParserAdapter(WTSBaseDataMgr * bgMgr, DataManager* dtMgr, IndexFactory *idxFactory);
	
	/**
	 * @brief 析构函数
	 * 
	 * 清理ParserAdapter对象。
	 * 
	 * @warning 析构前应该调用release()释放Parser资源
	 */
	~ParserAdapter();

public:
	/**
	 * @brief 初始化适配器（从配置加载Parser模块）
	 * 
	 * 该方法从配置中读取Parser模块名称，动态加载模块并创建Parser实例。
	 * 
	 * 初始化流程：
	 * 1. 保存适配器ID和配置
	 * 2. 从配置中读取module参数
	 * 3. 构建模块的完整路径（支持多个查找路径）
	 * 4. 加载动态库
	 * 5. 获取createParser函数指针
	 * 6. 创建Parser实例
	 * 7. 解析过滤器配置
	 * 8. 注册回调接口（registerSpi）
	 * 9. 初始化Parser（调用Parser的init方法）
	 * 10. 订阅合约
	 * 
	 * 查找路径：
	 * 1. 当前目录：直接查找模块文件
	 * 2. parsers目录：<module_dir>/parsers/<module>
	 * 
	 * 过滤器配置：
	 * - filter：交易所过滤，逗号分隔（如"SHFE,DCE"）
	 * - code：合约过滤，逗号分隔（如"SHFE.rb2105,DCE.i2105"）
	 * 
	 * @param id 适配器唯一标识符（用于日志和管理）
	 * @param cfg 配置参数对象
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * @note 配置会被保存并引用计数+1
	 * @note 失败时会记录详细的错误日志
	 * 
	 * @see run() 启动Parser连接
	 */
	bool	init(const char* id, WTSVariant* cfg);

	/**
	 * @brief 初始化适配器（直接注入Parser实例）
	 * 
	 * 该方法用于外部已创建好的Parser实例，直接注入到适配器中。
	 * 适用于：
	 * - 自定义的Parser实现
	 * - 测试场景（注入Mock对象）
	 * - 需要特殊初始化逻辑的Parser
	 * 
	 * 初始化流程：
	 * 1. 保存Parser实例指针
	 * 2. 保存适配器ID
	 * 3. 注册回调接口
	 * 4. 初始化Parser（传入NULL配置）
	 * 5. 订阅所有合约（全市场订阅）
	 * 
	 * @param id 适配器唯一标识符
	 * @param api Parser实例指针（外部创建和管理）
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * @note Parser实例由外部管理，适配器不负责销毁
	 * @note _remover会被设置为NULL
	 * 
	 * @see init() 从配置加载的初始化方法
	 */
	bool	initExt(const char* id, IParserApi* api);

	/**
	 * @brief 释放适配器资源
	 * 
	 * 该方法释放Parser和相关资源。
	 * 
	 * 释放流程：
	 * 1. 设置停止标志
	 * 2. 调用Parser的release()方法
	 * 3. 使用_remover销毁Parser对象（如果有）
	 * 
	 * @note 对于initExt方式创建的，只调用release()不销毁对象
	 * @note 配置对象的释放由调用者负责
	 */
	void	release();

	/**
	 * @brief 启动Parser连接
	 * 
	 * 该方法调用Parser的connect()方法，启动与行情服务器的连接。
	 * 连接是异步的，实际连接结果通过回调通知。
	 * 
	 * @return bool 连接启动成功返回true，失败返回false
	 * 
	 * @note 调用前应该先调用init()或initExt()
	 * @note Parser为NULL时返回false
	 */
	bool	run();

	/**
	 * @brief 获取适配器ID
	 * 
	 * @return const char* 适配器ID的C风格字符串
	 * 
	 * @note 内联函数，零开销
	 */
	const char* id() const { return _id.c_str(); }

public:
	//////////////////////////////////////////////////////////////////////////
	// IParserSpi 接口实现
	// 以下方法实现了IParserSpi接口，接收来自Parser的各种回调
	//////////////////////////////////////////////////////////////////////////
	
	/**
	 * @brief 处理合约列表回调（IParserSpi接口）
	 * 
	 * Parser在连接成功后可能推送合约列表。
	 * 当前实现为空，未使用此功能。
	 * 
	 * @param aySymbols 合约代码数组
	 * 
	 * @note 当前为空实现
	 */
	virtual void handleSymbolList(const WTSArray* aySymbols) override;

	/**
	 * @brief 处理Tick行情数据回调（IParserSpi接口 - 最重要）
	 * 
	 * Parser每收到一笔Tick行情都会回调此方法。这是最核心的数据处理接口。
	 * 
	 * 处理流程：
	 * 1. 检查停止标志
	 * 2. 验证数据有效性（交易日期、行情日期）
	 * 3. 验证合约信息
	 * 4. 写入DataManager
	 * 5. 转发给IndexFactory（如果存在）
	 * 
	 * @param quote Tick行情数据指针
	 * @param procFlag 处理标志（0=正常，1=仅写入不缓存）
	 * 
	 * @see DataManager::writeTick() 数据写入方法
	 * @see IndexFactory::handle_quote() 指数处理方法
	 */
	virtual void handleQuote(WTSTickData *quote, uint32_t procFlag) override;

	/**
	 * @brief 处理委托队列数据回调（IParserSpi接口）
	 * 
	 * Parser收到委托队列数据时回调此方法。
	 * 
	 * @param ordQueData 委托队列数据指针
	 */
	virtual void handleOrderQueue(WTSOrdQueData* ordQueData) override;

	/**
	 * @brief 处理逐笔成交数据回调（IParserSpi接口）
	 * 
	 * Parser收到逐笔成交数据时回调此方法。
	 * 
	 * @param transData 逐笔成交数据指针
	 */
	virtual void handleTransaction(WTSTransData* transData) override;

	/**
	 * @brief 处理逐笔委托数据回调（IParserSpi接口）
	 * 
	 * Parser收到逐笔委托数据时回调此方法。
	 * 
	 * @param ordDetailData 逐笔委托数据指针
	 */
	virtual void handleOrderDetail(WTSOrdDtlData* ordDetailData) override;

	/**
	 * @brief 处理Parser日志回调（IParserSpi接口）
	 * 
	 * Parser通过此方法输出日志，适配器将日志转发到框架的日志系统。
	 * 
	 * @param ll 日志级别
	 * @param message 日志内容
	 * 
	 * @see WTSLogger 日志系统
	 */
	virtual void handleParserLog(WTSLogLevel ll, const char* message) override;

	/**
	 * @brief 获取基础数据管理器（IParserSpi接口）
	 * 
	 * Parser通过此方法获取基础数据管理器，用于验证合约信息等。
	 * 
	 * @return IBaseDataMgr* 基础数据管理器指针
	 */
	virtual IBaseDataMgr* getBaseDataMgr() override;

private:
	/**
	 * @brief Parser实例指针
	 * 
	 * 指向实际的Parser对象（如ParserCTP、ParserXTP等）。
	 * - init()方式：由动态库创建，适配器管理生命周期
	 * - initExt()方式：外部创建，适配器不管理生命周期
	 */
	IParserApi*			_parser_api;
	
	/**
	 * @brief Parser销毁函数指针
	 * 
	 * 指向动态库中的deleteParser函数，用于销毁Parser实例。
	 * - init()方式：指向有效函数
	 * - initExt()方式：为NULL
	 */
	FuncDeleteParser	_remover;
	
	/**
	 * @brief 基础数据管理器指针
	 * 
	 * 用于验证合约信息、查询交易日历等。
	 */
	WTSBaseDataMgr*		_bd_mgr;
	
	/**
	 * @brief 数据管理器指针
	 * 
	 * 接收行情数据并进行存储和广播。
	 */
	DataManager*		_dt_mgr;
	
	/**
	 * @brief 指数工厂指针
	 * 
	 * 处理指数计算相关的逻辑，可为NULL。
	 */
	IndexFactory*		_idx_fact;

	/**
	 * @brief 停止标志
	 * 
	 * 控制适配器是否继续处理数据。
	 * - false：正常处理
	 * - true：停止处理，丢弃所有数据
	 */
	bool				_stopped;

	/**
	 * @typedef ExchgFilter
	 * @brief 交易所/合约过滤器类型
	 * 
	 * 使用哈希集合存储过滤条件，O(1)查询性能。
	 */
	typedef wt_hashset<std::string>	ExchgFilter;
	
	/**
	 * @brief 交易所过滤器
	 * 
	 * 存储允许订阅的交易所列表。
	 * - 为空：不过滤，订阅所有交易所
	 * - 非空：只订阅列表中的交易所
	 * 
	 * 示例：{"SHFE", "DCE"} 表示只订阅上期所和大商所
	 */
	ExchgFilter			_exchg_filter;
	
	/**
	 * @brief 合约代码过滤器
	 * 
	 * 存储允许订阅的合约代码列表。
	 * - 为空：不过滤（如果exchg_filter也为空，订阅全市场）
	 * - 非空：只订阅列表中的合约
	 * 
	 * 支持格式：
	 * - "SHFE.rb2105"：具体合约
	 * - "SHFE.rb"：整个品种
	 * 
	 * @note code_filter优先级高于exchg_filter
	 */
	ExchgFilter			_code_filter;
	
	/**
	 * @brief 配置对象指针
	 * 
	 * 保存初始化时的配置参数，供后续使用。
	 * 使用引用计数管理生命周期。
	 */
	WTSVariant*			_cfg;
	
	/**
	 * @brief 适配器唯一标识符
	 * 
	 * 用于日志输出、管理器查找等。
	 * 应该在系统中唯一。
	 */
	std::string			_id;
};

/**
 * @typedef ParserAdapterPtr
 * @brief ParserAdapter智能指针类型
 * 
 * 使用智能指针管理ParserAdapter的生命周期，自动释放内存。
 */
typedef std::shared_ptr<ParserAdapter>	ParserAdapterPtr;

/**
 * @typedef ParserAdapterMap
 * @brief ParserAdapter映射表类型
 * 
 * 从适配器ID映射到适配器智能指针。
 */
typedef wt_hashmap<std::string, ParserAdapterPtr>	ParserAdapterMap;

/**
 * @class ParserAdapterMgr
 * @brief 行情解析器适配器管理器类
 * 
 * 该类管理多个ParserAdapter实例，提供统一的管理接口。
 * 支持同时连接多个行情源，每个行情源对应一个ParserAdapter。
 * 
 * 主要功能：
 * 1. 添加适配器：addAdapter()
 * 2. 获取适配器：getAdapter()
 * 3. 统一启动：run()
 * 4. 统一释放：release()
 * 5. 查询数量：size()
 * 
 * 使用场景：
 * - 同时连接CTP和XTP
 * - 主备行情源切换
 * - 多数据源融合
 * 
 * 继承关系：
 * - 私有继承boost::noncopyable：禁止拷贝
 * 
 * 典型使用：
 * @code
 *   ParserAdapterMgr mgr;
 *   
 *   // 添加CTP行情源
 *   ParserAdapterPtr ctp(new ParserAdapter(...));
 *   ctp->init("ctp", ctpConfig);
 *   mgr.addAdapter("ctp", ctp);
 *   
 *   // 添加XTP行情源
 *   ParserAdapterPtr xtp(new ParserAdapter(...));
 *   xtp->init("xtp", xtpConfig);
 *   mgr.addAdapter("xtp", xtp);
 *   
 *   // 统一启动
 *   mgr.run();
 *   
 *   // 获取特定适配器
 *   ParserAdapterPtr p = mgr.getAdapter("ctp");
 *   
 *   // 统一释放
 *   mgr.release();
 * @endcode
 */
class ParserAdapterMgr : private boost::noncopyable
{
public:
	/**
	 * @brief 释放所有适配器
	 * 
	 * 遍历所有适配器，依次调用release()释放资源。
	 * 
	 * @note 会清空内部映射表
	 */
	void	release();

	/**
	 * @brief 启动所有适配器
	 * 
	 * 遍历所有适配器，依次调用run()启动Parser连接。
	 * 
	 * @note 会记录启动的Parser数量
	 */
	void	run();

	/**
	 * @brief 获取指定ID的适配器
	 * 
	 * @param id 适配器ID
	 * @return ParserAdapterPtr 适配器智能指针，不存在返回空指针
	 */
	ParserAdapterPtr getAdapter(const char* id);

	/**
	 * @brief 添加适配器到管理器
	 * 
	 * @param id 适配器ID（应该唯一）
	 * @param adapter 适配器智能指针
	 * @return bool 添加成功返回true，ID重复或参数无效返回false
	 * 
	 * @note ID重复会记录错误日志
	 */
	bool	addAdapter(const char* id, ParserAdapterPtr& adapter);

	/**
	 * @brief 获取适配器数量
	 * 
	 * @return uint32_t 当前管理的适配器数量
	 */
	uint32_t size() const { return (uint32_t)_adapters.size(); }

public:
	/**
	 * @brief 适配器映射表（公有成员，便于外部访问）
	 * 
	 * 存储所有的ParserAdapter实例。
	 * - Key：适配器ID
	 * - Value：适配器智能指针
	 * 
	 * @note 设计为public便于遍历和高级操作
	 */
	ParserAdapterMap _adapters;
};

