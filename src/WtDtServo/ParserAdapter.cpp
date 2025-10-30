/*!
 * \file ParserAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader行情解析器适配器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是ParserAdapter（行情解析器适配器）类和ParserAdapterMgr（适配器管理器）类的
 * 具体实现，提供了解析器适配器的完整功能实现。该文件实现了动态加载解析器模块、
 * 解析器初始化、数据过滤、事件处理、订阅管理等核心功能。
 * 
 * 核心实现机制：
 * 
 * 1. 动态库加载（Dynamic Library Loading）：
 *    - 使用DLLHelper动态加载解析器模块
 *    - 通过符号表获取createParser和deleteParser函数
 *    - 支持模块路径的自动查找和解析
 * 
 * 2. 过滤器机制（Filter Mechanism）：
 *    - 支持交易所过滤（exchg_filter）
 *    - 支持合约代码过滤（code_filter）
 *    - 支持品种ID过滤（自动展开为所有合约）
 *    - 灵活的数据过滤规则
 * 
 * 3. 订阅管理（Subscription Management）：
 *    - 根据过滤器构建订阅列表
 *    - 支持多种订阅格式（全代码、交易所.代码、品种ID等）
 *    - 自动处理合约信息的获取和展开
 * 
 * 4. 事件处理（Event Handling）：
 *    - 实现IParserSpi接口的所有回调方法
 *    - 将解析器事件转发给数据服务运行器
 *    - 实现日志的统一管理和转发
 * 
 * 主要功能实现：
 * 
 * 1. ParserAdapter类实现：
 *    - 构造和析构函数
 *    - init()：从配置文件初始化解析器
 *    - initExt()：使用外部API初始化解析器
 *    - release()：释放解析器资源
 *    - run()：启动解析器
 *    - 各种事件处理回调函数
 * 
 * 2. ParserAdapterMgr类实现：
 *    - release()：释放所有适配器
 *    - run()：启动所有适配器
 *    - getAdapter()：获取指定适配器
 *    - addAdapter()：添加适配器
 * 
 * 使用场景：
 * - 多数据源接入和管理
 * - 行情数据的实时接收和处理
 * - 不同交易所数据的统一管理
 * - 自定义解析器的集成
 * 
 * 技术特点：
 * - 插件化架构，支持动态加载
 * - 灵活的数据过滤机制
 * - 统一的事件处理接口
 * - 完善的错误处理和日志记录
 * 
 * 注意事项：
 * - 解析器模块必须导出createParser和deleteParser函数
 * - 过滤器配置支持多种格式
 * - 数据验证包含日期和合约信息检查
 * - 解析器停止后不再处理新数据
 */
#include "ParserAdapter.h"                                                      // 包含解析器适配器头文件
#include "WtHelper.h"                                                           // 包含辅助工具类（用于获取模块目录）
#include "WtDtRunner.h"                                                         // 包含数据服务运行器头文件

#include "../Share/StrUtil.hpp"                                                 // 包含字符串工具类
#include "../Share/DLLHelper.hpp"                                               // 包含动态库加载工具
#include "../Share/StdUtils.hpp"                                                 // 包含标准工具类
#include "../Share/CodeHelper.hpp"                                               // 包含代码解析工具

#include "../Includes/WTSVariant.hpp"                                            // 包含配置变体类
#include "../Includes/WTSContractInfo.hpp"                                       // 包含合约信息类
#include "../Includes/WTSDataDef.hpp"                                            // 包含数据结构定义
#include "../Includes/WTSVariant.hpp"                                            // 包含配置变体类（重复包含，原代码如此）

#include "../WTSTools/WTSBaseDataMgr.h"                                         // 包含基础数据管理器
#include "../WTSTools/WTSLogger.h"                                               // 包含日志工具类


//////////////////////////////////////////////////////////////////////////
//ParserAdapter类实现
//////////////////////////////////////////////////////////////////////////

/**
 * @brief ParserAdapter构造函数
 * @param bgMgr 基础数据管理器指针
 * @param runner 数据服务运行器指针
 * 
 * 初始化适配器对象，设置所有成员变量为初始值。
 * 解析器API和删除函数指针初始化为NULL，停止标志初始化为false。
 */
