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
 * 指标数据类
 * 整合多个指标线，提供完整的指标数据管理
 * 主要功能：
 * - 管理多个指标线
 * - 提供指标线交叉检测
 * - 支持最大值、最小值计算
 * - 可配置基准线和精度
 */
//指标类
class WTSExpressData : public WTSObject
{
public:
	static WTSExpressData*	create(const char* title, WTSExpressType eType = WET_Unique)
	{
		WTSExpressData* pRet = new WTSExpressData;
		pRet->setTitle(title);
		pRet->setType(eType);
		return pRet;
	}

protected:
	WTSExpressData() :m_ayExpLines(NULL), m_dDevide(1.0), m_dBaseLine(0.0), m_bBaseLine(false){}

	virtual ~WTSExpressData()
	{
		
	}

public:
	virtual void release()
	{
		if (isSingleRefs() && m_ayExpLines)
			m_ayExpLines->release();

		WTSObject::release();
	}

	void		setTitle(const char* title){m_strExpTitle = title;}
	const char* getTitle() const{return m_strExpTitle.c_str();}

	void		setType(WTSExpressType eType){m_expType = eType;}
	WTSExpressType	getType() const{return m_expType;}

	void		setBaseLine(bool bEnable = true, double dBaseLine = 0.0)
	{
		m_bBaseLine = bEnable;
		m_dBaseLine = dBaseLine;
	}

	bool		hasBaseLine() const{ return m_bBaseLine; }
	double		getBaseLine() const{ return m_dBaseLine; }

	uint32_t	addExpLine(WTSExpressLine* line)
	{
		if(NULL == line)
			return INVALID_UINT32;

		if(NULL == m_ayExpLines)
			m_ayExpLines = WTSArray::create();

		m_ayExpLines->append(line, false);

		return m_ayExpLines->size() - 1;
	}


	WTSExpressLine*	getExpLine(uint32_t idx)
	{
		if(NULL == m_ayExpLines || idx >= m_ayExpLines->size())
			return NULL;

		return STATIC_CONVERT(m_ayExpLines->at(idx), WTSExpressLine*);
	}

	uint32_t	getLineCount(){return (NULL == m_ayExpLines)?0:m_ayExpLines->size();}

	bool		crossOver(uint32_t idx0, uint32_t idx1)
	{
		if(NULL == m_ayExpLines)
			return false;

		if(idx0 >= m_ayExpLines->size() || idx1 >= m_ayExpLines->size())
			return false;

		WTSExpressLine* line0 = STATIC_CONVERT(m_ayExpLines->at(idx0),WTSExpressLine*);
		WTSExpressLine* line1 = STATIC_CONVERT(m_ayExpLines->at(idx1),WTSExpressLine*);

		if(line0->size() < 2 || line1->size() < 2)
			return false;

		double preValue0 = line0->at(line0->size()-2);
		double curValue0 = line0->at(line0->size()-1);

		double preValue1 = line1->at(line1->size()-2);
		double curValue1 = line1->at(line1->size()-1);

		if(preValue0 <= preValue1 && curValue0 > curValue1)
			return true;

		return false;
	}

	bool		crossUnder(uint32_t idx0, uint32_t idx1)
	{
		if(NULL == m_ayExpLines)
			return false;

		if(idx0 >= m_ayExpLines->size() || idx1 >= m_ayExpLines->size())
			return false;

		WTSExpressLine* line0 = STATIC_CONVERT(m_ayExpLines->at(idx0),WTSExpressLine*);
		WTSExpressLine* line1 = STATIC_CONVERT(m_ayExpLines->at(idx1),WTSExpressLine*);

		if(line0->size() < 2 || line1->size() < 2)
			return false;

		double preValue0 = line0->at(line0->size()-2);
		double curValue0 = line0->at(line0->size()-1);

		double preValue1 = line1->at(line1->size()-2);
		double curValue1 = line1->at(line1->size()-1);

		if(preValue0 >= preValue1 && curValue0 < curValue1)
			return true;

		return false;
	}

	double		maxvalue(int32_t head, int32_t tail) const
	{
		double ret = INVALID_DOUBLE;
		WTSArray::ConstIterator it = m_ayExpLines->begin();
		for(; it != m_ayExpLines->end(); it++)
		{
			WTSExpressLine* line = STATIC_CONVERT(*it, WTSExpressLine*);
			bool bAbs = (line->getLineType() == WELT_VolStick || line->getLineType() == WELT_AStickLine);
			double v = line->maxvalue(head, tail, bAbs);
			if(v == INVALID_DOUBLE)
				continue;
			if(ret == INVALID_DOUBLE)
				ret = v;
			else
				ret = max(ret, v);
		}

		return ret;
	}

	double		minvalue(int32_t head, int32_t tail) const
	{
		double ret = INVALID_DOUBLE;
		WTSArray::ConstIterator it = m_ayExpLines->begin();
		for(; it != m_ayExpLines->end(); it++)
		{
			WTSExpressLine* line = STATIC_CONVERT(*it, WTSExpressLine*);
			bool bAbs = (line->getLineType() == WELT_VolStick || line->getLineType() == WELT_AStickLine);
			if(bAbs)//原因是成交量柱,是以0开始绘制的
				return 0;
			double v = line->minvalue(head, tail, bAbs);
			if (v == INVALID_DOUBLE)
				continue;
			if(ret == INVALID_DOUBLE)
				ret = v;
			else
				ret = min(ret, v);
		}

		return ret;
	}

	uint32_t	size() const
	{
		if(NULL == m_ayExpLines || m_ayExpLines->size()==0)
			return 0;

		return STATIC_CONVERT(m_ayExpLines->at(0), WTSExpressLine*)->size();
	}

	uint32_t	getPrecision() const { return m_uPrec; }
	void setPrecision(uint32_t prec){ m_uPrec = prec; }

	void		setDevide(double dvd){m_dDevide = dvd;}
	double		getDevide() const{return m_dDevide;}

protected:
	WTSArray*		m_ayExpLines;
	std::string		m_strExpTitle;
	WTSExpressType	m_expType;
	uint32_t		m_uPrec;
	double			m_dDevide;

	bool			m_bHasTitle;

	bool			m_bBaseLine;
	double			m_dBaseLine;
};

NS_WTP_END