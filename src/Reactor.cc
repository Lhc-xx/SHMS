#include "Reactor.hpp"

#include <cerrno>
#include <cstring>
#include <exception>
#include <sstream>
#include <vector>

#if defined(__linux__)
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

namespace {

#if defined(__linux__)
std::string systemError(const char* operation, int errorNumber) {
    std::ostringstream stream;
    stream << operation << ": " << std::strerror(errorNumber);
    return stream.str();
}
#endif

}  // namespace

namespace shms {

Reactor::Reactor()
    : epollFd_(-1),
      wakeFd_(-1),
      running_(false),
      stopRequested_(false) {}

Reactor::~Reactor() {
    // The owner should call shutdown after run() returns. stop() is still
    // issued here so a blocked loop can observe the destruction request.
    stop();
    shutdown();
}

bool Reactor::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (epollFd_ >= 0) {
        return true;
    }

#if defined(__linux__)
    const int epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) {
        return setError(systemError("epoll_create1 failed", errno));
    }

    const int wakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeFd < 0) {
        const int errorNumber = errno;
        close(epollFd);
        return setError(systemError("eventfd failed", errorNumber));
    }

    epoll_event wakeEvent;
    std::memset(&wakeEvent, 0, sizeof(wakeEvent));
    wakeEvent.events = EPOLLIN;
    wakeEvent.data.fd = wakeFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, wakeFd, &wakeEvent) < 0) {
        const int errorNumber = errno;
        close(wakeFd);
        close(epollFd);
        return setError(systemError("epoll_ctl add wake fd failed",
                                    errorNumber));
    }

    epollFd_ = epollFd;
    wakeFd_ = wakeFd;
    stopRequested_.store(false);
    clearError();
    return true;
#else
    return setError("Reactor requires Linux epoll support");
#endif
}

void Reactor::shutdown() {
    stop();

    // Closing descriptors while another thread is inside epoll_wait would
    // race with the kernel call. The public lifecycle requires run() to have
    // returned before shutdown(); retain the descriptors if that is violated.
    if (running()) {
        setError("cannot shut down a running Reactor");
        return;
    }

    int epollFd = -1;
    int wakeFd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        epollFd = epollFd_;
        wakeFd = this->wakeFd_;
        epollFd_ = -1;
        wakeFd_ = -1;
        handlers_.clear();
    }

#if defined(__linux__)
    if (wakeFd >= 0) {
        close(wakeFd);
    }
    if (epollFd >= 0) {
        close(epollFd);
    }
#else
    (void)epollFd;
    (void)wakeFd;
#endif
}

bool Reactor::add(int fd, std::uint32_t events, EventHandler handler) {
    if (fd < 0 || events == 0 || !handler) {
        return setError("add requires a valid fd, events, and callback");
    }

#if defined(__linux__)
    std::lock_guard<std::mutex> lock(mutex_);
    if (epollFd_ < 0) {
        return setError("Reactor is not initialized");
    }
    if (fd == wakeFd_) {
        return setError("the internal wake fd cannot be registered");
    }
    if (handlers_.find(fd) != handlers_.end()) {
        return setError("fd is already registered");
    }

    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.fd = fd;
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        return setError(systemError("epoll_ctl add failed", errno));
    }

    handlers_[fd] = std::move(handler);
    clearError();
    return true;
#else
    (void)fd;
    (void)events;
    (void)handler;
    return setError("Reactor requires Linux epoll support");
#endif
}

bool Reactor::modify(int fd,
                     std::uint32_t events,
                     EventHandler handler) {
    if (fd < 0 || events == 0 || !handler) {
        return setError("modify requires a valid fd, events, and callback");
    }

#if defined(__linux__)
    std::lock_guard<std::mutex> lock(mutex_);
    if (epollFd_ < 0) {
        return setError("Reactor is not initialized");
    }
    std::map<int, EventHandler>::iterator it = handlers_.find(fd);
    if (it == handlers_.end()) {
        return setError("fd is not registered");
    }

    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.fd = fd;
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0) {
        return setError(systemError("epoll_ctl modify failed", errno));
    }

    it->second = std::move(handler);
    clearError();
    return true;
