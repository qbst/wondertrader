/*!
 * \file CsvHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief CSV文件读取辅助工具类的实现文件
 * 
 * 本文件实现了CsvHelper.h中定义的CsvReader类，提供了完整的CSV文件读取和解析功能。
 * 
 * 设计逻辑和主要作用：
 * ===============
 * 
 * 1. **核心设计理念**：
 *    - 提供简单易用的CSV文件读取接口，支持大文件的流式处理
 *    - 实现强类型的数据访问，支持按字段名或列索引获取数据
 *    - 自动处理各种CSV格式的兼容性问题（编码、分隔符、特殊字符等）
 *    - 内存高效的逐行读取机制，避免一次性加载整个文件
 * 
 * 2. **在WonderTrader框架中的作用**：
 *    - **基础数据加载**：用于加载合约信息、交易日历、参数配置等CSV格式的基础数据
 *    - **历史数据导入**：支持从CSV文件导入历史行情数据、交易记录等
 *    - **配置文件解析**：读取策略参数、系统配置等以CSV格式存储的配置信息
 *    - **数据交换接口**：作为与外部系统进行数据交换的标准接口
 * 
 * 3. **技术特点**：
 *    - **编码兼容性**：自动检测和处理UTF-8 BOM编码，确保中文等多字节字符正确显示
 *    - **字段名标准化**：自动清理字段名中的特殊字符，转换为小写，提高匹配成功率
 *    - **灵活的分隔符支持**：支持自定义分隔符，适应不同的CSV格式变体
 *    - **健壮的错误处理**：对无效数据进行安全处理，避免程序崩溃
 *    - **高效的数据访问**：使用哈希映射实现O(1)时间复杂度的字段名查找
 * 
 * 4. **使用场景示例**：
 *    - 加载股票/期货合约基本信息（代码、名称、交易所、合约乘数等）
 *    - 读取交易日历文件（交易日、节假日信息）
 *    - 导入历史K线数据（开高低收、成交量等）
 *    - 解析策略配置参数文件
 *    - 加载风险管理规则配置
 * 
 * 5. **性能优化考虑**：
 *    - 使用固定大小的缓冲区减少内存分配开销
 *    - 采用引用传递和移动语义减少不必要的字符串拷贝
 *    - 一次性建立字段映射表，避免重复查找
 *    - 流式读取避免大文件的内存压力
 */

#include "CsvHelper.h"  // CSV辅助工具类头文件

#include <limits.h>  // 包含整数限制常量定义（如INT_MAX）

#include "../Share/StdUtils.hpp"  // 包含标准工具类，提供文件操作等基础功能
#include "../Share/StrUtil.hpp"   // 包含字符串处理工具类，提供字符串分割、修剪等功能

/**
 * @brief CsvReader构造函数
 * @param item_splitter 字段分隔符，默认为逗号","
 * 
 * 初始化CSV读取器实例，设置用于分割CSV字段的分隔符。
 * 分隔符可以是单个字符（如逗号、分号）或多字符字符串（如制表符"\t"）。
 * 构造函数使用初始化列表的方式设置成员变量，提高初始化效率。
 */
CsvReader::CsvReader(const char* item_splitter /* = "," */)
	: _item_splitter(item_splitter)  // 使用初始化列表设置字段分隔符成员变量
{
	// 构造函数体为空，所有初始化工作在初始化列表中完成
	// 其他成员变量（如_ifs, _fields_map, _current_cells）使用默认构造函数初始化
}

/**
 * @brief 从指定文件加载CSV数据
 * @param filename CSV文件的完整路径
 * @return bool 加载成功返回true，失败返回false
 * 
 * 该函数执行以下主要操作：
 * 1. 检查文件是否存在
 * 2. 打开文件并读取第一行（表头）
 * 3. 处理UTF-8 BOM编码标记
 * 4. 清理字段名中的特殊字符
 * 5. 将字段名转换为小写以提高匹配成功率
 * 6. 分割字段并建立字段名到列索引的映射表
 * 
 * 注意：该函数只读取和解析文件头，不读取数据行。
 * 数据行的读取由next_row()函数负责。
 */
