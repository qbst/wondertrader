/*!
 * \file IndexWorker.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 指数计算工作器实现文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件实现了IndexWorker类，提供指数的实时计算功能。这是WonderTrader框架中
 * 最复杂的计算模块之一，涉及多种权重算法、触发策略和时间管理。
 * 
 * 核心实现逻辑：
 * 
 * 1. 初始化流程（init方法）：
 *    a) 读取指数基本配置（交易所、代码、触发器等）
 *    b) 获取指数对应的合约信息
 *    c) 处理连续合约代码（如HOT、ND等）
 *    d) 解析成分合约列表（支持commodities和codes两种方式）
 *    e) 订阅所有成分合约
 *    f) 获取成分的当前行情作为初始数据
 * 
 * 2. 行情处理流程（handle_quote方法）：
 *    a) 更新成分合约的行情缓存
 *    b) 判断是否应该触发计算
 *    c) 如果是触发合约或time模式，执行触发逻辑
 *    d) 根据timeout选择即时或延时计算
 * 
 * 3. 指数计算流程（generate_tick方法）：
 *    a) 锁定数据，遍历所有成分
 *    b) 根据权重算法累加价值和权重基数
 *    c) 计算指数值
 *    d) 更新开高低收
 *    e) 生成指数Tick
 *    f) 推送到IndexFactory
 * 
 * 关键算法详解：
 * 
 * 1. 权重算法实现：
 *    
 *    固定权重（weight_alg=0）：
 *    ```
 *    total_base = 1
 *    total_value += price * weight
 *    index = total_value / total_base / total_weight * stand_scale
 *    ```
 *    
 *    动态总持（weight_alg=1）：
 *    ```
 *    total_base += open_interest
 *    total_value += open_interest * price * weight
 *    index = total_value / total_base / total_weight * stand_scale
 *    ```
 *    
 *    动态成交量（weight_alg=2）：
 *    ```
 *    total_base += total_volume
 *    total_value += total_volume * price * weight
 *    index = total_value / total_base / total_weight * stand_scale
 *    ```
 * 
 * 2. 延时触发算法：
 *    - 第一次触发：创建触发线程
 *    - 设置重算时间：当前时间 + timeout
 *    - 触发线程循环等待
 *    - 时间到达：执行计算
 *    - 后续触发：只更新重算时间，不重复计算
 * 
 * 3. 开高低收计算：
 *    - 第一次：open=high=low=price
 *    - 后续：high=max(high,price), low=min(low,price)
 *    - close始终等于最新价
 * 
 * 4. 时间处理：
 *    - 取所有成分的最大时间作为指数时间
 *    - 加上timeout作为延时补偿
 *    - 确保时间的单调递增
 * 
 * 成分配置方式：
 * 
 * 方式1：按品种配置（commodities）：
 * ```json
 * "commodities": [
 *   "SHFE.rb",
 *   {"code": "DCE.i", "weight": 1.5}
 * ]
 * ```
 * - 订阅该品种的所有合约
 * - 适用：需要全部合约的场景
 * 
 * 方式2：按合约配置（codes）：
 * ```json
 * "codes": [
 *   "SHFE.rb2105",
 *   {"code": "DCE.i2105", "weight": 1.5}
 * ]
 * ```
 * - 订阅指定的合约
 * - 适用：精确控制成分的场景
 * - 支持连续合约（HOT、ND等）
 * 
 * 性能优化：
 * - SpinMutex：轻量级锁，适合短临界区
 * - 数据预分配：避免动态内存分配
 * - 批量计算：一次计算所有成分
 * - 延时合并：多次触发合并为一次计算
 * 
 * 线程安全：
 * - _weight_scales：SpinMutex保护
 * - _cache：仅在计算线程访问
 * - _process：原子操作（bool）
 */

#include "IndexWorker.h"                        // 包含IndexWorker类定义
#include "IndexFactory.h"                       // 包含IndexFactory类定义

