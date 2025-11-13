/*!
 * \file WTSHotMgr.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader主力合约管理器实现文件
 * 
 * 文件设计逻辑与核心作用总结：
 * ================================
 * 
 * 本文件是WonderTrader框架中主力合约管理系统的核心实现，负责管理期货市场中
 * 主力合约的切换规则和复权因子计算。在期货交易中，由于合约有到期日，投资者
 * 需要在旧合约到期前切换到新的主力合约，本系统提供了完整的主力切换管理方案。
 * 
 * 核心设计理念：
 * =============
 * 1. **多规则支持**：同时支持标准主力(HOT)、次主力(2ND)和自定义规则
 * 2. **时间序列管理**：基于时间序列的主力切换历史记录和查询
 * 3. **复权处理**：提供复权因子计算，确保切换前后数据的连续性
 * 4. **灵活配置**：支持通过配置文件加载各种主力切换规则
 * 5. **高效查询**：提供快速的主力合约查询和判断算法
 * 
 * 主要功能模块：
 * =============
 * 1. **配置加载模块**：
 *    - 加载主力合约配置文件(loadHots)
 *    - 加载次主力合约配置文件(loadSeconds)
 *    - 加载自定义规则配置文件(loadCustomRules)
 * 
 * 2. **主力查询模块**：
 *    - 获取当前主力合约代码(getRawCode)
 *    - 获取前期主力合约代码(getPrevRawCode)
 *    - 判断合约是否为主力(isHot/isSecond/isCustomHot)
 * 
 * 3. **历史分段模块**：
 *    - 分割主力历史时段(splitHotSecions/splitSecondSecions/splitCustomSections)
 *    - 支持按时间范围查询主力变化历史
 * 
 * 4. **复权计算模块**：
 *    - 获取规则标签(getRuleTag)
 *    - 计算复权因子(getRuleFactor)
 *    - 处理价格连续性问题
 * 
 * 数据结构设计：
 * =============
 * - WTSCustomSwitchMap: 规则标签 -> 品种映射的多层级结构
 * - WTSProductHotMap: 品种代码 -> 日期映射，管理单品种主力历史
 * - WTSDateHotMap: 日期 -> 切换项映射，按时间序列存储切换规则
 * - CustomSwitchCodes: 规则对应的当前主力代码集合，用于快速判断
 * 
 * 复权因子计算逻辑：
 * =================
 * 复权因子 = 旧合约收盘价 / 新合约收盘价
 * 累积复权因子 = 前期累积因子 × 当期复权因子
 * 用途：确保主力切换时价格数据的连续性，避免因切换造成的价格跳跃
 * 
 * 应用场景：
 * =========
 * - 量化策略回测：提供连续的主力合约数据
 * - 实时交易系统：识别当前主力合约进行交易
 * - 数据分析：提供主力切换历史分析
 * - 风控系统：基于主力合约进行风险控制
 * - 数据归档：按主力规则整理历史数据
 */
// 主力合约管理器头文件包含
#include "WTSHotMgr.h"              // 主力合约管理器类定义
#include "../WTSUtils/WTSCfgLoader.h"  // 配置文件加载工具

// WonderTrader核心数据结构头文件
#include "../Includes/WTSSwitchItem.hpp"  // 主力切换项数据结构
#include "../Includes/WTSVariant.hpp"     // 通用变量容器类

// 工具类头文件包含
#include "../Share/StrUtil.hpp"      // 字符串处理工具
#include "../Share/TimeUtils.hpp"    // 时间处理工具
#include "../Share/CodeHelper.hpp"   // 合约代码处理工具
#include "../Share/StdUtils.hpp"     // 标准工具类
#include "../Share/decimal.h"        // 高精度小数运算库


/**
 * @brief 主力合约管理器构造函数
 * 
 * 功能说明：
 * 初始化主力合约管理器的基本状态，设置所有成员变量的初始值。
 * 采用初始化列表的方式确保成员变量在对象创建时就被正确初始化。
 * 
 * 初始化内容：
 * - m_mapCustRules: 自定义规则映射表，初始化为NULL
 * - m_bInitialized: 初始化状态标志，初始化为false
 * 
 * 设计特点：
 * - 延迟初始化：实际的数据加载在loadHots等方法中进行
 * - 状态管理：通过m_bInitialized标志管理初始化状态
 * - 内存安全：指针初始化为NULL，避免野指针问题
 */
WTSHotMgr::WTSHotMgr()
	: m_mapCustRules(NULL)     // 初始化自定义规则映射表为空指针
	, m_bInitialized(false)   // 初始化状态标志为false，表示未初始化
{
	// 构造函数体为空，所有初始化工作通过初始化列表完成
}


/**
 * @brief 主力合约管理器析构函数
 * 
 * 功能说明：
 * 负责清理主力合约管理器占用的所有资源，确保内存安全。
 * 析构函数采用空实现，实际的资源清理通过release()方法进行。
 * 
 * 设计理念：
 * - 显式资源管理：通过release()方法显式释放资源
 * - 异常安全：析构函数不抛出异常
 * - RAII原则：资源获取即初始化，确保资源正确释放
 * 
 * 注意事项：
 * 如果对象使用完毕后没有调用release()，可能会导致内存泄漏。
 * 建议在不再使用对象时主动调用release()方法。
 */
WTSHotMgr::~WTSHotMgr()
{
	// 析构函数为空，资源清理由release()方法负责
	// 这种设计允许用户在需要时显式控制资源释放时机
}

/**
 * @brief 获取标准合约代码对应的规则标签
 * 
 * 功能说明：
 * 根据标准合约代码查找对应的主力规则标签。规则标签用于标识该合约
 * 使用哪种主力切换规则（如HOT主力、2ND次主力或自定义规则）。
 * 
 * @param stdCode 标准合约代码，格式：交易所.品种代码[+/-]，如"SHFE.AU+"
 * @return const char* 规则标签字符串，如果未找到返回空字符串
 * 
 * 算法逻辑：
 * 1. 检查自定义规则映射表是否已初始化
 * 2. 处理合约代码的后缀符号（+/-）
 * 3. 查找点号分隔符，区分完整代码和品种代码
 * 4. 在自定义规则中查找匹配的规则标签
 * 
 * 代码格式处理：
 * - 支持带+/-后缀的合约代码
 * - 支持交易所.品种格式的完整代码
 * - 支持纯品种代码格式
 * 
 * 使用场景：
 * - 确定合约使用的主力规则类型
 * - 为复权因子计算提供规则标识
 * - 支持多规则并存的复杂交易环境
 */
