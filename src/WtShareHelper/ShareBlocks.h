/*!
 * \file ShareBlocks.h
 * \project	WonderTrader
 *
 * \brief 共享内存块核心类定义
 * 
 * 本文件定义了ShareBlocks类及其相关的数据结构，实现基于内存映射文件的进程间共享内存。
 * 
 * 设计说明：
 * - 使用内存映射文件（Memory-Mapped File）实现进程间共享内存
 * - 支持Master/Slave模式：Master创建和写入，Slave读取
 * - 提供域（domain）、节（section）、键（key）的三级结构
 * - 支持多种数据类型：int32、int64、uint32、uint64、double、string
 * - 使用紧凑的内存布局（#pragma pack），提高内存利用率
 * - 提供命令队列功能，支持进程间命令传递
 * 
 * 数据结构层次：
 * ShmBlock（共享内存块）
 *   └─ SecInfo[]（节数组，最多64个）
 *        └─ KeyInfo[]（键数组，最多64个）
 *             └─ _data[]（数据区域，1024字节）
 * 
 * 使用流程：
 * Master模式：
 * 1. init_master - 创建或打开共享内存块
 * 2. allocate_xxx - 分配键值对
 * 3. set_xxx - 设置值
 * 4. commit_section - 提交节
 * 
 * Slave模式：
 * 1. init_slave - 连接到共享内存块
 * 2. update_slave - 更新数据
 * 3. get_xxx - 获取值
 */
#pragma once
#include <stdint.h>                  // 标准整数类型定义
#include <memory>                    // 智能指针支持

#include "../Share/BoostMappingFile.hpp"  // Boost内存映射文件类
#include "../Includes/FasterDefs.h"  // 快速哈希映射等定义

USING_NS_WTP;                        // 使用WonderTrader命名空间

typedef std::shared_ptr<BoostMappingFile> MappedFilePtr;  // 内存映射文件智能指针类型定义

namespace shareblock                 // shareblock命名空间
{
	const char BLK_FLAG[] = "&^%$#@!\0";  // 共享内存块标志字符串，用于标识有效的共享内存块

	const int FLAG_SIZE = 8;         // 标志大小（字节），BLK_FLAG的长度
	const int MAX_SEC_CNT = 64;      // 最大节数量，每个共享内存块最多64个节
	const int MAX_KEY_CNT = 64;      // 最大键数量，每个节最多64个键
	const int MAX_CMD_SIZE = 64;     // 最大命令大小（字节），命令队列中每条命令的最大长度

	typedef uint64_t ValueType;      // 值类型定义，用于标识数据类型
	const ValueType	SMVT_INT32 = 1;   // 值类型：32位有符号整数
	const ValueType	SMVT_UINT32 = 2;  // 值类型：32位无符号整数
	const ValueType	SMVT_INT64 = 3;   // 值类型：64位有符号整数
	const ValueType	SMVT_UINT64 = 4;  // 值类型：64位无符号整数
	const ValueType	SMVT_DOUBLE = 5;  // 值类型：双精度浮点数
	const ValueType	SMVT_STRING = 6;  // 值类型：字符串（固定64字节）

	const std::size_t SMVT_SIZES[] = { 0,4,4,8,8,8,64 };  // 各类型的大小数组（字节），索引对应ValueType值

	#pragma pack(push, 1)             // 开始紧凑内存布局（1字节对齐）
	/**
	 * @struct KeyInfo
	 * @brief 键信息结构
	 * 
	 * 定义了键的元数据信息，包括键名、类型、数据偏移和更新时间。
	 * 使用紧凑的内存布局，减少内存占用。
	 */
	typedef struct _KeyInfo
	{
		char		_key[32];        // 键名称（32字节固定长度），标识键的唯一名称
		ValueType	_type;           // 值类型（64位无符号整数），标识数据类型（SMVT_INT32等）
		uint32_t	_offset;         // 数据偏移量（32位无符号整数），数据在_data数组中的偏移位置
		uint64_t	_updatetime;     // 更新时间戳（64位无符号整数，毫秒），记录键的最后修改时间
	} KeyInfo;

