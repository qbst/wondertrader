/*!
 * \file WTSCollection.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader集合组件定义文件
 * 
 * 本文件定义了WonderTrader框架中各种集合容器的实现。
 * 这些集合容器基于STL容器，但增加了引用计数管理、类型安全等特性。
 * 主要用于平台内部的数据存储和管理，确保内存安全和数据一致性。
 * 
 * 主要包含：
 * 1. WTSArray类：平台数组容器，支持引用计数管理
 * 2. WTSCollection类：通用集合容器，支持多种数据类型
 * 3. 相关的迭代器和操作函数
 * 4. 类型安全的模板方法
 */
#pragma once
#include "WTSObject.hpp"    // WonderTrader对象基类
#include <vector>            // STL向量容器
#include <map>               // STL映射容器
#include <functional>        // 函数对象支持
#include <algorithm>         // 算法库
#include "FasterDefs.h"      // 快速定义文件

#include <deque>             // STL双端队列

NS_WTP_BEGIN

//////////////////////////////////////////////////////////////////////////
//WTSArray

/**
 * @brief 平台数组容器类
 * 
 * 该类是WonderTrader平台内部的数组容器，内部使用vector实现。
 * 数据使用WTSObject指针对象，所有WTSObject的派生类都可以使用。
 * 该类提供了引用计数管理、类型安全访问等特性，确保内存安全。
 * 主要用于平台内部使用，为各种数据提供统一的存储接口。
 */
class WTSArray : public WTSObject
{
public:
	/**
	 * @brief 数组迭代器类型定义
	 */
	typedef std::vector<WTSObject*>::iterator Iterator;                    // 正向迭代器
	typedef std::vector<WTSObject*>::const_iterator ConstIterator;         // 常量正向迭代器

	typedef std::vector<WTSObject*>::reverse_iterator ReverseIterator;     // 反向迭代器
	typedef std::vector<WTSObject*>::const_reverse_iterator ConstReverseIterator;  // 常量反向迭代器

	/**
	 * @brief 排序函数类型定义
	 * 
	 * 用于自定义数组元素的排序规则
	 */
	typedef std::function<bool(WTSObject*, WTSObject*)>	SortFunc;

	/**
	 * @brief 创建数组对象
	 * @return 新创建的数组对象指针
	 * 
	 * 静态工厂方法，用于创建新的数组对象实例。
	 */
	static WTSArray* create()
	{
		WTSArray* pRet = new WTSArray();
		return pRet;
	}

	/**
	 * @brief 读取数组长度
	 * @return 数组当前包含的元素数量
	 */
	inline
	uint32_t size() const{ return (uint32_t)_vec.size(); }

	/**
	 * @brief 调整数组大小
	 * @param _size 新的数组大小
	 * 
	 * 清空数组并重新分配空间，调用该函数会预先分配长度。
	 * 预先分配好的数据都是NULL，需要后续设置。
	 */
	void resize(uint32_t _size)
	{
		if(!_vec.empty())
			clear();

		_vec.resize(_size, NULL);
	}

	/**
	 * @brief 读取数组指定位置的数据
	 * @param idx 数组索引位置
	 * @return 指定位置的WTSObject指针，越界返回NULL
	 * 
	 * 对比grab接口，at接口只取得数据，不增加数据的引用计数。
	 * grab接口读取数据以后，会增加引用计数。
	 */
	inline
	WTSObject* at(uint32_t idx)
	{
		if(idx <0 || idx >= _vec.size())
			return NULL;

		WTSObject* pRet = _vec.at(idx);
		return pRet;
	}

	/**
	 * @brief 查找对象在数组中的索引位置
	 * @param obj 要查找的对象指针
	 * @return 对象在数组中的索引位置，未找到返回-1
	 */
	inline
	uint32_t idxOf(WTSObject* obj)
	{
		if (obj == NULL)
			return -1;

		uint32_t idx = 0;
		auto it = _vec.begin();
		for (; it != _vec.end(); it++, idx++)
		{
			if (obj == (*it))
				return idx;
		}

		return -1;
	}

