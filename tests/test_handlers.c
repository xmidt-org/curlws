/*
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
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


static int __close_called = 0;
static int __close_code = 0;
CWScode cws_close(CWS *handle, int code, const char *reason, size_t len)
{
    IGNORE_UNUSED(handle);
    IGNORE_UNUSED(code);
    IGNORE_UNUSED(reason);
    IGNORE_UNUSED(len);
    __close_called++;
    __close_code = code;

    return CWSE_OK;
}

static int __on_pong_rv = 0;
int on_pong(void *user, CWS *handle, const void *p, size_t len)
{
    IGNORE_UNUSED(user);
    IGNORE_UNUSED(handle);
    IGNORE_UNUSED(p);
    IGNORE_UNUSED(len);

    return __on_pong_rv;
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

    populate_callbacks(&priv.cb, &src);

    src.on_connect  = (int (*)(void*, CWS*, const char*)) 1;
    src.on_text     = (int (*)(void*, CWS*, const char*, size_t)) 2;
    src.on_binary   = (int (*)(void*, CWS*, const void*, size_t)) 3;
    src.on_fragment = (int (*)(void*, CWS*, int, const void*, size_t)) 4;
    src.on_ping     = (int (*)(void*, CWS*, const void*, size_t)) 5;
    src.on_pong     = (int (*)(void*, CWS*, const void*, size_t)) 6;
    src.on_close    = (int (*)(void*, CWS*, int, const char*, size_t)) 7;
    src.configure   = (CURLcode (*)(void*, CWS*, CURL*)) 8;

    populate_callbacks(&priv.cb, &src);
    CU_ASSERT(priv.cb.on_connect_fn  == (int (*)(void*, CWS*, const char*)) 1);
    CU_ASSERT(priv.cb.on_text_fn     == (int (*)(void*, CWS*, const char*, size_t)) 2);
    CU_ASSERT(priv.cb.on_binary_fn   == (int (*)(void*, CWS*, const void*, size_t)) 3);
    CU_ASSERT(priv.cb.on_fragment_fn == (int (*)(void*, CWS*, int, const void*, size_t)) 4);
    CU_ASSERT(priv.cb.on_ping_fn     == (int (*)(void*, CWS*, const void*, size_t)) 5);
    CU_ASSERT(priv.cb.on_pong_fn     == (int (*)(void*, CWS*, const void*, size_t)) 6);
    CU_ASSERT(priv.cb.on_close_fn    == (int (*)(void*, CWS*, int, const char*, size_t)) 7);

}


void check_output(FILE *f, const char *e1, const char *e2)
{
    char got[256];

    CU_ASSERT_FATAL(NULL != f);
    CU_ASSERT_FATAL(NULL != e1);
    CU_ASSERT_FATAL(NULL != e2);

    fflush(f);
    rewind(f);

    memset(got, 0, sizeof(got));
    CU_ASSERT_FATAL(NULL != fgets(got, sizeof(got), f));
    CU_ASSERT_STRING_EQUAL(e1, got);

    if (0 != strcmp(e1, got)) {
        printf("\n");
        printf("l1 got: '%s'\n", got);
        printf("wanted: '%s'\n", e1);
    }

    memset(got, 0, sizeof(got));
    CU_ASSERT_FATAL(NULL != fgets(got, sizeof(got), f));
    CU_ASSERT_STRING_EQUAL(e2, got);

    if (0 != strcmp(e2, got)) {
        printf("\n");
        printf("l2 got: '%s'\n", got);
        printf("wanted: '%s'\n", e2);
    }

    /* reset the stream for the next test */
    rewind(f);
}


void test_close_on_rv()
{
    CWS priv;

    memset(&priv, 0, sizeof(priv));

    populate_callbacks(&priv.cb, NULL);

    priv.cb.on_pong_fn = on_pong;

    __on_pong_rv = -1;
    __close_called = 0;
    cb_on_pong(&priv, NULL, 0);
    CU_ASSERT(1 == __close_called);
    CU_ASSERT(1011 == __close_code);

    __on_pong_rv = 1000;
    __close_called = 0;
    cb_on_pong(&priv, NULL, 0);
    CU_ASSERT(1 == __close_called);
    CU_ASSERT(1000 == __close_code);
}


void test_defaults_dont_crash()
{
    CWS priv;
    FILE *f_tmp = NULL;
    struct cws_config src;
    char got[256];

    memset(&priv, 0, sizeof(priv));
    memset(&src, 0, sizeof(src));
    memset(got, 0, sizeof(got));

    f_tmp = tmpfile();
    CU_ASSERT_FATAL(NULL != f_tmp);

    priv.cfg.verbose_stream = f_tmp;

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

    /* Make sure nothing is output */
    CU_ASSERT_FATAL(NULL == fgets(got, sizeof(got), f_tmp));

    /* Check the verbose debug callback */
    priv.cfg.verbose = 3;

    populate_callbacks(&priv.cb, &src);

    /* Make sure nothing crashes if we're in verbose mode. */
    cb_on_connect(&priv, NULL);
    check_output(f_tmp,
                 "< websocket on_connect() protos: '(null)'\n",
                 "> websocket on_connect()\n");

    cb_on_text(&priv, NULL, 0);
    check_output(f_tmp,
                 "< websocket on_text() len: 0, text: ''\n",
                 "> websocket on_text()\n");

    /* There is an extra 0 on purpose. */
    cb_on_text(&priv, "0123456789012345678901234567890", 30);
    check_output(f_tmp,
                 "< websocket on_text() len: 30, text: '012345678901234567890123456789'\n",
                 "> websocket on_text()\n");

    /* A longer string so it's truncated */
    cb_on_text(&priv, "01234567890123456789012345678901234567890123456789", 50);
    check_output(f_tmp,
                 "< websocket on_text() len: 50, text: '0123456789012345678901234567890123456789...'\n",
                 "> websocket on_text()\n");

    cb_on_binary(&priv, NULL, 0);
    check_output(f_tmp,
                 "< websocket on_binary() len: 0, [buf]\n",
                 "> websocket on_binary()\n");

    cb_on_fragment(&priv, 0, NULL, 0);
    check_output(f_tmp,
                 "< websocket on_fragment() info: 0x00000000, len: 0, [buf]\n",
                 "> websocket on_fragment()\n");

    cb_on_ping(&priv, NULL, 0);
    check_output(f_tmp,
                 "< websocket on_ping() len: 0, [buf]\n",
                 "> websocket on_ping()\n");

    cb_on_pong(&priv, NULL, 0);
    check_output(f_tmp,
                 "< websocket on_pong() len: 0, [buf]\n",
                 "> websocket on_pong()\n");

    cb_on_close(&priv, 0, NULL, 0);
    check_output(f_tmp,
                 "< websocket on_close() code: 0, len: 0, text: ''\n",
                 "> websocket on_close()\n");

    cb_on_close(&priv, 1001, "include a string", SIZE_MAX);
    check_output(f_tmp,
                 "< websocket on_close() code: 1001, len: 16, text: 'include a string'\n",
                 "> websocket on_close()\n");

    cb_on_close(&priv, 0, "include a string", 0);
    check_output(f_tmp,
                 "< websocket on_close() code: 0, len: 0, text: ''\n",
                 "> websocket on_close()\n");

    fclose(f_tmp);
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "Test populate_callbacks()",  .fn = test_populate_callbacks  },
        { .label = "Test close on non-zero rv",  .fn = test_close_on_rv         },
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
