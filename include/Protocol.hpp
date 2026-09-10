#ifndef SMART_HOME_PROTOCOL_HPP
#define SMART_HOME_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace shms {

// API 规范定义了一个 8 字节头部，后跟不透明消息体：
// Type（4 字节，网络字节序）+ Length（4 字节，网络字节序）+ Value。
struct ProtocolMessage {
    std::uint32_t type;
    std::string body;
};

// 通信规范预留的模块标识。可以添加具体请求/响应类型，而无需修改帧格式。
enum class ProtocolModule : std::uint32_t {
    User = 1000,
    Camera = 2000,
    Video = 3000,
    Record = 4000,
    Ptz = 5000,
    System = 9000
};

class ProtocolCodec {
public:
    static const std::size_t kHeaderSize = 8;

    // 编码一个完整数据帧。消息体对传输层是不透明的，可以为空或包含任意
    // 二进制数据。
    static bool encode(std::uint32_t type,
                       const std::string& body,
                       std::string* frame,
                       std::string* error = nullptr);
};

// TCP 字节流的增量解析器。它可以处理拆分的头部、拆分的消息体，以及一次
// 套接字读取中收到多个数据帧的情况。
class ProtocolParser {
public:
    explicit ProtocolParser(std::size_t maxBodySize = 1024 * 1024);

    // 追加字节并提取当前可用的所有完整数据帧。输入格式错误或消息体过大
    // 时，解析器会进入失败状态，直到调用 reset() 才返回正常。
    bool append(const void* data,
                std::size_t size,
                std::vector<ProtocolMessage>* messages);
    bool append(const std::string& data,
                std::vector<ProtocolMessage>* messages);

    void reset();
    bool failed() const;
    std::size_t bufferedBytes() const;
    std::size_t maxBodySize() const;
    std::string lastError() const;

private:
    bool fail(const std::string& message);

    const std::size_t maxBodySize_;
    std::string buffer_;
    bool failed_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_PROTOCOL_HPP 头文件保护宏