	/**
	 * @struct SecInfo
	 * @brief 节信息结构
	 * 
	 * 定义了节的元数据信息，包括节名、键数组、数据区域等。
	 * 每个节最多包含64个键，数据区域大小为1024字节。
	 */
	typedef struct _SectionInfo
	{
		char		_name[32];       // 节名称（32字节固定长度），标识节的唯一名称
		KeyInfo		_keys[MAX_KEY_CNT];  // 键信息数组（最多64个），存储该节下所有键的元数据
		uint16_t	_count;			//数据条数，即key的个数（16位无符号整数），当前已分配的键数量
		uint16_t	_state;			//状态（16位无符号整数）：0-无效，1-生效，2-已删除
		uint32_t	_offset;		//记录下一个可分配地址的偏移量（32位无符号整数），用于分配新键的数据空间
		uint64_t	_updatetime;     // 更新时间戳（64位无符号整数，毫秒），记录节的最后修改时间
		char		_data[1024];     // 数据区域（1024字节），存储所有键的实际数据

		/**
		 * @brief 获取指定偏移处的数据指针
		 * @tparam T 数据类型模板参数
		 * @param offset 数据偏移量
		 * @return 返回类型T的指针
		 * 
		 * 根据偏移量从_data数组中获取指定类型的数据指针。
		 * 用于访问键的实际数据。
		 */
		template<typename T>
		T* get(uint32_t offset)
		{
			return (T*)(_data + offset);  // 将_data数组的偏移位置转换为类型T的指针
		}

		/**
		 * @brief 构造函数
		 * 
		 * 初始化结构体，将所有字段清零。
		 */
		_SectionInfo()
		{
			memset(this, 0, sizeof(_SectionInfo));  // 将整个结构体清零
		}
	} SecInfo;

	/**
	 * @struct ShmBlock
	 * @brief 共享内存块结构
	 * 
	 * 定义了整个共享内存块的布局结构。
	 * 包含标志、名称、节数组和元数据信息。
	 */
	typedef struct _ShmBlock
	{
		char		_flag[8];        // 标志（8字节），用于标识有效的共享内存块，值为BLK_FLAG
		char		_name[32];       // 块名称（32字节固定长度），标识共享内存块的名称
		SecInfo		_sections[MAX_SEC_CNT];  // 节数组（最多64个），存储所有节的元数据和数据
		uint64_t	_updatetime;     // 更新时间戳（64位无符号整数，毫秒），记录块的最后修改时间
		uint32_t	_count;          // 节数量（32位无符号整数），当前已分配的节数量

		/**
		 * @brief 构造函数
		 * 
		 * 初始化结构体，将所有字段清零。
		 */
		_ShmBlock()
		{
			memset(this, 0, sizeof(_ShmBlock));  // 将整个结构体清零
		}
	} ShmBlock;

	/**
	 * @struct CmdInfo
	 * @brief 命令信息结构
	 * 
	 * 定义了命令队列中单条命令的信息。
	 * 包含命令状态和命令内容。
	 */
	typedef struct _CmdInfo
	{
		uint32_t	_state;          // 命令状态（32位无符号整数），0=未读，1=已读
		char		_command[MAX_CMD_SIZE];  // 命令内容（64字节固定长度），存储命令字符串

		/**
		 * @brief 构造函数
		 * 
		 * 初始化结构体，将所有字段清零。
		 */
		_CmdInfo() { memset(this, 0, sizeof(_CmdInfo)); }  // 将整个结构体清零
	} CmdInfo;

	/**
	 * @struct _CmdBlock
	 * @brief 命令块结构（模板）
	 * @tparam N 命令队列容量（默认128）
	 * 
	 * 定义了命令队列的循环缓冲区结构。
	 * 使用volatile关键字确保多线程环境下的可见性。
	 */
	template <int N = 128>
	struct _CmdBlock
	{
		uint32_t	_capacity = N;   // 队列容量（32位无符号整数），命令队列的最大容量
	 	volatile uint32_t	_readable;  // 可读索引（32位无符号整数，volatile），下一个可读命令的索引
		volatile uint32_t	_writable;  // 可写索引（32位无符号整数，volatile），下一个可写位置的索引
		uint32_t	_cmdpid;         // 命令进程ID（32位无符号整数），命令下达者的进程ID
		CmdInfo		_commands[N];     // 命令数组（N个），存储所有命令

		/**
		 * @brief 构造函数
		 * 
		 * 初始化命令块：
		 * - _readable初始化为UINT32_MAX（表示未初始化）
		 * - _writable初始化为0（表示从0开始写入）
		 * - _cmdpid初始化为0（表示未设置）
		 */
		_CmdBlock():_readable(UINT32_MAX),_writable(0),_cmdpid(0){}  // 初始化列表
	};

	typedef _CmdBlock<128>	CmdBlock;  // 命令块类型定义，容量为128

	#pragma pack(pop)                 // 恢复默认内存布局


