/*!
 * \file ShareBlocks.cpp
 * \project	WonderTrader
 *
 * \brief ShareBlocks类的实现文件，实现共享内存块的所有功能
 * 
 * 本文件实现了ShareBlocks类的所有方法，包括：
 * - Master/Slave模式的初始化
 * - 共享内存块的创建和映射
 * - 键值对的分配、设置、获取
 * - 节和键的管理
 * - 命令队列功能
 * 
 * 技术实现：
 * - 使用Boost内存映射文件实现进程间共享
 * - 使用哈希映射表快速查找
 * - 支持自动清理无效的节
 * - 使用紧凑的内存布局提高效率
 */
#include "ShareBlocks.h"             // 包含类定义头文件
#include "../Share/BoostFile.hpp"    // Boost文件操作类
#include "../Share/TimeUtils.hpp"    // 时间工具函数
#include "../Share/StdUtils.hpp"     // 标准工具函数

using namespace shareblock;          // 使用shareblock命名空间

/**
 * @brief 初始化Master模式的实现
 * @param name 域名称
 * @param path 文件路径
 * @return 返回初始化是否成功
 * 
 * 初始化流程：
 * 1. 检查是否已初始化
 * 2. 确定文件名（如果path为空则使用name）
 * 3. 如果文件不存在，创建文件并设置大小为ShmBlock大小
 * 4. 映射文件到内存
 * 5. 清理无效的节（state为0或count为0）
 * 6. 加载已有的键到索引映射表
 * 7. 将节的state置为0（用于后续检测是否使用）
 */
bool ShareBlocks::init_master(const char* name, const char* path/* = ""*/)
{
	ShmPair& shm = (ShmPair&)_shm_blocks[name];  // 获取或创建共享内存块配对，使用域名称作为key
	if (shm._block != NULL)                      // 如果已初始化，直接返回成功
		return true;

	std::string filename = path;                 // 文件名字符串
	if (filename.empty())                        // 如果路径为空
		filename = name;                         // 使用域名称作为文件名

	if(!StdFile::exists(filename.c_str()))      // 如果文件不存在
	{
		BoostFile bf;                           // 创建Boost文件对象
		bf.create_new_file(filename.c_str());   // 创建新文件
		bf.truncate_file(sizeof(ShmBlock));     // 设置文件大小为ShmBlock结构体大小
		bf.close_file();                        // 关闭文件
	}

	shm._domain.reset(new BoostMappingFile);     // 创建内存映射文件对象
	shm._domain->map(filename.c_str());         // 将文件映射到内存
	shm._master = true;                          // 设置为Master模式
	shm._block = (ShmBlock*)shm._domain->addr();  // 获取映射内存的地址，转换为ShmBlock指针

	/*
	 *	By Wesley @ 2023.09.20
	 *	这里要做一个清理，如果state为0，则不再保留
	 */
	std::vector<SecInfo> aySecs;                 // 临时向量，存储有效的节
	for (uint32_t i = 0; i < shm._block->_count; i++)  // 遍历所有节
	{
		SecInfo& secInfo = shm._block->_sections[i];  // 获取第i个节
		if (secInfo._count == 0 || secInfo._state != 1)  // 如果节无效（count为0或state不为1）
			continue;                            // 跳过该节

		aySecs.emplace_back(secInfo);            // 将有效节添加到临时向量
	}

	if(aySecs.size() != shm._block->_count)     // 如果有无效的节需要清理
	{
		shm._block->_count = (uint32_t)aySecs.size();  // 更新节数量
		memset(shm._block->_sections, 0, sizeof(SecInfo)*MAX_SEC_CNT);  // 清空所有节数组
		if (shm._block->_count > 0)              // 如果有有效节
			memcpy(shm._block->_sections, aySecs.data(), sizeof(SecInfo)*shm._block->_count);  // 复制有效节回数组

		shm._blocktime = shm._block->_updatetime;  // 更新块的缓存时间戳
	}

	{
		//这里要做初始化，要把已经有的key加载进去
		for (uint32_t i = 0; i < shm._block->_count; i++)  // 遍历所有有效节
		{
			SecInfo& secInfo = shm._block->_sections[i];  // 获取第i个节
			if (secInfo._count == 0)            // 如果节没有键，跳过
				continue;

			//置零的目的看后面会不会用到
			//如果不会用到，那么就不会变成1
			//下次启动就会删掉这个section
			secInfo._state = 0;                 // 将节的state置为0，用于后续检测是否使用

			ShmPair::KVPair& kvPair = shm._sections[secInfo._name];  // 获取或创建节的键值对
			kvPair._index = i;                  // 设置节索引
			for (uint32_t j = 0; j < secInfo._count; j++)  // 遍历节的所有键
			{
				KeyInfo& key = secInfo._keys[j];  // 获取第j个键
				kvPair._keys[key._key] = &key;    // 将键添加到映射表，key为键名称，value为KeyInfo指针
			}
		}
	}

	return true;                                 // 返回成功
}

/**
 * @brief 初始化Slave模式的实现
 * @param name 域名称
 * @param path 文件路径
 * @return 返回初始化是否成功
 * 
 * 初始化流程：
 * 1. 检查是否已初始化
 * 2. 确定文件名（如果path为空则使用name）
 * 3. 检查文件是否存在，不存在则返回失败
 * 4. 映射文件到内存
 * 5. 设置为Slave模式
 * 6. 加载已有的键到索引映射表
 */
