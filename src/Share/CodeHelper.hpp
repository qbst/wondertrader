/*!
 * \file CodeHelper.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 代码辅助类,封装到一起方便使用
 * 
 * 文件设计逻辑与作用总结：
 * 本文件提供了期货、期权、股票等金融产品代码的标准化处理和解析功能。
 * 主要功能包括：
 * 1. 合约代码的标准化转换（原始代码转标准代码）
 * 2. 合约代码信息的提取和解析
 * 3. 主力合约、次主力合约代码的生成
 * 4. 复权代码的处理和识别
 * 5. 不同交易所代码格式的统一处理
 * 
 * 该类主要用于WonderTrader框架中处理各种金融产品的代码标准化问题，确保：
 * - 不同交易所的代码格式能够统一处理
 * - 主力合约、复权合约等特殊代码能够正确识别
 * - 代码解析和转换的一致性和准确性
 * - 支持期货、期权、股票等多种产品类型
 * 
 * 通过提供统一的代码处理接口，简化了框架中各种代码相关的业务逻辑。
 */
#pragma once
#include "fmtlib.h"                            // 格式化库头文件，提供字符串格式化功能
#include "StrUtil.hpp"                         // 字符串工具头文件，提供字符串处理功能
#include "../Includes/WTSTypes.h"              // WonderTrader类型定义头文件
#include "../Includes/IHotMgr.h"               // 主力合约管理接口头文件

#include <boost/xpressive/xpressive_dynamic.hpp>  // boost正则表达式库头文件，用于代码格式验证


USING_NS_WTP;                                  // 使用WonderTrader命名空间

//主力合约后缀
static const char* SUFFIX_HOT = ".HOT";        // 主力合约代码的标准后缀

//次主力合约后缀
static const char* SUFFIX_2ND = ".2ND";        // 次主力合约代码的标准后缀

//前复权合约代码后缀
static const char SUFFIX_QFQ = '-';            // 前复权合约代码的标准后缀

//后复权合约代码后缀
static const char SUFFIX_HFQ = '+';            // 后复权合约代码的标准后缀

/**
 * @brief 代码辅助工具类
 * 
 * 该类提供了期货、期权、股票等金融产品代码的标准化处理和解析功能。
 * 支持多种代码格式的转换、解析和验证，是WonderTrader框架中代码处理的核心工具类。
 * 
 * 主要功能：
 * - 合约代码的标准化转换
 * - 合约代码信息的提取和解析
 * - 主力合约和次主力合约代码生成
 * - 复权代码的处理和识别
 * - 不同交易所代码格式的统一处理
 * 
 * 适用场景：
 * - 行情数据接入时的代码标准化
 * - 交易接口调用前的代码转换
 * - 数据存储和查询时的代码处理
 * - 策略开发中的代码识别和验证
 */
