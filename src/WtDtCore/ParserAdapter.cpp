/*!
 * \file ParserAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 行情解析器适配器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是ParserAdapter类的具体实现，提供了行情解析器的适配和管理功能。
 * 该模块实现了复杂的订阅策略，支持灵活的过滤配置，是连接Parser和DataManager
 * 的关键桥梁。
 * 
 * 核心实现逻辑：
 * 
 * 1. 动态加载与创建：
 *    - 从配置文件读取模块名称
 *    - 支持多路径查找（当前目录、parsers子目录）
 *    - 动态加载Parser动态库
 *    - 获取创建和销毁函数指针
 *    - 创建Parser实例
 * 
 * 2. 智能订阅策略：
 *    优先级：code_filter > exchg_filter > 全市场
 *    
 *    a) code_filter模式（优先级最高）：
 *       - 解析每个代码（支持多种格式）
 *       - "SSE.600000"：具体合约
 *       - "SSE"："SSE.股票代码"
 *       - "SSE.ETF"：订阅整个品种
 *       - 如果是品种，订阅该品种的所有合约
 *    
 *    b) exchg_filter模式：
 *       - 订阅指定交易所的所有合约
 *       - 例如：filter="SHFE,DCE" 订阅上期所和大商所
 *    
 *    c) 全市场模式：
 *       - 无任何过滤器时
 *       - 订阅所有交易所的所有合约
 * 
 * 3. 数据验证与转发：
 *    - 验证数据的基本有效性（日期、时间）
 *    - 验证合约信息存在性
 *    - 设置合约信息到数据对象
 *    - 转发给DataManager
 *    - 转发给IndexFactory（如果存在）
 * 
 * 4. 状态管理：
 *    - 使用_stopped标志控制数据处理
 *    - stopped时丢弃所有数据
 *    - 避免关闭过程中的数据处理
 * 
 * 订阅策略详解：
 * 
 * 合约代码格式支持：
 * - 单段：exchg（只有交易所，错误格式）
 * - 两段：exchg.code（交易所.合约代码或品种）
 * - 三段：exchg.category.code（交易所.品种类别.代码，如CFFEX.IF.2105）
 * 
 * 品种订阅逻辑：
 * - 先尝试作为合约查找
 * - 如果找不到，尝试作为品种查找
 * - 如果是品种，订阅该品种的所有合约
 * - 支持动态扩展（新合约自动包含）
 * 
 * 数据处理优化：
 * - 早期返回：无效数据直接返回，避免后续处理
 * - 合约信息缓存：设置到数据对象，避免重复查询
 * - 条件判断优化：先检查停止标志和基本有效性
 * 
 * 日志策略：
 * - 使用动态日志分类：log_dyn("parser", id, ...)
 * - 按Parser ID分类日志，便于问题定位
 * - 重要事件记录INFO级别
 * - 错误情况记录ERROR/FATAL级别
 */

#include "ParserAdapter.h"                      // 包含ParserAdapter类定义
#include "DataManager.h"                        // 包含DataManager类定义
#include "StateMonitor.h"                       // 包含StateMonitor类定义（SS_RECEIVING等）
#include "WtHelper.h"                           // 包含辅助工具类
#include "IndexFactory.h"                       // 包含IndexFactory类定义

#include "../Share/StrUtil.hpp"                 // 包含字符串工具类
#include "../Share/DLLHelper.hpp"               // 包含动态库加载工具

#include "../Includes/WTSVariant.hpp"           // 包含配置参数类
#include "../Includes/WTSContractInfo.hpp"      // 包含合约信息类
#include "../Includes/WTSDataDef.hpp"           // 包含数据定义
#include "../Includes/WTSVariant.hpp"           // 重复包含（可删除）

#include "../WTSTools/WTSBaseDataMgr.h"         // 包含基础数据管理器
#include "../WTSTools/WTSLogger.h"              // 包含日志系统


//////////////////////////////////////////////////////////////////////////
// ParserAdapter 类实现
//////////////////////////////////////////////////////////////////////////

/**
 * @brief 构造函数实现
 * 
 * 使用初始化列表初始化所有成员变量。
 * 
 * @param bgMgr 基础数据管理器指针
 * @param dtMgr 数据管理器指针
 * @param idxFactory 指数工厂指针（可为NULL）
 */