bool ShareBlocks::init_slave(const char* name, const char* path/* = ""*/)
{
	ShmPair& shm = (ShmPair&)_shm_blocks[name];  // 获取或创建共享内存块配对，使用域名称作为key
	if (shm._block != NULL)                      // 如果已初始化，直接返回成功
		return true;

	std::string filename = path;                 // 文件名字符串
	if (filename.empty())                        // 如果路径为空
		filename = name;                         // 使用域名称作为文件名

	if (!BoostFile::exists(filename.c_str()))   // 如果文件不存在
		return false;                            // 返回失败（Slave模式不能创建文件）

	shm._domain.reset(new BoostMappingFile);     // 创建内存映射文件对象
	shm._domain->map(filename.c_str());         // 将文件映射到内存
	shm._master = false;                         // 设置为Slave模式
	shm._block = (ShmBlock*)shm._domain->addr();  // 获取映射内存的地址，转换为ShmBlock指针
	shm._blocktime = shm._block->_updatetime;    // 保存块的更新时间戳，用于后续检测数据变化

	//slave模式下，应该需要加载一下
	//if (strcmp(shm._block->_flag, BLK_FLAG) == 0)
	{
		//这里要做初始化，要把已经有的key加载进去
		for (uint32_t i = 0; i < shm._block->_count; i++)  // 遍历所有节
		{
			SecInfo& secInfo = shm._block->_sections[i];  // 获取第i个节
			if (secInfo._count == 0 || secInfo._state != 1)  // 如果节无效（count为0或state不为1）
				continue;                            // 跳过该节

			ShmPair::KVPair& kvPair = shm._sections[secInfo._name];  // 获取或创建节的键值对
			kvPair._index = i;                  // 设置节索引
			for (uint32_t j = 0; j < secInfo._count; j++)  // 遍历节的所有键
			{
				KeyInfo& key = secInfo._keys[j];  // 获取第j个键
				kvPair._keys[key._key] = &key;     // 将键添加到映射表，key为键名称，value为KeyInfo指针
			}
		}
	}

	return true;                                 // 返回成功
}

/**
 * @brief 更新Slave数据的实现
 * @param name 域名称
 * @param bForce 是否强制更新
 * @return 返回是否更新成功
 * 
 * 更新流程：
 * 1. 检查共享内存块是否存在
 * 2. 检查数据是否变化（通过更新时间戳），如果未变化且不强制更新，则返回false
 * 3. 清空索引映射表
 * 4. 重新加载所有节和键到索引映射表
 * 5. 更新块的缓存时间戳
 */
bool ShareBlocks::update_slave(const char* name, bool bForce)
{
	ShmPair& shm = (ShmPair&)_shm_blocks[name];  // 获取共享内存块配对
	if (shm._block == NULL)                      // 如果块不存在，返回失败
		return false;

	if (shm._blocktime == shm._block->_updatetime && !bForce)  // 如果数据未变化且不强制更新
		return false;                            // 返回false，表示不需要更新

	{
		shm._sections.clear();                   // 清空节的索引映射表

		//这里要做初始化，要把已经有的key加载进去
		for (uint32_t i = 0; i < shm._block->_count; i++)  // 遍历所有节
		{
			SecInfo& secInfo = shm._block->_sections[i];  // 获取第i个节
			if (secInfo._count == 0)            // 如果节没有键，跳过
				continue;

			ShmPair::KVPair& kvPair = shm._sections[secInfo._name];  // 获取或创建节的键值对
			kvPair._index = i;                  // 设置节索引
			for (uint32_t j = 0; j < secInfo._count; j++)  // 遍历节的所有键
			{
				KeyInfo& key = secInfo._keys[j];  // 获取第j个键
				kvPair._keys[key._key] = &key;    // 将键添加到映射表
			}
		}
	}

	shm._blocktime = shm._block->_updatetime;    // 更新块的缓存时间戳

	return true;                                 // 返回成功
}

/**
 * @brief 释放Slave连接的实现
 * @param name 域名称
 * @return 返回是否释放成功
 * 
 * 释放流程：
 * 1. 查找共享内存块配对
 * 2. 检查是否为Slave模式（只有Slave可以释放）
 * 3. 清理所有资源（块指针、索引映射表、内存映射文件）
 * 4. 从映射表中移除
 */
bool ShareBlocks::release_slave(const char* name)
{
	auto it = _shm_blocks.find(name);            // 在映射表中查找共享内存块配对
	if (it == _shm_blocks.end())                 // 如果不存在，返回成功（已经释放）
		return true;

	ShmPair& shm = it->second;                   // 获取共享内存块配对引用

	//只有slave需要释放
	if (shm._master)                             // 如果是Master模式，不能释放
		return false;

	shm._block = NULL;                           // 清空块指针
	shm._sections.clear();                       // 清空节的索引映射表
	shm._domain.reset();                         // 释放内存映射文件（智能指针自动管理）
	shm._blocktime = 0;                          // 重置块的缓存时间戳

	_shm_blocks.erase(it);                       // 从映射表中移除
	return true;                                 // 返回成功
}

