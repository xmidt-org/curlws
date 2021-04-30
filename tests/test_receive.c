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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>
#include <curl/curl.h>

#include "../src/internal.h"
#include "../src/ws.h"

#include "../src/receive.c"

#undef curl_easy_setopt


CURLcode curl_easy_setopt(CURL *easy, CURLoption option, ... )
{
    (void) easy;
    (void) option;
    return CURLE_OK;
}


CURLcode curl_easy_pause(CURL *easy, int bitmask)
{
    (void) easy;
    (void) bitmask;
    return CURLE_OK;
}


struct mock_cws_close {
    int code;
    const char *reason;
    size_t len;
    int seen;
    CWScode rv;

    int more;
};
static struct mock_cws_close *__cws_close_goal = NULL;
CWScode cws_close(CWS *priv, int code, const char *reason, size_t len)
{
    CWScode rv = CWSE_OK;

    //printf( "cws_close( ... code: %d, reason: '%.*s', len: %ld )\n", code, (int) len, reason, len );

    if (__cws_close_goal) {
        CU_ASSERT(NULL != priv);
        priv->close_state = CLOSE_QUEUED;
        CU_ASSERT(__cws_close_goal->code == code);
        CU_ASSERT(__cws_close_goal->len == len);
        /* Make sure the string lengths actually are the same ... */
        if (SIZE_MAX == len) {
            len = strlen(reason);
        }
        if (SIZE_MAX == __cws_close_goal->len) {
            __cws_close_goal->len = strlen(reason);
        }
        CU_ASSERT(__cws_close_goal->len == len);

        if ((NULL != __cws_close_goal->reason) && (__cws_close_goal->len == len)) {
            for (size_t i = 0; i < len; i++) {
                if (__cws_close_goal->reason[i] != reason[i]) {
                    printf( "%ld: want: '%c', got: '%c'\n", i, __cws_close_goal->reason[i], reason[i]);
                }
                CU_ASSERT(__cws_close_goal->reason[i] == reason[i]);
            }
        }

        rv = __cws_close_goal->rv;

        __cws_close_goal->seen++;
        if (0 != __cws_close_goal->more) {
            __cws_close_goal = &__cws_close_goal[1];
        } else {
            __cws_close_goal = NULL;
        }
    }

    return rv;
}


struct mock_stream {
    int info;
    const char *data;
    size_t len;
    int seen;
    int more;
};

static struct mock_stream *__on_fragment_goal = NULL;
static int on_fragment(void *data, CWS *handle, int info, const void *buffer, size_t len)
{
    (void) data;

    //printf("on_fragment( ... info: %08x, buffer: 0x%p, len: %ld )\n", info, buffer, len);

    if (__on_fragment_goal) {
        CU_ASSERT(NULL != handle);

        CU_ASSERT(__on_fragment_goal->info == info);
        if (__on_fragment_goal->info != info) {
            printf("info: want: %08x, got: %08x\n", __on_fragment_goal->info, info);
        }

        CU_ASSERT(__on_fragment_goal->len == len);
        if (__on_fragment_goal->len != len) {
            printf("len: want: %ld, got: %ld\n", __on_fragment_goal->len, len);
        }

        if (NULL != __on_fragment_goal->data) {
            for (size_t i = 0; i < len; i++) {
                const char *c = (const char*) buffer;

                if (__on_fragment_goal->data[i] != c[i]) {
                    printf("data[%zd]: want: '%c' (0x%02x), got: '%c' (0x%02x)\n",
                            i, __on_fragment_goal->data[i], __on_fragment_goal->data[i],
                            c[i], c[i]);
                }
                CU_ASSERT(__on_fragment_goal->data[i] == c[i]);
            }
        }
        __on_fragment_goal->seen++;
        if (0 != __on_fragment_goal->more) {
            __on_fragment_goal = &__on_fragment_goal[1];
        } else {
            __on_fragment_goal = NULL;
        }
    }

    return 0;
}


struct mock_close {
    int code;
    const char *reason;
    size_t len;
    int seen;
    int more;
};