ParserAdapter::ParserAdapter(WTSBaseDataMgr * bgMgr, DataManager* dtMgr, IndexFactory *idxFactory)
	: _parser_api(NULL)              // Parser实例指针初始化为空
	, _remover(NULL)                 // 销毁函数指针初始化为空
	, _stopped(false)                // 停止标志初始化为false（运行状态）
	, _bd_mgr(bgMgr)                 // 保存基础数据管理器指针
	, _dt_mgr(dtMgr)                 // 保存数据管理器指针
	, _idx_fact(idxFactory)          // 保存指数工厂指针
	, _cfg(NULL)                     // 配置对象指针初始化为空
	// _exchg_filter、_code_filter、_id使用默认构造函数
{
}


/**
 * @brief 析构函数实现
 * 
 * 析构函数为空。资源释放应该通过release()方法完成。
 */
ParserAdapter::~ParserAdapter()
{
}

/**
 * @brief 初始化适配器（外部注入Parser实例）实现
 * 
 * 该方法用于直接注入外部创建的Parser实例，不进行动态库加载。
 * 适用于自定义Parser或测试场景。
 * 
 * @param id 适配器ID
 * @param api Parser实例指针
 * @return bool 初始化成功返回true，失败返回false
 */
bool ParserAdapter::initExt(const char* id, IParserApi* api)
{
	// 参数验证：Parser指针不能为空
	if (api == NULL)
		return false;

	// 保存Parser实例指针（外部创建，外部管理生命周期）
	_parser_api = api;
	
	// 保存适配器ID
	_id = id;

	if (_parser_api)                            // 双重检查（其实前面已经检查过）
	{
		// 注册回调接口
		// Parser会通过IParserSpi接口回调ParserAdapter
		_parser_api->registerSpi(this);

		// 初始化Parser（传入NULL配置）
		// 外部注入的Parser应该已经配置好，这里只是形式上调用
		if (_parser_api->init(NULL))
		{
			// 初始化成功，订阅所有合约（全市场订阅）
			ContractSet contractSet;                // 合约代码集合
			
			// 获取所有合约
			WTSArray* ayContract = _bd_mgr->getContracts();
			WTSArray::Iterator it = ayContract->begin();
			for (; it != ayContract->end(); it++)
			{
				// 获取合约信息
				WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
				// 将完整代码（如"SHFE.rb2105"）添加到订阅集合
				contractSet.insert(contract->getFullCode());
			}

			// 释放合约数组（引用计数-1）
			ayContract->release();

			// 订阅所有合约
			_parser_api->subscribe(contractSet);
			
			// 清空订阅集合（释放内存）
			contractSet.clear();
		}
		else
		{
			// Parser初始化失败
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());
		}
	}

	return true;
}


/**
 * @brief 初始化适配器（从配置加载Parser）实现
 * 
 * 这是最复杂的初始化方法，包含完整的动态加载、过滤器设置、订阅逻辑。
 * 
 * @param id 适配器ID
 * @param cfg 配置对象
 * @return bool 初始化成功返回true，失败返回false
 */
