/*!
 * \file ParserAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 解析器适配器实现文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件实现了解析器适配器类ParserAdapter和解析器适配器管理器类ParserAdapterMgr的所有功能。
 * 
 * 主要实现功能：
 * 1. 解析器生命周期管理：初始化、运行、释放解析器。
 * 2. 动态库加载：从指定目录加载解析器模块（DLL/SO）。
 * 3. 合约订阅：根据过滤器和配置订阅合约行情。
 * 4. 行情数据处理：接收并处理Tick、委托队列、委托明细、逐笔成交数据。
 * 5. 数据过滤：应用交易所过滤器和合约代码过滤器。
 * 6. 时间戳校验：可选的时间戳校验，过滤错误时间戳的数据。
 * 7. 代码转换：将原始合约代码转换为标准合约代码。
 * 8. 日志转发：将解析器内部的日志转发给统一日志系统。
 * 9. 适配器管理：管理多个解析器适配器实例。
 */
#include "ParserAdapter.h"  // 包含解析器适配器头文件
#include "WtEngine.h"  // 包含WonderTrader引擎头文件
#include "WtCtaTicker.h"  // 包含CTA行情接收器头文件
#include "WtHelper.h"  // 包含WonderTrader辅助工具类

#include "../Share/CodeHelper.hpp"  // 包含代码辅助工具类
#include "../Share/TimeUtils.hpp"  // 包含时间工具类

#include "../Includes/WTSContractInfo.hpp"  // 包含合约信息头文件
#include "../Includes/WTSDataDef.hpp"  // 包含数据定义头文件
#include "../Includes/WTSVariant.hpp"  // 包含配置变体头文件
#include "../Includes/IBaseDataMgr.h"  // 包含基础数据管理器接口头文件
#include "../Includes/IHotMgr.h"  // 包含热点合约管理器接口头文件

#include "../WTSTools/WTSLogger.h"  // 包含日志记录工具

USING_NS_WTP;  // 使用WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
//ParserAdapter
/**
 * @brief 构造函数实现
 *
 * 初始化解析器适配器对象，将所有指针成员初始化为NULL，布尔成员初始化为false。
 */
ParserAdapter::ParserAdapter()
	: _parser_api(NULL)  // 初始化解析器API指针为NULL
	, _remover(NULL)  // 初始化删除函数指针为NULL
	, _stopped(false)  // 初始化停止标志为false
	, _bd_mgr(NULL)  // 初始化基础数据管理器指针为NULL
	, _stub(NULL)  // 初始化存根指针为NULL
	, _cfg(NULL)  // 初始化配置指针为NULL
{
}


/**
 * @brief 析构函数实现
 *
 * 清理解析器适配器对象。
 * 注意：资源的释放应在release方法中完成。
 */
ParserAdapter::~ParserAdapter()
{
}

/**
 * @brief 扩展初始化解析器适配器实现
 * @param id 解析器ID
 * @param api 解析器API指针（已创建好的）
 * @param stub 解析器存根指针，用于接收行情数据
 * @param bgMgr 基础数据管理器指针
 * @param hotMgr 热点合约管理器指针，可选参数，默认NULL
 * @return bool 初始化成功返回true，失败返回false
 *
 * 使用已创建的解析器API实例初始化适配器。
 * 处理流程：
 * 1. 检查API指针是否有效。
 * 2. 保存参数。
 * 3. 注册SPI回调接口。
 * 4. 初始化解析器。
 * 5. 订阅所有合约。
 */