/**
 * @brief 获取节的更新时间的实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回更新时间戳
 * 
 * 获取流程：
 * 1. 查找共享内存块
 * 2. 查找节
 * 3. 返回节的更新时间戳
 */
uint64_t ShareBlocks::get_section_updatetime(const char* domain, const char* section)
{
	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回0
		return 0;

	const ShmPair& shm = (ShmPair&)it->second;  // 获取共享内存块配对引用
	auto sit = shm._sections.find(section);      // 在节的映射表中查找节
	if (sit == shm._sections.end())             // 如果不存在，返回0
		return 0;

	const ShmPair::KVPair& kvPair = sit->second;  // 获取键值对引用
	const SecInfo& secInfo = shm._block->_sections[kvPair._index];  // 获取节信息
	return secInfo._updatetime;                  // 返回节的更新时间戳
}

/**
 * @brief 提交节的实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回是否提交成功
 * 
 * 提交流程：
 * 1. 查找共享内存块和节
 * 2. 更新节的修改时间为当前时间
 * 3. 这会触发块的更新时间戳变化，通知Slave数据已更新
 */
bool ShareBlocks::commit_section(const char* domain, const char* section)
{
	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回失败
		return false;

	ShmPair& shm = (ShmPair&)it->second;         // 获取共享内存块配对引用
	auto sit = shm._sections.find(section);      // 在节的映射表中查找节
	if (sit == shm._sections.end())              // 如果不存在，返回失败
		return false;

	ShmPair::KVPair& kvPair = (ShmPair::KVPair&)sit->second;  // 获取键值对引用
	SecInfo& secInfo = shm._block->_sections[kvPair._index];  // 获取节信息引用
	secInfo._updatetime = TimeUtils::getLocalTimeNow();  // 更新节的修改时间为当前时间（毫秒）
	return true;                                 // 返回成功
}

/**
 * @brief 删除节的实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回是否删除成功
 * 
 * 删除流程：
 * 1. 查找共享内存块和节
 * 2. 从索引映射表中移除节
 * 3. 将节的state标记为2（已删除）
 * 4. 更新块的修改时间
 */
bool ShareBlocks::delete_section(const char* domain, const char*section)
{
	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回失败
		return false;

	ShmPair& shm = (ShmPair&)it->second;         // 获取共享内存块配对引用
	auto sit = shm._sections.find(section);      // 在节的映射表中查找节
	if (sit == shm._sections.end())              // 如果不存在，返回成功（已经删除）
		return true;

	uint32_t idx = sit->second._index;           // 获取节在数组中的索引
	shm._sections.erase(sit);                    // 从索引映射表中移除节
	shm._block->_sections[idx]._state = 2;       // 将节的state标记为2（已删除）
	shm._block->_updatetime = TimeUtils::getLocalTimeNow();  // 更新块的修改时间为当前时间
	return true;                                 // 返回成功
}

/**
 * @brief 创建或验证键值对的实现（内部方法）
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param vType 值类型
 * @param secInfo 节信息指针（输出参数）
 * @return 返回KeyInfo指针，如果失败返回nullptr
 * 
 * 创建流程：
 * 1. 查找共享内存块
 * 2. 如果节不存在，创建新节（只有Master可以创建）
 * 3. 如果键不存在，创建新键（只有Master可以创建）
 * 4. 检查空间限制（节数量、键数量、数据区域大小）
 * 5. 返回KeyInfo指针
 */
void* ShareBlocks::make_valid(const char* domain, const char* section, const char* key, ValueType vType, SecInfo* &secInfo)
{
	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回nullptr
		return nullptr;

	std::size_t len = SMVT_SIZES[vType];          // 获取数据类型的大小（字节）

	ShmPair& shm = (ShmPair&)it->second;         // 获取共享内存块配对引用
	KeyInfo* keyInfo = nullptr;                  // 键信息指针
	ShmPair::KVPair* kvPair = nullptr;           // 键值对指针
	auto sit = shm._sections.find(section);      // 在节的映射表中查找节
	if (sit == shm._sections.end())              // 如果节不存在
	{
		//如果不是master，就不能创建
		if (!shm._master)                         // 如果不是Master模式，不能创建节
			return nullptr;

		if (shm._block->_count == MAX_SEC_CNT)    // 如果已达到最大节数量
		{
			//已经没有额外的空间可以分配了
			return nullptr;                       // 返回nullptr
		}

		secInfo = &shm._block->_sections[shm._block->_count];  // 获取新的节位置
		wt_strcpy(secInfo->_name, section);       // 复制节名称
		secInfo->_updatetime = TimeUtils::getLocalTimeNow();  // 设置节的创建时间
		kvPair = &shm._sections[section];        // 创建节的键值对
		kvPair->_index = shm._block->_count;     // 设置节索引
		shm._block->_count++;                    // 增加节数量
	}
	else                                         // 如果节已存在
	{
		kvPair = (ShmPair::KVPair*)&sit->second;  // 获取键值对指针
	}

	secInfo = &shm._block->_sections[kvPair->_index];  // 获取节信息指针
	secInfo->_state = 1;                         // 设置节状态为1（生效）

	auto kit = kvPair->_keys.find(key);          // 在键的映射表中查找键
	if (kit == kvPair->_keys.end())              // 如果键不存在
	{
		//如果不是master，就不能创建
		if (!shm._master)                         // 如果不是Master模式，不能创建键
			return nullptr;

		if (secInfo->_count == MAX_KEY_CNT)      // 如果已达到最大键数量
			return nullptr;                       // 返回nullptr

		if (secInfo->_offset + len > 1024)       // 如果数据区域空间不足
			return nullptr;                       // 返回nullptr

		keyInfo = &secInfo->_keys[secInfo->_count];  // 获取新的键位置
		wt_strcpy(keyInfo->_key, key);          // 复制键名称
		keyInfo->_updatetime = TimeUtils::getLocalTimeNow();  // 设置键的创建时间
		keyInfo->_offset = secInfo->_offset;    // 设置键的数据偏移量
		kvPair->_keys[key] = keyInfo;            // 将键添加到映射表

		//字符串固定最大长度为64
		secInfo->_count++;                       // 增加键数量
		secInfo->_offset += (uint32_t)len;       // 更新下一个可分配地址的偏移量
	}
	else                                         // 如果键已存在
	{
		keyInfo = kit->second;                   // 获取键信息指针
	}

	return keyInfo;                              // 返回键信息指针
}

