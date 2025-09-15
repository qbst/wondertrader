/*!
 * \file WTSBaseDataMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader基础数据管理器的实现文件
 * 
 * 本文件实现了WTSBaseDataMgr.h中定义的基础数据管理器类，是WonderTrader框架中
 * 最核心的基础设施之一，负责管理整个交易系统运行所需的所有静态基础数据。
 * 
 * 设计逻辑和主要作用：
 * ===============
 * 
 * 1. **核心设计理念**：
 *    - 作为交易系统的"数据字典"，提供所有基础信息的统一访问接口
 *    - 实现多层级的数据组织结构，支持复杂的多交易所、多品种环境
 *    - 提供高效的查询机制，支持按多种维度快速检索数据
 *    - 确保数据的完整性和一致性，避免系统运行中的数据错误
 * 
 * 2. **在WonderTrader框架中的核心作用**：
 *    - **合约信息管理**：维护所有交易合约的基本信息（代码、规格、生命周期等）
 *    - **品种信息管理**：管理各种交易品种的参数（最小变动价位、合约乘数、保证金等）
 *    - **交易时间管理**：处理复杂的交易时段规则（开盘、收盘、夜盘、休市等）
 *    - **节假日管理**：维护各地区的交易日历，支持交易日计算
 *    - **数据配置加载**：从配置文件加载基础数据，支持热更新
 * 
 * 3. **数据组织架构**：
 *    - **按交易所分层**：每个交易所维护独立的合约列表
 *    - **多版本支持**：同一合约代码可能对应多个不同的合约（不同月份、不同交易所）
 *    - **时间维度管理**：支持按日期查询有效合约，处理合约的上市和退市
 *    - **关联关系维护**：合约与品种、品种与交易时段的多重关联关系
 * 
 * 4. **核心技术特点**：
 *    - **高性能查询**：使用哈希表实现O(1)时间复杂度的数据检索
 *    - **内存管理**：采用智能指针和引用计数，确保内存安全
 *    - **配置驱动**：支持通过JSON/INI等配置文件灵活配置数据
 *    - **交易日历算法**：实现复杂的交易日计算和节假日判断逻辑
 *    - **时区处理**：支持多时区的时间转换和交易时间计算
 * 
 * 5. **主要使用场景**：
 *    - 策略引擎查询合约信息进行交易决策
 *    - 风控模块获取品种参数进行风险计算
 *    - 数据处理模块根据交易时间进行数据归档
 *    - 行情处理模块验证合约有效性
 *    - 回测引擎模拟历史交易环境
 * 
 * 6. **性能优化考虑**：
 *    - 数据预加载和缓存机制，避免运行时的频繁IO操作
 *    - 多级索引结构，支持多维度的快速查询
 *    - 引用计数管理，避免不必要的内存拷贝
 *    - 延迟加载策略，按需加载大量数据
 * 
 * 7. **扩展性设计**：
 *    - 插件化的数据加载机制，支持不同格式的数据源
 *    - 标准化的接口设计，便于集成新的交易所和品种
 *    - 事件通知机制，支持数据变更的实时通知
 *    - 配置热更新，支持运行时动态调整参数
 */
// 基础数据管理器相关头文件包含
#include "WTSBaseDataMgr.h"          // 基础数据管理器类定义
#include "../WTSUtils/WTSCfgLoader.h" // 配置文件加载器，用于读取各种配置文件
#include "WTSLogger.h"               // 日志记录器，用于记录系统运行日志

// WTS核心数据结构头文件包含
#include "../Includes/WTSContractInfo.hpp" // 合约信息数据结构定义
#include "../Includes/WTSSessionInfo.hpp"  // 交易时段信息数据结构定义
#include "../Includes/WTSVariant.hpp"      // 变体数据类型定义，用于配置参数解析

// 通用工具类头文件包含
#include "../Share/StrUtil.hpp"      // 字符串处理工具类
#include "../Share/StdUtils.hpp"     // 标准工具类，提供文件操作等基础功能

/**
 * @brief 默认节假日模板标识符
 * 
 * 当没有明确指定节假日模板时，系统默认使用中国的节假日模板。
 * 这个常量定义了默认的节假日模板ID，用于交易日计算和节假日判断。
 * 
 * 中国节假日模板包含：
 * - 法定节假日：春节、清明节、劳动节、端午节、中秋节、国庆节等
 * - 周末休息日：周六、周日（除非调休）
 * - 特殊调休安排：根据国务院发布的节假日安排进行调整
 */
const char* DEFAULT_HOLIDAY_TPL = "CHINA";

/**
 * @brief WTSBaseDataMgr构造函数
 * 
 * 初始化基础数据管理器的所有核心数据容器。构造函数采用初始化列表的方式
 * 将所有指针成员初始化为NULL，然后在构造函数体中创建实际的容器对象。
 * 
 * 初始化的数据容器包括：
 * - 交易所合约映射表：按交易所组织合约信息
 * - 交易时段映射表：管理各种交易时间模板
 * - 商品品种映射表：存储品种的基本参数和规则
 * - 合约映射表：支持合约的多版本管理
 * 
 * 这些容器都使用WTS框架的智能指针管理，支持引用计数和自动内存管理。
 */
WTSBaseDataMgr::WTSBaseDataMgr()
	: m_mapExchgContract(NULL)    // 交易所合约映射表指针初始化为NULL
	, m_mapSessions(NULL)         // 交易时段映射表指针初始化为NULL
	, m_mapCommodities(NULL)      // 商品品种映射表指针初始化为NULL
	, m_mapContracts(NULL)        // 合约映射表指针初始化为NULL
{
	// 创建交易所合约映射表，用于按交易所组织和管理合约信息
	// 数据结构：交易所代码 -> 该交易所的所有合约列表
	m_mapExchgContract = WTSExchgContract::create();
	
	// 创建交易时段映射表，用于管理各种交易时间模板
	// 数据结构：时段ID -> 交易时段信息（开盘时间、收盘时间、休市时间等）
	m_mapSessions = WTSSessionMap::create();
	
	// 创建商品品种映射表，用于存储品种的基本参数和交易规则
	// 数据结构：品种标准ID -> 品种信息（最小变动价位、合约乘数、保证金率等）
	m_mapCommodities = WTSCommodityMap::create();
	
	// 创建合约映射表，支持同名合约的多版本管理
	// 数据结构：合约代码 -> 合约信息数组（支持多个交易所的同名合约）
	m_mapContracts = WTSContractMap::create();
}

/**
 * @brief WTSBaseDataMgr析构函数
 * 
 * 清理所有分配的资源，释放内存。析构函数按照与构造函数相反的顺序
 * 释放各个数据容器，确保没有内存泄漏。
 * 
 * 释放过程：
 * 1. 检查指针是否有效（防止重复释放）
 * 2. 调用release()方法减少引用计数
 * 3. 将指针设置为NULL（防止悬空指针）
 * 
 * 由于使用了WTS框架的引用计数机制，release()方法会自动处理内存释放，
 * 只有当引用计数降为0时才会真正释放内存。
 */
WTSBaseDataMgr::~WTSBaseDataMgr()
{
	// 释放交易所合约映射表
	if (m_mapExchgContract)
	{
		m_mapExchgContract->release();  // 减少引用计数，可能触发内存释放
		m_mapExchgContract = NULL;      // 避免悬空指针
	}

	// 释放交易时段映射表
	if (m_mapSessions)
	{
		m_mapSessions->release();       // 减少引用计数，可能触发内存释放
		m_mapSessions = NULL;           // 避免悬空指针
	}

	// 释放商品品种映射表
	if (m_mapCommodities)
	{
		m_mapCommodities->release();    // 减少引用计数，可能触发内存释放
		m_mapCommodities = NULL;        // 避免悬空指针
	}

	// 释放合约映射表
	if(m_mapContracts)
	{
		m_mapContracts->release();      // 减少引用计数，可能触发内存释放
		m_mapContracts = NULL;          // 避免悬空指针
	}
}

/**
 * @brief 根据标准品种ID获取商品信息
 * @param exchgpid 标准品种ID，格式为"交易所.品种代码"（如"SHFE.cu"）
 * @return WTSCommodityInfo* 商品信息指针，未找到返回NULL
 * 
 * 该函数通过标准品种ID直接查询商品信息。标准品种ID是交易所代码和品种代码
 * 的组合，用点号分隔，这种格式确保了品种在全系统范围内的唯一性。
 * 
 * 使用示例：
 * - "SHFE.cu" : 上海期货交易所的铜
 * - "DCE.m"   : 大连商品交易所的豆粕  
 * - "CZCE.CF" : 郑州商品交易所的棉花
 * 
 * 该方法是最直接的商品信息查询方式，时间复杂度为O(1)。
 */
WTSCommodityInfo* WTSBaseDataMgr::getCommodity(const char* exchgpid)
{
	// 直接使用标准品种ID在商品映射表中查找对应的商品信息
	// 由于使用哈希表存储，查找效率为O(1)
	return (WTSCommodityInfo*)m_mapCommodities->get(exchgpid);
}

/**
 * @brief 根据交易所代码和品种代码获取商品信息
 * @param exchg 交易所代码（如"SHFE"、"DCE"、"CZCE"等）
 * @param pid 品种代码（如"cu"、"m"、"CF"等）
 * @return WTSCommodityInfo* 商品信息指针，未找到返回NULL
 * 
 * 该函数通过分离的交易所代码和品种代码查询商品信息。内部会将这两个参数
 * 组合成标准品种ID，然后进行查询。这种方式更符合用户的使用习惯。
 * 
 * 查询过程：
 * 1. 检查商品映射表是否有效
 * 2. 将交易所代码和品种代码组合成标准格式的键值
 * 3. 使用组合后的键值进行哈希查找
 * 
 * 该方法适用于已知具体交易所和品种代码的场景。
 */
