#include "Configuration.hpp"
#include "MyLogger.hpp"
#include "ProtocolTcpServer.hpp"
#include "Reactor.hpp"
#include "ThreadPool.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // 第一个参数是可选的，因此从项目根目录启动服务端时可以直接使用文档
    // 中的默认路径 ./conf/server.conf。
    const std::string path = argc > 1 ? argv[1] : "./conf/server.conf";
    shms::Configuration& configuration = shms::Configuration::instance();

    if (!configuration.load(path)) {
        std::cerr << "Failed to load configuration: "
                  << configuration.lastError() << std::endl;
        return EXIT_FAILURE;
    }

    // 在加载配置后立即初始化日志。后续服务端模块都可以通过同一个单例
    // 记录操作。
    shms::MyLogger& logger = shms::MyLogger::instance();
    if (!logger.initialize(configuration.logFile())) {
        std::cerr << "Failed to initialize logger: " << logger.lastError()
                  << std::endl;
        return EXIT_FAILURE;
    }

    // 根据服务端配置创建工作线程池。线程池负责执行网络事件之外的业务任务，
    // 并在服务退出时完成已接收任务的收尾。
    shms::ThreadPool threadPool(configuration.threadNum(),
                                configuration.taskNum());
    if (!threadPool.start()) {
        logger.error("thread pool initialization failed");
        std::cerr << "Failed to initialize thread pool" << std::endl;
        return EXIT_FAILURE;
    }
    logger.info("thread pool started");

    // 在启动阶段初始化网络事件循环，并由 ProtocolTcpServer 注册监听套接字；
    // 随后 run() 将在服务生命周期内持续处理网络事件。
    shms::Reactor reactor;
    if (!reactor.initialize()) {
        logger.error("reactor initialization failed: " + reactor.lastError());
        std::cerr << "Failed to initialize reactor: " << reactor.lastError()
                  << std::endl;
        threadPool.stop();
        logger.shutdown();
        return EXIT_FAILURE;
    }
    logger.info("reactor initialized");

    // ProtocolTcpServer 负责监听套接字、协议解析和每条连接的会话生命周期；
    // 所有就绪通知仍由单个 Reactor 线程处理。
    shms::ProtocolTcpServer protocolServer(reactor,
                                           configuration.ip(),
                                           configuration.port());
    protocolServer.setConnectionHandler(
        [&logger](shms::TcpConnection& connection) {
            logger.info("tcp client connected: fd=" +
                        std::to_string(connection.fd()));
        });
    protocolServer.setCloseHandler([&logger](shms::TcpConnection&) {
        logger.info("tcp client disconnected");
    });
    protocolServer.setProtocolErrorHandler(
        [&logger](shms::TcpConnection& connection,
                  const std::string& message) {
            logger.warn("protocol error: fd=" +
                        std::to_string(connection.fd()) + ", error=" + message);
        });
    if (!protocolServer.start()) {
        logger.error("TCP server initialization failed: " +
                     protocolServer.lastError());
        std::cerr << "Failed to initialize TCP server: "
                  << protocolServer.lastError() << std::endl;
        reactor.shutdown();
        threadPool.stop();
        logger.shutdown();
        return EXIT_FAILURE;
    }
    logger.info("TCP server listening on " + configuration.ip() + ":" +
                std::to_string(protocolServer.port()));
    logger.info("server configuration loaded");

    std::cout << "Configuration loaded successfully" << std::endl
              << "ip=" << configuration.ip() << std::endl
              << "port=" << configuration.port() << std::endl
              << "thread_num=" << configuration.threadNum() << std::endl
              << "task_num=" << configuration.taskNum() << std::endl
              << "video_path=" << configuration.videoPath() << std::endl
              << "log_file=" << configuration.logFile() << std::endl;
    logger.info("server bootstrap completed");
    // 协议层已经接管 TCP 收包和心跳响应；后续业务模块通过会话配置器注册
    // 用户、摄像头等消息处理器。
    if (!reactor.run()) {
        logger.error("reactor stopped with error: " + reactor.lastError());
    }
    protocolServer.stop();
    reactor.shutdown();
    threadPool.stop();
    logger.info("thread pool stopped");
    logger.shutdown();
    return EXIT_SUCCESS;
}