#include "../Includes/IBaseDataMgr.h"           // 包含基础数据管理器接口
#include "../Includes/WTSVariant.hpp"           // 包含配置参数类
#include "../Includes/WTSContractInfo.hpp"      // 包含合约信息类
#include "../Includes/WTSDataDef.hpp"           // 包含数据定义

#include "../Share/CodeHelper.hpp"              // 包含代码解析辅助工具
#include "../Share/decimal.h"                   // 包含高精度浮点数比较
#include "../Share/TimeUtils.hpp"               // 包含时间工具类

#include "../WTSTools/WTSLogger.h"              // 包含日志系统

/**
 * 权重算法名称数组（用于日志输出）
 * - 索引0："Fixed"（固定权重）
 * - 索引1："DynamicInterest"（动态总持）
 * - 索引2："DynamicVolume"（动态成交量）
 */
const char* WEIGHT_ALGS[] = 
{
	"Fixed",                            // 固定权重算法
	"DynamicInterest",                  // 动态总持算法
	"DynamicVolume"                     // 动态成交量算法
};

/**
 * @brief 初始化指数工作器实现
 * 
 * 这是IndexWorker最复杂的方法，负责解析配置、订阅成分合约、初始化权重因子。
 * 
 * 实现步骤详解：
 * 
 * 步骤1：读取基本配置
 * 步骤2：处理触发器配置（支持连续合约）
 * 步骤3：解析成分合约（commodities或codes）
 * 步骤4：订阅成分并初始化权重因子
 * 
 * @param config 指数配置参数
 * @return bool 初始化成功返回true，失败返回false
 */