WTSCommodityInfo* WTSBaseDataMgr::getCommodity(const char* exchg, const char* pid)
{
	// 检查商品映射表是否已初始化，防止空指针访问
	if (m_mapCommodities == NULL)
		return NULL;

	// 创建固定大小的字符缓冲区，用于构建标准品种ID
	// 64字节足够容纳大部分交易所和品种代码的组合
	char key[64] = { 0 };
	
	// 使用fmt库的format_to函数构建标准品种ID
	// 格式："{交易所代码}.{品种代码}"，如"SHFE.cu"
	// fmt库提供了高效且安全的字符串格式化功能
	fmt::format_to(key, "{}.{}", exchg, pid);

	// 使用构建好的标准品种ID在商品映射表中查找商品信息
	return (WTSCommodityInfo*)m_mapCommodities->get(key);
}


/**
 * @brief 获取合约信息
 * @param code 合约代码（如"cu2012"、"000001"等）
 * @param exchg 交易所代码，默认为空字符串（搜索所有交易所）
 * @param uDate 查询日期（YYYYMMDD格式），用于检查合约有效期，默认为0（不检查）
 * @return WTSContractInfo* 合约信息指针，未找到或已过期返回NULL
 * 
 * 该函数是基础数据管理器中最重要的查询函数之一，支持多种查询模式：
 * 
 * 1. **全局查询模式**（exchg为空）：
 *    - 在所有交易所中查找指定代码的合约
 *    - 支持同名合约的多版本管理（不同交易所的同名合约）
 *    - 返回第一个符合条件的有效合约
 * 
 * 2. **指定交易所查询模式**（指定exchg）：
 *    - 仅在指定交易所中查找合约
 *    - 查询效率更高，避免跨交易所的歧义
 *    - 适用于明确知道交易所的场景
 * 
 * 3. **时间有效性检查**（指定uDate）：
 *    - 检查合约在指定日期是否有效（未过期且已上市）
 *    - 支持历史回测和实时交易的不同需求
 *    - uDate为0时跳过时间检查
 * 
 * 查询算法特点：
 * - 使用哈希表实现O(1)或O(n)的查询效率（n为同名合约数量）
 * - 支持合约生命周期管理（上市日期、到期日期）
 * - 处理合约代码的大小写敏感性问题
 */
WTSContractInfo* WTSBaseDataMgr::getContract(const char* code, const char* exchg /* = "" */, uint32_t uDate /* = 0 */)
{
	// 将合约代码转换为std::string，便于后续的查找操作
	// 同时确保字符串的生命周期和安全性
	auto lKey = std::string(code);

	// 判断是否指定了交易所代码，决定使用哪种查询模式
	if (strlen(exchg) == 0)
	{
		// **全局查询模式**：在所有交易所中查找指定合约代码
		
		// 在全局合约映射表中查找指定的合约代码
		// 该映射表的结构：合约代码 -> 合约信息数组（支持多个同名合约）
		auto it = m_mapContracts->find(lKey);
		if (it == m_mapContracts->end())
			return NULL;  // 未找到该合约代码，返回NULL

		// 获取该合约代码对应的合约信息数组
		// 同一个合约代码可能对应多个合约（不同交易所、不同月份等）
		WTSArray* ayInst = (WTSArray*)it->second;
		if (ayInst == NULL || ayInst->size() == 0)
			return NULL;  // 合约数组为空或无效，返回NULL

		// 遍历所有同名合约，找到第一个符合时间条件的合约
		for(std::size_t i = 0; i < ayInst->size(); i++)
		{
			WTSContractInfo* cInfo = (WTSContractInfo*)ayInst->at(i);
			
			// 检查合约的时间有效性
			// 如果uDate为0，跳过时间检查；否则检查合约是否在指定日期有效
			// 有效条件：合约已上市（开始日期 <= 查询日期）且未到期（到期日期 >= 查询日期）
			if (uDate == 0 || (cInfo->getOpenDate() <= uDate && cInfo->getExpireDate() >= uDate))
				return cInfo;  // 找到符合条件的合约，立即返回
		}
		return NULL;  // 遍历完所有合约都不符合条件，返回NULL
	}
	else
	{
		// **指定交易所查询模式**：仅在指定交易所中查找合约
		
		// 将交易所代码转换为std::string用于查找
		auto sKey = std::string(exchg);
		
		// 在交易所合约映射表中查找指定的交易所
		// 该映射表的结构：交易所代码 -> 该交易所的合约列表
		auto it = m_mapExchgContract->find(sKey);
		if (it != m_mapExchgContract->end())
		{
			// 获取该交易所的合约列表
			WTSContractList* contractList = (WTSContractList*)it->second;
			
			// 在该交易所的合约列表中查找指定的合约代码
			// 该列表的结构：合约代码 -> 合约信息
			auto it = contractList->find(lKey);
			if (it != contractList->end())
			{
				WTSContractInfo* cInfo = (WTSContractInfo*)it->second;
				
				// 检查合约的时间有效性（逻辑与全局查询模式相同）
				// 确保返回的合约在指定日期是有效的
				if (uDate == 0 || (cInfo->getOpenDate() <= uDate && cInfo->getExpireDate() >= uDate))
					return cInfo;  // 合约有效，返回合约信息
			}

			return NULL;  // 在指定交易所中未找到该合约或合约已过期
		}
	}

	// 所有查询路径都未找到合约，返回NULL
	return NULL;
}

/**
 * @brief 获取合约数量统计
 * @param exchg 交易所代码，默认为空字符串（统计所有交易所）
 * @param uDate 查询日期（YYYYMMDD格式），用于筛选有效合约，默认为0（不筛选）
 * @return uint32_t 符合条件的合约数量
 * 
 * 该函数用于统计系统中合约的数量，支持按交易所和时间进行筛选。
 * 主要用途包括：
 * - 系统资源评估：了解需要处理的合约规模
 * - 数据完整性检查：验证数据加载是否正确
 * - 性能优化参考：根据合约数量调整算法策略
 * - 监控和报表：生成系统运行状态报告
 * 
 * 统计逻辑：
 * 1. 如果指定交易所，仅统计该交易所的合约
 * 2. 如果未指定交易所，统计所有交易所的合约
 * 3. 如果指定日期，仅统计在该日期有效的合约
 * 4. 如果未指定日期，统计所有合约（包括已过期的）
 */
uint32_t  WTSBaseDataMgr::getContractSize(const char* exchg /* = "" */, uint32_t uDate /* = 0 */)
{
	uint32_t ret = 0;  // 初始化合约计数器
	
	// 检查是否指定了特定的交易所
	if (strlen(exchg) > 0)
	{
		// **指定交易所统计模式**：仅统计指定交易所的合约数量
		
		// 在交易所合约映射表中查找指定的交易所
		auto it = m_mapExchgContract->find(std::string(exchg));
		if (it != m_mapExchgContract->end())
		{
			// 获取该交易所的合约列表
			WTSContractList* contractList = (WTSContractList*)it->second;
			
			// 遍历该交易所的所有合约
			auto it2 = contractList->begin();
			for (; it2 != contractList->end(); it2++)
			{
				WTSContractInfo* cInfo = (WTSContractInfo*)it2->second;
				
				// 检查合约是否在指定日期有效
				// 如果uDate为0，统计所有合约；否则只统计有效合约
				if (uDate == 0 || (cInfo->getOpenDate() <= uDate && cInfo->getExpireDate() >= uDate))
					ret++;  // 符合条件的合约数量加1
			}
		}
	}
	else
	{
		// **全局统计模式**：统计所有交易所的合约数量
		
		// 遍历所有交易所
		auto it = m_mapExchgContract->begin();
		for (; it != m_mapExchgContract->end(); it++)
		{
			// 获取当前交易所的合约列表
			WTSContractList* contractList = (WTSContractList*)it->second;
			
			// 遍历当前交易所的所有合约
			auto it2 = contractList->begin();
			for (; it2 != contractList->end(); it2++)
			{
				WTSContractInfo* cInfo = (WTSContractInfo*)it2->second;
				
				// 检查合约是否在指定日期有效（逻辑与指定交易所模式相同）
				if (uDate == 0 || (cInfo->getOpenDate() <= uDate && cInfo->getExpireDate() >= uDate))
					ret++;  // 符合条件的合约数量加1
			}
		}
	}

	// 返回统计得到的合约总数量
	return ret;
}

/**
 * @brief 获取合约列表
 * @param exchg 交易所代码，默认为空字符串（获取所有交易所的合约）
 * @param uDate 查询日期（YYYYMMDD格式），用于筛选有效合约，默认为0（不筛选）
 * @return WTSArray* 合约信息数组，调用者负责释放内存
 * 
 * 该函数返回符合条件的所有合约信息的数组，是批量获取合约数据的主要接口。
 * 与getContract()函数的区别在于：
 * - getContract()：返回单个合约信息
 * - getContracts()：返回合约信息数组
 * 
 * 主要应用场景：
 * - 策略初始化：获取可交易的合约列表
 * - 数据分析：批量处理合约数据
 * - 系统监控：检查合约状态和数量
 * - 界面展示：在用户界面中显示合约列表
 * 
 * 内存管理：
 * - 函数创建并返回WTSArray对象
 * - 调用者必须调用release()方法释放内存
 * - 数组中的合约信息使用引用计数管理
 * 
 * 性能考虑：
 * - 大量合约时可能消耗较多内存
 * - 建议使用日期筛选减少返回的数据量
 * - 返回的数组按交易所和合约代码组织
 */
