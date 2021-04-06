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


struct mock_cws_close {
    int code;
    const char *reason;
    size_t len;
    int seen;
    CWScode rv;

    struct mock_cws_close *next;
};
static struct mock_cws_close *__cws_close_goal = NULL;
CWScode cws_close(CWS *priv, int code, const char *reason, size_t len)
{
    CWScode rv;

    printf( "cws_close( ... code: %d, reason: '%.*s', len: %ld )\n", code, (int) len, reason, len );

    CU_ASSERT_FATAL(NULL != __cws_close_goal);
    CU_ASSERT(NULL != priv);
    priv->closed = true;
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
    __cws_close_goal = __cws_close_goal->next;

    return rv;
}


struct mock_stream {
    int info;
    const char *data;
    size_t len;
    int seen;
    struct mock_stream *next;
};

static struct mock_stream *__on_stream_goal = NULL;
static void on_stream(void *data, CWS *handle, int info, const void *buffer, size_t len)
{
    (void) data;

    //printf("on_stream( ... info: %08x, buffer: 0x%p, len: %ld )\n", info, buffer, len);

    CU_ASSERT_FATAL(NULL != __on_stream_goal);
    CU_ASSERT(NULL != handle);

    CU_ASSERT(__on_stream_goal->info == info);
    if (__on_stream_goal->info != info) {
        printf("info: want: %08x, got: %08x\n", __on_stream_goal->info, info);
    }

    CU_ASSERT(__on_stream_goal->len == len);
    if (__on_stream_goal->len != len) {
        printf("len: want: %ld, got: %ld\n", __on_stream_goal->len, len);
    }

    if (NULL != __on_stream_goal->data) {
        for (size_t i = 0; i < len; i++) {
            const char *c = (const char*) buffer;

            if (__on_stream_goal->data[i] != c[i]) {
                printf("data[%zd]: want: '%c' (0x%02x), got: '%c' (0x%02x)\n",
                        i, __on_stream_goal->data[i], __on_stream_goal->data[i],
                        c[i], c[i]);
            }
            CU_ASSERT(__on_stream_goal->data[i] == c[i]);
        }
    }
    __on_stream_goal->seen++;
    __on_stream_goal = __on_stream_goal->next;
}


struct mock_close {
    int code;
    const char *reason;
    size_t len;
    int seen;

    struct mock_close *next;
};

static struct mock_close *__on_close_goal = NULL;
static void on_close(void *data, CWS *handle, int code, const char *reason, size_t len)
{
    (void) data;

    //printf( "on_close( ... code: %d, reason: '%.*s', len: %lu )\n", code, (int) len, reason, len );

    CU_ASSERT_FATAL(NULL != __on_close_goal);
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
    __on_close_goal = __on_close_goal->next;
}


struct mock_piong {
    const void *data;
    size_t len;
    int seen;

    struct mock_piong *next;
};

static struct mock_piong *__on_ping_goal = NULL;
static void on_ping(void *data, CWS *handle, const void *buffer, size_t len)
{
    (void) data;

    //printf( "on_ping( ... buffer: 0x%p, len: %ld )\n", buffer, len );

    CU_ASSERT_FATAL(NULL != __on_ping_goal);
    CU_ASSERT(NULL != handle);
    CU_ASSERT(__on_ping_goal->len == len);
    if (NULL != __on_ping_goal->data) {
        for (size_t i = 0; i < len; i++) {
            CU_ASSERT(((uint8_t*)__on_ping_goal->data)[i] == ((uint8_t*)buffer)[i]);
        }
    }
    __on_ping_goal->seen++;
    __on_ping_goal = __on_ping_goal->next;
}


