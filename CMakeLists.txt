cmake_minimum_required(VERSION 3.10)

project(SmartHomeMonitoringSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The server uses std::mutex and log4cpp's thread-safe Category implementation.
find_package(Threads REQUIRED)

# Production builds must link the requested log4cpp dependency instead of
# silently falling back to an alternative logger.
find_path(LOG4CPP_INCLUDE_DIR NAMES log4cpp/Category.hh)
find_library(LOG4CPP_LIBRARY NAMES log4cpp)
if(NOT LOG4CPP_INCLUDE_DIR OR NOT LOG4CPP_LIBRARY)
    message(FATAL_ERROR
        "log4cpp is required. Install it with: sudo apt-get install liblog4cpp5-dev")
endif()

# UserDao uses the prepared-statement API from the MySQL-compatible client
# library. Keep this dependency explicit so cloud builds fail with a useful
# installation hint instead of silently using unsafe SQL concatenation.
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

# Configuration parsing is kept as a small reusable library for the server and
# its unit test.
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

# ThreadPool owns the worker threads used by the future Reactor business
# dispatch path and is independently testable without network dependencies.
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

# Reactor owns the Linux epoll/eventfd event loop. It has no third-party
# dependency, so TCP and protocol modules can build on top of this target.
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

# TCP communication owns the listening socket and accepted connections while
# Reactor remains responsible for readiness notification and dispatch.
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

# Protocol parses the Type/Length/Value stream and dispatches decoded messages
# without coupling the transport layer to login or camera business logic.
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

# Database contains the MySQL RAII client and the first t_user DAO. Password
# hashing and user-session business rules remain in the service layer.
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

# MyLogger contains the only direct dependency on log4cpp.
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

# The bootstrap executable currently validates startup configuration and
# initializes logging; later server modules will be linked here.
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

# Unit tests are intentionally dependency-light except for the logger test,
# which exercises the same log4cpp library used by the production executable.
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
