/**
 * @file sha_256_encrypt.h
 * @brief SHA-256哈希算法接口定义文件
 * 
 * 本文件定义了ATP交易系统中用于密码哈希处理的SHA-256算法接口。
 * SHA-256是一种单向哈希函数，属于SHA-2（Secure Hash Algorithm 2）系列，
 * 产生256位（32字节）的哈希值，广泛用于密码存储和数据完整性验证。
 * 
 * 设计逻辑：
 * - 采用SHA-256单向哈希算法，确保密码不可逆转换
 * - 提供统一的密码哈希接口，便于系统集成
 * - 支持跨平台使用，兼容Windows和Linux系统
 * - 遵循标准的SHA-256算法实现，确保兼容性
 * 
 * 主要特性：
 * 1. 单向性 - 从哈希值无法反推原始数据
 * 2. 确定性 - 相同输入总是产生相同哈希值
 * 3. 雪崩效应 - 输入的微小变化导致输出大幅变化
 * 4. 抗碰撞性 - 极难找到产生相同哈希值的不同输入
 * 
 * 应用场景：
 * - 用户密码存储前的哈希处理
 * - 数据完整性校验
 * - 数字签名的消息摘要生成
 * 
 * 安全说明：
 * SHA-256是不可逆的哈希算法，解密函数仅为接口完整性而保留，
 * 实际使用中应返回NULL表示不支持解密操作。
 * 
 * @author ATP开发团队
 * @version 1.0
 * @date 2023
 */

#ifndef _SHA_ENCRYPTED_PASSWORD_H_          // 防止头文件重复包含的预处理保护
#define _SHA_ENCRYPTED_PASSWORD_H_          // 定义头文件保护宏

#define ENCRYPT_API_USE_STATIC              // 定义使用静态链接的加密API

// 根据编译平台定义API导出宏
#if defined _WIN32                          // Windows平台编译配置
#   if    defined ENCRYPT_API_USE_STATIC    // 静态链接模式
#      define  ENCRYPT_API                  // 空定义，静态链接不需要导出声明
#   elif  defined ENCRYPT_API_BUILD_EXPORT // 动态库导出模式
#      define  ENCRYPT_API __declspec(dllexport)  // Windows DLL导出声明
#   else                                    // 动态库导入模式
#      define  ENCRYPT_API __declspec(dllimport)  // Windows DLL导入声明
#   endif
#else                                       // Linux/Unix平台编译配置
#      define  ENCRYPT_API  __attribute__((visibility("default")))  // GCC可见性属性
#endif

extern "C"                                  // 使用C语言链接规范，确保C++兼容性
{
	/**
	 * @brief SHA-256密码哈希加密接口
	 * 
	 * 使用SHA-256算法对输入的明文密码进行单向哈希处理。
	 * 生成的哈希值长度固定为256位（32字节），具有良好的安全性和唯一性。
	 * 
	 * @param[in] buf 待哈希的明文密码数据指针
	 * @param[in] len 待哈希数据的字节长度
	 * @param[out] out_len 哈希后数据的字节长度（输出参数，通常为32字节）
	 * @param[in] ext_buffer 外部提供的缓冲区（可选，可为NULL）
	 * @param[in] ext_buffer_len 外部缓冲区的大小
	 * @return 哈希后的密文数据指针，失败返回NULL
	 * 
	 * @note 重要提醒：
	 * - 函数内部会申请内存保存哈希结果，调用者必须负责释放该内存
	 * - SHA-256产生的哈希值长度固定为32字节（256位）
	 * - 相同的输入总是产生相同的哈希输出
	 * - 哈希过程不可逆，无法从哈希值恢复原始密码
	 * - 建议在密码哈希前添加随机盐值以增强安全性
	 * 
	 * @example
	 * const char* password = "mypassword";
	 * unsigned int out_len = 0;
	 * void* hashed = EncryptedPasswordSha256(password, strlen(password), &out_len, NULL, 0);
	 * if (hashed) {
	 *     // 使用哈希数据（通常为32字节）
	 *     printf("哈希长度: %u字节\n", out_len);
	 *     free(hashed);  // 释放内存
	 * }
	 */
	ENCRYPT_API void* EncryptedPasswordSha256(const char* buf, unsigned int len, unsigned int *out_len,void* ext_buffer,unsigned int ext_buffer_len);

	/**
	 * @brief SHA-256密码解密接口（不支持操作）
	 * 
	 * 由于SHA-256是单向哈希算法，从设计上就不支持解密操作。
	 * 此函数仅为保持接口完整性而存在，实际调用时直接返回NULL。
	 * 
	 * @param[in] buf 待解密的哈希数据指针（无效参数）
	 * @param[in] len 待解密数据的字节长度（无效参数）
	 * @param[out] out_len 解密后数据的字节长度（输出参数，将被设为0）
	 * @param[in] ext_buffer 外部提供的缓冲区（无效参数）
	 * @param[in] ext_buffer_len 外部缓冲区的大小（无效参数）
	 * @return 始终返回NULL，表示不支持解密操作
	 * 
	 * @note 重要说明：
	 * - SHA-256属于不可逆的哈希算法，从理论上无法进行解密
	 * - 此函数的存在仅为保持API接口的一致性
	 * - 实际应用中应通过比较哈希值来验证密码正确性
	 * - 如需密码验证，应将用户输入的密码进行同样的哈希处理后与存储的哈希值比较
	 * 
	 * @warning 
	 * 调用此函数将始终返回NULL，不会执行任何实际的解密操作。
	 * 请勿依赖此函数进行密码恢复或解密操作。
	 */
    ENCRYPT_API void *DecryptPasswordSha256(const char *buf, unsigned int len, unsigned int *out_len, void *ext_buffer, unsigned int ext_buffer_len);
}

#endif /*_SHA_ENCRYPTED_PASSWORD_H_*/      // 结束头文件保护