class CodeHelper
{
public:
	/**
	 * @brief 合约代码信息结构体
	 * 
	 * 存储解析后的合约代码的各个组成部分，包括交易所、品种、合约代码等信息。
	 * 用于代码标准化后的信息提取和后续处理。
	 */
	typedef struct _CodeInfo
	{
		char _code[MAX_INSTRUMENT_LENGTH];		// 合约代码，如"600000"、"IF2112"
		char _exchg[MAX_INSTRUMENT_LENGTH];		// 交易所代码，如"SSE"、"CFFEX"
		char _product[MAX_INSTRUMENT_LENGTH];	// 品种代码，如"STK"、"IF"
		char _ruletag[MAX_INSTRUMENT_LENGTH];	// 规则标签，用于标识特殊的交易规则
		char _fullpid[MAX_INSTRUMENT_LENGTH];	// 完整品种ID，格式为"交易所.品种"

		//By Wesley @ 2021.12.25
		//去掉合约类型，这里不再进行判断
		//整个CodeHelper会重构
		//ContractCategory	_category;		//合约类型
		//union
		//{
		//	uint8_t	_hotflag;	//主力标记，0-非主力，1-主力，2-次主力
		//	uint8_t	_exright;	//是否是复权代码,如SH600000Q: 0-不复权, 1-前复权, 2-后复权
		//};

		/*
		 *	By Wesley @ 2022.03.07
		 *	取消原来的union
		 *	要把主力标记和复权标记分开处理
		 *	因为后面要对主力合约做复权处理了
		 */
		uint8_t	_exright;	//是否是复权代码,如SH600000Q: 0-不复权, 1-前复权, 2-后复权

		/**
		 * @brief 检查是否为复权代码
		 * 
		 * @return true 是复权代码，false 不是复权代码
		 * 
		 * 通过检查_exright字段是否为0来判断是否为复权代码
		 */
		inline bool isExright() const { return _exright != 0; }

		/**
		 * @brief 检查是否为前复权代码
		 * 
		 * @return true 是前复权代码，false 不是前复权代码
		 * 
		 * 通过检查_exright字段是否等于1来判断是否为前复权代码
		 */
		inline bool isForwardAdj() const { return _exright == 1; }

		/**
		 * @brief 检查是否为后复权代码
		 * 
		 * @return true 是后复权代码，false 不是后复权代码
		 * 
		 * 通过检查_exright字段是否等于2来判断是否为后复权代码
		 */
		inline bool isBackwardAdj() const { return _exright == 2; }

		/**
		 * @brief 获取标准品种ID
		 * 
		 * @return 标准品种ID字符串，格式为"交易所.品种"
		 * 
		 * 如果_fullpid字段为空，则动态生成标准品种ID
		 */
		inline const char* stdCommID()
		{
			if (strlen(_fullpid) == 0)          // 检查_fullpid是否为空
				fmtutil::format_to(_fullpid, "{}.{}", _exchg, _product);  // 动态生成标准品种ID

			return _fullpid;                     // 返回标准品种ID
		}

		/**
		 * @brief 默认构造函数
		 * 
		 * 初始化所有字段为0，确保结构体处于安全状态
		 */
		_CodeInfo()
		{
			memset(this, 0, sizeof(_CodeInfo)); // 将所有字段初始化为0
			//_category = CC_Future;            // 注释掉的合约类型初始化
		}

		/**
		 * @brief 清空所有字段
		 * 
		 * 将所有字段重置为0，用于结构体的重复使用
		 */
		inline void clear()
		{
			memset(this, 0, sizeof(_CodeInfo)); // 将所有字段重置为0
		}

		/**
		 * @brief 检查是否有规则标签
		 * 
		 * @return true 有规则标签，false 没有规则标签
		 * 
		 * 通过检查_ruletag字段的长度来判断是否有规则标签
		 */
		inline bool hasRule() const
		{
			return strlen(_ruletag) > 0;         // 检查规则标签长度是否大于0
		}
	} CodeInfo;

private:
	/**
	 * @brief 查找字符串中指定字符的位置
	 * 
	 * @param src 源字符串
	 * @param symbol 要查找的字符，默认为'.'
	 * @param bReverse 是否反向查找，默认为false
	 * @return 找到的位置，如果未找到返回std::string::npos
	 * 
	 * 在字符串中查找指定字符的位置，支持正向和反向查找
	 */
	static inline std::size_t find(const char* src, char symbol = '.', bool bReverse = false)
	{
		std::size_t len = strlen(src);           // 获取字符串长度
		if (len != 0)                            // 检查字符串是否为空
		{
			if (bReverse)                        // 反向查找
			{
				for (std::size_t idx = len - 1; idx >= 0; idx--)  // 从后往前遍历
				{
					if (src[idx] == symbol)      // 检查当前字符是否为目标字符
						return idx;               // 找到目标字符，返回位置
				}
			}
			else                                  // 正向查找
			{
				for (std::size_t idx = 0; idx < len; idx++)  // 从前往后遍历
				{
					if (src[idx] == symbol)      // 检查当前字符是否为目标字符
						return idx;               // 找到目标字符，返回位置
				}
			}
		}

		return std::string::npos;                // 未找到目标字符，返回npos
	}

public:
	/*
	 *	是否是期货期权合约代码
	 *	CFFEX.IO2007.C.4000
	 */
	/**
	 * @brief 检查是否为标准中国期货期权合约代码
	 * 
	 * @param code 要检查的合约代码
	 * @return true 是标准期货期权代码，false 不是
	 * 
	 * 通过状态机方式验证代码格式，支持CFFEX.IO2007.C.4000等格式
	 */
	static bool	isStdChnFutOptCode(const char* code)
	{
		/* 定义正则表达式 */
		//static cregex reg_stk = cregex::compile("^[A-Z]+.[A-z]+\\d{4}.(C|P).\\d+$");	//CFFEX.IO2007.C.4000
		//return 	regex_match(code, reg_stk);
		char state = 0;                          // 初始化状态机状态
		std::size_t i = 0;                       // 初始化字符索引
		for(; ; i++)                             // 遍历代码字符串
		{
			char ch = code[i];                   // 获取当前字符
			if(ch == '\0')                       // 检查是否到达字符串结尾
				break;

			if(state == 0)                       // 状态0：期望交易所代码（大写字母）
			{
				if (!('A' <= ch && ch <= 'Z'))   // 检查是否为大写字母
					return false;                 // 如果不是大写字母，返回false

				state += 1;                       // 状态转移到下一个状态
			}
			else if (state == 1)                 // 状态1：继续交易所代码或遇到分隔符
			{
				if ('A' <= ch && ch <= 'Z')      // 继续大写字母
					continue;                     // 继续处理

				if (ch == '.')                    // 遇到分隔符
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state == 2)                 // 状态2：期望品种代码（字母）
			{
				if (!('A' <= ch && ch <= 'z'))   // 检查是否为字母
					return false;                 // 如果不是字母，返回false

				state += 1;                       // 状态转移到下一个状态
			}
			else if (state == 3)                 // 状态3：继续品种代码或遇到数字
			{
				if ('A' <= ch && ch <= 'z')      // 继续字母
					continue;                     // 继续处理

				if ('0' <= ch && ch <= '9')      // 遇到数字
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state >= 4 && state <= 6)   // 状态4-6：处理年份数字
			{
				if ('0' <= ch && ch <= '9')      // 检查是否为数字
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state == 7)                 // 状态7：期望分隔符
			{
				if (ch == '.')                    // 检查是否为分隔符
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state == 8)                 // 状态8：期望期权类型（C或P）
			{
				if (ch == 'C' || ch == 'P')      // 检查是否为C或P
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state == 9)                 // 状态9：期望分隔符
			{
				if (ch == '.')                    // 检查是否为分隔符
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state == 10)                // 状态10：期望执行价格数字
			{
				if ('0' <= ch && ch <= '9')      // 检查是否为数字
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state == 11)                // 状态11：继续执行价格数字
			{
				if ('0' <= ch && ch <= '9')      // 继续数字
					continue;                     // 继续处理
				else
					return false;                 // 其他字符返回false
			}
		}

		return (state == 11);                    // 检查是否达到最终状态
	}

