/**
 * @file atp_quant_msg.h
 * @brief ATP量化交易API消息定义文件
 * 
 * 本文件定义了ATP量化交易系统使用的所有消息类和数据结构，包括：
 * 1. 基础工具类 - Buffer（二进制数据流）、ATPProperties（配置属性）
 * 2. 错误信息类 - ATPRspErrorInfo（统一错误信息格式）
 * 3. 认证相关类 - ClientFeatureCode（终端识别码）、ATPLoginProperty（登录属性）
 * 4. 用户信息类 - ATPCustomerInfo（客户信息）、ATPConnectionInfo（连接信息）
 * 5. 交易消息类 - 下单、撤单、查询等各种业务消息的请求和响应
 * 
 * 设计逻辑：
 * - 采用面向对象设计，每种消息都是独立的类
 * - 使用纯虚函数定义接口，隐藏具体实现细节
 * - 提供统一的消息创建和销毁接口（NewMessage/DeleteMessage）
 * - 支持消息的序列化和反序列化（Encode/Decode）
 * - 禁用拷贝构造和赋值操作，确保消息对象的唯一性
 * - 使用RAII模式管理消息对象的生命周期
 * 
 * 版权信息：
 * Copyright (c) 2018 Archforce Financial Technology. All rights reserved.
 * 未经许可，不得以任何形式复制、修改或分发本软件。
 * 更多信息请访问：archforce.cn
 */

// 代码生成器标记，用于版本控制和代码同步
// requests:5f024b1066515f963731eec331bd9d61 responses:b0e958b18fde3671566c702f3c2a40de constants:3c699dd546cc3a70c1497ce1c443e2cc types:77cca557b8ed3391410ffcd7964e7093 trade_messages:3d837391385002e580b6d269eb6d2c26 template:cbd7ccf06b1731da577df564fc12dad1 code:6aebb897521436000d87bdda869bcd55

#ifndef ATP_QUANT_MSG_H_                  // 防止头文件重复包含的预处理保护
#define ATP_QUANT_MSG_H_                  // 定义头文件保护宏
#include <atp_quant_constant.h>           // 包含ATP量化交易常量定义

// ================================================================================================
// 前置声明区域
// ================================================================================================
namespace atp                             // ATP命名空间
{
    namespace api                         // API基础组件命名空间
    {
        class PropertiesImpl;             // 前置声明：属性实现类，用于内部实现细节的封装
    }
}

namespace atp                             // ATP命名空间
{
    namespace quant_api                   // 量化API命名空间
    {
        /**
         * @class Buffer
         * @brief 二进制数据流封装类
         * 
         * 用于封装二进制数据流，主要用于消息的序列化和反序列化。
         * 提供了数据长度和数据指针的统一管理，确保数据传输的安全性。
         * 
         * 使用场景：
         * - 消息编码后的二进制数据存储
         * - 网络传输的数据包封装
         * - 消息解码时的数据源
         */
        class QUANT_API Buffer
        {
        public:
            /**
             * @brief 默认构造函数
             * 
             * 创建空的数据流对象，数据长度为0，数据指针为空。
             */
            Buffer() : data_size(0), data(nullptr) {}

            /**
             * @brief 参数化构造函数
             * @param data_size 二进制数据流的长度（字节数）
             * @param data 指向二进制数据流首地址的指针
             * 
             * 创建包含指定数据的数据流对象，通常用于消息解码。
             */
            Buffer(const uint32_t data_size, const char* data) : data_size(data_size), data(data) {}

        public:
            uint32_t data_size;               // 二进制数据流的长度（字节数）
            const char* data;                 // 指向二进制数据流首地址的常量指针
        };

        /**
         * @class ATPProperties
         * @brief 配置属性管理类
         * 
         * 用于管理ATP量化API的各种配置参数，支持多种数据类型的键值对存储。
         * 提供类型安全的参数设置和获取接口，支持默认值机制。
         * 
         * 主要功能：
         * - 支持字符串、布尔值、各种整数类型的参数存储
         * - 提供类型安全的参数获取接口
         * - 支持参数存在性检查
         * - 使用PIMPL模式隐藏实现细节
         */
        class QUANT_API ATPProperties
        {
        public:
            /**
             * @brief 构造函数
             * 
             * 创建空的配置属性对象，初始化内部实现。
             */
            ATPProperties();

            /**
             * @brief 析构函数
             * 
             * 清理配置属性对象，释放内部实现资源。
             */
            ~ATPProperties();

            /**
             * @brief 禁用拷贝构造函数
             * 
             * 配置属性对象包含内部状态，不允许拷贝构造。
             */
            ATPProperties(const ATPProperties&) = delete;

            /**
             * @brief 禁用赋值操作符
             * 
             * 配置属性对象包含内部状态，不允许赋值操作。
             */
            ATPProperties& operator=(const ATPProperties&) = delete;

            // ================================================================================================
            // 参数设置接口（支持多种数据类型）
            // ================================================================================================
            
            /**
             * @brief 设置字符串类型参数
             * @param key 参数键名
             * @param value 参数值（字符串）
             */
            void SetValue(const char* key, const char* value);

            /**
             * @brief 设置布尔类型参数
             * @param key 参数键名
             * @param value 参数值（布尔值）
             */
            void SetValue(const char* key, bool value);

            /**
             * @brief 设置字符类型参数
             * @param key 参数键名
             * @param value 参数值（字符）
             */
            void SetValue(const char* key, char value);

            /**
             * @brief 设置8位无符号整数参数
             * @param key 参数键名
             * @param value 参数值（8位无符号整数）
             */
            void SetValue(const char* key, uint8_t value);

            /**
             * @brief 设置8位有符号整数参数
             * @param key 参数键名
             * @param value 参数值（8位有符号整数）
             */
            void SetValue(const char* key, int8_t value);

            /**
             * @brief 设置16位无符号整数参数
             * @param key 参数键名
             * @param value 参数值（16位无符号整数）
             */
            void SetValue(const char* key, uint16_t value);

            /**
             * @brief 设置16位有符号整数参数
             * @param key 参数键名
             * @param value 参数值（16位有符号整数）
             */
            void SetValue(const char* key, int16_t value);

            /**
             * @brief 设置32位无符号整数参数
             * @param key 参数键名
             * @param value 参数值（32位无符号整数）
             */
            void SetValue(const char* key, uint32_t value);

            /**
             * @brief 设置32位有符号整数参数
             * @param key 参数键名
             * @param value 参数值（32位有符号整数）
             */
            void SetValue(const char* key, int32_t value);

            /**
             * @brief 设置64位无符号整数参数
             * @param key 参数键名
             * @param value 参数值（64位无符号整数）
             */
            void SetValue(const char* key, uint64_t value);

            /**
             * @brief 设置64位有符号整数参数
             * @param key 参数键名
             * @param value 参数值（64位有符号整数）
             */
            void SetValue(const char* key, int64_t value);

            // ================================================================================================
            // 参数获取接口（支持默认值）
            // ================================================================================================

            /**
             * @brief 获取字符串类型参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return const char* 参数值或默认值
             */
            const char* GetValue(const char* key, const char* default_value) const;

            /**
             * @brief 获取布尔类型参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return bool 参数值或默认值
             */
            bool GetValue(const char* key, bool default_value) const;

            /**
             * @brief 获取字符类型参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return char 参数值或默认值
             */
            char GetValue(const char* key, char default_value) const;

            /**
             * @brief 获取8位无符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return uint8_t 参数值或默认值
             */
            uint8_t GetValue(const char* key, uint8_t default_value) const;

            /**
             * @brief 获取8位有符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return int8_t 参数值或默认值
             */
            int8_t GetValue(const char* key, int8_t default_value) const;

            /**
             * @brief 获取16位无符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return uint16_t 参数值或默认值
             */
            uint16_t GetValue(const char* key, uint16_t default_value) const;

            /**
             * @brief 获取16位有符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return int16_t 参数值或默认值
             */
            int16_t GetValue(const char* key, int16_t default_value) const;

            /**
             * @brief 获取32位无符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return uint32_t 参数值或默认值
             */
            uint32_t GetValue(const char* key, uint32_t default_value) const;

            /**
             * @brief 获取32位有符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return int32_t 参数值或默认值
             */
            int32_t GetValue(const char* key, int32_t default_value) const;