	/**
	 * @class ShareBlocks
	 * @brief 共享内存块管理类
	 * 
	 * 该类是WtShareHelper模块的核心类，负责：
	 * - 管理多个共享内存块（通过域ID区分）
	 * - 支持Master/Slave模式
	 * - 提供键值对的分配、设置、获取功能
	 * - 提供命令队列功能
	 * 
	 * 设计特点：
	 * - 使用单例模式，确保全局唯一实例
	 * - 使用内存映射文件实现进程间共享
	 * - 使用哈希映射表快速查找域、节、键
	 * - 支持自动清理无效的节
	 */
	class ShareBlocks
	{
	private:
		/**
		 * @brief 私有构造函数
		 * 
		 * 禁止外部创建实例，只能通过one()方法获取单例。
		 */
		ShareBlocks(){}

	public:
		/**
		 * @brief 获取单例实例
		 * @return 返回ShareBlocks实例的引用
		 * 
		 * 使用Meyers' Singleton模式实现单例。
		 * 线程安全（C++11标准保证静态局部变量初始化的线程安全性）。
		 */
		static ShareBlocks& one()
		{
			static ShareBlocks inst;  // 静态局部变量：全局唯一的ShareBlocks实例，首次调用时初始化
			return inst;               // 返回实例的引用
		}

		/**
		 * @brief 初始化Master模式
		 * @param name 域名称
		 * @param path 文件路径
		 * @return 返回初始化是否成功
		 * 
		 * 创建或打开共享内存块，Master模式可以创建和写入数据。
		 */
		bool	init_master(const char* name, const char* path = "");

		/**
		 * @brief 初始化Slave模式
		 * @param name 域名称
		 * @param path 文件路径
		 * @return 返回初始化是否成功
		 * 
		 * 连接到已存在的共享内存块，Slave模式只能读取数据。
		 */
		bool	init_slave(const char* name, const char* path = "");

		/**
		 * @brief 更新Slave数据
		 * @param name 域名称
		 * @param bForce 是否强制更新
		 * @return 返回是否更新成功
		 * 
		 * 刷新Slave的数据，从共享内存块中重新加载数据。
		 */
		bool	update_slave(const char* name, bool bForce);

		/**
		 * @brief 释放Slave连接
		 * @param name 域名称
		 * @return 返回是否释放成功
		 * 
		 * 释放Slave连接，清理相关资源。
		 */
		bool	release_slave(const char* name);

		/**
		 * @brief 获取节列表
		 * @param domain 域名称
		 * @return 返回节名称向量
		 * 
		 * 获取指定域下的所有有效节名称。
		 */
		std::vector<std::string>	get_sections(const char* domain);

		/**
		 * @brief 获取键列表
		 * @param domain 域名称
		 * @param section 节名称
		 * @return 返回键信息指针向量
		 * 
		 * 获取指定域和节下的所有键信息。
		 */
		std::vector<KeyInfo*>		get_keys(const char* domain, const char* section);

		/**
		 * @brief 获取节的更新时间
		 * @param domain 域名称
		 * @param section 节名称
		 * @return 返回更新时间戳
		 * 
		 * 获取指定节的最后更新时间。
		 */
		uint64_t get_section_updatetime(const char* domain, const char* section);

		/**
		 * @brief 提交节（更新修改时间）
		 * @param domain 域名称
		 * @param section 节名称
		 * @return 返回是否提交成功
		 * 
		 * 提交节，更新节的修改时间为当前时间。
		 */
		bool	commit_section(const char* domain, const char* section);

		/**
		 * @brief 删除节
		 * @param domain 域名称
		 * @param section 节名称
		 * @return 返回是否删除成功
		 * 
		 * 删除指定的节，标记为无效状态。
		 */
		bool	delete_section(const char* domain, const char*section);

		/**
		 * @brief 分配字符串类型的键值对
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param initVal 初始值
		 * @param bForceWrite 是否强制写入
		 * @return 返回字符串指针
		 * 
		 * 分配或获取字符串类型的键值对。
		 */
		const char* allocate_string(const char* domain, const char* section, const char* key, const char* initVal = "", bool bForceWrite = false);

		/**
		 * @brief 分配int32类型的键值对
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param initVal 初始值
		 * @param bForceWrite 是否强制写入
		 * @return 返回int32指针
		 * 
		 * 分配或获取int32类型的键值对。
		 */
		int32_t*	allocate_int32(const char* domain, const char* section, const char* key, int32_t initVal = 0, bool bForceWrite = false);

		/**
		 * @brief 分配int64类型的键值对
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param initVal 初始值
		 * @param bForceWrite 是否强制写入
		 * @return 返回int64指针
		 * 
		 * 分配或获取int64类型的键值对。
		 */
		int64_t*	allocate_int64(const char* domain, const char* section, const char* key, int64_t initVal = 0, bool bForceWrite = false);