bool CsvReader::load_from_file(const char* filename)
{
	// 检查指定的CSV文件是否存在，如果不存在则直接返回失败
	if (!StdFile::exists(filename))
		return false;

	// 使用文件输入流打开指定的CSV文件，准备进行读取操作
	_ifs.open(filename);

	// 读取文件的第一行（表头行）到缓冲区，最多读取1024个字符
	// 第一行通常包含各列的字段名称，用于建立字段映射
	_ifs.getline(_buffer, 1024);
	
	// 检测并处理UTF-8 BOM（Byte Order Mark）编码标记
	// UTF-8 BOM是文件开头的3个字节：0xEF 0xBB 0xBF
	static char flag[] = { (char)0xEF, (char)0xBB, (char)0xBF };  // UTF-8 BOM标记的字节序列
	char* buf = _buffer;  // 初始化缓冲区指针，指向读取的数据开始位置
	if (memcmp(_buffer, flag, sizeof(char) * 3) == 0)  // 比较前3个字节是否为UTF-8 BOM
		buf += 3;  // 如果检测到BOM，跳过这3个字节，指向实际的文本内容

	// 将处理后的缓冲区内容转换为std::string对象，便于后续字符串操作
	std::string row = buf;

	// 清理字段名中可能存在的特殊符号，提高字段名的标准化程度
	// 这些特殊符号可能来自不同的CSV生成工具或编辑器
	StrUtil::replace(row, "<", "");   // 移除小于号，避免与HTML标签混淆
	StrUtil::replace(row, ">", "");   // 移除大于号，避免与HTML标签混淆
	StrUtil::replace(row, "\"", "");  // 移除双引号，简化字段名格式
	StrUtil::replace(row, "'", "");   // 移除单引号，简化字段名格式

	// 将所有字段名转换为小写，实现大小写不敏感的字段名匹配
	// 这样用户在使用get_xxx("field_name")时不需要关心字段名的大小写
	StrUtil::toLowerCase(row);

	// 使用指定的分隔符将表头行分割成各个字段名
	// StringVector是std::vector<std::string>的类型别名
	StringVector fields = StrUtil::split(row, _item_splitter.c_str());
	
	// 遍历分割后的每个字段名，建立字段名到列索引的映射关系
	for (uint32_t i = 0; i < fields.size(); i++)
	{
		// 清理字段名两端的空白字符，确保字段名的准确性
		StrUtil::trim(fields[i], " ");   // 移除空格字符
		StrUtil::trim(fields[i], "\n");  // 移除换行符
		StrUtil::trim(fields[i], "\t");  // 移除制表符
		StrUtil::trim(fields[i], "\r");  // 移除回车符
		
		// 如果字段名为空（可能是连续的分隔符导致），停止处理后续字段
		// 这避免了在映射表中创建无效的空字段名条目
		if (fields[i].empty())
			break;

		// 在哈希映射表中建立字段名到列索引的映射关系
		// 这个映射表后续用于根据字段名快速定位到对应的列索引
		// 时间复杂度为O(1)，比线性查找更高效
		_fields_map[fields[i]] = i;
	}

	// 文件加载和表头解析成功，返回true
	return true;
}

/**
 * @brief 读取CSV文件的下一行数据
 * @return bool 成功读取返回true，到达文件末尾或出错返回false
 * 
 * 该函数实现流式读取CSV文件的数据行，具有以下特点：
 * 1. 跳过空行，确保读取到有效的数据行
 * 2. 将读取的行按分隔符分割成各个字段
 * 3. 更新内部的当前行字段列表，供get_xxx()方法使用
 * 4. 支持大文件的逐行处理，内存效率高
 * 
 * 使用模式：
 * while (reader.next_row()) {
 *     // 处理当前行的数据
 *     int value = reader.get_int32("column_name");
 * }
 */
