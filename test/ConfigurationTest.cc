#include "Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str());
    expect(output.is_open(), "open temporary configuration file");
    output << content;
    expect(output.good(), "write temporary configuration file");
}

}  // 匿名命名空间

int main() {
    // 函数内静态变量必须始终返回同一个对象。
    shms::Configuration& configuration = shms::Configuration::instance();

    expect(&configuration == &shms::Configuration::instance(),
           "Configuration is a singleton");
    // 验证文档规定的 server.conf 格式和全部六个必需值。
    expect(configuration.load("../conf/server.conf"),
           "load the project configuration");
    expect(configuration.loaded(), "configuration reports loaded state");
    expect(configuration.ip() == "127.0.0.1", "read ip");
    expect(configuration.port() == 7777, "read port");
    expect(configuration.threadNum() == 4, "read thread_num");
    expect(configuration.taskNum() == 10000, "read task_num");
    expect(configuration.videoPath() == "./data/", "read video_path");
    expect(configuration.logFile() == "./log/server.log", "read log_file");

    // 应接受行内注释和允许范围内的最大端口值。
    const std::string validPath = "configuration_test_valid.conf";
    writeFile(validPath,
              "# 允许注释和空行\n"
              "ip 10.0.0.8 # 行内注释\n"
              "port 65535\n"
              "thread_num 2\n"
              "task_num 10\n"
              "video_path /tmp/video\n"
              "log_file /tmp/server.log\n");
    expect(configuration.load(validPath), "load a valid custom configuration");
    expect(configuration.ip() == "10.0.0.8", "read custom ip");
    expect(configuration.port() == 65535, "read maximum valid port");
    std::remove(validPath.c_str());

    // 重新加载失败时不能替换内存中最后一份有效配置。
    const std::string invalidPath = "configuration_test_invalid.conf";
    writeFile(invalidPath,
              "ip 10.0.0.8\n"
              "port 0\n"
              "thread_num 2\n"
              "task_num 10\n"
              "video_path /tmp/video\n"
              "log_file /tmp/server.log\n");
    expect(!configuration.load(invalidPath), "reject an invalid port");
    expect(configuration.lastError().find("port") != std::string::npos,
           "explain invalid port");
    expect(configuration.ip() == "10.0.0.8",
           "preserve the last valid configuration after failure");
    std::remove(invalidPath.c_str());

    expect(!configuration.load("configuration_file_that_does_not_exist.conf"),
           "reject a missing configuration file");
    expect(configuration.lastError().find("cannot open") != std::string::npos,
           "explain missing configuration file");
    expect(configuration.loaded(),
           "preserve the loaded state after a missing file");

    std::cout << "All Configuration tests passed" << std::endl;
    return EXIT_SUCCESS;
}
