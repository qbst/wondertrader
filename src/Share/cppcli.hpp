/*!
 * \file cppcli.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief C++命令行参数解析库
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了一个完整的C++命令行参数解析库，提供了灵活、易用的命令行参数处理功能。
 * 主要功能包括：
 * 1. 命令行参数的解析和验证
 * 2. 支持短参数和长参数格式
 * 3. 参数类型验证（字符串、整数、浮点数）
 * 4. 参数值范围限制和枚举值限制
 * 5. 必需参数检查
 * 6. 自动生成帮助文档
 * 7. 跨平台兼容性（Windows/Linux）
 * 
 * 该类主要用于WonderTrader框架中各种命令行工具的参数处理，如：
 * - 回测程序的参数配置
 * - 数据导入导出工具的参数设置
 * - 策略运行时的参数传递
 * - 系统配置和调试工具的参数处理
 * 
 * 通过提供统一的命令行参数处理接口，简化了框架中各种工具程序的开发。
 */
#pragma once

#include <algorithm>                            // 标准算法库头文件
#include <assert.h>                             // 断言头文件
#include <fstream>                              // 文件流头文件
#include <functional>                           // 函数对象头文件
#include <iomanip>                              // 输入输出流操作头文件
#include <iostream>                             // 输入输出流头文件
#include <map>                                  // 映射容器头文件
#include <mutex>                                // 互斥锁头文件
#include <ostream>                              // 输出流头文件
#include <sstream>                              // 字符串流头文件
#include <stdlib.h>                             // 标准库头文件
#include <string>                               // 字符串头文件
#include <vector>                               // 向量容器头文件

#if defined(WIN32) || defined(_WIN64) || defined(__WIN32__)
    #include "direct.h"                         // Windows目录操作头文件
    #include <windows.h>                        // Windows API头文件
    #define CPPCLI_SEPARATOR_TYPE    "\\"       // Windows路径分隔符
    #define CPPCLI_SEPARATOR_NO_TYPE "/"        // 非Windows路径分隔符
#else
    #include <unistd.h>                         // Unix标准头文件
    #define CPPCLI_SEPARATOR_TYPE    "/"        // Unix路径分隔符
    #define CPPCLI_SEPARATOR_NO_TYPE "\\"       // 非Unix路径分隔符
#endif

// #define CPPCLI_DEBUG                          // 调试模式开关（注释掉表示关闭）

/**
 * @brief cppcli命名空间
 * 
 * 包含命令行参数解析库的所有类和功能，提供完整的命令行参数处理解决方案。
 */
namespace cppcli {
    static std::mutex _coutMutex;               // 全局输出互斥锁，确保多线程环境下的输出安全

#ifdef CPPCLI_DEBUG
    /**
     * @brief 调试信息打印函数模板
     * 
     * @tparam Args 可变参数类型
     * @param args 要打印的参数
     * 
     * 在调试模式下打印调试信息，支持任意数量和类型的参数
     */
    template <class... Args>
    void __cppcli_debug_print(const Args &...args)
    {
        std::unique_lock<std::mutex> lock(_coutMutex);  // 获取输出锁
        std::cout << "[CPPCLI_DEBUG] ";                 // 打印调试标识
        auto printFunc = [](auto i) { std::cout << i; }; // 定义打印函数
        std::initializer_list<int>{(printFunc(args), 0)...};  // 依次打印所有参数
        std::cout << std::endl;                          // 换行
    }

#endif
}   // namespace cppcli

#ifdef CPPCLI_DEBUG
    #define CPPCLI_DEBUG_PRINT(...) cppcli::__cppcli_debug_print(__VA_ARGS__)  // 调试打印宏定义
#endif

namespace cppcli {
    class Option;                               // 前向声明：选项类
    class Rule;                                 // 前向声明：规则类

    /**
     * @brief 错误退出枚举类型
     * 
     * 定义命令行参数解析失败时的退出行为
     */
    enum ErrorExitEnum {
        EXIT_PRINT_RULE = 0x00,                 // 只打印规则信息
        EXIT_PRINT_RULE_HELPDOC = 0x01,         // 打印规则信息和帮助文档
    };
    
    /**
     * @brief 帮助文档枚举类型
     * 
     * 定义帮助文档的显示方式
     */
    enum HelpDocEnum {
        USE_DEFAULT_HELPDOC = 0x00,             // 使用默认帮助文档
        USE_UER_DEFINED_HELPDOC = 0x01,         // 使用用户自定义帮助文档
    };

    /**
     * @brief 详细实现命名空间
     * 
     * 包含库的内部实现细节，不直接暴露给用户
     */
    namespace detail {
        /**
         * @brief 错误事件类型枚举
         * 
         * 定义各种参数验证错误的类型
         */
        enum ErrorEventType {
            MECESSARY_ERROR = 0x00,             // 必需参数缺失错误
            VALUETYPE_ERROR = 0x01,             // 参数值类型错误
            ONEOF_ERROR = 0x02,                 // 参数值不在允许范围内错误
            NUMRANGE_ERROR = 0x03,              // 参数值超出数值范围错误
        };

        /**
         * @brief 参数值类型枚举
         * 
         * 定义支持的命令行参数值类型
         */
        enum ValueTypeEnum {
            STRING = 0x00,                      // 字符串类型
            INT = 0x01,                         // 整数类型
            DOUBLE = 0x02,                      // 浮点数类型
        };

        /**
         * @brief 路径工具类
         * 
         * 提供文件路径相关的工具方法，如文件名提取、路径分割等
         */
        class pathUtil final {
          private:
            /**
             * @brief 字符串全局替换方法
             * 
             * @param str 目标字符串
             * @param pattern 要替换的模式
             * @param newpat 新的替换内容
             * @return 替换的次数
             * 
             * 在字符串中全局替换指定的模式
             */
            static int replace_all(std::string &str, const std::string &pattern, const std::string &newpat);

          public:
            /**
             * @brief 获取文件名（包含扩展名）
             * 
             * @param filePath 文件路径
             * @return 文件名
             * 
             * 从完整文件路径中提取文件名部分
             */
            static std::string getFilename(const std::string &filePath);
            
            /**
             * @brief 获取文件名（不包含扩展名）
             * 
             * @param filePath 文件路径
             * @return 不含扩展名的文件名
             * 
             * 从完整文件路径中提取文件名，去除扩展名
             */
            static std::string getFilenameWithOutSuffix(const std::string &filePath);
            
            /**
             * @brief 获取文件扩展名
             * 
             * @param filePath 文件路径
             * @return 文件扩展名
             * 
             * 从完整文件路径中提取文件扩展名
             */
            static std::string getFileSuffix(const std::string &filePath);
            
            /**
             * @brief 获取文件目录
             * 
             * @param filePath 文件路径
             * @return 文件目录路径
             * 
             * 从完整文件路径中提取文件所在的目录路径
             */
            static std::string getFileDir(const std::string &filePath);
        };
        
        /**
         * @brief 算法工具类
         * 
         * 提供命令行参数解析相关的算法方法
         */
        class algoUtil final {
          private:
          public:
            /**
             * @brief 初始化命令映射
             * 
             * @param length 参数数组长度
             * @param strArr 参数字符串数组
             * @param stringMap 输出参数映射
             * 
             * 将命令行参数解析为键值对映射，支持短参数和长参数格式
             */
            static void InitCommandMap(int length, char *strArr[], std::map<std::string, std::string> &stringMap);
            
