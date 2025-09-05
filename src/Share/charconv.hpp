/*!
 * \file charconv.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 字符编码转换工具类
 * 
 * 文件设计逻辑与作用总结：
 * 本文件提供了多种字符编码转换功能，主要用于处理不同编码格式之间的转换。
 * 主要功能包括：
 * 1. UTF-8与本地编码（如GBK、GB2312）之间的相互转换
 * 2. URL编码和解码功能
 * 3. 编码格式检测（UTF-8、GBK）
 * 4. 跨平台兼容性处理（Windows/Linux）
 * 
 * 该类主要用于WonderTrader框架中处理不同数据源的编码问题，如：
 * - 从不同交易所获取的行情数据编码转换
 * - 配置文件、日志文件的多语言支持
 * - 网络通信中的URL编码处理
 * - 数据存储和读取时的编码统一
 * 
 * 通过提供统一的编码转换接口，确保框架能够正确处理各种编码格式的数据。
 */
#pragma once
#include <stdlib.h>                            // 标准库头文件，提供字符串转换函数
#include <string>                              // 标准字符串头文件
#ifdef _MSC_VER
#include <windows.h>                           // Windows API头文件，用于编码转换
#else
#include <iconv.h>                             // Linux/Unix编码转换头文件
#endif

/**
 * @brief UTF-8到本地编码转换类
 * 
 * 该类提供将UTF-8编码的字符串转换为本地系统编码（Windows下为ANSI，Linux下为GB2312）的功能。
 * 支持自动检测纯ASCII字符串，避免不必要的转换开销。
 * 
 * 主要特性：
 * - 自动检测纯ASCII字符串，跳过转换
 * - 跨平台兼容（Windows/Linux）
 * - 智能内存管理，避免内存泄漏
 * - 支持字符串和字符指针输入
 * 
 * 使用场景：
 * - 处理从网络或文件读取的UTF-8数据
 * - 将国际化文本转换为本地显示格式
 * - 与不支持UTF-8的旧系统接口兼容
 */
class UTF8toChar
{
public :
	/**
	 * @brief 构造函数（字符指针版本）
	 * 
	 * @param utf8_string UTF-8编码的字符串指针
	 * 
	 * 使用UTF-8字符串初始化转换对象，自动进行编码转换
	 */
	UTF8toChar(const char *utf8_string)
	{
		init(utf8_string);                     // 调用初始化方法进行编码转换
	}

	/**
	 * @brief 构造函数（字符串版本）
	 * 
	 * @param utf8_string UTF-8编码的字符串引用
	 * 
	 * 使用UTF-8字符串初始化转换对象，自动进行编码转换
	 */
	UTF8toChar(const std::string& utf8_string)
	{
		init(utf8_string.c_str());             // 调用初始化方法进行编码转换
	}

