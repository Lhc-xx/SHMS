#ifndef SMART_HOME_CAMERA_PROTOCOL_HANDLER_HPP
#define SMART_HOME_CAMERA_PROTOCOL_HANDLER_HPP

#include "CameraProtocol.hpp"
#include "CameraService.hpp"
#include "Protocol.hpp"

#include <mutex>
#include <string>

namespace shms {

// 将摄像头列表和查看请求交给 CameraService，并把业务结果编码为协议响应。
// 具体 TCP 连接负责在调用前完成登录身份校验。
class CameraProtocolHandler {
public:
    explicit CameraProtocolHandler(CameraService& service);
    CameraProtocolHandler(const CameraProtocolHandler&) = delete;
    CameraProtocolHandler& operator=(const CameraProtocolHandler&) = delete;

    // 处理一条摄像头消息。业务失败会返回结构化响应，报文格式错误返回 false。
    bool handle(const ProtocolMessage& request,
                const std::string& username,
                ProtocolMessage* response);

    std::string lastError() const;

private:
    bool setError(const std::string& message);
    void clearError();

    CameraService& service_;
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_CAMERA_PROTOCOL_HANDLER_HPP 头文件保护宏