		/**
		 * @brief 分配uint32类型的键值对
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param initVal 初始值
		 * @param bForceWrite 是否强制写入
		 * @return 返回uint32指针
		 * 
		 * 分配或获取uint32类型的键值对。
		 */
		uint32_t*	allocate_uint32(const char* domain, const char* section, const char* key, uint32_t initVal = 0, bool bForceWrite = false);

		/**
		 * @brief 分配uint64类型的键值对
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param initVal 初始值
		 * @param bForceWrite 是否强制写入
		 * @return 返回uint64指针
		 * 
		 * 分配或获取uint64类型的键值对。
		 */
		uint64_t*	allocate_uint64(const char* domain, const char* section, const char* key, uint64_t initVal = 0, bool bForceWrite = false);

		/**
		 * @brief 分配double类型的键值对
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param initVal 初始值
		 * @param bForceWrite 是否强制写入
		 * @return 返回double指针
		 * 
		 * 分配或获取double类型的键值对。
		 */
		double*		allocate_double(const char* domain, const char* section, const char* key, double initVal = 0, bool bForceWrite = false);

		/**
		 * @brief 设置字符串值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param val 值
		 * @return 返回是否设置成功
		 * 
		 * 设置字符串类型的键值。
		 */
		bool	set_string(const char* domain, const char* section, const char* key, const char* val);

		/**
		 * @brief 设置int32值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param val 值
		 * @return 返回是否设置成功
		 * 
		 * 设置int32类型的键值。
		 */
		bool	set_int32(const char* domain, const char* section, const char* key, int32_t val);

		/**
		 * @brief 设置int64值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param val 值
		 * @return 返回是否设置成功
		 * 
		 * 设置int64类型的键值。
		 */
		bool	set_int64(const char* domain, const char* section, const char* key, int64_t val);

		/**
		 * @brief 设置uint32值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param val 值
		 * @return 返回是否设置成功
		 * 
		 * 设置uint32类型的键值。
		 */
		bool	set_uint32(const char* domain, const char* section, const char* key, uint32_t val);

		/**
		 * @brief 设置uint64值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param val 值
		 * @return 返回是否设置成功
		 * 
		 * 设置uint64类型的键值。
		 */
		bool	set_uint64(const char* domain, const char* section, const char* key, uint64_t val);

		/**
		 * @brief 设置double值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param val 值
		 * @return 返回是否设置成功
		 * 
		 * 设置double类型的键值。
		 */
		bool	set_double(const char* domain, const char* section, const char* key, double val);

		/**
		 * @brief 获取字符串值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param defVal 默认值
		 * @return 返回字符串指针
		 * 
		 * 获取字符串类型的键值。
		 */
		const char*	get_string(const char* domain, const char* section, const char* key, const char* defVal = "");

		/**
		 * @brief 获取int32值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param defVal 默认值
		 * @return 返回int32值
		 * 
		 * 获取int32类型的键值。
		 */
		int32_t		get_int32(const char* domain, const char* section, const char* key, int32_t defVal = 0);

		/**
		 * @brief 获取int64值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param defVal 默认值
		 * @return 返回int64值
		 * 
		 * 获取int64类型的键值。
		 */
		int64_t		get_int64(const char* domain, const char* section, const char* key, int64_t defVal = 0);

		/**
		 * @brief 获取uint32值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param defVal 默认值
		 * @return 返回uint32值
		 * 
		 * 获取uint32类型的键值。
		 */
		uint32_t	get_uint32(const char* domain, const char* section, const char* key, uint32_t defVal = 0);

		/**
		 * @brief 获取uint64值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param defVal 默认值
		 * @return 返回uint64值
		 * 
		 * 获取uint64类型的键值。
		 */
		uint64_t	get_uint64(const char* domain, const char* section, const char* key, uint64_t defVal = 0);

		/**
		 * @brief 获取double值
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param defVal 默认值
		 * @return 返回double值
		 * 
		 * 获取double类型的键值。
		 */
		double		get_double(const char* domain, const char* section, const char* key, double defVal = 0);

	public:
		/**
		 * @brief 初始化命令队列
		 * @param name 命令队列名称
		 * @param isCmder 是否为命令下达者
		 * @param path 文件路径
		 * @return 返回初始化是否成功
		 * 
		 * 初始化命令队列，用于进程间命令传递。
		 */
		bool	init_cmder(const char* name, bool isCmder = false, const char* path = "");