	/**
	 * @brief 初始化并执行编码转换
	 * 
	 * @param utf8_string UTF-8编码的字符串指针
	 * 
	 * 核心转换逻辑，根据平台选择不同的转换方法：
	 * - Windows平台：使用MultiByteToWideChar和WideCharToMultiByte
	 * - Linux平台：使用iconv库进行转换
	 */
	void init(const char *utf8_string)
	{
		if (0 == utf8_string)                   // 检查输入字符串是否为空指针
			t_string = 0;                       // 如果为空指针，设置输出为0
		else if (0 == *utf8_string)             // 检查输入字符串是否为空字符串
		{
			needFree = false;                   // 空字符串不需要释放内存
			t_string = ("");                    // 设置输出为空字符串
		}
		else if ( isPureAscii(utf8_string))     // 检查是否为纯ASCII字符串
		{
			needFree = false;                   // 纯ASCII字符串不需要释放内存
			t_string = (char *)utf8_string;     // 直接使用原字符串，无需转换
		}
		else
		{
			// 非ASCII字符串需要编码转换
			needFree = true;                    // 标记需要释放转换后的内存

			// 计算转换后的字符串长度
			std::size_t string_len = strlen(utf8_string);  // 获取原字符串长度
			std::size_t dst_len = string_len * 2 + 2;      // 估算转换后的长度（通常本地编码比UTF-8长）
#ifdef _MSC_VER
			// Windows平台转换逻辑
			wchar_t *buffer = new wchar_t[string_len + 1];  // 分配宽字符缓冲区
			MultiByteToWideChar(CP_UTF8, 0, utf8_string, -1, buffer, (int)string_len + 1);  // UTF-8转宽字符
			buffer[string_len] = 0;             // 确保字符串以null结尾

			t_string = new char[string_len * 2 + 2];  // 分配目标字符串缓冲区
			WideCharToMultiByte(CP_ACP, 0, buffer, -1, t_string, (int)dst_len, 0, 0);  // 宽字符转ANSI
			t_string[string_len * 2 + 1] = 0;  // 确保字符串以null结尾
			delete[] buffer;                    // 释放宽字符缓冲区
#else
			// Linux平台转换逻辑
			iconv_t cd;                         // 声明iconv转换描述符
			t_string = new char[dst_len];       // 分配目标字符串缓冲区
			char* p = t_string;                 // 获取目标字符串指针
			cd = iconv_open("gb2312", "utf-8"); // 打开UTF-8到GB2312的转换
			if (cd != 0)                        // 检查转换描述符是否有效
			{
				memset(t_string, 0, dst_len);   // 初始化目标缓冲区为0
				iconv(cd, (char**)&utf8_string, &string_len, &p, &dst_len);  // 执行编码转换
				iconv_close(cd);                 // 关闭转换描述符
				t_string[dst_len] = '\0';       // 确保字符串以null结尾
			}
#endif
		}
	}

	/**
	 * @brief 类型转换操作符
	 * 
	 * @return 转换后的本地编码字符串指针
	 * 
	 * 允许对象直接用作字符串指针，简化使用方式
	 */
	operator const char*()
	{
		return t_string;                        // 返回转换后的字符串指针
	}

	/**
	 * @brief 获取转换后的字符串
	 * 
	 * @return 转换后的本地编码字符串指针
	 * 
	 * 显式获取转换后的字符串，推荐使用此方法
	 */
	const char* c_str()
	{
		return t_string;                        // 返回转换后的字符串指针
	}

	/**
	 * @brief 析构函数
	 * 
	 * 自动释放转换过程中分配的内存，实现RAII资源管理
	 */
	~UTF8toChar()
	{
		if (needFree)                           // 检查是否需要释放内存
			delete[] t_string;                  // 释放转换后的字符串内存
	}

private :
	char *t_string;                            // 存储转换后的字符串指针
	bool needFree;                              // 标记是否需要释放内存

	/**
	 * @brief 检查字符串是否为纯ASCII编码
	 * 
	 * @param s 要检查的字符串指针
	 * @return true 为纯ASCII，false 包含非ASCII字符
	 * 
	 * 通过检查每个字符的最高位来判断是否为纯ASCII字符串
	 * 纯ASCII字符串的最高位为0，非ASCII字符串的最高位为1
	 */
	bool isPureAscii(const char *s)
	{
		while (*s != 0) { if (*(s++) & 0x80) return false; }  // 检查每个字符的最高位
		return true;                             // 所有字符都是ASCII，返回true
	}

	// 禁用拷贝构造和赋值操作，防止内存管理问题
	UTF8toChar(const UTF8toChar &rhs);          // 禁用拷贝构造函数
	UTF8toChar &operator=(const UTF8toChar &rhs);  // 禁用赋值操作符
};

/**
 * @brief 本地编码到UTF-8转换类
 * 
 * 该类提供将本地系统编码（Windows下为ANSI，Linux下为GB2312）转换为UTF-8编码的功能。
 * 与UTF8toChar类功能相反，用于将本地数据转换为标准UTF-8格式。
 * 
 * 主要特性：
 * - 自动检测纯ASCII字符串，跳过转换
 * - 跨平台兼容（Windows/Linux）
 * - 智能内存管理，避免内存泄漏
 * - 支持字符串和字符指针输入
 * 
 * 使用场景：
 * - 将本地数据转换为标准UTF-8格式
 * - 数据存储和网络传输前的编码统一
 * - 与支持UTF-8的新系统接口兼容
 */
