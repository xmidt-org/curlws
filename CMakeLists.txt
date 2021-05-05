# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.13)

project(curlws VERSION 1.0.2)

math(EXPR curlws_BITWISE_VERSION "${curlws_VERSION_MAJOR} << 16 | ${curlws_VERSION_MINOR} << 8 | ${curlws_VERSION_PATCH}" OUTPUT_FORMAT HEXADECIMAL)

add_definitions(-std=c99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wpedantic")

if (USE_OPENSSL_SHA)
    # Search for OpenSSL
    find_package(PkgConfig REQUIRED)
    pkg_search_module(OPENSSL REQUIRED openssl)

    if( OPENSSL_FOUND )
        include_directories(${OPENSSL_INCLUDE_DIRS})
        message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
    else()
        # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
    endif()
endif()


#
# Find the required libcurl library and headers
#
include(FindPkgConfig)
pkg_check_modules(CURL libcurl REQUIRED libcurl>=7.50.2)
include_directories(
  SYSTEM ${CURL_INCLUDE_DIRS}
)

include(CTest)


add_library(curlws SHARED "")

add_subdirectory(src)
add_subdirectory(examples)

if (BUILD_TESTING)
    #
    # Find the required cunit library and headers
    #
    find_package(PkgConfig REQUIRED)
    pkg_search_module(CUNIT REQUIRED cunit)

    if( CUNIT )
        include_directories(${CUNIT_INCLUDE_DIRS})
        message(STATUS "Using CUnit ${CUNIT_VERSION}")
    else()
        # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
    endif()

  add_subdirectory(tests)
endif (BUILD_TESTING)

add_custom_target(license
                  COMMAND reuse lint
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
