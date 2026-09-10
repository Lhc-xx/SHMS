#include "ThreadPool.hpp"

#include <exception>

namespace shms {

ThreadPool::ThreadPool(std::size_t threadCount, std::size_t maxQueueSize)
    : threadCount_(threadCount),
      maxQueueSize_(maxQueueSize),
      running_(false) {}

ThreadPool::~ThreadPool() {
    // 所有者不能早于工作线程销毁。stop() 会清空排队任务并等待所有工作
    // 线程退出，然后任务队列和同步对象才会离开作用域。
    stop();
}

bool ThreadPool::start() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (running_) {
        return true;
    }
    if (threadCount_ == 0 || maxQueueSize_ == 0) {
        return false;
    }

    running_ = true;
    try {
        workers_.reserve(threadCount_);
        for (std::size_t index = 0; index < threadCount_; ++index) {
            workers_.push_back(std::thread(&ThreadPool::workerLoop, this));
        }
    } catch (const std::exception&) {
        // 如果工作线程创建失败，先唤醒已经启动的线程，并在报告错误前将
        // 线程池恢复到干净的停止状态。
        running_ = false;
        notEmpty_.notify_all();
        notFull_.notify_all();
        lock.unlock();
        for (std::vector<std::thread>::iterator it = workers_.begin();
             it != workers_.end(); ++it) {
            if (it->joinable()) {
                it->join();
            }
        }
        workers_.clear();
        return false;
    }
    return true;
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && workers_.empty()) {
            return;
        }
        // 工作线程会持续运行直到队列清空，然后发现 running_ == false 并
        // 退出，从而优雅地处理已经接收的任务。
        running_ = false;
    }

    notEmpty_.notify_all();
    notFull_.notify_all();

    for (std::vector<std::thread>::iterator it = workers_.begin();
         it != workers_.end(); ++it) {
        if (it->joinable()) {
            it->join();
        }
    }

    workers_.clear();
}

bool ThreadPool::submit(Task task) {
    if (!task) {
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this]() {
        return !running_ || tasks_.size() < maxQueueSize_;
    });
    if (!running_) {
        return false;
    }

    tasks_.push_back(std::move(task));
    lock.unlock();
    notEmpty_.notify_one();
    return true;
}

std::size_t ThreadPool::threadCount() const {
    return threadCount_;
}

std::size_t ThreadPool::pendingTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

bool ThreadPool::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void ThreadPool::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            notEmpty_.wait(lock, [this]() {
                return !running_ || !tasks_.empty();
            });

            // 关闭期间处理已经接收的任务。只有 stop() 关闭任务提交且队列
            // 中不再有任务后才退出。
            if (!running_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
            notFull_.notify_one();
        }

        // 有问题的业务任务不能终止工作线程，也不能阻止其他客户端请求处理。
        try {
            task();
        } catch (const std::exception&) {
            // 后续日志/分发层将记录任务失败；线程池的职责是保证工作线程
            // 继续存活。
        } catch (...) {
            // 同时隔离非标准异常，避免其逸出工作线程循环。
        }
    }
}

}  // shms 命名空间