bool ParserAdapter::initExt(const char* id, IParserApi* api, IParserStub* stub, IBaseDataMgr* bgMgr, IHotMgr* hotMgr/* = NULL*/)
{
	if (api == NULL)  // 如果API指针为空
		return false;  // 返回false

	_parser_api = api;  // 保存解析器API指针
	_stub = stub;  // 保存存根指针
	_bd_mgr = bgMgr;  // 保存基础数据管理器指针
	_hot_mgr = hotMgr;  // 保存热点合约管理器指针
	_id = id;  // 保存解析器ID

	if (_parser_api)  // 如果解析器API存在
	{
		_parser_api->registerSpi(this);  // 注册SPI回调接口（将当前对象注册为回调接收者）

		if (_parser_api->init(NULL))  // 如果解析器初始化成功（传入NULL配置，使用默认配置）
		{
			ContractSet contractSet;  // 创建合约集合
			WTSArray* ayContract = _bd_mgr->getContracts();  // 获取所有合约列表
			WTSArray::Iterator it = ayContract->begin();  // 获取合约列表的起始迭代器
			for (; it != ayContract->end(); it++)  // 遍历所有合约
			{
				WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);  // 将迭代器元素转换为合约信息指针
				contractSet.insert(contract->getFullCode());  // 将合约全代码添加到合约集合中
			}

			ayContract->release();  // 释放合约列表数组

			_parser_api->subscribe(contractSet);  // 订阅所有合约
			contractSet.clear();  // 清空合约集合
		}
		else  // 如果解析器初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());  // 记录错误日志：解析器初始化失败
		}
	}

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 初始化解析器适配器实现
 * @param id 解析器ID
 * @param cfg 配置参数
 * @param stub 解析器存根指针，用于接收行情数据
 * @param bgMgr 基础数据管理器指针
 * @param hotMgr 热点合约管理器指针，可选参数，默认NULL
 * @return bool 初始化成功返回true，失败返回false
 *
 * 根据配置参数初始化解析器适配器，包括：
 * 1. 加载解析器模块（动态库）。
 * 2. 创建解析器API实例。
 * 3. 解析过滤器配置（交易所过滤器和合约代码过滤器）。
 * 4. 注册SPI回调接口。
 * 5. 初始化解析器。
 * 6. 根据过滤器配置订阅合约。
 */
