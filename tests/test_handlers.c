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

#include "../src/handlers.h"
#include "../src/internal.h"

#include "../src/handlers.c"

/*----------------------------------------------------------------------------*/
/*                               Mock Functions                               */
/*----------------------------------------------------------------------------*/
CWScode cws_pong(CWS *handle, const void *data, size_t len)
{
    IGNORE_UNUSED(handle);
    IGNORE_UNUSED(data);
    IGNORE_UNUSED(len);

    return CWSE_OK;
}


/*----------------------------------------------------------------------------*/
/*                               Test Functions                               */
/*----------------------------------------------------------------------------*/
void test_populate_callbacks()
{
    CWS obj;
    struct cws_config src;

    memset(&obj, 0, sizeof(obj));
    memset(&src, 0, sizeof(src));

    populate_callbacks(&obj, &src);
    CU_ASSERT(obj.on_connect_fn == _default_on_connect);
    CU_ASSERT(obj.on_text_fn    == _default_on_text);
    CU_ASSERT(obj.on_binary_fn  == _default_on_binary);
    CU_ASSERT(obj.on_ping_fn    == _default_on_ping);
    CU_ASSERT(obj.on_pong_fn    == _default_on_pong);
    CU_ASSERT(obj.on_close_fn   == _default_on_close);
    CU_ASSERT(obj.get_random_fn == _default_get_random);
    CU_ASSERT(obj.debug_fn      == _default_debug);
    CU_ASSERT(obj.configure_fn  == _default_configure);

    src.verbose = 1;
    populate_callbacks(&obj, &src);
    CU_ASSERT(obj.debug_fn == _default_verbose_debug);

    src.on_connect = (void*) 1;
    src.on_text    = (void*) 2;
    src.on_binary  = (void*) 3;
    src.on_stream  = (void*) 4;
    src.on_ping    = (void*) 5;
    src.on_pong    = (void*) 6;
    src.on_close   = (void*) 7;
    src.get_random = (void*) 8;
    src.debug      = (void*) 9;
    src.configure  = (void*) 10;

    populate_callbacks(&obj, &src);
    CU_ASSERT(obj.on_connect_fn == (void*) 1);
    CU_ASSERT(obj.on_text_fn    == (void*) 2);
    CU_ASSERT(obj.on_binary_fn  == (void*) 3);
    CU_ASSERT(obj.on_stream_fn  == (void*) 4);
    CU_ASSERT(obj.on_ping_fn    == (void*) 5);
    CU_ASSERT(obj.on_pong_fn    == (void*) 6);
    CU_ASSERT(obj.on_close_fn   == (void*) 7);
    CU_ASSERT(obj.get_random_fn == (void*) 8);
    CU_ASSERT(obj.debug_fn      == (void*) 9);
    CU_ASSERT(obj.configure_fn  == (void*) 10);

}


void test_defaults_dont_crash()
{
    CWS obj;
    struct cws_config src;

    memset(&obj, 0, sizeof(obj));
    memset(&src, 0, sizeof(src));

    populate_callbacks(&obj, NULL);

    (obj.on_connect_fn)(NULL, NULL, NULL);
    (obj.on_text_fn)(NULL, NULL, NULL, 0);
    (obj.on_binary_fn)(NULL, NULL, NULL, 0);
    (obj.on_stream_fn)(NULL, NULL, 0, NULL, 0);
    (obj.on_ping_fn)(NULL, NULL, NULL, 0);
    (obj.on_pong_fn)(NULL, NULL, NULL, 0);
    (obj.on_close_fn)(NULL, NULL, 0, NULL, 0);
    (obj.get_random_fn)(NULL, NULL, NULL, 0);
    (obj.configure_fn)(NULL, NULL, NULL);
    (obj.debug_fn)(NULL, NULL, "Hello, world.");

    /* Check the verbose debug callback */
    src.verbose = 3;
    populate_callbacks(&obj, &src);
    (obj.debug_fn)(NULL, NULL, "Hello, world.");
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


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "Test populate_callbacks()",  .fn = test_populate_callbacks  },
        { .label = "Test _default_get_random()", .fn = test_default_random      },
        { .label = "Test defaults don't crash",  .fn = test_defaults_dont_crash },
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
