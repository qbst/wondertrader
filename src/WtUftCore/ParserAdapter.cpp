/*!
 * \file ParserAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 行情解析器适配器实现文件
 *
 * 本文件实现了ParserAdapter类和ParserAdapterMgr类的所有功能，包括：
 * 1. 构造函数和析构函数：初始化适配器，清理资源
 * 2. 动态加载：动态加载行情解析器模块
 * 3. 初始化：从配置文件或外部API初始化适配器
 * 4. 数据过滤：按交易所和合约代码过滤行情数据
 * 5. 数据订阅：订阅符合条件的合约行情数据
 * 6. 数据回调：处理解析器推送的各种行情数据
 * 7. 适配器管理：管理多个适配器的生命周期
 */
#include "ParserAdapter.h"  // 行情解析器适配器头文件
#include "WtUftEngine.h"  // UFT引擎头文件
#include "WtHelper.h"  // WonderTrader辅助工具

#include "../Includes/WTSContractInfo.hpp"  // 合约信息类
#include "../Includes/WTSDataDef.hpp"  // 数据定义
#include "../Includes/WTSVariant.hpp"  // 变体配置类
#include "../Includes/IBaseDataMgr.h"  // 基础数据管理器接口

#include "../Share/StrUtil.hpp"  // 字符串工具函数

#include "../WTSTools/WTSLogger.h"  // 日志工具

USING_NS_WTP;  // 使用WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
//ParserAdapter
/**
 * @brief 构造函数
 * 
 * 创建行情解析器适配器实例，初始化所有成员变量为NULL或false。
 */
ParserAdapter::ParserAdapter()
	: _parser_api(NULL)  // 解析器API指针初始化为NULL
	, _remover(NULL)  // 删除解析器函数指针初始化为NULL
	, _stopped(false)  // 停止标志初始化为false
	, _bd_mgr(NULL)  // 基础数据管理器指针初始化为NULL
	, _stub(NULL)  // 存根接口指针初始化为NULL
	, _cfg(NULL)  // 配置参数指针初始化为NULL
{
}


/**
 * @brief 析构函数
 * 
 * 清理行情解析器适配器占用的资源。
 * 注意：实际释放操作在release()方法中完成。
 */
ParserAdapter::~ParserAdapter()
{
}

/**
 * @brief 初始化行情解析器适配器（从配置文件）
 * @param id 适配器ID
 * @param cfg 配置参数，包含解析器模块路径和订阅配置
 * @param stub 行情数据存根接口指针，用于接收行情数据
 * @param bgMgr 基础数据管理器指针，用于获取合约信息
 * @return 初始化成功返回true，失败返回false
 * 
 * 从配置文件加载行情解析器模块，初始化解析器，订阅行情数据。
 * 流程：
 * 1. 验证配置参数
 * 2. 动态加载解析器模块
 * 3. 创建解析器API实例
 * 4. 解析过滤器和订阅列表
 * 5. 注册回调接口
 * 6. 初始化解析器并订阅合约
 */
