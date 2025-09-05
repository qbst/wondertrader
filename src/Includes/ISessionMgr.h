/*!
 * \file ISessionMgr.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易时间模板管理器接口定义文件
 * 
 * 本文件定义了WonderTrader框架中交易时间模板管理器的接口。
 * 交易时间模板管理器负责为不同的合约提供对应的交易时间模板信息，
 * 包括交易时段、休市时间、夜盘时间等，确保策略能够正确处理不同品种的交易时间。
 * 
 * 主要功能：
 * 1. 根据合约代码和交易所获取对应的交易时间模板
 * 2. 支持不同品种的差异化交易时间设置
 * 3. 为策略提供准确的交易时间判断依据
 */
#pragma once

#include "WTSMarcos.h"

NS_WTP_BEGIN
class WTSSessionInfo;    // 交易时间模板信息类

/**
 * @brief 交易时间模板管理器接口
 * 
 * 该接口定义了交易时间模板管理器的基本功能。
 * 通过该接口，策略可以获取任意合约对应的交易时间模板信息，
 * 从而正确判断当前是否处于交易时段，避免在非交易时间执行交易操作。
 */
class ISessionMgr
{
public:
	/**
	 * @brief 获取合约所属的交易时间模板对象
	 * @param code 合约代码
	 * @param exchg 交易所代码，为空时使用默认交易所
	 * @return 交易时间模板对象指针，不存在则为NULL
	 * 
	 * 该方法根据合约代码和交易所代码，返回对应的交易时间模板对象。
	 * 交易时间模板包含了该合约的详细交易时间信息，如开盘时间、收盘时间、
	 * 午休时间、夜盘时间等，策略可以通过这些信息判断当前是否可以进行交易。
	 */
	virtual WTSSessionInfo* getSession(const char* code, const char* exchg = "")	= 0;
};
NS_WTP_END