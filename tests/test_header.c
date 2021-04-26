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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>
#include <curl/curl.h>

#include "../src/internal.h"
#include "../src/ws.h"

#include "../src/header.c"

#undef curl_easy_setopt
#undef curl_easy_getinfo

CURLcode curl_easy_setopt(CURL *easy, CURLoption option, ... )
{
    (void) easy;
    (void) option;
    return CURLE_OK;
}

static long __http_version = 0;
static long __http_code = 0;
CURLcode curl_easy_getinfo(CURL *easy, CURLINFO info, ... )
{
    va_list ap;
    long *arg;

    (void) easy;
    (void) info;

    va_start(ap, info);
    arg = va_arg(ap, long*);
    switch (info) {
        case CURLINFO_RESPONSE_CODE:
            *arg = __http_code;
            break;
        case CURLINFO_HTTP_VERSION:
            *arg = __http_version;
            break;
        default:
            break;
    }
    va_end(ap);

    return CURLE_OK;
}

void on_connect(void *data, CWS *priv, const char *websocket_protocols)
{
    (void) data;
    (void) priv;
    (void) websocket_protocols;
}

void on_close(void *data, CWS *priv, int code, const char *reason, size_t len)
{
    (void) data;
    (void) priv;
    (void) code;
    (void) reason;
    (void) len;
}

void _cws_cleanup(CWS *priv)
{
    (void) priv;
}

void setup_test(CWS *priv)
{
    memset(priv, 0, sizeof(CWS));

    priv->cb.on_connect_fn = on_connect;
    priv->cb.on_close_fn = on_close;

    header_init(priv);
    srand(0);
}


