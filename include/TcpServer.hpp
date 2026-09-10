#ifndef SMART_HOME_TCP_SERVER_HPP
#define SMART_HOME_TCP_SERVER_HPP

#include "Reactor.hpp"
#include "TcpConnection.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace shms {

// 将已接收连接接入 Reactor 的非阻塞 IPv4 监听器。
// 协议解析有意放在此类之外，由后续 TLV/协议层负责。
class TcpServer {
public:
    using ConnectionHandler = std::function<void(TcpConnection&)>;
    using MessageHandler =
        std::function<void(TcpConnection&, const std::string&)>;

    TcpServer(Reactor& reactor,
              const std::string& bindIp,
              std::uint16_t port,
              int backlog = 128);
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    ~TcpServer();

    // 执行绑定和监听。测试可以传入端口 0，启动成功后会由内核分配端口，
    // 并通过 port() 返回。
    bool start();

    // 停止接收新客户端，关闭活动连接，并从 Reactor 中注销所有描述符。
    // 客户端描述符由此类负责拥有。
    void stop();

    bool running() const;
    std::uint16_t port() const;
    std::size_t connectionCount() const;

    void setConnectionHandler(ConnectionHandler handler);
    void setMessageHandler(MessageHandler handler);
    void setCloseHandler(ConnectionHandler handler);

    std::string lastError() const;

private:
    bool acceptConnections();
    bool registerConnection(int fd);
    void handleConnectionEvent(int fd, std::uint32_t events);
    void handleConnectionClosed(TcpConnection& connection, int fd);
    std::shared_ptr<TcpConnection> findConnection(int fd) const;
    bool setError(const std::string& message);
    void clearError();

    Reactor& reactor_;
    const std::string bindIp_;
    const std::uint16_t requestedPort_;
    const int backlog_;

    mutable std::mutex mutex_;
    int listenFd_;
    std::uint16_t boundPort_;
    std::map<int, std::shared_ptr<TcpConnection> > connections_;
    ConnectionHandler connectionHandler_;
    MessageHandler messageHandler_;
    ConnectionHandler closeHandler_;
    std::atomic<bool> running_;

    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_TCP_SERVER_HPP 头文件保护宏
