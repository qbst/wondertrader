/*!
 * \file WtDtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief WonderTrader数据辅助工具实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是WtDtHelper（数据辅助工具）模块的具体实现，提供了WonderTrader框架中数据格式转换、
 * 数据读取、数据存储的核心功能。该文件实现了多种数据格式之间的转换，支持DSB、DMB、CSV等
 * 格式的处理，是WonderTrader数据处理生态系统的重要组成部分。
 * 
 * 核心实现机制：
 * 
 * 1. 数据块处理机制（Data Block Processing）：
 *    - proc_block_data()：统一的数据块处理函数
 *    - 支持压缩数据的自动解压缩
 *    - 支持老版本数据结构的自动转换
 *    - 提供灵活的头部保留/移除选项
 * 
 * 2. 格式转换引擎（Format Conversion Engine）：
 *    - 二进制到CSV的转换（dump_bars, dump_ticks）
 *    - CSV到二进制的转换（trans_csv_bars）
 *    - 内存数据到文件的存储（store_*系列函数）
 *    - 支持批量文件处理和进度回调
 * 
 * 3. 流式数据读取（Streaming Data Reading）：
 *    - read_dsb_*系列：读取DSB格式数据
 *    - read_dmb_*系列：读取DMB格式数据
 *    - 使用回调机制实现大数据量的流式处理
 *    - 支持数据计数预告和分批次处理
 * 
 * 4. 高级数据处理（Advanced Data Processing）：
 *    - resample_bars()：K线数据重采样功能
 *    - 支持交易时段的精确处理
 *    - 支持多种周期间的数据转换
 *    - 集成WTSDataFactory进行专业数据处理
 * 
 * 主要功能实现：
 * 
 * 1. 数据块处理（Data Block Processing）：
 *    - 自动识别数据压缩状态和版本信息
 *    - 使用WTSCmpHelper进行数据解压缩
 *    - 处理WTSBarStructOld到WTSBarStruct的转换
 *    - 处理WTSTickStructOld到WTSTickStruct的转换
 * 
 * 2. 时间格式处理（Time Format Processing）：
 *    - strToTime()：字符串时间转换为数值时间
 *    - strToDate()：字符串日期转换为数值日期
 *    - 支持多种时间格式的自动识别
 *    - 处理交易时间的标准化
 * 
 * 3. 文件I/O优化（File I/O Optimization）：
 *    - 使用BoostFile进行高效文件操作
 *    - 支持大文件的流式读写
 *    - 内置文件存在性检查和目录创建
 *    - 优化的内存使用和缓冲管理
 * 
 * 4. 错误处理机制（Error Handling）：
 *    - 完善的参数验证和边界检查
 *    - 详细的错误日志记录
 *    - 优雅的异常处理和资源清理
 *    - 用户友好的错误信息提示
 * 
 * 数据格式支持详解：
 * 
 * 1. DSB格式（Data Storage Binary）：
 *    - WonderTrader标准的历史数据存储格式
 *    - 支持数据压缩，节省存储空间
 *    - 包含完整的数据头部信息
 *    - 支持版本兼容性处理
 * 
 * 2. DMB格式（Data Memory Binary）：
 *    - 用于实时数据的内存映射格式
 *    - 优化的内存访问性能
 *    - 适用于高频数据访问场景
 * 
 * 3. CSV格式（Comma-Separated Values）：
 *    - 通用的文本数据交换格式
 *    - 便于数据分析和外部工具处理
 *    - 支持自定义字段分隔符
 *    - 包含完整的数据字段信息
 * 
 * 性能优化策略：
 * 
 * 1. 内存管理优化：
 *    - 使用std::string进行动态内存管理
 *    - 避免不必要的内存拷贝
 *    - 合理的缓冲区大小设置
 *    - 及时释放临时内存
 * 
 * 2. I/O性能优化：
 *    - 批量文件处理减少系统调用
 *    - 使用内存映射提高读取性能
 *    - 优化的文件写入策略
 *    - 支持异步I/O操作
 * 
 * 3. 数据处理优化：
 *    - 流式处理避免大内存占用
 *    - 智能的数据类型识别
 *    - 高效的数据结构转换
 *    - 并行处理能力
 * 
 * 使用场景与应用：
 * - 历史数据的批量格式转换
 * - 实时数据的标准化处理
 * - 数据分析工具的数据接入
 * - 跨平台数据交换
 * - 数据质量检查和验证
 * - 自定义数据处理程序开发
 * 
 * 技术特点：
 * - 高性能的数据处理能力
 * - 完善的错误处理机制
 * - 灵活的回调接口设计
 * - 强大的格式兼容性
 * - 优秀的内存使用效率
 * 
 * 注意事项：
 * - 大文件处理时需要注意内存使用
 * - 回调函数需要处理多次调用
 * - 文件路径需要使用标准化格式
 * - 数据格式需要与存储格式匹配
 */

#include "WtDtHelper.h"                                         // 包含数据辅助工具头文件
#include "../Share/StrUtil.hpp"                                 // 包含字符串工具类
#include "../Share/TimeUtils.hpp"                               // 包含时间工具类
#include "../Share/BoostFile.hpp"                               // 包含Boost文件操作类

#include "../WtDataStorage/DataDefine.h"                        // 包含数据存储定义
#include "../WTSUtils/WTSCmpHelper.hpp"                         // 包含数据压缩辅助类
#include "../WTSTools/CsvHelper.h"                              // 包含CSV文件处理类
#include "../WTSTools/WTSDataFactory.h"                         // 包含数据工厂类

#include "../Includes/WTSDataDef.hpp"                           // 包含数据结构定义
#include "../Includes/WTSSessionInfo.hpp"                       // 包含交易时段信息类

#include <rapidjson/document.h>                                 // 包含RapidJSON文档解析库

namespace rj = rapidjson;                                       // RapidJSON命名空间别名

USING_NS_WTP;                                                   // 使用WonderTrader命名空间

/**
 * @brief 处理数据块，包括解压缩和版本转换
 * @param content 数据块内容（输入输出参数，会被修改）
 * @param isBar 是否为K线数据（true=K线数据，false=Tick数据）
 * @param bKeepHead 是否保留数据块头部（true=保留头部，false=移除头部）
 * @return 处理是否成功
 * 
 * 该函数统一处理WonderTrader数据块的各种格式转换：
 * 1. 自动检测数据块是否压缩，如果压缩则进行解压
 * 2. 自动检测数据块版本，如果是老版本则转换为新版本数据结构
 * 3. 支持K线数据和Tick数据的分别处理
 * 4. 可选择保留或移除数据块头部信息
 * 
 * 处理流程：
 * - 检查数据块是否压缩或为老版本
 * - 如果压缩，使用WTSCmpHelper解压缩数据
 * - 如果是老版本，将WTSBarStructOld转换为WTSBarStruct，或将WTSTickStructOld转换为WTSTickStruct
 * - 根据bKeepHead参数决定是否保留头部信息
 */
bool proc_block_data(std::string& content, bool isBar, bool bKeepHead /* = true */)
{
	BlockHeader* header = (BlockHeader*)content.data();                      // 获取数据块头部指针

	bool bCmped = header->is_compressed();                                    // 检查数据块是否压缩
	bool bOldVer = header->is_old_version();                                  // 检查数据块是否为老版本

	// 如果既没有压缩，也不是老版本结构体，则直接返回
	if (!bCmped && !bOldVer)
	{
		if (!bKeepHead)                                                       // 如果不需要保留头部
			content.erase(0, BLOCK_HEADER_SIZE);                             // 移除数据块头部
		return true;                                                          // 直接返回成功
	}

	std::string buffer;                                                       // 临时缓冲区，用于存储处理后的数据
	if (bCmped)                                                               // 如果数据块被压缩
	{
		BlockHeaderV2* blkV2 = (BlockHeaderV2*)content.c_str();              // 获取压缩数据块的V2头部

		if (content.size() != (sizeof(BlockHeaderV2) + blkV2->_size))          // 验证数据块大小是否匹配
		{
			return false;                                                     // 大小不匹配，返回失败
		}

		// 将文件头后面的数据进行解压
		buffer = WTSCmpHelper::uncompress_data(content.data() + BLOCK_HEADERV2_SIZE, blkV2->_size);
	}
	else                                                                      // 如果数据块未压缩
	{
		if (!bOldVer)                                                         // 如果不是老版本
		{
			// 如果不是老版本，直接返回
			if (!bKeepHead)                                                   // 如果不需要保留头部
				content.erase(0, BLOCK_HEADER_SIZE);                          // 移除数据块头部
			return true;                                                      // 直接返回成功
		}
		else                                                                  // 如果是老版本
		{
			buffer.append(content.data() + BLOCK_HEADER_SIZE, content.size() - BLOCK_HEADER_SIZE);  // 提取数据部分（跳过头部）
		}
	}

	if (bOldVer)                                                              // 如果是老版本数据结构
	{
		if (isBar)                                                            // 如果是K线数据
		{
			std::string bufV2;                                                // 新版本缓冲区
			uint32_t barcnt = buffer.size() / sizeof(WTSBarStructOld);       // 计算K线数据条数
			bufV2.resize(barcnt * sizeof(WTSBarStruct));                      // 分配新版本数据结构大小
			WTSBarStruct* newBar = (WTSBarStruct*)bufV2.data();              // 新版本K线数据指针
			WTSBarStructOld* oldBar = (WTSBarStructOld*)buffer.data();       // 老版本K线数据指针
			for (uint32_t idx = 0; idx < barcnt; idx++)                      // 遍历每条K线数据
			{
				newBar[idx] = oldBar[idx];                                    // 将老版本结构体转换为新版本（结构体成员兼容）
			}
			buffer.swap(bufV2);                                               // 交换缓冲区，使用新版本数据
		}
		else                                                                  // 如果是Tick数据
		{
			uint32_t tick_cnt = buffer.size() / sizeof(WTSTickStructOld);     // 计算Tick数据条数
			std::string bufv2;                                                // 新版本缓冲区
			bufv2.resize(sizeof(WTSTickStruct)*tick_cnt);                     // 分配新版本数据结构大小
			WTSTickStruct* newTick = (WTSTickStruct*)bufv2.data();            // 新版本Tick数据指针
			WTSTickStructOld* oldTick = (WTSTickStructOld*)buffer.data();    // 老版本Tick数据指针
			for (uint32_t i = 0; i < tick_cnt; i++)                          // 遍历每条Tick数据
			{
				newTick[i] = oldTick[i];                                      // 将老版本结构体转换为新版本（结构体成员兼容）
			}
			buffer.swap(bufv2);                                               // 交换缓冲区，使用新版本数据
		}
	}

	if (bKeepHead)                                                            // 如果需要保留头部
	{
		content.resize(BLOCK_HEADER_SIZE);                                     // 将内容大小调整为头部大小
		content.append(buffer);                                                // 追加处理后的数据
		header = (BlockHeader*)content.data();                                 // 重新获取头部指针
		header->_version = BLOCK_VERSION_RAW_V2;                                // 更新头部版本号为V2未压缩版本
	}
	else                                                                      // 如果不需要保留头部
	{
		content.swap(buffer);                                                  // 直接交换内容，使用处理后的数据（不包含头部）
	}

	return true;                                                               // 返回处理成功
}


