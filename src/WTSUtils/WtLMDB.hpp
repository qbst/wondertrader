/*!
 * \file WtLMDB.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief LMDB数据库封装类
 * 
 * 设计逻辑与作用：
 * 这个文件提供了对LMDB（Lightning Memory-Mapped Database）的C++封装，是WonderTrader
 * 数据存储和检索的核心组件。LMDB是一个高性能的嵌入式键值数据库，特别适合高频交易
 * 数据的存储需求。
 * 
 * 主要功能特性：
 * 1. 数据库管理：创建、打开、关闭LMDB环境和数据库实例
 * 2. 事务处理：支持读写事务的完整生命周期管理
 * 3. 数据操作：提供键值对的增删改查功能
 * 4. 范围查询：支持按键范围进行数据检索
 * 5. 游标操作：提供高效的数据遍历和定位功能
 * 6. 内存映射：利用操作系统的内存映射机制提高性能
 * 
 * 在量化交易系统中的应用：
 * - 历史数据存储：高效存储大量的K线、tick数据
 * - 策略参数持久化：保存策略配置和运行时状态
 * - 交易记录管理：记录订单、成交、持仓等交易信息
 * - 实时数据缓存：缓存最新的市场数据供快速访问
 * - 系统配置存储：保存系统设置和用户偏好
 */
#pragma once

#include <stdint.h>        // 标准整数类型定义
#include <string>          // C++字符串类
#include <functional>      // 函数对象支持
#include <vector>          // 动态数组容器
#include <algorithm>       // 算法库

// 平台相关的头文件包含
#if _WIN32
#include <direct.h>        // Windows目录操作
#include <io.h>           // Windows I/O操作
#else
#include <string.h>        // POSIX字符串操作
#include <unistd.h>        // POSIX标准定义
#include <sys/stat.h>      // 文件状态信息
#endif

#include "../Includes/WTSMarcos.h"  // WonderTrader宏定义
#include "lmdb/lmdb.h"              // LMDB原生API

NS_WTP_BEGIN

// 类型定义
typedef std::vector<std::string> ValueArray;                                    // 值数组类型
typedef std::function<void(const ValueArray&, const ValueArray&)> LMDBQueryCallback;  // 查询回调函数类型

/**
 * @brief LMDB数据库环境管理类
 * 
 * 这个类封装了LMDB数据库环境的创建、配置和管理功能。
 * 一个环境可以包含多个数据库实例，提供统一的资源管理。
 */
class WtLMDB
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化LMDB环境对象，设置访问模式。
	 * 
	 * @param bReadOnly 是否为只读模式，默认为false（读写模式）
	 */
	WtLMDB(bool bReadOnly = false)
		: _env(NULL)                          // LMDB环境句柄，初始化为空
		, _dbi(0)                            // 数据库实例标识符，初始化为0
		, _errno(0)                          // 错误码，初始化为0（成功）
		, _readonly(bReadOnly)               // 只读模式标志
	{}

	/**
	 * @brief 析构函数
	 * 
	 * 自动清理LMDB环境和数据库资源，确保无内存泄漏。
	 */
	~WtLMDB()
	{
		if (_dbi != 0)
			mdb_dbi_close(_env, _dbi);        // 关闭数据库实例
		
		if (_env != NULL)
			mdb_env_close(_env);              // 关闭LMDB环境
	}