ParserAdapter::ParserAdapter(WTSBaseDataMgr * bgMgr, WtDtRunner* runner)
	: _parser_api(NULL)                                                          // 解析器API指针初始化为NULL
	, _remover(NULL)                                                             // 解析器删除函数指针初始化为NULL
	, _stopped(false)                                                            // 停止标志初始化为false
	, _bd_mgr(bgMgr)                                                             // 设置基础数据管理器指针
	, _dt_runner(runner)                                                         // 设置数据服务运行器指针
	, _cfg(NULL)                                                                 // 配置信息指针初始化为NULL
{
}

/**
 * @brief ParserAdapter析构函数
 * 
 * 清理适配器资源。
 * 注意：应在release()之后调用，确保解析器资源已正确释放。
 */
ParserAdapter::~ParserAdapter()
{
}

/**
 * @brief 初始化适配器（外部API）
 * @param id 解析器ID（唯一标识）
 * @param api 解析器API指针（外部创建）
 * @return 是否初始化成功
 * 
 * 使用外部创建的解析器API对象初始化适配器。
 * 适用于解析器已经创建或需要特殊初始化的场景。
 * 该方法会注册回调接口、初始化解析器并订阅所有合约。
 */
bool ParserAdapter::initExt(const char* id, IParserApi* api)
{
	if (api == NULL)                                                             // 如果API指针为空
		return false;                                                            // 返回false表示失败

	_parser_api = api;                                                            // 保存解析器API指针
	_id = id;                                                                     // 保存解析器ID

	if (_parser_api)                                                              // 如果解析器API指针有效
	{
		_parser_api->registerSpi(this);                                          // 向解析器注册回调接口（this指针，即ParserAdapter对象）

		if (_parser_api->init(NULL))                                             // 初始化解析器（传入NULL表示使用默认配置）
		{
			ContractSet contractSet;                                             // 创建合约集合，用于存储订阅的合约代码
			WTSArray* ayContract = _bd_mgr->getContracts();                     // 从基础数据管理器获取所有合约列表
			WTSArray::Iterator it = ayContract->begin();                        // 获取合约列表的迭代器起始位置
			for (; it != ayContract->end(); it++)                                // 遍历所有合约
			{
				WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);  // 将数组元素转换为合约信息指针
				contractSet.insert(contract->getFullCode());                     // 将合约的完整代码（格式：交易所.合约代码）插入到订阅集合中
			}

			ayContract->release();                                               // 释放合约列表数组对象

			_parser_api->subscribe(contractSet);                                // 向解析器订阅所有合约（订阅合约集合）
			contractSet.clear();                                                 // 清空合约集合
		}
		else                                                                     // 如果解析器初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());  // 记录错误日志
		}
	}

	return true;                                                                  // 返回true表示成功（即使初始化失败也返回true，因为API对象已设置）
}


/**
 * @brief 初始化适配器（从配置文件）
 * @param id 解析器ID（唯一标识）
 * @param cfg 配置信息（WTSVariant对象）
 * @return 是否初始化成功
 * 
 * 从配置信息中加载解析器模块，初始化解析器，并设置订阅列表。
 * 配置信息应包含module（模块名）、filter（交易所过滤）、code（合约过滤）等字段。
 * 该方法会动态加载解析器动态库，获取创建和删除函数，并根据过滤器构建订阅列表。
 */