		/**
		 * @brief 添加命令
		 * @param name 命令队列名称
		 * @param cmd 命令内容
		 * @return 返回是否添加成功
		 * 
		 * 向命令队列添加一条命令。
		 */
		bool	add_cmd(const char* name, const char* cmd);

		/**
		 * @brief 获取命令
		 * @param name 命令队列名称
		 * @param lastIdx 上次读取的索引（引用）
		 * @return 返回命令内容
		 * 
		 * 从命令队列获取下一条命令。
		 */
		const char*	get_cmd(const char* name, uint32_t& lastIdx);

	private:
		/**
		 * @brief 创建或验证键值对（内部方法）
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param vType 值类型
		 * @param secInfo 节信息指针（输出参数）
		 * @return 返回KeyInfo指针，如果失败返回nullptr
		 * 
		 * 创建或获取键值对，如果不存在则创建。
		 * 只有Master模式可以创建。
		 */
		void*	make_valid(const char* domain, const char* section, const char* key, ValueType vType, SecInfo* &secInfo);

		/**
		 * @brief 检查键值对是否有效（内部方法）
		 * @param domain 域名称
		 * @param section 节名称
		 * @param key 键名称
		 * @param vType 值类型
		 * @param secInfo 节信息指针（输出参数）
		 * @return 返回KeyInfo指针，如果不存在或类型不匹配返回nullptr
		 * 
		 * 检查键值对是否存在且类型匹配。
		 * 不创建新的键值对。
		 */
		void*	check_valid(const char* domain, const char* section, const char* key, ValueType vType, SecInfo* &secInfo);

	private:
		/**
		 * @struct ShmPair
		 * @brief 共享内存块配对结构
		 * 
		 * 存储一个共享内存块的所有相关信息，包括：
		 * - 内存映射文件指针
		 * - 共享内存块指针
		 * - Master/Slave标志
		 * - 块的缓存时间戳
		 * - 节和键的索引映射表
		 */
		typedef struct _ShmPair
		{
			MappedFilePtr	_domain;        // 内存映射文件智能指针，管理内存映射文件的生命周期
			ShmBlock*		_block;         // 共享内存块指针，指向映射的内存区域
			bool			_master;        // Master标志（布尔值），true表示Master模式，false表示Slave模式
			uint64_t		_blocktime;     // 块的缓存时间戳（64位无符号整数，毫秒），用于检测数据是否变化

			typedef wt_hashmap<std::string, KeyInfo*>	KVMap;  // 键值映射表类型定义，key为键名称，value为KeyInfo指针
			/**
			 * @struct KVPair
			 * @brief 键值对结构
			 * 
			 * 存储一个节的所有键信息，包括：
			 * - 节在数组中的索引
			 * - 键的映射表
			 */
			typedef struct _KVPair
			{
				uint32_t	_index;         // 节索引（32位无符号整数），节在_sections数组中的位置
				KVMap		_keys;          // 键映射表，key为键名称，value为KeyInfo指针
			} KVPair;
			typedef wt_hashmap<std::string, KVPair>	SectionMap;  // 节映射表类型定义，key为节名称，value为KVPair
			SectionMap	_sections;        // 节映射表，快速查找节和键

			/**
			 * @brief 构造函数
			 * 
			 * 初始化结构体：
			 * - _block初始化为nullptr
			 * - _master初始化为false
			 */
			_ShmPair() :_block(nullptr),_master(false)
			{
			}
		}ShmPair;
		typedef wt_hashmap<std::string, ShmPair>	ShmBlockMap;  // 共享内存块映射表类型定义，key为域名称，value为ShmPair
		ShmBlockMap		_shm_blocks;      // 共享内存块映射表，管理所有共享内存块

		/**
		 * @struct CmdPair
		 * @brief 命令块配对结构
		 * 
		 * 存储一个命令队列的所有相关信息，包括：
		 * - 内存映射文件指针
		 * - 命令块指针
		 * - 命令下达者标志
		 */
		typedef struct _CmdPair
		{
			MappedFilePtr	_domain;        // 内存映射文件智能指针，管理内存映射文件的生命周期
			CmdBlock*		_block;         // 命令块指针，指向映射的内存区域
			bool			_cmder;         // 命令下达者标志（布尔值），true表示下达命令，false表示接收命令
		} CmdPair;
		typedef wt_hashmap<std::string, CmdPair>	CmdBlockMap;  // 命令块映射表类型定义，key为命令队列名称，value为CmdPair
		CmdBlockMap		_cmd_blocks;      // 命令块映射表，管理所有命令队列
	};
}