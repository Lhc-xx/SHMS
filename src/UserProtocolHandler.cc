#include "UserProtocolHandler.hpp"

#include "UserProtocol.hpp"

namespace {

shms::UserResponseCode toResponseCode(shms::UserResultCode code) {
    switch (code) {
        case shms::UserResultCode::Success:
            return shms::UserResponseCode::Success;
        case shms::UserResultCode::InvalidArgument:
            return shms::UserResponseCode::InvalidArgument;
        case shms::UserResultCode::UserAlreadyExists:
            return shms::UserResponseCode::UserAlreadyExists;
        case shms::UserResultCode::UserNotFound:
            return shms::UserResponseCode::UserNotFound;
        case shms::UserResultCode::InvalidPassword:
            return shms::UserResponseCode::InvalidPassword;
        case shms::UserResultCode::StorageError:
            return shms::UserResponseCode::StorageError;
        case shms::UserResultCode::PasswordError:
            return shms::UserResponseCode::PasswordError;
    }
    return shms::UserResponseCode::ProtocolError;
}

}  // 匿名命名空间

namespace shms {

UserProtocolHandler::UserProtocolHandler(UserService& service)
    : service_(service) {}

bool UserProtocolHandler::handle(const ProtocolMessage& request,
                                 ProtocolMessage* response) {
    return handle(request, response, nullptr);
}

bool UserProtocolHandler::handle(const ProtocolMessage& request,
                                 ProtocolMessage* response,
                                 std::string* authenticatedUsername) {
    if (response == nullptr) {
        return setError("response output cannot be null");
    }
    if (authenticatedUsername != nullptr) {
        authenticatedUsername->clear();
    }
    response->type = 0;
    response->body.clear();

    const std::uint32_t registerRequest =
        static_cast<std::uint32_t>(UserMessageType::RegisterRequest);
    const std::uint32_t loginRequest =
        static_cast<std::uint32_t>(UserMessageType::LoginRequest);
    std::uint32_t responseType = 0;
    if (request.type == registerRequest) {
        responseType = static_cast<std::uint32_t>(
            UserMessageType::RegisterResponse);
    } else if (request.type == loginRequest) {
        responseType = static_cast<std::uint32_t>(UserMessageType::LoginResponse);
    } else {
        return setError("unsupported user request type");
    }

    UserCredentials credentials;
    std::string decodeError;
    if (!UserProtocolCodec::decodeCredentials(request.body,
                                              &credentials,
                                              &decodeError)) {
        return setError(decodeError);
    }

    UserServiceResult result =
        request.type == registerRequest
            ? service_.registerUser(credentials.username, credentials.password)
            : service_.login(credentials.username, credentials.password);
    UserResponse userResponse;
    userResponse.code = toResponseCode(result.code);
    userResponse.userId = result.succeeded() ? result.userId : 0;
    userResponse.message = result.message;
    std::string encodeError;
    if (!UserProtocolCodec::encodeResponse(userResponse,
                                           &response->body,
                                           &encodeError)) {
        return setError(encodeError);
    }
    response->type = responseType;
    if (authenticatedUsername != nullptr &&
        request.type == loginRequest && result.succeeded()) {
        *authenticatedUsername = credentials.username;
    }
    clearError();
    return true;
}

std::string UserProtocolHandler::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool UserProtocolHandler::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void UserProtocolHandler::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