bool ParserAdapter::init(const char* id, WTSVariant* cfg)
{
	if (cfg == NULL)                                                             // 如果配置信息为空
		return false;                                                            // 返回false表示失败

	_id = id;                                                                     // 保存解析器ID

	if (_cfg != NULL)                                                            // 如果配置信息已经存在（防止重复初始化）
		return false;                                                            // 返回false表示失败

	_cfg = cfg;                                                                   // 保存配置信息指针
	_cfg->retain();                                                               // 增加配置信息的引用计数（防止被释放）

	{
		// 加载解析器模块
		if (cfg->getString("module").empty())                                    // 如果配置中没有指定模块名
			return false;                                                        // 返回false表示失败

		std::string module = DLLHelper::wrap_module(cfg->getCString("module"), "lib");  // 包装模块名（添加lib前缀和平台后缀，如"libParserCTP.so"）

		if (!StdFile::exists(module.c_str()))                                    // 如果指定路径的模块文件不存在
		{
			module = WtHelper::get_module_dir();                                 // 获取模块目录路径
			module += "parsers/";                                                // 拼接parsers子目录
			module += DLLHelper::wrap_module(cfg->getCString("module"), "lib");  // 拼接包装后的模块名
		}

		DllHandle hInst = DLLHelper::load_library(module.c_str());               // 动态加载解析器模块动态库
		if (hInst == NULL)                                                       // 如果加载失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser module {} loading failed", _id.c_str(), module.c_str());  // 记录错误日志
			return false;                                                        // 返回false表示失败
		}
		else                                                                     // 如果加载成功
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] Parser module {} loaded", _id.c_str(), module.c_str());  // 记录信息日志
		}

		FuncCreateParser pFuncCreateParser = (FuncCreateParser)DLLHelper::get_symbol(hInst, "createParser");  // 从动态库中获取createParser函数符号
		if (NULL == pFuncCreateParser)                                           // 如果函数符号不存在
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Entrance function createParser not found", _id.c_str());  // 记录致命错误日志
			return false;                                                        // 返回false表示失败
		}

		_parser_api = pFuncCreateParser();                                       // 调用createParser函数创建解析器API对象
		if (NULL == _parser_api)                                                 // 如果创建失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Creating parser api failed", _id.c_str());  // 记录致命错误日志
			return false;                                                        // 返回false表示失败
		}

		_remover = (FuncDeleteParser)DLLHelper::get_symbol(hInst, "deleteParser");  // 从动态库中获取deleteParser函数符号（用于后续释放解析器）
	}


	// 解析交易所过滤器配置
	const std::string& strFilter = cfg->getString("filter");                    // 从配置中获取filter字段（交易所过滤器，逗号分隔）
	if (!strFilter.empty())                                                      // 如果过滤器配置不为空
	{
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");          // 按逗号分割过滤器字符串，得到交易所列表
		auto it = ayFilter.begin();                                              // 获取过滤器列表的迭代器起始位置
		for (; it != ayFilter.end(); it++)                                      // 遍历所有交易所
		{
			_exchg_filter.insert(*it);                                           // 将交易所代码插入到交易所过滤器中
		}
	}

	// 解析合约代码过滤器配置
	std::string strCodes = cfg->getString("code");                               // 从配置中获取code字段（合约代码过滤器，逗号分隔）
	if (!strCodes.empty())                                                       // 如果合约代码过滤器配置不为空
	{
		const StringVector &ayCodes = StrUtil::split(strCodes, ",");            // 按逗号分割合约代码字符串，得到合约代码列表
		auto it = ayCodes.begin();                                               // 获取合约代码列表的迭代器起始位置
		for (; it != ayCodes.end(); it++)                                       // 遍历所有合约代码
		{
			_code_filter.insert(*it);                                            // 将合约代码插入到合约代码过滤器中
		}
	}

	if (_parser_api)                                                              // 如果解析器API对象创建成功
	{
		_parser_api->registerSpi(this);                                          // 向解析器注册回调接口（this指针，即ParserAdapter对象）

		if (_parser_api->init(cfg))                                             // 使用配置信息初始化解析器
		{
			ContractSet contractSet;                                            // 创建合约集合，用于存储订阅的合约代码
			if (!_code_filter.empty())                                          // 如果配置了合约代码过滤器（优先判断合约过滤器）
			{
				ExchgFilter::iterator it = _code_filter.begin();               // 获取合约代码过滤器的迭代器起始位置
				for (; it != _code_filter.end(); it++)                         // 遍历所有合约代码过滤器
				{
					// 全代码格式说明：股票格式如SSE.600000，期货格式如CFFEX.IF2005
					std::string code, exchg;                                     // 合约代码和交易所代码
					auto ay = StrUtil::split((*it).c_str(), ".");               // 按点号分割合约代码字符串
					if (ay.size() == 1)                                         // 如果只有一个部分（只有合约代码，无交易所）
						code = ay[0];                                           // 则整个字符串就是合约代码
					else if (ay.size() == 2)                                    // 如果有两个部分（交易所.合约代码）
					{
						exchg = ay[0];                                          // 第一部分是交易所代码
						code = ay[1];                                           // 第二部分是合约代码
					}
					else if (ay.size() == 3)                                    // 如果有三个部分（交易所.品种.合约代码，如CFFEX.IF.IF2005）
					{
						exchg = ay[0];                                          // 第一部分是交易所代码
						code = ay[2];                                           // 第三部分是合约代码（跳过品种部分）
					}
					WTSContractInfo* contract = _bd_mgr->getContract(code.c_str(), exchg.c_str());  // 根据合约代码和交易所代码获取合约信息
					if (contract)                                               // 如果找到合约信息
						contractSet.insert(contract->getFullCode());            // 将合约的完整代码插入到订阅集合中
					else                                                        // 如果找不到合约信息（可能是品种ID）
					{
						// 如果是品种ID，则将该品种下全部合约都加到订阅列表
						WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(exchg.c_str(), code.c_str());  // 根据交易所和代码获取品种信息
						if (commInfo)                                            // 如果找到品种信息
						{
							const auto& codes = commInfo->getCodes();           // 获取该品种下的所有合约代码列表
							for (const auto& c : codes)                         // 遍历所有合约代码
							{
								contractSet.insert(fmt::format("{}.{}", exchg, c.c_str()));  // 将每个合约的完整代码（交易所.合约代码）插入到订阅集合中
							}
						}
					}
				}
			}
			else if (!_exchg_filter.empty())                                    // 如果没有配置合约代码过滤器，但配置了交易所过滤器
			{
				ExchgFilter::iterator it = _exchg_filter.begin();              // 获取交易所过滤器的迭代器起始位置
				for (; it != _exchg_filter.end(); it++)                       // 遍历所有交易所
				{
					const char* exchg = (*it).c_str();                         // 获取交易所代码字符串
					WTSArray* ayContract = _bd_mgr->getContracts(exchg);      // 从基础数据管理器获取指定交易所的所有合约列表
					auto cnt = ayContract->size();                             // 获取合约数量
					WTSArray::Iterator it = ayContract->begin();                // 获取合约列表的迭代器起始位置
					for (; it != ayContract->end(); it++)                       // 遍历所有合约
					{
						WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);  // 将数组元素转换为合约信息指针
						contractSet.insert(contract->getFullCode());            // 将合约的完整代码插入到订阅集合中
					}

					ayContract->release();                                      // 释放合约列表数组对象

					WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] {} contracts of {} added to sublist...", _id.c_str(), cnt, exchg);  // 记录信息日志，显示添加了多少个合约
				}
			}
			else                                                                // 如果既没有配置合约代码过滤器，也没有配置交易所过滤器
			{
				WTSArray* ayContract = _bd_mgr->getContracts();                 // 从基础数据管理器获取所有交易所的所有合约列表
				WTSArray::Iterator it = ayContract->begin();                    // 获取合约列表的迭代器起始位置
				for (; it != ayContract->end(); it++)                           // 遍历所有合约
				{
					WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);  // 将数组元素转换为合约信息指针
					contractSet.insert(contract->getFullCode());                // 将合约的完整代码插入到订阅集合中
				}

				ayContract->release();                                          // 释放合约列表数组对象
			}

			_parser_api->subscribe(contractSet);                                // 向解析器订阅合约集合中的所有合约
			contractSet.clear();                                                // 清空合约集合
		}
		else                                                                     // 如果解析器初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());  // 记录错误日志
		}
	}
	else                                                                         // 如果解析器API对象创建失败
	{
		WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: creating api failed...", _id.c_str());  // 记录错误日志
	}

	return true;                                                                  // 返回true表示成功（即使初始化失败也返回true，因为配置已保存）
}

