#include "ProtocolSession.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

}  // 匿名命名空间

int main() {
    std::vector<std::string> sentFrames;
    shms::ProtocolSession session(
        [&sentFrames](const std::string& frame) {
            sentFrames.push_back(frame);
            return true;
        },
        64);

    int handled = 0;
    expect(session.registerHandler(
                1234,
                [&session, &handled](const shms::ProtocolMessage& message) {
                    ++handled;
                    if (!session.sendMessage(1235, message.body)) {
                        std::exit(EXIT_FAILURE);
                    }
                }),
           "register a session handler");

    std::string request;
    expect(shms::ProtocolCodec::encode(1234, "fragmented", &request),
           "encode a session request");
    expect(session.onData(request.data(), 3), "accept fragmented session data");
    expect(handled == 0 && session.bufferedBytes() == 3,
           "retain an incomplete session frame");
    expect(session.onData(request.data() + 3, request.size() - 3),
           "dispatch a completed session frame");
    expect(handled == 1 && sentFrames.size() == 1,
           "send a handler response");

    shms::ProtocolParser responseParser(64);
    std::vector<shms::ProtocolMessage> responses;
    expect(responseParser.append(sentFrames[0], &responses) &&
               responses.size() == 1 && responses[0].type == 1235 &&
               responses[0].body == "fragmented",
           "decode the handler response");

    std::string heartbeat;
    expect(shms::ProtocolCodec::encode(
               static_cast<std::uint32_t>(
                   shms::SystemMessageType::HeartbeatRequest),
               std::string(),
               &heartbeat),
           "encode a heartbeat request");
    expect(session.onData(heartbeat), "handle a heartbeat request");
    expect(sentFrames.size() == 2, "send a heartbeat response");
    responses.clear();
    responseParser.reset();
    expect(responseParser.append(sentFrames[1], &responses) &&
               responses.size() == 1 &&
               responses[0].type == static_cast<std::uint32_t>(
                                         shms::SystemMessageType::HeartbeatResponse) &&
               responses[0].body.empty(),
           "decode the heartbeat response");

    std::string unknownFrame;
    expect(shms::ProtocolCodec::encode(9999, "unknown", &unknownFrame),
           "encode an unknown request");
    expect(!session.onData(unknownFrame), "reject an unknown message");
    expect(session.lastError().find("handler") != std::string::npos,
           "explain the unknown message failure");
    shms::ProtocolSession malformedSession(
        [](const std::string&) { return true; }, 4);
    std::string oversized;
    expect(shms::ProtocolCodec::encode(1234, "12345", &oversized),
           "encode an oversized request");
    expect(!malformedSession.onData(oversized),
           "reject an oversized session frame");
    expect(malformedSession.failed(), "latch parser failure in session");

    std::cout << "All ProtocolSession tests passed" << std::endl;
    return EXIT_SUCCESS;
}