            /**
             * @brief 获取64位无符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return uint64_t 参数值或默认值
             */
            uint64_t GetValue(const char* key, uint64_t default_value) const;

            /**
             * @brief 获取64位有符号整数参数
             * @param key 参数键名
             * @param default_value 默认值，当参数不存在时返回
             * @return int64_t 参数值或默认值
             */
            int64_t GetValue(const char* key, int64_t default_value) const;

            /**
             * @brief 检查参数是否存在
             * @param key 参数键名
             * @return bool true表示参数存在，false表示参数不存在
             */
            bool HasKey(const char* key) const;

        private:
            atp::api::PropertiesImpl* impl_;  // PIMPL模式：指向内部实现的指针，隐藏实现细节
        };

        /**
         * @class ATPRspErrorInfo
         * @brief 错误信息响应类
         * 
         * 统一的错误信息格式，用于所有API调用的错误返回。
         * 包含错误码和错误描述信息，提供标准化的错误处理机制。
         * 
         * 错误判断逻辑：
         * - error_id = 0：操作成功
         * - error_id != 0：操作失败，通过GetErrorMsg()获取详细错误描述
         */
        class QUANT_API ATPRspErrorInfo
        {
        public:
            /**
             * @brief 构造函数
             * 
             * 创建错误信息对象，默认错误码为0（成功）。
             */
            ATPRspErrorInfo() : error_id(0) {}

            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构。
             */
            virtual ~ATPRspErrorInfo() = default;

            /**
             * @brief 获取错误描述信息
             * @return const char* 错误描述字符串，提供详细的错误说明
             * 
             * 当error_id不为0时，此方法返回具体的错误描述信息，
             * 帮助开发者理解错误原因和解决方案。
             */
            virtual const char* GetErrorMsg() const = 0;

        public:
            int32_t error_id;                 // 错误码：0表示成功，非0表示具体的错误类型
        };

        /**
         * @class ClientFeatureCode
         * @brief 客户端终端识别码类
         * 
         * 用于收集和管理客户端的硬件和软件特征信息，生成唯一的终端识别码。
         * 这些信息用于客户端身份验证和安全控制，防止非法终端接入。
         * 
         * 主要功能：
         * - 收集客户端硬件信息（CPU、硬盘、MAC地址等）
         * - 收集网络信息（公网IP、内网IP、端口等）
         * - 收集软件信息（终端类型、软件版本等）
         * - 生成综合的终端识别码用于身份验证
         */
        class QUANT_API ClientFeatureCode
        {
        public:
            /**
             * @brief 默认构造函数
             * 
             * 创建空的终端识别码对象。
             */
            ClientFeatureCode() = default;

            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构。
             */
            virtual ~ClientFeatureCode() = default;

            /**
             * @brief 禁用拷贝构造函数
             * 
             * 终端识别码对象包含唯一的硬件信息，不允许拷贝。
             */
            ClientFeatureCode(const ClientFeatureCode&) = delete;

            /**
             * @brief 禁用赋值操作符
             * 
             * 终端识别码对象包含唯一的硬件信息，不允许赋值操作。
             */
            ClientFeatureCode& operator=(const ClientFeatureCode&) = delete;

            /**
             * @brief 创建终端识别码对象（静态工厂方法）
             * @return ClientFeatureCode* 新创建的终端识别码对象指针
             * 
             * 使用工厂模式创建对象，确保对象的正确初始化。
             */
            static ClientFeatureCode* NewMessage();

            /**
             * @brief 销毁终端识别码对象（静态销毁方法）
             * @param ptr 要销毁的终端识别码对象指针
             * 
             * 与NewMessage()配对使用，确保对象的正确销毁和内存释放。
             */
            static void DeleteMessage(ClientFeatureCode* ptr);

        public:
            // ================================================================================================
            // 终端基本信息设置和获取接口
            // ================================================================================================
            
            /**
             * @brief 设置终端类型
             * @param terminal_type 终端类型字符串（如"PC"、"Mobile"等）
             */
            virtual void SetTerminalType(const char* terminal_type) = 0;
            /**
             * @brief 获取终端类型
             * @return const char* 终端类型字符串
             */
            virtual const char* GetTerminalType() const = 0;
            
            // ================================================================================================
            // 网络信息设置和获取接口
            // ================================================================================================
            
            /**
             * @brief 设置公网IP地址
             * @param iip 公网IP地址字符串
             */
            virtual void SetIip(const char* iip) = 0;
            /**
             * @brief 获取公网IP地址
             * @return const char* 公网IP地址字符串
             */
            virtual const char* GetIip() const = 0;
            
            /**
             * @brief 设置公网端口号
             * @param iport 公网端口号字符串
             */
            virtual void SetIport(const char* iport) = 0;
            /**
             * @brief 获取公网端口号
             * @return const char* 公网端口号字符串
             */
            virtual const char* GetIport() const = 0;
            
            /**
             * @brief 设置内网IP地址
             * @param lip 内网IP地址字符串
             */
            virtual void SetLip(const char* lip) = 0;
            /**
             * @brief 获取内网IP地址
             * @return const char* 内网IP地址字符串
             */
            virtual const char* GetLip() const = 0;
            
            /**
             * @brief 设置MAC地址
             * @param mac 网卡MAC地址字符串
             */
            virtual void SetMac(const char* mac) = 0;
            /**
             * @brief 获取MAC地址
             * @return const char* 网卡MAC地址字符串
             */
            virtual const char* GetMac() const = 0;
            
            // ================================================================================================
            // 硬件信息设置和获取接口
            // ================================================================================================
            
            /**
             * @brief 设置硬盘序列号
             * @param hd 硬盘序列号字符串
             */
            virtual void SetHd(const char* hd) = 0;
            /**
             * @brief 获取硬盘序列号
             * @return const char* 硬盘序列号字符串
             */
            virtual const char* GetHd() const = 0;
            
            /**
             * @brief 设置PC终端设备名
             * @param pcn PC设备名称字符串
             */
            virtual void SetPcn(const char* pcn) = 0;
            /**
             * @brief 获取PC终端设备名
             * @return const char* PC设备名称字符串
             */
            virtual const char* GetPcn() const = 0;
            
            /**
             * @brief 设置CPU序列号
             * @param cpu CPU序列号字符串
             */
            virtual void SetCpu(const char* cpu) = 0;
            /**
             * @brief 获取CPU序列号
             * @return const char* CPU序列号字符串
             */
            virtual const char* GetCpu() const = 0;
            
            /**
             * @brief 设置硬盘分区信息
             * @param pi 硬盘分区信息字符串
             */
            virtual void SetPi(const char* pi) = 0;
            /**
             * @brief 获取硬盘分区信息
             * @return const char* 硬盘分区信息字符串
             */
            virtual const char* GetPi() const = 0;
            
            /**
             * @brief 设置系统盘卷标号
             * @param vol 系统盘卷标号字符串
             */
            virtual void SetVol(const char* vol) = 0;
            /**
             * @brief 获取系统盘卷标号
             * @return const char* 系统盘卷标号字符串
             */
            virtual const char* GetVol() const = 0;
            
            /**
             * @brief 设置交易终端软件名称及版本
             * @param tername 终端软件名称和版本信息字符串
             */
            virtual void SetTername(const char* tername) = 0;
            /**
             * @brief 获取交易终端软件名称及版本
             * @return const char* 终端软件名称和版本信息字符串
             */
            virtual const char* GetTername() const = 0;
        };

        /**
         * @class ATPLoginProperty
         * @brief 登录属性信息类
         * 
         * 封装用户登录ATP交易系统所需的认证信息和配置参数。
         * 支持两种登录模式：客户号模式和资金账号模式。
         * 
         * 主要功能：
         * - 管理用户认证信息（用户ID、密码、营业部等）
         * - 支持多种登录模式
         * - 提供扩展字段支持自定义参数
         * - 确保登录信息的安全传输
         */
        class QUANT_API ATPLoginProperty
        {
        public:
            /**
             * @brief 默认构造函数
             * 
             * 创建空的登录属性对象。
             */
            ATPLoginProperty() = default;

            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构。
             */
            virtual ~ATPLoginProperty() = default;

            /**
             * @brief 禁用拷贝构造函数
             * 
             * 登录属性包含敏感信息，不允许拷贝。
             */
            ATPLoginProperty(const ATPLoginProperty&) = delete;

