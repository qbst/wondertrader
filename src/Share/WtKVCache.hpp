/**
 * @file WtKVCache.hpp
 * @brief WonderTrader键值缓存类
 * 
 * 该文件提供了高性能的键值对缓存系统，主要包括：
 * 1. 基于内存映射文件的持久化缓存存储
 * 2. 支持自动扩容的缓存管理
 * 3. 线程安全的缓存操作
 * 4. 基于哈希表的快速索引查找
 * 5. 日期感知的缓存重置机制
 * 
 * 设计逻辑：
 * - 使用Boost的内存映射文件实现持久化存储
 * - 通过自旋锁保证线程安全
 * - 使用哈希表提供O(1)的查找性能
 * - 支持自动扩容，避免缓存溢出
 * - 基于日期的缓存重置，支持每日数据更新
 * - 使用结构化的缓存块管理内存布局
 * 
 * 主要作用：
 * - 为WonderTrader框架提供高性能的数据缓存能力
 * - 支持高频交易等对性能要求极高的场景
 * - 提供持久化的缓存存储，支持程序重启后的数据恢复
 * - 通过内存映射实现高效的磁盘I/O操作
 */
#pragma once  // 防止头文件重复包含
#include "SpinMutex.hpp"  // 包含自旋锁支持
#include "BoostFile.hpp"  // 包含Boost文件操作支持
#include "BoostMappingFile.hpp"  // 包含Boost内存映射文件支持
#include "../Includes/FasterDefs.h"  // 包含Faster库定义

#define SIZE_STEP 200  // 定义缓存初始大小步长
#define CACHE_FLAG "&^%$#@!\0"  // 定义缓存文件标识符
#define FLAG_SIZE 8  // 定义标识符大小

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;  // Boost内存映射文件智能指针类型别名

#pragma warning(disable:4200)  // 禁用C4200警告（零长度数组）

NS_WTP_BEGIN  // 开始WonderTrader命名空间

typedef std::function<void(const char*)> CacheLogger;  // 缓存日志记录器函数类型定义

/**
 * @class WtKVCache
 * @brief WonderTrader键值缓存类
 * 
 * 该类提供了高性能的键值对缓存系统，支持持久化存储和自动扩容。
 * 使用内存映射文件实现高效的磁盘I/O操作，通过哈希表提供快速的查找性能。
 * 支持线程安全操作和基于日期的缓存重置。
 */
class WtKVCache
{
public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化WtKVCache对象，不执行任何特殊操作。
	 */
	WtKVCache() {}  // 默认构造函数

	/**
	 * @brief 删除拷贝构造函数
	 * 
	 * 禁止拷贝构造，确保缓存对象的唯一性。
	 */
	WtKVCache(const WtKVCache&) = delete;  // 删除拷贝构造函数

	/**
	 * @brief 删除赋值操作符
	 * 
	 * 禁止赋值操作，确保缓存对象的唯一性。
	 */
	WtKVCache& operator=(const WtKVCache&) = delete;  // 删除赋值操作符

private:
	

	/**
	 * @struct _CacheItem
	 * @brief 缓存项结构体
	 * 
	 * 该结构体表示单个缓存项，包含键和值。
	 * 键和值都使用固定长度的字符数组存储。
	 */
	typedef struct _CacheItem
	{
		char	_key[64] = { 0 };  // 缓存项键，最大64字符
		char	_val[64] = { 0 };  // 缓存项值，最大64字符
	} CacheItem;  // 缓存项类型别名

	/**
	 * @struct CacheBlock
	 * @brief 缓存块结构体
	 * 
	 * 该结构体表示一个缓存块，包含块头信息和缓存项数组。
	 * 使用零长度数组实现动态大小的缓存项存储。
	 */
	typedef struct CacheBlock
	{
		char		_blk_flag[FLAG_SIZE];  // 块标识符，用于验证文件完整性
		uint32_t	_size;  // 当前缓存项数量
		uint32_t	_capacity;  // 缓存块容量（最大缓存项数量）
		uint32_t	_date;  // 缓存日期，用于判断是否需要重置
		CacheItem	_items[0];  // 缓存项数组，零长度数组实现动态大小
	} CacheBlock;  // 缓存块类型别名

