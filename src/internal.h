/*
 * Copyright (C) 2016 Gustavo Sverzut Barbieri
 * Copyright (c) 2021 Comcast Cable Communications Management, LLC
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * https://opensource.org/licenses/MIT
 */
#ifndef __CWS_INTERNAL_H__
#define __CWS_INTERNAL_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "frame.h"
#include "memory.h"
#include "ws.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define IGNORE_UNUSED(x) (void) x

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/

struct cws_object {
    /* The initial URL to connect to. */
    char *url;

    /* The curl object that represents the connection. */
    CURL *easy;

    /* The headers curl needs us to store. */
    struct curl_slist *headers;

    /* The websocket extensions */
    struct {
        char *requested;
        char *received;
    } websocket_protocols;

    /* The key header that the server is expected to return. */
    char expected_key_header[WS_HTTP_EXPECTED_KEY_SIZE];

    /* The user provided data. */
    void *user;

    /* The callback functions used.  These will be called without NULL
     * checking as the library expects to use default values. */
    void (*on_connect_fn)(void*, CWS*, const char*);
    void (*on_text_fn)(void*, CWS*, const char*, size_t);
    void (*on_binary_fn)(void*, CWS*, const void*, size_t);
    void (*on_stream_fn)(void*, CWS*, int, const void*, size_t);
    void (*on_ping_fn)(void*, CWS*, const void*, size_t);
    void (*on_pong_fn)(void*, CWS*, const void*, size_t);
    void (*on_close_fn)(void*, CWS*, int, const char*, size_t);
    void (*get_random_fn)(void*, CWS*, void*, size_t);
    void (*debug_fn)(void*, CWS*, const char*, ...);
    void (*configure_fn)(void*, CWS*, CURL*);

    /* The memory configuration & pool. */
    struct mem_pool_config mem_cfg;
    pool_t *mem;

    /* The details about the last data frame queued to send. */
    int last_sent_data_frame_info;

    /* The maximum payload size */
    size_t max_payload_size;

    /* The outgoing queue of bytes to send. */
    struct cws_buf_queue *send;

    /* The structure needed to deal with the incoming data stream */
    struct {
        /* The information needed across fragments */
        int fragment_info;

        /* Valid if header.needed = 0. The decoded WS frame header.*/
        struct cws_frame frame;

        /* Scratch space for collecting the data needed to decode WS a frame
         * header. */
        struct {
            uint8_t buf[WS_FRAME_HEADER_MAX];
            size_t used;
            size_t needed;
        } header;

        /* Scratch space for collecting the data needed to decode control WS
         * payload.  The header is still stored in the header struct above. */
        struct {
            uint8_t buf[WS_CTL_PAYLOAD_MAX];
            size_t used;
            size_t needed;
        } control;
    } recv;

    /* Connection State flags */
    uint8_t dispatching;
    uint8_t pause_flags;
    bool redirection;
    bool accepted;
    bool upgraded;
    bool connection_websocket;
    bool closed;
    bool deleted;
};

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/

/**
 * Internal only cleanup function.
 */
void _cws_cleanup(CWS *priv);
#endif

