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

// Linux epoll based event loop used by the server's single Reactor thread.
// The class does not own registered sockets; the caller remains responsible
// for closing them after removing them from the Reactor.
class Reactor {
public:
    using EventHandler = std::function<void(int, std::uint32_t)>;

    Reactor();
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    ~Reactor();

    // Create the epoll instance and an internal eventfd used to wake a
    // blocked epoll_wait when stop() is called from another thread.
    bool initialize();

    // Release the Reactor's internal descriptors. Registered client
    // descriptors are only deregistered and are never closed here.
    void shutdown();

    // Register a descriptor and its callback. add/modify/remove are expected
    // to run on the Reactor thread; stop() is safe from a control thread.
    bool add(int fd, std::uint32_t events, EventHandler handler);
    bool modify(int fd, std::uint32_t events, EventHandler handler);
    bool remove(int fd);

    // Wait for and dispatch one batch of events. A timeout of -1 waits
    // forever, while zero performs a non-blocking poll. The return value is
    // the number of client callbacks dispatched, or -1 on a Reactor error.
    int pollOnce(int timeoutMs);

    // Run until stop() is requested. The loop is intentionally owned by one
    // thread so callbacks can safely perform normal Reactor operations.
    bool run();

    // Request a graceful stop and wake a blocked run/poll call.
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

}  // namespace shms

#endif  // SMART_HOME_REACTOR_HPP