const char* WTSHotMgr::getRuleTag(const char* stdCode)
{
	// 检查自定义规则映射表是否已初始化，如果未初始化则返回空字符串
	if (m_mapCustRules == NULL)
		return "";

	// 获取标准合约代码的长度，用于后续的字符串处理
	auto len = strlen(stdCode);
	
	// 检查合约代码末尾是否有+/-后缀符号，如果有则排除这些符号
	// +表示看涨期权，-表示看跌期权，这些符号不影响规则查找
	if (stdCode[len - 1] == '+' || stdCode[len - 1] == '-')
		len--;  // 减少长度，排除后缀符号

	// 查找最后一个点号的位置，用于分离交易所代码和品种代码
	// 标准格式：交易所.品种，如"SHFE.AU"
	auto idx = StrUtil::findLast(stdCode, '.');
	
	// 构造完整品种代码用于查找
	std::string fullPid;
	if (idx == std::string::npos)
	{
		// 如果没有找到点号，说明是纯品种代码格式（如"AU"）
		// 这种情况下无法确定交易所，需要遍历所有规则查找
		// 但为了保持兼容性，先尝试直接查找（虽然通常不会成功）
		fullPid = std::string(stdCode, len);
	}
	else
	{
		// 如果找到了点号，提取完整品种代码（交易所.品种）
		fullPid = std::string(stdCode, len);
	}

	// 遍历所有规则标签，查找哪个规则包含该品种
	// m_mapCustRules 的键是规则标签（如"HOT"、"2ND"），值是品种映射表
	WTSCustomSwitchMap::ConstIterator it = m_mapCustRules->begin();
	for (; it != m_mapCustRules->end(); it++)
	{
		// 获取当前规则标签对应的品种映射表
		WTSProductHotMap* prodMap = STATIC_CONVERT(it->second, WTSProductHotMap*);
		if (prodMap == NULL)
			continue;

		// 在品种映射表中查找完整品种代码
		// WTSProductHotMap 的键是完整品种代码（如"SHFE.AU"）
		WTSDateHotMap* dtMap = STATIC_CONVERT(prodMap->get(fullPid.c_str()), WTSDateHotMap*);
		if (dtMap != NULL)
		{
			// 找到了匹配的品种，返回对应的规则标签
			// it->first 是规则标签（如"HOT"、"2ND"）
			return it->first.c_str();
		}

		// 如果没有找到完整品种代码，且输入是纯品种代码（无交易所前缀）
		// 尝试在所有品种中查找匹配的品种代码
		if (idx == std::string::npos)
		{
			// 遍历当前规则下的所有品种，查找品种代码匹配的项
			WTSProductHotMap::ConstIterator prodIt = prodMap->begin();
			for (; prodIt != prodMap->end(); prodIt++)
			{
				// prodIt->first 是完整品种代码（如"SHFE.AU"）
				// 提取品种代码部分（点号后的部分）
				auto dotPos = prodIt->first.find_last_of('.');
				if (dotPos != std::string::npos)
				{
					std::string pid = prodIt->first.substr(dotPos + 1);
					if (pid == fullPid)
					{
						// 找到匹配的品种代码，返回规则标签
						return it->first.c_str();
					}
				}
			}
		}
	}

	// 未找到对应规则，返回空字符串
	return "";
}

/**
 * @brief 获取指定规则和品种在特定日期的复权因子
 * 
 * 功能说明：
 * 根据规则标签、品种代码和查询日期，计算并返回对应的复权因子。复权因子用于
 * 处理主力合约切换时的价格连续性问题，确保历史数据的一致性。
 * 
 * @param ruleTag 规则标签，如"HOT"、"2ND"或自定义规则标签
 * @param fullPid 完整品种代码，格式：交易所.品种，如"SHFE.AU"
 * @param uDate 查询日期，格式YYYYMMDD，默认为0表示获取最新复权因子
 * @return double 复权因子值，1.0表示无需复权调整
 * 
 * 复权因子计算逻辑：
 * - 复权因子 = 累积调整系数，用于价格连续性处理
 * - 每次主力切换时，复权因子 *= (旧合约收盘价 / 新合约收盘价)
 * - 复权因子确保切换前后的价格数据保持连续性
 * 
 * 日期查找算法：
 * 1. uDate=0：返回最新的复权因子
 * 2. 精确匹配：如果查询日期正好是切换日期，返回该切换的复权因子
 * 3. 区间查找：如果查询日期在两个切换日期之间，返回前一个切换的复权因子
 * 4. 边界处理：如果查询日期早于所有切换日期，返回1.0（无复权）
 * 
 * 使用场景：
 * - 历史数据复权处理
 * - 连续合约价格计算
 * - 策略回测数据调整
 * - 风控系统价格标准化
 */
double WTSHotMgr::getRuleFactor(const char* ruleTag, const char* fullPid, uint32_t uDate /* = 0 */ )
{
	// 检查自定义规则映射表是否已初始化，未初始化返回默认复权因子1.0
	if (m_mapCustRules == NULL)
		return 1.0;

	// 根据规则标签获取对应的品种映射表
	WTSProductHotMap* prodMap = (WTSProductHotMap*)m_mapCustRules->get(ruleTag);
	if (prodMap == NULL)
		return 1.0;  // 规则不存在，返回默认复权因子

	// 根据完整品种代码获取对应的日期映射表
	WTSDateHotMap* dtMap = STATIC_CONVERT(prodMap->get(fullPid), WTSDateHotMap*);
	if (dtMap == NULL)
		return 1.0;  // 品种不存在，返回默认复权因子

	// 如果查询日期为0，返回最新的复权因子（最后一次切换的复权因子）
	if(uDate == 0)
	{
		// 获取日期映射表中的最后一个切换项（rbegin()返回最大日期的切换项）
		WTSSwitchItem* pItem = STATIC_CONVERT(dtMap->rbegin()->second, WTSSwitchItem*);
		return pItem->get_factor();  // 返回最新的复权因子
	}

	// 使用lower_bound查找第一个大于等于查询日期的切换项
	// lower_bound返回第一个不小于uDate的元素位置
	auto it = dtMap->lower_bound(uDate);
	
	if(it == dtMap->end())
	{
		// 如果找不到，说明查询日期大于所有记录的切换日期
		// 返回最后一条记录的复权因子（最新的复权因子）
		WTSSwitchItem* pItem = STATIC_CONVERT(dtMap->rbegin()->second, WTSSwitchItem*);
		return pItem->get_factor();
	}
	else
	{
		// 找到了大于等于查询日期的切换项，需要进一步判断
		WTSSwitchItem* pItem = STATIC_CONVERT(it->second, WTSSwitchItem*);
		
		// 检查切换日期是否等于查询日期
		if (pItem->switch_date() == uDate)
		{
			// 如果相等，说明查询日期正好是切换日期，直接返回该切换的复权因子
			return pItem->get_factor();
		}
		else
		{
			// 如果切换日期大于查询日期，说明查询日期在前一个时段
			// 需要返回前一个时段的复权因子
			
			if (it == dtMap->begin())
			{
				// 如果已经是第一个切换项，说明查询日期早于所有切换日期
				// 返回1.0，表示无需复权调整
				return 1.0;
			}
			else
			{
				// 不是第一个切换项，回退到前一个切换项
				it--;  // 迭代器向前移动一位
				WTSSwitchItem* pItem = STATIC_CONVERT(it->second, WTSSwitchItem*);
				return pItem->get_factor();  // 返回前一个时段的复权因子
			}
		}
	}
}

#pragma region "主力接口"
/**
 * @brief 加载主力合约配置文件
 * 
 * 功能说明：
 * 加载标准主力合约的切换规则配置文件，并设置管理器的初始化状态。
 * 主力合约通常是指成交量和持仓量最大的合约，是市场交易的主要标的。
 * 
 * @param filename 主力配置文件路径，通常为JSON格式的配置文件
 * @return bool 加载成功返回true，失败返回false
 * 
 * 实现逻辑：
 * 1. 调用loadCustomRules方法，使用"HOT"标签加载配置
 * 2. 设置管理器初始化状态为true
 * 3. 返回加载结果
 * 
 * 配置文件格式：
 * {
 *   "交易所": {
 *     "品种": [
 *       {
 *         "date": 切换日期,
 *         "from": "旧主力合约",
 *         "to": "新主力合约",
 *         "oldclose": 旧合约收盘价,
 *         "newclose": 新合约收盘价
 *       }
 *     ]
 *   }
 * }
 * 
 * 使用场景：
 * - 系统启动时加载主力规则
 * - 策略回测前准备主力数据
 * - 实时交易系统初始化
 */
