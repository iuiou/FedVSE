cmake_minimum_required(VERSION 3.12)
project(FedVS VERSION 1.0 DESCRIPTION "An efficient and secure algorithm for federated vector similarity search with filters" LANGUAGES CXX)  

set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(USE_SGX STREQUAL "ON")
    message(STATUS "Compiling SGX MPC:")

    set(Enclave_Test_Key "${CMAKE_BINARY_DIR}/Enclave_private_test.pem")  
    if(NOT EXISTS ${Enclave_Test_Key})  
        message(STATUS "There is no enclave test key<${Enclave_Test_Key}>.")  
        message(STATUS "The project will generate a key<${Enclave_Test_Key}> for test.")  
        execute_process(  
            COMMAND openssl genrsa -out ${Enclave_Test_Key} -3 3072  
            RESULT_VARIABLE openssl_result  
            OUTPUT_QUIET  
            ERROR_QUIET  
        )  
        if(NOT openssl_result EQUAL 0)  
            message(FATAL_ERROR "Failed to generate RSA key.")  
        endif()  
    endif()

    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/sgx/cmake")
    find_package(SGX REQUIRED)

    message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

    set(EDL_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/sgx/Enclave)
    set(E_SRCS ${CMAKE_SOURCE_DIR}/sgx/Enclave/Enclave.cpp)

    # if(SGX_HW AND SGX_MODE STREQUAL "Release")
        set(LDS ${CMAKE_SOURCE_DIR}/sgx/Enclave/Enclave.lds)
    # else()
    #     set(LDS ${CMAKE_SOURCE_DIR}/sgx/Enclave/Enclave_debug.lds)
    # endif()

    add_enclave_library(enclave 
        SRCS ${E_SRCS} 
        EDL ${EDL_SEARCH_PATHS}/Enclave.edl 
        EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} 
        LDSCRIPT ${LDS})

    message(STATUS "Enclave_Test_Key=${Enclave_Test_Key}")
    enclave_sign(enclave 
        KEY ${Enclave_Test_Key} 
        CONFIG ${EDL_SEARCH_PATHS}/Enclave.config.xml)

    set(SRCS ${CMAKE_SOURCE_DIR}/sgx/mpc/SgxMpc.cpp)
    add_untrusted_library(SgxMpc 
        STATIC 
        SRCS ${SRCS} 
        EDL ${EDL_SEARCH_PATHS}/Enclave.edl
        EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Boost 1.85.0 COMPONENTS program_options system REQUIRED)
