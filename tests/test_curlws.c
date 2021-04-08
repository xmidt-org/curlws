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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CUnit/Basic.h>
#include <curl/curl.h>

#include "../src/curlws.h"
#include "../src/curlws.c"

void cws_random(CWS *priv, void *buffer, size_t len)
{
    uint8_t *bytes = buffer;
    size_t i;

    IGNORE_UNUSED(priv);

    /* Note that this does NOT need to be a crypto level randomization function
     * but is simply used to prevent intermediary caches from causing issues. */
    for (i = 0; i < len; i++) {
        bytes[i] = (0x0ff & rand());
    }
}

void test_cws_add_websocket_protocols()
{
    CWS obj;
    struct curl_slist *headers = NULL;
    struct curl_slist *rv = NULL;

    memset(&obj, 0, sizeof(obj));

    rv = _cws_add_websocket_protocols(&obj, headers, NULL);
    CU_ASSERT(NULL == rv);

    rv = _cws_add_websocket_protocols(&obj, headers, "chat");
    CU_ASSERT(NULL != rv);
    CU_ASSERT_STRING_EQUAL(obj.websocket_protocols.requested, "chat");
    free(obj.websocket_protocols.requested);
    obj.websocket_protocols.requested = NULL;

    CU_ASSERT_STRING_EQUAL(rv->data, "Sec-WebSocket-Protocol: chat");
    CU_ASSERT(NULL == rv->next);

    curl_slist_free_all(rv);
}


void test_calculate_websocket_key()
{
    CWS obj;
    struct curl_slist *headers = NULL;
    struct curl_slist *rv = NULL;

    memset(&obj, 0, sizeof(obj));

    srand( 1000 );

    rv = _cws_calculate_websocket_key(&obj, headers);
    CU_ASSERT(NULL != rv);

    //printf("\n\nGot: '%s'\n", headers->data);
    //printf("Got: '%s'\n", obj.expected_key_header);

    CU_ASSERT_STRING_EQUAL(rv->data, "Sec-WebSocket-Key: tluJnnQlu3K8f3LD4vsxcQ==");
    CU_ASSERT(NULL == rv->next);

    /* TODO: Validate this pair is correct. */
    CU_ASSERT_STRING_EQUAL(obj.expected_key_header, "4522FSSIYHASSkUaUouiInl8Cvk=");


    curl_slist_free_all(rv);
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


void test_add_headers()
{
    struct cws_config src;
    int found;
    struct curl_slist *rv = NULL;
    struct curl_slist *headers = NULL;
    const char *list[] = {
        "cat: 1",
        "dog: 2"
    };

    memset(&src, 0, sizeof(src));

    rv = _cws_add_headers(headers, &src, NULL, 0);
    CU_ASSERT(NULL == rv);

    src.extra_headers = curl_slist_append(src.extra_headers, "Header-A: 123");
    src.extra_headers = curl_slist_append(src.extra_headers, "Header-B: 654");
    rv = _cws_add_headers(headers, &src, list, sizeof(list)/sizeof(char*));
    CU_ASSERT(NULL != rv);

    headers = rv;
    found = 0;
    while (rv) {
        if (0 == strcmp(rv->data, "Header-A: 123")) {
            found |= 1;
        }
        if (0 == strcmp(rv->data, "Header-B: 654")) {
            found |= 2;
        }
        if (0 == strcmp(rv->data, "cat: 1")) {
            found |= 4;
        }
        if (0 == strcmp(rv->data, "dog: 2")) {
            found |= 8;
        }
        rv = rv->next;
    }
    CU_ASSERT(0x0f == found);
    curl_slist_free_all(headers);
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
        { .label = "_cws_calculate_websocket_key() Tests", .fn = test_calculate_websocket_key     },
        { .label = "_validate_config() Tests",             .fn = test_validate_config             },
        { .label = "_cws_add_headers() Tests",             .fn = test_add_headers                 },
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
