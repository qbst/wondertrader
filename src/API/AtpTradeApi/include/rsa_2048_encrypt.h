/**
 * @file rsa_2048_encrypt.h
 * @brief RSA 2048位加密算法接口定义文件
 * 
 * 本文件定义了ATP交易系统中用于密码加密和解密的RSA 2048位加密算法接口。
 * RSA 2048是一种非对称加密算法，具有较高的安全性，适用于敏感信息的加密传输。
 * 
 * 设计逻辑：
 * - 采用RSA 2048位非对称加密算法，确保密码传输的安全性
 * - 使用公钥进行加密，私钥进行解密的标准RSA流程
 * - 支持跨平台使用，兼容Windows和Linux系统
 * - 通过环境变量配置密钥路径，提高系统的灵活性和安全性
 * 
 * 主要功能：
 * 1. 密码加密 - 使用RSA公钥对明文密码进行加密
 * 2. 密码解密 - 使用RSA私钥对密文进行解密还原
 * 
 * 安全要求：
 * - RSA_PUBLIC_KEY_PATH: 环境变量，指定RSA公钥文件路径
 * - RSA_PRIVATE_KEY_PATH: 环境变量，指定RSA私钥文件路径
 * - 密钥文件应妥善保管，避免泄露
 * 
 * 内存管理：
 * - 加密/解密函数内部申请内存存储结果
 * - 调用者负责释放返回的内存指针
 * 
 * @author ATP开发团队
 * @version 1.0
 * @date 2023
 */

#ifndef _RSA_ENCRYPTED_PASSWORD_H_          // 防止头文件重复包含的预处理保护
#define _RSA_ENCRYPTED_PASSWORD_H_          // 定义头文件保护宏

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
	 * @brief RSA 2048位密码加密接口
	 * 
	 * 使用RSA 2048位公钥对输入的明文密码进行加密处理。
	 * 加密过程使用PKCS#1填充方案，确保加密结果的安全性和随机性。
	 * 
	 * @param[in] buf 待加密的明文密码数据指针
	 * @param[in] len 待加密数据的字节长度
	 * @param[out] out_len 加密后数据的字节长度（输出参数）
	 * @param[in] ext_buffer 外部提供的缓冲区（可选，可为NULL）
	 * @param[in] ext_buffer_len 外部缓冲区的大小
	 * @return 加密后的密文数据指针，失败返回NULL
	 * 
	 * @note 重要提醒：
	 * - 函数内部会申请内存保存加密结果，调用者必须负责释放该内存
	 * - 加密前需要设置环境变量RSA_PUBLIC_KEY_PATH，指定RSA公钥文件的完整路径
	 * - 公钥文件格式应符合PEM或DER标准格式
	 * - 加密失败时返回NULL，调用者应检查返回值
	 * 
	 * @example
	 * const char* password = "mypassword";
	 * unsigned int out_len = 0;
	 * void* encrypted = EncryptedPasswordRSA2048(password, strlen(password), &out_len, NULL, 0);
	 * if (encrypted) {
	 *     // 使用加密数据
	 *     free(encrypted);  // 释放内存
	 * }
	 */
	ENCRYPT_API void *EncryptedPasswordRSA2048(const char *buf, unsigned int len, unsigned int *out_len, void *ext_buffer, unsigned int ext_buffer_len);

	/**
	 * @brief RSA 2048位密码解密接口
	 * 
	 * 使用RSA 2048位私钥对输入的密文数据进行解密处理。
	 * 解密过程会还原原始的明文密码数据。
	 * 
	 * @param[in] buf 待解密的密文数据指针
	 * @param[in] len 待解密数据的字节长度
	 * @param[out] out_len 解密后数据的字节长度（输出参数）
	 * @param[in] ext_buffer 外部提供的缓冲区（可选，可为NULL）
	 * @param[in] ext_buffer_len 外部缓冲区的大小
	 * @return 解密后的明文数据指针，失败返回NULL
	 * 
	 * @note 重要提醒：
	 * - 函数内部会申请内存保存解密结果，调用者必须负责释放该内存
	 * - 解密前需要设置环境变量RSA_PRIVATE_KEY_PATH，指定RSA私钥文件的完整路径
	 * - 私钥文件格式应符合PEM或DER标准格式
	 * - 私钥文件应严格保密，避免泄露造成安全风险
	 * - 解密失败时返回NULL，调用者应检查返回值
	 * 
	 * @example
	 * unsigned int out_len = 0;
	 * void* decrypted = DecryptPasswordRSA2048(encrypted_data, encrypted_len, &out_len, NULL, 0);
	 * if (decrypted) {
	 *     // 使用解密数据
	 *     free(decrypted);  // 释放内存
	 * }
	 */
    ENCRYPT_API void *DecryptPasswordRSA2048(const char *buf, unsigned int len, unsigned int *out_len, void *ext_buffer, unsigned int ext_buffer_len);
}

#endif /*_RSA_ENCRYPTED_PASSWORD_H_*/      // 结束头文件保护