bool ParserAdapter::init(const char* id, WTSVariant* cfg, IParserStub* stub, IBaseDataMgr* bgMgr, IHotMgr* hotMgr/* = NULL*/)
{
	if (cfg == NULL)  // 如果配置参数为空
		return false;  // 返回false

	_stub = stub;  // 保存存根指针
	_bd_mgr = bgMgr;  // 保存基础数据管理器指针
	_hot_mgr = hotMgr;  // 保存热点合约管理器指针
	_id = id;  // 保存解析器ID

	if (_cfg != NULL)  // 如果配置已经存在（表示已经初始化过）
		return false;  // 返回false，避免重复初始化

	_cfg = cfg;  // 保存配置指针
	_cfg->retain();  // 增加配置对象的引用计数

	_check_time = cfg->getBoolean("check_time");  // 从配置中获取是否检查时间戳标志

	{
		//加载模块
		if (cfg->getString("module").empty())  // 如果配置中模块名称为空
			return false;  // 返回false

		std::string module = DLLHelper::wrap_module(cfg->getCString("module"), "lib");  // 封装模块名称（根据平台添加前缀和后缀）

		//先看工作目录下是否有交易模块
		std::string dllpath = WtHelper::getModulePath(module.c_str(), "parsers", true);  // 获取工作目录下的模块路径
		//如果没有,则再看模块目录,即dll同目录下
		if (!StdFile::exists(dllpath.c_str()))  // 如果工作目录下不存在该模块
			dllpath = WtHelper::getModulePath(module.c_str(), "parsers", false);  // 获取安装目录下的模块路径

		DllHandle hInst = DLLHelper::load_library(dllpath.c_str());  // 加载解析器模块动态库
		if (hInst == NULL)  // 如果加载失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser module {} loading failed", _id.c_str(), dllpath.c_str());  // 记录错误日志：模块加载失败
			return false;  // 返回false
		}
		else  // 如果加载成功
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] Parser module {} loaded", _id.c_str(), dllpath.c_str());  // 记录信息日志：模块加载成功
		}

		FuncCreateParser pFuncCreateParser = (FuncCreateParser)DLLHelper::get_symbol(hInst, "createParser");  // 获取创建解析器的函数指针
		if (NULL == pFuncCreateParser)  // 如果函数指针为空
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Entrance function createParser not found", _id.c_str());  // 记录致命错误日志：入口函数未找到
			return false;  // 返回false
		}

		_parser_api = pFuncCreateParser();  // 调用创建函数，创建解析器API实例
		if (NULL == _parser_api)  // 如果解析器API实例创建失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Creating parser api failed", _id.c_str());  // 记录致命错误日志：创建解析器API失败
			return false;  // 返回false
		}

		_remover = (FuncDeleteParser)DLLHelper::get_symbol(hInst, "deleteParser");  // 获取删除解析器的函数指针
	}
	

	const std::string& strFilter = cfg->getString("filter");  // 从配置中获取交易所过滤器字符串
	if (!strFilter.empty())  // 如果过滤器字符串不为空
	{
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");  // 分割过滤器字符串，获取交易所列表
		auto it = ayFilter.begin();  // 获取过滤器列表的起始迭代器
		for (; it != ayFilter.end(); it++)  // 遍历所有交易所
		{
			_exchg_filter.insert(*it);  // 将交易所添加到交易所过滤器集合中
		}
	}

	std::string strCodes = cfg->getString("code");  // 从配置中获取合约代码过滤器字符串
	if (!strCodes.empty())  // 如果合约代码过滤器字符串不为空
	{
		const StringVector &ayCodes = StrUtil::split(strCodes, ",");  // 分割合约代码字符串，获取合约代码列表
		auto it = ayCodes.begin();  // 获取合约代码列表的起始迭代器
		for (; it != ayCodes.end(); it++)  // 遍历所有合约代码
		{
			_code_filter.insert(*it);  // 将合约代码添加到合约代码过滤器集合中
		}
	}

	if (_parser_api)  // 如果解析器API存在
	{
		_parser_api->registerSpi(this);  // 注册SPI回调接口（将当前对象注册为回调接收者）

		if (_parser_api->init(cfg))  // 如果解析器初始化成功
		{
			ContractSet contractSet;  // 创建合约集合
			if (!_code_filter.empty())  // 如果合约代码过滤器不为空，优先判断合约过滤器
			{
				ExchgFilter::iterator it = _code_filter.begin();  // 获取合约代码过滤器的起始迭代器
				for (; it != _code_filter.end(); it++)  // 遍历所有合约代码过滤器
				{
					//全代码,形式如SSE.600000,期货代码为CFFEX.IF2005
					std::string code, exchg;  // 定义合约代码和交易所变量
					auto ay = StrUtil::split((*it).c_str(), ".");  // 分割合约代码字符串（格式：交易所.合约代码或交易所.品种.合约代码）
					if (ay.size() == 1)  // 如果只有一个部分（只有合约代码）
						code = ay[0];  // 设置为合约代码
					else if (ay.size() == 2)  // 如果有两个部分（交易所.合约代码）
					{
						exchg = ay[0];  // 第一部分为交易所
						code = ay[1];  // 第二部分为合约代码
					}
					else if (ay.size() == 3)  // 如果有三个部分（交易所.品种.合约代码）
					{
						exchg = ay[0];  // 第一部分为交易所
						code = ay[2];  // 第三部分为合约代码（跳过品种部分）
					}
					WTSContractInfo* contract = _bd_mgr->getContract(code.c_str(), exchg.c_str());  // 根据合约代码和交易所获取合约信息
					if(contract)  // 如果合约信息存在
						contractSet.insert(contract->getFullCode());  // 将合约全代码添加到合约集合中
					else  // 如果合约信息不存在
					{
						//如果是品种ID，则将该品种下全部合约都加到订阅列表
						WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(exchg.c_str(), code.c_str());  // 根据交易所和代码获取商品信息（可能是品种）
						if(commInfo)  // 如果商品信息存在（说明是品种）
						{
							const auto& codes = commInfo->getCodes();  // 获取该品种下的所有合约代码
							for(const auto& c : codes)  // 遍历所有合约代码
							{
								contractSet.insert(fmt::format("{}.{}", exchg, c.c_str()));  // 将合约全代码添加到合约集合中
							}							
						}
					}
				}
			}
			else if (!_exchg_filter.empty())  // 如果交易所过滤器不为空（且合约代码过滤器为空）
			{
				ExchgFilter::iterator it = _exchg_filter.begin();  // 获取交易所过滤器的起始迭代器
				for (; it != _exchg_filter.end(); it++)  // 遍历所有交易所
				{
					WTSArray* ayContract =_bd_mgr->getContracts((*it).c_str());  // 获取指定交易所的所有合约列表
					WTSArray::Iterator it = ayContract->begin();  // 获取合约列表的起始迭代器
					for (; it != ayContract->end(); it++)  // 遍历所有合约
					{
						WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);  // 将迭代器元素转换为合约信息指针
						contractSet.insert(contract->getFullCode());  // 将合约全代码添加到合约集合中
					}

					ayContract->release();  // 释放合约列表数组
				}
			}
			else  // 如果过滤器和合约代码过滤器都为空
			{
				WTSArray* ayContract =_bd_mgr->getContracts();  // 获取所有合约列表
				WTSArray::Iterator it = ayContract->begin();  // 获取合约列表的起始迭代器
				for (; it != ayContract->end(); it++)  // 遍历所有合约
				{
					WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);  // 将迭代器元素转换为合约信息指针
					contractSet.insert(contract->getFullCode());  // 将合约全代码添加到合约集合中
				}

				ayContract->release();  // 释放合约列表数组
			}

			_parser_api->subscribe(contractSet);  // 订阅合约集合中的所有合约
			contractSet.clear();  // 清空合约集合
		}
		else  // 如果解析器初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());  // 记录错误日志：解析器初始化失败
		}
	}
	else  // 如果解析器API不存在
	{
		WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: creating api failed...", _id.c_str());  // 记录错误日志：创建解析器API失败
	}

	WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] Parser initialzied, check_time: {}", _id.c_str(), _check_time);  // 记录信息日志：解析器初始化完成，输出时间检查标志

	return true;  // 返回true，表示初始化成功
}

