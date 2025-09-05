/*!
 * \file FasterDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 高性能哈希容器定义头文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader框架中高性能哈希容器的基础类型和模板类，用于提供高效的数据存储和查找功能。
 * 主要功能包括：
 * 1. 封装高性能哈希映射容器（robin_map和unordered_dense::map）
 * 2. 封装高性能哈希集合容器（robin_set和unordered_dense::set）
 * 3. 提供自定义字符串哈希算法（BKDRHash）
 * 4. 支持std::string类型的特化模板
 * 5. 为WonderTrader提供统一的高性能容器接口
 * 
 * 该类主要用于WonderTrader框架中的高频数据存储和查找，如合约信息缓存、订单管理等。
 * 通过性能测试和优化，选择了最适合量化交易场景的哈希容器实现。
 */
#pragma once  // 防止头文件重复包含
#include <string.h>  // 包含C风格字符串函数
#include "WTSMarcos.h"  // 包含WonderTrader宏定义
#include "../FasterLibs/tsl/robin_map.h"  // 包含robin_map高性能哈希映射容器
#include "../FasterLibs/tsl/robin_set.h"  // 包含robin_set高性能哈希集合容器

#include "../FasterLibs/ankerl/unordered_dense.h"  // 包含ankerl高性能哈希容器

/*
 *	By Wesley @ 2023.08.15
 *	很遗憾，robin_map搭配std::string在数据量大的时候（经测试在13106条数据，不同测试机可能具体数值不同）
 *	会出现bad allocate的异常
 *	我猜测是std::string无法像string那样自动优化
 *	所以数据量大的时候，就会占用非常大的内存，当运行环境内存较小时，就会出现异常
 *	所以这次把LongKey和LongKey都注释掉，改成std::string
 */

 /*
  *	By Wesley @ 2023.08.16
  *	ankerl写入速度比robin好很多，大概快1/3，尤其数据量在40w以内的时候
  *	但是robin的读取速度比robin好，不过到了30w条数据以内，差别就不大
  *	按照wondertrader的场景，还是ankerl要好很多
  * 具体可以参考以下页面的性能对比
  * https://martin.ankerl.com/2022/08/27/hashmap-bench-01/#benchmark-results-table
  */

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * @struct string_hash
 * @brief 字符串哈希函数结构体
 * 
 * 该结构体实现了BKDRHash算法，用于为std::string类型提供高效的哈希计算。
 * BKDRHash算法具有较好的分布性和计算效率，适合字符串哈希。
 * 
 * 主要特性：
 * - 使用BKDRHash算法计算字符串哈希值
 * - 支持std::string类型的哈希计算
 * - 返回31位正整数哈希值
 */
struct string_hash
{
	/**
	 * @brief 字符串哈希计算操作符
	 * @param key 要计算哈希值的字符串
	 * @return std::size_t 返回计算得到的哈希值
	 * 
	 * 该函数使用BKDRHash算法计算字符串的哈希值。
	 * BKDRHash算法通过种子值131进行迭代计算，具有良好的分布性。
	 * 
	 * 算法原理：
	 * - 使用种子值131（31、131、1313、13131、131313等）
	 * - 逐字符迭代计算：hash = hash * seed + char
	 * - 返回31位正整数结果（去掉符号位）
	 */
	std::size_t operator()(const std::string& key) const
	{
		size_t seed = 131; // 31 131 1313 13131 131313 etc..  // 设置BKDRHash算法的种子值
		size_t hash = 0;  // 初始化哈希值为0

		char* str = (char*)key.c_str();  // 获取字符串的C风格指针
		while (*str)  // 遍历字符串的每个字符
		{
			hash = hash * seed + (*str++);  // 执行BKDRHash算法：hash = hash * seed + char
		}

		return (hash & 0x7FFFFFFF);  // 返回31位正整数结果，去掉符号位
	}
};

/**
 * @class fastest_hashmap
 * @brief 最快哈希映射容器模板类
 * @tparam Key 键类型模板参数
 * @tparam T 值类型模板参数
 * 
 * 该类封装了tsl::robin_map高性能哈希映射容器，为WonderTrader提供统一的接口。
 * 对于std::string类型，使用自定义的string_hash哈希函数。
 * 
 * 主要特性：
 * - 基于tsl::robin_map实现，具有优秀的性能
 * - 支持任意键值类型
 * - 为std::string提供特化实现
 * - 继承容器类型，保持原有接口
 */
template<class Key, class T>
class fastest_hashmap : public tsl::robin_map<Key, T>  // 继承robin_map容器
{
public:
	typedef tsl::robin_map<Key, T>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类robin_map的默认构造函数初始化容器。
	 */
	fastest_hashmap():Container(){}  // 调用基类构造函数
};

/**
 * @class fastest_hashmap<std::string, T>
 * @brief 最快哈希映射容器std::string特化版本
 * @tparam T 值类型模板参数
 * 
 * 该类为std::string键类型提供特化实现，使用自定义的string_hash哈希函数。
 * 解决了std::string在robin_map中的性能问题。
 */
template<class T>
class fastest_hashmap<std::string, T> : public tsl::robin_map<std::string, T, string_hash>  // 继承robin_map，使用string_hash
{
public:
	typedef tsl::robin_map<std::string, T, string_hash>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类robin_map的构造函数，使用string_hash哈希函数。
	 */
	fastest_hashmap() :Container() {}  // 调用基类构造函数
};