            /**
             * @brief 检查字符串是否为整数
             * 
             * @param value 要检查的字符串
             * @return true 是整数，false 不是整数
             * 
             * 验证字符串是否可以转换为整数类型
             */
            static bool isInt(const std::string &value);
            
            /**
             * @brief 检查字符串是否为浮点数
             * 
             * @param value 要检查的字符串
             * @return true 是浮点数，false 不是浮点数
             * 
             * 验证字符串是否可以转换为浮点数类型
             */
            static bool isDouble(const std::string &value);
            
            /**
             * @brief 验证字符串是否为数值类型
             * 
             * @param value 要检查的字符串
             * @return true 是数值类型，false 不是数值类型
             * 
             * 验证字符串是否为整数或浮点数
             */
            static bool verifyDouble(const std::string &value);
        };


    }   // namespace detail
}   // namespace cppcli

/**
 * @brief 字符串全局替换方法
 * 
 * @param str 目标字符串
 * @param pattern 要替换的模式
 * @param newpat 新的替换内容
 * @return 替换的次数
 * 
 * 在字符串中全局替换指定的模式，返回实际替换的次数
 */
int cppcli::detail::pathUtil::replace_all(std::string &str, const std::string &pattern, const std::string &newpat)
{

    int count = 0;                              // 初始化替换计数器
    const size_t nsize = newpat.size();         // 获取新字符串的长度
    const size_t psize = pattern.size();        // 获取模式字符串的长度

    for (size_t pos = str.find(pattern, 0); pos != std::string::npos; pos = str.find(pattern, pos + nsize))  // 循环查找并替换所有匹配的模式
    {
        str.replace(pos, psize, newpat);        // 替换当前位置的模式
        count++;                                // 计数器递增
    }
    return count;                               // 返回替换次数
}

/**
 * @brief 获取文件名（包含扩展名）
 * 
 * @param filePath 文件路径
 * @return 文件名
 * 
 * 从完整文件路径中提取文件名部分，支持跨平台路径分隔符
 */
std::string cppcli::detail::pathUtil::getFilename(const std::string &filePath)
{
    std::string filePathCopy(filePath);         // 创建文件路径的副本
    replace_all(filePathCopy, CPPCLI_SEPARATOR_NO_TYPE, CPPCLI_SEPARATOR_TYPE);  // 统一路径分隔符

    assert(std::ifstream(filePathCopy.c_str()).good());  // 断言文件路径有效
    std::string::size_type pos = filePathCopy.find_last_of(CPPCLI_SEPARATOR_TYPE) + 1;  // 查找最后一个分隔符位置
    return std::move(filePathCopy.substr(pos, filePathCopy.length() - pos));  // 返回文件名部分
}

/**
 * @brief 获取文件名（不包含扩展名）
 * 
 * @param filePath 文件路径
 * @return 不含扩展名的文件名
 * 
 * 从完整文件路径中提取文件名，去除扩展名部分
 */
std::string cppcli::detail::pathUtil::getFilenameWithOutSuffix(const std::string &filePath)
{
    std::string filename = getFilename(filePath);  // 先获取完整文件名
    return std::move(filename.substr(0, filename.rfind(".")));  // 截取到最后一个点之前的部分
}

/**
 * @brief 获取文件扩展名
 * 
 * @param filePath 文件路径
 * @return 文件扩展名
 * 
 * 从完整文件路径中提取文件扩展名部分
 */
std::string cppcli::detail::pathUtil::getFileSuffix(const std::string &filePath)
{
    std::string filename = getFilename(filePath);  // 先获取完整文件名
    return std::move(filename.substr(filename.find_last_of('.') + 1));  // 截取最后一个点之后的部分
}

/**
 * @brief 获取文件目录
 * 
 * @param filePath 文件路径
 * @return 文件目录路径
 * 
 * 从完整文件路径中提取文件所在的目录路径
 */
std::string cppcli::detail::pathUtil::getFileDir(const std::string &filePath)
{
    std::string filePathCopy(filePath);  // 创建文件路径的副本
    replace_all(filePathCopy, CPPCLI_SEPARATOR_NO_TYPE, CPPCLI_SEPARATOR_TYPE);  // 统一路径分隔符
    std::string::size_type pos = filePathCopy.find_last_of(CPPCLI_SEPARATOR_TYPE);  // 查找最后一个分隔符位置
    return std::move(filePathCopy.substr(0, pos));  // 返回目录部分（不包含文件名）
}



/**
 * @brief 初始化命令映射
 * 
 * @param length 参数数组长度
 * @param strArr 参数字符串数组
 * @param stringMap 输出参数映射
 * 
 * 将命令行参数解析为键值对映射，支持短参数和长参数格式
 * 跳过第一个参数（程序名），从第二个参数开始解析
 */
void cppcli::detail::algoUtil::InitCommandMap(int length, char *strArr[], std::map<std::string, std::string> &stringMap)
{
    // 初始化临时变量
    std::string keyTmp;                           // 临时存储参数键
    std::string valueTmp;                         // 临时存储参数值
    int keyIndex = -1;                            // 当前参数键的索引位置

    for (int currentIndex = 1; currentIndex < length; currentIndex++)  // 从索引1开始，跳过程序名
    {
        std::string theStr(strArr[currentIndex]);  // 获取当前参数字符串
        if (keyIndex != -1 && theStr.size() > 0 && currentIndex == keyIndex + 1)  // 如果之前有键且当前是下一个参数
        {
            // 判断当前字符串是否为命令键，如果是则设置值为空字符串
            if(theStr.find_first_of('-') == 0 && theStr.size() > 1 && !isdigit(theStr.at(1)))
            {
                valueTmp = "";                     // 如果是以-开头的命令键，设置值为空
            }
            else
            {   
                valueTmp = theStr;                 // 否则使用当前字符串作为值
            }

            stringMap.insert(std::make_pair(std::move(keyTmp), std::move(valueTmp)));  // 将键值对插入映射
            keyTmp.clear();                        // 清空临时键
            valueTmp.clear();                      // 清空临时值
            keyIndex = -1;                         // 重置键索引
        }

        // 检查当前字符串是否为参数键（以-开头且不是数字）
        if (theStr.find_first_of('-') == 0 && int(std::count(theStr.begin(), theStr.end(), '-')) < theStr.size() && !isdigit(theStr.at(1)))
        {
            keyIndex = currentIndex;               // 记录当前键的索引位置
            keyTmp = std::move(theStr);            // 保存当前键
        }

        // 处理最后一个参数的情况
        if (currentIndex == length - 1 && keyIndex != -1)
        {
            stringMap.insert(std::make_pair(std::move(keyTmp), std::move("")));  // 最后一个键没有值，设置为空字符串
        }
    }
}
/**
 * @brief 检查字符串是否为整数
 * 
 * @param value 要检查的字符串
 * @return true 是整数，false 不是整数
 * 
 * 验证字符串是否可以转换为整数类型，支持负数
 */
bool cppcli::detail::algoUtil::isInt(const std::string &value)
{
    if (value.empty())                             // 检查字符串是否为空
    {
        return false;                              // 空字符串不是整数
    }

    int startPos = value.at(0) == '-' ? 1 : 0;    // 如果第一个字符是负号，从第二个字符开始检查
    for (int i = startPos; i < value.size(); i++)  // 遍历字符串的每个字符
    {
        if (isdigit(value.at(i)) == 0)             // 检查字符是否为数字
            return false;                          // 如果不是数字，返回false
    }
    return true;                                   // 所有字符都是数字，返回true
}