	/*
	 *	是否是标准分月期货合约代码
	 *	//CFFEX.IF.2007
	 */
	/**
	 * @brief 检查是否为标准分月期货合约代码
	 * 
	 * @param code 要检查的合约代码
	 * @return true 是标准分月期货代码，false 不是
	 * 
	 * 使用正则表达式验证代码格式，支持CFFEX.IF.2007等格式
	 */
	static inline bool	isStdMonthlyFutCode(const char* code)
	{
		using namespace boost::xpressive;        // 使用boost::xpressive命名空间
		/* 定义正则表达式 */
		static cregex reg_stk = cregex::compile("^[A-Z]+.[A-z]+.\\d{4}$");	//CFFEX.IO.2007
		return 	regex_match(code, reg_stk);      // 使用正则表达式匹配
	}

	/*
	 *	标准代码转标准品种ID
	 *	如SHFE.ag.1912->SHFE.ag
	 *	如果是简化的股票代码，如SSE.600000，则转成SSE.STK
	 */
	/**
	 * @brief 标准代码转标准品种ID
	 * 
	 * @param stdCode 标准合约代码
	 * @return 标准品种ID字符串
	 * 
	 * 从标准合约代码中提取品种信息，生成标准品种ID
	 */
	static inline std::string stdCodeToStdCommID2(const char* stdCode)
	{
		auto idx = find(stdCode, '.', true);     // 从后往前查找第一个分隔符
		auto idx2 = find(stdCode, '.', false);   // 从前往后查找第一个分隔符
		if (idx != idx2)                         // 检查是否为三段式代码
		{
			//前后两个.不是同一个，说明是三段的代码
			//提取前两段作为品种代码
			return std::string(stdCode, idx);    // 返回前两段作为品种ID
		}
		else
		{
			//两段的代码，直接返回
			//主要针对某些交易所，每个合约的交易规则都不同的情况
			//这种情况，就把合约直接当成品种来用
			return stdCode;                      // 返回原代码作为品种ID
		}		
	}