bool IndexWorker::init(WTSVariant* config)
{
	// 参数验证
	if (config == NULL)
		return false;

	// ===== 步骤1：读取基本配置 =====
	
	// 读取指数所属的交易所
	_exchg = config->getCString("exchg");
	
	// 读取指数代码
	_code = config->getCString("code");

	// 获取指数对应的合约信息
	// 指数也被视为一个合约，需要在基础数据中预定义
	_cInfo = _factor->get_bd_mgr()->getContract(_code.c_str(), _exchg.c_str());

	// 读取触发器配置
	// trigger：触发指数计算的条件
	// - "time"：任意成分更新时触发
	// - "SHFE.rb2105"：指定合约更新时触发
	_trigger = config->getCString("trigger");
	
	// 读取延时配置
	// timeout：触发后延时多少毫秒再计算（0=立即计算）
	_timeout = config->getUInt32("timeout");

	// 读取标准化系数
	// stand_scale：将计算值调整到期望范围
	// 例如：计算值3000，期望值1000，系数=1000/3000=0.333
	_stand_scale = config->getDouble("stand_scale");
	if (decimal::eq(_stand_scale, 0.0))         // 如果系数为0（未配置或错误）
		_stand_scale = 1.0;                     // 默认为1.0（不缩放）

	// ===== 步骤2：处理触发器（如果是连续合约，转换为分月合约） =====
	
	// 获取主力合约管理器
	IHotMgr* hotMgr = _factor->get_hot_mgr();

	// 如果触发器不是"time"，可能是合约代码
	if (_trigger != "time")
	{
		// 解析标准代码（支持HOT、ND等连续合约）
		// 例如："SHFE.rb.HOT" → 当前主力合约"SHFE.rb2105"
		CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(_trigger.c_str(), hotMgr);
		
		// 如果包含规则标签（HOT、ND等）
		if (strlen(cInfo._ruletag) > 0)
			// 获取当前的真实合约代码
			// 因为是实时处理的，需要使用当前的分月合约，而不是连续代码
			_trigger = fmt::format("{}.{}", cInfo._exchg, hotMgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID()));
	}

	// ===== 步骤3：读取权重算法配置 =====
	
	// weight_alg：权重算法类型
	// 0=固定权重，1=动态总持，2=动态成交量
	_weight_alg = config->getUInt32("weight_alg");

	// ===== 步骤4：解析成分合约配置 =====
	
	// 获取成分配置（两种方式：commodities或codes）
	WTSVariant* cfgComms = config->get("commodities");      // 按品种配置
	WTSVariant* cfgCodes = config->get("codes");            // 按合约配置
	
	// 方式1：按品种配置（commodities）
	if (cfgComms != NULL && cfgComms->size() > 0)
	{
		IBaseDataMgr* bdMgr = _factor->get_bd_mgr();

		std::size_t cnt = cfgComms->size();                 // 品种数量
		for (std::size_t i = 0; i < cnt; i++)
		{
			WTSVariant* cfgItem = cfgComms->get(i);         // 单个品种配置
			std::string fullPid;                            // 完整品种ID
			double weight = 1.0;                            // 权重值（默认1.0）

			// 如果是对象格式，可以配置权重
			// 例如：{"code": "SHFE.rb", "weight": 1.5}
			if (cfgItem->isObject())
			{
				fullPid = cfgItem->getCString("code");
				weight = cfgItem->getDouble("weight");
				if (decimal::eq(weight, 0.0))               // 权重为0视为1.0
					weight = 1.0;
			}
			else                                            // 如果是字符串格式
			{
				// 例如："SHFE.rb"
				fullPid = cfgItem->asCString();
				// weight使用默认值1.0
			}

			// 通过品种代码找到对应的合约列表，并订阅
			WTSCommodityInfo* commInfo = bdMgr->getCommodity(fullPid.c_str());
			if (commInfo == NULL)                           // 品种不存在
				continue;                                   // 跳过

			// 获取该品种的所有合约代码
			const auto& codes = commInfo->getCodes();
			for (const auto& c : codes)                     // 遍历所有合约
			{
				// 构建完整代码
				std::string fullCode = fmt::format("{}.{}", commInfo->getExchg(), c.c_str());
				
				// 创建或获取该合约的权重因子
				WeightFactor& wFactor = _weight_scales[fullCode];
				wFactor._weight = weight;                   // 设置权重值

				// 订阅该合约的行情，并读取最后的快照作为基础数据
				WTSTickData* lastTick = _factor->sub_ticks(fullCode.c_str());
				if (lastTick)                               // 如果有当前行情
				{
					// 将Tick结构体拷贝到权重因子中
					// 作为初始数据，避免第一次计算时数据不全
					memcpy(&wFactor._tick, &lastTick->getTickStruct(), sizeof(WTSTickStruct));
					lastTick->release();                    // 释放Tick引用
				}

				// 记录订阅成功日志
				WTSLogger::info("Consist {} of block index {}.{} subscribed", fullCode, _exchg, _code);
			}
		}
	}
	// 方式2：按合约配置（codes）
	else if (cfgCodes != NULL && cfgCodes->size() > 0)
	{
		IBaseDataMgr* bdMgr = _factor->get_bd_mgr();
		IHotMgr* hotMgr = _factor->get_hot_mgr();

		std::size_t cnt = cfgCodes->size();                 // 合约数量
		for (std::size_t i = 0; i < cnt; i++)
		{
			WTSVariant* cfgItem = cfgCodes->get(i);         // 单个合约配置
			std::string fullCode;                           // 完整合约代码
			double weight = 1.0;                            // 权重值（默认1.0）

			// 解析权重配置（与commodities相同）
			if (cfgItem->isObject())
			{
				std::string fullPid = cfgItem->getCString("code");
				weight = cfgItem->getDouble("weight");
				if (decimal::eq(weight, 0.0))
					weight = 1.0;
			}
			else
			{
				fullCode = cfgItem->asCString();
			}

			// 解析合约代码格式
			auto ay = StrUtil::split(fullCode, ".");
			std::string exchg = ay[0];                      // 交易所
			std::string code;                               // 合约代码
			
			if(ay.size() == 2)                              // 两段格式
			{
				// 这是fullcode格式，即CFFEX.IF2202
				code = ay[1];
			}
			else                                            // 三段或更多
			{
				// 大于2，就是stdCode格式，主要考虑CFFEX.IF.HOT
				// 解析标准代码，处理HOT、ND等连续合约
				CodeHelper::CodeInfo cInfo = CodeHelper::extractStdCode(fullCode.c_str(), hotMgr);
				
				if (strlen(cInfo._ruletag) > 0)             // 包含规则标签（HOT、ND等）
				{
					// 获取当前的真实合约代码
					code = hotMgr->getCustomRawCode(cInfo._ruletag, cInfo.stdCommID());
					WTSLogger::info("{} contract confirmed: {} -> {}.{}", cInfo._ruletag, fullCode, cInfo._exchg, code);
				}
				else
				{
					code = cInfo._code;                     // 普通合约代码
				}
				
				// 重新构建完整代码
				fullCode = fmt::format("{}.{}", cInfo._exchg, code);
			}

			// 验证合约信息
			WTSContractInfo* cInfo = _factor->get_bd_mgr()->getContract(code.c_str(), exchg.c_str());
			if(cInfo == NULL)                               // 合约不存在
			{
				WTSLogger::error("Consist {} of block index {}.{} not exists", fullCode, _exchg, _code);
				continue;                                   // 跳过该成分
			}

			// 创建权重因子并订阅
			WeightFactor& wFactor = _weight_scales[fullCode];
			wFactor._weight = weight;

			// 订阅的时候读取最后的快照，作为基础数据
			WTSTickData* lastTick = _factor->sub_ticks(fullCode.c_str());
			if (lastTick)
			{
				memcpy(&wFactor._tick, &lastTick->getTickStruct(), sizeof(WTSTickStruct));
				lastTick->release();
			}

			WTSLogger::info("Consist {} of block index {}.{} subscribed", fullCode, _exchg, _code);
		}
	}

	// 记录初始化完成日志
	WTSLogger::info("Block index {}.{} initialized，weight algorithm: {}, trigger: {}, timeout: {}", 
		_exchg, _code, WEIGHT_ALGS[_weight_alg], _trigger, _timeout);

	return true;
}