/**
 * @brief 释放适配器资源
 * 
 * 停止解析器并释放相关资源。
 * 会调用解析器的release()方法，并根据创建方式决定是否删除解析器对象。
 * 如果解析器是通过动态库创建的（有_remover函数），则使用_remover删除；
 * 否则直接使用delete删除。
 */
void ParserAdapter::release()
{
	_stopped = true;                                                              // 设置停止标志为true，表示解析器已停止（后续不再处理新数据）
	if (_parser_api)                                                              // 如果解析器API对象存在
	{
		_parser_api->release();                                                  // 调用解析器的release()方法，释放解析器资源
	}

	if (_remover)                                                                 // 如果有删除函数指针（说明是通过动态库创建的解析器）
		_remover(_parser_api);                                                   // 使用动态库的删除函数删除解析器对象
	else                                                                         // 如果没有删除函数指针（说明是直接创建的解析器）
		delete _parser_api;                                                       // 直接使用delete删除解析器对象
}

/**
 * @brief 启动解析器
 * @return 是否启动成功
 * 
 * 启动解析器，开始接收行情数据。
 * 实际上是调用解析器的connect()方法建立连接。
 * 如果解析器API不存在，则返回false。
 */
bool ParserAdapter::run()
{
	if (_parser_api == NULL)                                                     // 如果解析器API对象不存在
		return false;                                                            // 返回false表示失败

	_parser_api->connect();                                                      // 调用解析器的connect()方法建立连接
	return true;                                                                  // 返回true表示成功
}

