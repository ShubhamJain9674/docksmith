cmake_minimum_required(VERSION 3.16)
project(docksmith)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build type = Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# OpenMP (since you're using it)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found")
endif()

# openSSL for SHA256 hashing (EVP API - modern OpenSSL 3.0+)
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL not found.Install it")
endif()

# Link OpenSSL



# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable
add_executable(docksmith ${SOURCES})

# Include directories
target_include_directories(docksmith PRIVATE include src/public)

# Link OpenMP
if(OpenMP_CXX_FOUND)
    target_link_libraries(docksmith OpenMP::OpenMP_CXX)
endif()

target_link_libraries(docksmith OpenSSL::Crypto)