/**
 * @brief 处理成分合约的行情数据实现
 * 
 * 该方法在成分合约有新行情时被调用，负责更新缓存并判断是否触发计算。
 * 
 * @param newTick 新的Tick数据指针
 */
void IndexWorker::handle_quote(WTSTickData* newTick)
{
	// 获取合约的完整代码
	const char* fullCode = newTick->getContractInfo()->getFullCode();

	// 更新该成分的行情缓存（需要加锁保护）
	{
		SpinLock lock(_mtx_data);                           // 自旋锁，RAII自动释放
		
		// 查找该合约的权重因子
		auto it = _weight_scales.find(fullCode);
		if (it == _weight_scales.end())                     // 如果不是该指数的成分
			return;                                         // 直接返回（可能是其他指数的成分）

		// 获取权重因子引用
		WeightFactor& wFactor = (WeightFactor&)it->second;
		
		// 更新行情数据：将新Tick的结构体拷贝到缓存
		memcpy(&wFactor._tick, &newTick->getTickStruct(), sizeof(WTSTickStruct));
	} // 锁在此处自动释放

	// ===== 判断是否触发指数计算 =====
	
	// 如果使用time触发模式，第一个成分合约的行情进来以后，会去更新指数重算时间
	// 如果触发器不是"time"，且当前合约不是触发合约
	if(_trigger != "time" && _trigger.compare(fullCode) != 0)
		return;                                             // 不触发计算，直接返回

	// 到这里说明：是触发合约或time模式，需要触发计算
	
	// ===== 根据timeout选择触发策略 =====
	
	if(_timeout == 0)                                       // 如果没有设置超时时间
	{
		// 立即生成指数（同步计算）
		generate_tick();
	}
	else                                                    // 如果设置了超时时间
	{
		// 延时触发策略：避免频繁计算
		
		// 第一次触发：创建触发线程
		if(_thrd_trigger == NULL)
		{
			// 创建触发线程，使用lambda表达式定义线程函数
			_thrd_trigger.reset(new StdThread([this]() {
				
				// 线程主循环
				while(!_stopped)
				{
					// 内层循环：等待触发信号
					while(!_process)
					{
						// 使用条件变量等待
						// _process被设为true时会被唤醒
						StdUniqueLock lck(_mtx_trigger);
						_cond_trigger.wait(_mtx_trigger);
					}

					// 收到触发信号，等待超时时间到达
					do
					{
						uint64_t now = TimeUtils::getLocalTimeNow();  // 当前时间（毫秒）
						
						// 检查是否到达重算时间
						if(now >= _recalc_time)
							break;                          // 时间到了，退出等待

						// 短暂睡眠，避免CPU空转
						// 5ms的粒度在金融场景下足够
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					} while (true);

					// 时间到达，开始生成指数
					generate_tick();
					
					// 重置处理标志，等待下次触发
					_process = false;
				}
				// 线程退出
			}));
		}

		// 后续触发：更新重算时间
		if(!_process)                                       // 如果当前没有待处理的触发
		{
			_process = true;                                // 设置处理标志
			
			// 计算重算时间：当前时间 + 超时时间
			_recalc_time = TimeUtils::getLocalTimeNow() + _timeout;
			
			// 唤醒触发线程
			// 如果线程正在等待，会被唤醒并检查时间
			// 如果线程正在睡眠，下次循环会检查新的_recalc_time
			_cond_trigger.notify_all();
		}
		// 如果_process已经是true，说明已有触发在等待
		// 不需要重复设置，触发线程会在适当时间执行
	}
}