/**
 * @brief 释放解析器适配器实现
 *
 * 释放解析器资源，包括：
 * 1. 设置停止标志。
 * 2. 调用解析器的release方法。
 * 3. 删除解析器API实例（通过删除函数或delete操作符）。
 */
void ParserAdapter::release()
{
	_stopped = true;  // 设置停止标志为true
	if (_parser_api)  // 如果解析器API存在
	{
		_parser_api->release();  // 调用解析器的release方法，释放资源
	}

	if (_remover)  // 如果删除函数指针存在
		_remover(_parser_api);  // 通过删除函数删除解析器API实例
	else  // 如果删除函数指针不存在
		delete _parser_api;  // 直接使用delete操作符删除解析器API实例
}

/**
 * @brief 运行解析器适配器实现
 * @return bool 运行成功返回true，失败返回false
 *
 * 连接解析器，开始接收行情数据。
 */
bool ParserAdapter::run()
{
	if (_parser_api == NULL)  // 如果解析器API不存在
		return false;  // 返回false

	_parser_api->connect();  // 连接解析器，开始接收行情数据
	return true;  // 返回true，表示运行成功
}

//合理毫秒数时间差
const int RESONABLE_MILLISECS = 60 * 60 * 1000;  // 定义合理的时间差常量：1小时（毫秒），用于时间戳校验
/**
 * @brief 处理实时行情实现
 * @param quote 实时行情数据指针
 * @param procFlag 处理标志
 *
 * 当解析器收到实时行情时被调用。
 * 处理流程：
 * 1. 检查行情数据是否有效（非空、未停止、有日期和时间）。
 * 2. 应用交易所过滤器。
 * 3. 获取或设置合约信息。
 * 4. 可选的时间戳校验（如果启用）。
 * 5. 将原始合约代码转换为标准合约代码。
 * 6. 通过存根接口推送给策略引擎。
 */