	/*
	 *	从基础分月合约代码提取基础品种代码
	 *	如ag1912 -> ag
	 *	这个只有分月期货品种才有意义
	 *	这个不会有永续合约的代码传到这里来，如果有的话就是调用的地方有Bug!
	 */
	/**
	 * @brief 从基础分月合约代码提取基础品种代码
	 * 
	 * @param code 基础分月合约代码
	 * @return 基础品种代码字符串
	 * 
	 * 从分月合约代码中提取品种部分，如ag1912 -> ag
	 */
	static inline std::string rawMonthCodeToRawCommID(const char* code)
	{
		int nLen = 0;                            // 初始化长度计数器
		while ('A' <= code[nLen] && code[nLen] <= 'z')  // 查找字母部分
			nLen++;                              // 计数器递增

		return std::string(code, nLen);          // 返回字母部分作为品种代码
	}

	/*
	 *	基础分月合约代码转标准码
	 *	如ag1912转成全码
	 *	这个不会有永续合约的代码传到这里来，如果有的话就是调用的地方有Bug!
	 */
	/**
	 * @brief 基础分月合约代码转标准代码
	 * 
	 * @param code 基础分月合约代码
	 * @param exchg 交易所代码
	 * @param isComm 是否为商品代码，默认为false
	 * @return 标准合约代码字符串
	 * 
	 * 将基础分月合约代码转换为标准格式，如ag1912 -> SHFE.ag.1912
	 */
	static inline std::string rawMonthCodeToStdCode(const char* code, const char* exchg, bool isComm = false)
	{
		thread_local static char buffer[64] = { 0 };  // 线程局部静态缓冲区
		std::size_t len = 0;                     // 初始化长度计数器
		if(isComm)                               // 检查是否为商品代码
		{
			len = strlen(exchg);                 // 获取交易所代码长度
			memcpy(buffer, exchg, len);          // 复制交易所代码
			buffer[len] = '.';                   // 添加分隔符
			len += 1;                            // 长度递增

			auto clen = strlen(code);            // 获取合约代码长度
			memcpy(buffer+len, code, clen);      // 复制合约代码
			len += clen;                         // 长度递增
			buffer[len] = '\0';                  // 添加字符串结尾
			len += 1;                            // 长度递增
		}
		else
		{
			std::string pid = rawMonthCodeToRawCommID(code);  // 提取品种代码
			len = strlen(exchg);                 // 获取交易所代码长度
			memcpy(buffer, exchg, len);          // 复制交易所代码
			buffer[len] = '.';                   // 添加分隔符
			len += 1;                            // 长度递增

			memcpy(buffer + len, pid.c_str(), pid.size());  // 复制品种代码
			len += pid.size();                   // 长度递增
			buffer[len] = '.';                   // 添加分隔符
			len += 1;                            // 长度递增

			char* s = (char*)code;               // 获取合约代码指针
			s += pid.size();                     // 跳过品种部分
			if (strlen(s) == 4)                  // 检查是否为4位数字
			{
				wt_strcpy(buffer + len, s, 4);   // 复制4位数字
				len += 4;                         // 长度递增
			}
			else
			{
				if (s[0] > '5')                  // 检查第一位数字
					buffer[len] = '1';            // 大于5设置为1
				else
					buffer[len] = '2';            // 小于等于5设置为2
				len += 1;                         // 长度递增
				wt_strcpy(buffer + len, s, 3);   // 复制后3位数字
				len += 3;                         // 长度递增
			}
		}

		return std::string(buffer, len);         // 返回标准合约代码
	}