	/**
	 * @struct _CacheBlockPair
	 * @brief 缓存块和文件映射对结构体
	 * 
	 * 该结构体将缓存块与对应的内存映射文件关联起来。
	 * 提供统一的缓存访问接口。
	 */
	typedef struct _CacheBlockPair
	{
		CacheBlock*		_block;  // 缓存块指针
		BoostMFPtr		_file;   // 内存映射文件智能指针

		/**
		 * @brief 默认构造函数
		 * 
		 * 初始化缓存块对，将指针设为NULL。
		 */
		_CacheBlockPair()
		{
			_block = NULL;  // 初始化缓存块指针为NULL
			_file = NULL;   // 初始化文件指针为NULL
		}
	} CacheBlockPair;  // 缓存块对类型别名

	CacheBlockPair	_cache;  // 缓存块和文件映射对
	SpinMutex		_lock;   // 自旋锁，保证线程安全
	wt_hashmap<std::string, uint32_t> _indice;  // 哈希表索引，键到索引位置的映射

private:
	/**
	 * @brief 调整缓存大小
	 * @param newCap 新的容量大小
	 * @param logger 日志记录器，默认为nullptr
	 * @return bool 调整成功返回true，失败返回false
	 * 
	 * 该函数通过扩展文件大小来增加缓存容量。
	 * 调用前应确保线程安全，函数内部会重新映射文件。
	 */
	bool	resize(uint32_t newCap, CacheLogger logger = nullptr)
	{
		if (_cache._file == NULL)  // 检查文件是否已映射
			return false;  // 文件未映射，返回失败

		//调用该函数之前,应该保证线程安全了
		CacheBlock* cBlock = _cache._block;  // 获取当前缓存块指针
		if (cBlock->_capacity >= newCap)  // 如果当前容量已经足够
			return _cache._file->addr();  // 返回文件地址（表示成功）

		std::string filename = _cache._file->filename();  // 获取文件名
		uint64_t uOldSize = sizeof(CacheBlock) + sizeof(CacheItem)*cBlock->_capacity;  // 计算旧文件大小
		uint64_t uNewSize = sizeof(CacheBlock) + sizeof(CacheItem)*newCap;  // 计算新文件大小
		std::string data;  // 声明数据字符串
		data.resize((std::size_t)(uNewSize - uOldSize), 0);  // 调整数据大小，填充0
		try  // 使用异常处理确保安全性
		{
			BoostFile f;  // 创建Boost文件对象
			f.open_existing_file(filename.c_str());  // 打开现有文件
			f.seek_to_end();  // 移动到文件末尾
			f.write_file(data.c_str(), data.size());  // 写入扩展数据
			f.close_file();  // 关闭文件
		}
		catch (std::exception&)  // 捕获所有异常
		{
			if (logger) logger("Got an exception while resizing cache file");  // 记录调整大小异常
			return false;  // 返回失败
		}


		_cache._file.reset();  // 重置文件映射
		BoostMappingFile* pNewMf = new BoostMappingFile();  // 创建新的内存映射文件对象
		try  // 使用异常处理确保安全性
		{
			if (!pNewMf->map(filename.c_str()))  // 尝试映射文件
			{
				delete pNewMf;  // 映射失败，删除对象
				if (logger) logger("Mapping cache file failed");  // 记录映射失败
				return false;  // 返回失败
			}
		}
		catch (std::exception&)  // 捕获所有异常
		{
			if (logger) logger("Got an exception while mapping cache file");  // 记录映射异常
			return false;  // 返回失败
		}

		_cache._file.reset(pNewMf);  // 重置文件映射为新的映射文件

		_cache._block = (CacheBlock*)_cache._file->addr();  // 更新缓存块指针
		_cache._block->_capacity = newCap;  // 更新容量
		return true;  // 返回成功
	}

public:
	/**
	 * @brief 初始化缓存
	 * @param filename 缓存文件名
	 * @param uDate 缓存日期
	 * @param logger 日志记录器，默认为nullptr
	 * @return bool 初始化成功返回true，失败返回false
	 * 
	 * 该函数初始化缓存系统，如果文件不存在则创建新文件。
	 * 支持基于日期的缓存重置，自动恢复损坏的缓存文件。
	 */
	bool	init(const char* filename, uint32_t uDate, CacheLogger logger = nullptr)
	{
		bool isNew = false;  // 标记是否为新文件
		if (!BoostFile::exists(filename))  // 如果文件不存在
		{
			uint64_t uSize = sizeof(CacheBlock) + sizeof(CacheItem) * SIZE_STEP;  // 计算初始文件大小
			BoostFile bf;  // 创建Boost文件对象
			bf.create_new_file(filename);  // 创建新文件
			bf.truncate_file((uint32_t)uSize);  // 设置文件大小
			bf.close_file();  // 关闭文件

			isNew = true;  // 标记为新文件
		}

		_cache._file.reset(new BoostMappingFile);  // 创建新的内存映射文件对象
		if (!_cache._file->map(filename))  // 尝试映射文件
		{
			_cache._file.reset();  // 映射失败，重置文件对象
			if (logger) logger("Mapping cache file failed");  // 记录映射失败
			return false;  // 返回失败
		}
		_cache._block = (CacheBlock*)_cache._file->addr();  // 获取缓存块指针

		if (!isNew &&  _cache._block->_date != uDate)  // 如果不是新文件且日期不匹配
		{
			 _cache._block->_size = 0;  // 重置缓存项数量
			 _cache._block->_date = uDate;  // 更新缓存日期

			memset(& _cache._block->_items, 0, sizeof(CacheItem)* _cache._block->_capacity);  // 清空所有缓存项

			if (logger) logger("Cache file reset due to a different date");  // 记录日期重置日志
		}

		if (isNew)  // 如果是新文件
		{
			 _cache._block->_capacity = SIZE_STEP;  // 设置初始容量
			 _cache._block->_size = 0;  // 设置初始大小
			 _cache._block->_date = uDate;  // 设置缓存日期
			strcpy( _cache._block->_blk_flag, CACHE_FLAG);  // 设置块标识符
		}
		else  // 如果是现有文件
		{
			//检查缓存文件是否有问题,要自动恢复
			do  // 使用do-while循环（只执行一次）
			{
				uint64_t uSize = sizeof(CacheBlock) + sizeof(CacheItem) *  _cache._block->_capacity;  // 计算期望文件大小
				uint64_t realSz =  _cache._file->size();  // 获取实际文件大小
				if (realSz != uSize)  // 如果大小不匹配
				{
					uint32_t realCap = (uint32_t)((realSz - sizeof(CacheBlock)) / sizeof(CacheItem));  // 计算实际容量
					uint32_t markedCap =  _cache._block->_capacity;  // 获取标记容量
					//文件大小不匹配,一般是因为capacity改了,但是实际没扩容
					//这是做一次扩容即可
					 _cache._block->_capacity = realCap;  // 更新为实际容量
					 _cache._block->_size = (realCap < markedCap) ? realCap : markedCap;  // 取较小值作为实际大小
				}

			} while (false);  // 循环条件为false，只执行一次
		}

		//这里把索引加到hashmap中
		for (uint32_t i = 0; i < _cache._block->_size; i++)  // 遍历所有缓存项
			_indice[_cache._block->_items[i]._key] = i;  // 将键和索引添加到哈希表

		return true;  // 返回成功
	}