/**
 * @brief 检查键值对是否有效的实现（内部方法）
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param vType 值类型
 * @param secInfo 节信息指针（输出参数）
 * @return 返回KeyInfo指针，如果不存在或类型不匹配返回nullptr
 * 
 * 检查流程：
 * 1. 查找共享内存块
 * 2. 查找节，如果不存在返回nullptr
 * 3. 查找键，如果不存在返回nullptr
 * 4. 检查键的类型是否匹配，如果不匹配返回nullptr
 * 5. 返回KeyInfo指针
 */
void* ShareBlocks::check_valid(const char* domain, const char* section, const char* key, ValueType vType, SecInfo* &secInfo)
{
	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回nullptr
		return nullptr;

	ShmPair& shm = (ShmPair&)it->second;         // 获取共享内存块配对引用
	KeyInfo* keyInfo = nullptr;                  // 键信息指针
	ShmPair::KVPair* kvPair = nullptr;           // 键值对指针
	auto sit = shm._sections.find(section);      // 在节的映射表中查找节
	if (sit == shm._sections.end())              // 如果节不存在
	{
		return nullptr;                           // 返回nullptr
	}
	else                                         // 如果节存在
	{
		kvPair = (ShmPair::KVPair*)&sit->second;  // 获取键值对指针
	}

	secInfo = &shm._block->_sections[kvPair->_index];  // 获取节信息指针

	auto kit = kvPair->_keys.find(key);          // 在键的映射表中查找键
	if (kit == kvPair->_keys.end())              // 如果键不存在
	{
		return nullptr;                           // 返回nullptr
	}
	else                                         // 如果键存在
	{
		keyInfo = kit->second;                   // 获取键信息指针
		if (keyInfo->_type != vType)             // 如果键的类型不匹配
			return nullptr;                       // 返回nullptr

		return keyInfo;                          // 返回键信息指针
	}
}

/**
 * @brief 获取节列表的实现
 * @param domain 域名称
 * @return 返回节名称向量
 * 
 * 获取流程：
 * 1. 查找共享内存块
 * 2. 遍历所有节，收集状态为1（生效）的节名称
 * 3. 返回节名称向量
 */
std::vector<std::string> ShareBlocks::get_sections(const char* domain)
{
	static std::vector<std::string> emptyRet;    // 静态空向量，用于返回空结果

	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回空向量
		return emptyRet;

	std::vector<std::string> ret;                // 结果向量
	const ShmPair& shm = it->second;             // 获取共享内存块配对引用
	for (uint32_t i = 0; i < shm._block->_count; i++)  // 遍历所有节
	{
		if (shm._block->_sections[i]._state != 1)  // 如果节状态不为1（无效或已删除）
			continue;                            // 跳过该节

		ret.emplace_back(shm._block->_sections[i]._name);  // 将节名称添加到结果向量
	}

	return std::move(ret);                        // 移动返回结果向量（避免拷贝）
}

/**
 * @brief 获取键列表的实现
 * @param domain 域名称
 * @param section 节名称
 * @return 返回键信息指针向量
 * 
 * 获取流程：
 * 1. 查找共享内存块
 * 2. 如果索引映射表与块中的节数量不一致，强制更新Slave
 * 3. 查找节
 * 4. 收集节中所有键的信息指针
 * 5. 返回键信息指针向量
 */
std::vector<KeyInfo*> ShareBlocks::get_keys(const char* domain, const char* section)
{
	static std::vector<KeyInfo*> emptyRet;       // 静态空向量，用于返回空结果

	auto it = _shm_blocks.find(domain);          // 在映射表中查找共享内存块
	if (it == _shm_blocks.end())                 // 如果不存在，返回空向量
		return emptyRet;

	const ShmPair& shm = it->second;             // 获取共享内存块配对引用
	if(shm._sections.size() != shm._block->_count)  // 如果索引映射表与块中的节数量不一致（数据可能已更新）
	{
		update_slave(domain, true);              // 强制更新Slave，重新加载数据
	}

	auto sit = shm._sections.find(section);      // 在节的映射表中查找节
	if (sit == shm._sections.end())              // 如果不存在，返回空向量
		return emptyRet;

	std::vector<KeyInfo*> ret;                   // 结果向量
	const ShmPair::KVPair& kvPair = sit->second;  // 获取键值对引用
	for (auto& v : kvPair._keys)                 // 遍历键映射表
	{
		ret.emplace_back(v.second);              // 将键信息指针添加到结果向量
	}

	return std::move(ret);                       // 移动返回结果向量（避免拷贝）
}

