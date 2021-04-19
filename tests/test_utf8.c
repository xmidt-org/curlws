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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/Basic.h>

#include "../src/utf8.h"


void test_utf8()
{
    struct t {
        int rv;
        size_t expect;
        size_t len;
        const char *data;
    } tests[] = {
        { .rv =  0, .expect =  1, .len = 1, .data = "\x00"                }, /* 0x0000 */
        { .rv =  0, .expect =  1, .len = 1, .data = "\x01"                }, /* 0x0001 */
        { .rv =  0, .expect =  1, .len = 1, .data = "\x7e"                }, /* 0x007e */
        { .rv =  0, .expect =  1, .len = 1, .data = "\x7f"                }, /* 0x007f */
        { .rv = -1, .expect =  1, .len = 1, .data = "\x80"                }, /* invalid 0x0080 */
        { .rv = -1, .expect =  1, .len = 1, .data = "\x81"                }, /* invalid 0x0081 */
        { .rv = -1, .expect =  2, .len = 2, .data = "\xc1\xbf"            }, /* invalid 0x007f */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc2\x80"            }, /* 0x0080 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc2\x81"            }, /* 0x0081 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc3\x88"            }, /* 0x00c8 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc3\x90"            }, /* 0x00d0 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc3\xa0"            }, /* 0x00e0 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc3\xb0"            }, /* 0x00f0 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc3\xb8"            }, /* 0x00f8 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc3\xbf"            }, /* 0x00ff */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xc4\x80"            }, /* 0x0100 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xd0\x80"            }, /* 0x0400 */
        { .rv =  0, .expect =  2, .len = 2, .data = "\xdf\xbf"            }, /* 0x07ff */
        { .rv = -1, .expect =  3, .len = 3, .data = "\xe0\x9f\xbf"        }, /* invalid 0x07ff */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xe0\xa0\x80"        }, /* 0x0800 */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xe0\xa0\x81"        }, /* 0x0801 */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xe1\x80\x80"        }, /* 0x1000 */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xed\x80\x80"        }, /* 0xd000 */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xed\x9f\xbf"        }, /* 0xd7ff */
        { .rv = -1, .expect =  3, .len = 3, .data = "\xed\xa0\x80"        }, /* invalid 0xd800 */
        { .rv = -1, .expect =  3, .len = 3, .data = "\xed\xbf\xbf"        }, /* invalid 0xdfff */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xee\x80\x80"        }, /* 0xe000 */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xef\xbf\xbe"        }, /* 0xfffe */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xef\xbf\xbf"        }, /* 0xffff */
        { .rv = -1, .expect =  4, .len = 4, .data = "\xf0\x8f\xbf\xbf"    }, /* invalid 0xffff */
        { .rv =  0, .expect =  4, .len = 4, .data = "\xf0\x90\x80\x80"    }, /* 0x10000 */
        { .rv =  0, .expect =  4, .len = 4, .data = "\xf0\x90\x80\x81"    }, /* 0x10001 */
        { .rv =  0, .expect =  4, .len = 4, .data = "\xf1\x80\x80\x80"    }, /* 0x40000 */
        { .rv =  0, .expect =  4, .len = 4, .data = "\xf4\x8f\xbf\xbe"    }, /* 0x10fffe */
        { .rv =  0, .expect =  4, .len = 4, .data = "\xf4\x8f\xbf\xbf"    }, /* 0x10ffff */
        { .rv = -1, .expect =  4, .len = 4, .data = "\xf4\xa0\x80\x80"    }, /* invalid 0x110000 */
        { .rv =  0, .expect =  3, .len = 3, .data = "\xef\xbf\xbd"        }, /* 0xFFFD */
        { .rv =  0, .expect =  0, .len = 3, .data = "\xf4\x8f\xbf\xbf"    }, /* too short 0x10ffff */
        { .rv = -1, .expect =  2, .len = 2, .data = "\xc2\xff"            }, /* invalid 2nd char */
        { .rv = -1, .expect =  3, .len = 3, .data = "\xe2\x00\x80"        }, /* invalid 2nd char */
        { .rv = -1, .expect =  3, .len = 3, .data = "\xe2\x80\x00"        }, /* invalid 3nd char */
        { .rv = -1, .expect =  4, .len = 4, .data = "\xf1\x00\x80\x80"    }, /* invalid 2nd char */
        { .rv = -1, .expect =  4, .len = 4, .data = "\xf1\x80\x00\x80"    }, /* invalid 3nd char */
        { .rv = -1, .expect =  4, .len = 4, .data = "\xf1\x80\x80\x00"    }, /* invalid 4th char */

        { .rv =  0, .expect = 14, .len = 14, .data = "a\xc2\x80\xe8\x80\x80\xf1\x80\x80\x80\xf1\xbf\xbf\xbf" },
    };

    for (size_t i = 0; i < sizeof(tests)/sizeof(struct t); i++) {
        struct t *test = &tests[i];
        int got;

        got = utf8_validate(test->data, &test->len);
        if (test->rv != got) {
            printf("%zd - rv expected: %d, got: %d, len expected: %zd, len got: %zd, data: 0x%02x",
                   i, test->rv, got, test->expect, test->len, 0xff & test->data[0]);
            if (1 < test->len) {
                printf("%02x", 0xff & test->data[1]);
            }
            if (2 < test->len) {
                printf("%02x", 0xff & test->data[2]);
            }
            if (3 < test->len) {
                printf("%02x", 0xff & test->data[3]);
            }
            printf("\n");
        }
        /* Run the test again so it's easier to debug. */
        CU_ASSERT(test->rv == utf8_validate(test->data, &test->len));
        CU_ASSERT(test->len == test->expect);
    }
}


void test_maybe_valid()
{
    CU_ASSERT(true == utf8_maybe_valid("\xe1", 1));
    CU_ASSERT(false == utf8_maybe_valid("\xf5", 1));
    CU_ASSERT(true == utf8_maybe_valid("\xf4", 1));
    CU_ASSERT(false == utf8_maybe_valid("\xf4\xa0", 2));
}

void test_get_size()
{
    CU_ASSERT(1 == utf8_get_size('a'));
    CU_ASSERT(2 == utf8_get_size(0xc4));
    CU_ASSERT(3 == utf8_get_size(0xe1));
    CU_ASSERT(4 == utf8_get_size(0xf4));
    CU_ASSERT(0 == utf8_get_size(0xf5));
}


void add_suites( CU_pSuite *suite )
{
    *suite = CU_add_suite( "utf8 tests", NULL, NULL );
    CU_add_test( *suite, "general utf8() Tests", test_utf8 );
    CU_add_test( *suite, "utf8_maybe_valid() Tests", test_maybe_valid );
    CU_add_test( *suite, "utf8_get_size() Tests", test_get_size );
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
