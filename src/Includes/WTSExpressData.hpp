/*!
 * \file WTSExpressData.hpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief Wt指标数据定义文件
 * 
 * 文件设计逻辑与作用总结：
 * 本文件定义了WonderTrader系统的技术指标数据结构和相关功能，用于支持各种技术分析指标的计算和展示。
 * 
 * 主要功能：
 * 1. 指标线信息类(WTSLineInfo)：定义指标线的颜色、宽度、样式等视觉属性
 * 2. 指标参数类(WTSExpressParams)：管理指标计算所需的参数配置
 * 3. 指标线类(WTSExpressLine)：继承自数值数组，增加指标特有的属性和方法
 * 4. 指标数据类(WTSExpressData)：整合多个指标线，提供完整的指标数据管理
 * 
 * 设计特点：
 * - 支持多种指标线类型（折线、柱状图、面积图等）
 * - 提供指标线交叉检测功能，便于信号生成
 * - 支持指标数据的最大值、最小值计算
 * - 可配置指标线的显示样式和精度
 * - 基于引用计数机制管理内存，提高性能
 */
#pragma once
#include <stdint.h>      // 标准整数类型定义，提供固定大小的整数类型
#include "WTSDataDef.hpp" // WonderTrader数据定义，包含数值数组等基础数据结构
#include "WTSMarcos.h"    // WonderTrader宏定义，包含系统常量和类型定义

#ifdef _MSC_VER
#include <WTypes.h>  // Windows类型定义，包含COLORREF等颜色相关类型
#else
typedef unsigned long	COLORREF;  // 颜色引用类型，用于表示RGB颜色值
typedef unsigned char	BYTE;      // 字节类型，8位无符号整数
typedef unsigned short	WORD;      // 字类型，16位无符号整数
typedef unsigned long	DWORD;     // 双字类型，32位无符号整数
#define RGB(r,g,b)	((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))  // RGB颜色宏，将红绿蓝分量组合成颜色值
#endif

NS_WTP_BEGIN  // 开始WonderTrader命名空间

//////////////////////////////////////////////////////////////////////////
/**
 * 指标线信息类
 * 
 * 功能概述：
 * 定义技术指标线的视觉显示属性，包括颜色、宽度、样式等。
 * 主要用于图表显示系统中指标线的绘制配置。
 * 
 * 主要特性：
 * - 继承自WTSObject，支持引用计数和自动内存管理
 * - 封装线条的颜色、宽度、样式等视觉属性
 * - 提供静态工厂方法创建线型对象
 * - 支持多种线条样式的配置
 * 
 * 使用场景：
 * - 技术指标的图形化显示
 * - 自定义指标线的样式配置
 * - 图表系统中的线条渲染设置
 */
class WTSLineInfo : public WTSObject
{
public:
	/**
	 * 静态工厂方法：创建指标线信息对象
	 * 
	 * @param clr 线条颜色，RGB颜色值
	 * @param width 线条宽度，默认为1像素
	 * @param style 线条样式，默认为0（实线）
	 * @return 新创建的线型信息对象指针
	 * 
	 * 使用示例：
	 * WTSLineInfo* lineInfo = WTSLineInfo::create(RGB(255,0,0), 2, 0); // 红色2像素实线
	 */
	static WTSLineInfo* create(COLORREF clr, int width = 1, int style = 0)
	{
		WTSLineInfo* pRet = new WTSLineInfo();  // 创建新实例
		pRet->_line_color = clr;    // 设置线条颜色
		pRet->_line_width = width;  // 设置线条宽度
		pRet->_line_style = style;  // 设置线条样式
		return pRet;
	}

	/**
	 * 获取线条颜色
	 * @return 线条的RGB颜色值
	 */
	COLORREF color() const{return _line_color;}
	
	/**
	 * 获取线条宽度
	 * @return 线条宽度（像素）
	 */
	int width() const{return _line_width;}
	
	/**
	 * 获取线条样式
	 * @return 线条样式代码（0=实线，1=虚线等）
	 */
	int style() const{return _line_style;}

protected:
	/**
	 * 保护构造函数
	 * 初始化默认属性：白色、1像素宽度、实线样式
	 */
	WTSLineInfo()
	:_line_color(RGB(255,255,255))  // 默认白色
	,_line_width(1)                 // 默认1像素宽度
	,_line_style(0){}               // 默认实线样式

protected:
	COLORREF	_line_color;  // 线条颜色，使用RGB格式存储
	int			_line_width;  // 线条宽度，单位为像素
	int			_line_style;  // 线条样式，0=实线，1=虚线，2=点线等
};


//////////////////////////////////////////////////////////////////////////
/**
 * 指标参数类
 * 管理技术指标计算所需的参数配置
 * 主要功能：
 * - 存储指标参数列表
 * - 支持参数的添加、设置和获取
 * - 提供参数数量查询
 * - 支持数组式访问
 */
