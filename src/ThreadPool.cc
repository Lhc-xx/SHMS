#include "ThreadPool.hpp"

#include <exception>

namespace shms {

ThreadPool::ThreadPool(std::size_t threadCount, std::size_t maxQueueSize)
    : threadCount_(threadCount),
      maxQueueSize_(maxQueueSize),
      running_(false) {}

ThreadPool::~ThreadPool() {
    // The owner must not outlive worker threads. stop() drains queued work and
    // joins every worker before the task queue and synchronization objects go
    // out of scope.
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
        // If a thread cannot be created, wake any workers that did start and
        // return the pool to a clean stopped state before reporting failure.
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
        // Workers continue until the queue is empty, then observe running_ ==
        // false and exit. This provides graceful draining of accepted tasks.
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

            // Drain accepted tasks during shutdown. Exit only after stop() has
            // closed submissions and no queued task remains.
            if (!running_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
            notFull_.notify_one();
        }

        // A bad business task must not terminate its worker or prevent other
        // client requests from being processed.
        try {
            task();
        } catch (const std::exception&) {
            // The future logging/dispatcher layer will record task failures;
            // the pool's responsibility is to keep the worker alive.
        } catch (...) {
            // Also isolate non-standard exceptions from the worker loop.
        }
    }
}

}  // namespace shms