	/*
	 *	原始常规代码转标准代码
	 *	这种主要针对非分月合约而言
	 */
	/**
	 * @brief 原始常规代码转标准代码
	 * 
	 * @param code 原始常规代码
	 * @param exchg 交易所代码
	 * @param pid 品种代码
	 * @return 标准合约代码字符串
	 * 
	 * 将原始常规代码转换为标准格式，主要针对非分月合约
	 */
	static inline std::string rawFlatCodeToStdCode(const char* code, const char* exchg, const char* pid)
	{
		thread_local static char buffer[64] = { 0 };  // 线程局部静态缓冲区
		auto len = strlen(exchg);                // 获取交易所代码长度
		memcpy(buffer, exchg, len);              // 复制交易所代码
		buffer[len] = '.';                       // 添加分隔符
		len += 1;                                // 长度递增

		auto plen = strlen(pid);                 // 获取品种代码长度
		auto clen = strlen(code);                // 获取合约代码长度

		if (strcmp(code, pid) == 0 || plen == 0)  // 检查合约代码是否等于品种代码或品种代码为空
		{
			memcpy(buffer + len, code, clen);    // 直接复制合约代码
			len += clen;                         // 长度递增
			buffer[len] = '\0';                  // 添加字符串结尾
		}
		else
		{
			memcpy(buffer + len, pid, plen);     // 复制品种代码
			len += plen;                         // 长度递增
			buffer[len] = '.';                   // 添加分隔符
			len += 1;                            // 长度递增

			memcpy(buffer + len, code, clen);    // 复制合约代码
			len += clen;                         // 长度递增
			buffer[len] = '\0';                  // 添加字符串结尾
		}

		return buffer;                           // 返回标准合约代码
	}

	/**
	 * @brief 检查是否为分月合约代码
	 * 
	 * @param code 要检查的合约代码
	 * @return true 是分月合约，false 不是分月合约
	 * 
	 * 通过状态机方式检查代码末尾是否为3-6位数字来判断是否为分月合约
	 */
	static inline bool isMonthlyCode(const char* code)
	{
		//using namespace boost::xpressive;
		//最后3-6位都是数字，才是分月合约
		//static cregex reg_stk = cregex::compile("^.*[A-z|-]\\d{3,6}$");	//CFFEX.IO.2007
		//return 	regex_match(code, reg_stk);
		auto len = strlen(code);                 // 获取代码长度
		char state = 0;                          // 初始化状态机状态
		for (std::size_t i = 0; i < len; i++)   // 从后往前遍历
		{
			char ch = code[len - i - 1];         // 获取从后往前的字符
			if (0 <= state && state < 3)         // 状态0-2：检查数字
			{
				if (!('0' <= ch && ch <= '9'))   // 检查是否为数字
					return false;                 // 如果不是数字，返回false

				state += 1;                       // 状态转移到下一个状态
			}
			else if (3 == state || 4 == state)   // 状态3-4：检查数字或字母
			{
				if ('0' <= ch && ch <= '9')      // 检查是否为数字
					state += 1;                   // 状态转移到下一个状态

				else if ('A' <= ch && ch <= 'z')  // 检查是否为字母
				{
					state = 7;                    // 状态转移到最终状态
					break;                        // 跳出循环
				}
			}
			else if (4 == state)                 // 状态4：检查数字或字母/连字符
			{
				if ('0' <= ch && ch <= '9')      // 检查是否为数字
					state += 1;                   // 状态转移到下一个状态

				else if (('A' <= ch && ch <= 'z') || ch == '-')  // 检查是否为字母或连字符
				{
					state = 7;                    // 状态转移到最终状态
					break;                        // 跳出循环
				}
			}
			else if (state < 6)                  // 状态5：检查数字
			{
				if ('0' <= ch && ch <= '9')      // 检查是否为数字
					state += 1;                   // 状态转移到下一个状态
				else
					return false;                 // 其他字符返回false
			}
			else if (state >= 6)                 // 状态6及以上：检查字母或连字符
			{
				if (('A' <= ch && ch <= 'z') || ch == '-')  // 检查是否为字母或连字符
				{
					state = 7;                    // 状态转移到最终状态
					break;                        // 跳出循环
				}
				else
				{
					return false;                 // 其他字符返回false
				}
			}
		}

		return state == 7;                       // 检查是否达到最终状态
	}

