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
target_link_libraries (test_utils)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_SHA1 COMMAND ${MEMORY_CHECK} ./test_sha1)

add_executable(test_sha1 test_sha1.c ../src/sha1_openssl.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_sha1 gcov)
endif()
target_link_libraries (test_sha1 cunit)
target_link_libraries (test_sha1 ${OPENSSL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Frame COMMAND ${MEMORY_CHECK} ./test_frame)

add_executable(test_frame test_frame.c ../src/frame.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_frame gcov)
endif()
target_link_libraries (test_frame cunit)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Frame_Senders COMMAND ${MEMORY_CHECK} ./test_frame_senders)

add_executable(test_frame_senders test_frame_senders.c ../src/frame_senders.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_frame_senders gcov)
endif()
target_link_libraries (test_frame_senders cunit)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Data_Block_Sender COMMAND ${MEMORY_CHECK} ./test_data_block_sender)

add_executable(test_data_block_sender test_data_block_sender.c ../src/data_block_sender.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_data_block_sender gcov)
endif()
target_link_libraries (test_data_block_sender cunit)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Handlers COMMAND ${MEMORY_CHECK} ./test_handlers)

add_executable(test_handlers test_handlers.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_handlers gcov)
endif()
target_link_libraries (test_handlers cunit)
target_link_libraries (test_handlers ${CURL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
#add_test(NAME Test_Receive COMMAND ${MEMORY_CHECK} ./test_receive)

add_executable(test_receive test_receive.c
                           ../src/frame.c
                           ../src/utf8.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_receive gcov)
endif()
target_link_libraries (test_receive cunit)
target_link_libraries (test_receive)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
#add_test(NAME Test_Receive COMMAND ${MEMORY_CHECK} ./test_receive)

add_executable(test_autobahn_27 test_autobahn_27.c
                           ../src/frame.c
                           ../src/utf8.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_autobahn_27 gcov)
endif()
target_link_libraries (test_autobahn_27 cunit)
target_link_libraries (test_autobahn_27)
# ---------------------------------------------------------------------------- #



# ---------------------------------------------------------------------------- #
add_test(NAME Test_Send COMMAND ${MEMORY_CHECK} ./test_send)

add_executable(test_send test_send.c ../src/frame.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_send gcov)
endif()
target_link_libraries (test_send cunit)
target_link_libraries (test_send)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Header COMMAND ${MEMORY_CHECK} ./test_header)

add_executable(test_header test_header.c ../src/utils.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_header gcov)
endif()
target_link_libraries (test_header cunit)
target_link_libraries (test_header)
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_test(NAME Test_Utf8 COMMAND ${MEMORY_CHECK} ./test_utf8)

add_executable(test_utf8 test_utf8.c ../src/utf8.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_utf8 gcov)
endif()
target_link_libraries (test_utf8 cunit)
target_link_libraries (test_utf8)
# ---------------------------------------------------------------------------- #



# ---------------------------------------------------------------------------- #
add_test(NAME Test_Curlws COMMAND ${MEMORY_CHECK} ./test_curlws)

add_executable(test_curlws test_curlws.c
                           ../src/data_block_sender.c
                           ../src/frame_senders.c
                           ../src/frame.c
                           ../src/handlers.c
                           ../src/header.c
                           ../src/memory.c
                           ../src/receive.c
                           ../src/send.c
                           ../src/sha1_openssl.c
                           ../src/utf8.c
                           ../src/utils.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_curlws gcov)
endif()
target_link_libraries (test_curlws cunit)
target_link_libraries (test_curlws ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
#add_test(NAME Test_MockedCurlws COMMAND ${MEMORY_CHECK} ./test_mocked_curlws)
#
#add_executable(test_mocked_curlws test_mocked_curlws.c ../src/utils.c ../src/memory.c)
#
#if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
#target_link_libraries (test_mocked_curlws gcov)
#endif()
#target_link_libraries (test_mocked_curlws cunit)
#target_link_libraries (test_mocked_curlws ${OPENSSL_LIBRARIES})
# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
#add_test(NAME Test_Autobahn___1__10  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001   1  10)
#add_test(NAME Test_Autobahn__11__20  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  11  20)
#add_test(NAME Test_Autobahn__21__30  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  21  30)
#add_test(NAME Test_Autobahn__31__40  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  31  40)
#add_test(NAME Test_Autobahn__41__50  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  41  50)
#add_test(NAME Test_Autobahn__51__60  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  51  60)
#add_test(NAME Test_Autobahn__61__70  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  61  70)
#add_test(NAME Test_Autobahn__71__80  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  71  80)
#add_test(NAME Test_Autobahn__81__90  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  81  90)
#add_test(NAME Test_Autobahn__91_100  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001  91 100)
#add_test(NAME Test_Autobahn_101_110  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 101 110)
#add_test(NAME Test_Autobahn_111_120  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 111 120)
#add_test(NAME Test_Autobahn_121_130  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 121 130)
#add_test(NAME Test_Autobahn_131_140  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 131 140)
#add_test(NAME Test_Autobahn_141_150  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 141 150)
#add_test(NAME Test_Autobahn_151_160  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 151 160)
#add_test(NAME Test_Autobahn_161_170  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 161 170)
#add_test(NAME Test_Autobahn_171_180  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 171 180)
#add_test(NAME Test_Autobahn_181_190  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 181 190)
#add_test(NAME Test_Autobahn_191_200  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 191 200)
#add_test(NAME Test_Autobahn_201_210  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 201 210)
#add_test(NAME Test_Autobahn_211_220  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 211 220)
#add_test(NAME Test_Autobahn_221_230  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 221 230)
#add_test(NAME Test_Autobahn_231_240  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 231 240)
#add_test(NAME Test_Autobahn_241_250  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 241 250)
#add_test(NAME Test_Autobahn_251_260  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 251 260)
#add_test(NAME Test_Autobahn_261_270  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 261 270)
#add_test(NAME Test_Autobahn_271_280  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 271 280)
#add_test(NAME Test_Autobahn_281_290  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 281 290)
#add_test(NAME Test_Autobahn_291_300  COMMAND ${MEMORY_CHECK} ./test_autobahn ws://localhost:9001 291 300)

add_executable(test_autobahn autobahn-testee.c
                             ../src/curlws.c
                             ../src/data_block_sender.c
                             ../src/frame_senders.c
                             ../src/frame.c
                             ../src/handlers.c
                             ../src/header.c
                             ../src/memory.c
                             ../src/random.c
                             ../src/receive.c
                             ../src/send.c
                             ../src/sha1_openssl.c
                             ../src/utf8.c
                             ../src/utils.c)


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
target_link_libraries (test_autobahn gcov)
endif()
target_link_libraries (test_autobahn ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES})
# ---------------------------------------------------------------------------- #

add_custom_target(coverage
                  COMMAND lcov -q --capture
                          --directory ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/
                          --output-file coverage.info
                  COMMAND genhtml coverage.info
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
