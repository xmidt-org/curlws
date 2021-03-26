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
#include <inttypes.h>
#include <curl/curl.h>

#define __INCLUDE_TEST_DECODER__
#include "../src/curlws.h"
#include "../src/curlws.c"

#undef curl_easy_getinfo
#undef curl_easy_setopt

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define IGNORE_UNUSED(x) (void) x;
#define MAX_MOCK_CALLS   50

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                                 Mock Curl                                  */
/*----------------------------------------------------------------------------*/
typedef struct {
    bool init;
} mock_curl_t;

static curl_version_info_data __curl_info;
curl_version_info_data* curl_version_info(CURLversion v)
{
    IGNORE_UNUSED(v);

    return &__curl_info;
}


CURL* curl_easy_init()
{
    static mock_curl_t c;

    CU_ASSERT(false == c.init);
    c.init = true;

    return &c;
}

void curl_easy_cleanup(CURL *easy)
{
    mock_curl_t *c = (mock_curl_t*) easy;

    CU_ASSERT(NULL != c);
    CU_ASSERT(true == c->init);

    memset(c, 0, sizeof(mock_curl_t));
}

static int __easy_getinfo_count;
static CURLcode __easy_getinfo_rv[MAX_MOCK_CALLS];
CURLcode curl_easy_getinfo(CURL *easy, CURLINFO info, ... )
{
    CU_ASSERT(NULL != easy);
    IGNORE_UNUSED(info);
    return __easy_getinfo_rv[__easy_getinfo_count++];
}

static int __easy_setopt_count;
static CURLcode __easy_setopt_rv[MAX_MOCK_CALLS];
CURLcode curl_easy_setopt(CURL *easy, CURLoption option, ... )
{
    CU_ASSERT(NULL != easy);
    IGNORE_UNUSED(option);
    return __easy_setopt_rv[__easy_setopt_count++];
}

struct curl_slist *curl_slist_append(struct curl_slist *list, const char *s)
{
    IGNORE_UNUSED(list);
    IGNORE_UNUSED(s);

    return NULL;
}

void curl_slist_free_all(struct curl_slist *list)
{
    IGNORE_UNUSED(list);
}

static int __easy_pause_count;
static CURL *__easy_pause_easy;
static CURLcode __easy_pause_rv[MAX_MOCK_CALLS];
CURLcode curl_easy_pause(CURL *easy, int bitmask)
{
    CU_ASSERT(__easy_pause_easy == easy);
    IGNORE_UNUSED(bitmask);
    return __easy_pause_rv[__easy_pause_count++];
}