WTSArray* WTSBaseDataMgr::getContracts(const char* exchg /* = "" */, uint32_t uDate /* = 0 */)
{
	// 创建用于存储结果的数组对象
	// WTSArray支持引用计数和自动内存管理
	WTSArray* ay = WTSArray::create();
	
	// 检查是否指定了特定的交易所
	if(strlen(exchg) > 0)
	{
		// **指定交易所模式**：仅获取指定交易所的合约列表
		
		// 在交易所合约映射表中查找指定的交易所
		auto it = m_mapExchgContract->find(std::string(exchg));
		if (it != m_mapExchgContract->end())
		{
			// 获取该交易所的合约列表
			WTSContractList* contractList = (WTSContractList*)it->second;
			
			// 遍历该交易所的所有合约
			auto it2 = contractList->begin();
			for (; it2 != contractList->end(); it2++)
			{
				WTSContractInfo* cInfo = (WTSContractInfo*)it2->second;
				
				// 检查合约的时间有效性
				// 只有符合时间条件的合约才会被添加到结果数组中
				if (uDate == 0 || (cInfo->getOpenDate() <= uDate && cInfo->getExpireDate() >= uDate))
					ay->append(cInfo, true);  // 第二个参数true表示增加引用计数
			}
		}
	}
	else
	{
		// **全局模式**：获取所有交易所的合约列表
		
		// 遍历所有交易所
		auto it = m_mapExchgContract->begin();
		for (; it != m_mapExchgContract->end(); it++)
		{
			// 获取当前交易所的合约列表
			WTSContractList* contractList = (WTSContractList*)it->second;
			
			// 遍历当前交易所的所有合约
			auto it2 = contractList->begin();
			for (; it2 != contractList->end(); it2++)
			{
				WTSContractInfo* cInfo = (WTSContractInfo*)it2->second;
				
				// 检查合约的时间有效性（逻辑与指定交易所模式相同）
				if (uDate == 0 || (cInfo->getOpenDate() <= uDate && cInfo->getExpireDate() >= uDate))
					ay->append(cInfo, true);  // 将符合条件的合约添加到结果数组
			}
		}
	}

	// 返回包含所有符合条件合约的数组
	// 注意：调用者需要负责调用ay->release()来释放内存
	return ay;
}

/**
 * @brief 获取所有交易时段信息
 * @return WTSArray* 包含所有交易时段信息的数组，调用者负责释放内存
 * 
 * 该函数返回系统中配置的所有交易时段信息，主要用于：
 * - 系统初始化：了解所有可用的交易时段
 * - 配置验证：检查交易时段配置的完整性
 * - 界面展示：在管理界面中显示所有交易时段
 * - 数据分析：分析不同交易时段的特征
 * 
 * 返回的数组包含系统中所有已配置的交易时段信息，每个时段包含：
 * - 时段ID和名称
 * - 开盘和收盘时间
 * - 休市时间段
 * - 时区信息
 */
WTSArray* WTSBaseDataMgr::getAllSessions()
{
	// 创建用于存储所有交易时段信息的数组
	WTSArray* ay = WTSArray::create();
	
	// 遍历交易时段映射表，将所有时段信息添加到数组中
	for (auto it = m_mapSessions->begin(); it != m_mapSessions->end(); it++)
	{
		// 将时段信息添加到数组，第二个参数true表示增加引用计数
		ay->append(it->second, true);
	}
	
	// 返回包含所有交易时段信息的数组
	return ay;
}

/**
 * @brief 根据时段ID获取交易时段信息
 * @param sid 时段ID（如"TRADING"、"DAY"、"NIGHT"等）
 * @return WTSSessionInfo* 交易时段信息指针，未找到返回NULL
 * 
 * 该函数通过时段ID直接查询对应的交易时段信息。时段ID是交易时段的唯一标识符，
 * 通常在配置文件中定义，用于区分不同的交易时间模板。
 * 
 * 常见的时段ID示例：
 * - "DAY"：日盘交易时段
 * - "NIGHT"：夜盘交易时段  
 * - "TRADING"：完整交易时段（包含日盘和夜盘）
 * 
 * 该方法是获取交易时段信息的最直接方式，时间复杂度为O(1)。
 */
WTSSessionInfo* WTSBaseDataMgr::getSession(const char* sid)
{
	// 直接使用时段ID在交易时段映射表中查找对应的时段信息
	return (WTSSessionInfo*)m_mapSessions->get(sid);
}

/**
 * @brief 根据合约代码获取交易时段信息
 * @param code 合约代码（如"cu2012"、"000001"等）
 * @param exchg 交易所代码，默认为空字符串（搜索所有交易所）
 * @return WTSSessionInfo* 交易时段信息指针，未找到返回NULL
 * 
 * 该函数通过合约代码间接获取对应的交易时段信息。查询路径为：
 * 合约代码 -> 合约信息 -> 品种信息 -> 交易时段信息
 * 
 * 这种查询方式的优点：
 * - 用户只需知道合约代码，无需了解时段ID
 * - 自动获取该合约适用的交易时间规则
 * - 支持不同品种使用不同的交易时段
 * 
 * 主要应用场景：
 * - 策略交易：根据合约获取可交易时间
 * - 数据处理：按合约的交易时间进行数据分组
 * - 风控管理：检查合约是否在交易时间内
 */
WTSSessionInfo* WTSBaseDataMgr::getSessionByCode(const char* code, const char* exchg /* = "" */)
{
	// 首先根据合约代码获取合约信息
	WTSContractInfo* ct = getContract(code, exchg);
	if (ct == NULL)
		return NULL;  // 合约不存在，返回NULL

	// 通过合约信息获取品种信息，再获取交易时段信息
	// 查询链：合约 -> 品种 -> 交易时段
	return ct->getCommInfo()->getSessionInfo();
}

/**
 * @brief 判断指定日期是否为节假日
 * @param pid 品种ID或节假日模板ID
 * @param uDate 要检查的日期（YYYYMMDD格式）
 * @param isTpl 是否直接使用模板ID，默认为false（使用品种ID）
 * @return bool 是节假日返回true，否则返回false
 * 
 * 该函数是交易日历系统的核心函数，用于判断指定日期是否为节假日。
 * 节假日判断遵循以下优先级：
 * 
 * 1. **周末检查**（最高优先级）：
 *    - 周六（wd=6）和周日（wd=0）默认为节假日
 *    - 即使不在节假日模板中，周末也被视为节假日
 * 
 * 2. **节假日模板检查**：
 *    - 根据品种ID获取对应的节假日模板
 *    - 在模板的节假日集合中查找指定日期
 *    - 支持不同地区和品种使用不同的节假日规则
 * 
 * 参数说明：
 * - 当isTpl=false时，pid被视为品种ID，需要查找对应的模板ID
 * - 当isTpl=true时，pid被直接视为模板ID使用
 * 
 * 应用场景：
 * - 交易日计算：确定下一个交易日
 * - 策略调度：避免在节假日执行交易操作
 * - 数据处理：跳过节假日的数据分析
 * - 风控检查：验证交易时间的合法性
 */
bool WTSBaseDataMgr::isHoliday(const char* pid, uint32_t uDate, bool isTpl /* = false */)
{
	// 首先检查是否为周末（周六或周日）
	// TimeUtils::getWeekDay()返回：0=周日, 1=周一, ..., 6=周六
	uint32_t wd = TimeUtils::getWeekDay(uDate);
	if (wd == 0 || wd == 6)
		return true;  // 周末始终视为节假日

	// 确定要使用的节假日模板ID
	std::string tplid = pid;
	if (!isTpl)
	{
		// 如果传入的是品种ID，需要查找对应的节假日模板ID
		// 不同品种可能使用不同地区的节假日规则
		tplid = getTplIDByPID(pid);
	}

	// 在节假日模板映射表中查找指定的模板
	auto it = m_mapTradingDay.find(tplid.c_str());
	if(it != m_mapTradingDay.end())
	{
		// 获取节假日模板对象
		const TradingDayTpl& tpl = it->second;
		
		// 在节假日集合中查找指定日期
		// 如果找到，说明该日期是节假日
		return (tpl._holidays.find(uDate) != tpl._holidays.end());
	}

	// 未找到对应的节假日模板，默认不是节假日
	// 这种情况通常发生在模板配置不完整时
	return false;
}


/**
 * @brief 释放所有资源
 * 
 * 该函数用于释放基础数据管理器占用的所有资源，主要在以下情况下调用：
 * - 系统关闭时的资源清理
 * - 重新加载配置前的资源释放
 * - 内存不足时的主动清理
 * 
 * 释放顺序和析构函数保持一致，确保资源的正确释放。
 * 与析构函数的区别：
 * - release()：主动释放资源，对象仍然可用
 * - 析构函数：对象生命周期结束时的自动清理
 * 
 * 注意：调用此函数后，需要重新调用相应的load函数才能正常使用。
 */
