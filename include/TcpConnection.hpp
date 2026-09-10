#ifndef SMART_HOME_TCP_CONNECTION_HPP
#define SMART_HOME_TCP_CONNECTION_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace shms {

// 拥有一个已接收的 TCP 套接字。套接字配置为非阻塞模式，并且只会由
// close() 或析构函数关闭一次。
class TcpConnection {
public:
    using DataHandler =
        std::function<void(TcpConnection&, const std::string&)>;
    using CloseHandler = std::function<void(TcpConnection&, int)>;

    explicit TcpConnection(int fd);
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    ~TcpConnection();

    // 将待发送字节加入队列。当 hasPendingWrite() 为 true 时，Reactor
    // 回调应包含 EPOLLOUT，以便逐步发送队列内容。
    bool send(const std::string& data);

    // 处理一次 Reactor 事件掩码。读写回调都在 Reactor 线程中调用；回调
    // 抛出的异常只会关闭当前连接。
    bool handleEvents(std::uint32_t events);

    // 关闭套接字并通知一次关闭处理器。连接不会关闭其他描述符，也不会将
    // 自身从 Reactor 中移除。
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

}  // shms 命名空间

#endif  // SMART_HOME_TCP_CONNECTION_HPP 头文件保护宏
