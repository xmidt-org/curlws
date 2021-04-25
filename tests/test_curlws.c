/*
 * Copyright (c) 2021  Comcast Cable Communications Management, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://opensource.org/licenses/MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CUnit/Basic.h>
#include <curl/curl.h>

#include "../src/curlws.h"
#include "../src/internal.h"
#include "../src/frame_senders.h"

#include "curlws_mocks.c"

#define validate_and_reset(...)                     \
do {                                                \
    const char *test[] = {__VA_ARGS__};             \
    curl_slist_compare(__curl_easy_setopt, test);   \
    curl_slist_free_all(__curl_easy_setopt);        \
    __curl_easy_setopt = NULL;                      \
} while(0)

#define reset_setopt()                          \
do {                                            \
    curl_slist_free_all(__curl_easy_setopt);    \
    __curl_easy_setopt = NULL;                  \
} while(0)

#define str(s) #s
#define xstr(s) str(s)

#define MY_CURL_SSLVERSION_TLS  6
#define MY_HTTP_VERSION         2

int __my_cfg_fn_seen = 0;
void my_cfg_fn(void *user, CWS *handle, CURL *easy)
{
    CU_ASSERT(NULL == user);
    CU_ASSERT(NULL != handle);
    CU_ASSERT(NULL != easy);

    __my_cfg_fn_seen++;
}


void test_create_curl_version()
{
    CWS *ws;
    FILE *f_tmp = NULL;
    struct cws_config cfg;
    char got[256];

    memset(got, 0, sizeof(got));
    memset(&cfg, 0, sizeof(cfg));
    reset_setopt();

    f_tmp = tmpfile();
    CU_ASSERT_FATAL(NULL != f_tmp);

    __curl_info_test_value = true;
    __curl_info.version_num = 0x060000;
    __curl_info.version = "6.0.0";

    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    cfg.verbose_stream = f_tmp;
    cfg.verbose = 1;

    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    reset_setopt();

    rewind(f_tmp);
    CU_ASSERT_FATAL(NULL != fgets(got, sizeof(got), f_tmp));
    fclose(f_tmp);
    f_tmp = NULL;

    CU_ASSERT_STRING_EQUAL(got, "ERROR: CURL version '6.0.0'. At least '7.50.2' "
                                "is required for curlws to work reliably\n");

    __curl_info_test_value = false;
}


void test_create_verbose()
{
    CWS *ws;
    FILE *f_tmp = NULL;
    struct cws_config cfg;
    char got[256];

    memset(got, 0, sizeof(got));
    memset(&cfg, 0, sizeof(cfg));
    reset_setopt();

    f_tmp = tmpfile();
    CU_ASSERT_FATAL(NULL != f_tmp);

    /* Make it generally valid first. */
    cfg.url = "wss://example.com";

    /* Invalid verbose options */
    cfg.verbose = -1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    cfg.verbose = 12;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    /* Valid options */
    cfg.verbose = 1;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");

    cfg.verbose = 1;
    cfg.verbose_stream = (FILE*) 0x1234;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    CU_ASSERT((FILE*) 0x1234 == ws->cfg.verbose_stream);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");


    cfg.verbose = 2;
    cfg.verbose_stream = (FILE*) 0x1234;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    CU_ASSERT((FILE*) 0x1234 == ws->cfg.verbose_stream);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_VERBOSE        : 1",
        "CURLOPT_STDERR         : 0x1234",
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");

    cfg.verbose = 3;
    cfg.verbose_stream = NULL;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_VERBOSE        : 1",
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");

    cfg.verbose = 0;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");


}

void test_create_easy_init_fail()
{
    CWS *ws;
    struct cws_config cfg;
    struct mock_curl_easy_init test = {
        .rv = NULL,
        .seen = 0,
        .more = 0,
    };

    memset(&cfg, 0, sizeof(cfg));

    reset_setopt();

    __curl_easy_init = &test;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();
}

void test_create_basic()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));

    reset_setopt();

    ws = cws_create(NULL);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    /* A minimal, but reasonable case. */
    cfg.url = "wss://example.com";
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);

    /* Validate the expected header based on a fixed random number */
    CU_ASSERT_STRING_EQUAL(ws->expected_key_header, "4522FSSIYHASSkUaUouiInl8Cvk=");
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_config_cb()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();


    /* Checking the config callback works */
    __my_cfg_fn_seen = 0;
    cfg.configure = my_cfg_fn;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
    cfg.configure = NULL;
    CU_ASSERT(1 == __my_cfg_fn_seen);
}


