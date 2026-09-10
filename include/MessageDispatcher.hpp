#ifndef SMART_HOME_MESSAGE_DISPATCHER_HPP
#define SMART_HOME_MESSAGE_DISPATCHER_HPP

#include "Protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace shms {

// Routes decoded protocol messages to business-layer handlers. It does not
// parse or own TCP connections, keeping transport and business concerns apart.
class MessageDispatcher {
public:
    using Handler = std::function<void(const ProtocolMessage&)>;

    MessageDispatcher() = default;
    MessageDispatcher(const MessageDispatcher&) = delete;
    MessageDispatcher& operator=(const MessageDispatcher&) = delete;

    // Register exactly one handler per message type. A duplicate registration
    // is rejected so a later module cannot silently replace business logic.
    bool registerHandler(std::uint32_t type, Handler handler);
    bool unregisterHandler(std::uint32_t type);

    // Dispatch one decoded message. Unknown types are reported to the caller
    // so protocol errors are not silently discarded.
    bool dispatch(const ProtocolMessage& message);

    std::size_t handlerCount() const;
    std::string lastError() const;

private:
    bool setError(const std::string& message);
    void clearError();

    mutable std::mutex mutex_;
    std::map<std::uint32_t, Handler> handlers_;
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // namespace shms

#endif  // SMART_HOME_MESSAGE_DISPATCHER_HPP
