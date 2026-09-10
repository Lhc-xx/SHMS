#include "CameraDao.hpp"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace {

const char kCreateCameraTableSql[] =
    "CREATE TABLE IF NOT EXISTS t_camera ("
    "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
    "type TINYINT UNSIGNED NOT NULL,"
    "serial_no VARCHAR(64) NOT NULL,"
    "channels SMALLINT UNSIGNED NOT NULL,"
    "ip VARCHAR(45) NOT NULL,"
    "rtsp VARCHAR(512) NOT NULL,"
    "rtmp VARCHAR(512) NOT NULL,"
    "PRIMARY KEY (id),"
    "UNIQUE KEY uk_t_camera_serial_no (serial_no)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

bool parseUnsigned(const std::string& text,
                   std::uint64_t maximum,
                   std::uint64_t* value) {
    if (text.empty() || value == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed =
        std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        parsed > maximum) {
        return false;
    }
    *value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool validText(const std::string& value, std::size_t maximum) {
    if (value.empty() || value.size() > maximum) {
        return false;
    }
    for (std::string::const_iterator it = value.begin();
         it != value.end();
         ++it) {
        if (*it == '\0' || *it == '\r' || *it == '\n' || *it == '\t' ||
            *it == ' ') {
            return false;
        }
    }
    return true;
}

}  // 匿名命名空间

namespace shms {

CameraDao::CameraDao(MySqlClient& client) : client_(client) {}

bool CameraDao::initializeSchema() {
    if (!client_.execute(kCreateCameraTableSql, std::vector<std::string>())) {
        return setError(client_.lastError());
    }
    clearError();
    return true;
}

bool CameraDao::createCamera(std::uint32_t type,
                             const std::string& serialNo,
                             std::uint32_t channels,
                             const std::string& ip,
                             const std::string& rtsp,
                             const std::string& rtmp,
                             std::uint64_t* cameraId) {
    if (!validateCamera(type, serialNo, channels, ip, rtsp, rtmp)) {
        return false;
    }

    const char sql[] =
        "INSERT INTO t_camera "
        "(type, serial_no, channels, ip, rtsp, rtmp) VALUES (?, ?, ?, ?, ?, ?)";
    std::vector<std::string> parameters;
    parameters.push_back(std::to_string(type));
    parameters.push_back(serialNo);
    parameters.push_back(std::to_string(channels));
    parameters.push_back(ip);
    parameters.push_back(rtsp);
    parameters.push_back(rtmp);
    std::uint64_t insertId = 0;
    if (!client_.execute(sql, parameters, nullptr, &insertId)) {
        return setError(client_.lastError());
    }
    if (cameraId != nullptr) {
        *cameraId = insertId;
    }
    clearError();
    return true;
}

bool CameraDao::findById(std::uint64_t cameraId,
                         CameraRecord* record,
                         bool* found) {
    if (record == nullptr || found == nullptr) {
        return setError("findById requires record and found outputs");
    }
    *record = CameraRecord();
    *found = false;
    if (cameraId == 0) {
        return setError("camera id must be positive");
    }

    const char sql[] =
        "SELECT id, type, serial_no, channels, ip, rtsp, rtmp FROM t_camera "
        "WHERE id = ? LIMIT 1";
    std::vector<std::string> parameters(1, std::to_string(cameraId));
    std::vector<std::vector<std::string> > rows;
    if (!client_.query(sql, parameters, &rows)) {
        return setError(client_.lastError());
    }
    if (rows.empty()) {
        clearError();
        return true;
    }
    if (rows[0].size() != 7U) {
        return setError("unexpected t_camera result shape");
    }

    std::uint64_t parsedId = 0;
    std::uint64_t parsedType = 0;
    std::uint64_t parsedChannels = 0;
    if (!parseUnsigned(rows[0][0],
                       std::numeric_limits<std::uint64_t>::max(),
                       &parsedId) ||
        !parseUnsigned(rows[0][1], 1U, &parsedType) ||
        !parseUnsigned(rows[0][3], 64U, &parsedChannels)) {
        return setError("unexpected t_camera numeric value");
    }
    record->id = parsedId;
    record->type = static_cast<std::uint32_t>(parsedType);
    record->serialNo = rows[0][2];
    record->channels = static_cast<std::uint32_t>(parsedChannels);
    record->ip = rows[0][4];
    record->rtsp = rows[0][5];
    record->rtmp = rows[0][6];
    *found = true;
    clearError();
    return true;
}

bool CameraDao::listCameras(std::vector<CameraRecord>* cameras) {
    if (cameras == nullptr) {
        return setError("listCameras requires a camera output");
    }
    cameras->clear();

    const char sql[] =
        "SELECT id, type, serial_no, channels, ip, rtsp, rtmp FROM t_camera "
        "ORDER BY id ASC";
    std::vector<std::vector<std::string> > rows;
    if (!client_.query(sql, std::vector<std::string>(), &rows)) {
        return setError(client_.lastError());
    }
    for (std::vector<std::vector<std::string> >::const_iterator row =
             rows.begin();
         row != rows.end();
         ++row) {
        if (row->size() != 7U) {
            cameras->clear();
            return setError("unexpected t_camera result shape");
        }
        std::uint64_t parsedId = 0;
        std::uint64_t parsedType = 0;
        std::uint64_t parsedChannels = 0;
        if (!parseUnsigned((*row)[0],
                           std::numeric_limits<std::uint64_t>::max(),
                           &parsedId) ||
            !parseUnsigned((*row)[1], 1U, &parsedType) ||
            !parseUnsigned((*row)[3], 64U, &parsedChannels)) {
            cameras->clear();
            return setError("unexpected t_camera numeric value");
        }
        CameraRecord record;
        record.id = parsedId;
        record.type = static_cast<std::uint32_t>(parsedType);
        record.serialNo = (*row)[2];
        record.channels = static_cast<std::uint32_t>(parsedChannels);
        record.ip = (*row)[4];
        record.rtsp = (*row)[5];
        record.rtmp = (*row)[6];
        cameras->push_back(record);
    }
    clearError();
    return true;
}

std::string CameraDao::lastError() const {
    return lastError_;
}

bool CameraDao::validateCamera(std::uint32_t type,
                               const std::string& serialNo,
                               std::uint32_t channels,
                               const std::string& ip,
                               const std::string& rtsp,
                               const std::string& rtmp) {
    if (type > 1U) {
        return setError("camera type must be 0 or 1");
    }
    if (!validText(serialNo, 64U)) {
        return setError("camera serial number must contain 1 to 64 characters");
    }
    if (channels == 0U || channels > 64U) {
        return setError("camera channels must be between 1 and 64");
    }
    if (!validText(ip, 45U)) {
        return setError("camera ip must contain 1 to 45 characters");
    }
    if (rtsp.size() > 512U || rtmp.size() > 512U ||
        rtsp.find('\0') != std::string::npos ||
        rtmp.find('\0') != std::string::npos) {
        return setError("camera stream url must not exceed 512 characters");
    }
    return true;
}

bool CameraDao::setError(const std::string& message) {
    lastError_ = message;
    return false;
}

void CameraDao::clearError() {
    lastError_.clear();
}

}  // shms 命名空间