void test_create_redirect()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Invalid redirect count */
    cfg.max_redirects = -2;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    /* Valid redirect count (unlimited) */
    cfg.max_redirects = -1;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_FOLLOWLOCATION : 1",
        "CURLOPT_MAXREDIRS      : -1",
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");

    /* Valid redirect count (fixed) */
    cfg.max_redirects = 10;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_FOLLOWLOCATION : 1",
        "CURLOPT_MAXREDIRS      : 10",
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_interface()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Any interface value is accepted. */
    cfg.interface = "not_really_valid";
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_INTERFACE      : not_really_valid",
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_ip()
{
    CWS *ws;
    struct cws_config cfg;
    int invalid[] = { -1, 1, 2, 3, 5, 7, 8, 9, 10 };

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Invalid ip option */
    for (size_t i = 0; i < sizeof(invalid)/sizeof(int); i++) {
        cfg.ip_version = invalid[i];
        ws = cws_create(&cfg);
        CU_ASSERT(NULL == ws);
        reset_setopt();
    }

    /* Valid ip options */
    cfg.ip_version = 4;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_IPRESOLVE      : " xstr(CURL_IPRESOLVE_V4),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");

    cfg.ip_version = 6;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_IPRESOLVE      : " xstr(CURL_IPRESOLVE_V6),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_insecure()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Invalid security option */
    cfg.insecure_ok = 1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);
    reset_setopt();

    /* Valid security option - no security */
    cfg.insecure_ok = CURLWS_INSECURE_MODE;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_SSL_VERIFYPEER : 0",
        "CURLOPT_SSL_VERIFYHOST : 0",
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");

    cfg.insecure_ok = 0;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_ws_protos()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Websocket_protocols are not NULL */
    cfg.websocket_protocols = "chat";
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    CU_ASSERT_STRING_EQUAL(ws->cfg.ws_protocols_requested, "chat");
    cws_destroy(ws);
    cfg.websocket_protocols = NULL;
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Sec-WebSocket-Protocol: chat",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_expect()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* expect invalid */
    cfg.expect = -1;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL == ws);
    reset_setopt();

    /* Valid expect */
    cfg.expect = 1;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Expect: 101",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_payload_size()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Valid max payload size */
    cfg.max_payload_size = 10;
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    cfg.max_payload_size = 0;
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13");
}


void test_create_extra_headers()
{
    CWS *ws;
    struct cws_config cfg;
    char *invalid[] = {
        "Connection: invalid",
        "Content-Length: 42",
        "Content-Type: binary",
        "Expect: invalid",
        "Sec-WebSocket-Accept: anything",
        "Sec-WebSocket-Key: random",
        "Sec-WebSocket-Protocol: chat",
        "Sec-WebSocket-Version: 42",
        "Transfer-Encoding: 7",
        "Upgrade: No!"
    };
    /* TODO: Add more invalid headers... */

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();

    /* Valid headers */
    cfg.extra_headers = curl_slist_append(cfg.extra_headers, "Safe: header");
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13",
        "Safe: header");

    cfg.extra_headers = curl_slist_append(cfg.extra_headers, "Extra-Safe: header");
    ws = cws_create(&cfg);
    CU_ASSERT_FATAL(NULL != ws);
    cws_destroy(ws);
    validate_and_reset(
        "CURLOPT_URL            : https://example.com",
        "CURLOPT_SSLVERSION     : " xstr(MY_CURL_SSLVERSION_TLS),
        "CURLOPT_HTTP_VERSION   : " xstr(MY_HTTP_VERSION),
        "CURLOPT_UPLOAD         : 1",
        "CURLOPT_CUSTOMREQUEST  : GET",
        "CURLOPT_FORBID_REUSE   : 1",
        "CURLOPT_FRESH_CONNECT  : 1",
        "Transfer-Encoding:",
        "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==",
        "Connection: Upgrade",
        "Upgrade: websocket",
        "Sec-WebSocket-Version: 13",
        "Safe: header",
        "Extra-Safe: header");

    for (size_t i = 0; i < sizeof(invalid)/sizeof(char*); i++) {
        /* Invalid headers */
        curl_slist_free_all(cfg.extra_headers);
        cfg.extra_headers = NULL;

        cfg.extra_headers = curl_slist_append(cfg.extra_headers, invalid[i]);
        ws = cws_create(&cfg);
        if (NULL != ws) {
            printf("Expected failure, but it passed: '%s'\n", invalid[i]);
        }
        CU_ASSERT(NULL == ws);
        reset_setopt();
    }

    curl_slist_free_all(cfg.extra_headers);
}


