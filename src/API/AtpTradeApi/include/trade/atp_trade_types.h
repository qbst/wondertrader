/**
 * @file atp_trade_types.h
 * @brief ATP交易系统基础数据类型定义文件
 * 
 * 本文件定义了ATP交易系统中使用的所有基础数据类型、别名和联合体。
 * 这些类型定义为整个ATP交易API提供了统一的数据类型基础，确保了
 * 数据在不同模块间传递时的类型安全性和一致性。
 * 
 * 设计逻辑：
 * - 使用typedef为标准C++类型创建具有业务含义的别名
 * - 定义固定长度的字符数组类型，确保数据结构的内存布局一致
 * - 提供精确的数值类型定义，支持金融计算的精度要求
 * - 统一不同平台下的数据类型表示，确保跨平台兼容性
 * 
 * 类型分类：
 * 1. 基础数值类型 - 整型、浮点型、布尔型等
 * 2. 字符串和字符数组类型 - 固定长度的业务字段
 * 3. 标识符类型 - 账户、订单、证券等业务标识
 * 4. 金额和数量类型 - 支持高精度的金融计算
 * 5. 时间和日期类型 - 交易时间戳和日期表示
 * 6. 枚举和标志类型 - 业务状态和选项标识
 * 7. 联合体类型 - 多类型数据的统一表示
 * 
 * 精度说明：
 * - 金额类型使用N18(4)格式，支持18位整数部分和4位小数部分
 * - 数量类型使用N15(2)格式，支持15位整数部分和2位小数部分
 * - 价格类型使用N13(4)格式，支持13位整数部分和4位小数部分
 * 
 * 编码标准：
 * - 所有字符串类型使用UTF-8编码
 * - 固定长度字符数组以null结尾
 * - 数值类型采用小端字节序（little-endian）
 * 
 * @author ATP开发团队
 * @version 1.0
 * @date 2023
 */

// 代码生成器标记，用于版本控制和自动化构建
// types:b27cbce8a0509cca6b8d355c8f46e949 template:3193722197a4f5b21a92934cfb63f986 code:15d8e52bc1a461e9b05ba8f9fed2b7a5

#ifndef ATP_TRADE_TYPES_H                   // 防止头文件重复包含的预处理保护
#define ATP_TRADE_TYPES_H                   // 定义头文件保护宏

#include <cstdint>                          // 包含标准整数类型定义
#include <set>                              // 包含STL集合容器
#include <string>                           // 包含STL字符串类
#include <cstring>                          // 包含C风格字符串处理函数



// ==================== 基础返回和版本类型 ====================
typedef int32_t ATPRetCodeType;                    ///< 函数返回结果类型，用于表示API调用的成功或失败状态
typedef uint32_t ATPVersionType;                   ///< ATP协议版本号类型，用于标识使用的协议版本

// ==================== 投票相关类型 ====================
typedef std::string ATPAbstentionVotesType;        ///< 股东大会投票中的弃权票数量，使用字符串表示支持大数值
typedef std::string ATPAffirmativeVotesType;       ///< 股东大会投票中的赞成票数量，使用字符串表示支持大数值

// ==================== 账户相关类型 ====================
typedef char ATPAccountIDType[13];                 ///< 证券账户ID，固定12字符长度加结束符，用于唯一标识证券账户
typedef int8_t ATPAccountTypeType;                  ///< 账户类型标识，区分不同类型的交易账户（如普通账户、信用账户等）
typedef char ATPAccountSubCodeType[7];              ///< 合约账户子编码，用于期权等衍生品交易的子账户标识
typedef char ATPBranchIDType[11];                   ///< 营业部ID，固定10字符长度加结束符，标识券商的营业部门

// ==================== 系统分区和索引类型 ====================
typedef uint8_t ATPAgwPartitionType;               ///< AGW（应用网关）分区号，用于系统负载均衡和数据分片
typedef uint32_t ATPAgwIndexType;                  ///< AGW回报记录索引号，用于追踪和排序回报消息

// ==================== 金额和数量类型 ====================
typedef int64_t ATPAmtType;                        ///< 金额类型，N18(4)格式，支持18位整数4位小数的高精度金额计算
typedef uint8_t ATPBatchType;                      ///< 批量委托类型，标识批量操作的处理方式（等量拆分、递减拆分等）