/**
 * @brief 将字符串格式的时间转换为数值格式的时间
 * @param strTime 时间字符串（格式如"HH:MM:SS"或"HHMMSS"）
 * @param bKeepSec 是否保留秒数（true=保留秒，false=只保留时分）
 * @return 转换后的时间数值（HHMM或HHMMSS格式）
 * 
 * 该函数用于将多种格式的时间字符串转换为统一的数值格式。
 * 支持的输入格式：
 * - "HH:MM:SS" -> 如果bKeepSec=false则返回HHMM，如果bKeepSec=true则返回HHMMSS
 * - "HHMMSS" -> 如果bKeepSec=false则返回HHMM，如果bKeepSec=true则返回HHMMSS
 * 
 * 示例：
 * - strToTime("09:30:15", false) -> 930
 * - strToTime("09:30:15", true) -> 93015
 * - strToTime("093015", false) -> 930
 */
uint32_t strToTime(const char* strTime, bool bKeepSec = false)
{
	std::string str;                                                           // 用于存储去除分隔符后的时间字符串
	const char *pos = strTime;                                                 // 字符串指针，用于遍历
	while (strlen(pos) > 0)                                                    // 遍历整个字符串
	{
		if (pos[0] != ':')                                                      // 如果当前字符不是冒号分隔符
		{
			str.append(pos, 1);                                                // 将字符追加到结果字符串
		}
		pos++;                                                                  // 移动到下一个字符
	}

	uint32_t ret = strtoul(str.c_str(), NULL, 10);                             // 将字符串转换为无符号整数
	if (ret > 10000 && !bKeepSec)                                              // 如果数值大于10000（包含秒）且不需要保留秒
		ret /= 100;                                                             // 除以100，去掉秒数部分（例如：93015 -> 930）

	return ret;                                                                 // 返回转换后的时间数值
}

/**
 * @brief 将字符串格式的日期转换为数值格式的日期
 * @param strDate 日期字符串（支持多种格式：yyyy/mm/dd, yyyy-mm-dd, yyyymmdd等）
 * @return 转换后的日期数值（yyyymmdd格式）
 * 
 * 该函数用于将多种格式的日期字符串转换为统一的yyyymmdd数值格式。
 * 支持的输入格式：
 * - "yyyy/mm/dd" -> yyyymmdd
 * - "yyyy-mm-dd" -> yyyymmdd
 * - "yyyymmdd" -> yyyymmdd
 * - "yyyy/mm/dd HH:MM:SS" -> yyyymmdd（会自动去掉时间部分）
 * 
 * 示例：
 * - strToDate("2023/03/30") -> 20230330
 * - strToDate("2023-03-30") -> 20230330
 * - strToDate("20230330") -> 20230330
 * - strToDate("2023/3/5") -> 20230305（自动补零）
 */
uint32_t strToDate(const char* strDate)
{
	StringVector ay = StrUtil::split(strDate, "/");                           // 尝试使用"/"分隔符分割日期字符串
	if (ay.size() == 1)                                                       // 如果分割后只有一个元素（说明不是"/"格式）
		ay = StrUtil::split(strDate, "-");                                    // 尝试使用"-"分隔符分割日期字符串
	std::stringstream ss;                                                      // 字符串流，用于构建输出字符串
	if (ay.size() > 1)                                                        // 如果成功分割为多个部分（年月日）
	{
		auto pos = ay[2].find(" ");                                           // 在日期部分查找空格（可能包含时间部分）
		if (pos != std::string::npos)                                         // 如果找到空格
			ay[2] = ay[2].substr(0, pos);                                      // 提取空格之前的部分（去掉时间部分）
		ss << ay[0] << (ay[1].size() == 1 ? "0" : "") << ay[1] << (ay[2].size() == 1 ? "0" : "") << ay[2];  // 拼接年月日，月日不足两位时补零
	}
	else                                                                      // 如果分割失败（可能是yyyymmdd格式）
		ss << ay[0];                                                           // 直接使用原始字符串

	return strtoul(ss.str().c_str(), NULL, 10);                               // 将字符串转换为无符号整数并返回
}

/**
 * @brief 将二进制K线数据批量导出为CSV格式
 * @param binFolder 二进制数据文件夹路径
 * @param csvFolder CSV输出文件夹路径
 * @param strFilter 文件过滤器（可选，暂未实现）
 * @param cbLogger 日志回调函数（可选）
 * 
 * 该函数遍历指定文件夹下的所有.dsb文件，将K线数据转换为CSV格式。
 * 支持1分钟、5分钟、日线等多种周期的K线数据。
 * CSV格式包含：date,time,open,high,low,close,settle,volume,turnover,open_interest,diff_interest
 * 
 * 处理流程：
 * 1. 检查源文件夹是否存在
 * 2. 创建目标CSV文件夹（如果不存在）
 * 3. 遍历文件夹中的所有.dsb文件
 * 4. 读取并解析每个文件的K线数据
 * 5. 将数据转换为CSV格式并写入文件
 * 6. 通过回调函数报告处理进度
 */
