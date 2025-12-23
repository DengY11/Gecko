cmake_minimum_required(VERSION 3.16)
project(GeckoWebFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
    find_package(nlohmann_json QUIET)
endif()

set(GECKO_SOURCES
    src/context.cpp
    src/engine.cpp
    src/fast_http_parser.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/io_thread_pool.cpp
    src/logger.cpp
    src/router.cpp
    src/server.cpp
    src/thread_pool.cpp
)

add_library(gecko STATIC ${GECKO_SOURCES})
target_include_directories(gecko PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gecko PUBLIC Threads::Threads)
target_compile_options(gecko PRIVATE -Wall -Wextra)

if (nlohmann_json_FOUND)
    target_link_libraries(gecko PUBLIC nlohmann_json::nlohmann_json)
else()
    message(STATUS "nlohmann_json not found; assuming <nlohmann/json.hpp> is available in your include path")
endif()

add_executable(example_gin_style example_gin_style.cpp)
target_link_libraries(example_gin_style PRIVATE gecko)

option(GECKO_BUILD_TESTS "Build test executables" ON)
if (GECKO_BUILD_TESTS)
    enable_testing()

    function(add_gecko_test target source_file)
        add_executable(${target} ${source_file})
        target_link_libraries(${target} PRIVATE gecko)
        add_test(NAME ${target} COMMAND ${target})
    endfunction()

    add_gecko_test(router_tests tests/router/test_router.cpp)
    add_gecko_test(http_basic_tests tests/http/test_basic_requests.cpp)
    add_gecko_test(http_constructors_tests tests/http/test_constructors.cpp)
    add_gecko_test(http_error_tests tests/http/test_error_cases.cpp)
    add_gecko_test(http_special_case_tests tests/http/test_special_cases.cpp)
    add_gecko_test(performance_tests tests/performance/performance_test.cpp)
endif()
