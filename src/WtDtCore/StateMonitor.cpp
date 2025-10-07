/*!
 * \file StateMonitor.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 交易时段状态监控器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件是StateMonitor类的具体实现，提供了复杂的交易时段状态管理逻辑。
 * 这是WonderTrader数据传输核心中最复杂的模块之一，需要处理：
 * - 多个交易时段的并发状态管理
 * - 复杂的时间偏移计算（夜盘跨日等）
 * - 交易日历的查询和验证
 * - 状态转换的精确时机控制
 * 
 * 核心实现逻辑：
 * 
 * 1. 配置加载与初始化（initialize方法）：
 *    - 解析状态配置文件（JSON格式）
 *    - 为每个交易时段创建StateInfo
 *    - 从WTSSessionInfo提取时间区间
 *    - 处理时间偏移（分钟 ↔ 时间格式转换）
 *    - 初始化交易日信息
 * 
 * 2. 状态监控线程（run方法）：
 *    - 每秒执行一次状态检查
 *    - 获取当前日期和时间
 *    - 遍历所有交易时段
 *    - 根据当前状态执行对应的转换逻辑
 *    - 记录状态转换日志
 * 
 * 3. 状态转换逻辑（8种状态的处理）：
 *    每种状态都有独立的转换逻辑，考虑多种条件：
 *    - 当前时间
 *    - 偏移时间
 *    - 是否节假日
 *    - 是否在交易区间内
 *    - 是否所有品种都休市
 * 
 * 4. 时间处理技巧：
 *    - 偏移时间：处理夜盘跨日（如21:00偏移为-300分钟）
 *    - 时间转换：HHMM ↔ 分钟数 ↔ HHMM
 *    - 前一日：处理夜盘后半夜的情况
 *    - 偏移日期：考虑时间偏移后的实际日期
 * 
 * 关键算法详解：
 * 
 * 1. 时间偏移转换算法：
 *    HHMM格式的时间需要进行偏移计算时：
 *    a) HHMM → 分钟数：time/100*60 + time%100
 *    b) 应用偏移：minutes ± offset
 *    c) 分钟数 → HHMM：minutes/60*100 + minutes%60
 * 
 * 2. 节假日判断算法：
 *    考虑夜盘的复杂性：
 *    a) 如果时间往后偏移（夜盘）：
 *       - 当前日期不是交易日 且
 *       - 不处于夜盘后半夜（交易时间且昨天是交易日）
 *    b) 如果时间不偏移或往前偏移：
 *       - 偏移后的日期不是交易日
 * 
 * 3. 状态转换决策树：
 *    根据当前状态、当前时间、偏移时间、节假日等条件，
 *    决定是否转换到新状态。转换条件非常复杂，需要
 *    仔细理解代码中的逻辑。
 * 
 * 复杂场景处理：
 * 
 * 1. 夜盘场景（时间偏移为负）：
 *    - 21:00的夜盘属于下一个交易日
 *    - 需要检查下一日是否是交易日
 *    - 处理跨日的时间计算
 * 
 * 2. 中途休盘场景：
 *    - 上午11:30-13:00休盘
 *    - 状态在RECEIVING和PAUSED间切换
 *    - 准确识别休盘时间段
 * 
 * 3. 多品种场景：
 *    - 同一时段可能包含多个品种
 *    - 需要检查是否所有品种都休市
 *    - 只要有一个品种交易，时段就不是节假日
 * 
 * 4. 盘后处理场景：
 *    - 收盘后等待到处理时间
 *    - 触发数据转储
 *    - 转换到已处理状态
 * 
 * 性能优化：
 * - 每秒检查一次，精度足够且开销小
 * - 使用2ms的睡眠粒度，避免CPU空转
 * - 状态转换时才输出日志，减少日志量
 * 
 * 错误处理：
 * - 配置文件不存在时记录错误并返回失败
 * - 交易时段信息不存在时跳过并记录错误
 * - 线程异常时能够安全退出
 */