void test_destroy_failures()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = "wss://example.com";
    reset_setopt();


    /* Simulate a failure during creation. */
    ws = (CWS*) calloc(1, sizeof(CWS));
    cws_destroy(ws);
}

void test_close()
{
    CWS ws;
    struct mock_sender test[] = {
        { .options = CWS_CLOSE,              .data = "\x03\xe9goodbye", .len = 9, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_CLOSE,              .data = NULL,              .len = 0, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_CLOSE,              .data = "\x03\xe9",        .len = 2, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_CLOSE | CWS_URGENT, .data = "\x03\xe9goodbye", .len = 9, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_CLOSE | CWS_URGENT, .data = NULL,              .len = 0, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_CLOSE | CWS_URGENT, .data = NULL,              .len = 0, .rv = CWSE_OK, .seen = 0, .more = 0 },
    };

    memset(&ws, 0, sizeof(CWS));
    __frame_sender_control = &test[0];

    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_close(NULL, 0, NULL, 0));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_close(&ws, 0, NULL, 3));

    CU_ASSERT(CWSE_INVALID_CLOSE_REASON_CODE == cws_close(&ws, 9999, NULL, 0));
    CU_ASSERT(CWSE_INVALID_CLOSE_REASON_CODE == cws_close(&ws, 0, "bye", 3));
    CU_ASSERT(CWSE_INVALID_CLOSE_REASON_CODE == cws_close(&ws, 0, "bye", SIZE_MAX));

    CU_ASSERT(CWSE_INVALID_UTF8 == cws_close(&ws, 1001, "\x80\x00", SIZE_MAX));
    CU_ASSERT(CWSE_INVALID_UTF8 == cws_close(&ws, 1001, "abcd\xc4", 5));
    CU_ASSERT(CWSE_APP_DATA_LENGTH_TOO_LONG == cws_close(&ws, 1001, "really long", 200));

    CU_ASSERT(CWSE_OK == cws_close(&ws, 1001, "goodbye", SIZE_MAX));
    CU_ASSERT(CWSE_OK == cws_close(&ws, 0, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_close(&ws, 1001, "ignored", 0));

    /* Urgent close */
    CU_ASSERT(CWSE_OK == cws_close(&ws, -1001, "goodbye", SIZE_MAX));
    CU_ASSERT(CWSE_OK == cws_close(&ws, -1, NULL, 0));
}

void test_ping_pong()
{
    CWS ws;
    memset(&ws, 0, sizeof(CWS));

    struct mock_sender test[] = {
        { .options = CWS_PING, .data = NULL,    .len = 0, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_PONG, .data = NULL,    .len = 0, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_PING, .data = "bingo", .len = 5, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_PONG, .data = "bongo", .len = 5, .rv = CWSE_OK, .seen = 0, .more = 0 },
    };
    __frame_sender_control = &test[0];

    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_ping(NULL, NULL, 0));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_pong(NULL, NULL, 0));

    CU_ASSERT(CWSE_OK == cws_ping(&ws, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_pong(&ws, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_ping(&ws, "bingo123", 5));
    CU_ASSERT(CWSE_OK == cws_pong(&ws, "bongo123", 5));
}

void test_send_blk()
{
    CWS ws;
    memset(&ws, 0, sizeof(CWS));

    struct mock_sender test[] = {
        { .options = CWS_BINARY, .data = "random data", .len = 11, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_BINARY, .data = NULL,          .len =  0, .rv = CWSE_OK, .seen = 0, .more = 2 },
        { .options = CWS_BINARY, .data = NULL,          .len =  0, .rv = CWSE_OK, .seen = 0, .more = 3 },
        { .options = CWS_TEXT,   .data = "random data", .len = 11, .rv = CWSE_OK, .seen = 0, .more = 4 },
        { .options = CWS_TEXT,   .data = "random data", .len = 11, .rv = CWSE_OK, .seen = 0, .more = 5 },
        { .options = CWS_TEXT,   .data = NULL,          .len =  0, .rv = CWSE_OK, .seen = 0, .more = 6 },
        { .options = CWS_TEXT,   .data = NULL,          .len =  0, .rv = CWSE_OK, .seen = 0, .more = 0 },
    };
    __data_block_sender = &test[0];

    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_blk_binary(NULL, NULL, 0));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_blk_binary(&ws, NULL, 2));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_blk_text(NULL, NULL, 0));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_blk_text(&ws, NULL, 2));

    CU_ASSERT(CWSE_OK == cws_send_blk_binary(&ws, "random data", 11));
    CU_ASSERT(CWSE_OK == cws_send_blk_binary(&ws, "random data", 0));
    CU_ASSERT(CWSE_OK == cws_send_blk_binary(&ws, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_send_blk_text(&ws, "random data", 11));
    CU_ASSERT(CWSE_OK == cws_send_blk_text(&ws, "random data", SIZE_MAX));
    CU_ASSERT(CWSE_OK == cws_send_blk_text(&ws, "random data", 0));
    CU_ASSERT(CWSE_OK == cws_send_blk_text(&ws, NULL, 0));

    CU_ASSERT(CWSE_INVALID_UTF8 == cws_send_blk_text(&ws, "\xc4 data", 6));
    CU_ASSERT(CWSE_INVALID_UTF8 == cws_send_blk_text(&ws, "data \xc2", 6));
}