/**
 * @brief 检查字符串是否为浮点数
 * 
 * @param value 要检查的字符串
 * @return true 是浮点数，false 不是浮点数
 * 
 * 验证字符串是否可以转换为浮点数类型，支持负数和小数点
 */
bool cppcli::detail::algoUtil::isDouble(const std::string &value)
{
    if (value.empty())                             // 检查字符串是否为空
    {
        return false;                              // 空字符串不是浮点数
    }
    if (value.size() < 3)                          // 浮点数至少需要3个字符（如"0.1"）
        return false;                              // 长度不足，返回false
    
    std::string tmpValue = value.at(0) == '-' ? value.substr(0, value.npos) : value;  // 处理负号
    int numCount = 0;                              // 数字字符计数器
    for (char const &c : tmpValue)                 // 遍历字符串的每个字符
    {
        if (isdigit(c) != 0)                       // 检查字符是否为数字
            numCount++;                             // 数字字符计数加1
    }

    // 检查是否为有效的浮点数格式：数字字符数=总字符数-1（小数点），且小数点位置合理
    if (numCount == tmpValue.size() - 1 && tmpValue.rfind('.') > 0 && tmpValue.rfind('.') < tmpValue.size() - 1)
    {
        return true;                               // 符合浮点数格式，返回true
    }
    return false;                                  // 不符合格式，返回false
}

/**
 * @brief 验证字符串是否为数值类型
 * 
 * @param value 要检查的字符串
 * @return true 是数值类型，false 不是数值类型
 * 
 * 验证字符串是否为整数或浮点数，用于参数类型验证
 */
bool cppcli::detail::algoUtil::verifyDouble(const std::string &value)
{
    if (isInt(value) || isDouble(value))           // 检查是否为整数或浮点数
        return true;                               // 是数值类型，返回true
    return false;                                  // 不是数值类型，返回false
}

/* ################################################################### */
/* ################################################################### */
/* ################################################################### */
/* ################################################################### */
/* ################################################################### */

namespace cppcli {

    class Rule {
      private:
        class detail {
          public:
            struct HelpDocStruct {
                static cppcli::HelpDocEnum _helpDocType;
                static cppcli::Rule *rule;
            };
        };

      private:
        friend class Option;                        // 允许Option类访问私有成员

        std::string _inputValue;                    // 用户输入的参数值
        std::string _shortParam;                    // 短参数名（如"-h"）
        std::string _longParam;                     // 长参数名（如"--help"）
        std::string _helpInfo;                      // 参数的帮助信息描述
        bool _necessary = false;                    // 是否为必需参数
        std::vector<std::string> _limitOneVec;      // 参数值的枚举限制列表
        std::pair<double, double> _limitNumRange;   // 数值参数的范围限制（最小值，最大值）
        cppcli::detail::ValueTypeEnum _valueType = cppcli::detail::ValueTypeEnum::STRING;  // 参数值类型（字符串/整数/浮点数）
        std::string _default = "[EMPTY]";           // 参数的默认值
        std::string _errorInfo;                     // 错误信息
        bool _existsInMap = false;                  // 参数是否存在于命令行参数映射中

      public:
        Rule() = delete;

        Rule(const std::string &, const std::string &) = delete;
        Rule(const std::string &) = delete;

        // Rule& operator=(const cppcli::Rule&) = delete;
        /**
         * @brief 构造函数（非必需参数）
         * 
         * @param shortParam 短参数名
         * @param longParam 长参数名
         * @param helpInfo 帮助信息
         * 
         * 创建非必需参数的规则对象，数值范围限制初始化为无效值
         */
        Rule(const std::string &shortParam, const std::string &longParam, const std::string helpInfo)
            : _shortParam(shortParam), _longParam(longParam), _helpInfo(helpInfo),
              _limitNumRange(std::make_pair(double(-1), double(-1))){};  // 数值范围限制初始化为无效值

        /**
         * @brief 构造函数（可指定是否必需）
         * 
         * @param shortParam 短参数名
         * @param longParam 长参数名
         * @param helpInfo 帮助信息
         * @param necessary 是否为必需参数
         * 
         * 创建参数规则对象，可以指定参数是否必需
         */
        Rule(const std::string &shortParam, const std::string &longParam, const std::string helpInfo, bool necessary)
            : _shortParam(shortParam), _longParam(longParam), _helpInfo(helpInfo), _necessary(necessary),
              _limitNumRange(std::make_pair(double(-1), double(-1))){};  // 数值范围限制初始化为无效值

        /**
         * @brief 限制参数值为整数类型
         * 
         * @return 当前Rule对象指针，支持链式调用
         * 
         * 设置参数值类型为整数，用于后续的类型验证
         */
        Rule *limitInt();   // 设置参数值类型为整数

        /**
         * @brief 限制参数值为浮点数类型
         * 
         * @return 当前Rule对象指针，支持链式调用
         * 
         * 设置参数值类型为浮点数，用于后续的类型验证
         */
        Rule *limitDouble();  // 设置参数值类型为浮点数

        /**
         * @brief 将当前参数设置为帮助参数
         * 
         * @return 当前Rule对象指针，支持链式调用
         * 
         * 设置当前参数为帮助参数，用户可以通过该参数查看帮助信息
         */
        Rule *asHelpParam();  // 设置为帮助参数

        /**
         * @brief 检查参数是否存在于命令行参数中
         * 
         * @return true 参数存在，false 参数不存在
         * 
         * 检查用户是否在命令行中提供了该参数
         */
        bool exists();  // 检查参数是否存在

        /**
         * @brief 获取字符串类型的参数值
         * 
         * @tparam T 模板参数类型，必须为std::string
         * @return 参数值字符串
         * 
         * 模板特化：当T为std::string时，直接返回输入值
         */
        template <class T, class = typename std::enable_if<std::is_same<T, std::string>::value>::type>
        const std::string get()
        {
            return _inputValue;                     // 返回字符串类型的参数值
        }

        /**
         * @brief 获取整数类型的参数值
         * 
         * @tparam T 模板参数类型，必须为int
         * @return 参数值整数
         * 
         * 模板特化：当T为int时，将字符串转换为整数
         */
        template <class T, class = typename std::enable_if<std::is_same<T, int>::value>::type>
        int get()
        {
            return std::stoi(_inputValue);          // 将字符串转换为整数并返回
        }

        /**
         * @brief 获取浮点数类型的参数值
         * 
         * @tparam T 模板参数类型，必须为double
         * @return 参数值浮点数
         * 
         * 模板特化：当T为double时，将字符串转换为浮点数
         */
        template <class T, class = typename std::enable_if<std::is_same<T, double>::value>::type>
        double get()
        {
            return std::stod(_inputValue);          // 将字符串转换为浮点数并返回
        }

        /**
         * @brief 限制参数值为指定的枚举值之一
         * 
         * @tparam Args 可变参数类型
         * @param args 允许的参数值列表
         * @return 当前Rule对象指针，支持链式调用
         * 
         * 设置参数值的枚举限制，用户只能输入指定的值之一
         */
        template <class... Args>
        Rule *limitOneOf(Args... args)
        {
            std::ostringstream oss;                 // 创建字符串流用于类型转换

            // 定义lambda函数，将每个参数值添加到限制列表中
            auto addToVec = [this, &oss](auto i) {
                oss << i;                           // 将参数值转换为字符串
                _limitOneVec.push_back(std::move(oss.str()));  // 添加到限制列表
                oss.str("");                        // 清空字符串流
            };
            
            // 使用折叠表达式处理所有参数
            std::initializer_list<int>{(addToVec(args), 0)...};  // 依次处理每个参数
            return this;                            // 返回当前对象，支持链式调用
        }