/**
 * @brief 分配字符串类型键值对的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回字符串指针
 * 
 * 分配流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 如果键是新分配的（type为0）或强制写入，使用初始值填充
 * 3. 返回字符串指针
 */
const char* ShareBlocks::allocate_string(const char* domain, const char* section, const char* key, const char* initVal /* = "" */, bool bForceWrite/* = false*/)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_STRING, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回NULL
		return NULL;

	if (keyInfo->_type == 0 || bForceWrite)      // 如果键是新分配的（type为0）或强制写入
	{
		//如果type为0，说明是新分配的，则用初始值填充
		keyInfo->_type = SMVT_STRING;            // 设置键的类型为字符串
		wt_strcpy(secInfo->_data + keyInfo->_offset, initVal, SMVT_SIZES[SMVT_STRING]);  // 复制初始值到数据区域（最多64字节）
	}

	return (secInfo->_data + keyInfo->_offset);  // 返回字符串指针
}

/**
 * @brief 分配int32类型键值对的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回int32指针
 * 
 * 分配流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 如果键是新分配的或强制写入，使用初始值填充
 * 3. 返回int32指针
 */
int32_t* ShareBlocks::allocate_int32(const char* domain, const char* section, const char* key, int32_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_INT32, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回NULL
		return NULL;

	if (keyInfo->_type == 0 || bForceWrite)      // 如果键是新分配的（type为0）或强制写入
	{
		//如果type为0，说明是新分配的，则用初始值填充
		keyInfo->_type = SMVT_INT32;             // 设置键的类型为int32
		*secInfo->get<int32_t>(keyInfo->_offset) = initVal;  // 将初始值写入数据区域
	}

	return secInfo->get<int32_t>(keyInfo->_offset);  // 返回int32指针
}

/**
 * @brief 分配int64类型键值对的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回int64指针
 * 
 * 分配流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 如果键是新分配的或强制写入，使用初始值填充
 * 3. 返回int64指针
 */
int64_t* ShareBlocks::allocate_int64(const char* domain, const char* section, const char* key, int64_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_INT64, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回NULL
		return NULL;

	if (keyInfo->_type == 0 || bForceWrite)      // 如果键是新分配的（type为0）或强制写入
	{
		//如果type为0，说明是新分配的，则用初始值填充
		keyInfo->_type = SMVT_INT64;             // 设置键的类型为int64
		*secInfo->get<int64_t>(keyInfo->_offset) = initVal;  // 将初始值写入数据区域
	}

	return secInfo->get<int64_t>(keyInfo->_offset);  // 返回int64指针
}

/**
 * @brief 分配uint32类型键值对的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回uint32指针
 * 
 * 分配流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 如果键是新分配的或强制写入，使用初始值填充
 * 3. 返回uint32指针
 */
uint32_t* ShareBlocks::allocate_uint32(const char* domain, const char* section, const char* key, uint32_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_UINT32, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回NULL
		return NULL;

	if (keyInfo->_type == 0 || bForceWrite)      // 如果键是新分配的（type为0）或强制写入
	{
		//如果type为0，说明是新分配的，则用初始值填充
		keyInfo->_type = SMVT_UINT32;            // 设置键的类型为uint32
		*secInfo->get<uint32_t>(keyInfo->_offset) = initVal;  // 将初始值写入数据区域
	}

	return secInfo->get<uint32_t>(keyInfo->_offset);  // 返回uint32指针
}

/**
 * @brief 分配uint64类型键值对的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回uint64指针
 * 
 * 分配流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 如果键是新分配的或强制写入，使用初始值填充
 * 3. 返回uint64指针
 */
uint64_t* ShareBlocks::allocate_uint64(const char* domain, const char* section, const char* key, uint64_t initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_UINT64, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回NULL
		return NULL;

	if (keyInfo->_type == 0 || bForceWrite)      // 如果键是新分配的（type为0）或强制写入
	{
		//如果type为0，说明是新分配的，则用初始值填充
		keyInfo->_type = SMVT_UINT64;            // 设置键的类型为uint64
		*secInfo->get<uint64_t>(keyInfo->_offset) = initVal;  // 将初始值写入数据区域
	}

	return secInfo->get<uint64_t>(keyInfo->_offset);  // 返回uint64指针
}

/**
 * @brief 分配double类型键值对的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param initVal 初始值
 * @param bForceWrite 是否强制写入
 * @return 返回double指针
 * 
 * 分配流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 如果键是新分配的或强制写入，使用初始值填充
 * 3. 返回double指针
 */
double* ShareBlocks::allocate_double(const char* domain, const char* section, const char* key, double initVal /* = 0 */, bool bForceWrite/* = false*/)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_DOUBLE, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回NULL
		return NULL;

	if(keyInfo->_type == 0 || bForceWrite)       // 如果键是新分配的（type为0）或强制写入
	{
		//如果type为0，说明是新分配的，则用初始值填充
		keyInfo->_type = SMVT_DOUBLE;            // 设置键的类型为double
		*secInfo->get<double>(keyInfo->_offset) = initVal;  // 将初始值写入数据区域
	}

	return secInfo->get<double>(keyInfo->_offset);  // 返回double指针
}