void test_bin_stream()
{
    CWS ws;
    memset(&ws, 0, sizeof(CWS));

    struct mock_sender test[] = {
        { .options = CWS_BINARY | CWS_FIRST | CWS_LAST, .data = "hi",  .len = 2, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_BINARY | CWS_FIRST | CWS_LAST, .data = NULL,  .len = 0, .rv = CWSE_OK, .seen = 0, .more = 2 },
        { .options = CWS_BINARY | CWS_FIRST | CWS_LAST, .data = NULL,  .len = 0, .rv = CWSE_OK, .seen = 0, .more = 3 },
        { .options = CWS_BINARY | CWS_FIRST,            .data = "hi",  .len = 2, .rv = CWSE_OK, .seen = 0, .more = 4 },
        { .options = CWS_CONT,                          .data = "mid", .len = 3, .rv = CWSE_OK, .seen = 0, .more = 5 },
        { .options = CWS_CONT               | CWS_LAST, .data = "bye", .len = 3, .rv = CWSE_OK, .seen = 0, .more = 0 },
    };
    __frame_sender_data = &test[0];

    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_strm_binary(NULL, 0, NULL, 0));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_strm_binary(&ws, CWS_FIRST|CWS_LAST, NULL, 2));
    CU_ASSERT(CWSE_STREAM_CONTINUITY_ISSUE == cws_send_strm_binary(&ws, 0, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, CWS_FIRST|CWS_LAST, "hi", 2));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, CWS_FIRST|CWS_LAST, "hi", 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, CWS_FIRST|CWS_LAST, NULL, 0));

    /* A bit of a workaround since we mocked out the code that handles this */
    ws.last_sent_data_frame_info = CWS_BINARY;
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, CWS_FIRST, "hi", 2));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, 0, "ignored", 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, 0, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, 0, "mid", 3));
    CU_ASSERT(CWSE_OK == cws_send_strm_binary(&ws, CWS_LAST, "bye", 3));
    ws.last_sent_data_frame_info = 0;
}