/**
 * @brief 处理合约列表回调
 * @param aySymbols 合约列表数组
 * 
 * 当解析器返回合约列表时调用。
 * 当前实现为空，可扩展用于处理合约信息。
 * 预留接口，可用于未来扩展功能。
 */
void ParserAdapter::handleSymbolList( const WTSArray* aySymbols )
{
	// 当前实现为空，预留接口用于未来扩展
	// 可以在此处处理解析器返回的合约列表，例如更新合约信息、验证合约有效性等
}

/**
 * @brief 处理逐笔成交数据回调
 * @param transData 逐笔成交数据指针
 * 
 * 当解析器接收到逐笔成交数据时调用。
 * 当前实现为空，可扩展用于处理Level-2数据。
 * 会验证数据有效性（交易日期和动作日期），并检查合约是否存在。
 */
void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	if (_stopped)                                                                // 如果解析器已停止
		return;                                                                  // 直接返回，不处理数据


	if (transData->actiondate() == 0 || transData->tradingdate() == 0)          // 如果交易日期或动作日期为0（无效数据）
		return;                                                                  // 直接返回，不处理无效数据

	WTSContractInfo* contract = _bd_mgr->getContract(transData->code(), transData->exchg());  // 根据合约代码和交易所代码获取合约信息
	if (contract == NULL)                                                        // 如果找不到合约信息
		return;                                                                  // 直接返回，不处理未知合约的数据

	// 当前实现为空，预留接口用于未来扩展
	// 可以在此处处理逐笔成交数据，例如：
	// - 存储到数据库
	// - 转发给其他模块
	// - 进行数据统计和分析
}

/**
 * @brief 处理逐笔委托数据回调
 * @param ordDetailData 逐笔委托数据指针
 * 
 * 当解析器接收到逐笔委托数据时调用。
 * 当前实现为空，可扩展用于处理Level-2数据。
 * 会验证数据有效性（交易日期和动作日期），并检查合约是否存在。
 */
void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDetailData)
{
	if (_stopped)                                                                // 如果解析器已停止
		return;                                                                  // 直接返回，不处理数据

	if (ordDetailData->actiondate() == 0 || ordDetailData->tradingdate() == 0)  // 如果交易日期或动作日期为0（无效数据）
		return;                                                                  // 直接返回，不处理无效数据

	WTSContractInfo* contract = _bd_mgr->getContract(ordDetailData->code(), ordDetailData->exchg());  // 根据合约代码和交易所代码获取合约信息
	if (contract == NULL)                                                        // 如果找不到合约信息
		return;                                                                  // 直接返回，不处理未知合约的数据

	// 当前实现为空，预留接口用于未来扩展
	// 可以在此处处理逐笔委托数据，例如：
	// - 存储到数据库
	// - 转发给其他模块
	// - 进行数据统计和分析
}

/**
 * @brief 处理委托队列数据回调
 * @param ordQueData 委托队列数据指针
 * 
 * 当解析器接收到委托队列数据时调用。
 * 当前实现为空，可扩展用于处理Level-2数据。
 * 会验证数据有效性（交易日期和动作日期），并检查合约是否存在。
 */
void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	if (_stopped)                                                                // 如果解析器已停止
		return;                                                                  // 直接返回，不处理数据

	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)        // 如果交易日期或动作日期为0（无效数据）
		return;                                                                  // 直接返回，不处理无效数据

	WTSContractInfo* contract = _bd_mgr->getContract(ordQueData->code(), ordQueData->exchg());  // 根据合约代码和交易所代码获取合约信息
	if (contract == NULL)                                                        // 如果找不到合约信息
		return;                                                                  // 直接返回，不处理未知合约的数据
		
	// 当前实现为空，预留接口用于未来扩展
	// 可以在此处处理委托队列数据，例如：
	// - 存储到数据库
	// - 转发给其他模块
	// - 进行数据统计和分析
}

/**
 * @brief 处理行情数据回调
 * @param quote Tick行情数据指针
 * @param procFlag 处理标志位
 * 
 * 当解析器接收到新的Tick数据时调用。
 * 会将数据转发给数据服务运行器进行处理。
 * 支持数据过滤，只处理订阅的合约数据。
 * 会验证数据有效性（交易日期和动作日期），并检查解析器是否已停止。
 */