void WTSBaseDataMgr::release()
{
	// 释放交易所合约映射表
	// 包含所有交易所的合约信息
	if (m_mapExchgContract)
	{
		m_mapExchgContract->release();  // 减少引用计数，可能触发内存释放
		m_mapExchgContract = NULL;      // 避免悬空指针
	}

	// 释放交易时段映射表
	// 包含所有交易时间模板信息
	if (m_mapSessions)
	{
		m_mapSessions->release();       // 减少引用计数，可能触发内存释放
		m_mapSessions = NULL;           // 避免悬空指针
	}

	// 释放商品品种映射表
	// 包含所有品种的基本参数和规则
	if (m_mapCommodities)
	{
		m_mapCommodities->release();    // 减少引用计数，可能触发内存释放
		m_mapCommodities = NULL;        // 避免悬空指针
	}
	
	// 注意：m_mapContracts在这里没有释放，可能是因为它与m_mapExchgContract共享数据
	// 或者在其他地方进行释放，这需要根据具体的设计来确定
}

/**
 * @brief 加载交易时段配置
 * @param filename 交易时段配置文件路径
 * @return bool 加载成功返回true，失败返回false
 * 
 * 该函数从配置文件中加载所有交易时段的定义，是系统初始化的关键步骤之一。
 * 交易时段配置包含了各个品种的交易时间规则，是交易系统正常运行的基础。
 * 
 * 配置文件格式（JSON）示例：
 * {
 *   "DAY": {
 *     "name": "日盘",
 *     "offset": 0,
 *     "auction": {"from": 85500, "to": 90000},
 *     "sections": [
 *       {"from": 90000, "to": 101500},
 *       {"from": 103000, "to": 150000}
 *     ]
 *   }
 * }
 * 
 * 配置项说明：
 * - id: 时段标识符（如"DAY"、"NIGHT"）
 * - name: 时段显示名称
 * - offset: 时区偏移量（小时）
 * - auction/auctions: 集合竞价时间段
 * - sections: 连续交易时间段数组
 * 
 * 加载过程：
 * 1. 检查配置文件是否存在
 * 2. 解析JSON格式的配置文件
 * 3. 遍历所有时段定义
 * 4. 创建WTSSessionInfo对象
 * 5. 设置集合竞价时间
 * 6. 添加连续交易时间段
 * 7. 将时段信息加入映射表
 */
bool WTSBaseDataMgr::loadSessions(const char* filename)
{
	// 检查交易时段配置文件是否存在
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("Trading sessions configuration file {} not exists", filename);
		return false;  // 文件不存在，加载失败
	}

	// 使用配置加载器解析配置文件
	// WTSCfgLoader支持JSON、INI等多种格式
	WTSVariant* root = WTSCfgLoader::load_from_file(filename);
	if (root == NULL)
	{
		WTSLogger::error("Loading session config file {} failed", filename);
		return false;  // 配置文件解析失败
	}

	// 遍历配置文件中的所有交易时段定义
	for(const std::string& id : root->memberNames())
	{
		// 获取当前时段的配置对象
		WTSVariant* jVal = root->get(id);

		// 从配置中读取时段的基本信息
		const char* name = jVal->getCString("name");    // 时段显示名称
		int32_t offset = jVal->getInt32("offset");      // 时区偏移量

		// 创建交易时段信息对象
		WTSSessionInfo* sInfo = WTSSessionInfo::create(id.c_str(), name, offset);

		// 处理集合竞价时间配置
		// 支持单个集合竞价时间段（auction）和多个集合竞价时间段（auctions）
		if (jVal->has("auction"))
		{
			// 单个集合竞价时间段配置
			WTSVariant* jAuc = jVal->get("auction");
			sInfo->setAuctionTime(jAuc->getUInt32("from"), jAuc->getUInt32("to"));
		}
		else if (jVal->has("auctions"))
		{
			// 多个集合竞价时间段配置（支持多次集合竞价）
			WTSVariant* jAucs = jVal->get("auctions");
			for (uint32_t i = 0; i < jAucs->size(); i++)
			{
				WTSVariant* jSec = jAucs->get(i);
				sInfo->addAuctionTime(jSec->getUInt32("from"), jSec->getUInt32("to"));
			}
		}

		// 处理连续交易时间段配置
		WTSVariant* jSecs = jVal->get("sections");
		if (jSecs == NULL || !jSecs->isArray())
			continue;  // 没有交易时间段配置，跳过当前时段

		// 遍历所有连续交易时间段
		for (uint32_t i = 0; i < jSecs->size(); i++)
		{
			WTSVariant* jSec = jSecs->get(i);
			// 添加交易时间段（开始时间、结束时间）
			// 时间格式：HHMMSS（如90000表示9:00:00）
			sInfo->addTradingSection(jSec->getUInt32("from"), jSec->getUInt32("to"));
		}

		// 将配置好的交易时段信息添加到映射表中
		m_mapSessions->add(id.c_str(), sInfo);
	}

	// 释放配置文件解析产生的根对象，避免内存泄漏
	root->release();

	// 所有交易时段配置加载成功
	return true;
}

/**
 * @brief 解析商品品种配置信息
 * @param pCommInfo 商品信息对象指针
 * @param jPInfo 包含品种配置的JSON对象
 * 
 * 该函数是loadCommodities()的辅助函数，用于从JSON配置中解析单个商品品种的
 * 详细参数，并设置到WTSCommodityInfo对象中。
 * 
 * 解析的配置参数包括：
 * - pricetick: 最小变动价位（如0.01表示价格最小变动单位为0.01）
 * - volscale: 合约乘数（如10表示每手合约代表10个单位标的物）
 * - category: 合约类别（期货、股票、期权等）
 * - covermode: 平仓模式（平今、平昨等）
 * - pricemode: 报价模式（绝对价格、相对价格等）
 * - trademode: 交易模式（多空双向、只做多、只做空）
 * - lotstick: 手数最小变动单位
 * - minlots: 最小交易手数
 * 
 * 该函数处理了配置的兼容性，为可选字段提供合理的默认值。
 */
void parseCommodity(WTSCommodityInfo* pCommInfo, WTSVariant* jPInfo)
{
	// 设置最小变动价位，决定价格的精度
	// 例如：0.01表示价格最小变动单位为0.01元
	pCommInfo->setPriceTick(jPInfo->getDouble("pricetick"));
	
	// 设置合约乘数，决定每手合约的标的物数量
	// 例如：10表示每手合约代表10个单位的标的物
	pCommInfo->setVolScale(jPInfo->getUInt32("volscale"));

	// 设置合约类别，支持期货、股票、期权等不同类型
	if (jPInfo->has("category"))
		pCommInfo->setCategory((ContractCategory)jPInfo->getUInt32("category"));
	else
		pCommInfo->setCategory(CC_Future);  // 默认为期货类别

	// 设置平仓模式，决定如何处理持仓的平仓操作
	// 包括平今、平昨、先开先平等不同模式
	pCommInfo->setCoverMode((CoverMode)jPInfo->getUInt32("covermode"));
	
	// 设置报价模式，决定价格的表示方式
	// 包括绝对价格、相对价格、百分比等不同模式
	pCommInfo->setPriceMode((PriceMode)jPInfo->getUInt32("pricemode"));

	// 设置交易模式，决定允许的交易方向
	if (jPInfo->has("trademode"))
		pCommInfo->setTradingMode((TradingMode)jPInfo->getUInt32("trademode"));
	else
		pCommInfo->setTradingMode(TM_Both);  // 默认支持多空双向交易

	// 设置手数相关参数，控制交易数量的精度
	double lotsTick = 1;  // 手数最小变动单位，默认为1
	double minLots = 1;   // 最小交易手数，默认为1手
	
	// 从配置中读取手数参数（如果存在）
	if (jPInfo->has("lotstick"))
		lotsTick = jPInfo->getDouble("lotstick");
	if (jPInfo->has("minlots"))
		minLots = jPInfo->getDouble("minlots");
		
	// 应用手数参数设置
	pCommInfo->setLotsTick(lotsTick);  // 设置手数最小变动单位
	pCommInfo->setMinLots(minLots);    // 设置最小交易手数
}

/**
 * @brief 加载商品品种配置
 * @param filename 商品品种配置文件路径
 * @return bool 加载成功返回true，失败返回false
 * 
 * 该函数从配置文件中加载所有商品品种的定义，是基础数据管理器初始化的核心步骤。
 * 商品品种配置包含了各个交易品种的详细参数，是交易系统正常运行的基础。
 * 
 * 配置文件格式（JSON）示例：
 * {
 *   "SHFE": {
 *     "cu": {
 *       "name": "铜",
 *       "session": "TRADING",
 *       "holiday": "CHINA",
 *       "pricetick": 10,
 *       "volscale": 5,
 *       "category": 1,
 *       "covermode": 0,
 *       "pricemode": 0,
 *       "trademode": 2
 *     }
 *   }
 * }
 * 
 * 数据结构层次：
 * 交易所 -> 品种 -> 品种详细参数
 * 
 * 加载过程：
 * 1. 检查配置文件是否存在
 * 2. 解析JSON格式的配置文件
 * 3. 遍历所有交易所
 * 4. 遍历每个交易所的所有品种
 * 5. 创建WTSCommodityInfo对象
 * 6. 解析品种详细参数
 * 7. 关联交易时段信息
 * 8. 建立品种映射关系
 * 9. 维护时段与品种的关联
 */