#else
    (void)fd;
    (void)events;
    (void)handler;
    return setError("Reactor requires Linux epoll support");
#endif
}

bool Reactor::remove(int fd) {
    if (fd < 0) {
        return setError("remove requires a valid fd");
    }

#if defined(__linux__)
    std::lock_guard<std::mutex> lock(mutex_);
    if (epollFd_ < 0) {
        return setError("Reactor is not initialized");
    }
    if (handlers_.find(fd) == handlers_.end()) {
        return setError("fd is not registered");
    }
    if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        return setError(systemError("epoll_ctl remove failed", errno));
    }

    handlers_.erase(fd);
    clearError();
    return true;
#else
    (void)fd;
    return setError("Reactor requires Linux epoll support");
#endif
}

int Reactor::pollOnce(int timeoutMs) {
#if defined(__linux__)
    int epollFd = -1;
    int wakeFd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        epollFd = epollFd_;
        wakeFd = wakeFd_;
    }
    if (epollFd < 0) {
        setError("Reactor is not initialized");
        return -1;
    }

    std::vector<epoll_event> events(64);
    int eventCount = 0;
    do {
        eventCount = epoll_wait(epollFd,
                                events.data(),
                                static_cast<int>(events.size()),
                                timeoutMs);
    } while (eventCount < 0 && errno == EINTR);

    if (eventCount < 0) {
        setError(systemError("epoll_wait failed", errno));
        return -1;
    }

    int dispatched = 0;
    for (int index = 0; index < eventCount; ++index) {
        const int fd = events[static_cast<std::size_t>(index)].data.fd;
        const std::uint32_t eventMask =
            events[static_cast<std::size_t>(index)].events;
        if (fd == wakeFd) {
            drainWake();
            continue;
        }

        EventHandler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::map<int, EventHandler>::const_iterator it =
                handlers_.find(fd);
            if (it != handlers_.end()) {
                handler = it->second;
            }
        }
        if (!handler) {
            continue;
        }

        try {
            handler(fd, eventMask);
        } catch (const std::exception& exception) {
            setError(std::string("event handler failed: ") + exception.what());
        } catch (...) {
            setError("event handler failed with a non-standard exception");
        }
        ++dispatched;
    }
    return dispatched;
#else
    (void)timeoutMs;
    setError("Reactor requires Linux epoll support");
    return -1;
#endif
}

bool Reactor::run() {
    if (!initialized()) {
        return setError("Reactor is not initialized");
    }

    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return setError("Reactor is already running");
    }

    stopRequested_.store(false);
    while (!stopRequested_.load()) {
        if (pollOnce(-1) < 0) {
            running_.store(false);
            return false;
        }
    }

    running_.store(false);
    clearError();
    return true;
}

void Reactor::stop() {
    stopRequested_.store(true);
    wake();
}

bool Reactor::initialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return epollFd_ >= 0;
}

bool Reactor::running() const {
    return running_.load();
}

std::size_t Reactor::registeredCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handlers_.size();
}

std::string Reactor::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool Reactor::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void Reactor::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

void Reactor::wake() {
#if defined(__linux__)
    int wakeFd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wakeFd = wakeFd_;
    }
    if (wakeFd < 0) {
        return;
    }

    const std::uint64_t signal = 1;
    const ssize_t result = write(wakeFd, &signal, sizeof(signal));
    // eventfd is level-triggered and one pending value is enough to wake the
    // loop. EAGAIN only means another stop request already signaled it.
    if (result < 0 && errno != EAGAIN) {
        setError(systemError("eventfd wake failed", errno));
    }
#endif
}

void Reactor::drainWake() {
#if defined(__linux__)
    int wakeFd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wakeFd = wakeFd_;
    }
    if (wakeFd < 0) {
        return;
    }

    std::uint64_t signal = 0;
    while (read(wakeFd, &signal, sizeof(signal)) ==
           static_cast<ssize_t>(sizeof(signal))) {
    }
#endif
}

}  // namespace shms
