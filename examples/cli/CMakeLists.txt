# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
# SPDX-License-Identifier: MIT

add_executable(cli cli.c)

target_link_libraries (cli curlws)
target_link_libraries (cli ${CURL_LIBRARIES})
target_link_libraries (cli ${TROWER_LIBRARIES})
target_link_libraries (cli ${OPENSSL_LIBRARIES})
