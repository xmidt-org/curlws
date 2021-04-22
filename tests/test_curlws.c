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


void test_create_destroy()
{
    CWS *ws;
    struct cws_config cfg;

    memset(&cfg, 0, sizeof(cfg));

    ws = cws_create(NULL);
    CU_ASSERT(NULL == ws);

    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* A minimal, but reasonable case. */
    cfg.url = "wss://example.com";
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Validate the expected header based on a fixed random number */
    srand( 1000 );
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    CU_ASSERT_STRING_EQUAL(ws->expected_key_header, "4522FSSIYHASSkUaUouiInl8Cvk=");
    cws_destroy(ws);

    /* Invalid redirect count */
    cfg.max_redirects = -2;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* Valid redirect count (unlimited) */
    cfg.max_redirects = -1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Any interface value is accepted. */
    cfg.interface = "not really valid";
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Invalid verbose options */
    cfg.verbose = -1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    cfg.verbose = 12;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* Valid options */
    cfg.verbose = 1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    cfg.verbose = 2;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    cfg.verbose = 3;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    cfg.verbose = 0;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Invalid ip option */
    cfg.ip_version = -1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    cfg.ip_version = 5;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* Valid ip options */
    cfg.ip_version = 4;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    cfg.ip_version = 6;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Invalid security option */
    cfg.insecure_ok = 1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* Valid security option - no security */
    cfg.insecure_ok = CURLWS_INSECURE_MODE;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    cfg.insecure_ok = 0;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Websocket_protocols are not NULL */
    cfg.websocket_protocols = "chat";
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    CU_ASSERT_STRING_EQUAL(ws->cfg.ws_protocols_requested, "chat");
    cws_destroy(ws);
    cfg.websocket_protocols = NULL;

    /* Explicit expect invalid */
    cfg.explicit_expect = -1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    /* Valid explicit expect */
    cfg.explicit_expect = 1;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Valid max payload size */
    cfg.max_payload_size = 10;
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);
    cfg.max_payload_size = 0;

    /* Valid headers */
    cfg.extra_headers = curl_slist_append(cfg.extra_headers, "Safe: header");
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    cfg.extra_headers = curl_slist_append(cfg.extra_headers, "Extra-Safe: header");
    ws = cws_create(&cfg);
    CU_ASSERT(NULL != ws);
    cws_destroy(ws);

    /* Invalid headers */
    cfg.extra_headers = curl_slist_append(cfg.extra_headers, "Expect: header");
    ws = cws_create(&cfg);
    CU_ASSERT(NULL == ws);

    curl_slist_free_all(cfg.extra_headers);
    cfg.extra_headers = NULL;

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
        { .label = "create/destroy Tests", .fn = test_create_destroy              },
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
