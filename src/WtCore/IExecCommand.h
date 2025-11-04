/*!
 * \file IExecCommand.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 *
 * \brief 执行命令接口头文件
 *
 * 文件设计逻辑与作用总结：
 * 本文件定义了执行器相关的接口类，用于实现执行命令模式。
 * IExecuterStub是一个执行器存根接口，提供执行器所需的基础信息查询功能。
 * IExecCommand是执行命令接口，定义了执行器需要实现的命令接口，包括设置目标仓位、
 * 仓位变动通知和行情回调等。
 * 
 * 设计模式：
 * 使用了命令模式和执行器模式，将执行逻辑封装为命令对象，通过执行器存根提供上下文信息。
 * 这种设计使得执行逻辑和执行器实现解耦，便于扩展和维护。
 */
#pragma once  // 防止头文件重复包含
#include "../Includes/FasterDefs.h"  // 包含WonderTrader的快速定义，如wt_hashmap
#include <stdint.h>  // 包含标准整数类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间
class WTSCommodityInfo;  // 前向声明：商品信息类
class WTSSessionInfo;  // 前向声明：交易会话信息类
class IHotMgr;  // 前向声明：热点合约管理器接口
class WTSTickData;  // 前向声明：Tick数据类

/**
 * @class IExecuterStub
 * @brief 执行器存根接口类
 *
 * 该接口定义了执行器所需的基础信息查询功能。
 * 执行命令对象通过该接口获取执行所需的信息，如当前时间、商品信息、交易会话信息等。
 * 使用存根模式，将执行器的实现细节隐藏，只暴露必要的查询接口。
 */
class IExecuterStub
{
public:
	/**
	 * @brief 获取实时时间
	 * @return uint64_t 返回当前实时时间戳（纳秒级）
	 *
	 * 获取执行器当前的实时时间戳。
	 */
	virtual uint64_t get_real_time() = 0;  // 获取实时时间

	/**
	 * @brief 获取商品信息
	 * @param stdCode 标准合约代码
	 * @return WTSCommodityInfo* 返回商品信息对象指针
	 *
	 * 根据标准合约代码获取对应的商品信息，包括交易规则、手续费等。
	 */
	virtual WTSCommodityInfo* get_comm_info(const char* stdCode) = 0;  // 获取商品信息

	/**
	 * @brief 获取交易会话信息
	 * @param stdCode 标准合约代码
	 * @return WTSSessionInfo* 返回交易会话信息对象指针
	 *
	 * 根据标准合约代码获取对应的交易会话信息，包括交易时间段、开盘时间等。
	 */
	virtual WTSSessionInfo* get_sess_info(const char* stdCode) = 0;  // 获取交易会话信息

	/**
	 * @brief 获取热点合约管理器
	 * @return IHotMgr* 返回热点合约管理器接口指针
	 *
	 * 获取热点合约管理器，用于查询主力合约、次主力合约等。
	 */
	virtual IHotMgr* get_hot_mon() = 0;  // 获取热点合约管理器

	/**
	 * @brief 获取交易日期
	 * @return uint32_t 返回当前交易日期（格式：YYYYMMDD）
	 *
	 * 获取执行器当前的交易日期。
	 */
	virtual uint32_t get_trading_day() = 0;  // 获取交易日期
};

/**
 * @class IExecCommand
 * @brief 执行命令接口类
 *
 * 该接口定义了执行器需要实现的命令接口。
 * 执行命令对象通过实现这些接口，定义具体的执行逻辑。
 * 执行器会调用这些接口来执行相应的命令。
 * 
 * 主要功能：
 * 1. 设置目标仓位：根据目标仓位映射表设置各合约的目标持仓。
 * 2. 仓位变动通知：当合约仓位发生变化时通知执行命令。
 * 3. 行情回调：当收到实时行情数据时通知执行命令。
 */
class IExecCommand
{
public:
	/**
	 * @brief 构造函数
	 * @param name 执行命令名称
	 *
	 * 初始化执行命令对象，设置命令名称，并将执行器存根指针初始化为NULL。
	 */
	IExecCommand(const char* name) :_stub(NULL), _name(name){}  // 构造函数，初始化名称和存根指针

	/**
	 * @brief 设置目标仓位
	 * @param targets 目标仓位映射表，键为合约代码，值为目标持仓数量
	 *
	 * 根据目标仓位映射表设置各合约的目标持仓。
	 * 执行器会根据当前持仓和目标持仓的差异，生成相应的交易指令。
	 * 默认实现为空，子类需要重写此方法以实现具体的执行逻辑。
	 */
	virtual void set_position(const wt_hashmap<std::string, double>& targets) {}  // 设置目标仓位

	/**
	 * @brief 合约仓位变动通知
	 * @param stdCode 标准合约代码
	 * @param diffPos 仓位变动数量（正数表示增加，负数表示减少）
	 *
	 * 当合约仓位发生变化时被调用，通知执行命令仓位变动情况。
	 * 执行命令可以根据仓位变动情况更新内部状态或执行其他逻辑。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void on_position_changed(const char* stdCode, double diffPos) {}  // 合约仓位变动通知

	/**
	 * @brief 实时行情回调
	 * @param stdCode 标准合约代码
	 * @param newTick 新的Tick数据指针
	 *
	 * 当收到实时行情数据时被调用，通知执行命令最新的行情信息。
	 * 执行命令可以根据行情数据调整执行策略或触发执行逻辑。
	 * 默认实现为空，子类可以重写此方法以实现具体的处理逻辑。
	 */
	virtual void on_tick(const char* stdCode, WTSTickData* newTick) {}  // 实时行情回调

	/**
	 * @brief 设置执行器存根
	 * @param stub 执行器存根指针
	 *
	 * 设置执行命令关联的执行器存根，用于获取执行所需的信息。
	 */
	inline void setStub(IExecuterStub* stub) { _stub = stub; }  // 设置执行器存根指针

	/**
	 * @brief 获取执行命令名称
	 * @return const char* 返回执行命令名称字符串
	 *
	 * 获取执行命令的名称，用于标识和日志记录。
	 */
	inline const char* name() const { return _name.c_str(); }  // 获取执行命令名称

	/**
	 * @brief 设置执行命令名称
	 * @param name 新的执行命令名称
	 *
	 * 设置执行命令的名称。
	 */
	inline void setName(const char* name) { _name = name; }  // 设置执行命令名称

protected:
	IExecuterStub*	_stub;  // 执行器存根指针，用于获取执行所需的信息
	std::string		_name;  // 执行命令名称
};
NS_WTP_END  // 结束WonderTrader命名空间