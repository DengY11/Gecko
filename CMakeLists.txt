cmake_minimum_required(VERSION 3.16)
project(GeckoWebFramework VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
    find_package(nlohmann_json QUIET)
endif()

option(GECKO_BUILD_EXAMPLES "Build example executables" ON)
option(GECKO_BUILD_TESTS "Build test executables" ON)
option(GECKO_ENABLE_GRPC "Enable optional gRPC RPC server support" OFF)

set(GECKO_PUBLIC_HEADERS
    src/http/context.hpp
    src/http/engine.hpp
    src/http/fast_http_parser.hpp
    src/http/http_request.hpp
    src/http/http_response.hpp
    src/http/io_thread_pool.hpp
    src/http/middlewares.hpp
    src/http/request_pool.hpp
    src/http/router.hpp
    src/http/server_config.hpp
    src/http/server.hpp
    src/http/thread_pool.hpp
    src/logger/logger.hpp
    src/rpc/rpc_server.hpp
    src/tracing/tracer.hpp
)

set(GECKO_SOURCES
    src/http/context.cpp
    src/http/engine.cpp
    src/http/fast_http_parser.cpp
    src/http/http_request.cpp
    src/http/http_response.cpp
    src/http/io_thread_pool.cpp
    src/http/router.cpp
    src/http/server.cpp
    src/http/thread_pool.cpp
    src/logger/logger.cpp
    src/rpc/rpc_server.cpp
    src/tracing/tracer.cpp
)

add_library(gecko STATIC ${GECKO_SOURCES})
add_library(Gecko::gecko ALIAS gecko)

target_include_directories(gecko
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/http>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/logger>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/rpc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/tracing>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/gecko>
)
target_link_libraries(gecko PUBLIC Threads::Threads)
target_compile_options(gecko PRIVATE -Wall -Wextra)

if (nlohmann_json_FOUND)
    target_link_libraries(gecko PUBLIC nlohmann_json::nlohmann_json)
else()
    message(STATUS "nlohmann_json not found; assuming <nlohmann/json.hpp> is available in your include path")
endif()

if (GECKO_ENABLE_GRPC)
    find_package(gRPC CONFIG QUIET)
    if (NOT gRPC_FOUND)
        message(FATAL_ERROR "GECKO_ENABLE_GRPC is ON but gRPC package was not found. Install gRPC C++ or disable the option.")
    endif()
    target_link_libraries(gecko PUBLIC gRPC::grpc++ gRPC::grpc++_reflection)
    target_compile_definitions(gecko PUBLIC GECKO_ENABLE_GRPC)
endif()

if (GECKO_BUILD_EXAMPLES)
    add_executable(example_gin_style examples/gin_style.cpp)
    target_link_libraries(example_gin_style PRIVATE gecko)

    if (GECKO_ENABLE_GRPC)
        if (gRPC_DIR)
            list(APPEND CMAKE_PREFIX_PATH ${gRPC_DIR}/..)
        endif()

        find_package(Protobuf CONFIG QUIET HINTS ${gRPC_DIR}/../protobuf ${gRPC_DIR}/..)
        if (NOT Protobuf_FOUND)
            find_package(Protobuf REQUIRED)
        endif()

        set(GREETER_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/examples/proto/greeter.proto)
        set(GREETER_PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/greeter.pb.cc)
        set(GREETER_PROTO_HDR ${CMAKE_CURRENT_BINARY_DIR}/greeter.pb.h)
        set(GREETER_GRPC_SRC ${CMAKE_CURRENT_BINARY_DIR}/greeter.grpc.pb.cc)
        set(GREETER_GRPC_HDR ${CMAKE_CURRENT_BINARY_DIR}/greeter.grpc.pb.h)

        if (TARGET protobuf::protoc)
            set(GREETER_PROTOC $<TARGET_FILE:protobuf::protoc>)
        else()
            set(GREETER_PROTOC ${Protobuf_PROTOC_EXECUTABLE})
        endif()

        add_custom_command(
            OUTPUT ${GREETER_PROTO_SRC} ${GREETER_PROTO_HDR} ${GREETER_GRPC_SRC} ${GREETER_GRPC_HDR}
            COMMAND ${GREETER_PROTOC}
                --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/examples/proto
                --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                ${GREETER_PROTO}
            DEPENDS ${GREETER_PROTO}
            COMMENT "Generating gRPC sources for greeter.proto"
            VERBATIM
        )

        add_library(gecko_greeter_proto STATIC
            ${GREETER_PROTO_SRC}
            ${GREETER_GRPC_SRC}
        )
        target_include_directories(gecko_greeter_proto PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}
            ${Protobuf_INCLUDE_DIRS}
        )
        target_compile_definitions(gecko_greeter_proto PRIVATE NDEBUG)
        target_link_libraries(gecko_greeter_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)

        add_executable(example_http_rpc_middleware examples/http_rpc_middleware.cpp)
        target_link_libraries(example_http_rpc_middleware PRIVATE
            gecko
            gecko_greeter_proto
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
        )
        target_include_directories(example_http_rpc_middleware PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endif()

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
    add_gecko_test(cooperative_thread_pool_tests tests/performance/test_thread_pool_cooperative.cpp)
endif()

install(TARGETS gecko
    EXPORT GeckoTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY src/http DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gecko FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/logger DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gecko FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/rpc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gecko FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/tracing DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gecko FILES_MATCHING PATTERN "*.hpp")

install(EXPORT GeckoTargets
    FILE GeckoTargets.cmake
    NAMESPACE Gecko::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Gecko
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GeckoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/GeckoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Gecko
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/GeckoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/GeckoConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/GeckoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Gecko
)