	/*
	 *	期货期权代码标准化
	 *	标准期货期权代码格式为CFFEX.IO2008.C.4300
	 *	-- 暂时没有地方调用 --
	 */
	/**
	 * @brief 期货期权代码标准化
	 * 
	 * @param code 原始期货期权代码
	 * @param exchg 交易所代码
	 * @return 标准期货期权代码字符串
	 * 
	 * 将原始期货期权代码转换为标准格式，支持不同交易所的代码格式
	 */
	static inline std::string rawFutOptCodeToStdCode(const char* code, const char* exchg)
	{
		using namespace boost::xpressive;        // 使用boost::xpressive命名空间
		/* 定义正则表达式 */
		static cregex reg_stk = cregex::compile("^[A-z]+\\d{4}-(C|P)-\\d+$");	//中金所、大商所格式IO2013-C-4000
		bool bMatch = regex_match(code, reg_stk);  // 使用正则表达式匹配
		if(bMatch)                               // 如果匹配成功
		{
			std::string s = std::move(fmt::format("{}.{}", exchg, code));  // 格式化标准代码
			StrUtil::replace(s, "-", ".");        // 将连字符替换为点号
			return s;                             // 返回标准代码
		}
		else
		{
			//郑商所上期所期权代码格式ZC010P11600

			//先从后往前定位到P或C的位置
			std::size_t idx = strlen(code) - 1;   // 获取代码长度减1
			for(; idx >= 0; idx--)               // 从后往前遍历
			{
				if(!isdigit(code[idx]))          // 检查是否为数字
					break;                        // 遇到非数字字符，跳出循环
			}
			
			std::string s = exchg;                // 创建结果字符串，以交易所代码开头
			s.append(".");                       // 添加分隔符
			s.append(code, idx-3);               // 添加品种代码（前3位）
			if(strcmp(exchg, "CZCE") == 0)       // 检查是否为郑商所
				s.append("2");                    // 郑商所特殊处理，添加"2"
			s.append(&code[idx - 3], 3);         // 添加月份代码（3位）
			s.append(".");                       // 添加分隔符
			s.append(&code[idx], 1);             // 添加期权类型（C或P）
			s.append(".");                       // 添加分隔符
			s.append(&code[idx + 1]);            // 添加执行价格
			return s;                             // 返回标准代码
		}
	}

	/*
	 *	标准合约代码转主力代码
	 */
	/**
	 * @brief 标准合约代码转主力代码
	 * 
	 * @param stdCode 标准合约代码
	 * @return 主力合约代码字符串
	 * 
	 * 将标准合约代码转换为对应的主力合约代码，如CFFEX.IF.2112 -> CFFEX.IF.HOT
	 */
	static inline std::string stdCodeToStdHotCode(const char* stdCode)
	{
		std::size_t idx = find(stdCode, '.', true);  // 从后往前查找分隔符
		if (idx == std::string::npos)            // 检查是否找到分隔符
			return "";                           // 未找到分隔符，返回空字符串		
		
		std::string stdWrappedCode;              // 创建包装后的代码字符串
		stdWrappedCode.resize(idx + strlen(SUFFIX_HOT) + 1);  // 调整字符串大小
		memcpy((char*)stdWrappedCode.data(), stdCode, idx);  // 复制原代码的前半部分
		wt_strcpy((char*)stdWrappedCode.data()+idx, SUFFIX_HOT);  // 添加主力合约后缀
		return stdWrappedCode;                   // 返回主力合约代码
	}

	/*
	 *	标准合约代码转次主力代码
	 */
	/**
	 * @brief 标准合约代码转次主力代码
	 * 
	 * @param stdCode 标准合约代码
	 * @return 次主力合约代码字符串
	 * 
	 * 将标准合约代码转换为对应的次主力合约代码，如CFFEX.IF.2112 -> CFFEX.IF.2ND
	 */
	static inline std::string stdCodeToStd2ndCode(const char* stdCode)
	{
		std::size_t idx = find(stdCode, '.', true);  // 从后往前查找分隔符
		if (idx == std::string::npos)            // 检查是否找到分隔符
			return "";                           // 未找到分隔符，返回空字符串

		std::string stdWrappedCode;              // 创建包装后的代码字符串
		stdWrappedCode.resize(idx + strlen(SUFFIX_2ND) + 1);  // 调整字符串大小
		memcpy((char*)stdWrappedCode.data(), stdCode, idx);  // 复制原代码的前半部分
		wt_strcpy((char*)stdWrappedCode.data() + idx, SUFFIX_2ND);  // 添加次主力合约后缀
		return stdWrappedCode;                   // 返回次主力合约代码
	}