void test_txt_stream()
{
    CWS ws;
    memset(&ws, 0, sizeof(CWS));

    struct mock_sender test[] = {
        { .options = CWS_TEXT   | CWS_FIRST | CWS_LAST, .data = "hi",  .len = 2, .rv = CWSE_OK, .seen = 0, .more = 1 },
        { .options = CWS_TEXT   | CWS_FIRST | CWS_LAST, .data = "hi",  .len = 2, .rv = CWSE_OK, .seen = 0, .more = 2 },
        { .options = CWS_TEXT   | CWS_FIRST | CWS_LAST, .data = NULL,  .len = 0, .rv = CWSE_OK, .seen = 0, .more = 3 },
        { .options = CWS_TEXT   | CWS_FIRST | CWS_LAST, .data = NULL,  .len = 0, .rv = CWSE_OK, .seen = 0, .more = 4 },
        { .options = CWS_TEXT   | CWS_FIRST,            .data = "hi",  .len = 2, .rv = CWSE_OK, .seen = 0, .more = 5 },
        { .options = CWS_CONT,                          .data = "mid", .len = 3, .rv = CWSE_OK, .seen = 0, .more = 6 },
        { .options = CWS_CONT               | CWS_LAST, .data = "bye", .len = 3, .rv = CWSE_OK, .seen = 0, .more = 0 },
    };
    __frame_sender_data = &test[0];

    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_strm_text(NULL, 0, NULL, 0));
    CU_ASSERT(CWSE_BAD_FUNCTION_ARGUMENT == cws_send_strm_text(&ws, CWS_FIRST|CWS_LAST, NULL, 2));
    CU_ASSERT(CWSE_INVALID_UTF8 == cws_send_strm_text(&ws, CWS_FIRST|CWS_LAST, "\xc2w", 2));
    CU_ASSERT(CWSE_STREAM_CONTINUITY_ISSUE == cws_send_strm_text(&ws, 0, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, CWS_FIRST|CWS_LAST, "hi", SIZE_MAX));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, CWS_FIRST|CWS_LAST, "hi", 2));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, CWS_FIRST|CWS_LAST, "hi", 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, CWS_FIRST|CWS_LAST, NULL, 0));

    /* A bit of a workaround since we mocked out the code that handles this */
    ws.last_sent_data_frame_info = CWS_BINARY;
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, CWS_FIRST, "hi", 2));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, 0, "ignored", 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, 0, NULL, 0));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, 0, "mid", 3));
    CU_ASSERT(CWSE_OK == cws_send_strm_text(&ws, CWS_LAST, "bye", 3));
    ws.last_sent_data_frame_info = 0;
}

void test_multi_handles()
{
    CWS ws;

    memset(&ws, 0, sizeof(ws));

    /* These are pretty simple */
    CU_ASSERT(CURLM_UNKNOWN_OPTION == cws_multi_add_handle(NULL, NULL));
    CU_ASSERT(CURLM_UNKNOWN_OPTION == cws_multi_remove_handle(NULL, NULL));

    CU_ASSERT(CURLM_OK == cws_multi_add_handle(&ws, NULL));
    CU_ASSERT(CURLM_OK == cws_multi_remove_handle(&ws, NULL));
}



void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "create: basic Tests",         .fn = test_create_basic          },
        { .label = "create: config cb Tests",     .fn = test_create_config_cb      },
        { .label = "create: redirect Tests",      .fn = test_create_redirect       },
        { .label = "create: interface Tests",     .fn = test_create_interface      },
        { .label = "create: force IP ver Tests",  .fn = test_create_ip             },
        { .label = "create: insecure Tests",      .fn = test_create_insecure       },
        { .label = "create: ws protos Tests",     .fn = test_create_ws_protos      },
        { .label = "create: expect Tests",        .fn = test_create_expect         },
        { .label = "create: payload size Tests",  .fn = test_create_payload_size   },
        { .label = "create: extra headers Tests", .fn = test_create_extra_headers  },
        { .label = "create: curl version Tests",  .fn = test_create_curl_version   },
        { .label = "create: verbose Tests",       .fn = test_create_verbose        },
        { .label = "create: easy_init Failure",   .fn = test_create_easy_init_fail },
        { .label = "destroy failures Tests",      .fn = test_destroy_failures },
        { .label = "cws_close Tests",       .fn = test_close          },
        { .label = "cws_ping/pong Tests",   .fn = test_ping_pong      },
        { .label = "cws_send_blk Tests",    .fn = test_send_blk       },
        { .label = "bin stream Tests",      .fn = test_bin_stream     },
        { .label = "txt stream Tests",      .fn = test_txt_stream     },
        { .label = "multi handles Tests",   .fn = test_multi_handles  },
        { .label = NULL, .fn = NULL }
    };
    int i;

    *suite = CU_add_suite( "curlws.c tests", NULL, NULL );

    for (i = 0; NULL != tests[i].fn; i++) {
        CU_add_test( *suite, tests[i].label, tests[i].fn );
    }
}


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main( void )
{
    unsigned rv = 1;
    CU_pSuite suite = NULL;

    if( CUE_SUCCESS == CU_initialize_registry() ) {
        add_suites( &suite );

        if( NULL != suite ) {
            CU_basic_set_mode( CU_BRM_VERBOSE );
            CU_basic_run_tests();
            printf( "\n" );
            CU_basic_show_failures( CU_get_failure_list() );
            printf( "\n\n" );
            rv = CU_get_number_of_tests_failed();
        }

        CU_cleanup_registry();
    }

    if( 0 != rv ) {
        return 1;
    }
    return 0;
}