/**
 * 指标参数类
 * 
 * 功能概述：
 * 管理技术指标计算所需的参数配置，如移动平均线的周期、布林带的标准差倍数等。
 * 提供参数的添加、修改、查询等功能，支持多参数指标的配置管理。
 * 
 * 主要特性：
 * - 继承自WTSObject，支持引用计数和自动内存管理
 * - 动态管理参数列表，支持任意数量的参数
 * - 提供数组式访问接口，使用方便
 * - 支持参数的增删改查操作
 * 
 * 使用场景：
 * - 技术指标的参数配置（如MA周期、RSI周期等）
 * - 策略参数的动态调整
 * - 指标计算引擎的参数传递
 * 
 * 使用示例：
 * WTSExpressParams* params = WTSExpressParams::create();
 * params->addParam(20);  // 添加MA周期20
 * params->addParam(2);   // 添加布林带标准差倍数2
 */
class WTSExpressParams : public WTSObject
{
public:
	/**
	 * 静态工厂方法：创建参数对象
	 * 
	 * @return 新创建的指标参数对象指针
	 */
	static	WTSExpressParams* create()
	{
		WTSExpressParams* pRet = new WTSExpressParams;  // 创建新实例
		return pRet;
	}

	/**
	 * 添加参数到参数列表末尾
	 * 
	 * @param param 要添加的参数值
	 * 
	 * 使用示例：
	 * params->addParam(14);  // 添加RSI周期14
	 */
	void	addParam(int param)
	{
		m_vecParams.emplace_back(param);  // 在向量末尾就地构造新参数
	}

	/**
	 * 设置指定位置的参数值
	 * 
	 * @param idx 参数索引位置
	 * @param param 新的参数值
	 * 
	 * 注意：如果索引超出范围，操作将被忽略
	 */
	void	setParam(uint32_t idx, int param)
	{
		if(idx >= m_vecParams.size())  // 检查索引范围
			return;

		m_vecParams[idx] = param;  // 设置指定位置的参数值
	}

	/**
	 * 获取指定位置的参数值
	 * 
	 * @param idx 参数索引位置
	 * @return 参数值，如果索引无效则返回INVALID_INT32
	 */
	int		getParam(uint32_t idx) const
	{
		if(idx >= m_vecParams.size())  // 检查索引范围
			return INVALID_INT32;

		return m_vecParams[idx];  // 返回指定位置的参数值
	}

	/**
	 * 获取参数总数
	 * 
	 * @return 当前存储的参数个数
	 */
	uint32_t	getParamCount() const{return m_vecParams.size();}

	/**
	 * 重载操作符[]，提供数组式访问
	 * 
	 * @param idx 参数索引
	 * @return 指定位置参数的引用，可用于读写
	 * 
	 * 使用示例：
	 * params[0] = 20;  // 设置第一个参数为20
	 * int period = params[0];  // 读取第一个参数
	 */
	int&	operator[](uint32_t idx){return m_vecParams[idx];}

protected:
	vector<int>		m_vecParams;  // 参数存储向量，保存所有指标参数
};

//////////////////////////////////////////////////////////////////////////
/**
 * 指标线类
 * 继承自数值数组，增加指标特有的属性和方法
 * 主要功能：
 * - 支持多种线型（折线、柱状图、面积图等）
 * - 可配置显示样式和精度
 * - 支持线型信息管理
 * - 提供格式化字符串生成
 */
/**
 * 指标线类
 * 
 * 功能概述：
 * 继承自数值数组（WTSValueArray），专门用于存储和管理技术指标的数据线。
 * 在数值数组基础上增加了线型、样式、格式化等指标特有的属性和方法。
 * 
 * 主要特性：
 * - 继承数值数组的所有功能，支持数据存储和统计计算
 * - 支持多种线型（折线、柱状图、面积图等）
 * - 可配置显示样式和数值精度
 * - 支持线型信息和标签管理
 * - 提供数值格式化功能
 * 
 * 使用场景：
 * - 技术指标的数据存储（如MA、RSI、MACD等）
 * - 图表显示中的指标线渲染
 * - 指标数据的格式化输出
 * - 多线指标的管理（如布林带的上中下轨）
 * 
 * 支持的线型：
 * - WELT_Polyline: 折线图
 * - WELT_VolStick: 成交量柱状图
 * - WELT_AStickLine: 面积图等
 */
class WTSExpressLine : public WTSValueArray
{
public:
	/**
	 * 静态工厂方法：创建指标线对象
	 * 
	 * @param size 初始数据大小
	 * @param lineType 线型类型，默认为折线图
	 * @param decimal 小数位数，默认为0
	 * @param uStyle 显示样式，默认为线条和标题都可见
	 * @return 新创建的指标线对象指针
	 * 
	 * 使用示例：
	 * WTSExpressLine* maLine = WTSExpressLine::create(100, WELT_Polyline, 2);
	 */
	static WTSExpressLine*	create(uint32_t size, WTSExpressLineType lineType = WELT_Polyline, uint32_t decimal = 0, uint32_t uStyle = ELS_LINE_VISIBLE|ELS_TITLE_VISIBLE)
	{
		WTSExpressLine* pRet = new WTSExpressLine;  // 创建新实例
		pRet->m_uDecimal = decimal;    // 设置小数位数
		pRet->m_uStyle = uStyle;       // 设置显示样式
		pRet->m_lineType = lineType;   // 设置线型类型
		pRet->resize(size);            // 调整数据大小

		return pRet;
	}