	/*
	 *	标准期货期权代码转原代码
	 *	-- 暂时没有地方调用 --
	 */
	/**
	 * @brief 标准期货期权代码转原代码
	 * 
	 * @param stdCode 标准期货期权代码
	 * @return 原始期货期权代码字符串
	 * 
	 * 将标准期货期权代码转换回原始格式，去除交易所前缀和分隔符
	 */
	static inline std::string stdFutOptCodeToRawCode(const char* stdCode)
	{
		std::string ret = stdCode;               // 创建结果字符串
		auto pos = ret.find(".");                // 查找第一个分隔符
		ret = ret.substr(pos + 1);               // 截取分隔符后的部分
		if (strncmp(stdCode, "CFFEX", 5) == 0 || strncmp(stdCode, "DCE", 3) == 0)  // 检查是否为中金所或大商所
			StrUtil::replace(ret, ".", "-");     // 将点号替换为连字符
		else
			StrUtil::replace(ret, ".", "");      // 将点号替换为空字符串
		return ret;                              // 返回原始代码
	}

	/**
	 * @brief 获取代码中月份的位置
	 * 
	 * @param code 合约代码
	 * @return 月份在代码中的位置，如果未找到返回-1
	 * 
	 * 查找合约代码中数字部分（月份）的起始位置
	 */
	static inline int indexCodeMonth(const char* code)
	{
		if (strlen(code) == 0)                   // 检查代码是否为空
			return -1;                           // 代码为空，返回-1

		std::size_t idx = 0;                     // 初始化索引
		std::size_t len = strlen(code);          // 获取代码长度
		while(idx < len)                         // 遍历代码
		{
			if (isdigit(code[idx]))              // 检查是否为数字
				return (int)idx;                  // 找到数字，返回位置

			idx++;                               // 索引递增
		}
		return -1;                               // 未找到数字，返回-1
	}

	/*
	 *	提取标准期货期权代码的信息
	 */
	/**
	 * @brief 提取标准期货期权代码的信息
	 * 
	 * @param stdCode 标准期货期权代码
	 * @return 代码信息结构体
	 * 
	 * 从标准期货期权代码中提取交易所、合约代码、品种等信息
	 */
	static CodeInfo extractStdChnFutOptCode(const char* stdCode)
	{
		CodeInfo codeInfo;                       // 创建代码信息结构体

		StringVector ay = StrUtil::split(stdCode, ".");  // 按点号分割代码
		wt_strcpy(codeInfo._exchg, ay[0].c_str());  // 复制交易所代码
		if(strcmp(codeInfo._exchg, "SHFE") == 0 || strcmp(codeInfo._exchg, "INE") == 0)  // 检查是否为上期所或INE
		{
			fmt::format_to(codeInfo._code, "{}{}{}", ay[1], ay[2], ay[3]);  // 格式化合约代码
		}
		else if (strcmp(codeInfo._exchg, "CZCE") == 0)  // 检查是否为郑商所
		{
			std::string& s = ay[1];              // 获取品种月份部分
			fmt::format_to(codeInfo._code, "{}{}{}{}", s.substr(0, s.size()-4), s.substr(s.size()-3), ay[2], ay[3]);  // 格式化合约代码
		}
		else
		{
			fmt::format_to(codeInfo._code, "{}-{}-{}", ay[1], ay[2], ay[3]);  // 格式化合约代码
		}

		int mpos = indexCodeMonth(ay[1].c_str());  // 获取月份位置

		if(strcmp(codeInfo._exchg, "CZCE") == 0)  // 检查是否为郑商所
		{
			memcpy(codeInfo._product, ay[1].c_str(), mpos);  // 复制品种代码
			strcat(codeInfo._product, ay[2].c_str());        // 拼接品种代码
		}
		else if (strcmp(codeInfo._exchg, "CFFEX") == 0)  // 检查是否为中金所
		{
			memcpy(codeInfo._product, ay[1].c_str(), mpos);  // 复制品种代码
		}
		else
		{
			memcpy(codeInfo._product, ay[1].c_str(), mpos);  // 复制品种代码
			strcat(codeInfo._product, "_o");                  // 添加期权标识
		}

		return codeInfo;                         // 返回代码信息
	}