	/**
	 * @brief 模板方法：读取数组指定位置的数据并转换为指定类型
	 * @param idx 数组索引位置
	 * @return 指定类型的对象指针，越界或类型不匹配返回NULL
	 * 
	 * 该模板方法提供了类型安全的数组访问，自动进行类型转换。
	 */
	template<typename T> 
	inline T* at(uint32_t idx)
	{
		if(idx <0 || idx >= _vec.size())
			return NULL;

		WTSObject* pRet = _vec.at(idx);
		return static_cast<T*>(pRet);
	}

	/**
	 * @brief 数组下标操作符重载
	 * @param idx 数组索引位置
	 * @return 指定位置的WTSObject指针，越界返回NULL
	 * 
	 * 用法同at函数，提供更直观的数组访问方式。
	 */
	inline
	WTSObject* operator [](uint32_t idx)
	{
		if(idx <0 || idx >= _vec.size())
			return NULL;

		WTSObject* pRet = _vec.at(idx);
		return pRet;
	}

	/**
	 * @brief 读取数组指定位置的数据并增加引用计数
	 * @param idx 数组索引位置
	 * @return 指定位置的WTSObject指针，越界返回NULL
	 * 
	 * 该接口会调用对象的retain()方法增加引用计数，
	 * 调用者需要负责调用release()方法释放引用。
	 */
	inline
	WTSObject*	grab(uint32_t idx)
	{
		if(idx <0 || idx >= _vec.size())
			return NULL;

		WTSObject* pRet = _vec.at(idx);
		if (pRet)
			pRet->retain();

		return pRet;
	}

	/**
	 * @brief 数组末尾追加数据
	 * @param obj 要追加的对象指针
	 * @param bAutoRetain 是否自动增加引用计数，默认为true
	 * 
	 * 数据会自动增加到数组末尾，如果bAutoRetain为true，
	 * 则自动调用对象的retain()方法增加引用计数。
	 */
	inline
	void append(WTSObject* obj, bool bAutoRetain = true)
	{
		if (bAutoRetain && obj)
			obj->retain();

		_vec.emplace_back(obj);
	}

	/**
	 * @brief 设置指定位置的数据
	 * @param idx 数组索引位置
	 * @param obj 要设置的对象指针
	 * @param bAutoRetain 是否自动增加引用计数，默认为true
	 * 
	 * 如果该位置已有数据，则释放掉原有对象的引用。
	 * 新数据如果bAutoRetain为true，则引用计数增加。
	 */
	inline
	void set(uint32_t idx, WTSObject* obj, bool bAutoRetain = true)
	{
		if(idx >= _vec.size() || obj == NULL)
			return;

		if(bAutoRetain)
			obj->retain();

		WTSObject* oldObj = _vec.at(idx);
		if(oldObj)
			oldObj->release();

		_vec[idx] = obj;
	}

	/**
	 * @brief 追加另一个数组的所有元素
	 * @param ay 要追加的数组指针
	 * 
	 * 将另一个数组的所有元素追加到当前数组末尾，
	 * 然后清空源数组。
	 */
	inline
	void append(WTSArray* ay)
	{
		if(ay == NULL)
			return;

		_vec.insert(_vec.end(), ay->_vec.begin(), ay->_vec.end());
		ay->_vec.clear();
	}

	/**
	 * @brief 数组清空
	 * 
	 * 数组内所有数据释放引用，调用对象的release()方法。
	 * 清空后数组长度为0，但保留已分配的内存空间。
	 */
	void clear()
	{
		{
			std::vector<WTSObject*>::iterator it = _vec.begin();
			for (; it != _vec.end(); it++)
			{
				WTSObject* obj = (*it);
				if (obj)
					obj->release();
			}
		}
		
		_vec.clear();
	}

	/**
	 * @brief 释放数组对象
	 * 
	 * 该方法用于释放WTSArray对象，如果引用计数为1，
	 * 则释放所有数据并删除对象。
	 */
	virtual void release()
	{
		if (m_uRefs == 0)
			return;

		try
		{
			m_uRefs--;
			if (m_uRefs == 0)
			{
				clear();
				delete this;
			}
		}
		catch(...)
		{

		}
	}

	/**
	 * @brief 取得数组对象起始位置的迭代器
	 * @return 数组起始位置的迭代器
	 */
	inline
	Iterator begin()
	{
		return _vec.begin();
	}

