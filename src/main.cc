#include "Configuration.hpp"
#include "MyLogger.hpp"
#include "ThreadPool.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // The first argument is optional so the documented default
    // ./conf/server.conf works when the server is launched from the project
    // root.
    const std::string path = argc > 1 ? argv[1] : "./conf/server.conf";
    shms::Configuration& configuration = shms::Configuration::instance();

    if (!configuration.load(path)) {
        std::cerr << "Failed to load configuration: "
                  << configuration.lastError() << std::endl;
        return EXIT_FAILURE;
    }

    // Initialize logging immediately after configuration. Any later server
    // module can now record operations through the same singleton instance.
    shms::MyLogger& logger = shms::MyLogger::instance();
    if (!logger.initialize(configuration.logFile())) {
        std::cerr << "Failed to initialize logger: " << logger.lastError()
                  << std::endl;
        return EXIT_FAILURE;
    }

    // Build the worker pool from the server configuration. The current
    // bootstrap exits after proving startup/cleanup; Reactor will submit real
    // client tasks here in the next framework increment.
    shms::ThreadPool threadPool(configuration.threadNum(),
                                configuration.taskNum());
    if (!threadPool.start()) {
        logger.error("thread pool initialization failed");
        std::cerr << "Failed to initialize thread pool" << std::endl;
        return EXIT_FAILURE;
    }
    logger.info("thread pool started");
    logger.info("server configuration loaded");

    std::cout << "Configuration loaded successfully" << std::endl
              << "ip=" << configuration.ip() << std::endl
              << "port=" << configuration.port() << std::endl
              << "thread_num=" << configuration.threadNum() << std::endl
              << "task_num=" << configuration.taskNum() << std::endl
              << "video_path=" << configuration.videoPath() << std::endl
              << "log_file=" << configuration.logFile() << std::endl;
    logger.info("server bootstrap completed");
    threadPool.stop();
    logger.info("thread pool stopped");
    logger.shutdown();
    return EXIT_SUCCESS;
}