bool WTSHotMgr::loadHots(const char* filename)
{
	// 调用通用的自定义规则加载方法，使用"HOT"作为规则标签
	loadCustomRules("HOT", filename);
	
	// 设置管理器初始化状态为true，表示已成功加载配置
	m_bInitialized = true;
	
	// 返回加载成功标志（注意：实际成功与否应该检查loadCustomRules的返回值）
	return true;
}

/**
 * @brief 获取上一期主力合约的原始代码
 * 
 * 功能说明：
 * 获取指定交易所、品种在指定日期的前一期主力合约代码。主要用于主力切换
 * 分析、历史数据处理和策略回测中的主力合约追踪。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param pid 品种代码，如"AU"、"IF"、"RB"等
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return const char* 上一期主力合约的原始代码，如"AU2406"
 * 
 * 实现逻辑：
 * 1. 构造完整品种代码（交易所.品种格式）
 * 2. 调用通用的getPrevCustomRawCode方法
 * 3. 使用"HOT"标签查找前一期主力合约
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 每个线程都有自己独立的缓冲区，避免线程间的数据竞争。
 * 
 * 使用场景：
 * - 主力切换分析：比较新旧主力合约的差异
 * - 历史数据处理：获取历史时点的主力信息
 * - 策略回测：模拟主力切换过程
 * - 数据验证：验证主力切换的连续性
 */
const char* WTSHotMgr::getPrevRawCode(const char* exchg, const char* pid, uint32_t dt)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullPid[64] = { 0 };
	
	// 格式化生成完整品种代码："交易所.品种"
	fmtutil::format_to(fullPid, "{}.{}", exchg, pid);

	// 调用通用的自定义规则查询方法，使用"HOT"标签获取前一期主力合约
	return getPrevCustomRawCode("HOT", fullPid, dt);
}

/**
 * @brief 获取主力合约的原始代码
 * 
 * 功能说明：
 * 获取指定交易所、品种在指定日期的当前主力合约代码。这是最常用的主力查询
 * 接口，用于确定在特定时点应该交易哪个合约。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param pid 品种代码，如"AU"、"IF"、"RB"等  
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return const char* 主力合约的原始代码，如"AU2409"
 * 
 * 实现逻辑：
 * 1. 构造完整品种代码（交易所.品种格式）
 * 2. 调用通用的getCustomRawCode方法
 * 3. 使用"HOT"标签查找当前主力合约
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 主力合约确定逻辑：
 * - 根据配置的切换规则和日期确定主力合约
 * - 如果查询日期在切换日期之前，返回旧主力合约
 * - 如果查询日期在切换日期之后，返回新主力合约
 * - 如果查询日期正好是切换日期，返回新主力合约
 * 
 * 使用场景：
 * - 实时交易：确定当前应该交易的主力合约
 * - 行情订阅：订阅主力合约的实时行情
 * - 策略执行：基于主力合约进行交易决策
 * - 数据分析：获取历史各时点的主力合约
 */
const char* WTSHotMgr::getRawCode(const char* exchg, const char* pid, uint32_t dt)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullPid[64] = { 0 };
	
	// 格式化生成完整品种代码："交易所.品种"
	fmtutil::format_to(fullPid, "{}.{}", exchg, pid);

	// 调用通用的自定义规则查询方法，使用"HOT"标签获取当前主力合约
	return getCustomRawCode("HOT", fullPid, dt);
}

/**
 * @brief 判断合约是否为主力合约
 * 
 * 功能说明：
 * 判断指定交易所的某个原始合约在指定日期是否为主力合约。这是一个重要的
 * 判断接口，用于验证合约的主力地位。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param rawCode 原始合约代码，如"AU2409"、"IF2406"等
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return bool 是主力合约返回true，否则返回false
 * 
 * 实现逻辑：
 * 1. 构造完整合约代码（交易所.合约格式）
 * 2. 调用通用的isCustomHot方法
 * 3. 使用"HOT"标签判断是否为主力合约
 * 
 * 判断算法：
 * - 查找指定日期的主力切换规则
 * - 比较给定合约代码与规则中的主力合约代码
 * - 考虑切换日期的边界条件
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 使用场景：
 * - 合约筛选：从多个合约中筛选出主力合约
 * - 交易验证：验证交易的合约是否为主力
 * - 数据分析：分析主力合约的历史变化
 * - 风控系统：基于主力地位进行风险控制
 */
bool WTSHotMgr::isHot(const char* exchg, const char* rawCode, uint32_t dt)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullCode[64] = { 0 };
	
	// 格式化生成完整合约代码："交易所.合约"
	fmtutil::format_to(fullCode, "{}.{}", exchg, rawCode);

	// 调用通用的自定义规则判断方法，使用"HOT"标签判断是否为主力
	return isCustomHot("HOT", fullCode, dt);
}

/**
 * @brief 分割主力合约历史时段
 * 
 * 功能说明：
 * 将指定时间范围内的主力合约历史按切换时点分割成多个时段，每个时段对应一个主力合约。
 * 这个功能对于历史数据分析、策略回测和主力切换研究非常重要。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param pid 品种代码，如"AU"、"IF"、"RB"等
 * @param sDt 开始日期，格式YYYYMMDD
 * @param eDt 结束日期，格式YYYYMMDD
 * @param sections 输出参数，主力历史时段列表，每个时段包含合约代码、起止时间和复权因子
 * @return bool 分割成功返回true，失败返回false
 * 
 * 实现逻辑：
 * 1. 构造完整品种代码（交易所.品种格式）
 * 2. 调用通用的splitCustomSections方法
 * 3. 使用"HOT"标签分割主力历史时段
 * 
 * 时段分割原理：
 * - 根据主力切换日期将时间范围分割成连续的时段
 * - 每个时段对应一个主力合约，包含该合约的有效期间
 * - 每个时段还包含对应的复权因子，用于价格调整
 * 
 * 输出格式：
 * sections[i] = {
 *   contractCode: "AU2406",     // 该时段的主力合约代码
 *   startDate: 20240301,       // 时段开始日期
 *   endDate: 20240327,         // 时段结束日期
 *   factor: 1.05               // 复权因子
 * }
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 使用场景：
 * - 策略回测：按主力时段分别处理历史数据
 * - 数据分析：分析不同主力合约的表现
 * - 连续合约构建：为连续合约数据提供时段信息
 * - 主力切换研究：研究主力切换的规律和影响
 */
bool WTSHotMgr::splitHotSecions(const char* exchg, const char* pid, uint32_t sDt, uint32_t eDt, HotSections& sections)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullPid[64] = { 0 };
	
	// 格式化生成完整品种代码："交易所.品种"
	fmtutil::format_to(fullPid, "{}.{}", exchg, pid);

	// 调用通用的自定义规则时段分割方法，使用"HOT"标签分割主力历史时段
	return splitCustomSections("HOT", fullPid, sDt, eDt, sections);
}
#pragma endregion "主力接口"