static struct mock_close *__on_close_goal = NULL;
static int on_close(void *data, CWS *handle, int code, const char *reason, size_t len)
{
    (void) data;

    //printf( "on_close( ... code: %d, reason: '%.*s', len: %lu )\n", code, (int) len, reason, len );

    if (__on_close_goal) {
        CU_ASSERT(NULL != handle);
        CU_ASSERT(__on_close_goal->code == code);
        CU_ASSERT(__on_close_goal->len == len);
        /* Make sure the string lengths actually are the same ... */
        if (SIZE_MAX == len) {
            len = strlen(reason);
        }
        if (SIZE_MAX == __on_close_goal->len) {
            __on_close_goal->len = strlen(reason);
        }
        CU_ASSERT(__on_close_goal->len == len);

        if ((NULL != __on_close_goal->reason) && (__on_close_goal->len == len)) {
            for (size_t i = 0; i < len; i++) {
                CU_ASSERT(__on_close_goal->reason[i] == reason[i]);
            }
        }
        __on_close_goal->seen++;
        if (0 != __on_close_goal->more) {
            __on_close_goal = &__on_close_goal[1];
        } else {
            __on_close_goal = NULL;
        }
    }

    return 0;
}


struct mock_ping {
    const void *data;
    size_t len;
    int seen;
    int more;
};

static struct mock_ping *__on_ping_goal = NULL;
static int on_ping(void *data, CWS *handle, const void *buffer, size_t len)
{
    (void) data;

    //printf( "on_ping( ... buffer: 0x%p, len: %ld )\n", buffer, len );

    if (__on_ping_goal) {
        CU_ASSERT(NULL != handle);
        CU_ASSERT(__on_ping_goal->len == len);
        if (NULL != __on_ping_goal->data) {
            for (size_t i = 0; i < len; i++) {
                CU_ASSERT(((uint8_t*)__on_ping_goal->data)[i] == ((uint8_t*)buffer)[i]);
            }
        }
        __on_ping_goal->seen++;
        if (0 != __on_ping_goal->more) {
            __on_ping_goal = &__on_ping_goal[1];
        } else {
            __on_ping_goal = NULL;
        }
    }

    return 0;
}


struct mock_pong {
    const void *data;
    size_t len;
    int seen;
    int more;
};

static struct mock_pong *__on_pong_goal = NULL;
static int on_pong(void *data, CWS *handle, const void *buffer, size_t len)
{
    (void) data;

    //printf( "on_pong( ... buffer: 0x%p, len: %ld )\n", buffer, len );

    if (__on_pong_goal) {
        CU_ASSERT(NULL != handle);
        CU_ASSERT(__on_pong_goal->len == len);
        if (NULL != __on_pong_goal->data) {
            for (size_t i = 0; i < len; i++) {
                CU_ASSERT(((uint8_t*)__on_pong_goal->data)[i] == ((uint8_t*)buffer)[i]);
            }
        }
        __on_pong_goal->seen++;
        if (0 != __on_pong_goal->more) {
            __on_pong_goal = &__on_pong_goal[1];
        } else {
            __on_pong_goal = NULL;
        }
    }

    return 0;
}


struct test_vector {
    const char *test_name;
    const char *in;
    const int *blocks;
    const size_t *rv;
    size_t block_count;

    bool follow_redirects;
    bool redirection;

    struct mock_ping *ping;
    struct mock_pong *pong;
    struct mock_stream *stream;
    struct mock_close *close;
};


void run_test( struct test_vector *v )
{
    CWS priv;
    const char *p;
    struct mock_ping *ping;
    struct mock_pong *pong;
    struct mock_stream *stream;
    struct mock_close *close;

    memset(&priv, 0, sizeof(CWS));

    receive_init(&priv);
    priv.cb.on_fragment_fn = on_fragment;
    priv.cb.on_close_fn = on_close;
    priv.cb.on_ping_fn = on_ping;
    priv.cb.on_pong_fn = on_pong;

    priv.cfg.follow_redirects = v->follow_redirects;
    priv.header_state.redirection = v->redirection;

    __on_ping_goal = v->ping;
    __on_pong_goal = v->pong;
    __on_fragment_goal = v->stream;
    __on_close_goal = v->close;

    p = v->in;
    for (size_t i = 0; i < v->block_count; i++) {
        size_t rv = _writefunction_cb(p, v->blocks[i], 1, &priv);

        if (rv != v->rv[i]) {
            printf("%s - '%.*s' byte count: %d got: %zd\n", v->test_name,
                   (int) v->blocks[i], p, v->blocks[i], rv);
        }
        CU_ASSERT_FATAL(v->rv[i] == rv);

        p += v->blocks[i];
    }

    ping = v->ping;
    while (ping) {
        CU_ASSERT_FATAL(1 == ping->seen);
        ping->seen = 0;    /* reset the test */
        if (ping->more) {
            ping = &ping[1];
        } else {
            ping = NULL;
        }
    }

    pong = v->pong;
    while (pong) {
        CU_ASSERT_FATAL(1 == pong->seen);
        pong->seen = 0;    /* reset the test */
        if (pong->more) {
            pong = &pong[1];
        } else {
            pong = NULL;
        }
    }

    stream = v->stream;
    while (stream) {
        CU_ASSERT_FATAL(1 == stream->seen);
        stream->seen = 0;    /* reset the test */
        if (stream->more) {
            stream = &stream[1];
        } else {
            stream = NULL;
        }
    }

    close = v->close;
    while (close) {
        CU_ASSERT_FATAL(1 == close->seen);
        close->seen = 0;    /* reset the test */
        if (close->more) {
            close = &close[1];
        } else {
            close = NULL;
        }
    }
}