        /**
         * @brief 限制数值参数的范围
         * 
         * @tparam T 数值类型，支持int、float、double
         * @param min 最小值
         * @param max 最大值
         * @return 当前Rule对象指针，支持链式调用
         * 
         * 设置数值参数的有效范围，超出范围的值将被拒绝
         */
        template <class T,
                  class = typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, float>::value ||
                                                  std::is_same<T, double>::value>::type>
        Rule *limitNumRange(T min, T max)
        {
            _limitNumRange = std::make_pair(double(min), double(max));  // 将范围限制存储为double类型
            return this;                            // 返回当前对象，支持链式调用
        }

        /**
         * @brief 设置参数的默认值
         * 
         * @tparam T 默认值类型
         * @param defaultValue 默认值
         * @return 当前Rule对象指针，支持链式调用
         * 
         * 当用户未提供参数值时，使用指定的默认值
         */
        template <class T>
        Rule *setDefault(const T &defaultValue)
        {
            std::ostringstream oss;                 // 创建字符串流
            oss << defaultValue;                    // 将默认值转换为字符串
            _default = oss.str();                   // 存储默认值字符串
            return this;                            // 返回当前对象，支持链式调用
        }

      private:
        /**
         * @brief 获取错误信息
         * 
         * @param errorEventType 错误事件类型
         * @return 格式化的错误信息字符串
         * 
         * 根据错误类型生成相应的错误提示信息
         */
        const std::string getError(cppcli::detail::ErrorEventType errorEventType);

        /**
         * @brief 构建帮助信息行
         * 
         * @return 格式化的帮助信息字符串
         * 
         * 生成参数的帮助信息显示行，包含参数名、描述、是否必需、默认值等
         */
        std::string buildHelpInfoLine();

#ifdef CPPCLI_DEBUG
        /**
         * @brief 调试信息输出
         * 
         * @return 调试信息字符串
         * 
         * 在调试模式下输出参数的详细信息，用于调试和开发
         */
        std::string debugInfo() const;
#endif
    };

}   // namespace cppcli

// 静态成员变量初始化
cppcli::HelpDocEnum cppcli::Rule::detail::HelpDocStruct::_helpDocType = cppcli::HelpDocEnum::USE_DEFAULT_HELPDOC;  // 默认使用默认帮助文档类型
cppcli::Rule *cppcli::Rule::detail::HelpDocStruct::rule = nullptr;  // 帮助参数规则指针，初始化为空

/**
 * @brief 限制参数值为整数类型
 * 
 * @return 当前Rule对象指针，支持链式调用
 * 
 * 设置参数值类型为整数，用于后续的类型验证
 */
cppcli::Rule *cppcli::Rule::limitInt()
{
    _valueType = cppcli::detail::ValueTypeEnum::INT;  // 设置值类型为整数
    return this;                                       // 返回当前对象，支持链式调用
}

/**
 * @brief 限制参数值为浮点数类型
 * 
 * @return 当前Rule对象指针，支持链式调用
 * 
 * 设置参数值类型为浮点数，用于后续的类型验证
 */
cppcli::Rule *cppcli::Rule::limitDouble()
{
    _valueType = cppcli::detail::ValueTypeEnum::DOUBLE;  // 设置值类型为浮点数
    return this;                                          // 返回当前对象，支持链式调用
}

/**
 * @brief 将当前参数设置为帮助参数
 * 
 * @return 当前Rule对象指针，支持链式调用
 * 
 * 设置当前参数为帮助参数，用户可以通过该参数查看帮助信息
 */
cppcli::Rule *cppcli::Rule::asHelpParam()
{
    if (_necessary == true)                              // 如果参数被标记为必需
    {
        _necessary = false;                              // 取消必需标记（帮助参数不应该是必需的）
    }
    cppcli::Rule::detail::HelpDocStruct::rule = this;   // 将当前规则设置为帮助参数规则
    return this;                                         // 返回当前对象，支持链式调用
}

/**
 * @brief 检查参数是否存在于命令行参数中
 * 
 * @return true 参数存在，false 参数不存在
 * 
 * 检查用户是否在命令行中提供了该参数
 */
bool cppcli::Rule::exists() { return _existsInMap; }     // 返回参数是否存在于映射中

const std::string cppcli::Rule::getError(cppcli::detail::ErrorEventType errorEventType)
{
    std::ostringstream oss;

    oss << "[";
    switch (errorEventType)
    {
    case cppcli::detail::ErrorEventType::MECESSARY_ERROR: {
        if (_longParam.empty())
            oss << _shortParam;
        else
            oss << _shortParam << std::move(" | ") << _longParam;
        break;
    }
    case cppcli::detail::ErrorEventType::VALUETYPE_ERROR: {
        if (_valueType == cppcli::detail::ValueTypeEnum::DOUBLE)
            oss << std::move(" NUMBER (DOUBLE) ");
        else if (_valueType == cppcli::detail::ValueTypeEnum::INT)
            oss << std::move(" NUMBER (INT) ");
        break;
    }
    case cppcli::detail::ErrorEventType::ONEOF_ERROR: {
        for (int i = 0; i < _limitOneVec.size(); i++)
        {
            if (i == (_limitOneVec.size() - 1))
            {
                oss << _limitOneVec.at(i);
                break;
            }
            oss << _limitOneVec.at(i) << std::move(" ");
        }

        break;
    }
    case cppcli::detail::ErrorEventType::NUMRANGE_ERROR: {
        oss << _limitNumRange.first << std::move("(MIN), ") << _limitNumRange.second << std::move("(MAX)");
        break;
    }
    }
    oss << "]";
    return std::move(oss.str());
}

std::string cppcli::Rule::buildHelpInfoLine()
{
    std::ostringstream oss;

    int commandsDis = 28;
    int helpInfoDis = 36;
    int necessaryDis = 20;
    int defaultStrDis = 20;
    int theDis = 2;

    oss << std::setw(commandsDis) << std::left << (_longParam.empty() ? _shortParam : _shortParam + " | " + _longParam);

    int writeTime = _helpInfo.size() % helpInfoDis - theDis == 0 ? int(_helpInfo.size() / (helpInfoDis - theDis))
                                                                 : int((_helpInfo.size() / (helpInfoDis - theDis))) + 1;
    std::string necessaryOutStr = _necessary ? "true" : "false";
    std::string defaultValueOutStr = _default == "[EMPTY]" ? _default : "=" + _default;
 
    if (writeTime == 1)
    {
        oss << std::setw(helpInfoDis) << std::left << _helpInfo;
        oss << std::setw(necessaryDis) << std::left << "MUST-ENTER[" + necessaryOutStr + "]";
        oss << std::setw(defaultStrDis) << std::left << "DEFAULT->" + _default;
        oss << std::endl;
        return std::move(oss.str());
    }
    int pos = 0;
    for (int i = 0; i < writeTime; i++)
    {
        if (i == 0)
        {
            oss << std::setw(helpInfoDis) << std::setw(helpInfoDis) << _helpInfo.substr(pos, helpInfoDis - theDis);
            oss << std::setw(necessaryDis) << std::left << "MUST-ENTER[" + necessaryOutStr + "]";
            oss << std::setw(defaultStrDis) << std::left << "DEFAULT->" + _default;
            oss << std::endl;
            pos += helpInfoDis - theDis;
        }
        else
        {
            oss << std::setw(commandsDis + 4) << std::left << "";
            oss << _helpInfo.substr(pos, helpInfoDis - theDis);
            oss << std::endl;
            pos += helpInfoDis - theDis;
        }
    }

    return std::move(oss.str());
}