class ChartoUTF8
{
public :
	/**
	 * @brief 构造函数（字符串版本）
	 * 
	 * @param str 本地编码的字符串引用
	 * 
	 * 使用本地编码字符串初始化转换对象，自动进行编码转换
	 */
	ChartoUTF8(const std::string& str)
	{
		init(str.c_str());                      // 调用初始化方法进行编码转换
	}

	/**
	 * @brief 构造函数（字符指针版本）
	 * 
	 * @param t_string 本地编码的字符串指针
	 * 
	 * 使用本地编码字符串初始化转换对象，自动进行编码转换
	 */
	ChartoUTF8(const char *t_string)
	{
		init(t_string);                         // 调用初始化方法进行编码转换
	}

	/**
	 * @brief 初始化并执行编码转换
	 * 
	 * @param t_string 本地编码的字符串指针
	 * 
	 * 核心转换逻辑，将本地编码转换为UTF-8格式：
	 * - Windows平台：使用MultiByteToWideChar和WideCharToMultiByte
	 * - Linux平台：使用iconv库进行转换
	 */
	void init(const char *t_string)
	{
		if (0 == t_string)                      // 检查输入字符串是否为空指针
			utf8_string = 0;                    // 如果为空指针，设置输出为0
		else if (0 == *t_string)                // 检查输入字符串是否为空字符串
		{
			utf8_string = "";                   // 设置输出为空字符串
			needFree = false;                   // 空字符串不需要释放内存
		}
		else if (isPureAscii((char *)t_string)) // 检查是否为纯ASCII字符串
		{
			utf8_string = (char *)t_string;     // 纯ASCII字符串直接使用，无需转换
			needFree = false;                   // 不需要释放内存
		}
		else
		{
			// 非ASCII字符串需要编码转换
			needFree = true;                    // 标记需要释放转换后的内存

			// 计算转换后的字符串长度
			std::size_t string_len = strlen(t_string);  // 获取原字符串长度
			std::size_t dst_len = string_len * 5;       // 估算转换后的长度（UTF-8通常比本地编码长）
#ifdef _MSC_VER		
			// Windows平台转换逻辑
			// 先转换为宽字符（Unicode）
			wchar_t *w_string = new wchar_t[string_len + 1];  // 分配宽字符缓冲区
			MultiByteToWideChar(CP_ACP, 0, t_string, -1, w_string, (int)string_len + 1);  // ANSI转宽字符
			w_string[string_len] = 0;            // 确保字符串以null结尾

			// 再从宽字符转换为UTF-8
			utf8_string = new char[dst_len];    // 分配目标字符串缓冲区
			WideCharToMultiByte(CP_UTF8, 0, w_string, -1, utf8_string, (int)dst_len, 0, 0);  // 宽字符转UTF-8
			utf8_string[string_len * 3] = 0;    // 确保字符串以null结尾

			if (w_string != (wchar_t *)t_string) // 检查是否为临时分配的宽字符缓冲区
				delete[] w_string;               // 释放宽字符缓冲区
#else
			// Linux平台转换逻辑
			iconv_t cd;                         // 声明iconv转换描述符
			utf8_string = new char[dst_len];    // 分配目标字符串缓冲区
			char* p = utf8_string;              // 获取目标字符串指针
			cd = iconv_open("utf-8", "gb2312"); // 打开GB2312到UTF-8的转换
			if (cd != 0)                        // 检查转换描述符是否有效
			{
				memset(utf8_string, 0, dst_len); // 初始化目标缓冲区为0
				iconv(cd, (char**)&t_string, &string_len, &p, &dst_len);  // 执行编码转换
				iconv_close(cd);                 // 关闭转换描述符
			}
#endif
		}
	}

	/**
	 * @brief 类型转换操作符
	 * 
	 * @return 转换后的UTF-8字符串指针
	 * 
	 * 允许对象直接用作字符串指针，简化使用方式
	 */
	operator const char*()
	{
		return utf8_string;                     // 返回转换后的UTF-8字符串指针
	}