	/*
	 *	提起标准代码的信息
	 */
	/**
	 * @brief 提取标准代码的信息
	 * 
	 * @param stdCode 标准代码
	 * @param hotMgr 主力合约管理器指针
	 * @return 代码信息结构体
	 * 
	 * 从标准代码中提取各种信息，支持期货、期权、股票等多种产品类型
	 */
	static CodeInfo extractStdCode(const char* stdCode, IHotMgr *hotMgr)
	{
		//期权的代码规则和其他都不一样，所以单独判断
		if(isStdChnFutOptCode(stdCode))         // 检查是否为标准中国期货期权代码
		{
			return extractStdChnFutOptCode(stdCode);  // 调用期货期权代码提取方法
		}
		else
		{
			/*
			 *	By Wesley @ 2021.12.25
			 *	1、先看是不是Q和H结尾的，如果是复权标记确认以后，最后一段长度-1，复制到code，如SSE.STK.600000Q
			 *	2、再看是不是分月合约，如果是，则将product字段拼接月份给code（郑商所特殊处理），如CFFEX.IF.2112
			 *	3、最后看看是不是HOT和2ND结尾的，如果是，则将product拷贝给code，如DCE.m.HOT
			 *	4、如果都不是，则原样复制第三段，如BINANCE.DC.BTCUSDT/SSE.STK.600000
			 */
			thread_local static CodeInfo codeInfo;  // 线程局部静态代码信息结构体
			codeInfo.clear();                     // 清空结构体
			auto idx = StrUtil::findFirst(stdCode, '.');  // 查找第一个分隔符
			wt_strcpy(codeInfo._exchg, stdCode, idx);  // 复制交易所代码

			auto idx2 = StrUtil::findFirst(stdCode + idx + 1, '.');  // 查找第二个分隔符
			if (idx2 == std::string::npos)       // 检查是否为两段式代码
			{
				wt_strcpy(codeInfo._product, stdCode + idx + 1);  // 复制品种代码

				//By Wesley @ 2021.12.29
				//如果是两段的合约代码，如OKEX.BTC-USDT
				//则品种代码和合约代码一致
				wt_strcpy(codeInfo._code, stdCode + idx + 1);  // 复制合约代码
			}
			else
			{
				wt_strcpy(codeInfo._product, stdCode + idx + 1, idx2);  // 复制品种代码
				const char* ext = stdCode + idx + idx2 + 2;  // 获取扩展部分
				std::size_t extlen = strlen(ext);  // 获取扩展部分长度
				char lastCh = ext[extlen - 1];     // 获取最后一个字符
				if (lastCh == SUFFIX_QFQ || lastCh == SUFFIX_HFQ)  // 检查是否为复权标记
				{
					codeInfo._exright = (lastCh == SUFFIX_QFQ) ? 1 : 2;  // 设置复权标记

					extlen--;                      // 长度减1
					lastCh = ext[extlen - 1];      // 获取倒数第二个字符
				}
				
				if (extlen == 4 && '0' <= lastCh && lastCh <= '9')  // 检查是否为分月合约
				{
					//如果最后一段是4位数字，说明是分月合约
					//TODO: 这样的判断存在一个假设，最后一位是数字的一定是期货分月合约，以后可能会有问题，先注释一下
					//那么code得加上品种id
					//郑商所得单独处理一下，这个只能hardcode了
					auto i = wt_strcpy(codeInfo._code, codeInfo._product);  // 复制品种代码
					if (memcmp(codeInfo._exchg, "CZCE", 4) == 0)  // 检查是否为郑商所
						wt_strcpy(codeInfo._code + i, ext + 1, extlen-1);  // 郑商所特殊处理
					else
						wt_strcpy(codeInfo._code + i, ext, extlen);        // 其他交易所正常处理
				}
				else
				{
					const char* ruleTag = (hotMgr != NULL) ? hotMgr->getRuleTag(ext) :"";  // 获取规则标签
					if (strlen(ruleTag) == 0)     // 检查是否有规则标签
						wt_strcpy(codeInfo._code, ext, extlen);  // 没有规则标签，直接复制扩展部分
					else
					{
						wt_strcpy(codeInfo._code, codeInfo._product);  // 有规则标签，复制品种代码
						wt_strcpy(codeInfo._ruletag, ruleTag);         // 复制规则标签
					}
				}
			}			

			return codeInfo;                     // 返回代码信息
		}
	}
};

