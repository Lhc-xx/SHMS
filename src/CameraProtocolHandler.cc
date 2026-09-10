#include "CameraProtocolHandler.hpp"

namespace {

const std::uint32_t kListRequest = static_cast<std::uint32_t>(
    shms::CameraMessageType::ListRequest);
const std::uint32_t kViewRequest = static_cast<std::uint32_t>(
    shms::CameraMessageType::ViewRequest);
const std::uint32_t kListResponse = static_cast<std::uint32_t>(
    shms::CameraMessageType::ListResponse);
const std::uint32_t kViewResponse = static_cast<std::uint32_t>(
    shms::CameraMessageType::ViewResponse);

bool validUsername(const std::string& username) {
    if (username.empty() || username.size() > 20U ||
        username.find('\0') != std::string::npos) {
        return false;
    }
    for (std::string::const_iterator it = username.begin();
         it != username.end();
         ++it) {
        if (*it == '\r' || *it == '\n' || *it == '\t' || *it == ' ') {
            return false;
        }
    }
    return true;
}

shms::CameraResponseCode responseCodeForViewError(
    const std::string& error) {
    return error == "camera not found"
               ? shms::CameraResponseCode::NotFound
               : shms::CameraResponseCode::InvalidArgument;
}

}  // 匿名命名空间

namespace shms {

CameraProtocolHandler::CameraProtocolHandler(CameraService& service)
    : service_(service) {}

bool CameraProtocolHandler::handle(const ProtocolMessage& request,
                                   const std::string& username,
                                   ProtocolMessage* response) {
    if (response == nullptr) {
        return setError("response output cannot be null");
    }
    response->type = 0;
    response->body.clear();
    if (!validUsername(username)) {
        return setError("authenticated username is required");
    }

    CameraResponse cameraResponse;
    if (request.type == kListRequest) {
        if (!CameraProtocolCodec::decodeListRequest(request.body,
                                                    nullptr)) {
            return setError("camera list request body is invalid");
        }
        response->type = kListResponse;
        if (!service_.listCameras(&cameraResponse.cameras)) {
            cameraResponse.code = CameraResponseCode::StorageError;
            cameraResponse.message = service_.lastError();
            cameraResponse.cameras.clear();
        } else {
            cameraResponse.code = CameraResponseCode::Success;
            cameraResponse.message = "camera list loaded";
        }
    } else if (request.type == kViewRequest) {
        std::uint64_t cameraId = 0;
        std::string decodeError;
        if (!CameraProtocolCodec::decodeViewRequest(request.body,
                                                    &cameraId,
                                                    &decodeError)) {
            return setError(decodeError);
        }
        response->type = kViewResponse;
        CameraRecord record;
        if (!service_.viewCamera(username, cameraId, &record)) {
            cameraResponse.code = responseCodeForViewError(
                service_.lastError());
            cameraResponse.message = service_.lastError();
        } else {
            cameraResponse.code = CameraResponseCode::Success;
            cameraResponse.message = "camera view loaded";
            cameraResponse.cameras.push_back(record);
        }
    } else {
        return setError("unsupported camera request type");
    }

    std::string encodeError;
    if (!CameraProtocolCodec::encodeResponse(cameraResponse,
                                             &response->body,
                                             &encodeError)) {
        response->type = 0;
        response->body.clear();
        return setError(encodeError);
    }
    clearError();
    return true;
}

std::string CameraProtocolHandler::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool CameraProtocolHandler::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void CameraProtocolHandler::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
