#include "CameraService.hpp"

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

shms::CameraRecord makeCamera(std::uint64_t id,
                              const std::string& serialNo) {
    shms::CameraRecord record;
    record.id = id;
    record.type = id % 2U == 0U ? 1U : 0U;
    record.serialNo = serialNo;
    record.channels = 2;
    record.ip = "192.168.1." + std::to_string(id);
    record.rtsp = "rtsp://camera/" + std::to_string(id);
    record.rtmp = "rtmp://camera/" + std::to_string(id);
    return record;
}

class MemoryCameraStore : public shms::CameraStore {
public:
    MemoryCameraStore() : failList_(false), failFind_(false) {}

    bool listCameras(std::vector<shms::CameraRecord>* cameras) override {
        if (failList_ || cameras == nullptr) {
            lastError_ = "memory camera list failed";
            return false;
        }
        *cameras = records_;
        lastError_.clear();
        return true;
    }

    bool findById(std::uint64_t cameraId,
                  shms::CameraRecord* record,
                  bool* found) override {
        if (failFind_ || record == nullptr || found == nullptr) {
            lastError_ = "memory camera lookup failed";
            return false;
        }
        *record = shms::CameraRecord();
        *found = false;
        for (std::vector<shms::CameraRecord>::const_iterator it =
                 records_.begin();
             it != records_.end();
             ++it) {
            if (it->id == cameraId) {
                *record = *it;
                *found = true;
                break;
            }
        }
        lastError_.clear();
        return true;
    }

    std::string lastError() const override { return lastError_; }

    bool failList_;
    bool failFind_;
    std::vector<shms::CameraRecord> records_;

private:
    std::string lastError_;
};

}  // 匿名命名空间

int main() {
    MemoryCameraStore store;
    store.records_.push_back(makeCamera(2, "camera-002"));
    store.records_.push_back(makeCamera(1, "camera-001"));

    shms::CameraService service(store);
    expect(service.load(), "load cameras into memory");
    expect(service.cameraCount() == 2U, "count cached cameras");

    std::vector<shms::CameraRecord> cameras;
    expect(service.listCameras(&cameras) && cameras.size() == 2U,
           "list cached cameras");
    expect(cameras[0].id == 1U && cameras[1].id == 2U,
           "sort cached cameras by id");

    shms::CameraRecord record;
    expect(service.findCamera(2, &record) && record.serialNo == "camera-002",
           "find a cached camera");
    expect(!service.findCamera(99, &record), "report a missing camera");
    expect(service.viewCamera("alice", 1, &record) && record.id == 1U,
           "view a camera and return its record");
    expect(!service.viewCamera("", 1, nullptr),
           "reject a missing viewer name");

    store.failList_ = true;
    expect(!service.load(), "report camera store failures");
    expect(service.cameraCount() == 2U,
           "retain the last valid cache after a failed reload");

    store.failList_ = false;
    store.records_.push_back(makeCamera(2, "duplicate-id"));
    expect(!service.load(), "reject duplicate camera ids");
    expect(service.cameraCount() == 2U,
           "retain the cache after invalid data");

    std::cout << "All CameraService tests passed" << std::endl;
    return EXIT_SUCCESS;
}
