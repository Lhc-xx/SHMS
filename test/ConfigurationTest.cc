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

}  // namespace

int main() {
    shms::Configuration& configuration = shms::Configuration::instance();

    expect(&configuration == &shms::Configuration::instance(),
           "Configuration is a singleton");
    expect(configuration.load("../conf/server.conf"),
           "load the project configuration");
    expect(configuration.loaded(), "configuration reports loaded state");
    expect(configuration.ip() == "127.0.0.1", "read ip");
    expect(configuration.port() == 8000, "read port");
    expect(configuration.threadNum() == 4, "read thread_num");
    expect(configuration.taskNum() == 10000, "read task_num");
    expect(configuration.videoPath() == "./data/", "read video_path");
    expect(configuration.logFile() == "./log/server.log", "read log_file");

    const std::string validPath = "configuration_test_valid.conf";
    writeFile(validPath,
              "# comments and blank lines are allowed\n"
              "ip 10.0.0.8 # inline comment\n"
              "port 65535\n"
              "thread_num 2\n"
              "task_num 10\n"
              "video_path /tmp/video\n"
              "log_file /tmp/server.log\n");
    expect(configuration.load(validPath), "load a valid custom configuration");
    expect(configuration.ip() == "10.0.0.8", "read custom ip");
    expect(configuration.port() == 65535, "read maximum valid port");
    std::remove(validPath.c_str());

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
