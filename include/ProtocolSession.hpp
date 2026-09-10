#ifndef SMART_HOME_PROTOCOL_SESSION_HPP
#define SMART_HOME_PROTOCOL_SESSION_HPP

#include "MessageDispatcher.hpp"
#include "Protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace shms {

enum class SystemMessageType : std::uint32_t {
    HeartbeatRequest = 9001,
    HeartbeatResponse = 9002
};

// 单条 TCP 连接的协议会话，维护独立的分包/粘包解析状态并分发完整消息。
// 发送函数由 TcpConnection::send 适配，便于将协议层与具体连接所有权分离。
class ProtocolSession {
public:
    using SendHandler = std::function<bool(const std::string&)>;

    explicit ProtocolSession(SendHandler sender,
                             std::size_t maxBodySize = 1024 * 1024);
    ProtocolSession(const ProtocolSession&) = delete;
    ProtocolSession& operator=(const ProtocolSession&) = delete;

    // 注册业务消息处理器。心跳处理器会在构造时自动注册。
    bool registerHandler(std::uint32_t type, MessageDispatcher::Handler handler);
    bool unregisterHandler(std::uint32_t type);

    // 追加 TCP 收到的字节，解析并分发当前已有的全部完整数据帧。
    // 解析失败、未知消息或响应发送失败时返回 false。
    bool onData(const void* data, std::size_t size);
    bool onData(const std::string& data);

    // 编码并发送一条完整协议消息。
    bool sendMessage(std::uint32_t type,
                     const std::string& body,
                     std::string* error = nullptr);

    // 保存当前连接登录成功后的用户名，供后续业务消息进行权限校验和操作日志记录。
    bool setAuthenticatedUser(const std::string& username,
                              std::string* error = nullptr);
    void clearAuthenticatedUser();
    bool authenticated() const;
    std::string authenticatedUser() const;

    bool failed() const;
    std::size_t bufferedBytes() const;
    std::string lastError() const;

private:
    bool sendHeartbeatResponse();
    bool setError(const std::string& message);
    void clearError();

    SendHandler sender_;
    ProtocolParser parser_;
    MessageDispatcher dispatcher_;
    mutable std::mutex contextMutex_;
    std::string authenticatedUser_;
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_PROTOCOL_SESSION_HPP 头文件保护宏