            /**
             * @brief 禁用赋值操作符
             * 
             * 登录属性包含敏感信息，不允许赋值操作。
             */
            ATPLoginProperty& operator=(const ATPLoginProperty&) = delete;

            /**
             * @brief 创建登录属性对象（静态工厂方法）
             * @return ATPLoginProperty* 新创建的登录属性对象指针
             * 
             * 使用工厂模式创建对象，确保对象的正确初始化。
             */
            static ATPLoginProperty* NewMessage();

            /**
             * @brief 销毁登录属性对象（静态销毁方法）
             * @param ptr 要销毁的登录属性对象指针
             * 
             * 与NewMessage()配对使用，确保对象的正确销毁和敏感信息清理。
             */
            static void DeleteMessage(ATPLoginProperty* ptr);

        public:
            /**
             * @brief 设置用户ID
             * @param user_id 用户标识符
             * 
             * 根据登录模式的不同：
             * - 客户号登录模式：填入客户号
             * - 资金账号登录模式：填入资金账号
             */
            virtual void SetUserId(const char* user_id) = 0;
            /**
             * @brief 获取用户ID
             * @return const char* 用户标识符字符串
             */
            virtual const char* GetUserId() const = 0;
            
            /**
             * @brief 设置营业部ID
             * @param branch_id 营业部标识符
             * 
             * 用于标识用户所属的营业部，影响业务权限和资金清算。
             */
            virtual void SetBranchId(const char* branch_id) = 0;
            /**
             * @brief 获取营业部ID
             * @return const char* 营业部标识符字符串
             */
            virtual const char* GetBranchId() const = 0;
            
            /**
             * @brief 设置客户密码
             * @param password 客户交易密码
             * 
             * 用于用户身份验证，建议使用加密传输。
             */
            virtual void SetPassword(const char* password) = 0;
            /**
             * @brief 获取客户密码
             * @return const char* 客户交易密码字符串
             */
            virtual const char* GetPassword() const = 0;
            
            /**
             * @brief 设置登录模式
             * @param login_mode 登录模式标识
             * 
             * 支持的登录模式：
             * - kCustIDMode(1)：客户号登录模式
             * - kFundAccountIDMode(2)：资金账号登录模式
             */
            virtual void SetLoginMode(uint8_t login_mode) = 0;
            /**
             * @brief 获取登录模式
             * @return uint8_t 登录模式标识
             */
            virtual uint8_t GetLoginMode() const = 0;
            
            /**
             * @brief 设置扩展数据字段
             * @param extra_data 扩展数据字符串
             * 
             * 用于传递自定义的登录参数或附加信息。
             */
            virtual void SetExtraData(const char* extra_data) = 0;
            /**
             * @brief 获取扩展数据字段
             * @return const char* 扩展数据字符串
             */
            virtual const char* GetExtraData() const = 0;
        };

        /**
         * @class ATPCustomerInfo
         * @brief 交易用户信息类
         * 
         * 封装登录成功后返回的客户完整信息，包括客户的所有资金账户、
         * 证券账户以及对应的市场权限和分区信息。
         * 
         * 数据结构：
         * - 客户号（唯一标识）
         * - 资金账户数组（一个客户可能有多个资金账户）
         * - 每个资金账户下的证券账户数组（支持多市场交易）
         * - 每个证券账户的市场权限和分区信息
         * 
         * 主要用途：
         * - 获取客户的完整账户体系信息
         * - 确定客户的交易权限和可交易市场
         * - 为后续交易操作提供账户选择依据
         */
        class QUANT_API ATPCustomerInfo
        {
        public:
            /**
             * @brief 默认构造函数
             * 
             * 创建空的客户信息对象。
             */
            ATPCustomerInfo() = default;

            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构。
             */
            virtual ~ATPCustomerInfo() = default;

            /**
             * @brief 禁用拷贝构造函数
             * 
             * 客户信息包含完整的账户体系，不允许拷贝。
             */
            ATPCustomerInfo(const ATPCustomerInfo&) = delete;

            /**
             * @brief 禁用赋值操作符
             * 
             * 客户信息包含完整的账户体系，不允许赋值操作。
             */
            ATPCustomerInfo& operator=(const ATPCustomerInfo&) = delete;

        public:
            /**
             * @brief 获取客户号
             * @return const char* 客户号字符串，客户的唯一标识符
             */
            virtual const char* GetCustId() const = 0;
            
            /**
             * @brief 获取是否为多分区账号标志
             * @return uint8_t 多分区标志，1表示多分区账号，0表示单分区账号
             * 
             * 多分区账号可以在不同的交易分区进行交易，提高交易并发性能。
             */
            virtual uint8_t GetIsMultiPartitions() const = 0;
            
            /**
             * @brief 获取资金账户数组大小
             * @return uint32_t 资金账户的数量
             * 
             * 一个客户可能拥有多个资金账户，用于不同的业务类型。
             */
            virtual uint32_t FundAccountArraySize() const = 0;
            
            /**
             * @brief 获取指定索引的资金账户ID
             * @param index 资金账户数组索引
             * @return const char* 资金账户ID字符串
             */
            virtual const char* FundAccountArray_GetFundAccountId(uint32_t index) const = 0;
            
            /**
             * @brief 获取指定资金账户对应的营业部ID
             * @param index 资金账户数组索引
             * @return const char* 营业部ID字符串
             */
            virtual const char* FundAccountArray_GetBranchId(uint32_t index) const = 0;
            
            /**
             * @brief 获取指定资金账户下的证券账户数组大小
             * @param index 资金账户数组索引
             * @return uint32_t 该资金账户下证券账户的数量
             * 
             * 一个资金账户可能对应多个证券账户，用于不同市场的交易。
             */
            virtual uint32_t FundAccountArray_AccountArraySize(uint32_t index) const = 0;
            
            /**
             * @brief 获取指定的证券账户ID
             * @param index1 资金账户数组索引
             * @param index2 证券账户数组索引
             * @return const char* 证券账户ID字符串
             */
            virtual const char* FundAccountArray_AccountArray_GetAccountId(uint32_t index1, uint32_t index2) const = 0;
            
            /**
             * @brief 获取指定证券账户对应的市场代码
             * @param index1 资金账户数组索引
             * @param index2 证券账户数组索引
             * @return uint16_t 市场代码（如101-上海，102-深圳）
             */
            virtual uint16_t FundAccountArray_AccountArray_GetMarketId(uint32_t index1, uint32_t index2) const = 0;
            
            /**
             * @brief 获取指定证券账户的角色
             * @param index1 资金账户数组索引
             * @param index2 证券账户数组索引
             * @return uint8_t 账户角色标识
             */
            virtual uint8_t FundAccountArray_AccountArray_GetAccountRole(uint32_t index1, uint32_t index2) const = 0;
            
            /**
             * @brief 获取指定证券账户所属的分区号
             * @param index1 资金账户数组索引
             * @param index2 证券账户数组索引
             * @return uint8_t 分区号，用于确定交易路由
             */
            virtual uint8_t FundAccountArray_AccountArray_GetPartitionNo(uint32_t index1, uint32_t index2) const = 0;
        };

        /**
         * @class ATPConnectionInfo
         * @brief 连接信息类
         * 
         * 封装ATP量化API的连接状态和配置信息，提供连接相关的状态查询功能。
         * 主要用于监控连接状态和获取连接相关的配置信息。
         * 
         * 主要功能：
         * - 获取API实例的唯一标识
         * - 查询多通道推送的订阅状态
         * - 提供连接状态的诊断信息
         */
        class QUANT_API ATPConnectionInfo
        {
        public:
            /**
             * @brief 默认构造函数
             * 
             * 创建空的连接信息对象。
             */
            ATPConnectionInfo() = default;

            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构。
             */
            virtual ~ATPConnectionInfo() = default;

            /**
             * @brief 禁用拷贝构造函数
             * 
             * 连接信息包含实例特定的状态，不允许拷贝。
             */
            ATPConnectionInfo(const ATPConnectionInfo&) = delete;

            /**
             * @brief 禁用赋值操作符
             * 
             * 连接信息包含实例特定的状态，不允许赋值操作。
             */
            ATPConnectionInfo& operator=(const ATPConnectionInfo&) = delete;