#ifdef CPPCLI_DEBUG
std::string cppcli::Rule::debugInfo() const
{

    std::ostringstream oss;

    if (_longParam.empty())
    {
        oss << "command params --> " << _shortParam << std::endl;
    }
    else
    {
        oss << "command params --> " << _shortParam << "|" << _longParam << std::endl;
    }

    oss << "[CPPCLI_DEBUG]     input value = " << _inputValue << std::endl;
    oss << "[CPPCLI_DEBUG]     necessary = " << _necessary << std::endl;
    oss << "[CPPCLI_DEBUG]     valueType = " << _valueType << std::endl;
    oss << "[CPPCLI_DEBUG]     default = " << _default << std::endl;
    oss << "[CPPCLI_DEBUG]     exist = " << _existsInMap << std::endl;

    oss << "[CPPCLI_DEBUG]     limitOneVec = (";
    for (int i = 0; i < _limitOneVec.size(); i++)
    {
        if (i == _limitOneVec.size() - 1)
        {
            oss << _limitOneVec.at(i);
            break;
        }
        oss << _limitOneVec.at(i) << ", ";
    }
    oss << "), size=" << _limitOneVec.size() << std::endl;

    oss << "[CPPCLI_DEBUG]     limitNumRange = (" << _limitNumRange.first << " " << _limitNumRange.second << ")";

    return oss.str();
}
#endif

/* =================================================================================*/
/* =================================================================================*/
/* ===============================    Option    =====================================*/
/* =================================================================================*/
/* =================================================================================*/

namespace cppcli {

    class Option {
      private:
        class detail {
            detail() = delete;
            detail(const detail &) = delete;
            friend class cppcli::Option;
            static int necessaryVerify(Option &opt);
            static int valueTypeVerify(Option &opt);
            static int numRangeVerify(Option &opt);
            static int oneOfVerify(Option &opt);
        };

      public:
        Option(int argc, char *argv[]);
        Option(const cppcli::Option &) = delete;
        Option operator=(const cppcli::Option &) = delete;
        cppcli::Rule *operator()(const std::string &shortParam, const std::string &longParam,
                                 const std::string helpInfo);
        cppcli::Rule *operator()(const std::string &shortParam, const std::string &longParam,
                                 const std::string helpInfo, bool necessary);
        ~Option();
        void parse();
        bool exists(const std::string shortParam);
        bool exists(const cppcli::Rule *rule);





#ifdef CPPCLI_DEBUG
        void printCommandMap();
#endif

        const std::string getWorkPath();
        const std::string getExecPath();

      private:
        cppcli::ErrorExitEnum _exitType = cppcli::ErrorExitEnum::EXIT_PRINT_RULE;
        std::map<std::string, std::string> _commandMap;
        std::vector<cppcli::Rule *> _ruleVec;

        std::string _workPath;   // exe path
        std::string _execPath;   // exec command path

      private:
        void rulesGainInputValue();
        std::string getInputValue(const cppcli::Rule &rule);
        std::string buildHelpDoc();
        void printHelpDoc();
        bool mapExists(const cppcli::Rule *rule);
        void pathInit(int argc, char *argv[]);

        void errorExitFunc(const std::string errorInfo, int index, cppcli::ErrorExitEnum exitType,
                           cppcli::detail::ErrorEventType eventType);

        template <class T, class = typename std::enable_if<std::is_same<T, std::string>::value>::type>
        std::string get(const std::string shortParam)
        {
            for (cppcli::Rule *rule : _ruleVec)
            {
                if (rule->_shortParam == shortParam)
                {
                    return rule->get<std::string>();
                }
            }
            std::cout << "error: don't set where short-param = " << shortParam << std::endl;
            std::exit(-1);
        }

        template <class T, class = typename std::enable_if<std::is_same<T, int>::value>::type>
        int get(const std::string shortParam)
        {
            for (cppcli::Rule *rule : _ruleVec)
            {
                if (rule->_shortParam == shortParam)
                {
                    return rule->get<int>();
                }
            }
            std::cout << "error: don't set where short-param = " << shortParam << std::endl;
            std::exit(-1);
        }

        template <class T, class = typename std::enable_if<std::is_same<T, double>::value>::type>
        double get(const std::string shortParam)
        {
            for (cppcli::Rule *rule : _ruleVec)
            {
                if (rule->_shortParam == shortParam)
                {
                    return rule->get<double>();
                }
            }
            std::cout << "error: don't set where short-param = " << shortParam << std::endl;
            std::exit(-1);
        }

        /**
         * @brief 通过Rule指针获取字符串类型的参数值
         * 
         * @tparam T 模板参数类型，必须为std::string
         * @param rule 参数规则对象指针
         * @return 参数值字符串
         * 
         * 模板特化：当T为std::string时，通过Rule对象获取字符串类型的参数值
         */
        template <class T, class = typename std::enable_if<std::is_same<T, std::string>::value>::type>
        std::string get(cppcli::Rule *rule)
        {
            return rule->get<std::string>();  // 调用Rule对象的get方法获取字符串值
        }

        /**
         * @brief 通过Rule指针获取整数类型的参数值
         * 
         * @tparam T 模板参数类型，必须为int
         * @param rule 参数规则对象指针
         * @return 参数值整数
         * 
         * 模板特化：当T为int时，通过Rule对象获取整数类型的参数值
         */
        template <class T, class = typename std::enable_if<std::is_same<T, int>::value>::type>
        int get(cppcli::Rule *rule)
        {
            return rule->get<int>();  // 调用Rule对象的get方法获取整数值
        }

        /**
         * @brief 通过Rule指针获取浮点数类型的参数值
         * 
         * @tparam T 模板参数类型，必须为double
         * @param rule 参数规则对象指针
         * @return 参数值浮点数
         * 
         * 模板特化：当T为double时，通过Rule对象获取浮点数类型的参数值
         */
        template <class T, class = typename std::enable_if<std::is_same<T, double>::value>::type>
        double get(cppcli::Rule *rule)
        {
            return rule->get<double>();  // 调用Rule对象的get方法获取浮点数值
        }

    };

}   // namespace cppcli

/**
 * @brief 验证必需参数是否都已提供
 * 
 * @param opt Option对象引用
 * @return 验证失败的规则索引，-1表示验证通过
 * 
 * 检查所有标记为必需的参数是否都在命令行中提供，如果缺少必需参数则返回失败规则的索引
 */
int cppcli::Option::Option::detail::necessaryVerify(Option &opt)
{
    cppcli::Rule *rule = nullptr;  // 声明规则指针
    for (int index = 0; index < opt._ruleVec.size(); index ++)  // 遍历所有规则
    {
        rule = opt._ruleVec.at(index);  // 获取当前规则
        if (rule->_necessary && !opt.mapExists(rule))  // 如果是必需参数且不存在于命令行中
        {
#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("failed in necessaryVerify, fail rule in following");  // 调试信息：验证失败
            CPPCLI_DEBUG_PRINT(rule->debugInfo(), "\n");  // 打印失败规则的调试信息
#endif
            return index;  // 返回失败规则的索引
        }
    }
    return -1;  // 所有必需参数都已提供，验证通过
};

