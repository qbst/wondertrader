/**
 * @file atp_quant_api.h
 * @brief ATP量化交易API核心接口定义文件
 * 
 * 本文件定义了ATP量化交易系统的核心API接口，包括：
 * 1. ATPQuantHandler - 事件回调处理接口，用于接收交易响应和推送消息
 * 2. ATPQuantAPI - 主要的交易API接口，提供登录、下单、查询等核心功能
 * 
 * 设计逻辑：
 * - 采用异步回调模式，所有交易操作都通过回调函数返回结果
 * - 支持现货交易的完整生命周期：登录→下单→撤单→查询→登出
 * - 提供丰富的查询功能：订单查询、成交查询、资金查询、持仓查询等
 * - 支持ETF申购赎回、资金划拨等扩展业务功能
 * - 使用RAII设计模式管理资源生命周期
 * - 通过配置参数支持灵活的连接和性能优化设置
 * 
 * 版权信息：
 * Copyright (c) 2018 Archforce Financial Technology. All rights reserved.
 * 未经许可，不得以任何形式复制、修改或分发本软件。
 * 更多信息请访问：archforce.cn
 */

// 代码生成器标记，用于版本控制和代码同步
// requests:5f024b1066515f963731eec331bd9d61 responses:b0e958b18fde3671566c702f3c2a40de template:3a7a64e1ed9f5358ed8966384ffd67b9 code:174b44dcb1ce47cf1820a42d41b40a5d

#ifndef ATP_QUANT_API_H_                  // 防止头文件重复包含的预处理保护
#define ATP_QUANT_API_H_                  // 定义头文件保护宏
#include "atp_quant_msg.h"                // 包含ATP量化交易消息定义

namespace atp                             // ATP命名空间，避免符号冲突
{
    namespace quant_api                   // 量化API子命名空间
    {
        class Context;                    // 前置声明：上下文类，用于内部实现细节的封装
        /**
         * @class ATPQuantHandler
         * @brief ATP量化交易事件回调处理接口类
         * 
         * 该接口定义了ATP量化交易系统中所有异步事件的回调方法。
         * 客户端需要继承此接口并实现相关回调方法来处理：
         * 1. 连接状态变化事件（登录、登出、重连）
         * 2. 交易操作响应事件（下单、撤单响应）
         * 3. 实时推送事件（订单状态变化、成交回报）
         * 4. 查询结果事件（资金、持仓、订单、成交查询结果）
         * 
         * 所有回调方法都有默认空实现，客户端只需重写感兴趣的方法即可。
         */
        class QUANT_API ATPQuantHandler
        {
        public:
            /**
             * @brief 虚析构函数
             * 
             * 确保派生类对象能够正确析构，释放相关资源
             */
            virtual ~ATPQuantHandler() = default;
        public:
            /**
             * @brief 登录成功回调函数
             * @param msg 客户信息，包含登录成功后的账户详细信息
             * 
             * 当客户端成功登录ATP交易系统后触发此回调。
             * msg参数包含客户号、资金账户、证券账户等重要信息。
             */
            virtual void OnLogin(const ATPCustomerInfo& msg) {}

            /**
             * @brief 登出成功回调函数
             * @param desc 登出描述信息，说明登出的原因或状态
             * 
             * 当客户端主动登出或被系统强制登出时触发此回调。
             * 可用于清理资源或记录登出日志。
             */
            virtual void OnLogout(const char* desc) {}

            /**
             * @brief 自动重连回调函数
             * @param desc 重连描述信息，说明重连的状态或进度
             * 
             * 当网络连接中断后，系统自动尝试重新连接时触发此回调。
             * 可用于通知用户系统正在恢复连接。
             */
            virtual void OnRecovering(const char* desc) {}