void ParserAdapter::handleQuote(WTSTickData *quote, uint32_t procFlag)
{
	if (quote == NULL || _stopped || quote->actiondate() == 0 || quote->tradingdate() == 0)  // 如果行情数据为空、已停止、或日期/时间为0
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(quote->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且该交易所不在过滤器中
		return;  // 直接返回，不处理该交易所的数据

	WTSContractInfo* cInfo = quote->getContractInfo();  // 获取行情数据中的合约信息
	if (cInfo == NULL)  // 如果合约信息为空
	{
		cInfo = _bd_mgr->getContract(quote->code(), quote->exchg());  // 从基础数据管理器获取合约信息
		quote->setContractInfo(cInfo);  // 将合约信息设置到行情数据中，避免重复查询
	}

	if (cInfo == NULL)  // 如果合约信息仍为空（说明合约不存在）
		return;  // 直接返回

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取商品信息
	WTSSessionInfo* sInfo = commInfo->getSessionInfo();  // 获取交易会话信息

	if (_check_time)  // 如果启用了时间戳检查
	{
		int64_t tick_time = TimeUtils::makeTime(quote->actiondate(), quote->actiontime());  // 构造行情时间戳（毫秒级）
		int64_t local_time = TimeUtils::getLocalTimeNow();  // 获取本地当前时间戳（毫秒级）

		/*
		 *	By Wesley @ 2022.04.20
		 *	如果最新的tick时间，和本地时间相差太大
		 *	则认为tick的时间戳是错误的
		 *	这里要求本地时间是要时常进行校准的
		 */
		if (tick_time - local_time > RESONABLE_MILLISECS)  // 如果行情时间戳比本地时间大超过1小时（认为时间戳错误）
		{
			WTSLogger::warn("Tick of {} with wrong timestamp {}.{} received, skipped", cInfo->getFullCode(), quote->actiondate(), quote->actiontime());  // 记录警告日志：行情时间戳错误，跳过
			return;  // 直接返回，不处理该行情数据
		}
	}

	std::string stdCode;  // 定义标准合约代码变量
	if (commInfo->getCategoty() == CC_FutOption || commInfo->getCategoty() == CC_SpotOption)  // 如果是期货期权或现货期权
	{
		stdCode = CodeHelper::rawFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());  // 将原始期权代码转换为标准代码
	}
	else if(CodeHelper::isMonthlyCode(quote->code()))  // 如果是分月合约代码
	{
		//如果是分月合约，则进行主力和次主力的判断
		stdCode = CodeHelper::rawMonthCodeToStdCode(cInfo->getCode(), cInfo->getExchg());  // 将原始分月合约代码转换为标准代码
	}
	else  // 如果是其他类型的合约
	{
		stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), cInfo->getProduct());  // 将原始平铺代码转换为标准代码
	}
	quote->setCode(stdCode.c_str());  // 将标准合约代码设置到行情数据中

	_stub->handle_push_quote(quote);  // 通过存根接口推送行情数据给策略引擎
}

/**
 * @brief 处理委托队列数据实现
 * @param ordQueData 委托队列数据指针
 *
 * 当解析器收到委托队列数据时被调用。
 * 处理流程：
 * 1. 检查数据是否有效（未停止、有日期和时间）。
 * 2. 应用交易所过滤器。
 * 3. 获取合约信息。
 * 4. 将原始合约代码转换为标准合约代码。
 * 5. 通过存根接口推送给策略引擎。
 */
void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(ordQueData->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且该交易所不在过滤器中
		return;  // 直接返回，不处理该交易所的数据

	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)  // 如果日期或交易日期为0
		return;  // 直接返回

	WTSContractInfo* cInfo = _bd_mgr->getContract(ordQueData->code(), ordQueData->exchg());  // 从基础数据管理器获取合约信息
	if (cInfo == NULL)  // 如果合约信息不存在
		return;  // 直接返回

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取商品信息
	std::string stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), commInfo->getProduct());  // 将原始平铺代码转换为标准代码
	ordQueData->setCode(stdCode.c_str());  // 将标准合约代码设置到委托队列数据中

	if (_stub)  // 如果存根指针存在
		_stub->handle_push_order_queue(ordQueData);  // 通过存根接口推送委托队列数据给策略引擎
}

/**
 * @brief 处理逐笔委托数据实现
 * @param ordDtlData 逐笔委托数据指针
 *
 * 当解析器收到逐笔委托数据时被调用。
 * 处理流程：
 * 1. 检查数据是否有效（未停止、有日期和时间）。
 * 2. 应用交易所过滤器。
 * 3. 获取合约信息。
 * 4. 将原始合约代码转换为标准合约代码。
 * 5. 通过存根接口推送给策略引擎。
 */
void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDtlData)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(ordDtlData->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且该交易所不在过滤器中
		return;  // 直接返回，不处理该交易所的数据

	if (ordDtlData->actiondate() == 0 || ordDtlData->tradingdate() == 0)  // 如果日期或交易日期为0
		return;  // 直接返回

	WTSContractInfo* cInfo = _bd_mgr->getContract(ordDtlData->code(), ordDtlData->exchg());  // 从基础数据管理器获取合约信息
	if (cInfo == NULL)  // 如果合约信息不存在
		return;  // 直接返回

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取商品信息
	std::string stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), commInfo->getProduct());  // 将原始平铺代码转换为标准代码
	ordDtlData->setCode(stdCode.c_str());  // 将标准合约代码设置到逐笔委托数据中

	if (_stub)  // 如果存根指针存在
		_stub->handle_push_order_detail(ordDtlData);  // 通过存根接口推送逐笔委托数据给策略引擎
}

