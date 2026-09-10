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

// 将解码后的协议消息路由到业务层处理器。此类不负责解析，也不拥有 TCP
// 连接，从而保持传输层与业务层职责分离。
class MessageDispatcher {
public:
    using Handler = std::function<void(const ProtocolMessage&)>;

    MessageDispatcher() = default;
    MessageDispatcher(const MessageDispatcher&) = delete;
    MessageDispatcher& operator=(const MessageDispatcher&) = delete;

    // 为每种消息类型注册唯一处理器。重复注册会被拒绝，避免后续模块静默
    // 替换已有业务逻辑。
    bool registerHandler(std::uint32_t type, Handler handler);
    bool unregisterHandler(std::uint32_t type);

    // 分发一条已解码消息。未知类型会报告给调用方，避免协议错误被静默丢弃。
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

}  // shms 命名空间

#endif  // SMART_HOME_MESSAGE_DISPATCHER_HPP 头文件保护宏