	/**
	 * @brief 获取转换后的字符串
	 * 
	 * @return 转换后的UTF-8字符串指针
	 * 
	 * 显式获取转换后的字符串，推荐使用此方法
	 */
	const char* c_str() const
	{
		return utf8_string;                     // 返回转换后的UTF-8字符串指针
	}

	/**
	 * @brief 析构函数
	 * 
	 * 自动释放转换过程中分配的内存，实现RAII资源管理
	 */
	~ChartoUTF8()
	{
		if (needFree)                           // 检查是否需要释放内存
			delete[] utf8_string;               // 释放转换后的字符串内存
	}

private :
	char *utf8_string;                          // 存储转换后的UTF-8字符串指针
	bool needFree;                              // 标记是否需要释放内存

	/**
	 * @brief 检查字符串是否为纯ASCII编码
	 * 
	 * @param s 要检查的字符串指针
	 * @return true 为纯ASCII，false 包含非ASCII字符
	 * 
	 * 通过检查每个字符的最高位来判断是否为纯ASCII字符串
	 */
	bool isPureAscii(const char *s)
	{
		while (*s != 0) { if (*(s++) & 0x80) return false; }  // 检查每个字符的最高位
		return true;                             // 所有字符都是ASCII，返回true
	}

	// 禁用拷贝构造和赋值操作，防止内存管理问题
	ChartoUTF8(const ChartoUTF8 &rhs);          // 禁用拷贝构造函数
	ChartoUTF8 &operator=(const ChartoUTF8 &rhs);  // 禁用赋值操作符
};

/**
 * @brief URL编码类
 * 
 * 该类提供将普通字符串转换为URL编码格式的功能，确保字符串可以安全地用于URL中。
 * 支持ASCII和非ASCII字符的编码，非ASCII字符会被转换为百分号编码格式。
 * 
 * 主要特性：
 * - 自动检测ASCII和非ASCII字符
 * - 将空格转换为%20
 * - 将非ASCII字符转换为十六进制编码
 * - 支持中文字符等Unicode字符
 * 
 * 使用场景：
 * - HTTP请求参数编码
 * - 文件下载链接生成
 * - 网络通信中的安全传输
 */
class URLEncode
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param src 要编码的源字符串
	 * 
	 * 自动将源字符串转换为URL编码格式，支持ASCII和非ASCII字符
	 */
	URLEncode(const char* src)
	{ 
		char hex[] = "0123456789ABCDEF";       // 十六进制字符表，用于编码转换

		for (unsigned int i = 0; i < strlen(src); ++i)  // 遍历源字符串的每个字符
		{  
			const char cc = src[i];             // 获取当前字符
			if (isPureAscii(&cc))               // 检查是否为纯ASCII字符
			{  
				if (cc == ' ')                   // 检查是否为空格字符
				{  
					encoded_string += "%20";     // 空格转换为%20
				}  
				else 
					encoded_string += cc;        // ASCII字符直接添加
			}  
			else 
			{  
				// 非ASCII字符转换为百分号编码
				unsigned char c = static_cast<unsigned char>(src[i]);  // 转换为无符号字符
				encoded_string += '%';           // 添加百分号前缀
				encoded_string += hex[c / 16];   // 添加高4位十六进制字符
				encoded_string += hex[c % 16];   // 添加低4位十六进制字符
			}  
		}  
	}
	
	/**
	 * @brief 类型转换操作符
	 * 
	 * @return 编码后的URL字符串
	 * 
	 * 允许对象直接用作字符串，简化使用方式
	 */
	operator const char*(){return encoded_string.c_str();}  // 返回编码后的字符串

private:
	/**
	 * @brief 检查字符串是否为纯ASCII编码
	 * 
	 * @param s 要检查的字符串指针
	 * @return true 为纯ASCII，false 包含非ASCII字符
	 * 
	 * 通过检查每个字符的最高位来判断是否为纯ASCII字符串
	 */
	bool isPureAscii(const char *s)
	{
		while (*s != 0) { if (*(s++) & 0x80) return false; }  // 检查每个字符的最高位
		return true;                             // 所有字符都是ASCII，返回true
	}

