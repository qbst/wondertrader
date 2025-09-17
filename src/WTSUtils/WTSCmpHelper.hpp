/*!
 * \file WTSCmpHelper.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 数据压缩辅助类
 * 
 * 设计逻辑与作用：
 * 这个文件提供了基于Zstandard（zstd）算法的数据压缩和解压缩功能，是WonderTrader
 * 数据存储优化的重要组件。Zstandard是Facebook开发的高性能压缩算法，在压缩率
 * 和速度之间提供了很好的平衡，特别适合实时数据处理场景。
 * 
 * 主要功能特性：
 * 1. 高效压缩：使用zstd算法提供优秀的压缩率和速度
 * 2. 灵活配置：支持不同的压缩级别设置
 * 3. 内存安全：提供完整的错误检查和异常处理
 * 4. 简单接口：封装复杂的zstd API，提供易用的C++接口
 * 5. 自动管理：自动处理内存分配和缓冲区管理
 * 
 * 在量化交易系统中的应用：
 * - 历史数据压缩：减少K线、tick数据的存储空间
 * - 网络传输优化：压缩行情数据以提高传输效率
 * - 日志压缩：压缩系统日志和交易记录
 * - 缓存优化：压缩内存缓存数据以提高容量
 * - 备份存储：压缩备份数据以节省存储成本
 */
#pragma once

#include <string>                              // C++标准字符串类
#include <stdint.h>                            // 标准整数类型定义

#include "../WTSUtils/zstdlib/zstd.h"          // Zstandard压缩库

/**
 * @brief 数据压缩辅助类
 * 
 * 提供基于Zstandard算法的数据压缩和解压缩功能。
 * 所有方法都是静态方法，可以直接通过类名调用。
 * 该类封装了zstd库的复杂API，提供简洁易用的接口。
 */
class WTSCmpHelper
{
public:
	/**
	 * @brief 压缩数据
	 * 
	 * 使用Zstandard算法压缩指定的原始数据。
	 * 支持设置不同的压缩级别以平衡压缩率和速度。
	 * 
	 * @param data 待压缩的原始数据指针
	 * @param dataLen 原始数据的字节长度
	 * @param uLevel 压缩级别，默认为1（1-22，数值越大压缩率越高但速度越慢）
	 * @return std::string 压缩后的数据，以字符串形式返回
	 */
	static std::string compress_data(const void* data, size_t dataLen, uint32_t uLevel = 1)
	{
		std::string desBuf;                       // 目标缓冲区
		std::size_t const desLen = ZSTD_compressBound(dataLen);  // 计算压缩后的最大可能大小
		desBuf.resize(desLen, 0);                 // 预分配足够的缓冲区空间
		// 执行压缩操作
		size_t const cSize = ZSTD_compress((void*)desBuf.data(), desLen, data, dataLen, uLevel);
		desBuf.resize(cSize);                     // 调整到实际压缩后的大小
		return desBuf;                            // 返回压缩后的数据
	}

	/**
	 * @brief 解压缩数据
	 * 
	 * 使用Zstandard算法解压缩之前压缩的数据。
	 * 会自动检测原始数据大小并进行完整性验证。
	 * 
	 * @param data 待解压缩的压缩数据指针
	 * @param dataLen 压缩数据的字节长度
	 * @return std::string 解压缩后的原始数据，以字符串形式返回
	 * @throws std::runtime_error 当解压缩失败或数据不完整时抛出异常
	 */
	static std::string uncompress_data(const void* data, size_t dataLen)
	{
		std::string desBuf;                       // 目标缓冲区
		// 从压缩数据头部获取原始数据大小
		unsigned long long const desLen = ZSTD_getFrameContentSize(data, dataLen);
		desBuf.resize((std::size_t)desLen, 0);    // 根据原始大小预分配缓冲区
		// 执行解压缩操作
		size_t const dSize = ZSTD_decompress((void*)desBuf.data(), (size_t)desLen, data, dataLen);
		// 验证解压缩结果的完整性
		if (dSize != desLen)
			throw std::runtime_error("uncompressed data size does not match calculated data size");
		return desBuf;                            // 返回解压缩后的原始数据
	}
};