public:
	/**
	 * @brief 获取LMDB环境句柄
	 * @return MDB_env* LMDB环境指针
	 */
	inline MDB_env* env() const{ return _env; }
	
	/**
	 * @brief 获取数据库实例标识符
	 * @return MDB_dbi 数据库实例标识符
	 */
	inline MDB_dbi	dbi() const { return _dbi; }

	/**
	 * @brief 更新数据库实例标识符
	 * 
	 * 在事务上下文中打开或获取数据库实例标识符。
	 * 
	 * @param txn 事务句柄
	 * @return MDB_dbi 数据库实例标识符
	 */
	inline MDB_dbi update_dbi(MDB_txn* txn)
	{
		if (_dbi != 0)
			return _dbi;                      // 如果已经打开，直接返回

		_errno = mdb_dbi_open(txn, NULL, 0, &_dbi);  // 打开默认数据库
		return _dbi;
	}

	/**
	 * @brief 打开LMDB数据库
	 * 
	 * 创建或打开指定路径的LMDB数据库环境。
	 * 
	 * @param path 数据库文件路径
	 * @param mapsize 内存映射大小，默认16MB
	 * @return bool 成功返回true，失败返回false
	 */
	bool open(const char* path, std::size_t mapsize = 16*1024*1024)
	{
		// 检查路径是否存在，不存在则创建目录
#if _MSC_VER
        int ret = _access(path, 0);           // Windows平台路径访问检查
#else
        int ret = access(path, 0);            // POSIX平台路径访问检查
#endif
		if(ret != 0)
		{
#if _WIN32
			_mkdir(path);                     // Windows创建目录
#else
			mkdir(path, 777);                 // POSIX创建目录，设置权限为777
#endif
		}

		int _errno = mdb_env_create(&_env);   // 创建LMDB环境
		if (_errno != MDB_SUCCESS)
			return false;

		_errno = mdb_env_open(_env, path, 0, 0664);  // 打开LMDB环境，设置文件权限
		if (_errno != MDB_SUCCESS)
			return false;

		_errno = mdb_env_set_mapsize(_env, mapsize); // 设置内存映射大小

		return true;
	}

	/**
	 * @brief 更新错误码
	 * @param error 错误码
	 */
	inline void update_errno(int error) { _errno = error; }

	/**
	 * @brief 检查是否有错误
	 * @return bool 有错误返回true，无错误返回false
	 */
	inline bool has_error() const { return _errno != MDB_SUCCESS; }

	/**
	 * @brief 检查是否为只读模式
	 * @return bool 只读模式返回true，读写模式返回false
	 */
	inline bool is_readonly() const { return _readonly; }

	/**
	 * @brief 获取错误消息
	 * @return const char* 错误描述字符串
	 */
	inline const char* errmsg()
	{
		return mdb_strerror(_errno);          // 将错误码转换为可读字符串
	}

private:
	MDB_env*	_env;                        // LMDB环境句柄
	MDB_dbi		_dbi;                        // 数据库实例标识符
	int			_errno;                      // 最后一次操作的错误码
	bool		_readonly;                   // 只读模式标志
};

/**
 * @brief LMDB查询和事务管理类
 * 
 * 这个类封装了LMDB事务的完整生命周期，提供数据的增删改查功能。
 * 采用RAII（Resource Acquisition Is Initialization）设计模式，
 * 确保事务资源的正确管理。
 */
class WtLMDBQuery
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * 创建一个新的LMDB事务，根据数据库的访问模式自动选择只读或读写事务。
	 * 
	 * @param db LMDB数据库环境引用
	 */
	WtLMDBQuery(WtLMDB& db)
		: _txn(NULL)                          // 事务句柄，初始化为空
		, _commited(false)                    // 提交状态标志，初始化为未提交
		, _db(db)                            // 数据库环境引用
	{
		_readonly = db.is_readonly();         // 获取数据库访问模式
		// 开始事务，根据访问模式设置事务类型
		_db.update_errno(mdb_txn_begin(_db.env(), NULL, (_readonly ? MDB_RDONLY : 0), &_txn));
		_dbi = _db.update_dbi(_txn);          // 获取数据库实例标识符
	}
	
	/**
	 * @brief 析构函数
	 * 
	 * 自动处理事务的结束，只读事务直接中止，读写事务根据状态决定提交或中止。
	 */
	~WtLMDBQuery()
	{
		if (_readonly)
			mdb_txn_abort(_txn);              // 只读事务直接中止
		else if(!_commited)
			_db.update_errno(mdb_txn_commit(_txn));  // 读写事务自动提交（如果未手动提交）
	}