/**
 * @brief 设置字符串值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 设置流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 设置键的类型
 * 3. 复制值到数据区域
 */
bool ShareBlocks::set_string(const char* domain, const char* section, const char* key, const char* val)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_STRING, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回false
		return false;

	keyInfo->_type = SMVT_STRING;                // 设置键的类型为字符串
	wt_strcpy(secInfo->_data + keyInfo->_offset, val, SMVT_SIZES[SMVT_STRING]);  // 复制值到数据区域（最多64字节）

	return true;                                 // 返回成功
}

/**
 * @brief 设置int32值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 设置流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 设置键的类型
 * 3. 写入值到数据区域
 */
bool ShareBlocks::set_int32(const char* domain, const char* section, const char* key, int32_t val)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_INT32, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回false
		return false;

	keyInfo->_type = SMVT_INT32;                 // 设置键的类型为int32
	*secInfo->get<int32_t>(keyInfo->_offset) = val;  // 写入值到数据区域

	return true;                                 // 返回成功
}

/**
 * @brief 设置int64值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 设置流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 设置键的类型
 * 3. 写入值到数据区域
 */
bool ShareBlocks::set_int64(const char* domain, const char* section, const char* key, int64_t val)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_INT64, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回false
		return false;

	keyInfo->_type = SMVT_INT64;                 // 设置键的类型为int64
	*secInfo->get<int64_t>(keyInfo->_offset) = val;  // 写入值到数据区域

	return true;                                 // 返回成功
}

/**
 * @brief 设置uint32值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 设置流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 设置键的类型
 * 3. 写入值到数据区域
 */
bool ShareBlocks::set_uint32(const char* domain, const char* section, const char* key, uint32_t val)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_UINT32, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回false
		return false;

	keyInfo->_type = SMVT_UINT32;                // 设置键的类型为uint32
	*secInfo->get<uint32_t>(keyInfo->_offset) = val;  // 写入值到数据区域

	return true;                                 // 返回成功
}

/**
 * @brief 设置uint64值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 设置流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 设置键的类型
 * 3. 写入值到数据区域
 */
bool ShareBlocks::set_uint64(const char* domain, const char* section, const char* key, uint64_t val)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_UINT64, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回false
		return false;

	keyInfo->_type = SMVT_UINT64;                // 设置键的类型为uint64
	*secInfo->get<uint64_t>(keyInfo->_offset) = val;  // 写入值到数据区域

	return true;                                 // 返回成功
}

/**
 * @brief 设置double值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param val 值
 * @return 返回是否设置成功
 * 
 * 设置流程：
 * 1. 调用make_valid创建或获取键值对
 * 2. 设置键的类型
 * 3. 写入值到数据区域
 */
bool ShareBlocks::set_double(const char* domain, const char* section, const char* key, double val)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)make_valid(domain, section, key, SMVT_DOUBLE, secInfo);  // 创建或获取键值对
	if (keyInfo == nullptr)                      // 如果失败，返回false
		return false;

	keyInfo->_type = SMVT_DOUBLE;                // 设置键的类型为double
	*secInfo->get<double>(keyInfo->_offset) = val;  // 写入值到数据区域

	return true;                                 // 返回成功
}

/**
 * @brief 获取字符串值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回字符串指针
 * 
 * 获取流程：
 * 1. 调用check_valid检查键值对是否存在且类型匹配
 * 2. 如果不存在，返回默认值
 * 3. 返回字符串指针
 */
const char* ShareBlocks::get_string(const char* domain, const char* section, const char* key, const char* defVal /* = "" */)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)check_valid(domain, section, key, SMVT_STRING, secInfo);  // 检查键值对是否有效
	if (keyInfo == nullptr)                      // 如果不存在或类型不匹配，返回默认值
		return defVal;

	return (const char*)(secInfo->_data + keyInfo->_offset);  // 返回字符串指针
}

/**
 * @brief 获取int32值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回int32值
 * 
 * 获取流程：
 * 1. 调用check_valid检查键值对是否存在且类型匹配
 * 2. 如果不存在，返回默认值
 * 3. 从数据区域读取值并返回
 */
int32_t ShareBlocks::get_int32(const char* domain, const char* section, const char* key, int32_t defVal /* = 0 */)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)check_valid(domain, section, key, SMVT_INT32, secInfo);  // 检查键值对是否有效
	if (keyInfo == nullptr)                      // 如果不存在或类型不匹配，返回默认值
		return defVal;

	return *(int32_t*)(secInfo->_data + keyInfo->_offset);  // 从数据区域读取int32值并返回
}

/**
 * @brief 获取uint32值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回uint32值
 * 
 * 获取流程：
 * 1. 调用check_valid检查键值对是否存在且类型匹配
 * 2. 如果不存在，返回默认值
 * 3. 从数据区域读取值并返回
 */
uint32_t ShareBlocks::get_uint32(const char* domain, const char* section, const char* key, uint32_t defVal /* = 0 */)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)check_valid(domain, section, key, SMVT_UINT32, secInfo);  // 检查键值对是否有效
	if (keyInfo == nullptr)                      // 如果不存在或类型不匹配，返回默认值
		return defVal;

	return *(uint32_t*)(secInfo->_data + keyInfo->_offset);  // 从数据区域读取uint32值并返回
}

