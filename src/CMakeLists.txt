#
# SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
#
# SPDX-License-Identifier: MIT
#

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
        random.c
        receive.c
        send.c
        utils.c
        utf8.c
        verbose.c
        ws.c
)

configure_file(curlwsver.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/curlwsver.h)

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
         ${CMAKE_CURRENT_BINARY_DIR}/curlwsver.h
         ${CMAKE_CURRENT_SOURCE_DIR}/curlws.h
         DESTINATION include/curlws)

set_target_properties(curlws PROPERTIES
        OUTPUT_NAME curlws)

install(TARGETS curlws
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
       )