	/**
	 * 构造函数
	 * 初始化线型信息数组为空
	 */
	WTSExpressLine(): m_ayLineInfo(NULL){}

	/**
	 * 重写释放方法
	 * 在释放自身的同时，释放关联的线型信息
	 */
	virtual void release()
	{
		if(isSingleRefs() && m_ayLineInfo)  // 如果是最后一个引用且有线型信息
		{
			m_ayLineInfo->release();  // 释放线型信息数组
		}

		WTSObject::release();  // 调用基类释放方法
	}

	/**
	 * 重写保持方法
	 * 增加引用计数
	 */
	virtual void retain()
	{
		WTSObject::retain();  // 调用基类保持方法
	}

	/**
	 * 获取数值格式化字符串
	 * 根据设置的小数位数生成格式化字符串，用于数值显示
	 * 
	 * @return C风格格式化字符串（如"%.2f"表示保留2位小数）
	 */
	const char* getFormat()
	{
		if(m_strFormat.empty())  // 如果格式字符串未生成
		{
			char format[12] = {0};
			sprintf(format, "%%.%df", m_uDecimal);  // 生成格式字符串
			m_strFormat = format;
		}

		return m_strFormat.c_str();  // 返回格式字符串
	}

	/**
	 * 获取小数位数设置
	 * 
	 * @return 当前设置的小数位数
	 */
	uint32_t	getDecimal(){return m_uDecimal;}

	/**
	 * 添加线型信息
	 * 为指标线添加颜色、宽度、样式等显示属性
	 * 
	 * @param lineInfo 线型信息对象指针
	 */
	void		addLineInfo(WTSLineInfo* lineInfo)
	{
		if(m_ayLineInfo == NULL)  // 如果线型信息数组未初始化
		{
			m_ayLineInfo = WTSArray::create();  // 创建线型信息数组
		}
		m_ayLineInfo->append(lineInfo, true);  // 添加线型信息并增加引用
	}

	/**
	 * 获取指定索引的线型信息
	 * 
	 * @param idx 线型信息索引，默认为0
	 * @return 线型信息对象指针，无效时返回NULL
	 */
	WTSLineInfo*	getLineInfo(uint32_t idx = 0)
	{
		if(m_ayLineInfo == NULL || m_ayLineInfo->size()==0 || idx >= m_ayLineInfo->size())
			return NULL;  // 检查有效性

		return STATIC_CONVERT(m_ayLineInfo->at(idx), WTSLineInfo*);  // 返回线型信息
	}

	/**
	 * 清空所有线型信息
	 */
	void		clearLineInfo()
	{
		if(m_ayLineInfo)
			m_ayLineInfo->clear();  // 清空线型信息数组
	}

	/**
	 * 设置线条标签
	 * 
	 * @param tag 标签字符串
	 */
	void		setLineTag(const char* tag){m_strLineTag = tag;}
	
	/**
	 * 获取线条标签
	 * 
	 * @return 标签字符串
	 */
	const char*	getLineTag(){return m_strLineTag.c_str();}

	/**
	 * 检查是否具有指定样式
	 * 
	 * @param uStyle 要检查的样式标志
	 * @return 如果具有该样式返回true，否则返回false
	 */
	bool		isStyle(uint32_t uStyle) const{ return (m_uStyle & uStyle) == uStyle; }

	/**
	 * 设置线型类型
	 * 
	 * @param lineType 新的线型类型
	 */
	void		setLineType(WTSExpressLineType lineType){m_lineType = lineType;}
	
	/**
	 * 获取线型类型
	 * 
	 * @return 当前的线型类型
	 */
	WTSExpressLineType getLineType() const{return m_lineType;}

protected:
	WTSArray*		m_ayLineInfo;   // 线型信息数组，存储颜色、宽度等属性
	std::string		m_strLineTag;   // 线条标签，用于标识不同的指标线
	uint32_t		m_uStyle;       // 显示样式标志，控制线条和标题的可见性

	uint32_t		m_uDecimal;     // 小数位数，控制数值显示精度
	std::string		m_strFormat;    // 格式化字符串，用于数值显示
	WTSExpressLineType	m_lineType; // 线型类型，决定图表中的显示方式
};
typedef vector<WTSExpressLine*>	WTSVecExpLines;