#pragma region "次主力接口"
/**
 * @brief 加载次主力合约配置文件
 * 
 * 功能说明：
 * 加载次主力合约的切换规则配置文件。次主力合约通常是指成交量和持仓量排第二的合约，
 * 在主力合约流动性不足或者需要分散风险时使用。
 * 
 * @param filename 次主力配置文件路径，通常为JSON格式的配置文件
 * @return bool 加载成功返回true，失败返回false
 * 
 * 实现逻辑：
 * 调用通用的loadCustomRules方法，使用"2ND"作为规则标签加载次主力配置。
 * 
 * 次主力合约特点：
 * - 流动性：次于主力合约，但仍有一定的交易活跃度
 * - 风险分散：可以作为主力合约的补充或替代
 * - 套利机会：与主力合约之间可能存在价差套利机会
 * - 切换逻辑：通常跟随主力合约的切换模式
 * 
 * 配置文件格式：
 * 与主力合约配置格式相同，包含切换日期、新旧合约代码、收盘价等信息。
 * 
 * 使用场景：
 * - 多合约策略：同时关注主力和次主力合约的策略
 * - 流动性管理：在主力合约流动性不足时使用次主力
 * - 套利交易：利用主力和次主力之间的价差进行套利
 * - 风险对冲：使用次主力合约进行风险对冲
 */
bool WTSHotMgr::loadSeconds(const char* filename)
{
	// 调用通用的自定义规则加载方法，使用"2ND"作为次主力规则标签
	return loadCustomRules("2ND", filename);
}

/**
 * @brief 获取上一期次主力合约的原始代码
 * 
 * 功能说明：
 * 获取指定交易所、品种在指定日期的前一期次主力合约代码。主要用于次主力合约的
 * 历史追踪、切换分析和策略回测中的次主力合约管理。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param pid 品种代码，如"AU"、"IF"、"RB"等
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return const char* 上一期次主力合约的原始代码，如"AU2403"
 * 
 * 实现逻辑：
 * 1. 构造完整品种代码（交易所.品种格式）
 * 2. 调用通用的getPrevCustomRawCode方法
 * 3. 使用"2ND"标签查找前一期次主力合约
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 使用场景：
 * - 次主力切换分析：比较新旧次主力合约的差异
 * - 历史数据处理：获取历史时点的次主力信息
 * - 套利策略：分析次主力合约间的套利机会
 * - 数据验证：验证次主力切换的连续性
 */
const char* WTSHotMgr::getPrevSecondRawCode(const char* exchg, const char* pid, uint32_t dt)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullPid[64] = { 0 };
	
	// 格式化生成完整品种代码："交易所.品种"
	fmtutil::format_to(fullPid, "{}.{}", exchg, pid);

	// 调用通用的自定义规则查询方法，使用"2ND"标签获取前一期次主力合约
	return getPrevCustomRawCode("2ND", fullPid, dt);
}

/**
 * @brief 获取次主力合约的原始代码
 * 
 * 功能说明：
 * 获取指定交易所、品种在指定日期的当前次主力合约代码。次主力合约是成交量和持仓量
 * 排第二的合约，在多合约策略和风险分散中发挥重要作用。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param pid 品种代码，如"AU"、"IF"、"RB"等
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return const char* 次主力合约的原始代码，如"AU2406"
 * 
 * 实现逻辑：
 * 1. 构造完整品种代码（交易所.品种格式）
 * 2. 调用通用的getCustomRawCode方法
 * 3. 使用"2ND"标签查找当前次主力合约
 * 
 * 次主力确定逻辑：
 * - 根据配置的次主力切换规则和日期确定次主力合约
 * - 如果查询日期在切换日期之前，返回旧次主力合约
 * - 如果查询日期在切换日期之后，返回新次主力合约
 * - 如果查询日期正好是切换日期，返回新次主力合约
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 使用场景：
 * - 多合约策略：同时交易主力和次主力合约的策略
 * - 流动性管理：当主力合约流动性不足时使用次主力
 * - 套利交易：利用主力和次主力之间的价差
 * - 风险分散：通过次主力合约分散交易风险
 */
const char* WTSHotMgr::getSecondRawCode(const char* exchg, const char* pid, uint32_t dt)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullPid[64] = { 0 };
	
	// 格式化生成完整品种代码："交易所.品种"
	fmtutil::format_to(fullPid, "{}.{}", exchg, pid);

	// 调用通用的自定义规则查询方法，使用"2ND"标签获取当前次主力合约
	return getCustomRawCode("2ND", fullPid, dt);
}

/**
 * @brief 判断合约是否为次主力合约
 * 
 * 功能说明：
 * 判断指定交易所的某个原始合约在指定日期是否为次主力合约。次主力合约是成交量和持仓量
 * 排第二的合约，在主力合约流动性不足或需要分散风险时使用。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param rawCode 原始合约代码，如"AU2409"、"IF2406"等
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return bool 是次主力合约返回true，否则返回false
 * 
 * 实现逻辑：
 * 1. 构造完整合约代码（交易所.合约格式）
 * 2. 调用通用的isCustomHot方法
 * 3. 使用"2NDT"标签判断是否为次主力合约（注意：这里应该是"2ND"，代码中的"2NDT"可能是笔误）
 * 
 * 判断算法：
 * - 查找指定日期的次主力切换规则
 * - 比较给定合约代码与规则中的次主力合约代码
 * - 考虑切换日期的边界条件
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 使用场景：
 * - 合约筛选：从多个合约中筛选出次主力合约
 * - 交易验证：验证交易的合约是否为次主力
 * - 套利分析：分析次主力合约的历史变化
 * - 多合约策略：基于次主力地位进行策略决策
 */
bool WTSHotMgr::isSecond(const char* exchg, const char* rawCode, uint32_t dt)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullCode[64] = { 0 };
	
	// 格式化生成完整合约代码："交易所.合约"
	fmtutil::format_to(fullCode, "{}.{}", exchg, rawCode);

	// 调用通用的自定义规则判断方法，使用"2NDT"标签判断是否为次主力
	// 注意：这里的标签应该是"2ND"，"2NDT"可能是代码中的笔误
	return isCustomHot("2NDT", fullCode, dt);
}

/**
 * @brief 分割次主力合约历史时段
 * 
 * 功能说明：
 * 将指定时间范围内的次主力合约历史按切换时点分割成多个时段，每个时段对应一个次主力合约。
 * 这个功能对于次主力合约的历史数据分析、套利策略回测和多合约管理非常重要。
 * 
 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
 * @param pid 品种代码，如"AU"、"IF"、"RB"等
 * @param sDt 开始日期，格式YYYYMMDD
 * @param eDt 结束日期，格式YYYYMMDD
 * @param sections 输出参数，次主力历史时段列表，每个时段包含合约代码、起止时间和复权因子
 * @return bool 分割成功返回true，失败返回false
 * 
 * 实现逻辑：
 * 1. 构造完整品种代码（交易所.品种格式）
 * 2. 调用通用的splitCustomSections方法
 * 3. 使用"2ND"标签分割次主力历史时段
 * 
 * 时段分割原理：
 * - 根据次主力切换日期将时间范围分割成连续的时段
 * - 每个时段对应一个次主力合约，包含该合约的有效期间
 * - 每个时段还包含对应的复权因子，用于价格调整
 * 
 * 输出格式：
 * sections[i] = {
 *   contractCode: "AU2403",     // 该时段的次主力合约代码
 *   startDate: 20240301,       // 时段开始日期
 *   endDate: 20240327,         // 时段结束日期
 *   factor: 1.03               // 复权因子
 * }
 * 
 * 线程安全性：
 * 使用thread_local存储临时字符串缓冲区，确保多线程环境下的安全性。
 * 
 * 使用场景：
 * - 套利策略：按次主力时段分别处理历史数据进行套利分析
 * - 多合约策略：分析不同次主力合约的表现差异
 * - 流动性研究：研究次主力合约的流动性变化规律
 * - 风险分散：基于次主力时段进行风险分散策略设计
 */