bool ParserAdapter::init(const char* id, WTSVariant* cfg)
{
	// 参数验证
	if (cfg == NULL)
		return false;

	// 保存适配器ID
	_id = id;

	// 防止重复初始化
	if (_cfg != NULL)
		return false;

	// 保存配置对象并增加引用计数
	_cfg = cfg;
	_cfg->retain();                             // 引用计数+1，防止配置被释放

	{
		// ===== 第一部分：动态加载Parser模块 =====
		
		// 检查配置中是否包含module参数
		if (cfg->getString("module").empty())
			return false;                       // module是必需参数

		// 构建模块文件名
		// wrap_module：添加平台特定的前缀和后缀
		// "lib"前缀：Linux需要，Windows可选
		// 例如："ParserCTP" → Windows:"ParserCTP.dll", Linux:"libParserCTP.so"
		std::string module = DLLHelper::wrap_module(cfg->getCString("module"), "lib");;

		// 尝试在当前目录查找模块文件
		if (!StdFile::exists(module.c_str()))
		{
			// 当前目录不存在，尝试在parsers子目录查找
			module = WtHelper::get_module_dir();    // 获取模块根目录
			module += "parsers/";                   // 添加parsers子目录
			module += DLLHelper::wrap_module(cfg->getCString("module"), "lib");  // 添加模块文件名
		}

		// 加载动态库
		DllHandle hInst = DLLHelper::load_library(module.c_str());
		if (hInst == NULL)                      // 加载失败
		{
			// 记录详细的错误信息
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser module {} loading failed", _id.c_str(), module.c_str());
			return false;
		}
		else                                    // 加载成功
		{
			// 记录成功信息
			WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] Parser module {} loaded", _id.c_str(), module.c_str());
		}

		// 获取createParser函数指针
		// "createParser"：约定的工厂函数名称
		FuncCreateParser pFuncCreateParser = (FuncCreateParser)DLLHelper::get_symbol(hInst, "createParser");
		if (NULL == pFuncCreateParser)          // 未找到函数
		{
			// 这是致命错误，模块不符合规范
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Entrance function createParser not found", _id.c_str());
			return false;
		}

		// 调用工厂函数创建Parser实例
		_parser_api = pFuncCreateParser();
		if (NULL == _parser_api)                // 创建失败
		{
			// 这可能是内存不足或其他内部错误
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Creating parser api failed", _id.c_str());
			return false;
		}

		// 获取deleteParser函数指针（用于后续销毁）
		_remover = (FuncDeleteParser)DLLHelper::get_symbol(hInst, "deleteParser");
	}

	// ===== 第二部分：解析过滤器配置 =====

	// 解析交易所过滤器配置
	const std::string& strFilter = cfg->getString("filter");
	if (!strFilter.empty())                     // 如果配置了filter参数
	{
		// 使用逗号分割字符串
		// 例如："SHFE,DCE,CZCE" → {"SHFE", "DCE", "CZCE"}
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");
		auto it = ayFilter.begin();
		for (; it != ayFilter.end(); it++)
		{
			// 将每个交易所代码插入过滤器集合
			_exchg_filter.insert(*it);
		}
	}

	// 解析合约代码过滤器配置
	std::string strCodes = cfg->getString("code");
	if (!strCodes.empty())                      // 如果配置了code参数
	{
		// 使用逗号分割字符串
		// 例如："SHFE.rb2105,DCE.i2105" → {"SHFE.rb2105", "DCE.i2105"}
		const StringVector &ayCodes = StrUtil::split(strCodes, ",");
		auto it = ayCodes.begin();
		for (; it != ayCodes.end(); it++)
		{
			// 将每个合约代码插入过滤器集合
			_code_filter.insert(*it);
		}
	}

	// ===== 第三部分：初始化Parser并订阅合约 =====

	if (_parser_api)                            // 如果Parser创建成功
	{
		// 注册回调接口（将this指针作为IParserSpi传给Parser）
		_parser_api->registerSpi(this);

		// 初始化Parser（传入完整配置）
		if (_parser_api->init(cfg))
		{
			// Parser初始化成功，开始构建订阅列表
			ContractSet contractSet;            // 用于存储要订阅的合约代码
			
			// ===== 订阅策略1：合约代码过滤（优先级最高） =====
			if (!_code_filter.empty())          // 如果配置了合约过滤器
			{
				// 遍历过滤器中的所有代码
				ExchgFilter::iterator it = _code_filter.begin();
				for (; it != _code_filter.end(); it++)
				{
					// 解析合约代码格式
					// 全代码形式如：SSE.600000（股票），CFFEX.IF2005（期货）
					std::string code, exchg;            // 合约代码和交易所
					
					// 使用点号分割字符串
					auto ay = StrUtil::split((*it).c_str(), ".");
					
					if (ay.size() == 1)                 // 单段：只有代码，无交易所
						code = ay[0];
					else if (ay.size() == 2)            // 两段：交易所.代码
					{
						exchg = ay[0];                  // 第一段是交易所
						code = ay[1];                   // 第二段是代码
					}
					else if (ay.size() == 3)            // 三段：交易所.类别.代码（如CFFEX.IF.2105）
					{
						exchg = ay[0];                  // 第一段是交易所
						code = ay[2];                   // 第三段是代码（跳过类别）
					}
					
					// 尝试作为合约查找
					WTSContractInfo* contract = _bd_mgr->getContract(code.c_str(), exchg.c_str());
					if (contract)                       // 如果是有效的合约
						// 添加到订阅列表
						contractSet.insert(contract->getFullCode());
					else                                // 如果不是合约
					{
						// 尝试作为品种查找
						// 如果是品种ID，则将该品种下全部合约都加到订阅列表
						WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(exchg.c_str(), code.c_str());
						if (commInfo)                   // 如果是有效的品种
						{
							// 获取该品种的所有合约代码
							const auto& codes = commInfo->getCodes();
							for (const auto& c : codes)
							{
								// 构建完整代码并添加到订阅列表
								// fmt::format：格式化字符串（类似sprintf）
								contractSet.insert(fmt::format("{}.{}", exchg, c.c_str()));
							}
						}
						// 如果既不是合约也不是品种，忽略（可能是配置错误）
					}
				}
			}
			// ===== 订阅策略2：交易所过滤 =====
			else if (!_exchg_filter.empty())    // 如果配置了交易所过滤器
			{
				// 遍历过滤器中的所有交易所
				ExchgFilter::iterator it = _exchg_filter.begin();
				for (; it != _exchg_filter.end(); it++)
				{
					const char* exchg = (*it).c_str();      // 交易所代码
					
					// 获取该交易所的所有合约
					WTSArray* ayContract = _bd_mgr->getContracts(exchg);
					auto cnt = ayContract->size();          // 合约数量
					
					// 遍历所有合约
					WTSArray::Iterator it = ayContract->begin();
					for (; it != ayContract->end(); it++)
					{
						WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
						// 添加到订阅列表
						contractSet.insert(contract->getFullCode());
					}

					// 释放合约数组
					ayContract->release();

					// 记录订阅信息
					WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] {} contracts of {} added to sublist...", _id.c_str(), cnt, exchg);
				}
			}
			// ===== 订阅策略3：全市场订阅（无任何过滤器） =====
			else
			{
				// 获取所有合约（不指定交易所）
				WTSArray* ayContract = _bd_mgr->getContracts();
				WTSArray::Iterator it = ayContract->begin();
				for (; it != ayContract->end(); it++)
				{
					WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
					// 添加到订阅列表
					contractSet.insert(contract->getFullCode());
				}

				// 释放合约数组
				ayContract->release();
				// 全市场订阅通常合约数量很大，这里没有记录日志
			}

			// 执行订阅操作
			// 将构建好的订阅列表传递给Parser
			_parser_api->subscribe(contractSet);
			
			// 清空订阅集合（释放内存）
			contractSet.clear();
		}
		else                                    // Parser初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());
		}
	}
	else                                        // Parser创建失败
	{
		WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: creating api failed...", _id.c_str());
	}

	return true;
}