	/**
	 * @brief 取得数组对象起始位置的常量迭代器
	 * @return 数组起始位置的常量迭代器
	 */
	inline
	ConstIterator begin() const
	{
		return _vec.begin();
	}

	/**
	 * @brief 取得数组对象末尾位置的反向迭代器
	 * @return 数组末尾位置的反向迭代器
	 */
	inline
	ReverseIterator rbegin()
	{
		return _vec.rbegin();
	}

	/**
	 * @brief 取得数组对象末尾位置的常量反向迭代器
	 * @return 数组末尾位置的常量反向迭代器
	 */
	inline
	ConstReverseIterator rbegin() const
	{
		return _vec.rbegin();
	}

	/**
	 * @brief 取得数组对象末尾位置的迭代器
	 * @return 数组末尾位置的迭代器
	 */
	inline
	Iterator end()
	{
		return _vec.end();
	}

	/**
	 * @brief 取得数组对象末尾位置的常量迭代器
	 * @return 数组末尾位置的常量迭代器
	 */
	inline
	ConstIterator end() const
	{
		return _vec.end();
	}

	/**
	 * @brief 取得数组对象末尾位置的反向迭代器
	 * @return 数组末尾位置的反向迭代器
	 */
	inline
	ReverseIterator rend()
	{
		return _vec.rend();
	}

	/**
	 * @brief 取得数组对象末尾位置的常量反向迭代器
	 * @return 数组末尾位置的常量反向迭代器
	 */
	inline
	ConstReverseIterator rend() const
	{
		return _vec.rend();
	}

	/**
	 * @brief 对数组元素进行排序
	 * @param func 排序函数对象
	 * 
	 * 使用指定的排序函数对数组元素进行排序。
	 */
	inline
	void	sort(SortFunc func)
	{
		std::sort(_vec.begin(), _vec.end(), func);
	}

protected:
	/**
	 * @brief 构造函数
	 * 
	 * 初始化WTSArray对象，设置引用计数为0。
	 */
	WTSArray():_holding(false){}
	
	/**
	 * @brief 析构函数
	 * 
	 * 释放WTSArray对象，但不会释放其内部数据。
	 */
	virtual ~WTSArray(){}

	std::vector<WTSObject*>	_vec;        // 内部存储的向量容器
	std::atomic<bool>		_holding;     // 原子布尔值，用于线程安全控制
};


/**
 * @brief 模板Map容器类
 * 
 * 该类是WonderTrader平台内部的Map容器，内部采用std::map实现。
 * 模板类型为key类型，数据使用WTSObject指针对象。
 * 所有WTSObject的派生类都适用，提供引用计数管理和类型安全访问。
 * 
 * @tparam T key的类型，可以是任何支持比较操作的类型
 */
template <class T>
class WTSMap : public WTSObject
{
public:
	/**
	 * @brief 容器迭代器的类型定义
	 */
	typedef typename std::map<T, WTSObject*>::_MyType;                    // 内部map类型
	typedef typename _MyType::iterator			Iterator;                   // 正向迭代器
	typedef typename _MyType::const_iterator	ConstIterator;              // 常量正向迭代器
	typedef typename _MyType::reverse_iterator			ReverseIterator;      // 反向迭代器
	typedef typename _MyType::const_reverse_iterator	ConstReverseIterator;  // 常量反向迭代器

	/**
	 * @brief 创建map容器
	 * @return 新创建的map容器对象指针
	 * 
	 * 静态工厂方法，用于创建新的map容器实例。
	 */
	static WTSMap<T>*	create()
	{
		WTSMap<T>* pRet = new WTSMap<T>();
		return pRet;
	}

	/**
	 * @brief 返回map容器的大小
	 * @return 容器当前包含的键值对数量
	 */
	inline
	uint32_t size() const{ return (uint32_t)_map.size(); }

	/**
	 * @brief 读取指定key对应的数据
	 * @param _key 要查找的键
	 * @return 对应的WTSObject指针，没有则返回NULL
	 * 
	 * 不增加数据的引用计数，仅获取数据指针。
	 */
	inline
	WTSObject* get(const T &_key)
	{
		Iterator it = _map.find(_key);
		if(it == _map.end())
			return NULL;

		WTSObject* pRet = it->second;
		return pRet;
	}