	/**
	 * @brief 清空缓存
	 * 
	 * 该函数清空所有缓存项和索引，重置缓存大小。
	 * 使用自旋锁保证线程安全。
	 */
	inline void clear()
	{
		_lock.lock();  // 获取锁

		if (_cache._block == NULL)  // 如果缓存块未初始化
			return;  // 直接返回

		_indice.clear();  // 清空索引哈希表

		memset(_cache._block->_items, 0, sizeof(CacheItem)*_cache._block->_capacity);  // 清空所有缓存项
		_cache._block->_size = 0;  // 重置缓存大小

		_lock.unlock();  // 释放锁
	}

	/**
	 * @brief 获取缓存值
	 * @param key 缓存键
	 * @return const char* 返回缓存值，如果不存在返回空字符串
	 * 
	 * 该函数通过哈希表快速查找缓存值。
	 * 线程安全，无需加锁。
	 */
	inline const char*	get(const char* key) const
	{
		auto it = _indice.find(key);  // 在哈希表中查找键
		if (it == _indice.end())  // 如果未找到
			return "";  // 返回空字符串

		return _cache._block->_items[it->second]._val;  // 返回对应的值
	}

	/**
	 * @brief 设置缓存值
	 * @param key 缓存键
	 * @param val 缓存值
	 * @param len 值长度，默认为0（自动计算）
	 * @param logger 日志记录器，默认为nullptr
	 * 
	 * 该函数设置或更新缓存值。如果键已存在则更新，否则添加新项。
	 * 支持自动扩容，使用自旋锁保证线程安全。
	 */
	void	put(const char* key, const char*val, std::size_t len = 0, CacheLogger logger = nullptr)
	{
		auto it = _indice.find(key);  // 在哈希表中查找键
		if (it != _indice.end())  // 如果键已存在
		{
			wt_strcpy(_cache._block->_items[it->second]._val, val, len);  // 更新现有值
		}
		else  // 如果键不存在
		{
			_lock.lock();  // 获取锁
			if(_cache._block->_size == _cache._block->_capacity)  // 如果缓存已满
				resize(_cache._block->_capacity*2, logger);  // 扩容为原来的2倍

			_indice[key] = _cache._block->_size;  // 添加新键到索引
			wt_strcpy(_cache._block->_items[_cache._block->_size]._key, key);  // 设置新项的键
			wt_strcpy(_cache._block->_items[_cache._block->_size]._val, val, len);  // 设置新项的值
			_cache._block->_size += 1;  // 增加缓存大小
			_lock.unlock();  // 释放锁
		}
	}