// ==================== 公告和业务类型 ====================
typedef std::string ATPAnnouncementNumberType;     ///< 公司公告编号，用于标识上市公司发布的各类公告
typedef uint8_t ATPBusinessTypeType;               ///< 业务类型标识，区分不同的交易业务（现货、期权、融资融券等）
typedef char ATPBusinessRejectTextType[51];        ///< 业务拒绝原因描述文本，固定50字符长度加结束符

// ==================== 基础数据类型 ====================
typedef bool ATPBoolType;                          ///< 布尔类型，用于表示是/否、真/假等二元状态
typedef char ATPCashMarginType;                    ///< 信用标识，区分普通交易和融资融券交易
typedef std::string ATPClientFeatureCodeType;    ///< 终端识别码
typedef int64_t ATPClientSeqIDType;    ///< 用户系统消息序号需要全局保持唯一，由API使用者负责维护
typedef int64_t ATPClOrdNOType;    ///< 客户订单编号 
typedef char ATPClOrdIDType[11];    ///< 申报合同号
typedef char ATPContactorType[13];    ///< 联系人
typedef char ATPContactInfoType[31];    ///< 联系人信息
typedef char ATPConfirmIDType[9];    ///< 约定号
typedef char ATPCorporateActionIDType[21];    ///< 公司行为代码
typedef uint8_t ATPCoveredOrUncoveredType;    ///< 备兑标识,衍生品交易填写
typedef char ATPCustIDType[17];    ///< 客户号ID
typedef char ATPCurrencyType[5];    ///< 货币种类（仅支持人民币）
typedef int64_t ATPDate8Type;    ///< 日期,格式(YYYYmmDD)
typedef uint8_t ATPDesignationTransTypeType;    ///< 转托管注册类型 
typedef uint8_t ATPETFTradeReportTypeType;    ///< ETF 成交回报类型
typedef char ATPEnforceFlagType;    ///< 强制风险标识
typedef char ATPEntitlementNumberType[11];    ///< 权益编号
typedef char ATPExecTypeType;    ///< 执行类型
typedef char ATPExecIDType[33];    ///< 执行编号,单个交易日不重复 
typedef char ATPFundAccountIDType[17];    ///< 资金账户ID
typedef uint64_t ATPGeneralMeetingSeqType;    ///< 股东大会编码
typedef int64_t ATPIndexType;    ///< 回报记录号
typedef uint32_t ATPLengthType;    ///< 长度,表示字节为单位的数据长度
typedef char ATPLotTypeType;    ///< 订单数量类型
typedef uint8_t ATPLoginModeType;    ///< 账户登录模式
typedef uint16_t ATPMarketIDType;    ///< 市场代码
typedef uint16_t ATPMaxPriceLevelsType;    ///< 最多成交价位数
typedef uint32_t ATPMsgTypeType;    ///< 消息类型
typedef char ATPMemberMarginAccountType[26];    ///< 公司保证金账号
typedef std::string ATPNegativeVotesType;    ///< 反对数量
typedef char ATPOrdTypeType;    ///< 订单类型
typedef int8_t ATPOrderWayType;    ///< 委托方式
typedef uint8_t ATPOrdStatusType;    ///< 订单状态
typedef char ATPOrderIDType[17];    ///< 订单编号
typedef char ATPOrdRejReasonType[6];    ///< 订单拒绝原因代码
typedef uint8_t ATPOrderQueryConditionType;    ///< 订单查询条件
typedef uint8_t ATPPartitionType;    ///< 分区号
typedef char ATPPasswordType[129];    ///< 密码
typedef char ATPPBUIDType[7];    ///< 申报交易单元
typedef uint8_t ATPPBUTypeType;    ///< 交易单元类型
typedef uint8_t ATPPermissonErrorCodeType;    ///< 密码校验服务错误码
typedef int64_t ATPPriceType;    ///< 委托价格 N13(4)
typedef uint8_t ATPPricingType;    ///< 是否协议交易定价
typedef uint8_t ATPPricePropertyType;    ///< 价格类型
typedef char ATPPositionEffectType;    ///< 开平仓标识
typedef std::string ATPProxyFormNumberType;    ///< 议案编号
typedef int64_t ATPQtyType;    ///< 数量 N15(2)
typedef uint16_t ATPQueryResultCodeType;    ///< 查询结果代码
typedef uint16_t ATPRejectReasonCodeType;    ///< 拒绝代码
typedef uint8_t ATPReturnSeqType;    ///< 返回顺序，按时间排序，排序方法
typedef int64_t ATPReportIndexType;    ///< 回报记录号，从1开始编号
typedef char ATPRemarkType[128];    ///< 资金划拨备注
typedef char ATPSecurityIDType[9];    ///< 证券代码
typedef char ATPSecuritySymbolType[41];    ///< 证券简称
typedef char ATPSideType;    ///< 买卖方向
typedef std::string ATPStringType;    ///< 字符串
typedef char ATPTendererType[7];    ///< 要约收购参与人编码
typedef int64_t ATPTotalNumType;    ///< 指定查询返回数量,填 0表示不指定
typedef uint8_t ATPTransferWayType;    ///< 划拨方式
typedef uint8_t ATPTrdSubTypeType;    ///< 是否受限股份
typedef int64_t ATPTimeStampType;    ///< 委托时间20190101121212001  (毫秒)
typedef int64_t ATPTimeType;    ///< 时间
typedef char ATPTimeInForceType;    ///< 订单有效期类型
typedef char ATPUserInfoType[17];    ///< 用户私有信息
typedef uint32_t ATPValidationCodeType;    ///< 密码激活校验号
typedef char ATPVotingProposalType[11];    ///< 投票议案编号
typedef char ATPVotingPreferenceType;    ///< 投票意向
typedef char ATPVotingSegmentType[3];    ///< 分段统计段号
typedef std::string ATPExtraData;    ///< 扩展字段类型
typedef char ATPQuoteReqIDType[11];    ///< 报价请求ID
typedef uint8_t ATPQuoteStatusType;    ///< 报价状态  
typedef char ATPSecondaryOrderIDType[17];    ///< 组合编号
typedef char ATPLegSideType;    ///< 持仓类型
typedef uint8_t ATPQuoteFlagType;    ///< 是否报价，1=报价，其他非报价
typedef uint8_t ATPPositionSideType;    ///< 持仓方向
typedef char ATPPbuIDType[7];    ///< 转出交易单元
typedef uint8_t ATPRepayWayType;    ///< 还款方式
typedef char ATPCloseOutFlagType;    ///< 了结标识
typedef char ATPCashMarginSetType[41];    ///< 信用标识集合
typedef char ATPOpeningSerialNumType[33];    ///< 开仓流水号
typedef uint8_t ATPCreditIdentificationType;    ///< 融资融券信用标识
typedef uint8_t ATPInterestRateProductType;    ///< 利率产品
typedef uint32_t ATPRollOverNumberType;    ///< 已展期次数
typedef uint64_t ATPDepositRatioType;    ///< 保证金比例
typedef char ATPQueryContentsType;    ///< 查询内容
typedef uint8_t ATPPartitionNoType;    ///< 分区号
typedef uint16_t ATPCRDUnderlyingTypeType;    ///< 融资融券标的类型
typedef int64_t ATPCRDUnderlyingRateType;    ///< 融资融券标的折算率N13(4)
typedef char ATPCRDBusinessSwitchType[33];    ///< 融资融券业务开关
typedef uint8_t ATPContractStatusType;    ///< 合同状态
typedef uint8_t ATPCreditRatingType;    ///< 信用级别
typedef uint8_t ATPCloseoutStatusType;    ///< 平仓状态
typedef int64_t ATPMtnGuaranteeRatioType;    ///< 维持担保比例N7(4)
typedef uint8_t ATPAccountRole;    ///< 账户角色
typedef uint8_t BusinessAbstractType;    ///< 抽象业务编码：1.现货业务，2.经纪期权，3.两融业务
typedef char ATPEntitlementType[3];    ///< 公司行为代码
typedef uint32_t ATPBatchNoType;    ///< 强平批次
typedef uint64_t ATPRiskRatio_def;    ///< 风控阈值比例,N18(8)
typedef uint32_t ATPListDateType;    ///< 挂牌日期
typedef uint16_t ATPSecurityTypeType;    ///< 证券类别
typedef int64_t ATPMarginParamType;    ///< 价保证金比例计算参数,N8(4)
typedef char ATPCallOrPutType;    ///< 认沽认购
typedef char UnderlyingTypeType[33];    ///< 标的类型
typedef char ATPTradeRootNumType[13];    ///< 交易节点路由参数
typedef uint8_t ATPFirstUsePrivateAmountType;    ///< 是否先用保底额度
typedef int64_t ATPSeqNumType;    ///< 外部系统流水号
typedef char ATPDesignatedTransactionSecurityIDType[9];    ///< 指定交易业务委托消息证券代码
typedef uint16_t ExternalInsErrorCodeType;    ///< 外部实时指令拒绝代码
typedef uint16_t ATPReasonCodeType;    ///< 同步客户回报结束消息结果代码
typedef uint8_t ATPIssueTypeType;    ///< 发行方式  1增发发行 2首次发行 3基金发行
typedef int64_t ATPPriceTickType;    ///< 价格档位
typedef uint64_t ATPSecurityStatusType;    ///< 证券状态
typedef uint16_t ATPErrorCodeTypeType;    ///< 错误码类型
typedef char ATPShareNegotiabilityType[3];    ///< 股份性质 00流通股
typedef uint8_t ATPRedeliverType;    ///< 直接还券类型
typedef uint8_t ATPImmediateEffectType;    ///< 实时划转标志,0:非实时划转,1:实时划转
typedef uint16_t ATPOwnerType;    ///< 所有者类型
typedef int32_t ATPAlgoDurationSecType;    ///< 算法执行长度(单位秒)
typedef int32_t ATPAlgoPartRateType;    ///< 算法参与比例,N5(4)值域[0,10000]
typedef uint64_t ATPAlgoBasketCountType;    ///< 一篮子算法母单个数
typedef uint8_t ATPAlgoStatusType;    ///< 母单状态
typedef uint64_t APTContractUnitType;    ///< 合约单位类型,N15(2)
typedef uint8_t ATPIsQueryMarginQtyType;    ///< 是否输出融券专户可用数量
typedef char ATPOrderFlagType;    ///< 订单标识
typedef uint8_t ATPTradeReportTypeType;    ///< 成交申报消息类型
typedef uint8_t ATPTransAffairTypeType;    ///< 成交申报事务类别
typedef char ATPMemberIDType[7];    ///< 交易商代码
typedef char ATPInvestorTypeType[3];    ///< 交易主体类型
typedef char ATPInvestorIDType[11];    ///< 交易主体代码
typedef char ATPInvestorNameType[121];    ///< 客户名称
typedef char ATPTraderCodeType[9];    ///< 交易员代码
typedef uint16_t ATPSettlTypeType;    ///< 结算方式
typedef uint8_t ATPSettlPeriodType;    ///< 结算周期
typedef char ATPMemoType[161];    ///< 协商成交备注
typedef char ATPTradeHandlingInstrType;    ///< 成交申报模式
typedef char ATPAlgoOrdTypeType;    ///< 算法母单订单类型
typedef uint8_t ATPAlgoOperationType;    ///< 算法操作类型
typedef uint8_t ATPTransferTypeType;    ///< 持仓划拨方式
typedef uint32_t ATPParticularFlagsType;    ///< 特殊委托标识，按位表示特殊委托，0-正常委托，第一位表示夜市委托 
typedef uint8_t ATPMultiChannelFlagType;    ///< 消息订阅标志
typedef uint8_t ATPMultiChannelResultType;    ///< 订阅结果
typedef uint8_t ATPBusinessStatusType;    ///< 业务状态
typedef int64_t ATPSerialNoType;    ///< 流水号
typedef uint8_t ATPEtfPurchaseRedemptionFlagType;    ///< 是否返回ETF实时申赎成交：0-不返回，1-返回

union ErrorCodeUnion
{
public :
    ErrorCodeUnion(int param)
    {
        error_code_t = param;
    }
    ErrorCodeUnion(const char* param)
    {
        memset(error_code_c, 0, sizeof(error_code_c));
        strncpy(error_code_c, param, sizeof(error_code_c) - 1);
    }
    uint32_t error_code_t;
    char error_code_c[10];
};

#endif