
/**
 * @file XTTradeStruct.h
 * @brief XT交易系统数据结构定义文件
 * 
 * 本文件定义了XT交易系统使用的所有数据结构，包括：
 * 1. 用户认证相关结构 - 登录请求和响应数据
 * 2. 查询请求结构 - 账户、持仓、委托、成交查询参数
 * 3. 交易操作结构 - 下单、撤单请求数据
 * 4. 响应数据结构 - 各种查询和操作的响应结果
 * 5. 错误信息结构 - 统一的错误码和错误描述格式
 * 
 * 设计逻辑：
 * - 使用#pragma pack(1)确保结构体内存紧密排列，保证网络传输的一致性
 * - 统一的字段命名规范，便于理解和维护
 * - 分离请求和响应结构，明确数据流向
 * - 预留足够的字符串空间，适应不同长度的业务数据
 * - 支持多种交易类型和查询模式，满足复杂的业务需求
 */

#pragma once                                    // 防止头文件重复包含的预处理指令

#pragma pack(1)                                 // 设置结构体按1字节对齐，确保数据包的紧密性

/**
 * @struct tagXTReqUserLoginField
 * @brief 用户登录请求数据结构
 * 
 * 包含用户登录所需的完整认证信息，支持多种认证方式：
 * - 基本用户名密码认证
 * - 扩展密码认证（如资金密码）
 * - 机器信息验证（硬件指纹）
 * - 令牌认证（动态口令等）
 * - 扩展属性支持（自定义认证参数）
 */
typedef struct  
{
	int				Line;						// 线路标识符，用于指定连接的服务器线路
	char			UserType[2];				// 用户类型标识（如个人投资者、机构投资者等）
	char			UserID[32];					// 用户标识符，通常为客户号或登录名
	char			InvestorID[32];				// 投资者标识符，用于区分同一用户下的不同投资账户

	char			Password[41];				// 用户密码，最大40字符加结束符
	char			ExtraPassword[41];			// 扩展密码，如资金密码或交易密码
	char			MachineInfo[2048];			// 机器信息，包含硬件指纹、MAC地址等设备标识
	char			AuthToken[2048];			// 认证令牌，支持动态口令、数字证书等高级认证
	char			ExtendAttrs[512];			// 扩展属性，用于传递自定义的认证参数
}tagXTReqUserLoginField;

/**
 * @struct tagXTReqQryAccountField
 * @brief 查询资金账户请求数据结构
 * 
 * 用于请求查询指定用户的资金账户信息，包含基本的用户身份标识
 * 查询结果将包含可用资金、冻结资金、总资产等财务信息
 */
typedef struct  
{
	int				Line;						// 线路标识符，指定查询请求的服务器线路
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符，用于多账户用户的账户区分
}tagXTReqQryAccountField;

/**
 * @struct tagXTReqQryPositionField
 * @brief 查询持仓请求数据结构
 * 
 * 用于请求查询指定用户的持仓信息，包含基本的用户身份标识
 * 查询结果将包含各个合约的持仓数量、持仓成本、浮动盈亏等信息
 */
typedef struct  
{
	int				Line;						// 线路标识符，指定查询请求的服务器线路
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符，用于多账户用户的账户区分
}tagXTReqQryPositionField;

/**
 * @struct tagXTReqQryOrderField
 * @brief 查询委托单请求数据结构
 * 
 * 用于请求查询指定用户在特定时间范围内的委托单信息
 * 支持按时间范围过滤和按委托状态过滤，满足不同的查询需求
 */
typedef struct  
{
	int				Line;						// 线路标识符，指定查询请求的服务器线路
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符，用于多账户用户的账户区分
	int				TimeBegin;					// 查询起始时间，格式为HHMMSS（如093000表示9:30:00）
	int				TimeEnd;					// 查询截止时间，格式为HHMMSS（如150000表示15:00:00）
	int				Mode;						// 查询模式：0=查询全部报单，1=仅查询可撤销的报单
}tagXTReqQryOrderField;

/**
 * @struct tagXTReqQryTradeField
 * @brief 查询成交记录请求数据结构
 * 
 * 用于请求查询指定用户在特定时间范围内的成交记录
 * 通过时间范围过滤，可以获取指定时段的成交历史
 */
