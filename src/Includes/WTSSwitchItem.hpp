/*!
 * \file WTSSwitchItem.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt主力切换规则对象定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统中主力合约切换规则的核心类，用于管理期货合约的主力切换逻辑。
 * 
 * 主要功能：
 * 1. 主力切换规则：定义期货合约从旧主力到新主力的切换规则
 * 2. 切换时间管理：记录具体的切换日期和生效时间
 * 3. 调整因子：提供价格和数量的调整系数，确保切换前后的数据连续性
 * 4. 合约信息：管理交易所、品种、新旧合约代码等关键信息
 * 
 * 设计特点：
 * - 工厂模式：使用静态工厂方法创建对象，简化对象管理
 * - 不可变设计：对象创建后主要属性不可修改，确保数据一致性
 * - 继承体系：继承自WTSObject，支持引用计数和内存管理
 * - 灵活配置：支持不同交易所和品种的切换规则配置
 * 
 * 应用场景：
 * - 期货主力合约自动切换
 * - 历史数据连续性处理
 * - 回测系统的主力切换逻辑
 * - 实时交易的主力合约识别
 */
#pragma once  // 防止头文件被重复包含
#include "WTSObject.hpp"  // 包含WonderTrader基础对象类

NS_WTP_BEGIN  // 开始WonderTrader命名空间

/**
 * 主力切换规则对象类
 * 
 * 功能概述：
 * 定义期货合约主力切换的规则和配置信息，包括切换时间、新旧合约代码、调整因子等。
 * 主要用于期货市场的主力合约自动切换，确保数据连续性和交易逻辑的正确性。
 * 
 * 主要特性：
 * - 切换规则管理：记录从旧主力到新主力的切换规则
 * - 时间控制：精确控制切换的生效时间
 * - 调整因子：提供价格和数量的调整系数
 * - 合约信息：管理交易所、品种、合约代码等关键信息
 * 
 * 设计模式：
 * - 工厂模式：使用静态create方法创建对象
 * - 不可变设计：核心属性创建后不可修改
 * - 继承设计：继承自WTSObject支持引用计数
 * 
 * 使用示例：
 * WTSSwitchItem* item = WTSSwitchItem::create("SHFE", "AU", "AU2406", "AU2409", 20240327);
 * item->set_factor(1.05);  // 设置调整因子
 */
class WTSSwitchItem : public WTSObject
{
protected:
	/**
	 * 受保护的构造函数
	 * 初始化调整因子为1.0，表示默认不进行调整
	 * 使用受保护访问级别，强制通过工厂方法创建对象
	 */
	WTSSwitchItem():_factor(1.0){}
	
	/**
	 * 虚析构函数
	 * 支持多态销毁，确保派生类对象能够正确清理资源
	 */
	virtual ~WTSSwitchItem(){}

public:
	/**
	 * 静态工厂方法
	 * 创建主力切换规则对象，设置所有必要的属性
	 * 
	 * @param exchg 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	 * @param product 品种代码，如"AU"、"IF"、"RB"等
	 * @param from 旧主力合约代码，如"AU2406"、"IF2403"等
	 * @param to 新主力合约代码，如"AU2409"、"IF2406"等
	 * @param dt 切换日期，格式：YYYYMMDD，如20240327
	 * @return 新创建的主力切换规则对象指针
	 * 
	 * 设计说明：
	 * - 使用工厂模式，集中管理对象创建逻辑
	 * - 一次性设置所有必要属性，避免对象状态不一致
	 * - 返回对象指针，调用者负责内存管理
	 * 
	 * 使用示例：
	 * WTSSwitchItem* item = WTSSwitchItem::create("SHFE", "AU", "AU2406", "AU2409", 20240327);
	 */
	static WTSSwitchItem* create(const char* exchg, const char* product, const char* from, const char* to, uint32_t dt)
	{
		WTSSwitchItem* pRet = new WTSSwitchItem();		// 创建新的切换规则对象
		pRet->_exchg = exchg;		// 设置交易所代码
		pRet->_product = product;	// 设置品种代码
		pRet->_from = from;			// 设置旧主力合约代码
		pRet->_to = to;				// 设置新主力合约代码
		pRet->_dt = dt;				// 设置切换日期
		return pRet;				// 返回创建的对象指针
	}

