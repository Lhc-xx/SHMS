#include "MessageDispatcher.hpp"

#include <exception>

namespace shms {

bool MessageDispatcher::registerHandler(std::uint32_t type, Handler handler) {
    if (type == 0 || !handler) {
        return setError("handler registration requires a type and callback");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (handlers_.find(type) != handlers_.end()) {
        return setError("message type already has a handler");
    }
    handlers_[type] = std::move(handler);
    clearError();
    return true;
}

bool MessageDispatcher::unregisterHandler(std::uint32_t type) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::uint32_t, Handler>::iterator it = handlers_.find(type);
    if (it == handlers_.end()) {
        return setError("message type has no registered handler");
    }
    handlers_.erase(it);
    clearError();
    return true;
}

bool MessageDispatcher::dispatch(const ProtocolMessage& message) {
    Handler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::uint32_t, Handler>::const_iterator it =
            handlers_.find(message.type);
        if (it == handlers_.end()) {
            return setError("no handler for message type");
        }
        handler = it->second;
    }

    try {
        handler(message);
    } catch (const std::exception& exception) {
        return setError(std::string("message handler failed: ") +
                        exception.what());
    } catch (...) {
        return setError("message handler failed with a non-standard exception");
    }
    clearError();
    return true;
}

std::size_t MessageDispatcher::handlerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handlers_.size();
}

std::string MessageDispatcher::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool MessageDispatcher::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void MessageDispatcher::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