bool WTSBaseDataMgr::loadCommodities(const char* filename)
{
	// 检查商品品种配置文件是否存在
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("Commodities configuration file {} not exists", filename);
		return false;  // 文件不存在，加载失败
	}

	// 使用配置加载器解析配置文件
	WTSVariant* root = WTSCfgLoader::load_from_file(filename);
	if (root == NULL)
	{
		WTSLogger::error("Loading commodities config file {} failed", filename);
		return false;  // 配置文件解析失败
	}

	// 第一层循环：遍历所有交易所
	for(const std::string& exchg : root->memberNames())
	{
		// 获取当前交易所的配置对象
		WTSVariant* jExchg = root->get(exchg);

		// 第二层循环：遍历当前交易所的所有品种
		for (const std::string& pid : jExchg->memberNames())
		{
			// 获取当前品种的配置对象
			WTSVariant* jPInfo = jExchg->get(pid);

			// 从配置中读取品种的基本信息
			const char* name = jPInfo->getCString("name");       // 品种显示名称
			const char* sid = jPInfo->getCString("session");     // 交易时段ID
			const char* hid = jPInfo->getCString("holiday");     // 节假日模板ID

			// 验证必要的配置项
			if (strlen(sid) == 0)
			{
				WTSLogger::warn("No session configured for {}.{}", exchg.c_str(), pid.c_str());
				continue;  // 没有配置交易时段，跳过该品种
			}

			// 创建商品品种信息对象
			// 参数：品种代码、品种名称、交易所代码、交易时段ID、节假日模板ID
			WTSCommodityInfo* pCommInfo = WTSCommodityInfo::create(pid.c_str(), name, exchg.c_str(), sid, hid);
			
			// 解析品种的详细参数（价格精度、合约乘数等）
			parseCommodity(pCommInfo, jPInfo);

			// 获取并关联交易时段信息
			WTSSessionInfo* sInfo = getSession(sid);
			pCommInfo->setSessionInfo(sInfo);

			// 构建品种的标准ID（格式：交易所.品种代码）
			std::string key = fmt::format("{}.{}", exchg.c_str(), pid.c_str());
			
			// 确保商品映射表已初始化
			if (m_mapCommodities == NULL)
				m_mapCommodities = WTSCommodityMap::create();

			// 将品种信息添加到映射表中
			// 第三个参数false表示不自动增加引用计数
			m_mapCommodities->add(key, pCommInfo, false);

			// 维护时段与品种代码的关联关系
			// 用于快速查询某个时段下的所有品种
			m_mapSessionCode[sid].insert(key);
		}
	}

	// 记录配置加载成功的日志
	WTSLogger::info("Commodities configuration file {} loaded", filename);
	
	// 释放配置文件解析产生的根对象，避免内存泄漏
	root->release();
	
	// 所有商品品种配置加载成功
	return true;
}

/**
 * @brief 加载合约配置
 * @param filename 合约配置文件路径
 * @return bool 加载成功返回true，失败返回false
 * 
 * 该函数从配置文件中加载所有交易合约的定义，是基础数据管理器初始化的最后一个关键步骤。
 * 合约配置包含了每个具体交易合约的详细信息，包括交易限制、生命周期、保证金等参数。
 * 
 * 配置文件格式（JSON）示例：
 * {
 *   "SHFE": {
 *     "cu2012": {
 *       "name": "沪铜2012",
 *       "exchg": "SHFE",
 *       "product": "cu",
 *       "maxmarketqty": 500,
 *       "maxlimitqty": 1000,
 *       "minmarketqty": 1,
 *       "minlimitqty": 1,
 *       "opendate": 20200301,
 *       "expiredate": 20201215,
 *       "longmarginratio": 0.08,
 *       "shortmarginratio": 0.08
 *     }
 *   }
 * }
 * 
 * 特殊处理逻辑：
 * 1. **标准模式**：合约配置包含product字段，引用已定义的品种
 * 2. **兼容模式**：合约配置包含rules字段，自动创建对应的品种
 * 
 * 数据结构层次：
 * 交易所 -> 合约 -> 合约详细参数
 * 
 * 加载过程：
 * 1. 检查配置文件是否存在
 * 2. 解析JSON格式的配置文件
 * 3. 遍历所有交易所
 * 4. 遍历每个交易所的所有合约
 * 5. 处理品种关联（标准模式或兼容模式）
 * 6. 创建WTSContractInfo对象
 * 7. 设置交易限制和生命周期
 * 8. 建立多级映射关系
 * 9. 维护合约与品种的关联
 */
bool WTSBaseDataMgr::loadContracts(const char* filename)
{
	// 检查合约配置文件是否存在
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("Contracts configuration file {} not exists", filename);
		return false;  // 文件不存在，加载失败
	}

	// 使用配置加载器解析配置文件
	WTSVariant* root = WTSCfgLoader::load_from_file(filename);
	if (root == NULL)
	{
		WTSLogger::error("Loading contracts config file {} failed", filename);
		return false;  // 配置文件解析失败
	}

	// 第一层循环：遍历所有交易所
	for(const std::string& exchg : root->memberNames())
	{
		// 获取当前交易所的配置对象
		WTSVariant* jExchg = root->get(exchg);

		// 第二层循环：遍历当前交易所的所有合约
		for(const std::string& code : jExchg->memberNames())
		{
			// 获取当前合约的配置对象
			WTSVariant* jcInfo = jExchg->get(code);

			// 处理品种关联：支持两种模式
			// 1. 标准模式：通过product字段引用已定义的品种
			// 2. 兼容模式：通过rules字段自动创建品种（向后兼容）
			WTSCommodityInfo* commInfo = NULL;
			std::string pid;  // 品种ID
			
			if(jcInfo->has("product"))
			{
				// **标准模式**：引用已定义的品种
				pid = jcInfo->getCString("product");
				commInfo = getCommodity(jcInfo->getCString("exchg"), pid.c_str());
			}
			else if(jcInfo->has("rules"))
			{
				// **兼容模式**：自动创建品种
				// 这种模式主要用于向后兼容，当合约配置中包含完整的品种规则时
				pid = code.c_str();  // 使用合约代码作为品种ID
				WTSVariant* jPInfo = jcInfo->get("rules");
				const char* name = jcInfo->getCString("name");
				std::string sid = jPInfo->getCString("session");
				std::string hid;
				if(jPInfo->has("holiday"))
					hid = jPInfo->getCString("holiday");

				// 如果没有指定交易时段，默认使用全天交易
				if (sid.empty())
					sid = "ALLDAY";

				// 创建新的商品品种信息
				commInfo = WTSCommodityInfo::create(pid.c_str(), name, exchg.c_str(), sid.c_str(), hid.c_str());
				parseCommodity(commInfo, jPInfo);  // 解析品种详细参数
				
				// 关联交易时段信息
				WTSSessionInfo* sInfo = getSession(sid.c_str());
				commInfo->setSessionInfo(sInfo);

				// 将自动创建的品种添加到品种映射表
				std::string key = fmt::format("{}.{}", exchg.c_str(), pid.c_str());
				if (m_mapCommodities == NULL)
					m_mapCommodities = WTSCommodityMap::create();

				m_mapCommodities->add(key, commInfo, false);
				m_mapSessionCode[sid].insert(key);

				WTSLogger::debug("Commodity {} has been automatically added", key.c_str());
			}

			// 验证品种信息是否获取成功
			if (commInfo == NULL)
			{
				WTSLogger::warn("Commodity {}.{} not found, contract {} skipped", 
					jcInfo->getCString("exchg"), jcInfo->getCString("product"), code.c_str());
				continue;  // 跳过无效的合约
			}

			// 创建合约信息对象
			WTSContractInfo* cInfo = WTSContractInfo::create(code.c_str(),
				jcInfo->getCString("name"),
				jcInfo->getCString("exchg"),
				pid.c_str());

			// 关联品种信息
			cInfo->setCommInfo(commInfo);

			// 设置交易数量限制（市价单和限价单的最大最小数量）
			uint32_t maxMktQty = 1000000;  // 默认市价单最大数量
			uint32_t maxLmtQty = 1000000;  // 默认限价单最大数量
			uint32_t minMktQty = 1;        // 默认市价单最小数量
			uint32_t minLmtQty = 1;        // 默认限价单最小数量
			
			// 从配置中读取交易数量限制（如果存在）
			if (jcInfo->has("maxmarketqty"))
				maxMktQty = jcInfo->getUInt32("maxmarketqty");
			if (jcInfo->has("maxlimitqty"))
				maxLmtQty = jcInfo->getUInt32("maxlimitqty");
			if (jcInfo->has("minmarketqty"))
				minMktQty = jcInfo->getUInt32("minmarketqty");
			if (jcInfo->has("minlimitqty"))
				minLmtQty = jcInfo->getUInt32("minlimitqty");
			
			// 应用交易数量限制设置
			cInfo->setVolumeLimits(maxMktQty, maxLmtQty, minMktQty, minLmtQty);

			// 设置合约生命周期（上市日期和到期日期）
			uint32_t opendate = 0;    // 上市日期，0表示无限制
			uint32_t expiredate = 0;  // 到期日期，0表示无限制
			if (jcInfo->has("opendate"))
				opendate = jcInfo->getUInt32("opendate");
			if (jcInfo->has("expiredate"))
				expiredate = jcInfo->getUInt32("expiredate");
			
			// 应用生命周期设置
			cInfo->setDates(opendate, expiredate);

			// 设置保证金比率（多头和空头）
			double lMargin = 0;  // 多头保证金比率
			double sMargin = 0;  // 空头保证金比率
			if (jcInfo->has("longmarginratio"))
				lMargin = jcInfo->getDouble("longmarginratio");
			if (jcInfo->has("shortmarginratio"))
				sMargin = jcInfo->getDouble("shortmarginratio");
			
			// 应用保证金比率设置
			cInfo->setMarginRatios(lMargin, sMargin);

			// 建立交易所级别的合约映射关系
			WTSContractList* contractList = (WTSContractList*)m_mapExchgContract->get(std::string(cInfo->getExchg()));
			if (contractList == NULL)
			{
				// 如果该交易所的合约列表不存在，创建新的列表
				contractList = WTSContractList::create();
				m_mapExchgContract->add(std::string(cInfo->getExchg()), contractList, false);
			}
			// 将合约添加到交易所的合约列表中
			contractList->add(std::string(cInfo->getCode()), cInfo, false);

			// 在品种信息中记录该合约代码
			commInfo->addCode(code.c_str());

			// 建立全局合约映射关系（支持同名合约的多版本管理）
			std::string key = std::string(cInfo->getCode());
			WTSArray* ayInst = (WTSArray*)m_mapContracts->get(key);
			if(ayInst == NULL)
			{
				// 如果该合约代码的数组不存在，创建新的数组
				ayInst = WTSArray::create();
				m_mapContracts->add(key, ayInst, false);
			}

			// 将合约信息添加到数组中（支持多个交易所的同名合约）
			ayInst->append(cInfo, true);
		}
	}

	// 记录配置加载成功的日志
	WTSLogger::info("Contracts configuration file {} loaded, {} exchanges", filename, m_mapExchgContract->size());
	
	// 释放配置文件解析产生的根对象，避免内存泄漏
	root->release();
	
	// 所有合约配置加载成功
	return true;
}

