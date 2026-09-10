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

// 面向服务端业务任务的有界工作线程池。队列上限可配置，因此 server.conf
// 中的 task_num 可以控制内存使用量。
class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(std::size_t threadCount, std::size_t maxQueueSize);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool();

    // 启动所有工作线程。重复启动已运行的线程池不会产生额外影响。
    bool start();

    // 停止接收任务，完成已经排队的任务并等待工作线程退出。重复调用
    // stop 是安全的。
    void stop();

    // 提交任务。队列满时会施加反压，直到有空位或调用 stop()；停止接收
    // 任务后返回 false。
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

}  // shms 命名空间

#endif  // SMART_HOME_THREAD_POOL_HPP 头文件保护宏
