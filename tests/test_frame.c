/*
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/frame.h"


void test_validate()
{
    // clang-format off
    struct {
        const char *desc;
        struct cws_frame f;
        frame_dir dir;
        int rv;
    } tests[] = {
        { .desc = "A happy path continuation frame",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_CONTINUATION,
          .f.is_control = 0,
          .f.mask = 1,
          .f.masking_key = {0, 0, 0, 123},
          .f.payload_len = 1200,
          .dir = FRAME_DIR_C2S,
          .rv = 0 },
        { .desc = "A happy path text frame",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_TEXT,
          .f.is_control = 0,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 1200,
          .dir = FRAME_DIR_S2C,
          .rv = 0 },
        { .desc = "A happy path ping frame",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PING,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 0,
          .dir = FRAME_DIR_S2C,
          .rv = 0 },
        { .desc = "A happy path pong frame",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PONG,
          .f.is_control = 1,
          .f.mask = 1,
          .f.masking_key = {0, 0, 4, 0xd2},
          .f.payload_len = 0,
          .dir = FRAME_DIR_C2S,
          .rv = 0 },

        { .desc = "A error path ping frame -- invalid mask check",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PING,
          .f.is_control = 1,
          .f.mask = 1,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 0,
          .dir = FRAME_DIR_S2C,
          .rv = -1 },
        { .desc = "A error path ping frame -- invalid mask check",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PING,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 4, 0xd2},
          .f.payload_len = 0,
          .dir = FRAME_DIR_C2S,
          .rv = -1 },

        { .desc = "A error path pong frame -- invalid mask",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PONG,
          .f.is_control = 1,
          .f.mask = 1,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 0,
          .dir = FRAME_DIR_S2C,
          .rv = -1 },
        { .desc = "A error path pong frame -- invalid mask",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PONG,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 4, 0xd2},
          .f.payload_len = 0,
          .dir = FRAME_DIR_C2S,
          .rv = -1 },
        { .desc = "A error path pong frame -- invalid mask",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PONG,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 0,
          .dir = FRAME_DIR_C2S,
          .rv = -1 },

        { .desc = "A error path ping frame -- invalid control msg",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PING,
          .f.is_control = 0,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 0,
          .dir = FRAME_DIR_S2C,
          .rv = -2 },
        { .desc = "A error path text frame -- invalid control msg",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_TEXT,
          .f.is_control = 1,
          .f.mask = 1,
          .f.masking_key = {0, 0, 4, 0xd2},
          .f.payload_len = 0,
          .dir = FRAME_DIR_C2S,
          .rv = -2 },

        { .desc = "A error path ping frame -- invalid payload len",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_PING,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 130,
          .dir = FRAME_DIR_S2C,
          .rv = -3 },
        { .desc = "A error path text frame -- invalid payload len",
          .f.fin = 1,
          .f.opcode = WS_OPCODE_TEXT,
          .f.is_control = 0,
          .f.mask = 1,
          .f.masking_key = {0, 0, 4, 0xd2},
          .f.payload_len = UINT64_MAX,
          .dir = FRAME_DIR_C2S,
          .rv = -3 },

        { .desc = "A error path ping frame -- invalid fin",
          .f.fin = 0,
          .f.opcode = WS_OPCODE_PING,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 10,
          .dir = FRAME_DIR_S2C,
          .rv = -4 },

        { .desc = "A error path invalid opcode frame",
          .f.fin = 0,
          .f.opcode = 5,
          .f.is_control = 1,
          .f.mask = 0,
          .f.masking_key = {0, 0, 0, 0},
          .f.payload_len = 10,
          .dir = FRAME_DIR_S2C,
          .rv = -5 },

        { .rv = -99 }
    };
    // clang-format on
    int i;

    for (i = 0; tests[i].rv != -99; i++) {
        int rv;

        rv = frame_validate(&tests[i].f, tests[i].dir);
        if (tests[i].rv != rv) {
            printf("\nTest Failed: %s\n", tests[i].desc);
            printf("expected: %d, got: %d\n", tests[i].rv, rv);
        }
        CU_ASSERT(tests[i].rv == rv);
    }
}

void test_decode()
{
    // clang-format off
    struct {
        struct cws_frame f;
        size_t len;
        uint8_t frame[14];
        ssize_t delta[15];
        int rv[15];
    } tests[] = {
        /* Basic with mask */
        { .len = 6,
          .frame         = {     0x8a, 0x84, 0x01, 0x02, 0x03, 0x04, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,   -4,   -3,   -2,   -1,    6, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .f.fin         = 1,
          .f.opcode      = 0xa,
          .f.mask        = 1,
          .f.masking_key = { 1, 2, 3, 4 },
          .f.payload_len = 4,
          .f.payload     = NULL },
        /* Basic tiny */
        { .len = 2,
          .frame         = {     0x89, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .f.fin         = 1,
          .f.opcode      = 0x9,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 4,
          .f.payload     = NULL },
        /* Basic +2 bytes for length */
        { .len = 4,
          .frame         = {     0x00, 0x7e, 0x00, 0xc8, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,   -2,   -1,    4, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 0,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 200,
          .f.payload     = NULL },
        /* Basic +8 bytes for length */
        { .len = 10,
          .frame         = {     0x01, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,   -8,   -7,   -6,   -5,   -4,   -3,   -2,   -1,   10, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 1,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 0x50000,
          .f.payload     = NULL },
        /* Invalid reserved bits */
        { .len = 2,
          .frame         = {     0x69, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,   -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 0,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 0,
          .f.payload     = NULL },
        /* Invalid opcode */
        { .len = 2,
          .frame         = {     0x85, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,   -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 0,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 0,
          .f.payload     = NULL },
        /* Invalid length ... too short for the size used. */
        { .len = 4,
          .frame         = {     0x00, 0x7e, 0x00, 0x10, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,   -2,   -1,    0, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0,    0,   -3, 0, 0, 0, 0, 0, 0, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 0,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 0,
          .f.payload     = NULL },
        /* Invalid length ... MSB set when it should always be 0. */
        { .len = 10,
          .frame         = {     0x01, 0x7f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,   -8,   -7,   -6,   -5,   -4,   -3,   -2,   -1,    0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   -4, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 0,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 0,
          .f.payload     = NULL },        /* Invalid length ... too short for the size used. */
        { .len = 10,
          .frame         = {     0x01, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0, 0 ,0, 0 },
          .delta         = { -2,   -1,   -8,   -7,   -6,   -5,   -4,   -3,   -2,   -1,    0, 0, 0 ,0, 0 },
          .rv            = {  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   -5, 0, 0 ,0, 0 },
          .f.fin         = 0,
          .f.opcode      = 0,
          .f.mask        = 0,
          .f.masking_key = { 0, 0, 0, 0},
          .f.payload_len = 0,
          .f.payload     = NULL },
        { .len = 0 }
    };
    // clang-format on

    size_t i, j;

    for (j = 0; 0 < tests[j].len; j++) {
        struct cws_frame f;
        for (i = 0; i < tests[j].len + 1; i++) {
            int rv;
            ssize_t delta = 5;

            rv = frame_decode(&f, tests[j].frame, i, NULL);
            if (tests[j].rv[i] != rv) {
                printf("%zd.%zd %d =?= %d\n", j, i, tests[j].rv[i], rv);
            }
            CU_ASSERT(tests[j].rv[i] == rv);
            rv = frame_decode(&f, tests[j].frame, i, &delta);
            CU_ASSERT(tests[j].rv[i] == rv);

            if (tests[j].delta[i] != delta) {
                printf("%zd.%zd %zd =?= %zd\n", j, i, tests[j].delta[i], delta);
            }
            CU_ASSERT(tests[j].delta[i] == delta);
        }

        CU_ASSERT(tests[j].f.fin == f.fin);
        CU_ASSERT(tests[j].f.opcode == f.opcode);
        CU_ASSERT(tests[j].f.mask == f.mask);
        CU_ASSERT(tests[j].f.masking_key[0] == f.masking_key[0]);
        CU_ASSERT(tests[j].f.masking_key[1] == f.masking_key[1]);
        CU_ASSERT(tests[j].f.masking_key[2] == f.masking_key[2]);
        CU_ASSERT(tests[j].f.masking_key[3] == f.masking_key[3]);
        CU_ASSERT(tests[j].f.payload_len == f.payload_len);
        CU_ASSERT(tests[j].f.payload == f.payload);
    }
}

void test_encode()
{
    uint8_t buffer[256];
    const uint8_t expect[] = { 0x89, 0x84, 0x01, 0x02, 0x03, 0x04, 0x51, 0x4b, 0x4d, 0x43 };

    struct cws_frame f = {
        .fin         = 1,
        .mask        = 1,
        .is_control  = 1,
        .opcode      = WS_OPCODE_PING,
        .masking_key = {1, 2, 3, 4},
        .payload_len = 4,
        .payload     = "PING"
    };
    size_t rv;

    rv = frame_encode(&f, buffer, 256);
    CU_ASSERT(sizeof(expect) == rv);

    for (size_t i = 0; i < sizeof(expect); i++) {
        if (expect[i] != buffer[i]) {
            printf("%zd 0x%02x =?= 0x%02x\n", i, expect[i], buffer[i]);
        }
        CU_ASSERT(expect[i] == buffer[i]);
    }
}


void test_encode_too_short()
{
    uint8_t buffer[256];

    struct cws_frame f = {
        .fin         = 1,
        .mask        = 1,
        .is_control  = 1,
        .opcode      = WS_OPCODE_BINARY,
        .masking_key = {0, 0, 0, 0},
        .payload_len = 0x10000,
        .payload     = "PING"
    };
    size_t rv;

    rv = frame_encode(&f, buffer, 1);
    CU_ASSERT(0 == rv);

    /* Header is 6, payload is 0x10000, so there are
     * extra length bytes needed */
    rv = frame_encode(&f, buffer, 0x10000 + 7);
    CU_ASSERT(0 == rv);
}


void test_encode_long()
{
    // clang-format off
    const uint8_t header1[] = { 0x82, 0xff,                 /* Base header */
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, /* payload len */
                                0x00, 0x00, 0x00, 0x00 };   /* mask */
    const uint8_t header2[] = { 0x82, 0xfe,                 /* base header */
                                0x10, 0x00,                 /* payload len */
                                0x00, 0x00, 0x00, 0x00 };   /* mask */
    // clang-format off

    uint8_t *payload;
    uint8_t *buffer1;
    uint8_t *buffer2;
    uint8_t *expect1;
    uint8_t *expect2;

    struct cws_frame f = {
        .fin = 1,
        .mask = 1,
        .is_control = 0,
        .opcode = WS_OPCODE_BINARY,
        .masking_key = {0, 0, 0, 0},
        .payload_len = 0x10000,
    };

    size_t rv, i;

    payload = (uint8_t*) malloc(0x10000);

    /* 2+4+8 bytes for the uint64_t length */
    buffer1 = (uint8_t*) malloc(0x10000 + 14 + 1);
    expect1 = (uint8_t*) malloc(0x10000 + 14 + 1);
    /* 2+4+2 bytes for the uint16_t length */
    buffer2 = (uint8_t*) malloc(0x01000 + 8 + 1 );
    expect2 = (uint8_t*) malloc(0x01000 + 8 + 1 );

    memcpy(expect1, &header1, sizeof(header1));
    memcpy(expect2, &header2, sizeof(header2));
    for (i = 0; i < 0x10000; i++) {
        payload[i] = 0xff & i;
        expect1[i + sizeof(header1)] = 0xff & i;
    }
    for (i = 0; i < 0x1000; i++) {
        expect2[i + sizeof(header2)] = 0xff & i;
    }
    /* guard check */
    buffer1[0x10000+14] = 0xa5;
    buffer2[0x01000+8] = 0xa5;


    f.payload = payload;

    rv = frame_encode(&f, buffer1, 0x10000 + 14);
    CU_ASSERT((0x10000 + 14) == rv);
    CU_ASSERT(0xa5 == buffer1[0x10000+14]);

    for (i = 0; i < (0x10000+14); i++) {
        if (expect1[i] != buffer1[i]) {
            printf("case 1: %zd 0x%02x =?= 0x%02x\n", i, expect1[i], buffer1[i]);
        }
        CU_ASSERT(expect1[i] == buffer1[i]);
    }


    f.payload_len = 0x01000;
    rv = frame_encode(&f, buffer2, 0x01000 + 8);
    CU_ASSERT((0x01000 + 8) == rv);
    CU_ASSERT(0xa5 == buffer2[0x01000+8]);

    for (i = 0; i < sizeof(expect2); i++) {
        if (expect2[i] != buffer2[i]) {
            printf("case 2: %zd 0x%02x =?= 0x%02x\n", i, expect2[i], buffer2[i]);
        }
        CU_ASSERT(expect2[i] == buffer2[i]);
    }

    free(payload);
    free(buffer1);
    free(buffer2);
    free(expect1);
    free(expect2);
}

void test_to_string()
{
    struct cws_frame f;

    memset(&f, 0, sizeof(struct cws_frame));

    CU_ASSERT_STRING_EQUAL("invalid frame", frame_opcode_to_string(NULL));

    for (int i = 0; i < 16; i++) {
        char buf[20];

        f.opcode = i;

        switch (i) {
            case WS_OPCODE_CONTINUATION:
                sprintf(buf, "CONT");
                break;
            case WS_OPCODE_TEXT:
                sprintf(buf, "TEXT");
                break;
            case WS_OPCODE_BINARY:
                sprintf(buf, "BINARY");
                break;
            case WS_OPCODE_CLOSE:
                sprintf(buf, "CLOSE");
                break;
            case WS_OPCODE_PING:
                sprintf(buf, "PING");
                break;
            case WS_OPCODE_PONG:
                sprintf(buf, "PONG");
                break;
            default:
                sprintf(buf, "Unknown (0x%x)", i);
                break;
        }

        printf("\nExpected: '%s', got: '%s'\n", buf, frame_opcode_to_string(&f));
        CU_ASSERT_STRING_EQUAL_FATAL(buf, frame_opcode_to_string(&f));
    }
}



void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "Basic Encode Tests",       .fn = test_encode           },
        { .label = "Encode Too Short Tests",   .fn = test_encode_too_short },
        { .label = "Encode Long Buffer Tests", .fn = test_encode_long      },
        { .label = "Basic Decode Tests",       .fn = test_decode           },
        { .label = "Basic Validate Tests",     .fn = test_validate         },
        { .label = "Basic to_string Tests",    .fn = test_to_string        },
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
