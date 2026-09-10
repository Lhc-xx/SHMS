#include "MessageDispatcher.hpp"
#include "Protocol.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    std::string firstFrame;
    std::string secondFrame;
    std::string error;
    const std::string binaryBody("camera-001\0binary", 17);
    expect(shms::ProtocolCodec::encode(
               static_cast<std::uint32_t>(shms::ProtocolModule::User),
               std::string("login"),
               &firstFrame,
               &error),
           "encode a user frame");
    expect(shms::ProtocolCodec::encode(
               static_cast<std::uint32_t>(shms::ProtocolModule::Camera),
               binaryBody,
               &secondFrame,
               &error),
           "encode a binary camera frame");
    expect(firstFrame.size() == shms::ProtocolCodec::kHeaderSize + 5,
           "include the protocol header");

    shms::ProtocolParser parser(64);
    std::vector<shms::ProtocolMessage> messages;
    expect(parser.append(firstFrame.data(), 3, &messages),
           "accept a fragmented header");
    expect(messages.empty(), "wait for an incomplete frame");
    expect(parser.append(firstFrame.data() + 3, firstFrame.size() - 3,
                         &messages),
           "complete the fragmented frame");
    expect(messages.size() == 1, "extract one complete frame");
    expect(messages[0].type ==
               static_cast<std::uint32_t>(shms::ProtocolModule::User),
           "decode message type");
    expect(messages[0].body == "login", "decode message body");

    messages.clear();
    const std::string combined = firstFrame + secondFrame;
    expect(parser.append(combined, &messages),
           "accept coalesced TCP frames");
    expect(messages.size() == 2, "extract multiple frames from one read");
    expect(messages[1].body == binaryBody && messages[1].body[10] == '\0',
           "preserve binary body bytes");

    shms::ProtocolParser limitedParser(4);
    std::string oversizedFrame;
    expect(shms::ProtocolCodec::encode(1234, "12345", &oversizedFrame),
           "encode an oversized test frame");
    expect(!limitedParser.append(oversizedFrame, &messages),
           "reject an oversized protocol body");
    expect(limitedParser.failed(), "latch parser failure state");
    expect(limitedParser.lastError().find("maximum") != std::string::npos,
           "explain oversized body");
    limitedParser.reset();
    messages.clear();
    std::string smallFrame;
    expect(shms::ProtocolCodec::encode(1234, "ok", &smallFrame),
           "encode a frame within the reset parser limit");
    expect(limitedParser.append(smallFrame, &messages),
           "reset parser after malformed input");

    shms::MessageDispatcher dispatcher;
    shms::ProtocolMessage loginMessage;
    loginMessage.type = static_cast<std::uint32_t>(shms::ProtocolModule::User);
    loginMessage.body = "login";
    shms::ProtocolMessage cameraMessage;
    cameraMessage.type =
        static_cast<std::uint32_t>(shms::ProtocolModule::Camera);
    cameraMessage.body = binaryBody;
    int dispatchCount = 0;
    expect(dispatcher.registerHandler(1000,
                                      [&dispatchCount](
                                          const shms::ProtocolMessage& message) {
                                          if (message.body == "login") {
                                              ++dispatchCount;
                                          }
                                      }),
           "register message handler");
    expect(!dispatcher.registerHandler(1000,
                                       [](const shms::ProtocolMessage&) {}),
           "reject duplicate message handler");
    expect(dispatcher.dispatch(loginMessage), "dispatch decoded message");
    expect(dispatchCount == 1, "invoke registered message handler");
    expect(!dispatcher.dispatch(cameraMessage),
           "report an unknown message type");
    expect(dispatcher.unregisterHandler(1000), "remove message handler");
    expect(dispatcher.handlerCount() == 0, "report empty dispatcher");

    std::cout << "All Protocol tests passed" << std::endl;
    return EXIT_SUCCESS;
}
