#include "TcpServer.hpp"

#include <cerrno>
#include <cstring>
#include <exception>
#include <sstream>
#include <vector>

#if defined(__linux__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#if defined(__linux__)
std::string systemError(const char* operation, int errorNumber) {
    std::ostringstream stream;
    stream << operation << ": " << std::strerror(errorNumber);
    return stream.str();
}

bool makeNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    const int descriptorFlags = fcntl(fd, F_GETFD, 0);
    return descriptorFlags >= 0 &&
           fcntl(fd, F_SETFD, descriptorFlags | FD_CLOEXEC) == 0;
}
#endif

}  // namespace

namespace shms {

TcpServer::TcpServer(Reactor& reactor,
                     const std::string& bindIp,
                     std::uint16_t port,
                     int backlog)
    : reactor_(reactor),
      bindIp_(bindIp),
      requestedPort_(port),
      backlog_(backlog),
      listenFd_(-1),
      boundPort_(0),
      running_(false) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
#if defined(__linux__)
    if (running()) {
        return true;
    }
    if (!reactor_.initialized()) {
        return setError("cannot start TCP server with an uninitialized Reactor");
    }
    if (backlog_ <= 0) {
        return setError("TCP listen backlog must be positive");
    }

    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        return setError(systemError("socket failed", errno));
    }
    if (!makeNonBlocking(listener)) {
        const int errorNumber = errno;
        ::close(listener);
        return setError(systemError("configure listener failed", errorNumber));
    }

    int reuseAddress = 1;
    if (setsockopt(listener,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuseAddress,
                   sizeof(reuseAddress)) < 0) {
        const int errorNumber = errno;
        ::close(listener);
        return setError(systemError("setsockopt SO_REUSEADDR failed",
                                    errorNumber));
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(requestedPort_);
    if (bindIp_ == "0.0.0.0" || bindIp_ == "*") {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, bindIp_.c_str(), &address.sin_addr) != 1) {
        ::close(listener);
        return setError("bind ip must be a valid IPv4 address");
    }

    if (::bind(listener,
               reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) < 0) {
        const int errorNumber = errno;
        ::close(listener);
        return setError(systemError("bind failed", errorNumber));
    }
    if (::listen(listener, backlog_) < 0) {
        const int errorNumber = errno;
        ::close(listener);
        return setError(systemError("listen failed", errorNumber));
    }

    sockaddr_in boundAddress;
    std::memset(&boundAddress, 0, sizeof(boundAddress));
    socklen_t boundAddressLength = sizeof(boundAddress);
    if (getsockname(listener,
                    reinterpret_cast<sockaddr*>(&boundAddress),
                    &boundAddressLength) < 0) {
        const int errorNumber = errno;
        ::close(listener);
        return setError(systemError("getsockname failed", errorNumber));
    }

    if (!reactor_.add(
            listener,
            EPOLLIN,
            [this](int fd, std::uint32_t events) {
                if ((events & EPOLLIN) != 0) {
                    acceptConnections();
                }
                if ((events & (EPOLLERR | EPOLLHUP)) != 0) {
                    setError("TCP listener reported an error");
                }
                (void)fd;
            })) {
        const std::string error = reactor_.lastError();
        ::close(listener);
        return setError("register listener failed: " + error);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        listenFd_ = listener;
        boundPort_ = ntohs(boundAddress.sin_port);
    }
    running_.store(true);
    clearError();
    return true;
#else
    return setError("TcpServer requires Linux socket support");
#endif
}

void TcpServer::stop() {
#if defined(__linux__)
    running_.store(false);

    int listener = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listener = listenFd_;
        listenFd_ = -1;
        boundPort_ = 0;
    }
    if (listener >= 0) {
        reactor_.remove(listener);
        ::close(listener);
    }

    std::vector<std::shared_ptr<TcpConnection> > connections;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::map<int, std::shared_ptr<TcpConnection> >::const_iterator it =
                 connections_.begin();
             it != connections_.end();
             ++it) {
            connections.push_back(it->second);
        }
    }
    for (std::vector<std::shared_ptr<TcpConnection> >::iterator it =
             connections.begin();
         it != connections.end();
         ++it) {
        (*it)->close();
    }
#endif
}

bool TcpServer::running() const {
    return running_.load();
}

std::uint16_t TcpServer::port() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return boundPort_;
}

std::size_t TcpServer::connectionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

void TcpServer::setConnectionHandler(ConnectionHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    connectionHandler_ = std::move(handler);
}