If (NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find Boost version 1.85.0 or higher with the require components")  
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif ()

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

# enable ccache if possible
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    message(STATUS "using ccache")
endif(CCACHE_FOUND)

include(DefineOptions)

set_option_category("Build")

define_option(MILVUS_BUILD_TEST "Build with testing" OFF)
define_option(MILVUS_BUILD_COVERAGE "Build with coverage" OFF)
define_option_string(MILVUS_SDK_VERSION
    "Version for sdk"
    "2.0.0")
define_option_string(MILVUS_SDK_RELEASE
    "Release number for sdk"
    "1")

set_option_category("Thirdparty")
# use thirdparty from:
#  auto: Find -> Fetch_Content_Then_Find
#  package: Find
#  module: Fetch_Content_Then_Find
define_option_string(MILVUS_WITH_GRPC           "Using gRPC from"       "module" "package" "package")
define_option_string(MILVUS_WITH_ZLIB           "Using Zlib from"       "module" "package" "module")
define_option_string(MILVUS_WITH_NLOHMANN_JSON  "nlohmann json from"    "module" "package" "module")
define_option_string(MILVUS_WITH_GTEST          "Using GTest from"      "module" "package" "module")

include(Grpc)
include(ThirdPartyPackages)
include(MilvusProtoGen)

add_subdirectory(milvus)

include_directories(${CMAKE_SOURCE_DIR}/milvus/include)

get_filename_component(brokersilo_proto "./proto/BrokerSilo.proto" ABSOLUTE)
get_filename_component(brokersilo_proto_path "${brokersilo_proto}" PATH)
get_filename_component(userbroker_proto "./proto/UserBroker.proto" ABSOLUTE)
get_filename_component(userbroker_proto_path "${userbroker_proto}" PATH)
#set(CMAKE_EXE_LINKER_FLAGS "-static") ## enable this, program has segmentation fault

#compile proto file
set(brokersilo_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/BrokerSilo.pb.cc")
set(brokersilo_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/BrokerSilo.pb.h")
set(brokersilo_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/BrokerSilo.grpc.pb.cc")
set(brokersilo_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/BrokerSilo.grpc.pb.h")

set(userbroker_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/UserBroker.pb.cc")
set(userbroker_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/UserBroker.pb.h")
set(userbroker_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/UserBroker.grpc.pb.cc")
set(userbroker_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/UserBroker.grpc.pb.h")

add_custom_command(
    OUTPUT "${brokersilo_proto_srcs}" "${brokersilo_proto_hdrs}" "${brokersilo_grpc_srcs}" "${brokersilo_grpc_hdrs}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${brokersilo_proto_path}"
        --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
        "${brokersilo_proto}"
    DEPENDS "${brokersilo_proto}")

add_custom_command(
    OUTPUT "${userbroker_proto_srcs}" "${userbroker_proto_hdrs}" "${userbroker_grpc_srcs}" "${userbroker_grpc_hdrs}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${userbroker_proto_path}"
        --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
        "${userbroker_proto}"
    DEPENDS "${userbroker_proto}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
add_library(grpc_proto
    ${brokersilo_grpc_srcs}
    ${brokersilo_grpc_hdrs}
    ${brokersilo_proto_srcs}
    ${brokersilo_proto_hdrs}
    ${userbroker_grpc_srcs}
    ${userbroker_grpc_hdrs}
    ${userbroker_proto_srcs}
    ${userbroker_proto_hdrs})

target_link_libraries(grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

add_library(File_IO
    "./utils/File_IO.h"
    "./utils/File_IO.cpp")

add_library(AES
    "./crypto/AES.h"
    "./crypto/AES.cpp")

add_library(pgmindexlib INTERFACE)
target_include_directories(pgmindexlib INTERFACE pgm)

set(_target "silo")
message(STATUS "Compiling executable ${_target}")
add_executable(${_target} "./Silo.cpp")
target_link_libraries(${_target} PRIVATE
    # asan
    milvus_sdk
    grpc_proto
    File_IO
    AES
    pgmindexlib
    "pthread"
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
target_link_libraries(${_target} PRIVATE Boost::program_options Boost::system)

set(_target "broker")
message(STATUS "Compiling executable ${_target}")
if(USE_SGX STREQUAL "ON")
    set(SRCS "${CMAKE_SOURCE_DIR}/Broker.cpp")
    message(STATUS "SRCS=${SRCS}")
    message(STATUS "EDL=${CMAKE_SOURCE_DIR}/sgx/Enclave/Enclave.edl")
    add_untrusted_executable(${_target} 
        SRCS ${SRCS}
        EDL ${EDL_SEARCH_PATHS}/Enclave.edl
        EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
    target_compile_definitions(${_target} PRIVATE USE_INTEL_SGX)
    # if(LOCAL_DEBUG STREQUAL "ON")
    #     target_compile_definitions(${_target} PRIVATE LOCAL_DEBUG)
    # endif()
    add_dependencies(${_target} enclave-sign)
    target_link_libraries(${_target}
        milvus_sdk
        grpc_proto
        File_IO
        AES
        SgxMpc
        "pthread"
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
    target_link_libraries(${_target} Boost::program_options Boost::system)
else()
    add_executable(${_target} "./Broker.cpp")
    target_compile_definitions(${_target} PRIVATE USE_TRUSTED_BROKER)
    target_link_libraries(${_target} PRIVATE
        # asan
        milvus_sdk
        grpc_proto
        File_IO
        AES
        "pthread"
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
    target_link_libraries(${_target} PRIVATE Boost::program_options Boost::system)
endif()

set(_target "user")
message(STATUS "Compiling executable ${_target}")
add_executable(${_target} "./User.cpp")
target_link_libraries(${_target} PRIVATE
    # asan
    grpc_proto
    File_IO
    AES
    "pthread"
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
target_link_libraries(${_target} PRIVATE Boost::program_options Boost::system)




