#include "TcpServer.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
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

bool waitFor(const std::function<bool()>& predicate) {
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}
#endif

}  // 匿名命名空间

int main() {
#if !defined(__linux__)
    std::cout << "TCP server tests skipped: Linux sockets are required"
              << std::endl;
    return EXIT_SUCCESS;
#else
    shms::Reactor reactor;
    expect(reactor.initialize(), "initialize Reactor");

    shms::TcpServer server(reactor, "127.0.0.1", 0);
    std::atomic<int> connected(0);
    std::atomic<int> messages(0);
    std::atomic<int> closed(0);
    server.setConnectionHandler([&connected](shms::TcpConnection&) {
        ++connected;
    });
    server.setMessageHandler([&messages](shms::TcpConnection& connection,
                                         const std::string& data) {
        ++messages;
        if (!connection.send(data)) {
            std::exit(EXIT_FAILURE);
        }
    });
    server.setCloseHandler([&closed](shms::TcpConnection&) { ++closed; });
    expect(server.start(), "bind and start TCP server");
    expect(server.running(), "report running TCP server");
    expect(server.port() != 0, "report kernel-assigned listener port");

    std::atomic<bool> loopFinished(false);
    std::atomic<bool> loopSucceeded(false);
    std::thread loop([&reactor, &loopFinished, &loopSucceeded]() {
        loopSucceeded.store(reactor.run());
        loopFinished.store(true);
    });
    expect(waitFor([&reactor]() { return reactor.running(); }),
           "enter Reactor loop");

    const int client = ::socket(AF_INET, SOCK_STREAM, 0);
    expect(client >= 0, "create TCP client");
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(server.port());
    expect(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1,
           "parse test server address");
    expect(::connect(client,
                     reinterpret_cast<const sockaddr*>(&address),
                     sizeof(address)) == 0,
           "connect TCP client");
    expect(waitFor([&connected]() { return connected.load() == 1; }),
           "accept client connection");

    const char payload[] = "hello";
    expect(::send(client, payload, sizeof(payload) - 1, 0) > 0,
           "send client payload");
    char response[sizeof(payload)] = {0};
    expect(::recv(client, response, sizeof(response) - 1, 0) ==
               static_cast<ssize_t>(sizeof(payload) - 1),
           "receive echoed payload");
    expect(std::string(response, sizeof(payload) - 1) == "hello",
           "preserve payload bytes");
    expect(waitFor([&messages]() { return messages.load() == 1; }),
           "complete message dispatch");

    ::close(client);
    expect(waitFor([&closed]() { return closed.load() == 1; }),
           "close client connection");
    expect(server.connectionCount() == 0,
           "remove closed client from server registry");

    server.stop();
    reactor.stop();
    loop.join();
    expect(loopFinished.load(), "finish Reactor loop");
    expect(loopSucceeded.load(), "stop Reactor cleanly");
    expect(!server.running(), "report stopped TCP server");
    reactor.shutdown();

    std::cout << "All TCP server tests passed" << std::endl;
    return EXIT_SUCCESS;
#endif
}