	/**
	 * @brief 下标操作符重载
	 * @param _key 要查找的键
	 * @return 对应的WTSObject指针，没有则返回NULL
	 * 
	 * 用法同get函数，提供更直观的访问方式。
	 */
	inline
	WTSObject* operator[](const T &_key)
	{
		Iterator it = _map.find(_key);
		if(it == _map.end())
			return NULL;

		WTSObject* pRet = it->second;
		return pRet;
	}

	/**
	 * @brief 读取指定key对应的数据并增加引用计数
	 * @param _key 要查找的键
	 * @return 对应的WTSObject指针，没有则返回NULL
	 * 
	 * 增加数据的引用计数，调用者需要负责调用release()方法释放引用。
	 */
	inline
	WTSObject* grab(const T &_key)
	{
		Iterator it = _map.find(_key);
		if(it == _map.end())
			return NULL;

		WTSObject* pRet = it->second;
		if (pRet)
			pRet->retain();

		return pRet;
	}

	/**
	 * @brief 新增一个数据
	 * @param _key 键
	 * @param obj 要添加的对象指针
	 * @param bAutoRetain 是否自动增加引用计数，默认为true
	 * 
	 * 如果key存在，则将原有数据释放，新数据如果bAutoRetain为true则引用计数增加。
	 */
	inline
	void add(T _key, WTSObject* obj, bool bAutoRetain = true)
	{
		if(bAutoRetain && obj)
			obj->retain();

		WTSObject* pOldObj = NULL;
		Iterator it = _map.find(_key);
		if(it != _map.end())
		{
			pOldObj = it->second;
		}

		_map[_key] = obj;

		if (pOldObj) pOldObj->release();
	}

	/**
	 * @brief 根据key删除一个数据
	 * @param _key 要删除的键
	 * 
	 * 如果key存在，则对应数据引用计数-1。
	 */
	inline
	void remove(T _key)
	{
		Iterator it = _map.find(_key);
		if(it != _map.end())
		{
			WTSObject* obj = it->second;
			_map.erase(it);
			if (obj) obj->release();
		}
	}

	/**
	 * @brief 获取容器起始位置的迭代器
	 * @return 容器起始位置的正向迭代器
	 */
	Iterator begin()
	{
		return _map.begin();
	}

	/**
	 * @brief 获取容器起始位置的常量迭代器
	 * @return 容器起始位置的常量正向迭代器
	 */
	ConstIterator begin() const
	{
		return _map.begin();
	}

	/**
	 * @brief 获取容器末尾位置的迭代器
	 * @return 容器末尾位置的正向迭代器
	 */
	Iterator end()
	{
		return _map.end();
	}

	/**
	 * @brief 获取容器末尾位置的常量迭代器
	 * @return 容器末尾位置的常量正向迭代器
	 */
	ConstIterator end() const
	{
		return _map.end();
	}

	/**
	 * @brief 获取容器起始位置的反向迭代器
	 * @return 容器起始位置的反向迭代器
	 */
	ReverseIterator rbegin()
	{
		return _map.rbegin();
	}

	/**
	 * @brief 获取容器起始位置的常量反向迭代器
	 * @return 容器起始位置的常量反向迭代器
	 */
	ConstReverseIterator rbegin() const
	{
		return _map.rbegin();
	}

	/**
	 * @brief 获取容器末尾位置的反向迭代器
	 * @return 容器末尾位置的反向迭代器
	 */
	ReverseIterator rend()
	{
		return _map.rend();
	}

	/**
	 * @brief 获取容器末尾位置的常量反向迭代器
	 * @return 容器末尾位置的常量反向迭代器
	 */
	ConstReverseIterator rend() const
	{
		return _map.rend();
	}

	/**
	 * @brief 查找指定key的迭代器
	 * @param key 要查找的键
	 * @return 找到的迭代器，未找到返回end()
	 */
	inline
	Iterator find(const T& key)
	{
		return _map.find(key);
	}

	/**
	 * @brief 查找指定key的常量迭代器
	 * @param key 要查找的键
	 * @return 找到的常量迭代器，未找到返回end()
	 */
	inline
	ConstIterator find(const T& key) const
	{
		return _map.find(key);
	}