//////////////////////////////////////////////////////////////////////////
/**
 * 技术指标数据类
 * 
 * 功能概述：
 * 这是WonderTrader系统中技术指标的核心数据管理类，用于整合和管理多个指标线，
 * 提供完整的技术指标数据存储、计算和分析功能。每个WTSExpressData对象代表
 * 一个完整的技术指标（如MACD、布林带、KDJ等）。
 * 
 * 主要特性：
 * - 继承自WTSObject，支持引用计数和自动内存管理
 * - 管理多个指标线（WTSExpressLine），支持复合指标
 * - 提供指标线之间的交叉检测功能，便于信号生成
 * - 支持指标数据的最大值、最小值统计计算
 * - 可配置基准线和显示精度，满足不同指标需求
 * - 支持多种指标类型（主图、副图、独立窗口等）
 * 
 * 核心功能：
 * 1. 指标线管理：添加、获取、删除指标线
 * 2. 交叉检测：检测指标线之间的金叉、死叉信号
 * 3. 统计计算：计算指标在指定区间的最大值、最小值
 * 4. 基准线设置：为振荡类指标设置中轴线（如RSI的50线）
 * 5. 精度控制：设置指标数值的显示精度
 * 6. 标题管理：设置指标的显示名称
 * 
 * 使用场景：
 * - 技术分析系统中的指标数据存储
 * - 量化交易策略中的信号生成
 * - 图表系统中的指标显示
 * - 指标计算引擎的数据管理
 * 
 * 使用示例：
 * ```cpp
 * // 创建MACD指标
 * WTSExpressData* macd = WTSExpressData::create("MACD", WET_SubGraph);
 * 
 * // 添加MACD线、信号线、柱状图
 * WTSExpressLine* macdLine = WTSExpressLine::create(100);
 * WTSExpressLine* signalLine = WTSExpressLine::create(100);
 * WTSExpressLine* histogram = WTSExpressLine::create(100, WELT_VolStick);
 * 
 * macd->addExpLine(macdLine);
 * macd->addExpLine(signalLine);
 * macd->addExpLine(histogram);
 * 
 * // 设置基准线（零轴）
 * macd->setBaseLine(true, 0.0);
 * 
 * // 检测金叉信号
 * bool golden_cross = macd->crossOver(0, 1); // MACD线上穿信号线
 * ```
 */
class WTSExpressData : public WTSObject
{
public:
	/**
	 * 静态工厂方法：创建技术指标数据对象
	 * 
	 * 这是创建WTSExpressData对象的标准方法，采用工厂模式确保对象的正确初始化。
	 * 创建的对象已经设置了标题和类型，可以直接使用。
	 * 
	 * @param title 指标标题，用于显示和标识，如"MACD"、"RSI"、"布林带"等
	 * @param eType 指标类型，决定指标的显示位置和方式：
	 *              - WET_Unique: 独立窗口显示（默认）
	 *              - WET_MainGraph: 主图显示（如移动平均线）
	 *              - WET_SubGraph: 副图显示（如MACD、RSI）
	 * @return 新创建的技术指标数据对象指针，调用者负责管理其生命周期
	 * 
	 * 使用示例：
	 * ```cpp
	 * WTSExpressData* rsi = WTSExpressData::create("RSI", WET_SubGraph);
	 * WTSExpressData* ma = WTSExpressData::create("MA", WET_MainGraph);
	 * ```
	 */
	static WTSExpressData*	create(const char* title, WTSExpressType eType = WET_Unique)
	{
		WTSExpressData* pRet = new WTSExpressData;  // 创建新的指标数据对象
		pRet->setTitle(title);   // 设置指标标题
		pRet->setType(eType);    // 设置指标类型
		return pRet;
	}

protected:
	/**
	 * 保护构造函数
	 * 
	 * 初始化指标数据对象的默认状态，防止外部直接实例化。
	 * 必须通过静态工厂方法create()来创建对象。
	 * 
	 * 初始化参数：
	 * - m_ayExpLines: 指标线数组初始化为NULL
	 * - m_dDevide: 除数因子初始化为1.0（无缩放）
	 * - m_dBaseLine: 基准线数值初始化为0.0
	 * - m_bBaseLine: 基准线启用状态初始化为false（禁用）
	 */
	WTSExpressData() :m_ayExpLines(NULL), m_dDevide(1.0), m_dBaseLine(0.0), m_bBaseLine(false){}

	/**
	 * 虚析构函数
	 * 
	 * 支持多态销毁，确保派生类对象能够正确清理资源。
	 * 实际的资源清理工作在release()方法中完成。
	 */
	virtual ~WTSExpressData()
	{
		// 资源清理由release()方法处理
	}

public:
	/**
	 * 重写释放方法
	 * 
	 * 当引用计数降为0时，释放对象持有的所有资源。
	 * 特别处理指标线数组的释放，确保内存不泄漏。
	 */
	virtual void release()
	{
		if (isSingleRefs() && m_ayExpLines)  // 如果是最后一个引用且有指标线数组
			m_ayExpLines->release();         // 释放指标线数组

		WTSObject::release();  // 调用基类释放方法
	}

