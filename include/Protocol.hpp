#ifndef SMART_HOME_PROTOCOL_HPP
#define SMART_HOME_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace shms {

// The API specification defines an 8-byte header followed by an opaque body:
// Type (4 bytes, network order) + Length (4 bytes, network order) + Value.
struct ProtocolMessage {
    std::uint32_t type;
    std::string body;
};

// Module identifiers reserved by the communication specification. Individual
// request/response types can be added without changing the frame format.
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

    // Encode one complete frame. The body is opaque to the transport layer
    // and may contain zero bytes or arbitrary binary data.
    static bool encode(std::uint32_t type,
                       const std::string& body,
                       std::string* frame,
                       std::string* error = nullptr);
};

// Incremental parser for a TCP byte stream. It handles fragmented headers,
// fragmented bodies, and multiple frames received in one socket read.
class ProtocolParser {
public:
    explicit ProtocolParser(std::size_t maxBodySize = 1024 * 1024);

    // Append bytes and extract every complete frame currently available.
    // On malformed input or an oversized body, the parser enters a failed
    // state and returns false until reset() is called.
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

}  // namespace shms

#endif  // SMART_HOME_PROTOCOL_HPP