CURLMcode curl_multi_add_handle(CURL *easy, CURLM *multi_handle)
{
    IGNORE_UNUSED(easy);
    IGNORE_UNUSED(multi_handle);
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle(CURL *easy, CURLM *multi_handle)
{
    IGNORE_UNUSED(easy);
    IGNORE_UNUSED(multi_handle);
    return CURLM_OK;
}


/*----------------------------------------------------------------------------*/

const void* print_frame( const void *buffer, size_t len )
{
    struct cws_frame f;
    ssize_t rv;


    rv = _cws_decode_frame( &f, buffer, len );

    if (0 != rv) {
        printf("Problem decoding the frame: %ld\n", rv);
    }

    printf("Frame:\n"
                "\t        fin: %d\n"
                "\t is_control: %d\n"
                "\t     opcode: %d\n"
                "\t       mask: %d\n"
                "\t   mask key: 0x%08x\n"
                "\tpayload len: %" PRIu64 "\n"
                "\tpayload (still masked):\n",
                f.fin, f.is_control, f.opcode, f.mask, f.masking_key, f.payload_len );

    return f.next;
}

/*----------------------------------------------------------------------------*/
/*                                   Tests                                    */
/*----------------------------------------------------------------------------*/

void test_cws_write()
{
    CWS priv;
    uint8_t buf1[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    uint8_t buf2[5]  = { 20, 21, 22, 23, 24 };
    int i;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);

    CU_ASSERT(CWSE_OK == _cws_write(&priv, buf1, sizeof(buf1)));

    CU_ASSERT(NULL != priv.send.buffer);
    CU_ASSERT(10 == priv.send.len);
    //printf("\n");
    for (i = 0; i < 10; i++) {
        //printf("%d: %d =?= %d\n", i, buf1[i], priv.send.buffer[i]);
        CU_ASSERT(buf1[i] == priv.send.buffer[i]);
    }

    /* Act like we're paused. */
    priv.easy = (CURL*) 123;
    __easy_pause_easy = (CURL*) 123;
    priv.pause_flags = CURLPAUSE_SEND;
    __easy_pause_count = 0;
    __easy_pause_rv[0] = CURLE_OK;
    CU_ASSERT(CWSE_OK == _cws_write(&priv, buf2, sizeof(buf2)));
    CU_ASSERT(1 == __easy_pause_count)

    CU_ASSERT(NULL != priv.send.buffer);
    CU_ASSERT(15 == priv.send.len);
    for (i = 0; i < 10; i++) {
        //printf("%d: %d =?= %d\n", i, buf1[i], priv.send.buffer[i]);
        CU_ASSERT(buf1[i] == priv.send.buffer[i]);
    }
    for (i = 10; i < 15; i++) {
        //printf("%d: %d =?= %d\n", i, buf2[i-10], priv.send.buffer[i]);
        CU_ASSERT(buf2[i-10] == priv.send.buffer[i]);
    }

    /* It's been unpaused, make sure we don't call pause again. */
    CU_ASSERT(CWSE_OK == _cws_write(&priv, buf2, sizeof(buf2)));
    CU_ASSERT(1 == __easy_pause_count)

    free(priv.send.buffer);
}


void test_cws_send_data()
{
    CWS priv;
    uint8_t in[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    uint8_t out[7] = { 8, 7, 0, 0, 0, 9, 8 };

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);
    /* Use the write function to populate the buffers we're going to send. */
    CU_ASSERT(CWSE_OK == _cws_write(&priv, in, sizeof(in)));

    CU_ASSERT(3 == _cws_send_data((char*) &out[2], 1, 3, &priv));
    CU_ASSERT(8 == out[0]);
    CU_ASSERT(7 == out[1]);
    CU_ASSERT(1 == out[2]);
    CU_ASSERT(2 == out[3]);
    CU_ASSERT(3 == out[4]);
    CU_ASSERT(9 == out[5]);
    CU_ASSERT(8 == out[6]);
    CU_ASSERT(7 == priv.send.len);
    CU_ASSERT(NULL != priv.send.buffer);

    CU_ASSERT(3 == _cws_send_data((char*) &out[2], 1, 3, &priv));
    CU_ASSERT(4 == priv.send.len);
    CU_ASSERT(NULL != priv.send.buffer);

    CU_ASSERT(3 == _cws_send_data((char*) &out[2], 1, 3, &priv));
    CU_ASSERT(1 == priv.send.len);
    CU_ASSERT(NULL != priv.send.buffer);

    CU_ASSERT(1 == _cws_send_data((char*) &out[2], 1, 3, &priv));
    CU_ASSERT(0 == priv.send.len);
    CU_ASSERT(NULL == priv.send.buffer);

    CU_ASSERT(CURL_READFUNC_PAUSE == _cws_send_data((char*) &out[2], 1, 3, &priv));
    CU_ASSERT(0 == priv.send.len);
    CU_ASSERT(NULL == priv.send.buffer);
}


void test_cws_check_connection()
{
    struct {
        const char *in;
        bool before;
        bool after;
    } tests[] = {
        { .in = "fail",    .before = true,  .after = false },
        { .in = "badword", .before = true,  .after = false },
        { .in = "upgrade", .before = false, .after = true  },
        { .in = "UpGrade", .before = false, .after = true  },
        { .in = NULL }
    };

    CWS priv;
    int i;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);

    for (i = 0; NULL != tests[i].in; i++) {
        priv.upgraded = tests[i].before;
        _cws_check_connection(&priv, tests[i].in, strlen(tests[i].in));
        CU_ASSERT(tests[i].after == priv.upgraded);
    }
}


void test_cws_check_upgrade()
{
    struct {
        const char *in;
        bool before;
        bool after;
    } tests[] = {
        { .in = "fail",      .before = true,  .after = false },
        { .in = "Invalid12", .before = true,  .after = false },
        { .in = "websocket", .before = false, .after = true  },
        { .in = "WebSocket", .before = false, .after = true  },
        { .in = NULL }
    };

    CWS priv;
    int i;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);

    for (i = 0; NULL != tests[i].in; i++) {
        priv.connection_websocket = tests[i].before;
        _cws_check_upgrade(&priv, tests[i].in, strlen(tests[i].in));
        CU_ASSERT(tests[i].after == priv.connection_websocket);
    }
}


