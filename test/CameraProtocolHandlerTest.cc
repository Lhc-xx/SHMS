#include "CameraProtocolHandler.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

shms::CameraRecord makeCamera(std::uint64_t id) {
    shms::CameraRecord record;
    record.id = id;
    record.type = 0;
    record.serialNo = "serial-" + std::to_string(id);
    record.channels = 2;
    record.ip = "192.168.1." + std::to_string(id);
    record.rtsp = "rtsp://camera/" + std::to_string(id);
    record.rtmp = "rtmp://camera/" + std::to_string(id);
    return record;
}

class MemoryCameraStore : public shms::CameraStore {
public:
    MemoryCameraStore() : failList_(false) {}

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
        if (record == nullptr || found == nullptr) {
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
    std::vector<shms::CameraRecord> records_;

private:
    std::string lastError_;
};

}  // 匿名命名空间

int main() {
    MemoryCameraStore store;
    store.records_.push_back(makeCamera(2));
    store.records_.push_back(makeCamera(1));
    shms::CameraService service(store);
    expect(service.load(), "load camera cache");

    shms::CameraProtocolHandler handler(service);
    shms::ProtocolMessage request;
    shms::ProtocolMessage response;
    request.type = static_cast<std::uint32_t>(
        shms::CameraMessageType::ListRequest);
    expect(handler.handle(request, "alice", &response),
           "handle camera list request");
    expect(response.type == static_cast<std::uint32_t>(
                                shms::CameraMessageType::ListResponse),
           "return camera list response type");
    shms::CameraResponse listResponse;
    std::string error;
    expect(shms::CameraProtocolCodec::decodeResponse(response.body,
                                                     &listResponse,
                                                     &error),
           "decode camera list response");
    expect(listResponse.code == shms::CameraResponseCode::Success &&
               listResponse.cameras.size() == 2U &&
               listResponse.cameras[0].id == 1U,
           "return the sorted camera list");

    request.type = static_cast<std::uint32_t>(
        shms::CameraMessageType::ViewRequest);
    expect(shms::CameraProtocolCodec::encodeViewRequest(2,
                                                        &request.body,
                                                        &error),
           "encode camera view request");
    expect(handler.handle(request, "alice", &response),
           "handle camera view request");
    shms::CameraResponse viewResponse;
    expect(shms::CameraProtocolCodec::decodeResponse(response.body,
                                                     &viewResponse,
                                                     &error),
           "decode camera view response");
    expect(viewResponse.code == shms::CameraResponseCode::Success &&
               viewResponse.cameras.size() == 1U &&
               viewResponse.cameras[0].id == 2U,
           "return the requested camera");

    expect(shms::CameraProtocolCodec::encodeViewRequest(99,
                                                        &request.body,
                                                        &error),
           "encode missing camera request");
    expect(handler.handle(request, "alice", &response),
           "return a business response for a missing camera");
    expect(shms::CameraProtocolCodec::decodeResponse(response.body,
                                                     &viewResponse,
                                                     &error) &&
               viewResponse.code == shms::CameraResponseCode::NotFound,
           "return a camera not found response");

    expect(!handler.handle(request, "", &response),
           "reject a camera request without a user");
    request.type = static_cast<std::uint32_t>(
        shms::CameraMessageType::ListRequest);
    request.body = "invalid";
    expect(!handler.handle(request, "alice", &response),
           "reject a malformed camera list request");

    request.body.clear();
    store.failList_ = true;
    expect(!service.load(), "simulate a camera cache loading failure");
    expect(handler.handle(request, "alice", &response),
           "serve the last valid camera cache");
    expect(shms::CameraProtocolCodec::decodeResponse(response.body,
                                                     &listResponse,
                                                     &error) &&
               listResponse.code == shms::CameraResponseCode::Success &&
               listResponse.cameras.size() == 2U,
           "retain the last valid camera cache");

    std::cout << "All CameraProtocolHandler tests passed" << std::endl;
    return EXIT_SUCCESS;
}
