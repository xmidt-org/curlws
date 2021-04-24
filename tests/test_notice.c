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
#include <stdbool.h>

#include "../src/curlws.h"

static char* global_filename;

char* read_notice_file(const char *filename)
{
    FILE *f = NULL;
    long len = 0;
    char *buf = NULL;
 
    f = fopen(filename, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);

        buf = (char*) malloc(sizeof(char)*(len + 1));
        if (buf) {
            if (len == (long) fread(buf, 1, len, f)) {
                buf[len] = '\0';
                free(buf);
                buf = NULL;
            }
        }
        fclose(f);
    }

    return buf;
}

void test_notice()
{
    char *goal = NULL;

    goal = read_notice_file(global_filename);
    if (goal) {
        const char *got = NULL;
        size_t goal_size, got_size;

        got = cws_get_notice();
        goal_size = strlen(goal);
        got_size = strlen(got);

        CU_ASSERT(goal_size == got_size);
        //printf("goal: %zd, got: %zdn", goal_size, got_size);

        CU_ASSERT_NSTRING_EQUAL(goal, got, goal_size);

        free(goal);
    }
}

void add_suites( CU_pSuite *suite )
{
    *suite = CU_add_suite( "utils.c tests", NULL, NULL );
    CU_add_test( *suite, "cws_get_notice() Tests", test_notice );
}


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main( int argc, char *argv[] )
{
    unsigned rv = 1;
    CU_pSuite suite = NULL;

    if (2 != argc) {
        fprintf(stderr, "Usage:\n%s path/to/NOTICE\n", argv[0]);
        return -1;
    }

    global_filename = argv[1];
    fprintf(stderr, "File: '%s'\n", global_filename);

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