/**
 * @brief 获取int64值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回int64值
 * 
 * 获取流程：
 * 1. 调用check_valid检查键值对是否存在且类型匹配
 * 2. 如果不存在，返回默认值
 * 3. 从数据区域读取值并返回
 */
int64_t ShareBlocks::get_int64(const char* domain, const char* section, const char* key, int64_t defVal /* = 0 */)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)check_valid(domain, section, key, SMVT_INT64, secInfo);  // 检查键值对是否有效
	if (keyInfo == nullptr)                      // 如果不存在或类型不匹配，返回默认值
		return defVal;

	return *(int64_t*)(secInfo->_data + keyInfo->_offset);  // 从数据区域读取int64值并返回
}

/**
 * @brief 获取uint64值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回uint64值
 * 
 * 获取流程：
 * 1. 调用check_valid检查键值对是否存在且类型匹配
 * 2. 如果不存在，返回默认值
 * 3. 从数据区域读取值并返回
 */
uint64_t ShareBlocks::get_uint64(const char* domain, const char* section, const char* key, uint64_t defVal /* = 0 */)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)check_valid(domain, section, key, SMVT_UINT64, secInfo);  // 检查键值对是否有效
	if (keyInfo == nullptr)                      // 如果不存在或类型不匹配，返回默认值
		return defVal;

	return *(uint64_t*)(secInfo->_data + keyInfo->_offset);  // 从数据区域读取uint64值并返回
}

/**
 * @brief 获取double值的实现
 * @param domain 域名称
 * @param section 节名称
 * @param key 键名称
 * @param defVal 默认值
 * @return 返回double值
 * 
 * 获取流程：
 * 1. 调用check_valid检查键值对是否存在且类型匹配
 * 2. 如果不存在，返回默认值
 * 3. 从数据区域读取值并返回
 */
double ShareBlocks::get_double(const char* domain, const char* section, const char* key, double defVal /* = 0 */)
{
	SecInfo* secInfo = nullptr;                  // 节信息指针（输出参数）
	KeyInfo* keyInfo = (KeyInfo*)check_valid(domain, section, key, SMVT_DOUBLE, secInfo);  // 检查键值对是否有效
	if (keyInfo == nullptr)                      // 如果不存在或类型不匹配，返回默认值
		return defVal;

	return *(double*)(secInfo->_data + keyInfo->_offset);  // 从数据区域读取double值并返回
}

/**
 * @brief 初始化命令队列的实现
 * @param name 命令队列名称
 * @param isCmder 是否为命令下达者
 * @param path 文件路径
 * @return 返回初始化是否成功
 * 
 * 初始化流程：
 * 1. 检查是否已初始化
 * 2. 确定文件名（如果path为空则使用".cmd"）
 * 3. 如果文件不存在，创建文件并设置大小为CmdBlock大小
 * 4. 映射文件到内存
 * 5. 如果容量为0，使用placement new初始化CmdBlock
 * 6. 如果是命令下达者，设置进程ID
 * 7. 调整读写索引（处理索引溢出）
 */
bool ShareBlocks::init_cmder(const char* name, bool isCmder /* = false */, const char* path /* = "" */)
{
	CmdPair& cmdPair = _cmd_blocks[name];        // 获取或创建命令块配对，使用命令队列名称作为key
	if (cmdPair._block != NULL)                   // 如果已初始化，直接返回成功
		return true;

	std::string filename = path;                  // 文件名字符串
	if (filename.empty())                        // 如果路径为空
		filename = ".cmd";                        // 使用默认文件名".cmd"

	if (!StdFile::exists(filename.c_str()))      // 如果文件不存在
	{
		BoostFile bf;                            // 创建Boost文件对象
		bf.create_new_file(filename.c_str());    // 创建新文件
		bf.truncate_file(sizeof(CmdBlock));      // 设置文件大小为CmdBlock结构体大小
		bf.close_file();                         // 关闭文件
	}

	cmdPair._domain.reset(new BoostMappingFile);  // 创建内存映射文件对象
	cmdPair._domain->map(filename.c_str());      // 将文件映射到内存
	cmdPair._cmder = isCmder;                    // 设置命令下达者标志
	cmdPair._block = (CmdBlock*)cmdPair._domain->addr();  // 获取映射内存的地址，转换为CmdBlock指针
	if(cmdPair._block->_capacity == 0)           // 如果容量为0（新创建的文件）
		new(cmdPair._domain->addr()) CmdBlock();  // 使用placement new初始化CmdBlock结构体

	if(cmdPair._cmder)                           // 如果是命令下达者
#ifdef _MSC_VER                                  // Windows平台
		cmdPair._block->_cmdpid = _getpid();     // 获取当前进程ID（Windows API）
#else                                            // Unix/Linux平台
		cmdPair._block->_cmdpid = getpid();      // 获取当前进程ID（Unix API）
#endif
  
	
	//启动的时候都做一下偏移
	cmdPair._block->_writable %= cmdPair._block->_capacity;  // 将可写索引取模，处理索引溢出
	if(cmdPair._block->_readable != UINT32_MAX)  // 如果可读索引已初始化（不为UINT32_MAX）
	{
		cmdPair._block->_readable %= cmdPair._block->_capacity;  // 将可读索引取模
		if (cmdPair._block->_readable > cmdPair._block->_writable)  // 如果可读索引大于可写索引（说明已循环一圈）
			cmdPair._block->_writable += cmdPair._block->_capacity;  // 将可写索引加上容量，保持正确的顺序
	}

	return true;                                 // 返回成功
}

