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

add_executable(my_logger_test
    test/MyLoggerTest.cc
)

target_link_libraries(my_logger_test
    PRIVATE
        smart_home_logger
)

add_test(NAME my_logger_test COMMAND my_logger_test)