void test_null_in()
{
    struct test_vector tests[] = {
        {
            .test_name = "Test null block, zero len",
            .in = NULL,
            .blocks = (int[1]){ 0 },
            .rv = (size_t[1]){ 0 },
            .block_count = 1,

            .follow_redirects = false,
            .redirection = false,

            .ping = NULL,
            .pong = NULL,
            .stream = NULL,
            .close = NULL,
        },
        {
            .test_name = "Test null block, invalid len",
            .in = NULL,
            .blocks = (int[1]){ 1 },
            .rv = (size_t[1]){ 1 },
            .block_count = 1,

            .follow_redirects = false,
            .redirection = false,

            .ping = NULL,
            .pong = NULL,
            .stream = NULL,
            .close = NULL,
        },
        {
            .test_name = "Test 0 len block",
            .in = "ignored",
            .blocks = (int[1]){ 0 },
            .rv = (size_t[1]){ 0 },
            .block_count = 1,

            .follow_redirects = false,
            .redirection = false,

            .ping = NULL,
            .pong = NULL,
            .stream = NULL,
            .close = NULL,
        },
    };

    run_test( &tests[0] );
    run_test( &tests[1] );
    run_test( &tests[2] );
}


void test_redirection()
{
    struct test_vector tests[] = {
        {
            .test_name = "Test 1",
            .in = "\x89\x04ping",           /* PING with payload 'ping' */
            .blocks = (int[1]){ 6 },
            .rv = (size_t[1]){ 6 },
            .block_count = 1,

            .follow_redirects = true,
            .redirection = false,

            .ping = (struct mock_ping[1]) {
                {
                    .data = "ping",
                    .len = 4,
                    .seen = 0,
                    .more = 0,
                },
            },
            .pong = NULL,
            .stream = NULL,
            .close = NULL,
        }, {
            .test_name = "Redirection active, while accepted",
            .in = "\x89\x04ping",           /* PING with payload 'ping' */
            .blocks = (int[1]){ 6 },
            .rv = (size_t[1]){ 6 },
            .block_count = 1,

            .follow_redirects = true,
            .redirection = true,

            .ping = NULL,
            .pong = NULL,
            .stream = NULL,
            .close = NULL,
        },
    };

    run_test( &tests[0] );
    run_test( &tests[1] );
}


void test_simple()
{
    struct test_vector test = {
        .test_name = "Test 1",
        .in = "\x01\x00"                /* TEXT with no payload. */
              "\x00\x05hello"           /* CONT with 5 bytes of text */
              "\x89\x04ping"            /* PING with payload 'ping' */
              "\x8a\x04pong"            /* PONG with payload 'pong' */
              "\x80\x03ᚅ"               /* Final CONT with 3 bytes of text */
              "\x81\x04𒀺"               /* TEXT with 4 bytes of UTF8 text */
              "\x82\x05-_/-|"           /* BIN as a single packet */
              "\x82\x00"                /* BIN as a single packet */
              "\x81\x02hi"              /* TEXT as a single packet */
              "\x82\x05-_/-|"           /* BIN as a single packet */
              "\x88\x06\x03\xe8|bye"    /* Close as a single packet */
              "\x01\x07ignored",        /* Close as a single packet */
        .blocks = (int[12]){ 2, 7, 6, 6, 5, 6, 7, 2, 4, 7, 8, 9 },
        .rv = (size_t[12]){ 2, 7, 6, 6, 5, 6, 7, 2, 4, 7, 8, 9 },
        .block_count = 12,

        .follow_redirects = false,
        .redirection = false,

        .ping = (struct mock_ping[1]) {
            {
                .data = "ping",
                .len = 4,
                .seen = 0,
                .more = 0,
            },
        },
        .pong = (struct mock_pong[1]) {
            {
                .data = "pong",
                .len = 4,
                .seen = 0,
                .more = 0,
            },
        },
        .stream = (struct mock_stream[8]) {
            { .info = CWS_TEXT   | CWS_FIRST,            .len = 0, .data = NULL,    .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 5, .data = "hello", .seen = 0, .more = 1, },
            { .info = CWS_CONT               | CWS_LAST, .len = 3, .data = "ᚅ",     .seen = 0, .more = 1, },
            { .info = CWS_TEXT   | CWS_FIRST | CWS_LAST, .len = 4, .data = "𒀺",     .seen = 0, .more = 1, },
            { .info = CWS_BINARY | CWS_FIRST | CWS_LAST, .len = 5, .data = "-_/-|", .seen = 0, .more = 1, },
            { .info = CWS_BINARY | CWS_FIRST | CWS_LAST, .len = 0, .data = 0,       .seen = 0, .more = 1, },
            { .info = CWS_TEXT   | CWS_FIRST | CWS_LAST, .len = 2, .data = "hi",    .seen = 0, .more = 1, },
            { .info = CWS_BINARY | CWS_FIRST | CWS_LAST, .len = 5, .data = "-_/-|", .seen = 0, .more = 0, },
        },
        .close = NULL,
    };

    run_test( &test );
}