public:
	/**
	 * @brief 回滚事务
	 * 
	 * 中止当前事务，丢弃所有未提交的更改。
	 */
	inline void	rollback()
	{
		if (_commited)
			return;                           // 如果已提交，无需回滚

		mdb_txn_abort(_txn);                  // 中止事务
		_commited = true;                     // 标记为已处理
	}

	/**
	 * @brief 提交事务
	 * 
	 * 将当前事务的所有更改持久化到数据库中。
	 */
	inline void	commit()
	{
		if (_commited || _readonly)
			return;                           // 如果已提交或只读模式，无需提交

		_db.update_errno(mdb_txn_commit(_txn)); // 提交事务
		_commited = true;                     // 标记为已提交
	}

	/**
	 * @brief 存储键值对（字符串版本）
	 * 
	 * 将字符串键值对存储到数据库中。
	 * 
	 * @param key 键字符串
	 * @param val 值字符串
	 * @return bool 成功返回true，失败返回false
	 */
	bool put(const std::string& key, const std::string& val)
	{
		return put((void*)key.data(), key.size(), (void*)val.data(), val.size());
	}

	/**
	 * @brief 存储键值对并立即提交（字符串版本）
	 * 
	 * 将字符串键值对存储到数据库中并立即提交事务。
	 * 
	 * @param key 键字符串
	 * @param val 值字符串
	 * @return bool 成功返回true，失败返回false
	 */
	bool put_and_commit(const std::string& key, const std::string& val)
	{
		return put_and_commit((void*)key.data(), key.size(), (void*)val.data(), val.size());
	}

	/**
	 * @brief 存储键值对（原始数据版本）
	 * 
	 * 将原始二进制数据作为键值对存储到数据库中。
	 * 
	 * @param key 键数据指针
	 * @param klen 键数据长度
	 * @param val 值数据指针
	 * @param vlen 值数据长度
	 * @return bool 成功返回true，失败返回false
	 */
	bool put(void* key, std::size_t klen, void* val, std::size_t vlen)
	{
		MDB_val mKey, mData;                  // LMDB键值结构体
		mKey.mv_data = key;                   // 设置键数据
		mKey.mv_size = klen;                  // 设置键长度

		mData.mv_data = val;                  // 设置值数据
		mData.mv_size = vlen;                 // 设置值长度
		int _errno = mdb_put(_txn, _dbi, &mKey, &mData, 0);  // 执行存储操作
		_db.update_errno(_errno);             // 更新错误状态
		return (_errno == MDB_SUCCESS);       // 返回操作结果
	}

	/**
	 * @brief 存储键值对并立即提交（原始数据版本）
	 * 
	 * 将原始二进制数据作为键值对存储到数据库中并立即提交事务。
	 * 
	 * @param key 键数据指针
	 * @param klen 键数据长度
	 * @param val 值数据指针
	 * @param vlen 值数据长度
	 * @return bool 成功返回true，失败返回false
	 */
	bool put_and_commit(void* key, std::size_t klen, void* val, std::size_t vlen)
	{
		MDB_val mKey, mData;                  // LMDB键值结构体
		mKey.mv_data = key;                   // 设置键数据
		mKey.mv_size = klen;                  // 设置键长度

		mData.mv_data = val;                  // 设置值数据
		mData.mv_size = vlen;                 // 设置值长度
		int _errno = mdb_put(_txn, _dbi, &mKey, &mData, 0);  // 执行存储操作
		_db.update_errno(_errno);             // 更新错误状态
		if (_errno != MDB_SUCCESS)
			return false;                     // 存储失败直接返回

		_errno = mdb_txn_commit(_txn);        // 立即提交事务
		_db.update_errno(_errno);             // 更新错误状态
		_commited = true;                     // 标记为已提交
		return (_errno == MDB_SUCCESS);       // 返回提交结果
	}

	/**
	 * @brief 读取键对应的值（字符串版本）
	 * 
	 * 根据键字符串查找并返回对应的值。
	 * 
	 * @param key 键字符串
	 * @return std::string 对应的值字符串，不存在则返回空字符串
	 */
	std::string get(const std::string& key)
	{
		return get((void*)key.data(), key.size());
	}

	/**
	 * @brief 读取指定键的数据（原始数据版本）
	 * 
	 * 根据原始键数据查找并返回对应的值。
	 * 使用游标进行高效的数据检索。
	 * 
	 * @param key 键数据指针
	 * @param klen 键数据长度
	 * @return std::string 对应的值字符串，不存在则返回空字符串
	 */
	std::string get(void* key, std::size_t klen)
	{
		MDB_cursor* cursor;                   // 数据库游标
		int _errno = mdb_cursor_open(_txn, _dbi, &cursor);  // 打开游标
		_db.update_errno(_errno);             // 更新错误状态
		if (_errno != MDB_SUCCESS)
			return std::move(std::string());  // 打开失败返回空字符串

		MDB_val mKey, mData;                  // LMDB键值结构体
		mKey.mv_data = key;                   // 设置查找键
		mKey.mv_size = klen;                  // 设置键长度

		_errno = mdb_cursor_get(cursor, &mKey, &mData, MDB_NEXT);  // 查找数据
		_db.update_errno(_errno);             // 更新错误状态
		if (_errno != MDB_SUCCESS)
			return std::move(std::string());  // 查找失败返回空字符串

		auto ret = std::string((char*)mData.mv_data, mData.mv_size);  // 构造结果字符串
		mdb_cursor_close(cursor);             // 关闭游标
		return std::move(ret);                // 返回查找结果
	}

	/*
	 *	读取区间数据
	 */
	int get_range(const std::string& lower_key, const std::string& upper_key, LMDBQueryCallback cb)
	{
		MDB_cursor* cursor;
		int _errno = mdb_cursor_open(_txn, _dbi, &cursor);
		_db.update_errno(_errno);
		if (_errno != MDB_SUCCESS)
			return 0;

		MDB_val lKey, rKey, mData;
		lKey.mv_data = (void*)lower_key.data();
		lKey.mv_size = lower_key.size();

		rKey.mv_data = (void*)upper_key.data();
		rKey.mv_size = upper_key.size();
		
		if (_errno != MDB_SUCCESS)
			return 0;

		int cnt = 0;
		MDB_cursor_op op = MDB_SET_RANGE;
		std::vector<std::string> ayKeys, ayVals;
		for(; (_errno = mdb_cursor_get(cursor, &lKey, &mData, op))==MDB_SUCCESS;)
		{
			_db.update_errno(_errno);
			if(_errno == MDB_NOTFOUND)
				break;

			if(memcmp(lKey.mv_data, rKey.mv_data, lKey.mv_size) > 0)
				break;

			//回调
			//cb(std::string((char*)lKey.mv_data, lKey.mv_size), std::string((char*)mData.mv_data, mData.mv_size), false);
			ayKeys.emplace_back(std::string((char*)lKey.mv_data, lKey.mv_size));
			ayVals.emplace_back(std::string((char*)mData.mv_data, mData.mv_size));
			cnt++;
			op = MDB_NEXT;
		} 

		cb(ayKeys, ayVals);
		mdb_cursor_close(cursor);
		return cnt;
	}

	/*
	 *	读取upper_key之前的数据，从upper_key往前找，找到以后在做一个reverse
	 *	@lower_key	下边界，这个必须要有，因为如果多个合约存一个库的话，不加的话可能会读到别的合约的数据
	 *	@upper_key	上边界
	 *	@count		目标数据条数
	 *	@cb			回调函数
	 */
	int get_lowers(const std::string& lower_key, const std::string& upper_key, int count, LMDBQueryCallback cb)
	{
		MDB_cursor* cursor;
		int _errno = mdb_cursor_open(_txn, _dbi, &cursor);
		_db.update_errno(_errno);
		if (_errno != MDB_SUCCESS)
			return 0;

		MDB_val rKey, mData;
		rKey.mv_data = (void*)upper_key.data();
		rKey.mv_size = upper_key.size();

		int cnt = 0;
		std::vector<std::string> ayKeys, ayVals;
		_errno = mdb_cursor_get(cursor, &rKey, &mData, MDB_SET_RANGE);
		_db.update_errno(_errno);

		if (_errno == MDB_NOTFOUND)
		{
			_errno = mdb_cursor_get(cursor, &rKey, &mData, MDB_LAST);
			_db.update_errno(_errno);
		}

		for (; _errno != MDB_NOTFOUND;)
		{
			//往前查找，所以如果拿到的key，比右边界大，则直接往前退回一条
			if (memcmp(rKey.mv_data, upper_key.data(), upper_key.size()) > 0)
			{
				_errno = mdb_cursor_get(cursor, &rKey, &mData, MDB_PREV);
				_db.update_errno(_errno);
				continue;
			}

			if (memcmp(rKey.mv_data, lower_key.data(), lower_key.size()) < 0)
				break;

			//回调
			ayKeys.emplace_back(std::string((char*)rKey.mv_data, rKey.mv_size));
			ayVals.emplace_back(std::string((char*)mData.mv_data, mData.mv_size));
			cnt++;

			//如果找到目标数量，则退出
			if(cnt == count)
				break;
			
			_errno = mdb_cursor_get(cursor, &rKey, &mData, MDB_PREV);
			_db.update_errno(_errno);
		}

		//向前查找，是逆序的，需要做一个reverse
		std::reverse(ayKeys.begin(), ayKeys.end());
		std::reverse(ayVals.begin(), ayVals.end());
		cb(ayKeys, ayVals);
		mdb_cursor_close(cursor);
		return cnt;
	}

	/*
	 *	读取lower_key之后的数据，从lower_key往后找
	 *	@lower_key	下边界
	 *	@upper_key	上边界，这个必须要有，因为如果多个合约存一个库的话，不加的话可能会读到别的合约的数据
	 *	@count		目标数据条数
	 *	@cb			回调函数
	 */
	int get_uppers(const std::string& lower_key, const std::string& upper_key, int count, LMDBQueryCallback cb)
	{
		MDB_cursor* cursor;
		int _errno = mdb_cursor_open(_txn, _dbi, &cursor);
		if (_errno != MDB_SUCCESS)
			return 0;

		MDB_val bKey, mData;
		bKey.mv_data = (void*)lower_key.data();
		bKey.mv_size = lower_key.size();

		int cnt = 0;
		std::vector<std::string> ayKeys, ayVals;
		_errno = mdb_cursor_get(cursor, &bKey, &mData, MDB_SET_RANGE);
		_db.update_errno(_errno);
		for (; _errno != MDB_NOTFOUND;)
		{
			if (memcmp(bKey.mv_data, upper_key.data(), upper_key.size()) > 0)
				break;

			//回调
			ayKeys.emplace_back(std::string((char*)bKey.mv_data, bKey.mv_size));
			ayVals.emplace_back(std::string((char*)mData.mv_data, mData.mv_size));
			cnt++;

			//如果找到目标数量，则退出
			if (cnt == count)
				break;

			_errno = mdb_cursor_get(cursor, &bKey, &mData, MDB_NEXT);
			_db.update_errno(_errno);
		}

		cb(ayKeys, ayVals);
		mdb_cursor_close(cursor);
		return cnt;
	}

	inline int get_all(LMDBQueryCallback cb)
	{
		MDB_cursor* cursor;
		int _errno = mdb_cursor_open(_txn, _dbi, &cursor);
		if (_errno != MDB_SUCCESS)
			return 0;

		MDB_val bKey, mData;
		std::vector<std::string> ayKeys, ayVals;
		for (; _errno != MDB_NOTFOUND;)
		{
			_errno = mdb_cursor_get(cursor, &bKey, &mData, MDB_NEXT);
			_db.update_errno(_errno);

			ayKeys.emplace_back(std::string((const char*)bKey.mv_data, bKey.mv_size));
			ayVals.emplace_back(std::string((const char*)mData.mv_data, mData.mv_size));
		}
		cb(ayKeys, ayVals);
		return (int)ayVals.size();
	}

private:
	WtLMDB&		_db;                         // 数据库环境引用
	MDB_txn*	_txn;                        // 当前事务句柄
	MDB_dbi		_dbi;                        // 数据库实例标识符
	bool		_readonly;                   // 只读模式标志
	bool		_commited;                   // 事务提交状态标志
};

NS_WTP_END
