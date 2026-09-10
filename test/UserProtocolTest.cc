#include "UserProtocol.hpp"
#include "UserProtocolHandler.hpp"

#include <cstdlib>
#include <iostream>
#include <map>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

class MemoryUserStore : public shms::UserStore {
public:
    MemoryUserStore() : nextId_(1) {}

    bool createUser(const std::string& name,
                    const std::string& setting,
                    const std::string& encrypt,
                    std::uint64_t* userId) override {
        if (users_.find(name) != users_.end()) {
            lastError_ = "duplicate user";
            return false;
        }
        shms::UserRecord record;
        record.id = nextId_++;
        record.name = name;
        record.setting = setting;
        record.encrypt = encrypt;
        users_[name] = record;
        if (userId != nullptr) {
            *userId = record.id;
        }
        lastError_.clear();
        return true;
    }

    bool findByName(const std::string& name,
                    shms::UserRecord* record,
                    bool* found) override {
        if (record == nullptr || found == nullptr) {
            lastError_ = "invalid lookup output";
            return false;
        }
        *record = shms::UserRecord();
        *found = false;
        std::map<std::string, shms::UserRecord>::const_iterator it =
            users_.find(name);
        if (it != users_.end()) {
            *record = it->second;
            *found = true;
        }
        lastError_.clear();
        return true;
    }

    std::string lastError() const override { return lastError_; }

private:
    std::uint64_t nextId_;
    std::map<std::string, shms::UserRecord> users_;
    std::string lastError_;
};

}  // 匿名命名空间

int main() {
    shms::UserCredentials credentials;
    credentials.username = "alice";
    credentials.password = "p@ss:word";
    std::string body;
    std::string error;
    expect(shms::UserProtocolCodec::encodeCredentials(credentials,
                                                       &body,
                                                       &error),
           "encode user credentials");
    shms::UserCredentials decoded;
    expect(shms::UserProtocolCodec::decodeCredentials(body,
                                                       &decoded,
                                                       &error) &&
               decoded.username == credentials.username &&
               decoded.password == credentials.password,
           "decode user credentials");

    std::string malformed = body;
    malformed.push_back('x');
    expect(!shms::UserProtocolCodec::decodeCredentials(malformed,
                                                       &decoded,
                                                       &error),
           "reject trailing credential bytes");
    expect(!shms::UserProtocolCodec::encodeCredentials(
               shms::UserCredentials{"", "password"}, &body, &error),
           "reject an empty protocol username");

    shms::UserResponse response;
    response.code = shms::UserResponseCode::Success;
    response.userId = 42;
    response.message = "login succeeded";
    expect(shms::UserProtocolCodec::encodeResponse(response, &body, &error),
           "encode user response");
    shms::UserResponse decodedResponse;
    expect(shms::UserProtocolCodec::decodeResponse(body,
                                                   &decodedResponse,
                                                   &error) &&
               decodedResponse.code == shms::UserResponseCode::Success &&
               decodedResponse.userId == 42U &&
               decodedResponse.message == response.message,
           "decode user response");

    MemoryUserStore store;
    shms::UserService service(store);
    shms::UserProtocolHandler handler(service);
    shms::ProtocolMessage request;
    request.type = static_cast<std::uint32_t>(
        shms::UserMessageType::RegisterRequest);
    request.body = body;
    expect(shms::UserProtocolCodec::encodeCredentials(credentials,
                                                       &request.body,
                                                       &error),
           "encode registration request");
    shms::ProtocolMessage responseMessage;
    expect(handler.handle(request, &responseMessage),
           "handle registration request");
    expect(responseMessage.type == static_cast<std::uint32_t>(
                                      shms::UserMessageType::RegisterResponse),
           "return registration response type");
    expect(shms::UserProtocolCodec::decodeResponse(responseMessage.body,
                                                   &decodedResponse,
                                                   &error) &&
               decodedResponse.code == shms::UserResponseCode::Success &&
               decodedResponse.userId == 1U,
           "return registration success");

    request.type = static_cast<std::uint32_t>(shms::UserMessageType::LoginRequest);
    expect(handler.handle(request, &responseMessage),
           "handle login request");
    expect(responseMessage.type == static_cast<std::uint32_t>(
                                      shms::UserMessageType::LoginResponse),
           "return login response type");
    expect(shms::UserProtocolCodec::decodeResponse(responseMessage.body,
                                                   &decodedResponse,
                                                   &error) &&
               decodedResponse.code == shms::UserResponseCode::Success &&
               decodedResponse.userId == 1U,
           "return login success");

    credentials.password = "wrong";
    expect(shms::UserProtocolCodec::encodeCredentials(credentials,
                                                       &request.body,
                                                       &error),
           "encode invalid login request");
    expect(handler.handle(request, &responseMessage),
           "return invalid login response");
    expect(shms::UserProtocolCodec::decodeResponse(responseMessage.body,
                                                   &decodedResponse,
                                                   &error) &&
               decodedResponse.code == shms::UserResponseCode::InvalidPassword,
           "return invalid password code");

    request.type = 9999;
    expect(!handler.handle(request, &responseMessage),
           "reject an unknown user request type");
    expect(handler.lastError().find("unsupported") != std::string::npos,
           "explain an unknown user request type");

    std::cout << "All UserProtocol tests passed" << std::endl;
    return EXIT_SUCCESS;
}