	/**
	 * 设置指标标题
	 * 
	 * @param title 指标标题字符串，用于图表显示和指标识别
	 * 
	 * 使用示例：
	 * ```cpp
	 * indicator->setTitle("移动平均线MA(20)");
	 * ```
	 */
	void		setTitle(const char* title){m_strExpTitle = title;}
	
	/**
	 * 获取指标标题
	 * 
	 * @return 当前设置的指标标题字符串
	 */
	const char* getTitle() const{return m_strExpTitle.c_str();}

	/**
	 * 设置指标类型
	 * 
	 * 指标类型决定了指标在图表系统中的显示方式和位置。
	 * 
	 * @param eType 指标类型枚举值：
	 *              - WET_Unique: 独立窗口显示
	 *              - WET_MainGraph: 主图显示（与K线重叠）
	 *              - WET_SubGraph: 副图显示（独立子窗口）
	 */
	void		setType(WTSExpressType eType){m_expType = eType;}
	
	/**
	 * 获取指标类型
	 * 
	 * @return 当前设置的指标类型
	 */
	WTSExpressType	getType() const{return m_expType;}

	/**
	 * 设置基准线配置
	 * 
	 * 基准线是技术指标中的重要参考线，用于辅助判断指标的趋势和信号。
	 * 例如RSI的50线、MACD的零轴线、布林带的中轨线等。
	 * 
	 * @param bEnable 是否启用基准线显示，默认为true
	 * @param dBaseLine 基准线的数值位置，默认为0.0
	 * 
	 * 使用示例：
	 * ```cpp
	 * rsi->setBaseLine(true, 50.0);    // RSI指标设置50中轴线
	 * macd->setBaseLine(true, 0.0);    // MACD指标设置零轴线
	 * bb->setBaseLine(false);          // 布林带不需要基准线
	 * ```
	 */
	void		setBaseLine(bool bEnable = true, double dBaseLine = 0.0)
	{
		m_bBaseLine = bEnable;    // 设置基准线启用状态
		m_dBaseLine = dBaseLine;  // 设置基准线数值位置
	}

	/**
	 * 检查是否启用了基准线
	 * 
	 * @return 如果启用了基准线返回true，否则返回false
	 */
	bool		hasBaseLine() const{ return m_bBaseLine; }
	
	/**
	 * 获取基准线数值
	 * 
	 * @return 基准线的数值位置
	 */
	double		getBaseLine() const{ return m_dBaseLine; }

	/**
	 * 添加指标线到指标数据中
	 * 
	 * 将一个指标线对象添加到当前指标的线集合中。每个指标可以包含多条线，
	 * 例如MACD指标包含MACD线、信号线和柱状图线。
	 * 
	 * @param line 要添加的指标线对象指针，不能为NULL
	 * @return 添加成功后返回该线在数组中的索引位置，失败返回INVALID_UINT32
	 * 
	 * 使用示例：
	 * ```cpp
	 * WTSExpressData* macd = WTSExpressData::create("MACD");
	 * WTSExpressLine* macdLine = WTSExpressLine::create(100);
	 * WTSExpressLine* signalLine = WTSExpressLine::create(100);
	 * 
	 * uint32_t idx0 = macd->addExpLine(macdLine);    // 添加MACD线，返回索引0
	 * uint32_t idx1 = macd->addExpLine(signalLine);  // 添加信号线，返回索引1
	 * ```
	 */
	uint32_t	addExpLine(WTSExpressLine* line)
	{
		if(NULL == line)  // 检查参数有效性
			return INVALID_UINT32;

		if(NULL == m_ayExpLines)  // 如果指标线数组未初始化
			m_ayExpLines = WTSArray::create();  // 创建指标线数组

		m_ayExpLines->append(line, false);  // 添加指标线（不增加引用计数）

		return m_ayExpLines->size() - 1;  // 返回新添加线的索引
	}

	/**
	 * 根据索引获取指标线对象
	 * 
	 * @param idx 指标线的索引位置，从0开始
	 * @return 指定索引的指标线对象指针，无效时返回NULL
	 * 
	 * 使用示例：
	 * ```cpp
	 * WTSExpressLine* macdLine = indicator->getExpLine(0);    // 获取第一条线
	 * WTSExpressLine* signalLine = indicator->getExpLine(1);  // 获取第二条线
	 * ```
	 */
	WTSExpressLine*	getExpLine(uint32_t idx)
	{
		if(NULL == m_ayExpLines || idx >= m_ayExpLines->size())  // 检查有效性
			return NULL;

		return STATIC_CONVERT(m_ayExpLines->at(idx), WTSExpressLine*);  // 返回指标线对象
	}

