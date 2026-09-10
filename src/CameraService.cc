#include "CameraService.hpp"

#include "MyLogger.hpp"

namespace shms {

CameraService::CameraService(CameraStore& store, MyLogger* logger)
    : store_(store), logger_(logger) {}

bool CameraService::load() {
    std::vector<CameraRecord> loaded;
    if (!store_.listCameras(&loaded)) {
        return setError("camera list loading failed");
    }

    std::map<std::uint64_t, CameraRecord> next;
    for (std::vector<CameraRecord>::const_iterator it = loaded.begin();
         it != loaded.end();
         ++it) {
        if (!validRecord(*it)) {
            return setError("invalid camera record from storage");
        }
        if (!next.insert(std::make_pair(it->id, *it)).second) {
            return setError("duplicate camera id from storage");
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cameras_.swap(next);
    }
    clearError();
    return true;
}

bool CameraService::findCamera(std::uint64_t cameraId,
                               CameraRecord* record) const {
    if (record == nullptr) {
        return setError("camera output cannot be null");
    }
    *record = CameraRecord();
    if (cameraId == 0) {
        return setError("camera id must be positive");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::uint64_t, CameraRecord>::const_iterator it =
        cameras_.find(cameraId);
    if (it == cameras_.end()) {
        return setError("camera not found");
    }
    *record = it->second;
    clearError();
    return true;
}

bool CameraService::listCameras(std::vector<CameraRecord>* cameras) const {
    if (cameras == nullptr) {
        return setError("camera list output cannot be null");
    }
    cameras->clear();
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::map<std::uint64_t, CameraRecord>::const_iterator it =
             cameras_.begin();
         it != cameras_.end();
         ++it) {
        cameras->push_back(it->second);
    }
    clearError();
    return true;
}

bool CameraService::viewCamera(const std::string& username,
                               std::uint64_t cameraId,
                               CameraRecord* record) {
    if (!validUsername(username)) {
        return setError("username is invalid");
    }

    CameraRecord viewed;
    if (!findCamera(cameraId, &viewed)) {
        return false;
    }
    if (record != nullptr) {
        *record = viewed;
    }
    if (logger_ != nullptr) {
        logger_->recordCameraView(username, std::to_string(cameraId));
    }
    clearError();
    return true;
}

std::size_t CameraService::cameraCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cameras_.size();
}

std::string CameraService::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool CameraService::validRecord(const CameraRecord& record) {
    if (record.id == 0 || record.type > 1U || record.channels == 0U ||
        record.channels > 64U || record.serialNo.empty() ||
        record.serialNo.size() > 64U || record.ip.empty() ||
        record.ip.size() > 45U || record.rtsp.size() > 512U ||
        record.rtmp.size() > 512U) {
        return false;
    }
    for (std::string::const_iterator it = record.serialNo.begin();
         it != record.serialNo.end();
         ++it) {
        if (*it == '\0' || *it == '\r' || *it == '\n' || *it == '\t' ||
            *it == ' ') {
            return false;
        }
    }
    for (std::string::const_iterator it = record.ip.begin();
         it != record.ip.end();
         ++it) {
        if (*it == '\0' || *it == '\r' || *it == '\n' || *it == '\t' ||
            *it == ' ') {
            return false;
        }
    }
    return record.rtsp.find('\0') == std::string::npos &&
           record.rtmp.find('\0') == std::string::npos;
}

bool CameraService::validUsername(const std::string& username) {
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

bool CameraService::setError(const std::string& message) const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void CameraService::clearError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
