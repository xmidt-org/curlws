/*
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <CUnit/Basic.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/internal.h"
#include "../src/ws.h"

#include "../src/receive.c"

#undef curl_easy_setopt


CURLcode curl_easy_setopt(CURL *easy, CURLoption option, ...)
{
    (void) easy;
    (void) option;
    return CURLE_OK;
}


CURLcode curl_easy_pause(CURL *easy, int bitmask)
{
    (void) easy;
    (void) bitmask;
    return CURLE_OK;
}


struct mock_ping {
    const char *data;
    size_t len;
    int seen;

    struct mock_ping *next;
};


static struct mock_ping *__on_ping_goal = NULL;
static int on_ping(void *data, CWS *handle, const void *buffer, size_t len)
{
    (void) data;

    // printf( "on_ping(data, handle, '%.*s', %zd)\n", (int) len, (const char*) buffer, len);

    CU_ASSERT_FATAL(NULL != __on_ping_goal);
    CU_ASSERT(NULL != handle);
    CU_ASSERT(__on_ping_goal->len == len);
    if (NULL != __on_ping_goal->data) {
        for (size_t i = 0; i < len; i++) {
            CU_ASSERT(__on_ping_goal->data[i] == ((char *) buffer)[i]);
        }
    }
    __on_ping_goal->seen++;
    __on_ping_goal = __on_ping_goal->next;

    return 0;
}


CWScode cws_close(CWS *priv, int code, const char *reason, size_t len)
{
    (void) priv;
    (void) code;
    (void) reason;
    (void) len;

    return CWSE_OK;
}

void setup_test(CWS *priv)
{
    memset(priv, 0, sizeof(CWS));
    receive_init(priv);
    priv->cb.on_ping_fn = on_ping;
}


void test_27()
{
    const char *p;
    CWS priv;
    const char *in =
        "\x89\x09payload-0"
        "\x89\x09payload-1"
        "\x89\x09payload-2"
        "\x89\x09payload-3"
        "\x89\x09payload-4"
        "\x89\x09payload-5"
        "\x89\x09payload-6"
        "\x89\x09payload-7"
        "\x89\x09payload-8"
        "\x89\x09payload-9";

    size_t list[] = {
        2, 5, 1, 1, 1, 1, 2, 6, 1, 1,
        2, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 2, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 3, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1
    };

    struct mock_ping pings[] = {
        {.data = "payload-0", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-1", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-2", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-3", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-4", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-5", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-6", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-7", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-8", .len = 9, .seen = 0, .next = NULL},
        {.data = "payload-9", .len = 9, .seen = 0, .next = NULL},
    };

    setup_test(&priv);

    pings[0].next  = &pings[1];
    pings[1].next  = &pings[2];
    pings[2].next  = &pings[3];
    pings[3].next  = &pings[4];
    pings[4].next  = &pings[5];
    pings[5].next  = &pings[6];
    pings[6].next  = &pings[7];
    pings[7].next  = &pings[8];
    pings[8].next  = &pings[9];
    __on_ping_goal = pings;

    p = in;
    for (size_t i = 0; i < sizeof(list) / sizeof(size_t); i++) {
        if (list[i] != _receive_cb(p, list[i], 1, &priv)) {
            printf("Failed to accept the bytes.\n");
        }
        p += list[i];
    }

    CU_ASSERT(1 == pings[0].seen);
    CU_ASSERT(1 == pings[1].seen);
    CU_ASSERT(1 == pings[2].seen);
    CU_ASSERT(1 == pings[3].seen);
    CU_ASSERT(1 == pings[4].seen);
    CU_ASSERT(1 == pings[5].seen);
    CU_ASSERT(1 == pings[6].seen);
    CU_ASSERT(1 == pings[7].seen);
    CU_ASSERT(1 == pings[8].seen);
    CU_ASSERT(1 == pings[9].seen);
}


void add_suites(CU_pSuite *suite)
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        {.label = "test 27", .fn = test_27},
        {     .label = NULL,    .fn = NULL}
    };
    int i;

    *suite = CU_add_suite("curlws.c tests", NULL, NULL);

    for (i = 0; NULL != tests[i].fn; i++) {
        CU_add_test(*suite, tests[i].label, tests[i].fn);
    }
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