void TcpServer::setMessageHandler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    messageHandler_ = std::move(handler);
}

void TcpServer::setCloseHandler(ConnectionHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    closeHandler_ = std::move(handler);
}

std::string TcpServer::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool TcpServer::acceptConnections() {
#if defined(__linux__)
    while (running()) {
        sockaddr_storage peerAddress;
        std::memset(&peerAddress, 0, sizeof(peerAddress));
        socklen_t peerAddressLength = sizeof(peerAddress);
        const int listener = [&]() {
            std::lock_guard<std::mutex> lock(mutex_);
            return listenFd_;
        }();
        if (listener < 0) {
            return false;
        }

        const int client = ::accept(listener,
                                    reinterpret_cast<sockaddr*>(&peerAddress),
                                    &peerAddressLength);
        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            }
            setError(systemError("accept failed", errno));
            return false;
        }

        if (!makeNonBlocking(client)) {
            const int errorNumber = errno;
            ::close(client);
            setError(systemError("configure client socket failed",
                                 errorNumber));
            continue;
        }
        if (!registerConnection(client)) {
            // registerConnection transfers ownership to TcpConnection before
            // it can fail; its destructor closes the client descriptor.
            continue;
        }
    }
    return true;
#else
    return setError("TcpServer requires Linux socket support");
#endif
}

bool TcpServer::registerConnection(int fd) {
#if defined(__linux__)
    std::shared_ptr<TcpConnection> connection(new TcpConnection(fd));
    if (!connection->valid()) {
        setError(connection->lastError());
        return false;
    }

    connection->setDataHandler(
        [this](TcpConnection& current, const std::string& data) {
            MessageHandler handler;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                handler = messageHandler_;
            }
            if (handler) {
                handler(current, data);
            }
        });
    connection->setCloseHandler(
        [this](TcpConnection& current, int closedFd) {
            handleConnectionClosed(current, closedFd);
        });

    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_[connection->fd()] = connection;
    }

    if (!reactor_.add(
            connection->fd(),
            EPOLLIN | EPOLLRDHUP,
            [this](int currentFd, std::uint32_t events) {
                handleConnectionEvent(currentFd, events);
            })) {
        const std::string error = reactor_.lastError();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.erase(connection->fd());
        }
        setError("register client failed: " + error);
        return false;
    }

    ConnectionHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handler = connectionHandler_;
    }
    if (handler) {
        try {
            handler(*connection);
        } catch (const std::exception& exception) {
            setError(std::string("connection handler failed: ") +
                     exception.what());
            connection->close();
            return false;
        } catch (...) {
            setError("connection handler failed with a non-standard "
                     "exception");
            connection->close();
            return false;
        }
    }
    return true;
#else
    (void)fd;
    return setError("TcpServer requires Linux socket support");
#endif
}

void TcpServer::handleConnectionEvent(int fd, std::uint32_t events) {
#if defined(__linux__)
    std::shared_ptr<TcpConnection> connection = findConnection(fd);
    if (!connection || !connection->handleEvents(events)) {
        return;
    }

    std::uint32_t nextEvents = EPOLLIN | EPOLLRDHUP;
    if (connection->hasPendingWrite()) {
        nextEvents |= EPOLLOUT;
    }
    if (!reactor_.modify(
            fd,
            nextEvents,
            [this](int currentFd, std::uint32_t currentEvents) {
                handleConnectionEvent(currentFd, currentEvents);
            })) {
        connection->close();
    }
#else
    (void)fd;
    (void)events;
#endif
}

void TcpServer::handleConnectionClosed(TcpConnection& connection, int fd) {
    ConnectionHandler handler;
    bool owned = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<int, std::shared_ptr<TcpConnection> >::iterator it =
            connections_.find(fd);
        if (it != connections_.end() && it->second.get() == &connection) {
            connections_.erase(it);
            handler = closeHandler_;
            owned = true;
        }
    }
    if (!owned) {
        return;
    }

    reactor_.remove(fd);
    if (handler) {
        try {
            handler(connection);
        } catch (const std::exception& exception) {
            setError(std::string("close handler failed: ") + exception.what());
        } catch (...) {
            setError("close handler failed with a non-standard exception");
        }
    }
}

std::shared_ptr<TcpConnection> TcpServer::findConnection(int fd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, std::shared_ptr<TcpConnection> >::const_iterator it =
        connections_.find(fd);
    return it == connections_.end() ? std::shared_ptr<TcpConnection>()
                                    : it->second;
}

bool TcpServer::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void TcpServer::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // namespace shms