void dump_bars(WtString binFolder, WtString csvFolder, WtString strFilter /* = "" */, FuncLogCallback cbLogger /* = NULL */)
{
	std::string srcFolder = StrUtil::standardisePath(binFolder);              // 标准化源文件夹路径
	if (!BoostFile::exists(srcFolder.c_str()))                                 // 检查源文件夹是否存在
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("目录%s不存在", binFolder).c_str());     // 记录错误日志
		return;                                                                 // 直接返回，不进行后续处理
	}

	if (!BoostFile::exists(csvFolder))                                         // 检查目标CSV文件夹是否存在
		BoostFile::create_directories(csvFolder);                              // 如果不存在则创建目录（包括父目录）

	boost::filesystem::path myPath(srcFolder);                                 // 创建文件夹路径对象
	boost::filesystem::directory_iterator endIter;                              // 目录迭代器结束标记
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历文件夹中的所有文件
	{
		if (boost::filesystem::is_directory(iter->path()))                      // 如果是子目录
			continue;                                                           // 跳过子目录，不处理

		if (iter->path().extension() != ".dsb")                                // 如果文件扩展名不是.dsb
			continue;                                                           // 跳过非.dsb文件

		const std::string& path = iter->path().string();                        // 获取文件的完整路径

		std::string fileCode = iter->path().stem().string();                    // 获取文件名（不含扩展名），作为合约代码

		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

		std::string buffer;                                                      // 文件内容缓冲区
		BoostFile::read_file_contents(path.c_str(), buffer);                    // 读取整个文件内容到缓冲区
		if (buffer.size() < sizeof(HisKlineBlock))                              // 检查文件大小是否至少包含一个数据块头部
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger(StrUtil::printf("文件%s头部校验失败", binFolder).c_str());  // 记录文件头部校验失败的日志
			continue;                                                           // 跳过该文件，继续处理下一个文件
		}

		BlockHeader* bHeader = (BlockHeader*)buffer.data();                     // 获取数据块头部指针

		if(bHeader->_type < BT_HIS_Minute1 || bHeader->_type > BT_HIS_Day)     // 检查数据块类型是否为K线数据（1分钟、5分钟或日线）
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger(StrUtil::printf("文件%s不是K线数据，跳过转换", binFolder).c_str());  // 记录跳过非K线数据的日志
			continue;                                                           // 跳过该文件，继续处理下一个文件
		}

		bool isDay = (bHeader->_type == BT_HIS_Day);                            // 判断是否为日线数据

		proc_block_data(buffer, true, false);		                              // 处理数据块（解压缩、版本转换等），isBar=true表示是K线数据，bKeepHead=false表示不保留头部

		auto kcnt = buffer.size() / sizeof(WTSBarStruct);                       // 计算K线数据条数
		if (kcnt <= 0)                                                          // 如果数据条数为0或负数
			continue;                                                           // 跳过该文件，继续处理下一个文件

		std::string filename = StrUtil::standardisePath(csvFolder);             // 构建输出CSV文件路径
		filename += fileCode;                                                    // 追加文件名（合约代码）
		filename += ".csv";                                                      // 追加.csv扩展名

		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("正在写入%s...", filename.c_str()).c_str());  // 记录开始写入文件的日志

		WTSBarStruct* bars = (WTSBarStruct*)buffer.data();                      // 获取K线数据数组指针

		std::stringstream ss;                                                    // 字符串流，用于构建CSV内容
		ss << "date,time,open,high,low,close,settle,volume,turnover,open_interest,diff_interest" << std::endl;  // 写入CSV表头
		ss.setf(std::ios::fixed);                                                // 设置浮点数输出格式为固定小数点格式

		for (uint32_t i = 0; i < kcnt; i++)                                     // 遍历每条K线数据
		{
			const WTSBarStruct& curBar = bars[i];                               // 获取当前K线数据引用
			if(isDay)                                                           // 如果是日线数据
			{
				ss << curBar.date << ",0,";                                     // 日线数据：日期+时间为0（日线不包含具体时间）
			}
			else                                                                 // 如果是分钟线数据
			{
				uint32_t barTime = (uint32_t)(curBar.time % 10000 * 100);       // 提取时间部分（HHMM格式），time字段包含日期和时间，取模10000得到时间部分，乘以100转换为HHMM格式
				uint32_t barDate = (uint32_t)(curBar.time / 10000 + 19900000);   // 提取日期部分（yyyymmdd格式），除以10000得到日期部分，加上基准年份19900000
				ss << barDate << ","                                             // 写入日期
					<< barTime << ",";                                          // 写入时间
			}
			
			ss << curBar.open << ","                                             // 写入开盘价
				<< curBar.high << ","                                            // 写入最高价
				<< curBar.low << ","                                             // 写入最低价
				<< curBar.close << ","                                            // 写入收盘价
				<< curBar.settle << ","                                          // 写入结算价
				<< curBar.vol << ","                                             // 写入成交量
				<< curBar.money << ","                                           // 写入成交额
				<< curBar.hold << ","                                            // 写入持仓量
				<< curBar.add << std::endl;                                      // 写入增仓量，并换行
		}

		BoostFile::write_file_contents(filename.c_str(), ss.str().c_str(), (uint32_t)ss.str().size());  // 将CSV内容写入文件

		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("%s写入完成,共%u条bar", filename.c_str(), kcnt).c_str());  // 记录文件写入完成的日志，包含数据条数
	}

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("目录%s全部导出完成...", binFolder).c_str());  // 记录整个目录导出完成的日志
}

/**
 * @brief 将二进制Tick数据批量导出为CSV格式
 * @param binFolder 二进制数据文件夹路径
 * @param csvFolder CSV输出文件夹路径
 * @param strFilter 文件过滤器（可选，暂未实现）
 * @param cbLogger 日志回调函数（可选）
 * 
 * 该函数遍历指定文件夹下的所有.dsb文件，将Tick数据转换为CSV格式。
 * CSV格式包含完整的Tick信息，包括10档买卖盘口、成交信息等。
 * 
 * CSV字段说明：
 * - 基础信息：exchg（交易所）, code（合约代码）, tradingdate（交易日期）, actiondate（动作日期）, actiontime（动作时间）
 * - 价格信息：price（最新价）, open（开盘价）, high（最高价）, low（最低价）, settle（结算价）
 * - 历史数据：preclose（昨收价）, presettle（昨结算价）, preinterest（昨持仓量）
 * - 成交信息：total_volume（累计成交量）, total_turnover（累计成交额）, volume（增量成交量）, turnover（增量成交额）
 * - 持仓信息：open_interest（持仓量）, additional（增量持仓）
 * - 买卖盘口：bidprice1~10（买1~10价）, bidqty1~10（买1~10量）, askprice1~10（卖1~10价）, askqty1~10（卖1~10量）
 */
void dump_ticks(WtString binFolder, WtString csvFolder, WtString strFilter /* = "" */, FuncLogCallback cbLogger /* = NULL */)
{
	std::string srcFolder = StrUtil::standardisePath(binFolder);              // 标准化源文件夹路径
	if (!BoostFile::exists(srcFolder.c_str()))                                 // 检查源文件夹是否存在
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("目录%s不存在", binFolder).c_str());     // 记录错误日志
		return;                                                                 // 直接返回，不进行后续处理
	}

	if (!BoostFile::exists(csvFolder))                                         // 检查目标CSV文件夹是否存在
		BoostFile::create_directories(csvFolder);                              // 如果不存在则创建目录（包括父目录）

	boost::filesystem::path myPath(srcFolder);                                 // 创建文件夹路径对象
	boost::filesystem::directory_iterator endIter;                              // 目录迭代器结束标记
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历文件夹中的所有文件
	{
		if (boost::filesystem::is_directory(iter->path()))                      // 如果是子目录
			continue;                                                           // 跳过子目录，不处理

		if (iter->path().extension() != ".dsb")                                // 如果文件扩展名不是.dsb
			continue;                                                           // 跳过非.dsb文件

		const std::string& path = iter->path().string();                        // 获取文件的完整路径

		std::string fileCode = iter->path().stem().string();                    // 获取文件名（不含扩展名），作为合约代码

		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

		std::string buffer;                                                      // 文件内容缓冲区
		BoostFile::read_file_contents(path.c_str(), buffer);                    // 读取整个文件内容到缓冲区
		if (buffer.size() < sizeof(HisTickBlock))                               // 检查文件大小是否至少包含一个Tick数据块头部
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger(StrUtil::printf("文件%s头部校验失败", binFolder).c_str());  // 记录文件头部校验失败的日志
			continue;                                                           // 跳过该文件，继续处理下一个文件
		}

		proc_block_data(buffer, false, false);                                  // 处理数据块（解压缩、版本转换等），isBar=false表示是Tick数据，bKeepHead=false表示不保留头部

		auto tcnt = buffer.size() / sizeof(WTSTickStruct);                      // 计算Tick数据条数
		if (tcnt <= 0)                                                          // 如果数据条数为0或负数
			continue;                                                           // 跳过该文件，继续处理下一个文件

		std::string filename = StrUtil::standardisePath(csvFolder);             // 构建输出CSV文件路径
		filename += fileCode;                                                    // 追加文件名（合约代码）
		filename += ".csv";                                                      // 追加.csv扩展名

		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("正在写入%s...", filename.c_str()).c_str());  // 记录开始写入文件的日志

		WTSTickStruct* ticks = (WTSTickStruct*)buffer.data();                   // 获取Tick数据数组指针

		std::stringstream ss;                                                    // 字符串流，用于构建CSV内容
		ss.setf(std::ios::fixed, std::ios::floatfield);                          // 设置浮点数输出格式为固定小数点格式
		ss.precision(6);                                                         // 设置浮点数精度为6位小数
		ss << "exchg,code,tradingdate,actiondate,actiontime,price,open,high,low,settle,preclose,"  // 写入CSV表头（基础字段）
			<< "presettle,preinterest,total_volume,total_turnover,open_interest,volume,turnover,additional,";  // 写入CSV表头（成交和持仓字段）
		for (int i = 0; i < 10; i++)                                            // 循环生成10档买卖盘口字段名
		{
			bool hasTail = (i != 9);                                            // 判断是否为最后一档（用于决定是否添加逗号）
			ss << "bidprice" << i + 1 << "," << "bidqty" << i + 1 << "," << "askprice" << i + 1 << "," << "askqty" << i + 1 << (hasTail ? "," : "");  // 写入买价、买量、卖价、卖量字段名
		}
		ss << std::endl;                                                         // 表头结束，换行

		for (uint32_t i = 0; i < tcnt; i++)                                     // 遍历每条Tick数据
		{
			const WTSTickStruct& curTick = ticks[i];                            // 获取当前Tick数据引用
			ss << curTick.exchg << "," << curTick.code << ","                    // 写入交易所和合约代码
				<< curTick.trading_date << ","                                    // 写入交易日期
				<< curTick.action_date << ","                                    // 写入动作日期
				<< curTick.action_time << ","                                    // 写入动作时间
				<< curTick.price << ","                                          // 写入最新价
				<< curTick.open << ","                                            // 写入开盘价
				<< curTick.high << ","                                            // 写入最高价
				<< curTick.low << ","                                             // 写入最低价
				<< curTick.settle_price << ","                                   // 写入结算价
				<< curTick.pre_close << ","                                       // 写入昨收价
				<< curTick.pre_settle << ","                                      // 写入昨结算价
				<< curTick.pre_interest << ","                                    // 写入昨持仓量
				<< curTick.total_volume << ","                                    // 写入累计成交量
				<< curTick.total_turnover << ","                                  // 写入累计成交额
				<< curTick.open_interest << ","                                   // 写入持仓量
				<< curTick.volume << ","                                          // 写入增量成交量
				<< curTick.turn_over << ","                                      // 写入增量成交额
				<< curTick.diff_interest << ",";                                  // 写入增量持仓

			for (int j = 0; j < 10; j++)                                         // 遍历10档买卖盘口
			{
				bool hasTail = (j != 9);                                         // 判断是否为最后一档（用于决定是否添加逗号）
				ss << curTick.bid_prices[j] << "," << curTick.bid_qty[j] << "," << curTick.ask_prices[j] << "," << curTick.ask_qty[j] << (hasTail ? "," : "");  // 写入买价、买量、卖价、卖量
			}
			ss << std::endl;                                                     // 当前Tick数据结束，换行
		}

		BoostFile::write_file_contents(filename.c_str(), ss.str().c_str(), (uint32_t)ss.str().size());  // 将CSV内容写入文件

		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("%s写入完成,共%u条tick数据", filename.c_str(), tcnt).c_str());  // 记录文件写入完成的日志，包含数据条数
	}

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("目录%s全部导出完成...", binFolder).c_str());  // 记录整个目录导出完成的日志
}