/**
 * @brief 验证参数值类型是否正确
 * 
 * @param opt Option对象引用
 * @return 验证失败的规则索引，-1表示验证通过
 * 
 * 检查所有非字符串类型的参数值是否符合其声明的类型（整数或浮点数）
 */
int cppcli::Option::Option::detail::valueTypeVerify(Option &opt)
{
    cppcli::Rule *rule = nullptr;  // 声明规则指针
    for (int index = 0; index < opt._ruleVec.size(); index ++)  // 遍历所有规则
    {
        rule = opt._ruleVec.at(index);  // 获取当前规则
        // 跳过字符串类型或不存在于命令行中的参数
        if (rule->_valueType == cppcli::detail::ValueTypeEnum::STRING || !opt.mapExists(rule))
        {
            continue;  // 继续检查下一个规则
        }

        // 验证整数类型参数
        if (rule->_valueType == cppcli::detail::ValueTypeEnum::INT &&
            !cppcli::detail::algoUtil::isInt(rule->_inputValue))
        {
#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("failed in valueTypeVerify, fail rule in following");  // 调试信息：类型验证失败
            CPPCLI_DEBUG_PRINT(rule->debugInfo(), "\n");  // 打印失败规则的调试信息
#endif
           
            return index;  // 返回失败规则的索引
        }

        // 验证浮点数类型参数
        if (rule->_valueType == cppcli::detail::ValueTypeEnum::DOUBLE &&
            !cppcli::detail::algoUtil::verifyDouble(rule->_inputValue))
        {
#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("failed in valueTypeVerify, fail rule in following");  // 调试信息：类型验证失败
            CPPCLI_DEBUG_PRINT(rule->debugInfo(), "\n");  // 打印失败规则的调试信息
#endif
            return index;  // 返回失败规则的索引
        }

    }
    return -1;  // 所有参数类型验证通过
}

/**
 * @brief 验证数值参数是否在指定范围内
 * 
 * @param opt Option对象引用
 * @return 验证失败的规则索引，-1表示验证通过
 * 
 * 检查所有设置了数值范围限制的参数值是否在允许的范围内
 */
int cppcli::Option::Option::detail::numRangeVerify(Option &opt)
{
    cppcli::Rule *rule = nullptr;  // 声明规则指针
    for (int index = 0; index < opt._ruleVec.size(); index ++)  // 遍历所有规则
    {
        rule = opt._ruleVec.at(index);  // 获取当前规则
        // 跳过字符串类型或不存在于命令行中的参数
        if (rule->_valueType == cppcli::detail::ValueTypeEnum::STRING || !opt.mapExists(rule))
        {
            continue;  // 继续检查下一个规则
        }

        // 跳过未设置数值范围限制的参数（默认值为-1表示未设置）
        if (rule->_limitNumRange.first == -1 && rule->_limitNumRange.second == -1)
        {
            continue;  // 继续检查下一个规则
        }

        // 检查参数值是否为空或不是有效的数值
        if(rule->_inputValue.empty() || !cppcli::detail::algoUtil::verifyDouble(rule->_inputValue))
        {
#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("failed in numRangeVerify, fail rule in following");  // 调试信息：范围验证失败
            CPPCLI_DEBUG_PRINT(rule->debugInfo(), "\n");  // 打印失败规则的调试信息
#endif
            return index;  // 返回失败规则的索引
        }

        // 检查数值是否在指定范围内
        if (std::stod(rule->_inputValue) < rule->_limitNumRange.first ||
            std::stod(rule->_inputValue) > rule->_limitNumRange.second)
        {
#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("failed in numRangeVerify, fail rule in following");  // 调试信息：范围验证失败
            CPPCLI_DEBUG_PRINT(rule->debugInfo(), "\n");  // 打印失败规则的调试信息
#endif
            return index;  // 返回失败规则的索引
        }
    }
    return -1;  // 所有数值范围验证通过
}

/**
 * @brief 验证参数值是否为允许的枚举值之一
 * 
 * @param opt Option对象引用
 * @return 验证失败的规则索引，-1表示验证通过
 * 
 * 检查所有设置了枚举值限制的参数值是否在允许的值列表中
 */
int cppcli::Option::Option::detail::oneOfVerify(Option &opt)
{

    cppcli::Rule *rule = nullptr;  // 声明规则指针
    for (int index = 0; index < opt._ruleVec.size(); index ++)  // 遍历所有规则
    {
        rule = opt._ruleVec.at(index);  // 获取当前规则
        // 跳过未设置枚举值限制或不存在于命令行中的参数
        if (rule->_limitOneVec.size() == 0 || !opt.mapExists(rule))
        {
            continue;  // 继续检查下一个规则
        }

        // 检查参数值是否在允许的枚举值列表中
        if (std::find(rule->_limitOneVec.begin(), rule->_limitOneVec.end(), rule->_inputValue) ==
            rule->_limitOneVec.end())
        {

#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("failed in oneOfVerify, fail rule in following");  // 调试信息：枚举值验证失败
            CPPCLI_DEBUG_PRINT(rule->debugInfo(), "\n");  // 打印失败规则的调试信息
#endif

            return index;  // 返回失败规则的索引
        }
    }
    return -1;  // 所有枚举值验证通过
}

#ifdef CPPCLI_DEBUG
/**
 * @brief 打印命令行参数映射表（调试模式）
 * 
 * 在调试模式下输出所有解析后的命令行参数键值对，用于调试和验证参数解析结果
 */
void cppcli::Option::Option::printCommandMap()
{
    CPPCLI_DEBUG_PRINT("-- commandMap, size = ", _commandMap.size());  // 打印参数映射表的大小
    for (const std::pair<std::string, std::string> &pr : _commandMap)  // 遍历所有参数键值对
    {
        CPPCLI_DEBUG_PRINT("    ", pr.first, "=", pr.second);  // 打印每个参数的键值对
    }
    CPPCLI_DEBUG_PRINT("-- end commandMap");  // 打印结束标记
}
#endif

/**
 * @brief 错误退出处理函数
 * 
 * @param errorInfo 错误信息前缀
 * @param index 失败规则的索引
 * @param exitType 退出类型（是否打印帮助文档）
 * @param eventType 错误事件类型
 * 
 * 当参数验证失败时，输出详细的错误信息并退出程序
 */
void cppcli::Option::errorExitFunc(const std::string errorInfo, int index, cppcli::ErrorExitEnum exitType,
                                   cppcli::detail::ErrorEventType eventType)
{

    cppcli::Rule rule = *_ruleVec.at(index);  // 获取失败的规则对象

    // std::unique_lock<std::mutex> lock(cppcli::_coutMutex);  // 注释掉的互斥锁
    std::ostringstream oss;  // 创建字符串流用于构建错误信息
    // 如果不是必需参数缺失错误，添加参数标识信息
    if (eventType != cppcli::detail::ErrorEventType::MECESSARY_ERROR)
        oss << ", where command param = [" << rule._shortParam << "]";
    // 如果设置了帮助参数，添加帮助提示信息
    if (cppcli::Rule::detail::HelpDocStruct::rule != nullptr)
    {
        oss << std::endl << "Use [" << cppcli::Rule::detail::HelpDocStruct::rule->_shortParam << "] gain help doc";
    }

    std::cout << errorInfo << rule.getError(eventType) << oss.str() << std::endl;  // 输出完整错误信息
    // 如果退出类型要求打印帮助文档，则输出帮助信息
    if (exitType == cppcli::EXIT_PRINT_RULE_HELPDOC)
        std::cout << buildHelpDoc();

    std::exit(0);  // 退出程序
}