        public:
            /**
             * @brief 获取API实例ID
             * @return int32_t API实例的唯一标识符
             * 
             * 当系统中存在多个ATPQuantAPI实例时，通过实例ID可以区分不同的实例，
             * 便于日志记录和问题定位。
             */
            virtual int32_t GetInstanceId() const = 0;
            
            /**
             * @brief 获取多通道主推消息订阅结果
             * @return uint8_t 订阅结果标识
             * 
             * 返回多通道推送消息的订阅状态：
             * - 0: 未订阅或订阅失败
             * - 1: 订阅成功
             * - 其他值: 特定的订阅状态
             */
            virtual uint8_t GetMultiChannelResult() const = 0;
        };



        // ================================================================================================
        // 交易消息类定义区域
        // ================================================================================================
        
        /**
         * @class ATPReqCashAuctionOrderMsg
         * @brief 现货集中竞价委托请求消息类
         * 
         * 用于封装现货交易的委托请求信息，支持股票、基金、债券等现货品种的买卖。
         * 包含完整的委托信息：账户信息、证券信息、价格数量、订单类型等。
         * 
         * 主要功能：
         * - 封装现货买卖委托的所有必要信息
         * - 支持多种订单类型（限价、市价等）
         * - 提供消息序列化和反序列化功能
         * - 支持批量委托的批次号管理
         * 
         * 使用流程：
         * 1. 调用NewMessage()创建消息对象
         * 2. 设置各种委托参数（账户、证券、价格等）
         * 3. 调用ATPQuantAPI::ReqCashAuctionOrder()发送委托
         * 4. 通过OnRspCashAuctionOrder()接收委托响应
         * 5. 调用DeleteMessage()销毁消息对象
         */
        class QUANT_API ATPReqCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 默认构造函数
             * 
             * 创建空的现货委托请求消息对象。
             */
            ATPReqCashAuctionOrderMsg() = default;

            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构。
             */
            virtual ~ATPReqCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造函数
             * 
             * 委托消息包含特定的交易信息，不允许拷贝。
             */
            ATPReqCashAuctionOrderMsg(const ATPReqCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值操作符
             * 
             * 委托消息包含特定的交易信息，不允许赋值操作。
             */
            ATPReqCashAuctionOrderMsg& operator=(const ATPReqCashAuctionOrderMsg&) = delete;
            
            /**
             * @brief 创建现货委托消息对象（静态工厂方法）
             * @param business_type 业务类型，指定具体的交易业务
             * @return ATPReqCashAuctionOrderMsg* 新创建的委托消息对象指针
             * 
             * 根据不同的业务类型创建对应的委托消息对象，
             * 不同业务类型可能有不同的字段要求。
             */
            static ATPReqCashAuctionOrderMsg* NewMessage(ATPBusinessTypeType business_type);

            /**
             * @brief 销毁现货委托消息对象（静态销毁方法）
             * @param msg_ptr 要销毁的委托消息对象指针
             * 
             * 与NewMessage()配对使用，确保对象的正确销毁和内存释放。
             */
            static void DeleteMessage(ATPReqCashAuctionOrderMsg* msg_ptr);

            /**
             * @brief 将消息对象编码为二进制数据流
             * @return Buffer 编码后的二进制数据流
             * 
             * 将委托消息对象序列化为二进制格式，用于网络传输。
             * 编码后的数据可以通过网络发送到交易系统。
             */
            virtual Buffer Encode() = 0;

            /**
             * @brief 将二进制数据流解码为消息对象
             * @param[in] buffer 二进制数据流
             * @return bool 解码是否成功，true表示成功，false表示失败
             * 
             * 将从网络接收的二进制数据反序列化为消息对象，
             * 用于解析服务器返回的响应数据。
             */
            virtual bool Decode(const Buffer& buffer) = 0;

        public:
            // ================================================================================================
            // 账户信息字段设置和获取接口
            // ================================================================================================
            
            /**
             * @brief 设置客户号ID
             * @param cust_id 客户号字符串，客户的唯一标识符
             */
            virtual void SetCustId(const char* cust_id) = 0;
            /**
             * @brief 获取客户号ID
             * @return const char* 客户号字符串
             */
            virtual const char* GetCustId() const = 0;
            
            /**
             * @brief 设置资金账号ID
             * @param fund_account_id 资金账号字符串
             */
            virtual void SetFundAccountId(const char* fund_account_id) = 0;
            /**
             * @brief 获取资金账号ID
             * @return const char* 资金账号字符串
             */
            virtual const char* GetFundAccountId() const = 0;
            
            /**
             * @brief 设置营业部ID
             * @param branch_id 营业部标识符字符串
             */
            virtual void SetBranchId(const char* branch_id) = 0;
            /**
             * @brief 获取营业部ID
             * @return const char* 营业部标识符字符串
             */
            virtual const char* GetBranchId() const = 0;
            
            /**
             * @brief 设置证券账户ID
             * @param account_id 证券账户字符串
             */
            virtual void SetAccountId(const char* account_id) = 0;
            /**
             * @brief 获取证券账户ID
             * @return const char* 证券账户字符串
             */
            virtual const char* GetAccountId() const = 0;
            
            /**
             * @brief 设置客户交易密码
             * @param password 客户交易密码字符串
             */
            virtual void SetPassword(const char* password) = 0;
            /**
             * @brief 获取客户交易密码
             * @return const char* 客户交易密码字符串
             */
            virtual const char* GetPassword() const = 0;
            
            // ================================================================================================
            // 证券和交易信息字段设置和获取接口
            // ================================================================================================
            
            /**
             * @brief 设置证券代码
             * @param security_id 证券代码字符串（配股业务时为配售权证代码）
             */
            virtual void SetSecurityId(const char* security_id) = 0;
            /**
             * @brief 获取证券代码
             * @return const char* 证券代码字符串
             */
            virtual const char* GetSecurityId() const = 0;
            
            /**
             * @brief 设置市场代码
             * @param market_id 市场代码（101-上海，102-深圳，103-香港，104-北京）
             */
            virtual void SetMarketId(uint16_t market_id) = 0;
            /**
             * @brief 获取市场代码
             * @return uint16_t 市场代码
             */
            virtual uint16_t GetMarketId() const = 0;
            
            /**
             * @brief 设置买卖方向
             * @param side 买卖方向字符（'1'-买入，'2'-卖出）
             */
            virtual void SetSide(char side) = 0;
            /**
             * @brief 获取买卖方向
             * @return char 买卖方向字符
             */
            virtual char GetSide() const = 0;
            
            /**
             * @brief 设置申报数量
             * @param order_qty 申报数量，精度N15(2)（股票为股，基金为份，债券为张）
             */
            virtual void SetOrderQty(double order_qty) = 0;
            /**
             * @brief 获取申报数量
             * @return double 申报数量
             */
            virtual double GetOrderQty() const = 0;
            
            /**
             * @brief 设置委托价格
             * @param price 委托价格，精度N13(4)（市价单可填0）
             */
            virtual void SetPrice(double price) = 0;
            /**
             * @brief 获取委托价格
             * @return double 委托价格
             */
            virtual double GetPrice() const = 0;
            
            /**
             * @brief 设置订单类型
             * @param order_type 订单类型字符（'a'-限价，'b'-本方最优等）
             */
            virtual void SetOrderType(char order_type) = 0;
            /**
             * @brief 获取订单类型
             * @return char 订单类型字符
             */
            virtual char GetOrderType() const = 0;
            
            /**
             * @brief 设置客户自定义委托批号
             * @param batch_cl_ord_no 委托批号（0为系统保留值，不允许使用）
             * 
             * @note ATP3.2.3版本开始支持，且仅支持现货集中竞价业务
             * 用于批量管理订单，支持按批次号进行批量撤单等操作。
             */
            virtual void SetBatchClOrdNo(uint64_t batch_cl_ord_no) = 0;
            /**
             * @brief 获取客户自定义委托批号
             * @return uint64_t 委托批号
             */
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };
        /**
         * @brief 现货订单委托响应
         */
        class QUANT_API ATPRspCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashAuctionOrderMsg(const ATPRspCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashAuctionOrderMsg& operator=(const ATPRspCashAuctionOrderMsg&) = delete;
        public:
            /**
             * @brief 构建消息
             */
            static ATPRspCashAuctionOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashAuctionOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //客户自定义委托批号（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //委托价格N13(4)
            virtual double GetPrice() const = 0;
            //委托数量N15(2)
            virtual double GetOrderQty() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //订单类型
            virtual char GetOrderType() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
        };
        /**
         * @brief 现货撤单委托响应
         */
        class QUANT_API ATPRspCashCancelOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashCancelOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashCancelOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashCancelOrderMsg(const ATPRspCashCancelOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashCancelOrderMsg& operator=(const ATPRspCashCancelOrderMsg&) = delete;
        public:
            /**
             * @brief 构建消息
             */
            static ATPRspCashCancelOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashCancelOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //原委托的客户订单编号
            virtual int64_t GetOrigClOrdNo() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
            //客户自定义委托批号，指示被撤消订单的batch_cl_ord_no。（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };
        /**
         * @brief 现货主推消息
         */
        class QUANT_API ATPRtnCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRtnCashAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRtnCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRtnCashAuctionOrderMsg(const ATPRtnCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRtnCashAuctionOrderMsg& operator=(const ATPRtnCashAuctionOrderMsg&) = delete;
        public:
            /**
             * @brief 构建消息
             */
            static ATPRtnCashAuctionOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRtnCashAuctionOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账号ID 
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual const char* GetAccountId() const = 0;
            //业务类型 
            virtual uint8_t GetBusinessType() const = 0;
            //客户订单编号 
            virtual int64_t GetClOrdNo() const = 0;
            //客户自定义委托批号（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //证券代码 
            virtual const char* GetSecurityId() const = 0;
            //市场代码 
            virtual uint16_t GetMarketId() const = 0;
            //订单状态 
            virtual uint8_t GetOrdStatus() const = 0;
            //订单标识 
            virtual uint8_t GetOrdSign() const = 0;
            //委托价格N13(4) 
            virtual double GetPrice() const = 0;
            //委托数量N15(2) 
            virtual double GetOrderQty() const = 0;
            //未成交部分的数量N15(2) 
            virtual double GetLeavesQty() const = 0;
            //累计成交数量N15(2) 
            virtual double GetCumQty() const = 0;
            //买卖方向 
            virtual char GetSide() const = 0;
            //委托时间 
            virtual int64_t GetTransactTime() const = 0;
            //交易所订单编号 
            virtual const char* GetOrderId() const = 0;
            //申报合同号 
            virtual const char* GetClOrdId() const = 0;
            //冻结交易金额N15(4)
            virtual double GetFrozenTradeValue() const = 0;
            //冻结费用N15(4)
            virtual double GetFrozenFee() const = 0;
            //成交编号
            virtual const char* GetExecId() const = 0;
            //成交价格N13(4)
            virtual double GetLastPx() const = 0;
            //成交数量N15(2)
            virtual double GetLastQty() const = 0;
            //成交金额N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //ETF成交回报类型
            virtual uint8_t GetEtfTradeReportType() const = 0;
            //信用标识
            virtual char GetCashMargin() const = 0;
            //到期日
            virtual int64_t GetMaturityDate() const = 0;
            //原委托的客户订单编号
            virtual int64_t GetOrigClOrdNo() const = 0;
            //母单订单编号
            virtual int64_t GetParentClOrdNo() const = 0;
            //拒绝原因代码
            virtual int32_t GetRejectReasonCode() const = 0;
            //拒绝原因描述
            virtual const char* GetOrdRejReason() const = 0;
        };
        /**
         * @brief 现货通用撤单消息
         */
        class QUANT_API ATPReqCashCancelOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashCancelOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashCancelOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashCancelOrderMsg(const ATPReqCashCancelOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashCancelOrderMsg& operator=(const ATPReqCashCancelOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashCancelOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashCancelOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //原始交易客户方（券商）订单编号，指示被撤消订单的cl_ord_no（ATP3.2.3开始支持现货集中竞价非必填，其他业务必填）
            virtual void SetOrigClOrdNo(int64_t) = 0;
            virtual int64_t GetOrigClOrdNo() const = 0;
            //客户自定义委托批号，指示被撤消订单的batch_cl_ord_no。batch_cl_ord_no与orig_cl_ord_no至少填写一项。若orig_cl_ord_no不为0，则按orig_cl_ord_no撤单；否则，batch_cl_ord_no不为0，按batch_cl_ord_no撤单（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };
        /**
         * @brief 现货订单查询消息
         */
        class QUANT_API ATPReqCashOrderQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashOrderQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashOrderQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashOrderQueryMsg(const ATPReqCashOrderQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashOrderQueryMsg& operator=(const ATPReqCashOrderQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashOrderQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashOrderQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //客户订单编号，0表示查所有
            virtual void SetClOrdNo(int64_t) = 0;
            virtual int64_t GetClOrdNo() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //买卖方向，'0'表示查所有
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //委托查询条件，0表示查所有
            virtual void SetOrderQueryCondition(uint8_t) = 0;
            virtual uint8_t GetOrderQueryCondition() const = 0;
            //查询返回数量，0表示按能返回的最大数量返回，具体数量请咨询券商
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)(ATP3.2.3开始支持，且仅支持现货集中竞价)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };
        /**
         * @brief 现货订单查询结果
         */
        class QUANT_API ATPRspCashOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashOrderQueryResultMsg(const ATPRspCashOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashOrderQueryResultMsg& operator=(const ATPRspCashOrderQueryResultMsg&) = delete;
        public:
            //终端识别码（ATP3.2.3 开始支持）
            virtual const char* GetClientFeatureCode() const = 0;
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类别
            virtual char GetOrdType() const = 0;
            //订单标识 
            virtual uint8_t GetOrdSign() const = 0;
            //当前申报的状态（对于撤单订单，填写被撤订单的当前状态，找不到原始订单时，固定填8 = Reject ）
            virtual uint8_t GetOrdStatus() const = 0;
            //申报状态详细信息
            virtual const char* GetOrderStatusInfo() const = 0;
            //客户委托时间
            virtual int64_t GetTransactTime() const = 0;
            //委托价格N13(4)
            virtual double GetOrderPrice() const = 0;
            //平均成交价格N13(4)
            virtual double GetExecPrice() const = 0;
            //委托数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetOrderQty() const = 0;
            //未成交部分的数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetLeavesQty() const = 0;
            //累计成交数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetCumQty() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //交易所订单编号
            virtual const char* GetOrderId() const = 0;
            //申报合同号,上交所：以QP1开头,表示为交易所保证金强制平仓；以CV1开头,表示为交易所备兑强制平仓；
            virtual const char* GetClOrdId() const = 0;
            //对于撤单订单，为原始交易客户方（券商）订单编号，指示被撤消订单的cl_ord_no; 对于普通订单，取值为0
            virtual int64_t GetOrigClOrdNo() const = 0;
            //冻结交易金额N15(4)
            virtual double GetFrozenTradeValue() const = 0;
            //冻结费用N15(4)
            virtual double GetFrozenFee() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //委托金额N15(4)
            virtual double GetOrderEntrustedAmt() const = 0;
            //成交金额N15(4)
            virtual double GetOrderCumTransactionAmt() const = 0;
            //执行类型（ATP3.1.8 开始支持）
            virtual char GetExecType() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
            //已撤单数量N15(2)（ATP3.1.9 开始支持）
            virtual double GetCanceledQty() const = 0;
            //订单标识（ATP3.1.9 开始支持）（返回默认值表示当前版本不支持）
            virtual char GetOrderFlag() const = 0;
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户自定义委托批号，对于撤单订单，为原订单的客户自定义委托批号。（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };
        /**
         * @brief 现货股份查询
         */
        class QUANT_API ATPReqCashShareQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashShareQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashShareQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashShareQueryMsg(const ATPReqCashShareQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashShareQueryMsg& operator=(const ATPReqCashShareQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashShareQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashShareQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示按能返回的最大数量返回，具体数量请咨询券商
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };
        /**
         * @brief 现货股份查询结果
         */
        class QUANT_API ATPRspCashShareQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashShareQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashShareQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashShareQueryResultMsg(const ATPRspCashShareQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashShareQueryResultMsg& operator=(const ATPRspCashShareQueryResultMsg&) = delete;
        public:
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //日初持仓量N15(2)
            virtual double GetInitQty() const = 0;
            //剩余股份数量N15(2)
            virtual double GetLeavesQty() const = 0;
            //可用股份数量N15(2)
            virtual double GetAvailableQty() const = 0;
            //浮动盈亏N15(4)
            virtual double GetProfitLoss() const = 0;
            //市值N15(4)
            virtual double GetMarketValue() const = 0;
            //成本价N15(4)
            virtual double GetCostPrice() const = 0;
            //最新价N13(4)
            virtual double GetLastPrice() const = 0;
            //股份买入解冻
            virtual double GetStockBuy() const = 0;
            //股份卖出冻结
            virtual double GetStockSale() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //对于ETF基金表示可赎回数量，对于其他证券表示可用于ETF申购的数量（ATP3.2.1开始支持）
            virtual double GetEtfRedemptionQty() const = 0;
        };
        /**
         * @brief 现货按证券账户划转资金请求消息
         */
        class QUANT_API ATPReqCashExternalInsTETransFundReqMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExternalInsTETransFundReqMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExternalInsTETransFundReqMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExternalInsTETransFundReqMsg(const ATPReqCashExternalInsTETransFundReqMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExternalInsTETransFundReqMsg& operator=(const ATPReqCashExternalInsTETransFundReqMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExternalInsTETransFundReqMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExternalInsTETransFundReqMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //资金转出账户ID 
            virtual void SetFundOutAccountId(const char*) = 0;
            virtual const char* GetFundOutAccountId() const = 0;
            //资金转出市场代码
            virtual void SetFundOutMarketId(uint16_t) = 0;
            virtual uint16_t GetFundOutMarketId() const = 0;
            //资金转入账户ID 
            virtual void SetFundInAccountId(const char*) = 0;
            virtual const char* GetFundInAccountId() const = 0;
            //资金转入市场代码
            virtual void SetFundInMarketId(uint16_t) = 0;
            virtual uint16_t GetFundInMarketId() const = 0;
            //划拨金额 N15(4)
            virtual void SetValue(double) = 0;
            virtual double GetValue() const = 0;
        };
        /**
         * @brief 按证券账户划转资金响应消息
         */
        class QUANT_API ATPRspCashExternalInsTETransFundRespMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExternalInsTETransFundRespMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExternalInsTETransFundRespMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExternalInsTETransFundRespMsg(const ATPRspCashExternalInsTETransFundRespMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExternalInsTETransFundRespMsg& operator=(const ATPRspCashExternalInsTETransFundRespMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
        };
        /**
         * @brief 现货资金划拨请求消息
         */
        class QUANT_API ATPReqCashExtFundTransferOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExtFundTransferOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExtFundTransferOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExtFundTransferOtherMsg(const ATPReqCashExtFundTransferOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExtFundTransferOtherMsg& operator=(const ATPReqCashExtFundTransferOtherMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExtFundTransferOtherMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExtFundTransferOtherMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID（ATP3.1.9开始支持非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //划拨方式 ：1=转入；2=转出
            virtual void SetTransferWay(uint8_t) = 0;
            virtual uint8_t GetTransferWay() const = 0;
            //划拨金额 N13(2) 
            virtual void SetTransferValue(double) = 0;
            virtual double GetTransferValue() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };
        /**
         * @brief 现货资金划拨(转入、转出)应答消息
         */
        class QUANT_API ATPRspCashExtFundTransferResultOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExtFundTransferResultOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExtFundTransferResultOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExtFundTransferResultOtherMsg(const ATPRspCashExtFundTransferResultOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExtFundTransferResultOtherMsg& operator=(const ATPRspCashExtFundTransferResultOtherMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //回报时间
            virtual int64_t GetTransactTime() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //日初可用余额N15(4)
            virtual double GetInitAmt() const = 0;
            //委托预冻结N15(4)
            virtual double GetOrderFrozen() const = 0;
            //买入成交N15(4)
            virtual double GetBuyTrade() const = 0;
            //卖出成交N15(4)
            virtual double GetSellTrade() const = 0;
            //异常冻结N15(4)
            virtual double GetUnusualFrozen() const = 0;
            //异常冻结取消N15(4)
            virtual double GetUnusualFrozenCancel() const = 0;
            //冻结费用N15(4)
            virtual double GetFeeFrozen() const = 0;
            //成交费用N15(4)
            virtual double GetFeeTrade() const = 0;
            //当日入金N15(4)
            virtual double GetTodayIn() const = 0;
            //当日出金N15(4)
            virtual double GetTodayOut() const = 0;
            //临时调增N15(4)
            virtual double GetTempAdd() const = 0;
            //临时调减N15(4)
            virtual double GetTempSub() const = 0;
            //临时冻结N15(4)
            virtual double GetTempFrozen() const = 0;
            //临时冻结取消N15(4)
            virtual double GetTempFrozenCancel() const = 0;
            //调整前资金余额N15(4)
            virtual double GetPreBalance() const = 0;
            //调整前T+0可用资金N15(4)
            virtual double GetPreAvailableT0() const = 0;
            //调整前T+1在途可用资金N15(4)
            virtual double GetPreOnTheWayT1() const = 0;
            //转移金额N13(2)
            virtual double GetTransferValue() const = 0;
        };
        /**
         * @brief 现货成交查询消息
         */
        class QUANT_API ATPReqCashTradeOrderQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashTradeOrderQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashTradeOrderQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashTradeOrderQueryMsg(const ATPReqCashTradeOrderQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashTradeOrderQueryMsg& operator=(const ATPReqCashTradeOrderQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashTradeOrderQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashTradeOrderQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型， 0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //查询返回数量，0表示按能返回的最大数量返回，具体数量请咨询券商
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序，1表示按时间正序排序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //客户订单编号，0表示查所有
            virtual void SetClOrdNo(int64_t) = 0;
            virtual int64_t GetClOrdNo() const = 0;
            //报盘合同号 
            virtual void SetClOrdId(const char*) = 0;
            virtual const char* GetClOrdId() const = 0;
            //执行编号 
            virtual void SetExecId(const char*) = 0;
            virtual const char* GetExecId() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)(ATP3.2.3开始支持，且仅支持现货集中竞价)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };
        /**
         * @brief 现货成交查询结果
         */
        class QUANT_API ATPRspCashTradeOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashTradeOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashTradeOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashTradeOrderQueryResultMsg(const ATPRspCashTradeOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashTradeOrderQueryResultMsg& operator=(const ATPRspCashTradeOrderQueryResultMsg&) = delete;
        public:
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类别
            virtual char GetOrdType() const = 0;
            //执行类型（ATP3.1.8 开始支持）
            virtual char GetExecType() const = 0;
            //执行编号
            virtual const char* GetExecId() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //交易所订单编号
            virtual const char* GetOrderId() const = 0;
            //申报合同号,上交所：以QP1开头,表示为交易所保证金强制平仓；以CV1开头,表示为交易所备兑强制平仓；
            virtual const char* GetClOrdId() const = 0;
            //客户委托时间
            virtual int64_t GetTransactTime() const = 0;
            //成交价格N13(4)
            virtual double GetLastPx() const = 0;
            //成交数量N15(2)
            virtual double GetLastQty() const = 0;
            //成交金额N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //成交费用N15(4)
            virtual double GetFee() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
            //ETF成交回报类型
            virtual uint8_t GetEtfTradeReportType() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //客户自定义委托批号
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };
        /**
         * @brief 现货证券信息查询消息
         */
        class QUANT_API ATPReqCashExtQuerySecurityInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExtQuerySecurityInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExtQuerySecurityInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExtQuerySecurityInfoMsg(const ATPReqCashExtQuerySecurityInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExtQuerySecurityInfoMsg& operator=(const ATPReqCashExtQuerySecurityInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExtQuerySecurityInfoMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExtQuerySecurityInfoMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码，0表示查所有
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示按能返回的最大数量返回，具体数量请咨询券商
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };
        /**
         * @brief 现货证券信息查询结果消息
         */
        class QUANT_API ATPRspCashExtQueryResultSecurityInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExtQueryResultSecurityInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExtQueryResultSecurityInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExtQueryResultSecurityInfoMsg(const ATPRspCashExtQueryResultSecurityInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExtQueryResultSecurityInfoMsg& operator=(const ATPRspCashExtQueryResultSecurityInfoMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //昨日收盘价N13(4)
            virtual double GetPrevClosePrice() const = 0;
            //涨停价N13(4)
            virtual double GetPriceUpperLimit() const = 0;
            //跌停价N13(4)
            virtual double GetPriceLowerLimit() const = 0;
            //最小变动价位N13(4)
            virtual double GetPriceTick() const = 0;
            //限价买数量下限N15(2)
            virtual double GetBuyQtyLowerLimit() const = 0;
            //限价卖数量下限N15(2)
            virtual double GetSellQtyLowerLimit() const = 0;
            //是否支持回转交易
            virtual bool GetDayTrading() const = 0;
            //涨跌幅限制
            virtual bool GetHasPriceLimit() const = 0;
            //证券状态
            virtual uint64_t GetSecurityStatus() const = 0;
            //限价买数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetBuyQtyUnit() const = 0;
            //限价卖数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetSellQtyUnit() const = 0;
            //市价买数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetMarketBuyQtyUnit() const = 0;
            //市价卖数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetMarketSellQtyUnit() const = 0;
            //市价买数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketBuyQtyUpperLimit() const = 0;
            //市价买数量下限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketBuyQtyLowerLimit() const = 0;
            //市价卖数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketSellQtyUpperLimit() const = 0;
            //市价卖数量下限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketSellQtyLowerLimit() const = 0;
            //限价买数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetBuyQtyUpperLimit() const = 0;
            //限价卖数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetSellQtyUpperLimit() const = 0;
        };
        /**
         * @brief 现货资金查询消息
         */
        class QUANT_API ATPReqCashFundQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashFundQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashFundQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashFundQueryMsg(const ATPReqCashFundQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashFundQueryMsg& operator=(const ATPReqCashFundQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashFundQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashFundQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID （ATP3.1.7版本开始支持此字段非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID（ATP3.1.7版本开始支持此字段非必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //货币种类 （ATP3.1.7起开始支持非必填）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };
        /**
         * @brief 现货资金查询结果
         */
        class QUANT_API ATPRspCashFundQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashFundQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashFundQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashFundQueryResultMsg(const ATPRspCashFundQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashFundQueryResultMsg& operator=(const ATPRspCashFundQueryResultMsg&) = delete;
        public:
            //资金余额N15(4)
            virtual double GetLeavesValue() const = 0;
            //日初资金金额N15(4)
            virtual double GetInitLeavesValue() const = 0;
            //当前所有冻结N15(4)
            virtual double GetFrozenAll() const = 0;
            //当日可取资金N15(4)
            virtual double GetAvailableT0() const = 0;
            //当日可用资金N15(4)
            virtual double GetAvailableT1() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
        };
        /**
         * @brief 现货资产查询消息
         */
        class QUANT_API ATPReqCashAssetQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashAssetQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashAssetQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashAssetQueryMsg(const ATPReqCashAssetQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashAssetQueryMsg& operator=(const ATPReqCashAssetQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashAssetQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashAssetQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //货币种类 
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };
        /**
         * @brief 现货资产查询结果
         */
        class QUANT_API ATPRspCashAssetQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashAssetQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashAssetQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashAssetQueryResultMsg(const ATPRspCashAssetQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashAssetQueryResultMsg& operator=(const ATPRspCashAssetQueryResultMsg&) = delete;
        public:
            //资金余额,N15(4)
            virtual double GetLeavesValue() const = 0;
            //日初资金金额,N15(4)
            virtual double GetInitLeavesValue() const = 0;
            //当前所有冻结,N15(4)
            virtual double GetFrozenAll() const = 0;
            //当日可取资金,N15(4)
            virtual double GetAvailableT0() const = 0;
            //当日可用资金,N15(4)
            virtual double GetAvailableT1() const = 0;
            //资产总值,N15(4)
            virtual double GetTotalAsset() const = 0;
            //总市值,N15(4)
            virtual double GetMarketValue() const = 0;
            //买入冻结,N15(4)
            virtual double GetFundBuy() const = 0;
            //卖出解冻,N15(4)
            virtual double GetFundSale() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
        };
        /**
         * @brief 现货成交汇总查询消息
         */
        class QUANT_API ATPReqCashTradeCollectQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashTradeCollectQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashTradeCollectQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashTradeCollectQueryMsg(const ATPReqCashTradeCollectQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashTradeCollectQueryMsg& operator=(const ATPReqCashTradeCollectQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashTradeCollectQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashTradeCollectQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //密码 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //市场代码，0表示查所有 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码 
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
        };
        /**
         * @brief 现货成交汇总查询结果
         */
        class QUANT_API ATPRspCashCollectQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashCollectQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashCollectQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashCollectQueryResultMsg(const ATPRspCashCollectQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashCollectQueryResultMsg& operator=(const ATPRspCashCollectQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买入均价N13(4)
            virtual double GetBuyAvgPrice() const = 0;
            //买入累计总量（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetBuyCumQty() const = 0;
            //买入成交总金额，精确到3位小数N15(4)
            virtual double GetBuyTotalValueTraded() const = 0;
            //卖出均价N13(4)
            virtual double GetSellAvgPrice() const = 0;
            //卖出累计总量（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetSellCumQty() const = 0;
            //卖出成交金额，精确到3位小数N15(4)
            virtual double GetSellTotalValueTraded() const = 0;
            //买卖合计成交金额，精确到3位小数N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码 
            virtual uint16_t GetMarketId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
        };
        /**
         * @brief 现货最大可委托数查询消息
         */
        class QUANT_API ATPReqCashMaxOrderQtyQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashMaxOrderQtyQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashMaxOrderQtyQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashMaxOrderQtyQueryMsg(const ATPReqCashMaxOrderQtyQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashMaxOrderQtyQueryMsg& operator=(const ATPReqCashMaxOrderQtyQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashMaxOrderQtyQueryMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashMaxOrderQtyQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券代码 
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //密码 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //业务类型 
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //买卖方向 
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //委托价格N13(4)(市价填0，科创版填保护限价) 
            virtual void SetPrice(double) = 0;
            virtual double GetPrice() const = 0;
            //订单类型 
            virtual void SetOrderType(char) = 0;
            virtual char GetOrderType() const = 0;
        };
        /**
         * @brief 现货最大可委托数查询结果
         */
        class QUANT_API ATPRspCashMaxOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashMaxOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashMaxOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashMaxOrderQueryResultMsg(const ATPRspCashMaxOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashMaxOrderQueryResultMsg& operator=(const ATPRspCashMaxOrderQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //最大可委托数（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetMaxOrderQty() const = 0;
            //理论可委托数（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetTheoreticalOrderQty() const = 0;
        };
        /**
         * @brief ETF信息查询消息
         */
        class QUANT_API ATPReqExtQueryETFInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExtQueryETFInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExtQueryETFInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExtQueryETFInfoMsg(const ATPReqExtQueryETFInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExtQueryETFInfoMsg& operator=(const ATPReqExtQueryETFInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExtQueryETFInfoMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExtQueryETFInfoMsg* msg_ptr);

        public:
            //市场代码，0表示查所有
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示按能返回的最大数量返回，具体数量请咨询券商
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //申赎类型（默认值为0，表示查所有）
            virtual void SetPrType(uint8_t) = 0;
            virtual uint8_t GetPrType() const = 0;
        };
        /**
         * @brief ETF信息查询结果
         */
        class QUANT_API ATPRspExtQueryResultETFInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExtQueryResultETFInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExtQueryResultETFInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExtQueryResultETFInfoMsg(const ATPRspExtQueryResultETFInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExtQueryResultETFInfoMsg& operator=(const ATPRspExtQueryResultETFInfoMsg&) = delete;
        public:
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //ETF代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //申赎类型（1-申赎，2-实物申赎）
            virtual uint8_t GetPrType() const = 0;
            //是否有PD券商资格
            virtual bool GetIsPdBroker() const = 0;
            //是否允许申购
            virtual bool GetIsPurchase() const = 0;
            //是否允许赎回
            virtual bool GetIsRedemption() const = 0;
            //ETF申购赎回单位N15(2)
            virtual double GetPurchaseRedemptionUnit() const = 0;
            //ETF股票篮现金差额N15(4)
            virtual double GetEstimateCashComponent() const = 0;
            //ETF最大现金替代比例N6(5)
            virtual double GetMaxCashRatio() const = 0;
            //ETF股票篮记录数
            virtual double GetTotalRecordNum() const = 0;
            //ETF单位净值
            virtual double GetNav() const = 0;
            //ETF基准单位净值
            virtual double GetNavPerCu() const = 0;
            //单账户累计申购上限N18(2)
            virtual double GetPurchaseLimitPerUser() const = 0;
            //单账户累计赎回上限N18(2)
            virtual double GetRedemptionLimitPerUser() const = 0;
            //单账户净申购上限N18(2)
            virtual double GetNetPurchaseLimitPerUser() const = 0;
            //单账户净赎回上限N18(2)
            virtual double GetNetRedemptionLimitPerUser() const = 0;
        };
        
        /**
         * @class ATPReqExtQueryETFComponentInfoMsg
         * @brief ETF成分股信息查询请求消息类
         * 
         * 用于查询ETF基金的成分股详细信息，包括成分股代码、权重、现金替代标志等。
         * 这些信息是进行ETF申购赎回操作的重要依据。
         * 
         * 主要功能：
         * - 查询指定ETF的所有成分股信息
         * - 支持按成分股代码过滤查询
         * - 获取现金替代相关参数
         * - 支持分页查询大量成分股数据
         */
        class QUANT_API ATPReqExtQueryETFComponentInfoMsg
        {
        public:
            /**
             * @brief 默认构造函数
             */
            ATPReqExtQueryETFComponentInfoMsg() = default;

            /**
             * @brief 虚析构函数
             */
            virtual ~ATPReqExtQueryETFComponentInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造函数
             */
            ATPReqExtQueryETFComponentInfoMsg(const ATPReqExtQueryETFComponentInfoMsg&) = delete;

            /**
             * @brief 禁用赋值操作符
             */
            ATPReqExtQueryETFComponentInfoMsg& operator=(const ATPReqExtQueryETFComponentInfoMsg&) = delete;
            
            /**
             * @brief 创建ETF成分股查询消息对象（静态工厂方法）
             * @return ATPReqExtQueryETFComponentInfoMsg* 新创建的查询消息对象指针
             */
            static ATPReqExtQueryETFComponentInfoMsg* NewMessage();

            /**
             * @brief 销毁ETF成分股查询消息对象（静态销毁方法）
             * @param msg_ptr 要销毁的查询消息对象指针
             */
            static void DeleteMessage(ATPReqExtQueryETFComponentInfoMsg* msg_ptr);

        public:
            /**
             * @brief 设置市场代码
             * @param market_id 市场代码，0表示查询所有市场
             */
            virtual void SetMarketId(uint16_t market_id) = 0;
            /**
             * @brief 获取市场代码
             * @return uint16_t 市场代码
             */
            virtual uint16_t GetMarketId() const = 0;
            
            /**
             * @brief 设置ETF证券代码
             * @param security_id ETF代码字符串，空字符串表示查询所有ETF
             */
            virtual void SetSecurityId(const char* security_id) = 0;
            /**
             * @brief 获取ETF证券代码
             * @return const char* ETF代码字符串
             */
            virtual const char* GetSecurityId() const = 0;
            
            /**
             * @brief 设置成分股代码过滤条件
             * @param component_security_id 成分股代码，空字符串表示查询所有成分股
             */
            virtual void SetComponentSecurityId(const char* component_security_id) = 0;
            /**
             * @brief 获取成分股代码过滤条件
             * @return const char* 成分股代码字符串
             */
            virtual const char* GetComponentSecurityId() const = 0;
            
            /**
             * @brief 设置查询返回数量限制
             * @param return_num 返回数量，0表示按最大数量返回
             */
            virtual void SetReturnNum(int64_t return_num) = 0;
            /**
             * @brief 获取查询返回数量限制
             * @return int64_t 返回数量限制
             */
            virtual int64_t GetReturnNum() const = 0;
            
            /**
             * @brief 设置申赎类型过滤条件
             * @param pr_type 申赎类型（1-普通申赎，2-实物申赎）
             */
            virtual void SetPrType(uint8_t pr_type) = 0;
            /**
             * @brief 获取申赎类型过滤条件
             * @return uint8_t 申赎类型
             */
            virtual uint8_t GetPrType() const = 0;
        };
        
        /**
         * @class ATPRspExtQueryResultETFComponentInfoMsg
         * @brief ETF成分股信息查询结果消息类
         * 
         * 封装ETF成分股查询的返回结果，包含成分股的详细信息和现金替代参数。
         * 这些信息用于ETF申购赎回时的成分股处理和现金替代计算。
         * 
         * 主要信息：
         * - 成分股基本信息（代码、名称、市场）
         * - 现金替代标志和比例
         * - 成分股在ETF中的权重和数量
         * - 申购赎回时的现金替代金额
         */
        class QUANT_API ATPRspExtQueryResultETFComponentInfoMsg
        {
        public:
            /**
             * @brief 默认构造函数
             */
            ATPRspExtQueryResultETFComponentInfoMsg() = default;

            /**
             * @brief 虚析构函数
             */
            virtual ~ATPRspExtQueryResultETFComponentInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造函数
             */
            ATPRspExtQueryResultETFComponentInfoMsg(const ATPRspExtQueryResultETFComponentInfoMsg&) = delete;

            /**
             * @brief 禁用赋值操作符
             */
            ATPRspExtQueryResultETFComponentInfoMsg& operator=(const ATPRspExtQueryResultETFComponentInfoMsg&) = delete;
            
        public:
            /**
             * @brief 获取ETF所属市场代码
             * @return uint16_t 市场代码
             */
            virtual uint16_t GetMarketId() const = 0;
            
            /**
             * @brief 获取ETF代码
             * @return const char* ETF证券代码字符串
             */
            virtual const char* GetSecurityId() const = 0;
            
            /**
             * @brief 获取ETF证券简称
             * @return const char* ETF证券简称字符串
             */
            virtual const char* GetSecuritySymbol() const = 0;
            
            /**
             * @brief 获取成分股所属市场代码
             * @return uint16_t 成分股市场代码
             */
            virtual uint16_t GetComponentMarketId() const = 0;
            
            /**
             * @brief 获取成分股代码
             * @return const char* 成分股证券代码字符串
             */
            virtual const char* GetComponentSecurityId() const = 0;
            
            /**
             * @brief 获取成分股简称
             * @return const char* 成分股证券简称字符串
             */
            virtual const char* GetComponentSecuritySymbol() const = 0;
            
            /**
             * @brief 获取现金替代标志
             * @return uint8_t 现金替代标志（0-禁止，1-允许，2-必须，3-退补）
             */
            virtual uint8_t GetSubstituteFlag() const = 0;
            
            /**
             * @brief 获取申购溢价比例
             * @return double 申购时的溢价比例，精度N7(5)
             */
            virtual double GetPremiumRatio() const = 0;
            
            /**
             * @brief 获取赎回折价比例
             * @return double 赎回时的折价比例，精度N7(5)
             */
            virtual double GetRedemptionPremiumRatio() const = 0;
            
            /**
             * @brief 获取成分股在ETF中的数量
             * @return double 成分股数量，最小单位度量（股或张）
             */
            virtual double GetComponentShareQty() const = 0;
            
            /**
             * @brief 获取申购现金替代金额
             * @return double 申购时的现金替代金额，精度N18(4)
             */
            virtual double GetPurchaseCashSubstitute() const = 0;
            
            /**
             * @brief 获取赎回现金替代金额
             * @return double 赎回时的现金替代金额，精度N18(4)
             */
            virtual double GetRedemptionCashSubstitute() const = 0;
            
            /**
             * @brief 获取申赎类型
             * @return uint8_t 申赎类型（1-普通申赎，2-实物申赎）
             */
            virtual uint8_t GetPrType() const = 0;
        };
   } //namespace quant_api    // 结束量化API命名空间
} //namespace atp            // 结束ATP命名空间

#endif //  ATP_QUANT_MSG_H_    // 结束头文件保护