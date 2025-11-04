/*!
 * \file WtFilterMgr.h
 * \project	WonderTrader
 * 
 * \brief 过滤器管理器类头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中的信号过滤器管理类，用于管理和执行交易信号的过滤规则。
 * 主要功能包括：
 * 1. 策略过滤器：根据策略名称过滤交易信号，支持忽略或重定向仓位
 * 2. 合约过滤器：根据合约代码过滤交易信号，支持忽略或重定向仓位
 * 3. 执行器过滤器：根据执行器通道ID过滤交易信号，禁用特定执行器
 * 4. 动态加载：支持从配置文件动态加载过滤器规则，并支持热重载
 * 5. 过滤判断：提供多种过滤判断接口，检查信号是否被过滤
 * 
 * 设计模式：
 * - 策略模式：不同的过滤操作（忽略、重定向）通过枚举值实现
 * - 配置驱动：过滤器规则通过配置文件定义，运行时加载
 * - 观察者模式：通过EventNotifier通知过滤器配置变化
 * 
 * 使用场景：
 * 该管理器主要用于交易执行系统中，允许用户通过配置文件灵活控制
 * 哪些策略、合约或执行器的信号需要被过滤或修改，实现风险控制和仓位管理。
 */
#pragma once  // 防止头文件重复包含
#include <string>  // 包含标准字符串类型
#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap
#include "../Includes/WTSMarcos.h"  // 包含WonderTrader的宏定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

class EventNotifier;  // 前向声明：事件通知器类，用于通知过滤器配置变化

/**
 * @class WtFilterMgr
 * @brief 过滤器管理器类
 * 
 * 该类负责管理和执行交易信号的过滤规则，支持策略级、合约级和执行器级的过滤。
 * 过滤器规则通过配置文件定义，支持动态加载和热重载。
 * 
 * 主要功能：
 * - 加载过滤器配置文件，解析过滤规则
 * - 检查策略信号是否被过滤
 * - 检查合约信号是否被过滤
 * - 检查执行器是否被禁用
 * - 支持增量头寸的特殊处理
 * - 支持仓位重定向功能
 */
class WtFilterMgr
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化过滤器管理器，将过滤器时间戳初始化为0，事件通知器初始化为NULL。
	 */
	WtFilterMgr():_filter_timestamp(0), _notifier(NULL){}  // 初始化过滤器时间戳和通知器指针

	/**
	 * @brief 设置事件通知器
	 * @param notifier 事件通知器指针
	 * 
	 * 设置事件通知器，用于在过滤器配置重新加载时发送通知。
	 */
	void		set_notifier(EventNotifier* notifier) { _notifier = notifier; }  // 设置事件通知器指针

	/**
	 * @brief 加载信号过滤器
	 * @param fileName 过滤器配置文件路径，为空字符串时使用上次设置的路径
	 * 
	 * 从配置文件加载过滤器规则，支持以下类型的过滤器：
	 * - strategy_filters: 策略过滤器
	 * - code_filters: 合约代码过滤器
	 * - executer_filters: 执行器过滤器
	 * 
	 * 如果配置文件被修改，会自动重新加载。
	 */
	void		load_filters(const char* fileName = "");  // 加载过滤器配置文件

	/**
	 * @brief 检查策略是否被过滤
	 * @param straName 策略名称
	 * @param targetPos 目标仓位引用，如果过滤器是重定向操作，该值会被修改为目标仓位
	 * @param isDiff 是否是增量头寸，默认为false
	 * @return bool 如果被过滤（忽略）返回true，否则返回false
	 * 
	 * 该函数检查指定策略的信号是否被过滤器过滤掉。
	 * 过滤操作类型：
	 * - ignore: 忽略该策略的信号，返回true，目标仓位不变
	 * - redirect: 重定向仓位，返回false，目标仓位被修改为配置文件中的目标值
	 * 
	 * 特殊处理：
	 * - 如果是增量头寸（isDiff=true）且被过滤器触发，直接返回true忽略该变动
	 */
	bool		is_filtered_by_strategy(const char* straName, double& targetPos, bool isDiff = false);  // 检查策略是否被过滤

	/**
	 * @brief 检查合约是否被过滤
	 * @param stdCode 标准合约代码
	 * @param targetPos 目标仓位引用，如果过滤器是重定向操作，该值会被修改为目标仓位
	 * @return bool 如果被过滤（忽略）返回true，否则返回false
	 * 
	 * 该函数检查指定合约的信号是否被过滤器过滤掉。
	 * 过滤操作类型：
	 * - ignore: 忽略该合约的信号，返回true，目标仓位不变
	 * - redirect: 重定向仓位，返回false，目标仓位被修改为配置文件中的目标值
	 * 
	 * 匹配规则：
	 * - 优先匹配完整合约代码
	 * - 如果完整代码不匹配，则匹配品种代码
	 * - 同一时间只有一个过滤器生效，合约代码优先级高于品种代码
	 */
	bool		is_filtered_by_code(const char* stdCode, double& targetPos);  // 检查合约是否被过滤

	/**
	 * @brief 检查执行器是否被过滤
	 * @param execid 交易通道ID（执行器标识）
	 * @return bool 如果被过滤（禁用）返回true，否则返回false
	 * 
	 * 该函数检查指定执行器是否被过滤器禁用。
	 * 如果执行器被禁用，该执行器将不执行任何交易信号。
	 */
	bool		is_filtered_by_executer(const char* execid);  // 检查执行器是否被过滤