	/**
	 * @brief 根据迭代器删除元素
	 * @param it 要删除元素的迭代器
	 */
	inline
	void erase(ConstIterator it)
	{
		_map.erase(it);
	}

	/**
	 * @brief 根据迭代器删除元素
	 * @param it 要删除元素的迭代器
	 */
	inline
	void erase(Iterator it)
	{
		_map.erase(it);
	}

	/**
	 * @brief 根据key删除元素
	 * @param key 要删除的键
	 */
	inline
	void erase(T key)
	{
		_map.erase(key);
	}

	/**
	 * @brief 查找第一个不小于指定key的迭代器
	 * @param key 要查找的键
	 * @return 第一个不小于指定key的迭代器
	 */
	Iterator lower_bound(const T& key)
	{
		 return _map.lower_bound(key);
	}

	/**
	 * @brief 查找第一个不小于指定key的常量迭代器
	 * @param key 要查找的键
	 * @return 第一个不小于指定key的常量迭代器
	 */
	ConstIterator lower_bound(const T& key) const
	{
		return _map.lower_bound(key);
	}

	/**
	 * @brief 查找第一个大于指定key的迭代器
	 * @param key 要查找的键
	 * @return 第一个大于指定key的迭代器
	 */
	Iterator upper_bound(const T& key)
	{
	 	 return _map.upper_bound(key);
	}
	 
	/**
	 * @brief 查找第一个大于指定key的常量迭代器
	 * @param key 要查找的键
	 * @return 第一个大于指定key的常量迭代器
	 */
	ConstIterator upper_bound(const T& key) const
	{
		return _map.upper_bound(key);
	}

	/**
	 * @brief 获取容器中最后一个元素
	 * @return 最后一个元素的WTSObject指针，容器为空返回NULL
	 */
	inline
	WTSObject* last() 
	{
		if(_map.empty())
			return NULL;
		
		return _map.rbegin()->second;
	}
	

	/**
	 * @brief 清空容器
	 * 
	 * 容器内所有数据引用计数-1，然后清空容器。
	 */
	void clear()
	{
		Iterator it = _map.begin();
		for(; it != _map.end(); it++)
		{
			it->second->release();
		}
		_map.clear();
	}

	/**
	 * @brief 释放容器对象
	 * 
	 * 如果容器引用计数为1，则清空所有数据并删除对象。
	 */
	virtual void release()
	{
		if (m_uRefs == 0)
			return;

		try
		{
			m_uRefs--;
			if (m_uRefs == 0)
			{
				clear();
				delete this;
			}
		}
		catch(...)
		{

		}
	}

protected:
	/**
	 * @brief 默认构造函数
	 */
	WTSMap(){}
	
	/**
	 * @brief 析构函数
	 */
	~WTSMap(){}

	std::map<T, WTSObject*>	_map;    // 内部存储的map容器
};

/**
 * @brief 模板哈希Map容器类
 * 
 * 该类是WonderTrader平台内部的哈希Map容器，内部采用wt_hashmap实现。
 * 模板类型为key类型，数据使用WTSObject指针对象。
 * 所有WTSObject的派生类都适用，提供哈希查找和引用计数管理。
 * 
 * @tparam T key的类型
 * @tparam Hash 哈希函数类型，默认为std::hash<T>
 */
template <typename T, class Hash = std::hash<T>>
class WTSHashMap : public WTSObject
{
protected:
	/**
	 * @brief 默认构造函数
	 */
	WTSHashMap() {}
	
	/**
	 * @brief 虚析构函数
	 */
	virtual ~WTSHashMap() {}

	//std::unordered_map<T, WTSObject*>	_map;    // 标准哈希map（已注释）
	wt_hashmap<T, WTSObject*, Hash>	_map;        // 快速哈希map

public:
	/**
	 * @brief 容器迭代器的类型定义
	 */
	typedef wt_hashmap<T, WTSObject*, Hash>		_MyType;        // 内部哈希map类型
	typedef typename _MyType::const_iterator	ConstIterator;   // 常量迭代器

	/**
	 * @brief 创建哈希map容器
	 * @return 新创建的哈希map容器对象指针
	 * 
	 * 静态工厂方法，用于创建新的哈希map容器实例。
	 */
	static WTSHashMap<T, Hash>*	create() noexcept
	{
		WTSHashMap<T, Hash>* pRet = new WTSHashMap<T, Hash>();
		return pRet;
	}

