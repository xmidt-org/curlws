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

target_sources(curlws
    PRIVATE
        cb.c
        curlws.c
        data_block_sender.c
        frame.c
        frame_senders.c
        handlers.c
        header.c
        memory.c
        notice.c
        random.c
        receive.c
        send.c
        utils.c
        utf8.c
        verbose.c
        ws.c
)

if (USE_OPENSSL_SHA)
    MESSAGE(STATUS "Using Openssl for SHA1")
    target_sources(curlws
        PRIVATE
            sha1_openssl.c
    )
else()
    MESSAGE(STATUS "Using include SHA1 implementation")
    target_sources(curlws
        PRIVATE
            sha1_internal.c
    )
    add_subdirectory(sha1)
endif()

install (FILES
         ${CMAKE_CURRENT_SOURCE_DIR}/curlws.h
         DESTINATION include/curlws)

set_target_properties(curlws PROPERTIES
        OUTPUT_NAME curlws
        SOVERSION "0.0.0")

install(TARGETS curlws
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
       )

