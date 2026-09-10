#include "ProtocolSession.hpp"

#include <stdexcept>
#include <utility>

namespace {

bool validAuthenticatedUser(const std::string& username) {
    if (username.empty() || username.size() > 20U ||
        username.find('\0') != std::string::npos) {
        return false;
    }
    for (std::string::const_iterator it = username.begin();
         it != username.end();
         ++it) {
        if (*it == '\r' || *it == '\n' || *it == '\t' || *it == ' ') {
            return false;
        }
    }
    return true;
}

}  // 匿名命名空间

namespace shms {

ProtocolSession::ProtocolSession(SendHandler sender, std::size_t maxBodySize)
    : sender_(std::move(sender)), parser_(maxBodySize) {
    dispatcher_.registerHandler(
        static_cast<std::uint32_t>(SystemMessageType::HeartbeatRequest),
        [this](const ProtocolMessage&) {
            if (!sendHeartbeatResponse()) {
                throw std::runtime_error(lastError());
            }
        });
}

bool ProtocolSession::registerHandler(std::uint32_t type,
                                      MessageDispatcher::Handler handler) {
    if (!dispatcher_.registerHandler(type, std::move(handler))) {
        return setError(dispatcher_.lastError());
    }
    clearError();
    return true;
}

bool ProtocolSession::unregisterHandler(std::uint32_t type) {
    if (!dispatcher_.unregisterHandler(type)) {
        return setError(dispatcher_.lastError());
    }
    clearError();
    return true;
}

bool ProtocolSession::onData(const void* data, std::size_t size) {
    std::vector<ProtocolMessage> messages;
    if (!parser_.append(data, size, &messages)) {
        return setError(parser_.lastError());
    }
    for (std::vector<ProtocolMessage>::const_iterator it = messages.begin();
         it != messages.end();
         ++it) {
        if (!dispatcher_.dispatch(*it)) {
            return setError(dispatcher_.lastError());
        }
    }
    clearError();
    return true;
}

bool ProtocolSession::onData(const std::string& data) {
    return onData(data.data(), data.size());
}

bool ProtocolSession::sendMessage(std::uint32_t type,
                                  const std::string& body,
                                  std::string* error) {
    std::string frame;
    if (!ProtocolCodec::encode(type, body, &frame, error)) {
        return setError(error == nullptr ? "protocol frame encoding failed"
                                         : *error);
    }
    if (!sender_) {
        const std::string message = "protocol session has no sender";
        if (error != nullptr) {
            *error = message;
        }
        return setError(message);
    }
    if (!sender_(frame)) {
        const std::string message = "protocol response send failed";
        if (error != nullptr) {
            *error = message;
        }
        return setError(message);
    }
    if (error != nullptr) {
        error->clear();
    }
    clearError();
    return true;
}

bool ProtocolSession::setAuthenticatedUser(const std::string& username,
                                            std::string* error) {
    if (!validAuthenticatedUser(username)) {
        const std::string message = "authenticated username is invalid";
        if (error != nullptr) {
            *error = message;
        }
        return setError(message);
    }
    {
        std::lock_guard<std::mutex> lock(contextMutex_);
        authenticatedUser_ = username;
    }
    if (error != nullptr) {
        error->clear();
    }
    clearError();
    return true;
}

void ProtocolSession::clearAuthenticatedUser() {
    std::lock_guard<std::mutex> lock(contextMutex_);
    authenticatedUser_.clear();
}

bool ProtocolSession::authenticated() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    return !authenticatedUser_.empty();
}

std::string ProtocolSession::authenticatedUser() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    return authenticatedUser_;
}

bool ProtocolSession::failed() const {
    return parser_.failed();
}

std::size_t ProtocolSession::bufferedBytes() const {
    return parser_.bufferedBytes();
}

std::string ProtocolSession::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool ProtocolSession::sendHeartbeatResponse() {
    return sendMessage(
        static_cast<std::uint32_t>(SystemMessageType::HeartbeatResponse),
        std::string());
}

bool ProtocolSession::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void ProtocolSession::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
