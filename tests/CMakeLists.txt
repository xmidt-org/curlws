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

if(NOT DISABLE_VALGRIND)
set (MEMORY_CHECK valgrind --leak-check=full --show-reachable=yes  --error-exitcode=1 -v)
endif ()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -g -fprofile-arcs -ftest-coverage -O0 -finstrument-functions")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W  -g -fprofile-arcs -ftest-coverage -O0 -finstrument-functions")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -O0 -finstrument-functions")


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Memory COMMAND ${MEMORY_CHECK} ./test_memory)

add_executable(test_memory test_memory.c ../src/memory.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_memory gcov)
endif()
target_link_libraries (test_memory cunit)
target_link_libraries (test_memory)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Utils COMMAND ${MEMORY_CHECK} ./test_utils)

add_executable(test_utils test_utils.c ../src/utils.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_utils gcov)
endif()
target_link_libraries (test_utils cunit)
target_link_libraries (test_utils ${OPENSSL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Curlws COMMAND ${MEMORY_CHECK} ./test_curlws)

add_executable(test_curlws test_curlws.c ../src/utils.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_curlws gcov)
endif()
target_link_libraries (test_curlws cunit)
target_link_libraries (test_curlws ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_MockedCurlws COMMAND ${MEMORY_CHECK} ./test_mocked_curlws)

add_executable(test_mocked_curlws test_mocked_curlws.c ../src/utils.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_mocked_curlws gcov)
endif()
target_link_libraries (test_mocked_curlws cunit)
target_link_libraries (test_mocked_curlws ${OPENSSL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Autobahn COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 1 40)

add_executable(test_autobahn autobahn-testee.c ../src/utils.c ../src/curlws.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_autobahn gcov)
endif()
target_link_libraries (test_autobahn ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Frame COMMAND ${MEMORY_CHECK} ./test_frame)

add_executable(test_frame test_frame.c ../src/frame.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_frame gcov)
endif()
target_link_libraries (test_frame cunit)
target_link_libraries (test_frame ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES})
# ---------------------------------------------------------------------------- #


add_custom_target(coverage
                  COMMAND lcov -q --capture
                          --directory ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/
                          --output-file coverage.info
                  COMMAND genhtml coverage.info
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