	/**
	 * 获取指标线总数
	 * 
	 * @return 当前指标包含的指标线数量，如果没有则返回0
	 * 
	 * 使用示例：
	 * ```cpp
	 * uint32_t lineCount = indicator->getLineCount();
	 * for(uint32_t i = 0; i < lineCount; i++) {
	 *     WTSExpressLine* line = indicator->getExpLine(i);
	 *     // 处理每条指标线
	 * }
	 * ```
	 */
	uint32_t	getLineCount(){return (NULL == m_ayExpLines)?0:m_ayExpLines->size();}

	/**
	 * 检测指标线上穿（金叉）信号
	 * 
	 * 检测两条指标线是否发生了上穿交叉，即第一条线从下方穿越第二条线到上方。
	 * 这是技术分析中重要的买入信号，如MACD线上穿信号线、短期均线上穿长期均线等。
	 * 
	 * 判断逻辑：
	 * - 前一个时点：line0 <= line1（第一条线在第二条线下方或相等）
	 * - 当前时点：line0 > line1（第一条线在第二条线上方）
	 * 
	 * @param idx0 第一条指标线的索引（被检测的线）
	 * @param idx1 第二条指标线的索引（参考线）
	 * @return 如果发生上穿返回true，否则返回false
	 * 
	 * 使用示例：
	 * ```cpp
	 * // 检测MACD金叉信号
	 * bool goldenCross = macd->crossOver(0, 1);  // MACD线上穿信号线
	 * 
	 * // 检测均线金叉信号
	 * bool maCross = ma->crossOver(0, 1);        // 短期MA上穿长期MA
	 * 
	 * if(goldenCross) {
	 *     // 处理买入信号
	 * }
	 * ```
	 */
	bool		crossOver(uint32_t idx0, uint32_t idx1)
	{
		if(NULL == m_ayExpLines)  // 检查指标线数组是否存在
			return false;

		if(idx0 >= m_ayExpLines->size() || idx1 >= m_ayExpLines->size())  // 检查索引有效性
			return false;

		WTSExpressLine* line0 = STATIC_CONVERT(m_ayExpLines->at(idx0),WTSExpressLine*);  // 获取第一条线
		WTSExpressLine* line1 = STATIC_CONVERT(m_ayExpLines->at(idx1),WTSExpressLine*);  // 获取第二条线

		if(line0->size() < 2 || line1->size() < 2)  // 检查数据点是否足够（至少需要2个点）
			return false;

		double preValue0 = line0->at(line0->size()-2);  // 第一条线的前一个值
		double curValue0 = line0->at(line0->size()-1);  // 第一条线的当前值

		double preValue1 = line1->at(line1->size()-2);  // 第二条线的前一个值
		double curValue1 = line1->at(line1->size()-1);  // 第二条线的当前值

		if(preValue0 <= preValue1 && curValue0 > curValue1)  // 判断上穿条件
			return true;

		return false;
	}

	/**
	 * 检测指标线下穿（死叉）信号
	 * 
	 * 检测两条指标线是否发生了下穿交叉，即第一条线从上方穿越第二条线到下方。
	 * 这是技术分析中重要的卖出信号，如MACD线下穿信号线、短期均线下穿长期均线等。
	 * 
	 * 判断逻辑：
	 * - 前一个时点：line0 >= line1（第一条线在第二条线上方或相等）
	 * - 当前时点：line0 < line1（第一条线在第二条线下方）
	 * 
	 * @param idx0 第一条指标线的索引（被检测的线）
	 * @param idx1 第二条指标线的索引（参考线）
	 * @return 如果发生下穿返回true，否则返回false
	 * 
	 * 使用示例：
	 * ```cpp
	 * // 检测MACD死叉信号
	 * bool deathCross = macd->crossUnder(0, 1);  // MACD线下穿信号线
	 * 
	 * // 检测均线死叉信号
	 * bool maCross = ma->crossUnder(0, 1);       // 短期MA下穿长期MA
	 * 
	 * if(deathCross) {
	 *     // 处理卖出信号
	 * }
	 * ```
	 */
	bool		crossUnder(uint32_t idx0, uint32_t idx1)
	{
		if(NULL == m_ayExpLines)  // 检查指标线数组是否存在
			return false;

		if(idx0 >= m_ayExpLines->size() || idx1 >= m_ayExpLines->size())  // 检查索引有效性
			return false;

		WTSExpressLine* line0 = STATIC_CONVERT(m_ayExpLines->at(idx0),WTSExpressLine*);  // 获取第一条线
		WTSExpressLine* line1 = STATIC_CONVERT(m_ayExpLines->at(idx1),WTSExpressLine*);  // 获取第二条线

		if(line0->size() < 2 || line1->size() < 2)  // 检查数据点是否足够（至少需要2个点）
			return false;

		double preValue0 = line0->at(line0->size()-2);  // 第一条线的前一个值
		double curValue0 = line0->at(line0->size()-1);  // 第一条线的当前值

		double preValue1 = line1->at(line1->size()-2);  // 第二条线的前一个值
		double curValue1 = line1->at(line1->size()-1);  // 第二条线的当前值

		if(preValue0 >= preValue1 && curValue0 < curValue1)  // 判断下穿条件
			return true;

		return false;
	}

