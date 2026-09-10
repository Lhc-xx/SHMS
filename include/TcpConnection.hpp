#ifndef SMART_HOME_TCP_CONNECTION_HPP
#define SMART_HOME_TCP_CONNECTION_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace shms {

// Owns one accepted TCP socket. The socket is configured as non-blocking and
// is closed exactly once by close() or the destructor.
class TcpConnection {
public:
    using DataHandler =
        std::function<void(TcpConnection&, const std::string&)>;
    using CloseHandler = std::function<void(TcpConnection&, int)>;

    explicit TcpConnection(int fd);
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    ~TcpConnection();

    // Queue bytes for transmission. The Reactor callback should include
    // EPOLLOUT while hasPendingWrite() is true so the queue can drain.
    bool send(const std::string& data);

    // Consume one Reactor event mask. Read and write callbacks are invoked on
    // the Reactor thread; callback exceptions close only this connection.
    bool handleEvents(std::uint32_t events);

    // Close the socket and notify the close handler once. The connection does
    // not close any other descriptor or remove itself from the Reactor.
    void close();

    int fd() const;
    bool valid() const;
    bool closed() const;
    bool hasPendingWrite() const;
    std::size_t pendingWriteBytes() const;

    void setDataHandler(DataHandler handler);
    void setCloseHandler(CloseHandler handler);

    std::string lastError() const;

private:
    static const std::size_t kMaxPendingWriteBytes = 4 * 1024 * 1024;

    bool readAvailable();
    bool flushWrite();
    bool setError(const std::string& message);
    void clearError();

    mutable std::mutex mutex_;
    int fd_;
    std::deque<std::string> writeQueue_;
    std::size_t writeOffset_;
    std::size_t pendingWriteBytes_;
    DataHandler dataHandler_;
    CloseHandler closeHandler_;

    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // namespace shms

#endif  // SMART_HOME_TCP_CONNECTION_HPP
