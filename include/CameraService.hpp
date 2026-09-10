#ifndef SMART_HOME_CAMERA_SERVICE_HPP
#define SMART_HOME_CAMERA_SERVICE_HPP

#include "CameraStore.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace shms {

class MyLogger;

// 摄像头业务服务，负责登录后加载设备列表、内存缓存、查询和查看日志。
// 缓存只在一次完整加载成功后替换，数据库临时故障不会清空上一次有效列表。
class CameraService {
public:
    explicit CameraService(CameraStore& store, MyLogger* logger = nullptr);
    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;

    // 从存储层重新加载全部摄像头，并以摄像头编号建立内存索引。
    bool load();

    // 查询缓存中的一个摄像头，查询不到时返回 false。
    bool findCamera(std::uint64_t cameraId, CameraRecord* record) const;

    // 按摄像头编号升序复制当前缓存，供协议层返回给客户端。
    bool listCameras(std::vector<CameraRecord>* cameras) const;

    // 记录用户查看摄像头的操作，并可同时返回摄像头信息。
    bool viewCamera(const std::string& username,
                   std::uint64_t cameraId,
                   CameraRecord* record = nullptr);

    std::size_t cameraCount() const;
    std::string lastError() const;

private:
    static bool validRecord(const CameraRecord& record);
    static bool validUsername(const std::string& username);
    bool setError(const std::string& message) const;
    void clearError() const;

    CameraStore& store_;
    MyLogger* logger_;
    mutable std::mutex mutex_;
    std::map<std::uint64_t, CameraRecord> cameras_;

    mutable std::mutex errorMutex_;
    mutable std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_CAMERA_SERVICE_HPP 头文件保护宏
