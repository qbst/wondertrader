/*!
 * \file WTSError.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt错误对象定义
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统的错误处理机制，提供统一的错误信息封装和管理。
 * 
 * 主要功能：
 * 1. 错误代码枚举：定义系统中可能出现的各种错误类型
 * 2. 错误信息封装：将错误代码和描述信息组合成错误对象
 * 3. 错误对象管理：基于引用计数机制管理错误对象的生命周期
 * 
 * 设计特点：
 * - 继承自WTSObject，支持引用计数和自动内存管理
 * - 提供静态工厂方法创建错误对象
 * - 支持错误代码和错误信息的分离存储
 * - 便于在系统中传递和处理错误信息
 */
#pragma once
#include "WTSObject.hpp"  // 包含WonderTrader基础对象类，提供引用计数功能
#include "WTSTypes.h"      // 包含WonderTrader类型定义

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * WonderTrader错误信息类
 * 
 * 功能概述：
 * 本类用于封装和管理系统中产生的各种错误信息，提供统一的错误处理机制。
 * 
 * 主要特性：
 * - 继承自WTSObject，支持引用计数和自动内存管理
 * - 封装错误代码和错误描述信息
 * - 提供静态工厂方法创建错误对象
 * - 支持多种错误类型的统一管理
 * 
 * 使用场景：
 * - API调用失败时返回错误信息
 * - 数据处理过程中的异常情况
 * - 网络通信或文件操作的错误处理
 * - 系统内部状态异常的报告
 */
class WTSError : public WTSObject
{
protected:
	/**
	 * 保护构造函数
	 * 防止直接实例化，必须通过静态工厂方法创建
	 * 初始化错误代码为WEC_NONE（无错误），错误信息为空字符串
	 */
	WTSError():m_errCode(WEC_NONE),m_strMsg(""){}
	
	/**
	 * 虚析构函数
	 * 支持多态销毁，确保派生类对象能够正确清理资源
	 */
	virtual ~WTSError(){}

public:
	/**
	 * 静态工厂方法：创建错误对象
	 * 
	 * @param ec 错误代码，定义错误的类型（如网络错误、数据错误等）
	 * @param errmsg 错误描述信息，提供错误的详细说明
	 * @return 新创建的错误对象指针，调用者负责管理其生命周期
	 * 
	 * 使用示例：
	 * WTSError* error = WTSError::create(WEC_NETWORK_ERROR, "连接服务器失败");
	 */
	static WTSError* create(WTSErroCode ec, const char* errmsg)
	{
		WTSError* pRet = new WTSError;  // 创建新的错误对象实例
		pRet->m_errCode = ec;           // 设置错误代码
		pRet->m_strMsg = errmsg;        // 设置错误信息字符串

		return pRet;  // 返回创建的实例指针
	}

	/**
	 * 获取错误信息字符串
	 * 
	 * @return 错误描述信息的C风格字符串指针
	 * 
	 * 注意：返回的指针指向内部字符串，不应该被修改或释放
	 */
	const char*		getMessage() const{return m_strMsg.c_str();}
	
	/**
	 * 获取错误代码
	 * 
	 * @return 错误代码枚举值，用于程序逻辑中的错误类型判断
	 */
	WTSErroCode		getErrorCode() const{return m_errCode;}

protected:
	WTSErroCode		m_errCode;  // 错误代码，标识错误的类型和级别
	std::string		m_strMsg;   // 错误信息，提供错误的详细描述和上下文信息
};


NS_WTP_END