	/**
	 * @brief 检查键是否存在
	 * @param key 要检查的键
	 * @return bool 键存在返回true，不存在返回false
	 * 
	 * 该函数通过哈希表快速检查键是否存在。
	 * 线程安全，无需加锁。
	 */
	inline bool	has(const char* key) const 
	{
		return (_indice.find(key) != _indice.end());  // 在哈希表中查找键
	}

	/**
	 * @brief 获取缓存项数量
	 * @return uint32_t 返回当前缓存项数量
	 * 
	 * 该函数返回当前缓存中的项数量。
	 * 线程安全，无需加锁。
	 */
	inline uint32_t size() const
	{
		if (_cache._block == 0)  // 如果缓存块未初始化
			return 0;  // 返回0

		return _cache._block->_size;  // 返回缓存大小
	}

	/**
	 * @brief 获取缓存容量
	 * @return uint32_t 返回当前缓存容量
	 * 
	 * 该函数返回当前缓存的最大容量。
	 * 线程安全，无需加锁。
	 */
	inline uint32_t capacity() const
	{
		if (_cache._block == 0)  // 如果缓存块未初始化
			return 0;  // 返回0

		return _cache._block->_capacity;  // 返回缓存容量
	}
};

NS_WTP_END  // 结束WonderTrader命名空间