bool WTSHotMgr::splitSecondSecions(const char* exchg, const char* pid, uint32_t sDt, uint32_t eDt, HotSections& sections)
{
	// 使用线程局部存储的字符缓冲区，确保多线程安全
	static thread_local char fullPid[64] = { 0 };
	
	// 格式化生成完整品种代码："交易所.品种"
	fmtutil::format_to(fullPid, "{}.{}", exchg, pid);

	// 调用通用的自定义规则时段分割方法，使用"2ND"标签分割次主力历史时段
	return splitCustomSections("2ND", fullPid, sDt, eDt, sections);
}

#pragma endregion "次主力接口"

#pragma region "自定义主力接口"
/**
 * @brief 加载自定义主力规则配置文件
 * 
 * 功能说明：
 * 从配置文件中加载自定义的主力切换规则，支持用户定义个性化的主力判断标准。
 * 这是一个通用的配置加载方法，可以支持多种不同的自定义规则标签。
 * 
 * @param tag 规则标签，用于标识不同的自定义规则，如"HOT"、"2ND"或用户自定义标签
 * @param filename 配置文件路径，通常为JSON格式的配置文件
 * @return bool 加载成功返回true，失败返回false
 * 
 * 配置文件格式：
 * {
 *   "交易所代码": {
 *     "品种代码": [
 *       {
 *         "date": 切换日期(YYYYMMDD),
 *         "from": "旧主力合约代码",
 *         "to": "新主力合约代码",
 *         "oldclose": 旧合约收盘价,
 *         "newclose": 新合约收盘价
 *       }
 *     ]
 *   }
 * }
 * 
 * 实现逻辑：
 * 1. 检查配置文件是否存在
 * 2. 使用WTSCfgLoader加载JSON配置文件
 * 3. 初始化自定义规则映射表（如果尚未初始化）
 * 4. 为指定规则标签创建品种映射表
 * 5. 遍历配置文件中的所有交易所和品种
 * 6. 为每个品种创建日期映射表
 * 7. 解析每个切换项并计算复权因子
 * 8. 将切换项添加到相应的映射表中
 * 9. 维护当前主力代码集合用于快速判断
 * 
 * 复权因子计算：
 * - 复权因子 *= (旧合约收盘价 / 新合约收盘价)
 * - 累积复权因子确保价格数据的连续性
 * - 如果旧合约收盘价为0，则复权因子保持不变
 * 
 * 数据结构组织：
 * - 三层映射结构：规则标签 -> 品种 -> 日期 -> 切换项
 * - 快速查找集合：规则标签 -> 当前主力合约代码集合
 * 
 * 使用场景：
 * - 加载标准主力规则（HOT标签）
 * - 加载次主力规则（2ND标签）
 * - 加载用户自定义的主力判断规则
 * - 支持多种主力规则的并存和切换
 */
bool WTSHotMgr::loadCustomRules(const char* tag, const char* filename)
{
	// 检查配置文件是否存在，如果不存在则返回加载失败
	if (!StdFile::exists(filename))
	{
		return false;
	}

	// 使用配置加载器从文件中加载配置，返回根节点变体对象
	WTSVariant* root = WTSCfgLoader::load_from_file(filename);
	if (root == NULL)
		return false;  // 配置文件加载失败，返回false

	// 如果自定义规则映射表尚未初始化，则创建新的映射表
	if (m_mapCustRules == NULL)
		m_mapCustRules = WTSCustomSwitchMap::create();

	// 获取指定规则标签对应的品种映射表，如果不存在则创建新的
	WTSProductHotMap* prodMap = (WTSProductHotMap*)m_mapCustRules->get(tag);
	if(prodMap == NULL)
	{
		// 创建新的品种映射表并添加到规则映射中
		prodMap = WTSProductHotMap::create();
		m_mapCustRules->add(tag, prodMap, false);  // false表示不自动释放
	}

	// 遍历配置文件中的所有交易所
	for (const std::string& exchg : root->memberNames())
	{
		// 获取当前交易所的配置节点
		WTSVariant* jExchg = root->get(exchg);

		// 遍历当前交易所下的所有品种
		for (const std::string& pid : jExchg->memberNames())
		{
			// 获取当前品种的配置节点
			WTSVariant* jProduct = jExchg->get(pid);
			
			// 构造完整品种代码："交易所.品种"
			std::string fullPid = fmt::format("{}.{}", exchg, pid);

			// 为当前品种创建日期映射表
			WTSDateHotMap* dateMap = WTSDateHotMap::create();
			prodMap->add(fullPid.c_str(), dateMap, false);  // 添加到品种映射中

			// 用于记录最后一个合约代码和累积复权因子
			std::string lastCode;
			double factor = 1.0;  // 初始复权因子为1.0
			
			// 遍历当前品种的所有切换项配置
			for (uint32_t i = 0; i < jProduct->size(); i++)
			{
				// 获取当前切换项的配置
				WTSVariant* jHotItem = jProduct->get(i);
				
				// 创建主力切换项对象
				WTSSwitchItem* pItem = WTSSwitchItem::create(
					exchg.c_str(), pid.c_str(),                    // 交易所和品种代码
					jHotItem->getCString("from"),                  // 旧主力合约代码
					jHotItem->getCString("to"),                    // 新主力合约代码
					jHotItem->getUInt32("date"));                  // 切换日期

				// 计算复权因子：基于旧合约和新合约的收盘价
				double oldclose = jHotItem->getDouble("oldclose");  // 旧合约收盘价
				double newclose = jHotItem->getDouble("newclose");  // 新合约收盘价
				
				// 累积复权因子计算：factor *= (oldclose / newclose)
				// 如果旧合约收盘价为0，则复权因子保持不变（乘以1.0）
				factor *= (decimal::eq(oldclose, 0.0) ? 1.0 : (oldclose/ newclose));
				
				// 设置切换项的复权因子
				pItem->set_factor(factor);
				
				// 将切换项添加到日期映射表中，以切换日期为键
				dateMap->add(pItem->switch_date(), pItem, false);
				
				// 记录最后一个合约代码，用于构建当前主力代码集合
				lastCode = jHotItem->getCString("to");
			}

			// 构造完整合约代码："交易所.合约代码"
			std::string fullCode = fmt::format("{}.{}", exchg.c_str(), lastCode.c_str());
			
			// 将最新的主力合约代码添加到快速查找集合中
			// 这个集合用于快速判断某个合约是否为当前主力
			m_mapCustCodes[tag].insert(fullCode);
		}
	}

	// 释放配置文件根节点对象的内存
	root->release();
	
	// 返回加载成功标志
	return true;
}

/**
 * @brief 获取自定义规则的前一期主力合约代码
 * 
 * 功能说明：
 * 根据自定义规则标签、完整品种代码和查询日期，获取前一期的主力合约代码。
 * 这个方法主要用于分析主力合约的历史切换情况，支持策略回测和数据分析。
 * 
 * @param tag 规则标签，如"HOT"、"2ND"或用户自定义标签
 * @param fullPid 完整品种代码，格式：交易所.品种，如"SHFE.AU"
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return const char* 前一期主力合约的原始代码，如"AU2403"，未找到返回空字符串
 * 
 * 算法逻辑：
 * 1. 参数验证：检查自定义规则映射表是否已初始化
 * 2. 日期处理：如果查询日期为0，则使用当前日期
 * 3. 规则查找：根据规则标签获取对应的品种映射表
 * 4. 品种查找：根据品种代码获取对应的日期映射表
 * 5. 时间定位：使用lower_bound查找第一个大于等于查询日期的切换项
 * 6. 前期查找：根据查找结果定位到前一期的主力合约
 * 
 * 查找策略：
 * - 如果找到大于等于查询日期的切换项：
 *   - 如果查询日期小于切换日期，向前回退一位
 *   - 如果已经是第一个或最后一个切换项，返回空字符串
 *   - 否则再向前回退一位，获取前一期主力
 * - 如果没找到（查询日期大于所有切换日期）：
 *   - 从最后一个切换项开始向前回退两位
 *   - 获取前一期主力合约代码
 * 
 * 边界处理：
 * - 查询日期早于所有切换日期：返回空字符串
 * - 只有一个切换项：返回空字符串
 * - 规则或品种不存在：返回空字符串
 * 
 * 使用场景：
 * - 主力切换分析：比较前后期主力合约的差异
 * - 策略回测：获取历史时点的前期主力信息
 * - 数据验证：验证主力切换的连续性和合理性
 * - 套利分析：分析前后期主力合约的价差变化
 */