	/**
	 * 获取交易所代码
	 * 返回交易所的标识代码，用于区分不同的交易市场
	 * 
	 * @return 交易所代码的C风格字符串指针
	 * 
	 * 常见交易所代码：
	 * - SHFE: 上海期货交易所
	 * - DCE: 大连商品交易所
	 * - CFFEX: 中国金融期货交易所
	 * - CZCE: 郑州商品交易所
	 */
	const char*		exchg() const{return _exchg.c_str();}
	
	/**
	 * 获取品种代码
	 * 返回期货品种的标识代码，用于区分不同的交易品种
	 * 
	 * @return 品种代码的C风格字符串指针
	 * 
	 * 常见品种代码：
	 * - AU: 黄金
	 * - IF: 沪深300股指期货
	 * - RB: 螺纹钢
	 * - M: 豆粕
	 */
	const char*		product() const{return _product.c_str();}
	
	/**
	 * 获取旧主力合约代码
	 * 返回切换前的旧主力合约代码，用于识别需要切换的合约
	 * 
	 * @return 旧主力合约代码的C风格字符串指针
	 * 
	 * 合约代码格式：
	 * - 商品期货：品种代码+月份，如AU2406表示2024年6月黄金
	 * - 股指期货：品种代码+月份，如IF2403表示2024年3月沪深300
	 */
	const char*		from() const{return _from.c_str();}
	
	/**
	 * 获取新主力合约代码
	 * 返回切换后的新主力合约代码，用于识别切换目标合约
	 * 
	 * @return 新主力合约代码的C风格字符串指针
	 * 
	 * 合约代码格式：
	 * - 商品期货：品种代码+月份，如AU2409表示2024年9月黄金
	 * - 股指期货：品种代码+月份，如IF2406表示2024年6月沪深300
	 */
	const char*		to() const{return _to.c_str();}
	
	/**
	 * 获取切换日期
	 * 返回主力合约切换的具体日期，用于控制切换时机
	 * 
	 * @return 切换日期，格式：YYYYMMDD，如20240327
	 * 
	 * 日期格式说明：
	 * - YYYY: 4位年份，如2024
	 * - MM: 2位月份，如03表示3月
	 * - DD: 2位日期，如27表示27日
	 */
	uint32_t		switch_date() const{return _dt;}

	/**
	 * 设置调整因子
	 * 设置价格和数量的调整系数，用于处理切换前后的数据连续性
	 * 
	 * @param factor 调整因子，通常为1.0表示不调整，大于1.0表示放大，小于1.0表示缩小
	 * 
	 * 调整因子用途：
	 * - 价格调整：处理新旧合约的价格差异
	 * - 数量调整：处理新旧合约的手数差异
	 * - 数据连续性：确保切换前后的数据平滑过渡
	 * 
	 * 使用示例：
	 * item->set_factor(1.05);  // 设置调整因子为1.05，表示放大5%
	 */
	void	set_factor(double factor) { _factor = factor; }
	
	/**
	 * 获取调整因子
	 * 返回当前设置的调整因子值
	 * 
	 * @return 调整因子值，double类型
	 * 
	 * 调整因子说明：
	 * - 1.0: 不进行调整，保持原始值
	 * - >1.0: 放大调整，如1.05表示放大5%
	 * - <1.0: 缩小调整，如0.95表示缩小5%
	 */
	double	get_factor() const { return _factor; }

private:
	std::string		_exchg;		// 交易所代码，如"SHFE"、"DCE"、"CFFEX"等
	std::string		_product;		// 品种代码，如"AU"、"IF"、"RB"等
	std::string		_from;			// 旧主力合约代码，如"AU2406"、"IF2403"等
	std::string		_to;			// 新主力合约代码，如"AU2409"、"IF2406"等
	uint32_t		_dt;			// 切换日期，格式：YYYYMMDD，如20240327
	double			_factor;		// 调整因子，用于价格和数量的调整，默认值为1.0
};

NS_WTP_END  // 结束WonderTrader命名空间
