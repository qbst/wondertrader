/*!
 * \file WTSVariant.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt通用变量对象定义文件
 * 
 * 设计逻辑与作用：
 * WTSVariant是一个通用数据容器，设计目标是类似Json的Value类，用于存储各种类型的数据。
 * 与Json不同的地方在于，WTSVariant满足WT系统内的派生关系，可以通过引用计数管理数据，
 * 从而减少数据复制，提高系统性能。支持的数据类型包括：空值、数组、整数、无符号整数、
 * 长整数、无符号长整数、字符串、实数、布尔值、对象等。该类主要用于配置管理、数据传递、
 * 参数存储等场景，为整个交易系统提供了灵活的数据存储解决方案。
 */
#pragma once  // 防止头文件被重复包含

#include "WTSTypes.h"        // 包含WonderTrader基本类型定义
#include "WTSObject.hpp"     // 包含WonderTrader对象基类
#include "WTSCollection.hpp" // 包含WonderTrader集合类

#include <string>            // 包含标准字符串库
#include <string.h>          // 包含C风格字符串函数
#include <vector>            // 包含标准向量容器
#include <map>               // 包含标准映射容器

// 根据编译器类型定义64位整数的格式化字符串
#ifdef _MSC_VER
#define INT64_FMT	"%I64d"  // Visual Studio编译器使用的64位整数格式
#define UINT64_FMT	"%I64u"  // Visual Studio编译器使用的64位无符号整数格式
#else
#define INT64_FMT	"%ld"    // 其他编译器使用的64位整数格式
#define UINT64_FMT	"%lu"    // 其他编译器使用的64位无符号整数格式
#endif


NS_WTP_BEGIN  // 开始WonderTrader命名空间

/*
 *	WTSVariant是一个通用数据容器,设计目标是Json的Value类
 *	和Json不同的地方在于,WTSVariant满足WT系统内的派生关系
 *	可以通过引用计数管理数据,从而减少数据复制
 */
class WTSVariant : public WTSObject  // 继承自WTSObject基类，支持引用计数
{
public:
	typedef WTSArray					ChildrenArray;        // 子元素数组类型别名
	typedef WTSHashMap<std::string>		ChildrenMap;          // 子元素映射类型别名
	typedef std::vector<std::string>	MemberNames;          // 成员名称向量类型别名

