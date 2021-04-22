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
    CWS priv;
    struct cws_config src;

    memset(&priv, 0, sizeof(priv));
    memset(&src, 0, sizeof(src));

    populate_callbacks(&priv.cb, &src);
    CU_ASSERT(priv.cb.on_connect_fn  == NULL);
    CU_ASSERT(priv.cb.on_text_fn     == NULL);
    CU_ASSERT(priv.cb.on_binary_fn   == NULL);
    CU_ASSERT(priv.cb.on_fragment_fn == _default_on_fragment);
    CU_ASSERT(priv.cb.on_ping_fn     == _default_on_ping);
    CU_ASSERT(priv.cb.on_pong_fn     == NULL);
    CU_ASSERT(priv.cb.on_close_fn    == NULL);
    CU_ASSERT(priv.cb.configure_fn   == NULL);

    populate_callbacks(&priv.cb, &src);

    src.on_connect  = (void (*)(void*, CWS*, const char*)) 1;
    src.on_text     = (void (*)(void*, CWS*, const char*, size_t)) 2;
    src.on_binary   = (void (*)(void*, CWS*, const void*, size_t)) 3;
    src.on_fragment = (void (*)(void*, CWS*, int, const void*, size_t)) 4;
    src.on_ping     = (void (*)(void*, CWS*, const void*, size_t)) 5;
    src.on_pong     = (void (*)(void*, CWS*, const void*, size_t)) 6;
    src.on_close    = (void (*)(void*, CWS*, int, const char*, size_t)) 7;
    src.configure   = (void (*)(void*, CWS*, CURL*)) 8;

    populate_callbacks(&priv.cb, &src);
    CU_ASSERT(priv.cb.on_connect_fn  == (void (*)(void*, CWS*, const char*)) 1);
    CU_ASSERT(priv.cb.on_text_fn     == (void (*)(void*, CWS*, const char*, size_t)) 2);
    CU_ASSERT(priv.cb.on_binary_fn   == (void (*)(void*, CWS*, const void*, size_t)) 3);
    CU_ASSERT(priv.cb.on_fragment_fn == (void (*)(void*, CWS*, int, const void*, size_t)) 4);
    CU_ASSERT(priv.cb.on_ping_fn     == (void (*)(void*, CWS*, const void*, size_t)) 5);
    CU_ASSERT(priv.cb.on_pong_fn     == (void (*)(void*, CWS*, const void*, size_t)) 6);
    CU_ASSERT(priv.cb.on_close_fn    == (void (*)(void*, CWS*, int, const char*, size_t)) 7);
    CU_ASSERT(priv.cb.configure_fn   == (void (*)(void*, CWS*, CURL*)) 8);

}


void test_defaults_dont_crash()
{
    CWS priv;
    struct cws_config src;

    memset(&priv, 0, sizeof(priv));
    memset(&src, 0, sizeof(src));

    populate_callbacks(&priv.cb, NULL);

    cb_on_connect(&priv, NULL);
    cb_on_text(&priv, NULL, 0);
    cb_on_binary(&priv, NULL, 0);
    cb_on_fragment(&priv, 0, NULL, 0);
    cb_on_ping(&priv, NULL, 0);
    cb_on_pong(&priv, NULL, 0);
    cb_on_close(&priv, 0, NULL, 0);
    cb_on_close(&priv, 0, "include a string", SIZE_MAX);
    cb_on_close(&priv, 0, "include a string", 0);

    /* Check the verbose debug callback */
    priv.cfg.verbose = 3;
    populate_callbacks(&priv.cb, &src);

    /* Make sure nothing crashes if we're in verbose mode. */
    cb_on_connect(&priv, NULL);
    cb_on_text(&priv, NULL, 0);
    cb_on_binary(&priv, NULL, 0);
    cb_on_fragment(&priv, 0, NULL, 0);
    cb_on_ping(&priv, NULL, 0);
    cb_on_pong(&priv, NULL, 0);
    cb_on_close(&priv, 0, NULL, 0);
    cb_on_close(&priv, 0, "include a string", SIZE_MAX);
    cb_on_close(&priv, 0, "include a string", 0);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "Test populate_callbacks()",  .fn = test_populate_callbacks  },
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