bool ParserAdapter::init(const char* id, WTSVariant* cfg, IParserStub* stub, IBaseDataMgr* bgMgr)
{
	if (cfg == NULL)  // 如果配置参数为空
		return false;  // 返回false

	_stub = stub;  // 保存存根接口指针
	_bd_mgr = bgMgr;  // 保存基础数据管理器指针
	_id = id;  // 保存适配器ID

	if (_cfg != NULL)  // 如果已经初始化过
		return false;  // 返回false，避免重复初始化

	_cfg = cfg;  // 保存配置参数指针
	_cfg->retain();  // 增加配置参数引用计数

	{
		//加载模块
		if (cfg->getString("module").empty())  // 如果模块名称为空
			return false;  // 返回false

		std::string module = DLLHelper::wrap_module(cfg->getCString("module"), "lib");;  // 包装模块名称

		//先看工作目录下是否有交易模块
		std::string dllpath = WtHelper::getModulePath(module.c_str(), "parsers", true);  // 获取工作目录下的模块路径
		//如果没有,则再看模块目录,即dll同目录下
		if (!StdFile::exists(dllpath.c_str()))  // 如果工作目录下不存在
			dllpath = WtHelper::getModulePath(module.c_str(), "parsers", false);  // 使用模块目录下的路径

		DllHandle hInst = DLLHelper::load_library(dllpath.c_str());  // 加载动态库
		if (hInst == NULL)  // 如果加载失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser module {} loading failed", _id.c_str(), dllpath.c_str());  // 记录错误日志
			return false;  // 返回false
		}
		else  // 如果加载成功
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_INFO, "[{}] Parser module {} loaded", _id.c_str(), dllpath.c_str());  // 记录信息日志
		}

		FuncCreateParser pFuncCreateParser = (FuncCreateParser)DLLHelper::get_symbol(hInst, "createParser");  // 获取创建解析器函数
		if (NULL == pFuncCreateParser)  // 如果获取失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Entrance function createParser not found", _id.c_str());  // 记录致命错误日志
			return false;  // 返回false
		}

		_parser_api = pFuncCreateParser();  // 创建解析器API实例
		if (NULL == _parser_api)  // 如果创建失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[{}] Creating parser api failed", _id.c_str());  // 记录致命错误日志
			return false;  // 返回false
		}

		_remover = (FuncDeleteParser)DLLHelper::get_symbol(hInst, "deleteParser");  // 获取删除解析器函数
	}
	

	const std::string& strFilter = cfg->getString("filter");  // 获取交易所过滤器字符串
	if (!strFilter.empty())  // 如果过滤器不为空
	{
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");  // 按逗号分割过滤器字符串
		auto it = ayFilter.begin();  // 获取迭代器
		for (; it != ayFilter.end(); it++)  // 遍历过滤器列表
		{
			_exchg_filter.insert(*it);  // 添加到交易所过滤器集合
		}
	}

	std::string strCodes = cfg->getString("code");  // 获取合约代码过滤器字符串
	if (!strCodes.empty())  // 如果合约代码过滤器不为空
	{
		const StringVector &ayCodes = StrUtil::split(strCodes, ",");  // 按逗号分割合约代码字符串
		auto it = ayCodes.begin();  // 获取迭代器
		for (; it != ayCodes.end(); it++)  // 遍历合约代码列表
		{
			_code_filter.insert(*it);  // 添加到合约代码过滤器集合
		}
	}

	if (_parser_api)  // 如果解析器API创建成功
	{
		_parser_api->registerSpi(this);  // 注册回调接口（this指向ParserAdapter实例）

		if (_parser_api->init(cfg))  // 如果解析器初始化成功
		{
			ContractSet contractSet;  // 创建合约集合
			WTSArray* ay = _bd_mgr->getContracts();  // 获取所有合约列表
			for(auto it = ay->begin(); it != ay->end(); it++)  // 遍历合约列表
			{
				WTSContractInfo* cInfo = STATIC_CONVERT(*it, WTSContractInfo*);  // 转换为合约信息指针

				//先检查合约和品种是否符合条件
				if(!_code_filter.empty())  // 如果合约代码过滤器不为空
				{
					auto cit = _code_filter.find(cInfo->getFullCode());  // 查找合约代码
					auto pit = _code_filter.find(cInfo->getFullPid());  // 查找品种代码
					if (cit != _code_filter.end() || pit != _code_filter.end())  // 如果合约代码或品种代码在过滤器中
					{
						contractSet.insert(cInfo->getFullCode());  // 添加到订阅集合
						continue;  // 继续下一个合约
					}
				}
				
				//再检查交易所是否符合条件
				if (!_exchg_filter.empty())  // 如果交易所过滤器不为空
				{
					auto eit = _exchg_filter.find(cInfo->getExchg());  // 查找交易所
					if (eit != _exchg_filter.end())  // 如果交易所在过滤器中
					{
						contractSet.insert(cInfo->getFullCode());  // 添加到订阅集合
						continue;  // 继续下一个合约
					}
					else  // 如果交易所不在过滤器中
					{
						continue;  // 跳过该合约
					}
				}

				if(_code_filter.empty() && _exchg_filter.empty())  // 如果两个过滤器都为空
					contractSet.insert(cInfo->getFullCode());  // 订阅所有合约

			}
			ay->release();  // 释放合约数组

			_parser_api->subscribe(contractSet);  // 订阅合约集合
			contractSet.clear();  // 清空合约集合
		}
		else  // 如果解析器初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());  // 记录错误日志
		}
	}
	else  // 如果解析器API创建失败
	{
		WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: creating api failed...", _id.c_str());  // 记录错误日志
	}

	return true;  // 返回成功
}

