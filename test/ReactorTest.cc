#include "Reactor.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

#if defined(__linux__)
#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#endif

namespace {

#if defined(__linux__)
void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
#endif

}  // namespace

int main() {
#if !defined(__linux__)
    // The production server target is Ubuntu 22.04; keep non-Linux builds
    // informative rather than pretending that epoll is available there.
    std::cout << "Reactor tests skipped: Linux epoll is required" << std::endl;
    return EXIT_SUCCESS;
#else
    shms::Reactor reactor;
    expect(!reactor.add(-1, EPOLLIN, [](int, std::uint32_t) {}),
           "reject invalid descriptor before initialization");
    expect(reactor.initialize(), "initialize epoll Reactor");
    expect(reactor.initialized(), "report initialized state");
    expect(!reactor.add(0, 0, [](int, std::uint32_t) {}),
           "reject an empty event mask");

    int pipeFds[2] = {-1, -1};
    expect(pipe(pipeFds) == 0, "create notification pipe");

    std::atomic<int> callbacks(0);
    std::atomic<std::uint32_t> observedEvents(0);
    expect(reactor.add(pipeFds[0], EPOLLIN,
                       [&callbacks, &observedEvents](
                           int fd, std::uint32_t events) {
                           char buffer[32];
                           while (read(fd, buffer, sizeof(buffer)) > 0) {
                           }
                           observedEvents.store(events);
                           ++callbacks;
                       }),
           "register a readable descriptor");
    expect(reactor.registeredCount() == 1,
           "report registered descriptor count");
    expect(!reactor.add(pipeFds[0], EPOLLIN,
                        [](int, std::uint32_t) {}),
           "reject duplicate descriptor registration");
    expect(reactor.modify(pipeFds[0], EPOLLIN | EPOLLET,
                          [&callbacks, &observedEvents](
                              int fd, std::uint32_t events) {
                              char buffer[32];
                              (void)read(fd, buffer, sizeof(buffer));
                              observedEvents.store(events);
                              ++callbacks;
                          }),
           "modify a registered descriptor");

    const char payload[] = "ping";
    expect(write(pipeFds[1], payload, sizeof(payload)) > 0,
           "write an event to the pipe");
    expect(reactor.pollOnce(1000) == 1, "dispatch one readable event");
    expect(callbacks.load() == 1, "invoke the registered callback");
    expect((observedEvents.load() & EPOLLIN) != 0,
           "pass the readable event mask");

    expect(reactor.remove(pipeFds[0]), "remove a registered descriptor");
    expect(reactor.registeredCount() == 0,
           "report no descriptors after removal");
    expect(!reactor.remove(pipeFds[0]), "reject removing an unknown fd");
    close(pipeFds[0]);
    close(pipeFds[1]);
    reactor.shutdown();
    expect(!reactor.initialized(), "report shutdown state");

    // Verify that stop() wakes a thread blocked in epoll_wait instead of
    // requiring a client event or a polling timeout to end the service.
    shms::Reactor runningReactor;
    expect(runningReactor.initialize(), "initialize Reactor for run test");
    std::atomic<bool> runFinished(false);
    std::atomic<bool> runSucceeded(false);
    std::thread loop([&runningReactor, &runFinished, &runSucceeded]() {
        runSucceeded.store(runningReactor.run());
        runFinished.store(true);
    });

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!runningReactor.running() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    expect(runningReactor.running(), "enter the Reactor event loop");
    runningReactor.stop();
    loop.join();
    expect(runFinished.load(), "finish the Reactor event loop");
    expect(runSucceeded.load(), "stop the Reactor cleanly");
    runningReactor.shutdown();

    std::cout << "All Reactor tests passed" << std::endl;
    return EXIT_SUCCESS;
#endif
}