/**
 * @brief 加载节假日配置
 * @param filename 节假日配置文件路径
 * @return bool 加载成功返回true，失败返回false
 * 
 * 该函数从配置文件中加载各种节假日模板的定义，是交易日历系统的核心组件。
 * 节假日配置用于判断某个日期是否为交易日，对于交易系统的调度和风控至关重要。
 * 
 * 配置文件格式（JSON）示例：
 * {
 *   "CHINA": [
 *     20230101,  // 元旦
 *     20230121,  // 春节
 *     20230122,  // 春节
 *     20230123,  // 春节
 *     20230405,  // 清明节
 *     20230501,  // 劳动节
 *     20230622,  // 端午节
 *     20230929,  // 中秋节
 *     20231001,  // 国庆节
 *     20231002   // 国庆节
 *   ],
 *   "US": [
 *     20230101,  // New Year's Day
 *     20230220,  // Presidents' Day
 *     20230529,  // Memorial Day
 *     20230704,  // Independence Day
 *     20230904,  // Labor Day
 *     20231123,  // Thanksgiving
 *     20231225   // Christmas
 *   ]
 * }
 * 
 * 数据结构：
 * 节假日模板ID -> 节假日日期列表
 * 
 * 加载过程：
 * 1. 检查配置文件是否存在
 * 2. 解析JSON格式的配置文件
 * 3. 遍历所有节假日模板
 * 4. 为每个模板创建节假日集合
 * 5. 将所有节假日日期添加到对应的集合中
 * 
 * 注意事项：
 * - 日期格式为YYYYMMDD的整数形式
 * - 周末（周六、周日）不需要在配置中明确列出，系统会自动识别
 * - 支持多个地区的节假日模板，如中国、美国、欧洲等
 */
bool WTSBaseDataMgr::loadHolidays(const char* filename)
{
	// 检查节假日配置文件是否存在
	if (!StdFile::exists(filename))
	{
		WTSLogger::error("Holidays configuration file {} not exists", filename);
		return false;  // 文件不存在，加载失败
	}

	// 使用配置加载器解析配置文件
	WTSVariant* root = WTSCfgLoader::load_from_file(filename);
	if (root == NULL)
	{
		WTSLogger::error("Loading holidays config file {} failed", filename);
		return false;  // 配置文件解析失败
	}

	// 遍历所有节假日模板（如"CHINA"、"US"、"EU"等）
	for (const std::string& hid : root->memberNames())
	{
		// 获取当前节假日模板的配置对象
		WTSVariant* jHolidays = root->get(hid);
		
		// 验证配置格式：节假日列表必须是数组格式
		if(!jHolidays->isArray())
			continue;  // 格式不正确，跳过该模板

		// 获取或创建对应的交易日模板对象
		// 如果该模板不存在，map会自动创建一个新的模板
		TradingDayTpl& trdDayTpl = m_mapTradingDay[hid];
		
		// 遍历节假日列表中的所有日期
		for(uint32_t i = 0; i < jHolidays->size(); i++)
		{
			// 获取单个节假日配置项
			WTSVariant* hItem = jHolidays->get(i);
			
			// 将节假日日期添加到节假日集合中
			// 使用set容器自动去重，确保同一日期不会重复添加
			// 日期格式：YYYYMMDD（如20230101表示2023年1月1日）
			trdDayTpl._holidays.insert(hItem->asUInt32());
		}
	}

	// 释放配置文件解析产生的根对象，避免内存泄漏
	root->release();

	// 所有节假日配置加载成功
	return true;
}

/**
 * @brief 获取边界时间
 * @param stdPID 标准品种ID或时段ID
 * @param tDate 交易日期（YYYYMMDD格式），0表示使用当前日期
 * @param isSession 是否直接使用时段ID，默认为false（使用品种ID）
 * @param isStart 是否获取开始时间，默认为true（获取开始时间），false为结束时间
 * @return uint64_t 边界时间（YYYYMMDDHHMM格式），获取失败返回0
 * 
 * 该函数是交易时间计算的核心函数，用于获取指定交易日的开盘或收盘边界时间。
 * 该函数处理了复杂的交易时间逻辑，包括：
 * 
 * 1. **时区偏移处理**：
 *    - 无偏移（offset=0）：标准交易时间，开盘收盘都在同一自然日
 *    - 负偏移（offset<0）：外盘模式，交易日推后
 *    - 正偏移（offset>0）：夜盘模式，夜盘属于下一个交易日
 * 
 * 2. **周末和节假日处理**：
 *    - 自动跳过周末和节假日
 *    - 获取最近的有效交易日
 * 
 * 3. **复杂时间边界计算**：
 *    - 夜盘开始时间：属于上一个交易日的晚上
 *    - 夜盘结束时间：属于当前交易日
 *    - 外盘时间：跨越自然日的处理
 * 
 * 应用场景：
 * - 数据归档：确定数据的归属交易日
 * - 策略调度：在正确的时间启动和停止策略
 * - 风控检查：验证交易时间的合法性
 * - 行情处理：处理跨日行情的时间归属
 */
uint64_t WTSBaseDataMgr::getBoundaryTime(const char* stdPID, uint32_t tDate, bool isSession /* = false */, bool isStart /* = true */)
{
	// 如果没有指定日期，使用当前日期
	if(tDate == 0)
		tDate = TimeUtils::getCurDate();
	
	// 初始化节假日模板ID和相关变量
	std::string tplid = stdPID;
	bool isTpl = false;
	WTSSessionInfo* sInfo = NULL;
	
	// 根据参数类型获取交易时段信息
	if (isSession)
	{
		// **直接时段模式**：stdPID是时段ID
		sInfo = getSession(stdPID);
		tplid = DEFAULT_HOLIDAY_TPL;  // 使用默认节假日模板
		isTpl = true;
	}
	else
	{
		// **品种模式**：stdPID是品种ID，需要通过品种获取时段信息
		WTSCommodityInfo* cInfo = getCommodity(stdPID);
		if (cInfo == NULL)
			return 0;  // 品种不存在，返回0

		sInfo = cInfo->getSessionInfo();
	}

	// 验证交易时段信息是否有效
	if (sInfo == NULL)
		return 0;  // 时段信息无效，返回0

	// 检查指定日期是否为周末，如果是则调整到有效交易日
	uint32_t weekday = TimeUtils::getWeekDay(tDate);
	if (weekday == 6 || weekday == 0)  // 周六或周日
	{
		if (isStart)
			tDate = getNextTDate(tplid.c_str(), tDate, 1, isTpl);  // 获取下一个交易日
		else
			tDate = getPrevTDate(tplid.c_str(), tDate, 1, isTpl);  // 获取上一个交易日
	}

	// **情况1：无时区偏移**（最简单的情况）
	// 开盘和收盘都在同一个自然日内，直接返回对应时间
	if (sInfo->getOffsetMins() == 0)
	{
		if (isStart)
			return (uint64_t)tDate * 10000 + sInfo->getOpenTime();   // 开盘时间
		else
			return (uint64_t)tDate * 10000 + sInfo->getCloseTime();  // 收盘时间
	}

	// **情况2：负时区偏移**（外盘模式）
	// 交易日推后，一般用于外盘交易
	if(sInfo->getOffsetMins() < 0)
	{
		// 这种情况比较简单，按自然日计算即可
		if (isStart)
			return (uint64_t)tDate * 10000 + sInfo->getOpenTime();  // 开盘时间在当前日期
		else
			// 收盘时间在下一个自然日
			return (uint64_t)TimeUtils::getNextDate(tDate) * 10000 + sInfo->getCloseTime();
	}
	else
	{
		// **情况3：正时区偏移**（夜盘模式）
		// 国内期货夜盘都是这种情况，夜盘算作下一个交易日
		// 这种情况比较复杂，主要是节假日后第一天的边界处理比较麻烦
		
		if(!isStart)
		{
			// 收盘时间相对简单，不需要特殊处理
			return (uint64_t)tDate * 10000 + sInfo->getCloseTime();
		}

		// 开盘时间的处理：
		// 核心思路：夜盘开始时间一定是上一个交易日的晚上
		// 所以需要获取上一个交易日，然后使用该日期的开盘时间
		tDate = getPrevTDate(tplid.c_str(), tDate, 1, isTpl);
		return (uint64_t)tDate * 10000 + sInfo->getOpenTime();
	}
}

