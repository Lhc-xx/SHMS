#ifndef SMART_HOME_USER_PROTOCOL_HPP
#define SMART_HOME_USER_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace shms {

enum class UserMessageType : std::uint32_t {
    RegisterRequest = 1001,
    RegisterResponse = 1002,
    LoginRequest = 1003,
    LoginResponse = 1004
};

enum class UserResponseCode : std::uint32_t {
    Success = 0,
    InvalidArgument = 1,
    UserAlreadyExists = 2,
    UserNotFound = 3,
    InvalidPassword = 4,
    StorageError = 5,
    PasswordError = 6,
    ProtocolError = 7
};

struct UserCredentials {
    std::string username;
    std::string password;
};

struct UserResponse {
    UserResponseCode code;
    std::uint64_t userId;
    std::string message;

    UserResponse()
        : code(UserResponseCode::ProtocolError), userId(0) {}
};

// 用户协议消息体编解码器。请求使用两个长度前缀字符串，响应使用结果码、
// 8 字节用户编号和一个长度前缀消息，所有整数均采用网络字节序。
class UserProtocolCodec {
public:
    static const std::size_t kMaxUsernameBytes = 20;
    static const std::size_t kMaxPasswordBytes = 128;
    static const std::size_t kMaxMessageBytes = 512;

    // 编码注册或登录请求体。
    static bool encodeCredentials(const UserCredentials& credentials,
                                  std::string* body,
                                  std::string* error = nullptr);

    // 解码注册或登录请求体，并拒绝截断、超长或多余数据。
    static bool decodeCredentials(const std::string& body,
                                  UserCredentials* credentials,
                                  std::string* error = nullptr);

    // 编码注册或登录响应体。
    static bool encodeResponse(const UserResponse& response,
                               std::string* body,
                               std::string* error = nullptr);

    // 解码注册或登录响应体。
    static bool decodeResponse(const std::string& body,
                               UserResponse* response,
                               std::string* error = nullptr);
};

}  // shms 命名空间

#endif  // SMART_HOME_USER_PROTOCOL_HPP 头文件保护宏
