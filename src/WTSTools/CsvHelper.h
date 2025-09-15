/*!
 * \file CsvHelper.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CSV文件读取辅助工具类
 * 
 * 本文件定义了用于读取和解析CSV格式文件的辅助工具类。CSV（Comma-Separated Values）
 * 是一种常用的数据交换格式，广泛用于数据导入导出、配置文件等场景。
 * 
 * 主要功能：
 * - 解析CSV文件的字段头和数据行
 * - 支持按字段名或列索引访问数据
 * - 提供多种数据类型的转换（整数、浮点数、字符串）
 * - 自动处理UTF-8 BOM编码
 * - 支持自定义分隔符
 * - 逐行读取，内存效率高
 * 
 * 设计特点：
 * - 简单易用的API接口
 * - 支持大文件的流式读取
 * - 自动类型转换和错误处理
 * - 兼容多种CSV格式变体
 */
#pragma once
#include <string.h>        // C字符串处理函数
#include <string>          // 标准字符串类
#include <unordered_map>   // 哈希映射容器
#include <stdint.h>        // 标准整数类型定义
#include <fstream>         // 文件流操作
#include <vector>          // 动态数组容器
#include <sstream>         // 字符串流操作

/**
 * @class CsvReader
 * @brief CSV文件读取器
 * 
 * CsvReader提供了一个简单而高效的CSV文件读取接口。支持按字段名或列索引
 * 访问数据，自动处理数据类型转换，适用于各种CSV数据处理场景。
 * 
 * 使用方法：
 * 1. 创建CsvReader实例，可指定分隔符
 * 2. 调用load_from_file()加载CSV文件
 * 3. 使用next_row()逐行读取数据
 * 4. 使用get_xxx()方法获取字段值
 * 
 * 示例代码：
 * @code
 * CsvReader reader(",");
 * if (reader.load_from_file("data.csv")) {
 *     while (reader.next_row()) {
 *         int id = reader.get_int32("id");
 *         std::string name = reader.get_string("name");
 *         double price = reader.get_double("price");
 *     }
 * }
 * @endcode
 */
class CsvReader
{
public:
	/**
	 * @brief 构造函数
	 * @param item_splitter 字段分隔符，默认为逗号","
	 * 
	 * 创建CSV读取器实例，可以指定自定义的字段分隔符。
	 * 常用的分隔符包括逗号、分号、制表符等。
	 */
	CsvReader(const char* item_splitter = ",");

public:
	/**
	 * @brief 从文件加载CSV数据
	 * @param filename CSV文件路径
	 * @return bool 加载成功返回true，失败返回false
	 * 
	 * 打开指定的CSV文件，解析文件头，建立字段映射。
	 * 自动处理UTF-8 BOM编码，清理字段名中的特殊字符。
	 */
	bool	load_from_file(const char* filename);

public:
	/**
	 * @name 基本信息查询接口
	 * @brief 获取CSV文件的基本信息
	 * @{
	 */
	
	/**
	 * @brief 获取字段数量
	 * @return uint32_t 字段总数
	 * 
	 * 返回CSV文件中定义的字段（列）总数。
	 */
	inline uint32_t	col_count() { return (uint32_t)_fields_map.size(); }
	
	/**
	 * @brief 获取所有字段名称
	 * @return const char* 以逗号分隔的字段名称字符串
	 * 
	 * 返回CSV文件头中定义的所有字段名称，用逗号连接。
	 * 主要用于调试和日志输出。
	 */
	const char* fields() const 
	{ 
		static std::string s;
		if(s.empty())
		{
			std::stringstream ss;
			for (auto item : _fields_map)
				ss << item.first << ",";

			s = ss.str();
			s = s.substr(0, s.size() - 1);
		}

		return s.c_str();
	}
	
	/** @} */

	/**
	 * @name 按列索引获取数据接口
	 * @brief 根据列索引获取当前行的字段值
	 * @{
	 */
	
	/**
	 * @brief 获取32位有符号整数值
	 * @param col 列索引（从0开始）
	 * @return int32_t 转换后的整数值，转换失败返回0
	 */
	int32_t		get_int32(int32_t col);
	
	/**
	 * @brief 获取32位无符号整数值
	 * @param col 列索引（从0开始）
	 * @return uint32_t 转换后的无符号整数值，转换失败返回0
	 */
	uint32_t	get_uint32(int32_t col);