/**
 * @brief 处理逐笔成交数据实现
 * @param transData 逐笔成交数据指针
 *
 * 当解析器收到逐笔成交数据时被调用。
 * 处理流程：
 * 1. 检查数据是否有效（未停止、有日期和时间）。
 * 2. 应用交易所过滤器。
 * 3. 获取合约信息。
 * 4. 将原始合约代码转换为标准合约代码。
 * 5. 通过存根接口推送给策略引擎。
 */
void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(transData->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且该交易所不在过滤器中
		return;  // 直接返回，不处理该交易所的数据

	if (transData->actiondate() == 0 || transData->tradingdate() == 0)  // 如果日期或交易日期为0
		return;  // 直接返回

	WTSContractInfo* cInfo = _bd_mgr->getContract(transData->code(), transData->exchg());  // 从基础数据管理器获取合约信息
	if (cInfo == NULL)  // 如果合约信息不存在
		return;  // 直接返回

	WTSCommodityInfo* commInfo = cInfo->getCommInfo();  // 获取商品信息
	std::string stdCode = CodeHelper::rawFlatCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), commInfo->getProduct());  // 将原始平铺代码转换为标准代码
	transData->setCode(stdCode.c_str());  // 将标准合约代码设置到逐笔成交数据中

	if (_stub)  // 如果存根指针存在
		_stub->handle_push_transaction(transData);  // 通过存根接口推送逐笔成交数据给策略引擎
}


/**
 * @brief 处理解析器日志实现
 * @param ll 日志级别
 * @param message 日志消息内容
 *
 * 当解析器产生日志时被调用，将日志转发给统一的日志系统。
 */
void ParserAdapter::handleParserLog(WTSLogLevel ll, const char* message)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	WTSLogger::log_dyn_raw("parser", _id.c_str(), ll, message);  // 调用日志记录器记录解析器日志，使用动态日志通道
}


//////////////////////////////////////////////////////////////////////////
//ParserAdapterMgr
/**
 * @brief 释放所有解析器适配器实现
 *
 * 遍历所有已添加的解析器适配器，调用它们的release方法释放资源，
 * 然后清空适配器映射表。
 */
void ParserAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有解析器适配器
	{
		it->second->release();  // 调用适配器的release方法，释放资源
	}

	_adapters.clear();  // 清空适配器映射表
}

/**
 * @brief 添加解析器适配器实现
 * @param id 解析器ID
 * @param adapter 解析器适配器共享指针
 * @return bool 添加成功返回true，失败返回false
 *
 * 将解析器适配器添加到管理器中。
 * 如果ID已存在，则添加失败并记录错误日志。
 */
bool ParserAdapterMgr::addAdapter(const char* id, ParserAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(id) == 0)  // 如果适配器指针为空或ID为空
		return false;  // 返回false

	auto it = _adapters.find(id);  // 在适配器映射表中查找该ID
	if (it != _adapters.end())  // 如果ID已存在
	{
		WTSLogger::error(" Same name of parsers: {}", id);  // 记录错误日志：解析器名称重复
		return false;  // 返回false
	}

	_adapters[id] = adapter;  // 将适配器添加到映射表中

	return true;  // 返回true，表示添加成功
}


/**
 * @brief 获取解析器适配器实现
 * @param id 解析器ID
 * @return ParserAdapterPtr 返回解析器适配器的共享指针，如果未找到则返回空指针
 *
 * 根据解析器ID从适配器映射表中获取对应的解析器适配器实例。
 */
ParserAdapterPtr ParserAdapterMgr::getAdapter(const char* id)
{
	auto it = _adapters.find(id);  // 在适配器映射表中查找该ID
	if (it != _adapters.end())  // 如果找到
	{
		return it->second;  // 返回适配器指针
	}

	return ParserAdapterPtr();  // 如果未找到，返回空指针
}

/**
 * @brief 运行所有解析器适配器实现
 *
 * 遍历所有已添加的解析器适配器，调用它们的run方法启动解析器，
 * 然后记录启动的解析器数量。
 */
void ParserAdapterMgr::run()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有解析器适配器
	{
		it->second->run();  // 调用适配器的run方法，启动解析器
	}

	WTSLogger::info("{} parsers started", _adapters.size());  // 记录信息日志：启动的解析器数量
}