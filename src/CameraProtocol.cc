#include "CameraProtocol.hpp"

#include <limits>

namespace {

void writeUint32(std::uint32_t value, std::string* output) {
    output->push_back(static_cast<char>((value >> 24) & 0xffU));
    output->push_back(static_cast<char>((value >> 16) & 0xffU));
    output->push_back(static_cast<char>((value >> 8) & 0xffU));
    output->push_back(static_cast<char>(value & 0xffU));
}

void writeUint64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

std::uint32_t readUint32(const std::string& body, std::size_t offset) {
    return (static_cast<std::uint32_t>(
                static_cast<unsigned char>(body[offset]))
            << 24) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(body[offset + 1U]))
            << 16) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(body[offset + 2U]))
            << 8) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(body[offset + 3U]));
}

std::uint64_t readUint64(const std::string& body, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8U; ++index) {
        value = (value << 8U) |
                static_cast<std::uint64_t>(
                    static_cast<unsigned char>(body[offset + index]));
    }
    return value;
}

bool setError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool canRead(const std::string& body,
             std::size_t offset,
             std::size_t size) {
    return offset <= body.size() && size <= body.size() - offset;
}

bool validText(const std::string& value,
               std::size_t maximum,
               bool allowEmpty) {
    if ((!allowEmpty && value.empty()) || value.size() > maximum ||
        value.find('\0') != std::string::npos) {
        return false;
    }
    return true;
}

bool validRecord(const shms::CameraRecord& record) {
    return record.id != 0 && record.type <= 1U && record.channels > 0U &&
           record.channels <= 64U &&
           validText(record.serialNo, shms::CameraProtocolCodec::kMaxSerialBytes,
                     false) &&
           validText(record.ip, shms::CameraProtocolCodec::kMaxIpBytes, false) &&
           validText(record.rtsp, shms::CameraProtocolCodec::kMaxUrlBytes, true) &&
           validText(record.rtmp, shms::CameraProtocolCodec::kMaxUrlBytes, true);
}

void appendString(const std::string& value, std::string* body) {
    writeUint32(static_cast<std::uint32_t>(value.size()), body);
    body->append(value);
}

bool readString(const std::string& body,
                std::size_t* offset,
                std::size_t maximum,
                bool allowEmpty,
                std::string* value,
                std::string* error,
                const char* field) {
    if (offset == nullptr || value == nullptr || !canRead(body, *offset, 4U)) {
        return setError(error, std::string(field) + " length is missing");
    }
    const std::uint32_t length = readUint32(body, *offset);
    *offset += 4U;
    if (length > maximum || (!allowEmpty && length == 0U) ||
        !canRead(body, *offset, static_cast<std::size_t>(length))) {
        return setError(error, std::string(field) + " is invalid");
    }
    value->assign(body.data() + *offset, static_cast<std::size_t>(length));
    *offset += static_cast<std::size_t>(length);
    if (!validText(*value, maximum, allowEmpty)) {
        return setError(error, std::string(field) + " contains invalid data");
    }
    return true;
}

bool encodeRecord(const shms::CameraRecord& record, std::string* body) {
    if (!validRecord(record)) {
        return false;
    }
    writeUint64(record.id, body);
    writeUint32(record.type, body);
    writeUint32(record.channels, body);
    appendString(record.serialNo, body);
    appendString(record.ip, body);
    appendString(record.rtsp, body);
    appendString(record.rtmp, body);
    return true;
}

bool decodeRecord(const std::string& body,
                  std::size_t* offset,
                  shms::CameraRecord* record,
                  std::string* error) {
    if (offset == nullptr || record == nullptr || !canRead(body, *offset, 16U)) {
        return setError(error, "camera record header is truncated");
    }
    record->id = readUint64(body, *offset);
    *offset += 8U;
    record->type = readUint32(body, *offset);
    *offset += 4U;
    record->channels = readUint32(body, *offset);
    *offset += 4U;
    if (!readString(body,
                    offset,
                    shms::CameraProtocolCodec::kMaxSerialBytes,
                    false,
                    &record->serialNo,
                    error,
                    "serial_no") ||
        !readString(body,
                    offset,
                    shms::CameraProtocolCodec::kMaxIpBytes,
                    false,
                    &record->ip,
                    error,
                    "ip") ||
        !readString(body,
                    offset,
                    shms::CameraProtocolCodec::kMaxUrlBytes,
                    true,
                    &record->rtsp,
                    error,
                    "rtsp") ||
        !readString(body,
                    offset,
                    shms::CameraProtocolCodec::kMaxUrlBytes,
                    true,
                    &record->rtmp,
                    error,
                    "rtmp")) {
        return false;
    }
    if (!validRecord(*record)) {
        return setError(error, "camera record is invalid");
    }
    return true;
}

}  // 匿名命名空间