	// 值类型枚举，定义了WTSVariant支持的所有数据类型
	typedef enum
	{
		VT_Null,      // 空值类型：表示无数据
		VT_Array,     // 数组类型：表示数组数据
		VT_Int32,     // 32位整数类型：表示32位有符号整数
		VT_Uint32,    // 32位无符号整数类型：表示32位无符号整数
		VT_Int64,     // 64位整数类型：表示64位有符号整数
		VT_Uint64,    // 64位无符号整数类型：表示64位无符号整数
		VT_String,    // 字符串类型：表示字符串数据
		VT_Real,      // 实数类型：表示浮点数数据
		VT_Boolean,   // 布尔类型：表示布尔值数据
		VT_Object     // 对象类型：表示键值对对象
	}ValueType;


protected:
	// 保护构造函数，初始化类型为空值
	WTSVariant() :_type(VT_Null){}

private:
	// 私有静态工厂方法：创建32位整数类型的WTSVariant
	static inline WTSVariant* create(int32_t i32)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Int32;               // 设置类型为32位整数
		char s[32] = { 0 };                 // 创建32字节的字符缓冲区
		sprintf(s, "%d", i32);              // 将整数转换为字符串
		ret->_value._string = new std::string(s);  // 创建字符串对象并赋值
		return ret;                          // 返回创建的对象
	}

	// 私有静态工厂方法：创建32位无符号整数类型的WTSVariant
	static inline WTSVariant* create(uint32_t u32)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Uint32;              // 设置类型为32位无符号整数
		char s[32] = { 0 };                 // 创建32字节的字符缓冲区
		sprintf(s, "%u", u32);              // 将无符号整数转换为字符串
		ret->_value._string = new std::string(s);  // 创建字符串对象并赋值
		return ret;                          // 返回创建的对象
	}

	// 私有静态工厂方法：创建64位整数类型的WTSVariant
	static inline WTSVariant* create(int64_t i64)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Int64;               // 设置类型为64位整数
		char s[32] = { 0 };                 // 创建32字节的字符缓冲区
		sprintf(s, INT64_FMT, i64);         // 将64位整数转换为字符串
		ret->_value._string = new std::string(s);  // 创建字符串对象并赋值
		return ret;                          // 返回创建的对象
	}

	// 私有静态工厂方法：创建64位无符号整数类型的WTSVariant
	static inline WTSVariant* create(uint64_t u64)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Uint64;              // 设置类型为64位无符号整数
		char s[32] = { 0 };                 // 创建32字节的字符缓冲区
		sprintf(s, UINT64_FMT, u64);        // 将64位无符号整数转换为字符串
		ret->_value._string = new std::string(s);  // 创建字符串对象并赋值
		return ret;                          // 返回创建的对象
	}

	// 私有静态工厂方法：创建实数类型的WTSVariant
	static inline WTSVariant* create(double _real)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Real;                // 设置类型为实数
		char s[32] = { 0 };                 // 创建32字节的字符缓冲区
		sprintf(s, "%.10f", _real);         // 将实数转换为字符串，保留10位小数
		ret->_value._string = new std::string(s);  // 创建字符串对象并赋值
		return ret;                          // 返回创建的对象
	}

	// 私有静态工厂方法：创建字符串类型的WTSVariant
	static inline WTSVariant* create(const char* _string)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_String;              // 设置类型为字符串
		ret->_value._string = new std::string(_string);  // 创建字符串对象并赋值
		return ret;                          // 返回创建的对象
	}

	// 私有静态工厂方法：创建布尔类型的WTSVariant
	static inline WTSVariant* create(bool _bool)
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Boolean;             // 设置类型为布尔值
		ret->_value._string = new std::string(_bool ? "true" : "false");  // 创建布尔字符串并赋值
		return ret;                          // 返回创建的对象
	}