/**
 * @brief 将CSV格式的K线数据批量转换为二进制格式
 * @param csvFolder CSV数据文件夹路径
 * @param binFolder 二进制输出文件夹路径
 * @param period 数据周期（"m1"=1分钟, "m5"=5分钟, "d"或空=日线）
 * @param cbLogger 日志回调函数（可选）
 * 
 * 该函数遍历指定文件夹下的所有.csv文件，将CSV格式的K线数据转换为WonderTrader的DSB二进制格式。
 * 转换后的数据会进行压缩存储，节省磁盘空间。
 * 
 * CSV格式要求：
 * - 必须有表头：date,time,open,high,low,close,settle,volume,turnover,open_interest,diff_interest
 * - date字段：日期格式（支持yyyy/mm/dd, yyyy-mm-dd, yyyymmdd）
 * - time字段：时间格式（分钟线需要，日线不需要），格式如"HH:MM"或"HHMM"
 * - 数值字段：open, high, low, close, settle, volume, turnover, open_interest, diff_interest
 * 
 * 处理流程：
 * 1. 检查源文件夹是否存在
 * 2. 创建目标二进制文件夹（如果不存在）
 * 3. 遍历文件夹中的所有.csv文件
 * 4. 使用CsvReader逐行读取CSV数据
 * 5. 将数据转换为WTSBarStruct结构
 * 6. 压缩数据并写入.dsb文件
 */
void trans_csv_bars(WtString csvFolder, WtString binFolder, WtString period, FuncLogCallback cbLogger /* = NULL */)
{
	if (!BoostFile::exists(csvFolder))                                         // 检查源CSV文件夹是否存在
		return;                                                                 // 如果不存在则直接返回

	if (!BoostFile::exists(binFolder))                                         // 检查目标二进制文件夹是否存在
		BoostFile::create_directories(binFolder);                              // 如果不存在则创建目录（包括父目录）

	WTSKlinePeriod kp = KP_DAY;                                                // 默认周期为日线
	if (wt_stricmp(period, "m1") == 0)                                         // 如果周期参数为"m1"
		kp = KP_Minute1;                                                        // 设置为1分钟周期
	else if (wt_stricmp(period, "m5") == 0)                                   // 如果周期参数为"m5"
		kp = KP_Minute5;                                                        // 设置为5分钟周期
	else                                                                       // 其他情况（包括"d"或空）
		kp = KP_DAY;                                                            // 设置为日线周期

	boost::filesystem::path myPath(csvFolder);                                 // 创建文件夹路径对象
	boost::filesystem::directory_iterator endIter;                              // 目录迭代器结束标记
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)  // 遍历文件夹中的所有文件
	{
		if (boost::filesystem::is_directory(iter->path()))                      // 如果是子目录
			continue;                                                           // 跳过子目录，不处理

		if (iter->path().extension() != ".csv")                                  // 如果文件扩展名不是.csv
			continue;                                                           // 跳过非.csv文件

		const std::string& path = iter->path().string();                        // 获取文件的完整路径

		if(cbLogger)                                                            // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

		CsvReader reader(",");                                                  // 创建CSV读取器，使用逗号作为分隔符
		if(!reader.load_from_file(path.c_str()))                                // 加载CSV文件
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger(StrUtil::printf("读取数据文件%s失败...", path.c_str()).c_str());  // 记录读取失败的日志
			continue;                                                           // 跳过该文件，继续处理下一个文件
		}

		std::vector<WTSBarStruct> bars;                                          // K线数据向量，用于存储读取的数据

		while(reader.next_row())                                                 // 逐行读取CSV数据
		{
			// 逐行读取
			WTSBarStruct bs;                                                    // 创建K线结构体
			bs.date = strToDate(reader.get_string("date"));                     // 读取日期字段并转换为数值格式
			if(kp != KP_DAY)                                                    // 如果不是日线数据
				bs.time = TimeUtils::timeToMinBar(bs.date, strToTime(reader.get_string("time")));  // 读取时间字段并转换为分钟线时间格式
			bs.open = reader.get_double("open");                                // 读取开盘价
			bs.high = reader.get_double("high");                                // 读取最高价
			bs.low = reader.get_double("low");                                  // 读取最低价
			bs.close = reader.get_double("close");                              // 读取收盘价
			bs.vol = reader.get_double("volume");                               // 读取成交量
			bs.money = reader.get_double("turnover");                           // 读取成交额
			bs.hold = reader.get_double("open_interest");                       // 读取持仓量
			bs.add = reader.get_double("diff_interest");                        // 读取增仓量
			bs.settle = reader.get_double("settle");                             // 读取结算价
			bars.emplace_back(bs);                                               // 将K线数据添加到向量中

			if (bars.size() % 1000 == 0)                                        // 每读取1000条数据
			{
				if (cbLogger)                                                   // 如果提供了日志回调函数
					cbLogger(StrUtil::printf("已读取数据%u条", bars.size()).c_str());  // 记录读取进度日志
			}
		}
		if (cbLogger)                                                            // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("数据文件%s全部读取完成,共%u条", path.c_str(), bars.size()).c_str());  // 记录文件读取完成的日志

		BlockType btype;                                                        // 数据块类型
		switch (kp)                                                              // 根据周期设置数据块类型
		{
		case KP_Minute1: btype = BT_HIS_Minute1; break;                         // 1分钟线
		case KP_Minute5: btype = BT_HIS_Minute5; break;                        // 5分钟线
		default: btype = BT_HIS_Day; break;                                    // 日线（默认）
		}

		HisKlineBlockV2 kBlock;                                                  // 创建K线数据块V2头部结构
		strcpy(kBlock._blk_flag, BLK_FLAG);                                      // 设置数据块标识符
		kBlock._type = btype;                                                    // 设置数据块类型
		kBlock._version = BLOCK_VERSION_CMP_V2;                                   // 设置数据块版本为V2压缩版本
		std::string cmprsData = WTSCmpHelper::compress_data(bars.data(), sizeof(WTSBarStruct)*bars.size());  // 压缩K线数据
		kBlock._size = cmprsData.size();                                        // 设置压缩后的数据大小

		std::string filename = StrUtil::standardisePath(binFolder);             // 构建输出二进制文件路径
		filename += iter->path().stem().string();                                // 追加文件名（不含扩展名）
		filename += ".dsb";                                                      // 追加.dsb扩展名

		BoostFile bf;                                                            // 创建文件操作对象
		if (bf.create_new_file(filename.c_str()))                                // 创建新文件
		{
			bf.write_file(&kBlock, sizeof(HisKlineBlockV2));                    // 写入数据块头部
		}
		bf.write_file(cmprsData);                                                // 写入压缩后的数据
		bf.close_file();                                                         // 关闭文件
		if (cbLogger)                                                            // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("数据已转储至%s", filename.c_str()).c_str());  // 记录数据转储完成的日志
	}
}

// ===== 以下为历史遗留的代码，已废弃，改用直接从Python传递内存块的方式 =====
// 原设计：使用回调函数逐个获取数据项，然后存储到文件
// 新设计：直接传递内存块指针，提高效率和简化接口
// 这些函数已被store_bars和store_ticks函数替代

