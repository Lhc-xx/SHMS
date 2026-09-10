#ifndef SMART_HOME_THREAD_POOL_HPP
#define SMART_HOME_THREAD_POOL_HPP

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace shms {

// A bounded worker pool for server-side business tasks. The queue limit is
// deliberately configurable so task_num from server.conf controls memory use.
class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(std::size_t threadCount, std::size_t maxQueueSize);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool();

    // Start all workers. Starting an already running pool is idempotent.
    bool start();

    // Stop accepting tasks, finish tasks already queued, and join workers.
    // Calling stop more than once is safe.
    void stop();

    // Submit a task. A full queue applies back-pressure until space is
    // available or stop() is called. Returns false when submission is closed.
    bool submit(Task task);

    std::size_t threadCount() const;
    std::size_t pendingTaskCount() const;
    bool running() const;

private:
    void workerLoop();

    const std::size_t threadCount_;
    const std::size_t maxQueueSize_;

    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::vector<std::thread> workers_;
    std::deque<Task> tasks_;
    bool running_;
};

}  // namespace shms

#endif  // SMART_HOME_THREAD_POOL_HPP