/**
 * @brief 计算交易日
 * @param stdPID 标准品种ID或时段ID
 * @param uDate 自然日期（YYYYMMDD格式），0表示使用当前日期和时间
 * @param uTime 时间（HHMM格式），当uDate为0时会自动获取当前时间
 * @param isSession 是否直接使用时段ID，默认为false（使用品种ID）
 * @return uint32_t 对应的交易日（YYYYMMDD格式）
 * 
 * 该函数是交易日计算的核心算法，用于根据自然日期和时间确定对应的交易日。
 * 这在处理跨日交易（如夜盘）时特别重要，因为夜盘的交易时间可能跨越两个自然日，
 * 但在交易逻辑上属于同一个交易日。
 * 
 * 核心算法逻辑：
 * 
 * 1. **全天候交易**（7×24小时）：
 *    - 根据偏移时间判断是否跨日
 *    - 正偏移：当前时间>偏移时间时，交易日为下一日
 *    - 负偏移：当前时间<偏移时间时，交易日为前一日
 * 
 * 2. **正偏移**（夜盘模式，如国内期货）：
 *    - 当前时间>偏移时间：说明跨到了下一个交易日
 *    - 周末时间：自动跳转到下一个交易日
 *    - 示例：2015-10-16 23:00，偏移5小时，属于2015-10-19交易日
 * 
 * 3. **负偏移**（外盘模式）：
 *    - 当前时间<偏移时间：仍属于前一个交易日
 *    - 周末时间：跳转到下一个交易日
 *    - 示例：2015-10-17 01:00，偏移-5小时，属于2015-10-16交易日
 * 
 * 4. **无偏移**（标准模式）：
 *    - 周末时间：跳转到下一个交易日
 *    - 其他情况：交易日等于自然日
 * 
 * 应用场景：
 * - 数据归档：确定行情数据的归属交易日
 * - 订单处理：确定订单的交易日归属
 * - 结算系统：按正确的交易日进行结算
 * - 策略回测：正确处理跨日数据
 */
uint32_t WTSBaseDataMgr::calcTradingDate(const char* stdPID, uint32_t uDate, uint32_t uTime, bool isSession /* = false */)
{
	// 如果没有指定日期，获取当前日期和时间
	if (uDate == 0)
	{
		TimeUtils::getDateTime(uDate, uTime);  // 获取当前日期和时间
		uTime /= 100000;  // 转换时间格式为HHMM
	}

	// 初始化节假日模板ID和相关变量
	std::string tplid = stdPID;
	bool isTpl = false;
	WTSSessionInfo* sInfo = NULL;
	
	// 根据参数类型获取交易时段信息
	if(isSession)
	{
		// **直接时段模式**：stdPID是时段ID
		sInfo = getSession(stdPID);
		tplid = DEFAULT_HOLIDAY_TPL;  // 使用默认节假日模板
		isTpl = true;
	}
	else
	{
		// **品种模式**：stdPID是品种ID，需要通过品种获取时段信息
		WTSCommodityInfo* cInfo = getCommodity(stdPID);
		if (cInfo == NULL)
			return uDate;  // 品种不存在，返回原始日期
		
		sInfo = cInfo->getSessionInfo();
	}

	// 验证交易时段信息是否有效
	if (sInfo == NULL)
		return uDate;  // 时段信息无效，返回原始日期
	
	// 计算偏移时间点
	uint32_t offMin = sInfo->offsetTime(uTime, true);
	
	// **特殊情况：7×24小时全天候交易**
	uint32_t total_mins = sInfo->getTradingMins();
	if(total_mins == 1440)  // 1440分钟 = 24小时
	{
		if(sInfo->getOffsetMins() > 0 && uTime > offMin)
		{
			// 正偏移且当前时间超过偏移时间，交易日为下一日
			return TimeUtils::getNextDate(uDate, 1);
		}
		else if (sInfo->getOffsetMins() < 0 && uTime < offMin)
		{
			// 负偏移且当前时间小于偏移时间，交易日为前一日
			return TimeUtils::getNextDate(uDate, -1);
		}

		// 其他情况，交易日等于自然日
		return uDate;
	}

	// 获取当前日期是星期几
	uint32_t weekday = TimeUtils::getWeekDay(uDate);
	
	// **正偏移处理**（夜盘模式，如国内期货）
	if (sInfo->getOffsetMins() > 0)
	{
		// 如果当前时间大于偏移时间，说明跨到了下一个交易日
		if (uTime > offMin)
		{
			// 示例：2015-10-16 23:00，偏移300分钟（5小时），偏移时间为5:00
			// 由于23:00 > 5:00，所以交易日为下一个交易日
			return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
		}
		else if (weekday == 6 || weekday == 0)  // 周六或周日
		{
			// 示例：2015-10-17 01:00，周六，交易日为2015-10-19（下周一）
			return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
		}
	}
	// **负偏移处理**（外盘模式）
	else if (sInfo->getOffsetMins() < 0)
	{
		// 如果当前时间小于偏移时间，说明还属于前一个交易日
		if (uTime < offMin)
		{
			// 示例：2015-10-17 01:00，偏移-300分钟（-5小时），偏移时间为20:00
			// 由于01:00 < 20:00，所以交易日为前一个交易日
			return getPrevTDate(tplid.c_str(), uDate, 1, isTpl);
		}
		else if (weekday == 6 || weekday == 0)  // 周六或周日
		{
			// 因为是负偏移，如果在周末，则跳转到下一个交易日
			return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
		}
	}
	// **无偏移处理**（标准模式）
	else if (weekday == 6 || weekday == 0)  // 周六或周日
	{
		// 如果没有偏移且在周末，直接跳转到下一个交易日
		return getNextTDate(tplid.c_str(), uDate, 1, isTpl);
	}

	// 其他所有情况，交易日等于自然日
	return uDate;
}

/**
 * @brief 获取交易日
 * @param pid 品种ID或节假日模板ID
 * @param uOffDate 偏移日期（YYYYMMDD格式），0表示使用当前日期
 * @param uOffMinute 偏移分钟数（暂未使用）
 * @param isTpl 是否直接使用模板ID，默认为false（使用品种ID）
 * @return uint32_t 对应的交易日（YYYYMMDD格式）
 * 
 * 该函数用于获取当前或指定日期对应的交易日，主要用于缓存和快速查询。
 * 与calcTradingDate()的区别：
 * - getTradingDate()：简化版本，主要处理日期级别的交易日转换
 * - calcTradingDate()：完整版本，处理精确到分钟的交易日计算
 * 
 * 功能特点：
 * - 支持交易日缓存，避免重复计算
 * - 自动处理周末到下一个交易日的转换
 * - 支持品种ID和模板ID两种查询方式
 * 
 * 主要用途：
 * - 系统初始化时确定当前交易日
 * - 快速查询某个日期的交易日归属
 * - 为其他交易日计算函数提供基础支持
 */
uint32_t WTSBaseDataMgr::getTradingDate(const char* pid, uint32_t uOffDate /* = 0 */, uint32_t uOffMinute /* = 0 */, bool isTpl /* = false */)
{
	// 确定要使用的节假日模板ID
	const char* tplID = isTpl ? pid : getTplIDByPID(pid);

	// 获取当前日期作为默认值
	uint32_t curDate = TimeUtils::getCurDate();
	
	// 查找对应的交易日模板
	auto it = m_mapTradingDay.find(tplID);
	if (it == m_mapTradingDay.end())
	{
		// 如果没有找到模板，返回当前日期
		return curDate;
	}

	// 获取交易日模板对象
	TradingDayTpl* tpl = (TradingDayTpl*)&it->second;
	
	// 如果模板中已缓存了当前交易日，且没有指定偏移日期，直接返回缓存值
	if (tpl->_cur_tdate != 0 && uOffDate == 0)
		return tpl->_cur_tdate;

	// 如果没有指定偏移日期，使用当前日期
	if (uOffDate == 0)
		uOffDate = curDate;

	// 获取指定日期是星期几
	uint32_t weekday = TimeUtils::getWeekDay(uOffDate);

	// 如果是周末，需要跳转到下一个交易日
	if (weekday == 6 || weekday == 0)  // 周六或周日
	{
		// 获取下一个交易日并缓存到模板中
		tpl->_cur_tdate = getNextTDate(tplID, uOffDate, 1, true);
		uOffDate = tpl->_cur_tdate;
	}

	// 其他情况，交易日等于指定日期
	return uOffDate;
}