/**
 * @brief Option类构造函数
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数字符串数组
 * 
 * 初始化Option对象，设置工作路径和执行路径，解析命令行参数到映射表中
 */
cppcli::Option::Option(int argc, char *argv[])
{
    // 初始化工作路径和执行路径
    pathInit(argc, argv);

#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("---------------- argc argv start");  // 调试信息：开始处理命令行参数
    // 定义lambda函数，将所有命令行参数连接成字符串用于调试输出
    auto beStr = [&]() -> const std::string {
        std::ostringstream oss;
        for (int i = 0; i < argc; i++)  // 遍历所有参数
            oss << argv[i] << "  ";      // 将参数用空格分隔连接
        return oss.str();
    };
    CPPCLI_DEBUG_PRINT("argc = ", argc, " || argv = ", beStr());  // 打印参数数量和完整参数字符串
#endif

    _ruleVec.reserve(64);  // 预分配规则向量容量，避免频繁重新分配

    // 将命令行参数解析并保存到映射表中
    cppcli::detail::algoUtil::InitCommandMap(argc, argv, _commandMap);
#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("---------------- argv map start");  // 调试信息：开始处理参数映射
    printCommandMap();  // 打印参数映射表
#endif
}

/**
 * @brief Option类析构函数
 * 
 * 释放所有动态分配的Rule对象，清空规则向量，防止内存泄漏
 */
cppcli::Option::~Option()
{
    for (cppcli::Rule *rule : _ruleVec)  // 遍历所有规则对象
    {
        if (rule != nullptr)  // 检查指针是否有效
        {
            delete (rule);  // 释放动态分配的内存
        }
    }
    _ruleVec.clear();  // 清空规则向量
}


/**
 * @brief 函数调用操作符重载（非必需参数）
 * 
 * @param shortParam 短参数名
 * @param longParam 长参数名
 * @param helpInfo 帮助信息
 * @return 新创建的Rule对象指针
 * 
 * 创建新的参数规则并添加到规则向量中，支持链式调用
 */
cppcli::Rule *cppcli::Option::operator()(const std::string &shortParam, const std::string &longParam,
                                         const std::string helpInfo)
{
    // 验证短参数名必须包含"-"字符
    if (shortParam.find("-") == shortParam.npos)
    {
        std::cout << "short-param must contains \"-\" " << std::endl;  // 输出错误信息
        std::exit(-1);  // 退出程序
    }
    // 验证长参数名如果非空则必须包含"-"字符
    if (!longParam.empty() && longParam.find("-") == longParam.npos)
    {
        std::cout << "long-param must empty or contains \"-\" " << std::endl;  // 输出错误信息
        std::exit(-1);  // 退出程序
    }


    _ruleVec.push_back(new cppcli::Rule(shortParam, longParam, helpInfo));  // 创建新规则并添加到向量
    return _ruleVec.back();  // 返回新创建的规则对象指针
}

/**
 * @brief 函数调用操作符重载（可指定是否必需）
 * 
 * @param shortParam 短参数名
 * @param longParam 长参数名
 * @param helpInfo 帮助信息
 * @param necessary 是否为必需参数
 * @return 新创建的Rule对象指针
 * 
 * 创建新的参数规则并添加到规则向量中，可以指定参数是否必需
 */
cppcli::Rule *cppcli::Option::operator()(const std::string &shortParam, const std::string &longParam,
                                         const std::string helpInfo, bool necessary)
{
    // 验证短参数名必须包含"-"字符
    if (shortParam.find("-") == shortParam.npos)
    {
        std::cout << "short-param must contains \"-\" " << std::endl;  // 输出错误信息
        std::exit(-1);  // 退出程序
    }
    // 验证长参数名如果非空则必须包含"-"字符
    if (!longParam.empty() && longParam.find("-") == longParam.npos)
    {
        std::cout << "long-param must empty or contains \"-\" " << std::endl;  // 输出错误信息
        std::exit(-1);  // 退出程序
    }

    _ruleVec.push_back(new cppcli::Rule(shortParam, longParam, helpInfo, necessary));  // 创建新规则并添加到向量
    return _ruleVec.back();  // 返回新创建的规则对象指针
}

/**
 * @brief 初始化工作路径和执行路径
 * 
 * @param argc 命令行参数数量（未使用）
 * @param argv 命令行参数字符串数组（未使用）
 * 
 * 根据操作系统获取当前工作目录和可执行文件路径，支持Windows和Linux平台
 */
void cppcli::Option::pathInit(int argc, char *argv[])
{

    char execBuf[1024];  // 执行路径缓冲区
    char workBuf[1024];  // 工作路径缓冲区
#if defined(WIN32) || defined(_WIN64) || defined(__WIN32__)
    _getcwd(execBuf, sizeof(execBuf));  // Windows平台：获取当前工作目录
    GetModuleFileName(NULL, workBuf, sizeof(workBuf));  // Windows平台：获取可执行文件路径
#else
    auto none1 = getcwd(execBuf, sizeof(execBuf));  // Linux平台：获取当前工作目录
    auto none2 = readlink("/proc/self/exe", workBuf, sizeof(workBuf));  // Linux平台：获取可执行文件路径
#endif
    _execPath = execBuf;  // 设置执行路径为当前工作目录
    _workPath = std::move(cppcli::detail::pathUtil::getFileDir(workBuf));  // 设置工作路径为可执行文件所在目录

#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("execPath = ", _execPath, ", workPath = ", _workPath);  // 调试信息：打印路径信息
#endif
}

/**
 * @brief 获取规则的输入值
 * 
 * @param rule 要查询的规则对象引用
 * @return 规则的输入值字符串，如果未找到则返回空字符串
 * 
 * 从命令行参数映射表中查找规则的输入值，优先查找短参数，其次查找长参数
 */
std::string cppcli::Option::getInputValue(const cppcli::Rule &rule)
{

    std::string inputValue;  // 存储输入值的字符串
    // 首先查找短参数
    if (_commandMap.find(rule._shortParam) != _commandMap.end())
    {
        inputValue = _commandMap[rule._shortParam];  // 找到短参数，获取其值
    }
    // 然后查找长参数（如果短参数未找到或长参数存在）
    if (_commandMap.find(rule._longParam) != _commandMap.end())
    {
        inputValue = _commandMap[rule._longParam];  // 找到长参数，获取其值
    }

    return inputValue;  // 返回找到的输入值
}

/**
 * @brief 为所有规则获取输入值
 * 
 * 遍历所有规则，为存在于命令行中的规则设置输入值，为未提供值的规则设置默认值
 */