private:
	std::string encoded_string;                 // 存储编码后的URL字符串
};

/**
 * @brief URL解码类
 * 
 * 该类提供将URL编码格式的字符串转换回普通字符串的功能。
 * 支持%XX格式的十六进制编码和+号表示的空格。
 * 
 * 主要特性：
 * - 将%20转换回空格
 * - 将+号转换回空格
 * - 将%XX格式的十六进制编码转换回原字符
 * - 智能判断是否需要解码
 * 
 * 使用场景：
 * - HTTP请求参数解码
 * - URL解析和处理
 * - 网络通信中的数据接收
 */
class URLDecode
{
public:
	/**
	 * @brief 构造函数
	 * 
	 * @param src 要解码的URL编码字符串
	 * 
	 * 自动将URL编码字符串解码为普通字符串，处理各种编码格式
	 */
	URLDecode(const char* src)
	{ 
		int hex = 0;                           // 存储十六进制值
		for (unsigned int i = 0; i < strlen(src); ++i)  // 遍历源字符串的每个字符
		{  
			switch (src[i])                     // 根据当前字符类型进行处理
			{  
			case '+':                           // 处理+号（表示空格）
				decoded_string += ' ';           // 将+号转换为空格
				break;  
			case '%':                           // 处理百分号编码
				if (isxdigit(src[i + 1]) && isxdigit(src[i + 2]))  // 检查后面两个字符是否为十六进制
				{
					std::string hexStr;          // 创建十六进制字符串
					hexStr += src[i+1];          // 添加第一个十六进制字符
					hexStr += src[i+2];          // 添加第二个十六进制字符
					hex = strtol(hexStr.c_str(), 0, 16);  // 将十六进制字符串转换为整数
					// 判断是否需要解码：字母、数字、特殊符号和保留字可以不经过编码直接用于URL
					if (!((hex >= 48 && hex <= 57) || //0-9  
						(hex >=97 && hex <= 122) ||   //a-z  
						(hex >=65 && hex <= 90) ||    //A-Z  
						//一些特殊符号及保留字[$-_.+!*'(),]  [$&+,/:;=?@]  
						hex == 0x21 || hex == 0x24 || hex == 0x26 || hex == 0x27 || hex == 0x28 || hex == 0x29 
						|| hex == 0x2a || hex == 0x2b|| hex == 0x2c || hex == 0x2d || hex == 0x2e || hex == 0x2f 
						|| hex == 0x3A || hex == 0x3B|| hex == 0x3D || hex == 0x3f || hex == 0x40 || hex == 0x5f 
						))  
					{  
						decoded_string += char(hex);  // 将十六进制值转换为字符并添加
						i += 2;                   // 跳过已处理的十六进制字符
					}  
					else decoded_string += '%';   // 不需要解码的字符保持原样
				}else {  
					decoded_string += '%';        // 无效的百分号编码保持原样
				}  
				break;  
			default:
				decoded_string += src[i];         // 其他字符直接添加
				break;  
			}  
		}  
	}

	/**
	 * @brief 类型转换操作符
	 * 
	 * @return 解码后的普通字符串
	 * 
	 * 允许对象直接用作字符串，简化使用方式
	 */
	operator const char*(){return decoded_string.c_str();}  // 返回解码后的字符串

private:
	std::string decoded_string;                 // 存储解码后的普通字符串
};

/**
 * @brief 编码格式检测辅助类
 * 
 * 该类提供检测字符串编码格式的功能，支持UTF-8和GBK编码的识别。
 * 通过分析字节序列的特征来判断编码类型，为后续的编码转换提供依据。
 * 
 * 主要功能：
 * - UTF-8编码格式检测
 * - GBK编码格式检测
 * - 编码格式的智能识别
 * 
 * 使用场景：
 * - 数据源编码格式的自动识别
 * - 编码转换前的格式判断
 * - 数据完整性验证
 */
