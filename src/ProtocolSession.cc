#include "ProtocolSession.hpp"

#include <stdexcept>
#include <utility>

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
