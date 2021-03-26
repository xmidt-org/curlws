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
#include <stdbool.h>

#include "../src/curlws.h"
#include "../src/curlws.c"


void test_cws_add_websocket_protocols()
{
    CWS obj;

    memset(&obj, 0, sizeof(obj));

    CU_ASSERT(true == _cws_add_websocket_protocols(&obj, NULL));
    CU_ASSERT(NULL == obj.headers);

    CU_ASSERT(true == _cws_add_websocket_protocols(&obj, "chat"));
    CU_ASSERT(NULL != obj.headers);
    CU_ASSERT_STRING_EQUAL(obj.websocket_protocols.requested, "chat");
    free(obj.websocket_protocols.requested);
    obj.websocket_protocols.requested = NULL;

    CU_ASSERT_STRING_EQUAL(obj.headers->data, "Sec-WebSocket-Protocol: chat");
    CU_ASSERT(NULL == obj.headers->next);

    curl_slist_free_all(obj.headers);
    obj.headers = NULL;
}


void test_populate_callbacks()
{
    CWS obj;
    struct cws_config src;

    memset(&obj, 0, sizeof(obj));
    memset(&src, 0, sizeof(src));

    _populate_callbacks(&obj, &src);
    CU_ASSERT(obj.on_connect_fn == _default_on_connect);
    CU_ASSERT(obj.on_text_fn    == _default_on_text);
    CU_ASSERT(obj.on_binary_fn  == _default_on_binary);
    CU_ASSERT(obj.on_ping_fn    == _default_on_ping);
    CU_ASSERT(obj.on_pong_fn    == _default_on_pong);
    CU_ASSERT(obj.on_close_fn   == _default_on_close);
    CU_ASSERT(obj.get_random_fn == _default_get_random);
    CU_ASSERT(obj.debug_fn      == _default_debug);
    CU_ASSERT(obj.configure_fn  == _default_configure);

    src.verbose = true;
    _populate_callbacks(&obj, &src);
    CU_ASSERT(obj.debug_fn == _default_verbose_debug);

    src.on_connect = (void*) 1;
    src.on_text    = (void*) 2;
    src.on_binary  = (void*) 3;
    src.on_ping    = (void*) 4;
    src.on_pong    = (void*) 5;
    src.on_close   = (void*) 6;
    src.get_random = (void*) 7;
    src.debug      = (void*) 8;
    src.configure  = (void*) 9;

    _populate_callbacks(&obj, &src);
    CU_ASSERT(obj.on_connect_fn == (void*) 1);
    CU_ASSERT(obj.on_text_fn    == (void*) 2);
    CU_ASSERT(obj.on_binary_fn  == (void*) 3);
    CU_ASSERT(obj.on_ping_fn    == (void*) 4);
    CU_ASSERT(obj.on_pong_fn    == (void*) 5);
    CU_ASSERT(obj.on_close_fn   == (void*) 6);
    CU_ASSERT(obj.get_random_fn == (void*) 7);
    CU_ASSERT(obj.debug_fn      == (void*) 8);
    CU_ASSERT(obj.configure_fn  == (void*) 9);

}


void test_default_random()
{
    uint8_t buf[10];
    size_t i;
    uint8_t expect[10] = { 0xb6, 0x5b, 0x89, 0x9e, 0x74, 0x25, 0xbb, 0x72, 0, 0 };

    memset(buf, 0, sizeof(buf));
    srand( 1000 );

    _default_get_random(NULL, NULL, buf, sizeof(buf) - 2);

    for (i = 0; i < sizeof(buf); i++ ) {
        //printf( "%ld = 0x%02x ? 0x%02x\n", i, expect[i], buf[i] );
        CU_ASSERT(buf[i] == expect[i]);
    }
}


void test_calculate_websocket_key()
{
    CWS obj;

    memset(&obj, 0, sizeof(obj));

    srand( 1000 );
    obj.get_random_fn = _default_get_random;

    CU_ASSERT(true == _cws_calculate_websocket_key(&obj));

    /* Only works once. */
    CU_ASSERT(false == _cws_calculate_websocket_key(&obj));

    //printf("\n\nGot: '%s'\n", obj.headers->data);
    //printf("Got: '%s'\n", obj.expected_key_header);

    CU_ASSERT_STRING_EQUAL(obj.headers->data, "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==");
    CU_ASSERT(NULL == obj.headers->next);

    /* TODO: Validate this pair is correct. */
    CU_ASSERT_STRING_EQUAL(obj.expected_key_header, "4522FSSIYHASSkUaUouiInl8Cvk=");


    free(obj.expected_key_header);
    curl_slist_free_all(obj.headers);
    obj.headers = NULL;
}


void test_validate_config()
{
    struct cws_config src;

    memset(&src, 0, sizeof(src));

    CU_ASSERT(false == _validate_config(NULL));
    CU_ASSERT(false == _validate_config(&src));

    src.url = "http://example.com";
    CU_ASSERT(true == _validate_config(&src));

    src.max_redirects = -4;
    CU_ASSERT(false == _validate_config(&src));
    src.max_redirects = 0;

    src.ip_version = 4;
    CU_ASSERT(true == _validate_config(&src));
    src.ip_version = 6;
    CU_ASSERT(true == _validate_config(&src));
    src.ip_version = 9;
    CU_ASSERT(false == _validate_config(&src));
}


void test_add_extra_headers()
{
    CWS obj;
    struct cws_config src;
    int found;
    struct curl_slist *p;

    memset(&obj, 0, sizeof(obj));
    memset(&src, 0, sizeof(src));

    CU_ASSERT(true == _cws_add_extra_headers(&obj, &src));

    src.extra_headers = curl_slist_append(src.extra_headers, "Header-A: 123");
    src.extra_headers = curl_slist_append(src.extra_headers, "Header-B: 654");
    CU_ASSERT(true == _cws_add_extra_headers(&obj, &src));

    p = obj.headers;
    found = 0;
    while (p) {
        if (0 == strcmp(p->data, "Header-A: 123")) {
            found |= 1;
        }
        if (0 == strcmp(p->data, "Header-B: 654")) {
            found |= 2;
        }
        p = p->next;
    }
    CU_ASSERT(3 == found);
    curl_slist_free_all(obj.headers);
    curl_slist_free_all(src.extra_headers);
}


void test_create_destroy()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));

    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* A minimal, but reasonable case. */
    cfg.url = "wss://example.com";
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Simulate a failure during creation. */
    ws = (CWS*) calloc(1, sizeof(CWS));
    cws_destroy(ws);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "_cws_add_websocket_protocols() Tests", .fn = test_cws_add_websocket_protocols },
        { .label = "_populate_callbacks() Tests",          .fn = test_populate_callbacks          },
        { .label = "_default_get_random() Tests",          .fn = test_default_random              },
        { .label = "_cws_calculate_websocket_key() Tests", .fn = test_calculate_websocket_key     },
        { .label = "_validate_config() Tests",             .fn = test_validate_config             },
        { .label = "_cws_add_extra_headers() Tests",       .fn = test_add_extra_headers           },
        { .label = "create/destroy Tests",                 .fn = test_create_destroy              },
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
