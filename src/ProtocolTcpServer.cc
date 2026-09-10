#include "ProtocolTcpServer.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace shms {

ProtocolTcpServer::ProtocolTcpServer(Reactor& reactor,
                                     const std::string& bindIp,
                                     std::uint16_t port,
                                     int backlog)
    : tcpServer_(reactor, bindIp, port, backlog) {
    tcpServer_.setConnectionHandler(
        [this](TcpConnection& connection) { handleConnection(connection); });
    tcpServer_.setMessageHandler(
        [this](TcpConnection& connection, const std::string& data) {
            handleData(connection, data);
        });
    tcpServer_.setCloseHandler(
        [this](TcpConnection& connection) { handleClose(connection); });
}

ProtocolTcpServer::~ProtocolTcpServer() {
    stop();
}

bool ProtocolTcpServer::start() {
    if (!tcpServer_.start()) {
        return setError(tcpServer_.lastError());
    }
    clearError();
    return true;
}

void ProtocolTcpServer::stop() {
    tcpServer_.stop();
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}

bool ProtocolTcpServer::running() const {
    return tcpServer_.running();
}

std::uint16_t ProtocolTcpServer::port() const {
    return tcpServer_.port();
}

std::size_t ProtocolTcpServer::connectionCount() const {
    return tcpServer_.connectionCount();
}

std::size_t ProtocolTcpServer::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

void ProtocolTcpServer::setSessionConfigurer(SessionConfigurer configurer) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionConfigurer_ = std::move(configurer);
}

std::string ProtocolTcpServer::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void ProtocolTcpServer::handleConnection(TcpConnection& connection) {
    TcpConnection* connectionPointer = &connection;
    std::shared_ptr<ProtocolSession> session(new ProtocolSession(
        [connectionPointer](const std::string& frame) {
            return connectionPointer->send(frame);
        }));

    SessionConfigurer configurer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configurer = sessionConfigurer_;
    }
    if (configurer && !configurer(*session)) {
        setError("protocol session configuration failed");
        throw std::runtime_error(lastError());
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        SessionEntry entry;
        entry.connection = &connection;
        entry.session = session;
        sessions_[connection.fd()] = entry;
    }
    clearError();
}

void ProtocolTcpServer::handleData(TcpConnection& connection,
                                   const std::string& data) {
    std::shared_ptr<ProtocolSession> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<int, SessionEntry>::const_iterator it =
            sessions_.find(connection.fd());
        if (it != sessions_.end()) {
            session = it->second.session;
        }
    }
    if (!session) {
        setError("protocol session is missing for TCP connection");
        connection.close();
        return;
    }
    if (!session->onData(data)) {
        setError(session->lastError());
        connection.close();
    }
}

void ProtocolTcpServer::handleClose(TcpConnection& connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::map<int, SessionEntry>::iterator it = sessions_.begin();
         it != sessions_.end();
         ++it) {
        if (it->second.connection == &connection) {
            sessions_.erase(it);
            return;
        }
    }
}

bool ProtocolTcpServer::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void ProtocolTcpServer::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