/**
 * @brief 生成指数Tick数据实现
 * 
 * 这是指数计算的核心方法，根据所有成分的行情和权重算法计算指数值。
 * 
 * 计算流程：
 * 1. 锁定数据，遍历所有成分
 * 2. 检查数据完整性
 * 3. 根据权重算法累加
 * 4. 计算指数值
 * 5. 更新开高低收
 * 6. 生成Tick并推送
 */
void IndexWorker::generate_tick()
{
	// ===== 步骤1：初始化累加变量 =====
	
	double total_base = 0.0;	                // 权重基数（归一化因子）
	double total_value = 0.0;	                // 数值累加（价值总和）
	double total_vol = 0.0;		                // 指数总成交量（所有成分的累加）
	double total_amt = 0.0;		                // 指数总成交额（所有成分的累加）
	double total_hold = 0.0;	                // 指数总持仓（所有成分的累加）
	uint64_t maxTime = 0;		                // 最后一笔tick的时间（取所有成分的最大值）
	uint32_t tDate = 0;			                // 交易日（取所有成分的最大值）

	double total_weight = 0;                    // 总权重（所有成分权重之和）

	// ===== 步骤2：锁定数据并遍历所有成分 =====
	{
		// 先把数据锁住，避免行情更新过程中的数据不一致
		SpinLock lock(_mtx_data);
		
		// 遍历所有成分合约的权重因子
		for (const auto& v : _weight_scales)
		{
			// 获取权重因子（只读）
			const WeightFactor& wFactor = v.second;
			
			// 检查数据完整性
			// 如果数据不全，直接退出（不计算指数）
			// action_date=0表示该成分还没有收到行情
			if (wFactor._tick.action_date == 0)
				return;                                     // 数据不全，不计算

			// 计算该成分的时间戳
			// makeTime：将日期+时间转换为毫秒时间戳
			uint64_t curTime = TimeUtils::makeTime(wFactor._tick.action_date, wFactor._tick.action_time);
			
			// 取所有成分的最大时间作为指数时间
			maxTime = std::max(maxTime, curTime);
			
			// 取所有成分的最大交易日作为指数交易日
			tDate = std::max(tDate, wFactor._tick.trading_date);

			// 累加成交量、成交额、持仓量（用于指数的统计数据）
			total_vol += wFactor._tick.total_volume;        // 累加成交量
			total_amt += wFactor._tick.total_turnover;      // 累加成交额
			total_hold += wFactor._tick.open_interest;      // 累加持仓量

			// 累加总权重
			total_weight += wFactor._weight;

			// ===== 步骤3：根据权重算法计算 =====
			
			switch (_weight_alg)
			{
			case 0:    // 算法0：固定权重
				total_base = 1;	                            // 权重基数为1
				// 累加：价格 * 权重
				total_value += wFactor._tick.price * wFactor._weight;
				break;
				
			case 1:	   // 算法1：动态总持权重
				// 权重基数：累加持仓量
				total_base += wFactor._tick.open_interest;
				// 累加：价格 * 持仓量 * 权重
				total_value += wFactor._tick.open_interest * wFactor._tick.price * wFactor._weight;
				break;
				
			case 2:	   // 算法2：动态成交量权重
				// 权重基数：累加成交量
				total_base += wFactor._tick.total_volume;
				// 累加：价格 * 成交量 * 权重
				total_value += wFactor._tick.total_volume * wFactor._tick.price * wFactor._weight;
				break;
				
			default:
				break;
			}
		}
	} // 锁释放

	// ===== 步骤4：计算指数值 =====
	
	// 指数计算公式：
	// index = total_value / total_base / total_weight * stand_scale
	// 
	// 固定权重示例：
	//   total_value = p1*w1 + p2*w2 + p3*w3
	//   total_base = 1
	//   total_weight = w1 + w2 + w3
	//   index = (p1*w1 + p2*w2 + p3*w3) / 1 / (w1+w2+w3) * scale
	//         = 加权平均价 * scale
	double index = total_value / total_base / total_weight * _stand_scale;

	// ===== 步骤5：时间处理 =====
	
	// 时间做一个修正：加上超时时间
	// 原因：延时触发导致时间滞后，加上timeout补偿
	maxTime += _timeout;
	
	// 将时间戳转换为日期和时间
	TimeUtils::Time32 tm32(maxTime);

	// ===== 步骤6：更新指数Tick数据 =====
	
	if(_cache.action_time == 0)                             // 如果是第一次计算
	{
		// 初始化所有字段
		strcpy(_cache.exchg, _exchg.c_str());               // 设置交易所
		strcpy(_cache.code, _code.c_str());                 // 设置指数代码
		_cache.trading_date = tDate;                        // 设置交易日

		// 第一次：开高低收都等于当前值
		_cache.price = index;                               // 最新价
		_cache.open = index;                                // 开盘价
		_cache.high = index;                                // 最高价
		_cache.low = index;		                            // 最低价

		// 设置时间
		_cache.action_date = tm32.date();                   // 行情日期
		_cache.action_time = tm32.time_ms();                // 行情时间（毫秒）
		
		// 设置统计数据
		_cache.total_volume = total_vol;                    // 总成交量
		_cache.open_interest = total_hold;                  // 总持仓量
		_cache.total_turnover = total_amt;                  // 总成交额
	}
	else                                                    // 如果不是第一次
	{
		// 更新价格相关字段
		_cache.price = index;                               // 更新最新价
		_cache.high = std::max(_cache.high, index);         // 更新最高价
		_cache.low = std::min(_cache.low, index);           // 更新最低价
		// close自动等于最新价

		// 更新时间
		_cache.action_date = tm32.date();
		_cache.action_time = tm32.time_ms();

		// 更新统计数据
		_cache.total_volume = total_vol;
		_cache.open_interest = total_hold;
		_cache.total_turnover = total_amt;
	}

	// ===== 步骤7：创建Tick对象并推送 =====
	
	// 使用_cache创建WTSTickData对象
	WTSTickData *newTick = WTSTickData::create(_cache);
	
	// 设置合约信息（指数也被视为一个合约）
	newTick->setContractInfo(_cInfo);
	
	// 推送到IndexFactory，Factory会写入DataManager
	_factor->push_tick(newTick);
	
	// 记录调试日志
	WTSLogger::debug("{}.{} - {}.{} - {}", _cache.exchg, _cache.code, _cache.action_date, _cache.action_time, _cache.price);
	
	// 释放Tick引用
	// create会设置引用计数为1，这里释放减为0，自动删除
	newTick->release();
}