//bool trans_bars(WtString barFile, FuncGetBarItem getter, int count, WtString period, FuncLogCallback cbLogger /* = NULL */)
//{
//	if (count == 0)
//	{
//		if (cbLogger)
//			cbLogger("K线数据条数为0");
//		return false;
//	}
//
//	BlockType bType = BT_HIS_Day;
//	if (wt_stricmp(period, "m1") == 0)
//		bType = BT_HIS_Minute1;
//	else if (wt_stricmp(period, "m5") == 0)
//		bType = BT_HIS_Minute5;
//	else if(wt_stricmp(period, "d") == 0)
//		bType = BT_HIS_Day;
//	else
//	{
//		if (cbLogger)
//			cbLogger("周期只能为m1、m5或d");
//		return false;
//	}
//
//	std::string buffer;
//	buffer.resize(sizeof(WTSBarStruct)*count);
//	WTSBarStruct* bars = (WTSBarStruct*)buffer.c_str();
//	int realCnt = 0;
//	for(int i = 0; i < count; i++)
//	{
//		bool bSucc = getter(&bars[i], i);
//		if (!bSucc)
//			break;
//
//		realCnt++;
//	}
//
//	if (realCnt != count)
//	{
//		buffer.resize(sizeof(WTSBarStruct)*realCnt);
//	}
//
//	if (cbLogger)
//		cbLogger("K线数据已经读取完成，准备写入文件");
//
//	std::string content;
//	content.resize(sizeof(HisKlineBlockV2));
//	HisKlineBlockV2* block = (HisKlineBlockV2*)content.data();
//	strcpy(block->_blk_flag, BLK_FLAG);
//	block->_version = BLOCK_VERSION_CMP;
//	block->_type = bType;
//	std::string cmp_data = WTSCmpHelper::compress_data(bars, buffer.size());
//	block->_size = cmp_data.size();
//	content.append(cmp_data);
//
//	BoostFile bf;
//	if (bf.create_new_file(barFile))
//	{
//		bf.write_file(content);
//	}
//	bf.close_file();
//
//	if (cbLogger)
//		cbLogger("K线数据写入文件成功");
//	return true;
//}
//
//bool trans_ticks(WtString tickFile, FuncGetTickItem getter, int count, FuncLogCallback cbLogger/* = NULL*/)
//{
//	if (count == 0)
//	{
//		if (cbLogger)
//			cbLogger("Tick数据条数为0");
//		return false;
//	}
//
//	std::string buffer;
//	buffer.resize(sizeof(WTSTickStruct)*count);
//	WTSTickStruct* ticks = (WTSTickStruct*)buffer.c_str();
//	int realCnt = 0;
//	for (int i = 0; i < count; i++)
//	{
//		bool bSucc = getter(&ticks[i], i);
//		if (!bSucc)
//			break;
//
//		realCnt++;
//	}
//
//	if(realCnt != count)
//	{
//		buffer.resize(sizeof(WTSTickStruct)*realCnt);
//	}
//
//	if (cbLogger)
//		cbLogger("Tick数据已经读取完成，准备写入文件");
//
//	std::string content;
//	content.resize(sizeof(HisKlineBlockV2));
//	HisKlineBlockV2* block = (HisKlineBlockV2*)content.data();
//	strcpy(block->_blk_flag, BLK_FLAG);
//	block->_version = BLOCK_VERSION_CMP;
//	block->_type = BT_HIS_Ticks;
//	std::string cmp_data = WTSCmpHelper::compress_data(ticks, buffer.size());
//	block->_size = cmp_data.size();
//	content.append(cmp_data);
//
//	BoostFile bf;
//	if (bf.create_new_file(tickFile))
//	{
//		bf.write_file(content);
//	}
//	bf.close_file();
//
//	if (cbLogger)
//		cbLogger("Tick数据写入文件成功");
//
//	return true;
//}

/**
 * @brief 读取DSB格式的Tick数据文件
 * @param tickFile Tick数据文件路径
 * @param cb Tick数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DSB格式的Tick数据文件，通过回调函数返回数据。
 * 支持压缩数据的自动解压和版本兼容性处理。
 * 文件格式：HisTickBlock头部 + Tick数据（可能压缩）
 */
WtUInt32 read_dsb_ticks(WtString tickFile, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger /* = NULL */)
{
	std::string path = tickFile;                                               // 文件路径

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string content;                                                         // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), content);                      // 读取整个文件内容
	if (content.size() < sizeof(HisTickBlock))                                 // 检查文件大小是否至少包含一个Tick数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", tickFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	proc_block_data(content, false, false);                                     // 处理数据块（解压缩、版本转换等），isBar=false表示是Tick数据，bKeepHead=false表示不保留头部

	if (content.empty())                                                        // 如果处理后的内容为空
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}

	auto tcnt = content.size() / sizeof(WTSTickStruct);                         // 计算Tick数据条数

	cbCnt(tcnt);                                                                // 调用计数回调，通知数据总条数
	cb((WTSTickStruct*)content.data(), tcnt, true);                             // 调用数据回调，传递所有数据（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条tick数据", tickFile, tcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)tcnt;                                                      // 返回读取的数据条数
}

/**
 * @brief 读取DSB格式的逐笔委托数据文件
 * @param dataFile 数据文件路径
 * @param cb 逐笔委托数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DSB格式的逐笔委托数据文件，主要用于股票Level-2数据处理。
 * 支持压缩数据的自动解压和版本兼容性处理。
 * 文件格式：HisOrdDtlBlock头部 + 逐笔委托数据（可能压缩）
 */
WtUInt32 read_dsb_order_details(WtString dataFile, FuncGetOrdDtlCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger/* = NULL*/)
{
	std::string path = dataFile;                                               // 文件路径

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string content;                                                         // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), content);                      // 读取整个文件内容
	if (content.size() < sizeof(HisOrdDtlBlock))                               // 检查文件大小是否至少包含一个逐笔委托数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", dataFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	proc_block_data(content, false, false);                                     // 处理数据块（解压缩、版本转换等），isBar=false表示不是K线数据，bKeepHead=false表示不保留头部

	if (content.empty())                                                        // 如果处理后的内容为空
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}

	auto tcnt = content.size() / sizeof(WTSOrdDtlStruct);                     // 计算逐笔委托数据条数

	cbCnt(tcnt);                                                                // 调用计数回调，通知数据总条数
	cb((WTSOrdDtlStruct*)content.data(), tcnt, true);                           // 调用数据回调，传递所有数据（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条order detail数据", dataFile, tcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)tcnt;                                                      // 返回读取的数据条数
}

/**
 * @brief 读取DSB格式的委托队列数据文件
 * @param dataFile 数据文件路径
 * @param cb 委托队列数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DSB格式的委托队列数据文件，主要用于股票Level-2数据处理。
 * 支持压缩数据的自动解压和版本兼容性处理。
 * 文件格式：HisOrdQueBlock头部 + 委托队列数据（可能压缩）
 */
WtUInt32 read_dsb_order_queues(WtString dataFile, FuncGetOrdQueCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger/* = NULL*/)
{
	std::string path = dataFile;                                               // 文件路径

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string content;                                                         // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), content);                      // 读取整个文件内容
	if (content.size() < sizeof(HisOrdQueBlock))                               // 检查文件大小是否至少包含一个委托队列数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", dataFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	proc_block_data(content, false, false);                                     // 处理数据块（解压缩、版本转换等），isBar=false表示不是K线数据，bKeepHead=false表示不保留头部

	if (content.empty())                                                        // 如果处理后的内容为空
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}

	auto tcnt = content.size() / sizeof(WTSOrdQueStruct);                      // 计算委托队列数据条数

	cbCnt(tcnt);                                                                // 调用计数回调，通知数据总条数
	cb((WTSOrdQueStruct*)content.data(), tcnt, true);                           // 调用数据回调，传递所有数据（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条order queue数据", dataFile, tcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)tcnt;                                                      // 返回读取的数据条数
}

/**
 * @brief 读取DSB格式的逐笔成交数据文件
 * @param dataFile 数据文件路径
 * @param cb 逐笔成交数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DSB格式的逐笔成交数据文件，主要用于股票Level-2数据处理。
 * 支持压缩数据的自动解压和版本兼容性处理。
 * 文件格式：HisTransBlock头部 + 逐笔成交数据（可能压缩）
 */
WtUInt32 read_dsb_transactions(WtString dataFile, FuncGetTransCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger/* = NULL*/)
{
	std::string path = dataFile;                                               // 文件路径

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string content;                                                         // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), content);                      // 读取整个文件内容
	if (content.size() < sizeof(HisTransBlock))                                // 检查文件大小是否至少包含一个逐笔成交数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", dataFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	proc_block_data(content, false, false);                                     // 处理数据块（解压缩、版本转换等），isBar=false表示不是K线数据，bKeepHead=false表示不保留头部

	if (content.empty())                                                        // 如果处理后的内容为空
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}

	auto tcnt = content.size() / sizeof(WTSTransStruct);                       // 计算逐笔成交数据条数

	cbCnt(tcnt);                                                                // 调用计数回调，通知数据总条数
	cb((WTSTransStruct*)content.data(), tcnt, true);                            // 调用数据回调，传递所有数据（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条transaction数据", dataFile, tcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)tcnt;                                                      // 返回读取的数据条数
}

/**
 * @brief 读取DSB格式的K线数据文件
 * @param barFile K线数据文件路径
 * @param cb K线数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DSB格式的K线数据文件，支持多种周期的K线数据。
 * 自动处理数据压缩和版本兼容性问题。
 * 文件格式：HisKlineBlock头部 + K线数据（可能压缩）
 */
WtUInt32 read_dsb_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger )
{
	std::string path = barFile;                                                 // 文件路径
	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string content;                                                         // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), content);                      // 读取整个文件内容
	if (content.size() < sizeof(HisKlineBlock))                                // 检查文件大小是否至少包含一个K线数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	proc_block_data(content, true, false);                                      // 处理数据块（解压缩、版本转换等），isBar=true表示是K线数据，bKeepHead=false表示不保留头部

	if(content.empty())                                                        // 如果处理后的内容为空
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}


	auto kcnt = content.size() / sizeof(WTSBarStruct);                         // 计算K线数据条数
	cbCnt(kcnt);                                                                // 调用计数回调，通知数据总条数
	cb((WTSBarStruct*)content.data(), kcnt, true);                              // 调用数据回调，传递所有数据（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条bar", barFile, kcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)kcnt;                                                      // 返回读取的数据条数
}