/**
 * @brief 释放适配器资源实现
 * 
 * 该方法释放Parser和相关资源。
 */
void ParserAdapter::release()
{
	// 设置停止标志，后续的数据回调会被忽略
	_stopped = true;
	
	if (_parser_api)
	{
		// 调用Parser的release方法，让Parser释放内部资源
		_parser_api->release();
	}

	// 销毁Parser对象
	if (_remover)
		// 如果有销毁函数，使用模块提供的销毁函数
		// 确保内存在正确的堆上释放
		_remover(_parser_api);
	else
		// 如果是initExt方式创建的，使用delete
		// 注意：initExt的Parser由外部管理，这里delete可能不合适
		// 建议：initExt的Parser应该不在这里delete
		delete _parser_api;
}

/**
 * @brief 启动Parser连接实现
 * 
 * @return bool 连接启动成功返回true，失败返回false
 */
bool ParserAdapter::run()
{
	// 检查Parser是否已创建
	if (_parser_api == NULL)
		return false;

	// 调用Parser的connect方法，启动连接
	// 连接是异步的，结果通过回调通知
	_parser_api->connect();
	
	return true;
}

/**
 * @brief 处理合约列表回调实现
 * 
 * 当前为空实现，未使用此功能。
 * 
 * @param aySymbols 合约代码数组
 */
void ParserAdapter::handleSymbolList( const WTSArray* aySymbols )
{
	// 空实现：Parser推送的合约列表暂不处理
}

/**
 * @brief 处理逐笔成交数据回调实现
 * 
 * Parser收到逐笔成交数据时回调此方法。
 * 
 * @param transData 逐笔成交数据指针
 */