void test_more_complex()
{
    struct test_vector test = {
        .test_name = "Test 2",
        .in = "\x01\x00"                /* TEXT with no payload. */
              "\x00\x05hello"           /* CONT with 5 bytes of text */
              "\x89\x04ping"            /* PING with payload 'ping' */
              "\x8a\x04pong"            /* PONG with payload 'pong' */
              "\x80\x03ᚅ"               /* Final CONT with 3 bytes of UTF8 text */
              "\x81\x04𒀺"               /* TEXT with 4 bytes of UTF8 text */
              "\x82\x05-_/-|"           /* BIN as a single packet */
              "\x82\x00"                /* BIN as a single packet */
              "\x81\x02hi"              /* TEXT as a single packet */
              "\x82\x05-_/-|"           /* BIN as a single packet */
              "\x88\x06\x03\xe8|bye"    /* Close as a single packet */
              "\x01\x07ignored",        /* Close as a single packet */
        .blocks = (int[69]){ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                             1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                             1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                             1,1,1,1,1,1,1,1,1 },
        .rv = (size_t[69]){ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
                            1,1,1,1,1,1,1,1,1 },
        .block_count = 69,

        .follow_redirects = false,
        .redirection = false,

        .ping = (struct mock_ping[1]) {
            { .data = "ping", .len = 4, .seen = 0, .more = 0, },
        },
        .pong = (struct mock_pong[1]) {
            { .data = "pong", .len = 4, .seen = 0, .more = 0, },
        },
        .stream = (struct mock_stream[25]) {
            { .info = CWS_TEXT   | CWS_FIRST,            .len = 0, .data = NULL, .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "h",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "e",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "l",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "l",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "o",  .seen = 0, .more = 1, },
            { .info = CWS_CONT               | CWS_LAST, .len = 3, .data = "ᚅ",  .seen = 0, .more = 1, },
            { .info = CWS_TEXT   | CWS_FIRST,            .len = 0, .data = NULL, .seen = 0, .more = 1, },
            { .info = CWS_CONT               | CWS_LAST, .len = 4, .data = "𒀺",  .seen = 0, .more = 1, },
            { .info = CWS_BINARY | CWS_FIRST,            .len = 0, .data = NULL, .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "-",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "_",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "/",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "-",  .seen = 0, .more = 1, },
            { .info = CWS_CONT               | CWS_LAST, .len = 1, .data = "|",  .seen = 0, .more = 1, },
            { .info = CWS_BINARY | CWS_FIRST | CWS_LAST, .len = 0, .data = NULL, .seen = 0, .more = 1, },
            { .info = CWS_TEXT   | CWS_FIRST,            .len = 0, .data = NULL, .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "h",  .seen = 0, .more = 1, },
            { .info = CWS_CONT               | CWS_LAST, .len = 1, .data = "i",  .seen = 0, .more = 1, },
            { .info = CWS_BINARY | CWS_FIRST,            .len = 0, .data = NULL, .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "-",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "_",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "/",  .seen = 0, .more = 1, },
            { .info = CWS_CONT,                          .len = 1, .data = "-",  .seen = 0, .more = 1, },
            { .info = CWS_CONT               | CWS_LAST, .len = 1, .data = "|",  .seen = 0, .more = 0, },
        },
        .close = NULL,
    };

    run_test( &test );
}

void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "simple Tests",       .fn = test_simple        },
        { .label = "more complex Tests ", .fn = test_more_complex },
        { .label = "invalid input Tests", .fn = test_null_in      },
        { .label = "redirection Tests  ", .fn = test_redirection  },
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