/**
 * @brief 获取下一个交易日
 * @param pid 品种ID或节假日模板ID
 * @param uDate 起始日期（YYYYMMDD格式）
 * @param days 向前推进的交易日天数，默认为1
 * @param isTpl 是否直接使用模板ID，默认为false（使用品种ID）
 * @return uint32_t 下一个交易日（YYYYMMDD格式）
 * 
 * 该函数用于计算指定日期之后的第N个交易日，是交易日历系统的核心算法之一。
 * 
 * 算法逻辑：
 * 1. 从起始日期开始，逐日向后推进
 * 2. 对每一天进行检查：
 *    - 跳过周末（周六、周日）
 *    - 跳过节假日（根据节假日模板判断）
 *    - 只有工作日且非节假日才计为有效交易日
 * 3. 找到指定数量的交易日后返回
 * 
 * 应用场景：
 * - 计算合约到期日后的下一个交易日
 * - 策略中计算N个交易日后的日期
 * - 数据处理中跳过非交易日
 * - 结算系统中计算交收日期
 * 
 * 注意：该函数会一直循环直到找到足够数量的交易日，
 * 因此对于大的days参数可能会有性能影响。
 */
uint32_t WTSBaseDataMgr::getNextTDate(const char* pid, uint32_t uDate, int days /* = 1 */, bool isTpl /* = false */)
{
	uint32_t curDate = uDate;  // 当前检查的日期
	int left = days;           // 剩余需要找到的交易日天数
	
	// 循环查找，直到找到足够数量的交易日
	while (true)
	{
		// 构建时间结构体，用于日期计算
		tm t;
		memset(&t, 0, sizeof(tm));
		t.tm_year = curDate / 10000 - 1900;        // 年份（相对于1900年）
		t.tm_mon = (curDate % 10000) / 100 - 1;    // 月份（0-11）
		t.tm_mday = curDate % 100;                 // 日期（1-31）
		
		// 转换为时间戳并加一天（86400秒）
		time_t ts = mktime(&t);
		ts += 86400;  // 加一天的秒数

		// 将时间戳转换回日期格式
		tm* newT = localtime(&ts);
		curDate = (newT->tm_year + 1900) * 10000 + (newT->tm_mon + 1) * 100 + newT->tm_mday;
		
		// 检查新日期是否为有效交易日
		// 条件：不是周末（周日=0，周六=6）且不是节假日
		if (newT->tm_wday != 0 && newT->tm_wday != 6 && !isHoliday(pid, curDate, isTpl))
		{
			// 找到一个有效交易日，剩余天数减1
			left--;
			if (left == 0)
				return curDate;  // 找到了足够数量的交易日，返回结果
		}
	}
}

/**
 * @brief 获取前一个交易日
 * @param pid 品种ID或节假日模板ID
 * @param uDate 起始日期（YYYYMMDD格式）
 * @param days 向后回退的交易日天数，默认为1
 * @param isTpl 是否直接使用模板ID，默认为false（使用品种ID）
 * @return uint32_t 前一个交易日（YYYYMMDD格式）
 * 
 * 该函数用于计算指定日期之前的第N个交易日，与getNextTDate()功能相反。
 * 
 * 算法逻辑：
 * 1. 从起始日期开始，逐日向前回退
 * 2. 对每一天进行检查：
 *    - 跳过周末（周六、周日）
 *    - 跳过节假日（根据节假日模板判断）
 *    - 只有工作日且非节假日才计为有效交易日
 * 3. 找到指定数量的交易日后返回
 * 
 * 应用场景：
 * - 计算夜盘开始时间的归属交易日
 * - 策略中计算N个交易日前的日期
 * - 数据分析中回溯历史交易日
 * - 风控系统中计算历史基准日期
 */
uint32_t WTSBaseDataMgr::getPrevTDate(const char* pid, uint32_t uDate, int days /* = 1 */, bool isTpl /* = false */)
{
	uint32_t curDate = uDate;  // 当前检查的日期
	int left = days;           // 剩余需要找到的交易日天数
	
	// 循环查找，直到找到足够数量的交易日
	while (true)
	{
		// 构建时间结构体，用于日期计算
		tm t;
		memset(&t, 0, sizeof(tm));
		t.tm_year = curDate / 10000 - 1900;        // 年份（相对于1900年）
		t.tm_mon = (curDate % 10000) / 100 - 1;    // 月份（0-11）
		t.tm_mday = curDate % 100;                 // 日期（1-31）
		
		// 转换为时间戳并减一天（86400秒）
		time_t ts = mktime(&t);
		ts -= 86400;  // 减一天的秒数

		// 将时间戳转换回日期格式
		tm* newT = localtime(&ts);
		curDate = (newT->tm_year + 1900) * 10000 + (newT->tm_mon + 1) * 100 + newT->tm_mday;
		
		// 检查新日期是否为有效交易日
		// 条件：不是周末（周日=0，周六=6）且不是节假日
		if (newT->tm_wday != 0 && newT->tm_wday != 6 && !isHoliday(pid, curDate, isTpl))
		{
			// 找到一个有效交易日，剩余天数减1
			left--;
			if (left == 0)
				return curDate;  // 找到了足够数量的交易日，返回结果
		}
	}
}

/**
 * @brief 判断指定日期是否为交易日
 * @param pid 品种ID或节假日模板ID
 * @param uDate 要检查的日期（YYYYMMDD格式）
 * @param isTpl 是否直接使用模板ID，默认为false（使用品种ID）
 * @return bool 是交易日返回true，否则返回false
 * 
 * 该函数用于快速判断指定日期是否为有效的交易日。
 * 交易日的判断条件：
 * 1. 不是周末（周六、周日）
 * 2. 不是节假日（根据节假日模板判断）
 * 
 * 该函数是isHoliday()的逆函数，提供了更直观的交易日判断接口。
 * 
 * 应用场景：
 * - 数据处理：过滤非交易日的数据
 * - 策略调度：确定策略运行日期
 * - 系统监控：检查系统运行状态
 * - 报表生成：按交易日统计数据
 */
bool WTSBaseDataMgr::isTradingDate(const char* pid, uint32_t uDate, bool isTpl /* = false */)
{
	// 获取指定日期是星期几
	uint32_t wd = TimeUtils::getWeekDay(uDate);
	
	// 检查是否为有效交易日：不是周末且不是节假日
	if (wd != 0 && wd != 6 && !isHoliday(pid, uDate, isTpl))
	{
		return true;  // 是有效交易日
	}

	// 是周末或节假日，不是交易日
	return false;
}

/**
 * @brief 设置当前交易日
 * @param pid 品种ID或节假日模板ID
 * @param uDate 要设置的交易日（YYYYMMDD格式）
 * @param isTpl 是否直接使用模板ID，默认为false（使用品种ID）
 * 
 * 该函数用于手动设置当前交易日，主要用于：
 * - 系统初始化时设置基准交易日
 * - 回测系统中设置模拟的当前交易日
 * - 数据修复时调整交易日缓存
 * 
 * 设置的交易日会被缓存在对应的交易日模板中，
 * 后续的getTradingDate()调用可以直接返回缓存值。
 */
void WTSBaseDataMgr::setTradingDate(const char* pid, uint32_t uDate, bool isTpl /* = false */)
{
	// 确定要使用的节假日模板ID
	std::string tplID = pid;
	if (!isTpl)
		tplID = getTplIDByPID(pid);

	// 查找对应的交易日模板
	auto it = m_mapTradingDay.find(tplID);
	if (it == m_mapTradingDay.end())
		return;  // 模板不存在，无法设置

	// 获取交易日模板对象并设置当前交易日
	TradingDayTpl* tpl = (TradingDayTpl*)&it->second;
	tpl->_cur_tdate = uDate;  // 缓存当前交易日
}

/**
 * @brief 获取指定时段的所有品种代码集合
 * @param sid 时段ID（如"DAY"、"NIGHT"等）
 * @return CodeSet* 品种代码集合指针，未找到返回NULL
 * 
 * 该函数返回使用指定交易时段的所有品种代码集合。
 * 这个映射关系在loadCommodities()过程中建立，
 * 用于快速查询某个时段下的所有相关品种。
 * 
 * 应用场景：
 * - 按时段批量处理品种数据
 * - 时段级别的监控和统计
 * - 策略中按时段分组处理
 * - 系统资源分配和调度
 */
CodeSet* WTSBaseDataMgr::getSessionComms(const char* sid)
{
	// 在时段代码映射表中查找指定的时段ID
	auto it = m_mapSessionCode.find(sid);
	if (it == m_mapSessionCode.end())
		return NULL;  // 时段不存在，返回NULL

	// 返回该时段对应的品种代码集合
	return (CodeSet*)&it->second;
}

/**
 * @brief 根据品种ID获取对应的节假日模板ID
 * @param pid 标准品种ID（格式：交易所.品种代码）
 * @return const char* 节假日模板ID，未找到返回默认模板ID
 * 
 * 该函数是一个重要的辅助函数，用于从标准品种ID中提取节假日模板信息。
 * 
 * 查询过程：
 * 1. 分割标准品种ID获取交易所和品种代码
 * 2. 查询对应的商品信息
 * 3. 从商品信息中获取节假日模板ID
 * 4. 如果获取失败，返回默认模板ID
 * 
 * 该函数确保了所有需要节假日模板的操作都能获得有效的模板ID。
 */
const char* WTSBaseDataMgr::getTplIDByPID(const char* pid)
{
	// 分割标准品种ID，格式："交易所.品种代码"
	const StringVector& ay = StrUtil::split(pid, ".");
	
	// 根据交易所和品种代码获取商品信息
	WTSCommodityInfo* commInfo = getCommodity(ay[0].c_str(), ay[1].c_str());
	if (commInfo == NULL)
		return DEFAULT_HOLIDAY_TPL;  // 商品不存在，返回默认节假日模板

	// 从商品信息中获取节假日模板ID
	return commInfo->getTradingTpl();
}