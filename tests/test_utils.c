/*
 * SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <CUnit/Basic.h>
#include <stdbool.h>

#include "../src/utils.h"


void test_cws_strdup()
{
    char *tmp;
    char *empty = "e";

    CU_ASSERT(NULL == cws_strdup(NULL));

    tmp = cws_strdup( &empty[1] );
    CU_ASSERT(NULL != tmp);
    CU_ASSERT('\0' == *tmp);
    free(tmp);

    tmp = cws_strdup( "asdf" );
    CU_ASSERT(NULL != tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "asdf");
    free(tmp);
}


void test_cws_strndup()
{
    char *tmp;
    char *empty = "e";
    CU_ASSERT(NULL == cws_strndup(NULL, 0));

    tmp = cws_strndup( &empty[1], 5 );
    CU_ASSERT(NULL != tmp);
    CU_ASSERT('\0' == *tmp);
    free(tmp);

    tmp = cws_strndup( &empty[1], 0 );
    CU_ASSERT(NULL == tmp);

    tmp = cws_strndup( "foo", 0 );
    CU_ASSERT(NULL == tmp);

    tmp = cws_strndup( "asdf", 12 );
    CU_ASSERT(NULL != tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "asdf");
    free(tmp);

    tmp = cws_strndup( "asdf", 2 );
    CU_ASSERT(NULL != tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "as");
    free(tmp);
}


void test_cws_strncasecmp()
{
    const char foo[] = "foo";
    CU_ASSERT(0 == cws_strncasecmp("a", "b", 0));
    CU_ASSERT(0 == cws_strncasecmp("a", "a", 1));
    CU_ASSERT(0 == cws_strncasecmp(foo, foo, 3));
    CU_ASSERT(0 == cws_strncasecmp(foo, "FOO", 1));
    CU_ASSERT(0 == cws_strncasecmp(foo, "FOO", 2));
    CU_ASSERT(0 == cws_strncasecmp(foo, "FOO", 3));
    CU_ASSERT(0 == cws_strncasecmp(foo, "FOO", 4));
    CU_ASSERT(0 >  cws_strncasecmp("a", "bOO", 4));
    CU_ASSERT(0 <  cws_strncasecmp("c", "bOO", 4));
    CU_ASSERT(0 >  cws_strncasecmp("foo", "FOOd", 4));
    CU_ASSERT(0 == cws_strncasecmp("foo", "FOOd", 3));
    CU_ASSERT(0 != cws_strncasecmp("foo", "food", 4));
    CU_ASSERT(0 != cws_strncasecmp("websocket", "websocket dog", 13));
}


void test_cws_has_prefix()
{
    CU_ASSERT(true  == cws_has_prefix("FOOBAR: goo", 11, "FOO"));
    CU_ASSERT(false == cws_has_prefix("FOOBAR: goo", 11, "FOOT"));
    CU_ASSERT(false == cws_has_prefix("FOOBAR: goo", 11, "FOOBAR: goobar"));
    CU_ASSERT(true  == cws_has_prefix("FOOBAR: goo", 11, "FOOBAR:"));
}


void test_cws_trim()
{
    struct {
        const char *in;
        const char *out;
    } tests[] = {
        { .in = "Nothing to trim.",       .out = "Nothing to trim."    },
        { .in = "  Something to trim.  ", .out = "Something to trim."  },
        { .in = "                      ", .out = ""                    },
        { .in = "",                       .out = ""                    },
        { .in = NULL, .out = NULL }
    };
    int i;

    for (i = 0; NULL != tests[i].in; i++) {
        const char *tmp;
        size_t len;

        //printf("Testing: %d - '%s' -> '%s'\n", i, tests[i].in, tests[i].out);

        len = strlen(tests[i].in);
        tmp = cws_trim(tests[i].in, &len);
        CU_ASSERT(NULL != tmp);

        //printf("got: (%d) '%.*s'\n", (int) len, (int) len, tmp);
        CU_ASSERT_NSTRING_EQUAL(tmp, tests[i].out, len);
        CU_ASSERT(len == strlen(tests[i].out));
    }
}


void test_cws_rewrite_url()
{
    struct {
        const char *in;
        const char *out;
    } tests[] = {
        { .in = "http://boo",  .out = "http://boo"  },
        { .in = "ws://boo",    .out = "http://boo"  },
        { .in = "wss://boo",   .out = "https://boo" },
        { .in = "https://boo", .out = "https://boo" },
        { .in = "ftp://boo",   .out = "ftp://boo"   },
        { .in = NULL, .out = NULL }
    };

    int i;

    for (i = 0; NULL != tests[i].in; i++) {
        char *tmp;

        //printf("Testing: %d - '%s' -> '%s'\n", i, tests[i].in, tests[i].out);

        tmp = cws_rewrite_url(tests[i].in);
        CU_ASSERT(NULL != tmp);
        CU_ASSERT_STRING_EQUAL(tmp, tests[i].out);
        free(tmp);
    }
}


void add_suites( CU_pSuite *suite )
{
    *suite = CU_add_suite( "utils.c tests", NULL, NULL );
    CU_add_test( *suite, "cws_strdup() Tests", test_cws_strdup );
    CU_add_test( *suite, "cws_strndup() Tests", test_cws_strndup );
    CU_add_test( *suite, "cws_rewrite_url() Tests", test_cws_rewrite_url );
    CU_add_test( *suite, "cws_strncasecmp() Tests", test_cws_strncasecmp );
    CU_add_test( *suite, "cws_trim() Tests", test_cws_trim );
    CU_add_test( *suite, "cws_has_prefix() Tests", test_cws_has_prefix );
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
