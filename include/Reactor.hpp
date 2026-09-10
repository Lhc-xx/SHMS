#ifndef SMART_HOME_REACTOR_HPP
#define SMART_HOME_REACTOR_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace shms {

// 服务端单个 Reactor 线程使用的 Linux epoll 事件循环。
// 此类不拥有已注册的套接字；调用方从 Reactor 中移除套接字后仍负责关闭它们。
class Reactor {
public:
    using EventHandler = std::function<void(int, std::uint32_t)>;

    Reactor();
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    ~Reactor();

    // 创建 epoll 实例和内部 eventfd。其他线程调用 stop() 时，eventfd 用于
    // 唤醒阻塞中的 epoll_wait。
    bool initialize();

    // 释放 Reactor 的内部描述符。已注册的客户端描述符只会被注销，不会在
    // 此处关闭。
    void shutdown();

    // 注册描述符及其回调。add/modify/remove 应在 Reactor 线程中执行；
    // stop() 可以由控制线程安全调用。
    bool add(int fd, std::uint32_t events, EventHandler handler);
    bool modify(int fd, std::uint32_t events, EventHandler handler);
    bool remove(int fd);

    // 等待并分发一批事件。超时值 -1 表示永久等待，0 表示非阻塞轮询。
    // 返回值为已分发的客户端回调数量；发生 Reactor 错误时返回 -1。
    int pollOnce(int timeoutMs);

    // 持续运行直到收到 stop() 请求。事件循环固定由一个线程拥有，确保
    // 回调可以安全执行普通 Reactor 操作。
    bool run();

    // 请求优雅停止，并唤醒阻塞中的 run/poll 调用。
    void stop();

    bool initialized() const;
    bool running() const;
    std::size_t registeredCount() const;
    std::string lastError() const;

private:
    bool setError(const std::string& message);
    void clearError();
    void wake();
    void drainWake();

    int epollFd_;
    int wakeFd_;
    mutable std::mutex mutex_;
    std::map<int, EventHandler> handlers_;
    std::atomic<bool> running_;
    std::atomic<bool> stopRequested_;

    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_REACTOR_HPP 头文件保护宏