void test_redirection()
{
    CWS priv;

    setup_test(&priv);

    /* Start with a redirection */
    __http_code = 307;
    __http_version = CURL_HTTP_VERSION_1_1;
    priv.cfg.follow_redirects = true;
    CU_ASSERT(30 == _header_cb("HTTP/1.1 307 Temporary Redirect", 30, 1, &priv));
    CU_ASSERT(true == priv.header_state.redirection);
    /* Assert that the headers that are normally for us are ignored. */
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  foobar ", 30, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    CU_ASSERT(2 == _header_cb("\r\n", 2, 1, &priv));

    /* Now go onto a successful call */
    __http_code = 101;
    __http_version = CURL_HTTP_VERSION_1_1;
    priv.header_state.accepted = true;
    priv.header_state.upgraded = true;
    priv.header_state.connection_websocket = true;
    priv.header_state.ws_protocols_received = cws_strndup("chat", 4);
    CU_ASSERT(32 == _header_cb("HTTP/1.1 101  Switching Protocols", 32, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    CU_ASSERT(false == priv.header_state.upgraded);
    CU_ASSERT(false == priv.header_state.connection_websocket);
    CU_ASSERT(NULL == priv.header_state.ws_protocols_received);

    /* Now try a redirect when we didn't want to be redirected. */
    __http_code = 307;
    __http_version = CURL_HTTP_VERSION_1_1;
    priv.cfg.follow_redirects = false;
    CU_ASSERT(0 == _header_cb("HTTP/1.1 307 Temporary Redirect", 30, 1, &priv));
}


void test_simple()
{
    CWS priv;

    setup_test(&priv);

    /* A simple, successful call. */
    __http_code = 101;
    __http_version = CURL_HTTP_VERSION_1_1;
    priv.header_state.accepted = true;
    priv.header_state.upgraded = true;
    priv.header_state.connection_websocket = true;
    CU_ASSERT(32 == _header_cb("HTTP/1.1 101  Switching Protocols", 32, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    CU_ASSERT(false == priv.header_state.upgraded);
    CU_ASSERT(false == priv.header_state.connection_websocket);

    snprintf(priv.expected_key_header, 29, "foobar");
    priv.expected_key_header_len = 6;
    CU_ASSERT(false == priv.header_state.accepted);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  foobar ", 30, 1, &priv));
    CU_ASSERT(true == priv.header_state.accepted);

    CU_ASSERT(false == priv.header_state.upgraded);
    CU_ASSERT(20 == _header_cb("Connection:  upgrade", 20, 1, &priv));
    CU_ASSERT(true == priv.header_state.upgraded);
    CU_ASSERT(false == priv.header_state.connection_websocket);
    CU_ASSERT(18 == _header_cb("Upgrade: websocket", 18, 1, &priv));
    CU_ASSERT(true == priv.header_state.connection_websocket);

    CU_ASSERT(2 == _header_cb("\r\n", 2, 1, &priv));
}

void test_simple_with_protocols()
{
    CWS priv;

    setup_test(&priv);

    /* A simple, successful call. */
    __http_code = 101;
    __http_version = CURL_HTTP_VERSION_1_1;
    priv.header_state.accepted = true;
    priv.header_state.upgraded = true;
    priv.header_state.connection_websocket = true;
    priv.header_state.ws_protocols_received = cws_strndup("chat", 4);
    CU_ASSERT(32 == _header_cb("HTTP/1.1 101  Switching Protocols", 32, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    CU_ASSERT(false == priv.header_state.upgraded);
    CU_ASSERT(false == priv.header_state.connection_websocket);
    CU_ASSERT(NULL == priv.header_state.ws_protocols_received);

    snprintf(priv.expected_key_header, 29, "foobar");
    priv.expected_key_header_len = 6;
    CU_ASSERT(false == priv.header_state.accepted);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  foobar ", 30, 1, &priv));
    CU_ASSERT(true == priv.header_state.accepted);
    CU_ASSERT(NULL == priv.header_state.ws_protocols_received);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Protocol:dog,cat", 30, 1, &priv));
    CU_ASSERT_STRING_EQUAL("dog,cat", priv.header_state.ws_protocols_received);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Protocol:       ", 30, 1, &priv));
    CU_ASSERT(NULL == priv.header_state.ws_protocols_received);

    CU_ASSERT(false == priv.header_state.upgraded);
    CU_ASSERT(20 == _header_cb("Connection:  upgrade", 20, 1, &priv));
    CU_ASSERT(true == priv.header_state.upgraded);
    CU_ASSERT(false == priv.header_state.connection_websocket);
    CU_ASSERT(18 == _header_cb("Upgrade: websocket", 18, 1, &priv));
    CU_ASSERT(true == priv.header_state.connection_websocket);

    CU_ASSERT(2 == _header_cb("\r\n", 2, 1, &priv));
}


void check_output(FILE *f, const char *e1, const char *e2)
{
    char got[256];

    CU_ASSERT_FATAL(NULL != f);
    CU_ASSERT_FATAL(NULL != e1);
    CU_ASSERT_FATAL(NULL != e2);

    fflush(f);
    rewind(f);
    memset(got, 0, sizeof(got));
    CU_ASSERT_FATAL(NULL != fgets(got, sizeof(got), f));

    if (0 != strcmp(e1, got)) {
        printf("\n");
        printf("   got: '%s'\n", got);
        printf("wanted: '%s'\n", e1);
    }

    CU_ASSERT_STRING_EQUAL(e1, got);

    memset(got, 0, sizeof(got));
    CU_ASSERT_FATAL(NULL != fgets(got, sizeof(got), f));

    if (0 != strcmp(e2, got)) {
        printf("\n");
        printf("   got: '%s'\n", got);
        printf("wanted: '%s'\n", e2);
    }

    CU_ASSERT_STRING_EQUAL(e2, got);

    /* reset the stream for the next test */
    rewind(f);
}

void test_failures()
{
    CWS priv;
    FILE *f_tmp = NULL;
    char got[256];

    setup_test(&priv);
    memset(got, 0, sizeof(got));

    f_tmp = tmpfile();
    CU_ASSERT_FATAL(NULL != f_tmp);

    __http_code = 404;
    __http_version = CURL_HTTP_VERSION_1_1;
    CU_ASSERT(0 == _header_cb("HTTP/1.1 404                  ", 30, 1, &priv));

    __http_code = 101;
    __http_version = CURL_HTTP_VERSION_1_0;
    CU_ASSERT(0 == _header_cb("HTTP/1.0 101                  ", 30, 1, &priv));

    /* Back to normal. */
    __http_code = 101;
    __http_version = CURL_HTTP_VERSION_1_1;

    /* Fake header :) */
    CU_ASSERT(2 == _header_cb("a:", 2, 1, &priv));

    /* Too soon, the headers telling us we're connected aren't there. */
    CU_ASSERT(0 == _header_cb("\r\n", 2, 1, &priv));


    /* This test must be first! */
    /* Less out of bounds, no output. */
    snprintf(priv.expected_key_header, 29, "foobar");
    priv.expected_key_header_len = 6;
    priv.header_state.accepted = true;
    CU_ASSERT(1000 == _header_cb("Sec-WebSocket-Accept:  nobarke123123123...totally_invalid", 1000, 1, &priv));
    CU_ASSERT_FATAL(NULL == fgets(got, sizeof(got), f_tmp));
    CU_ASSERT(false == priv.header_state.accepted);

    /* Wrong accept string. */
    priv.cfg.verbose = 1;
    priv.cfg.verbose_stream = f_tmp;

    /* different lengths */
    snprintf(priv.expected_key_header, 29, "foobar");
    priv.expected_key_header_len = 6;
    priv.header_state.accepted = true;
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  nobar  ", 30, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    check_output(f_tmp, "< websocket header received: 30\n",
                 "! websocket header expected (value len=6): 'Sec-WebSocket-Accept: foobar', got (len=5): 'nobar'\n");

    /* Way out of bounds. */
    snprintf(priv.expected_key_header, 29, "foobar");
    priv.expected_key_header_len = 6;
    priv.header_state.accepted = true;
    CU_ASSERT(SIZE_MAX == _header_cb("Sec-WebSocket-Accept:  nobarke123123123...totally_invalid", SIZE_MAX, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    check_output(f_tmp, "< websocket header received: 18446744073709551615\n",
                 "! websocket header expected (value len=6): 'Sec-WebSocket-Accept: foobar', got (len=18446744073709551592): 'nobark...'\n");

    /* Less out of bounds. */
    snprintf(priv.expected_key_header, 29, "foobar");
    priv.expected_key_header_len = 6;
    priv.header_state.accepted = true;
    CU_ASSERT(1000 == _header_cb("Sec-WebSocket-Accept:  nobarke123123123...totally_invalid", 1000, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    check_output(f_tmp, "< websocket header received: 1000\n",
                 "! websocket header expected (value len=6): 'Sec-WebSocket-Accept: foobar', got (len=977): 'nobark...'\n");


    priv.header_state.accepted = true;
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  notbar ", 30, 1, &priv));
    CU_ASSERT(false == priv.header_state.accepted);
    check_output(f_tmp, "< websocket header received: 30\n",
                 "! websocket header expected (value len=6): 'Sec-WebSocket-Accept: foobar', got (len=6): 'notbar'\n");

    priv.header_state.upgraded = true;
    CU_ASSERT(20 == _header_cb("Connection:  BADrade", 20, 1, &priv));
    CU_ASSERT(false == priv.header_state.upgraded);
    check_output(f_tmp, "< websocket header received: 20\n",
                 "! websocket header expected (value len=7): 'Connection: upgrade', got (len=7): 'BADrade'\n");

    priv.header_state.connection_websocket = true;
    CU_ASSERT(18 == _header_cb("Upgrade: BADsocket", 18, 1, &priv));
    CU_ASSERT(false == priv.header_state.connection_websocket);
    check_output(f_tmp, "< websocket header received: 18\n",
                 "! websocket header expected (value len=9): 'Upgrade: websocket', got (len=9): 'BADsocket'\n");

    fclose(f_tmp);
    f_tmp = NULL;
}



void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "simple cb Tests               ", .fn = test_simple                },
        { .label = "simple cb with protocols Tests", .fn = test_simple_with_protocols },
        { .label = "redirection Tests             ", .fn = test_redirection           },
        { .label = "failure Tests                 ", .fn = test_failures              },
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