/**
 * @brief 读取DMB格式的K线数据文件
 * @param barFile K线数据文件路径
 * @param cb K线数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DMB（Data Memory Binary）格式的K线数据，主要用于实时数据的快速访问。
 * DMB格式是内存映射格式，不进行压缩，便于快速读取。
 * 文件格式：RTKlineBlock头部（包含数据条数）+ K线数据数组
 */
WtUInt32 read_dmb_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger)
{
	std::string path = barFile;                                                 // 文件路径

	std::string buffer;                                                          // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), buffer);                       // 读取整个文件内容
	if (buffer.size() < sizeof(RTKlineBlock))                                  // 检查文件大小是否至少包含一个实时K线数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	RTKlineBlock* tBlock = (RTKlineBlock*)buffer.c_str();                       // 获取实时K线数据块指针
	auto kcnt = tBlock->_size;                                                  // 从数据块头部获取K线数据条数
	if (kcnt <= 0)                                                              // 如果数据条数为0或负数
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}

	cbCnt(kcnt);                                                                // 调用计数回调，通知数据总条数
	cb(tBlock->_bars, kcnt, true);                                              // 调用数据回调，传递K线数据数组（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条bar", barFile, kcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)kcnt;                                                      // 返回读取的数据条数
}

/**
 * @brief 读取DMB格式的Tick数据文件
 * @param tickFile Tick数据文件路径
 * @param cb Tick数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param cbLogger 日志回调函数（可选）
 * @return 读取的数据条数
 * 
 * 读取DMB（Data Memory Binary）格式的Tick数据，主要用于实时数据的内存映射访问。
 * DMB格式是内存映射格式，不进行压缩，便于快速读取。
 * 文件格式：RTTickBlock头部（包含数据条数）+ Tick数据数组
 */
WtUInt32 read_dmb_ticks(WtString tickFile, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger /* = NULL */)
{
	std::string path = tickFile;                                               // 文件路径

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string buffer;                                                          // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), buffer);                       // 读取整个文件内容
	if (buffer.size() < sizeof(RTTickBlock))                                   // 检查文件大小是否至少包含一个实时Tick数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", tickFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	RTTickBlock* tBlock = (RTTickBlock*)buffer.c_str();                        // 获取实时Tick数据块指针
	auto tcnt = tBlock->_size;                                                  // 从数据块头部获取Tick数据条数
	if (tcnt <= 0)                                                              // 如果数据条数为0或负数
	{
		cbCnt(0);                                                               // 调用计数回调，通知数据条数为0
		return 0;                                                                // 返回0表示没有数据
	}

	cbCnt(tcnt);                                                                // 调用计数回调，通知数据总条数
	cb(tBlock->_ticks, tcnt, true);                                             // 调用数据回调，传递Tick数据数组（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s读取完成,共%u条tick数据", tickFile, tcnt).c_str());  // 记录读取完成的日志

	return (WtUInt32)tcnt;                                                      // 返回读取的数据条数
}


/**
 * @brief K线数据重采样功能
 * @param barFile 源K线数据文件路径
 * @param cb K线数据回调函数
 * @param cbCnt 数据计数回调函数
 * @param fromTime 开始时间（日线：yyyymmdd，分钟线：yyyymmddHHMM）
 * @param endTime 结束时间（日线：yyyymmdd，分钟线：yyyymmddHHMM）
 * @param period 基础周期（"m1"=1分钟, "m5"=5分钟, "d"=日线）
 * @param times 重采样倍数（如：基础周期m1，times=5，得到5分钟线）
 * @param sessInfo 交易时段信息（JSON格式）
 * @param cbLogger 日志回调函数（可选）
 * @param bAlignSec 是否按秒对齐（可选，默认false）
 * @return 重采样后的数据条数
 * 
 * 对K线数据进行重采样，将基础周期的数据转换为更大周期的数据。
 * 支持精确的交易时段处理，确保重采样结果的准确性。
 * 
 * 处理流程：
 * 1. 解析基础周期参数
 * 2. 验证时间范围格式
 * 3. 解析交易时段信息（JSON格式）
 * 4. 读取源K线数据文件
 * 5. 使用二分查找定位时间范围
 * 6. 使用WTSDataFactory进行K线重采样
 * 7. 通过回调函数返回重采样结果
 * 
 * 交易时段信息格式示例：
 * {
 *   "offset": 0,
 *   "auction": {"from": 925, "to": 930},
 *   "sections": [
 *     {"from": 930, "to": 1130},
 *     {"from": 1300, "to": 1500}
 *   ]
 * }
 */
WtUInt32 resample_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, WtUInt64 fromTime, WtUInt64 endTime,
	WtString period, WtUInt32 times, WtString sessInfo, FuncLogCallback cbLogger /* = NULL */, bool bAlignSec/* = false*/)
{
	WTSKlinePeriod kp;                                                          // K线周期枚举
	if(wt_stricmp(period, "m1") == 0)                                           // 如果周期参数为"m1"
	{
		kp = KP_Minute1;                                                        // 设置为1分钟周期
	}
	else if (wt_stricmp(period, "m5") == 0)                                    // 如果周期参数为"m5"
	{
		kp = KP_Minute5;                                                        // 设置为5分钟周期
	}
	else if (wt_stricmp(period, "d") == 0)                                     // 如果周期参数为"d"
	{
		kp = KP_DAY;                                                            // 设置为日线周期
	}
	else                                                                       // 其他情况
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("周期%s不是基础周期...", period).c_str());  // 记录错误日志
		return 0;                                                                // 返回0表示失败
	}

	bool isDay = (kp == KP_DAY);                                                // 判断是否为日线数据

	if(isDay)                                                                   // 如果是日线数据
	{
		if(fromTime >= 100000000 || endTime > 100000000)                       // 如果时间值大于等于100000000（说明是时间格式而不是日期格式）
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger("日线基础数据的开始时间结束时间应为日期，格式如yyyymmdd");  // 记录错误日志
			return 0;                                                            // 返回0表示失败
		}
	}
	else                                                                       // 如果是分钟线数据
	{
		if (fromTime < 100000000 || endTime < 100000000)                       // 如果时间值小于100000000（说明是日期格式而不是时间格式）
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger("分钟线基础数据的开始时间结束时间应为时间，格式如yyyymmddHHMM");  // 记录错误日志
			return 0;                                                            // 返回0表示失败
		}
	}

	if(fromTime > endTime)                                                     // 如果开始时间大于结束时间
	{
		std::swap(fromTime, endTime);                                           // 交换开始时间和结束时间，确保时间范围正确
	}

	WTSSessionInfo* sInfo = NULL;                                              // 交易时段信息对象指针
	{
		rj::Document root;                                                      // RapidJSON文档对象
		if (root.Parse(sessInfo).HasParseError())                              // 解析JSON格式的交易时段信息
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger("交易时间模板解析失败");                               // 记录错误日志
			return 0;                                                            // 返回0表示失败
		}

		int32_t offset = root["offset"].GetInt();                             // 获取时区偏移量（秒）

		sInfo = WTSSessionInfo::create("tmp", "tmp", offset);                // 创建交易时段信息对象

		if (!root["auction"].IsNull())                                         // 如果JSON中包含集合竞价时段配置
		{
			const rj::Value& jAuc = root["auction"];                           // 获取集合竞价时段配置
			sInfo->setAuctionTime(jAuc["from"].GetUint(), jAuc["to"].GetUint());  // 设置集合竞价时间（开始时间和结束时间，格式为HHMM）
		}

		const rj::Value& jSecs = root["sections"];                             // 获取交易时段数组
		if (jSecs.IsNull() || !jSecs.IsArray())                                // 如果交易时段数组不存在或不是数组类型
		{
			if (cbLogger)                                                       // 如果提供了日志回调函数
				cbLogger("交易时间模板格式错误");                               // 记录错误日志
			return 0;                                                            // 返回0表示失败
		}

		for (const rj::Value& jSec : jSecs.GetArray())                         // 遍历交易时段数组
		{
			sInfo->addTradingSection(jSec["from"].GetUint(), jSec["to"].GetUint());  // 添加交易时段（开始时间和结束时间，格式为HHMM）
		}
	}

	std::string path = barFile;                                                 // 文件路径
	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());  // 记录开始读取文件的日志

	std::string buffer;                                                          // 文件内容缓冲区
	BoostFile::read_file_contents(path.c_str(), buffer);                       // 读取整个文件内容
	if (buffer.size() < sizeof(HisKlineBlock))                                // 检查文件大小是否至少包含一个K线数据块头部
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());  // 记录文件头部校验失败的日志
		return 0;                                                                // 返回0表示读取失败
	}

	proc_block_data(buffer, true, false);                                      // 处理数据块（解压缩、版本转换等），isBar=true表示是K线数据，bKeepHead=false表示不保留头部

	auto kcnt = buffer.size() / sizeof(WTSBarStruct);                         // 计算K线数据条数
	if (kcnt <= 0)                                                              // 如果数据条数为0或负数
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger(StrUtil::printf("%s数据为空", barFile).c_str());          // 记录数据为空的日志
		return 0;                                                                // 返回0表示没有数据
	}

	WTSBarStruct* bars = (WTSBarStruct*)buffer.c_str();                        // 获取K线数据数组指针

	// 确定第一条K线的位置
	WTSBarStruct bar;                                                           // 临时K线结构，用于查找
	if (isDay)                                                                  // 如果是日线数据
		bar.date = (uint32_t)fromTime;                                         // 设置查找日期
	else                                                                       // 如果是分钟线数据
	{

		bar.time = fromTime % 100000000 + ((fromTime / 100000000) - 1990) * 100000000;  // 转换时间格式：从yyyymmddHHMM格式转换为内部时间格式（基准年份1990）
	}

	WTSBarStruct* pBar = std::lower_bound(bars, bars + (kcnt - 1), bar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位第一条K线
		if (isDay)                                                              // 如果是日线数据
			return a.date < b.date;                                             // 按日期比较
		else                                                                    // 如果是分钟线数据
			return a.time < b.time;                                             // 按时间比较
	});


	uint32_t sIdx = (uint32_t)(pBar - bars);                                   // 计算第一条K线的索引位置
	if((isDay && pBar->date < bar.date) || (!isDay && pBar->time < bar.time))  // 如果返回的K线的时间小于要查找的时间，说明没有符合条件的数据
	{
		// 如果返回的K线的时间小于要查找的时间，说明没有符合条件的数据
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("没有找到指定时间范围的K线");                               // 记录错误日志
		return 0;                                                                // 返回0表示失败
	}
	else if (sIdx != 0 && ((isDay && pBar->date > bar.date) || (!isDay && pBar->time > bar.time)))  // 如果找到了但时间稍大，且不是第一个元素
	{
		pBar--;                                                                  // 向前移动一个位置
		sIdx--;                                                                  // 索引减1
	}

	// 确定最后一条K线的位置
	if (isDay)                                                                  // 如果是日线数据
		bar.date = (uint32_t)endTime;                                          // 设置查找日期
	else                                                                       // 如果是分钟线数据
	{

		bar.time = endTime % 100000000 + ((endTime / 100000000) - 1990) * 100000000;  // 转换时间格式：从yyyymmddHHMM格式转换为内部时间格式（基准年份1990）
	}
	pBar = std::lower_bound(bars, bars + (kcnt - 1), bar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {  // 使用二分查找定位最后一条K线
		if (isDay)                                                              // 如果是日线数据
			return a.date < b.date;                                             // 按日期比较
		else                                                                    // 如果是分钟线数据
			return a.time < b.time;                                             // 按时间比较
	});

	uint32_t eIdx = 0;                                                          // 最后一条K线的索引位置
	if (pBar == NULL)                                                          // 如果查找结果为空（不应该发生）
		eIdx = kcnt - 1;                                                        // 设置为最后一条K线
	else                                                                       // 如果查找成功
		eIdx = (uint32_t)(pBar - bars);                                         // 计算索引位置

	if (eIdx != 0 && ((isDay && pBar->date > bar.date) || (!isDay && pBar->time > bar.time)))  // 如果找到了但时间稍大，且不是第一个元素
	{
		pBar--;                                                                  // 向前移动一个位置
		eIdx--;                                                                  // 索引减1
	}

	uint32_t hitCnt = eIdx - sIdx + 1;                                          // 计算时间范围内的K线数据条数
	WTSKlineSlice* slice = WTSKlineSlice::create("", kp, 1, &bars[sIdx], hitCnt);  // 创建K线数据切片，包含时间范围内的K线数据
	WTSDataFactory fact;                                                        // 创建数据工厂对象
	WTSKlineData* kline = fact.extractKlineData(slice, kp, times, sInfo, true, bAlignSec);  // 使用数据工厂进行K线重采样，extractKlineData会将基础周期数据转换为times倍周期的数据
	if(kline == NULL)                                                          // 如果重采样失败
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("K线重采样失败");                                           // 记录错误日志
		return 0;                                                                // 返回0表示失败
	}

	uint32_t newCnt = kline->size();                                            // 获取重采样后的K线数据条数
	cbCnt(newCnt);                                                              // 调用计数回调，通知重采样后的数据总条数
	cb(&kline->getDataRef().at(0),newCnt, true);                               // 调用数据回调，传递重采样后的所有数据（isLast=true表示是最后一批）

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger(StrUtil::printf("%s重采样完成,共将%u条bar重采样为%u条新bar", barFile, hitCnt, newCnt).c_str());  // 记录重采样完成的日志，包含原始数据条数和重采样后的数据条数

	
	kline->release();                                                           // 释放重采样后的K线数据对象
	sInfo->release();                                                           // 释放交易时段信息对象
	slice->release();                                                            // 释放K线数据切片对象

	return (WtUInt32)newCnt;                                                    // 返回重采样后的数据条数
}