void test_cws_check_protocol()
{
    CWS priv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);

    _cws_check_protocol(&priv, "chat, bob", 4);
    CU_ASSERT_STRING_EQUAL(priv.websocket_protocols.received, "chat");
    _cws_check_protocol(&priv, "chat, bob", 9);
    CU_ASSERT_STRING_EQUAL(priv.websocket_protocols.received, "chat, bob");

    free(priv.websocket_protocols.received);
}


void test_cws_check_accept()
{
    CWS priv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);

    priv.expected_key_header = "12340934";
    priv.accepted = false;
    _cws_check_accept(&priv, "12340934", 8);
    CU_ASSERT(true == priv.accepted);

    priv.accepted = true;
    _cws_check_accept(&priv, "12340934", 7);
    CU_ASSERT(false == priv.accepted);

    priv.accepted = true;
    _cws_check_accept(&priv, "invalid4", 8);
    CU_ASSERT(false == priv.accepted);
}


void test_is_close_status_valid()
{
    int i;
    struct {
        int  code;
        int valid_c2s;
        int valid_s2c;
    } tests[] = {
        { .code =    0, .valid_c2s = -1, .valid_s2c = -1 },
        { .code =  999, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1000, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1001, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1002, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1003, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1004, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1005, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1006, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1007, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1008, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1009, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 1010, .valid_c2s =  0, .valid_s2c = -1 },
        { .code = 1011, .valid_c2s = -1, .valid_s2c =  0 },
        { .code = 1012, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1013, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1014, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1015, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 1016, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 2999, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = 3000, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 3001, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 3999, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 4000, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 4001, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 4999, .valid_c2s =  0, .valid_s2c =  0 },
        { .code = 5000, .valid_c2s = -1, .valid_s2c = -1 },
        { .code = -1 }
    };

    for (i = 0; -1 < tests[i].code; i++) {
        int c2s, s2c;

        c2s = _is_valid_close_to_server(tests[i].code);
        s2c = _is_valid_close_from_server(tests[i].code);

        // printf("code: %d (%d =? %d), (%d =? %d)\n", tests[i].code, c2s, tests[i].valid_c2s, s2c, tests[i].valid_s2c);

        CU_ASSERT_FATAL(tests[i].valid_c2s == c2s);
        CU_ASSERT_FATAL(tests[i].valid_s2c == s2c);
    }
}


void test_opcode_is_control()
{
    CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 0));
    CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 1));
    CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 2));
    // TODO: unsure why these are control
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 3));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 4));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 5));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 6));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 7));
    CU_ASSERT(true  == _cws_opcode_is_control((enum cws_opcode) 8));
    CU_ASSERT(true  == _cws_opcode_is_control((enum cws_opcode) 9));
    CU_ASSERT(true  == _cws_opcode_is_control((enum cws_opcode) 10));
    // TODO: they are not defined, should they be control?
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 11));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 12));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 13));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 14));
    //CU_ASSERT(false == _cws_opcode_is_control((enum cws_opcode) 15));
}