	/**
	 * 计算指标在指定区间内的最大值
	 * 
	 * 遍历指标的所有线条，计算在指定数据区间内所有线条的最大值。
	 * 这个功能用于图表系统的Y轴自动缩放，确保所有指标数据都能完整显示。
	 * 
	 * @param head 区间起始位置（负数表示从尾部开始计算的位置）
	 * @param tail 区间结束位置（负数表示从尾部开始计算的位置）
	 * @return 指定区间内所有指标线的最大值，如果无有效数据返回INVALID_DOUBLE
	 * 
	 * 特殊处理：
	 * - 对于成交量柱状图（WELT_VolStick）和面积图（WELT_AStickLine），使用绝对值计算
	 * - 这样可以正确处理负值柱状图的显示范围
	 * 
	 * 使用示例：
	 * ```cpp
	 * // 计算最近20个数据点的最大值
	 * double maxVal = indicator->maxvalue(-20, -1);
	 * 
	 * // 计算全部数据的最大值
	 * double allMax = indicator->maxvalue(0, -1);
	 * ```
	 */
	double		maxvalue(int32_t head, int32_t tail) const
	{
		double ret = INVALID_DOUBLE;  // 初始化返回值为无效值
		WTSArray::ConstIterator it = m_ayExpLines->begin();  // 获取指标线数组迭代器
		for(; it != m_ayExpLines->end(); it++)  // 遍历所有指标线
		{
			WTSExpressLine* line = STATIC_CONVERT(*it, WTSExpressLine*);  // 获取当前指标线
			bool bAbs = (line->getLineType() == WELT_VolStick || line->getLineType() == WELT_AStickLine);  // 判断是否需要绝对值处理
			double v = line->maxvalue(head, tail, bAbs);  // 计算当前线的最大值
			if(v == INVALID_DOUBLE)  // 如果当前线无有效数据
				continue;
			if(ret == INVALID_DOUBLE)  // 如果是第一个有效值
				ret = v;
			else
				ret = max(ret, v);  // 取更大的值
		}

		return ret;
	}

	/**
	 * 计算指标在指定区间内的最小值
	 * 
	 * 遍历指标的所有线条，计算在指定数据区间内所有线条的最小值。
	 * 这个功能用于图表系统的Y轴自动缩放，确保所有指标数据都能完整显示。
	 * 
	 * @param head 区间起始位置（负数表示从尾部开始计算的位置）
	 * @param tail 区间结束位置（负数表示从尾部开始计算的位置）
	 * @return 指定区间内所有指标线的最小值，如果无有效数据返回INVALID_DOUBLE
	 * 
	 * 特殊处理：
	 * - 对于成交量柱状图（WELT_VolStick）和面积图（WELT_AStickLine），直接返回0
	 * - 这是因为成交量柱状图总是从0开始绘制，最小值固定为0
	 * - 这样可以确保柱状图的基线正确显示
	 * 
	 * 使用示例：
	 * ```cpp
	 * // 计算最近20个数据点的最小值
	 * double minVal = indicator->minvalue(-20, -1);
	 * 
	 * // 计算全部数据的最小值
	 * double allMin = indicator->minvalue(0, -1);
	 * ```
	 */
	double		minvalue(int32_t head, int32_t tail) const
	{
		double ret = INVALID_DOUBLE;  // 初始化返回值为无效值
		WTSArray::ConstIterator it = m_ayExpLines->begin();  // 获取指标线数组迭代器
		for(; it != m_ayExpLines->end(); it++)  // 遍历所有指标线
		{
			WTSExpressLine* line = STATIC_CONVERT(*it, WTSExpressLine*);  // 获取当前指标线
			bool bAbs = (line->getLineType() == WELT_VolStick || line->getLineType() == WELT_AStickLine);  // 判断是否为柱状图类型
			if(bAbs)  // 如果是柱状图类型（成交量柱等）
				return 0;  // 柱状图的最小值固定为0，因为柱状图是从0开始绘制的
			double v = line->minvalue(head, tail, bAbs);  // 计算当前线的最小值
			if (v == INVALID_DOUBLE)  // 如果当前线无有效数据
				continue;
			if(ret == INVALID_DOUBLE)  // 如果是第一个有效值
				ret = v;
			else
				ret = min(ret, v);  // 取更小的值
		}

		return ret;
	}