	/**
	 * @brief 返回哈希map容器的大小
	 * @return 容器当前包含的键值对数量
	 */
	inline uint32_t size() const noexcept {return (uint32_t)_map.size();}

	/**
	 * @brief 读取指定key对应的数据
	 * @param _key 要查找的键
	 * @return 对应的WTSObject指针，没有则返回NULL
	 * 
	 * 不增加数据的引用计数，仅获取数据指针。
	 */
	inline WTSObject* get(const T &_key) noexcept
	{
		auto it = _map.find(_key);
		if(it == _map.end())
			return NULL;

		WTSObject* pRet = it->second;
		return pRet;
	}

	/**
	 * @brief 读取指定key对应的数据并增加引用计数
	 * @param _key 要查找的键
	 * @return 对应的WTSObject指针，没有则返回NULL
	 * 
	 * 增加数据的引用计数，调用者需要负责调用release()方法释放引用。
	 */
	inline WTSObject* grab(const T &_key) noexcept
	{
		auto it = _map.find(_key);
		if(it == _map.end())
			return NULL;

		WTSObject* pRet = it->second;
		pRet->retain();
		return pRet;
	}

	/**
	 * @brief 新增一个数据,并增加数据引用计数
	 * @param _key 要添加的键
	 * @param obj 要添加的对象指针
	 * @param bAutoRetain 是否自动增加引用计数，默认为true
	 * 
	 * 如果key存在,则将原有数据释放
	 */
	inline void add(const T &_key, WTSObject* obj, bool bAutoRetain = true) noexcept
	{
		if (bAutoRetain && obj)
			obj->retain();

		WTSObject* pOldObj = NULL;
		auto it = _map.find(_key);
		if (it != _map.end())
		{
			pOldObj = it->second;
		}

		_map[_key] = obj;

		if (pOldObj) pOldObj->release();
	}

	/**
	 * @brief 根据key删除一个数据
	 * @param _key 要删除的键
	 * 
	 * 如果key存在,则对应数据引用计数-1
	 */
	inline void remove(const T &_key) noexcept
	{
		auto it = _map.find(_key);
		if (it != _map.end())
		{
			WTSObject* obj = it->second;
			_map.erase(it);
			if (obj) obj->release();
		}
	}


	/**
	 * @brief 获取容器起始位置的迭代器
	 */
	inline ConstIterator begin() const noexcept
	{
		return _map.begin();
	}

	/**
	 * @brief 获取容器末尾位置的迭代器
	 */
	inline ConstIterator end() const noexcept
	{
		return _map.end();
	}

	/**
	 * @brief 查找指定key的迭代器
	 * @param key 要查找的键
	 * @return 找到的迭代器，未找到返回end()
	 */
	inline ConstIterator find(const T& key) const noexcept
	{
		return _map.find(key);
	}

	/**
	 * @brief 清空容器
	 * 
	 * 容器内所有数据引用计数-1，然后清空容器。
	 */
	inline void clear() noexcept
	{
		ConstIterator it = _map.begin();
		for(; it != _map.end(); it++)
		{
			it->second->release();
		}
		_map.clear();
	}

	/**
	 * @brief 释放容器对象
	 * 
	 * 如果容器引用计数为1，则清空所有数据并删除对象。
	 */
	virtual void release() 
	{
		if (m_uRefs == 0)
			return;

		try
		{
			m_uRefs--;
			if (m_uRefs == 0)
			{
				clear();
				delete this;
			}
		}
		catch (...)
		{

		}
	}
};

//////////////////////////////////////////////////////////////////////////
//WTSQueue

/**
 * @brief 队列容器类
 * 
 * 该类是WonderTrader平台内部的队列容器，内部使用std::deque实现。
 * 提供先进先出（FIFO）的数据访问模式，支持引用计数管理。
 * 主要用于需要队列操作的数据处理场景。
 */
class WTSQueue : public WTSObject
{
public:
	/**
	 * @brief 队列迭代器类型定义
	 */
	typedef std::deque<WTSObject*>::iterator Iterator;                    // 正向迭代器
	typedef std::deque<WTSObject*>::const_iterator ConstIterator;         // 常量正向迭代器

