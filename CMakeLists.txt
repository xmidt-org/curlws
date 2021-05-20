# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

project(curlws VERSION 1.0.2)

include(BitwiseVersion)
include(CTest)
include(LicenseLinterTarget)
include(Coverage)

add_definitions(-std=c99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wpedantic")

# Search for OpenSSL
find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL openssl)

if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    if( USE_OPENSSL_SHA )
        message(FATAL_ERROR "OpenSSL required but is not availeble.")
    endif()
    message(STATUS "Using Internal SHA1 version.")
endif()


#
# Find the required libcurl library and headers
#
include(FindPkgConfig)
pkg_check_modules(CURL libcurl REQUIRED libcurl>=7.50.2)
include_directories(SYSTEM ${CURL_INCLUDE_DIRS})


add_library(curlws SHARED "")

add_subdirectory(src)
add_subdirectory(examples)

if (BUILD_TESTING)
    include(FindCunit)
    add_subdirectory(tests)
endif (BUILD_TESTING)
