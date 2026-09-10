#ifndef SMART_HOME_CAMERA_PROTOCOL_HPP
#define SMART_HOME_CAMERA_PROTOCOL_HPP

#include "CameraStore.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace shms {

// 摄像头模块的请求和响应消息类型，均属于 ProtocolModule::Camera（2000）。
enum class CameraMessageType : std::uint32_t {
    ListRequest = 2001,
    ListResponse = 2002,
    ViewRequest = 2003,
    ViewResponse = 2004
};

enum class CameraResponseCode : std::uint32_t {
    Success = 0,
    InvalidArgument = 1,
    NotFound = 2,
    StorageError = 3,
    ProtocolError = 4
};

struct CameraResponse {
    CameraResponseCode code;
    std::string message;
    std::vector<CameraRecord> cameras;

    CameraResponse() : code(CameraResponseCode::ProtocolError) {}
};

// 摄像头协议编解码器。整数使用网络字节序，字符串使用 uint32 长度前缀。
class CameraProtocolCodec {
public:
    static const std::size_t kMaxSerialBytes = 64;
    static const std::size_t kMaxIpBytes = 45;
    static const std::size_t kMaxUrlBytes = 512;
    static const std::size_t kMaxMessageBytes = 512;
    static const std::size_t kMaxCameraCount = 4096;

    // 编码和解码摄像头列表请求。请求体必须为空。
    static bool encodeListRequest(std::string* body,
                                  std::string* error = nullptr);
    static bool decodeListRequest(const std::string& body,
                                  std::string* error = nullptr);

    // 编码和解码查看摄像头请求，消息体只有一个 uint64 摄像头编号。
    static bool encodeViewRequest(std::uint64_t cameraId,
                                  std::string* body,
                                  std::string* error = nullptr);
    static bool decodeViewRequest(const std::string& body,
                                  std::uint64_t* cameraId,
                                  std::string* error = nullptr);

    // 编码和解码列表、查看共用的响应体。查看成功时 cameras 中只有一条记录。
    static bool encodeResponse(const CameraResponse& response,
                               std::string* body,
                               std::string* error = nullptr);
    static bool decodeResponse(const std::string& body,
                               CameraResponse* response,
                               std::string* error = nullptr);
};

}  // shms 命名空间

#endif  // SMART_HOME_CAMERA_PROTOCOL_HPP 头文件保护宏
