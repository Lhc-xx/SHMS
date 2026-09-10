#include "TcpConnection.hpp"

#include <cerrno>
#include <cstring>
#include <exception>
#include <sstream>
#include <utility>

#if defined(__linux__)
#include <fcntl.h>
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

}  // 匿名命名空间

namespace shms {

TcpConnection::TcpConnection(int fd)
    : fd_(fd),
      writeOffset_(0),
      pendingWriteBytes_(0) {
#if defined(__linux__)
    if (fd_ < 0) {
        setError("TcpConnection requires a valid socket fd");
    } else if (!makeNonBlocking(fd_)) {
        const int errorNumber = errno;
        ::close(fd_);
        fd_ = -1;
        setError(systemError("configure non-blocking socket failed",
                             errorNumber));
    }
#else
    (void)fd;
    fd_ = -1;
    setError("TcpConnection requires Linux socket support");
#endif
}

TcpConnection::~TcpConnection() {
    close();
}

bool TcpConnection::send(const std::string& data) {
    if (data.empty()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ < 0) {
        return setError("cannot send on a closed connection");
    }
    if (data.size() > kMaxPendingWriteBytes - pendingWriteBytes_) {
        return setError("pending write buffer limit exceeded");
    }

    writeQueue_.push_back(data);
    pendingWriteBytes_ += data.size();
    clearError();
    return true;
}

bool TcpConnection::handleEvents(std::uint32_t events) {
#if defined(__linux__)
    if (closed()) {
        return false;
    }

    // 先执行读取，使内核中已经排队的数据在处理对端半关闭之前交付。
    // 任何终止性错误仍会关闭套接字。
    if ((events & EPOLLIN) != 0 && !readAvailable()) {
        return false;
    }
    if ((events & EPOLLOUT) != 0 && !flushWrite()) {
        return false;
    }
    if ((events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
        setError("peer closed or socket reported an error");
        close();
        return false;
    }
    return !closed();
#else
    (void)events;
    return setError("TcpConnection requires Linux socket support");
#endif
}

void TcpConnection::close() {
    int closedFd = -1;
    CloseHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fd_ < 0) {
            return;
        }
        closedFd = fd_;
        fd_ = -1;
        writeQueue_.clear();
        writeOffset_ = 0;
        pendingWriteBytes_ = 0;
        handler = closeHandler_;
    }

#if defined(__linux__)
    ::close(closedFd);
#endif

    if (handler) {
        try {
            handler(*this, closedFd);
        } catch (const std::exception& exception) {
            setError(std::string("close handler failed: ") + exception.what());
        } catch (...) {
            setError("close handler failed with a non-standard exception");
        }
    }
}

int TcpConnection::fd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fd_;
}

bool TcpConnection::valid() const {
    return !closed();
}

bool TcpConnection::closed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fd_ < 0;
}

bool TcpConnection::hasPendingWrite() const {
    return pendingWriteBytes() != 0;
}

std::size_t TcpConnection::pendingWriteBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingWriteBytes_;
}

void TcpConnection::setDataHandler(DataHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    dataHandler_ = std::move(handler);
}

void TcpConnection::setCloseHandler(CloseHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    closeHandler_ = std::move(handler);
}

std::string TcpConnection::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool TcpConnection::readAvailable() {
#if defined(__linux__)
    char buffer[8192];
    while (true) {
        int socketFd = -1;
        DataHandler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            socketFd = fd_;
            handler = dataHandler_;
        }
        if (socketFd < 0) {
            return false;
        }

        const ssize_t received = ::recv(socketFd, buffer, sizeof(buffer), 0);
        if (received > 0) {
            if (handler) {
                try {
                    handler(*this,
                            std::string(buffer,
                                        static_cast<std::size_t>(received)));
                } catch (const std::exception& exception) {
                    setError(std::string("data handler failed: ") +
                             exception.what());
                    close();
                    return false;
                } catch (...) {
                    setError("data handler failed with a non-standard "
                             "exception");
                    close();
                    return false;
                }
            }
            if (closed()) {
                return false;
            }
            continue;
        }

        if (received == 0) {
            close();
            return false;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }

        const int errorNumber = errno;
        setError(systemError("recv failed", errorNumber));
        close();
        return false;
    }
#else
    return setError("TcpConnection requires Linux socket support");
#endif
}

bool TcpConnection::flushWrite() {
#if defined(__linux__)
    while (true) {
        int socketFd = -1;
        ssize_t sent = 0;
        int errorNumber = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            socketFd = fd_;
            if (socketFd < 0) {
                return false;
            }
            if (writeQueue_.empty()) {
                return true;
            }

            const std::string& data = writeQueue_.front();
            const std::size_t remaining = data.size() - writeOffset_;
            sent = ::send(socketFd,
                          data.data() + writeOffset_,
                          remaining,
                          MSG_NOSIGNAL);
            if (sent > 0) {
                writeOffset_ += static_cast<std::size_t>(sent);
                pendingWriteBytes_ -= static_cast<std::size_t>(sent);
                if (writeOffset_ == data.size()) {
                    writeQueue_.pop_front();
                    writeOffset_ = 0;
                }
                continue;
            }
            if (sent < 0) {
                errorNumber = errno;
            }
        }

        if (sent < 0 && (errorNumber == EAGAIN || errorNumber == EWOULDBLOCK)) {
            return true;
        }
        setError(systemError("send failed", errorNumber));
        close();
        return false;
    }
#else
    return setError("TcpConnection requires Linux socket support");
#endif
}

bool TcpConnection::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void TcpConnection::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