void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	// 检查停止标志，已停止时丢弃数据
	if (_stopped)
		return;

	// 验证数据基本有效性
	// actiondate：行情日期，格式YYYYMMDD
	// tradingdate：交易日期，格式YYYYMMDD
	// 任一为0表示数据无效
	if (transData->actiondate() == 0 || transData->tradingdate() == 0)
		return;

	// 获取合约信息（用于验证合约是否存在）
	WTSContractInfo* contract = _bd_mgr->getContract(transData->code(), transData->exchg());
	if (contract == NULL)                       // 合约不存在
		return;                                 // 丢弃数据

	// 设置合约信息到数据对象
	// 后续处理中可以直接使用，避免重复查询
	transData->setContractInfo(contract);

	// 转发给DataManager进行存储和广播
	_dt_mgr->writeTransaction(transData);
}

/**
 * @brief 处理逐笔委托数据回调实现
 * 
 * Parser收到逐笔委托数据时回调此方法。
 * 
 * @param ordDetailData 逐笔委托数据指针
 */
void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDetailData)
{
	// 检查停止标志
	if (_stopped)
		return;

	// 验证数据有效性
	if (ordDetailData->actiondate() == 0 || ordDetailData->tradingdate() == 0)
		return;

	// 获取并验证合约信息
	WTSContractInfo* contract = _bd_mgr->getContract(ordDetailData->code(), ordDetailData->exchg());
	if (contract == NULL)
		return;

	// 设置合约信息
	ordDetailData->setContractInfo(contract);

	// 转发给DataManager
	_dt_mgr->writeOrderDetail(ordDetailData);
}

/**
 * @brief 处理委托队列数据回调实现
 * 
 * Parser收到委托队列数据时回调此方法。
 * 
 * @param ordQueData 委托队列数据指针
 */
void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	// 检查停止标志
	if (_stopped)
		return;

	// 验证数据有效性
	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)
		return;

	// 获取并验证合约信息
	WTSContractInfo* contract = _bd_mgr->getContract(ordQueData->code(), ordQueData->exchg());
	if (contract == NULL)
		return;

	// 设置合约信息
	ordQueData->setContractInfo(contract);
		
	// 转发给DataManager
	_dt_mgr->writeOrderQueue(ordQueData);
}

/**
 * @brief 处理Tick行情数据回调实现（最核心的方法）
 * 
 * 这是ParserAdapter最重要的方法，处理Parser推送的Tick行情数据。
 * 
 * 处理流程：
 * 1. 检查停止标志
 * 2. 验证数据有效性（日期不能为0）
 * 3. 获取或验证合约信息
 * 4. 写入DataManager（存储+广播）
 * 5. 转发给IndexFactory（指数计算）
 * 
 * 数据验证：
 * - actiondate：行情日期，格式YYYYMMDD
 * - tradingdate：交易日期，格式YYYYMMDD
 * - 任一为0表示数据无效，可能是Parser初始化阶段的数据
 * 
 * 合约信息处理：
 * - 优先使用数据对象中的合约信息（Parser可能已设置）
 * - 如果没有，从BaseDataMgr查询
 * - 设置到数据对象，供后续使用
 * 
 * @param quote Tick行情数据指针
 * @param procFlag 处理标志（0=正常，1=仅写入）
 */
void ParserAdapter::handleQuote( WTSTickData *quote, uint32_t procFlag )
{
	// 第一步：检查停止标志
	// 如果适配器已停止，丢弃所有数据
	if (_stopped)
		return;

	// 第二步：验证数据的基本有效性
	// 检查行情日期和交易日期是否有效
	// 日期为0通常表示：
	// 1. Parser初始化阶段的测试数据
	// 2. 数据解析错误
	// 3. 网络传输错误
	if (quote->actiondate() == 0 || quote->tradingdate() == 0)
		return;

	// 第三步：获取合约信息
	// 尝试从Tick对象获取合约信息（Parser可能已设置）
	WTSContractInfo* contract = quote->getContractInfo();
	if (contract == NULL)                       // 如果Tick中没有合约信息
	{
		// 从BaseDataMgr查询合约信息
		contract = _bd_mgr->getContract(quote->code(), quote->exchg());
		
		// 设置合约信息到Tick对象
		// 后续处理（DataManager、IndexFactory）可以直接使用
		// 避免重复查询，提高性能
		quote->setContractInfo(contract);
	}

	// 第四步：验证合约信息
	if (contract == NULL)                       // 如果合约信息不存在
		return;                                 // 丢弃数据（可能是无效合约或配置错误）

	// 第五步：写入DataManager
	// DataManager会：
	// 1. 检查是否可接收（canSessionReceive）
	// 2. 写入磁盘文件
	// 3. 更新内存缓存
	// 4. 广播到所有Caster
	if (!_dt_mgr->writeTick(quote, procFlag))
		return;                                 // 写入失败，不再继续处理

	// 第六步：转发给IndexFactory（指数计算）
	if (_idx_fact)                              // 如果指数工厂存在
		// IndexFactory会检查该Tick是否是指数成分
		// 如果是，触发指数重算
		_idx_fact->handle_quote(quote);
}

