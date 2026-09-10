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

// Non-blocking IPv4 listener that adapts accepted connections to Reactor.
// Protocol parsing is deliberately outside this class and belongs to the
// following TLV/protocol layer.
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

    // Bind and listen. Port zero is accepted for tests and is replaced by the
    // kernel-assigned port() after start succeeds.
    bool start();

    // Stop accepting new clients, close active connections, and unregister
    // every descriptor from the Reactor. Client descriptors are owned here.
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

}  // namespace shms

#endif  // SMART_HOME_TCP_SERVER_HPP
