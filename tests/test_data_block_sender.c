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

#include "../src/internal.h"
#include "../src/data_block_sender.h"
#include "../src/ws.h"

struct mock {
    CWScode rv;
    int options;
    const void *data;
    size_t len;
    int seen;

    struct mock *next;
};

static struct mock *__goal = NULL;
CWScode frame_sender_data(CWS *priv, int options, const void *data, size_t len)
{
    CWScode rv;

    rv = __goal->rv;
    CU_ASSERT(NULL != priv);
    CU_ASSERT(__goal->options == options);
    CU_ASSERT(__goal->len == len);
    if (NULL != __goal->data) {
        for (size_t i = 0; i < len; i++) {
            if (((uint8_t*)__goal->data)[i] != ((uint8_t*)data)[i]) {
                printf("%ld expect: '%c' != '%c'\n", i, ((const char*)__goal->data)[i], ((const char*)data)[i]);
            }
            CU_ASSERT(((uint8_t*)__goal->data)[i] == ((uint8_t*)data)[i]);
        }
    } else {
        CU_ASSERT(NULL == data);
    }
    __goal->seen++;
    __goal = __goal->next;

    return rv;
}

void test_data_block_sender()
{
    CWS priv;

    memset(&priv, 0, sizeof(CWS));
    priv.cfg.max_payload_size = 10;

    CU_ASSERT(CWSE_INVALID_OPTIONS == data_block_sender(&priv, -1, NULL, 0));
    CU_ASSERT(CWSE_INVALID_OPTIONS == data_block_sender(&priv, CWS_BINARY|CWS_TEXT, NULL, 0));
    CU_ASSERT(CWSE_INVALID_OPTIONS == data_block_sender(&priv, CWS_TEXT|CWS_FIRST, NULL, 0));

    priv.closed = true;
    CU_ASSERT(CWSE_CLOSED_CONNECTION == data_block_sender(&priv, CWS_TEXT, NULL, 0));

    priv.closed = false;
    do {
        struct mock vector = {
            .rv = CWSE_OK,
            .options = CWS_TEXT | CWS_LAST | CWS_FIRST,
            .data = "ignore",
            .len = 6,
            .seen = 0,
            .next = NULL,
        };

        __goal = &vector;
        CU_ASSERT(CWSE_OK == data_block_sender(&priv, CWS_TEXT, "ignore", 6));
        CU_ASSERT(1 == vector.seen);

        vector.len = 0;
        vector.data = NULL;
        vector.seen = 0;
        __goal = &vector;
        CU_ASSERT(CWSE_OK == data_block_sender(&priv, CWS_TEXT, NULL, 0));
        CU_ASSERT(1 == vector.seen);

        vector.seen = 0;
        __goal = &vector;
        CU_ASSERT(CWSE_OK == data_block_sender(&priv, CWS_TEXT, "ignore", 0));
        CU_ASSERT(1 == vector.seen);

        vector.seen = 0;
        __goal = &vector;
        CU_ASSERT(CWSE_OK == data_block_sender(&priv, CWS_TEXT, NULL, 5));
        CU_ASSERT(1 == vector.seen);

    } while (0);

    do {
        struct mock vector[] = {
            {
                .rv = CWSE_OK,
                .options = CWS_BINARY | CWS_FIRST,
                .data = "0123456789",
                .len = 10,
                .seen = 0,
                .next = NULL,
            },
            {
                .rv = CWSE_OK,
                .options = CWS_CONT,
                .data = "abcdefghij",
                .len = 10,
                .seen = 0,
                .next = NULL,
            },
            {
                .rv = CWSE_OK,
                .options = CWS_CONT | CWS_LAST,
                .data = "9876543210",
                .len = 10,
                .seen = 0,
                .next = NULL,
            }
        };
        vector[0].next = &vector[1];
        vector[1].next = &vector[2];

        __goal = &vector[0];
        CU_ASSERT(CWSE_OK == data_block_sender(&priv, CWS_BINARY, "0123456789abcdefghij9876543210", 30));
        CU_ASSERT(1 == vector[0].seen);
        CU_ASSERT(1 == vector[1].seen);
        CU_ASSERT(1 == vector[2].seen);
    } while (0);

    do {
        struct mock vector[] = {
            {
                .rv = CWSE_OK,
                .options = CWS_BINARY | CWS_FIRST,
                .data = "0123456789",
                .len = 10,
                .seen = 0,
                .next = NULL,
            },
            {
                .rv = CWSE_OK,
                .options = CWS_CONT,
                .data = "abcdefghij",
                .len = 10,
                .seen = 0,
                .next = NULL,
            },
            {
                .rv = CWSE_OK,
                .options = CWS_CONT | CWS_LAST,
                .data = "9876543",
                .len = 7,
                .seen = 0,
                .next = NULL,
            }
        };
        vector[0].next = &vector[1];
        vector[1].next = &vector[2];

        __goal = &vector[0];
        CU_ASSERT(CWSE_OK == data_block_sender(&priv, CWS_BINARY, "0123456789abcdefghij9876543", 27));
        CU_ASSERT(1 == vector[0].seen);
        CU_ASSERT(1 == vector[1].seen);
        CU_ASSERT(1 == vector[2].seen);
    } while (0);

    do {
        struct mock vector[] = {
            {
                .rv = CWSE_OK,
                .options = CWS_BINARY | CWS_FIRST,
                .data = "0123456789",
                .len = 10,
                .seen = 0,
                .next = NULL,
            },
            {
                .rv = CWSE_CLOSED_CONNECTION,
                .options = CWS_CONT,
                .data = "abcdefghij",
                .len = 10,
                .seen = 0,
                .next = NULL,
            }
        };
        vector[0].next = &vector[1];

        __goal = &vector[0];
        CU_ASSERT(CWSE_CLOSED_CONNECTION == data_block_sender(&priv, CWS_BINARY, "0123456789abcdefghij9876543", 27));
        CU_ASSERT(1 == vector[0].seen);
        CU_ASSERT(1 == vector[1].seen);
    } while (0);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "data_block_sender() Tests",    .fn = test_data_block_sender },
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
