#include "ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    shms::ThreadPool pool(3, 32);
    expect(pool.threadCount() == 3, "report configured thread count");
    expect(pool.pendingTaskCount() == 0, "start with an empty queue");
    expect(pool.start(), "start worker threads");
    expect(pool.start(), "starting an active pool is idempotent");
    expect(pool.running(), "report running state");

    const int taskCount = 60;
    std::atomic<int> executed(0);
    std::mutex mutex;
    std::condition_variable completed;

    for (int index = 0; index < taskCount; ++index) {
        expect(pool.submit([&executed, &mutex, &completed, index]() {
            if (index == 7) {
                throw std::runtime_error("task failure must stay inside worker");
            }
            ++executed;
            completed.notify_one();
        }), "submit task while pool is running");
    }

    // One task intentionally throws, so 59 successful tasks are expected.
    std::unique_lock<std::mutex> lock(mutex);
    expect(completed.wait_for(lock, std::chrono::seconds(5), [&executed]() {
               return executed.load() == taskCount - 1;
           }),
           "finish all non-throwing tasks");
    lock.unlock();

    pool.stop();
    expect(!pool.running(), "report stopped state");
    expect(pool.pendingTaskCount() == 0, "drain the task queue on stop");
    expect(!pool.submit([]() {}), "reject tasks after stop");
    pool.stop();

    shms::ThreadPool invalidPool(0, 1);
    expect(!invalidPool.start(), "reject zero worker configuration");
    shms::ThreadPool invalidQueue(1, 0);
    expect(!invalidQueue.start(), "reject zero queue configuration");

    std::cout << "All ThreadPool tests passed" << std::endl;
    return EXIT_SUCCESS;
}