class EncodingHelper
{
public:
	/**
	 * @brief 检测字符串是否为GBK编码
	 * 
	 * @param data 要检测的数据指针
	 * @param len 数据长度
	 * @return true 为GBK编码，false 不是GBK编码
	 * 
	 * 通过分析字节序列的特征来判断是否为GBK编码：
	 * - 单字节：0x00-0x7F（兼容ASCII）
	 * - 双字节：0x81-0xFE + 0x40-0xFE（排除0xF7）
	 */
	static bool isGBK(unsigned char* data, std::size_t len) {
		std::size_t i = 0;                      // 初始化索引
		while (i < len) {                       // 遍历所有字节
			if (data[i] <= 0x7f) {              // 检查是否为单字节编码
				//编码小于等于127,只有一个字节的编码，兼容ASCII
				i++;                             // 移动到下一个字节
				continue;                        // 继续处理下一个字节
			}
			else {
				//大于127的使用双字节编码
				if (data[i] >= 0x81 &&
					data[i] <= 0xfe &&
					data[i + 1] >= 0x40 &&
					data[i + 1] <= 0xfe &&
					data[i + 1] != 0xf7) 
				{
					//如果有GBK编码的，就算整个字符串都是GBK编码
					return true;                  // 检测到GBK编码，返回true
				}
			}
		}
		return false;                            // 未检测到GBK编码，返回false
	}

	/**
	 * @brief 计算UTF-8字符的字节数
	 * 
	 * @param byte 要检查的字节
	 * @return UTF-8字符的字节数
	 * 
	 * 通过分析字节的高位1的个数来判断UTF-8字符的字节数：
	 * - 0xxx xxxx：1字节
	 * - 110x xxxx：2字节
	 * - 1110 xxxx：3字节
	 * - 1111 0xxx：4字节
	 * - 1111 10xx：5字节
	 * - 1111 110x：6字节
	 */
	static int preNUm(unsigned char byte) {
		unsigned char mask = 0x80;               // 初始化掩码为1000 0000
		int num = 0;                            // 初始化字节数计数器
		for (int i = 0; i < 8; i++) {           // 检查8位
			if ((byte & mask) == mask) {        // 检查当前位是否为1
				mask = mask >> 1;                // 掩码右移一位
				num++;                           // 字节数加1
			}
			else {
				break;                           // 遇到0位，停止计数
			}
		}
		return num;                              // 返回字节数
	}

	/**
	 * @brief 检测字符串是否为UTF-8编码
	 * 
	 * @param data 要检测的数据指针
	 * @param len 数据长度
	 * @return true 为UTF-8编码，false 不是UTF-8编码
	 * 
	 * 通过分析UTF-8编码的字节序列特征来判断编码格式：
	 * - 单字节：0xxx xxxx
	 * - 多字节：首字节以1开头，后续字节以10开头
	 * - 验证字节序列的完整性和有效性
	 */
	static bool isUtf8(unsigned char* data, std::size_t len) {
		int num = 0;                            // 存储UTF-8字符的字节数
		std::size_t i = 0;                      // 初始化索引
		while (i < len) {                       // 遍历所有字节
			if ((data[i] & 0x80) == 0x00)      // 检查是否为单字节UTF-8字符
			{
				// 0XXX_XXXX
				i++;                             // 移动到下一个字节
				continue;                        // 继续处理下一个字节
			}
			else if ((num = preNUm(data[i])) > 2)  // 检查是否为多字节UTF-8字符
			{
				// 110X_XXXX 10XX_XXXX
				// 1110_XXXX 10XX_XXXX 10XX_XXXX
				// 1111_0XXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
				// 1111_10XX 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
				// 1111_110X 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
				// preNUm() 返回首个字节8个bits中首bit前面1bit的个数，该数量也是该字符所使用的字节数        
				i++;                             // 移动到下一个字节
				for (int j = 0; j < num - 1; j++) {  // 检查后续字节
					//判断后面num - 1 个字节是不是都是10开
					if ((data[i] & 0xc0) != 0x80) {  // 检查是否为10开头的字节
						return false;             // 如果不是，返回false
					}
					i++;                         // 移动到下一个字节
				}
			}
			else 
			{
				//其他情况说明不是utf-8
				return false;                    // 返回false
			}
		}
		return true;                             // 所有字节都符合UTF-8格式，返回true
	}
};