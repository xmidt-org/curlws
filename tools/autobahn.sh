#!/bin/bash

# SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
# SPDX-License-Identifier: MIT


docker run -it \
           --rm \
           -v "${PWD}/reports:/reports" \
           -v "${PWD}/tools:/config" \
           -p 9001:9001 \
           -p 8080:8080 \
           --name fuzzingserver \
           crossbario/autobahn-testsuite