namespace shms {

const std::size_t CameraProtocolCodec::kMaxSerialBytes;
const std::size_t CameraProtocolCodec::kMaxIpBytes;
const std::size_t CameraProtocolCodec::kMaxUrlBytes;
const std::size_t CameraProtocolCodec::kMaxMessageBytes;
const std::size_t CameraProtocolCodec::kMaxCameraCount;

bool CameraProtocolCodec::encodeListRequest(std::string* body,
                                            std::string* error) {
    if (body == nullptr) {
        return setError(error, "body output cannot be null");
    }
    body->clear();
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CameraProtocolCodec::decodeListRequest(const std::string& body,
                                            std::string* error) {
    if (!body.empty()) {
        return setError(error, "camera list request body must be empty");
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CameraProtocolCodec::encodeViewRequest(std::uint64_t cameraId,
                                            std::string* body,
                                            std::string* error) {
    if (body == nullptr) {
        return setError(error, "body output cannot be null");
    }
    if (cameraId == 0) {
        return setError(error, "camera id must be positive");
    }
    body->clear();
    writeUint64(cameraId, body);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CameraProtocolCodec::decodeViewRequest(const std::string& body,
                                            std::uint64_t* cameraId,
                                            std::string* error) {
    if (cameraId == nullptr) {
        return setError(error, "camera id output cannot be null");
    }
    *cameraId = 0;
    if (body.size() != 8U) {
        return setError(error, "camera view request must contain one id");
    }
    *cameraId = readUint64(body, 0);
    if (*cameraId == 0) {
        *cameraId = 0;
        return setError(error, "camera id must be positive");
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CameraProtocolCodec::encodeResponse(const CameraResponse& response,
                                         std::string* body,
                                         std::string* error) {
    if (body == nullptr) {
        return setError(error, "body output cannot be null");
    }
    if (static_cast<std::uint32_t>(response.code) >
        static_cast<std::uint32_t>(CameraResponseCode::ProtocolError)) {
        return setError(error, "camera response code is invalid");
    }
    if (!validText(response.message, kMaxMessageBytes, true) ||
        response.cameras.size() > kMaxCameraCount) {
        return setError(error, "camera response is too large or invalid");
    }
    body->clear();
    writeUint32(static_cast<std::uint32_t>(response.code), body);
    appendString(response.message, body);
    writeUint32(static_cast<std::uint32_t>(response.cameras.size()), body);
    for (std::vector<CameraRecord>::const_iterator it =
             response.cameras.begin();
         it != response.cameras.end();
         ++it) {
        if (!encodeRecord(*it, body)) {
            body->clear();
            return setError(error, "camera response contains an invalid record");
        }
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CameraProtocolCodec::decodeResponse(const std::string& body,
                                         CameraResponse* response,
                                         std::string* error) {
    if (response == nullptr) {
        return setError(error, "response output cannot be null");
    }
    *response = CameraResponse();
    if (!canRead(body, 0, 8U)) {
        return setError(error, "camera response header is truncated");
    }
    const std::uint32_t code = readUint32(body, 0);
    if (code > static_cast<std::uint32_t>(CameraResponseCode::ProtocolError)) {
        return setError(error, "camera response code is invalid");
    }
    response->code = static_cast<CameraResponseCode>(code);
    std::size_t offset = 4U;
    if (!readString(body,
                    &offset,
                    kMaxMessageBytes,
                    true,
                    &response->message,
                    error,
                    "response message")) {
        return false;
    }
    if (!canRead(body, offset, 4U)) {
        return setError(error, "camera response count is missing");
    }
    const std::uint32_t count = readUint32(body, offset);
    offset += 4U;
    if (count > kMaxCameraCount) {
        return setError(error, "camera response count is too large");
    }
    response->cameras.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        CameraRecord record;
        if (!decodeRecord(body, &offset, &record, error)) {
            response->cameras.clear();
            return false;
        }
        response->cameras.push_back(record);
    }
    if (offset != body.size()) {
        response->cameras.clear();
        return setError(error, "camera response contains trailing data");
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

}  // shms 命名空间
