#include "CameraProtocol.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

shms::CameraRecord makeCamera(std::uint64_t id,
                              std::uint32_t type,
                              const std::string& serialNo) {
    shms::CameraRecord record;
    record.id = id;
    record.type = type;
    record.serialNo = serialNo;
    record.channels = 2;
    record.ip = "192.168.1.20";
    record.rtsp = "rtsp://192.168.1.20/live";
    record.rtmp = "rtmp://192.168.1.20/live";
    return record;
}

}  // 匿名命名空间

int main() {
    std::string body;
    std::string error;
    expect(shms::CameraProtocolCodec::encodeListRequest(&body, &error),
           "encode camera list request");
    expect(body.empty(), "keep camera list request body empty");
    expect(shms::CameraProtocolCodec::decodeListRequest(body, &error),
           "decode camera list request");
    expect(!shms::CameraProtocolCodec::decodeListRequest("unexpected", &error),
           "reject camera list request data");

    expect(shms::CameraProtocolCodec::encodeViewRequest(42, &body, &error),
           "encode camera view request");
    std::uint64_t cameraId = 0;
    expect(shms::CameraProtocolCodec::decodeViewRequest(body,
                                                        &cameraId,
                                                        &error),
           "decode camera view request");
    expect(cameraId == 42, "preserve camera id");
    expect(!shms::CameraProtocolCodec::encodeViewRequest(0, &body, &error),
           "reject an empty camera id");
    body.assign(8, '\0');
    expect(!shms::CameraProtocolCodec::decodeViewRequest(body,
                                                         &cameraId,
                                                         &error),
           "reject a zero camera id");

    shms::CameraResponse response;
    response.code = shms::CameraResponseCode::Success;
    response.message = "camera list loaded";
    response.cameras.push_back(makeCamera(1, 0, "serial-1"));
    response.cameras.push_back(makeCamera(2, 1, "serial-2"));
    expect(shms::CameraProtocolCodec::encodeResponse(response, &body, &error),
           "encode camera response");

    shms::CameraResponse decoded;
    expect(shms::CameraProtocolCodec::decodeResponse(body, &decoded, &error),
           "decode camera response");
    expect(decoded.code == shms::CameraResponseCode::Success,
           "preserve camera response code");
    expect(decoded.message == response.message,
           "preserve camera response message");
    expect(decoded.cameras.size() == 2U,
           "preserve camera response count");
    expect(decoded.cameras[1].id == 2 && decoded.cameras[1].type == 1U,
           "preserve camera response record");

    body.push_back('x');
    expect(!shms::CameraProtocolCodec::decodeResponse(body, &decoded, &error),
           "reject camera response trailing data");

    response.cameras[0].serialNo.assign(
        shms::CameraProtocolCodec::kMaxSerialBytes + 1U, 'x');
    expect(!shms::CameraProtocolCodec::encodeResponse(response, &body, &error),
           "reject an oversized camera record");

    std::cout << "All CameraProtocol tests passed" << std::endl;
    return EXIT_SUCCESS;
}