        public:
            /**
             * @brief 现货集中竞价委托响应回调函数
             * @param msg 委托响应消息，包含委托的基本信息和处理结果
             * @param error_info 错误信息，如果委托失败则包含具体错误描述
             * @param request_id 请求标识符，用于匹配请求和响应
             * 
             * 当客户端发送现货买卖委托后，交易系统返回的处理结果通过此回调通知。
             * 成功时error_info.error_id为0，失败时包含具体错误码和错误描述。
             */
            virtual void OnRspCashAuctionOrder(const ATPRspCashAuctionOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 现货委托状态变化推送回调函数
             * @param msg 委托状态推送消息，包含订单的最新状态信息
             * 
             * 当委托订单状态发生变化时（如已报、部分成交、全部成交、已撤销等），
             * 系统会主动推送订单状态更新，包含成交价格、成交数量等详细信息。
             * 这是获取订单实时状态的主要途径。
             */
            virtual void OnRtnCashAuctionOrder(const ATPRtnCashAuctionOrderMsg& msg) {}
            
            /**
             * @brief 现货撤单操作响应回调函数
             * @param msg 撤单响应消息，包含撤单操作的处理结果
             * @param error_info 错误信息，如果撤单失败则包含具体错误描述
             * @param request_id 请求标识符，用于匹配撤单请求和响应
             * 
             * 当客户端发送撤单请求后，交易系统返回的处理结果通过此回调通知。
             * 撤单成功与否可通过error_info判断。
             */
            virtual void OnRspCashCancelOrder(const ATPRspCashCancelOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 现货订单查询结果回调函数
             * @param msg 订单查询结果消息，包含订单的详细信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回客户端查询的订单信息，可能包含多条记录。
             * 通过isLast参数判断是否还有更多数据。
             */
            virtual void OnRspCashOrderQueryResult(const ATPRspCashOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief 现货股份持仓查询结果回调函数
             * @param msg 股份查询结果消息，包含持仓的详细信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回客户端持有的股份信息，包括持仓数量、可用数量、成本价、
             * 市值、盈亏等重要信息。可能包含多只股票的持仓记录。
             */
            virtual void OnRspCashShareQueryResult(const ATPRspCashShareQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货按证券账户划转资金响应回调函数
             * @param msg 资金划转响应消息，包含划转操作的处理结果
             * @param request_id 请求标识符，用于匹配划转请求和响应
             * @param error_info 错误信息，如果划转失败则包含具体错误描述
             * 
             * 当客户端发送按证券账户划转资金的请求后，系统返回的处理结果。
             * 用于不同证券账户之间的资金调拨操作。
             */
            virtual void OnRspCashExternalInsTETransFundResp(const ATPRspCashExternalInsTETransFundRespMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            
            /**
             * @brief 现货资金划拨（转入、转出）响应回调函数
             * @param msg 资金划拨结果消息，包含划拨后的资金状态信息
             * @param request_id 请求标识符，用于匹配划拨请求和响应
             * @param error_info 错误信息，如果划拨失败则包含具体错误描述
             * 
             * 返回资金转入或转出操作的详细结果，包含资金余额变化、
             * 冻结金额、可用金额等重要的资金状态信息。
             */
            virtual void OnRspCashExtFundTransferResult(const ATPRspCashExtFundTransferResultOtherMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            
            /**
             * @brief 现货成交记录查询结果回调函数
             * @param msg 成交查询结果消息，包含成交记录的详细信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回客户端的成交记录，包含成交价格、成交数量、成交时间、
             * 成交费用等详细信息。可能包含多条成交记录。
             */
            virtual void OnRspCashTradeOrderQueryResult(const ATPRspCashTradeOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief 现货证券信息查询结果回调函数
             * @param msg 证券信息查询结果消息，包含证券的基本信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回证券的基本信息，如证券代码、名称、涨跌停价、最小变动价位、
             * 交易规则等重要的证券属性信息。
             */
            virtual void OnRspCashExtQueryResultSecurityInfo(const ATPRspCashExtQueryResultSecurityInfoMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief 现货资金查询结果回调函数
             * @param msg 资金查询结果消息，包含资金账户的详细信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回客户端的资金状况，包含资金余额、可用资金、冻结资金等
             * 重要的资金信息，用于资金管理和风险控制。
             */
            virtual void OnRspCashFundQueryResult(const ATPRspCashFundQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief 现货资产查询结果回调函数
             * @param msg 资产查询结果消息，包含客户总资产的详细信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回客户端的总资产状况，包含资金余额、股票市值、总资产等
             * 综合性的资产信息，用于资产配置和投资决策。
             */
            virtual void OnRspCashAssetQueryResult(const ATPRspCashAssetQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief 现货成交汇总查询结果回调函数
             * @param msg 成交汇总查询结果消息，包含按证券汇总的成交信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回按证券代码汇总的成交信息，包含买入均价、卖出均价、
             * 总成交量、总成交额等统计信息。
             */
            virtual void OnRspCashTradeCollectQueryResult(const ATPRspCashCollectQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief 现货最大可委托数量查询结果回调函数
             * @param msg 最大可委托数查询结果消息，包含可委托的最大数量
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回指定证券在当前资金和持仓条件下可委托的最大数量，
             * 用于下单前的风险控制和资金规划。
             */
            virtual void OnRspCashMaxOrderQueryResult(const ATPRspCashMaxOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief ETF基金信息查询结果回调函数
             * @param msg ETF信息查询结果消息，包含ETF的基本信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回ETF基金的基本信息，包含申购赎回单位、现金替代比例、
             * 净值等ETF特有的重要参数信息。
             */
            virtual void OnRspExtQueryResultETFInfo(const ATPRspExtQueryResultETFInfoMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            
            /**
             * @brief ETF成分股信息查询结果回调函数
             * @param msg ETF成分股查询结果消息，包含成分股的详细信息
             * @param request_id 请求标识符，用于匹配查询请求和响应
             * @param error_info 错误信息，如果查询失败则包含具体错误描述
             * @param isLast 是否为最后一条查询结果，用于批量查询的分页处理
             * 
             * 返回ETF基金的成分股信息，包含成分股代码、权重、现金替代标志、
             * 溢价比例等用于ETF申购赎回的关键信息。
             */
            virtual void OnRspExtQueryResultETFComponentInfo(const ATPRspExtQueryResultETFComponentInfoMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
        };

        /**
         * @class ATPQuantAPI
         * @brief ATP量化交易主API接口类
         * 
         * 该类是ATP量化交易系统的核心接口，提供完整的交易功能：
         * 1. 连接管理 - 初始化、登录、登出、资源管理
         * 2. 交易操作 - 现货买卖、撤单操作
         * 3. 查询功能 - 订单、成交、资金、持仓、证券信息查询
         * 4. 扩展功能 - ETF申购赎回、资金划拨等
         * 
         * 使用流程：
         * 1. 调用Init()进行全局初始化
         * 2. 创建ATPQuantAPI实例并传入回调处理器和配置参数
         * 3. 调用Login()登录交易系统
         * 4. 进行各种交易和查询操作
         * 5. 调用Logout()登出系统
         * 6. 调用Release()释放实例资源
         * 7. 调用Stop()进行全局清理
         */
        class QUANT_API ATPQuantAPI
        {
        public:
            /**
             * @brief 构造函数
             * @param[in] handler 事件回调处理器指针，用于接收所有异步事件通知
             * @param[in] properties 配置参数对象，包含连接和性能相关的各种配置
             *
             * 主要配置参数说明：
             * - "locations": 网关服务器地址，格式为"ip:port;ip:port"，必填参数
             * - "order_way": 委托方式标识，char类型，必填参数
             * - "client_feature_code": 终端识别码，用于客户端身份标识，必填参数
             * - "callback_resource_mode": 回调线程资源配置模式，默认为低时延模式
             * - "callback_thread_mode": 回调线程模型，默认为共享模式
             * - "group_id": 回调线程组标识，默认为0
             * - "min_resident_micro": 回调线程休眠时间（微秒），默认为1μs
             * - "recevie_thread_cpu": 回调线程绑定的CPU核心，0xFF表示不绑定
             * - "is_tcp_direct": 是否启用TCP Direct优化，默认为false
             * - "bind_ip_address": 绑定的本地网卡地址，启用TcpDirect或RUDP时必填
             * - "connection_protocol": 连接协议类型，默认为TCP协议
             * - "agw_user": 网关用户名，空值表示匿名登录模式，非空表示网关用户模式
             * - "agw_password": 网关用户密码
             * - "retransmit_mode": 重传模式，Quick模式不重传历史消息，Restart模式重传所有历史消息
             * - "multi_channel_flag": 多通道自主订阅标志，默认以后台开关为准
             */
            ATPQuantAPI(const ATPQuantHandler* handler, const ATPProperties* properties);

            /**
             * @brief 析构函数
             * 
             * 自动清理API实例相关的资源，包括网络连接、线程等。
             * 建议在析构前先调用Release()方法进行显式资源清理。
             */
            ~ATPQuantAPI();

            /**
             * @brief 禁用拷贝构造函数
             * 
             * ATPQuantAPI对象包含网络连接和线程资源，不允许拷贝构造，
             * 避免资源管理混乱和重复释放问题。
             */
            ATPQuantAPI(const ATPQuantAPI&) = delete;
            
            /**
             * @brief 禁用赋值操作符
             * 
             * ATPQuantAPI对象包含网络连接和线程资源，不允许赋值操作，
             * 确保每个实例的资源独立性和生命周期管理的正确性。
             */
            ATPQuantAPI& operator=(const ATPQuantAPI&) = delete;

            /**
             * @brief 全局初始化函数（静态方法）
             * @param[in] properties 全局配置参数对象
             * @return ATPErrorCodeType 错误码，0表示成功，非0表示失败
             * 
             * @note 此方法只需要调用一次，非线程安全，通常在程序启动时调用
             * 
             * 全局配置参数说明：
             * - "log_level": 业务日志级别，默认为Info级别
             * - "common_log_path": 业务日志文件路径，默认为"./log/quant_api_common_log_yyyymmdd.log"
             * - "indicator_log_path": 指标日志文件路径，默认为"./log/quant_api_indicator_log_yyyymmdd.log"
             * - "io_log_path": IO操作日志文件路径，默认为"./log/quant_api_io_log_yyyymmdd.log"
             * - "is_enable_latency": 是否启用时延统计功能，默认为false
             * 
             * 该方法负责初始化全局资源，如日志系统、线程池等，
             * 必须在创建任何ATPQuantAPI实例之前调用。
             */
            static ATPErrorCodeType  Init(const ATPProperties* properties);

            /**
             * @brief 全局停止函数（静态方法）
             * @return ATPErrorCodeType 错误码，0表示成功，非0表示失败
             * 
             * @note 只能在Init()成功后调用，且只能调用一次
             * 
             * 停止所有全局资源和服务，包括：
             * - 关闭日志系统
             * - 停止后台线程
             * - 清理全局缓存
             * 
             * 调用后，所有ATPQuantAPI对象都将无法发送消息，
             * 通常在程序退出前调用进行全局清理。
             */
            static ATPErrorCodeType Stop();

            /**
             * @brief 用户登录接口
             * @param[in] property 登录属性对象，包含用户认证信息
             * @return ATPErrorCodeType 错误码，0表示登录请求发送成功，非0表示发送失败
             * 
             * 发送用户登录请求到ATP交易系统。登录结果通过OnLogin()回调通知。
             * 登录属性包含客户号/资金账号、营业部ID、密码、登录模式等信息。
             * 支持两种登录模式：客户号模式和资金账号模式。
             */
            ATPErrorCodeType  Login(const ATPLoginProperty* property);

            /**
             * @brief 用户登出接口
             * @return ATPErrorCodeType 错误码，0表示登出请求发送成功，非0表示发送失败
             * 
             * 发送用户登出请求，断开与ATP交易系统的会话连接。
             * 登出结果通过OnLogout()回调通知。登出后需要重新登录才能进行交易操作。
             */
            ATPErrorCodeType  Logout();

            /**
             * @brief 释放API实例资源
             * @return ATPErrorCodeType 错误码，0表示释放成功，非0表示释放失败
             * 
             * 显式释放当前API实例占用的资源，包括网络连接、回调线程等。
             * 释放后该实例将无法再进行任何操作。建议在不再使用API时主动调用。
             */
            ATPErrorCodeType  Release();

            /**
             * @brief 获取当前交易用户账号信息
             * @param[in] cust_id 客户号ID，可选参数，默认为空字符串
             * @return ATPCustomerInfo* 客户信息指针，失败时返回nullptr
             * 
             * @note 如果API未处于登录成功且会话保持状态，则返回空指针
             * 
             * 根据登录模式的不同行为：
             * - 匿名连接模式：返回当前连接登录的客户信息
             * - 网关用户模式：传入客户号时返回对应客户信息，未传入时返回最后一次登录成功的客户信息
             * 
             * 返回的信息包含客户号、资金账户、证券账户、分区信息等。
             */
            ATPCustomerInfo* GetCustomerInfo(const char* cust_id = "") const;

            /**
             * @brief 获取API版本号
             * @return const char* API版本字符串
             * 
             * 返回当前ATP量化API的版本信息，用于版本兼容性检查和问题诊断。
             * 版本格式通常为"x.y.z"的形式。
             */
            const char* GetVersion() const;

            /**
             * @brief 获取当前API对象实例ID
             * @return int32_t 实例ID，用于区分不同的API实例
             * 
             * 当系统中存在多个ATPQuantAPI实例时，通过实例ID可以区分不同的实例，
             * 便于日志记录和问题定位。
             */
            int32_t GetInstanceId() const;

            /**
             * @brief 获取处理后的终端识别码（静态方法）
             * @param[in] client_feature_code 原始终端识别码对象
             * @return const char* 处理完成的终端识别码字符串
             * 
             * 将ClientFeatureCode对象中的各种终端信息（IP地址、MAC地址、硬盘序列号等）
             * 组合处理成最终的终端识别码字符串，用于客户端身份验证。
             */
            static const char* GetClientFeatureCode(const ClientFeatureCode* client_feature_code);

            /**
             * @brief 获取当前用户连接信息
             * @return ATPConnectionInfo* 连接信息指针，失败时返回nullptr
             * 
             * 返回当前连接的详细信息，包含实例ID、多通道订阅结果等状态信息，
             * 用于连接状态监控和诊断。
             */
            ATPConnectionInfo* GetConnectionInfo() const;

        public:
            /**
             * @brief 现货集中竞价下单请求
             * @param[in] msg 现货委托消息指针，包含下单的完整信息
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 发送现货买卖委托到交易系统，支持多种订单类型（限价、市价等）。
             * 委托结果通过OnRspCashAuctionOrder()回调返回，
             * 订单状态变化通过OnRtnCashAuctionOrder()推送。
             */
            ATPErrorCodeType ReqCashAuctionOrder(const ATPReqCashAuctionOrderMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货委托撤单请求
             * @param[in] msg 撤单消息指针，包含要撤销的订单信息
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 发送撤单请求到交易系统，可通过原订单号或批次号进行撤单。
             * 撤单结果通过OnRspCashCancelOrder()回调返回。
             */
            ATPErrorCodeType ReqCashCancelOrder(const ATPReqCashCancelOrderMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货订单查询请求
             * @param[in] msg 订单查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询客户的订单信息，支持按多种条件过滤（时间、证券、状态等）。
             * 查询结果通过OnRspCashOrderQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashOrderQuery(const ATPReqCashOrderQueryMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货股份持仓查询请求
             * @param[in] msg 股份查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询客户的股份持仓信息，包含持仓数量、可用数量、成本价等。
             * 查询结果通过OnRspCashShareQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashShareQuery(const ATPReqCashShareQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货按证券账户划转资金请求
             * @param[in] msg 资金划转消息指针，包含划转的详细信息
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 在不同证券账户之间进行资金划转，用于多账户资金调配。
             * 划转结果通过OnRspCashExternalInsTETransFundResp()回调返回。
             */
            ATPErrorCodeType ReqCashExternalInsTETransFundReq(const ATPReqCashExternalInsTETransFundReqMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货资金划拨请求（转入/转出）
             * @param[in] msg 资金划拨消息指针，包含划拨方式和金额
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 进行资金的转入或转出操作，用于银证转账等资金流转。
             * 划拨结果通过OnRspCashExtFundTransferResult()回调返回。
             */
            ATPErrorCodeType ReqCashExtFundTransferOther(const ATPReqCashExtFundTransferOtherMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货成交记录查询请求
             * @param[in] msg 成交查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询客户的成交记录，支持按时间、证券等条件过滤。
             * 查询结果通过OnRspCashTradeOrderQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashTradeOrderQuery(const ATPReqCashTradeOrderQueryMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货证券信息查询请求
             * @param[in] msg 证券信息查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询证券的基本信息，如涨跌停价、交易规则等。
             * 查询结果通过OnRspCashExtQueryResultSecurityInfo()回调返回。
             */
            ATPErrorCodeType ReqCashExtQuerySecurityInfo(const ATPReqCashExtQuerySecurityInfoMsg* msg, const int64_t request_id);
            /**
             * @brief 现货资金查询请求
             * @param[in] msg 资金查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询客户的资金状况，包含可用资金、冻结资金等信息。
             * 查询结果通过OnRspCashFundQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashFundQuery(const ATPReqCashFundQueryMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货资产查询请求
             * @param[in] msg 资产查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询客户的总资产信息，包含资金和股票的综合资产状况。
             * 查询结果通过OnRspCashAssetQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashAssetQuery(const ATPReqCashAssetQueryMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货成交汇总查询请求
             * @param[in] msg 成交汇总查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询按证券汇总的成交统计信息，包含买卖均价、总成交量等。
             * 查询结果通过OnRspCashTradeCollectQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashTradeCollectQuery(const ATPReqCashTradeCollectQueryMsg* msg, const int64_t request_id);
            
            /**
             * @brief 现货最大可委托数量查询请求
             * @param[in] msg 最大可委托数查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询指定证券在当前条件下可委托的最大数量，用于风险控制。
             * 查询结果通过OnRspCashMaxOrderQueryResult()回调返回。
             */
            ATPErrorCodeType ReqCashMaxOrderQtyQuery(const ATPReqCashMaxOrderQtyQueryMsg* msg, const int64_t request_id);
            
            /**
             * @brief ETF基金信息查询请求
             * @param[in] msg ETF信息查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询ETF基金的基本信息，如申购赎回单位、净值等。
             * 查询结果通过OnRspExtQueryResultETFInfo()回调返回。
             */
            ATPErrorCodeType ReqExtQueryETFInfo(const ATPReqExtQueryETFInfoMsg* msg, const int64_t request_id);
            
            /**
             * @brief ETF成分股信息查询请求
             * @param[in] msg ETF成分股查询消息指针，包含查询条件
             * @param[in] request_id 请求标识符，用于匹配请求和响应
             * @return ATPErrorCodeType 错误码，0表示请求发送成功，非0表示发送失败
             * 
             * 查询ETF基金的成分股详细信息，用于ETF申购赎回操作。
             * 查询结果通过OnRspExtQueryResultETFComponentInfo()回调返回。
             */
            ATPErrorCodeType ReqExtQueryETFComponentInfo(const ATPReqExtQueryETFComponentInfoMsg* msg, const int64_t request_id);

        private:
            atp::quant_api::Context* context_;    // 内部上下文对象指针，封装实现细节
        };
    } //namespace quant_api    // 结束量化API命名空间
} //namespace atp            // 结束ATP命名空间

#endif    //ATP_QUANT_API_H_      // 结束头文件保护