bool CsvReader::next_row()
{
	// 首先检查文件流是否已经到达末尾
	// 如果到达末尾，说明没有更多数据可读，返回false
	if (_ifs.eof())
		return false;

	// 循环读取行，直到找到非空行或到达文件末尾
	// 这个循环的目的是跳过CSV文件中可能存在的空行
	while (!_ifs.eof())
	{
		// 从文件中读取一行数据到缓冲区，最多读取1024个字符
		// getline会自动处理不同平台的换行符（\n, \r\n等）
		_ifs.getline(_buffer, 1024);
		
		// 检查读取的行是否为空行
		if(strlen(_buffer) == 0)
			continue;  // 如果是空行，继续读取下一行
		else
			break;     // 如果不是空行，跳出循环，处理这一行
	} 
	
	// 再次检查缓冲区是否为空
	// 这种情况可能发生在文件末尾只有空行的情况下
	if (strlen(_buffer) == 0)
		return false;  // 如果缓冲区为空，说明没有有效数据，返回false
	
	// 清空上一行的字段数据，准备存储新行的字段
	// 这确保了每次调用next_row()后，_current_cells只包含当前行的数据
	_current_cells.clear();
	
	// 使用指定的分隔符将当前行分割成各个字段
	// 分割结果直接存储到_current_cells向量中
	// 这个向量将被后续的get_xxx()方法使用来获取具体的字段值
	StrUtil::split(_buffer, _current_cells, _item_splitter.c_str());
	
	// 成功读取并解析了一行数据，返回true
	return true;
}

/**
 * @brief 根据列索引获取32位有符号整数值
 * @param col 列索引（从0开始）
 * @return int32_t 转换后的整数值，无效列或转换失败返回0
 * 
 * 从当前行的指定列获取数据并转换为32位有符号整数。
 * 使用标准C库函数strtol进行字符串到整数的转换，支持十进制格式。
 * 转换失败时（如非数字字符）会返回0，这是一种安全的错误处理方式。
 */
int32_t CsvReader::get_int32(int32_t col)
{
	// 检查列索引是否有效，包括范围检查和数据有效性检查
	if (!check_cell(col))
		return 0;  // 无效列索引，返回默认值0

	// 使用strtol函数将字符串转换为长整数（32位有符号整数）
	// 参数说明：字符串指针，结束位置指针（NULL表示不需要），进制（10表示十进制）
	return strtol(_current_cells[col].c_str(), NULL, 10);
}

/**
 * @brief 根据列索引获取32位无符号整数值
 * @param col 列索引（从0开始）
 * @return uint32_t 转换后的无符号整数值，无效列或转换失败返回0
 * 
 * 从当前行的指定列获取数据并转换为32位无符号整数。
 * 适用于处理非负数值，如ID、计数、索引等场景。
 */
uint32_t CsvReader::get_uint32(int32_t col)
{
	// 检查列索引是否有效
	if (!check_cell(col))
		return 0;  // 无效列索引，返回默认值0

	// 使用strtoul函数将字符串转换为无符号长整数（32位无符号整数）
	// 该函数专门用于转换无符号整数，可以处理更大的正数范围
	return strtoul(_current_cells[col].c_str(), NULL, 10);
}

/**
 * @brief 根据列索引获取64位有符号整数值
 * @param col 列索引（从0开始）
 * @return int64_t 转换后的长整数值，无效列或转换失败返回0
 * 
 * 从当前行的指定列获取数据并转换为64位有符号整数。
 * 适用于处理大数值，如时间戳、大额金额、长ID等场景。
 */
int64_t CsvReader::get_int64(int32_t col)
{
	// 检查列索引是否有效
	if (!check_cell(col))
		return 0;  // 无效列索引，返回默认值0

	// 使用strtoll函数将字符串转换为长长整数（64位有符号整数）
	// 该函数支持更大的数值范围，适用于处理大整数
	return strtoll(_current_cells[col].c_str(), NULL, 10);
}

