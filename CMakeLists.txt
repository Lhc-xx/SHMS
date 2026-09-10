cmake_minimum_required(VERSION 3.10)

project(SmartHomeMonitoringSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 服务端使用 std::mutex 和 log4cpp 提供的线程安全 Category 实现。
find_package(Threads REQUIRED)

# 生产构建必须链接指定的 log4cpp 依赖，不能静默退回其他日志库。
find_path(LOG4CPP_INCLUDE_DIR NAMES log4cpp/Category.hh)
find_library(LOG4CPP_LIBRARY NAMES log4cpp)
if(NOT LOG4CPP_INCLUDE_DIR OR NOT LOG4CPP_LIBRARY)
    message(FATAL_ERROR
        "log4cpp is required. Install it with: sudo apt-get install liblog4cpp5-dev")
endif()

# UserDao 使用兼容 MySQL 的客户端库提供的预处理语句 API。
# 显式保留该依赖，使云服务器构建缺少安装包时能给出明确提示，避免静默使用不安全的 SQL 拼接。
find_path(MYSQL_INCLUDE_DIR NAMES mysql/mysql.h mysql.h)
find_library(MYSQL_LIBRARY NAMES mysqlclient mariadb)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR
        "MySQL client is required. Install it with: sudo apt-get install default-libmysqlclient-dev")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 配置解析被拆分为可复用的小型库，供服务端和单元测试共同使用。
add_library(smart_home_configuration
    src/Configuration.cc
)

target_include_directories(smart_home_configuration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_configuration
    PUBLIC
        Threads::Threads
)

# ThreadPool 管理 Reactor 业务分发路径使用的工作线程，且无需网络依赖即可独立测试。
add_library(smart_home_thread_pool
    src/ThreadPool.cc
)

target_include_directories(smart_home_thread_pool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_thread_pool
    PUBLIC
        Threads::Threads
)

# Reactor 管理 Linux epoll/eventfd 事件循环，不依赖第三方库，TCP 和协议模块可在其上构建。
add_library(smart_home_reactor
    src/Reactor.cc
)

target_include_directories(smart_home_reactor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_reactor
    PUBLIC
        Threads::Threads
)

# TCP 通信模块管理监听套接字和已接收连接，Reactor 负责就绪通知与事件分发。
add_library(smart_home_tcp
    src/TcpConnection.cc
    src/TcpServer.cc
)

target_include_directories(smart_home_tcp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_tcp
    PUBLIC
        smart_home_reactor
        Threads::Threads
)

# Protocol 解析 Type/Length/Value 数据流并分发解码后的消息，不与登录或摄像头业务逻辑耦合。
add_library(smart_home_protocol
    src/Protocol.cc
    src/MessageDispatcher.cc
)

target_include_directories(smart_home_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(smart_home_protocol
    PUBLIC
        Threads::Threads
)

# Database 包含 MySQL RAII 客户端和首个 t_user DAO；密码哈希及用户会话规则仍由服务层负责。
add_library(smart_home_database
    src/MySqlClient.cc
    src/UserDao.cc
)

target_include_directories(smart_home_database
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${MYSQL_INCLUDE_DIR}
)

target_link_libraries(smart_home_database
    PUBLIC
        ${MYSQL_LIBRARY}
        Threads::Threads
)

# MyLogger 是唯一直接依赖 log4cpp 的模块。
add_library(smart_home_logger
    src/MyLogger.cc
)

target_include_directories(smart_home_logger
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LOG4CPP_INCLUDE_DIR}
)

target_link_libraries(smart_home_logger
    PUBLIC
        ${LOG4CPP_LIBRARY}
        Threads::Threads
)

# 启动程序负责校验启动配置并初始化日志，后续服务模块将在此处继续链接。
add_executable(SmartHomeServer
    src/main.cc
)

target_link_libraries(SmartHomeServer
    PRIVATE
        smart_home_configuration
        smart_home_logger
        smart_home_thread_pool
        smart_home_reactor
        smart_home_tcp
        smart_home_protocol
)

enable_testing()

# 单元测试尽量减少依赖；日志测试例外，它会验证生产程序使用的同一套 log4cpp 库。
add_executable(configuration_test
    test/ConfigurationTest.cc
)

target_link_libraries(configuration_test
    PRIVATE
        smart_home_configuration
)

add_test(NAME configuration_test COMMAND configuration_test)

add_executable(thread_pool_test
    test/ThreadPoolTest.cc
)

target_link_libraries(thread_pool_test
    PRIVATE
        smart_home_thread_pool
)

add_test(NAME thread_pool_test COMMAND thread_pool_test)

add_executable(reactor_test
    test/ReactorTest.cc
)

target_link_libraries(reactor_test
    PRIVATE
        smart_home_reactor
)

add_test(NAME reactor_test COMMAND reactor_test)

add_executable(tcp_server_test
    test/TcpServerTest.cc
)

target_link_libraries(tcp_server_test
    PRIVATE
        smart_home_tcp
)

add_test(NAME tcp_server_test COMMAND tcp_server_test)

add_executable(protocol_test
    test/ProtocolTest.cc
)

target_link_libraries(protocol_test
    PRIVATE
        smart_home_protocol
)

add_test(NAME protocol_test COMMAND protocol_test)

add_executable(database_test
    test/DatabaseTest.cc
)

target_link_libraries(database_test
    PRIVATE
        smart_home_database
)

add_test(NAME database_test COMMAND database_test)

add_executable(my_logger_test
    test/MyLoggerTest.cc
)

target_link_libraries(my_logger_test
    PRIVATE
        smart_home_logger
)

add_test(NAME my_logger_test COMMAND my_logger_test)
