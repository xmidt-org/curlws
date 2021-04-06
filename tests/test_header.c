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

void debug_fn(void *data, CWS *priv, const char *format, ...)
{
    (void) data;
    (void) priv;
    (void) format;
}

void _cws_cleanup(CWS *priv)
{
    (void) priv;
}

void setup_test(CWS *priv)
{
    memset(priv, 0, sizeof(CWS));

    priv->on_connect_fn = on_connect;
    priv->on_close_fn = on_close;
    priv->debug_fn = debug_fn;

    header_init(priv);
    srand(0);
}


void test_simple()
{
    CWS priv;

    setup_test(&priv);

    /* Start with a redirection */
    __http_code = 307;
    __http_version = CURL_HTTP_VERSION_1_1;
    CU_ASSERT(30 == _header_cb("HTTP/1.1 307 Temporary Redirect", 30, 1, &priv));
    CU_ASSERT(true == priv.redirection);
    /* Assert that the headers that are normally for us are ignored. */
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  foobar ", 30, 1, &priv));
    CU_ASSERT(false == priv.accepted);
    CU_ASSERT(2 == _header_cb("\r\n", 2, 1, &priv));

    /* Now go onto a successful call */
    __http_code = 101;
    __http_version = CURL_HTTP_VERSION_1_1;
    priv.accepted = true;
    priv.upgraded = true;
    priv.connection_websocket = true;
    priv.websocket_protocols.received = cws_strndup("chat", 4);
    CU_ASSERT(32 == _header_cb("HTTP/1.1 101  Switching Protocols", 32, 1, &priv));
    CU_ASSERT(false == priv.accepted);
    CU_ASSERT(false == priv.upgraded);
    CU_ASSERT(false == priv.connection_websocket);
    CU_ASSERT(NULL == priv.websocket_protocols.received);

    snprintf(priv.expected_key_header, 29, "foobar");
    CU_ASSERT(false == priv.accepted);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  foobar ", 30, 1, &priv));
    CU_ASSERT(true == priv.accepted);
    CU_ASSERT(NULL == priv.websocket_protocols.received);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Protocol:dog,cat", 30, 1, &priv));
    CU_ASSERT_STRING_EQUAL("dog,cat", priv.websocket_protocols.received);
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Protocol:       ", 30, 1, &priv));
    CU_ASSERT(NULL == priv.websocket_protocols.received);

    CU_ASSERT(false == priv.upgraded);
    CU_ASSERT(20 == _header_cb("Connection:  upgrade", 20, 1, &priv));
    CU_ASSERT(true == priv.upgraded);
    CU_ASSERT(false == priv.connection_websocket);
    CU_ASSERT(18 == _header_cb("Upgrade: websocket", 18, 1, &priv));
    CU_ASSERT(true == priv.connection_websocket);

    CU_ASSERT(2 == _header_cb("\r\n", 2, 1, &priv));
}


void test_failures()
{
    CWS priv;

    setup_test(&priv);

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

    /* Wrong accept string. */
    snprintf(priv.expected_key_header, 29, "foobar");
    priv.accepted = true;
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  nobar  ", 30, 1, &priv));
    CU_ASSERT(false == priv.accepted);

    priv.accepted = true;
    CU_ASSERT(30 == _header_cb("Sec-WebSocket-Accept:  notbar ", 30, 1, &priv));
    CU_ASSERT(false == priv.accepted);

    priv.upgraded = true;
    CU_ASSERT(20 == _header_cb("Connection:  BADrade", 20, 1, &priv));
    CU_ASSERT(false == priv.upgraded);

    priv.connection_websocket = true;
    CU_ASSERT(18 == _header_cb("Upgrade: BADsocket", 18, 1, &priv));
    CU_ASSERT(false == priv.connection_websocket);
}



void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "simple cb Tests", .fn = test_simple        },
        { .label = "failure Tests", .fn = test_failures },
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