static struct mock_piong *__on_pong_goal = NULL;
static void on_pong(void *data, CWS *handle, const void *buffer, size_t len)
{
    (void) data;

    //printf( "on_pong( ... buffer: 0x%p, len: %ld )\n", buffer, len );

    CU_ASSERT_FATAL(NULL != __on_pong_goal);
    CU_ASSERT(NULL != handle);
    CU_ASSERT(__on_pong_goal->len == len);
    if (NULL != __on_pong_goal->data) {
        for (size_t i = 0; i < len; i++) {
            CU_ASSERT(((uint8_t*)__on_pong_goal->data)[i] == ((uint8_t*)buffer)[i]);
        }
    }
    __on_pong_goal->seen++;
    __on_pong_goal = __on_pong_goal->next;
}

void setup_test(CWS *priv)
{
    memset(priv, 0, sizeof(CWS));
    receive_init(priv);
    priv->on_stream_fn = on_stream;
    priv->on_close_fn = on_close;
    priv->on_ping_fn = on_ping;
    priv->on_pong_fn = on_pong;
}


void test_simple()
{
    CWS priv;
    uint8_t test[] = { 0x81, 0x00 };
    struct mock_stream vector[] = {
        {
            .info = CWS_TEXT | CWS_FIRST_FRAME | CWS_LAST_FRAME,
            .len = 0,
            .data = NULL,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_stream_goal = vector;
    CU_ASSERT(2 == _writefunction_cb((const char*) test, 2, 1, &priv));
    CU_ASSERT(1 == vector[0].seen);
}


void test_simple_one_byte()
{
    CWS priv;
    uint8_t test[] = { 0x81, 0x01, 'a' };
    struct mock_stream vector[] = {
        {
            .info = CWS_TEXT | CWS_FIRST_FRAME | CWS_LAST_FRAME,
            .len = 1,
            .data = "a",
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_stream_goal = vector;
    CU_ASSERT(3 == _writefunction_cb((const char*) test, 3, 1, &priv));
    CU_ASSERT(1 == vector[0].seen);
}


void test_simple_one_byte_binary()
{
    CWS priv;
    uint8_t test[] = { 0x82, 0x01, 'a' };
    struct mock_stream vector[] = {
        {
            .info = CWS_BINARY | CWS_FIRST_FRAME | CWS_LAST_FRAME,
            .len = 1,
            .data = "a",
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_stream_goal = vector;
    CU_ASSERT(3 == _writefunction_cb((const char*) test, 3, 1, &priv));
    CU_ASSERT(1 == vector[0].seen);
}


void test_invalid_frame_test()
{
    CWS priv;
    uint8_t test[] = { 0x72, 0x01, 'a' };
    struct mock_cws_close cws_close_vector[] = {
        {
            .code = 1002,
            .reason = NULL,
            .len = 0,
            .seen = 0,
            .next = NULL,
        }
    };


    setup_test(&priv);

    __cws_close_goal = cws_close_vector;
    CU_ASSERT(0 == _writefunction_cb((const char*) test, 3, 1, &priv));
    CU_ASSERT(1 == cws_close_vector[0].seen);
}


void test_simple_stream()
{
    CWS priv;
    uint8_t test[] = { 0x81, 0x1f, '0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', '0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', '0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', '8' };
    const char *p;

    struct mock_stream vector[] = {
        {
            .info = CWS_TEXT | CWS_FIRST_FRAME,
            .len = 8,
            .data = "01234567",
            .seen = 0,
            .next = NULL,
        },
        {
            .info = CWS_CONT,
            .len = 10,
            .data = "8901234567",
            .seen = 0,
            .next = NULL,
        },
        {
            .info = CWS_CONT,
            .len = 10,
            .data = "8901234567",
            .seen = 0,
            .next = NULL,
        },
        {
            .info = CWS_CONT | CWS_LAST_FRAME,
            .len = 3,
            .data = "898",
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    vector[0].next = &vector[1];
    vector[1].next = &vector[2];
    vector[2].next = &vector[3];

    __on_stream_goal = vector;
    p = (const char*) test;
    CU_ASSERT(1 == _cws_process_frame(&priv, &p[0], 1));
    CU_ASSERT(1 == _cws_process_frame(&priv, &p[1], 1));
    CU_ASSERT(8 == _cws_process_frame(&priv, &p[2], 8));
    CU_ASSERT(10 == _cws_process_frame(&priv, &p[10], 10));
    CU_ASSERT(10 == _cws_process_frame(&priv, &p[20], 10));
    CU_ASSERT(3 == _cws_process_frame(&priv, &p[30], 3));
    CU_ASSERT(1 == vector[0].seen);
    CU_ASSERT(1 == vector[1].seen);
    CU_ASSERT(1 == vector[2].seen);
    CU_ASSERT(1 == vector[3].seen);
}


void test_simple_close()
{
    CWS priv;
    uint8_t test[] = { 0x88, 0x0c, 0x03, 0xe9, 'G', 'o', 'i', 'n', 'g', ' ', 'A', 'w', 'a', 'y' };
    struct mock_close vector[] = {
        {
            .len = 10,
            .code = 1001,
            .reason = "Going Away",
            .seen = 0,
            .next = NULL,
        }
    };
    struct mock_cws_close cws_close_vector[] = {
        {
            .code = 1001,
            .reason = NULL,
            .len = 0,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_close_goal = vector;
    __cws_close_goal = cws_close_vector;
    CU_ASSERT(2 == _cws_process_frame(&priv, (const char*) &test[0], 2));
    CU_ASSERT(6 == _cws_process_frame(&priv, (const char*) &test[2], 6));
    CU_ASSERT(6 == _cws_process_frame(&priv, (const char*) &test[8], 6));
    CU_ASSERT(1 == vector[0].seen);
    CU_ASSERT(1 == cws_close_vector[0].seen);
}

void test_empty_close()
{
    CWS priv;
    uint8_t test[] = { 0x88, 0x00 };
    struct mock_close vector[] = {
        {
            .len = 0,
            .code = 1005,
            .reason = NULL,
            .seen = 0,
            .next = NULL,
        }
    };
    struct mock_cws_close cws_close_vector[] = {
        {
            .code = 0,
            .reason = NULL,
            .len = 0,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_close_goal = vector;
    __cws_close_goal = cws_close_vector;
    CU_ASSERT(2 == _cws_process_frame(&priv, (const char*) &test[0], 2));
    CU_ASSERT(1 == vector[0].seen);
    CU_ASSERT(1 == cws_close_vector[0].seen);
}

void test_bad_length_close()
{
    CWS priv;
    uint8_t test[] = { 0x88, 0x01, 0x12 };
    struct mock_close vector[] = {
        {
            .len = 0,
            .code = 1002,
            .reason = NULL,
            .seen = 0,
            .next = NULL,
        }
    };
    struct mock_cws_close cws_close_vector[] = {
        {
            .code = 1002,
            .reason = "invalid close payload length",
            .len = SIZE_MAX,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_close_goal = vector;
    __cws_close_goal = cws_close_vector;
    CU_ASSERT(3 == _cws_process_frame(&priv, (const char*) &test[0], 3));
    CU_ASSERT(1 == vector[0].seen);
    CU_ASSERT(1 == cws_close_vector[0].seen);
}

void test_bad_reason_close()
{
    CWS priv;
    uint8_t test[] = { 0x88, 0x02, 0x00, 0xff };
    struct mock_close vector[] = {
        {
            .len = 0,
            .code = 1002,
            .reason = NULL,
            .seen = 0,
            .next = NULL,
        }
    };
    struct mock_cws_close cws_close_vector[] = {
        {
            .code = 1002,
            .reason = "invalid close reason",
            .len = SIZE_MAX,
            .seen = 0,
            .next = NULL,
        }
    };


    setup_test(&priv);

    __on_close_goal = vector;
    __cws_close_goal = cws_close_vector;
    CU_ASSERT(4 == _cws_process_frame(&priv, (const char*) &test[0], 4));
    CU_ASSERT(1 == vector[0].seen);
    CU_ASSERT(1 == cws_close_vector[0].seen);
}


void test_ping()
{
    CWS priv;
    uint8_t test[] = { 0x89, 0x00 };
    struct mock_piong vector[] = {
        {
            .len = 0,
            .data = NULL,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_ping_goal = vector;
    CU_ASSERT(2 == _cws_process_frame(&priv, (const char*) &test[0], 2));
    CU_ASSERT(1 == vector[0].seen);
}

void test_pong()
{
    CWS priv;
    uint8_t test[] = { 0x8a, 0x00 };
    struct mock_piong vector[] = {
        {
            .len = 0,
            .data = NULL,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_pong_goal = vector;
    CU_ASSERT(2 == _cws_process_frame(&priv, (const char*) &test[0], 2));
    CU_ASSERT(1 == vector[0].seen);
}

void test_close_codes()
{
    CU_ASSERT(false == _is_valid_close_from_server( 999));
    CU_ASSERT(true  == _is_valid_close_from_server(1000));
    CU_ASSERT(true  == _is_valid_close_from_server(1001));
    CU_ASSERT(true  == _is_valid_close_from_server(1002));
    CU_ASSERT(true  == _is_valid_close_from_server(1003));
    CU_ASSERT(false == _is_valid_close_from_server(1004));
    CU_ASSERT(false == _is_valid_close_from_server(1005));
    CU_ASSERT(false == _is_valid_close_from_server(1006));
    CU_ASSERT(true  == _is_valid_close_from_server(1007));
    CU_ASSERT(true  == _is_valid_close_from_server(1008));
    CU_ASSERT(true  == _is_valid_close_from_server(1009));
    CU_ASSERT(false == _is_valid_close_from_server(1010));
    CU_ASSERT(true  == _is_valid_close_from_server(1011));
    CU_ASSERT(false == _is_valid_close_from_server(1012));
    CU_ASSERT(false == _is_valid_close_from_server(1013));
    CU_ASSERT(false == _is_valid_close_from_server(1014));
    CU_ASSERT(false == _is_valid_close_from_server(1015));
    CU_ASSERT(false == _is_valid_close_from_server(1016));
    CU_ASSERT(false == _is_valid_close_from_server(2999));
    CU_ASSERT(true  == _is_valid_close_from_server(3000));
    CU_ASSERT(true  == _is_valid_close_from_server(3999));
    CU_ASSERT(true  == _is_valid_close_from_server(4000));
    CU_ASSERT(true  == _is_valid_close_from_server(4999));
    CU_ASSERT(false == _is_valid_close_from_server(5000));
}

void test_exit_cases_for_writefunction()
{
    CWS priv;
    uint8_t test[] = { 0x89, 0x00 };
    struct mock_piong vector[] = {
        {
            .len = 0,
            .data = NULL,
            .seen = 0,
            .next = NULL,
        }
    };

    setup_test(&priv);

    __on_ping_goal = vector;


    /* Redirecting ... we just ignore that data. */
    priv.redirection = true;
    CU_ASSERT(2 == _writefunction_cb((const char*) test, 2, 1, &priv));
    priv.redirection = false;

    /* closed ... we consume it all and exit. */
    priv.closed = true;
    CU_ASSERT(2 == _writefunction_cb((const char*) test, 2, 1, &priv));
    priv.closed = false;

    CU_ASSERT(0 == vector[0].seen);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "simple text Tests        ", .fn = test_simple        },
        { .label = "simple text 1 byte Test  ", .fn = test_simple_one_byte },
        { .label = "simple binary 1 byte Test", .fn = test_simple_one_byte_binary },
        { .label = "simple text stream Tests ", .fn = test_simple_stream },
        { .label = "simple close Tests       ", .fn = test_simple_close },
        { .label = "empty close Tests        ", .fn = test_empty_close  },
        { .label = "bad length close Tests   ", .fn = test_bad_length_close  },
        { .label = "bad reason close Tests   ", .fn = test_bad_reason_close  },
        { .label = "test ping                ", .fn = test_ping  },
        { .label = "test pong                ", .fn = test_pong  },
        { .label = "test close codes         ", .fn = test_close_codes  },
        { .label = "invalid frame Test       ", .fn = test_invalid_frame_test },
        { .label = "exit cases for _writefn  ", .fn = test_exit_cases_for_writefunction },
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