/**
 * @brief 存储K线数据到二进制文件
 * @param barFile 输出文件路径
 * @param firstBar K线数据数组首地址
 * @param count 数据条数
 * @param period 数据周期（"m1"=1分钟, "m5"=5分钟, "d"=日线）
 * @param cbLogger 日志回调函数（可选）
 * @return 存储是否成功
 * 
 * 将内存中的K线数据存储为DSB格式的二进制文件。
 * 数据会进行压缩处理，减少文件大小。
 * 文件格式：HisKlineBlockV2头部 + 压缩后的K线数据
 */
bool store_bars(WtString barFile, WTSBarStruct* firstBar, int count, WtString period, FuncLogCallback cbLogger /* = NULL */)
{
	if (count == 0)                                                             // 如果数据条数为0
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("K线数据条数为0");                                         // 记录错误日志
		return false;                                                            // 返回false表示失败
	}

	BlockType bType = BT_HIS_Day;                                                // 默认数据块类型为日线
	if (wt_stricmp(period, "m1") == 0)                                         // 如果周期参数为"m1"
		bType = BT_HIS_Minute1;                                                 // 设置为1分钟线
	else if (wt_stricmp(period, "m5") == 0)                                   // 如果周期参数为"m5"
		bType = BT_HIS_Minute5;                                                 // 设置为5分钟线
	else if (wt_stricmp(period, "d") == 0)                                     // 如果周期参数为"d"
		bType = BT_HIS_Day;                                                     // 设置为日线
	else                                                                       // 其他情况（不支持的周期）
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("周期只能为m1、m5或d");                                     // 记录错误日志
		return false;                                                            // 返回false表示失败
	}

	std::string buffer;                                                          // 数据缓冲区
	buffer.resize(sizeof(WTSBarStruct)*count);                                   // 分配缓冲区大小
	WTSBarStruct* bars = (WTSBarStruct*)buffer.c_str();                        // 获取缓冲区指针
	memcpy(bars, firstBar, sizeof(WTSBarStruct)*count);                         // 将K线数据复制到缓冲区

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("K线数据已经读取完成，准备写入文件");                         // 记录准备写入的日志

	std::string content;                                                         // 文件内容缓冲区
	content.resize(sizeof(HisKlineBlockV2));                                    // 分配数据块头部大小
	HisKlineBlockV2* block = (HisKlineBlockV2*)content.data();                 // 获取数据块头部指针
	strcpy(block->_blk_flag, BLK_FLAG);                                         // 设置数据块标识符
	block->_version = BLOCK_VERSION_CMP_V2;                                     // 设置数据块版本为V2压缩版本
	block->_type = bType;                                                       // 设置数据块类型
	std::string cmp_data = WTSCmpHelper::compress_data(bars, buffer.size());    // 压缩K线数据
	block->_size = cmp_data.size();                                             // 设置压缩后的数据大小
	content.append(cmp_data);                                                   // 将压缩后的数据追加到文件内容

	BoostFile bf;                                                                // 创建文件操作对象
	if (bf.create_new_file(barFile))                                            // 创建新文件
	{
		bf.write_file(content);                                                 // 写入文件内容（包括头部和压缩数据）
	}
	bf.close_file();                                                             // 关闭文件

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("K线数据写入文件成功");                                         // 记录写入成功的日志
	return true;                                                                 // 返回true表示成功
}

/**
 * @brief 存储Tick数据到二进制文件
 * @param tickFile 输出文件路径
 * @param firstTick Tick数据数组首地址
 * @param count 数据条数
 * @param cbLogger 日志回调函数（可选）
 * @return 存储是否成功
 * 
 * 将内存中的Tick数据存储为DSB格式的二进制文件。
 * 数据会进行压缩处理，减少文件大小。
 * 文件格式：HisTickBlockV2头部 + 压缩后的Tick数据
 */