/**
 * @brief 初始化行情解析器适配器（外部API）
 * @param id 适配器ID
 * @param api 外部行情解析器API指针
 * @param stub 行情数据存根接口指针，用于接收行情数据
 * @param bgMgr 基础数据管理器指针，用于获取合约信息
 * @return 初始化成功返回true，失败返回false
 * 
 * 使用外部提供的行情解析器API初始化适配器，适用于嵌入式场景。
 * 与init()的区别是不需要动态加载模块，直接使用外部提供的API。
 */
bool ParserAdapter::initExt(const char* id, IParserApi* api, IParserStub* stub, IBaseDataMgr* bgMgr)
{
	if (api == NULL)  // 如果API指针为空
		return false;  // 返回false

	_parser_api = api;  // 保存外部API指针
	_stub = stub;  // 保存存根接口指针
	_bd_mgr = bgMgr;  // 保存基础数据管理器指针
	_id = id;  // 保存适配器ID

	if (_parser_api)  // 如果解析器API存在
	{
		_parser_api->registerSpi(this);  // 注册回调接口

		if (_parser_api->init(NULL))  // 如果解析器初始化成功（传入NULL配置）
		{
			ContractSet contractSet;  // 创建合约集合
			WTSArray* ay = _bd_mgr->getContracts();  // 获取所有合约列表
			for (auto it = ay->begin(); it != ay->end(); it++)  // 遍历合约列表
			{
				WTSContractInfo* cInfo = STATIC_CONVERT(*it, WTSContractInfo*);  // 转换为合约信息指针

				//先检查合约和品种是否符合条件
				if (!_code_filter.empty())  // 如果合约代码过滤器不为空
				{
					auto cit = _code_filter.find(cInfo->getFullCode());  // 查找合约代码
					auto pit = _code_filter.find(cInfo->getFullPid());  // 查找品种代码
					if (cit != _code_filter.end() || pit != _code_filter.end())  // 如果合约代码或品种代码在过滤器中
					{
						contractSet.insert(cInfo->getFullCode());  // 添加到订阅集合
						continue;  // 继续下一个合约
					}
				}

				//再检查交易所是否符合条件
				if (!_exchg_filter.empty())  // 如果交易所过滤器不为空
				{
					auto eit = _exchg_filter.find(cInfo->getExchg());  // 查找交易所
					if (eit != _code_filter.end())  // 如果交易所在过滤器中（注意：这里应该是_exchg_filter）
					{
						contractSet.insert(cInfo->getFullCode());  // 添加到订阅集合
						continue;  // 继续下一个合约
					}
					else  // 如果交易所不在过滤器中
					{
						continue;  // 跳过该合约
					}
				}

				if (_code_filter.empty() && _exchg_filter.empty())  // 如果两个过滤器都为空
					contractSet.insert(cInfo->getFullCode());  // 订阅所有合约

			}
			ay->release();  // 释放合约数组

			_parser_api->subscribe(contractSet);  // 订阅合约集合
			contractSet.clear();  // 清空合约集合
		}
		else  // 如果解析器初始化失败
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[{}] Parser initializing failed: api initializing failed...", _id.c_str());  // 记录错误日志
		}
	}

	return true;  // 返回成功
}