#include "StateMonitor.h"                       // 包含StateMonitor类定义
#include "DataManager.h"                        // 包含DataManager类定义

#include "../Share/TimeUtils.hpp"               // 包含时间工具类
#include "../Includes/WTSContractInfo.hpp"      // 包含合约信息类
#include "../Includes/WTSSessionInfo.hpp"       // 包含交易时段信息类
#include "../Includes/WTSVariant.hpp"           // 包含配置参数类

#include "../WTSTools/WTSBaseDataMgr.h"         // 包含基础数据管理器
#include "../WTSTools/WTSLogger.h"              // 包含日志系统
#include "../WTSUtils/WTSCfgLoader.h"           // 包含配置加载器


/**
 * @brief 构造函数实现
 * 
 * 使用初始化列表初始化所有成员变量为默认值。
 */
StateMonitor::StateMonitor()
	: _stopped(false)                   // 停止标志初始化为false（运行状态）
	, _bd_mgr(NULL)                     // 基础数据管理器指针初始化为空
	, _dt_mgr(NULL)                     // 数据管理器指针初始化为空
	// _map使用默认构造函数，初始化为空映射表
	// _thrd使用默认构造函数，初始化为空智能指针
{
}


/**
 * @brief 析构函数实现
 * 
 * 析构函数为空。资源清理应该通过stop()方法完成。
 * 
 * @warning 析构前应该调用stop()停止监控线程
 */
StateMonitor::~StateMonitor()
{
}

/**
 * @brief 初始化状态监控器实现
 * 
 * 这是StateMonitor最复杂的方法之一，负责从配置文件加载状态控制规则，
 * 并为每个交易时段创建和初始化状态信息。
 * 
 * 实现步骤详解：
 * 
 * 步骤1：保存依赖组件引用
 * 步骤2：检查配置文件是否存在
 * 步骤3：加载配置文件
 * 步骤4：遍历所有交易时段配置
 * 步骤5：为每个时段创建StateInfo
 * 步骤6：提取并转换交易时间区间
 * 步骤7：初始化交易日信息
 * 
 * @param filename 状态配置文件路径
 * @param bdMgr 基础数据管理器指针
 * @param dtMgr 数据管理器指针
 * @return bool 初始化成功返回true，失败返回false
 */