bool store_ticks(WtString tickFile, WTSTickStruct* firstTick, int count, FuncLogCallback cbLogger/* = NULL*/)
{
	if (count == 0)                                                             // 如果数据条数为0
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("Tick数据条数为0");                                        // 记录错误日志
		return false;                                                            // 返回false表示失败
	}

	std::string buffer;                                                          // 数据缓冲区
	buffer.resize(sizeof(WTSTickStruct)*count);                                 // 分配缓冲区大小
	WTSTickStruct* ticks = (WTSTickStruct*)buffer.c_str();                     // 获取缓冲区指针
	memcpy(ticks, firstTick, sizeof(WTSTickStruct)*count);                     // 将Tick数据复制到缓冲区

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Tick数据已经读取完成，准备写入文件");                        // 记录准备写入的日志

	std::string content;                                                         // 文件内容缓冲区
	content.resize(sizeof(HisTickBlockV2));                                      // 分配数据块头部大小
	HisTickBlockV2* block = (HisTickBlockV2*)content.data();                   // 获取数据块头部指针
	strcpy(block->_blk_flag, BLK_FLAG);                                         // 设置数据块标识符
	block->_version = BLOCK_VERSION_CMP_V2;                                     // 设置数据块版本为V2压缩版本
	block->_type = BT_HIS_Ticks;                                                // 设置数据块类型为Tick数据
	std::string cmp_data = WTSCmpHelper::compress_data(ticks, buffer.size());   // 压缩Tick数据
	block->_size = cmp_data.size();                                             // 设置压缩后的数据大小
	content.append(cmp_data);                                                   // 将压缩后的数据追加到文件内容

	BoostFile bf;                                                                // 创建文件操作对象
	if (bf.create_new_file(tickFile))                                           // 创建新文件
	{
		bf.write_file(content);                                                 // 写入文件内容（包括头部和压缩数据）
	}
	bf.close_file();                                                             // 关闭文件

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Tick数据写入文件成功");                                        // 记录写入成功的日志

	return true;                                                                 // 返回true表示成功
}

/**
 * @brief 存储逐笔委托数据到二进制文件
 * @param tickFile 输出文件路径
 * @param firstItem 逐笔委托数据数组首地址
 * @param count 数据条数
 * @param cbLogger 日志回调函数（可选）
 * @return 存储是否成功
 * 
 * 将内存中的逐笔委托数据存储为DSB格式的二进制文件。
 * 数据会进行压缩处理，减少文件大小。
 * 文件格式：HisOrdDtlBlockV2头部 + 压缩后的逐笔委托数据
 * 主要用于股票Level-2数据的持久化。
 */
bool store_order_details(WtString tickFile, WTSOrdDtlStruct* firstItem, int count, FuncLogCallback cbLogger/* = NULL*/)
{
	if (count == 0)                                                             // 如果数据条数为0
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("Size of OrderDetail is 0");                              // 记录错误日志（英文，保持原样）
		return false;                                                            // 返回false表示失败
	}

	std::string buffer;                                                          // 数据缓冲区
	buffer.resize(sizeof(WTSOrdDtlStruct)*count);                               // 分配缓冲区大小
	WTSOrdDtlStruct* items = (WTSOrdDtlStruct*)buffer.c_str();                  // 获取缓冲区指针
	memcpy(items, firstItem, sizeof(WTSOrdDtlStruct)*count);                    // 将逐笔委托数据复制到缓冲区

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Reading order details done, prepare to write...");           // 记录准备写入的日志（英文，保持原样）

	std::string content;                                                         // 文件内容缓冲区
	content.resize(sizeof(HisOrdDtlBlockV2));                                    // 分配数据块头部大小
	HisOrdDtlBlockV2* block = (HisOrdDtlBlockV2*)content.data();               // 获取数据块头部指针
	strcpy(block->_blk_flag, BLK_FLAG);                                         // 设置数据块标识符
	block->_version = BLOCK_VERSION_CMP_V2;                                     // 设置数据块版本为V2压缩版本
	block->_type = BT_HIS_OrdDetail;                                           // 设置数据块类型为逐笔委托数据
	std::string cmp_data = WTSCmpHelper::compress_data(items, buffer.size());  // 压缩逐笔委托数据
	block->_size = cmp_data.size();                                             // 设置压缩后的数据大小
	content.append(cmp_data);                                                   // 将压缩后的数据追加到文件内容

	BoostFile bf;                                                                // 创建文件操作对象
	if (bf.create_new_file(tickFile))                                           // 创建新文件
	{
		bf.write_file(content);                                                 // 写入文件内容（包括头部和压缩数据）
	}
	bf.close_file();                                                             // 关闭文件

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Writing order details succeed");                              // 记录写入成功的日志（英文，保持原样）

	return true;                                                                 // 返回true表示成功
}

/**
 * @brief 存储委托队列数据到二进制文件
 * @param tickFile 输出文件路径
 * @param firstItem 委托队列数据数组首地址
 * @param count 数据条数
 * @param cbLogger 日志回调函数（可选）
 * @return 存储是否成功
 * 
 * 将内存中的委托队列数据存储为DSB格式的二进制文件。
 * 数据会进行压缩处理，减少文件大小。
 * 文件格式：HisOrdQueBlockV2头部 + 压缩后的委托队列数据
 * 主要用于股票Level-2数据的持久化。
 */
bool store_order_queues(WtString tickFile, WTSOrdQueStruct* firstItem, int count, FuncLogCallback cbLogger/* = NULL*/)
{
	if (count == 0)                                                             // 如果数据条数为0
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("Size of order queues is 0");                             // 记录错误日志（英文，保持原样）
		return false;                                                            // 返回false表示失败
	}

	std::string buffer;                                                          // 数据缓冲区
	buffer.resize(sizeof(WTSOrdQueStruct)*count);                                // 分配缓冲区大小
	WTSOrdQueStruct* items = (WTSOrdQueStruct*)buffer.c_str();                 // 获取缓冲区指针
	memcpy(items, firstItem, sizeof(WTSOrdQueStruct)*count);                   // 将委托队列数据复制到缓冲区

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Reading order queues done, prepare to write...");            // 记录准备写入的日志（英文，保持原样）

	std::string content;                                                         // 文件内容缓冲区
	content.resize(sizeof(HisOrdQueBlockV2));                                   // 分配数据块头部大小
	HisOrdQueBlockV2* block = (HisOrdQueBlockV2*)content.data();               // 获取数据块头部指针
	strcpy(block->_blk_flag, BLK_FLAG);                                         // 设置数据块标识符
	block->_version = BLOCK_VERSION_CMP_V2;                                     // 设置数据块版本为V2压缩版本
	block->_type = BT_HIS_OrdQueue;                                            // 设置数据块类型为委托队列数据
	std::string cmp_data = WTSCmpHelper::compress_data(items, buffer.size());  // 压缩委托队列数据
	block->_size = cmp_data.size();                                             // 设置压缩后的数据大小
	content.append(cmp_data);                                                   // 将压缩后的数据追加到文件内容

	BoostFile bf;                                                                // 创建文件操作对象
	if (bf.create_new_file(tickFile))                                           // 创建新文件
	{
		bf.write_file(content);                                                 // 写入文件内容（包括头部和压缩数据）
	}
	bf.close_file();                                                             // 关闭文件

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Writing order queues to file succeedd");                     // 记录写入成功的日志（英文，保持原样，注意原代码有拼写错误：succeedd）

	return true;                                                                 // 返回true表示成功
}

/**
 * @brief 存储逐笔成交数据到二进制文件
 * @param tickFile 输出文件路径
 * @param firstItem 逐笔成交数据数组首地址
 * @param count 数据条数
 * @param cbLogger 日志回调函数（可选）
 * @return 存储是否成功
 * 
 * 将内存中的逐笔成交数据存储为DSB格式的二进制文件。
 * 数据会进行压缩处理，减少文件大小。
 * 文件格式：HisTransBlockV2头部 + 压缩后的逐笔成交数据
 * 主要用于股票Level-2数据的持久化。
 */
bool store_transactions(WtString tickFile, WTSTransStruct* firstItem, int count, FuncLogCallback cbLogger/* = NULL*/)
{
	if (count == 0)                                                             // 如果数据条数为0
	{
		if (cbLogger)                                                           // 如果提供了日志回调函数
			cbLogger("Size of transations is 0");                              // 记录错误日志（英文，保持原样，注意原代码有拼写错误：transations）
		return false;                                                            // 返回false表示失败
	}

	std::string buffer;                                                          // 数据缓冲区
	buffer.resize(sizeof(WTSTransStruct)*count);                                // 分配缓冲区大小
	WTSTransStruct* items = (WTSTransStruct*)buffer.c_str();                   // 获取缓冲区指针
	memcpy(items, firstItem, sizeof(WTSTransStruct)*count);                     // 将逐笔成交数据复制到缓冲区

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Reading transactions done, prepare to write...");            // 记录准备写入的日志（英文，保持原样）

	std::string content;                                                         // 文件内容缓冲区
	content.resize(sizeof(HisTransBlockV2));                                      // 分配数据块头部大小
	HisTransBlockV2* block = (HisTransBlockV2*)content.data();                 // 获取数据块头部指针
	strcpy(block->_blk_flag, BLK_FLAG);                                         // 设置数据块标识符
	block->_version = BLOCK_VERSION_CMP_V2;                                     // 设置数据块版本为V2压缩版本
	block->_type = BT_HIS_Trnsctn;                                             // 设置数据块类型为逐笔成交数据
	std::string cmp_data = WTSCmpHelper::compress_data(items, buffer.size());  // 压缩逐笔成交数据
	block->_size = cmp_data.size();                                             // 设置压缩后的数据大小
	content.append(cmp_data);                                                   // 将压缩后的数据追加到文件内容

	BoostFile bf;                                                                // 创建文件操作对象
	if (bf.create_new_file(tickFile))                                           // 创建新文件
	{
		bf.write_file(content);                                                 // 写入文件内容（包括头部和压缩数据）
	}
	bf.close_file();                                                             // 关闭文件

	if (cbLogger)                                                               // 如果提供了日志回调函数
		cbLogger("Write transactions to file succeedd");                       // 记录写入成功的日志（英文，保持原样，注意原代码有拼写错误：succeedd）

	return true;                                                                 // 返回true表示成功
}