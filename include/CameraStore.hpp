#ifndef SMART_HOME_CAMERA_STORE_HPP
#define SMART_HOME_CAMERA_STORE_HPP

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

// 摄像头服务依赖的存储抽象。生产环境使用 CameraDao，测试可以使用内存
// 存储验证设备缓存和查询逻辑。
class CameraStore {
public:
    virtual ~CameraStore() {}

    // 读取全部摄像头，调用方负责检查返回记录是否合法。
    virtual bool listCameras(std::vector<CameraRecord>* cameras) = 0;

    // 按摄像头编号查询记录，查询不到时 found 为 false。
    virtual bool findById(std::uint64_t cameraId,
                          CameraRecord* record,
                          bool* found) = 0;

    // 返回最近一次存储操作的错误信息。
    virtual std::string lastError() const = 0;
};

}  // shms 命名空间

#endif  // SMART_HOME_CAMERA_STORE_HPP 头文件保护宏