bool StateMonitor::initialize(const char* filename, WTSBaseDataMgr* bdMgr, DataManager* dtMgr)
{
	// 步骤1：保存依赖组件的引用
	_bd_mgr = bdMgr;                            // 保存基础数据管理器指针
	_dt_mgr = dtMgr;                            // 保存数据管理器指针

	// 步骤2：检查配置文件是否存在
	if (!StdFile::exists(filename))             // 使用StdFile工具类检查文件
	{
		// 配置文件不存在，记录错误并返回失败
		WTSLogger::error("State config file {} not exists", filename);
		return false;
	}

	// 步骤3：加载配置文件
	// load_from_file：支持JSON、YAML等格式
	// 返回WTSVariant对象，类似于动态类型的容器
	WTSVariant* config = WTSCfgLoader::load_from_file(filename);
	if (config == NULL)                         // 如果加载失败
	{
		// 可能是文件格式错误或内容无效
		WTSLogger::error("Loading state config failed");
		return false;
	}

	// 步骤4：获取所有交易时段的名称（配置文件的顶层key）
	// memberNames()返回所有成员名称的集合
	auto keys = config->memberNames();
	
	// 遍历所有交易时段配置
	for (const std::string& sid : keys)         // sid: session id（交易时段ID）
	{
		// 获取该时段的配置对象
		WTSVariant* jItem = config->get(sid.c_str());

		// 从BaseDataMgr获取该时段的详细信息
		// WTSSessionInfo包含：交易时间、偏移分钟、节假日规则等
		WTSSessionInfo* ssInfo = _bd_mgr->getSession(sid.c_str());
		if (ssInfo == NULL)                     // 如果时段信息不存在
		{
			// 配置文件中定义的时段在基础数据中找不到
			// 记录错误并跳过该时段
			WTSLogger::error("Trading session template [{}] not exists,state control rule skipped", sid);
			continue;                           // 继续处理下一个时段
		}

		// 步骤5：创建StateInfo对象（使用智能指针自动管理内存）
		StatePtr stateInfo(new StateInfo);
		
		// 设置交易时段信息指针
		stateInfo->_sInfo = ssInfo;
		
		// 从配置中读取时间参数（单位：HHMM格式）
		stateInfo->_init_time = jItem->getUInt32("inittime");	    // 初始化时间，如0830（8:30）
		stateInfo->_close_time = jItem->getUInt32("closetime");	    // 收盘时间，如1505（15:05）
		stateInfo->_proc_time = jItem->getUInt32("proctime");	    // 盘后处理时间，如1530（15:30）

		// 复制时段ID到字符数组
		strcpy(stateInfo->_session, sid.c_str());

		// 步骤6a：提取集合竞价时间区间
		// getAuctionSections()返回集合竞价的时间段（已经过偏移处理）
		// 注意：这里面是偏移过的时间，要注意了!!!
		const auto& auctions = ssInfo->getAuctionSections();
		
		for(const auto& secInfo : auctions)     // 遍历所有集合竞价时段
		{
			uint32_t stime = secInfo.first;     // 开始时间（偏移后的）
			uint32_t etime = secInfo.second;    // 结束时间（偏移后的）

			// 时间格式转换：HHMM → 分钟数
			// 例如：0930 → 9*60+30 = 570分钟
			// 算法：先取小时数（/100）转为分钟（*60），再加上分钟数（%100）
			stime = stime / 100 * 60 + stime % 100;     // HHMM → 分钟
			etime = etime / 100 * 60 + etime % 100;     // HHMM → 分钟

			// 时间格式转换：分钟数 → HHMM
			// 例如：570分钟 → 9*100+30 = 0930
			// 算法：先取小时数（/60）转为HHMM格式（*100），再加上余数分钟（%60）
			// 注意：这个转换似乎是恒等变换（分钟→HHMM→分钟→HHMM），可能是为了规范化
			stime = stime / 60 * 100 + stime % 60;      // 分钟 → HHMM
			etime = etime / 60 * 100 + etime % 60;      // 分钟 → HHMM
			
			// 将转换后的时间区间添加到sections集合
			stateInfo->_sections.emplace_back(StateInfo::Section({ stime, etime }));
		}

		// 步骤6b：提取连续竞价时间区间（正常交易时间）
		// getTradingSections()返回连续竞价的时间段（已经过偏移处理）
		// 注意：这里面是偏移过的时间，要注意了!!!
		const auto& sections = ssInfo->getTradingSections();
		
		for (const auto& secInfo : sections)    // 遍历所有连续竞价时段
		{
			uint32_t stime = secInfo.first;     // 开始时间（偏移后的）
			uint32_t etime = secInfo.second;    // 结束时间（偏移后的）

			// 第一次转换：HHMM → 分钟数
			stime = stime / 100 * 60 + stime % 100;
			etime = etime / 100 * 60 + etime % 100;

			// 扩展时间区间（前后各扩展1分钟）
			// 目的：确保边界时间的数据也能被接收
			// 例如：9:30开盘，实际从9:29开始接收
			stime--;                            // 开始时间提前1分钟
			etime++;                            // 结束时间延后1分钟

			// 第二次转换：分钟数 → HHMM
			// 注意：这里没有考虑跨小时的情况（如59分钟+1=60分钟应该进位）
			// 实际运行中，由于只扩展1分钟，通常不会有问题
			stime = stime / 60 * 100 + stime % 60;
			etime = etime / 60 * 100 + etime % 60;
			
			// 将扩展后的时间区间添加到sections集合
			stateInfo->_sections.emplace_back(StateInfo::Section({ stime, etime }));
		}

		// 将创建好的StateInfo添加到映射表
		// key: 时段ID，value: StateInfo智能指针
		_map[stateInfo->_session] = stateInfo;

		// 步骤7：初始化交易日信息
		// 获取该时段对应的所有品种
		CodeSet* pCommSet =  _bd_mgr->getSessionComms(stateInfo->_session);
		if (pCommSet)                           // 如果品种集合存在
		{
			// 获取当前日期和时间
			uint32_t curDate = TimeUtils::getCurDate();         // 格式：YYYYMMDD
			uint32_t curMin = TimeUtils::getCurMin() / 100;     // 格式：HHMM（去掉秒数）
			
			// 计算偏移后的日期和时间
			// 例如：夜盘21:00属于下一交易日
			uint32_t offDate = ssInfo->getOffsetDate(curDate, curMin);  // 偏移日期
			uint32_t offMin = ssInfo->offsetTime(curMin, true);         // 偏移时间

			// 遍历该时段的所有品种，设置交易日
			for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
			{
				const char* pid = (*it).c_str();    // 品种ID（如"SHFE.rb"）

				// 获取该品种在指定日期和时间的交易日
				// 然后设置为当前交易日
				// 第三个参数false：不考虑节假日
				// 第四个参数false：直接设置，不检查
				 _bd_mgr->setTradingDate(pid,  _bd_mgr->getTradingDate(pid, offDate, offMin, false), false);
				
				// 计算前一日期（用于夜盘判断）
				uint32_t prevDate = TimeUtils::getNextDate(curDate, -1);
				
				// 复杂的节假日判断逻辑
				// 判断该品种今天是否应该交易
				if ((ssInfo->getOffsetMins() > 0 &&                         // 如果时间往后偏移（夜盘）
					(! _bd_mgr->isTradingDate(pid, curDate) &&              // 且当前日期不是交易日
					!(ssInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||  // 且不是夜盘后半夜
					(ssInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))  // 或者时间不偏移且偏移日期不是交易日
					)
				{
					// 该品种今天休市
					WTSLogger::info("Instrument {} is in holiday", pid);
				}
			}
		}
	}
	
	// 初始化成功
	return true;
}

/**
 * @brief 启动状态监控线程实现
 * 
 * 该方法创建并启动一个独立的监控线程，线程每秒检查一次时间和状态，
 * 并根据预定义的规则进行状态转换。这是StateMonitor的核心方法。
 * 
 * 线程执行逻辑概览：
 * 1. 外层循环：检查停止标志
 * 2. 时间等待：每秒执行一次
 * 3. 获取当前日期时间
 * 4. 遍历所有交易时段
 * 5. 根据当前状态执行对应的转换逻辑
 * 6. 处理盘后数据转储
 * 
 * 监控频率：
 * - 每秒检查一次（1000ms间隔）
 * - 使用2ms的睡眠粒度避免CPU空转
 * - 精度足够，开销很小
 * 
 * 状态转换触发：
 * - 时间到达预定时点自动转换
 * - 检测到节假日自动转换
 * - 所有品种都休市时转换为节假日
 * 
 * @note 使用智能指针管理线程，自动清理资源
 */
void StateMonitor::run()
{
	// 检查线程是否已创建（避免重复创建）
	if(_thrd == NULL)
	{
		// 创建新线程，使用lambda表达式定义线程函数
		// this捕获：lambda内部可以访问StateMonitor的成员
		_thrd.reset(new StdThread([this](){

			// 外层循环：持续运行直到收到停止信号
			while (!_stopped)
			{
				// 静态变量：记录上次执行时间，确保每秒执行一次
				static uint64_t lastTime = 0;

				// 内层循环：等待1秒间隔
				while(true)
				{
					// 获取当前时间（毫秒级时间戳）
					uint64_t now = TimeUtils::getLocalTimeNow();
					
					// 检查是否已经过了1秒（1000ms）
					if(now - lastTime >= 1000)
						break;                  // 时间到了，退出等待循环

					// 检查停止标志
					if(_stopped)
						break;                  // 收到停止信号，退出等待循环

					// 短暂睡眠，避免CPU空转
					// 2ms的粒度既不会过度占用CPU，也能保证及时响应
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
				}

				// 再次检查停止标志
				if(_stopped)
					break;                      // 退出外层循环，线程结束

				// 获取当前日期和时间（用于状态判断）
				uint32_t curDate = TimeUtils::getCurDate();         // 当前日期，格式YYYYMMDD
				uint32_t curMin = TimeUtils::getCurMin() / 100;     // 当前时间，格式HHMM（去掉秒）

				// 遍历所有交易时段，检查并更新状态
				auto it = _map.begin();
				for (; it != _map.end(); it++)
				{
					// 获取StateInfo智能指针（使用引用避免智能指针的复制开销）
					StatePtr& stateInfo = (StatePtr&)it->second;

					// 获取该时段的详细配置信息
					WTSSessionInfo* sInfo = stateInfo->_sInfo;

					// 计算偏移后的日期
					// 用于处理夜盘等跨日情况
					// 例如：21:00属于下一交易日
					uint32_t offDate = sInfo->getOffsetDate(curDate, curMin);
					
					// 计算前一日期
					// 用于判断夜盘后半夜的情况
					// 例如：凌晨1:00，虽然是新的自然日，但仍属于昨天的交易日
					uint32_t prevDate = TimeUtils::getNextDate(curDate, -1);

					// 根据当前状态执行对应的转换逻辑
					// 每种状态都有独立的处理分支
					switch(stateInfo->_state)
					{
					// ========== 状态1：SS_ORIGINAL（未初始化） ==========
					case SS_ORIGINAL:
						{
							// 获取偏移后的时间（用于夜盘判断）
							uint32_t offTime = sInfo->offsetTime(curMin, true);
							// 获取偏移后的初始化时间
							uint32_t offInitTime = sInfo->offsetTime(stateInfo->_init_time, true);
							// 获取偏移后的收盘时间
							uint32_t offCloseTime = sInfo->offsetTime(stateInfo->_close_time, false);
							// 获取集合竞价开始时间
							uint32_t aucStartTime = sInfo->getAuctionStartTime(true);

							// 检查该时段的所有品种是否都在节假日
							bool isAllHoliday = true;               // 假设全部休市
							std::stringstream ss_a, ss_b;          // 字符串流，用于日志
							
							// 获取该时段对应的所有品种
							CodeSet* pCommSet =  _bd_mgr->getSessionComms(stateInfo->_session);
							if (pCommSet)
							{
								// 遍历所有品种，检查是否交易日
								for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
								{
									const char* pid = (*it).c_str();
									
									// 复杂的节假日判断条件
									// 条件组合：
									// (条件A) 或 (条件B)
									//
									// 条件A（夜盘情况）：时间往后偏移 且 (当前日不是交易日 且 不是夜盘后半夜)
									// 条件B（正常情况）：时间不偏移 且 偏移日期不是交易日
									if ((sInfo->getOffsetMins() > 0 &&
										(! _bd_mgr->isTradingDate(pid, curDate) &&	// 当前日期不是交易日
										!(sInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||  // 且不在夜盘后半夜
										(sInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))  // 或偏移日期不是交易日
										)
									{
										// 该品种休市
										ss_a << pid << ",";
										WTSLogger::info("Instrument {} is in holiday", pid);
									}
									else
									{
										// 该品种交易
										ss_b << pid << ",";
										isAllHoliday = false;       // 有品种交易，不是全部休市
									}
								}

							}
							else
							{
								// 该时段没有对应的品种，转换为节假日状态
								WTSLogger::info("No corresponding instrument of trading session {}[{}], changed into holiday state", sInfo->name(), stateInfo->_session);
								stateInfo->_state = SS_Holiday;
							}

							// 根据品种休市情况决定状态转换
							if(isAllHoliday)                        // 如果所有品种都休市
							{
								// 转换为节假日状态
								WTSLogger::info("All instruments of trading session {}[{}] are in holiday, changed into holiday state", sInfo->name(), stateInfo->_session);
								stateInfo->_state = SS_Holiday;
							}
							else if (offTime >= offCloseTime)       // 如果已经过了收盘时间
							{
								// 直接转换为已收盘状态（跳过前面的状态）
								// 这种情况发生在系统启动时间晚于收盘时间
								stateInfo->_state = SS_CLOSED;
								WTSLogger::info("Trading session {}[{}] stopped receiving data", sInfo->name(), stateInfo->_session);
							}
							else if (aucStartTime != -1 && offTime >= aucStartTime)  // 如果已到集合竞价时间
							{
								// 检查当前是否在交易区间内
								if (stateInfo->isInSections(offTime))
								{
									// 在交易区间内，转换为接收中状态
									stateInfo->_state = SS_RECEIVING;
									WTSLogger::info("Trading session {}[{}] started receiving data", sInfo->name(), stateInfo->_session);
								}
								else
								{
									// 不在交易区间内
									// 判断是否已过收盘时间
									if(offTime < sInfo->getCloseTime(true))  // 还未到收盘
									{
										// 处于中途休盘时间
										stateInfo->_state = SS_PAUSED;
										WTSLogger::info("Trading session {}[{}] paused receiving data", sInfo->name(), stateInfo->_session);
									}
									else                                    // 已过收盘但未到数据收盘时间
									{
										// 收盘后还需要接收一段时间（等待结算价等）
										// 大于市场收盘时间，但是没有大于接收收盘时间，则还要继续接收，主要是要收结算价
										stateInfo->_state = SS_RECEIVING;
										WTSLogger::info("Trading session {}[{}] started receiving data", sInfo->name(), stateInfo->_session);
									}
									
								}
							}								
							else if (offTime >= offInitTime)        // 如果已到初始化时间但未到集合竞价
							{
								// 转换为已初始化状态
								stateInfo->_state = SS_INITIALIZED;
								WTSLogger::info("Trading session {}[{}] initialized", sInfo->name(), stateInfo->_session);
							}

							// 如果还未到初始化时间，保持SS_ORIGINAL状态
						}
						break;
						
					// ========== 状态2：SS_INITIALIZED（已初始化） ==========
					case SS_INITIALIZED:
						{
							// 获取偏移后的当前时间
							uint32_t offTime = sInfo->offsetTime(curMin, true);
							// 获取偏移后的集合竞价开始时间
							uint32_t offAucSTime = sInfo->getAuctionStartTime(true);
							
							// 检查是否到达集合竞价时间
							if (offAucSTime == -1 || offTime >= sInfo->getAuctionStartTime(true))
							{
								// 已到达集合竞价时间或没有集合竞价
								// 进一步判断是否在交易区间内
								if (!stateInfo->isInSections(offTime) && offTime < sInfo->getCloseTime(true))
								{
									// 不在交易区间内，且未到收盘时间
									// 转换为暂停状态（中途休盘）
									stateInfo->_state = SS_PAUSED;

									WTSLogger::info("Trading session {}[{}] paused receiving data", sInfo->name(), stateInfo->_session);
								}
								else
								{
									// 在交易区间内，或已过收盘时间
									// 转换为接收中状态
									stateInfo->_state = SS_RECEIVING;
									WTSLogger::info("Trading session {}[{}] started receiving data", sInfo->name(), stateInfo->_session);
								}
								
							}
							// 如果还未到集合竞价时间，保持SS_INITIALIZED状态
						}
						break;
						
					// ========== 状态3：SS_RECEIVING（接收中） ==========
					case SS_RECEIVING:
						{
							// 获取偏移后的当前时间
							uint32_t offTime = sInfo->offsetTime(curMin, true);
							// 获取偏移后的收盘时间
							uint32_t offCloseTime = sInfo->offsetTime(stateInfo->_close_time, false);
							
							// 检查是否到达收盘时间
							if (offTime >= offCloseTime)
							{
								// 已到达收盘时间，停止接收数据
								stateInfo->_state = SS_CLOSED;

								WTSLogger::info("Trading session {}[{}] stopped receiving data", sInfo->name(), stateInfo->_session);
							}
							else if (offTime >= sInfo->getAuctionStartTime(true))  // 已过集合竞价时间
							{
								// 检查是否还在正常交易时间内
								if (offTime < sInfo->getCloseTime(true))           // 未到市场收盘时间
								{
									// 检查是否在交易区间内
									if (!stateInfo->isInSections(offTime))
									{
										// 不在交易区间内，说明进入中途休盘
										// 转换为暂停状态
										stateInfo->_state = SS_PAUSED;

										WTSLogger::info("Trading session {}[{}] paused receiving data", sInfo->name(), stateInfo->_session);
									}
									// 如果在交易区间内，保持SS_RECEIVING状态
								}
								else
								{
									// 已过市场收盘时间
									// 这是下午收盘以后的时间
									// 这里不能改状态，因为要收结算价
									// 保持SS_RECEIVING状态，继续接收收盘数据
								}
							}
							// 如果在正常交易时间内，保持SS_RECEIVING状态
						}
						break;
						
					// ========== 状态4：SS_PAUSED（暂停） ==========
					case SS_PAUSED:
						{
							// 暂停状态只能转换为交易状态或节假日状态
							// 获取当前星期几（用于日志，实际未使用）
							uint32_t weekDay = TimeUtils::getWeekDay();

							// 再次检查是否所有品种都休市
							bool isAllHoliday = true;
							CodeSet* pCommSet =  _bd_mgr->getSessionComms(stateInfo->_session);
							if (pCommSet)
							{
								for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
								{
									const char* pid = (*it).c_str();
									
									// 节假日判断逻辑（与SS_ORIGINAL中相同）
									if ((sInfo->getOffsetMins() > 0 &&
										(! _bd_mgr->isTradingDate(pid, curDate) &&
										!(sInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||
										(sInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))
										)
									{
										// 该品种休市
										WTSLogger::info("Instrument {} is in holiday", pid);
									}
									else
									{
										// 该品种交易
										isAllHoliday = false;
									}
								}
							}
							
							if (!isAllHoliday)                      // 如果有品种交易
							{
								// 获取偏移后的当前时间
								uint32_t offTime = sInfo->offsetTime(curMin, true);
								
								// 检查是否在交易区间内
								if (stateInfo->isInSections(offTime))
								{
									// 在交易区间内，恢复接收
									stateInfo->_state = SS_RECEIVING;
									WTSLogger::info("Trading session {}[{}] continued to receive data", sInfo->name(), stateInfo->_session);
								}
								// 如果不在交易区间内，保持SS_PAUSED状态
							}
							else                                    // 如果所有品种都休市
							{
								// 转换为节假日状态
								WTSLogger::info("All instruments of trading session {}[{}] are in holiday, changed into holiday state", sInfo->name(), stateInfo->_session);
								stateInfo->_state = SS_Holiday;
							}
						}
						break;
						
					// ========== 状态5：SS_CLOSED（已收盘） ==========
					case SS_CLOSED:
						{
							// 获取偏移后的当前时间
							uint32_t offTime = sInfo->offsetTime(curMin, true);
							// 获取偏移后的盘后处理时间
							uint32_t offProcTime = sInfo->offsetTime(stateInfo->_proc_time, true);
							
							// 检查是否到达盘后处理时间
							if (offTime >= offProcTime)
							{
								// 检查该时段是否已经处理过
								if(!_dt_mgr->isSessionProceeded(stateInfo->_session))
								{
									// 还未处理，转换为处理中状态
									stateInfo->_state = SS_PROCING;

									WTSLogger::info("Trading session {}[{}] started processing closing task", sInfo->name(), stateInfo->_session);
									
									// 触发历史数据转储
									// DataManager会将实时数据转换为历史数据
									_dt_mgr->transHisData(stateInfo->_session);
								}
								else
								{
									// 已经处理过，直接转换为已处理状态
									stateInfo->_state = SS_PROCED;
								}
							}
							else if (offTime >= sInfo->getAuctionStartTime(true) && offTime <= sInfo->getCloseTime(true))
							{
								// 处于集合竞价到收盘之间的时间
								// 检查是否在交易区间内
								if (!stateInfo->isInSections(offTime))
								{
									// 不在交易区间内，转换为暂停状态
									// 这种情况很少见，可能是系统重启导致的状态不一致
									stateInfo->_state = SS_PAUSED;

									WTSLogger::info("Trading session {}[{}] paused receiving data", sInfo->name(), stateInfo->_session);
								}
							}
							// 其他情况保持SS_CLOSED状态
						}
						break;
						
					// ========== 状态6：SS_PROCING（处理中） ==========
					case SS_PROCING:
						// 处理中是一个短暂的过渡状态
						// 数据转储完成后立即转换为已处理状态
						stateInfo->_state = SS_PROCED;
						break;
						
					// ========== 状态7：SS_PROCED（已处理）和状态8：SS_Holiday（节假日） ==========
					case SS_PROCED:
					case SS_Holiday:
						{
							// 这两种状态的处理逻辑相同：等待下一交易日
							
							// 获取偏移后的当前时间
							uint32_t offTime = sInfo->offsetTime(curMin, true);
							// 获取偏移后的初始化时间
							uint32_t offInitTime = sInfo->offsetTime(stateInfo->_init_time, true);
							
							// 检查是否到达下一交易日的初始化时间之前
							// 即：处于0:00到初始化时间之间
							if (offTime >= 0 && offTime < offInitTime)
							{
								// 再次检查是否所有品种都休市
								bool isAllHoliday = true;
								CodeSet* pCommSet =  _bd_mgr->getSessionComms(stateInfo->_session);
								if (pCommSet)
								{
									for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
									{
										const char* pid = (*it).c_str();
										
										// 节假日判断逻辑
										if ((sInfo->getOffsetMins() > 0 &&
											(! _bd_mgr->isTradingDate(pid, curDate) &&
											!(sInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||
											(sInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))
											)
										{
											// 该品种休市（不记录日志，避免刷屏）
										}
										else
										{
											// 该品种交易
											isAllHoliday = false;
										}
									}
								}

								// 如果有品种交易，重置为原始状态，开始新的交易日循环
								if(!isAllHoliday)
								{
									stateInfo->_state = SS_ORIGINAL;
									WTSLogger::info("Trading session {}[{}] state reset", sInfo->name(), stateInfo->_session);
								}
								// 如果仍然全部休市，保持当前状态
							}
							// 如果还未到初始化时间前的时间窗口，保持当前状态
						}
						break;
					}
					// switch结束
					
				} // for循环结束：所有时段处理完毕

				// 更新lastTime，为下一次循环做准备
				lastTime = TimeUtils::getLocalTimeNow();

				// 特殊处理：检查是否所有时段都进入处理中状态
				// isAllInState(SS_PROCING)：所有时段都在处理中
				// !isAllInState(SS_Holiday)：不是所有时段都是节假日
				if (isAllInState(SS_PROCING) && !isAllInState(SS_Holiday))
				{
					// 所有时段都处理完成，触发缓存清理
					// "CMD_CLEAR_CACHE"是特殊命令，通知DataManager清理所有缓存
					_dt_mgr->transHisData("CMD_CLEAR_CACHE");
				}
			} // while (!_stopped) 结束
			
		}));  // lambda表达式和thread创建结束
	} // if(_thrd == NULL) 结束
}

/**
 * @brief 停止状态监控实现
 * 
 * 该方法设置停止标志并等待监控线程结束。
 * 
 * 停止流程：
 * 1. 设置_stopped标志为true
 * 2. 监控线程检测到标志后退出循环
 * 3. join()等待线程完全结束
 * 
 * @note 该方法会阻塞直到线程结束
 * @note 应该在程序退出或不再需要监控时调用
 */
void StateMonitor::stop()
{
	// 设置停止标志
	// 监控线程会在下次循环检查时发现并退出
	_stopped = true;

	// 等待线程结束
	// join()会阻塞当前线程，直到监控线程完全退出
	if (_thrd)
		_thrd->join();
}