const char* WTSHotMgr::getPrevCustomRawCode(const char* tag, const char* fullPid, uint32_t dt /* = 0 */)
{
	// 检查自定义规则映射表是否已初始化
	if (m_mapCustRules == NULL)
		return "";  // 未初始化，返回空字符串

	// 如果查询日期为0，则使用当前日期
	if (dt == 0)
		dt = TimeUtils::getCurDate();

	// 再次检查自定义规则映射表（防御性编程，实际上这个检查是冗余的）
	if (m_mapCustRules == NULL)
		return "";

	// 根据规则标签获取对应的品种映射表
	WTSProductHotMap* prodMap = (WTSProductHotMap*)m_mapCustRules->get(tag);
	if (prodMap == NULL)
		return "";  // 规则不存在，返回空字符串

	// 根据完整品种代码获取对应的日期映射表
	WTSDateHotMap* dtMap = STATIC_CONVERT(prodMap->get(fullPid), WTSDateHotMap*);
	if (dtMap == NULL)
		return "";  // 品种不存在，返回空字符串

	// 使用lower_bound查找第一个大于等于查询日期的切换项
	WTSDateHotMap::ConstIterator cit = dtMap->lower_bound(dt);
	
	if (cit != dtMap->end())
	{
		// 找到了大于等于查询日期的切换项
		
		// 如果查询日期小于找到的切换日期，需要向前回退一位
		if (dt < cit->first)
			cit--;

		// 检查是否已经到达边界（第一个或最后一个位置）
		if (cit == dtMap->end() || cit == dtMap->begin())
			return "";  // 无法获取前一期，返回空字符串

		// 向前回退一位，获取前一期的切换项
		cit--;

		// 获取前一期切换项并返回其目标合约代码
		WTSSwitchItem* pItem = STATIC_CONVERT(cit->second, WTSSwitchItem*);
		return pItem->to();  // 返回前一期主力合约代码
	}
	else
	{
		// 没找到大于等于查询日期的切换项，说明查询日期大于所有切换日期
		
		// 从最后一个位置开始向前回退
		cit--;

		// 检查是否已经到达边界
		if (cit == dtMap->end() || cit == dtMap->begin())
			return "";  // 无法获取前一期，返回空字符串

		// 再向前回退一位，获取前一期的切换项
		cit--;

		// 获取前一期切换项并返回其目标合约代码
		WTSSwitchItem* pItem = STATIC_CONVERT(cit->second, WTSSwitchItem*);
		return pItem->to();  // 返回前一期主力合约代码
	}

	// 理论上不会执行到这里，但为了代码完整性保留
	return "";
}

/**
 * @brief 获取自定义规则的当前主力合约代码
 * 
 * 功能说明：
 * 根据自定义规则标签、完整品种代码和查询日期，获取当前生效的主力合约代码。
 * 这是自定义主力查询的核心方法，支持任意自定义规则的主力合约查找。
 * 
 * @param tag 规则标签，如"HOT"、"2ND"或用户自定义标签
 * @param fullPid 完整品种代码，格式：交易所.品种，如"SHFE.AU"
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return const char* 当前主力合约的原始代码，如"AU2409"，未找到返回空字符串
 * 
 * 算法逻辑：
 * 1. 参数验证：检查自定义规则映射表是否已初始化
 * 2. 日期处理：如果查询日期为0，则使用当前日期
 * 3. 规则查找：根据规则标签获取对应的品种映射表
 * 4. 品种查找：根据品种代码获取对应的日期映射表
 * 5. 时间定位：使用lower_bound查找第一个大于等于查询日期的切换项
 * 6. 主力确定：根据查找结果确定当前生效的主力合约
 * 
 * 查找策略：
 * - 如果找到大于等于查询日期的切换项：
 *   - 如果查询日期小于切换日期，向前回退一位（使用前一个切换项）
 *   - 如果回退后到达末尾，返回空字符串
 *   - 否则返回该切换项的目标合约代码
 * - 如果没找到（查询日期大于所有切换日期）：
 *   - 返回最后一个切换项的目标合约代码
 * 
 * 时间边界处理：
 * - 查询日期等于切换日期：返回新主力合约
 * - 查询日期在两个切换日期之间：返回前一个切换的主力合约
 * - 查询日期早于所有切换日期：返回空字符串
 * - 查询日期晚于所有切换日期：返回最新主力合约
 * 
 * 使用场景：
 * - 实时交易：确定当前应该交易的主力合约
 * - 策略执行：基于自定义规则进行交易决策
 * - 数据分析：获取历史各时点的主力合约
 * - 回测系统：模拟历史时点的主力合约选择
 */
const char* WTSHotMgr::getCustomRawCode(const char* tag, const char* fullPid, uint32_t dt /* = 0 */)
{
	// 检查自定义规则映射表是否已初始化
	if (m_mapCustRules == NULL)
		return "";  // 未初始化，返回空字符串

	// 如果查询日期为0，则使用当前日期
	if (dt == 0)
		dt = TimeUtils::getCurDate();

	// 根据规则标签获取对应的品种映射表
	WTSProductHotMap* prodMap = (WTSProductHotMap*)m_mapCustRules->get(tag);
	if (prodMap == NULL)
		return "";  // 规则不存在，返回空字符串

	// 根据完整品种代码获取对应的日期映射表
	WTSDateHotMap* dtMap = STATIC_CONVERT(prodMap->get(fullPid), WTSDateHotMap*);
	if (dtMap == NULL)
		return "";  // 品种不存在，返回空字符串

	// 使用lower_bound查找第一个大于等于查询日期的切换项
	WTSDateHotMap::ConstIterator cit = dtMap->lower_bound(dt);
	
	if (cit != dtMap->end())
	{
		// 找到了大于等于查询日期的切换项
		
		// 如果查询日期小于找到的切换日期，需要使用前一个切换项
		if (dt < cit->first)
			cit--;

		// 检查回退后是否到达末尾
		if (cit == dtMap->end())
			return "";  // 无有效的主力合约，返回空字符串

		// 获取当前生效的切换项并返回其目标合约代码
		WTSSwitchItem* pItem = STATIC_CONVERT(cit->second, WTSSwitchItem*);
		return pItem->to();  // 返回当前主力合约代码
	}
	else
	{
		// 没找到大于等于查询日期的切换项，说明查询日期大于所有切换日期
		// 返回最后一个切换项的目标合约代码（最新主力合约）
		WTSSwitchItem* pItem = STATIC_CONVERT(dtMap->last(), WTSSwitchItem*);
		return pItem->to();  // 返回最新主力合约代码
	}

	// 理论上不会执行到这里，但为了代码完整性保留
	return "";
}

