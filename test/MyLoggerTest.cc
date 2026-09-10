#include "MyLogger.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

std::string readFile(const std::string& path) {
    std::ifstream input(path.c_str());
    expect(input.is_open(), "open generated log file");
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

}  // 匿名命名空间

int main() {
    // 测试在可执行文件旁写入日志，并删除自己生成的文件，避免污染源码目录。
    const std::string path = "my_logger_test.log";
    std::remove(path.c_str());

    shms::MyLogger& logger = shms::MyLogger::instance();
    expect(&logger == &shms::MyLogger::instance(),
           "MyLogger is a singleton");
    expect(!logger.initialized(), "logger starts uninitialized");
    expect(!logger.info("message before initialization"),
           "reject writes before initialization");

    // 选择 DEBUG 级别，以便验证所有受支持的日志级别。
    expect(logger.initialize(path, shms::MyLogger::Level::Debug),
           "initialize log4cpp file appender");
    expect(logger.initialized(), "logger reports initialized state");
    expect(logger.debug("debug message"), "write debug message");
    expect(logger.info("info message"), "write info message");
    expect(logger.warn("warning message"), "write warning message");
    expect(logger.error("error message"), "write error message");
    expect(logger.recordUserRegistration("alice", true),
           "record successful registration");
    expect(logger.recordUserLogin("alice", true), "record successful login");
    expect(logger.recordCameraView("alice", "camera-001"),
           "record camera view");

    // 多个工作线程模拟服务端工作线程池，并验证每次操作都保持为完整的日志记录。
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) {
        workers.push_back(std::thread([&logger, worker]() {
            for (int index = 0; index < 10; ++index) {
                logger.info("concurrent message " + std::to_string(worker) +
                            "-" + std::to_string(index));
            }
        }));
    }
    for (std::vector<std::thread>::iterator it = workers.begin();
         it != workers.end(); ++it) {
        it->join();
    }

    logger.shutdown();
    const std::string contents = readFile(path);
    expect(contents.find("[DEBUG]") != std::string::npos,
           "write debug level");
    expect(contents.find("user registration succeeded: username=alice") !=
               std::string::npos,
           "write registration operation");
    expect(contents.find("user login succeeded: username=alice") !=
               std::string::npos,
           "write login operation");
    expect(contents.find("camera viewed: username=alice, camera_id=camera-001") !=
               std::string::npos,
           "write camera view operation");
    expect(contents.find("concurrent message") != std::string::npos,
           "write concurrent messages");
    expect(!logger.initialized(), "logger shuts down cleanly");

    std::remove(path.c_str());
    std::cout << "All MyLogger tests passed" << std::endl;
    return EXIT_SUCCESS;
}