typedef struct
{
	int				Line;						// 线路标识符，指定查询请求的服务器线路
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符，用于多账户用户的账户区分
	int				TimeBegin;					// 查询起始时间，格式为HHMMSS
	int				TimeEnd;					// 查询截止时间，格式为HHMMSS
}tagXTReqQryTradeField;

/**
 * @struct tagXTReqOrderInsertField
 * @brief 报单录入（下单）请求数据结构
 * 
 * 包含下单所需的完整信息，支持多种订单类型和交易方式：
 * - 支持不同交易所的合约下单
 * - 支持多种价格类型（限价、市价等）
 * - 支持买卖方向和开平仓操作
 * - 支持套期保值标识
 * - 包含完整的订单标识和数量价格信息
 */
typedef struct  
{
	int				Line;						// 线路标识符，指定下单请求的服务器线路
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符，用于多账户用户的账户区分
	char			Exchange[16];				// 交易所标识符（如SHFE、DCE、CZCE、CFFEX等）
	char			Code[32];					// 合约代码（如rb2401、IF2312等）
	char			OrderRef[32];				// 客户端报单编号，用于客户端跟踪订单
	char			PriceType[2];				// 报单价格条件：限价单、市价单、最优价等
	char			Direction[2];				// 买卖方向：买入(B)或卖出(S)
	char			Offset[2];					// 开平标志：开仓(O)、平仓(C)、平今(T)等
	char			Hedge[2];					// 套期保值标志：投机(S)、套保(H)、套利(A)
	double			LimitPrice;					// 委托价格（限价单时有效）
	int				VolumeOrigin;				// 委托数量（手数）
}tagXTReqOrderInsertField;

/**
 * @struct tagXTReqOrderCancelField
 * @brief 报单撤销（撤单）请求数据结构
 * 
 * 用于撤销之前提交的报单，需要提供准确的订单标识信息
 * 支持通过系统订单号进行撤单操作，确保撤单的准确性
 */
typedef struct  
{
	int				Line;						// 线路标识符，指定撤单请求的服务器线路
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符，用于多账户用户的账户区分
	char			Exchange[16];				// 交易所标识符，必须与原报单一致
	char			Code[32];					// 合约代码，必须与原报单一致
	char			ActionRef[32];				// 撤单操作编号，用于客户端跟踪撤单操作
	char			OrderSysID[32];				// 系统报单编号，由交易所或柜台系统生成的唯一标识
}tagXTReqOrderCancelField;


// ================================================================================================
// 响应数据结构定义区域
// ================================================================================================

/**
 * @struct tagXTRspUserLoginField
 * @brief 用户登录响应数据结构
 * 
 * 目前为空结构体，登录成功与否主要通过错误信息字段判断
 * 预留结构用于未来可能的登录响应信息扩展
 */
typedef struct  
{
	// 当前版本暂无响应字段，登录结果通过错误信息结构体传递
}tagXTRspUserLoginField;

/**
 * @struct tagXTOrderField
 * @brief 委托单信息数据结构
 * 
 * 包含委托单的完整信息，用于：
 * 1. 委托单查询响应 - 返回历史委托单信息
 * 2. 委托单状态推送 - 实时推送委托单状态变化
 * 
 * 涵盖了委托单从提交到最终状态的全生命周期信息
 */
typedef struct
{
	int				Line;						// 线路标识符
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符
	char			Exchange[16];				// 交易所标识符
	char			Code[32];					// 合约代码
	char			OrderRef[32];				// 客户端报单编号
	char			PriceType[2];				// 报单价格条件
	char			Direction[2];				// 买卖方向
	char			Offset[2];					// 开平标志
	char			Hedge[2];					// 套期保值标志
	double			LimitPrice;					// 委托价格
	int				VolumeOrigin;				// 原始委托数量
	char			OrderSysID[32];				// 系统报单编号（交易所或柜台生成的唯一标识）
	char			OrderStatus[2];				// 报单状态：未报(0)、已报(1)、部成(2)、全成(3)、撤单(4)等
	int				VolumeTraded;				// 已成交数量
	int				VolumeRemain;				// 剩余未成交数量
	int				InsertDate;					// 报单日期，格式为YYYYMMDD
	int				InsertTime;					// 报单时间，格式为HHMMSS
	char			StatusMsg[256];				// 状态描述信息，包含详细的状态说明或错误信息
}tagXTOrderField;