public:
	// 公共静态工厂方法：创建对象类型的WTSVariant
	static inline WTSVariant* createObject()
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Object;              // 设置类型为对象
		ret->_value._map = ChildrenMap::create();  // 创建子元素映射并赋值
		return ret;                          // 返回创建的对象
	}

	// 公共静态工厂方法：创建数组类型的WTSVariant
	static inline WTSVariant* createArray()
	{
		WTSVariant* ret = new WTSVariant();  // 创建新的WTSVariant对象
		ret->_type = VT_Array;               // 设置类型为数组
		ret->_value._array = ChildrenArray::create();  // 创建子元素数组并赋值
		return ret;                          // 返回创建的对象
	}

	// 检查对象类型是否包含指定键
	inline bool has(const char* key) const
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		auto it = _value._map->find(key);  // 在映射中查找指定键
		if (it == _value._map->end())      // 如果未找到，返回false
			return false;

		return true;  // 找到键，返回true
	}

	// 将WTSVariant转换为32位整数
	inline int32_t asInt32() const
	{
		switch (_type)  // 根据类型进行转换
		{
		case VT_Null:  // 空值类型
			return 0;  // 返回0
		case VT_Int32:  // 32位整数类型
		case VT_Uint32: // 32位无符号整数类型
		case VT_Int64:  // 64位整数类型
		case VT_Uint64: // 64位无符号整数类型
		case VT_Real:   // 实数类型
		case VT_String: // 字符串类型
			return _value._string ? (int32_t)atof(_value._string->c_str()) : 0;  // 转换为整数
		default:        // 其他类型
			return 0;   // 返回0
		}
	}

	// 将WTSVariant转换为32位无符号整数
	inline uint32_t asUInt32() const
	{
		switch (_type)  // 根据类型进行转换
		{
		case VT_Null:  // 空值类型
			return 0;  // 返回0
		case VT_Int32:  // 32位整数类型
		case VT_Uint32: // 32位无符号整数类型
		case VT_Int64:  // 64位整数类型
		case VT_Uint64: // 64位无符号整数类型
		case VT_Real:   // 实数类型
		case VT_String: // 字符串类型
			return _value._string ? (uint32_t)atof(_value._string->c_str()) : 0;  // 转换为无符号整数
		default:        // 其他类型
			return 0;   // 返回0
		}
	}

	// 将WTSVariant转换为64位整数
	inline int64_t asInt64() const
	{
		switch (_type)  // 根据类型进行转换
		{
		case VT_Null:  // 空值类型
			return 0;  // 返回0
		case VT_Int32:  // 32位整数类型
		case VT_Uint32: // 32位无符号整数类型
		case VT_Int64:  // 64位整数类型
		case VT_Uint64: // 64位无符号整数类型
		case VT_Real:   // 实数类型
		case VT_String: // 字符串类型
			return _value._string ? strtoll(_value._string->c_str(), NULL, 10) : 0;  // 转换为64位整数
		default:        // 其他类型
			return 0;   // 返回0
		}
	}

	// 将WTSVariant转换为64位无符号整数
	inline uint64_t asUInt64() const
	{
		switch (_type)  // 根据类型进行转换
		{
		case VT_Null:  // 空值类型
			return 0;  // 返回0
		case VT_Int32:  // 32位整数类型
		case VT_Uint32: // 32位无符号整数类型
		case VT_Int64:  // 64位整数类型
		case VT_Uint64: // 64位无符号整数类型
		case VT_Real:   // 实数类型
		case VT_String: // 字符串类型
			return _value._string ? strtoull(_value._string->c_str(), NULL, 10) : 0;  // 转换为64位无符号整数
		default:        // 其他类型
			return 0;   // 返回0
		}
	}

	// 将WTSVariant转换为双精度浮点数
	inline double asDouble() const
	{
		switch (_type)  // 根据类型进行转换
		{
		case VT_Null:  // 空值类型
			return 0.0;  // 返回0.0
		case VT_Int32:  // 32位整数类型
		case VT_Uint32: // 32位无符号整数类型
		case VT_Int64:  // 64位整数类型
		case VT_Uint64: // 64位无符号整数类型
		case VT_Real:   // 实数类型
		case VT_String: // 字符串类型
			return _value._string ? strtod(_value._string->c_str(), NULL) : 0.0;  // 转换为双精度浮点数
		default:        // 其他类型
			return 0.0;  // 返回0.0
		}
	}

	// 将WTSVariant转换为标准字符串
	inline std::string	asString() const
	{
		switch (_type)  // 根据类型进行转换
		{
		case VT_Null:  // 空值类型
			return "";  // 返回空字符串
		case VT_Int32:  // 32位整数类型
		case VT_Uint32: // 32位无符号整数类型
		case VT_Int64:  // 64位整数类型
		case VT_Uint64: // 64位无符号整数类型
		case VT_Real:   // 实数类型
		case VT_String: // 字符串类型
		case VT_Boolean: // 布尔类型
			return _value._string ? *_value._string : "";  // 返回字符串内容
		default:        // 其他类型
			return "";   // 返回空字符串
		}
	}

	// 将WTSVariant转换为C风格字符串
	inline const char* asCString() const
	{
		if (_type != VT_Object && _type != VT_Array && _value._string != NULL)  // 如果不是对象或数组类型且有字符串值
			return _value._string->c_str();  // 返回C风格字符串

		return "";  // 返回空字符串
	}

	// 将WTSVariant转换为布尔值
	inline bool asBoolean() const
	{
		if (_value._string)  // 如果有字符串值
		{
			return wt_stricmp(_value._string->c_str(), "true") == 0 || wt_stricmp(_value._string->c_str(), "yes") == 0;  // 检查是否为true或yes
		}

		return false;  // 返回false
	}

	// 获取对象中指定名称的32位整数值
	inline int32_t getInt32(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asInt32();    // 返回其32位整数值

		return 0;  // 返回0
	}

	// 获取对象中指定名称的32位无符号整数值
	inline uint32_t getUInt32(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asUInt32();   // 返回其32位无符号整数值

		return 0;  // 返回0
	}

	// 获取对象中指定名称的64位整数值
	inline int64_t getInt64(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asInt64();    // 返回其64位整数值

		return 0;  // 返回0
	}

	// 获取对象中指定名称的64位无符号整数值
	inline uint64_t getUInt64(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asUInt64();   // 返回其64位无符号整数值

		return 0;  // 返回0
	}

	// 获取对象中指定名称的双精度浮点数值
	inline double getDouble(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asDouble();   // 返回其双精度浮点数值

		return 0.0;  // 返回0.0
	}

	// 获取对象中指定名称的字符串值
	inline std::string getString(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asString();   // 返回其字符串值

		return "";  // 返回空字符串
	}

	// 获取对象中指定名称的C风格字符串值
	inline const char* getCString(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asCString();  // 返回其C风格字符串值

		return "";  // 返回空字符串
	}

	// 获取对象中指定名称的布尔值
	inline bool getBoolean(const char* name) const
	{
		WTSVariant* p = get(name);  // 获取指定名称的子对象
		if (p)  // 如果找到子对象
			return p->asBoolean();  // 返回其布尔值

		return false;  // 返回false
	}

	// 获取对象中指定名称的子对象
	inline WTSVariant* get(const char* name) const
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回NULL
			return NULL;

		if (_value._map == NULL)  // 如果映射为空，返回NULL
			return NULL;

		WTSVariant* ret = static_cast<WTSVariant*>(_value._map->get(name));  // 从映射中获取指定名称的对象
		return ret;  // 返回找到的对象
	}

	// 获取对象中指定名称的子对象（重载版本，接受标准字符串）
	inline WTSVariant* get(const std::string& name) const
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回NULL
			return NULL;

		if (_value._map == NULL)  // 如果映射为空，返回NULL
			return NULL;

		WTSVariant* ret = static_cast<WTSVariant*>(_value._map->get(name));  // 从映射中获取指定名称的对象
		return ret;  // 返回找到的对象
	}

	// 获取数组中指定索引的子对象
	inline WTSVariant* get(uint32_t idx) const
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回NULL
			return NULL;

		if (_value._array == NULL)  // 如果数组为空，返回NULL
			return NULL;

		WTSVariant* ret = static_cast<WTSVariant*>(_value._array->at(idx));  // 从数组中获取指定索引的对象
		return ret;  // 返回找到的对象
	}

	// 向对象中添加字符串类型的子对象
	inline bool append(const char* _name, const char* _string)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_string);  // 创建字符串类型的WTSVariant
		_value._map->add(_name, item, false);            // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加32位整数类型的子对象
	inline bool append(const char* _name, int32_t _i32)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_i32);  // 创建32位整数类型的WTSVariant
		_value._map->add(_name, item, false);         // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加32位无符号整数类型的子对象
	inline bool append(const char* _name, uint32_t _u32)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_u32);  // 创建32位无符号整数类型的WTSVariant
		_value._map->add(_name, item, false);          // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加64位整数类型的子对象
	inline bool append(const char* _name, int64_t _i64)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_i64);  // 创建64位整数类型的WTSVariant
		_value._map->add(_name, item, false);         // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加64位无符号整数类型的子对象
	inline bool append(const char* _name, uint64_t _u64)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_u64);  // 创建64位无符号整数类型的WTSVariant
		_value._map->add(_name, item, false);          // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加实数类型的子对象
	inline bool append(const char* _name, double _real)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_real);  // 创建实数类型的WTSVariant
		_value._map->add(_name, item, false);          // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加布尔类型的子对象
	inline bool append(const char* _name, bool _bool)
	{
		if (_type != VT_Object)  // 如果不是对象类型，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		WTSVariant* item = WTSVariant::create(_bool);  // 创建布尔类型的WTSVariant
		_value._map->add(_name, item, false);          // 添加到映射中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向对象中添加WTSVariant类型的子对象
	inline bool append(const char* _name, WTSVariant *item, bool bAutoRetain = true)
	{
		if (_type != VT_Object || NULL == item)  // 如果不是对象类型或项目为空，返回false
			return false;

		if (_value._map == NULL)  // 如果映射为空，创建新的映射
		{
			_value._map = ChildrenMap::create();
		}

		_value._map->add(_name, item, bAutoRetain);  // 添加到映射中

		return true;  // 返回成功
	}

	// 向数组中添加字符串类型的子对象
	inline bool append(const char* _str)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_str);  // 创建字符串类型的WTSVariant
		_value._array->append(item, false);            // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加32位整数类型的子对象
	inline bool append(int32_t _i32)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_i32);  // 创建32位整数类型的WTSVariant
		_value._array->append(item, false);            // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加32位无符号整数类型的子对象
	inline bool append(uint32_t _u32)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_u32);  // 创建32位无符号整数类型的WTSVariant
		_value._array->append(item, false);            // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加64位整数类型的子对象
	inline bool append(int64_t _i64)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_i64);  // 创建64位整数类型的WTSVariant
		_value._array->append(item, false);            // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加64位无符号整数类型的子对象
	inline bool append(uint64_t _u64)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_u64);  // 创建64位无符号整数类型的WTSVariant
		_value._array->append(item, false);            // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加实数类型的子对象
	inline bool append(double _real)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_real);  // 创建实数类型的WTSVariant
		_value._array->append(item, false);             // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加布尔类型的子对象
	inline bool append(bool _bool)
	{
		if (_type != VT_Array)  // 如果不是数组类型，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		WTSVariant* item = WTSVariant::create(_bool);  // 创建布尔类型的WTSVariant
		_value._array->append(item, false);             // 添加到数组中
		//item->release();  // 注释掉的释放操作

		return true;  // 返回成功
	}

	// 向数组中添加WTSVariant类型的子对象
	inline bool append(WTSVariant *item, bool bAutoRetain = true)
	{
		if (_type != VT_Array || NULL == item)  // 如果不是数组类型或项目为空，返回false
			return false;

		if (_value._array == NULL)  // 如果数组为空，创建新的数组
		{
			_value._array = ChildrenArray::create();
		}

		_value._array->append(item, bAutoRetain);  // 添加到数组中

		return true;  // 返回成功
	}

	// 获取数组或对象的元素数量
	inline uint32_t size() const
	{
		if (_type != VT_Array && _type != VT_Object)  // 如果不是数组或对象类型，返回0
			return 0;

		else if (_type == VT_Array)  // 如果是数组类型
		{
			return (_value._array == NULL) ? 0 : _value._array->size();  // 返回数组大小
		}
		else  // 如果是对象类型
		{
			return (_value._map == NULL) ? 0 : _value._map->size();      // 返回映射大小
		}
	}

	// 获取对象的所有成员名称
	inline MemberNames memberNames() const
	{
		MemberNames names;  // 创建成员名称向量
		if (_type == VT_Object && _value._map != NULL)  // 如果是对象类型且映射不为空
		{
			auto it = _value._map->begin();  // 获取映射迭代器
			for (; it != _value._map->end(); it++)  // 遍历映射
			{
				names.emplace_back(it->first);  // 添加成员名称到向量中
			}
		}

		return std::move(names);  // 返回成员名称向量
	}

	// 释放对象资源
	virtual void release()
	{
		if (isSingleRefs())  // 如果是最后一个引用
		{
			switch (_type)  // 根据类型释放相应资源
			{
			case VT_Array:  // 数组类型
				if (NULL != _value._array)  // 如果数组不为空
				{
					_value._array->release();  // 释放数组资源
				}
				break;
			case VT_Object:  // 对象类型
				if (NULL != _value._map)  // 如果映射不为空
				{
					_value._map->release();  // 释放映射资源
				}
				break;
			default:  // 其他类型
				if (NULL != _value._string)  // 如果字符串不为空
				{
					delete _value._string;  // 删除字符串对象
				}
				break;
			}
		}
		WTSObject::release();  // 调用基类的释放方法
	}

	// 获取值类型
	inline ValueType type() const{ return _type; }

	// 检查是否为数组类型
	inline bool isArray() const{ return _type == VT_Array; }
	// 检查是否为对象类型
	inline bool isObject() const{ return _type == VT_Object; }

private:
	// 值持有者联合体，用于存储不同类型的值
	union ValueHolder
	{
		std::string*	_string;      // 字符串指针

		ChildrenMap*	_map;         // 子元素映射指针
		ChildrenArray*	_array;       // 子元素数组指针
	};
	ValueHolder	_value;  // 值持有者实例
	ValueType	_type;   // 值类型
};

NS_WTP_END  // 结束WonderTrader命名空间