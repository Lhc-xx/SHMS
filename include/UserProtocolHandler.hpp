#ifndef SMART_HOME_USER_PROTOCOL_HANDLER_HPP
#define SMART_HOME_USER_PROTOCOL_HANDLER_HPP

#include "Protocol.hpp"
#include "UserService.hpp"

#include <mutex>
#include <string>

namespace shms {

// 将用户注册/登录协议请求交给 UserService，并生成对应响应。
// 业务失败会编码为响应结果码返回；只有报文格式或消息类型错误才返回 false。
class UserProtocolHandler {
public:
    explicit UserProtocolHandler(UserService& service);
    UserProtocolHandler(const UserProtocolHandler&) = delete;
    UserProtocolHandler& operator=(const UserProtocolHandler&) = delete;

    // 处理一条用户模块消息，response 输出完整的消息类型和消息体。
    bool handle(const ProtocolMessage& request, ProtocolMessage* response);

    std::string lastError() const;

private:
    bool setError(const std::string& message);
    void clearError();

    UserService& service_;
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_USER_PROTOCOL_HANDLER_HPP 头文件保护宏
