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
#include "../src/frame_senders.h"
#include "../src/ws.h"

void cws_random(CWS *priv, void *buffer, size_t len)
{
    uint8_t *bytes = buffer;

    (void) priv;

    for (size_t i = 0; i < len; i++) {
        bytes[i] = (0x0ff & i);
    }
}

static struct cws_frame __send_frame_frame;
CWScode send_frame(CWS *priv, const struct cws_frame *f)
{
    CU_ASSERT(NULL != priv);
    CU_ASSERT(NULL != f);
    CU_ASSERT(__send_frame_frame.fin == f->fin);
    CU_ASSERT(__send_frame_frame.mask == f->mask);
    CU_ASSERT(__send_frame_frame.is_control == f->is_control);
    CU_ASSERT(__send_frame_frame.opcode == f->opcode);
    CU_ASSERT(__send_frame_frame.masking_key[0] == f->masking_key[0]);
    CU_ASSERT(__send_frame_frame.masking_key[1] == f->masking_key[1]);
    CU_ASSERT(__send_frame_frame.masking_key[2] == f->masking_key[2]);
    CU_ASSERT(__send_frame_frame.masking_key[3] == f->masking_key[3]);
    CU_ASSERT(__send_frame_frame.payload_len == f->payload_len);
    if (NULL == __send_frame_frame.payload) {
        CU_ASSERT(NULL == f->payload);
    } else {
        CU_ASSERT(NULL != f->payload);
    }

    return CWSE_OK;
}

void test_frame_sender_control()
{
    CWS priv;

    memset(&priv, 0, sizeof(CWS));

    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_control(&priv, -1, NULL, 0));
    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_control(&priv, CWS_PING|CWS_PONG, NULL, 0));

    priv.closed = true;
    CU_ASSERT(CWSE_CLOSED_CONNECTION == frame_sender_control(&priv, CWS_PING, NULL, 0));

    priv.closed = false;
    CU_ASSERT(CWSE_APP_DATA_LENGTH_TOO_LONG == frame_sender_control(&priv, CWS_PING, "ignore", 1000));

    /* Valid, but empty payloads. */
    __send_frame_frame.fin = 1;
    __send_frame_frame.mask = 1;
    __send_frame_frame.is_control = 1;
    __send_frame_frame.opcode = WS_OPCODE_PING;
    __send_frame_frame.masking_key[0] = 0;
    __send_frame_frame.masking_key[1] = 1;
    __send_frame_frame.masking_key[2] = 2;
    __send_frame_frame.masking_key[3] = 3;
    __send_frame_frame.payload_len = 0;
    __send_frame_frame.payload = NULL;
    CU_ASSERT(CWSE_OK == frame_sender_control(&priv, CWS_PING, NULL, 1000));
    CU_ASSERT(CWSE_OK == frame_sender_control(&priv, CWS_PING, "ignore", 0));

    __send_frame_frame.opcode = WS_OPCODE_PONG;
    __send_frame_frame.payload = "data";
    __send_frame_frame.payload_len = 4;
    CU_ASSERT(CWSE_OK == frame_sender_control(&priv, CWS_PONG, "database", 4));

    __send_frame_frame.opcode = WS_OPCODE_CLOSE;
    CU_ASSERT(CWSE_OK == frame_sender_control(&priv, CWS_CLOSE, "database", 4));
}


void test_frame_sender_data()
{
    CWS priv;

    memset(&priv, 0, sizeof(CWS));

    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_data(&priv, -1, NULL, 0));
    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_data(&priv, CWS_CONT|CWS_TEXT, NULL, 0));

    priv.closed = true;
    CU_ASSERT(CWSE_CLOSED_CONNECTION == frame_sender_data(&priv, CWS_TEXT|CWS_FIRST, NULL, 0));

    priv.closed = false;
    CU_ASSERT(CWSE_STREAM_CONTINUITY_ISSUE == frame_sender_data(&priv, CWS_CONT, "ignore", 5));

    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_data(&priv, CWS_CONT|CWS_FIRST, "ignore", 5));
    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_data(&priv, CWS_TEXT, "ignore", 5));
    CU_ASSERT(CWSE_INVALID_OPTIONS == frame_sender_data(&priv, CWS_BINARY, "ignore", 5));

    /* Valid, but empty payloads. */
    __send_frame_frame.fin = 0;
    __send_frame_frame.mask = 1;
    __send_frame_frame.is_control = 0;
    __send_frame_frame.opcode = WS_OPCODE_BINARY;
    __send_frame_frame.masking_key[0] = 0;
    __send_frame_frame.masking_key[1] = 1;
    __send_frame_frame.masking_key[2] = 2;
    __send_frame_frame.masking_key[3] = 3;
    __send_frame_frame.payload_len = 1;
    __send_frame_frame.payload = "H";
    CU_ASSERT(CWSE_OK == frame_sender_data(&priv, CWS_BINARY|CWS_FIRST, "H", 1));

    CU_ASSERT(CWSE_STREAM_CONTINUITY_ISSUE == frame_sender_data(&priv, CWS_TEXT|CWS_FIRST, "ignore", 5));
    CU_ASSERT(CWSE_STREAM_CONTINUITY_ISSUE == frame_sender_data(&priv, CWS_BINARY|CWS_FIRST, "ignore", 5));

    __send_frame_frame.opcode = WS_OPCODE_CONTINUATION;
    CU_ASSERT(CWSE_OK == frame_sender_data(&priv, CWS_CONT, "H", 1));

    __send_frame_frame.payload_len = 0;
    __send_frame_frame.payload = NULL;

    CU_ASSERT(CWSE_OK == frame_sender_data(&priv, CWS_CONT, "H", 0));
    CU_ASSERT(CWSE_OK == frame_sender_data(&priv, CWS_CONT, NULL, 1));

    __send_frame_frame.fin = 1;
    __send_frame_frame.payload_len = 1;
    __send_frame_frame.payload = "H";
    CU_ASSERT(CWSE_OK == frame_sender_data(&priv, CWS_CONT|CWS_LAST, "H", 1));

    CU_ASSERT(CWSE_STREAM_CONTINUITY_ISSUE == frame_sender_data(&priv, CWS_CONT, "ignore", 5));
}




void add_suites( CU_pSuite *suite )
{
    struct {
        const char *label;
        void (*fn)(void);
    } tests[] = {
        { .label = "frame_sender_control() Tests", .fn = test_frame_sender_control },
        { .label = "frame_sender_data() Tests",    .fn = test_frame_sender_data },
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
