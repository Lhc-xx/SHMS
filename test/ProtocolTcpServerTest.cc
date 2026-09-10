#include "ProtocolTcpServer.hpp"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
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

bool receiveFrame(int socketFd,
                  std::string* frame,
                  std::size_t expectedSize) {
    frame->clear();
    while (frame->size() < expectedSize) {
        char buffer[256];
        const ssize_t received = ::recv(socketFd, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return false;
        }
        frame->append(buffer, static_cast<std::size_t>(received));
    }
    return frame->size() == expectedSize;
}
#endif

}  // 匿名命名空间

int main() {
#if !defined(__linux__)
    std::cout << "Protocol TCP server tests skipped: Linux sockets are required"
              << std::endl;
    return EXIT_SUCCESS;
#else
    shms::Reactor reactor;
    expect(reactor.initialize(), "initialize Reactor");

    shms::ProtocolTcpServer server(reactor, "127.0.0.1", 0);
    server.setSessionConfigurer([](shms::ProtocolSession& session) {
        return session.registerHandler(
            1234,
            [&session](const shms::ProtocolMessage& message) {
                if (!session.sendMessage(1235, message.body)) {
                    throw std::runtime_error(session.lastError());
                }
            });
    });
    expect(server.start(), "start protocol TCP server");

    std::atomic<bool> loopFinished(false);
    std::thread loop([&reactor, &loopFinished]() {
        reactor.run();
        loopFinished.store(true);
    });
    expect(waitFor([&reactor]() { return reactor.running(); }),
           "enter protocol Reactor loop");

    const int client = ::socket(AF_INET, SOCK_STREAM, 0);
    expect(client >= 0, "create TCP client");
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(server.port());
    expect(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1,
           "parse protocol server address");
    expect(::connect(client,
                     reinterpret_cast<const sockaddr*>(&address),
                     sizeof(address)) == 0,
           "connect protocol TCP client");
    expect(waitFor([&server]() { return server.sessionCount() == 1U; }),
           "create a protocol session for the connection");

    std::string request;
    expect(shms::ProtocolCodec::encode(1234, "hello", &request),
           "encode protocol request");
    expect(::send(client, request.data(), 3, 0) == 3,
           "send a fragmented protocol header");
    expect(::send(client, request.data() + 3, request.size() - 3, 0) > 0,
           "send the rest of the protocol frame");
    std::string expectedResponse;
    expect(shms::ProtocolCodec::encode(1235, "hello", &expectedResponse),
           "encode expected protocol response");
    std::string response;
    expect(receiveFrame(client, &response, expectedResponse.size()),
           "receive a protocol response");
    expect(response == expectedResponse,
           "preserve protocol response type and body");

    std::string heartbeat;
    expect(shms::ProtocolCodec::encode(
               static_cast<std::uint32_t>(
                   shms::SystemMessageType::HeartbeatRequest),
               std::string(),
               &heartbeat),
           "encode heartbeat request");
    expect(::send(client, heartbeat.data(), heartbeat.size(), 0) > 0,
           "send heartbeat request");
    std::string heartbeatResponse;
    expect(shms::ProtocolCodec::encode(
               static_cast<std::uint32_t>(
                   shms::SystemMessageType::HeartbeatResponse),
               std::string(),
               &heartbeatResponse),
           "encode expected heartbeat response");
    expect(receiveFrame(client, &response, heartbeatResponse.size()),
           "receive heartbeat response");
    expect(response == heartbeatResponse,
           "return heartbeat response over TCP");

    ::close(client);
    expect(waitFor([&server]() { return server.sessionCount() == 0U; }),
           "release the protocol session after disconnect");

    server.stop();
    reactor.stop();
    loop.join();
    expect(loopFinished.load(), "finish protocol Reactor loop");
    reactor.shutdown();

    std::cout << "All ProtocolTcpServer tests passed" << std::endl;
    return EXIT_SUCCESS;
#endif
}