/**
 * @brief 添加命令的实现
 * @param name 命令队列名称
 * @param cmd 命令内容
 * @return 返回是否添加成功
 * 
 * 添加流程：
 * 1. 查找命令块配对
 * 2. 检查进程ID是否匹配（只有创建者可以添加命令）
 * 3. 增加可写索引
 * 4. 计算实际索引（取模）
 * 5. 写入命令数据
 * 6. 更新可读索引
 */
bool ShareBlocks::add_cmd(const char* name, const char* cmd)
{
	auto it = _cmd_blocks.find(name);            // 在映射表中查找命令块配对
	if (it == _cmd_blocks.end())                 // 如果不存在，返回失败
		return false;

	CmdPair& cmdPair = it->second;               // 获取命令块配对引用

	if (cmdPair._block == NULL)                  // 如果块不存在，返回失败
		return false;

#ifdef _MSC_VER                                  // Windows平台
    if (cmdPair._block->_cmdpid != _getpid())    // 如果进程ID不匹配（不是创建者）
#else                                            // Unix/Linux平台
	if (cmdPair._block->_cmdpid != getpid())     // 如果进程ID不匹配（不是创建者）
#endif
	
		return false;                             // 返回失败（只有创建者可以添加命令）

	/*
	 *	先移动写的下标，然后写入数据
	 *	写完了以后，再移动读的下标
	 */
	uint32_t wIdx = cmdPair._block->_writable++;  // 获取当前可写索引并递增（原子操作，volatile保证可见性）
	uint32_t realIdx = wIdx % cmdPair._block->_capacity;  // 计算实际索引（取模，实现循环缓冲区）
	cmdPair._block->_commands[realIdx]._state = 0;  // 设置命令状态为0（未读）
	strcpy(cmdPair._block->_commands[realIdx]._command, cmd);  // 复制命令内容到命令数组
	cmdPair._block->_readable = wIdx;            // 更新可读索引为当前可写索引（表示有新命令可读）
	return true;                                 // 返回成功
}

/**
 * @brief 获取命令的实现
 * @param name 命令队列名称
 * @param lastIdx 上次读取的索引（引用，用于跟踪读取位置）
 * @return 返回命令内容，如果没有新命令返回空字符串
 * 
 * 获取流程：
 * 1. 查找命令块配对
 * 2. 检查是否为命令下达者（下达者不需要获取命令）
 * 3. 处理各种索引状态：
 *    - 如果可读索引为UINT32_MAX（未初始化），设置lastIdx为999999并返回空
 *    - 如果lastIdx为UINT32_MAX（首次调用），设置为当前可读索引并返回空
 *    - 如果lastIdx为999999（刚启动），重置为0并返回第一条命令
 *    - 如果lastIdx大于等于可读索引（已读完），返回空
 *    - 否则，递增lastIdx并返回下一条命令
 */
const char* ShareBlocks::get_cmd(const char* name, uint32_t& lastIdx)
{
	auto it = _cmd_blocks.find(name);            // 在映射表中查找命令块配对
	if (it == _cmd_blocks.end())                 // 如果不存在，返回空字符串
		return "";

	CmdPair& cmdPair = it->second;               // 获取命令块配对引用

	if (cmdPair._block == NULL)                  // 如果块不存在，返回空字符串
		return "";

	//指令下达者就不需要获取指令了
	if (cmdPair._cmder)                          // 如果是命令下达者，不需要获取命令
		return "";

	//说明刚启动，之前的命令全部作废
	if (cmdPair._block->_readable == UINT32_MAX)  // 如果可读索引未初始化（刚启动，之前的命令作废）
	{
		lastIdx = 999999;                         // 设置lastIdx为999999（特殊标记）
		return "";                                // 返回空字符串
	}
	else if (lastIdx == UINT32_MAX && cmdPair._block->_readable != UINT32_MAX)  // 如果lastIdx未初始化（首次调用）
	{
		lastIdx = cmdPair._block->_readable;      // 设置lastIdx为当前可读索引
		return "";                                // 返回空字符串（不读取旧命令）
	}
	else if(lastIdx == 999999 && cmdPair._block->_readable != UINT32_MAX)  // 如果lastIdx为999999（刚启动状态）
	{
		lastIdx = 0;                              // 重置lastIdx为0
		return cmdPair._block->_commands[lastIdx]._command;  // 返回第一条命令
	}
	else if(lastIdx >= cmdPair._block->_readable)  // 如果lastIdx大于等于可读索引（已读完所有命令）
	{
		return "";                                // 返回空字符串
	}
	else                                         // 否则，还有未读的命令
	{
		lastIdx++;                                // 递增lastIdx
		return cmdPair._block->_commands[lastIdx % cmdPair._block->_capacity]._command;  // 返回下一条命令（取模实现循环缓冲区）
	}
}