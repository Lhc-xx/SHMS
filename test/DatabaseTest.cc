#include "CameraDao.hpp"
#include "MySqlClient.hpp"
#include "UserDao.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

}  // 匿名命名空间

int main() {
    // 单元测试不要求部署凭据或正在运行的数据库。它验证参数校验和可确定的
    // 断开连接错误；README 中的云服务器集成测试命令用于验证真实 MySQL 实例。
    shms::MySqlClient client;
    expect(!client.connected(), "start disconnected");

    std::vector<std::vector<std::string> > rows;
    expect(!client.query("SELECT 1", std::vector<std::string>(), &rows),
           "reject query before connection");
    expect(client.lastError().find("not connected") != std::string::npos,
           "explain disconnected query");

    shms::UserDao dao(client);
    expect(!dao.createUser("", "salt", "cipher"),
           "reject empty user name before database access");
    expect(dao.lastError().find("1 to 20") != std::string::npos,
           "explain user name validation");
    expect(!dao.createUser("alice", "", "cipher"),
           "reject empty setting before database access");
    expect(!dao.createUser("alice", "salt", ""),
           "reject empty ciphertext before database access");

    shms::UserRecord record;
    bool found = false;
    expect(!dao.findByName("", &record, &found),
           "reject empty lookup name");
    expect(!found, "leave lookup result false on validation failure");

    shms::CameraDao cameraDao(client);
    expect(!cameraDao.createCamera(2,
                                   "camera-001",
                                   2,
                                   "192.168.1.10",
                                   "rtsp://camera/stream",
                                   "rtmp://camera/live"),
           "reject an unknown camera type");
    expect(cameraDao.lastError().find("0 or 1") != std::string::npos,
           "explain camera type validation");
    expect(!cameraDao.createCamera(0,
                                   "",
                                   2,
                                   "192.168.1.10",
                                   "rtsp://camera/stream",
                                   "rtmp://camera/live"),
           "reject an empty camera serial number");
    expect(!cameraDao.createCamera(0,
                                   "camera-001",
                                   0,
                                   "192.168.1.10",
                                   "rtsp://camera/stream",
                                   "rtmp://camera/live"),
           "reject zero camera channels");

    shms::CameraRecord camera;
    expect(!cameraDao.findById(0, &camera, &found),
           "reject a zero camera id");
    std::vector<shms::CameraRecord> cameras;
    expect(!cameraDao.listCameras(nullptr),
           "reject a null camera list output");

    std::cout << "All Database tests passed" << std::endl;
    return EXIT_SUCCESS;
}
