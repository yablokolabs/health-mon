cmake_minimum_required(VERSION 3.25)
project(health-mon VERSION 0.1.0 LANGUAGES CXX
    DESCRIPTION "System health monitoring, TMR voting, and degraded mode management for avionics research")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_library(health INTERFACE)
target_include_directories(health INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

option(HEALTH_BUILD_TESTS "Build tests" ON)
if(HEALTH_BUILD_TESTS)
    enable_testing()
    add_executable(test_health tests/test_health.cpp)
    target_link_libraries(test_health PRIVATE health)
    add_test(NAME test_health COMMAND test_health)
endif()
