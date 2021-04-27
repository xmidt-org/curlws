#   Copyright 2021 Comcast Cable Communications Management, LLC
#
#   Permission is hereby granted, free of charge, to any person obtaining
#   a copy of this software and associated documentation files (the
#   "Software"), to deal in the Software without restriction, including
#   without limitation the rights to use, copy, modify, merge, publish,
#   distribute, sublicense, and/or sell copies of the Software, and to
#   permit persons to whom the Software is furnished to do so, subject to
#   the following conditions:
#
#   The above copyright notice and this permission notice shall be
#   included in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
#   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
#   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
#   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
#   https://opensource.org/licenses/MIT

project(curlws)
cmake_minimum_required(VERSION 3.13)

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
pkg_check_modules(CURL libcurl REQUIRED)
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