/**
 * @brief 处理Parser日志回调实现
 * 
 * Parser通过此方法输出日志，适配器转发到框架日志系统。
 * 
 * @param ll 日志级别
 * @param message 日志内容
 */
void ParserAdapter::handleParserLog( WTSLogLevel ll, const char* message)
{
	// 检查停止标志
	if (_stopped)
		return;

	// 使用"parser"类别的日志
	// log_raw_by_cat：按类别输出原始日志
	// Parser的日志已经格式化，这里直接输出
	WTSLogger::log_raw_by_cat("parser", ll, message);
}

/**
 * @brief 获取基础数据管理器实现
 * 
 * Parser通过此方法获取BaseDataMgr，用于验证合约等。
 * 
 * @return IBaseDataMgr* 基础数据管理器指针
 */
IBaseDataMgr* ParserAdapter::getBaseDataMgr()
{
	return _bd_mgr;
}


//////////////////////////////////////////////////////////////////////////
// ParserAdapterMgr 类实现
// 管理多个ParserAdapter实例的管理器类
//////////////////////////////////////////////////////////////////////////

/**
 * @brief 释放所有适配器实现
 * 
 * 遍历所有适配器，依次调用release()释放资源，最后清空映射表。
 */
void ParserAdapterMgr::release()
{
	// 遍历所有适配器
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		// 调用每个适配器的release方法
		// it->second是ParserAdapterPtr智能指针
		// ->运算符返回ParserAdapter*裸指针
		it->second->release();
	}

	// 清空映射表
	// 智能指针的引用计数减为0，自动删除ParserAdapter对象
	_adapters.clear();
}

/**
 * @brief 添加适配器到管理器实现
 * 
 * 将新的适配器添加到管理器，使用ID作为唯一标识。
 * 
 * @param id 适配器ID（应该唯一）
 * @param adapter 适配器智能指针
 * @return bool 添加成功返回true，失败返回false
 */
bool ParserAdapterMgr::addAdapter(const char* id, ParserAdapterPtr& adapter)
{
	// 参数验证
	if (adapter == NULL || strlen(id) == 0)
		return false;

	// 检查ID是否已存在
	auto it = _adapters.find(id);
	if (it != _adapters.end())                  // ID已存在
	{
		// 记录错误：不允许重复的ID
		WTSLogger::error(" Same name of parsers: %s", id);
		return false;
	}

	// 添加到映射表
	// 使用[]运算符，如果key不存在会自动创建
	_adapters[id] = adapter;

	return true;
}

/**
 * @brief 获取指定ID的适配器实现
 * 
 * @param id 适配器ID
 * @return ParserAdapterPtr 适配器智能指针，不存在返回空指针
 */
ParserAdapterPtr ParserAdapterMgr::getAdapter(const char* id)
{
	// 在映射表中查找
	auto it = _adapters.find(id);
	if (it != _adapters.end())                  // 找到了
	{
		return it->second;                      // 返回智能指针
	}

	// 未找到，返回空智能指针
	return ParserAdapterPtr();
}

/**
 * @brief 启动所有适配器实现
 * 
 * 遍历所有适配器，依次调用run()启动Parser连接。
 */
void ParserAdapterMgr::run()
{
	// 遍历所有适配器
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		// 调用每个适配器的run方法
		// 启动Parser与行情服务器的连接
		it->second->run();
	}

	// 记录启动信息：启动了多少个Parser
	WTSLogger::info("{} parsers started", _adapters.size());
}