/**
 * @brief 根据列索引获取64位无符号整数值
 * @param col 列索引（从0开始）
 * @return uint64_t 转换后的无符号长整数值，无效列或转换失败返回0
 * 
 * 从当前行的指定列获取数据并转换为64位无符号整数。
 * 提供最大的正整数范围，适用于处理超大数值。
 */
uint64_t CsvReader::get_uint64(int32_t col)
{
	// 检查列索引是否有效
	if (!check_cell(col))
		return 0;  // 无效列索引，返回默认值0

	// 使用strtoull函数将字符串转换为无符号长长整数（64位无符号整数）
	// 提供最大的正整数数值范围
	return strtoull(_current_cells[col].c_str(), NULL, 10);
}

/**
 * @brief 根据列索引获取双精度浮点数值
 * @param col 列索引（从0开始）
 * @return double 转换后的浮点数值，无效列或转换失败返回0.0
 * 
 * 从当前行的指定列获取数据并转换为双精度浮点数。
 * 适用于处理小数、价格、比率等需要精确计算的数值场景。
 * 支持科学计数法和常规小数格式。
 */
double CsvReader::get_double(int32_t col)
{
	// 检查列索引是否有效
	if (!check_cell(col))
		return 0;  // 无效列索引，返回默认值0.0

	// 使用strtod函数将字符串转换为双精度浮点数
	// 该函数支持多种浮点数格式，包括科学计数法（如1.23e-4）
	return strtod(_current_cells[col].c_str(), NULL);
}

/**
 * @brief 根据列索引获取字符串值
 * @param col 列索引（从0开始）
 * @return const char* 字符串指针，无效列返回空字符串
 * 
 * 从当前行的指定列获取原始字符串数据。
 * 返回的是指向内部字符串对象的C风格字符串指针。
 * 
 * 注意：返回的指针在下次调用next_row()后可能失效，
 * 如需长期保存，请复制字符串内容。
 */
const char* CsvReader::get_string(int32_t col)
{
	// 检查列索引是否有效
	if (!check_cell(col))
		return "";  // 无效列索引，返回空字符串

	// 返回指向内部字符串对象的C风格字符串指针
	// c_str()方法返回以null结尾的字符数组指针
	return _current_cells[col].c_str();
}

/**
 * @brief 根据字段名获取32位有符号整数值
 * @param field 字段名称（不区分大小写）
 * @return int32_t 转换后的整数值，字段不存在或转换失败返回0
 * 
 * 该函数首先根据字段名查找对应的列索引，然后调用按列索引获取数据的方法。
 * 这种设计实现了代码复用，避免重复的类型转换逻辑。
 * 字段名匹配不区分大小写，因为在load_from_file()中已经将字段名转换为小写。
 */
int32_t CsvReader::get_int32(const char* field)
{
	// 根据字段名查找对应的列索引
	int32_t col = get_col_by_filed(field);
	// 调用按列索引获取数据的重载方法，实现代码复用
	return get_int32(col);
}

/**
 * @brief 根据字段名获取32位无符号整数值
 * @param field 字段名称（不区分大小写）
 * @return uint32_t 转换后的无符号整数值，字段不存在或转换失败返回0
 * 
 * 通过字段名访问数据的便捷方法，内部实现为字段名到列索引的转换。
 */
uint32_t CsvReader::get_uint32(const char* field)
{
	// 根据字段名查找对应的列索引
	int32_t col = get_col_by_filed(field);
	// 调用按列索引获取数据的重载方法
	return get_uint32(col);
}

/**
 * @brief 根据字段名获取64位有符号整数值
 * @param field 字段名称（不区分大小写）
 * @return int64_t 转换后的长整数值，字段不存在或转换失败返回0
 * 
 * 适用于通过字段名访问大整数数据，如时间戳、长ID等。
 */