void ParserAdapter::handleQuote( WTSTickData *quote, uint32_t procFlag )
{
	if (quote == NULL || _stopped || quote->actiondate() == 0 || quote->tradingdate() == 0)  // 如果行情数据为空、解析器已停止、或交易日期/动作日期无效
		return;                                                                  // 直接返回，不处理数据

	if (_dt_runner)                                                              // 如果数据服务运行器存在
		_dt_runner->proc_tick(quote);                                            // 将Tick数据转发给数据服务运行器处理（proc_tick会更新实时K线并触发回调）
}

/**
 * @brief 处理解析器日志回调
 * @param ll 日志级别
 * @param message 日志消息
 * 
 * 当解析器输出日志时调用。
 * 将日志转发到WonderTrader的日志系统。
 * 如果解析器已停止，则不处理日志。
 */
void ParserAdapter::handleParserLog( WTSLogLevel ll, const char* message)
{
	if (_stopped)                                                                // 如果解析器已停止
		return;                                                                  // 直接返回，不处理日志

	WTSLogger::log_raw_by_cat("parser", ll, message);                           // 将日志转发到WonderTrader的日志系统（分类为"parser"）
}

/**
 * @brief 获取基础数据管理器
 * @return 基础数据管理器指针
 * 
 * 返回适配器使用的基础数据管理器。
 * 解析器可以通过此接口获取合约信息等基础数据。
 */
IBaseDataMgr* ParserAdapter::getBaseDataMgr()
{
	return _bd_mgr;                                                              // 返回基础数据管理器指针
}


//////////////////////////////////////////////////////////////////////////
//ParserAdapterMgr类实现
//////////////////////////////////////////////////////////////////////////

/**
 * @brief 释放所有适配器资源
 * 
 * 遍历所有适配器，调用它们的release()方法释放资源。
 * 然后清空适配器映射表。
 * 用于程序退出时清理所有解析器资源。
 */
void ParserAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)              // 遍历适配器映射表中的所有适配器
	{
		it->second->release();                                                  // 调用每个适配器的release()方法释放资源
	}

	_adapters.clear();                                                          // 清空适配器映射表
}

/**
 * @brief 添加适配器
 * @param id 解析器ID（唯一标识）
 * @param adapter 适配器智能指针
 * @return 是否添加成功
 * 
 * 将适配器添加到管理器中。
 * 如果ID已存在，添加失败并返回false。
 * 会检查适配器指针和ID的有效性。
 */
bool ParserAdapterMgr::addAdapter(const char* id, ParserAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(id) == 0)                                     // 如果适配器指针为空或ID为空字符串
		return false;                                                           // 返回false表示失败

	auto it = _adapters.find(id);                                               // 在适配器映射表中查找指定ID的适配器
	if (it != _adapters.end())                                                  // 如果找到了（ID已存在）
	{
		WTSLogger::error(" Same name of parsers: %s", id);                     // 记录错误日志（解析器名称重复）
		return false;                                                           // 返回false表示失败（不允许重复的ID）
	}

	_adapters[id] = adapter;                                                    // 将适配器添加到映射表中（键=ID，值=适配器指针）

	return true;                                                                // 返回true表示成功
}

/**
 * @brief 获取指定ID的适配器
 * @param id 解析器ID
 * @return 适配器智能指针（如果不存在则返回空指针）
 * 
 * 根据ID查找并返回对应的适配器。
 * 用于访问已注册的解析器。
 * 如果ID不存在，返回空的智能指针。
 */
ParserAdapterPtr ParserAdapterMgr::getAdapter(const char* id)
{
	auto it = _adapters.find(id);                                               // 在适配器映射表中查找指定ID的适配器
	if (it != _adapters.end())                                                  // 如果找到了
	{
		return it->second;                                                      // 返回对应的适配器智能指针
	}

	return ParserAdapterPtr();                                                  // 如果没找到，返回空的智能指针
}

/**
 * @brief 启动所有适配器
 * 
 * 遍历所有适配器，调用它们的run()方法启动解析器。
 * 用于批量启动所有配置的解析器。
 * 启动完成后会记录日志，显示启动的解析器数量。
 */
void ParserAdapterMgr::run()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)              // 遍历适配器映射表中的所有适配器
	{
		it->second->run();                                                      // 调用每个适配器的run()方法启动解析器
	}

	WTSLogger::info("{} parsers started", _adapters.size());                   // 记录信息日志，显示启动了多少个解析器
}