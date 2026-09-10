#include "UserProtocol.hpp"

#include <algorithm>
#include <limits>

namespace {

void writeUint32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24) & 0xffU));
    output->push_back(static_cast<char>((value >> 16) & 0xffU));
    output->push_back(static_cast<char>((value >> 8) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

void writeUint64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

std::uint32_t readUint32(const std::string& body, std::size_t offset) {
    return (static_cast<std::uint32_t>(
                static_cast<unsigned char>(body[offset]))
            << 24) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(body[offset + 1U]))
            << 16) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(body[offset + 2U]))
            << 8) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(body[offset + 3U]));
}

std::uint64_t readUint64(const std::string& body, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i) {
        value = (value << 8U) |
                static_cast<std::uint64_t>(
                    static_cast<unsigned char>(body[offset + i]));
    }
    return value;
}

bool setError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool validField(const std::string& value,
                std::size_t maximum,
                const char* field,
                std::string* error,
                bool allowEmpty) {
    if ((!allowEmpty && value.empty()) || value.size() > maximum ||
        value.size() > static_cast<std::size_t>(
                            std::numeric_limits<std::uint32_t>::max())) {
        return setError(error, std::string(field) + " exceeds its limit");
    }
    return true;
}

bool appendField(const std::string& value,
                 std::size_t maximum,
                 const char* field,
                 std::string* body,
                 std::string* error,
                 bool allowEmpty) {
    if (body == nullptr) {
        return setError(error, "body output cannot be null");
    }
    if (!validField(value, maximum, field, error, allowEmpty)) {
        return false;
    }
    writeUint32(static_cast<std::uint32_t>(value.size()), body);
    body->append(value);
    return true;
}

bool readField(const std::string& body,
               std::size_t* offset,
               std::size_t maximum,
               const char* field,
               std::string* value,
               std::string* error,
               bool allowEmpty) {
    if (offset == nullptr || value == nullptr ||
        body.size() - *offset < 4U) {
        return setError(error, std::string(field) + " length is missing");
    }
    const std::uint32_t length = readUint32(body, *offset);
    *offset += 4U;
    if (length > maximum || (!allowEmpty && length == 0U) ||
        static_cast<std::size_t>(length) > body.size() - *offset) {
        return setError(error, std::string(field) + " is invalid");
    }
    *value = body.substr(*offset, length);
    *offset += length;
    return true;
}

}  // 匿名命名空间

namespace shms {

bool UserProtocolCodec::encodeCredentials(
    const UserCredentials& credentials,
    std::string* body,
    std::string* error) {
    if (body == nullptr) {
        return setError(error, "body output cannot be null");
    }
    body->clear();
    if (!appendField(credentials.username,
                     kMaxUsernameBytes,
                     "username",
                     body,
                     error,
                     false) ||
        !appendField(credentials.password,
                      kMaxPasswordBytes,
                      "password",
                      body,
                      error,
                      false)) {
        body->clear();
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool UserProtocolCodec::decodeCredentials(const std::string& body,
                                          UserCredentials* credentials,
                                          std::string* error) {
    if (credentials == nullptr) {
        return setError(error, "credentials output cannot be null");
    }
    credentials->username.clear();
    credentials->password.clear();
    std::size_t offset = 0;
    if (!readField(body,
                   &offset,
                   kMaxUsernameBytes,
                   "username",
                   &credentials->username,
                   error,
                   false) ||
        !readField(body,
                   &offset,
                   kMaxPasswordBytes,
                   "password",
                   &credentials->password,
                   error,
                   false)) {
        credentials->username.clear();
        credentials->password.clear();
        return false;
    }
    if (offset != body.size()) {
        credentials->username.clear();
        credentials->password.clear();
        return setError(error, "credentials body contains trailing data");
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool UserProtocolCodec::encodeResponse(const UserResponse& response,
                                       std::string* body,
                                       std::string* error) {
    if (body == nullptr) {
        return setError(error, "body output cannot be null");
    }
    body->clear();
    if (!validField(response.message,
                    kMaxMessageBytes,
                    "message",
                    error,
                    true)) {
        return false;
    }
    writeUint32(static_cast<std::uint32_t>(response.code), body);
    writeUint64(response.userId, body);
    writeUint32(static_cast<std::uint32_t>(response.message.size()), body);
    body->append(response.message);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool UserProtocolCodec::decodeResponse(const std::string& body,
                                       UserResponse* response,
                                       std::string* error) {
    if (response == nullptr) {
        return setError(error, "response output cannot be null");
    }
    *response = UserResponse();
    if (body.size() < 16U) {
        return setError(error, "response body is truncated");
    }
    const std::uint32_t code = readUint32(body, 0);
    if (code > static_cast<std::uint32_t>(UserResponseCode::ProtocolError)) {
        return setError(error, "response code is invalid");
    }
    response->code = static_cast<UserResponseCode>(code);
    response->userId = readUint64(body, 4);
    const std::uint32_t messageLength = readUint32(body, 12);
    if (messageLength > kMaxMessageBytes ||
        static_cast<std::size_t>(messageLength) > body.size() - 16U) {
        *response = UserResponse();
        return setError(error, "response message is invalid");
    }
    if (static_cast<std::size_t>(messageLength) != body.size() - 16U) {
        *response = UserResponse();
        return setError(error, "response body contains trailing data");
    }
    response->message = body.substr(16U, messageLength);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

}  // shms 命名空间