private:
	//////////////////////////////////////////////////////////////////////////
	//信号过滤器相关定义
	/**
	 * @enum FilterAction
	 * @brief 过滤器操作类型枚举
	 * 
	 * 定义了过滤器可以执行的操作类型：
	 * - FA_Ignore: 忽略操作，即维持原有仓位，不执行该信号
	 * - FA_Redirect: 重定向操作，即将目标仓位同步到指定值
	 * - FA_None: 无操作，表示未定义的操作类型
	 */
	typedef enum tagFilterAction
	{
		FA_Ignore,		// 忽略操作，即维持原有仓位，不执行该信号
		FA_Redirect,	// 重定向持仓操作，即同步到指定目标仓位
		FA_None = 99  // 无操作，表示未定义的操作类型
	} FilterAction;  // 过滤器操作类型枚举别名

	/**
	 * @struct _FilterItem
	 * @brief 过滤器项结构体
	 * 
	 * 存储单个过滤器的配置信息，包括关键字、操作类型和目标仓位。
	 */
	typedef struct _FilterItem
	{
		std::string		_key;		// 过滤器关键字，用于匹配策略名称或合约代码
		FilterAction	_action;	// 过滤操作类型，决定是忽略还是重定向
		double			_target;	// 目标仓位值，只有当_action为FA_Redirect时才生效
	} FilterItem;  // 过滤器项结构体类型别名

	typedef wt_hashmap<std::string, FilterItem>	FilterMap;  // 过滤器映射表类型别名，键为字符串，值为过滤器项
	FilterMap		_stra_filters;	// 策略过滤器映射表，键为策略名称，值为过滤器配置

	FilterMap		_code_filters;	// 代码过滤器映射表，键为合约代码或品种代码，值为过滤器配置
	                // 注意：同一时间只有一个生效，合约代码优先级高于品种代码

	//交易通道过滤器
	typedef wt_hashmap<std::string, bool>	ExecuterFilters;  // 执行器过滤器映射表类型别名，键为执行器ID，值为是否禁用（true表示禁用）
	ExecuterFilters	_exec_filters;  // 执行器过滤器映射表，存储所有被禁用的执行器ID

	std::string		_filter_file;	// 过滤器配置文件路径
	uint64_t		_filter_timestamp;	// 过滤器文件最后修改时间戳，用于检测文件是否被修改

	EventNotifier*	_notifier;  // 事件通知器指针，用于通知过滤器配置变化
};

NS_WTP_END  // 结束WonderTrader命名空间