/**
 * @brief 释放资源
 * 
 * 释放行情解析器适配器占用的资源，断开连接，释放解析器模块。
 * 设置停止标志，释放解析器API，如果存在删除函数则调用，否则直接删除。
 */
void ParserAdapter::release()
{
	_stopped = true;  // 设置停止标志
	if (_parser_api)  // 如果解析器API存在
	{
		_parser_api->release();  // 释放解析器API
	}

	if (_remover)  // 如果删除函数存在
		_remover(_parser_api);  // 调用删除函数
	else  // 如果删除函数不存在
		delete _parser_api;  // 直接删除解析器API
}

/**
 * @brief 启动解析器
 * @return 启动成功返回true，失败返回false
 * 
 * 启动行情解析器，开始接收行情数据。
 */
bool ParserAdapter::run()
{
	if (_parser_api == NULL)  // 如果解析器API不存在
		return false;  // 返回false

	_parser_api->connect();  // 连接解析器
	return true;  // 返回成功
}

/**
 * @brief 处理实时行情（IParserSpi接口）
 * @param quote 实时行情数据指针
 * @param procFlag 处理标志，是否需要切片
 * 
 * 当解析器接收到Tick行情数据时调用。
 * 验证数据有效性，获取合约信息，标准化合约代码，转发给存根接口。
 */
void ParserAdapter::handleQuote(WTSTickData *quote, uint32_t procFlag)
{
	if (quote == NULL || _stopped || quote->actiondate() == 0)  // 如果行情数据为空、已停止或日期无效
		return;  // 直接返回

	WTSContractInfo* cInfo = quote->getContractInfo();  // 获取合约信息
	if (cInfo == NULL) cInfo = _bd_mgr->getContract(quote->code(), quote->exchg());  // 如果合约信息为空，从基础数据管理器获取
	if (cInfo == NULL)  // 如果仍然获取不到合约信息
		return;  // 直接返回

	quote->setCode(cInfo->getFullCode());  // 设置标准化合约代码

	_stub->handle_push_quote(quote);  // 转发Tick数据给存根接口
}

/**
 * @brief 处理委托队列数据（IParserSpi接口，股票level2）
 * @param ordQueData 委托队列数据指针
 * 
 * 当解析器接收到委托队列数据时调用，用于股票level2行情。
 * 验证数据有效性，检查交易所过滤器，获取合约信息，标准化合约代码，转发给存根接口。
 */
void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(ordQueData->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且交易所不在过滤器中
		return;  // 直接返回

	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)  // 如果日期无效
		return;  // 直接返回

	WTSContractInfo* cInfo = _bd_mgr->getContract(ordQueData->code(), ordQueData->exchg());  // 获取合约信息
	if (cInfo == NULL)  // 如果获取不到合约信息
		return;  // 直接返回

	ordQueData->setCode(cInfo->getFullCode());  // 设置标准化合约代码

	if (_stub)  // 如果存根接口存在
		_stub->handle_push_order_queue(ordQueData);  // 转发委托队列数据给存根接口
}

/**
 * @brief 处理逐笔委托数据（IParserSpi接口，股票level2）
 * @param ordDtlData 逐笔委托数据指针
 * 
 * 当解析器接收到逐笔委托数据时调用，用于股票level2行情。
 * 验证数据有效性，检查交易所过滤器，获取合约信息，标准化合约代码，转发给存根接口。
 */
