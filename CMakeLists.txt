cmake_minimum_required(VERSION 3.10)

project(SmartHomeMonitoringSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

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

add_executable(SmartHomeServer
    src/main.cc
)

target_link_libraries(SmartHomeServer
    PRIVATE
        smart_home_configuration
)

enable_testing()

add_executable(configuration_test
    test/ConfigurationTest.cc
)

target_link_libraries(configuration_test
    PRIVATE
        smart_home_configuration
)

add_test(NAME configuration_test COMMAND configuration_test)
