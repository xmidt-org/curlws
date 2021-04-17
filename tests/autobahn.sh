#!/bin/bash
docker run -it \
           --rm \
           -v "${PWD}/reports:/reports" \
           -v "${PWD}/tests:/config" \
           -p 9001:9001 \
           -p 8080:8080 \
           --name fuzzingserver \
           crossbario/autobahn-testsuite