void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDtlData)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(ordDtlData->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且交易所不在过滤器中
		return;  // 直接返回

	if (ordDtlData->actiondate() == 0 || ordDtlData->tradingdate() == 0)  // 如果日期无效
		return;  // 直接返回

	WTSContractInfo* cInfo = _bd_mgr->getContract(ordDtlData->code(), ordDtlData->exchg());  // 获取合约信息
	if (cInfo == NULL)  // 如果获取不到合约信息
		return;  // 直接返回

	ordDtlData->setCode(cInfo->getFullCode());  // 设置标准化合约代码

	if (_stub)  // 如果存根接口存在
		_stub->handle_push_order_detail(ordDtlData);  // 转发委托明细数据给存根接口
}

/**
 * @brief 处理逐笔成交数据（IParserSpi接口）
 * @param transData 逐笔成交数据指针
 * 
 * 当解析器接收到逐笔成交数据时调用。
 * 验证数据有效性，检查交易所过滤器，获取合约信息，标准化合约代码，转发给存根接口。
 */
void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	if (!_exchg_filter.empty() && (_exchg_filter.find(transData->exchg()) == _exchg_filter.end()))  // 如果交易所过滤器不为空且交易所不在过滤器中
		return;  // 直接返回

	if (transData->actiondate() == 0 || transData->tradingdate() == 0)  // 如果日期无效
		return;  // 直接返回

	WTSContractInfo* cInfo = _bd_mgr->getContract(transData->code(), transData->exchg());  // 获取合约信息
	if (cInfo == NULL)  // 如果获取不到合约信息
		return;  // 直接返回

	transData->setCode(cInfo->getFullCode());  // 设置标准化合约代码

	if (_stub)  // 如果存根接口存在
		_stub->handle_push_transaction(transData);  // 转发逐笔成交数据给存根接口
}


/**
 * @brief 处理解析器日志（IParserSpi接口）
 * @param ll 日志级别
 * @param message 日志消息
 * 
 * 当解析器输出日志时调用，转发日志到日志系统。
 */
void ParserAdapter::handleParserLog(WTSLogLevel ll, const char* message)
{
	if (_stopped)  // 如果已停止
		return;  // 直接返回

	WTSLogger::log_dyn_raw("parser", _id.c_str(), ll, message);  // 记录动态日志
}


//////////////////////////////////////////////////////////////////////////
//ParserAdapterMgr
/**
 * @brief 释放所有适配器
 * 
 * 释放所有管理的适配器资源，清空适配器映射表。
 */
void ParserAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		it->second->release();  // 释放适配器资源
	}

	_adapters.clear();  // 清空适配器映射表
}

/**
 * @brief 添加适配器
 * @param id 适配器ID
 * @param adapter 适配器智能指针
 * @return 添加成功返回true，失败返回false
 * 
 * 将适配器添加到管理器中。如果ID已存在，则添加失败。
 */
bool ParserAdapterMgr::addAdapter(const char* id, ParserAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(id) == 0)  // 如果适配器为空或ID为空
		return false;  // 返回false

	auto it = _adapters.find(id);  // 查找适配器
	if (it != _adapters.end())  // 如果适配器已存在
	{
		WTSLogger::error(" Same name of parsers: {}", id);  // 记录错误日志
		return false;  // 返回false
	}

	_adapters[id] = adapter;  // 添加适配器到映射表

	return true;  // 返回成功
}


/**
 * @brief 获取适配器
 * @param id 适配器ID
 * @return 适配器智能指针，如果不存在返回空指针
 * 
 * 根据适配器ID查找对应的适配器。
 */
ParserAdapterPtr ParserAdapterMgr::getAdapter(const char* id)
{
	auto it = _adapters.find(id);  // 查找适配器
	if (it != _adapters.end())  // 如果找到
	{
		return it->second;  // 返回适配器智能指针
	}

	return ParserAdapterPtr();  // 返回空指针
}

/**
 * @brief 启动所有适配器
 * 
 * 启动所有管理的适配器，开始接收行情数据。
 */
void ParserAdapterMgr::run()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)  // 遍历所有适配器
	{
		it->second->run();  // 启动适配器
	}

	WTSLogger::info("{} parsers started", _adapters.size());  // 记录信息日志
}