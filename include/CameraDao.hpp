#ifndef SMART_HOME_CAMERA_DAO_HPP
#define SMART_HOME_CAMERA_DAO_HPP

#include "MySqlClient.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace shms {

struct CameraRecord {
    std::uint64_t id;
    std::uint32_t type;
    std::string serialNo;
    std::uint32_t channels;
    std::string ip;
    std::string rtsp;
    std::string rtmp;

    CameraRecord() : id(0), type(0), channels(0) {}
};

// 摄像头信息 DAO，对应需求文档中的 t_camera 表。
// 所有写入和查询都通过 MySqlClient 的预处理语句执行。
class CameraDao {
public:
    explicit CameraDao(MySqlClient& client);
    CameraDao(const CameraDao&) = delete;
    CameraDao& operator=(const CameraDao&) = delete;

    // 如果数据表不存在则创建，便于部署脚本和测试重复执行。
    bool initializeSchema();

    // 新增摄像头。type 取 0（枪机）或 1（球机），channels 取 1 到 64。
    bool createCamera(std::uint32_t type,
                      const std::string& serialNo,
                      std::uint32_t channels,
                      const std::string& ip,
                      const std::string& rtsp,
                      const std::string& rtmp,
                      std::uint64_t* cameraId = nullptr);

    // 根据摄像头编号查询信息，查询不到时 found 为 false，而不是数据库错误。
    bool findById(std::uint64_t cameraId,
                  CameraRecord* record,
                  bool* found);

    // 按编号升序读取全部摄像头，供用户登录后加载设备列表。
    bool listCameras(std::vector<CameraRecord>* cameras);

    std::string lastError() const;

private:
    bool validateCamera(std::uint32_t type,
                        const std::string& serialNo,
                        std::uint32_t channels,
                        const std::string& ip,
                        const std::string& rtsp,
                        const std::string& rtmp);
    bool setError(const std::string& message);
    void clearError();

    MySqlClient& client_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_CAMERA_DAO_HPP 头文件保护宏
