/*
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <CUnit/Basic.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/sha1.h"


void test_cws_sha1()
{
    const char in[] = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    size_t len      = strlen(in);
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


void add_suites(CU_pSuite *suite)
{
    *suite = CU_add_suite("sha1.c tests", NULL, NULL);
    CU_add_test(*suite, "cws_sha1() Tests", test_cws_sha1);
}


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main(void)
{
    unsigned rv     = 1;
    CU_pSuite suite = NULL;

    if (CUE_SUCCESS == CU_initialize_registry()) {
        add_suites(&suite);

        if (NULL != suite) {
            CU_basic_set_mode(CU_BRM_VERBOSE);
            CU_basic_run_tests();
            printf("\n");
            CU_basic_show_failures(CU_get_failure_list());
            printf("\n\n");
            rv = CU_get_number_of_tests_failed();
        }

        CU_cleanup_registry();
    }

    if (0 != rv) {
        return 1;
    }
    return 0;
}