void cppcli::Option::rulesGainInputValue()
{
    std::string inputValue;  // 临时存储输入值

    for (cppcli::Rule *rule : _ruleVec)  // 遍历所有规则
    {
        if (!mapExists(rule))  // 如果规则不存在于命令行参数中
            continue;  // 跳过该规则
            
        rule->_existsInMap = true;  // 标记规则存在于映射中
        inputValue = getInputValue(*rule);  // 获取规则的输入值
        
        if (!inputValue.empty())  // 如果输入值非空
        {
            rule->_inputValue = inputValue;  // 设置规则的输入值
            
            continue;  // 继续处理下一个规则
        }
        // 如果输入值为空且规则有默认值
        if(inputValue.empty() && rule->_default != "[EMPTY]")
        {
            rule->_inputValue = rule->_default;  // 使用默认值
        }
    }
}

/**
 * @brief 检查规则是否存在于命令行参数映射中
 * 
 * @param rule 要检查的规则对象指针
 * @return true 规则存在于映射中，false 规则不存在
 * 
 * 检查规则的短参数或长参数是否在命令行参数映射表中存在
 */
bool cppcli::Option::mapExists(const cppcli::Rule *rule)
{
    if (rule != nullptr)  // 检查规则指针是否有效
    {
        // 检查短参数或长参数是否存在于映射中
        return _commandMap.find(rule->_shortParam) != _commandMap.end() ||
               _commandMap.find(rule->_longParam) != _commandMap.end();
    }
    return false;  // 规则指针无效，返回false
}

/**
 * @brief 检查规则是否存在于命令行参数中
 * 
 * @param rule 要检查的规则对象指针
 * @return true 规则存在，false 规则不存在
 * 
 * 检查指定的规则是否在命令行参数中存在，在调试模式下输出详细信息
 */
bool cppcli::Option::exists(const cppcli::Rule *rule)
{
#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("---------------- exist rule");  // 调试信息：开始检查规则存在性
    CPPCLI_DEBUG_PRINT(rule->debugInfo());  // 打印规则的调试信息
#endif
    return mapExists(rule);  // 调用mapExists检查规则是否存在
}

/**
 * @brief 通过短参数名检查规则是否存在
 * 
 * @param shortParam 短参数名
 * @return true 规则存在，false 规则不存在
 * 
 * 根据短参数名查找对应的规则，并检查该规则是否在命令行参数中存在
 */
bool cppcli::Option::exists(const std::string shortParam)
{

    for (cppcli::Rule *rule : _ruleVec)  // 遍历所有规则
    {
        if (rule->_shortParam == shortParam)  // 找到匹配的短参数名
        {
#ifdef CPPCLI_DEBUG
            CPPCLI_DEBUG_PRINT("---------------- exist rule");  // 调试信息：开始检查规则存在性
            CPPCLI_DEBUG_PRINT(rule->debugInfo());  // 打印规则的调试信息
#endif
            return mapExists(rule);  // 检查该规则是否存在于命令行参数中
        }
    }
    return false;  // 未找到匹配的规则，返回false
}

/**
 * @brief 构建帮助文档
 * 
 * @return 格式化的帮助文档字符串
 * 
 * 遍历所有规则，为每个规则生成帮助信息行，组合成完整的帮助文档
 */
std::string cppcli::Option::buildHelpDoc()
{
    std::ostringstream oss;  // 创建字符串流用于构建帮助文档
    oss << "options:" << std::endl;  // 添加帮助文档标题
    for (cppcli::Rule *rule : _ruleVec)  // 遍历所有规则
    {
        oss << rule->buildHelpInfoLine();  // 为每个规则添加帮助信息行
    }
    return oss.str();  // 返回完整的帮助文档字符串
}

/**
 * @brief 打印帮助文档
 * 
 * 检查是否设置了帮助参数，如果用户提供了帮助参数则输出帮助文档并退出程序
 */
void cppcli::Option::printHelpDoc()
{
#ifdef CPPCLI_DEBUG
    if (nullptr == cppcli::Rule::detail::HelpDocStruct::rule)  // 检查是否设置了帮助参数规则
    {
        CPPCLI_DEBUG_PRINT("warning: you don't set help param\n");  // 调试警告：未设置帮助参数
    }
#endif

    if (!mapExists(cppcli::Rule::detail::HelpDocStruct::rule))  // 检查帮助参数是否存在于命令行中
    {
        return;  // 如果不存在，直接返回
    }

    std::cout << buildHelpDoc();  // 输出帮助文档
    std::exit(0);  // 退出程序
}

/**
 * @brief 获取工作路径
 * 
 * @return 工作路径字符串
 * 
 * 返回可执行文件所在的目录路径
 */
const std::string cppcli::Option::getWorkPath() { return _workPath; }

/**
 * @brief 获取执行路径
 * 
 * @return 执行路径字符串
 * 
 * 返回程序执行时的当前工作目录路径
 */
const std::string cppcli::Option::getExecPath() { return _execPath; }

/**
 * @brief 解析和验证所有命令行参数
 * 
 * 执行完整的参数解析流程：获取输入值、检查帮助参数、验证必需参数、类型验证、范围验证和枚举值验证
 */
void cppcli::Option::parse()
{
    // 为所有规则获取对应的输入值
    rulesGainInputValue();

#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("---------------- rules vector start");  // 调试信息：开始处理规则向量
    for (int i = 0; i < _ruleVec.size(); i++)  // 遍历所有规则
    {
        CPPCLI_DEBUG_PRINT("vec index = ", i, "  ", _ruleVec[i]->debugInfo());  // 打印每个规则的调试信息
    }

#endif

    // 检查是否存在帮助参数，如果存在则打印帮助文档
    printHelpDoc();

    // 执行各种验证检查
    int necessaryResult = Option::detail::necessaryVerify(*this);  // 验证必需参数
    int valueTypeResult = Option::detail::valueTypeVerify(*this);  // 验证参数值类型
    int oneOfResult = Option::detail::oneOfVerify(*this);  // 验证枚举值
    int numRangeResult = Option::detail::numRangeVerify(*this);  // 验证数值范围

#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("---------------- verify result");  // 调试信息：验证结果
    CPPCLI_DEBUG_PRINT("necessaryResult=", necessaryResult, ", valueTypeResult=", valueTypeResult,
                       ",oneOfResult=", oneOfResult, ", numRangeResult", numRangeResult);  // 打印各项验证结果

#endif

    // 检查必需参数验证结果
    if (necessaryResult > -1)
    {
        errorExitFunc("Must enter this param: ", necessaryResult, _exitType,
                      cppcli::detail::ErrorEventType::MECESSARY_ERROR);  // 必需参数缺失，退出程序
    }

    // 检查参数值类型验证结果
    if (valueTypeResult > -1)
    {
        errorExitFunc("Please enter the correct type: ", valueTypeResult, _exitType,
                      cppcli::detail::ErrorEventType::VALUETYPE_ERROR);  // 参数类型错误，退出程序
    }

    // 检查枚举值验证结果
    if (oneOfResult > -1)
    {
        errorExitFunc("Must be one of these values: ", oneOfResult, _exitType,
                      cppcli::detail::ErrorEventType::ONEOF_ERROR);  // 枚举值错误，退出程序
    }

    // 检查数值范围验证结果
    if (numRangeResult > -1)
    {
        errorExitFunc("Must be within this range: ", numRangeResult, _exitType,
                      cppcli::detail::ErrorEventType::NUMRANGE_ERROR);  // 数值范围错误，退出程序
    }

#ifdef CPPCLI_DEBUG
    CPPCLI_DEBUG_PRINT("---------------- parse result");  // 调试信息：解析结果
    CPPCLI_DEBUG_PRINT(">>>>>>>>>   PASS   <<<<<<<<<<");  // 所有验证通过
#endif
}
