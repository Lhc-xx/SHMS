#ifndef SMART_HOME_PROTOCOL_TCP_SERVER_HPP
#define SMART_HOME_PROTOCOL_TCP_SERVER_HPP

#include "ProtocolSession.hpp"
#include "TcpServer.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace shms {

// 将 TcpServer 和 ProtocolSession 连接起来，为每个 TCP 连接维护独立会话。
// 协议格式错误、未知消息或响应发送失败都会关闭当前连接，避免继续使用失步的字节流。
class ProtocolTcpServer {
public:
    using SessionConfigurer = std::function<bool(ProtocolSession&)>;

    ProtocolTcpServer(Reactor& reactor,
                      const std::string& bindIp,
                      std::uint16_t port,
                      int backlog = 128);
    ProtocolTcpServer(const ProtocolTcpServer&) = delete;
    ProtocolTcpServer& operator=(const ProtocolTcpServer&) = delete;

    ~ProtocolTcpServer();

    // 启动底层 TCP 服务端。启动前可设置会话配置器以注册业务处理器。
    bool start();
    void stop();

    bool running() const;
    std::uint16_t port() const;
    std::size_t connectionCount() const;
    std::size_t sessionCount() const;

    // 为每条新连接配置协议处理器；返回 false 时该连接不会继续提供服务。
    void setSessionConfigurer(SessionConfigurer configurer);

    std::string lastError() const;

private:
    struct SessionEntry {
        SessionEntry() : connection(nullptr) {}

        TcpConnection* connection;
        std::shared_ptr<ProtocolSession> session;
    };

    void handleConnection(TcpConnection& connection);
    void handleData(TcpConnection& connection, const std::string& data);
    void handleClose(TcpConnection& connection);
    bool setError(const std::string& message);
    void clearError();

    TcpServer tcpServer_;
    mutable std::mutex mutex_;
    std::map<int, SessionEntry> sessions_;
    SessionConfigurer sessionConfigurer_;

    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_PROTOCOL_TCP_SERVER_HPP 头文件保护宏
