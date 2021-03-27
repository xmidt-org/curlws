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

#include "../src/memory.h"

void test_silly()
{
    pool_t *pool;

    pool = mem_init_pool(NULL);
    CU_ASSERT(NULL == pool);

    mem_cleanup_pool(NULL);
}

void test_basic()
{
    void *ctrl, *ctrl_last, *data, *data_last;
    pool_t *pool;
    struct mem_pool_config cfg = {
        .control_block_size = 128,
        .data_block_size = 4096
    };

    pool = mem_init_pool(&cfg);
    CU_ASSERT(NULL != pool);

    ctrl = mem_alloc_ctrl(pool);
    CU_ASSERT(NULL != ctrl);
    memset(ctrl, 5, 128);
    ctrl_last = ctrl;

    data = mem_alloc_data(pool);
    CU_ASSERT(NULL != data);
    memset(data, 9, 4096);
    data_last = data;

    mem_free(ctrl);
    mem_free(data);

    ctrl = mem_alloc_ctrl(pool);
    CU_ASSERT(ctrl == ctrl_last);

    data = mem_alloc_data(pool);
    CU_ASSERT(data == data_last);

    mem_cleanup_pool(pool);
}

void test_lots()
{
    void *c[10];
    void *d[10];
    int i;

    pool_t *pool;
    struct mem_pool_config cfg = {
        .control_block_size = 128,
        .data_block_size = 4096
    };

    pool = mem_init_pool(&cfg);
    CU_ASSERT(NULL != pool);

    for (i = 0; i < 10; i++) {
        c[i] = mem_alloc_ctrl(pool);
        CU_ASSERT(NULL != c[i]);

        d[i] = mem_alloc_data(pool);
        CU_ASSERT(NULL != d[i]);
    }

    mem_free(c[4]);
    mem_free(c[4]);
    mem_free(d[3]);

    for (i = 0; i < 10; i++) {
        mem_free(c[i]);
        mem_free(d[i]);
    }

    for (i = 0; i < 10; i++) {
        c[i] = mem_alloc_ctrl(pool);
        CU_ASSERT(NULL != c[i]);

        d[i] = mem_alloc_data(pool);
        CU_ASSERT(NULL != d[i]);
    }

    mem_free(c[4]);

    mem_cleanup_pool(pool);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "Basic Tests", .fn = test_basic },
        { .label = "Silly Tests", .fn = test_silly },
        { .label = "Lots Tests", .fn = test_lots },
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