/**
 * @brief 判断合约是否为自定义规则的主力合约
 * 
 * 功能说明：
 * 根据自定义规则标签、完整合约代码和查询日期，判断指定合约是否为该规则下的主力合约。
 * 支持两种判断模式：当前时点快速判断和历史时点精确判断。
 * 
 * @param tag 规则标签，如"HOT"、"2ND"或用户自定义标签
 * @param fullCode 完整合约代码，格式：交易所.合约，如"SHFE.AU2409"
 * @param dt 查询日期，格式YYYYMMDD，默认为0表示当前日期
 * @return bool 是主力合约返回true，否则返回false
 * 
 * 算法逻辑：
 * 1. 参数验证：检查自定义规则映射表是否已初始化
 * 2. 快速判断模式（dt=0）：直接在当前主力代码集合中查找
 * 3. 历史判断模式（dt>0）：通过切换规则进行精确判断
 * 
 * 快速判断模式（dt=0）：
 * - 获取指定规则的当前主力合约代码集合
 * - 直接在集合中查找指定的完整合约代码
 * - 时间复杂度：O(1)，适用于实时交易场景
 * 
 * 历史判断模式（dt>0）：
 * - 解析完整合约代码，提取交易所、合约代码和品种代码
 * - 构造完整品种代码用于查找切换规则
 * - 使用lower_bound查找对应日期的切换项
 * - 比较切换项的目标合约与查询合约是否一致
 * - 时间复杂度：O(log n)，适用于历史数据分析
 * 
 * 合约代码解析：
 * - 从完整合约代码中提取原始合约代码（点号后的部分）
 * - 使用CodeHelper将月份合约代码转换为品种代码
 * - 构造完整品种代码：交易所.品种
 * 
 * 切换日期边界处理：
 * - 登记的换月日期是开始生效的交易日
 * - 如果是下午盘后确定主力，dt可能是第二天
 * - 因此dt必须大于等于切换日期才使用新的切换项
 * - 如果切换日期大于查询日期，需要使用前一个切换项
 * 
 * 使用场景：
 * - 实时交易：快速判断当前合约是否为主力（dt=0模式）
 * - 历史分析：判断历史时点的主力合约地位（dt>0模式）
 * - 合约筛选：从多个合约中筛选出主力合约
 * - 策略验证：验证交易策略中的主力合约选择
 */
bool WTSHotMgr::isCustomHot(const char* tag, const char* fullCode, uint32_t dt /* = 0 */)
{
	// 检查自定义规则映射表是否已初始化
	if (m_mapCustRules == NULL)
		return false;  // 未初始化，返回false

	// 获取指定规则的当前主力合约代码集合
	const auto& curHotCodes = m_mapCustCodes[tag];
	if (curHotCodes.empty())
		return false;  // 当前主力代码集合为空，返回false

	// 快速判断模式：如果查询日期为0，直接在当前主力代码集合中查找
	if (dt == 0)
	{
		// 在当前主力代码集合中查找指定的完整合约代码
		auto it = curHotCodes.find(fullCode);
		if (it == curHotCodes.end())
			return false;  // 未找到，不是当前主力合约
		else
			return true;   // 找到了，是当前主力合约
	}

	// 历史判断模式：需要通过切换规则进行精确判断
	
	// 查找完整合约代码中第一个点号的位置，用于分离交易所和合约代码
	auto idx = StrUtil::findFirst(fullCode, '.');
	
	// 提取点号后面的原始合约代码部分
	const char* rawCode = fullCode + idx + 1;
	
	// 构造完整品种代码：交易所.品种
	std::string fullPid(fullCode, idx);  // 提取交易所部分
	fullPid += ".";                      // 添加分隔符
	fullPid += CodeHelper::rawMonthCodeToRawCommID(rawCode);  // 添加品种代码

	// 根据规则标签获取对应的品种映射表
	WTSProductHotMap* prodMap = (WTSProductHotMap*)m_mapCustRules->get(tag);
	if (prodMap == NULL)
		return "";  // 规则不存在，返回false（注意：这里应该返回false而不是空字符串）

	// 根据完整品种代码获取对应的日期映射表
	WTSDateHotMap* dtMap = STATIC_CONVERT(prodMap->get(fullPid), WTSDateHotMap*);
	if (dtMap == NULL)
		return "";  // 品种不存在，返回false（注意：这里应该返回false而不是空字符串）

	// 使用lower_bound查找第一个大于等于查询日期的切换项
	WTSDateHotMap::ConstIterator cit = dtMap->lower_bound(dt);
	
	if (cit != dtMap->end())
	{
		// 找到了大于等于查询日期的切换项
		WTSSwitchItem* pItem = STATIC_CONVERT(cit->second, WTSSwitchItem*);
		
		// 因为登记的换月日期是开始生效的交易日，如果是下午盘后确定主力的话
		// 那么dt就会是第二天，所以，dt必须大于等于切换日期
		if (pItem->switch_date() > dt)
			cit--;  // 如果切换日期大于查询日期，使用前一个切换项

		// 重新获取切换项（可能已经回退）
		pItem = STATIC_CONVERT(cit->second, WTSSwitchItem*);
		
		// 比较切换项的目标合约代码与查询的原始合约代码
		if (strcmp(pItem->to(), rawCode) == 0)
			return true;  // 匹配，是主力合约
	}
	else if (dtMap->size() > 0)
	{
		// 没找到大于等于查询日期的切换项，但日期映射表不为空
		// 使用最后一个切换项进行判断
		WTSSwitchItem* pItem = (WTSSwitchItem*)dtMap->last();
		
		// 比较最后一个切换项的目标合约代码与查询的原始合约代码
		if (strcmp(pItem->to(), rawCode) == 0)
			return true;  // 匹配，是主力合约
	}

	// 所有情况都不匹配，不是主力合约
	return false;
}

/**
 * @brief 分割自定义规则的主力历史时段
 * 
 * 功能说明：
 * 根据自定义规则标签、完整品种代码和时间范围，将主力合约历史按切换时点分割成多个连续时段。
 * 每个时段对应一个主力合约的生效期间，包含合约代码、起止时间和复权因子。
 * 这是主力历史分析和策略回测的核心功能。
 * 
 * @param tag 规则标签，如"HOT"、"2ND"或用户自定义标签
 * @param fullPid 完整品种代码，格式：交易所.品种，如"SHFE.AU"
 * @param sDt 开始日期，格式YYYYMMDD
 * @param eDt 结束日期，格式YYYYMMDD
 * @param sections 输出参数，主力历史时段列表，类型为HotSections（std::vector<HotSection>）
 * @return bool 分割成功返回true，失败返回false
 * 
 * HotSection结构体说明：
 * - _code: 分月合约代码，如"AU2409"
 * - _s_date: 时段开始日期，格式YYYYMMDD
 * - _e_date: 时段结束日期，格式YYYYMMDD
 * - _factor: 复权因子，用于价格调整
 * 
 * 算法逻辑：
 * 1. 参数验证：检查映射表和品种数据是否存在
 * 2. 初始化变量：设置起始日期、当前主力等状态变量
 * 3. 遍历切换项：按时间顺序处理所有主力切换事件
 * 4. 时段划分：根据切换时点将时间范围分割成连续时段
 * 5. 边界处理：处理查询范围的边界情况
 * 6. 结果输出：构造HotSection对象并添加到输出列表
 * 
 * 时段分割策略：
 * - 当切换日期大于结束日期时：
 *   - 添加从当前左边界到结束日期的时段
 *   - 使用当前切换项的源合约代码
 * - 当左边界小于当前切换日期时：
 *   - 添加从左边界到切换日期前一天的时段
 *   - 使用当前切换项的源合约代码
 *   - 更新左边界为切换日期
 * - 处理最后一个时段：
 *   - 如果左边界大于等于最后切换日期，添加剩余时段
 *   - 使用最新的主力合约代码
 * 
 * 边界条件处理：
 * - 源合约代码为空：跳过该时段（通常是第一条规则）
 * - 时间范围检查：确保时段在查询范围内
 * - 复权因子传递：每个时段携带对应的复权因子
 * 
 * 复权因子说明：
 * - 复权因子用于处理主力切换时的价格跳跃
 * - 每次切换时复权因子会更新
 * - 时段中的复权因子是该时段生效时的累积因子
 * 
 * 使用场景：
 * - 策略回测：按主力时段分别处理历史数据
 * - 连续合约构建：为连续合约数据提供时段划分
 * - 主力切换研究：分析主力切换的时间规律
 * - 数据分析：统计不同主力合约的表现
 * - 复权数据处理：基于时段进行价格复权调整
 */
