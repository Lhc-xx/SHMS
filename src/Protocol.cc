#include "Protocol.hpp"

#include <limits>
#include <utility>

namespace {

std::uint32_t readUint32(const char* data) {
    return (static_cast<std::uint32_t>(
                static_cast<unsigned char>(data[0]))
            << 24) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(data[1]))
            << 16) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(data[2]))
            << 8) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(data[3]));
}

void writeUint32(std::uint32_t value, char* data) {
    data[0] = static_cast<char>((value >> 24) & 0xff);
    data[1] = static_cast<char>((value >> 16) & 0xff);
    data[2] = static_cast<char>((value >> 8) & 0xff);
    data[3] = static_cast<char>(value & 0xff);
}

}  // namespace

namespace shms {

const std::size_t ProtocolCodec::kHeaderSize;

bool ProtocolCodec::encode(std::uint32_t type,
                           const std::string& body,
                           std::string* frame,
                           std::string* error) {
    if (frame == nullptr) {
        if (error != nullptr) {
            *error = "frame output cannot be null";
        }
        return false;
    }
    if (type == 0) {
        if (error != nullptr) {
            *error = "message type must be non-zero";
        }
        return false;
    }
    if (body.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        if (error != nullptr) {
            *error = "message body exceeds the 32-bit length field";
        }
        return false;
    }

    frame->assign(ProtocolCodec::kHeaderSize, '\0');
    writeUint32(type, &(*frame)[0]);
    writeUint32(static_cast<std::uint32_t>(body.size()), &(*frame)[4]);
    frame->append(body);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

ProtocolParser::ProtocolParser(std::size_t maxBodySize)
    : maxBodySize_(
          maxBodySize >
                  static_cast<std::size_t>(
                      std::numeric_limits<std::uint32_t>::max()) -
                      ProtocolCodec::kHeaderSize
              ? static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max()) -
                    ProtocolCodec::kHeaderSize
              : maxBodySize),
      failed_(false) {}

bool ProtocolParser::append(const void* data,
                            std::size_t size,
                            std::vector<ProtocolMessage>* messages) {
    if (messages == nullptr) {
        return fail("message output cannot be null");
    }
    if (failed_) {
        return false;
    }
    if (data == nullptr && size != 0) {
        return fail("input data cannot be null when size is non-zero");
    }

    const std::size_t maxFrameSize =
        ProtocolCodec::kHeaderSize + maxBodySize_;
    if (buffer_.size() > maxFrameSize ||
        size > maxFrameSize - buffer_.size()) {
        return fail("protocol input exceeds the maximum frame size");
    }
    if (size != 0) {
        buffer_.append(static_cast<const char*>(data), size);
    }

    while (buffer_.size() >= ProtocolCodec::kHeaderSize) {
        const std::uint32_t type = readUint32(buffer_.data());
        const std::uint32_t bodySize = readUint32(buffer_.data() + 4);
        if (type == 0) {
            return fail("protocol message type must be non-zero");
        }
        if (bodySize > maxBodySize_) {
            return fail("protocol body exceeds configured maximum");
        }

        const std::size_t frameSize =
            ProtocolCodec::kHeaderSize + static_cast<std::size_t>(bodySize);
        if (buffer_.size() < frameSize) {
            break;
        }

        ProtocolMessage message;
        message.type = type;
        message.body.assign(buffer_.data() + ProtocolCodec::kHeaderSize,
                            bodySize);
        messages->push_back(std::move(message));
        buffer_.erase(0, frameSize);
    }
    lastError_.clear();
    return true;
}

bool ProtocolParser::append(const std::string& data,
                            std::vector<ProtocolMessage>* messages) {
    return append(data.data(), data.size(), messages);
}

void ProtocolParser::reset() {
    buffer_.clear();
    failed_ = false;
    lastError_.clear();
}

bool ProtocolParser::failed() const {
    return failed_;
}

std::size_t ProtocolParser::bufferedBytes() const {
    return buffer_.size();
}

std::size_t ProtocolParser::maxBodySize() const {
    return maxBodySize_;
}

std::string ProtocolParser::lastError() const {
    return lastError_;
}

bool ProtocolParser::fail(const std::string& message) {
    failed_ = true;
    lastError_ = message;
    return false;
}

}  // namespace shms