void test_sending_ping()
{
    CWS priv;
    uint8_t expect0[] = { 0x89, 0x84, 0x67, 0xc6, 0x69, 0x73, 0x37, 0x8f, 0x27, 0x34 };
    uint8_t expect1[] = { 0x89, 0x80, 0x51, 0xff, 0x4a, 0xec };
    uint8_t expect2[] = { 0x89, 0x80, 0x29, 0xcd, 0xba, 0xab };
    size_t i;
    int rv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);
    srand(0);

    rv = cws_ping(&priv, "PING", 4);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect0) == priv.send.len);
    for (i = 0; i < sizeof(expect0); i++ ) {
        CU_ASSERT(expect0[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_ping(&priv, NULL, 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect1) == priv.send.len);
    for (i = 0; i < sizeof(expect1); i++ ) {
        CU_ASSERT(expect1[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_ping(&priv, "Ignored", 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect2) == priv.send.len);
    for (i = 0; i < sizeof(expect2); i++ ) {
        CU_ASSERT(expect2[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_ping(&priv, "Too long", 126);
    CU_ASSERT(CWSE_APP_DATA_LENGTH_TOO_LONG == rv);
}


void test_sending_pong()
{
    CWS priv;
    uint8_t expect0[] = { 0x8a, 0x84, 0x67, 0xc6, 0x69, 0x73, 0x37, 0x8f, 0x27, 0x34 };
    uint8_t expect1[] = { 0x8a, 0x80, 0x51, 0xff, 0x4a, 0xec };
    uint8_t expect2[] = { 0x8a, 0x80, 0x29, 0xcd, 0xba, 0xab };
    size_t i;
    int rv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);
    srand(0);

    rv = cws_pong(&priv, "PING", 4);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect0) == priv.send.len);
    for (i = 0; i < sizeof(expect0); i++ ) {
        CU_ASSERT(expect0[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_pong(&priv, NULL, 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect1) == priv.send.len);
    for (i = 0; i < sizeof(expect1); i++ ) {
        CU_ASSERT(expect1[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_pong(&priv, "Ignored", 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect2) == priv.send.len);
    for (i = 0; i < sizeof(expect2); i++ ) {
        CU_ASSERT(expect2[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_pong(&priv, "Too long", 126);
    CU_ASSERT(CWSE_APP_DATA_LENGTH_TOO_LONG == rv);
}


void test_send_text()
{
    CWS priv;
    uint8_t expect0[] = { 0x81, 0x83, 0x67, 0xc6, 0x69, 0x73, 0x04, 0xa7, 0x1d };
    uint8_t expect1[] = { 0x81, 0x82, 0x51, 0xff, 0x4a, 0xec, 0x35, 0x90 };
    uint8_t expect2[] = { 0x81, 0x80, 0x29, 0xcd, 0xba, 0xab };
    size_t i;
    int rv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);
    srand(0);

    rv = cws_send_text(&priv, "cat", SIZE_MAX);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect0) == priv.send.len);
    for (i = 0; i < sizeof(expect0); i++ ) {
        CU_ASSERT(expect0[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_send_text(&priv, "dog", 2 );

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect1) == priv.send.len);
    for (i = 0; i < sizeof(expect1); i++ ) {
        CU_ASSERT(expect1[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_send_text(&priv, "Ignored", 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect2) == priv.send.len);
    for (i = 0; i < sizeof(expect2); i++ ) {
        CU_ASSERT(expect2[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

}


void test_send_binary()
{
    CWS priv;
    uint8_t expect0[] = { 0x82, 0x83, 0x67, 0xc6, 0x69, 0x73, 0x04, 0xa7, 0x1d };
    uint8_t expect1[] = { 0x82, 0x83, 0x51, 0xff, 0x4a, 0xec, 0x35, 0x90, 0x2d };
    uint8_t expect2[] = { 0x82, 0x80, 0x29, 0xcd, 0xba, 0xab };
    size_t i;
    int rv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);
    srand(0);

    rv = cws_send_binary(&priv, "cat", 3);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect0) == priv.send.len);
    for (i = 0; i < sizeof(expect0); i++ ) {
        CU_ASSERT(expect0[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_send_binary(&priv, "dog", 3 );

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect1) == priv.send.len);
    for (i = 0; i < sizeof(expect1); i++ ) {
        CU_ASSERT(expect1[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;

    rv = cws_send_binary(&priv, "Ignored", 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect2) == priv.send.len);
    for (i = 0; i < sizeof(expect2); i++ ) {
        CU_ASSERT(expect2[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;
}


void test_close()
{
    CWS priv;
    uint8_t expect0[] = { 0x88, 0x80, 0x67, 0xc6, 0x69, 0x73 };
    uint8_t expect1[] = { 0x88, 0x82, 0x51, 0xff, 0x4a, 0xec, 0x52, 0x16 };
    uint8_t expect2[] = { 0x88, 0x88, 0x29, 0xcd, 0xba, 0xab, 0x2a, 0x24, 0xd9, 0xc7, 0x46, 0xbe, 0xdf, 0xcf };
    uint8_t expect3[] = { 0x88, 0xfd, 0xf2, 0xfb, 0xe3, 0x46, 0xf1, 0x12, 0xb5, 0x27, 0x9e, 0x92, 0x87, 0x6a, 0xd2, 0x99,
                          0x96, 0x32, 0xd2, 0x89, 0x86, 0x27, 0x9e, 0x97, 0x9a, 0x66, 0x9e, 0x94, 0x8d, 0x21, 0xdc, 0xdb,
                          0xc3, 0x6e, 0xc3, 0xc9, 0xd0, 0x66, 0x86, 0x94, 0x97, 0x27, 0x9e, 0xdb, 0x80, 0x2e, 0x93, 0x89,
                          0x82, 0x25, 0x86, 0x9e, 0x91, 0x35, 0xdb, 0xdb, 0xae, 0x08, 0xbd, 0xab, 0xb2, 0x14, 0xa1, 0xaf,
                          0xb6, 0x10, 0xa5, 0xa1, 0xba, 0x1c, 0xc2, 0xca, 0xd1, 0x75, 0xc6, 0xce, 0xd5, 0x71, 0xca, 0xc2,
                          0x82, 0x24, 0x91, 0x9f, 0x86, 0x20, 0x95, 0x93, 0x8a, 0x2c, 0x99, 0x97, 0x8e, 0x28, 0x9d, 0x8b,
                          0x92, 0x34, 0x81, 0x8f, 0x96, 0x30, 0x85, 0x83, 0x9a, 0x3c, 0xb3, 0xb9, 0xa0, 0x02, 0xb7, 0xbd,
                          0xa4, 0x0e, 0xbb, 0xb1, 0xa8, 0x0a, 0xbf, 0xb5, 0xac, 0x16, 0xa3, 0xa9, 0xb0, 0x12, 0xa7, 0xad,
                          0xb4, 0x1c, 0xab };

    size_t i;
    int rv;

    memset(&priv, 0, sizeof(priv));
    _populate_callbacks(&priv, NULL);
    srand(0);

    rv = cws_close(&priv, 0, NULL, 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect0) == priv.send.len);
    for (i = 0; i < sizeof(expect0); i++ ) {
        CU_ASSERT(expect0[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;
    priv.closed = false;

    rv = cws_close(&priv, 1001, NULL, 0);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect1) == priv.send.len);
    for (i = 0; i < sizeof(expect1); i++ ) {
        CU_ASSERT(expect1[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;
    priv.closed = false;

    rv = cws_close(&priv, 1001, "closed", SIZE_MAX);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect2) == priv.send.len);
    for (i = 0; i < sizeof(expect2); i++ ) {
        CU_ASSERT(expect2[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;
    priv.closed = false;

    rv = cws_close(&priv, 1001, "Valid, but really long.  (123 total characters) MNOPQRSTUVWZYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWZY", SIZE_MAX);

    CU_ASSERT(0 == rv);
    CU_ASSERT(sizeof(expect3) == priv.send.len);
    for (i = 0; i < sizeof(expect3); i++ ) {
        CU_ASSERT(expect3[i] == priv.send.buffer[i]);
    }

    free(priv.send.buffer);
    priv.send.buffer = NULL;
    priv.send.len = 0;
    priv.closed = false;

    rv = cws_close(&priv, 1001, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 124);
    CU_ASSERT(CWSE_APP_DATA_LENGTH_TOO_LONG == rv);

    rv = cws_close(&priv, 999, "closed", SIZE_MAX);
    CU_ASSERT(CWSE_INVALID_CLOSE_REASON_CODE == rv);

    priv.closed = true;
    rv = cws_close(&priv, 1001, "closed", SIZE_MAX);
    CU_ASSERT(CWSE_CLOSED_CONNECTION == rv);
}

void test_curl_version()
{
    struct cws_config config;

    memset(&config, 0, sizeof(struct cws_config));

    __curl_info.version_num = 0x073100; // Version 7.49.0
    CU_ASSERT(NULL == cws_create(&config));

    __curl_info.version_num = 0x073201; // Version 7.50.1
    CU_ASSERT(NULL == cws_create(&config));
}


void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "_cws_write() Tests",                    .fn = test_cws_write             },
        { .label = "_cws_send_data() Tests",                .fn = test_cws_send_data         },
        { .label = "_cws_check_connection() Tests",         .fn = test_cws_check_connection  },
        { .label = "_cws_check_upgrade() Tests",            .fn = test_cws_check_upgrade     },
        { .label = "_cws_check_protocol() Tests",           .fn = test_cws_check_protocol    },
        { .label = "_cws_check_accept() Tests",             .fn = test_cws_check_accept      },
        { .label = "_cws_is_close_status_valid() Tests",    .fn = test_is_close_status_valid },
        { .label = "_cws_opcode_is_control() Tests",        .fn = test_opcode_is_control     },
        { .label = "cws_ping() Tests",                      .fn = test_sending_ping          },
        { .label = "cws_pong() Tests",                      .fn = test_sending_pong          },
        { .label = "cws_send_text() Tests",                 .fn = test_send_text             },
        { .label = "cws_send_binary() Tests",               .fn = test_send_binary           },
        { .label = "cws_close() Tests",                     .fn = test_close                 },
        { .label = "curl version Tests",                    .fn = test_curl_version          },
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