/**
 * @struct tagXTTradeField
 * @brief 成交记录信息数据结构
 * 
 * 包含成交的详细信息，用于：
 * 1. 成交查询响应 - 返回历史成交记录
 * 2. 成交回报推送 - 实时推送新的成交信息
 * 
 * 记录了每笔成交的完整信息，是交易记录和盈亏计算的重要数据源
 */
typedef struct
{
	int				Line;						// 线路标识符
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符
	char			Exchange[16];				// 交易所标识符
	char			Code[32];					// 合约代码
	char			OrderRef[32];				// 对应的客户端报单编号
	char			TradeID[32];				// 成交编号（交易所生成的唯一成交标识）
	char			Direction[2];				// 买卖方向
	char			OrderSysID[32];				// 对应的系统报单编号
	char			Offset[2];					// 开平标志
	char			Hedge[2];					// 套期保值标志
	double			Price;						// 实际成交价格
	int				Volume;						// 成交数量（手数）
	int				TradeDate;					// 成交日期，格式为YYYYMMDD
	int				TradeTime;					// 成交时间，格式为HHMMSS
}tagXTTradeField;

/**
 * @struct tagXTRspAccountField
 * @brief 资金账户查询响应数据结构
 * 
 * 包含用户资金账户的核心财务信息，用于：
 * - 资金状况查询和监控
 * - 风险控制和资金管理
 * - 可用资金计算和下单风控
 */
typedef struct
{
	int				Line;						// 线路标识符
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符
	double			FrozenMargin;				// 冻结的保证金（已用于开仓但尚未成交的保证金）
	double			FrozenCash;					// 冻结的资金（委托单占用的资金）
	double			Available;					// 可用资金（可用于新开仓的资金余额）
}tagXTRspAccountField;


/**
 * @struct tagXTRspPositionField
 * @brief 持仓查询响应数据结构
 * 
 * 包含用户持仓的详细信息，用于：
 * - 持仓状况查询和监控
 * - 持仓盈亏计算
 * - 平仓操作的风险控制
 * - 保证金占用情况分析
 */
typedef struct
{
	int				Line;						// 线路标识符
	char			UserType[2];				// 用户类型标识
	char			UserID[32];					// 用户标识符
	char			InvestorID[32];				// 投资者标识符
	char			Exchange[16];				// 交易所标识符
	char			Code[32];					// 合约代码
	char			PosiDirection[2];			// 持仓方向：多头(L)表示买入持仓，空头(S)表示卖出持仓
	char			Hedge[2];					// 套期保值标志：投机(S)、套保(H)、套利(A)
	int				YdPosition;					// 昨日持仓数量（可平仓的历史持仓）
	int				Position;					// 今日总持仓数量（包括昨仓和今仓）
	double			PositionCost;				// 持仓平均成本价（用于盈亏计算）
	int				FrozenPosition;				// 冻结持仓数量（已提交平仓委托但未成交的数量）
	double			UseMargin;					// 该持仓占用的保证金金额
}tagXTRspPositionField;

/**
 * @struct tagXTRspInfoField
 * @brief 错误信息响应数据结构
 * 
 * 统一的错误信息格式，用于所有API调用的错误返回：
 * - 成功操作：ErrorID = 0，ErrorMsg为空或成功信息
 * - 失败操作：ErrorID != 0，ErrorMsg包含具体错误描述
 * 
 * 提供标准化的错误处理机制，便于客户端统一处理各种异常情况
 */
typedef struct  
{
	int				ErrorID;					// 错误代码：0表示成功，非0表示具体的错误类型
	char			ErrorMsg[256];				// 错误描述信息，提供详细的错误说明或操作结果描述
}tagXTRspInfoField;












#pragma pack()									// 恢复默认的结构体字节对齐方式