	/**
	 * @brief 创建队列对象
	 * @return 新创建的队列对象指针
	 * 
	 * 静态工厂方法，用于创建新的队列对象实例。
	 */
	static WTSQueue* create()
	{
		WTSQueue* pRet = new WTSQueue();
		return pRet;
	}

	/**
	 * @brief 弹出队列头部元素
	 * 
	 * 移除队列头部的元素，但不返回该元素。
	 * 如果队列为空，则不会有任何操作。
	 */
	void pop()
	{
		_queue.pop_front();
	}

	/**
	 * @brief 向队列尾部添加元素
	 * @param obj 要添加的对象指针
	 * @param bAutoRetain 是否自动增加引用计数，默认为true
	 * 
	 * 将对象添加到队列尾部，如果bAutoRetain为true，
	 * 则自动调用对象的retain()方法增加引用计数。
	 */
	void push(WTSObject* obj, bool bAutoRetain = true)
	{
		if (obj && bAutoRetain)
			obj->retain();

		_queue.emplace_back(obj);
	}

	/**
	 * @brief 获取队列头部元素
	 * @param bRetain 是否增加引用计数，默认为true
	 * @return 队列头部元素的WTSObject指针，队列为空返回NULL
	 * 
	 * 获取队列头部元素但不移除，如果bRetain为true，
	 * 则调用对象的retain()方法增加引用计数。
	 */
	WTSObject* front(bool bRetain = true)
	{
		if(_queue.empty())
			return NULL;

		WTSObject* obj = _queue.front();
		if(bRetain)
			obj->retain();

		return obj;
	}

	/**
	 * @brief 获取队列尾部元素
	 * @param bRetain 是否增加引用计数，默认为true
	 * @return 队列尾部元素的WTSObject指针，队列为空返回NULL
	 * 
	 * 获取队列尾部元素但不移除，如果bRetain为true，
	 * 则调用对象的retain()方法增加引用计数。
	 */
	WTSObject* back(bool bRetain = true)
	{
		if(_queue.empty())
			return NULL;

		WTSObject* obj = _queue.back();
		if(bRetain)
			obj->retain();

		return obj;
	}

	/**
	 * @brief 获取队列大小
	 * @return 队列当前包含的元素数量
	 */
	uint32_t size() const{ return (uint32_t)_queue.size(); }

	/**
	 * @brief 检查队列是否为空
	 * @return 队列为空返回true，否则返回false
	 */
	bool	empty() const{return _queue.empty();}

	/**
	 * @brief 释放队列对象
	 * 
	 * 该方法用于释放WTSQueue对象，如果引用计数为1，
	 * 则释放所有数据并删除对象。
	 */
	void release()
	{
		if (m_uRefs == 0)
			return;

		try
		{
			m_uRefs--;
			if (m_uRefs == 0)
			{
				clear();
				delete this;
			}
		}
		catch (...)
		{

		}
	}

	/**
	 * @brief 清空队列
	 * 
	 * 队列内所有数据引用计数-1，然后清空队列。
	 */
	void clear()
	{
		Iterator it = begin();
		for(; it != end(); it++)
		{
			(*it)->release();
		}
		_queue.clear();
	}

	/**
	 * @brief 取得队列起始位置的迭代器
	 * @return 队列起始位置的正向迭代器
	 */
	Iterator begin()
	{
		return _queue.begin();
	}

	/**
	 * @brief 取得队列起始位置的常量迭代器
	 * @return 队列起始位置的常量正向迭代器
	 */
	ConstIterator begin() const
	{
		return _queue.begin();
	}

	/**
	 * @brief 交换两个队列的内容
	 * @param right 要交换的队列指针
	 * 
	 * 将当前队列与指定队列的内容进行交换。
	 */
	void swap(WTSQueue* right)
	{
		_queue.swap(right->_queue);
	}

protected:
	/**
	 * @brief 默认构造函数
	 */
	WTSQueue(){}
	
	/**
	 * @brief 析构函数
	 */
	~WTSQueue(){}

	std::deque<WTSObject*>	_queue;    // 内部存储的双端队列容器
};

NS_WTP_END