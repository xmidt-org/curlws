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

#include "../src/send.c"

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

void* mem_alloc_ctrl(pool_t *pool)
{
    (void) pool;
    return malloc(WS_CTL_PAYLOAD_MAX);
}

void* mem_alloc_data(pool_t *pool)
{
    (void) pool;
    return malloc(1024);
}

void mem_free(void *ptr)
{
    free(ptr);
}



void setup_test(CWS *priv)
{
    memset(priv, 0, sizeof(CWS));
    priv->max_payload_size = 1024;
    priv->mem_cfg.data_block_size = 1024 + WS_FRAME_HEADER_MAX;
    send_init(priv);
    srand(0);
}


void test_simple()
{
    CWS priv;
    //size_t rv;
    uint8_t buffer[40];
    char *payload = "hello";
    char *ping = "ping";

    uint8_t expect[] = {   0x81, 0x85,                   /* header */
                           0x01, 0x02, 0x03, 0x04,       /* mask */
                           0x69, 0x67, 0x6f, 0x68, 0x6e, /* 'hello' encoded */
                           0x89, 0x84,                   /* header */
                           0x01, 0x02, 0x03, 0x04,       /* mask */
                           0x71, 0x6b, 0x6d, 0x63,       /* 'ping' encoded */
                           0x8a, 0x84,                   /* header */
                           0x01, 0x02, 0x03, 0x04,       /* mask */
                           0x71, 0x6b, 0x6d, 0x63 };     /* 'ping' encoded */
    struct cws_frame f[] = {
        {
            .fin = 1,
            .mask = 1,
            .is_control = 0,
            .opcode = 1,
            .masking_key = {1, 2, 3, 4},
            .payload_len = 5,
            .payload = payload,
        },
        {
            .fin = 1,
            .mask = 1,
            .is_control = 1,
            .opcode = 9,
            .masking_key = {1, 2, 3, 4},
            .payload_len = 4,
            .payload = ping,
        },
        {
            .fin = 1,
            .mask = 1,
            .is_control = 1,
            .opcode = 0xa,
            .masking_key = {1, 2, 3, 4},
            .payload_len = 4,
            .payload = ping,
        },
    };

    setup_test(&priv);

    /* Ignore redirection */
    priv.redirection = true;
    CU_ASSERT(40 == _readfunction_cb((char*) buffer, 40, 1, &priv));
    priv.redirection = false;

    /* Test the empty send queue --> pause things behavior */
    CU_ASSERT(NULL == priv.send);
    CU_ASSERT(0 == priv.pause_flags);
    CU_ASSERT(CURL_READFUNC_PAUSE == _readfunction_cb((char*) buffer, 40, 1, &priv));
    CU_ASSERT(CURLPAUSE_SEND == priv.pause_flags);

    /* Send 2 frames in a large enough buffer - starting from paused state */
    priv.pause_flags = CURLPAUSE_SEND;
    CU_ASSERT(CWSE_OK == send_frame(&priv, &f[0]));
    CU_ASSERT(CWSE_OK == send_frame(&priv, &f[1]));
    CU_ASSERT(CWSE_OK == send_frame(&priv, &f[2]));
    CU_ASSERT(31 == _readfunction_cb((char*) buffer, 40, 1, &priv));

    for (size_t i = 0; i < sizeof(expect); i++) {
        CU_ASSERT(expect[i] == buffer[i]);
    }
}


void test_small_buffer()
{
    CWS priv;
    //size_t rv;
    uint8_t buffer[10];
    char *payload = "hello";
    char *ping = "ping";

    uint8_t expect[] = {   0x81, 0x85,                   /* header */
                           0x01, 0x02, 0x03, 0x04,       /* mask */
                           0x69, 0x67, 0x6f };           /* 'hel' encoded */
    struct cws_frame f[] = {
        {
            .fin = 1,
            .mask = 1,
            .is_control = 0,
            .opcode = 1,
            .masking_key = {1, 2, 3, 4},
            .payload_len = 5,
            .payload = payload,
        },
        {
            .fin = 1,
            .mask = 1,
            .is_control = 1,
            .opcode = 9,
            .masking_key = {1, 2, 3, 4},
            .payload_len = 4,
            .payload = ping,
        },
    };

    setup_test(&priv);

    CU_ASSERT(CWSE_OK == send_frame(&priv, &f[0]));
    CU_ASSERT(10 == _readfunction_cb((char*) buffer, 10, 1, &priv));

    for (size_t i = 0; i < sizeof(expect); i++) {
        CU_ASSERT(expect[i] == buffer[i]);
    }

    /* There is a buffer that is not freed. */
    free(priv.send);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "simple cb Tests", .fn = test_simple        },
        { .label = "simple small buffer", .fn = test_small_buffer        },
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