	/**
	 * @brief 获取64位有符号整数值
	 * @param col 列索引（从0开始）
	 * @return int64_t 转换后的长整数值，转换失败返回0
	 */
	int64_t		get_int64(int32_t col);
	
	/**
	 * @brief 获取64位无符号整数值
	 * @param col 列索引（从0开始）
	 * @return uint64_t 转换后的无符号长整数值，转换失败返回0
	 */
	uint64_t	get_uint64(int32_t col);

	/**
	 * @brief 获取双精度浮点数值
	 * @param col 列索引（从0开始）
	 * @return double 转换后的浮点数值，转换失败返回0.0
	 */
	double		get_double(int32_t col);

	/**
	 * @brief 获取字符串值
	 * @param col 列索引（从0开始）
	 * @return const char* 字符串指针，无效列返回空字符串
	 */
	const char*	get_string(int32_t col);
	
	/** @} */

	/**
	 * @name 按字段名获取数据接口
	 * @brief 根据字段名获取当前行的字段值
	 * @{
	 */
	
	/**
	 * @brief 根据字段名获取32位有符号整数值
	 * @param field 字段名称
	 * @return int32_t 转换后的整数值，字段不存在或转换失败返回0
	 */
	int32_t		get_int32(const char* field);
	
	/**
	 * @brief 根据字段名获取32位无符号整数值
	 * @param field 字段名称
	 * @return uint32_t 转换后的无符号整数值，字段不存在或转换失败返回0
	 */
	uint32_t	get_uint32(const char* field);

	/**
	 * @brief 根据字段名获取64位有符号整数值
	 * @param field 字段名称
	 * @return int64_t 转换后的长整数值，字段不存在或转换失败返回0
	 */
	int64_t		get_int64(const char* field);
	
	/**
	 * @brief 根据字段名获取64位无符号整数值
	 * @param field 字段名称
	 * @return uint64_t 转换后的无符号长整数值，字段不存在或转换失败返回0
	 */
	uint64_t	get_uint64(const char* field);

	/**
	 * @brief 根据字段名获取双精度浮点数值
	 * @param field 字段名称
	 * @return double 转换后的浮点数值，字段不存在或转换失败返回0.0
	 */
	double		get_double(const char* field);

	/**
	 * @brief 根据字段名获取字符串值
	 * @param field 字段名称
	 * @return const char* 字符串指针，字段不存在返回空字符串
	 */
	const char*	get_string(const char* field);
	
	/** @} */

	/**
	 * @brief 读取下一行数据
	 * @return bool 成功读取返回true，到达文件末尾或出错返回false
	 * 
	 * 从CSV文件中读取下一行数据，解析各个字段值。
	 * 调用此方法后，可以使用get_xxx()方法获取当前行的字段值。
	 * 
	 * 使用模式：
	 * @code
	 * while (reader.next_row()) {
	 *     // 处理当前行数据
	 *     int value = reader.get_int32("column_name");
	 * }
	 * @endcode
	 */
	bool		next_row();

private:
	/**
	 * @name 私有辅助方法
	 * @brief 内部使用的辅助函数
	 * @{
	 */
	
	/**
	 * @brief 检查列索引是否有效
	 * @param col 列索引
	 * @return bool 有效返回true，无效返回false
	 * 
	 * 验证指定的列索引是否在有效范围内，用于防止数组越界。
	 */
	bool		check_cell(int32_t col);
	
	/**
	 * @brief 根据字段名获取列索引
	 * @param field 字段名称
	 * @return int32_t 对应的列索引，未找到返回INT_MAX
	 * 
	 * 在字段映射表中查找指定字段名对应的列索引。
	 */
	int32_t		get_col_by_filed(const char* field);
	
	/** @} */

private:
	/**
	 * @name 私有成员变量
	 * @brief CSV读取器的内部数据存储
	 * @{
	 */
	
	/// 文件输入流，用于读取CSV文件内容
	std::ifstream	_ifs;
	
	/// 行数据缓冲区，存储当前读取的行内容
	char			_buffer[1024];
	
	/// 字段分隔符字符串
	std::string		_item_splitter;

	/// 字段名到列索引的映射表，用于按字段名查找数据
	std::unordered_map<std::string, int32_t> _fields_map;
	
	/// 当前行的字段值列表，存储解析后的各个字段值
	std::vector<std::string> _current_cells;
	
	/** @} */
};
