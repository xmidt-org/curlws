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
    CU_ASSERT(NULL != tmp);
    CU_ASSERT('\0' == *tmp);
    free(tmp);

    tmp = cws_strndup( "foo", 0 );
    CU_ASSERT(NULL != tmp);
    CU_ASSERT('\0' == *tmp);
    free(tmp);

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


void test_cws_encode_base64()
{
    struct {
        const char *in;
        size_t len;
        const char *out;
    } tests[] = {
        { .in = "http",        .len = 0,    .out = ""      },
        { .in = "http",        .len = 1,    .out = "aA=="  },
        { .in = "http",        .len = 2,    .out = "aHQ="  },
        { .in = "http",        .len = 3,    .out = "aHR0"  },
        { .in = "http",        .len = 4,    .out = "aHR0cA=="  },
        { .in = NULL, .out = NULL }
    };

    int i;

    for (i = 0; NULL != tests[i].in; i++) {
        char tmp[60];

        //printf("Testing: %d - '%s' -> '%s'\n", i, tests[i].in, tests[i].out);

        cws_encode_base64(tests[i].in, tests[i].len, tmp);
        CU_ASSERT(NULL != tmp);
        CU_ASSERT_STRING_EQUAL(tmp, tests[i].out);
    }
}


void test_cws_sha1()
{
    const char in[] = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    size_t len = strlen(in);
    size_t i, j;
    uint8_t buffer[24];
    uint8_t expect[24] = { 0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f, 0x16, 0x90, 0xf6,
                           0x46, 0x06, 0xcf, 0x38, 0x59, 0x45, 0xb2, 0xbe, 0xc4, 0xea,
                           /* These are extra to make sure we only get 20 bytes. */
                           0, 0, 0, 0 };

    /* Run through twice to make sure that the results are consistent.*/
    for (j = 0; j < 2; j++) {
        memset(buffer, 0, 24);
        cws_sha1(in, len, buffer);

        for (i = 0; i < sizeof(expect); i++) {
            CU_ASSERT(buffer[i] == expect[i]);
        }
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
    CU_add_test( *suite, "cws_encode_base64() Tests", test_cws_encode_base64 );
    CU_add_test( *suite, "cws_sha1() Tests", test_cws_sha1 );
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