/**
 * @class fastest_hashset
 * @brief 最快哈希集合容器模板类
 * @tparam Key 键类型模板参数
 * 
 * 该类封装了tsl::robin_set高性能哈希集合容器，为WonderTrader提供统一的接口。
 * 对于std::string类型，使用自定义的string_hash哈希函数。
 * 
 * 主要特性：
 * - 基于tsl::robin_set实现，具有优秀的性能
 * - 支持任意键类型
 * - 为std::string提供特化实现
 * - 继承容器类型，保持原有接口
 */
template<class Key>
class fastest_hashset : public tsl::robin_set<Key>  // 继承robin_set容器
{
public:
	typedef tsl::robin_set<Key>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类robin_set的默认构造函数初始化容器。
	 */
	fastest_hashset() :Container() {}  // 调用基类构造函数
};

/**
 * @class fastest_hashset<std::string>
 * @brief 最快哈希集合容器std::string特化版本
 * 
 * 该类为std::string键类型提供特化实现，使用自定义的string_hash哈希函数。
 * 解决了std::string在robin_set中的性能问题。
 */
template<>
class fastest_hashset<std::string> : public tsl::robin_set<std::string, string_hash>  // 继承robin_set，使用string_hash
{
public:
	typedef tsl::robin_set<std::string, string_hash>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类robin_set的构造函数，使用string_hash哈希函数。
	 */
	fastest_hashset() :Container() {}  // 调用基类构造函数
};

typedef fastest_hashset<std::string> CodeSet;  // 定义代码集合类型别名，用于存储合约代码等字符串集合

//////////////////////////////////////////////////////////////////////////
//下面使用unordered_dense

/**
 * @class wt_hashmap
 * @brief WonderTrader哈希映射容器模板类
 * @tparam Key 键类型模板参数
 * @tparam T 值类型模板参数
 * @tparam Hash 哈希函数类型模板参数，默认为std::hash<Key>
 * 
 * 该类封装了ankerl::unordered_dense::map高性能哈希映射容器，为WonderTrader提供统一的接口。
 * ankerl容器在写入性能方面优于robin_map，特别适合WonderTrader的使用场景。
 * 
 * 主要特性：
 * - 基于ankerl::unordered_dense::map实现，写入性能优秀
 * - 支持任意键值类型和哈希函数
 * - 为std::string提供特化实现
 * - 继承容器类型，保持原有接口
 */
template<class Key, class T, class Hash = std::hash<Key>>
class wt_hashmap : public ankerl::unordered_dense::map<Key, T, Hash>  // 继承ankerl哈希映射容器
{
public:
	typedef ankerl::unordered_dense::map<Key, T, Hash>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类ankerl::unordered_dense::map的默认构造函数初始化容器。
	 */
	wt_hashmap() :Container() {}  // 调用基类构造函数
};

/**
 * @class wt_hashmap<std::string, T, string_hash>
 * @brief WonderTrader哈希映射容器std::string特化版本
 * @tparam T 值类型模板参数
 * 
 * 该类为std::string键类型提供特化实现，使用自定义的string_hash哈希函数。
 * 结合ankerl容器的性能和自定义哈希函数，提供最佳的字符串键性能。
 */
template<class T>
class wt_hashmap<std::string, T, string_hash> : public ankerl::unordered_dense::map<std::string, T, string_hash>  // 继承ankerl容器，使用string_hash
{
public:
	typedef ankerl::unordered_dense::map<std::string, T, string_hash>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类ankerl::unordered_dense::map的构造函数，使用string_hash哈希函数。
	 */
	wt_hashmap() :Container() {}  // 调用基类构造函数
};

/**
 * @class wt_hashset
 * @brief WonderTrader哈希集合容器模板类
 * @tparam Key 键类型模板参数
 * @tparam Hash 哈希函数类型模板参数，默认为std::hash<Key>
 * 
 * 该类封装了ankerl::unordered_dense::set高性能哈希集合容器，为WonderTrader提供统一的接口。
 * ankerl容器在写入性能方面优于robin_set，特别适合WonderTrader的使用场景。
 * 
 * 主要特性：
 * - 基于ankerl::unordered_dense::set实现，写入性能优秀
 * - 支持任意键类型和哈希函数
 * - 为std::string提供特化实现
 * - 继承容器类型，保持原有接口
 */
template<class Key, class Hash = std::hash<Key>>
class wt_hashset : public ankerl::unordered_dense::set<Key, Hash>  // 继承ankerl哈希集合容器
{
public:
	typedef ankerl::unordered_dense::set<Key, Hash>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类ankerl::unordered_dense::set的默认构造函数初始化容器。
	 */
	wt_hashset() :Container() {}  // 调用基类构造函数
};

/**
 * @class wt_hashset<std::string, string_hash>
 * @brief WonderTrader哈希集合容器std::string特化版本
 * 
 * 该类为std::string键类型提供特化实现，使用自定义的string_hash哈希函数。
 * 结合ankerl容器的性能和自定义哈希函数，提供最佳的字符串键性能。
 */
template<>
class wt_hashset<std::string, string_hash> : public ankerl::unordered_dense::set<std::string, string_hash>  // 继承ankerl容器，使用string_hash
{
public:
	typedef ankerl::unordered_dense::set<std::string, string_hash>	Container;  // 定义容器类型别名
	/**
	 * @brief 默认构造函数
	 * 
	 * 调用基类ankerl::unordered_dense::set的构造函数，使用string_hash哈希函数。
	 */
	wt_hashset() :Container() {}  // 调用基类构造函数
};

NS_WTP_END  // 结束WonderTrader命名空间