	/**
	 * 获取指标数据的大小（数据点数量）
	 * 
	 * 返回指标中第一条线的数据点数量。通常情况下，同一个指标的所有线条
	 * 应该具有相同的数据点数量，所以使用第一条线的大小作为整个指标的大小。
	 * 
	 * @return 指标数据的点数，如果没有指标线则返回0
	 * 
	 * 使用示例：
	 * ```cpp
	 * uint32_t dataPoints = indicator->size();
	 * if(dataPoints > 0) {
	 *     // 处理指标数据
	 * }
	 * ```
	 */
	uint32_t	size() const
	{
		if(NULL == m_ayExpLines || m_ayExpLines->size()==0)  // 检查是否有指标线
			return 0;

		return STATIC_CONVERT(m_ayExpLines->at(0), WTSExpressLine*)->size();  // 返回第一条线的大小
	}

	/**
	 * 获取指标显示精度（小数位数）
	 * 
	 * @return 当前设置的显示精度
	 */
	uint32_t	getPrecision() const { return m_uPrec; }
	
	/**
	 * 设置指标显示精度（小数位数）
	 * 
	 * 设置指标数值在图表和界面中显示时保留的小数位数。
	 * 
	 * @param prec 小数位数，如2表示保留2位小数（99.12）
	 * 
	 * 使用示例：
	 * ```cpp
	 * indicator->setPrecision(4);  // 设置为4位小数精度
	 * ```
	 */
	void setPrecision(uint32_t prec){ m_uPrec = prec; }

	/**
	 * 设置除数因子
	 * 
	 * 设置用于缩放指标数值的除数因子。主要用于调整指标的显示范围，
	 * 例如将大数值缩小以便于图表显示。
	 * 
	 * @param dvd 除数因子，默认为1.0（无缩放）
	 * 
	 * 使用示例：
	 * ```cpp
	 * indicator->setDevide(100.0);  // 将指标值缩小100倍显示
	 * ```
	 */
	void		setDevide(double dvd){m_dDevide = dvd;}
	
	/**
	 * 获取除数因子
	 * 
	 * @return 当前设置的除数因子
	 */
	double		getDevide() const{return m_dDevide;}

protected:
	/**
	 * 指标线数组指针
	 * 
	 * 存储属于当前技术指标的所有指标线对象。每个技术指标可以包含多条线，
	 * 例如：MACD指标包含MACD线、信号线和柱状图线；布林带包含上轨、中轨、下轨线。
	 * 使用WTSArray智能数组管理，支持引用计数和自动内存管理。
	 */
	WTSArray*		m_ayExpLines;
	
	/**
	 * 指标标题字符串
	 * 
	 * 存储技术指标的显示名称，用于图表系统中的标识和显示。
	 * 例如："MACD(12,26,9)"、"RSI(14)"、"布林带(20,2)"等。
	 * 标题通常包含指标名称和关键参数信息。
	 */
	std::string		m_strExpTitle;
	
	/**
	 * 指标类型枚举
	 * 
	 * 定义技术指标在图表系统中的显示方式和位置：
	 * - WET_Unique: 独立窗口显示，拥有专属的显示区域
	 * - WET_MainGraph: 主图显示，与K线图重叠显示（如移动平均线）
	 * - WET_SubGraph: 副图显示，在K线图下方的独立子窗口中显示（如MACD、RSI）
	 */
	WTSExpressType	m_expType;
	
	/**
	 * 显示精度（小数位数）
	 * 
	 * 控制指标数值在界面显示时保留的小数位数。
	 * 例如：m_uPrec=2表示显示为99.12的格式，m_uPrec=4表示显示为99.1234的格式。
	 * 不同类型的指标需要不同的精度：价格类指标通常2-4位，比率类指标可能需要更高精度。
	 */
	uint32_t		m_uPrec;
	
	/**
	 * 除数因子（缩放因子）
	 * 
	 * 用于调整指标数值的显示范围，通过除法运算对指标值进行缩放。
	 * 默认值为1.0表示不进行缩放。例如：
	 * - 设置为100.0可将大数值缩小100倍便于显示
	 * - 设置为0.01可将小数值放大100倍提高可读性
	 * 主要用于优化图表显示效果。
	 */
	double			m_dDevide;

	/**
	 * 是否具有标题标志
	 * 
	 * 指示当前指标是否设置了有效的标题。
	 * true表示指标有标题且应该在界面中显示，false表示无标题或不显示标题。
	 * 用于控制图表系统中指标标题的显示逻辑。
	 */
	bool			m_bHasTitle;

	/**
	 * 基准线启用标志
	 * 
	 * 控制是否启用并显示基准线（参考线）。
	 * true表示启用基准线，false表示禁用基准线。
	 * 基准线是技术指标中的重要参考线，如RSI的50线、MACD的零轴线等。
	 */
	bool			m_bBaseLine;
	
	/**
	 * 基准线数值位置
	 * 
	 * 定义基准线在Y轴上的具体数值位置。
	 * 例如：RSI指标的基准线通常设置为50.0，MACD指标的基准线设置为0.0。
	 * 只有当m_bBaseLine为true时，此数值才会生效并在图表中显示对应的水平参考线。
	 */
	double			m_dBaseLine;
};

NS_WTP_END