bool WTSHotMgr::splitCustomSections(const char* tag, const char* fullPid, uint32_t sDt, uint32_t eDt, HotSections& sections)
{
	// 检查自定义规则映射表是否已初始化
	if (m_mapCustRules == NULL)
		return false;  // 未初始化，返回失败

	// 根据规则标签获取对应的品种映射表
	WTSProductHotMap* prodMap = (WTSProductHotMap*)m_mapCustRules->get(tag);
	if (prodMap == NULL)
		return false;  // 规则不存在，返回失败

	// 根据完整品种代码获取对应的日期映射表
	WTSDateHotMap* dtMap = STATIC_CONVERT(prodMap->get(fullPid), WTSDateHotMap*);
	if (dtMap == NULL)
		return false;  // 品种不存在，返回失败

	// 初始化时段分割的状态变量
	uint32_t leftDate = sDt;        // 当前时段的左边界（开始日期）
	uint32_t lastDate = 0;          // 最后一个切换日期
	const char* curHot = "";        // 当前主力合约代码
	auto cit = dtMap->begin();      // 日期映射表的迭代器，从第一个切换项开始
	double prevFactor = 1.0;        // 前一个时段的复权因子

	// 遍历所有的主力切换项，按时间顺序处理
	for (; cit != dtMap->end(); cit++)
	{
		// 获取当前切换项的切换日期和切换对象
		uint32_t curDate = cit->first;                                // 当前切换日期
		WTSSwitchItem* hotItem = (WTSSwitchItem*)cit->second;        // 当前切换项对象

		// 情况1：如果当前切换日期大于查询结束日期
		if (curDate > eDt)
		{
			// 添加从左边界到结束日期的时段，使用当前切换项的源合约
			sections.emplace_back(HotSection(hotItem->from(), leftDate, eDt, prevFactor));
		}
		// 情况2：如果左边界小于当前切换日期（正常的时段分割情况）
		else if (leftDate < curDate)
		{
			// 如果开始日期小于当前切换的日期，则添加一段
			// 检查源合约代码是否为空（主要是第一条规则的情况）
			if (strlen(hotItem->from()) > 0)
			{
				// 这里from为空，主要是第一条规则，如果真的遇到这种情况，
				// 也没有太好的办法，只能不要这一段数据了，一般情况下是够的
				
				// 添加从左边界到切换日期前一天的时段
				sections.emplace_back(HotSection(
					hotItem->from(),                          // 使用源合约代码
					leftDate,                                 // 时段开始日期
					TimeUtils::getNextDate(curDate, -1),     // 时段结束日期（切换日期前一天）
					prevFactor                                // 使用前一个复权因子
				));
			}

			// 更新左边界为当前切换日期，准备处理下一个时段
			leftDate = curDate;
		}

		// 更新状态变量，准备处理下一个切换项
		lastDate = curDate;                    // 记录最后一个切换日期
		prevFactor = hotItem->get_factor();    // 更新复权因子
		curHot = hotItem->to();                // 更新当前主力合约代码
	}

	// 处理最后一个时段：如果左边界大于等于最后切换日期且最后切换日期不为0
	if (leftDate >= lastDate && lastDate != 0)
	{
		// 添加从最后切换日期到结束日期的时段，使用最新的主力合约
		sections.emplace_back(HotSection(curHot, leftDate, eDt, prevFactor));
	}

	// 时段分割成功完成
	return true;
}
#pragma endregion "自定义主力接口"

/**
 * @brief 释放主力合约管理器占用的资源
 * 
 * 功能说明：
 * 释放主力合约管理器在运行过程中分配的所有内存资源，确保程序结束时不会出现内存泄漏。
 * 这个方法应该在主力管理器不再使用时调用，通常在程序退出或重新初始化时执行。
 * 
 * 资源释放内容：
 * - 自定义规则映射表：释放所有自定义主力规则的配置数据
 * - 嵌套数据结构：递归释放品种映射、日期映射和切换项等多层数据结构
 * - 内存指针重置：将指针设置为NULL，避免悬空指针问题
 * 
 * 历史版本兼容：
 * 代码中保留了历史版本的注释代码，用于参考早期版本的数据结构：
 * - m_pExchgHotMap：按交易所组织的主力映射表
 * - m_pExchgScndMap：按交易所组织的次主力映射表
 * 这些结构在当前版本中已被更灵活的自定义规则映射表替代。
 * 
 * 内存管理原则：
 * - RAII原则：资源获取即初始化，确保资源正确释放
 * - 防御性编程：在释放前检查指针是否为空
 * - 递归释放：由于使用了嵌套的WTS容器，调用release()会递归释放所有子对象
 * 
 * 调用时机：
 * - 程序正常退出时
 * - 主力管理器重新初始化前
 * - 内存资源紧张时的主动清理
 * - 单元测试的清理阶段
 * 
 * 线程安全性：
 * 这个方法不是线程安全的，应该在确保没有其他线程访问主力管理器时调用。
 * 通常在程序的单线程初始化或清理阶段执行。
 * 
 * 使用注意事项：
 * - 调用release()后，主力管理器将不可用，需要重新初始化
 * - 不要在其他方法正在使用管理器时调用此方法
 * - 调用后应避免再次调用其他成员方法
 */
void WTSHotMgr::release()
{
	// 以下为历史版本的资源释放代码，已被注释但保留用于参考
	// 早期版本使用按交易所分类的映射表结构
	//if (m_pExchgHotMap)
	//{
	//	m_pExchgHotMap->release();      // 释放主力映射表
	//	m_pExchgHotMap = NULL;          // 重置指针为NULL
	//}

	//if (m_pExchgScndMap)
	//{
	//	m_pExchgScndMap->release();     // 释放次主力映射表
	//	m_pExchgScndMap = NULL;         // 重置指针为NULL
	//}

	// 释放当前版本使用的自定义规则映射表
	if(m_mapCustRules)
	{
		// 调用WTS容器的release()方法，会递归释放所有嵌套的数据结构：
		// - 规则标签映射（WTSCustomSwitchMap）
		// - 品种映射表（WTSProductHotMap）
		// - 日期映射表（WTSDateHotMap）
		// - 切换项对象（WTSSwitchItem）
		m_mapCustRules->release();
		
		// 将指针重置为NULL，避免悬空指针问题
		m_mapCustRules = NULL;
	}
	
	// 注意：m_mapCustCodes是STL容器，会在对象析构时自动释放
	// 不需要手动释放，但在重新初始化时会被自动清空
}