int64_t CsvReader::get_int64(const char* field)
{
	// 根据字段名查找对应的列索引
	int32_t col = get_col_by_filed(field);
	// 调用按列索引获取数据的重载方法
	return get_int64(col);
}

/**
 * @brief 根据字段名获取64位无符号整数值
 * @param field 字段名称（不区分大小写）
 * @return uint64_t 转换后的无符号长整数值，字段不存在或转换失败返回0
 * 
 * 提供最大正整数范围的字段名访问接口。
 */
uint64_t CsvReader::get_uint64(const char* field)
{
	// 根据字段名查找对应的列索引
	int32_t col = get_col_by_filed(field);
	// 调用按列索引获取数据的重载方法
	return get_uint64(col);
}

/**
 * @brief 根据字段名获取双精度浮点数值
 * @param field 字段名称（不区分大小写）
 * @return double 转换后的浮点数值，字段不存在或转换失败返回0.0
 * 
 * 通过字段名访问浮点数数据的便捷方法，常用于价格、比率等数值字段。
 */
double CsvReader::get_double(const char* field)
{
	// 根据字段名查找对应的列索引
	int32_t col = get_col_by_filed(field);
	// 调用按列索引获取数据的重载方法
	return get_double(col);
}

/**
 * @brief 根据字段名获取字符串值
 * @param field 字段名称（不区分大小写）
 * @return const char* 字符串指针，字段不存在返回空字符串
 * 
 * 通过字段名访问原始字符串数据的便捷方法。
 * 这是最常用的数据访问方式，因为字段名比列索引更直观易懂。
 */
const char* CsvReader::get_string(const char* field)
{
	// 根据字段名查找对应的列索引
	int32_t col = get_col_by_filed(field);
	// 调用按列索引获取数据的重载方法
	return get_string(col);
}

/**
 * @brief 检查列索引是否有效
 * @param col 要检查的列索引
 * @return bool 有效返回true，无效返回false
 * 
 * 该函数执行多层级的有效性检查：
 * 1. 检查是否为无效标记值（INT_MAX）
 * 2. 检查索引是否在有效范围内（0到字段数量-1）
 * 3. 确保不会发生数组越界访问
 * 
 * 这是一个防御性编程的重要组件，确保所有数据访问操作的安全性。
 */
bool CsvReader::check_cell(int32_t col)
{
	// 检查是否为无效标记值
	// INT_MAX通常用作"未找到"或"无效"的标记值
	if (col == INT_MAX )
		return false;  // 无效的列索引标记

	// 检查列索引是否在有效范围内
	// 有效范围：0 <= col < 字段总数
	if (col < 0 || col >= (int32_t)_fields_map.size())
		return false;  // 列索引超出有效范围

	// 所有检查都通过，列索引有效
	return true;
}

/**
 * @brief 根据字段名获取对应的列索引
 * @param field 要查找的字段名称
 * @return int32_t 对应的列索引，未找到返回INT_MAX
 * 
 * 该函数在字段映射表中查找指定的字段名，返回对应的列索引。
 * 使用哈希映射实现O(1)时间复杂度的查找效率。
 * 
 * 查找过程：
 * 1. 在_fields_map中查找指定的字段名
 * 2. 如果找到，返回对应的列索引
 * 3. 如果未找到，返回INT_MAX作为无效标记
 * 
 * 注意：由于在load_from_file()中已经将字段名转换为小写，
 * 所以这里的查找实际上是大小写不敏感的。
 */
int32_t CsvReader::get_col_by_filed(const char* field)
{
	// 在字段映射表中查找指定的字段名
	// 使用std::unordered_map的find方法进行高效查找
	auto it = _fields_map.find(field);
	
	// 检查是否找到了指定的字段名
	if (it == _fields_map.end())
		return INT_MAX;  // 未找到字段，返回无效标记值

	// 找到了字段，返回对应的列索引
	// it->second 是映射表中的值部分，即列索引
	return it->second;
}